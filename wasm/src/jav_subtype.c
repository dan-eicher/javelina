/* jav_subtype.c — the §3.3 reference subtype relation. Pure logic; see the header. */
#include "jav_subtype.h"

/* The abstract heap-type lattice as a parent function: each abstract type's immediate
 * supertype, or HT_BOT-as-no-parent (a top: any/func/extern/exn). The bottom types
 * (none/nofunc/noextern/noexn) are handled separately — they sit below their WHOLE
 * hierarchy, concrete types included, which a single parent edge can't express. */
static int32_t abstract_parent(int32_t a) {
    switch (a) {
        case HT_I31:    return HT_EQ;
        case HT_STRUCT: return HT_EQ;
        case HT_ARRAY:  return HT_EQ;
        case HT_EQ:     return HT_ANY;
        case HT_NOFUNC: return HT_FUNC;     /* via the bottom rule too, but the chain is fine */
        case HT_NOEXTERN: return HT_EXTERN;
        case HT_NONE:   return HT_ANY;      /* none ≤ any; the full bottom rule is separate */
        case HT_NOEXN:  return HT_EXN;
        default:        return HT_BOT;       /* any / func / extern / exn: tops (no parent) */
    }
}

/* a ≤ b for two ABSTRACT heap types: climb a's parent chain to b. */
static int abstract_sub(int32_t a, int32_t b) {
    for (int32_t t = a; t != HT_BOT; t = abstract_parent(t))
        if (t == b) return 1;
    return 0;
}

/* Is heap type `b` in the `any` hierarchy (any/eq/i31/struct/array/none or a concrete
 * struct/array type)? Used for `none ≤ b`. */
static int in_any_hierarchy(const jav_subtype_ctx_t* cx, int32_t b) {
    if (b >= 0) {   /* concrete struct/array (not func) live under `any` */
        if (cx && (uint32_t)b < cx->ntypes)
            return cx->kinds[b] == WST_STRUCT || cx->kinds[b] == WST_ARRAY;
        return 0;
    }
    switch (b) {
        case HT_ANY: case HT_EQ: case HT_I31: case HT_STRUCT: case HT_ARRAY: case HT_NONE:
            return 1;
        default: return 0;
    }
}
/* Is `b` in the `func` hierarchy (func/nofunc or a concrete func type)? */
static int in_func_hierarchy(const jav_subtype_ctx_t* cx, int32_t b) {
    if (b >= 0) {
        if (cx && (uint32_t)b < cx->ntypes) return cx->kinds[b] == WST_FUNC;
        return 0;
    }
    return b == HT_FUNC || b == HT_NOFUNC;
}

int jav_ht_sub(const jav_subtype_ctx_t* cx, int32_t a, int32_t b) {
    if (a == b) return 1;             /* reflexivity */
    if (a == HT_BOT) return 1;        /* ⊥ ≤ everything */

    /* the bottom types: subtypes of their entire hierarchy (concrete types included) */
    if (a == HT_NONE)    return in_any_hierarchy(cx, b);
    if (a == HT_NOFUNC)  return in_func_hierarchy(cx, b);
    if (a == HT_NOEXTERN) return b == HT_EXTERN || b == HT_NOEXTERN;
    if (a == HT_NOEXN)   return b == HT_EXN || b == HT_NOEXN;

    if (a >= 0) {                     /* concrete a */
        if (b >= 0) {                 /* concrete ≤ concrete (§3.3.10): clos(t)=clos(b) [canonical equality]
                                       * for a or any declared-supertype ancestor t of a. */
            if (!cx) return a == b;
            const int32_t* canon = cx->canon;
            for (int32_t t = a; t >= 0 && (uint32_t)t < cx->ntypes; t = cx->supers[t])
                if (canon ? canon[t] == canon[b] : t == b) return 1;
            return 0;
        }
        if (!cx || (uint32_t)a >= cx->ntypes) return 0;
        switch (cx->kinds[a]) {       /* concrete ≤ abstract: by structural kind */
            case WST_STRUCT: return abstract_sub(HT_STRUCT, b);   /* struct ≤ eq ≤ any */
            case WST_ARRAY:  return abstract_sub(HT_ARRAY, b);
            case WST_FUNC:   return abstract_sub(HT_FUNC, b);
            default:         return 0;
        }
    }

    /* abstract a: an abstract type is never below a concrete one (the only such cases —
     * none/nofunc/… — were handled above), so b must be abstract too. */
    if (b >= 0) return 0;
    return abstract_sub(a, b);
}

int jav_ht_compatible(const jav_subtype_ctx_t* cx, int32_t a, int32_t b) {
    static const int32_t tops[] = { HT_ANY, HT_FUNC, HT_EXTERN, HT_EXN };
    for (unsigned i = 0; i < sizeof tops / sizeof tops[0]; i++)
        if (jav_ht_sub(cx, a, tops[i]) && jav_ht_sub(cx, b, tops[i])) return 1;
    return 0;
}

int jav_rt_sub(const jav_subtype_ctx_t* cx,
                int a_nullable, int32_t a, int b_nullable, int32_t b) {
    if (!jav_ht_sub(cx, a, b)) return 0;
    return !a_nullable || b_nullable;   /* nullable source needs a nullable target */
}
