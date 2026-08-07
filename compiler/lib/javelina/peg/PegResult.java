package javelina.peg;

// What a parse produced. Not a boolean — a boolean is what a matcher wants, and
// designing to the matcher is how a library ends up useless for anything else.
//
// `value` is the only Object in the public surface, so a port to a generic Java
// drops in exactly one type parameter.
//
// Fields are not final: JLS 1.0 section 8.3.1.2 requires a final field's
// declarator to carry its initializer, and these are filled in by the machine.
public class PegResult {

    // Did the start expression match at all. A match need not reach the end of
    // input; `end` says where it stopped and the caller decides whether a
    // prefix match counts.
    public boolean matched;
    public int end;

    // The value returned by the outermost action, or null when the grammar has
    // no actions — which is the whole of the regex case.
    public Object value;

    // Capture slots, indexed by the slot number given to Peg.capture. A slot
    // that never matched is null. An array of Span rather than parallel
    // int[] start/end arrays: parallel arrays are the 1.0 idiom a modern port
    // would want to be a record, and changing that shape would break callers.
    public Span[] captures;

    // The furthest position the parse reached before failing, and what was
    // wanted there. A matcher never looks at these; a parser is unusable
    // without them, which is exactly why they are here and not deferred until
    // the first client complains.
    public int failPos;
    public String[] expected;

    public PegResult() {
        matched = false;
        end = 0;
        value = null;
        captures = new Span[0];
        failPos = 0;
        expected = new String[0];
    }
}
