// Lib14c — JLS chapter 14, batch three: §14.3.3 and §14.3.4. From pp.268-269.
//
//   §14.3.3 a local variable (or parameter) hides a field of the same name throughout its
//           scope, and the hidden field is still reachable as `this.x`. The spec's own example
//           is the constructor idiom: "the constructor takes parameters having the same names
//           as the fields to be initialized ... this.first = first;".
//   §14.3.4 "A local variable declaration statement is an executable statement. Every time it
//           is executed, the declarators are processed IN ORDER FROM LEFT TO RIGHT ... Each
//           initialization (except the first) is executed only if the evaluation of the
//           preceding initialization expression completes normally."
public class Lib14c {

    private Lib14c() {}

    public static void install(Registry r) {
        r.register(new Sn14HidingThis());
        r.register(new Sn14DeclaratorOrder());
        r.register(new Sn14DeclaratorStopsOnThrow());
    }
}

/** §14.3.3, the spec's Pair idiom verbatim in shape: a constructor whose parameters have the
 *  same names as the fields they initialize, so each parameter HIDES its field and `this.first`
 *  is the only way to reach the field.
 *
 *  The template checks both directions in one program -- the bare name is the parameter, the
 *  qualified name is the field -- because an implementation that resolved the bare name to the
 *  field would make `this.first = first` a self-assignment and leave the fields at their
 *  defaults, which is a silent wrong answer rather than a compile error. */
class Sn14HidingThis implements Snippet, Declaring {
    public String   id()        { return "t14.hiding.this.reaches.field"; }
    public String[] sections()  { return Strs.of("14.3.3"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Pair {\n"
                     + "    int first, second;\n"
                     + "    T14Pair(int first, int second) {\n"
                     + "        this.first = first;\n"
                     + "        this.second = second;\n"
                     + "    }\n"
                     // The parameter hides the field; `field` names the parameter and
                     // `this.field` the field, in one expression.
                     + "    int gap(int first) { return first - this.first; }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Pair p = new T14Pair(" + h[0] + ", 5);"
             + " System.out.println(p.first + p.second + p.gap(" + h[0] + " + 7)); }";
    }
    // first = hole, second = 5, gap(hole+7) = (hole+7) - hole = 7
    public Val expect(Val[] h) { return Val.ofInt(h[0].asInt() + 12); }
}

/** §14.3.4: "the declarators are processed in order from left to right", and a later declarator
 *  may use an earlier one -- so the ORDER is observable in the values rather than only in the
 *  fact that it compiled. */
class Sn14DeclaratorOrder implements Snippet {
    public String   id()        { return "t14.declarator.order"; }
    public String[] sections()  { return Strs.of("14.3.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }

    public String render(String[] h) {
        return "{ int a = (" + h[0] + ") + 1, b = a * 2, c = b + a;"
             + " System.out.println(a + \"|\" + b + \"|\" + c); }";
    }
    public Val expect(Val[] h) {
        int a = h[0].asInt() + 1, b = a * 2, c = b + a;
        return Val.ofString(a + "|" + b + "|" + c);
    }
}

/** §14.3.4: "Each initialization (except the first) is executed only if the evaluation of the
 *  preceding initialization expression completes normally."
 *
 *  The middle initializer throws, so the third is never evaluated -- observed through a counter
 *  the initializers bump, not through the variables themselves, which are out of scope once the
 *  declaration completes abruptly. */
class Sn14DeclaratorStopsOnThrow implements Snippet, Declaring {
    public String   id()        { return "t14.declarator.stops.on.throw"; }
    public String[] sections()  { return Strs.of("14.3.4"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.none(); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T14Steps {\n"
                     + "    static int count = 0;\n"
                     + "    static int step(int n) { count += n; return n; }\n"
                     + "    static int boom() { count += 10; throw new RuntimeException(\"b\"); }\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T14Steps.count = 0;"
             + " try { int a = T14Steps.step(1), b = T14Steps.boom(), c = T14Steps.step(100); }"
             + " catch (RuntimeException e) { }"
             + " System.out.println(T14Steps.count); }";
    }
    // 1 from the first, 10 from the throwing second, and the third never runs.
    public Val expect(Val[] h) { return Val.ofInt(11); }
}
