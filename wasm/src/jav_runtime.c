/*
 * jav_runtime.c — the runtime-service backend for the generated WASM handlers:
 * the control-transfer natives declared in wasm.def. This is the thin layer
 * under the runtime_api line; the generated interpreter compiles against the
 * prototypes alone.
 */
#include "runtime_api.h"
#include "interp.h"      /* jav_invoke / jav_top_ref_matches / jav_ht_hierarchy — defined here, declared there */
#include "heap.h"        /* the full heap_t — only the backend inspects it */
#include "jav_mem.h"     /* §4.6.8 linear-memory access: MEM_TRAP/mem_at/mem_ok + the inline-native i32 load/store */
#include "validate.h"    /* jav_functype_t — call_indirect's dynamic type match */
#include "bbq_vec.h"     /* the growable vector backing heap_t.mems */
#include <string.h>
#include <stdlib.h>

jav_status_t interp_run(vm_t* vm, heap_t* h);   /* the interp entry — call runs the callee through it (bail-to-interp) */

/* Addrtype-width (memory64/table64) stack access is the value model's, exposed by opgen as
 * JV_NPOP_ADDR(f,name) (tag-driven pop) / JV_NPUSH_ADDR(f,val,is64) (push) in runtime_api.h —
 * one owner, no hand-rolled re-implementation here. */

/* doControlTransferFromSTP (Titzer Listing 1): on a taken branch, keep the top
 * `vals` values, drop the `pop` values beneath them, then advance the code
 * cursor by delta_ip (relative to the post-operand position the handler left)
 * and the side-table pointer by delta_stp. */
static jav_status_t do_throw(vm_t* vm, gc_obj_t* exn);   /* the exception unwinder (defined below) */
static inline gc_obj_t* as_obj(s8 raw);                  /* JAV_NULLREF → NULL, else the gc_obj pointer (defined below) */

/* The §4.4.8 side-table walk is now `jav_do_transfer` (a static inline in jav_frame.h) — inlined into the
 * generated branch handler/stencil (no extern) and shared by the br_table/return/throw natives below. */

/* br_on_null/non_null (§4.4.8): is the operand ref the null sentinel? The DSL bodies branch on this and
 * (on the non-null path) re-push the ref through the value model — carried as an any_t so the full 8-byte
 * value AND its runtime tag survive (the old s4/T_REF wrapper truncated a managed ref and mis-tagged it). */


/* return is now inline in the .def (`func_return()` → set code.pos to the function end; the dispatcher
 * then halts with JAV_RETURN) — no native. */

/* call func: BOTH tiers funnel through here. Pop the callee's params off the
 * caller's operand stack, then run the callee to completion as a NESTED run (the C
 * call stack mirrors the WASM call stack). The callee is run by the interpreter
 * today (bail-to-interp); a host import or a JITed callee would dispatch here too.
 * On return the result is pushed onto the caller's stack and control resumes at the
 * opcode after the call — the interp handler tail-calls jav_next, the JIT stencil
 * tail-calls its _HOLE_cont. A trapping callee jumps the caller to its end. */
/* §8: the flat per-vm context cache is GONE. The active instance context is `vm->frame.ctx` (set by
 * jav_vm_init to the bare-VM `&vm->cluster`, or by the loader to `&inst->ctx`); EVERY cluster read goes
 * through it. jav_call_fn sets the callee frame's ctx and the frame save/restore (`*caller = saved`)
 * carries it back — so a re-entrant call cannot corrupt the caller's context (the A3 root fix). */

/* Call a function by FUNCINST REFERENCE (the funcaddr model, §4.2.1): `fn` may belong to a
 * DIFFERENT instance than the caller (a funcref pulled from a shared table / call_ref), and the
 * call seam switches the vm context to fn->inst_ctx. jav_call (by funcidx) is the same thing on
 * the current instance's function table. */
/* PRECONDITION: the params are already popped — sp sits BELOW them, and they stay in place in
 * the shared value stack at caller->stack[caller->sp ..], which is where the callee frame is
 * carved. The call opcodes declare the param count in their signature, so opgen drops sp
 * (`flag: pops_first`) before the body hands off; the two entries that do NOT come from an
 * opcode — jav_call (by funcidx) and jav_invoke_fn (the c-api) — drop it themselves. Doing it
 * here as well is the double-pop that this function's six opcode callers would each suffer. */
jav_status_t jav_call_fn(vm_t* vm, heap_t* h, const jav_func_t* fn) {
    frame_t* caller = &vm->frame;
    u4 np = fn->num_params;

    u4 depth_limit = vm->max_call_depth;   /* tunable; 0 or over the ceiling → the default */
    if (depth_limit == 0 || depth_limit > MAX_CALL_DEPTH) depth_limit = MAX_CALL_DEPTH;
    if (vm->call_depth >= depth_limit) {   /* stack exhaustion: trap, don't overflow the C stack */
        vm->exhausted = "call stack exhausted";
        vm->trapped = 1; vm->frame.code.pos = vm->frame.code.length; return JAV_TRAP;
    }
    frame_t saved = *caller;          /* lightweight save (~80 bytes — base ptrs, not arrays) */
    const instctx_t* loaded = saved.ctx;   /* §4.2 the instance context currently in the vm cache (caller's) */

    /* The callee frame's base — carved ABOVE the caller's regions, and FIXED for the
     * whole tail-call chain: each return_call REUSES this base (frame reuse, no stack
     * growth), so deep tail recursion costs O(1) C-stack. */
    slot_t* base_locals = saved.locals + saved.num_locals;
    u1*     base_ltypes = saved.local_types + saved.num_locals;
    slot_t* base_stack  = saved.stack + saved.sp;
    u1*     base_stypes = saved.stack_types + saved.sp;
    /* The current callee's params: for the first call they sit on the caller's stack at
     * saved.sp..; after a return_call they're the top of the just-finished frame (which
     * shares base_stack). Copying them into base_locals is cross-partition either way
     * (value_stack -> locals_store), so it never aliases. */
    slot_t* arg_src  = &saved.stack[saved.sp];
    u1*     argt_src = &saved.stack_types[saved.sp];

    vm->call_depth++;                 /* ONE level for the whole chain — tail calls don't deepen it */
    jav_status_t st;
    for (;;) {
        u4 nlocals = np + fn->num_locals;
        /* Reserve guard: room for this frame's per-frame caps — a clean trap, never an
         * overflow. Each arm names itself: the three are different conditions with different
         * fixes (recurse less, use fewer locals per frame, compile the function differently),
         * and collapsing them into one anonymous trap is what made a function over the local
         * cap indistinguishable from deep recursion.
         *
         * MAX_LOCALS is checked HERE and not only in the validator: §7.6 types a body against
         * the locals it declares and says nothing about how many an engine will admit, so this
         * cap is ours (§A "Implementation Limitations"), and a module carrying such a function
         * is well-formed — it just cannot run on this engine. */
        if ((size_t)(base_stack - vm->value_stack) + MAX_STACK > POOL_SLOTS) {
            vm->exhausted = "value stack exhausted"; st = JAV_TRAP; break;
        }
        if ((size_t)(base_locals - vm->locals_store) + nlocals > POOL_SLOTS) {
            vm->exhausted = "locals store exhausted"; st = JAV_TRAP; break;
        }
        if (nlocals > MAX_LOCALS) {
            vm->exhausted = "function exceeds the engine's per-frame local limit";
            st = JAV_TRAP; break;
        }

        frame_t* callee = &vm->frame;
        bbq_ctx_init(&callee->code, fn->code, fn->code_len);
        callee->locals = base_locals; callee->local_types = base_ltypes;
        callee->stack  = base_stack;  callee->stack_types  = base_stypes;
        for (u4 i = 0; i < np; i++) {              /* params: value stack -> locals, no aliasing */
            callee->locals[i]      = arg_src[i];
            callee->local_types[i] = argt_src[i];
        }
        for (u4 i = np; i < nlocals; i++) { callee->locals[i].l = 0; callee->local_types[i] = 0; }
        callee->num_locals = nlocals;
        callee->sp = 0;
        callee->sidetable = fn->sidetable; callee->stp = 0;
        callee->trytable = fn->trytable; callee->ntry = fn->ntry;   /* the callee's exception handlers */

        /* §4.2.6 switch the vm's instance context to the callee's DEFINING instance, so an
         * imported function runs against ITS memory/globals/tables, not the caller's. A host
         * funcinst (inst_ctx == NULL) keeps the caller's context. Same-instance calls (the
         * common case) don't change `loaded`, so this is a no-op there. */
        const instctx_t* fctx = fn->inst_ctx ? fn->inst_ctx : loaded;
        callee->ctx = fctx;   /* §8: the frame OWNS its context — every cluster read goes through
                               * frame.ctx, so there is no flat cache to load here or restore on return. */
        loaded = fctx;        /* track the chain's current ctx for the next return_call's default */

        /* Run the callee through its tier seam (NULL → interpret) — the runtime never
         * learns whether it is interpreted or JITed; the table entry decides. On a
         * return_call the run ends with tail_pending set (both tiers terminate exactly
         * as for `return`: the native jumps to the function end). */
        vm->tail_pending = 0;
        st = fn->invoke ? fn->invoke(vm, h, fn->invoke_ctx) : interp_run(vm, h);
        if (st == JAV_TRAP || vm->unwinding || !vm->tail_pending) break;

        /* return_call*: re-target and reuse the frame base. The new callee's params are
         * the top np values of the just-finished frame (in base_stack); its try handlers
         * exited with it, so drop any still pinned at this depth before the rebuild. The
         * target is a funcinst REFERENCE (tail_fn, possibly cross-instance) or a current-
         * instance funcidx (tail_func) — return_call_indirect/_ref set the former. */
        fn = vm->tail_fn ? vm->tail_fn : &vm->frame.ctx->functions[vm->tail_func];
        while (bbq_vec_len(vm->handlers) > 0 && bbq_vec_last(vm->handlers).call_depth >= vm->call_depth)
            bbq_vec_pop(vm->handlers);
        np = fn->num_params;
        /* Same popped-params precondition as the entry path: return_call* declares its param
         * count, so opgen already dropped sp past the args — they sit AT sp, not below it. */
        arg_src  = &base_stack[vm->frame.sp];
        argt_src = &base_stypes[vm->frame.sp];
    }
    /* the callee chain is done — drop any handlers it left installed. A branch out of a
     * try block past an enclosing try_table leaves that outer handler pinned (nothing prunes
     * it before the frame ends); without this it leaks into the next invocation reusing this
     * depth and can wrongly catch a later throw. (Unwinding already consumed them in do_throw.) */
    while (bbq_vec_len(vm->handlers) > 0 && bbq_vec_last(vm->handlers).call_depth >= vm->call_depth)
        bbq_vec_pop(vm->handlers);
    vm->call_depth--;
    /* §4.4.7 the results are the TOP num_results values of the callee's stack, not the
     * bottom ones: a mid-function `return` ends the run with whatever the body had below
     * them still in place. Capture the callee's final sp before the restore so they can
     * be normalised to the frame base, where the caller (and the c-api) expect them. */
    u4 callee_sp = vm->frame.sp;
    /* the trap-trace records `fn`'s index in ITS OWN instance's function table — capture that table
     * NOW, while the callee's ctx is still current (before `*caller = saved` restores the caller's). */
    const jav_func_t* callee_funcs = vm->frame.ctx->functions;
    /* §7.1.8 frame offset: the callee's byte offset — for the innermost frame the trapping
     * instruction (jav_next left instr_pc there), for an outer frame its inward `call`. Captured
     * before the restore below, which swaps in the caller's frame. */
    u4 trap_pc = vm->frame.instr_pc;

    *caller = saved;                  /* §8: restore the caller's frame — frame.ctx (its context) comes back WITH it */
    if (vm->unwinding) {              /* the chain threw and it escaped — resume the search here */
        vm->unwinding = 0;
        return do_throw(vm, vm->pending_exn);
    }
    if (st == JAV_TRAP) {             /* record this frame (its index in the callee's own function table) + its pc */
        bbq_vec_push(vm->trap_trace, (u4)(fn - callee_funcs));
        bbq_vec_push(vm->trap_pcs, trap_pc);
        vm->trapped = 1; vm->frame.code.pos = vm->frame.code.length; return JAV_TRAP;
    }
    /* Expose the callee's results to the caller. They are the top `n` of the callee's
     * stack (base_stack[callee_sp-n .. callee_sp)); slide them down to base_stack[0..n)
     * — which IS caller->stack + caller->sp — when the body left anything beneath them.
     * One rule for every arity; a lone result is not a special case. */
    u4 n = fn->num_results;
    if (callee_sp > n) {
        slot_t* rs = base_stack + (callee_sp - n);
        u1*     rt = base_stypes + (callee_sp - n);
        for (u4 i = 0; i < n; i++) { base_stack[i] = rs[i]; base_stypes[i] = rt[i]; }
    }
    caller->sp += n;
    return JAV_OK;
}

/* Call by funcidx into the CURRENT instance's function table (direct `call`, the common path). */
/* §4.6.2 ref.func: the funcaddr of the module's func x as a funcref any_t (T_REF scalar = funcinst ptr).
 * call x ≡ (ref.func x) (call_ref) — validation asserts x is in range, so no runtime check. */
/* Invoke a module function by funcidx — the loader (start fn) + c-api entry point (NOT the call opcode,
 * which composes jav_funcaddr + jav_invoke_ref). Keeps the defensive bounds-trap for external callers. */
jav_status_t jav_call(vm_t* vm, heap_t* h, s4 func) {
    if (func < 0 || (u4)func >= vm->frame.ctx->num_functions) {
        vm->trapped = 1; vm->frame.code.pos = vm->frame.code.length; return JAV_TRAP;
    }
    const jav_func_t* fn = &vm->frame.ctx->functions[func];
    vm->frame.sp -= fn->num_params;   /* not an opcode: satisfy jav_call_fn's popped-params precondition */
    return jav_call_fn(vm, h, fn);
}

/* §7.1.8 top-level invocation: run `funcidx` (args already on vm->frame, sp = nargs) and classify the
 * outcome, so the embedder boundary doesn't open-code the exn-unwinding protocol. RETURN: results sit
 * at frame.stack[0..]. TRAP: vm->trap_trace holds the func-index chain. EXN: *escaped is the uncaught
 * exception OBJECT (a managed GC ref, consumed here). */
/* Classify a completed top-level call into the §7.1.8 outcome (shared by both invoke entries). */
static jav_invoke_t jav_classify_outcome(vm_t* vm, jav_status_t st, gc_obj_t** escaped) {
    if (st == JAV_TRAP) return JAV_INVOKE_TRAP;
    if (vm->unwinding) { *escaped = vm->pending_exn; vm->unwinding = 0; vm->pending_exn = NULL; return JAV_INVOKE_EXN; }
    return JAV_INVOKE_RETURN;
}

jav_invoke_t jav_invoke(vm_t* vm, heap_t* h, s4 funcidx, gc_obj_t** escaped) {
    bbq_vec_free(vm->trap_trace); vm->trap_trace = NULL;   /* fresh trace for this invocation */
    bbq_vec_free(vm->trap_pcs); vm->trap_pcs = NULL;
    return jav_classify_outcome(vm, jav_call(vm, h, funcidx), escaped);
}

/* §7.1.8 top-level invocation by FUNCINST REFERENCE (the §4.2.1 funcaddr model): the embedder
 * boundary's entry for a funcref pulled from a table/global (no funcidx in the current instance).
 * The funcinst is self-describing (jav_call_fn switches the vm to fn->inst_ctx), so `vm` need only
 * be a driver in the funcinst's store; args sit on vm->frame (sp = nargs) exactly as for jav_invoke. */
jav_invoke_t jav_invoke_fn(vm_t* vm, heap_t* h, const jav_func_t* fn, gc_obj_t** escaped) {
    bbq_vec_free(vm->trap_trace); vm->trap_trace = NULL;
    bbq_vec_free(vm->trap_pcs); vm->trap_pcs = NULL;
    vm->frame.sp -= fn->num_params;   /* not an opcode: satisfy jav_call_fn's popped-params precondition */
    return jav_classify_outcome(vm, jav_call_fn(vm, h, fn), escaped);
}

/* Structural equality of two module function types (MVP: exact match; reference
 * subtyping arrives with #41). The dynamic gate call_indirect needs, and §4.5.2
 * import linking reuses (a func import matches on exact type). */
/* §3(b) the param count of a TYPE immediate — call_indirect / call_ref name their callee by
 * reference, but both carry a typeidx, and the param count is a property of that static type.
 * Taking it from the type (not the funcinst) makes the declared stack effect identical to the
 * one the validator derives; the type gate — call_indirect's IndirectCallTypeMismatch guard,
 * call_ref's validation — proves the callee agrees before the frame is carved. */
s4 jav_type_nparams(vm_t* vm, heap_t* h, s4 type) { (void)h; return (s4)vm->frame.ctx->types[type].nparams; }

int jav_functype_eq(const jav_functype_t* a, const jav_functype_t* b) {
    if (a->nparams != b->nparams || a->nresults != b->nresults) return 0;
    for (uint16_t i = 0; i < a->nparams;  i++) if (a->params[i]  != b->params[i])  return 0;
    for (uint16_t i = 0; i < a->nresults; i++) if (a->results[i] != b->results[i]) return 0;
    return 1;
}

/* call_indirect: pop the i32 index, look up the funcref in table `tbl`, and trap
 * on out-of-bounds / null / a function whose type doesn't match typeidx `typ`;
 * otherwise dispatch through the ordinary call path (which pops the params). */
/* §4.6.2 ref.cast (ref null typ) for call_indirect: does the (non-null) funcref r's type match typ?
 * §4.5.2 the funcinst carries its own functype; an instance funcref sharing our lattice checks via the
 * lattice (covers declared `sub` + recursive types), a host/cross-instance funcref (different type space)
 * via a structural closed-type match. The DSL body does the bounds (table.get) + null-trap + invoke. */
/* §4.4: a funcref's runtime type is its funcinst's CLOSED type — module-local type indices are
 * NOT meaningful across modules. Match a funcref from ANOTHER instance (or a host func) against
 * OUR target typeidx via the session registry's global-id lattice: §4.5.2 / §3.3.10 closed-type
 * subtyping, the SAME relation link_imports uses (a WASM funcinst's closed id comes off its own
 * gcanon; a host func's is interned on demand). Structural jav_functype_eq only matched primitive
 * sigs — its param compare is by module-local valtype, so any (ref $X) sig failed cross-module. */
static int funcref_closed_matches(vm_t* vm, const jav_func_t* fn, s4 target) {
    heap_t* hp = vm->heap;
    jav_typereg_t* reg = hp ? hp->typereg : NULL;
    const s4* rg = vm->frame.ctx->gcanon;
    if (!reg || !rg || !hp->typereg_gid_sub)   /* no session registry: structural fallback (single-module case) */
        return jav_functype_eq(fn->sig, &vm->frame.ctx->types[target]);
    int32_t prov = (fn->inst_ctx && fn->inst_ctx->gcanon)          /* WASM: funcinst's closed id via its own gcanon */
                 ? fn->inst_ctx->gcanon[fn->type_index]
                 : (hp->typereg_intern_ft ? hp->typereg_intern_ft(reg, fn->sig) : -1);  /* host: intern its functype */
    int32_t req = rg[target];
    if (prov < 0 || req < 0) return 0;
    return hp->typereg_gid_sub(reg, prov, req);   /* §4.5.2 closed-type subtyping over global ids */
}

/* §4.4/§4.5.2 the struct/array twin of funcref_closed_matches: a managed aggregate ref whose rtt is
 * NOT one of OUR module's types is a cross-instance value carrying its DEFINING module's closed type.
 * Its runtime type is that rtt's store-global canonical id (fixed at §4.5.2 absorb, `gc_rtt.gid`); match
 * it against our target typeidx's global id via the session registry — the same closed-type relation
 * link_imports and the funcref path use. Only concrete targets reach here; abstract targets (HT_STRUCT/
 * HT_ARRAY/…) are handled by value_heaptype's fallback, which is correct without a per-module id. */
static int gcref_closed_matches(vm_t* vm, const gc_obj_t* o, s4 target) {
    heap_t* hp = vm->heap;
    jav_typereg_t* reg = hp ? hp->typereg : NULL;
    const s4* rg = vm->frame.ctx->gcanon;
    if (!reg || !rg || !hp->typereg_gid_sub) return 0;   /* no session registry ⇒ no cross-module identity */
    int32_t prov = o->rtt->gid;                          /* the object's store-global closed type id */
    int32_t req  = rg[target];
    if (prov < 0 || req < 0) return 0;
    return hp->typereg_gid_sub(reg, prov, req);
}

int jav_funcref_typematch(vm_t* vm, heap_t* h, any_t r, s4 typ) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_types) return 0;
    const jav_func_t* fn = (const jav_func_t*)(uintptr_t)r.bits;   /* §4.2.1 funcref = a funcinst REFERENCE */
    return (fn->inst_ctx && vm->frame.ctx->lattice && fn->inst_ctx->lattice == vm->frame.ctx->lattice)
        ? jav_ht_sub(vm->frame.ctx->lattice, (s4)fn->type_index, (s4)typ)   /* same instance: local lattice */
        : funcref_closed_matches(vm, fn, typ);                             /* cross-instance / host: closed-type */
}

/* call_ref: the callee is a funcref on top (a function index, or null). Null traps; the
 * static type was proven by the validator (no dynamic type gate). Pop the funcref, leaving
 * the params below, then dispatch through the ordinary nested call path (which pops them). */
/* Invoke (§4.6.2): enter the funcinst's frame — the irreducible frame machinery (jav_call_fn). The funcref
 * `r` is a non-null funcinst pointer (the .def body null-traps first); its params stay below it on the
 * stack for jav_call_fn to consume. This is the one native primitive `call`/`call_ref`/`call_indirect`
 * (which §4.6.2 defines as compositions of ref.func / table.get / ref.cast + call_ref) all bottom out in. */
jav_status_t jav_invoke_ref(vm_t* vm, heap_t* h, any_t r) {
    return jav_call_fn(vm, h, (const jav_func_t*)(uintptr_t)r.bits);
}

/* ── tail calls (return_call / return_call_indirect / return_call_ref) ────────
 * A tail call REPLACES the current frame instead of nesting one: the native records
 * the target and ends this run (jump to the function end — both tiers then terminate
 * exactly as for `return`). jav_call's loop rebuilds the callee in the same frame
 * base (reuse, no C-stack growth), so tail recursion is bounded only by progress. The
 * callee's params stay on the operand stack; the loop copies them into the locals. */
/* return_call* tail Invoke (§4.6.2): set up a tail call to the (non-null) funcref — the frame-reuse path
 * in jav_call_fn's loop (tail_fn; line 167 uses it directly). The .def bodies do the funcaddr/table.get/
 * null/cast composition, same as call / call_ref / call_indirect but ending here, not jav_invoke_ref. */
jav_status_t jav_tail_invoke_ref(vm_t* vm, heap_t* h, any_t r) { (void)h;
    vm->tail_pending = 1; vm->tail_fn = (const jav_func_t*)(uintptr_t)r.bits;
    vm->frame.code.pos = vm->frame.code.length;
    return JAV_RETURN;
}

/* return_call_indirect: the call_indirect gate (bounds / null / dynamic type match),
 * then record the resolved target rather than nesting. The i32 index is on top, ABOVE
 * the params; popping it leaves the params on top for the frame rebuild. */

/* Table read/write/grow/fill (§4.6.7) — STACK-DRIVEN natives: a table entry is a
 * reference value (slot-sized, with a per-entry T_REF/T_GCREF tag, like the value stack),
 * so these ops pop/push the value AND its tag to be reftype-polymorphic (funcref scalar vs
 * managed gc pointer). A T_REF entry holds a scalar handle (funcidx/i31, −1 = null in slot.r);
 * a T_GCREF entry holds an 8-byte gc_obj pointer in slot.l that the GC scans. OOB → trap. */
/* §7.1.9 table_read/table_write — the c-api's entry to the SAME storage access the opcodes use
 * (TABLEINST_GET/SET, jav_frame.h), with the bounds check the embedder needs because its index was
 * never validated. The opcodes reach the identical macro through TABLE_GET/SET, their bounds
 * already proved by the declared guard — one authority for the (refs, types) layout, two callers
 * differing only in who establishes the precondition. No trap/boxing here: each caller surfaces OOB. */
int jav_tableinst_read(const jav_tableinst_t* t, u8 i, s8* raw, u1* tag) {
    if (i >= (u8)bbq_vec_len(t->refs)) return 0;
    any_t e = TABLEINST_GET(t, i);
    *raw = e.bits; *tag = e.kind; return 1;
}
int jav_tableinst_write(jav_tableinst_t* t, u8 i, s8 raw, u1 tag) {
    if (i >= (u8)bbq_vec_len(t->refs)) return 0;
    TABLEINST_SET(t, i, ((any_t){ .bits = raw, .kind = tag }));
    return 1;
}


/* Linear memory — THROUGH the heap (bounds-checked). MEM_TRAP / mem_at / mem_ok and the inline-native
 * i32 load/store live in jav_mem.h now (folded into the handler/stencil); the remaining width variants
 * below are still natives sharing those helpers. */

/* The heap owns each memory's bytes; an out-of-range memidx or OOB access traps. */
int jav_mem_add(heap_t* heap, u4 pages, u4 maxpages, int has_max, int is64) {
    jav_mem_t m;
    m.size = (u8)pages * 65536;
    m.has_max = (u1)(has_max != 0);
    m.max  = (u8)maxpages * 65536;   /* meaningful only when has_max */
    m.is64 = (u1)(is64 != 0);
    m.data = m.size ? (u1*)calloc(1, m.size) : NULL;
    int idx = bbq_vec_len(heap->mems);
    bbq_vec_push(heap->mems, m);
    return idx;
}
void jav_heap_free_mems(heap_t* heap) {
    for (int i = 0; i < bbq_vec_len(heap->mems); i++) free(heap->mems[i].data);
    bbq_vec_free(heap->mems);
    if (heap->typereg_free) heap->typereg_free(heap->typereg);   // loader-set destructor; no engine→loader symbol dep
    heap->typereg = NULL; heap->typereg_free = NULL;
}


/* f32x4.demote_f64x2_zero (§4.6): demote the two f64 lanes to f32 (lanes 0,1); lanes 2,3 = 0. */
v128_t jav_v128_demote_f64x2(vm_t* vm, heap_t* h, v128_t a) { (void)vm; (void)h;
    v128_t r; memset(&r,0,sizeof r); r.f32[0] = (f4)a.f64[0]; r.f32[1] = (f4)a.f64[1]; return r; }
/* memory.grow: append `delta` zero pages to memory `mi` (up to its max), reallocating the owned
 * buffer; returns the old page count, or -1 if it can't grow (over max / OOM). `delta` arrives
 * addrtype-width so a memory64 grow isn't truncated. */
s8   mem_grow_inst(jav_mem_t* m, s8 delta) {
    if (!m || delta < 0) return -1;
    u8 oldpages = m->size / 65536;
    u8 newpages = oldpages + (u8)delta;
    /* §4.5.3.8: cap at the declared max if any, else at the §3.2.15 addrtype ceiling (2^16 pages for
     * memory32, 2^48 for memory64). No sentinel: an unbounded memory is has_max==0, NOT max==ceiling. */
    u8 maxpages = m->has_max ? m->max / 65536 : (m->is64 ? (UINT64_C(1) << 48) : 65536u);
    if (newpages > maxpages) return -1;
    u8 newsize = newpages * 65536;
    u1* nd = (u1*)realloc(m->data, newsize ? newsize : 1);
    if (!nd) return -1;
    memset(nd + m->size, 0, newsize - m->size);   /* new pages are zero */
    m->data = nd; m->size = newsize;
    return (s8)oldpages;
}
/* Execution path (§4.6.8 memory.grow): resolve the module memidx through the frame's instance
 * to a store meminst, then grow it. The embedder (§7.1) instead holds a memaddr and calls
 * mem_grow_inst directly — no frame, no memidx resolution. */
s8   mem_grow(vm_t* vm, heap_t* h, s4 mi, s8 delta) { return mem_grow_inst(mem_at(vm, h, mi), delta); }
/* memory.size / memory.grow as stack-driven ops: the result (size) and operand (delta) carry the
 * memory's addrtype width (i64 for memory64, i32 else) — the tag is set/read by is64, not hardcoded. */
/* memory.size / memory.grow are now inline in the .def (`push_addr(...)` + the mem_pages/mem_is64 accessors);
 * mem_grow (the realloc storage layer) stays the native boundary. */

/* ── bulk memory (memory.fill / copy / init, data.drop) ──────────────────────
 * Ranges are bounds-checked against the live memory size (trap on overflow); a
 * zero-length op at the boundary is valid (mem_ok(ea,0) = ea <= size). */
/* d/n arrive addrtype-width (u8 via GPOP_ADDR) so a memory64 address ≥ 2³² bounds-checks
 * (mem_ok is u8) instead of truncating; mem32 values are zero-extended, so the same path
 * serves both. v is the i32 fill byte. */
void jav_memory_fill(vm_t* vm, heap_t* h, s4 mi, s8 d, s4 v, s8 n) {
    jav_mem_t* m = mem_at(vm, h, mi);
    if (!mem_ok(m, (u8)d, (u8)n)) { MEM_TRAP(vm); return; }
    memset(m->data + (u8)d, (int)((u4)v & 0xFF), (u8)n);
}
void jav_memory_copy(vm_t* vm, heap_t* h, s4 dmi, s4 smi, s8 d, s8 s, s8 n) {
    jav_mem_t* dm = mem_at(vm, h, dmi), *sm = mem_at(vm, h, smi);
    if (!mem_ok(dm, (u8)d, (u8)n) || !mem_ok(sm, (u8)s, (u8)n)) { MEM_TRAP(vm); return; }
    memmove(dm->data + (u8)d, sm->data + (u8)s, (u8)n);   /* overlap-safe (same memory) */
}
void jav_memory_init(vm_t* vm, heap_t* h, s4 mi, s4 seg, s8 d, s4 s, s4 n) {
    jav_mem_t* m = mem_at(vm, h, mi);
    if ((u4)seg >= vm->frame.ctx->num_data_segs) { MEM_TRAP(vm); return; }
    const jav_data_seg_t* ds = &vm->frame.ctx->data_segs[seg];
    u8 seglen = vm->frame.ctx->data_dropped[seg] ? 0 : ds->len;   // §4.6.7: a dropped segment is ε (length 0), not an unconditional trap
    if (n < 0 || s < 0 || (u8)s + (u8)n > seglen || !mem_ok(m, (u8)d, (u8)n)) { MEM_TRAP(vm); return; }   // s,n are i32 segment indices; d is addrtype
    memcpy(m->data + (u8)d, ds->bytes + s, (u8)n);
}
void jav_data_drop(vm_t* vm, heap_t* h, s4 seg) { (void)h;
    if ((u4)seg < vm->frame.ctx->num_data_segs) vm->frame.ctx->data_dropped[seg] = 1;
}

/* ── bulk table ops (§4.6.7) — the table analogue of the memory bulk ops above.
 * size reports the entry count; grow appends `delta` copies of an init ref, honoring
 * the table's max (push old size, else -1); fill/copy/init move ref ranges, OOB → trap
 * (copy is overlap-safe). Indices are table32 (i32); a funcref rides as an int. */
/* table.size is now inline in the .def (`push_addr(table_len, table_is64)` → the addr-width JV_NPUSH_ADDR). */
/* §7.1.9 table_grow — the bounds/cap + append authority (declared in jav_frame.h), shared by the
 * table.grow opcode and the c-api wasm_table_grow. */
s8 jav_tableinst_grow(jav_tableinst_t* t, u8 delta, s8 raw, u1 tag) {
    u8 oldsize = bbq_vec_len(t->refs), newsize = oldsize + delta;
    /* §4.5.3.10: cap at the declared max if any, else at the §3.2.16 addrtype ceiling. */
    u8 maxcap = t->has_max ? (u8)t->max : (t->is64 ? UINT64_MAX : 0xFFFFFFFFu);
    if (newsize > maxcap) return -1;
    for (u8 k = 0; k < delta; k++) { bbq_vec_push(t->refs, raw); bbq_vec_push(t->types, tag); }
    return (s8)oldsize;
}
/* table.grow x: the §7.1.9 grow boundary (the allocator). opgen pops the addr-width delta + the init ref
 * (any) and pushes the old size via push_addr in the .def body; this native does the bounds/cap + append. */
s8 jav_table_grow(vm_t* vm, heap_t* h, s4 tbl, s8 delta, any_t initref) { (void)h;
    if ((u4)tbl >= bbq_vec_len(vm->frame.ctx->tables) || delta < 0) return -1;
    return jav_tableinst_grow(&vm->frame.ctx->tables[tbl], (u8)delta, initref.bits, initref.kind);
}
/* table.fill x (§4.6): opgen pops index i, the fill ref v (any_t), and count n; fill [i,i+n) with v. */
void jav_table_init(vm_t* vm, heap_t* h, s4 tbl, s4 seg, s8 d, s4 s, s4 n) { (void)h;
    jav_tableinst_t* t = (u4)tbl < bbq_vec_len(vm->frame.ctx->tables) ? &vm->frame.ctx->tables[tbl] : NULL;
    if (!t || (u4)seg >= vm->frame.ctx->num_elem_segs) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsTableAccess); return; }
    const jav_elem_seg_t* es = &vm->frame.ctx->elem_segs[seg];
    u8 seglen = vm->frame.ctx->elem_dropped[seg] ? 0 : es->len;   // §4.6.7: a dropped elem segment is ε (length 0)
    u8 tl = bbq_vec_len(t->refs);                       // d is addrtype; s,n are i32 segment indices
    if (n < 0 || s < 0 || (u8)s + (u8)n > seglen || (u8)d > tl || (u8)n > tl - (u8)d) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsTableAccess); return; }
    /* copy each value WITH its runtime tag (jav_elem_seg_t.types): a GC-allocated elem item must
     * land as a scanned T_GCREF slot, not a hardcoded funcref tag (which hid it from the tracer). */
    for (s4 k = 0; k < n; k++) { t->refs[d + k] = (s8)es->values[s + k]; t->types[d + k] = es->types[s + k]; }
}
void jav_elem_drop(vm_t* vm, heap_t* h, s4 seg) { (void)h;
    if ((u4)seg < vm->frame.ctx->num_elem_segs) vm->frame.ctx->elem_dropped[seg] = 1;
}

/* ── exceptions (throw / throw_ref / try_table) ──────────────────────────────
 * try_table installs a handler over its body; throw/throw_ref unwind to the nearest
 * matching catch — same-frame via the side-table (set stp/code.pos, do_transfer),
 * cross-frame by ending the function with vm->unwinding set so jav_call resumes the
 * search in the caller. An exn instance is a MANAGED GC object; an exnref is the
 * object pointer (a normal T_GCREF managed ref), so the store has no cap and dead
 * exceptions are reclaimed by liveness. */

/* The managed exception object payload (§4.2.12): the tag's store identity, the field count, the field
 * values as full 16-byte slots (so a v128 field survives), then the per-field runtime type tags right
 * after the slots. WHICH fields are managed references is ALSO recorded in the object's rtt
 * (ref_offsets, from the same ftypes) so the generic tracer follows them. */
static inline exn_obj_t* exn_obj(gc_obj_t* o) { return EXN_OBJ(o); }   /* layout authority: jav_frame.h */
static inline u1* exn_ftypes(gc_obj_t* o) { return EXN_FTYPES(o); }
#define EXN_FIELD_OFF(k) ((uint32_t)(sizeof(gc_obj_t) + offsetof(exn_obj_t, fields) + (size_t)(k) * sizeof(slot_t)))
/* total object size for `nfields`: header + value slots + the trailing per-field type tags. */
static inline uint32_t exn_obj_size(u4 nfields) {
    return (uint32_t)(sizeof(gc_obj_t) + offsetof(exn_obj_t, fields) + (size_t)nfields * (sizeof(slot_t) + 1));
}

/* The GC-object layout for an exception of tag `tagaddr` whose `nfields` fields have runtime types
 * `ftypes` (T_GCREF ⇒ a ref field). A tag's field shape is static, so one rtt serves every throw of
 * it — cached on the heap by tagaddr. Heap-owned (freed at jav_heap_gc_destroy). */
const gc_rtt_t* jav_exn_rtt_for(heap_t* heap, u4 tagaddr, u4 nfields, const u1* ftypes) {
    for (int i = 0, n = bbq_vec_len(heap->exn_rtts); i < n; i++)
        if (heap->exn_rtts[i].tag == tagaddr) return heap->exn_rtts[i].rtt;
    u4 nrefs = 0; for (u4 k = 0; k < nfields; k++) if (ftypes[k] == T_GCREF) nrefs++;
    gc_rtt_t* r = (gc_rtt_t*)malloc(sizeof(gc_rtt_t) + (size_t)nrefs * sizeof(uint32_t));
    r->size = exn_obj_size(nfields);
    r->nrefs = nrefs; r->kind = GC_KIND_STRUCT; r->elem_is_ref = 0; r->elem_store_w = 0;
    u4 j = 0; for (u4 k = 0; k < nfields; k++) if (ftypes[k] == T_GCREF) r->ref_offsets[j++] = EXN_FIELD_OFF(k);
    jav_exn_rtt_t e = { tagaddr, r }; bbq_vec_push(heap->exn_rtts, e);
    return r;
}

/* §7.1.12 exn-object accessors for the embedder (the c-api reads the managed object without knowing
 * the GC layout): tag identity, field count, a field value, and a field's runtime type tag. */
u4     jav_exn_tag(gc_obj_t* o)         { return exn_obj(o)->tag; }
u4     jav_exn_nfields(gc_obj_t* o)     { return exn_obj(o)->nfields; }
slot_t jav_exn_field(gc_obj_t* o, u4 k) { return exn_obj(o)->fields[k]; }
u1     jav_exn_ftype(gc_obj_t* o, u4 k) { return exn_ftypes(o)[k]; }
/* Build a managed exn object from host-supplied field values + types (the c-api install direction).
 * `fields`/`ftypes` are read AFTER the alloc, so a collection it triggers (with the host copy's refs
 * already rooted by the caller) can relocate them safely. */
gc_obj_t* jav_exn_alloc(vm_t* vm, u4 tagaddr, u4 nfields, const slot_t* fields, const u1* ftypes) {
    const gc_rtt_t* rtt = jav_exn_rtt_for(vm->heap, tagaddr, nfields, ftypes);
    gc_obj_t* o = jav_gc_new(vm, rtt, rtt->size);
    exn_obj_t* x = exn_obj(o); x->tag = tagaddr; x->nfields = nfields;
    u1* ft = exn_ftypes(o);
    for (u4 k = 0; k < nfields; k++) { x->fields[k] = fields[k]; ft[k] = ftypes[k]; }
    return o;
}

/* drop handlers in the current frame whose try block has been exited (pc past end_pc),
 * so a later throw never matches a dead handler and the stack stays bounded. */
static void prune_handlers(vm_t* vm, u4 pc) {
    while (bbq_vec_len(vm->handlers) > 0) {
        jav_handler_t* hd = &bbq_vec_last(vm->handlers);
        if (hd->call_depth != vm->call_depth || pc < hd->end_pc) break;
        bbq_vec_pop(vm->handlers);
    }
}

/* Unwind exception OBJECT `exnobj` to the nearest matching catch in the CURRENT frame; if none, mark
 * the vm unwinding so the caller's jav_call resumes the search (pending_exn keeps the object alive). */
static jav_status_t do_throw(vm_t* vm, gc_obj_t* exnobj) {
    frame_t* f = &vm->frame;
    exn_obj_t* exn = exn_obj(exnobj);
    prune_handlers(vm, (u4)f->code.pos);
    while (bbq_vec_len(vm->handlers) > 0 && bbq_vec_last(vm->handlers).call_depth == vm->call_depth) {
        jav_handler_t* hd = &bbq_vec_last(vm->handlers);
        bbq_ctx_t cc; bbq_ctx_init(&cc, hd->catches,
                                   f->code.length - (size_t)(hd->catches - f->code.data));
        int matched = -1; u1 mk = 0;
        for (u4 i = 0; i < hd->ncatch; i++) {
            u1 ck = 0; bbq_read_u8(&cc, &ck);
            u4 tag = 0; if (ck == 0 || ck == 1) bbq_read_uleb128_u32(&cc, &tag);
            u4 label; bbq_read_uleb128_u32(&cc, &label); (void)label;
            // §4.4 catch matches by TAG ADDRESS identity: module.tags[catch_x] == exn.tag (both tagaddrs).
            if (ck == 2 || ck == 3 || (tag < vm->frame.ctx->num_tags && vm->frame.ctx->tag_ids[tag] == exn->tag)) { matched = (int)i; mk = ck; break; }
        }
        if (matched >= 0) {
            f->sp = hd->sp;                                  /* restore to the try base */
            const u1* ft = exn_ftypes(exnobj);
            if (mk == 0 || mk == 1)                          /* catch / catch_ref: push the tag's fields (precise types) */
                for (u4 k = 0; k < exn->nfields; k++) {
                    f->stack[f->sp] = exn->fields[k]; f->stack_types[f->sp] = ft[k]; f->sp++;
                }
            if (mk == 1 || mk == 3) {                        /* the _ref forms also push the exnref (the object) */
                f->stack[f->sp].l = (s8)(uintptr_t)exnobj; f->stack_types[f->sp] = T_GCREF; f->sp++;
            }
            bbq_vec_pop(vm->handlers);                       /* consume this handler */
            f->code.pos = hd->try_pc; f->stp = hd->catch_stp + (u4)matched;
            jav_do_transfer(vm);                                 /* ride the side-table to the catch label */
            return JAV_OK;
        }
        bbq_vec_pop(vm->handlers);                            /* this try didn't catch; keep searching outward */
    }
    vm->pending_exn = exnobj; vm->unwinding = 1;             /* escapes this frame -> propagate to the caller */
    f->code.pos = f->code.length;
    return JAV_OK;
}

/* throw (§4.4.10) substrate primitives — the reduction (alloc exn, fill its fields, unwind) is in the .def
 * body: alloc FIRST so the field operands stay stack-rooted across jav_gc_new, then pop each field, then
 * throw. The per-field copy stays native: an exn field is a full 16-byte slot_t (v128-capable), wider than
 * the 8-byte any_t value-model carrier — so pop() can't carry it. The alloc/loop/unwind composition is the
 * DSL's. */
ref_t jav_exn_alloc_for_throw(vm_t* vm, heap_t* h, s4 tag) {
    frame_t* f = &vm->frame;
    if ((u4)tag >= vm->frame.ctx->num_tags) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    u4 np = vm->frame.ctx->tags[tag].nparams;
    /* the np field types sit at [sp-np, sp) and STAY there (rooted) until the body pops them. */
    const gc_rtt_t* rtt = jav_exn_rtt_for(h, vm->frame.ctx->tag_ids[tag], np, &f->stack_types[f->sp - np]);
    gc_obj_t* o = jav_gc_new(vm, rtt, rtt->size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    exn_obj_t* x = exn_obj(o); x->tag = vm->frame.ctx->tag_ids[tag]; x->nfields = np;   // §4.2 store-identity tagaddr
    return (ref_t)(uintptr_t)o;
}
int jav_tag_nparams(vm_t* vm, heap_t* h, s4 tag) { (void)h;
    return (u4)tag < vm->frame.ctx->num_tags ? (int)vm->frame.ctx->tags[tag].nparams : 0;
}
jav_status_t jav_throw_exn(vm_t* vm, heap_t* h, ref_t exn) { (void)h;
    return do_throw(vm, as_obj((s8)exn));
}

/* The exnref is popped by opgen (declared operand) and the null case is a declared `error:`
 * guard, so this is just the unwind — no stack reach-down, no hidden trap cause. */
jav_status_t jav_throw_ref(vm_t* vm, heap_t* h, any_t r) { (void)h;
    return do_throw(vm, (gc_obj_t*)(uintptr_t)r.bits);
}

jav_status_t jav_try_table(vm_t* vm, heap_t* h) { (void)h;
    frame_t* f = &vm->frame;
    int32_t bt = 0; bbq_read_sleb128_i32(&f->code, &bt);
    if (bt == -29 || bt == -28) { int32_t ht = 0; bbq_read_sleb128_i32(&f->code, &ht); }  /* §5.3.3 (ref null? ht) blocktype: trailing heaptype */
    u4 nparams = (bt >= 0 && (u4)bt < vm->frame.ctx->num_types) ? vm->frame.ctx->types[bt].nparams : 0;
    u4 ncatch = 0; bbq_read_uleb128_u32(&f->code, &ncatch);
    const u1* catches = f->code.data + f->code.pos;          /* the first catch CLAUSE (after the count) */
    for (u4 i = 0; i < ncatch; i++) {                        /* skip the catch vector to the body */
        u1 ck = 0; bbq_read_u8(&f->code, &ck);
        u4 tmp; if (ck == 0 || ck == 1) bbq_read_uleb128_u32(&f->code, &tmp);
        bbq_read_uleb128_u32(&f->code, &tmp);
    }
    u4 try_pc = (u4)f->code.pos;                             /* body start = handler install point */
    prune_handlers(vm, try_pc);
    const jav_try_t* tt = NULL;
    for (u4 i = 0; i < f->ntry; i++) if (f->trytable[i].try_pc == try_pc) { tt = &f->trytable[i]; break; }
    if (tt) {
        jav_handler_t hd;                                            /* push onto the handler stack (a bbq_vec — no cap) */
        hd.call_depth = vm->call_depth; hd.sp = f->sp - nparams;     /* try base (body consumes the params) */
        hd.catch_stp = tt->catch_stp; hd.try_pc = try_pc; hd.end_pc = tt->end_pc;
        hd.catches = catches; hd.ncatch = ncatch;
        bbq_vec_push(vm->handlers, hd);
    }
    f->stp += ncatch;   /* the body skips the catch entries (do_throw references them absolutely via catch_stp) */
    return JAV_OK;
}

/* u64 -> f64 (f64.convert_i64_u): a native because x86-64 has no unsigned-64 ->
 * fp instruction — clang synthesizes it with a .rodata constant, which lives
 * fine here in normally-compiled code but cannot ride in a copy-and-patch stencil. */
f8   convert_u64_f64(vm_t* vm, heap_t* h, s8 a)      { (void)vm; (void)h; return (f8)(u8)a; }

/* float -> u64 trunc (i64.trunc_f*_u): range-check + convert + trap, all in
 * normal code — x86 has no float->unsigned-64 op, so clang's synthesized
 * conversion (with its .rodata constant) can't ride in a stencil. (-1, 2^64).
 * §7.10 gives these two DISTINCT trap reasons and the range test alone cannot
 * tell them apart: NaN is "invalid conversion to integer", everything else out
 * of range is "integer overflow". NaN is tested first because it satisfies
 * neither half of the range comparison. */
s8 trunc_u64_f32(vm_t* vm, heap_t* h, f4 a) { (void)h;
    if (a != a) { JAV_TRAP_WITH(vm, JAV_TRAP_InvalidConversionToInteger); return 0; }
    if (!(a > -1.0f && a < 18446744073709551616.0f)) { JAV_TRAP_WITH(vm, JAV_TRAP_IntegerOverflow); return 0; }
    return (s8)(u8)a; }
s8 trunc_u64_f64(vm_t* vm, heap_t* h, f8 a) { (void)h;
    if (a != a) { JAV_TRAP_WITH(vm, JAV_TRAP_InvalidConversionToInteger); return 0; }
    if (!(a > -1.0  && a < 18446744073709551616.0))  { JAV_TRAP_WITH(vm, JAV_TRAP_IntegerOverflow); return 0; }
    return (s8)(u8)a; }

/* ── GC: root scan + collector binding ──────────────────────────────────────
 * The collector (behind heap->gc) asks the runtime to enumerate its roots; we hand
 * it the managed slots (tagged T_GCREF) on the shared value stack + locals — the
 * GC follows only those, never funcref/i31 handles. Globals, tables, live element
 * segments and the in-flight exception are scanned too, each off its own per-slot
 * type array. A managed ref rides as an 8-byte pointer in slot.l. */
static void jav_gc_enum_roots(gc_heap_t* gch, gc_root_visit_fn visit, void* ctx) {
    vm_t* vm = (vm_t*)gch->user;
    frame_t* f = &vm->frame;
    size_t vtop = (size_t)(f->stack - vm->value_stack) + f->sp;        /* whole live operand stack */
    for (size_t i = 0; i < vtop; i++)
        if (vm->value_types[i] == T_GCREF) visit((gc_obj_t**)&vm->value_stack[i].l, ctx);
    size_t ltop = (size_t)(f->locals - vm->locals_store) + f->num_locals;
    for (size_t i = 0; i < ltop; i++)
        if (vm->local_type_store[i] == T_GCREF) visit((gc_obj_t**)&vm->locals_store[i].l, ctx);
    /* module globals (a bbq_vec bound from the active instance — its length is the count). */
    for (size_t i = 0, n = bbq_vec_len(vm->frame.ctx->globals); i < n; i++)
        if (vm->frame.ctx->global_types[i] == T_GCREF) visit((gc_obj_t**)&vm->frame.ctx->globals[i]->l, ctx);
    /* table entries: a managed-ref table (externref/struct/array/exn) holds gc pointers in
     * its T_GCREF-tagged slots — scan those (funcref/i31/null entries are T_REF, skipped). */
    for (size_t ti = 0, nt = bbq_vec_len(vm->frame.ctx->tables); ti < nt; ti++) {
        jav_tableinst_t* t = &vm->frame.ctx->tables[ti];
        for (size_t i = 0, n = bbq_vec_len(t->refs); i < n; i++)
            if (t->types[i] == T_GCREF) visit((gc_obj_t**)&t->refs[i], ctx);
    }
    /* §4.2.12 eleminst holds refs: a GC-allocated const-expr item parked in a LIVE (non-dropped)
     * element segment is reachable — table.init / array.new_elem can still materialize it — so it
     * is a root; a dropped segment is ε and must NOT keep its values alive. Mirrors
     * jav_instance_visit_roots (the c-api's per-instance scan): this engine-side scan covers the
     * BOUND instance for bare-VM embedders with no store hook, same as the global/table loops
     * above. The visit targets the mutable pool slot so evacuation can rewrite it. */
    for (size_t s = 0, ns = vm->frame.ctx->num_elem_segs; s < ns; s++) {
        if (vm->frame.ctx->elem_dropped[s]) continue;
        const jav_elem_seg_t* es = &vm->frame.ctx->elem_segs[s];
        for (u4 j = 0; j < es->len; j++)
            if (es->types[j] == T_GCREF) visit((gc_obj_t**)(uintptr_t)&es->values[j], ctx);
    }
    /* the exception being unwound across frames (§4.2.12): reachable only via pending_exn until a
     * handler re-pushes it, so it is a root for the duration of the unwind. */
    if (vm->unwinding && vm->pending_exn) visit(&vm->pending_exn, ctx);
    /* embedder extra roots (e.g. c-api host-created globals/tables holding managed refs). */
    if (vm->extra_roots) vm->extra_roots(vm->extra_roots_ctx, (jav_root_visit_fn)visit, ctx);
}

/* The heap checker found a broken invariant. This is the engine's own defect, so it is reported
 * the way stack exhaustion is — stop executing guest code and hand the outcome to the host — and
 * NOT by ending the process: javelina is a library, and a library that aborts turns any bug an
 * attacker can provoke into a way to kill the application. §1.1.3 puts the policy on the embedder
 * ("Such considerations are an embedder's responsibility"), so all that happens here is that the
 * vm stops. The invariant is a static string; retaining the pointer is safe. */
static void jav_gc_corruption(void* ctx, const gc_verify_t* what) {
    vm_t* vm = (vm_t*)ctx;
    vm->engine_fault = what->invariant ? what->invariant : "heap invariant violated";
    vm->trapped = 1;
    vm->frame.code.pos = vm->frame.code.length;   /* bail at the next check, as jav_call_fn does */
}

void jav_heap_gc_init(heap_t* heap, vm_t* vm) {
    heap->gc = jav_immix_collector_create(jav_gc_enum_roots, vm);
}

void jav_heap_gc_verify(heap_t* heap, vm_t* vm, int on) {
    if (heap->gc.set_corruption_handler)
        heap->gc.set_corruption_handler(heap->gc.self, on ? jav_gc_corruption : NULL, vm);
}

/* ── host externref boxing (§4.5.1 ref.host) ─────────────────────────────────
 * A GC-managed object wrapping one opaque host pointer; nrefs=0 so it is kept
 * alive but never traced into. An embedded host externref value IS one of these
 * (a normal T_GCREF managed ref), so it rides the value stack / locals / globals
 * and survives collection exactly like any struct/array. */
static const gc_rtt_t JAV_HOST_BOX_RTT = {
    /* designated — a POSITIONAL init here only stayed correct across the rtt's
     * field additions because GC_KIND_STRUCT happens to be 0 */
    .size = (uint32_t)(sizeof(gc_obj_t) + sizeof(void*)), .kind = GC_KIND_STRUCT,
};
gc_obj_t* jav_host_box_new(vm_t* vm, void* host) {
    gc_obj_t* o = jav_gc_new(vm, &JAV_HOST_BOX_RTT, JAV_HOST_BOX_RTT.size);
    *(void**)gc_obj_payload(o) = host;
    return o;
}
int jav_is_host_box(const gc_obj_t* o) { return o && o->rtt == &JAV_HOST_BOX_RTT; }
void* jav_host_box_get(const gc_obj_t* o) {
    return jav_is_host_box(o) ? *(void* const*)gc_obj_payload((gc_obj_t*)o) : (void*)o;
}
void jav_heap_gc_destroy(heap_t* heap) {
    if (heap->gc.self && heap->gc.destroy) { heap->gc.destroy(heap->gc.self); heap->gc.self = NULL; }
    for (int i = 0, n = bbq_vec_len(heap->exn_rtts); i < n; i++) free(heap->exn_rtts[i].rtt);  // heap-owned per-tag exn layouts
    bbq_vec_free(heap->exn_rtts); heap->exn_rtts = NULL;
}
/* allocate a `size`-byte managed object of type `rtt` through the heap's collector
 * (the seam struct.new / array.new call) */
gc_obj_t* jav_gc_new(vm_t* vm, const gc_rtt_t* rtt, u4 size) {
    return vm->heap->gc.alloc(vm->heap->gc.self, rtt, size);
}

/* ── struct.new / get / set ──────────────────────────────────────────────────
 * Fields are slot-sized (8 bytes), laid out after the gc_obj header; the rtt's
 * ref_offsets mark which are managed references (for tracing + the T_GCREF tag). */
/* Managed null is the embedder sentinel JAV_NULLREF stored in a slot; as_obj maps
 * it (and the NULL pointer) to NULL so a deref traps cleanly instead of faulting. */
static inline gc_obj_t* as_obj(s8 raw) {
    return JAV_REF_ISNULL(raw) ? NULL : (gc_obj_t*)(uintptr_t)raw;
}

/* struct.new (§4.6) substrate primitives — the reduction (alloc, then fill) now lives in the .def body:
 * it allocs FIRST, then pop()s the fields, so the field operands stay GC-rooted on the stack across the
 * allocation (no operand crosses jav_gc_new in a C local). These are the irreducible pieces it calls. */
/* Constructor-time field/element writes. These duplicate STRUCT_SET/ARRAY_SET, which are
 * already inline — but those take a ref-typed handle and a fresh object arrives as an
 * `any_t` carrier, which cannot be cast to a pointer. Dissolving them needs Stage 1's
 * indexed accessors (`result[i] = v`), where opgen's SIndex store owns the operand type;
 * a macro cannot bridge the two shapes without becoming a second authority. */
void jav_obj_set_field(vm_t* vm, heap_t* h, any_t o, s4 fld, any_t v) { (void)vm; (void)h;
    ((s8*)gc_obj_payload(as_obj(o.bits)))[fld] = v.bits;
}
/* Returns a REF, not an any_t carrier: the body then writes fields through the same
 * inline STRUCT_SET that struct.set uses (a ref-typed handle is what the macro casts),
 * and tags the result T_GCREF on push. That is what dissolves jav_obj_set_field. */
ref_t jav_struct_alloc(vm_t* vm, heap_t* h, s4 typ) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    gc_obj_t* o = jav_gc_new(vm, rtt, rtt->size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    return (ref_t)(uintptr_t)o;
}
int jav_struct_nfields(vm_t* vm, heap_t* h, s4 typ) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts) return 0;
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    /* The RTT's own count when set; 0 = a hand-built narrow-only rtt, where the
     * old size/8 derivation is exact (a v128 field makes size ≠ nfields*8, but
     * wide fields only ever arrive via build_rtts, which sets nfields). */
    return rtt->nfields ? (int)rtt->nfields
                        : (int)((rtt->size - (u4)sizeof(gc_obj_t)) / 8);
}
/* write field fld (8-byte, the .l view) of the freshly-allocated object o = v. */

/* struct.get (§4.6): read field `fld`. opgen pops the struct ref + pushes the result; the native returns
 * the field's 8-byte value bundled with its RUNTIME tag (a ref field is T_GCREF, else T_INT). On a null
 * trap it returns a null scalar ref — GC-safe, since GPUSH_ANY runs before the trap-bail. */
/* ── arrays ──────────────────────────────────────────────────────────────────
 * Layout: header, then the u4 length (padded to 8), then `length` 8-byte elements.
 * Elements ride the 8-byte (.l) view of a value — every scalar/ref fits, and the GC
 * tracer reads ref elements at this same 8-byte stride. (Not sizeof(slot_t): a slot
 * is 16 bytes for v128, which arrays don't yet carry.) get/set are bounds-checked. */
static inline s8* arr_elems(gc_obj_t* o) { return (s8*)((u1*)gc_obj_payload(o) + GC_ARRAY_ELEMS_OFFSET); }
static inline u4  arr_len(gc_obj_t* o)   { return *(u4*)gc_obj_payload(o); }

/* Constructor-time element write — the array counterpart of jav_obj_set_field above;
 * same Stage-1 note applies (ARRAY_SET is already inline but takes a ref, not any_t). */
void jav_obj_set_elem(vm_t* vm, heap_t* h, any_t o, s4 i, any_t v) { (void)vm; (void)h;
    arr_elems(as_obj(o.bits))[i] = v.bits;
}


/* ── downcasts (ref.test / ref.cast / br_on_cast), §4.4 / §3.3.3 ─────────────
 * The dynamic check is value's-runtime-heaptype <: target heaptype, decided by the real
 * §3.3 lattice (jav_ht_sub). A heap-type immediate < 0 is an abstract heap type, >= 0 a
 * concrete typeidx. The value's runtime heaptype is recovered from its representation —
 * but i31 and funcref share the unboxed T_REF representation, so the TARGET's top
 * hierarchy disambiguates them (the validator guarantees operand and target share a
 * hierarchy, §3.4.6 ref.test rt:ok with the operand <: the hierarchy top). */

/* The top hierarchy of a (target) heap type: 1 = func, 2 = extern, 3 = exn, 0 = internal
 * (the `any` family: any/eq/i31/struct/array/none + concrete struct/array). */
int jav_ht_hierarchy(const vm_t* vm, s4 ht) {
    if (ht >= 0) {                                  /* concrete typeidx → by its structural kind */
        const jav_subtype_ctx_t* cx = vm->frame.ctx->lattice;
        return (cx && (u4)ht < cx->ntypes && cx->kinds[ht] == WST_FUNC) ? 1 : 0;
    }
    switch (ht) {
        case HT_FUNC: case HT_NOFUNC:     return 1;
        case HT_EXTERN: case HT_NOEXTERN: return 2;
        case HT_EXN: case HT_NOEXN:       return 3;
        default:                          return 0;   /* any/eq/i31/struct/array/none */
    }
}

/* The concrete typeidx whose rtt is `rtt`, or -1 (the rtt pool is typeidx-indexed). */
static int32_t rtt_typeidx(const vm_t* vm, const gc_rtt_t* rtt) {
    for (u4 t = 0; t < vm->frame.ctx->num_struct_rtts; t++)
        if (vm->frame.ctx->struct_rtts[t] == rtt) return (int32_t)t;
    return -1;
}

/* The runtime heap type (an HT_* code or a concrete typeidx) of a NON-NULL operand,
 * read from its representation + the test's hierarchy `hier` (which disambiguates the
 * shared i31/funcref/extern-scalar T_REF encoding). */
static int32_t value_heaptype(vm_t* vm, u8 raw, u1 tag, int hier) {
    if (hier == 2) return HT_EXTERN;                /* any non-null externref ≤ (ref extern) */
    if (hier == 3) return HT_EXN;
    if (hier == 1) {                                /* func hierarchy: the funcref's concrete type */
        const jav_func_t* fn = (const jav_func_t*)(uintptr_t)raw;   /* §4.2.1 funcref = a funcinst pointer */
        return (int32_t)fn->type_index;
    }
    /* Ask the VALUE, not the tag. A §4.2.1 `ref.i31 u31` is self-identifying — tagged
     * (v << 3) | 1, so never IMX_OBJECT_ALIGN-aligned — and that test is exact wherever the value
     * came from. The `tag` is a side channel: an i31 read out of an anyref/eqref aggregate arrives
     * with tag T_GCREF, because ARRAY_GET/STRUCT_GET reconstruct it from a per-TYPE bit that
     * cannot distinguish the inhabitants of a union. Trusting it here dereferenced a scalar as a
     * gc_obj_t (jav_is_host_box → o->rtt) — a VM SIGSEGV on validated bytecode, the same defect as
     * the collector's, one layer up. Pinned by test_gc_refforms' "intact across GC" rows. */
    if (JAV_IS_I31(raw)) return HT_I31;              /* §4.2.1 ref.i31 — a value, not an address */
    if (tag == T_GCREF) {                            /* internal hierarchy: a managed aggregate / boxed extern */
        gc_obj_t* o = (gc_obj_t*)(uintptr_t)raw;
        if (jav_is_host_box(o)) return HT_ANY;       /* extern.convert_any value: top `any`, not eq */
        int32_t t = rtt_typeidx(vm, o->rtt);
        return t >= 0 ? t : (o->rtt->kind == GC_KIND_ARRAY ? HT_ARRAY : HT_STRUCT);
    }
    return HT_I31;                                   /* an unboxed T_REF scalar in the any family is an i31 */
}

/* §7.1.14 ref_type for the internal (GC/i31) hierarchy: the ABSTRACT runtime heaptype of a
 * non-null managed/i31 reference. Always abstract (never a concrete typeidx) — the wasm.h valtype
 * granularity is the abstract kind, which §7.1.14's Note explicitly permits as a less-precise
 * supertype. The c-api shim calls this for GC-ref values (funcref/externref it types directly). */
int32_t jav_ref_abstract_heaptype(vm_t* vm, u8 raw, u1 tag) {
    (void)vm;
    if (JAV_IS_I31(raw)) return HT_I31;   /* ask the VALUE, not the tag — see JAV_IS_I31 */
    if (tag == T_GCREF) {
        gc_obj_t* o = (gc_obj_t*)(uintptr_t)raw;
        if (jav_is_host_box(o)) return HT_ANY;                 /* extern.convert_any value: top `any` */
        return o->rtt->kind == GC_KIND_ARRAY ? HT_ARRAY : HT_STRUCT;
    }
    return HT_I31;                                             /* an unboxed scalar in the any family is an i31 */
}

/* §3.3 dynamic ref match on an EXPLICIT (raw, tag) ref — the §3.3-lattice query. A null ref matches iff
 * the target is nullable; else the value's runtime heaptype must be ≤ ht in the lattice. */
int jav_ref_matches(vm_t* vm, heap_t* h, s8 raw, s4 tag, s4 ht, s4 nullable) { (void)h;
    if (JAV_REF_ISNULL(raw)) return nullable;
    int hier = jav_ht_hierarchy(vm, ht);
    /* §4.5.2: a CROSS-INSTANCE funcref carries its concrete type in ITS defining instance's type
     * space — its local type_index is meaningless in OUR lattice, so a ht_sub against it is garbage.
     * Match structurally against our target functype, the SAME closed-type match link_imports and
     * call_indirect (jav_funcref_typematch) use. Without this, ref.cast of a funcref pulled from a
     * cross-module vtable slot (a jre method) always traps. */
    if (hier == 1 && ht >= 0) {
        const jav_func_t* fn = (const jav_func_t*)(uintptr_t)raw;
        if (!fn->inst_ctx || fn->inst_ctx->lattice != vm->frame.ctx->lattice)
            return funcref_closed_matches(vm, fn, ht);   /* cross-instance / host: closed-type subtyping */
    }
    /* §4.5.2: a CROSS-INSTANCE managed struct/array ref likewise carries its type in ITS defining
     * module's type space — its rtt is not in OUR struct_rtts, so value_heaptype below can only report
     * the abstract HT_STRUCT/HT_ARRAY and a concrete-target cast would wrongly fail. Match via the same
     * closed-type (global-id) relation as the funcref path above — the struct/array twin. */
    if (hier == 0 && ht >= 0 && tag == T_GCREF) {
        const gc_obj_t* o = (const gc_obj_t*)(uintptr_t)raw;
        if (!jav_is_host_box((gc_obj_t*)o) && rtt_typeidx(vm, o->rtt) < 0)   /* not our module's type */
            return gcref_closed_matches(vm, o, ht);
    }
    int32_t vht = value_heaptype(vm, (u8)raw, (u1)tag, hier);
    return jav_ht_sub(vm->frame.ctx->lattice, vht, ht);
}
/* peek-the-top wrapper: ref.test/ref.cast and br_on_cast/br_on_cast_fail and the c-api §7.1.15 arg check
 * all PEEK the operand ref (value + DYNAMIC tag) off the stack top. */
int jav_top_ref_matches(vm_t* vm, heap_t* h, s4 ht, s4 nullable) { (void)h;
    frame_t* f = &vm->frame;
    return jav_ref_matches(vm, vm->heap, (s8)f->stack[f->sp - 1].l, f->stack_types[f->sp - 1], ht, nullable);
}
/* ref.test/ref.test_null: §3.3 dynamic match on the popped ref (carried as an any_t so its runtime tag
 * survives the pop) → i32 boolean. ref.cast/ref.cast_null are the SAME query with a declared CastFailure
 * guard on it — the identity downcast the body re-pushes needs no second entry point here. */
int jav_ref_test(vm_t* vm, heap_t* h, any_t r, s4 ht, s4 nullable) {
    return jav_ref_matches(vm, h, r.bits, r.kind, ht, nullable);
}


/* ── the rest of struct.* / array.* (defaults, bulk, segments) — packed get_s/u now inline in the .def ───
 * struct.new_default (§4.6): alloc a struct with defaulted fields → managed gcref (opgen pushes it). No
 * operand crosses the alloc, so the pop/push are opgen's; on trap returns a GC-safe null scalar ref. */
any_t jav_struct_new_default(vm_t* vm, heap_t* h, s4 typ) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    gc_obj_t* o = jav_gc_new(vm, rtt, rtt->size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }   /* payload zeroed (numbers default 0) */
    for (u4 i = 0; i < rtt->nrefs; i++)                    /* every reference field defaults to null */
        *(s8*)((u1*)o + rtt->ref_offsets[i]) = (s8)(u4)JAV_NULLREF;
    return (any_t){ .bits = (s8)(uintptr_t)o, .kind = T_GCREF };
}


/* array.new_default (§4.6): alloc array[len] with defaulted elements → managed gcref. The only operand
 * (len) is a number popped by opgen — it doesn't cross the alloc as a root, so the hoist is GC-safe. */
any_t jav_array_new_default(vm_t* vm, heap_t* h, s4 typ, s4 len) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts || len < 0) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    u4 size = (u4)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (u4)len * jav_arr_stride(rtt);
    gc_obj_t* o = jav_gc_new(vm, rtt, size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    *(u4*)gc_obj_payload(o) = (u4)len;
    if (rtt->elem_is_ref) {                       /* scalars/v128 default 0 — the payload is zeroed */
        s8* e = arr_elems(o);
        for (s4 i = 0; i < len; i++) e[i] = (s8)(u4)JAV_NULLREF;
    }
    return (any_t){ .bits = (s8)(uintptr_t)o, .kind = T_GCREF };
}

/* array.new_fixed (§4.6) substrate primitives — same alloc-first-then-pop() hoist as struct.new (the
 * reduction is in the .def body). jav_obj_set_elem writes element i (8-byte .l view) of the fresh array. */
ref_t jav_array_alloc(vm_t* vm, heap_t* h, s4 typ, s4 n) { (void)h;   /* a REF — see jav_struct_alloc */
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts || n < 0) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    u4 size = (u4)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (u4)n * jav_arr_stride(rtt);
    gc_obj_t* o = jav_gc_new(vm, rtt, size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return 0; }
    *(u4*)gc_obj_payload(o) = (u4)n;
    return (ref_t)(uintptr_t)o;
}



/* read one little-endian numeric element of width w from a byte cursor. */
static s8 le_load(const u1* p, u1 w) {
    s8 v = 0; for (u1 b = 0; b < w; b++) v |= (s8)(u8)p[b] << (8 * b); return v;
}

/* array.new_data / new_elem: opgen pops (off, n) from the declared signature and
 * pushes the declared result — the native only builds the object. */
any_t jav_array_new_data(vm_t* vm, heap_t* h, s4 typ, s4 seg, s4 off, s4 n) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts || !vm->frame.ctx->data_segs || (u4)seg >= vm->frame.ctx->num_data_segs) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    u1 w = rtt->elem_store_w;                              // the RTT is the one authority for the data-segment stride (always set for an array)
    const jav_data_seg_t* d = &vm->frame.ctx->data_segs[seg];
    u8 dseglen = vm->frame.ctx->data_dropped[seg] ? 0 : d->len;      // §4.6.7: a dropped segment is ε (length 0)
    if (n < 0 || off < 0 || (u8)off + (u8)n * w > dseglen) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsMemoryAccess); return (any_t){ 0, T_REF }; }
    u4 size = (u4)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (u4)n * jav_arr_stride(rtt);
    gc_obj_t* o = jav_gc_new(vm, rtt, size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    *(u4*)gc_obj_payload(o) = (u4)n;
    u1* e = (u1*)arr_elems(o);
    for (s4 k = 0; k < n; k++) {
        u1* dst = e + (size_t)k * jav_arr_stride(rtt);
        if (w == 16) memcpy(dst, d->bytes + off + (size_t)k * 16, 16);  /* v128: the 16 lane bytes verbatim (§4.6) */
        else         *(s8*)dst = le_load(d->bytes + off + (size_t)k * w, w);
    }
    return (any_t){ .bits = (s8)(uintptr_t)o, .kind = T_GCREF };
}

any_t jav_array_new_elem(vm_t* vm, heap_t* h, s4 typ, s4 seg, s4 off, s4 n) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts || !vm->frame.ctx->elem_segs || (u4)seg >= vm->frame.ctx->num_elem_segs) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    const gc_rtt_t* rtt = vm->frame.ctx->struct_rtts[typ];
    const jav_elem_seg_t* el = &vm->frame.ctx->elem_segs[seg];
    u8 seglen = vm->frame.ctx->elem_dropped[seg] ? 0 : el->len;      // §4.6.7: a dropped elem segment is ε (length 0)
    if (n < 0 || off < 0 || (u8)off + (u8)n > seglen) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsTableAccess); return (any_t){ 0, T_REF }; }
    u4 size = (u4)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + (u4)n * GC_ARRAY_ELEM_BYTES;
    gc_obj_t* o = jav_gc_new(vm, rtt, size);
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return (any_t){ 0, T_REF }; }
    *(u4*)gc_obj_payload(o) = (u4)n;
    s8* e = arr_elems(o);
    for (s4 k = 0; k < n; k++) e[k] = el->values[off + k];
    return (any_t){ .bits = (s8)(uintptr_t)o, .kind = T_GCREF };
}

/* array.init_data / init_elem: opgen pops (arr, d, s, n) from the declared signature. */
void jav_array_init_data(vm_t* vm, heap_t* h, s4 typ, s4 seg, ref_t arr, s4 d, s4 s, s4 n) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts || !vm->frame.ctx->data_segs || (u4)seg >= vm->frame.ctx->num_data_segs) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return; }
    gc_obj_t* o = as_obj((s8)arr);
    const gc_rtt_t* artt = vm->frame.ctx->struct_rtts[typ];
    u1 w = artt->elem_store_w;                              // the RTT is the one authority for the data-segment stride
    const jav_data_seg_t* ds = &vm->frame.ctx->data_segs[seg];
    u8 seglen = vm->frame.ctx->data_dropped[seg] ? 0 : ds->len;      // §4.6.7: a dropped segment is ε (length 0)
    /* §7.10 gives array.init_data three distinct reasons; one combined bounds test
     * could only report one of them, so the three conditions stay separate — and in
     * THIS order, which is the reduction's (eval.ml ArrayInitData): null array, then
     * array_oob, then data_oob. Checking the segment first reports the memory reason
     * for an access that is out of bounds on both. */
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NullArrayReference); return; }
    if (n < 0 || d < 0 || (u8)d + (u8)n > arr_len(o)) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsArrayAccess); return; }
    if (s < 0 || (u8)s + (u8)n * w > seglen) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsMemoryAccess); return; }
    u1* e = (u1*)arr_elems(o);
    for (s4 k = 0; k < n; k++) {
        u1* dst = e + (size_t)(d + k) * jav_arr_stride(artt);
        if (w == 16) memcpy(dst, ds->bytes + s + (size_t)k * 16, 16);   /* v128: 16 lane bytes verbatim */
        else         *(s8*)dst = le_load(ds->bytes + s + (size_t)k * w, w);
    }
}

void jav_array_init_elem(vm_t* vm, heap_t* h, s4 typ, s4 seg, ref_t arr, s4 d, s4 s, s4 n) { (void)h; (void)typ;
    if (!vm->frame.ctx->elem_segs || (u4)seg >= vm->frame.ctx->num_elem_segs) { JAV_TRAP_WITH(vm, JAV_TRAP_NONE); return; }
    gc_obj_t* o = as_obj((s8)arr);
    const jav_elem_seg_t* el = &vm->frame.ctx->elem_segs[seg];
    u8 seglen = vm->frame.ctx->elem_dropped[seg] ? 0 : el->len;      // §4.6.7: a dropped elem segment is ε (length 0)
    /* Same three-way split and same reduction order as array.init_data, with the
     * elem segment's reason (eval.ml ArrayInitElem). */
    if (!o) { JAV_TRAP_WITH(vm, JAV_TRAP_NullArrayReference); return; }
    if (n < 0 || d < 0 || (u8)d + (u8)n > arr_len(o)) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsArrayAccess); return; }
    if (s < 0 || (u8)s + (u8)n > seglen) { JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsTableAccess); return; }
    s8* e = arr_elems(o);
    for (s4 k = 0; k < n; k++) e[d + k] = el->values[s + k];
}

/* ref.func: the funcref VALUE is the funcinst REFERENCE (§4.2.1 funcaddr) — a pointer to the
 * current instance's funcinst, which carries inst_ctx, so a funcref pulled into a shared table
 * and call_indirect'd by another instance still dispatches to THIS function with ITS context.
 * Consistent with every other ref being an unboxed pointer (Titzer §3.2.1); funcidx was the
 * lone anomaly. */
