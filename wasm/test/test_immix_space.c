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

    /* a collection cycle: mark the lines of the first 8 objects, reclaim */
    imx_space_clear_marks(&s);
    for (int i=0;i<8;i++) imx_block_mark_object(imx_block_of(ptrs[i]), ptrs[i], 64);
    imx_space_reclaim(&s);
    size_t tot = imx_space_total_blocks(&s);
    size_t part = imx_space_free_blocks(&s) + imx_space_recyclable_blocks(&s) + imx_space_unavailable_blocks(&s);
    CK(part == tot, "reclaim: free+recyclable+unavailable == total");
    CK(imx_space_recyclable_blocks(&s) >= 1, "the block with marked objects is recyclable");

    /* clear all marks + reclaim -> every block is free */
    imx_space_clear_marks(&s);
    imx_space_reclaim(&s);
    CK(imx_space_free_blocks(&s) == tot && imx_space_recyclable_blocks(&s) == 0,
       "no marks -> all blocks free");

    /* evacuation: reserve a target, allocate into it, then return it */
    imx_space_begin_evacuation(&s, 1);
    CK(imx_space_evacuation_active(&s), "begin_evacuation: active");
    void* e = imx_space_allocate_evacuation(&s, 64, 8);
    CK(e && in_data_region(e), "allocate_evacuation: a pointer on a target block");
    size_t free_before = imx_space_free_blocks(&s);
    imx_space_end_evacuation(&s);
    CK(!imx_space_evacuation_active(&s) && imx_space_free_blocks(&s) >= free_before,
       "end_evacuation: inactive, unused targets returned");

    imx_space_destroy(&s);
    printf("\nimmix space allocator (ported C): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
