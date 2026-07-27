// JLS 14.15
// EXPECT missing return value
//
// "A return statement with no Expression must be contained in the body of a method that is
// declared, using the keyword void, not to return any value."
//
// The mirror case — `return v;` in a void method — is the other half of the same rule; this
// one is the direction a caller would notice, since the invocation has a value to consume and
// there is nothing to hand it.
class Ch14ReturnNoValue {
    static int f() {
        return;
    }
}
