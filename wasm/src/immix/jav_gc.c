/* jav_gc.c — the WASM GC shell: rtt object model + mark/trace/evacuate collector
 * over the Immix engine. */
#include "jav_gc.h"
#include <string.h>
#include <stdlib.h>

/* ASAN manual poisoning (ASAN builds only): poison an evacuated object's OLD location so a stale
 * raw gc_obj_t* held across a safepoint — the classic moving-GC bug — faults on field access. The
 * header (rtt/epoch/forward) stays readable for the collector's forwarding; only the payload is
 * poisoned. Mirrors the free-block poisoning in immix_space.c. */
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
#  include <sanitizer/asan_interface.h>
#  define IMX_POISON(addr, size) __asan_poison_memory_region((addr), (size))
#else
#  define IMX_POISON(addr, size) ((void)(addr), (void)(size))
#endif

void gc_heap_init(gc_heap_t* h, gc_enum_roots_fn enum_roots, void* user) {
    memset(h, 0, sizeof *h);
    imx_space_init(&h->space);
    h->enum_roots = enum_roots;
    h->user = user;
    h->epoch = 1;               /* fresh objects get epoch 0, so they're "unmarked" vs epoch 1 */
    h->evac_headroom_div = 40;  /* reserve ~2.5% of blocks as evacuation targets */
    h->gc_threshold = GC_INITIAL_THRESHOLD;
}

void gc_heap_destroy(gc_heap_t* h) {
    bbq_vec_free(h->worklist);
    for (int i = 0; i < (int)bbq_vec_len(h->large_objs); i++) free(h->large_objs[i]);   /* release LOS storage */
    bbq_vec_free(h->large_objs);
    imx_space_destroy(&h->space);
    memset(h, 0, sizeof *h);
}

gc_obj_t* gc_alloc(gc_heap_t* h, const gc_rtt_t* rtt, uint32_t size) {
    /* Threshold safepoint: collect early, before the heap fills. Safe here because
     * allocating natives root their operands FIRST and hold no raw gc_obj_t* across
     * this call — so collecting now sees a consistent root set. */
#ifdef GC_STRESS
    gc_collect(h);   /* stress: collect + evacuate at EVERY safepoint to surface rooting bugs (raw
                      * gc_obj_t* held across an allocation) — pairs with the ASAN heap poisoning */
#else
    if (h->bytes_allocated + size >= h->gc_threshold) gc_collect(h);
#endif

    gc_obj_t* o;
    if (size > IMX_MEDIUM_MAX) {              /* large object: too big for a block → its own malloc'd LOS slot */
        o = malloc(size);
        if (!o) { gc_collect(h); o = malloc(size); }   /* a collect frees dead large objects back to the system */
        if (!o) return NULL;
        bbq_vec_push(h->large_objs, o);
        h->large_bytes += size;
    } else {
        o = imx_space_allocate(&h->space, size, IMX_OBJECT_ALIGN);
        if (!o) { gc_collect(h); o = imx_space_allocate(&h->space, size, IMX_OBJECT_ALIGN); if (!o) return NULL; }
    }
    memset(o, 0, size);         /* zero the payload (ref fields / elements start null) */
    o->rtt = rtt;               /* forward=NULL, epoch=0 from the memset */
    h->bytes_allocated += size;
    return o;
}

/* the live byte size of an object: a struct's is fixed; an array's is computed from
 * its length (stored at the start of the payload). */
uint32_t gc_obj_size(const gc_obj_t* o) {
    const gc_rtt_t* r = o->rtt;
    if (r->kind == GC_KIND_ARRAY) {
        uint32_t len = *(const uint32_t*)((const uint8_t*)o + sizeof(gc_obj_t));
        uint32_t w = r->elem_heap_w ? r->elem_heap_w : GC_ARRAY_ELEM_BYTES;   /* 0 = unset (hand-built rtt) */
        return (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + len * w;
    }
    return r->size;
}

/* Mark `o` live this cycle; if its block is an evacuation candidate, MOVE it to a
 * target block (leaving a forwarding pointer) — the spec's "rewrite the slot in
 * place" pattern. Returns the (possibly relocated) object so the caller updates its
 * slot. Re-tracing is avoided by the mark epoch. */
static gc_obj_t* gc_mark1(gc_heap_t* h, gc_obj_t* o) {
    /* null refs: the NULL pointer or the embedder's all-ones null sentinel (a managed
     * ref field set to WASM null) — neither is a heap object, so never dereference. */
    if (!o) return o;
    /* NOT A POINTER: every heap object is IMX_OBJECT_ALIGN-aligned, so a slot with low bits set
     * is a self-identifying non-address value — a §2.3.4 pointer-tagged `ref.i31` is the one that
     * occurs today. This is what lets anyref/eqref containers stay traced: the scalars among their
     * elements are refused HERE, per value, instead of a per-type bit having to decide for a union
     * it cannot decide for. Testing full alignment rather than just the reserved tag bit means any
     * future non-address form is also refused instead of dereferenced. */
    if ((uintptr_t)o & (IMX_OBJECT_ALIGN - 1u)) return o;
    if (o->forward) o = o->forward;          /* already evacuated this cycle */
    if (o->epoch == h->epoch) return o;      /* already marked this cycle */

    uint32_t sz = gc_obj_size(o);
    h->live_bytes += sz;                     /* this object survives → count it once (epoch guard above) */
    if (sz > IMX_MEDIUM_MAX) {                /* large object: its own malloc'd slot, not in a block — mark in
                                              * place (never evacuate, no block/line marks, no imx_block_of). */
        o->epoch = h->epoch;
        bbq_vec_push(h->worklist, o);         /* trace its references next */
        return o;
    }
    if (imx_space_evacuation_active(&h->space) &&
        imx_space_is_evacuation_candidate(&h->space, imx_block_of(o))) {
        void* dst = imx_space_allocate_evacuation(&h->space, sz, IMX_OBJECT_ALIGN);
        if (dst) {
            memcpy(dst, o, sz);
            imx_block_clear_object_start(imx_block_of(o), o);
            o->forward = (gc_obj_t*)dst;
            IMX_POISON((uint8_t*)o + sizeof(gc_obj_t), sz - sizeof(gc_obj_t));  /* stale-ref field access faults */
            o = (gc_obj_t*)dst;
        }
    }
    o->epoch = h->epoch;
    imx_block_mark_object(imx_block_of(o), o, sz);   /* set the engine's line marks */
    bbq_vec_push(h->worklist, o);            /* trace its references next */
    return o;
}

static void gc_visit_root(gc_obj_t** slot, void* ctx) {
    gc_heap_t* h = ctx;
    *slot = gc_mark1(h, *slot);
}

/* Sweep the large-object space: free every large object not marked this cycle (its epoch lags the
 * heap epoch), compacting the survivor list. Runs each collection, before the epoch advances. */
static void los_sweep(gc_heap_t* h) {
    int n = (int)bbq_vec_len(h->large_objs), w = 0;
    for (int i = 0; i < n; i++) {
        gc_obj_t* o = h->large_objs[i];
        if (o->epoch == h->epoch) h->large_objs[w++] = o;       /* survived → keep + compact to the front */
        else { h->large_bytes -= gc_obj_size(o); free(o); }      /* unreachable → reclaim to the system */
    }
    while ((int)bbq_vec_len(h->large_objs) > w) bbq_vec_pop(h->large_objs);
}

/* §3.2: "By default immix reserves 2.5% of the heap as compaction headroom" — total/40.
 * The blocks are set aside by reclaim at the END of a collection and drawn on at the start
 * of the next, because the mutator empties the free list in between. */
static size_t evac_headroom(const gc_heap_t* h) {
    size_t n = imx_space_total_blocks((imx_space_t*)&h->space) / h->evac_headroom_div;
    return n < 1 ? 1 : n;
}

/* ── The heap-invariant checker (see jav_gc.h) ───────────────────────────────── */

#define GC_VFAIL(inv, obj, det) do { \
        v->fail.invariant = (inv); v->fail.object = (obj); v->fail.detail = (det); \
        return; \
    } while (0)

typedef struct {
    gc_heap_t*  h;
    gc_verify_t fail;
    gc_obj_t**  work;      /* bbq_vec: transitive walk, mirroring gc_collect's */
    gc_obj_t**  seen;      /* bbq_vec: visited, so a cycle terminates */
} gc_verify_ctx_t;

static bool gcv_seen(gc_verify_ctx_t* v, const gc_obj_t* o) {
    for (int i = 0; i < (int)bbq_vec_len(v->seen); i++) if (v->seen[i] == o) return true;
    return false;
}

/* Is `o` a live LOS entry? Large objects are malloc'd, so they are in no block and the
 * block-residency checks below must not run on them. */
static bool gcv_in_los(gc_heap_t* h, const gc_obj_t* o) {
    for (int i = 0; i < (int)bbq_vec_len(h->large_objs); i++) if (h->large_objs[i] == o) return true;
    return false;
}

static bool gcv_block_known(imx_space_t* s, const imx_block_t* b) {
    for (int i = 0; i < (int)bbq_vec_len(s->all_blocks); i++) if (s->all_blocks[i] == b) return true;
    return false;
}

/* Check one reference and enqueue its target. `slot` is where the reference lives, so a
 * failure can say WHICH field pointed at the bad object rather than only naming the object. */
static void gcv_visit(gc_verify_ctx_t* v, gc_obj_t* o, const void* slot) {
    if (v->fail.invariant) return;
    if (!o) return;
    if ((uintptr_t)o & (IMX_OBJECT_ALIGN - 1u)) return;   /* a tagged non-pointer (ref.i31) */
    if (gcv_seen(v, o)) return;
    bbq_vec_push(v->seen, o);

    gc_heap_t* h = v->h;

    if (!o->rtt) GC_VFAIL("header: reachable object has a NULL rtt", o, slot);

    /* THE forwarding invariant, and the one a missed slot update trips. When a collection
     * has finished, every reference to an evacuated object must have been rewritten to its
     * new address — so nothing REACHABLE may still be a forwarding source. */
    if (o->forward)
        GC_VFAIL("forwarding: a reachable reference still points at an EVACUATED source "
                 "(its slot was never updated)", o, slot);

    /* gc_obj_live is the authority for "marked this cycle" — the epoch is bumped at the end
     * of gc_collect, so a survivor carries epoch == h->epoch - 1, not h->epoch. */
    if (!gc_obj_live(h, o))
        GC_VFAIL("mark: a reachable object is not marked live for this epoch", o, slot);

    uint32_t sz = gc_obj_size(o);
    if (sz == 0) GC_VFAIL("size: reachable object computes a zero size", o, slot);

    if (sz > IMX_MEDIUM_MAX) {
        if (!gcv_in_los(h, o))
            GC_VFAIL("LOS: an object larger than a block's data area is not in large_objs", o, slot);
    } else {
        imx_block_t* b = imx_block_of(o);
        if (!gcv_block_known(&h->space, b))
            GC_VFAIL("residency: reachable object is not inside any block this space owns", o, b);
        uint8_t* ds = imx_block_data_start(b);
        uint8_t* de = imx_block_data_end(b);
        if ((uint8_t*)o < ds || (uint8_t*)o + sz > de)
            GC_VFAIL("residency: object straddles or sits outside its block's data region", o, b);
        /* The lines the object occupies must be marked, or the allocator may hand its storage
         * out again while it is still reachable. The rule is imx_block_mark_object's, not a
         * stricter one of our own: CONSERVATIVE line marking marks the line of the object's
         * start, and only spans further when the object is larger than a line — a small object
         * straddling two lines leaves the second unmarked on purpose, and imx_lb_next_hole
         * compensates by skipping the first free line after a marked run. */
        size_t first = imx_line_of(o);
        size_t last  = sz > IMX_LINE_SIZE
                     ? imx_line_of((const void*)((uintptr_t)o + sz - 1)) : first;
        for (size_t ln = first; ln <= last; ln++)
            if (!imx_lb_test(&b->line_marks, ln))
                GC_VFAIL("line map: a live object occupies a line the map records as free", o, b);
    }
    bbq_vec_push(v->work, o);
}

static void gcv_root(gc_obj_t** slot, void* ctx) {
    gc_verify_ctx_t* v = (gc_verify_ctx_t*)ctx;
    gcv_visit(v, *slot, slot);
}

bool gc_verify(gc_heap_t* h, gc_verify_t* out) {
    gc_verify_ctx_t v = { h, { NULL, NULL, NULL }, NULL, NULL };

    h->enum_roots(h, gcv_root, &v);
    while (!v.fail.invariant && bbq_vec_len(v.work)) {
        gc_obj_t* o = bbq_vec_pop(v.work);
        const gc_rtt_t* rtt = o->rtt;
        if (rtt->kind == GC_KIND_ARRAY) {
            if (rtt->elem_is_ref) {
                uint32_t len = *(uint32_t*)((uint8_t*)o + sizeof(gc_obj_t));
                gc_obj_t** e = (gc_obj_t**)((uint8_t*)o + sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET);
                for (uint32_t i = 0; i < len && !v.fail.invariant; i++) gcv_visit(&v, e[i], &e[i]);
            }
        } else {
            for (uint32_t i = 0; i < rtt->nrefs && !v.fail.invariant; i++) {
                gc_obj_t** f = (gc_obj_t**)((uint8_t*)o + rtt->ref_offsets[i]);
                gcv_visit(&v, *f, f);
            }
        }
    }

    /* The LOS half: a swept list must contain only this epoch's survivors. An entry the
     * sweep should have freed is a leak; one freed while reachable would have failed above. */
    for (int i = 0; !v.fail.invariant && i < (int)bbq_vec_len(h->large_objs); i++) {
        gc_obj_t* o = h->large_objs[i];
        if (!o) { v.fail.invariant = "LOS: a NULL entry survived the sweep"; v.fail.object = NULL; break; }
        if (!gc_obj_live(h, o)) {
            v.fail.invariant = "LOS: an entry that is not marked for this epoch survived the sweep";
            v.fail.object = o;
        }
    }

    bbq_vec_free(v.work); bbq_vec_free(v.seen);
    if (out) *out = v.fail;
    return v.fail.invariant == NULL;
}

void gc_collect(gc_heap_t* h) {
    imx_space_clear_marks(&h->space);
    imx_space_begin_evacuation(&h->space, evac_headroom(h));

    h->live_bytes = 0;                       /* the tracer sums survivors as it marks */
    bbq_vec_clear(h->worklist);
    h->enum_roots(h, gc_visit_root, h);      /* mark + evacuate the roots */

    while (bbq_vec_len(h->worklist)) {        /* transitive closure */
        gc_obj_t* o = bbq_vec_pop(h->worklist);
        const gc_rtt_t* rtt = o->rtt;
        if (rtt->kind == GC_KIND_ARRAY) {     /* array: trace each element if elements are refs */
            if (rtt->elem_is_ref) {
                uint32_t len = *(uint32_t*)((uint8_t*)o + sizeof(gc_obj_t));
                gc_obj_t** e = (gc_obj_t**)((uint8_t*)o + sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET);
                for (uint32_t i = 0; i < len; i++) e[i] = gc_mark1(h, e[i]);
            }
        } else {                              /* struct: trace each reference field */
            for (uint32_t i = 0; i < rtt->nrefs; i++) {
                gc_obj_t** field = (gc_obj_t**)((uint8_t*)o + rtt->ref_offsets[i]);
                *field = gc_mark1(h, *field);
            }
        }
    }

    imx_space_end_evacuation(&h->space);
    imx_space_reclaim(&h->space, evac_headroom(h));   /* reclassify; set aside the next cycle's headroom */
    los_sweep(h);                            /* free unmarked large objects (independent of the block engine) */
    h->epoch++;

    /* Re-base the budget on the survivors and grow the threshold to track the live
     * set (1.5×), so the next cycle fires proportionally — not at the heap ceiling. */
    h->bytes_allocated = h->live_bytes;
    if (h->bytes_allocated > h->gc_threshold / 2) {
        size_t base = h->bytes_allocated > h->gc_threshold ? h->bytes_allocated : h->gc_threshold;
        h->gc_threshold = base + base / 2;
    }

    /* The oracle, opt-in. Corruption surfaces two or three collections after its cause, so the
     * only place it can be attributed is HERE, at the end of the cycle that produced it. What
     * happens next is the embedder's call, not ours: this is a library, and a library that ends
     * its host's process hands anyone who can provoke the bug a way to kill the application. */
    if (h->on_corruption) {
        gc_verify_t r;
        if (!gc_verify(h, &r)) h->on_corruption(h->on_corruption_ctx, &r);
    }
}

/* ── Immix as a swappable collector ── */
static gc_obj_t* immix_v_alloc(void* self, const gc_rtt_t* rtt, uint32_t size) { return gc_alloc((gc_heap_t*)self, rtt, size); }
static void      immix_v_collect(void* self)                    { gc_collect((gc_heap_t*)self); }
static void      immix_v_destroy(void* self)                    { gc_heap_destroy((gc_heap_t*)self); free(self); }
static void      immix_v_set_handler(void* self, void (*cb)(void*, const gc_verify_t*), void* ctx) {
    gc_heap_t* h = (gc_heap_t*)self;
    h->on_corruption = cb; h->on_corruption_ctx = ctx;
}

jav_collector_t jav_immix_collector_create(gc_enum_roots_fn enum_roots, void* user) {
    gc_heap_t* h = malloc(sizeof *h);
    gc_heap_init(h, enum_roots, user);
    return (jav_collector_t){ h, immix_v_alloc, immix_v_collect, immix_v_destroy, immix_v_set_handler };
}
