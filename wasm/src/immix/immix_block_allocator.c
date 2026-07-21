/* immix_block_allocator.c — the block pool (ported C from AiPL block_allocator.cpp). */
#include "immix_block_allocator.h"
#include <stdlib.h>
#include <string.h>

void imx_block_pool_init(imx_block_pool_t* p) { memset(p, 0, sizeof *p); }

void imx_block_pool_destroy(imx_block_pool_t* p) {
    for (int i = 0; i < bbq_vec_len(p->allocated); i++) free(p->allocated[i]);
    bbq_vec_free(p->allocated);
    bbq_vec_free(p->freelist);
    memset(p, 0, sizeof *p);
}

imx_block_t* imx_block_pool_acquire(imx_block_pool_t* p) {
    p->total_acquired++;
    if (bbq_vec_len(p->freelist)) {                  /* reuse a released block */
        imx_block_t* b = bbq_vec_pop(p->freelist);
        imx_block_clear_marks(b);
        return b;
    }
    imx_block_t* b = aligned_alloc(IMX_BLOCK_SIZE, IMX_BLOCK_SIZE);
    if (!b) return NULL;
    imx_block_init(b);
    bbq_vec_push(p->allocated, b);
    p->total_os_allocs++;
    return b;
}

void imx_block_pool_release(imx_block_pool_t* p, imx_block_t* b) {
    p->total_released++;
    bbq_vec_push(p->freelist, b);
}

size_t imx_block_pool_trim(imx_block_pool_t* p, size_t keep) {
    size_t freed = 0;
    while ((size_t)bbq_vec_len(p->freelist) > keep) {
        imx_block_t* b = bbq_vec_pop(p->freelist);
        int n = bbq_vec_len(p->allocated);
        for (int i = 0; i < n; i++)                  /* swap-remove from the live set */
            if (p->allocated[i] == b) { p->allocated[i] = bbq_vec_pop(p->allocated); break; }
        free(b);
        p->total_os_frees++; freed++;
    }
    return freed;
}
