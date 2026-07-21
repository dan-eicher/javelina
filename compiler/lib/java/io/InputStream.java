package java.io;

// java.io.InputStream (JLS 1.0 §22.3) — the abstract superclass of all byte input streams.
// A subclass need only implement read(); the rest is layered over it.
public abstract class InputStream {
    public abstract int read() throws IOException;

    public int read(byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (len <= 0) return 0;
        int c = read();
        if (c == -1) return -1;
        b[off] = (byte) c;
        int i = 1;
        while (i < len) {
            c = read();
            if (c == -1) break;
            b[off + i] = (byte) c;
            i = i + 1;
        }
        return i;
    }

    public long skip(long n) throws IOException {
        long remaining = n;
        while (remaining > 0) {
            if (read() == -1) break;
            remaining = remaining - 1;
        }
        return n - remaining;
    }

    public int available() throws IOException { return 0; }
    public void close() throws IOException { }
    public void mark(int readlimit) { }
    public void reset() throws IOException { throw new IOException("mark/reset not supported"); }
    public boolean markSupported() { return false; }
}
