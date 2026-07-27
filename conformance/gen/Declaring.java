// Declaring — a snippet that needs companion TYPE DECLARATIONS beside the case class.
//
// A separate interface rather than a method on Snippet, for two reasons. Java 1.0 has no
// default methods, so putting it on Snippet would mean adding `return Strs.none();` to all
// fifty-seven implementations — churn that says nothing. And the separation is honest: almost
// no snippet needs this, so `instanceof Declaring` reads as "this one is unusual", which it is.
//
// It exists because some sections are ABOUT declarations and cannot be reached from an
// expression at all. §4.5.3 names seven kinds of variable, and three of them — a class
// variable, an instance variable, a constructor parameter — cannot be introduced by any
// statement. Chapters 6 (Names), 8 (Classes) and 9 (Interfaces) are the same problem at scale.
// Without this the only choices were to leave those sections permanently UNCOVERED, or to
// claim them on the subset an expression happens to reach — which marks a section covered
// while most of what it says goes untested, and is exactly what the cardinality gate exists
// to make visible elsewhere.
//
// Emit hoists the declarations to file scope and deduplicates by text, so two snippets naming
// the same helper share one. Names must therefore be globally unique across libraries — every
// case is one compilation unit — so prefix them the way snippet ids are prefixed.
public interface Declaring {

    /** Whole top-level class or interface declarations, as source. */
    String[] decls();

    /** Single-type-import declarations, as the imported name alone ("java.util.Vector").
     *  Emit writes the `import ...;` line and places it ahead of every declaration, which is
     *  where §7.5 requires it. Separate from decls() for that ordering reason, and because
     *  §4.4 counts an imported type as a position in its own right — the one position that is
     *  not reachable from the body of a class at all. */
    String[] imports();
}
