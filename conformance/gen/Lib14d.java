// Lib14d — JLS chapter 14, batch four: §14.5 and §14.6. From p.271.
//
//   §14.5  "An empty statement does nothing. ... Execution of an empty statement always
//          completes normally."
//   §14.6  "Unlike C and C++, the Java language has no goto statement; identifier statement
//          labels are used with break (§14.13) or continue (§14.14) statements appearing
//          anywhere within the labeled statement."
//          "A statement labeled by an identifier must not appear anywhere within another
//          statement labeled by the same identifier, or a compile-time error will occur. TWO
//          STATEMENTS CAN BE LABELED BY THE SAME IDENTIFIER only if neither statement contains
//          the other."
//          "There is no restriction against using the same identifier as a label and as the
//          name of a ... local variable" -- which is §6.2's claim and is covered by
//          t6.name.label.separate.namespace.
public class Lib14d {

    private Lib14d() {}

    public static void install(Registry r) {
        r.register(new Sn14EmptyStatement());
        r.register(new Sn14LabelSiblings());
        r.register(new Sn14LabelReachesInner());
        r.register(new Sn14RejectNestedSameLabel());
    }
}

/** §14.5: the empty statement in the three places it turns up, each time doing nothing and
 *  completing normally. The `for` with an empty body is the one that matters most -- the loop
 *  must still run its update and condition, so the counter proves the statement was a no-op
 *  rather than the loop being skipped. */
class Sn14EmptyStatement implements Snippet {
    public String   id()        { return "t14.empty.statement"; }
    public String[] sections()  { return Strs.of("14.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " ;"                                        // on its own, between statements
             + " for (int i = 0; i < 3; i++) ;"            // as a loop body
             + " if (got == 0) ; else got = 99;"           // as a branch
             + " got += 5;"
             + " ;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(5 + h[0].asInt()); }
}

/** §14.6: "Two statements can be labeled by the same identifier ONLY IF NEITHER STATEMENT
 *  CONTAINS THE OTHER." Two sibling loops both labeled `same` are therefore legal, and each
 *  break binds to its own. An implementation keeping one global label table would reject this
 *  or bind the second break to the first label. */
class Sn14LabelSiblings implements Snippet {
    public String   id()        { return "t14.label.siblings.same.identifier"; }
    public String[] sections()  { return Strs.of("14.6"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " same: for (int i = 0; i < 5; i++) { got += 1; if (i == 1) break same; }"
             + " same: for (int j = 0; j < 5; j++) { got += 10; if (j == 2) break same; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // 2 iterations of the first, 3 of the second
    public Val expect(Val[] h) { return Val.ofInt(32 + h[0].asInt()); }
}

/** §14.6: labels "are used with break or continue statements appearing ANYWHERE WITHIN the
 *  labeled statement" -- so a break three levels deep still reaches the outermost label, which
 *  is what replaces goto. The counter distinguishes reaching the right label from reaching the
 *  nearest one. */
class Sn14LabelReachesInner implements Snippet {
    public String   id()        { return "t14.label.reaches.from.within"; }
    public String[] sections()  { return Strs.of("14.6"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " outer: for (int i = 0; i < 3; i++) {"
             + "   for (int j = 0; j < 3; j++) {"
             + "     { if (i + j == 2) break outer; got += 1; }"
             + "   }"
             + " }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // (0,0)(0,1) then (0,2) breaks: 2 increments.
    public Val expect(Val[] h) { return Val.ofInt(2 + h[0].asInt()); }
}

/** §14.6: "A statement labeled by an identifier must not appear anywhere within another
 *  statement labeled by the same identifier, or a compile-time error will occur."
 *
 *  Paired with t14.label.siblings.same.identifier, which uses the SAME identifier twice
 *  legally: the rule is about containment, not about reuse, so a compiler that simply banned a
 *  repeated label would satisfy this rejection and fail that case. */
class Sn14RejectNestedSameLabel implements Snippet {
    public String   id()        { return "t14.reject.nested.same.label"; }
    public String[] sections()  { return Strs.of("14.6"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14NestedLabel {\n"
             + "    static void f() {\n"
             + "        dup: for (int i = 0; i < 3; i++) {\n"
             + "            dup: for (int j = 0; j < 3; j++) {\n"
             + "                break dup;\n"
             + "            }\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("dup"); }
}
