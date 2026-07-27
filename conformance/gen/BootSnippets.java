// BootSnippets — the worked example, and the engine's own self-test.
//
// This is a snippet library exactly like any other: a class with `static void install`,
// registering Snippet instances. It is deliberately small, and it is chosen to touch every
// mechanism the engine has, so that generating from it exercises the engine end to end:
//
//   leaves of every kind        lit.*             (terminate the recursion)
//   one-hole composition        conv.*            (a value folded through a conversion)
//   two-hole composition        arith.*, str.*    (an odometer over two hole lists)
//   THROWS composed, not run    arith.div.int     (b == 0 -> ArithmeticException)
//   THROWS propagated           every strict op   (Val.firstThrow)
//   a reference cast that fails cast.obj2string   (§5.1.5 -> ClassCastException)
//   the float/double carrier    lit.float.tenth   (Float.toString vs Double.toString)
//   a "void" statement          stmt.print.*      (renders as a statement, prints its line)
//
// Java 1.0 has no anonymous or nested classes, so each snippet is a package-private
// top-level class in this file. That is the shape every snippet library will have.
//
// THE RULE FOR expect(): compose from the holes and from the CITED RULE. Never re-derive an
// answer by doing the same operation javelinac is under test for, where the spec states the
// result outright. arith.mul.int computes in long and truncates (§15.16.1's "low 32 bits"),
// and arith.div.int names the MIN_VALUE / -1 overflow (§15.16.2) rather than trusting the
// generator's own divide to produce it.
public class BootSnippets {

    private BootSnippets() {}

    public static void install(Registry r) {
        // ---- leaves ---------------------------------------------------------------------
        r.register(new SnIntLit("lit.int.zero",  "0",            0));
        r.register(new SnIntLit("lit.int.seven", "7",            7));
        r.register(new SnIntLit("lit.int.neg3",  "-3",          -3));
        r.register(new SnIntLit("lit.int.neg1",  "-1",          -1));
        r.register(new SnIntLit("lit.int.min",   "-2147483648", -2147483648));

        r.register(new SnLongLit("lit.long.4g",   "4294967296L", 4294967296L));
        r.register(new SnLongLit("lit.long.neg1", "-1L",         -1L));

        r.register(new SnCharLit());
        r.register(new SnDoubleLit());
        r.register(new SnFloatLit());
        r.register(new SnBoolLit("lit.bool.true",  "true",  true));
        r.register(new SnBoolLit("lit.bool.false", "false", false));
        r.register(new SnStringLit());

        r.register(new SnObjString());
        r.register(new SnObjNull());
        r.register(new SnObjStringBuffer());

        // ---- one hole -------------------------------------------------------------------
        r.register(new SnWidenIntToLong());
        r.register(new SnWidenIntToDouble());
        r.register(new SnWidenFloatToDouble());
        r.register(new SnNarrowIntToChar());
        r.register(new SnCastObjToString());

        // ---- two holes ------------------------------------------------------------------
        r.register(new SnMulInt());
        r.register(new SnDivInt());
        r.register(new SnAddLong());
        r.register(new SnAddDouble());
        r.register(new SnConcat());

        // ---- statements -----------------------------------------------------------------
        r.register(new SnPrintLong());
        r.register(new SnPrintString());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// leaves
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §3.10.1 integer literals. -2147483648 is the one decimal int literal that may appear
 *  only as the operand of unary minus, and it is here on purpose. */
class SnIntLit implements Snippet {
    private final String name, text;
    private final int    v;
    SnIntLit(String name, String text, int v) { this.name = name; this.text = text; this.v = v; }
    public String   id()          { return name; }
    public String[] sections()    { return Strs.of("3.10.1"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(" + text + ")"; }
    public Val      expect(Val[] h)    { return Val.ofInt(v); }
}

/** §3.10.1 integer literals of type long (the L suffix). */
class SnLongLit implements Snippet {
    private final String name, text;
    private final long   v;
    SnLongLit(String name, String text, long v) { this.name = name; this.text = text; this.v = v; }
    public String   id()          { return name; }
    public String[] sections()    { return Strs.of("3.10.1"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(" + text + ")"; }
    public Val      expect(Val[] h)    { return Val.ofLong(v); }
}

/** §3.10.4 character literals. */
class SnCharLit implements Snippet {
    public String   id()          { return "lit.char.A"; }
    public String[] sections()    { return Strs.of("3.10.4"); }
    public String   type()        { return "char"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "('A')"; }
    public Val      expect(Val[] h)    { return Val.ofChar('A'); }
}

/** §3.10.2 floating-point literals — 0.1 is not representable, so this also pins §20.10.15
 *  printing of the nearest double. */
class SnDoubleLit implements Snippet {
    public String   id()          { return "lit.double.tenth"; }
    public String[] sections()    { return Strs.of("3.10.2", "4.2.3"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(0.1)"; }
    public Val      expect(Val[] h)    { return Val.ofDouble(0.1); }
}

/** §3.10.2 with the f suffix. Printed by Float.toString, which is a DIFFERENT function from
 *  Double.toString on the same value — the carrier check. */
class SnFloatLit implements Snippet {
    public String   id()          { return "lit.float.tenth"; }
    public String[] sections()    { return Strs.of("3.10.2", "4.2.3"); }
    public String   type()        { return "float"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(0.1f)"; }
    public Val      expect(Val[] h)    { return Val.ofFloat(0.1f); }
}

/** §3.10.3 boolean literals. */
class SnBoolLit implements Snippet {
    private final String  name, text;
    private final boolean v;
    SnBoolLit(String name, String text, boolean v) { this.name = name; this.text = text; this.v = v; }
    public String   id()          { return name; }
    public String[] sections()    { return Strs.of("3.10.3"); }
    public String   type()        { return "boolean"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(" + text + ")"; }
    public Val      expect(Val[] h)    { return Val.ofBoolean(v); }
}

/** §3.10.5 string literals. */
class SnStringLit implements Snippet {
    public String   id()          { return "lit.string.hi"; }
    public String[] sections()    { return Strs.of("3.10.5"); }
    public String   type()        { return "String"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "(\"hi\")"; }
    public Val      expect(Val[] h)    { return Val.ofString("hi"); }
}

/** §5.1.4 widening reference conversion, String to Object. The Val's type stays the RUN-TIME
 *  class, which is what the §5.1.5 cast below reads. */
class SnObjString implements Snippet {
    public String   id()          { return "obj.string"; }
    public String[] sections()    { return Strs.of("5.1.4"); }
    public String   type()        { return "Object"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "((Object)\"ob\")"; }
    public Val      expect(Val[] h)    { return Val.ofRef("String", "ob"); }
}

/** §3.10.7 the null literal, widened to Object (§5.1.4). */
class SnObjNull implements Snippet {
    public String   id()          { return "obj.null"; }
    public String[] sections()    { return Strs.of("3.10.7", "5.1.4"); }
    public String   type()        { return "Object"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "((Object)null)"; }
    public Val      expect(Val[] h)    { return Val.ofNull(); }
}

/** An Object that is NOT a String, with a deterministic toString (§20.13.13 returns the
 *  buffer's contents — no identity hash, so the printed line is stable). This is the value
 *  that makes cast.obj2string throw. */
class SnObjStringBuffer implements Snippet {
    public String   id()          { return "obj.strbuf"; }
    public String[] sections()    { return Strs.of("5.1.4"); }
    public String   type()        { return "Object"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String   render(String[] h) { return "((Object)new StringBuffer(\"sb\"))"; }
    public Val      expect(Val[] h)    { return Val.ofRef("StringBuffer", "sb"); }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// one hole
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §5.1.2 int to long: sign-extends, never throws. */
class SnWidenIntToLong implements Snippet {
    public String   id()          { return "conv.widen.int2long"; }
    public String[] sections()    { return Strs.of("5.1.2"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "((long)" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofLong((long) h[0].asInt());
    }
}

/** §5.1.2 int to double: exact for every int (a double has 53 bits of mantissa). */
class SnWidenIntToDouble implements Snippet {
    public String   id()          { return "conv.widen.int2double"; }
    public String[] sections()    { return Strs.of("5.1.2"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "((double)" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofDouble((double) h[0].asInt());
    }
}

/** §5.1.2 float to double: exact, and the printed form CHANGES (0.1f prints "0.1" as a
 *  float and "0.10000000149011612" as a double). If the generator carried floats sloppily
 *  this line would be wrong. */
class SnWidenFloatToDouble implements Snippet {
    public String   id()          { return "conv.widen.float2double"; }
    public String[] sections()    { return Strs.of("5.1.2"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("float"); }
    public String   render(String[] h) { return "((double)" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofDouble((double) h[0].asFloat());
    }
}

/** §5.1.3 int to char: keeps the low 16 bits, never throws. */
class SnNarrowIntToChar implements Snippet {
    public String   id()          { return "conv.narrow.int2char"; }
    public String[] sections()    { return Strs.of("5.1.3"); }
    public String   type()        { return "char"; }
    public String[] holeTypes()   { return Strs.of("int"); }
    public String   render(String[] h) { return "((char)" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofChar((char) (h[0].asInt() & 0xFFFF));   // §5.1.3: the low n bits
    }
}

/** §15.15 / §5.1.5 narrowing reference conversion: checked at run time, and a
 *  ClassCastException when the value's class is not assignment compatible. The null
 *  reference always passes (§5.1.5). */
class SnCastObjToString implements Snippet {
    public String   id()          { return "cast.obj2string"; }
    public String[] sections()    { return Strs.of("15.15", "5.1.5"); }
    public String   type()        { return "String"; }
    public String[] holeTypes()   { return Strs.of("Object"); }
    public String   render(String[] h) { return "((String)" + h[0] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        if (h[0].isNull())                return Val.ofNull();
        if (h[0].type().equals("String")) return Val.ofString(h[0].display());
        return Val.thrown("java.lang.ClassCastException");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// two holes
// ═══════════════════════════════════════════════════════════════════════════════════════

/** §15.16.1 int multiplication: the result is the low 32 bits of the mathematical product,
 *  so this composes it in long and truncates rather than re-doing an int multiply. */
class SnMulInt implements Snippet {
    public String   id()          { return "arith.mul.int"; }
    public String[] sections()    { return Strs.of("15.16.1", "4.2.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " * " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;   // §15.6 left-to-right
        long product = (long) h[0].asInt() * (long) h[1].asInt();
        return Val.ofInt((int) product);
    }
}

/** §15.16.2 int division. Two rules the operator does not obey elsewhere: division by zero
 *  throws ArithmeticException, and MIN_VALUE / -1 overflows to MIN_VALUE rather than
 *  throwing. Both are stated here, not observed. */
class SnDivInt implements Snippet {
    public String   id()          { return "arith.div.int"; }
    public String[] sections()    { return Strs.of("15.16.2", "4.2.2"); }
    public String   type()        { return "int"; }
    public String[] holeTypes()   { return Strs.of("int", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " / " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        int a = h[0].asInt(), b = h[1].asInt();
        if (b == 0) return Val.thrown("java.lang.ArithmeticException");
        if (a == -2147483648 && b == -1) return Val.ofInt(-2147483648);   // §15.16.2 overflow
        return Val.ofInt(a / b);
    }
}

/** §15.17.2 long addition, wrapping (§4.2.2 two's-complement, no overflow signal). */
class SnAddLong implements Snippet {
    public String   id()          { return "arith.add.long"; }
    public String[] sections()    { return Strs.of("15.17.2", "4.2.2"); }
    public String   type()        { return "long"; }
    public String[] holeTypes()   { return Strs.of("long", "long"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofLong(h[0].asLong() + h[1].asLong());
    }
}

/** §15.17.2 double addition (§4.2.4 IEEE 754 round-to-nearest). */
class SnAddDouble implements Snippet {
    public String   id()          { return "arith.add.double"; }
    public String[] sections()    { return Strs.of("15.17.2", "4.2.4"); }
    public String   type()        { return "double"; }
    public String[] holeTypes()   { return Strs.of("double", "double"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofDouble(h[0].asDouble() + h[1].asDouble());
    }
}

/** §15.17.1 string concatenation, with §15.17.1.1's string conversion of the right operand
 *  (and of a null left operand, which becomes "null"). */
class SnConcat implements Snippet {
    public String   id()          { return "str.concat.int"; }
    public String[] sections()    { return Strs.of("15.17.1", "15.17.1.1", "5.1.6"); }
    public String   type()        { return "String"; }
    public String[] holeTypes()   { return Strs.of("String", "int"); }
    public String   render(String[] h) { return "(" + h[0] + " + " + h[1] + ")"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return Val.ofString(h[0].stringConversion() + h[1].stringConversion());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// statements ("void")
// ═══════════════════════════════════════════════════════════════════════════════════════
//
// A void snippet renders a COMPLETE STATEMENT and must print exactly one line itself, or
// throw before printing anything. Emit does not wrap it in a println; its expected line is
// its expect().display().

/** §14.7 expression statement — a method invocation used for its effect. */
class SnPrintLong implements Snippet {
    public String   id()          { return "stmt.print.long"; }
    public String[] sections()    { return Strs.of("14.7"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("long"); }
    public String   render(String[] h) { return "System.out.println(" + h[0] + ");"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return h[0];                       // the line printed IS the hole's value
    }
}

/** §14.7, and the null-String print path (§22.14: println of a null String prints "null"). */
class SnPrintString implements Snippet {
    public String   id()          { return "stmt.print.string"; }
    public String[] sections()    { return Strs.of("14.7"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("String"); }
    public String   render(String[] h) { return "System.out.println(" + h[0] + ");"; }
    public Val      expect(Val[] h) {
        Val t = Val.firstThrow(h); if (t != null) return t;
        return h[0];
    }
}
