package java.lang;

// java.lang.StackOverflowError — JLS 1.0 §20.22 standard exception hierarchy.
public class StackOverflowError extends VirtualMachineError {
    public StackOverflowError() { }
    public StackOverflowError(String s) { super(s); }
}
