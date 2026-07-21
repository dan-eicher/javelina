/* immix_line_map.c — line/object bitmaps (ported C from AiPL src/immix/line_map.cpp). */
#include "immix_line_map.h"
#include <string.h>

_Static_assert(IMX_LINES_PER_BLOCK % 8 == 0, "line bitmap requires a multiple of 8 lines");
_Static_assert(IMX_WORDS_PER_BLOCK % 8 == 0, "object bitmap requires a multiple of 8 words");
_Static_assert(sizeof(imx_line_bitmap_t)   == IMX_LINES_PER_BLOCK / 8, "one bit per line");
_Static_assert(sizeof(imx_object_bitmap_t) == IMX_WORDS_PER_BLOCK / 8, "one bit per word");

void imx_lb_clear(imx_line_bitmap_t* m) { memset(m->bits, 0, sizeof m->bits); }
void imx_ob_clear(imx_object_bitmap_t* m) { memset(m->bits, 0, sizeof m->bits); }

void imx_lb_set_range(imx_line_bitmap_t* m, size_t start, size_t end) {
    size_t i = start;
    while (i < end && (i % 8) != 0) { imx_lb_set(m, i); i++; }
    while (i + 8 <= end) { m->bits[i / 8] = 0xFF; i += 8; }
    while (i < end) { imx_lb_set(m, i); i++; }
}

size_t imx_lb_count_set(const imx_line_bitmap_t* m, size_t start, size_t end) {
    size_t count = 0, i = start;
    while (i < end && (i % 8) != 0) { if (imx_lb_test(m, i)) count++; i++; }
    while (i + 8 <= end) { count += (size_t)__builtin_popcount(m->bits[i / 8]); i += 8; }
    while (i < end) { if (imx_lb_test(m, i)) count++; i++; }
    return count;
}

size_t imx_lb_count_holes(const imx_line_bitmap_t* m, size_t start, size_t end) {
    if (start >= end) return 0;
    size_t count = 0, i = start;
    int prev_marked = 1;   /* a virtual marked line before the range so a leading hole counts */
    while (i < end && (i % 8) != 0) {
        int cur = imx_lb_test(m, i);
        if (!cur && prev_marked) count++;
        prev_marked = cur; i++;
    }
    while (i + 8 <= end) {
        uint8_t byte = m->bits[i / 8];
        if (byte == 0xFF) { prev_marked = 1; }
        else if (byte == 0x00) { if (prev_marked) count++; prev_marked = 0; }
        else {
            for (int b = 0; b < 8; ++b) {
                int cur = (byte >> b) & 1u;
                if (!cur && prev_marked) count++;
                prev_marked = cur;
            }
        }
        i += 8;
    }
    while (i < end) {
        int cur = imx_lb_test(m, i);
        if (!cur && prev_marked) count++;
        prev_marked = cur; i++;
    }
    return count;
}

int imx_lb_next_hole(const imx_line_bitmap_t* m, size_t from, size_t end, imx_hole_t* out) {
    size_t i = from;
    while (i < end) {
        while (i < end && imx_lb_test(m, i)) i++;
        if (i >= end) return 0;

        int preceded_by_marked = (i > 0) && imx_lb_test(m, i - 1);
        if (preceded_by_marked) {           /* the line after a marked run may be a conservative spill */
            i++;
            if (i >= end) return 0;
            if (imx_lb_test(m, i)) continue;
        }
        out->start_line = i;
        while (i < end && !imx_lb_test(m, i)) i++;
        out->end_line = i;
        return 1;
    }
    return 0;
}
