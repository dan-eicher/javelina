package java.lang;

// java.lang.AbstractMethodError — JLS 1.0 §20.22 standard exception hierarchy.
public class AbstractMethodError extends IncompatibleClassChangeError {
    public AbstractMethodError() { }
    public AbstractMethodError(String s) { super(s); }
}
