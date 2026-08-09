/* sema.c — Java 1.0 semantic analyzer implementation
 *
 * Two-pass type checker following JLS rules for Java 1.0.
 * Uses BBQ CRT: bbq_arena, bbq_htree, bbq_vec.
 */

#include "javelina/compiler/sema.h"
#include "javelina/compiler/analyses.h"
#include "javelina/compiler/jtype_meta.h"
#include "javelina/compiler/type_lattice.h"   /* the one JLS conversion authority */
#include "javelina/compiler/const_expr.h"     /* §15.27 — simd lane/mask immediates */
#include "gen/simd_intrinsics.h"              /* the generated javelina.simd table */
#include "bbq_buf.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════ */

uint32_t str_hash(const char* s) {
    uint32_t h = 5381;
    while (*s) h = h * 33 + (uint32_t)(unsigned char)*s++;
    return h ? h : 1; /* htree doesn't allow key 0 */
}

static uint32_t ptr_key(const void* p) {
    uint32_t k = (uint32_t)(uintptr_t)p;
    return k ? k : 1;
}

static void diag(sema_ctx_t* ctx, diag_level_t level, ast_srcloc loc,
                 const char* fmt, ...) {
    sema_diag_t d = {0};
    d.level = level;
    d.loc = loc;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d.message, sizeof(d.message), fmt, ap);
    va_end(ap);
    bbq_vec_push(ctx->diags, d);
}

#define sema_error(ctx, loc, ...) diag((ctx), DIAG_ERROR, (loc), __VA_ARGS__)

static java_type_t* arena_type(sema_ctx_t* ctx, java_type_t t) {
    java_type_t* p = (java_type_t*)bbq_arena_alloc(ctx->arena, sizeof(java_type_t));
    *p = t;
    return p;
}

/* §10.2: a declarator's own bracket pairs add to the type written before the name, so
 * `int v[][]` and `int[] v[]` and `int[][] v` all declare int[][]. `dims` is the COUNT of
 * those pairs, not a flag — wrapping once regardless of the count would silently give
 * `int v[][]` the type int[]. */
static java_type_t declarator_type(sema_ctx_t* ctx, java_type_t base, int32_t dims) {
    for (int32_t i = 0; i < dims; i++) base = jt_array(arena_type(ctx, base));
    return base;
}

static const char* arena_strdup(sema_ctx_t* ctx, const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* d = (char*)bbq_arena_alloc(ctx->arena, len);
    memcpy(d, s, len);
    return d;
}

/* Flatten a ast_name_t AST node to a string: "com.example.Foo" →
 * "com.example.Foo". Public so the compiler / driver can reuse the
 * same flattening sema does internally for type resolution. */
const char* sema_name_to_str(const sema_ctx_t* ctx, const ast_name_t* n) {
    if (!n) return NULL;
    if (n->tag == AST_SIMPLENAME) {
        size_t k = strlen(n->simple_name.id);
        char* d = (char*)bbq_arena_alloc(ctx->arena, k + 1);
        memcpy(d, n->simple_name.id, k + 1);
        return d;
    }
    const char* prefix = sema_name_to_str(ctx, n->qualified_name.qualifier);
    size_t plen = strlen(prefix);
    const char* id = n->qualified_name.id;
    size_t ilen = strlen(id);
    char* buf = (char*)bbq_arena_alloc(ctx->arena, plen + 1 + ilen + 1);
    memcpy(buf, prefix, plen);
    buf[plen] = '.';
    memcpy(buf + plen + 1, id, ilen);
    buf[plen + 1 + ilen] = '\0';
    return buf;
}

/* Internal alias retained so the rest of sema.c keeps reading the
 * shorter name. */
static const char* name_to_str(sema_ctx_t* ctx, const ast_name_t* n) {
    return sema_name_to_str(ctx, n);
}


/* ═══════════════════════════════════════════════════════════════
 * ast_modifier_t helpers
 * ═══════════════════════════════════════════════════════════════ */

static int modifiers_to_flags(const ast_modifier_t* mods, int count) {
    int flags = 0;
    for (int i = 0; i < count; i++) {
        switch (mods[i]) {
        case AST_PUBLIC:    flags |= ACC_PUBLIC;    break;
        case AST_PROTECTED: flags |= ACC_PROTECTED; break;
        case AST_PRIVATE:   flags |= ACC_PRIVATE;   break;
        case AST_STATIC:    flags |= ACC_STATIC;    break;
        case AST_FINAL:     flags |= ACC_FINAL;     break;
        case AST_ABSTRACT:  flags |= SEMA_ACC_ABSTRACT;  break;
        case AST_NATIVE:       flags |= ACC_NATIVE;        break;
        case AST_SYNCHRONIZED: flags |= ACC_SYNCHRONIZED;  break;
        case AST_TRANSIENT:    flags |= ACC_TRANSIENT;     break;
        case AST_VOLATILE:     flags |= ACC_VOLATILE;      break;
        }
    }
    return flags;
}

/* ═══════════════════════════════════════════════════════════════
 * Scope management
 * ═══════════════════════════════════════════════════════════════ */

static void scope_push(sema_ctx_t* ctx) {
    bbq_htree* scope = bbq_htree_create();
    bbq_vec_push(ctx->scopes, scope);
}

static void scope_pop(sema_ctx_t* ctx) {
    int n = bbq_vec_len(ctx->scopes);
    if (n > 0) {
        bbq_htree_destroy(ctx->scopes[n - 1]);
        bbq__vec_hdr(ctx->scopes)->len--;
    }
}

/* ρ-frame stack helpers — track the same scopes ddcg pushes ρ for,
 * so break/continue resolution can record the target depth at sema
 * time and codegen just walks the chain by integer count. */
static void frame_push_loop(sema_ctx_t* ctx, const ast_stmt_t* s) {
    sema_frame_t f = { SEMA_FRAME_LOOP, ctx->pending_frame_label, s };
    bbq_vec_push(ctx->frames, f);
    ctx->pending_frame_label = NULL;
}
static void frame_push_switch(sema_ctx_t* ctx, const ast_stmt_t* s) {
    sema_frame_t f = { SEMA_FRAME_SWITCH, ctx->pending_frame_label, s };
    bbq_vec_push(ctx->frames, f);
    ctx->pending_frame_label = NULL;
}
static void frame_push_labeled_block(sema_ctx_t* ctx, const char* label) {
    sema_frame_t f = { SEMA_FRAME_LABELED_BLOCK, label, NULL };
    bbq_vec_push(ctx->frames, f);
}
static void frame_pop(sema_ctx_t* ctx) {
    int n = bbq_vec_len(ctx->frames);
    if (n > 0) bbq__vec_hdr(ctx->frames)->len--;
}
/* JLS §14.15: unlabeled `break` targets the innermost enclosing
 * while/do/for/switch — NOT labeled blocks. */
static int frame_find_innermost_break_target(const sema_ctx_t* ctx) {
    int n = bbq_vec_len(ctx->frames);
    for (int i = n - 1; i >= 0; i--) {
        if (ctx->frames[i].kind == SEMA_FRAME_LOOP ||
            ctx->frames[i].kind == SEMA_FRAME_SWITCH) return i;
    }
    return -1;
}
/* JLS §14.16: unlabeled `continue` targets the innermost enclosing
 * loop only — switch and labeled blocks don't count. */
static int frame_find_innermost_loop(const sema_ctx_t* ctx) {
    int n = bbq_vec_len(ctx->frames);
    for (int i = n - 1; i >= 0; i--) {
        if (ctx->frames[i].kind == SEMA_FRAME_LOOP) return i;
    }
    return -1;
}
/* Returns frame index (0 = outermost) of labeled target, or -1. */
static int frame_find_label(const sema_ctx_t* ctx, const char* label) {
    int n = bbq_vec_len(ctx->frames);
    for (int i = n - 1; i >= 0; i--) {
        const char* fl = ctx->frames[i].label;
        if (fl && strcmp(fl, label) == 0) return i;
    }
    return -1;
}

static int32_t scope_declare(sema_ctx_t* ctx, const char* name, java_type_t type,
                              ast_srcloc loc) {
    int n = bbq_vec_len(ctx->scopes);
    if (n == 0) return -1;
    bbq_htree* top = ctx->scopes[n - 1];
    uint32_t key = str_hash(name);
    /* §14.3.2: "The name of the local variable parameter may not be redeclared as a local
     * variable or exception parameter within its scope, or a compile-time error occurs; that
     * is, HIDING THE NAME OF A LOCAL VARIABLE IS NOT PERMITTED."
     *
     * So the search covers every ENCLOSING scope, not just the innermost: `int n = 1; { int n
     * = 2; }` is an error, and checking only the top scope let it through. A local hiding a
     * FIELD is a different question and stays legal — that is §14.3.3, and §6.3.1's worked
     * example depends on it — which is why this walks the local scope stack and never consults
     * the class's members. */
    for (int i = n - 1; i >= 0; i--) {
        if (bbq_htree_search(ctx->scopes[i], key)) {
            sema_error(ctx, loc, "redeclaration of '%s'", name);
            return -1;
        }
    }
    sema_var_t* v = (sema_var_t*)bbq_arena_alloc(ctx->arena, sizeof(sema_var_t));
    v->type = type;
    v->is_final = ctx->declaring_final;
    v->is_param = ctx->declaring_params;
    v->init_expr = ctx->declaring_init;
    v->slot = ctx->next_slot++;
    bbq_htree_insert(top, key, v);
    return v->slot;
}

/* Returns the full scope entry for a name, or NULL if not in scope.
 * Used by Phase B annotations to recover slot/is_param info, and by
 * AST_IDENT analysis to distinguish locals from fields. */
static sema_var_t* scope_lookup_var(sema_ctx_t* ctx, const char* name) {
    uint32_t key = str_hash(name);
    for (int i = bbq_vec_len(ctx->scopes) - 1; i >= 0; i--) {
        sema_var_t* v = (sema_var_t*)bbq_htree_search(ctx->scopes[i], key);
        if (v) return v;
    }
    return NULL;
}

/* If target is an AST_IDENT resolving to a final local in scope,
 * emit an error. Used by AST_ASSIGN, AST_COMPOUNDASSIGN, and the
 * inc/dec cases of AST_UNARY. Field/array targets are skipped here
 * (final-field handling lives in those cases directly). */
static void check_not_final_local(sema_ctx_t* ctx, ast_expr_t* target,
                                   ast_srcloc loc) {
    if (!target || target->tag != AST_IDENT) return;
    sema_var_t* v = scope_lookup_var(ctx, target->ident.name);
    if (v && v->is_final) {
        sema_error(ctx, loc, "cannot assign to final variable '%s'",
                   target->ident.name);
    }
}

/* JLS §15.26 / §15.14 / §15.15: an assignment or inc/dec target
 * must be a variable — one of: simple name (AST_IDENT), field
 * access (AST_FIELDACCESS or AST_SUPERACCESS), or array access
 * (AST_ARRAYACCESS). Anything else (literal, call result, binary
 * expression, etc.) is illegal. Also: `array.length` is implicitly
 * final and not assignable. */
static bool is_assignable_target(const ast_expr_t* e) {
    if (!e) return false;
    if (e->tag == AST_IDENT || e->tag == AST_SUPERACCESS ||
        e->tag == AST_ARRAYACCESS)
        return true;
    if (e->tag == AST_FIELDACCESS) {
        /* Reject array.length writes specifically. */
        if (e->field_access.field &&
            strcmp(e->field_access.field, "length") == 0) {
            /* We can't easily tell here whether obj is an array,
             * but `length` is reserved for array.length and any
             * field literally named "length" on a class is rare;
             * matching by name is correct for the array case and
             * a false positive for unusual class fields. */
            return false;
        }
        return true;
    }
    return false;
}

/* §8.3.1.2: "Any attempt to assign to a final field results in a compile-time
 * error." No carve-out exists — a 1.0 final is bound by its DECLARATOR (the
 * same section requires the initializer there), so a constructor or static-
 * initializer assignment is an attempt like any other; §8.5 and §8.6 grant no
 * exception. The declarator's own initialization is not an assignment
 * expression and never reaches here. */
static void* encode_member_loc(int owner, int index);                            /* defined below */
static const sema_field_t* decode_field_loc(const sema_ctx_t* ctx, void* enc);   /* defined below */

static void check_not_final_field(sema_ctx_t* ctx, ast_expr_t* target,
                                    ast_srcloc loc) {
    if (!target) return;
    /* Resolved fields live in the resolved_fields htree, populated
     * by AST_FIELDACCESS / AST_SUPERACCESS / unqualified-ident-as-
     * field cases. */
    const sema_field_t* f = decode_field_loc(ctx,
        bbq_htree_search(ctx->resolved_fields, ptr_key(target)));
    if (!f || !(f->modifiers & ACC_FINAL)) return;
    sema_error(ctx, loc, "cannot assign to final field '%s'", f->name);
}

/* Map a Java type tag (java_type_tag_t) to a SEMA_DT_* tag. */
/* JLS type tag → SIR/SEMA dt. The one map lives in the type lattice (SEMA_DT_*
 * and SIR_DT* share ordinals); this forwards. */
static inline int32_t type_tag_to_dt(int32_t tag) {
    return (int32_t)lat_tag_to_dt(tag);
}

/* Returns true if the expression has any observable side effect:
 * call, allocation, assignment, compound assign, inc/dec,
 * array/field access (may throw), div/rem (may throw), checked
 * cast, or any subexpression containing one of those.
 *
 * Children must already be in side_effects before this is called
 * (ensured by analyze_expr's post-order walk). */
static bool compute_side_effects(const sema_ctx_t* ctx, const ast_expr_t* e) {
    if (!e) return false;

    #define CHILD(x) ((x) != NULL && \
                      bbq_htree_search(ctx->side_effects, ptr_key(x)) != NULL)

    switch (e->tag) {
    /* Always effectful */
    case AST_METHODCALL:
    case AST_SUPERCALL:
    case AST_CONSTRUCTORCALL:
    case AST_NEW:
    case AST_NEWARRAY:
    case AST_ARRAYINIT:
    case AST_ASSIGN:
    case AST_COMPOUNDASSIGN:
    case AST_ARRAYACCESS:
    case AST_FIELDACCESS:
    case AST_SUPERACCESS:
        return true;

    case AST_UNARY:
        if (e->unary.op == AST_PREINC || e->unary.op == AST_PREDEC ||
            e->unary.op == AST_POSTINC || e->unary.op == AST_POSTDEC)
            return true;
        return CHILD(e->unary.e);

    case AST_BINARY:
        if (e->binary.op == AST_DIV || e->binary.op == AST_REM)
            return true;
        return CHILD(e->binary.lhs) || CHILD(e->binary.rhs);

    case AST_CAST:
        /* CheckCast on a reference type may throw CCE; numeric
         * narrowing (S2B, I2S, ...) is pure. */
        if (e->cast.ty && (e->cast.ty->tag == AST_CLASSTYPE ||
                           e->cast.ty->tag == AST_ARRAYTYPE))
            return true;
        return CHILD(e->cast.e);

    case AST_INSTANCEOF:
        /* JVM instanceof returns false on null, never throws. */
        return CHILD(e->instance_of.e);

    case AST_TERNARY:
        return CHILD(e->ternary.test) ||
               CHILD(e->ternary.then) || CHILD(e->ternary.else_);

    case AST_IDENT: {
        /* Bare ident resolving to an instance/static field is field
         * access — may throw NPE for instance fields, may trigger
         * class init for static fields. Treat both as effectful. */
        const sema_ident_info_t* info = sema_ident_kind(ctx, e);
        if (info && (info->kind == SEMA_IDENT_INSTANCE_FIELD ||
                     info->kind == SEMA_IDENT_STATIC_FIELD))
            return true;
        return false;
    }

    /* Pure leaves */
    case AST_INTLIT:
    case AST_BOOLLIT:
    case AST_NULLLIT:
    case AST_THIS:
    case AST_SUPER:
        return false;

    default:
        return false;
    }
    #undef CHILD
}

/* ═══════════════════════════════════════════════════════════════
 * ast_type_t resolution: AST ast_type_t* → java_type_t
 * ═══════════════════════════════════════════════════════════════ */

static void sema_register_array_type(sema_ctx_t* ctx, java_type_t arr);   /* §10.8; defined below */

/* The compilation unit of the class currently being analyzed — the §6.5.4
 * resolution context. -1 during synthetic-class processing (FQN-or-unnamed). */
static int cur_unit(const sema_ctx_t* ctx) {
    return (ctx->current_class_id >= 0 &&
            ctx->current_class_id < (int)bbq_vec_len(ctx->classes))
         ? ctx->classes[ctx->current_class_id].unit_idx : -1;
}

static java_type_t resolve_type(sema_ctx_t* ctx, const ast_type_t* t, ast_srcloc loc) {
    if (!t) return jt_error();
    switch (t->tag) {
    case AST_BYTETYPE:  return jt_prim(JT_BYTE);
    case AST_SHORTTYPE: return jt_prim(JT_SHORT);
    case AST_CHARTYPE:  return jt_prim(JT_CHAR);
    case AST_INTTYPE:   return jt_prim(JT_INT);
    case AST_LONGTYPE:  return jt_prim(JT_LONG);
    case AST_FLOATTYPE: return jt_prim(JT_FLOAT);
    case AST_DOUBLETYPE: return jt_prim(JT_DOUBLE);
    case AST_BOOLTYPE:  return jt_prim(JT_BOOL);
    case AST_VOIDTYPE:  return jt_prim(JT_VOID);
    case AST_CLASSTYPE: {
        int id = sema_resolve_type(ctx, cur_unit(ctx),
                                   name_to_str(ctx, t->class_type.name), loc, false);
        if (id < 0) return jt_error();   /* §6.5.4 already errored */
        /* Record THIS node's resolution — post-sema queries read it. */
        bbq_htree_insert(ctx->type_class_ids, ptr_key(t), (void*)(uintptr_t)(id + 1));
        /* javelina.simd.V128 is the v128 VALUE type — every use site gets the
         * value width, never a reference. This one hook is what makes a V128
         * non-nullable, non-Object, ==-less: the JLS checks downstream reject a
         * non-numeric non-reference primitive on their own. */
        if (id == ctx->wk.v128_id) return jt_prim(JT_V128);
        return jt_class(id);
    }
    case AST_ARRAYTYPE: {
        java_type_t elem = resolve_type(ctx, t->array_type.element, loc);
        if (jt_is_error(elem)) return jt_error();
        java_type_t arr = jt_array(arena_type(ctx, elem));
        sema_register_array_type(ctx, arr);   /* §10.8: this array type gets a Class object */
        return arr;
    }
    }
    return jt_error();
}

/* ═══════════════════════════════════════════════════════════════
 * Conversion rules (JLS Ch 5)
 * ═══════════════════════════════════════════════════════════════ */


/* §5.1.2 widening / §5.1.3 narrowing primitive conversion — the rules live in the
 * type lattice (the one authority, spec-verified); these forward. */
static bool is_widening_prim(java_type_t from, java_type_t to) {
    return lat_is_widening_prim(from, to);
}
static bool is_narrowing_prim(java_type_t from, java_type_t to) {
    return lat_is_narrowing_prim(from, to);
}

static bool is_subclass_of(const sema_ctx_t* ctx, int sub_id, int super_id) {
    if (sub_id < 0 || super_id < 0) return false;
    int id = sub_id;
    while (id >= 0) {
        if (id == super_id) return true;
        id = ctx->classes[id].super_id;
    }
    return false;
}

/* JLS §11.2: the UNCHECKED exceptions are RuntimeException and Error (and their
 * subclasses); everything else is checked and must be declared or caught. */
static bool is_unchecked_exc(const sema_ctx_t* ctx, int class_id) {
    int rt = ctx->wk.runtime_exception_id, er = ctx->wk.error_id;
    return (rt >= 0 && is_subclass_of(ctx, class_id, rt))
        || (er >= 0 && is_subclass_of(ctx, class_id, er));
}

bool sema_is_subclass_of(const sema_ctx_t* ctx, int sub_id, int super_id) {
    return is_subclass_of(ctx, sub_id, super_id);
}

int sema_common_superclass(const sema_ctx_t* ctx, int a, int b) {
    if (a < 0 || b < 0) return -1;
    /* Walk a's extends chain, flagging ancestors; then walk b's
     * chain to find the first flagged class. O(N) per call. */
    int na = 0;
    for (int id = a; id >= 0; id = ctx->classes[id].super_id) na++;
    int* ancestors = (int*)bbq_arena_alloc(ctx->arena, (size_t)na * sizeof(int));
    int k = 0;
    for (int id = a; id >= 0; id = ctx->classes[id].super_id)
        ancestors[k++] = id;
    for (int id = b; id >= 0; id = ctx->classes[id].super_id)
        for (int i = 0; i < na; i++)
            if (ancestors[i] == id) return id;
    return -1;
}

static bool implements_interface(const sema_ctx_t* ctx, int class_id, int iface_id) {
    if (class_id < 0 || iface_id < 0) return false;
    int id = class_id;
    while (id >= 0) {
        const sema_class_t* c = &ctx->classes[id];
        for (int i = 0; i < c->interface_count; i++) {
            if (c->interface_ids[i] == iface_id) return true;
            /* `interface I extends J` records J among I's own interfaces,
             * so a class implementing I implements J transitively. Recurse
             * through the sub-interface's super-interface list (and its
             * super_id chain) — is_subclass_of alone only follows the
             * class super chain and misses interface-extends-interface. */
            if (implements_interface(ctx, c->interface_ids[i], iface_id)) return true;
        }
        id = c->super_id;
    }
    return false;
}

/* ── JLS §8.4.8 virtual dispatch — the ONE place these rules are written ──── */

bool sema_is_virtual_method(const sema_method_t* m) {
    return m && !(m->modifiers & ACC_STATIC) && !(m->modifiers & ACC_PRIVATE)
        && !m->is_constructor;
}

bool sema_same_vsig(const sema_method_t* a, const sema_method_t* b) {
    if (!a || !b) return false;
    if (strcmp(a->name, b->name) != 0 || a->param_count != b->param_count) return false;
    /* The return type is part of the slot's functype identity: a §8.4.8.1 override keeps
     * it (JLS 1.0 has no covariant returns), so this never splits a real override — but
     * it stops two UNRELATED classes' same-name/params methods with different returns
     * from sharing one functype (File.length()→long vs String.length()→int, which once
     * handed File the i32 slot). */
    if (!jt_eq(a->return_type, b->return_type)) return false;
    for (int i = 0; i < a->param_count; i++)
        if (!jt_eq(a->param_types[i], b->param_types[i])) return false;
    return true;
}

/* Does every invocation of `m` become opcodes rather than a call? A Move* bitcast, a Math
 * f64 op (sqrt/floor/ceil/rint), a Class.newInstance helper, a javelina.simd intrinsic —
 * each is replaced by the instructions it denotes, so no function is ever called and no
 * import can name one. Orthogonal to §13.1: the reference still exists (`Math.sqrt(x)`
 * references java.lang.Math.sqrt whether or not one backend has an f64.sqrt), which is why
 * the reference set records it and only the IMPORT set filters it out. */
bool sema_method_lowers_inline(const sema_method_t* m) {
    return m && (m->move_kind != 0 || m->math_kind != 0
              || m->class_kind != 0 || m->simd_id != 0);
}

/* JLS §15.12.4.4 step 1's predicate: is this declaration NON-ABSTRACT — the thing the
 * dynamic lookup stops at? §9.4 makes every interface method implicitly abstract. §8.4.3.4
 * makes `abstract native` a compile-time error, so a native declaration IS concrete and ends
 * the search, even though its body lives outside the module. */
bool sema_method_is_concrete(const sema_ctx_t* ctx, int class_id, const sema_method_t* m) {
    const sema_class_t* c = sema_get_class(ctx, class_id);
    if (!c || !m || c->is_interface) return false;
    return !(m->modifiers & SEMA_ACC_ABSTRACT);
}

/* Which method does an object of EXACTLY `exact_class_id` run for the virtual method a
 * call site named? Walk `exact`'s own methods first, then up the extends chain — the
 * nearest declaration of the signature wins (§8.4.8: a subclass override hides its
 * super's). Fails (returns false) when the signature is nowhere concrete, and a caller
 * that cannot get an answer must not devirtualize.
 *
 * §15.12.4.4 step 1 stops at "a declaration for a NON-ABSTRACT method named m with the
 * same descriptor". Non-abstract is the whole test: a native method is non-abstract
 * (§8.4.3.4 makes `abstract native` a compile-time error) and so ends the lookup, even
 * though it has no Java body. Whether THIS module emits a body for it is a separate,
 * emission-side question (sema_method_is_defined) that a caller needing one — the
 * inliner — asks for itself. */
bool sema_resolve_virtual(const sema_ctx_t* ctx, int exact_class_id,
                          int decl_class_id, int decl_method_idx,
                          int* out_class_id, int* out_method_idx) {
    if (!ctx || exact_class_id < 0 || decl_class_id < 0) return false;
    const sema_class_t* dc = sema_get_class(ctx, decl_class_id);
    if (!dc || decl_method_idx < 0
            || decl_method_idx >= (int)bbq_vec_len(dc->methods)) return false;
    const sema_method_t* want = &dc->methods[decl_method_idx];
    if (!sema_is_virtual_method(want)) return false;

    for (int id = exact_class_id; id >= 0; id = ctx->classes[id].super_id) {
        const sema_class_t* c = &ctx->classes[id];
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!sema_is_virtual_method(&c->methods[j])) continue;
            if (!sema_same_vsig(&c->methods[j], want)) continue;
            /* Step 1 matches a NON-abstract declaration only; an abstract one (a class that
             * re-abstracts an inherited method) leaves the search to step 2 — the superclass —
             * rather than ending it. */
            if (!sema_method_is_concrete(ctx, id, &c->methods[j])) break;
            if (out_class_id)  *out_class_id  = id;
            if (out_method_idx) *out_method_idx = j;
            return true;
        }
    }
    return false;
}

/* Does `class_id` override Object.finalize()? A finalizable object is reachable from the
 * finalizer, so escape analysis (§6) may never treat one as method-local. This is JLS §8.4.8
 * overriding, so it IS sema_resolve_virtual against Object.finalize — not a name lookup: the
 * answer is "the body a virtual call would land in", and that is the one machine that knows.
 * Object's own declaration is `native` (no body), so a class that does not override resolves
 * to nothing; a class that does resolves to itself. */
bool sema_class_overrides_finalize(const sema_ctx_t* ctx, int class_id) {
    if (!ctx || class_id < 0 || ctx->wk.object_id < 0 || ctx->wk.finalize_method_id < 0)
        return false;
    int impl_class = -1;
    if (!sema_resolve_virtual(ctx, class_id, ctx->wk.object_id,
                              ctx->wk.finalize_method_id, &impl_class, NULL))
        return false;                       /* nothing overrides it — Object's is native */
    return impl_class != ctx->wk.object_id;
}

/* JLS §4.10.2 between two class ids — the ONE place the rule is written. */
bool sema_ref_is_subtype(const sema_ctx_t* ctx, int sub_id, int super_id) {
    if (!ctx || sub_id < 0 || super_id < 0) return false;
    if (sub_id == super_id) return true;
    /* §5.1.4/§4.10.2: every reference type (incl. every interface) is a subtype of
     * Object — even though an interface is in nobody's extends chain. */
    if (super_id == ctx->wk.object_id) return true;
    if (is_subclass_of(ctx, sub_id, super_id)) return true;
    if (ctx->classes[super_id].is_interface &&
        implements_interface(ctx, sub_id, super_id)) return true;
    return false;
}

static bool is_widening_ref(const sema_ctx_t* ctx, java_type_t from, java_type_t to) {
    if (from.tag == JT_NULL && jt_is_reference(to)) return true;
    if (from.tag == JT_CLASS && to.tag == JT_CLASS)
        if (sema_ref_is_subtype(ctx, from.class_id, to.class_id)) return true;
    if (from.tag == JT_ARRAY) {
        /* JLS §10.2 array covariance: A[] widens to B[] iff A widens to B — element
         * REFERENCE widening only (recurse), so int[]↮long[] but String[]<:Object[]. */
        if (to.tag == JT_ARRAY)
            return jt_eq(*from.element, *to.element)
                || is_widening_ref(ctx, *from.element, *to.element);
        /* §10.7: every array is an Object and implements Cloneable. */
        if (to.tag == JT_CLASS &&
            (to.class_id == ctx->wk.object_id || to.class_id == ctx->wk.cloneable_id))
            return true;
    }
    return false;
}

/* §15.27 constant expressions, as sema's own rules need them. Both ask the ONE evaluator
 * (const_expr.h). This file used to pattern-match the AST instead — a literal, a char
 * literal, or a negated literal, plus one level of static-final indirection — which is a
 * strictly smaller language than §15.27 defines, and the gap was observable: `byte b = 40+2;`
 * was rejected though §5.2's condition ("a constant expression of type int") plainly holds.
 * The codegen side had already been moved onto the evaluator; this was the remaining copy.
 *
 * A constant's TYPE matters here, not just its value, which is why the evaluator keeps the
 * integral tags distinct and why these are two functions rather than one. */

/* §5.2's first condition, exactly: "The expression is a constant expression of type int." */
/* §5.2's constant-expression premise: "if the expression is a constant expression
 * of type byte, short, char, or int". All four, not int alone — `byte b = 'A';`
 * and `bytes[i] = '\n';` are assignment conversions of a CHAR constant, and the
 * value is what the rule range-checks, not the tag. Restricting this to JT_INT
 * rejected them as incompatible. (is_const_integral_expr below already accepts
 * the four for §14.11 case labels; this is the same set for the same reason.) */
static bool is_const_int_expr(const sema_ctx_t* ctx, const ast_expr_t* e, int32_t* out) {
    if (!e) return false;
    jls_const_t c = jls_const_eval(ctx, e);
    switch (c.tag) {
    case JT_BYTE: case JT_SHORT: case JT_CHAR: case JT_INT:
        if (out) *out = c.v.i;
        return true;
    default:
        return false;
    }
}

/* §14.11 case labels: a constant expression of any integral type, whose VALUE is then
 * range-checked against the selector. `case 'a':` on an `int` switch is legal, so this one
 * accepts every integral tag rather than int alone. */
static bool is_const_integral_expr(const sema_ctx_t* ctx, const ast_expr_t* e, int32_t* out) {
    if (!e) return false;
    jls_const_t c = jls_const_eval(ctx, e);
    switch (c.tag) {
    case JT_BYTE: case JT_SHORT: case JT_CHAR: case JT_INT:
        if (out) *out = c.v.i;
        return true;
    default:
        return false;
    }
}

static bool is_assignable(const sema_ctx_t* ctx, java_type_t target, java_type_t value,
                          bool is_constant, int32_t const_value) {
    if (jt_is_error(target) || jt_is_error(value)) return true; /* suppress cascade */
    if (jt_eq(target, value)) return true;
    if (is_widening_prim(value, target)) return true;
    if (is_widening_ref(ctx, value, target)) return true;
    /* JLS §5.2 constant narrowing, both halves of it:
     *
     *   "if the expression is a constant expression of type byte, short, char,
     *    or int: A narrowing primitive conversion may be used if the variable is
     *    of type byte, short, or char, and the value of the constant expression
     *    is representable in the type of the variable."
     *
     * SOURCE is any of the four — `byte b = 'A';` and `bytes[i] = '\n';` are
     * char constants — and TARGET is byte, short or char (char being unsigned,
     * 0..65535). Restricting the source to int rejected every char constant. */
    if (is_constant &&
        (value.tag == JT_BYTE || value.tag == JT_SHORT ||
         value.tag == JT_CHAR || value.tag == JT_INT) &&
        (target.tag == JT_BYTE || target.tag == JT_SHORT || target.tag == JT_CHAR) &&
        const_value >= jtype_min[target.tag] &&
        const_value <= jtype_max[target.tag]) {
        return true;
    }
    return false;
}

/* §15.24's second numeric bullet: "If one of the operands is of type T where T is byte, short,
 * or char, and the other operand is a constant expression of type int whose value is
 * representable in type T, then the type of the conditional expression is T."
 *
 * `t` is the candidate T and `other` is the arm that must be the int constant. The range check
 * reads jtype_min/jtype_max, the same table §5.2's constant narrowing uses above — one
 * authority for "representable in T", not a second copy of the bounds. */
static bool ternary_const_fits(const sema_ctx_t* ctx, java_type_t t, const ast_expr_t* other) {
    if (t.tag != JT_BYTE && t.tag != JT_SHORT && t.tag != JT_CHAR) return false;
    int32_t cv = 0;
    if (!is_const_int_expr(ctx, other, &cv)) return false;
    return cv >= jtype_min[t.tag] && cv <= jtype_max[t.tag];
}

/* Enough of a name for §15.24's diagnostic, which is always about two REFERENCE types. */
static const char* ternary_ref_name(const sema_ctx_t* ctx, java_type_t t) {
    if (t.tag == JT_CLASS && t.class_id >= 0) return ctx->classes[t.class_id].name;
    if (t.tag == JT_ARRAY) return "an array type";
    if (t.tag == JT_NULL)  return "the null type";
    return "a reference type";
}

/* JLS §10.6: an array initializer has no type of its own. Each element
 * expression is ASSIGNMENT-CONVERTED to the array's component type, so the test
 * is §5.2 per element — identity, widening, or a constant that fits — and
 * is_assignable is where that lives.
 *
 * This used to demand every element be an int CONSTANT, which admitted
 * `byte[] b = {1,2}` and rejected `int[] p = {aChar, aChar}`, `long[] r = {i}`
 * and `int[] t = {aByte}` — every widening, and every non-constant element. The
 * caller compares the initializer's inferred array type first, and that type is
 * taken from the FIRST element, so `{c, c}` arrives here as char[] against
 * int[]: array types do not widen, and only this path can accept it. */
static bool is_array_init_narrowable(const sema_ctx_t* ctx, java_type_t target,
                                     const ast_expr_t* init) {
    if (!init || init->tag != AST_ARRAYINIT) return false;
    if (target.tag != JT_ARRAY || !target.element) return false;
    java_type_t comp = *target.element;
    for (int i = 0; i < init->array_init.elems_count; i++) {
        const ast_expr_t* el = init->array_init.elems[i];
        /* A nested initializer is converted against the next component type
         * down: {{1,2},{3}} against int[][]. */
        if (el && el->tag == AST_ARRAYINIT) {
            if (!is_array_init_narrowable(ctx, comp, el)) return false;
            continue;
        }
        int32_t cv = 0;
        bool isc = is_const_int_expr(ctx, el, &cv);
        if (!is_assignable(ctx, comp, sema_type_of(ctx, el), isc, cv)) return false;
    }
    return true;
}

/* JLS §10.6: an array initializer's component type is the *target* (declared)
 * type, not the literal's natural type. Re-record both the element-tag (read by
 * the backend's arrayinit atype/dt) and the node's result type (read by
 * sema_type_of for the spilled-array local's ref descriptor) so every consumer
 * agrees. Recurses through every dimension: a nested initializer's component type
 * is the enclosing element type, so multi-dim inits (`int[][] a = {{1,2,3}}`) get
 * the declared width at each level, not a per-level narrowest-fit of the constants.
 * Used at every array-initializer context (local var, field). */
static void retype_array_init(sema_ctx_t* ctx, ast_expr_t* init, java_type_t array_type) {
    if (!init || init->tag != AST_ARRAYINIT) return;
    if (array_type.tag != JT_ARRAY || !array_type.element) return;
    bbq_htree_insert(ctx->array_init_elem_types, ptr_key(init),
                     (void*)(uintptr_t)array_type.element->tag);
    bbq_htree_insert(ctx->expr_types, ptr_key(init), arena_type(ctx, array_type));
    /* Descend: each element is delivered to the component type. */
    for (int i = 0; i < init->array_init.elems_count; i++)
        retype_array_init(ctx, init->array_init.elems[i], *array_type.element);
}

/* javelina.simd: an intrinsic's immediate operands (lane / const halves /
 * shuffle mask) are §15.27 compile-time constants — validated ONCE here at the
 * call site and stashed; the ddcg reads the stash as SIR payloads and never
 * re-evaluates. A non-constant or out-of-range immediate is a sema error
 * naming the parameter; there is NO runtime-lane fallback. */
static int64_t simd_const_long(jls_const_t k) {
    return k.tag == JT_LONG ? k.v.l : (int64_t)k.v.i;   /* int consts widen to the long param */
}
static void check_simd_imms(sema_ctx_t* ctx, const ast_expr_t* e,
                            const sema_method_t* m) {
    const simd_intrinsic_t* r = &simd_intrinsics[m->simd_id - 1];
    int fam = r->family;
    bool lane_fam  = (fam >= 10 && fam <= 17)            /* extract / replace */
                  ||  fam == 22 || fam == 23;            /* load/storeN_lane (128/N lanes) */
    bool bytes_fam = fam == 18 || fam == 19;             /* const / shuffle */
    if (!lane_fam && !bytes_fam) return;
    int ac = e->method_call.args_count;
    if (ac != r->nargs) return;                          /* arity error already reported */
    sema_simd_imm_t* im = bbq_arena_alloc(ctx->arena, sizeof *im);
    im->lane = 0; im->lo = 0; im->hi = 0;
    if (lane_fam) {
        const ast_expr_t* la = e->method_call.args[ac - 1];
        jls_const_t k = jls_const_eval(ctx, la);
        if (k.tag == JT_VOID) {
            sema_error(ctx, e->loc, "'%s.%s': lane must be a compile-time constant",
                       r->cls, r->method);
            return;
        }
        int32_t lane = k.v.i;
        if (lane < 0 || lane >= r->lanes) {
            sema_error(ctx, e->loc, "'%s.%s': lane %d out of range 0..%d",
                       r->cls, r->method, lane, r->lanes - 1);
            return;
        }
        im->lane = lane;
    } else {
        jls_const_t klo = jls_const_eval(ctx, e->method_call.args[ac - 2]);
        jls_const_t khi = jls_const_eval(ctx, e->method_call.args[ac - 1]);
        if (klo.tag == JT_VOID || khi.tag == JT_VOID) {
            sema_error(ctx, e->loc, "'%s.%s': the immediate halves must be "
                       "compile-time constants", r->cls, r->method);
            return;
        }
        im->lo = simd_const_long(klo);
        im->hi = simd_const_long(khi);
        if (fam == 19) {                                 /* shuffle: 16 mask bytes, each 0..31 */
            for (int b = 0; b < 16; b++) {
                uint8_t v = (uint8_t)((b < 8 ? (uint64_t)im->lo >> (8 * b)
                                             : (uint64_t)im->hi >> (8 * (b - 8))));
                if (v > 31) {
                    sema_error(ctx, e->loc, "'%s.%s': mask byte %d is %d — a "
                               "shuffle lane index must be 0..31", r->cls, r->method, b, v);
                    return;
                }
            }
        }
    }
    bbq_htree_insert(ctx->simd_imms, ptr_key(e), im);
}

/* ═══════════════════════════════════════════════════════════════
 * Class/member lookup helpers
 * ═══════════════════════════════════════════════════════════════ */

static const sema_field_t* find_field(const sema_ctx_t* ctx, int class_id,
                                    const char* name) {
    while (class_id >= 0) {
        const sema_class_t* c = &ctx->classes[class_id];
        for (int i = 0; i < bbq_vec_len(c->fields); i++) {
            if (strcmp(c->fields[i].name, name) == 0)
                return &c->fields[i];
        }
        /* JLS §8.2/§9.2: fields are inherited from SUPERINTERFACES too — a class's implemented
         * interfaces and an interface's extended interfaces (e.g. `SubIface.field` where field is
         * declared in a super-interface). The resolved field keeps its declaring-class owner/index. */
        for (int i = 0; i < c->interface_count; i++) {
            const sema_field_t* f = find_field(ctx, c->interface_ids[i], name);
            if (f) return f;
        }
        class_id = c->super_id;
    }
    return NULL;
}

/* §6.4.2: "A class may have two or more fields with the same simple name if they are declared
 * in different interfaces and inherited. An attempt to refer to any of the fields by its simple
 * name results in a compile-time error." §6.4.3 states the same for an interface extending two
 * interfaces. Counts the DISTINCT declarations of `name` reachable through interfaces.
 *
 * Two things bound it, both from the text rather than from taste:
 *   - a declaration in a class HIDES what it would otherwise inherit (§8.3), so the walk stops
 *     at the first class that declares the name and reports one;
 *   - only the INTERFACE side is counted, because that is the case §6.4.2 states. A superclass
 *     field clashing with an interface constant is a different question and this does not
 *     answer it — inventing a rule for it is what §15.24's "common supertype" was.
 *
 * A diamond reaching one interface twice contributes ONE declaration, hence the dedup by
 * pointer identity rather than by name. */
static int field_decls_via_interfaces(const sema_ctx_t* ctx, int class_id, const char* name,
                                      const sema_field_t** found, int cap, int n) {
    if (class_id < 0) return n;
    const sema_class_t* c = &ctx->classes[class_id];
    for (int i = 0; i < bbq_vec_len(c->fields); i++) {
        if (strcmp(c->fields[i].name, name) == 0) {
            const sema_field_t* f = &c->fields[i];
            for (int j = 0; j < n; j++) if (found[j] == f) return n;   /* already seen (diamond) */
            if (n < cap) found[n++] = f;
            return n;                                                  /* hides anything above */
        }
    }
    for (int i = 0; i < c->interface_count; i++)
        n = field_decls_via_interfaces(ctx, c->interface_ids[i], name, found, cap, n);
    return field_decls_via_interfaces(ctx, c->super_id, name, found, cap, n);
}

/* True when `name` is ambiguous as a SIMPLE name in class_id. Qualified access stays legal --
 * §6.4.2's error is about the simple name, and Colors.BLACK still resolves. */
static bool field_simple_name_ambiguous(const sema_ctx_t* ctx, int class_id, const char* name) {
    const sema_field_t* found[8];
    int n = field_decls_via_interfaces(ctx, class_id, name, found, 8, 0);
    return n > 1;
}

/* JLS §5.3 method invocation conversion: assignment conversion (§5.2), except an
 * int constant is never implicitly narrowed (is_constant=false takes that path). */
static bool mic_convertible(const sema_ctx_t* ctx, java_type_t to, java_type_t from) {
    return jt_is_error(from) || is_assignable(ctx, to, from, false, 0);
}

/* JLS §15.11.2.1: a declaration is applicable to a call iff its parameter count
 * equals the argument count and every argument is method-invocation-convertible to
 * the corresponding parameter type. (arg_types NULL = caller without argument
 * types — arity alone.) */
static bool sig_applicable(const sema_ctx_t* ctx, const sema_method_t* m,
                           int arg_count, const java_type_t* arg_types) {
    if (m->param_count != arg_count) return false;
    if (!arg_types) return true;
    for (int j = 0; j < arg_count; j++)
        if (!mic_convertible(ctx, m->param_types[j], arg_types[j])) return false;
    return true;
}

/* JLS §15.11.2.2: m (declared in class mc) is more specific than n (declared in nc)
 * iff mc is method-invocation-convertible to nc AND each Mj converts to the
 * corresponding Nj. Both are applicable to the same call, so the arities agree. */
static bool sig_more_specific(const sema_ctx_t* ctx,
                              const sema_method_t* m, int mc,
                              const sema_method_t* n, int nc) {
    if (!mic_convertible(ctx, jt_class(nc), jt_class(mc))) return false;
    for (int j = 0; j < m->param_count; j++)
        if (!mic_convertible(ctx, n->param_types[j], m->param_types[j])) return false;
    return true;
}

/* JLS §15.11.2.2: the most specific of the applicable candidates cand[0..ncand)
 * (each declared in cand_cls[]) is the UNIQUE maximally-specific one — applicable
 * with no OTHER candidate more specific than it. NULL when there is no candidate or
 * the choice is ambiguous (two or more maximally specific). */
static const sema_method_t* most_specific_sig(const sema_ctx_t* ctx,
        const sema_method_t* const* cand, const int* cand_cls, int ncand) {
    const sema_method_t* winner = NULL; int nmax = 0;
    for (int i = 0; i < ncand; i++) {
        bool maximal = true;
        for (int k = 0; k < ncand && maximal; k++)
            if (k != i && sig_more_specific(ctx, cand[k], cand_cls[k], cand[i], cand_cls[i]))
                maximal = false;        /* cand[k] is more specific → cand[i] not maximal */
        if (maximal) { winner = cand[i]; nmax++; }
    }
    return nmax == 1 ? winner : NULL;
}

/* JLS §15.11 method resolution: gather the applicable (name + §15.11.2.1)
 * declarations across the class and its supertypes, then choose the most specific
 * (§15.11.2.2). arg_types NULL (a caller without argument types) → first arity match. */
static const sema_method_t* find_method(const sema_ctx_t* ctx, int class_id,
                                      const char* name, int arg_count,
                                      const java_type_t* arg_types) {
    const sema_method_t* arity_match = NULL;
    /* Every applicable declaration, however many there are. A fixed array here
     * silently dropped candidates past its bound, and §15.11.2.2 picks the most
     * specific of the applicable SET — drop one and the winner can change, or an
     * ambiguity can go unreported. The old bound (32) was comfortable only
     * because java.lang's largest overload set is StringBuffer.append's ten. */
    const sema_method_t** cand = NULL; int* cand_cls = NULL;
    for (int walk = class_id; walk >= 0; walk = ctx->classes[walk].super_id) {
        const sema_class_t* c = &ctx->classes[walk];
        for (int i = 0; i < bbq_vec_len(c->methods); i++) {
            const sema_method_t* m = &c->methods[i];
            if (m->is_constructor || strcmp(m->name, name) != 0) continue;
            if (m->param_count == arg_count) {
                if (!arity_match) arity_match = m;
                if (!arg_types) { bbq_vec_free(cand); bbq_vec_free(cand_cls); return m; }
            }
            if (arg_types && sig_applicable(ctx, m, arg_count, arg_types))
                { bbq_vec_push(cand, m); bbq_vec_push(cand_cls, walk); }
        }
    }
    if (!arg_types) { bbq_vec_free(cand); bbq_vec_free(cand_cls); return arity_match; }
    const sema_method_t* best = most_specific_sig(ctx, cand, cand_cls,
                                                  (int)bbq_vec_len(cand));
    bbq_vec_free(cand); bbq_vec_free(cand_cls);
    return best ? best : arity_match;          /* arity_match: none-applicable / ambiguous → caller reports */
}

/* resolved_methods / resolved_ctors / resolved_fields store a resolved member's STABLE identity
 * — its (declaring class_id, class-local index), READ off the struct's stamped `owner`/`index`
 * (stamp_member_identity) — NOT the raw `sema_*_t*`, which goes stale when a later phase
 * (`synth_array_classes`/`synth_clone_methods`) grows and REALLOCATES a vec after `analyze_bodies`
 * captured it. Synth only APPENDS, so an already-resolved member's (class, index) never shifts;
 * decode to a fresh valid pointer on read (post-synth). idx: low 20 bits; class_id: the rest
 * (+1 so the encoded value is never 0 = htree "missing"). No pointer-range search — the identity
 * is the authority, not the address. */
static void* encode_member_loc(int owner, int index) {
    if (owner < 0 || index < 0) return NULL;
    return (void*)((((uintptr_t)(owner + 1)) << 20) | (uintptr_t)index);
}
static void* encode_method_loc(const sema_ctx_t* ctx, const sema_method_t* m) {
    (void)ctx; return m ? encode_member_loc(m->owner, m->index) : NULL;
}
static const sema_method_t* decode_method_loc(const sema_ctx_t* ctx, void* enc) {
    uintptr_t v = (uintptr_t)enc;
    if (!v) return NULL;
    int ci = (int)((v >> 20) - 1);
    int idx = (int)(v & 0xFFFFF);
    if (ci < 0 || ci >= (int)bbq_vec_len(ctx->classes)) return NULL;
    const sema_class_t* c = &ctx->classes[ci];
    if (idx < 0 || idx >= (int)bbq_vec_len(c->methods)) return NULL;
    return &c->methods[idx];
}
static const sema_field_t* decode_field_loc(const sema_ctx_t* ctx, void* enc) {
    uintptr_t v = (uintptr_t)enc;
    if (!v) return NULL;
    int ci = (int)((v >> 20) - 1);
    int idx = (int)(v & 0xFFFFF);
    if (ci < 0 || ci >= (int)bbq_vec_len(ctx->classes)) return NULL;
    const sema_class_t* c = &ctx->classes[ci];
    if (idx < 0 || idx >= (int)bbq_vec_len(c->fields)) return NULL;
    return &c->fields[idx];
}

/* JLS §15.8.2 / §15.11.2: constructor overload resolution. Constructors are not
 * inherited, so the candidates are this class's own ctors; the SAME applicability +
 * most-specific rules apply (a `new T(args)` picks the ctor as a method call would).
 * Sets *has_explicit if the class declares any ctor (false ⇒ the default no-arg ctor
 * applies, §8.8.7). arg_types NULL → first arity match (e.g. the implicit super()). */
static const sema_method_t* find_constructor(const sema_ctx_t* ctx, int class_id,
                                           int arg_count, const java_type_t* arg_types,
                                           bool* has_explicit) {
    *has_explicit = false;
    if (class_id < 0) return NULL;
    const sema_class_t* c = &ctx->classes[class_id];
    const sema_method_t* arity_match = NULL;
    const sema_method_t** cand = NULL; int* cand_cls = NULL;   /* every applicable ctor */
    for (int i = 0; i < bbq_vec_len(c->methods); i++) {
        const sema_method_t* m = &c->methods[i];
        if (!m->is_constructor) continue;
        *has_explicit = true;
        if (m->param_count == arg_count) {
            if (!arity_match) arity_match = m;
            if (!arg_types) { bbq_vec_free(cand); bbq_vec_free(cand_cls); return m; }
        }
        if (arg_types && sig_applicable(ctx, m, arg_count, arg_types))
            { bbq_vec_push(cand, m); bbq_vec_push(cand_cls, class_id); }
    }
    if (!arg_types) { bbq_vec_free(cand); bbq_vec_free(cand_cls); return arity_match; }
    const sema_method_t* best = most_specific_sig(ctx, cand, cand_cls,
                                                  (int)bbq_vec_len(cand));
    bbq_vec_free(cand); bbq_vec_free(cand_cls);
    return best ? best : arity_match;
}

/* ═══════════════════════════════════════════════════════════════
 * Pass 1: Declaration collection
 * ═══════════════════════════════════════════════════════════════ */

/* §7.4.1: the fully qualified name of type T in package P is P.T; a type in
 * the unnamed package (§7.4.2) is named by its simple name alone. */
static const char* make_fq_name(sema_ctx_t* ctx, const char* pkg, const char* simple) {
    if (!pkg) return simple;
    size_t pl = strlen(pkg), sl = strlen(simple);
    char* fq = (char*)bbq_arena_alloc(ctx->arena, pl + 1 + sl + 1);
    memcpy(fq, pkg, pl); fq[pl] = '.'; memcpy(fq + pl + 1, simple, sl + 1);
    return fq;
}

/* Register a type declaration under its FULLY QUALIFIED name — §7.5.1: "The
 * compiler keeps track of types by their fully qualified names (§6.7)."
 * `unit_idx` is the owning compilation unit; its package gives the FQN.
 * Two types with one FQN = the same type declared twice: a compile-time
 * error (previously the second silently overwrote the table slot). */
static void register_class(sema_ctx_t* ctx, ast_type_decl_t* td, int unit_idx) {
    sema_class_t sc = {0};
    sc.ast_node = td;
    sc.import_pkg = -1;
    sc.unit_idx = unit_idx;
    sc.fields = NULL;  /* bbq_vec */
    sc.methods = NULL;

    if (td->tag == AST_CLASSDECL) {
        sc.name = arena_strdup(ctx, td->class_decl.name);
        sc.modifiers = modifiers_to_flags(td->class_decl.mods,
                                          td->class_decl.mods_count);
        sc.is_interface = false;
    } else {
        sc.name = arena_strdup(ctx, td->interface_decl.name);
        sc.modifiers = modifiers_to_flags(td->interface_decl.mods,
                                          td->interface_decl.mods_count);
        sc.is_interface = true;
    }
    const char* pkg = (unit_idx >= 0) ? ctx->units[unit_idx].package : NULL;
    sc.fq_name = make_fq_name(ctx, pkg, sc.name);

    uint32_t key = str_hash(sc.fq_name);
    if (bbq_htree_contains(ctx->class_by_name, key)) {
        sema_error(ctx, (ast_srcloc){0}, "duplicate declaration of type '%s'",
                   sc.fq_name);
        return;
    }
    int id = bbq_vec_len(ctx->classes);
    bbq_htree_insert(ctx->class_by_name, key, (void*)(uintptr_t)id);
    bbq_vec_push(ctx->classes, sc);
}

/* §10 arrays: synthesize the RefArray class — the single Object-subclass every
 * REFERENCE array is represented by (so String[] and Object[] are one WASM type and
 * covariance is free). It is NOT a prelude .java class: its `data` field is a raw
 * array of the top reference type (JT_NULL element → (array (mut anyref)) at emit),
 * which has no Java source syntax. Fields, in struct order past the Class header:
 * [1] elementClass (the component type's Class, for the §10.10 store check) and
 * [2] data (the backing store). No methods (§10.7 clone/Cloneable land later). Runs
 * after resolve_wellknown (needs Object + Class ids). */
static void synth_refarray_class(sema_ctx_t* ctx) {
    ctx->wk.refarray_id = -1;
    if (ctx->wk.object_id < 0 || ctx->wk.class_reflect_id < 0) return;  /* broken prelude already flagged */
    sema_class_t sc = {0};
    sc.name         = arena_strdup(ctx, "RefArray");
    sc.fq_name      = sc.name;
    sc.unit_idx     = -1;   /* synthetic: unnamed package, no imports */
    sc.super_id     = ctx->wk.object_id;
    /* §10.7: every array implements Cloneable. (Primitive arrays get this with the
     * §10.8 primitive-array overlay — a separate migration.) */
    if (ctx->wk.cloneable_id >= 0) {
        sc.interface_ids = (int*)bbq_arena_alloc(ctx->arena, sizeof(int));
        sc.interface_ids[0] = ctx->wk.cloneable_id;
        sc.interface_count = 1;
    } else { sc.interface_ids = NULL; sc.interface_count = 0; }
    sc.modifiers    = ACC_PUBLIC | ACC_FINAL;
    sc.is_interface = false;
    sc.import_pkg   = 0;      /* JRE-internal (like the prelude): not user code, not exported */
    sc.ast_node     = NULL;   /* synthesized — resolve_hierarchy/register_members skip it */
    sc.fields = NULL; sc.methods = NULL;

    java_type_t* topref = (java_type_t*)bbq_arena_alloc(ctx->arena, sizeof *topref);
    *topref = jt_null();     /* the top reference element (emits as anyref) */
    sema_field_t ec = { .name = arena_strdup(ctx, "elementClass"),
                        .type = jt_class(ctx->wk.class_reflect_id),
                        .modifiers = ACC_PRIVATE, .index = 0, .init_expr = NULL };
    sema_field_t dt = { .name = arena_strdup(ctx, "data"),
                        .type = jt_array(topref),
                        .modifiers = ACC_PRIVATE, .index = 1, .init_expr = NULL };
    bbq_vec_push(sc.fields, ec);
    bbq_vec_push(sc.fields, dt);

    int id = (int)bbq_vec_len(ctx->classes);
    bbq_htree_insert(ctx->class_by_name, str_hash(sc.name), (void*)(uintptr_t)id);
    bbq_vec_push(ctx->classes, sc);
    ctx->wk.refarray_id = id;
}

/* §10.7/§10.8: synthesize the per-width PrimArray overlays — one `{ Class header; (array W)
 * data }` struct per WASM backing width, so a primitive array is an Object with a Class and
 * its ops reach the concrete backing via GetField (no downcast). Like RefArray they
 * implement Cloneable (§10.7) and can't be prelude .java classes (the `(array W)` data field
 * has no Java source type). Indexed by lat_prim_storage_index; the `data` element's packed
 * storagetype IS the backing width (byte→i8, short/char→i16, …). */
static void synth_primarray_class(sema_ctx_t* ctx) {
    for (int i = 0; i < 8; i++) ctx->wk.primarray_ids[i] = -1;
    if (ctx->wk.object_id < 0 || ctx->wk.class_reflect_id < 0) return;
    static const struct { java_type_tag_t rep; const char* name; } widths[8] = {
        { JT_BYTE,  "ByteArray"  }, { JT_SHORT, "ShortArray" }, { JT_CHAR,  "CharArray"  },
        { JT_INT,   "IntArray"   }, { JT_LONG,  "LongArray"  },
        { JT_FLOAT, "FloatArray" }, { JT_DOUBLE,"DoubleArray"},
        { JT_V128,  "V128Array"  },   /* javelina.simd V128[] — the 8th width */
    };
    for (int i = 0; i < 8; i++) {
        sema_class_t sc = {0};
        sc.name         = arena_strdup(ctx, widths[i].name);
        sc.fq_name      = sc.name;
        sc.unit_idx     = -1;
        sc.super_id     = ctx->wk.object_id;
        if (ctx->wk.cloneable_id >= 0) {   /* §10.7: arrays implement Cloneable */
            sc.interface_ids = (int*)bbq_arena_alloc(ctx->arena, sizeof(int));
            sc.interface_ids[0] = ctx->wk.cloneable_id;
            sc.interface_count = 1;
        } else { sc.interface_ids = NULL; sc.interface_count = 0; }
        sc.modifiers    = ACC_PUBLIC | ACC_FINAL;
        sc.is_interface = false;
        sc.import_pkg   = 0;
        sc.ast_node     = NULL;
        sc.fields = NULL; sc.methods = NULL;

        java_type_t* ep = (java_type_t*)bbq_arena_alloc(ctx->arena, sizeof *ep);
        *ep = jt_prim(widths[i].rep);   /* the backing element (packed storagetype = the width) */
        sema_field_t dt = { .name = arena_strdup(ctx, "data"),
                            /* raw-marked: the lattice maps it to the concrete (array W), never
                             * re-overlaying it into a PrimArray (cf. RefArray's jt_null backing). */
                            .type = jt_raw_array(ep),
                            .modifiers = ACC_PRIVATE, .index = 0, .init_expr = NULL };
        bbq_vec_push(sc.fields, dt);

        int id = (int)bbq_vec_len(ctx->classes);
        bbq_htree_insert(ctx->class_by_name, str_hash(sc.name), (void*)(uintptr_t)id);
        bbq_vec_push(ctx->classes, sc);
        ctx->wk.primarray_ids[i] = id;
    }
}

/* §20.3.2 JVM array-class descriptor: "[" per dimension + the element signature
 * (Z/B/C/S/I/J/F/D for a primitive, or L<fully-qualified-name>; for a class — fq_name
 * is already dotted, e.g. java.lang.Object). Arena-owned. */
static const char* sema_array_descriptor(sema_ctx_t* ctx, java_type_t arr) {
    char tmp[256]; size_t n = 0;
    java_type_t t = arr;
    while (t.tag == JT_ARRAY && t.element && n < sizeof tmp - 1) { tmp[n++] = '['; t = *t.element; }
    if (t.tag == JT_CLASS) {
        const char* fq = ctx->classes[t.class_id].fq_name;
        if (n < sizeof tmp - 1) tmp[n++] = 'L';
        for (const char* p = fq; *p && n < sizeof tmp - 2; p++) tmp[n++] = *p;
        if (n < sizeof tmp - 1) tmp[n++] = ';';
    } else {
        char c = '?';
        switch (t.tag) {
        case JT_BOOL:  c = 'Z'; break; case JT_BYTE:   c = 'B'; break; case JT_CHAR: c = 'C'; break;
        case JT_SHORT: c = 'S'; break; case JT_INT:    c = 'I'; break; case JT_LONG: c = 'J'; break;
        case JT_FLOAT: c = 'F'; break; case JT_DOUBLE: c = 'D'; break; default: break;
        }
        if (n < sizeof tmp - 1) tmp[n++] = c;
    }
    tmp[n] = '\0';
    return arena_strdup(ctx, tmp);
}

/* §10.8: record a distinct array type (and its nested array components) so its Class
 * object is synthesized after body analysis. Deduped by jt_eq. */
static void sema_register_array_type(sema_ctx_t* ctx, java_type_t arr) {
    if (arr.tag != JT_ARRAY || !arr.element) return;
    for (int i = 0; i < (int)bbq_vec_len(ctx->array_class_types); i++)
        if (jt_eq(ctx->array_class_types[i], arr)) return;
    bbq_vec_push(ctx->array_class_types, arr);
    bbq_vec_push(ctx->array_class_ids, -1);            /* filled by synth_array_classes */
    sema_register_array_type(ctx, *arr.element);       /* the component type has its own Class too */
}

/* §10.8/§20.3.2: after body analysis, synthesize one data-only Class per registered
 * array type — named by its descriptor, extending Object, implementing Cloneable
 * (§20.1.5 every array is Cloneable). An array's field-0 header points at this at
 * allocation, so getClass()/getName() are exact. */
static void synth_array_classes(sema_ctx_t* ctx) {
    if (ctx->wk.object_id < 0) return;
    for (int i = 0; i < (int)bbq_vec_len(ctx->array_class_types); i++) {
        sema_class_t sc = {0};
        sc.name         = sema_array_descriptor(ctx, ctx->array_class_types[i]);
        sc.fq_name      = sc.name;
        sc.unit_idx     = -1;
        sc.super_id     = ctx->wk.object_id;
        if (ctx->wk.cloneable_id >= 0) {
            sc.interface_ids = (int*)bbq_arena_alloc(ctx->arena, sizeof(int));
            sc.interface_ids[0] = ctx->wk.cloneable_id;
            sc.interface_count = 1;
        }
        sc.modifiers    = ACC_PUBLIC | ACC_FINAL;
        sc.is_interface = false;
        sc.import_pkg   = 0;         /* synthesized (like the overlays): no ctor emission */
        sc.ast_node     = NULL;
        sc.fields = NULL; sc.methods = NULL;
        ctx->array_class_ids[i] = (int)bbq_vec_len(ctx->classes);
        bbq_vec_push(ctx->classes, sc);
    }
}

/* §20.1.5: does class `ci` (transitively — superclass chain + interfaces) implement
 * Cloneable? Arrays and their overlays declare Cloneable directly. */
static bool class_is_cloneable(const sema_ctx_t* ctx, int ci) {
    while (ci >= 0) {
        const sema_class_t* c = &ctx->classes[ci];
        for (int i = 0; i < c->interface_count; i++) {
            if (c->interface_ids[i] == ctx->wk.cloneable_id) return true;
            if (class_is_cloneable(ctx, c->interface_ids[i])) return true;  /* interface extends Cloneable */
        }
        ci = c->super_id;
    }
    return false;
}

/* §20.1.5: synthesize the per-type internalClone override (the shallow copy) for every
 * Cloneable class and array overlay, so Object.clone()'s `this.internalClone()` reaches the
 * runtime type's copy. Object's own internalClone is the declared placeholder. */
static void synth_clone_methods(sema_ctx_t* ctx) {
    if (ctx->wk.object_id < 0 || ctx->wk.cloneable_id < 0) return;
    int nclasses = (int)bbq_vec_len(ctx->classes);
    for (int ci = 0; ci < nclasses; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->is_interface || ci == ctx->wk.object_id) continue;
        /* Real classes (ast_node) clone by field copy; §10.8 array Classes clone by array.copy
         * (they are an array's field-0 / dispatch type). The shared overlay structs never head
         * a user array, so they get nothing. */
        if (!c->ast_node && sema_array_class_overlay(ctx, ci) < 0) continue;
        if (!class_is_cloneable(ctx, ci)) continue;
        sema_method_t sm; memset(&sm, 0, sizeof sm);
        sm.name                = arena_strdup(ctx, "internalClone");
        sm.return_type         = jt_class(ctx->wk.object_id);
        sm.modifiers           = 0;                 /* package-private, like Object.internalClone */
        sm.is_synthetic_clone  = true;
        sm.ast_node            = NULL;
        sm.param_count         = 0;
        sm.max_user_slots      = 1;   /* `this` occupies slot 0; the copy's temp starts past it */
        bbq_vec_push(c->methods, sm);
    }
}

/* JLS §12.4.2: synthesize a static void `$ensure_init` per needs_init class — the lazy class-init
 * barrier target. Body generated in compile_method (build_ensure_init). Added here (like the synthetic
 * clone) so it precedes the re-stamp + function-table build. */
static void synth_ensure_init_methods(sema_ctx_t* ctx) {
    int nclasses = (int)bbq_vec_len(ctx->classes);
    for (int ci = 0; ci < nclasses; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (!c->needs_init || !c->ast_node) continue;
        sema_method_t sm; memset(&sm, 0, sizeof sm);
        sm.name                   = arena_strdup(ctx, "$ensure_init");
        sm.return_type            = jt_prim(JT_VOID);
        sm.modifiers              = ACC_STATIC;
        sm.is_synthetic_ensure_init = true;
        sm.ast_node               = NULL;
        sm.param_count            = 0;
        sm.max_user_slots         = 0;   /* static, no params; body uses no locals (state via globals) */
        bbq_vec_push(c->methods, sm);
    }
}

/* §20.3.6: synthesize `static Object $newInstance() { return new C(); }` per INSTANTIABLE class —
 * the factory `Class.newInstance` calls through the funcref its Class singleton carries.
 *
 * §12.5 opens by listing the two ways a class instance is explicitly created: evaluating a class
 * instance creation expression, and invoking newInstance. They are the SAME creation procedure, so
 * this is given a real `new C()` AST body and compiled by the one lowering the compiler already has
 * (the ddcg's `new_expr` rule). Writing its SIR by hand would be a second implementation of object
 * creation, free to disagree with the first about the §12.4.1 initialization barrier, the §12.5
 * constructor chain, or default field values — and it did.
 *
 * A class is instantiable iff `new C()` would compile: not an interface (§9), not abstract (§8.1.2),
 * and declaring (or having been given) a no-argument constructor. Everything else keeps a null
 * factory, which is how newInstance raises InstantiationException (§11.5.1.2). Synthesized
 * array/overlay classes (!ast_node) are never instantiable this way.
 *
 * Runs BEFORE analyze_bodies so the body is type-checked, slot-allocated and constructor-resolved
 * exactly like source. */
static int class_noarg_ctor_index(const sema_class_t* c) {
    for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
        if (c->methods[mi].is_constructor && c->methods[mi].param_count == 0) return mi;
    return -1;
}

static void synth_new_instance_methods(sema_ctx_t* ctx) {
    if (ctx->wk.object_id < 0) return;                     /* broken prelude already flagged */
    int nclasses = (int)bbq_vec_len(ctx->classes);
    for (int ci = 0; ci < nclasses; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (!c->ast_node || c->is_interface) continue;
        if (c->modifiers & SEMA_ACC_ABSTRACT) continue;
        if (class_noarg_ctor_index(c) < 0) continue;

        bbq_arena* a = ctx->arena;
        ast_stmt_t* ret = ast_return(a, ast_new(a, ast_simple_name(a, c->name), NULL, 0));
        ast_stmt_t* body = ast_block(a, (ast_stmt_t**)bbq_arena_alloc(a, sizeof(ast_stmt_t*)), 1);
        body->block.stmts[0] = ret;
        ast_modifier_t* mods = (ast_modifier_t*)bbq_arena_alloc(a, sizeof(ast_modifier_t));
        mods[0] = AST_STATIC;
        ast_member_t* mem = ast_method_decl(a,
            ast_class_type(a, ast_simple_name(a, ctx->classes[ctx->wk.object_id].name)),
            arena_strdup(ctx, "$newInstance"), NULL, 0, NULL, 0, body, mods, 1);

        sema_method_t sm; memset(&sm, 0, sizeof sm);
        sm.name                     = mem->method_decl.name;
        sm.return_type              = jt_class(ctx->wk.object_id);
        sm.modifiers                = ACC_STATIC;
        sm.is_synthetic_new_instance = true;
        sm.ast_node                 = mem;
        sm.param_count              = 0;
        bbq_vec_push(c->methods, sm);
    }
}

/* E7.1a: detect the program's `public static void main(String[])` and synthesize the exported
 * `$main(int argc, int base) -> int` entry wrapper (its SIR is built by build_main in compile_method).
 * The wrapper decodes argv via java.io.Startup.args and invokes main under a top-level catch(Throwable)
 * that prints the trace and returns exit code 1. Synthesized only in WHOLE/PLUGIN (a jre.wasm has no
 * program entry). Records the helper ids the compiler needs; if a helper is missing (a broken prelude)
 * no `$main` is emitted (the rest of the module still assembles). Called after body analysis + the
 * other synth passes, so it appends AND is covered by the following member-identity re-stamp. */
static void synth_main_method(sema_ctx_t* ctx) {
    ctx->wk.main_class_id = ctx->wk.main_method_id = -1;
    ctx->wk.startup_id = ctx->wk.startup_args_method_id = ctx->wk.throwable_pst_method_id = -1;
    if (ctx->mode == SEMA_MODE_RUNTIME) return;   /* the runtime module has no program entry */

    int nclasses = (int)bbq_vec_len(ctx->classes);
    for (int ci = ctx->num_library_classes; ci < nclasses && ctx->wk.main_class_id < 0; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (!c->ast_node) continue;
        for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++) {
            const sema_method_t* m = &c->methods[mi];
            if (strcmp(m->name, "main") != 0)          continue;
            if (!(m->modifiers & ACC_STATIC))          continue;
            if (m->return_type.tag != JT_VOID)         continue;
            if (m->param_count != 1)                   continue;
            java_type_t p = m->param_types[0];          /* must be String[] */
            if (p.tag != JT_ARRAY || !p.element || p.element->tag != JT_CLASS
                || p.element->class_id != ctx->wk.string_id) continue;
            ctx->wk.main_class_id = ci; ctx->wk.main_method_id = mi;
            break;
        }
    }
    if (ctx->wk.main_class_id < 0) return;         /* no entry point — nothing to synthesize */

    ctx->wk.startup_id = sema_find_class(ctx, "java.io.Startup");
    if (ctx->wk.startup_id >= 0) {
        const sema_class_t* su = &ctx->classes[ctx->wk.startup_id];
        for (int i = 0; i < (int)bbq_vec_len(su->methods); i++)
            if (strcmp(su->methods[i].name, "args") == 0) { ctx->wk.startup_args_method_id = i; break; }
    }
    if (ctx->wk.throwable_id >= 0) {
        const sema_class_t* th = &ctx->classes[ctx->wk.throwable_id];
        for (int i = 0; i < (int)bbq_vec_len(th->methods); i++)
            if (strcmp(th->methods[i].name, "printStackTrace") == 0 && th->methods[i].param_count == 0)
                { ctx->wk.throwable_pst_method_id = i; break; }
    }
    if (ctx->wk.startup_id < 0 || ctx->wk.startup_args_method_id < 0 || ctx->wk.throwable_pst_method_id < 0) {
        ctx->wk.main_class_id = ctx->wk.main_method_id = -1;   /* can't build the wrapper — emit no $main */
        return;
    }

    /* append `$main(int argc, int base) -> int` to the main class */
    sema_class_t* mc = &ctx->classes[ctx->wk.main_class_id];
    java_type_t* params = (java_type_t*)bbq_arena_alloc(ctx->arena, 2 * sizeof(java_type_t));
    params[0] = jt_prim(JT_INT); params[1] = jt_prim(JT_INT);
    sema_method_t sm; memset(&sm, 0, sizeof sm);
    sm.name              = arena_strdup(ctx, "$main");
    sm.return_type       = jt_prim(JT_INT);
    sm.modifiers         = ACC_STATIC | ACC_PUBLIC;
    sm.is_synthetic_main = true;
    sm.ast_node          = NULL;
    sm.param_types       = params;
    sm.param_count       = 2;
    sm.max_user_slots    = 2;   /* two params; body temps are allocated past this in build_main */
    bbq_vec_push(mc->methods, sm);   /* appends → main_method_id (the source main) stays valid */
}

int sema_array_class_id(const sema_ctx_t* ctx, java_type_t arr) {
    for (int i = 0; i < (int)bbq_vec_len(ctx->array_class_types); i++)
        if (jt_eq(ctx->array_class_types[i], arr)) return ctx->array_class_ids[i];
    return -1;
}

/* §10.2 the Class of an array Class's component type — String for String[], String[]'s
 * Class for String[][], or -1 for a primitive-component array (and for a non-array class).
 * Computed on demand (all array Classes exist by singleton-emission time). */
/* Is class_id one of the array VALUE overlays (RefArray / a per-width PrimArray)? These
 * carry a raw backing and clone via array.copy, unlike a plain struct copy. */
/* If class_id is a §10.8 array Class, the overlay struct its array VALUES are represented by
 * (RefArray / the per-width PrimArray), else -1. An array's field 0 — what getClass() and
 * VIRTUAL DISPATCH read — is this array Class, so the array Class (not the shared overlay,
 * which is never an array's header) must own the §20.1.5 internalClone; its body operates on
 * the overlay struct the runtime receiver actually is. */
int sema_array_class_overlay(const sema_ctx_t* ctx, int class_id) {
    for (int i = 0; i < (int)bbq_vec_len(ctx->array_class_ids); i++)
        if (ctx->array_class_ids[i] == class_id)
            return lat_array_overlay_class(ctx, ctx->array_class_types[i]);
    return -1;
}

bool sema_is_overlay(const sema_ctx_t* ctx, int class_id) {
    if (class_id == sema_refarray_id(ctx)) return true;
    for (int si = 0; si < 8; si++) if (class_id == sema_primarray_id(ctx, si)) return true;
    return false;
}

int sema_array_component_class(const sema_ctx_t* ctx, int class_id) {
    for (int i = 0; i < (int)bbq_vec_len(ctx->array_class_ids); i++) {
        if (ctx->array_class_ids[i] != class_id) continue;
        java_type_t elem = *ctx->array_class_types[i].element;
        if (elem.tag == JT_CLASS) return elem.class_id;
        if (elem.tag == JT_ARRAY) return sema_array_class_id(ctx, elem);
        return -1;   /* primitive component → no Class link (exactness via identity) */
    }
    return -1;       /* not an array Class */
}

static void resolve_wellknown(sema_ctx_t* ctx);   /* defined below; called from collect_decls */
static void resolve_wellknown_methods(sema_ctx_t* ctx);  /* getClass idx — after register_members */

/* JLS §12.4.2: does this class DECLARE its own static-initializer code — a static field with an
 * initializer, or a static-initializer block? Drives needs_init (whether the class has a `<clinit>`).
 * The barrier layer separately excludes compile-time-constant field READS per §12.4.1. */
static bool class_has_own_static_init(const sema_class_t* c) {
    ast_type_decl_t* td = c->ast_node;
    if (!td) return false;
    ast_member_t** members; int mc;
    if (td->tag == AST_CLASSDECL) { members = td->class_decl.members; mc = td->class_decl.members_count; }
    else                          { members = td->interface_decl.members; mc = td->interface_decl.members_count; }
    for (int mi = 0; mi < mc; mi++) {
        ast_member_t* m = members[mi];
        if (m->tag == AST_STATICINIT) return true;
        if (m->tag != AST_FIELDDECL) continue;
        for (int di = 0; di < m->field_decl.decls_count; di++) {
            if (!m->field_decl.decls[di]->init) continue;
            const char* nm = m->field_decl.decls[di]->name;   /* is this DECLARED field static? */
            for (int fi = 0; fi < (int)bbq_vec_len(c->fields); fi++)
                if (strcmp(c->fields[fi].name, nm) == 0 && (c->fields[fi].modifiers & ACC_STATIC))
                    return true;
        }
    }
    return false;
}

/* JLS §12.4: a class needs initialization iff it — or any superCLASS (not interface, §12.4.1) —
 * declares static-init code. Run after register_members (fields) + resolve_hierarchy (super_ids).
 * Also synthesizes each needs_init class's `$initstate` static-int field — the §12.4.2 init-state
 * slot (0=uninit, 1=in-progress, 2=done, 3=erroneous) that `T$ensure_init` reads/writes. Making it a
 * real static field reuses the whole static-field→global machinery (alloc, name-based cross-module). */
static void compute_needs_init(sema_ctx_t* ctx) {
    for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
        bool ni = false;
        for (int s = ci; s >= 0; s = ctx->classes[s].super_id)   /* self + super chain */
            if (class_has_own_static_init(&ctx->classes[s])) { ni = true; break; }
        ctx->classes[ci].needs_init = ni;
    }
    for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (!c->needs_init || !c->ast_node) continue;          /* only source classes that need init */
        sema_field_t sf; memset(&sf, 0, sizeof sf);
        sf.name = "$initstate";
        sf.type = jt_prim(JT_INT);
        sf.modifiers = ACC_STATIC;
        sf.owner = ci;
        sf.index = (int)bbq_vec_len(c->fields);
        sf.init_expr = NULL;                                   /* default 0 = uninitialized */
        bbq_vec_push(c->fields, sf);
    }
}

static void resolve_hierarchy(sema_ctx_t* ctx) {
    /* The class-tree root for implicit super linking, from the well-known
     * registry (resolve_wellknown ran first). A missing Object already raised a
     * hard error there; downstream linking proceeds with -1 and is a no-op. */
    int object_id = ctx->wk.object_id;

    for (int i = 0; i < bbq_vec_len(ctx->classes); i++) {
        sema_class_t* c = &ctx->classes[i];
        ast_type_decl_t* td = c->ast_node;
        if (!td) continue; /* imported from .exp */

        /* Resolve superclass — §6.5.4 against the declaring class's unit. */
        c->super_id = -1;
        if (td->tag == AST_CLASSDECL && td->class_decl.super_class) {
            const char* sname = name_to_str(ctx, td->class_decl.super_class);
            int sid = sema_resolve_type(ctx, c->unit_idx, sname, td->loc, false);
            if (sid < 0) {
                /* §6.5.4 already errored */
            } else {
                c->super_id = sid;
                if (ctx->classes[sid].modifiers & ACC_FINAL)
                    sema_error(ctx, td->loc,
                        "class '%s' cannot extend final class '%s'",
                        c->name, ctx->classes[sid].name);
            }
        } else if (td->tag == AST_CLASSDECL && i != object_id) {
            /* Implicit `extends Object` (but Object itself is the root). */
            if (object_id < 0) {
                sema_error(ctx, td->loc,
                    "class '%s': cannot resolve implicit super java/lang/Object",
                    c->name);
            } else {
                c->super_id = object_id;
            }
        }
        /* Interfaces leave super_id = -1. */

        /* Resolve interfaces */
        ast_name_t** ifaces;
        int iface_count;
        if (td->tag == AST_CLASSDECL) {
            ifaces = td->class_decl.interfaces;
            iface_count = td->class_decl.interfaces_count;
        } else {
            ifaces = td->interface_decl.extends_;
            iface_count = td->interface_decl.extends__count;
        }

        if (iface_count > 0) {
            c->interface_ids = (int*)bbq_arena_alloc(ctx->arena,
                (size_t)iface_count * sizeof(int));
            c->interface_count = iface_count;
            for (int j = 0; j < iface_count; j++) {
                const char* iname = name_to_str(ctx, ifaces[j]);
                int iid = sema_resolve_type(ctx, c->unit_idx, iname, td->loc, false);
                if (iid < 0) {
                    c->interface_ids[j] = -1;   /* §6.5.4 already errored */
                } else if (!ctx->classes[iid].is_interface) {
                    sema_error(ctx, td->loc,
                        "'%s' is not an interface",
                        ctx->classes[iid].name);
                    c->interface_ids[j] = -1;
                } else {
                    c->interface_ids[j] = iid;
                }
            }
        }
    }

    /* Detect cycles in class super_id chains. Walk each class up to
     * nclasses steps; if depth exceeds nclasses, the chain has a
     * cycle. Break it by clearing super_id so downstream walkers
     * (validate_abstract_impl, validate_overrides, is_subclass_of,
     * find_field, find_method, is_widening_ref, has_concrete_method)
     * don't loop. The sema error was already emitted. */
    {
        int nclasses = bbq_vec_len(ctx->classes);
        for (int i = 0; i < nclasses; i++) {
            sema_class_t* c = &ctx->classes[i];
            if (!c->ast_node) continue;  /* skip imports */
            int sid = c->super_id;
            int depth = 0;
            while (sid >= 0 && depth <= nclasses) {
                sid = ctx->classes[sid].super_id;
                depth++;
            }
            if (depth > nclasses) {
                sema_error(ctx, c->ast_node->loc,
                    "cycle in class hierarchy involving '%s'", c->name);
                c->super_id = -1;
            }
        }
    }
}

/* Forward decl: defined in the hierarchy validation section below. */
static bool params_match(const sema_method_t* a, const sema_method_t* b);

static void register_members(sema_ctx_t* ctx) {
    for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        ast_type_decl_t* td = c->ast_node;
        if (!td) continue; /* built-in class (Object) */
        /* Signature types resolve per §6.5.4 against THIS class's unit. */
        ctx->current_class_id = ci;

        ast_member_t** members;
        int member_count;
        if (td->tag == AST_CLASSDECL) {
            members = td->class_decl.members;
            member_count = td->class_decl.members_count;
        } else {
            members = td->interface_decl.members;
            member_count = td->interface_decl.members_count;
        }

        for (int mi = 0; mi < member_count; mi++) {
            ast_member_t* m = members[mi];
            switch (m->tag) {
            case AST_FIELDDECL: {
                java_type_t ftype = resolve_type(ctx, m->field_decl.ty, m->loc);
                int fmods = modifiers_to_flags(m->field_decl.mods,
                                               m->field_decl.mods_count);
                /* JLS §9.3: interface fields are implicitly public,
                 * static, final, and must have an initializer. */
                if (c->is_interface) {
                    fmods |= ACC_PUBLIC | ACC_STATIC | ACC_FINAL;
                }
                for (int di = 0; di < m->field_decl.decls_count; di++) {
                    ast_var_decl_t* vd = m->field_decl.decls[di];
                    if (c->is_interface && !vd->init) {
                        sema_error(ctx, m->loc,
                            "interface field '%s' must have an initializer",
                            vd->name);
                    }
                    /* JLS §8.3: field name must be unique within the class */
                    bool dup = false;
                    for (int fi = 0; fi < bbq_vec_len(c->fields); fi++) {
                        if (strcmp(c->fields[fi].name, vd->name) == 0) {
                            sema_error(ctx, m->loc,
                                "duplicate field '%s' in class '%s'",
                                vd->name, c->name);
                            dup = true;
                            break;
                        }
                    }
                    if (dup) continue;
                    java_type_t vtype = declarator_type(ctx, ftype, vd->dims);
                    sema_field_t sf = {
                        .name = arena_strdup(ctx, vd->name),
                        .type = vtype,
                        .modifiers = fmods,
                        .index = bbq_vec_len(c->fields),
                        .init_expr = vd->init
                    };
                    bbq_vec_push(c->fields, sf);
                }
                break;
            }
            case AST_METHODDECL: {
                java_type_t ret = resolve_type(ctx, m->method_decl.ret, m->loc);
                int mmods = modifiers_to_flags(m->method_decl.mods,
                                               m->method_decl.mods_count);
                /* WASM has no threads/monitors — the one deliberate
                 * departure from Java 1.0 (the rest is implemented). */
                if (mmods & ACC_SYNCHRONIZED)
                    sema_error(ctx, m->loc,
                        "'synchronized' is not supported (WASM has no threads)");
                int pc = m->method_decl.params_count;
                java_type_t* ptypes = NULL;
                const char** pnames = NULL;
                if (pc > 0) {
                    ptypes = (java_type_t*)bbq_arena_alloc(ctx->arena,
                        (size_t)pc * sizeof(java_type_t));
                    pnames = (const char**)bbq_arena_alloc(ctx->arena,
                        (size_t)pc * sizeof(const char*));
                    for (int pi = 0; pi < pc; pi++) {
                        ast_param_t* p = m->method_decl.params[pi];
                        ptypes[pi] = resolve_type(ctx, p->ty, m->loc);
                        pnames[pi] = arena_strdup(ctx, p->name);
                        /* JLS §8.4.1: parameter names must be unique */
                        for (int pj = 0; pj < pi; pj++) {
                            if (strcmp(pnames[pj], pnames[pi]) == 0) {
                                sema_error(ctx, m->loc,
                                    "duplicate parameter name '%s' in method '%s'",
                                    p->name, m->method_decl.name);
                                break;
                            }
                        }
                    }
                }
                /* JLS §8.4.2: method signature (name + param types) must be
                 * unique within the class. Two methods with the same erased
                 * signature are forbidden. */
                {
                    sema_method_t probe = {
                        .name = m->method_decl.name,
                        .param_types = ptypes,
                        .param_count = pc,
                    };
                    for (int si = 0; si < bbq_vec_len(c->methods); si++) {
                        if (c->methods[si].is_constructor) continue;
                        if (strcmp(c->methods[si].name, m->method_decl.name) == 0
                            && params_match(&c->methods[si], &probe)) {
                            sema_error(ctx, m->loc,
                                "duplicate method '%s' with same signature in class '%s'",
                                m->method_decl.name, c->name);
                            break;
                        }
                    }
                }
                /* Interface methods are implicitly public AND abstract (JLS §9.4) */
                if (c->is_interface) mmods |= SEMA_ACC_ABSTRACT | ACC_PUBLIC;
                java_type_t* thrown = NULL;
                int thrown_n = m->method_decl.throws__count;
                if (thrown_n > 0) {
                    thrown = (java_type_t*)bbq_arena_alloc(ctx->arena,
                        (size_t)thrown_n * sizeof(java_type_t));
                    for (int ti = 0; ti < thrown_n; ti++) {
                        const char* tn = sema_name_to_str(ctx, m->method_decl.throws_[ti]);
                        int tid = sema_resolve_type(ctx, c->unit_idx, tn, m->loc, false);
                        thrown[ti] = tid >= 0 ? jt_class(tid) : jt_error();
                    }
                }
                sema_method_t sm = {
                    .name = arena_strdup(ctx, m->method_decl.name),
                    .return_type = ret,
                    .param_types = ptypes,
                    .param_names = pnames,
                    .param_count = pc,
                    .modifiers = mmods,
                    .is_constructor = false,
                    .thrown_types = thrown,
                    .thrown_count = thrown_n,
                    .ast_node = m
                };
                bbq_vec_push(c->methods, sm);
                break;
            }
            case AST_CONSTRUCTORDECL: {
                int mmods = modifiers_to_flags(m->constructor_decl.mods,
                                               m->constructor_decl.mods_count);
                int pc = m->constructor_decl.params_count;
                java_type_t* ptypes = NULL;
                const char** pnames = NULL;
                if (pc > 0) {
                    ptypes = (java_type_t*)bbq_arena_alloc(ctx->arena,
                        (size_t)pc * sizeof(java_type_t));
                    pnames = (const char**)bbq_arena_alloc(ctx->arena,
                        (size_t)pc * sizeof(const char*));
                    for (int pi = 0; pi < pc; pi++) {
                        ast_param_t* p = m->constructor_decl.params[pi];
                        ptypes[pi] = resolve_type(ctx, p->ty, m->loc);
                        pnames[pi] = arena_strdup(ctx, p->name);
                        /* JLS §8.8.1: constructor parameter names must be unique */
                        for (int pj = 0; pj < pi; pj++) {
                            if (strcmp(pnames[pj], pnames[pi]) == 0) {
                                sema_error(ctx, m->loc,
                                    "duplicate parameter name '%s' in constructor",
                                    p->name);
                                break;
                            }
                        }
                    }
                }
                /* JLS §8.8.2: constructor signature must be unique */
                {
                    sema_method_t probe = {
                        .name = c->name,
                        .param_types = ptypes,
                        .param_count = pc,
                    };
                    for (int si = 0; si < bbq_vec_len(c->methods); si++) {
                        if (!c->methods[si].is_constructor) continue;
                        if (params_match(&c->methods[si], &probe)) {
                            sema_error(ctx, m->loc,
                                "duplicate constructor with same signature in class '%s'",
                                c->name);
                            break;
                        }
                    }
                }
                /* JLS §8.8.5: a constructor may declare a throws clause, exactly like a method —
                 * capture it so a checked exception thrown in the body is covered (else "unhandled"). */
                java_type_t* c_thrown = NULL;
                int c_thrown_n = m->constructor_decl.throws__count;
                if (c_thrown_n > 0) {
                    c_thrown = (java_type_t*)bbq_arena_alloc(ctx->arena,
                        (size_t)c_thrown_n * sizeof(java_type_t));
                    for (int ti = 0; ti < c_thrown_n; ti++) {
                        const char* tn = sema_name_to_str(ctx, m->constructor_decl.throws_[ti]);
                        int tid = sema_resolve_type(ctx, c->unit_idx, tn, m->loc, false);
                        c_thrown[ti] = tid >= 0 ? jt_class(tid) : jt_error();
                    }
                }
                sema_method_t sm = {
                    .name = arena_strdup(ctx, c->name),
                    .return_type = jt_prim(JT_VOID),
                    .param_types = ptypes,
                    .param_names = pnames,
                    .param_count = pc,
                    .modifiers = mmods,
                    .is_constructor = true,
                    .thrown_types = c_thrown,
                    .thrown_count = c_thrown_n,
                    .ast_node = m
                };
                bbq_vec_push(c->methods, sm);
                break;
            }
            case AST_STATICINIT:
                /* Static initializers are analyzed in pass 2 */
                break;
            }
        }

        /* JLS §8.8.9: a class that declares NO constructors gets a default,
         * no-argument constructor with an empty body. (Interfaces get none.)
         * The implicit super() is prepended in pass 2 like any ctor, and the
         * instance-field-init prologue runs at compile. Marked synthetic so the
         * root (Object)'s default ctor is emitted even though Object is a library
         * class — every super-chain bottoms out at Object.<init> (a no-op). */
        if (td->tag == AST_CLASSDECL) {
            bool has_ctor = false;
            for (int si = 0; si < (int)bbq_vec_len(c->methods); si++)
                if (c->methods[si].is_constructor) { has_ctor = true; break; }
            if (!has_ctor) {
                ast_stmt_t*   body = ast_block(ctx->arena, NULL, 0);
                ast_member_t* cd   = ast_constructor_decl(ctx->arena, c->name,
                                         NULL, 0, NULL, 0, body, NULL, 0);
                cd->loc = td->loc;
                sema_method_t sm; memset(&sm, 0, sizeof sm);
                sm.name                 = arena_strdup(ctx, c->name);
                sm.return_type          = jt_prim(JT_VOID);
                sm.modifiers            = c->modifiers & ACC_PUBLIC;  /* §8.8.9: class's access */
                sm.is_constructor       = true;
                sm.is_synthetic_default = true;
                sm.ast_node             = cd;
                bbq_vec_push(c->methods, sm);
            }
        }
    }
}

/* Join an import declaration's ident parts into an interned dotted name. */
static const char* import_parts_to_str(sema_ctx_t* ctx, const char** parts, int n) {
    size_t total = 0;
    for (int i = 0; i < n; i++) total += strlen(parts[i]) + 1;
    char* s = (char*)bbq_arena_alloc(ctx->arena, total);
    size_t o = 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(parts[i]);
        memcpy(s + o, parts[i], l); o += l;
        s[o++] = (i + 1 < n) ? '.' : '\0';
    }
    return s;
}

/* §7.3 intake: record one compilation unit — its package (§7.4) and its raw
 * import lists. "java.lang" is appended to the on-demand list per §7.5.3
 * ("as if the declaration import java.lang.*; appeared in each unit").
 * Validation of the lists (§7.5.1/§7.5.2) runs AFTER registration, when the
 * class table can answer whether an imported type exists. */
static int intern_unit(sema_ctx_t* ctx, const ast_program_t* prog) {
    sema_unit_t u = {0};
    u.prog = prog;
    u.package = prog->package_ ? name_to_str(ctx, prog->package_) : NULL;
    for (int i = 0; i < prog->imports_count; i++) {
        const ast_import_t* im = prog->imports[i];
        if (im->tag == AST_SINGLEIMPORT) {
            const char* s = import_parts_to_str(ctx, im->single_import.parts,
                                                im->single_import.parts_count);
            bbq_vec_push(u.singles, s);
        } else {
            const char* s = import_parts_to_str(ctx, im->wildcard_import.parts,
                                                im->wildcard_import.parts_count);
            bbq_vec_push(u.ondemands, s);
        }
    }
    bbq_vec_push(u.ondemands, "java.lang");   /* §7.5.3 automatic import */
    int idx = (int)bbq_vec_len(ctx->units);
    bbq_vec_push(ctx->units, u);
    return idx;
}

/* Is `pkg` a known package per our §7.2 host rule: the declared package of
 * some registered class, or a proper dotted prefix of one ("java" is known
 * because "java.lang" is). */
static bool package_known(const sema_ctx_t* ctx, const char* pkg) {
    size_t pl = strlen(pkg);
    for (int i = 0; i < (int)bbq_vec_len(ctx->classes); i++) {
        int u = ctx->classes[i].unit_idx;
        const char* cp = (u >= 0) ? ctx->units[u].package : NULL;
        if (!cp) continue;
        if (strncmp(cp, pkg, pl) == 0 && (cp[pl] == 0 || cp[pl] == '.')) return true;
    }
    return false;
}

/* The simple (last) segment of a dotted name. */
static const char* fq_simple(const char* fq) {
    const char* last = fq;
    for (const char* p = fq; *p; p++) if (*p == '.') last = p + 1;
    return last;
}

/* §7.5.1 / §7.5.2 import validation, per unit, after registration. */
static void validate_imports(sema_ctx_t* ctx) {
    for (int ui = 0; ui < (int)bbq_vec_len(ctx->units); ui++) {
        sema_unit_t* u = &ctx->units[ui];
        for (int i = 0; i < (int)bbq_vec_len(u->singles); i++) {
            const char* fq = u->singles[i];
            int cid = sema_find_class(ctx, fq);
            if (cid < 0) {
                sema_error(ctx, (ast_srcloc){0},
                           "import '%s' does not name a class or interface (§7.5.1)", fq);
                continue;
            }
            /* §7.5.1: if not in the current package, must be accessible (public). */
            const char* dpkg = ctx->classes[cid].unit_idx >= 0
                             ? ctx->units[ctx->classes[cid].unit_idx].package : NULL;
            bool same_pkg = (dpkg == NULL && u->package == NULL) ||
                            (dpkg && u->package && strcmp(dpkg, u->package) == 0);
            if (!same_pkg && !(ctx->classes[cid].modifiers & ACC_PUBLIC))
                sema_error(ctx, (ast_srcloc){0},
                           "imported type '%s' is not public (§7.5.1, §6.6)", fq);
            const char* simple = fq_simple(fq);
            /* Two single-type-imports with one simple name: error unless the
             * same type (the duplicate is ignored). */
            for (int j = 0; j < i; j++) {
                if (strcmp(fq_simple(u->singles[j]), simple) != 0) continue;
                if (strcmp(u->singles[j], fq) != 0)
                    sema_error(ctx, (ast_srcloc){0},
                               "conflicting imports '%s' and '%s' (§7.5.1)",
                               u->singles[j], fq);
            }
            /* Colliding with a type declared in this unit: error (unless it
             * IS that type — importing yourself is a no-op dup). */
            for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++)
                if (ctx->classes[ci].unit_idx == ui && ci != cid &&
                    strcmp(ctx->classes[ci].name, simple) == 0)
                    sema_error(ctx, (ast_srcloc){0},
                               "import '%s' conflicts with type '%s' declared in "
                               "this compilation unit (§7.5.1)", fq, simple);
        }
        for (int i = 0; i < (int)bbq_vec_len(u->ondemands); i++) {
            const char* pkg = u->ondemands[i];
            /* §7.5.2: naming the current package or java.lang is a legal,
             * ignored duplicate. Unknown package = error. */
            if (!package_known(ctx, pkg) &&
                !(u->package && strcmp(pkg, u->package) == 0))
                sema_error(ctx, (ast_srcloc){0},
                           "package '%s' in import-on-demand is not known (§7.5.2)", pkg);
        }
    }
}

static void collect_decls(sema_ctx_t* ctx, ast_program_t** units, int nunits) {
    /* Register all classes/interfaces first — one intake per §7.3 unit, each
     * type under its §7.4.1 fully qualified name. */
    for (int uix = 0; uix < nunits; uix++) {
        int ui = intern_unit(ctx, units[uix]);
        for (int i = 0; i < units[uix]->types_count; i++)
            register_class(ctx, units[uix]->types[i], ui);
    }
    validate_imports(ctx);

    /* Resolve the well-known java.lang ids ONCE, after all classes are
     * registered, BEFORE hierarchy resolution (which reads wk.object_id). */
    resolve_wellknown(ctx);

    /* §10: synthesize the RefArray + PrimArray overlays as real classes, now that
     * Object/Class are resolved. Appended last so existing class ids are stable. */
    synth_refarray_class(ctx);
    synth_primarray_class(ctx);

    /* Resolve hierarchy (superclass/interfaces) */
    resolve_hierarchy(ctx);

    /* Register fields and methods */
    register_members(ctx);
    resolve_wellknown_methods(ctx);   /* getClass()'s method index — needs the methods vec populated */
    compute_needs_init(ctx);          /* JLS §12.4 — needs super_ids + fields, both now populated */
}

/* ═══════════════════════════════════════════════════════════════
 * Hierarchy validation (between pass 1 and pass 2)
 * ═══════════════════════════════════════════════════════════════ */

/* Check if class_id has a concrete (non-abstract) method with the given
   name and param count, searching the class and its superclasses. */
static bool has_concrete_method(const sema_ctx_t* ctx, int class_id,
                                const char* name, int param_count) {
    while (class_id >= 0) {
        const sema_class_t* c = &ctx->classes[class_id];
        for (int i = 0; i < bbq_vec_len(c->methods); i++) {
            const sema_method_t* m = &c->methods[i];
            if (!m->is_constructor && !(m->modifiers & SEMA_ACC_ABSTRACT) &&
                strcmp(m->name, name) == 0 && m->param_count == param_count)
                return true;
        }
        class_id = c->super_id;
    }
    return false;
}

/* Check that a method with matching name+params has compatible types. */
static bool params_match(const sema_method_t* a, const sema_method_t* b) {
    if (a->param_count != b->param_count) return false;
    for (int i = 0; i < a->param_count; i++)
        if (!jt_eq(a->param_types[i], b->param_types[i])) return false;
    return true;
}

static void validate_modifier_combos(sema_ctx_t* ctx) {
    for (int i = 0; i < bbq_vec_len(ctx->classes); i++) {
        sema_class_t* c = &ctx->classes[i];
        if (!c->ast_node) continue;

        /* abstract + final on class is illegal */
        if ((c->modifiers & SEMA_ACC_ABSTRACT) && (c->modifiers & ACC_FINAL))
            sema_error(ctx, c->ast_node->loc,
                       "class '%s' cannot be both abstract and final", c->name);

        for (int j = 0; j < bbq_vec_len(c->methods); j++) {
            sema_method_t* m = &c->methods[j];
            if (!m->ast_node) continue;

            /* abstract + final on method is illegal */
            if ((m->modifiers & SEMA_ACC_ABSTRACT) && (m->modifiers & ACC_FINAL))
                sema_error(ctx, m->ast_node->loc,
                           "method '%s' cannot be both abstract and final", m->name);

            /* abstract + private is illegal */
            if ((m->modifiers & SEMA_ACC_ABSTRACT) && (m->modifiers & ACC_PRIVATE))
                sema_error(ctx, m->ast_node->loc,
                           "method '%s' cannot be both abstract and private", m->name);

            /* abstract + static is illegal */
            if ((m->modifiers & SEMA_ACC_ABSTRACT) && (m->modifiers & ACC_STATIC))
                sema_error(ctx, m->ast_node->loc,
                           "method '%s' cannot be both abstract and static", m->name);

            /* abstract method must not have body */
            if ((m->modifiers & SEMA_ACC_ABSTRACT) && !m->is_constructor) {
                ast_stmt_t* body = (m->ast_node->tag == AST_METHODDECL) ?
                    m->ast_node->method_decl.body : NULL;
                if (body)
                    sema_error(ctx, m->ast_node->loc,
                               "abstract method '%s' must not have a body", m->name);
            }

            /* non-abstract method in concrete class must have body */
            if (!(m->modifiers & SEMA_ACC_ABSTRACT) && !(m->modifiers & ACC_NATIVE) &&
                !c->is_interface && !m->is_constructor) {
                ast_stmt_t* body = (m->ast_node->tag == AST_METHODDECL) ?
                    m->ast_node->method_decl.body : NULL;
                if (!body)
                    sema_error(ctx, m->ast_node->loc,
                               "non-abstract method '%s' must have a body", m->name);
            }
        }
    }
}

static void validate_abstract_impl(sema_ctx_t* ctx) {
    for (int i = 0; i < bbq_vec_len(ctx->classes); i++) {
        sema_class_t* c = &ctx->classes[i];
        if (!c->ast_node) continue;
        if (c->is_interface) continue;
        if (c->modifiers & SEMA_ACC_ABSTRACT) continue; /* abstract class need not impl */

        /* Check all abstract methods from superclass chain */
        int super = c->super_id;
        while (super >= 0) {
            const sema_class_t* sc = &ctx->classes[super];
            for (int j = 0; j < bbq_vec_len(sc->methods); j++) {
                const sema_method_t* sm = &sc->methods[j];
                if (sm->is_constructor) continue;
                if (!(sm->modifiers & SEMA_ACC_ABSTRACT)) continue;
                if (!has_concrete_method(ctx, i, sm->name, sm->param_count))
                    sema_error(ctx, c->ast_node->loc,
                               "class '%s' must implement abstract method '%s'",
                               c->name, sm->name);
            }
            super = sc->super_id;
        }

        /* Check all interface methods */
        for (int ii = 0; ii < c->interface_count; ii++) {
            int iid = c->interface_ids[ii];
            if (iid < 0) continue;
            const sema_class_t* iface = &ctx->classes[iid];
            for (int j = 0; j < bbq_vec_len(iface->methods); j++) {
                const sema_method_t* im = &iface->methods[j];
                if (im->is_constructor) continue;
                if (!has_concrete_method(ctx, i, im->name, im->param_count))
                    sema_error(ctx, c->ast_node->loc,
                               "class '%s' must implement interface method '%s'",
                               c->name, im->name);
            }
        }

        /* §9.2: if two implemented interfaces declare methods with
         * the same name and param types but different return types,
         * the class can't satisfy both — compile error. */
        for (int ia = 0; ia < c->interface_count; ia++) {
            int aid = c->interface_ids[ia];
            if (aid < 0) continue;
            const sema_class_t* ai = &ctx->classes[aid];
            for (int ib = ia + 1; ib < c->interface_count; ib++) {
                int bid = c->interface_ids[ib];
                if (bid < 0) continue;
                const sema_class_t* bi = &ctx->classes[bid];
                for (int ma = 0; ma < bbq_vec_len(ai->methods); ma++) {
                    const sema_method_t* am = &ai->methods[ma];
                    for (int mb = 0; mb < bbq_vec_len(bi->methods); mb++) {
                        const sema_method_t* bm = &bi->methods[mb];
                        if (strcmp(am->name, bm->name) != 0) continue;
                        if (am->param_count != bm->param_count) continue;
                        if (!jt_eq(am->return_type, bm->return_type))
                            sema_error(ctx, c->ast_node->loc,
                                "interface method return type conflict on '%s'",
                                am->name);
                    }
                }
            }
        }
    }
}

static void validate_overrides(sema_ctx_t* ctx) {
    for (int i = 0; i < bbq_vec_len(ctx->classes); i++) {
        sema_class_t* c = &ctx->classes[i];
        if (!c->ast_node) continue;

        for (int j = 0; j < bbq_vec_len(c->methods); j++) {
            sema_method_t* m = &c->methods[j];
            if (m->is_constructor) continue;
            if (!m->ast_node) continue;

            /* Look for overridden method in superclass chain */
            int super = c->super_id;
            while (super >= 0) {
                const sema_class_t* sc = &ctx->classes[super];
                for (int k = 0; k < bbq_vec_len(sc->methods); k++) {
                    const sema_method_t* sm = &sc->methods[k];
                    if (sm->is_constructor) continue;
                    if (strcmp(sm->name, m->name) != 0) continue;
                    if (!params_match(m, sm)) continue;

                    /* Found an overridden method — check constraints */

                    /* Static method cannot override instance method (and vice versa) */
                    if ((m->modifiers & ACC_STATIC) != (sm->modifiers & ACC_STATIC))
                        sema_error(ctx, m->ast_node->loc,
                                   "static/instance mismatch on override of '%s'",
                                   m->name);

                    /* Cannot override final method */
                    if (sm->modifiers & ACC_FINAL)
                        sema_error(ctx, m->ast_node->loc,
                                   "cannot override final method '%s'", m->name);

                    /* Return type must match (no covariant returns in Java 1.0) */
                    if (!jt_eq(m->return_type, sm->return_type))
                        sema_error(ctx, m->ast_node->loc,
                                   "override of '%s' has incompatible return type",
                                   m->name);

                    /* Cannot reduce visibility (JLS §8.4.6.3) */
                    int m_vis = m->modifiers & (ACC_PUBLIC|ACC_PROTECTED|ACC_PRIVATE);
                    int s_vis = sm->modifiers & (ACC_PUBLIC|ACC_PROTECTED|ACC_PRIVATE);
                    bool bad_vis = false;
                    if (s_vis & ACC_PUBLIC)
                        bad_vis = !(m_vis & ACC_PUBLIC);
                    else if (s_vis & ACC_PROTECTED)
                        bad_vis = (m_vis & ACC_PRIVATE);
                    if (bad_vis)
                        sema_error(ctx, m->ast_node->loc,
                                   "override of '%s' reduces visibility", m->name);

                    /* §11.2: override can't add checked exceptions */
                    for (int ti = 0; ti < m->thrown_count; ti++) {
                        java_type_t tt = m->thrown_types[ti];
                        if (tt.tag != JT_CLASS) continue;
                        if (is_unchecked_exc(ctx, tt.class_id))
                            continue;
                        bool parent_declares = false;
                        for (int si2 = 0; si2 < sm->thrown_count; si2++) {
                            java_type_t st = sm->thrown_types[si2];
                            if (st.tag == JT_CLASS &&
                                (st.class_id == tt.class_id ||
                                 is_subclass_of(ctx, tt.class_id, st.class_id)))
                                { parent_declares = true; break; }
                        }
                        if (!parent_declares)
                            sema_error(ctx, m->ast_node->loc,
                                "override of '%s' throws checked exception "
                                "'%s' not declared by parent",
                                m->name, ctx->classes[tt.class_id].name);
                    }

                    goto next_method; /* only check first override */
                }
                super = sc->super_id;
            }
            next_method:;
        }
    }
}

static void validate_hierarchy(sema_ctx_t* ctx) {
    validate_modifier_combos(ctx);
    validate_abstract_impl(ctx);
    validate_overrides(ctx);
}

/* ═══════════════════════════════════════════════════════════════
 * Access control
 * ═══════════════════════════════════════════════════════════════ */

/* The declared package of class `class_id`, from its compilation unit (§7.4);
 * NULL = the unnamed package (§7.4.2) — synthetics included. */
static const char* class_package(const sema_ctx_t* ctx, int class_id) {
    if (class_id < 0) return NULL;
    int u = ctx->classes[class_id].unit_idx;
    return (u >= 0) ? ctx->units[u].package : NULL;
}

static bool same_package(const sema_ctx_t* ctx, int a, int b) {
    const char* pa = class_package(ctx, a);
    const char* pb = class_package(ctx, b);
    if (!pa && !pb) return true;             /* both unnamed (§7.4.2) */
    return pa && pb && strcmp(pa, pb) == 0;
}

/* `qual_class` is the class type of the QUALIFIER at the access site — the type of Q in Q.Id
 * or of E in E.Id — and is what §6.6.2 turns on. Pass -1 where the access has no qualifier
 * (a simple name) or where §6.6.2 permits it outright (super.Id, which never reaches here:
 * AST_SUPERACCESS and AST_SUPERCALL are their own nodes). */
static bool check_access(const sema_ctx_t* ctx, int from_class, int target_class,
                          int member_mods, ast_srcloc loc, const char* member_name,
                          int qual_class) {
    /* Public: always accessible */
    if (member_mods & ACC_PUBLIC) return true;

    /* Private: only from the same class */
    if (member_mods & ACC_PRIVATE) {
        if (from_class == target_class) return true;
        sema_error((sema_ctx_t*)ctx, loc,
                   "'%s' has private access in '%s'",
                   member_name, ctx->classes[target_class].name);
        return false;
    }

    /* Protected: §6.6.1 grants the package and the subclass; §6.6.2 then constrains HOW a
     * subclass in another package may reach it. Let C be the class declaring the member and S
     * the subclass in whose body the use occurs — for a qualified access, "the access is
     * permitted if and only if the type of the expression Q is S or a subclass of S".
     *
     * S is `from_class`, not the declaring class: a subclass is trusted with the
     * implementation of ITSELF, not with that of every other subclass of its superclass. So
     * `p.x` on a Point-typed parameter is an error inside Point3d even though Point3d extends
     * Point, which is exactly §6.6.7's worked example. */
    if (member_mods & ACC_PROTECTED) {
        if (same_package(ctx, from_class, target_class)) return true;
        if (is_subclass_of(ctx, from_class, target_class)) {
            if (qual_class < 0) return true;                        /* simple name: the receiver is `this` */
            if (qual_class == from_class) return true;
            if (is_subclass_of(ctx, qual_class, from_class)) return true;
            sema_error((sema_ctx_t*)ctx, loc,
                       "protected member '%s' is not accessible through an expression of type '%s'; "
                       "it must be '%s' or a subclass",
                       member_name, ctx->classes[qual_class].name,
                       ctx->classes[from_class].name);
            return false;
        }
        sema_error((sema_ctx_t*)ctx, loc,
                   "'%s' has protected access in '%s'",
                   member_name, ctx->classes[target_class].name);
        return false;
    }

    /* Package-private (no modifier): same package only */
    if (same_package(ctx, from_class, target_class)) return true;
    sema_error((sema_ctx_t*)ctx, loc,
               "'%s' has package-private access in '%s'",
               member_name, ctx->classes[target_class].name);
    return false;
}

/* ═══════════════════════════════════════════════════════════════
 * Pass 2: Body analysis — expression type inference
 * ═══════════════════════════════════════════════════════════════ */

/* Forward declarations */
/* Record a resolved call/ctor target as a function import iff it is NOT a defined
 * function — i.e. a library-class method or constructor (the extern API) or a
 * native method. The host supplies it at instantiation. Deduplicated; first-
 * referenced order = the import's funcidx. Recording referenced-only means a
 * module carries no unused library functions. */
/* ── JLS §13.1: record ONE symbolic reference ──────────────────────────────────────────────
 *
 * "This specification does not mandate the use of any specific binary file format. Rather, it
 * specifies properties that any binary format for compiled types must obey." So this records
 * what the SOURCE references, in Java terms, and says nothing about what a backend does with
 * it — whether the reference becomes a WASM import, a constant-pool entry, or nothing at all
 * because the same compilation emits the target.
 *
 * Attributed to `ctx->current_class_id` because §13.1 states its rules for "a Java binary
 * representation FOR A CLASS or interface": a reference belongs to the class that makes it. */
static void sema_note_ref(sema_ctx_t* ctx, sema_ref_kind_t kind, int decl_class, int member) {
    if (decl_class < 0) return;
    int from = ctx->current_class_id;
    if (from < 0) return;
    for (int i = 0; i < (int)bbq_vec_len(ctx->refs); i++)
        if (ctx->refs[i].kind == (int)kind && ctx->refs[i].from_class == from
            && ctx->refs[i].decl_class == decl_class && ctx->refs[i].member == member) return;
    sema_ref_ent_t r = { (int)kind, from, decl_class, member };
    bbq_vec_push(ctx->refs, r);
}

/* A field access, per §13.1: "a symbolic reference to the class or interface in which the field
 * is declared, plus the simple name of the field ... The reference must also include a symbolic
 * reference to the declared type of the field" — (decl_class, index) fixes both, and the
 * declared type is recorded as a type reference in its own right.
 *
 * EXCEPT: "References to fields that are static, final, and initialized with compile-time
 * constant expressions are resolved at compile time to the constant value that is denoted. NO
 * REFERENCE to such a constant field should be present in the code in a binary file (except in
 * the class or interface containing the constant field, which will have code to initialize
 * it)." So a constant field read leaves NO reference — the value was substituted. */
static void sema_note_field_ref(sema_ctx_t* ctx, const sema_field_t* f) {
    if (!f || f->owner < 0) return;
    if ((f->modifiers & ACC_STATIC) && (f->modifiers & ACC_FINAL)
        && f->init_expr && jls_const_is_constant(ctx, f->init_expr))
        return;   /* §13.1: "resolved at compile time to the constant value that is denoted" */
    /* The spec's exception — "except in the class or interface containing the constant field,
     * which will have code to initialize it" — is about the INITIALIZER, a different construct
     * from a read. A read of the constant folds wherever it appears, including at home. */
    sema_note_ref(ctx, SEMA_REF_FIELD, f->owner, f->index);
    if (f->type.tag == JT_CLASS)                 /* "a symbolic reference to the declared type" */
        sema_note_ref(ctx, SEMA_REF_TYPE, f->type.class_id, -1);
}

static void sema_note_import(sema_ctx_t* ctx, const sema_method_t* m) {
    if (!m) return;
    /* Only EMITTED code generates imports — a native call site inside a body that is
     * actually compiled. Library method bodies ARE emitted (the E6 compiled overlays),
     * so a compiled library method calling a native (e.g. Float.equals → floatToIntBits)
     * MUST register that native as an import, exactly like user code: otherwise the
     * emitted body references a funcidx no import provides ("unknown function"). We are
     * only ever inside analyze here for a method that HAS a body (native/abstract methods
     * have none), i.e. an emitted method — so the referring class being library vs user
     * is irrelevant. The target-is-defined check below still keeps compiled→compiled
     * calls out of the import list. */
    if (ctx->current_class_id < 0) return;
    int dc = m->owner, idx = m->index;   /* stamped identity — no address search */
    if (dc < 0) return;

    /* §13.1 first, and WITHOUT the intrinsic filters below: `Math.sqrt(x)` is a reference to
     * java.lang.Math.sqrt whether or not this backend happens to have an f64.sqrt opcode to
     * lower it to. "Lowers inline" is a statement about one target. */
    sema_note_ref(ctx, SEMA_REF_METHOD, dc, idx);
    if (m->return_type.tag == JT_CLASS)          /* "a symbolic reference to the return type" */
        sema_note_ref(ctx, SEMA_REF_TYPE, m->return_type.class_id, -1);
    for (int p = 0; p < m->param_count; p++)     /* "...to the type of each parameter" */
        if (m->param_types[p].tag == JT_CLASS)
            sema_note_ref(ctx, SEMA_REF_TYPE, m->param_types[p].class_id, -1);

    /* Bodiless call targets — the host-extern candidates. A backend reads this to find the
     * user-declared natives, which no reference rule identifies: they are the environment
     * edges, not library members. The filters below are target-shaped and deliberately absent
     * from the reference set above: an intrinsic that lowers inline is still a §13.1
     * reference. */
    if (sema_method_lowers_inline(m)) return;
    if (sema_method_is_defined(ctx, dc, m)) return;   /* emitted as a defined function, not an import */
    if ((m->modifiers & SEMA_ACC_ABSTRACT) || ctx->classes[dc].is_interface) return;
    for (int i = 0; i < (int)bbq_vec_len(ctx->import_funcs); i++)
        if (ctx->import_funcs[i].class_id == dc && ctx->import_funcs[i].method_id == idx)
            return;
    sema_func_ent_t e = { dc, idx };
    bbq_vec_push(ctx->import_funcs, e);
}

static java_type_t analyze_expr(sema_ctx_t* ctx, ast_expr_t* e);
static void analyze_stmt(sema_ctx_t* ctx, ast_stmt_t* s);
static bool is_throwable_subclass(const sema_ctx_t* ctx, int class_id);
static bool params_match(const sema_method_t* a, const sema_method_t* b);

static void store_type(sema_ctx_t* ctx, const ast_expr_t* e, java_type_t t) {
    bbq_htree_insert(ctx->expr_types, ptr_key(e), arena_type(ctx, t));
}

/* JLS §6.5.2 (ambiguous name reclassification): a package-qualified name path used as a value —
 * e.g. the base `java.io.FileDescriptor` of `java.io.FileDescriptor.in` — names a TYPE, not an
 * expression. Return the class id it names, or -1 if `e` is not a multi-part name path, its leftmost
 * segment IS a resolvable variable/field (so it's an expression name), or the path names no class.
 * (A single-ident base is left to analyze_expr, which already resolves a bare class name.) */
static int qualified_type_base(sema_ctx_t* ctx, const ast_expr_t* e) {
    if (!e || e->tag != AST_FIELDACCESS) return -1;
    /* §6.5 puts no bound on a qualified name's length. The old sixteen-segment
     * array made a longer path return -1 — "not a type name" — and the 256-byte
     * buffer truncated the path before it was probed, so both limits answered a
     * different question than the one asked, silently. */
    const char** segs = NULL;
    const ast_expr_t* cur = e;
    while (cur && cur->tag == AST_FIELDACCESS) {
        bbq_vec_push(segs, cur->field_access.field);
        cur = cur->field_access.obj;
    }
    int n = (int)bbq_vec_len(segs);
    if (!cur || cur->tag != AST_IDENT || n == 0) { bbq_vec_free(segs); return -1; }
    const char* head = cur->ident.name;
    if (scope_lookup_var(ctx, head)) { bbq_vec_free(segs); return -1; }      /* leftmost is a local/param */
    if (find_field(ctx, ctx->current_class_id, head)) { bbq_vec_free(segs); return -1; }  /* leftmost is a field */
    bbq_buf nb; bbq_buf_init(&nb);                                      /* "head.segs[n-1]....segs[0]" */
    bbq_buf_append(&nb, head, strlen(head));
    for (int i = n - 1; i >= 0; i--) {
        bbq_buf_append(&nb, ".", 1);
        bbq_buf_append(&nb, segs[i], strlen(segs[i]));
    }
    bbq_buf_append(&nb, "", 1);                                         /* NUL */
    bbq_vec_free(segs);
    const char* buf = (const char*)nb.data;
    /* §6.5.4.2 probe: the whole dotted path as a package-qualified type name.
     * (The old fallback that retried the LAST segment as a simple name would
     * accept `bogus.pkg.FileDescriptor` — Q must name the real package.)
     * Nested shapes (`pkg.Class.staticField.x`) resolve by recursion: this
     * probe misses, the caller descends, and the shorter base probes again. */
    int r = sema_resolve_type(ctx, cur_unit(ctx), buf, e->loc, true);
    bbq_buf_free(&nb);
    return r;
}

/* §15.27's String half. Is `e` a constant String expression, and if so, which code units does
 * it denote? Returns its ArrayInit of chars, or NULL.
 *
 * "Constant expression" here is §15.27's: a String literal, or a `+` of two constant String
 * expressions. It must run on the UN-DESUGARED tree — once desugar_string_literal has rewritten
 * a StringLit into `new String(chars).intern()` there is no literal left to recognise, which is
 * exactly the ordering const_expr.c called a blocker. */
static ast_expr_t* string_const_chars(ast_expr_t* e) {
    if (!e) return NULL;
    if (e->tag == AST_STRINGLIT) return e->string_lit.chars;
    if (e->tag == AST_BINARY && e->binary.op == AST_ADD) {
        ast_expr_t* l = string_const_chars(e->binary.lhs);
        ast_expr_t* r = string_const_chars(e->binary.rhs);
        if (l && r) return e;      /* foldable; the caller splices the two element lists */
    }
    return NULL;
}

/* Append every char element of a constant String expression to `out` (a bbq_vec), in order. */
static void string_const_flatten(ast_expr_t* e, ast_expr_t*** out) {
    if (e->tag == AST_STRINGLIT) {
        ast_expr_t* ci = e->string_lit.chars;
        if (ci->tag == AST_ARRAYINIT)
            for (int i = 0; i < ci->array_init.elems_count; i++)
                bbq_vec_push(*out, ci->array_init.elems[i]);
        return;                    /* an empty literal is new char[0] — it contributes none */
    }
    string_const_flatten(e->binary.lhs, out);
    string_const_flatten(e->binary.rhs, out);
}

/* §15.27 + §3.10.5: replace a constant String concatenation with the single literal it
 * denotes, so it is interned like any other literal rather than built fresh by the ddcg's
 * StringBuffer desugar. This is what makes the spec's own example hold:
 *
 *     hello == ("Hel" + "lo")     true   — a constant expression, folded to a literal
 *     hello == ("Hel" + lo)       false  — computed at run time, a distinct object
 *
 * Returns true if `e` was folded in place. */
static bool fold_string_concat(sema_ctx_t* ctx, ast_expr_t* e) {
    if (!string_const_chars(e) || e->tag != AST_BINARY) return false;
    ast_expr_t** elems = NULL;
    string_const_flatten(e, &elems);
    int n = (int)bbq_vec_len(elems);
    ast_expr_t* chars;
    if (n == 0) {
        /* §10.6: an empty initializer has no element type to infer, so the empty literal is
         * an explicit `new char[0]` — the same shape jstr_to_array produces. */
        ast_expr_t** dims = (ast_expr_t**)bbq_arena_alloc(ctx->arena, sizeof(ast_expr_t*));
        dims[0] = ast_int_lit(ctx->arena, 0);
        chars = ast_new_array(ctx->arena, ast_char_type(ctx->arena), dims, 1);
    } else {
        ast_expr_t** arr = (ast_expr_t**)bbq_arena_alloc(ctx->arena, (size_t)n * sizeof(*arr));
        memcpy(arr, elems, (size_t)n * sizeof(*arr));
        chars = ast_array_init(ctx->arena, arr, n);
    }
    bbq_vec_free(elems);
    ast_srcloc loc = e->loc;
    *e = *ast_string_lit(ctx->arena, chars);   /* in place: the parent already holds this node */
    e->loc = loc;
    return true;
}

static java_type_t analyze_expr(sema_ctx_t* ctx, ast_expr_t* e) {
    if (!e) return jt_error();
    java_type_t result = jt_error();

    switch (e->tag) {
    case AST_INTLIT:
        /* §3.10.1: an integer literal has type int. (Narrowing to a byte/short/char
         * target is a §5.2 constant assignment conversion, decided at the delivery
         * point — NOT by pre-tagging the literal's type.) */
        result = jt_prim(JT_INT);
        break;

    /* §3.10.5: "A string literal is always of type String." The node is rewritten into
     * `new String(chars).intern()` by desugar_string_literals, AFTER §15.27 has folded any
     * constant String expression around it — that ordering is the whole reason the literal
     * survives the parser. Typing it here is what lets §15.17.1's concatenation rule see a
     * String on either side while it is still a literal. */
    case AST_STRINGLIT: {
        /* §3.10.5: the literal IS a String, and literals "are interned so as to share unique
         * instances". Desugar to `new String(chars).intern()` — HERE and not in the parser,
         * because a parser-side rewrite leaves no literal for §15.27's fold (above) to
         * recognise. By the time control reaches this node every enclosing constant
         * concatenation has already folded, so what remains is a genuine literal.
         *
         * Rewritten IN PLACE: the parent already holds this node, and the ddcg has no rule for
         * a StringLit that survives (it panics, deliberately). */
        ast_expr_t* chars = e->string_lit.chars;
        ast_expr_t** args = (ast_expr_t**)bbq_arena_alloc(ctx->arena, sizeof(ast_expr_t*));
        args[0] = chars;
        ast_srcloc loc = e->loc;
        ast_expr_t* fresh = ast_new(ctx->arena, ast_simple_name(ctx->arena, "String"), args, 1);
        *e = *ast_method_call(ctx->arena, fresh, "intern", NULL, 0);
        e->loc = loc;
        return analyze_expr(ctx, e);      /* now an AST_METHODCALL — terminates */
    }

    case AST_LONGLIT:   result = jt_prim(JT_LONG);   e->etype = JT_LONG;   break;
    case AST_FLOATLIT:  result = jt_prim(JT_FLOAT);  e->etype = JT_FLOAT;  break;
    case AST_DOUBLELIT: result = jt_prim(JT_DOUBLE); e->etype = JT_DOUBLE; break;
    case AST_CHARLIT:   result = jt_prim(JT_CHAR);   e->etype = JT_CHAR;   break;

    case AST_BOOLLIT:
        result = jt_prim(JT_BOOL);
        break;

    case AST_NULLLIT:
        result = jt_null();
        break;

    case AST_IDENT: {
        const char* name = e->ident.name;
        sema_var_t* v = scope_lookup_var(ctx, name);
        if (v) {
            result = v->type;
            /* Phase B: record ident kind = LOCAL or PARAM */
            sema_ident_info_t* info = (sema_ident_info_t*)
                bbq_arena_alloc(ctx->arena, sizeof(sema_ident_info_t));
            info->kind = v->is_param ? SEMA_IDENT_PARAM : SEMA_IDENT_LOCAL;
            info->slot = v->slot;
            info->dt   = type_tag_to_dt(v->type.tag);
            info->field = NULL;
            info->var_is_final = v->is_final;
            info->var_init = v->init_expr;
            bbq_htree_insert(ctx->ident_kinds, ptr_key(e), info);
        } else {
            /* Try as field of current class */
            const sema_field_t* f = find_field(ctx, ctx->current_class_id, name);
            if (f && field_simple_name_ambiguous(ctx, ctx->current_class_id, name))
                sema_error(ctx, e->loc,
                    "reference to '%s' is ambiguous: it is inherited from more than one "
                    "interface (§6.4.2); qualify it with the interface name",
                    name);
            if (f) {
                if (ctx->in_static_context && !(f->modifiers & ACC_STATIC))
                    sema_error(ctx, e->loc,
                        "instance field '%s' cannot be referenced from static context",
                        name);
                if (ctx->static_init_field_limit >= 0 &&
                    f->index > ctx->static_init_field_limit)
                    sema_error(ctx, e->loc,
                        "forward reference to static field '%s'", name);
                result = f->type;
                bbq_htree_insert(ctx->resolved_fields, ptr_key(e), encode_member_loc(f->owner, f->index));
                sema_note_field_ref(ctx, f);   /* §13.1 */
                /* Phase B: record ident kind = INSTANCE_FIELD or STATIC_FIELD */
                sema_ident_info_t* info = (sema_ident_info_t*)
                    bbq_arena_alloc(ctx->arena, sizeof(sema_ident_info_t));
                info->kind = (f->modifiers & ACC_STATIC) ? SEMA_IDENT_STATIC_FIELD
                                                          : SEMA_IDENT_INSTANCE_FIELD;
                info->slot = 0;
                info->dt   = type_tag_to_dt(f->type.tag);
                info->field = f;
                info->var_is_final = false;
                info->var_init = NULL;
                bbq_htree_insert(ctx->ident_kinds, ptr_key(e), info);
            } else {
                /* Try as class name — §6.5.4 probe against the current unit
                 * (a miss here is "undefined symbol", not a type error). */
                int cid = sema_resolve_type(ctx, cur_unit(ctx), name, e->loc, true);
                if (cid >= 0) {
                    result = jt_class(cid);
                    /* Record the classification: downstream passes must never
                     * re-resolve the name (resolution is unit-relative). */
                    sema_ident_info_t* info = (sema_ident_info_t*)
                        bbq_arena_alloc(ctx->arena, sizeof(sema_ident_info_t));
                    info->kind = SEMA_IDENT_CLASSREF;
                    info->slot = cid;               /* the resolved class id */
                    info->dt   = type_tag_to_dt(JT_CLASS);
                    info->field = NULL;
                    info->var_is_final = false;
                    info->var_init = NULL;
                    bbq_htree_insert(ctx->ident_kinds, ptr_key(e), info);
                } else {
                    sema_error(ctx, e->loc, "undefined '%s'", name);
                }
            }
        }
        break;
    }

    case AST_FIELDACCESS: {
        /* JLS §6.5.2: a package-qualified type as the base (java.io.FileDescriptor.in) → resolve the
         * base as a type name, not a value chain. sema_may_have_effects is false for the unanalyzed
         * base, so codegen's static_field_acc never evaluates it. */
        int qcls = qualified_type_base(ctx, e->field_access.obj);
        java_type_t obj;
        if (qcls >= 0) { obj = jt_class(qcls); store_type(ctx, e->field_access.obj, obj); }  /* codegen reads the base's type */
        else obj = analyze_expr(ctx, e->field_access.obj);
        if (jt_is_error(obj)) break;
        if (obj.tag == JT_CLASS) {
            const sema_field_t* f = find_field(ctx, obj.class_id, e->field_access.field);
            if (f) {
                /* qcls >= 0 is a TypeName base (Q.Id, §6.6.2's second bullet); otherwise the
                 * base is an ExpressionName or a Primary, and obj.class_id is its type. Both
                 * bullets impose the same test, so both pass obj.class_id. */
                check_access(ctx, ctx->current_class_id, obj.class_id,
                             f->modifiers, e->loc, e->field_access.field,
                             obj.class_id);
                result = f->type;
                bbq_htree_insert(ctx->resolved_fields, ptr_key(e), encode_member_loc(f->owner, f->index));
                sema_note_field_ref(ctx, f);   /* §13.1 */
            } else {
                sema_error(ctx, e->loc, "no field '%s' in class '%s'",
                           e->field_access.field, ctx->classes[obj.class_id].name);
            }
        } else if (obj.tag == JT_ARRAY && strcmp(e->field_access.field, "length") == 0) {
            result = jt_prim(JT_INT); /* JLS §10.7: the array length field is int */
        } else {
            sema_error(ctx, e->loc, "field access on non-class type");
        }
        break;
    }

    case AST_ARRAYACCESS: {
        java_type_t arr = analyze_expr(ctx, e->array_access.arr);
        java_type_t idx = analyze_expr(ctx, e->array_access.index);
        if (!jt_is_error(arr) && arr.tag != JT_ARRAY)
            sema_error(ctx, e->loc, "array access on non-array type");
        /* §10.4 (p.195): "Arrays must be indexed by int values; short, byte, or char
         * values may also be used as index values because they are subjected to
         * unary numeric promotion (§5.6.1) and become int values. An attempt to
         * access an array component with a long index value results in a
         * compile-time error." The test is therefore "promotes to int", not "is
         * numeric" — long, float and double promote to themselves. Accepting them
         * let a long index reach the backend, which emitted an i64 where the array
         * op wants an i32, and the module failed §7 validation: a compile-time error
         * the spec names, surfacing as a codegen bug. */
        if (!jt_is_error(idx) && !(idx.tag == JT_INT || idx.tag == JT_SHORT
                                   || idx.tag == JT_BYTE || idx.tag == JT_CHAR))
            sema_error(ctx, e->loc, "array index must be int (JLS 10.4)");
        if (arr.tag == JT_ARRAY && arr.element)
            result = *arr.element;
        break;
    }

    case AST_METHODCALL: {
        java_type_t obj_type;
        int target_class;
        /* §6.5.2 reclassification, the same one AST_FIELDACCESS does: the
         * receiver of `java.lang.Integer.parseInt(...)` is an AmbiguousName
         * that names a TYPE, not a value chain. Analyzing it as an expression
         * descends to the leftmost segment and reports "undefined 'java'".
         * §15.11.1: "If it is a qualified name of the form TypeName .
         * Identifier, then the name of the method is the Identifier and the
         * class to search is the one named by the TypeName." */
        bool obj_is_typename = false;
        if (e->method_call.obj) {
            int qcls = qualified_type_base(ctx, e->method_call.obj);
            if (qcls >= 0) {
                obj_type = jt_class(qcls);
                obj_is_typename = true;
                store_type(ctx, e->method_call.obj, obj_type);  /* codegen reads the base's type */
            } else {
                obj_type = analyze_expr(ctx, e->method_call.obj);
            }
            if (jt_is_error(obj_type)) break;
            if (obj_type.tag == JT_ARRAY) {
                target_class = ctx->wk.object_id;   /* §10.7 an array is an Object — its Object methods (getClass/hashCode/equals/toString/clone) dispatch */
            } else if (obj_type.tag != JT_CLASS) {
                sema_error(ctx, e->loc, "method call on non-class type");
                break;
            } else {
                target_class = obj_type.class_id;
            }
        } else {
            /* An unqualified call's receiver is `this` — a class type, never an
             * array (reading obj_type below on this path was an uninitialized
             * read, found by MSAN; O0/O2 builds diverged on the garbage). */
            obj_type = jt_class(ctx->current_class_id);
            target_class = ctx->current_class_id;
        }

        /* Analyze arguments and collect types for overload resolution */
        int ac = e->method_call.args_count;
        /* One entry per argument. The fixed 32 clamped only the WRITE — the full
         * `ac` still reached find_method, and sig_applicable reads arg_types[j]
         * for every j < arg_count, so a call with more arguments than the bound
         * decided overload resolution off uninitialised stack. §8.4.1 admits 255. */
        java_type_t* call_arg_types = NULL;
        for (int i = 0; i < ac; i++)
            bbq_vec_push(call_arg_types, analyze_expr(ctx, e->method_call.args[i]));

        const sema_method_t* m = find_method(ctx, target_class,
                                           e->method_call.method, ac,
                                           ac > 0 ? call_arg_types : NULL);
        if (!m) {
            sema_error(ctx, e->loc, "no method '%s' with %d args in '%s'",
                       e->method_call.method, ac,
                       ctx->classes[target_class].name);
        } else {
            /* An unqualified call's receiver is `this`, which §6.6.2 does not constrain; a
             * qualified one is E.Id(...) and its qualifier's type is the test. */
            check_access(ctx, ctx->current_class_id, target_class,
                         m->modifiers, e->loc, e->method_call.method,
                         e->method_call.obj && obj_type.tag == JT_CLASS
                             ? obj_type.class_id : -1);
            if (ctx->in_static_context && !e->method_call.obj &&
                !(m->modifiers & ACC_STATIC))
                sema_error(ctx, e->loc,
                    "instance method '%s' cannot be referenced from static context",
                    e->method_call.method);
            /* §15.11.3: with a TypeName qualifier "the compile-time declaration
             * should be static. If the compile-time declaration for the method
             * invocation is for an instance method, then a compile-time error
             * occurs. (The reason is that a method invocation of this form does
             * not specify a reference to an object that can serve as this
             * within the instance method.)" There is no receiver to emit, so
             * accepting it would be a miscompile, not a lax diagnostic. */
            if (obj_is_typename && !(m->modifiers & ACC_STATIC))
                sema_error(ctx, e->loc,
                    "instance method '%s' cannot be invoked through the type name '%s'",
                    e->method_call.method, ctx->classes[target_class].name);
            /* Check argument types */
            for (int i = 0; i < ac && i < m->param_count; i++) {
                java_type_t at = sema_type_of(ctx, e->method_call.args[i]);
                if (!is_assignable(ctx, m->param_types[i], at, false, 0) &&
                    !jt_is_error(at)) {
                    sema_error(ctx, e->loc,
                               "arg %d: cannot convert to parameter type", i + 1);
                }
            }
            if (m->simd_id != 0) check_simd_imms(ctx, e, m);
            /* §11.2: checked exceptions must be caught or declared. §10.7 exception: an array's
             * clone() is public and never throws CloneNotSupportedException (arrays are always
             * Cloneable), even though it resolves through Object.clone here — so don't inherit
             * Object.clone's throws clause for an array receiver. */
            bool array_clone = obj_type.tag == JT_ARRAY &&
                               strcmp(e->method_call.method, "clone") == 0;
            for (int ti = 0; !array_clone && ti < m->thrown_count; ti++) {
                java_type_t tt = m->thrown_types[ti];
                if (tt.tag != JT_CLASS) continue;
                if (is_unchecked_exc(ctx, tt.class_id))
                    continue;
                bool handled = false;
                if (ctx->current_method) {
                    for (int di = 0; di < ctx->current_method->thrown_count; di++) {
                        java_type_t dt = ctx->current_method->thrown_types[di];
                        if (dt.tag == JT_CLASS &&
                            (dt.class_id == tt.class_id ||
                             is_subclass_of(ctx, tt.class_id, dt.class_id)))
                            { handled = true; break; }
                    }
                }
                for (int ci2 = 0; !handled && ci2 < bbq_vec_len(ctx->caught_types); ci2++) {
                    java_type_t ct = ctx->caught_types[ci2];
                    if (ct.tag == JT_CLASS &&
                        (ct.class_id == tt.class_id ||
                         is_subclass_of(ctx, tt.class_id, ct.class_id)))
                        handled = true;
                }
                if (!handled)
                    sema_error(ctx, e->loc,
                        "unhandled checked exception: %s throws %s",
                        m->name, ctx->classes[tt.class_id].name);
            }
            result = m->return_type;
            bbq_htree_insert(ctx->resolved_methods, ptr_key(e), encode_method_loc(ctx, m));
            sema_note_import(ctx, m);          /* native target → function import */

            /* Phase B: invoke_kinds + target_classes */
            sema_invoke_kind_t ikind;
            if (m->modifiers & ACC_STATIC) {
                ikind = SEMA_INVOKE_STATIC;
            } else if (ctx->classes[target_class].is_interface) {
                ikind = SEMA_INVOKE_INTERFACE;
            } else if (m->modifiers & ACC_PRIVATE) {
                ikind = SEMA_INVOKE_SPECIAL;
            } else {
                ikind = SEMA_INVOKE_VIRTUAL;
            }
            bbq_htree_insert(ctx->invoke_kinds, ptr_key(e),
                             (void*)(uintptr_t)((int)ikind + 1));
            bbq_htree_insert(ctx->target_classes, ptr_key(e),
                             (void*)(uintptr_t)(target_class + 1));
        }
        bbq_vec_free(call_arg_types);
        break;
    }

    case AST_NEW: {
        const char* cname = name_to_str(ctx, e->new_.class_);
        int cid = sema_resolve_type(ctx, cur_unit(ctx), cname, e->loc, false);
        if (cid < 0) break;   /* §6.5.4 already errored */
        if (ctx->classes[cid].is_interface) {
            sema_error(ctx, e->loc, "cannot instantiate interface '%s'", cname);
            break;
        }
        if (ctx->classes[cid].modifiers & SEMA_ACC_ABSTRACT) {
            sema_error(ctx, e->loc,
                "cannot instantiate abstract class '%s'", cname);
            break;
        }
        int ac = e->new_.args_count;
        /* One entry per argument. Past the old bound the types were dropped and
         * NULL passed instead, which silently degrades §15.11.2 to an arity-only
         * match — a wrong constructor rather than a diagnostic. */
        java_type_t* ctor_at = NULL;
        for (int i = 0; i < ac; i++)
            bbq_vec_push(ctor_at, analyze_expr(ctx, e->new_.args[i]));
        bool has_explicit;
        const sema_method_t* ctor = find_constructor(ctx, cid, ac,
                                        ac > 0 ? ctor_at : NULL, &has_explicit);
        bbq_vec_free(ctor_at);
        if (!ctor && (has_explicit || ac > 0)) {
            sema_error(ctx, e->loc, "no constructor with %d args for '%s'",
                       ac, ctx->classes[cid].name);
        }
        /* Phase B: resolve the effective constructor (including the
         * synthesized super-default fallback) for DDCG. */
        {
            const sema_method_t* effective_ctor = ctor;
            if (!effective_ctor) {
                int sup = ctx->classes[cid].super_id;
                if (sup >= 0) {
                    bool dummy;
                    effective_ctor = find_constructor(ctx, sup, 0, NULL, &dummy);
                }
            }
            if (effective_ctor) {
                bbq_htree_insert(ctx->resolved_ctors, ptr_key(e),
                                 encode_method_loc(ctx, effective_ctor));
                sema_note_import(ctx, effective_ctor);   /* library ctor → host import */
            }
        }
        /* Record the resolved target class UNCONDITIONALLY — the ddcg reads
         * this, never re-resolving the spelled name (resolution is
         * unit-relative; a codegen-time name lookup would be §7-blind). */
        bbq_htree_insert(ctx->target_classes, ptr_key(e),
                         (void*)(uintptr_t)(cid + 1));
        result = jt_class(cid);
        break;
    }

    case AST_NEWARRAY: {
        /* `element` is the base; `dims` is one entry per bracket level (rank =
         * dims_count), a NULL entry being an unsized `[]` (jagged) dim. Each sized
         * dim must be numeric; per JLS §15.10.1 a sized dim may not follow an
         * unsized one. Result type = base wrapped `rank` times. */
        java_type_t elem = resolve_type(ctx, e->new_array.element, e->loc);
        int rank = e->new_array.dims_count;
        bool seen_empty = false;
        for (int d = 0; d < rank; d++) {
            ast_expr_t* dx = e->new_array.dims[d];
            if (!dx) { seen_empty = true; continue; }
            if (seen_empty)
                sema_error(ctx, e->loc,
                    "a sized array dimension cannot follow an unsized one");
            java_type_t sz = analyze_expr(ctx, dx);
            if (!jt_is_error(sz) && !jt_is_numeric(sz))
                sema_error(ctx, e->loc, "array size must be numeric");
        }
        java_type_t t = elem;
        for (int d = 0; d < rank; d++) t = jt_array(arena_type(ctx, t));
        result = t;
        sema_register_array_type(ctx, result);   /* §10.8 */
        break;
    }

    case AST_ARRAYINIT: {
        /* Infer element type from first element, check all match */
        java_type_t elem = jt_error();
        for (int i = 0; i < e->array_init.elems_count; i++) {
            java_type_t et = analyze_expr(ctx, e->array_init.elems[i]);
            if (i == 0) elem = et;
        }
        result = jt_array(arena_type(ctx, elem));

        /* Provisional element type = the elements' own (un-contextual) type — an
         * int-literal element is int (§3.10.1), not "the narrowest width that fits"
         * retype_array_init overrides this with the §10.6
         * contextual component type at the declaration/field, recursively per level. */
        int32_t elem_jt = (result.tag == JT_ARRAY && result.element)
                          ? result.element->tag : JT_BYTE;
        bbq_htree_insert(ctx->array_init_elem_types, ptr_key(e),
                         (void*)(uintptr_t)elem_jt);
        break;
    }

    case AST_CAST: {
        java_type_t target = resolve_type(ctx, e->cast.ty, e->loc);
        java_type_t value = analyze_expr(ctx, e->cast.e);
        /* JLS §5.5: casting allows widening + narrowing for primitives,
           widening + narrowing for references, but not boolean↔numeric */
        if (!jt_is_error(target) && !jt_is_error(value)) {
            bool ok = jt_eq(target, value) ||
                      is_widening_prim(value, target) ||
                      is_narrowing_prim(value, target) ||
                      is_widening_ref(ctx, value, target) ||
                      (jt_is_reference(value) && jt_is_reference(target));
            if (!ok)
                sema_error(ctx, e->loc, "illegal cast");
            /* §5.5: "Some casts can be proven incorrect at compile time; such casts result
             * in a compile-time error." Which ones is §5.1.7's forbidden list, and its
             * reference bullets are transcribed below — each is a case where NO run-time
             * value could satisfy the cast, so deferring to a ClassCastException reports at
             * run time what the spec says to report at compile time.
             *
             * This used to fire only when BOTH classes were final, which is a much smaller
             * rule than the one cited: `class A {} class B {} (B) someA` was accepted. */
            if (ok && target.tag == JT_CLASS && value.tag == JT_CLASS) {
                const sema_class_t* tc = &ctx->classes[target.class_id];
                const sema_class_t* vc = &ctx->classes[value.class_id];
                bool t_iface = tc->is_interface, v_iface = vc->is_interface;
                bool t_sub_v = is_subclass_of(ctx, target.class_id, value.class_id);
                bool v_sub_t = is_subclass_of(ctx, value.class_id, target.class_id);

                if (!t_iface && !v_iface) {
                    /* "from class type S to a different class type T if S is not a subclass
                     * of T and T is not a subclass of S" — unconditional: single inheritance
                     * means no object is ever an instance of two unrelated classes. */
                    if (!t_sub_v && !v_sub_t)
                        sema_error(ctx, e->loc,
                            "impossible cast: '%s' and '%s' are unrelated classes (§5.1.7)",
                            vc->name, tc->name);
                } else if (!v_iface && t_iface) {
                    /* "from class type S to interface type K if S is final and does not
                     * implement K". A non-final S may have a subclass that implements K. */
                    if ((vc->modifiers & ACC_FINAL) &&
                        !implements_interface(ctx, value.class_id, target.class_id))
                        sema_error(ctx, e->loc,
                            "impossible cast: '%s' is final and does not implement '%s' (§5.1.7)",
                            vc->name, tc->name);
                } else if (v_iface && !t_iface) {
                    /* "from interface type J to class type T if T is final and does not
                     * implement J" — the mirror of the bullet above. */
                    if ((tc->modifiers & ACC_FINAL) &&
                        !implements_interface(ctx, target.class_id, value.class_id))
                        sema_error(ctx, e->loc,
                            "impossible cast: '%s' is final and does not implement '%s' (§5.1.7)",
                            tc->name, vc->name);
                } else {
                    /* "from interface type J to interface type K if J and K declare methods
                     * with the same signature but different return types" — a class could
                     * only implement both by declaring one method with two return types. */
                    for (int vi = 0; vi < (int)bbq_vec_len(vc->methods); vi++) {
                        const sema_method_t* vm = &vc->methods[vi];
                        for (int ti = 0; ti < (int)bbq_vec_len(tc->methods); ti++) {
                            const sema_method_t* tm = &tc->methods[ti];
                            if (strcmp(vm->name, tm->name) != 0 ||
                                vm->param_count != tm->param_count) continue;
                            bool params_eq = true;
                            for (int p = 0; p < vm->param_count; p++)
                                if (!jt_eq(vm->param_types[p], tm->param_types[p]))
                                    { params_eq = false; break; }
                            if (params_eq && !jt_eq(vm->return_type, tm->return_type))
                                sema_error(ctx, e->loc,
                                    "impossible cast: '%s' and '%s' both declare '%s' with "
                                    "different return types (§5.1.7)",
                                    vc->name, tc->name, vm->name);
                        }
                    }
                }
            }
        }
        result = target;
        break;
    }

    case AST_INSTANCEOF: {
        java_type_t val = analyze_expr(ctx, e->instance_of.e);
        java_type_t ty = resolve_type(ctx, e->instance_of.ty, e->loc);
        if (!jt_is_error(val) && !jt_is_reference(val))
            sema_error(ctx, e->loc, "instanceof requires reference type");
        if (!jt_is_error(ty) && !jt_is_reference(ty))
            sema_error(ctx, e->loc, "instanceof target must be reference type");
        bbq_htree_insert(ctx->instanceof_types, ptr_key(e), arena_type(ctx, ty));  /* package the target for codegen */
        result = jt_prim(JT_BOOL);
        break;
    }

    case AST_BINARY: {
        /* §15.27: a constant String concatenation is computed at compile time and "then
         * treated as if [it] were literals" — so it interns like one. This must run BEFORE the
         * operands are analysed: analysing a StringLit desugars it to new String(..).intern(),
         * and after that there is no literal left to fold. */
        if (fold_string_concat(ctx, e)) return analyze_expr(ctx, e);
        java_type_t lhs = analyze_expr(ctx, e->binary.lhs);
        java_type_t rhs = analyze_expr(ctx, e->binary.rhs);
        if (jt_is_error(lhs) || jt_is_error(rhs)) break;

        switch (e->binary.op) {
        /* JLS §15.18.1: `+` with a String operand is string concatenation (result
         * String); the other operand undergoes §5.4 string conversion, so any
         * non-void type is legal. sema only assigns the type — the ddcg does the
         * defunctionalizing desugar into StringBuffer.append(...).toString(). */
        case AST_ADD:
            if ((lhs.tag == JT_CLASS && lhs.class_id == ctx->wk.string_id) ||
                (rhs.tag == JT_CLASS && rhs.class_id == ctx->wk.string_id)) {
                if (lhs.tag == JT_VOID || rhs.tag == JT_VOID)
                    sema_error(ctx, e->loc, "string concatenation operand cannot be void");
                /* A v128 has no §5.4 string conversion (it is not an Object and not a
                 * JLS primitive) — rejecting here keeps it out of the StringBuffer
                 * append desugar, which has no V128 overload. */
                if (lhs.tag == JT_V128 || rhs.tag == JT_V128)
                    sema_error(ctx, e->loc, "string concatenation operand cannot be a V128");
                result = jt_class(ctx->wk.string_id);
                break;
            }
            /* fall through to numeric add */
        /* §15.17/§15.18 arithmetic: result type = §5.6.2 binary numeric promotion
         * of the two operands (byte/short/char/int→int, long/float/double kept). */
        case AST_SUB: case AST_MUL:
        case AST_DIV: case AST_REM:
            if (!jt_is_numeric(lhs) || !jt_is_numeric(rhs))
                sema_error(ctx, e->loc, "arithmetic requires numeric operands");
            result = jt_prim(lat_promote(lhs, rhs));
            break;
        /* §15.19 shift: result type = §5.6.1 unary numeric promotion of the LEFT
         * operand only (the shift count's type is independent). */
        case AST_SHL: case AST_SHR: case AST_USHR:
            if (!jt_is_numeric(lhs) || !jt_is_numeric(rhs))
                sema_error(ctx, e->loc, "shift requires numeric operands");
            result = jt_prim(lat_promote(lhs, jt_prim(JT_INT)));
            break;
        /* §15.22 bitwise: numeric → §5.6.2 promotion; both boolean → boolean. */
        case AST_BITAND: case AST_BITOR: case AST_BITXOR:
            if (lhs.tag == JT_BOOL && rhs.tag == JT_BOOL)
                result = jt_prim(JT_BOOL);
            else if (jt_is_numeric(lhs) && jt_is_numeric(rhs))
                result = jt_prim(lat_promote(lhs, rhs));
            else
                sema_error(ctx, e->loc, "bitwise op requires numeric or boolean operands");
            break;
        /* Comparison: numeric, result boolean */
        case AST_LT: case AST_GT: case AST_LE: case AST_GE:
            if (!jt_is_numeric(lhs) || !jt_is_numeric(rhs))
                sema_error(ctx, e->loc, "comparison requires numeric operands");
            result = jt_prim(JT_BOOL);
            break;
        /* Equality: numeric or reference, result boolean */
        case AST_EQ: case AST_NE:
            if (jt_is_numeric(lhs) && jt_is_numeric(rhs))
                result = jt_prim(JT_BOOL);
            else if (jt_is_reference(lhs) && jt_is_reference(rhs))
                result = jt_prim(JT_BOOL);
            else if (lhs.tag == JT_BOOL && rhs.tag == JT_BOOL)
                result = jt_prim(JT_BOOL);
            else
                sema_error(ctx, e->loc, "equality requires compatible operands");
            break;
        /* Logical: boolean, result boolean */
        case AST_AND: case AST_OR:
            if (lhs.tag != JT_BOOL || rhs.tag != JT_BOOL)
                sema_error(ctx, e->loc, "logical op requires boolean operands");
            result = jt_prim(JT_BOOL);
            break;
        default:
            break;
        }
        break;
    }

    case AST_UNARY: {
        java_type_t operand = analyze_expr(ctx, e->unary.e);
        if (jt_is_error(operand)) break;
        switch (e->unary.op) {
        case AST_POS: case AST_NEG:
            if (!jt_is_numeric(operand))
                sema_error(ctx, e->loc, "unary +/- requires numeric operand");
            /* §15.15.3/4 → §5.6.1 unary numeric promotion: byte/short/char/int→int, else keep. */
            result = jt_prim(lat_promote(operand, jt_prim(JT_INT)));
            break;
        case AST_BITNOT:
            if (!jt_is_numeric(operand))
                sema_error(ctx, e->loc, "~ requires numeric operand");
            result = jt_prim(lat_promote(operand, jt_prim(JT_INT))); /* §15.15.5 → §5.6.1 */
            break;
        case AST_LOGNOT:
            if (operand.tag != JT_BOOL)
                sema_error(ctx, e->loc, "! requires boolean operand");
            result = jt_prim(JT_BOOL);
            break;
        case AST_PREINC: case AST_PREDEC:
        case AST_POSTINC: case AST_POSTDEC:
            if (!jt_is_numeric(operand))
                sema_error(ctx, e->loc, "++/-- requires numeric operand");
            if (!is_assignable_target(e->unary.e))
                sema_error(ctx, e->loc,
                    "++/-- requires a variable, field, or array element");
            check_not_final_local(ctx, e->unary.e, e->loc);
            check_not_final_field(ctx, e->unary.e, e->loc);
            result = operand; /* type preserved for inc/dec */
            break;
        default:
            break;
        }
        break;
    }

    case AST_ASSIGN: {
        java_type_t target = analyze_expr(ctx, e->assign.target);
        java_type_t value = analyze_expr(ctx, e->assign.value);
        if (!is_assignable_target(e->assign.target))
            sema_error(ctx, e->loc,
                "assignment requires a variable, field, or array element");
        check_not_final_local(ctx, e->assign.target, e->loc);
        check_not_final_field(ctx, e->assign.target, e->loc);
        int32_t cv = 0;
        bool is_const = is_const_int_expr(ctx, e->assign.value, &cv);
        if (!is_assignable(ctx, target, value, is_const, cv) &&
            !jt_is_error(target) && !jt_is_error(value)) {
            sema_error(ctx, e->loc, "incompatible types in assignment");
        }
        result = target;
        break;
    }

    case AST_COMPOUNDASSIGN: {
        java_type_t target = analyze_expr(ctx, e->compound_assign.target);
        java_type_t value = analyze_expr(ctx, e->compound_assign.value);
        if (!is_assignable_target(e->compound_assign.target))
            sema_error(ctx, e->loc,
                "compound assignment requires a variable, field, or array element");
        check_not_final_local(ctx, e->compound_assign.target, e->loc);
        check_not_final_field(ctx, e->compound_assign.target, e->loc);
        if (!jt_is_numeric(target) || !jt_is_numeric(value))
            if (!jt_is_error(target) && !jt_is_error(value))
                sema_error(ctx, e->loc, "compound assignment requires numeric types");
        result = target;
        break;
    }

    case AST_TERNARY: {
        java_type_t cond = analyze_expr(ctx, e->ternary.test);
        java_type_t then = analyze_expr(ctx, e->ternary.then);
        java_type_t else_ = analyze_expr(ctx, e->ternary.else_);
        if (!jt_is_error(cond) && cond.tag != JT_BOOL)
            sema_error(ctx, e->loc, "ternary condition must be boolean");
        if (jt_is_error(then) || jt_is_error(else_)) break;      /* suppress the cascade */

        /* §15.24 "The type of a conditional expression is determined as follows", p.368, in
         * the spec's own order — the bullets are ordered and the order is load-bearing:
         * byte/short is checked BEFORE the constant rule, and both before promotion.
         *
         * This used to be three lines whose reference case returned the `then` arm, commented
         * "simplified: should find common supertype". It was not a simplification but three
         * wrong answers: it rejected `short r = c ? aByte : aShort` and `byte r = c ? aByte
         * : 5`, both legal, and it accepted `c ? aString : aStringBuffer`, which the last
         * bullet makes a compile-time error. The ledger meanwhile called §15.24 COVERED. */
        if (jt_eq(then, else_)) {
            /* "If the second and third operands have the same type (which may be the null
             * type), then that is the type of the conditional expression." */
            result = then;
        } else if (jt_is_numeric(then) && jt_is_numeric(else_)) {
            if ((then.tag == JT_BYTE  && else_.tag == JT_SHORT) ||
                (then.tag == JT_SHORT && else_.tag == JT_BYTE)) {
                /* "If one of the operands is of type byte and the other is of type short,
                 * then the type of the conditional expression is short." */
                result = jt_prim(JT_SHORT);
            } else if (ternary_const_fits(ctx, then, e->ternary.else_)) {
                result = then;
            } else if (ternary_const_fits(ctx, else_, e->ternary.then)) {
                result = else_;
            } else {
                /* "Otherwise, binary numeric promotion (§5.6.2) is applied." */
                result = jt_prim(lat_promote(then, else_));
            }
        } else if (then.tag == JT_NULL && jt_is_reference(else_)) {
            /* "If one of the second and third operands is of the null type and the type of
             * the other is a reference type, then the type ... is that reference type." */
            result = else_;
        } else if (else_.tag == JT_NULL && jt_is_reference(then)) {
            result = then;
        } else if (jt_is_reference(then) && jt_is_reference(else_)) {
            /* "it must be possible to convert one of the types to the other type (call this
             * latter type T) by assignment conversion (§5.2); the type ... is T. It is a
             * compile-time error if neither type is assignment compatible with the other."
             *
             * Direction matters: T is the TARGET of the conversion, i.e. the wider arm. */
            if (is_assignable(ctx, else_, then, false, 0))      result = else_;
            else if (is_assignable(ctx, then, else_, false, 0)) result = then;
            else
                sema_error(ctx, e->loc,
                    "incompatible conditional operands: neither '%s' nor '%s' is assignment "
                    "compatible with the other (§15.24)",
                    ternary_ref_name(ctx, then), ternary_ref_name(ctx, else_));
        } else {
            /* "All other cases result in a compile-time error" (p.367) — a boolean against a
             * reference, a numeric against a reference, and so on. */
            sema_error(ctx, e->loc, "incompatible ternary branch types");
        }
        break;
    }

    case AST_THIS:
        if (ctx->in_static_context)
            sema_error(ctx, e->loc,
                "'this' cannot be referenced from static context");
        result = jt_class(ctx->current_class_id);
        break;

    case AST_SUPER:
        if (ctx->in_static_context)
            sema_error(ctx, e->loc,
                "'super' cannot be referenced from static context");
        if (ctx->current_class_id >= 0) {
            int sid = ctx->classes[ctx->current_class_id].super_id;
            if (sid >= 0)
                result = jt_class(sid);
            else
                sema_error(ctx, e->loc, "no superclass");
        }
        break;

    case AST_SUPERACCESS: {
        int sid = ctx->current_class_id >= 0 ?
                  ctx->classes[ctx->current_class_id].super_id : -1;
        if (sid >= 0) {
            const sema_field_t* f = find_field(ctx, sid, e->super_access.field);
            if (f) {
                result = f->type;
                bbq_htree_insert(ctx->resolved_fields, ptr_key(e), encode_member_loc(f->owner, f->index));
                sema_note_field_ref(ctx, f);   /* §13.1 */
            } else {
                sema_error(ctx, e->loc, "no field '%s' in superclass",
                           e->super_access.field);
            }
        }
        break;
    }

    case AST_SUPERCALL: {
        int sid = ctx->current_class_id >= 0 ?
                  ctx->classes[ctx->current_class_id].super_id : -1;
        int ac = e->super_call.args_count;
        java_type_t* super_arg_types = NULL;      /* one entry per argument */
        for (int i = 0; i < ac; i++)
            bbq_vec_push(super_arg_types, analyze_expr(ctx, e->super_call.args[i]));
        if (sid >= 0) {
            const sema_method_t* m = find_method(ctx, sid, e->super_call.method, ac,
                                                  ac > 0 ? super_arg_types : NULL);
            if (m) {
                result = m->return_type;
                bbq_htree_insert(ctx->resolved_methods, ptr_key(e), encode_method_loc(ctx, m));
                sema_note_import(ctx, m);      /* native super-call target → import */
                /* Phase B: super calls always emit invokespecial */
                bbq_htree_insert(ctx->invoke_kinds, ptr_key(e),
                                 (void*)(uintptr_t)((int)SEMA_INVOKE_SPECIAL + 1));
                bbq_htree_insert(ctx->target_classes, ptr_key(e),
                                 (void*)(uintptr_t)(sid + 1));
            } else {
                sema_error(ctx, e->loc, "no method '%s' in superclass",
                           e->super_call.method);
            }
        }
        bbq_vec_free(super_arg_types);
        break;
    }

    case AST_CONSTRUCTORCALL: {
        int target = e->constructor_call.is_super ?
            (ctx->current_class_id >= 0 ?
             ctx->classes[ctx->current_class_id].super_id : -1) :
            ctx->current_class_id;
        int ac = e->constructor_call.args_count;
        java_type_t* cc_at = NULL;                /* one entry per argument */
        for (int i = 0; i < ac; i++)
            bbq_vec_push(cc_at, analyze_expr(ctx, e->constructor_call.args[i]));
        if (target >= 0) {
            bool has_explicit;
            const sema_method_t* ctor = find_constructor(ctx, target, ac,
                                            ac > 0 ? cc_at : NULL, &has_explicit);
            if (ctor) {
                /* Phase B: record resolved ctor for DDCG */
                bbq_htree_insert(ctx->resolved_ctors, ptr_key(e), encode_method_loc(ctx, ctor));
                sema_note_import(ctx, ctor);   /* super()/this() to a library ctor → host import */
                /* Phase B: this()/super() always emit invokespecial */
                bbq_htree_insert(ctx->invoke_kinds, ptr_key(e),
                                 (void*)(uintptr_t)((int)SEMA_INVOKE_SPECIAL + 1));
                bbq_htree_insert(ctx->target_classes, ptr_key(e),
                                 (void*)(uintptr_t)(target + 1));
            }
        }
        bbq_vec_free(cc_at);
        result = jt_prim(JT_VOID);
        break;
    }
    }

    store_type(ctx, e, result);
    if (!e->etype)
        e->etype = result.tag;

    /* Pre-compute the SIR data type for DDCG (Phase B annotation) via the
     * one type authority — the lattice maps the JLS type tag to its dt. */
    int32_t dt = type_tag_to_dt(e->etype);
    bbq_htree_insert(ctx->data_types, ptr_key(e),
                     (void*)(uintptr_t)(dt + 1));

    /* Phase B: side_effects. Children are already in the table
     * (post-order), so we just look them up. Only insert when true
     * — missing means false. */
    if (compute_side_effects(ctx, e)) {
        bbq_htree_insert(ctx->side_effects, ptr_key(e), (void*)1);
    }

    return result;
}
/* ═══════════════════════════════════════════════════════════════
 * Pass 2: Body analysis — statement checking
 * ═══════════════════════════════════════════════════════════════ */

static void analyze_stmt(sema_ctx_t* ctx, ast_stmt_t* s) {
    if (!s) return;
    switch (s->tag) {
    case AST_BLOCK:
        scope_push(ctx);
        for (int i = 0; i < s->block.stmts_count; i++)
            analyze_stmt(ctx, s->block.stmts[i]);
        scope_pop(ctx);
        break;

    case AST_LOCALVARDECL: {
        java_type_t ty = resolve_type(ctx, s->local_var_decl.ty, s->loc);
        int lmods = modifiers_to_flags(s->local_var_decl.mods,
                                        s->local_var_decl.mods_count);
        bool is_final = (lmods & ACC_FINAL) != 0;
        for (int i = 0; i < s->local_var_decl.decls_count; i++) {
            ast_var_decl_t* vd = s->local_var_decl.decls[i];
            java_type_t vtype = declarator_type(ctx, ty, vd->dims);
            /* §14.3.2: "The scope of a local variable declared in a block is the rest of the
             * block, INCLUDING ITS OWN INITIALIZER." So the name is declared BEFORE the
             * initializer is analyzed. Analyzing first put the initializer outside the scope,
             * which was wrong in both directions at once: the spec's `int x = (x=2)*2;` — which
             * it prints as compiling and printing 4 — failed with "undefined 'x'", while its
             * `static int x; ... int x = x;` — which it prints as a compile-time error —
             * compiled, because `x` fell through to the field.
             *
             * `int x = x;` is now an error for the reason the spec gives: the local "does not
             * yet have a value and cannot be used", which is §16 definite assignment. */
            ctx->declaring_final = is_final;
            ctx->declaring_init  = vd->init;
            int32_t slot = scope_declare(ctx, vd->name, vtype, s->loc);
            ctx->declaring_final = false;
            ctx->declaring_init  = NULL;
            if (vd->init) {
                java_type_t init_type = analyze_expr(ctx, vd->init);
                int32_t cv = 0;
                bool is_const = is_const_int_expr(ctx, vd->init, &cv);
                if (!is_assignable(ctx, vtype, init_type, is_const, cv) &&
                    !is_array_init_narrowable(ctx, vtype, vd->init) &&
                    !jt_is_error(init_type)) {
                    sema_error(ctx, s->loc, "incompatible initializer for '%s'",
                               vd->name);
                }
                retype_array_init(ctx, vd->init, vtype);
            }
            if (slot >= 0) {
                bbq_htree_insert(ctx->slot_allocs, ptr_key(vd),
                                 (void*)(uintptr_t)(slot + 1));
                java_type_t* tp = (java_type_t*)bbq_arena_alloc(ctx->arena, sizeof(java_type_t));
                *tp = vtype;
                bbq_htree_insert(ctx->local_types, ptr_key(vd), tp);
            }
        }
        break;
    }

    case AST_EXPRSTMT: {
        analyze_expr(ctx, s->expr_stmt.e);
        /* JLS §14.8: an expression statement must be one of:
         * assignment, pre/post inc/dec, method invocation, class
         * instance creation. A bare arithmetic expression like
         * `1 + 2;` is not a valid statement. */
        ast_expr_t* es = s->expr_stmt.e;
        if (es) {
            bool ok = false;
            switch (es->tag) {
            case AST_ASSIGN:
            case AST_COMPOUNDASSIGN:
            case AST_METHODCALL:
            case AST_SUPERCALL:
            case AST_CONSTRUCTORCALL:
            case AST_NEW:
                ok = true;
                break;
            case AST_UNARY:
                if (es->unary.op == AST_PREINC || es->unary.op == AST_PREDEC ||
                    es->unary.op == AST_POSTINC || es->unary.op == AST_POSTDEC)
                    ok = true;
                break;
            default:
                break;
            }
            if (!ok)
                sema_error(ctx, s->loc,
                    "not a statement: expression result is unused");
        }
        break;
    }

    case AST_IF: {
        java_type_t cond = analyze_expr(ctx, s->if_.test);
        if (!jt_is_error(cond) && cond.tag != JT_BOOL)
            sema_error(ctx, s->loc, "if condition must be boolean");
        analyze_stmt(ctx, s->if_.then);
        analyze_stmt(ctx, s->if_.else_);
        break;
    }

    case AST_WHILE: {
        java_type_t cond = analyze_expr(ctx, s->while_.test);
        if (!jt_is_error(cond) && cond.tag != JT_BOOL)
            sema_error(ctx, s->loc, "while condition must be boolean");
        ctx->loop_depth++;
        frame_push_loop(ctx, s);
        analyze_stmt(ctx, s->while_.body);
        frame_pop(ctx);
        ctx->loop_depth--;
        break;
    }

    case AST_DOWHILE: {
        ctx->loop_depth++;
        frame_push_loop(ctx, s);
        analyze_stmt(ctx, s->do_while.body);
        frame_pop(ctx);
        ctx->loop_depth--;
        java_type_t cond = analyze_expr(ctx, s->do_while.test);
        if (!jt_is_error(cond) && cond.tag != JT_BOOL)
            sema_error(ctx, s->loc, "do-while condition must be boolean");
        break;
    }

    case AST_FOR: {
        scope_push(ctx);
        if (s->for_.init) analyze_stmt(ctx, s->for_.init);
        if (s->for_.test) {
            java_type_t cond = analyze_expr(ctx, s->for_.test);
            if (!jt_is_error(cond) && cond.tag != JT_BOOL)
                sema_error(ctx, s->loc, "for condition must be boolean");
        }
        for (int i = 0; i < s->for_.update_count; i++)
            analyze_expr(ctx, s->for_.update[i]);
        ctx->loop_depth++;
        frame_push_loop(ctx, s);
        analyze_stmt(ctx, s->for_.body);
        frame_pop(ctx);
        ctx->loop_depth--;
        scope_pop(ctx);
        break;
    }

    case AST_SWITCH: {
        java_type_t sel = analyze_expr(ctx, s->switch_.selector);
        if (!jt_is_error(sel) && !jt_is_integral(sel))
            sema_error(ctx, s->loc, "switch selector must be integral");

        /* JLS §14.11: case value must fit the promoted selector type.
         * Byte/bool selectors narrow to -128..127, short to the int16
         * range, int has no extra check. A case value outside its
         * selector's range can never match — the compiler should
         * reject it, not silently accept a dead case. */
        int32_t range_min = INT32_MIN, range_max = INT32_MAX;
        if (!jt_is_error(sel) &&
            (sel.tag == JT_BYTE || sel.tag == JT_BOOL || sel.tag == JT_SHORT)) {
            range_min = jtype_min[sel.tag];
            range_max = jtype_max[sel.tag];
        }

        ctx->loop_depth++; /* for break */
        frame_push_switch(ctx, s);
        /* JLS §14.11: at most one default; case values must be
         * compile-time constants and unique within the switch. */
        bool seen_default = false;
        for (int i = 0; i < s->switch_.cases_count; i++) {
            ast_switch_case_t* sc = s->switch_.cases[i];
            if (sc->value) {
                analyze_expr(ctx, sc->value);
                int32_t cv = 0;
                if (!is_const_integral_expr(ctx, sc->value, &cv)) {
                    sema_error(ctx, s->loc,
                        "switch case value must be a compile-time constant");
                } else {
                    /* Range check against selector type. */
                    if (cv < range_min || cv > range_max) {
                        sema_error(ctx, s->loc,
                            "case value %d out of range for switch selector "
                            "type (allowed: %d..%d)", cv, range_min, range_max);
                    }
                    /* Check for duplicate case value */
                    for (int j = 0; j < i; j++) {
                        ast_switch_case_t* prev = s->switch_.cases[j];
                        int32_t pv = 0;
                        if (prev->value && is_const_integral_expr(ctx, prev->value, &pv)
                            && pv == cv) {
                            sema_error(ctx, s->loc,
                                "duplicate case value %d", cv);
                            break;
                        }
                    }
                }
            } else {
                if (seen_default)
                    sema_error(ctx, s->loc, "duplicate default in switch");
                seen_default = true;
            }
            for (int j = 0; j < sc->stmts_count; j++)
                analyze_stmt(ctx, sc->stmts[j]);
        }
        frame_pop(ctx);
        ctx->loop_depth--;

        /* Build the sema_switch_info side table. Errors above may
         * have already recorded diagnostics; we still build the info
         * so downstream compilation doesn't choke on a partial
         * analysis. Non-constant case values fall back to 0. */
        sema_switch_info_t* info = (sema_switch_info_t*)
            bbq_arena_alloc(ctx->arena, sizeof(sema_switch_info_t));
        info->default_idx = -1;

        int ncases = 0;
        for (int i = 0; i < s->switch_.cases_count; i++) {
            if (s->switch_.cases[i]->value) ncases++;
            else if (info->default_idx < 0) info->default_idx = i;
        }

        info->cases_count = ncases;
        info->case_values = ncases > 0
            ? (int32_t*)bbq_arena_alloc(ctx->arena, (size_t)ncases * sizeof(int32_t))
            : NULL;
        info->case_ast_indices = ncases > 0
            ? (int*)bbq_arena_alloc(ctx->arena, (size_t)ncases * sizeof(int))
            : NULL;

        int k = 0;
        for (int i = 0; i < s->switch_.cases_count; i++) {
            ast_switch_case_t* sc = s->switch_.cases[i];
            if (!sc->value) continue;
            int32_t cv = 0;
            (void)is_const_integral_expr(ctx, sc->value, &cv);
            info->case_values[k] = cv;
            info->case_ast_indices[k] = i;
            k++;
        }

        /* Sort case values ascending — keep case_ast_indices in sync.
         * Small N — insertion sort over a (value, ast_index) pair. */
        for (int i = 1; i < ncases; i++) {
            int32_t v = info->case_values[i];
            int     ai = info->case_ast_indices[i];
            int j = i - 1;
            while (j >= 0 && info->case_values[j] > v) {
                info->case_values[j + 1] = info->case_values[j];
                info->case_ast_indices[j + 1] = info->case_ast_indices[j];
                j--;
            }
            info->case_values[j + 1] = v;
            info->case_ast_indices[j + 1] = ai;
        }

        if (ncases > 0) {
            info->low  = info->case_values[0];
            info->high = info->case_values[ncases - 1];
        } else {
            info->low = info->high = 0;
        }

        /* Degeneracy classification. Target-equality detection walks
         * the AST (two cases share a target if their body pointer
         * sequences are structurally identical). For cheap but
         * effective coverage we check the common trivial shapes:
         * default-only (no cases), and all-cases-plus-default empty
         * (all bodies are just `break;` — compiler will fold to a
         * single goto anyway). */
        if (ncases == 0) {
            info->degeneracy = SEMA_SWITCH_DEFAULT_ONLY;
        } else {
            /* All-same-target detection is expensive and rare;
             * leaving NORMAL lets compiler.c fall through to the
             * standard SIR_SWITCH path. A future pass can upgrade
             * this to detect `case N: break` chains with common
             * join points. */
            info->degeneracy = SEMA_SWITCH_NORMAL;
        }

        bbq_htree_insert(ctx->switch_infos, ptr_key(s), info);
        break;
    }

    case AST_TRY: {
        ctx->uses_exceptions = true;
        int prev_caught = bbq_vec_len(ctx->caught_types);
        for (int i = 0; i < s->try_.catches_count; i++) {
            java_type_t ctype = resolve_type(ctx, s->try_.catches[i]->ty, s->loc);
            if (ctype.tag == JT_CLASS)
                bbq_vec_push(ctx->caught_types, ctype);
        }
        analyze_stmt(ctx, s->try_.body);
        if (ctx->caught_types)
            bbq__vec_hdr(ctx->caught_types)->len = prev_caught;
        for (int i = 0; i < s->try_.catches_count; i++) {
            ast_catch_clause_t* cc = s->try_.catches[i];
            scope_push(ctx);
            java_type_t ct = resolve_type(ctx, cc->ty, s->loc);
            if (!jt_is_error(ct) && ct.tag == JT_CLASS &&
                !is_throwable_subclass(ctx, ct.class_id)) {
                sema_error(ctx, s->loc,
                    "catch type '%s' is not a subclass of Throwable",
                    ctx->classes[ct.class_id].name);
            }
            /* JLS §11.2.3: catch clauses must be ordered most-specific
             * first. A clause is unreachable if a previous clause's
             * type is a supertype. */
            if (ct.tag == JT_CLASS) {
                for (int j = 0; j < i; j++) {
                    ast_catch_clause_t* prev = s->try_.catches[j];
                    java_type_t prev_t = resolve_type(ctx, prev->ty, s->loc);
                    if (prev_t.tag == JT_CLASS &&
                        is_subclass_of(ctx, ct.class_id, prev_t.class_id)) {
                        sema_error(ctx, s->loc,
                            "catch clause for '%s' is unreachable; "
                            "shadowed by earlier catch for '%s'",
                            ctx->classes[ct.class_id].name,
                            ctx->classes[prev_t.class_id].name);
                        break;
                    }
                }
            }
            int32_t slot = scope_declare(ctx, cc->name, ct, s->loc);
            if (slot >= 0) {
                bbq_htree_insert(ctx->slot_allocs, ptr_key(cc),
                                 (void*)(uintptr_t)(slot + 1));
            }
            analyze_stmt(ctx, cc->body);
            scope_pop(ctx);
        }
        analyze_stmt(ctx, s->try_.finally_);
        break;
    }

    case AST_THROW: {
        ctx->uses_exceptions = true;
        java_type_t t = analyze_expr(ctx, s->throw_.e);
        if (jt_is_error(t)) break;
        if (!jt_is_reference(t)) {
            sema_error(ctx, s->loc, "throw requires reference type");
        } else if (t.tag == JT_CLASS && !is_throwable_subclass(ctx, t.class_id)) {
            sema_error(ctx, s->loc,
                "throw target type '%s' is not a subclass of Throwable",
                ctx->classes[t.class_id].name);
        } else if (t.tag == JT_CLASS) {
            if (!is_unchecked_exc(ctx, t.class_id)) {
                bool handled = false;
                if (ctx->current_method) {
                    for (int di = 0; di < ctx->current_method->thrown_count; di++) {
                        java_type_t dt = ctx->current_method->thrown_types[di];
                        if (dt.tag == JT_CLASS &&
                            (dt.class_id == t.class_id ||
                             is_subclass_of(ctx, t.class_id, dt.class_id)))
                            { handled = true; break; }
                    }
                }
                for (int ci2 = 0; !handled && ci2 < bbq_vec_len(ctx->caught_types); ci2++) {
                    java_type_t ct = ctx->caught_types[ci2];
                    if (ct.tag == JT_CLASS &&
                        (ct.class_id == t.class_id ||
                         is_subclass_of(ctx, t.class_id, ct.class_id)))
                        handled = true;
                }
                if (!handled)
                    sema_error(ctx, s->loc,
                        "unhandled checked exception: throws %s",
                        ctx->classes[t.class_id].name);
            }
        }
        break;
    }

    case AST_RETURN:
        if (s->return_.value) {
            java_type_t t = analyze_expr(ctx, s->return_.value);
            /* JLS §8.8.7: a constructor body may have `return;` but
             * not `return expr;` — a constructor's return type is
             * implicitly void and the constructor returns an object
             * via the special initializer protocol. */
            if (ctx->in_constructor) {
                sema_error(ctx, s->loc,
                    "'return' with value not allowed in constructor");
            } else {
                int32_t cv = 0;
                bool is_const = is_const_int_expr(ctx, s->return_.value, &cv);
                if (!is_assignable(ctx, ctx->current_return_type, t, is_const, cv) &&
                    !jt_is_error(t)) {
                    sema_error(ctx, s->loc, "incompatible return type");
                }
            }
        } else if (!ctx->in_constructor && ctx->current_return_type.tag != JT_VOID) {
            sema_error(ctx, s->loc, "missing return value");
        }
        break;

    case AST_BREAK:
    case AST_CONTINUE: {
        const char* lbl = (s->tag == AST_BREAK) ? s->break_.label : s->continue_.label;
        int target_idx = -1;
        if (lbl) {
            target_idx = frame_find_label(ctx, lbl);
            if (target_idx < 0) {
                sema_error(ctx, s->loc, "label '%s' not found", lbl);
            } else if (s->tag == AST_CONTINUE &&
                       ctx->frames[target_idx].kind != SEMA_FRAME_LOOP) {
                sema_error(ctx, s->loc,
                    "continue target '%s' is not a loop", lbl);
            }
        } else {
            target_idx = (s->tag == AST_BREAK)
                ? frame_find_innermost_break_target(ctx)
                : frame_find_innermost_loop(ctx);
            if (target_idx < 0) {
                sema_error(ctx, s->loc, "%s outside loop",
                           s->tag == AST_BREAK ? "break" : "continue");
            }
        }
        /* Record depth = (innermost frame) - (target frame), so 0 means
         * "innermost frame is the target." Codegen walks `depth` parent
         * links from current ρ. Keyed off the stmt pointer; the +1
         * sentinel mirrors other side-tables (htree treats 0 as "no
         * entry"). */
        if (target_idx >= 0) {
            int depth = bbq_vec_len(ctx->frames) - 1 - target_idx;
            bbq_htree* tbl = (s->tag == AST_BREAK)
                ? ctx->break_target_depths
                : ctx->continue_target_depths;
            bbq_htree_insert(tbl, (uint32_t)(uintptr_t)s,
                             (void*)(intptr_t)(depth + 1));
            /* Mark the loop as one whose continue target is a real label —
             * two transfers reach it, so the backend must frame it. */
            if (s->tag == AST_CONTINUE && ctx->frames[target_idx].stmt)
                bbq_htree_insert(ctx->loops_with_continue,
                                 (uint32_t)(uintptr_t)ctx->frames[target_idx].stmt,
                                 (void*)1);
        }
        break;
    }

    case AST_LABELED: {
        bool lbl_is_loop_or_switch = s->labeled.body &&
            (s->labeled.body->tag == AST_WHILE ||
             s->labeled.body->tag == AST_DOWHILE ||
             s->labeled.body->tag == AST_FOR ||
             s->labeled.body->tag == AST_SWITCH);
        /* §14.6: "A statement labeled by an identifier must not appear anywhere within another
         * statement labeled by the same identifier, or a compile-time error will occur. Two
         * statements can be labeled by the same identifier ONLY IF NEITHER STATEMENT CONTAINS
         * THE OTHER."
         *
         * So the rule is CONTAINMENT, not reuse. ctx->labels is pushed on entry and popped
         * after the body, so whatever is on it here is exactly the set of enclosing labels --
         * searching it rejects the nested case and leaves two sibling loops sharing a label
         * alone, which the spec explicitly permits and t14.label.siblings.same.identifier
         * exercises. Banning the repeated identifier outright would pass the rejection and
         * break that one. */
        for (int li = 0; li < bbq_vec_len(ctx->labels); li++) {
            if (strcmp(ctx->labels[li].name, s->labeled.label) == 0) {
                sema_error(ctx, s->loc,
                    "label '%s' is already the label of an enclosing statement",
                    s->labeled.label);
                break;
            }
        }
        /* Keep the legacy `labels` stack populated for any code still
         * consulting it (sema's own diagnostics). */
        sema_label_t entry = { s->labeled.label,
                               s->labeled.body &&
                               (s->labeled.body->tag == AST_WHILE ||
                                s->labeled.body->tag == AST_DOWHILE ||
                                s->labeled.body->tag == AST_FOR) };
        bbq_vec_push(ctx->labels, entry);
        if (lbl_is_loop_or_switch) {
            /* Pass the label down — the body's loop/switch picks it up
             * in frame_push_loop and clears pending_frame_label. */
            const char* prev = ctx->pending_frame_label;
            ctx->pending_frame_label = s->labeled.label;
            analyze_stmt(ctx, s->labeled.body);
            ctx->pending_frame_label = prev;
        } else {
            /* Labeled block — push our own frame (continue can't target
             * a labeled block; codegen uses continue_target=nil). */
            frame_push_labeled_block(ctx, s->labeled.label);
            analyze_stmt(ctx, s->labeled.body);
            frame_pop(ctx);
        }
        (void)bbq_vec_pop(ctx->labels);
        break;
    }

    case AST_EMPTY:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Exception-type helpers
 * ═══════════════════════════════════════════════════════════════ */

/* Returns true if `class_id` is Throwable or a subclass — by the cached
 * well-known id (resolve_wellknown), not a name walk. */
static bool is_throwable_subclass(const sema_ctx_t* ctx, int class_id) {
    return is_subclass_of(ctx, class_id, ctx->wk.throwable_id);
}


/* ═══════════════════════════════════════════════════════════════
 * Pass 2: Analyze method/constructor bodies
 * ═══════════════════════════════════════════════════════════════ */

static void analyze_bodies(sema_ctx_t* ctx) {
    /* ctx->analyze_from is 0 for a normal whole-program run. When a caller has
     * already type-checked a prefix (the java.lang prelude) it is the count of
     * those classes, so their ~450 method bodies are not re-checked. */
    for (int ci = ctx->analyze_from; ci < bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        ctx->current_class_id = ci;

        /* Check field initializers */
        {
            ast_type_decl_t* td = c->ast_node;
            if (td) {
                ast_member_t** members = td->tag == AST_CLASSDECL ?
                    td->class_decl.members : td->interface_decl.members;
                int mc2 = td->tag == AST_CLASSDECL ?
                    td->class_decl.members_count : td->interface_decl.members_count;
                for (int fi = 0; fi < mc2; fi++) {
                    if (members[fi]->tag == AST_FIELDDECL) {
                        ast_member_t* fm = members[fi];
                        /* §8.3.2: an instance field initializer is instance context (it may reference
                         * `this` and instance fields); a static field initializer is static context.
                         * Set it explicitly — the previous method/class analyzed leaves it in an
                         * arbitrary state, and an instance initializer that reads an instance field
                         * would otherwise be spuriously rejected. */
                        ctx->in_static_context =
                            (modifiers_to_flags(fm->field_decl.mods, fm->field_decl.mods_count) & ACC_STATIC) != 0;
                        java_type_t ftype = resolve_type(ctx, fm->field_decl.ty, fm->loc);
                        for (int di = 0; di < fm->field_decl.decls_count; di++) {
                            ast_var_decl_t* vd = fm->field_decl.decls[di];
                            if (vd->init) {
                                java_type_t vtype = declarator_type(ctx, ftype, vd->dims);
                                scope_push(ctx);
                                /* §8.3.3: in a static field init, only
                                 * fields declared before this one are
                                 * visible. find_field_index returns the
                                 * field's position in the class vec. */
                                int prev_limit = ctx->static_init_field_limit;
                                /* §8.3.2.3: both static and instance field
                                 * initializers restrict forward references. */
                                for (int fi2 = 0; fi2 < bbq_vec_len(c->fields); fi2++) {
                                    if (strcmp(c->fields[fi2].name, vd->name) == 0) {
                                        ctx->static_init_field_limit = fi2;
                                        break;
                                    }
                                }
                                java_type_t it = analyze_expr(ctx, vd->init);
                                ctx->static_init_field_limit = prev_limit;
                                scope_pop(ctx);
                                int32_t cv = 0;
                                bool is_const = is_const_int_expr(ctx, vd->init, &cv);
                                if (!is_assignable(ctx, vtype, it, is_const, cv) &&
                                    !is_array_init_narrowable(ctx, vtype, vd->init) &&
                                    !jt_is_error(it))
                                    sema_error(ctx, fm->loc,
                                        "incompatible initializer for field '%s'", vd->name);
                                retype_array_init(ctx, vd->init, vtype);
                            }
                        }
                    }
                }
            }
        }

        /* §8.3.1.2: "A field can be declared final, in which case its declarator must
         * include a variable initializer or a compile-time error occurs." Java 1.0 has no
         * blank finals, static or instance. Neither a static initializer block nor a
         * constructor assignment is a substitute, because each is itself "an attempt to
         * assign to a final field", which the same paragraph makes an error — so the rule
         * is a property of the DECLARATOR alone and needs no definite-assignment analysis.
         *
         * Imported classes are skipped: their fields carry no declaration-site AST, so a
         * NULL init_expr there says nothing about how the field was written. */
        if (c->ast_node) {
            for (int fi = 0; fi < bbq_vec_len(c->fields); fi++) {
                const sema_field_t* ff = &c->fields[fi];
                if ((ff->modifiers & ACC_FINAL) && !ff->init_expr)
                    sema_error(ctx, c->ast_node->loc,
                        "final field '%s' has no initializer in its declarator",
                        ff->name);
            }
        }

        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            ast_member_t* ast = m->ast_node;
            if (!ast) continue; /* imported method, no body to analyze */
            ast_stmt_t* body = NULL;

            if (ast->tag == AST_METHODDECL)
                body = ast->method_decl.body;
            else if (ast->tag == AST_CONSTRUCTORDECL)
                body = ast->constructor_decl.body;

            if (!body) continue; /* abstract or native */

            /* JLS §8.8.7: if a constructor body does not begin with an
             * explicit this(…) or super(…), prepend an implicit
             * `super()` invocation so the parent chain is initialized.
             * Without this the verifier (which now enforces §4.10.1.9
             * uninitializedThis rules) correctly rejects the missing
             * invokespecial, and the VM skips the parent's side
             * effects (e.g. a superclass constructor's registration).
             * Skipped for Object itself (no super). Synthesized into
             * the same arena as the rest of the AST — downstream
             * passes see it as an ordinary AST_CONSTRUCTORCALL. */
            if (m->is_constructor && body->tag == AST_BLOCK &&
                c->super_id != -1) {
                bool has_explicit = false;
                if (body->block.stmts_count > 0) {
                    ast_stmt_t* first = body->block.stmts[0];
                    if (first->tag == AST_EXPRSTMT && first->expr_stmt.e &&
                        first->expr_stmt.e->tag == AST_CONSTRUCTORCALL)
                        has_explicit = true;
                }
                if (!has_explicit) {
                    ast_expr_t* call = (ast_expr_t*)bbq_arena_alloc(
                        ctx->arena, sizeof(ast_expr_t));
                    memset(call, 0, sizeof(*call));
                    call->tag = AST_CONSTRUCTORCALL;
                    call->loc = ast->loc;
                    call->constructor_call.is_super = true;
                    call->constructor_call.args = NULL;
                    call->constructor_call.args_count = 0;

                    ast_stmt_t* stmt = (ast_stmt_t*)bbq_arena_alloc(
                        ctx->arena, sizeof(ast_stmt_t));
                    memset(stmt, 0, sizeof(*stmt));
                    stmt->tag = AST_EXPRSTMT;
                    stmt->loc = ast->loc;
                    stmt->expr_stmt.e = call;

                    int new_count = body->block.stmts_count + 1;
                    ast_stmt_t** new_stmts = (ast_stmt_t**)bbq_arena_alloc(
                        ctx->arena, (size_t)new_count * sizeof(ast_stmt_t*));
                    new_stmts[0] = stmt;
                    for (int si = 0; si < body->block.stmts_count; si++)
                        new_stmts[si + 1] = body->block.stmts[si];
                    body->block.stmts = new_stmts;
                    body->block.stmts_count = new_count;
                }
            }

            ctx->current_return_type = m->return_type;
            ctx->in_static_context = (m->modifiers & ACC_STATIC) != 0;
            ctx->in_static_init = false;
            ctx->in_constructor = m->is_constructor;
            ctx->current_method = m;
            ctx->loop_depth = 0;
            bbq_vec_free(ctx->frames);
            ctx->frames = NULL;
            ctx->pending_frame_label = NULL;
            ctx->next_slot = 0;  /* Phase B: reset per-method slot counter */

            scope_push(ctx);

            /* Phase B: declarations during this+params loop are PARAM. */
            ctx->declaring_params = true;

            /* Declare 'this' for instance methods (gets slot 0) */
            if (!(m->modifiers & ACC_STATIC)) {
                ast_srcloc loc = ast->loc;
                scope_declare(ctx, "this", jt_class(ci), loc);
            }

            /* Declare parameters (slots after this, monotonically) */
            for (int pi = 0; pi < m->param_count; pi++) {
                ast_srcloc loc = ast->loc;
                scope_declare(ctx, m->param_names[pi], m->param_types[pi], loc);
            }

            ctx->declaring_params = false;

            /* Analyze body — body locals and catch vars allocate
             * slots from ctx->next_slot via scope_declare. */
            analyze_stmt(ctx, body);
            scope_pop(ctx);

            /* Phase B: record per-method slot count for DDCG */
            m->max_user_slots = ctx->next_slot;


            /* Constructor delegation order (JLS §8.8.7.1): if this
             * is a constructor whose body is a block, any
             * this()/super() must be at index 0. */
            if (m->is_constructor && body->tag == AST_BLOCK) {
                for (int si = 1; si < body->block.stmts_count; si++) {
                    ast_stmt_t* s = body->block.stmts[si];
                    if (s->tag == AST_EXPRSTMT && s->expr_stmt.e &&
                        s->expr_stmt.e->tag == AST_CONSTRUCTORCALL) {
                        sema_error(ctx, s->loc,
                            "constructor call must be first statement of constructor body");
                    }
                }
            }

            /* Reachability / unreachable-code, definite-assignment,
             * and missing-return checks are all run by analyses_run
             * on the dataflow engine after analyze_bodies completes. */
        }

        /* Static initializers */
        ast_type_decl_t* td = c->ast_node;
        if (!td) continue; /* built-in */
        ast_member_t** members;
        int member_count;
        if (td->tag == AST_CLASSDECL) {
            members = td->class_decl.members;
            member_count = td->class_decl.members_count;
        } else {
            members = td->interface_decl.members;
            member_count = td->interface_decl.members_count;
        }
        /* Static initializer blocks (JLS §8.7): analyzed as a static-context
         * body. All such blocks (across every class) fold into one synthesized
         * <clinit>, so their local slots draw from a single shared counter
         * (clinit_next_slot) to keep distinct ranges in that one function. */
        for (int mi = 0; mi < member_count; mi++) {
            if (members[mi]->tag != AST_STATICINIT) continue;
            ast_stmt_t* body = members[mi]->static_init.body;
            ctx->current_return_type = jt_prim(JT_VOID);
            ctx->current_class_id = ci;
            ctx->in_static_context = true;
            ctx->in_static_init = true;
            ctx->in_constructor = false;
            ctx->current_method = NULL;
            ctx->loop_depth = 0;
            bbq_vec_free(ctx->frames); ctx->frames = NULL;
            ctx->pending_frame_label = NULL;
            ctx->next_slot = ctx->clinit_next_slot;  /* continue the shared space */
            scope_push(ctx);
            ctx->declaring_params = false;
            analyze_stmt(ctx, body);
            scope_pop(ctx);
            ctx->in_static_init = false;
            ctx->clinit_next_slot = ctx->next_slot;   /* reserve this block's slots */
        }
    }
}

int32_t sema_clinit_slots(const sema_ctx_t* ctx) { return ctx->clinit_next_slot; }

/* ═══════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════ */

void sema_init(sema_ctx_t* ctx, bbq_arena* arena) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->arena = arena;
    ctx->classes = NULL; /* bbq_vec */
    ctx->class_by_name = bbq_htree_create();
    ctx->scopes = NULL;
    ctx->expr_types = bbq_htree_create();
    ctx->resolved_methods = bbq_htree_create();
    ctx->simd_imms = bbq_htree_create();
    ctx->import_funcs = NULL;          /* bbq_vec, accumulated at call resolution */
    ctx->resolved_fields = bbq_htree_create();
    ctx->data_types = bbq_htree_create();
    ctx->slot_allocs = bbq_htree_create();
    ctx->local_types = bbq_htree_create();
    ctx->ident_kinds = bbq_htree_create();
    ctx->resolved_ctors = bbq_htree_create();
    ctx->invoke_kinds = bbq_htree_create();
    ctx->target_classes = bbq_htree_create();
    ctx->side_effects = bbq_htree_create();
    ctx->array_init_elem_types = bbq_htree_create();
    ctx->instanceof_types = bbq_htree_create();
    ctx->switch_infos = bbq_htree_create();
    ctx->break_target_depths = bbq_htree_create();
    ctx->continue_target_depths = bbq_htree_create();
    ctx->loops_with_continue = bbq_htree_create();
    ctx->type_class_ids = bbq_htree_create();
    ctx->units = NULL;   /* bbq_vec */
    ctx->labels = NULL;
    ctx->frames = NULL;
    ctx->pending_frame_label = NULL;
    ctx->diags = NULL;

    /* No built-in class stubs. Object / Throwable / the whole
     * java.lang hierarchy come from lang.exp — callers must load
     * the runtime export manifest before analyzing user source, or
     * name resolution for classes like implicit-Object supers and
     * throw/catch types will error loudly with a srcloc. The old
     * Object/Throwable sentinels pretended to satisfy those lookups
     * but produced CAP entries referencing class_id 0 (unbound in
     * the runtime classpool), so the loader would silently bind
     * them to the wrong slot at install time. */
    ctx->static_init_field_limit = -1;
    ctx->caught_types = NULL;
    ctx->current_method = NULL;
}

/* Detect cycles in constructor this()-delegation chains
 * (JLS §8.8.7). For each constructor whose body's first statement
 * is a this(...) call, walk the target chain via resolved_ctors;
 * if we revisit the start, the chain is recursive. */
static void validate_constructor_chains(sema_ctx_t* ctx) {
    for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* start = &c->methods[mi];
            if (!start->is_constructor || !start->ast_node) continue;
            if (start->ast_node->tag != AST_CONSTRUCTORDECL) continue;
            ast_stmt_t* body = start->ast_node->constructor_decl.body;
            if (!body || body->tag != AST_BLOCK || body->block.stmts_count == 0)
                continue;

            const sema_method_t* cur = start;
            int depth = 0;
            int max_depth = 0;
            for (int x = 0; x < bbq_vec_len(ctx->classes); x++)
                max_depth += bbq_vec_len(ctx->classes[x].methods);

            for (;;) {
                if (!cur->ast_node ||
                    cur->ast_node->tag != AST_CONSTRUCTORDECL) break;
                ast_stmt_t* cb = cur->ast_node->constructor_decl.body;
                if (!cb || cb->tag != AST_BLOCK || cb->block.stmts_count == 0) break;
                ast_stmt_t* first = cb->block.stmts[0];
                if (first->tag != AST_EXPRSTMT) break;
                ast_expr_t* ce = first->expr_stmt.e;
                if (!ce || ce->tag != AST_CONSTRUCTORCALL) break;
                if (ce->constructor_call.is_super) break;  /* super() can't cycle */
                const sema_method_t* next = sema_resolved_constructor(ctx, ce);
                if (!next) break;
                if (next == start) {
                    sema_error(ctx, start->ast_node->loc,
                        "recursive constructor delegation in class '%s'",
                        c->name);
                    goto next_method;
                }
                if (++depth > max_depth) {
                    /* Cycle that doesn't pass through start — still bad. */
                    sema_error(ctx, start->ast_node->loc,
                        "recursive constructor chain reachable from class '%s'",
                        c->name);
                    goto next_method;
                }
                cur = next;
            }
            next_method:;
        }
    }
}

/* Resolve the spec-defined well-known java.lang classes (JLS 1.0 §20/§11.5) ONCE
 * into ctx->wk, by name from the loaded prelude. The compiler OWNS the JRE
 * interface, so an absent class is a hard error (broken prelude) caught here —
 * not a silent -1 surfacing at a use site. This table is the ONE place the
 * canonical java.lang names live in C; everything else reads ctx->wk.<role>_id. */
static void resolve_wellknown(sema_ctx_t* ctx) {
    static const struct { size_t off; const char* name; } table[] = {
        { offsetof(sema_wellknown_t, object_id),              "Object" },
        { offsetof(sema_wellknown_t, throwable_id),           "Throwable" },
        { offsetof(sema_wellknown_t, exception_id),           "Exception" },
        { offsetof(sema_wellknown_t, error_id),               "Error" },
        { offsetof(sema_wellknown_t, runtime_exception_id),   "RuntimeException" },
        { offsetof(sema_wellknown_t, cloneable_id),           "Cloneable" },
        { offsetof(sema_wellknown_t, string_id),              "String" },
        { offsetof(sema_wellknown_t, string_buffer_id),       "StringBuffer" },
        { offsetof(sema_wellknown_t, class_reflect_id),       "Class" },
        { offsetof(sema_wellknown_t, null_pointer_id),        "NullPointerException" },
        { offsetof(sema_wellknown_t, array_index_oob_id),     "ArrayIndexOutOfBoundsException" },
        { offsetof(sema_wellknown_t, class_cast_id),          "ClassCastException" },
        { offsetof(sema_wellknown_t, negative_array_size_id), "NegativeArraySizeException" },
        { offsetof(sema_wellknown_t, arithmetic_id),          "ArithmeticException" },
        { offsetof(sema_wellknown_t, array_store_id),         "ArrayStoreException" },
        { offsetof(sema_wellknown_t, index_oob_id),           "IndexOutOfBoundsException" },
        { offsetof(sema_wellknown_t, exc_in_init_id),         "ExceptionInInitializerError" },
        { offsetof(sema_wellknown_t, no_class_def_id),        "NoClassDefFoundError" },
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        char fq[64];
        snprintf(fq, sizeof fq, "java.lang.%s", table[i].name);   /* FQN-keyed table (§7.5.1) */
        int id = sema_find_class(ctx, fq);
        if (id < 0)
            sema_error(ctx, (ast_srcloc){0},
                       "runtime class 'java.lang.%s' missing from the JRE prelude",
                       table[i].name);
        *(int*)((char*)&ctx->wk + table[i].off) = id;
    }
    /* javelina.simd.V128 — soft, unlike the java.lang table: a program with no
     * SIMD library still compiles; one that names V128 without it gets the
     * ordinary "unknown type" error. */
    ctx->wk.v128_id = sema_find_class(ctx, "javelina.simd.V128");
}

/* Object.getClass()'s method index (§20.1.1) — its forwarder reads field 0 (the object's
 * Class) rather than calling a host import. Resolved AFTER register_members (getClass is
 * only in Object's methods vec by then). */
/* Stamp the first method named `name` in `class_id` with a Move* intrinsic kind (§20.9/§20.10
 * raw bit accessors). Done ONCE at well-known resolution; call sites read m->move_kind. */
/* Does `m`'s parameter list match the generated intrinsic descriptor `ptypes`
 * (one char per parameter — V=V128, I=int, J=long, F=float, D=double)?
 *
 * The bind from a javelina.simd stub to its wasm opcode has to name the METHOD,
 * and a name does not (§8.4.7). Matching on the name alone worked only while the
 * generated stubs held one method per name; a convenience overload — an `add`
 * taking a scalar beside one taking a vector, which is what a usable API is made
 * of — would take whichever entry came first and emit its opcode. */
static bool simd_sig_matches(const sema_method_t* m, const char* ptypes) {
    int n = ptypes ? (int)strlen(ptypes) : 0;
    if (m->param_count != n) return false;
    for (int i = 0; i < n; i++) {
        java_type_tag_t t = m->param_types[i].tag;
        switch (ptypes[i]) {
            /* V128 is the v128 VALUE type — sema maps the class to the value
             * width at every use site, so a parameter of it is JT_V128, never a
             * reference. */
            case 'V': if (t != JT_V128)   return false; break;
            case 'I': if (t != JT_INT)    return false; break;
            case 'J': if (t != JT_LONG)   return false; break;
            case 'F': if (t != JT_FLOAT)  return false; break;
            case 'D': if (t != JT_DOUBLE) return false; break;
            default:  return false;      /* '?' — the generator could not type it */
        }
    }
    return true;
}

/* The index of the method in `class_id` named `name` taking exactly the one parameter type
 * `param` and returning `ret`, or -1.
 *
 * A name does not identify a method — §8.4.7 lets any number of methods share one, and
 * java.lang.Math alone has twelve overloads across abs/min/max/round. Every stamper below
 * attaches a LOWERING decision (an opcode, a bitcast, a call form) to a method, so picking
 * the wrong overload emits the wrong instruction for the right-looking source. Passing
 * `JT_VOID` for `param` matches a no-argument method. */
static int find_method_sig(sema_ctx_t* ctx, int class_id, const char* name,
                           java_type_tag_t param, java_type_tag_t ret) {
    if (class_id < 0) return -1;
    sema_class_t* c = &ctx->classes[class_id];
    int want_params = (param == JT_VOID) ? 0 : 1;
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++) {
        const sema_method_t* m = &c->methods[i];
        if (strcmp(m->name, name) != 0) continue;
        if (m->param_count != want_params) continue;
        if (want_params && m->param_types[0].tag != param) continue;
        if (m->return_type.tag != ret) continue;
        return i;
    }
    return -1;
}

static void stamp_move_kind(sema_ctx_t* ctx, int class_id, const char* name, int kind,
                            java_type_tag_t param, java_type_tag_t ret) {
    int i = find_method_sig(ctx, class_id, name, param, ret);
    if (i >= 0) ctx->classes[class_id].methods[i].move_kind = kind;
}

/* Stamp the method `double name(double)` in `class_id` with a Math f64-op intrinsic kind
 * (§20.11 sqrt/floor/ceil/rint — each a single wasm opcode). Same one-time-at-resolution
 * model as move_kind.
 *
 * The SIGNATURE is part of the key, not just the name. Each of these four is declared once
 * in §20.11, so a name alone identifies them today — but they share a class with abs, min,
 * max and round, which have twelve overloads between them, and a first-match-by-name stamp
 * would attach the intrinsic to whichever overload the methods vector happens to list first.
 * That is the same wrong-key defect as resolving an ident by name and taking what turns up:
 * it would lower `abs(double)` to an i32 op the moment anyone stamped an overloaded name. */
static void stamp_math_kind(sema_ctx_t* ctx, int class_id, const char* name, int kind) {
    int i = find_method_sig(ctx, class_id, name, JT_DOUBLE, JT_DOUBLE);
    if (i >= 0) ctx->classes[class_id].methods[i].math_kind = kind;
}

/* Stamp the first method named `name` in `class_id` as never returning null.
 *
 * The §7 summary derives COMPILER_RET_NONNULL by proving no reachable `return` can yield null —
 * but only from a BODY, and the whole point of the well-known table is that the RTL is not
 * recompiled with every plugin. A compiled-library build has no java.lang bodies to analyse, so
 * a contract the library spec guarantees is simply lost unless it is stated. */
static void stamp_ret_nonnull(sema_ctx_t* ctx, int class_id, const char* name) {
    if (class_id < 0) return;
    sema_class_t* c = &ctx->classes[class_id];
    /* EVERY overload, not the first: String.valueOf has nine, StringBuffer.append and insert
     * have a dozen each, substring has two. The contract is the name's, and stamping one of
     * nine would be a mechanism that silently covers a ninth of what it claims. (The other
     * stampers here return after one match because their targets have unique names.) */
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
        if (strcmp(c->methods[i].name, name) == 0) c->methods[i].ret_nonnull = true;
}

/* §20.3.6: stamp Class's two newInstance helper natives with their intrinsic kind, so each lowers
 * to a ClassInstantiable/ClassConstruct SIR node over the receiver Class (never a real call/import).
 * Same one-time-at-resolution model as move_kind/math_kind. */
static void stamp_class_kind(sema_ctx_t* ctx, int class_id, const char* name, int kind,
                             java_type_tag_t ret) {
    int i = find_method_sig(ctx, class_id, name, JT_VOID, ret);
    if (i >= 0) ctx->classes[class_id].methods[i].class_kind = kind;
}

static void resolve_wellknown_methods(sema_ctx_t* ctx) {
    ctx->wk.getclass_method_id = -1;
    ctx->wk.finalize_method_id = -1;
    if (ctx->wk.object_id < 0) return;
    const sema_class_t* obj = &ctx->classes[ctx->wk.object_id];
    for (int i = 0; i < (int)bbq_vec_len(obj->methods); i++) {
        if (strcmp(obj->methods[i].name, "getClass") == 0) ctx->wk.getclass_method_id = i;
        else if (strcmp(obj->methods[i].name, "finalize") == 0) ctx->wk.finalize_method_id = i;
    }
    /* §10.10: Class.arrayStoreCheck(Class, Object) — the reference-array store guard.
     * §15.19.2: Class.isInstance(Object) — the reference-array instanceof/checkcast guard. */
    ctx->wk.arraystore_check_method_id = -1;
    ctx->wk.is_instance_method_id = -1;
    if (ctx->wk.class_reflect_id >= 0) {
        const sema_class_t* cls = &ctx->classes[ctx->wk.class_reflect_id];
        for (int i = 0; i < (int)bbq_vec_len(cls->methods); i++) {
            if (strcmp(cls->methods[i].name, "arrayStoreCheck") == 0) ctx->wk.arraystore_check_method_id = i;
            else if (strcmp(cls->methods[i].name, "isInstance") == 0) ctx->wk.is_instance_method_id = i;
        }
    }
    /* §20.18.16 System.arraycopy — resolved to a `SIR_ARRAYCOPY` (array.copy) intrinsic rather
     * than an import. Soft (no error if absent): the intrinsic only fires when both resolve. */
    ctx->wk.system_id = sema_find_class(ctx, "java.lang.System");
    ctx->wk.arraycopy_method_id = -1;
    if (ctx->wk.system_id >= 0) {
        const sema_class_t* sys = &ctx->classes[ctx->wk.system_id];
        for (int i = 0; i < (int)bbq_vec_len(sys->methods); i++)
            if (strcmp(sys->methods[i].name, "arraycopy") == 0) { ctx->wk.arraycopy_method_id = i; break; }
    }
    /* §20.9/§20.10 raw bit accessors → Move* bitcast intrinsics. Stamp the method identity ONCE
     * here (soft: no-op if the class/method is absent); call sites read m->move_kind. */
    ctx->wk.float_id  = sema_find_class(ctx, "java.lang.Float");
    ctx->wk.double_id = sema_find_class(ctx, "java.lang.Double");
    stamp_move_kind(ctx, ctx->wk.float_id,  "floatToRawIntBits",   1, JT_FLOAT,  JT_INT);
    stamp_move_kind(ctx, ctx->wk.float_id,  "intBitsToFloat",      2, JT_INT,    JT_FLOAT);
    stamp_move_kind(ctx, ctx->wk.double_id, "doubleToRawLongBits", 3, JT_DOUBLE, JT_LONG);
    stamp_move_kind(ctx, ctx->wk.double_id, "longBitsToDouble",    4, JT_LONG,   JT_DOUBLE);
    /* §20.11 Math f64 ops with a direct wasm opcode → inline intrinsics (never a host import). The
     * transcendentals with no opcode stay real methods (a Java fdlibm impl). */
    /* ── §20 library contracts: reference results that are never null ────────────────────
     * STATED, not derived. The §7 summary proves this from a body, and the RTL is not
     * recompiled with every plugin — against a compiled java.lang there is no body — so a
     * contract the library spec guarantees is simply lost unless it is declared here.
     *
     * The list is the SPEC's, not what today's tests happen to exercise: each of these
     * returns a freshly built object, `this`, or a canonical instance, and none has a
     * `return null` anywhere in the RTL (verified per class; java.lang.Class is the one class
     * that does return null — getClassLoader, §20.3.7 "this model has no class loaders" — and
     * is therefore NOT stamped).
     *
     * Soundness for a subclass override: the stamp is consulted per RESOLVED TARGET, so an
     * override that returned null would be a different (unstamped) method. Only the
     * short-circuit in cp_invoke_ret_fresh skips resolution, and it additionally requires the
     * class or method to be final. String is final and Object.getClass is a final method, so
     * those short-circuit; StringBuffer is NOT final — §20.13 declares `public class
     * StringBuffer`, and later Java making it final is not this spec — so its stamps take the
     * resolved-target path instead. Still sound, just not free. */
    stamp_ret_nonnull(ctx, ctx->wk.object_id, "getClass");        /* §20.1.5 — final method */

    stamp_ret_nonnull(ctx, ctx->wk.string_id, "intern");          /* §20.12.47 canonical instance */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "toString");        /* §20.12.13 returns this      */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "substring");       /* §20.12.35/36               */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "concat");          /* §20.12.37                  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "replace");         /* §20.12.38                  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "toLowerCase");     /* §20.12.39                  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "toUpperCase");     /* §20.12.41                  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "trim");            /* §20.12.43                  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "toCharArray");     /* §20.12.45 — a fresh char[] */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "valueOf");         /* §20.12.48-56 — never null  */
    stamp_ret_nonnull(ctx, ctx->wk.string_id, "copyValueOf");     /* §20.12.9/10                */

    /* java.lang.Class. NOT getSuperclass — it is null for Object (§20.3.4) — and NOT
     * getClassLoader, which this model returns null from by design (§20.3.7). Nor newInstance:
     * it bottoms out in a native, so its result cannot be verified by reading the RTL, and a
     * contract stated on faith is the thing this table exists to avoid. */
    stamp_ret_nonnull(ctx, ctx->wk.class_reflect_id, "getName");   /* §20.3.1 — a fresh String */
    stamp_ret_nonnull(ctx, ctx->wk.class_reflect_id, "toString");  /* §20.3.2 — concat, never null */
    stamp_ret_nonnull(ctx, ctx->wk.class_reflect_id, "forName");   /* §20.3.8 — returns or throws */

    stamp_ret_nonnull(ctx, ctx->wk.string_buffer_id, "append");   /* §20.13.9-24 — returns this */
    stamp_ret_nonnull(ctx, ctx->wk.string_buffer_id, "insert");   /* §20.13.26-36 — returns this*/
    stamp_ret_nonnull(ctx, ctx->wk.string_buffer_id, "reverse");  /* §20.13.25 — returns this   */
    stamp_ret_nonnull(ctx, ctx->wk.string_buffer_id, "toString"); /* §20.13.8                   */

    int math_id = sema_find_class(ctx, "java.lang.Math");
    stamp_math_kind(ctx, math_id, "sqrt",  1);
    stamp_math_kind(ctx, math_id, "floor", 2);
    stamp_math_kind(ctx, math_id, "ceil",  3);
    stamp_math_kind(ctx, math_id, "rint",  4);

    /* javelina.simd — every generated intrinsic stamped from the generated
     * table (the toml's 6th consumer; the table and the stub classes come from
     * ONE generator run, so a row that fails to stamp means the stubs and the
     * table diverged — loud, not soft: that desync would silently turn an
     * intrinsic into a host import). Soft only when the simd library itself is
     * absent (wk.v128_id < 0, no simd classes loaded). */
    if (ctx->wk.v128_id >= 0) {
        for (int si = 0; si < SIMD_INTRINSIC_COUNT; si++) {
            const simd_intrinsic_t* r = &simd_intrinsics[si];
            char fq[64];
            snprintf(fq, sizeof fq, "javelina.simd.%s", r->cls);   /* FQN-keyed (§7.5.1) */
            int cid = sema_find_class(ctx, fq);
            sema_method_t* m = NULL;
            if (cid >= 0) {
                sema_class_t* c = &ctx->classes[cid];
                for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
                    if (strcmp(c->methods[i].name, r->method) == 0
                            && simd_sig_matches(&c->methods[i], r->ptypes))
                        { m = &c->methods[i]; break; }
            }
            if (!m) {
                sema_error(ctx, (ast_srcloc){0},
                           "simd intrinsic '%s.%s' is in the generated table but not "
                           "in the loaded stubs — regenerate (make gen)", r->cls, r->method);
                continue;
            }
            m->simd_id = si + 1;
        }
    }

    /* §15.17.3: `%` on float/double is the truncated remainder (C fmod), and WASM has
     * no f32.rem/f64.rem opcode — so the ddcg desugars it to a call to Math's fdlibm
     * fmod, the way `+` on Strings desugars to StringBuffer calls. Bind both overloads
     * by parameter width. Soft (stays -1 if absent): the lowering only fires when the
     * helper resolves, and a missing tile then fails loud in the backend. */
    ctx->wk.math_id = math_id;
    ctx->wk.fmod_float_id = -1;
    ctx->wk.fmod_double_id = -1;
    if (math_id >= 0) {
        const sema_class_t* mc = &ctx->classes[math_id];
        for (int i = 0; i < (int)bbq_vec_len(mc->methods); i++) {
            const sema_method_t* m = &mc->methods[i];
            if (strcmp(m->name, "fmod") != 0 || m->param_count != 2) continue;
            if (m->param_types[0].tag == JT_FLOAT)  ctx->wk.fmod_float_id  = i;
            if (m->param_types[0].tag == JT_DOUBLE) ctx->wk.fmod_double_id = i;
        }
    }

    stamp_class_kind(ctx, ctx->wk.class_reflect_id, "instantiable", 1, JT_BOOL); /* §20.3.6 */
    stamp_class_kind(ctx, ctx->wk.class_reflect_id, "construct",    2, JT_CLASS);
}

/* Stamp every method/field with its (declaring class, class-local index) — the stable IDENTITY
 * that resolution stores and codegen reads. So NO query recovers a member's (class, index) by a
 * pointer-range search over the vecs (undefined across allocations; stale after a synth realloc):
 * it is read straight off the struct. Idempotent — positions are fixed by append order, so a
 * re-stamp after synth only fills in newly-appended members (their structs may have moved, but the
 * owner/index VALUES move with them). */
static void stamp_member_identity(sema_ctx_t* ctx) {
    for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) { c->methods[j].owner = ci; c->methods[j].index = j; }
        for (int j = 0; j < (int)bbq_vec_len(c->fields);  j++) { c->fields[j].owner  = ci; c->fields[j].index  = j; }
    }
}

bool sema_analyze(sema_ctx_t* ctx, ast_program_t* program) {
    return sema_analyze_units(ctx, &program, 1);
}

bool sema_analyze_units(sema_ctx_t* ctx, ast_program_t** units, int n) {
    collect_decls(ctx, units, n);
    /* Mark the bundled java.lang runtime (the lowest class_ids — registered ahead of
     * user code) as library BEFORE body analysis: a library class is the extern API,
     * so its methods AND constructors are host imports, never emitted. Import
     * classification happens at call resolution (sema_note_import) during
     * analyze_bodies, so import_pkg must be set first. */
    for (int ci = 0; ci < ctx->num_library_classes && ci < (int)bbq_vec_len(ctx->classes); ci++)
        ctx->classes[ci].import_pkg = 0;
    /* (§20.3.2 note: fq_name is now the REAL §7.4.1 name, set at registration
     * from the unit's package declaration — the old "everything bundled is
     * java.lang" prefix hack lied for java.io/java.util/javelina.simd.) */
    validate_hierarchy(ctx);
    /* §20.3.6: `static Object $newInstance() { return new C(); }` per instantiable class. It carries
     * a real AST body, so it must exist BEFORE the member re-stamp and body analysis that follow —
     * it is type-checked and lowered exactly like source, through the one `new C()` lowering. */
    synth_new_instance_methods(ctx);
    stamp_member_identity(ctx);   /* source members carry their (class, index) before resolution reads it */
    analyze_bodies(ctx);
    synth_array_classes(ctx);   /* §10.8: every array type used is now registered → give each its Class */
    synth_clone_methods(ctx);   /* §20.1.5: per-type internalClone override for Cloneable classes + overlays */
    synth_ensure_init_methods(ctx);  /* §12.4.2: per-needs_init-class `$ensure_init` (lazy class-init barrier target) */
    synth_main_method(ctx);          /* E7.1a: the `$main(argc,base)->int` program-entry wrapper (if a main exists) */
    stamp_member_identity(ctx);   /* re-stamp: synth appended array-class + internalClone + $ensure_init + $main members */
    validate_constructor_chains(ctx);
    if (sema_error_count(ctx) == 0) analyses_run(ctx);

    /* Build the emitted-function table: defined methods in (class, method) order.
     * Each entry's position IS its module function index — the single authority
     * the InvokeStatic immediate and the module assembler read (sema_func_*). */
    bbq_vec_free(ctx->functions);
    ctx->functions = NULL;
    for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
        const sema_class_t* c = sema_get_class(ctx, ci);
        for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++) {
            if (!sema_method_is_defined(ctx, ci, &c->methods[mi])) continue;
            sema_func_ent_t e = { ci, mi };
            bbq_vec_push(ctx->functions, e);
        }
    }
    /* No mode-dependent rewriting of the reference set happens here. It is what
     * sema_note_import accumulated during analysis — one entry per resolved reference, JLS
     * §13.1 — and every consumer that wants something else (a WASM import section, which also
     * needs a funcref for vtable slots this module never names) derives it downstream.
     *
     * ── The §8.4.6.1 dispatch table ───────────────────────────────────────────────────────
     * Every virtual signature a class inherits or declares, with the implementation that
     * actually runs for it. sema_resolve_virtual is the one rule; this walks each class's OWN
     * ancestry (super chain + interfaces) rather than every class pair, and publishes the
     * answers so no consumer re-walks per dispatch site. */
    /* §13.1, the per-class half: "A Java binary representation for a class or interface must
     * also contain all of the following: If it is a class and is not class java.lang.Object,
     * then a symbolic reference to the direct superclass of this class; a symbolic reference to
     * each direct superinterface, if any." Recorded per class, not per body, so
     * current_class_id is set around it. */
    {
        int saved = ctx->current_class_id;
        for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
            const sema_class_t* c = sema_get_class(ctx, ci);
            ctx->current_class_id = ci;
            if (!c->is_interface && ci != ctx->wk.object_id && c->super_id >= 0)
                sema_note_ref(ctx, SEMA_REF_SUPERCLASS, c->super_id, -1);
            for (int ii = 0; ii < c->interface_count; ii++)
                sema_note_ref(ctx, SEMA_REF_SUPERINTERFACE, c->interface_ids[ii], -1);
        }
        ctx->current_class_id = saved;
    }

    /* Keyed by the declaration a CALL SITE NAMES, so the lookup below needs no signature
     * comparison: a row exists for every (exact class, declaring class, method) the SIR can
     * ask about. Deduplicating by signature would have been wrong — an overriding class would
     * lose the row for the introducing declaration, and `Base.g` on a `Sub` receiver is exactly
     * what a call site names (JLS §13.1: the class in which the method is DECLARED).
     *
     * Rows for one class are contiguous, and vtarget_base indexes them, so a consumer scans
     * only that class's rows and compares two ints. That is what lets Click's transfer stay
     * O(inputs): sema_resolve_virtual walks the ancestry and vsig-compares every method at each
     * level, which inside a fixpoint runs per receiver, per call site, per iteration. */
    bbq_vec_free(ctx->vtargets); ctx->vtargets = NULL;
    bbq_vec_free(ctx->vtarget_base); ctx->vtarget_base = NULL;
    int ncls = (int)bbq_vec_len(ctx->classes);
    for (int ci = 0; ci < ncls; ci++) {
        int base = (int)bbq_vec_len(ctx->vtargets);
        bbq_vec_push(ctx->vtarget_base, base);
        /* Every class ci is a subtype of — its super chain AND every interface reachable
         * through it, at any depth. sema_ref_is_subtype (§4.10.2) is the one authority for
         * that relation; walking `interface_ids` directly sees only the first level and misses
         * an interface that extends another. */
        for (int ai = 0; ai < ncls; ai++) {
            if (!sema_ref_is_subtype(ctx, ci, ai)) continue;
            const sema_class_t* a = sema_get_class(ctx, ai);
            for (int mi = 0; mi < (int)bbq_vec_len(a->methods); mi++) {
                if (!sema_is_virtual_method(&a->methods[mi])) continue;
                int rc = -1, rm = -1;
                sema_resolve_virtual(ctx, ci, ai, mi, &rc, &rm);   /* leaves -1/-1 if abstract */
                sema_vtarget_ent_t v = { ci, ai, mi, rc, rm };
                bbq_vec_push(ctx->vtargets, v);
            }
        }
    }
    bbq_vec_push(ctx->vtarget_base, (int)bbq_vec_len(ctx->vtargets));   /* sentinel */

    if (ctx->mode == SEMA_MODE_RUNTIME) {
        /* jre must export the COMPLETE java.lang native surface — a plugin may call/vtable a
         * native java.lang never itself referenced (e.g. String.getBytes). Add every concrete
         * library native to import_funcs (deduped) so it gets a funcidx (primitive → direct
         * host import; ref-carrying → forwarded below) and is exported. */
        for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++) {
            const sema_class_t* c = sema_get_class(ctx, ci);
            if (c->import_pkg < 0 || !c->ast_node) continue;
            for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++) {
                const sema_method_t* m = &c->methods[mi];
                if (!((m->modifiers & ACC_NATIVE) && !(m->modifiers & SEMA_ACC_ABSTRACT))) continue;
                if (sema_method_is_defined(ctx, ci, m)) continue;   /* bodied/synth already in functions */
                bool dup = false;
                for (int k = 0; k < (int)bbq_vec_len(ctx->import_funcs); k++)
                    if (ctx->import_funcs[k].class_id == ci && ctx->import_funcs[k].method_id == mi) { dup = true; break; }
                if (!dup) { sema_func_ent_t e = { ci, mi }; bbq_vec_push(ctx->import_funcs, e); }
            }
        }
    }

    /* A referenced native whose signature carries a reference (its receiver, a
     * parameter, or its return) gets a marshaling FORWARDER: a defined function
     * with the method's natural type that bridges to the externref-typed host
     * import (§3.3.10 makes a struct-typed import unlinkable, so the forwarder
     * converts any↔extern at the boundary). Appended as a defined entry so it
     * flows through the function/type/vtable machinery like a compiled method.
     * A primitive-only native stays a direct import (natural type == host-ABI). */
    for (int i = 0; i < (int)bbq_vec_len(ctx->import_funcs); i++) {
        sema_func_ent_t e = ctx->import_funcs[i];
        const sema_class_t* ic = sema_get_class(ctx, e.class_id);
        if (ctx->mode == SEMA_MODE_PLUGIN && ic->import_pkg >= 0 && ic->ast_node) continue;  /* java.lang: jre's forwarder */
        const sema_method_t* m = &ic->methods[e.method_id];
        bool needs = !(m->modifiers & ACC_STATIC)
                  || m->return_type.tag == JT_CLASS || m->return_type.tag == JT_ARRAY;
        for (int p = 0; !needs && p < m->param_count; p++)
            needs = m->param_types[p].tag == JT_CLASS || m->param_types[p].tag == JT_ARRAY;
        if (needs) bbq_vec_push(ctx->functions, e);
    }
    /* The table is FINAL here — both append loops above are its only writers.
     * (Re)build its index in place; sema_func_index reads it from now on. */
    bbq_hmap_free(&ctx->functions_idx);
    bbq_hmap_init(&ctx->functions_idx, 2 * bbq_vec_len(ctx->functions));
    for (int i = 0; i < (int)bbq_vec_len(ctx->functions); i++) {
        uint64_t k = ((uint64_t)(uint32_t)ctx->functions[i].class_id << 32)
                   | (uint32_t)ctx->functions[i].method_id;
        bbq_hmap_put(&ctx->functions_idx, k, (void*)(intptr_t)(i + 1));
    }
    return sema_error_count(ctx) == 0;
}

/* Does a method carry a compilable body? (Mirrors the compiler's body detection
 * — one authority for "has a body".) */
static bool method_has_body(const sema_method_t* m) {
    if (!m->ast_node) return false;
    if (m->ast_node->tag == AST_METHODDECL)      return m->ast_node->method_decl.body != NULL;
    if (m->ast_node->tag == AST_CONSTRUCTORDECL) return m->ast_node->constructor_decl.body != NULL;
    return false;
}

bool sema_method_is_defined(const sema_ctx_t* ctx, int class_id, const sema_method_t* m) {
    const sema_class_t* c = sema_get_class(ctx, class_id);
    /* A synthesized class (no source) — an array overlay — owns no source methods, but its
     * compiler-synthesized §20.1.5 internalClone IS a defined function (else clone() on an
     * array falls through to Object's placeholder and aliases). */
    if (!c->ast_node) return m->is_synthetic_clone || m->is_synthetic_ensure_init;
    /* A library class splits two ways. Members with a BODY are emitted as defined
     * functions: CONSTRUCTORS are §12.5 object-init code (super-chain, field inits)
     * the VM knows nothing about, and a compiled OVERLAY method (e.g. String.length()
     * reading its own char[] value field) is the module operating on its own GC data.
     * Emitting both avoids importing a func type that references a module struct (the
     * §3.3.10 host-match headache). The genuine extern API is the NATIVE (bodiless)
     * methods — the only true environment edges — recorded referenced-only as host
     * imports by sema_note_import. */
    if (c->import_pkg >= 0) {
        /* PLUGIN: a real java.lang SOURCE class's bodies live in jre.wasm — this thin module
         * IMPORTS them (below the funcidx range), never defines them. (Synthesized array
         * overlays hit the !ast_node branch above and stay defined locally in every mode.) */
        if (ctx->mode == SEMA_MODE_PLUGIN && c->ast_node) return false;
        return m->is_synthetic_default || m->is_synthetic_clone || m->is_synthetic_ensure_init
            || m->is_synthetic_new_instance || method_has_body(m);
    }
    return m->is_synthetic_clone || m->is_synthetic_ensure_init || m->is_synthetic_main
        || m->is_synthetic_new_instance || method_has_body(m);
}

int sema_func_count(const sema_ctx_t* ctx) { return (int)bbq_vec_len(ctx->functions); }

sema_func_ent_t sema_func_at(const sema_ctx_t* ctx, int i) { return ctx->functions[i]; }

int sema_import_count(const sema_ctx_t* ctx) { return (int)bbq_vec_len(ctx->import_funcs); }
sema_func_ent_t sema_import_at(const sema_ctx_t* ctx, int i) { return ctx->import_funcs[i]; }

int sema_ref_count(const sema_ctx_t* ctx) { return (int)bbq_vec_len(ctx->refs); }
sema_ref_ent_t sema_ref_at(const sema_ctx_t* ctx, int i) { return ctx->refs[i]; }

int sema_vtarget_count(const sema_ctx_t* ctx) { return (int)bbq_vec_len(ctx->vtargets); }
sema_vtarget_ent_t sema_vtarget_at(const sema_ctx_t* ctx, int i) { return ctx->vtargets[i]; }

void sema_vtarget_range(const sema_ctx_t* ctx, int exact, int* lo, int* hi) {
    *lo = *hi = 0;
    if (!ctx || exact < 0 || exact + 1 >= (int)bbq_vec_len(ctx->vtarget_base)) return;
    *lo = ctx->vtarget_base[exact];
    *hi = ctx->vtarget_base[exact + 1];
}

bool sema_vtarget_find(const sema_ctx_t* ctx, int exact, int decl_class, int decl_method,
                       int* out_class, int* out_method) {
    if (!ctx || exact < 0 || exact + 1 >= (int)bbq_vec_len(ctx->vtarget_base)) return false;
    int lo = ctx->vtarget_base[exact], hi = ctx->vtarget_base[exact + 1];
    for (int i = lo; i < hi; i++) {
        const sema_vtarget_ent_t* v = &ctx->vtargets[i];
        if (v->decl_class != decl_class || v->decl_method != decl_method) continue;
        if (v->impl_class < 0) return false;        /* abstract: no body to call */
        if (out_class)  *out_class  = v->impl_class;
        if (out_method) *out_method = v->impl_method;
        return true;
    }
    return false;
}

int sema_func_index(const sema_ctx_t* ctx, int class_id, int method_id) {
    uint64_t k = ((uint64_t)(uint32_t)class_id << 32) | (uint32_t)method_id;
    void* v = bbq_hmap_get(&ctx->functions_idx, k);
    return v ? (int)(intptr_t)v - 1 : -1;
}

java_type_t sema_type_of(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    java_type_t* t = (java_type_t*)bbq_htree_search(ctx->expr_types, ptr_key(expr));
    return t ? *t : jt_error();
}

const sema_switch_info_t* sema_switch_info(const sema_ctx_t* ctx,
                                            const ast_stmt_t* stmt) {
    return (const sema_switch_info_t*)bbq_htree_search(ctx->switch_infos, ptr_key(stmt));
}

const sema_method_t* sema_resolved_method(const sema_ctx_t* ctx, const ast_expr_t* call) {
    return decode_method_loc(ctx, bbq_htree_search(ctx->resolved_methods, ptr_key(call)));
}

/* The ONE resolution of a catch clause's exception class. Its readers are the §14.19 catch-block
 * reachability check and the backend's exception-table emit; when they each resolved the name
 * themselves they disagreed — one handled `catch (java.lang.InterruptedException e)` and the other
 * silently answered "unknown" and skipped its rule. The declared type may be written as a simple
 * name or a fully qualified one. */
int sema_catch_class_id(const sema_ctx_t* ctx, const ast_catch_clause_t* cc) {
    if (!cc || !cc->ty || cc->ty->tag != AST_CLASSTYPE) return -1;
    /* Read resolve_type's §6.5.4 record for the clause's type node — never
     * re-resolve the spelled name here (resolution is unit-relative). */
    void* v = bbq_htree_search(ctx->type_class_ids, ptr_key(cc->ty));
    return v ? (int)(uintptr_t)v - 1 : -1;
}

const sema_field_t* sema_resolved_field(const sema_ctx_t* ctx, const ast_expr_t* access) {
    return decode_field_loc(ctx, bbq_htree_search(ctx->resolved_fields, ptr_key(access)));
}

int sema_find_class(const sema_ctx_t* ctx, const char* name) {
    if (!name) return -1;
    /* FQN-keyed (§7.5.1: "the compiler keeps track of types by their fully
     * qualified names"). Use htree_contains to distinguish "class_id = 0"
     * (the built-in Object) from "not in table." */
    uint32_t key = str_hash(name);
    if (!bbq_htree_contains(ctx->class_by_name, key)) return -1;
    return (int)(uintptr_t)bbq_htree_search(ctx->class_by_name, key);
}

/* §6.6 class-type accessibility from unit `ui`: public, or same package. */
static bool type_accessible(const sema_ctx_t* ctx, int ui, int cid) {
    if (ctx->classes[cid].modifiers & ACC_PUBLIC) return true;
    const char* dpkg = class_package(ctx, cid);
    const char* upkg = (ui >= 0) ? ctx->units[ui].package : NULL;
    if (!dpkg && !upkg) return true;
    return dpkg && upkg && strcmp(dpkg, upkg) == 0;
}

/* §6.5.4 — the meaning of a type name spelled in compilation unit `ui`,
 * transcribed IN ORDER from JLS 1.0 p.93-94.
 *
 * Qualified `Q.Id` (§6.5.4.2): Q must be a package name, Id a type declared
 * in it, accessible (§6.6) — else a compile-time error.
 *
 * Simple `Id` (§6.5.4.1):
 *   1. a type with that name declared in the CURRENT unit, or named by one
 *      of its single-type-import declarations (§7.5.1);
 *   2. otherwise, a type with that name declared in another unit of the
 *      package containing the identifier (§7.1);
 *   3. otherwise, a type declared by EXACTLY ONE type-import-on-demand
 *      declaration of the unit (§7.5.2 — the automatic java.lang.* included);
 *   4. otherwise, more than one on-demand match: ambiguous, compile-time error;
 *   5. otherwise, undefined: compile-time error.
 *
 * `probe` suppresses the errors (for §6.5.2 ambiguous-name reclassification,
 * where "not a type" just means "keep classifying"). Returns class id or -1. */
int sema_resolve_type(sema_ctx_t* ctx, int ui, const char* spelled,
                      ast_srcloc loc, bool probe) {
    if (!spelled) return -1;
    if (strchr(spelled, '.')) {                        /* §6.5.4.2 Q.Id */
        int cid = sema_find_class(ctx, spelled);
        if (cid < 0) {
            if (!probe) sema_error(ctx, loc, "unknown type '%s'", spelled);
            return -1;
        }
        if (!type_accessible(ctx, ui, cid)) {
            if (!probe) sema_error(ctx, loc,
                "type '%s' is not accessible from this package (§6.6)", spelled);
            return -1;
        }
        return cid;
    }
    const sema_unit_t* u = (ui >= 0 && ui < (int)bbq_vec_len(ctx->units))
                         ? &ctx->units[ui] : NULL;
    /* Step 1a: declared in the current unit. */
    if (u)
        for (int ci = 0; ci < (int)bbq_vec_len(ctx->classes); ci++)
            if (ctx->classes[ci].unit_idx == ui &&
                strcmp(ctx->classes[ci].name, spelled) == 0) return ci;
    /* Step 1b: named by a single-type-import of the unit. */
    if (u)
        for (int i = 0; i < (int)bbq_vec_len(u->singles); i++)
            if (strcmp(fq_simple(u->singles[i]), spelled) == 0) {
                int cid = sema_find_class(ctx, u->singles[i]);
                if (cid >= 0) return cid;              /* missing import already errored */
            }
    /* Step 2: another unit of the current package (incl. the unnamed one —
     * synthetics and unnamed-package units share the simple-name namespace). */
    {
        const char* pkg = u ? u->package : NULL;
        int cid = sema_find_class(ctx, make_fq_name(ctx, pkg, spelled));
        if (cid >= 0) return cid;
    }
    /* Step 3: exactly one type-import-on-demand (public types only, §7.5.2). */
    if (u) {
        int hit = -1, nhits = 0;
        for (int i = 0; i < (int)bbq_vec_len(u->ondemands); i++) {
            int cid = sema_find_class(ctx, make_fq_name(ctx, u->ondemands[i], spelled));
            if (cid < 0 || !(ctx->classes[cid].modifiers & ACC_PUBLIC)) continue;
            if (hit != cid) { hit = cid; nhits++; }
        }
        if (nhits == 1) return hit;
        if (nhits > 1) {                               /* step 4 */
            if (!probe) sema_error(ctx, loc,
                "type name '%s' is ambiguous: more than one import-on-demand "
                "declares it (§6.5.4.1)", spelled);
            return -1;
        }
    }
    if (!probe) sema_error(ctx, loc, "unknown type '%s'", spelled);   /* step 5 */
    return -1;
}

const sema_class_t* sema_get_class(const sema_ctx_t* ctx, int class_id) {
    if (class_id < 0 || class_id >= bbq_vec_len(ctx->classes)) return NULL;
    return &ctx->classes[class_id];
}

/* JLS §12.4: does class_id require initialization (§12.4.2 barrier)? The DDCG's active-use rules
 * gate the init barrier on this — the single authority for "which classes init." */
bool sema_class_needs_init(const sema_ctx_t* ctx, int class_id) {
    const sema_class_t* c = sema_get_class(ctx, class_id);
    return c && c->needs_init;
}

/* The class-local method index (cp) of class_id's synthesized `$ensure_init`, or -1. The DDCG builds
 * `InvokeStatic(class_id, cp)` as the barrier statement. */
int sema_ensure_init_cp(const sema_ctx_t* ctx, int class_id) {
    const sema_class_t* c = sema_get_class(ctx, class_id);
    if (!c) return -1;
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
        if (c->methods[i].is_synthetic_ensure_init) return i;
    return -1;
}

int sema_error_count(const sema_ctx_t* ctx) {
    int count = 0;
    for (int i = 0; i < bbq_vec_len(ctx->diags); i++)
        if (ctx->diags[i].level == DIAG_ERROR) count++;
    return count;
}

const sema_diag_t* sema_diags(const sema_ctx_t* ctx, int* count) {
    *count = bbq_vec_len(ctx->diags);
    return ctx->diags;
}

/* The field a field-access expr resolves to: qualified (FieldAccess, via the
 * resolved_fields table) or unqualified (Ident bound to an instance/static
 * field). NULL if it is not a field access. */
static const sema_field_t* field_of_expr(const sema_ctx_t* ctx, const ast_expr_t* e) {
    if (!e) return NULL;
    if (e->tag == AST_FIELDACCESS) return sema_resolved_field(ctx, e);
    if (e->tag == AST_IDENT) {
        const sema_ident_info_t* info = sema_ident_kind(ctx, e);
        if (info && (info->kind == SEMA_IDENT_INSTANCE_FIELD ||
                     info->kind == SEMA_IDENT_STATIC_FIELD))
            return info->field;
    }
    return NULL;
}

int sema_field_index(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    const sema_field_t* f = field_of_expr(ctx, expr);
    return f ? f->index : -1;            /* class-local position in the field vec */
}

int sema_field_decl_class(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    const sema_field_t* f = field_of_expr(ctx, expr);
    return f ? f->owner : -1;            /* declaring class — stamped identity, no address search */
}

/* The class_id of a class-typed AST type node (instanceof / cast target).
 * Reads resolve_type's §6.5.4 record for the node — no re-resolution
 * (resolution is unit-relative). -1 if sema never resolved this node. */
int sema_type_class_id(const sema_ctx_t* cctx, const ast_type_t* ty) {
    if (!ty || ty->tag != AST_CLASSTYPE) return -1;
    void* v = bbq_htree_search(cctx->type_class_ids, ptr_key(ty));
    return v ? (int)(uintptr_t)v - 1 : -1;
}

/* The method a call expr resolves to: a normal method (resolved_methods) or a
 * constructor (resolved_ctors). NULL if it is not a resolved call. */
static const sema_method_t* method_of_expr(const sema_ctx_t* ctx, const ast_expr_t* e) {
    const sema_method_t* m = sema_resolved_method(ctx, e);
    return m ? m : sema_resolved_constructor(ctx, e);
}

/* The called method's index (position in its declaring class's methods vec) and
 * that declaring class — the WASM function identity, the way field accesses
 * resolve struct fields. -1 if the expr resolves to no method. */
int sema_method_index(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    const sema_method_t* m = method_of_expr(ctx, expr);
    return m ? m->index : -1;            /* class-local position — stamped identity */
}

int sema_method_decl_class(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    const sema_method_t* m = method_of_expr(ctx, expr);
    return m ? m->owner : -1;            /* declaring class — stamped identity, no address search */
}

int32_t sema_data_type(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    void* v = bbq_htree_search(ctx->data_types, ptr_key(expr));
    return v ? (int32_t)((int)(uintptr_t)v - 1) : -1;
}

/* ── JLS §15.18.1 string concatenation: the well-known StringBuffer method
 * identities the ddcg needs to defunctionalize `a + b` into
 * new StringBuffer().append(a).append(b).toString(). append/toString/<init> are
 * all declared in StringBuffer (the compiled overlay), so the target class is
 * always wk.string_buffer_id and only the method index varies. ── */
static int sb_method_idx(const sema_ctx_t* ctx, const sema_method_t* m) {
    if (!m || ctx->wk.string_buffer_id < 0) return -1;
    return (m->owner == ctx->wk.string_buffer_id) ? m->index : -1;   /* StringBuffer's own, else inherited */
}

/* §15.17.3: Math's class id, and the fmod overload matching the remainder's width. */
int sema_frem_class(const sema_ctx_t* ctx) { return ctx->wk.math_id; }
int sema_frem_method(const sema_ctx_t* ctx, int32_t dt) {
    if (dt == SIR_DTFLOAT)  return ctx->wk.fmod_float_id;
    if (dt == SIR_DTDOUBLE) return ctx->wk.fmod_double_id;
    return -1;                      /* integral % is a Rem node (guarded, §15.16.2) */
}

/* True iff `node` is a `+` whose sema result type is String (i.e. concatenation). */
bool sema_binary_is_concat(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_BINARY || node->binary.op != AST_ADD) return false;
    java_type_t t = sema_type_of(ctx, node);
    return t.tag == JT_CLASS && t.class_id == ctx->wk.string_id;
}
int sema_string_buffer_id(const sema_ctx_t* ctx) { return ctx->wk.string_buffer_id; }
int sema_sb_ctor_index(const sema_ctx_t* ctx) {
    bool he;
    return sb_method_idx(ctx, find_constructor(ctx, ctx->wk.string_buffer_id, 0, NULL, &he));
}
int sema_sb_tostring_index(const sema_ctx_t* ctx) {
    return sb_method_idx(ctx, find_method(ctx, ctx->wk.string_buffer_id, "toString", 0, NULL));
}
/* §15.17.1.1 string conversion → the append overload for `operand`:
 *   • a String     → append(String)   (String.toString() is the identity);
 *   • ANY OTHER reference (class, array, null) → append(Object): a reference is
 *     converted via toString(), which append(Object)→String.valueOf(obj) performs
 *     (and null/toString-returning-null both yield "null" through append(String));
 *   • a primitive  → append(int/long/char/boolean/float/double) (each wraps + toStrings).
 * NB: append(char[]) is NOT used for a char[] operand — §15.17.1.1 converts an array
 * by toString() ("[C@…"), i.e. append(Object). */
static java_type_t sb_append_target_type(const sema_ctx_t* ctx, java_type_t at) {
    if (at.tag == JT_CLASS && at.class_id == ctx->wk.string_id) return at;
    if (at.tag == JT_CLASS || at.tag == JT_ARRAY || at.tag == JT_NULL)
        return jt_class(ctx->wk.object_id);
    return at;   /* primitive */
}
int sema_sb_append_index(const sema_ctx_t* ctx, const ast_expr_t* operand) {
    java_type_t t = sb_append_target_type(ctx, sema_type_of(ctx, operand));
    return sb_method_idx(ctx, find_method(ctx, ctx->wk.string_buffer_id, "append", 1, &t));
}
/* The class id of `operand`'s append overload's (reference) parameter — String or
 * Object — used to give the arg temp a concrete ref descriptor (a null operand has
 * none of its own). -1 for a primitive param. */
int sema_sb_append_param_class(const sema_ctx_t* ctx, const ast_expr_t* operand) {
    java_type_t t = sb_append_target_type(ctx, sema_type_of(ctx, operand));
    return (t.tag == JT_CLASS) ? t.class_id : -1;
}

int32_t sema_slot(const sema_ctx_t* ctx, const void* decl) {
    void* v = bbq_htree_search(ctx->slot_allocs, ptr_key(decl));
    return v ? (int32_t)((int)(uintptr_t)v - 1) : -1;
}

java_type_t sema_var_type(const sema_ctx_t* ctx, const void* decl) {
    java_type_t* tp = (java_type_t*)bbq_htree_search(ctx->local_types, ptr_key(decl));
    return tp ? *tp : jt_error();
}

int32_t sema_param_slot(const sema_method_t* m, int idx) {
    int32_t base = (m->modifiers & ACC_STATIC) ? 0 : 1;
    return base + idx;
}

int32_t sema_max_user_slots(const sema_method_t* m) {
    return m->max_user_slots;
}

const sema_ident_info_t* sema_ident_kind(const sema_ctx_t* ctx,
                                          const ast_expr_t* expr) {
    return (const sema_ident_info_t*)
        bbq_htree_search(ctx->ident_kinds, ptr_key(expr));
}

const sema_method_t* sema_resolved_constructor(const sema_ctx_t* ctx,
                                                const ast_expr_t* expr) {
    return decode_method_loc(ctx, bbq_htree_search(ctx->resolved_ctors, ptr_key(expr)));
}

const sema_method_t* sema_resolved_super_method(const sema_ctx_t* ctx,
                                                 const ast_expr_t* supercall) {
    return decode_method_loc(ctx, bbq_htree_search(ctx->resolved_methods, ptr_key(supercall)));
}

const sema_field_t* sema_resolved_super_field(const sema_ctx_t* ctx,
                                               const ast_expr_t* superaccess) {
    return decode_field_loc(ctx, bbq_htree_search(ctx->resolved_fields, ptr_key(superaccess)));
}

int32_t sema_invoke_kind(const sema_ctx_t* ctx, const ast_expr_t* call) {
    void* v = bbq_htree_search(ctx->invoke_kinds, ptr_key(call));
    return v ? (int32_t)((int)(uintptr_t)v - 1) : -1;
}

/* §15 implicit-exception codegen: the well-known exception class ids the backend
 * guards synthesize `new <exc>()` for, and the no-arg ctor's per-class method
 * index (the InvokeSpecial cp). The compiler OWNS the java.lang interface, so
 * these are resolved from the loaded prelude — never a host concern. */
int sema_arithmetic_exc_id(const sema_ctx_t* ctx)    { return ctx->wk.arithmetic_id; }
int sema_null_pointer_exc_id(const sema_ctx_t* ctx)  { return ctx->wk.null_pointer_id; }
int sema_array_index_exc_id(const sema_ctx_t* ctx)   { return ctx->wk.array_index_oob_id; }
int sema_neg_array_size_exc_id(const sema_ctx_t* ctx){ return ctx->wk.negative_array_size_id; }
int sema_class_cast_exc_id(const sema_ctx_t* ctx)    { return ctx->wk.class_cast_id; }
int sema_index_oob_exc_id(const sema_ctx_t* ctx)     { return ctx->wk.index_oob_id; }
int sema_array_store_exc_id(const sema_ctx_t* ctx)   { return ctx->wk.array_store_id; }

/* §20.18.16 System.arraycopy → a `SIR_ARRAYCOPY` (WASM array.copy) intrinsic. Fires for the
 * resolved System.arraycopy(5 args) whose SRC argument has a CONCRETE array static type — the
 * element width + backing come from that type (the whole stdlib passes concrete char[]/Object[]/
 * int[]; only the parameter is erased to Object). A null/Object/erased src falls through to the
 * normal call path (the runtime-dispatch case is deferred). */
bool sema_is_arraycopy(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_METHODCALL || node->method_call.args_count != 5) return false;
    if (ctx->wk.system_id < 0 || ctx->wk.arraycopy_method_id < 0) return false;
    if (strcmp(node->method_call.method, "arraycopy") != 0) return false;
    if (sema_method_decl_class(ctx, node) != ctx->wk.system_id) return false;
    /* The fast array.copy intrinsic fires ONLY when both args are concrete arrays of the SAME
     * primitive element type — then no §10.10 store check is possible and array.copy is width-exact.
     * Everything else (reference arrays, erased-Object args, mismatched primitive widths like
     * int[]→long[]) goes through the compiled System.arraycopy body: runtime kind dispatch, per-
     * element aastore (the §10.10 ArrayStoreException check), or an ArrayStoreException on mismatch. */
    java_type_t st = sema_type_of(ctx, node->method_call.args[0]);
    java_type_t dt = sema_type_of(ctx, node->method_call.args[2]);
    if (st.tag != JT_ARRAY || dt.tag != JT_ARRAY || !st.element || !dt.element) return false;
    if (st.element->tag == JT_CLASS || st.element->tag == JT_ARRAY || st.element->tag == JT_NULL) return false;
    return jt_eq(*st.element, *dt.element);   /* same primitive width */
}

/* §20.9/§20.10 raw IEEE-754 bit accessors → a bit-preserving Move* node. Fires for a resolved
 * 1-arg call to java.lang.Float.{floatToRawIntBits,intBitsToFloat} / Double.{doubleToRawLongBits,
 * longBitsToDouble}. The public floatToIntBits/doubleToLongBits are compiled Java on top of these
 * (they add NaN canonicalisation), so only the RAW accessors are the intrinsic. */
int sema_move_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_METHODCALL) return 0;
    int dc = sema_method_decl_class(ctx, node);       /* resolved (owner, index) — the stamped identity */
    if (dc < 0) return 0;
    int idx = sema_method_index(ctx, node);
    if (idx < 0 || idx >= (int)bbq_vec_len(ctx->classes[dc].methods)) return 0;
    return ctx->classes[dc].methods[idx].move_kind;   /* READ the kind stamped in resolve_wellknown_methods */
}

/* Predicate form (the where-guard): is this call a Move* bit-accessor intrinsic? */
bool sema_is_move_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node) {
    return sema_move_intrinsic_kind(ctx, node) != 0;
}

/* Does (class_id, method_idx) name a well-known method whose reference result is never null?
 * Stated in resolve_wellknown_methods, because the RTL is not recompiled with every plugin and
 * a body-derived summary is therefore unavailable for java.lang. */
bool sema_method_ret_nonnull(const sema_ctx_t* ctx, int class_id, int method_idx) {
    const sema_class_t* c = sema_get_class(ctx, class_id);
    if (!c || method_idx < 0 || method_idx >= (int)bbq_vec_len(c->methods)) return false;
    return c->methods[method_idx].ret_nonnull;
}

/* §20.11 Math.sqrt/floor/ceil/rint → an inline f64 opcode. Reads the kind stamped in
 * resolve_wellknown_methods off the resolved (owner, index) identity — the same model as move_kind. */
int sema_math_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_METHODCALL) return 0;
    int dc = sema_method_decl_class(ctx, node);
    if (dc < 0) return 0;
    int idx = sema_method_index(ctx, node);
    if (idx < 0 || idx >= (int)bbq_vec_len(ctx->classes[dc].methods)) return 0;
    return ctx->classes[dc].methods[idx].math_kind;
}
bool sema_is_math_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node) {
    return sema_math_intrinsic_kind(ctx, node) != 0;
}

/* javelina.simd accessors — the resolved call's generated-table row, and the
 * validated-immediates stash check_simd_imms recorded. */
static const simd_intrinsic_t* simd_row_of(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_METHODCALL) return NULL;
    int dc = sema_method_decl_class(ctx, node);
    if (dc < 0) return NULL;
    int idx = sema_method_index(ctx, node);
    if (idx < 0 || idx >= (int)bbq_vec_len(ctx->classes[dc].methods)) return NULL;
    int sid = ctx->classes[dc].methods[idx].simd_id;
    return sid ? &simd_intrinsics[sid - 1] : NULL;
}
bool sema_is_simd_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node) {
    return simd_row_of(ctx, node) != NULL;
}
int sema_simd_family(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const simd_intrinsic_t* r = simd_row_of(ctx, node);
    return r ? r->family : 0;
}
int sema_simd_op(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const simd_intrinsic_t* r = simd_row_of(ctx, node);
    return r ? r->wop : 0;
}
int sema_simd_align(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const simd_intrinsic_t* r = simd_row_of(ctx, node);
    return r ? r->align : 0;
}
int sema_simd_awidth(const sema_ctx_t* ctx, const ast_expr_t* node) {
    /* The access width in bytes: the spec's natural alignment IS log2(width)
     * for every memarg/memlane row — the Mem bounds-guard span. */
    const simd_intrinsic_t* r = simd_row_of(ctx, node);
    return r ? (1 << r->align) : 0;
}
static const sema_simd_imm_t* simd_imm_of(const sema_ctx_t* ctx, const ast_expr_t* node) {
    return (const sema_simd_imm_t*)bbq_htree_search(ctx->simd_imms, ptr_key(node));
}
int32_t sema_simd_lane(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const sema_simd_imm_t* im = simd_imm_of(ctx, node);
    return im ? im->lane : 0;
}
int64_t sema_simd_lo(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const sema_simd_imm_t* im = simd_imm_of(ctx, node);
    return im ? im->lo : 0;
}
int64_t sema_simd_hi(const sema_ctx_t* ctx, const ast_expr_t* node) {
    const sema_simd_imm_t* im = simd_imm_of(ctx, node);
    return im ? im->hi : 0;
}

int sema_class_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node) {
    if (!node || node->tag != AST_METHODCALL) return 0;
    int dc = sema_method_decl_class(ctx, node);
    if (dc < 0) return 0;
    int idx = sema_method_index(ctx, node);
    if (idx < 0 || idx >= (int)bbq_vec_len(ctx->classes[dc].methods)) return 0;
    return ctx->classes[dc].methods[idx].class_kind;
}
bool sema_is_class_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node) {
    return sema_class_intrinsic_kind(ctx, node) != 0;
}

int sema_class_reflect_id(const sema_ctx_t* ctx)     { return ctx->wk.class_reflect_id; }
int sema_refarray_id(const sema_ctx_t* ctx)          { return ctx->wk.refarray_id; }
int sema_primarray_id(const sema_ctx_t* ctx, int storage_index) {
    return (storage_index >= 0 && storage_index < 8) ? ctx->wk.primarray_ids[storage_index] : -1;
}
int sema_arraystore_check_method(const sema_ctx_t* ctx) { return ctx->wk.arraystore_check_method_id; }
int sema_is_instance_method(const sema_ctx_t* ctx) { return ctx->wk.is_instance_method_id; }
int sema_getclass_method_id(const sema_ctx_t* ctx)   { return ctx->wk.getclass_method_id; }
int sema_object_id(const sema_ctx_t* ctx)            { return ctx->wk.object_id; }

int sema_noarg_ctor_index(const sema_ctx_t* ctx, int class_id) {
    if (class_id < 0) return -1;
    bool has_explicit;
    const sema_method_t* m = find_constructor(ctx, class_id, 0, NULL, &has_explicit);
    if (!m) return -1;
    return m->index;                     /* stamped class-local position */
}

/* The `(String)` constructor of `class_id` — how a compiler-synthesized exception carries the
 * reason. §11.4's worked program prints `ArithmeticException` WITH the message "/ by zero", so
 * a guard that used the no-arg constructor could not produce the specified output. */
int sema_string_arg_ctor_index(const sema_ctx_t* ctx, int class_id) {
    if (class_id < 0 || ctx->wk.string_id < 0) return -1;
    java_type_t arg = jt_class(ctx->wk.string_id);
    bool has_explicit;
    const sema_method_t* m = find_constructor(ctx, class_id, 1, &arg, &has_explicit);
    /* Only a genuine (String) match: find_constructor falls back to any 1-arg constructor,
     * and handing a char[] to, say, `Foo(int)` would be a silent miscompile. */
    if (!m || m->param_count != 1 || !jt_eq(m->param_types[0], arg)) return -1;
    return m->index;
}

/* §20.12.9 `String(char[])` — how a pooled run of code units becomes a String. */
int sema_string_chars_ctor_index(const sema_ctx_t* ctx) {
    if (ctx->wk.string_id < 0) return -1;
    java_type_t elem = jt_prim(JT_CHAR);
    java_type_t arg = jt_array(&elem);
    bool has_explicit;
    const sema_method_t* m = find_constructor(ctx, ctx->wk.string_id, 1, &arg, &has_explicit);
    if (!m || m->param_count != 1 || m->param_types[0].tag != JT_ARRAY) return -1;
    return m->index;
}

int sema_string_class_id(const sema_ctx_t* ctx) { return ctx->wk.string_id; }

int32_t sema_target_class(const sema_ctx_t* ctx, const ast_expr_t* call) {
    void* v = bbq_htree_search(ctx->target_classes, ptr_key(call));
    return v ? (int32_t)((int)(uintptr_t)v - 1) : -1;
}

bool sema_may_have_effects(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    return bbq_htree_search(ctx->side_effects, ptr_key(expr)) != NULL;
}

int32_t sema_array_init_elem_type(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    void* v = bbq_htree_search(ctx->array_init_elem_types, ptr_key(expr));
    return v ? (int32_t)(uintptr_t)v : -1;
}

java_type_t sema_instanceof_type(const sema_ctx_t* ctx, const ast_expr_t* expr) {
    void* v = bbq_htree_search(ctx->instanceof_types, ptr_key(expr));
    return v ? *(java_type_t*)v : jt_error();
}


bool sema_uses_exceptions(const sema_ctx_t* ctx) {
    return ctx->uses_exceptions;
}

/* §6.14.2 Table 6-17 — class access flags: PUBLIC=0x01, FINAL=0x10,
 * INTERFACE=0x40, ABSTRACT=0x80. The low five bits (PUBLIC/PRIVATE/
 * PROTECTED/STATIC/FINAL) coincide with sema's internal bitset.
 * Sema tracks INTERFACE via a dedicated `is_interface` bool; ABSTRACT
 * lives at SEMA_ACC_ABSTRACT (0x20) internally. */
uint8_t sema_class_access_flags(const sema_class_t* c) {
    uint8_t acc = (uint8_t)(c->modifiers & 0x1F);
    if (c->is_interface)                acc |= 0x40;
    if (c->modifiers & SEMA_ACC_ABSTRACT) acc |= 0x80;
    return acc;
}

/* §6.14.4 Table 6-20 — method access flags. ACC_INIT (0x80) is set
 * by callers from `is_constructor`, not from modifiers. */
uint8_t sema_method_access_flags(const sema_method_t* m) {
    uint8_t acc = (uint8_t)(m->modifiers & 0x1F);
    if (m->modifiers & SEMA_ACC_ABSTRACT) acc |= 0x40;
    return acc;
}

/* §6.14.3 Table 6-18 — field access flags use only the low five bits. */
uint8_t sema_field_access_flags(const sema_field_t* f) {
    return (uint8_t)(f->modifiers & 0x1F);
}

/* §6.15.2 Table 6-21 — debug-component class access flags use the
 * full JLS bit layout (u16), distinct from the §6.14 u8 encoding. */
uint16_t sema_class_debug_access_flags(const sema_class_t* c) {
    uint16_t f = (uint16_t)(c->modifiers & 0x001F);
    if (c->modifiers & SEMA_ACC_ABSTRACT) f |= 0x0400;
    if (c->is_interface)                  f |= 0x0200;
    return f;
}

static int collect_interfaces_rec(const sema_ctx_t* ctx, const sema_class_t* c,
                                    const sema_class_t** out, int* n, int max_out) {
    /* DFS into super chain first (supers' interfaces come first). */
    if (c->super_id >= 0 && c->super_id < bbq_vec_len(ctx->classes)) {
        collect_interfaces_rec(ctx, &ctx->classes[c->super_id], out, n, max_out);
    }
    for (int i = 0; i < c->interface_count; i++) {
        int iid = c->interface_ids[i];
        if (iid < 0 || iid >= bbq_vec_len(ctx->classes)) continue;
        const sema_class_t* iface = &ctx->classes[iid];
        /* Recurse to add iface's super-interfaces before iface itself. */
        collect_interfaces_rec(ctx, iface, out, n, max_out);
        bool dup = false;
        for (int k = 0; k < *n; k++) if (out[k] == iface) { dup = true; break; }
        if (!dup && *n < max_out) out[(*n)++] = iface;
    }
    return *n;
}

int sema_transitive_interfaces(const sema_ctx_t* ctx, const sema_class_t* c,
                                 const sema_class_t** out, int max_out) {
    int n = 0;
    collect_interfaces_rec(ctx, c, out, &n, max_out);
    return n;
}

/* §6.9.2.5 implementer lookup. Walks the receiver class's super chain
 * (subclass first, then super) so a subclass override wins over an
 * inherited implementation. Full signature equality is required:
 * name + return type + parameter count + each parameter type. */
static bool method_signatures_match(const sema_method_t* a, const sema_method_t* b) {
    if (strcmp(a->name, b->name) != 0) return false;
    if (a->param_count != b->param_count) return false;
    if (!jt_eq(a->return_type, b->return_type)) return false;
    for (int i = 0; i < a->param_count; i++)
        if (!jt_eq(a->param_types[i], b->param_types[i])) return false;
    return true;
}

const sema_method_t* sema_implementing_method(const sema_ctx_t* ctx,
                                                const sema_class_t* cls,
                                                const sema_method_t* iface_method) {
    const sema_class_t* c = cls;
    while (c) {
        int nm = bbq_vec_len(c->methods);
        for (int i = 0; i < nm; i++) {
            const sema_method_t* m = &c->methods[i];
            if (m->is_constructor) continue;
            if (m->modifiers & ACC_STATIC) continue;
            if (method_signatures_match(m, iface_method)) return m;
        }
        if (c->super_id < 0 || c->super_id >= bbq_vec_len(ctx->classes)) break;
        c = &ctx->classes[c->super_id];
    }
    return NULL;
}

int sema_break_target_depth(const sema_ctx_t* ctx, const ast_stmt_t* stmt) {
    void* v = bbq_htree_search(ctx->break_target_depths,
                                (uint32_t)(uintptr_t)stmt);
    return v ? (int)(intptr_t)v - 1 : -1;
}

int sema_continue_target_depth(const sema_ctx_t* ctx, const ast_stmt_t* stmt) {
    void* v = bbq_htree_search(ctx->continue_target_depths,
                                (uint32_t)(uintptr_t)stmt);
    return v ? (int)(intptr_t)v - 1 : -1;
}

bool sema_loop_has_continue(const sema_ctx_t* ctx, const ast_stmt_t* stmt) {
    return bbq_htree_search(ctx->loops_with_continue,
                             (uint32_t)(uintptr_t)stmt) != NULL;
}


int sema_diag_format(const sema_diag_t* d, char* buf, int bufsize) {
    const char* level = (d->level == DIAG_ERROR) ? "error" : "warning";
    if (d->loc.line > 0) {
        if (d->loc.file)
            return snprintf(buf, (size_t)bufsize, "%s:%d:%d: %s: %s",
                            d->loc.file, d->loc.line, d->loc.col, level, d->message);
        return snprintf(buf, (size_t)bufsize, "%d:%d: %s: %s",
                        d->loc.line, d->loc.col, level, d->message);
    }
    return snprintf(buf, (size_t)bufsize, "%s: %s", level, d->message);
}

void sema_destroy(sema_ctx_t* ctx) {
    /* Destroy scope stack */
    for (int i = 0; i < bbq_vec_len(ctx->scopes); i++)
        bbq_htree_destroy(ctx->scopes[i]);
    bbq_vec_free(ctx->scopes);

    bbq_htree_destroy(ctx->class_by_name);
    bbq_htree_destroy(ctx->expr_types);
    bbq_htree_destroy(ctx->resolved_methods);
    bbq_htree_destroy(ctx->simd_imms);
    bbq_vec_free(ctx->import_funcs);
    bbq_vec_free(ctx->refs);
    bbq_vec_free(ctx->vtargets);
    bbq_vec_free(ctx->vtarget_base);
    bbq_htree_destroy(ctx->resolved_fields);
    bbq_htree_destroy(ctx->data_types);
    bbq_htree_destroy(ctx->slot_allocs);
    bbq_htree_destroy(ctx->local_types);
    bbq_htree_destroy(ctx->ident_kinds);
    bbq_htree_destroy(ctx->resolved_ctors);
    bbq_htree_destroy(ctx->invoke_kinds);
    bbq_htree_destroy(ctx->target_classes);
    bbq_htree_destroy(ctx->side_effects);
    bbq_htree_destroy(ctx->array_init_elem_types);
    bbq_htree_destroy(ctx->instanceof_types);
    bbq_htree_destroy(ctx->switch_infos);
    bbq_htree_destroy(ctx->break_target_depths);
    bbq_htree_destroy(ctx->continue_target_depths);
    bbq_htree_destroy(ctx->loops_with_continue);
    bbq_htree_destroy(ctx->type_class_ids);
    for (int i = 0; i < (int)bbq_vec_len(ctx->units); i++) {
        bbq_vec_free(ctx->units[i].singles);
        bbq_vec_free(ctx->units[i].ondemands);
    }
    bbq_vec_free(ctx->units);
    bbq_vec_free(ctx->labels);
    bbq_vec_free(ctx->frames);

    /* Free bbq_vecs for class fields/methods */
    for (int i = 0; i < bbq_vec_len(ctx->classes); i++) {
        bbq_vec_free(ctx->classes[i].fields);
        bbq_vec_free(ctx->classes[i].methods);
    }
    bbq_vec_free(ctx->classes);
    bbq_vec_free(ctx->diags);
    bbq_vec_free(ctx->functions);
    bbq_hmap_free(&ctx->functions_idx);
    bbq_vec_free(ctx->caught_types);
    bbq_vec_free(ctx->array_class_types);
    bbq_vec_free(ctx->array_class_ids);
}
