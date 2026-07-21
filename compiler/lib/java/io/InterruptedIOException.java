package java.io;

// java.io.InterruptedIOException (JLS 1.0 §22.31) — an I/O operation was interrupted.
public class InterruptedIOException extends IOException {
    public int bytesTransferred = 0;
    public InterruptedIOException() { super(); }
    public InterruptedIOException(String s) { super(s); }
}
