package java.lang;

// java.lang.IllegalAccessError — JLS 1.0 §20.22 standard exception hierarchy.
public class IllegalAccessError extends IncompatibleClassChangeError {
    public IllegalAccessError() { }
    public IllegalAccessError(String s) { super(s); }
}
