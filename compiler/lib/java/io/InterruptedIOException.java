package java.io;

// java.io.InterruptedIOException (JLS 1.0 §22.30) — an I/O operation was interrupted.
public class InterruptedIOException extends IOException {
    public int bytesTransferred = 0;
    public InterruptedIOException() { super(); }
    public InterruptedIOException(String s) { super(s); }
}
