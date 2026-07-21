package java.util;

// Package-private Enumeration over a Hashtable's keys (keys=true) or values
// (keys=false) — returned by Hashtable.keys()/elements() (JLS 1.0 §21.3). Walks the
// bucket array high→low, following each chain.
class HashtableEnumerator implements Enumeration {
    boolean keys;
    int index;
    HashtableEntry[] table;
    HashtableEntry entry;

    HashtableEnumerator(HashtableEntry[] table, boolean keys) {
        super();
        this.table = table;
        this.keys = keys;
        this.index = table.length;
        this.entry = null;
    }

    public boolean hasMoreElements() {
        if (entry != null) return true;
        while (index > 0) {
            index = index - 1;
            entry = table[index];
            if (entry != null) return true;
        }
        return false;
    }

    public Object nextElement() {
        if (entry == null) {
            while (index > 0) {
                index = index - 1;
                entry = table[index];
                if (entry != null) break;
            }
        }
        if (entry != null) {
            HashtableEntry e = entry;
            entry = e.next;
            return keys ? e.key : e.value;
        }
        throw new NoSuchElementException("HashtableEnumerator");
    }
}
