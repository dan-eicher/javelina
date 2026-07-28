// Val — a statically-known value, composed rather than observed.
//
// A Val is what a stitching's expect() returns: the answer the generator KNOWS, folded up
// from its holes' answers. It never comes from running the program. If a Val had to be
// learned by executing the case, the corpus would be a snapshot of current behaviour and
// would pin a miscompile as the expectation.
//
// display() is the whole point of the class: it renders exactly the bytes
// System.out.println would emit for this value, because that string IS the .expected line.
// So the kind alone is not enough — `char` and `int` share the LONG kind but print
// differently, and `float` and `double` share DOUBLE but Float.toString(0.1f) is "0.1"
// while Double.toString(0.1f) is "0.10000000149011612". Every Val therefore carries the
// Java TYPE it has as well as its kind, and display() dispatches on the pair.
//
// THROWS is a value like any other: a stitching that throws has a known outcome, and its
// display() is the exception's fully-qualified class name — which is what Class.getName()
// returns (§20.3.2) and what Emit's catch clause prints.
public final class Val {

    // ---- kinds -------------------------------------------------------------------------
    public static final int LONG    = 0;   // byte, short, char, int, long
    public static final int DOUBLE  = 1;   // float, double
    public static final int STRING  = 2;   // java.lang.String (payload may be null)
    public static final int BOOLEAN = 3;   // boolean
    public static final int REF     = 4;   // any other reference; payload is its println text
    public static final int THROWS  = 5;   // evaluation completes abruptly; payload is the FQN

    // None of these is `final`, though every one is write-once: §8.3.1.2 requires a final
    // field's declarator to carry its initializer, which a constructor parameter cannot be.
    private int     kind;
    // For LONG/DOUBLE/BOOLEAN this is the JAVA TYPE — "byte","short","char","int","long",
    // "float","double","boolean" — and display() dispatches on it, so it must be exact.
    // For STRING/REF it is the value's RUN-TIME class ("String", "StringBuffer", ...), or
    // "null" for the null reference (§4.1: the null type, whose only value is null); that
    // is what a §5.1.5 cast snippet needs to decide whether the cast throws.
    // For THROWS it is "" — a throw has no value type.
    private String  type;
    private long    l;
    private double  d;
    private boolean b;
    private String  s;            // STRING payload | REF println-text | THROWS class name

    private Val(int kind, String type, long l, double d, boolean b, String s) {
        this.kind = kind; this.type = type;
        this.l = l; this.d = d; this.b = b; this.s = s;
    }

    // ---- constructors ------------------------------------------------------------------

    public static Val ofByte(byte v)     { return new Val(LONG, "byte",  (long) v, 0.0, false, null); }
    public static Val ofShort(short v)   { return new Val(LONG, "short", (long) v, 0.0, false, null); }
    public static Val ofChar(char v)     { return new Val(LONG, "char",  (long) v, 0.0, false, null); }
    public static Val ofInt(int v)       { return new Val(LONG, "int",   (long) v, 0.0, false, null); }
    public static Val ofLong(long v)     { return new Val(LONG, "long",  v,        0.0, false, null); }

    // A float rides in the double field. The widening is exact and asFloat()/display()
    // narrow it straight back, so nothing is lost HERE — but a snippet must do its float
    // arithmetic in float locals and only then call ofFloat. Computing in double and
    // narrowing at the end rounds once where Java rounds twice.
    public static Val ofFloat(float v)   { return new Val(DOUBLE, "float",  0L, (double) v, false, null); }
    public static Val ofDouble(double v) { return new Val(DOUBLE, "double", 0L, v,          false, null); }

    public static Val ofBoolean(boolean v) { return new Val(BOOLEAN, "boolean", 0L, 0.0, v, null); }

    /** A String value. `v` may be null — that is the null String, and it prints "null". */
    public static Val ofString(String v) { return new Val(STRING, "String", 0L, 0.0, false, v); }

    /** A non-String reference whose println text (String.valueOf(obj)) is `display`. */
    public static Val ofRef(String type, String display) {
        return new Val(REF, type, 0L, 0.0, false, display);
    }

    /** The null reference — §4.1's null type, assignable to every reference type. Its
     *  string conversion is "null" (§15.17.1.1) and it survives every cast (§5.1.5). */
    public static Val ofNull() { return new Val(REF, "null", 0L, 0.0, false, "null"); }

    /** True for the null reference, whatever its static type was. */
    public boolean isNull() { return kind == REF && type.equals("null"); }

    /** Evaluation completes abruptly. `className` is the FULLY QUALIFIED name, e.g.
     *  "java.lang.ArithmeticException" — Class.getName() is §20.3.2 fully qualified. */
    public static Val thrown(String className) {
        if (className == null) throw new RuntimeException("Val.thrown: null class name");
        return new Val(THROWS, "", 0L, 0.0, false, className);
    }

    // ---- accessors ---------------------------------------------------------------------
    //
    // Every accessor checks its kind. A snippet's expect() that reads asLong() off a THROWS
    // hole is a generator bug, and the whole design rests on generator bugs being loud:
    // a silently-zero payload would compose into a wrong .expected line, which reads as a
    // compiler failure on the case that consumes it.

    public int     kind()     { return kind; }
    public String  type()     { return type; }
    public boolean isThrows() { return kind == THROWS; }

    public long asLong() {
        if (kind != LONG) throw new RuntimeException("Val.asLong on kind " + kindName(kind));
        return l;
    }
    public int    asInt()  { return (int)  asLong(); }
    public char   asChar() { return (char) asLong(); }

    public double asDouble() {
        if (kind != DOUBLE) throw new RuntimeException("Val.asDouble on kind " + kindName(kind));
        return d;
    }
    public float asFloat() { return (float) asDouble(); }

    public boolean asBoolean() {
        if (kind != BOOLEAN) throw new RuntimeException("Val.asBoolean on kind " + kindName(kind));
        return b;
    }

    /** The String payload. May be null: that is the null String, not an error. */
    public String asString() {
        if (kind != STRING) throw new RuntimeException("Val.asString on kind " + kindName(kind));
        return s;
    }

    public String thrownClass() {
        if (kind != THROWS) throw new RuntimeException("Val.thrownClass on kind " + kindName(kind));
        return s;
    }

    // ---- composition helpers -----------------------------------------------------------

    /** The first hole that throws, in left-to-right evaluation order (§15.6), or null if
     *  none does. The common shape of a strict operator's expect():
     *      Val t = Val.firstThrow(h); if (t != null) return t;
     *  Short-circuiting operators (§15.22, §15.23, §15.24) must NOT use this — they decide
     *  operand by operand, which is exactly the rule they exist to exercise. */
    public static Val firstThrow(Val[] holes) {
        for (int i = 0; i < holes.length; i++) if (holes[i].isThrows()) return holes[i];
        return null;
    }

    /** §5.1.6 string conversion: the text `"" + v` yields, which is display() for every
     *  kind except THROWS (a throw has no string conversion — check firstThrow first). */
    public String stringConversion() {
        if (kind == THROWS) throw new RuntimeException("Val.stringConversion on a THROWS");
        return display();
    }

    // ---- rendering ---------------------------------------------------------------------

    /** Exactly the bytes System.out.println emits for this value — the .expected line.
     *  For THROWS, the caught exception's fully-qualified class name, which is what
     *  Emit's catch clause prints. */
    public String display() {
        if (kind == LONG) {
            if (type.equals("char")) return String.valueOf((char) l);
            return Long.toString(l);                 // byte/short/int/long all print as digits
        }
        if (kind == DOUBLE) {
            if (type.equals("float")) return Float.toString((float) d);   // §20.9.16
            return Double.toString(d);                                    // §20.10.15
        }
        if (kind == BOOLEAN) return b ? "true" : "false";
        if (kind == STRING)  return (s == null) ? "null" : s;
        if (kind == REF)     return s;
        return s;                                    // THROWS: the class name
    }

    /** Diagnostic form — never written to a .expected file. */
    public String toString() {
        if (kind == THROWS) return "throws " + s;
        return type + " " + display();
    }

    public boolean equals(Object o) {
        if (!(o instanceof Val)) return false;
        Val v = (Val) o;
        if (v.kind != kind || !v.type.equals(type)) return false;
        if (kind == LONG)    return v.l == l;
        // Bit equality, not ==: §4.2.3 makes NaN != NaN and 0.0 == -0.0, and both of those
        // would defeat a value comparison used to check a composed expectation.
        if (kind == DOUBLE)  return Double.doubleToLongBits(v.d) == Double.doubleToLongBits(d);
        if (kind == BOOLEAN) return v.b == b;
        if (s == null) return v.s == null;
        return s.equals(v.s);
    }

    public int hashCode() { return kind * 31 + display().hashCode(); }

    private static String kindName(int k) {
        if (k == LONG)    return "LONG";
        if (k == DOUBLE)  return "DOUBLE";
        if (k == STRING)  return "STRING";
        if (k == BOOLEAN) return "BOOLEAN";
        if (k == REF)     return "REF";
        if (k == THROWS)  return "THROWS";
        return "?";
    }
}
