// Lib6 — JLS chapter 6, Names.
//
// Chapter 6 is about which DECLARATION an identifier denotes, so almost every claim needs a
// declaration beside the case class rather than an expression inside it. That is what
// Declaring is for, and this library is its heaviest user.
//
// The chapter splits three ways, and only the first is here:
//
//   single compilation unit, runs      -> this file
//   compile-time error                 -> conformance/reject/ch6_*
//   spans two packages                 -> needs companion compilation units, which Emit
//                                         cannot yet write; see the ledger rows still UNCOVERED
//
// Where the spec prints a program AND its output (§6.3.1), the output is reproduced exactly
// rather than paraphrased into a number: the section's claim IS that text.
public class Lib6 {

    private Lib6() {}

    public static void install(Registry r) {
        r.register(new Sn6CtorName());
        r.register(new Sn6LabelNamespace());
        r.register(new Sn6ForwardFromCtor());
        r.register(new Sn6Hiding());
    }
}

/** §6.1: "Constructors (§8.6) are also introduced by declarations, but use the name of the
 *  class in which they are declared rather than introducing a new name." Because the
 *  constructor introduces no name, the class name is still free for a method — a compiler that
 *  entered constructors into the ordinary name table would reject this. */
class Sn6CtorName implements Snippet, Declaring {

    public String   id()          { return "t6.decl.ctor.introduces.no.name"; }
    public String[] sections()    { return Strs.of("6.1"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T6Ctor {\n"
                     + "    int v;\n"
                     + "    T6Ctor(int v) { this.v = v; }\n"
                     + "    static int T6Ctor(int n) { return n * 2; }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T6Ctor o = new T6Ctor(" + h[0] + ");"
             + " System.out.println(o.v + T6Ctor.T6Ctor(3)); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(h[0].asInt() + 6); }
}

/** §6.2: identifiers used "as labels in labeled statements (§14.6) and in break (§14.13) and
 *  continue (§14.14) statements" are listed among the identifiers that are NOT part of a name.
 *  So a label may spell the same identifier as a local variable in scope without hiding it —
 *  `n` is the label of the loop and the variable the loop increments, in one body. */
class Sn6LabelNamespace implements Snippet, Declaring {

    public String   id()          { return "t6.name.label.separate.namespace"; }
    public String[] sections()    { return Strs.of("6.2"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String[] imports()     { return Strs.none(); }
    public String[] decls()       { return Strs.none(); }

    public String render(String[] h) {
        return "{ int n = " + h[0] + ";"
             + " n: for (int i = 0; i < 3; i++) {"
             + " if (i == 1) continue n;"
             + " n = n + 1; }"
             + " System.out.println(n); }";
    }

    // i == 1 takes the labeled continue and skips the increment; i == 0 and i == 2 do not.
    public Val expect(Val[] h) { return Val.ofInt(h[0].asInt() + 2); }
}

/** §6.3: "The declaration of a member needs to appear before it is used only when the use is
 *  in a field initialization expression." The spec's second Test program is reproduced with
 *  its shape intact — a constructor assigning `k`, declared three lines later, and a field
 *  initializer `int i = j;` reading a field declared ABOVE it, which is the legal direction.
 *  The illegal one (`int i = j;` above `int j = 1;`) is reject/ch6_forward_reference_in_field. */
class Sn6ForwardFromCtor implements Snippet, Declaring {

    public String   id()          { return "t6.scope.ctor.uses.later.field"; }
    public String[] sections()    { return Strs.of("6.3"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T6Scope {\n"
                     + "    T6Scope() { k = 2; }\n"
                     + "    int j = 1;\n"
                     + "    int i = j;\n"
                     + "    int k;\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T6Scope o = new T6Scope();"
             + " System.out.println(o.i + o.j + o.k + " + h[0] + "); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(h[0].asInt() + 4); }
}

/** §6.3.1: the worked example, whose stated output is "x=0, Test.x=1". A local variable hides
 *  a class variable of the same name, and the hidden one is still reachable through the class
 *  type name — the two halves are one claim, so both are in the printed line.
 *
 *  No hole: the section's claim IS this exact text, and a hole would replace the stated output
 *  with an arithmetic identity that no longer quotes the spec. */
class Sn6Hiding implements Snippet, Declaring {

    public String   id()          { return "t6.hide.local.hides.classvar"; }
    public String[] sections()    { return Strs.of("6.3.1"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T6Hide {\n"
                     + "    static int x = 1;\n"
                     + "    static String show() {\n"
                     + "        int x = 0;\n"
                     + "        return \"x=\" + x + \", T6Hide.x=\" + T6Hide.x;\n"
                     + "    }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) { return "{ System.out.println(T6Hide.show()); }"; }

    public Val expect(Val[] h) { return Val.ofString("x=0, T6Hide.x=1"); }
}
