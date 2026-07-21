package java.lang;

// java.lang.VerifyError — JLS 1.0 §20.22 standard exception hierarchy.
public class VerifyError extends LinkageError {
    public VerifyError() { }
    public VerifyError(String s) { super(s); }
}
