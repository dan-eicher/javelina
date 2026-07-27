// Snippet — one composable fragment of Java, with its own answer.
//
// THE CONTRACT. Do not change a signature; the snippet libraries are written against it.
//
// A Snippet is a piece of Java source with typed holes, plus a function that composes its
// value from its holes' values. Validity is an INVARIANT of the generator, not a property
// checked afterwards: a hole typed "int" is only ever filled by a Snippet whose type() is
// "int", so every stitched program type-checks BY CONSTRUCTION. A generated program that
// fails to compile is a bug in this generator or in javelinac — never an expected outcome.
public interface Snippet {

    /** Stable dotted id, e.g. "conv.widen.int2long". Unique across the whole registry;
     *  it is the case comment that names which snippet produced a failing line. */
    String   id();

    /** The JLS sections this snippet exercises, e.g. {"5.1.2"}. Emit joins these into the
     *  `// JLS <section>` markers the coverage join reads. */
    String[] sections();

    /** The Java type this snippet's expression yields — "int", "long", "char", "boolean",
     *  "String", a reference type name, or "void" for a statement. */
    String   type();

    /** The required type of each hole, in source order. An empty array is a leaf. */
    String[] holeTypes();

    /** The Java source text, with the holes' already-rendered text substituted in. Must be
     *  a parenthesised expression, or — when type() is "void" — a complete statement. */
    String   render(String[] holes);

    /** The expected value, COMPOSED from the holes' values. Never observed by running. */
    Val      expect(Val[] holes);
}
