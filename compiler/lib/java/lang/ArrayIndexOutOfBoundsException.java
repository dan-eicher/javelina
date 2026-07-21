package java.lang;

// java.lang.ArrayIndexOutOfBoundsException — JLS 1.0 §20.22 standard exception
// hierarchy; thrown by array access when the index is out of bounds (§15.12).
public class ArrayIndexOutOfBoundsException extends IndexOutOfBoundsException {
    public ArrayIndexOutOfBoundsException() { }
    public ArrayIndexOutOfBoundsException(int index) { }
    public ArrayIndexOutOfBoundsException(String s) { super(s); }
}
