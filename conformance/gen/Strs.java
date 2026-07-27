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

    /** Sort in place, lexicographically. An insertion sort because the arrays here are tens
     *  of elements and JLS 1.0's java.util has no Arrays (that class arrives in 1.2), so the
     *  alternative is not a library call but a second hand-rolled sort somewhere else. */
    public static void sort(String[] a) {
        for (int i = 1; i < a.length; i++) {
            String v = a[i];
            int j = i - 1;
            while (j >= 0 && a[j].compareTo(v) > 0) { a[j + 1] = a[j]; j--; }
            a[j + 1] = v;
        }
    }
}
