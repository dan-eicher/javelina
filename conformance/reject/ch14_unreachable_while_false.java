// JLS 14.19
// EXPECT unreachable statement
//
// "The contained statement [of a while] is reachable iff the while statement is reachable and
// the condition expression is not a constant expression whose value is false."
//
// So `while (false)` is an ERROR, while `if (false)` is not — §14.19 exempts the if statement
// on purpose, "to allow... conditional compilation". conformance/jls/Ch14.java asserts the
// accepted side of both, using a non-final local for the loop so its condition is not a
// §15.27 constant expression.
class Ch14WhileFalse {
    static void f() {
        int n = 0;
        while (false) n = 1;
    }
}
