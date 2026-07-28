// LibIntro — the snippet library for JLS chapters 1 (Introduction) and 2 (Grammars).
//
// crisp-tallying-chapters' coverage table dispositions both chapters N/A — ch1 "prose, no
// testable rule", ch2 "notation for the rest of the spec" — and then says of every such
// entry: "The N/A dispositions above are *chapter-level hypotheses*. They do not authorise
// skipping a section: each leaf section still gets its own row and its own reason."
//
// Read against the sections, the hypothesis does not hold for any of the three leaves:
//
//   §1.1 (p.5)  is not prose. It prints a complete program AND the output it produces —
//               "This program ... prints the arguments given to it on the command line" — so
//               a spec whose very first example does not run is one this target fails.
//   §2.2 (p.7)  "the input elements that are white space and comments are discarded" before
//               the syntactic grammar sees anything. That is a claim about MEANING: a comment
//               between two tokens cannot change what a program computes.
//   §2.4 (p.8)  the notation — "[x] denotes zero or one occurrences", "{x} denotes zero or
//               more" — is not decoration either. Applied to §14.12's ForStatement, whose
//               ForInit, Expression and ForUpdate are each bracketed, it is the claim that
//               all eight combinations of present/absent are legal Java. That is directly
//               compilable, and a parser that dropped one would still pass every test built
//               only from the forms people usually write.
//
// So all three are covered here rather than left N/A on a chapter-level guess.
public class LibIntro {

    private LibIntro() {}

    public static void install(Registry r) {
        example(r);
        lexicalGrammar(r);
        notation(r);
    }

    /* ── §1.1 the spec's own first program ──────────────────────────────────────────────
     * "class Test { public static void main(String[] args) { for (int i = 0; i <
     * args.length; i++) System.out.print(i == 0 ? args[i] : " " + args[i]);
     * System.out.println(); } }" — the arguments, separated by single spaces.
     *
     * The loop is reproduced exactly; the argument array is built in the snippet because a
     * generated case receives no argv, and printing is System.out's business (§20.18) rather
     * than this section's. */
    private static void example(Registry r) {
        // Statements, not expressions: the loop has to live somewhere, and a snippet renders
        // an expression or a statement — never a method the expression could call.
        e(r, "t1.ex.one",   "{ \"Hello,\" }",             "Hello,");
        e(r, "t1.ex.two",   "{ \"Hello,\", \"world.\" }", "Hello, world.");
        e(r, "t1.ex.none",  "{ }",                         "");
        e(r, "t1.ex.three", "{ \"a\", \"b\", \"c\" }",     "a b c");
    }

    /** §1.1's loop verbatim over `args`, accumulating instead of printing per element so the
     *  whole line is one composed expectation. `i == 0 ? args[i] : " " + args[i]` is the
     *  spec's own conditional, and it is what puts the separator BETWEEN and never leading. */
    private static void e(Registry r, String id, String init, String expected) {
        // `String[] args = { ... };` — the DECLARATION form (§10.6). Not
        // `new String[] { ... }`: §15.9's ArrayCreationExpression is
        // `new TypeName DimExprs Dims_opt`, and DimExprs is not optional, so an array
        // creation with an initializer and no dimension expression is 1.1 syntax.
        r.register(new SnILeaf(id, Strs.of("1.1", "10.6"), "void",
            "{ String[] args = " + init + "; String out = \"\";"
          + " for (int i = 0; i < args.length; i++)"
          + " out = out + (i == 0 ? args[i] : \" \" + args[i]);"
          + " System.out.println(out); }", Val.ofString(expected)));
    }

    /* ── §2.2 white space and comments are DISCARDED ────────────────────────────────────
     * Each of these computes a value that a reader would get wrong if the comment or the
     * white space were not discarded — so agreement is evidence, not coincidence. */
    private static void lexicalGrammar(Registry r) {
        r.register(new SnILeaf("t2.lex.comment.block", Strs.of("2.2", "3.7"), "int",
            "(1 /* a comment between the tokens */ + 2)", Val.ofInt(3)));
        r.register(new SnILeaf("t2.lex.comment.line", Strs.of("2.2", "3.7"), "int",
            "(10 + // a line comment ends at the newline\n 5)", Val.ofInt(15)));
        r.register(new SnILeaf("t2.lex.ws.tab", Strs.of("2.2", "3.6"), "int",
            "(7\t*\t6)", Val.ofInt(42)));
        // A comment SEPARATES tokens rather than joining them: `a+/**/+b` is `a + (+b)`, not
        // `a ++ b`, because §3.5's longest-match runs on the token stream after §2.2 has
        // discarded the comment — but the comment is still a boundary.
        r.register(new SnILeaf("t2.lex.comment.separates", Strs.of("2.2", "3.5"), "int",
            "(3 + /**/ + 4)", Val.ofInt(7)));
    }

    /* ── §2.4 the notation's optional parts are a compilable claim ──────────────────────
     * ForStatement (§14.12): `for ( [ForInit] ; [Expression] ; [ForUpdate] ) Statement`.
     * Three bracketed parts, so eight forms — every one legal. Each snippet below counts to
     * the same answer by a different one of the eight, so a form that failed to parse would
     * fail the build and a form that parsed but ran wrong would fail its expectation.
     *
     * Eight is derived from the notation (2^3), not quoted from §2.4, so it is deliberately
     * NOT a row in conformance/cardinality.tsv — that table takes counts the spec STATES. */
    private static void notation(Registry r) {
        //          init      cond      update
        f(r, "t2.for.111", "{ int n = 0; for (int i = 0; i < 3; i++) n = n + 1;"
                         + " System.out.println(n); }");
        f(r, "t2.for.110", "{ int n = 0; for (int i = 0; i < 3; ) { n = n + 1; i++; }"
                         + " System.out.println(n); }");
        f(r, "t2.for.101", "{ int n = 0; int i = 0; for (i = 0; ; i++) { if (n == 3) break;"
                         + " n = n + 1; } System.out.println(n); }");
        f(r, "t2.for.100", "{ int n = 0; for (int i = 0; ; ) { if (n == 3) break;"
                         + " n = n + 1; } System.out.println(n); }");
        f(r, "t2.for.011", "{ int n = 0; int i = 0; for (; i < 3; i++) n = n + 1;"
                         + " System.out.println(n); }");
        f(r, "t2.for.010", "{ int n = 0; int i = 0; for (; i < 3; ) { n = n + 1; i++; }"
                         + " System.out.println(n); }");
        f(r, "t2.for.001", "{ int n = 0; int i = 0; for (; ; i++) { if (n == 3) break;"
                         + " n = n + 1; } System.out.println(n); }");
        f(r, "t2.for.000", "{ int n = 0; for (;;) { if (n == 3) break; n = n + 1; }"
                         + " System.out.println(n); }");
    }

    /** One of §2.4's eight for-forms. All eight count to 3, so the form is the only variable. */
    private static void f(Registry r, String id, String text) {
        r.register(new SnILeaf(id, Strs.of("2.4", "14.12"), "void", text, Val.ofInt(3)));
    }
}

/** A zero-hole snippet for chapters 1-2 — "a degenerate stitch, not an exemption" (the plan,
 *  §4). These sections are claims about fixed programs, so there is nothing to parameterise. */
class SnILeaf implements Snippet {
    // Constructor-assigned, so not final (§8.3.1.2 wants the initializer in the declarator).
    private String   name, text, ty;
    private String[] secs;
    private Val      v;

    SnILeaf(String name, String[] secs, String ty, String text, Val v) {
        this.name = name; this.secs = secs; this.ty = ty; this.text = text; this.v = v;
    }
    public String   id()          { return name; }
    public String[] sections()    { return secs; }
    public String   type()        { return ty; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return text; }
    public Val      expect(Val[] h)    { return v; }
}
