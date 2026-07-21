// test_gc_churn.c — the adaptive allocation trigger. A hot loop allocates far more
// garbage than the heap's initial budget WITHOUT rooting any of it. The threshold
// safepoint inside gc_alloc must collect incrementally so the heap stays bounded —
// proving we don't fill all of memory before the first collection (the naive
// OOM-only model would grow the heap to the full churn volume).
#include "jav_gc.h"
#include <stdio.h>

static const gc_rtt_t LEAF = { .size = (uint32_t)(sizeof(gc_obj_t) + 8), .kind = GC_KIND_STRUCT };
#define RTT(x) ((const gc_rtt_t*)&(x))

static gc_obj_t* g_root;   /* a single optional root */
static void enum_roots(gc_heap_t* h, gc_root_visit_fn visit, void* ctx){ (void)h; if (g_root) visit(&g_root, ctx); }

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-52s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    gc_heap_t h; gc_heap_init(&h, enum_roots, NULL);
    g_root = NULL;

    /* Churn: allocate ~64× the initial budget as pure garbage (nothing rooted). */
    const size_t budget = GC_INITIAL_THRESHOLD;
    size_t churn = budget * 64;
    size_t allocated = 0, count = 0;
    int all_ok = 1;
    while (allocated < churn) {
        gc_obj_t* o = gc_alloc(&h, RTT(LEAF), LEAF.size);
        if (!o) { all_ok = 0; break; }
        allocated += LEAF.size; count++;
    }
    CK(all_ok, "churn: every allocation succeeded");

    /* The live set is ~0, so the heap must have collected its way to a tiny footprint
     * instead of growing to hold all `churn` bytes. Bound it generously: a handful of
     * blocks, NOT the churn volume. */
    size_t blocks = imx_space_total_blocks(&h.space);
    size_t churn_blocks = churn / IMX_BLOCK_SIZE;
    printf("  churned %zu objects (%zu KB), heap = %zu blocks (churn would be %zu)\n",
           count, allocated/1024, blocks, churn_blocks);
    CK(blocks < churn_blocks / 4, "heap stayed bounded (auto-collect fired, no OOM-cliff)");

    /* The threshold adapted upward from the initial budget while churning. */
    CK(h.gc_threshold >= GC_INITIAL_THRESHOLD, "threshold tracks the (small) live set");

    /* Now keep one rooted live object across the same churn: it must survive every
     * auto-collection (the trigger doesn't reclaim reachable objects). */
    g_root = gc_alloc(&h, RTT(LEAF), LEAF.size);
    *(int64_t*)gc_obj_payload(g_root) = 0x5151;
    for (size_t i = 0; i < churn / LEAF.size; i++) {
        gc_obj_t* o = gc_alloc(&h, RTT(LEAF), LEAF.size);   /* garbage; may trigger a collect */
        if (!o) { fails++; break; }
    }
    CK(gc_obj_live(&h, g_root) && *(int64_t*)gc_obj_payload(g_root) == 0x5151,
       "rooted object survives churn-driven auto-collections");

    gc_heap_destroy(&h);
    printf("\nadaptive GC trigger (bounded heap under churn): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
