package java.lang;

public final class Long extends Number {
    public static final long MIN_VALUE = 0x8000000000000000L;
    public static final long MAX_VALUE = 0x7fffffffffffffffL;

    private long value;

    public Long(long value) { this.value = value; }
    public Long(String s) throws NumberFormatException { this.value = parseLong(s, 10); }

    public int intValue()       { return (int)value; }
    public long longValue()     { return value; }
    public float floatValue()   { return (float)value; }
    public double doubleValue() { return (double)value; }

    public int hashCode() { return (int)(value ^ (value >>> 32)); }

    public boolean equals(Object obj) {
        if (obj instanceof Long) {
            return value == ((Long)obj).longValue();
        }
        return false;
    }

    public static long parseLong(String s) throws NumberFormatException {
        return parseLong(s, 10);
    }

    /* §20.8 parseLong — JLS algorithm: accumulate negatively (covers MIN_VALUE),
     * per-step overflow guards. */
    public static long parseLong(String s, int radix) throws NumberFormatException {
        long result = 0;
        boolean negative = false;
        int i = 0;
        int max = s.length();
        long limit;
        long multmin;
        int digit;
        if (max > 0) {
            if (s.charAt(0) == '-') {
                negative = true;
                limit = MIN_VALUE;
                i++;
            } else {
                limit = -MAX_VALUE;
            }
            multmin = limit / radix;
            if (i < max) {
                digit = Character.digit(s.charAt(i++), radix);
                if (digit < 0) {
                    throw new NumberFormatException(s);
                } else {
                    result = -digit;
                }
            }
            while (i < max) {
                digit = Character.digit(s.charAt(i++), radix);
                if (digit < 0) {
                    throw new NumberFormatException(s);
                }
                if (result < multmin) {
                    throw new NumberFormatException(s);
                }
                result *= radix;
                if (result < limit + digit) {
                    throw new NumberFormatException(s);
                }
                result -= digit;
            }
        } else {
            throw new NumberFormatException(s);
        }
        if (negative) {
            if (i > 1) {
                return result;
            } else {
                throw new NumberFormatException(s);
            }
        } else {
            return -result;
        }
    }

    public static Long valueOf(String s) throws NumberFormatException {
        return new Long(parseLong(s, 10));
    }
    public static Long valueOf(String s, int radix) throws NumberFormatException {
        return new Long(parseLong(s, radix));
    }

    public String toString() { return toString(value); }

    // §20.8 toString — negative-space digit build (handles MIN_VALUE), Character.forDigit for radix.
    public static String toString(long i) {
        char[] buf = new char[20];                 // -9223372036854775808 is 20 chars
        int charPos = 20;
        boolean negative = i < 0;
        if (!negative) i = -i;
        while (i <= -10) {
            buf[--charPos] = (char)('0' - (int)(i % 10));
            i = i / 10;
        }
        buf[--charPos] = (char)('0' - (int)i);
        if (negative) buf[--charPos] = '-';
        return new String(buf, charPos, 20 - charPos);
    }
    public static String toString(long i, int radix) {
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX) radix = 10;
        if (radix == 10) return toString(i);
        char[] buf = new char[65];
        int charPos = 65;
        boolean negative = i < 0;
        if (!negative) i = -i;
        while (i <= -radix) {
            buf[--charPos] = Character.forDigit((int)(-(i % radix)), radix);
            i = i / radix;
        }
        buf[--charPos] = Character.forDigit((int)(-i), radix);
        if (negative) buf[--charPos] = '-';
        return new String(buf, charPos, 65 - charPos);
    }
    public static String toHexString(long i)    { return toUnsignedString(i, 4); }
    public static String toOctalString(long i)  { return toUnsignedString(i, 3); }
    public static String toBinaryString(long i) { return toUnsignedString(i, 1); }
    private static String toUnsignedString(long i, int shift) {
        char[] buf = new char[64];
        int charPos = 64;
        int radix = 1 << shift;
        long mask = radix - 1;
        do {
            buf[--charPos] = Character.forDigit((int)(i & mask), radix);
            i >>>= shift;
        } while (i != 0);
        return new String(buf, charPos, 64 - charPos);
    }
    // ── §20.8.21-.23: read a system property and interpret it as a long. `null` (or the supplied
    // default) whenever the property is absent or does not have the correct numeric format. ──
    public static Long getLong(String nm) { return getLong(nm, null); }

    public static Long getLong(String nm, long val) {
        Long result = getLong(nm, null);
        return (result == null) ? new Long(val) : result;
    }

    public static Long getLong(String nm, Long val) {
        String v = System.getProperty(nm);
        if (v == null) return val;
        try { return decode(v); }
        catch (NumberFormatException e) { return val; }
    }

    // §20.8.23's radix rules, identical to Integer's (§20.7.23).
    private static Long decode(String v) throws NumberFormatException {
        if (v.length() == 0) throw new NumberFormatException();
        if (v.startsWith("0x") && !v.startsWith("0x-")) return valueOf(v.substring(2), 16);
        if (v.startsWith("#")  && !v.startsWith("#-"))  return valueOf(v.substring(1), 16);
        if (v.startsWith("0")  && v.length() > 1)       return valueOf(v.substring(1), 8);
        return valueOf(v, 10);
    }
}
