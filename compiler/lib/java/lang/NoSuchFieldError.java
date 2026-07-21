package java.lang;

// java.lang.NoSuchFieldError — JLS 1.0 §20.22 standard exception hierarchy.
public class NoSuchFieldError extends IncompatibleClassChangeError {
    public NoSuchFieldError() { }
    public NoSuchFieldError(String s) { super(s); }
}
