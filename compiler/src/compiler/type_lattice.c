#include "javelina/compiler/type_lattice.h"
#include "bbq_vec.h"

#include <stdlib.h>
#include <string.h>

static bool types_equal(const Type* a, const Type* b) {
    if (a->kind != b->kind) return false;
    switch (a->kind) {
    case TK_TOP:
    case TK_BOTTOM:
    case TK_NULL:
        return true;
    case TK_PRIM:
        return a->prim.width == b->prim.width;
    case TK_REF:
        return a->ref.class_id == b->ref.class_id;
    case TK_ARRAY:
        return a->array.dim == b->array.dim
            && a->array.class_id == b->array.class_id;
    case TK_PRIM_ARRAY:
        return a->prim_array.width == b->prim_array.width
            && a->prim_array.dim == b->prim_array.dim;
    }
    return false;
}

static Type* intern_find(const type_pool_t* pool, const Type* candidate) {
    int n = bbq_vec_len(pool->interned);
    for (int i = 0; i < n; i++)
        if (types_equal(pool->interned[i], candidate))
            return pool->interned[i];
    return NULL;
}

static Type* intern_add(type_pool_t* pool, type_kind_t kind) {
    Type* t = (Type*)bbq_arena_alloc(pool->arena, sizeof(Type));
    memset(t, 0, sizeof(*t));
    t->kind = kind;
    bbq_vec_push(pool->interned, t);
    return t;
}

void type_pool_init(type_pool_t* pool, bbq_arena* arena) {
    pool->arena = arena;
    pool->interned = NULL;
    pool->top    = intern_add(pool, TK_TOP);
    pool->bottom = intern_add(pool, TK_BOTTOM);
    pool->null_  = intern_add(pool, TK_NULL);
}

void type_pool_destroy(type_pool_t* pool) {
    bbq_vec_free(pool->interned);
    pool->interned = NULL;
    pool->top = NULL;
    pool->bottom = NULL;
    pool->null_ = NULL;
}

const Type* type_top(const type_pool_t* pool)    { return pool->top; }
const Type* type_bottom(const type_pool_t* pool) { return pool->bottom; }
const Type* type_null(const type_pool_t* pool)   { return pool->null_; }

const Type* type_make_prim(type_pool_t* pool, sir_datatype_t width) {
    Type probe = { .kind = TK_PRIM, .prim = { .width = width } };
    Type* hit = intern_find(pool, &probe);
    if (hit) return hit;
    Type* t = intern_add(pool, TK_PRIM);
    t->prim.width = width;
    return t;
}

const Type* type_make_ref(type_pool_t* pool, int32_t class_id) {
    Type probe = { .kind = TK_REF, .ref = { .class_id = class_id } };
    Type* hit = intern_find(pool, &probe);
    if (hit) return hit;
    Type* t = intern_add(pool, TK_REF);
    t->ref.class_id = class_id;
    return t;
}

const Type* type_make_array(type_pool_t* pool, int32_t dim, int32_t class_id) {
    Type probe = { .kind = TK_ARRAY, .array = { .dim = dim, .class_id = class_id } };
    Type* hit = intern_find(pool, &probe);
    if (hit) return hit;
    Type* t = intern_add(pool, TK_ARRAY);
    t->array.dim = dim;
    t->array.class_id = class_id;
    return t;
}

const Type* type_make_prim_array(type_pool_t* pool, int32_t dim,
                                  sir_datatype_t width) {
    Type probe = { .kind = TK_PRIM_ARRAY,
                   .prim_array = { .dim = dim, .width = width } };
    Type* hit = intern_find(pool, &probe);
    if (hit) return hit;
    Type* t = intern_add(pool, TK_PRIM_ARRAY);
    t->prim_array.dim = dim;
    t->prim_array.width = width;
    return t;
}

/* Is this kind a REFERENCE type — something every value of which is an Object (§10.7)? */
static bool tk_is_ref(type_kind_t k) {
    return k == TK_REF || k == TK_ARRAY || k == TK_PRIM_ARRAY || k == TK_NULL;
}

/* Is `high` the type `Object`? Every reference type is a subtype of it (§4.10.2) — which
 * is what makes the reference kinds JOIN instead of collapsing to BOTTOM. */
static bool tk_is_object(const sema_ctx_t* sema, const Type* high) {
    return sema && high->kind == TK_REF && high->ref.class_id == sema->wk.object_id;
}

const Type* type_meet(const sema_ctx_t* sema, const Type* a, const Type* b,
                       type_pool_t* pool) {
    if (a->kind == TK_TOP)    return b;
    if (b->kind == TK_TOP)    return a;
    if (a->kind == TK_BOTTOM) return pool->bottom;
    if (b->kind == TK_BOTTOM) return pool->bottom;
    if (a == b) return a;
    /* NULL is the minimum of the reference hierarchy — it meets to the
     * other operand when that operand is any reference kind (REF /
     * ARRAY / PRIM_ARRAY), and meets to BOTTOM against PRIM (cross-
     * kind). NULL ∩ NULL handled by `a == b` above. */
    if (a->kind == TK_NULL) return (b->kind == TK_REF
                                 || b->kind == TK_ARRAY
                                 || b->kind == TK_PRIM_ARRAY)
                                 ? b : pool->bottom;
    if (b->kind == TK_NULL) return (a->kind == TK_REF
                                 || a->kind == TK_ARRAY
                                 || a->kind == TK_PRIM_ARRAY)
                                 ? a : pool->bottom;

    /* MEET AGREES WITH ⊑, which is the semilattice contract: when one side already sits
     * above the other, IT is the join. Without this, `A ⊔ I` (A implements I) walked the
     * extends chains and answered Object — losing the interface — and `A[] ⊔ Object`
     * answered BOTTOM, losing the type of a value that is plainly an Object (§10.7). */
    if (type_leq(sema, a, b)) return b;
    if (type_leq(sema, b, a)) return a;

    /* Two REFERENCE types always have an upper bound: Object (§4.10.2). Only the absence
     * of a hierarchy (sema == NULL — the pure-algebra case) leaves them incomparable. This
     * covers the cross-kind reference joins too: an array and a class, or a reference array
     * and a primitive array, are both Objects. (Java's true LUB can be an intersection type
     * — `A & I` — which this lattice has no name for; Object is the sound single upper
     * bound, and the exact class of an OBJECT is what τ̂ carries anyway.) */
    if (sema && tk_is_ref(a->kind) && tk_is_ref(b->kind)) {
        if (a->kind == TK_REF && b->kind == TK_REF) {
            int lcs = sema_common_superclass(sema, a->ref.class_id, b->ref.class_id);
            if (lcs >= 0) return type_make_ref(pool, lcs);
        }
        if (a->kind == TK_ARRAY && b->kind == TK_ARRAY
                && a->array.dim == b->array.dim) {
            /* §10.2: covariant elements — the join of the element classes. */
            int lcs = sema_common_superclass(sema, a->array.class_id, b->array.class_id);
            if (lcs >= 0) return type_make_array(pool, a->array.dim, lcs);
        }
        if (sema->wk.object_id >= 0) return type_make_ref(pool, sema->wk.object_id);
    }

    if (a->kind != b->kind) return pool->bottom;
    switch (a->kind) {
    case TK_PRIM:
        /* Primitive widths are siblings, not subtypes; explicit
         * S2I/I2S conversions are required between them.  Two PrimLats
         * meet only if they have the same width. */
        return (a->prim.width == b->prim.width) ? a : pool->bottom;
    case TK_REF: {
        int ac = a->ref.class_id, bc = b->ref.class_id;
        if (ac == bc) return a;
        if (!sema) return pool->bottom;
        int lcs = sema_common_superclass(sema, ac, bc);
        if (lcs < 0) return pool->bottom;
        return type_make_ref(pool, lcs);
    }
    case TK_ARRAY: {
        if (a->array.dim != b->array.dim) return pool->bottom;
        int ac = a->array.class_id, bc = b->array.class_id;
        if (ac == bc) return a;
        if (!sema) return pool->bottom;
        int lcs = sema_common_superclass(sema, ac, bc);
        if (lcs < 0) return pool->bottom;
        return type_make_array(pool, a->array.dim, lcs);
    }
    case TK_PRIM_ARRAY:
        /* Sibling discrete element widths — like PRIM, distinct widths
         * (or dims) are incomparable. */
        return (a->prim_array.width == b->prim_array.width
             && a->prim_array.dim == b->prim_array.dim) ? a : pool->bottom;
    case TK_TOP:
    case TK_BOTTOM:
    case TK_NULL:
        /* Unreachable — handled above. */
        return pool->bottom;
    }
    return pool->bottom;
}

/* The lattice's ⊑ IS JLS §4.10.2 reference subtyping. It asks sema_ref_is_subtype — the ONE
 * predicate — and never the extends chain, which answers "not a subtype" for every
 * interface (an interface is in nobody's `extends`), and which is how a consumer that DROPS
 * on "no" would delete every object implementing the interface it was asked about. */
bool type_leq(const sema_ctx_t* sema, const Type* low, const Type* high) {
    if (low->kind  == TK_TOP)    return true;
    if (high->kind == TK_BOTTOM) return true;
    if (low->kind  == TK_BOTTOM) return high->kind == TK_BOTTOM;
    if (high->kind == TK_TOP)    return low->kind  == TK_TOP;
    if (low == high) return true;
    /* NULL ⊑ every reference kind (and ⊑ NULL itself, caught above); incomparable
     * with PRIM. */
    if (low->kind == TK_NULL)  return tk_is_ref(high->kind) && high->kind != TK_NULL;
    if (high->kind == TK_NULL) return false;

    /* §10.7: EVERY array is an Object (and a Cloneable). This crosses the kinds — an
     * array is TK_ARRAY / TK_PRIM_ARRAY and Object is TK_REF — so it must be answered
     * before the same-kind switch, which would otherwise call them incomparable and throw
     * the fact away. */
    if (tk_is_ref(low->kind) && tk_is_object(sema, high)) return true;
    if (tk_is_ref(low->kind) && sema && high->kind == TK_REF
            && sema->wk.cloneable_id >= 0
            && high->ref.class_id == sema->wk.cloneable_id
            && (low->kind == TK_ARRAY || low->kind == TK_PRIM_ARRAY)) return true;

    if (low->kind != high->kind) return false;
    switch (low->kind) {
    case TK_PRIM:
        /* Sibling discrete widths — equal-or-incomparable. */
        return low->prim.width == high->prim.width;
    case TK_REF:
        return sema_ref_is_subtype(sema, low->ref.class_id, high->ref.class_id);
    case TK_ARRAY: {
        /* §10.2 array covariance: `A[] ⊑ B[]` iff `A ⊑ B`. Same rank required — there is
         * no name for an array-of-arbitrary-rank that subsumes both. */
        if (low->array.dim != high->array.dim) return false;
        return sema_ref_is_subtype(sema, low->array.class_id, high->array.class_id);
    }
    case TK_PRIM_ARRAY:
        /* A primitive element type has no subtypes, so these are equal-or-incomparable. */
        return low->prim_array.width == high->prim_array.width
            && low->prim_array.dim == high->prim_array.dim;
    case TK_TOP:
    case TK_BOTTOM:
    case TK_NULL:
        return false;
    }
    return false;
}

/* ── Reference-type representation authority — see type_lattice.h ── */

int32_t lat_root_class(const sema_ctx_t* sema) {
    int n = (int)bbq_vec_len(sema->classes);
    for (int ci = 0; ci < n; ci++) {
        const sema_class_t* c = sema_get_class(sema, ci);
        if (c->super_id < 0 && !c->is_interface) return ci;   /* java.lang.Object */
    }
    return 0;
}

int32_t lat_value_class(const sema_ctx_t* sema, int32_t class_id) {
    if (class_id >= 0 && class_id < (int)bbq_vec_len(sema->classes)
        && sema_get_class(sema, class_id)->is_interface)
        return lat_root_class(sema);   /* interface value = an object = the root */
    return class_id;
}

int32_t lat_handler_landing_class(const sema_ctx_t* sema, int32_t catch_class_id) {
    /* No declared type (the region's catch-all / finally path) ⟹ it lands as a Throwable. */
    if (catch_class_id < 0) return lat_value_class(sema, sema->wk.throwable_id);
    return lat_value_class(sema, catch_class_id);
}

bool lat_array_elem_is_ref(java_type_t elem) {
    return elem.tag == JT_CLASS || elem.tag == JT_ARRAY;   /* class/nested-array element → collapses to RefArray */
}

int32_t lat_refarray_class(const sema_ctx_t* sema) {
    return sema_refarray_id(sema);   /* synthesized in collect_decls; -1 only if the prelude is broken */
}

/* The per-width PrimArray overlay index (0=i8 1=i16-short 2=i16-char 3=i32
 * 4=i64 5=f32 6=f64) — the WASM backing width a primitive element packs into
 * (byte/bool→i8; char gets its own i16 slot, distinct from short). */
int lat_prim_storage_index(sir_datatype_t dt) {
    switch (dt) {
    case SIR_DTBYTE:   return 0;   /* i8 (byte; boolean folds here) */
    case SIR_DTSHORT:  return 1;
    case SIR_DTCHAR:   return 2;   /* distinct from short — its own (array i16) typeidx */
    case SIR_DTLONG:   return 4;
    case SIR_DTFLOAT:  return 5;
    case SIR_DTDOUBLE: return 6;
    default:           return 3;   /* i32 (int) */
    }
}

int32_t lat_primarray_class(const sema_ctx_t* sema, sir_datatype_t dt) {
    return sema_primarray_id(sema, lat_prim_storage_index(dt));
}

/* The overlay's backing-store cell: RefArray.data is field 1 (elementClass is 0),
 * a PrimArray's data is field 0. Both are written once, by the allocation that
 * creates the overlay, and are invisible to Java. */
bool lat_is_array_data_cell(const sema_ctx_t* sema, int32_t class_id, int field_idx) {
    if (class_id == sema_refarray_id(sema)) return field_idx == 1;
    for (int si = 0; si < 7; si++)
        if (class_id == sema_primarray_id(sema, si)) return field_idx == 0;
    return false;
}

/* §10.7/§10.8 the OVERLAY class an array-typed VALUE `arr` is represented by, or -1 when
 * `arr` is the concrete backing of an overlay (no further overlay). The SINGLE authority —
 * every consumer (field type, field default, valtype, singleton) maps this to a typeidx and
 * never re-decides. The concrete-backing cases, both mapping to -1:
 *   - a raw-marked array (a PrimArray overlay's own `data` field, JT_ARRAY_RAW), and
 *   - a JT_NULL element (RefArray's anyref[] backing — the top reference).
 * Otherwise: a reference element → RefArray (§10.2 covariance is free); a primitive element
 * → the per-width PrimArray (so a primitive array is an Object). */
int32_t lat_array_overlay_class(const sema_ctx_t* sema, java_type_t arr) {
    if (arr.tag == JT_ARRAY && arr.class_id == JT_ARRAY_RAW) return -1;  /* overlay's concrete backing */
    java_type_t elem = arr.element ? *arr.element : jt_null();
    if (elem.tag == JT_CLASS || elem.tag == JT_ARRAY) return lat_refarray_class(sema);
    if (elem.tag == JT_NULL) return -1;   /* the RefArray backing (anyref[]) — concrete */
    return lat_primarray_class(sema, lat_tag_to_dt(elem.tag));
}

/* ── JLS conversion authority (§5.1.2/§5.1.3/§5.6) — see type_lattice.h ── */

lat_valtype_t lat_dt_valtype(sir_datatype_t dt) {
    switch (dt) {
    case SIR_DTLONG:   return LAT_VT_I64;
    case SIR_DTFLOAT:  return LAT_VT_F32;
    case SIR_DTDOUBLE: return LAT_VT_F64;
    case SIR_DTREF:    return LAT_VT_REF;
    default:           return LAT_VT_I32;   /* byte/short/char/int */
    }
}

sir_datatype_t lat_tag_to_dt(int32_t jt_tag) {
    switch (jt_tag) {
    case JT_BYTE: case JT_BOOL: return SIR_DTBYTE;
    case JT_SHORT:              return SIR_DTSHORT;
    case JT_CHAR:               return SIR_DTCHAR;
    case JT_INT:                return SIR_DTINT;
    case JT_LONG:               return SIR_DTLONG;
    case JT_FLOAT:              return SIR_DTFLOAT;
    case JT_DOUBLE:             return SIR_DTDOUBLE;
    default:                    return SIR_DTREF;
    }
}

/* The one Java-tag → SIR array-element-type (atype) authority. Primitive element
 * arrays carry an atype; ref elements use ATCLASS/ATREFARRAY (concrete ref
 * referents go on a separate path). */
sir_atype_t lat_tag_to_atype(int32_t jt_tag) {
    switch (jt_tag) {
    case JT_BOOL:   return SIR_ATBOOL;
    case JT_BYTE:   return SIR_ATBYTE;
    case JT_SHORT:  return SIR_ATSHORT;
    case JT_CHAR:   return SIR_ATCHAR;
    case JT_INT:    return SIR_ATINT;
    case JT_LONG:   return SIR_ATLONG;
    case JT_FLOAT:  return SIR_ATFLOAT;
    case JT_DOUBLE: return SIR_ATDOUBLE;
    case JT_CLASS:  return SIR_ATCLASS;
    case JT_ARRAY:  return SIR_ATREFARRAY;
    default:        return SIR_ATINT;
    }
}

sir_datatype_t lat_atype_to_dt(sir_atype_t atype) {
    switch (atype) {
    case SIR_ATBOOL:   return SIR_DTBYTE;   /* boolean packs as byte */
    case SIR_ATBYTE:   return SIR_DTBYTE;
    case SIR_ATSHORT:  return SIR_DTSHORT;
    case SIR_ATCHAR:   return SIR_DTCHAR;
    case SIR_ATINT:    return SIR_DTINT;
    case SIR_ATLONG:   return SIR_DTLONG;
    case SIR_ATFLOAT:  return SIR_DTFLOAT;
    case SIR_ATDOUBLE: return SIR_DTDOUBLE;
    default:           return SIR_DTREF;    /* ATCLASS / ATREFARRAY */
    }
}

int lat_widen_rank(java_type_tag_t t) {
    switch (t) {
    case JT_BYTE:   return 1;
    case JT_SHORT:  return 2;
    case JT_CHAR:   return 2;   /* same rank as short, disjoint chain (see _prim) */
    case JT_INT:    return 3;
    case JT_LONG:   return 4;
    case JT_FLOAT:  return 5;
    case JT_DOUBLE: return 6;
    default:        return 0;   /* not numeric (bool, ref, void, …) */
    }
}

bool lat_is_widening_prim(java_type_t from, java_type_t to) {
    if (!jt_is_numeric(from) || !jt_is_numeric(to)) return false;
    if (from.tag == to.tag) return false;
    if (from.tag == JT_CHAR) return to.tag == JT_INT  || to.tag == JT_LONG
                                 || to.tag == JT_FLOAT || to.tag == JT_DOUBLE;
    if (to.tag == JT_CHAR) return false;  /* nothing widens TO char */
    return lat_widen_rank(from.tag) < lat_widen_rank(to.tag);
}

bool lat_is_narrowing_prim(java_type_t from, java_type_t to) {
    if (!jt_is_numeric(from) || !jt_is_numeric(to)) return false;
    if (from.tag == to.tag) return false;
    return !lat_is_widening_prim(from, to);
}

java_type_tag_t lat_promote(java_type_t a, java_type_t b) {
    if (a.tag == JT_DOUBLE || b.tag == JT_DOUBLE) return JT_DOUBLE;
    if (a.tag == JT_FLOAT  || b.tag == JT_FLOAT)  return JT_FLOAT;
    if (a.tag == JT_LONG   || b.tag == JT_LONG)   return JT_LONG;
    return JT_INT;
}

sir_datatype_t lat_promote_dt(sir_datatype_t a, sir_datatype_t b) {
    if (a == SIR_DTREF    || b == SIR_DTREF)    return SIR_DTREF;    /* refs: no promo */
    if (a == SIR_DTDOUBLE || b == SIR_DTDOUBLE) return SIR_DTDOUBLE;
    if (a == SIR_DTFLOAT  || b == SIR_DTFLOAT)  return SIR_DTFLOAT;
    if (a == SIR_DTLONG   || b == SIR_DTLONG)   return SIR_DTLONG;
    return SIR_DTINT;                                                /* byte/short/char/int → int */
}

sir_datatype_t lat_unary_promote_dt(sir_datatype_t a) {
    if (a == SIR_DTBYTE || a == SIR_DTSHORT || a == SIR_DTCHAR) return SIR_DTINT;
    return a;
}

lat_conv_t lat_num_conv(sir_datatype_t from, sir_datatype_t to) {
    if (from == to) return LAT_CONV_IDENTITY;
    switch (to) {
    case SIR_DTBYTE:
        if (from == SIR_DTLONG)   return LAT_CONV_L2B;
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2B;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2B;
        if (from == SIR_DTSHORT)  return LAT_CONV_S2B;
        return LAT_CONV_I2B;                       /* int/char → byte */
    case SIR_DTSHORT:
        if (from == SIR_DTLONG)   return LAT_CONV_L2S;
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2S;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2S;
        return LAT_CONV_I2S;                       /* int/char/byte → short */
    case SIR_DTCHAR:
        if (from == SIR_DTLONG)   return LAT_CONV_L2C;
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2C;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2C;
        return LAT_CONV_I2C;                       /* int/short/byte → char */
    case SIR_DTINT:
        if (from == SIR_DTLONG)   return LAT_CONV_L2I;
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2I;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2I;
        if (from == SIR_DTSHORT)  return LAT_CONV_S2I;
        return LAT_CONV_IDENTITY;                  /* byte/char → int: already i32 */
    case SIR_DTLONG:
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2L;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2L;
        return LAT_CONV_I2L;                       /* byte/short/char/int → long */
    case SIR_DTFLOAT:
        if (from == SIR_DTLONG)   return LAT_CONV_L2F;
        if (from == SIR_DTDOUBLE) return LAT_CONV_D2F;
        return LAT_CONV_I2F;                       /* int-family → float */
    case SIR_DTDOUBLE:
        if (from == SIR_DTLONG)   return LAT_CONV_L2D;
        if (from == SIR_DTFLOAT)  return LAT_CONV_F2D;
        return LAT_CONV_I2D;                       /* int-family → double */
    default: return LAT_CONV_NONE;                 /* to ref / non-numeric */
    }
}
