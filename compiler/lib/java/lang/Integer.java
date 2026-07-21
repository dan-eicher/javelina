package java.lang;

public final class Integer extends Number {
    public static final int MIN_VALUE = 0x80000000;
    public static final int MAX_VALUE = 0x7fffffff;

    private int value;

    public Integer(int value) { this.value = value; }
    public Integer(String s) throws NumberFormatException { this.value = parseInt(s, 10); }

    public int intValue() { return value; }
    public long longValue() { return (long)value; }
    public float floatValue() { return (float)value; }
    public double doubleValue() { return (double)value; }

    public int hashCode() { return value; }

    public boolean equals(Object obj) {
        if (obj instanceof Integer) {
            return value == ((Integer)obj).intValue();
        }
        return false;
    }

    public static int parseInt(String s) throws NumberFormatException {
        return parseInt(s, 10);
    }

    /* §20.7 parseInt — JLS algorithm: accumulate negatively so MIN_VALUE parses
     * without overflowing the positive range, with per-step overflow guards. */
    public static int parseInt(String s, int radix) throws NumberFormatException {
        int result = 0;
        boolean negative = false;
        int i = 0;
        int max = s.length();
        int limit;
        int multmin;
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

    public static Integer valueOf(String s) throws NumberFormatException {
        return new Integer(parseInt(s, 10));
    }
    public static Integer valueOf(String s, int radix) throws NumberFormatException {
        return new Integer(parseInt(s, radix));
    }

    public String toString() { return toString(value); }

    // §20.7 toString — build digits in NEGATIVE space so MIN_VALUE (which can't be
    // negated) formats without overflow. Character.forDigit supplies radix digits.
    public static String toString(int i) {
        char[] buf = new char[11];                 // -2147483648 is 11 chars
        int charPos = 11;
        boolean negative = i < 0;
        if (!negative) i = -i;                     // work with i <= 0
        while (i <= -10) {
            buf[--charPos] = (char)('0' - (i % 10));
            i = i / 10;
        }
        buf[--charPos] = (char)('0' - i);
        if (negative) buf[--charPos] = '-';
        return new String(buf, charPos, 11 - charPos);
    }
    public static String toString(int i, int radix) {
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX) radix = 10;
        if (radix == 10) return toString(i);
        char[] buf = new char[33];
        int charPos = 33;
        boolean negative = i < 0;
        if (!negative) i = -i;
        while (i <= -radix) {
            buf[--charPos] = Character.forDigit(-(i % radix), radix);
            i = i / radix;
        }
        buf[--charPos] = Character.forDigit(-i, radix);
        if (negative) buf[--charPos] = '-';
        return new String(buf, charPos, 33 - charPos);
    }
    public static String toHexString(int i)    { return toUnsignedString(i, 4); }
    public static String toOctalString(int i)  { return toUnsignedString(i, 3); }
    public static String toBinaryString(int i) { return toUnsignedString(i, 1); }
    // Unsigned radix (2^shift) formatting: >>> feeds the bits in unsigned, so a
    // negative int prints its full 32-bit magnitude.
    private static String toUnsignedString(int i, int shift) {
        char[] buf = new char[32];
        int charPos = 32;
        int radix = 1 << shift;
        int mask = radix - 1;
        do {
            buf[--charPos] = Character.forDigit(i & mask, radix);
            i >>>= shift;
        } while (i != 0);
        return new String(buf, charPos, 32 - charPos);
    }
    // ── §20.7.21-.23: read a system property and interpret it as an integer. `null` (or the supplied
    // default) whenever the property is absent or does not have the correct numeric format. ──
    public static Integer getInteger(String nm) { return getInteger(nm, null); }

    public static Integer getInteger(String nm, int val) {
        Integer result = getInteger(nm, null);
        return (result == null) ? new Integer(val) : result;
    }

    public static Integer getInteger(String nm, Integer val) {
        String v = System.getProperty(nm);
        if (v == null) return val;
        try { return decode(v); }
        catch (NumberFormatException e) { return val; }
    }

    // §20.7.23's radix rules: "0x" or "#" (not followed by a minus sign) is hexadecimal, a leading
    // "0" before another character is octal, anything else is decimal.
    private static Integer decode(String v) throws NumberFormatException {
        if (v.length() == 0) throw new NumberFormatException();
        if (v.startsWith("0x") && !v.startsWith("0x-")) return valueOf(v.substring(2), 16);
        if (v.startsWith("#")  && !v.startsWith("#-"))  return valueOf(v.substring(1), 16);
        if (v.startsWith("0")  && v.length() > 1)       return valueOf(v.substring(1), 8);
        return valueOf(v, 10);
    }
}
