// JLS 14.19
// EXPECT unreachable statement
//
// "A local variable declaration statement can be reached iff... the preceding statement can
// complete normally." A return never completes normally (§14.15), so nothing may follow it in
// the same block.
//
// This is the structural half of the rule — reachability follows from can-complete-normally,
// not from any dataflow — and it is why §14.19 is decidable at all.
class Ch14AfterReturn {
    static int f() {
        return 1;
        int n = 2;
    }
}
