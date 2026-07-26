// A subtype hierarchy four levels deep, with a guard field AND a reference
// field declared at every level, plus an interface implemented at the root.
//
// Why the shape matters to the collector: an object's traced reference slots
// come from its RTT, and the RTT for H3 has to describe the fields H3 declares
// AND every field it inherited. A field map that is right for the level it was
// declared at but wrong once inherited shows up as a reference the tracer never
// visits (so its target dies while still reachable) or one it visits at the
// wrong offset. Checking every level's own fields through a virtual `check()`
// that chains up through super is what makes that observable.
//
// The class-identity check is the second half: `getClass()` reads field 0, so a
// mis-copied header shows up there first, and two instances of the same class
// must share ONE interned Class object — a gc_rtt_t lifetime bug breaks that
// before it breaks anything else.

interface Probe {
    int probe();
}

class H0 implements Probe {
    int id;
    int g0;
    Object r0;

    H0(int id) {
        this.id = id;
        this.g0 = GcTorture.mix(id);
        this.r0 = new Node(id);
    }

    boolean check() {
        if (g0 != GcTorture.mix(id)) return false;
        if (r0 == null) return false;
        return ((Node) r0).guard == GcTorture.mix(id);
    }

    public int probe() { return 1; }

    String expectName() { return "H0"; }
}

class H1 extends H0 {
    int g1;
    Object r1;

    H1(int id) {
        super(id);
        this.g1 = GcTorture.mix(id + 11);
        this.r1 = new Node(id + 11);
    }

    boolean check() {
        if (!super.check()) return false;
        if (g1 != GcTorture.mix(id + 11)) return false;
        if (r1 == null) return false;
        return ((Node) r1).guard == GcTorture.mix(id + 11);
    }

    public int probe() { return super.probe() * 3 + 2; }

    String expectName() { return "H1"; }
}

class H2 extends H1 {
    int g2;
    Object r2;

    H2(int id) {
        super(id);
        this.g2 = GcTorture.mix(id + 22);
        this.r2 = new Node(id + 22);
    }

    boolean check() {
        if (!super.check()) return false;
        if (g2 != GcTorture.mix(id + 22)) return false;
        if (r2 == null) return false;
        return ((Node) r2).guard == GcTorture.mix(id + 22);
    }

    public int probe() { return super.probe() * 5 + 3; }

    String expectName() { return "H2"; }
}

class H3 extends H2 {
    int g3;
    Object r3;

    H3(int id) {
        super(id);
        this.g3 = GcTorture.mix(id + 33);
        this.r3 = new Node(id + 33);
    }

    boolean check() {
        if (!super.check()) return false;
        if (g3 != GcTorture.mix(id + 33)) return false;
        if (r3 == null) return false;
        return ((Node) r3).guard == GcTorture.mix(id + 33);
    }

    public int probe() { return super.probe() * 7 + 4; }

    String expectName() { return "H3"; }
}

class Hier {

    static H0 make(int i) {
        int k = i & 3;
        if (k == 0) return new H0(i);
        if (k == 1) return new H1(i);
        if (k == 2) return new H2(i);
        return new H3(i);
    }

    static int run(int n) {
        H0[] a = new H0[n];
        for (int i = 0; i < n; i++) a[i] = make(i);
        // Link each through the field declared at the ROOT, so the reference
        // being traced lives at the same offset for four different RTTs.
        for (int i = 1; i < n; i++) a[i].r0 = a[i - 1];

        GcTorture.churn(n * 2);

        int sum = 0;
        for (int i = 0; i < n; i++) {
            H0 h = a[i];
            if (h == null) GcTorture.fail("hier", "present", "i=" + i);
            if (i > 0 && h.r0 != a[i - 1]) GcTorture.fail("hier", "base-ref", "i=" + i);

            // check() chains up through super, so every level's own fields are
            // tested. i==0 still owns its original Node in r0; the rest had r0
            // overwritten above, so their root-level check is the link check.
            if (i == 0 && !h.check()) GcTorture.fail("hier", "level-fields", "i=0");
            if (i > 0) {
                boolean deep = true;
                if (h instanceof H1) {
                    H1 x = (H1) h;
                    if (x.g1 != GcTorture.mix(i + 11)) deep = false;
                    if (deep && ((Node) x.r1).guard != GcTorture.mix(i + 11)) deep = false;
                }
                if (deep && h instanceof H2) {
                    H2 x = (H2) h;
                    if (x.g2 != GcTorture.mix(i + 22)) deep = false;
                    if (deep && ((Node) x.r2).guard != GcTorture.mix(i + 22)) deep = false;
                }
                if (deep && h instanceof H3) {
                    H3 x = (H3) h;
                    if (x.g3 != GcTorture.mix(i + 33)) deep = false;
                    if (deep && ((Node) x.r3).guard != GcTorture.mix(i + 33)) deep = false;
                }
                if (!deep) GcTorture.fail("hier", "level-fields", "i=" + i);
                if (h.g0 != GcTorture.mix(i)) GcTorture.fail("hier", "root-field", "i=" + i);
            }

            String cn = h.getClass().getName();
            if (!cn.equals(h.expectName())) GcTorture.fail("hier", "class-name", "i=" + i + " got=" + cn);

            // Same class => one interned Class object, for every instance.
            if (i >= 4 && h.getClass() != a[i - 4].getClass()) {
                GcTorture.fail("hier", "class-identity", "i=" + i);
            }

            // Interface dispatch over the same objects.
            Probe p = h;
            if (p.probe() != h.probe()) GcTorture.fail("hier", "interface-dispatch", "i=" + i);

            sum = sum * 31 + h.probe() + h.id;
        }
        return sum;
    }
}
