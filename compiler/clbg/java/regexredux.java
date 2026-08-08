/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   contributed by Francois Green

   PORT of the published regexredux javaxint-6 program. That entry is written
   in modern Java — `var`, Streams, Map.entry, List.of — and runs its work on
   CompletableFuture.supplyAsync and parallelStream. Neither the syntax nor the
   concurrency is portable here, and the comparison this harness makes is
   against single-threaded published rows anyway, so this is the same ALGORITHM
   written straight:

     - read the whole subject from stdin,
     - strip FASTA headers and newlines with `>.*\n|\n`,
     - count matches of each of the nine variants,
     - apply the five replacements in order, each to the previous result,
     - print the counts, then the three lengths.

   The regexes, their order, the replacement strings and the output format are
   the published ones. java.util.regex is a post-1.0 extension in this runtime,
   built on the PEG machine, and this program is what it is for.
*/

import java.io.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class regexredux {

    public static void main(String[] args) throws IOException {

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        byte[] buf = new byte[65536];
        int count;
        while ((count = System.in.read(buf)) > 0) {
            baos.write(buf, 0, count);
        }
        String input = baos.toString();

        String sequence = Pattern.compile(">.*\n|\n").matcher(input).replaceAll("");

        String[] variants = new String[9];
        variants[0] = "agggtaaa|tttaccct";
        variants[1] = "[cgt]gggtaaa|tttaccc[acg]";
        variants[2] = "a[act]ggtaaa|tttacc[agt]t";
        variants[3] = "ag[act]gtaaa|tttac[agt]ct";
        variants[4] = "agg[act]taaa|ttta[agt]cct";
        variants[5] = "aggg[acg]aaa|ttt[cgt]ccct";
        variants[6] = "agggt[cgt]aa|tt[acg]accct";
        variants[7] = "agggta[cgt]a|t[acg]taccct";
        variants[8] = "agggtaa[cgt]|[acg]ttaccct";

        for (int i = 0; i < variants.length; i++) {
            Matcher m = Pattern.compile(variants[i]).matcher(sequence);
            int n = 0;
            while (m.find()) n++;
            System.out.println(variants[i] + " " + n);
        }

        String[] pat = new String[5];
        String[] rep = new String[5];
        pat[0] = "tHa[Nt]";                 rep[0] = "<4>";
        pat[1] = "aND|caN|Ha[DS]|WaS";      rep[1] = "<3>";
        pat[2] = "a[NSt]|BY";               rep[2] = "<2>";
        pat[3] = "<[^>]*>";                 rep[3] = "|";
        pat[4] = "\\|[^|][^|]*\\|";         rep[4] = "-";

        String replacements = sequence;
        for (int i = 0; i < pat.length; i++) {
            replacements = Pattern.compile(pat[i]).matcher(replacements).replaceAll(rep[i]);
        }

        System.out.println();
        System.out.println(input.length());
        System.out.println(sequence.length());
        System.out.println(replacements.length());
    }
}
