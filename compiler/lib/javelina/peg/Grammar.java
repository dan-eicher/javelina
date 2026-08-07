package javelina.peg;

import java.util.Hashtable;

// A set of named rules and a start expression, built at runtime.
//
// Opaque on purpose: the Hashtable behind `define` never appears in a
// signature, so a port swaps it for a HashMap without touching a caller.
//
// finish() is where a grammar stops being editable and starts being runnable.
// It resolves every rule reference to an index — so the machine indexes an
// array instead of hashing a string per call — and it rejects the two ways a
// grammar can be unrunnable: a reference to a rule nobody defined, and left
// recursion.
//
// Left recursion is caught HERE rather than in the machine. A PEG with a
// left-recursive rule has no proof tree for any input, so it is a defect in the
// grammar, not an input that failed; finding it once at construction beats
// paying a cycle check on every rule entry, and it means the machine never has
// to carry an active-rule stack.
public class Grammar {

    private Hashtable index;      // rule name -> Integer slot
    private String[] names;
    private Pexp[] bodies;
    private int count;
    private Pexp startExpr;
    private boolean finished;

    public Grammar() {
        index = new Hashtable();
        names = new String[8];
        bodies = new Pexp[8];
        count = 0;
        startExpr = null;
        finished = false;
    }

    public void define(String name, Pexp body) {
        if (finished) throw new PegException("grammar already finished: " + name);
        if (index.get(name) != null) throw new PegException("rule defined twice: " + name);
        if (count == names.length) {
            String[] n2 = new String[count * 2];
            Pexp[] b2 = new Pexp[count * 2];
            System.arraycopy(names, 0, n2, 0, count);
            System.arraycopy(bodies, 0, b2, 0, count);
            names = n2;
            bodies = b2;
        }
        names[count] = name;
        bodies[count] = body;
        index.put(name, new Integer(count));
        count++;
    }

    public void start(Pexp e) {
        if (finished) throw new PegException("grammar already finished");
        startExpr = e;
    }

    public void finish() {
        if (finished) return;
        if (startExpr == null) throw new PegException("grammar has no start expression");
        resolve(startExpr);
        for (int i = 0; i < count; i++) resolve(bodies[i]);
        checkLeftRecursion();
        finished = true;
    }

    // Package-private: what the machine needs, and nothing a caller can reach.
    boolean isFinished() { return finished; }
    Pexp startExpr()     { return startExpr; }
    Pexp[] ruleBodies()  { return bodies; }
    String ruleName(int id) { return names[id]; }

    // ── Reference resolution ───────────────────────────────

    private void resolve(Pexp e) {
        if (e == null) return;
        switch (e.kind) {
            case Pexp.KIND_PRULE: {
                PRule r = (PRule) e;
                Integer slot = (Integer) index.get(r.name);
                if (slot == null) throw new PegException("undefined rule: " + r.name);
                r.id = slot.intValue();
                return;
            }
            case Pexp.KIND_PSEQ: {
                Pexp[] es = ((PSeq) e).elems;
                for (int i = 0; i < es.length; i++) resolve(es[i]);
                return;
            }
            case Pexp.KIND_PCHOICE: {
                Pexp[] as = ((PChoice) e).alts;
                for (int i = 0; i < as.length; i++) resolve(as[i]);
                return;
            }
            case Pexp.KIND_PSTAR:    resolve(((PStar) e).body); return;
            case Pexp.KIND_PPLUS:    resolve(((PPlus) e).body); return;
            case Pexp.KIND_POPT:     resolve(((POpt) e).body); return;
            case Pexp.KIND_PAND:     resolve(((PAnd) e).body); return;
            case Pexp.KIND_PNOT:     resolve(((PNot) e).body); return;
            case Pexp.KIND_PACTION:  resolve(((PAction) e).body); return;
            default: return;        // terminals hold no references
        }
    }

    // ── Left recursion ─────────────────────────────────────
    //
    // A rule is left-recursive when it can reach itself without consuming.
    // Finding that needs nullability first: in `A = B A`, A is only in leftmost
    // position after B if B can match empty. Both are least fixed points, so
    // both iterate to stability.

    private boolean[] nullable;

    private void checkLeftRecursion() {
        nullable = new boolean[count];
        boolean changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < count; i++) {
                if (!nullable[i] && isNullable(bodies[i])) {
                    nullable[i] = true;
                    changed = true;
                }
            }
        }

        // reach[i][j]: rule i can enter rule j without consuming input.
        boolean[][] reach = new boolean[count][count];
        for (int i = 0; i < count; i++) leftCalls(bodies[i], reach[i]);

        changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < count; i++) {
                for (int j = 0; j < count; j++) {
                    if (!reach[i][j]) continue;
                    for (int k = 0; k < count; k++) {
                        if (reach[j][k] && !reach[i][k]) {
                            reach[i][k] = true;
                            changed = true;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < count; i++) {
            if (reach[i][i]) {
                throw new PegException("left-recursive rule: " + names[i]);
            }
        }
    }

    private boolean isNullable(Pexp e) {
        switch (e.kind) {
            case Pexp.KIND_PSEQ: {
                Pexp[] es = ((PSeq) e).elems;
                for (int i = 0; i < es.length; i++) {
                    if (!isNullable(es[i])) return false;
                }
                return true;
            }
            case Pexp.KIND_PCHOICE: {
                Pexp[] as = ((PChoice) e).alts;
                for (int i = 0; i < as.length; i++) {
                    if (isNullable(as[i])) return true;
                }
                return false;
            }
            case Pexp.KIND_PSTAR:
            case Pexp.KIND_POPT:
            case Pexp.KIND_PAND:
            case Pexp.KIND_PNOT:
            case Pexp.KIND_PTEST:
            case Pexp.KIND_PCAPSTART:
            case Pexp.KIND_PCAPEND:
                return true;                                    // zero-width
            case Pexp.KIND_PPLUS:    return isNullable(((PPlus) e).body);
            case Pexp.KIND_PACTION:  return isNullable(((PAction) e).body);
            case Pexp.KIND_PLITERAL: return ((PLiteral) e).value.length() == 0;
            case Pexp.KIND_PRULE:    return nullable[((PRule) e).id];
            default:                 return false;              // PAny, PClass
        }
    }

    private void leftCalls(Pexp e, boolean[] out) {
        switch (e.kind) {
            case Pexp.KIND_PSEQ: {
                Pexp[] es = ((PSeq) e).elems;
                for (int i = 0; i < es.length; i++) {
                    leftCalls(es[i], out);
                    if (!isNullable(es[i])) return;   // past here nothing is leftmost
                }
                return;
            }
            case Pexp.KIND_PCHOICE: {
                Pexp[] as = ((PChoice) e).alts;
                for (int i = 0; i < as.length; i++) leftCalls(as[i], out);
                return;
            }
            case Pexp.KIND_PSTAR:    leftCalls(((PStar) e).body, out); return;
            case Pexp.KIND_PPLUS:    leftCalls(((PPlus) e).body, out); return;
            case Pexp.KIND_POPT:     leftCalls(((POpt) e).body, out); return;
            case Pexp.KIND_PAND:     leftCalls(((PAnd) e).body, out); return;
            case Pexp.KIND_PNOT:     leftCalls(((PNot) e).body, out); return;
            case Pexp.KIND_PACTION:  leftCalls(((PAction) e).body, out); return;
            case Pexp.KIND_PRULE:    out[((PRule) e).id] = true; return;
            default: return;
        }
    }
}
