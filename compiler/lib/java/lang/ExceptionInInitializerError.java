package java.lang;

// java.lang.ExceptionInInitializerError — JLS 1.0 §20.23. NOTE: the 1st-edition spec contradicts itself —
// the §20.23 class declaration says `extends RuntimeException`, but the §20.22 exception hierarchy (and every
// later edition, and real Java) places it under LinkageError → Error. We follow §20.22: this MUST be an Error so
// that §12.4.2 step 7 (a superclass init failure is rethrown as the SAME exception, never re-wrapped) falls out
// of the $ensure_init `catch (Error) → rethrow` arm — $ensure_init then provably throws only Errors.
public class ExceptionInInitializerError extends LinkageError {
    private Throwable exception;
    public ExceptionInInitializerError() { }
    public ExceptionInInitializerError(String s) { super(s); }
    public ExceptionInInitializerError(Throwable thrown) { this.exception = thrown; }
    public Throwable getException() { return exception; }
}
