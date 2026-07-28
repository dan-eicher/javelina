// Lib14e — JLS chapter 14, batch five: §14.8.1, §14.8.2, §14.10, §14.10.1. From pp.273-278.
//
//   §14.8.1 if-then. "If the value is true, then the contained Statement is executed ... If the
//           value is false, NO FURTHER ACTION IS TAKEN and the if-then statement completes
//           normally." And the condition itself: "If evaluation of the Expression completes
//           abruptly for some reason, the if-then statement completes abruptly for the same
//           reason."
//   §14.8.2 if-then-else: "executing one or the other BUT NOT BOTH".
//   §14.10  while. "If the value of the Expression is false THE FIRST TIME IT IS EVALUATED,
//           then the Statement is not executed" -- the test precedes the body.
//   §14.10.1 abrupt completion of the body, four cases:
//             break, no label      -> "no further action is taken and the while statement
//                                     completes normally"
//             continue, no label   -> "the entire while statement is executed again"
//             continue with label L, the while HAS L        -> executed again
//             continue with label L, the while HAS NOT L    -> "the while statement completes
//                                     abruptly because of a continue with label L"
//             any other reason     -> abruptly, for the same reason
public class Lib14e {

    private Lib14e() {}

    public static void install(Registry r) {
        r.register(new Sn14IfThenFalse());
        r.register(new Sn14IfThenElseOneBranch());
        r.register(new Sn14IfConditionAbrupt());
        r.register(new Sn14WhileTestsFirst());
        r.register(new Sn14WhileBreakAndContinue());
        r.register(new Sn14WhileContinueOuterLabel());
    }
}

/** §14.8.1: false takes "no further action" and the statement still completes normally, so the
 *  code after it runs. Both branches of the rule are in one line -- a taken if and an untaken
 *  one -- because an implementation that fell through on false would print the same total as
 *  one that skipped the following statement on true. */
class Sn14IfThenFalse implements Snippet {
    public String   id()        { return "t14.if.then.taken.and.not"; }
    public String[] sections()  { return Strs.of("14.8.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; boolean no = false, yes = true;"
             + " if (no) got += 100;"
             + " got += 1;"                        // the if-then completed normally
             + " if (yes) got += 2;"
             + " got += 4;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7 + h[0].asInt()); }
}

/** §14.8.2: "executing one or the other BUT NOT BOTH". Each branch adds a distinct amount, so
 *  running both, or neither, is a different number rather than a coincidence. */
class Sn14IfThenElseOneBranch implements Snippet {
    public String   id()        { return "t14.if.then.else.exactly.one"; }
    public String[] sections()  { return Strs.of("14.8.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; boolean yes = true, no = false;"
             + " if (yes) got += 1; else got += 100;"
             + " if (no) got += 200; else got += 2;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3 + h[0].asInt()); }
}

/** §14.8.1: "If evaluation of the Expression completes abruptly for some reason, the if-then
 *  statement completes abruptly for the same reason." The condition throws, so NEITHER branch
 *  runs and the statement after the if never executes either -- the exception propagates out
 *  of the if, not around it. */
class Sn14IfConditionAbrupt implements Snippet, Declaring {
    public String   id()        { return "t14.if.condition.abrupt"; }
    public String[] sections()  { return Strs.of("14.8.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Cond {\n"
                     + "    static boolean boom() { throw new RuntimeException(\"c\"); }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { if (T14Cond.boom()) got += 1; else got += 2; got += 100; }"
             + " catch (RuntimeException e) { got += 8; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(8); }
}

/** §14.10: "If the value of the Expression is false the first time it is evaluated, then the
 *  Statement is NOT EXECUTED." The while tests before the body, so a false condition runs it
 *  zero times -- which is the difference from `do` (§14.11) and the reason both exist.
 *
 *  The condition is a variable rather than the literal `false`: §14.19 makes `while (false) S;`
 *  a compile-time error for unreachability, which reject/…unreachable_while_false pins. */
class Sn14WhileTestsFirst implements Snippet {
    public String   id()        { return "t14.while.tests.first"; }
    public String[] sections()  { return Strs.of("14.10"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; boolean go = false;"
             + " while (go) { got += 100; }"           // zero iterations
             + " int n = 0;"
             + " while (n < 3) { n++; got += 1; }"     // three
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3 + h[0].asInt()); }
}

/** §14.10.1's first two cases: an unlabeled break makes the while complete NORMALLY (so the
 *  statement after it runs), and an unlabeled continue re-executes "the entire while
 *  statement" -- meaning the condition is re-evaluated, not just the body restarted. The
 *  counter distinguishes those: a continue that skipped the test would loop forever. */
class Sn14WhileBreakAndContinue implements Snippet {
    public String   id()        { return "t14.while.break.and.continue"; }
    public String[] sections()  { return Strs.of("14.10.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; int n = 0;"
             + " while (n < 10) { n++; if (n == 3) break; got += 1; }"
             + " got += 10;"                            // reached: break completes normally
             + " int m = 0;"
             + " while (m < 4) { m++; if (m == 2) continue; got += 100; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // first loop: n=1,2 add 1 each (n=3 breaks) -> 2; +10; second: m=1,3,4 add 100 -> 300
    public Val expect(Val[] h) { return Val.ofInt(312 + h[0].asInt()); }
}

/** §14.10.1's subtle case: a `continue L` whose label is NOT this while's — "the while
 *  statement completes abruptly because of a continue with label L", which propagates outward
 *  until a loop labeled L catches it and is executed again.
 *
 *  So the inner loop is exited, the outer one continues, and the statement after the inner
 *  loop never runs. That last part is what separates this from `break inner`. */
class Sn14WhileContinueOuterLabel implements Snippet {
    public String   id()        { return "t14.while.continue.outer.label"; }
    public String[] sections()  { return Strs.of("14.10.1", "14.14"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; int i = 0;"
             + " outer: while (i < 3) {"
             + "   i++;"
             + "   int j = 0;"
             + "   while (j < 3) { j++; if (j == 2) continue outer; got += 1; }"
             + "   got += 100;"                         // never reached
             + " }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // each outer pass adds 1 (j==1) then continues at j==2, skipping the += 100. Three passes.
    public Val expect(Val[] h) { return Val.ofInt(3 + h[0].asInt()); }
}
