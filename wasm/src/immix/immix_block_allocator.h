/*
 * immix_block_allocator.h — the block pool (ported C from AiPL block_allocator).
 *
 * Owns OS-acquired BLOCK_SIZE-aligned blocks: a free list for reuse + the set of
 * all live blocks (for trim + teardown). The C++ std::vector/std::unordered_set
 * become BBQ `bbq_vec`s (block counts are modest, so trim's linear swap-remove is
 * fine — no need for a hash set).
 */
#ifndef IMX_BLOCK_ALLOCATOR_H
#define IMX_BLOCK_ALLOCATOR_H

#include "immix_block.h"
#include "bbq_vec.h"
#include <stdint.h>

typedef struct {
    imx_block_t** allocated;   /* bbq_vec: all OS-acquired blocks */
    imx_block_t** freelist;    /* bbq_vec: released, reusable (LIFO) */
    uint64_t total_acquired, total_released, total_os_allocs, total_os_frees;
} imx_block_pool_t;

void         imx_block_pool_init(imx_block_pool_t* p);
void         imx_block_pool_destroy(imx_block_pool_t* p);
imx_block_t* imx_block_pool_acquire(imx_block_pool_t* p);           /* reuse or OS-alloc; NULL on OOM */
void         imx_block_pool_release(imx_block_pool_t* p, imx_block_t* b);
size_t       imx_block_pool_trim(imx_block_pool_t* p, size_t keep);  /* free pooled blocks down to `keep` */

static inline size_t imx_block_pool_allocated(imx_block_pool_t* p) { return (size_t)bbq_vec_len(p->allocated); }
static inline size_t imx_block_pool_free(imx_block_pool_t* p)      { return (size_t)bbq_vec_len(p->freelist); }
static inline size_t imx_block_pool_live(imx_block_pool_t* p)      { return imx_block_pool_allocated(p) - imx_block_pool_free(p); }

#endif /* IMX_BLOCK_ALLOCATOR_H */
