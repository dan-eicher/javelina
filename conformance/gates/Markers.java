// Markers -- every `// JLS <section>` claim in the corpus, and which artifact makes it.
//
// Coverage is COMPUTED from the artifacts, never typed. A test claims the sections it covers
// with a marker; this collects them so the join can reconcile the claims against the ledger in
// both directions -- a claim with no row, and a row with no claim.
//
// Three trees are scanned, and each earns its place:
//   conformance/jls        the cited-section suite
//   conformance/reject     programs that must NOT compile -- a rule of the form "a compile-time
//                          error occurs" is covered by a program that fails for that reason,
//                          and there is no runnable assertion to write instead
//   conformance/generated  the stitched cases, whose markers Emit writes from each snippet's
//                          sections[] -- which is the whole path by which stitching becomes
//                          counted coverage
public class Markers {

    public String[] section;    // parallel arrays; one entry per (section, artifact) pair
    public String[] artifact;
    public int      count;

    public static Markers scan(String dir) {
        Markers m = new Markers();
        java.util.Vector secs = new java.util.Vector();
        java.util.Vector arts = new java.util.Vector();

        scanDir(dir + "/jls",       secs, arts);
        scanDir(dir + "/reject",    secs, arts);
        scanDir(dir + "/generated", secs, arts);

        m.count = secs.size();
        m.section = new String[m.count];  secs.copyInto(m.section);
        m.artifact = new String[m.count]; arts.copyInto(m.artifact);
        return m;
    }

    private static void scanDir(String path, java.util.Vector secs, java.util.Vector arts) {
        String[] names = new java.io.File(path).list();
        if (names == null) return;                 // an absent tree is not an error: reject/
        for (int i = 0; i < names.length; i++) {   // and generated/ may legitimately be empty
            String n = names[i];
            if (!n.endsWith(".java")) continue;
            String artifact = n.substring(0, n.length() - 5);
            byte[] b = Tsv.readAll(path + "/" + n);
            if (b == null) continue;
            collect(new String(b, 0, 0, b.length), artifact, secs, arts);
        }
    }

    /** Every `// JLS <number>` in `text`, deduplicated per (section, artifact). The marker must
     *  be the whole comment's opening -- a line MENTIONING "JLS 5.1.2" in prose is not a claim,
     *  which is why the prefix is anchored rather than searched for anywhere. */
    private static void collect(String text, String artifact,
                                java.util.Vector secs, java.util.Vector arts) {
        int i = 0, n = text.length();
        while (i < n) {
            int e = text.indexOf('\n', i);
            if (e < 0) e = n;
            String line = text.substring(i, e);
            i = e + 1;

            int p = 0;
            while (p < line.length() && (line.charAt(p) == ' ' || line.charAt(p) == '\t')) p++;
            if (p + 2 > line.length() || line.charAt(p) != '/' || line.charAt(p + 1) != '/') continue;
            p += 2;
            while (p < line.length() && line.charAt(p) == ' ') p++;
            if (!line.startsWith("JLS", p)) continue;
            p += 3;
            if (p >= line.length() || line.charAt(p) != ' ') continue;
            while (p < line.length() && line.charAt(p) == ' ') p++;

            int s = p;
            while (p < line.length()) {
                char c = line.charAt(p);
                if ((c >= '0' && c <= '9') || c == '.') p++; else break;
            }
            if (p == s) continue;
            String sec = line.substring(s, p);
            while (sec.length() > 0 && sec.charAt(sec.length() - 1) == '.')
                sec = sec.substring(0, sec.length() - 1);   // "§5.1." -> "5.1"
            if (sec.length() == 0) continue;

            if (!has(secs, arts, sec, artifact)) { secs.addElement(sec); arts.addElement(artifact); }
        }
    }

    private static boolean has(java.util.Vector secs, java.util.Vector arts,
                               String sec, String artifact) {
        for (int i = 0; i < secs.size(); i++)
            if (secs.elementAt(i).equals(sec) && arts.elementAt(i).equals(artifact)) return true;
        return false;
    }

    public boolean covers(String sec) {
        for (int i = 0; i < count; i++) if (section[i].equals(sec)) return true;
        return false;
    }

    /** The artifacts claiming `sec`, comma-joined -- what the ledger's reason column records.
     *
     *  SORTED, because this string is written into a checked-in file: File.list returns
     *  directory order, so an unsorted join would rewrite dozens of rows with the same set of
     *  artifacts in a different sequence every time the directory changed, and the diff would
     *  say coverage moved when nothing had. */
    public String by(String sec) {
        java.util.Vector v = new java.util.Vector();
        for (int i = 0; i < count; i++) if (section[i].equals(sec)) v.addElement(artifact[i]);
        String[] a = new String[v.size()];
        v.copyInto(a);
        for (int i = 1; i < a.length; i++) {          // insertion sort: JLS 1.0 has no Arrays
            String x = a[i];
            int j = i - 1;
            while (j >= 0 && a[j].compareTo(x) > 0) { a[j + 1] = a[j]; j--; }
            a[j + 1] = x;
        }
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < a.length; i++) {
            if (i > 0) b.append(',');
            b.append(a[i]);
        }
        return b.toString();
    }
}
