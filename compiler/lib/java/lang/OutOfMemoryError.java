package java.lang;

// java.lang.OutOfMemoryError — JLS 1.0 §20.22 standard exception hierarchy.
public class OutOfMemoryError extends VirtualMachineError {
    public OutOfMemoryError() { }
    public OutOfMemoryError(String s) { super(s); }
}
