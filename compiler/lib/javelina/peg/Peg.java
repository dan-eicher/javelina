package javelina.peg;

// The construction API. Everything a caller builds a grammar from goes through
// here, so the generated node classes stay an implementation detail — nobody
// reads a `kind` tag, which is what keeps that 1.0 stand-in for sealed types
// out of the surface a port would have to preserve.
public class Peg {

    private Peg() { }

    // ── Composition ────────────────────────────────────────

    public static Pexp seq(Pexp[] elems) {
        return new PSeq(elems);
    }

    public static Pexp seq(Pexp a, Pexp b) {
        Pexp[] e = new Pexp[2];
        e[0] = a; e[1] = b;
        return new PSeq(e);
    }

    public static Pexp seq(Pexp a, Pexp b, Pexp c) {
        Pexp[] e = new Pexp[3];
        e[0] = a; e[1] = b; e[2] = c;
        return new PSeq(e);
    }

    // Ordered: the first alternative that matches wins, and the rest are never
    // tried. This is the operation regular expressions do not have — their `|`
    // is commutative, a regex library's is not — and it is why a regex is a PEG.
    public static Pexp choice(Pexp[] alts) {
        return new PChoice(alts);
    }

    public static Pexp choice(Pexp a, Pexp b) {
        Pexp[] e = new Pexp[2];
        e[0] = a; e[1] = b;
        return new PChoice(e);
    }

    // Possessive, as all PEG repetition is: it consumes as much as it can and
    // never gives any back. `star(lit("a"))` followed by `lit("a")` can never
    // match, and that is correct, not a bug.
    public static Pexp star(Pexp body) { return new PStar(body); }
    public static Pexp plus(Pexp body) { return new PPlus(body); }
    public static Pexp opt(Pexp body)  { return new POpt(body); }

    // Zero-width assertions.
    public static Pexp and(Pexp body) { return new PAnd(body); }
    public static Pexp not(Pexp body) { return new PNot(body); }

    // A reference to a named rule. The id stays -1 until Grammar.finish
    // resolves it — a miss is negative, never zero, so an unresolved reference
    // cannot be mistaken for rule zero.
    public static Pexp rule(String name) {
        return new PRule(name, -1);
    }

    // ── Terminals ──────────────────────────────────────────

    public static Pexp any() { return new PAny(); }

    public static Pexp lit(String value) { return new PLiteral(value); }

    public static Pexp range(char lo, char hi) {
        int[] los = new int[1]; los[0] = lo;
        int[] his = new int[1]; his[0] = hi;
        return new PClass(new Charset(los, his, false));
    }

    public static Pexp notRange(char lo, char hi) {
        int[] los = new int[1]; los[0] = lo;
        int[] his = new int[1]; his[0] = hi;
        return new PClass(new Charset(los, his, true));
    }

    public static Pexp chars(String set)    { return charsOf(set, false); }
    public static Pexp notChars(String set) { return charsOf(set, true); }

    private static Pexp charsOf(String set, boolean negated) {
        int n = set.length();
        int[] lo = new int[n];
        int[] hi = new int[n];
        for (int i = 0; i < n; i++) {
            lo[i] = set.charAt(i);
            hi[i] = set.charAt(i);
        }
        return new PClass(new Charset(lo, hi, negated));
    }

    // The general form, for a caller that already has ranges — the regex
    // translation builds classes this way.
    public static Pexp charClass(int[] lo, int[] hi, boolean negated) {
        return new PClass(new Charset(lo, hi, negated));
    }

    public static Pexp test(PegPredicate p) { return new PTest(p); }

    // ── Captures and actions ───────────────────────────────

    // The wrapper form, for a caller writing a grammar by hand. It expands to
    // the two marks the machine actually runs, which is also the form a
    // continuation-passing producer (the regex transformation) emits directly.
    public static Pexp capture(int slot, Pexp body) {
        Pexp[] e = new Pexp[3];
        e[0] = new PCapStart(slot);
        e[1] = body;
        e[2] = new PCapEnd(slot);
        return new PSeq(e);
    }

    public static Pexp capStart(int slot) { return new PCapStart(slot); }
    public static Pexp capEnd(int slot)   { return new PCapEnd(slot); }

    public static Pexp action(PegAction a, Pexp body) {
        return new PAction(a, body);
    }
}
