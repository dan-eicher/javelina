/*
 * jit_driver.h — the copy-and-patch JIT entry point. Pure C: it drives
 * jitterator's C executable-code buffer (jit_codebuf.h). The extern "C" guard
 * remains so a C++ consumer could still call in.
 */
#ifndef JAV_JIT_DRIVER_H
#define JAV_JIT_DRIVER_H

#include "runtime_api.h"   /* vm_t, jav_status_t */
#include "jav_ttree.h"     /* jav_tctx_t — the tier-2 walk's view of the module */

#ifdef __cplusplus
extern "C" {
#endif

/* JIT-compile the function body at vm->frame.code (positioned at the first
 * instruction, past the locals vector) and run it on vm. On JAV_RETURN the
 * results are the top of the frame stack (jav_tos). Operands are decoded once,
 * with the SAME bbq_read_* readers the container reader and interpreter use. */
jav_status_t jav_jit_run(vm_t* vm);

/* The JIT cache unit: compile a function ONCE (jit_compile), re-enter per call
 * (jit_enter), free when evicted (jit_free). A function table can store the handle
 * as `invoke_ctx` and `jit_invoke` as its `invoke`, so the runtime dispatches into
 * JITed callees through the same seam as interp/host — caller stays oblivious. */
/* `tcx` is the module context §3.2's resolve() reads — the classes of the locals,
 * globals, fields, elements and addrtypes this body's instructions name. NULL
 * compiles tier-1 only; the tree is what tier-2's tiling runs on. */
typedef struct jit_func_s jit_func_t;
jit_func_t*   jit_compile(bbq_ctx_t code, const jav_tctx_t* tcx);
jav_status_t jit_enter(const jit_func_t* fn, vm_t* vm);
void          jit_free(jit_func_t* fn);
jav_status_t jit_invoke(vm_t* vm, heap_t* h, void* ctx);   /* ctx = jit_func_t* */

#ifdef __cplusplus
}
#endif

#endif /* JAV_JIT_DRIVER_H */
