package java.lang;

// java.lang.Throwable — JLS 1.0 §20.22. A GC-struct overlay: the detail message is an ordinary
// reference field, so everything (ctors, getMessage, toString, printStackTrace) is a compiled body.
public class Throwable {
    private String detailMessage;
    public Throwable() { }
    public Throwable(String message) { this.detailMessage = message; }
    public String getMessage() { return detailMessage; }

    // §20.22.4: the class name, and ": " + the detail message when non-null.
    public String toString() {
        String s = getClass().getName();
        String m = getMessage();
        return (m != null) ? (s + ": " + m) : s;
    }

    // §20.22.5: fillInStackTrace records the current call stack into this throwable. The VM does not
    // yet expose stack frames to the guest, so this returns `this` and the printed trace is just the
    // throwable header (its toString) — a documented floor limitation, not a silent stub.
    public Throwable fillInStackTrace() { return this; }

    // §20.22.6: print this throwable (and its backtrace, when available) to System.err or the stream.
    public void printStackTrace() { printStackTrace(System.err); }
    public void printStackTrace(java.io.PrintStream s) { s.println(this.toString()); }
}
