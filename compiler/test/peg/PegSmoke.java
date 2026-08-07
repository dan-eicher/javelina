// Behavioural gate for the pegc Java backend: a parser generated from
// test/peg/Fixture.peg, compiled by javelinac, run by javelina.
//
// The pegc-side pins check the emitted text; these check that the text means
// what it should. Ordered choice has to backtrack, a not-predicate has to
// consume nothing, and both have to hold identically in the interpreter and
// under the JIT — the script runs this program in both tiers and diffs.
//
// Prints one line per case and a final PEG-RESULT line the script gates on.
public class PegSmoke {
    static int failures = 0;

    public static void main(String[] args) {
        check("[]",           true,  "empty body");
        check("[a]",          true,  "ordered choice restores: Pair alt 1 consumed the ident then failed");
        check("[a:b]",        true,  "Pair alt 1 matches whole");
        check("[a:b c]",      true,  "both Pair alternatives in one body");
        check("[\"hi\"]",     true,  "Item alt 2 after alt 1 fails at the quote");
        check("[([a])]",      true,  "nested Start through the group alternative");
        check("[\"a(b\"]",    true,  "not-predicate: the quote ends the string, the paren is content");
        check("[a b c d e]",  true,  "repetition over many items");
        check("[(]",          false, "unclosed group fails");
        check("[9]",          false, "a digit cannot start an ident");
        check("[a:]",         false, "Pair alt 1 half-matched, alt 2 leaves the colon unconsumed");
        check("x",            false, "wrong opener");
        check("",             false, "empty input");

        // The runtime machine, including the differential gate against this
        // very parser — same grammar, two paths, one answer.
        failures += MachineSmoke.run();

        System.out.println("PEG-RESULT failures=" + failures);
    }

    static void check(String src, boolean expect, String what) {
        FixtureParser p = new FixtureParser(src.toCharArray());
        // A parse that succeeds without reaching the end of input has not
        // matched the subject, only a prefix of it.
        boolean got = p.parse() && p.cursor().save() == src.length();
        if (got != expect) failures++;
        System.out.println((got == expect ? "ok   " : "FAIL ") + what
            + "  [" + src + "] got=" + got + " want=" + expect);
    }
}
