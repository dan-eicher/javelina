/* const_expr.h — JLS §15.27 compile-time constant expressions.
 *
 * The ONE evaluator. Its readers today:
 *   - §14.19 reachability: is a while/do/for condition a constant expression with value `true`
 *     (the loop cannot complete normally) or `false` (its body is unreachable)?
 *   - the DDCG loop rules: the same fact, so `while (true)`, `while (1==1)`, `while (CONST)` and
 *     `for (;;)` all lower identically — no test, no false edge, no exit anchor.
 *
 * A second implementation of this predicate would be free to disagree with the first, and the two
 * would: they already did, when "constant" meant "a boolean literal" here and "a literal or a
 * negated literal" in sema_field_const_int.
 */
#ifndef JAVELINA_COMPILER_CONST_EXPR_H
#define JAVELINA_COMPILER_CONST_EXPR_H

#include "gen/java_ast.h"
#include "javelina/compiler/sema.h"

#include <stdbool.h>
#include <stdint.h>

/* A constant's TYPE is sema's `java_type_tag_t` — NOT a private enum of this file's own.
 *
 * It used to be one (`jls_const_kind_t`: JC_BOOL, JC_BYTE, … JC_DOUBLE), which was a
 * re-encoding of the very tags sema already defines, and it dragged a second copy of JLS
 * §5.6.1/§5.6.2 numeric promotion along with it (`unary_promote`, `binary_promote`) — while
 * type_lattice.h calls itself, in its own words, "the JLS conversion authority
 * (§5.1.2/§5.1.3/§5.6)" and exports `lat_promote` / `lat_widen_rank` for exactly that.
 * Two implementations of the promotion rules, in two type vocabularies, are free to
 * disagree — which is the failure this header's own comment (below) warns about, one level
 * down. The types come from sema; the promotion comes from the lattice; only the VALUE
 * arithmetic is this file's own.
 *
 * §15.27 admits primitive types and String; the String half is not implemented (the parser
 * desugars string literals away — see const_expr.c). The integral tags stay DISTINCT (byte
 * is not int) because a constant's type, not just its value, decides §5.2 assignment
 * narrowing and §15.24's conditional-operator result type. */
typedef struct {
    java_type_tag_t tag;   /* JT_VOID ⟹ not a constant expression */
    union {
        bool    b;
        int32_t i;   /* byte, short, char, int — already narrowed to the tag's range */
        int64_t l;
        float   f;
        double  d;
    } v;
} jls_const_t;

/* Evaluate `e` per §15.27. `tag == JT_VOID` means "not a constant expression" — including
 * the cases where the expression denotes no value at all, such as an integer division by
 * zero. */
jls_const_t jls_const_eval(const sema_ctx_t* ctx, const ast_expr_t* e);
/* Is `e` a constant expression at all? */
bool jls_const_is_constant(const sema_ctx_t* ctx, const ast_expr_t* e);

/* Convenience for §14.19, which speaks of "a constant expression with value true" / "false". */
bool jls_const_is_true (const sema_ctx_t* ctx, const ast_expr_t* e);
bool jls_const_is_false(const sema_ctx_t* ctx, const ast_expr_t* e);

#endif /* JAVELINA_COMPILER_CONST_EXPR_H */
