/*
 * jav_subtype.h — the WASM 3.0 §3.3 reference subtype relation, as a standalone,
 * exhaustively-testable function (no validator, no engine). A heap type is either an
 * ABSTRACT type (a negative code, the spec's sleb value) or a CONCRETE type (a
 * non-negative typeidx). Concrete subtyping needs a context: each concrete type has
 * a structural KIND (struct/array/func) and an optional declared SUPERTYPE.
 *
 * This is the lattice the validator MUST consult for every ref match. It is pure
 * logic over a small finite domain, so test_subtype.c checks it edge-by-edge against
 * §3.3 — the one part of the verifier that needs no spec-test oracle to be sure of.
 */
#ifndef JAV_SUBTYPE_H
#define JAV_SUBTYPE_H

#include <stdint.h>

/* Abstract heap-type codes = the spec's heaptype sleb values (§5.3.4). Concrete
 * types are >= 0 (a typeidx). HT_BOT is the internal bottom for dead code. */
enum {
    HT_BOT      = -128,  /* (internal) ⊥ — subtype of everything; unreachable/dead code */
    HT_NOEXN    = -12,
    HT_NOFUNC   = -13,
    HT_NOEXTERN = -14,
    HT_NONE     = -15,
    HT_FUNC     = -16,
    HT_EXTERN   = -17,
    HT_ANY      = -18,
    HT_EQ       = -19,
    HT_I31      = -20,
    HT_STRUCT   = -21,
    HT_ARRAY    = -22,
    HT_EXN      = -23,
};

/* The concrete-type structural kind (which abstract top a concrete type sits under). */
enum { WST_STRUCT = 0, WST_ARRAY = 1, WST_FUNC = 2 };

/* Per-concrete-type info the relation needs: its kind and its declared supertype
 * (a typeidx, or -1 for none). Indexed by typeidx; ntypes bounds the index. */
typedef struct {
    const uint8_t* kinds;   /* kinds[t] ∈ {WST_STRUCT, WST_ARRAY, WST_FUNC} */
    const int32_t* supers;  /* supers[t] = declared supertype typeidx, or -1 */
    uint32_t       ntypes;
    const int32_t* canon;   /* §3.3.10 closure: canonical id per typeidx (clos(a)=clos(b) ⇔ canon[a]==canon[b]);
                             * NULL ⇒ fall back to typeidx identity (concrete equivalence = same index). */
} jav_subtype_ctx_t;

/* Does heap type `a` match (is a subtype of) heap type `b` under context `cx`?
 * Implements §3.3 "Matching": reflexivity, ⊥, the abstract lattice (eq≤any, i31/
 * struct/array≤eq, nofunc≤func, noextern≤extern, noexn≤exn), concrete≤abstract by
 * kind, concrete≤concrete via the declared supertype chain, and the bottom types
 * (none/nofunc/noextern/noexn) below their whole hierarchy. `cx` may be NULL when no
 * concrete types are involved. */
int jav_ht_sub(const jav_subtype_ctx_t* cx, int32_t a, int32_t b);

/* Are heap types `a` and `b` in the same top hierarchy (any / func / extern / exn)?
 * This is the validity condition for ref.test/ref.cast (§3.4.6): a cast is well-typed
 * iff a common supertype rt' exists, i.e. operand and target share a hierarchy. */
int jav_ht_compatible(const jav_subtype_ctx_t* cx, int32_t a, int32_t b);

/* Reference subtyping: (ref null_a? a) ≤ (ref null_b? b) iff a ≤ b and a non-null
 * source may satisfy a nullable target but not vice-versa (null_a ⇒ null_b). */
int jav_rt_sub(const jav_subtype_ctx_t* cx,
                int a_nullable, int32_t a, int b_nullable, int32_t b);

#endif /* JAV_SUBTYPE_H */
