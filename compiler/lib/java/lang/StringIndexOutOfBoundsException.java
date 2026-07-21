package java.lang;

// java.lang.StringIndexOutOfBoundsException — JLS 1.0 §20.22 standard exception
// hierarchy; thrown by String/StringBuffer index methods (§20.12).
public class StringIndexOutOfBoundsException extends IndexOutOfBoundsException {
    public StringIndexOutOfBoundsException() { }
    public StringIndexOutOfBoundsException(String s) { super(s); }
    public StringIndexOutOfBoundsException(int index) {
        super("String index out of range: " + index);
    }
}
