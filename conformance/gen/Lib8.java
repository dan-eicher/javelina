// Lib8 — JLS chapter 8, Classes.
//
// Only §8.3.1.2 so far, and only its POSITIVE half. The section makes three claims, two of
// which are compile-time errors and therefore live in conformance/reject, where the oracle is
// javelinac's exit code:
//
//   "its declarator must include a variable initializer or a compile-time error occurs"
//        -> reject/ch8_final_field_no_initializer.java
//   "Any attempt to assign to a final field results in a compile-time error"
//        -> reject/ch8_final_field_assigned.java
//
// The third claim is about a program that RUNS, so it belongs here:
//
//   "If a final field holds a reference to an object, then the state of the object may be
//    changed by operations on the object, but the field will always refer to the same object.
//    This applies also to arrays, because arrays are objects; if a final field holds a
//    reference to an array, then the components of the array may be changed by operations on
//    the array, but the field will always refer to the same array." (p.146)
//
// Without it, §8.3.1.2 would be marked COVERED on the two rejections alone while the one thing
// it says about running programs went untested — and the claim is load-bearing rather than
// decorative: compiler/lib/java/lang/FDBigInteger.java fills POW_5_CACHE's components after
// the declarator has initialized it, which is legal only because of this sentence.
//
// The rest of chapter 8 is not here. It is its own pass.
public class Lib8 {

    private Lib8() {}

    public static void install(Registry r) {
        r.register(new Sn8FinalReferent());
        r.register(new Sn8FinalNoInitializer());
        r.register(new Sn8FinalAssigned());
        r.register(new Sn8FinalAssignedInCtor());
        r.register(new Sn8FinalAssignedInStaticInit());
    }
}

/** §8.3.1.2: a final field's REFERENT is mutable; only the binding is fixed. Both halves are
 *  in one program on purpose — the mutation and the identity check are a single claim, and a
 *  test that changed the state without confirming the field still pointed at it would pass on
 *  an implementation that quietly reassigned. */
class Sn8FinalReferent implements Snippet, Declaring {

    public String   id()          { return "t8.final.referent.mutable"; }
    public String[] sections()    { return Strs.of("8.3.1.2"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }
    public String[] imports()     { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T8Holder {\n"
                     + "    final int[]       cells = new int[3];\n"
                     + "    final StringBuffer buf  = new StringBuffer(\"a\");\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T8Holder o = new T8Holder();"
             + " int[] sameArray = o.cells; StringBuffer sameBuf = o.buf;"
             + " o.cells[0] = 5; o.cells[1] = 6;"          // components of a final array change
             + " o.buf.append(\"b\");"                     // state of a final field's object changes
             + " System.out.println(o.cells[0] + o.cells[1] + o.buf.length()"
             + " + ((o.cells == sameArray && o.buf == sameBuf) ? 1 : 0)); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(14); }
}

/** §8.3.1.2, first sentence: "A field can be declared final, in which case its declarator must
 *  include a variable initializer or a compile-time error occurs."
 *
 *  Java 1.0 has no blank finals: the initializer is required IN THE DECLARATOR, so a
 *  constructor that assigns the field is not a substitute. That is the 1.1 rule, and applying
 *  it here would accept this program. */
class Sn8FinalNoInitializer implements Snippet {
    public String   id()        { return "t8.final.no.initializer"; }
    public String[] sections()  { return Strs.of("8.3.1.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T8FinalNoInit {\n"
             + "    final int blank;\n"
             + "\n"
             + "    T8FinalNoInit(int v) {\n"
             + "        blank = v;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("final field"); }
}

/** §8.3.1.2, second sentence: "Any attempt to assign to a final field results in a compile-time
 *  error."
 *
 *  The field HAS its required initializer, so the first sentence is satisfied and only the
 *  second is at issue -- an assignment from an ordinary method, which is the case a blank-final
 *  implementation would still reject but for the wrong reason (already assigned) and only
 *  inside a constructor. */
class Sn8FinalAssigned implements Snippet {
    public String   id()        { return "t8.final.assigned"; }
    public String[] sections()  { return Strs.of("8.3.1.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T8FinalAssigned {\n"
             + "    final int fixed = 7;\n"
             + "\n"
             + "    void bump() {\n"
             + "        fixed = 8;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("final field"); }
}

/** §8.3.1.2, second sentence, in a CONSTRUCTOR: "any attempt" has no constructor carve-out —
 *  the 1.0 field is bound by its declarator, and a constructor assignment on top of it is an
 *  attempt like any other (the 1.1 blank-final rule is what would accept it; §8.5 and §8.6
 *  grant no exception either). */
class Sn8FinalAssignedInCtor implements Snippet {
    public String   id()        { return "t8.final.assigned.ctor"; }
    public String[] sections()  { return Strs.of("8.3.1.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T8FinalAssignedCtor {\n"
             + "    final int fixed = 7;\n"
             + "\n"
             + "    T8FinalAssignedCtor() {\n"
             + "        fixed = 8;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("final field"); }
}

/** §8.3.1.2, second sentence, in a STATIC INITIALIZER: the static twin of the constructor
 *  case — a static final is bound by its declarator, and the initializer block's assignment
 *  is an attempt like any other. */
class Sn8FinalAssignedInStaticInit implements Snippet {
    public String   id()        { return "t8.final.assigned.staticinit"; }
    public String[] sections()  { return Strs.of("8.3.1.2"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T8FinalAssignedStatic {\n"
             + "    static final int FIXED = 7;\n"
             + "\n"
             + "    static {\n"
             + "        FIXED = 8;\n"
             + "    }\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("final field"); }
}
