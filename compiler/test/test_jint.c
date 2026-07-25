// test_jint.c — the exact-arithmetic core (jint.h), pinned at the value level.
//
// JLS 1.0 §15.16 (*/%), §15.17.2 (+ -), §15.18 (shift), §15.21.1 (bitwise)
// integer semantics, including the corners C leaves undefined (MIN/-1) or that
// wrap, and the remainder examples from §15.16.3 verbatim. The owning-level test:
// its consumers — const_expr's §15.27 folds and the optimizer's KNOWN-lattice
// folds — reach these values only through narrow contexts, so the core is pinned
// directly here rather than left to sparse integration coverage.
#include "javelina/compiler/jint.h"
#include <limits.h>

#include "javelina_test.h"

int main(void) {
    // §15.17.2 (+ -) and §15.16.1 (*): the low-order bits of the two's-complement result.
    CHECK(jint_add(1, 2) == 3,                      "add");
    CHECK(jint_add(INT32_MAX, 1) == INT32_MIN,      "add wraps past MAX");
    CHECK(jint_sub(0, INT32_MIN) == INT32_MIN,      "sub: 0 - MIN wraps to MIN");
    CHECK(jint_mul(0x10000, 0x10000) == 0,          "mul: 2^16 * 2^16 = 0 (low 32 bits)");
    CHECK(jint_neg(INT32_MIN) == INT32_MIN,         "neg MIN wraps to MIN (§15.14.4)");
    // §15.16.2/.3: MIN/-1 throws nothing — div is the dividend, rem is 0.
    CHECK(jint_div(INT32_MIN, -1) == INT32_MIN,     "div MIN/-1 == MIN (defined, not C UB)");
    CHECK(jint_rem(INT32_MIN, -1) == 0,             "rem MIN/-1 == 0");
    CHECK(jint_div(-7, 2) == -3,                    "div rounds toward zero");
    // §15.16.3's own examples: the remainder's sign follows the dividend.
    CHECK(jint_rem(5, 3) == 2,                      "rem 5%3 == 2");
    CHECK(jint_rem(5, -3) == 2,                     "rem 5%(-3) == 2");
    CHECK(jint_rem(-5, 3) == -2,                    "rem (-5)%3 == -2");
    CHECK(jint_rem(-5, -3) == -2,                   "rem (-5)%(-3) == -2");
    // §15.18: an int shift uses only the low 5 bits of the distance.
    CHECK(jint_shl(1, 33) == 2,                     "shl distance masked (33 & 31 = 1)");
    CHECK(jint_shr(-8, 1) == -4,                    "shr is arithmetic (sign-extends)");
    CHECK(jint_ushr(-1, 28) == 15,                  "ushr is logical (zero-fills)");
    CHECK(jint_and(0xF0, 0x3C) == 0x30,             "and");
    CHECK(jint_or (0xF0, 0x0F) == 0xFF,             "or");
    CHECK(jint_xor(0xFF, 0x0F) == 0xF0,             "xor");

    // The long (int64) twins.
    CHECK(jlong_add(INT64_MAX, 1) == INT64_MIN,     "long add wraps past MAX");
    CHECK(jlong_mul(0x100000000LL, 0x100000000LL) == 0, "long mul: 2^32 * 2^32 = 0 (low 64)");
    CHECK(jlong_div(INT64_MIN, -1) == INT64_MIN,    "long div MIN/-1 == MIN");
    CHECK(jlong_rem(INT64_MIN, -1) == 0,            "long rem MIN/-1 == 0");
    CHECK(jlong_neg(INT64_MIN) == INT64_MIN,        "long neg MIN wraps to MIN");

    return TEST_SUMMARY("test_jint");
}
