package java.lang;

// Parse-result of FloatingDecimal.readJavaFormatString: the scanned decimal (sign, digits, decimal exponent),
// converted to a correctly-rounded double/float. De-nested from OpenJDK's ASCIIToBinaryBuffer +
// PreparedASCIIToBinaryBuffer (JLS 1.0 has no nested classes) into one top-level class: `isSpecial` instances
// carry a preset value (Infinity/NaN/signed zero); normal instances run the round-to-nearest algorithm.
// DoubleConsts/FloatConsts inlined as IEEE-754 constants; Integer/Long.numberOf{Leading,Trailing}Zeros as helpers.
class ASCIIToBinaryBuffer {

    // ── IEEE-754 field layout (jdk.internal.math.{Double,Float}Consts) ──
    private static final long D_SIGN_BIT_MASK   = 0x8000000000000000L;
    private static final long D_EXP_BIT_MASK    = 0x7FF0000000000000L;
    private static final long D_SIGNIF_BIT_MASK = 0x000FFFFFFFFFFFFFL;
    private static final int  D_EXP_BIAS = 1023;
    private static final int  EXP_SHIFT = 52;              // SIGNIFICAND_WIDTH(53) - 1
    private static final long FRACT_HOB = (1L << EXP_SHIFT);

    private static final int F_SIGN_BIT_MASK   = 0x80000000;
    private static final int F_EXP_BIT_MASK    = 0x7F800000;
    private static final int F_SIGNIF_BIT_MASK = 0x007FFFFF;
    private static final int F_EXP_BIAS = 127;
    private static final int SINGLE_EXP_SHIFT = 23;        // SIGNIFICAND_WIDTH(24) - 1
    private static final int SINGLE_FRACT_HOB = (1 << SINGLE_EXP_SHIFT);

    private static final int MAX_DECIMAL_DIGITS = 15;
    private static final int MAX_DECIMAL_EXPONENT = 308;
    private static final int MIN_DECIMAL_EXPONENT = -324;
    private static final int MAX_NDIGITS = 1100;
    private static final int SINGLE_MAX_DECIMAL_DIGITS = 7;
    private static final int SINGLE_MAX_DECIMAL_EXPONENT = 38;
    private static final int SINGLE_MIN_DECIMAL_EXPONENT = -45;
    private static final int SINGLE_MAX_NDIGITS = 200;
    private static final int INT_DECIMAL_DIGITS = 9;

    private static final double[] SMALL_10_POW = {
        1.0e0,
        1.0e1, 1.0e2, 1.0e3, 1.0e4, 1.0e5,
        1.0e6, 1.0e7, 1.0e8, 1.0e9, 1.0e10,
        1.0e11, 1.0e12, 1.0e13, 1.0e14, 1.0e15,
        1.0e16, 1.0e17, 1.0e18, 1.0e19, 1.0e20,
        1.0e21, 1.0e22
    };
    private static final float[] SINGLE_SMALL_10_POW = {
        1.0e0f,
        1.0e1f, 1.0e2f, 1.0e3f, 1.0e4f, 1.0e5f,
        1.0e6f, 1.0e7f, 1.0e8f, 1.0e9f, 1.0e10f
    };
    private static final double[] BIG_10_POW = { 1e16, 1e32, 1e64, 1e128, 1e256 };
    private static final double[] TINY_10_POW = { 1e-16, 1e-32, 1e-64, 1e-128, 1e-256 };
    private static final int MAX_SMALL_TEN = SMALL_10_POW.length - 1;
    private static final int SINGLE_MAX_SMALL_TEN = SINGLE_SMALL_10_POW.length - 1;

    boolean isNegative;
    int decExponent;
    char[] digits;
    int nDigits;

    private boolean isSpecial;
    private double specialDouble;
    private float specialFloat;

    ASCIIToBinaryBuffer(boolean negSign, int decExponent, char[] digits, int n) {
        this.isNegative = negSign;
        this.decExponent = decExponent;
        this.digits = digits;
        this.nDigits = n;
    }

    ASCIIToBinaryBuffer(double doubleVal, float floatVal) {   // prepared Infinity/NaN/signed-zero
        this.isSpecial = true;
        this.specialDouble = doubleVal;
        this.specialFloat = floatVal;
    }

    // ── Integer/Long.numberOf{Leading,Trailing}Zeros (absent in JLS 1.0) ──
    private static int nlz(int i) {
        if (i == 0) return 32;
        int n = 1;
        if (i >>> 16 == 0) { n += 16; i <<= 16; }
        if (i >>> 24 == 0) { n += 8; i <<= 8; }
        if (i >>> 28 == 0) { n += 4; i <<= 4; }
        if (i >>> 30 == 0) { n += 2; i <<= 2; }
        n -= i >>> 31;
        return n;
    }
    private static int nlzLong(long i) {
        if (i <= 0) return i == 0 ? 64 : 0;
        int n = 1;
        int x = (int) (i >>> 32);
        if (x == 0) { n += 32; x = (int) i; }
        if (x >>> 16 == 0) { n += 16; x <<= 16; }
        if (x >>> 24 == 0) { n += 8; x <<= 8; }
        if (x >>> 28 == 0) { n += 4; x <<= 4; }
        if (x >>> 30 == 0) { n += 2; x <<= 2; }
        n -= x >>> 31;
        return n;
    }
    private static int ntz(int i) {
        if (i == 0) return 32;
        int n = 31;
        int y = i << 16; if (y != 0) { n -= 16; i = y; }
        y = i << 8;  if (y != 0) { n -= 8; i = y; }
        y = i << 4;  if (y != 0) { n -= 4; i = y; }
        y = i << 2;  if (y != 0) { n -= 2; i = y; }
        return n - ((i << 1) >>> 31);
    }
    private static int ntzLong(long i) {
        if (i == 0) return 64;
        int x, y;
        int n = 63;
        y = (int) i; if (y != 0) { n -= 32; x = y; } else x = (int) (i >>> 32);
        y = x << 16; if (y != 0) { n -= 16; x = y; }
        y = x << 8;  if (y != 0) { n -= 8; x = y; }
        y = x << 4;  if (y != 0) { n -= 4; x = y; }
        y = x << 2;  if (y != 0) { n -= 2; x = y; }
        return n - ((x << 1) >>> 31);
    }

    double doubleValue() {
        if (isSpecial) return specialDouble;
        int kDigits = Math.min(nDigits, MAX_DECIMAL_DIGITS + 1);
        int iValue = (int) digits[0] - (int) '0';
        int iDigits = Math.min(kDigits, INT_DECIMAL_DIGITS);
        for (int i = 1; i < iDigits; i++) {
            iValue = iValue * 10 + (int) digits[i] - (int) '0';
        }
        long lValue = (long) iValue;
        for (int i = iDigits; i < kDigits; i++) {
            lValue = lValue * 10L + (long) ((int) digits[i] - (int) '0');
        }
        double dValue = (double) lValue;
        int exp = decExponent - kDigits;
        if (nDigits <= MAX_DECIMAL_DIGITS) {
            if (exp == 0 || dValue == 0.0) {
                return (isNegative) ? -dValue : dValue;
            } else if (exp >= 0) {
                if (exp <= MAX_SMALL_TEN) {
                    double rValue = dValue * SMALL_10_POW[exp];
                    return (isNegative) ? -rValue : rValue;
                }
                int slop = MAX_DECIMAL_DIGITS - kDigits;
                if (exp <= MAX_SMALL_TEN + slop) {
                    dValue *= SMALL_10_POW[slop];
                    double rValue = dValue * SMALL_10_POW[exp - slop];
                    return (isNegative) ? -rValue : rValue;
                }
            } else {
                if (exp >= -MAX_SMALL_TEN) {
                    double rValue = dValue / SMALL_10_POW[-exp];
                    return (isNegative) ? -rValue : rValue;
                }
            }
        }
        if (exp > 0) {
            if (decExponent > MAX_DECIMAL_EXPONENT + 1) {
                return (isNegative) ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
            }
            if ((exp & 15) != 0) {
                dValue *= SMALL_10_POW[exp & 15];
            }
            if ((exp >>= 4) != 0) {
                int j;
                for (j = 0; exp > 1; j++, exp >>= 1) {
                    if ((exp & 1) != 0) {
                        dValue *= BIG_10_POW[j];
                    }
                }
                double t = dValue * BIG_10_POW[j];
                if (Double.isInfinite(t)) {
                    t = dValue / 2.0;
                    t *= BIG_10_POW[j];
                    if (Double.isInfinite(t)) {
                        return (isNegative) ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
                    }
                    t = Double.MAX_VALUE;
                }
                dValue = t;
            }
        } else if (exp < 0) {
            exp = -exp;
            if (decExponent < MIN_DECIMAL_EXPONENT - 1) {
                return (isNegative) ? -0.0 : 0.0;
            }
            if ((exp & 15) != 0) {
                dValue /= SMALL_10_POW[exp & 15];
            }
            if ((exp >>= 4) != 0) {
                int j;
                for (j = 0; exp > 1; j++, exp >>= 1) {
                    if ((exp & 1) != 0) {
                        dValue *= TINY_10_POW[j];
                    }
                }
                double t = dValue * TINY_10_POW[j];
                if (t == 0.0) {
                    t = dValue * 2.0;
                    t *= TINY_10_POW[j];
                    if (t == 0.0) {
                        return (isNegative) ? -0.0 : 0.0;
                    }
                    t = Double.MIN_VALUE;
                }
                dValue = t;
            }
        }
        if (nDigits > MAX_NDIGITS) {
            nDigits = MAX_NDIGITS + 1;
            digits[MAX_NDIGITS] = '1';
        }
        FDBigInteger bigD0 = new FDBigInteger(lValue, digits, kDigits, nDigits);
        exp = decExponent - nDigits;
        long ieeeBits = Double.doubleToRawLongBits(dValue);
        final int B5 = Math.max(0, -exp);
        final int D5 = Math.max(0, exp);
        bigD0 = bigD0.multByPow52(D5, 0);
        bigD0.makeImmutable();
        FDBigInteger bigD = null;
        int prevD2 = 0;

        correctionLoop:
        while (true) {
            int binexp = (int) (ieeeBits >>> EXP_SHIFT);
            long bigBbits = ieeeBits & D_SIGNIF_BIT_MASK;
            if (binexp > 0) {
                bigBbits |= FRACT_HOB;
            } else {
                int leadingZeros = nlzLong(bigBbits);
                int shift = leadingZeros - (63 - EXP_SHIFT);
                bigBbits <<= shift;
                binexp = 1 - shift;
            }
            binexp -= D_EXP_BIAS;
            int lowOrderZeros = ntzLong(bigBbits);
            bigBbits >>>= lowOrderZeros;
            final int bigIntExp = binexp - EXP_SHIFT + lowOrderZeros;
            final int bigIntNBits = EXP_SHIFT + 1 - lowOrderZeros;
            int B2 = B5;
            int D2 = D5;
            int Ulp2;
            if (bigIntExp >= 0) {
                B2 += bigIntExp;
            } else {
                D2 -= bigIntExp;
            }
            Ulp2 = B2;
            int hulpbias;
            if (binexp <= -D_EXP_BIAS) {
                hulpbias = binexp + lowOrderZeros + D_EXP_BIAS;
            } else {
                hulpbias = 1 + lowOrderZeros;
            }
            B2 += hulpbias;
            D2 += hulpbias;
            int common2 = Math.min(B2, Math.min(D2, Ulp2));
            B2 -= common2;
            D2 -= common2;
            Ulp2 -= common2;
            FDBigInteger bigB = FDBigInteger.valueOfMulPow52(bigBbits, B5, B2);
            if (bigD == null || prevD2 != D2) {
                bigD = bigD0.leftShift(D2);
                prevD2 = D2;
            }
            FDBigInteger diff;
            int cmpResult;
            boolean overvalue;
            if ((cmpResult = bigB.cmp(bigD)) > 0) {
                overvalue = true;
                diff = bigB.leftInplaceSub(bigD);
                if ((bigIntNBits == 1) && (bigIntExp > -D_EXP_BIAS + 1)) {
                    Ulp2 -= 1;
                    if (Ulp2 < 0) {
                        Ulp2 = 0;
                        diff = diff.leftShift(1);
                    }
                }
            } else if (cmpResult < 0) {
                overvalue = false;
                diff = bigD.rightInplaceSub(bigB);
            } else {
                break correctionLoop;
            }
            cmpResult = diff.cmpPow52(B5, Ulp2);
            if ((cmpResult) < 0) {
                break correctionLoop;
            } else if (cmpResult == 0) {
                if ((ieeeBits & 1) != 0) {
                    ieeeBits += overvalue ? -1 : 1;
                }
                break correctionLoop;
            } else {
                ieeeBits += overvalue ? -1 : 1;
                if (ieeeBits == 0 || ieeeBits == D_EXP_BIT_MASK) {
                    break correctionLoop;
                }
                continue;
            }
        }
        if (isNegative) {
            ieeeBits |= D_SIGN_BIT_MASK;
        }
        return Double.longBitsToDouble(ieeeBits);
    }

    float floatValue() {
        if (isSpecial) return specialFloat;
        int kDigits = Math.min(nDigits, SINGLE_MAX_DECIMAL_DIGITS + 1);
        int iValue = (int) digits[0] - (int) '0';
        for (int i = 1; i < kDigits; i++) {
            iValue = iValue * 10 + (int) digits[i] - (int) '0';
        }
        float fValue = (float) iValue;
        int exp = decExponent - kDigits;
        if (nDigits <= SINGLE_MAX_DECIMAL_DIGITS) {
            if (exp == 0 || fValue == 0.0f) {
                return (isNegative) ? -fValue : fValue;
            } else if (exp >= 0) {
                if (exp <= SINGLE_MAX_SMALL_TEN) {
                    fValue *= SINGLE_SMALL_10_POW[exp];
                    return (isNegative) ? -fValue : fValue;
                }
                int slop = SINGLE_MAX_DECIMAL_DIGITS - kDigits;
                if (exp <= SINGLE_MAX_SMALL_TEN + slop) {
                    fValue *= SINGLE_SMALL_10_POW[slop];
                    fValue *= SINGLE_SMALL_10_POW[exp - slop];
                    return (isNegative) ? -fValue : fValue;
                }
            } else {
                if (exp >= -SINGLE_MAX_SMALL_TEN) {
                    fValue /= SINGLE_SMALL_10_POW[-exp];
                    return (isNegative) ? -fValue : fValue;
                }
            }
        } else if ((decExponent >= nDigits) && (nDigits + decExponent <= MAX_DECIMAL_DIGITS)) {
            long lValue = (long) iValue;
            for (int i = kDigits; i < nDigits; i++) {
                lValue = lValue * 10L + (long) ((int) digits[i] - (int) '0');
            }
            double dv = (double) lValue;
            exp = decExponent - nDigits;
            dv *= SMALL_10_POW[exp];
            fValue = (float) dv;
            return (isNegative) ? -fValue : fValue;
        }
        double dValue = fValue;
        if (exp > 0) {
            if (decExponent > SINGLE_MAX_DECIMAL_EXPONENT + 1) {
                return (isNegative) ? Float.NEGATIVE_INFINITY : Float.POSITIVE_INFINITY;
            }
            if ((exp & 15) != 0) {
                dValue *= SMALL_10_POW[exp & 15];
            }
            if ((exp >>= 4) != 0) {
                int j;
                for (j = 0; exp > 0; j++, exp >>= 1) {
                    if ((exp & 1) != 0) {
                        dValue *= BIG_10_POW[j];
                    }
                }
            }
        } else if (exp < 0) {
            exp = -exp;
            if (decExponent < SINGLE_MIN_DECIMAL_EXPONENT - 1) {
                return (isNegative) ? -0.0f : 0.0f;
            }
            if ((exp & 15) != 0) {
                dValue /= SMALL_10_POW[exp & 15];
            }
            if ((exp >>= 4) != 0) {
                int j;
                for (j = 0; exp > 0; j++, exp >>= 1) {
                    if ((exp & 1) != 0) {
                        dValue *= TINY_10_POW[j];
                    }
                }
            }
        }
        fValue = Math.max(Float.MIN_VALUE, Math.min(Float.MAX_VALUE, (float) dValue));

        if (nDigits > SINGLE_MAX_NDIGITS) {
            nDigits = SINGLE_MAX_NDIGITS + 1;
            digits[SINGLE_MAX_NDIGITS] = '1';
        }
        FDBigInteger bigD0 = new FDBigInteger(iValue, digits, kDigits, nDigits);
        exp = decExponent - nDigits;
        int ieeeBits = Float.floatToRawIntBits(fValue);
        final int B5 = Math.max(0, -exp);
        final int D5 = Math.max(0, exp);
        bigD0 = bigD0.multByPow52(D5, 0);
        bigD0.makeImmutable();
        FDBigInteger bigD = null;
        int prevD2 = 0;

        correctionLoop:
        while (true) {
            int binexp = ieeeBits >>> SINGLE_EXP_SHIFT;
            int bigBbits = ieeeBits & F_SIGNIF_BIT_MASK;
            if (binexp > 0) {
                bigBbits |= SINGLE_FRACT_HOB;
            } else {
                int leadingZeros = nlz(bigBbits);
                int shift = leadingZeros - (31 - SINGLE_EXP_SHIFT);
                bigBbits <<= shift;
                binexp = 1 - shift;
            }
            binexp -= F_EXP_BIAS;
            int lowOrderZeros = ntz(bigBbits);
            bigBbits >>>= lowOrderZeros;
            final int bigIntExp = binexp - SINGLE_EXP_SHIFT + lowOrderZeros;
            final int bigIntNBits = SINGLE_EXP_SHIFT + 1 - lowOrderZeros;
            int B2 = B5;
            int D2 = D5;
            int Ulp2;
            if (bigIntExp >= 0) {
                B2 += bigIntExp;
            } else {
                D2 -= bigIntExp;
            }
            Ulp2 = B2;
            int hulpbias;
            if (binexp <= -F_EXP_BIAS) {
                hulpbias = binexp + lowOrderZeros + F_EXP_BIAS;
            } else {
                hulpbias = 1 + lowOrderZeros;
            }
            B2 += hulpbias;
            D2 += hulpbias;
            int common2 = Math.min(B2, Math.min(D2, Ulp2));
            B2 -= common2;
            D2 -= common2;
            Ulp2 -= common2;
            FDBigInteger bigB = FDBigInteger.valueOfMulPow52(bigBbits, B5, B2);
            if (bigD == null || prevD2 != D2) {
                bigD = bigD0.leftShift(D2);
                prevD2 = D2;
            }
            FDBigInteger diff;
            int cmpResult;
            boolean overvalue;
            if ((cmpResult = bigB.cmp(bigD)) > 0) {
                overvalue = true;
                diff = bigB.leftInplaceSub(bigD);
                if ((bigIntNBits == 1) && (bigIntExp > -F_EXP_BIAS + 1)) {
                    Ulp2 -= 1;
                    if (Ulp2 < 0) {
                        Ulp2 = 0;
                        diff = diff.leftShift(1);
                    }
                }
            } else if (cmpResult < 0) {
                overvalue = false;
                diff = bigD.rightInplaceSub(bigB);
            } else {
                break correctionLoop;
            }
            cmpResult = diff.cmpPow52(B5, Ulp2);
            if ((cmpResult) < 0) {
                break correctionLoop;
            } else if (cmpResult == 0) {
                if ((ieeeBits & 1) != 0) {
                    ieeeBits += overvalue ? -1 : 1;
                }
                break correctionLoop;
            } else {
                ieeeBits += overvalue ? -1 : 1;
                if (ieeeBits == 0 || ieeeBits == F_EXP_BIT_MASK) {
                    break correctionLoop;
                }
                continue;
            }
        }
        if (isNegative) {
            ieeeBits |= F_SIGN_BIT_MASK;
        }
        return Float.intBitsToFloat(ieeeBits);
    }
}
