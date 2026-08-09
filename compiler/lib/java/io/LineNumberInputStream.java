package java.io;

// java.io.LineNumberInputStream (JLS 1.0 §22.12) — a FilterInputStream that counts lines,
// normalising every line terminator ('\r', '\n', "\r\n") to a single '\n'. (Deprecated in 1.1.)
public class LineNumberInputStream extends FilterInputStream {
    private int pushBack = -1;
    private int lineNumber;
    private int markLineNumber;

    public LineNumberInputStream(InputStream in) { super(in); }

    public int read() throws IOException {
        int c;
        if (pushBack != -1) { c = pushBack; pushBack = -1; }
        else c = in.read();
        if (c == '\r') {
            pushBack = in.read();
            if (pushBack == '\n') pushBack = -1;   // absorb the LF of a CRLF pair
            lineNumber = lineNumber + 1;
            return '\n';
        }
        if (c == '\n') {
            lineNumber = lineNumber + 1;
            return '\n';
        }
        return c;
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
        long skipped = 0;
        while (skipped < n && read() != -1) skipped = skipped + 1;
        return skipped;
    }

    public int getLineNumber() { return lineNumber; }
    public void setLineNumber(int lineNumber) { this.lineNumber = lineNumber; }

    public int available() throws IOException {
        return (pushBack == -1) ? in.available() : in.available() + 1;
    }

    public void mark(int readlimit) { markLineNumber = lineNumber; in.mark(readlimit); }

    public void reset() throws IOException {
        lineNumber = markLineNumber;
        pushBack = -1;
        in.reset();
    }
}
