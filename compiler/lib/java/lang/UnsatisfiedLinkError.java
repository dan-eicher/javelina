package java.lang;

// java.lang.UnsatisfiedLinkError — JLS 1.0 §20.22 standard exception hierarchy.
public class UnsatisfiedLinkError extends LinkageError {
    public UnsatisfiedLinkError() { }
    public UnsatisfiedLinkError(String s) { super(s); }
}
