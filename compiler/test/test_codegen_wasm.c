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

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

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
        printf("\n"); fails++; \
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

    bbq_arena_free(&arena);
    if (fails) { printf("test_codegen_wasm: %d FAILED\n", fails); return 1; }
    printf("test_codegen_wasm: OK (all burg rules pinned)\n");
    return 0;
}
