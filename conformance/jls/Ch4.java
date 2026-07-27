// Ch4 — JLS chapter 4, Types, Values, and Variables. One method per leaf section; the
// `// JLS <n>` markers are read by conformance/join-ledger.sh.
//
// The behaviour each method asserts is the one the ledger records for that section, which came
// from reading the section — not from what happened to be convenient to write.

interface Ch4Face { int face(); }
class Ch4Impl implements Ch4Face { public int face() { return 7; } }
class Ch4Box { int n; Ch4Box(int n) { this.n = n; } }

public class Ch4 {

    // JLS 4.1
    static void s4_1() {
        // The null reference is the only value of the null type, and it is assignable to — and
        // castable to — every reference type.
        String s = null;
        Object o = null;
        int[] a = null;
        Ch4Face f = null;
        Check.eq("4.1", "null is assignable to a class type", s, null);
        Check.eq("4.1", "null is assignable to an array type", a, null);
        Check.eq("4.1", "null is assignable to an interface type", f, null);
        Check.same("4.1", "a cast of null to any reference type yields null", (String) null, o);
    }

    // JLS 4.2
    static void s4_2() {
        // Primitive values share no state: assigning one variable to another copies the VALUE,
        // so a later change to the source cannot be seen through the destination.
        int a = 1;
        int b = a;
        a = 99;
        Check.eq("4.2", "a primitive assignment copies the value, it does not alias", b, 1);
        // boolean has exactly two values.
        boolean t = true, ff = false;
        Check.eq("4.2", "boolean has exactly two values", t != ff, true);
    }

    // JLS 4.2.1
    static void s4_2_1() {
        // Spelled as literals, not as Byte.MIN_VALUE / Short.MIN_VALUE: §20 of THIS spec has no
        // java.lang.Byte and no java.lang.Short — they arrive in 1.1. The range is the rule; the
        // wrapper class is not.
        byte bmin = -128, bmax = 127;
        Check.eq("4.2.1", "byte is -128..127", (long) bmin, -128L);
        Check.eq("4.2.1", "byte is -128..127", (long) bmax, 127L);
        Check.eq("4.2.1", "byte wraps at its bounds", (long) (byte) (bmax + 1), -128L);
        short smin = -32768, smax = 32767;
        Check.eq("4.2.1", "short is -32768..32767", (long) smin, -32768L);
        Check.eq("4.2.1", "short is -32768..32767", (long) smax, 32767L);
        Check.eq("4.2.1", "short wraps at its bounds", (long) (short) (smax + 1), -32768L);
        Check.eq("4.2.1", "int is -2^31..2^31-1", (long) Integer.MIN_VALUE, -2147483648L);
        Check.eq("4.2.1", "int is -2^31..2^31-1", (long) Integer.MAX_VALUE, 2147483647L);
        Check.eq("4.2.1", "long is -2^63..2^63-1", Long.MIN_VALUE, -9223372036854775808L);
        Check.eq("4.2.1", "long is -2^63..2^63-1", Long.MAX_VALUE, 9223372036854775807L);
        // char is UNSIGNED — the one integral type that is, and the reason it is not just a short.
        Check.eq("4.2.1", "char is 0..65535, unsigned", (long) (int) Character.MIN_VALUE, 0L);
        Check.eq("4.2.1", "char is 0..65535, unsigned", (long) (int) Character.MAX_VALUE, 65535L);
    }

    // JLS 4.2.2
    static void s4_2_2() {
        // Integer arithmetic is 32-bit unless an operand is long, and overflow WRAPS silently —
        // there is no exception and no saturation.
        int max = Integer.MAX_VALUE;
        Check.eq("4.2.2", "int overflow wraps silently", (long) (max + 1), (long) Integer.MIN_VALUE);
        Check.eq("4.2.2", "int arithmetic stays 32-bit", (long) (1000000 * 1000000), -727379968L);
        Check.eq("4.2.2", "one long operand makes it 64-bit", 1000000L * 1000000L, 1000000000000L);
        Check.eq("4.2.2", "long overflow wraps too", Long.MAX_VALUE + 1L, Long.MIN_VALUE);

        // The two exceptions to "no exception": integer division and remainder by zero.
        try { int x = 1 / 0; Check.notThrown("4.2.2", "int / 0 throws ArithmeticException"); }
        catch (ArithmeticException e) { Check.thrown("4.2.2", "int / 0 throws ArithmeticException"); }
        try { int x = 1 % 0; Check.notThrown("4.2.2", "int % 0 throws ArithmeticException"); }
        catch (ArithmeticException e) { Check.thrown("4.2.2", "int % 0 throws ArithmeticException"); }

        // §4.2.2's named special case: MIN_VALUE / -1 overflows and wraps to MIN_VALUE.
        Check.eq("4.2.2", "MIN_VALUE / -1 wraps rather than throwing",
                 (long) (Integer.MIN_VALUE / -1), (long) Integer.MIN_VALUE);
    }

    // JLS 4.2.3
    static void s4_2_3() {
        // IEEE 754. Positive and negative zero COMPARE equal but are distinguishable by the
        // sign of the infinity they produce under division — the classic pair.
        double pz = 0.0, nz = -0.0;
        Check.eq("4.2.3", "0.0 == -0.0 is true", pz == nz, true);
        Check.eq("4.2.3", "but 1.0/0.0 is +Inf", 1.0 / pz == Double.POSITIVE_INFINITY, true);
        Check.eq("4.2.3", "and 1.0/-0.0 is -Inf", 1.0 / nz == Double.NEGATIVE_INFINITY, true);

        // NaN is UNORDERED: every comparison with it is false, including with itself, which is
        // why x != x is the classic NaN test.
        double nan = 0.0 / 0.0;
        Check.eq("4.2.3", "NaN != NaN is true (NaN is unordered)", nan != nan, true);
        Check.eq("4.2.3", "NaN == NaN is false", nan == nan, false);
        Check.eq("4.2.3", "NaN < 1.0 is false", nan < 1.0, false);
        Check.eq("4.2.3", "NaN > 1.0 is also false", nan > 1.0, false);
        Check.eq("4.2.3", "Double.isNaN agrees", Double.isNaN(nan), true);
    }

    // JLS 4.2.4
    static void s4_2_4() {
        // A binary float op promotes to double if either operand is double (§5.6.2).
        float f = 1.5f;
        double d = f * 2.0;               // float * double -> double
        Check.eqBits("4.2.4", "float op double promotes to double", d, 3.0);
        Check.eqBits("4.2.4", "float op float stays float", f * 2.0f, 3.0f);
        // Overflow produces a signed infinity rather than an exception.
        Check.eq("4.2.4", "float overflow yields infinity, not an exception",
                 Float.MAX_VALUE * 2.0f == Float.POSITIVE_INFINITY, true);
        // Underflow is gradual, not flush-to-zero.
        Check.eq("4.2.4", "underflow is gradual, not flush-to-zero",
                 Double.MIN_VALUE / 2.0 == 0.0, true);
    }

    // JLS 4.2.5
    static void s4_2_5() {
        boolean b = true;
        int n = 0;
        if (b) n += 1;
        while (b) { n += 2; b = false; }
        do { n += 4; } while (b);
        for (; !b; ) { n += 8; b = true; }
        Check.eq("4.2.5", "boolean drives if/while/do/for", n, 15);
        Check.eq("4.2.5", "and the conditional operator", (n == 15) ? 1 : 2, 1);
        // §5.4 string conversion of a boolean is "true"/"false".
        Check.eq("4.2.5", "string conversion of a boolean", "" + true, "true");
        Check.eq("4.2.5", "string conversion of a boolean", "" + false, "false");
    }

    // JLS 4.3
    static void s4_3() {
        // A variable may be declared with a class, an interface, or an array type.
        Ch4Box byClass = new Ch4Box(1);
        Ch4Face byFace = new Ch4Impl();
        int[] byArray = new int[2];
        Check.eq("4.3", "a class-typed variable", byClass.n, 1);
        Check.eq("4.3", "an interface-typed variable", byFace.face(), 7);
        Check.eq("4.3", "an array-typed variable", byArray.length, 2);
    }

    // JLS 4.3.1
    static void s4_3_1() {
        // References ALIAS: two references to one object see each other's mutations. This is the
        // difference from §4.2's primitives, and the reason it is stated separately.
        Ch4Box a = new Ch4Box(1);
        Ch4Box b = a;
        a.n = 42;
        Check.eq("4.3.1", "mutation through one reference is seen through another", b.n, 42);
        Check.same("4.3.1", "…because they are the same object", a, b);
        Ch4Box c = new Ch4Box(42);
        Check.notSame("4.3.1", "equal contents do not make one object", a, c);
    }

    // JLS 4.3.2
    static void s4_3_2() {
        // Every class AND every array inherits Object's methods.
        Ch4Box o = new Ch4Box(3);
        int[] arr = new int[1];
        Check.isTrue("4.3.2", "a class instance has getClass", o.getClass() != null);
        Check.isTrue("4.3.2", "an ARRAY has getClass too", arr.getClass() != null);
        Check.eq("4.3.2", "equals is reflexive by identity by default", o.equals(o), true);
        Check.eq("4.3.2", "hashCode is stable across calls", o.hashCode() == o.hashCode(), true);
        Check.isTrue("4.3.2", "toString is non-null", o.toString() != null);
        Object asObject = arr;
        Check.same("4.3.2", "an array is assignable to Object", asObject, arr);
    }

    // JLS 4.3.3
    static void s4_3_3() {
        // A String's value never changes: every "mutating" operation returns a new String.
        String s = "abc";
        String up = s.toUpperCase();
        Check.eq("4.3.3", "the original is unchanged by toUpperCase", s, "abc");
        Check.eq("4.3.3", "and the result is the new value", up, "ABC");
        Check.notSame("4.3.3", "…a distinct object", s, up);
        Check.eq("4.3.3", "concatenation does not mutate either operand", s.concat("d"), "abcd");
        Check.eq("4.3.3", "the operand is still itself", s, "abc");
    }

    // JLS 4.3.4
    static void s4_3_4() {
        // Type identity: the same fully-qualified name is the same type, and two array types are
        // the same iff their component types are.
        int[] a = new int[1], b = new int[9];
        long[] c = new long[1];
        Check.same("4.3.4", "same component type = same array type", a.getClass(), b.getClass());
        Check.notSame("4.3.4", "different component type = different array type",
                      a.getClass(), c.getClass());
        String s1 = "x";
        Object s2 = new StringBuffer().append("y").toString();
        Check.same("4.3.4", "the same class name is the same Class object",
                   s1.getClass(), s2.getClass());
    }

    // JLS 4.5.1
    static void s4_5_1() {
        // A primitive variable holds a value of EXACTLY its declared type — a narrowing cast
        // keeps no residue of the wider value.
        byte b = (byte) 200;
        Check.eq("4.5.1", "(byte)200 is -56, and stays -56", (long) b, -56L);
        short sh = (short) 70000;
        Check.eq("4.5.1", "(short)70000 wraps to 4464", (long) sh, 4464L);
        char c = (char) -1;
        Check.eq("4.5.1", "(char)-1 is 65535 (char is unsigned)", (long) (int) c, 65535L);
        int i = (int) 3.99;
        Check.eq("4.5.1", "(int)3.99 truncates toward zero", (long) i, 3L);
        Check.eq("4.5.1", "(int)-3.99 truncates toward zero too", (long) (int) -3.99, -3L);
    }

    // JLS 4.5.2
    static void s4_5_2() {
        // A reference variable holds null, or a reference to an object of an assignment-
        // compatible class.
        Ch4Face f = null;
        Check.eq("4.5.2", "a reference variable may hold null", f, null);
        f = new Ch4Impl();
        Check.eq("4.5.2", "…or any implementing instance", f.face(), 7);
        Object o = f;
        Check.same("4.5.2", "widening to Object keeps identity", o, f);
    }

    // JLS 4.5.4
    static void s4_5_4() {
        // Fields and array components get type DEFAULTS before any use. Locals do not (they must
        // be definitely assigned, §16), which is why this is tested through arrays and fields.
        int[] i = new int[1];
        long[] l = new long[1];
        float[] f = new float[1];
        double[] d = new double[1];
        char[] c = new char[1];
        boolean[] z = new boolean[1];
        String[] s = new String[1];
        Check.eq("4.5.4", "int defaults to 0", (long) i[0], 0L);
        Check.eq("4.5.4", "long defaults to 0L", l[0], 0L);
        Check.eqBits("4.5.4", "float defaults to 0.0f", f[0], 0.0f);
        Check.eqBits("4.5.4", "double defaults to 0.0d", d[0], 0.0);
        Check.eq("4.5.4", "char defaults to the null character", (long) (int) c[0], 0L);
        Check.eq("4.5.4", "boolean defaults to false", z[0], false);
        Check.eq("4.5.4", "a reference defaults to null", s[0], null);
    }

    // JLS 4.5.5
    static void s4_5_5() {
        // A variable's declared TYPE is not the object's CLASS: an interface-typed variable holds
        // an implementing instance, and an array's class name is the descriptor form.
        // Via Object, not via the interface: §9.2 says an interface's members are only the ones
        // it declares and inherits from superinterfaces, so an interface-typed expression has no
        // getClass of its own on this edition of the spec.
        Ch4Face f = new Ch4Impl();
        Object asObj = f;
        Check.eq("4.5.5", "an interface variable holds an implementing class instance",
                 asObj.getClass().getName(), "Ch4Impl");
        int[] ia = new int[1];
        Check.eq("4.5.5", "an int[]'s class is named \"[I\"", ia.getClass().getName(), "[I");
        Object o = "text";
        Check.eq("4.5.5", "an Object variable reports the runtime class",
                 o.getClass().getName(), "java.lang.String");
    }

    public static void run() {
        s4_1(); s4_2(); s4_2_1(); s4_2_2(); s4_2_3(); s4_2_4(); s4_2_5();
        s4_3(); s4_3_1(); s4_3_2(); s4_3_3(); s4_3_4();
        s4_5_1(); s4_5_2(); s4_5_4(); s4_5_5();
    }
}
