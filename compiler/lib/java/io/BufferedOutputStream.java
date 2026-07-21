package java.io;

// java.io.BufferedOutputStream (JLS 1.0 §22.18) — buffers writes, flushing in blocks.
public class BufferedOutputStream extends FilterOutputStream {
    protected byte[] buf;
    protected int count;
    public BufferedOutputStream(OutputStream out) { this(out, 512); }
    public BufferedOutputStream(OutputStream out, int size) { super(out); buf = new byte[size]; count = 0; }

    private void flushBuffer() throws IOException {
        if (count > 0) { out.write(buf, 0, count); count = 0; }
    }
    public void write(int b) throws IOException {
        if (count >= buf.length) flushBuffer();
        buf[count] = (byte) b;
        count = count + 1;
    }
    public void write(byte[] b, int off, int len) throws IOException {
        if (len >= buf.length) { flushBuffer(); out.write(b, off, len); return; }
        if (count + len > buf.length) flushBuffer();
        System.arraycopy(b, off, buf, count, len);
        count = count + len;
    }
    public void flush() throws IOException { flushBuffer(); out.flush(); }
}
