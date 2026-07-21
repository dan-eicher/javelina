package java.util;

// java.util.BitSet (JLS 1.0 §21.6) — a growable set of bits. Represented as an array of
// 64-bit words (the internal layout is not spec-visible; only the bit behavior is).
public class BitSet implements Cloneable {
    private long[] bits;

    public BitSet() { this(64); }

    public BitSet(int nbits) {
        int words = (nbits + 63) / 64;
        if (words < 1) words = 1;
        bits = new long[words];
    }

    private void ensure(int wordsNeeded) {
        if (wordsNeeded > bits.length) {
            long[] grown = new long[wordsNeeded];
            System.arraycopy(bits, 0, grown, 0, bits.length);
            bits = grown;
        }
    }

    public void set(int bit) {
        int w = bit / 64;
        ensure(w + 1);
        bits[w] = bits[w] | (1L << (bit % 64));
    }

    public void clear(int bit) {
        int w = bit / 64;
        if (w < bits.length) bits[w] = bits[w] & (~(1L << (bit % 64)));
    }

    public boolean get(int bit) {
        int w = bit / 64;
        return (w < bits.length) && ((bits[w] & (1L << (bit % 64))) != 0);
    }

    public void and(BitSet set) {
        int n = bits.length < set.bits.length ? bits.length : set.bits.length;
        for (int i = 0; i < n; i = i + 1) bits[i] = bits[i] & set.bits[i];
        for (int i = n; i < bits.length; i = i + 1) bits[i] = 0;
    }

    public void or(BitSet set) {
        ensure(set.bits.length);
        for (int i = 0; i < set.bits.length; i = i + 1) bits[i] = bits[i] | set.bits[i];
    }

    public void xor(BitSet set) {
        ensure(set.bits.length);
        for (int i = 0; i < set.bits.length; i = i + 1) bits[i] = bits[i] ^ set.bits[i];
    }

    public int size() { return bits.length * 64; }

    public boolean equals(Object obj) {
        if (!(obj instanceof BitSet)) return false;
        if (this == obj) return true;
        BitSet other = (BitSet) obj;
        int min = bits.length < other.bits.length ? bits.length : other.bits.length;
        for (int i = 0; i < min; i = i + 1)
            if (bits[i] != other.bits[i]) return false;
        for (int i = min; i < bits.length; i = i + 1)
            if (bits[i] != 0) return false;
        for (int i = min; i < other.bits.length; i = i + 1)
            if (other.bits[i] != 0) return false;
        return true;
    }

    public int hashCode() {
        long h = 1234;
        for (int i = bits.length; i > 0; i = i - 1)
            h = h ^ (bits[i - 1] * i);
        return (int) ((h >> 32) ^ h);
    }

    public Object clone() {
        try {
            BitSet copy = (BitSet) super.clone();
            copy.bits = new long[bits.length];
            System.arraycopy(bits, 0, copy.bits, 0, bits.length);
            return copy;
        } catch (CloneNotSupportedException e) {
            throw new InternalError();
        }
    }

    public String toString() {
        StringBuffer buf = new StringBuffer();
        buf.append("{");
        boolean first = true;
        int n = size();
        for (int i = 0; i < n; i = i + 1) {
            if (get(i)) {
                if (!first) buf.append(", ");
                buf.append(String.valueOf(i));
                first = false;
            }
        }
        buf.append("}");
        return buf.toString();
    }
}
