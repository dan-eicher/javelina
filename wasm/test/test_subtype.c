// test_subtype.c — independent conformance test for the §3.3 reference subtype
// relation (jav_subtype.c), edge by edge against the spec. No validator, no engine:
// the relation is pure logic over a finite domain, so it can be PROVEN here. This is
// the bedrock the verifier rests on — modeled on yoctojc's test_lattice.c.
#include "jav_subtype.h"
#include <stdio.h>

static int fails = 0;
/* assert a <: b holds (want=1) or not (want=0) under context cx. */
static void S(const jav_subtype_ctx_t* cx, int32_t a, int32_t b, int want, const char* msg){
    int got = jav_ht_sub(cx, a, b);
    if (got != want) { printf("  FAIL  %s : ht_sub(%d,%d)=%d want %d\n", msg, a, b, got, want); fails++; }
}
static void R(const jav_subtype_ctx_t* cx, int an,int32_t a, int bn,int32_t b, int want, const char* msg){
    int got = jav_rt_sub(cx, an, a, bn, b);
    if (got != want) { printf("  FAIL  %s : rt_sub(%d %d, %d %d)=%d want %d\n", msg, an,a, bn,b, got, want); fails++; }
}

int main(void){
    /* ── the abstract `any` hierarchy: any > eq > {i31, struct, array}; none ⊥ ── */
    S(0, HT_EQ, HT_ANY, 1, "eq <: any");
    S(0, HT_I31, HT_EQ, 1, "i31 <: eq");
    S(0, HT_I31, HT_ANY, 1, "i31 <: any (transitive)");
    S(0, HT_STRUCT, HT_EQ, 1, "struct <: eq");
    S(0, HT_ARRAY, HT_EQ, 1, "array <: eq");
    S(0, HT_STRUCT, HT_ANY, 1, "struct <: any");
    S(0, HT_ANY, HT_EQ, 0, "any </: eq (not upward)");
    S(0, HT_STRUCT, HT_ARRAY, 0, "struct </: array (siblings)");
    S(0, HT_I31, HT_STRUCT, 0, "i31 </: struct (siblings)");
    S(0, HT_ANY, HT_ANY, 1, "any <: any (reflexive)");

    /* ── the func / extern / exn hierarchies are disjoint from any ── */
    S(0, HT_NOFUNC, HT_FUNC, 1, "nofunc <: func");
    S(0, HT_NOEXTERN, HT_EXTERN, 1, "noextern <: extern");
    S(0, HT_NOEXN, HT_EXN, 1, "noexn <: exn");
    S(0, HT_FUNC, HT_ANY, 0, "func </: any (disjoint)");
    S(0, HT_EXTERN, HT_ANY, 0, "extern </: any (disjoint)");
    S(0, HT_ANY, HT_FUNC, 0, "any </: func (disjoint)");

    /* ── the bottom types sit below their WHOLE hierarchy ── */
    S(0, HT_NONE, HT_ANY, 1, "none <: any");
    S(0, HT_NONE, HT_EQ, 1, "none <: eq");
    S(0, HT_NONE, HT_I31, 1, "none <: i31");
    S(0, HT_NONE, HT_STRUCT, 1, "none <: struct");
    S(0, HT_NONE, HT_FUNC, 0, "none </: func (wrong hierarchy)");
    S(0, HT_NOFUNC, HT_NONE, 0, "nofunc </: none (disjoint bottoms)");
    S(0, HT_BOT, HT_ANY, 1, "bot <: any");
    S(0, HT_BOT, HT_FUNC, 1, "bot <: func (bot below all)");

    /* ── concrete types under a context: $0 struct, $1 array, $2 func, $3 struct<:$0 ── */
    static const uint8_t  KINDS[4] = { WST_STRUCT, WST_ARRAY, WST_FUNC, WST_STRUCT };
    static const int32_t  SUPERS[4] = { -1, -1, -1, 0 };   /* $3's declared supertype is $0 */
    static const jav_subtype_ctx_t cx = { KINDS, SUPERS, 4 };

    S(&cx, 0, HT_STRUCT, 1, "$0(struct) <: struct");
    S(&cx, 0, HT_EQ, 1, "$0(struct) <: eq");
    S(&cx, 0, HT_ANY, 1, "$0(struct) <: any");
    S(&cx, 0, HT_ARRAY, 0, "$0(struct) </: array");
    S(&cx, 1, HT_ARRAY, 1, "$1(array) <: array");
    S(&cx, 2, HT_FUNC, 1, "$2(func) <: func");
    S(&cx, 2, HT_ANY, 0, "$2(func) </: any (func hierarchy)");
    S(&cx, 3, 0, 1, "$3 <: $0 (declared supertype)");
    S(&cx, 0, 3, 0, "$0 </: $3 (not downward)");
    S(&cx, 3, HT_STRUCT, 1, "$3 <: struct (via kind)");
    S(&cx, 3, HT_ANY, 1, "$3 <: any (transitive)");
    S(&cx, 0, 1, 0, "$0 </: $1 (unrelated concretes)");
    S(&cx, 0, 0, 1, "$0 <: $0 (reflexive concrete)");
    S(&cx, HT_NONE, 0, 1, "none <: $0 (concrete struct in any-hierarchy)");
    S(&cx, HT_NONE, 2, 0, "none </: $2 (func, wrong hierarchy)");
    S(&cx, HT_NOFUNC, 2, 1, "nofunc <: $2 (concrete func)");
    S(&cx, HT_STRUCT, 0, 0, "struct </: $0 (abstract not below concrete)");

    /* ── reference subtyping: nullability (non-null <: nullable, not vice-versa) ── */
    R(&cx, 0,HT_STRUCT, 1,HT_STRUCT, 1, "(ref struct) <: (ref null struct)");
    R(&cx, 1,HT_STRUCT, 0,HT_STRUCT, 0, "(ref null struct) </: (ref struct)");
    R(&cx, 1,HT_STRUCT, 1,HT_STRUCT, 1, "(ref null struct) <: (ref null struct)");
    R(&cx, 0,0,        1,HT_EQ,     1, "(ref $0) <: (ref null eq)");
    R(&cx, 1,0,        0,HT_EQ,     0, "(ref null $0) </: (ref eq) [nullability]");

    /* ── cast compatibility (same top hierarchy) ── */
    #define C(a,b,want,msg) do { if (jav_ht_compatible(&cx,(a),(b))!=(want)) { printf("  FAIL  %s\n", msg); fails++; } } while(0)
    C(0, HT_STRUCT, 1, "$0 ~ struct (cast within any-hierarchy)");
    C(HT_ANY, 0, 1, "any ~ $0 (downcast to concrete struct)");
    C(HT_I31, HT_STRUCT, 1, "i31 ~ struct (both any-hierarchy)");
    C(HT_FUNC, HT_STRUCT, 0, "func !~ struct (disjoint hierarchies)");
    C(2, HT_STRUCT, 0, "$2(func) !~ struct (disjoint)");
    C(HT_EXTERN, HT_ANY, 0, "extern !~ any (disjoint)");
    #undef C

    if (fails) { printf("\n§3.3 subtype lattice: %d FAILURES\n", fails); return 1; }
    printf("  all §3.3 subtype edges verified\n");
    printf("\n§3.3 subtype lattice (independent conformance): ALL PASS\n");
    return 0;
}
