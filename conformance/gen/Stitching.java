// Stitching — one enumerated tree: a Snippet with each of its holes filled by a Stitching
// of the hole's required type.
//
// Immutable, and deliberately SHARED: the enumerator memoises sub-enumerations, so the same
// Stitching object hangs under many parents. That is why render()/expect() are computed once
// and cached — without it a depth-3 enumeration re-renders the same subtree thousands of
// times, and re-runs the same expect() folds with it.
//
// The two halves that must never disagree:
//   render() produces the Java source, holes rendered in;
//   expect() folds the SAME tree bottom-up into the value that source must produce.
// Neither ever runs the program.
//
// render(), expect(), id(), sections(), depth() and size() all recurse over the tree. The
// recursion is BOUNDED BY THE TREE'S HEIGHT, which the enumerator fixed at (depth bound + 1)
// levels when it built it — the structure cannot be cyclic, because a Stitching's children
// are enumerated strictly before it exists.
public final class Stitching {

    // Constructor-assigned, so not final (§8.3.1.2 wants the initializer in the declarator).
    private Snippet     snip;
    private Stitching[] kids;

    private String    renderCache;
    private Val       expectCache;
    private String    idCache;
    private String[]  sectionCache;

    public Stitching(Snippet s, Stitching[] kids) {
        if (s == null) throw new RuntimeException("Stitching: null snippet");
        if (kids == null) kids = new Stitching[0];
        String[] hs = s.holeTypes();
        if (hs.length != kids.length)
            throw new RuntimeException("Stitching " + s.id() + ": " + kids.length
                                       + " fills for " + hs.length + " holes");
        // The validity invariant, enforced where it is established rather than checked
        // downstream: a hole typed T is only ever filled by a snippet whose type() is T.
        // Every stitched program therefore type-checks by construction.
        for (int i = 0; i < kids.length; i++)
            if (!kids[i].snippet().type().equals(hs[i]))
                throw new RuntimeException("Stitching " + s.id() + ": hole " + i + " wants "
                                           + hs[i] + ", got " + kids[i].snippet().type()
                                           + " from " + kids[i].snippet().id());
        this.snip = s;
        this.kids = kids;
    }

    public Snippet     snippet()  { return snip; }
    public Stitching[] children() { return kids; }
    public String      type()     { return snip.type(); }

    /** The Java source text — an expression, or a statement when type() is "void". */
    public String render() {
        if (renderCache == null) {
            String[] hs = new String[kids.length];
            for (int i = 0; i < kids.length; i++) hs[i] = kids[i].render();
            renderCache = snip.render(hs);
            if (renderCache == null) throw new RuntimeException("Snippet " + snip.id() + ": render() returned null");
        }
        return renderCache;
    }

    /** The composed expected value. */
    public Val expect() {
        if (expectCache == null) {
            Val[] vs = new Val[kids.length];
            for (int i = 0; i < kids.length; i++) vs[i] = kids[i].expect();
            expectCache = snip.expect(vs);
            if (expectCache == null) throw new RuntimeException("Snippet " + snip.id() + ": expect() returned null");
            // Where display() DISPATCHES on the type, the type must be the declared one, or
            // the .expected line is silently rendered by the wrong printing rule. `float`
            // vs `double` is the case that bites (Float.toString(0.1f) is "0.1",
            // Double.toString of the same value is "0.10000000149011612"), and `char` vs
            // `int` is the other. Reference kinds are exempt on purpose: there the Val's
            // type is the RUN-TIME class, which a §5.1.5 cast snippet needs and which is
            // legitimately narrower than the snippet's static type.
            int k = expectCache.kind();
            if ((k == Val.LONG || k == Val.DOUBLE || k == Val.BOOLEAN)
                && !snip.type().equals("void") && !expectCache.type().equals(snip.type()))
                throw new RuntimeException("Snippet " + snip.id() + ": type() is " + snip.type()
                                           + " but expect() returned a " + expectCache.type());
        }
        return expectCache;
    }

    /** "outer(inner,inner)" — a stable name for this tree, written as the case comment so a
     *  failing line names the snippets that produced it. */
    public String id() {
        if (idCache == null) {
            if (kids.length == 0) idCache = snip.id();
            else {
                StringBuffer b = new StringBuffer(snip.id());
                b.append('(');
                for (int i = 0; i < kids.length; i++) { if (i > 0) b.append(','); b.append(kids[i].id()); }
                b.append(')');
                idCache = b.toString();
            }
        }
        return idCache;
    }

    /** Every JLS section this tree exercises, deduped, in pre-order (this snippet's own
     *  sections first, then each hole's). Emit sorts the per-file union. */
    public String[] sections() {
        if (sectionCache == null) {
            java.util.Vector v = new java.util.Vector();
            collect(v);
            sectionCache = new String[v.size()];
            for (int i = 0; i < sectionCache.length; i++) sectionCache[i] = (String) v.elementAt(i);
        }
        return sectionCache;
    }

    private void collect(java.util.Vector into) {
        String[] mine = snip.sections();
        for (int i = 0; i < mine.length; i++) if (!into.contains(mine[i])) into.addElement(mine[i]);
        for (int i = 0; i < kids.length; i++) kids[i].collect(into);
    }

    /** Companion type declarations this stitching needs, its children's included. Deduplicated
     *  by text, because a helper reached through two different holes is still one declaration
     *  and a case is one compilation unit. */
    public void collectDecls(java.util.Vector into) {
        if (snip instanceof Declaring) {
            String[] d = ((Declaring) snip).decls();
            for (int i = 0; i < d.length; i++) if (!into.contains(d[i])) into.addElement(d[i]);
        }
        for (int i = 0; i < kids.length; i++) kids[i].collectDecls(into);
    }

    /** Single-type-import names this stitching needs, its children's included. Deduplicated for
     *  the same reason as decls(): §7.5 makes importing the same name twice legal but importing
     *  two different types under one simple name an error, so one entry per name is also the
     *  form that would surface such a clash. */
    public void collectImports(java.util.Vector into) {
        if (snip instanceof Declaring) {
            String[] m = ((Declaring) snip).imports();
            for (int i = 0; i < m.length; i++) if (!into.contains(m[i])) into.addElement(m[i]);
        }
        for (int i = 0; i < kids.length; i++) kids[i].collectImports(into);
    }

    /** 1 for a leaf; 1 + the deepest child otherwise. */
    public int depth() {
        int d = 0;
        for (int i = 0; i < kids.length; i++) { int c = kids[i].depth(); if (c > d) d = c; }
        return d + 1;
    }

    /** Number of snippets in the tree. */
    public int size() {
        int n = 1;
        for (int i = 0; i < kids.length; i++) n += kids[i].size();
        return n;
    }

    public String toString() { return id(); }
}
