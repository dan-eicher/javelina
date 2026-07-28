// Lib4Ref — JLS §4.3 and §4.5, the reference-type half of chapter 4.
//
// These six sections were COVERED by conformance/jls/Ch4.java and by nothing else. That class
// is not input to this file: crisp-tallying-chapters §4 has the corpus written from the spec,
// and porting the old assertions forward would launder whatever they missed into the generator,
// which would then certify it. Every claim below is transcribed from the page cited beside it.
//
//   §4.3    p.37  "There are three kinds of reference types: class types (§8), interface types
//                 (§9), and array types (§10)."
//   §4.3.2  p.40  "The standard class Object is a superclass of all other classes. A variable
//                 of type Object can hold a reference to any object, whether it is an instance
//                 of a class or an array (§10). All class and array types inherit the methods
//                 of class Object" -- getClass, toString, equals, hashCode, clone.
//   §4.3.3  p.41  "A String object has a constant (unchanging) value. String literals (§3.10.5)
//                 are references to instances of class String. The string concatenation
//                 operator + (§15.17.1) implicitly creates a new String object."
//   §4.3.4  p.42  two reference types are the same if both are class or interface types with
//                 the same fully qualified name, or both array types with the same component
//                 type.
//   §4.5.2  p.44  "A variable of reference type can hold either of the following: a null
//                 reference [or] a reference to any object whose class is assignment
//                 compatible with the type of the variable."
//   §4.5.5  p.47  "Every object belongs to some particular class: the class that was mentioned
//                 in the creation expression ..., the class whose class object was used to
//                 invoke the newInstance method (§20.3.6) ..., or the String class for objects
//                 implicitly created by the string concatenation operator +. ... An object is
//                 said to be an instance of its class and of all superclasses of its class."
//
// The helper declarations are shared: Emit deduplicates decls by text, so one T4Ref hierarchy
// serves every template here and each case gets exactly one copy.
public class Lib4Ref {

    private Lib4Ref() {}

    public static void install(Registry r) {
        r.register(new Sn4RefKinds());
        r.register(new Sn4ObjectMembers());
        r.register(new Sn4StringConstant());
        r.register(new Sn4SameRefTypes());
        r.register(new Sn4RefVarHolds());
        r.register(new Sn4TypesVsClasses());
    }

    /** The hierarchy every template in this file shares. A class, an interface it implements,
     *  and a subclass -- the three things §4.3/§4.5.5 need in order to say anything. */
    static String[] decls() {
        String[] d = { "interface T4RefFace {\n"
                     + "    int face();\n"
                     + "}",

                       "class T4Ref implements T4RefFace {\n"
                     + "    int n;\n"
                     + "    T4Ref(int n) { this.n = n; }\n"
                     + "    public int face() { return 7; }\n"
                     + "}",

                       "class T4RefSub extends T4Ref {\n"
                     + "    T4RefSub(int n) { super(n * 2); }\n"
                     + "}" };
        return d;
    }
}

/** §4.3: one variable of each of the three kinds of reference type -- a class type, an
 *  interface type, and an array type. The interface-typed variable holds the same object as
 *  the class-typed one, which is what makes it a KIND OF TYPE rather than a kind of object. */
class Sn4RefKinds implements Snippet, Declaring {
    public String   id()        { return "t4.ref.three.kinds"; }
    public String[] sections()  { return Strs.of("4.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib4Ref.decls(); }

    public String render(String[] h) {
        return "{ T4Ref byClass = new T4Ref(" + h[0] + ");"
             + " T4RefFace byFace = byClass;"
             + " int[] byArray = new int[2];"
             + " System.out.println(byClass.n + byFace.face() + byArray.length); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(h[0].asInt() + 9); }
}

/** §4.3.2: an Object-typed variable holds an instance OR an array, and both inherit Object's
 *  methods. Each of the five contributes a distinct bit so a single missing one changes the
 *  total rather than being absorbed. clone is excluded on purpose: it is `protected` and
 *  §6.6.2 governs reaching it from another class, which is not what this section claims. */
class Sn4ObjectMembers implements Snippet, Declaring {
    public String   id()        { return "t4.ref.object.members"; }
    public String[] sections()  { return Strs.of("4.3.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib4Ref.decls(); }

    public String render(String[] h) {
        return "{ Object fromClass = new T4Ref(" + h[0] + ");"
             + " Object fromArray = new int[3];"
             + " int got = 0;"
             + " if (fromClass.getClass() != null) got += 1;"
             + " if (fromArray.getClass() != null) got += 2;"        // an ARRAY has getClass too
             + " if (fromClass.equals(fromClass)) got += 4;"
             + " if (fromClass.hashCode() == fromClass.hashCode()) got += 8;"
             + " if (fromClass.toString() != null) got += 16;"
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(31 + h[0].asInt()); }
}

/** §4.3.3: the value is constant, so every "mutating" operation returns a NEW String and leaves
 *  the original alone; and + "implicitly creates a new String object", which is why the result
 *  is not the same object as the literal it was built from.
 *
 *  `s` is a non-final local, so `s + "d"` is NOT a constant expression and §15.27 does not fold
 *  and intern it -- that distinction is the whole reason the identity comparison is meaningful. */
class Sn4StringConstant implements Snippet {
    public String   id()        { return "t4.ref.string.constant"; }
    public String[] sections()  { return Strs.of("4.3.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "{ String s = \"abc\";"
             + " String up = s.toUpperCase();"
             + " String cat = s + \"d\";"
             + " System.out.println(s + \"|\" + up + \"|\" + cat"
             + " + \"|\" + (s == cat ? 1 : 0) + (s == \"abc\" ? 2 : 0)); }";
    }
    // s unchanged by either operation; cat is a new object (0); the literal IS the interned
    // instance the variable already refers to (2), which is §3.10.5's half of the same claim.
    public Val expect(Val[] h) { return Val.ofString("abc|ABC|abcd|02"); }
}

/** §4.3.4: same fully qualified name means the same class, and two array types are the same
 *  type exactly when their component types are. Observable through Class identity. */
class Sn4SameRefTypes implements Snippet {
    public String   id()        { return "t4.ref.same.types"; }
    public String[] sections()  { return Strs.of("4.3.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int[] a = new int[1]; int[] b = new int[9]; long[] c = new long[1];"
             + " String lit = \"x\";"
             + " Object built = new StringBuffer().append(\"y\").toString();"
             + " int got = 0;"
             + " if (a.getClass() == b.getClass()) got += 1;"       // same component type
             + " if (a.getClass() != c.getClass()) got += 2;"       // different component type
             + " if (lit.getClass() == built.getClass()) got += 4;" // same name, same class
             + " System.out.println(got + (" + h[0] + ")); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(7 + h[0].asInt()); }
}

/** §4.5.2: a reference variable holds null, or a reference to any object whose class is
 *  assignment compatible with the variable's type. Both alternatives, and the interface-typed
 *  variable is the one that shows "assignment compatible" is not "the same class". */
class Sn4RefVarHolds implements Snippet, Declaring {
    public String   id()        { return "t4.ref.var.holds"; }
    public String[] sections()  { return Strs.of("4.5.2"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib4Ref.decls(); }

    public String render(String[] h) {
        return "{ T4RefFace f = null;"
             + " int got = (f == null) ? 1 : 0;"
             + " f = new T4RefSub(" + h[0] + ");"                   // a SUBCLASS instance
             + " Object asObject = f;"                              // and Object holds it too
             + " T4Ref back = (T4Ref) asObject;"
             + " System.out.println(got + f.face() + back.n); }";
    }
    // T4RefSub's constructor doubles: super(n * 2).
    public Val expect(Val[] h) { return Val.ofInt(8 + 2 * h[0].asInt()); }
}

/** §4.5.5: "An object is said to be an instance of its class and of all superclasses of its
 *  class", and an object's class comes from its ORIGIN -- the creation expression, or the
 *  String class for an object the + operator created implicitly.
 *
 *  The declared type of `held` is T4Ref throughout; its class is T4RefSub. That difference is
 *  the section's title. */
class Sn4TypesVsClasses implements Snippet, Declaring {
    public String   id()        { return "t4.ref.types.vs.classes"; }
    public String[] sections()  { return Strs.of("4.5.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }
    public String[] decls()     { return Lib4Ref.decls(); }

    public String render(String[] h) {
        return "{ T4Ref held = new T4RefSub(" + h[0] + ");"
             + " int got = 0;"
             + " if (held instanceof T4RefSub) got += 1;"            // instance of its class
             + " if (held instanceof T4Ref) got += 2;"               // ...and of its superclass
             + " if (held instanceof T4RefFace) got += 4;"           // ...and its interfaces
             + " if (held.getClass() == new T4RefSub(0).getClass()) got += 8;"
             + " String made = \"a\" + held.n;"                      // + creates a String
             + " if (made.getClass() == \"lit\".getClass()) got += 16;"
             + " System.out.println(got); }";
    }
    public Val expect(Val[] h) { return Val.ofInt(31); }
}
