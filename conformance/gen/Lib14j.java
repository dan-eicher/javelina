// Lib14j — JLS chapter 14, batch ten: §14.4 and §14.18, the chapter's last two IN rows.
// From pp.269-270 and pp.290-291.
//
//   §14.4 Statements. The section's substance is the dangling else: "The Java language, like C
//         and C++ and many languages before them, arbitrarily decree that an else clause belongs
//         to the INNERMOST if to which it might possibly belong. This rule is captured by the
//         following grammar" -- the StatementNoShortIf hierarchy, whose whole job is to make the
//         then-branch of an if-then-else unable to be a bare if-then, so the else cannot float
//         outward. The spec's own misleadingly formatted example is `if (door.isOpen()) if
//         (resident.isVisible()) resident.greet("Hello!"); else door.bell.ring();`, where "one
//         might surmise that the programmer intended the else clause to belong to the outer if
//         statement" and the language says otherwise.
//
//   §14.18 The try statement. The grammar has TWO productions --
//              TryStatement:  try Block Catches
//                             try Block Catches_opt Finally
//          so Catches is optional only when a Finally is present: try-finally with no catch at
//          all is a legal try statement.
//          The catch clause: "A catch clause must have exactly one parameter (which is called an
//          EXCEPTION PARAMETER); the declared type of the exception parameter must be the class
//          Throwable or a subclass of Throwable, or a compile-time error occurs. The SCOPE of the
//          parameter variable is the Block of the catch clause. An exception parameter must not
//          have the same name as a local variable or parameter IN WHOSE SCOPE IT IS DECLARED, or
//          a compile-time error occurs." And: "The name of the parameter may not be redeclared as
//          a local variable or exception parameter within the Block of the catch clause; that is,
//          HIDING the name of an exception parameter is not permitted."
//          Finally: "A finally clause ensures that the finally block is executed after the try
//          block and any catch block that might be executed, NO MATTER HOW control leaves the try
//          block or catch block."
public class Lib14j {

    private Lib14j() {}

    public static void install(Registry r) {
        r.register(new Sn14DanglingElseBindsInnermost());
        r.register(new Sn14DanglingElseThroughLoop());
        r.register(new Sn14TryFinallyWithoutCatch());
        r.register(new Sn14CatchParameterScopeIsItsBlock());

        r.register(new Sn14RejectCatchParameterNotThrowable());
        r.register(new Sn14RejectCatchParameterShadowsLocal());
        r.register(new Sn14RejectLocalHidesCatchParameter());
    }
}

/** §14.4: "an else clause belongs to the INNERMOST if to which it might possibly belong."
 *
 *  Written in the spec's own misleading formatting, so the source reads as though the else were
 *  the outer if's. Three truth combinations, two of which separate the two readings:
 *
 *      a  b   innermost (correct)   outermost (wrong)
 *      T  T   X                     X
 *      T  F   Y                     nothing        <- differs
 *      F  -   nothing               Y              <- differs
 *
 *  A single combination cannot tell them apart, which is exactly why a hand-written case for a
 *  parse rule tends to pass under either parse. */
class Sn14DanglingElseBindsInnermost implements Snippet {
    public String   id()        { return "t14.dangling.else.binds.innermost"; }
    public String[] sections()  { return Strs.of("14.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " for (int i = 0; i < 3; i++) {"
             + "   boolean a = i < 2;"        // T, T, F
             + "   boolean b = i < 1;"        // T, F, F
             + "   if (a)"
             + "       if (b)"
             + "           out.append('X');"
             + "   else out.append('Y');"     // formatted as the OUTER if's; it is the inner's
             + "   out.append('.');"
             + " }"
             + " System.out.println(out.toString()); }";
    }
    // T/T -> X, T/F -> Y, F/F -> neither. Outermost binding would print "X..Y.".
    public Val expect(Val[] h) { return Val.ofString("X.Y.."); }
}

/** §14.4's grammar, one level down: StatementNoShortIf has its OWN WhileStatementNoShortIf and
 *  ForStatementNoShortIf productions, so the "cannot be a short if" property propagates THROUGH
 *  an intervening loop rather than stopping at the if.
 *
 *  `if (a) while (c) if (b) X; else Y;` therefore binds the else to the innermost if, inside the
 *  loop -- so it can run on more than one iteration, which no binding to either outer construct
 *  could produce. */
class Sn14DanglingElseThroughLoop implements Snippet {
    public String   id()        { return "t14.dangling.else.through.loop"; }
    public String[] sections()  { return Strs.of("14.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " boolean a = true;"
             + " if (a)"
             + "     for (int j = 0; j < 4; j++)"
             + "         if (j % 2 == 0)"
             + "             out.append('X');"
             + "         else out.append('Y');"
             + " out.append('.');"
             + " System.out.println(out.toString()); }";
    }
    // the else is the inner if's, so it alternates across the loop's four iterations.
    public Val expect(Val[] h) { return Val.ofString("XYXY."); }
}

/** §14.18's second production, `try Block Catches_opt Finally`: Catches is optional when a
 *  Finally is present, so a try statement with NO catch clause at all is legal.
 *
 *  And "a finally clause ensures that the finally block is executed ... NO MATTER HOW control
 *  leaves the try block": three different exits from three catch-less try statements -- normal
 *  completion, a break, and a throw caught further out -- each running its finally. */
class Sn14TryFinallyWithoutCatch implements Snippet {
    public String   id()        { return "t14.try.finally.without.catch"; }
    public String[] sections()  { return Strs.of("14.18"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer out = new StringBuffer();"
             + " try { out.append('n'); } finally { out.append('1'); }"
             + " here: { try { out.append('b'); break here; } finally { out.append('2'); } }"
             + " try {"
             + "   try { out.append('t'); throw new RuntimeException(\"v\"); }"
             + "   finally { out.append('3'); }"
             + " } catch (RuntimeException e) { out.append('c'); }"
             + " System.out.println(out.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("n1b2t3c"); }
}

/** §14.18: "The scope of the parameter variable is the Block of the catch clause."
 *
 *  Two try statements in the same block, each with a catch parameter named `e`. If the scope
 *  leaked past its own Block, the second declaration would be a redeclaration of a local still in
 *  scope -- which §14.3.2 makes a compile-time error, so the case would not compile at all. That
 *  it runs is the assertion; the two different values prove each `e` is its own variable. */
class Sn14CatchParameterScopeIsItsBlock implements Snippet {
    public String   id()        { return "t14.catch.parameter.scope.is.its.block"; }
    public String[] sections()  { return Strs.of("14.18"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { throw new ArithmeticException(\"a\"); }"
             + " catch (ArithmeticException e) { got += 1; }"
             + " try { throw new NullPointerException(); }"
             + " catch (NullPointerException e) { got += 20; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(21 + h[0].asInt()); }
}

/** §14.18: "the declared type of the exception parameter must be the class Throwable or a
 *  subclass of Throwable, or a compile-time error occurs."
 *
 *  Distinct from §14.16's rule about the THROW's expression: this one is about the catch clause,
 *  and a compiler can enforce either without the other. */
class Sn14RejectCatchParameterNotThrowable implements Snippet {
    public String   id()        { return "t14.reject.catch.parameter.not.throwable"; }
    public String[] sections()  { return Strs.of("14.18"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14CatchString {\n"
             + "    static void f() {\n"
             + "        try {\n"
             + "            int n = 1;\n"
             + "        } catch (String s) {\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("catch type 'String' is not a subclass of Throwable"); }
}

/** §14.18: "An exception parameter must not have the same name as a local variable or parameter
 *  IN WHOSE SCOPE IT IS DECLARED, or a compile-time error occurs."
 *
 *  The local `e` is declared before the try, so the catch parameter is declared inside its
 *  scope. t14.catch.parameter.scope.is.its.block is the legal counterpart: two catch parameters
 *  named alike are fine precisely because neither is in the other's scope. */
class Sn14RejectCatchParameterShadowsLocal implements Snippet {
    public String   id()        { return "t14.reject.catch.parameter.shadows.local"; }
    public String[] sections()  { return Strs.of("14.18"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14CatchShadows {\n"
             + "    static void f() {\n"
             + "        int e = 1;\n"
             + "        try {\n"
             + "            throw new RuntimeException(\"v\");\n"
             + "        } catch (RuntimeException e) {\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("redeclaration of 'e'"); }
}

/** §14.18: "The name of the parameter may not be redeclared as a local variable or exception
 *  parameter within the Block of the catch clause; that is, HIDING the name of an exception
 *  parameter is not permitted."
 *
 *  The other direction from the case above: there the local came first, here the parameter does.
 *  Both are errors, and a compiler that checked only the enclosing locals when declaring the
 *  parameter would accept this one. */
class Sn14RejectLocalHidesCatchParameter implements Snippet {
    public String   id()        { return "t14.reject.local.hides.catch.parameter"; }
    public String[] sections()  { return Strs.of("14.18"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T14HidesParameter {\n"
             + "    static void f() {\n"
             + "        try {\n"
             + "            throw new RuntimeException(\"v\");\n"
             + "        } catch (RuntimeException e) {\n"
             + "            int e = 2;\n"
             + "        }\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("redeclaration of 'e'"); }
}
