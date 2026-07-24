// test_lattice.c — the SIR optimizer's type lattice (type_lattice.c) over OUR
// type space: the full Java 1.0 type space. The point of interest is that
// long/float/double/char are present and behave as distinct sibling widths
// (you can't unify a long and a double in the value lattice; conversions are
// explicit). Pure algebra — sema is NULL, so reference meets use equal-class
// only (the class-hierarchy meet is exercised in test_sema).
#include "javelina/compiler/type_lattice.h"
#include "javelina/compiler/sema.h"
#include "java_parser.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

/* The representation authority is only meaningful against a REAL class table — the
 * PrimArray/RefArray overlays are SYNTHESIZED by sema, so a NULL sema cannot see them
 * (which is exactly why this half of the lattice went unpinned). Parse the prelude. */
/* §7.3 per-unit parse (see jtest_units.h). */
#include "jtest_units.h"


int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 4096);
    type_pool_t pool; type_pool_init(&pool, &arena);
    const sema_ctx_t* NS = NULL;  /* pure-algebra: no class hierarchy */

    const Type* top = type_top(&pool);
    const Type* bot = type_bottom(&pool);
    const Type* nul = type_null(&pool);

    /* The full Java 1.0 primitive width set, plus the v128 SIMD width (a value
     * width like the others: sibling of all, promoted with nothing). */
    sir_datatype_t widths[] = { SIR_DTBYTE, SIR_DTSHORT, SIR_DTCHAR, SIR_DTINT,
                                SIR_DTLONG, SIR_DTFLOAT, SIR_DTDOUBLE, SIR_DTV128 };
    const char* names[]     = { "byte","short","char","int","long","float","double","v128" };
    int NW = (int)(sizeof(widths)/sizeof(widths[0]));
    const Type* prim[8];
    for (int i = 0; i < NW; i++) prim[i] = type_make_prim(&pool, widths[i]);

    /* ── TOP is the meet identity; BOTTOM is absorbing ── */
    for (int i = 0; i < NW; i++) {
        CHECK(type_meet(NS, top, prim[i], &pool) == prim[i], "TOP is meet identity");
        CHECK(type_meet(NS, prim[i], bot, &pool) == bot, "BOTTOM absorbs");
        CHECK(type_leq(NS, top, prim[i]), "TOP <= everything");
        CHECK(type_leq(NS, prim[i], bot), "everything <= BOTTOM");
    }

    /* ── Each width is reflexive and interns to one Type ── */
    for (int i = 0; i < NW; i++) {
        CHECK(prim[i] == type_make_prim(&pool, widths[i]), "same width interns once");
        CHECK(type_meet(NS, prim[i], prim[i], &pool) == prim[i], "prim meet self");
        CHECK(type_leq(NS, prim[i], prim[i]), "prim <= self");
    }

    /* ── Distinct widths are incomparable SIBLINGS — the full-Java-1.0 fact:
     *    long/float/double/char don't unify with each other or with int ── */
    for (int i = 0; i < NW; i++)
        for (int j = 0; j < NW; j++)
            if (i != j) {
                char msg[64];
                snprintf(msg, sizeof msg, "%s meet %s = BOTTOM", names[i], names[j]);
                CHECK(type_meet(NS, prim[i], prim[j], &pool) == bot, msg);
                snprintf(msg, sizeof msg, "%s !<= %s", names[i], names[j]);
                CHECK(!type_leq(NS, prim[i], prim[j]), msg);
            }

    /* ── References, arrays, primitive-arrays, and null ── */
    const Type* refA  = type_make_ref(&pool, 0);
    const Type* refB  = type_make_ref(&pool, 1);
    const Type* arrI  = type_make_array(&pool, 1, 0);
    const Type* arr2I = type_make_array(&pool, 2, 0);
    const Type* parL  = type_make_prim_array(&pool, 1, SIR_DTLONG);
    const Type* parD  = type_make_prim_array(&pool, 1, SIR_DTDOUBLE);

    /* null ⊑ every reference kind, incomparable with primitives. */
    CHECK(type_leq(NS, nul, refA), "null <= ref");
    CHECK(type_leq(NS, nul, arrI), "null <= array");
    CHECK(type_leq(NS, nul, parL), "null <= prim-array");
    CHECK(!type_leq(NS, nul, prim[3]), "null !<= int (cross-kind)");
    CHECK(type_meet(NS, nul, refA, &pool) == refA, "null meet ref = ref");
    CHECK(type_meet(NS, nul, prim[4], &pool) == bot, "null meet long = BOTTOM");

    /* Cross-kind always meets to BOTTOM. */
    CHECK(type_meet(NS, prim[3], refA, &pool) == bot, "int meet ref = BOTTOM");
    CHECK(type_meet(NS, refA, parL, &pool) == bot, "ref meet prim-array = BOTTOM");
    CHECK(type_meet(NS, arrI, parL, &pool) == bot, "ref-array meet prim-array = BOTTOM");

    /* Equal-class refs/arrays are reflexive; distinct (no sema) -> BOTTOM;
     * array dim mismatch -> BOTTOM. */
    CHECK(type_meet(NS, refA, refA, &pool) == refA, "ref meet self");
    CHECK(type_meet(NS, refA, refB, &pool) == bot, "distinct refs (no sema) = BOTTOM");
    CHECK(type_meet(NS, arrI, arrI, &pool) == arrI, "array meet self");
    CHECK(type_meet(NS, arrI, arr2I, &pool) == bot, "array dim mismatch = BOTTOM");

    /* Primitive-array element widths are siblings too (long[] vs double[]). */
    CHECK(type_meet(NS, parL, parL, &pool) == parL, "long[] meet self");
    CHECK(type_meet(NS, parL, parD, &pool) == bot, "long[] meet double[] = BOTTOM");
    CHECK(type_leq(NS, parL, parL) && !type_leq(NS, parL, parD), "prim-array leq by width");

    /* Primitive arrays carry their dim: int[] and int[][] are distinct
     * lattice Types (dim mismatch = BOTTOM, like TK_ARRAY). */
    const Type* parI  = type_make_prim_array(&pool, 1, SIR_DTINT);
    const Type* parI2 = type_make_prim_array(&pool, 2, SIR_DTINT);
    CHECK(parI != parI2, "int[] and int[][] intern to distinct Types");
    CHECK(parI2 == type_make_prim_array(&pool, 2, SIR_DTINT), "same (dim,width) interns once");
    CHECK(type_meet(NS, parI, parI2, &pool) == bot, "int[] meet int[][] = BOTTOM");
    CHECK(type_meet(NS, parI2, parI2, &pool) == parI2, "int[][] meet self");
    CHECK(!type_leq(NS, parI, parI2) && !type_leq(NS, parI2, parI), "prim-array dims incomparable");
    CHECK(type_leq(NS, nul, parI2), "null <= int[][]");
    CHECK(type_meet(NS, nul, parI2, &pool) == parI2, "null meet int[][] = int[][]");

    /* ── The order WITH a class hierarchy (JLS §4.10.2) ───────────────────────
     *
     * Everything above runs with sema = NULL — pure algebra, no hierarchy. That is why
     * the lattice's REFERENCE order was never tested at all, and why it was WRONG: its
     * TK_REF case asked the EXTENDS CHAIN (`sema_is_subclass_of`), which answers "not a
     * subtype" for every interface, and its cross-kind rule meets an array with a class
     * to BOTTOM although JLS §10.7 makes every array an Object.
     *
     * The lattice is the type AUTHORITY (spec §10: "consulted, never duplicated"), so its
     * ⊑ must BE §4.10.2 — one predicate, `sema_ref_is_subtype`. A hierarchy built by hand
     * here: 0=Object, 1=I (interface), 2=A implements I, 3=B extends A, 4=C (unrelated). */
    {
        sema_ctx_t s;
        sema_init(&s, &arena);
        static const char* names[5] = { "Object", "I", "A", "B", "C" };
        for (int i = 0; i < 5; i++) {
            sema_class_t c;
            memset(&c, 0, sizeof c);
            c.name = names[i];
            c.fq_name = names[i];
            c.super_id = (i == 0 || i == 1) ? -1 : (i == 3 ? 2 : 0);
            c.is_interface = (i == 1);
            bbq_vec_push(s.classes, c);
        }
        /* A implements I. */
        int* ifaces = (int*)bbq_arena_alloc(&arena, sizeof(int));
        ifaces[0] = 1;
        s.classes[2].interface_ids   = ifaces;
        s.classes[2].interface_count = 1;
        s.wk.object_id    = 0;
        s.wk.cloneable_id = -1;

        const Type* tObj = type_make_ref(&pool, 0);
        const Type* tI   = type_make_ref(&pool, 1);
        const Type* tA   = type_make_ref(&pool, 2);
        const Type* tB   = type_make_ref(&pool, 3);
        const Type* tC   = type_make_ref(&pool, 4);
        const Type* aA   = type_make_array(&pool, 1, 2);   /* A[]  */
        const Type* aB   = type_make_array(&pool, 1, 3);   /* B[]  */
        const Type* pI   = type_make_prim_array(&pool, 1, SIR_DTINT);

        /* §8.1.4 the extends chain — the part that already worked. */
        CHECK(type_leq(&s, tB, tA), "B extends A ⟹ B ⊑ A");
        CHECK(!type_leq(&s, tA, tB), "…and not the other way");
        CHECK(!type_leq(&s, tA, tC), "unrelated classes are incomparable (fail closed)");

        /* §4.10.2 — an INTERFACE is a supertype nobody's extends chain mentions. This is
         * the one the old predicate got wrong, and it is the same bug that would have
         * deleted every object implementing an interface from a cast's points-to set. */
        CHECK(type_leq(&s, tA, tI), "A implements I ⟹ A ⊑ I");
        CHECK(type_leq(&s, tB, tI), "…and B, which inherits the implements from A");
        CHECK(!type_leq(&s, tC, tI), "…but not C, which does not implement it");

        /* §4.10.2 — EVERY reference type is a subtype of Object, including an interface
         * (which no extends chain records) and including arrays (§10.7). */
        CHECK(type_leq(&s, tA, tObj) && type_leq(&s, tI, tObj), "every ref ⊑ Object");
        CHECK(type_leq(&s, aA, tObj), "every ARRAY is an Object (§10.7)");
        CHECK(type_leq(&s, pI, tObj), "…including a primitive array");

        /* §10.2 array covariance: A[] ⊑ B[] iff A ⊑ B. */
        CHECK(type_leq(&s, aB, aA), "B[] ⊑ A[] — array covariance (§10.2)");
        CHECK(!type_leq(&s, aA, aB), "…and not the other way");

        /* MEET must agree with ⊑: if one side is already above the other, IT is the join.
         * Meeting an array with Object must give Object, not BOTTOM — `c ? new A[1] : o`
         * is an Object, and answering BOTTOM throws the type away. */
        CHECK(type_meet(&s, tA, tI, &pool) == tI, "A ⊔ I = I (I is above A)");
        CHECK(type_meet(&s, tB, tA, &pool) == tA, "B ⊔ A = A");
        CHECK(type_meet(&s, tA, tC, &pool) == tObj, "unrelated classes ⊔ = their LUB, Object");
        CHECK(type_meet(&s, aA, tObj, &pool) == tObj, "A[] ⊔ Object = Object (§10.7)");
        CHECK(type_meet(&s, pI, tObj, &pool) == tObj, "int[] ⊔ Object = Object");
        CHECK(type_meet(&s, aB, aA, &pool) == aA, "B[] ⊔ A[] = A[] (covariant elements)");

        /* …and the axioms still hold on the new cases (Click §3.2.1). */
        CHECK(type_meet(&s, tA, tI, &pool) == type_meet(&s, tI, tA, &pool), "meet commutes");
        CHECK(type_meet(&s, tA, tA, &pool) == tA, "meet is idempotent");
        CHECK(type_meet(&s, top, tA, &pool) == tA, "TOP is the meet identity");
        CHECK(type_meet(&s, bot, tA, &pool) == bot, "BOTTOM absorbs");
        sema_destroy(&s);
    }

    /* ── §5.6 NUMERIC PROMOTION — the lattice is the authority for it ────────────
     *
     * type_lattice.h calls itself "the JLS conversion authority (§5.1.2/§5.1.3/§5.6)", and
     * `lat_promote` is the §5.6.2 rule. It had NO pin — which is how const_expr.c came to
     * carry a second copy of it (with its own type enum), free to disagree. It now asks
     * this one, so this is the pin that keeps them honest.
     *
     * §5.6.1 UNARY promotion is not a separate rule: it is §5.6.2 against `int` (byte,
     * short and char widen to int; anything wider is unchanged). That identity is what
     * const_expr relies on, so it is pinned here rather than assumed. */
    {
        #define JT(t) ((java_type_t){ .tag = (t), .class_id = -1, .element = NULL })
        /* §5.6.2: double, else float, else long, else int. */
        CHECK(lat_promote(JT(JT_INT),   JT(JT_LONG))   == JT_LONG,   "int ⊕ long = long");
        CHECK(lat_promote(JT(JT_LONG),  JT(JT_FLOAT))  == JT_FLOAT,  "long ⊕ float = float");
        CHECK(lat_promote(JT(JT_FLOAT), JT(JT_DOUBLE)) == JT_DOUBLE, "float ⊕ double = double");
        CHECK(lat_promote(JT(JT_BYTE),  JT(JT_SHORT))  == JT_INT,    "byte ⊕ short = INT");
        CHECK(lat_promote(JT(JT_CHAR),  JT(JT_CHAR))   == JT_INT,    "char ⊕ char = INT");
        CHECK(lat_promote(JT(JT_INT),   JT(JT_INT))    == JT_INT,    "int ⊕ int = int");
        /* Commutative — a promotion that depended on operand order would silently give
         * `a + b` and `b + a` different types. */
        CHECK(lat_promote(JT(JT_FLOAT), JT(JT_LONG))
              == lat_promote(JT(JT_LONG), JT(JT_FLOAT)), "promotion is commutative");

        /* §5.6.1 as §5.6.2-against-int — the identity const_expr's unary_promote uses. */
        CHECK(lat_promote(JT(JT_BYTE),   JT(JT_INT)) == JT_INT,    "unary: byte → int");
        CHECK(lat_promote(JT(JT_SHORT),  JT(JT_INT)) == JT_INT,    "unary: short → int");
        CHECK(lat_promote(JT(JT_CHAR),   JT(JT_INT)) == JT_INT,    "unary: char → int");
        CHECK(lat_promote(JT(JT_LONG),   JT(JT_INT)) == JT_LONG,   "unary: long stays long");
        CHECK(lat_promote(JT(JT_FLOAT),  JT(JT_INT)) == JT_FLOAT,  "unary: float stays float");
        CHECK(lat_promote(JT(JT_DOUBLE), JT(JT_INT)) == JT_DOUBLE, "unary: double stays double");

        /* And the ONE tag→width authority agrees about which SIR width each tag lands in
         * — const_expr picks a literal node with it. */
        CHECK(lat_tag_to_dt(JT_BYTE)   == SIR_DTBYTE,   "byte → i8 width");
        CHECK(lat_tag_to_dt(JT_CHAR)   == SIR_DTCHAR,   "char → its own i16 width");
        CHECK(lat_tag_to_dt(JT_INT)    == SIR_DTINT,    "int → i32");
        CHECK(lat_tag_to_dt(JT_LONG)   == SIR_DTLONG,   "long → i64");
        CHECK(lat_tag_to_dt(JT_FLOAT)  == SIR_DTFLOAT,  "float → f32");
        CHECK(lat_tag_to_dt(JT_DOUBLE) == SIR_DTDOUBLE, "double → f64");
        #undef JT
    }

    type_pool_destroy(&pool);

    /* dt → WASM valtype: the ONE lowering map every consumer reads (slot
     * pools, substitution gates, locals emitter, burg i32-family test). */
    CHECK(lat_dt_valtype(SIR_DTBYTE)   == LAT_VT_I32, "valtype: byte→i32");
    CHECK(lat_dt_valtype(SIR_DTSHORT)  == LAT_VT_I32, "valtype: short→i32");
    CHECK(lat_dt_valtype(SIR_DTCHAR)   == LAT_VT_I32, "valtype: char→i32");
    CHECK(lat_dt_valtype(SIR_DTINT)    == LAT_VT_I32, "valtype: int→i32");
    CHECK(lat_dt_valtype(SIR_DTLONG)   == LAT_VT_I64, "valtype: long→i64");
    CHECK(lat_dt_valtype(SIR_DTFLOAT)  == LAT_VT_F32, "valtype: float→f32");
    CHECK(lat_dt_valtype(SIR_DTDOUBLE) == LAT_VT_F64, "valtype: double→f64");
    CHECK(lat_dt_valtype(SIR_DTREF)    == LAT_VT_REF, "valtype: ref→ref");
    CHECK(lat_dt_valtype(SIR_DTV128)   == LAT_VT_V128, "valtype: v128→v128 (NOT the i32 default)");

    /* atype → dt: the one SIR array-element-type → element-width map (the
     * inverse of lat_tag_to_atype over primitives; boolean packs as byte —
     * no separate bool storage width). */
    CHECK(lat_atype_to_dt(SIR_ATBOOL)   == SIR_DTBYTE,   "atype_to_dt: bool[] packs as byte");
    CHECK(lat_atype_to_dt(SIR_ATBYTE)   == SIR_DTBYTE,   "atype_to_dt: byte");
    CHECK(lat_atype_to_dt(SIR_ATSHORT)  == SIR_DTSHORT,  "atype_to_dt: short");
    CHECK(lat_atype_to_dt(SIR_ATCHAR)   == SIR_DTCHAR,   "atype_to_dt: char");
    CHECK(lat_atype_to_dt(SIR_ATINT)    == SIR_DTINT,    "atype_to_dt: int");
    CHECK(lat_atype_to_dt(SIR_ATLONG)   == SIR_DTLONG,   "atype_to_dt: long");
    CHECK(lat_atype_to_dt(SIR_ATFLOAT)  == SIR_DTFLOAT,  "atype_to_dt: float");
    CHECK(lat_atype_to_dt(SIR_ATDOUBLE) == SIR_DTDOUBLE, "atype_to_dt: double");
    CHECK(lat_atype_to_dt(SIR_ATV128)   == SIR_DTV128,   "atype_to_dt: v128 (NOT the ref default)");
    CHECK(lat_tag_to_atype(JT_V128)     == SIR_ATV128,   "tag_to_atype: v128 (NOT the int default)");

    /* The 8 storage slots (i8, i16-short, i16-char, i32, i64, f32, f64, v128):
     * boolean folds into byte's slot; char keeps its own distinct slot. */
    CHECK(lat_prim_storage_index(SIR_DTBYTE) == lat_prim_storage_index(SIR_DTBYTE), "storage: stable");
    CHECK(lat_prim_storage_index(SIR_DTCHAR) != lat_prim_storage_index(SIR_DTSHORT),
          "storage: char has its own slot, distinct from short");
    { int idx[8], n = 0;
      idx[n++] = lat_prim_storage_index(SIR_DTBYTE);
      idx[n++] = lat_prim_storage_index(SIR_DTSHORT);
      idx[n++] = lat_prim_storage_index(SIR_DTCHAR);
      idx[n++] = lat_prim_storage_index(SIR_DTINT);
      idx[n++] = lat_prim_storage_index(SIR_DTLONG);
      idx[n++] = lat_prim_storage_index(SIR_DTFLOAT);
      idx[n++] = lat_prim_storage_index(SIR_DTDOUBLE);
      idx[n++] = lat_prim_storage_index(SIR_DTV128);   /* v128 defaulting into int's slot = the silent-misclassify bug */
      for (int i = 0; i < n; i++)
          for (int j = i + 1; j < n; j++)
              CHECK(idx[i] != idx[j], "storage: the 8 width slots are distinct");
    }
    /* The storage-index INVERSE round-trips over all 8 widths — a local si→dt
     * table drifting from lat_prim_storage_index is the V128Array-clone bug. */
    { sir_datatype_t ws[] = { SIR_DTBYTE, SIR_DTSHORT, SIR_DTCHAR, SIR_DTINT,
                              SIR_DTLONG, SIR_DTFLOAT, SIR_DTDOUBLE, SIR_DTV128 };
      for (int i = 0; i < 8; i++) {
          CHECK(lat_prim_storage_dt(lat_prim_storage_index(ws[i])) == ws[i],
                "storage inverse: dt -> index -> dt round-trips");
          CHECK(lat_atype_to_dt(lat_dt_to_atype(ws[i])) == ws[i],
                "atype inverse: dt -> atype -> dt round-trips");
      }
    }

    /* ── JLS conversion authority (§5.1.2/§5.1.3/§5.6) — the spec tables ──
     * Pins the one type-conversion authority the whole frontend reads (sema, the
     * ddcg cast lowering, the module assembler). Verified against the JLS text. */
    /* type→dt (complete; the single map — no second copy in sema/compiler.c) */
    CHECK(lat_tag_to_dt(JT_BOOL)   == SIR_DTBYTE,   "tag_to_dt: bool→byte");
    CHECK(lat_tag_to_dt(JT_CHAR)   == SIR_DTCHAR,   "tag_to_dt: char→char");
    CHECK(lat_tag_to_dt(JT_LONG)   == SIR_DTLONG,   "tag_to_dt: long→long");
    CHECK(lat_tag_to_dt(JT_DOUBLE) == SIR_DTDOUBLE, "tag_to_dt: double→double");
    CHECK(lat_tag_to_dt(JT_CLASS)  == SIR_DTREF,    "tag_to_dt: class→ref");

    /* §5.1.2 widening — a sample of the 19, plus the non-widenings */
    CHECK( lat_is_widening_prim(jt_prim(JT_BYTE),  jt_prim(JT_INT)),    "widen: byte→int");
    CHECK( lat_is_widening_prim(jt_prim(JT_INT),   jt_prim(JT_LONG)),   "widen: int→long");
    CHECK( lat_is_widening_prim(jt_prim(JT_CHAR),  jt_prim(JT_INT)),    "widen: char→int");
    CHECK( lat_is_widening_prim(jt_prim(JT_FLOAT), jt_prim(JT_DOUBLE)), "widen: float→double");
    CHECK(!lat_is_widening_prim(jt_prim(JT_LONG),  jt_prim(JT_INT)),    "widen: long→int is NOT");
    CHECK(!lat_is_widening_prim(jt_prim(JT_CHAR),  jt_prim(JT_SHORT)),  "widen: char→short is NOT");
    CHECK(!lat_is_widening_prim(jt_prim(JT_BYTE),  jt_prim(JT_CHAR)),   "widen: byte→char is NOT");

    /* §5.1.3 narrowing — incl. the sideways conversions */
    CHECK( lat_is_narrowing_prim(jt_prim(JT_INT),  jt_prim(JT_BYTE)),   "narrow: int→byte");
    CHECK( lat_is_narrowing_prim(jt_prim(JT_LONG), jt_prim(JT_INT)),    "narrow: long→int");
    CHECK( lat_is_narrowing_prim(jt_prim(JT_CHAR), jt_prim(JT_SHORT)),  "narrow: char→short");
    CHECK( lat_is_narrowing_prim(jt_prim(JT_BYTE), jt_prim(JT_CHAR)),   "narrow: byte→char");
    CHECK(!lat_is_narrowing_prim(jt_prim(JT_BYTE), jt_prim(JT_INT)),    "narrow: byte→int is NOT");

    /* §5.6.2 binary numeric promotion */
    CHECK(lat_promote(jt_prim(JT_INT),   jt_prim(JT_LONG))   == JT_LONG,   "promote: int,long→long");
    CHECK(lat_promote(jt_prim(JT_INT),   jt_prim(JT_INT))    == JT_INT,    "promote: int,int→int");
    CHECK(lat_promote(jt_prim(JT_FLOAT), jt_prim(JT_DOUBLE)) == JT_DOUBLE, "promote: float,double→double");
    CHECK(lat_promote(jt_prim(JT_LONG),  jt_prim(JT_FLOAT))  == JT_FLOAT,  "promote: long,float→float");
    CHECK(lat_promote(jt_prim(JT_BYTE),  jt_prim(JT_SHORT))  == JT_INT,    "promote: byte,short→int");

    /* lat_num_conv — the single (from→to) conversion realization */
    CHECK(lat_num_conv(SIR_DTINT,   SIR_DTLONG)   == LAT_CONV_I2L,      "conv: int→long = I2L");
    CHECK(lat_num_conv(SIR_DTLONG,  SIR_DTINT)    == LAT_CONV_L2I,      "conv: long→int = L2I");
    CHECK(lat_num_conv(SIR_DTINT,   SIR_DTDOUBLE) == LAT_CONV_I2D,      "conv: int→double = I2D");
    CHECK(lat_num_conv(SIR_DTFLOAT, SIR_DTDOUBLE) == LAT_CONV_F2D,      "conv: float→double = F2D");
    CHECK(lat_num_conv(SIR_DTLONG,  SIR_DTFLOAT)  == LAT_CONV_L2F,      "conv: long→float = L2F");
    CHECK(lat_num_conv(SIR_DTINT,   SIR_DTINT)    == LAT_CONV_IDENTITY, "conv: int→int = identity");
    CHECK(lat_num_conv(SIR_DTBYTE,  SIR_DTINT)    == LAT_CONV_IDENTITY, "conv: byte→int = identity (i32)");
    CHECK(lat_num_conv(SIR_DTSHORT, SIR_DTINT)    == LAT_CONV_S2I,      "conv: short→int = S2I");
    CHECK(lat_num_conv(SIR_DTINT,   SIR_DTBYTE)   == LAT_CONV_I2B,      "conv: int→byte = I2B");
    CHECK(lat_num_conv(SIR_DTLONG,  SIR_DTBYTE)   == LAT_CONV_L2B,      "conv: long→byte = L2B (composite)");
    CHECK(lat_num_conv(SIR_DTCHAR,  SIR_DTSHORT)  == LAT_CONV_I2S,      "conv: char→short = I2S");
    CHECK(lat_num_conv(SIR_DTBYTE,  SIR_DTCHAR)   == LAT_CONV_I2C,      "conv: byte→char = I2C");
    CHECK(lat_num_conv(SIR_DTINT,   SIR_DTREF)    == LAT_CONV_NONE,     "conv: int→ref = none");

    /* ── v128: a value width with NO numeric conversions and NO promotion ──
     * Conversion is spelled as intrinsics (splat/extract), never `(cast)`. The
     * per-case fallthroughs would otherwise claim IDENTITY/I2B/I2L/... for a
     * v128 operand — each of these pins one lie. */
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTV128)   == LAT_CONV_IDENTITY, "conv: v128→v128 = identity");
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTINT)    == LAT_CONV_NONE,     "conv: v128→int = NONE (not identity)");
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTBYTE)   == LAT_CONV_NONE,     "conv: v128→byte = NONE (not I2B)");
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTLONG)   == LAT_CONV_NONE,     "conv: v128→long = NONE (not I2L)");
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTFLOAT)  == LAT_CONV_NONE,     "conv: v128→float = NONE (not I2F)");
    CHECK(lat_num_conv(SIR_DTV128, SIR_DTDOUBLE) == LAT_CONV_NONE,     "conv: v128→double = NONE (not I2D)");
    CHECK(lat_num_conv(SIR_DTINT,  SIR_DTV128)   == LAT_CONV_NONE,     "conv: int→v128 = NONE");
    CHECK(lat_promote_dt(SIR_DTV128, SIR_DTV128) == SIR_DTV128,        "promote_dt: v128 passes through (like ref)");
    CHECK(lat_promote_dt(SIR_DTV128, SIR_DTINT)  == SIR_DTV128,        "promote_dt: v128 never promotes to int");
    CHECK(lat_unary_promote_dt(SIR_DTV128)       == SIR_DTV128,        "unary promote: v128 unchanged");
    CHECK(!jt_is_numeric(jt_prim(JT_V128)),                            "v128 is NOT a JLS numeric type");
    CHECK(!lat_is_widening_prim(jt_prim(JT_V128), jt_prim(JT_INT)),    "no widening from v128");
    CHECK(!lat_is_narrowing_prim(jt_prim(JT_INT), jt_prim(JT_V128)),   "no narrowing to v128");

    /* ── The REFERENCE / ARRAY REPRESENTATION authority ────────────────────────────
     *
     * Everything above is the primitive-conversion half. This is the OTHER half — the
     * one the header calls "the §10.2 array REPRESENTATION authority … The single
     * authority; consumers turn it into a struct/array typeidx and NEVER RE-DECIDE" —
     * and until now NOTHING pinned it. That is how `sir_ref_descriptor` came to keep a
     * private copy of the mapping, get the concrete-backing case wrong, and emit a jre
     * the §7.6 validator rejected (584 exec failures). An authority nobody tests is not
     * an authority; it is a suggestion.
     *
     * These take a sema, so this section builds a REAL one (the prelude's synthesized
     * overlay classes are the whole point — they do not exist in a NULL sema). */
    {
        sema_ctx_t s; sema_init(&s, &arena);
        jtest_build_flat(NULL, &arena);   /* prelude only, no user unit */
        jtest_analyze(&s);

        /* ── the ROOT and the value-class collapse ── */
        int32_t root = lat_root_class(&s);
        CHECK(root >= 0, "root: java.lang.Object resolves (the unique super-less class)");
        int32_t obj = sema_find_class(&s, "java.lang.Object");
        CHECK(root == obj, "root == java.lang.Object");
        CHECK(lat_value_class(&s, root) == root, "value_class: a class is itself");
        /* An INTERFACE value is an object — it collapses to the root (no interface
         * object is ever instantiated). */
        int32_t cln = sema_find_class(&s, "java.lang.Cloneable");
        if (cln >= 0)
            CHECK(lat_value_class(&s, cln) == root,
                  "value_class: an INTERFACE collapses to the root — an interface value "
                  "is just an object");

        /* ── the handler LANDING type (added 07-14; §6's catch-all carries no type) ── */
        int32_t thr = s.wk.throwable_id;
        CHECK(lat_handler_landing_class(&s, thr) == lat_value_class(&s, thr),
              "landing: a TYPED catch lands as its declared class");
        CHECK(lat_handler_landing_class(&s, -1) == lat_value_class(&s, thr),
              "landing: the catch-all (catch_class_id -1: no declared type) lands as "
              "Throwable — the representation answer, so the SEMANTIC field stays -1");

        /* ── §10.2's collapse predicate ── */
        java_type_t p_int = jt_prim(JT_INT);
        java_type_t cls   = jt_class(root);
        java_type_t a_int = jt_array(&p_int);
        CHECK(lat_array_elem_is_ref(cls)   == true,  "elem_is_ref: a CLASS element → ref");
        CHECK(lat_array_elem_is_ref(a_int) == true,  "elem_is_ref: a NESTED ARRAY element → ref");
        CHECK(lat_array_elem_is_ref(p_int) == false, "elem_is_ref: a PRIMITIVE element → not ref");

        /* ── the overlay classes exist and are distinct per width ── */
        int32_t ra = lat_refarray_class(&s);
        CHECK(ra >= 0, "refarray: the ONE synthesized RefArray class exists");
        int32_t pa_i = lat_primarray_class(&s, SIR_DTINT);
        int32_t pa_d = lat_primarray_class(&s, SIR_DTDOUBLE);
        CHECK(pa_i >= 0 && pa_d >= 0, "primarray: a per-width PrimArray overlay exists");
        CHECK(pa_i != pa_d, "primarray: the widths are DISTINCT overlays");
        CHECK(pa_i != ra,   "primarray: a PrimArray is not the RefArray");
        int32_t pa_v = lat_primarray_class(&s, SIR_DTV128);
        CHECK(pa_v >= 0,    "primarray: the v128 (8th width) overlay exists");
        CHECK(pa_v != pa_i && pa_v != pa_d && pa_v != ra,
              "primarray: V128Array is its own overlay — a v128 defaulting into "
              "int's storage slot is the silent-misclassify bug");
        CHECK(lat_is_array_data_cell(&s, pa_v, 0) == true,
              "data_cell: V128Array.data (field 0) IS a backing-store cell (si<8)");

        /* ── lat_array_overlay_class: THE function sir_ref_descriptor should have asked ──
         * "or -1 when `arr` is the concrete backing of an overlay (a JT_ARRAY_RAW-marked
         *  array, or a JT_NULL element)". Both markers, both directions. */
        java_type_t a_cls  = jt_array(&cls);
        java_type_t aa_cls = jt_array(&a_cls);
        CHECK(lat_array_overlay_class(&s, a_int) == pa_i,
              "overlay: int[] is represented by the int PrimArray overlay");
        CHECK(lat_array_overlay_class(&s, a_cls) == ra,
              "overlay: C[] is represented by the ONE RefArray (covariance is identity)");
        CHECK(lat_array_overlay_class(&s, aa_cls) == ra,
              "overlay: C[][] — a nested array element is a REF — is also RefArray");

        java_type_t raw_int = jt_raw_array(&p_int);
        CHECK(lat_array_overlay_class(&s, raw_int) == -1,
              "overlay: a JT_ARRAY_RAW backing is NOT overlaid — it is the concrete "
              "(array W), and re-overlaying it is what type-mismatched the jre");
        java_type_t nul_e   = jt_null();
        java_type_t raw_ref = jt_array(&nul_e);
        CHECK(lat_array_overlay_class(&s, raw_ref) == -1,
              "overlay: RefArray's own backing (a JT_NULL element) is NOT overlaid either "
              "— the SECOND marker, and the one that survived the first fix");

        /* ── the backing-store CELL (the optimizer's immutability licence) ──
         * The two overlays do NOT agree on the index — PrimArray.data is field 0, but
         * RefArray's field 0 is `elem` (its component Class) and `data` is field 1
         * (ddcg_primarray_data_field / ddcg_refarray_data_field). Which is precisely why
         * this is the lattice's question and not the caller's to guess at. */
        CHECK(lat_is_array_data_cell(&s, pa_i, 0) == true,
              "data_cell: PrimArray.data (field 0) IS the backing-store cell");
        CHECK(lat_is_array_data_cell(&s, ra, 1) == true,
              "data_cell: RefArray.data (field 1) IS the backing-store cell");
        CHECK(lat_is_array_data_cell(&s, ra, 0) == false,
              "data_cell: RefArray's field 0 is `elem` (the component Class), NOT the "
              "backing — the two overlays disagree on the index, so nobody may assume it");
        CHECK(lat_is_array_data_cell(&s, root, 0) == false,
              "data_cell: an ordinary class's field 0 is NOT a backing store");
        sema_destroy(&s);
    }

    return TEST_SUMMARY("test_lattice");
}
