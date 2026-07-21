/*
 * immix_space.h — the Immix space (ported C from AiPL include/immix/space.h).
 *
 * The allocator: small objects (<= LINE_SIZE) bump through holes (runs of free
 * lines); medium objects take a whole free block (overflow allocator); large is a
 * future Large-Object-Space. A collection cycle is driven by the caller:
 * clear_marks() -> mark live (sets line marks) -> reclaim() (reclassify + rebuild
 * the free/recyclable/unavailable lists). Opportunistic evacuation (begin/end) moves
 * objects out of fragmented blocks to defragment. std::vector -> bbq_vec.
 */
#ifndef IMX_SPACE_H
#define IMX_SPACE_H

#include "immix_block.h"
#include "immix_block_allocator.h"

#define IMX_SMALL_MAX IMX_LINE_SIZE   /* <= this: small (bump-in-holes); above: medium (whole block) */
/* The largest object a single block can hold (its data area). Anything above this cannot fit in the
 * block/line engine at all and goes to the large-object space (LOS) — dedicated malloc'd storage,
 * marked in place (never evacuated), swept whole. A fresh block's cursor starts data-aligned, so an
 * object up to this size always fits in a fresh block; strictly above it never does. */
#define IMX_MEDIUM_MAX ((uint32_t)(IMX_BLOCK_SIZE - IMX_DATA_START_OFFSET))

typedef struct {
    imx_block_pool_t pool;
    imx_block_t** all_blocks;          /* bbq_vec: every block ever acquired */
    imx_block_t** free_blocks;         /* bbq_vec: post-reclaim partition by state */
    imx_block_t** recyclable_blocks;
    imx_block_t** unavailable_blocks;

    uint8_t* small_cursor; uint8_t* small_limit; imx_block_t* small_block; size_t small_next_line;
    uint8_t* medium_cursor; uint8_t* medium_limit; imx_block_t* medium_block;

    int evacuation_active;
    imx_block_t** evacuation_targets;     /* bbq_vec: free blocks reserved as evac destinations */
    imx_block_t** evacuation_candidates;  /* bbq_vec: blocks whose objects may be evacuated */
    uint8_t* evac_cursor; uint8_t* evac_limit; size_t evac_target_idx;
} imx_space_t;

void  imx_space_init(imx_space_t* s);
void  imx_space_destroy(imx_space_t* s);
void* imx_space_allocate(imx_space_t* s, size_t size, size_t alignment);   /* NULL only on OOM */
void  imx_space_clear_marks(imx_space_t* s);
void  imx_space_reclaim(imx_space_t* s);

void  imx_space_begin_evacuation(imx_space_t* s, size_t max_target_blocks);
int   imx_space_is_evacuation_candidate(const imx_space_t* s, imx_block_t* block);
void* imx_space_allocate_evacuation(imx_space_t* s, size_t size, size_t alignment);
void  imx_space_end_evacuation(imx_space_t* s);
static inline int imx_space_evacuation_active(const imx_space_t* s) { return s->evacuation_active; }

void  imx_space_for_each_block(imx_space_t* s, void (*fn)(imx_block_t* b, void* ctx), void* ctx);

static inline size_t imx_space_total_blocks(imx_space_t* s)       { return (size_t)bbq_vec_len(s->all_blocks); }
static inline size_t imx_space_free_blocks(imx_space_t* s)        { return (size_t)bbq_vec_len(s->free_blocks); }
static inline size_t imx_space_recyclable_blocks(imx_space_t* s)  { return (size_t)bbq_vec_len(s->recyclable_blocks); }
static inline size_t imx_space_unavailable_blocks(imx_space_t* s) { return (size_t)bbq_vec_len(s->unavailable_blocks); }

#endif /* IMX_SPACE_H */
