/* immix_block.c — one Immix block (ported C from AiPL src/immix/block.cpp). */
#include "immix_block.h"
#include <assert.h>

_Static_assert(sizeof(imx_block_t) <= IMX_DATA_START_OFFSET,
               "ImmixBlock header must fit within the reserved metadata lines");

void imx_block_init(imx_block_t* b) {
    imx_lb_clear(&b->line_marks);
    imx_ob_clear(&b->object_starts);
    b->state = IMX_BLOCK_FREE;
    b->hole_count = 0;
    b->_pad = 0;
}

void imx_block_clear_marks(imx_block_t* b) {
    imx_lb_clear(&b->line_marks);
    b->state = IMX_BLOCK_FREE;
    b->hole_count = 0;
}

void imx_block_record_allocation(imx_block_t* b, const void* obj) {
    assert(imx_block_owns(b, obj) && "record_allocation: address not in this block");
    imx_ob_set(&b->object_starts, imx_word_of(obj));
}

void imx_block_clear_object_start(imx_block_t* b, const void* obj) {
    assert(imx_block_owns(b, obj));
    imx_ob_clear_bit(&b->object_starts, imx_word_of(obj));
}

void imx_block_mark_object(imx_block_t* b, const void* obj, size_t size) {
    assert(imx_block_owns(b, obj) && "mark_object: address not in this block");
    assert(size > 0 && "mark_object: zero-size object");

    size_t first_line = imx_line_of(obj);
    assert(first_line >= IMX_DATA_START_LINE && "mark_object: address in metadata region");

    imx_lb_set(&b->line_marks, first_line);
    if (size > IMX_LINE_SIZE) {                    /* the object spans multiple lines */
        uintptr_t last_byte = (uintptr_t)obj + size - 1;
        size_t last_line = imx_line_of((const void*)last_byte);
        assert(last_line < IMX_LINES_PER_BLOCK && "mark_object: object past block end");
        for (size_t i = first_line + 1; i <= last_line; ++i) imx_lb_set(&b->line_marks, i);
    }
}

void imx_block_classify(imx_block_t* b) {
    size_t marked = imx_lb_count_set(&b->line_marks, IMX_DATA_START_LINE, IMX_LINES_PER_BLOCK);
    b->hole_count = (uint16_t)imx_lb_count_holes(&b->line_marks, IMX_DATA_START_LINE, IMX_LINES_PER_BLOCK);
    if (marked == 0)                b->state = IMX_BLOCK_FREE;
    else if (marked == IMX_DATA_LINES) b->state = IMX_BLOCK_UNAVAILABLE;
    else                            b->state = IMX_BLOCK_RECYCLABLE;
}

void imx_block_for_each_object(imx_block_t* b, void (*fn)(void* obj, void* ctx), void* ctx) {
    for (size_t w = IMX_FIRST_DATA_WORD; w < IMX_LAST_DATA_WORD; ++w)
        if (imx_ob_test(&b->object_starts, w)) fn(imx_block_addr_of_word(b, w), ctx);
}
