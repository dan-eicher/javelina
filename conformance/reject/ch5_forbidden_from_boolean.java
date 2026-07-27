// JLS 5.1.7
// EXPECT illegal cast
//
// "There is no permitted conversion from the type boolean other than the identity conversion
// and string conversion." So `"" + b` is fine and `(int) b` is not — the pair is the rule.
class Ch5FromBoolean {
    static int f() {
        boolean b = true;
        return (int) b;
    }
}
