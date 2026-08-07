package javelina.peg;

// The CEK machine: control (a parsing expression), environment (position,
// captures, values), continuation (what happens next).
//
// PEG needs two continuations rather than one, because ordered choice is
// failure-directed control flow: `k` says what to do when the current
// expression succeeds, `bt` says what to do when it fails. Both are chains of
// heap objects.
//
// That is the whole reason for the design. java.util.regex has the same node
// tree and calls a virtual `match` on each node, recursing on the Java stack —
// which is why a long alternation there throws StackOverflowError. Here the
// continuation is data, so the loop below is flat and the stack depth does not
// depend on the pattern or the input. `peg_machine_stack_is_o1` is the pin.
//
// An earlier version of this design justified the flat loop by wasm tail calls.
// That was imprecise: what makes the stack O(1) is putting the continuation in
// the heap. Once it is there, a plain loop is equivalent to a self tail call
// and simpler, so this is a loop.
public class PegMachine {

    private static final int MAX_EXPECTED = 8;

    private Grammar grammar;
    private Pexp[] ruleBody;

    private String input;
    private int end;
    private int pos;

    private Kont k;
    private Bt bt;

    // Captures, with a trail so a failed alternative leaves none behind.
    private Span[] caps;
    private int[] trailSlot;
    private int[] trailStart;
    private int[] trailLen_;      // saved len per trail entry
    private int trailN;

    // Action results, likewise trimmed on backtrack.
    private Object[] vals;
    private int valN;

    private int failPos;
    private String[] expected;
    private int expectedN;

    public PegMachine(Grammar grammar) {
        if (!grammar.isFinished()) grammar.finish();
        this.grammar = grammar;
        this.ruleBody = grammar.ruleBodies();
        this.caps = new Span[8];
        this.trailSlot = new int[16];
        this.trailStart = new int[16];
        this.trailLen_ = new int[16];
        this.vals = new Object[16];
        this.expected = new String[MAX_EXPECTED];
    }

    public PegResult run(String input) {
        return run(input, 0);
    }

    public PegResult run(String input, int start) {
        this.input = input;
        this.end = input.length();
        this.pos = start;
        this.k = new KHalt();
        this.bt = null;
        this.trailN = 0;
        this.valN = 0;
        this.failPos = start;
        this.expectedN = 0;
        for (int i = 0; i < caps.length; i++) caps[i] = null;

        Pexp c = grammar.startExpr();
        boolean failing = false;

        for (;;) {
            if (failing) {
                if (bt == null) return failure();
                if (bt.kind == Bt.KIND_BALT) {
                    BAlt ba = (BAlt) bt;
                    restore(ba.pos, ba.caps, ba.vals);
                    c = ba.alts[ba.idx];
                    if (ba.idx + 1 < ba.alts.length) {
                        bt = new BAlt(ba.alts, ba.idx + 1, ba.pos, ba.k,
                                      ba.caps, ba.vals, ba.next);
                        k = new KCommit(ba.k);
                    } else {
                        // The last alternative owns the choice: nothing left to
                        // fall back to, so its failures propagate outward.
                        bt = ba.next;
                        k = ba.k;
                    }
                } else {
                    BRecover br = (BRecover) bt;
                    restore(br.pos, br.caps, br.vals);
                    bt = br.next;
                    k = br.k;
                    c = null;                      // failure became success
                }
                failing = false;
                continue;
            }

            if (c == null) {
                // Apply the continuation.
                switch (k.kind) {
                    case Kont.KIND_KHALT:
                        return success();

                    case Kont.KIND_KSEQREST: {
                        KSeqRest sr = (KSeqRest) k;
                        if (sr.idx >= sr.elems.length) {
                            k = sr.next;
                        } else {
                            k = new KSeqRest(sr.elems, sr.idx + 1, sr.next);
                            c = sr.elems[sr.idx];
                        }
                        break;
                    }

                    case Kont.KIND_KSTARBODY: {
                        KStarBody sb = (KStarBody) k;
                        BRecover br = (BRecover) bt;
                        if (pos == br.pos) {
                            // The body matched empty. Looping again would never
                            // terminate, so stop here and keep what we have.
                            bt = br.next;
                            k = sb.next;
                        } else {
                            bt = new BRecover(pos, sb.next, trailN, valN, br.next);
                            c = sb.body;
                        }
                        break;
                    }

                    case Kont.KIND_KPLUSREST: {
                        KPlusRest pr = (KPlusRest) k;
                        bt = new BRecover(pos, pr.next, trailN, valN, bt);
                        k = new KStarBody(pr.body, pr.next);
                        c = pr.body;
                        break;
                    }

                    case Kont.KIND_KANDRESTORE: {
                        KAndRestore ar = (KAndRestore) k;
                        pos = ar.pos;
                        k = ar.next;
                        break;
                    }

                    case Kont.KIND_KNOTRESTORE: {
                        KNotRestore nr = (KNotRestore) k;
                        bt = bt.kind == Bt.KIND_BALT ? ((BAlt) bt).next : ((BRecover) bt).next;
                        pos = nr.pos;
                        failing = true;
                        break;
                    }

                    case Kont.KIND_KCOMMIT: {
                        KCommit kc = (KCommit) k;
                        bt = bt.kind == Bt.KIND_BALT ? ((BAlt) bt).next : ((BRecover) bt).next;
                        k = kc.next;
                        break;
                    }

                    case Kont.KIND_KACTIONEND: {
                        KActionEnd ae = (KActionEnd) k;
                        int n = valN - ae.vals;
                        Object[] parts = new Object[n];
                        System.arraycopy(vals, ae.vals, parts, 0, n);
                        valN = ae.vals;
                        pushValue(ae.a.act(input, ae.start, pos, parts));
                        k = ae.next;
                        break;
                    }
                }
                continue;
            }

            // Evaluate the control expression.
            switch (c.kind) {
                case Pexp.KIND_PSEQ: {
                    Pexp[] es = ((PSeq) c).elems;
                    if (es.length == 0) { c = null; break; }
                    k = new KSeqRest(es, 1, k);
                    c = es[0];
                    break;
                }

                case Pexp.KIND_PCHOICE: {
                    Pexp[] as = ((PChoice) c).alts;
                    if (as.length == 0) { failing = true; break; }
                    if (as.length > 1) {
                        bt = new BAlt(as, 1, pos, k, trailN, valN, bt);
                        k = new KCommit(k);
                    }
                    c = as[0];
                    break;
                }

                case Pexp.KIND_PSTAR: {
                    Pexp body = ((PStar) c).body;
                    bt = new BRecover(pos, k, trailN, valN, bt);
                    k = new KStarBody(body, k);
                    c = body;
                    break;
                }

                case Pexp.KIND_PPLUS: {
                    Pexp body = ((PPlus) c).body;
                    k = new KPlusRest(body, k);
                    c = body;
                    break;
                }

                case Pexp.KIND_POPT: {
                    Pexp body = ((POpt) c).body;
                    bt = new BRecover(pos, k, trailN, valN, bt);
                    k = new KCommit(k);
                    c = body;
                    break;
                }

                case Pexp.KIND_PAND: {
                    k = new KAndRestore(pos, k);
                    c = ((PAnd) c).body;
                    break;
                }

                case Pexp.KIND_PNOT: {
                    Pexp body = ((PNot) c).body;
                    bt = new BRecover(pos, k, trailN, valN, bt);
                    k = new KNotRestore(pos, k);
                    c = body;
                    break;
                }

                case Pexp.KIND_PRULE:
                    c = ruleBody[((PRule) c).id];
                    break;

                case Pexp.KIND_PANY:
                    if (pos < end) { pos++; c = null; }
                    else { noteFail("any character"); failing = true; }
                    break;

                case Pexp.KIND_PLITERAL: {
                    String v = ((PLiteral) c).value;
                    int n = v.length();
                    if (pos + n <= end && input.regionMatches(pos, v, 0, n)) {
                        pos += n;
                        c = null;
                    } else {
                        noteFail("\"" + v + "\"");
                        failing = true;
                    }
                    break;
                }

                case Pexp.KIND_PCLASS: {
                    Charset s = ((PClass) c).set;
                    if (pos < end && inClass(s, input.charAt(pos))) {
                        pos++;
                        c = null;
                    } else {
                        noteFail("a character in a class");
                        failing = true;
                    }
                    break;
                }

                case Pexp.KIND_PTEST:
                    if (((PTest) c).p.holds(input, pos, end)) c = null;
                    else { noteFail("a position predicate"); failing = true; }
                    break;

                // A capture opens with len -1 and closes by rewriting it. Both
                // marks go through the same trail as any other capture write,
                // so an alternative that opened a group and then failed leaves
                // nothing behind.
                case Pexp.KIND_PCAPSTART:
                    setCapture(((PCapStart) c).slot, pos, -1);
                    c = null;
                    break;

                case Pexp.KIND_PCAPEND: {
                    int slot = ((PCapEnd) c).slot;
                    Span open = slot < caps.length ? caps[slot] : null;
                    int st = open == null ? pos : open.start;
                    setCapture(slot, st, pos - st);
                    c = null;
                    break;
                }

                case Pexp.KIND_PACTION: {
                    PAction ac = (PAction) c;
                    k = new KActionEnd(ac.a, pos, valN, k);
                    c = ac.body;
                    break;
                }
            }
        }
    }

    // ── State ──────────────────────────────────────────────

    private static boolean inClass(Charset s, char ch) {
        boolean hit = false;
        for (int i = 0; i < s.lo.length; i++) {
            if (ch >= s.lo[i] && ch <= s.hi[i]) { hit = true; break; }
        }
        return s.negated ? !hit : hit;
    }

    private void restore(int p, int trail, int val) {
        pos = p;
        while (trailN > trail) {
            trailN--;
            int slot = trailSlot[trailN];
            // A negative START means the slot held nothing. Length cannot carry
            // that flag: an OPEN capture has length -1, so using it for both
            // made undoing a capture-end erase its capture-start, and any group
            // with pattern after it lost its extent.
            if (trailStart[trailN] < 0) caps[slot] = null;
            else caps[slot] = new Span(trailStart[trailN], trailLen_[trailN]);
        }
        valN = val;
    }

    private void setCapture(int slot, int start, int len) {
        if (slot >= caps.length) {
            Span[] c2 = new Span[slot * 2 + 2];
            System.arraycopy(caps, 0, c2, 0, caps.length);
            caps = c2;
        }
        if (trailN == trailSlot.length) {
            int n = trailN * 2;
            int[] a = new int[n]; System.arraycopy(trailSlot, 0, a, 0, trailN); trailSlot = a;
            int[] b = new int[n]; System.arraycopy(trailStart, 0, b, 0, trailN); trailStart = b;
            int[] d = new int[n]; System.arraycopy(trailLen_, 0, d, 0, trailN); trailLen_ = d;
        }
        Span old = caps[slot];
        trailSlot[trailN] = slot;
        trailStart[trailN] = old == null ? -1 : old.start;
        trailLen_[trailN] = old == null ? 0 : old.len;
        trailN++;
        caps[slot] = new Span(start, len);
    }

    private void pushValue(Object v) {
        if (valN == vals.length) {
            Object[] a = new Object[valN * 2];
            System.arraycopy(vals, 0, a, 0, valN);
            vals = a;
        }
        vals[valN++] = v;
    }

    // The furthest failure, which is the one worth reporting: a parse that dies
    // deep inside an alternative it almost matched says more than the position
    // where the outermost choice finally ran out.
    private void noteFail(String what) {
        if (pos > failPos) {
            failPos = pos;
            expectedN = 0;
        }
        if (pos == failPos && expectedN < MAX_EXPECTED) {
            for (int i = 0; i < expectedN; i++) {
                if (expected[i].equals(what)) return;
            }
            expected[expectedN++] = what;
        }
    }

    private PegResult success() {
        PegResult r = new PegResult();
        r.matched = true;
        r.end = pos;
        r.value = valN > 0 ? vals[valN - 1] : null;
        r.captures = new Span[caps.length];
        System.arraycopy(caps, 0, r.captures, 0, caps.length);
        r.failPos = failPos;
        r.expected = expectedList();
        return r;
    }

    private PegResult failure() {
        PegResult r = new PegResult();
        r.matched = false;
        r.end = failPos;
        r.failPos = failPos;
        r.expected = expectedList();
        return r;
    }

    private String[] expectedList() {
        String[] e = new String[expectedN];
        System.arraycopy(expected, 0, e, 0, expectedN);
        return e;
    }
}
