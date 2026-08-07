package javelina.peg;

// The cursor pegc-generated Java parsers run on — the Java sibling of
// pegc/runtime/peg_runtime.h plus the runtime block CParserImpl.frame pastes
// into every generated C parser. Same semantics, ported rather than reinvented:
// the whitespace table, the comment specs (plain, nested, and structured), and
// the skip memo are all the C runtime's, with two deliberate differences.
//
// A mark is an int, not a struct. The C runtime carries line and column
// through every save and restore because it maintains them incrementally on
// each advance; here they are recovered by scanning from the start of the
// input, which only a diagnostic ever asks for. That makes `advance` a single
// increment and a mark free to take, which matters because PEG backtracking
// takes one per alternative, per repetition step, and per predicate.
//
// No field is final. JLS 1.0 section 8.3.1.2: "A field can be declared final,
// in which case its declarator must include a variable initializer or a
// compile-time error occurs."
public class PegCursor {

    public static final int MAX_COMMENTS = 4;

    private char[] input;
    private int pos;
    private int end;

    // The whitespace predicate, tabulated. Java 1.0 has no function values, so
    // where the C runtime stores a function pointer and tabulates it once, the
    // generated parser fills this table directly at setup. The classification
    // is pure, so the table is exact.
    private boolean[] wsTab;

    private String[] commentOpen;
    private String[] commentClose;
    private boolean[] commentNested;
    private boolean[] commentStructured;
    private int commentCount;

    // The skip memo. Backtracking re-enters `skip` at positions it has already
    // skipped; skipping is deterministic in position, so one (from, to) pair
    // erases nearly all of the repeated work.
    private int skipFrom;
    private int skipTo;

    public PegCursor(char[] input) {
        this.input = input;
        this.pos = 0;
        this.end = input.length;
        this.wsTab = new boolean[256];
        this.commentOpen = new String[MAX_COMMENTS];
        this.commentClose = new String[MAX_COMMENTS];
        this.commentNested = new boolean[MAX_COMMENTS];
        this.commentStructured = new boolean[MAX_COMMENTS];
        this.commentCount = 0;
        this.skipFrom = -1;
        this.skipTo = -1;
    }

    // ── Position ───────────────────────────────────────────

    public boolean atEnd() {
        return pos >= end;
    }

    public char peekChar() {
        return input[pos];
    }

    public void advance() {
        pos++;
    }

    public int save() {
        return pos;
    }

    public void restore(int mark) {
        pos = mark;
    }

    public int length() {
        return end;
    }

    // ── Matching ───────────────────────────────────────────

    public boolean match(String s) {
        int n = s.length();
        if (pos + n > end) return false;
        for (int i = 0; i < n; i++) {
            if (input[pos + i] != s.charAt(i)) return false;
        }
        pos += n;
        return true;
    }

    public boolean peekAt(String s) {
        int n = s.length();
        if (pos + n > end) return false;
        for (int i = 0; i < n; i++) {
            if (input[pos + i] != s.charAt(i)) return false;
        }
        return true;
    }

    public boolean peekAtChar(char c) {
        return pos < end && input[pos] == c;
    }

    public String text(Span span) {
        return new String(input, span.start, span.len);
    }

    // ── Skip configuration ─────────────────────────────────

    public void setWhitespace(char c, boolean isWhitespace) {
        if (c < 256) wsTab[c] = isWhitespace;
        skipFrom = -1;
    }

    public void addComment(String open, String close, boolean nested, boolean structured) {
        if (commentCount < MAX_COMMENTS) {
            commentOpen[commentCount] = open;
            commentClose[commentCount] = close;
            commentNested[commentCount] = nested;
            commentStructured[commentCount] = structured;
            commentCount++;
        }
        skipFrom = -1;
    }

    // ── Skipping ───────────────────────────────────────────

    public void skip() {
        if (pos == skipFrom && skipTo >= 0) {
            pos = skipTo;
            return;
        }
        int from = pos;
        for (;;) {
            boolean skipped = false;
            while (pos < end && input[pos] < 256 && wsTab[input[pos]]) {
                pos++;
                skipped = true;
            }
            boolean foundComment = false;
            for (int i = 0; i < commentCount; i++) {
                // First-character rejection: at a non-comment position — almost
                // all of them — each spec dies on one comparison.
                if (pos < end && input[pos] != commentOpen[i].charAt(0)) continue;
                if (skipComment(i)) {
                    foundComment = true;
                    break;
                }
            }
            if (!foundComment && !skipped) break;
        }
        skipFrom = from;
        skipTo = pos;
    }

    private boolean skipComment(int i) {
        if (commentStructured[i]) return skipStructured(i);

        String open = commentOpen[i];
        String close = commentClose[i];
        int openLen = open.length();
        if (pos + openLen > end) return false;
        if (!peekAt(open)) return false;
        pos += openLen;

        int closeLen = close.length();
        // A single-LF close is the line-comment idiom, and a line ends at LF or
        // CR (or CRLF), so such a comment terminates on a bare CR too.
        boolean lineClose = (closeLen == 1 && close.charAt(0) == '\n');
        int depth = 1;
        while (pos < end && depth > 0) {
            if (commentNested[i] && pos + openLen <= end && peekAt(open)) {
                pos += openLen;
                depth++;
            } else if (lineClose && (input[pos] == '\n' || input[pos] == '\r')) {
                pos++;
                depth--;
            } else if (pos + closeLen <= end && peekAt(close)) {
                pos += closeLen;
                depth--;
            } else {
                pos++;
            }
        }
        return true;
    }

    // A structured skip element (an annotation like `(@id ... )`): from `open`,
    // walk to the matching close counting generic ()-nesting, treating "..."
    // string literals with backslash escapes as opaque so their parens do not
    // perturb the balance. The `close` delimiter is unused — paren depth ends
    // it. Language-specific awareness (inner comments, identifier validation)
    // is not generic; a grammar needing it overrides this in its own runtime.
    private boolean skipStructured(int i) {
        String open = commentOpen[i];
        int openLen = open.length();
        if (pos + openLen > end) return false;
        if (!peekAt(open)) return false;
        pos += openLen;

        int depth = 1;
        while (pos < end && depth > 0) {
            char c = input[pos];
            if (c == '"') {
                pos++;
                while (pos < end && input[pos] != '"') {
                    if (input[pos] == '\\' && pos + 1 < end) pos++;
                    pos++;
                }
                if (pos < end) pos++;
            } else if (c == '(') {
                depth++;
                pos++;
            } else if (c == ')') {
                depth--;
                pos++;
            } else {
                pos++;
            }
        }
        return true;
    }

    // ── Diagnostics ────────────────────────────────────────
    //
    // Line and column are a pure function of position, so they are computed
    // when asked rather than maintained on every advance. A parse reports at
    // most a handful of positions; it advances millions of times.

    public int lineAt(int mark) {
        int line = 1;
        for (int i = 0; i < mark && i < end; i++) {
            if (input[i] == '\n') line++;
        }
        return line;
    }

    public int columnAt(int mark) {
        int col = 1;
        for (int i = 0; i < mark && i < end; i++) {
            if (input[i] == '\n') col = 1;
            else col++;
        }
        return col;
    }
}
