package java.lang;

// java.lang.IndexOutOfBoundsException — JLS 1.0 §20.22 standard exception hierarchy.
public class IndexOutOfBoundsException extends RuntimeException {
    public IndexOutOfBoundsException() { }
    public IndexOutOfBoundsException(String s) { super(s); }
}
