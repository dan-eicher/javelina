// Lib14 — JLS chapter 14, Blocks and Statements.
//
// Starts with the chapter's COMPILE-TIME ERRORS, migrated out of conformance/reject/. §3 lists
// "a compile-time error" as one of the three things a template's `expects` may be, and §4 says
// such a template "still carries sections[] and expects and still goes through the same
// emitter" -- so a hand-written tree beside the generator was the subset-chosen-at-authoring-
// time §4 forbids, whatever the programs in it said.
//
// Chapter 14 is where destination-driven lowering lives, so most of it is about statements that
// RUN, and 21 of its rows are still carried by conformance/jls/Ch14.java. Those come next; this
// file is the chapter's landing place, not its finished library.
public class Lib14 {

    private Lib14() {}

    public static void install(Registry r) {
        // §14.9: "It is a compile-time error for two of the case constants ... to have the same
        // value" -- the switch's labels are a set, not a sequence.
        rej(r, "duplicate.case.value", Strs.of("14.9"), "duplicate case value",
            "class T14DuplicateCase {\n"
          + "    static void f(int x) {\n"
          + "        switch (x) {\n"
          + "            case 1: break;\n"
          + "            case 1: break;\n"
          + "        }\n"
          + "    }\n"
          + "}");

        // §14.8: the if condition is a boolean, and Java has no truthiness -- `if (1)` is an
        // error rather than a test against zero, which is the C habit this rules out.
        rej(r, "if.condition.not.boolean", Strs.of("14.8"), "if condition must be boolean",
            "class T14IfNotBoolean {\n"
          + "    static void f() {\n"
          + "        if (1) return;\n"
          + "    }\n"
          + "}");

        // §14.15: a `return` with no expression may appear only in a void method.
        rej(r, "return.without.value", Strs.of("14.15"), "missing return value",
            "class T14ReturnNoValue {\n"
          + "    static int f() {\n"
          + "        return;\n"
          + "    }\n"
          + "}");

        // §14.19 unreachable statements. Three shapes, because the rule is structural and each
        // reaches it by a different route: after an abrupt-completing statement inside a loop,
        // after one at the end of a method body, and the special case of `while (false)`, whose
        // body is unreachable BY THE RULE even though a constant-folding compiler could argue
        // the loop simply never runs.
        rej(r, "unreachable.after.break", Strs.of("14.19"), "unreachable statement",
            "class T14AfterBreak {\n"
          + "    static void f() {\n"
          + "        while (true) {\n"
          + "            break;\n"
          + "            int n = 1;\n"
          + "        }\n"
          + "    }\n"
          + "}");
        rej(r, "unreachable.after.return", Strs.of("14.19"), "unreachable statement",
            "class T14AfterReturn {\n"
          + "    static int f() {\n"
          + "        return 1;\n"
          + "        int n = 2;\n"
          + "    }\n"
          + "}");
        rej(r, "unreachable.while.false", Strs.of("14.19"), "unreachable statement",
            "class T14WhileFalse {\n"
          + "    static void f() {\n"
          + "        int n = 0;\n"
          + "        while (false) n = 1;\n"
          + "    }\n"
          + "}");
    }

    private static void rej(Registry r, String name, String[] secs, String diag, String src) {
        r.register(new Sn14Reject(name, secs, diag, src));
    }
}

/** A §14 rejection: a whole compilation unit the spec says must not compile. */
class Sn14Reject implements Snippet {

    private String   name;
    private String[] secs;
    private String   diag;
    private String   src;

    Sn14Reject(String name, String[] secs, String diag, String src) {
        this.name = name; this.secs = secs; this.diag = diag; this.src = src;
    }

    public String   id()        { return "t14.reject." + name; }
    public String[] sections()  { return secs; }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }
    public String   render(String[] h) { return src; }
    public Val      expect(Val[] h)    { return Val.compileError(diag); }
}
