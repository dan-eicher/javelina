/*
 * heap.h — the heap, BEHIND the runtime_api line. Handlers never include this:
 * they receive heap_t* (opaque, forward-declared in the substrate) and pass it
 * to the runtime natives. Only the backend (jav_runtime.c) includes this and
 * inspects the struct.
 *
 * Today the heap holds the WASM linear memory AND the GC. The collector plugs in
 * through `jav_collector_t gc` — the collector-agnostic seam the plan reserved:
 * the ported Immix engine, a future mark-compact, or a trivial bump-no-collect all
 * implement the same interface, swapped here without touching a generated handler.
 */
#ifndef JAV_HEAP_H
#define JAV_HEAP_H

#include "runtime_api.h"   /* u1, u4, and the opaque heap_t typedef */
#include "jav_gc.h"       /* gc_obj_t/gc_rtt_t object contract + the jav_collector_t interface */

/* One WASM linear memory instance (§4.2.8). The heap holds a growable vector of these,
 * indexed by memidx — multiple memories and memory64 are first-class. The bytes are
 * owned (malloc'd, growable via memory.grow). */
typedef struct {
    u1* data;     /* the memory's bytes (owned; realloc'd by memory.grow) */
    u8  size;     /* current size in bytes */
    u8  max;      /* maximum size in bytes (the memtype limit) — meaningful only when has_max */
    u1  has_max;  /* 1 ⇒ the memtype declared a max; 0 ⇒ unbounded (grow caps at the §3.2.15 addrtype ceiling) */
    u1  is64;     /* 1 ⇒ a 64-bit memory (memory64) */
} jav_mem_t;

/* §4.5.2 the session-wide closed-type registry (jav_module_index.h). Lives on the heap so every
 * instance sharing this heap canonicalizes into ONE id space — the precondition for cross-module
 * import matching. Forward-declared (opaque here); created lazily by jav_heap_typereg. */
typedef struct jav_typereg jav_typereg_t;

struct heap_t {
    jav_mem_t* mems;    /* bbq_vec of linear memories, indexed by memidx */
    jav_collector_t gc; /* the SWAPPABLE collector — every managed alloc/collect dispatches through here */
    jav_typereg_t* typereg;            /* the shared closed-type registry (lazily allocated by the loader) */
    void (*typereg_free)(jav_typereg_t*); /* set by the loader's jav_heap_typereg; the engine teardown calls it
                                           * blind, so jav_runtime.o keeps NO loader-symbol dependency (one-way arrow) */
    /* §4.5.2 cross-module closed-type match over the registry's global id space, set by the loader
     * alongside typereg_free (same one-way arrow — the interp's ref.cast/call_indirect on a
     * cross-instance funcref call through these, never linking a loader symbol). */
    int      (*typereg_gid_sub)(jav_typereg_t*, int32_t prov, int32_t req);        /* prov <: req over global ids */
    int32_t  (*typereg_intern_ft)(jav_typereg_t*, const struct jav_functype*);     /* a host func's functype → global id */
    u4 next_tag_id;                    /* §4.2 monotonic tagaddr allocator: each DEFINED tag gets a fresh session-unique
                                        * id; imported tags inherit the exporter's → throw/catch match by store identity */
    struct jav_exn_rtt* exn_rtts;      /* bbq_vec: per-tagaddr GC-object layout for exception instances, built lazily at
                                        * throw and cached here (one rtt per distinct tag). Owns each rtt; freed at GC destroy. */
};

/* Cached exception-object layout for one tag identity (tagaddr). The rtt describes the managed exn
 * object `{ u4 tag; u4 nfields; slot_t fields[nfields] }` for the generic tracer; built once per tag. */
typedef struct jav_exn_rtt { u4 tag; struct gc_rtt* rtt; } jav_exn_rtt_t;

/* The exn-object layout for tag identity `tagaddr` carrying the `nfields` field value-types `ftypes`
 * (T_GCREF marks a ref field) — looked up in the heap cache, built + cached on first use. Heap-owned. */
const struct gc_rtt* jav_exn_rtt_for(heap_t* heap, u4 tagaddr, u4 nfields, const u1* ftypes);

/* The heap's closed-type registry, created on first use (defined in the loader, jav_module_index.c).
 * Every caller already builds a heap, so this is the one id space the whole session shares — no
 * store required. */
jav_typereg_t* jav_heap_typereg(heap_t* heap);

/* Append a zero-initialized linear memory of `pages` × 64KiB; returns its memidx. `maxpages`
 * is the declared limit, meaningful only when `has_max` (else grow caps at the addrtype ceiling).
 * The heap owns the bytes — freed by jav_heap_free_mems. */
int  jav_mem_add(heap_t* heap, u4 pages, u4 maxpages, int has_max, int is64);
/* Free every memory's bytes and the vector (the GC is freed separately). */
void jav_heap_free_mems(heap_t* heap);

/* Grow a memory instance by `delta` pages, reallocating its owned buffer (zero-filled new pages);
 * returns the old page count, or -1 if it can't grow (over max / OOM). This is the §7.1 embedder
 * entry: the host holds a memaddr (a store meminst) and grows it directly — no frame, no memidx.
 * The §4.6.8 execution path (mem_grow) resolves a module memidx to a meminst, then calls this. */
s8   mem_grow_inst(jav_mem_t* m, s8 delta);

/* Bind the heap's collector (Immix today) with the runtime's root source = `vm`. */
void      jav_heap_gc_init(heap_t* heap, vm_t* vm);
/* Arm (or disarm) the collector's heap-invariant checker. On a violation the vm stops executing
 * guest code and the outcome reaches the host as a trap carrying the invariant's name — the engine
 * never ends the host process. Off by default: the check walks the whole reachable graph. */
void      jav_heap_gc_verify(heap_t* heap, vm_t* vm, int on);
void      jav_heap_gc_destroy(heap_t* heap);
/* Allocate a `size`-byte managed object of type `rtt` through the heap's collector. */
gc_obj_t* jav_gc_new(vm_t* vm, const gc_rtt_t* rtt, u4 size);

#endif /* JAV_HEAP_H */
