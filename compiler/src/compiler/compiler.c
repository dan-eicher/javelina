/* compiler.c — DDCG-driven AST → SIR compiler.
 *
 * Walks classes/methods, sets up a per-method ddcg_ctx_t, and calls
 * the ddcgc-emitted dispatcher (ddcg_compile_stmt) on each method
 * body. Returns sir_method_t** plus per-method try-region metadata
 * stored on compiler_ctx_t for compiler_get_try_regions.
 *
 * Rule bodies live in grammar/compiler.ddcg; AUX bodies (sema queries,
 * temp/label/scope helpers) live in compiler_helpers.c. */

#include "javelina/compiler/compiler.h"
#include "javelina/compiler/compiler_runtime.h"
#include "bbq_hmap.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"
#include "javelina/compiler/sir_support.h"
#include "gen/compiler_compile.h"
#include "gen/sir_ast.h"
#include "bbq_vec.h"

#include <string.h>

/* JLS type → SIR datatype — the one map lives in the type lattice. */
static inline sir_datatype_t jt_data_type(java_type_t t) {
    return lat_tag_to_dt(t.tag);
}

static inline void** jpush(bbq_arena* a, void** old, int* count, void* item) {
    int n = *count;
    void** nw = (void**)bbq_arena_alloc(a, (size_t)(n + 1) * sizeof(void*));
    if (old && n > 0) memcpy(nw, old, (size_t)n * sizeof(void*));
    nw[n] = item;
    *count = n + 1;
    return nw;
}

static const sema_field_t* find_instance_field(const sema_class_t* cls, const char* name) {
    for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++)
        if (!(cls->fields[i].modifiers & ACC_STATIC) &&
            strcmp(cls->fields[i].name, name) == 0)
            return &cls->fields[i];
    return NULL;
}

/* JLS §12.5 step 4: this class's OWN instance variable initializers, in textual
 * (left-to-right) source order, lowered to `this.f = (T)<init>` (put_field on
 * `this` = local 0, §5.2 widening via cg_deliver_conv). Built tail-first onto
 * `cont`. Inherited fields are initialized by the superclass <init> through the
 * super() call (steps 2-3), so only this class's declared fields appear here.
 * (Java 1.0 instance initializer blocks would interleave here too, but the AST
 * has no AST_INSTANCEINIT — only static blocks exist.) */
static sir_node_t* synth_instance_prologue(ddcg_ctx_t* yctx, int class_id, sir_node_t* cont) {
    const sema_class_t* cls = sema_get_class(yctx->sema, class_id);
    if (!cls->ast_node || cls->ast_node->tag != AST_CLASSDECL) return cont;
    ast_member_t** members = cls->ast_node->class_decl.members;
    int member_count = cls->ast_node->class_decl.members_count;
    yctx->current_class_id_ = class_id;
    sir_node_t* cur = cont;                          /* tail-first: last field built first */
    for (int mi = member_count - 1; mi >= 0; mi--) {
        ast_member_t* m = members[mi];
        if (m->tag != AST_FIELDDECL) continue;
        for (int di = m->field_decl.decls_count - 1; di >= 0; di--) {
            ast_var_decl_t* vd = m->field_decl.decls[di];
            if (!vd->init) continue;
            const sema_field_t* f = find_instance_field(cls, vd->name);
            if (!f) continue;                        /* static field — runs in <clinit> */
            sir_datatype_t dt = jt_data_type(f->type);
            sir_node_t* ref = (dt == SIR_DTREF)
                ? ddcg_expr_ref(yctx, (ast_expr_t*)vd->init) : NULL;
            int t_v = ddcg_ddcg_alloc_temp(yctx, dt);
            sir_node_t* pf = sir_put_field(yctx->arena, dt,
                                 sir_load_local(yctx->arena, 0, SIR_DTREF, NULL),
                                 class_id, f->index,
                                 sir_load_local(yctx->arena, t_v, dt, ref), cur);
            delta_t dest = (dt == SIR_DTREF) ? locref(yctx, t_v, ref) : loc(yctx, t_v, dt);
            cur = ddcg_cg_deliver_conv(yctx, (ast_expr_t*)vd->init, dt, rho_root(yctx),
                                       dest, single(yctx, pf), pf);
        }
    }
    return cur;
}

/* The leading explicit/implicit constructor-invocation statement of a ctor body
 * — this(...) or super(...) (sema prepends an implicit super() to every ctor of a
 * class with a superclass, JLS §8.8.7) — or NULL if there is none (only Object,
 * which has no superclass). JLS §12.5 sequences it FIRST (step 2/3), before the
 * instance-field initializers (step 4) and the rest of the body (step 5). */
static ast_stmt_t* ctor_leading_call(ast_stmt_t* body) {
    if (!body || body->tag != AST_BLOCK || body->block.stmts_count == 0) return NULL;
    ast_stmt_t* s0 = body->block.stmts[0];
    if (s0 && s0->tag == AST_EXPRSTMT && s0->expr_stmt.e
        && s0->expr_stmt.e->tag == AST_CONSTRUCTORCALL)
        return s0;
    return NULL;
}

/* §20.1.5/§10.7 the array-overlay internalClone body: a fresh overlay whose backing is an
 * array.copy of `this`'s backing (deep backing, shallow elements), carrying `this`'s runtime
 * array Class (getClass()) and, for a RefArray, its elementClass. `this` is local 0. */
static sir_node_t* build_overlay_clone(ddcg_ctx_t* yctx, int class_id) {
    bbq_arena* a = yctx->arena;
    const sema_ctx_t* sema = yctx->sema;
    bool is_ref = (class_id == sema_refarray_id(sema));
    sir_datatype_t width = SIR_DTREF;
    sir_atype_t    at    = SIR_ATINT;
    int data_field = is_ref ? 1 : 0;   /* ddcg_refarray_data_field / ddcg_primarray_data_field */
    if (!is_ref) {
        /* The width comes from the LATTICE's storage-index inverse — a local
         * si→dt table here is how V128Array.clone allocated an (array i32)
         * backing into the (ref null (array v128)) data field (a §3.4.7
         * struct.set mismatch only the VM validator caught). */
        width = SIR_DTINT;
        for (int si = 0; si < 8; si++)
            if (class_id == sema_primarray_id(sema, si)) {
                width = lat_prim_storage_dt(si);
                at    = lat_dt_to_atype(width);
                break;
            }
    }
    int t = yctx->next_temp_++;   /* the clone overlay (slot 1, past `this`) */

    #define OVL_THIS()  sir_load_this(a, SIR_DTREF, class_id)                                   /* (ref overlay) this */
    #define OVL_CLONE() sir_load_local(a, t, SIR_DTREF, sir_class_ref(a, class_id))             /* (ref overlay) clone */
    #define THIS_DATA() sir_get_field(a, SIR_DTREF, OVL_THIS(), class_id, data_field)           /* this's raw backing */

    /* return clone; */
    sir_node_t* ret = sir_return(a, OVL_CLONE(), SIR_DTREF);
    /* clone header = this.getClass() (the runtime array Class, e.g. "[LString;") */
    sir_node_t* getcls = sir_invoke_special(a, OVL_THIS(), sema_object_id(sema),
                                            sema_getclass_method_id(sema), NULL, 0, SIR_DTREF);
    sir_node_t* chain = sir_set_header(a, OVL_CLONE(), getcls, class_id, ret);
    /* RefArray: clone.elementClass = this.elementClass (field 0) */
    if (is_ref)
        chain = sir_put_field(a, SIR_DTREF, OVL_CLONE(), class_id, 0 /*elem field*/,
                              sir_get_field(a, SIR_DTREF, OVL_THIS(), class_id, 0), chain);
    /* array.copy clone.data[0..] <- this.data[0..], len = this.data.length */
    sir_node_t* clone_data = sir_get_field(a, SIR_DTREF, OVL_CLONE(), class_id, data_field);
    chain = sir_array_copy(a, width, clone_data, sir_load_const(a, 0, SIR_DTINT),
                           THIS_DATA(), sir_load_const(a, 0, SIR_DTINT),
                           sir_array_length(a, THIS_DATA()), chain);
    /* clone.data = a fresh backing of this.data.length */
    sir_node_t* backing = is_ref
        ? sir_new_ref_array(a, 0, sir_array_length(a, THIS_DATA()), NULL)
        : sir_new_array(a, at, sir_array_length(a, THIS_DATA()));
    chain = sir_put_field(a, SIR_DTREF, OVL_CLONE(), class_id, data_field, backing, chain);
    /* clone = new overlay */
    return sir_store_local(a, t, SIR_DTREF, sir_class_ref(a, class_id), sir_new(a, class_id), chain);
    #undef OVL_THIS
    #undef OVL_CLONE
    #undef THIS_DATA
}

static sir_node_t* build_class_static_inits(ddcg_ctx_t* yctx, int class_id, sir_node_t* cont,
                                            int region);  /* fwd */
sir_node_t* ddcg_ref_descriptor(ddcg_ctx_t* ctx, java_type_t t);  /* compiler_helpers.c — type → ref descriptor */

/* Index of the named field / method in a class (synthetic $initstate / $ensure_init lookups). */
static int class_field_idx_by_name(const sema_ctx_t* s, int cid, const char* name) {
    const sema_class_t* c = sema_get_class(s, cid);
    for (int i = 0; i < (int)bbq_vec_len(c->fields); i++)
        if (strcmp(c->fields[i].name, name) == 0) return i;
    return -1;
}
static int class_method_idx_by_name(const sema_ctx_t* s, int cid, const char* name) {
    const sema_class_t* c = sema_get_class(s, cid);
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
        if (strcmp(c->methods[i].name, name) == 0) return i;
    return -1;
}

/* Find a constructor of `cid` by signature: `pc` params, and (pc>=1) param0 is class `p0`.
 * §12.4.2 needs ExceptionInInitializerError(Throwable) — distinct from its (String) overload —
 * and the no-arg NoClassDefFoundError(). Returns the method index, or -1. */
static int class_ctor_idx(const sema_ctx_t* s, int cid, int pc, int p0) {
    const sema_class_t* c = sema_get_class(s, cid);
    for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++) {
        const sema_method_t* m = &c->methods[mi];
        if (!m->is_constructor || m->param_count != pc) continue;
        if (pc == 0 || (m->param_types[0].tag == JT_CLASS && m->param_types[0].class_id == p0))
            return mi;
    }
    return -1;
}

/* `throw new Cls(arg?)` as a SIR chain producing no value (the Throw is the tail): allocate a temp,
 * struct.new, run the ctor as an effect, then throw the reference. `arg` may be NULL (no-arg ctor).
 *
 * `region` is the try region enclosing this throw, or -1 for none (spec §6) — the same fact the
 * grammar's record_throw_regions reads off ρ. These throws are hand-built, so they say it here.
 * EVERY caller below is either outside any region or inside a HANDLER (which its own try cannot
 * catch), so they all pass -1 today; the parameter exists so that stays a decision, not an
 * omission. */
static sir_node_t* build_throw_new(ddcg_ctx_t* yctx, int cls_id, int ctor_idx, sir_node_t* arg,
                                   int region) {
    bbq_arena* a = yctx->arena;
    int t = yctx->next_temp_++;
    sir_node_t** args = NULL; int argc = 0;
    if (arg) { args = (sir_node_t**)bbq_arena_alloc(a, sizeof(sir_node_t*)); args[0] = arg; argc = 1; }
    sir_node_t* thr   = sir_throw(a, sir_load_local(a, t, SIR_DTREF, sir_class_ref(a, cls_id)));
    ddcg_record_except(yctx, thr, region);
    /* The ctor CALL and the allocation are excepting points too (JLS §11.1: the callee
     * throws; OutOfMemoryError is specified AT the `new`) — same rule as the grammar's
     * throw_new_noarg, which this mirrors. */
    sir_node_t* call  = sir_invoke_special(a, sir_load_local(a, t, SIR_DTREF, sir_class_ref(a, cls_id)),
                                           cls_id, ctor_idx, args, argc, SIR_DTSHORT);
    ddcg_record_except(yctx, call, region);
    sir_node_t* ctor  = sir_expr_effect(a, call, 1, thr);   /* ctor is void: is_void discards */
    sir_node_t* nw    = sir_new(a, cls_id);
    ddcg_record_except(yctx, nw, region);
    return sir_store_local(a, t, SIR_DTREF, sir_class_ref(a, cls_id), nw, ctor);
}

/* JLS §12.4.2 (single-threaded ⇒ steps 1-2 no-op): the `$ensure_init` state machine. $initstate is
 * 0=uninit, 1=in-progress, 2=done, 3=erroneous.
 *   if ($initstate == 3) throw new NoClassDefFoundError();   // step 5 (erroneous re-entry)
 *   if ($initstate != 0) return;                             // steps 3,4 (in-progress OR done → return)
 *   $initstate = 1;                                          // step 6 (in-progress)
 *   try {
 *     Super$ensure_init();                                   // step 7 (if super needs_init)
 *     <step 8 own inits>;                                    // static field initializers + blocks
 *     $initstate = 2;                                        // step 9 (done)
 *   } catch (Error e)     { $initstate = 3; throw e; }        // steps 10,11 (an Error is not wrapped)
 *     catch (Throwable e) { $initstate = 3;                   // steps 10,11 (wrap a non-Error)
 *                           throw new ExceptionInInitializerError(e); }
 * The two catch-type entries ARE the instanceof-Error split — no explicit test. */
static sir_node_t* build_ensure_init(ddcg_ctx_t* yctx, int class_id) {
    bbq_arena* a = yctx->arena;
    const sema_ctx_t* s = yctx->sema;
    int st = class_field_idx_by_name(s, class_id, "$initstate");
    int super_id = sema_get_class(s, class_id)->super_id;
    bool super_ni = super_id >= 0 && sema_get_class(s, super_id)->needs_init;

    /* The region id for the Error/Throwable pair below. Allocated BEFORE the body is built,
     * because the body's §15 guard throws — the static initializers compile through the DDCG
     * and emit them — must name a region whose ExceptionEntry handlers do not exist yet.
     * build_class_static_inits compiles them under a try_frame carrying this id, so those
     * throws record their enclosure exactly as a source `throw` in a try block does. */
    int region = ddcg_next_region_id(yctx);

    /* ── the protected region: step 7 (super) → step 8 (own inits) → step 9 (state=done) ── */
    sir_node_t* set_done = sir_put_static(a, SIR_DTINT, class_id, st,          /* step 9 */
                                          sir_load_const(a, 2, SIR_DTINT), sir_return_void(a));
    sir_node_t* body = build_class_static_inits(yctx, class_id, set_done, region);  /* step 8 */
    if (super_ni) {                                                            /* step 7 (super before step 8) */
        int sem = class_method_idx_by_name(s, super_id, "$ensure_init");
        /* A CALL inside the protected body: the super's initializer can throw (§12.4.2
         * steps 10/11 rethrow), so it is an excepting point of THIS region (§11.1). */
        sir_node_t* sup = sir_invoke_static(a, super_id, sem, NULL, 0, SIR_DTINT);
        ddcg_record_except(yctx, sup, region);
        body = sir_expr_effect(a, sup, 1, body);   /* void: placeholder dt, discarded */
    }
    sir_node_t* try_start = sir_nop(a, body);

    /* ── steps 10/11: two typed catch handlers, each with its own landing temp ── */
    int throwable_id = s->wk.throwable_id, error_id = s->wk.error_id;

    /* Both rethrows live INSIDE a handler of this very region, so this region cannot catch
     * them — they leave $ensure_init, and nothing encloses it. Region -1: they escape, which
     * is the truth (§6), not a fail-closed approximation of it. */
    int ex_e = yctx->next_temp_++;                                            /* catch (Error e) → rethrow as-is */
    sir_node_t* rethrow = sir_throw(a, sir_load_local(a, ex_e, SIR_DTREF, sir_class_ref(a, error_id)));
    ddcg_record_except(yctx, rethrow, -1);
    sir_node_t* h_error = sir_exception_entry(a, ex_e, error_id,
        sir_put_static(a, SIR_DTINT, class_id, st, sir_load_const(a, 3, SIR_DTINT), rethrow));

    int ex_t = yctx->next_temp_++;                                            /* catch (Throwable e) → wrap */
    int eiie_id = s->wk.exc_in_init_id;
    sir_node_t* wrap = build_throw_new(yctx, eiie_id, class_ctor_idx(s, eiie_id, 1, throwable_id),
                                       sir_load_local(a, ex_t, SIR_DTREF, sir_class_ref(a, throwable_id)),
                                       -1);
    sir_node_t* h_throwable = sir_exception_entry(a, ex_t, throwable_id,
        sir_put_static(a, SIR_DTINT, class_id, st, sir_load_const(a, 3, SIR_DTINT), wrap));

    /* wrap the body: Error region innermost (matched first), then Throwable (catch-all).
     * Both handlers carry the SAME region id — they are two handlers of one try, which is
     * what a throw in the body recorded against. */
    ddcg_record_try_region(yctx, try_start, h_error, error_id, region);
    sir_node_t* region_e = sir_try_region(a, h_error, try_start);
    ddcg_record_try_region(yctx, try_start, h_throwable, throwable_id, region);
    sir_node_t* region_t = sir_try_region(a, h_throwable, region_e);
    /* The chain exists now — stamp the exception continuation onto the recorded
     * excepting nodes, exactly as the try rule's patch_excepts does. */
    ddcg_patch_excepts(yctx, region, region_t);

    sir_node_t* set_ip = sir_put_static(a, SIR_DTINT, class_id, st,           /* step 6 */
                                        sir_load_const(a, 1, SIR_DTINT), region_t);

    /* ── entry dispatch: erroneous → NCDFE (step 5); done/in-progress → return (3,4); uninit → run ── */
    int ncdfe_id = s->wk.no_class_def_id;
    sir_node_t* erroneous = sir_branch(a,
        sir_eq(a, sir_get_static(a, SIR_DTINT, class_id, st), sir_load_const(a, 3, SIR_DTINT)),
        /* Step 5's throw is in the ENTRY DISPATCH, before the try — no region encloses it. */
        build_throw_new(yctx, ncdfe_id, class_ctor_idx(s, ncdfe_id, 0, -1), NULL, -1),
        sir_return_void(a));
    return sir_branch(a,
        sir_ne(a, sir_get_static(a, SIR_DTINT, class_id, st), sir_load_const(a, 0, SIR_DTINT)),
        erroneous, set_ip);
}

/* E7.1a: the synthesized `$main(int argc, int base) -> int` program-entry wrapper. (argc, base) are
 * locals 0/1 — the runner passes the argument count and the staging-memory offset of the NUL-separated
 * UTF-8 arguments. Body (JLS §12.5 has no bearing — this is our embedding entry, not a Java construct):
 *   String[] a = java.io.Startup.args(argc, base);
 *   try { UserMain.main(a); return 0; }
 *   catch (Throwable t) { t.printStackTrace(); return 1; }
 * The catch keeps an uncaught exception's diagnostic inside the guest as a Java stack trace with a
 * clean exit code 1; a normal return is exit 0. Mirrors build_ensure_init's try-region construction. */
static sir_node_t* build_main(ddcg_ctx_t* yctx, int class_id) {
    (void)class_id;
    bbq_arena* a = yctx->arena;
    const sema_ctx_t* s = yctx->sema;
    int string_id = s->wk.string_id, throwable_id = s->wk.throwable_id;
    sir_node_t* strarr_ref = sir_array_ref(a, string_id, 1);   /* the String[] ref descriptor */

    int t_argv = yctx->next_temp_++;                           /* String[] a */
    int region = ddcg_next_region_id(yctx);

    /* protected region: UserMain.main(a); return 0.
     * The call to user main IS the region's excepting point — it is the whole reason the
     * handler exists (§11.1: the callee's exceptions propagate to the invocation). */
    sir_node_t** margs = (sir_node_t**)bbq_arena_alloc(a, sizeof(sir_node_t*));
    margs[0] = sir_load_local(a, t_argv, SIR_DTREF, strarr_ref);
    sir_node_t* mcall = sir_invoke_static(a, s->wk.main_class_id, s->wk.main_method_id,
                                          margs, 1, SIR_DTINT);   /* void: dt discarded */
    ddcg_record_except(yctx, mcall, region);
    sir_node_t* prot = sir_expr_effect(a, mcall,
        1, sir_return(a, sir_load_const(a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* try_start = sir_nop(a, prot);

    /* catch (Throwable t) { t.printStackTrace(); return 1; }
     * printStackTrace runs INSIDE the handler, so this region cannot catch what it throws
     * (region -1), and nothing encloses $main. */
    int ex_t = yctx->next_temp_++;
    sir_node_t* pst = sir_invoke_virtual(a,
        sir_load_local(a, ex_t, SIR_DTREF, sir_class_ref(a, throwable_id)),
        throwable_id, s->wk.throwable_pst_method_id, NULL, 0, SIR_DTINT);   /* void: dt discarded */
    ddcg_record_except(yctx, pst, -1);
    sir_node_t* handler = sir_exception_entry(a, ex_t, throwable_id,
        sir_expr_effect(a, pst, 1, sir_return(a, sir_load_const(a, 1, SIR_DTINT), SIR_DTINT)));
    ddcg_record_try_region(yctx, try_start, handler, throwable_id, region);
    sir_node_t* tr = sir_try_region(a, handler, try_start);
    ddcg_patch_excepts(yctx, region, tr);

    /* prologue: a = java.io.Startup.args(argc, base) — BEFORE the try, so no region
     * encloses it (-1): an exception from it leaves $main uncaught. */
    sir_node_t** aargs = (sir_node_t**)bbq_arena_alloc(a, 2 * sizeof(sir_node_t*));
    aargs[0] = sir_load_local(a, 0, SIR_DTINT, NULL);
    aargs[1] = sir_load_local(a, 1, SIR_DTINT, NULL);
    sir_node_t* argv_call = sir_invoke_static(a, s->wk.startup_id, s->wk.startup_args_method_id,
                                              aargs, 2, SIR_DTREF);
    ddcg_record_except(yctx, argv_call, -1);
    return sir_store_local(a, t_argv, SIR_DTREF, strarr_ref, argv_call, tr);
}

static sir_method_t* compile_method(ddcg_ctx_t* yctx, int class_id,
                                          const sema_method_t* sm) {
    yctx->current_class_id_ = class_id;
    yctx->current_return_dt_ = jt_data_type(sm->return_type);
    yctx->current_return_ref_ = ddcg_ref_descriptor(yctx, sm->return_type);  /* NULL for primitive/void */
    yctx->next_temp_ = sema_max_user_slots(sm);
    yctx->next_region_ = 0;    /* region ids are per-method, like temps */

    /* §20.1.5 the synthesized internalClone override: `return <shallow copy of this>`. No AST
     * body — its value is the CloneCopy leaf (struct.new class_id copying each field of this). */
    if (sm->is_synthetic_clone) {
        int midx = 0;
        const sema_class_t* cls = sema_get_class(yctx->sema, class_id);
        for (int i = 0; i < bbq_vec_len(cls->methods); i++)
            if (&cls->methods[i] == sm) { midx = i; break; }
        /* An array Class deep-copies its overlay's backing (array.copy over the overlay struct
         * the runtime receiver is); a plain class copies its own fields. */
        int overlay = sema_array_class_overlay(yctx->sema, class_id);
        sir_node_t* entry = overlay >= 0
            ? build_overlay_clone(yctx, overlay)
            : sir_return(yctx->arena, sir_clone_copy(yctx->arena, class_id), SIR_DTREF);
        return sir_method(yctx->arena, sm->name, class_id, midx, yctx->next_temp_, entry);
    }

    /* JLS §12.4.2: the synthesized `$ensure_init` — no AST body; its SIR is the init state machine. */
    if (sm->is_synthetic_ensure_init) {
        int midx = class_method_idx_by_name(yctx->sema, class_id, "$ensure_init");
        yctx->next_temp_ = sema_clinit_slots(yctx->sema);   /* step 8 temps base past static-block local slots */
        /* steps 10/11 record try-regions here; this branch returns before the normal-path reset below,
         * so clear the previous method's already-collected facts (caller copied them) — else they bleed in. */
        { compiler_fact_t* v = (compiler_fact_t*)yctx->facts_;
          bbq_vec_free(v); yctx->facts_ = NULL; }
        sir_node_t* body = build_ensure_init(yctx, class_id);
        return sir_method(yctx->arena, sm->name, class_id, midx, yctx->next_temp_, body);
    }

    /* E7.1a: the synthesized `$main` program-entry wrapper — no AST body; build_main emits its SIR
     * (records one try-region, so clear the prior method's like the $ensure_init branch does). */
    if (sm->is_synthetic_main) {
        const sema_class_t* cls = sema_get_class(yctx->sema, class_id);
        int midx = 0;
        for (int i = 0; i < bbq_vec_len(cls->methods); i++)
            if (&cls->methods[i] == sm) { midx = i; break; }
        { compiler_fact_t* v = (compiler_fact_t*)yctx->facts_;
          bbq_vec_free(v); yctx->facts_ = NULL; }
        sir_node_t* body = build_main(yctx, class_id);
        return sir_method(yctx->arena, sm->name, class_id, midx, yctx->next_temp_, body);
    }
    {
        /* bbq_vec_free is a macro that needs an lvalue T*; the
         * void* storage on yctx isn't usable directly. Stage through
         * a local. */
        compiler_fact_t* v = (compiler_fact_t*)yctx->facts_;
        bbq_vec_free(v);
        yctx->facts_ = NULL;
    }

    ast_stmt_t* body = NULL;
    if (sm->ast_node) {
        if (sm->ast_node->tag == AST_METHODDECL)
            body = sm->ast_node->method_decl.body;
        else if (sm->ast_node->tag == AST_CONSTRUCTORDECL)
            body = sm->ast_node->constructor_decl.body;
    }

    /* The body's normal-completion edge. JLS §8.4.7/§14.21: a value-returning
     * method's body cannot complete normally (else a compile error), so its
     * fall-off point is unreachable and carries NO implicit return — javac emits
     * nothing there. Only a void method gets the implicit `return`. (When WASM's
     * §7.6 still treats the function end as reachable — it resets reachability
     * after every `end` — the structurer caps a non-void body with `unreachable`.) */
    bool returns_value = sm->return_type.tag != JT_VOID;
    sir_node_t* fall_off_label = returns_value
        ? sir_nop(yctx->arena, NULL)
        : sir_nop(yctx->arena, sir_return_void(yctx->arena));

    /* JLS §12.5 constructor order, sequenced verbatim:
     *   step 2/3 — the leading this()/super() invocation (sema-prepended);
     *   step 4   — the instance variable + instance initializers (the prologue),
     *              SKIPPED when the ctor delegates via this() (the delegate runs them);
     *   step 5   — the rest of the constructor body.
     * Built back-to-front in CPS: rest → [step4] → leading call. A non-constructor
     * (or Object's ctor, which has no leading call) just compiles its body, with
     * the prologue at entry for the latter (no super to precede it). */
    bool is_ctor = sm->ast_node && sm->ast_node->tag == AST_CONSTRUCTORDECL;
    ast_stmt_t* lead = is_ctor ? ctor_leading_call(body) : NULL;
    sir_node_t* entry;
    if (lead) {
        bool is_this = !lead->expr_stmt.e->constructor_call.is_super;
        ast_stmt_t* rest = ast_block(yctx->arena, body->block.stmts + 1,
                                     body->block.stmts_count - 1);
        sir_node_t* rest_c = ddcg_compile_stmt(yctx, rest, rho_root(yctx), effect(yctx),
                                               single(yctx, fall_off_label), fall_off_label);
        sir_node_t* step4 = is_this ? rest_c
                          : synth_instance_prologue(yctx, class_id, rest_c);
        entry = ddcg_compile_stmt(yctx, lead, rho_root(yctx), effect(yctx),
                                  single(yctx, step4), step4);
    } else {
        entry = body
            ? ddcg_compile_stmt(yctx, body, rho_root(yctx), effect(yctx),
                                 single(yctx, fall_off_label), fall_off_label)
            : fall_off_label;
        if (is_ctor)   /* Object's ctor: no super precedes, so step 4 runs at entry */
            entry = synth_instance_prologue(yctx, class_id, entry);
    }

    const sema_class_t* cls = sema_get_class(yctx->sema, class_id);
    int midx = 0;
    for (int i = 0; i < bbq_vec_len(cls->methods); i++)
        if (&cls->methods[i] == sm) { midx = i; break; }

    int32_t max_locals = yctx->next_temp_;
    return sir_method(yctx->arena, sm->name, class_id, midx, max_locals, entry);
}

/* One <clinit> step, in textual order (JLS §8.7/§12.4.2): a static field's
 * declaration-site initializer (kind 0) or a static initializer block (kind 1). */
typedef struct { int kind; int ci; const sema_field_t* fld; ast_stmt_t* block; } clinit_step_t;

/* The static field declared by `name` in `cls`, or NULL. */
static const sema_field_t* find_static_field(const sema_class_t* cls, const char* name) {
    for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++)
        if ((cls->fields[i].modifiers & ACC_STATIC) &&
            strcmp(cls->fields[i].name, name) == 0)
            return &cls->fields[i];
    return NULL;
}

/* Synthesize the module initializer (<clinit>): a void function running each user
 * class's static field declaration-site initializers AND static initializer
 * blocks, in textual order within a class (JLS §12.4.2) and class_id order across
 * classes (true lazy per-class init is an AOT-start-function divergence). Field
 * inits lower to `<field> = <init>` (PutStatic, §5.2 widening); static blocks
 * compile via the normal statement path. Their local slots were reserved by sema
 * (sema_clinit_slots) so temps base past them. Returns NULL when there is no
 * static initializer at all. The caller captures yctx->facts_ as the clinit's.
 *
 * `region` is the enclosing try region ($ensure_init's Error/Throwable pair), or -1 when
 * there is none. The initializers compile through the DDCG and emit §15 guards, whose
 * `throw new …` must record its enclosure like any other throw (§6) — so they are compiled
 * under a try_frame carrying the id, exactly as the try rule does for a source try block. */
static sir_node_t* build_class_static_inits(ddcg_ctx_t* yctx, int class_id, sir_node_t* cont,
                                            int region) {
    const sema_class_t* cls = sema_get_class(yctx->sema, class_id);
    if (!cls->ast_node) return cont;
    ast_type_decl_t* td = cls->ast_node;
    ast_member_t** members; int member_count;
    if (td->tag == AST_CLASSDECL) { members = td->class_decl.members; member_count = td->class_decl.members_count; }
    else { members = td->interface_decl.members; member_count = td->interface_decl.members_count; }
    clinit_step_t* steps = NULL;                /* bbq_vec, in textual order */
    for (int mi = 0; mi < member_count; mi++) {
        ast_member_t* m = members[mi];
        if (m->tag == AST_FIELDDECL) {
            for (int di = 0; di < m->field_decl.decls_count; di++) {
                ast_var_decl_t* vd = m->field_decl.decls[di];
                if (!vd->init) continue;
                const sema_field_t* f = find_static_field(cls, vd->name);
                if (!f) continue;                       /* instance field — runs in <init> */
                clinit_step_t s = { 0, class_id, f, NULL };
                bbq_vec_push(steps, s);
            }
        } else if (m->tag == AST_STATICINIT) {
            clinit_step_t s = { 1, class_id, NULL, m->static_init.body };
            bbq_vec_push(steps, s);
        }
    }
    int n = (int)bbq_vec_len(steps);
    sir_node_t* cur = cont;
    /* The initializers run INSIDE $ensure_init's try region — so they compile under its
     * frame, and the §15 guard throws they emit record it (§6). region < 0 (no enclosing
     * try) leaves ρ at the root, and those throws record nothing: they escape. */
    rho_t init_rho = region >= 0 ? try_frame(yctx, region, rho_root(yctx))
                                 : rho_root(yctx);
    for (int k = n - 1; k >= 0; k--) {
        yctx->current_class_id_ = steps[k].ci;
        if (steps[k].kind == 0) {
            const sema_field_t* f = steps[k].fld;
            sir_datatype_t dt = jt_data_type(f->type);
            /* A ref/array field's temp + put_static carry the field's CONCRETE ref type (§3.4). */
            sir_node_t* ref = (dt == SIR_DTREF) ? ddcg_expr_ref(yctx, (ast_expr_t*)f->init_expr) : NULL;
            int t_v = ddcg_ddcg_alloc_temp(yctx, dt);
            sir_node_t* ps = sir_put_static(yctx->arena, dt, steps[k].ci, f->index,
                                            sir_load_local(yctx->arena, t_v, dt, ref), cur);
            delta_t dest = (dt == SIR_DTREF) ? locref(yctx, t_v, ref) : loc(yctx, t_v, dt);
            cur = ddcg_cg_deliver_conv(yctx, (ast_expr_t*)f->init_expr, dt,
                                       init_rho, dest, single(yctx, ps), ps);
        } else {
            cur = ddcg_compile_stmt(yctx, steps[k].block, init_rho,
                                    effect(yctx), single(yctx, cur), cur);
        }
    }
    bbq_vec_free(steps);
    return cur;
}

void compiler_init(compiler_ctx_t* ctx, bbq_arena* arena, const sema_ctx_t* sema) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->arena = arena;
    ctx->sema = sema;
}

void compiler_destroy(compiler_ctx_t* ctx) {
    (void)ctx;
}

/* ── §7's DEFUNCTIONALIZED CALL GRAPH ──────────────────────────────────────────
 *
 * See the contract on compiler_ctx_t. This CONSUMES two things and derives nothing:
 * the lowered value graph (the SIR: where the call sites are) and the ONE dispatch rule
 * (sema_ref_is_subtype + sema_resolve_virtual: what each site can reach). The result is a
 * COMPLETE, FINITE target set per site, fixed before any analysis runs.
 *
 * The map from a callee's (declaring class, class-local index) to a METHOD INDEX. A miss
 * is not a failure: it is §7's bottom-method boundary (abstract declaration, native, a
 * cross-module jre import) — the site has a target the analysis has no body for. */
/* THE (class, method) → method-index key. One definition, shared by every lookup. */
#define CG_MI_KEY(c, m) (((uint64_t)(uint32_t)(c) << 32) | (uint32_t)(m))

/* Build (or rebuild) the shared index. Values are index+1 so a miss reads back as -1. */
static void mi_build(compiler_ctx_t* ctx) {
    if (ctx->mi_built) bbq_hmap_free(&ctx->method_index);
    bbq_hmap_init(&ctx->method_index, 0);
    for (int i = 0; i < ctx->method_count; i++) {
        const sir_method_t* m = ctx->methods[i];
        if (!m) continue;
        bbq_hmap_put(&ctx->method_index, CG_MI_KEY(m->class_id, m->method_id),
                     (void*)(intptr_t)(i + 1));
    }
    ctx->mi_built = true;
    ctx->mi_count = ctx->method_count;
}

int compiler_method_index(const compiler_ctx_t* ctx, int class_id, int method_id) {
    /* Hashed, not scanned. The analysis asks this per dispatch target, per call site, per
     * fixpoint iteration; a linear scan makes it quadratic-on-quadratic, which is precisely
     * what the call-graph builder's local index was introduced to avoid — and this is the
     * same question, so it is the same index. Mutable-through-const because the index is a
     * derived cache of ctx->methods, not state of its own. */
    compiler_ctx_t* mut = (compiler_ctx_t*)ctx;
    if (!ctx->mi_built || ctx->mi_count != ctx->method_count) mi_build(mut);
    return (int)(intptr_t)bbq_hmap_get(&mut->method_index, CG_MI_KEY(class_id, method_id)) - 1;
}

/* Every method index this call site can reach, pushed onto `out` (a bbq_vec<int>).
 *
 * A STATIC or SPECIAL call names its callee outright — special is JLS §15.11's
 * non-virtual invocation (ctor / super / private), so it does NOT dispatch.
 * A VIRTUAL or INTERFACE call fans out to its finite target set: every class in the
 * program that is a subtype of the declared receiver (§4.10.2), resolved by the ONE rule.
 * A class that resolves to nothing (an abstract method with no implementation) contributes
 * no edge — fail-closed, and it is the same answer the devirtualizer gets. */
static void cg_push_targets(compiler_ctx_t* ctx, bbq_hmap* midx,
                            const sir_node_t* call, int** out) {
    const sema_ctx_t* s = ctx->sema;
    int decl_cls, decl_m;
    bool virt;
    #define CG_KEY(c, m) CG_MI_KEY((c), (m))
    #define CG_LOOKUP(c, m) ((int)(intptr_t)bbq_hmap_get(midx, CG_KEY((c), (m))) - 1)
    switch (call->tag) {
    case SIR_INVOKESTATIC:
        decl_cls = call->invoke_static.class_id;
        decl_m   = call->invoke_static.method_idx;    virt = false; break;
    case SIR_INVOKESPECIAL:
        decl_cls = call->invoke_special.class_id;
        decl_m   = call->invoke_special.method_idx;   virt = false; break;
    case SIR_INVOKEVIRTUAL:
        decl_cls = call->invoke_virtual.class_id;
        decl_m   = call->invoke_virtual.method_idx;   virt = true;  break;
    case SIR_INVOKEINTERFACE:
        decl_cls = call->invoke_interface.class_id;
        decl_m   = call->invoke_interface.method_idx; virt = true;  break;
    default: return;
    }
    if (decl_cls < 0 || decl_m < 0) return;

    if (!virt) {
        int t = CG_LOOKUP(decl_cls, decl_m);
        if (t >= 0) bbq_vec_push(*out, t);
        return;
    }
    int nc = (int)bbq_vec_len(s->classes);
    for (int k = 0; k < nc; k++) {
        if (!sema_ref_is_subtype(s, k, decl_cls)) continue;
        int rc = -1, rm = -1;
        if (!sema_resolve_virtual(s, k, decl_cls, decl_m, &rc, &rm)) continue;
        int t = CG_LOOKUP(rc, rm);
        if (t >= 0) bbq_vec_push(*out, t);
    }
    #undef CG_LOOKUP
    #undef CG_KEY
}

/* This method's call sites: ONE LINEAR SCAN OF ITS SPINE — a scan, not a
 * traversal. The spine comes from THE collector; nothing here follows a successor.
 *
 * A call is an EXPRESSION — a producer, delivering its value to a destination (the DDCG
 * paper's δ) — so it lives inside a spine node's OPERAND TREE, not on the spine. Reading a
 * node's own operands is not a traversal either; it is reading the node. */
static void cg_scan_expr(compiler_ctx_t* ctx, bbq_hmap* midx, sir_node_t* e, int** out) {
    if (!e) return;
    cg_push_targets(ctx, midx, e, out);
    int a = sir_arity(e);
    for (int i = 0; i < a; i++) cg_scan_expr(ctx, midx, sir_child(e, i), out);
}

static void cg_scan_method(compiler_ctx_t* ctx, bbq_hmap* midx,
                           sir_method_t* m, int** out) {
    if (!m || !m->entry) return;
    sir_node_t** spine = sir_collect_spine(m->entry);
    for (int i = 0; i < (int)bbq_vec_len(spine); i++) {
        sir_node_t* n = spine[i];
        int a = sir_arity(n);
        for (int j = 0; j < a; j++) cg_scan_expr(ctx, midx, sir_child(n, j), out);
    }
    bbq_vec_free(spine);
}

void compiler_build_callgraph(compiler_ctx_t* ctx) {
    if (!ctx || ctx->cg_built) return;
    int n = ctx->method_count;
    ctx->cg_off = (int*)bbq_arena_alloc(ctx->arena, (size_t)(n > 0 ? n : 1) * sizeof(int));
    ctx->cg_cnt = (int*)bbq_arena_alloc(ctx->arena, (size_t)(n > 0 ? n : 1) * sizeof(int));

    /* THE shared (declaring class, class-local index) → method index. This used to be a local
     * hmap here, which fixed the quadratic-on-quadratic scan for the call-graph builder and
     * left compiler_method_index — the same lookup, asked far more often from inside the
     * analysis fixpoint — still scanning. One index, both callers. */
    if (!ctx->mi_built || ctx->mi_count != ctx->method_count) mi_build(ctx);
    bbq_hmap* midx = &ctx->method_index;

    int* flat = NULL;                       /* bbq_vec<int>: the CSR's edge array */
    for (int m = 0; m < n; m++) {
        int* tg = NULL;                     /* this method's callees, with duplicates */
        cg_scan_method(ctx, midx, ctx->methods[m], &tg);
        ctx->cg_off[m] = (int)bbq_vec_len(flat);
        int cnt = 0;
        for (int i = 0; i < (int)bbq_vec_len(tg); i++) {   /* dedup: one edge per callee */
            bool dup = false;
            for (int k = 0; k < cnt && !dup; k++)
                if (flat[ctx->cg_off[m] + k] == tg[i]) dup = true;
            if (dup) continue;
            bbq_vec_push(flat, tg[i]);
            cnt++;
        }
        ctx->cg_cnt[m] = cnt;
        bbq_vec_free(tg);
    }
    int ne = (int)bbq_vec_len(flat);
    ctx->cg_edge = (int*)bbq_arena_alloc(ctx->arena, (size_t)(ne > 0 ? ne : 1) * sizeof(int));
    for (int i = 0; i < ne; i++) ctx->cg_edge[i] = flat[i];
    bbq_vec_free(flat);   /* the index is ctx-owned now — it outlives this build on purpose */
    ctx->cg_built = true;
}

int compiler_callee_count(const compiler_ctx_t* ctx, int m) {
    if (!ctx || !ctx->cg_built || m < 0 || m >= ctx->method_count) return 0;
    return ctx->cg_cnt[m];
}

int compiler_callee(const compiler_ctx_t* ctx, int m, int k) {
    if (k < 0 || k >= compiler_callee_count(ctx, m)) return -1;
    return ctx->cg_edge[ctx->cg_off[m] + k];
}

/* Reverse-topological (callees-first) order — a DFS POSTORDER over the call graph, done
 * ITERATIVELY so a deep jre call chain cannot overflow the C stack. `state`: 0 unseen,
 * 1 on the DFS stack (children being walked), 2 emitted. A back edge points at a node in
 * state 1 or 2 and is simply not descended — "we ignore back edges" (Choi §4) falls out of
 * the visited check, with no cycle detection and no SCC pass. `childit[m]` is the next
 * unwalked callee of m; each node is pushed at most once, so a size-n cursor array suffices. */
int compiler_analysis_order(compiler_ctx_t* ctx, int* order) {
    if (!ctx || !ctx->cg_built) return 0;
    int n = ctx->method_count;
    if (n <= 0) return 0;
    bbq_arena* a = ctx->arena;
    char* st  = (char*)bbq_arena_alloc(a, (size_t)n);            /* 0 unseen / 1 open / 2 done */
    int*  it  = (int*)bbq_arena_alloc(a, (size_t)n * sizeof(int)); /* next unwalked callee    */
    int*  stk = (int*)bbq_arena_alloc(a, (size_t)n * sizeof(int)); /* the DFS node stack       */
    memset(st, 0, (size_t)n);
    int sp = 0, oc = 0;
    for (int root = 0; root < n; root++) {
        if (st[root]) continue;
        stk[sp++] = root; st[root] = 1; it[root] = 0;
        while (sp > 0) {
            int m = stk[sp - 1];
            if (it[m] < compiler_callee_count(ctx, m)) {
                int c = compiler_callee(ctx, m, it[m]++);
                if (c >= 0 && c < n && st[c] == 0) { stk[sp++] = c; st[c] = 1; it[c] = 0; }
            } else {
                order[oc++] = m; st[m] = 2; sp--;      /* all callees done → emit in postorder */
            }
        }
    }
    return oc;
}

const compiler_summary_t* compiler_method_summary(const compiler_ctx_t* ctx,
                                                  int method_idx) {
    if (!ctx || !ctx->summaries || method_idx < 0 || method_idx >= ctx->method_count)
        return NULL;
    const compiler_summary_t* s = &ctx->summaries[method_idx];
    return s->computed ? s : NULL;
}

/* THE ONE GETTER. Every fact the DDCG recorded for this method — try regions,
 * scopes, §15 guards, allocation sites, throw/handler enclosure — in record order.
 * Readers filter on `.kind`; see the PAYLOAD TABLE in compiler.h. */
const compiler_fact_t* compiler_get_facts(const compiler_ctx_t* ctx,
                                          int method_idx, int* count) {
    if (method_idx < 0 || method_idx >= ctx->method_count
            || !ctx->all_facts || !ctx->all_fact_counts) {
        *count = 0;
        return NULL;
    }
    *count = ctx->all_fact_counts[method_idx];
    return ctx->all_facts[method_idx];
}

sir_method_t** compiler_compile(compiler_ctx_t* ctx,
                                 ast_program_t* program,
                                 int* out_count) {
    (void)program;

    ddcg_ctx_t yctx;
    memset(&yctx, 0, sizeof(yctx));
    yctx.arena = ctx->arena;
    yctx.sema  = ctx->sema;

    sir_method_t** methods = NULL;
    int mc = 0;

    /* THE SIDECAR, copied out per method: one row type, one pair of vecs. */
    compiler_fact_t** f_ptrs   = NULL;  /* bbq_vec */
    int*              f_counts = NULL;  /* bbq_vec */

    /* Lower the same class range sema type-checked. sema->analyze_from is 0 for
     * a whole-program compile; a caller that type-checked only user classes
     * against an already-analyzed prelude (sema_analyze_more, or analyze_from
     * set directly) has no expression types recorded for the library bodies, so
     * lowering them would walk a sema gap. Same bound, one source of truth. */
    for (int ci = ctx->sema->analyze_from; ci < bbq_vec_len(ctx->sema->classes); ci++) {
        const sema_class_t* cls = sema_get_class(ctx->sema, ci);
        /* Iterate EVERY class (no ast_node skip) exactly like sema's function table
         * (sema.c "Build the emitted-function table"): a synthesized class with no source
         * — an array overlay — still owns a defined method (its §20.1.5 internalClone). */

        for (int mi = 0; mi < bbq_vec_len(cls->methods); mi++) {
            const sema_method_t* sm = &cls->methods[mi];
            /* Emit exactly the methods in sema's function table (same predicate),
             * so methods[] aligns 1:1 with the module function indices. Library
             * (java.lang) and body-less methods are excluded. */
            if (!sema_method_is_defined(ctx->sema, ci, sm)) continue;

            sir_method_t* m = compile_method(&yctx, ci, sm);
            methods = (sir_method_t**)jpush(ctx->arena, (void**)methods, &mc, m);

            /* One accumulator, one copy-out — for EVERY kind of fact. Adding a
             * fifth kind adds nothing here. That is the point of the row. */
            compiler_fact_t* yfv = (compiler_fact_t*)yctx.facts_;
            int fc = bbq_vec_len(yfv);
            compiler_fact_t* f_copy = NULL;
            if (fc > 0) {
                f_copy = (compiler_fact_t*)bbq_arena_alloc(ctx->arena,
                    (size_t)fc * sizeof(compiler_fact_t));
                memcpy(f_copy, yfv, (size_t)fc * sizeof(compiler_fact_t));
            }
            bbq_vec_push(f_ptrs, f_copy);
            bbq_vec_push(f_counts, fc);
        }
    }
    {
        compiler_fact_t* v = (compiler_fact_t*)yctx.facts_;
        bbq_vec_free(v);
        yctx.facts_ = NULL;
    }

    ctx->method_count = mc;
    ctx->methods = methods;      /* the context owns the compiled methods */
    ctx->all_facts = (compiler_fact_t**)bbq_arena_alloc(ctx->arena,
        (mc ? (size_t)mc : 1) * sizeof(compiler_fact_t*));
    ctx->all_fact_counts = (int*)bbq_arena_alloc(ctx->arena,
        (mc ? (size_t)mc : 1) * sizeof(int));
    for (int i = 0; i < mc; i++) {
        ctx->all_facts[i] = f_ptrs ? f_ptrs[i] : NULL;
        ctx->all_fact_counts[i] = f_counts ? f_counts[i] : 0;
    }
    bbq_vec_free(f_ptrs);
    bbq_vec_free(f_counts);

    /* The module initializer is built last, after user methods, so it never
     * perturbs their indices; the assembler appends it past the function table.
     * Capture its facts (static blocks may have loops/ifs, hence SCOPEs) the same
     * way a method's are captured. */
    {
        compiler_fact_t* fv0 = (compiler_fact_t*)yctx.facts_;
        bbq_vec_free(fv0); yctx.facts_ = NULL;
    }
    ctx->clinit = NULL;   /* JLS §12.4: static inits run LAZILY at first active use via the $ensure_init
                           * barriers (no eager combined <clinit>). The reflect-fixup bootstrap stays,
                           * emitted separately at module start (wasm_module.c). */
    ctx->clinit_facts = NULL;
    ctx->clinit_fact_count = 0;
    if (ctx->clinit) {
        compiler_fact_t* cfv = (compiler_fact_t*)yctx.facts_;
        int cfc = bbq_vec_len(cfv);
        if (cfc > 0) {
            compiler_fact_t* copy = (compiler_fact_t*)bbq_arena_alloc(ctx->arena,
                (size_t)cfc * sizeof(compiler_fact_t));
            memcpy(copy, cfv, (size_t)cfc * sizeof(compiler_fact_t));
            ctx->clinit_facts = copy;
            ctx->clinit_fact_count = cfc;
        }
        bbq_vec_free(cfv); yctx.facts_ = NULL;
    }

    *out_count = mc;
    return methods;
}
