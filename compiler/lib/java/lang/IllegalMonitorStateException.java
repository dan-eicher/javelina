package java.lang;

// java.lang.IllegalMonitorStateException — JLS 1.0 §20.22 standard exception hierarchy.
public class IllegalMonitorStateException extends RuntimeException {
    public IllegalMonitorStateException() { }
    public IllegalMonitorStateException(String s) { super(s); }
}
