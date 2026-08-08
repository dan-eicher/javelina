/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   modified by Mehmet D. AKIN

   PORT of the published fasta javaxint-2 program. Three changes, none of them
   touching the generator or the output:

     - `frequency` was a nested class (Java 1.1). It is hoisted to a top-level
       class; the fields, constructor and every use are unchanged.
     - `new frequency[]{ ... }` is an array initializer inside a creation
       expression. JLS 1.0 section 15.8 has no ArrayInitializer alternative for
       that production — it arrived in 1.1 — so each array is declared and
       filled element by element, in the same order.
     - `String.getBytes()` returning a fresh byte[] is Java 1.1. JLS 1.0 has
       only `getBytes(int srcBegin, int srcEnd, byte[] dst, int dstBegin)`,
       which copies the low byte of each char — exactly what this ASCII data
       needs — so `bytesOf` wraps it.

   The pseudo-random sequence, the weights, the cumulative selection, the line
   length and the buffering are all as published, so the output is identical.
*/

import java.io.IOException;
import java.io.OutputStream;

class fasta {
    public static final int IM = 139968;
    public static final int IA = 3877;
    public static final int IC = 29573;
    public static int last = 42;

    public static final int LINE_LENGTH = 60;

    // pseudo-random number generator
    public static final double random(double max) {
        last = (last * IA + IC) % IM;
        return max * last / IM;
    }

    /* JLS 1.0 section 22.12: String has getBytes(srcBegin, srcEnd, dst,
     * dstBegin), which writes the low byte of each character. The no-argument
     * form that allocates is 1.1. */
    static byte[] bytesOf(String s) {
        byte[] b = new byte[s.length()];
        s.getBytes(0, s.length(), b, 0);
        return b;
    }

    // Weighted selection from alphabet
    public static String ALU =
              "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
            + "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
            + "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT"
            + "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
            + "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG"
            + "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
            + "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
    public static byte[] ALUB = bytesOf(ALU);

    public static final frequency[] IUB = makeIUB();
    public static final frequency[] HomoSapiens = makeHomoSapiens();

    static frequency[] makeIUB() {
        frequency[] a = new frequency[15];
        a[0]  = new frequency('a', 0.27);
        a[1]  = new frequency('c', 0.12);
        a[2]  = new frequency('g', 0.12);
        a[3]  = new frequency('t', 0.27);

        a[4]  = new frequency('B', 0.02);
        a[5]  = new frequency('D', 0.02);
        a[6]  = new frequency('H', 0.02);
        a[7]  = new frequency('K', 0.02);
        a[8]  = new frequency('M', 0.02);
        a[9]  = new frequency('N', 0.02);
        a[10] = new frequency('R', 0.02);
        a[11] = new frequency('S', 0.02);
        a[12] = new frequency('V', 0.02);
        a[13] = new frequency('W', 0.02);
        a[14] = new frequency('Y', 0.02);
        return a;
    }

    static frequency[] makeHomoSapiens() {
        frequency[] a = new frequency[4];
        a[0] = new frequency('a', 0.3029549426680d);
        a[1] = new frequency('c', 0.1979883004921d);
        a[2] = new frequency('g', 0.1975473066391d);
        a[3] = new frequency('t', 0.3015094502008d);
        return a;
    }

    public static void makeCumulative(frequency[] a) {
        double cp = 0.0;
        for (int i = 0; i < a.length; i++) {
            cp += a[i].p;
            a[i].p = cp;
        }
    }

    // naive
    public final static byte selectRandom(frequency[] a) {
        int len = a.length;
        double r = random(1.0);
        for (int i = 0; i < len; i++)
            if (r < a[i].p)
                return a[i].c;
        return a[len - 1].c;
    }

    static int BUFFER_SIZE = 1024;
    static int index = 0;
    static byte[] bbuffer = new byte[BUFFER_SIZE];
    static final void makeRandomFasta(String id, String desc, frequency[] a, int n, OutputStream writer) throws IOException
    {
        index = 0;
        int m = 0;
        String descStr = ">" + id + " " + desc + '\n';
        writer.write(bytesOf(descStr));
        while (n > 0) {
            if (n < LINE_LENGTH) m = n;  else m = LINE_LENGTH;
            if(BUFFER_SIZE - index < m){
                writer.write(bbuffer, 0, index);
                index = 0;
            }
            for (int i = 0; i < m; i++) {
                bbuffer[index++] = selectRandom(a);
            }
            bbuffer[index++] = '\n';
            n -= LINE_LENGTH;
        }
        if(index != 0) writer.write(bbuffer, 0, index);
    }

    static final void makeRepeatFasta(String id, String desc, String alu, int n, OutputStream writer) throws IOException
    {
        index = 0;
        int m = 0;
        int k = 0;
        int kn = ALUB.length;
        String descStr = ">" + id + " " + desc + '\n';
        writer.write(bytesOf(descStr));
        while (n > 0) {
            if (n < LINE_LENGTH) m = n; else m = LINE_LENGTH;
            if(BUFFER_SIZE - index < m){
                writer.write(bbuffer, 0, index);
                index = 0;
            }
            for (int i = 0; i < m; i++) {
                if (k == kn) k = 0;
                bbuffer[index++] = ALUB[k];
                k++;
            }
            bbuffer[index++] = '\n';
            n -= LINE_LENGTH;
        }
        if(index != 0) writer.write(bbuffer, 0, index);
    }

    public static void main(String[] args) throws IOException {
        makeCumulative(HomoSapiens);
        makeCumulative(IUB);
        int n = 2500000;
        if (args.length > 0)
            n = Integer.parseInt(args[0]);
        OutputStream out = System.out;
        makeRepeatFasta("ONE", "Homo sapiens alu", ALU, n * 2, out);
        makeRandomFasta("TWO", "IUB ambiguity codes", IUB, n * 3, out);
        makeRandomFasta("THREE", "Homo sapiens frequency", HomoSapiens, n * 5, out);
        out.close();
    }
}

/* Hoisted from `fasta.frequency` — a nested class is Java 1.1. Unchanged
   otherwise: same fields, same constructor, same byte narrowing of the char. */
class frequency {
    public byte c;
    public double p;

    public frequency(char c, double p) {
        this.c = (byte)c;
        this.p = p;
    }
}
