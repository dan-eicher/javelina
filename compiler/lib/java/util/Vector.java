package java.util;

// java.util.Vector (JLS 1.0 §21.11) — a growable array of objects. Ported minus the
// `synchronized` modifiers (javelina targets Java 1.0 − synchronized).
public class Vector implements Cloneable {
    protected Object[] elementData;
    protected int elementCount;
    protected int capacityIncrement;

    public Vector(int initialCapacity, int capacityIncrement) {
        super();
        if (initialCapacity < 0) throw new IllegalArgumentException();
        this.elementData = new Object[initialCapacity];
        this.capacityIncrement = capacityIncrement;
    }
    public Vector(int initialCapacity) { this(initialCapacity, 0); }
    public Vector() { this(10); }

    public final void copyInto(Object[] anArray) {
        System.arraycopy(elementData, 0, anArray, 0, elementCount);
    }

    public final void trimToSize() {
        int oldCapacity = elementData.length;
        if (elementCount < oldCapacity) {
            Object[] oldData = elementData;
            elementData = new Object[elementCount];
            System.arraycopy(oldData, 0, elementData, 0, elementCount);
        }
    }

    public final void ensureCapacity(int minCapacity) {
        if (minCapacity > elementData.length) ensureCapacityHelper(minCapacity);
    }

    private void ensureCapacityHelper(int minCapacity) {
        int oldCapacity = elementData.length;
        Object[] oldData = elementData;
        int newCapacity = (capacityIncrement > 0) ? (oldCapacity + capacityIncrement)
                                                   : (oldCapacity * 2);
        if (newCapacity < minCapacity) newCapacity = minCapacity;
        elementData = new Object[newCapacity];
        System.arraycopy(oldData, 0, elementData, 0, elementCount);
    }

    public final void setSize(int newSize) {
        if (newSize > elementCount) {
            ensureCapacity(newSize);
        } else {
            for (int i = newSize; i < elementCount; i = i + 1) elementData[i] = null;
        }
        elementCount = newSize;
    }

    public final int capacity() { return elementData.length; }
    public final int size() { return elementCount; }
    public final boolean isEmpty() { return elementCount == 0; }
    public final Enumeration elements() { return new VectorEnumerator(this); }

    public final boolean contains(Object elem) { return indexOf(elem, 0) >= 0; }
    public final int indexOf(Object elem) { return indexOf(elem, 0); }

    public final int indexOf(Object elem, int index) {
        for (int i = index; i < elementCount; i = i + 1) {
            if (elem == null) { if (elementData[i] == null) return i; }
            else if (elem.equals(elementData[i])) return i;
        }
        return -1;
    }

    public final int lastIndexOf(Object elem) { return lastIndexOf(elem, elementCount - 1); }

    public final int lastIndexOf(Object elem, int index) {
        for (int i = index; i >= 0; i = i - 1) {
            if (elem == null) { if (elementData[i] == null) return i; }
            else if (elem.equals(elementData[i])) return i;
        }
        return -1;
    }

    public final Object elementAt(int index) {
        if (index >= elementCount || index < 0) throw new ArrayIndexOutOfBoundsException();
        return elementData[index];
    }

    public final Object firstElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[0];
    }

    public final Object lastElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[elementCount - 1];
    }

    public final void setElementAt(Object obj, int index) {
        if (index >= elementCount || index < 0) throw new ArrayIndexOutOfBoundsException();
        elementData[index] = obj;
    }

    public final void removeElementAt(int index) {
        if (index >= elementCount || index < 0) throw new ArrayIndexOutOfBoundsException();
        int j = elementCount - index - 1;
        if (j > 0) System.arraycopy(elementData, index + 1, elementData, index, j);
        elementCount = elementCount - 1;
        elementData[elementCount] = null;
    }

    public final void insertElementAt(Object obj, int index) {
        if (index > elementCount || index < 0) throw new ArrayIndexOutOfBoundsException();
        ensureCapacity(elementCount + 1);
        System.arraycopy(elementData, index, elementData, index + 1, elementCount - index);
        elementData[index] = obj;
        elementCount = elementCount + 1;
    }

    public final void addElement(Object obj) {
        ensureCapacity(elementCount + 1);
        elementData[elementCount] = obj;
        elementCount = elementCount + 1;
    }

    public final boolean removeElement(Object obj) {
        int i = indexOf(obj);
        if (i >= 0) { removeElementAt(i); return true; }
        return false;
    }

    public final void removeAllElements() {
        for (int i = 0; i < elementCount; i = i + 1) elementData[i] = null;
        elementCount = 0;
    }

    public Object clone() {
        try {
            Vector v = (Vector) super.clone();
            v.elementData = new Object[elementCount];
            System.arraycopy(elementData, 0, v.elementData, 0, elementCount);
            return v;
        } catch (CloneNotSupportedException e) {
            throw new InternalError();
        }
    }

    public final String toString() {
        int max = size() - 1;
        StringBuffer buf = new StringBuffer();
        buf.append("[");
        for (int i = 0; i <= max; i = i + 1) {
            buf.append(String.valueOf(elementData[i]));
            if (i < max) buf.append(", ");
        }
        buf.append("]");
        return buf.toString();
    }
}
