// Lib14f — JLS chapter 14, batch six: §14.11, §14.11.1, §14.11.2. From pp.278-280.
//
//   §14.11   "A do statement is executed by FIRST EXECUTING THE STATEMENT" and "executing a do
//            statement always executes the contained Statement AT LEAST ONCE" -- the difference
//            from while (§14.10), where a false condition runs the body zero times.
//   §14.11.1 abrupt completion of the body:
//              break, no label     -> the do completes normally
//              continue, no label  -> "the EXPRESSION IS EVALUATED", then true re-runs the whole
//                                     do and false completes it normally. Note what this is NOT:
//                                     continue does not restart the body, it jumps to the test.
//              continue with label L, do HAS L      -> same, via the Expression
//              continue with label L, do lacks L    -> completes abruptly, propagating
//   §14.11.2 the worked example: a toHexString built on do, "because at least one digit must be
//            generated, the do statement is an appropriate control structure".
public class Lib14f {

    private Lib14f() {}

    public static void install(Registry r) {
        r.register(new Sn14DoAtLeastOnce());
        r.register(new Sn14DoBreakAndContinue());
        r.register(new Sn14DoHexExample());
    }
}

/** §14.11: the body runs at least once even when the condition is false from the start -- the
 *  test follows the body. Paired against a while with the same false condition in the same
 *  line, which runs zero times, so the two control structures are distinguished rather than
 *  each being checked in isolation. */
class Sn14DoAtLeastOnce implements Snippet {
    public String   id()        { return "t14.do.at.least.once"; }
    public String[] sections()  { return Strs.of("14.11", "14.10"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int doRuns = 0, whileRuns = 0; boolean go = false;"
             + " do { doRuns += 1; } while (go);"
             + " while (go) { whileRuns += 1; }"
             + " System.out.println(doRuns + \"|\" + whileRuns + \"|\" + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofString("1|0|" + h[0].asInt()); }
}

/** §14.11.1: an unlabeled break completes the do NORMALLY, and an unlabeled continue evaluates
 *  THE EXPRESSION rather than restarting the body.
 *
 *  The continue loop is written so those two readings differ: the counter is bumped before the
 *  continue and the condition advances the index, so "jump to the test" terminates while
 *  "restart the body" would spin forever on the same value. */
class Sn14DoBreakAndContinue implements Snippet {
    public String   id()        { return "t14.do.break.and.continue"; }
    public String[] sections()  { return Strs.of("14.11.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; int n = 0;"
             + " do { n++; if (n == 3) break; got += 1; } while (n < 10);"
             + " got += 10;"                             // reached: break completed it normally
             + " int m = 0;"
             + " do { m++; if (m == 2) continue; got += 100; } while (m < 4);"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // first: n=1,2 add 1 (n=3 breaks) -> 2; +10; second: m=1,3,4 add 100 -> 300
    public Val expect(Val[] h) { return Val.ofInt(312 + h[0].asInt()); }
}

/** §14.11.2's worked example, reproduced with its shape intact -- append the low nibble, shift
 *  right unsigned, repeat while non-zero, then reverse.
 *
 *  The expectations are TRANSCRIBED, not obtained by calling Integer.toHexString: composing an
 *  expectation from the library the case also exercises would agree with itself whatever either
 *  one did. Zero is the case the section exists for -- "at least one digit must be generated",
 *  so the answer is "0" and not the empty string, which is what a while loop here would give. */
class Sn14DoHexExample implements Snippet, Declaring {
    public String   id()        { return "t14.do.hex.example"; }
    public String[] sections()  { return Strs.of("14.11.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Hex {\n"
                     + "    public static String toHexString(int i) {\n"
                     + "        StringBuffer buf = new StringBuffer(8);\n"
                     + "        do {\n"
                     + "            buf.append(Character.forDigit(i & 0xF, 16));\n"
                     + "            i >>>= 4;\n"
                     + "        } while (i != 0);\n"
                     + "        return buf.reverse().toString();\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ System.out.println(T14Hex.toHexString(0) + \"|\""
             + " + T14Hex.toHexString(255) + \"|\""
             + " + T14Hex.toHexString(4096) + \"|\""
             + " + T14Hex.toHexString(-1)); }";
    }
    // 0 -> "0" (the at-least-once digit); 255 -> ff; 4096 -> 1000; -1 -> ffffffff, which needs
    // the UNSIGNED shift: an arithmetic >> would never reach zero and would not terminate.
    public Val expect(Val[] h) { return Val.ofString("0|ff|1000|ffffffff"); }
}
