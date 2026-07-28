// SPDX-License-Identifier: GPL-2.0-with-classpath-exception
// Derived from OpenJDK; GPLv2 + Classpath Exception (see compiler/lib/LICENSE).
package java.lang;

// Faithful port of OpenJDK jdk.internal.math.FloatingDecimal's binary->String (dtoa) path — the shortest
// decimal string that round-trips to the same double/float (JLS §20.10.15 / §20.9.16). De-nested from the
// nested BinaryToASCIIBuffer + ExceptionalBinaryToASCIIBuffer (JLS 1.0 has no nested classes): an `image`
// field carries the fixed text for Infinity/NaN; ThreadLocal reuse dropped (a fresh buffer per call).
// Double/Float.toString delegate here via FloatingDecimal.toStringDouble/toStringFloat.
class BinaryToASCIIBuffer {

    private static final long D_SIGN_BIT_MASK   = 0x8000000000000000L;
    private static final long D_EXP_BIT_MASK    = 0x7FF0000000000000L;
    private static final long D_SIGNIF_BIT_MASK = 0x000FFFFFFFFFFFFFL;
    private static final int  D_EXP_BIAS = 1023;
    private static final int  EXP_SHIFT = 52;
    private static final long FRACT_HOB = (1L << EXP_SHIFT);
    private static final long EXP_ONE   = ((long) D_EXP_BIAS) << EXP_SHIFT;

    private static final int F_SIGN_BIT_MASK   = 0x80000000;
    private static final int F_EXP_BIT_MASK    = 0x7F800000;
    private static final int F_SIGNIF_BIT_MASK = 0x007FFFFF;
    private static final int F_EXP_BIAS = 127;
    private static final int SINGLE_EXP_SHIFT = 23;
    private static final int SINGLE_FRACT_HOB = (1 << SINGLE_EXP_SHIFT);

    private static final int MAX_SMALL_BIN_EXP = 62;
    private static final int MIN_SMALL_BIN_EXP = -(63 / 3);

    private static final int[] insignificantDigitsNumber = {
        0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3,
        4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7,
        8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 11, 11, 11,
        12, 12, 12, 12, 13, 13, 13, 14, 14, 14,
        15, 15, 15, 15, 16, 16, 16, 17, 17, 17,
        18, 18, 18, 19
    };
    // approximately ceil(log2(LONG_5_POW[i]))
    private static final int[] N_5_BITS = {
        0, 3, 5, 7, 10, 12, 14, 17, 19, 21, 24, 26, 28, 31, 33, 35,
        38, 40, 42, 45, 47, 49, 52, 54, 56, 59, 61
    };

    // ── prepared converters for the special values ──
    static final BinaryToASCIIBuffer B2AC_POSITIVE_INFINITY = new BinaryToASCIIBuffer("Infinity", false);
    static final BinaryToASCIIBuffer B2AC_NEGATIVE_INFINITY = new BinaryToASCIIBuffer("-Infinity", true);
    static final BinaryToASCIIBuffer B2AC_NOT_A_NUMBER      = new BinaryToASCIIBuffer("NaN", false);
    // §8.3.1.2: a final field's "declarator must include a variable initializer", so the two
    // signed zeros are built by an initializer rather than by a static block assigning them --
    // that assignment is itself what the section's next sentence forbids. The helper exists
    // because §15.9's ArrayCreationExpression has no initializer form (`new char[]{'0'}` is
    // later syntax), so a one-element array cannot be written as an expression.
    static final BinaryToASCIIBuffer B2AC_POSITIVE_ZERO = new BinaryToASCIIBuffer(false, zeroDigit());
    static final BinaryToASCIIBuffer B2AC_NEGATIVE_ZERO = new BinaryToASCIIBuffer(true,  zeroDigit());

    private static char[] zeroDigit() {
        char[] z = new char[1];
        z[0] = '0';
        return z;
    }

    private boolean isNegative;
    private int decExponent;
    private int firstDigitIndex;
    private int nDigits;
    private char[] digits;   // not final: §8.3.1.2 admits no field a constructor assigns
    private final char[] buffer = new char[26];
    private String image;   // non-null → Infinity/NaN, toJavaFormatString returns it verbatim
    private boolean exactDecimalConversion = false;
    private boolean decimalDigitsRoundedUp = false;

    BinaryToASCIIBuffer() {
        this.digits = new char[20];
    }
    BinaryToASCIIBuffer(boolean isNegative, char[] digits) {  // signed zero
        this.isNegative = isNegative;
        this.decExponent = 0;
        this.digits = digits;
        this.firstDigitIndex = 0;
        this.nDigits = digits.length;
    }
    BinaryToASCIIBuffer(String image, boolean isNegative) {  // Infinity / NaN
        this.image = image;
        this.isNegative = isNegative;
        this.digits = null;
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
    private static void fillChar(char[] a, int from, int to, char c) {
        for (int k = from; k < to; k++) a[k] = c;
    }

    String toJavaFormatString() {
        if (image != null) return image;
        int len = getChars(buffer);
        return new String(buffer, 0, len);
    }

    private void setSign(boolean isNegative) {
        this.isNegative = isNegative;
    }

    // ── binary → shortest decimal digits (Dragon-style; FDBigInteger for the hard case) ──
    static BinaryToASCIIBuffer converterFor(double d) {
        long dBits = Double.doubleToRawLongBits(d);
        boolean isNegative = (dBits & D_SIGN_BIT_MASK) != 0;
        long fractBits = dBits & D_SIGNIF_BIT_MASK;
        int binExp = (int) ((dBits & D_EXP_BIT_MASK) >> EXP_SHIFT);
        if (binExp == (int) (D_EXP_BIT_MASK >> EXP_SHIFT)) {
            if (fractBits == 0L) return isNegative ? B2AC_NEGATIVE_INFINITY : B2AC_POSITIVE_INFINITY;
            return B2AC_NOT_A_NUMBER;
        }
        int nSignificantBits;
        if (binExp == 0) {
            if (fractBits == 0L) return isNegative ? B2AC_NEGATIVE_ZERO : B2AC_POSITIVE_ZERO;
            int leadingZeros = nlzLong(fractBits);
            int shift = leadingZeros - (63 - EXP_SHIFT);
            fractBits <<= shift;
            binExp = 1 - shift;
            nSignificantBits = 64 - leadingZeros;
        } else {
            fractBits |= FRACT_HOB;
            nSignificantBits = EXP_SHIFT + 1;
        }
        binExp -= D_EXP_BIAS;
        BinaryToASCIIBuffer buf = new BinaryToASCIIBuffer();
        buf.setSign(isNegative);
        buf.dtoa(binExp, fractBits, nSignificantBits, true);
        return buf;
    }

    static BinaryToASCIIBuffer converterFor(float f) {
        int fBits = Float.floatToRawIntBits(f);
        boolean isNegative = (fBits & F_SIGN_BIT_MASK) != 0;
        int fractBits = fBits & F_SIGNIF_BIT_MASK;
        int binExp = (fBits & F_EXP_BIT_MASK) >> SINGLE_EXP_SHIFT;
        if (binExp == (F_EXP_BIT_MASK >> SINGLE_EXP_SHIFT)) {
            if (fractBits == 0) return isNegative ? B2AC_NEGATIVE_INFINITY : B2AC_POSITIVE_INFINITY;
            return B2AC_NOT_A_NUMBER;
        }
        int nSignificantBits;
        if (binExp == 0) {
            if (fractBits == 0) return isNegative ? B2AC_NEGATIVE_ZERO : B2AC_POSITIVE_ZERO;
            int leadingZeros = nlz(fractBits);
            int shift = leadingZeros - (31 - SINGLE_EXP_SHIFT);
            fractBits <<= shift;
            binExp = 1 - shift;
            nSignificantBits = 32 - leadingZeros;
        } else {
            fractBits |= SINGLE_FRACT_HOB;
            nSignificantBits = SINGLE_EXP_SHIFT + 1;
        }
        binExp -= F_EXP_BIAS;
        BinaryToASCIIBuffer buf = new BinaryToASCIIBuffer();
        buf.setSign(isNegative);
        buf.dtoa(binExp, ((long) fractBits) << (EXP_SHIFT - SINGLE_EXP_SHIFT), nSignificantBits, true);
        return buf;
    }

    private static int insignificantDigitsForPow2(int p2) {
        if (p2 > 1 && p2 < insignificantDigitsNumber.length) return insignificantDigitsNumber[p2];
        return 0;
    }

    private void developLongDigits(int decExponent, long lvalue, int insignificantDigits) {
        if (insignificantDigits != 0) {
            long pow10 = FDBigInteger.LONG_5_POW[insignificantDigits] << insignificantDigits;
            long residue = lvalue % pow10;
            lvalue /= pow10;
            decExponent += insignificantDigits;
            if (residue >= (pow10 >> 1)) {
                lvalue++;
            }
        }
        int digitno = digits.length - 1;
        int c;
        if (lvalue <= Integer.MAX_VALUE) {
            int ivalue = (int) lvalue;
            c = ivalue % 10;
            ivalue /= 10;
            while (c == 0) {
                decExponent++;
                c = ivalue % 10;
                ivalue /= 10;
            }
            while (ivalue != 0) {
                digits[digitno--] = (char) (c + '0');
                decExponent++;
                c = ivalue % 10;
                ivalue /= 10;
            }
            digits[digitno] = (char) (c + '0');
        } else {
            c = (int) (lvalue % 10L);
            lvalue /= 10L;
            while (c == 0) {
                decExponent++;
                c = (int) (lvalue % 10L);
                lvalue /= 10L;
            }
            while (lvalue != 0L) {
                digits[digitno--] = (char) (c + '0');
                decExponent++;
                c = (int) (lvalue % 10L);
                lvalue /= 10;
            }
            digits[digitno] = (char) (c + '0');
        }
        this.decExponent = decExponent + 1;
        this.firstDigitIndex = digitno;
        this.nDigits = this.digits.length - digitno;
    }

    private void dtoa(int binExp, long fractBits, int nSignificantBits, boolean isCompatibleFormat) {
        final int tailZeros = ntzLong(fractBits);
        final int nFractBits = EXP_SHIFT + 1 - tailZeros;
        decimalDigitsRoundedUp = false;
        exactDecimalConversion = false;
        int nTinyBits = Math.max(0, nFractBits - binExp - 1);
        if (binExp <= MAX_SMALL_BIN_EXP && binExp >= MIN_SMALL_BIN_EXP) {
            if ((nTinyBits < FDBigInteger.LONG_5_POW.length) && ((nFractBits + N_5_BITS[nTinyBits]) < 64)) {
                if (nTinyBits == 0) {
                    int insignificant;
                    if (binExp > nSignificantBits) {
                        insignificant = insignificantDigitsForPow2(binExp - nSignificantBits - 1);
                    } else {
                        insignificant = 0;
                    }
                    if (binExp >= EXP_SHIFT) {
                        fractBits <<= (binExp - EXP_SHIFT);
                    } else {
                        fractBits >>>= (EXP_SHIFT - binExp);
                    }
                    developLongDigits(0, fractBits, insignificant);
                    return;
                }
            }
        }
        int decExp = estimateDecExp(fractBits, binExp);
        int B2, B5, S2, S5, M2, M5;
        B5 = Math.max(0, -decExp);
        B2 = B5 + nTinyBits + binExp;
        S5 = Math.max(0, decExp);
        S2 = S5 + nTinyBits;
        M5 = B5;
        M2 = B2 - nSignificantBits;
        fractBits >>>= tailZeros;
        B2 -= nFractBits - 1;
        int common2factor = Math.min(B2, S2);
        B2 -= common2factor;
        S2 -= common2factor;
        M2 -= common2factor;
        if (nFractBits == 1) {
            M2 -= 1;
        }
        if (M2 < 0) {
            B2 -= M2;
            S2 -= M2;
            M2 = 0;
        }
        int ndigit = 0;
        boolean low, high;
        long lowDigitDifference;
        int q;
        int Bbits = nFractBits + B2 + ((B5 < N_5_BITS.length) ? N_5_BITS[B5] : (B5 * 3));
        int tenSbits = S2 + 1 + (((S5 + 1) < N_5_BITS.length) ? N_5_BITS[(S5 + 1)] : ((S5 + 1) * 3));
        if (Bbits < 64 && tenSbits < 64) {
            if (Bbits < 32 && tenSbits < 32) {
                int b = ((int) fractBits * FDBigInteger.SMALL_5_POW[B5]) << B2;
                int s = FDBigInteger.SMALL_5_POW[S5] << S2;
                int m = FDBigInteger.SMALL_5_POW[M5] << M2;
                int tens = s * 10;
                ndigit = 0;
                q = b / s;
                b = 10 * (b % s);
                m *= 10;
                low = (b < m);
                high = (b + m > tens);
                if ((q == 0) && !high) {
                    decExp--;
                } else {
                    digits[ndigit++] = (char) ('0' + q);
                }
                if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
                    high = low = false;
                }
                while (!low && !high) {
                    q = b / s;
                    b = 10 * (b % s);
                    m *= 10;
                    if (m > 0L) {
                        low = (b < m);
                        high = (b + m > tens);
                    } else {
                        low = true;
                        high = true;
                    }
                    digits[ndigit++] = (char) ('0' + q);
                }
                lowDigitDifference = (b << 1) - tens;
                exactDecimalConversion = (b == 0);
            } else {
                long b = (fractBits * FDBigInteger.LONG_5_POW[B5]) << B2;
                long s = FDBigInteger.LONG_5_POW[S5] << S2;
                long m = FDBigInteger.LONG_5_POW[M5] << M2;
                long tens = s * 10L;
                ndigit = 0;
                q = (int) (b / s);
                b = 10L * (b % s);
                m *= 10L;
                low = (b < m);
                high = (b + m > tens);
                if ((q == 0) && !high) {
                    decExp--;
                } else {
                    digits[ndigit++] = (char) ('0' + q);
                }
                if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
                    high = low = false;
                }
                while (!low && !high) {
                    q = (int) (b / s);
                    b = 10 * (b % s);
                    m *= 10;
                    if (m > 0L) {
                        low = (b < m);
                        high = (b + m > tens);
                    } else {
                        low = true;
                        high = true;
                    }
                    digits[ndigit++] = (char) ('0' + q);
                }
                lowDigitDifference = (b << 1) - tens;
                exactDecimalConversion = (b == 0);
            }
        } else {
            FDBigInteger Sval = FDBigInteger.valueOfPow52(S5, S2);
            int shiftBias = Sval.getNormalizationBias();
            Sval = Sval.leftShift(shiftBias);
            FDBigInteger Bval = FDBigInteger.valueOfMulPow52(fractBits, B5, B2 + shiftBias);
            FDBigInteger Mval = FDBigInteger.valueOfPow52(M5 + 1, M2 + shiftBias + 1);
            FDBigInteger tenSval = FDBigInteger.valueOfPow52(S5 + 1, S2 + shiftBias + 1);
            ndigit = 0;
            q = Bval.quoRemIteration(Sval);
            low = (Bval.cmp(Mval) < 0);
            high = tenSval.addAndCmp(Bval, Mval) <= 0;
            if ((q == 0) && !high) {
                decExp--;
            } else {
                digits[ndigit++] = (char) ('0' + q);
            }
            if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
                high = low = false;
            }
            while (!low && !high) {
                q = Bval.quoRemIteration(Sval);
                Mval = Mval.multBy10();
                low = (Bval.cmp(Mval) < 0);
                high = tenSval.addAndCmp(Bval, Mval) <= 0;
                digits[ndigit++] = (char) ('0' + q);
            }
            if (high && low) {
                Bval = Bval.leftShift(1);
                lowDigitDifference = Bval.cmp(tenSval);
            } else {
                lowDigitDifference = 0L;
            }
            exactDecimalConversion = (Bval.cmp(FDBigInteger.ZERO) == 0);
        }
        this.decExponent = decExp + 1;
        this.firstDigitIndex = 0;
        this.nDigits = ndigit;
        if (high) {
            if (low) {
                if (lowDigitDifference == 0L) {
                    if ((digits[firstDigitIndex + nDigits - 1] & 1) != 0) {
                        roundup();
                    }
                } else if (lowDigitDifference > 0) {
                    roundup();
                }
            } else {
                roundup();
            }
        }
    }

    private void roundup() {
        int i = (firstDigitIndex + nDigits - 1);
        int q = digits[i];
        if (q == '9') {
            while (q == '9' && i > firstDigitIndex) {
                digits[i] = '0';
                q = digits[--i];
            }
            if (q == '9') {
                decExponent += 1;
                digits[firstDigitIndex] = '1';
                return;
            }
        }
        digits[i] = (char) (q + 1);
        decimalDigitsRoundedUp = true;
    }

    static int estimateDecExp(long fractBits, int binExp) {
        double d2 = Double.longBitsToDouble(EXP_ONE | (fractBits & D_SIGNIF_BIT_MASK));
        double d = (d2 - 1.5D) * 0.289529654D + 0.176091259 + (double) binExp * 0.301029995663981;
        long dBits = Double.doubleToRawLongBits(d);
        int exponent = (int) ((dBits & D_EXP_BIT_MASK) >> EXP_SHIFT) - D_EXP_BIAS;
        boolean isNeg = (dBits & D_SIGN_BIT_MASK) != 0;
        if (exponent >= 0 && exponent < 52) {
            long mask = D_SIGNIF_BIT_MASK >> exponent;
            int r = (int) (((dBits & D_SIGNIF_BIT_MASK) | FRACT_HOB) >> (EXP_SHIFT - exponent));
            return isNeg ? (((mask & dBits) == 0L) ? -r : -r - 1) : r;
        } else if (exponent < 0) {
            return (((dBits & ~D_SIGN_BIT_MASK) == 0) ? 0 : (isNeg ? -1 : 0));
        } else {
            return (int) d;
        }
    }

    private int getChars(char[] result) {
        int i = 0;
        if (isNegative) {
            result[0] = '-';
            i = 1;
        }
        if (decExponent > 0 && decExponent < 8) {
            int charLength = Math.min(nDigits, decExponent);
            System.arraycopy(digits, firstDigitIndex, result, i, charLength);
            i += charLength;
            if (charLength < decExponent) {
                charLength = decExponent - charLength;
                fillChar(result, i, i + charLength, '0');
                i += charLength;
                result[i++] = '.';
                result[i++] = '0';
            } else {
                result[i++] = '.';
                if (charLength < nDigits) {
                    int t = nDigits - charLength;
                    System.arraycopy(digits, firstDigitIndex + charLength, result, i, t);
                    i += t;
                } else {
                    result[i++] = '0';
                }
            }
        } else if (decExponent <= 0 && decExponent > -3) {
            result[i++] = '0';
            result[i++] = '.';
            if (decExponent != 0) {
                fillChar(result, i, i - decExponent, '0');
                i -= decExponent;
            }
            System.arraycopy(digits, firstDigitIndex, result, i, nDigits);
            i += nDigits;
        } else {
            result[i++] = digits[firstDigitIndex];
            result[i++] = '.';
            if (nDigits > 1) {
                System.arraycopy(digits, firstDigitIndex + 1, result, i, nDigits - 1);
                i += nDigits - 1;
            } else {
                result[i++] = '0';
            }
            result[i++] = 'E';
            int e;
            if (decExponent <= 0) {
                result[i++] = '-';
                e = -decExponent + 1;
            } else {
                e = decExponent - 1;
            }
            if (e <= 9) {
                result[i++] = (char) (e + '0');
            } else if (e <= 99) {
                result[i++] = (char) (e / 10 + '0');
                result[i++] = (char) (e % 10 + '0');
            } else {
                result[i++] = (char) (e / 100 + '0');
                e %= 100;
                result[i++] = (char) (e / 10 + '0');
                result[i++] = (char) (e % 10 + '0');
            }
        }
        return i;
    }
}
