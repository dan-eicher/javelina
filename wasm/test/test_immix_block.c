// test_immix_block.c — the ported Immix block: O(1) block_of off the alignment,
// object-start recording + iteration, mark_object line-spanning, and classify
// (free/recyclable/unavailable).
#include "immix_block.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-46s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

typedef struct { void* seen[8]; int n; } collect_t;
static void collect(void* obj, void* ctx){ collect_t* c=ctx; if(c->n<8) c->seen[c->n++]=obj; }

int main(void){
    imx_block_t* b = aligned_alloc(IMX_BLOCK_SIZE, IMX_BLOCK_SIZE);
    imx_block_init(b);

    /* block_of recovers the base from any interior pointer */
    uint8_t* mid = (uint8_t*)b + 5000;
    CK(imx_block_of(mid) == b && imx_block_owns(b, mid), "block_of / owns: interior pointer -> base");
    CK(b->state == IMX_BLOCK_FREE && b->hole_count == 0, "init: free, no holes");

    /* record two objects in the data region; iterate them back */
    void* o1 = imx_block_data_start(b);
    void* o2 = imx_block_data_start(b) + 256;
    imx_block_record_allocation(b, o1);
    imx_block_record_allocation(b, o2);
    collect_t c = {{0},0};
    imx_block_for_each_object(b, collect, &c);
    CK(c.n==2 && c.seen[0]==o1 && c.seen[1]==o2, "record + for_each_object: both seen, in order");
    imx_block_clear_object_start(b, o1);
    c.n=0; imx_block_for_each_object(b, collect, &c);
    CK(c.n==1 && c.seen[0]==o2, "clear_object_start: o1 gone");

    /* mark a small (single-line) object */
    imx_block_mark_object(b, o1, 64);
    CK(imx_lb_count_set(&b->line_marks, IMX_DATA_START_LINE, IMX_LINES_PER_BLOCK)==1, "mark_object 64B: 1 line");

    /* mark a 300-byte object at data_start -> spans 3 lines (640..939 -> lines 5,6,7) */
    imx_block_clear_marks(b);
    imx_block_mark_object(b, imx_block_data_start(b), 300);
    CK(imx_lb_count_set(&b->line_marks, IMX_DATA_START_LINE, IMX_LINES_PER_BLOCK)==3, "mark_object 300B: spans 3 lines");

    /* classify: a few marked lines -> recyclable */
    imx_block_classify(b);
    CK(b->state == IMX_BLOCK_RECYCLABLE, "classify: partial -> recyclable");
    /* no marks -> free */
    imx_block_clear_marks(b); imx_block_classify(b);
    CK(b->state == IMX_BLOCK_FREE, "classify: empty -> free");
    /* every data line marked -> unavailable */
    imx_lb_set_range(&b->line_marks, IMX_DATA_START_LINE, IMX_LINES_PER_BLOCK); imx_block_classify(b);
    CK(b->state == IMX_BLOCK_UNAVAILABLE, "classify: full -> unavailable");

    free(b);
    printf("\nimmix block (ported C): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
