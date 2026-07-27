// JLS 5.1.7
// EXPECT incompatible initializer for 'i'
//
// "There is no permitted conversion from the null type to any primitive type." The null
// reference is assignable to every REFERENCE type (§4.1) and to no primitive one.
class Ch5NullToPrim {
    static void f() {
        int i = null;
    }
}
