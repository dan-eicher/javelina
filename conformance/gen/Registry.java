// Registry — every Snippet, indexed by the type it yields.
//
// Registration is explicit, so a snippet library is just a class with
//
//     public static void install(Registry r) { r.register(new ...); ... }
//
// and adding a library is one call in GenMain. Nothing scans, nothing reflects, nothing
// depends on class-loading order: the enumeration below is reproducible run to run because
// this list is built by executing install() calls in a written-down order.
//
// Two orders matter and both are registration order:
//   - byType(t) returns t's snippets in the order they were registered;
//   - types() returns the types in the order each was FIRST registered.
// Nothing in this class ever iterates a Hashtable, whose order is unspecified.
public class Registry {

    private final java.util.Vector    snippets  = new java.util.Vector();
    private final java.util.Vector    typeOrder = new java.util.Vector();  // type names, first-seen order
    private final java.util.Hashtable buckets   = new java.util.Hashtable(); // String -> Vector of Snippet
    private final java.util.Hashtable idMap     = new java.util.Hashtable(); // String -> Snippet

    public Registry() { }

    /** Add a snippet. Ids are unique registry-wide: a duplicate id makes two different
     *  fragments indistinguishable in a case comment, which is the one place a failing
     *  line names what produced it. */
    public void register(Snippet s) {
        if (s == null) throw new RuntimeException("Registry.register: null snippet");
        String id = s.id();
        if (id == null || id.length() == 0) throw new RuntimeException("Registry.register: empty id");
        if (idMap.get(id) != null) throw new RuntimeException("Registry.register: duplicate id " + id);

        String t = s.type();
        if (t == null || t.length() == 0)
            throw new RuntimeException("Registry.register: " + id + " has no type()");
        String[] hs = s.holeTypes();
        if (hs == null) throw new RuntimeException("Registry.register: " + id + " has null holeTypes()");
        for (int i = 0; i < hs.length; i++)
            if (hs[i] == null || hs[i].length() == 0 || hs[i].equals("void"))
                throw new RuntimeException("Registry.register: " + id + " hole " + i + " has no usable type");
        String[] secs = s.sections();
        if (secs == null || secs.length == 0)
            throw new RuntimeException("Registry.register: " + id + " cites no JLS section");

        idMap.put(id, s);
        snippets.addElement(s);
        java.util.Vector v = (java.util.Vector) buckets.get(t);
        if (v == null) { v = new java.util.Vector(); buckets.put(t, v); typeOrder.addElement(t); }
        v.addElement(s);
    }

    /** The snippets yielding type `t`, in registration order. Empty if none — never null. */
    public Snippet[] byType(String t) {
        java.util.Vector v = (java.util.Vector) buckets.get(t);
        if (v == null) return new Snippet[0];
        Snippet[] out = new Snippet[v.size()];
        for (int i = 0; i < out.length; i++) out[i] = (Snippet) v.elementAt(i);
        return out;
    }

    /** True if any snippet yields `t`. A hole whose type nothing produces is a library
     *  defect, not a coverage gap; Stitcher fails loudly on one. */
    public boolean produces(String t) { return buckets.get(t) != null; }

    /** Every registered type, in the order each was first registered. */
    public String[] types() {
        String[] out = new String[typeOrder.size()];
        for (int i = 0; i < out.length; i++) out[i] = (String) typeOrder.elementAt(i);
        return out;
    }

    /** Every snippet, in registration order. */
    public Snippet[] all() {
        Snippet[] out = new Snippet[snippets.size()];
        for (int i = 0; i < out.length; i++) out[i] = (Snippet) snippets.elementAt(i);
        return out;
    }

    public Snippet byId(String id) { return (Snippet) idMap.get(id); }

    public int size() { return snippets.size(); }
}
