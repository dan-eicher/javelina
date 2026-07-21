package java.lang;

// java.lang.NumberFormatException — JLS 1.0 §20.22 standard exception hierarchy.
public class NumberFormatException extends IllegalArgumentException {
    public NumberFormatException() { }
    public NumberFormatException(String s) { super(s); }
}
