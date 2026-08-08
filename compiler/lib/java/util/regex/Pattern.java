package java.util.regex;

import java.util.Vector;
import javelina.peg.Grammar;
import javelina.peg.PegCursor;
import javelina.peg.PegException;
import javelina.peg.RGroup;
import javelina.peg.RegexBuilder;
import javelina.peg.RegexParse;
import javelina.peg.RegexParser;
import javelina.peg.RegexToPeg;
import javelina.peg.Rexp;

// A compiled pattern.
//
// This is a post-1.0 extension: JLS 1.0 has no regex package at all. It is a
// thin facade — the syntax is parsed by a pegc-generated parser, the resulting
// tree is transformed into a PEG by the proof-backed transformation in
// javelina.peg.RegexToPeg, and a PEG machine runs it. Nothing about matching
// happens here.
//
// Group 0 is the whole match, and groups 1..n are the parenthesised groups in
// source order, as java.util.regex numbers them: the parser starts allocating
// user slots at 1 and the whole pattern is wrapped in slot 0.
public final class Pattern {

    public static final int CASE_INSENSITIVE = 0x02;
    public static final int MULTILINE        = 0x08;
    public static final int LITERAL          = 0x10;
    public static final int DOTALL           = 0x20;

    private String source;
    private int patternFlags;
    private Grammar anchoredGrammar;
    private Grammar searchGrammar;
    private int groups;

    private Pattern(String source, int flags) {
        this.source = source;
        this.patternFlags = flags;

        Rexp tree;
        RegexParse st = new RegexParse();
        st.slots = 1;                       // slot 0 is reserved for the whole match

        if ((flags & LITERAL) != 0) {
            tree = literalTree(source);
        } else {
            RegexParser rp = new RegexParser(source.toCharArray());
            PegCursor cur = rp.cursor();
            cur.userData = st;
            boolean ok;
            try {
                ok = rp.parse();
            } catch (PegException e) {
                // The grammar rejects a construct outright — a backreference is
                // the one that exists. Its message explains why.
                throw new PatternSyntaxException(e.getMessage(), source, cur.save());
            }
            if (!ok || cur.save() != source.length()) {
                throw new PatternSyntaxException("cannot parse the pattern", source, cur.save());
            }
            tree = st.result;
        }

        this.groups = st.slots - 1;
        Rexp whole = new RGroup(0, tree);
        this.anchoredGrammar = RegexToPeg.anchored(whole, patternFlags);
        this.searchGrammar = RegexToPeg.search(whole, patternFlags);
    }

    private static Rexp literalTree(String s) {
        Rexp r = null;
        for (int i = 0; i < s.length(); i++) {
            Rexp c = RegexBuilder.literal(s.charAt(i));
            r = (r == null) ? c : RegexBuilder.seq(r, c);
        }
        return r == null ? RegexBuilder.empty() : r;
    }

    public static Pattern compile(String regex) {
        return new Pattern(regex, 0);
    }

    public static Pattern compile(String regex, int flags) {
        return new Pattern(regex, flags);
    }

    public Matcher matcher(String input) {
        return new Matcher(this, input);
    }

    public String pattern() { return source; }
    public int flags()      { return patternFlags; }
    public String toString() { return source; }

    public static boolean matches(String regex, String input) {
        return compile(regex).matcher(input).matches();
    }

    // Package-private: what a Matcher needs.
    Grammar anchored()  { return anchoredGrammar; }
    Grammar searching() { return searchGrammar; }
    int groupCount()    { return groups; }

    // ── split ──────────────────────────────────────────────

    public String[] split(String input) {
        return split(input, 0);
    }

    public String[] split(String input, int limit) {
        Vector parts = new Vector();
        Matcher m = matcher(input);
        int at = 0;
        while (m.find()) {
            if (limit > 0 && parts.size() == limit - 1) break;
            // A zero-width match at position 0 does not produce a leading
            // empty string, matching the JDK.
            if (m.end() == 0 && m.start() == 0) {
                if (!m.findFrom(1)) break;
                continue;
            }
            parts.addElement(input.substring(at, m.start()));
            at = m.end();
        }
        parts.addElement(input.substring(at));

        // With no limit, trailing empty strings are dropped.
        int n = parts.size();
        if (limit == 0) {
            while (n > 0 && ((String) parts.elementAt(n - 1)).length() == 0) n--;
        }
        String[] out = new String[n];
        for (int i = 0; i < n; i++) out[i] = (String) parts.elementAt(i);
        return out;
    }

    // Every metacharacter escaped, so the result matches `s` literally.
    public static String quote(String s) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '\\' || c == '|' || c == '*' || c == '+' || c == '?'
             || c == '(' || c == ')' || c == '[' || c == ']' || c == '{'
             || c == '}' || c == '.' || c == '^' || c == '$') {
                sb.append('\\');
            }
            sb.append(c);
        }
        return sb.toString();
    }
}
