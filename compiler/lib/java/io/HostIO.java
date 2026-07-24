package java.io;

// javelina-internal host I/O surface (WASI-shaped): these natives are supplied by the embedder and
// operate on OFFSETS into the module's I/O staging linear memory (the GC-byte[]↔linear-memory bounce
// buffer). The guest copies a byte[] into the memory (via Mem.i32_store8), then hands the host an
// (offset, length); the host touches only linear memory (wasm_memory_data). `checksum` is a probe.
public final class HostIO {
    private HostIO() {}
    public static native int checksum(int off, int len);   // sum of `len` staging bytes at `off` (probe)

    // WASI-shaped fd surface. Offsets/lengths index the staging memory. fd_write reads the memory and
    // writes to the file; fd_read reads the file INTO the memory; both return the byte count (fd_read -1 at EOF).
    public static native int fd_open_temp();               // open a fresh read+write temp file → fd (test floor)
    public static native int open(int nameoff, int namelen, int flags);   // path bytes in memory → fd (-1 on failure); flag 0 = read, 1 = write/truncate, 2 = read+write (create, no truncate)
    public static native int fd_write(int fd, int off, int len);
    public static native int fd_read(int fd, int off, int len);
    public static native void fd_seek(int fd, int pos);
    public static native void fd_close(int fd);
    public static native long fd_size(int fd);             // current file length in bytes (flushes buffered writes) — RandomAccessFile.length

    // §22.4 File metadata/actions. The path bytes index the staging memory (nameoff, namelen), same
    // convention as `open`. `stat` returns a flags word (0 if the path does not exist), so one call
    // answers exists/isDirectory/isFile/canRead/canWrite; the STAT_* bit masks below name the bits.
    public static final int STAT_EXISTS = 1, STAT_DIR = 2, STAT_FILE = 4, STAT_READ = 8, STAT_WRITE = 16;
    public static native int  stat(int nameoff, int namelen);                 // flags; 0 if absent
    public static native long fileSize(int nameoff, int namelen);             // byte length, or -1
    public static native long fileModified(int nameoff, int namelen);         // last-modified epoch millis, or 0
    public static native int  unlink(int nameoff, int namelen);              // delete file/empty dir → 0 ok, else -1
    public static native int  mkdir(int nameoff, int namelen);               // create directory → 0 ok, else -1
    public static native int  rename(int fromoff, int fromlen, int tooff, int tolen);  // → 0 ok, else -1
    public static native int  list(int nameoff, int namelen, int outoff);    // NUL-separated entries → memory@outoff; total bytes, or -1 if not a directory

    // §20.18.7 system properties. A host function cannot build a GC String, so property text crosses
    // as bytes: java.lang.System writes the key into the staging memory and reads the value back out.
    public static native int getprop(int keyoff, int keylen, int outoff);    // value bytes → memory@outoff; length, or -1 if absent
    public static native int propnames(int outoff);                          // NUL-separated key names → memory@outoff; total bytes
}
