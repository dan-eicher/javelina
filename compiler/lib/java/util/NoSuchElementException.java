package java.util;

// java.util.NoSuchElementException (JLS 1.0 §21) — thrown by an Enumeration's
// nextElement() (and other iterators) when no more elements remain.
public class NoSuchElementException extends RuntimeException {
    public NoSuchElementException() { }
    public NoSuchElementException(String s) { super(s); }
}
