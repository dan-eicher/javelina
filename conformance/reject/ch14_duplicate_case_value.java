// JLS 14.9
// EXPECT duplicate case value
//
// "No two of the case constant expressions associated with a switch statement may have the
// same value." Two labels with one value would leave the jump ambiguous, so it is rejected
// rather than resolved by position.
//
// conformance/jls/Ch14.java asserts the legal neighbour — several labels SHARING one statement
// group (`case 1: case 2:`), which is a different thing entirely and is allowed.
class Ch14DuplicateCase {
    static void f(int x) {
        switch (x) {
            case 1: break;
            case 1: break;
        }
    }
}
