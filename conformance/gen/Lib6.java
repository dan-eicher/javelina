// Lib6 — JLS chapter 6, Names.
//
// Chapter 6 is about which DECLARATION an identifier denotes, so almost every claim needs a
// declaration beside the case class rather than an expression inside it. That is what
// Declaring is for, and this library is its heaviest user.
//
// All of it is here, in three shapes, all of them templates through the one emitter:
//
//   single compilation unit, runs      -> a Snippet, optionally with Declaring companions
//   compile-time error                 -> a Snippet whose expects is Val.compileError
//   spans two packages                 -> a Snippet that also implements Units
//
// The rows still UNCOVERED are the ones not yet written, not ones the generator cannot express.
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
        r.register(new Sn6Inherited());
        r.register(new Sn6FieldMethodSameName());
        r.register(new Sn6ArrayClassIdentity());
        r.register(new Sn6ArrayLength());

        // §6.3, the illegal direction. The rule is stated as an exception rather than a blanket
        // ordering requirement -- "the declaration of a member needs to appear before it is used
        // ONLY when the use is in a field initialization expression" -- so this is an error
        // while the same reference from a CONSTRUCTOR is not, which is t6.scope.ctor.uses
        // .later.field above. Rejecting both would be as wrong as accepting both.
        r.register(new Sn6Reject("forward.reference.in.field", Strs.of("6.3"),
            "forward reference",
            "class T6ForwardRef {\n"
          + "    int i = j;\n"
          + "    int j = 1;\n"
          + "}"));

        // §6.4.2: "A class may have two or more fields with the same simple name if they are
        // declared in different interfaces and inherited. An attempt to refer to any of the
        // fields by its simple name results in a compile-time error." Inheriting both is legal;
        // only the SIMPLE NAME reference is the error, and Colors.BLACK would still resolve --
        // so a compiler rejecting the class declaration itself fails this for the wrong reason.
        r.register(new Sn6Reject("ambiguous.inherited.field", Strs.of("6.4.2"), "ambiguous",
            "interface T6Colors {\n"
          + "    int WHITE = 0, BLACK = 1;\n"
          + "}\n"
          + "\n"
          + "interface T6Separates {\n"
          + "    int CYAN = 0, MAGENTA = 1, YELLOW = 2, BLACK = 3;\n"
          + "}\n"
          + "\n"
          + "class T6AmbiguousField implements T6Colors, T6Separates {\n"
          + "    static int use() {\n"
          + "        return BLACK;\n"
          + "    }\n"
          + "}"));

        // §6.4.3: the same ambiguity in an INTERFACE extending two interfaces, and in a field
        // initializer rather than a method body. The spec states it separately, so it is pinned
        // separately -- one case passing does not demonstrate the other.
        r.register(new Sn6Reject("ambiguous.interface.field", Strs.of("6.4.3"), "ambiguous",
            "interface T6IColors {\n"
          + "    int WHITE = 0, BLACK = 1;\n"
          + "}\n"
          + "\n"
          + "interface T6ISeparates {\n"
          + "    int CYAN = 0, MAGENTA = 1, YELLOW = 2, BLACK = 3;\n"
          + "}\n"
          + "\n"
          + "interface T6ColorsAndSeparates extends T6IColors, T6ISeparates {\n"
          + "    int DEFAULT = BLACK;\n"
          + "}"));

        r.register(new Sn6ProtectedViaSuperclassExpr());
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

/** §6.4: "Members are either declared in the type, or inherited because they are accessible
 *  members of a superclass or superinterface which are neither hidden nor overridden." Both
 *  halves in one program: `base` is inherited and reachable by simple name in the subclass,
 *  while `tag()` is overridden, so the subclass's own definition is the member -- and calling
 *  it through a superclass-typed variable still reaches the override. */
class Sn6Inherited implements Snippet, Declaring {

    public String   id()          { return "t6.members.inherited"; }
    public String[] sections()    { return Strs.of("6.4"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T6Base {\n"
                     + "    int base = 100;\n"
                     + "    int tag() { return 1; }\n"
                     + "}",

                       "class T6Derived extends T6Base {\n"
                     + "    int tag() { return 2; }\n"
                     + "    int useInherited() { return base; }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T6Base b = new T6Derived();"
             + " T6Derived d = new T6Derived();"
             + " System.out.println(d.useInherited() + b.tag() + (" + h[0] + ")); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(102 + h[0].asInt()); }
}

/** §6.4.2: "There is no restriction against a field and a method of a class type having the
 *  same simple name." §6.5.1 is what makes it work -- the identifier before a `(` is a
 *  MethodName and elsewhere an ExpressionName -- so both uses are in one expression. */
class Sn6FieldMethodSameName implements Snippet, Declaring {

    public String   id()          { return "t6.members.field.method.same.name"; }
    public String[] sections()    { return Strs.of("6.4.2", "6.5.1"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T6Both {\n"
                     + "    int count = 40;\n"
                     + "    int count() { return 2; }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T6Both o = new T6Both();"
             + " System.out.println(o.count + o.count() + (" + h[0] + ")); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(42 + h[0].asInt()); }
}

/** §6.4.4, first line of the worked output: "all arrays whose components are of type int are
 *  instances of the same array type, which is int[]" -- so two int arrays of DIFFERENT lengths
 *  share one Class object. Prints the spec's "true". */
class Sn6ArrayClassIdentity implements Snippet {

    public String   id()          { return "t6.array.class.identity"; }
    public String[] sections()    { return Strs.of("6.4.4"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] ia = new int[3]; int[] ib = new int[6];"
             + " System.out.println(ia.getClass() == ib.getClass()); }";
    }

    public Val expect(Val[] h) { return Val.ofBoolean(true); }
}

/** §6.4.4, second line: "The field length, which is a constant (final) field of every array;
 *  its type is int and it contains the number of components of the array." The spec's own
 *  output text, "ia has length=3", verbatim. */
class Sn6ArrayLength implements Snippet {

    public String   id()          { return "t6.array.length"; }
    public String[] sections()    { return Strs.of("6.4.4"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String render(String[] h) {
        return "{ int[] ia = new int[3];"
             + " System.out.println(\"ia has length=\" + ia.length); }";
    }

    public Val expect(Val[] h) { return Val.ofString("ia has length=3"); }
}

/** A §6 rejection: a whole compilation unit the spec says must not compile. */
class Sn6Reject implements Snippet {

    private String   name;
    private String[] secs;
    private String   diag;
    private String   src;

    Sn6Reject(String name, String[] secs, String diag, String src) {
        this.name = name; this.secs = secs; this.diag = diag; this.src = src;
    }

    public String   id()        { return "t6.reject." + name; }
    public String[] sections()  { return secs; }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }
    public String   render(String[] h) { return src; }
    public Val      expect(Val[] h)    { return Val.compileError(diag); }
}

/** §6.6.2's worked example from §6.6.7, which needs TWO PACKAGES: let C be the class declaring
 *  the protected member and S the subclass in whose body the use occurs. "If the access is by a
 *  qualified name Q.Id, where Q is an ExpressionName, then the access is permitted if and only
 *  if the type of the expression Q is S or a subclass of S."
 *
 *  `p` has type Point, which is C, not S — so `p.x` is an error even though Point3d extends
 *  Point. §6.6.7: "while Point3d ... is a subclass of Point ..., it is not involved in the
 *  implementation of a Point (the type of the parameter p)."
 *
 *  delta3d is the CONTRAST and must keep compiling, since q has type Point3d, which is S. It is
 *  in the same unit on purpose: a compiler that "fixed" the error by banning protected access
 *  outright would satisfy the rejection and silently break the accompanying positive case. */
class Sn6ProtectedViaSuperclassExpr implements Snippet, Units {

    public String   id()        { return "t6.reject.protected.via.superclass.expr"; }
    public String[] sections()  { return Strs.of("6.6.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }
    public String   render(String[] h) { return ""; }      // the units carry the program
    public Val      expect(Val[] h)    {
        return Val.compileError("protected member 'x' is not accessible through");
    }

    public String[] unitPaths() {
        return Strs.of("points/Point.java", "threePoint/Point3d.java");
    }

    public String[] unitSources() {
        return Strs.of(
            "package points;\n"
          + "\n"
          + "public class Point {\n"
          + "    protected int x, y;\n"
          + "}\n",

            "package threePoint;\n"
          + "\n"
          + "import points.Point;\n"
          + "\n"
          + "public class Point3d extends Point {\n"
          + "    protected int z;\n"
          + "\n"
          + "    public void delta(Point p) {\n"
          + "        p.x += this.x;\n"
          + "        p.y += this.y;\n"
          + "    }\n"
          + "\n"
          + "    public void delta3d(Point3d q) {\n"
          + "        q.x += this.x;\n"
          + "        q.y += this.y;\n"
          + "        q.z += this.z;\n"
          + "    }\n"
          + "}\n");
    }
}
