// test_codegen_wasm.c — EXHAUSTIVE byte-exact tiling of every codegen_wasm.burg
// value + spine rule. burgc --coverage proves each SIR node HAS a rule; it does
// NOT prove the rule emits the right bytes (a rule naming WOP_I32_SUB where it
// meant WOP_I32_ADD passes coverage). So every rule is pinned here to its exact
// WASM bytes — the enumerated checklist that confirms breadth instead of letting
// a later corpus discover it. Each value tree is wrapped in Return so it reduces
// to the START nonterminal; Return emits a trailing 0x0F (type-agnostic).
#include "codegen_matcher.h"
#include <stdio.h>
#include <string.h>

#include "javelina_test.h"

static int eq_code(const burg_ctx_t* ctx, const uint8_t* exp, int n) {
    return (int)bbq_vec_len(ctx->emit.code) == n && memcmp(ctx->emit.code, exp, (size_t)n) == 0;
}

/* Tile `tree` (must reduce to START) and compare emitted bytes. */
#define TILE(tree, m, ...) do { \
    burg_ctx_t ctx = {0}; burg_ctx_init(&ctx); \
    burg_rewrite((tree), &ctx); \
    const uint8_t want[] = { __VA_ARGS__ }; \
    CHECK(!burg_has_error(&ctx), m " (no burg error)"); \
    if (!eq_code(&ctx, want, (int)sizeof want)) { \
        printf("  FAIL  %s\n    want:", m); \
        for (int i=0;i<(int)sizeof want;i++) printf(" %02X", want[i]); \
        printf("\n    got: "); \
        for (int i=0;i<(int)bbq_vec_len(ctx.emit.code);i++) printf(" %02X", ctx.emit.code[i]); \
        printf("\n"); TEST_FAILED(); \
    } \
    bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx); } while (0)

int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 4096);
    bbq_arena* a = &arena;

    /* typed operands (local.get is polymorphic → 0x20 slot; the data_type only
     * selects the nonterminal the parent op matches against). */
    #define I0 sir_load_local(a, 0, SIR_DTINT, NULL)
    #define I1 sir_load_local(a, 1, SIR_DTINT, NULL)
    #define L0 sir_load_local(a, 0, SIR_DTLONG, NULL)
    #define L1 sir_load_local(a, 1, SIR_DTLONG, NULL)
    #define F0 sir_load_local(a, 0, SIR_DTFLOAT, NULL)
    #define F1 sir_load_local(a, 1, SIR_DTFLOAT, NULL)
    #define D0 sir_load_local(a, 0, SIR_DTDOUBLE, NULL)
    #define D1 sir_load_local(a, 1, SIR_DTDOUBLE, NULL)
    #define RI(t) sir_return(a, (t), SIR_DTINT)
    #define RL(t) sir_return(a, (t), SIR_DTLONG)
    #define RF(t) sir_return(a, (t), SIR_DTFLOAT)
    #define RD(t) sir_return(a, (t), SIR_DTDOUBLE)

    /* ── constants ─────────────────────────────────────────────────────── */
    TILE(RI(sir_load_const(a, 5, SIR_DTINT)), "i32.const 5",      0x41,0x05, 0x0F);
    TILE(RL(sir_load_long_const(a, 5)),       "i64.const 5",      0x42,0x05, 0x0F);
    TILE(RF(sir_load_float_const(a, 1.5f)),   "f32.const 1.5",    0x43,0x00,0x00,0xC0,0x3F, 0x0F);
    TILE(RD(sir_load_double_const(a, 2.25)),  "f64.const 2.25",   0x44,0,0,0,0,0,0,0x02,0x40, 0x0F);

    /* ── i32 arithmetic / bitwise / shift ──────────────────────────────── */
    TILE(RI(sir_add(a,SIR_DTINT,I0,I1)), "i32.add",  0x20,0,0x20,1,0x6A,0x0F);
    TILE(RI(sir_sub(a,SIR_DTINT,I0,I1)), "i32.sub",  0x20,0,0x20,1,0x6B,0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,I1)), "i32.mul",  0x20,0,0x20,1,0x6C,0x0F);
    TILE(RI(sir_div(a,SIR_DTINT,I0,I1)), "i32.div_s",0x20,0,0x20,1,0x6D,0x0F);
    TILE(RI(sir_rem(a,SIR_DTINT,I0,I1)), "i32.rem_s",0x20,0,0x20,1,0x6F,0x0F);
    TILE(RI(sir_and(a,SIR_DTINT,I0,I1)), "i32.and",  0x20,0,0x20,1,0x71,0x0F);
    TILE(RI(sir_or (a,SIR_DTINT,I0,I1)), "i32.or",   0x20,0,0x20,1,0x72,0x0F);
    TILE(RI(sir_xor(a,SIR_DTINT,I0,I1)), "i32.xor",  0x20,0,0x20,1,0x73,0x0F);
    TILE(RI(sir_shl(a,SIR_DTINT,I0,I1)), "i32.shl",  0x20,0,0x20,1,0x74,0x0F);
    TILE(RI(sir_shr(a,SIR_DTINT,I0,I1)), "i32.shr_s",0x20,0,0x20,1,0x75,0x0F);
    TILE(RI(sir_ushr(a,SIR_DTINT,I0,I1)),"i32.shr_u",0x20,0,0x20,1,0x76,0x0F);
    /* no i32.neg → x * -1 (operand reduced first, then const -1, then mul). */
    TILE(RI(sir_neg(a,SIR_DTINT,I0)),    "i32 neg = x * -1", 0x20,0, 0x41,0x7F, 0x6C, 0x0F);

    /* ── i64 arithmetic / bitwise / shift ──────────────────────────────── */
    TILE(RL(sir_add(a,SIR_DTLONG,L0,L1)), "i64.add",  0x20,0,0x20,1,0x7C,0x0F);
    TILE(RL(sir_sub(a,SIR_DTLONG,L0,L1)), "i64.sub",  0x20,0,0x20,1,0x7D,0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,L0,L1)), "i64.mul",  0x20,0,0x20,1,0x7E,0x0F);
    TILE(RL(sir_div(a,SIR_DTLONG,L0,L1)), "i64.div_s",0x20,0,0x20,1,0x7F,0x0F);
    TILE(RL(sir_rem(a,SIR_DTLONG,L0,L1)), "i64.rem_s",0x20,0,0x20,1,0x81,0x0F);
    TILE(RL(sir_and(a,SIR_DTLONG,L0,L1)), "i64.and",  0x20,0,0x20,1,0x83,0x0F);
    TILE(RL(sir_or (a,SIR_DTLONG,L0,L1)), "i64.or",   0x20,0,0x20,1,0x84,0x0F);
    TILE(RL(sir_xor(a,SIR_DTLONG,L0,L1)), "i64.xor",  0x20,0,0x20,1,0x85,0x0F);
    TILE(RL(sir_shl(a,SIR_DTLONG,L0,L1)), "i64.shl",  0x20,0,0x20,1,0x86,0x0F);
    TILE(RL(sir_shr(a,SIR_DTLONG,L0,L1)), "i64.shr_s",0x20,0,0x20,1,0x87,0x0F);
    TILE(RL(sir_ushr(a,SIR_DTLONG,L0,L1)),"i64.shr_u",0x20,0,0x20,1,0x88,0x0F);
    /* i64 neg = x * -1 (i64.const -1; i64.mul). */
    TILE(RL(sir_neg(a,SIR_DTLONG,L0)),    "i64 neg = x * -1", 0x20,0, 0x42,0x7F, 0x7E, 0x0F);

    /* ── f32 / f64 arithmetic (real neg ops exist) ─────────────────────── */
    TILE(RF(sir_add(a,SIR_DTFLOAT,F0,F1)), "f32.add", 0x20,0,0x20,1,0x92,0x0F);
    TILE(RF(sir_sub(a,SIR_DTFLOAT,F0,F1)), "f32.sub", 0x20,0,0x20,1,0x93,0x0F);
    TILE(RF(sir_mul(a,SIR_DTFLOAT,F0,F1)), "f32.mul", 0x20,0,0x20,1,0x94,0x0F);
    TILE(RF(sir_div(a,SIR_DTFLOAT,F0,F1)), "f32.div", 0x20,0,0x20,1,0x95,0x0F);
    TILE(RF(sir_neg(a,SIR_DTFLOAT,F0)),    "f32.neg", 0x20,0,0x8C,0x0F);
    TILE(RD(sir_add(a,SIR_DTDOUBLE,D0,D1)),"f64.add", 0x20,0,0x20,1,0xA0,0x0F);
    TILE(RD(sir_sub(a,SIR_DTDOUBLE,D0,D1)),"f64.sub", 0x20,0,0x20,1,0xA1,0x0F);
    TILE(RD(sir_mul(a,SIR_DTDOUBLE,D0,D1)),"f64.mul", 0x20,0,0x20,1,0xA2,0x0F);
    TILE(RD(sir_div(a,SIR_DTDOUBLE,D0,D1)),"f64.div", 0x20,0,0x20,1,0xA3,0x0F);
    TILE(RD(sir_neg(a,SIR_DTDOUBLE,D0)),   "f64.neg", 0x20,0,0x9A,0x0F);

    /* ── §5.1 conversions (operand reduced first, then the convert op) ──── */
    TILE(RL(sir_i2_l(a,I0)), "i2l i64.extend_i32_s", 0x20,0,0xAC,0x0F);
    TILE(RF(sir_i2_f(a,I0)), "i2f f32.convert_i32_s",0x20,0,0xB2,0x0F);
    TILE(RD(sir_i2_d(a,I0)), "i2d f64.convert_i32_s",0x20,0,0xB7,0x0F);
    TILE(RI(sir_l2_i(a,L0)), "l2i i32.wrap_i64",     0x20,0,0xA7,0x0F);
    TILE(RF(sir_l2_f(a,L0)), "l2f f32.convert_i64_s",0x20,0,0xB4,0x0F);
    TILE(RD(sir_l2_d(a,L0)), "l2d f64.convert_i64_s",0x20,0,0xB9,0x0F);
    TILE(RD(sir_f2_d(a,F0)), "f2d f64.promote_f32",  0x20,0,0xBB,0x0F);
    TILE(RF(sir_d2_f(a,D0)), "d2f f32.demote_f64",   0x20,0,0xB6,0x0F);
    /* float→int = saturating trunc (JLS §5.1.3), 0xFC-prefixed */
    TILE(RI(sir_f2_i(a,F0)), "f2i trunc_sat i32 f32",0x20,0,0xFC,0x00,0x0F);
    TILE(RI(sir_d2_i(a,D0)), "d2i trunc_sat i32 f64",0x20,0,0xFC,0x02,0x0F);
    TILE(RL(sir_f2_l(a,F0)), "f2l trunc_sat i64 f32",0x20,0,0xFC,0x04,0x0F);
    TILE(RL(sir_d2_l(a,D0)), "d2l trunc_sat i64 f64",0x20,0,0xFC,0x06,0x0F);
    /* bit-preserving moves (typed bitcast) — same bits, reinterpreted across float/int domains */
    TILE(RI(sir_move_f2_i(a,F0)), "movef2i i32.reinterpret_f32", 0x20,0,0xBC,0x0F);
    TILE(RF(sir_move_i2_f(a,I0)), "movei2f f32.reinterpret_i32", 0x20,0,0xBE,0x0F);
    TILE(RL(sir_move_d2_l(a,D0)), "moved2l i64.reinterpret_f64", 0x20,0,0xBD,0x0F);
    TILE(RD(sir_move_l2_d(a,L0)), "movel2d f64.reinterpret_i64", 0x20,0,0xBF,0x0F);
    /* int-family narrowings */
    TILE(RI(sir_i2_b(a,I0)), "i2b i32.extend8_s",    0x20,0,0xC0,0x0F);
    TILE(RI(sir_i2_s(a,I0)), "i2s i32.extend16_s",   0x20,0,0xC1,0x0F);
    TILE(RI(sir_i2_c(a,I0)), "i2c mask 0xFFFF",       0x20,0,0x41,0xFF,0xFF,0x03,0x71,0x0F);
    TILE(RI(sir_s2_b(a,sir_load_local(a,0,SIR_DTSHORT, NULL))), "s2b i32.extend8_s", 0x20,0,0xC0,0x0F);
    TILE(RI(sir_s2_i(a,sir_load_local(a,0,SIR_DTSHORT, NULL))), "s2i no-op",         0x20,0,0x0F);

    /* ── spine rules ───────────────────────────────────────────────────── */
    TILE(sir_return_void(a), "return void", 0x0F);
    TILE(sir_store_local(a, 2, SIR_DTINT, NULL,  I0, NULL), "store_local i32", 0x20,0,0x21,2);
    TILE(sir_store_local(a, 2, SIR_DTLONG, NULL, L0, NULL), "store_local i64", 0x20,0,0x21,2);
    TILE(sir_store_local(a, 2, SIR_DTFLOAT, NULL,F0, NULL), "store_local f32", 0x20,0,0x21,2);
    TILE(sir_store_local(a, 2, SIR_DTDOUBLE, NULL,D0,NULL), "store_local f64", 0x20,0,0x21,2);

    /* ── comparisons: distinct CPS nodes, operand type from children, i32 result ─ */
    TILE(RI(sir_eq(a,I0,I1)), "i32.eq",   0x20,0,0x20,1,0x46,0x0F);
    TILE(RI(sir_ne(a,I0,I1)), "i32.ne",   0x20,0,0x20,1,0x47,0x0F);
    TILE(RI(sir_lt(a,I0,I1)), "i32.lt_s", 0x20,0,0x20,1,0x48,0x0F);
    TILE(RI(sir_le(a,I0,I1)), "i32.le_s", 0x20,0,0x20,1,0x4C,0x0F);
    TILE(RI(sir_gt(a,I0,I1)), "i32.gt_s", 0x20,0,0x20,1,0x4A,0x0F);
    TILE(RI(sir_ge(a,I0,I1)), "i32.ge_s", 0x20,0,0x20,1,0x4E,0x0F);
    TILE(RI(sir_eq(a,L0,L1)), "i64.eq",   0x20,0,0x20,1,0x51,0x0F);
    TILE(RI(sir_lt(a,L0,L1)), "i64.lt_s", 0x20,0,0x20,1,0x53,0x0F);
    TILE(RI(sir_eq(a,F0,F1)), "f32.eq",   0x20,0,0x20,1,0x5B,0x0F);
    TILE(RI(sir_lt(a,F0,F1)), "f32.lt",   0x20,0,0x20,1,0x5D,0x0F);
    TILE(RI(sir_eq(a,D0,D1)), "f64.eq",   0x20,0,0x20,1,0x61,0x0F);
    TILE(RI(sir_ge(a,D0,D1)), "f64.ge",   0x20,0,0x20,1,0x66,0x0F);

    /* ── LogNot = (x == 0) (i32.eqz) ───────────────────────────────────── */
    TILE(RI(sir_log_not(a,SIR_DTINT,I0)), "lognot i32.eqz", 0x20,0,0x45,0x0F);

    /* ── ExprEffect: evaluate, drop the result (void calls leave none) ──── */
    TILE(sir_expr_effect(a, I0, 0, NULL), "expr_effect drop",         0x20,0,0x1A);
    TILE(sir_expr_effect(a, I0, 1, NULL), "expr_effect void no-drop", 0x20,0);

    /* ── value-as-statement chain: burg_rewrite on a BARE value subtree tiles it
     *    with no extra bytes (the structured emit's cond-tiling mechanism). ──── */
    TILE(sir_lt(a, I0, I1), "bare cond → value bytes only", 0x20,0,0x20,1,0x48);
    TILE(sir_add(a, SIR_DTINT, I0, I1), "bare i32 value → value bytes only", 0x20,0,0x20,1,0x6A);

    /* ── Object model: New/GetField/PutField + LoadThis all resolve the class
     *    typeidx via the wasm_types layout (ctx->types, topologically remapped from
     *    class_id), so they are pinned end-to-end in test_codegen_object, not in
     *    this typeless TILE harness. (LoadThis now ref.casts `this` to its class —
     *    a typed op, so it can no longer be tiled without ctx->types.) ────────── */

    /* ── WASM v128 SIMD families — the node's `op` payload must reach the bytes
     *    (two DIFFERENT ops per family where the family has them; two lanes for
     *    the lane-immediate families; two constants for SimdConst). Sub-opcodes
     *    ≥ 0x80 are ULEB two-byte (0xAE → AE 01); relaxed rows are ≥ 0x100. ── */
    #define V0 sir_load_local(a, 0, SIR_DTV128, NULL)
    #define V1 sir_load_local(a, 1, SIR_DTV128, NULL)
    #define V2c sir_load_local(a, 2, SIR_DTV128, NULL)
    #define RV(t) sir_return(a, (t), SIR_DTV128)
    TILE(RV(sir_simd_bin(a, WOP_I32X4_ADD, V0, V1)), "i32x4.add",
         0x20,0, 0x20,1, 0xFD,0xAE,0x01, 0x0F);
    TILE(RV(sir_simd_bin(a, WOP_I32X4_SUB, V0, V1)), "i32x4.sub (payload reaches bytes)",
         0x20,0, 0x20,1, 0xFD,0xB1,0x01, 0x0F);
    TILE(RV(sir_simd_un(a, WOP_I32X4_NEG, V0)), "i32x4.neg",
         0x20,0, 0xFD,0xA1,0x01, 0x0F);
    TILE(RV(sir_simd_un(a, WOP_I32X4_ABS, V0)), "i32x4.abs",
         0x20,0, 0xFD,0xA0,0x01, 0x0F);
    TILE(RV(sir_simd_shift(a, WOP_I32X4_SHL, V0, I0)), "i32x4.shl",
         0x20,0, 0x20,0, 0xFD,0xAB,0x01, 0x0F);
    TILE(RV(sir_simd_shift(a, WOP_I32X4_SHR_S, V0, I0)), "i32x4.shr_s",
         0x20,0, 0x20,0, 0xFD,0xAC,0x01, 0x0F);
    TILE(RV(sir_simd_tern(a, WOP_V128_BITSELECT, V0, V1, V2c)), "v128.bitselect",
         0x20,0, 0x20,1, 0x20,2, 0xFD,0x52, 0x0F);
    TILE(RV(sir_simd_tern(a, WOP_F32X4_RELAXED_MADD, V0, V1, V2c)), "f32x4.relaxed_madd (2-byte uleb)",
         0x20,0, 0x20,1, 0x20,2, 0xFD,0x85,0x02, 0x0F);
    TILE(RI(sir_simd_test_i(a, WOP_V128_ANY_TRUE, V0)), "v128.any_true",
         0x20,0, 0xFD,0x53, 0x0F);
    TILE(RI(sir_simd_test_i(a, WOP_I32X4_ALL_TRUE, V0)), "i32x4.all_true",
         0x20,0, 0xFD,0xA3,0x01, 0x0F);
    TILE(RV(sir_simd_splat_i(a, WOP_I32X4_SPLAT, I0)), "i32x4.splat",
         0x20,0, 0xFD,0x11, 0x0F);
    TILE(RV(sir_simd_splat_l(a, WOP_I64X2_SPLAT, L0)), "i64x2.splat",
         0x20,0, 0xFD,0x12, 0x0F);
    TILE(RV(sir_simd_splat_f(a, WOP_F32X4_SPLAT, F0)), "f32x4.splat",
         0x20,0, 0xFD,0x13, 0x0F);
    TILE(RV(sir_simd_splat_d(a, WOP_F64X2_SPLAT, D0)), "f64x2.splat",
         0x20,0, 0xFD,0x14, 0x0F);
    TILE(RI(sir_simd_extract_i(a, WOP_I32X4_EXTRACT_LANE, 0, V0)), "i32x4.extract_lane 0",
         0x20,0, 0xFD,0x1B, 0x00, 0x0F);
    TILE(RI(sir_simd_extract_i(a, WOP_I32X4_EXTRACT_LANE, 3, V0)), "i32x4.extract_lane 3 (lane reaches bytes)",
         0x20,0, 0xFD,0x1B, 0x03, 0x0F);
    TILE(RL(sir_simd_extract_l(a, WOP_I64X2_EXTRACT_LANE, 1, V0)), "i64x2.extract_lane 1",
         0x20,0, 0xFD,0x1D, 0x01, 0x0F);
    TILE(RF(sir_simd_extract_f(a, WOP_F32X4_EXTRACT_LANE, 2, V0)), "f32x4.extract_lane 2",
         0x20,0, 0xFD,0x1F, 0x02, 0x0F);
    TILE(RD(sir_simd_extract_d(a, WOP_F64X2_EXTRACT_LANE, 1, V0)), "f64x2.extract_lane 1",
         0x20,0, 0xFD,0x21, 0x01, 0x0F);
    TILE(RV(sir_simd_replace_i(a, WOP_I32X4_REPLACE_LANE, 2, V0, I0)), "i32x4.replace_lane 2",
         0x20,0, 0x20,0, 0xFD,0x1C, 0x02, 0x0F);
    TILE(RV(sir_simd_replace_l(a, WOP_I64X2_REPLACE_LANE, 0, V0, L1)), "i64x2.replace_lane 0",
         0x20,0, 0x20,1, 0xFD,0x1E, 0x00, 0x0F);
    TILE(RV(sir_simd_replace_f(a, WOP_F32X4_REPLACE_LANE, 3, V0, F1)), "f32x4.replace_lane 3",
         0x20,0, 0x20,1, 0xFD,0x20, 0x03, 0x0F);
    TILE(RV(sir_simd_replace_d(a, WOP_F64X2_REPLACE_LANE, 1, V0, D1)), "f64x2.replace_lane 1",
         0x20,0, 0x20,1, 0xFD,0x22, 0x01, 0x0F);
    TILE(RV(sir_simd_const(a, 0x0807060504030201LL, 0x100F0E0D0C0B0A09LL)), "v128.const bytes 1..16 LE",
         0xFD,0x0C, 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                    0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10, 0x0F);
    TILE(RV(sir_simd_const(a, 0, 0)), "v128.const zero (different payload)",
         0xFD,0x0C, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0x0F);
    TILE(RV(sir_simd_shuffle(a, 0x0706050403020100LL, 0x0F0E0D0C0B0A0908LL, V0, V1)),
         "i8x16.shuffle identity mask",
         0x20,0, 0x20,1, 0xFD,0x0D, 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                                    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F, 0x0F);
    TILE(sir_store_local(a, 3, SIR_DTV128, NULL,
                         sir_simd_bin(a, WOP_I32X4_ADD, V0, V1), NULL),
         "v128 StoreLocal", 0x20,0, 0x20,1, 0xFD,0xAE,0x01, 0x21,3);
    /* A dead v128 VALUE kept for its effect (Click wraps it): value + drop.
     * The tile family is one-per-valtype — v128 was the missing sixth. */
    TILE(sir_expr_effect(a, sir_simd_bin(a, WOP_I32X4_ADD, V0, V1), 0, NULL),
         "v128 ExprEffect drops", 0x20,0, 0x20,1, 0xFD,0xAE,0x01, 0x1A);

    /* ── v128 linear-memory ops — memarg = (align, offset 0); the align byte
     * is the SIR payload (carried from the toml column), so each pin reads it
     * in the bytes. memlane adds the lane byte AFTER the memarg. ─────────── */
    TILE(RV(sir_simd_mem_load(a, WOP_V128_LOAD, 4, I0)), "v128.load align=4",
         0x20,0, 0xFD,0x00, 0x04,0x00, 0x0F);
    TILE(RV(sir_simd_mem_load(a, WOP_V128_LOAD8X8_S, 3, I0)), "v128.load8x8_s align=3",
         0x20,0, 0xFD,0x01, 0x03,0x00, 0x0F);
    TILE(RV(sir_simd_mem_load(a, WOP_V128_LOAD32_ZERO, 2, I0)), "v128.load32_zero align=2",
         0x20,0, 0xFD,0x5C, 0x02,0x00, 0x0F);
    TILE(RV(sir_simd_mem_load_lane(a, WOP_V128_LOAD8_LANE, 0, 5, I0, V1)),
         "v128.load8_lane align=0 lane=5 (lane reaches bytes)",
         0x20,0, 0x20,1, 0xFD,0x54, 0x00,0x00, 0x05, 0x0F);
    TILE(sir_simd_mem_store(a, WOP_V128_STORE, 4, I0, V1, NULL), "v128.store align=4",
         0x20,0, 0x20,1, 0xFD,0x0B, 0x04,0x00);
    TILE(sir_simd_mem_store_lane(a, WOP_V128_STORE16_LANE, 1, 7, I0, V1, NULL),
         "v128.store16_lane align=1 lane=7",
         0x20,0, 0x20,1, 0xFD,0x59, 0x01,0x00, 0x07);

    /* ── scalar linear-memory tiles + the memory admin ops ─────────────── */
    TILE(RI(sir_mem_load_i(a, WOP_I32_LOAD, 2, I0)), "i32.load align=2",
         0x20,0, 0x28, 0x02,0x00, 0x0F);
    TILE(RL(sir_mem_load_l(a, WOP_I64_LOAD32_S, 2, I0)), "i64.load32_s align=2 (op reaches bytes)",
         0x20,0, 0x34, 0x02,0x00, 0x0F);
    TILE(RD(sir_mem_load_d(a, WOP_F64_LOAD, 3, I0)), "f64.load align=3",
         0x20,0, 0x2B, 0x03,0x00, 0x0F);
    TILE(sir_mem_store_i(a, WOP_I32_STORE8, 0, I0, I1, NULL), "i32.store8 align=0",
         0x20,0, 0x20,1, 0x3A, 0x00,0x00);
    TILE(sir_mem_store_l(a, WOP_I64_STORE, 3, I0, L1, NULL), "i64.store align=3",
         0x20,0, 0x20,1, 0x37, 0x03,0x00);
    TILE(RI(sir_mem_size(a)), "memory.size",
         0x3F, 0x00, 0x0F);
    TILE(RI(sir_mem_grow(a, I0)), "memory.grow",
         0x20,0, 0x40, 0x00, 0x0F);
    TILE(sir_mem_fill(a, I0, I1, sir_load_local(a, 2, SIR_DTINT, NULL), NULL),
         "memory.fill (one memidx)",
         0x20,0, 0x20,1, 0x20,2, 0xFC,0x0B, 0x00);
    TILE(sir_mem_copy(a, I0, I1, sir_load_local(a, 2, SIR_DTINT, NULL), NULL),
         "memory.copy (two memidx)",
         0x20,0, 0x20,1, 0x20,2, 0xFC,0x0A, 0x00,0x00);
    #undef V0
    #undef V1
    #undef V2c
    #undef RV

    bbq_arena_free(&arena);
    return TEST_SUMMARY("test_codegen_wasm");
}
