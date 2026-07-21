package java.lang;

// java.lang.ClassNotFoundException — JLS 1.0 §20.22 standard exception hierarchy.
public class ClassNotFoundException extends Exception {
    public ClassNotFoundException() { }
    public ClassNotFoundException(String s) { super(s); }
}
