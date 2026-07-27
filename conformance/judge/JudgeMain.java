// JudgeMain — the stitched corpus's verdict, decided ON the VM.
//
//   javelina --jre <jre.wasm> --root <parent> judge.wasm <dir>
//
// crisp-tallying-chapters §3: "the compiler's main job is to stress the VM, so the instrument
// stays a Java program compiled by javelinac. A C tool, a native binding or a new BBQ backend
// all delete the instrument and keep the scaffolding." Shell that diffs two files is the same
// mistake in a cheaper costume — it decides whether the corpus is correct while exercising
// nothing.
//
// The division of labour with conformance/cases.mk is forced, not chosen: this program cannot
// spawn javelinac (JLS 1.0 §20.16 Runtime is not part of the implemented runtime), so building
// and running the cases has to be a driver outside. Everything that is a DECISION about files
// already on disk belongs here, and this program is itself a nontrivial Java 1.0 workload —
// directory enumeration, byte-stream reads, string comparison, arrays — so the verdict costs
// the VM real work instead of costing awk none.
//
// Each Case<N>.expected is the expectation the GENERATOR composed from its snippets' `expects`,
// never observed by running anything. Comparing it against Case<N>.nojit.out and
// Case<N>.jit.out therefore checks the compiler and both execution tiers against an
// independent answer, and a disagreement between the two tiers alone names a config that is
// WRONG rather than slow.
public class JudgeMain {

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("usage: JudgeMain <generated-dir>");
            System.exit(2);
            return;
        }
        String dir = args[0];

        String[] names = new java.io.File(dir).list();
        if (names == null || names.length == 0) {
            System.out.println("  FAIL  judge: " + dir + " is empty or unreadable —");
            System.out.println("        no cases is a vacuous pass, which is the defect this exists for");
            System.exit(1);
            return;
        }

        int cases = 0, fails = 0;
        for (int i = 0; i < names.length; i++) {
            String n = names[i];
            if (!n.startsWith("Case") || !n.endsWith(".expected")) continue;
            String stem = n.substring(0, n.length() - 9);       // strip ".expected"
            cases++;

            byte[] want = read(dir + "/" + n);
            if (want == null) {
                System.out.println("  FAIL  " + stem + ": its composed expectation is unreadable");
                fails++;
                continue;
            }
            if (!check(dir, stem, "nojit", want)) fails++;
            if (!check(dir, stem, "jit",   want)) fails++;
        }

        if (cases == 0) {
            System.out.println("  FAIL  judge: no Case*.expected in " + dir);
            System.exit(1);
            return;
        }
        if (fails != 0) {
            System.out.println("  FAIL  stitched corpus: " + cases + " cases, "
                             + fails + " tier-results wrong");
            System.exit(1);
            return;
        }
        System.out.println("  ....  stitched corpus: " + cases
                         + " cases match their composed expectation on both tiers");
    }

    /** One tier of one case. Reports the FIRST differing line rather than the whole file: a
     *  case holds many stitchings, and the line number locates which one. */
    private static boolean check(String dir, String stem, String tier, byte[] want) {
        String path = dir + "/" + stem + "." + tier + ".out";
        byte[] got = read(path);
        if (got == null) {
            System.out.println("  FAIL  " + stem + " (" + tier + "): no output — it never ran");
            return false;
        }
        int line = firstDiffLine(want, got);
        if (line < 0) return true;
        System.out.println("  FAIL  " + stem + " (" + tier
                         + "): output differs from its COMPOSED expectation at line " + line);
        System.out.println("        want: " + lineAt(want, line));
        System.out.println("        got:  " + lineAt(got,  line));
        return false;
    }

    /** 1-based line number of the first difference, or -1 when the two agree exactly. A
     *  trailing-length difference counts at the first line one of them does not have. */
    private static int firstDiffLine(byte[] a, byte[] b) {
        int i = 0, j = 0, line = 1;
        while (i < a.length && j < b.length) {
            if (a[i] != b[j]) return line;
            if (a[i] == (byte) '\n') line++;
            i++; j++;
        }
        if (i == a.length && j == b.length) return -1;
        return line;
    }

    private static String lineAt(byte[] b, int line) {
        int cur = 1, start = 0;
        for (int i = 0; i < b.length; i++) {
            if (cur == line && b[i] == (byte) '\n') return new String(b, 0, start, i - start);
            if (b[i] == (byte) '\n') { cur++; start = i + 1; }
        }
        if (cur == line && start <= b.length)
            return new String(b, 0, start, b.length - start);
        return "(no such line)";
    }

    /** Whole file, or null when it cannot be read. §22.14 byte streams — the outputs are ASCII
     *  by construction, and comparing BYTES avoids a charset question the corpus never asked. */
    private static byte[] read(String path) {
        java.io.FileInputStream in = null;
        try {
            in = new java.io.FileInputStream(path);
            byte[] buf = new byte[4096];
            int len = 0;
            while (true) {
                if (len == buf.length) {
                    byte[] bigger = new byte[buf.length * 2];
                    System.arraycopy(buf, 0, bigger, 0, len);
                    buf = bigger;
                }
                int n = in.read(buf, len, buf.length - len);
                if (n < 0) break;
                len = len + n;
            }
            byte[] out = new byte[len];
            System.arraycopy(buf, 0, out, 0, len);
            return out;
        } catch (java.io.IOException e) {
            return null;
        } finally {
            if (in != null) { try { in.close(); } catch (java.io.IOException e) { } }
        }
    }
}
