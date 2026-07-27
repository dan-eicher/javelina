// Ch15 — JLS chapter 15, Expressions. Currently the §15.27 constant-expression rules for
// String, which are what make §3.10.5's interning observable.
//
// The core case is the spec's OWN worked example (§3.10.5, printed p.26). Its six points are
// stated there as prose; this asserts them:
//
//     hello == "Hello"                 literal strings in the same class are one object
//     Other.hello == hello             ...across classes in the same package too
//     hello == ("Hel"+"lo")            a CONSTANT expression is computed at compile time and
//                                      "then treated as if [it] were literals" — so it interns
//     hello == ("Hel"+lo)              computed at RUN time, therefore a distinct object
//     hello == ("Hel"+lo).intern()     interning a computed string yields the literal's object

class Ch15Other { static String hello = "Hello"; }

public class Ch15 {

    // JLS 15.27
    static void s15_27() {
        String hello = "Hello", lo = "lo";

        Check.same("15.27", "a literal is one shared instance within a class", hello, "Hello");
        Check.same("15.27", "and across classes in the same package", Ch15Other.hello, hello);

        // The rule this section adds: a constant expression is folded at compile time, so the
        // result IS a literal and shares its instance.
        Check.same("15.27", "a CONSTANT concatenation folds to a literal and interns",
                   hello, ("Hel" + "lo"));
        Check.same("15.27", "folding is transitive across several constant operands",
                   hello, ("H" + "e" + "l" + "lo"));

        // ...and the boundary: one non-constant operand makes the whole expression non-constant,
        // so it is built at run time and is a DIFFERENT object with equal contents.
        Check.notSame("15.27", "a run-time concatenation is a distinct object", hello, ("Hel" + lo));
        Check.eq("15.27", "…with equal contents all the same", hello.equals("Hel" + lo), true);
        Check.same("15.27", "and interning it yields the literal's instance",
                   hello, ("Hel" + lo).intern());

        // A folded constant is an ordinary String in every other respect.
        Check.eq("15.27", "the folded literal has the right length", ("Hel" + "lo").length(), 5);
        Check.eq("15.27", "the folded literal has the right contents",
                 ("Hel" + "lo").charAt(4), 'o');
        Check.eq("15.27", "folding an empty operand changes nothing", ("" + "ab" + "").length(), 2);
    }

    public static void run() {
        s15_27();
    }
}
