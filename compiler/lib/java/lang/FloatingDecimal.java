package java.lang;

// Faithful port of OpenJDK jdk.internal.math.FloatingDecimal — the String->binary (parse) path.
// Double/Float.valueOf(String) delegate here for a correctly-rounded result (JLS §20.10.13 / §20.9.14).
// The nested ASCIIToBinaryConverter/ASCIIToBinaryBuffer design is flattened to the top-level ASCIIToBinaryBuffer
// (JLS 1.0 has no nested classes). Hexadecimal floating-point strings are a Java 1.5 feature and are NOT valid
// input under JLS 1.0, so readJavaFormatString has no 0x branch (they fall through to a NumberFormatException).
class FloatingDecimal {

    private static final String NAN_REP = "NaN";
    private static final String INFINITY_REP = "Infinity";
    private static final int    NAN_LENGTH = 3;        // "NaN".length()
    private static final int    INFINITY_LENGTH = 8;   // "Infinity".length()
    private static final int    BIG_DECIMAL_EXPONENT = 324; // abs(MIN_DECIMAL_EXPONENT)

    // Prepared parse-results for the special values (sign is applied by the caller's negative flag for Infinity).
    private static final ASCIIToBinaryBuffer A2BC_POSITIVE_INFINITY =
        new ASCIIToBinaryBuffer(Double.POSITIVE_INFINITY, Float.POSITIVE_INFINITY);
    private static final ASCIIToBinaryBuffer A2BC_NEGATIVE_INFINITY =
        new ASCIIToBinaryBuffer(Double.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY);
    private static final ASCIIToBinaryBuffer A2BC_NOT_A_NUMBER =
        new ASCIIToBinaryBuffer(Double.NaN, Float.NaN);
    private static final ASCIIToBinaryBuffer A2BC_POSITIVE_ZERO =
        new ASCIIToBinaryBuffer(0.0d, 0.0f);
    private static final ASCIIToBinaryBuffer A2BC_NEGATIVE_ZERO =
        new ASCIIToBinaryBuffer(-0.0d, -0.0f);

    public static double parseDouble(String s) throws NumberFormatException {
        return readJavaFormatString(s).doubleValue();
    }

    public static float parseFloat(String s) throws NumberFormatException {
        return readJavaFormatString(s).floatValue();
    }

    static ASCIIToBinaryBuffer readJavaFormatString(String in) throws NumberFormatException {
        boolean isNegative = false;
        boolean signSeen = false;
        int decExp;
        char c;

    parseNumber:
        try {
            in = in.trim();   // throws NullPointerException if null
            int len = in.length();
            if (len == 0) {
                throw new NumberFormatException("empty String");
            }
            int i = 0;
            switch (in.charAt(i)) {
            case '-':
                isNegative = true;
                // FALLTHROUGH
            case '+':
                i++;
                signSeen = true;
            }
            c = in.charAt(i);
            if (c == 'N') {  // Check for NaN
                if ((len - i) == NAN_LENGTH && in.indexOf(NAN_REP, i) == i) {
                    return A2BC_NOT_A_NUMBER;
                }
                break parseNumber;
            } else if (c == 'I') {  // Check for Infinity strings
                if ((len - i) == INFINITY_LENGTH && in.indexOf(INFINITY_REP, i) == i) {
                    return isNegative ? A2BC_NEGATIVE_INFINITY : A2BC_POSITIVE_INFINITY;
                }
                break parseNumber;
            }
            // (No hex floating-point branch: hex float literals are JLS 1.5, invalid here.)

            char[] digits = new char[len];
            int nDigits = 0;
            boolean decSeen = false;
            int decPt = 0;
            int nLeadZero = 0;
            int nTrailZero = 0;

        skipLeadingZerosLoop:
            while (i < len) {
                c = in.charAt(i);
                if (c == '0') {
                    nLeadZero++;
                } else if (c == '.') {
                    if (decSeen) {
                        throw new NumberFormatException("multiple points");
                    }
                    decPt = i;
                    if (signSeen) {
                        decPt -= 1;
                    }
                    decSeen = true;
                } else {
                    break skipLeadingZerosLoop;
                }
                i++;
            }
        digitLoop:
            while (i < len) {
                c = in.charAt(i);
                if (c >= '1' && c <= '9') {
                    digits[nDigits++] = c;
                    nTrailZero = 0;
                } else if (c == '0') {
                    digits[nDigits++] = c;
                    nTrailZero++;
                } else if (c == '.') {
                    if (decSeen) {
                        throw new NumberFormatException("multiple points");
                    }
                    decPt = i;
                    if (signSeen) {
                        decPt -= 1;
                    }
                    decSeen = true;
                } else {
                    break digitLoop;
                }
                i++;
            }
            nDigits -= nTrailZero;
            boolean isZero = (nDigits == 0);
            if (isZero && nLeadZero == 0) {
                break parseNumber;
            }
            if (decSeen) {
                decExp = decPt - nLeadZero;
            } else {
                decExp = nDigits + nTrailZero;
            }
            if ((i < len) && (((c = in.charAt(i)) == 'e') || (c == 'E'))) {
                int expSign = 1;
                int expVal = 0;
                int reallyBig = Integer.MAX_VALUE / 10;
                boolean expOverflow = false;
                switch (in.charAt(++i)) {
                case '-':
                    expSign = -1;
                    // FALLTHROUGH
                case '+':
                    i++;
                }
                int expAt = i;
            expLoop:
                while (i < len) {
                    if (expVal >= reallyBig) {
                        expOverflow = true;
                    }
                    c = in.charAt(i++);
                    if (c >= '0' && c <= '9') {
                        expVal = expVal * 10 + ((int) c - (int) '0');
                    } else {
                        i--;
                        break expLoop;
                    }
                }
                int expLimit = BIG_DECIMAL_EXPONENT + nDigits + nTrailZero;
                if (expOverflow || (expVal > expLimit)) {
                    if (!expOverflow && (expSign == 1 && decExp < 0)
                            && (expVal + decExp) < expLimit) {
                        decExp += expVal;
                    } else {
                        decExp = expSign * expLimit;
                    }
                } else {
                    decExp = decExp + expSign * expVal;
                }
                if (i == expAt) {
                    break parseNumber;
                }
            }
            if (i < len &&
                ((i != len - 1) ||
                (in.charAt(i) != 'f' &&
                 in.charAt(i) != 'F' &&
                 in.charAt(i) != 'd' &&
                 in.charAt(i) != 'D'))) {
                break parseNumber;
            }
            if (isZero) {
                return isNegative ? A2BC_NEGATIVE_ZERO : A2BC_POSITIVE_ZERO;
            }
            return new ASCIIToBinaryBuffer(isNegative, decExp, digits, nDigits);
        } catch (StringIndexOutOfBoundsException e) { }
        throw new NumberFormatException("For input string: \"" + in + "\"");
    }

    // Binary->String (dtoa) direction for Double/Float.toString: the shortest decimal that round-trips
    // to the same value (JLS §20.10.15 / §20.9.16), developed in BinaryToASCIIBuffer.
    public static String toStringDouble(double d) {
        return BinaryToASCIIBuffer.converterFor(d).toJavaFormatString();
    }

    public static String toStringFloat(float f) {
        return BinaryToASCIIBuffer.converterFor(f).toJavaFormatString();
    }
}
