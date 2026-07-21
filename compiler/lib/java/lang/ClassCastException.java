package java.lang;

// java.lang.ClassCastException — JLS 1.0 §20.22 standard exception hierarchy.
public class ClassCastException extends RuntimeException {
    public ClassCastException() { }
    public ClassCastException(String s) { super(s); }
}
