// test_jbound.c — the bound-arithmetic core (jbound.h), pinned at the value level.
//
// The twin of test_jint.c. jbound.h is where the JLS 1.0 interval algebra lives:
// which corners bound a op b, and which cases the spec leaves unclaimable. Its two
// consumers — the SIR optimizer's range folds and the sema linter's interval walker
// — reach these rules only through their own lattices and their own overflow
// policies, so the algebra is pinned directly here.
//
// The two failure kinds are distinct and both are pinned: `ok == false` is a
// SPEC-level refusal every consumer must honour, `overflow == true` is a report
// whose response is the consumer's policy.
#include "javelina/compiler/jbound.h"
#include <limits.h>

#include "javelina_test.h"

#define W32_MIN ((int64_t)INT32_MIN)
#define W32_MAX ((int64_t)INT32_MAX)

int main(void) {
    // §15.17.2 — monotone corners.
    {
        jbound_t r = jbound_add(1, 5, 10, 20, W32_MIN, W32_MAX);
        CHECK(r.ok && !r.overflow && r.lo == 11 && r.hi == 25, "add: [1,5]+[10,20] = [11,25]");
        // "the low-order bits": a sum leaving int32 wrapped at runtime, so the
        // interval claim is unrepresentable — reported, not silently clamped.
        jbound_t o = jbound_add(W32_MAX, W32_MAX, 1, 1, W32_MIN, W32_MAX);
        CHECK(o.overflow, "add: MAX+1 leaves the width — overflow REPORTED (§15.17.2 wraps)");
        // The same corners in i64 do not overflow: the width is the caller's.
        jbound_t w = jbound_add(W32_MAX, W32_MAX, 1, 1, INT64_MIN, INT64_MAX);
        CHECK(w.ok && !w.overflow && w.lo == 2147483648LL,
              "add: the same sum is exact at i64 width — the width is the caller's");
    }
    // a - b is least when a is least and b is GREATEST (the corner that a
    // both-lo/both-hi reading gets wrong).
    {
        jbound_t r = jbound_sub(0, 10, 1, 4, W32_MIN, W32_MAX);
        CHECK(r.ok && r.lo == -4 && r.hi == 9, "sub: [0,10]-[1,4] = [-4,9]");
    }
    // §15.14.4 — -MIN wraps, so a range reaching the width minimum claims nothing.
    {
        jbound_t r = jbound_neg(-5, 3, W32_MIN, W32_MAX);
        CHECK(r.ok && r.lo == -3 && r.hi == 5, "neg: -[-5,3] = [-3,5]");
        jbound_t n = jbound_neg(W32_MIN, 0, W32_MIN, W32_MAX);
        CHECK(!n.ok, "neg: a range reaching MIN claims nothing (-MIN wraps, §15.14.4)");
    }
    // §15.16.1 — four corners, and the sign-spanning case the two-corner reading misses.
    {
        jbound_t r = jbound_mul(-3, 2, -4, 5, W32_MIN, W32_MAX);
        CHECK(r.ok && r.lo == -15 && r.hi == 12, "mul: [-3,2]*[-4,5] = [-15,12] (four corners)");
        jbound_t o = jbound_mul(0x10000, 0x10000, 0x10000, 0x10000, W32_MIN, W32_MAX);
        CHECK(o.overflow, "mul: 2^16*2^16 leaves int32 — overflow REPORTED (§15.16.1 wraps)");
    }
    // §15.18 — only a single in-range distance is an interval; a masked distance is not.
    {
        jbound_t r = jbound_shl(1, 3, 2, 2, W32_MIN, W32_MAX, 31);
        CHECK(r.ok && r.lo == 4 && r.hi == 12, "shl: [1,3]<<2 = [4,12]");
        CHECK(!jbound_shl(1, 3, 1, 2, W32_MIN, W32_MAX, 31).ok,
              "shl: a RANGE of distances claims nothing");
        CHECK(!jbound_shl(1, 3, 32, 32, W32_MIN, W32_MAX, 31).ok,
              "shl: a distance past the width is masked (§15.18) — no claim");
    }
    // §15.16.2 — corner-monotone, with the spec's two carve-outs refused.
    {
        jbound_t r = jbound_div(10, 20, 2, 5, W32_MIN);
        CHECK(r.ok && r.lo == 2 && r.hi == 10, "div: [10,20]/[2,5] = [2,10]");
        CHECK(!jbound_div(10, 20, 0, 5, W32_MIN).ok,
              "div: a divisor range containing 0 claims nothing (§15.16.2 throws)");
        CHECK(!jbound_div(10, 20, -1, 1, W32_MIN).ok, "div: a divisor spanning 0 likewise");
        CHECK(!jbound_div(W32_MIN, 0, -5, -1, W32_MIN).ok,
              "div: the MIN/-1 corner claims nothing (§15.16.2: overflows to the dividend)");
        jbound_t neg = jbound_div(-7, -7, 2, 2, W32_MIN);
        CHECK(neg.ok && neg.lo == -3 && neg.hi == -3, "div rounds toward 0, not down");
    }
    // §15.16.3 — |r| < |divisor|, sign follows the DIVIDEND.
    {
        jbound_t nn = jbound_rem(0, 100, 5, 5, W32_MIN);
        CHECK(nn.ok && nn.lo == 0 && nn.hi == 4, "rem: a non-negative dividend gives [0,|d|-1]");
        jbound_t np = jbound_rem(-100, -1, 5, 5, W32_MIN);
        CHECK(np.ok && np.lo == -4 && np.hi == 0, "rem: a negative dividend gives [-(|d|-1),0]");
        jbound_t sp = jbound_rem(-100, 100, 5, 5, W32_MIN);
        CHECK(sp.ok && sp.lo == -4 && sp.hi == 4, "rem: a sign-spanning dividend spans both");
        jbound_t nd = jbound_rem(0, 100, -5, -5, W32_MIN);
        CHECK(nd.ok && nd.lo == 0 && nd.hi == 4, "rem: the DIVISOR's sign does not reach the result");
        CHECK(!jbound_rem(0, 10, 0, 5, W32_MIN).ok, "rem: a divisor range containing 0 claims nothing");
        CHECK(!jbound_rem(0, 10, W32_MIN, W32_MIN, W32_MIN).ok,
              "rem: |MIN| is unrepresentable — that divisor claims nothing");
        // The dividend's own bound wins when it is tighter than |d|-1.
        jbound_t tight = jbound_rem(0, 2, 100, 100, W32_MIN);
        CHECK(tight.ok && tight.lo == 0 && tight.hi == 2, "rem: a dividend tighter than |d|-1 keeps its own bound");
    }
    // §15.21.1 — it is the MASK side that must be non-negative.
    {
        jbound_t r = jbound_and(W32_MIN, W32_MAX, 7, 7);
        CHECK(r.ok && r.lo == 0 && r.hi == 7,
              "and: a non-negative mask bounds a SIGN-SPANNING operand to [0,k]");
        jbound_t both = jbound_and(0, 5, 0, 100);
        CHECK(both.ok && both.lo == 0 && both.hi == 5, "and: two non-negative ranges bound by the smaller top");
        jbound_t tighter = jbound_and(0, 3, 255, 255);
        CHECK(tighter.ok && tighter.hi == 3, "and: the other side's top tightens when it cannot be negative");
        jbound_t nofit = jbound_and(-1, -1, 7, 7);
        CHECK(nofit.ok && nofit.hi == 7, "and: (-1)&7 is 7 — a negative side does NOT tighten");
        CHECK(!jbound_and(W32_MIN, W32_MAX, W32_MIN, W32_MAX).ok,
              "and: two sign-spanning ranges claim nothing");
    }
    // Narrowing: the result always lies in the target width; a fitting range is
    // value-preserving and comes back unchanged.
    {
        jbound_t fits = jbound_narrow(0, 100, INT8_MIN, INT8_MAX);
        CHECK(fits.ok && fits.lo == 0 && fits.hi == 100, "narrow: a fitting range is value-preserving");
        jbound_t trunc = jbound_narrow(0, 1000, INT8_MIN, INT8_MAX);
        CHECK(trunc.ok && trunc.lo == INT8_MIN && trunc.hi == INT8_MAX,
              "narrow: a crossing range truncates to the target width");
        jbound_t ch = jbound_narrow(-5, 5, 0, 65535);
        CHECK(ch.ok && ch.lo == 0 && ch.hi == 65535, "narrow: char is unsigned — [-5,5] truncates to the full range");
    }
    // ── Branch narrowing ────────────────────────────────────────────────
    // Operator normalisation: which side is tested, and which arm was taken.
    CHECK(jbound_cmp_flip(JB_LT) == JB_GT && jbound_cmp_flip(JB_GE) == JB_LE,
          "flip: `k < x` states `x > k`");
    CHECK(jbound_cmp_flip(JB_EQ) == JB_EQ && jbound_cmp_flip(JB_NE) == JB_NE,
          "flip: equality is symmetric");
    CHECK(jbound_cmp_negate(JB_LT) == JB_GE && jbound_cmp_negate(JB_EQ) == JB_NE,
          "negate: the not-taken arm proves the negation");
    {
        jbound_t lt = jbound_narrow_by_cmp(JB_LT, 10, 10, W32_MIN, W32_MAX);
        CHECK(lt.ok && lt.lo == W32_MIN && lt.hi == 9, "narrow: x < 10 gives [MIN,9]");
        jbound_t ge = jbound_narrow_by_cmp(JB_GE, 0, 0, W32_MIN, W32_MAX);
        CHECK(ge.ok && ge.lo == 0 && ge.hi == W32_MAX, "narrow: x >= 0 gives [0,MAX]");
        jbound_t eq = jbound_narrow_by_cmp(JB_EQ, 7, 7, W32_MIN, W32_MAX);
        CHECK(eq.ok && eq.lo == 7 && eq.hi == 7, "narrow: x == 7 pins x");
        CHECK(!jbound_narrow_by_cmp(JB_NE, 7, 7, W32_MIN, W32_MAX).ok,
              "narrow: != proves no interval");
        CHECK(!jbound_narrow_by_cmp(JB_LT, W32_MIN, W32_MIN, W32_MIN, W32_MAX).ok,
              "narrow: x < MIN names the empty set — no claim");
        CHECK(!jbound_narrow_by_cmp(JB_GT, W32_MAX, W32_MAX, W32_MIN, W32_MAX).ok,
              "narrow: x > MAX likewise");
        // The other side is an INTERVAL, not just a constant: `x < y` with
        // y ∈ [3,8] proves x ≤ 7. A constant-only rule cannot state this.
        jbound_t iv = jbound_narrow_by_cmp(JB_LT, 3, 8, W32_MIN, W32_MAX);
        CHECK(iv.ok && iv.hi == 7, "narrow: x < y for y in [3,8] gives x <= 7");
        jbound_t gv = jbound_narrow_by_cmp(JB_GT, 3, 8, W32_MIN, W32_MAX);
        CHECK(gv.ok && gv.lo == 4, "narrow: x > y for y in [3,8] gives x >= 4");
    }
    // ── Lattice ops ─────────────────────────────────────────────────────
    {
        jbound_t j = jbound_hull(0, 5, 10, 20);
        CHECK(j.ok && j.lo == 0 && j.hi == 20, "hull: encloses both");
        jbound_t m = jbound_meet(0, 10, 5, 20);
        CHECK(m.ok && m.lo == 5 && m.hi == 10, "meet: intersects");
        CHECK(!jbound_meet(0, 4, 5, 9).ok, "meet: disjoint intervals are empty");
        CHECK(jbound_contains(2, 3, 0, 10) && !jbound_contains(0, 10, 2, 3),
              "contains: interval inclusion, one direction only");
        CHECK(jbound_widen_lo_grew(0, -1) && !jbound_widen_lo_grew(0, 0),
              "widen: a lower bound that fell has grown");
        CHECK(jbound_widen_hi_grew(0, 1) && !jbound_widen_hi_grew(5, 5),
              "widen: an upper bound that rose has grown");
    }
    return TEST_SUMMARY("test_jbound");
}
