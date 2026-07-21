package java.io;

// java.io.PushbackInputStream (JLS 1.0 §22.10) — a FilterInputStream with a one-byte pushback
// buffer, so a look-ahead byte can be returned to the stream (unread) and re-read.
public class PushbackInputStream extends FilterInputStream {
    protected int pushBack = -1;      // the pushed-back byte, or -1 when empty

    public PushbackInputStream(InputStream in) { super(in); }

    public int read() throws IOException {
        if (pushBack != -1) { int c = pushBack; pushBack = -1; return c; }
        return in.read();
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (len <= 0) return 0;
        int c = read();                       // honours the pushback for the first byte
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

    public void unread(int ch) throws IOException {
        if (pushBack != -1) throw new IOException("Push back buffer is full");
        pushBack = ch & 0xFF;
    }

    public int available() throws IOException {
        return (pushBack == -1) ? in.available() : in.available() + 1;
    }

    public long skip(long n) throws IOException {
        long skipped = 0;
        while (skipped < n && read() != -1) skipped = skipped + 1;
        return skipped;
    }

    public boolean markSupported() { return false; }
}
