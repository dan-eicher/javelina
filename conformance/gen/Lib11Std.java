// Lib11Std — JLS §11.5.1.1, §11.5.1.2 and §11.5.2: the standard exception classes.
//
// These three are LISTS. The spec names each class together with the operation that raises it,
// which makes them the one shape where a sampled test passes while most of the section goes
// untested -- so each entry gets its own template, raised BY THE OPERATION THE SPEC NAMES
// rather than by `throw new X()`, which would prove only that the class exists.
//
// §11.5.1.1 (p.209), package java.lang:
//   ArithmeticException           "an integer division (§15.16.2) operation with a zero divisor"
//   ArrayStoreException           "store into an array component a value whose class is not
//                                  assignment compatible with the component type"
//   ClassCastException            "cast (§5.4, §15.15) a reference to an object to an
//                                  inappropriate type"
//   IllegalArgumentException      "a method was passed an invalid or inappropriate argument",
//                                  with NumberFormatException among its subclasses
//   NumberFormatException         "convert a String to a value of a numeric type, but the
//                                  String did not have an appropriate format"
//   IndexOutOfBoundsException     "an index of some sort (such as to an array, a string, or a
//                                  vector) ... was out of range" -- three, so three cases
//   NegativeArraySizeException    "create an array with a negative length (§15.9)"
//   NullPointerException          "use a null reference in a case where an object reference
//                                  was required"
// ...and java.util:
//   EmptyStackException           "access an element of an empty stack"
//   NoSuchElementException        "access an element of an empty vector"
//
// NOT TESTABLE HERE, and listed rather than quietly skipped:
//   IllegalThreadStateException, IllegalMonitorStateException -- both are about threads, and
//     chapter 17 is N/A for this target (no threads, no synchronized).
//   SecurityException -- "a security violation was detected (§20.17)"; raising one needs a
//     SecurityManager to be installed, and §20.17 is not part of this runtime.
//
// §11.5.1.2 (p.210): the checked classes. CloneNotSupportedException is raised by the
// operation the spec names. ClassNotFoundException, IllegalAccessException and
// InstantiationException need Class.forName / Class.newInstance on a class chosen at run time;
// InterruptedException is a thread, UTFDataFormatException needs a malformed modified-UTF-8
// stream, and the five java.net classes have no implementation on this target.
//
// §11.5.2 (p.211): "The class Error is a separate subclass of Throwable, distinct from
// Exception in the class hierarchy, to allow programs to use the idiom `} catch (Exception e)
// {` to catch all exceptions from which recovery may be possible without catching errors from
// which recovery is typically not possible." That IDIOM is the claim, so the template runs it.
public class Lib11Std {

    private Lib11Std() {}

    public static void install(Registry r) {
        // §11.5.1.1, one per class, raised by the operation the spec names for it.
        // The expected name is FULLY QUALIFIED: §20.3.2's getName is, and these classes live in
        // java.lang however they are spelled in source.
        u(r, "arithmetic", "int zero = 0; int q = 7 / zero;", "java.lang.ArithmeticException");
        u(r, "arraystore",
          "Object[] objs = new String[2]; objs[0] = new StringBuffer(\"x\");",
          "java.lang.ArrayStoreException");
        u(r, "classcast",
          "Object o = \"a string\"; StringBuffer sb = (StringBuffer) o;",
          "java.lang.ClassCastException");
        u(r, "numberformat", "int n = Integer.parseInt(\"not a number\");",
          "java.lang.NumberFormatException");
        u(r, "negativearraysize", "int n = -1; int[] a = new int[n];",
          "java.lang.NegativeArraySizeException");
        u(r, "nullpointer", "String s = null; int n = s.length();",
          "java.lang.NullPointerException");
        u(r, "emptystack", "java.util.Stack st = new java.util.Stack(); Object o = st.pop();",
          "java.util.EmptyStackException");
        u(r, "nosuchelement",
          "java.util.Vector v = new java.util.Vector();"
        + " Object o = v.elements().nextElement();",
          "java.util.NoSuchElementException");

        r.register(new Sn11IndexKinds());
        r.register(new Sn11IllegalArgumentParent());
        r.register(new Sn11CloneNotSupported());
        r.register(new Sn11ErrorIdiom());
    }

    /** One unchecked-exception template: run `stmt`, expect `fqcn` to come out of it. */
    private static void u(Registry r, String name, String stmt, String fqcn) {
        r.register(new Sn11Std(name, stmt, fqcn));
    }
}

/** One entry of §11.5.1.1, raised by the operation the section names for it. The expectation is
 *  the exception's fully qualified name, which is what §20.3.2 getName returns and what Emit's
 *  THROWS handling prints -- so a case that threw the WRONG standard exception fails as loudly
 *  as one that threw nothing. */
class Sn11Std implements Snippet {

    private String name;
    private String stmt;
    private String fqcn;

    Sn11Std(String name, String stmt, String fqcn) {
        this.name = name; this.stmt = stmt; this.fqcn = fqcn;
    }

    public String   id()        { return "t11.std." + name; }
    public String[] sections()  { return Strs.of("11.5.1.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ String got = \"none\";"
             + " try { " + stmt + " }"
             + " catch (Throwable t) { got = t.getClass().getName(); }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofString(fqcn); }
}

/** §11.5.1.1 names THREE things an IndexOutOfBoundsException indexes: "an index of some sort
 *  (such as to an array, a string, or a vector) or a subrange ... was out of range". One case
 *  per kind, because an implementation can easily get the array right and the string wrong. */
class Sn11IndexKinds implements Snippet {
    public String   id()        { return "t11.std.indexoutofbounds.kinds"; }
    public String[] sections()  { return Strs.of("11.5.1.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " int[] a = new int[2];"
             + " try { int x = a[5]; } catch (IndexOutOfBoundsException e) { got += 1; }"
             + " try { char c = \"ab\".charAt(9); }"
             + " catch (IndexOutOfBoundsException e) { got += 2; }"
             + " java.util.Vector v = new java.util.Vector();"
             + " try { Object o = v.elementAt(3); }"
             + " catch (IndexOutOfBoundsException e) { got += 4; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7); }
}

/** §11.5.1.1 lists NumberFormatException as a SUBCLASS of IllegalArgumentException. That
 *  relation is a claim in its own right: catching the parent must catch the child. */
class Sn11IllegalArgumentParent implements Snippet {
    public String   id()        { return "t11.std.illegalargument.parent"; }
    public String[] sections()  { return Strs.of("11.5.1.1"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { int n = Integer.parseInt(\"zzz\"); }"
             + " catch (IllegalArgumentException e) { got += 1; }"      // the parent catches it
             + " NumberFormatException nfe = new NumberFormatException(\"x\");"
             + " if (nfe instanceof IllegalArgumentException) got += 2;"
             + " if (nfe instanceof RuntimeException) got += 4;"        // ...and both are unchecked
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7); }
}

/** §11.5.1.2: "CloneNotSupportedException: The clone method (§20.1.5) of class Object has been
 *  invoked to clone an object, but the class of that object does not implement the Cloneable
 *  interface."
 *
 *  Both sides, because the exception is defined by the ABSENCE of Cloneable and a test of the
 *  failing side alone would pass on an implementation where clone never works at all. */
class Sn11CloneNotSupported implements Snippet, Declaring {
    public String   id()        { return "t11.std.clonenotsupported"; }
    public String[] sections()  { return Strs.of("11.5.1.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T11NotCloneable {\n"
                     + "    int v = 5;\n"
                     + "    Object copy() throws CloneNotSupportedException { return clone(); }\n"
                     + "}",

                       "class T11Cloneable implements Cloneable {\n"
                     + "    int v = 6;\n"
                     + "    Object copy() throws CloneNotSupportedException { return clone(); }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { new T11NotCloneable().copy(); }"
             + " catch (CloneNotSupportedException e) { got += 1; }"
             + " try { Object c = new T11Cloneable().copy();"
             + "       if (c != null) got += 2; }"
             + " catch (CloneNotSupportedException e) { got += 8; }"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(3); }
}

/** §11.5.2: "The class Error is a separate subclass of Throwable, distinct from Exception in
 *  the class hierarchy, to allow programs to use the idiom `} catch (Exception e) {` to catch
 *  all exceptions from which recovery may be possible without catching errors from which
 *  recovery is typically not possible."
 *
 *  The template runs that idiom literally: one catch (Exception) that takes the checked and the
 *  unchecked exception and lets the Error straight through to an outer handler. If Error were
 *  under Exception the idiom would be useless, and the printed number says so. */
class Sn11ErrorIdiom implements Snippet, Declaring {
    public String   id()        { return "t11.error.idiom.separate"; }
    public String[] sections()  { return Strs.of("11.5.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib11.decls(); }

    public String render(String[] h) {
        return "{ int got = 0;"
             + " try { T11Thrower.raiseChecked(); } catch (Exception e) { got += 1; }"
             + " try { T11Thrower.raiseUnchecked(); } catch (Exception e) { got += 2; }"
             + " try {"
             + "   try { T11Thrower.raiseError(); }"
             + "   catch (Exception e) { got += 100; }"          // must NOT fire
             + " } catch (Throwable t) { got += 4; }"            // the Error lands here
             + " if (new Error() instanceof Exception) got += 200;"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7); }
}
