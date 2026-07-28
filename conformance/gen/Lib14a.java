// Lib14a — JLS chapter 14, batch one: §14.1 and §14.3.2.
//
// Chapter 14 is 23 rows carried only by conformance/jls/Ch14.java, and it is done in batches so
// each is transcribed from its own pages and verified before the next. Ch14.java is not read.
//
//   §14.1   p.264  "An abrupt completion always has an associated reason, which is one of the
//                  following" -- SEVEN, enumerated: break with no label, break with a label,
//                  continue with no label, continue with a label, return with no value, return
//                  with a value, and throw with a given value. Also: "the only reason an
//                  EXPRESSION can complete abruptly is that an exception is thrown".
//   §14.3.2 p.267  "The scope of a local variable declared in a block is the rest of the block,
//                  INCLUDING ITS OWN INITIALIZER." Two programs are printed with their output,
//                  and one is printed as a compile-time error.
public class Lib14a {

    private Lib14a() {}

    public static void install(Registry r) {
        // §14.1's seven reasons, one template each. A closed list the spec prints in full, so a
        // subset would be a subset I chose -- and each reason takes a different path through the
        // ddcg's destination-driven lowering, which is precisely where they can differ.
        reason(r, "break.unlabeled",
               "for (int i = 0; i < 3; i++) { got += 1; if (i == 1) break; }", 2);
        reason(r, "break.labeled",
               "outer: for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) {"
             + " got += 1; if (j == 1) break outer; } }", 2);
        reason(r, "continue.unlabeled",
               "for (int i = 0; i < 3; i++) { if (i == 1) continue; got += 1; }", 2);
        reason(r, "continue.labeled",
               "outer: for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) {"
             + " if (j == 1) continue outer; got += 1; } }", 3);
        reason(r, "throw.value",
               "try { got += 1; throw new RuntimeException(\"r\"); }"
             + " catch (RuntimeException e) { got += 1; }", 2);

        // The two `return` reasons need a method to return FROM, so they carry declarations.
        r.register(new Sn14ReturnReasons());

        // §14.3.2, the spec's own three programs.
        r.register(new Sn14ScopeOwnInitializer());
        r.register(new Sn14ScopeMultiDeclarator());
        r.register(new Sn14RejectSelfInitializer());
        r.register(new Sn14RejectRedeclareLocal());
    }

    private static void reason(Registry r, String name, String body, int expect) {
        r.register(new Sn14Reason(name, body, expect));
    }
}

/** One of §14.1's seven reasons for abrupt completion, exercised where it changes a count. The
 *  count is what distinguishes them: an unlabeled `continue` that behaved like a labeled one, or
 *  a labeled `break` that only left the inner loop, prints a different number rather than
 *  silently doing the wrong thing. */
class Sn14Reason implements Snippet {

    private String name;
    private String body;
    private int    expect;

    Sn14Reason(String name, String body, int expect) {
        this.name = name; this.body = body; this.expect = expect;
    }

    public String   id()        { return "t14.abrupt." + name; }
    public String[] sections()  { return Strs.of("14.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0; " + body + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(expect + h[0].asInt()); }
}

/** §14.1's remaining two reasons: "a return with no value" and "a return with a given value".
 *  Both need a method, so they share one companion class -- and the void one is observable only
 *  through a side effect, which is why it writes to a field before returning. */
class Sn14ReturnReasons implements Snippet, Declaring {
    public String   id()        { return "t14.abrupt.return.both"; }
    public String[] sections()  { return Strs.of("14.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Ret {\n"
                     + "    static int mark = 0;\n"
                     + "    static void noValue(int n) {\n"
                     + "        mark = 1;\n"
                     + "        if (n >= 0) return;\n"      // a return with no value
                     + "        mark = 99;\n"
                     + "    }\n"
                     // A return with a given value. Unguarded on purpose: a guard here would
                     // make expect() model the guard rather than the RETURN, and the hole
                     // reaches negative values (lit.int.neg3), which is how the first draft of
                     // this template composed a wrong answer.
                     + "    static int withValue(int n) {\n"
                     + "        return n + 4;\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Ret.mark = 0; T14Ret.noValue(1);"
             + " System.out.println(T14Ret.mark + T14Ret.withValue(" + h[0] + ")); }";
    }
    // mark is 1 (the statement after the return did not run), withValue is hole + 4.
    public Val expect(Val[] h) { return Val.ofInt(5 + h[0].asInt()); }
}

/** §14.3.2, the program the spec says DOES compile: "int x = (x=2)*2;" ... "because the local
 *  variable x is definitely assigned (§16) before it is used. It prints: 4".
 *
 *  This is the positive half of "the scope ... is the rest of the block, INCLUDING ITS OWN
 *  INITIALIZER" -- the initializer may mention x precisely because x is already in scope there. */
class Sn14ScopeOwnInitializer implements Snippet {
    public String   id()        { return "t14.scope.own.initializer"; }
    public String[] sections()  { return Strs.of("14.3.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int x = (x = 2) * 2; System.out.println(x); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(4); }
}

/** §14.3.2's other printed program, whose stated output is "2+1=3": a later declarator in the
 *  same declaration sees an earlier one, because the scope began at that earlier declarator. */
class Sn14ScopeMultiDeclarator implements Snippet {
    public String   id()        { return "t14.scope.multi.declarator"; }
    public String[] sections()  { return Strs.of("14.3.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int two = 2, three = two + 1;"
             + " System.out.println(\"2+1=\" + three); }";
    }
    public Val expect(Val[] h) { return Val.ofString("2+1=3"); }
}

/** §14.3.2's compile-time error, verbatim:
 *
 *      class Test {
 *          static int x;
 *          public static void main(String[] args) {
 *              int x = x;
 *          }
 *      }
 *
 *  "causes a compile-time error because the initialization of x is within the scope of the
 *  declaration of x as a local variable, and the local x does not yet have a value and cannot
 *  be used." The static field of the same name is what makes it a scope question rather than an
 *  undefined-name one: without the local's scope reaching its own initializer, `x` would
 *  resolve to the field and this would compile. */
class Sn14RejectSelfInitializer implements Snippet {
    public String   id()        { return "t14.reject.self.initializer"; }
    public String[] sections()  { return Strs.of("14.3.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14SelfInit {\n"
             + "    static int x;\n"
             + "    static void f() {\n"
             + "        int x = x;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("x"); }
}

/** §14.3.2: "The name of the local variable parameter may not be redeclared as a local variable
 *  or exception parameter within its scope, or a compile-time error occurs; that is, hiding the
 *  name of a local variable is not permitted."
 *
 *  Note what this does NOT say: a local may hide a FIELD, which §14.3.3 is about and which
 *  t6.hide.local.hides.classvar already exercises. Only local-over-local is banned. */
class Sn14RejectRedeclareLocal implements Snippet {
    public String   id()        { return "t14.reject.redeclare.local"; }
    public String[] sections()  { return Strs.of("14.3.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14Redeclare {\n"
             + "    static void f() {\n"
             + "        int n = 1;\n"
             + "        {\n"
             + "            int n = 2;\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("n"); }
}
