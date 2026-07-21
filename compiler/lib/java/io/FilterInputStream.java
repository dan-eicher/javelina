package java.io;

// java.io.FilterInputStream (JLS 1.0 §22.4) — wraps another InputStream, delegating every
// operation. Subclasses (Buffered/Data/Pushback/...) override to transform the stream.
public class FilterInputStream extends InputStream {
    protected InputStream in;
    protected FilterInputStream(InputStream in) { this.in = in; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public long skip(long n) throws IOException { return in.skip(n); }
    public int available() throws IOException { return in.available(); }
    public void close() throws IOException { in.close(); }
    public void mark(int readlimit) { in.mark(readlimit); }
    public void reset() throws IOException { in.reset(); }
    public boolean markSupported() { return in.markSupported(); }
}
