// GenMain — install the snippet libraries, stitch, write the cases.
//
//   javelina --jre build/jre.wasm gen.wasm --root <outdir-parent> <outdir> [depth] [cap] [perCase]
//
// Adding a snippet library is ONE LINE below. Nothing scans and nothing reflects, so the
// enumeration is a function of this list and of each library's registration order, and two
// runs of the same build write byte-identical files.
//
// Everything this program writes is COMPOSED. It never runs a generated case to learn an
// answer; if it did, the corpus would pin whatever javelinac does today, including a
// miscompile, and would be a snapshot rather than an oracle.
public class GenMain {

    // Defaults. Every one is a knob the harness may override on the command line; they are
    // stated here so a bare run is reproducible.
    private static final int DEF_DEPTH    = 2;     // levels of holes below a root snippet
    private static final int DEF_CAP      = 250;   // stitchings kept per (type, depth)
    private static final int DEF_PER_CASE = 150;   // stitchings per Case file

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("usage: GenMain <outdir> [depth] [capPerType] [perCase]");
            System.exit(2);
            return;
        }
        String dir     = args[0];
        int    depth   = arg(args, 1, DEF_DEPTH);
        int    cap     = arg(args, 2, DEF_CAP);
        int    perCase = arg(args, 3, DEF_PER_CASE);

        // ---- the snippet libraries ------------------------------------------------------
        Registry reg = new Registry();
        BootSnippets.install(reg);                  // the leaves everything else stitches onto
        LibIntro.install(reg);                      // JLS chapters 1-2, Introduction / Grammars
        Lib3.install(reg);                          // JLS chapter 3, Lexical Structure
        Lib4.install(reg);                          // JLS chapter 4, Types, Values, Variables
        Lib5.install(reg);                          // JLS chapter 5, Conversions and Promotions
        // ADD LIBRARIES HERE:  XxxSnippets.install(reg);

        // ---- enumerate ------------------------------------------------------------------
        Stitcher st = new Stitcher(reg, cap);
        Stitching[] all = st.stitchAll(depth);

        // ---- write ----------------------------------------------------------------------
        Emit em = new Emit(dir, perCase);
        int files;
        try {
            files = em.write(all);
            Emit.writeText(dir, "CAP-DROPS.txt", drops(st, reg, depth, perCase, all.length));
            Emit.writeText(dir, "SECTIONS.tsv", manifest(all));
        } catch (java.io.IOException e) {
            System.out.println("gen: FAILED writing to " + dir + ": " + e.getClass().getName()
                               + " " + e.getMessage());
            System.exit(1);
            return;
        }

        // ---- report ---------------------------------------------------------------------
        String[] secs = em.sectionsCovered();
        System.out.println("gen: snippets=" + reg.size() + " types=" + reg.types().length
                           + " depth=" + depth + " cap=" + cap + " perCase=" + perCase);
        System.out.println("gen: stitchings=" + all.length + " cases=" + files
                           + " lines=" + em.lines());
        StringBuffer sb = new StringBuffer();
        sb.append("gen: sections=").append(secs.length);
        for (int i = 0; i < secs.length; i++) sb.append(' ').append(secs[i]);
        System.out.println(sb.toString());
        System.out.println("gen: cap cuts=" + st.cutCount() + " dropped=" + st.dropCount()
                           + " (recorded in CAP-DROPS.txt)");
        String[] log = st.dropLog();
        for (int i = 0; i < log.length; i++) System.out.println("gen: " + log[i]);
    }

    /** `section <TAB> snippet-id` for every ROOT snippet that reached a written case, one
     *  line per pair, deduplicated.
     *
     *  This is what makes a declared cardinality checkable. A `// JLS 5.1.2` marker says only
     *  that SOMETHING covering §5.1.2 was written — and §5.1.2 names nineteen conversions, so
     *  one surviving case out of nineteen produces exactly the same marker as all nineteen.
     *  The per-type cap makes that the normal outcome rather than a corner: this enumeration
     *  wrote 17 of the 19, and the ledger read COVERED.
     *
     *  ROOT snippets only. A stitching's children are the values its holes were filled with,
     *  and a §5.1.2 conversion appearing as some other snippet's operand is not a case FOR
     *  §5.1.2 — counting it would let a section reach its declared count without ever being
     *  the thing under test. */
    private static String manifest(Stitching[] all) {
        java.util.Vector seen = new java.util.Vector();
        for (int i = 0; i < all.length; i++) {
            Snippet  s    = all[i].snippet();
            String[] secs = s.sections();
            for (int j = 0; j < secs.length; j++) {
                String line = secs[j] + "\t" + s.id();
                if (!seen.contains(line)) seen.addElement(line);
            }
        }
        String[] lines = new String[seen.size()];
        seen.copyInto(lines);
        Strs.sort(lines);
        StringBuffer b = new StringBuffer();
        b.append("# SECTIONS.tsv -- section <TAB> snippet id, for every root snippet in a written\n");
        b.append("# case. Read by conformance/check-cardinality.sh, which compares the count per\n");
        b.append("# section against the number the spec states for it.\n");
        for (int i = 0; i < lines.length; i++) b.append(lines[i]).append("\n");
        return b.toString();
    }

    /** The cap record, written beside the cases. It is written even when nothing was cut,
     *  because the absence of a record is indistinguishable from a complete enumeration and
     *  a truncated enumeration that says nothing reads as full coverage. */
    private static String drops(Stitcher st, Registry reg, int depth, int perCase, int kept) {
        StringBuffer b = new StringBuffer();
        b.append("# CAP-DROPS.txt -- what the per-type cap discarded from this enumeration.\n");
        b.append("#\n");
        b.append("# snippets  ").append(reg.size()).append("\n");
        b.append("# types     ").append(reg.types().length).append("\n");
        b.append("# depth     ").append(depth).append("\n");
        b.append("# cap       ").append(st.capPerType()).append("  (per type, per depth)\n");
        b.append("# perCase   ").append(perCase).append("\n");
        b.append("# kept      ").append(kept).append("  stitchings written\n");
        b.append("# cuts      ").append(st.cutCount()).append("\n");
        b.append("# dropped   ").append(st.dropCount()).append("\n");
        b.append("\n");
        String[] log = st.dropLog();
        if (log.length == 0) {
            b.append("the cap never fired: this enumeration is COMPLETE at depth ")
             .append(depth).append(".\n");
        } else {
            for (int i = 0; i < log.length; i++) b.append(log[i]).append("\n");
        }
        return b.toString();
    }

    private static int arg(String[] args, int i, int dflt) {
        if (args.length <= i) return dflt;
        try {
            return Integer.parseInt(args[i]);
        } catch (NumberFormatException e) {
            System.out.println("gen: argument " + i + " (" + args[i] + ") is not a number; using " + dflt);
            return dflt;
        }
    }
}
