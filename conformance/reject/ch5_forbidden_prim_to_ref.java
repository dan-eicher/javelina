// JLS 5.1.7
// EXPECT illegal cast
//
// "Except for the string conversions, there is no permitted conversion from any primitive
// type to any reference type." A cast is not a string conversion (§5.5), so this is the case
// that separates `(Object) 1` from `"" + 1`.
class Ch5PrimToRef {
    static Object f() {
        int i = 1;
        return (Object) i;
    }
}
