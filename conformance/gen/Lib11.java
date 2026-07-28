// Lib11 — JLS chapter 11, Exceptions.
//
// Eight of the eleven rows carried only by conformance/jls/Ch11.java, which is not read while
// writing these. Transcribed from pp.202-208.
//
// NOT HERE YET, and named rather than implied: §11.5.1.1 (Standard Runtime Exceptions),
// §11.5.1.2 (Standard Checked Exceptions) and §11.5.2 (The Class Error). All three are LISTS --
// the spec enumerates each exception class together with the operation that throws it -- and a
// list is the one shape where sampling silently passes. They get one template per entry once
// pp.209-211 are transcribed, the way §3.9's 47 keywords did.
public class Lib11 {

    private Lib11() {}

    public static void install(Registry r) {
        r.register(new Sn11Causes());
        r.register(new Sn11ErrorUnchecked());
        r.register(new Sn11RuntimeUnchecked());
        r.register(new Sn11FinallyDiscards());
        r.register(new Sn11Precise());
        r.register(new Sn11Hierarchy());
        r.register(new Sn11ExceptionSuperclass());
        r.register(new Sn11ThrowerExample());
    }

    /** The declarations chapter 11 needs: a checked exception, an unchecked one, and an Error,
     *  each thrown from a method whose `throws` clause is exactly what §11.2 requires. */
    static String[] decls() {
        String[] d = { "class T11Checked extends Exception {\n"
                     + "    T11Checked() { super(); }\n"
                     + "    T11Checked(String s) { super(s); }\n"
                     + "}",

                       "class T11Unchecked extends RuntimeException {\n"
                     + "    T11Unchecked(String s) { super(s); }\n"
                     + "}",

                       "class T11Err extends Error {\n"
                     + "    T11Err(String s) { super(s); }\n"
                     + "}",

                       "class T11Thrower {\n"
                     // §11.2.1: an Error needs no throws clause, though it is thrown out.
                     + "    static int raiseError() { throw new T11Err(\"e\"); }\n"
                     // §11.2.2: nor does a RuntimeException.
                     + "    static int raiseUnchecked() { throw new T11Unchecked(\"u\"); }\n"
                     // ...while a CHECKED one must declare it -- the contrast that makes the\n"
                     // two above claims about `throws` rather than about throwing.\n"
                     + "    static int raiseChecked() throws T11Checked { throw new T11Checked(\"c\"); }\n"
                     // §11.3: a finally that completes abruptly DISCARDS the original reason.
                     + "    static int finallyWins() {\n"
                     + "        try { throw new T11Unchecked(\"discarded\"); }\n"
                     + "        finally { return 7; }\n"
                     + "    }\n"
                     + "}" };
        return d;
    }
}

/** §11.1, p.202: "An exception is thrown for one of three reasons" -- a condition the VM
 *  detects synchronously (the section's own example is "an integer divide by zero"), a `throw`
 *  statement, or an asynchronous exception. The third needs Thread.stop or an InternalError and
 *  is out of this language subset; the first two are here.
 *
 *  Also "these exceptions are not thrown at an arbitrary point in the program, but rather at a
 *  point where they are specified as a possible result": the division is what throws, so the
 *  statement before it completed and the statement after it did not run. */
class Sn11Causes implements Snippet {
    public String   id()        { return "t11.causes.vm.and.throw"; }
    public String[] sections()  { return Strs.of("11.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; int zero = 0;"
             + " try { int q = (" + h[0] + ") / zero; got += 1; }"
             + " catch (ArithmeticException e) { got += 2; }"          // VM-detected
             + " try { throw new RuntimeException(\"raised\"); }"
             + " catch (RuntimeException e) { got += 4; }"             // a throw statement
             + " System.out.println(got); }";
    }
    // The `got += 1` after the division never runs: 2 + 4.
    public Val expect(Val[] h) { return Val.ofInt(6); }
}

/** §11.2.1, p.203: "Those unchecked exception classes which are the error classes (Error and
 *  its subclasses) are exempted from compile-time checking."
 *
 *  raiseError() declares no `throws` and still compiles -- that IS the claim. If Error were
 *  checked, T11Thrower would not compile and no case in this batch would run. */
class Sn11ErrorUnchecked implements Snippet, Declaring {
    public String   id()        { return "t11.error.unchecked"; }
    public String[] sections()  { return Strs.of("11.2.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib11.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { T11Thrower.raiseError(); }"
             + " catch (Error e) { got += 1; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(1 + h[0].asInt()); }
}

/** §11.2.2, p.203: "The runtime exception classes (RuntimeException and its subclasses) are
 *  exempted from compile-time checking." Same shape as §11.2.1 and stated separately by the
 *  spec, so pinned separately -- Error and RuntimeException are exempt for different reasons
 *  and an implementation could exempt one without the other. */
class Sn11RuntimeUnchecked implements Snippet, Declaring {
    public String   id()        { return "t11.runtime.unchecked"; }
    public String[] sections()  { return Strs.of("11.2.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib11.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { T11Thrower.raiseUnchecked(); }"
             + " catch (RuntimeException e) { got += 2; }"
             + " try { T11Thrower.raiseChecked(); }"          // the contrast: declared throws
             + " catch (T11Checked e) { got += 4; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(6 + h[0].asInt()); }
}

/** §11.3, p.205: "If a finally clause is executed because of abrupt completion of a try block
 *  and the finally clause itself completes abruptly, then the reason for the abrupt completion
 *  of the try block is DISCARDED and the new reason for abrupt completion is propagated from
 *  there."
 *
 *  finallyWins() throws and its finally returns 7. The exception is discarded entirely -- the
 *  call returns normally, so the catch below never fires. An implementation that let the throw
 *  win, or that ran the finally without discarding, changes the printed number. */
class Sn11FinallyDiscards implements Snippet, Declaring {
    public String   id()        { return "t11.finally.discards.exception"; }
    public String[] sections()  { return Strs.of("11.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib11.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { got = T11Thrower.finallyWins(); }"
             + " catch (RuntimeException e) { got = -1; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7); }
}

/** §11.3.1, p.205: "when the transfer of control takes place, all effects of the statements
 *  executed and expressions evaluated before the point from which the exception is thrown must
 *  appear to have taken place. No expressions, statements, or parts thereof that occur after
 *  the point from which the exception is thrown may appear to have been evaluated."
 *
 *  Both halves in one program: the store before the division is visible, the one after it is
 *  not, and the counter shows exactly one increment happened. This is the section optimisation
 *  breaks -- "if optimized code has speculatively executed some of the expressions or
 *  statements which follow the point at which the exception occurs, such code must be prepared
 *  to hide this speculative execution", so it is run on both tiers like everything else. */
class Sn11Precise implements Snippet {
    public String   id()        { return "t11.precise.effects"; }
    public String[] sections()  { return Strs.of("11.3.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] log = new int[4]; int i = 0; int zero = 0;"
             + " try { log[i++] = 1; int q = 9 / zero; log[i++] = 2; }"
             + " catch (ArithmeticException e) { }"
             + " System.out.println(i + \"|\" + log[0] + \"|\" + log[1]); }";
    }
    // one increment, the first store visible, the second never evaluated
    public Val expect(Val[] h) { return Val.ofString("1|1|0"); }
}

/** §11.5, p.208: "The possible exceptions in a Java program are organized in a hierarchy of
 *  classes, rooted at class Throwable, A DIRECT SUBCLASS OF Object. The classes Exception and
 *  Error are DIRECT subclasses of Throwable. The class RuntimeException is a DIRECT subclass of
 *  Exception."
 *
 *  Every link is stated as DIRECT, so getSuperclass() is the right instrument -- instanceof
 *  would pass on a hierarchy with extra levels spliced in. */
class Sn11Hierarchy implements Snippet {
    public String   id()        { return "t11.hierarchy.direct.links"; }
    public String[] sections()  { return Strs.of("11.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ Throwable t = new Throwable();"
             + " Exception ex = new Exception();"
             + " Error er = new Error();"
             + " RuntimeException re = new RuntimeException();"
             + " System.out.println(t.getClass().getSuperclass() + \"|\""
             + " + ex.getClass().getSuperclass() + \"|\""
             + " + er.getClass().getSuperclass() + \"|\""
             + " + re.getClass().getSuperclass()); }";
    }
    public Val expect(Val[] h) {
        return Val.ofString("class java.lang.Object|class java.lang.Throwable"
                          + "|class java.lang.Throwable|class java.lang.Exception");
    }
}

/** §11.5.1, p.208: "The class Exception is the superclass of all the exceptions that ordinary
 *  programs may wish to recover from." So one `catch (Exception e)` catches a checked exception
 *  and an unchecked one alike -- while an Error, which is not under Exception, passes straight
 *  through it. The Error arm is what makes this a claim about the hierarchy rather than about
 *  catch matching anything. */
class Sn11ExceptionSuperclass implements Snippet, Declaring {
    public String   id()        { return "t11.exception.superclass.of.recoverable"; }
    public String[] sections()  { return Strs.of("11.5.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib11.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { T11Thrower.raiseChecked(); } catch (Exception e) { got += 1; }"
             + " try { T11Thrower.raiseUnchecked(); } catch (Exception e) { got += 2; }"
             + " try { T11Thrower.raiseError(); }"
             + " catch (Exception e) { got += 4; }"           // an Error is NOT an Exception
             + " catch (Error e) { got += 8; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(11); }
}

/** §11.4, pp.206-207: the worked `thrower` example. Its stated output for each argument is two
 *  lines -- the finally's `[thrower("X") done]` FIRST, then the caller's report -- and that
 *  order is the claim §11.4 draws out: "the finally clause is executed on every invocation of
 *  thrower, whether or not an exception occurs".
 *
 *  Joined with " / " rather than a newline because a stitching is one printed line; the order
 *  and the text are preserved exactly. The four arguments are separate templates so a single
 *  broken case names which one. */
class Sn11ThrowerExample implements Snippet, Declaring {
    public String   id()        { return "t11.thrower.example"; }
    public String[] sections()  { return Strs.of("11.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T11TestException extends Exception {\n"
                     + "    T11TestException() { super(); }\n"
                     + "    T11TestException(String s) { super(s); }\n"
                     + "}",

                       "class T11Ex {\n"
                     + "    static StringBuffer out = new StringBuffer();\n"
                     + "    static int thrower(String s) throws T11TestException {\n"
                     + "        try {\n"
                     + "            if (s.equals(\"divide\")) { int i = 0; return i / i; }\n"
                     + "            if (s.equals(\"null\")) { s = null; return s.length(); }\n"
                     + "            if (s.equals(\"test\"))\n"
                     + "                throw new T11TestException(\"Test message\");\n"
                     + "            return 0;\n"
                     + "        } finally {\n"
                     + "            out.append(\"[thrower(\\\"\" + s + \"\\\") done] \");\n"
                     + "        }\n"
                     + "    }\n"
                     + "    static String run(String a) {\n"
                     + "        out = new StringBuffer();\n"
                     + "        try {\n"
                     + "            thrower(a);\n"
                     + "            out.append(\"Test \\\"\" + a + \"\\\" didn't throw an exception\");\n"
                     + "        } catch (Exception e) {\n"
                     + "            out.append(\"Test \\\"\" + a + \"\\\" threw a \" + e.getClass()\n"
                     + "                     + \" with message: \" + e.getMessage());\n"
                     + "        }\n"
                     + "        return out.toString();\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ System.out.println(T11Ex.run(\"divide\")); }";
    }

    // p.207: `[thrower("divide") done]` then `Test "divide" threw a class
    // java.lang.ArithmeticException with message: / by zero`. The finally's line comes FIRST.
    public Val expect(Val[] h) {
        return Val.ofString("[thrower(\"divide\") done] Test \"divide\" threw a "
                          + "class java.lang.ArithmeticException with message: / by zero");
    }
}
