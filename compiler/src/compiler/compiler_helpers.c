/* compiler_helpers.c — AUX bodies for compiler.ddcg.
 *
 * Sema-query trampolines, SIR helpers, ctx-state accessors. */

#include "gen/compiler_compile.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"
#include "javelina/compiler/sir_support.h"
#include "javelina/compiler/analyses.h"
#include "javelina/compiler/const_expr.h"   /* §15.27 — the ONE evaluator */
#include "bbq_vec.h"
#include "bbq_hmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((noreturn))
sir_node_t* ddcg_panic_unreachable(ddcg_ctx_t* ctx, const char* arg) {
    (void)ctx;
    fprintf(stderr, "ycdg: panic_unreachable: %s\n", arg ? arg : "(null)");
    abort();
}

/* sema_data_type returns -1 when the node wasn't tagged. The default is
 * SIR_DTINT — the WASM default integer width (a node that needed a sub-int
 * width is always tagged). */
sir_datatype_t ddcg_sema_data_type(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    int32_t dt = sema_data_type(ctx->sema, expr);
    return dt < 0 ? SIR_DTINT : (sir_datatype_t)dt;
}

/* Parametric fallback variant — caller-provided when the node isn't
 * tagged. Used by binop where operands fall back to the binop's own dt. */
sir_datatype_t ddcg_sema_data_type_or(ddcg_ctx_t* ctx, ast_expr_t* expr,
                                       sir_datatype_t fallback) {
    int32_t dt = sema_data_type(ctx->sema, expr);
    return dt < 0 ? fallback : (sir_datatype_t)dt;
}

sir_datatype_t ddcg_current_return_dt(ddcg_ctx_t* ctx) {
    return (sir_datatype_t)ctx->current_return_dt_;
}

sir_node_t* ddcg_current_return_ref(ddcg_ctx_t* ctx) {
    return (sir_node_t*)ctx->current_return_ref_;
}

/* Wrap a sir node in a Nop if it isn't already a Nop or
 * ExceptionEntry — so emit_backpatch can resolve a branch's arms. */
sir_node_t* ddcg_ddcg_label(ddcg_ctx_t* ctx, sir_node_t* n) {
    if (!n) return sir_nop(ctx->arena, NULL);
    if (n->tag == SIR_NOP || n->tag == SIR_EXCEPTIONENTRY) return n;
    return sir_nop(ctx->arena, n);
}

/* SIR back-patch primitive — see sir_set_next AUX comment in
 * compiler.ddcg. Returns the patched node so call-site lets can
 * chain (`let _ = sir_set_next(top, test);`). */
sir_node_t* ddcg_sir_set_next(ddcg_ctx_t* ctx, sir_node_t* node, sir_node_t* next) {
    (void)ctx;
    sir_set_next(node, next);
    return node;
}

int ddcg_ddcg_alloc_temp(ddcg_ctx_t* ctx, sir_datatype_t dt) {
    /* One WASM local per value, every width — matching sema's 1-slot-per-var
     * numbering and the one-local-per-slot locals declaration. */
    (void)dt;
    int slot = ctx->next_temp_;
    ctx->next_temp_ += 1;
    return slot;
}

/* Returns the SEMA_IDENT_* tag, or -1 when sema didn't resolve. */
int ddcg_sema_ident_kind(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    return info ? (int)info->kind : -1;
}

int ddcg_sema_ident_slot(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    return info ? info->slot : 0;
}

int ddcg_sema_var_slot(ddcg_ctx_t* ctx, ast_var_decl_t* vd) {
    return sema_slot(ctx->sema, vd);
}

sir_datatype_t ddcg_sema_ident_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    return info ? (sir_datatype_t)info->dt : SIR_DTINT;
}

/* The SIR reference-type descriptor (ClassRef / ArrayRef / PrimArray) for a Java
 * reference type, or NULL for a primitive (whose data_type fully describes it).
 * This is the proven referent the SIR carries to burg so a reference is never
 * type-erased — built from sema's resolved type, mirroring the type_lattice.
 *
 * ctx-FREE, because the DDCG is not its only caller: §6's scalar replacement mints the
 * descriptor for a field's slot from `sema_field_t.type` and must produce the byte-
 * identical node the DDCG would have. One builder, two callers (declared in
 * sir_op_gamma.h beside its inverse, gamma_ref_to_type). */
sir_node_t* sir_ref_descriptor(bbq_arena* arena, java_type_t t) {
    if (t.tag == JT_CLASS)
        return sir_class_ref(arena, t.class_id);
    if (t.tag == JT_ARRAY) {
        /* The CONCRETE BACKING of an overlay (sema.h: a JT_ARRAY marked class_id ==
         * JT_ARRAY_RAW) is an `(array W)` / `(array anyref)` that "must NOT be re-overlaid
         * into a PrimArray/RefArray" — the lattice says so too (lat_array_overlay_class
         * maps it to -1: not overlaid). The SIR's descriptor vocabulary is
         * ClassRef | ArrayRef | PrimArray — every one of them names an OVERLAY, so there
         * is no node here that can name a backing. Say so, instead of handing back an
         * overlay for a value that is a bare array: that lie typed a scalar-replaced
         * slot `(ref $PrimArray)` while it held `(array i32)`, and the §7.6 validator
         * rejected the jre for it. NULL = "no descriptor names this"; a caller that
         * needs one must fail CLOSED. Pinned: test_gamma §9. */
        if (t.class_id == JT_ARRAY_RAW) return NULL;
        int dim = 0;
        java_type_t* e = &t;
        while (e->tag == JT_ARRAY && e->element) { dim++; e = e->element; }
        /* The backing is marked TWO ways (type_lattice.h: "a PRIMITIVE array — and
         * RefArray's own backing, whose element is the top reference = JT_NULL — stays a
         * concrete, invariant array"). RefArray.data is an array with a JT_NULL element,
         * and a "PrimArray of width DTREF" is not a type. Not nameable either. */
        if (e->tag == JT_NULL) return NULL;
        return (e->tag == JT_CLASS)
             ? sir_array_ref(arena, e->class_id, dim)
             : sir_prim_array(arena, lat_tag_to_dt(e->tag), dim);
    }
    return NULL;
}

sir_node_t* ddcg_ref_descriptor(ddcg_ctx_t* ctx, java_type_t t) {
    return sir_ref_descriptor(ctx->arena, t);
}

/* The ONE dt → storage-narrowing authority (declared beside sir_ref_descriptor, same
 * one-builder discipline). An i8/i16 struct field narrows on struct.set and re-extends on
 * struct.get_s/_u — the STORAGE is byte/short/char semantics; an i32 local performs none.
 * §6's scalar replacement re-establishes it on the store side with the same conversion
 * nodes sema's cast lowering names (I2B/I2S/I2C). BOOL rides SIR_DTBYTE and its values are
 * 0/1 by construction, on which i2b is the identity. */
sir_node_t* sir_narrow_to_storage(bbq_arena* arena, sir_datatype_t dt, sir_node_t* value) {
    switch (dt) {
        case SIR_DTBYTE:  return sir_i2_b(arena, value);
        case SIR_DTSHORT: return sir_i2_s(arena, value);
        case SIR_DTCHAR:  return sir_i2_c(arena, value);
        default:          return value;   /* full-width dt: storage narrows nothing */
    }
}

/* THE spine collector — see sir_support.h. The ONE place a continuation edge is followed;
 * every consumer downstream scans the LIST. */
sir_node_t** sir_collect_spine(sir_node_t* entry) {
    sir_node_t** out = NULL;
    if (!entry) return out;
    sir_node_t** stack = NULL;
    bbq_hmap seen; bbq_hmap_init(&seen, 0);
    bbq_vec_push(stack, entry);
    while (bbq_vec_len(stack)) {
        sir_node_t* n = stack[bbq_vec_len(stack) - 1];
        bbq__vec_hdr(stack)->len--;
        if (!n || bbq_hmap_get(&seen, (uint64_t)(uintptr_t)n)) continue;
        bbq_hmap_put(&seen, (uint64_t)(uintptr_t)n, (void*)1);
        bbq_vec_push(out, n);
        int sc = sir_succ_count(n);
        for (int i = 0; i < sc; i++) {
            sir_node_t* s = sir_succ(n, i);
            if (s) bbq_vec_push(stack, s);
        }
    }
    bbq_vec_free(stack);
    bbq_hmap_free(&seen);
    return out;
}

/* The ref descriptor for a local's DECLARED type (sema's stored resolution) — types the slot when the
 * initializer carries no descriptor of its own, i.e. a `null` literal. NULL for a primitive local. */
sir_node_t* ddcg_sema_var_ref(ddcg_ctx_t* ctx, ast_var_decl_t* vd) {
    return ddcg_ref_descriptor(ctx, sema_var_type(ctx->sema, vd));
}

/* The reference descriptor for an identifier use (local/param/field), from its
 * sema-resolved type; NULL when the identifier is of primitive type. */
sir_node_t* ddcg_sema_ident_ref(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return ddcg_ref_descriptor(ctx, sema_type_of(ctx->sema, expr));
}

/* The reference descriptor for any expression's result type — the referent a
 * spill temp holding that value must be typed as. NULL for primitives. */
sir_node_t* ddcg_expr_ref(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return ddcg_ref_descriptor(ctx, sema_type_of(ctx->sema, expr));
}

/* The reference descriptor for the ELEMENT type of an array-typed expression —
 * carried on ArrayLoad/ArrayStore so burg selects the concrete array typeidx
 * (array-of-(ref class)) instead of the covariant structref collapse. NULL when
 * the element is primitive (the numeric array rules fully describe it). */
sir_node_t* ddcg_array_elem_ref(ddcg_ctx_t* ctx, ast_expr_t* arr) {
    java_type_t t = sema_type_of(ctx->sema, arr);
    if (t.tag == JT_ARRAY && t.element)
        return ddcg_ref_descriptor(ctx, *t.element);
    return NULL;
}

int ddcg_sema_field_index(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_field_index(ctx->sema, expr);
}

int ddcg_sema_field_decl_class(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_field_decl_class(ctx->sema, expr);
}

int ddcg_sema_method_index(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_method_index(ctx->sema, expr);
}

int ddcg_sema_method_decl_class(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_method_decl_class(ctx->sema, expr);
}

int ddcg_current_class_id(ddcg_ctx_t* ctx) {
    return ctx->current_class_id_;
}

/* §10 RefArray overlay: the synthesized class id, and the class-local field index
 * of its `data` backing array (fields are [0]=elementClass, [1]=data). */
int ddcg_refarray_class(ddcg_ctx_t* ctx)      { return sema_refarray_id(ctx->sema); }
int ddcg_refarray_data_field(ddcg_ctx_t* ctx) { (void)ctx; return 1; }
int ddcg_arraystore_check_class(ddcg_ctx_t* ctx)  { return sema_class_reflect_id(ctx->sema); }
int ddcg_arraystore_check_method(ddcg_ctx_t* ctx) { return sema_arraystore_check_method(ctx->sema); }
int ddcg_class_reflect_id(ddcg_ctx_t* ctx)        { return sema_class_reflect_id(ctx->sema); }
int ddcg_class_is_instance_method(ddcg_ctx_t* ctx){ return sema_is_instance_method(ctx->sema); }
int ddcg_primarray_class_dt(ddcg_ctx_t* ctx, sir_datatype_t dt)    { return lat_primarray_class(ctx->sema, dt); }
/* atype → element width via the ONE authority (lat_atype_to_dt). A local copy of
 * this map lived here with a default-to-int arm — which sent `new V128[]` to the
 * IntArray overlay while the backing was (array v128): a §3.4.7 struct.set type
 * mismatch the VM validator rightly rejected. The lattice header already said
 * this file's duplicates were replaced; this one had survived. */
int ddcg_primarray_class_atype(ddcg_ctx_t* ctx, sir_atype_t atype) { return lat_primarray_class(ctx->sema, lat_atype_to_dt(atype)); }
int ddcg_primarray_data_field(ddcg_ctx_t* ctx) { (void)ctx; return 0; }

/* §10.8 the Class object for an array-typed expression's type (its getClass()), or -1. */
int ddcg_array_class_id(ddcg_ctx_t* ctx, ast_expr_t* node) {
    return sema_array_class_id(ctx->sema, sema_type_of(ctx->sema, node));
}
/* §10.8 the Class object of the `dims`-dimensional array of node's BASE element — for each
 * nesting level of a multi-dimensional allocation. -1 if that type wasn't registered. */
int ddcg_array_class_at(ddcg_ctx_t* ctx, ast_expr_t* node, int dims) {
    java_type_t t = sema_type_of(ctx->sema, node);
    while (t.tag == JT_ARRAY && t.element) t = *t.element;   /* base element */
    java_type_t cur = t;
    for (int i = 0; i < dims; i++) {
        java_type_t* ep = (java_type_t*)bbq_arena_alloc(ctx->sema->arena, sizeof *ep);
        *ep = cur;
        cur = jt_array(ep);
    }
    return sema_array_class_id(ctx->sema, cur);
}

/* The element datatype of an array-typed expression — for the PrimBacking downcast width. */
sir_datatype_t ddcg_sema_array_elem_dt(ddcg_ctx_t* ctx, ast_expr_t* obj) {
    java_type_t t = sema_type_of(ctx->sema, obj);
    if (t.tag == JT_ARRAY && t.element) return lat_tag_to_dt(t.element->tag);
    return SIR_DTINT;
}
int ddcg_refarray_elem_field(ddcg_ctx_t* ctx) { (void)ctx; return 0; }

/* §10.10: the element type's class id for an array-creation node, or -1 when the
 * element is a primitive or a nested array (not a reference CLASS). */
int ddcg_array_elem_class(ddcg_ctx_t* ctx, ast_expr_t* node) {
    java_type_t t = sema_type_of(ctx->sema, node);
    if (t.tag == JT_ARRAY && t.element) {
        if (t.element->tag == JT_CLASS) return t.element->class_id;             /* class/interface component */
        if (t.element->tag == JT_ARRAY) return sema_array_class_id(ctx->sema, *t.element);  /* §10.10 multi-dim: the component array's Class */
    }
    return -1;
}

/* §10.10 the elementClass (component Class) for the `comp_dims`-deep component of a
 * multi-dim allocation level — the base class (0-dim), or the component array's Class. */
int ddcg_array_elem_class_at(ddcg_ctx_t* ctx, ast_expr_t* node, int comp_dims) {
    java_type_t t = sema_type_of(ctx->sema, node);
    while (t.tag == JT_ARRAY && t.element) t = *t.element;   /* base element */
    if (comp_dims <= 0) return t.tag == JT_CLASS ? t.class_id : -1;  /* base class, or primitive */
    java_type_t cur = t;
    for (int i = 0; i < comp_dims; i++) {
        java_type_t* ep = (java_type_t*)bbq_arena_alloc(ctx->sema->arena, sizeof *ep);
        *ep = cur; cur = jt_array(ep);
    }
    return sema_array_class_id(ctx->sema, cur);
}

/* JLS §13.1: a use of a CONSTANT VARIABLE (§4.12.4 — a final variable of primitive or
 * String type whose initializer is a constant expression) is resolved to its VALUE at
 * compile time. So is any other constant expression (§15.27). Returns (folded, literal);
 * the literal node is meaningful only when `folded`.
 *
 * ASKS THE ONE §15.27 EVALUATOR, for EVERY type. This used to be `sema_static_final_int`:
 * it folded byte/short/int and gave up on the other five — a `static final long`, `double`,
 * `float`, `char` or `boolean` emitted a real GetStatic, so `if (DEBUG)` with
 * `static final boolean DEBUG = false;` could not even be seen as unreachable (§14.19). It
 * also carried its own constant predicate (`sema_int_constant`), a THIRD one beside
 * jls_const_eval and the type lattice — and const_expr.h's own header warns that two
 * implementations of this predicate are free to disagree, and did.
 *
 * The literal's SIR width comes from `lat_tag_to_dt`, which the type lattice calls "the ONE
 * tag→width authority". */
ddcg_tup_bool_sir_node_t_ptr_t ddcg_fold_const_expr(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    ddcg_tup_bool_sir_node_t_ptr_t r = { false, NULL };
    jls_const_t c = jls_const_eval(ctx->sema, expr);
    switch (c.tag) {
    case JT_BOOL:
    case JT_BYTE: case JT_SHORT: case JT_CHAR: case JT_INT:
        r._0 = true;
        r._1 = sir_load_const(ctx->arena,
                              c.tag == JT_BOOL ? (c.v.b ? 1 : 0) : c.v.i,
                              lat_tag_to_dt(c.tag));
        return r;
    case JT_LONG:
        r._0 = true;
        r._1 = sir_load_long_const(ctx->arena, c.v.l);
        return r;
    case JT_FLOAT:
        r._0 = true;
        r._1 = sir_load_float_const(ctx->arena, c.v.f);
        return r;
    case JT_DOUBLE:
        r._0 = true;
        r._1 = sir_load_double_const(ctx->arena, c.v.d);
        return r;
    default:
        return r;   /* not a constant expression */
    }
}

/* All loop-frame state lives in ρ; sema computes break/continue
 * target depths during analysis and codegen just queries them. */

/* Arena-allocate a rho copy so loop_frame's `parent: rho*` field
 * stays valid for the lifetime of the compile (the bbq_arena
 * outlives every gen() recursion). */
struct rho* ddcg_rho_alloc_parent(struct ddcg_ctx* ctx, rho_t parent) {
    rho_t* p = (rho_t*)bbq_arena_alloc(ctx->arena, sizeof(rho_t));
    *p = parent;
    return p;
}

int ddcg_sema_break_target_depth(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    return sema_break_target_depth(ctx->sema, stmt);
}

int ddcg_sema_continue_target_depth(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    return sema_continue_target_depth(ctx->sema, stmt);
}

/* ── Field / array access helpers ─────────────────────────── */

/* JLS type tag → SIR datatype via the one type authority. */
static inline sir_datatype_t jt_to_dt(java_type_t t) {
    return lat_tag_to_dt(t.tag);
}

sir_datatype_t ddcg_sema_field_acc_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_field_t* f = sema_resolved_field(ctx->sema, expr);
    return f ? jt_to_dt(f->type) : SIR_DTINT;
}

/* JLS §5.3 method-invocation conversion: the dt argument `arg_idx` must be
 * delivered AS — the resolved callee's DECLARED parameter type, not the argument
 * expression's own type. The arg chain feeds this to cg_deliver_conv, so e.g.
 * the int literal in `new Long(42)` widens to long for the (long) ctor param.
 * (class_id, method_idx) is the same (defining class, method index) the invoke
 * node already carries. SIR_DTINT is a defensive fallback for an out-of-range
 * lookup — resolved calls always hit a real param. */
sir_datatype_t ddcg_sema_param_dt(ddcg_ctx_t* ctx, int class_id, int method_idx, int arg_idx) {
    const sema_class_t* cls = sema_get_class(ctx->sema, class_id);
    if (cls && method_idx >= 0 && method_idx < (int)bbq_vec_len(cls->methods)) {
        const sema_method_t* m = &cls->methods[method_idx];
        if (arg_idx >= 0 && arg_idx < m->param_count)
            return jt_to_dt(m->param_types[arg_idx]);
    }
    return SIR_DTINT;
}

/* The reference descriptor for a resolved call's PARAM slot — the type the arg
 * spill temp is declared at. Used to type a ref temp whose ARGUMENT has no
 * descriptor of its own (a `null` literal: `sema_type_of` is JT_NULL, so
 * ddcg_expr_ref is NULL): the slot still holds a `param-type` reference, and the
 * param type is the single authority for it. NULL for a primitive param. */
sir_node_t* ddcg_sema_param_ref(ddcg_ctx_t* ctx, int class_id, int method_idx, int arg_idx) {
    const sema_class_t* cls = sema_get_class(ctx->sema, class_id);
    if (cls && method_idx >= 0 && method_idx < (int)bbq_vec_len(cls->methods)) {
        const sema_method_t* m = &cls->methods[method_idx];
        if (arg_idx >= 0 && arg_idx < m->param_count)
            return ddcg_ref_descriptor(ctx, m->param_types[arg_idx]);
    }
    return NULL;
}

/* JLS §15.18.1 string-concatenation — the ddcg reads these to defunctionalize a
 * String `+` into new StringBuffer().append(..).toString() at SIR level. */
bool ddcg_sema_binary_is_concat(ddcg_ctx_t* ctx, ast_expr_t* e) { return sema_binary_is_concat(ctx->sema, e); }
int  ddcg_sema_string_buffer_id(ddcg_ctx_t* ctx)                { return sema_string_buffer_id(ctx->sema); }
int  ddcg_sema_sb_ctor_index(ddcg_ctx_t* ctx)                   { return sema_sb_ctor_index(ctx->sema); }
int  ddcg_sema_sb_tostring_index(ddcg_ctx_t* ctx)               { return sema_sb_tostring_index(ctx->sema); }
int  ddcg_sema_sb_append_index(ddcg_ctx_t* ctx, ast_expr_t* e)  { return sema_sb_append_index(ctx->sema, e); }
int  ddcg_sema_sb_append_param_class(ddcg_ctx_t* ctx, ast_expr_t* e) { return sema_sb_append_param_class(ctx->sema, e); }

bool ddcg_sema_field_is_static(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_field_t* f = sema_resolved_field(ctx->sema, expr);
    return f && (f->modifiers & ACC_STATIC);
}

int ddcg_sema_field_obj_class_id(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (expr->tag != AST_FIELDACCESS) return 0;
    java_type_t ot = sema_type_of(ctx->sema, expr->field_access.obj);
    return (int)ot.class_id;
}

bool ddcg_sema_is_array_length(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (expr->tag != AST_FIELDACCESS) return false;
    if (strcmp(expr->field_access.field, "length") != 0) return false;
    java_type_t ot = sema_type_of(ctx->sema, expr->field_access.obj);
    return ot.tag == JT_ARRAY;
}

sir_datatype_t ddcg_sema_array_acc_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (expr->tag != AST_ARRAYACCESS) return SIR_DTINT;
    java_type_t at = sema_type_of(ctx->sema, expr->array_access.arr);
    if (at.tag == JT_ARRAY && at.element)
        return jt_to_dt(*at.element);
    return SIR_DTINT;
}

bool ddcg_sema_may_have_effects(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_may_have_effects(ctx->sema, expr);
}

sir_datatype_t ddcg_sema_super_field_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_field_t* f = sema_resolved_super_field(ctx->sema, expr);
    return f ? jt_to_dt(f->type) : SIR_DTINT;
}

bool ddcg_sema_super_field_is_static(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_field_t* f = sema_resolved_super_field(ctx->sema, expr);
    return f && (f->modifiers & ACC_STATIC);
}

int ddcg_sema_super_field_cp(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    /* The field's class-local index (what wasm_types_field_index/struct.get need). */
    const sema_field_t* f = sema_resolved_super_field(ctx->sema, expr);
    return f ? f->index : 0;
}

int ddcg_sema_super_parent_class_id(ddcg_ctx_t* ctx) {
    const sema_class_t* cls = sema_get_class(ctx->sema, ctx->current_class_id_);
    return cls ? cls->super_id : -1;
}

sir_atype_t ddcg_sema_arrayinit_atype(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    int32_t jt = sema_array_init_elem_type(ctx->sema, expr);
    return lat_tag_to_atype(jt);
}

sir_datatype_t ddcg_sema_arrayinit_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    int32_t jt = sema_array_init_elem_type(ctx->sema, expr);
    return lat_tag_to_dt((java_type_tag_t)jt);  /* the one type authority — full set */
}

/* djb2 over a LENGTH, not a NUL-terminated string — sema's str_hash cannot be reused here
 * because a pooled run is arbitrary bytes and UTF-16LE ASCII carries a 0x00 every other one.
 * Same multiplier and the same key-0 avoidance (bbq_htree reserves it). */
static uint32_t bytes_hash(const uint8_t* b, size_t n) {
    uint32_t h = 5381;
    for (size_t i = 0; i < n; i++) h = h * 33 + b[i];
    return h ? h : 1;
}

/* Intern a run of bytes into the module's constant-data pool; answer its BYTE OFFSET.
 *
 * Takes raw bytes, NOT an AST node, deliberately. The pool is the module's read-only data —
 * today its only client is §10.6 constant array initializers, but the section is general and
 * a later producer (debug data, say) has bytes and no expression to hand. Keeping the
 * primitive at the byte level is what lets it have a second client without being rewritten.
 *
 * CONTENT-ADDRESSED: keyed by the run's hash, so an identical run is one lookup rather than a
 * scan over everything pooled so far. A hit still memcmps — the key is a uint32_t over
 * arbitrary bytes, so two distinct runs can collide, and sharing them would silently hand one
 * client another's contents. On a collision the run is appended and left unindexed, which
 * costs a dedup opportunity and never correctness. */
int const_pool_intern(uint8_t** pool, bbq_htree** index, const uint8_t* run, size_t runlen) {
    if (!*index) *index = bbq_htree_create();
    uint32_t key = bytes_hash(run, runlen);

    void* found = bbq_htree_search(*index, key);
    if (found) {
        size_t off = (size_t)(uintptr_t)found - 1;          /* stored +1: offset 0 is valid */
        if (off + runlen <= (size_t)bbq_vec_len(*pool) &&
            memcmp(*pool + off, run, runlen) == 0) return (int)off;
    }

    size_t off = (size_t)bbq_vec_len(*pool);
    for (size_t k = 0; k < runlen; k++) bbq_vec_push(*pool, run[k]);
    if (!found) bbq_htree_insert(*index, key, (void*)(uintptr_t)(off + 1));
    return (int)off;
}

/* Element stride in data-segment bytes. The engine reads a segment with
 * `le_load(bytes + off + k*w, w)` where w is the RTT's packed store width, so this must be
 * that same width. A ref element has no data-segment form at all. */
static int const_elem_stride(java_type_tag_t jt) {
    switch (jt) {
    case JT_BOOL: case JT_BYTE:    return 1;
    case JT_SHORT: case JT_CHAR:   return 2;
    case JT_INT: case JT_FLOAT:    return 4;
    case JT_LONG: case JT_DOUBLE:  return 8;
    default:                       return 0;   /* not a primitive: no segment form */
    }
}

/* One constant element as the raw bits its stride-many bytes carry. Floats go in by their
 * IEEE 754 pattern, which is what a load of the array reads back — a decimal round-trip
 * would be a second, lossier representation of a value the evaluator already has exactly. */
static bool const_elem_bits(const sema_ctx_t* sema, const ast_expr_t* e,
                            java_type_tag_t jt, uint64_t* out) {
    jls_const_t c = jls_const_eval(sema, e);
    if (c.tag == JT_VOID) return false;
    /* §5.2 / §10.6: the element assignment-converts to the array's element type, so an int
     * constant in a byte[] narrows here exactly as it would at a store. */
    switch (jt) {
    case JT_BOOL:   *out = (c.tag == JT_BOOL) ? (c.v.b ? 1u : 0u) : ((uint32_t)c.v.i & 1u); return true;
    case JT_BYTE:   *out = (uint8_t)(int8_t)c.v.i;            return true;
    case JT_SHORT:
    case JT_CHAR:   *out = (uint16_t)c.v.i;                   return true;
    case JT_INT:    *out = (uint32_t)c.v.i;                   return true;
    case JT_LONG:   *out = (uint64_t)(c.tag == JT_LONG ? c.v.l : (int64_t)c.v.i); return true;
    case JT_FLOAT: {
        float f = (c.tag == JT_FLOAT)  ? c.v.f
                : (c.tag == JT_DOUBLE) ? (float)c.v.d
                : (c.tag == JT_LONG)   ? (float)c.v.l : (float)c.v.i;
        uint32_t bits; memcpy(&bits, &f, 4); *out = bits; return true;
    }
    case JT_DOUBLE: {
        double d = (c.tag == JT_DOUBLE) ? c.v.d
                 : (c.tag == JT_FLOAT)  ? (double)c.v.f
                 : (c.tag == JT_LONG)   ? (double)c.v.l : (double)c.v.i;
        uint64_t bits; memcpy(&bits, &d, 8); *out = bits; return true;
    }
    default: return false;
    }
}

bool ddcg_arrayinit_is_const(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr || expr->tag != AST_ARRAYINIT || !ctx->const_data_) return false;
    java_type_tag_t jt = (java_type_tag_t)sema_array_init_elem_type(ctx->sema, expr);
    if (const_elem_stride(jt) == 0) return false;
    /* An EMPTY initializer stays on the ordinary path: `new char[0]` is already one
     * allocation and no stores, so there is nothing to win and a zero-length run would be a
     * needless special case downstream. */
    if (expr->array_init.elems_count == 0) return false;
    for (int i = 0; i < expr->array_init.elems_count; i++) {
        uint64_t bits;
        if (!const_elem_bits(ctx->sema, expr->array_init.elems[i], jt, &bits)) return false;
    }
    return true;
}

int ddcg_const_data_offset(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    java_type_tag_t jt = (java_type_tag_t)sema_array_init_elem_type(ctx->sema, expr);
    size_t w = (size_t)const_elem_stride(jt);
    size_t n = (size_t)expr->array_init.elems_count;

    /* Serialize into a scratch run first, so the pool can be searched for an identical one.
     * Arena memory: small, dies with the compile, same lifetime as the pool it may join. */
    uint8_t* run = (uint8_t*)bbq_arena_alloc(ctx->arena, n * w);
    for (size_t i = 0; i < n; i++) {
        uint64_t bits = 0;
        (void)const_elem_bits(ctx->sema, expr->array_init.elems[i], jt, &bits);
        for (size_t b = 0; b < w; b++)          /* little-endian, per jav_array_new_data */
            run[i * w + b] = (uint8_t)((bits >> (8 * b)) & 0xFF);
    }

    return const_pool_intern((uint8_t**)ctx->const_data_,
                             (bbq_htree**)ctx->const_data_index_, run, n * w);
}

int ddcg_sema_invoke_kind(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return (int)sema_invoke_kind(ctx->sema, expr);
}


int ddcg_sema_invoke_target_class(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    int32_t tc = sema_target_class(ctx->sema, expr);
    return tc < 0 ? 0 : tc;
}

sir_datatype_t ddcg_sema_invoke_dt(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    java_type_t rt = sema_type_of(ctx->sema, expr);
    if (rt.tag == JT_VOID) return SIR_DTINT;
    return jt_to_dt(rt);
}

bool ddcg_sema_invoke_is_void(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_type_of(ctx->sema, expr).tag == JT_VOID;
}

int ddcg_sema_new_target_class(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    int32_t tc = sema_target_class(ctx->sema, expr);
    if (tc >= 0) return tc;
    /* No name re-resolution here: sema records target_classes for every
     * AST_NEW it accepts (§6.5.4 resolution is unit-relative — a codegen-time
     * table lookup on the spelled name would be §7-blind). Absent = sema
     * rejected the expression. */
    return -1;
}

int ddcg_sema_new_ctor_cp(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    /* The resolved constructor's per-class method_id (−1 if none → implicit
     * default ctor). The InvokeSpecial emit maps (target_class, method_id) to the
     * global function index via wasm_func_index. */
    return sema_method_index(ctx->sema, expr);
}

/* §15 implicit-exception guard support (backend synthesizes `new <exc>()`). */
int ddcg_sema_arithmetic_exc_id(ddcg_ctx_t* ctx) {
    return sema_arithmetic_exc_id(ctx->sema);
}
int ddcg_sema_null_pointer_exc_id(ddcg_ctx_t* ctx) {
    return sema_null_pointer_exc_id(ctx->sema);
}
int ddcg_sema_array_index_exc_id(ddcg_ctx_t* ctx) {
    return sema_array_index_exc_id(ctx->sema);
}
int ddcg_sema_neg_array_size_exc_id(ddcg_ctx_t* ctx) {
    return sema_neg_array_size_exc_id(ctx->sema);
}
int ddcg_sema_class_cast_exc_id(ddcg_ctx_t* ctx) {
    return sema_class_cast_exc_id(ctx->sema);
}
int ddcg_sema_index_oob_exc_id(ddcg_ctx_t* ctx) {
    return sema_index_oob_exc_id(ctx->sema);
}
int ddcg_sema_array_store_exc_id(ddcg_ctx_t* ctx) {
    return sema_array_store_exc_id(ctx->sema);
}
int ddcg_sema_noarg_ctor_index(ddcg_ctx_t* ctx, int class_id) {
    return sema_noarg_ctor_index(ctx->sema, class_id);
}
int ddcg_sema_string_arg_ctor_index(ddcg_ctx_t* ctx, int class_id) {
    return sema_string_arg_ctor_index(ctx->sema, class_id);
}
int ddcg_sema_string_chars_ctor_index(ddcg_ctx_t* ctx) {
    return sema_string_chars_ctor_index(ctx->sema);
}
int ddcg_sema_string_class_id(ddcg_ctx_t* ctx) {
    return sema_string_class_id(ctx->sema);
}

/* Pool an ASCII message as UTF-16LE code units and answer its byte offset. The messages are
 * the spec's own ("/ by zero"), so one byte per unit and the high half is always zero — but
 * the units are written in full, because what lands in the segment must be exactly what a
 * `(array i16)` reads back. */
int ddcg_const_string_offset(ddcg_ctx_t* ctx, const char* s) {
    size_t n = strlen(s);
    uint8_t* run = (uint8_t*)bbq_arena_alloc(ctx->arena, n * 2);
    for (size_t i = 0; i < n; i++) {
        run[i * 2]     = (uint8_t)s[i];
        run[i * 2 + 1] = 0;
    }
    return const_pool_intern((uint8_t**)ctx->const_data_,
                             (bbq_htree**)ctx->const_data_index_, run, n * 2);
}

int ddcg_const_string_length(ddcg_ctx_t* ctx, const char* s) {
    (void)ctx; return (int)strlen(s);
}

/* JLS §15.17.3: `%` on float/double is the truncated remainder and WASM has no
 * f32.rem/f64.rem — the ddcg desugars it to a call to Math's fdlibm fmod. */
int ddcg_sema_frem_class(ddcg_ctx_t* ctx) {
    return sema_frem_class(ctx->sema);
}
int ddcg_sema_frem_method(ddcg_ctx_t* ctx, sir_datatype_t dt) {
    return sema_frem_method(ctx->sema, (int32_t)dt);
}

/* JLS §15.16.2: integer / and % by zero throw ArithmeticException. WASM's
 * i32/i64.div_s traps instead, so these ops route through a guarded lowering
 * that checks the divisor and throws. True iff `e` is such an op (int/long). */
bool ddcg_is_guarded_intdiv(ddcg_ctx_t* ctx, ast_expr_t* e) {
    if (e->tag != AST_BINARY) return false;
    if (e->binary.op != AST_DIV && e->binary.op != AST_REM) return false;
    int32_t dt = sema_data_type(ctx->sema, e);
    return dt == SIR_DTINT || dt == SIR_DTLONG;
}

/* JLS §20.18.16: a resolved System.arraycopy with a concrete-array src → the array.copy intrinsic. */
bool ddcg_sema_is_arraycopy(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_is_arraycopy(ctx->sema, e);
}

/* §20.9/§20.10: a resolved Float/Double raw bit accessor → a Move* bitcast (0 none, 1-4 kind). */
int ddcg_sema_move_intrinsic_kind(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_move_intrinsic_kind(ctx->sema, e);
}
bool ddcg_sema_is_move_intrinsic(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_is_move_intrinsic(ctx->sema, e);
}
/* §20.11: a resolved Math.sqrt/floor/ceil/rint → an f64 opcode (0 none, 1 sqrt, 2 floor, 3 ceil, 4 rint). */
int ddcg_sema_math_intrinsic_kind(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_math_intrinsic_kind(ctx->sema, e);
}
bool ddcg_sema_is_math_intrinsic(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_is_math_intrinsic(ctx->sema, e);
}
/* javelina.simd: a resolved intrinsic call's family / opcode / validated lane
 * (the generated table + the sema stash — the ddcg never re-derives). */
bool ddcg_sema_is_simd_intrinsic(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_is_simd_intrinsic(ctx->sema, e);
}
int ddcg_sema_simd_family(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_simd_family(ctx->sema, e);
}
int ddcg_sema_simd_op(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_simd_op(ctx->sema, e);
}
int ddcg_sema_simd_lane(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return (int)sema_simd_lane(ctx->sema, e);
}
int ddcg_sema_simd_align(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_simd_align(ctx->sema, e);
}
int ddcg_sema_simd_awidth(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_simd_awidth(ctx->sema, e);
}
/* The 128-bit const/shuffle immediate halves do not fit an int extern: write
 * the sema-validated stash INTO the built node, by tag. Returns 0 (extern
 * calling convention). */
int ddcg_sema_simd_fill(ddcg_ctx_t* ctx, sir_node_t* n, ast_expr_t* e) {
    int64_t lo = sema_simd_lo(ctx->sema, e), hi = sema_simd_hi(ctx->sema, e);
    if (n->tag == SIR_SIMDCONST)   { n->simd_const.lo   = lo; n->simd_const.hi   = hi; }
    if (n->tag == SIR_SIMDSHUFFLE) { n->simd_shuffle.lo = lo; n->simd_shuffle.hi = hi; }
    return 0;
}
int ddcg_sema_class_intrinsic_kind(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_class_intrinsic_kind(ctx->sema, e);
}
bool ddcg_sema_is_class_intrinsic(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return sema_is_class_intrinsic(ctx->sema, e);
}
bool ddcg_sema_class_needs_init(ddcg_ctx_t* ctx, int cls)   { return sema_class_needs_init(ctx->sema, cls); }
int  ddcg_sema_ensure_init_cp(ddcg_ctx_t* ctx, int cls)     { return sema_ensure_init_cp(ctx->sema, cls); }

bool ddcg_sema_super_method_resolved(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return sema_resolved_super_method(ctx->sema, expr) != NULL;
}

int ddcg_sema_ctor_call_target_class(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (expr->tag != AST_CONSTRUCTORCALL) return -1;
    if (expr->constructor_call.is_super) {
        const sema_class_t* cls = sema_get_class(ctx->sema, ctx->current_class_id_);
        return cls ? cls->super_id : -1;
    }
    return ctx->current_class_id_;
}

int ddcg_sema_catch_class_id(ddcg_ctx_t* ctx, ast_catch_clause_t* cc) {
    return sema_catch_class_id(ctx->sema, cc);
}

int ddcg_sema_catch_cp_classref(ddcg_ctx_t* ctx, ast_catch_clause_t* cc) {
    /* The caught type's class id (for the handler's ref.test/cast).
     * 0 (the root) if unresolved. */
    int cid = ddcg_sema_catch_class_id(ctx, cc);
    return cid < 0 ? 0 : cid;
}

int ddcg_sema_catch_slot(ddcg_ctx_t* ctx, ast_catch_clause_t* cc) {
    int slot = sema_slot(ctx->sema, cc);
    if (slot >= 0) return slot;
    int t = ctx->next_temp_;
    ctx->next_temp_ += 1;
    return t;
}

int ddcg_sema_throwable_class_id(ddcg_ctx_t* ctx) {
    return ctx->sema->wk.throwable_id;
}

bool ddcg_target_is_ident(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr && expr->tag == AST_IDENT;
}

bool ddcg_target_is_static_field(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr || expr->tag != AST_FIELDACCESS) return false;
    return ddcg_sema_field_is_static(ctx, expr);
}

bool ddcg_target_is_instance_field(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr || expr->tag != AST_FIELDACCESS) return false;
    return !ddcg_sema_field_is_static(ctx, expr);
}

bool ddcg_target_is_array_access(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr && expr->tag == AST_ARRAYACCESS;
}

bool ddcg_target_is_super_access(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr && expr->tag == AST_SUPERACCESS;
}

/* Dybvig §3.2 Figure 8: a "simple expression" is a side-effect-free
 * leaf — literal, `this`, local/param ident, or `-literal`. Drives
 * the four-case binop dispatch in compiler.ddcg. The classification
 * mirrors cg_operand_tree's match clauses; keep them in sync. */
bool ddcg_is_simple_operand(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr) return false;
    switch (expr->tag) {
        case AST_INTLIT:
        case AST_BOOLLIT:
        case AST_NULLLIT:
        case AST_THIS:
            return true;
        case AST_IDENT: {
            const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
            return info && (info->kind == SEMA_IDENT_LOCAL
                         || info->kind == SEMA_IDENT_PARAM);
        }
        case AST_UNARY:
            return expr->unary.op == AST_NEG
                && expr->unary.e
                && expr->unary.e->tag == AST_INTLIT;
        default:
            return false;
    }
}

/* Side-effect-immutable subset of is_simple_operand — drops Ident
 * because a slot value can change mid-expression. Used by Figure 8
 * case 2 (LHS simple, RHS complex), where inlining a slot-reading
 * LHS would defer the read past RHS's mutations. */
bool ddcg_is_constant_operand(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    if (!expr) return false;
    switch (expr->tag) {
        case AST_INTLIT:
        case AST_BOOLLIT:
        case AST_NULLLIT:
        case AST_THIS:
            return true;
        case AST_UNARY:
            return expr->unary.op == AST_NEG
                && expr->unary.e
                && expr->unary.e->tag == AST_INTLIT;
        default:
            return false;
    }
}

ast_expr_t* ddcg_field_acc_obj(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr->field_access.obj;
}

ast_expr_t* ddcg_array_acc_arr(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr->array_access.arr;
}

ast_expr_t* ddcg_array_acc_index(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    (void)ctx;
    return expr->array_access.index;
}

bool ddcg_fieldacc_is_array_length(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return ddcg_sema_is_array_length(ctx, expr);
}

bool ddcg_fieldacc_is_static(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    return ddcg_sema_field_is_static(ctx, expr);
}

bool ddcg_ident_is_local_or_param(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    if (!info) return false;
    return info->kind == SEMA_IDENT_LOCAL || info->kind == SEMA_IDENT_PARAM;
}

bool ddcg_ident_is_instance_field(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    return info && info->kind == SEMA_IDENT_INSTANCE_FIELD;
}

bool ddcg_ident_is_static_field(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    const sema_ident_info_t* info = sema_ident_kind(ctx->sema, expr);
    return info && info->kind == SEMA_IDENT_STATIC_FIELD;
}

/* Translate the type lattice's numeric-conversion decision into the ddcg cast
 * kind the cast lowering matches on. The DECISION is the lattice's
 * (`lat_num_conv`); this is the trivial enum mapping. */
static sema_cast_kind_t lat_conv_to_cast_kind(ddcg_ctx_t* ctx, lat_conv_t c) {
    switch (c) {
    case LAT_CONV_I2L: return CastI2L(ctx);
    case LAT_CONV_I2F: return CastI2F(ctx);
    case LAT_CONV_I2D: return CastI2D(ctx);
    case LAT_CONV_L2I: return CastL2I(ctx);
    case LAT_CONV_L2F: return CastL2F(ctx);
    case LAT_CONV_L2D: return CastL2D(ctx);
    case LAT_CONV_F2I: return CastF2I(ctx);
    case LAT_CONV_F2L: return CastF2L(ctx);
    case LAT_CONV_F2D: return CastF2D(ctx);
    case LAT_CONV_D2I: return CastD2I(ctx);
    case LAT_CONV_D2L: return CastD2L(ctx);
    case LAT_CONV_D2F: return CastD2F(ctx);
    case LAT_CONV_I2B: return CastI2B(ctx);
    case LAT_CONV_I2S: return CastI2S(ctx);
    case LAT_CONV_I2C: return CastI2C(ctx);
    case LAT_CONV_S2I: return CastS2I(ctx);
    case LAT_CONV_S2B: return CastS2B(ctx);
    case LAT_CONV_L2B: return CastL2B(ctx);
    case LAT_CONV_L2S: return CastL2S(ctx);
    case LAT_CONV_L2C: return CastL2C(ctx);
    case LAT_CONV_F2B: return CastF2B(ctx);
    case LAT_CONV_F2S: return CastF2S(ctx);
    case LAT_CONV_F2C: return CastF2C(ctx);
    case LAT_CONV_D2B: return CastD2B(ctx);
    case LAT_CONV_D2S: return CastD2S(ctx);
    case LAT_CONV_D2C: return CastD2C(ctx);
    case LAT_CONV_NONE:
    case LAT_CONV_IDENTITY:
    default:           return CastIdentity(ctx);   /* no conversion node */
    }
}

sema_cast_kind_t ddcg_sema_cast_kind(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr || expr->tag != AST_CAST || !expr->cast.ty)
        return CastNoop(ctx);
    /* §5.5 reference cast. Read the PACKAGED target type (a cast expression's static type IS
     * its target) and let the type lattice + §10.8 array Classes resolve the representation —
     * never re-derive it from the AST here. */
    java_type_t tgt = sema_type_of(ctx->sema, expr);
    if (tgt.tag == JT_CLASS)
        return CastClass(ctx, tgt.class_id);
    if (tgt.tag == JT_ARRAY && tgt.element) {
        /* A single-dim array of a NON-REFERENCE element IS precisely its PrimArray overlay →
         * plain ref.cast the struct. A reference/multi-dim array shares one RefArray struct, so
         * its element type needs the §15.19.2 runtime Class.isInstance check before the
         * (structural) ref.cast. The §4.2 predicate is the divider, not a list of tags: V128 is
         * not a JLS numeric type but it is the 8th packed width and has its own overlay, and
         * spelling the rule as "numeric or boolean" sent V128[] down the reflect path to a
         * structural cast at RefArray — a type mismatch the module validator rejected. */
        if (!jt_is_reference(*tgt.element)) {
            int overlay = lat_array_overlay_class(ctx->sema, tgt);
            if (overlay >= 0) return CastClass(ctx, overlay);
        }
        return CastArrayReflect(ctx, sema_array_class_id(ctx->sema, tgt));
    }

    /* Numeric primitive cast → the one conversion authority (§5.1.2/§5.1.3). */
    int32_t target_dt = sema_data_type(ctx->sema, expr);
    if (target_dt < 0) target_dt = SIR_DTINT;
    int32_t src_dt = sema_data_type(ctx->sema, expr->cast.e);
    if (src_dt < 0) src_dt = target_dt;
    return lat_conv_to_cast_kind(ctx,
               lat_num_conv((sir_datatype_t)src_dt, (sir_datatype_t)target_dt));
}

/* §5.2 assignment conversion: the implicit numeric conversion when delivering `v`
 * to a context of declared type `target_dt` (return, assignment, arg, …). Same
 * authority as the explicit cast — `lat_num_conv` — so they can never diverge.
 * CastIdentity/CastNoop ⇒ no conversion node. */
sema_cast_kind_t ddcg_assign_conv(ddcg_ctx_t* ctx, ast_expr_t* v, sir_datatype_t target_dt) {
    int32_t src_dt = sema_data_type(ctx->sema, v);
    if (src_dt < 0) src_dt = (int32_t)target_dt;
    return lat_conv_to_cast_kind(ctx,
               lat_num_conv((sir_datatype_t)src_dt, target_dt));
}

/* §5.6.2 binary / §5.6.1 unary numeric promotion computation types (the lattice
 * is the authority) and the (from→to) conversion realization for promoting a
 * binop operand subtree — same `lat_num_conv` decision as assignment/casts, so
 * no path re-decides conversions. */
sir_datatype_t ddcg_dt_promote(ddcg_ctx_t* ctx, sir_datatype_t a, sir_datatype_t b) {
    (void)ctx; return lat_promote_dt(a, b);
}
sir_datatype_t ddcg_dt_unary(ddcg_ctx_t* ctx, sir_datatype_t a) {
    (void)ctx; return lat_unary_promote_dt(a);
}
sema_cast_kind_t ddcg_num_conv(ddcg_ctx_t* ctx, sir_datatype_t from, sir_datatype_t to) {
    return lat_conv_to_cast_kind(ctx, lat_num_conv(from, to));
}

sema_instanceof_kind_t ddcg_sema_instanceof_kind(ddcg_ctx_t* ctx, ast_expr_t* expr) {
    if (!expr || expr->tag != AST_INSTANCEOF || !expr->instance_of.ty)
        return InstanceOfAlwaysFalse(ctx);

    /* Read the PACKAGED target type sema resolved (§15.19.2). The lattice + §10.8 array Classes
     * decide the check; a single-dim primitive array is precise (ref.test its overlay), a
     * reference/multi-dim array needs the runtime element-precise Class.isInstance. */
    java_type_t tgt = sema_instanceof_type(ctx->sema, expr);
    if (tgt.tag == JT_CLASS)
        return InstanceOfClass(ctx, tgt.class_id);
    if (tgt.tag == JT_ARRAY && tgt.element) {
        /* Same divider as the cast above, and for the same reason — one rule, both sites. */
        if (!jt_is_reference(*tgt.element)) {
            int overlay = lat_array_overlay_class(ctx->sema, tgt);
            if (overlay >= 0) return InstanceOfClass(ctx, overlay);
        }
        return InstanceOfArrayReflect(ctx, sema_array_class_id(ctx->sema, tgt));
    }
    return InstanceOfAlwaysFalse(ctx);
}

/* ── new-array (incl. multi-dimensional) support ──────────────────────────
 * `NewArray` carries the base element type + one `dims` entry per bracket level
 * (NULL = unsized `[]`). These leaf queries feed the `cg_new_array` lowering. */

/* The fully-unwrapped base element type of a new-array (peeling `rank` levels). */
static java_type_t newarr_base(ddcg_ctx_t* ctx, ast_expr_t* node) {
    java_type_t t = sema_type_of(ctx->sema, node);
    int rank = node->new_array.dims_count;
    for (int i = 0; i < rank && t.tag == JT_ARRAY && t.element; i++) t = *t.element;
    return t;
}

/* Total bracket levels (array rank). */
int ddcg_array_rank(ddcg_ctx_t* ctx, ast_expr_t* node) {
    (void)ctx;
    return node->new_array.dims_count;
}

/* Number of leading SIZED dims (the non-NULL prefix); trailing unsized dims are
 * left null at runtime. (JLS forbids a sized dim after an unsized one.) */
int ddcg_array_sized_dims(ddcg_ctx_t* ctx, ast_expr_t* node) {
    (void)ctx;
    int s = 0, rank = node->new_array.dims_count;
    for (int i = 0; i < rank; i++) { if (!node->new_array.dims[i]) break; s++; }
    return s;
}

/* The element referent of an array whose element is the base wrapped `depth`
 * times (depth 0 == the base itself); NULL when that element is a primitive (the
 * caller then allocates a primitive array via the atype). */
sir_node_t* ddcg_array_eref_at(ddcg_ctx_t* ctx, ast_expr_t* node, int depth) {
    java_type_t cur = newarr_base(ctx, node);
    for (int i = 0; i < depth; i++) {
        java_type_t* ep = (java_type_t*)bbq_arena_alloc(ctx->arena, sizeof *ep);
        *ep = cur; cur = jt_array(ep);
    }
    return ddcg_ref_descriptor(ctx, cur);
}

/* The SIR atype for the base (used for a primitive innermost array). */
sir_atype_t ddcg_array_base_atype(ddcg_ctx_t* ctx, ast_expr_t* node) {
    return lat_tag_to_atype(newarr_base(ctx, node).tag);
}

/* The size expression for sized dimension `i` (NULL for an unsized `[]`). */
ast_expr_t* ddcg_array_dim(ddcg_ctx_t* ctx, ast_expr_t* node, int i) {
    (void)ctx;
    return (i >= 0 && i < node->new_array.dims_count) ? node->new_array.dims[i] : NULL;
}

/* Stamp source location onto a SIR spine head before the dispatcher
 * returns. ddcgc-recognised magic AUX: see compiler.ddcg's declaration.
 * Mirrors the legacy compiler's wrapper — nodes the rule body builds
 * but didn't explicitly stamp inherit the dispatching AST node's loc
 * (held in ctx->current_loc, which the dispatcher entry sets before
 * the rule body runs). */
sir_node_t* ddcg_on_dispatch_result(ddcg_ctx_t* ctx, sir_node_t* n) {
    /* sir_srcloc and ast_srcloc are distinct types with the same
     * field shape (file/line/col) — asdl emits one per schema and
     * doesn't share. Field-by-field copy bridges them. */
    if (n && n->loc.line == 0) {
        n->loc.file = ctx->current_loc.file;
        n->loc.line = ctx->current_loc.line;
        n->loc.col  = ctx->current_loc.col;
    }
    return n;
}

/* ── THE SIDECAR: one row, one accumulator ───────────────────
 *
 * The DDCG is the stage that KNOWS — which branch is a guard and what it tests,
 * which allocation site sits in a loop, where a scope ends, which handlers enclose
 * a Throw. It has every one of these in hand at the instant it builds the node, so
 * it SAYS SO here, and later stages READ it. An optimizer that recovers any of this
 * by walking the SIR is a second authority for a fact the frontend owns — spec §8,
 * "Why there is no dominator tree".
 *
 * ONE row type (compiler_fact_t), ONE accumulator (ctx->facts_), ONE getter
 * (compiler_get_facts). The functions below are its TYPED CONSTRUCTORS — they exist
 * so a guard cannot be recorded with a scope's payload by mistake, not because the
 * kinds have different storage. They do not.
 *
 * The DSL holds the accumulator as void* to keep compiler.h out of its preamble;
 * bbq_vec_push needs a real T* lvalue, hence the cast dance in one place. */
static int ddcg_fact_push(ddcg_ctx_t* ctx, sir_node_t* key, sir_node_t* aux,
                          int kind, int a, int b, int c, int d) {
    compiler_fact_t* vec = (compiler_fact_t*)ctx->facts_;
    compiler_fact_t f;
    f.key  = key;
    f.aux  = aux;
    f.kind = kind;
    f.a = a; f.b = b; f.c = c; f.d = d;
    bbq_vec_push(vec, f);
    ctx->facts_ = vec;
    return 0;
}

int ddcg_record_try_region(ddcg_ctx_t* ctx, sir_node_t* try_start,
                            sir_node_t* handler, int catch_type, int region) {
    return ddcg_fact_push(ctx, try_start, handler,
                          COMPILER_FACT_TRY_REGION, catch_type, region, 0, 0);
}

/* A fresh region id, unique within the method. Allocated BEFORE the try's body is
 * compiled — the Throws in the body must name the region while its ExceptionEntry
 * handlers do not exist yet (they wrap the compiled body). Same shape as the temp-slot
 * allocator; compiler.c resets it per method. */
int ddcg_next_region_id(ddcg_ctx_t* ctx) {
    return ctx->next_region_++;
}

/* An EXCEPTING node — one the JLS specifies can throw — enclosed by region `region`.
 * One row per enclosing region, emitted by record_except_regions walking ρ. WE know the
 * enclosure because we are building the try; the optimizer must never recover it by
 * walking the SIR (§8), and must never CLASSIFY nodes to decide which can throw (that
 * taxonomy would be a second effect authority — the same disease).
 *
 * Two consumers, both spec-mandated:
 *   §1  the handler MERGES the state at every excepting point of its region. JLS §11.3.1:
 *       exceptions are precise — "all effects of the statements executed and expressions
 *       evaluated BEFORE the point from which the exception is thrown must appear to have
 *       taken place". These rows ARE that merge's φ inputs.
 *   §6  an exception object caught by one of these regions never leaves the method.
 *
 * region < 0 means "this frame carries no handlers" — the finally_frame a CATCH body
 * runs under. A catch body sits PAST the try_table, so the try it belongs to cannot
 * catch what it throws; recording a row for it would be fail-OPEN. Drop it: the node is
 * then covered only by whatever ρ's parent chain names, which is exactly right. */
int ddcg_record_except(ddcg_ctx_t* ctx, sir_node_t* node, int region) {
    if (region < 0) return 0;
    return ddcg_fact_push(ctx, node, NULL,
                          COMPILER_FACT_EXCEPT_REGION, region, 0, 0, 0);
}

/* Stamp the exception continuation (spec §1's second γ) onto every node recorded
 * excepting under `region` — called by the try rule the moment its handler chain
 * exists (build order: handlers are built AFTER the body they protect, so the mint
 * sites could only record the region ID; this closes the loop).
 *
 * FIRST WRITE WINS: rules evaluate inside-out, so an inner try patches before its
 * enclosing one — the surviving stamp is the INNERMOST enclosing region, which is
 * where control actually lands (every chain ends in a catch-all; outer regions are
 * reached via that catch-all's rethrow, which records its own row). Walks the fact
 * ROWS — never the graph. */
int ddcg_patch_excepts(ddcg_ctx_t* ctx, int region, sir_node_t* chain) {
    compiler_fact_t* v = (compiler_fact_t*)ctx->facts_;
    for (int i = 0; i < (int)bbq_vec_len(v); i++)
        if (v[i].kind == COMPILER_FACT_EXCEPT_REGION && v[i].a == region
                && v[i].key && v[i].key->exc == NULL)
            v[i].key->exc = chain;
    return 0;
}

int ddcg_record_scope(ddcg_ctx_t* ctx, sir_node_t* header,
                       sir_node_t* exit, int kind) {
    return ddcg_fact_push(ctx, header, exit,
                          COMPILER_FACT_SCOPE, kind, 0, 0, 0);
}

int ddcg_record_guard(ddcg_ctx_t* ctx, sir_node_t* branch, int kind,
                       int subject_slot, int aux_slot, int throw_on_true) {
    return ddcg_fact_push(ctx, branch, NULL, COMPILER_FACT_GUARD,
                          kind, subject_slot, aux_slot, throw_on_true);
}

int ddcg_record_alloc(ddcg_ctx_t* ctx, sir_node_t* node, int in_loop) {
    return ddcg_fact_push(ctx, node, NULL,
                          COMPILER_FACT_ALLOC, in_loop, 0, 0, 0);
}

/* ── Switch helpers ─────────────────────────────────────────── */

int ddcg_sema_switch_default_idx(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    const sema_switch_info_t* info = sema_switch_info(ctx->sema, stmt);
    return info ? info->default_idx : -1;
}

/* §14.19 is structural over the AST, but its special treatment of a constant-`true` condition
 * reads §15.27, which needs sema to resolve a name to a final variable's initializer. */
bool ddcg_sema_can_complete_normally(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    return jls_can_complete_normally(ctx->sema, stmt);
}

bool ddcg_sema_is_constant_true(ddcg_ctx_t* ctx, ast_expr_t* e) {
    return jls_is_constant_true(ctx->sema, e);
}

bool ddcg_sema_switch_is_default_only(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    const sema_switch_info_t* info = sema_switch_info(ctx->sema, stmt);
    return info && info->degeneracy == SEMA_SWITCH_DEFAULT_ONLY;
}

sir_datatype_t ddcg_sema_switch_selector_dt(ddcg_ctx_t* ctx, ast_stmt_t* stmt) {
    /* The switch selector (byte/short/char/int) is consumed as the i32 value the
     * br_table / comparison chain operates on — one width on WASM. */
    (void)ctx; (void)stmt;
    return SIR_DTINT;
}

/* Construct the SIR_SWITCH node. body_heads is parallel to the AST
 * cases[] array (head[i] = compiled head of cases[i]'s body); sema
 * gives us sorted case_values + parallel case_ast_indices that map
 * each sorted slot back to its AST case. The default target lands
 * on the default body's head if there's a default case, else on
 * Lbreak. */
sir_node_t* ddcg_build_switch_node(ddcg_ctx_t* ctx, ast_stmt_t* stmt,
                                    ddcg_list_sir_node_t_ptr_t body_heads,
                                    sir_node_t* Lbreak,
                                    sir_node_t* selector,
                                    sir_datatype_t sel_dt) {
    const sema_switch_info_t* info = sema_switch_info(ctx->sema, stmt);
    int n = info ? info->cases_count : 0;

    sir_node_t** case_targets = (n > 0)
        ? (sir_node_t**)bbq_arena_alloc(ctx->arena, (size_t)n * sizeof(sir_node_t*))
        : NULL;
    int32_t* case_values = (n > 0)
        ? (int32_t*)bbq_arena_alloc(ctx->arena, (size_t)n * sizeof(int32_t))
        : NULL;
    for (int k = 0; k < n; k++) {
        case_values[k]  = info->case_values[k];
        int ast_idx     = info->case_ast_indices[k];
        case_targets[k] = ddcg_ddcg_label(ctx, body_heads.data[ast_idx]);
    }

    sir_node_t* default_target;
    if (info && info->default_idx >= 0) {
        default_target = ddcg_ddcg_label(ctx, body_heads.data[info->default_idx]);
    } else {
        default_target = ddcg_ddcg_label(ctx, Lbreak);
    }

    return sir_switch(ctx->arena, selector,
                       case_targets, n,
                       case_values, n,
                       default_target, sel_dt);
}

