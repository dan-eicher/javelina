// Tsv -- read a tab-separated table, skipping comment and blank lines.
//
// Shared by every gate below so the ledger, the cardinality table and the generator's manifest
// are parsed once rather than three times in three dialects of awk. Tab-separated because that
// is the format already on disk; the columns are positional and the LAST column may itself
// contain prose, so a row is split on tabs and never on anything else.
public class Tsv {

    private Tsv() {}

    /** Every non-comment row of `path`, each already split into its tab-separated fields.
     *  Returns null when the file cannot be read -- the callers treat that as a hard failure
     *  rather than an empty table, because a missing inventory is not an empty inventory. */
    public static String[][] read(String path) {
        byte[] b = readAll(path);
        if (b == null) return null;
        String text = new String(b, 0, 0, b.length);

        java.util.Vector rows = new java.util.Vector();
        int i = 0, n = text.length();
        while (i < n) {
            int e = text.indexOf('\n', i);
            if (e < 0) e = n;
            String line = text.substring(i, e);
            i = e + 1;
            if (line.length() == 0) continue;
            if (line.charAt(0) == '#') continue;
            if (isBlank(line)) continue;
            rows.addElement(split(line));
        }
        String[][] out = new String[rows.size()][];
        rows.copyInto(out);
        return out;
    }

    private static boolean isBlank(String s) {
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c != ' ' && c != '\t' && c != '\r') return false;
        }
        return true;
    }

    /** Split on TAB only. A trailing \r is dropped so a file written with CRLF parses the
     *  same as one written with LF -- §3.4 treats both as line terminators, and the ledger is
     *  edited by hand. */
    public static String[] split(String line) {
        java.util.Vector f = new java.util.Vector();
        int i = 0, n = line.length();
        while (true) {
            int e = line.indexOf('\t', i);
            if (e < 0) { f.addElement(trimCR(line.substring(i, n))); break; }
            f.addElement(trimCR(line.substring(i, e)));
            i = e + 1;
        }
        String[] out = new String[f.size()];
        f.copyInto(out);
        return out;
    }

    private static String trimCR(String s) {
        int n = s.length();
        while (n > 0 && s.charAt(n - 1) == '\r') n--;
        return s.substring(0, n);
    }

    /** The whole file as bytes, or null. */
    public static byte[] readAll(String path) {
        java.io.FileInputStream in = null;
        try {
            in = new java.io.FileInputStream(path);
            byte[] buf = new byte[8192];
            int len = 0;
            while (true) {
                if (len == buf.length) {
                    byte[] bigger = new byte[buf.length * 2];
                    System.arraycopy(buf, 0, bigger, 0, len);
                    buf = bigger;
                }
                int r = in.read(buf, len, buf.length - len);
                if (r < 0) break;
                len = len + r;
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

    /** The last non-comment, non-blank line -- how uncovered-floor carries its number under a
     *  long explanatory header. */
    public static String lastValueLine(String path) {
        String[][] rows = read(path);
        if (rows == null || rows.length == 0) return null;
        String[] last = rows[rows.length - 1];
        return last.length > 0 ? last[0].trim() : null;
    }

    public static int parseInt(String s, int dflt) {
        try { return Integer.parseInt(s.trim()); } catch (NumberFormatException e) { return dflt; }
    }
}
