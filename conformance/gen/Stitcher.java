// Stitcher — the enumerator.
//
// Given a Registry, a target type and a depth bound, enumerate every stitching of that type
// DETERMINISTICALLY. Three orders make it reproducible run to run:
//
//   1. snippets are visited in REGISTRATION order (Registry.byType);
//   2. a snippet's holes are filled in odometer order with hole 0 varying SLOWEST — the
//      last hole is the inner loop, exactly as nested for-loops would run it;
//   3. when the cap bites, each snippet's quota is filled from the FRONT of that order.
//
// Nothing here iterates a Hashtable; the memo table is only ever looked up by an explicit
// key. So two runs of the same registry emit byte-identical cases.
//
// TERMINATION is the depth bound: a snippet with holes is only expanded while depth > 0,
// and each level of recursion decrements it, so leaves (holeTypes().length == 0) are the
// only thing enumerable at depth 0. FINITENESS at a given depth is the per-type cap.
//
// THE CAP IS A FAIR SHARE, NOT A PREFIX. Truncating the concatenated list would give the
// whole budget to whichever snippet was registered first — at depth 2 the boot library's
// arith.mul.int took all 250 of `int`'s slots and arith.div.int, with every
// ArithmeticException case in it, contributed ZERO. A snippet's coverage must not depend on
// where in install() it happens to sit. So the budget is water-filled: every snippet gets an
// equal share up to a level L, snippets offering less than L take only what they have, and
// the remainder goes round in registration order.
//
// THE CAP IS RECORDED. A truncated enumeration that says nothing reads as full coverage, so
// every cut is logged with the type, the depth, how many stitchings were enumerable, how
// many were kept, and what each snippet kept of what it offered. GenMain writes that log
// next to the cases.
public class Stitcher {

    /** Saturation point for the enumerable-count arithmetic. Well past any real cap, and it
     *  keeps a product of hole counts from overflowing into a nonsense drop report. */
    private static final long SAT = 1000000000000000L;   // 1e15

    // Constructor-assigned, so not final (§8.3.1.2 wants the initializer in the declarator).
    private Registry reg;
    private int      cap;

    private final java.util.Hashtable memo  = new java.util.Hashtable();  // "type@depth" -> Stitching[]
    private final java.util.Vector    drops = new java.util.Vector();     // report lines, in the order cuts fired
    private long dropped = 0;
    private int  cuts    = 0;

    /** @param capPerType the most stitchings kept for one (type, depth). Must be > 0. */
    public Stitcher(Registry r, int capPerType) {
        if (r == null) throw new RuntimeException("Stitcher: null registry");
        if (capPerType <= 0) throw new RuntimeException("Stitcher: cap must be positive, got " + capPerType);
        this.reg = r;
        this.cap = capPerType;
    }

    public int capPerType() { return cap; }

    /** Every stitching of `type` whose tree is at most `depth` levels of holes below the
     *  root (depth 0 = leaves only), subject to the cap. Order is fixed; the array is
     *  memoised and shared.
     *
     *  Recursive, and BOUNDED BY `depth`: the only recursive call is stitch(hole, depth-1),
     *  guarded by `depth <= 0`, so the call chain is at most depth+1 frames deep. depth is
     *  a command-line integer in the low single digits — the enumeration count explodes
     *  long before the stack would. */
    public Stitching[] stitch(String type, int depth) {
        if (depth < 0) depth = 0;
        String key = type + "@" + depth;
        Stitching[] hit = (Stitching[]) memo.get(key);
        if (hit != null) return hit;

        // A hole type nothing produces is a LIBRARY defect, not a coverage gap: every
        // stitching that wanted it would vanish and the run would still look complete.
        if (!reg.produces(type))
            throw new RuntimeException("Stitcher: no snippet produces type " + type);

        Snippet[] cands = reg.byType(type);
        int n = cands.length;

        // ---- pass 1: what does each snippet offer at this depth? ------------------------
        long[] offer = new long[n];
        java.util.Vector fills = new java.util.Vector();   // per snippet: null, or a Vector of Stitching[]
        long enumerable = 0;
        for (int si = 0; si < n; si++) {
            String[] hs = cands[si].holeTypes();
            if (hs.length == 0) {                    // leaf: terminates the recursion
                offer[si] = 1;
                fills.addElement(null);
            } else if (depth <= 0) {                 // holes need a level to fill them
                offer[si] = 0;
                fills.addElement(null);
            } else {
                java.util.Vector sub = new java.util.Vector();
                long product = 1;
                for (int i = 0; i < hs.length; i++) {
                    Stitching[] f = stitch(hs[i], depth - 1);
                    sub.addElement(f);
                    product = sat(product * f.length);
                }
                offer[si] = product;                 // 0 if some hole type has nothing here
                fills.addElement(sub);
            }
            enumerable = sat(enumerable + offer[si]);
        }

        // ---- pass 2: the budget ---------------------------------------------------------
        int[] quota = waterFill(offer, cap);

        // ---- pass 3: materialise --------------------------------------------------------
        java.util.Vector out = new java.util.Vector();
        java.util.Vector cutDetail = null;
        for (int si = 0; si < n; si++) {
            if (quota[si] < offer[si])
                cutDetail = note(cutDetail, cands[si].id(), quota[si], offer[si]);
            if (quota[si] == 0) continue;

            String[] hs = cands[si].holeTypes();
            if (hs.length == 0) { out.addElement(new Stitching(cands[si], new Stitching[0])); continue; }

            java.util.Vector sub = (java.util.Vector) fills.elementAt(si);
            for (int k = 0; k < quota[si]; k++) {
                Stitching[] kids = new Stitching[hs.length];
                long rem = k;
                for (int i = hs.length - 1; i >= 0; i--) {   // hole 0 slowest, last hole fastest
                    Stitching[] f = (Stitching[]) sub.elementAt(i);
                    kids[i] = f[(int) (rem % f.length)];
                    rem = rem / f.length;
                }
                out.addElement(new Stitching(cands[si], kids));
            }
        }

        if (cutDetail != null) {
            long lost = enumerable - out.size();
            cuts++;
            dropped += lost;
            drops.addElement("CAP " + type + "@" + depth + "  cap=" + cap
                             + "  enumerable=" + enumerable + (enumerable >= SAT ? "(saturated)" : "")
                             + "  kept=" + out.size() + "  dropped=" + lost);
            for (int i = 0; i < cutDetail.size(); i++)
                drops.addElement("    " + (String) cutDetail.elementAt(i));
        }

        Stitching[] res = new Stitching[out.size()];
        for (int i = 0; i < res.length; i++) res[i] = (Stitching) out.elementAt(i);
        memo.put(key, res);
        return res;
    }

    /** Stitch every registered type in registration order, concatenated. The roots of the
     *  whole corpus: "void" statement snippets included, since a statement is a case too. */
    public Stitching[] stitchAll(int depth) {
        String[] ts = reg.types();
        java.util.Vector out = new java.util.Vector();
        for (int i = 0; i < ts.length; i++) {
            Stitching[] r = stitch(ts[i], depth);
            for (int j = 0; j < r.length; j++) out.addElement(r[j]);
        }
        Stitching[] res = new Stitching[out.size()];
        for (int i = 0; i < res.length; i++) res[i] = (Stitching) out.elementAt(i);
        return res;
    }

    /** Split `budget` across the offers so that no snippet is starved by another's size:
     *  find the largest level L with sum(min(offer_i, L)) <= budget, give everyone
     *  min(offer_i, L), then hand the remainder out one at a time in registration order.
     *  A snippet offering fewer than L keeps ALL of them — the share is a ceiling, not a
     *  quota it must fill. */
    static int[] waterFill(long[] offer, int budget) {
        int n = offer.length;
        int[] quota = new int[n];

        long total = 0;
        for (int i = 0; i < n; i++) total = sat(total + offer[i]);
        if (total <= budget) {
            for (int i = 0; i < n; i++) quota[i] = (int) offer[i];
            return quota;
        }

        long lo = 0, hi = budget;
        while (lo < hi) {                                  // largest L with fill(L) <= budget
            long mid = lo + (hi - lo + 1) / 2;
            if (fill(offer, mid) <= budget) lo = mid; else hi = mid - 1;
        }
        long level = lo;
        long used = fill(offer, level);
        long rem = budget - used;
        for (int i = 0; i < n; i++) quota[i] = (int) (offer[i] < level ? offer[i] : level);
        for (int i = 0; i < n && rem > 0; i++)
            if (offer[i] > quota[i]) { quota[i]++; rem--; }
        return quota;
    }

    private static long fill(long[] offer, long level) {
        long s = 0;
        for (int i = 0; i < offer.length; i++) s = sat(s + (offer[i] < level ? offer[i] : level));
        return s;
    }

    /** Total stitchings the cap discarded across every (type, depth) it fired on. */
    public long dropCount() { return dropped; }

    /** How many (type, depth) enumerations were truncated. */
    public int cutCount() { return cuts; }

    /** The drop log, one line per cut plus indented per-snippet detail. Empty if the cap
     *  never fired — and an empty log is the claim that the enumeration IS complete. */
    public String[] dropLog() {
        String[] out = new String[drops.size()];
        for (int i = 0; i < out.length; i++) out[i] = (String) drops.elementAt(i);
        return out;
    }

    private static java.util.Vector note(java.util.Vector v, String id, long kept, long offered) {
        if (v == null) v = new java.util.Vector();
        v.addElement("kept " + kept + " of " + offered + " from " + id
                     + "  (dropped " + (offered - kept) + ")");
        return v;
    }

    private static long sat(long v) { return (v < 0 || v > SAT) ? SAT : v; }
}
