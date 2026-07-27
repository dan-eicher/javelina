// JLS 5.2
// EXPECT incompatible initializer for 'b'
//
// §5.2's third condition: "The value of the expression ... is representable in the type of
// the variable." 127 is a byte and 128 is not, so the implicit narrowing stops exactly at
// the range boundary rather than wrapping. conformance/jls/Ch5.java asserts the accepting
// side at both bounds (-128 and 127); this is the first value past it.
class Ch5NotRepresentable {
    static void f() {
        byte b = 128;
    }
}
