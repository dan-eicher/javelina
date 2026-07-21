package java.lang;

// Faithful port of OpenJDK jdk.internal.math.FDBigInteger — the arbitrary-precision integer used by
// FloatingDecimal for correctly-rounded decimal<->binary conversion (JLS 1.0 has no java.math.BigInteger).
// Adapted to JLS 1.0: no `new int[]{...}` (hoisted to declaration initializers), no java.util.Arrays
// (copyOf/zeroFill helpers), no Integer.numberOfLeadingZeros (nlz helper), debug toBigInteger/toHexString dropped.
// value = (sum data[i]<<(32*i)) << (offset*32); data[nWords-1]!=0; immutable instances are shared/cached.
class FDBigInteger {

    private static final long LONG_MASK = 0xffffffffL;

    static final int[] SMALL_5_POW = {
        1, 5, 25, 125, 625, 3125, 15625, 78125, 390625, 1953125, 9765625, 48828125, 244140625, 1220703125
    };
    static final long[] LONG_5_POW = {
        1L, 5L, 25L, 125L, 625L, 3125L, 15625L, 78125L, 390625L, 1953125L, 9765625L, 48828125L,
        244140625L, 1220703125L, 6103515625L, 30517578125L, 152587890625L, 762939453125L,
        3814697265625L, 19073486328125L, 95367431640625L, 476837158203125L, 2384185791015625L,
        11920928955078125L, 59604644775390625L, 298023223876953125L, 1490116119384765625L
    };   // 5^0..5^26 (27 entries) — MUST match N_5_BITS.length; the fast-path guard pairs
         // `nTinyBits < LONG_5_POW.length` with N_5_BITS[nTinyBits], so a longer table would
         // let nTinyBits index N_5_BITS out of bounds (the 0.1f dtoa trap).

    private static final int MAX_FIVE_POW = 340;
    private static final FDBigInteger[] POW_5_CACHE;
    static {
        POW_5_CACHE = new FDBigInteger[MAX_FIVE_POW];
        int i = 0;
        while (i < SMALL_5_POW.length) {
            int[] one = { SMALL_5_POW[i] };
            FDBigInteger pow5 = new FDBigInteger(one, 0);
            pow5.makeImmutable();
            POW_5_CACHE[i] = pow5;
            i++;
        }
        FDBigInteger prev = POW_5_CACHE[i - 1];
        while (i < MAX_FIVE_POW) {
            prev = prev.mult(5);
            POW_5_CACHE[i] = prev;
            prev.makeImmutable();
            i++;
        }
    }

    static final FDBigInteger ZERO;
    static {
        int[] empty = new int[0];
        ZERO = new FDBigInteger(empty, 0);
        ZERO.makeImmutable();
    }

    private int[] data;   // value: data[0] is least significant
    private int offset;   // number of least significant zero-padding ints
    private int nWords;   // data[nWords-1] != 0; nWords==0 means zero
    private boolean isImmutable = false;

    // ── JLS-1.0 helpers (stand in for java.util.Arrays / Integer.numberOfLeadingZeros) ──
    private static int[] copyOf(int[] src, int n) {
        int[] r = new int[n];
        int m = (src.length < n) ? src.length : n;
        System.arraycopy(src, 0, r, 0, m);
        return r;
    }
    private static void zeroFill(int[] a, int from, int to) {
        for (int i = from; i < to; i++) a[i] = 0;
    }
    private static int nlz(int i) {   // Integer.numberOfLeadingZeros
        if (i == 0) return 32;
        int n = 1;
        if (i >>> 16 == 0) { n += 16; i <<= 16; }
        if (i >>> 24 == 0) { n += 8; i <<= 8; }
        if (i >>> 28 == 0) { n += 4; i <<= 4; }
        if (i >>> 30 == 0) { n += 2; i <<= 2; }
        n -= i >>> 31;
        return n;
    }

    private FDBigInteger(int[] data, int offset) {
        this.data = data;
        this.offset = offset;
        this.nWords = data.length;
        trimLeadingZeros();
    }

    FDBigInteger(long lValue, char[] digits, int kDigits, int nDigits) {
        int n = Math.max((nDigits + 8) / 9, 2);
        data = new int[n];
        data[0] = (int) lValue;
        data[1] = (int) (lValue >>> 32);
        offset = 0;
        nWords = 2;
        int i = kDigits;
        int limit = nDigits - 5;
        int v;
        while (i < limit) {
            int ilim = i + 5;
            v = (int) digits[i++] - (int) '0';
            while (i < ilim) {
                v = 10 * v + (int) digits[i++] - (int) '0';
            }
            multAddMe(100000, v);
        }
        int factor = 1;
        v = 0;
        while (i < nDigits) {
            v = 10 * v + (int) digits[i++] - (int) '0';
            factor *= 10;
        }
        if (factor != 1) {
            multAddMe(factor, v);
        }
        trimLeadingZeros();
    }

    static FDBigInteger valueOfPow52(int p5, int p2) {
        if (p5 != 0) {
            if (p2 == 0) {
                return big5pow(p5);
            } else if (p5 < SMALL_5_POW.length) {
                int pow5 = SMALL_5_POW[p5];
                int wordcount = p2 >> 5;
                int bitcount = p2 & 0x1f;
                if (bitcount == 0) {
                    int[] r = { pow5 };
                    return new FDBigInteger(r, wordcount);
                } else {
                    int[] r = { pow5 << bitcount, pow5 >>> (32 - bitcount) };
                    return new FDBigInteger(r, wordcount);
                }
            } else {
                return big5pow(p5).leftShift(p2);
            }
        } else {
            return valueOfPow2(p2);
        }
    }

    static FDBigInteger valueOfMulPow52(long value, int p5, int p2) {
        int v0 = (int) value;
        int v1 = (int) (value >>> 32);
        int wordcount = p2 >> 5;
        int bitcount = p2 & 0x1f;
        if (p5 != 0) {
            if (p5 < SMALL_5_POW.length) {
                long pow5 = SMALL_5_POW[p5] & LONG_MASK;
                long carry = (v0 & LONG_MASK) * pow5;
                v0 = (int) carry;
                carry >>>= 32;
                carry = (v1 & LONG_MASK) * pow5 + carry;
                v1 = (int) carry;
                int v2 = (int) (carry >>> 32);
                if (bitcount == 0) {
                    int[] r = { v0, v1, v2 };
                    return new FDBigInteger(r, wordcount);
                } else {
                    int[] r = {
                        v0 << bitcount,
                        (v1 << bitcount) | (v0 >>> (32 - bitcount)),
                        (v2 << bitcount) | (v1 >>> (32 - bitcount)),
                        v2 >>> (32 - bitcount)
                    };
                    return new FDBigInteger(r, wordcount);
                }
            } else {
                FDBigInteger pow5 = big5pow(p5);
                int[] r;
                if (v1 == 0) {
                    r = new int[pow5.nWords + 1 + ((p2 != 0) ? 1 : 0)];
                    mult(pow5.data, pow5.nWords, v0, r);
                } else {
                    r = new int[pow5.nWords + 2 + ((p2 != 0) ? 1 : 0)];
                    mult(pow5.data, pow5.nWords, v0, v1, r);
                }
                return (new FDBigInteger(r, pow5.offset)).leftShift(p2);
            }
        } else if (p2 != 0) {
            if (bitcount == 0) {
                int[] r = { v0, v1 };
                return new FDBigInteger(r, wordcount);
            } else {
                int[] r = {
                    v0 << bitcount,
                    (v1 << bitcount) | (v0 >>> (32 - bitcount)),
                    v1 >>> (32 - bitcount)
                };
                return new FDBigInteger(r, wordcount);
            }
        }
        int[] r = { v0, v1 };
        return new FDBigInteger(r, 0);
    }

    private static FDBigInteger valueOfPow2(int p2) {
        int wordcount = p2 >> 5;
        int bitcount = p2 & 0x1f;
        int[] r = { 1 << bitcount };
        return new FDBigInteger(r, wordcount);
    }

    private void trimLeadingZeros() {
        int i = nWords;
        if (i > 0 && (data[--i] == 0)) {
            while (i > 0 && data[i - 1] == 0) {
                i--;
            }
            this.nWords = i;
            if (i == 0) {
                this.offset = 0;
            }
        }
    }

    int getNormalizationBias() {
        if (nWords == 0) {
            throw new IllegalArgumentException("Zero value cannot be normalized");
        }
        int zeros = nlz(data[nWords - 1]);
        return (zeros < 4) ? 28 + zeros : zeros - 4;
    }

    private static void leftShift(int[] src, int idx, int[] result, int bitcount, int anticount, int prev) {
        for (; idx > 0; idx--) {
            int v = (prev << bitcount);
            prev = src[idx - 1];
            v |= (prev >>> anticount);
            result[idx] = v;
        }
        int v = prev << bitcount;
        result[0] = v;
    }

    FDBigInteger leftShift(int shift) {
        if (shift == 0 || nWords == 0) {
            return this;
        }
        int wordcount = shift >> 5;
        int bitcount = shift & 0x1f;
        if (this.isImmutable) {
            if (bitcount == 0) {
                return new FDBigInteger(copyOf(data, nWords), offset + wordcount);
            } else {
                int anticount = 32 - bitcount;
                int idx = nWords - 1;
                int prev = data[idx];
                int hi = prev >>> anticount;
                int[] result;
                if (hi != 0) {
                    result = new int[nWords + 1];
                    result[nWords] = hi;
                } else {
                    result = new int[nWords];
                }
                leftShift(data, idx, result, bitcount, anticount, prev);
                return new FDBigInteger(result, offset + wordcount);
            }
        } else {
            if (bitcount != 0) {
                int anticount = 32 - bitcount;
                if ((data[0] << bitcount) == 0) {
                    int idx = 0;
                    int prev = data[idx];
                    for (; idx < nWords - 1; idx++) {
                        int v = (prev >>> anticount);
                        prev = data[idx + 1];
                        v |= (prev << bitcount);
                        data[idx] = v;
                    }
                    int v = prev >>> anticount;
                    data[idx] = v;
                    if (v == 0) {
                        nWords--;
                    }
                    offset++;
                } else {
                    int idx = nWords - 1;
                    int prev = data[idx];
                    int hi = prev >>> anticount;
                    int[] result = data;
                    int[] src = data;
                    if (hi != 0) {
                        if (nWords == data.length) {
                            int[] grown = new int[nWords + 1];
                            data = grown;
                            result = grown;
                        }
                        result[nWords++] = hi;
                    }
                    leftShift(src, idx, result, bitcount, anticount, prev);
                }
            }
            offset += wordcount;
            return this;
        }
    }

    private int size() {
        return nWords + offset;
    }

    int quoRemIteration(FDBigInteger S) throws IllegalArgumentException {
        int thSize = this.size();
        int sSize = S.size();
        if (thSize < sSize) {
            int p = multAndCarryBy10(this.data, this.nWords, this.data);
            if (p != 0) {
                this.data[nWords++] = p;
            } else {
                trimLeadingZeros();
            }
            return 0;
        } else if (thSize > sSize) {
            throw new IllegalArgumentException("disparate values");
        }
        long q = (this.data[this.nWords - 1] & LONG_MASK) / (S.data[S.nWords - 1] & LONG_MASK);
        long diff = multDiffMe(q, S);
        if (diff != 0L) {
            long sum = 0L;
            int tStart = S.offset - this.offset;
            int[] sd = S.data;
            int[] td = this.data;
            while (sum == 0L) {
                for (int sIndex = 0, tIndex = tStart; tIndex < this.nWords; sIndex++, tIndex++) {
                    sum += (td[tIndex] & LONG_MASK) + (sd[sIndex] & LONG_MASK);
                    td[tIndex] = (int) sum;
                    sum >>>= 32;
                }
                q -= 1;
            }
        }
        multAndCarryBy10(this.data, this.nWords, this.data);
        trimLeadingZeros();
        return (int) q;
    }

    FDBigInteger multBy10() {
        if (nWords == 0) {
            return this;
        }
        if (isImmutable) {
            int[] res = new int[nWords + 1];
            res[nWords] = multAndCarryBy10(data, nWords, res);
            return new FDBigInteger(res, offset);
        } else {
            int p = multAndCarryBy10(this.data, this.nWords, this.data);
            if (p != 0) {
                if (nWords == data.length) {
                    if (data[0] == 0) {
                        System.arraycopy(data, 1, data, 0, --nWords);
                        offset++;
                    } else {
                        data = copyOf(data, data.length + 1);
                    }
                }
                data[nWords++] = p;
            } else {
                trimLeadingZeros();
            }
            return this;
        }
    }

    FDBigInteger multByPow52(int p5, int p2) {
        if (this.nWords == 0) {
            return this;
        }
        FDBigInteger res = this;
        if (p5 != 0) {
            int[] r;
            int extraSize = (p2 != 0) ? 1 : 0;
            if (p5 < SMALL_5_POW.length) {
                r = new int[this.nWords + 1 + extraSize];
                mult(this.data, this.nWords, SMALL_5_POW[p5], r);
                res = new FDBigInteger(r, this.offset);
            } else {
                FDBigInteger pow5 = big5pow(p5);
                r = new int[this.nWords + pow5.size() + extraSize];
                mult(this.data, this.nWords, pow5.data, pow5.nWords, r);
                res = new FDBigInteger(r, this.offset + pow5.offset);
            }
        }
        return res.leftShift(p2);
    }

    private static void mult(int[] s1, int s1Len, int[] s2, int s2Len, int[] dst) {
        for (int i = 0; i < s1Len; i++) {
            long v = s1[i] & LONG_MASK;
            long p = 0L;
            for (int j = 0; j < s2Len; j++) {
                p += (dst[i + j] & LONG_MASK) + v * (s2[j] & LONG_MASK);
                dst[i + j] = (int) p;
                p >>>= 32;
            }
            dst[i + s2Len] = (int) p;
        }
    }

    FDBigInteger leftInplaceSub(FDBigInteger subtrahend) {
        FDBigInteger minuend;
        if (this.isImmutable) {
            minuend = new FDBigInteger(copyOf(this.data, this.data.length), this.offset);
        } else {
            minuend = this;
        }
        int offsetDiff = subtrahend.offset - minuend.offset;
        int[] sData = subtrahend.data;
        int[] mData = minuend.data;
        int subLen = subtrahend.nWords;
        int minLen = minuend.nWords;
        if (offsetDiff < 0) {
            int rLen = minLen - offsetDiff;
            if (rLen < mData.length) {
                System.arraycopy(mData, 0, mData, -offsetDiff, minLen);
                zeroFill(mData, 0, -offsetDiff);
            } else {
                int[] r = new int[rLen];
                System.arraycopy(mData, 0, r, -offsetDiff, minLen);
                mData = r;
                minuend.data = mData;
            }
            minuend.offset = subtrahend.offset;
            minuend.nWords = minLen = rLen;
            offsetDiff = 0;
        }
        long borrow = 0L;
        int mIndex = offsetDiff;
        for (int sIndex = 0; sIndex < subLen && mIndex < minLen; sIndex++, mIndex++) {
            long diff = (mData[mIndex] & LONG_MASK) - (sData[sIndex] & LONG_MASK) + borrow;
            mData[mIndex] = (int) diff;
            borrow = diff >> 32;
        }
        for (; borrow != 0 && mIndex < minLen; mIndex++) {
            long diff = (mData[mIndex] & LONG_MASK) + borrow;
            mData[mIndex] = (int) diff;
            borrow = diff >> 32;
        }
        minuend.trimLeadingZeros();
        return minuend;
    }

    FDBigInteger rightInplaceSub(FDBigInteger subtrahend) {
        FDBigInteger minuend = this;
        if (subtrahend.isImmutable) {
            subtrahend = new FDBigInteger(copyOf(subtrahend.data, subtrahend.data.length), subtrahend.offset);
        }
        int offsetDiff = minuend.offset - subtrahend.offset;
        int[] sData = subtrahend.data;
        int[] mData = minuend.data;
        int subLen = subtrahend.nWords;
        int minLen = minuend.nWords;
        if (offsetDiff < 0) {
            int rLen = minLen;
            if (rLen < sData.length) {
                System.arraycopy(sData, 0, sData, -offsetDiff, subLen);
                zeroFill(sData, 0, -offsetDiff);
            } else {
                int[] r = new int[rLen];
                System.arraycopy(sData, 0, r, -offsetDiff, subLen);
                sData = r;
                subtrahend.data = sData;
            }
            subtrahend.offset = minuend.offset;
            subLen -= offsetDiff;
            offsetDiff = 0;
        } else {
            int rLen = minLen + offsetDiff;
            if (rLen >= sData.length) {
                sData = copyOf(sData, rLen);
                subtrahend.data = sData;
            }
        }
        int sIndex = 0;
        long borrow = 0L;
        for (; sIndex < offsetDiff; sIndex++) {
            long diff = 0L - (sData[sIndex] & LONG_MASK) + borrow;
            sData[sIndex] = (int) diff;
            borrow = diff >> 32;
        }
        for (int mIndex = 0; mIndex < minLen; sIndex++, mIndex++) {
            long diff = (mData[mIndex] & LONG_MASK) - (sData[sIndex] & LONG_MASK) + borrow;
            sData[sIndex] = (int) diff;
            borrow = diff >> 32;
        }
        subtrahend.nWords = sIndex;
        subtrahend.trimLeadingZeros();
        return subtrahend;
    }

    private static int checkZeroTail(int[] a, int from) {
        while (from > 0) {
            if (a[--from] != 0) {
                return 1;
            }
        }
        return 0;
    }

    int cmp(FDBigInteger other) {
        int aSize = nWords + offset;
        int bSize = other.nWords + other.offset;
        if (aSize > bSize) {
            return 1;
        } else if (aSize < bSize) {
            return -1;
        }
        int aLen = nWords;
        int bLen = other.nWords;
        while (aLen > 0 && bLen > 0) {
            int a = data[--aLen];
            int b = other.data[--bLen];
            if (a != b) {
                return ((a & LONG_MASK) < (b & LONG_MASK)) ? -1 : 1;
            }
        }
        if (aLen > 0) {
            return checkZeroTail(data, aLen);
        }
        if (bLen > 0) {
            return -checkZeroTail(other.data, bLen);
        }
        return 0;
    }

    int cmpPow52(int p5, int p2) {
        if (p5 == 0) {
            int wordcount = p2 >> 5;
            int bitcount = p2 & 0x1f;
            int sz = this.nWords + this.offset;
            if (sz > wordcount + 1) {
                return 1;
            } else if (sz < wordcount + 1) {
                return -1;
            }
            int a = this.data[this.nWords - 1];
            int b = 1 << bitcount;
            if (a != b) {
                return ((a & LONG_MASK) < (b & LONG_MASK)) ? -1 : 1;
            }
            return checkZeroTail(this.data, this.nWords - 1);
        }
        return this.cmp(big5pow(p5).leftShift(p2));
    }

    int addAndCmp(FDBigInteger x, FDBigInteger y) {
        FDBigInteger big;
        FDBigInteger small;
        int xSize = x.size();
        int ySize = y.size();
        int bSize;
        int sSize;
        if (xSize >= ySize) {
            big = x; small = y; bSize = xSize; sSize = ySize;
        } else {
            big = y; small = x; bSize = ySize; sSize = xSize;
        }
        int thSize = this.size();
        if (bSize == 0) {
            return thSize == 0 ? 0 : 1;
        }
        if (sSize == 0) {
            return this.cmp(big);
        }
        if (bSize > thSize) {
            return -1;
        }
        if (bSize + 1 < thSize) {
            return 1;
        }
        long top = (big.data[big.nWords - 1] & LONG_MASK);
        if (sSize == bSize) {
            top += (small.data[small.nWords - 1] & LONG_MASK);
        }
        if ((top >>> 32) == 0) {
            if (((top + 1) >>> 32) == 0) {
                if (bSize < thSize) {
                    return 1;
                }
                long v = (this.data[this.nWords - 1] & LONG_MASK);
                if (v < top) {
                    return -1;
                }
                if (v > top + 1) {
                    return 1;
                }
            }
        } else {
            if (bSize + 1 > thSize) {
                return -1;
            }
            top >>>= 32;
            long v = (this.data[this.nWords - 1] & LONG_MASK);
            if (v < top) {
                return -1;
            }
            if (v > top + 1) {
                return 1;
            }
        }
        return this.cmp(big.add(small));
    }

    void makeImmutable() {
        this.isImmutable = true;
    }

    private FDBigInteger mult(int i) {
        if (this.nWords == 0) {
            return this;
        }
        int[] r = new int[nWords + 1];
        mult(data, nWords, i, r);
        return new FDBigInteger(r, offset);
    }

    private FDBigInteger mult(FDBigInteger other) {
        if (this.nWords == 0) {
            return this;
        }
        if (this.size() == 1) {
            return other.mult(data[0]);
        }
        if (other.nWords == 0) {
            return other;
        }
        if (other.size() == 1) {
            return this.mult(other.data[0]);
        }
        int[] r = new int[nWords + other.nWords];
        mult(this.data, this.nWords, other.data, other.nWords, r);
        return new FDBigInteger(r, this.offset + other.offset);
    }

    private FDBigInteger add(FDBigInteger other) {
        FDBigInteger big, small;
        int bigLen, smallLen;
        int tSize = this.size();
        int oSize = other.size();
        if (tSize >= oSize) {
            big = this; bigLen = tSize; small = other; smallLen = oSize;
        } else {
            big = other; bigLen = oSize; small = this; smallLen = tSize;
        }
        int[] r = new int[bigLen + 1];
        int i = 0;
        long carry = 0L;
        for (; i < smallLen; i++) {
            carry += (i < big.offset ? 0L : (big.data[i - big.offset] & LONG_MASK))
                   + ((i < small.offset ? 0L : (small.data[i - small.offset] & LONG_MASK)));
            r[i] = (int) carry;
            carry >>= 32;
        }
        for (; i < bigLen; i++) {
            carry += (i < big.offset ? 0L : (big.data[i - big.offset] & LONG_MASK));
            r[i] = (int) carry;
            carry >>= 32;
        }
        r[bigLen] = (int) carry;
        return new FDBigInteger(r, 0);
    }

    private void multAddMe(int iv, int addend) {
        long v = iv & LONG_MASK;
        long p = v * (data[0] & LONG_MASK) + (addend & LONG_MASK);
        data[0] = (int) p;
        p >>>= 32;
        for (int i = 1; i < nWords; i++) {
            p += v * (data[i] & LONG_MASK);
            data[i] = (int) p;
            p >>>= 32;
        }
        if (p != 0L) {
            data[nWords++] = (int) p;
        }
    }

    private long multDiffMe(long q, FDBigInteger S) {
        long diff = 0L;
        if (q != 0) {
            int deltaSize = S.offset - this.offset;
            if (deltaSize >= 0) {
                int[] sd = S.data;
                int[] td = this.data;
                for (int sIndex = 0, tIndex = deltaSize; sIndex < S.nWords; sIndex++, tIndex++) {
                    diff += (td[tIndex] & LONG_MASK) - q * (sd[sIndex] & LONG_MASK);
                    td[tIndex] = (int) diff;
                    diff >>= 32;
                }
            } else {
                deltaSize = -deltaSize;
                int[] rd = new int[nWords + deltaSize];
                int sIndex = 0;
                int rIndex = 0;
                int[] sd = S.data;
                for (; rIndex < deltaSize && sIndex < S.nWords; sIndex++, rIndex++) {
                    diff -= q * (sd[sIndex] & LONG_MASK);
                    rd[rIndex] = (int) diff;
                    diff >>= 32;
                }
                int tIndex = 0;
                int[] td = this.data;
                for (; sIndex < S.nWords; sIndex++, tIndex++, rIndex++) {
                    diff += (td[tIndex] & LONG_MASK) - q * (sd[sIndex] & LONG_MASK);
                    rd[rIndex] = (int) diff;
                    diff >>= 32;
                }
                this.nWords += deltaSize;
                this.offset -= deltaSize;
                this.data = rd;
            }
        }
        return diff;
    }

    private static int multAndCarryBy10(int[] src, int srcLen, int[] dst) {
        long carry = 0;
        for (int i = 0; i < srcLen; i++) {
            long product = (src[i] & LONG_MASK) * 10L + carry;
            dst[i] = (int) product;
            carry = product >>> 32;
        }
        return (int) carry;
    }

    private static void mult(int[] src, int srcLen, int value, int[] dst) {
        long val = value & LONG_MASK;
        long carry = 0;
        for (int i = 0; i < srcLen; i++) {
            long product = (src[i] & LONG_MASK) * val + carry;
            dst[i] = (int) product;
            carry = product >>> 32;
        }
        dst[srcLen] = (int) carry;
    }

    private static void mult(int[] src, int srcLen, int v0, int v1, int[] dst) {
        long v = v0 & LONG_MASK;
        long carry = 0;
        for (int j = 0; j < srcLen; j++) {
            long product = v * (src[j] & LONG_MASK) + carry;
            dst[j] = (int) product;
            carry = product >>> 32;
        }
        dst[srcLen] = (int) carry;
        v = v1 & LONG_MASK;
        carry = 0;
        for (int j = 0; j < srcLen; j++) {
            long product = (dst[j + 1] & LONG_MASK) + v * (src[j] & LONG_MASK) + carry;
            dst[j + 1] = (int) product;
            carry = product >>> 32;
        }
        dst[srcLen + 1] = (int) carry;
    }

    private static FDBigInteger big5pow(int p) {
        if (p < MAX_FIVE_POW) {
            return POW_5_CACHE[p];
        }
        return big5powRec(p);
    }

    private static FDBigInteger big5powRec(int p) {
        if (p < MAX_FIVE_POW) {
            return POW_5_CACHE[p];
        }
        int q = p >> 1;
        int r = p - q;
        FDBigInteger bigq = big5powRec(q);
        if (r < SMALL_5_POW.length) {
            return bigq.mult(SMALL_5_POW[r]);
        } else {
            return bigq.mult(big5powRec(r));
        }
    }
}
