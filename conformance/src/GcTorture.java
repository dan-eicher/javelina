// GcTorture — the object-graph torture program: a GC fuzzer that is also the
// largest real Java program this toolchain runs.
//
// It builds object graphs of deliberate shapes, allocates hard enough to force
// many collections, and then walks the graphs back checking invariants that only
// hold if the collector (and the compiler feeding it) got everything right.
//
// WHAT IS OBSERVED, and what is only designed for — the distinction matters:
//
//   observed  every reachable object still carries guard == mix(id); every
//             back-link, cross-link and ring closes on the SAME object identity;
//             every constructed count is recovered exactly; the class header
//             (field 0) still names the right class; array payloads round-trip.
//             A collector that loses, duplicates, mis-sizes or fails to forward
//             an object breaks one of these BY NAME, not as a mystery crash.
//
//   observed  cross-config agreement: all four of {-O0,-O} x {interp,jit} must
//             produce identical RESULT lines. That is the differential oracle,
//             and it is what catches a MISCOMPILE rather than a collector bug.
//
//   derived   that collections happen at all. A Java program cannot see the
//             collector — there is no Runtime, no GC counter, and Stage 1 adds
//             no engine surface. Allocation volume here is far above
//             GC_INITIAL_THRESHOLD (4 blocks = 128 KiB, jav_gc.h:73); measured
//             with temporary instrumentation, a full-scale run collects 22 times.
//
//   NOT here  non-nullable references. javelinac emits only nullable ref types,
//             so no Java program can exercise one — measured, not assumed. It is
//             recorded against the stage that emits raw modules, not claimed
//             here. See conformance/README.md.
//
// Determinism is a hard requirement: one LCG, reseeded per kernel from the seed
// printed on the first line, so a failing kernel replays exactly and alone. No
// clock, no Math.random, no identity-hash iteration order.

public class GcTorture {

    // Kernel table. Index-aligned with SCALE_FULL/SCALE_QUICK; NAMES.length is
    // the loop bound and run.sh asserts the RESULT-line count against it, so a
    // kernel that silently stops running cannot pass as a green run.
    static final String[] NAMES = {
        "chain", "cycles", "cross", "fanout", "refarr",
        "hier", "los", "evac", "nulled", "simd", "survive"
    };
    static final int[] SCALE_FULL = {
        200000, 40000, 30000, 1500, 20000,
        20000, 24, 20000, 40000, 200, 0
    };
    static final int[] SCALE_QUICK = {
        20000, 4000, 3000, 200, 3000,
        3000, 6, 3000, 6000, 40, 0
    };

    static final long DEFAULT_SEED = 20260725L;

    // ── determinism ─────────────────────────────────────────────────────────
    // A 64-bit LCG (Knuth's multiplier). next() returns the top 31 bits, so it
    // is always non-negative and `% bound` needs no sign fixup.
    static long lcg;

    static void seed(long s) { lcg = s; }

    static int next() {
        lcg = lcg * 6364136223846793005L + 1442695040888963407L;
        return (int) (lcg >>> 33);
    }

    static int next(int bound) { return next() % bound; }

    // ── the per-object invariant ────────────────────────────────────────────
    // Every graph node stores mix(id) at construction and is checked against it
    // after the churn. An object that was mis-copied during evacuation, or whose
    // fields were clobbered by a neighbour's mis-sized allocation, fails here.
    static int mix(int id) {
        int h = id * 1103515245 + 12345;
        h = h ^ (h >>> 16);
        h = h * 668265263;
        h = h ^ (h >>> 15);
        return h;
    }

    // ── named failure ───────────────────────────────────────────────────────
    // The plan forbids printf spelunking: a failure says WHICH invariant broke
    // and on which object. Call sites test the condition inline and only build
    // the detail string on the failing branch, so the happy path allocates
    // nothing and the verification loops do not perturb what they measure.
    static void fail(String kernel, String invariant, String detail) {
        System.out.println("FAIL " + kernel + " invariant=" + invariant + " " + detail);
        System.out.flush();
        System.exit(1);
    }

    // ── garbage ─────────────────────────────────────────────────────────────
    // Short-lived objects in a mix of size classes: Node is ~76 bytes (small,
    // bump-allocated in holes) and the byte[] payloads span small into medium
    // (>128 bytes in-heap, which refills only from FREE blocks — immix_space.c:105).
    // One in every 128 is retained, which is what leaves blocks PARTIALLY live:
    // a partially-live block classifies RECYCLABLE (immix_block.c:48-54) and
    // recyclable blocks are exactly the next cycle's evacuation candidates.
    static Object sink;

    static void churn(int rounds) {
        for (int i = 0; i < rounds; i++) {
            Node a = new Node(i);
            a.payload = new byte[next(96) + 1];
            Node b = new Node(i + 1);
            b.next = a;
            if ((i & 127) == 0) sink = b;
        }
    }

    // ── the survivor set ────────────────────────────────────────────────────
    // Built before the kernel loop and verified by the LAST kernel, so it is
    // held live across every collection every other kernel provokes. This is the
    // "objects that outlive several collections" shape, and it is the one that
    // cannot be faked by a kernel-local graph.
    static Node[] survivors;
    static int survivorCount;

    static void buildSurvivors(int n) {
        survivors = new Node[n];
        for (int i = 0; i < n; i++) {
            Node x = new Node(i);
            x.sub = 7;
            if (i > 0) {
                x.prev = survivors[i - 1];
                survivors[i - 1].next = x;
            }
            survivors[i] = x;
        }
        survivorCount = n;
    }

    static int survive(int unused) {
        int n = survivorCount;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            Node x = survivors[i];
            if (x == null) fail("survive", "present", "i=" + i);
            if (x.guard != mix(x.id)) fail("survive", "guard", "i=" + i + " id=" + x.id);
            if (x.id != i) fail("survive", "identity", "i=" + i + " id=" + x.id);
            if (x.sub != 7) fail("survive", "tag", "i=" + i + " sub=" + x.sub);
            if (i > 0 && x.prev != survivors[i - 1]) fail("survive", "backlink", "i=" + i);
            if (i + 1 < n && x.next != survivors[i + 1]) fail("survive", "forwardlink", "i=" + i);
            sum = sum * 31 + x.id;
        }
        return sum ^ n;
    }

    static int run(int which, int scale) {
        switch (which) {
            case 0:  return Graph.chain(scale);
            case 1:  return Graph.cycles(scale);
            case 2:  return Graph.cross(scale);
            case 3:  return Graph.fanout(scale);
            case 4:  return Graph.refarr(scale);
            case 5:  return Hier.run(scale);
            case 6:  return Graph.los(scale);
            case 7:  return Graph.evac(scale);
            case 8:  return Graph.nulled(scale);
            case 9:  return Simd.run(scale);
            case 10: return survive(scale);
        }
        fail("dispatch", "kernel-index", "which=" + which);
        return -1;
    }

    public static void main(String[] args) {
        boolean quick = args.length > 0 && args[0].equals("quick");
        long s = DEFAULT_SEED;
        if (args.length > 1) s = Long.parseLong(args[1]);

        System.out.println("SEED " + s + (quick ? " quick" : " full"));

        int[] scales = quick ? SCALE_QUICK : SCALE_FULL;
        buildSurvivors(quick ? 4000 : 20000);

        for (int w = 0; w < NAMES.length; w++) {
            // Reseed per kernel so a failing kernel replays on its own: its
            // checksum does not depend on how much churn ran before it.
            seed(s + w);
            int check = run(w, scales[w]);
            System.out.println("RESULT " + NAMES[w] + " checksum=" + check);
        }
    }
}
