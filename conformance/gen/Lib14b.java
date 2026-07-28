// Lib14b — JLS chapter 14, batch two: §14.2 and §14.3.1. Transcribed from pp.265-267.
//
//   §14.2   "A block is executed by executing each of the local variable declaration statements
//           and other statements in order from first to last (left to right). If all of these
//           block statements complete normally, then the block completes normally. If any of
//           these block statements complete abruptly for any reason, then the block completes
//           abruptly for the same reason."
//   §14.3.1 "The type of the variable is denoted by the Type that appears at the start of the
//           local variable declaration, FOLLOWED BY any bracket pairs that follow the
//           Identifier in the declarator." With two worked equivalences:
//               int a, b[], c[][];            ==  int a; int[] b; int[][] c;
//               float[][] f[][], g[][][], h[];  ==  float[][][][] f;
//                                                  float[][][][][] g;
//                                                  float[][][] h;
//           and "a local variable declaration can also appear in the header of a for statement
//           (§14.12). In this case it is executed in the same manner as if it were part of a
//           local variable declaration statement."
public class Lib14b {

    private Lib14b() {}

    public static void install(Registry r) {
        r.register(new Sn14BlockOrder());
        r.register(new Sn14BlockAbrupt());
        r.register(new Sn14DeclaratorDims());
        r.register(new Sn14DeclaratorDimsFloat());
        r.register(new Sn14DeclIntermixedAndForHeader());
    }
}

/** §14.2: "in order from first to last (left to right)". Each statement appends a distinct
 *  digit, so the printed string IS the execution order -- a reordering shows up as a different
 *  string rather than the same total. Declarations are interleaved with statements because
 *  §14.3 says they "may be intermixed freely" and the ordering claim covers both kinds. */
class Sn14BlockOrder implements Snippet {
    public String   id()        { return "t14.block.order"; }
    public String[] sections()  { return Strs.of("14.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ StringBuffer b = new StringBuffer();"
             + " b.append('1');"
             + " int x = 2; b.append((char)('0' + x));"
             + " { b.append('3'); }"
             + " int y = 4; b.append((char)('0' + y));"
             + " b.append('5');"
             + " System.out.println(b.toString()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("12345"); }
}

/** §14.2: "If any of these block statements complete abruptly for any reason, then the block
 *  completes abruptly FOR THE SAME REASON." The inner block is exited by a labeled break, and
 *  the reason propagates out through it -- so the statement after the inner block does not run
 *  and neither does the one after the outer. */
class Sn14BlockAbrupt implements Snippet {
    public String   id()        { return "t14.block.abrupt.same.reason"; }
    public String[] sections()  { return Strs.of("14.2", "14.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " here: { got += 1; { got += 2; if (got > 0) break here; got += 100; }"
             + "         got += 200; }"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3 + h[0].asInt()); }
}

/** §14.3.1's first equivalence: `int a, b[], c[][];` is `int a; int[] b; int[][] c;`.
 *
 *  Observed through the run-time type signature (§20.1.1), which encodes the depth exactly:
 *  "[I" is an int array, "[[I" an array of them. A compiler that distributed the declarator's
 *  brackets to every variable, or dropped them, still COMPILES -- the signature is what tells
 *  the three apart. */
class Sn14DeclaratorDims implements Snippet {
    public String   id()        { return "t14.declarator.dims.int"; }
    public String[] sections()  { return Strs.of("14.3.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int a, b[], c[][];"
             + " a = 1; b = new int[1]; c = new int[1][1];"
             + " System.out.println(a + \"|\" + b.getClass().getName()"
             + " + \"|\" + c.getClass().getName()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("1|[I|[[I"); }
}

/** §14.3.1's second, which the spec labels "Yechh!" and prints the expansion of anyway:
 *
 *      float[][] f[][], g[][][], h[];
 *
 *  is float[][][][] f (2+2), float[][][][][] g (2+3), float[][][] h (2+1). The type's brackets
 *  are added to EVERY declarator and the declarator's own brackets to that one alone, so the
 *  three depths differ from each other and from the type. */
class Sn14DeclaratorDimsFloat implements Snippet {
    public String   id()        { return "t14.declarator.dims.float"; }
    public String[] sections()  { return Strs.of("14.3.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ float[][] f[][], g[][][], hh[];"
             + " f = new float[1][1][1][1];"
             + " g = new float[1][1][1][1][1];"
             + " hh = new float[1][1][1];"
             + " System.out.println(f.getClass().getName() + \"|\""
             + " + g.getClass().getName() + \"|\" + hh.getClass().getName()); }";
    }
    public Val expect(Val[] h) { return Val.ofString("[[[[F|[[[[[F|[[[F"); }
}

/** §14.3: "Local variable declaration statements may be intermixed freely with other kinds of
 *  statements in the block." And §14.3.1: "a local variable declaration can also appear in the
 *  header of a for statement (§14.12). In this case it is EXECUTED IN THE SAME MANNER as if it
 *  were part of a local variable declaration statement" — including the multi-declarator form,
 *  where a later declarator sees an earlier one. */
class Sn14DeclIntermixedAndForHeader implements Snippet {
    public String   id()        { return "t14.decl.intermixed.and.for.header"; }
    public String[] sections()  { return Strs.of("14.3.1", "14.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " got += 1;"
             + " int first = 2;"                        // a declaration after a statement
             + " got += first;"
             + " for (int i = 0, limit = i + 3; i < limit; i++) got += 10;"
             + " int last = 4;"                         // ...and another after that
             + " System.out.println(got + last + (" + h[0] + ")); }";
    }
    // 1 + 2 + 30 + 4
    public Val expect(Val[] h) { return Val.ofInt(37 + h[0].asInt()); }
}
