// test_gamma.c — structural coverage of the Click γ-table (sir_op_gamma),
// ported from yoctojc's test_sir_op_gamma_coverage.c and adapted to the
// distinct comparison nodes. Pins the invariants the engine relies on when it
// indexes sir_op_gamma[node->tag]: every tag has a row, the row's tag matches
// its index, mnemonics present, the commutative set, and the comparison rows'
// fold/cong-fold. A drift fails a named test, not as a mystery regression.
#include "javelina/compiler/sir_op_gamma.h"
#include "javelina/compiler/sir_optimizer.h"
#include "javelina/compiler/type_lattice.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

int main(void) {
    // 1. Every row is indexed by its own tag — so sir_op_gamma[node->tag]
    //    reads the right row. This also requires EVERY tag (incl. the new
    //    Eq/Ne/Lt/Le/Gt/Ge and LoadLongConst/Float/Double) to have a row.
    for (int i = 0; i < SIR_TAG_COUNT; i++) {
        if ((int)sir_op_gamma[i].tag != i) {
            printf("  FAIL  sir_op_gamma[%d].tag = %d (missing/mis-tagged row)\n",
                   i, (int)sir_op_gamma[i].tag);
            fails++;
        }
    }

    // 2. Every row carries a non-empty mnemonic (dumps/diagnostics key off it).
    for (int i = 0; i < SIR_TAG_COUNT; i++)
        CHECK(sir_op_gamma[i].mnemonic && sir_op_gamma[i].mnemonic[0],
              "every row has a non-empty mnemonic");

    // 3. The commutative set (§3.2.1): arithmetic ADD/MUL + bitwise AND/OR/XOR,
    //    plus the comparison EQ/NE (a==b ≡ b==a). LT/LE/GT/GE are not.
    for (int i = 0; i < SIR_TAG_COUNT; i++) {
        bool expected = (i == SIR_ADD || i == SIR_MUL || i == SIR_AND
                      || i == SIR_OR  || i == SIR_XOR
                      || i == SIR_EQ  || i == SIR_NE);
        CHECK(sir_op_gamma[i].is_commutative == expected,
              "is_commutative matches the §3.2.1 set");
    }

    // 4. Each comparison row: arity 2, a fold_cmp, and the §4.6 reflexive fold.
    int cmps[] = { SIR_EQ, SIR_NE, SIR_LT, SIR_LE, SIR_GT, SIR_GE };
    for (size_t i = 0; i < sizeof(cmps)/sizeof(cmps[0]); i++) {
        const sir_op_gamma_t* g = &sir_op_gamma[cmps[i]];
        CHECK(g->arity == 2, "comparison row arity 2");
        CHECK(g->fold_cmp != NULL, "comparison row has fold_cmp");
        CHECK(g->cong_fold == GC_CMP_REFLEXIVE, "comparison row is reflexive-folding");
    }

    // 5. fold_cmp dispatches by tag and computes the right 0/1.
    const sir_op_gamma_t* glt = &sir_op_gamma[SIR_LT];
    CHECK(glt->fold_cmp(SIR_LT, 3, 5) == 1 && glt->fold_cmp(SIR_LT, 5, 3) == 0, "LT fold");
    CHECK(sir_op_gamma[SIR_EQ].fold_cmp(SIR_EQ, 4, 4) == 1, "EQ fold (equal)");
    CHECK(sir_op_gamma[SIR_NE].fold_cmp(SIR_NE, 4, 4) == 0, "NE fold (equal)");
    CHECK(sir_op_gamma[SIR_GE].fold_cmp(SIR_GE, 5, 5) == 1, "GE fold (reflexive true)");

    // 6. Move* bit-preserving conversions (typed bitcast): fixed result domain + the fold
    //    reinterprets the constant's bits (raw, not a numeric convert). IEEE-754 known-answers.
    CHECK(sir_op_gamma[SIR_MOVEF2I].type_prim_fixed_dt == SIR_DTINT,    "MoveF2I result i32");
    CHECK(sir_op_gamma[SIR_MOVEI2F].type_prim_fixed_dt == SIR_DTFLOAT,  "MoveI2F result f32");
    CHECK(sir_op_gamma[SIR_MOVED2L].type_prim_fixed_dt == SIR_DTLONG,   "MoveD2L result i64");
    CHECK(sir_op_gamma[SIR_MOVEL2D].type_prim_fixed_dt == SIR_DTDOUBLE, "MoveL2D result f64");
    /* An f32 KNOWN lives in `fvalue`, NOT in the double: widening an f32 to
     * double quiets a signaling NaN, and Move* is a bit-preserving reinterpret
     * (§20.9.18 floatToRawIntBits does not canonicalise). */
    { cp_const_t f1 = { .state = CP_C_KNOWN, .cwidth = CP_W_F32, .fvalue = 1.0f };
      CHECK(sir_op_gamma[SIR_MOVEF2I].fold_convert(f1).value == 0x3F800000, "MoveF2I fold 1.0f -> 0x3F800000"); }
    { cp_const_t i1 = { .state = CP_C_KNOWN, .cwidth = CP_W_I32, .value = 0x3F800000 };
      CHECK(sir_op_gamma[SIR_MOVEI2F].fold_convert(i1).fvalue == 1.0f, "MoveI2F fold 0x3F800000 -> 1.0f"); }
    /* The raw sNaN payload survives the round-trip — the reason f32 has its own
     * carrier at all (a double detour returns 0x7FC00001). */
    { cp_const_t sn = { .state = CP_C_KNOWN, .cwidth = CP_W_I32, .value = 0x7F800001 };
      cp_const_t f = sir_op_gamma[SIR_MOVEI2F].fold_convert(sn);
      CHECK(sir_op_gamma[SIR_MOVEF2I].fold_convert(f).value == 0x7F800001,
            "Move round-trip preserves the raw sNaN bits (0x7F800001)"); }
    { cp_const_t d1 = { .state = CP_C_KNOWN, .cwidth = CP_W_F64, .dvalue = 1.0 };
      CHECK(sir_op_gamma[SIR_MOVED2L].fold_convert(d1).lvalue == 0x3FF0000000000000LL, "MoveD2L fold 1.0 -> 0x3FF0..."); }
    { cp_const_t l1 = { .state = CP_C_KNOWN, .cwidth = CP_W_I64, .lvalue = 0x3FF0000000000000LL };
      CHECK(sir_op_gamma[SIR_MOVEL2D].fold_convert(l1).dvalue == 1.0, "MoveL2D fold 0x3FF0... -> 1.0"); }

    // 7. γ_T produces JLS-correct lattice kinds — no JavaCard 16-bit
    //    holdovers. arr.length is int (§10.7); comparisons, instanceof and
    //    the Class-instantiable test are boolean (§15.20/§15.20.2/§15.21)
    //    = the compiler's JT_BOOL→byte convention (lat_tag_to_dt).
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        type_pool_t pool; type_pool_init(&pool, &a);

        sir_node_t* arr = sir_load_local(&a, 0, SIR_DTREF, NULL);
        const Type* tlen = gamma_type_for_node(NULL, sir_array_length(&a, arr), &pool);
        CHECK(tlen->kind == TK_PRIM && tlen->prim.width == SIR_DTINT,
              "arraylength γ type is prim(int) — JLS §10.7");

        sir_node_t* l = sir_load_const(&a, 1, SIR_DTINT);
        sir_node_t* r = sir_load_const(&a, 2, SIR_DTINT);
        sir_node_t* cmps[] = { sir_eq(&a, l, r), sir_ne(&a, l, r),
                               sir_lt(&a, l, r), sir_le(&a, l, r),
                               sir_gt(&a, l, r), sir_ge(&a, l, r) };
        for (size_t i = 0; i < sizeof(cmps)/sizeof(cmps[0]); i++) {
            const Type* t = gamma_type_for_node(NULL, cmps[i], &pool);
            CHECK(t->kind == TK_PRIM && t->prim.width == SIR_DTBYTE,
                  "comparison γ type is prim(byte) — boolean convention");
        }
        const Type* tio = gamma_type_for_node(NULL,
            sir_instance_of(&a, arr, SIR_ATCLASS, 3), &pool);
        CHECK(tio->kind == TK_PRIM && tio->prim.width == SIR_DTBYTE,
              "instanceof γ type is prim(byte) — boolean convention");
        const Type* tci = gamma_type_for_node(NULL,
            sir_class_instantiable(&a, arr), &pool);
        CHECK(tci->kind == TK_PRIM && tci->prim.width == SIR_DTBYTE,
              "classinstantiable γ type is prim(byte) — boolean convention");

        // NewArray covers EVERY primitive element kind (char[] is
        // ubiquitous — String), each an interned prim-array of the
        // lattice's atype→dt width, dim 1. Boolean packs as byte.
        struct { sir_atype_t at; sir_datatype_t dt; const char* m; } na[] = {
            { SIR_ATBOOL,   SIR_DTBYTE,   "newarray bool → prim_array(byte)" },
            { SIR_ATBYTE,   SIR_DTBYTE,   "newarray byte → prim_array(byte)" },
            { SIR_ATSHORT,  SIR_DTSHORT,  "newarray short → prim_array(short)" },
            { SIR_ATCHAR,   SIR_DTCHAR,   "newarray char → prim_array(char)" },
            { SIR_ATINT,    SIR_DTINT,    "newarray int → prim_array(int)" },
            { SIR_ATLONG,   SIR_DTLONG,   "newarray long → prim_array(long)" },
            { SIR_ATFLOAT,  SIR_DTFLOAT,  "newarray float → prim_array(float)" },
            { SIR_ATDOUBLE, SIR_DTDOUBLE, "newarray double → prim_array(double)" },
        };
        for (size_t i = 0; i < sizeof(na)/sizeof(na[0]); i++) {
            sir_node_t* n = sir_new_array(&a, na[i].at,
                                          sir_load_const(&a, 4, SIR_DTINT));
            const Type* t = gamma_type_for_node(NULL, n, &pool);
            CHECK(t->kind == TK_PRIM_ARRAY
                  && t->prim_array.width == na[i].dt
                  && t->prim_array.dim == 1, na[i].m);
        }

        // 8. gamma_jt_to_type keeps primitive-array precision: int[] and
        //    int[][] are distinct prim-array Types; class arrays unchanged.
        java_type_t ji  = jt_prim(JT_INT);
        java_type_t jai = jt_array(&ji);
        const Type* tai = gamma_jt_to_type(jai, &pool);
        CHECK(tai->kind == TK_PRIM_ARRAY && tai->prim_array.width == SIR_DTINT
              && tai->prim_array.dim == 1, "jt int[] → prim_array(int, dim 1)");
        java_type_t jaai = jt_array(&jai);
        const Type* taai = gamma_jt_to_type(jaai, &pool);
        CHECK(taai->kind == TK_PRIM_ARRAY && taai->prim_array.width == SIR_DTINT
              && taai->prim_array.dim == 2, "jt int[][] → prim_array(int, dim 2)");
        java_type_t jch = jt_prim(JT_CHAR);
        java_type_t jac = jt_array(&jch);
        const Type* tac = gamma_jt_to_type(jac, &pool);
        CHECK(tac->kind == TK_PRIM_ARRAY && tac->prim_array.width == SIR_DTCHAR
              && tac->prim_array.dim == 1, "jt char[] → prim_array(char, dim 1)");
        java_type_t jcl = jt_class(7);
        java_type_t jacl = jt_array(&jcl);
        const Type* tacl = gamma_jt_to_type(jacl, &pool);
        CHECK(tacl->kind == TK_ARRAY && tacl->array.dim == 1
              && tacl->array.class_id == 7, "jt C[] → array(1, C) — unchanged");
        const Type* tcl = gamma_jt_to_type(jcl, &pool);
        CHECK(tcl->kind == TK_REF && tcl->ref.class_id == 7, "jt C → ref(C) — unchanged");
        const Type* tpi = gamma_jt_to_type(jt_prim(JT_INT), &pool);
        CHECK(tpi->kind == TK_PRIM && tpi->prim.width == SIR_DTINT,
              "jt bare int → prim(int) — no BOTTOM gap for prim fields/returns");
        const Type* tpb = gamma_jt_to_type(jt_prim(JT_BOOL), &pool);
        CHECK(tpb->kind == TK_PRIM && tpb->prim.width == SIR_DTBYTE,
              "jt bare boolean → prim(byte) — the JT_BOOL convention");

        // 9. sir_ref_descriptor — THE ref-descriptor authority (compiler_helpers.c),
        //    declared here beside its inverse gamma_ref_to_type. It maps a sema
        //    java_type_t to the descriptor node the SIR carries so a reference is never
        //    type-erased. It had NO test: the DDCG was its only caller and never asked it
        //    the one question it gets wrong, so §6's scalar replacement — the first caller
        //    to ask generically — emitted a type-invalid jre (584 exec failures).
        //
        //    ENUMERATED over the java_type_t space, not just the case that bit us.
        {
            java_type_t p_int = jt_prim(JT_INT);
            java_type_t p_chr = jt_prim(JT_CHAR);
            java_type_t cls   = jt_class(7);

            // primitive → NULL (the data_type fully describes it)
            CHECK(sir_ref_descriptor(&a, p_int) == NULL,
                  "§9: a primitive has no ref descriptor");
            // class → ClassRef
            sir_node_t* d_cls = sir_ref_descriptor(&a, cls);
            CHECK(d_cls && d_cls->tag == SIR_CLASSREF && d_cls->class_ref.class_id == 7,
                  "§9: C → ClassRef(C)");
            // C[] → ArrayRef(C, 1); C[][] → dim 2
            java_type_t a_cls = jt_array(&cls), aa_cls = jt_array(&a_cls);
            sir_node_t* d_ac = sir_ref_descriptor(&a, a_cls);
            CHECK(d_ac && d_ac->tag == SIR_ARRAYREF && d_ac->array_ref.class_id == 7
               && d_ac->array_ref.dim == 1, "§9: C[] → ArrayRef(C, dim 1)");
            sir_node_t* d_aac = sir_ref_descriptor(&a, aa_cls);
            CHECK(d_aac && d_aac->tag == SIR_ARRAYREF && d_aac->array_ref.dim == 2,
                  "§9: C[][] → ArrayRef(C, dim 2)");
            // int[] → PrimArray(int, 1); char[] → PrimArray(char, 1)
            java_type_t a_int = jt_array(&p_int), a_chr = jt_array(&p_chr);
            sir_node_t* d_ai = sir_ref_descriptor(&a, a_int);
            CHECK(d_ai && d_ai->tag == SIR_PRIMARRAY && d_ai->prim_array.width == SIR_DTINT
               && d_ai->prim_array.dim == 1, "§9: int[] → PrimArray(int, dim 1)");
            sir_node_t* d_ac2 = sir_ref_descriptor(&a, a_chr);
            CHECK(d_ac2 && d_ac2->tag == SIR_PRIMARRAY
               && d_ac2->prim_array.width == SIR_DTCHAR, "§9: char[] → PrimArray(char)");

            // ── THE CASE THAT WAS NEVER ASKED (sema.h:55) ────────────────────────
            // "A JT_ARRAY marked (class_id == JT_ARRAY_RAW) is the CONCRETE backing of an
            //  array overlay — the overlay's own `data` field, an (array W)/(array anyref)
            //  that MUST NOT be re-overlaid into a PrimArray/RefArray."
            //
            // sir_ref_descriptor never looks at class_id, so it re-overlays it and hands
            // back (ref $PrimArray) for a value that IS (array i32). The descriptor must
            // never claim an overlay for the concrete backing — that is the type mismatch
            // the validator rejected the jre for.
            java_type_t raw_int = jt_raw_array(&p_int);
            sir_node_t* d_raw = sir_ref_descriptor(&a, raw_int);
            CHECK(!(d_raw && d_raw->tag == SIR_PRIMARRAY),
                  "§9: the overlay's raw BACKING must not be re-overlaid into a PrimArray "
                  "(sema.h:55) — it is (array W), not (ref $PrimArray)");

            // …and the backing is marked TWO ways, per type_lattice.h:151 — "a PRIMITIVE
            // array (and RefArray's own backing, whose element is the top reference =
            // JT_NULL) stays a concrete, invariant array". PrimArray.data is JT_ARRAY_RAW;
            // RefArray.data is an array with a JT_NULL ELEMENT (sema.c:968 says so in as
            // many words). Both are the concrete backing; neither is nameable.
            java_type_t p_null   = jt_null();
            java_type_t raw_null = jt_array(&p_null);
            sir_node_t* d_rn = sir_ref_descriptor(&a, raw_null);
            CHECK(d_rn == NULL,
                  "§9: RefArray's jt_null BACKING is not nameable either — a PrimArray of "
                  "width DTREF is not a type (type_lattice.h:151)");
        }

        type_pool_destroy(&pool);
        bbq_arena_free(&a);
    }

    if (fails) { printf("test_gamma: %d FAILED\n", fails); return 1; }
    printf("test_gamma: OK\n");
    return 0;
}
