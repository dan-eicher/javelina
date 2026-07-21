package java.lang;

// java.lang.IllegalArgumentException — JLS 1.0 §20.22 standard exception hierarchy.
public class IllegalArgumentException extends RuntimeException {
    public IllegalArgumentException() { }
    public IllegalArgumentException(String s) { super(s); }
}
