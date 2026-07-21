package java.io;

// java.io.OutputStream (JLS 1.0 §22.15) — the abstract superclass of all byte output streams.
// A subclass need only implement write(int); the rest is layered over it.
public abstract class OutputStream {
    public abstract void write(int b) throws IOException;

    public void write(byte[] b) throws IOException {
        write(b, 0, b.length);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        for (int i = 0; i < len; i = i + 1) write(b[off + i]);
    }

    public void flush() throws IOException { }
    public void close() throws IOException { }
}
