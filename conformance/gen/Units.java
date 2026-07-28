// Units — a template whose program is SEVERAL compilation units.
//
// Most templates render one unit, because most rules can be broken inside one. Chapter 6's
// cannot: access control, protected access and package-qualified names are rules ABOUT the
// boundary between packages, so the smallest program that exhibits one is two files in two
// packages. §7.3 allows only one `package` declaration per compilation unit, so this is not a
// formatting preference — a single-file version of such a program does not exist.
//
// A separate interface rather than a method on Snippet, for the same reason as Declaring:
// Java 1.0 has no default methods, and `instanceof Units` reads as "this one is unusual".
//
// Emit writes these as a DIRECTORY named for the template, laid out so javelinac's --libdir
// walk finds each unit under the package path its declaration names.
public interface Units {

    /** Relative paths inside the case directory, e.g. "points/Point.java". The directory
     *  structure has to match the package declarations: javelinac walks directories. */
    String[] unitPaths();

    /** The source of each unit, in the same order and of the same length as unitPaths(). */
    String[] unitSources();
}
