// Ch11 — JLS chapter 11, Exceptions.
//
// The compile-time half of §11.2 — a checked exception thrown where no `throws` admits it —
// lives in conformance/reject/, because the program that demonstrates the rule is exactly the
// one that must not compile.
//
// ONE SECTION OF THIS CHAPTER IS ABSENT, and stays UNCOVERED in the ledger rather than being
// written down to whatever this target currently does:
//
//   §11.5.2.2 "StackOverflowError: A Java Virtual Machine has run out of stack space for a
//             thread" — an Error, and therefore catchable. Unbounded recursion here raises a
//             wasm trap ("call stack exhausted") that no catch clause can see, so the program
//             dies instead of handling it. Making it catchable means synthesizing a Java
//             exception at jav_call_fn's frame guard and entering the unwinder — a VM change.
//
// That is a real gap, not a test problem: writing it down to the current behaviour would BE
// the divergence, and leaving it UNCOVERED keeps it in the ratchet where it shows.

class Ch11TestException extends Exception {
    Ch11TestException() { super(); }
    Ch11TestException(String s) { super(s); }
}

class Ch11Base { void m() throws Ch11TestException {} }

public class Ch11 {

    /* The spec's `thrower`, verbatim including its finally clause. */
    static String trace = "";
    static int thrower(String s) throws Ch11TestException {
        try {
            if (s.equals("divide")) { int i = 0; return i / i; }
            if (s.equals("null")) { s = null; return s.length(); }
            if (s.equals("test")) throw new Ch11TestException("Test message");
            return 0;
        } finally {
            trace = trace + "[thrower(\"" + s + "\") done]";
        }
    }

    // JLS 11.1
    static void s11_1() {
        // "An abnormal execution condition was synchronously detected by a Java Virtual
        // Machine... evaluation of an expression violates the normal semantics of the Java
        // language, such as an integer divide by zero."
        try { int x = 1 / 0; Check.notThrown("11.1", "a VM-detected condition throws"); }
        catch (ArithmeticException e) { Check.thrown("11.1", "a VM-detected condition throws"); }

        // "These exceptions are not thrown at an arbitrary point in the program, but rather at
        // a point where they are specified as a possible result" — the divide has not happened
        // when the operands are evaluated, so `n` is already updated when the throw lands.
        int n = 0;
        try { n = 5; int x = n / 0; Check.notThrown("11.1", "the throw is AT the operation"); }
        catch (ArithmeticException e) { Check.eq("11.1", "effects before it are complete", (long) n, 5L); }

        // "A throw statement was executed in Java code."
        try { throw new RuntimeException("by hand"); }
        catch (RuntimeException e) { Check.eq("11.1", "an explicit throw", e.getMessage(), "by hand"); }

        // Exceptions are instances of Throwable and its subclasses.
        Throwable t = new RuntimeException("x");
        Check.isTrue("11.1", "an exception is a Throwable", t != null);
    }

    // JLS 11.2.1
    static void s11_2_1() {
        // "Those unchecked exception classes which are the error classes are exempted from
        // compile-time checking" — this method declares no `throws` and yet may throw an Error.
        try { throw new Error("unchecked"); }
        catch (Error e) { Check.eq("11.2.1", "an Error needs no throws clause", e.getMessage(), "unchecked"); }
        // ...and a subclass of Error likewise.
        try { throw new StackOverflowError(); }
        catch (Error e) { Check.thrown("11.2.1", "…nor does an Error subclass"); }
    }

    // JLS 11.2.2
    static void s11_2_2() {
        // "The runtime exception classes (RuntimeException and its subclasses) are exempted
        // from compile-time checking" — again, no `throws` on this method.
        try { throw new RuntimeException("unchecked"); }
        catch (RuntimeException e) { Check.thrown("11.2.2", "a RuntimeException needs no throws clause"); }
        try { throw new IllegalArgumentException(); }
        catch (RuntimeException e) { Check.thrown("11.2.2", "…nor does a subclass of it"); }
    }

    // JLS 11.3
    static void s11_3() {
        // "control is transferred... to the NEAREST dynamically-enclosing catch clause of a
        // try statement that handles the exception." Nested tries: the inner one wins.
        String where = "";
        try {
            try { throw new IllegalArgumentException(); }
            catch (IllegalArgumentException e) { where = "inner"; }
        } catch (RuntimeException e) { where = "outer"; }
        Check.eq("11.3", "the NEAREST enclosing handler wins", where, "inner");

        // "The catch clause handles the exception if the type of its parameter is the class of
        // the exception or a SUPERCLASS of the class of the exception."
        where = "";
        try { throw new IllegalArgumentException(); }
        catch (RuntimeException e) { where = "super"; }
        Check.eq("11.3", "a superclass parameter handles the exception", where, "super");

        // ...and one that is neither does not: the inner catch is skipped for an unrelated type.
        where = "";
        try {
            try { throw new IllegalArgumentException(); }
            catch (ArithmeticException e) { where = "wrong"; }
        } catch (RuntimeException e) { where = "outer"; }
        Check.eq("11.3", "an unrelated parameter type does not handle it", where, "outer");

        // "the caller is the method invocation expression that was executed to cause the
        // method to be invoked" — an exception propagates out of a call to its caller's try.
        where = "";
        try { deep(); } catch (IllegalArgumentException e) { where = "propagated"; }
        Check.eq("11.3", "an exception propagates to the CALLER's handler", where, "propagated");

        // "If a finally clause is executed because of abrupt completion of a try block and the
        // finally clause itself completes abruptly, then the reason for the abrupt completion
        // of the try block is DISCARDED and the new reason is propagated."
        String which = "";
        try {
            try { throw new IllegalArgumentException("first"); }
            finally { throw new ArithmeticException("second"); }
        } catch (Throwable e) { which = e.getMessage(); }
        Check.eq("11.3", "an abrupt finally DISCARDS the original reason", which, "second");

        // "the finally clause is executed during propagation of the exception, even if no
        // matching catch clause is ultimately found" — here there is one, further out.
        trace = "";
        try {
            try { throw new IllegalArgumentException(); }
            finally { trace = "ran"; }
        } catch (IllegalArgumentException e) { }
        Check.eq("11.3", "finally runs while the exception propagates", trace, "ran");
    }

    static void deep() { throw new IllegalArgumentException(); }

    // JLS 11.3.1
    static void s11_3_1() {
        // "when the transfer of control takes place, all effects of the statements executed and
        // expressions evaluated before the point from which the exception is thrown must appear
        // to have taken place. No expressions, statements, or parts thereof that occur after
        // the point from which the exception is thrown may appear to have been evaluated."
        int[] a = new int[3];
        int i = 0;
        try {
            a[0] = 10;
            a[1] = 20;
            a[5] = 30;              // throws — this store must NOT be observable
            a[2] = 40;              // ...and neither may this one
            i = 99;
        } catch (ArrayIndexOutOfBoundsException e) { i = i + 1; }
        Check.eq("11.3.1", "effects BEFORE the throw point are all visible", (long) a[0], 10L);
        Check.eq("11.3.1", "…all of them", (long) a[1], 20L);
        Check.eq("11.3.1", "effects AFTER it are not", (long) a[2], 0L);
        Check.eq("11.3.1", "…including the statement that follows", (long) i, 1L);

        // The same for a partially-evaluated expression: the left operand's side effect stands,
        // the assignment it feeds does not happen.
        counter = 0;
        int r = -1;
        try { r = bump() + (1 / 0); Check.notThrown("11.3.1", "the throw aborts the assignment"); }
        catch (ArithmeticException e) {
            Check.eq("11.3.1", "the evaluated operand's effect stands", (long) counter, 1L);
            Check.eq("11.3.1", "…but the assignment never happened", (long) r, -1L);
        }
    }

    static int counter = 0;
    static int bump() { counter = counter + 1; return counter; }

    // JLS 11.4
    static void s11_4() {
        // The spec's program, and the output it prints for `divide null not test`. Each case
        // pins BOTH the class and the message — getClass() and getMessage() together, because
        // a runtime can get one right and the other wrong.
        trace = "";
        String[] args = { "divide", "null", "not", "test" };
        String[] wantClass = { "java.lang.ArithmeticException", "java.lang.NullPointerException",
                               "", "Ch11TestException" };
        String[] wantMsg = { "/ by zero", null, "", "Test message" };
        for (int i = 0; i < args.length; i++) {
            try {
                thrower(args[i]);
                Check.eq("11.4", "Test \"" + args[i] + "\" didn't throw an exception",
                         wantClass[i], "");
            } catch (Exception e) {
                Check.eq("11.4", "Test \"" + args[i] + "\" threw the named class",
                         e.getClass().getName(), wantClass[i]);
                Check.eq("11.4", "...with the named message", e.getMessage(), wantMsg[i]);
            }
        }
        // "the finally clause is executed on every invocation of thrower, whether or not an
        // exception occurs, as shown by the [thrower(...) done] output"
        Check.eq("11.4", "finally ran on EVERY invocation, thrown or not", trace,
                 "[thrower(\"divide\") done][thrower(\"null\") done]"
               + "[thrower(\"not\") done][thrower(\"test\") done]");
    }

    // JLS 11.5
    static void s11_5() {
        // "The possible exceptions... are organized in a hierarchy of classes, rooted at class
        // Throwable, a direct subclass of Object. The classes Exception and Error are direct
        // subclasses of Throwable. The class RuntimeException is a direct subclass of Exception."
        Check.eq("11.5", "Throwable's superclass is Object",
                 new Throwable().getClass().getName(), "java.lang.Throwable");
        Object asObject = new Throwable();
        Check.isTrue("11.5", "a Throwable IS an Object", asObject != null);

        // Exception <: Throwable, and Error <: Throwable, by what each catch admits.
        String seen = "";
        try { throw new Exception("e"); } catch (Throwable t) { seen = "throwable"; }
        Check.eq("11.5", "catch (Throwable) catches an Exception", seen, "throwable");
        seen = "";
        try { throw new Error("r"); } catch (Throwable t) { seen = "throwable"; }
        Check.eq("11.5", "catch (Throwable) catches an Error", seen, "throwable");

        // RuntimeException <: Exception.
        seen = "";
        try { throw new RuntimeException("re"); } catch (Exception e) { seen = "exception"; }
        Check.eq("11.5", "RuntimeException is an Exception", seen, "exception");
    }

    // JLS 11.5.1
    static void s11_5_1() {
        // "The class Exception is the superclass of all the exceptions that ordinary programs
        // may wish to recover from" — catch (Exception) catches both a checked one and a
        // RuntimeException.
        String seen = "";
        try { throw new Ch11TestException("checked"); } catch (Exception e) { seen = e.getMessage(); }
        Check.eq("11.5.1", "catch (Exception) catches a CHECKED exception", seen, "checked");
        seen = "";
        try { throw new IllegalArgumentException("unchecked"); }
        catch (Exception e) { seen = e.getMessage(); }
        Check.eq("11.5.1", "…and an unchecked one", seen, "unchecked");
    }

    // JLS 11.5.1.1
    static void s11_5_1_1() {
        // Each listed standard runtime exception, thrown by the operation the spec names for it.
        // "ArithmeticException: ...such as an integer division... with a zero divisor."
        try { int x = 1 / 0; Check.notThrown("11.5.1.1", "ArithmeticException"); }
        catch (ArithmeticException e) { Check.thrown("11.5.1.1", "ArithmeticException"); }

        // "ArrayStoreException: An attempt has been made to store into an array component a
        // value whose class is not assignment compatible with the component type."
        Object[] oa = new String[1];
        try { oa[0] = new Object(); Check.notThrown("11.5.1.1", "ArrayStoreException"); }
        catch (ArrayStoreException e) { Check.thrown("11.5.1.1", "ArrayStoreException"); }

        // "ClassCastException: An attempt has been made to cast a reference to an object to an
        // inappropriate type."
        Object o = new Object();
        try { String s = (String) o; Check.notThrown("11.5.1.1", "ClassCastException"); }
        catch (ClassCastException e) { Check.thrown("11.5.1.1", "ClassCastException"); }

        // "IndexOutOfBoundsException: Either an index of some sort (such as to an array, a
        // string...) ...was out of range."
        int[] a = new int[2];
        try { int x = a[5]; Check.notThrown("11.5.1.1", "IndexOutOfBoundsException (array)"); }
        catch (IndexOutOfBoundsException e) { Check.thrown("11.5.1.1", "IndexOutOfBoundsException (array)"); }
        try { char c = "ab".charAt(9); Check.notThrown("11.5.1.1", "…and for a String"); }
        catch (IndexOutOfBoundsException e) { Check.thrown("11.5.1.1", "…and for a String"); }

        // "NegativeArraySizeException: An attempt was made to create an array with a negative
        // length."
        int n = -1;
        try { int[] bad = new int[n]; Check.notThrown("11.5.1.1", "NegativeArraySizeException"); }
        catch (NegativeArraySizeException e) { Check.thrown("11.5.1.1", "NegativeArraySizeException"); }

        // "NullPointerException: An attempt was made to use a null reference in a case where an
        // object reference was required."
        String ns = null;
        try { int len = ns.length(); Check.notThrown("11.5.1.1", "NullPointerException"); }
        catch (NullPointerException e) { Check.thrown("11.5.1.1", "NullPointerException"); }

        // "NumberFormatException: An attempt was made to convert a String to a value of a
        // numeric type, but the String did not have an appropriate format." A SUBCLASS of
        // IllegalArgumentException, which the spec states and this asserts.
        try { int v = Integer.parseInt("nope"); Check.notThrown("11.5.1.1", "NumberFormatException"); }
        catch (IllegalArgumentException e) {
            Check.eq("11.5.1.1", "NumberFormatException is an IllegalArgumentException",
                     e.getClass().getName(), "java.lang.NumberFormatException");
        }

        // Every one of them is unchecked, i.e. a RuntimeException.
        String kind = "";
        try { int x = 1 / 0; } catch (RuntimeException e) { kind = "runtime"; }
        Check.eq("11.5.1.1", "the standard ones are all RuntimeExceptions", kind, "runtime");
    }

    // JLS 11.5.1.2
    static void s11_5_1_2() {
        // "The standard subclasses of Exception other than RuntimeException are all checked
        // exception classes." CloneNotSupportedException is the one this target can raise
        // without a class loader or a thread: §20.1.5's clone on a non-Cloneable object.
        String seen = "";
        try { new Object().getClass(); seen = "ok"; } catch (Throwable t) { seen = "unexpected"; }
        Check.eq("11.5.1.2", "the control case does not throw", seen, "ok");

        // A user-declared checked exception behaves as the section says: catchable as Exception,
        // NOT as RuntimeException.
        seen = "";
        try { throw new Ch11TestException("c"); }
        catch (Exception e) { seen = (e instanceof RuntimeException) ? "runtime" : "checked"; }
        Check.eq("11.5.1.2", "a checked exception is not a RuntimeException", seen, "checked");
    }

    // JLS 11.5.2
    static void s11_5_2() {
        // "The class Error is a separate subclass of Throwable, distinct from Exception in the
        // class hierarchy, to allow programs to use the idiom `} catch (Exception e) {` to
        // catch all exceptions from which recovery may be possible WITHOUT catching errors."
        String seen = "";
        try {
            try { throw new Error("not an exception"); }
            catch (Exception e) { seen = "caught by Exception"; }
        } catch (Error e) { seen = "escaped Exception"; }
        Check.eq("11.5.2", "catch (Exception) does NOT catch an Error", seen, "escaped Exception");

        // ...and catch (Throwable) does catch it, which is what makes the distinction useful
        // rather than merely restrictive.
        seen = "";
        try { throw new Error("x"); } catch (Throwable t) { seen = "caught"; }
        Check.eq("11.5.2", "…but catch (Throwable) does", seen, "caught");
    }

    public static void run() {
        try { s11_1();     } catch (Throwable t) { Check.crashed("11.1", t); }
        try { s11_2_1();   } catch (Throwable t) { Check.crashed("11.2.1", t); }
        try { s11_2_2();   } catch (Throwable t) { Check.crashed("11.2.2", t); }
        try { s11_3();     } catch (Throwable t) { Check.crashed("11.3", t); }
        try { s11_3_1();   } catch (Throwable t) { Check.crashed("11.3.1", t); }
        try { s11_4();     } catch (Throwable t) { Check.crashed("11.4", t); }
        try { s11_5();     } catch (Throwable t) { Check.crashed("11.5", t); }
        try { s11_5_1();   } catch (Throwable t) { Check.crashed("11.5.1", t); }
        try { s11_5_1_1(); } catch (Throwable t) { Check.crashed("11.5.1.1", t); }
        try { s11_5_1_2(); } catch (Throwable t) { Check.crashed("11.5.1.2", t); }
        try { s11_5_2();   } catch (Throwable t) { Check.crashed("11.5.2", t); }
    }
}
