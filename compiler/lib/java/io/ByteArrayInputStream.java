package java.io;

// java.io.ByteArrayInputStream (JLS 1.0 §22.6) — an InputStream backed by a byte array.
public class ByteArrayInputStream extends InputStream {
    protected byte[] buf;
    protected int pos;
    protected int count;
    protected int markpos = 0;

    public ByteArrayInputStream(byte[] buf) {
        this.buf = buf; this.pos = 0; this.count = buf.length;
    }
    public ByteArrayInputStream(byte[] buf, int offset, int length) {
        this.buf = buf; this.pos = offset;
        this.count = (offset + length < buf.length) ? (offset + length) : buf.length;
        this.markpos = offset;
    }

    public int read() {
        if (pos < count) { int b = buf[pos] & 0xff; pos = pos + 1; return b; }
        return -1;
    }

    public int read(byte[] b, int off, int len) {
        if (pos >= count) return -1;
        if (pos + len > count) len = count - pos;
        if (len <= 0) return 0;
        System.arraycopy(buf, pos, b, off, len);
        pos = pos + len;
        return len;
    }

    public long skip(long n) {
        int k = (int) n;
        if (pos + k > count) k = count - pos;
        if (k < 0) return 0;
        pos = pos + k;
        return k;
    }

    public int available() { return count - pos; }
    public boolean markSupported() { return true; }
    public void mark(int readAheadLimit) { markpos = pos; }
    public void reset() { pos = markpos; }
}
