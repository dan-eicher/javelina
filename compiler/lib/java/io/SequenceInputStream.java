package java.io;

import java.util.Enumeration;
import java.util.Vector;

// java.io.SequenceInputStream (JLS 1.0 §22.12) — the logical concatenation of a series of input
// streams: reads exhaust each stream in turn (closing it), then advance to the next.
public class SequenceInputStream extends InputStream {
    private Enumeration e;
    private InputStream in;

    public SequenceInputStream(InputStream s1, InputStream s2) {
        Vector v = new Vector(2);
        v.addElement(s1);
        v.addElement(s2);
        e = v.elements();
        try { nextStream(); } catch (IOException ex) { }
    }

    public SequenceInputStream(Enumeration e) {
        this.e = e;
        try { nextStream(); } catch (IOException ex) { }
    }

    final void nextStream() throws IOException {
        if (in != null) in.close();
        if (e.hasMoreElements()) in = (InputStream) e.nextElement();
        else in = null;
    }

    public int read() throws IOException {
        if (in == null) return -1;
        int c = in.read();
        if (c == -1) { nextStream(); return read(); }
        return c;
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (in == null) return -1;
        if (len == 0) return 0;
        int n = in.read(b, off, len);
        if (n <= 0) { nextStream(); return read(b, off, len); }
        return n;
    }

    public int available() throws IOException {
        return (in == null) ? 0 : in.available();
    }

    public void close() throws IOException {
        while (in != null) nextStream();
    }
}
