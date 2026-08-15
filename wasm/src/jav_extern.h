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
//
// jav_config_set_verify_heap asks the collector to check its own invariants at the end of every
// collection. Corruption surfaces two or three collections after its cause, so the end of the
// cycle that produced it is the only place it can still be attributed. Off by default (it walks
// the whole reachable graph); a config option rather than a build flag because the programs worth
// checking are the ones the shipped binaries run.
//
// A violation is an ENGINE defect, and it is reported the way stack exhaustion is: the vm stops
// executing guest code and the host gets a wasm_trap_t whose message names the invariant, with
// jav_capi_last_status reading JAV_TRAP. The store is then finished — every later call on it
// returns the same trap rather than running on a heap known to be unsound. The engine does NOT
// end the process: it is a library, and one that aborts turns any bug an attacker can provoke
// into a way to kill the application. §1.1.3 places that policy with the embedder.
typedef struct wasm_config_t wasm_config_t;
void     jav_config_set_jit(wasm_config_t* c, int jit);
void     jav_config_set_verify_heap(wasm_config_t* c, int on);
uint32_t jav_capi_jit_count(const wasm_store_t* s);
// Functions the JIT DECLINED, which stayed on the interpreter. The complement of
// jit_count and the more interesting half: falling back is correct, so it is
// silent, and a tier that compiled nothing answers every question exactly like
// one that worked. Read both to tell those apart.
uint32_t jav_capi_jit_declined(const wasm_store_t* s);
// How many operand-stack slots this build's JIT keeps in registers (Ertl's cache
// size). Fixed when the stencil table was generated, so a binary has exactly ONE
// of the two JIT tiers and cannot be asked for the other — which is why a test
// that names a tier has to be able to check rather than assume.
int      jav_jit_cache_slots(void);
// What the tier-2 stitcher did while compiling. Every out-param is optional.
//   cached_ops  instructions that ran with an operand in a register
//   deep        how many of those used a slot above the first
//   occupancy   each instruction's entry state, SUMMED — register-slots held per
//               instruction, not slots read; a value idle in a register across
//               five instructions adds five
//   transitions spills and fills stamped between them, and their split
//   mem_slots   operand-stack slots the compiled code still moves to or from
//               memory — the quantity stack caching removes, and so the one to
//               compare cache sizes on
// The RATIO is the direct evidence for where Ertl's curve turns over (§2.6), and
// it discriminates where a wall-clock benchmark cannot — so an embedder measuring
// a cache size reads these rather than timing two builds and hoping nothing else
// moved. mem_slots was absent here, which is how a caller came to derive an
// accesses-avoided figure from occupancy instead of reporting a measured one.
void     jav_jit_cache_stats(uint64_t* cached_ops, uint64_t* deep, uint64_t* occupancy,
                             uint64_t* transitions, uint64_t* spills, uint64_t* fills,
                             uint64_t* mem_slots);
// Zero them. The counters are process-global and every module compiled adds to
// them, so a runtime that loads a large shared runtime before the program under
// measurement must reset between the two or the program's own figures are lost
// in the runtime's — 24 functions against 1289 resolve to nothing.
void     jav_jit_cache_stats_reset(void);

// Tier 3's own observable, on the same terms as the cache stats above:
// how many roots the eq-sat pass extracted differently and rebuilt, process-
// global across every module compiled at tier 3 (0 at every other tier).
// An embedder measuring whether the rewrite earns its place on a corpus reads
// this beside the wall clock — a 1.00x with rewritten=0 says "nothing to
// fold", the same ratio with a large count says "folding buys nothing here",
// and those are different verdicts about different things.
uint64_t jav_capi_eqsat_rewritten(void);
// …and per-rule fire counts (parallel arrays, process-global), returning the
// rule count. A corpus meant to exercise the rewrite gates on these: a rule
// no input ever fires is vocabulary, not capability, and only a per-rule
// number can say which.
int      jav_capi_eqsat_rules(const char* const** names,
                              const unsigned long long** fires);

#endif // JAV_EXTERN_H
