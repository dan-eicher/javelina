/*
 * interp.h — the in-place WASM interpreter (tier 0): a THREADED continuation
 * machine over the opgen-generated handlers, structurally identical to the JIT
 * (each handler musttail-jumps to its successor). interp_run crosses into the
 * chain and returns when a handler terminates; the outcome (trap vs return, and
 * the result) lives in the vm.
 */
#ifndef JAV_INTERP_H
#define JAV_INTERP_H

#include "runtime_api.h"   /* vm_t, heap_t, jav_status_t */

/* Run from frame.code.pos until the function returns or traps. On JAV_RETURN
 * the results are the top of the frame stack (jav_tos). The code cursor must be
 * positioned at the first instruction (past the locals declaration vector). */
jav_status_t interp_run(vm_t* vm, heap_t* h);
/* Invoke a module function by funcidx (loader start fn + the jav_invoke entry); the `call` opcode itself
 * composes jav_funcaddr + jav_invoke_ref in the .def. Was a generated runtime_api.h decl; now hand-written. */
jav_status_t jav_call(vm_t* vm, heap_t* h, s4 func);

/* 1 if the top-of-stack ref value dynamically matches `(ref null? ht)` — the §3.3 lattice
 * check used by ref.test/cast and (the embedder boundary) wasm_func_call arg typing. The
 * value must already sit at frame.stack[sp-1]; the instance lattice must be bound. */
int jav_top_ref_matches(vm_t* vm, heap_t* h, s4 ht, s4 nullable);

/* The top hierarchy of a heaptype: 0 internal/any, 1 func, 2 extern, 3 exn. Used at the
 * embedder boundary to reject a ref value from the wrong hierarchy before a §3.3 lattice check. */
int jav_ht_hierarchy(const vm_t* vm, s4 ht);

/* §7.1.8 invocation outcome: the engine owns the call ABI; the embedder boundary builds its trap
 * object from this rather than reading vm->unwinding/pending_exn/trapped directly. */
typedef enum { JAV_INVOKE_RETURN, JAV_INVOKE_TRAP, JAV_INVOKE_EXN } jav_invoke_t;
/* On JAV_INVOKE_EXN, *escaped is the uncaught exception OBJECT (a managed GC ref); NULL otherwise. */
jav_invoke_t jav_invoke(vm_t* vm, heap_t* h, s4 funcidx, struct gc_obj** escaped);
/* §4.2.1 funcaddr-model variant: invoke a funcinst directly (a funcref from a table/global). */
jav_invoke_t jav_invoke_fn(vm_t* vm, heap_t* h, const jav_func_t* fn, struct gc_obj** escaped);

#endif /* JAV_INTERP_H */
