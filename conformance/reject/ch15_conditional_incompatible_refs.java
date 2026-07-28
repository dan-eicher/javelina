// JLS 15.24
// EXPECT incompatible conditional operands
//
// §15.24, p.368, the last bullet of "The type of a conditional expression is determined as
// follows":
//
//   "If the second and third operands are of different reference types, then it must be
//    possible to convert one of the types to the other type (call this latter type T) by
//    assignment conversion (§5.2); the type of the conditional expression is T. It is a
//    compile-time error if neither type is assignment compatible with the other type."
//
// String and StringBuffer are unrelated: neither is assignment compatible with the other, so
// there is no T. Both are subclasses of Object, but §15.24 asks for a conversion BETWEEN THE
// TWO OPERAND TYPES, not for a common supertype -- Java 1.0 has no least-upper-bound rule, and
// reading one in is how this compiled before: sema.c returned the second operand's type and
// the assignment to Object then succeeded.
//
// The positive half of the same bullet is conformance/gen/Lib15.java's t15.cond.ref.widen,
// where String DOES convert to Object. A compiler that rejected every mixed-reference
// conditional would pass this file and fail that one.
class Ch15CondIncompatibleRefs {
    static Object pick(boolean c, String s, StringBuffer b) {
        return c ? s : b;
    }
}
