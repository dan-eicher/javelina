// Ch14 — JLS chapter 14, Blocks and Statements.
//
// Control flow, which on this target means the DDCG's destination-driven lowering: the shapes
// asserted here are the ones the backend has to frame correctly (loop headers, join labels,
// break/continue targets, finally on every exit path). §14.11.2's do-loop is the spec's own
// worked toHexString.
//
// The compile-time halves — §14.8's non-boolean condition, §14.19's unreachable statement —
// live in conformance/reject/, where the artifact under test is javelinac's diagnostic.

public class Ch14 {

    static String log = "";
    static int    ticks = 0;
    static int tick(int v) { ticks = ticks + 1; return v; }
    static int field = 100;

    // JLS 14.1
    static void s14_1() {
        // "if the evaluation of the expression or the execution of the substatement completes
        // abruptly, then the statement completes abruptly for the same reason." A throw inside
        // an expression statement aborts the statement, so what follows does not run.
        log = "";
        try { log = log + "a"; if (blowUp()) log = log + "never"; log = log + "unreached"; }
        catch (ArithmeticException e) { log = log + "b"; }
        Check.eq("14.1", "a substatement's abrupt completion propagates", log, "ab");
    }

    static boolean blowUp() { int x = 1 / 0; return true; }

    // JLS 14.2
    static void s14_2() {
        // "A block is executed by executing each of the local variable declaration statements
        // and other statements in order from first to last."
        log = "";
        { log = log + "1"; log = log + "2"; log = log + "3"; }
        Check.eq("14.2", "a block runs its statements first to last", log, "123");
        // An empty block completes normally and does nothing.
        log = "x";
        { }
        Check.eq("14.2", "an empty block does nothing", log, "x");
        // Blocks nest, and an inner block's statements run in place.
        log = "";
        { log = log + "a"; { log = log + "b"; } log = log + "c"; }
        Check.eq("14.2", "a nested block runs in place", log, "abc");
    }

    // JLS 14.3
    static void s14_3() {
        // "A local variable declaration statement may be intermixed freely with other kinds of
        // statements in the block" — a declaration after executable statements is legal.
        int a = 1;
        log = "" + a;
        int b = 2;                 // declared AFTER a statement
        Check.eq("14.3", "a declaration may follow a statement", (long) (a + b), 3L);
    }

    // JLS 14.3.1
    static void s14_3_1() {
        // "The array type of a variable is denoted by... brackets following the declarator" —
        // `int a[]` and `int[] a` declare the same type, and both forms may be mixed.
        int a[] = { 1, 2 };
        int[] b = { 3, 4 };
        int c[][] = { { 5 }, { 6, 7 } };
        Check.eq("14.3.1", "trailing brackets declare an array", (long) a[1], 2L);
        Check.eq("14.3.1", "…the same type as the leading form", (long) b[0], 3L);
        Check.eq("14.3.1", "…and nest for extra dimensions", (long) c[1][1], 7L);
        // Mixed in one declaration: `int d, e[]` gives an int and an int[].
        int d = 8, e[] = { 9 };
        Check.eq("14.3.1", "brackets bind to the DECLARATOR, not the declaration",
                 (long) (d + e[0]), 17L);
    }

    // JLS 14.3.2
    static void s14_3_2() {
        // "The scope of a local variable declared in a block is the rest of the block in which
        // the declaration appears, starting with its own initializer."
        int outer = 1;
        {
            int inner = 2;
            Check.eq("14.3.2", "a local is in scope for the rest of its block",
                     (long) (outer + inner), 3L);
        }
        // `inner` is out of scope here — a reference would be a compile-time error, so the
        // observable half is that the NAME can be reused for a fresh variable.
        {
            int inner = 5;
            Check.eq("14.3.2", "…and the name is free again once the block ends",
                     (long) inner, 5L);
        }
    }

    // JLS 14.3.3
    static void s14_3_3() {
        // "the declaration of a local variable... hides the declaration of any field with the
        // same name throughout the scope of the local variable."
        Check.eq("14.3.3", "the field is visible before the local is declared",
                 (long) field, 100L);
        int field = 7;                    // hides Ch14.field from here on
        Check.eq("14.3.3", "a local hides a same-named field", (long) field, 7L);
        Check.eq("14.3.3", "…and the field is still reachable as Ch14.field",
                 (long) Ch14.field, 100L);
    }

    // JLS 14.3.4
    static void s14_3_4() {
        // "the declarators are processed in order from left to right; ...the initializer is
        // evaluated and the assignment performed."
        ticks = 0;
        int p = tick(1), q = tick(2), r = tick(3);
        Check.eq("14.3.4", "declarators initialize left to right", (long) ticks, 3L);
        Check.eq("14.3.4", "…each with its own value", (long) (p * 100 + q * 10 + r), 123L);
        // A later declarator may refer to an earlier one in the SAME declaration.
        int m = 4, n = m + 1;
        Check.eq("14.3.4", "a declarator may use an earlier one", (long) n, 5L);
    }

    // JLS 14.5
    static void s14_5() {
        // "An empty statement does nothing." It completes normally, so the statement after it
        // still runs — and a stray `;` as a loop body is a legal (empty) body.
        log = "a";
        ;
        log = log + "b";
        Check.eq("14.5", "the empty statement does nothing", log, "ab");
        int n = 0;
        for (int i = 0; i < 3; i++) ;      // empty body
        for (int i = 0; i < 3; i++) n = n + 1;
        Check.eq("14.5", "…and is a legal loop body", (long) n, 3L);
    }

    // JLS 14.6
    static void s14_6() {
        // "A labeled statement completes normally if the... statement completes normally", and
        // a `break Label` transfers control out of the LABELED statement, not just the loop.
        log = "";
        outer:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (j == 1) continue outer;      // next i
                if (i == 2) break outer;         // leave BOTH loops
                log = log + i + j;
            }
        }
        Check.eq("14.6", "labeled break/continue target the named statement", log, "0010");

        // A label on a plain block: `break Label` exits the block.
        log = "";
        done: {
            log = log + "in";
            if (log.length() == 2) break done;
            log = log + "never";
        }
        Check.eq("14.6", "a labeled block may be broken out of", log, "in");
    }

    // JLS 14.7
    static void s14_7() {
        // "the value of the expression is discarded" — the statement is executed for effect.
        ticks = 0;
        tick(9);                          // result discarded
        Check.eq("14.7", "an expression statement runs for effect", (long) ticks, 1L);
        int n = 0;
        n++;                              // the listed forms: ++, --, assignment, call, new
        n--;
        n = 5;
        n += 2;
        Check.eq("14.7", "…and the listed forms are all statements", (long) n, 7L);
    }

    // JLS 14.8.1
    static void s14_8_1() {
        // "If the value is true, then the contained Statement is executed; otherwise... no
        // further action is taken."
        log = "";
        if (true) log = log + "t";
        if (false) log = log + "f";
        Check.eq("14.8.1", "if-then runs the body only when true", log, "t");
    }

    // JLS 14.8.2
    static void s14_8_2() {
        // "If the value is true, then the first contained Statement is executed; otherwise the
        // second... is executed." Exactly one, never both, never neither.
        log = "";
        if (true) log = log + "a"; else log = log + "b";
        if (false) log = log + "c"; else log = log + "d";
        Check.eq("14.8.2", "if-then-else runs exactly one branch", log, "ad");

        // The dangling else binds to the INNERMOST if it could belong to (§14.4's grammar).
        log = "";
        if (true) if (false) log = log + "x"; else log = log + "inner";
        Check.eq("14.8.2", "a dangling else binds to the innermost if", log, "inner");
    }

    // JLS 14.9
    static void s14_9() {
        // "the statements after the matching case label are executed" — and control FALLS
        // THROUGH into the following cases until a break.
        log = "";
        for (int i = 0; i < 5; i++) {
            switch (i) {
                case 0: log = log + "z"; break;
                case 1:
                case 2: log = log + "s"; break;          // shared label group
                case 3: log = log + "f";                  // falls through
                default: log = log + "d"; break;
            }
        }
        Check.eq("14.9", "switch matches, groups labels, and falls through", log, "zssfdd");

        // The selector accepts char/byte/short/int (§14.9's list).
        char c = 'b';
        int hit = 0;
        switch (c) { case 'a': hit = 1; break; case 'b': hit = 2; break; default: hit = 3; }
        Check.eq("14.9", "a char selector works", (long) hit, 2L);
        byte b = 2;
        switch (b) { case 1: hit = 10; break; case 2: hit = 20; break; default: hit = 30; }
        Check.eq("14.9", "…and a byte selector", (long) hit, 20L);

        // "If no case matches and there is no default, no statements are executed."
        log = "";
        switch (99) { case 1: log = "one"; break; }
        Check.eq("14.9", "no match and no default runs nothing", log, "");

        // default need not be last, and is chosen only when nothing else matches.
        hit = 0;
        switch (7) { default: hit = 1; break; case 7: hit = 2; break; }
        Check.eq("14.9", "default is not positional", (long) hit, 2L);
    }

    // JLS 14.10
    static void s14_10() {
        // "the Expression is evaluated... if true, the contained Statement is executed" — the
        // condition is tested BEFORE each iteration, so a false condition runs the body zero
        // times.
        // The condition is a non-final local, so it is NOT a §15.27 constant expression —
        // `while (false)` itself is a compile-time error under §14.19 (the body is
        // unreachable), which conformance/reject/ pins.
        int n = 0;
        boolean never = false;
        while (never) n = 99;
        Check.eq("14.10", "while tests before the body, so zero iterations", (long) n, 0L);
        n = 0;
        int i = 0;
        while (i < 4) { n = n + i; i = i + 1; }
        Check.eq("14.10", "…and re-tests before each iteration", (long) n, 6L);
    }

    // JLS 14.10.1
    static void s14_10_1() {
        // "if... a break statement with no label, then the while statement completes normally";
        // continue "the while statement is executed again" — i.e. re-test.
        log = "";
        int i = 0;
        while (i < 5) {
            i = i + 1;
            if (i == 2) continue;          // skip the append for 2
            if (i == 4) break;             // leave before appending 4
            log = log + i;
        }
        Check.eq("14.10.1", "break exits and continue re-tests", log, "13");
    }

    // JLS 14.11
    static void s14_11() {
        // "the contained Statement is executed FIRST, then the Expression is evaluated" — so
        // the body always runs at least once, even when the condition is false.
        int n = 0;
        do { n = n + 1; } while (false);
        Check.eq("14.11", "do-while runs its body at least once", (long) n, 1L);
        n = 0;
        int i = 0;
        do { n = n + i; i = i + 1; } while (i < 4);
        Check.eq("14.11", "…and tests after each iteration", (long) n, 6L);
    }

    // JLS 14.11.1
    static void s14_11_1() {
        // In do, continue jumps to the TEST (not the top), so the test still decides.
        log = "";
        int i = 0;
        do {
            i = i + 1;
            if (i == 2) continue;          // to the test, not past it
            if (i == 5) break;
            log = log + i;
        } while (i < 6);
        Check.eq("14.11.1", "continue in do jumps to the test", log, "134");
    }

    // JLS 14.11.2
    static void s14_11_2() {
        // The spec's own worked do-loop, and the values it computes.
        Check.eq("14.11.2", "the spec's toHexString do-loop: 0", toHex(0), "0");
        Check.eq("14.11.2", "…15", toHex(15), "f");
        Check.eq("14.11.2", "…16", toHex(16), "10");
        Check.eq("14.11.2", "…255", toHex(255), "ff");
        Check.eq("14.11.2", "…a larger value", toHex(48879), "beef");
    }

    /* The spec's §14.11.2 example: a do statement, because the result must have at least one
     * digit even for zero — which is exactly what a test-first loop would get wrong. */
    static String toHex(int i) {
        String digits = "0123456789abcdef";
        String result = "";
        do {
            result = digits.charAt(i & 15) + result;
            i = i >>> 4;
        } while (i != 0);
        return result;
    }

    // JLS 14.12
    static void s14_12() {
        // "ForInit... then... the condition, the contained Statement, then ForUpdate."
        log = "";
        for (int i = 0; i < 3; i++) log = log + i;
        Check.eq("14.12", "for runs init, then test/body/update", log, "012");
        // Every clause is optional; `for (;;)` with a break is the spec's infinite loop.
        int n = 0;
        for (;;) { n = n + 1; if (n == 3) break; }
        Check.eq("14.12", "for(;;) is an infinite loop until break", (long) n, 3L);
    }

    // JLS 14.12.1
    static void s14_12_1() {
        // "the expressions are evaluated in order from left to right" — and a for-init
        // declaration may declare several variables.
        ticks = 0;
        int total = 0;
        for (int a = tick(1), b = tick(2); a < 2; a++) total = total + a + b;
        Check.eq("14.12.1", "for-init declarators evaluate left to right", (long) ticks, 2L);
        Check.eq("14.12.1", "…and are all in scope in the body", (long) total, 3L);
    }

    // JLS 14.12.2
    static void s14_12_2() {
        // "the Expression is evaluated... if false, the for statement completes normally" —
        // a condition false on entry runs the body zero times and the update never.
        ticks = 0;
        int n = 0;
        for (int i = 10; i < 3; i = tick(i + 1)) n = n + 1;
        Check.eq("14.12.2", "a for whose condition starts false runs zero times", (long) n, 0L);
        Check.eq("14.12.2", "…and never evaluates the update", (long) ticks, 0L);
    }

    // JLS 14.12.3
    static void s14_12_3() {
        // "continue... the ForUpdate part is executed" — unlike break, continue still updates,
        // which is what keeps a `continue` in a for loop from spinning forever.
        log = "";
        for (int i = 0; i < 5; i++) {
            if (i == 1) continue;          // update STILL runs
            if (i == 3) break;             // update does not
            log = log + i;
        }
        Check.eq("14.12.3", "continue runs the update, break does not", log, "02");
    }

    // JLS 14.13
    static void s14_13() {
        // "transfers control out of the innermost enclosing switch, while, do, or for" —
        // innermost, so an inner loop's break leaves the outer one running.
        log = "";
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 5; j++) { if (j == 1) break; log = log + i + j; }
        }
        Check.eq("14.13", "break leaves the INNERMOST loop only", log, "0010");
        // break in a switch leaves the switch, not the enclosing loop.
        log = "";
        for (int i = 0; i < 3; i++) {
            switch (i) { case 1: break; default: log = log + i; }
            log = log + ".";
        }
        // i=0 default appends "0" then "."; i=1 breaks out of the switch, appending only ".";
        // i=2 default appends "2" then ".".
        Check.eq("14.13", "break in a switch leaves the switch", log, "0..2.");
    }

    // JLS 14.14
    static void s14_14() {
        // "ends the current iteration of the innermost enclosing while, do, or for" — and a
        // labeled continue names an OUTER loop.
        log = "";
        for (int i = 0; i < 3; i++) { if (i == 1) continue; log = log + i; }
        Check.eq("14.14", "continue ends the current iteration", log, "02");
        log = "";
        lbl:
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { if (j == 1) continue lbl; log = log + i + j; }
        }
        Check.eq("14.14", "a labeled continue targets the outer loop", log, "001020");
    }

    // JLS 14.15
    static void s14_15() {
        // "the value... becomes the value of the method invocation" — and a return in the
        // middle of a method abandons the rest of it.
        Check.eq("14.15", "return hands its value back", (long) five(), 5L);
        log = "";
        early();
        Check.eq("14.15", "…and abandons what follows", log, "before");
        // A return inside a loop leaves the method, not just the loop.
        Check.eq("14.15", "return from inside a loop leaves the METHOD", (long) firstOver(3), 4L);
    }

    static int five() { return 5; }
    static void early() { log = log + "before"; if (true) return; log = log + "after"; }
    static int firstOver(int n) { for (int i = 0; i < 100; i++) if (i > n) return i; return -1; }

    // JLS 14.16
    static void s14_16() {
        // "control is transferred to the innermost enclosing catch clause" that can accept it.
        log = "";
        try { throw new ArithmeticException("t"); }
        catch (ArithmeticException e) { log = e.getMessage(); }
        Check.eq("14.16", "throw reaches the accepting catch", log, "t");
        // The thrown value is the evaluated expression — the SAME object arrives.
        ArithmeticException thrown = new ArithmeticException("same");
        Object caught = null;
        try { throw thrown; } catch (ArithmeticException e) { caught = e; }
        Check.same("14.16", "the caught object IS the thrown one", caught, thrown);
    }

    // JLS 14.18.1
    static void s14_18_1() {
        // "the first such catch clause is selected" — leftmost wins among those that accept.
        log = "";
        try { throw new ArithmeticException("x"); }
        catch (ArithmeticException e) { log = "specific"; }
        catch (RuntimeException e)    { log = "general"; }
        Check.eq("14.18.1", "the FIRST accepting catch is selected", log, "specific");

        // ...and a catch whose type does not accept it is skipped, even if it comes first.
        log = "";
        try { throw new ArithmeticException("x"); }
        catch (NullPointerException e) { log = "wrong"; }
        catch (RuntimeException e)     { log = "right"; }
        Check.eq("14.18.1", "a non-accepting catch is skipped", log, "right");

        // "If the try block completes normally, then no catch clause is executed."
        log = "";
        try { log = log + "ok"; } catch (RuntimeException e) { log = log + "caught"; }
        Check.eq("14.18.1", "a normal try runs no catch", log, "ok");
    }

    // JLS 14.18.2
    static void s14_18_2() {
        // finally runs on EVERY exit path: normal, caught, and propagating.
        log = "";
        try { log = log + "t"; } finally { log = log + "f"; }
        Check.eq("14.18.2", "finally runs after a normal try", log, "tf");

        log = "";
        try { throw new ArithmeticException(); }
        catch (ArithmeticException e) { log = log + "c"; }
        finally { log = log + "f"; }
        Check.eq("14.18.2", "…after a caught throw", log, "cf");

        log = "";
        try {
            try { throw new ArithmeticException(); } finally { log = log + "f"; }
        } catch (ArithmeticException e) { log = log + "c"; }
        Check.eq("14.18.2", "…and while an exception propagates", log, "fc");

        // finally also runs when the try block RETURNS — the classic case.
        log = "";
        int v = returnsFromTry();
        Check.eq("14.18.2", "…and when the try returns", log, "f");
        Check.eq("14.18.2", "…without changing the returned value", (long) v, 1L);

        // "If the finally block completes abruptly... the try statement completes abruptly for
        // that reason" — a return in finally overrides the try's.
        Check.eq("14.18.2", "an abrupt finally overrides the try's completion",
                 (long) finallyWins(), 2L);
    }

    static int returnsFromTry() { try { return 1; } finally { log = log + "f"; } }
    static int finallyWins()    { try { return 1; } finally { return 2; } }

    public static void run() {
        try { s14_1();    } catch (Throwable t) { Check.crashed("14.1", t); }
        try { s14_2();    } catch (Throwable t) { Check.crashed("14.2", t); }
        try { s14_3();    } catch (Throwable t) { Check.crashed("14.3", t); }
        try { s14_3_1();  } catch (Throwable t) { Check.crashed("14.3.1", t); }
        try { s14_3_2();  } catch (Throwable t) { Check.crashed("14.3.2", t); }
        try { s14_3_3();  } catch (Throwable t) { Check.crashed("14.3.3", t); }
        try { s14_3_4();  } catch (Throwable t) { Check.crashed("14.3.4", t); }
        try { s14_5();    } catch (Throwable t) { Check.crashed("14.5", t); }
        try { s14_6();    } catch (Throwable t) { Check.crashed("14.6", t); }
        try { s14_7();    } catch (Throwable t) { Check.crashed("14.7", t); }
        try { s14_8_1();  } catch (Throwable t) { Check.crashed("14.8.1", t); }
        try { s14_8_2();  } catch (Throwable t) { Check.crashed("14.8.2", t); }
        try { s14_9();    } catch (Throwable t) { Check.crashed("14.9", t); }
        try { s14_10();   } catch (Throwable t) { Check.crashed("14.10", t); }
        try { s14_10_1(); } catch (Throwable t) { Check.crashed("14.10.1", t); }
        try { s14_11();   } catch (Throwable t) { Check.crashed("14.11", t); }
        try { s14_11_1(); } catch (Throwable t) { Check.crashed("14.11.1", t); }
        try { s14_11_2(); } catch (Throwable t) { Check.crashed("14.11.2", t); }
        try { s14_12();   } catch (Throwable t) { Check.crashed("14.12", t); }
        try { s14_12_1(); } catch (Throwable t) { Check.crashed("14.12.1", t); }
        try { s14_12_2(); } catch (Throwable t) { Check.crashed("14.12.2", t); }
        try { s14_12_3(); } catch (Throwable t) { Check.crashed("14.12.3", t); }
        try { s14_13();   } catch (Throwable t) { Check.crashed("14.13", t); }
        try { s14_14();   } catch (Throwable t) { Check.crashed("14.14", t); }
        try { s14_15();   } catch (Throwable t) { Check.crashed("14.15", t); }
        try { s14_16();   } catch (Throwable t) { Check.crashed("14.16", t); }
        try { s14_18_1(); } catch (Throwable t) { Check.crashed("14.18.1", t); }
        try { s14_18_2(); } catch (Throwable t) { Check.crashed("14.18.2", t); }
    }
}
