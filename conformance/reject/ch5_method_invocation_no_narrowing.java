// JLS 5.3
// EXPECT cannot convert to parameter type
//
// The spec's own example, verbatim, including its comment:
//
//     static int m(byte a, int b) { return a+b; }
//     static int m(short a, short b) { return a-b; }
//     m(12, 2);                  // compile-time error
//
// "causes a compile-time error because the integer literals 12 and 2 have type int, so
// neither method m matches under the rules of (§15.11.2)."
//
// This is the boundary between §5.2 and §5.3: the same literals WOULD narrow implicitly in
// an assignment. Method invocation conversion deliberately excludes that, so the two
// contexts are not interchangeable — which is why Ch5.java's §5.3 checks can only cover the
// conversions that ARE allowed, and this case has to live here.
class Ch5NoNarrowingAtCall {
    static int m(byte a, int b) { return a + b; }
    static int m(short a, short b) { return a - b; }
    static void f() {
        m(12, 2);
    }
}
