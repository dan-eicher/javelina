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

/* ── An independent minimum over the exported rules ────────────────────────────
 *
 * The labeler's whole claim is that its cover is the CHEAPEST the grammar admits.
 * Nothing inside the matcher can check that — the DP is the thing under test, and
 * the classic failure (an incomplete chain-rule closure) yields a cover that is
 * merely legal, not minimal. Once B2 and B3 add rules that genuinely COMPETE,
 * that claim is what decides the emitted bytes, so it needs a second opinion.
 *
 * So this searches the rule set directly, from the table burgc exports with
 * --emit-rule-table. It never calls burg_label/burg_dp: plain recursive
 * minimization, no memo table, no precomputed closures — deliberately the
 * opposite shape to the generated single-pass DP, so agreeing by coincidence
 * would take the same mistake made twice in two different algorithms.
 *
 * A pattern is a preorder walk, and it is matched in two passes because the ORDER
 * matters: shape first, then the guard, then the children's own minima. A
 * where-clause reads the node's payload union, so running one before its pattern
 * has matched would read the wrong variant's fields. (The labeler never does:
 * each guard sits inside its terminal's switch arm.)
 *
 * `take_max` runs the same search for the DEAREST cover instead of the cheapest.
 * It is not a second oracle — it is how the test reports whether the DP had any
 * choice to make at all. Where the two agree for every tree, the grammar admits
 * exactly one cover and "the labeler picks the minimum" is true but vacuous; the
 * count of trees where they differ is the number where minimality is doing real
 * work. B2 and B3 are what make that number climb. */
static int oracle_search(sir_node_t* n, int nt, burg_ctx_t* ctx, int fuel, int take_max);
static int oracle_best(sir_node_t* n, int nt, burg_ctx_t* ctx, int fuel) {
    return oracle_search(n, nt, ctx, fuel, 0);
}

static int oracle_shape(sir_node_t* n, const burg_pat_node_t* p, int* i) {
    const burg_pat_node_t* h = &p[(*i)++];
    if (!h->is_term) return 1;                    /* nonterminal leaf: any node */
    if ((int)BURG_NODE_OP(n) != h->sym || BURG_NODE_ARITY(n) < h->nkids) return 0;
    for (int k = 0; k < h->nkids; k++)
        if (!oracle_shape(BURG_NODE_CHILD(n, k), p, i)) return 0;
    return 1;
}

static int oracle_kids(sir_node_t* n, const burg_pat_node_t* p, int* i,
                       burg_ctx_t* ctx, int fuel, int* acc, int take_max) {
    const burg_pat_node_t* h = &p[(*i)++];
    if (!h->is_term) {
        int c = oracle_search(n, h->sym, ctx, fuel - 1, take_max);
        if (c < 0) return 0;
        *acc += c;
        return 1;
    }
    for (int k = 0; k < h->nkids; k++)
        if (!oracle_kids(BURG_NODE_CHILD(n, k), p, i, ctx, fuel, acc, take_max)) return 0;
    return 1;
}

static int oracle_search(sir_node_t* n, int nt, burg_ctx_t* ctx, int fuel, int take_max) {
    if (fuel <= 0) return -1;                     /* bounds chain-rule cycles */
    int best = -1;
    for (int r = 0; r < burg_rule_table_len; r++) {
        const burg_rule_row_t* row = &burg_rule_table[r];
        if (row->nonterm != nt) continue;
        int i = 0;
        if (!oracle_shape(n, row->pat, &i)) continue;
        if (!burg_rule_guard(row->rule, n, ctx)) continue;
        i = 0;
        int acc = 0;
        if (!oracle_kids(n, row->pat, &i, ctx, fuel, &acc, take_max)) continue;
        int c = row->cost + acc;
        if (best < 0 || (take_max ? c > best : c < best)) best = c;
    }
    return best;
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

    /* §20.11 Math.sqrt/floor/ceil/rint — the four direct-opcode intrinsics. The
     * grammar gives each its own rule, so each needs its own byte pin: a rule
     * naming WOP_F64_FLOOR where it meant WOP_F64_CEIL passes burgc --coverage. */
    TILE(RD(sir_f64_sqrt(a,D0)),    "f64.sqrt",    0x20,0,0x9F,0x0F);
    TILE(RD(sir_f64_floor(a,D0)),   "f64.floor",   0x20,0,0x9C,0x0F);
    TILE(RD(sir_f64_ceil(a,D0)),    "f64.ceil",    0x20,0,0x9B,0x0F);
    TILE(RD(sir_f64_nearest(a,D0)), "f64.nearest", 0x20,0,0x9E,0x0F);

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
    TILE(RI(sir_ge_u(a,I0,I1)), "i32.ge_u", 0x20,0,0x20,1,0x4F,0x0F);
    TILE(RI(sir_eq(a,L0,L1)), "i64.eq",   0x20,0,0x20,1,0x51,0x0F);
    TILE(RI(sir_lt(a,L0,L1)), "i64.lt_s", 0x20,0,0x20,1,0x53,0x0F);
    TILE(RI(sir_eq(a,F0,F1)), "f32.eq",   0x20,0,0x20,1,0x5B,0x0F);
    TILE(RI(sir_lt(a,F0,F1)), "f32.lt",   0x20,0,0x20,1,0x5D,0x0F);
    TILE(RI(sir_eq(a,D0,D1)), "f64.eq",   0x20,0,0x20,1,0x61,0x0F);
    TILE(RI(sir_ge(a,D0,D1)), "f64.ge",   0x20,0,0x20,1,0x66,0x0F);

    /* ── LogNot = (x == 0) (i32.eqz) ───────────────────────────────────── */
    TILE(RI(sir_log_not(a,SIR_DTINT,I0)), "lognot i32.eqz", 0x20,0,0x45,0x0F);

    /* ── compare against zero / null: the constant folds INTO the compare ──
     *
     * The first place in this grammar where two covers really compete, so these
     * pins are byte-exact on purpose: "contains an eqz" would pass even if the
     * elided constant were still being pushed, which is the whole saving.
     *
     * Each was falsified by removal on 2026-07-30 — deleting the rule pair drops
     * the cover back to the general compare and the pin goes red with the
     * `i32.const 0` (or `ref.null`) back in the byte string. Both operand orders
     * are pinned: the elided side emits nothing, so postorder leaves the stack
     * right either way, and only a pin proves that rather than assuming it. */
    #define Z   sir_load_const(a, 0, SIR_DTINT)
    #define Z64 sir_load_long_const(a, 0)
    #define NUL sir_load_null(a)
    #define A0  sir_load_local(a, 0, SIR_DTREF, NULL)
    /* x == 0 → local.get; i32.eqz   (was: local.get; i32.const 0; i32.eq) */
    TILE(RI(sir_eq(a, I0, Z)), "x == 0 → i32.eqz",          0x20,0, 0x45, 0x0F);
    TILE(RI(sir_eq(a, Z, I0)), "0 == x → i32.eqz (commuted)", 0x20,0, 0x45, 0x0F);
    /* x != 0 → eqz; eqz  (2 bytes, against i32.const 0 + i32.ne = 3) */
    TILE(RI(sir_ne(a, I0, Z)), "x != 0 → i32.eqz i32.eqz",  0x20,0, 0x45,0x45, 0x0F);
    TILE(RI(sir_ne(a, Z, I0)), "0 != x → i32.eqz i32.eqz (commuted)", 0x20,0, 0x45,0x45, 0x0F);
    /* the i64 twins: i64.eqz yields an i32, so the inversion is an i32.eqz */
    TILE(RI(sir_eq(a, L0, Z64)), "l == 0 → i64.eqz",        0x20,0, 0x50, 0x0F);
    TILE(RI(sir_eq(a, Z64, L0)), "0 == l → i64.eqz (commuted)", 0x20,0, 0x50, 0x0F);
    TILE(RI(sir_ne(a, L0, Z64)), "l != 0 → i64.eqz i32.eqz", 0x20,0, 0x50,0x45, 0x0F);
    TILE(RI(sir_ne(a, Z64, L0)), "0 != l → i64.eqz i32.eqz (commuted)", 0x20,0, 0x50,0x45, 0x0F);
    /* r == null → ref.is_null  (was: ref.null none; ref.eq) */
    TILE(RI(sir_eq(a, A0, NUL)), "r == null → ref.is_null", 0x20,0, 0xD1, 0x0F);
    TILE(RI(sir_eq(a, NUL, A0)), "null == r → ref.is_null (commuted)", 0x20,0, 0xD1, 0x0F);
    /* r != null → ref.is_null; i32.eqz  (was: ref.null none; ref.eq; i32.eqz) */
    TILE(RI(sir_ne(a, A0, NUL)), "r != null → ref.is_null i32.eqz", 0x20,0, 0xD1,0x45, 0x0F);
    TILE(RI(sir_ne(a, NUL, A0)), "null != r → ref.is_null i32.eqz (commuted)",
         0x20,0, 0xD1,0x45, 0x0F);
    /* ── the branch context: two goals, `cond` and `ncond` ─────────────────
     *
     * The structurer reduces a branch condition at whichever goal its site wants
     * — `cond` for the truth, `ncond` for the inverse — so both are pinned here,
     * each with its BYTES and its COST. The cost half is not decoration: the
     * structurer picks between the goals by comparing them, so a rule that emits
     * the right bytes at the wrong price makes the wrong site-level choice, and a
     * bytes-only pin would let that through.
     *
     * This replaces a pin on a `cond_neg` flag. Polarity is no longer something a
     * rule reports out of band; it is which goal covered the tree, so `GOAL_TILE`
     * asserts on the derivation itself and there is no flag left to leak.
     *
     * Falsified by removal on 2026-07-30 (see the STATUS block): dropping the
     * context rules leaves each condition on the `cond: i32` / `ncond: i32`
     * fallbacks, and every pin whose expectation is not exactly that goes red. */
    #define GOAL_TILE(tree, goal, want_cost, m, ...) do { \
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx); \
        sir_node_t* _t = (tree); \
        burg_state_t* _st = burg_label_root(_t, &ctx); \
        int _ok = _st && burg_rule(_st, (goal)); \
        CHECK(_ok, m " (a rule covers it at this goal)"); \
        if (_ok) { \
            CHECK(burg_cost(_st, (goal)) == (want_cost), m " (cost)"); \
            burg_reduce(_t, _st, (goal), &ctx); \
        } \
        const uint8_t want[] = { __VA_ARGS__ }; \
        if (!eq_code(&ctx, want, (int)sizeof want)) { \
            printf("  FAIL  %s\n    want:", m); \
            for (int i=0;i<(int)sizeof want;i++) printf(" %02X", want[i]); \
            printf("\n    got: "); \
            for (int i=0;i<(int)bbq_vec_len(ctx.emit.code);i++) printf(" %02X", ctx.emit.code[i]); \
            printf("\n"); TEST_FAILED(); \
        } \
        bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx); } while (0)

    /* A bare truth value: free as a condition, one eqz as its inverse. The two
     * costs here are what every site-level choice below is measured against
     * (local.get is 2, so cond=2 and ncond=3 for this operand). */
    GOAL_TILE(I0, cond_NT,  2, "if (x): the value IS the condition",  0x20,0);
    GOAL_TILE(I0, ncond_NT, 3, "if (!x): one eqz over it",            0x20,0, 0x45);
    /* `x != 0` needs no code at all; its inverse needs the eqz. */
    GOAL_TILE(sir_ne(a, I0, Z), cond_NT,  2, "cond(x != 0) → tile x, emit nothing", 0x20,0);
    GOAL_TILE(sir_ne(a, Z, I0), cond_NT,  2, "cond(0 != x) → commuted",             0x20,0);
    GOAL_TILE(sir_ne(a, I0, Z), ncond_NT, 3, "ncond(x != 0) → eqz",           0x20,0, 0x45);
    GOAL_TILE(sir_ne(a, Z, I0), ncond_NT, 3, "ncond(0 != x) → commuted",      0x20,0, 0x45);
    /* `x == 0` is the exact mirror — free at `ncond`, an eqz at `cond`. */
    GOAL_TILE(sir_eq(a, I0, Z), ncond_NT, 2, "ncond(x == 0) → tile x, emit nothing", 0x20,0);
    GOAL_TILE(sir_eq(a, Z, I0), ncond_NT, 2, "ncond(0 == x) → commuted",             0x20,0);
    GOAL_TILE(sir_eq(a, I0, Z), cond_NT,  3, "cond(x == 0) → eqz",            0x20,0, 0x45);
    GOAL_TILE(sir_eq(a, Z, I0), cond_NT,  3, "cond(0 == x) → commuted",       0x20,0, 0x45);
    /* i64 needs its eqz to reach an i32 truth value either way. */
    GOAL_TILE(sir_eq(a, L0, Z64), cond_NT,  3, "cond(l == 0) → i64.eqz",      0x20,0, 0x50);
    GOAL_TILE(sir_eq(a, Z64, L0), cond_NT,  3, "cond(0 == l) → commuted",     0x20,0, 0x50);
    GOAL_TILE(sir_eq(a, L0, Z64), ncond_NT, 4, "ncond(l == 0) → i64.eqz eqz", 0x20,0, 0x50,0x45);
    GOAL_TILE(sir_ne(a, L0, Z64), ncond_NT, 3, "ncond(l != 0) → i64.eqz",     0x20,0, 0x50);
    GOAL_TILE(sir_ne(a, Z64, L0), ncond_NT, 3, "ncond(0 != l) → commuted",    0x20,0, 0x50);
    GOAL_TILE(sir_ne(a, L0, Z64), cond_NT,  4, "cond(l != 0) → i64.eqz eqz",  0x20,0, 0x50,0x45);
    /* ref.is_null answers `== null` outright; `!= null` is it inverted. */
    GOAL_TILE(sir_eq(a, A0, NUL), cond_NT,  3, "cond(r == null) → ref.is_null",  0x20,0, 0xD1);
    GOAL_TILE(sir_eq(a, NUL, A0), cond_NT,  3, "cond(null == r) → commuted",     0x20,0, 0xD1);
    GOAL_TILE(sir_ne(a, A0, NUL), ncond_NT, 3, "ncond(r != null) → ref.is_null", 0x20,0, 0xD1);
    GOAL_TILE(sir_ne(a, NUL, A0), ncond_NT, 3, "ncond(null != r) → commuted",    0x20,0, 0xD1);

    /* ── float comparisons as conditions ───────────────────────────────────────
     * The i32/i64 zero cases above specialize to `eqz`; f64 has no f64.eqz, so a
     * float compare against zero must go through f64.const + f64.eq like any other
     * compare, and its INVERSE costs one more byte rather than being free. Both
     * polarities are pinned because `emit_spine` prices cond against ncond to decide
     * whether to exchange an if's arms, and an f64 condition that mis-costs would
     * silently swap arms at every float test.
     *
     * These are `ASCIIToBinaryBuffer.doubleValue`'s own conditions (`dValue == 0.0`,
     * `exp == 0`) — the method whose tail the backend emits twice, and which nothing
     * in this file covered. */
    GOAL_TILE(sir_eq(a, D0, D1), cond_NT,  5, "cond(d1 == d2) → f64.eq",
              0x20,0, 0x20,1, 0x61);
    GOAL_TILE(sir_eq(a, D0, D1), ncond_NT, 6, "ncond(d1 == d2) → f64.eq eqz",
              0x20,0, 0x20,1, 0x61, 0x45);
    GOAL_TILE(sir_ne(a, D0, D1), cond_NT,  5, "cond(d1 != d2) → f64.ne",
              0x20,0, 0x20,1, 0x62);
    GOAL_TILE(sir_ne(a, D0, D1), ncond_NT, 6, "ncond(d1 != d2) → f64.ne eqz",
              0x20,0, 0x20,1, 0x62, 0x45);
    GOAL_TILE(sir_eq(a, D0, sir_load_double_const(a, 0.0)), cond_NT, 12,
              "cond(d == 0.0) → no f64.eqz exists, so const+eq",
              0x20,0, 0x44,0,0,0,0,0,0,0,0, 0x61);
    GOAL_TILE(sir_eq(a, D0, sir_load_double_const(a, 0.0)), ncond_NT, 13,
              "ncond(d == 0.0) → const+eq+eqz",
              0x20,0, 0x44,0,0,0,0,0,0,0,0, 0x61, 0x45);
    GOAL_TILE(sir_ne(a, A0, NUL), cond_NT,  4, "cond(r != null) → is_null eqz",  0x20,0, 0xD1,0x45);
    GOAL_TILE(sir_eq(a, A0, NUL), ncond_NT, 4, "ncond(r == null) → is_null eqz", 0x20,0, 0xD1,0x45);
    /* A relational compare has no zero to fold: it materialises, and its inverse
     * is the generic one-eqz fallback. */
    GOAL_TILE(sir_lt(a, I0, I1), cond_NT,  5, "cond(x < y) is unaffected",
              0x20,0, 0x20,1, 0x48);
    GOAL_TILE(sir_lt(a, I0, I1), ncond_NT, 6, "ncond(x < y) → one eqz",
              0x20,0, 0x20,1, 0x48, 0x45);
    /* Negation is STRUCTURAL now: the truth of !e is the falsity of e, so the two
     * goals swap across a LogNot and a double negation cancels by derivation
     * rather than by a flag flipping twice. The DDCG never leaves a LogNot at a
     * branch (it swaps destinations upstream), so these rules fire ZERO times
     * compiling the JRE — which is exactly why they need hand-built fixtures: no
     * Java source in the corpus can reach them. */
    GOAL_TILE(sir_log_not(a, SIR_DTINT, I0), ncond_NT, 2, "ncond(!x) = cond(x)", 0x20,0);
    GOAL_TILE(sir_log_not(a, SIR_DTINT, I0), cond_NT,  3, "cond(!x) = ncond(x)", 0x20,0, 0x45);
    GOAL_TILE(sir_log_not(a, SIR_DTINT, sir_log_not(a, SIR_DTINT, I0)), cond_NT, 2,
              "cond(!!x) → two negations cancel by derivation", 0x20,0);
    GOAL_TILE(sir_log_not(a, SIR_DTINT, sir_eq(a, I0, Z)), cond_NT, 2,
              "cond(!(x == 0)) → the goals compose", 0x20,0);
    #undef GOAL_TILE

    /* ── identity elision and constant folding ─────────────────────────────
     *
     * Falsified by removal on 2026-07-30: dropping the identity rules puts the
     * constant and its opcode back in every byte string below; dropping the fold
     * rules puts both operands and the opcode back. The NON-identity and
     * refused-fold pins are the other half — they are what stops a rule that
     * "works" from also eliding `0 - x` or folding a division by zero. */
    #define ONE   sir_load_const(a, 1, SIR_DTINT)
    #define M1    sir_load_const(a, -1, SIR_DTINT)
    #define K(v)  sir_load_const(a, (v), SIR_DTINT)
    #define KL(v) sir_load_long_const(a, (v))
    #define LONE  sir_load_long_const(a, 1)

    /* An operator with its identity element emits the operand and nothing else. */
    TILE(RI(sir_add(a,SIR_DTINT,I0,Z)),  "x + 0 → x",   0x20,0, 0x0F);
    TILE(RI(sir_add(a,SIR_DTINT,Z,I0)),  "0 + x → x",   0x20,0, 0x0F);
    TILE(RI(sir_sub(a,SIR_DTINT,I0,Z)),  "x - 0 → x",   0x20,0, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,ONE)),"x * 1 → x",   0x20,0, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,ONE,I0)),"1 * x → x",   0x20,0, 0x0F);
    TILE(RI(sir_div(a,SIR_DTINT,I0,ONE)),"x / 1 → x",   0x20,0, 0x0F);
    TILE(RI(sir_or (a,SIR_DTINT,I0,Z)),  "x | 0 → x",   0x20,0, 0x0F);
    TILE(RI(sir_or (a,SIR_DTINT,Z,I0)),  "0 | x → x",   0x20,0, 0x0F);
    TILE(RI(sir_xor(a,SIR_DTINT,I0,Z)),  "x ^ 0 → x",   0x20,0, 0x0F);
    TILE(RI(sir_and(a,SIR_DTINT,I0,M1)), "x & -1 → x",  0x20,0, 0x0F);
    TILE(RI(sir_and(a,SIR_DTINT,M1,I0)), "-1 & x → x",  0x20,0, 0x0F);
    TILE(RI(sir_shl(a,SIR_DTINT,I0,Z)),  "x << 0 → x",  0x20,0, 0x0F);
    TILE(RI(sir_shr(a,SIR_DTINT,I0,Z)),  "x >> 0 → x",  0x20,0, 0x0F);
    TILE(RI(sir_ushr(a,SIR_DTINT,I0,Z)), "x >>> 0 → x", 0x20,0, 0x0F);
    TILE(RL(sir_add(a,SIR_DTLONG,L0,Z64)),  "l + 0 → l",  0x20,0, 0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,L0,LONE)), "l * 1 → l",  0x20,0, 0x0F);
    TILE(RL(sir_and(a,SIR_DTLONG,L0,KL(-1))), "l & -1 → l", 0x20,0, 0x0F);

    /* §15.19 types a shift COUNT as int at every operand width, and cg_promote
     * widens it, so a long shift's count is I2L(LoadConst) — NEVER a
     * LoadLongConst. The first fixture here used to build the long-const shape
     * by hand, which kept a pattern-dead grammar rule green while shipped code
     * carried `>>> 0` to the wasm (found by tier-3's rule counters:
     * shru64_zero fired on RandomAccessFile's byte ladders while shru32_zero
     * stayed silent). These pin the shape the frontend actually builds. */
    TILE(RL(sir_shl (a,SIR_DTLONG,L0,sir_i2_l(a,Z))), "l << I2L(0) → l",  0x20,0, 0x0F);
    TILE(RL(sir_shr (a,SIR_DTLONG,L0,sir_i2_l(a,Z))), "l >> I2L(0) → l",  0x20,0, 0x0F);
    TILE(RL(sir_ushr(a,SIR_DTLONG,L0,sir_i2_l(a,Z))), "l >>> I2L(0) → l", 0x20,0, 0x0F);
    /* …and a widened CONSTANT is one i64.const, not a const plus an extend. */
    TILE(RL(sir_i2_l(a,K(5))),  "i2l(const 5) → i64.const 5",   0x42,0x05, 0x0F);
    TILE(RL(sir_i2_l(a,K(-1))), "i2l(const -1) sign-extends",   0x42,0x7F, 0x0F);
    /* A nonzero count keeps its shift — the identity must not overreach. */
    TILE(RL(sir_shl(a,SIR_DTLONG,L0,sir_i2_l(a,K(1)))), "l << I2L(1) keeps its shift",
         0x20,0, 0x42,0x01, 0x86, 0x0F);
    /* Both sides known: the answer, through the I2L-wrapped count shape. */
    TILE(RL(sir_ushr(a,SIR_DTLONG,KL(5),sir_i2_l(a,K(1)))), "5L >>> I2L(1) → const 2",
         0x42,0x02, 0x0F);

    /* …and the orders that are NOT identities keep their operator. Without these
     * a rule written for the commutative case would quietly break subtraction. */
    TILE(RI(sir_sub(a,SIR_DTINT,Z,I0)),  "0 - x is NOT x", 0x41,0x00, 0x20,0, 0x6B, 0x0F);
    TILE(RI(sir_div(a,SIR_DTINT,ONE,I0)),"1 / x is NOT x", 0x41,0x01, 0x20,0, 0x6D, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,Z)),  "x * 0 keeps its multiply", 0x20,0, 0x41,0x00, 0x6C, 0x0F);
    TILE(RI(sir_and(a,SIR_DTINT,I0,Z)),  "x & 0 keeps its and",      0x20,0, 0x41,0x00, 0x71, 0x0F);

    /* Both operands constant: the answer, not the recipe. */
    TILE(RI(sir_add(a,SIR_DTINT,K(3),K(4))),  "3 + 4 → const 7",   0x41,0x07, 0x0F);
    TILE(RI(sir_sub(a,SIR_DTINT,K(3),K(4))),  "3 - 4 → const -1",  0x41,0x7F, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,K(3),K(4))),  "3 * 4 → const 12",  0x41,0x0C, 0x0F);
    TILE(RI(sir_div(a,SIR_DTINT,K(7),K(2))),  "7 / 2 → const 3",   0x41,0x03, 0x0F);
    TILE(RI(sir_rem(a,SIR_DTINT,K(7),K(2))),  "7 % 2 → const 1",   0x41,0x01, 0x0F);
    TILE(RI(sir_and(a,SIR_DTINT,K(6),K(3))),  "6 & 3 → const 2",   0x41,0x02, 0x0F);
    TILE(RI(sir_or (a,SIR_DTINT,K(6),K(3))),  "6 | 3 → const 7",   0x41,0x07, 0x0F);
    TILE(RI(sir_xor(a,SIR_DTINT,K(6),K(3))),  "6 ^ 3 → const 5",   0x41,0x05, 0x0F);
    TILE(RI(sir_shl(a,SIR_DTINT,K(1),K(3))),  "1 << 3 → const 8",  0x41,0x08, 0x0F);
    TILE(RI(sir_shr(a,SIR_DTINT,K(-8),K(1))), "-8 >> 1 → const -4",0x41,0x7C, 0x0F);
    TILE(RI(sir_ushr(a,SIR_DTINT,K(-8),K(28))),"-8 >>> 28 → const 15", 0x41,0x0F, 0x0F);
    TILE(RI(sir_neg(a,SIR_DTINT,K(5))),       "-(5) → const -5",   0x41,0x7B, 0x0F);
    TILE(RL(sir_add(a,SIR_DTLONG,KL(3),KL(4))), "3L + 4L → const 7L", 0x42,0x07, 0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,KL(3),KL(4))), "3L * 4L → const 12L",0x42,0x0C, 0x0F);
    TILE(RL(sir_neg(a,SIR_DTLONG,KL(5))),       "-(5L) → const -5L",  0x42,0x7B, 0x0F);

    /* §15.19: a shift count is MASKED to the operand's width, so folding must mask
     * too. `1 << 32` is 1, not 0 — a fold that used C's shift directly would be
     * undefined here and would disagree with the engine either way. */
    TILE(RI(sir_shl(a,SIR_DTINT,K(1),K(32))),  "1 << 32 → const 1 (count masked)", 0x41,0x01, 0x0F);
    TILE(RL(sir_shl(a,SIR_DTLONG,KL(1),sir_i2_l(a,K(64)))),"1L << 64 → const 1L (masked)", 0x42,0x01, 0x0F);

    /* §15.17.2/§15.17.3: division by zero THROWS at run time, and MIN/-1 overflows
     * rather than trapping. Folding either would move a runtime exception to
     * compile time or compute UB, so both must be refused and emit as usual. */
    TILE(RI(sir_div(a,SIR_DTINT,K(7),Z)), "7 / 0 does NOT fold",
         0x41,0x07, 0x41,0x00, 0x6D, 0x0F);
    TILE(RI(sir_rem(a,SIR_DTINT,K(7),Z)), "7 % 0 does NOT fold",
         0x41,0x07, 0x41,0x00, 0x6F, 0x0F);
    TILE(RI(sir_div(a,SIR_DTINT,K(INT32_MIN),M1)), "INT_MIN / -1 does NOT fold",
         0x41,0x80,0x80,0x80,0x80,0x78, 0x41,0x7F, 0x6D, 0x0F);
    TILE(RL(sir_div(a,SIR_DTLONG,KL(7),KL(0))), "7L / 0L does NOT fold",
         0x42,0x07, 0x42,0x00, 0x7F, 0x0F);

    /* ── strength reduction: ×2^k is a shift ───────────────────────────────
     *
     * §15.17.1 multiplication is two's-complement wraparound, so x·2^k ≡ x<<k
     * for the POSITIVE powers of two. The family is k in [2, 2^30]: ×1 is the
     * identity rule's (cost 0), and negative constants — including MIN_VALUE —
     * are not powers of two as int values, so they keep their multiply. */
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(8))),  "x * 8 → x << 3",  0x20,0, 0x41,0x03, 0x74, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,K(8),I0)),  "8 * x → x << 3",  0x20,0, 0x41,0x03, 0x74, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(2))),  "x * 2 → x << 1",  0x20,0, 0x41,0x01, 0x74, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(1<<30))), "x * 2^30 → x << 30", 0x20,0, 0x41,0x1E, 0x74, 0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,L0,KL(16))), "l * 16L → l << 4", 0x20,0, 0x42,0x04, 0x86, 0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,KL(16),L0)), "16L * l → l << 4", 0x20,0, 0x42,0x04, 0x86, 0x0F);

    /* …and the constants NOT in the family keep their multiply. */
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(6))),  "x * 6 keeps its multiply",  0x20,0, 0x41,0x06, 0x6C, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(-8))), "x * -8 keeps its multiply", 0x20,0, 0x41,0x78, 0x6C, 0x0F);
    TILE(RI(sir_mul(a,SIR_DTINT,I0,K(INT32_MIN))), "x * MIN keeps its multiply",
         0x20,0, 0x41,0x80,0x80,0x80,0x80,0x78, 0x6C, 0x0F);
    TILE(RL(sir_mul(a,SIR_DTLONG,L0,KL(6))), "l * 6L keeps its multiply", 0x20,0, 0x42,0x06, 0x7E, 0x0F);

    /* ── analysis-gated division: /2^k is shr_s when the dividend is ≥ 0 ───
     *
     * §15.17.2 division truncates toward zero; for a non-negative dividend
     * truncation and flooring agree, so x/2^k ≡ x>>k (arithmetic). The sign is
     * not in the tree — it is the optimizer's published §8.1.1 fact, installed
     * on the burg context and read by the rule's where-guard. No fact strip, a
     * fact that admits negatives, or a divisor off the 2^k family = the plain
     * division. FTILE tiles with a one-row strip on the DIVIDEND node. */
    #define FTILE(dvd, tree, st, lo_, val_, m, ...) do { \
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx); \
        compiler_click_vfact_t vrow; memset(&vrow, 0, sizeof vrow); \
        vrow.constant.state = (st); vrow.constant.cwidth = CP_W_I32; \
        vrow.constant.lo = (lo_); vrow.constant.hi = INT32_MAX; \
        vrow.constant.value = (val_); \
        struct compiler_click_facts f; memset(&f, 0, sizeof f); \
        f.computed = true; f.vnode_count = 1; f.v = &vrow; \
        bbq_hmap_init(&f.expr_idx.map, 0); \
        bbq_hmap_put(&f.expr_idx.map, (uint64_t)(uintptr_t)(dvd), (void*)1); \
        ctx.facts = &f; \
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
        bbq_hmap_free(&f.expr_idx.map); \
        bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx); } while (0)

    { sir_node_t* x = I0;
      FTILE(x, RI(sir_div(a,SIR_DTINT,x,K(8))), CP_C_RANGE, 0, 0,
            "x pub >= 0: x / 8 -> x >> 3 (shr_s)", 0x20,0, 0x41,0x03, 0x75, 0x0F); }
    { sir_node_t* x = I0;
      FTILE(x, RI(sir_div(a,SIR_DTINT,x,K(8))), CP_C_KNOWN, 0, 5,
            "x pub KNOWN 5: x / 8 -> x >> 3 (shr_s)", 0x20,0, 0x41,0x03, 0x75, 0x0F); }
    { sir_node_t* x = I0;
      FTILE(x, RI(sir_div(a,SIR_DTINT,x,K(8))), CP_C_RANGE, -1, 0,
            "x pub lo=-1: x / 8 keeps div_s", 0x20,0, 0x41,0x08, 0x6D, 0x0F); }
    { sir_node_t* x = I0;
      FTILE(x, RI(sir_div(a,SIR_DTINT,x,K(6))), CP_C_RANGE, 0, 0,
            "x pub >= 0 but 6 is not 2^k: div_s stays", 0x20,0, 0x41,0x06, 0x6D, 0x0F); }
    TILE(RI(sir_div(a,SIR_DTINT,I0,K(8))), "no facts: x / 8 keeps div_s",
         0x20,0, 0x41,0x08, 0x6D, 0x0F);
    #undef FTILE

    #undef ONE
    #undef M1
    #undef K
    #undef KL
    #undef LONE

    /* A condition derivation is NOT a value derivation, and no cost can make it one.
     *
     * `ncond` emits the INVERSE of its condition outright, and `cond` is reached
     * only where a caller asked for a truth test; a value derivation is neither.
     * So a chain rule from either goal to stmt would assert they are
     * interchangeable, which is false, and a large cost on that chain only hides
     * the assertion: cost ranks covers that ARE interchangeable, it cannot encode
     * "never pick this", because there is always an input deep enough to pay the
     * number off.
     *
     * That input is this one. The negation rules compose — `cond: LogNot(ncond)`
     * and its mirror alternate at 0 each — so every nesting level leaves the
     * polarity derivation flat while the i32 derivation grows by the byte its
     * i32.eqz costs. With a `stmt: cond = 500` chain in the grammar the cond path
     * won a plain VALUE tiling at 101 nested negations, eliding every eqz and
     * yielding the negation of the program's value, still validating as an i32.
     *
     * The min-cost search was not at fault; it found the cheapest cover of the
     * rules it was given, which is the entire point of it. The rule set was wrong.
     * The optimality oracle above therefore cannot catch this — it agrees with the
     * labeler — so the check has to be on the emitted BYTES at a value site.
     *
     * The tree is tiled BARE — no Return wrapper. That matters: `stmt: Return(i32)`
     * demands i32 of its child, so a wrapped fixture never consults a stmt chain at
     * all and would pass no matter what. A bare value subtree reaching stmt through
     * the chains is exactly what the structurer's emit_value does. */
    {
        sir_node_t* deep = I0;
        for (int i = 0; i < 128; i++) deep = sir_log_not(a, SIR_DTINT, deep);
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx);
        burg_rewrite(deep, &ctx);
        int eqz = 0;
        for (int i = 0; i < (int)bbq_vec_len(ctx.emit.code); i++)
            if (ctx.emit.code[i] == 0x45) eqz++;
        CHECK(!burg_has_error(&ctx), "deep negation: tiled");
        if (eqz != 128) {
            printf("  FAIL  a value tiling took a `cond` cover: 128 negations emitted "
                   "%d of the 128 i32.eqz they mean\n", eqz);
            TEST_FAILED();
        }
        bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx);
    }

    /* A NON-zero constant must NOT take the folded cover — the guard is what
     * separates them, and without this the rules could match every compare. */
    TILE(RI(sir_eq(a, I0, sir_load_const(a, 1, SIR_DTINT))), "x == 1 stays a general compare",
         0x20,0, 0x41,0x01, 0x46, 0x0F);
    /* Both sides constant-zero: whichever order wins, the surviving side is
     * materialised and the result is still (0 == 0) = 1. */
    TILE(RI(sir_eq(a, Z, Z)), "0 == 0 folds one side only", 0x41,0x00, 0x45, 0x0F);

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

    /* ══ costs are BYTES ═══════════════════════════════════════════════════
     *
     * A rule's cost is a CLAIM: "covering this node adds N bytes to the output."
     * The DP sums those claims; burg_cost reads the total for the cover it chose at
     * the root. The emitter writes the actual bytes. Nothing connects the two — the
     * labeler never emits, the actions never cost — so asserting they are EQUAL is
     * what anchors every cost in the grammar to something other than feel. Without
     * it a cost is a number someone once guessed, and the min-cost DP is deciding
     * between fictions.
     *
     * SCOPE. The identity holds for a FIXED-EMISSION rule: one whose action writes
     * the same number of bytes every time it fires. Three families in this grammar
     * are not fixed-emission and are excluded here, each for a stated reason:
     *
     *   - the SIMD families, whose whole design is ONE tile per family with the
     *     WOP_* value as the node's payload. The payload's encoding is 2 bytes for
     *     a sub-opcode < 0x80, 3 for the rest, 4 for the relaxed rows — 300-odd
     *     widths behind one rule. Splitting per opcode would undo the design; the
     *     cost is the minimum (0xFD + a one-byte sub-opcode) and is documented as
     *     a floor in the grammar, not asserted here.
     *   - the ctx->types families (field / static / array / object / invoke),
     *     whose immediates are typeidxs and global indices from the layout
     *     authority. Their emitted width is whatever the real program's LEB comes
     *     out at — in the jre, typeidxs run to three digits — so cost == bytes is
     *     false for them BY CONSTRUCTION, not merely untested. There is also no
     *     small-index wasm_types_t to anchor them against: wasm_types_class_typeidx
     *     falls back to the raw class_id only for an out-of-range class, and
     *     wasm_types_field_index then dereferences wt->sema for the same class.
     *     Their bytes are pinned end-to-end in test_codegen_object.c instead.
     *   - the LOCAL families (LoadLocal / StoreLocal / Inc), for the same reason
     *     and previously missed: the slot is a ULEB, so the access is two bytes
     *     up to slot 127 and three beyond it, and their declared costs are the
     *     small-slot figure. nbody's energy() reaches slot 160 at -O0, so this is
     *     an ordinary program's range, not a corner. Their exact bytes ARE pinned
     *     — by the TILE fixtures above, at the slots those fixtures name.
     *
     * The point of the exclusions is that this oracle checks a narrow, real
     * property: that a rule which always emits N bytes declares N. It is NOT a
     * claim that the cost model measures code size. A BURS cost is an abstract
     * weight, and this grammar's happen to be authored in byte units; retuning
     * them to measured op costs would be a legitimate change that this oracle
     * must not stand in the way of.
     *
     * The value-as-statement chains are ON the identity (cost 0 = the nothing
     * they emit) — the bare-value fixtures below exercise them, and they matter:
     * every variable-arity call argument is priced through cost[stmt], so a
     * chain that lied would put its lie into every call's cost.
     *
     * Everything else is here. A fixture that fails names its rule family. */
    #define COST_IS_BYTES(tree, m) do { \
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx); \
        sir_node_t* _t = (tree); \
        burg_state_t* _st = burg_label_root(_t, &ctx); \
        int _claim = (_st && burg_rule(_st, stmt_NT)) ? burg_cost(_st, stmt_NT) : -1; \
        burg_rewrite(_t, &ctx); \
        int _bytes = (int)bbq_vec_len(ctx.emit.code); \
        CHECK(!burg_has_error(&ctx), m " (no burg error)"); \
        if (_claim != _bytes) { \
            printf("  FAIL  cost != bytes: %s — the cover claims %d, the emitter wrote %d\n", \
                   m, _claim, _bytes); \
            TEST_FAILED(); \
        } \
        bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx); } while (0)

    /* Spine-rooted fixtures reduce to stmt through a real spine rule; the
     * bare-value ones further down reduce through the 0-cost chains. Immediates
     * are minimal-LEB (slots and small constants), which is the fixture the
     * costs are defined at. */

    /* constants — the literal's own encoding is the cost (f32 4 bytes, f64 8). */
    COST_IS_BYTES(RI(sir_load_const(a, 5, SIR_DTINT)),  "i32.const");
    COST_IS_BYTES(RL(sir_load_long_const(a, 5)),        "i64.const");
    COST_IS_BYTES(RF(sir_load_float_const(a, 1.5f)),    "f32.const");
    COST_IS_BYTES(RD(sir_load_double_const(a, 2.25)),   "f64.const");
    COST_IS_BYTES(sir_return(a, sir_load_null(a), SIR_DTREF), "ref.null none");

    /* locals are EXCLUDED — see the exclusion list above. The slot is a ULEB, so
     * these rules are not fixed-emission and the oracle's property does not apply
     * to them. The TILE fixtures pin their exact bytes at the slots they name. */

    /* arithmetic / bitwise / shifts — one opcode each, and the two synthesized
     * negations (no i32/i64.neg in WASM: x * -1). */
    COST_IS_BYTES(RI(sir_add(a,SIR_DTINT,I0,I1)),  "i32.add");
    COST_IS_BYTES(RI(sir_sub(a,SIR_DTINT,I0,I1)),  "i32.sub");
    COST_IS_BYTES(RI(sir_mul(a,SIR_DTINT,I0,I1)),  "i32.mul");
    COST_IS_BYTES(RI(sir_div(a,SIR_DTINT,I0,I1)),  "i32.div_s");
    COST_IS_BYTES(RI(sir_rem(a,SIR_DTINT,I0,I1)),  "i32.rem_s");
    COST_IS_BYTES(RI(sir_and(a,SIR_DTINT,I0,I1)),  "i32.and");
    COST_IS_BYTES(RI(sir_or (a,SIR_DTINT,I0,I1)),  "i32.or");
    COST_IS_BYTES(RI(sir_xor(a,SIR_DTINT,I0,I1)),  "i32.xor");
    COST_IS_BYTES(RI(sir_shl(a,SIR_DTINT,I0,I1)),  "i32.shl");
    COST_IS_BYTES(RI(sir_shr(a,SIR_DTINT,I0,I1)),  "i32.shr_s");
    COST_IS_BYTES(RI(sir_ushr(a,SIR_DTINT,I0,I1)), "i32.shr_u");
    COST_IS_BYTES(RI(sir_neg(a,SIR_DTINT,I0)),     "i32 neg (const -1 + mul)");
    COST_IS_BYTES(RL(sir_add(a,SIR_DTLONG,L0,L1)), "i64.add");
    COST_IS_BYTES(RL(sir_shl(a,SIR_DTLONG,L0,L1)), "i64.shl");
    COST_IS_BYTES(RL(sir_neg(a,SIR_DTLONG,L0)),    "i64 neg (const -1 + mul)");
    COST_IS_BYTES(RF(sir_add(a,SIR_DTFLOAT,F0,F1)),  "f32.add");
    COST_IS_BYTES(RF(sir_neg(a,SIR_DTFLOAT,F0)),     "f32.neg");
    COST_IS_BYTES(RD(sir_add(a,SIR_DTDOUBLE,D0,D1)), "f64.add");
    COST_IS_BYTES(RD(sir_neg(a,SIR_DTDOUBLE,D0)),    "f64.neg");

    /* comparisons — every operand type, plus ref identity (ref.eq, and != as
     * ref.eq + i32.eqz, the one two-opcode compare). */
    COST_IS_BYTES(RI(sir_eq(a,I0,I1)), "i32.eq");
    COST_IS_BYTES(RI(sir_lt(a,I0,I1)), "i32.lt_s");
    COST_IS_BYTES(RI(sir_eq(a,L0,L1)), "i64.eq");
    COST_IS_BYTES(RI(sir_eq(a,F0,F1)), "f32.eq");
    COST_IS_BYTES(RI(sir_ge(a,D0,D1)), "f64.ge");
    COST_IS_BYTES(RI(sir_eq(a, sir_load_null(a), sir_load_null(a))), "ref.eq");
    COST_IS_BYTES(RI(sir_ne(a, sir_load_null(a), sir_load_null(a))), "ref != (ref.eq + eqz)");
    COST_IS_BYTES(RI(sir_log_not(a,SIR_DTINT,I0)), "lognot (i32.eqz)");

    /* §5.1 conversions, the bit-preserving moves, the f64 intrinsics, and the
     * int-family narrowings — I2C is the one that carries a literal mask. */
    COST_IS_BYTES(RL(sir_i2_l(a,I0)), "i2l");
    COST_IS_BYTES(RF(sir_i2_f(a,I0)), "i2f");
    COST_IS_BYTES(RD(sir_i2_d(a,I0)), "i2d");
    COST_IS_BYTES(RI(sir_l2_i(a,L0)), "l2i");
    COST_IS_BYTES(RD(sir_f2_d(a,F0)), "f2d");
    COST_IS_BYTES(RF(sir_d2_f(a,D0)), "d2f");
    COST_IS_BYTES(RI(sir_f2_i(a,F0)), "f2i (trunc_sat, 0xFC-prefixed)");
    COST_IS_BYTES(RL(sir_d2_l(a,D0)), "d2l (trunc_sat, 0xFC-prefixed)");
    COST_IS_BYTES(RI(sir_move_f2_i(a,F0)), "movef2i");
    COST_IS_BYTES(RD(sir_move_l2_d(a,L0)), "movel2d");
    COST_IS_BYTES(RD(sir_f64_sqrt(a,D0)),  "f64.sqrt");
    COST_IS_BYTES(RD(sir_f64_floor(a,D0)), "f64.floor");
    COST_IS_BYTES(RI(sir_i2_b(a,I0)), "i2b");
    COST_IS_BYTES(RI(sir_i2_s(a,I0)), "i2s");
    COST_IS_BYTES(RI(sir_i2_c(a,I0)), "i2c (const 0xFFFF + and)");
    COST_IS_BYTES(RI(sir_s2_b(a,sir_load_local(a,0,SIR_DTSHORT,NULL))), "s2b");
    COST_IS_BYTES(RI(sir_s2_i(a,sir_load_local(a,0,SIR_DTSHORT,NULL))), "s2i (no-op, cost 0)");

    /* spine terminators and the label anchor. */
    COST_IS_BYTES(sir_return_void(a), "return void");
    COST_IS_BYTES(sir_throw(a, sir_load_null(a)), "throw (tag 0)");
    COST_IS_BYTES(sir_nop(a, NULL), "nop (label anchor, cost 0)");

    /* linear memory — the scalar loads/stores carry (align, offset) memargs, both
     * one byte here; memory.size/grow one memidx, fill one, copy two. */
    COST_IS_BYTES(RI(sir_mem_load_i(a, WOP_I32_LOAD, 2, I0)), "i32.load");
    COST_IS_BYTES(RD(sir_mem_load_d(a, WOP_F64_LOAD, 3, I0)), "f64.load");
    COST_IS_BYTES(sir_mem_store_i(a, WOP_I32_STORE8, 0, I0, I1, NULL), "i32.store8");
    COST_IS_BYTES(sir_mem_store_l(a, WOP_I64_STORE, 3, I0, L1, NULL), "i64.store");
    COST_IS_BYTES(RI(sir_mem_size(a)), "memory.size");
    COST_IS_BYTES(RI(sir_mem_grow(a, I0)), "memory.grow");
    COST_IS_BYTES(sir_mem_fill(a, I0, I1, sir_load_local(a,2,SIR_DTINT,NULL), NULL), "memory.fill");
    COST_IS_BYTES(sir_mem_copy(a, I0, I1, sir_load_local(a,2,SIR_DTINT,NULL), NULL), "memory.copy");

    /* array.len is the one array op with no typeidx immediate, so it belongs on the
     * identity even though the rest of the array family cannot. */
    COST_IS_BYTES(RI(sir_array_length(a, sir_load_null(a))), "array.len");

    /* The value-as-statement chains, ON the identity. A bare value tiled toward
     * stmt emits the value's bytes and nothing else, so the chains cost 0 — and
     * that zero is load-bearing, because every variable-arity call argument is
     * priced through cost[stmt] (iburg p.4: each child contributes its cost at
     * the nonterminal it reduces with). These are BARE trees, not spine-rooted:
     * that is what makes the chain the rule under test.
     *
     * (These replaced the 400-series "last resort" sentinels, which made the
     * chains the only rules in the grammar that lied about their bytes and would
     * have put a 400-byte fiction into every call argument's price. A sentinel
     * was never needed: no terminal here has both a direct stmt rule and a value
     * rule, so the chains are never contested — and a contested chain should be
     * decided on real bytes anyway, which 0 is.) */
    COST_IS_BYTES(sir_add(a, SIR_DTINT, I0, I1),    "stmt: i32 chain (bare value)");
    COST_IS_BYTES(sir_add(a, SIR_DTLONG, L0, L1),   "stmt: i64 chain (bare value)");
    COST_IS_BYTES(sir_add(a, SIR_DTFLOAT, F0, F1),  "stmt: f32 chain (bare value)");
    COST_IS_BYTES(sir_add(a, SIR_DTDOUBLE, D0, D1), "stmt: f64 chain (bare value)");
    COST_IS_BYTES(sir_load_null(a),                 "stmt: ref chain (bare value)");
    COST_IS_BYTES(sir_load_local(a, 0, SIR_DTV128, NULL), "stmt: v128 chain (bare value)");
    /* ══ the rest of the grammar: the families the tiler cannot EMIT here ══════
     *
     * Field, static, array, object, invoke and cast tiles all read ctx->types for
     * their typeidx / global / funcidx immediates, so this harness cannot run
     * their actions. It can still check their costs, and must: a cost is a claim
     * about a rule, not about any one program. (Compiling only the jre would make
     * that easy to forget — the jre's typeidxs happen to be three digits wide, but
     * a plugin's or a small program's are one, and the cost is defined at one.)
     *
     * LABELING runs the DP and the guards; only REDUCING runs the actions. So the
     * DP's claim is readable with no layout authority at all. The other side of
     * the identity comes from the encoder: the test writes the rule's opcode
     * sequence with minimal immediates through the same ew_emit/ew_u32 the action
     * uses, so the widths come from the generated wasm_op_enc[] table rather than
     * from a number typed into this file.
     *
     * ROOT_COST isolates the root rule: the DP's total for the tree, less the
     * covers of the children (each already pinned byte-exact above). */
    #define ROOT_COST(tree, nt, kids, m, EMIT_SEQ) do { \
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx); \
        burg_state_t* _st = burg_label_root((tree), &ctx); \
        CHECK(_st && burg_rule(_st, (nt)), m " (a rule covers it)"); \
        int _claim = _st ? burg_cost(_st, (nt)) - (kids) : -1; \
        emit_wasm_ctx _w = {0}; { emit_wasm_ctx* W = &_w; EMIT_SEQ } \
        int _want = (int)bbq_vec_len(_w.code); \
        if (_claim != _want) { \
            printf("  FAIL  cost != bytes: %s — the rule claims %d, its opcodes encode to %d\n", \
                   m, _claim, _want); \
            TEST_FAILED(); \
        } \
        bbq_vec_free(_w.code); burg_ctx_free(&ctx); } while (0)

    /* Children used below, with the costs pinned by the identity section above:
     * a ref/i32 local.get is 2, an i32.const is 2, ref.null is 2. */
    #define R0 sir_load_local(a, 0, SIR_DTREF, NULL)
    #define R1 sir_load_local(a, 1, SIR_DTREF, NULL)

    /* instance fields — struct.get / _s / _u / struct.set, all 0xFB + 2 immediates */
    ROOT_COST(sir_get_field(a, SIR_DTINT,  R0, 3, 1), i32_NT, 2, "struct.get (int field)",
              { ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_get_field(a, SIR_DTBYTE, R0, 3, 1), i32_NT, 2, "struct.get_s (byte field)",
              { ew_emit(W,WOP_STRUCT_GET_S); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_get_field(a, SIR_DTCHAR, R0, 3, 1), i32_NT, 2, "struct.get_u (char field)",
              { ew_emit(W,WOP_STRUCT_GET_U); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_get_field(a, SIR_DTLONG, R0, 3, 1), i64_NT, 2, "struct.get (long field)",
              { ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_get_field(a, SIR_DTREF,  R0, 3, 1), ref_NT, 2, "struct.get (ref field)",
              { ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_put_field(a, SIR_DTINT, R0, 3, 1, I0, NULL), stmt_NT, 4, "struct.set",
              { ew_emit(W,WOP_STRUCT_SET); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_set_header(a, R0, R1, 3, NULL), stmt_NT, 4, "struct.set (class header)",
              { ew_emit(W,WOP_STRUCT_SET); ew_u32(W,1); ew_u32(W,0); });

    /* statics → module globals */
    ROOT_COST(sir_get_static(a, SIR_DTINT, 3, 1), i32_NT, 0, "global.get (static read)",
              { ew_emit(W,WOP_GLOBAL_GET); ew_u32(W,1); });
    ROOT_COST(sir_put_static(a, SIR_DTINT, 3, 1, I0, NULL), stmt_NT, 2, "global.set (static write)",
              { ew_emit(W,WOP_GLOBAL_SET); ew_u32(W,1); });

    /* arrays — array.get/_s/_u/set take one typeidx; array.len takes none; a ref
     * element pays the extra ref.cast_null back to its static type. */
    ROOT_COST(sir_array_load(a, SIR_DTINT, R0, I0, NULL), i32_NT, 4, "array.get",
              { ew_emit(W,WOP_ARRAY_GET); ew_u32(W,1); });
    ROOT_COST(sir_array_load(a, SIR_DTBYTE, R0, I0, NULL), i32_NT, 4, "array.get_s",
              { ew_emit(W,WOP_ARRAY_GET_S); ew_u32(W,1); });
    ROOT_COST(sir_array_load(a, SIR_DTCHAR, R0, I0, NULL), i32_NT, 4, "array.get_u",
              { ew_emit(W,WOP_ARRAY_GET_U); ew_u32(W,1); });
    ROOT_COST(sir_array_load(a, SIR_DTREF, R0, I0, NULL), ref_NT, 4, "array.get + ref.cast_null",
              { ew_emit(W,WOP_ARRAY_GET); ew_u32(W,1);
                ew_emit(W,WOP_REF_CAST_NULL); ew_i32(W,1); });
    ROOT_COST(sir_array_store(a, SIR_DTINT, R0, I0, I1, NULL, NULL), stmt_NT, 6, "array.set",
              { ew_emit(W,WOP_ARRAY_SET); ew_u32(W,1); });
    ROOT_COST(sir_new_array(a, SIR_ATINT, I0), ref_NT, 2, "array.new_default",
              { ew_emit(W,WOP_ARRAY_NEW_DEFAULT); ew_u32(W,1); });
    ROOT_COST(sir_new_ref_array(a, 3, I0, NULL), ref_NT, 2, "array.new_default (ref backing)",
              { ew_emit(W,WOP_ARRAY_NEW_DEFAULT); ew_u32(W,1); });
    ROOT_COST(sir_array_new_data(a, SIR_ATBYTE, 1, 1, 1), ref_NT, 0, "array.new_data (+2 pushed consts)",
              { ew_emit(W,WOP_I32_CONST); ew_i32(W,1); ew_emit(W,WOP_I32_CONST); ew_i32(W,1);
                ew_emit(W,WOP_ARRAY_NEW_DATA); ew_u32(W,1); ew_u32(W,1); });
    ROOT_COST(sir_array_copy(a, SIR_DTINT, R0, I0, R1, I1,
                             sir_load_local(a, 2, SIR_DTINT, NULL), NULL), stmt_NT, 10, "array.copy",
              { ew_emit(W,WOP_ARRAY_COPY); ew_u32(W,1); ew_u32(W,1); });

    /* casts */
    ROOT_COST(sir_check_cast(a, R0, SIR_ATCLASS, 3), ref_NT, 2, "ref.cast_null (checkcast)",
              { ew_emit(W,WOP_REF_CAST_NULL); ew_i32(W,1); });
    /* InstanceOf carries a FLOOR: the class case is ref.test, the interface case
     * calls the helper instead, and which one is a LAYOUT property the guard may
     * not read. Pinning the floor against the sequence that produces it keeps the
     * floor a checked claim. */
    ROOT_COST(sir_instance_of(a, R0, SIR_ATCLASS, 3), i32_NT, 2, "ref.test (instanceof, class case = the floor)",
              { ew_emit(W,WOP_REF_TEST); ew_i32(W,1); });

    /* invocation — a direct call is the opcode plus a funcidx; a dispatched one
     * is the whole vtable sequence, which is why it costs an order more. */
    ROOT_COST(sir_invoke_static(a, 3, 1, NULL, 0, SIR_DTINT), i32_NT, 0, "call (static)",
              { ew_emit(W,WOP_CALL); ew_u32(W,1); });
    ROOT_COST(sir_invoke_special(a, R0, 3, 1, NULL, 0, SIR_DTINT), i32_NT, 2, "call (special)",
              { ew_emit(W,WOP_CALL); ew_u32(W,1); });
    ROOT_COST(sir_invoke_static(a, 3, 1, NULL, 0, SIR_DTINT), tail_NT, 0, "return_call (tail static)",
              { ew_emit(W,WOP_RETURN_CALL); ew_u32(W,1); });
    ROOT_COST(sir_invoke_virtual(a, R0, 3, 1, NULL, 0, SIR_DTINT), i32_NT, 2, "vtable dispatch + call_ref",
              { ew_emit(W,WOP_LOCAL_GET);  ew_u32(W,1);          /* reload the receiver   */
                ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,0);   /* obj → ClassDesc */
                ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1);   /* → vtable        */
                ew_emit(W,WOP_I32_CONST);  ew_i32(W,1);                 /* slot            */
                ew_emit(W,WOP_ARRAY_GET);  ew_u32(W,1);                 /* → funcref       */
                ew_emit(W,WOP_REF_CAST);   ew_i32(W,1);                 /* → exact type    */
                ew_emit(W,WOP_CALL_REF);   ew_i32(W,1); });
    ROOT_COST(sir_invoke_interface(a, R0, 3, 1, NULL, 0, SIR_DTINT), i32_NT, 2,
              "interface dispatch (+ the cast to root)",
              { ew_emit(W,WOP_LOCAL_GET);  ew_u32(W,1);
                ew_emit(W,WOP_REF_CAST);   ew_i32(W,1);                 /* → the root struct */
                ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,0);
                ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1);
                ew_emit(W,WOP_I32_CONST);  ew_i32(W,1);
                ew_emit(W,WOP_ARRAY_GET);  ew_u32(W,1);
                ew_emit(W,WOP_REF_CAST);   ew_i32(W,1);
                ew_emit(W,WOP_CALL_REF);   ew_i32(W,1); });

    /* …and a call's ARGUMENTS are part of its cover, so they are part of its cost.
     *
     * iburg p.4 (Fig. 3): "Each C sums the costs of the non-terminals on the
     * right-hand side and the cost of the relevant pattern or chain rule" — and
     * p.7's generated state() is that sentence as code: c = l->cost[reg_NT] +
     * r->cost[rc_NT] + 1. Every child contributes its cost at the nonterminal the
     * cover demands of it; the p.1 optimality claim (a minimum-cost cover of the
     * TREE) stands on exactly that invariant, because the root's cost[start] IS
     * the cover's cost only if nothing is covered without being summed.
     *
     * Our invokes are variable-arity — the one place this IR departs from the
     * paper's fixed-arity trees — and the adaptation reduced the extra children
     * with START but never costed them, so a ten-argument call priced the same as
     * a nullary one. These pins hold the restored invariant: an argument
     * contributes cost[START], the same goal the reducer tiles it with. (Every
     * fixture above passes NULL,0 args, which is why this gap survived B1's
     * "costs are bytes" test — the shape that made the claim true was the only
     * shape tested.) */
    {
        sir_node_t* args2[] = { I0, I1 };                    /* two local.get = 2+2 */
        ROOT_COST(sir_invoke_static(a, 3, 1, args2, 2, SIR_DTINT), i32_NT, 4,
                  "call (static, 2 args): the args are in the price",
                  { ew_emit(W,WOP_CALL); ew_u32(W,1); });
        ROOT_COST(sir_invoke_static(a, 3, 1, args2, 2, SIR_DTINT), tail_NT, 4,
                  "return_call (tail static, 2 args): same invariant at the tail goal",
                  { ew_emit(W,WOP_RETURN_CALL); ew_u32(W,1); });
    }
    {
        sir_node_t* args1[] = { I1 };
        ROOT_COST(sir_invoke_virtual(a, R0, 3, 1, args1, 1, SIR_DTINT), i32_NT, 4,
                  "vtable dispatch (1 arg): receiver AND arg both priced",
                  { ew_emit(W,WOP_LOCAL_GET);  ew_u32(W,1);
                    ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,0);
                    ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1);
                    ew_emit(W,WOP_I32_CONST);  ew_i32(W,1);
                    ew_emit(W,WOP_ARRAY_GET);  ew_u32(W,1);
                    ew_emit(W,WOP_REF_CAST);   ew_i32(W,1);
                    ew_emit(W,WOP_CALL_REF);   ew_i32(W,1); });
    }

    /* reflection (§20.3.6) */
    ROOT_COST(sir_class_instantiable(a, R0), i32_NT, 2, "Class.factory != null",
              { ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1);
                ew_emit(W,WOP_REF_IS_NULL); ew_emit(W,WOP_I32_EQZ); });
    ROOT_COST(sir_class_construct(a, R0), ref_NT, 2, "Class.newInstance",
              { ew_emit(W,WOP_STRUCT_GET); ew_u32(W,1); ew_u32(W,1);
                ew_emit(W,WOP_REF_CAST);   ew_i32(W,1);
                ew_emit(W,WOP_CALL_REF);   ew_u32(W,1); });

    /* allocation — FLOORS. Both actions emit one group per instance field, and a
     * class's field count is a layout property, not a node one. The floor is the
     * zero-field sequence, pinned here so it is a measured minimum. */
    ROOT_COST(sir_new(a, 3), ref_NT, 0, "struct.new (allocation, zero-field floor)",
              { ew_emit(W,WOP_GLOBAL_GET); ew_u32(W,1);
                ew_emit(W,WOP_STRUCT_NEW); ew_u32(W,1); });
    ROOT_COST(sir_clone_copy(a, 3), ref_NT, 0, "struct.new (clone, zero-field floor)",
              { ew_emit(W,WOP_STRUCT_NEW); ew_u32(W,1); });

    /* the catch landing pad */
    ROOT_COST(sir_exception_entry(a, 1, 3, NULL), stmt_NT, 0, "local.set (catch landing)",
              { ew_emit(W,WOP_LOCAL_SET); ew_u32(W,1); });

    /* ── SIMD: the opcode is a node payload, so each family splits on its encoded
     * width. Both arms of a representative family are pinned — a one-arm check
     * would pass with the other arm's cost wrong. ew_op_width is the same
     * wasm_op_enc[] row ew_emit encodes from, so this is not a restated number. */
    CHECK(ew_op_width(WOP_V128_BITSELECT) == 2 && ew_op_width(WOP_I32X4_ADD) == 3,
          "SIMD width fixtures straddle the 2/3-byte boundary");
    ROOT_COST(sir_simd_bin(a, WOP_V128_BITSELECT,
                           sir_load_local(a,0,SIR_DTV128,NULL),
                           sir_load_local(a,1,SIR_DTV128,NULL)), v128_NT, 4,
              "SIMD binary, 2-byte opcode", { ew_emit(W,WOP_V128_BITSELECT); });
    ROOT_COST(sir_simd_bin(a, WOP_I32X4_ADD,
                           sir_load_local(a,0,SIR_DTV128,NULL),
                           sir_load_local(a,1,SIR_DTV128,NULL)), v128_NT, 4,
              "SIMD binary, 3-byte opcode", { ew_emit(W,WOP_I32X4_ADD); });
    ROOT_COST(sir_simd_extract_i(a, WOP_I8X16_EXTRACT_LANE_S, 0,
                                 sir_load_local(a,0,SIR_DTV128,NULL)), i32_NT, 2,
              "SIMD extract, 2-byte opcode + lane",
              { ew_emit(W,WOP_I8X16_EXTRACT_LANE_S); ew_byte(W,0); });
    ROOT_COST(sir_simd_const(a, 0, 0), v128_NT, 0, "v128.const (16 immediate bytes)",
              { ew_emit(W,WOP_V128_CONST); for (int b = 0; b < 16; b++) ew_byte(W,0); });
    ROOT_COST(sir_simd_mem_load(a, WOP_V128_LOAD, 4, I0), v128_NT, 2, "v128.load",
              { ew_emit(W,WOP_V128_LOAD); ew_u32(W,4); ew_u32(W,0); });

    /* ══ the labeler picks a MINIMUM, not merely a cover ══════════════════════
     *
     * Every tree of depth ≤ 3 over a representative operator set, tiled two ways:
     * by the generated DP, and by the recursive search above over the exported
     * rules. The two numbers must agree on every one of them. The operator set
     * spans what makes the DP non-trivial — mixed operand types (so the guarded
     * per-valtype leaf selectors have to fire correctly), a unary and three binary
     * operators, and constants alongside locals so more than one leaf rule
     * competes at the same node. */
    {
        sir_node_t* leaves[] = {
            sir_load_const(a, 5, SIR_DTINT),
            sir_load_local(a, 0, SIR_DTINT, NULL),
            sir_load_long_const(a, 7),
            sir_load_local(a, 1, SIR_DTLONG, NULL),
            /* Zero constants are what give the DP something to DECIDE: a compare
             * against one has two covers, the general one and the folded one. */
            sir_load_const(a, 0, SIR_DTINT),
            sir_load_long_const(a, 0),
        };
        const int NL = (int)(sizeof leaves / sizeof leaves[0]);

        /* Level n from level n-1: every unary over it, and every binary pairing
         * a level-(n-1) tree with any tree at or below that level. */
        sir_node_t** pool = NULL;
        for (int i = 0; i < NL; i++) bbq_vec_push(pool, leaves[i]);
        int prev_begin = 0, prev_end = NL;
        for (int depth = 2; depth <= 3; depth++) {
            int nb = (int)bbq_vec_len(pool);
            for (int i = prev_begin; i < prev_end; i++) {
                bbq_vec_push(pool, sir_log_not(a, SIR_DTINT, pool[i]));
                for (int j = 0; j < prev_end; j++) {
                    bbq_vec_push(pool, sir_add(a, SIR_DTINT, pool[i], pool[j]));
                    bbq_vec_push(pool, sir_mul(a, SIR_DTINT, pool[i], pool[j]));
                    bbq_vec_push(pool, sir_eq(a, pool[i], pool[j]));
                }
            }
            prev_begin = nb; prev_end = (int)bbq_vec_len(pool);
        }

        int n = (int)bbq_vec_len(pool), checked = 0, disagreed = 0, decided = 0;
        for (int i = 0; i < n; i++) {
            burg_ctx_t ctx = {0}; burg_ctx_init(&ctx);
            burg_state_t* st = burg_label_root(pool[i], &ctx);
            int labeled  = (st && burg_rule(st, stmt_NT)) ? burg_cost(st, stmt_NT) : -1;
            int cheapest = oracle_search(pool[i], stmt_NT, &ctx, 32, 0);
            int dearest  = oracle_search(pool[i], stmt_NT, &ctx, 32, 1);
            if (labeled != cheapest) {
                if (disagreed < 5)
                    printf("  FAIL  tree %d: the labeler's cover costs %d, "
                           "an independent search finds %d\n", i, labeled, cheapest);
                disagreed++;
            }
            if (cheapest != dearest) decided++;   /* the grammar offered a choice */
            checked++;
            burg_ctx_free(&ctx);
        }
        CHECK(checked > 1000, "the enumeration is large enough to be worth running");
        if (disagreed) {
            printf("  FAIL  %d of %d trees: the labeler's cover is not the minimum\n",
                   disagreed, checked);
            TEST_FAILED();
        } else {
            printf("  note   %d trees (depth <= 3): the labeler's cover is the minimum "
                   "an independent search over the exported rules finds\n", checked);
            printf("  note   %d of them admit more than one cover — where that count is 0 "
                   "the DP has nothing to decide and minimality is true but idle\n", decided);
        }
        /* Both must also agree where NO cover exists, not just on numbers. */
        CHECK(oracle_best(sir_load_null(a), i64_NT, NULL, 32) < 0,
              "a null reference has no i64 derivation, and the search says so");
        bbq_vec_free(pool);
    }

    #undef R0
    #undef R1
    #undef ROOT_COST
    #undef COST_IS_BYTES

    bbq_arena_free(&arena);
    return TEST_SUMMARY("test_codegen_wasm");
}
