package java.lang;

// java.lang.NegativeArraySizeException — JLS 1.0 §20.22 standard exception hierarchy.
public class NegativeArraySizeException extends RuntimeException {
    public NegativeArraySizeException() { }
    public NegativeArraySizeException(String s) { super(s); }
}
