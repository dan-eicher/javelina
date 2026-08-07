package javelina.peg;

// A zero-width position test: the other place the library calls into user code.
//
// This is why there are no anchor nodes. `$` is expressible in core PEG as
// !any, but `^` under multiline semantics is a question about the position
// itself, which no combination of PEG operators can ask. One hook answers it,
// and answers \b, indentation sensitivity, and anything else positional — so a
// single client's needs never dictate the node set.
//
// A predicate must not consume: it is handed the position and reports, and the
// machine does not advance on its behalf.
//
// Single-method for the same reason as PegAction: it becomes a lambda on a port
// to Java 8 or later without touching this file.
public interface PegPredicate {
    boolean holds(String input, int pos, int end);
}
