// jav_extern.h — the loader↔embedder seam: two shared pieces between the instance loader and the
// wasm-c-api that don't belong in wasm.h. (The §4.2.3 store is wasm_store_t, wasm_capi.c.)
//   1. jav_project_export — the §4.5.2 externval projection: read export #index off a LIVE instance
//      entity into a jav_extern_t. The c-api's positional import marshaling calls it (one copy).
//   2. jav_capi_last_status/error — the §5/§4.5 verdict the spec surfaces only as NULL + a trap; a
//      conformance harness reads it to classify malformed/invalid/unlinkable/uninstantiable/trap/ok.
#ifndef JAV_EXTERN_H
#define JAV_EXTERN_H

#include "jav_instance.h"   // jav_instance_t / jav_extern_t / jav_modidx_t / vm_t
#include "heap.h"           // heap_t (the mem projection reads heap->mems)

// §4.5.2 the ONE externval projection — read export #index off the LIVE instance entity into an
// externval (its type is read off the live entity here, at import-match time, so a grown table/memory
// matches its CURRENT size). Shared by the c-api's positional import marshaling — one copy, no clone.
void jav_project_export(heap_t* heap, const jav_instance_t* inst, const jav_modidx_t* gm,
                        uint8_t kind, uint32_t index, jav_extern_t* out);

// The ONE sanctioned non-wasm.h readout of the engine's verdict (see file banner): a conformance
// harness drives the public wasm.h and reads back the §5/§4.5 outcome of the most recent operation
// on the store. wasm.h itself stays free of this — the spec surfaces failure only as NULL + a trap.
// CONTRACT: a last-write-wins per-store slot — read it IMMEDIATELY after the operation whose verdict you
// want, before any other call on the store. Defined only under the one-vm-per-store, single-threaded-
// store model (concurrent/interleaved store use is already unsound there, independent of this readout).
typedef struct wasm_store_t wasm_store_t;
jav_status_t jav_capi_last_status(const wasm_store_t* s);
jav_err_t    jav_capi_last_error (const wasm_store_t* s);

// §3.3.3 (Titzer) DEBUG EXTENSION — install a per-opcode probe on the store's interp tier. `cb` is
// called BEFORE each interpreted opcode with the embedder's `ctx` and the opcode byte; cb=NULL clears
// it. The core spec doesn't govern the embedder API, and wasm.h explicitly allows extensions (cf.
// wasm_config_t) — so this is a sanctioned sidecar debug hook, NOT in the vendored wasm.h. It observes
// INTERP-tier execution (the debug tier — the §1.1 reason an interpreter tier exists); a JIT'd function
// does not fire it. Opt-in: a store with no probe runs untouched.
void jav_capi_set_probe(wasm_store_t* s, void (*cb)(void* ctx, uint8_t op), void* ctx);

// TIER SELECTION — the embedder option, carried on wasm_config_t. wasm.h defines wasm_config_t as an
// opaque, field-less "embedder extension point" precisely so an engine can hang its own options there;
// this is javelina's one option. `jit` non-zero ⇒ every DEFINED function of every instance created on
// an engine built from this config is compiled to the copy-and-patch tier at instantiation, reached
// through the same jav_func_t::invoke seam as the interpreter (so the choice is semantics-free: a
// function's observable behaviour, traps included, is identical on either tier). Set it BEFORE
// wasm_engine_new_with_config, which reads and consumes the config.
//
// jav_capi_jit_count returns how many funcinsts the store actually placed on the JIT tier — 0 on a
// default engine. It is the observable that distinguishes "asked for the JIT" from "got the JIT"
// (a body carrying a `flag:no_jit` opcode falls back to the interpreter rather than failing).
typedef struct wasm_config_t wasm_config_t;
void     jav_config_set_jit(wasm_config_t* c, int jit);
uint32_t jav_capi_jit_count(const wasm_store_t* s);

#endif // JAV_EXTERN_H
