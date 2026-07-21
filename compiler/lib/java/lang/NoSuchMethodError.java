package java.lang;

// java.lang.NoSuchMethodError — JLS 1.0 §20.22 standard exception hierarchy.
public class NoSuchMethodError extends IncompatibleClassChangeError {
    public NoSuchMethodError() { }
    public NoSuchMethodError(String s) { super(s); }
}
