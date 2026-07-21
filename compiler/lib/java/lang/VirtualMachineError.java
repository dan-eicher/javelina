package java.lang;

// java.lang.VirtualMachineError — JLS 1.0 §20.22 standard exception hierarchy.
public class VirtualMachineError extends Error {
    public VirtualMachineError() { }
    public VirtualMachineError(String s) { super(s); }
}
