package java.util;

// java.util.Enumeration (JLS 1.0 §21.1) — iterates over a set of elements, one at a time.
public interface Enumeration {
    boolean hasMoreElements();
    Object nextElement();
}
