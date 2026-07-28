// Lib14h — JLS chapter 14, batch eight: §14.18.1 and §14.18.2. From pp.291-293.
//
//   §14.18.1 try-catch. "Exception handlers are considered in LEFT-TO-RIGHT order: the earliest
//            possible catch clause accepts the exception." If V is not assignable to any
//            parameter, "the try statement completes abruptly because of a throw of the value
//            V". The worked example prints "BlewIt" -- the SECOND catch, because BlewIt is not
//            a RuntimeException.
//   §14.18.2 try-catch-finally, a decision tree whose two interesting leaves are DISCARDS:
//              catch completes abruptly for R, finally abruptly for S
//                  -> the try completes abruptly for S "(AND REASON R IS DISCARDED)"
//              V matches no catch, finally completes abruptly for S
//                  -> the try completes abruptly for S "(and the throw of value V is DISCARDED
//                     AND FORGOTTEN)"
//            Both are cases where an exception silently disappears, which is exactly the shape
//            an implementation gets wrong by propagating the first reason it saw.
public class Lib14h {

    private Lib14h() {}

    public static void install(Registry r) {
        r.register(new Sn14CatchLeftToRight());
        r.register(new Sn14CatchNotAssignable());
        r.register(new Sn14FinallyOnNormal());
        r.register(new Sn14FinallyKeepsCatchReason());
        r.register(new Sn14FinallyDiscardsCatchReason());
        r.register(new Sn14FinallyDiscardsThrow());
    }

    /** The exception zoo §14.18's rules need: one checked, one unchecked, one unrelated. */
    static String[] decls() {
        String[] d = { "class T14BlewIt extends Exception {\n"
                     + "    T14BlewIt() { }\n"
                     + "    T14BlewIt(String s) { super(s); }\n"
                     + "}",

                       "class T14Blow {\n"
                     + "    static void blowUp() throws T14BlewIt { throw new T14BlewIt(); }\n"
                     + "}" };
        return d;
    }
}

/** §14.18.1's worked example. Two catch clauses, the first for RuntimeException and the second
 *  for BlewIt; the thrown BlewIt is not assignable to RuntimeException, so the SECOND is
 *  selected and the stated output is "BlewIt".
 *
 *  A handler search that took the last matching clause, or the most specific one, would print
 *  the same thing here -- which is why the third arm exists: a RuntimeException thrown into the
 *  same shape must take the FIRST clause. Two throws, one shape, opposite selections. */
class Sn14CatchLeftToRight implements Snippet, Declaring {
    public String   id()        { return "t14.catch.left.to.right"; }
    public String[] sections()  { return Strs.of("14.18.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib14h.decls(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " try { T14Blow.blowUp(); }"
             + " catch (RuntimeException r) { out.append(\"RuntimeException\"); }"
             + " catch (T14BlewIt b) { out.append(\"BlewIt\"); }"
             + " out.append('|');"
             + " try { throw new RuntimeException(\"r\"); }"
             + " catch (RuntimeException r) { out.append(\"first\"); }"
             + " catch (Throwable t) { out.append(\"second\"); }"
             + " System.out.println(out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("BlewIt|first"); }
}

/** §14.18.1: "If the run-time type of V is not assignable to the parameter of any catch clause
 *  of the try statement, then the try statement completes abruptly because of a throw of the
 *  value V" -- so it passes straight through the try and out to the enclosing handler, and the
 *  statement after the try never runs. */
class Sn14CatchNotAssignable implements Snippet, Declaring {
    public String   id()        { return "t14.catch.not.assignable.propagates"; }
    public String[] sections()  { return Strs.of("14.18.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib14h.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try {"
             // The non-matching clause must be UNCHECKED: a catch for a checked exception the
             // body cannot throw is itself a compile-time error (§11.2), which is a different
             // rule and would reject this case instead of exercising §14.18.1's.
             + "   try { throw new IllegalArgumentException(\"v\"); }"
             + "   catch (ArithmeticException b) { got += 100; }"   // not assignable
             + "   got += 200;"                                // not reached
             + " } catch (IllegalArgumentException e) { got += 3; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3); }
}

/** §14.18.2, first branch: "If execution of the try block completes normally, then the finally
 *  block is executed, and then ... the try statement completes normally." No exception is
 *  involved at all -- finally is not an exception mechanism. */
class Sn14FinallyOnNormal implements Snippet {
    public String   id()        { return "t14.finally.on.normal"; }
    public String[] sections()  { return Strs.of("14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " try { out.append('t'); } finally { out.append('f'); }"
             + " out.append('a');"                             // the try completed normally
             + " System.out.println(out.toString() + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofString("tfa" + h[0].asInt()); }
}

/** §14.18.2: "If the catch block completes abruptly for reason R, then the finally block is
 *  executed. ... If the finally block completes NORMALLY, then the try statement completes
 *  abruptly for reason R." The finally runs, and R survives it. */
class Sn14FinallyKeepsCatchReason implements Snippet {
    public String   id()        { return "t14.finally.keeps.catch.reason"; }
    public String[] sections()  { return Strs.of("14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " try {"
             + "   try { throw new RuntimeException(\"v\"); }"
             + "   catch (RuntimeException e) { out.append('c'); throw new RuntimeException(\"R\"); }"
             + "   finally { out.append('f'); }"
             + " } catch (Throwable t) { out.append('R'); }"
             + " System.out.println(out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("cfR"); }
}

/** §14.18.2's first discard: "If the finally block completes abruptly for reason S, then the
 *  try statement completes abruptly for reason S (AND REASON R IS DISCARDED)."
 *
 *  The catch throws R, the finally then returns -- an abrupt completion of its own -- so R is
 *  gone and the method returns normally. An implementation that let R win would throw. */
class Sn14FinallyDiscardsCatchReason implements Snippet, Declaring {
    public String   id()        { return "t14.finally.discards.catch.reason"; }
    public String[] sections()  { return Strs.of("14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Discard {\n"
                     + "    static int catchThrowsFinallyReturns() {\n"
                     + "        try {\n"
                     + "            throw new RuntimeException(\"v\");\n"
                     + "        } catch (RuntimeException e) {\n"
                     + "            throw new RuntimeException(\"R\");\n"
                     + "        } finally {\n"
                     + "            return 7;\n"
                     + "        }\n"
                     + "    }\n"
                     + "    static int noCatchFinallyReturns() {\n"
                     + "        try {\n"
                     + "            throw new RuntimeException(\"V\");\n"
                     + "        } finally {\n"
                     + "            return 9;\n"
                     + "        }\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ int a = 0, b = 0;"
             + " try { a = T14Discard.catchThrowsFinallyReturns(); }"
             + " catch (RuntimeException e) { a = -1; }"
             + " try { b = T14Discard.noCatchFinallyReturns(); }"
             + " catch (RuntimeException e) { b = -1; }"
             + " System.out.println(a + \"|\" + b); }";
    }
    // R discarded by the finally's return; V likewise "discarded and forgotten".
    public Val expect(Val[] h) { return Val.ofString("7|9"); }
}

/** §14.18.2's second discard, stated for a value that matched NO catch clause: "If the finally
 *  block completes abruptly for reason S, then the try statement completes abruptly for reason
 *  S (and the throw of value V is DISCARDED AND FORGOTTEN)."
 *
 *  Here the finally completes abruptly by BREAKING out of an enclosing loop rather than by
 *  returning, so the discard is exercised through a different abrupt reason than the case
 *  above -- §14.1 lists seven, and finally can complete abruptly by any of them. */
class Sn14FinallyDiscardsThrow implements Snippet {
    public String   id()        { return "t14.finally.discards.throw"; }
    public String[] sections()  { return Strs.of("14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " here: {"
             + "   try {"
             + "     try { throw new RuntimeException(\"V\"); }"
             + "     finally { out.append('f'); break here; }"
             + "   } catch (Throwable t) { out.append('X'); }"     // never reached
             + " }"
             + " out.append('d');"
             + " System.out.println(out.toString()); }";
    }
    // the break discards the pending throw entirely: no 'X'.
    public Val expect(Val[] h) { return Val.ofString("fd"); }
}
