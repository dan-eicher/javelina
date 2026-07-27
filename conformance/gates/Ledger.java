// Ledger -- the inventory, and the three transcription invariants that make a dropped row
// impossible to miss.
//
// crisp-tallying-chapters §2: "The source is a PDF, so there is no grep. The inventory is
// transcribed from the ToC -- which is ordered, numbered, and page-stamped, and therefore
// self-checking. Three invariants, each of which fails on exactly the mistake a transcriber
// makes... These are the answer to 'how do we know you didn't skip half of it'. Not assurance
// -- an invariant that breaks."
public class Ledger {

    /** Chapter start pages, read off the ToC chapter lines (java-langspec-1.0.pdf pp.7-19).
     *  The 23rd entry is a sentinel: the body ends at 765 and the Index begins at 767. */
    static final int[] CHAPTER_START = {
        0, 1, 7, 11, 29, 51, 77, 113, 127, 183, 193, 201, 215, 237,
        263, 301, 383, 399, 419, 433, 455, 615, 665, 767
    };

    public String[] sec;         // section number, e.g. "5.1.2"
    public String[] title;
    public int[]    page;
    public String[] status;      // COVERED | N/A | UNCOVERED
    public String[] reason;      // template ids for COVERED, a written reason for N/A
    public String[] behaviour;   // the testable claim, or ""
    public int      rows;

    /** Parse, or null when the file is unreadable. */
    public static Ledger load(String path) {
        String[][] r = Tsv.read(path);
        if (r == null) return null;
        Ledger L = new Ledger();
        L.rows = r.length;
        L.sec = new String[L.rows]; L.title = new String[L.rows]; L.page = new int[L.rows];
        L.status = new String[L.rows]; L.reason = new String[L.rows];
        L.behaviour = new String[L.rows];
        for (int i = 0; i < L.rows; i++) {
            String[] f = r[i];
            L.sec[i]       = f.length > 0 ? f[0] : "";
            L.title[i]     = f.length > 1 ? f[1] : "";
            L.page[i]      = f.length > 2 ? Tsv.parseInt(f[2], -1) : -1;
            L.status[i]    = f.length > 3 ? f[3] : "";
            L.reason[i]    = f.length > 4 ? f[4] : "";
            L.behaviour[i] = f.length > 5 ? f[5] : "";
        }
        return L;
    }

    public int indexOf(String section) {
        for (int i = 0; i < rows; i++) if (sec[i].equals(section)) return i;
        return -1;
    }

    /** The chapter number a section belongs to, or -1 if its number is malformed. */
    public static int chapterOf(String section) {
        int dot = section.indexOf('.');
        String head = dot < 0 ? section : section.substring(0, dot);
        return Tsv.parseInt(head, -1);
    }

    /** The parent section ("5.1.2" -> "5.1"), or "" for a top-level one. */
    public static String parentOf(String section) {
        int dot = section.lastIndexOf('.');
        return dot < 0 ? "" : section.substring(0, dot);
    }

    /** The trailing component as a number ("5.1.2" -> 2). */
    public static int leafOf(String section) {
        int dot = section.lastIndexOf('.');
        return Tsv.parseInt(dot < 0 ? section : section.substring(dot + 1), -1);
    }
}
