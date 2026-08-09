package java.util;

// java.util.EmptyStackException (JLS 1.0 §21.13) — thrown by Stack.pop()/peek() when
// the stack is empty. JDK 1.0 declares only the no-arg constructor.
public class EmptyStackException extends RuntimeException {
    public EmptyStackException() { }
}
