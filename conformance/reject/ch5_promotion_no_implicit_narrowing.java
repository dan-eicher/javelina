// JLS 5.6
// EXPECT incompatible initializer for 'c'
//
// "Numeric promotion contexts allow the use of an identity conversion or a widening
// primitive conversion" — and nothing else. So byte + byte promotes both operands to int
// (§5.6.2's last rule) and the SUM is an int expression, which does not assign back to a
// byte: `a + b` is not a constant expression, so §5.2's implicit narrowing cannot rescue it
// either.
//
// Ch5.java's §5.6 checks assert the value side (100 + 100 is 200, not a wrapped -56); this
// asserts that the type side is what makes that true.
class Ch5PromotionNarrowing {
    static void f() {
        byte a = 100, b = 100;
        byte c = a + b;
    }
}
