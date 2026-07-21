package java.lang;

// java.lang.LinkageError — JLS 1.0 §20.22 standard exception hierarchy.
public class LinkageError extends Error {
    public LinkageError() { }
    public LinkageError(String s) { super(s); }
}
