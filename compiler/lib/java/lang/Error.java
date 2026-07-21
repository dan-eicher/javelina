package java.lang;

// java.lang.Error — JLS 1.0 §20.22 standard exception hierarchy.
public class Error extends Throwable {
    public Error() { }
    public Error(String s) { super(s); }
}
