// JLS 14.8
// EXPECT if condition must be boolean
//
// "The Expression must have type boolean, or a compile-time error occurs."
//
// No conversion rescues it: §5.1.7 permits no conversion TO boolean other than the identity
// one, so an int condition is not merely unconventional here, it is unconvertible. This is the
// rule that makes `if (x = 1)` a compile error in Java where C accepts it.
class Ch14IfNotBoolean {
    static void f() {
        if (1) return;
    }
}
