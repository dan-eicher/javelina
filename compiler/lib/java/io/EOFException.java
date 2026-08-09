package java.io;

// java.io.EOFException (JLS 1.0 §22.28) — end of file/stream reached unexpectedly.
public class EOFException extends IOException {
    public EOFException() { super(); }
    public EOFException(String s) { super(s); }
}
