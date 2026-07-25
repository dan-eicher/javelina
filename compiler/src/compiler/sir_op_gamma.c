/* sir_op_gamma.c — γ per-opcode table for the Click optimizer.
 *
 * Single point of truth for per-opcode γ. Every property the engine
 * consults during PROPAGATE is one row read. Row layout in
 * sir_op_gamma.h.
 *
 * Designated initializers indexed by sir_node_t_tag (cf. interp.c's
 * dispatch[256]). Rows for spine opcodes are still populated so the
 * index → row identity invariant holds; their γ-bearing slots stay
 * zero (GT_NONE / GS_NONE / GC_NONE / NULL fold) because the engine
 * never asks γ questions of a spine node. */

#include "javelina/compiler/sir_op_gamma.h"
#include "javelina/compiler/sir_optimizer.h"  /* cp_const_t for range folds */
#include "javelina/compiler/jint.h"           /* the exact-arithmetic core */
#include "gen/sir_ast.h"
#include "bbq_vec.h"
#include <math.h>  /* isnan, for the JLS float→int narrowing folds */

/* With distinct comparison nodes (Eq/Ne/Lt/Le/Gt/Ge) the node TAG is the
 * operator (Click §3.8 congruence: "same function on congruent inputs").
 * So each comparison is its own congruence class — no bucket discriminator
 * — and commutativity is a static per-row bool (EQ/NE only). */

#include <limits.h>

/* ── §3.2.1 γ_K — per-opcode constant fold ──────────────────────────
 * Each helper computes its opcode's int32 fold. Binary ops return false
 * via *out=0 when the fold is undefined (DIV by 0 or INT_MIN / -1); the
 * unary / bitwise / shift / compare folds are total. Defined wraparound
 * for arith — Java semantics, matching cp_fold's pre-refactor behaviour
 * verbatim. */

static bool gamma_fold_add (int32_t l, int32_t r, int32_t* out) { *out = jint_add(l, r); return true; }
static bool gamma_fold_sub (int32_t l, int32_t r, int32_t* out) { *out = jint_sub(l, r); return true; }
static bool gamma_fold_mul (int32_t l, int32_t r, int32_t* out) { *out = jint_mul(l, r); return true; }
/* §15.16.2: division by zero THROWS — refuse the fold and keep the guard. The
 * core handles the non-throwing MIN/-1 case by the spec's stated value. */
static bool gamma_fold_div (int32_t l, int32_t r, int32_t* out) {
    if (r == 0) { *out = 0; return false; }
    *out = jint_div(l, r); return true;
}
static bool gamma_fold_rem (int32_t l, int32_t r, int32_t* out) {
    if (r == 0) { *out = 0; return false; }
    *out = jint_rem(l, r); return true;
}
static bool gamma_fold_and (int32_t l, int32_t r, int32_t* out) { *out = jint_and(l, r); return true; }
static bool gamma_fold_or  (int32_t l, int32_t r, int32_t* out) { *out = jint_or (l, r); return true; }
static bool gamma_fold_xor (int32_t l, int32_t r, int32_t* out) { *out = jint_xor(l, r); return true; }
static bool gamma_fold_shl (int32_t l, int32_t r, int32_t* out) { *out = jint_shl (l, r); return true; }
static bool gamma_fold_shr (int32_t l, int32_t r, int32_t* out) { *out = jint_shr (l, r); return true; }
static bool gamma_fold_ushr(int32_t l, int32_t r, int32_t* out) { *out = jint_ushr(l, r); return true; }

static bool gamma_fold_neg   (int32_t a, int32_t* out) { *out = jint_neg(a); return true; }
static bool gamma_fold_lognot(int32_t a, int32_t* out) { *out = (a == 0);    return true; }
static bool gamma_fold_s2b   (int32_t a, int32_t* out) { *out = (int32_t)(int8_t)a;            return true; }
static bool gamma_fold_i2b   (int32_t a, int32_t* out) { *out = (int32_t)(int8_t)a;            return true; }
static bool gamma_fold_i2s   (int32_t a, int32_t* out) { *out = (int32_t)(int16_t)a;           return true; }
static bool gamma_fold_i2c   (int32_t a, int32_t* out) { *out = a & 0xFFFF;                    return true; }
static bool gamma_fold_s2i   (int32_t a, int32_t* out) { *out = a;                             return true; }

/* ── §5.1.2/§5.1.3 primitive conversion folds ───────────────────────
 * A KNOWN operand of the source width → a KNOWN result of the target
 * width. float/double→int/long follow JLS §5.1.3 exactly (NaN→0, ±∞ /
 * overflow → clamp to MIN/MAX, otherwise round toward zero) — NOT a C
 * cast, which is UB out of range. float results round to f32 once
 * (single rounding via (float)source, no double-round through f64). */
static int32_t java_d2i(double d) {
    if (isnan(d)) return 0;
    if (d >=  2147483647.0) return INT32_MAX;   /* covers +inf */
    if (d <= -2147483648.0) return INT32_MIN;   /* covers -inf */
    return (int32_t)d;                          /* in range: trunc toward zero */
}
static int64_t java_d2l(double d) {
    if (isnan(d)) return 0;
    if (d >=  9223372036854775808.0) return INT64_MAX;   /* 2^63; covers +inf */
    if (d <  -9223372036854775808.0) return INT64_MIN;   /* -2^63; covers -inf */
    return (int64_t)d;
}
static cp_const_t kc_i32(int32_t v) { return (cp_const_t){ .state=CP_C_KNOWN, .cwidth=CP_W_I32, .value =v }; }
static cp_const_t kc_i64(int64_t v) { return (cp_const_t){ .state=CP_C_KNOWN, .cwidth=CP_W_I64, .lvalue=v }; }
static cp_const_t kc_f32(float v)   { return (cp_const_t){ .state=CP_C_KNOWN, .cwidth=CP_W_F32, .fvalue=v }; }
static cp_const_t kc_f64(double v)  { return (cp_const_t){ .state=CP_C_KNOWN, .cwidth=CP_W_F64, .dvalue=v }; }
/* Each fold reads its operand's own carrier (cp_known_f32 / cp_known_f64): an f32
 * is NOT held in the double — widening a signaling NaN quiets it, and the raw bits
 * are observable through the Move* reinterprets below. Numeric f32→f64 widening
 * (java_d2i/d2l's argument, F2D) stays exact, so those folds may take the double. */
static cp_const_t gamma_conv_i2l(cp_const_t a) { return kc_i64((int64_t)a.value); }
static cp_const_t gamma_conv_i2f(cp_const_t a) { return kc_f32((float)a.value); }
static cp_const_t gamma_conv_i2d(cp_const_t a) { return kc_f64((double)a.value); }
static cp_const_t gamma_conv_l2i(cp_const_t a) { return kc_i32((int32_t)a.lvalue); }
static cp_const_t gamma_conv_l2f(cp_const_t a) { return kc_f32((float)a.lvalue); }
static cp_const_t gamma_conv_l2d(cp_const_t a) { return kc_f64((double)a.lvalue); }
static cp_const_t gamma_conv_f2i(cp_const_t a) { return kc_i32(java_d2i((double)cp_known_f32(a))); }
static cp_const_t gamma_conv_f2l(cp_const_t a) { return kc_i64(java_d2l((double)cp_known_f32(a))); }
static cp_const_t gamma_conv_f2d(cp_const_t a) { return kc_f64((double)cp_known_f32(a)); }  /* f32→f64 numerically exact */
static cp_const_t gamma_conv_d2i(cp_const_t a) { return kc_i32(java_d2i(cp_known_f64(a))); }
static cp_const_t gamma_conv_d2l(cp_const_t a) { return kc_i64(java_d2l(cp_known_f64(a))); }
static cp_const_t gamma_conv_d2f(cp_const_t a) { return kc_f32((float)cp_known_f64(a)); }   /* round to f32 */

/* Move* fold: a bit-preserving reinterpret (not a numeric convert) — the constant's bits are
 * copied verbatim into the other domain. Raw (floatToRawIntBits semantics); NaN is NOT
 * canonicalised here (the public Float.floatToIntBits does that in compiled Java), so these
 * MUST read the exact carrier — a detour through double would quiet a signaling NaN. */
static cp_const_t gamma_move_f2i(cp_const_t a) { float f = cp_known_f32(a); int32_t b; memcpy(&b, &f, 4); return kc_i32(b); }
static cp_const_t gamma_move_i2f(cp_const_t a) { int32_t b = a.value;  float f;  memcpy(&f, &b, 4); return kc_f32(f); }
static cp_const_t gamma_move_d2l(cp_const_t a) { double d = cp_known_f64(a); int64_t b; memcpy(&b, &d, 8); return kc_i64(b); }
static cp_const_t gamma_move_l2d(cp_const_t a) { int64_t b = a.lvalue; double d;  memcpy(&d, &b, 8); return kc_f64(d); }

/* f64 math intrinsics (§20.11) — correctly-rounded/exact, so host libm folds match the wasm opcode. */
static cp_const_t gamma_f64sqrt(cp_const_t a)    { return kc_f64(sqrt(cp_known_f64(a))); }
static cp_const_t gamma_f64floor(cp_const_t a)   { return kc_f64(floor(cp_known_f64(a))); }
static cp_const_t gamma_f64ceil(cp_const_t a)    { return kc_f64(ceil(cp_known_f64(a))); }
static cp_const_t gamma_f64nearest(cp_const_t a) { return kc_f64(rint(cp_known_f64(a))); }

/* SIMD partition-init discriminators — the EXACT operator immediate(s) of a
 * family node, per the bucket_discriminator contract (no packing loss:
 * WOP_* < 2^16, lane < 32, so (op<<5)|lane is lossless). */
static uint32_t gamma_simd_disc_bin(const sir_node_t* e)       { return (uint32_t)e->simd_bin.op; }
static uint32_t gamma_simd_disc_un(const sir_node_t* e)        { return (uint32_t)e->simd_un.op; }
static uint32_t gamma_simd_disc_shift(const sir_node_t* e)     { return (uint32_t)e->simd_shift.op; }
static uint32_t gamma_simd_disc_tern(const sir_node_t* e)      { return (uint32_t)e->simd_tern.op; }
static uint32_t gamma_simd_disc_test(const sir_node_t* e)      { return (uint32_t)e->simd_test_i.op; }
static uint32_t gamma_simd_disc_splat_i(const sir_node_t* e)   { return (uint32_t)e->simd_splat_i.op; }
static uint32_t gamma_simd_disc_splat_l(const sir_node_t* e)   { return (uint32_t)e->simd_splat_l.op; }
static uint32_t gamma_simd_disc_splat_f(const sir_node_t* e)   { return (uint32_t)e->simd_splat_f.op; }
static uint32_t gamma_simd_disc_splat_d(const sir_node_t* e)   { return (uint32_t)e->simd_splat_d.op; }
static uint32_t gamma_simd_disc_extract_i(const sir_node_t* e) { return ((uint32_t)e->simd_extract_i.op << 5) | (uint32_t)e->simd_extract_i.lane; }
static uint32_t gamma_simd_disc_extract_l(const sir_node_t* e) { return ((uint32_t)e->simd_extract_l.op << 5) | (uint32_t)e->simd_extract_l.lane; }
static uint32_t gamma_simd_disc_extract_f(const sir_node_t* e) { return ((uint32_t)e->simd_extract_f.op << 5) | (uint32_t)e->simd_extract_f.lane; }
static uint32_t gamma_simd_disc_extract_d(const sir_node_t* e) { return ((uint32_t)e->simd_extract_d.op << 5) | (uint32_t)e->simd_extract_d.lane; }
static uint32_t gamma_simd_disc_replace_i(const sir_node_t* e) { return ((uint32_t)e->simd_replace_i.op << 5) | (uint32_t)e->simd_replace_i.lane; }
static uint32_t gamma_simd_disc_replace_l(const sir_node_t* e) { return ((uint32_t)e->simd_replace_l.op << 5) | (uint32_t)e->simd_replace_l.lane; }
static uint32_t gamma_simd_disc_replace_f(const sir_node_t* e) { return ((uint32_t)e->simd_replace_f.op << 5) | (uint32_t)e->simd_replace_f.lane; }
static uint32_t gamma_simd_disc_replace_d(const sir_node_t* e) { return ((uint32_t)e->simd_replace_d.op << 5) | (uint32_t)e->simd_replace_d.lane; }

/* CMP fold reads the op discriminator from the SIR node. EQ / NE / LT /
 * GE / GT / LE return 0 or 1 as a boolean represented in int32. */
static int32_t gamma_fold_cmp(int op, int32_t l, int32_t r) {  /* op = node tag */
    switch (op) {
        case SIR_EQ: return l == r;
        case SIR_NE: return l != r;
        case SIR_LT: return l <  r;
        case SIR_GE: return l >= r;
        case SIR_GT: return l >  r;
        case SIR_LE: return l <= r;
    }
    return 0;
}

/* ── Range-aware γ_K helpers ─────────────────────────────────────────
 * Each helper accepts cp_const_t (KNOWN normalizes to [k,k] internally)
 * and returns cp_const_t. Overflow / undefined results → CP_C_BOTTOM
 * (sound: BOTTOM means "no information," which is always a valid
 * super-set of the real value). KNOWN-vs-KNOWN cases delegate to the
 * existing pointwise fold by routing through the same arithmetic
 * (degenerate ranges collapse on output). */

static void cp_range_bounds(cp_const_t c, int64_t* lo, int64_t* hi) {
    if (c.state == CP_C_KNOWN) { *lo = *hi = cp_known_i64(c); }
    else /* CP_C_RANGE */      { *lo = c.lo;  *hi = c.hi; }
}

/* γ_K operand gate (Fig 3.2/3.3). Only KNOWN/RANGE carry an interval fact. TOP is
 * "not yet known" and DOMINATES — we do not propagate information until all the facts
 * are known; anything else (BOTTOM, REF) yields BOTTOM — no claim. Reading BOTTOM's
 * zeroed payload as [0,0] folded a §15 bounds guard to ALWAYS-THROW (the initProperties
 * miscompile); every fold below gates its operands HERE, before any bounds read. */
static bool cp_range_has_fact(cp_const_t c) {
    return c.state == CP_C_KNOWN || c.state == CP_C_RANGE;
}
static bool cp_gamma_gate2(cp_const_t a, cp_const_t b, cp_const_t* out) {
    if (a.state == CP_C_TOP || b.state == CP_C_TOP) {
        *out = (cp_const_t){ .state = CP_C_TOP };    return true;
    }
    if (!cp_range_has_fact(a) || !cp_range_has_fact(b)) {
        *out = (cp_const_t){ .state = CP_C_BOTTOM }; return true;
    }
    return false;
}
static bool cp_gamma_gate1(cp_const_t a, cp_const_t* out) {
    if (a.state == CP_C_TOP)   { *out = (cp_const_t){ .state = CP_C_TOP };    return true; }
    if (!cp_range_has_fact(a)) { *out = (cp_const_t){ .state = CP_C_BOTTOM }; return true; }
    return false;
}

/* Stride accessor: KNOWN is treated as a singleton with no inherent
 * stride (returns 0, the gcd-identity); RANGE returns its stored
 * stride. Click §4.5: ranges-with-strides — the gcd identity at 0
 * lets KNOWN ⊕ RANGE propagate the RANGE's stride through. */
static int64_t cp_range_stride(cp_const_t c) {
    return (c.state == CP_C_RANGE) ? c.stride : 0;
}

static int64_t cp_gamma_gcd(int64_t a, int64_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { int64_t t = a % b; a = b; b = t; }
    return a;
}

static cp_const_t cp_range_make(int64_t lo, int64_t hi, cp_cwidth_t w) {
    if (lo == hi) return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                                       .value = (int32_t)lo, .lvalue = lo };
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = lo, .hi = hi,
                         .stride = 1 };
}

/* Strided RANGE constructor: snaps hi upward to (hi - lo) % stride == 0
 * for canonicalization, or drops to dense (stride = 1) on overflow. */
static cp_const_t cp_range_make_strided(int64_t lo, int64_t hi, int64_t stride,
                                        cp_cwidth_t w) {
    if (lo == hi) return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                                       .value = (int32_t)lo, .lvalue = lo };
    if (stride <= 1) stride = 1;
    if (stride > 1) {
        int64_t span = hi - lo;
        int64_t rem  = span % stride;
        if (rem != 0) {
            int64_t bump = stride - rem;
            if (hi > cp_width_max(w) - bump) stride = 1;
            else                             hi += bump;
        }
    }
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = lo, .hi = hi,
                         .stride = stride };
}

static cp_const_t gamma_range_fold_add(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi, lo, hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    if (__builtin_add_overflow(a_lo, b_lo, &lo) ||
        __builtin_add_overflow(a_hi, b_hi, &hi) ||
        lo < cp_width_min(a.cwidth) || hi > cp_width_max(a.cwidth))
        return (cp_const_t){ .state = CP_C_BOTTOM };
    /* Strides add by gcd (Click §4.5): the sum-set's spacing is the
     * gcd of the input spacings. KNOWN's "stride 0" is gcd-identity,
     * so KNOWN ⊕ RANGE(s) propagates s. */
    int64_t stride = cp_gamma_gcd(cp_range_stride(a), cp_range_stride(b));
    return cp_range_make_strided(lo, hi, stride, a.cwidth);
}

static cp_const_t gamma_range_fold_sub(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi, lo, hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    /* a - b: min when a is smallest and b is largest; max symmetric. */
    if (__builtin_sub_overflow(a_lo, b_hi, &lo) ||
        __builtin_sub_overflow(a_hi, b_lo, &hi) ||
        lo < cp_width_min(a.cwidth) || hi > cp_width_max(a.cwidth))
        return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t stride = cp_gamma_gcd(cp_range_stride(a), cp_range_stride(b));
    return cp_range_make_strided(lo, hi, stride, a.cwidth);
}

static cp_const_t gamma_range_fold_neg(cp_const_t a) {
    cp_const_t g;
    if (cp_gamma_gate1(a, &g)) return g;
    int64_t lo, hi;
    cp_range_bounds(a, &lo, &hi);
    /* -MIN overflows the width. If lo == width-min, the range straddles
     * the unrepresentable result. */
    if (lo == cp_width_min(a.cwidth)) return (cp_const_t){ .state = CP_C_BOTTOM };
    /* Negation preserves stride: -{lo, lo+s, ..., hi} = {-hi, -hi+s, ..., -lo}. */
    return cp_range_make_strided(-hi, -lo, cp_range_stride(a), a.cwidth);
}

/* MUL: the four corner products bound the result; any corner that overflows
 * the type forfeits the claim (§15.16.1: "the result is the low-order bits
 * of the mathematical product" — a wrapped range is not an interval). A singleton multiplier SCALES the other side's spacing — the
 * stride PRODUCER (Click §4.5); two proper ranges have no usable product
 * spacing (stride 1). */
static cp_const_t gamma_range_fold_mul(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi, c[4];
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    if (__builtin_mul_overflow(a_lo, b_lo, &c[0]) ||
        __builtin_mul_overflow(a_lo, b_hi, &c[1]) ||
        __builtin_mul_overflow(a_hi, b_lo, &c[2]) ||
        __builtin_mul_overflow(a_hi, b_hi, &c[3]))
        return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t lo = c[0], hi = c[0];
    for (int i = 1; i < 4; i++) {
        if (c[i] < lo) lo = c[i];
        if (c[i] > hi) hi = c[i];
    }
    if (lo < cp_width_min(a.cwidth) || hi > cp_width_max(a.cwidth))
        return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t stride = 1, k = 0, s = 0;
    if      (a_lo == a_hi) { k = a_lo; s = cp_range_stride(b); }
    else if (b_lo == b_hi) { k = b_lo; s = cp_range_stride(a); }
    if (k != 0 && k != INT64_MIN) {
        if (s < 1) s = 1;                        /* dense RANGE side */
        if (__builtin_mul_overflow(k < 0 ? -k : k, s, &stride) || stride < 1)
            stride = 1;
    }
    return cp_range_make_strided(lo, hi, stride, a.cwidth);
}

/* SHL by a KNOWN in-range count is ×2^count — same corner/overflow
 * discipline as MUL via delegation, and the other stride producer. A
 * non-singleton or out-of-range count claims nothing (§15.18 masks the
 * count to the five low bits; modeling the mask is the wrap case we
 * refuse — the scalar fold, which masks exactly, stays the precise one). */
static cp_const_t gamma_range_fold_shl(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t b_lo, b_hi;
    cp_range_bounds(b, &b_lo, &b_hi);
    int maxsh = (a.cwidth == CP_W_I64) ? 63 : 31;
    if (b_lo != b_hi || b_lo < 0 || b_lo > maxsh)
        return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t f = (int64_t)1 << b_lo;
    cp_const_t m = { .state = CP_C_KNOWN, .cwidth = a.cwidth,
                     .value = (int32_t)f, .lvalue = f };
    return gamma_range_fold_mul(a, m);
}

/* DIV by a divisor RANGE excluding 0 (§15.16.2: /0 THROWS — the guard
 * stays): "integer division rounds toward 0" makes it corner-monotone once
 * 0 and the width-min/−1 pair ("integer overflow occurs and the result is
 * equal to the dividend") are excluded, so the four bound pairs bound the
 * result (the sema linter's twin rule; singleton-k-only was the
 * parity-audit gap). The stride survives only in the singleton-positive
 * case where the divisor divides both the spacing and the non-negative
 * base. */
static cp_const_t gamma_range_fold_div(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    if (b_lo <= 0 && b_hi >= 0) return (cp_const_t){ .state = CP_C_BOTTOM };
    /* width-min / −1 wraps (Java: MIN/−1 == MIN) — forfeit that corner. */
    if (a_lo == cp_width_min(a.cwidth) && b_lo <= -1 && b_hi >= -1)
        return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t c[4] = { a_lo / b_lo, a_lo / b_hi, a_hi / b_lo, a_hi / b_hi };
    int64_t lo = c[0], hi = c[0];
    for (int i = 1; i < 4; i++) {
        if (c[i] < lo) lo = c[i];
        if (c[i] > hi) hi = c[i];
    }
    int64_t k = b_lo, s = cp_range_stride(a);
    int64_t stride = (b_lo == b_hi && k > 0 && s > 1 && s % k == 0
                      && a_lo >= 0 && a_lo % k == 0) ? s / k : 1;
    return cp_range_make_strided(lo, hi, stride, a.cwidth);
}

/* REM by a divisor RANGE excluding 0: the result is bounded by |divisor|−1
 * and follows the DIVIDEND's sign (JLS §15.16.3 — the sema linter's twin
 * rule; positive-singleton + non-negative-dividend-only was the
 * parity-audit gap). The stride CONSUMER survives in the singleton case:
 * spacing a multiple of k pins every value to one residue. */
static cp_const_t gamma_range_fold_rem(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    if (b_lo <= 0 && b_hi >= 0) return (cp_const_t){ .state = CP_C_BOTTOM };
    /* |width-min| overflows — forfeit rather than mis-bound. */
    if (b_lo == cp_width_min(b.cwidth)) return (cp_const_t){ .state = CP_C_BOTTOM };
    int64_t blo_abs = b_lo < 0 ? -b_lo : b_lo;
    int64_t bhi_abs = b_hi < 0 ? -b_hi : b_hi;
    int64_t bound = (blo_abs > bhi_abs ? blo_abs : bhi_abs) - 1;
    int64_t k = b_lo, s = cp_range_stride(a);
    if (b_lo == b_hi && k > 0 && a_lo >= 0) {
        if (s > 0 && s % k == 0) {
            int64_t r = a_lo % k;
            return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = a.cwidth,
                                 .value = (int32_t)r, .lvalue = r };
        }
        if (a_hi < k) return a;                  /* identity: every value < k */
    }
    int64_t lo = a_lo >= 0 ? 0 : (a_lo > -bound ? a_lo : -bound);
    int64_t hi = a_hi <= 0 ? 0 : (a_hi < bound ? a_hi : bound);
    return cp_range_make(lo, hi, a.cwidth);
}

/* AND: a KNOWN non-negative mask k bounds the result to [0, k] for ANY
 * other side — two's complement: the mask clears the sign bit — so a
 * sign-spanning operand forfeits nothing (the sema linter's twin rule;
 * requiring both sides non-negative was the parity-audit gap). A LOW mask
 * (2^m − 1) with the spacing a multiple of 2^m is the stride consumer:
 * x ≡ x₀ (mod 2^m) determines x & (2^m−1) for all ints, negatives
 * included. Two proper non-negative ranges bound by min(hi, hi). */
static cp_const_t gamma_range_fold_and(cp_const_t a, cp_const_t b) {
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;
    int64_t a_lo, a_hi, b_lo, b_hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    int64_t k, r_lo, r_hi, s;
    if      (b_lo == b_hi && b_lo >= 0) { k = b_lo; r_lo = a_lo; r_hi = a_hi; s = cp_range_stride(a); }
    else if (a_lo == a_hi && a_lo >= 0) { k = a_lo; r_lo = b_lo; r_hi = b_hi; s = cp_range_stride(b); }
    else if (a_lo >= 0 && b_lo >= 0)      /* x & y ≤ min(x, y) for x, y ≥ 0 */
        return cp_range_make(0, a_hi < b_hi ? a_hi : b_hi, a.cwidth);
    else
        return (cp_const_t){ .state = CP_C_BOTTOM };
    if (k != INT64_MAX && ((k + 1) & k) == 0 && s > 0 && s % (k + 1) == 0) {
        int64_t r = r_lo & k;
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = a.cwidth,
                             .value = (int32_t)r, .lvalue = r };
    }
    /* The other side's hi tightens the bound only when it cannot be
     * negative (x = −1 & 7 is 7, not ≤ x_hi). */
    return cp_range_make(0, (r_lo >= 0 && r_hi < k) ? r_hi : k, a.cwidth);
}

/* Width-narrowing conversions. When the input range fits entirely
 * within the target width, the conversion is value-preserving — the
 * lattice fact passes through unchanged. When the range crosses
 * boundaries, the conversion truncates: result range is the target
 * width's full extent (sound but lossy; the rewrite layer separately
 * decides whether to actually drop the conversion based on the
 * preserved range). */
/* i2s/i2b/s2b narrow an i32 to 16/8 bits — the result is i32-typed. */
static cp_const_t gamma_range_fold_narrow(cp_const_t a, int32_t tgt_lo, int32_t tgt_hi) {
    cp_const_t g;
    if (cp_gamma_gate1(a, &g)) return g;
    int64_t lo, hi;
    cp_range_bounds(a, &lo, &hi);
    if (lo >= tgt_lo && hi <= tgt_hi) return a;  /* fits — pass through (stride preserved by identity) */
    return cp_range_make(tgt_lo, tgt_hi, CP_W_I32);  /* truncated — full target range */
}

static cp_const_t gamma_range_fold_i2s(cp_const_t a) { return gamma_range_fold_narrow(a, INT16_MIN, INT16_MAX); }
static cp_const_t gamma_range_fold_i2c(cp_const_t a) { return gamma_range_fold_narrow(a, 0, 65535); }
static cp_const_t gamma_range_fold_i2b(cp_const_t a) { return gamma_range_fold_narrow(a, INT8_MIN,  INT8_MAX);  }
static cp_const_t gamma_range_fold_s2b(cp_const_t a) { return gamma_range_fold_narrow(a, INT8_MIN,  INT8_MAX);  }
static cp_const_t gamma_range_fold_s2i(cp_const_t a) { return a; /* widening conversion: range exact */ }

/* CMP with range inputs: when the ranges are disjoint, fold to a
 * KNOWN 0/1 per op. Overlapping ranges → BOTTOM (no claim). Stride
 * (Click §4.5) tightens EQ/NE: a singleton k that lies inside a
 * strided range but isn't on a stride boundary is provably not in
 * the value set; two strided ranges with the same stride but
 * disagreeing bases mod stride have empty intersection. */
static cp_const_t gamma_range_fold_cmp(int op, cp_const_t a, cp_const_t b) {  /* op = node tag */
    cp_const_t g;
    if (cp_gamma_gate2(a, b, &g)) return g;   /* TOP dominates; BOTTOM/REF ⟹ no claim */
    int64_t a_lo, a_hi, b_lo, b_hi;
    cp_range_bounds(a, &a_lo, &a_hi);
    cp_range_bounds(b, &b_lo, &b_hi);
    int64_t a_s = cp_range_stride(a);
    int64_t b_s = cp_range_stride(b);
    /* Strict orderings between ranges. */
    bool a_lt_b = a_hi < b_lo;   /* every a < every b */
    bool a_gt_b = a_lo > b_hi;   /* every a > every b */
    bool a_le_b = a_hi <= b_lo;  /* every a ≤ every b — note overlap at boundary */
    bool a_ge_b = a_lo >= b_hi;
    /* Equality: both must be the same singleton. */
    bool eq_known = (a_lo == a_hi && b_lo == b_hi && a_lo == b_lo);
    bool ne_known = a_lt_b || a_gt_b;  /* disjoint intervals → not equal */
    /* Strided disjointness within overlapping intervals. KNOWN-vs-RANGE
     * with stride > 1: the singleton must land on a stride boundary
     * to be in the value set. Two RANGEs with the same stride and
     * disagreeing bases (mod stride) have empty intersection. */
    if (!ne_known) {
        if (a_lo == a_hi && b_s > 1) {
            /* a is singleton, b is strided */
            int64_t off = ((a_lo - b_lo) % b_s + b_s) % b_s;
            if (off != 0) ne_known = true;
        } else if (b_lo == b_hi && a_s > 1) {
            int64_t off = ((b_lo - a_lo) % a_s + a_s) % a_s;
            if (off != 0) ne_known = true;
        } else if (a_s > 1 && b_s > 1) {
            /* Any common value satisfies a_lo + i·a_s = b_lo + j·b_s, so
             * a_lo ≡ b_lo (mod gcd(a_s, b_s)); a disagreeing base residue
             * is an empty intersection. Equal strides are the gcd = a_s
             * special case. An overflowing base difference claims nothing. */
            int64_t d = cp_gamma_gcd(a_s, b_s), diff;
            if (d > 1 && !__builtin_sub_overflow(a_lo, b_lo, &diff)) {
                int64_t off = (diff % d + d) % d;
                if (off != 0) ne_known = true;
            }
        }
    }
    int32_t result = -1;  /* sentinel: not foldable */
    switch (op) {
        case SIR_EQ: if (eq_known) result = 1; else if (ne_known) result = 0; break;
        case SIR_NE: if (eq_known) result = 0; else if (ne_known) result = 1; break;
        case SIR_LT: if (a_lt_b) result = 1; else if (a_ge_b) result = 0; break;
        case SIR_GE: if (a_ge_b) result = 1; else if (a_lt_b) result = 0; break;
        case SIR_GT: if (a_gt_b) result = 1; else if (a_le_b) result = 0; break;
        case SIR_LE: if (a_le_b) result = 1; else if (a_gt_b) result = 0; break;
    }
    if (result < 0) return (cp_const_t){ .state = CP_C_BOTTOM };
    return (cp_const_t){ .state = CP_C_KNOWN, .value = result };
}

/* ── §3.2.1 γ_T — per-opcode type accessors ─────────────────────────
 * Each helper reads the SIR node's carried data_type or class_id from
 * the appropriate union arm. The struct layouts are sir.asdl-shape
 * facts (which field the opcode places its data_type / class_id in);
 * γ takes them and produces a Type. */

static sir_datatype_t gamma_dt_arith     (const struct sir_node_t* e) { return e->add.data_type; }
static sir_datatype_t gamma_dt_neg       (const struct sir_node_t* e) { return e->neg.data_type; }
static sir_datatype_t gamma_dt_lognot    (const struct sir_node_t* e) { return e->log_not.data_type; }
static sir_datatype_t gamma_dt_load_const(const struct sir_node_t* e) { return e->load_const.data_type; }

static int gamma_class_load_this    (const struct sir_node_t* e) { return e->load_this.class_id; }
static int gamma_class_new          (const struct sir_node_t* e) { return e->new_.class_id; }
static int gamma_class_new_ref_array(const struct sir_node_t* e) { return e->new_ref_array.class_id; }

/* ── Java-type → lattice-Type translation ──────────────────────────
 * Walks the array nesting and yields ARRAY(dim, class_id) for
 * arrays-of-classes, REF(class_id) for bare classes, PRIM(width) for
 * bare primitives, PRIM_ARRAY(dim, width) for primitive-element
 * arrays (the base widths route through lat_tag_to_dt — the ONE
 * tag→width authority), and BOTTOM for anything the lattice doesn't
 * represent (void, the declared null-type case among them). */
const Type* gamma_jt_to_type(java_type_t t, type_pool_t* pool) {
    int dim = 0;
    while (t.tag == JT_ARRAY && t.element) { dim++; t = *t.element; }
    switch (t.tag) {
    case JT_CLASS:
        return dim == 0 ? type_make_ref(pool, t.class_id)
                        : type_make_array(pool, dim, t.class_id);
    case JT_BOOL: case JT_BYTE: case JT_SHORT: case JT_CHAR:
    case JT_INT:  case JT_LONG: case JT_FLOAT: case JT_DOUBLE:
        return dim == 0 ? type_make_prim(pool, lat_tag_to_dt(t.tag))
                        : type_make_prim_array(pool, dim, lat_tag_to_dt(t.tag));
    default:
        return type_bottom(pool);
    }
}

/* Threaded ref-descriptor node (ClassRef / ArrayRef / PrimArray) → the
 * interned lattice Type. Referent IDENTITY is Type-pointer equality —
 * the one representation authority; consumers never invent their own
 * descriptor keys. BOTTOM when absent / not a descriptor node. */
const Type* gamma_ref_to_type(const struct sir_node_t* ref, type_pool_t* pool) {
    if (!ref) return type_bottom(pool);
    switch (ref->tag) {
        case SIR_CLASSREF:  return type_make_ref(pool, ref->class_ref.class_id);
        case SIR_ARRAYREF:  return type_make_array(pool, ref->array_ref.dim,
                                                   ref->array_ref.class_id);
        case SIR_PRIMARRAY: return type_make_prim_array(pool, ref->prim_array.dim,
                                                        ref->prim_array.width);
        default:            return type_bottom(pool);
    }
}

/* ── §3.2.1 γ_T sema-driven accessors ──────────────────────────────
 * INVOKE* return type and GETFIELD / GETSTATIC field type come from
 * sema's class table, indexed by the opcode's carried class_id +
 * method_idx / field_idx. Each helper reads its opcode's struct
 * overlay and returns the lattice Type, falling back to BOTTOM if
 * sema is unavailable or the index is out of range. */

static const Type* gamma_sema_invoke_virtual(const sema_ctx_t* sema,
                                              const struct sir_node_t* e,
                                              type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->invoke_virtual.class_id;
    int mi = e->invoke_virtual.method_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || mi < 0 || mi >= (int)bbq_vec_len((void*)cls->methods))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->methods[mi].return_type, pool);
}

static const Type* gamma_sema_invoke_special(const sema_ctx_t* sema,
                                              const struct sir_node_t* e,
                                              type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->invoke_special.class_id;
    int mi = e->invoke_special.method_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || mi < 0 || mi >= (int)bbq_vec_len((void*)cls->methods))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->methods[mi].return_type, pool);
}

static const Type* gamma_sema_invoke_static(const sema_ctx_t* sema,
                                             const struct sir_node_t* e,
                                             type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->invoke_static.class_id;
    int mi = e->invoke_static.method_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || mi < 0 || mi >= (int)bbq_vec_len((void*)cls->methods))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->methods[mi].return_type, pool);
}

static const Type* gamma_sema_invoke_interface(const sema_ctx_t* sema,
                                                const struct sir_node_t* e,
                                                type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->invoke_interface.class_id;
    int mi = e->invoke_interface.method_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || mi < 0 || mi >= (int)bbq_vec_len((void*)cls->methods))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->methods[mi].return_type, pool);
}

/* Partition-init bucket discriminators: the field a load reads is an
 * operator immediate — (class_id, field_idx) packed exactly, so loads
 * of different fields never start value-congruent. */
static uint32_t gamma_bucket_get_field(const struct sir_node_t* e) {
    return ((uint32_t)e->get_field.class_id << 16)
         | ((uint32_t)e->get_field.field_idx & 0xFFFF);
}
static uint32_t gamma_bucket_get_static(const struct sir_node_t* e) {
    return ((uint32_t)e->get_static.class_id << 16)
         | ((uint32_t)e->get_static.field_idx & 0xFFFF);
}

static const Type* gamma_sema_get_field(const sema_ctx_t* sema,
                                         const struct sir_node_t* e,
                                         type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->get_field.class_id;
    int fi = e->get_field.field_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || fi < 0 || fi >= (int)bbq_vec_len((void*)cls->fields))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->fields[fi].type, pool);
}

static const Type* gamma_sema_get_static(const sema_ctx_t* sema,
                                          const struct sir_node_t* e,
                                          type_pool_t* pool) {
    if (!sema) return type_bottom(pool);
    int cls_id = e->get_static.class_id;
    int fi = e->get_static.field_idx;
    const sema_class_t* cls = sema_get_class(sema, cls_id);
    if (!cls || fi < 0 || fi >= (int)bbq_vec_len((void*)cls->fields))
        return type_bottom(pool);
    return gamma_jt_to_type(cls->fields[fi].type, pool);
}

/* Table-driven γ_T dispatch for nodes whose type depends only on the
 * node's carried fields plus sema. GT_VIA_INPUT is caller-specific
 * (the engine reads vnode input types; test fixtures read their own
 * slot tables) — the caller handles it before invoking this helper.
 * CHECKCAST's atype discriminator dispatch lives here because it's a
 * per-node field read, same flavor as the other row-driven cases. */
const Type* gamma_type_for_node(const sema_ctx_t* sema,
                                 const struct sir_node_t* e,
                                 type_pool_t* pool) {
    if (!e || e->tag < 0 || e->tag >= SIR_TAG_COUNT)
        return type_bottom(pool);
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];

    if (g->type_kind == GT_CHECKCAST)
        return (e->check_cast.atype == SIR_ATCLASS)
             ? type_make_ref(pool, e->check_cast.class_id)
             : type_bottom(pool);

    switch (g->type_kind) {
        case GT_PRIM_DT:    return type_make_prim (pool, g->type_prim_dt(e));
        case GT_PRIM_FIXED: return type_make_prim (pool, g->type_prim_fixed_dt);
        case GT_REF:        return type_make_ref  (pool, g->type_class_id(e));
        case GT_ARRAY:      return type_make_array(pool, 1, g->type_class_id(e));
        case GT_NULL:       return type_null(pool);
        case GT_SEMA:
            return (g->type_sema && sema)
                 ? g->type_sema(sema, e, pool)
                 : type_bottom(pool);
        case GT_ARRAY_ELEM: {
            sir_datatype_t dt = e->array_load.data_type;
            return (dt == SIR_DTREF)
                 ? type_bottom(pool)
                 : type_make_prim(pool, dt);
        }
        case GT_PRIM_ARRAY: {
            /* NewArray's primitive element kind → dim-1 array of the
             * lattice's atype→dt width (boolean packs as byte). A ref
             * element atype never reaches here (NewRefArray is GT_ARRAY). */
            sir_datatype_t width = lat_atype_to_dt(e->new_array.elem_type);
            return (width == SIR_DTREF)
                 ? type_bottom(pool)
                 : type_make_prim_array(pool, 1, width);
        }
        case GT_VIA_INPUT:  /* caller handles */
        case GT_NONE:
        case GT_BOTTOM:
        case GT_CHECKCAST:  /* handled above */
        default:            return type_bottom(pool);
    }
}

const sir_op_gamma_t sir_op_gamma[SIR_TAG_COUNT] = {
    /* ── Value-bearing leaves ──────────────────────────────── *
     * is_leaf_pure: LoadConst/Null/Local/This are unconditionally pure
     * (no side effects, no allocations). is_congruent: pure values can
     * share an initial partition bucket per §4.4 (load constants /
     * locals / `this` are all candidates the engine can refine). */
    [SIR_LOADCONST]      = { .tag = SIR_LOADCONST,      .mnemonic = "loadconst",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_load_const },
    /* The wide-constant leaves (full Java 1.0). Pure leaves of fixed width;
     * they carry int64/float/double values the int32 cp lattice can't fold,
     * so no fold hook — sound (treated as opaque / BOTTOM there). */
    [SIR_LOADLONGCONST]   = { .tag = SIR_LOADLONGCONST,   .mnemonic = "loadlongconst",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTLONG },
    [SIR_LOADFLOATCONST]  = { .tag = SIR_LOADFLOATCONST,  .mnemonic = "loadfloatconst",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTFLOAT },
    [SIR_LOADDOUBLECONST] = { .tag = SIR_LOADDOUBLECONST, .mnemonic = "loaddoubleconst",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTDOUBLE },
    [SIR_LOADNULL]       = { .tag = SIR_LOADNULL,       .mnemonic = "loadnull",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_NULL },
    [SIR_LOADLOCAL]      = { .tag = SIR_LOADLOCAL,      .mnemonic = "loadlocal",
                             .is_congruent = true, .is_leaf_pure = true,
                             /* §4.7 COPY-Follower — type is reaching-def's type;
                              * engine-special arm in cp_node_type. */
                             .type_kind = GT_VIA_INPUT },
    [SIR_LOADTHIS]       = { .tag = SIR_LOADTHIS,       .mnemonic = "loadthis",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_REF, .type_class_id = gamma_class_load_this },
    /* LoadClass(K) — K's Class-object singleton, a pure congruent leaf. Its value type
     * is (ref Class), but a γ fn can't name Class's id here (no sema) and Click doesn't
     * run in the WASM backend where LoadClass lives → BOTTOM (precision gap, safe). */
    [SIR_LOADCLASS]      = { .tag = SIR_LOADCLASS,      .mnemonic = "loadclass",
                             .is_congruent = true, .is_leaf_pure = true,
                             .type_kind = GT_BOTTOM },
    [SIR_CLONECOPY]      = { .tag = SIR_CLONECOPY,      .mnemonic = "clonecopy",
                             .type_kind = GT_BOTTOM },

    /* ── Integer arithmetic ────────────────────────────────── *
     * ADD / MUL are commutative; SUB / DIV / REM / NEG are not.
     * DIV / REM can throw — not is_pure_if_children_pure (impure). */
    [SIR_ADD]            = { .tag = SIR_ADD,            .mnemonic = "add",
                             .is_commutative = true, .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_add,
                             .fold_binary_range = gamma_range_fold_add,
                             /* §4.8: x + 0 = x */
                             .identity_side = GS_EITHER, .identity_k = 0 },
    [SIR_SUB]            = { .tag = SIR_SUB,            .mnemonic = "sub",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_sub,
                             .fold_binary_range = gamma_range_fold_sub,
                             /* §4.8: x - 0 = x (right side only — not commutative) */
                             .identity_side = GS_RIGHT, .identity_k = 0,
                             /* §4.6: x - x = 0 when operands are partition-congruent */
                             .cong_fold = GC_ZERO },
    [SIR_MUL]            = { .tag = SIR_MUL,            .mnemonic = "mul",
                             .is_commutative = true, .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_mul,
                             .fold_binary_range = gamma_range_fold_mul,
                             /* §4.8: x * 1 = x */
                             .identity_side = GS_EITHER, .identity_k = 1,
                             /* absorbing: x * 0 = 0 (when x pure) */
                             .absorbing_side = GS_EITHER,
                             .absorbing_k = 0, .absorbing_result = 0 },
    [SIR_DIV]            = { .tag = SIR_DIV,            .mnemonic = "div",
                             .arity = 2, .fold_binary = gamma_fold_div,
                             .fold_binary_range = gamma_range_fold_div,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             /* §4.8: x / 1 = x */
                             .identity_side = GS_RIGHT, .identity_k = 1 },
    [SIR_REM]            = { .tag = SIR_REM,            .mnemonic = "rem",
                             .arity = 2, .fold_binary = gamma_fold_rem,
                             .fold_binary_range = gamma_range_fold_rem,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith },
    [SIR_NEG]            = { .tag = SIR_NEG,            .mnemonic = "neg",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_neg,
                             .fold_unary = gamma_fold_neg,
                             .fold_unary_range = gamma_range_fold_neg },

    /* ── Bitwise and shifts ────────────────────────────────── *
     * AND / OR / XOR are commutative; SHL / SHR / USHR are not. All
     * pure-if-children-pure (no traps). */
    [SIR_AND]            = { .tag = SIR_AND,            .mnemonic = "and",
                             .is_commutative = true, .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_and,
                             .fold_binary_range = gamma_range_fold_and,
                             /* §4.8: x & x = x (idempotent) */
                             .same_in_follower = true,
                             /* §4.8: x & -1 = x (all-ones identity) */
                             .identity_side = GS_EITHER, .identity_k = -1,
                             /* absorbing: x & 0 = 0 (when x pure) */
                             .absorbing_side = GS_EITHER,
                             .absorbing_k = 0, .absorbing_result = 0 },
    [SIR_OR]             = { .tag = SIR_OR,             .mnemonic = "or",
                             .is_commutative = true, .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_or,
                             /* §4.8: x | x = x (idempotent) */
                             .same_in_follower = true,
                             /* §4.8: x | 0 = x */
                             .identity_side = GS_EITHER, .identity_k = 0,
                             /* absorbing: x | -1 = -1 (when x pure) */
                             .absorbing_side = GS_EITHER,
                             .absorbing_k = -1, .absorbing_result = -1 },
    [SIR_XOR]            = { .tag = SIR_XOR,            .mnemonic = "xor",
                             .is_commutative = true, .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_xor,
                             /* §4.8: x ^ 0 = x */
                             .identity_side = GS_EITHER, .identity_k = 0,
                             /* §4.6: x ^ x = 0 when operands are partition-congruent */
                             .cong_fold = GC_ZERO },
    [SIR_SHL]            = { .tag = SIR_SHL,            .mnemonic = "shl",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_shl,
                             .fold_binary_range = gamma_range_fold_shl,
                             /* §4.8: x << 0 = x (right side only — not commutative) */
                             .identity_side = GS_RIGHT, .identity_k = 0 },
    [SIR_SHR]            = { .tag = SIR_SHR,            .mnemonic = "shr",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_shr,
                             .identity_side = GS_RIGHT, .identity_k = 0 },
    [SIR_USHR]           = { .tag = SIR_USHR,           .mnemonic = "ushr",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 2,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_arith,
                             .fold_binary = gamma_fold_ushr,
                             .identity_side = GS_RIGHT, .identity_k = 0 },

    /* ── Logical and conversions ───────────────────────────── */
    [SIR_LOGNOT]         = { .tag = SIR_LOGNOT,         .mnemonic = "lognot",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_DT, .type_prim_dt = gamma_dt_lognot,
                             .fold_unary = gamma_fold_lognot },
    [SIR_S2B]            = { .tag = SIR_S2B,            .mnemonic = "s2b",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                             .fold_unary = gamma_fold_s2b,
                             .fold_unary_range = gamma_range_fold_s2b },
    [SIR_S2I]            = { .tag = SIR_S2I,            .mnemonic = "s2i",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTINT,
                             .fold_unary = gamma_fold_s2i,
                             .fold_unary_range = gamma_range_fold_s2i },
    [SIR_I2S]            = { .tag = SIR_I2S,            .mnemonic = "i2s",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTSHORT,
                             .fold_unary = gamma_fold_i2s,
                             .fold_unary_range = gamma_range_fold_i2s },
    [SIR_I2B]            = { .tag = SIR_I2B,            .mnemonic = "i2b",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                             .fold_unary = gamma_fold_i2b,
                             .fold_unary_range = gamma_range_fold_i2b },

    /* ── §5.1.2/§5.1.3 width-changing conversions ────────────── *
     * Each is pure, congruent (same conversion of congruent operands
     * is congruent), arity 1, with a fixed result type and a
     * width-changing fold_convert (the KNOWN fold; no range fold). */
#define CONV_ROW(TAG, MN, DT, FN) \
    [TAG] = { .tag = TAG, .mnemonic = MN, .is_congruent = true, \
              .is_pure_if_children_pure = true, .arity = 1, \
              .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = DT, \
              .fold_convert = FN }
    /* I2C is hand-written (not CONV_ROW): char stays in the i32 carrier, so
     * it takes the same fold_unary + range-fold pair as I2S/I2B — a
     * fold_convert row would eat RANGE operands to BOTTOM before the range
     * dispatch ever saw them (the parity-audit gap). */
    [SIR_I2C]            = { .tag = SIR_I2C,            .mnemonic = "i2c",
                             .is_congruent = true,
                             .is_pure_if_children_pure = true, .arity = 1,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTCHAR,
                             .fold_unary = gamma_fold_i2c,
                             .fold_unary_range = gamma_range_fold_i2c },
    CONV_ROW(SIR_I2L, "i2l", SIR_DTLONG,   gamma_conv_i2l),
    CONV_ROW(SIR_I2F, "i2f", SIR_DTFLOAT,  gamma_conv_i2f),
    CONV_ROW(SIR_I2D, "i2d", SIR_DTDOUBLE, gamma_conv_i2d),
    CONV_ROW(SIR_L2I, "l2i", SIR_DTINT,    gamma_conv_l2i),
    CONV_ROW(SIR_L2F, "l2f", SIR_DTFLOAT,  gamma_conv_l2f),
    CONV_ROW(SIR_L2D, "l2d", SIR_DTDOUBLE, gamma_conv_l2d),
    CONV_ROW(SIR_F2I, "f2i", SIR_DTINT,    gamma_conv_f2i),
    CONV_ROW(SIR_F2L, "f2l", SIR_DTLONG,   gamma_conv_f2l),
    CONV_ROW(SIR_F2D, "f2d", SIR_DTDOUBLE, gamma_conv_f2d),
    CONV_ROW(SIR_D2I, "d2i", SIR_DTINT,    gamma_conv_d2i),
    CONV_ROW(SIR_D2L, "d2l", SIR_DTLONG,   gamma_conv_d2l),
    CONV_ROW(SIR_D2F, "d2f", SIR_DTFLOAT,  gamma_conv_d2f),
    CONV_ROW(SIR_MOVEF2I, "movef2i", SIR_DTINT,    gamma_move_f2i),
    CONV_ROW(SIR_MOVEI2F, "movei2f", SIR_DTFLOAT,  gamma_move_i2f),
    CONV_ROW(SIR_MOVED2L, "moved2l", SIR_DTLONG,   gamma_move_d2l),
    CONV_ROW(SIR_MOVEL2D, "movel2d", SIR_DTDOUBLE, gamma_move_l2d),
    CONV_ROW(SIR_F64SQRT,    "f64sqrt",    SIR_DTDOUBLE, gamma_f64sqrt),
    CONV_ROW(SIR_F64FLOOR,   "f64floor",   SIR_DTDOUBLE, gamma_f64floor),
    CONV_ROW(SIR_F64CEIL,    "f64ceil",    SIR_DTDOUBLE, gamma_f64ceil),
    CONV_ROW(SIR_F64NEAREST, "f64nearest", SIR_DTDOUBLE, gamma_f64nearest),
#undef CONV_ROW

    /* ── WASM v128 SIMD families — pure, NO folds (a wrong fold is a
     * miscompile; folding needs its own oracle). Identity = tag + an EXACT
     * discriminator: the `op` payload, or (op<<5)|lane for extract/replace
     * (lossless in 32 bits — WOP_* fits 16, lane < 32). SimdConst /
     * SimdShuffle carry 128 payload bits no exact 32-bit discriminator can
     * hold, and a HASHED bucket would MERGE two different constants forever
     * (equal-input nodes can never be split back by refinement) — so they
     * are NOT congruent: singleton partitions, no CSE, no wrong merge. */
#define SIMD_ROW(TAG, MN, AR, DT, DISC) \
    [TAG] = { .tag = TAG, .mnemonic = MN, .is_congruent = true, \
              .is_pure_if_children_pure = true, .arity = AR, \
              .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = DT, \
              .bucket_discriminator = DISC }
    SIMD_ROW(SIR_SIMDBIN,      "simdbin",      2, SIR_DTV128,   gamma_simd_disc_bin),
    SIMD_ROW(SIR_SIMDUN,       "simdun",       1, SIR_DTV128,   gamma_simd_disc_un),
    SIMD_ROW(SIR_SIMDSHIFT,    "simdshift",    2, SIR_DTV128,   gamma_simd_disc_shift),
    SIMD_ROW(SIR_SIMDTERN,     "simdtern",     3, SIR_DTV128,   gamma_simd_disc_tern),
    SIMD_ROW(SIR_SIMDTESTI,    "simdtesti",    1, SIR_DTINT,    gamma_simd_disc_test),
    SIMD_ROW(SIR_SIMDSPLATI,   "simdsplati",   1, SIR_DTV128,   gamma_simd_disc_splat_i),
    SIMD_ROW(SIR_SIMDSPLATL,   "simdsplatl",   1, SIR_DTV128,   gamma_simd_disc_splat_l),
    SIMD_ROW(SIR_SIMDSPLATF,   "simdsplatf",   1, SIR_DTV128,   gamma_simd_disc_splat_f),
    SIMD_ROW(SIR_SIMDSPLATD,   "simdsplatd",   1, SIR_DTV128,   gamma_simd_disc_splat_d),
    SIMD_ROW(SIR_SIMDEXTRACTI, "simdextracti", 1, SIR_DTINT,    gamma_simd_disc_extract_i),
    SIMD_ROW(SIR_SIMDEXTRACTL, "simdextractl", 1, SIR_DTLONG,   gamma_simd_disc_extract_l),
    SIMD_ROW(SIR_SIMDEXTRACTF, "simdextractf", 1, SIR_DTFLOAT,  gamma_simd_disc_extract_f),
    SIMD_ROW(SIR_SIMDEXTRACTD, "simdextractd", 1, SIR_DTDOUBLE, gamma_simd_disc_extract_d),
    SIMD_ROW(SIR_SIMDREPLACEI, "simdreplacei", 2, SIR_DTV128,   gamma_simd_disc_replace_i),
    SIMD_ROW(SIR_SIMDREPLACEL, "simdreplacel", 2, SIR_DTV128,   gamma_simd_disc_replace_l),
    SIMD_ROW(SIR_SIMDREPLACEF, "simdreplacef", 2, SIR_DTV128,   gamma_simd_disc_replace_f),
    SIMD_ROW(SIR_SIMDREPLACED, "simdreplaced", 2, SIR_DTV128,   gamma_simd_disc_replace_d),
#undef SIMD_ROW
    [SIR_SIMDCONST]   = { .tag = SIR_SIMDCONST,   .mnemonic = "simdconst",
                          .is_leaf_pure = true,   /* pure, NOT congruent — see block comment */
                          .arity = 0,
                          .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTV128 },
    [SIR_SIMDSHUFFLE] = { .tag = SIR_SIMDSHUFFLE, .mnemonic = "simdshuffle",
                          .is_pure_if_children_pure = true,   /* pure, NOT congruent */
                          .arity = 2,
                          .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTV128 },

    /* ── Compare ───────────────────────────────────────────── *
     * CMP's commutativity and bucket-init depend on e->cmp.op (only
     * EQ / NE are commutative; each op gets its own initial bucket). */
    /* One row per comparison node (Click §3.8: the tag is the operator, so
     * each is its own congruence class — no bucket discriminator). EQ/NE are
     * commutative; all six fold by tag and carry the §4.6 reflexive x⊙x fold. */
    /* A comparison's value is boolean (§15.20/§15.21) — the compiler's
     * JT_BOOL→byte convention (lat_tag_to_dt), matching the slot dt a
     * boolean store gives it. */
    [SIR_EQ] = { .tag = SIR_EQ, .mnemonic = "eq", .is_commutative = true,
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },
    [SIR_NE] = { .tag = SIR_NE, .mnemonic = "ne", .is_commutative = true,
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },
    [SIR_LT] = { .tag = SIR_LT, .mnemonic = "lt",
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },
    [SIR_LE] = { .tag = SIR_LE, .mnemonic = "le",
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },
    [SIR_GT] = { .tag = SIR_GT, .mnemonic = "gt",
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },
    [SIR_GE] = { .tag = SIR_GE, .mnemonic = "ge",
                 .is_congruent = true, .is_pure_if_children_pure = true, .arity = 2,
                 .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE,
                 .fold_cmp = gamma_fold_cmp,
                 .fold_cmp_range = gamma_range_fold_cmp, .cong_fold = GC_CMP_REFLEXIVE },

    /* ── Object and type ───────────────────────────────────── *
     * NEW allocates (effectful); INSTANCEOF is pure-if-children-pure
     * (it walks the type table but its result depends only on its
     * input ref and the discriminator); CHECKCAST can throw (impure). */
    [SIR_NEW]            = { .tag = SIR_NEW,            .mnemonic = "new",
                             .type_kind = GT_REF, .type_class_id = gamma_class_new },
    [SIR_INSTANCEOF]     = { .tag = SIR_INSTANCEOF,     .mnemonic = "instanceof",
                             .is_pure_if_children_pure = true, .arity = 1,
                             /* boolean (§15.20.2) — byte convention */
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE },
    [SIR_CHECKCAST]      = { .tag = SIR_CHECKCAST,      .mnemonic = "checkcast",
                             .arity = 1,
                             /* discriminator-dependent — engine-special arm */
                             .type_kind = GT_CHECKCAST },

    /* ── Array ─────────────────────────────────────────────── *
     * All array ops can null-deref / OOB-throw — not is_pure_if_children_pure. */
    [SIR_NEWARRAY]       = { .tag = SIR_NEWARRAY,       .mnemonic = "newarray",
                             .type_kind = GT_PRIM_ARRAY },
    [SIR_NEWREFARRAY]    = { .tag = SIR_NEWREFARRAY,    .mnemonic = "newrefarray",
                             .type_kind = GT_ARRAY, .type_class_id = gamma_class_new_ref_array },
    /* ArrayLoad: PRIM of element width for non-ref element types;
     * BOTTOM for ref-element arrays (the lattice would need an
     * element class_id the opcode doesn't carry). */
    /* ArrayLoad / GetField / GetStatic are congruent under the
     * partition engine ONCE the memory input has been wired
     * (cp_resolve_memory). The memory input is the reaching writer
     * of the cell at the consuming spine point — two reads with the
     * same (obj/arr, cell, reaching writer) end up in the same
     * partition and collapse via the §4.7 follower rule. Without
     * the memory input the congruence would be unsound (no way to
     * distinguish reads across intervening writes); cp_enum_expr's
     * extra input slot + cp_resolve_memory together provide it. */
    [SIR_ARRAYLOAD]      = { .tag = SIR_ARRAYLOAD,      .mnemonic = "arrayload",
                             .is_congruent = true,
                             .type_kind = GT_ARRAY_ELEM },
    [SIR_ARRAYLENGTH]    = { .tag = SIR_ARRAYLENGTH,    .mnemonic = "arraylength",
                             /* arr.length is int, and it is FINAL (§10.7): the length
                              * is a pure function of the array reference alone — no
                              * memory input, and two reads of the same array are the
                              * same value. That congruence is what lets a bounds guard
                              * `i >= a.length` see the loop's `i < a.length`. */
                             .is_congruent = true,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTINT },
    /* Linear-memory loads — mutable reads: NEVER congruent (two loads of one
     * address may see different bytes). MemSize/MemGrow are value ops with
     * the same posture (grow MUTATES the size). */
    [SIR_SIMDMEMLOAD]    = { .tag = SIR_SIMDMEMLOAD,    .mnemonic = "simdmemload",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTV128 },
    [SIR_SIMDMEMLOADLANE]= { .tag = SIR_SIMDMEMLOADLANE,.mnemonic = "simdmemloadlane",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTV128 },
    [SIR_MEMLOADI]       = { .tag = SIR_MEMLOADI,       .mnemonic = "memloadi",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTINT },
    [SIR_MEMLOADL]       = { .tag = SIR_MEMLOADL,       .mnemonic = "memloadl",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTLONG },
    [SIR_MEMLOADF]       = { .tag = SIR_MEMLOADF,       .mnemonic = "memloadf",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTFLOAT },
    [SIR_MEMLOADD]       = { .tag = SIR_MEMLOADD,       .mnemonic = "memloadd",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTDOUBLE },
    [SIR_MEMSIZE]        = { .tag = SIR_MEMSIZE,        .mnemonic = "memsize",
                             /* CONGRUENT — soundly, because a MemSize read carries the
                              * memsize CELL's reaching writer as its memory input (the
                              * Gate-5 follower rule): two reads merge only between kills
                              * (MemGrow, calls). This is what lets two Mem bounds guards
                              * over one region become one. */
                             .is_congruent = true,
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTINT },
    [SIR_MEMGROW]        = { .tag = SIR_MEMGROW,        .mnemonic = "memgrow",
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTINT },

    /* §20.3.6. Reading a Class's factory funcref can null-deref (the Class ref), so neither is
     * pure; ClassConstruct also allocates and runs a constructor, so it is not congruent either. */
    [SIR_CLASSINSTANTIABLE] = { .tag = SIR_CLASSINSTANTIABLE, .mnemonic = "classinstantiable",
                             .arity = 1, .is_congruent = true,
                             /* boolean predicate — byte convention */
                             .type_kind = GT_PRIM_FIXED, .type_prim_fixed_dt = SIR_DTBYTE },
    [SIR_CLASSCONSTRUCT] = { .tag = SIR_CLASSCONSTRUCT, .mnemonic = "classconstruct",
                             .arity = 1,
                             .type_kind = GT_BOTTOM },

    /* ── Field ─────────────────────────────────────────────── *
     * GetField can null-deref; GetStatic reads memory — neither pure.
     * Type is the field's declared type via sema's class table.
     * Congruent only per (class, field): the field is an operator
     * immediate, so two loads of different fields compute different
     * functions of the same (obj, memory) inputs — the bucket
     * discriminator keeps them in separate initial partitions. */
    [SIR_GETFIELD]       = { .tag = SIR_GETFIELD,       .mnemonic = "getfield",
                             .is_congruent = true,
                             .bucket_discriminator = gamma_bucket_get_field,
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_get_field },
    [SIR_GETSTATIC]      = { .tag = SIR_GETSTATIC,      .mnemonic = "getstatic",
                             .is_congruent = true,
                             .bucket_discriminator = gamma_bucket_get_static,
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_get_static },

    /* ── Invocation ────────────────────────────────────────── *
     * All side-effectful. Type is the method's declared return type
     * via sema's class table (each variant reads its own struct
     * overlay's class_id + method_idx). */
    [SIR_INVOKEVIRTUAL]  = { .tag = SIR_INVOKEVIRTUAL,  .mnemonic = "invokevirtual",
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_invoke_virtual },
    [SIR_INVOKESPECIAL]  = { .tag = SIR_INVOKESPECIAL,  .mnemonic = "invokespecial",
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_invoke_special },
    [SIR_INVOKESTATIC]   = { .tag = SIR_INVOKESTATIC,   .mnemonic = "invokestatic",
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_invoke_static },
    [SIR_INVOKEINTERFACE]= { .tag = SIR_INVOKEINTERFACE,.mnemonic = "invokeinterface",
                             .type_kind = GT_SEMA, .type_sema = gamma_sema_invoke_interface },

    /* ── Spine (no γ — engine never reads these rows for γ questions) ── */
    [SIR_STORELOCAL]     = { .tag = SIR_STORELOCAL,     .mnemonic = "storelocal" },
    [SIR_EXPREFFECT]     = { .tag = SIR_EXPREFFECT,     .mnemonic = "expreffect" },
    [SIR_ARRAYSTORE]     = { .tag = SIR_ARRAYSTORE,     .mnemonic = "arraystore" },
    [SIR_ARRAYCOPY]      = { .tag = SIR_ARRAYCOPY,      .mnemonic = "arraycopy" },
    [SIR_SIMDMEMSTORE]   = { .tag = SIR_SIMDMEMSTORE,   .mnemonic = "simdmemstore" },      /* v128.store (spine) */
    [SIR_SIMDMEMSTORELANE]={ .tag = SIR_SIMDMEMSTORELANE,.mnemonic = "simdmemstorelane" }, /* v128.storeN_lane (spine) */
    [SIR_MEMSTOREI]      = { .tag = SIR_MEMSTOREI,      .mnemonic = "memstorei" },  /* i32.store/8/16 (spine) */
    [SIR_MEMSTOREL]      = { .tag = SIR_MEMSTOREL,      .mnemonic = "memstorel" },
    [SIR_MEMSTOREF]      = { .tag = SIR_MEMSTOREF,      .mnemonic = "memstoref" },
    [SIR_MEMSTORED]      = { .tag = SIR_MEMSTORED,      .mnemonic = "memstored" },
    [SIR_MEMFILL]        = { .tag = SIR_MEMFILL,        .mnemonic = "memfill" },    /* memory.fill (spine) */
    [SIR_MEMCOPY]        = { .tag = SIR_MEMCOPY,        .mnemonic = "memcopy" },    /* memory.copy (spine) */
    [SIR_PUTFIELD]       = { .tag = SIR_PUTFIELD,       .mnemonic = "putfield" },
    [SIR_SETHEADER]      = { .tag = SIR_SETHEADER,      .mnemonic = "setheader" },
    [SIR_PUTSTATIC]      = { .tag = SIR_PUTSTATIC,      .mnemonic = "putstatic" },
    [SIR_BRANCH]         = { .tag = SIR_BRANCH,         .mnemonic = "branch" },
    [SIR_SWITCH]         = { .tag = SIR_SWITCH,         .mnemonic = "switch" },
    [SIR_RETURN]         = { .tag = SIR_RETURN,         .mnemonic = "return" },
    [SIR_RETURNVOID]     = { .tag = SIR_RETURNVOID,     .mnemonic = "returnvoid" },
    [SIR_THROW]          = { .tag = SIR_THROW,          .mnemonic = "throw" },
    [SIR_EXCEPTIONENTRY] = { .tag = SIR_EXCEPTIONENTRY, .mnemonic = "exceptionentry" },
    [SIR_TRYREGION]      = { .tag = SIR_TRYREGION,      .mnemonic = "tryregion" },
    [SIR_INC]            = { .tag = SIR_INC,            .mnemonic = "inc" },
    [SIR_NOP]            = { .tag = SIR_NOP,            .mnemonic = "nop" },
    /* Reference-type descriptor leaves — pure type metadata, never value-folded
     * by Click (they carry no operands), but every tag needs a row so
     * sir_op_gamma[tag] is total. */
    [SIR_CLASSREF]       = { .tag = SIR_CLASSREF,       .mnemonic = "classref" },
    [SIR_ARRAYREF]       = { .tag = SIR_ARRAYREF,       .mnemonic = "arrayref" },
    [SIR_PRIMARRAY]      = { .tag = SIR_PRIMARRAY,      .mnemonic = "primarray" },
};
