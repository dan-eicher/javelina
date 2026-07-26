// The graph-shape kernels. Each returns an int checksum folded from the data it
// walked, and calls GcTorture.fail() by invariant name the moment a structural
// property breaks. Both matter: the checksum catches a value-level divergence
// between two configs, the named invariants say what broke when the graph itself
// is wrong.

class Graph {

    // ── deep chains ─────────────────────────────────────────────────────────
    // A single chain n long. The collector traces this with an explicit worklist
    // (bbq_vec_push in gc_mark1) rather than recursion, so depth is a load test
    // on the worklist rather than the C stack. Verified by walking forward while
    // checking that each node's `prev` is the node we just came from — the pure
    // identity check that a missed forwarding-pointer update breaks.
    static int chain(int n) {
        Node head = null;
        for (int i = 0; i < n; i++) {
            Node x = new Node(i);
            x.next = head;
            if (head != null) head.prev = x;
            head = x;
        }

        GcTorture.churn(n / 4);

        int count = 0;
        int sum = 0;
        Node p = head;
        Node last = null;
        while (p != null) {
            if (p.guard != GcTorture.mix(p.id)) GcTorture.fail("chain", "guard", "id=" + p.id);
            if (last != null && p.prev != last) GcTorture.fail("chain", "backlink", "id=" + p.id);
            sum = sum * 31 + p.id;
            count++;
            last = p;
            p = p.next;
        }
        if (count != n) GcTorture.fail("chain", "count", "count=" + count + " want=" + n);
        return sum ^ count;
    }

    // ── cycles ──────────────────────────────────────────────────────────────
    // Self-cycle, 2-cycle and a ring n long. A cycle is the shape that separates
    // a real tracer from a naive one: the epoch guard in gc_mark1 is what stops
    // it looping, and a ring that comes back to a DIFFERENT object than it
    // started from is a forwarding bug that no reachability count would show.
    static int cycles(int n) {
        Node self = new Node(1);
        self.next = self;

        Node a = new Node(2);
        Node b = new Node(3);
        a.next = b;
        b.next = a;

        Node head = new Node(0);
        Node p = head;
        for (int i = 1; i < n; i++) {
            Node x = new Node(i);
            p.next = x;
            p = x;
        }
        p.next = head;                       // close the ring: length n

        GcTorture.churn(n / 2);

        if (self.next != self) GcTorture.fail("cycles", "self-cycle", "");
        if (self.guard != GcTorture.mix(1)) GcTorture.fail("cycles", "self-guard", "");
        if (a.next != b) GcTorture.fail("cycles", "two-cycle-fwd", "");
        if (b.next != a) GcTorture.fail("cycles", "two-cycle-back", "");

        int sum = 0;
        Node q = head;
        for (int i = 0; i < n; i++) {
            if (q.guard != GcTorture.mix(q.id)) GcTorture.fail("cycles", "ring-guard", "id=" + q.id);
            sum = sum * 31 + q.id;
            q = q.next;
        }
        if (q != head) GcTorture.fail("cycles", "ring-closes", "n steps did not return to head");

        // The other half of the falsifier: n-1 steps must NOT be back at the
        // head. Without this, a ring that got SHORTER would still pass the
        // check above on some other lap.
        Node r = head;
        for (int i = 0; i < n - 1; i++) r = r.next;
        if (r == head) GcTorture.fail("cycles", "ring-length", "n-1 steps returned to head");

        return sum;
    }

    // ── cross-links between subgraphs ───────────────────────────────────────
    // Two independently built subgraphs, tagged 1 and 2, wired to each other
    // every 4th node. Following a cross link must land in the OTHER subgraph, on
    // the expected id, and following it twice must return to the exact object we
    // started from.
    static int cross(int n) {
        Node[] u = new Node[n];
        Node[] v = new Node[n];
        for (int i = 0; i < n; i++) {
            u[i] = new Node(i);
            u[i].sub = 1;
        }
        for (int i = 0; i < n; i++) {
            v[i] = new Node(i);
            v[i].sub = 2;
        }
        for (int i = 0; i < n; i += 4) {
            u[i].cross = v[n - 1 - i];
            v[n - 1 - i].cross = u[i];
        }

        GcTorture.churn(n / 2);

        int sum = 0;
        for (int i = 0; i < n; i += 4) {
            Node x = u[i];
            Node y = x.cross;
            if (y == null) GcTorture.fail("cross", "link-present", "i=" + i);
            if (y.sub != 2) GcTorture.fail("cross", "link-side", "i=" + i + " sub=" + y.sub);
            if (y.id != n - 1 - i) GcTorture.fail("cross", "link-id", "i=" + i + " got=" + y.id);
            if (y.guard != GcTorture.mix(y.id)) GcTorture.fail("cross", "link-guard", "i=" + i);
            if (y.cross != x) GcTorture.fail("cross", "round-trip", "i=" + i);
            sum = sum * 31 + y.id;
        }
        return sum;
    }

    // ── wide fan-out ────────────────────────────────────────────────────────
    // A root holding w children, each holding 8 grandchildren, each holding 2
    // leaves: 25*w objects reachable from one root through arrays-of-refs. The
    // recovered count is exact, so a tracer that drops a slot shows up as a
    // count mismatch rather than a silently smaller graph.
    static int fanout(int w) {
        Node root = new Node(0);
        root.kids = new Node[w];
        for (int i = 0; i < w; i++) {
            Node k = new Node(i + 1);
            k.kids = new Node[8];
            for (int j = 0; j < 8; j++) {
                Node g = new Node((i + 1) * 100 + j);
                g.kids = new Node[2];
                g.kids[0] = new Node(g.id * 3);
                g.kids[1] = new Node(g.id * 3 + 1);
                k.kids[j] = g;
            }
            root.kids[i] = k;
        }

        GcTorture.churn(w * 4);

        int sum = 0;
        int count = 0;
        for (int i = 0; i < w; i++) {
            Node k = root.kids[i];
            if (k == null) GcTorture.fail("fanout", "kid-present", "i=" + i);
            if (k.guard != GcTorture.mix(k.id)) GcTorture.fail("fanout", "kid-guard", "i=" + i);
            count++;
            for (int j = 0; j < 8; j++) {
                Node g = k.kids[j];
                if (g == null) GcTorture.fail("fanout", "grandkid-present", "i=" + i + " j=" + j);
                if (g.guard != GcTorture.mix(g.id)) GcTorture.fail("fanout", "grandkid-guard", "i=" + i + " j=" + j);
                count++;
                for (int m = 0; m < 2; m++) {
                    Node gg = g.kids[m];
                    if (gg == null) GcTorture.fail("fanout", "leaf-present", "i=" + i + " j=" + j + " m=" + m);
                    if (gg.guard != GcTorture.mix(gg.id)) GcTorture.fail("fanout", "leaf-guard", "id=" + gg.id);
                    sum = sum * 31 + gg.id;
                    count++;
                }
            }
        }
        if (count != w * 25) GcTorture.fail("fanout", "count", "count=" + count + " want=" + (w * 25));
        return sum ^ count;
    }

    // ── arrays of refs, and the §10.10 store check ──────────────────────────
    // A heterogeneous Object[]: nodes, byte[] payloads, hierarchy instances and
    // nested Object[]s, walked back through instanceof and casts.
    //
    // The covariant-array store check is here on purpose. It is a §10.10 rule
    // that the OPTIMIZER can wrongly discharge — this harness's sibling gate
    // already caught exactly that (a Click strong-update on array elements
    // devirtualized on a false singleton). A dropped ArrayStoreException is a
    // heap-corrupting miscompile: it lets a B land in a C[] where every later
    // reader will read it as a C.
    static int refarr(int n) {
        Object[] a = new Object[n];
        for (int i = 0; i < n; i++) {
            int k = i & 3;
            if (k == 0) {
                a[i] = new Node(i);
            } else if (k == 1) {
                byte[] b = new byte[(i & 63) + 1];
                for (int j = 0; j < b.length; j++) b[j] = (byte) (i + j);
                a[i] = b;
            } else if (k == 2) {
                a[i] = Hier.make(i);
            } else {
                Object[] pair = new Object[2];
                pair[0] = new Node(i);
                pair[1] = new Node(i + 1);
                a[i] = pair;
            }
        }

        GcTorture.churn(n * 2);

        int sum = 0;
        for (int i = 0; i < n; i++) {
            Object o = a[i];
            if (o == null) GcTorture.fail("refarr", "slot-present", "i=" + i);
            int k = i & 3;
            if (k == 0) {
                if (!(o instanceof Node)) GcTorture.fail("refarr", "slot-type-node", "i=" + i);
                Node x = (Node) o;
                if (x.guard != GcTorture.mix(x.id)) GcTorture.fail("refarr", "node-guard", "i=" + i);
                sum = sum * 31 + x.id;
            } else if (k == 1) {
                if (!(o instanceof byte[])) GcTorture.fail("refarr", "slot-type-bytes", "i=" + i);
                byte[] b = (byte[]) o;
                if (b.length != (i & 63) + 1) GcTorture.fail("refarr", "bytes-length", "i=" + i + " len=" + b.length);
                for (int j = 0; j < b.length; j++) {
                    if (b[j] != (byte) (i + j)) GcTorture.fail("refarr", "bytes-payload", "i=" + i + " j=" + j);
                }
                sum = sum * 31 + b.length;
            } else if (k == 2) {
                if (!(o instanceof H0)) GcTorture.fail("refarr", "slot-type-hier", "i=" + i);
                H0 h = (H0) o;
                if (!h.check()) GcTorture.fail("refarr", "hier-fields", "i=" + i);
                sum = sum * 31 + h.probe();
            } else {
                if (!(o instanceof Object[])) GcTorture.fail("refarr", "slot-type-array", "i=" + i);
                Object[] pair = (Object[]) o;
                if (pair.length != 2) GcTorture.fail("refarr", "pair-length", "i=" + i);
                Node p0 = (Node) pair[0];
                Node p1 = (Node) pair[1];
                if (p0.guard != GcTorture.mix(i)) GcTorture.fail("refarr", "pair-guard-0", "i=" + i);
                if (p1.guard != GcTorture.mix(i + 1)) GcTorture.fail("refarr", "pair-guard-1", "i=" + i);
                sum = sum * 31 + p0.id + p1.id;
            }
        }

        // §10.10: storing an H1 into an H2[] viewed as H0[] must throw.
        H2[] deep = new H2[4];
        H0[] view = deep;
        boolean threw = false;
        try {
            view[0] = new H1(7);
        } catch (ArrayStoreException e) {
            threw = true;
        }
        if (!threw) GcTorture.fail("refarr", "arraystore-throws", "H1 stored into H2[] without throwing");
        if (deep[0] != null) GcTorture.fail("refarr", "arraystore-no-write", "the rejected store still landed");

        // ...and the legal store through the same alias must still work.
        H2 ok = new H2(9);
        view[1] = ok;
        if (deep[1] != ok) GcTorture.fail("refarr", "arraystore-legal", "a legal covariant store was lost");
        sum = sum * 31 + deep[1].probe();

        return sum;
    }

    // ── the LOS boundary, straddled exactly ─────────────────────────────────
    // gc_obj_size is 24 + 8 + len*w (jav_gc.c:66-73) and IMX_MEDIUM_MAX is
    // 32768-640 = 32128 (immix_space.h:22). Every non-v128 array stores 8 bytes
    // per element in the heap regardless of its Java element type
    // (jav_module_index.c:550), so:
    //
    //   len 4012 -> 24 + 8 + 32096 = 32128  the LARGEST medium object there is,
    //                                       filling one block's data area exactly
    //   len 4013 -> 24 + 8 + 32104 = 32136  the FIRST large object: its own
    //                                       malloc, never evacuated, marked in
    //                                       place, swept by los_sweep
    //
    // Both sides of that one-element step are built here every round. The large
    // one is also given a byte payload that is checked after the churn, because
    // LOS memory is plain malloc/free (jav_gc.c:49,131) and a sweep that frees a
    // live entry shows up as payload damage, not as a missing object.
    static final int MEDIUM_MAX_LEN = 4012;
    static final int LOS_MIN_LEN = 4013;

    static int los(int rounds) {
        Object[] keepMedium = new Object[rounds];
        Object[] keepLarge = new Object[rounds];
        Object[] keepBytes = new Object[rounds];

        for (int r = 0; r < rounds; r++) {
            Object[] m = new Object[MEDIUM_MAX_LEN];
            Object[] l = new Object[LOS_MIN_LEN];
            m[0] = new Node(r);
            m[MEDIUM_MAX_LEN - 1] = new Node(r + 1000);
            l[0] = new Node(r + 2000);
            l[LOS_MIN_LEN - 1] = new Node(r + 3000);

            byte[] big = new byte[LOS_MIN_LEN];
            for (int j = 0; j < LOS_MIN_LEN; j++) big[j] = (byte) (j * 7 + r);

            // Sparse retention: the odd rounds die immediately, so LOS entries
            // are both swept and retained within one run.
            if ((r & 1) == 0) {
                keepMedium[r] = m;
                keepLarge[r] = l;
                keepBytes[r] = big;
            }
            GcTorture.churn(64);
        }

        GcTorture.churn(rounds * 32);

        int sum = 0;
        for (int r = 0; r < rounds; r += 2) {
            Object[] m = (Object[]) keepMedium[r];
            Object[] l = (Object[]) keepLarge[r];
            byte[] big = (byte[]) keepBytes[r];

            if (m.length != MEDIUM_MAX_LEN) GcTorture.fail("los", "medium-length", "r=" + r + " len=" + m.length);
            if (l.length != LOS_MIN_LEN) GcTorture.fail("los", "large-length", "r=" + r + " len=" + l.length);
            if (big.length != LOS_MIN_LEN) GcTorture.fail("los", "bytes-length", "r=" + r + " len=" + big.length);

            Node m0 = (Node) m[0];
            Node mz = (Node) m[MEDIUM_MAX_LEN - 1];
            Node l0 = (Node) l[0];
            Node lz = (Node) l[LOS_MIN_LEN - 1];
            if (m0.guard != GcTorture.mix(r)) GcTorture.fail("los", "medium-head", "r=" + r);
            if (mz.guard != GcTorture.mix(r + 1000)) GcTorture.fail("los", "medium-tail", "r=" + r);
            if (l0.guard != GcTorture.mix(r + 2000)) GcTorture.fail("los", "large-head", "r=" + r);
            if (lz.guard != GcTorture.mix(r + 3000)) GcTorture.fail("los", "large-tail", "r=" + r);

            // An interior slot never written must still read null: a tracer that
            // walked past an object's end, or sized it wrong, writes here.
            if (m[1] != null) GcTorture.fail("los", "medium-interior", "r=" + r);
            if (l[1] != null) GcTorture.fail("los", "large-interior", "r=" + r);

            for (int j = 0; j < LOS_MIN_LEN; j++) {
                if (big[j] != (byte) (j * 7 + r)) GcTorture.fail("los", "large-payload", "r=" + r + " j=" + j);
            }

            sum = sum * 31 + m0.id + mz.id + l0.id + lz.id;
        }
        return sum;
    }

    // ── mixed lifetimes, forcing evacuation ─────────────────────────────────
    // Several rounds, each allocating far more than the collection threshold,
    // each retaining a SPARSE subset while whole runs die: the survivors end up
    // scattered across partially-live blocks, which is the fragmentation that
    // makes those blocks evacuation candidates.
    //
    // This kernel is why the collector moves at all. Written first, it showed
    // 22 collections, 176 evacuation attempts and ZERO objects moved, because
    // the headroom Immix §3.2 requires ("a small number of free blocks that it
    // never returns to the global allocator and only ever uses for evacuating")
    // was computed but never set aside. Blocks are now reserved at reclaim,
    // and candidates are chosen from statistics recorded at the previous sweep
    // rather than from a free list the mutator has already drained.
    //
    // The check is that every sparse survivor comes through intact and still
    // linked BY IDENTITY to its neighbours — which is exactly what breaks if a
    // moved object's referrers are not updated. Deliberately breaking that
    // update in gc_mark1 now fails this corpus; before the fix it changed
    // nothing at all, because nothing ever moved.
    static int evac(int n) {
        int keepLen = n / 8 + 2;
        Node[] keep = new Node[keepLen];
        int kept = 0;

        for (int round = 0; round < 4; round++) {
            Node prevKept = kept > 0 ? keep[kept - 1] : null;
            for (int i = 0; i < n; i++) {
                Node x = new Node(round * n + i);
                x.payload = new byte[(i & 31) + 1];
                if ((i & 7) == 0 && kept < keepLen) {
                    x.prev = prevKept;
                    if (prevKept != null) prevKept.next = x;
                    prevKept = x;
                    keep[kept] = x;
                    kept++;
                }
            }
            GcTorture.churn(n);
        }

        int sum = 0;
        for (int i = 0; i < kept; i++) {
            Node x = keep[i];
            if (x == null) GcTorture.fail("evac", "present", "i=" + i);
            if (x.guard != GcTorture.mix(x.id)) GcTorture.fail("evac", "guard", "i=" + i + " id=" + x.id);
            if (x.payload == null) GcTorture.fail("evac", "payload-present", "i=" + i);
            if (i > 0 && x.prev != keep[i - 1]) GcTorture.fail("evac", "backlink", "i=" + i);
            if (i + 1 < kept && x.next != keep[i + 1]) GcTorture.fail("evac", "forwardlink", "i=" + i);
            sum = sum * 31 + x.id;
        }
        if (kept == 0) GcTorture.fail("evac", "retained-any", "nothing was retained");
        return sum ^ kept;
    }

    // ── fields nulled between collections ───────────────────────────────────
    // Immix here has no write barrier, so dropping a reference and re-storing a
    // fresh one is purely a mark-phase question: the tracer must see the CURRENT
    // field value at the moment it runs. Half the payloads are nulled, then a
    // third of those are re-stored with a fresh object after more churn. All
    // three populations are checked: still-set, nulled-and-stayed-null, and
    // nulled-then-replaced.
    static int nulled(int n) {
        Node[] a = new Node[n];
        for (int i = 0; i < n; i++) {
            Node x = new Node(i);
            x.payload = new Node(i + 500000);
            a[i] = x;
        }

        GcTorture.churn(n);

        for (int i = 0; i < n; i += 2) a[i].payload = null;

        GcTorture.churn(n * 2);

        for (int i = 0; i < n; i += 6) a[i].payload = new Node(i + 900000);

        GcTorture.churn(n * 2);

        int sum = 0;
        for (int i = 0; i < n; i++) {
            Node x = a[i];
            if (x.guard != GcTorture.mix(i)) GcTorture.fail("nulled", "owner-guard", "i=" + i);
            if ((i & 1) == 1) {
                if (x.payload == null) GcTorture.fail("nulled", "kept-payload", "i=" + i);
                Node p = (Node) x.payload;
                if (p.guard != GcTorture.mix(i + 500000)) GcTorture.fail("nulled", "kept-guard", "i=" + i);
                sum = sum * 31 + p.id;
            } else if (i % 6 == 0) {
                if (x.payload == null) GcTorture.fail("nulled", "restored-payload", "i=" + i);
                Node p = (Node) x.payload;
                if (p.guard != GcTorture.mix(i + 900000)) GcTorture.fail("nulled", "restored-guard", "i=" + i);
                sum = sum * 31 + p.id;
            } else {
                if (x.payload != null) GcTorture.fail("nulled", "stayed-null", "i=" + i);
                sum = sum * 31 + i;
            }
        }
        return sum;
    }
}
