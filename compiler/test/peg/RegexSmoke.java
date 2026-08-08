// Pins for the regex-to-PEG transformation (javelina.peg.RegexToPeg).
//
// Where possible these ARE the paper's examples, so the pin and the proof are
// about the same thing: Medeiros, Mascarenhas and Ierusalimschy, "From regexes
// to parsing expression grammars", Sci. Comput. Program. 93 (2014) 3-18,
// cited by printed page.
import javelina.peg.Grammar;
import javelina.peg.Peg;
import javelina.peg.PegMachine;
import javelina.peg.PegResult;
import javelina.peg.RAlt;
import javelina.peg.RAnchorEnd;
import javelina.peg.RAnchorStart;
import javelina.peg.RAny;
import javelina.peg.RAtomic;
import javelina.peg.RegexToPeg;
import javelina.peg.RChar;
import javelina.peg.REmpty;
import javelina.peg.RGroup;
import javelina.peg.RLazyStar;
import javelina.peg.RLookahead;
import javelina.peg.RNegLookahead;
import javelina.peg.ROpt;
import javelina.peg.RPlus;
import javelina.peg.RPossStar;
import javelina.peg.RSeq;
import javelina.peg.RStar;
import javelina.peg.Rexp;

public class RegexSmoke {

    static int failures = 0;

    public static int run() {
        orderedAlternation();
        starGivesBackWhatPegStarWillNot();
        wellFormednessRewrite();
        extensions();
        anchorsAndGroups();
        search();
        return failures;
    }

    static void ok(boolean cond, String what) {
        if (!cond) failures++;
        System.out.println((cond ? "ok   " : "FAIL ") + "regex: " + what);
    }

    // ── Building regexes ───────────────────────────────────

    static Rexp ch(char c)              { return new RChar(c); }
    static Rexp seq(Rexp a, Rexp b)     { return new RSeq(a, b); }
    static Rexp alt(Rexp a, Rexp b)     { return new RAlt(a, b); }

    static Rexp str(String s) {
        if (s.length() == 0) return new REmpty();
        Rexp r = ch(s.charAt(s.length() - 1));
        for (int i = s.length() - 2; i >= 0; i--) r = seq(ch(s.charAt(i)), r);
        return r;
    }

    /* Flags 0: these pin the transformation itself, where `.` carries its
     * default meaning. The flag surface is pinned in PatternSmoke, through the
     * java.util.regex API that supplies them. */
    static PegResult match(Rexp e, String subject) {
        return new PegMachine(RegexToPeg.anchored(e, 0)).run(subject);
    }

    static PegResult find(Rexp e, String subject) {
        return new PegMachine(RegexToPeg.search(e, 0)).run(subject);
    }

    // ── The paper's opening example (printed p. 3) ─────────
    //
    // As regular expressions a|ab and ab|a denote the same language. As regexes
    // — and as PEGs — they do not behave the same, because alternation is tried
    // in order. This non-commutativity is the whole reason a regex is a PEG and
    // not a regular expression.
    static void orderedAlternation() {
        PegResult r1 = match(alt(str("a"), str("ab")), "ab");
        ok(r1.matched && r1.end == 1, "a|ab consumes just \"a\" of \"ab\"");

        PegResult r2 = match(alt(str("ab"), str("a")), "ab");
        ok(r2.matched && r2.end == 2, "ab|a consumes \"ab\" - order decides");
    }

    // The transformation's star is NOT the PEG star.
    //
    // Pi(e*, G_k) builds a rule A -> p_1 | p_k with the continuation INSIDE the
    // choice (Fig. 3, printed p. 8), so a repetition that consumed too much can
    // still give characters back. A bare PEG star cannot: it is possessive.
    // This is the difference the paper's b*b$ discussion turns on (p. 8), and
    // it is why Pi cannot be implemented as "star, then the rest".
    static void starGivesBackWhatPegStarWillNot() {
        PegResult r = match(seq(new RStar(ch('b')), ch('b')), "bb");
        ok(r.matched && r.end == 2, "Pi(b* b) matches \"bb\" - the star gives one back");

        Grammar naive = new Grammar();
        naive.start(Peg.seq(Peg.star(Peg.lit("b")), Peg.lit("b")));
        naive.finish();
        ok(!new PegMachine(naive).run("bb").matched,
           "the naive PEG star followed by b does NOT - it is possessive");
    }

    // Section 3.1 (printed p. 11): (a|epsilon)* b is not well-formed — the
    // repetition can spin without consuming, and Pi would yield a left-
    // recursive PEG with no proof tree for any input. f_out rewrites it to
    // something complete that matches the same language, so a caller never
    // meets the distinction.
    static void wellFormednessRewrite() {
        Rexp e = seq(new RStar(alt(ch('a'), new REmpty())), ch('b'));
        PegResult r1 = match(e, "aab");
        ok(r1.matched && r1.end == 3, "(a|eps)* b matches \"aab\" after the rewrite");
        PegResult r2 = match(e, "b");
        ok(r2.matched && r2.end == 1, "(a|eps)* b matches \"b\"");

        // A repetition whose body is exactly epsilon collapses to epsilon.
        PegResult r3 = match(seq(new RStar(new REmpty()), ch('b')), "b");
        ok(r3.matched, "(eps)* b terminates and matches");

        // And the nested case from the paper's worked example, p. 12.
        Rexp inner = new RStar(alt(ch('d'), new REmpty()));
        Rexp nested = new RStar(seq(ch('b'), seq(ch('c'), seq(new RStar(ch('a')), inner))));
        ok(match(nested, "bcaad").matched, "the p.12 worked example matches");
    }

    // Section 6 (printed p. 15): the four extensions no regular expression can
    // express. Each is pinned against the plain form it differs from — a test
    // that passes for both proves nothing.
    static void extensions() {
        // Atomic: ?>(a|ab) commits to "a" and never reconsiders, so the c fails.
        Rexp plain = seq(alt(str("a"), str("ab")), ch('c'));
        ok(match(plain, "abc").matched, "(a|ab)c backtracks into ab and matches");
        Rexp atomic = seq(new RAtomic(alt(str("a"), str("ab"))), ch('c'));
        ok(!match(atomic, "abc").matched, "?>(a|ab)c cannot backtrack, so it fails");

        // Possessive: a*+ keeps everything, so a following `a` has nothing left.
        ok(match(seq(new RStar(ch('a')), ch('a')), "aa").matched,
           "a* a matches - Pi's star gives back");
        ok(!match(seq(new RPossStar(ch('a')), ch('a')), "aa").matched,
           "a*+ a does not - possessive keeps everything");

        // Lazy: takes as few as it can. Alone, it matches nothing at all.
        PegResult lazy = match(new RLazyStar(ch('a')), "aa");
        ok(lazy.matched && lazy.end == 0, "a*? alone consumes nothing");
        PegResult greedy = match(new RStar(ch('a')), "aa");
        ok(greedy.matched && greedy.end == 2, "a* alone consumes everything");
        ok(match(seq(new RLazyStar(ch('a')), ch('b')), "aab").matched,
           "a*? b still reaches the b");

        // Lookahead, both polarities, consuming nothing either way.
        ok(match(seq(new RNegLookahead(ch('a')), ch('b')), "b").matched,
           "?!a b matches \"b\"");
        ok(!match(seq(new RNegLookahead(ch('a')), ch('a')), "a").matched,
           "?!a a fails");
        PegResult look = match(seq(new RLookahead(ch('a')), ch('a')), "a");
        ok(look.matched && look.end == 1, "?=a a matches and the lookahead consumed nothing");
        ok(!match(seq(new RLookahead(ch('b')), ch('a')), "a").matched, "?=b a fails");
    }

    static void anchorsAndGroups() {
        // `$` needs no predicate: in a PEG, end of input is !any.
        ok(match(seq(ch('a'), new RAnchorEnd()), "a").matched,
           "a$ matches at end of input");
        ok(!match(seq(ch('a'), new RAnchorEnd()), "ab").matched,
           "a$ fails when input remains");
        ok(match(seq(new RAnchorStart(), ch('a')), "a").matched,
           "^a matches at position 0");

        // A group captures its own extent and not the continuation — the thing
        // that forced captures to be two marks rather than a wrapper.
        PegResult r = match(seq(new RGroup(0, str("ab")), str("cd")), "abcd");
        ok(r.matched, "(ab)cd matches");
        ok(r.captures[0] != null && r.captures[0].start == 0 && r.captures[0].len == 2,
           "the group captured \"ab\" only, not the whole match");

        PegResult r2 = match(new RPlus(new RGroup(0, alt(ch('a'), ch('b')))), "ab");
        ok(r2.matched && r2.captures[0] != null && r2.captures[0].start == 1,
           "a repeated group reports its LAST iteration");

        // Optional, both taken and not.
        ok(match(seq(new ROpt(ch('a')), ch('b')), "ab").matched, "a?b matches \"ab\"");
        ok(match(seq(new ROpt(ch('a')), ch('b')), "b").matched, "a?b matches \"b\"");
    }

    // Section 4.1 (printed p. 13). A search must find the pattern anywhere, and
    // the FIRST-set skip must not change WHAT is found — only how fast.
    static void search() {
        Rexp abc = str("abc");
        PegResult r = find(abc, "xxxabc");
        ok(r.matched && r.end == 6, "search finds \"abc\" at offset 3");
        ok(find(abc, "abc").matched, "search finds a match at offset 0");
        ok(!find(abc, "xxxxxx").matched, "search reports no match when there is none");

        // A pattern whose FIRST set is unrestricted gets no skip; it must still
        // be correct.
        Rexp anyThenB = seq(new RAny(), ch('b'));
        PegResult r2 = find(anyThenB, "zzab");
        ok(r2.matched && r2.end == 4, "search works when FIRST is unrestricted");

        // Section 4.3's shape: a repetition of single characters followed by
        // something else. The retry skips a possessive run rather than one
        // character, which must not change the answer.
        Rexp digitsThenBang = seq(new RStar(ch('a')), ch('!'));
        ok(find(digitsThenBang, "zzaaa!").matched, "search over a* ! finds the match");
        ok(!find(digitsThenBang, "zzaaa").matched, "and reports none when absent");

        // A long subject with no match: the skip is what keeps this from being
        // quadratic, and correctness must survive it.
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < 5000; i++) sb.append("z");
        ok(!find(abc, sb.toString()).matched, "5000 non-matching characters, no match");
        sb.append("abc");
        ok(find(abc, sb.toString()).matched, "and the match at the very end is found");
    }
}
