package java.util;

// Package-private hash-bucket chain node for Hashtable (JLS 1.0 §21.3). clone()
// deep-copies the rest of the chain (used by Hashtable.clone()).
class HashtableEntry {
    int hash;
    Object key;
    Object value;
    HashtableEntry next;

    protected Object clone() {
        HashtableEntry entry = new HashtableEntry();
        entry.hash = hash;
        entry.key = key;
        entry.value = value;
        entry.next = (next != null) ? (HashtableEntry) next.clone() : null;
        return entry;
    }
}
