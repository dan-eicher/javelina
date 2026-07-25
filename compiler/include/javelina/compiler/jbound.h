// jbound.h — Java integer BOUNDS arithmetic, the one bound-arithmetic core.
//
// The twin of jint.h. jint.h answers "what is a op b" for exact values; this
// answers "what interval contains a op b" when a and b are only known to lie in
// intervals. Both encode the same JLS 1.0 (1997) algebra — verified against
// java-langspec-1.0.pdf, not from memory:
//   §15.16.1  *   an integer product is "the low-order bits of the mathematical
//                 product". A wrapped product is NOT an interval of the operands'
//                 corner products, so a corner that leaves the type's width makes
//                 the claim unrepresentable — see the overflow contract below.
//   §15.16.2  /   "Integer division rounds toward 0" — which is what makes the
//                 four corner quotients bound the result, once the two cases the
//                 spec carves out are excluded: divisor 0 ("an ArithmeticException
//                 is thrown") and MIN/-1 ("integer overflow occurs and the result
//                 is equal to the dividend").
//   §15.16.3  %   the sign follows the DIVIDEND and |r| < |divisor|.
//   §15.17.2  + - "the low-order bits" — the same wrap rule as *.
//   §15.18    shift: only the low 5 (int) / 6 (long) bits of the distance are used.
//                 A distance that is not a single in-range value is a MASKED
//                 distance, whose result set is not an interval — no claim.
//   §15.21.1  & ^ | integer bitwise.
//   §15.14.4  unary - : -MIN wraps, so a range whose low end is the width minimum
//                 has no negation interval.
//
// THE OVERFLOW CONTRACT. Two different things can stop a claim, and they belong
// to different owners:
//
//   ok == false      The SPEC leaves no interval to claim: a divisor range that
//                    spans 0, the MIN/-1 corner, a non-singleton shift distance,
//                    a mask that could be negative. Every consumer must honour
//                    these — they are Java semantics, not policy.
//
//   overflow == true The corner arithmetic left i64, or left the width the caller
//                    passed. The RESPONSE is the consumer's policy, which is why
//                    this is reported rather than decided here: the SIR optimizer
//                    forfeits to its lattice's no-fact element (BOTTOM), while the
//                    sema linter saturates to ±∞ and lets its own int32 wrap guard
//                    forfeit to TOP. Same algebra, two policies.
//
// Strides (Click §4.5) are deliberately NOT here: a stride is an analysis
// refinement of an interval, not part of the Java algebra, and only one consumer
// has them. This core answers the spec question; each consumer keeps what is its
// own.
#ifndef JAVELINA_JBOUND_H
#define JAVELINA_JBOUND_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t lo, hi;
    bool    ok;        // false ⇒ the spec leaves no interval to claim
    bool    overflow;  // true  ⇒ a corner left i64 or the caller's width
} jbound_t;

static inline jbound_t jbound_none(void) {
    return (jbound_t){ .lo = 0, .hi = 0, .ok = false, .overflow = false };
}
static inline jbound_t jbound_of(int64_t lo, int64_t hi) {
    return (jbound_t){ .lo = lo, .hi = hi, .ok = true, .overflow = false };
}
static inline jbound_t jbound_over(void) {
    return (jbound_t){ .lo = 0, .hi = 0, .ok = true, .overflow = true };
}
static inline bool jbound_fits(int64_t lo, int64_t hi, int64_t wmin, int64_t wmax) {
    return lo >= wmin && hi <= wmax;
}

static inline int64_t jbound_min2(int64_t a, int64_t b) { return a < b ? a : b; }
static inline int64_t jbound_max2(int64_t a, int64_t b) { return a > b ? a : b; }

// §15.17.2. Monotone in both operands, so the corners are lo+lo and hi+hi.
static inline jbound_t jbound_add(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin, int64_t wmax) {
    int64_t lo, hi;
    if (__builtin_add_overflow(a_lo, b_lo, &lo) || __builtin_add_overflow(a_hi, b_hi, &hi))
        return jbound_over();
    if (!jbound_fits(lo, hi, wmin, wmax)) return jbound_over();
    return jbound_of(lo, hi);
}

// §15.17.2. a - b is least when a is least and b is greatest.
static inline jbound_t jbound_sub(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin, int64_t wmax) {
    int64_t lo, hi;
    if (__builtin_sub_overflow(a_lo, b_hi, &lo) || __builtin_sub_overflow(a_hi, b_lo, &hi))
        return jbound_over();
    if (!jbound_fits(lo, hi, wmin, wmax)) return jbound_over();
    return jbound_of(lo, hi);
}

// §15.14.4: -MIN wraps to MIN, so a range reaching the width minimum straddles
// the unrepresentable result and claims nothing.
static inline jbound_t jbound_neg(int64_t a_lo, int64_t a_hi, int64_t wmin, int64_t wmax) {
    if (a_lo == wmin) return jbound_none();
    int64_t lo, hi;
    if (__builtin_sub_overflow((int64_t)0, a_hi, &lo) ||
        __builtin_sub_overflow((int64_t)0, a_lo, &hi))
        return jbound_over();
    if (!jbound_fits(lo, hi, wmin, wmax)) return jbound_over();
    return jbound_of(lo, hi);
}

// §15.16.1. The four corner products bound the result; any corner that leaves the
// width means the runtime product wrapped, and a wrapped set is not an interval.
static inline jbound_t jbound_mul(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin, int64_t wmax) {
    int64_t c[4];
    if (__builtin_mul_overflow(a_lo, b_lo, &c[0]) ||
        __builtin_mul_overflow(a_lo, b_hi, &c[1]) ||
        __builtin_mul_overflow(a_hi, b_lo, &c[2]) ||
        __builtin_mul_overflow(a_hi, b_hi, &c[3]))
        return jbound_over();
    int64_t lo = c[0], hi = c[0];
    for (int i = 1; i < 4; i++) {
        lo = jbound_min2(lo, c[i]);
        hi = jbound_max2(hi, c[i]);
    }
    if (!jbound_fits(lo, hi, wmin, wmax)) return jbound_over();
    return jbound_of(lo, hi);
}

// §15.18. Only a single in-range distance is modelled: the spec masks the distance
// to the low 5/6 bits, and a masked distance's result set is not an interval. A
// single distance k is ×2^k, which is §15.16.1's rule again.
static inline jbound_t jbound_shl(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin, int64_t wmax, int maxsh) {
    if (b_lo != b_hi || b_lo < 0 || b_lo > maxsh) return jbound_none();
    int64_t f = (int64_t)1 << b_lo;
    return jbound_mul(a_lo, a_hi, f, f, wmin, wmax);
}

// §15.16.2. Corner-monotone once the spec's two carve-outs are excluded: a divisor
// range containing 0 (throws — the guard stays) and the MIN/-1 pair (overflows to
// the dividend). `wmin` is the DIVIDEND's width minimum.
static inline jbound_t jbound_div(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin) {
    if (b_lo <= 0 && b_hi >= 0) return jbound_none();
    if (a_lo == wmin && b_lo <= -1 && b_hi >= -1) return jbound_none();
    int64_t c[4] = { a_lo / b_lo, a_lo / b_hi, a_hi / b_lo, a_hi / b_hi };
    int64_t lo = c[0], hi = c[0];
    for (int i = 1; i < 4; i++) {
        lo = jbound_min2(lo, c[i]);
        hi = jbound_max2(hi, c[i]);
    }
    return jbound_of(lo, hi);
}

// §15.16.3. |r| < |divisor| and the sign follows the dividend. `wmin` is the
// DIVISOR's width minimum: |MIN| is unrepresentable, so that divisor claims nothing.
static inline jbound_t jbound_rem(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi,
                                  int64_t wmin) {
    if (b_lo <= 0 && b_hi >= 0) return jbound_none();
    if (b_lo == wmin) return jbound_none();
    int64_t blo_abs = b_lo < 0 ? -b_lo : b_lo;
    int64_t bhi_abs = b_hi < 0 ? -b_hi : b_hi;
    int64_t bound   = jbound_max2(blo_abs, bhi_abs) - 1;
    int64_t lo = a_lo >= 0 ? 0 : jbound_max2(a_lo, -bound);
    int64_t hi = a_hi <= 0 ? 0 : jbound_min2(a_hi, bound);
    return jbound_of(lo, hi);
}

// §15.21.1. A non-negative mask k bounds the result to [0, k] whatever the other
// side's sign: two's complement means the mask clears the sign bit. So it is the
// MASK side that must be non-negative — requiring it of both loses `x & 7` for a
// sign-spanning x. Two non-negative ranges bound by the smaller top.
static inline jbound_t jbound_and(int64_t a_lo, int64_t a_hi, int64_t b_lo, int64_t b_hi) {
    int64_t k, r_lo, r_hi;
    if      (b_lo == b_hi && b_lo >= 0) { k = b_lo; r_lo = a_lo; r_hi = a_hi; }
    else if (a_lo == a_hi && a_lo >= 0) { k = a_lo; r_lo = b_lo; r_hi = b_hi; }
    else if (a_lo >= 0 && b_lo >= 0)    return jbound_of(0, jbound_min2(a_hi, b_hi));
    else                                return jbound_none();
    // The other side's top tightens the bound only when it cannot be negative:
    // (-1) & 7 is 7, not ≤ that side's top.
    return jbound_of(0, (r_lo >= 0 && r_hi < k) ? r_hi : k);
}

// §5.1.3 narrowing primitive conversion: the result always lies in the target
// width. A source range already inside it is value-preserving — the caller keeps
// its own fact (and whatever refinement rides on it) rather than taking this.
static inline jbound_t jbound_narrow(int64_t a_lo, int64_t a_hi,
                                     int64_t tgt_lo, int64_t tgt_hi) {
    if (a_lo >= tgt_lo && a_hi <= tgt_hi) return jbound_of(a_lo, a_hi);
    return jbound_of(tgt_lo, tgt_hi);
}

// ── Branch narrowing ────────────────────────────────────────────────────────
//
// What a comparison proves about the tested operand on the edge that was taken.
// Both narrowing tables reduce to this: normalise the operator for which side is
// tested and which arm was taken, then read the bound off the OTHER side. The
// other side is an INTERVAL, not a constant — a constant is the k_lo == k_hi case
// — so `x < y` narrows x by y's upper bound, which is what makes the rule work
// for a comparison against another variable and not only against a literal.
typedef enum { JB_LT, JB_LE, JB_GT, JB_GE, JB_EQ, JB_NE } jbound_cmp_t;

// The tested operand sits on the RIGHT: `k op x` states `x flip(op) k`.
static inline jbound_cmp_t jbound_cmp_flip(jbound_cmp_t op) {
    switch (op) {
        case JB_LT: return JB_GT;
        case JB_LE: return JB_GE;
        case JB_GT: return JB_LT;
        case JB_GE: return JB_LE;
        default:    return op;          // EQ / NE are symmetric
    }
}

// The not-taken arm proves the negation.
static inline jbound_cmp_t jbound_cmp_negate(jbound_cmp_t op) {
    switch (op) {
        case JB_LT: return JB_GE;
        case JB_LE: return JB_GT;
        case JB_GT: return JB_LE;
        case JB_GE: return JB_LT;
        case JB_EQ: return JB_NE;
        default:    return JB_EQ;
    }
}

// The interval the tested operand is confined to, given the other side's
// [k_lo, k_hi]. `ok == false` means the comparison proves nothing usable (NE, or
// a bound already at the width's edge, which would name an empty set).
static inline jbound_t jbound_narrow_by_cmp(jbound_cmp_t op, int64_t k_lo, int64_t k_hi,
                                            int64_t wmin, int64_t wmax) {
    switch (op) {
        case JB_LT:                                   // x < y ≤ k_hi  ⟹  x ≤ k_hi-1
            if (k_hi <= wmin) return jbound_none();
            return jbound_of(wmin, k_hi - 1);
        case JB_LE: return jbound_of(wmin, k_hi < wmax ? k_hi : wmax);
        case JB_GT:                                   // x > y ≥ k_lo  ⟹  x ≥ k_lo+1
            if (k_lo >= wmax) return jbound_none();
            return jbound_of(k_lo + 1, wmax);
        case JB_GE: return jbound_of(k_lo > wmin ? k_lo : wmin, wmax);
        case JB_EQ: return jbound_of(k_lo, k_hi);
        case JB_NE: return jbound_none();             // proves no interval
    }
    return jbound_none();
}

// ── Lattice ops ─────────────────────────────────────────────────────────────
//
// The interval algebra both lattices run. Only the numeric part lives here: each
// side dispatches its own non-numeric elements (references, floats, symbolic
// bounds, "no fact" polarity) before reaching these.
static inline jbound_t jbound_hull(int64_t a_lo, int64_t a_hi,
                                   int64_t b_lo, int64_t b_hi) {          // join
    return jbound_of(jbound_min2(a_lo, b_lo), jbound_max2(a_hi, b_hi));
}

static inline jbound_t jbound_meet(int64_t a_lo, int64_t a_hi,
                                   int64_t b_lo, int64_t b_hi) {          // intersect
    int64_t lo = jbound_max2(a_lo, b_lo), hi = jbound_min2(a_hi, b_hi);
    if (lo > hi) return jbound_none();                                    // empty
    return jbound_of(lo, hi);
}

static inline bool jbound_contains(int64_t a_lo, int64_t a_hi,
                                   int64_t b_lo, int64_t b_hi) {          // a ⊑ b
    return a_lo >= b_lo && a_hi <= b_hi;
}

// Widening (Nielson & Nielson, PoPA §4.2). The DECISION is shared — a bound that
// did not grow is kept, one that grew cannot be trusted to stabilise — while the
// replacement is the consumer's policy: the SIR optimizer snaps to the next
// element of its per-method K set, the linter jumps straight to ±∞. Both
// terminate; only the chain length differs.
static inline bool jbound_widen_lo_grew(int64_t prev_lo, int64_t in_lo) { return in_lo < prev_lo; }
static inline bool jbound_widen_hi_grew(int64_t prev_hi, int64_t in_hi) { return in_hi > prev_hi; }

#endif
