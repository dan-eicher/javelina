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
    bbq_vec_free(s->evacuation_targets); bbq_vec_free(s->evac_reserve);
    bbq_vec_free(s->defrag_scratch);
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

void imx_space_reclaim(imx_space_t* s, size_t headroom_blocks) {
    bbq_vec_clear(s->free_blocks); bbq_vec_clear(s->recyclable_blocks); bbq_vec_clear(s->unavailable_blocks);
    bbq_vec_clear(s->evac_reserve);
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

    /* §3.2 compaction headroom: set aside free blocks for the NEXT collection's evacuation.
     * This is the whole point of doing it HERE rather than at the top of a collection — the
     * mutator runs between now and then and would otherwise consume every free block, which
     * is precisely why it is out of free blocks when the collection fires. Blocks in the
     * reserve are invisible to take_free_block, so they survive that. */
    for (size_t i = 0; i < headroom_blocks && bbq_vec_len(s->free_blocks); i++)
        bbq_vec_push(s->evac_reserve, bbq_vec_pop(s->free_blocks));

    s->small_block = NULL; s->small_cursor = NULL; s->small_limit = NULL; s->small_next_line = 0;
    s->medium_block = NULL; s->medium_cursor = NULL; s->medium_limit = NULL;
}

/* ── opportunistic evacuation (Immix §3.2 / §3.2.1) ── */

/* §3.2.1 candidate selection, verbatim in structure:
 *
 *   "it selects candidate blocks with the greatest number of holes since holes indicate
 *    fragmentation ... immix uses two histograms indexed by hole count; a mark histogram
 *    estimating required space and an available histogram reflecting available space ...
 *    To identify candidates, immix walks the histograms, starting with the most fragmented
 *    bin, increments the required space by the volume in the mark histogram bin, and it
 *    decrements the available space by the volume in the available histogram bin ... When
 *    including the blocks in the bin would exceed the estimated available space, immix
 *    selects as defragmentation candidates all blocks in the previous bin and higher."
 *
 * Both histograms are built from the per-block statistics recorded at the LAST sweep, so
 * the mutator's allocation since then cannot erase them. The result is a single hole-count
 * threshold, which turns the candidate test into O(1) — it used to be a linear scan of a
 * candidate vector per marked object, i.e. O(live x candidates) every collection. */
#define IMX_HOLE_BINS (IMX_DATA_LINES / 2u + 2u)   /* a hole needs a free line + a gap, so <= 126 */

/* ONE DELIBERATE DEVIATION from §3.2.1, recorded rather than mis-transcribed.
 *
 * The paper selects whole bins ("all blocks in the previous bin and higher"), which works
 * when a large heap spreads blocks over many distinct hole counts. Measured on a small heap,
 * all five fragmented blocks shared ONE bin holding 775 marked lines against 251 lines of
 * target space, so whole-bin granularity is all-or-nothing and selects NOTHING — worse than
 * useless. We keep the paper's order (most fragmented first) and continue its own cost/benefit
 * reasoning below bin granularity: within a bin, take the CHEAPEST blocks first — fewest
 * marked lines — because that frees the most blocks per line copied. Arbitrary order would
 * not be a policy; this is the same greedy the histogram walk expresses, at finer grain.
 *
 * The paper's available-histogram decrement does not apply here; the reason is at the
 * `avail_lines` declaration below, where it can be checked against the code.
 *
 * Selection marks a per-block flag, so the candidate test stays O(1). It was previously a
 * linear scan of a candidate vector per marked object, i.e. O(live x candidates). */
static void select_candidates(imx_space_t* s, size_t ntargets) {
    bbq_vec_clear(s->defrag_scratch);
    size_t maxbin = 0;
    for (int i = 0; i < bbq_vec_len(s->all_blocks); i++) {
        imx_block_t* b = s->all_blocks[i];
        b->defrag_candidate = 0;
        if (b->defrag_marked == 0 || b->defrag_marked >= IMX_DATA_LINES) continue;  /* free or full */
        if (b->defrag_holes == 0) continue;                                          /* not fragmented */
        bbq_vec_push(s->defrag_scratch, b);
        size_t bin = b->defrag_holes < IMX_HOLE_BINS ? b->defrag_holes : IMX_HOLE_BINS - 1;
        if (bin > maxbin) maxbin = bin;
    }

    /* `avail_lines` is the space that can RECEIVE evacuated objects — the target blocks, whose
     * every data line is free by construction. `required` is what the chosen candidates must
     * copy. There is no available-histogram decrement here and that is not an omission: the
     * paper decrements because ITS evacuation allocator also draws on unused recyclable blocks,
     * so one block could be either a target or a candidate and choosing it as a candidate
     * withdraws it from the target pool. Our targets come from the reserve and the free list;
     * candidates are only ever recyclable blocks. The pools are disjoint, so there is nothing
     * to withdraw — subtracting anyway double-counts, and measurably selects nothing. */
    size_t avail_lines = ntargets * IMX_DATA_LINES;
    size_t required = 0;
    int nfrag = bbq_vec_len(s->defrag_scratch);
    for (size_t bin = maxbin; bin >= 1; bin--) {       /* most fragmented bin first */
        for (;;) {
            /* cheapest-first WITHIN the bin: fewest marked lines frees the most blocks per
             * line copied. Linear pick rather than a sort — a bin holds few blocks. */
            imx_block_t* best = NULL;
            for (int i = 0; i < nfrag; i++) {
                imx_block_t* b = s->defrag_scratch[i];
                if (b->defrag_candidate) continue;
                size_t bbin = b->defrag_holes < IMX_HOLE_BINS ? b->defrag_holes : IMX_HOLE_BINS - 1;
                if (bbin != bin) continue;
                if (!best || b->defrag_marked < best->defrag_marked) best = b;
            }
            if (!best) break;                                          /* bin exhausted */
            if (required + best->defrag_marked > avail_lines) break;    /* cheapest no longer fits */
            best->defrag_candidate = 1;
            required += best->defrag_marked;
        }
    }
}

void imx_space_begin_evacuation(imx_space_t* s, size_t max_target_blocks) {
    /* §3.2: the collector "uses the same allocator as the mutator ... once it exhausts any
     * unused recyclable blocks, it uses only completely free blocks", with the headroom as the
     * reserve that guarantees progress when the heap is otherwise full. Ours takes the reserve
     * first and then any completely free blocks still on the list. Restricting targets to the
     * reserve alone would cap evacuation at the headroom (~2.5% of the heap) even when free
     * blocks are sitting right there; measured, the free list is usually empty at this point —
     * which is exactly why the reserve has to exist — but when it is not, this uses it.
     * Recyclable blocks are deliberately NOT taken: they are where candidates come from, and
     * the collector must not allocate into a block it may be evacuating out of. */
    bbq_vec_clear(s->evacuation_targets);
    for (size_t i = 0; i < max_target_blocks && bbq_vec_len(s->evac_reserve); i++)
        bbq_vec_push(s->evacuation_targets, bbq_vec_pop(s->evac_reserve));
    s->evac_from_reserve = (size_t)bbq_vec_len(s->evacuation_targets);
    while (bbq_vec_len(s->free_blocks))
        bbq_vec_push(s->evacuation_targets, bbq_vec_pop(s->free_blocks));

    size_t ntargets = (size_t)bbq_vec_len(s->evacuation_targets);
    size_t ncand = 0;
    if (ntargets) {
        select_candidates(s, ntargets);
        for (int i = 0; i < bbq_vec_len(s->defrag_scratch); i++)
            if (s->defrag_scratch[i]->defrag_candidate) ncand++;
    }
    s->evacuation_active = (ntargets > 0 && ncand > 0);

    s->evac_target_idx = 0;
    if (s->evacuation_active) {
        imx_block_t* b = s->evacuation_targets[0];
        s->evac_cursor = imx_block_data_start(b); s->evac_limit = imx_block_data_end(b);
    } else { s->evac_cursor = NULL; s->evac_limit = NULL; }
}

int imx_space_is_evacuation_candidate(const imx_space_t* s, imx_block_t* block) {
    /* O(1). A target block was FREE at the last sweep (defrag_marked == 0), so selection
     * skipped it — the collector can never evacuate out of what it evacuates into. */
    return s->evacuation_active && block->defrag_candidate;
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
    /* Each target goes back where it came from. The headroom blocks return to the RESERVE —
     * §3.2's headroom is "never returned to the global allocator" — and blocks borrowed from
     * the free list return to the free list. Returning everything to the reserve would let it
     * grow without bound, which is the sink that would silently invalidate every "all blocks
     * reclaimed" check that counts the reserve. reclaim() rebuilds both immediately after, so
     * this matters for a caller that ends evacuation without reclaiming. */
    for (int i = 0; i < bbq_vec_len(s->evacuation_targets); i++) {
        if ((size_t)i < s->evac_from_reserve) bbq_vec_push(s->evac_reserve, s->evacuation_targets[i]);
        else                                  bbq_vec_push(s->free_blocks, s->evacuation_targets[i]);
    }
    s->evac_from_reserve = 0;
    bbq_vec_clear(s->evacuation_targets);
    s->evac_cursor = NULL; s->evac_limit = NULL; s->evac_target_idx = 0;
    s->evacuation_active = 0;
}

void imx_space_for_each_block(imx_space_t* s, void (*fn)(imx_block_t* b, void* ctx), void* ctx) {
    for (int i = 0; i < bbq_vec_len(s->all_blocks); i++) fn(s->all_blocks[i], ctx);
}
