package java.io;

// java.io.FilterOutputStream (JLS 1.0 §22.19) — wraps another OutputStream, delegating.
public class FilterOutputStream extends OutputStream {
    protected OutputStream out;
    public FilterOutputStream(OutputStream out) { this.out = out; }
    public void write(int b) throws IOException { out.write(b); }
    public void write(byte[] b, int off, int len) throws IOException {
        for (int i = 0; i < len; i = i + 1) write(b[off + i]);   // §22.16: byte-at-a-time via write(int)
    }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException { flush(); out.close(); }
}
