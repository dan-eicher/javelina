package java.lang;

// java.lang.IncompatibleClassChangeError — JLS 1.0 §20.22 standard exception hierarchy.
public class IncompatibleClassChangeError extends LinkageError {
    public IncompatibleClassChangeError() { }
    public IncompatibleClassChangeError(String s) { super(s); }
}
