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

    // JLS §20.22 fillInStackTrace is not yet implemented: the VM does not expose stack frames to
    // the guest, so this returns `this` and the printed trace is just the throwable header (its
    // toString). Cited on ONE line, and as `JLS §20.22` (the leaf the inventory carries), because
    // check-deferrals.sh scans line by line: the previous wording split "does not" from "yet
    // expose" across a line break, so the gate that exists to surface this could not see it.
    public Throwable fillInStackTrace() { return this; }

    // §20.22.6: print this throwable (and its backtrace, when available) to System.err or the stream.
    public void printStackTrace() { printStackTrace(System.err); }
    public void printStackTrace(java.io.PrintStream s) { s.println(this.toString()); }
}
