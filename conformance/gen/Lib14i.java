// Lib14i — JLS chapter 14, batch nine: §14.13, §14.14, §14.16. From pp.283-287.
//
//   §14.13 break. "A break statement with no label attempts to transfer control to the
//          INNERMOST ENCLOSING switch, while, do, or for statement; this statement, which is
//          called the break target, then immediately COMPLETES NORMALLY." With a label it
//          targets "the enclosing labeled statement that has the same Identifier as its label",
//          and "in this case, the break target NEED NOT BE a while, do, for, or switch
//          statement". "If no switch, while, do, or for statement encloses the break statement,
//          a compile-time error occurs"; likewise if no labeled statement with that Identifier
//          encloses it.
//          "The preceding descriptions say 'attempts to transfer control' rather than just
//          'transfers control' because if there are any try statements within the break target
//          whose try blocks contain the break statement, then any FINALLY clauses of those try
//          statements are executed, IN ORDER, INNERMOST TO OUTERMOST, before control is
//          transferred to the break target."
//
//   §14.14 continue. "A continue statement MAY OCCUR ONLY IN a while, do, or for statement" --
//          not a switch, which is where it differs from break. Unlabeled it targets the
//          innermost such statement and "immediately ends the current iteration and begins a
//          new one". Labeled, "the continue target MUST BE a while, do, or for statement or a
//          compile-time error occurs" -- again unlike break, whose labeled target need not be a
//          loop. Same finally rule, innermost to outermost.
//
//   §14.16 throw. "The Expression in a throw statement must denote a variable or value of a
//          reference type which is ASSIGNABLE (§5.2) to the type Throwable, or a compile-time
//          error occurs" -- assignable, so Throwable ITSELF qualifies by §5.2's identity
//          conversion. Beyond that, at least one of three conditions must hold: the type is
//          unchecked; or "the throw statement is contained in the try block of a try statement
//          ... and the type of the Expression is assignable ... to the type of the parameter of
//          at least one catch clause"; or it "is contained in a method or constructor
//          declaration and the type of the Expression is assignable ... to at least one type
//          listed in the throws clause".
//          Then the dynamics: "A throw statement FIRST EVALUATES the Expression. If the
//          evaluation of the Expression completes abruptly for some reason, then the throw
//          completes abruptly FOR THAT REASON." The transfer of control "may exit multiple
//          statements and multiple constructor, static and field initializer evaluations, and
//          method invocations until a try statement is found that catches the thrown value" --
//          and "if a throw statement is contained in a constructor declaration, but its value is
//          not caught by some try statement that contains it, then the CLASS INSTANCE CREATION
//          EXPRESSION ... will complete abruptly because of the throw."
public class Lib14i {

    private Lib14i() {}

    public static void install(Registry r) {
        r.register(new Sn14BreakTargetCompletesNormally());
        r.register(new Sn14BreakLabelNeedNotBeLoop());
        r.register(new Sn14BreakRunsFinalliesInnermostFirst());
        r.register(new Sn14ContinueEndsIteration());
        r.register(new Sn14ContinueRunsFinallies());
        r.register(new Sn14ThrowExitsMultipleFrames());
        r.register(new Sn14ThrowEvaluatesExpressionFirst());
        r.register(new Sn14ThrowFromConstructor());
        r.register(new Sn14ThrowTypeThrowableItself());

        r.register(new Sn14RejectBreakOutsideLoop());
        r.register(new Sn14RejectBreakUnknownLabel());
        r.register(new Sn14RejectContinueOutsideLoop());
        r.register(new Sn14RejectContinueInSwitch());
        r.register(new Sn14RejectContinueTargetNotLoop());
        r.register(new Sn14RejectThrowNonThrowable());
        r.register(new Sn14RejectThrowCheckedUndeclared());
    }
}

/** §14.13: the break target "then immediately COMPLETES NORMALLY" — so execution resumes after
 *  it, not after the whole method. The statement following each loop runs, which is what
 *  distinguishes completing normally from propagating. */
class Sn14BreakTargetCompletesNormally implements Snippet {
    public String   id()        { return "t14.break.target.completes.normally"; }
    public String[] sections()  { return Strs.of("14.13"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " for (int i = 0; i < 9; i++) { if (i == 2) break; got += 1; }"
             + " got += 10;"                              // the for completed NORMALLY
             + " switch (2) { case 2: got += 100; break; case 3: got += 5000; }"
             + " got += 1000;"                            // ...and so did the switch
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(1112 + h[0].asInt()); }
}

/** §14.13: with a label "the break target NEED NOT BE a while, do, for, or switch statement" --
 *  a labeled BLOCK is a legal target. §14.14 says the opposite for continue, whose target "must
 *  be a while, do, or for statement", and t14.reject.continue.in.switch pins that difference. */
class Sn14BreakLabelNeedNotBeLoop implements Snippet {
    public String   id()        { return "t14.break.label.need.not.be.loop"; }
    public String[] sections()  { return Strs.of("14.13"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " plain: { got += 1; if (got == 1) break plain; got += 100; }"
             + " got += 2;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3 + h[0].asInt()); }
}

/** §14.13: "any finally clauses of those try statements are executed, IN ORDER, INNERMOST TO
 *  OUTERMOST, before control is transferred to the break target."
 *
 *  Two nested try-finallies inside the loop, each appending a distinct character. The ORDER is
 *  the claim, so the printed string — not a sum — is what pins it: outermost-first would read
 *  "ob", and running only one would read "i" or "o". */
class Sn14BreakRunsFinalliesInnermostFirst implements Snippet {
    public String   id()        { return "t14.break.finallies.innermost.first"; }
    public String[] sections()  { return Strs.of("14.13", "14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " for (int i = 0; i < 3; i++) {"
             + "   try {"
             + "     try { out.append('x'); break; }"
             + "     finally { out.append('i'); }"
             + "   } finally { out.append('o'); }"
             + " }"
             + " out.append('b');"
             + " System.out.println(out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("xiob"); }
}

/** §14.14: an unlabeled continue "immediately ends the current iteration and BEGINS A NEW ONE",
 *  and a labeled one names the loop whose iteration begins anew. Both in one program, with the
 *  statement after the continue never running. */
class Sn14ContinueEndsIteration implements Snippet {
    public String   id()        { return "t14.continue.ends.iteration"; }
    public String[] sections()  { return Strs.of("14.14"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " for (int i = 0; i < 4; i++) { if (i == 1) continue; got += 1; }"
             + " outer: for (int i = 0; i < 3; i++) {"
             + "   for (int j = 0; j < 3; j++) { if (j == 1) continue outer; got += 10; }"
             + "   got += 500;"                           // never reached
             + " }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // 3 unskipped iterations, then 3 outer passes each adding 10 once before continuing
    public Val expect(Val[] h) { return Val.ofInt(33 + h[0].asInt()); }
}

/** §14.14: "If there are any try statements within the continue target whose try blocks contain
 *  the continue statement, then any finally clauses of those try statements are executed, in
 *  order, INNERMOST TO OUTERMOST, before control is transferred to the continue target."
 *
 *  §14.13's break says the same thing about its own target, but they are two separate sentences
 *  about two separate statements: an implementation can route break through the finally chain and
 *  not continue, so each gets its own case. Three iterations, so the chain runs three times. */
class Sn14ContinueRunsFinallies implements Snippet {
    public String   id()        { return "t14.continue.finallies.innermost.first"; }
    public String[] sections()  { return Strs.of("14.14", "14.18.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " for (int i = 0; i < 3; i++) {"
             + "   try {"
             + "     try { out.append('x'); continue; }"
             + "     finally { out.append('i'); }"
             + "   } finally { out.append('o'); }"
             + " }"
             + " out.append('d');"
             + " System.out.println(out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("xioxioxiod"); }
}

/** §14.16: a throw is "an immediate transfer of control that MAY EXIT MULTIPLE STATEMENTS and
 *  ... method invocations until a try statement is found that catches the thrown value."
 *
 *  Three frames deep with nested blocks and loops in between; nothing after the throw at any
 *  level runs, and the handler is the first enclosing one that matches. */
class Sn14ThrowExitsMultipleFrames implements Snippet, Declaring {
    public String   id()        { return "t14.throw.exits.multiple.frames"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Deep {\n"
                     + "    static StringBuffer out = new StringBuffer();\n"
                     + "    static void c() { out.append('c'); throw new RuntimeException(\"d\");\n"
                     + "                      }\n"
                     + "    static void b() { out.append('b'); c(); out.append('B'); }\n"
                     + "    static void a() {\n"
                     + "        out.append('a');\n"
                     + "        for (int i = 0; i < 3; i++) { { b(); } out.append('L'); }\n"
                     + "        out.append('A');\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Deep.out = new StringBuffer();"
             + " try { T14Deep.a(); } catch (RuntimeException e) { T14Deep.out.append('!'); }"
             + " System.out.println(T14Deep.out.toString()); }";
    }
    // a, b, c, then straight out: no 'B', no 'L', no 'A'.
    public Val expect(Val[] h) { return Val.ofString("abc!"); }
}

/** §14.16: "A throw statement FIRST EVALUATES the Expression. If the evaluation of the Expression
 *  completes abruptly for some reason, then the throw completes abruptly FOR THAT REASON."
 *
 *  The expression here is a call declared to return ArithmeticException that instead throws a
 *  NullPointerException. The reason that propagates is the EXPRESSION's, so the first catch is
 *  the one selected; an implementation that reached the throw with the expression's static type
 *  would land in the second. Two clauses, and only one of them can be right. */
class Sn14ThrowEvaluatesExpressionFirst implements Snippet, Declaring {
    public String   id()        { return "t14.throw.evaluates.expression.first"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Eval {\n"
                     + "    static StringBuffer out = new StringBuffer();\n"
                     + "    static ArithmeticException boom() {\n"
                     + "        out.append('e');\n"
                     + "        throw new NullPointerException();\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Eval.out = new StringBuffer();"
             + " try { throw T14Eval.boom(); }"
             + " catch (NullPointerException n) { T14Eval.out.append('n'); }"
             + " catch (ArithmeticException a) { T14Eval.out.append('a'); }"
             + " System.out.println(T14Eval.out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("en"); }
}

/** §14.16: "If a throw statement is contained in a constructor declaration, but its value is not
 *  caught by some try statement that contains it, then the CLASS INSTANCE CREATION EXPRESSION ...
 *  that invoked the constructor will complete abruptly because of the throw."
 *
 *  So the `new` expression itself completes abruptly: the assignment it feeds never happens, and
 *  the local keeps its previous value. That is the observable difference between the creation
 *  expression completing abruptly and the constructor merely returning a half-built object. */
class Sn14ThrowFromConstructor implements Snippet, Declaring {
    public String   id()        { return "t14.throw.from.constructor"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Ctor {\n"
                     + "    static StringBuffer out = new StringBuffer();\n"
                     + "    int v;\n"
                     + "    T14Ctor(int x) {\n"
                     + "        out.append('k');\n"
                     + "        if (x > 0) throw new RuntimeException(\"c\");\n"
                     + "        v = x;\n"
                     + "        out.append('K');\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Ctor.out = new StringBuffer();"
             + " T14Ctor got = null;"
             + " try { got = new T14Ctor(1); T14Ctor.out.append('X'); }"
             + " catch (RuntimeException e) { T14Ctor.out.append('!'); }"
             + " T14Ctor.out.append(got == null ? 'z' : 'o');"
             + " System.out.println(T14Ctor.out.toString()); }";
    }
    // 'k' then straight out of the creation expression: no 'K', no 'X', and got was never assigned.
    public Val expect(Val[] h) { return Val.ofString("k!z"); }
}

/** §14.16: the Expression's type must be "ASSIGNABLE (§5.2) to the type Throwable" -- assignable,
 *  not a strict subclass. §5.2 assignment conversion begins with the identity conversion (§5.1.1),
 *  so a value whose type is Throwable itself satisfies the rule.
 *
 *  It is also the shape that arises from catching broadly and rethrowing, so the first §14.16
 *  bullet carries it: the rethrow sits in a try block with a catch clause whose parameter type is
 *  Throwable, which is what makes a checked value legal to throw here without a throws clause. */
class Sn14ThrowTypeThrowableItself implements Snippet {
    public String   id()        { return "t14.throw.type.throwable.itself"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try {"
             + "   try { throw new RuntimeException(\"v\"); }"
             + "   catch (Throwable t) { got += 1; throw t; }"
             + " } catch (Throwable t2) { got += 20; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(21 + h[0].asInt()); }
}

/** §14.13: "If no switch, while, do, or for statement encloses the break statement, a
 *  compile-time error occurs." */
class Sn14RejectBreakOutsideLoop implements Snippet {
    public String   id()        { return "t14.reject.break.outside.loop"; }
    public String[] sections()  { return Strs.of("14.13"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14BreakLoose {\n"
             + "    static void f() {\n"
             + "        int n = 1;\n"
             + "        break;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("break outside loop"); }
}

/** §14.13: "If no labeled statement with Identifier as its label contains the break statement, a
 *  compile-time error occurs." The break here IS inside a for, so the unlabeled rule is
 *  satisfied and only the missing label can reject it. */
class Sn14RejectBreakUnknownLabel implements Snippet {
    public String   id()        { return "t14.reject.break.unknown.label"; }
    public String[] sections()  { return Strs.of("14.13"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14BreakNoSuchLabel {\n"
             + "    static void f() {\n"
             + "        for (int i = 0; i < 3; i++) {\n"
             + "            break nowhere;\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("label 'nowhere' not found"); }
}

/** §14.14: "If no while, do, or for statement encloses the continue statement, a compile-time
 *  error occurs." */
class Sn14RejectContinueOutsideLoop implements Snippet {
    public String   id()        { return "t14.reject.continue.outside.loop"; }
    public String[] sections()  { return Strs.of("14.14"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14ContinueLoose {\n"
             + "    static void f() {\n"
             + "        int n = 1;\n"
             + "        continue;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("continue outside loop"); }
}

/** §14.14: "A continue statement may occur ONLY IN a while, do, or for statement." A switch is
 *  a legal break target and NOT a legal continue target — the one asymmetry between the two
 *  statements, and the reason t14.break.target.completes.normally breaks out of a switch in the
 *  very same batch. */
class Sn14RejectContinueInSwitch implements Snippet {
    public String   id()        { return "t14.reject.continue.in.switch"; }
    public String[] sections()  { return Strs.of("14.14"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14ContinueSwitch {\n"
             + "    static void f(int x) {\n"
             + "        switch (x) {\n"
             + "            case 1: continue;\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("continue outside loop"); }
}

/** §14.14: "The continue target must be a while, do, or for statement or a compile-time error
 *  occurs."
 *
 *  This is the rule §14.13 explicitly does NOT have -- a labeled break's target "need not be a
 *  while, do, for, or switch statement", which t14.break.label.need.not.be.loop pins by breaking
 *  to a labeled block. Here the continue is inside a for (so it has a legal unlabeled target)
 *  and `here` does contain it (so the label resolves); the only thing wrong is that `here`
 *  labels a BLOCK. Nothing but the continue-target rule can reject it. */
class Sn14RejectContinueTargetNotLoop implements Snippet {
    public String   id()        { return "t14.reject.continue.target.not.loop"; }
    public String[] sections()  { return Strs.of("14.14"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14ContinueNotLoop {\n"
             + "    static void f() {\n"
             + "        here: {\n"
             + "            for (int i = 0; i < 3; i++) {\n"
             + "                continue here;\n"
             + "            }\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("continue target 'here' is not a loop"); }
}

/** §14.16: "The Expression in a throw statement must denote a variable or value of a reference
 *  type which is assignable (§5.2) to the type Throwable, or a compile-time error occurs." A
 *  String is a reference type and still not throwable. */
class Sn14RejectThrowNonThrowable implements Snippet {
    public String   id()        { return "t14.reject.throw.non.throwable"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14ThrowString {\n"
             + "    static void f() {\n"
             + "        throw \"not a throwable\";\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("is not a subclass of Throwable"); }
}

/** §14.16's three conditions, of which at least one must hold. Here none does: the type is
 *  checked (not a RuntimeException or Error), the throw is in no try block, and the method
 *  declares no throws clause. "A compile-time error occurs."
 *
 *  The corresponding LEGAL shape is t14.throw.type.throwable.itself, which satisfies the second
 *  condition by sitting in a try whose catch parameter it is assignable to. */
class Sn14RejectThrowCheckedUndeclared implements Snippet {
    public String   id()        { return "t14.reject.throw.checked.undeclared"; }
    public String[] sections()  { return Strs.of("14.16"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14Checked extends Exception {\n"
             + "}\n"
             + "\n"
             + "class T14ThrowUndeclared {\n"
             + "    static void f() {\n"
             + "        throw new T14Checked();\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("unhandled checked exception: throws T14Checked"); }
}
