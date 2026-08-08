package javelina.peg;

// The transformation Pi, from Medeiros, Mascarenhas and Ierusalimschy, "From
// regexes to parsing expression grammars", Science of Computer Programming 93
// (2014) 3-18. Figure and section numbers below are that paper's, by its
// PRINTED page numbers.
//
// The paper's point is that a regex is not a regular expression: a regex
// library's alternation is tried left to right and is therefore not
// commutative, which is exactly PEG ordered choice. Pi makes that precise and
// Lemma 7 proves it correct, so the semantics here rest on a proof rather than
// on whatever a hand-written backtracker happens to do.
//
// What is implemented:
//   Fig. 3  (p. 8)   Pi, the transformation itself
//   Fig. 4  (p. 11)  the empty and null predicates
//   Fig. 5  (p. 12)  f_out, the well-formedness rewrite
//   Fig. 6  (p. 12)  f_in, the same for the inside of a repetition
//   Fig. 7  (p. 15)  Pi for the four regex extensions
//   Fig. 8  (p. 16)  f_out and f_in for those extensions
//   Fig. 9  (p. 17)  FIRST sets
//   4.1     (p. 13)  the search pattern
//   4.2     (p. 13)  possessive repetition when FIRST sets are disjoint
//   4.3     (p. 13)  the two combined
//
// What is not, and will not be: backreferences. Section 6 adapts Pi for exactly
// four extensions and a backreference is not among them; the languages are not
// context-free, so no PEG expresses one.
public class RegexToPeg {

    private Grammar g;
    private int fresh;
    private boolean dotAll;

    /* Mirrors java.util.regex.Pattern.DOTALL, named here so the translator does
     * not depend on the package it is the engine for. */
    public static final int DOTALL = 0x20;

    /* java.util.regex on `.`: "In dotall mode, the expression . matches any
     * character, including a line terminator. By default this expression does
     * not match line terminators." The recognised terminators are LF (000A),
     * CR (000D), NEL (0085), LINE SEPARATOR (2028) and PARAGRAPH SEPARATOR
     * (2029), so an ordinary `.` is the complement of those five. Translating
     * it to a bare Peg.any() made `.` match a newline, which silently turned
     * `>.*\n` into a match over the whole input. */
    private static final int[] TERM_LO = { 0x0A, 0x0D, 0x85, 0x2028 };
    private static final int[] TERM_HI = { 0x0A, 0x0D, 0x85, 0x2029 };

    /* Mirrors java.util.regex.Pattern's other two flags. */
    public static final int CASE_INSENSITIVE = 0x02;
    public static final int MULTILINE        = 0x08;

    private boolean fold;
    private boolean multiline;

    private RegexToPeg(Grammar g, int flags) {
        this.g = g;
        this.fresh = 0;
        this.dotAll = (flags & DOTALL) != 0;
        this.fold = (flags & CASE_INSENSITIVE) != 0;
        this.multiline = (flags & MULTILINE) != 0;
    }

    /* `.` as a PEG expression, under this pattern's flags. */
    private Pexp anyChar() {
        return dotAll ? Peg.any() : Peg.charClass(TERM_LO, TERM_HI, true);
    }

    /* java.util.regex: "By default, case-insensitive matching assumes that only
     * characters in the US-ASCII charset are being matched. Unicode-aware case
     * folding can be enabled by specifying the UNICODE_CASE flag" — which this
     * package does not offer, so the fold is ASCII and only ASCII. */
    private static char asciiLower(char c) {
        return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c;
    }
    private static char asciiUpper(char c) {
        return (c >= 'a' && c <= 'z') ? (char) (c - 32) : c;
    }

    /* One literal character under the current flags. */
    private Pexp oneChar(char c) {
        char lo = asciiLower(c), up = asciiUpper(c);
        if (!fold || lo == up) return Peg.lit(String.valueOf(c));
        int[] a = { lo, up };
        int[] b = { lo, up };
        return Peg.charClass(a, b, false);
    }

    /* A character class under the current flags. Folding ADDS the case-swapped
     * ranges to the set before it is negated, which is right in both polarities:
     * [a-c] also admits A-C, and [^a-c] must reject A-C too. */
    private Pexp classOf(Charset s) {
        if (!fold) return Peg.charClass(s.lo, s.hi, s.negated);
        int n = s.lo.length;
        int[] lo = new int[n * 3];
        int[] hi = new int[n * 3];
        int m = 0;
        for (int i = 0; i < n; i++) {
            lo[m] = s.lo[i]; hi[m] = s.hi[i]; m++;
            /* the part of this range inside a-z, shifted to A-Z, and vice versa */
            int l = s.lo[i] > 'a' ? s.lo[i] : 'a';
            int h = s.hi[i] < 'z' ? s.hi[i] : 'z';
            if (l <= h) { lo[m] = l - 32; hi[m] = h - 32; m++; }
            l = s.lo[i] > 'A' ? s.lo[i] : 'A';
            h = s.hi[i] < 'Z' ? s.hi[i] : 'Z';
            if (l <= h) { lo[m] = l + 32; hi[m] = h + 32; m++; }
        }
        int[] flo = new int[m];
        int[] fhi = new int[m];
        for (int i = 0; i < m; i++) { flo[i] = lo[i]; fhi[i] = hi[i]; }
        return Peg.charClass(flo, fhi, s.negated);
    }

    private String freshName() {
        return "%" + (fresh++);          // '%' cannot occur in a caller's rule name
    }

    // ── Entry points ───────────────────────────────────────

    // An anchored match: the pattern must match starting where the parse
    // starts. Section 2 (p. 7): a standalone regular expression is transformed
    // with epsilon as the continuation, giving a PEG equivalent to `e`.
    public static Grammar anchored(Rexp e, int flags) {
        Grammar g = new Grammar();
        RegexToPeg t = new RegexToPeg(g, flags);
        g.start(t.pi(fOut(e), empty()));
        g.finish();
        return g;
    }

    // A search: find the pattern anywhere in the subject.
    //
    // Section 4.1: the naive search pattern advances one position at a time,
    // which makes the engine backtrack over the whole subject. If every
    // successful match must begin with a character in FIRST(e), the failing
    // prefix can be skipped wholesale:
    //
    //     S -> (![F] .)* (p | .S)
    //
    // Section 4.3 sharpens it when the pattern is a repetition of single
    // characters followed by something else: the repetition can be consumed
    // possessively before retrying, so the retry does not re-scan it.
    public static Grammar search(Rexp e, int flags) {
        Grammar g = new Grammar();
        RegexToPeg t = new RegexToPeg(g, flags);
        Rexp w = fOut(e);
        Pexp p = t.pi(w, empty());

        String s = t.freshName();
        Pexp sref = Peg.rule(s);

        FirstSet f = t.first(w);
        Pexp skip = f.skippable()
            ? Peg.star(Peg.seq(Peg.not(f.toPexp()), Peg.any()))
            : empty();

        // Section 4.3: with a leading repetition of single characters, retry
        // past a possessive run of them rather than one character at a time.
        Pexp retry;
        Rexp head = leadingUnitRepetition(w);
        if (head != null) {
            retry = Peg.seq(Peg.plus(t.pi(head, empty())), sref);
        } else {
            retry = Peg.seq(Peg.any(), sref);
        }

        g.define(s, Peg.seq(skip, Peg.choice(p, retry)));
        g.start(sref);
        g.finish();
        return g;
    }

    private static Pexp empty() {
        return Peg.lit("");
    }

    // ── Fig. 3 and Fig. 7: the transformation ──────────────

    private Pexp pi(Rexp e, Pexp k) {
        switch (e.kind) {
            // Pi(epsilon, G_k) = G_k
            case Rexp.KIND_REMPTY:
                return k;

            // Pi(a, G_k) = G_k[a p_k]
            case Rexp.KIND_RCHAR:
                return Peg.seq(oneChar((char) ((RChar) e).ch), k);
            case Rexp.KIND_RCLASS:
                return Peg.seq(classOf(((RClass) e).set), k);
            case Rexp.KIND_RANY:
                /* anyChar(), not Peg.any(): outside dotall mode `.` excludes the
                 * line terminators. The search scaffolding above and the end
                 * anchor below keep Peg.any() — those step over the subject and
                 * ask "is there a character at all", which newlines answer. */
                return Peg.seq(anyChar(), k);

            // The end anchor needs no predicate: in a PEG, "no more input" is
            // !any. The start anchor is a question about the position, which
            // core PEG cannot ask, so it is the one place a PTest is needed.
            /* java.util.regex: "By default these expressions only match at the
             * beginning and the end of the entire input sequence"; under
             * MULTILINE they match "just after or just before, respectively, a
             * line terminator or the end of the input sequence". Both are
             * questions about the position, so both are PTests — the end anchor
             * can no longer be `!any`, because by default `$` also matches
             * before a FINAL terminator. */
            case Rexp.KIND_RANCHORSTART:
                return Peg.seq(Peg.test(multiline ? (PegPredicate) new AtLineStart()
                                                  : (PegPredicate) new AtInputStart()), k);
            case Rexp.KIND_RANCHOREND:
                return Peg.seq(Peg.test(multiline ? (PegPredicate) new AtLineEnd()
                                                  : (PegPredicate) new AtInputEnd()), k);

            // Pi(e1 e2, G_k) = Pi(e1, Pi(e2, G_k))
            case Rexp.KIND_RSEQ: {
                RSeq s = (RSeq) e;
                // Section 4.2: for e1* e2 with disjoint FIRST sets the
                // repetition can be possessive, because nothing e1 consumes
                // could ever have started an e2 match.
                Pexp opt = repetitionOptimised(s, k);
                if (opt != null) return opt;
                return pi(s.left, pi(s.right, k));
            }

            // Pi(e1 | e2, G_k) = G_2[p_1 | p_2]
            case Rexp.KIND_RALT: {
                RAlt a = (RAlt) e;
                return Peg.choice(pi(a.left, k), pi(a.right, k));
            }

            // Pi(e1*, G_k) = A, with A -> p_1 | p_k
            case Rexp.KIND_RSTAR: {
                String a = freshName();
                Pexp ref = Peg.rule(a);
                Pexp p1 = pi(((RStar) e).body, ref);
                g.define(a, Peg.choice(p1, k));
                return ref;
            }

            // e+ is e e*, e? is e | epsilon. Neither needs its own rule in Pi.
            case Rexp.KIND_RPLUS: {
                Rexp b = ((RPlus) e).body;
                return pi(b, pi(new RStar(b), k));
            }
            case Rexp.KIND_ROPT:
                return Peg.choice(pi(((ROpt) e).body, k), k);

            // A capture is two marks, not a wrapper: Pi is continuation
            // passing, so a wrapper would enclose k as well.
            case Rexp.KIND_RGROUP: {
                RGroup gr = (RGroup) e;
                Pexp inner = pi(gr.body, Peg.seq(Peg.capEnd(gr.slot), k));
                return Peg.seq(Peg.capStart(gr.slot), inner);
            }

            // Fig. 7. An independent expression is transformed against an EMPTY
            // continuation and the result concatenated with the real one — that
            // is what makes it atomic, since the continuation can no longer
            // drive backtracking into it.
            case Rexp.KIND_RATOMIC:
                return Peg.seq(pi(((RAtomic) e).body, empty()), k);

            // "It is the same as ?>e* if the longest-match rule is used. The
            // semantics of Pi guarantees longest match, so it uses this
            // identity" (section 6).
            case Rexp.KIND_RPOSSSTAR:
                return pi(new RAtomic(new RStar(((RPossStar) e).body)), k);

            // Lazy repetition is the star production with the two alternatives
            // flipped: try the rest of the pattern first, and only take another
            // step of the repetition if that fails.
            case Rexp.KIND_RLAZYSTAR: {
                String a = freshName();
                Pexp ref = Peg.rule(a);
                Pexp p1 = pi(((RLazyStar) e).body, ref);
                g.define(a, Peg.choice(k, p1));
                return ref;
            }

            // Negative lookahead is PEG's not-predicate over the independent
            // transformation; positive lookahead is that not-predicate twice.
            case Rexp.KIND_RNEGLOOKAHEAD:
                return Peg.seq(Peg.not(pi(((RNegLookahead) e).body, empty())), k);
            case Rexp.KIND_RLOOKAHEAD:
                return Peg.seq(Peg.not(Peg.not(pi(((RLookahead) e).body, empty()))), k);

            default:
                return k;
        }
    }

    // Section 4.2. Only the disjoint-FIRST case is taken: it turns the
    // repetition possessive, which is a strict win. The general predicated form
    // is not applied, because it inserts a predicate on every iteration and the
    // paper's own measurements show that paying off only when the sets overlap.
    private Pexp repetitionOptimised(RSeq s, Pexp k) {
        Rexp rep = s.left;
        Rexp body;
        if (rep.kind == Rexp.KIND_RSTAR) body = ((RStar) rep).body;
        else return null;

        FirstSet f1 = first(body);
        FirstSet f2 = first(s.right);
        if (f1.isEmpty() || f2.isEmpty() || !f1.disjoint(f2)) return null;

        Pexp p2 = pi(s.right, k);
        Pexp p1 = pi(body, empty());
        return Peg.seq(Peg.star(p1), p2);
    }

    // Section 4.3's precondition: every string the repetition's body matches
    // has length one. Returns the body, or null when the shape does not apply.
    private static Rexp leadingUnitRepetition(Rexp e) {
        if (e.kind != Rexp.KIND_RSEQ) return null;
        Rexp l = ((RSeq) e).left;
        Rexp body;
        if (l.kind == Rexp.KIND_RSTAR) body = ((RStar) l).body;
        else if (l.kind == Rexp.KIND_RPLUS) body = ((RPlus) l).body;
        else return null;
        return unitLength(body) ? body : null;
    }

    private static boolean unitLength(Rexp e) {
        switch (e.kind) {
            case Rexp.KIND_RCHAR:
            case Rexp.KIND_RCLASS:
            case Rexp.KIND_RANY:
                return true;
            case Rexp.KIND_RALT:
                return unitLength(((RAlt) e).left) && unitLength(((RAlt) e).right);
            case Rexp.KIND_RGROUP:
                return unitLength(((RGroup) e).body);
            default:
                return false;
        }
    }

    // ── Fig. 4: empty and null ─────────────────────────────
    //
    // empty(e) asks whether e's language is exactly {epsilon}; null(e) whether
    // epsilon is in it. Section 6 extends both: null is true for every
    // extension except an independent expression, and empty is true for the two
    // lookaheads, which is deliberately conservative.

    static boolean isEmpty(Rexp e) {
        switch (e.kind) {
            case Rexp.KIND_REMPTY:       return true;
            case Rexp.KIND_RSEQ:         return isEmpty(((RSeq) e).left) && isEmpty(((RSeq) e).right);
            case Rexp.KIND_RALT:         return isEmpty(((RAlt) e).left) && isEmpty(((RAlt) e).right);
            case Rexp.KIND_RSTAR:        return isEmpty(((RStar) e).body);
            case Rexp.KIND_RPLUS:        return isEmpty(((RPlus) e).body);
            case Rexp.KIND_ROPT:         return isEmpty(((ROpt) e).body);
            case Rexp.KIND_RGROUP:       return isEmpty(((RGroup) e).body);
            case Rexp.KIND_RATOMIC:      return isEmpty(((RAtomic) e).body);
            case Rexp.KIND_RPOSSSTAR:    return isEmpty(((RPossStar) e).body);
            case Rexp.KIND_RLAZYSTAR:    return isEmpty(((RLazyStar) e).body);
            case Rexp.KIND_RLOOKAHEAD:
            case Rexp.KIND_RNEGLOOKAHEAD:
            case Rexp.KIND_RANCHORSTART:
            case Rexp.KIND_RANCHOREND:   return true;
            default:                     return false;      // a character
        }
    }

    static boolean isNull(Rexp e) {
        switch (e.kind) {
            case Rexp.KIND_REMPTY:       return true;
            case Rexp.KIND_RSEQ:         return isNull(((RSeq) e).left) && isNull(((RSeq) e).right);
            case Rexp.KIND_RALT:         return isNull(((RAlt) e).left) || isNull(((RAlt) e).right);
            case Rexp.KIND_RSTAR:        return true;
            case Rexp.KIND_RPLUS:        return isNull(((RPlus) e).body);
            case Rexp.KIND_ROPT:         return true;
            case Rexp.KIND_RGROUP:       return isNull(((RGroup) e).body);
            case Rexp.KIND_RATOMIC:      return isNull(((RAtomic) e).body);
            case Rexp.KIND_RPOSSSTAR:
            case Rexp.KIND_RLAZYSTAR:
            case Rexp.KIND_RLOOKAHEAD:
            case Rexp.KIND_RNEGLOOKAHEAD:
            case Rexp.KIND_RANCHORSTART:
            case Rexp.KIND_RANCHOREND:   return true;
            default:                     return false;
        }
    }

    // ── Fig. 5 and Fig. 8: f_out ───────────────────────────
    //
    // A regular expression with a subexpression e* where epsilon is in L(e) is
    // not well-formed: the repetition can spin without consuming, and Pi turns
    // it into a left-recursive PEG with no proof tree. f_out rewrites it into
    // one that is well-formed and matches the same language (Lemma 8), so a
    // caller never has to know the distinction.

    static Rexp fOut(Rexp e) {
        switch (e.kind) {
            case Rexp.KIND_RSEQ: {
                RSeq s = (RSeq) e;
                return new RSeq(fOut(s.left), fOut(s.right));
            }
            case Rexp.KIND_RALT: {
                RAlt a = (RAlt) e;
                return new RAlt(fOut(a.left), fOut(a.right));
            }
            case Rexp.KIND_RSTAR: {
                Rexp b = ((RStar) e).body;
                if (!isNull(b))  return new RStar(fOut(b));
                if (isEmpty(b))  return new REmpty();
                return new RStar(fIn(b));
            }
            case Rexp.KIND_RPLUS: {
                Rexp b = ((RPlus) e).body;
                if (!isNull(b))  return new RPlus(fOut(b));
                if (isEmpty(b))  return new REmpty();
                return new RPlus(fIn(b));
            }
            case Rexp.KIND_RPOSSSTAR: {
                Rexp b = ((RPossStar) e).body;
                if (!isNull(b))  return new RPossStar(fOut(b));
                if (isEmpty(b))  return new REmpty();
                return new RPossStar(fIn(b));
            }
            case Rexp.KIND_RLAZYSTAR: {
                Rexp b = ((RLazyStar) e).body;
                if (!isNull(b))  return new RLazyStar(fOut(b));
                if (isEmpty(b))  return new REmpty();
                return new RLazyStar(fIn(b));
            }
            case Rexp.KIND_ROPT:          return new ROpt(fOut(((ROpt) e).body));
            case Rexp.KIND_RGROUP:        return new RGroup(((RGroup) e).slot, fOut(((RGroup) e).body));
            case Rexp.KIND_RATOMIC:       return new RAtomic(fOut(((RAtomic) e).body));
            case Rexp.KIND_RLOOKAHEAD:    return new RLookahead(fOut(((RLookahead) e).body));
            case Rexp.KIND_RNEGLOOKAHEAD: return new RNegLookahead(fOut(((RNegLookahead) e).body));
            default:                      return e;        // epsilon or a character
        }
    }

    // ── Fig. 6 and Fig. 8: f_in ────────────────────────────
    //
    // Called only where not-empty(e) and null(e). It strips epsilon out of a
    // repetition's body without changing what the repetition matches, using the
    // identity (AB)* = (A|B)* when epsilon is in both A and B — which is why a
    // concatenation becomes a choice here, a rewrite that would be wrong
    // anywhere else.

    static Rexp fIn(Rexp e) {
        switch (e.kind) {
            case Rexp.KIND_RSEQ: {
                RSeq s = (RSeq) e;
                return fIn(new RAlt(s.left, s.right));
            }
            case Rexp.KIND_RALT: {
                RAlt a = (RAlt) e;
                Rexp e1 = a.left, e2 = a.right;
                if (isEmpty(e1) && isNull(e2))    return fIn(e2);
                if (isEmpty(e1) && !isNull(e2))   return fOut(e2);
                if (isNull(e1) && isEmpty(e2))    return fIn(e1);
                if (!isNull(e1) && isEmpty(e2))   return fOut(e1);
                if (!isNull(e1) && !isEmpty(e2))  return new RAlt(fOut(e1), fIn(e2));
                if (!isEmpty(e1) && !isNull(e2))  return new RAlt(fIn(e1), fOut(e2));
                return new RAlt(fIn(e1), fIn(e2));
            }
            case Rexp.KIND_RSTAR: {
                Rexp b = ((RStar) e).body;
                return isNull(b) ? fIn(b) : fOut(b);
            }
            case Rexp.KIND_RPLUS: {
                Rexp b = ((RPlus) e).body;
                return isNull(b) ? fIn(b) : fOut(b);
            }
            case Rexp.KIND_RPOSSSTAR: {
                Rexp b = ((RPossStar) e).body;
                return isNull(b) ? fIn(b) : fOut(b);
            }
            case Rexp.KIND_RLAZYSTAR: {
                Rexp b = ((RLazyStar) e).body;
                return isNull(b) ? fIn(b) : fOut(b);
            }
            // e? is e | epsilon, so Fig. 6's choice cases with an empty right
            // side apply directly.
            case Rexp.KIND_ROPT: {
                Rexp b = ((ROpt) e).body;
                return isNull(b) ? fIn(b) : fOut(b);
            }
            case Rexp.KIND_RGROUP:  return new RGroup(((RGroup) e).slot, fIn(((RGroup) e).body));
            case Rexp.KIND_RATOMIC: return new RAtomic(fIn(((RAtomic) e).body));
            default:                return e;
        }
    }

    // ── Fig. 9: FIRST ──────────────────────────────────────
    //
    // The set of characters a match can begin with. Conservative by
    // construction: an atomic grouping reports its subexpression's set, which
    // may be a proper superset of what it actually consumes.

    /* Not static: FIRST decides which positions the search may SKIP, so under
     * CASE_INSENSITIVE it has to name both cases. Reporting only the written
     * case skips every position where the other one starts, and the match is
     * missed entirely — the folded matcher never gets asked. */
    FirstSet first(Rexp e) {
        FirstSet f = new FirstSet();
        switch (e.kind) {
            case Rexp.KIND_RCHAR: {
                char c = (char) ((RChar) e).ch;
                f.add(c, c);
                if (fold) {
                    char lo = asciiLower(c), up = asciiUpper(c);
                    f.add(lo, lo);
                    f.add(up, up);
                }
                return f;
            }
            case Rexp.KIND_RCLASS: {
                Charset s = ((RClass) e).set;
                if (s.negated) { f.all = true; return f; }
                for (int i = 0; i < s.lo.length; i++) {
                    f.add(s.lo[i], s.hi[i]);
                    if (fold) {
                        int l = s.lo[i] > 'a' ? s.lo[i] : 'a';
                        int h = s.hi[i] < 'z' ? s.hi[i] : 'z';
                        if (l <= h) f.add(l - 32, h - 32);
                        l = s.lo[i] > 'A' ? s.lo[i] : 'A';
                        h = s.hi[i] < 'Z' ? s.hi[i] : 'Z';
                        if (l <= h) f.add(l + 32, h + 32);
                    }
                }
                return f;
            }
            case Rexp.KIND_RANY:
                f.all = true;
                return f;
            case Rexp.KIND_RSEQ: {
                RSeq s = (RSeq) e;
                f.union(first(s.left));
                if (isNull(s.left)) f.union(first(s.right));
                return f;
            }
            case Rexp.KIND_RALT:
                f.union(first(((RAlt) e).left));
                f.union(first(((RAlt) e).right));
                return f;
            case Rexp.KIND_RSTAR:     return first(((RStar) e).body);
            case Rexp.KIND_RPLUS:     return first(((RPlus) e).body);
            case Rexp.KIND_ROPT:      return first(((ROpt) e).body);
            case Rexp.KIND_RGROUP:    return first(((RGroup) e).body);
            case Rexp.KIND_RATOMIC:   return first(((RAtomic) e).body);
            case Rexp.KIND_RPOSSSTAR: return first(((RPossStar) e).body);
            case Rexp.KIND_RLAZYSTAR: return first(((RLazyStar) e).body);
            default:
                return f;             // epsilon, anchors, and both lookaheads
        }
    }
}

// The set of characters a pattern can start with, as inclusive ranges.
// `all` means "no useful restriction" — an unrestricted set makes the search
// skip a no-op, so the skip is simply not emitted.
class FirstSet {
    int[] lo;
    int[] hi;
    int n;
    boolean all;

    FirstSet() {
        lo = new int[8];
        hi = new int[8];
        n = 0;
        all = false;
    }

    void add(int a, int b) {
        if (all) return;
        if (n == lo.length) {
            int[] x = new int[n * 2]; System.arraycopy(lo, 0, x, 0, n); lo = x;
            int[] y = new int[n * 2]; System.arraycopy(hi, 0, y, 0, n); hi = y;
        }
        lo[n] = a;
        hi[n] = b;
        n++;
    }

    void union(FirstSet o) {
        if (o.all) { all = true; return; }
        for (int i = 0; i < o.n; i++) add(o.lo[i], o.hi[i]);
    }

    boolean isEmpty() { return !all && n == 0; }

    // Worth emitting a skip for: a set that restricts, and does not cover
    // everything.
    boolean skippable() { return !all && n > 0; }

    boolean disjoint(FirstSet o) {
        if (all || o.all) return false;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < o.n; j++) {
                if (lo[i] <= o.hi[j] && o.lo[j] <= hi[i]) return false;
            }
        }
        return true;
    }

    Pexp toPexp() {
        int[] l = new int[n];
        int[] h = new int[n];
        System.arraycopy(lo, 0, l, 0, n);
        System.arraycopy(hi, 0, h, 0, n);
        return Peg.charClass(l, h, false);
    }
}

// The one anchor that is not expressible in core PEG. `$` is !any; `^` asks
// where the position is, which no combination of PEG operators can ask — so it
// is a predicate, which is what PTest exists for.
class AtInputStart implements PegPredicate {
    public boolean holds(String input, int pos, int end) {
        return pos == 0;
    }
}

/* The line terminators java.util.regex recognises: LF, CR, CRLF as one unit,
 * NEL (0085), LINE SEPARATOR (2028) and PARAGRAPH SEPARATOR (2029). */
class LineTerm {
    static boolean is(char c) {
        return c == '\n' || c == '\r' || c == ''
            || c == ' ' || c == ' ';
    }
    /* Is `pos` in the MIDDLE of a CRLF? That position is inside one terminator,
     * so neither ^ nor $ may match there. */
    static boolean insideCrLf(String input, int pos, int end) {
        return pos > 0 && pos < end
            && input.charAt(pos - 1) == '\r' && input.charAt(pos) == '\n';
    }
}

/* MULTILINE ^: the start of input, or just after a line terminator — but not at
 * the end of input, where there is no line for it to begin. */
class AtLineStart implements PegPredicate {
    public boolean holds(String input, int pos, int end) {
        if (pos == 0) return true;
        if (pos >= end) return false;
        if (LineTerm.insideCrLf(input, pos, end)) return false;
        return LineTerm.is(input.charAt(pos - 1));
    }
}

/* Default $: the end of input, or just before a final terminator. A trailing
 * CRLF counts as the one terminator, so `a$` matches "a\r\n" at the same place
 * it matches "a\n". */
class AtInputEnd implements PegPredicate {
    public boolean holds(String input, int pos, int end) {
        if (pos == end) return true;
        if (LineTerm.insideCrLf(input, pos, end)) return false;
        if (!LineTerm.is(input.charAt(pos))) return false;
        int after = pos + 1;
        if (input.charAt(pos) == '\r' && after < end && input.charAt(after) == '\n') after++;
        return after == end;      /* only a FINAL terminator, nothing beyond it */
    }
}

/* MULTILINE $: the end of input, or just before any line terminator. */
class AtLineEnd implements PegPredicate {
    public boolean holds(String input, int pos, int end) {
        if (pos == end) return true;
        if (LineTerm.insideCrLf(input, pos, end)) return false;
        return LineTerm.is(input.charAt(pos));
    }
}
