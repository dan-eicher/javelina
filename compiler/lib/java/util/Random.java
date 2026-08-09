package java.util;

// java.util.Random (JLS 1.0 §21.9) — the 48-bit linear congruential PRNG. Ported minus
// `synchronized`. Constants and update are the spec's exactly, so the sequence is the
// canonical one (e.g. new Random(0).nextLong() == -4962768465676381896L).
public class Random {
    private long seed;

    private static final long multiplier = 0x5DEECE66DL;
    private static final long addend     = 0xBL;
    private static final long mask       = (1L << 48) - 1;

    private double  nextNextGaussian;
    private boolean haveNextNextGaussian = false;

    public Random() { this(System.currentTimeMillis()); }

    public Random(long seed) { setSeed(seed); }

    public void setSeed(long seed) {
        this.seed = (seed ^ multiplier) & mask;
        haveNextNextGaussian = false;
    }

    protected int next(int bits) {
        seed = (seed * multiplier + addend) & mask;
        return (int) (seed >>> (48 - bits));
    }

    public int nextInt() { return next(32); }

    public long nextLong() {
        return ((long) (next(32)) << 32) + next(32);
    }

    public float nextFloat() {
        int i = next(24);
        return i / ((float) (1 << 24));
    }

    public double nextDouble() {
        long l = ((long) (next(26)) << 27) + next(27);
        return l / (double) (1L << 53);
    }

    public double nextGaussian() {
        if (haveNextNextGaussian) {
            haveNextNextGaussian = false;
            return nextNextGaussian;
        } else {
            double v1;
            double v2;
            double s;
            do {
                v1 = 2 * nextDouble() - 1;
                v2 = 2 * nextDouble() - 1;
                s = v1 * v1 + v2 * v2;
            } while (s >= 1);
            double m = Math.sqrt(-2 * Math.log(s) / s);
            nextNextGaussian = v2 * m;
            haveNextNextGaussian = true;
            return v1 * m;
        }
    }
}
