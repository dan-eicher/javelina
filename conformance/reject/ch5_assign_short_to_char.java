// JLS 5.2
// EXPECT incompatible initializer for 'c'
//
// The spec's own example, verbatim: "because not all short values are char values, and
// neither are all char values short values."
//
//     short s = 123;
//     char c = s;                // error: would require cast
//
// §5.2's implicit narrowing needs a CONSTANT expression of type int; `s` is a variable, so
// none of the three conditions apply and only an explicit cast will do.
class Ch5ShortToChar {
    static void f() {
        short s = 123;
        char c = s;
    }
}
