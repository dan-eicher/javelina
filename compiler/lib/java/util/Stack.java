package java.util;

// java.util.Stack (JLS 1.0 §21.4) — a last-in-first-out stack of objects, extending Vector.
public class Stack extends Vector {
    public Object push(Object item) {
        addElement(item);
        return item;
    }

    public Object pop() {
        int len = size();
        Object obj = peek();
        removeElementAt(len - 1);
        return obj;
    }

    public Object peek() {
        int len = size();
        if (len == 0) throw new EmptyStackException();
        return elementAt(len - 1);
    }

    public boolean empty() {
        return size() == 0;
    }

    public int search(Object o) {
        int i = lastIndexOf(o);
        if (i >= 0) return size() - i;
        return -1;
    }
}
