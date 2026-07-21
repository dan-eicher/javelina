package java.io;

// java.io.UTFDataFormatException (JLS 1.0 §22.32) — malformed modified-UTF-8 in a DataInput.
public class UTFDataFormatException extends IOException {
    public UTFDataFormatException() { super(); }
    public UTFDataFormatException(String s) { super(s); }
}
