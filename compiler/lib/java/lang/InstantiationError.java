package java.lang;

// java.lang.InstantiationError — JLS 1.0 §20.22 standard exception hierarchy.
public class InstantiationError extends IncompatibleClassChangeError {
    public InstantiationError() { }
    public InstantiationError(String s) { super(s); }
}
