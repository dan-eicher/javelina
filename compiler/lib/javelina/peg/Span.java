package javelina.peg;

// A matched region of the input, as a half-open index pair. Zero-copy: the
// characters stay in the cursor's array, and `PegCursor.text(Span)` materialises
// a String only when a caller actually wants one.
//
// Fields are not final: JLS 1.0 section 8.3.1.2 requires a final field's
// declarator to carry its initializer, and a Span is filled in by `set` after
// the token that produced it has matched.
public class Span {
    public int start;
    public int len;

    public Span() {
        start = 0;
        len = 0;
    }

    public void set(int start, int len) {
        this.start = start;
        this.len = len;
    }

    public int end() {
        return start + len;
    }
}
