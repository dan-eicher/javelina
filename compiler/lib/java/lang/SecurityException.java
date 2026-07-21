package java.lang;

// java.lang.SecurityException — JLS 1.0 §20.22 standard exception hierarchy.
public class SecurityException extends RuntimeException {
    public SecurityException() { }
    public SecurityException(String s) { super(s); }
}
