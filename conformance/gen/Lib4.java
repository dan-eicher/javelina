// Lib4 — the snippet library for JLS chapter 4, Types, Values, and Variables.
//
// Every rule below is transcribed from java-langspec-1.0.pdf, not recalled. The sections it
// carries, and the sentence each snippet is the witness for:
//
//   §4.2    (p.31)  "Primitive values do not share state with other primitive values. ...
//                    The value of a variable of primitive type can be changed only by
//                    assignment operations on that variable."
//   §4.2.1  (p.31)  the five inclusive ranges, byte -128..127, short -32768..32767,
//                   int -2147483648..2147483647, long -2^63..2^63-1, char 0..65535
//   §4.2.2  (p.31)  32-bit unless an operand is long; "The built-in integer operators do not
//                   indicate overflow or underflow in any way"; / and % throw
//                   ArithmeticException "if the right-hand operand is zero"
//   §4.2.3  (p.33)  the IEEE 754 value set: signed zeros, signed infinities, NaN;
//                   "0.0==-0.0 is true"; "1.0/0.0 has the value positive infinity, while the
//                   value of 1.0/-0.0 is negative infinity"; "NaN is unordered";
//                   "x!=x is true if and only if x is NaN"
//   §4.2.4  (p.34)  double if either operand is double, else float; round to nearest;
//                   "Java floating-point operators produce no exceptions"; overflow ->
//                   signed infinity, underflow -> signed zero, gradual underflow, no
//                   flush-to-zero; "round toward zero when converting ... to an integer"
//   §4.2.5  (p.36)  boolean has exactly two values; ! ; == and != ; ?: ; string conversion
//   §4.5.1  (p.44)  "A variable of a primitive type always holds a value of that exact
//                   primitive type."
//   §4.5.4  (p.46)  the default value of every type, for a class variable, an instance
//                   variable, or an ARRAY COMPONENT — the one of the three an expression can
//                   reach without declaring a class.
//
// TWO THINGS THIS LIBRARY WILL NOT DO.
//
// It does not learn an answer by running anything: every expect() folds up from its holes
// and from the cited sentence. Where the book prints the answer -- 1000000*1000000 is
// -727379968, l*l is 1000000000000, the four gradual-underflow steps, (int)12345.6 is 12345
// -- the printed answer is what is written down, character for character.
//
// And it does not classify a floating-point value with the operators it is the oracle for.
// Asking `d != d` to find out whether d is NaN would make the NaN snippets tautologies: a
// compiler that got != wrong would compose an expectation that matched its own mistake. So
// F4 classifies by the IEEE 754 BIT PATTERN (Double.doubleToLongBits, §20.10.17), and orders
// values with an integer key, and F4.selfCheck() gates those bit patterns against the ones
// §4.2.3 names before a single snippet is registered.
public class Lib4 {

    private Lib4() {}

    // Registration is split one method per group, and it has to be: at -O0 javelinac gives
    // every nested temporary its own body-local slot and never reuses one, so a single method
    // holding all eighty-odd `r.register(new Sn4Leaf(id, Strs.of(..), ty, text, Val.of..()))`
    // statements needs more than the 1024 locals a frame may have (wasm/src/jav_frame.h
    // MAX_LOCALS, enforced in jav_runtime.c). The module still VALIDATES; the call to it traps
    // before its first statement runs. Thirty-two such statements are fine, forty-eight are
    // not, and -O coalesces the slots and takes either. Groups here are at most twelve.
    public static void install(Registry r) {
        F4.selfCheck();
        kindsAndPositions(r);
        ranges(r);
        overflow(r);
        integerOps(r);
        promotion(r);
        fpValues(r);
        fpLimits(r);
        fpOps(r);
        booleans(r);
        defaults(r);
        variables(r);
    }

    /* ── §4.5.3 the seven kinds of variable, and §4.4 where a type may be used ──────────
     *
     * §4.5.3 (p.45) names seven: "A class variable is a field declared using the keyword
     * static"; "An instance variable is a field declared without the keyword static"; "Array
     * components are unnamed variables"; "Method parameters name argument values passed to a
     * method"; "Constructor parameters name argument values passed to a constructor"; "An
     * exception-handler parameter is created each time an exception is caught by a catch
     * clause"; "Local variables are declared by local variable declaration statements".
     *
     * Four of the seven are statements a snippet can already render. Three — class variable,
     * instance variable, constructor parameter — are DECLARATIONS, which is why this is the
     * first user of Declaring. Covering §4.5.3 on the four that happen to be reachable would
     * mark the section covered while three of its seven kinds went untested.
     *
     * §4.4 (pp.42-43) is the same shape and gets ELEVEN snippets, one per position, because
     * the section is a closed bulleted list — seven places a type is used in DECLARATIONS
     * ("Imported types (§7.5)", "Fields", "Method parameters (§8.4.1)", "Method results
     * (§8.4)", "Constructor parameters (§8.6.1)", "Local variables (§14.3, §14.12)",
     * "Exception handler parameters (§14.18)") and, "in expressions of the following kinds",
     * four more ("Class instance creations (§15.8)", "Array creations (§15.9)", "Casts
     * (§15.15)", "The instanceof operator (§15.19.2)").
     *
     * One snippet per position rather than one snippet using several, so a position that stops
     * compiling is a count that drops rather than a line inside a program that still passes.
     * They are NOT a cardinality.tsv row: eleven is obtained by counting bullets, and that file
     * takes only counts the spec's own text names — the same reason §2.4's eight `for` forms
     * are not a row either.
     *
     * All eleven share one pair of declarations, deduplicated by Emit, so each names the SAME
     * type in a different place — which is what the section is about. `T4Node` is deliberately
     * used at every position it can occupy; where the position admits only a primitive or
     * another type (an interface constant, the imported name) the bullet's own example does
     * the same. */
    private static void kindsAndPositions(Registry r) {
        r.register(new Sn4Kinds());

        // the seven declaration positions
        r.register(new Sn4Pos("import", "java.util.Vector",
                              "{ Vector v = new Vector(); v.addElement(\"q\");"
                            + " System.out.println(v.size()); }", Val.ofInt(1)));
        r.register(new Sn4Pos("field",
                              "{ T4Node.shared = new T4Node(2L);"
                            + " T4Node.shared.tag = T4Node.shared.tag + T4Const.LIMIT;"
                            + " System.out.println(T4Node.shared.tag); }", Val.ofLong(9L)));
        r.register(new Sn4Pos("methodparam",
                              "{ System.out.println(T4Node.tagOf(new T4Node(11L))); }",
                              Val.ofLong(11L)));
        r.register(new Sn4Pos("methodresult",
                              "{ T4Node n = T4Node.make(12L); System.out.println(n.tag); }",
                              Val.ofLong(12L)));
        r.register(new Sn4Pos("ctorparam",
                              "{ T4Node a = new T4Node(20L); T4Node b = new T4Node(a);"
                            + " System.out.println(b.tag); }", Val.ofLong(21L)));
        r.register(new Sn4Pos("local",
                              "{ T4Node n = new T4Node(13L); System.out.println(n.tag); }",
                              Val.ofLong(13L)));
        r.register(new Sn4Pos("handler",
                              "{ long got = 0L;"
                            + " try { throw new T4Trouble(); }"
                            + " catch (T4Trouble e) { got = (e == null) ? 0L : 14L; }"
                            + " System.out.println(got); }", Val.ofLong(14L)));

        // the four expression positions
        r.register(new Sn4Pos("new",
                              "{ System.out.println(new T4Node(15L).tag); }", Val.ofLong(15L)));
        r.register(new Sn4Pos("arraynew",
                              "{ T4Node[] a = new T4Node[3]; a[2] = new T4Node(16L);"
                            + " System.out.println(a[2].tag + a.length); }", Val.ofLong(19L)));
        r.register(new Sn4Pos("cast",
                              "{ Object o = new T4Node(17L); T4Node n = (T4Node) o;"
                            + " System.out.println(n.tag); }", Val.ofLong(17L)));
        // both directions, so a `instanceof` that answered a constant would fail one of them.
        r.register(new Sn4Pos("instanceof",
                              "{ Object o = new T4Node(18L); Object s = \"text\";"
                            + " System.out.println((o instanceof T4Node)"
                            + " && !(s instanceof T4Node)); }", Val.ofBoolean(true)));
    }

    private static void ranges(Registry r) {
        // ── §4.2.1 the exact inclusive ranges ───────────────────────────────────────────
        // "The values of the integral types are integers in the following ranges."
        r.register(new Sn4Leaf("t4.rng.byte.min",  Strs.of("4.2.1"), "byte",
                               "((byte)(-128))",   Val.ofByte((byte) -128)));
        r.register(new Sn4Leaf("t4.rng.byte.max",  Strs.of("4.2.1"), "byte",
                               "((byte)(127))",    Val.ofByte((byte) 127)));
        r.register(new Sn4Leaf("t4.rng.short.min", Strs.of("4.2.1"), "short",
                               "((short)(-32768))", Val.ofShort((short) -32768)));
        r.register(new Sn4Leaf("t4.rng.short.max", Strs.of("4.2.1"), "short",
                               "((short)(32767))",  Val.ofShort((short) 32767)));
        r.register(new Sn4Leaf("t4.rng.int.min",   Strs.of("4.2.1"), "int",
                               "(-2147483648)",     Val.ofInt(-2147483648)));
        r.register(new Sn4Leaf("t4.rng.int.max",   Strs.of("4.2.1"), "int",
                               "(2147483647)",      Val.ofInt(2147483647)));
        r.register(new Sn4Leaf("t4.rng.long.min",  Strs.of("4.2.1"), "long",
                               "(-9223372036854775808L)", Val.ofLong(Long.MIN_VALUE)));
        r.register(new Sn4Leaf("t4.rng.long.max",  Strs.of("4.2.1"), "long",
                               "(9223372036854775807L)",  Val.ofLong(Long.MAX_VALUE)));
        // §4.2.1 gives char's bounds as the two unicode escapes u0000 and uffff -- "that is,
        // from 0 to 65535" -- so the escape is what is emitted, both because it is the form
        // the spec uses and because Emit's ASCII gate would reject the raw characters (§3.3
        // translates the escape before the compiler ever lexes the literal).
        r.register(new Sn4Leaf("t4.rng.char.min",  Strs.of("4.2.1", "3.3"), "char",
                               "('\\u0000')", Val.ofChar((char) 0)));
        r.register(new Sn4Leaf("t4.rng.char.max",  Strs.of("4.2.1", "3.3"), "char",
                               "('\\uffff')", Val.ofChar((char) 0xffff)));

    }

    private static void overflow(Registry r) {
        // ── §4.2.2 overflow is silent, and wraps ────────────────────────────────────────
        // Answers stated from §4.2.1's range plus "the result is the low-order bits of the
        // mathematical sum ... in some sufficiently large two's-complement format"
        // (§15.17.2), never by performing the operation under test at its own width.
        r.register(new Sn4Leaf("t4.ovf.int.max.plus1",  Strs.of("4.2.2", "4.2.1", "15.17.2"),
                               "int",  "((2147483647) + (1))",  Val.ofInt(-2147483648)));
        r.register(new Sn4Leaf("t4.ovf.int.min.minus1", Strs.of("4.2.2", "4.2.1", "15.17.2"),
                               "int",  "((-2147483648) - (1))", Val.ofInt(2147483647)));
        r.register(new Sn4Leaf("t4.ovf.long.max.plus1", Strs.of("4.2.2", "4.2.1", "15.17.2"),
                               "long", "((9223372036854775807L) + (1L))",
                               Val.ofLong(Long.MIN_VALUE)));
        r.register(new Sn4Leaf("t4.ovf.long.min.minus1", Strs.of("4.2.2", "4.2.1", "15.17.2"),
                               "long", "((-9223372036854775808L) - (1L))",
                               Val.ofLong(Long.MAX_VALUE)));
        // §15.14.4: "negation of the maximum negative int or long results in that same
        // maximum negative number. Overflow occurs in this case, but no exception is thrown."
        r.register(new Sn4Leaf("t4.ovf.int.min.neg",  Strs.of("4.2.2", "15.14.4"), "int",
                               "(-(-2147483648))",  Val.ofInt(-2147483648)));
        r.register(new Sn4Leaf("t4.ovf.long.min.neg", Strs.of("4.2.2", "15.14.4"), "long",
                               "(-(-9223372036854775808L))", Val.ofLong(Long.MIN_VALUE)));
        // §4.2.2's own worked example, and its own printed output: "The first multiplication
        // is performed in 32-bit precision, whereas the second multiplication is a long
        // multiplication. The value -727379968 is the decimal value of the low 32 bits of the
        // mathematical result, 1000000000000, which is a value too large for type int."
        r.register(new Sn4Leaf("t4.jls.mul.32bit", Strs.of("4.2.2", "15.16.1"), "int",
                               "((1000000) * (1000000))", Val.ofInt(-727379968)));
        r.register(new Sn4Leaf("t4.jls.mul.64bit", Strs.of("4.2.2", "15.16.1"), "long",
                               "(((long)1000000) * ((long)1000000))",
                               Val.ofLong(1000000000000L)));

    }

    private static void integerOps(Registry r) {
        // ── §4.2.2 the integer operators, composed ──────────────────────────────────────
        r.register(new Sn4AddInt());
        r.register(new Sn4SubInt());
        r.register(new Sn4RemInt());
        r.register(new Sn4DivLong());
        r.register(new Sn4RemLong());
        r.register(new Sn4NotInt());
        r.register(new Sn4NotLong());
        r.register(new Sn4NegInt());
        r.register(new Sn4NegLong());
        r.register(new Sn4PlusInt());

    }

    private static void promotion(Registry r) {
        // ── §4.2.2 the promotion rule ───────────────────────────────────────────────────
        r.register(new Sn4AddByte());
        r.register(new Sn4AddChar());
        r.register(new Sn4NegByte());
        r.register(new Sn4MulIntLong());

    }

    private static void fpValues(Registry r) {
        // ── §4.2.3 the IEEE 754 value set ───────────────────────────────────────────────
        r.register(new Sn4Leaf("t4.fp.d.poszero", Strs.of("4.2.3"), "double",
                               "(0.0)",    Val.ofDouble(0.0)));
        r.register(new Sn4Leaf("t4.fp.d.negzero", Strs.of("4.2.3", "15.14.4"), "double",
                               "(-0.0)",   Val.ofDouble(F4.D_NZERO)));
        // "1.0/0.0 has the value positive infinity, while the value of 1.0/-0.0 is negative
        // infinity" -- the sentence, verbatim, as two leaves.
        r.register(new Sn4Leaf("t4.fp.d.posinf",  Strs.of("4.2.3", "15.16.2"), "double",
                               "(1.0/0.0)",  Val.ofDouble(F4.D_PINF)));
        r.register(new Sn4Leaf("t4.fp.d.neginf",  Strs.of("4.2.3", "15.16.2"), "double",
                               "(1.0/-0.0)", Val.ofDouble(F4.D_NINF)));
        // §4.2.4's example prints "0.0/0.0 is Not-a-Number: NaN"; §15.16.2: "Division of a
        // zero by a zero results in NaN."
        r.register(new Sn4Leaf("t4.fp.d.nan",     Strs.of("4.2.3", "4.2.4", "15.16.2"),
                               "double", "(0.0/0.0)", Val.ofDouble(F4.D_NAN)));
        r.register(new Sn4Leaf("t4.fp.f.negzero", Strs.of("4.2.3", "15.14.4"), "float",
                               "(-0.0f)",     Val.ofFloat(F4.F_NZERO)));
        r.register(new Sn4Leaf("t4.fp.f.posinf",  Strs.of("4.2.3", "15.16.2"), "float",
                               "(1.0f/0.0f)", Val.ofFloat(F4.F_PINF)));
        r.register(new Sn4Leaf("t4.fp.f.nan",     Strs.of("4.2.3", "15.16.2"), "float",
                               "(0.0f/0.0f)", Val.ofFloat(F4.F_NAN)));

    }

    private static void fpLimits(Registry r) {
        // ── §4.2.4 overflow, gradual underflow, and the cast rounding ───────────────────
        // "An operation that overflows produces a signed infinity" -- §4.2.4's example
        // prints "overflow produces infinity: 1.0e+308*10==Infinity".
        r.register(new Sn4Leaf("t4.fp.d.overflow", Strs.of("4.2.4", "15.16.1"), "double",
                               "((1.0e308) * (10.0))", Val.ofDouble(F4.D_PINF)));
        r.register(new Sn4Leaf("t4.fp.f.overflow", Strs.of("4.2.4", "15.16.1"), "float",
                               "((1.0e38f) * (10.0f))", Val.ofFloat(F4.F_PINF)));
        // "Java requires support of IEEE 754 denormalized floating-point numbers and gradual
        // underflow ... Floating-point operations in Java do not 'flush to zero' if the
        // calculated result is a denormalized number." §4.2.4's example divides
        // 3.141592653589793E-305 by 100000 four times and prints
        //     3.1415926535898E-310  3.141592653E-315  3.142E-320  0.0
        // Each step is a leaf whose expectation is the book's printed decimal, parsed back.
        r.register(new Sn4Leaf("t4.fp.d.underflow.1", Strs.of("4.2.4"), "double",
                               "((3.141592653589793E-305) / (100000.0))",
                               Val.ofDouble(3.1415926535898E-310)));
        r.register(new Sn4Leaf("t4.fp.d.underflow.2", Strs.of("4.2.4"), "double",
                               "(((3.141592653589793E-305) / (100000.0)) / (100000.0))",
                               Val.ofDouble(3.141592653E-315)));
        r.register(new Sn4Leaf("t4.fp.d.underflow.3", Strs.of("4.2.4"), "double",
                               "((((3.141592653589793E-305) / (100000.0)) / (100000.0)) / (100000.0))",
                               Val.ofDouble(3.142E-320)));
        // "an operation that underflows produces a signed zero" -- the fifth value the
        // example prints is 0.0, not a denormal and not a trap.
        r.register(new Sn4Leaf("t4.fp.d.underflow.4", Strs.of("4.2.4"), "double",
                               "(((((3.141592653589793E-305) / (100000.0)) / (100000.0)) / (100000.0)) / (100000.0))",
                               Val.ofDouble(0.0)));
        // "Java uses round toward zero when converting a floating value to an integer
        // (§5.1.3), which acts, in this case, as though the number were truncated" --
        // §4.2.4's example prints "cast to int rounds toward 0: 12345 -12345".
        r.register(new Sn4Leaf("t4.fp.trunc.pos", Strs.of("4.2.4", "5.1.3"), "int",
                               "((int)(12345.6))",  Val.ofInt(12345)));
        r.register(new Sn4Leaf("t4.fp.trunc.neg", Strs.of("4.2.4", "5.1.3"), "int",
                               "((int)(-12345.6))", Val.ofInt(-12345)));

    }

    private static void fpOps(Registry r) {
        // ── §4.2.3 / §4.2.4 the floating-point operators, composed ──────────────────────
        r.register(new Sn4EqDouble());
        r.register(new Sn4NeDouble());
        r.register(new Sn4LtDouble());
        r.register(new Sn4GeDouble());
        r.register(new Sn4SelfNeDouble());
        r.register(new Sn4SelfNeFloat());
        r.register(new Sn4NegDouble());
        r.register(new Sn4AddDouble());
        r.register(new Sn4MulDouble());
        r.register(new Sn4DivDouble());
        r.register(new Sn4DivFloat());

    }

    private static void booleans(Registry r) {
        // ── §4.2.5 the boolean type ─────────────────────────────────────────────────────
        r.register(new Sn4NotBool());
        r.register(new Sn4EqBool());
        r.register(new Sn4StrBool());
        r.register(new Sn4CondInt());

    }

    private static void defaults(Registry r) {
        // ── §4.5.4 initial values ───────────────────────────────────────────────────────
        // "Each class variable, instance variable, or array component is initialized with a
        // default value when it is created (§15.8, §15.9, §20.3.6)." An array component is
        // the kind an expression can create and read without declaring a class, so
        // `(new T[1])[0]` is the witness -- parenthesised, because `new T[1][0]` is a
        // TWO-dimensional array creation (§15.9), not a creation followed by an index.
        r.register(new Sn4Leaf("t4.dflt.byte",    Strs.of("4.5.4"), "byte",
                               "((new byte[1])[0])",    Val.ofByte((byte) 0)));
        r.register(new Sn4Leaf("t4.dflt.short",   Strs.of("4.5.4"), "short",
                               "((new short[1])[0])",   Val.ofShort((short) 0)));
        r.register(new Sn4Leaf("t4.dflt.int",     Strs.of("4.5.4"), "int",
                               "((new int[1])[0])",     Val.ofInt(0)));
        r.register(new Sn4Leaf("t4.dflt.long",    Strs.of("4.5.4"), "long",
                               "((new long[1])[0])",    Val.ofLong(0L)));
        r.register(new Sn4Leaf("t4.dflt.float",   Strs.of("4.5.4"), "float",
                               "((new float[1])[0])",   Val.ofFloat(0.0f)));
        r.register(new Sn4Leaf("t4.dflt.double",  Strs.of("4.5.4"), "double",
                               "((new double[1])[0])",  Val.ofDouble(0.0)));
        r.register(new Sn4Leaf("t4.dflt.char",    Strs.of("4.5.4"), "char",
                               "((new char[1])[0])",    Val.ofChar((char) 0)));
        r.register(new Sn4Leaf("t4.dflt.boolean", Strs.of("4.5.4"), "boolean",
                               "((new boolean[1])[0])", Val.ofBoolean(false)));
        r.register(new Sn4Leaf("t4.dflt.string",  Strs.of("4.5.4"), "String",
                               "((new String[1])[0])",  Val.ofNull()));
        r.register(new Sn4Leaf("t4.dflt.object",  Strs.of("4.5.4"), "Object",
                               "((new Object[1])[0])",  Val.ofNull()));
        // §4.5.4 says POSITIVE zero, not merely zero -- "For type float, the default value is
        // positive zero, that is, 0.0f" -- and the two zeros print the same. §4.2.3 is what
        // tells them apart: 1.0/+0.0 is positive infinity, 1.0/-0.0 is negative infinity.
        r.register(new Sn4Leaf("t4.dflt.double.ispositive", Strs.of("4.5.4", "4.2.3"), "double",
                               "((1.0) / ((new double[1])[0]))",   Val.ofDouble(F4.D_PINF)));
        r.register(new Sn4Leaf("t4.dflt.float.ispositive",  Strs.of("4.5.4", "4.2.3"), "float",
                               "((1.0f) / ((new float[1])[0]))",   Val.ofFloat(F4.F_PINF)));

    }

    private static void variables(Registry r) {
        // ── §4.2 / §4.5.1 a primitive variable holds its own value, of its own type ─────
        r.register(new Sn4VarNoShare());
        r.register(new Sn4VarCharExact());

        // ── §4.5 (p.44) "A variable always contains a value that is assignment compatible
        // with its type." Witnessed by storing through a WIDER declared type and reading it
        // back: what comes out is the declared type's value, not the expression's — a long
        // variable given an int holds a long, and a double given an int holds a double, so
        // the division below is floating and not integer.
        r.register(new Sn4Leaf("t4.var.assignCompat.long", Strs.of("4.5", "5.1.2"), "void",
            "{ int i = 7; long v = i; System.out.println(v / 2); }", Val.ofLong(3L)));
        r.register(new Sn4Leaf("t4.var.assignCompat.double", Strs.of("4.5", "5.1.2"), "void",
            "{ int i = 7; double v = i; System.out.println(v / 2); }", Val.ofDouble(3.5)));
        // ...and an ARRAY COMPONENT is a variable too (§4.5's list), with the same rule: the
        // component type decides, not the expression assigned into it.
        r.register(new Sn4Leaf("t4.var.assignCompat.elem", Strs.of("4.5", "10.10"), "void",
            "{ double[] a = new double[1]; a[0] = 7; System.out.println(a[0] / 2); }",
            Val.ofDouble(3.5)));
        // §4.5.1: "A variable of a primitive type always holds a value of that exact
        // primitive type." 200 does not fit in a byte, so what the variable holds is the low
        // 8 bits of it read as a signed byte (§5.1.3) -- -56, with no widened residue kept.
        // javelinac warns on this narrowing; the warning is the point, and it is deliberate.
        r.register(new Sn4Leaf("t4.var.byte.residue", Strs.of("4.5.1", "5.1.3"), "void",
                               "{ byte b = (byte)200; System.out.println(b); }",
                               Val.ofByte((byte) -56)));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// F4 — the classification the composed expectations are built on.
//
// A snippet asking "is this operand NaN?" must not ask it with `!=`, and a snippet asking
// "is a less than b?" must not ask it with `<`: those are the operators the snippet exists to
// check, and using them would make the expectation agree with a wrong implementation by
// construction. Everything here goes through the IEEE 754 bit pattern instead
// (Double.doubleToLongBits / Float.floatToIntBits, §20.10.17 / §20.9.18, which canonicalise
// NaN and leave every other value's bits alone), and orders values with an integer key.
// ═══════════════════════════════════════════════════════════════════════════════════════
class F4 {

    private F4() {}

    // §4.2.3's three special forms, and the signed zero §15.14.4 produces from +0.0.
    static final double D_PINF  = Double.POSITIVE_INFINITY;
    static final double D_NINF  = Double.NEGATIVE_INFINITY;
    static final double D_NAN   = Double.NaN;
    static final double D_NZERO = -0.0;
    static final float  F_PINF  = Float.POSITIVE_INFINITY;
    static final float  F_NINF  = Float.NEGATIVE_INFINITY;
    static final float  F_NAN   = Float.NaN;
    static final float  F_NZERO = -0.0f;

    // The IEEE 754 double and single formats, as bit masks.
    static final long D_MAG = 0x7FFFFFFFFFFFFFFFL;   // everything but the sign
    static final long D_EXP = 0x7FF0000000000000L;   // a saturated exponent: infinity or NaN
    static final int  F_MAG = 0x7FFFFFFF;
    static final int  F_EXP = 0x7F800000;

    /** Gate the generator's OWN idea of each special value against the bit pattern §4.2.3
     *  names, before anything is registered. If this ever fires, every floating-point
     *  expectation below was composed from the wrong constant, and a corpus that pinned it
     *  would be pinning the generator's mistake as the compiler's contract. */
    static void selfCheck() {
        req(Double.doubleToLongBits(0.0)     == 0L,                   "+0.0d");
        req(Double.doubleToLongBits(D_NZERO) == 0x8000000000000000L,  "-0.0d");
        req(Double.doubleToLongBits(D_PINF)  == 0x7FF0000000000000L,  "+Infinity (double)");
        req(Double.doubleToLongBits(D_NINF)  == 0xFFF0000000000000L,  "-Infinity (double)");
        req(Double.doubleToLongBits(D_NAN)   == 0x7FF8000000000000L,  "NaN (double)");
        req(Float.floatToIntBits(0.0f)       == 0,                    "+0.0f");
        req(Float.floatToIntBits(F_NZERO)    == 0x80000000,           "-0.0f");
        req(Float.floatToIntBits(F_PINF)     == 0x7F800000,           "+Infinity (float)");
        req(Float.floatToIntBits(F_NINF)     == 0xFF800000,           "-Infinity (float)");
        req(Float.floatToIntBits(F_NAN)      == 0x7FC00000,           "NaN (float)");
    }

    private static void req(boolean ok, String what) {
        if (!ok) throw new RuntimeException("Lib4: the generator's own " + what
            + " is not the IEEE 754 bit pattern JLS 4.2.3 names -- every floating-point"
            + " expectation in this library would be composed from a wrong constant");
    }

    // ---- double ------------------------------------------------------------------------

    static long    dbits(double d) { return Double.doubleToLongBits(d); }
    static boolean dNaN(double d)  { return (dbits(d) & D_MAG) >  D_EXP; }
    static boolean dInf(double d)  { return (dbits(d) & D_MAG) == D_EXP; }
    static boolean dZero(double d) { return (dbits(d) & D_MAG) == 0L; }
    /** The IEEE sign BIT, so -0.0 counts as negative — which is what §15.16.1's and
     *  §15.16.2's "if both operands have the same sign" rule means. */
    static boolean dNeg(double d)  { return dbits(d) < 0L; }
    /** §15.14.4: "Unary minus merely inverts the sign of a floating-point number." */
    static double  dAbs(double d)  { return dNeg(d) ? -d : d; }
    static double  dSigned(boolean neg, double magnitude) { return neg ? -magnitude : magnitude; }

    /** A total order over the non-NaN doubles, as an integer key, so §15.19.1's comparison
     *  rules can be composed without performing a floating-point comparison. §4.2.3 fixes the
     *  arrangement — "negative infinity, negative finite nonzero values, negative zero,
     *  positive zero, positive finite nonzero values, and positive infinity" — and the two
     *  zeros map to the SAME key, which is §15.19.1's "-0.0<0.0 is false". */
    static long dKey(double d) {
        long b = dbits(d);
        return (b < 0L) ? (0x8000000000000000L - b) : b;
    }

    // ---- float -------------------------------------------------------------------------

    static int     fbits(float f) { return Float.floatToIntBits(f); }
    static boolean fNaN(float f)  { return (fbits(f) & F_MAG) >  F_EXP; }
    static boolean fInf(float f)  { return (fbits(f) & F_MAG) == F_EXP; }
    static boolean fZero(float f) { return (fbits(f) & F_MAG) == 0; }
    static boolean fNeg(float f)  { return fbits(f) < 0; }
    static float   fAbs(float f)  { return fNeg(f) ? -f : f; }
    static float   fSigned(boolean neg, float magnitude) { return neg ? -magnitude : magnitude; }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// Sn4Leaf — one leaf, one written-down answer.
//
// Every leaf in this library is a value the spec states outright, so they all share one
// class: the Val carries its own Java type and Stitching.expect() checks it against the
// snippet's, which keeps `char` from being declared as `int` or a float from riding as a
// double.
// ═══════════════════════════════════════════════════════════════════════════════════════
class Sn4Leaf implements Snippet {
    private final String   name, text, ty;
    private final String[] secs;
    private final Val      v;

    Sn4Leaf(String name, String[] secs, String ty, String text, Val v) {
        this.name = name; this.secs = secs; this.ty = ty; this.text = text; this.v = v;
    }
    public String   id()          { return name; }
    public String[] sections()    { return secs; }
    public String   type()        { return ty; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return text; }
    public Val      expect(Val[] h)    { return v; }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// §4.2.2 — the integer operators
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §4.2.2: "The built-in integer operators do not indicate overflow or underflow in any way."
 *  §15.17.2 says what the answer is instead: "If an integer addition overflows, then the
 *  result is the low-order bits of the mathematical sum as represented in some sufficiently
 *  large two's-complement format." long IS such a format for two ints, so the sum is formed
 *  there — exactly, never wrapping — and then narrowed to its low 32 bits (§5.1.3). */
class Sn4AddInt implements Snippet {
    public String   id()          { return "t4.add.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.17.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;      // §15.6 left-to-right
        long sum = (long) h[0].asInt() + (long) h[1].asInt();
        return Val.ofInt((int) sum);
    }
}

/** §15.17.2: "it is always the case that a-b produces the same result as a+(-b)"; the same
 *  low-order-bits rule, formed in long. */
class Sn4SubInt implements Snippet {
    public String   id()          { return "t4.sub.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.17.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " - " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        long diff = (long) h[0].asInt() - (long) h[1].asInt();
        return Val.ofInt((int) diff);
    }
}

/** §4.2.2: the integer remainder operator % "throws an ArithmeticException if the right-hand
 *  operand is zero". §15.16.3 fixes the value in every other case by an identity rather than
 *  by a second remainder: "(a/b)*b+(a%b) is equal to a. This identity holds even in the
 *  special case that the dividend is the negative integer of largest possible magnitude for
 *  its type and the divisor is -1 (the remainder is 0)." */
class Sn4RemInt implements Snippet {
    public String   id()          { return "t4.rem.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.16.3"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " % " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        int a = h[0].asInt(), b = h[1].asInt();
        if (b == 0) return Val.thrown("java.lang.ArithmeticException");
        if (a == Integer.MIN_VALUE && b == -1) return Val.ofInt(0);
        long q = (long) (a / b);                                   // §15.16.2, rounds toward 0
        return Val.ofInt((int) ((long) a - q * (long) b));          // a - (a/b)*b, exact in long
    }
}

/** §4.2.2: the integer divide operator / throws ArithmeticException "if the right-hand operand
 *  is zero". §15.16.2: "if the dividend is the negative integer of largest possible magnitude
 *  for its type, and the divisor is -1, then integer overflow occurs and the result is equal
 *  to the dividend. Despite the overflow, no exception is thrown in this case." Both stated,
 *  not observed; the ordinary quotient is §15.16.2's round-toward-zero division. */
class Sn4DivLong implements Snippet {
    public String   id()          { return "t4.div.long"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.16.2"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("long", "long"); }
    public String   render(String[] h) { return "(" + h[0] + " / " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        long a = h[0].asLong(), b = h[1].asLong();
        if (b == 0L) return Val.thrown("java.lang.ArithmeticException");
        if (a == Long.MIN_VALUE && b == -1L) return Val.ofLong(Long.MIN_VALUE);
        return Val.ofLong(a / b);
    }
}

/** §4.2.2 and §15.16.3 at 64 bits: zero divisor throws; MIN_VALUE % -1 is 0; otherwise the
 *  §15.16.3 identity a - (a/b)*b, in which (a/b)*b can never overflow because its magnitude
 *  is at most |a|. */
class Sn4RemLong implements Snippet {
    public String   id()          { return "t4.rem.long"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.16.3"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("long", "long"); }
    public String   render(String[] h) { return "(" + h[0] + " % " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        long a = h[0].asLong(), b = h[1].asLong();
        if (b == 0L) return Val.thrown("java.lang.ArithmeticException");
        if (a == Long.MIN_VALUE && b == -1L) return Val.ofLong(0L);
        return Val.ofLong(a - (a / b) * b);
    }
}

/** §15.14.5: "note that, in all cases, ~x equals (-x)-1." Composed from that identity in
 *  long, where both steps are exact for any int operand — including MIN_VALUE, whose
 *  complement is MAX_VALUE. */
class Sn4NotInt implements Snippet {
    public String   id()          { return "t4.not.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.5"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "(~" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        long v = -((long) h[0].asInt()) - 1L;
        return Val.ofInt((int) v);
    }
}

/** §15.14.5's identity at 64 bits. The one operand it cannot be applied to directly is
 *  MIN_VALUE, because §15.14.4 makes -MIN_VALUE be MIN_VALUE again; MIN_VALUE-1 then wraps
 *  (§4.2.2) to MAX_VALUE, which is stated here rather than left to a wrapping subtract. */
class Sn4NotLong implements Snippet {
    public String   id()          { return "t4.not.long"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.5"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("long"); }
    public String   render(String[] h) { return "(~" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        long a = h[0].asLong();
        if (a == Long.MIN_VALUE) return Val.ofLong(Long.MAX_VALUE);
        return Val.ofLong(-a - 1L);
    }
}

/** §15.14.4: "For integer values, negation is the same as subtraction from zero. ... the
 *  range of two's-complement values is not symmetric, so negation of the maximum negative int
 *  or long results in that same maximum negative number. Overflow occurs in this case, but no
 *  exception is thrown." */
class Sn4NegInt implements Snippet {
    public String   id()          { return "t4.neg.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.4"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "(-" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        int a = h[0].asInt();
        if (a == Integer.MIN_VALUE) return Val.ofInt(Integer.MIN_VALUE);
        return Val.ofInt(-a);
    }
}

/** §15.14.4 at 64 bits. */
class Sn4NegLong implements Snippet {
    public String   id()          { return "t4.neg.long"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.4"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("long"); }
    public String   render(String[] h) { return "(-" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        long a = h[0].asLong();
        if (a == Long.MIN_VALUE) return Val.ofLong(Long.MIN_VALUE);
        return Val.ofLong(-a);
    }
}

/** §15.14.3: "At run time, the value of the unary plus expression is the promoted value of
 *  the operand." For an int operand the promotion is the identity — the operator must not be
 *  allowed to quietly become anything else. */
class Sn4PlusInt implements Snippet {
    public String   id()          { return "t4.plus.int"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.3"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "(+" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofInt(h[0].asInt());
    }
}

/** §4.2.2: "Otherwise, the operation is carried out using 32-bit precision, and the result of
 *  the numerical operator is of type int. If either operand is not an int, it is first
 *  widened to type int by numeric promotion." Two bytes therefore add as ints: (byte)100 +
 *  (byte)100 is 200, not the -56 an 8-bit add would give. Both operands are within [-128,127]
 *  so the int sum is exact. */
class Sn4AddByte implements Snippet {
    public String   id()          { return "t4.promo.add.byte"; }
    public String[] sections()    { return Strs.of("4.2.2", "4.2.1", "15.17.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("byte", "byte"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofInt(h[0].asInt() + h[1].asInt());
    }
}

/** The same promotion for char, which §4.2.1 makes a 16-bit UNSIGNED integer: the largest
 *  char plus itself is 131070, and would be -2 if the widening to int sign-extended. */
class Sn4AddChar implements Snippet {
    public String   id()          { return "t4.promo.add.char"; }
    public String[] sections()    { return Strs.of("4.2.2", "4.2.1", "15.17.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("char", "char"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofInt(h[0].asInt() + h[1].asInt());
    }
}

/** §15.14.4 after §5.6.1 unary numeric promotion: "The type of the unary minus expression is
 *  the promoted type of the operand", so negating the smallest byte gives the int 128 and not
 *  the byte -128 an 8-bit negation would wrap to. */
class Sn4NegByte implements Snippet {
    public String   id()          { return "t4.promo.neg.byte"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.14.4", "5.6.1"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("byte"); }
    public String   render(String[] h) { return "(-" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofInt(-h[0].asInt());
    }
}

/** §4.2.2: "If an integer operator other than a shift operator has at least one operand of
 *  type long, then the operation is carried out using 64-bit precision, and the result of the
 *  numerical operator is of type long. If the other operand is not long, it is first widened
 *  (§5.1.2) to type long by numeric promotion (§5.6)." The mathematical product of two ints
 *  always fits in a long, so 64-bit precision means there is no wrap at all here — the exact
 *  contrast with the int multiplication that gives -727379968. */
class Sn4MulIntLong implements Snippet {
    public String   id()          { return "t4.promo.mul.int.long"; }
    public String[] sections()    { return Strs.of("4.2.2", "15.16.1", "5.1.2"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " * ((long)" + h[1] + "))"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofLong((long) h[0].asInt() * (long) h[1].asInt());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// §4.2.3 / §4.2.4 — the floating-point operators
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §15.20.1: "If either operand is NaN, then the result of == is false ... Positive zero and
 *  negative zero are considered equal. Therefore, -0.0==0.0 is true ... Otherwise, two
 *  distinct floating-point values are considered unequal by the equality operators." Distinct
 *  values are distinct bit patterns, so the last clause is a bit comparison — never a `==`. */
class Sn4EqDouble implements Snippet {
    public String   id()          { return "t4.fp.eq.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.20.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " == " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b))   return Val.ofBoolean(false);
        if (F4.dZero(a) && F4.dZero(b)) return Val.ofBoolean(true);
        return Val.ofBoolean(F4.dbits(a) == F4.dbits(b));
    }
}

/** §4.2.3: "the inequality operator != returns true if either operand is NaN". Everywhere
 *  else §15.20 gives "a!=b produces the same result as !(a==b)". */
class Sn4NeDouble implements Snippet {
    public String   id()          { return "t4.fp.ne.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.20.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " != " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b))   return Val.ofBoolean(true);
        if (F4.dZero(a) && F4.dZero(b)) return Val.ofBoolean(false);
        return Val.ofBoolean(F4.dbits(a) != F4.dbits(b));
    }
}

/** §15.19.1: "If either operand is NaN, then the result is false. All values other than NaN
 *  are ordered, with negative infinity less than all finite values, and positive infinity
 *  greater than all finite values. Positive zero and negative zero are considered equal.
 *  Therefore, -0.0<0.0 is false." Composed over F4.dKey, an integer key with exactly that
 *  arrangement, so no floating-point comparison decides the answer. */
class Sn4LtDouble implements Snippet {
    public String   id()          { return "t4.fp.lt.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.19.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " < " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b)) return Val.ofBoolean(false);
        return Val.ofBoolean(F4.dKey(a) < F4.dKey(b));
    }
}

/** §15.19.1 for >=, and §4.2.3's "(x<y) == !(x>=y) will be false if x or y is NaN": both
 *  operators return false on a NaN, which only holds because >= is NOT the negation of <. */
class Sn4GeDouble implements Snippet {
    public String   id()          { return "t4.fp.ge.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.19.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " >= " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b)) return Val.ofBoolean(false);
        return Val.ofBoolean(F4.dKey(a) >= F4.dKey(b));
    }
}

/** §4.2.3: "In particular, x!=x is true if and only if x is NaN." The one hole is rendered
 *  twice, which is the shape of the sentence; none of this corpus's expressions has a side
 *  effect, so evaluating it twice is evaluating the same value twice. */
class Sn4SelfNeDouble implements Snippet {
    public String   id()          { return "t4.fp.selfne.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.20.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("double"); }
    public String   render(String[] h) { return "(" + h[0] + " != " + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofBoolean(F4.dNaN(h[0].asDouble()));
    }
}

/** §4.2.3's x!=x at 32 bits — the float NaN must be a NaN too, not a quiet widening of one. */
class Sn4SelfNeFloat implements Snippet {
    public String   id()          { return "t4.fp.selfne.float"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.20.1"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("float"); }
    public String   render(String[] h) { return "(" + h[0] + " != " + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofBoolean(F4.fNaN(h[0].asFloat()));
    }
}

/** §15.14.4: "For floating-point values, negation is not the same as subtraction from zero,
 *  because if x is +0.0, then 0.0-x equals +0.0, but -x equals -0.0. Unary minus merely
 *  inverts the sign of a floating-point number. Special cases of interest: If the operand is
 *  NaN, the result is NaN (recall that NaN has no sign). If the operand is an infinity, the
 *  result is the infinity of opposite sign. If the operand is a zero, the result is the zero
 *  of opposite sign." */
class Sn4NegDouble implements Snippet {
    public String   id()          { return "t4.fp.neg.double"; }
    public String[] sections()    { return Strs.of("4.2.3", "15.14.4"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("double"); }
    public String   render(String[] h) { return "(-" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        double a = h[0].asDouble();
        if (F4.dNaN(a)) return Val.ofDouble(F4.D_NAN);
        return Val.ofDouble(F4.dSigned(!F4.dNeg(a), F4.dAbs(a)));
    }
}

/** §15.17.2, transcribed in the order the book lists it:
 *    "If either operand is NaN, the result is NaN.
 *     The sum of two infinities of opposite sign is NaN.
 *     The sum of two infinities of the same sign is the infinity of that sign.
 *     The sum of an infinity and a finite value is equal to the infinite operand.
 *     The sum of two zeros of opposite sign is positive zero.
 *     The sum of two zeros of the same sign is the zero of that sign.
 *     The sum of a zero and a nonzero finite value is equal to the nonzero operand.
 *     The sum of two nonzero finite values of the same magnitude and opposite sign is
 *       positive zero.
 *     In the remaining cases ... the sum is computed [and] rounded to the nearest
 *       representable value using IEEE 754 round-to-nearest mode." */
class Sn4AddDouble implements Snippet {
    public String   id()          { return "t4.fp.add.double"; }
    public String[] sections()    { return Strs.of("4.2.4", "4.2.3", "15.17.2"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b)) return Val.ofDouble(F4.D_NAN);
        if (F4.dInf(a) && F4.dInf(b))
            return (F4.dNeg(a) != F4.dNeg(b)) ? Val.ofDouble(F4.D_NAN) : Val.ofDouble(a);
        if (F4.dInf(a)) return Val.ofDouble(a);
        if (F4.dInf(b)) return Val.ofDouble(b);
        if (F4.dZero(a) && F4.dZero(b))
            return (F4.dNeg(a) != F4.dNeg(b)) ? Val.ofDouble(0.0) : Val.ofDouble(a);
        if (F4.dZero(a)) return Val.ofDouble(b);
        if (F4.dZero(b)) return Val.ofDouble(a);
        // same magnitude, opposite sign: the bits agree everywhere but the sign bit
        if (F4.dbits(a) == (F4.dbits(b) ^ 0x8000000000000000L)) return Val.ofDouble(0.0);
        return Val.ofDouble(a + b);
    }
}

/** §15.16.1, transcribed:
 *    "If either operand is NaN, the result is NaN.
 *     If the result is not NaN, the sign of the result is positive if both operands have the
 *       same sign, and negative if the operands have different signs.
 *     Multiplication of an infinity by a zero results in NaN.
 *     Multiplication of an infinity by a finite value results in a signed infinity. The sign
 *       is determined by the rule stated above.
 *     In the remaining cases ... the product is computed ... [and] rounded to the nearest
 *       representable value using IEEE 754 round-to-nearest mode."
 *  The sign rule is applied to a magnitude, so it decides the sign of the answer rather than
 *  being inherited from whatever the generator's own multiply produced. */
class Sn4MulDouble implements Snippet {
    public String   id()          { return "t4.fp.mul.double"; }
    public String[] sections()    { return Strs.of("4.2.4", "4.2.3", "15.16.1"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " * " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b)) return Val.ofDouble(F4.D_NAN);
        boolean neg = F4.dNeg(a) != F4.dNeg(b);
        if (F4.dInf(a) && F4.dZero(b)) return Val.ofDouble(F4.D_NAN);
        if (F4.dZero(a) && F4.dInf(b)) return Val.ofDouble(F4.D_NAN);
        if (F4.dInf(a) || F4.dInf(b))  return Val.ofDouble(F4.dSigned(neg, F4.D_PINF));
        return Val.ofDouble(F4.dSigned(neg, F4.dAbs(a) * F4.dAbs(b)));
    }
}

/** §15.16.2, transcribed:
 *    "If either operand is NaN, the result is NaN.
 *     If the result is not NaN, the sign of the result is positive if both operands have the
 *       same sign, negative if the operands have different signs.
 *     Division of an infinity by an infinity results in NaN.
 *     Division of an infinity by a finite value results in a signed infinity.
 *     Division of a finite value by an infinity results in a signed zero.
 *     Division of a zero by a zero results in NaN; division of zero by any other finite value
 *       results in a signed zero.
 *     Division of a nonzero finite value by a zero results in a signed infinity.
 *     In the remaining cases ... the quotient is computed [and rounded]."
 *  And §4.2.4's "Java floating-point operators produce no exceptions": unlike §15.16.2's
 *  integer division, a zero divisor here is a value, not a throw. */
class Sn4DivDouble implements Snippet {
    public String   id()          { return "t4.fp.div.double"; }
    public String[] sections()    { return Strs.of("4.2.4", "4.2.3", "15.16.2"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " / " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        double a = h[0].asDouble(), b = h[1].asDouble();
        if (F4.dNaN(a) || F4.dNaN(b)) return Val.ofDouble(F4.D_NAN);
        boolean neg = F4.dNeg(a) != F4.dNeg(b);
        if (F4.dInf(a) && F4.dInf(b))   return Val.ofDouble(F4.D_NAN);
        if (F4.dInf(a))                 return Val.ofDouble(F4.dSigned(neg, F4.D_PINF));
        if (F4.dInf(b))                 return Val.ofDouble(F4.dSigned(neg, 0.0));
        if (F4.dZero(a) && F4.dZero(b)) return Val.ofDouble(F4.D_NAN);
        if (F4.dZero(a))                return Val.ofDouble(F4.dSigned(neg, 0.0));
        if (F4.dZero(b))                return Val.ofDouble(F4.dSigned(neg, F4.D_PINF));
        return Val.ofDouble(F4.dSigned(neg, F4.dAbs(a) / F4.dAbs(b)));
    }
}

/** §15.16.2's rules at 32 bits, per §4.2.4: "Otherwise, the operation is carried out using
 *  32-bit floating-point arithmetic, and the result of the numerical operator is a value of
 *  type float." Every step is done in float locals, so the generator rounds once where Java
 *  rounds once. */
class Sn4DivFloat implements Snippet {
    public String   id()          { return "t4.fp.div.float"; }
    public String[] sections()    { return Strs.of("4.2.4", "4.2.3", "15.16.2"); }
    public String   type()        { return "float"; }
    public String[] holeTypes()   { return Strs.of("float", "float"); }
    public String   render(String[] h) { return "(" + h[0] + " / " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        float a = h[0].asFloat(), b = h[1].asFloat();
        if (F4.fNaN(a) || F4.fNaN(b)) return Val.ofFloat(F4.F_NAN);
        boolean neg = F4.fNeg(a) != F4.fNeg(b);
        if (F4.fInf(a) && F4.fInf(b))   return Val.ofFloat(F4.F_NAN);
        if (F4.fInf(a))                 return Val.ofFloat(F4.fSigned(neg, F4.F_PINF));
        if (F4.fInf(b))                 return Val.ofFloat(F4.fSigned(neg, 0.0f));
        if (F4.fZero(a) && F4.fZero(b)) return Val.ofFloat(F4.F_NAN);
        if (F4.fZero(a))                return Val.ofFloat(F4.fSigned(neg, 0.0f));
        if (F4.fZero(b))                return Val.ofFloat(F4.fSigned(neg, F4.F_PINF));
        float q = F4.fAbs(a) / F4.fAbs(b);
        return Val.ofFloat(F4.fSigned(neg, q));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// §4.2.5 — the boolean type
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §15.14.6: "the value of the unary logical complement expression is true if the operand
 *  value is false and false if the operand value is true." §4.2.5: the type has exactly two
 *  values, so ! is total. */
class Sn4NotBool implements Snippet {
    public String   id()          { return "t4.bool.not"; }
    public String[] sections()    { return Strs.of("4.2.5", "15.14.6"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("boolean"); }
    public String   render(String[] h) { return "(!" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofBoolean(!h[0].asBoolean());
    }
}

/** §15.20.2: "The result of == is true if the operands are both true or both false;
 *  otherwise, the result is false." */
class Sn4EqBool implements Snippet {
    public String   id()          { return "t4.bool.eq"; }
    public String[] sections()    { return Strs.of("4.2.5", "15.20.2"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.of("boolean", "boolean"); }
    public String   render(String[] h) { return "(" + h[0] + " == " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofBoolean(h[0].asBoolean() == h[1].asBoolean());
    }
}

/** §4.2.5: the string concatenation operator +, "when given a String operand and a boolean
 *  operand, will convert the boolean operand to a String (either "true" or "false")".
 *  §15.17.1.1 routes it through new Boolean(x) (§20.4), whose toString is those two words. */
class Sn4StrBool implements Snippet {
    public String   id()          { return "t4.bool.string"; }
    public String[] sections()    { return Strs.of("4.2.5", "15.17.1.1"); }
    public String   type()        { return "String"; }
    public String[] holeTypes()   { return Strs.of("boolean"); }
    public String   render(String[] h) { return "(\"b=\" + " + h[0] + ")"; }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofString("b=" + (h[0].asBoolean() ? "true" : "false"));
    }
}

/** §4.2.5 lists ?: among the operators a boolean drives, and §15.24 makes it the one that
 *  does not evaluate both arms — so this must NOT use Val.firstThrow: a throwing operand on
 *  the branch not taken never happens. */
class Sn4CondInt implements Snippet {
    public String   id()          { return "t4.bool.cond.int"; }
    public String[] sections()    { return Strs.of("4.2.5", "15.24"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("boolean", "int", "int"); }
    public String   render(String[] h) {
        return "(" + h[0] + " ? " + h[1] + " : " + h[2] + ")";
    }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return h[0].asBoolean() ? h[1] : h[2];
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// §4.2 / §4.5.1 — variables
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §4.2: "Primitive values do not share state with other primitive values. A variable whose
 *  type is a primitive type always holds a primitive value of that same type. The value of a
 *  variable of primitive type can be changed only by assignment operations on that variable."
 *  b is initialised from a and then assigned something else; a is untouched. */
class Sn4VarNoShare implements Snippet {
    public String   id()          { return "t4.var.noshare"; }
    public String[] sections()    { return Strs.of("4.2", "14.3"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) {
        return "{ int a = " + h[0] + "; int b = a; b = " + h[1]
             + "; System.out.println(a); }";
    }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;   // both run before the print
        return h[0];
    }
}

/** §4.5.1: "A variable of a primitive type always holds a value of that exact primitive
 *  type." The narrowing keeps the low 16 bits (§5.1.3) and §4.2.1 makes char unsigned, so
 *  widening the variable back to int can never read a negative number — however negative the
 *  int that was narrowed. */
class Sn4VarCharExact implements Snippet {
    public String   id()          { return "t4.var.char.exact"; }
    public String[] sections()    { return Strs.of("4.5.1", "4.2.1", "5.1.3"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) {
        return "{ char c = (char)" + h[0] + "; System.out.println((int)c); }";
    }
    public Val      expect(Val[] h) {
        if (h[0].isThrows()) return h[0];
        return Val.ofInt(h[0].asInt() & 0xFFFF);
    }
}

/** §4.5.3's seven kinds of variable, each written to and read back so the value proves the
 *  variable exists and holds what was put in it. The sum is composed here, not observed:
 *  1 + 2 + 4 + 8 + 16 + 32 + 64 = 127, so a kind that silently failed to store would change
 *  the total and name itself by which bit went missing. */
class Sn4Kinds implements Snippet, Declaring {
    public String   id()          { return "t4.kinds.seven"; }
    public String[] sections()    { return Strs.of("4.5.3"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String[] decls() {
        String[] d = { "class T4Kinds {\n"
                     + "    static int classVar;          // a class variable\n"
                     + "    int instanceVar;              // an instance variable\n"
                     + "    T4Kinds(int ctorParam) { instanceVar = ctorParam; }\n"
                     + "    static int viaMethod(int methodParam) { return methodParam; }\n"
                     + "}" };
        return d;
    }
    public String[] imports() { return Strs.none(); }
    public String render(String[] h) {
        return "{ int local = 1;"                                    // local variable
             + " int[] comp = new int[1]; comp[0] = 2;"               // array component
             + " T4Kinds.classVar = 4;"                               // class variable
             + " T4Kinds o = new T4Kinds(8);"                         // constructor parameter
             + " int inst = o.instanceVar;"                           // instance variable
             + " int viaM = T4Kinds.viaMethod(16);"                   // method parameter
             + " int handler = 0;"
             + " try { throw new ArithmeticException(); }"
             + " catch (ArithmeticException e) { handler = (e == null) ? 0 : 32; }" // handler param
             + " System.out.println(local + comp[0] + T4Kinds.classVar + inst + viaM"
             + " + handler + 64); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(127); }
}

/** §4.4: ONE of the eleven positions a type name may occupy. Each instance witnesses exactly
 *  one bullet, so the section's coverage is a count of positions rather than a single program
 *  in which a broken position could hide behind the ten that still work.
 *
 *  Every instance carries the SAME two declarations, which Emit deduplicates to one copy per
 *  compilation unit — the point of the section being that one type name appears in all these
 *  places, not that eleven types do. `T4Trouble` exists because §14.18's position needs a type
 *  in a catch clause and reusing a library exception would make the position about the library. */
class Sn4Pos implements Snippet, Declaring {

    // Not `final`: §8.3.1.2 says a final field's "declarator must include a variable
    // initializer or a compile-time error occurs", so Java 1.0 has no blank finals at all.
    private String what;
    private String body;
    private Val    value;
    private String imported;

    Sn4Pos(String what, String body, Val value) { this(what, null, body, value); }

    Sn4Pos(String what, String imported, String body, Val value) {
        this.what     = what;
        this.imported = imported;
        this.body     = body;
        this.value    = value;
    }

    public String   id()          { return "t4.pos." + what; }
    public String[] sections()    { return Strs.of("4.4"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String[] imports() { return imported == null ? Strs.none() : Strs.of(imported); }

    public String[] decls() {
        String[] d = {
            "interface T4Const {\n"
          + "    int LIMIT = 7;\n"
          + "}",

            "class T4Node implements T4Const {\n"
          + "    static T4Node shared;\n"
          + "    long tag;\n"
          + "    T4Node(long tag)   { this.tag = tag; }\n"
          + "    T4Node(T4Node src) { this.tag = src.tag + 1; }\n"
          + "    static T4Node make(long tag)   { return new T4Node(tag); }\n"
          + "    static long   tagOf(T4Node n)  { return n.tag; }\n"
          + "}",

            "class T4Trouble extends RuntimeException {\n"
          + "}" };
        return d;
    }

    public String render(String[] h) { return body; }
    public Val    expect(Val[] h)    { return value; }
}
