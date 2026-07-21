package java.lang;

// java.lang.CloneNotSupportedException — JLS 1.0 §20.22 standard exception hierarchy.
public class CloneNotSupportedException extends Exception {
    public CloneNotSupportedException() { }
    public CloneNotSupportedException(String s) { super(s); }
}
