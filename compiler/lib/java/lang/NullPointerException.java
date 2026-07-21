package java.lang;

// java.lang.NullPointerException — JLS 1.0 §20.22 standard exception hierarchy.
public class NullPointerException extends RuntimeException {
    public NullPointerException() { }
    public NullPointerException(String s) { super(s); }
}
