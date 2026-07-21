package java.io;

// java.io.IOException (JLS 1.0 §22.28) — signals that an I/O operation failed. Checked.
public class IOException extends Exception {
    public IOException() { super(); }
    public IOException(String s) { super(s); }
}
