package javelina.peg;

// Helpers the generated regex-syntax parser's actions call, so grammar/regex.peg
// stays one line of Java per production instead of a program embedded in a
// grammar. Also the place where regex sugar is desugared into the Rexp core:
// bounded repetition, the escape classes, and the class-under-construction.
public class RegexBuilder {

    // ── A character class under construction ───────────────

    private int[] lo;
    private int[] hi;
    private int n;
    private boolean negated;

    public RegexBuilder() {
        lo = new int[8];
        hi = new int[8];
        n = 0;
        negated = false;
    }

    public void negate() {
        negated = true;
    }

    public void addChar(char c) {
        addRange(c, c);
    }

    public void addRange(char a, char b) {
        if (n == lo.length) {
            int[] x = new int[n * 2]; System.arraycopy(lo, 0, x, 0, n); lo = x;
            int[] y = new int[n * 2]; System.arraycopy(hi, 0, y, 0, n); hi = y;
        }
        lo[n] = a;
        hi[n] = b;
        n++;
    }

    // \d \D \w \W \s \S inside a class. The negated forms cannot be expressed
    // as ranges inside a positive class, so they are only accepted where the
    // whole class is a single escape — the grammar enforces that.
    public void addEscapeClass(char c) {
        if (c == 'd') { addRange('0', '9'); return; }
        if (c == 'w') { addRange('a', 'z'); addRange('A', 'Z'); addRange('0', '9'); addChar('_'); return; }
        if (c == 's') { addChar(' '); addChar('\t'); addChar('\n'); addChar('\r'); addChar('\f'); return; }
        addChar(c);
    }

    public Rexp build() {
        int[] a = new int[n];
        int[] b = new int[n];
        System.arraycopy(lo, 0, a, 0, n);
        System.arraycopy(hi, 0, b, 0, n);
        return new RClass(new Charset(a, b, negated));
    }

    // ── Composition ────────────────────────────────────────

    public static Rexp seq(Rexp a, Rexp b) {
        if (a == null) return b;
        if (b == null) return a;
        return new RSeq(a, b);
    }

    public static Rexp alt(Rexp a, Rexp b) {
        return new RAlt(a, b);
    }

    public static Rexp empty() {
        return new REmpty();
    }

    public static Rexp literal(char c) {
        return new RChar(c);
    }

    // \d \w \s and their negations, outside a class.
    public static Rexp escapeClass(char c) {
        boolean neg = (c == 'D' || c == 'W' || c == 'S');
        char base = neg ? (char) (c + 32) : c;      // 'D' -> 'd'
        RegexBuilder b = new RegexBuilder();
        b.addEscapeClass(base);
        if (neg) b.negate();
        return b.build();
    }

    // The escapes that stand for one literal character.
    public static Rexp escapeChar(char c) {
        if (c == 'n') return new RChar('\n');
        if (c == 't') return new RChar('\t');
        if (c == 'r') return new RChar('\r');
        if (c == 'f') return new RChar('\f');
        if (c == '0') return new RChar('\0');
        return new RChar(c);                        // \. \\ \| \( and friends
    }

    // ── Quantifiers ────────────────────────────────────────
    //
    // Bounded repetition is sugar: e{2,4} is e e e? e?, e{2,} is e e e*, and
    // e{2} is e e. Expanding it here keeps the core free of a counted
    // repetition node the machine would otherwise have to run.

    public static Rexp bounded(Rexp e, int min, int max) {
        Rexp r = null;
        for (int i = 0; i < min; i++) r = seq(r, e);
        if (max < 0) {
            r = seq(r, new RStar(e));
        } else {
            for (int i = min; i < max; i++) r = seq(r, new ROpt(e));
        }
        return r == null ? empty() : r;
    }

    // A quantifier's modifier: '+' possessive, '?' lazy, absent greedy.
    public static Rexp quantify(Rexp e, char kind, char mod) {
        if (kind == '*') {
            if (mod == '+') return new RPossStar(e);
            if (mod == '?') return new RLazyStar(e);
            return new RStar(e);
        }
        if (kind == '+') {
            // e+ is e e*, and the modifier belongs to the star half.
            return seq(e, quantify(e, '*', mod));
        }
        // e? — a lazy optional prefers to skip, a possessive one never gives back.
        if (mod == '?') return new RAlt(empty(), e);
        if (mod == '+') return new RAtomic(new ROpt(e));
        return new ROpt(e);
    }
}
