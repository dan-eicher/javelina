package java.io;

// java.io.ByteArrayOutputStream (JLS 1.0 §22.18) — an OutputStream that accumulates bytes
// into a growable in-memory buffer. Ported minus `synchronized`.
public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf;
    protected int count;

    public ByteArrayOutputStream() { this(32); }
    public ByteArrayOutputStream(int size) { buf = new byte[size]; count = 0; }

    private void ensure(int newcount) {
        if (newcount > buf.length) {
            int cap = buf.length << 1;
            if (cap < newcount) cap = newcount;
            byte[] nb = new byte[cap];
            System.arraycopy(buf, 0, nb, 0, count);
            buf = nb;
        }
    }

    public void write(int b) {
        int newcount = count + 1;
        ensure(newcount);
        buf[count] = (byte) b;
        count = newcount;
    }

    public void write(byte[] b, int off, int len) {
        if (len == 0) return;
        int newcount = count + len;
        ensure(newcount);
        System.arraycopy(b, off, buf, count, len);
        count = newcount;
    }

    public void writeTo(OutputStream out) throws IOException { out.write(buf, 0, count); }

    public void reset() { count = 0; }

    public byte[] toByteArray() {
        byte[] nb = new byte[count];
        System.arraycopy(buf, 0, nb, 0, count);
        return nb;
    }

    public int size() { return count; }

    public String toString() { return new String(buf, 0, 0, count); }
    public String toString(int hibyte) { return new String(buf, hibyte, 0, count); }
}
