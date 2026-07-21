/*
 * jav_frame.h — the WASM execution SUBSTRATE, supplied to the generated
 * runtime_api.h via -DJAVELINA_BACKEND_TYPES. The value model (slot_t,
 * jav_status_t, the T_* tags) is GENERATED above this header's include point
 * (emit_value_model), so this file is a fragment: only ever included by
 * runtime_api.h, after the value model exists.
 *
 * The in-place code cursor is a bbq_ctx_t — pos IS the pc — so every read
 * (opcode dispatch AND operand decode) goes through bbq_runtime's readers, the
 * SAME decoders bbqc emits for the module container. One decoder, used by the
 * container reader, the interpreter, and (later) the JIT.
 */
#ifndef JAV_FRAME_H
#define JAV_FRAME_H

#include <stddef.h>        /* offsetof for the frame-at-0 JIT ABI assert */
#include <math.h>          /* libm — the generated float intrinsics (sqrt/fmin/…) */
#include <sys/mman.h>      /* the mmap'd value/locals pool (jav_vm_init) */
#include "bbq_runtime.h"   /* bbq_ctx_t + bbq_read_* — the shared decoders */
#include "jav_subtype.h"   /* jav_subtype_ctx_t — the §3.3 lattice for runtime ref.test/cast */
#include "immix/jav_gc.h"  /* gc_obj_t + gc_obj_payload — the object-layout vocabulary the inline GC-access macros read through */
#include "jav_trap_reason.h" /* the §7.10 trap vocabulary, generated from spec/instructions.toml */

/* opgen's seam for a fired `error:` guard: the generated default discards the code,
 * we record it. Defined here because runtime_api.h includes this header before the
 * generated `#ifndef OPGEN_GUARD_TRAP` default is reached, so ours wins. The guard
 * tail-calls the trap continuation immediately after, which is what sets `trapped`. */
#define OPGEN_GUARD_TRAP(vm, err) \
    ((vm)->trap_reason = (u1)JAV_REASON_OF_OPGEN(err))

/* A substrate native raises a trap and names its cause. Parking the cursor at the
 * end is what unwinds it: the generated interpreter checks no status after a native
 * call, so the next dispatch has to hit EOF. (The JIT emits an explicit
 * `if (vm->trapped)` after each native instead — the two tiers differ here.)
 * JAV_TRAP_NONE is a positive claim that the spec defines no trap for this failure —
 * a host allocation failure or a validation invariant, not a §7.10 trap. */
#define JAV_TRAP_WITH(vm, reason) do {                      \
        (vm)->trapped = 1;                                  \
        (vm)->trap_reason = (u1)(reason);                   \
        (vm)->frame.code.pos = (vm)->frame.code.length;     \
    } while (0)

#define MAX_STACK  1024
#define MAX_LOCALS 1024

/* The null reference value (any ref type), and the ONLY one.
 *
 * §2.3.4 Note requires i31 to be pointer-tagged — "Engines need to perform some form of pointer
 * tagging to achieve this, which is why ONE BIT is reserved ... it cannot be wider than 32 bits
 * minus the tag bit". With one reserved bit every tagged i31 is ODD, so null must be EVEN. The
 * previous all-ones sentinel is odd and is exactly what u31 max encodes to, so it cannot coexist
 * with the spec's tagging and had to move.
 *
 * 0 was already half of the representation — a zero-initialised ref local was ALSO treated as
 * null (jav_call_fn zeroes locals) — so this collapses a pre-existing dual encoding rather than
 * introducing one. 0 is not a valid gc_obj_t or funcinst pointer, and a null table slot is now
 * the natural zero-init rather than a (s8)-1 sentinel. */
#define JAV_NULLREF 0u

/* A MANAGED heap reference (struct/array): an 8-byte pointer in slot.l, tagged
 * distinct from T_REF (= funcref/i31, a word-sized handle, not a heap pointer) so
 * the GC root scan follows only managed slots. Follows the generated T_* enum (last
 * = T_REF = 6). */
#define T_GCREF 7

/* Side-table entry (Titzer §3.1): the missing branch info distilled from the
 * single validation pass. On a taken branch: keep the top `vals` values, drop
 * `pop` below them, advance the code cursor by delta_ip and the side-table
 * pointer by delta_stp. delta_ip is relative to the cursor AFTER the branch's
 * operand (where the handler leaves it). */
/* The entry layout is opgen's (emit_value_model, above this include point) — it owns
 * the Titzer side-table format per the opgen↔VM contract §1, so the validator that
 * FILLS the table and the generated walk that reads it share one definition. */
typedef opgen_st_entry_t jav_st_entry_t;

/* Per-try_table runtime metadata (validator-filled): keyed by the try_table's pc, it
 * gives the base side-table index of the catch entries (catch i transfers via
 * catch_stp + i) and the matching-end position (handler exits once pc passes it). */
typedef struct { u4 try_pc; u4 catch_stp; u4 ncatch; u4 end_pc; } jav_try_t;

/* A frame is LIGHTWEIGHT: its operand stack and locals are base pointers into the
 * vm's single shared value/locals stacks, not inline arrays. So a saved frame
 * (call/recursion) is ~80 bytes, not ~34KB — recursion goes deep, bounded by the
 * shared stack + the call-depth guard, not the C stack. The generated handlers /
 * stencils are unchanged: they already do f->stack[f->sp] / f->locals[index],
 * which a pointer satisfies identically. */
/* The current module instance's execution context (§4.2.2): the per-instance facets every
 * resolution reads (funcs/globals/tables/mem_addrs/types/…). An ENGINE-owned type the loader
 * fills in (like jav_func_t) — so a funcinst can name its defining instance WITHOUT the engine
 * depending on the loader's jav_instance_t. Defined below (after its field types). */
typedef struct instctx instctx_t;

typedef struct {
    bbq_ctx_t code;                     /* in-place code cursor; code.pos IS the pc */
    slot_t*   stack;                    /* this frame's operand stack (into vm->value_stack) */
    u1*       stack_types;
    u4        sp;
    slot_t*   locals;                   /* params first, then declared (into vm->locals_store) */
    u1*       local_types;
    u4        num_locals;
    const jav_st_entry_t* sidetable;   /* the function's side-table (validator-built) */
    u4        stp;                       /* current side-table index */
    const jav_try_t* trytable;         /* the function's try_table metadata (handler install, by pc) */
    u4        ntry;
    u4        instr_pc;                 /* start pc of the executing instruction (jav_next stamps it before decode);
                                          * the trap trace reads it for the §7.1.8 frame byte-offsets */
    const instctx_t* ctx;              /* §4.2.2 the instance this frame executes in (NULL = a host frame) */
} frame_t;

/* The vm's single shared value + locals stacks live in ONE mmap'd, lazily-committed
 * pool (jav_vm_init), not inline arrays — so vm_t is tiny and the pool reserves a
 * large virtual range that costs nothing until touched (frames carve base regions
 * out of it). The base never moves, so frame base-pointers stay valid (a realloc-
 * move would dangle them — that's why this isn't grow-by-doubling like jit_codebuf). */
#define POOL_SLOTS (1u << 20)   /* 1M slots per stack; reserved virtual, committed on touch */
/* Default WASM call-depth ceiling → a clean stack-exhaustion trap. Now bounded by
 * the shared stacks, not the C stack (lightweight frames), so it can be deep. The
 * embedder can dial it DOWN at runtime via vm->max_call_depth (e.g. to sandbox
 * untrusted code); 0 means "use this default". MAX_STACK/MAX_LOCALS are the
 * per-frame caps the validator enforces and jav_call reserves. With the heap pool
 * the bound is the C stack (each call still saves a frame_t), so it's deeper now. */
#define MAX_CALL_DEPTH 4096

typedef struct heap_t heap_t;   /* OPAQUE: the vm holds a handle; only the backend (behind runtime_api) inspects it */
struct gc_obj;                  /* GC object (jav_gc.h); forward-declared for the root-visit hook */
typedef void (*jav_root_visit_fn)(struct gc_obj** slot, void* ctx);  /* == gc_root_visit_fn */


typedef struct vm_s vm_t;   /* fwd: the invoke seam below names it */

/* Passive segments an array can be initialised from (array.new_data/new_elem and
 * the init_* forms). A data segment is raw bytes (read as packed numeric elements);
 * an element segment is a vector of reference values (funcidx / managed ref bits). */
typedef struct jav_data_seg { const u1* bytes;  u4 len; } jav_data_seg_t;
/* §4.2.12 eleminst ::= {type elemtype, refs ref*}. `types` is the parallel RUNTIME tag per value
 * (T_REF funcref/null, T_GCREF managed aggregate): elem items are const exprs, which in 3.0
 * include GC allocations, so a segment can park live heap objects — the tag is what lets the root
 * scan trace them and table.init write an honestly-tagged slot. */
typedef struct jav_elem_seg { const s8* values; const u1* types; u4 len; } jav_elem_seg_t;

/* A table (§4.5.3): a vector of reference values, indexed by tableidx — the table analogue
 * of jav_mem_t/heap->mems (one model, not a single funcref `table0`). Entries are SLOT-SIZED
 * (8 bytes) so a table can hold ANY reftype, with a PARALLEL per-entry tag array (just like
 * the value stack) since an `externref` may carry a scalar (funcidx/i31/null, T_REF) OR a
 * managed gc_obj pointer (T_GCREF) — tracing is per-entry. T_REF entries hold a scalar handle
 * (funcidx, −1 = null); T_GCREF entries hold an 8-byte gc_obj pointer the GC scans. Both
 * `refs` and `types` are bbq_vecs of the same length (the current size; grows together). */
typedef struct jav_tableinst {
    s8* refs;      /* entries: scalar handle (T_REF) OR gc_obj ptr (T_GCREF); a bbq_vec */
    u1* types;     /* parallel bbq_vec: per-entry runtime tag (T_REF / T_GCREF) for the GC scan */
    u4  max;       /* maximum entries — meaningful only when has_max */
    u1  has_max;   /* 1 ⇒ the tabletype declared a max; 0 ⇒ unbounded (grow caps at the §3.2.16 addrtype ceiling) */
    u1  reftype;   /* element reference type (a WVT_ tag) */
    s4  reftype_ht;/* element heaptype (HT_* code or typeidx) — the generic (ref null? ht) */
    u1  is64;      /* table64 addrtype (the index width) */
} jav_tableinst_t;

/* §7.1.9 table object operations — the ONE bounds-checked raw-entry authority, shared by the table.*
 * opcodes (which surface OOB as a trap + marshal the value model) and the wasm-c-api (which adds ref
 * boxing). Return 0 on out-of-bounds (i >= current size), 1 on success. (Size is bbq_vec_len(t->refs).) */
int jav_tableinst_read (const jav_tableinst_t* t, u8 i, s8* raw, u1* tag);
int jav_tableinst_write(jav_tableinst_t* t, u8 i, s8 raw, u1 tag);
/* §7.1.9 table_grow: append `delta` copies of (raw,tag), capped at the declared max (else the §3.2.16
 * addrtype ceiling). Returns the old size, or -1 if the grow would exceed the cap. */
s8  jav_tableinst_grow(jav_tableinst_t* t, u8 delta, s8 raw, u1 tag);

/* Exception support. A try_table installs a handler: the catch vector (in the code) maps tags (or
 * "all") to labels, and the validator emitted one side-table entry per catch, so a caught exception
 * branches via the side-table (catch_stp + matched index) exactly like a `br`. The active handlers
 * form a per-vm stack (vm->handlers, a bbq_vec — no fixed cap). */
/* An exception instance is a MANAGED GC object (§4.2.12): payload `{ u4 tag; u4 nfields; slot_t
 * fields[nfields] }`, described by a per-tag rtt (built lazily at throw, cached on the heap by
 * tagaddr). An `exnref` is therefore an ordinary T_GCREF gc_obj pointer — traced like any managed
 * ref, so the store has no cap and dead exceptions are reclaimed by liveness. The object accessor
 * lives in jav_runtime.c (it owns the GC layout). */
struct gc_obj;   /* jav_gc.h — the managed-object header; exnref / pending_exn point at one */
typedef struct {
    u4        call_depth;   /* the frame (call depth) this handler belongs to */
    u4        sp;           /* operand-stack height at the try base (restore point on catch) */
    u4        catch_stp;    /* base side-table index of this try_table's per-catch entries */
    u4        try_pc;       /* the body start — the do_transfer base for a caught exception's branch */
    u4        end_pc;       /* the matching `end` — once pc passes it the handler is stale (lazy prune) */
    const u1* catches;      /* the catch vector in the code stream (re-parsed to match the thrown tag) */
    u4        ncatch;
} jav_handler_t;

/* A module function the runtime can `call`. A WASM function carries its
 * instruction stream (the body AFTER its local-decl vector — same form
 * jav_typecheck + the frame cursor use), param/local/result counts, and side-
 * table. `invoke` is the ONE dispatch seam — jav_call runs every entry through it
 * and learns none of the kind: NULL → interpret the frame; a host import → a C thunk
 * (`jav_status_t(vm,h,ctx)`) that reads params from `frame.locals`, leaves its results at
 * `frame.stack[0..n)` with `frame.sp = n` — the SAME place a wasm callee leaves them, for
 * every arity — and returns JAV_RETURN; else a JIT
 * compiled handle (invoke_ctx its context). The caller only ever names a function
 * INDEX. (Opportunistic JIT is just a pointer-swap of `invoke` between calls.) */
struct jav_functype;       /* validate.h: a module function type (param/result valtypes) */

typedef struct {
    const u1* code;
    size_t    code_len;
    u4        num_params;
    u4        num_locals;   /* declared locals beyond the params, zero-initialized */
    u4        num_results;  /* 0 or 1 for core WASM (multi-value needs the type section) */
    u4        type_index;   /* this function's type (for call_indirect's dynamic type-match trap) */
    const jav_st_entry_t* sidetable;
    const jav_try_t* trytable;  /* the function's try_table metadata (NULL if no try ops) */
    u4           ntry;
    jav_status_t (*invoke)(vm_t* vm, heap_t* h, void* ctx);   /* the single dispatch: NULL → interp; a host C thunk; else JIT */
    void*        invoke_ctx;
    const instctx_t* inst_ctx;   /* §4.2.6 this funcinst's DEFINING instance context; a call switches the vm to it. NULL = host */
    const struct jav_functype* sig;   /* §4.5.2 this funcinst's functype — set for EVERY funcinst (instance + host), so
                                       * call_indirect's §4.6.2 dynamic type check and funcref reflection read it uniformly */
} jav_func_t;

/* §4.2 the per-instance execution context. The loader builds one per instance and points each
 * funcinst's inst_ctx at it; jav_call_fn sets the callee frame's `ctx` to it on a context switch.
 * §8: this IS the engine's per-instance state — every interp cluster read goes through `frame.ctx`,
 * there is no separate flat vm cache to keep in sync. */
struct instctx {
    const jav_func_t* functions;          u4 num_functions;
    slot_t**          globals;            u1* global_types;   /* §4.2.5 globalinst by REFERENCE: globals[i] aliases the
                                                              * defining instance's slot, so a mutable imported global is shared */
    jav_tableinst_t*  tables;
    const struct jav_functype* types;     u4 num_types;
    const struct gc_rtt* const* struct_rtts;  u4 num_struct_rtts;
    const u1* const*  type_field_packs;   /* typeidx → per-field/elem storage width (0/1/2); NULL row = unpacked */
    u4                num_type_field_packs;
    const jav_subtype_ctx_t*   lattice;
    const s4*         gcanon;             /* §4.5.2 module typeidx → session-global canonical id (cross-module import match) */
    const u4*         mem_addrs;          u4 num_mems;
    const struct jav_functype* tags;      u4 num_tags;
    const jav_data_seg_t* data_segs;      u4 num_data_segs;  u1* data_dropped;
    const jav_elem_seg_t* elem_segs;      u4 num_elem_segs;  u1* elem_dropped;
    const u4*         tag_ids;            /* §4.2 tagidx → store tagaddr identity (throw/catch match by identity) */
};


/* The machine state — the single pointer every handler/stencil receives. The
 * heap is an opaque handle ON the vm (not a threaded parameter) so the interp
 * and the JIT stencil share ONE ABI: both are `void f(vm_t*)`. A handler that
 * calls a runtime native passes `vm->heap` through; it never dereferences it.
 * (Operand decode goes straight through bbq_runtime's readers over frame.code —
 * the SAME decoders bbqc emits for the container — generated by opgen, so there
 * are no fetch wrappers here.) */
struct vm_s {
    frame_t frame;                      /* MUST stay first (copy-and-patch JIT ABI) */
    heap_t* heap;                       /* opaque: linear memory + the GC seam, behind runtime_api */
    u1      trapped;                     /* set by the trap continuation; bails through the machine */
    u1      trap_reason;                 /* jav_trap_reason_t: WHY it trapped. Set by a declared `error:`
                                          * guard through OPGEN_GUARD_TRAP, or named directly by a
                                          * substrate native. JAV_TRAP_NONE (0) = cause not yet carried;
                                          * the vocabulary is generated from instructions.toml. */
    const void* dispatch;               /* the interp's per-vm dispatch table (Titzer's DISPATCH register);
                                         * void* avoids coupling this header to gen_interp.h's handler type */
    void (*probe)(struct vm_s*, u1 op); /* §3.3.3 instrumentation seam: if set, called by jav_next BEFORE
                                         * each interp opcode with the op about to run (NULL = no probe).
                                         * interp-tier only (the JIT bakes its successor); a future two-table
                                         * design can move the per-op branch off the main path without an API change. */
    /* §4.2 / the §8 flat-cache collapse — the per-activation instance context. The ACTIVE context is
     * `vm->frame.ctx` (set to `&cluster` by jav_vm_init for the bare-VM/default path, or to
     * `&inst->ctx` by the loader). EVERY cluster read (globals, functions, tables, types, rtts,
     * mem_addrs, tags, segments, lattice …) goes through `frame.ctx`, so a re-entrant call — which
     * has its OWN frame and thus its own ctx, saved/restored with the frame by jav_call_fn — can no
     * longer corrupt the caller's context (the A3 fix; no flat cache to clobber). `cluster` is the
     * embedded default a bare-VM test populates; the loader points `frame.ctx` at the instance's own. */
    instctx_t cluster;
    jav_handler_t* handlers;            /* active try_table handler stack — a bbq_vec (length = count, no cap) */
    struct gc_obj* pending_exn;          /* the exn object being unwound when `unwinding` is set (a GC root then) */
    u1      unwinding;                   /* a throw escaped the current frame -> propagate to caller */
    /* return_call (tail call): the native records the target and ends the run; the args
     * stay on the operand stack. jav_call's loop then rebuilds the callee IN the same
     * frame base (frame reuse — no stack growth) and re-runs it, copying those stack
     * args into the reused frame's locals exactly as a normal call does. */
    u1      tail_pending;
    u4      tail_func;                   /* the callee function index (when tail_fn is NULL: a same-instance return_call) */
    const jav_func_t* tail_fn;           /* a return_call to a funcinst by REFERENCE (return_call_indirect/_ref); NULL ⇒ use tail_func */
    u4      call_depth;                 /* current nested-call depth (stack-exhaustion guard) */
    u4      max_call_depth;             /* tunable ceiling; 0 → MAX_CALL_DEPTH (embedder may dial down) */
    void*   pool;                       /* the one mmap backing all four regions (jav_vm_init) */
    size_t  pool_bytes;
    slot_t* value_stack;                /* partitioned views into pool; the one operand stack */
    u1*     value_types;
    slot_t* locals_store;               /* the one locals stack */
    u1*     local_type_store;
    /* Optional embedder extra GC roots: if set, jav_gc_enum_roots invokes this so roots the
     * engine can't see — e.g. the c-api's host-created globals/tables holding managed refs —
     * are scanned too. `visit` is a gc_root_visit_fn (jav_gc.h); kept opaque here. */
    void  (*extra_roots)(void* ctx, jav_root_visit_fn visit, void* visit_ctx);
    void*   extra_roots_ctx;
    /* §4.7.2 step 24 (allocmodule): jav_instantiate calls this once the instance is fully allocated —
     * every funcinst/global/table built, BEFORE active element/data segments + start run (steps 27-29).
     * A store owner appends the instance to its live set here so it is rooted as store state during any
     * segment/start GC (the spec puts the instance in the store at allocation, not after instantiation
     * returns). NULL = no owner: the bare-VM path relies on the bound-instance scan. `inst` is a
     * jav_instance_t* (kept void* to avoid coupling jav_frame.h to the instance type). */
    void  (*on_inst_alloc)(void* ctx, void* inst);
    void*   on_inst_alloc_ctx;
    /* Trap stack trace: a bbq_vec of func indices, innermost-first, that jav_call appends to
     * as it unwinds a trap (the embedder reads it via the c-api's wasm_trap_origin/trace).
     * The caller clears it before a fresh top-level invocation. trap_pcs is the PARALLEL vec of
     * each frame's byte offset (innermost = the trapping instruction; outer = its inward call). */
    u4*     trap_trace;
    u4*     trap_pcs;
    /* Tier selection (an embedder option, set from wasm_config_t before any instantiation): when
     * set, jav_instantiate places every DEFINED function of each instance on the copy-and-patch JIT
     * tier — jit_compile once at allocation, `invoke` swapped to jit_invoke. Callers stay oblivious
     * (the ONE invoke seam). A function whose body the JIT declines (a `flag:no_jit` opcode) keeps
     * its NULL invoke and runs interpreted, so a mixed module still runs. `jit_compiled` counts the
     * funcinsts actually placed on the tier — the embedder's readout that the choice took effect. */
    u1      jit_enabled;
    u4      jit_compiled;
};
_Static_assert(offsetof(vm_t, frame) == 0, "frame must be at offset 0 (copy-and-patch JIT ABI)");

/* Bind the root frame to the base of the shared stacks (the top-level entry isn't
 * reached via jav_call, which binds callee frames itself). */
static inline void jav_bind_root_frame(vm_t* vm) {
    vm->frame.stack       = vm->value_stack;
    vm->frame.stack_types = vm->value_types;
    vm->frame.locals      = vm->locals_store;
    vm->frame.local_types = vm->local_type_store;
}

/* Allocate the value/locals pool (one mmap, lazily committed) and bind the root
 * frame. Call after zeroing the vm; pair with jav_vm_free. Returns 0, or -1 if
 * the reservation fails. */
static inline int jav_vm_init(vm_t* vm) {
    size_t vs = (size_t)POOL_SLOTS * sizeof(slot_t), vt = (size_t)POOL_SLOTS;
    vm->pool_bytes = 2 * vs + 2 * vt;
    vm->pool = mmap(NULL, vm->pool_bytes, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (vm->pool == MAP_FAILED) { vm->pool = NULL; return -1; }
    char* p = (char*)vm->pool;
    vm->value_stack      = (slot_t*)p; p += vs;
    vm->value_types      = (u1*)p;     p += vt;
    vm->locals_store     = (slot_t*)p; p += vs;
    vm->local_type_store = (u1*)p;
    jav_bind_root_frame(vm);
    vm->frame.ctx = &vm->cluster;   /* default execution context (bare-VM); the loader overrides to &inst->ctx */
    return 0;
}

static inline void jav_vm_free(vm_t* vm) {
    if (vm->pool) munmap(vm->pool, vm->pool_bytes);
    vm->pool = NULL;
    bbq_vec_free(vm->trap_trace); vm->trap_trace = NULL;
    bbq_vec_free(vm->trap_pcs);   vm->trap_pcs   = NULL;
    bbq_vec_free(vm->handlers);   vm->handlers   = NULL;   /* the try_table handler stack (no fixed cap) */
}

/* §4.4.8 side-table walk (the control authority): move the `vals` result values down past `pop` dropped
 * slots, then advance code.pos by delta_ip / stp by delta_stp to the label target. A static inline so the
 * generated branch handler+stencil (via TRANSFER(), inlined — NO extern) AND the return/throw natives
 * share ONE walk. */
/* The walk itself is generated (opgen_do_transfer, emitted from the same entry format
 * opgen defines); this backend supplies only where the cursors live, via the
 * OPGEN_ST_TABLE/OPGEN_ST_PTR/OPGEN_IP defaults, which match frame_t as-is. */
/* The depth-0 "what did this produce" read: the top of the current frame's operand
 * stack. DERIVED, not stored — a second copy of the result is what let a return
 * protocol and a debug convenience be mistaken for each other. */
static inline slot_t jav_tos(const vm_t* vm) {
    return vm->frame.sp ? vm->frame.stack[vm->frame.sp - 1] : (slot_t){ .l = 0 };
}
static inline u1 jav_tos_type(const vm_t* vm) {
    return vm->frame.sp ? vm->frame.stack_types[vm->frame.sp - 1] : (u1)T_VOID;
}

#define jav_do_transfer(vm)  opgen_do_transfer(&(vm)->frame)
#define TRANSFER()           jav_do_transfer(vm)

/* §7.1.8 trap-frame offset: each stencil bakes its own source byte offset into instr_pc (the interp
 * stamps it via jav_next). OVERRIDES opgen's STENCIL_TRAP_PC default, which discards the offset (a
 * backend that records no trap site emits no _HOLE_pc hole). `f` is the stencil's frame_t*. */
#define STENCIL_TRAP_PC(pc)  (f->instr_pc = (u4)(pc))

/* §8: the generated interp reads/writes globals THROUGH the active instance context (vm->frame.ctx),
 * not a flat vm cache — these OVERRIDE opgen's `#ifndef`-guarded GLOBAL_* defaults (the backend header
 * is included before the generated macros), so a re-entrant call cannot corrupt the caller's globals. */
#define GLOBAL_GET(i)        (vm->frame.ctx->globals[(i)][0])
#define GLOBAL_SET(i, v)     (vm->frame.ctx->globals[(i)][0] = (v))
#define GLOBAL_TAG_SET(i, t) (vm->frame.ctx->global_types[(i)] = (t))
/* §7.1.9 tableinst access through the active instance context (§8), OVERRIDING opgen's TABLE_* defaults.
 * The element entry + its parallel runtime tag ride an any_t; the grow/realloc boundary stays a native
 * (jav_tableinst_grow) — so table.get/set inline in the stencil with no extern. */
#define TABLE_LEN(t)        ((u8)bbq_vec_len(vm->frame.ctx->tables[(t)].refs))
/* §7.1.9 the ONE table-entry storage access, over a tableinst. Both callers go through it: the
 * opcodes via TABLE_GET/SET (by table index, bounds already proved by their declared guard) and
 * the c-api via jav_tableinst_read/write (which add the bounds check the embedder has not had
 * validated). One authority for the (refs, types) pair — the layout is stated once. */
#define TABLEINST_GET(ti, i)     ((any_t){ .bits = (ti)->refs[(i)], .kind = (ti)->types[(i)] })
#define TABLEINST_SET(ti, i, v)  do { (ti)->refs[(i)] = (v).bits; (ti)->types[(i)] = (v).kind; } while (0)
#define TABLE_GET(t, i)     TABLEINST_GET(&vm->frame.ctx->tables[(t)], (i))
#define TABLE_SET(t, i, v)  TABLEINST_SET(&vm->frame.ctx->tables[(t)], (i), (v))
#define TABLE_IS64(t)       (vm->frame.ctx->tables[(t)].is64)   /* table64 addrtype: the size result rides i64, else i32 */
/* GC array length (§4.6): the u4 element count at the start of the object payload (the layout authority is
 * jav_gc.h). The caller guards null; a non-null gc ref's bits ARE the gc_obj pointer. */
/* A managed ref is null if it is the JAV_NULLREF sentinel (explicit ref.null) OR 0 (a zero-initialised
 * defaultable ref local — jav_call_fn zeroes locals); both map to a null object, mirroring as_obj. The
 * GC-access macros below require a non-null ref. */
/* Full-width, deliberately: the old form compared only the low 32 bits, which with null == 0
 * would read any pointer whose low word happens to be zero as null. */
#define JAV_REF_ISNULL(r)   (((u8)(r)) == 0)
/* §4.2.1 `ref.i31 u31` is a VALUE, not an address, yet it inhabits anyref/eqref beside
 * `ref.struct structaddr` / `ref.array arrayaddr` which ARE addresses. So the value is
 * SELF-IDENTIFYING: tagged (v << 3) | 1 by ref.i31 (spec/wasm.def). Every gc_obj_t is
 * IMX_OBJECT_ALIGN(8)-aligned, so an odd slot can never be a heap pointer, and null is 0 (even),
 * so a tagged i31 collides with neither. §2.3.4 fixes the reservation at ONE bit — "which is why
 * one bit is reserved ... cannot be wider than 32 bits minus the tag bit" — so the tag is bit 0
 * and the payload is bits 1..31, NOT a wider ad-hoc tag.
 *
 * THIS is the authority for "is this slot a scalar rather than a pointer". Ask the VALUE here,
 * never a parallel tag: a tag stored apart from the value is lost by anything that copies the
 * value without it, which is exactly how ARRAY_SET/ARRAY_GET (which drop .kind on store and
 * rebuild it from a per-TYPE bit) handed scalars to the collector and to value_heaptype to be
 * dereferenced. A union element type — anyref, eqref — has no correct per-type bit. */
#define JAV_IS_I31(r)       (((u8)(r) & 1u) != 0)
#define ARRAY_LEN(a)        ((s4)*(u4*)gc_obj_payload((gc_obj_t*)(uintptr_t)(a)))
/* GC array element access (§4.6): elements are 8-byte slots after GC_ARRAY_ELEMS_OFFSET; the per-array
 * RTT says whether elements are managed refs (the runtime value-tag). Caller guards null + bounds. */
#define ARR_ELEMS(o)            ((s8*)((u1*)gc_obj_payload((gc_obj_t*)(uintptr_t)(o)) + GC_ARRAY_ELEMS_OFFSET))
#define ARRAY_GET(ty, o, i)     ((any_t){ .bits = ARR_ELEMS(o)[(i)], .kind = (u1)(vm->frame.ctx->struct_rtts[(ty)]->elem_is_ref ? T_GCREF : T_INT) })
#define ARRAY_SET(ty, o, i, v)  (ARR_ELEMS(o)[(i)] = (v).bits)
/* GC struct field access (§4.6): field `f` is the f-th 8-byte payload slot; the per-field ref-ness
 * (the runtime value-tag) comes from the RTT. Caller guards null. */
#define STRUCT_FIELDS(o)        ((s8*)gc_obj_payload((gc_obj_t*)(uintptr_t)(o)))
#define STRUCT_GET(ty, f, o)    ((any_t){ .bits = STRUCT_FIELDS(o)[(f)], .kind = (u1)(gc_field_is_ref(vm->frame.ctx->struct_rtts[(ty)], (f)) ? T_GCREF : T_INT) })
#define STRUCT_SET(ty, f, o, v) (STRUCT_FIELDS(o)[(f)] = (v).bits)
/* §4.2.12 exception-instance layout — here, with the other object layouts, because `throw`'s body
 * writes fields through it exactly as struct.new writes through STRUCT_SET. The field VALUES are
 * the payload's slot array; their runtime TAGS are the u1 row immediately after them, so the GC
 * scan can tell a managed field from a scalar. One authority: jav_runtime.c reads it from here. */
typedef struct { u4 tag; u4 nfields; slot_t fields[]; } exn_obj_t;
#define EXN_OBJ(o)       ((exn_obj_t*)gc_obj_payload((gc_obj_t*)(uintptr_t)(o)))
#define EXN_FTYPES(o)    ((u1*)&EXN_OBJ(o)->fields[EXN_OBJ(o)->nfields])
#define EXN_SET(o, i, v) do { EXN_OBJ(o)->fields[(i)].l = (v).bits; EXN_FTYPES(o)[(i)] = (v).kind; } while (0)
/* Packed struct field / array element read (§4.6 *.get_s/u): sign/zero-extend from the packed STORAGE
 * width. Struct widths are a per-field row in the instance context; array width is the RTT's elem_store_w. */
#define STRUCT_PACK_W(ty, f)             ((u1)((vm->frame.ctx->type_field_packs && (u4)(ty) < vm->frame.ctx->num_type_field_packs && vm->frame.ctx->type_field_packs[(ty)]) ? vm->frame.ctx->type_field_packs[(ty)][(f)] : 0))
#define STRUCT_GET_PACKED(ty, f, o, sgn) pack_extend((s4)STRUCT_FIELDS(o)[(f)], STRUCT_PACK_W(ty, f), (sgn))
#define ARRAY_GET_PACKED(ty, o, i, sgn)  pack_extend((s4)ARR_ELEMS(o)[(i)], vm->frame.ctx->struct_rtts[(ty)]->elem_store_w, (sgn))

/* ── Domain natives, folded inline ────────────────────────────────────────────────────────────────
 * opgen carries NO hardcoded knowledge of these names: the spec declares each as an `inline native`,
 * and the generic lowering emits a direct `name(NATIVE_ARGS, …)` call (NATIVE_ARGS = vm, vm->heap; no
 * _HOLE_ patch, no extern). Each folds into the uppercase access macro above. The single leading `ctx`
 * param swallows the UNEXPANDED `NATIVE_ARGS` token (a function-like macro counts args before macro
 * expansion, so `vm, vm->heap` is ONE arg here); the body uses `vm` directly, in scope at the handler/
 * stencil. The control transfers are `inline native status`, so the generic control path bakes _HOLE_ip,
 * calls, resyncs; keeping these as macros defers JV_NPUSH_ADDR resolution to the (post-include) use site. */
#define table_len(ctx, t)                       TABLE_LEN(t)
#define table_is64(ctx, t)                      TABLE_IS64(t)
#define table_get(ctx, t, i)                    TABLE_GET((t), (i))
#define table_set(ctx, t, i, val)               TABLE_SET((t), (i), (val))
#define ref_isnull(ctx, r)                      JAV_REF_ISNULL(r)
#define array_length(ctx, a)                    ARRAY_LEN(a)
#define array_load(ctx, ty, o, i)               ARRAY_GET((ty), (o), (i))
#define array_store(ctx, ty, o, i, val)         ARRAY_SET((ty), (o), (i), (val))
#define struct_load(ctx, ty, f, o)              STRUCT_GET((ty), (f), (o))
#define struct_store(ctx, ty, f, o, val)        STRUCT_SET((ty), (f), (o), (val))
#define exn_store(ctx, i, o, val)               EXN_SET((o), (i), (val))
#define struct_load_packed(ctx, ty, f, o, sgn)  STRUCT_GET_PACKED((ty), (f), (o), (sgn))
#define array_load_packed(ctx, ty, o, i, sgn)   ARRAY_GET_PACKED((ty), (o), (i), (sgn))
#define push_addr(ctx, val, is64)               JV_NPUSH_ADDR(&vm->frame, (val), (is64))
#define transfer(ctx)                           jav_do_transfer(vm)
#define func_return(ctx)                        (vm->frame.code.pos = vm->frame.code.length)

/* Small VM-internal accessors — INLINE natives (folded into the handler/stencil, no extern). */
static inline int   jav_is_null(vm_t* vm, heap_t* h, any_t r) { (void)vm; (void)h; return JAV_REF_ISNULL(r.bits); }  /* both null reps: JAV_NULLREF sentinel AND bare-0 zero-init (call_ref/call_indirect null-guard + ref.is_null) */
static inline any_t jav_funcaddr(vm_t* vm, heap_t* h, s4 func) { (void)h; return (any_t){ .bits = (s8)(uintptr_t)&vm->frame.ctx->functions[func], .kind = T_REF }; }  /* §4.6.2 ref.func → funcref */
static inline s8    jav_ref_func(vm_t* vm, heap_t* h, s4 func) { (void)h; return (s8)(uintptr_t)&vm->frame.ctx->functions[func]; }
/* §3(b) the callee's arity, so `call`'s signature can DECLARE its stack effect instead
 * of lying with `( -- )` and letting the frame machinery move sp behind opgen's back. */
static inline s4 jav_func_nparams (vm_t* vm, heap_t* h, s4 func) { (void)h; return (s4)vm->frame.ctx->functions[func].num_params;  }
static inline s4 jav_func_nresults(vm_t* vm, heap_t* h, s4 func) { (void)h; return (s4)vm->frame.ctx->functions[func].num_results; }
/* The type-immediate form (jav_type_nparams, call_indirect / call_ref) canNOT be an inline here:
 * jav_functype_t is validate.h's, and frame.ctx->types is deliberately an incomplete type at this
 * layer. It lives in jav_runtime.c instead. */
/* Same, off a funcref — where the arity must come from the funcinst itself. */
static inline s4 jav_ref_nresults(vm_t* vm, heap_t* h, any_t r) { (void)vm; (void)h;
    const jav_func_t* fn = (const jav_func_t*)(uintptr_t)r.bits;
    return fn ? (s4)fn->num_results : 0;
}

/* §4.6.8 linear-memory access inlines (mem_load/store_* + mem_at/mem_ok). Included LAST — after vm_t/frame_t
 * are complete — so the generated handlers/stencils fold the byte access in. Needs the full heap_t; the heap
 * still owns the storage layer (alloc + memory.grow). */
#include "jav_mem.h"

#endif /* JAV_FRAME_H */
