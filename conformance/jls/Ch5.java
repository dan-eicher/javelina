// Ch5 — JLS chapter 5, Conversions and Promotions.
//
// This is the chapter that states its own cardinality: §5.1.2 names "the following 19 specific
// conversions" and §5.1.3 "the following 23 specific conversions". So both are enumerated here
// in full, one assertion per conversion, in the spec's own order. A chapter that counts its
// rules is a chapter where partial coverage is visible, and 19 and 23 are the counts to meet.
//
// Several cases are the spec's OWN worked examples, transcribed with the outputs it prints:
// §5.1.2's `big - (int)approx == -46`, §5.1.3's fmin/fmax table and its six-line narrowing
// program, §5.2's f=12.0 / l=0x123 / d=1.2300000190734863, §5.6.1's three lines, and §5.6.2's
// 7 and 0.25. Those are the least arguable assertions available: the expected value is printed
// in the specification.
//
// The compile-time halves of this chapter — §5.1.7's forbidden set, §5.2's rejected narrowings,
// §5.3's overload failure — cannot be asserted by a running program, and live in
// conformance/reject/ where the artifact under test is javelinac's diagnostic.

class Ch5Point            { int x, y; }
class Ch5Point3D extends Ch5Point { int z; }
interface Ch5Colorable    { void setColor(int c); }
class Ch5Colored extends Ch5Point implements Ch5Colorable {
    int color;
    public void setColor(int c) { this.color = c; }
}
interface Ch5Sub extends Ch5Colorable { }

public class Ch5 {

    // JLS 5.1.1
    static void s5_1_1() {
        // "it is permitted for a program to include redundant cast operators for the sake of
        // clarity" — a cast to a value's own type changes nothing.
        int i = 42;
        Check.eq("5.1.1", "a redundant cast to the same primitive type", (long) (int) i, 42L);
        double d = 1.5;
        Check.eqBits("5.1.1", "…and to the same floating type", (double) d, 1.5);
        String s = "x";
        Check.same("5.1.1", "…and to the same reference type", (String) s, s);

        // "The only permitted conversion that involves the type boolean is the identity
        // conversion from boolean to boolean."
        boolean b = true;
        Check.eq("5.1.1", "boolean's only conversion is identity", (boolean) b, true);
    }

    // JLS 5.1.2
    static void s5_1_2() {
        // All 19, in the spec's order. Each is written as an ASSIGNMENT, not a cast: a widening
        // conversion is the one that needs no cast, and spelling it with one would test §5.5.
        byte b = -5;
        short s;  int i;  long l;  float f;  double d;

        s = b; Check.eq("5.1.2", "1. byte to short",   (long) s, -5L);
        i = b; Check.eq("5.1.2", "2. byte to int",     (long) i, -5L);
        l = b; Check.eq("5.1.2", "3. byte to long",    l,        -5L);
        f = b; Check.eqBits("5.1.2", "4. byte to float",  f, -5.0f);
        d = b; Check.eqBits("5.1.2", "5. byte to double", d, -5.0);

        short s2 = -300;
        i = s2; Check.eq("5.1.2", "6. short to int",   (long) i, -300L);
        l = s2; Check.eq("5.1.2", "7. short to long",  l,        -300L);
        f = s2; Check.eqBits("5.1.2", "8. short to float",  f, -300.0f);
        d = s2; Check.eqBits("5.1.2", "9. short to double", d, -300.0);

        char c = 60000;
        i = c; Check.eq("5.1.2", "10. char to int",    (long) i, 60000L);
        l = c; Check.eq("5.1.2", "11. char to long",   l,        60000L);
        f = c; Check.eqBits("5.1.2", "12. char to float",  f, 60000.0f);
        d = c; Check.eqBits("5.1.2", "13. char to double", d, 60000.0);

        int i2 = -7;
        l = i2; Check.eq("5.1.2", "14. int to long",   l, -7L);
        f = i2; Check.eqBits("5.1.2", "15. int to float",  f, -7.0f);
        d = i2; Check.eqBits("5.1.2", "16. int to double", d, -7.0);

        long l2 = -9L;
        f = l2; Check.eqBits("5.1.2", "17. long to float",  f, -9.0f);
        d = l2; Check.eqBits("5.1.2", "18. long to double", d, -9.0);

        float f2 = 0.5f;
        d = f2; Check.eqBits("5.1.2", "19. float to double", d, 0.5);

        // "A widening conversion of a signed integer value to an integral type T simply
        // sign-extends" — vs — "A widening conversion of a character to an integral type T
        // zero-extends". The pair is the whole point: same bits, opposite result.
        byte neg = (byte) 0xFF;
        char high = (char) 0xFFFF;
        Check.eq("5.1.2", "a signed integral SIGN-extends", (long) (int) neg, -1L);
        Check.eq("5.1.2", "a char ZERO-extends", (long) (int) high, 65535L);

        // "conversions widening from an integral type to another integral type and from float
        // to double do not lose any information at all"
        Check.eq("5.1.2", "long to double is exact for small magnitudes",
                 (long) (double) 1234567L, 1234567L);
        Check.eqBits("5.1.2", "float to double is always exact", (double) 0.1f, (double) 0.1f);

        // "Conversion of an int or long value to float, or of a long value to double, may
        // result in loss of precision" — the spec's own program, and its printed answer.
        int big = 1234567890;
        float approx = big;
        Check.eq("5.1.2", "the spec's loss-of-precision example prints -46",
                 (long) (big - (int) approx), -46L);

        // "widening conversions among primitive types never result in a run-time exception"
        long huge = Long.MAX_VALUE;
        double asD = huge;
        Check.eq("5.1.2", "a lossy widening still never throws", asD > 9.0e18, true);
    }

    // JLS 5.1.3
    static void s5_1_3() {
        // All 23, in the spec's order.
        byte b;  short s;  char c;  int i;  long l;  float f;

        byte  vb = -1;
        c = (char) vb;  Check.eq("5.1.3", "1. byte to char", (long) (int) c, 65535L);

        short vs = 0x1234;
        b = (byte) vs;  Check.eq("5.1.3", "2. short to byte", (long) b, 0x34L);
        c = (char) vs;  Check.eq("5.1.3", "3. short to char", (long) (int) c, 0x1234L);

        char vc = 0xF001;
        b = (byte) vc;  Check.eq("5.1.3", "4. char to byte",  (long) b, 0x01L);
        s = (short) vc; Check.eq("5.1.3", "5. char to short", (long) s, -4095L);

        int vi = 0x12345678;
        b = (byte) vi;  Check.eq("5.1.3", "6. int to byte",  (long) b, 0x78L);
        s = (short) vi; Check.eq("5.1.3", "7. int to short", (long) s, 0x5678L);
        c = (char) vi;  Check.eq("5.1.3", "8. int to char",  (long) (int) c, 0x5678L);

        long vl = 0x1122334455667788L;
        b = (byte) vl;  Check.eq("5.1.3", "9. long to byte",   (long) b, -120L);      // 0x88
        s = (short) vl; Check.eq("5.1.3", "10. long to short", (long) s, 0x7788L);
        c = (char) vl;  Check.eq("5.1.3", "11. long to char",  (long) (int) c, 0x7788L);
        i = (int) vl;   Check.eq("5.1.3", "12. long to int",   (long) i, 0x55667788L);

        float vf = 300.7f;
        b = (byte) vf;  Check.eq("5.1.3", "13. float to byte",  (long) b, 44L);
        s = (short) vf; Check.eq("5.1.3", "14. float to short", (long) s, 300L);
        c = (char) vf;  Check.eq("5.1.3", "15. float to char",  (long) (int) c, 300L);
        i = (int) vf;   Check.eq("5.1.3", "16. float to int",   (long) i, 300L);
        l = (long) vf;  Check.eq("5.1.3", "17. float to long",  l, 300L);

        double vd = -70000.9;
        b = (byte) vd;  Check.eq("5.1.3", "18. double to byte",  (long) b, -112L);    // 0x90
        s = (short) vd; Check.eq("5.1.3", "19. double to short", (long) s, -4464L);
        c = (char) vd;  Check.eq("5.1.3", "20. double to char",  (long) (int) c, 61072L);
        i = (int) vd;   Check.eq("5.1.3", "21. double to int",   (long) i, -70000L);
        l = (long) vd;  Check.eq("5.1.3", "22. double to long",  l, -70000L);
        f = (float) vd; Check.eqBits("5.1.3", "23. double to float", f, -70000.9f);

        // Step 1 of the two-step float→integral rule, verbatim: NaN yields 0; a value too small
        // yields the smallest representable; too large yields the largest.
        Check.eq("5.1.3", "NaN to int is 0", (long) (int) (0.0f / 0.0f), 0L);
        Check.eq("5.1.3", "NaN to long is 0", (long) (0.0 / 0.0), 0L);

        // The spec's fmin/fmax table, all five printed lines.
        float fmin = Float.NEGATIVE_INFINITY, fmax = Float.POSITIVE_INFINITY;
        Check.eq("5.1.3", "long: -Inf clamps to the smallest long", (long) fmin, -9223372036854775808L);
        Check.eq("5.1.3", "long: +Inf clamps to the largest long", (long) fmax, 9223372036854775807L);
        Check.eq("5.1.3", "int: -Inf clamps to the smallest int", (long) (int) fmin, -2147483648L);
        Check.eq("5.1.3", "int: +Inf clamps to the largest int", (long) (int) fmax, 2147483647L);
        Check.eq("5.1.3", "short: the low 16 bits of those, so 0", (long) (short) fmin, 0L);
        Check.eq("5.1.3", "short: …and -1", (long) (short) fmax, -1L);
        Check.eq("5.1.3", "char: the same low 16 bits, so 0", (long) (int) (char) fmin, 0L);
        Check.eq("5.1.3", "char: …and 65535", (long) (int) (char) fmax, 65535L);
        Check.eq("5.1.3", "byte: the low 8 bits, so 0", (long) (byte) fmin, 0L);
        Check.eq("5.1.3", "byte: …and -1", (long) (byte) fmax, -1L);

        // The spec's six-line narrowing program, with its printed output.
        Check.eq("5.1.3", "(short)0x12345678 == 0x5678", (long) (short) 0x12345678, 0x5678L);
        Check.eq("5.1.3", "(byte)255 == -1", (long) (byte) 255, -1L);
        Check.eq("5.1.3", "(int)1e20f == 2147483647", (long) (int) 1e20f, 2147483647L);
        Check.eq("5.1.3", "(int)Float.NaN == 0", (long) (int) Float.NaN, 0L);
        Check.eq("5.1.3", "(float)-1e100 == -Infinity",
                 (float) -1e100 == Float.NEGATIVE_INFINITY, true);
        Check.eqBits("5.1.3", "(float)1e-50 == 0.0", (float) 1e-50, 0.0f);
        // "A double NaN is always converted to a float NaN."
        Check.eq("5.1.3", "a double NaN narrows to a float NaN",
                 Float.isNaN((float) (0.0 / 0.0)), true);

        // "narrowing conversions among primitive types never result in a run-time exception":
        // 1e300 clamps to the largest int in step 1, then step 2 keeps its low 8 bits, 0xff.
        Check.eq("5.1.3", "no narrowing throws, however far out of range", (long) (byte) 1e300, -1L);
    }

    // JLS 5.1.4
    static void s5_1_4() {
        // All eight forms, in the spec's order. Each is an assignment: a widening reference
        // conversion "never require[s] a special action at run time".
        Ch5Point3D p3 = new Ch5Point3D();
        Ch5Point p = p3;                                   // class S to class T, S subclass of T
        Check.same("5.1.4", "1. class to superclass", p, p3);
        Object o = p3;                                     // ...the Object special case
        Check.same("5.1.4", "1a. any class to Object", o, p3);

        Ch5Colored col = new Ch5Colored();
        Ch5Colorable k = col;                              // class S to interface K, S implements K
        Check.eq("5.1.4", "2. class to an implemented interface", k != null, true);

        Ch5Point nc = null; Ch5Colorable nk = null; int[] na = null;  // null to class/interface/array
        Check.eq("5.1.4", "3. null to a class type", nc, null);
        Check.eq("5.1.4", "3. null to an interface type", nk, null);
        Check.eq("5.1.4", "3. null to an array type", na, null);

        Ch5Sub sub = null;
        Ch5Colorable sup = sub;                            // interface J to superinterface K
        Check.eq("5.1.4", "4. interface to superinterface", sup, null);

        Object fromFace = k;                               // interface to Object
        Check.same("5.1.4", "5. interface to Object", fromFace, col);

        int[] ia = new int[2];
        Object fromArr = ia;                               // array to Object
        Check.same("5.1.4", "6. array to Object", fromArr, ia);

        Cloneable cl = ia;                                 // array to Cloneable
        Check.eq("5.1.4", "7. every array implements Cloneable", cl != null, true);

        Ch5Point3D[] p3a = new Ch5Point3D[3];              // SC[] to TC[] when SC widens to TC
        Ch5Point[] pa = p3a;
        Check.same("5.1.4", "8. covariant arrays when the components widen", pa, p3a);

        // "therefore never throw an exception at run time" — the whole chain above ran without
        // one, and reading back through the widened type still sees the object.
        pa[0] = p3;
        Check.same("5.1.4", "a widening reference conversion is not a copy", p3a[0], p3);
    }

    // JLS 5.1.5
    static void s5_1_5() {
        // "Such conversions require a test at run time... If not, then a ClassCastException is
        // thrown." Both outcomes, for each of the listed forms that this edition allows.
        Ch5Point3D p3 = new Ch5Point3D();
        Ch5Point p = p3;
        Ch5Point3D back = (Ch5Point3D) p;                  // class to subclass, succeeding
        Check.same("5.1.5", "1. superclass to subclass when the class matches", back, p3);

        Ch5Point plain = new Ch5Point();
        try {
            Ch5Point3D bad = (Ch5Point3D) plain;
            Check.notThrown("5.1.5", "1. …and throws when it does not");
        } catch (ClassCastException e) {
            Check.thrown("5.1.5", "1. …and throws when it does not");
        }

        Object o = new Ch5Colored();
        Ch5Colorable k = (Ch5Colorable) o;                 // Object to interface, succeeding
        Check.eq("5.1.5", "4. Object to an interface it implements", k != null, true);

        Object notColorable = new Ch5Point();
        try {
            Ch5Colorable bad = (Ch5Colorable) notColorable;
            Check.notThrown("5.1.5", "4. …and throws when it does not implement it");
        } catch (ClassCastException e) {
            Check.thrown("5.1.5", "4. …and throws when it does not implement it");
        }

        Object arr = new int[2];
        int[] ia = (int[]) arr;                            // Object to array type, succeeding
        Check.eq("5.1.5", "3. Object to an array type", ia.length, 2);
        try {
            long[] bad = (long[]) arr;
            Check.notThrown("5.1.5", "3. …and throws for the wrong component type");
        } catch (ClassCastException e) {
            Check.thrown("5.1.5", "3. …and throws for the wrong component type");
        }

        // The spec's own example: a Point[] cast to ColoredPoint[] throws even though the
        // element type is castable, because the ARRAY's class is not.
        Ch5Point[] pa = new Ch5Point[4];
        try {
            Ch5Colored[] cpa = (Ch5Colored[]) pa;
            Check.notThrown("5.1.5", "8. the spec's Point[] to ColoredPoint[] example throws");
        } catch (ClassCastException e) {
            Check.thrown("5.1.5", "8. the spec's Point[] to ColoredPoint[] example throws");
        }
        // ...and its companion: an int[] seen as Object cast to an interface it cannot have.
        Object shortvec = new int[2];
        try {
            Ch5Colorable c = (Ch5Colorable) shortvec;
            Check.notThrown("5.1.5", "the spec's (Colorable)int[] example throws");
        } catch (ClassCastException e) {
            Check.thrown("5.1.5", "the spec's (Colorable)int[] example throws");
        }

        // The covariant array cast that SUCCEEDS: the run-time class really is the subtype.
        Ch5Point[] fromSub = new Ch5Point3D[2];
        Ch5Point3D[] ok = (Ch5Point3D[]) fromSub;
        Check.same("5.1.5", "8. …and succeeds when the run-time array class matches", ok, fromSub);

        // "null always passes" (§5.5) — a narrowing cast of null is not a test at all.
        Ch5Point nothing = null;
        Ch5Point3D castNull = (Ch5Point3D) nothing;
        Check.eq("5.1.5", "casting null never throws", castNull, null);
    }

    // JLS 5.1.6
    static void s5_1_6() {
        // "There is a string conversion to type String from every other type, including the
        // null type." Every one of them, so "every other type" is not taken on trust.
        byte b = -1; short s = -2; char c = 'q'; int i = -3; long l = -4L;
        float f = 1.5f; double d = 2.5; boolean z = true; Object nul = null;
        Check.eq("5.1.6", "from byte",    "" + b, "-1");
        Check.eq("5.1.6", "from short",   "" + s, "-2");
        Check.eq("5.1.6", "from char",    "" + c, "q");
        Check.eq("5.1.6", "from int",     "" + i, "-3");
        Check.eq("5.1.6", "from long",    "" + l, "-4");
        Check.eq("5.1.6", "from float",   "" + f, "1.5");
        Check.eq("5.1.6", "from double",  "" + d, "2.5");
        Check.eq("5.1.6", "from boolean", "" + z, "true");
        Check.eq("5.1.6", "from a reference type", "" + "s", "s");
        Check.eq("5.1.6", "from the NULL TYPE", "" + nul, "null");
        Check.eq("5.1.6", "…including a null String", "" + (String) null, "null");
    }

    // JLS 5.2
    static void s5_2() {
        // "a narrowing primitive conversion may be used if all of the following conditions are
        // satisfied: the expression is a constant expression of type int; the type of the
        // variable is byte, short, or char; the value is representable in the type."
        byte theAnswer = 42;                    // the spec's own line
        Check.eq("5.2", "an int constant narrows implicitly to byte", (long) theAnswer, 42L);
        byte cast = (byte) 42;                  // "cast is permitted but not required"
        Check.eq("5.2", "…and the cast form means the same", (long) cast, 42L);
        short sh = 12345;
        Check.eq("5.2", "…to short", (long) sh, 12345L);
        char ch = 65;
        Check.eq("5.2", "…to char", (long) (int) ch, 65L);
        byte edgeLo = -128, edgeHi = 127;       // the representable boundary, both ends
        Check.eq("5.2", "…at the low bound", (long) edgeLo, -128L);
        Check.eq("5.2", "…at the high bound", (long) edgeHi, 127L);
        // A constant EXPRESSION, not merely a literal (§15.27).
        byte folded = 40 + 2;
        Check.eq("5.2", "the rule takes a constant EXPRESSION, not just a literal",
                 (long) folded, 42L);

        // The spec's assignment-conversion program, with its three printed lines.
        short s = 12;
        float f = s;                            // widen short to float
        Check.eq("5.2", "the spec's example prints f=12.0", "f=" + f, "f=12.0");
        char c = 'ģ';                           // U+0123, the spec's value
        long l = c;                             // widen char to long
        Check.eq("5.2", "…and l=0x123", "l=0x" + Long.toString(l, 16), "l=0x123");
        f = 1.23f;
        double d = f;                           // widen float to double
        Check.eq("5.2", "…and d=1.2300000190734863", "d=" + d, "d=1.2300000190734863");

        // "A value of the null type may be assigned to any reference type."
        Ch5Point p = null;
        Check.eq("5.2", "null assigns to any reference type", p, null);

        // "An assignment conversion never causes an exception. (Note, however, that an
        // assignment may result in an exception in a special case involving array elements.)"
        Ch5Point[] pa = new Ch5Point3D[2];
        try {
            pa[0] = new Ch5Point();
            Check.notThrown("5.2", "the array-element special case throws ArrayStoreException");
        } catch (ArrayStoreException e) {
            Check.thrown("5.2", "the array-element special case throws ArrayStoreException");
        }
    }

    static int m5_3(long a) { return 1; }
    static int m5_3(double a) { return 2; }

    // JLS 5.3
    static void s5_3() {
        // "Method invocation contexts allow the use of an identity conversion, a widening
        // primitive conversion, or a widening reference conversion." All three, at a call.
        Check.eq("5.3", "identity at a call site", (long) m5_3(1L), 1L);
        byte b = 3;
        Check.eq("5.3", "widening primitive at a call site (byte to long)", (long) m5_3(b), 1L);
        Check.eq("5.3", "…and the float ladder picks the double overload", (long) m5_3(1.0f), 2L);
        Check.eq("5.3", "a char widens at a call site too", (long) m5_3('a'), 1L);

        // A widening REFERENCE conversion at a call site.
        Check.eq("5.3", "widening reference at a call site", nameOf(new Ch5Colored()), "Ch5Colored");

        // The exclusion — "Method invocation conversions specifically do not include the
        // implicit narrowing of integer constants" — is a compile-time error, so the spec's
        // m(12, 2) example lives in conformance/reject/.
    }

    static String nameOf(Object o) { return o.getClass().getName(); }

    // JLS 5.4
    static void s5_4() {
        // "String conversion applies only to the operands of the binary + operator when one of
        // the arguments is a String."
        Check.eq("5.4", "String on the left converts the right operand", "n=" + 5, "n=5");
        Check.eq("5.4", "String on the right converts the left operand", 5 + "=n", "5=n");
        // ...and when NEITHER is a String, + is arithmetic, not concatenation.
        Check.eq("5.4", "with no String operand, + stays arithmetic", (long) (5 + 3), 8L);
        Check.eq("5.4", "…even for chars, which promote to int", (long) ('a' + 1), 98L);
        // Left-to-right grouping makes the difference observable in one expression.
        Check.eq("5.4", "(1+2)+\"s\" adds first", 1 + 2 + "s", "3s");
        Check.eq("5.4", "\"s\"+1+2 concatenates twice", "s" + 1 + 2, "s12");
        // "a new String which is the concatenation of the two strings is the result"
        String a = "ab";
        Check.eq("5.4", "the result carries both operands", a + "cd", "abcd");
        Check.eq("5.4", "…and the operand is unchanged", a, "ab");
    }

    // JLS 5.5
    static void s5_5() {
        // "Casting contexts allow the use of an identity conversion, a widening primitive
        // conversion, a narrowing primitive conversion, a widening reference conversion, or a
        // narrowing reference conversion" — the five, at a cast.
        int i = 5;
        Check.eq("5.5", "1. identity", (long) (int) i, 5L);
        Check.eq("5.5", "2. widening primitive", (long) (long) i, 5L);
        Check.eq("5.5", "3. narrowing primitive", (long) (byte) 300, 44L);
        Ch5Point3D p3 = new Ch5Point3D();
        Check.same("5.5", "4. widening reference", (Ch5Point) p3, p3);
        Ch5Point p = p3;
        Check.same("5.5", "5. narrowing reference", (Ch5Point3D) p, p3);

        // "a cast can do any permitted conversion other than a string conversion" — so a cast
        // does NOT stand in for §5.4: (String) of a non-String reference is a reference cast
        // that must fail its run-time test rather than stringify.
        Object o = new Ch5Point();
        try {
            String s = (String) o;
            Check.notThrown("5.5", "a cast is not a string conversion");
        } catch (ClassCastException e) {
            Check.thrown("5.5", "a cast is not a string conversion");
        }

        // A double cast is two conversions, and the intermediate type is observable.
        Check.eq("5.5", "(int)(byte)300 keeps the narrowing", (long) (int) (byte) 300, 44L);
        Check.eq("5.5", "(int)(char)-1 keeps the zero extension", (long) (int) (char) -1, 65535L);
    }

    // JLS 5.6
    static void s5_6() {
        // "Numeric promotion contexts allow the use of an identity conversion or a widening
        // primitive conversion" — so an arithmetic operand is never NARROWED implicitly. The
        // observable consequence: byte + byte is an int expression, which is why it cannot be
        // stored back into a byte without a cast (that half is in conformance/reject/).
        byte a = 100, b = 100;
        int sum = a + b;
        Check.eq("5.6", "byte + byte is an int expression and does not wrap", (long) sum, 200L);
        short s1 = 30000, s2 = 30000;
        Check.eq("5.6", "short + short likewise", (long) (s1 + s2), 60000L);
        char c1 = 60000, c2 = 60000;
        Check.eq("5.6", "char + char likewise, and zero-extends first", (long) (c1 + c2), 120000L);
    }

    // JLS 5.6.1
    static void s5_6_1() {
        // "If the operand is of compile-time type byte, short, or char, unary numeric promotion
        // promotes it to a value of type int" — in each of the five listed situations.
        byte b = 2;
        int[] a = new int[b];                        // 1. the dimension expression in a creation
        Check.eq("5.6.1", "1. an array dimension promotes to int", a.length, 2);

        char c = 1;                                  // U+0001, the spec's value
        a[c] = 1;                                    // 2. the index expression in an access
        Check.eq("5.6.1", "2. an array index promotes to int", (long) a[1], 1L);

        a[0] = -c;                                   // 3. unary minus
        Check.eq("5.6.1", "3. unary - promotes to int", (long) a[0], -1L);
        Check.eq("5.6.1", "3. unary + promotes to int", (long) (+c), 1L);

        b = -1;
        int i = ~b;                                  // 4. the bitwise complement
        Check.eq("5.6.1", "4. ~ promotes to int before complementing", (long) i, 0L);

        i = b << 4L;                                 // 5. each shift operand, SEPARATELY
        Check.eq("5.6.1", "5. a long shift distance does not widen the left operand",
                 (long) i, -16L);                    // 0xfffffff0, still an int

        // The spec prints exactly these three lines for that program.
        Check.eq("5.6.1", "the spec's example prints a: -1,1", "a: " + a[0] + "," + a[1], "a: -1,1");
        Check.eq("5.6.1", "…and ~0xffffffff==0x0",
                 "~0x" + Integer.toHexString(b) + "==0x" + Integer.toHexString(~b),
                 "~0xffffffff==0x0");
        Check.eq("5.6.1", "…and 0xffffffff<<4L==0xfffffff0",
                 "0x" + Integer.toHexString(b) + "<<4L==0x" + Integer.toHexString(b << 4L),
                 "0xffffffff<<4L==0xfffffff0");

        // "Otherwise, a unary numeric operand remains as is and is not converted."
        long l = 5L;
        Check.eq("5.6.1", "a long operand is NOT promoted", -l, -5L);
        Check.eqBits("5.6.1", "…nor is a float", -1.5f, -1.5f);
    }

    // JLS 5.6.2
    static void s5_6_2() {
        // The ladder, in the spec's order: double beats float beats long beats int.
        int i = 1;
        long l = 1L;
        float f = 1.0f;
        double d = 1.0;

        // "If either operand is of type double, the other is converted to double."
        Check.eqBits("5.6.2", "1. int op double is double", i / 2.0, 0.5);
        Check.eqBits("5.6.2", "1. …and double op float", d + f, 2.0);
        // "Otherwise, if either operand is of type float, the other is converted to float."
        Check.eqBits("5.6.2", "2. long op float is float", l / 2.0f, 0.5f);
        // "Otherwise, if either operand is of type long, the other is converted to long."
        Check.eq("5.6.2", "3. int op long is long", 1 + 9000000000L, 9000000001L);
        // "Otherwise, both operands are converted to type int."
        byte b = 1; short s = 1; char c = 1;
        Check.eq("5.6.2", "4. byte op short is int", (long) (b + s), 2L);
        Check.eq("5.6.2", "4. char op byte is int", (long) (c + b), 2L);

        // The listed operator categories, each shown promoting.
        Check.eqBits("5.6.2", "* / % promote", 3 * 0.5, 1.5);
        Check.eqBits("5.6.2", "+ - promote", 3 - 0.5, 2.5);
        Check.eq("5.6.2", "< <= > >= promote", 1 < 1.5, true);
        Check.eq("5.6.2", "== != promote", 1 == 1.0, true);
        Check.eq("5.6.2", "& ^ | promote", (long) (1 & 3L), 1L);
        Check.eqBits("5.6.2", "?: promotes in certain cases", true ? 1 : 2.0, 1.0);

        // The spec's own program, and both printed lines.
        int i0 = 0;
        float f1 = 1.0f;
        double d2 = 2.0;
        Check.eq("5.6.2", "i*f promotes to float, then float==double to double",
                 (i0 * f1) == d2, false);
        byte bb = 0x1f;
        char cc = 'G';
        int control = cc & bb;
        Check.eq("5.6.2", "the spec's char&byte example prints 7",
                 Integer.toHexString(control), "7");
        float ff = (bb == 0) ? f1 : 4.0f;
        Check.eq("5.6.2", "…and 1.0/f prints 0.25", "" + (1.0 / ff), "0.25");
    }

    public static void run() {
        try { s5_1_1(); } catch (Throwable t) { Check.crashed("5.1.1", t); }
        try { s5_1_2(); } catch (Throwable t) { Check.crashed("5.1.2", t); }
        try { s5_1_3(); } catch (Throwable t) { Check.crashed("5.1.3", t); }
        try { s5_1_4(); } catch (Throwable t) { Check.crashed("5.1.4", t); }
        try { s5_1_5(); } catch (Throwable t) { Check.crashed("5.1.5", t); }
        try { s5_1_6(); } catch (Throwable t) { Check.crashed("5.1.6", t); }
        try { s5_2();   } catch (Throwable t) { Check.crashed("5.2",   t); }
        try { s5_3();   } catch (Throwable t) { Check.crashed("5.3",   t); }
        try { s5_4();   } catch (Throwable t) { Check.crashed("5.4",   t); }
        try { s5_5();   } catch (Throwable t) { Check.crashed("5.5",   t); }
        try { s5_6();   } catch (Throwable t) { Check.crashed("5.6",   t); }
        try { s5_6_1(); } catch (Throwable t) { Check.crashed("5.6.1", t); }
        try { s5_6_2(); } catch (Throwable t) { Check.crashed("5.6.2", t); }
    }
}
