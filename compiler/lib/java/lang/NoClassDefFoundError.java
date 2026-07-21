package java.lang;

// java.lang.NoClassDefFoundError — JLS 1.0 §20.22 standard exception hierarchy.
public class NoClassDefFoundError extends LinkageError {
    public NoClassDefFoundError() { }
    public NoClassDefFoundError(String s) { super(s); }
}
