package java.io;

// java.io.FileInputStream (JLS 1.0 §22.4) — reads bytes from a host file via the embedder's fd
// surface. The host reads into the I/O staging linear memory (fd_read); the guest copies those bytes
// out into the caller's byte[] (Mem.load8). One fd_read per read(byte[]) call (up to one page).
public class FileInputStream extends InputStream {
    private FileDescriptor fd;

    public FileInputStream(String name) throws IOException {
        int len = name.length();
        for (int i = 0; i < len; i++) Mem.store8(i, name.charAt(i));
        int h = HostIO.open(0, len, 0);   // flag 0 = read
        if (h < 0) throw new FileNotFoundException(name);
        fd = new FileDescriptor(h);
    }

    public FileInputStream(FileDescriptor fdObj) { fd = fdObj; }

    public int read() throws IOException {
        int n = HostIO.fd_read(fd.fd, 0, 1);
        if (n <= 0) return -1;
        return Mem.load8(0);   // 0..255, zero-extended
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || len > b.length - off) throw new IndexOutOfBoundsException();
        if (len == 0) return 0;
        int chunk = len > 65536 ? 65536 : len;   // staging memory is one page
        int n = HostIO.fd_read(fd.fd, 0, chunk);
        if (n <= 0) return -1;
        for (int j = 0; j < n; j++) b[off + j] = (byte) Mem.load8(j);
        return n;
    }

    public void close() throws IOException { HostIO.fd_close(fd.fd); }

    public final FileDescriptor getFD() throws IOException { return fd; }
}
