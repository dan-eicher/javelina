package java.lang;

// java.lang.ClassCircularityError — JLS 1.0 §20.22 standard exception hierarchy.
public class ClassCircularityError extends LinkageError {
    public ClassCircularityError() { }
    public ClassCircularityError(String s) { super(s); }
}
