/*
 * immix_line_map.h — the line + object-start bitmaps of an Immix block.
 *
 * Ported to pure C from AiPL's C++ Immix (include/immix/line_map.h) — javelina's
 * runtime stays pure C (port, never link). A block is BLOCK_SIZE bytes split into
 * LINE_SIZE lines; the line bitmap marks live lines, the object bitmap marks each
 * 8-byte word where an object header begins (so the sweep walks objects with no
 * external list). `next_hole` finds runs of free lines for the bump allocator.
 */
#ifndef IMX_LINE_MAP_H
#define IMX_LINE_MAP_H

#include <stddef.h>
#include <stdint.h>

#define IMX_BLOCK_SIZE      (32u * 1024u)
#define IMX_LINE_SIZE       128u
#define IMX_LINES_PER_BLOCK (IMX_BLOCK_SIZE / IMX_LINE_SIZE)    /* 256 */
#define IMX_OBJECT_ALIGN    8u
#define IMX_WORDS_PER_BLOCK (IMX_BLOCK_SIZE / IMX_OBJECT_ALIGN) /* 4096 */

typedef struct { size_t start_line, end_line; } imx_hole_t;
static inline size_t imx_hole_lines(imx_hole_t h) { return h.end_line - h.start_line; }
static inline size_t imx_hole_bytes(imx_hole_t h) { return imx_hole_lines(h) * IMX_LINE_SIZE; }

/* One bit per line (live/dead). */
typedef struct { uint8_t bits[IMX_LINES_PER_BLOCK / 8]; } imx_line_bitmap_t;   /* 32 B */
/* One bit per 8-byte word, set where an object header begins. */
typedef struct { uint8_t bits[IMX_WORDS_PER_BLOCK / 8]; } imx_object_bitmap_t; /* 512 B */

void   imx_lb_clear(imx_line_bitmap_t* m);
static inline int  imx_lb_test(const imx_line_bitmap_t* m, size_t i) { return (m->bits[i / 8] >> (i % 8)) & 1u; }
static inline void imx_lb_set(imx_line_bitmap_t* m, size_t i)        { m->bits[i / 8] |= (uint8_t)(1u << (i % 8)); }
void   imx_lb_set_range(imx_line_bitmap_t* m, size_t start, size_t end);
size_t imx_lb_count_set(const imx_line_bitmap_t* m, size_t start, size_t end);
size_t imx_lb_count_holes(const imx_line_bitmap_t* m, size_t start, size_t end);
/* Find the next run of free lines in [from, end). Returns 1 + fills *out, else 0. */
int    imx_lb_next_hole(const imx_line_bitmap_t* m, size_t from, size_t end, imx_hole_t* out);

void   imx_ob_clear(imx_object_bitmap_t* m);
static inline int  imx_ob_test(const imx_object_bitmap_t* m, size_t w)      { return (m->bits[w / 8] >> (w % 8)) & 1u; }
static inline void imx_ob_set(imx_object_bitmap_t* m, size_t w)             { m->bits[w / 8] |= (uint8_t)(1u << (w % 8)); }
static inline void imx_ob_clear_bit(imx_object_bitmap_t* m, size_t w)       { m->bits[w / 8] &= (uint8_t)~(1u << (w % 8)); }

#endif /* IMX_LINE_MAP_H */
