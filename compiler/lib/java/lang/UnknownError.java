package java.lang;

// java.lang.UnknownError — JLS 1.0 §20.22 standard exception hierarchy.
public class UnknownError extends VirtualMachineError {
    public UnknownError() { }
    public UnknownError(String s) { super(s); }
}
