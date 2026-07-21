// test_immix_line.c — the ported Immix line/object bitmaps: set/test/range, popcount
// counting, hole counting, and next_hole's conservative-spill rule (a free line
// immediately after a marked run is skipped, since a small object may spill into it).
#include "immix_line_map.h"
#include <stdio.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-44s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    imx_line_bitmap_t lb; imx_lb_clear(&lb);
    CK(!imx_lb_test(&lb, 0) && !imx_lb_test(&lb, 255), "clear: all lines free");
    imx_lb_set(&lb, 5); imx_lb_set(&lb, 200);
    CK(imx_lb_test(&lb,5) && imx_lb_test(&lb,200) && !imx_lb_test(&lb,6), "set/test single lines");

    imx_lb_clear(&lb);
    imx_lb_set_range(&lb, 10, 20);
    CK(imx_lb_count_set(&lb,0,256)==10 && imx_lb_test(&lb,10) && imx_lb_test(&lb,19) && !imx_lb_test(&lb,20),
       "set_range [10,20): 10 lines, exclusive end");

    /* hole counting: [0,256) with [10,20) marked = 2 holes (before and after) */
    CK(imx_lb_count_holes(&lb,0,256)==2, "count_holes: 2 holes around a marked run");

    /* next_hole + the conservative-spill rule. marked = {10..19}. Holes: [0,10), then
     * after the marked run line 20 is skipped (spill guard), so [21,256). */
    imx_hole_t h;
    int f1 = imx_lb_next_hole(&lb, 0, 256, &h);
    CK(f1 && h.start_line==0 && h.end_line==10, "next_hole from 0: [0,10)");
    int f2 = imx_lb_next_hole(&lb, 20, 256, &h);   /* line 20 follows marked 19 -> spill-skip */
    CK(f2 && h.start_line==21 && h.end_line==256, "next_hole after marked: spill line skipped -> [21,256)");

    /* a fully-marked block has no hole */
    imx_lb_clear(&lb); imx_lb_set_range(&lb, 0, 256);
    CK(!imx_lb_next_hole(&lb, 0, 256, &h), "next_hole: full block -> none");

    /* object bitmap: per-word set/test/clear */
    imx_object_bitmap_t ob; imx_ob_clear(&ob);
    imx_ob_set(&ob, 100); imx_ob_set(&ob, 4095);
    CK(imx_ob_test(&ob,100) && imx_ob_test(&ob,4095) && !imx_ob_test(&ob,101), "object bitmap set/test");
    imx_ob_clear_bit(&ob, 100);
    CK(!imx_ob_test(&ob,100) && imx_ob_test(&ob,4095), "object bitmap clear_bit");

    printf("\nimmix line/object bitmaps (ported C): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
