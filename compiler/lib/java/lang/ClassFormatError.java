package java.lang;

// java.lang.ClassFormatError — JLS 1.0 §20.22 standard exception hierarchy.
public class ClassFormatError extends LinkageError {
    public ClassFormatError() { }
    public ClassFormatError(String s) { super(s); }
}
