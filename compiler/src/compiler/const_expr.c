/* const_expr.c — JLS §15.27, compile-time constant expressions.
 *
 * §15.27 defines a constant expression exhaustively:
 *
 *   A compile-time constant expression is an expression denoting a value of primitive type
 *   or a String that is composed using only the following:
 *     • Literals of primitive type and literals of type String
 *     • Casts to primitive types and casts to type String
 *     • The unary operators +, -, ~, and ! (but not ++ or --)
 *     • The multiplicative operators *, /, and %
 *     • The additive operators + and -
 *     • The shift operators <<, >>, and >>>
 *     • The relational operators <, <=, >, and >= (but not instanceof)
 *     • The equality operators == and !=
 *     • The bitwise and logical operators &, ^, and |
 *     • The conditional-and operator && and the conditional-or operator ||
 *     • The ternary conditional operator ? :
 *     • Simple names that refer to final variables whose initializers are constant expressions
 *     • Qualified names of the form TypeName.Identifier that refer to final variables whose
 *       initializers are constant expressions
 *
 * This is the ONE evaluator. §14.19 asks it whether a while/do/for condition is "a constant
 * expression with value true" (and whether a while's condition is constant `false`, which makes
 * the contained statement unreachable); the backend asks the same question to decide such a loop
 * has no test and no false edge; and §13.1 folds a use of a constant VARIABLE to its value.
 * Three readers, one computation — a second implementation would be free to disagree, and the
 * two would.
 *
 * WHAT IS THIS FILE'S OWN, AND WHAT IS NOT. Its own: the VALUE arithmetic — what `3 / 0` denotes
 * (nothing), what `INT_MIN / -1` wraps to, what `fmod` a float `%` is. NOT its own: TYPES. A
 * constant's type is sema's `java_type_tag_t`, and the promotion rules (§5.6.1 unary, §5.6.2
 * binary) come from the TYPE LATTICE, which calls itself "the JLS conversion authority
 * (§5.1.2/§5.1.3/§5.6)" and exports `lat_promote` for precisely this. This file used to carry a
 * private enum (`jls_const_kind_t`) that re-encoded sema's primitive tags, and its own copy of
 * both promotion rules — two implementations, in two vocabularies, free to disagree. They are
 * gone.
 *
 * NOT YET COVERED, and deliberately absent rather than approximated: the String half. The parser
 * desugars a string literal into `new String(char[]{…})` (grammar/Java.peg §3.10.5), so there is no
 * StringLit node to recognise, and §3.10.5 interning is not implemented either — `"a" == "a"` is
 * false on this target. Folding String constants before those two land would convert a spec
 * violation into a miscompile. They are sequenced together.
 */
#include "javelina/compiler/const_expr.h"
#include "javelina/compiler/jint.h"   /* the exact-arithmetic core */
#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"   /* §5.6 promotion — the ONE authority */

#include <math.h>
#include <stdint.h>
#include <string.h>

/* A `final int A = B;` / `final int B = A;` cycle would recurse forever. The spec has no such
 * program (each initializer must be a constant expression, and a cycle is not), but sema reports
 * that separately; the evaluator just refuses to descend. */
#define JLS_CONST_MAX_DEPTH 64

static jls_const_t eval(const sema_ctx_t* ctx, const ast_expr_t* e, int depth);

/* JT_VOID is the "no value" tag: an expression that is not a constant expression. */
static const jls_const_t NOT_CONSTANT = { JT_VOID, { .l = 0 } };

/* A bare primitive java_type_t, for handing a tag to the lattice's promotion. */
static java_type_t jt_of(java_type_tag_t t) {
    java_type_t j; memset(&j, 0, sizeof j); j.tag = t; j.class_id = -1; return j;
}

static bool is_integral(java_type_tag_t t) {
    return t == JT_BYTE || t == JT_SHORT || t == JT_CHAR || t == JT_INT || t == JT_LONG;
}
static bool is_numeric(java_type_tag_t t) {
    return is_integral(t) || t == JT_FLOAT || t == JT_DOUBLE;
}

static int32_t as_int(jls_const_t c) { return c.tag == JT_LONG ? (int32_t)c.v.l : c.v.i; }
static int64_t as_long(jls_const_t c) {
    switch (c.tag) {
    case JT_LONG:   return c.v.l;
    case JT_FLOAT:  return (int64_t)c.v.f;
    case JT_DOUBLE: return (int64_t)c.v.d;
    default:        return (int64_t)c.v.i;
    }
}
static float as_float(jls_const_t c) {
    switch (c.tag) {
    case JT_FLOAT:  return c.v.f;
    case JT_DOUBLE: return (float)c.v.d;
    case JT_LONG:   return (float)c.v.l;
    default:        return (float)c.v.i;
    }
}
static double as_double(jls_const_t c) {
    switch (c.tag) {
    case JT_DOUBLE: return c.v.d;
    case JT_FLOAT:  return (double)c.v.f;
    case JT_LONG:   return (double)c.v.l;
    default:        return (double)c.v.i;
    }
}

static jls_const_t mk_bool(bool b)    { jls_const_t c; c.tag = JT_BOOL;   c.v.b = b; return c; }
static jls_const_t mk_int(int32_t i)  { jls_const_t c; c.tag = JT_INT;    c.v.i = i; return c; }
static jls_const_t mk_long(int64_t l) { jls_const_t c; c.tag = JT_LONG;   c.v.l = l; return c; }
static jls_const_t mk_float(float f)  { jls_const_t c; c.tag = JT_FLOAT;  c.v.f = f; return c; }
static jls_const_t mk_double(double d){ jls_const_t c; c.tag = JT_DOUBLE; c.v.d = d; return c; }

/* §5.6.1 unary numeric promotion — ASKED OF THE LATTICE, not reimplemented. §5.6.1 is exactly
 * §5.6.2 against `int`: byte/short/char widen to int, and everything wider stays itself. */
static java_type_tag_t unary_promote(java_type_tag_t t) {
    if (!is_numeric(t)) return t;
    return lat_promote(jt_of(t), jt_of(JT_INT));
}
/* §5.6.2 binary numeric promotion — likewise the lattice's. */
static java_type_tag_t binary_promote(java_type_tag_t a, java_type_tag_t b) {
    return lat_promote(jt_of(a), jt_of(b));
}

/* §5.1.1-.3 the primitive conversions a cast performs. */
static jls_const_t cast_to(java_type_tag_t target, jls_const_t c) {
    if (target == JT_BOOL) return c.tag == JT_BOOL ? c : NOT_CONSTANT;
    if (c.tag == JT_BOOL || !is_numeric(c.tag)) return NOT_CONSTANT;
    jls_const_t r;
    r.tag = target;
    switch (target) {
    case JT_BYTE:   r.v.i = (int8_t)as_int(c);   break;
    case JT_SHORT:  r.v.i = (int16_t)as_int(c);  break;
    case JT_CHAR:   r.v.i = (uint16_t)as_int(c); break;
    /* §5.1.3 narrowing to int/long from float/double: NaN → 0, saturate at the extremes.
     * as_long/as_int's C casts are undefined there, so the float paths go through the
     * same clamping the language specifies. */
    case JT_INT:
        if (c.tag == JT_FLOAT || c.tag == JT_DOUBLE) {
            double d = as_double(c);
            r.v.i = (d != d)                 ? 0
                  : (d <= (double)INT32_MIN) ? INT32_MIN
                  : (d >= (double)INT32_MAX) ? INT32_MAX
                  : (int32_t)d;
        } else r.v.i = as_int(c);
        break;
    case JT_LONG:
        if (c.tag == JT_FLOAT || c.tag == JT_DOUBLE) {
            double d = as_double(c);
            r.v.l = (d != d)                 ? 0
                  : (d <= (double)INT64_MIN) ? INT64_MIN
                  : (d >= (double)INT64_MAX) ? INT64_MAX
                  : (int64_t)d;
        } else r.v.l = as_long(c);
        break;
    case JT_FLOAT:  r.v.f = as_float(c);  break;
    case JT_DOUBLE: r.v.d = as_double(c); break;
    default: return NOT_CONSTANT;
    }
    return r;
}

static java_type_tag_t prim_tag_of_type(const ast_type_t* t) {
    if (!t) return JT_VOID;
    switch (t->tag) {
    case AST_BYTETYPE:   return JT_BYTE;
    case AST_SHORTTYPE:  return JT_SHORT;
    case AST_CHARTYPE:   return JT_CHAR;
    case AST_INTTYPE:    return JT_INT;
    case AST_LONGTYPE:   return JT_LONG;
    case AST_FLOATTYPE:  return JT_FLOAT;
    case AST_DOUBLETYPE: return JT_DOUBLE;
    case AST_BOOLTYPE:   return JT_BOOL;
    default:             return JT_VOID;   /* a class or array type: §15.27 admits only casts to
                                            * primitive types and to String (see the header note) */
    }
}

/* §15.17 multiplicative, §15.18 additive. */
static jls_const_t arith(ast_binop_t op, jls_const_t a, jls_const_t b) {
    if (!is_numeric(a.tag) || !is_numeric(b.tag)) return NOT_CONSTANT;
    java_type_tag_t k = binary_promote(unary_promote(a.tag), unary_promote(b.tag));
    if (k == JT_DOUBLE) {
        double x = as_double(a), y = as_double(b);
        switch (op) {
        case AST_ADD: return mk_double(x + y);
        case AST_SUB: return mk_double(x - y);
        case AST_MUL: return mk_double(x * y);
        case AST_DIV: return mk_double(x / y);              /* §15.16.2: no exception; ±inf / NaN */
        /* §15.17.3: the floating remainder has the sign of the dividend and the magnitude of
         * x - (y * q) where q is x/y truncated toward zero — C's fmod, not IEEE remainder. */
        case AST_REM: return mk_double(fmod(x, y));
        default: return NOT_CONSTANT;
        }
    }
    if (k == JT_FLOAT) {
        float x = as_float(a), y = as_float(b);
        switch (op) {
        case AST_ADD: return mk_float(x + y);
        case AST_SUB: return mk_float(x - y);
        case AST_MUL: return mk_float(x * y);
        case AST_DIV: return mk_float(x / y);
        case AST_REM: return mk_float(fmodf(x, y));
        default: return NOT_CONSTANT;
        }
    }
    if (k == JT_LONG) {
        int64_t x = as_long(a), y = as_long(b);
        switch (op) {
        case AST_ADD: return mk_long(jlong_add(x, y));
        case AST_SUB: return mk_long(jlong_sub(x, y));
        case AST_MUL: return mk_long(jlong_mul(x, y));
        /* §15.27: the expression must DENOTE A VALUE. An integer division by zero denotes no
         * value (it throws), so it is not a constant expression. The core folds the
         * non-throwing MIN/-1 case (§15.16.2) by its stated value. */
        case AST_DIV: if (y == 0) return NOT_CONSTANT;
                      return mk_long(jlong_div(x, y));
        case AST_REM: if (y == 0) return NOT_CONSTANT;
                      return mk_long(jlong_rem(x, y));
        default: return NOT_CONSTANT;
        }
    }
    int32_t x = as_int(a), y = as_int(b);
    switch (op) {
    case AST_ADD: return mk_int(jint_add(x, y));
    case AST_SUB: return mk_int(jint_sub(x, y));
    case AST_MUL: return mk_int(jint_mul(x, y));
    case AST_DIV: if (y == 0) return NOT_CONSTANT;
                  return mk_int(jint_div(x, y));
    case AST_REM: if (y == 0) return NOT_CONSTANT;
                  return mk_int(jint_rem(x, y));
    default: return NOT_CONSTANT;
    }
}

/* §15.19 shift: each operand is unary-promoted separately; the result has the promoted type of
 * the LEFT operand; only the low 5 (int) or 6 (long) bits of the distance are used. */
static jls_const_t shift(ast_binop_t op, jls_const_t a, jls_const_t b) {
    if (!is_integral(a.tag) || !is_integral(b.tag)) return NOT_CONSTANT;
    if (unary_promote(a.tag) == JT_LONG) {
        int64_t x = as_long(a);
        int s = (int)(as_long(b) & 63);
        switch (op) {
        case AST_SHL:  return mk_long((int64_t)((uint64_t)x << s));
        case AST_SHR:  return mk_long(x >> s);
        case AST_USHR: return mk_long((int64_t)((uint64_t)x >> s));
        default: return NOT_CONSTANT;
        }
    }
    int32_t x = as_int(a);
    int s = (int)(as_long(b) & 31);
    switch (op) {
    case AST_SHL:  return mk_int((int32_t)((uint32_t)x << s));
    case AST_SHR:  return mk_int(x >> s);
    case AST_USHR: return mk_int((int32_t)((uint32_t)x >> s));
    default: return NOT_CONSTANT;
    }
}

/* §15.20 relational, §15.21 equality. */
static jls_const_t compare(ast_binop_t op, jls_const_t a, jls_const_t b) {
    if (a.tag == JT_BOOL || b.tag == JT_BOOL) {
        /* §15.21.2: == and != apply to two booleans. Nothing else does. */
        if (a.tag != JT_BOOL || b.tag != JT_BOOL) return NOT_CONSTANT;
        if (op == AST_EQ) return mk_bool(a.v.b == b.v.b);
        if (op == AST_NE) return mk_bool(a.v.b != b.v.b);
        return NOT_CONSTANT;
    }
    if (!is_numeric(a.tag) || !is_numeric(b.tag)) return NOT_CONSTANT;
    java_type_tag_t k = binary_promote(unary_promote(a.tag), unary_promote(b.tag));
    if (k == JT_DOUBLE || k == JT_FLOAT) {
        double x = as_double(a), y = as_double(b);
        switch (op) {                       /* NaN compares false everywhere except != */
        case AST_LT: return mk_bool(x <  y);
        case AST_GT: return mk_bool(x >  y);
        case AST_LE: return mk_bool(x <= y);
        case AST_GE: return mk_bool(x >= y);
        case AST_EQ: return mk_bool(x == y);
        case AST_NE: return mk_bool(x != y);
        default: return NOT_CONSTANT;
        }
    }
    int64_t x = as_long(a), y = as_long(b);
    switch (op) {
    case AST_LT: return mk_bool(x <  y);
    case AST_GT: return mk_bool(x >  y);
    case AST_LE: return mk_bool(x <= y);
    case AST_GE: return mk_bool(x >= y);
    case AST_EQ: return mk_bool(x == y);
    case AST_NE: return mk_bool(x != y);
    default: return NOT_CONSTANT;
    }
}

/* §15.22 bitwise and logical: on two booleans they are the logical operators; on two integral
 * operands they are bitwise, after binary numeric promotion. */
static jls_const_t bitwise(ast_binop_t op, jls_const_t a, jls_const_t b) {
    if (a.tag == JT_BOOL || b.tag == JT_BOOL) {
        if (a.tag != JT_BOOL || b.tag != JT_BOOL) return NOT_CONSTANT;
        switch (op) {
        case AST_BITAND: return mk_bool(a.v.b && b.v.b);
        case AST_BITOR:  return mk_bool(a.v.b || b.v.b);
        case AST_BITXOR: return mk_bool(a.v.b != b.v.b);
        default: return NOT_CONSTANT;
        }
    }
    if (!is_integral(a.tag) || !is_integral(b.tag)) return NOT_CONSTANT;
    if (binary_promote(unary_promote(a.tag), unary_promote(b.tag)) == JT_LONG) {
        int64_t x = as_long(a), y = as_long(b);
        switch (op) {
        case AST_BITAND: return mk_long(x & y);
        case AST_BITOR:  return mk_long(x | y);
        case AST_BITXOR: return mk_long(x ^ y);
        default: return NOT_CONSTANT;
        }
    }
    int32_t x = as_int(a), y = as_int(b);
    switch (op) {
    case AST_BITAND: return mk_int(x & y);
    case AST_BITOR:  return mk_int(x | y);
    case AST_BITXOR: return mk_int(x ^ y);
    default: return NOT_CONSTANT;
    }
}

/* §15.24 conditional operator: the result type when both arms are constants. The value is the
 * chosen arm's, converted to that type. (The byte/short/char cases exist because
 * `flag ? 'a' : 0` has type char, not int.) */
static java_type_tag_t conditional_tag(jls_const_t t, jls_const_t f) {
    if (t.tag == JT_BOOL && f.tag == JT_BOOL) return JT_BOOL;
    if (!is_numeric(t.tag) || !is_numeric(f.tag)) return JT_VOID;
    if (t.tag == f.tag) return t.tag;
    /* byte and short → short */
    if ((t.tag == JT_BYTE && f.tag == JT_SHORT) || (t.tag == JT_SHORT && f.tag == JT_BYTE))
        return JT_SHORT;
    /* one arm is byte/short/char, the other a constant int that fits in it → that type */
    for (int pass = 0; pass < 2; pass++) {
        jls_const_t narrow = pass ? f : t, wide = pass ? t : f;
        if (wide.tag != JT_INT) continue;
        int32_t v = wide.v.i;
        if (narrow.tag == JT_BYTE  && v >= -128   && v <= 127)   return JT_BYTE;
        if (narrow.tag == JT_SHORT && v >= -32768 && v <= 32767) return JT_SHORT;
        if (narrow.tag == JT_CHAR  && v >= 0      && v <= 65535) return JT_CHAR;
    }
    return binary_promote(unary_promote(t.tag), unary_promote(f.tag));
}

/* §15.27's last two bullets: a name referring to a final variable whose initializer is itself a
 * constant expression. `final` is necessary but not sufficient — `final int n = args.length;` is
 * a final variable with a non-constant initializer, and is not a constant. A blank final (no
 * initializer) is not one either. */
static jls_const_t eval_final_field(const sema_ctx_t* ctx, const sema_field_t* f, int depth) {
    if (!f || !(f->modifiers & ACC_FINAL) || !f->init_expr) return NOT_CONSTANT;
    if (jt_is_reference(f->type)) return NOT_CONSTANT;    /* String constants: see the header note */
    jls_const_t c = eval(ctx, f->init_expr, depth + 1);
    if (c.tag == JT_VOID) return NOT_CONSTANT;
    /* §4.12.4: the constant VARIABLE's type is the FIELD's declared type, not the
     * initializer's — `static final byte B = 1;` is a byte, though `1` is an int. §5.2's
     * assignment conversion already narrowed it at the declaration. */
    return is_numeric(f->type.tag) || f->type.tag == JT_BOOL ? cast_to(f->type.tag, c) : c;
}

static jls_const_t eval(const sema_ctx_t* ctx, const ast_expr_t* e, int depth) {
    if (!e || depth > JLS_CONST_MAX_DEPTH) return NOT_CONSTANT;
    switch (e->tag) {

    /* Literals of primitive type. (String literals are desugared away by the parser.) */
    case AST_INTLIT:    return mk_int(e->int_lit.value);
    case AST_LONGLIT:   return mk_long(e->long_lit.value);
    case AST_FLOATLIT:  return mk_float(e->float_lit.value);
    case AST_DOUBLELIT: return mk_double(e->double_lit.value);
    case AST_BOOLLIT:   return mk_bool(e->bool_lit.value);
    case AST_CHARLIT:   { jls_const_t c; c.tag = JT_CHAR; c.v.i = e->char_lit.value; return c; }

    /* Casts to primitive types. */
    case AST_CAST: {
        java_type_tag_t target = prim_tag_of_type(e->cast.ty);
        if (target == JT_VOID) return NOT_CONSTANT;
        jls_const_t inner = eval(ctx, e->cast.e, depth + 1);
        if (inner.tag == JT_VOID) return NOT_CONSTANT;
        return cast_to(target, inner);
    }

    /* The unary operators +, -, ~ and ! — but not ++ or --. */
    case AST_UNARY: {
        jls_const_t a = eval(ctx, e->unary.e, depth + 1);
        if (a.tag == JT_VOID) return NOT_CONSTANT;
        switch (e->unary.op) {
        case AST_POS:
            if (!is_numeric(a.tag)) return NOT_CONSTANT;
            return cast_to(unary_promote(a.tag), a);
        case AST_NEG: {
            if (!is_numeric(a.tag)) return NOT_CONSTANT;
            java_type_tag_t k = unary_promote(a.tag);
            if (k == JT_DOUBLE) return mk_double(-as_double(a));
            if (k == JT_FLOAT)  return mk_float(-as_float(a));
            if (k == JT_LONG)   return mk_long((int64_t)(0 - (uint64_t)as_long(a)));
            return mk_int((int32_t)(0 - (uint32_t)as_int(a)));
        }
        case AST_BITNOT: {
            if (!is_integral(a.tag)) return NOT_CONSTANT;
            if (unary_promote(a.tag) == JT_LONG) return mk_long(~as_long(a));
            return mk_int(~as_int(a));
        }
        case AST_LOGNOT:
            if (a.tag != JT_BOOL) return NOT_CONSTANT;
            return mk_bool(!a.v.b);
        default:
            return NOT_CONSTANT;   /* ++ / -- */
        }
    }

    case AST_BINARY: {
        /* && and || are listed, so both operands must themselves be constant expressions —
         * `true || f()` is not one, short-circuit notwithstanding. */
        if (e->binary.op == AST_AND || e->binary.op == AST_OR) {
            jls_const_t a = eval(ctx, e->binary.lhs, depth + 1);
            jls_const_t b = eval(ctx, e->binary.rhs, depth + 1);
            if (a.tag != JT_BOOL || b.tag != JT_BOOL) return NOT_CONSTANT;
            return mk_bool(e->binary.op == AST_AND ? (a.v.b && b.v.b) : (a.v.b || b.v.b));
        }
        jls_const_t a = eval(ctx, e->binary.lhs, depth + 1);
        if (a.tag == JT_VOID) return NOT_CONSTANT;
        jls_const_t b = eval(ctx, e->binary.rhs, depth + 1);
        if (b.tag == JT_VOID) return NOT_CONSTANT;
        switch (e->binary.op) {
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_REM:
            return arith(e->binary.op, a, b);
        case AST_SHL: case AST_SHR: case AST_USHR:
            return shift(e->binary.op, a, b);
        case AST_LT: case AST_GT: case AST_LE: case AST_GE: case AST_EQ: case AST_NE:
            return compare(e->binary.op, a, b);
        case AST_BITAND: case AST_BITOR: case AST_BITXOR:
            return bitwise(e->binary.op, a, b);
        default:
            return NOT_CONSTANT;
        }
    }

    case AST_TERNARY: {
        jls_const_t t = eval(ctx, e->ternary.test, depth + 1);
        if (t.tag != JT_BOOL) return NOT_CONSTANT;
        jls_const_t a = eval(ctx, e->ternary.then,  depth + 1);
        if (a.tag == JT_VOID) return NOT_CONSTANT;
        jls_const_t b = eval(ctx, e->ternary.else_, depth + 1);
        if (b.tag == JT_VOID) return NOT_CONSTANT;
        java_type_tag_t k = conditional_tag(a, b);
        if (k == JT_VOID) return NOT_CONSTANT;
        jls_const_t chosen = t.v.b ? a : b;
        return k == JT_BOOL ? chosen : cast_to(k, chosen);
    }

    /* Simple names referring to final variables with constant initializers. */
    case AST_IDENT: {
        const sema_ident_info_t* info = sema_ident_kind(ctx, e);
        if (!info) return NOT_CONSTANT;
        switch (info->kind) {
        case SEMA_IDENT_LOCAL:
            /* A parameter is never a constant variable: it has no initializer. */
            if (!info->var_is_final || !info->var_init) return NOT_CONSTANT;
            return eval(ctx, info->var_init, depth + 1);
        case SEMA_IDENT_STATIC_FIELD:
        case SEMA_IDENT_INSTANCE_FIELD:
            return eval_final_field(ctx, info->field, depth);
        default:
            return NOT_CONSTANT;
        }
    }

    /* Qualified names TypeName.Identifier referring to final variables with constant
     * initializers. sema resolved the access; a non-field qualified access has no entry. */
    case AST_FIELDACCESS:
        return eval_final_field(ctx, sema_resolved_field(ctx, e), depth);

    default:
        return NOT_CONSTANT;
    }
}

jls_const_t jls_const_eval(const sema_ctx_t* ctx, const ast_expr_t* e) {
    return eval(ctx, e, 0);
}

bool jls_const_is_constant(const sema_ctx_t* ctx, const ast_expr_t* e) {
    return eval(ctx, e, 0).tag != JT_VOID;
}

bool jls_const_is_true(const sema_ctx_t* ctx, const ast_expr_t* e) {
    jls_const_t c = eval(ctx, e, 0);
    return c.tag == JT_BOOL && c.v.b;
}

bool jls_const_is_false(const sema_ctx_t* ctx, const ast_expr_t* e) {
    jls_const_t c = eval(ctx, e, 0);
    return c.tag == JT_BOOL && !c.v.b;
}
