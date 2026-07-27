// JLS 5.1.7
// EXPECT does not implement
//
// "There is no permitted conversion from class type S to interface type K if S is final and
// does not implement K."
//
// FINAL is load-bearing here, unlike the class-to-class bullet: a non-final S may have a
// subclass that implements K, so the cast is only provably wrong when S can have none.
// conformance/jls/Ch5.java's §5.1.5 checks assert the non-final case still compiles and
// throws at run time, which is the other half of this rule.
interface Ch5FinalIface { void k(); }
final class Ch5FinalClass { int x; }

class Ch5FinalToIface {
    static Ch5FinalIface f() {
        Ch5FinalClass a = new Ch5FinalClass();
        return (Ch5FinalIface) a;
    }
}
