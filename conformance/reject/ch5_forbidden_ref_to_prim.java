// JLS 5.1.7
// EXPECT illegal cast
//
// "There is no permitted conversion from any reference type to any primitive type."
class Ch5RefToPrim {
    static int f(Object o) {
        return (int) o;
    }
}
