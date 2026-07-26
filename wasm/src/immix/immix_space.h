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
    imx_block_t** evacuation_targets;     /* bbq_vec: the reserve blocks in use this collection */
    /* §3.2: "immix sets aside a small number of free blocks that it never returns to the
     * global allocator and only ever uses for evacuating." take_free_block never touches
     * this vector, so the mutator cannot drain the headroom before a collection needs it. */
    imx_block_t** evac_reserve;           /* bbq_vec: the compaction headroom itself */
    imx_block_t** defrag_scratch;         /* bbq_vec: fragmented blocks, rebuilt per collection */
    size_t evac_from_reserve;             /* how many leading targets came from evac_reserve */
    uint8_t* evac_cursor; uint8_t* evac_limit; size_t evac_target_idx;
} imx_space_t;

void  imx_space_init(imx_space_t* s);
void  imx_space_destroy(imx_space_t* s);
void* imx_space_allocate(imx_space_t* s, size_t size, size_t alignment);   /* NULL only on OOM */
void  imx_space_clear_marks(imx_space_t* s);
/* Reclassify, rebuild the state partitions, and re-establish the evacuation headroom:
 * `headroom_blocks` free blocks are set aside for the NEXT collection's evacuation and are
 * not visible to the allocator (Immix §3.2). Pass 0 for a space with no defragmentation. */
void  imx_space_reclaim(imx_space_t* s, size_t headroom_blocks);

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
static inline size_t imx_space_reserve_blocks(imx_space_t* s)     { return (size_t)bbq_vec_len(s->evac_reserve); }
/* Blocks holding no live data: the allocator's free list PLUS the evacuation headroom that
 * reclaim set aside from it. When nothing is reachable this equals total_blocks — headroom
 * is reclaimed memory too, it is just no longer visible to the mutator's allocator. */
static inline size_t imx_space_empty_blocks(imx_space_t* s) {
    return imx_space_free_blocks(s) + imx_space_reserve_blocks(s);
}
/* The STRONG form of "everything was reclaimed": no block holds any live line. A counting
 * claim like `free == total` is the weak form — it can be satisfied by moving blocks into
 * another bucket, which is how a leak gets normalised when the partition changes shape. A
 * leaked object marks lines, so its block classifies RECYCLABLE or UNAVAILABLE, and no
 * amount of headroom can hide it from this. */
static inline int imx_space_all_reclaimed(imx_space_t* s) {
    return imx_space_recyclable_blocks(s) == 0 && imx_space_unavailable_blocks(s) == 0
        && imx_space_empty_blocks(s) == imx_space_total_blocks(s);
}

#endif /* IMX_SPACE_H */
