// JLS 5.1.7
// EXPECT illegal cast
//
// "There is no permitted conversion to the type boolean other than the identity conversion."
// §5.1.1 says the same thing from the other side: boolean's ONLY conversion is boolean to
// boolean, which is why boolean is not an integral type with two values.
class Ch5ToBoolean {
    static boolean f() {
        return (boolean) 1;
    }
}
