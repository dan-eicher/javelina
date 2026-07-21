package java.lang;

// java.lang.Object — the root of the class hierarchy (JLS 1.0 §20.1).
// Signatures verbatim from the spec; bodies are runtime-provided, so the
// methods are declared `native`. wait/notify* are part of Object's contract
// even though this target has no threads (calling them traps at runtime).
public class Object {
    // §20.1.4 identity hash: a per-object value assigned once and STORED. It must be
    // stable across garbage collection, so it can't be derived from the object's address
    // (the collector moves objects) — it's a field, lazily assigned from a global counter
    // on first hashCode() (0 = unassigned; the counter starts at 1 so it never collides
    // with the sentinel). Consistent with equals (identity), distinct per object.
    private int hash;
    private static int hashCounter;

    public final native Class getClass();

    // §20.1.2 default: the fully-qualified class name, '@', and the hex identity hash.
    public String toString() {
        return getClass().getName() + "@" + Integer.toHexString(hashCode());
    }
    public boolean equals(Object obj) { return this == obj; }
    public int hashCode() {
        if (hash == 0) { hashCounter = hashCounter + 1; hash = hashCounter; }
        return hash;
    }
    // §20.1.5 shallow copy. Throws CloneNotSupportedException unless the runtime class
    // implements Cloneable (all arrays do). The actual field/element copy is `internalClone`,
    // a VIRTUAL helper the compiler synthesizes as a real override for every Cloneable class
    // and array overlay — so `super.clone()` (a special call landing here) still copies the
    // RUNTIME type, exactly as the standard `super.clone()` idiom requires.
    protected Object clone() throws CloneNotSupportedException {
        if (!(this instanceof Cloneable)) throw new CloneNotSupportedException();
        return internalClone();
    }
    // Placeholder — never reached (Object is not Cloneable, so clone() throws first). The
    // compiler emits a real per-class shallow-copy body for every Cloneable class/array.
    Object internalClone() { return this; }
    public final native void wait() throws IllegalMonitorStateException, InterruptedException;
    public final native void wait(long millis) throws IllegalMonitorStateException, InterruptedException;
    public final native void wait(long millis, int nanos) throws IllegalMonitorStateException, InterruptedException;
    public final native void notify() throws IllegalMonitorStateException;
    public final native void notifyAll() throws IllegalMonitorStateException;
    protected native void finalize() throws Throwable;
}
