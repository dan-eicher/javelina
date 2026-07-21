package java.io;

// java.io.StringBufferInputStream (JLS 1.0 §22.13) — deprecated: an InputStream backed by a
// String, reading only the low 8 bits of each char. (Superseded by StringReader, JDK 1.1.)
public class StringBufferInputStream extends InputStream {
    protected String buffer;
    protected int pos;
    protected int count;

    public StringBufferInputStream(String s) {
        buffer = s;
        count = s.length();
    }

    public int read() {
        if (pos >= count) return -1;
        int c = buffer.charAt(pos) & 0xFF;
        pos = pos + 1;
        return c;
    }

    public int read(byte[] b, int off, int len) {
        if (pos >= count) return -1;
        if (pos + len > count) len = count - pos;
        if (len <= 0) return 0;
        int i = 0;
        while (i < len) {
            b[off + i] = (byte) (buffer.charAt(pos + i) & 0xFF);
            i = i + 1;
        }
        pos = pos + len;
        return len;
    }

    public long skip(long n) {
        if (n < 0) return 0;
        if (n > count - pos) n = count - pos;
        pos = pos + (int) n;
        return n;
    }

    public int available() { return count - pos; }

    public void reset() { pos = 0; }
}
