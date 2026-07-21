package java.util;

// java.util.Hashtable (JLS 1.0 §21.3) — a hash table mapping keys to values via
// separate chaining. Ported minus `synchronized` (javelina targets Java 1.0 − synchronized).
public class Hashtable extends Dictionary implements Cloneable {
    private HashtableEntry[] table;
    private int count;
    private int threshold;
    private float loadFactor;

    public Hashtable(int initialCapacity, float loadFactor) {
        super();
        if (initialCapacity <= 0 || loadFactor <= 0) throw new IllegalArgumentException();
        this.loadFactor = loadFactor;
        table = new HashtableEntry[initialCapacity];
        threshold = (int) (initialCapacity * loadFactor);
    }
    public Hashtable(int initialCapacity) { this(initialCapacity, 0.75f); }
    public Hashtable() { this(101, 0.75f); }

    public int size() { return count; }
    public boolean isEmpty() { return count == 0; }

    public Enumeration keys() { return new HashtableEnumerator(table, true); }
    public Enumeration elements() { return new HashtableEnumerator(table, false); }

    public boolean contains(Object value) {
        if (value == null) throw new NullPointerException();
        HashtableEntry[] tab = table;
        for (int i = tab.length - 1; i >= 0; i = i - 1) {
            for (HashtableEntry e = tab[i]; e != null; e = e.next) {
                if (e.value.equals(value)) return true;
            }
        }
        return false;
    }

    public boolean containsKey(Object key) {
        HashtableEntry[] tab = table;
        int hash = key.hashCode();
        int index = (hash & 0x7FFFFFFF) % tab.length;
        for (HashtableEntry e = tab[index]; e != null; e = e.next) {
            if (e.hash == hash && e.key.equals(key)) return true;
        }
        return false;
    }

    public Object get(Object key) {
        HashtableEntry[] tab = table;
        int hash = key.hashCode();
        int index = (hash & 0x7FFFFFFF) % tab.length;
        for (HashtableEntry e = tab[index]; e != null; e = e.next) {
            if (e.hash == hash && e.key.equals(key)) return e.value;
        }
        return null;
    }

    protected void rehash() {
        int oldCapacity = table.length;
        HashtableEntry[] oldTable = table;
        int newCapacity = oldCapacity * 2 + 1;
        HashtableEntry[] newTable = new HashtableEntry[newCapacity];
        threshold = (int) (newCapacity * loadFactor);
        table = newTable;
        for (int i = oldCapacity - 1; i >= 0; i = i - 1) {
            HashtableEntry old = oldTable[i];
            while (old != null) {
                HashtableEntry e = old;
                old = old.next;
                int index = (e.hash & 0x7FFFFFFF) % newCapacity;
                e.next = newTable[index];
                newTable[index] = e;
            }
        }
    }

    public Object put(Object key, Object value) {
        if (value == null) throw new NullPointerException();
        HashtableEntry[] tab = table;
        int hash = key.hashCode();
        int index = (hash & 0x7FFFFFFF) % tab.length;
        for (HashtableEntry e = tab[index]; e != null; e = e.next) {
            if (e.hash == hash && e.key.equals(key)) {
                Object old = e.value;
                e.value = value;
                return old;
            }
        }
        if (count >= threshold) {
            rehash();
            tab = table;
            index = (hash & 0x7FFFFFFF) % tab.length;
        }
        HashtableEntry e = new HashtableEntry();
        e.hash = hash;
        e.key = key;
        e.value = value;
        e.next = tab[index];
        tab[index] = e;
        count = count + 1;
        return null;
    }

    public Object remove(Object key) {
        HashtableEntry[] tab = table;
        int hash = key.hashCode();
        int index = (hash & 0x7FFFFFFF) % tab.length;
        HashtableEntry prev = null;
        for (HashtableEntry e = tab[index]; e != null; e = e.next) {
            if (e.hash == hash && e.key.equals(key)) {
                if (prev != null) prev.next = e.next;
                else tab[index] = e.next;
                count = count - 1;
                return e.value;
            }
            prev = e;
        }
        return null;
    }

    public void clear() {
        HashtableEntry[] tab = table;
        for (int index = tab.length - 1; index >= 0; index = index - 1) tab[index] = null;
        count = 0;
    }

    public Object clone() {
        try {
            Hashtable t = (Hashtable) super.clone();
            t.table = new HashtableEntry[table.length];
            for (int i = table.length - 1; i >= 0; i = i - 1) {
                t.table[i] = (table[i] != null) ? (HashtableEntry) table[i].clone() : null;
            }
            return t;
        } catch (CloneNotSupportedException e) {
            throw new InternalError();
        }
    }

    public String toString() {
        int max = size() - 1;
        StringBuffer buf = new StringBuffer();
        Enumeration k = keys();
        Enumeration e = elements();
        buf.append("{");
        for (int i = 0; i <= max; i = i + 1) {
            buf.append(String.valueOf(k.nextElement()));
            buf.append("=");
            buf.append(String.valueOf(e.nextElement()));
            if (i < max) buf.append(", ");
        }
        buf.append("}");
        return buf.toString();
    }
}
