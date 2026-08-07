// Pins for the runtime PEG machine (javelina.peg.PegMachine).
//
// The last section is the one that matters most: the same grammar, built once
// as a pegc-generated parser and once as a Grammar, must agree on every input.
// Because pegc is a hand-written generating extension rather than one derived
// from the machine by self-application (Glück, PEPM'09 section 1), nothing
// makes the two paths agree by construction — this test stands in for the
// derivation we did not do.
import javelina.peg.Grammar;
import javelina.peg.Peg;
import javelina.peg.PegAction;
import javelina.peg.PegException;
import javelina.peg.PegMachine;
import javelina.peg.PegResult;
import javelina.peg.Pexp;
import javelina.peg.Span;

public class MachineSmoke {

    static int failures = 0;

    public static int run() {
        orderedChoice();
        possessiveRepetition();
        captures();
        actions();
        failureReporting();
        deepRecursionKeepsTheStackFlat();
        badGrammarsAreRejected();
        agreesWithGeneratedParser();
        return failures;
    }

    static void ok(boolean cond, String what) {
        if (!cond) failures++;
        System.out.println((cond ? "ok   " : "FAIL ") + "machine: " + what);
    }

    static Grammar oneRule(Pexp start) {
        Grammar g = new Grammar();
        g.start(start);
        g.finish();
        return g;
    }

    // ── Ordered choice ─────────────────────────────────────
    //
    // The paper's own opening example (Medeiros et al., printed p. 3): as
    // regular expressions a|aa and aa|a denote the same language, but a regex
    // library — and a PEG — try alternatives in order, so the first matches
    // only "a" against the subject "aa". This non-commutativity IS the thesis
    // that a regex is a PEG.
    static void orderedChoice() {
        PegMachine m = new PegMachine(oneRule(Peg.choice(Peg.lit("a"), Peg.lit("aa"))));
        PegResult r = m.run("aa");
        ok(r.matched && r.end == 1, "a / aa matches just \"a\" against \"aa\"");

        PegMachine m2 = new PegMachine(oneRule(Peg.choice(Peg.lit("aa"), Peg.lit("a"))));
        PegResult r2 = m2.run("aa");
        // Printed strings stay ASCII: JLS 1.0 section 22.22.9 says PrintStream
        // writes "the low-order bytes of the characters", so a non-ASCII
        // character reaches stdout truncated. That is 1.0 behaving correctly.
        ok(r2.matched && r2.end == 2, "aa / a matches \"aa\": order decides");
    }

    // PEG repetition is possessive: it never gives back what it consumed, so a
    // star followed by the same element can never match. Correct, not a bug.
    static void possessiveRepetition() {
        PegMachine m = new PegMachine(oneRule(
            Peg.seq(Peg.star(Peg.lit("a")), Peg.lit("a"))));
        ok(!m.run("aaa").matched, "a* a never matches: repetition is possessive");

        // A star whose body can match empty must still terminate.
        PegMachine m2 = new PegMachine(oneRule(Peg.star(Peg.opt(Peg.lit("a")))));
        ok(m2.run("b").matched, "a star over a nullable body terminates");
    }

    static void captures() {
        PegMachine m = new PegMachine(oneRule(Peg.seq(
            Peg.capture(0, Peg.plus(Peg.range('a', 'z'))),
            Peg.capture(1, Peg.plus(Peg.range('0', '9'))))));
        PegResult r = m.run("abc123");
        ok(r.matched, "capture grammar matches");
        ok(r.captures[0] != null && r.captures[0].start == 0 && r.captures[0].len == 3,
           "slot 0 spans the letters");
        ok(r.captures[1] != null && r.captures[1].start == 3 && r.captures[1].len == 3,
           "slot 1 spans the digits");

        // A capture made inside an alternative that then fails must not survive.
        PegMachine m2 = new PegMachine(oneRule(Peg.choice(
            Peg.seq(Peg.capture(0, Peg.lit("a")), Peg.lit("!")),
            Peg.lit("ab"))));
        PegResult r2 = m2.run("ab");
        ok(r2.matched && r2.captures[0] == null,
           "a capture from a failed alternative is rewound");
    }

    static void actions() {
        PegAction leaf = new LeafAction();
        PegAction pair = new PairAction();
        PegMachine m = new PegMachine(oneRule(
            Peg.action(pair, Peg.seq(
                Peg.action(leaf, Peg.lit("a")),
                Peg.action(leaf, Peg.lit("b"))))));
        PegResult r = m.run("ab");
        ok(r.matched, "action grammar matches");
        ok("(a,b)".equals(r.value),
           "an action's value reaches the result and nested values arrive in parts");

        // No actions at all — the whole of the regex case — is not a special
        // case that needs handling, it just leaves value null.
        PegMachine m2 = new PegMachine(oneRule(Peg.lit("a")));
        ok(m2.run("a").value == null, "a grammar with no actions yields a null value");
    }

    // Regex never reads failPos, which is exactly why it is pinned: a library
    // that only builds what its first client needs never grows diagnostics.
    static void failureReporting() {
        PegMachine m = new PegMachine(oneRule(Peg.choice(
            Peg.seq(Peg.lit("ab"), Peg.lit("c")),
            Peg.lit("x"))));
        PegResult r = m.run("abd");
        ok(!r.matched, "abd does not match (ab c / x)");
        ok(r.failPos == 2,
           "failPos is the FURTHEST position reached (2), not where the choice gave up (0)");
        ok(r.expected.length > 0, "the failure says what it wanted");
    }

    // The reason for the CEK design. java.util.regex has the same node tree but
    // recurses on the Java stack, which is why deep nesting throws
    // StackOverflowError there. Here the continuation is heap data, so depth
    // costs memory and not stack.
    static void deepRecursionKeepsTheStackFlat() {
        int depth = 20000;
        Grammar g = new Grammar();
        // S = "(" S ")" / ""
        g.define("S", Peg.choice(
            Peg.seq(Peg.lit("("), Peg.rule("S"), Peg.lit(")")),
            Peg.lit("")));
        g.start(Peg.rule("S"));
        g.finish();

        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < depth; i++) sb.append("(");
        for (int i = 0; i < depth; i++) sb.append(")");
        String subject = sb.toString();

        PegResult r = new PegMachine(g).run(subject);
        ok(r.matched && r.end == subject.length(),
           depth + " levels of nesting parse without exhausting the stack");
    }

    // A grammar that cannot run is a programming error found once at
    // construction, not an input that failed to match — so finish() rejects it
    // rather than the machine looping.
    static void badGrammarsAreRejected() {
        Grammar g = new Grammar();
        g.define("A", Peg.choice(Peg.seq(Peg.rule("A"), Peg.lit("x")), Peg.lit("y")));
        g.start(Peg.rule("A"));
        boolean caught = false;
        try { g.finish(); } catch (PegException e) { caught = true; }
        ok(caught, "left recursion is rejected at finish, not discovered by looping");

        Grammar g2 = new Grammar();
        g2.start(Peg.rule("nope"));
        boolean caught2 = false;
        try { g2.finish(); } catch (PegException e) { caught2 = true; }
        ok(caught2, "a reference to an undefined rule is rejected");

        // Recursion that consumes first is fine — only LEFT recursion is a defect.
        Grammar g3 = new Grammar();
        g3.define("A", Peg.choice(Peg.seq(Peg.lit("x"), Peg.rule("A")), Peg.lit("y")));
        g3.start(Peg.rule("A"));
        boolean threw = false;
        try { g3.finish(); } catch (PegException e) { threw = true; }
        ok(!threw, "right recursion is accepted");
    }

    // ── The differential gate ──────────────────────────────

    // test/peg/Fixture.peg, rebuilt with the construction API. The generated
    // parser calls skip() before every terminal, so the whitespace rule is
    // written explicitly here in the same places — matching that discipline is
    // part of what the comparison checks.
    static Pexp tok(String s) {
        return Peg.seq(Peg.rule("_"), Peg.lit(s));
    }

    static Pexp call(String name) {
        return Peg.seq(Peg.rule("_"), Peg.rule(name));
    }

    static Grammar fixtureGrammar() {
        Grammar g = new Grammar();
        Pexp letter = Peg.range('a', 'z');
        Pexp digit = Peg.range('0', '9');

        g.define("_", Peg.star(Peg.chars(" \t\n")));
        g.define("ident", Peg.seq(letter, Peg.star(Peg.choice(letter, digit))));

        Pexp quote = Peg.lit("\"");
        g.define("string_lit", Peg.seq(
            quote,
            Peg.star(Peg.seq(Peg.not(quote), Peg.any())),
            quote));

        // JLS 1.0 section 15.8 has no ArrayInitializer alternative for a
        // creation expression, so `new Pexp[]{...}` is 1.1 syntax. An
        // initializer in a declarator is 1.0, hence the locals.
        Pexp[] pairAlt1 = { call("ident"), tok(":"), call("ident") };
        g.define("Pair", Peg.choice(Peg.seq(pairAlt1), call("ident")));

        Pexp[] group = { tok("("), Peg.rule("Start"), tok(")") };
        Pexp[] itemAlts = { Peg.rule("Pair"), call("string_lit"), Peg.seq(group) };
        g.define("Item", Peg.choice(itemAlts));

        Pexp[] startSeq = { tok("["), Peg.star(Peg.rule("Item")), tok("]") };
        g.define("Start", Peg.seq(startSeq));

        g.start(Peg.rule("Start"));
        g.finish();
        return g;
    }

    static void agreesWithGeneratedParser() {
        String[] inputs = {
            "[]", "[a]", "[a:b]", "[a:b c]", "[\"hi\"]", "[([a])]", "[\"a(b\"]",
            "[a b c d e]", "[(]", "[9]", "[a:]", "x", "", "[ a : b ]", "[[]]",
            "[a:b:c]", "[\"\"]", "[((([x])))]", "[a1 b2]", "[:]"
        };
        PegMachine m = new PegMachine(fixtureGrammar());
        int disagreements = 0;
        for (int i = 0; i < inputs.length; i++) {
            String s = inputs[i];
            FixtureParser p = new FixtureParser(s.toCharArray());
            boolean generated = p.parse() && p.cursor().save() == s.length();
            PegResult r = m.run(s);
            boolean machine = r.matched && r.end == s.length();
            if (generated != machine) {
                disagreements++;
                System.out.println("     disagreement on [" + s + "]: generated="
                    + generated + " machine=" + machine);
            }
        }
        ok(disagreements == 0,
           "machine and pegc-generated parser agree on all " + inputs.length
           + " inputs for the same grammar");
    }
}

// One top-level class per action: Java 1.0 has no anonymous classes. A port to
// Java 8 or later turns each of these into a lambda with no change to the
// library, which is why PegAction is a single-method interface.
class LeafAction implements PegAction {
    public Object act(String input, int start, int end, Object[] parts) {
        return input.substring(start, end);
    }
}

class PairAction implements PegAction {
    public Object act(String input, int start, int end, Object[] parts) {
        StringBuffer sb = new StringBuffer();
        sb.append("(");
        for (int i = 0; i < parts.length; i++) {
            if (i > 0) sb.append(",");
            sb.append(parts[i]);
        }
        sb.append(")");
        return sb.toString();
    }
}
