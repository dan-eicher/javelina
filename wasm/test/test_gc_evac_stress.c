// test_gc_evac_stress.c — stress the moving collector's pointer-update path. We build many
// linked chains of ref-field nodes (rooted at their heads), then churn enough garbage to force
// repeated collections WITH opportunistic evacuation. Each collection defragments and MOVES live
// nodes; every `next` field must be rewritten to the moved location. If any update is missed, a
// chain either reads a wrong value (silent) or, under ASAN heap poisoning, faults on the stale
// pointer into a reclaimed block. This covers the multi-reference / through-field case that the
// single-root churn test does not.
#include "jav_gc.h"
#include <stdio.h>

/* node { (ref next) @ payload+0 ; i64 val @ payload+8 } — one ref field. */
typedef struct { uint32_t size, nrefs; uint16_t nfields; uint8_t kind, elem_is_ref, elem_store_w, elem_heap_w; const uint32_t* field_off; int32_t gid; uint32_t off[1]; } rtt_ref1_t;
static const rtt_ref1_t NODE_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + 16, .nrefs = 1, .kind = GC_KIND_STRUCT, .gid = -1,
    .off = { (uint32_t)sizeof(gc_obj_t) }
};
#define NODE ((const gc_rtt_t*)&NODE_S)

/* ref array (the POW_5_CACHE shape): kind=ARRAY, elements are 8-byte managed refs. */
static const gc_rtt_t ARR_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET, .kind = GC_KIND_ARRAY,
    .elem_is_ref = 1, .elem_store_w = 8, .gid = -1
};
#define ARR (&ARR_S)
#define ARRLEN 200

#define NCHAIN   64
#define CHAINLEN 8
static gc_obj_t* heads[NCHAIN];
static gc_obj_t* g_arr;   /* a rooted ref array whose elements each point to a child node */
static void enum_roots(gc_heap_t* h, gc_root_visit_fn visit, void* ctx) {
    (void)h;
    for (int i = 0; i < NCHAIN; i++) if (heads[i]) visit(&heads[i], ctx);
    if (g_arr) visit(&g_arr, ctx);
}
static uint32_t*  arr_len(gc_obj_t* o)  { return (uint32_t*)((uint8_t*)o + sizeof(gc_obj_t)); }
static gc_obj_t** arr_elems(gc_obj_t* o){ return (gc_obj_t**)((uint8_t*)o + sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET); }

static gc_obj_t** node_next(gc_obj_t* o) { return (gc_obj_t**)gc_obj_payload(o); }
static int64_t*   node_val(gc_obj_t* o)  { return (int64_t*)((uint8_t*)gc_obj_payload(o) + 8); }

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-56s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void) {
    gc_heap_t h; gc_heap_init(&h, enum_roots, NULL);

    /* Build NCHAIN chains; head[i] -> node(len-1) -> ... -> node(0) -> NULL. val = i*1000 + j.
     * Interleave garbage between nodes so the live set is fragmented across blocks (→ recyclable
     * → evacuation candidates), which is what makes the collector actually MOVE the live nodes. */
    for (int i = 0; i < NCHAIN; i++) {
        heads[i] = NULL;                            /* the rooted slot for this chain's growing head */
        for (int j = 0; j < CHAINLEN; j++) {
            gc_obj_t* n = gc_alloc(&h, NODE, NODE_S.size);   /* alloc first (may collect + move heads[i]) */
            *node_next(n) = heads[i];               /* link to the (rooted, up-to-date) partial head */
            *node_val(n) = (int64_t)(i * 1000 + j);
            heads[i] = n;                           /* root the growing chain before any further alloc */
            for (int g = 0; g < 3; g++) { gc_obj_t* junk = gc_alloc(&h, NODE, NODE_S.size); (void)junk; }  /* fragment */
        }
    }

    /* Ref array (POW_5_CACHE shape): g_arr[i] -> node(val=i*7) -> child(val=i*7+1). */
    {
        size_t asz = sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (size_t)ARRLEN * GC_ARRAY_ELEM_BYTES;
        g_arr = gc_alloc(&h, ARR, (uint32_t)asz);
        *arr_len(g_arr) = ARRLEN;
        for (int i = 0; i < ARRLEN; i++) arr_elems(g_arr)[i] = NULL;   /* GC-safe: null before any alloc */
        for (int i = 0; i < ARRLEN; i++) {
            gc_obj_t* child = gc_alloc(&h, NODE, NODE_S.size);
            *node_next(child) = NULL; *node_val(child) = (int64_t)(i * 7 + 1);
            arr_elems(g_arr)[i] = child;                               /* stash so a collect keeps it */
            gc_obj_t* n = gc_alloc(&h, NODE, NODE_S.size);
            *node_next(n) = arr_elems(g_arr)[i]; *node_val(n) = (int64_t)(i * 7);
            arr_elems(g_arr)[i] = n;
            for (int g = 0; g < 2; g++) { gc_obj_t* junk = gc_alloc(&h, NODE, NODE_S.size); (void)junk; }
        }
    }

    /* The premise of this whole file, now CHECKED rather than assumed. Every assertion below
     * verifies that references were rewritten correctly when objects moved — which proves nothing
     * at all if nothing ever moves. Record where the rooted heads live before the churn; at least
     * one of them must be somewhere else afterwards. (Roots are visited and updated in place by
     * gc_visit_root, so an evacuated head is observable right here.) */
    gc_obj_t* before[NCHAIN];
    for (int i = 0; i < NCHAIN; i++) before[i] = heads[i];

    /* Churn ~128× the budget as pure garbage — forces many threshold collections, each of which
     * opportunistically evacuates the (now heavily fragmented) live chains to new locations. */
    const size_t churn = (size_t)GC_INITIAL_THRESHOLD * 128;
    for (size_t a = 0; a < churn; a += NODE_S.size) {
        gc_obj_t* junk = gc_alloc(&h, NODE, NODE_S.size);
        if (!junk) { CK(0, "churn allocation succeeded"); break; }
    }

    /* Every chain must be intact and correctly valued after all the moving. A missed `next` update
     * shows up here as a wrong value / wrong length (and faults earlier under ASAN poisoning). */
    int all_ok = 1;
    for (int i = 0; i < NCHAIN; i++) {
        gc_obj_t* n = heads[i];
        int j = CHAINLEN - 1;
        while (n && j >= 0) {
            if (*node_val(n) != (int64_t)(i * 1000 + j)) {
                printf("  chain %d node %d: val=%lld expected %d\n", i, j, (long long)*node_val(n), i*1000+j);
                all_ok = 0; break;
            }
            n = *node_next(n);
            j--;
        }
        if (j != -1 || n != NULL) { printf("  chain %d wrong length (stopped j=%d)\n", i, j); all_ok = 0; }
    }
    CK(all_ok, "all chains intact + correctly valued after churn-driven evacuation");

    int moved = 0;
    for (int i = 0; i < NCHAIN; i++) if (heads[i] != before[i]) moved++;
    printf("  (evacuated heads: %d of %d)\n", moved, NCHAIN);
    CK(moved > 0, "opportunistic evacuation MOVED at least one live object");

    int arr_ok = (*arr_len(g_arr) == ARRLEN);
    for (int i = 0; i < ARRLEN && arr_ok; i++) {
        gc_obj_t* n = arr_elems(g_arr)[i];
        if (!n || *node_val(n) != (int64_t)(i * 7)) { printf("  array[%d] node val wrong\n", i); arr_ok = 0; break; }
        gc_obj_t* child = *node_next(n);
        if (!child || *node_val(child) != (int64_t)(i * 7 + 1)) { printf("  array[%d] child val wrong\n", i); arr_ok = 0; break; }
    }
    CK(arr_ok, "rooted ref-array elements + their children survive evacuation intact");

    gc_heap_destroy(&h);
    printf("\nGC evacuation pointer-update stress: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
