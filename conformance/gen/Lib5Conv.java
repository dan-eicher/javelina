// Lib5Conv — JLS §5.4 and §5.5, the two chapter 5 sections carried only by
// conformance/jls/Ch5.java. Written from the page (p.67), not ported from that class.
public class Lib5Conv {

    private Lib5Conv() {}

    public static void install(Registry r) {
        r.register(new Sn5StringConvOnly());
        r.register(new Sn5CastFive());
    }
}

/** §5.4, p.67: "String conversion applies ONLY to the operands of the binary + operator when
 *  one of the arguments is a String. In this single special case, the other argument to the +
 *  is converted to a String, and a new String which is the concatenation of the two strings is
 *  the result of the +."
 *
 *  The load-bearing word is "only", and one expression cannot demonstrate it -- you need the
 *  contrast. All three below are the SAME operands with different grouping:
 *
 *      a + 2         no String operand at either +  -> arithmetic
 *      "" + a + 2    left-associative, so ("" + a) is a String and the second + converts 2
 *      a + 2 + ""    the first + is arithmetic; only the second sees a String
 *
 *  So string conversion is decided per-operator by that operator's own operands, not by
 *  whether a String appears anywhere in the expression. An implementation that made the whole
 *  expression "stringy" would print the same thing three times. */
class Sn5StringConvOnly implements Snippet {
    public String   id()        { return "t5.strconv.only.binary.plus"; }
    public String[] sections()  { return Strs.of("5.4", "15.17.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int a = " + h[0] + ";"
             + " System.out.println((a + 2) + \"|\" + (\"\" + a + 2) + \"|\" + (a + 2 + \"\")); }";
    }

    public Val expect(Val[] h) {
        int a = h[0].asInt();
        // The middle one is DIGIT CONCATENATION, not addition: "" + a gives a's digits, then
        // + 2 appends the character 2. The outer two are the arithmetic sum.
        return Val.ofString((a + 2) + "|" + a + "2|" + (a + 2));
    }
}

/** §5.5, p.67: "Casting contexts allow the use of an identity conversion (§5.1.1), a widening
 *  primitive conversion (§5.1.2), a narrowing primitive conversion (§5.1.3), a widening
 *  reference conversion (§5.1.4), or a narrowing reference conversion (§5.1.5)."
 *
 *  All five in one program, one per line, each contributing to the printed total. The forbidden
 *  half of the same section -- "a value of a primitive type cannot be cast to a reference type
 *  ..., nor can a value of a reference type be cast to a primitive type", and "a cast can do any
 *  permitted conversion OTHER THAN a string conversion" -- is already pinned by §5.1.7's
 *  rejection templates in Lib5. */
class Sn5CastFive implements Snippet {
    public String   id()        { return "t5.cast.five.conversions"; }
    public String[] sections()  { return Strs.of("5.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int identity = (int) (" + h[0] + ");"                 // §5.1.1
             + " long widenPrim = (long) identity;"                     // §5.1.2
             + " byte narrowPrim = (byte) 300;"                         // §5.1.3 -> 44
             + " Object widenRef = (Object) \"s\";"                     // §5.1.4
             + " String narrowRef = (String) widenRef;"                 // §5.1.5
             + " System.out.println(identity + widenPrim + narrowPrim"
             + " + narrowRef.length()); }";
    }

    // (byte) 300 keeps the low 8 bits: 300 - 256 = 44. narrowRef.length() is 1.
    public Val expect(Val[] h) { return Val.ofLong(2L * h[0].asInt() + 45L); }
}
