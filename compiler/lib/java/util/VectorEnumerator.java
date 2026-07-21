package java.util;

// Package-private Enumeration over a Vector's elements — returned by Vector.elements()
// (JLS 1.0 §21.1). Reads the Vector's protected elementData/elementCount (same package).
class VectorEnumerator implements Enumeration {
    Vector vector;
    int count;

    VectorEnumerator(Vector v) {
        super();
        this.vector = v;
        this.count = 0;
    }

    public boolean hasMoreElements() {
        return count < vector.elementCount;
    }

    public Object nextElement() {
        if (count < vector.elementCount) {
            Object o = vector.elementData[count];
            count = count + 1;
            return o;
        }
        throw new NoSuchElementException("VectorEnumerator");
    }
}
