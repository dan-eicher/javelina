package java.io;

// java.io.EOFException (JLS 1.0 §22.29) — end of file/stream reached unexpectedly.
public class EOFException extends IOException {
    public EOFException() { super(); }
    public EOFException(String s) { super(s); }
}
