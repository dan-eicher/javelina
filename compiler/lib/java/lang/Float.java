package java.lang;

public final class Float extends Number {
    public static final float MIN_VALUE = 1.4e-45f;
    public static final float MAX_VALUE = 3.4028235e+38f;
    public static final float NEGATIVE_INFINITY = -1.0f/0.0f;
    public static final float POSITIVE_INFINITY = 1.0f/0.0f;
    public static final float NaN = 0.0f/0.0f;

    private float value;

    public Float(float value) { this.value = value; }
    public Float(double value) { this.value = (float)value; }
    public Float(String s) throws NumberFormatException { this.value = valueOf(s).floatValue(); }

    public int intValue()       { return (int)value; }
    public long longValue()     { return (long)value; }
    public float floatValue()   { return value; }
    public double doubleValue() { return (double)value; }

    public boolean isNaN()      { return value != value; }
    public boolean isInfinite() { return value == POSITIVE_INFINITY || value == NEGATIVE_INFINITY; }
    public static boolean isNaN(float v)      { return v != v; }
    public static boolean isInfinite(float v) { return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY; }

    // §20.9.6/.7: bit-pattern equality/hash (NaN==NaN, -0.0f≠0.0f) via the intrinsic.
    public int hashCode() { return floatToIntBits(value); }
    public boolean equals(Object obj) {
        if (obj instanceof Float) {
            return floatToIntBits(value) == floatToIntBits(((Float)obj).floatValue());
        }
        return false;
    }

    // §20.9.16: shortest round-tripping decimal (compiled via FloatingDecimal).
    public String toString() { return FloatingDecimal.toStringFloat(value); }
    public static String toString(float f) { return FloatingDecimal.toStringFloat(f); }
    public static Float valueOf(String s) throws NullPointerException, NumberFormatException {
        return new Float(FloatingDecimal.parseFloat(s));   // correctly-rounded parse (§20.9.14)
    }
    // §20.9.18: floatToIntBits collapses every NaN bit pattern to the canonical quiet NaN;
    // the raw bitcast (intBitsToFloat's inverse) is floatToRawIntBits, a Move* intrinsic.
    public static int floatToIntBits(float value) {
        int bits = floatToRawIntBits(value);
        if (((bits & 0x7F800000) == 0x7F800000) && ((bits & 0x007FFFFF) != 0)) return 0x7FC00000;
        return bits;
    }
    public static native int floatToRawIntBits(float value);   // raw IEEE-754 bitcast (MoveF2I)
    public static native float intBitsToFloat(int bits);       // raw IEEE-754 bitcast (MoveI2F)
}
