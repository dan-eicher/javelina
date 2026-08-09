package java.io;

// java.io.BufferedInputStream (JLS 1.0 §22.10) — buffers reads from the wrapped stream.
//
// mark(readlimit) promises that reset() still works after up to `readlimit` more bytes are
// read, so the buffer is not a fixed window: while a mark is live the marked region is first
// SHIFTED down to reclaim the space before it, and only when the mark already sits at 0 does
// the buffer GROW, capped at readlimit. The mark expires exactly when honouring it would
// exceed that promise. Dropping the mark as soon as the initial buffer filled — the old
// "single-buffer window" — silently broke every reader that marks more than 2048 bytes ahead.
public class BufferedInputStream extends FilterInputStream {
    protected byte[] buf;
    protected int count;      // valid bytes in buf
    protected int pos;        // next byte to return
    protected int markpos = -1;
    protected int marklimit;

    public BufferedInputStream(InputStream in) { this(in, 2048); }
    public BufferedInputStream(InputStream in, int size) { super(in); buf = new byte[size]; }

    private void fill() throws IOException {
        if (markpos < 0) {
            pos = 0;                                    // no mark: reuse the whole buffer
        } else if (pos >= buf.length) {                 // no room left, and a mark is live
            if (markpos > 0) {                          // reclaim what precedes the mark
                int kept = pos - markpos;
                System.arraycopy(buf, markpos, buf, 0, kept);
                pos = kept; markpos = 0;
            } else if (buf.length >= marklimit) {
                markpos = -1; pos = 0;                  // readlimit already spent: the mark expires
            } else {
                int grown = pos * 2;                    // grow, never past readlimit
                if (grown < 0 || grown > marklimit) grown = marklimit;
                byte[] nbuf = new byte[grown];
                System.arraycopy(buf, 0, nbuf, 0, pos);
                buf = nbuf;
            }
        }
        count = pos;
        int n = in.read(buf, pos, buf.length - pos);
        if (n > 0) count = pos + n;
    }

    public int read() throws IOException {
        if (pos >= count) { fill(); if (pos >= count) return -1; }
        int b = buf[pos] & 0xff;
        pos = pos + 1;
        return b;
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (len <= 0) return 0;
        int total = 0;
        while (total < len) {
            if (pos >= count) { fill(); if (pos >= count) break; }
            int avail = count - pos;
            int n = (len - total < avail) ? (len - total) : avail;
            System.arraycopy(buf, pos, b, off + total, n);
            pos = pos + n;
            total = total + n;
        }
        return (total == 0) ? -1 : total;
    }

    public int available() throws IOException { return (count - pos) + in.available(); }
    public boolean markSupported() { return true; }
    public void mark(int readlimit) { marklimit = readlimit; markpos = pos; }
    public void reset() throws IOException {
        if (markpos < 0) throw new IOException("resetting to invalid mark");
        pos = markpos;
    }
    public long skip(long n) throws IOException {
        long remaining = n;
        while (remaining > 0) { if (read() == -1) break; remaining = remaining - 1; }
        return n - remaining;
    }
}
