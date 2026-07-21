// test_gc_los.c — the Immix large-object space. An object larger than a block's data area
// (IMX_MEDIUM_MAX) can't live in the block/line engine; it goes to the LOS: its own malloc'd
// allocation, marked in place (never evacuated), swept whole. We allocate a large ref-array
// (well past the block ceiling — previously a hard trap), fill it with children, then churn
// garbage (small nodes + UNROOTED large arrays) to force many collections. The big array must
// survive with its children intact (traced through its ref slots, children evacuated as they
// move), and the unrooted large-array garbage must be reclaimed (the LOS byte count returns to
// just the one rooted array).
#include "jav_gc.h"
#include <stdio.h>

/* leaf node { i64 val @ payload+0 } — small (a block object). */
static const gc_rtt_t NODE_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + 8, .nrefs = 0, .kind = GC_KIND_STRUCT, .gid = -1
};
#define NODE (&NODE_S)
static int64_t* node_val(gc_obj_t* o) { return (int64_t*)((uint8_t*)o + sizeof(gc_obj_t)); }

/* large ref array: 8-byte managed-ref elements — the POW_5_CACHE / big-Object[] shape. */
static const gc_rtt_t BIGARR_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET, .kind = GC_KIND_ARRAY,
    .elem_is_ref = 1, .elem_store_w = 8, .gid = -1
};
#define BIGARR (&BIGARR_S)
#define BIGLEN 5000    /* size = 24 + 8 + 5000*8 = 40032 B  >  IMX_MEDIUM_MAX (~32 KB) → LOS */

static uint32_t*  arr_len(gc_obj_t* o)   { return (uint32_t*)((uint8_t*)o + sizeof(gc_obj_t)); }
static gc_obj_t** arr_elems(gc_obj_t* o) { return (gc_obj_t**)((uint8_t*)o + sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET); }

static gc_obj_t* g_big;   /* the one rooted large array */
static void enum_roots(gc_heap_t* h, gc_root_visit_fn visit, void* ctx) {
    (void)h; if (g_big) visit(&g_big, ctx);
}

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-58s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void) {
    gc_heap_t h; gc_heap_init(&h, enum_roots, NULL);

    size_t bigsz = sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (size_t)BIGLEN * GC_ARRAY_ELEM_BYTES;
    CK(bigsz > IMX_MEDIUM_MAX, "test array exceeds IMX_MEDIUM_MAX (routes to the LOS)");

    g_big = gc_alloc(&h, BIGARR, (uint32_t)bigsz);
    CK(g_big != NULL, "large array allocation succeeds (LOS, not the old hard trap)");
    *arr_len(g_big) = BIGLEN;
    for (int i = 0; i < BIGLEN; i++) arr_elems(g_big)[i] = NULL;   /* GC-safe: null before any alloc */
    CK(h.large_bytes == bigsz, "the large array is accounted in the LOS");

    /* Fill each slot with a child leaf (val = i). Alloc first (may collect + move earlier children);
     * the slot in the rooted g_big keeps each child alive + up-to-date. */
    for (int i = 0; i < BIGLEN; i++) {
        gc_obj_t* c = gc_alloc(&h, NODE, NODE_S.size);
        *node_val(c) = (int64_t)i;
        arr_elems(g_big)[i] = c;
    }

    /* Churn: many small nodes + periodic UNROOTED large arrays (LOS garbage). Forces repeated
     * collections that mark g_big in place, trace/evacuate its children, and sweep the LOS garbage. */
#ifndef LOS_CHURN_MULT
#define LOS_CHURN_MULT 64          /* GC_STRESS collects every alloc → build with a small mult to stay fast */
#endif
    const size_t churn = (size_t)GC_INITIAL_THRESHOLD * LOS_CHURN_MULT;
    for (size_t a = 0; a < churn; a += NODE_S.size) {
        gc_obj_t* junk = gc_alloc(&h, NODE, NODE_S.size); (void)junk;
        if ((a % (GC_INITIAL_THRESHOLD * 4u)) < NODE_S.size) {
            gc_obj_t* bigjunk = gc_alloc(&h, BIGARR, (uint32_t)bigsz);   /* unrooted large array → must be reclaimed */
            if (bigjunk) *arr_len(bigjunk) = BIGLEN;                     /* len consistent with its alloc size */
        }
    }

    /* g_big survived at its original address (LOS objects are never moved), its length intact, and
     * every child survived with its value (found via g_big's ref slots, updated on evacuation). */
    int all_ok = (*arr_len(g_big) == BIGLEN);
    for (int i = 0; i < BIGLEN && all_ok; i++) {
        gc_obj_t* c = arr_elems(g_big)[i];
        if (!c || *node_val(c) != (int64_t)i) { printf("  big[%d] child val wrong\n", i); all_ok = 0; }
    }
    CK(all_ok, "large LOS array + all 5000 children survive churn intact");

    /* After a final collection only the rooted big array remains in the LOS — the unrooted large
     * arrays were swept (their storage freed, the byte count back to exactly one array). */
    gc_collect(&h);
    CK(h.large_bytes == bigsz, "unrooted large arrays are reclaimed (LOS swept back to one)");

    gc_heap_destroy(&h);
    printf("\nGC large-object-space: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
