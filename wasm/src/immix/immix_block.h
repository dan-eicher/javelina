/*
 * immix_block.h — one Immix block (ported C from AiPL include/immix/block.h).
 *
 * A block is BLOCK_SIZE bytes, BLOCK_SIZE-ALIGNED, with its metadata header in-band
 * at the base (the bitmaps + state), so any interior pointer recovers its block in
 * O(1) by masking (block_of) — no per-object back-pointer. The object-start bitmap
 * lets the sweep walk every object with no external list.
 */
#ifndef IMX_BLOCK_H
#define IMX_BLOCK_H

#include "immix_line_map.h"
#include <stdint.h>

enum { IMX_BLOCK_FREE = 0, IMX_BLOCK_RECYCLABLE = 1, IMX_BLOCK_UNAVAILABLE = 2 };

typedef struct imx_block {
    imx_line_bitmap_t   line_marks;     /* 32 B  — live lines */
    imx_object_bitmap_t object_starts;  /* 512 B — object-header words */
    uint8_t  state;                     /* IMX_BLOCK_* */
    uint16_t hole_count;
    /* Defragmentation statistics, recorded by classify during the sweep at the END of a
     * collection and DELIBERATELY NOT cleared by clear_marks — Immix §3.2.1 selects
     * candidates from "conservative statistics from the previous collection", so these
     * have to outlive the mark phase that erases line_marks/hole_count. Reading the live
     * recyclable list instead is what made evacuation unreachable: the allocator drains
     * it between collections, so it is empty at exactly the moment a collection runs. */
    uint16_t defrag_holes;              /* hole count at the last sweep */
    uint16_t defrag_marked;             /* marked data lines at the last sweep */
    uint8_t  defrag_candidate;          /* selected as an evacuation source this collection */
} imx_block_t;

#define IMX_METADATA_BYTES   (sizeof(imx_line_bitmap_t) + sizeof(imx_object_bitmap_t) + 1u + 2u + 2u + 2u + 1u)
#define IMX_METADATA_LINES   ((IMX_METADATA_BYTES + IMX_LINE_SIZE - 1u) / IMX_LINE_SIZE)
#define IMX_DATA_START_LINE  IMX_METADATA_LINES
#define IMX_DATA_START_OFFSET (IMX_DATA_START_LINE * IMX_LINE_SIZE)
#define IMX_DATA_LINES       (IMX_LINES_PER_BLOCK - IMX_METADATA_LINES)
#define IMX_DATA_BYTES       (IMX_DATA_LINES * IMX_LINE_SIZE)
#define IMX_FIRST_DATA_WORD  (IMX_DATA_START_OFFSET / IMX_OBJECT_ALIGN)
#define IMX_LAST_DATA_WORD   (IMX_BLOCK_SIZE / IMX_OBJECT_ALIGN)

/* address arithmetic — all O(1) masks off the BLOCK_SIZE alignment */
static inline imx_block_t* imx_block_of(const void* addr) {
    return (imx_block_t*)((uintptr_t)addr & ~((uintptr_t)IMX_BLOCK_SIZE - 1u));
}
static inline size_t imx_word_of(const void* addr) {
    return ((uintptr_t)addr & (IMX_BLOCK_SIZE - 1u)) / IMX_OBJECT_ALIGN;
}
static inline size_t imx_line_of(const void* addr) {
    return ((uintptr_t)addr & (IMX_BLOCK_SIZE - 1u)) / IMX_LINE_SIZE;
}
static inline uint8_t* imx_block_addr_of_word(imx_block_t* b, size_t w) {
    return (uint8_t*)b + w * IMX_OBJECT_ALIGN;
}
static inline uint8_t* imx_block_line_addr(imx_block_t* b, size_t line) {
    return (uint8_t*)b + line * IMX_LINE_SIZE;
}
static inline uint8_t* imx_block_data_start(imx_block_t* b) { return (uint8_t*)b + IMX_DATA_START_OFFSET; }
static inline uint8_t* imx_block_data_end(imx_block_t* b)   { return (uint8_t*)b + IMX_BLOCK_SIZE; }
static inline int imx_block_owns(const imx_block_t* b, const void* addr) { return imx_block_of(addr) == b; }

void imx_block_init(imx_block_t* b);
void imx_block_clear_marks(imx_block_t* b);
void imx_block_record_allocation(imx_block_t* b, const void* obj);
void imx_block_clear_object_start(imx_block_t* b, const void* obj);
void imx_block_mark_object(imx_block_t* b, const void* obj, size_t size);
void imx_block_classify(imx_block_t* b);

/* find the next free-line hole at/after `from_line` (default IMX_DATA_START_LINE) */
static inline int imx_block_next_hole(const imx_block_t* b, size_t from_line, imx_hole_t* out) {
    return imx_lb_next_hole(&b->line_marks, from_line, IMX_LINES_PER_BLOCK, out);
}
/* visit every live object-start address in the block */
void imx_block_for_each_object(imx_block_t* b, void (*fn)(void* obj, void* ctx), void* ctx);

#endif /* IMX_BLOCK_H */
