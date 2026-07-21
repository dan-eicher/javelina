package java.io;

// java.io.File (JLS 1.0 §22.4) — an abstract, system-independent pathname, plus the operations that
// query or mutate the underlying host file. Path bytes are staged into the I/O linear memory (Mem) and
// the embedder's stat/action host ops (HostIO) operate on the real file (the host touches only linear
// memory). The floor is Unix-shaped, so the separator is '/'.
public class File {
    public static final char   separatorChar     = '/';
    public static final String separator         = "/";
    public static final char   pathSeparatorChar = ':';
    public static final String pathSeparator     = ":";

    private String path;

    public File(String path) {
        if (path == null) throw new NullPointerException();
        this.path = path;
    }
    public File(String parent, String child) {
        if (child == null) throw new NullPointerException();
        if (parent != null && parent.length() > 0) {
            if (parent.charAt(parent.length() - 1) == separatorChar) this.path = parent + child;
            else this.path = parent + separator + child;
        } else {
            this.path = child;
        }
    }
    public File(File parent, String child) {
        this(parent == null ? null : parent.path, child);
    }

    // ── pure pathname decomposition (no host) ──
    public String getName() {
        int i = path.lastIndexOf(separatorChar);
        return (i < 0) ? path : path.substring(i + 1);
    }
    public String getPath() { return path; }
    public String getParent() {
        int i = path.lastIndexOf(separatorChar);
        if (i < 0) return null;                 // no separator → no parent
        if (i == 0) return separator;           // "/name" → "/"
        return path.substring(0, i);
    }
    public boolean isAbsolute() { return path.length() > 0 && path.charAt(0) == separatorChar; }
    public String getAbsolutePath() {
        // §22.4: an absolute path is returned as-is; a relative one resolves against the working
        // directory — which, for the embedder's sandboxed file floor, is the sandbox root ("/").
        return isAbsolute() ? path : separator + path;
    }

    // Stage the path bytes into the I/O memory at offset 0; returns the byte count. char→low 8 bits
    // (ASCII paths — the floor convention shared with open/FileInputStream).
    private int stagePath() {
        int n = path.length();
        for (int i = 0; i < n; i++) Mem.store8(i, path.charAt(i));
        return n;
    }

    // ── host-backed queries (one stat answers exists/dir/file/read/write) ──
    public boolean exists()      { int n = stagePath(); return (HostIO.stat(0, n) & HostIO.STAT_EXISTS) != 0; }
    public boolean isDirectory() { int n = stagePath(); return (HostIO.stat(0, n) & HostIO.STAT_DIR)    != 0; }
    public boolean isFile()      { int n = stagePath(); return (HostIO.stat(0, n) & HostIO.STAT_FILE)   != 0; }
    public boolean canRead()     { int n = stagePath(); return (HostIO.stat(0, n) & HostIO.STAT_READ)   != 0; }
    public boolean canWrite()    { int n = stagePath(); return (HostIO.stat(0, n) & HostIO.STAT_WRITE)  != 0; }
    public long    length()      { int n = stagePath(); long s = HostIO.fileSize(0, n); if (s < 0) return 0; return s; }
    public long    lastModified(){ int n = stagePath(); return HostIO.fileModified(0, n); }

    // ── host-backed actions ──
    public boolean delete() { int n = stagePath(); return HostIO.unlink(0, n) == 0; }
    public boolean mkdir()  { int n = stagePath(); return HostIO.mkdir(0, n) == 0; }
    public boolean mkdirs() {
        if (exists()) return false;
        if (mkdir()) return true;
        String p = getParent();
        if (p == null) return false;
        File parent = new File(p);
        if (!parent.exists()) parent.mkdirs();
        return mkdir();
    }
    public boolean renameTo(File dest) {
        int fn = stagePath();                              // from-path at [0, fn)
        int dn = dest.path.length();
        for (int i = 0; i < dn; i++) Mem.store8(fn + i, dest.path.charAt(i));   // to-path at [fn, fn+dn)
        return HostIO.rename(0, fn, fn, dn) == 0;
    }

    // ── directory listing (§22.4): the host writes the entry names NUL-separated past the staged path ──
    public String[] list() {
        int n = stagePath();
        int total = HostIO.list(0, n, n);   // entries written starting at offset n
        if (total < 0) return null;         // not a directory
        int count = 0;
        for (int i = 0; i < total; i++) if (Mem.load8(n + i) == 0) count++;
        String[] names = new String[count];
        int idx = 0;
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < total; i++) {
            int b = Mem.load8(n + i);
            if (b == 0) { names[idx++] = sb.toString(); sb = new StringBuffer(); }
            else sb.append((char) b);
        }
        return names;
    }
    public String[] list(FilenameFilter filter) {
        String[] all = list();
        if (all == null || filter == null) return all;
        int keep = 0;
        for (int i = 0; i < all.length; i++) if (filter.accept(this, all[i])) keep++;
        String[] out = new String[keep];
        int j = 0;
        for (int i = 0; i < all.length; i++) if (filter.accept(this, all[i])) out[j++] = all[i];
        return out;
    }
    // ── Object contract (§22.4) ──
    public boolean equals(Object obj) {
        if (obj instanceof File) return path.equals(((File) obj).path);
        return false;
    }
    public int hashCode() { return path.hashCode() ^ 1234321; }
    public String toString() { return path; }
}
