package javelina.peg;

// The state a regex parse carries across its productions, handed to the
// generated parser through PegCursor.userData.
//
// pegc emits no fields of its own — a generated parser is productions and
// nothing else — and its entry point takes no arguments, so a start production
// cannot return through a parameter. Both problems have the same answer, and it
// is the one the C runtime already uses: peg_state's void* user_data.
public class RegexParse {

    // What the parse built.
    public Rexp result;

    // The next capture slot. Allocated at the OPENING parenthesis, so group
    // numbering follows source order the way java.util.regex's does.
    public int slots;

    public RegexParse() {
        result = null;
        slots = 0;
    }
}
