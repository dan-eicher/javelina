// Lib14g — JLS chapter 14, batch seven: §14.12.1, §14.12.2, §14.12.3. From pp.281-282.
//
//   §14.12.1 ForInit. A statement-expression list is "evaluated in sequence from left to right;
//            their values, if any, are discarded", and on an abrupt one "any ForInit statement
//            expressions TO THE RIGHT of the one that completed abruptly are not evaluated".
//            A declaration's scope is "its own initializer and any further declarators in the
//            ForInit part, plus the Expression, ForUpdate, and contained Statement".
//   §14.12.2 ForUpdate likewise left to right with values discarded; and "if the Expression is
//            not present, then the only way a for statement can complete normally is by use of
//            a break statement".
//   §14.12.3 abrupt completion of the body. The one that separates a correct lowering from a
//            plausible one: an unlabeled continue performs TWO steps in sequence -- "first, if
//            the ForUpdate part is present, the expressions are evaluated ... second, another
//            for iteration step is performed". continue does NOT skip the update.
public class Lib14g {

    private Lib14g() {}

    public static void install(Registry r) {
        r.register(new Sn14ForInitOrder());
        r.register(new Sn14ForInitAbrupt());
        r.register(new Sn14ForInitDeclScope());
        r.register(new Sn14ForNoExpression());
        r.register(new Sn14ForContinueRunsUpdate());
    }
}

/** §14.12.1: a ForInit statement-expression list runs left to right and its values are
 *  discarded. Each expression appends a digit, so the order is in the printed string. */
class Sn14ForInitOrder implements Snippet, Declaring {
    public String   id()        { return "t14.for.init.order"; }
    public String[] sections()  { return Strs.of("14.12.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14For {\n"
                     + "    static StringBuffer log = new StringBuffer();\n"
                     + "    static int mark(char c) { log.append(c); return 0; }\n"
                     + "    static int boom(char c) { log.append(c); throw new RuntimeException(\"f\"); }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14For.log = new StringBuffer(); int i = 0;"
             + " for (T14For.mark('a'), T14For.mark('b'), i = 0; i < 2; i++) T14For.mark('x');"
             + " System.out.println(T14For.log.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("abxx"); }
}

/** §14.12.1: "any ForInit statement expressions TO THE RIGHT of the one that completed abruptly
 *  are not evaluated" -- so 'c' never appears, and neither does the body. */
class Sn14ForInitAbrupt implements Snippet, Declaring {
    public String   id()        { return "t14.for.init.abrupt.stops.right"; }
    public String[] sections()  { return Strs.of("14.12.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return new Sn14ForInitOrder().decls(); }

    public String render(String[] h) {
        return "{ T14For.log = new StringBuffer(); int i = 0;"
             + " try { for (T14For.mark('a'), T14For.boom('b'), T14For.mark('c'), i = 0;"
             + "            i < 2; i++) T14For.mark('x'); }"
             + " catch (RuntimeException e) { T14For.log.append('!'); }"
             + " System.out.println(T14For.log.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("ab!"); }
}

/** §14.12.1: a ForInit declaration's scope is "its own initializer and any further declarators
 *  in the ForInit part, plus the Expression, ForUpdate, and contained Statement" -- all four
 *  places, in one loop: `limit` sees `i`, and the condition, update and body all see both. */
class Sn14ForInitDeclScope implements Snippet {
    public String   id()        { return "t14.for.init.decl.scope"; }
    public String[] sections()  { return Strs.of("14.12.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " for (int i = 0, limit = i + 3; i < limit; i += 1) got += limit;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    // limit is 3 from the start, three iterations of += 3
    public Val expect(Val[] h) { return Val.ofInt(9 + h[0].asInt()); }
}

/** §14.12.2: "If the Expression is not present, then the only way a for statement can complete
 *  normally is by use of a break statement." `for (;;)` with no condition at all. */
class Sn14ForNoExpression implements Snippet {
    public String   id()        { return "t14.for.no.expression"; }
    public String[] sections()  { return Strs.of("14.12.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " for (;;) { got += 1; if (got == 4) break; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(4 + h[0].asInt()); }
}

/** §14.12.3, the case a plausible lowering gets wrong: an unlabeled continue performs TWO steps
 *  in sequence -- "first, if the ForUpdate part is present, the expressions are evaluated ...
 *  second, another for iteration step is performed."
 *
 *  So the update runs on the continuing iteration too. The second counter lives in ForUpdate,
 *  so it counts updates rather than bodies: 4 updates against 3 bodies is what says the
 *  continue went through the update and not straight to the test. A lowering that jumped to
 *  the condition would print 3|3 -- and, with the increment in ForUpdate, would not terminate
 *  at all if it skipped it entirely. */
class Sn14ForContinueRunsUpdate implements Snippet {
    public String   id()        { return "t14.for.continue.runs.update"; }
    public String[] sections()  { return Strs.of("14.12.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int bodies = 0, updates = 0;"
             + " for (int i = 0; i < 4; i++, updates++) {"
             + "   if (i == 1) continue;"
             + "   bodies += 1;"
             + " }"
             + " System.out.println(bodies + \"|\" + updates); }";
    }
    public Val expect(Val[] h) { return Val.ofString("3|4"); }
}
