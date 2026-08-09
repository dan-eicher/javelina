package java.io;

import javelina.simd.Mem;

// java.io.FileOutputStream (JLS 1.0 §22.16) — writes bytes to a host file via the embedder's fd
// surface. A byte[] crosses to the host by copying into the I/O staging linear memory (Mem.store8)
// one 64 KiB page at a time, then a single fd_write over that region.
public class FileOutputStream extends OutputStream {
    private FileDescriptor fd;

    public FileOutputStream(String name) throws IOException {
        int len = name.length();
        for (int i = 0; i < len; i++) Mem.i32_store8(i, name.charAt(i));
        int h = HostIO.open(0, len, 1);   // flag 1 = write/truncate
        if (h < 0) throw new FileNotFoundException(name);
        fd = new FileDescriptor(h);
    }

    public FileOutputStream(FileDescriptor fdObj) { fd = fdObj; }

    public void write(int b) throws IOException {
        Mem.i32_store8(0, b);
        HostIO.fd_write(fd.fd, 0, 1);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || len > b.length - off) throw new IndexOutOfBoundsException();
        int done = 0;
        while (done < len) {
            int chunk = len - done;
            if (chunk > 65536) chunk = 65536;   // staging memory is one page
            Mem.copyIn(b, off + done, chunk, 0);
            HostIO.fd_write(fd.fd, 0, chunk);
            done += chunk;
        }
    }

    public void close() throws IOException { HostIO.fd_close(fd.fd); }

    public final FileDescriptor getFD() throws IOException { return fd; }
}
