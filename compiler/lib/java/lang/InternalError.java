package java.lang;

// java.lang.InternalError — JLS 1.0 §20.22 standard exception hierarchy.
public class InternalError extends VirtualMachineError {
    public InternalError() { }
    public InternalError(String s) { super(s); }
}
