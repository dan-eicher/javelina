/* immix_space.c — the Immix space allocator (ported C from AiPL src/immix/space.cpp). */
#include "immix_space.h"
#include <assert.h>

/* ── AddressSanitizer manual poisoning (ASAN builds only; no-op otherwise) ──
 * Immix manages its own mmap'd regions, invisible to ASAN by default. We poison free
 * heap memory and unpoison it on allocation, so a stale pointer to an evacuated object's
 * old (reclaimed) location — the classic moving-GC missed-update bug — faults on access
 * with the culprit in the report, instead of silently reading a garbage/reused rtt. */
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
#  include <sanitizer/asan_interface.h>
#  define IMX_POISON(addr, size)   __asan_poison_memory_region((addr), (size))
#  define IMX_UNPOISON(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#  define IMX_POISON(addr, size)   ((void)(addr), (void)(size))   /* reference args → no -Wunused in normal builds */
#  define IMX_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif

static uint8_t* align_up(uint8_t* p, size_t align) {
    uintptr_t v = (uintptr_t)p;
    v = (v + align - 1) & ~((uintptr_t)align - 1);
    return (uint8_t*)v;
}

void imx_space_init(imx_space_t* s) {
    memset(s, 0, sizeof *s);
    imx_block_pool_init(&s->pool);
}

void imx_space_destroy(imx_space_t* s) {
    bbq_vec_free(s->all_blocks); bbq_vec_free(s->free_blocks);
    bbq_vec_free(s->recyclable_blocks); bbq_vec_free(s->unavailable_blocks);
    bbq_vec_free(s->evacuation_targets); bbq_vec_free(s->evacuation_candidates);
    imx_block_pool_destroy(&s->pool);
    memset(s, 0, sizeof *s);
}

/* ── block acquisition ── */
static imx_block_t* take_free_block(imx_space_t* s) {
    if (bbq_vec_len(s->free_blocks)) return bbq_vec_pop(s->free_blocks);
    imx_block_t* b = imx_block_pool_acquire(&s->pool);
    if (b) bbq_vec_push(s->all_blocks, b);
    return b;
}
static imx_block_t* take_recyclable_block(imx_space_t* s) {
    if (!bbq_vec_len(s->recyclable_blocks)) return NULL;
    return bbq_vec_pop(s->recyclable_blocks);
}

/* ── small allocator: bump through holes ── */
static int refill_small(imx_space_t* s) {
    imx_hole_t h;
    if (s->small_block && imx_lb_next_hole(&s->small_block->line_marks, s->small_next_line, IMX_LINES_PER_BLOCK, &h)) {
        s->small_cursor = imx_block_line_addr(s->small_block, h.start_line);
        s->small_limit  = imx_block_line_addr(s->small_block, h.end_line);
        s->small_next_line = h.end_line;
        return 1;
    }
    for (;;) {                                        /* acquire blocks until one yields a hole */
        imx_block_t* b = take_recyclable_block(s);
        if (!b) b = take_free_block(s);
        if (!b) return 0;
        s->small_block = b;
        s->small_next_line = IMX_DATA_START_LINE;
        if (imx_lb_next_hole(&b->line_marks, s->small_next_line, IMX_LINES_PER_BLOCK, &h)) {
            s->small_cursor = imx_block_line_addr(b, h.start_line);
            s->small_limit  = imx_block_line_addr(b, h.end_line);
            s->small_next_line = h.end_line;
            return 1;
        }
        s->small_cursor = NULL; s->small_limit = NULL;   /* no usable hole (spill rule); try another */
    }
}

static void* alloc_medium(imx_space_t* s, size_t size, size_t alignment);

static void* alloc_small(imx_space_t* s, size_t size, size_t alignment) {
    if (s->small_cursor) {                             /* fast path: bump in the current hole */
        uint8_t* p = align_up(s->small_cursor, alignment);
        if (p + size <= s->small_limit) {
            s->small_cursor = p + size;
            imx_block_record_allocation(s->small_block, p);
            return p;
        }
    }
    if (!refill_small(s)) return NULL;
    uint8_t* p = align_up(s->small_cursor, alignment);
    if (p + size <= s->small_limit) {
        s->small_cursor = p + size;
        imx_block_record_allocation(s->small_block, p);
        return p;
    }
    while (refill_small(s)) {                          /* alignment waste ate the hole; keep trying */
        p = align_up(s->small_cursor, alignment);
        if (p + size <= s->small_limit) {
            s->small_cursor = p + size;
            imx_block_record_allocation(s->small_block, p);
            return p;
        }
    }
    return alloc_medium(s, size, alignment);           /* pathological: fall back to a whole block */
}

/* ── medium allocator: a whole free block ── */
static int refill_medium(imx_space_t* s) {
    imx_block_t* b = take_free_block(s);
    if (!b) return 0;
    s->medium_block = b;
    s->medium_cursor = imx_block_data_start(b);
    s->medium_limit  = imx_block_data_end(b);
    return 1;
}
static void* alloc_medium(imx_space_t* s, size_t size, size_t alignment) {
    if (s->medium_cursor) {
        uint8_t* p = align_up(s->medium_cursor, alignment);
        if (p + size <= s->medium_limit) {
            s->medium_cursor = p + size;
            imx_block_record_allocation(s->medium_block, p);
            return p;
        }
    }
    if (!refill_medium(s)) return NULL;
    uint8_t* p = align_up(s->medium_cursor, alignment);
    if (p + size <= s->medium_limit) {
        s->medium_cursor = p + size;
        imx_block_record_allocation(s->medium_block, p);
        return p;
    }
    return NULL;                                       /* larger than a block's data area → LOS (future) */
}

void* imx_space_allocate(imx_space_t* s, size_t size, size_t alignment) {
    if (size == 0) return NULL;
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0 && "alignment must be a power of two");
    void* p = size <= IMX_SMALL_MAX ? alloc_small(s, size, alignment) : alloc_medium(s, size, alignment);
    if (p) IMX_UNPOISON(p, size);   /* fresh object memory is addressable */
    return p;
}

void imx_space_clear_marks(imx_space_t* s) {
    for (int i = 0; i < bbq_vec_len(s->all_blocks); i++) imx_block_clear_marks(s->all_blocks[i]);
}

void imx_space_reclaim(imx_space_t* s) {
    bbq_vec_clear(s->free_blocks); bbq_vec_clear(s->recyclable_blocks); bbq_vec_clear(s->unavailable_blocks);
    for (int i = 0; i < bbq_vec_len(s->all_blocks); i++) {
        imx_block_t* b = s->all_blocks[i];
        imx_block_classify(b);
        if (b->state == IMX_BLOCK_FREE) {
            uint8_t* ds = imx_block_data_start(b);      /* now-dead block: poison it whole so a stale */
            IMX_POISON(ds, (size_t)(imx_block_data_end(b) - ds));   /* ref into it faults on access */
            bbq_vec_push(s->free_blocks, b);
        }
        else if (b->state == IMX_BLOCK_RECYCLABLE)  bbq_vec_push(s->recyclable_blocks, b);
        else                                        bbq_vec_push(s->unavailable_blocks, b);
    }
    s->small_block = NULL; s->small_cursor = NULL; s->small_limit = NULL; s->small_next_line = 0;
    s->medium_block = NULL; s->medium_cursor = NULL; s->medium_limit = NULL;
}

/* ── opportunistic evacuation ── */
void imx_space_begin_evacuation(imx_space_t* s, size_t max_target_blocks) {
    s->evacuation_active = 1;
    bbq_vec_clear(s->evacuation_candidates);                       /* snapshot: currently-recyclable blocks */
    for (int i = 0; i < bbq_vec_len(s->recyclable_blocks); i++)
        bbq_vec_push(s->evacuation_candidates, s->recyclable_blocks[i]);

    bbq_vec_clear(s->evacuation_targets);                          /* reserve target blocks from the free list */
    size_t nfree = (size_t)bbq_vec_len(s->free_blocks);
    size_t take = max_target_blocks < nfree ? max_target_blocks : nfree;
    for (size_t i = 0; i < take; ++i) bbq_vec_push(s->evacuation_targets, bbq_vec_pop(s->free_blocks));
    s->evac_target_idx = 0;
    if (bbq_vec_len(s->evacuation_targets)) {
        imx_block_t* b = s->evacuation_targets[0];
        s->evac_cursor = imx_block_data_start(b); s->evac_limit = imx_block_data_end(b);
    } else { s->evac_cursor = NULL; s->evac_limit = NULL; }
}

int imx_space_is_evacuation_candidate(const imx_space_t* s, imx_block_t* block) {
    if (!s->evacuation_active) return 0;
    for (int i = 0; i < bbq_vec_len(s->evacuation_candidates); i++)
        if (s->evacuation_candidates[i] == block) return 1;
    return 0;
}

void* imx_space_allocate_evacuation(imx_space_t* s, size_t size, size_t alignment) {
    if (!s->evacuation_active) return NULL;
    for (;;) {
        if (s->evac_cursor) {
            uint8_t* p = align_up(s->evac_cursor, alignment);
            if (p + size <= s->evac_limit) {
                s->evac_cursor = p + size;
                imx_block_record_allocation(imx_block_of(p), p);
                IMX_UNPOISON(p, size);   /* evacuation target memory is addressable */
                return p;
            }
        }
        s->evac_target_idx++;
        if (s->evac_target_idx >= (size_t)bbq_vec_len(s->evacuation_targets)) return NULL;  /* out of headroom */
        imx_block_t* b = s->evacuation_targets[s->evac_target_idx];
        s->evac_cursor = imx_block_data_start(b); s->evac_limit = imx_block_data_end(b);
    }
}

void imx_space_end_evacuation(imx_space_t* s) {
    for (int i = 0; i < bbq_vec_len(s->evacuation_targets); i++)   /* unused targets back to the free pool */
        bbq_vec_push(s->free_blocks, s->evacuation_targets[i]);
    bbq_vec_clear(s->evacuation_targets); bbq_vec_clear(s->evacuation_candidates);
    s->evac_cursor = NULL; s->evac_limit = NULL; s->evac_target_idx = 0;
    s->evacuation_active = 0;
}

void imx_space_for_each_block(imx_space_t* s, void (*fn)(imx_block_t* b, void* ctx), void* ctx) {
    for (int i = 0; i < bbq_vec_len(s->all_blocks); i++) fn(s->all_blocks[i], ctx);
}
