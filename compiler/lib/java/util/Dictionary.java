package java.util;

// java.util.Dictionary (JLS 1.0 §21.2) — the abstract superclass of Hashtable: a
// mapping from keys to values.
public abstract class Dictionary {
    public Dictionary() { }
    public abstract int size();
    public abstract boolean isEmpty();
    public abstract Enumeration keys();
    public abstract Enumeration elements();
    public abstract Object get(Object key);
    public abstract Object put(Object key, Object value);
    public abstract Object remove(Object key);
}
