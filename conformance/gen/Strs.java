// Strs — String[] literals, for a language that has none.
//
// JLS 1.0 §15.9 "Array Creation Expressions" is
//
//     new PrimitiveType DimExprs Dims_opt
//     new ClassOrInterfaceType DimExprs Dims_opt
//
// with no ArrayInitializer alternative: `new String[] { "a" }` is Java 1.1 syntax and
// javelinac rejects it, correctly. §10.6 array initializers exist only in a declaration.
// Every Snippet needs two String[] constants — sections() and holeTypes() — so rather than
// each one declaring a local and returning it, they are built here.
//
// Snippet libraries are the caller: `return Strs.of("15.16.2", "4.2.2");`
public final class Strs {

    private Strs() {}

    /** The empty array — a leaf's holeTypes(). */
    public static String[] none() { return new String[0]; }

    public static String[] of(String a) {
        String[] s = { a }; return s;
    }
    public static String[] of(String a, String b) {
        String[] s = { a, b }; return s;
    }
    public static String[] of(String a, String b, String c) {
        String[] s = { a, b, c }; return s;
    }
    public static String[] of(String a, String b, String c, String d) {
        String[] s = { a, b, c, d }; return s;
    }
    public static String[] of(String a, String b, String c, String d, String e) {
        String[] s = { a, b, c, d, e }; return s;
    }
}
