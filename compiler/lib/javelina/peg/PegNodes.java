package javelina.peg;

// ============================================================
// Auto-generated from ASDL — do not edit by hand.
//
// Java 1.0 dialect: no nested classes, no generics, no enums, no collections.
// Every class is top-level; an ASDL sequence is an array; a field-free sum is a
// domain of int constants; a single-constructor sum is one final class. A
// multi-constructor sum carries its constructor as a `kind` tag on the base
// class, so consumers switch on an int rather than dispatching virtually over
// every constructor.
//
// No field is declared final. JLS 1.0 section 8.3.1.2: "A field can be declared
// final, in which case its declarator must include a variable initializer or a
// compile-time error occurs." Blank finals — declared here, assigned in the
// constructor — arrived in Java 1.1. Class-level final is 1.0 and is used.
//
// Parameter lists are built with concat() rather than a literal space before
// the name: inja's lstrip_blocks eats whitespace adjacent to a block tag, and a
// swallowed space produces `int value` written as one token.
// ============================================================

public abstract class Pexp {
    public static final int KIND_PSEQ = 0;
    public static final int KIND_PCHOICE = 1;
    public static final int KIND_PSTAR = 2;
    public static final int KIND_PPLUS = 3;
    public static final int KIND_POPT = 4;
    public static final int KIND_PAND = 5;
    public static final int KIND_PNOT = 6;
    public static final int KIND_PRULE = 7;
    public static final int KIND_PANY = 8;
    public static final int KIND_PLITERAL = 9;
    public static final int KIND_PCLASS = 10;
    public static final int KIND_PTEST = 11;
    public static final int KIND_PCAPTURE = 12;
    public static final int KIND_PACTION = 13;

    public int kind;

    Pexp(int kind) {
        this.kind = kind;
    }
}

public abstract class Kont {
    public static final int KIND_KHALT = 0;
    public static final int KIND_KSEQREST = 1;
    public static final int KIND_KSTARBODY = 2;
    public static final int KIND_KPLUSREST = 3;
    public static final int KIND_KANDRESTORE = 4;
    public static final int KIND_KNOTRESTORE = 5;
    public static final int KIND_KCOMMIT = 6;
    public static final int KIND_KCAPEND = 7;
    public static final int KIND_KACTIONEND = 8;

    public int kind;

    Kont(int kind) {
        this.kind = kind;
    }
}

public abstract class Bt {
    public static final int KIND_BALT = 0;
    public static final int KIND_BRECOVER = 1;

    public int kind;

    Bt(int kind) {
        this.kind = kind;
    }
}

public final class PSeq extends Pexp {
    public Pexp[] elems;

    public PSeq(Pexp[] elems) {
        super(Pexp.KIND_PSEQ);
        this.elems = elems;
    }
}

public final class PChoice extends Pexp {
    public Pexp[] alts;

    public PChoice(Pexp[] alts) {
        super(Pexp.KIND_PCHOICE);
        this.alts = alts;
    }
}

public final class PStar extends Pexp {
    public Pexp body;

    public PStar(Pexp body) {
        super(Pexp.KIND_PSTAR);
        this.body = body;
    }
}

public final class PPlus extends Pexp {
    public Pexp body;

    public PPlus(Pexp body) {
        super(Pexp.KIND_PPLUS);
        this.body = body;
    }
}

public final class POpt extends Pexp {
    public Pexp body;

    public POpt(Pexp body) {
        super(Pexp.KIND_POPT);
        this.body = body;
    }
}

public final class PAnd extends Pexp {
    public Pexp body;

    public PAnd(Pexp body) {
        super(Pexp.KIND_PAND);
        this.body = body;
    }
}

public final class PNot extends Pexp {
    public Pexp body;

    public PNot(Pexp body) {
        super(Pexp.KIND_PNOT);
        this.body = body;
    }
}

public final class PRule extends Pexp {
    public String name;
    public int id;

    public PRule(String name, int id) {
        super(Pexp.KIND_PRULE);
        this.name = name;
        this.id = id;
    }
}

public final class PAny extends Pexp {

    public PAny() {
        super(Pexp.KIND_PANY);
    }
}

public final class PLiteral extends Pexp {
    public String value;

    public PLiteral(String value) {
        super(Pexp.KIND_PLITERAL);
        this.value = value;
    }
}

public final class PClass extends Pexp {
    public Charset set;

    public PClass(Charset set) {
        super(Pexp.KIND_PCLASS);
        this.set = set;
    }
}

public final class PTest extends Pexp {
    public PegPredicate p;

    public PTest(PegPredicate p) {
        super(Pexp.KIND_PTEST);
        this.p = p;
    }
}

public final class PCapture extends Pexp {
    public int slot;
    public Pexp body;

    public PCapture(int slot, Pexp body) {
        super(Pexp.KIND_PCAPTURE);
        this.slot = slot;
        this.body = body;
    }
}

public final class PAction extends Pexp {
    public PegAction a;
    public Pexp body;

    public PAction(PegAction a, Pexp body) {
        super(Pexp.KIND_PACTION);
        this.a = a;
        this.body = body;
    }
}

public final class KHalt extends Kont {

    public KHalt() {
        super(Kont.KIND_KHALT);
    }
}

public final class KSeqRest extends Kont {
    public Pexp[] elems;
    public int idx;
    public Kont next;

    public KSeqRest(Pexp[] elems, int idx, Kont next) {
        super(Kont.KIND_KSEQREST);
        this.elems = elems;
        this.idx = idx;
        this.next = next;
    }
}

public final class KStarBody extends Kont {
    public Pexp body;
    public Kont next;

    public KStarBody(Pexp body, Kont next) {
        super(Kont.KIND_KSTARBODY);
        this.body = body;
        this.next = next;
    }
}

public final class KPlusRest extends Kont {
    public Pexp body;
    public Kont next;

    public KPlusRest(Pexp body, Kont next) {
        super(Kont.KIND_KPLUSREST);
        this.body = body;
        this.next = next;
    }
}

public final class KAndRestore extends Kont {
    public int pos;
    public Kont next;

    public KAndRestore(int pos, Kont next) {
        super(Kont.KIND_KANDRESTORE);
        this.pos = pos;
        this.next = next;
    }
}

public final class KNotRestore extends Kont {
    public int pos;
    public Kont next;

    public KNotRestore(int pos, Kont next) {
        super(Kont.KIND_KNOTRESTORE);
        this.pos = pos;
        this.next = next;
    }
}

public final class KCommit extends Kont {
    public Kont next;

    public KCommit(Kont next) {
        super(Kont.KIND_KCOMMIT);
        this.next = next;
    }
}

public final class KCapEnd extends Kont {
    public int slot;
    public int start;
    public Kont next;

    public KCapEnd(int slot, int start, Kont next) {
        super(Kont.KIND_KCAPEND);
        this.slot = slot;
        this.start = start;
        this.next = next;
    }
}

public final class KActionEnd extends Kont {
    public PegAction a;
    public int start;
    public int vals;
    public Kont next;

    public KActionEnd(PegAction a, int start, int vals, Kont next) {
        super(Kont.KIND_KACTIONEND);
        this.a = a;
        this.start = start;
        this.vals = vals;
        this.next = next;
    }
}

public final class BAlt extends Bt {
    public Pexp[] alts;
    public int idx;
    public int pos;
    public Kont k;
    public int caps;
    public int vals;
    public Bt next;

    public BAlt(Pexp[] alts, int idx, int pos, Kont k, int caps, int vals, Bt next) {
        super(Bt.KIND_BALT);
        this.alts = alts;
        this.idx = idx;
        this.pos = pos;
        this.k = k;
        this.caps = caps;
        this.vals = vals;
        this.next = next;
    }
}

public final class BRecover extends Bt {
    public int pos;
    public Kont k;
    public int caps;
    public int vals;
    public Bt next;

    public BRecover(int pos, Kont k, int caps, int vals, Bt next) {
        super(Bt.KIND_BRECOVER);
        this.pos = pos;
        this.k = k;
        this.caps = caps;
        this.vals = vals;
        this.next = next;
    }
}

public final class Charset {
    public int[] lo;
    public int[] hi;
    public boolean negated;

    public Charset(int[] lo, int[] hi, boolean negated) {
        this.lo = lo;
        this.hi = hi;
        this.negated = negated;
    }
}

