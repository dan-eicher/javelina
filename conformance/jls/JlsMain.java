// JlsMain — the JLS conformance suite's entry point.
//
// One chapter class per JLS chapter; each runs its per-section methods, each of which
// asserts through Check. The oracle is E7.4's: expected stdout (a deterministic RESULT
// line) and expected exit code (0 iff nothing failed). Failures print BEFORE the RESULT
// line and name the section that broke.
//
// Deterministic by construction: no clock, no random, no hash iteration order, no
// identity-dependent output. The same program must produce byte-identical stdout at both
// optimisation levels and on both execution tiers, so the corpus can star-diff it.
public class JlsMain {

    public static void main(String[] args) {
        Ch3.run();
        Ch10.run();
        Ch10b.run();

        System.out.println("RESULT jls checks=" + Check.checks + " fails=" + Check.fails);
        if (Check.fails != 0) System.exit(1);
    }
}
