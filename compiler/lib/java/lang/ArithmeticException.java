package java.lang;

// java.lang.ArithmeticException — JLS 1.0 §20.22 standard exception hierarchy.
public class ArithmeticException extends RuntimeException {
    public ArithmeticException() { }
    public ArithmeticException(String s) { super(s); }
}
