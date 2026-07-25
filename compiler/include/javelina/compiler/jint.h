// jint.h — Java integer arithmetic, the one exact-arithmetic core.
//
// The integer operations JLS 1.0 (1997) defines, computed exactly as the spec
// states — verified against java-langspec-1.0.pdf, not from memory (the edition
// matters: these are the 1997 section numbers, not a later edition's):
//   §15.16.1  *   an integer product is "the low-order bits of the mathematical
//                 product" in two's-complement (wraps; never traps)
//   §15.16.2  /   rounds toward 0; MIN/-1 overflows to the dividend, no exception;
//                 divisor 0 throws ArithmeticException (the caller's to test for)
//   §15.16.3  %   (a/b)*b+(a%b) == a, so MIN%-1 == 0 and the sign follows the
//                 dividend; divisor 0 throws
//   §15.17.2  + - the sum/difference is "the low-order bits" in two's-complement
//   §15.18    shift: only the low 5 bits (int) / 6 bits (long) of the distance are
//                 used (masked by 0x1f / 0x3f); >> sign-extends, >>> zero-extends
//   §15.21.1  & ^ | integer bitwise
//   §15.14.4  unary - : -MIN wraps to MIN, since -x == (~x)+1
//
// This is the ONE home for that algebra; const_expr (§15.27 compile-time
// constants) and the SIR optimizer's KNOWN-lattice folds share it, each keeping
// its own policy for the throwing cases (÷0, %0). The div/rem functions assume a
// nonzero divisor; the caller tests for zero and decides (const_expr: the
// expression denotes no value; the optimizer: keep the trap, refuse the fold).
#ifndef JAVELINA_JINT_H
#define JAVELINA_JINT_H
#include <stdint.h>

// §15.17.2 (+ -) and §15.16.1 (*): the low-order bits of the two's-complement result.
static inline int32_t jint_add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static inline int32_t jint_sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static inline int32_t jint_mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
// §15.16.2/.3: MIN/-1 throws nothing — div is the dividend, rem is 0 (both C UB,
// so returned by the spec's stated value, never computed).
static inline int32_t jint_div(int32_t a, int32_t b) { return (a == INT32_MIN && b == -1) ? INT32_MIN : a / b; }
static inline int32_t jint_rem(int32_t a, int32_t b) { return (a == INT32_MIN && b == -1) ? 0        : a % b; }
// §15.21.1: integer bitwise.
static inline int32_t jint_and(int32_t a, int32_t b) { return a & b; }
static inline int32_t jint_or (int32_t a, int32_t b) { return a | b; }
static inline int32_t jint_xor(int32_t a, int32_t b) { return a ^ b; }
// §15.18: an int shift uses only the low 5 bits of the distance; >> sign-extends,
// >>> zero-extends.
static inline int32_t jint_shl (int32_t a, int32_t b) { return (int32_t)((uint32_t)a << ((uint32_t)b & 31)); }
static inline int32_t jint_shr (int32_t a, int32_t b) { return a >> ((uint32_t)b & 31); }
static inline int32_t jint_ushr(int32_t a, int32_t b) { return (int32_t)((uint32_t)a >> ((uint32_t)b & 31)); }
// §15.14.4: -MIN wraps to MIN.
static inline int32_t jint_neg (int32_t a)            { return (int32_t)(0u - (uint32_t)a); }

// The long (int64) twins. §15.18: a long shift uses the low 6 bits (unused here
// until cp_fold_wide's i64 arms migrate to this core).
static inline int64_t jlong_add(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
static inline int64_t jlong_sub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
static inline int64_t jlong_mul(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }
static inline int64_t jlong_div(int64_t a, int64_t b) { return (a == INT64_MIN && b == -1) ? INT64_MIN : a / b; }
static inline int64_t jlong_rem(int64_t a, int64_t b) { return (a == INT64_MIN && b == -1) ? 0         : a % b; }
static inline int64_t jlong_neg(int64_t a)            { return (int64_t)(0u - (uint64_t)a); }

#endif
