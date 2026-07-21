package java.lang;

// java.lang.Class — JLS 1.0 §20.3. Class IS the runtime type: every object's header
// refers to its Class, which carries the class's name/superclass/interfaces (below)
// plus a compiler-synthesized vtable. There is no public constructor (§20.3); the
// compiler constructs one Class singleton per class as data and populates these
// fields. The methods are ordinary reads of those fields.
public final class Class {
    // name is a char[] (a raw array, not a String) so a Class singleton is buildable in
    // a const-init without the String→String.class→name→… bootstrap cycle; getName()
    // wraps it lazily. superclass/interfaces are Class refs (§20.3.4/§20.3.5).
    private char[]  name;         // §20.3.2 fully-qualified name, as chars
    private Class   superclass;   // §20.3.4 direct superclass (null for Object/interface)
    private boolean iface;        // §20.3.3 is this an interface?
    private Class[] interfaces;   // §20.3.5 directly declared interfaces
    // Internal (NOT §20.3 API — 1.0 has no getComponentType): for an array Class, the Class
    // of its component type (String for String[], String[] for String[][]); null otherwise,
    // and null for a primitive-component array. Used by assignableFrom for §10.2 array covariance.
    private Class   componentType;
    // The whole-world AOT class registry, for §20.3.8 forName. There is no loading on this target:
    // every class is already compiled in, so the reflection bootstrap links every Class singleton
    // into this chain at module start. A plugin's bootstrap prepends its own classes to the chain
    // it imports from jre, so a plugin sees library classes and its own.
    private static Class registry;
    private Class   next;

    public String getName()          { return new String(name); }
    public Class  getSuperclass()    { return superclass; }
    public boolean isInterface()     { return iface; }
    public Class[] getInterfaces()   { return interfaces; }

    // §20.3.1: "class "/"interface " + the fully-qualified name.
    public String toString()         { return (iface ? "interface " : "class ") + getName(); }

    // §5.1.4 widening reference conversion: is `other` a subtype of this class/interface
    // (i.e. is this assignable FROM other)? Walk other's DIRECT supertypes — its
    // superclass and its declared interfaces — transitively, looking for this. Internal;
    // the runtime subtype query for the §10.10 ArrayStore check.
    boolean assignableFrom(Class other) {
        if (other == null) return false;
        if (other == this) return true;
        // §10.2 array covariance: an array type is assignable-from another array type iff
        // their component types are (recursively). A primitive-component array has a null
        // componentType, so two distinct primitive arrays only match via `other == this`
        // above (exactness); a reference/array component recurses.
        if (componentType != null && other.componentType != null)
            return componentType.assignableFrom(other.componentType);
        if (assignableFrom(other.superclass)) return true;
        Class[] ifs = other.interfaces;
        if (ifs != null) {
            int n = ifs.length;
            for (int i = 0; i < n; i++)
                if (assignableFrom(ifs[i])) return true;
        }
        return false;
    }

    // §15.19.2: `x instanceof T` is true iff x is non-null and could be cast to T without a
    // ClassCastException — i.e. x's runtime class is assignable to T. The compiler lowers
    // instanceof/checkcast to a REFERENCE array type through this (a runtime reflection check,
    // element-precise via assignableFrom's §10.2 covariance — a static ref.test to the shared
    // RefArray struct can't tell String[] from Integer[]). Internal (not §20.3 API in 1.0).
    // Static (like arrayStoreCheck) so the compiler emits it as an InvokeStatic — a virtual call
    // would need its receiver pre-spilled to a local, which the array cast/instanceof lowering
    // doesn't have. `cls` is the target array Class; x is the value being tested.
    static boolean isInstance(Class cls, Object x) {
        return x != null && cls.assignableFrom(x.getClass());
    }

    // JLS §10.10: the compiler emits a call to this before every covariant reference-array
    // element store. If the value's runtime class is not assignable to the array's ACTUAL
    // element type, throw ArrayStoreException. null is assignable to any reference type,
    // and an unstamped element type (ec == null) is unchecked. Internal (not §20.3 API).
    static void arrayStoreCheck(Class ec, Object v) {
        if (ec != null && v != null && !ec.assignableFrom(v.getClass()))
            throw new ArrayStoreException();
    }

    // §20.3.7: this model has no class loaders, so a class has none — return null.
    public ClassLoader getClassLoader()  { return null; }

    // §20.3.8: "Given the fully-qualified name of a class, this method attempts to locate, load, and
    // link the class. If it succeeds, then a reference to the Class object for the class is
    // returned. If it fails, then a ClassNotFoundException is thrown." Whole-world AOT: locating IS
    // the lookup; nothing is loaded or linked at run time.
    public static Class forName(String className) throws ClassNotFoundException {
        for (Class c = registry; c != null; c = c.next)
            if (c.nameEquals(className)) return c;
        throw new ClassNotFoundException(className);
    }

    // Compare this singleton's raw char[] name to `s` without materializing a String per candidate.
    private boolean nameEquals(String s) {
        int n = name.length;
        if (s.length() != n) return false;
        for (int i = 0; i < n; i++)
            if (name[i] != s.charAt(i)) return false;
        return true;
    }

    // §20.3.6: "creates and returns a new instance of the class represented by this Class object.
    // This is done exactly as if by a class instance creation expression with an empty argument
    // list." The compiler synthesized a `static $newInstance()` factory for every instantiable
    // class and made this Class object's `factory` funcref point at it; a non-instantiable class
    // (interface, abstract, or no no-arg constructor) has a null factory — §11.5.1.2's
    // InstantiationException. `instantiable`/`construct` are compiler intrinsics over this receiver:
    // instantiable reads whether the factory is non-null; construct call_ref's it.
    public Object newInstance() throws InstantiationException, IllegalAccessException {
        if (!instantiable()) throw new InstantiationException(getName());
        return construct();
    }
    private native boolean instantiable();   // → ClassInstantiable(this): factory funcref non-null?
    private native Object  construct();      // → ClassConstruct(this): call_ref the factory
}
