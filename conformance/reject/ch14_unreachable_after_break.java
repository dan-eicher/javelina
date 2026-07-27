// JLS 14.19
// EXPECT unreachable statement
//
// break completes abruptly (§14.13), so a statement following it in the same block is
// unreachable — inside a `while (true)` whose body is otherwise perfectly reachable. The
// enclosing loop being infinite is what makes this a separate case from the return one: the
// method can still complete, and only this statement cannot be reached.
class Ch14AfterBreak {
    static void f() {
        while (true) {
            break;
            int n = 1;
        }
    }
}
