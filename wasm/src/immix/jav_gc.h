/*
 * jav_gc.h — the WASM GC shell over the ported Immix engine.
 *
 * The engine (immix_*) is type-agnostic; this layer adds the WASM object model:
 * every object carries a header pointing at an `rtt` (run-time type) — its size +
 * the byte offsets of its GC-reference fields, so a GENERIC tracer follows
 * references with NO per-type hand-written code (unlike AiPL's per-type C++ mark()).
 * The rtt is data, built from the module's type section at load time. Collection is
 * mark-from-roots + opportunistic evacuation (forwarding pointers, reference slots
 * rewritten in place); roots come from an embedder-supplied enumerator (the typed
 * value stack / locals / globals).
 */
#ifndef JAV_GC_H
#define JAV_GC_H

#include "immix_space.h"
#include <stdint.h>
#include <stdbool.h>

/* Run-time type descriptor: object layout for alloc + tracing. `size` is the total
 * object size (header + payload); `ref_offsets` are byte offsets from the object
 * base of fields that hold GC references. */
enum { GC_KIND_STRUCT = 0, GC_KIND_ARRAY = 1 };

typedef struct gc_rtt {
    uint32_t size;          /* struct: total object size (header + fields) */
    uint32_t nrefs;         /* struct: number of reference fields */
    uint16_t nfields;       /* struct: field count. 0 = UNSET (a hand-built narrow-only rtt —
                             * tests, the host box): consumers fall back to (size-header)/8,
                             * which is exact when no field is wide. build_rtts always sets it;
                             * a v128 field only ever arrives via build_rtts. */
    uint8_t  kind;          /* GC_KIND_STRUCT / GC_KIND_ARRAY */
    uint8_t  elem_is_ref;   /* array: elements are managed references */
    uint8_t  elem_store_w;  /* array: element STORAGE width 1/2/4/8/16 — the array.new_data/
                             * init_data data-segment stride (16 = v128, per §3.4.7's vectype). */
    uint8_t  elem_heap_w;   /* array: the IN-HEAP element stride — 16 for v128 elements, else
                             * GC_ARRAY_ELEM_BYTES. 0 = UNSET (hand-built rtt): consumers read
                             * GC_ARRAY_ELEM_BYTES, the pre-v128 behavior. */
    const uint32_t* field_off; /* struct: nfields+1 byte offsets of the fields from the PAYLOAD
                             * base (off[nfields] = payload size), or NULL ⇒ uniform 8-byte
                             * cells (no v128 field — the overwhelmingly common case) */
    int32_t  gid;           /* §4.5.2 store-global canonical type id of this rtt's type; -1 until the
                             * defining module is absorbed into the session registry. Lets a cross-
                             * instance struct/array ref resolve its runtime type for ref.test/ref.cast. */
    uint32_t ref_offsets[]; /* struct: byte offsets of reference fields */
} gc_rtt_t;

/* An array object's payload begins with its length (u4, padded to 8) then the
 * elements; this offset is where elements start. */
#define GC_ARRAY_ELEMS_OFFSET 8

/* The DEFAULT in-heap array element slot (the .l value view of a scalar/ref). Not an
 * invariant: a v128 element is 16 bytes, and the per-RTT stride is elem_heap_w — every
 * size/index computation reads the RTT, never this constant directly. */
#define GC_ARRAY_ELEM_BYTES 8

/* Object header at the base of every GC object; the payload follows. */
typedef struct gc_obj {
    const gc_rtt_t* rtt;
    struct gc_obj*  forward;   /* evacuation forwarding pointer, or NULL */
    uint32_t        epoch;     /* == heap epoch → marked this cycle (no per-cycle reset) */
    uint32_t        _pad;
} gc_obj_t;

struct gc_heap;
struct gc_verify;   /* the checker's verdict, defined below with the checker itself */
typedef void (*gc_root_visit_fn)(gc_obj_t** slot, void* ctx);
/* The embedder enumerates its roots, calling `visit(&slot, ctx)` for each slot that
 * holds a GC reference (the collector may rewrite *slot to a forwarded address). */
typedef void (*gc_enum_roots_fn)(struct gc_heap* h, gc_root_visit_fn visit, void* visit_ctx);

/* Initial allocation budget before the first collection; the threshold then
 * tracks the live set (grow-by-1.5×) so collections stay early and proportional
 * to survivors — never "fill the whole heap, then one giant stop-the-world". */
#define GC_INITIAL_THRESHOLD (4u * IMX_BLOCK_SIZE)

typedef struct gc_heap {
    imx_space_t      space;
    gc_enum_roots_fn enum_roots;
    void*            user;       /* opaque embedder handle (e.g. the vm) for enum_roots */
    uint32_t         epoch;
    gc_obj_t**       worklist;   /* bbq_vec: the trace mark-stack */
    gc_obj_t**       large_objs; /* bbq_vec: the large-object space — objects > IMX_MEDIUM_MAX, each its own
                                  * malloc'd allocation, marked in place (never evacuated) and swept whole. */
    size_t           large_bytes;/* live bytes held in the LOS (part of bytes_allocated / live_bytes) */
    size_t           evac_headroom_div;   /* reserve total_blocks/this for evacuation targets */
    size_t           bytes_allocated;     /* live + floating bytes since the last collection */
    size_t           gc_threshold;        /* collect when bytes_allocated would cross this */
    size_t           live_bytes;          /* survivors of the in-progress collection (tracer sums it) */
    /* Non-NULL ⇒ run gc_verify at the end of every collection and report a violation here. It is
     * both the switch and the policy in one field: a checker armed with nowhere to report has no
     * answer but to kill the process, which is the thing a library must never do. NULL (default)
     * means the check does not run — it walks the whole reachable graph. */
    void           (*on_corruption)(void* ctx, const struct gc_verify* what);
    void*            on_corruption_ctx;
} gc_heap_t;

/* ── The heap-invariant checker ──────────────────────────────────────────────
 *
 * "It didn't crash" proves nothing about a collector: corruption surfaces two or three
 * collections later as a mangled graph, by which time the cause is gone. This walks the
 * REACHABLE graph after a collection and checks the invariants that must hold, naming the
 * first one that does not.
 *
 * It walks from the ROOTS, not over the blocks: imx_block_for_each_object reads the
 * object-start bitmap, which is never cleared on reclaim or block reuse, so it yields the
 * accumulated union of every object ever started at each word — a block walk reads dead
 * headers. Reachability is the only honest enumeration.
 *
 * The second half of the point (Wasmtime's framing) is that this catches heap corruption
 * "or misoptimizations in our compiler": a wrongly-dropped ArrayStore check or a bad
 * memory-DSE writes a heap no compiler unit test would ever look at.
 *
 * Returns true when the heap is sound. On failure `out` names the invariant and the object
 * it failed on — never just a boolean, and never a crash. */
typedef struct gc_verify {
    const char* invariant;   /* NULL iff the heap verified; a static string, safe to retain */
    const gc_obj_t* object;  /* the object the invariant failed on, if any */
    const void* detail;      /* the referring slot / block, when it aids triage */
} gc_verify_t;

bool      gc_verify(gc_heap_t* h, gc_verify_t* out);

void      gc_heap_init(gc_heap_t* h, gc_enum_roots_fn enum_roots, void* user);
void      gc_heap_destroy(gc_heap_t* h);
/* allocate `size` bytes for an object of type `rtt` (size is explicit because arrays
 * vary by length). Zeroes the payload; collects + retries on OOM; NULL if still full. */
gc_obj_t* gc_alloc(gc_heap_t* h, const gc_rtt_t* rtt, uint32_t size);
void      gc_collect(gc_heap_t* h);
/* The actual byte size of a live object (struct: rtt->size; array: from its length). */
uint32_t  gc_obj_size(const gc_obj_t* o);

/* ── The swappable collector interface ───────────────────────────────────────
 * heap_t holds one of these; every GC opcode/native dispatches THROUGH it, never
 * naming a collector. The object contract (gc_obj_t header + gc_rtt_t field map)
 * is the shared part; the engine behind `self` is replaceable — Immix (moving),
 * a future mark-compact, or a trivial bump-no-collect — all implement this. */
typedef struct {
    void*     self;
    gc_obj_t* (*alloc)(void* self, const gc_rtt_t* rtt, uint32_t size);
    void      (*collect)(void* self);
    void      (*destroy)(void* self);
    /* Ask the collector to check its own invariants after every collection and report a violation
     * to `cb`. On the vtable rather than reached through `self` so the store never casts to a
     * collector it does not name; NULL for one that has no checker. The collector DETECTS; what to
     * do about it belongs to the embedder, per §1.1.3 ("an embedder's responsibility"). */
    void      (*set_corruption_handler)(void* self, void (*cb)(void*, const struct gc_verify*), void* ctx);
} jav_collector_t;

/* Build a collector backed by the ported Immix engine (heap-allocates the gc_heap;
 * destroy frees it). `enum_roots`/`user` wire the embedder's root source. */
jav_collector_t jav_immix_collector_create(gc_enum_roots_fn enum_roots, void* user);

/* the payload (after the header) of an object */
static inline void* gc_obj_payload(gc_obj_t* o) { return (uint8_t*)o + sizeof(gc_obj_t); }
/* Is struct field `fld` (an 8-byte slot after the header) a managed reference? The RTT lists the byte
 * offsets of reference fields; a field is a ref iff its offset is one of them. Inlined into the struct.get
 * handler/stencil (no extern) — clang folds the scan; structs have few ref fields. */
static inline int gc_field_is_ref(const gc_rtt_t* rtt, uint32_t fld) {
    uint32_t off = (uint32_t)sizeof(gc_obj_t) + fld * 8;
    for (uint32_t i = 0; i < rtt->nrefs; i++) if (rtt->ref_offsets[i] == off) return 1;
    return 0;
}
/* sign-/zero-extend a packed value of storage width w (1=i8 / 2=i16 bytes) to i32 (§4.6 struct/array.get_s/u).
 * Inlined into the get_s/u handler+stencil (no extern). */
static inline int32_t pack_extend(int32_t v, uint8_t w, int sgn) {
    if (w == 2) return sgn ? (int32_t)(int16_t)v : (int32_t)(uint16_t)v;
    return sgn ? (int32_t)(int8_t)v : (int32_t)(uint8_t)v;   /* default i8 */
}
/* was `o` marked live by the most recently completed collection? */
static inline int gc_obj_live(const gc_heap_t* h, const gc_obj_t* o) { return o->epoch == h->epoch - 1; }

#endif /* JAV_GC_H */
