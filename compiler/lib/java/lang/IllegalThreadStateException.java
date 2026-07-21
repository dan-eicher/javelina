package java.lang;

// java.lang.IllegalThreadStateException — JLS 1.0 §20.22 standard exception hierarchy.
public class IllegalThreadStateException extends IllegalArgumentException {
    public IllegalThreadStateException() { }
    public IllegalThreadStateException(String s) { super(s); }
}
