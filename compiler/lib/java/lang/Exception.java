package java.lang;

// java.lang.Exception — JLS 1.0 §20.22 standard exception hierarchy.
public class Exception extends Throwable {
    public Exception() { }
    public Exception(String s) { super(s); }
}
