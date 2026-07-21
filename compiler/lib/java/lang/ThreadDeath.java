package java.lang;

// java.lang.ThreadDeath — JLS 1.0 §20.22 standard exception hierarchy.
public class ThreadDeath extends Error {
    public ThreadDeath() { }
    public ThreadDeath(String s) { super(s); }
}
