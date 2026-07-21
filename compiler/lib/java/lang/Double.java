package java.lang;

public final class Double extends Number {
    public static final double MIN_VALUE = 5e-324;
    public static final double MAX_VALUE = 1.7976931348623157e+308;
    public static final double NEGATIVE_INFINITY = -1.0/0.0;
    public static final double POSITIVE_INFINITY = 1.0/0.0;
    public static final double NaN = 0.0/0.0;

    private double value;

    public Double(double value) { this.value = value; }
    public Double(String s) throws NumberFormatException { this.value = valueOf(s).doubleValue(); }

    public int intValue()       { return (int)value; }
    public long longValue()     { return (long)value; }
    public float floatValue()   { return (float)value; }
    public double doubleValue() { return value; }

    public boolean isNaN()      { return value != value; }
    public boolean isInfinite() { return value == POSITIVE_INFINITY || value == NEGATIVE_INFINITY; }
    public static boolean isNaN(double v)      { return v != v; }
    public static boolean isInfinite(double v) { return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY; }

    // §20.10.6/.7: bit-pattern equality/hash (NaN==NaN, -0.0≠0.0) via the intrinsic.
    public int hashCode() {
        long bits = doubleToLongBits(value);
        return (int)(bits ^ (bits >>> 32));
    }
    public boolean equals(Object obj) {
        if (obj instanceof Double) {
            return doubleToLongBits(value) == doubleToLongBits(((Double)obj).doubleValue());
        }
        return false;
    }

    // §20.10.15: shortest round-tripping decimal (compiled via FloatingDecimal).
    public String toString() { return FloatingDecimal.toStringDouble(value); }
    public static String toString(double d) { return FloatingDecimal.toStringDouble(d); }
    public static Double valueOf(String s) throws NullPointerException, NumberFormatException {
        return new Double(FloatingDecimal.parseDouble(s));   // correctly-rounded parse (§20.10.13)
    }
    // §20.10.17: doubleToLongBits collapses every NaN bit pattern to the canonical quiet NaN;
    // the raw bitcast (longBitsToDouble's inverse) is doubleToRawLongBits, a Move* intrinsic.
    public static long doubleToLongBits(double value) {
        long bits = doubleToRawLongBits(value);
        if (((bits & 0x7FF0000000000000L) == 0x7FF0000000000000L) && ((bits & 0x000FFFFFFFFFFFFFL) != 0L))
            return 0x7FF8000000000000L;
        return bits;
    }
    public static native long doubleToRawLongBits(double value);   // raw IEEE-754 bitcast (MoveD2L)
    public static native double longBitsToDouble(long bits);       // raw IEEE-754 bitcast (MoveL2D)
}
