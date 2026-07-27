// Check — the assertion floor for the JLS conformance suite.
//
// Every assertion carries the JLS section it comes from, so a failure names the rule that
// broke rather than the line that noticed. Output is deterministic and count-based: the
// suite prints one RESULT line per chapter and exits nonzero if anything failed, which is
// the "expected stdout + expected exit code" oracle E7.4 asks for.
//
// Java 1.0: no generics, no autoboxing, no varargs, and no lambdas — so an "expect a throw"
// helper cannot take a closure. Those assertions are written inline as try/catch around the
// offending expression, closed with thrown()/notThrown().
public class Check {
    public static int checks = 0;
    public static int fails  = 0;

    private Check() {}

    private static void bad(String section, String what, String detail) {
        fails++;
        System.out.println("FAIL JLS " + section + ": " + what + " -- " + detail);
    }

    /** integral / char / boolean-free numeric equality */
    public static void eq(String section, String what, long actual, long expected) {
        checks++;
        if (actual != expected) bad(section, what, "expected " + expected + ", got " + actual);
    }

    public static void eq(String section, String what, boolean actual, boolean expected) {
        checks++;
        if (actual != expected) bad(section, what, "expected " + expected + ", got " + actual);
    }

    /** Exact bit equality, so NaN and -0.0 are compared as VALUES rather than by ==
     *  (§4.2.3: NaN != NaN, and 0.0 == -0.0, both of which would defeat a naive check). */
    public static void eqBits(String section, String what, double actual, double expected) {
        checks++;
        long a = Double.doubleToLongBits(actual), e = Double.doubleToLongBits(expected);
        if (a != e) bad(section, what, "expected " + expected + ", got " + actual);
    }

    public static void eqBits(String section, String what, float actual, float expected) {
        checks++;
        int a = Float.floatToIntBits(actual), e = Float.floatToIntBits(expected);
        if (a != e) bad(section, what, "expected " + expected + ", got " + actual);
    }

    /** value equality via equals(), with null handled */
    public static void eq(String section, String what, Object actual, Object expected) {
        checks++;
        boolean ok = (expected == null) ? (actual == null) : expected.equals(actual);
        if (!ok) bad(section, what, "expected " + expected + ", got " + actual);
    }

    /** reference IDENTITY — distinct from eq(), and the difference is the point in
     *  §3.10.5 (interning), §10.7 (clone is a distinct array) and §10.8 (Class identity) */
    public static void same(String section, String what, Object a, Object b) {
        checks++;
        if (a != b) bad(section, what, "expected the same object");
    }

    public static void notSame(String section, String what, Object a, Object b) {
        checks++;
        if (a == b) bad(section, what, "expected distinct objects");
    }

    public static void isTrue(String section, String what, boolean b) {
        checks++;
        if (!b) bad(section, what, "expected true");
    }

    /** the catch arm of an inline throw assertion reached: the throw happened */
    public static void thrown(String section, String what) {
        checks++;
    }

    /** the line AFTER the offending expression reached: the throw did not happen */
    public static void notThrown(String section, String what) {
        checks++;
        bad(section, what, "expected an exception, none thrown");
    }

    /** a case that cannot be reached at all if the rule holds */
    public static void unreachable(String section, String what) {
        checks++;
        bad(section, what, "reached code that the rule makes unreachable");
    }

    /** An exception escaped a section's own body.
     *
     *  Without this the throw propagates out of main and the vm reports an uncaught trap with
     *  a frame list — which says the program died but not which RULE was being checked, and
     *  takes every later section down with it. Reporting it as a failure against the section
     *  keeps the remaining sections running and names the one that broke, which is the whole
     *  premise of a suite whose output is a count. */
    public static void crashed(String section, Throwable t) {
        checks++;
        bad(section, "the section threw instead of asserting", t.getClass().getName());
    }
}
