/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   A rewrite, as the plan calls for: every published Java entry for this task is
   built on the collections framework, generics and streams, none of which exist
   in this dialect. The task description is the specification followed here:

     "Read a FASTA format file line-by-line from stdin and extract DNA sequence
      THREE"
     "use the built-in or library hash table implementation to accumulate count
      values - lookup the count for a key and update the count in the hash table"
     "count all the 1-nucleotide and 2-nucleotide sequences, and write the code
      and percentage frequency, sorted by descending frequency and then ascending
      k-nucleotide key"
     "count all the 3- 4- 6- 12- and 18-nucleotide sequences, and write the count
      and code for the specific sequences GGT GGTA GGTATT GGTATTTTAATT
      GGTATTTTAATTTATAGT"

   java.util.Hashtable is the library hash table the rule asks for. Percentages
   print to three decimals; Java 1.0 has no String.format, so `pct` does the
   fixed-point rendering the way nbody's fmt9 does.
*/

import java.io.*;
import java.util.Hashtable;
import java.util.Enumeration;

public class knucleotide {

    public static void main(String[] args) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        byte[] chunk = new byte[65536];
        int got;
        while ((got = System.in.read(chunk)) > 0) baos.write(chunk, 0, got);
        String all = baos.toString();

        /* "extract DNA sequence THREE": everything after the >THREE header line,
         * with the line breaks removed, upper-cased. */
        int h = all.indexOf(">THREE");
        int start = (h < 0) ? 0 : all.indexOf('\n', h) + 1;
        StringBuffer sb = new StringBuffer();
        for (int i = start; i < all.length(); i++) {
            char c = all.charAt(i);
            if (c != '\n' && c != '\r') sb.append(c);
        }
        String seq = sb.toString().toUpperCase();

        writeFrequencies(seq, 1);
        System.out.println();
        writeFrequencies(seq, 2);
        System.out.println();

        writeCount(seq, "GGT");
        writeCount(seq, "GGTA");
        writeCount(seq, "GGTATT");
        writeCount(seq, "GGTATTTTAATT");
        writeCount(seq, "GGTATTTTAATTTATAGT");
    }

    /* One reading frame per starting offset, combined — which for a single
     * sequence is every k-length window. */
    static Hashtable countOf(String seq, int k) {
        Hashtable t = new Hashtable();
        int last = seq.length() - k;
        for (int i = 0; i <= last; i++) {
            String key = seq.substring(i, i + k);
            Counter c = (Counter) t.get(key);
            if (c == null) t.put(key, new Counter(1));
            else c.n++;
        }
        return t;
    }

    static void writeFrequencies(String seq, int k) {
        Hashtable t = countOf(seq, k);
        int n = t.size();
        String[] keys = new String[n];
        int[] counts = new int[n];
        int m = 0;
        for (Enumeration e = t.keys(); e.hasMoreElements(); ) {
            String key = (String) e.nextElement();
            keys[m] = key;
            counts[m] = ((Counter) t.get(key)).n;
            m++;
        }
        /* descending frequency, then ascending key — insertion sort, since
         * java.util.Collections is 1.2 */
        for (int i = 1; i < n; i++) {
            String sk = keys[i];
            int sc = counts[i];
            int j = i - 1;
            while (j >= 0 && (counts[j] < sc
                          || (counts[j] == sc && keys[j].compareTo(sk) > 0))) {
                keys[j + 1] = keys[j];
                counts[j + 1] = counts[j];
                j--;
            }
            keys[j + 1] = sk;
            counts[j + 1] = sc;
        }
        int total = seq.length() - k + 1;
        for (int i = 0; i < n; i++) {
            System.out.println(keys[i] + " " + pct(100.0 * counts[i] / total));
        }
    }

    static void writeCount(String seq, String frag) {
        int k = frag.length();
        int last = seq.length() - k;
        int n = 0;
        for (int i = 0; i <= last; i++) {
            if (seq.regionMatches(i, frag, 0, k)) n++;
        }
        System.out.println(n + "\t" + frag);
    }

    /* Three fixed decimals, half-up — Java 1.0 has no String.format and
     * java.text is 1.1. */
    static String pct(double v) {
        long scaled = (long) Math.floor(v * 1000.0 + 0.5);
        long whole = scaled / 1000L;
        long frac  = scaled % 1000L;
        String f = Long.toString(frac);
        StringBuffer sb = new StringBuffer();
        sb.append(Long.toString(whole));
        sb.append('.');
        for (int i = f.length(); i < 3; i++) sb.append('0');
        sb.append(f);
        return sb.toString();
    }
}

/* A mutable count, so a bump is a field write rather than a re-boxing and a
   second hashtable store. */
class Counter {
    int n;
    Counter(int n) { this.n = n; }
}
