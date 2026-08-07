package java.util.regex;

import javelina.peg.PegMachine;
import javelina.peg.PegResult;
import javelina.peg.Span;

// A match in progress.
//
// Two machines, because a match and a search are two different grammars: the
// anchored one for matches() and lookingAt(), the search one — carrying the
// FIRST-set skip from section 4.1 of the paper — for find(). Both are built
// once by the Pattern and reused.
//
// Group 0 is the whole match; a group that did not participate reports -1 for
// start and end, and null for group(), as the JDK does.
public final class Matcher {

    private Pattern owner;
    private String input;
    private PegMachine anchoredMachine;
    private PegMachine searchMachine;

    private Span[] caps;
    private boolean hasMatch;
    private int appendPos;
    private int searchFrom;

    Matcher(Pattern owner, String input) {
        this.owner = owner;
        this.input = input;
        this.anchoredMachine = new PegMachine(owner.anchored());
        this.searchMachine = new PegMachine(owner.searching());
        reset();
    }

    public Matcher reset() {
        caps = null;
        hasMatch = false;
        appendPos = 0;
        searchFrom = 0;
        return this;
    }

    public Matcher reset(String newInput) {
        this.input = newInput;
        return reset();
    }

    public Pattern pattern() { return owner; }
    public int groupCount()  { return owner.groupCount(); }

    // ── Matching ───────────────────────────────────────────

    // The whole input, start to finish.
    public boolean matches() {
        PegResult r = anchoredMachine.run(input, 0);
        boolean ok = r.matched && r.end == input.length();
        take(r, ok);
        return ok;
    }

    // A prefix of the input.
    public boolean lookingAt() {
        PegResult r = anchoredMachine.run(input, 0);
        take(r, r.matched);
        return r.matched;
    }

    // The next match anywhere at or after where the last one ended.
    public boolean find() {
        int from = searchFrom;
        if (hasMatch && end() == start()) from = end() + 1;      // zero-width: move on
        else if (hasMatch) from = end();
        if (from > input.length()) { hasMatch = false; return false; }
        return findFrom(from);
    }

    public boolean find(int start) {
        reset();
        return findFrom(start);
    }

    boolean findFrom(int from) {
        if (from < 0 || from > input.length()) {
            hasMatch = false;
            return false;
        }
        PegResult r = searchMachine.run(input, from);
        take(r, r.matched);
        if (r.matched) searchFrom = from;
        return r.matched;
    }

    private void take(PegResult r, boolean ok) {
        hasMatch = ok;
        caps = ok ? r.captures : null;
    }

    // ── Groups ─────────────────────────────────────────────

    // The JDK throws IllegalStateException here, but that class is Java 1.1 and
    // java.lang stays exactly 1.0 — adding it would put a 1.1 class in reach of
    // every 1.0 program to serve one extension package. A forward port replaces
    // these two throws and nothing else.
    private Span slot(int group) {
        if (!hasMatch) throw new RuntimeException("no match available");
        if (group < 0 || group > owner.groupCount()) {
            throw new IndexOutOfBoundsException("no group " + group);
        }
        if (caps == null || group >= caps.length) return null;
        Span s = caps[group];
        // A group still open (len -1) never closed, so it did not participate.
        return (s == null || s.len < 0) ? null : s;
    }

    public String group()          { return group(0); }
    public int start()             { return start(0); }
    public int end()               { return end(0); }

    public String group(int g) {
        Span s = slot(g);
        return s == null ? null : input.substring(s.start, s.start + s.len);
    }

    public int start(int g) {
        Span s = slot(g);
        return s == null ? -1 : s.start;
    }

    public int end(int g) {
        Span s = slot(g);
        return s == null ? -1 : s.start + s.len;
    }

    // ── Replacement ────────────────────────────────────────

    public Matcher appendReplacement(StringBuffer sb, String replacement) {
        if (!hasMatch) throw new RuntimeException("no match available");
        sb.append(input.substring(appendPos, start()));
        for (int i = 0; i < replacement.length(); i++) {
            char c = replacement.charAt(i);
            if (c == '\\' && i + 1 < replacement.length()) {
                sb.append(replacement.charAt(++i));
            } else if (c == '$' && i + 1 < replacement.length()
                       && replacement.charAt(i + 1) >= '0'
                       && replacement.charAt(i + 1) <= '9') {
                // The longest run of digits that still names a group, so $12
                // means group 12 when it exists and group 1 followed by '2'
                // when it does not.
                int g = 0;
                int j = i + 1;
                while (j < replacement.length()
                       && replacement.charAt(j) >= '0' && replacement.charAt(j) <= '9') {
                    int next = g * 10 + (replacement.charAt(j) - '0');
                    if (next > owner.groupCount()) break;
                    g = next;
                    j++;
                }
                String v = group(g);
                if (v != null) sb.append(v);
                i = j - 1;
            } else {
                sb.append(c);
            }
        }
        appendPos = end();
        return this;
    }

    public StringBuffer appendTail(StringBuffer sb) {
        sb.append(input.substring(appendPos));
        return sb;
    }

    public String replaceAll(String replacement) {
        reset();
        StringBuffer sb = new StringBuffer();
        while (find()) appendReplacement(sb, replacement);
        appendTail(sb);
        return sb.toString();
    }

    public String replaceFirst(String replacement) {
        reset();
        StringBuffer sb = new StringBuffer();
        if (find()) appendReplacement(sb, replacement);
        appendTail(sb);
        return sb.toString();
    }
}
