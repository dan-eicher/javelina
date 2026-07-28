// Lib15 — JLS chapter 15, Expressions. §15.24 only, for now.
//
// §15.24 was already marked COVERED by five generated cases before any of this existed. They
// exercised the operator, not its TYPE RULE, and the ledger's own reason line named the two
// rules that were missing -- "byte/short -> short; int constant representable in T -> T" --
// while sema.c computed neither. A `// JLS 15.24` marker is produced identically by a case that
// tests one path and by one that tests all five, which is the failure cardinality.tsv exists
// to make visible elsewhere; here it is made visible by giving each bullet its own snippet.
//
// The five bullets of "The type of a conditional expression is determined as follows" (p.368):
//
//   1. same type (which may be the null type)            -> that type
//   2a. one byte, one short                              -> short
//   2b. one is T in {byte,short,char}, other is a
//       constant int representable in T                  -> T
//   2c. otherwise                                        -> binary numeric promotion (§5.6.2)
//   3. one null type, one reference type                 -> that reference type
//   4. two different reference types                     -> the one the other converts to,
//                                                           or a compile-time error
//
// Bullet 4's error half cannot be a running program, so it is
// conformance/reject/ch15_conditional_incompatible_refs.java. Everything here RUNS, and each
// snippet is written so the computed type is observable in the printed VALUE rather than merely
// in the fact that it compiled -- `(true ? 'A' : 66)` prints "A" when the type is char and "65"
// when it is int, which is the whole difference between bullet 2b holding and not.
public class Lib15 {

    private Lib15() {}

    public static void install(Registry r) {
        r.register(new Sn15CondByteShort());
        r.register(new Sn15CondConstChar());
        r.register(new Sn15CondNullRef());
        r.register(new Sn15CondRefWiden());
        r.register(new Sn15CondPromote());
    }
}

/** §15.24 bullet 2a: "If one of the operands is of type byte and the other is of type short,
 *  then the type of the conditional expression is short." Observable because the result is
 *  assigned to a short: had the type promoted to int, §5.2 would reject the assignment (a
 *  non-constant int does not narrow implicitly), so this snippet would not compile at all. */
class Sn15CondByteShort implements Snippet {
    public String   id()          { return "t15.cond.byte.short"; }
    public String[] sections()    { return Strs.of("15.24"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ byte b = (byte) (" + h[0] + "); short s = 9;"
             + " short r = true ? b : s;"
             + " System.out.println(r); }";
    }
    public Val expect(Val[] h) { return Val.ofShort((short) (byte) h[0].asInt()); }
}

/** §15.24 bullet 2b, with T = char, which is the case that shows the difference. 66 is a
 *  constant expression of type int representable in char, so the type is char and BOTH arms
 *  print as characters. Both directions of the condition are in one line because the rule is
 *  about the expression's type, not about which arm ran. */
class Sn15CondConstChar implements Snippet {
    public String   id()          { return "t15.cond.const.char"; }
    public String[] sections()    { return Strs.of("15.24"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String render(String[] h) {
        return "{ System.out.println((true ? 'A' : 66) + \"\" + (false ? 'A' : 66)); }";
    }
    public Val expect(Val[] h) { return Val.ofString("AB"); }
}

/** §15.24 bullet 3: "If one of the second and third operands is of the null type and the type
 *  of the other is a reference type, then the type of the conditional expression is that
 *  reference type." Both orders, since the bullet is symmetric and an implementation that
 *  simply returned one arm would satisfy only one of them. */
class Sn15CondNullRef implements Snippet {
    public String   id()          { return "t15.cond.null.ref"; }
    public String[] sections()    { return Strs.of("15.24"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String render(String[] h) {
        return "{ String s = false ? null : \"ok\";"
             + " System.out.println(s + (true ? null : \"x\")); }";
    }
    public Val expect(Val[] h) { return Val.ofString("oknull"); }
}

/** §15.24 bullet 4, the half that succeeds: two different reference types where one converts to
 *  the other by §5.2, so the type is that latter type. String converts to Object, so T is
 *  Object and the assignment is legal. */
class Sn15CondRefWiden implements Snippet {
    public String   id()          { return "t15.cond.ref.widen"; }
    public String[] sections()    { return Strs.of("15.24"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.none(); }

    public String render(String[] h) {
        return "{ Object o = true ? \"str\" : new Object();"
             + " System.out.println(o); }";
    }
    public Val expect(Val[] h) { return Val.ofString("str"); }
}

/** §15.24 bullet 2c: neither special numeric case applies, so §5.6.2 binary numeric promotion
 *  gives the type. int against long promotes to long, and the hole rides through it. */
class Sn15CondPromote implements Snippet {
    public String   id()          { return "t15.cond.promote"; }
    public String[] sections()    { return Strs.of("15.24", "5.6.2"); }
    public String   type()        { return "void"; }
    public String[] holeTypes()   { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ long r = false ? (" + h[0] + ") : 100L;"
             + " System.out.println(r); }";
    }
    public Val expect(Val[] h) { return Val.ofLong(100L); }
}
