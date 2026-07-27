// Lib5 — the snippet library for JLS chapter 5, Conversions and Promotions.
//
// This is the chapter that states its own cardinality, which is why it is the one the plan's
// cardinality gate is aimed at: §5.1.2 (p.54) names "the following 19 specific conversions on
// primitive types... the widening primitive conversions", and §5.1.3 (p.55) "the following 23
// specific conversions... the narrowing primitive conversions". Both are enumerated here in
// full, in the spec's order, one snippet per conversion — so the gate can ask "by how many
// cases" and compare against a number transcribed from the spec rather than against whatever
// happened to be written.
//
// Every rule below is transcribed from java-langspec-1.0.pdf, not recalled:
//
//   §5.1.1  (p.54)  "A conversion from a type to that same type is permitted for any type";
//                   "The only permitted conversion that involves the type boolean is the
//                   identity conversion from boolean to boolean."
//   §5.1.2  (p.54)  the 19; "A widening conversion of a signed integer value to an integral
//                   type T simply sign-extends"; "A widening conversion of a character to an
//                   integral type T zero-extends"; "widening conversions among primitive
//                   types never result in a run-time exception"; and the worked example whose
//                   output is -46.
//   §5.1.3  (p.55)  the 23; the two-step float->integral rule (NaN gives 0, out of range
//                   clamps to the type's min/max, then a second narrowing for byte/char/
//                   short); the fmin/fmax table; "narrowing conversions among primitive types
//                   never result in a run-time exception".
//   §5.1.6  (p.60)  "There is a string conversion to type String from every other type,
//                   including the null type."
//   §5.2    (p.61)  the implicit narrowing of an int CONSTANT to byte/short/char when it is
//                   representable; `byte theAnswer = 42;`
//   §5.6.1  (p.73)  byte/short/char promote to int; a long shift distance does not promote
//                   the value being shifted.
//   §5.6.2  (p.74)  the double > float > long > int ladder.
//
// WHAT THIS LIBRARY DOES NOT COVER, and why it is not a silent gap: §5.1.4/§5.1.5 (reference
// conversions) and §5.3/§5.5 need a class hierarchy — several named types with a known
// subtype relation — which a stitched expression cannot introduce, since a snippet renders an
// expression or a statement and not a compilation unit. They stay UNCOVERED in the ledger
// until the generator can emit auxiliary type declarations. §5.1.7's forbidden set is a
// compile-time error and lives in conformance/reject/.
//
// The expectations are COMPOSED, never observed. Where a conversion is lossy the expected
// value is computed here by the rule the spec states, not by performing the conversion in the
// generator and trusting it: a generator compiled by a javelinac that got a narrowing wrong
// would otherwise compose an expectation matching its own mistake.
public class Lib5 {

    private Lib5() {}

    // One method per group. Lib4's note about MAX_LOCALS at -O0 no longer applies — -O0 packs
    // slots now — but the groups stay small because a group is also the unit you read.
    public static void install(Registry r) {
        identity(r);
        widen(r);
        widenRules(r);
        narrowIntegral(r);
        narrowFloating(r);
        narrowRules(r);
        stringConv(r);
        assignConv(r);
        promotion(r);
    }

    /* ── §5.1.1 identity ────────────────────────────────────────────────────────────────
     * "it is permitted for a program to include redundant cast operators for the sake of
     * clarity" — a cast to a value's own type changes nothing, and boolean has no other. */
    private static void identity(Registry r) {
        r.register(new Sn5Leaf("t5.id.int",  Strs.of("5.1.1"), "int",     "((int)42)",
                               Val.ofInt(42)));
        r.register(new Sn5Leaf("t5.id.long", Strs.of("5.1.1"), "long",    "((long)42L)",
                               Val.ofLong(42L)));
        r.register(new Sn5Leaf("t5.id.bool", Strs.of("5.1.1"), "boolean", "((boolean)true)",
                               Val.ofBoolean(true)));
    }

    /* ── §5.1.2 the NINETEEN, in the spec's order ───────────────────────────────────────
     * Written as a cast rather than an assignment only because a snippet renders an
     * expression; the conversion is the widening one either way, and §5.5 admits it. */
    private static void widen(Registry r) {
        //  byte to short, int, long, float, double            (1..5)
        w(r, "t5.w01.b2s", "short",  "((short)(byte)-5)",   Val.ofShort((short) -5));
        w(r, "t5.w02.b2i", "int",    "((int)(byte)-5)",     Val.ofInt(-5));
        w(r, "t5.w03.b2l", "long",   "((long)(byte)-5)",    Val.ofLong(-5L));
        w(r, "t5.w04.b2f", "float",  "((float)(byte)-5)",   Val.ofFloat(-5.0f));
        w(r, "t5.w05.b2d", "double", "((double)(byte)-5)",  Val.ofDouble(-5.0));
        //  short to int, long, float, double                  (6..9)
        w(r, "t5.w06.s2i", "int",    "((int)(short)-300)",   Val.ofInt(-300));
        w(r, "t5.w07.s2l", "long",   "((long)(short)-300)",  Val.ofLong(-300L));
        w(r, "t5.w08.s2f", "float",  "((float)(short)-300)", Val.ofFloat(-300.0f));
        w(r, "t5.w09.s2d", "double", "((double)(short)-300)",Val.ofDouble(-300.0));
    }

    private static void widenRules(Registry r) {
        //  char to int, long, float, double                   (10..13)
        w(r, "t5.w10.c2i", "int",    "((int)(char)60000)",    Val.ofInt(60000));
        w(r, "t5.w11.c2l", "long",   "((long)(char)60000)",   Val.ofLong(60000L));
        w(r, "t5.w12.c2f", "float",  "((float)(char)60000)",  Val.ofFloat(60000.0f));
        w(r, "t5.w13.c2d", "double", "((double)(char)60000)", Val.ofDouble(60000.0));
        //  int to long, float, double                         (14..16)
        w(r, "t5.w14.i2l", "long",   "((long)-7)",            Val.ofLong(-7L));
        w(r, "t5.w15.i2f", "float",  "((float)-7)",           Val.ofFloat(-7.0f));
        w(r, "t5.w16.i2d", "double", "((double)-7)",          Val.ofDouble(-7.0));
        //  long to float, double                              (17..18)
        w(r, "t5.w17.l2f", "float",  "((float)-9L)",          Val.ofFloat(-9.0f));
        w(r, "t5.w18.l2d", "double", "((double)-9L)",         Val.ofDouble(-9.0));
        //  float to double                                    (19)
        w(r, "t5.w19.f2d", "double", "((double)0.5f)",        Val.ofDouble(0.5));

        // "A widening conversion of a signed integer value to an integral type T simply
        // sign-extends"; "of a character to an integral type T ... zero-extends". Same bits,
        // opposite result — which is the pair, and the reason char is not a short.
        r.register(new Sn5Leaf("t5.w.signext", Strs.of("5.1.2"), "int",
                               "((int)(byte)0xFF)", Val.ofInt(-1)));
        r.register(new Sn5Leaf("t5.w.zeroext", Strs.of("5.1.2"), "int",
                               "((int)(char)0xFFFF)", Val.ofInt(65535)));
        // The spec's own loss-of-precision program: `int big = 1234567890; float approx = big;
        // System.out.println(big - (int)approx);` "which prints: -46".
        r.register(new Sn5Leaf("t5.w.lossy", Strs.of("5.1.2"), "void",
                               "{ int big = 1234567890; float approx = big;"
                             + " System.out.println(big - (int)approx); }", Val.ofInt(-46)));
    }

    /* ── §5.1.3 the TWENTY-THREE, in the spec's order ───────────────────────────────────
     * "A narrowing conversion of a signed integer to an integral type T simply discards all
     * but the n lowest order bits" — so every expectation below is that mask, computed here
     * from the rule. */
    private static void narrowIntegral(Registry r) {
        //  byte to char                                       (1)
        n(r, "t5.n01.b2c", "int", "((int)(char)(byte)-1)",        Val.ofInt(0xFFFF));
        //  short to byte, char                                (2..3)
        n(r, "t5.n02.s2b", "int", "((int)(byte)(short)0x1234)",   Val.ofInt(0x34));
        n(r, "t5.n03.s2c", "int", "((int)(char)(short)0x1234)",   Val.ofInt(0x1234));
        //  char to byte, short                                (4..5)
        n(r, "t5.n04.c2b", "int", "((int)(byte)(char)0xF001)",    Val.ofInt(0x01));
        n(r, "t5.n05.c2s", "int", "((int)(short)(char)0xF001)",   Val.ofInt(-4095));
        //  int to byte, short, char                           (6..8)
        n(r, "t5.n06.i2b", "int", "((int)(byte)0x12345678)",      Val.ofInt(0x78));
        n(r, "t5.n07.i2s", "int", "((int)(short)0x12345678)",     Val.ofInt(0x5678));
        n(r, "t5.n08.i2c", "int", "((int)(char)0x12345678)",      Val.ofInt(0x5678));
        //  long to byte, short, char, int                     (9..12)
        n(r, "t5.n09.l2b", "int", "((int)(byte)0x1122334455667788L)",  Val.ofInt(-120));
        n(r, "t5.n10.l2s", "int", "((int)(short)0x1122334455667788L)", Val.ofInt(0x7788));
        n(r, "t5.n11.l2c", "int", "((int)(char)0x1122334455667788L)",  Val.ofInt(0x7788));
        n(r, "t5.n12.l2i", "int", "((int)0x1122334455667788L)",        Val.ofInt(0x55667788));
    }

    private static void narrowFloating(Registry r) {
        //  float to byte, short, char, int, long              (13..17)
        n(r, "t5.n13.f2b", "int",  "((int)(byte)300.7f)",   Val.ofInt(44));
        n(r, "t5.n14.f2s", "int",  "((int)(short)300.7f)",  Val.ofInt(300));
        n(r, "t5.n15.f2c", "int",  "((int)(char)300.7f)",   Val.ofInt(300));
        n(r, "t5.n16.f2i", "int",  "((int)300.7f)",         Val.ofInt(300));
        n(r, "t5.n17.f2l", "long", "((long)300.7f)",        Val.ofLong(300L));
        //  double to byte, short, char, int, long, float      (18..23)
        n(r, "t5.n18.d2b", "int",   "((int)(byte)-70000.9)",  Val.ofInt(-112));
        n(r, "t5.n19.d2s", "int",   "((int)(short)-70000.9)", Val.ofInt(-4464));
        n(r, "t5.n20.d2c", "int",   "((int)(char)-70000.9)",  Val.ofInt(61072));
        n(r, "t5.n21.d2i", "int",   "((int)-70000.9)",        Val.ofInt(-70000));
        n(r, "t5.n22.d2l", "long",  "((long)-70000.9)",       Val.ofLong(-70000L));
        n(r, "t5.n23.d2f", "float", "((float)-70000.9)",      Val.ofFloat(-70000.9f));
    }

    private static void narrowRules(Registry r) {
        // Step 1: "If the floating-point number is NaN, the result ... is an int or long 0."
        r.register(new Sn5Leaf("t5.n.nan.i", Strs.of("5.1.3"), "int",
                               "((int)(0.0f/0.0f))",  Val.ofInt(0)));
        r.register(new Sn5Leaf("t5.n.nan.l", Strs.of("5.1.3"), "long",
                               "((long)(0.0/0.0))",   Val.ofLong(0L)));
        // The spec's fmin/fmax table, printed as `long: -9223372036854775808..9223372036854775807`
        // and `short: 0..-1` and `byte: 0..-1` — too small clamps to the smallest representable
        // int/long, too large to the largest, and byte/short/char then take the LOW BITS of those.
        r.register(new Sn5Leaf("t5.n.inf.l.min", Strs.of("5.1.3"), "long",
                               "((long)Float.NEGATIVE_INFINITY)", Val.ofLong(-9223372036854775808L)));
        r.register(new Sn5Leaf("t5.n.inf.l.max", Strs.of("5.1.3"), "long",
                               "((long)Float.POSITIVE_INFINITY)", Val.ofLong(9223372036854775807L)));
        r.register(new Sn5Leaf("t5.n.inf.i.min", Strs.of("5.1.3"), "int",
                               "((int)Float.NEGATIVE_INFINITY)",  Val.ofInt(-2147483648)));
        r.register(new Sn5Leaf("t5.n.inf.i.max", Strs.of("5.1.3"), "int",
                               "((int)Float.POSITIVE_INFINITY)",  Val.ofInt(2147483647)));
        r.register(new Sn5Leaf("t5.n.inf.s.min", Strs.of("5.1.3"), "int",
                               "((int)(short)Float.NEGATIVE_INFINITY)", Val.ofInt(0)));
        r.register(new Sn5Leaf("t5.n.inf.s.max", Strs.of("5.1.3"), "int",
                               "((int)(short)Float.POSITIVE_INFINITY)", Val.ofInt(-1)));
        r.register(new Sn5Leaf("t5.n.inf.b.max", Strs.of("5.1.3"), "int",
                               "((int)(byte)Float.POSITIVE_INFINITY)",  Val.ofInt(-1)));
        // The spec's six-line narrowing program, with the values it prints.
        r.register(new Sn5Leaf("t5.n.ex.short", Strs.of("5.1.3"), "int",
                               "((int)(short)0x12345678)", Val.ofInt(0x5678)));
        r.register(new Sn5Leaf("t5.n.ex.byte",  Strs.of("5.1.3"), "int",
                               "((int)(byte)255)",         Val.ofInt(-1)));
        r.register(new Sn5Leaf("t5.n.ex.big",   Strs.of("5.1.3"), "int",
                               "((int)1e20f)",             Val.ofInt(2147483647)));
        // "A double value too large for float yields infinity"; "too small ... underflows to
        // zero"; "A double NaN is always converted to a float NaN."
        r.register(new Sn5Leaf("t5.n.ex.negInf", Strs.of("5.1.3"), "boolean",
                               "((float)-1e100 == Float.NEGATIVE_INFINITY)", Val.ofBoolean(true)));
        r.register(new Sn5Leaf("t5.n.ex.under",  Strs.of("5.1.3"), "boolean",
                               "((float)1e-50 == 0.0f)",                     Val.ofBoolean(true)));
        r.register(new Sn5Leaf("t5.n.ex.nan",    Strs.of("5.1.3"), "boolean",
                               "(Float.isNaN((float)(0.0/0.0)))",            Val.ofBoolean(true)));
    }

    /* ── §5.1.6 string conversion from EVERY other type, including the null type ────────
     * "There is a string conversion to type String from every other type, including the null
     * type" — so every one of them, rather than a representative sample. */
    private static void stringConv(Registry r) {
        s(r, "t5.s.byte",   "(\"\" + (byte)-1)",     "-1");
        s(r, "t5.s.short",  "(\"\" + (short)-2)",    "-2");
        s(r, "t5.s.char",   "(\"\" + 'q')",          "q");
        s(r, "t5.s.int",    "(\"\" + -3)",           "-3");
        s(r, "t5.s.long",   "(\"\" + -4L)",          "-4");
        s(r, "t5.s.float",  "(\"\" + 1.5f)",         "1.5");
        s(r, "t5.s.double", "(\"\" + 2.5)",          "2.5");
        s(r, "t5.s.bool",   "(\"\" + true)",         "true");
        s(r, "t5.s.null",   "(\"\" + (String)null)", "null");
    }

    /* ── §5.2 assignment conversion: the implicit narrowing of an int CONSTANT ──────────
     * "a narrowing primitive conversion may be used if ... the expression is a constant
     * expression of type int ... the type of the variable is byte, short, or char ... the
     * value ... is representable in the type of the variable." The spec's own line is
     * `byte theAnswer = 42;`. The boundary belongs to conformance/reject/, where `byte b =
     * 128;` is rejected — a value one past representable. */
    private static void assignConv(Registry r) {
        r.register(new Sn5Leaf("t5.a.byte",   Strs.of("5.2"), "void",
                               "{ byte theAnswer = 42; System.out.println(theAnswer); }",
                               Val.ofInt(42)));
        r.register(new Sn5Leaf("t5.a.short",  Strs.of("5.2"), "void",
                               "{ short s = 12345; System.out.println(s); }", Val.ofInt(12345)));
        r.register(new Sn5Leaf("t5.a.char",   Strs.of("5.2"), "void",
                               "{ char c = 65; System.out.println((int)c); }", Val.ofInt(65)));
        r.register(new Sn5Leaf("t5.a.edge",   Strs.of("5.2"), "void",
                               "{ byte lo = -128; byte hi = 127; System.out.println(lo + hi); }",
                               Val.ofInt(-1)));
        // A constant EXPRESSION (§15.27), not merely a literal — the condition says
        // "constant expression", and `40 + 2` is one.
        r.register(new Sn5Leaf("t5.a.folded", Strs.of("5.2", "15.27"), "void",
                               "{ byte b = 40 + 2; System.out.println(b); }", Val.ofInt(42)));
        // The spec's assignment-conversion program prints f=12.0 and d=1.2300000190734863.
        r.register(new Sn5Leaf("t5.a.ex.f", Strs.of("5.2"), "String",
                               "(\"f=\" + (float)(short)12)", Val.ofString("f=12.0")));
        r.register(new Sn5Leaf("t5.a.ex.d", Strs.of("5.2"), "String",
                               "(\"d=\" + (double)1.23f)",    Val.ofString("d=1.2300000190734863")));
    }

    /* ── §5.6.1 unary and §5.6.2 binary numeric promotion ───────────────────────────────
     * "If the operand is of compile-time type byte, short, or char, unary numeric promotion
     * promotes it to a value of type int"; and the binary ladder double > float > long > int. */
    private static void promotion(Registry r) {
        // §5.6.1's own example prints ~0xffffffff==0x0 and 0xffffffff<<4L==0xfffffff0 — the
        // second because "a long shift distance (right operand) does not promote the value
        // being shifted (left operand) to long".
        r.register(new Sn5Leaf("t5.p.complement", Strs.of("5.6.1"), "int",
                               "(~(byte)-1)", Val.ofInt(0)));
        r.register(new Sn5Leaf("t5.p.shiftLong", Strs.of("5.6.1"), "int",
                               "((byte)-1 << 4L)", Val.ofInt(-16)));
        r.register(new Sn5Leaf("t5.p.unaryMinus", Strs.of("5.6.1"), "int",
                               "(-(char)1)", Val.ofInt(-1)));
        // §5.6.2's ladder, one snippet per rung, each observed through the RESULT type.
        r.register(new Sn5Leaf("t5.p.intDouble", Strs.of("5.6.2"), "double",
                               "(1 / 2.0)",   Val.ofDouble(0.5)));
        r.register(new Sn5Leaf("t5.p.longFloat", Strs.of("5.6.2"), "float",
                               "(1L / 2.0f)", Val.ofFloat(0.5f)));
        r.register(new Sn5Leaf("t5.p.intLong",  Strs.of("5.6.2"), "long",
                               "(1 + 9000000000L)", Val.ofLong(9000000001L)));
        r.register(new Sn5Leaf("t5.p.byteShort", Strs.of("5.6.2"), "int",
                               "((byte)1 + (short)1)", Val.ofInt(2)));
        // The spec's char&byte example: "converts the ASCII character G to the ASCII control-G
        // (BEL), by masking off all but the low 5 bits" — and prints 7.
        r.register(new Sn5Leaf("t5.p.ex.control", Strs.of("5.6.2"), "int",
                               "('G' & (byte)0x1f)", Val.ofInt(7)));
    }

    /* ── registration helpers ───────────────────────────────────────────────────────────
     * Separate helpers per section so a snippet cannot claim §5.1.2 while demonstrating a
     * narrowing: the section is a property of WHICH LIST the conversion is on, and that is
     * decided here rather than at each call. */
    private static void w(Registry r, String id, String ty, String text, Val v) {
        r.register(new Sn5Leaf(id, Strs.of("5.1.2"), ty, text, v));
    }
    private static void n(Registry r, String id, String ty, String text, Val v) {
        r.register(new Sn5Leaf(id, Strs.of("5.1.3"), ty, text, v));
    }
    private static void s(Registry r, String id, String text, String expected) {
        r.register(new Sn5Leaf(id, Strs.of("5.1.6"), "String", text, Val.ofString(expected)));
    }
}

/** A zero-hole chapter-5 snippet: a fixed expression or statement with a fixed expected
 *  value. Chapter 5's rules are about a conversion applied to a KNOWN value, so almost every
 *  one of them is a leaf — which the plan anticipates: "Where a chapter turns out not to be
 *  parameterisable, the template for it has zero holes — which is a degenerate stitch, not an
 *  exemption, and it still carries sections[] and expects and still goes through the same
 *  emitter." */
class Sn5Leaf implements Snippet {
    private final String   name, text, ty;
    private final String[] secs;
    private final Val      v;

    Sn5Leaf(String name, String[] secs, String ty, String text, Val v) {
        this.name = name; this.secs = secs; this.ty = ty; this.text = text; this.v = v;
    }
    public String   id()          { return name; }
    public String[] sections()    { return secs; }
    public String   type()        { return ty; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return text; }
    public Val      expect(Val[] h)    { return v; }
}
