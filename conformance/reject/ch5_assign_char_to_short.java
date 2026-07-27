// JLS 5.2
// EXPECT incompatible initializer for 's'
//
// The second line of the spec's same example — `s = c;` is rejected for the mirror reason.
// Both directions are listed because char is UNSIGNED: neither range contains the other, so
// this is not a case where one of the two assignments could be waved through.
class Ch5CharToShort {
    static void f() {
        char c = 1;
        short s = c;
    }
}
