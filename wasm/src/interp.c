/*
 * interp.c — the in-place WASM interpreter (tier 0), as a THREADED continuation
 * machine, structurally identical to the JIT: each generated handler does its
 * work and musttail-jumps to its successor. The only difference from the JIT is
 * resolution — the interpreter reads the next opcode and dispatches at runtime
 * (jav_next), where the JIT bakes the successor as _HOLE_cont. Traps bail
 * through the machine via jav_trap (never abort), as _HOLE_trap does for the JIT.
 */
#include "interp.h"
#include "gen_interp.h"
#include <stdlib.h>

#define TAIL __attribute__((musttail))

/* The dispatch continuation: read the next opcode and musttail to its handler.
 * Running off the end of the code is the function's implicit return — the
 * result(s) are on the value stack; stash the depth-0 result and terminate. */
void jav_next(vm_t* vm) {
    u1 op;
    vm->frame.instr_pc = (u4)vm->frame.code.pos;     /* instruction start, for §7.1.8 trap-frame byte offsets */
    if (!bbq_read_u8(&vm->frame.code, &op)) return;  /* function end: terminate this run. The results
                                                      * ARE the frame stack — nothing to stash. */
    if (vm->probe) vm->probe(vm, op);                /* §3.3.3 stop-before instrumentation seam (NULL = no probe) */
    TAIL return ((const opcode_handler_t*)vm->dispatch)[op](vm);
}

/* The trap continuation: record the trap and terminate the thread — control
 * unwinds back through the machine to interp_run. */
void jav_trap(vm_t* vm) {
    vm->trapped = 1;
}

/* Run the function at frame.code.pos to completion. Crosses from the C calling
 * convention into the threaded chain; returns when a handler terminates (end /
 * trap). The heap is stashed on the vm as the opaque handle handlers thread to
 * the natives. vm->trapped distinguishes trap from normal return; the results are
 * the top of the frame stack (jav_tos), not a side channel. */
jav_status_t interp_run(vm_t* vm, heap_t* h) {
    vm->dispatch = gen_interp_dispatch_table();   // per-vm dispatch (no shared global); the table is immutable
    vm->heap = h;
    vm->trapped = 0;
    vm->trap_reason = JAV_TRAP_NONE;   // clear with `trapped`, or the previous run's reason leaks into this one
    vm->exhausted = NULL;              // same: a stale limit message would mislabel the next trap
    vm->frame.stp = 0;
    jav_next(vm);
    return vm->trapped ? JAV_TRAP : JAV_RETURN;
}
