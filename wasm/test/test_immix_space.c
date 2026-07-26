// test_immix_space.c — the ported Immix space: bump allocation through holes,
// distinct/aligned/in-bounds pointers, a clear_marks->mark->reclaim cycle and its
// free/recyclable/unavailable partition, and opportunistic evacuation.
#include "immix_space.h"
#include <stdio.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-50s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

static int in_data_region(void* p){
    imx_block_t* b = imx_block_of(p);
    return (uint8_t*)p >= imx_block_data_start(b) && (uint8_t*)p < imx_block_data_end(b);
}

int main(void){
    imx_space_t s; imx_space_init(&s);

    /* allocate 64 small objects; all distinct, 8-aligned, in a block's data region */
    void* ptrs[64]; int distinct=1, aligned=1, inbounds=1;
    for (int i=0;i<64;i++){
        ptrs[i] = imx_space_allocate(&s, 64, 8);
        if (!ptrs[i]) { distinct=0; break; }
        if ((uintptr_t)ptrs[i] % 8) aligned=0;
        if (!in_data_region(ptrs[i])) inbounds=0;
        for (int j=0;j<i;j++) if (ptrs[j]==ptrs[i]) distinct=0;
    }
    CK(distinct, "64 small allocs: all non-NULL and distinct");
    CK(aligned, "all 8-aligned");
    CK(inbounds, "all within a block's data region");
    CK(imx_space_total_blocks(&s) >= 1, "at least one block acquired");

    /* a collection cycle: mark the lines of the first 8 objects, reclaim with no headroom */
    imx_space_clear_marks(&s);
    for (int i=0;i<8;i++) imx_block_mark_object(imx_block_of(ptrs[i]), ptrs[i], 64);
    imx_space_reclaim(&s, 0);
    size_t tot = imx_space_total_blocks(&s);
    size_t part = imx_space_free_blocks(&s) + imx_space_recyclable_blocks(&s) + imx_space_unavailable_blocks(&s);
    CK(part == tot, "reclaim(headroom 0): free+recyclable+unavailable == total");
    CK(imx_space_recyclable_blocks(&s) >= 1, "the block with marked objects is recyclable");

    /* Immix §3.2 headroom: reclaim SETS ASIDE free blocks that the allocator can no longer
     * see. The partition is now four-way, and the reserve is what makes evacuation possible
     * at the next collection — the mutator drains free_blocks but cannot touch this.
     * Grow past one block first, keeping only the original 8 objects live, so blocks come
     * back FREE and there is something to set aside. */
    for (int i=0;i<1500;i++) (void)imx_space_allocate(&s, 64, 8);
    imx_space_clear_marks(&s);
    for (int i=0;i<8;i++) imx_block_mark_object(imx_block_of(ptrs[i]), ptrs[i], 64);
    imx_space_reclaim(&s, 2);
    size_t part4 = imx_space_free_blocks(&s) + imx_space_recyclable_blocks(&s)
                 + imx_space_unavailable_blocks(&s) + imx_space_reserve_blocks(&s);
    CK(part4 == imx_space_total_blocks(&s),
       "reclaim(headroom): free+recyclable+unavailable+reserve == total");
    CK(imx_space_reserve_blocks(&s) >= 1, "headroom blocks were actually set aside");
    /* ...and BOUNDED by what was asked for. The reserve is headroom, not a sink: if blocks
     * could accumulate here, every "everything was reclaimed" check that counts the reserve
     * would pass while the heap grew without limit. */
    CK(imx_space_reserve_blocks(&s) <= 2, "the reserve never exceeds the requested headroom");
    imx_space_reclaim(&s, 2);
    CK(imx_space_reserve_blocks(&s) <= 2, "repeated reclaims do not grow the reserve");

    /* §3.2.1 is ON DEMAND: with nothing fragmented there is nothing to defragment, so
     * evacuation must NOT activate even though headroom exists. */
    imx_space_clear_marks(&s);
    imx_space_reclaim(&s, 2);
    CK(imx_space_recyclable_blocks(&s) == 0, "no marks -> nothing recyclable");
    imx_space_begin_evacuation(&s, 2);
    CK(!imx_space_evacuation_active(&s), "no fragmentation -> begin_evacuation stays INACTIVE");
    imx_space_end_evacuation(&s);

    /* ...and with a fragmented block plus headroom, it does activate and can allocate. */
    void* frag[8];
    for (int i=0;i<8;i++) frag[i] = imx_space_allocate(&s, 64, 8);
    imx_space_clear_marks(&s);
    for (int i=0;i<8;i++) imx_block_mark_object(imx_block_of(frag[i]), frag[i], 64);
    imx_space_reclaim(&s, 2);
    CK(imx_space_recyclable_blocks(&s) >= 1, "a partly-marked block is fragmented");
    imx_space_begin_evacuation(&s, 2);
    CK(imx_space_evacuation_active(&s), "fragmentation + headroom -> begin_evacuation ACTIVE");
    CK(imx_space_is_evacuation_candidate(&s, imx_block_of(frag[0])),
       "the fragmented block is selected as a candidate");
    void* e = imx_space_allocate_evacuation(&s, 64, 8);
    CK(e && in_data_region(e), "allocate_evacuation: a pointer on a target block");
    CK(!imx_space_is_evacuation_candidate(&s, imx_block_of(e)),
       "a target block is never itself a candidate");
    size_t reserve_before = imx_space_reserve_blocks(&s);
    imx_space_end_evacuation(&s);
    CK(!imx_space_evacuation_active(&s) && imx_space_reserve_blocks(&s) >= reserve_before,
       "end_evacuation: inactive, targets returned to the reserve (not the free list)");

    imx_space_destroy(&s);

    /* §3.2.1 within a bin: the headroom is small, so WHICH equally-fragmented blocks it is
     * spent on matters. Cheapest-first (fewest marked lines) frees the most blocks per line
     * copied. This case is built so arbitrary order gives a DIFFERENT answer: three blocks in
     * the same hole-count bin with 250, 1 and 1 marked lines, one target block (251 lines of
     * capacity), and the expensive one FIRST in block-acquisition order.
     *   cheapest-first: 1 + 1 = 2 lines, then 250 would need 252 > 251 -> expensive REJECTED.
     *   acquisition order: 250, then 1 = 251 fits, then 1 would need 252 -> expensive SELECTED.
     * So "the expensive block is not a candidate" fails the moment the order stops being a
     * policy, which is the whole claim. */
    {
        imx_space_t t; imx_space_init(&t);
        for (int i = 0; i < 4; i++) {           /* one whole block per medium alloc */
            void* p = imx_space_allocate(&t, IMX_MEDIUM_MAX, 8);
            CK(p != NULL, i == 0 ? "within-bin: four blocks acquired" : NULL);
        }
        CK(imx_space_total_blocks(&t) == 4, "within-bin: exactly four blocks");

        imx_space_clear_marks(&t);
        imx_block_t* expensive = t.all_blocks[0];
        imx_block_t* cheap_a   = t.all_blocks[1];
        imx_block_t* cheap_b   = t.all_blocks[2];
        /* one hole each (a single trailing run of free lines), different marked volumes */
        for (size_t ln = IMX_DATA_START_LINE; ln < IMX_DATA_START_LINE + 250; ln++)
            imx_block_mark_object(expensive, imx_block_line_addr(expensive, ln), 1);
        imx_block_mark_object(cheap_a, imx_block_line_addr(cheap_a, IMX_DATA_START_LINE), 1);
        imx_block_mark_object(cheap_b, imx_block_line_addr(cheap_b, IMX_DATA_START_LINE), 1);
        /* t.all_blocks[3] stays unmarked -> free -> becomes the reserve/target */

        imx_space_reclaim(&t, 1);
        CK(expensive->defrag_holes == cheap_a->defrag_holes,
           "within-bin: the three blocks share a hole-count bin");
        CK(expensive->defrag_marked == 250 && cheap_a->defrag_marked == 1,
           "within-bin: marked volumes are 250 vs 1");

        imx_space_begin_evacuation(&t, 1);
        CK(imx_space_evacuation_active(&t), "within-bin: evacuation active");
        CK(imx_space_is_evacuation_candidate(&t, cheap_a) &&
           imx_space_is_evacuation_candidate(&t, cheap_b),
           "within-bin: both cheap blocks selected");
        CK(!imx_space_is_evacuation_candidate(&t, expensive),
           "within-bin: the expensive block is NOT selected (cheapest-first, not arbitrary)");
        imx_space_end_evacuation(&t);
        imx_space_destroy(&t);
    }
    printf("\nimmix space allocator (ported C): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
