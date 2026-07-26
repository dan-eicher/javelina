// The SIMD leg: where v128 meets the collector.
//
// A V128 is a VALUE, not a reference — sema maps the class to the wasm v128
// width, so it can never be null and never be an Object. But `V128[]` IS a
// traced GC array, and it is the only array in the system whose elements are
// 16 bytes wide: jav_module_index.c:550 sets elem_heap_w to the element width
// when that exceeds 8, and gc_obj_size multiplies by it. So a V128[] crosses
// into the large-object space at a DIFFERENT length from every other array:
//
//   len 2006 -> 24 + 8 + 32096 = 32128  largest medium V128[]
//   len 2007 -> 24 + 8 + 32112 = 32144  first large V128[]
//
// versus the 4012/4013 step every 8-byte-stride array crosses at. Both sides of
// both boundaries are built, and what this file OBSERVES is that the contents
// survive on either side of the step.
//
// It does NOT observe the stride itself, and says so rather than implying it:
// forcing gc_obj_size to ignore elem_heap_w leaves every checksum here
// unchanged, because LOS liveness is driven by the mark epoch and not by the
// computed size. The stride is therefore pinned where breaking it DOES go red —
// wasm/test/test_gc.c, which asserts gc_obj_size against both strides and both
// of these boundary lengths. Same numbers, tested at the level that owns them.
//
// Deliberately excluded: every `relaxed_*` op. Relaxed SIMD is specified to
// permit implementation-defined results, so the interpreter and the JIT are
// both allowed to be right and disagree. Folding one into a checksum would make
// the cross-config oracle lie. Nothing here calls one.

import javelina.simd.V128;
import javelina.simd.I32x4;
import javelina.simd.Mem;

// An object with a v128 field: the struct case, where a 16-byte value sits
// between two reference slots and the field map has to get both offsets right.
class VNode {
    int id;
    int guard;
    V128 v;
    VNode next;
    Object payload;

    VNode(int id) {
        this.id = id;
        this.guard = GcTorture.mix(id);
        this.v = Simd.pattern(id);
    }
}

class Simd {

    static final int MEDIUM_MAX_LEN = 2006;
    static final int LOS_MIN_LEN = 2007;

    // A static v128 field, live for the whole run.
    static V128 held;

    // Lanes (id, id+1, id+2, id+3) — so laneSum is 4*id+6 and any lane that
    // moved, got dropped, or came back from the wrong object is visible.
    static V128 pattern(int id) {
        V128 v = I32x4.splat(id);
        v = I32x4.replace_lane(v, id + 1, 1);
        v = I32x4.replace_lane(v, id + 2, 2);
        v = I32x4.replace_lane(v, id + 3, 3);
        return v;
    }

    static int laneSum(V128 v) {
        return I32x4.extract_lane(v, 0) + I32x4.extract_lane(v, 1)
             + I32x4.extract_lane(v, 2) + I32x4.extract_lane(v, 3);
    }

    static int run(int n) {
        int sum = 0;

        held = pattern(12345);

        // ── v128 fields inside a traced object graph ────────────────────────
        VNode head = null;
        VNode[] all = new VNode[n];
        for (int i = 0; i < n; i++) {
            VNode x = new VNode(i);
            x.payload = new byte[(i & 63) + 1];
            x.next = head;
            head = x;
            all[i] = x;
        }

        GcTorture.churn(n * 16);

        for (int i = 0; i < n; i++) {
            VNode x = all[i];
            if (x.guard != GcTorture.mix(x.id)) GcTorture.fail("simd", "vnode-guard", "i=" + i);
            if (laneSum(x.v) != 4 * x.id + 6) {
                GcTorture.fail("simd", "vnode-lanes", "i=" + i + " got=" + laneSum(x.v));
            }
            if (x.payload == null) GcTorture.fail("simd", "vnode-payload", "i=" + i);
            sum = sum * 31 + laneSum(x.v);
        }
        if (laneSum(held) != 4 * 12345 + 6) GcTorture.fail("simd", "static-field", "got=" + laneSum(held));

        // ── V128[] across the 16-byte-stride LOS boundary ───────────────────
        int rounds = n / 10 + 2;
        Object[] keepMedium = new Object[rounds];
        Object[] keepLarge = new Object[rounds];

        for (int r = 0; r < rounds; r++) {
            V128[] m = new V128[MEDIUM_MAX_LEN];
            V128[] l = new V128[LOS_MIN_LEN];
            m[0] = pattern(r);
            m[MEDIUM_MAX_LEN - 1] = pattern(r + 100);
            l[0] = pattern(r + 200);
            l[LOS_MIN_LEN - 1] = pattern(r + 300);
            if ((r & 1) == 0) {
                keepMedium[r] = m;
                keepLarge[r] = l;
            }
            GcTorture.churn(32);
        }

        GcTorture.churn(rounds * 32);

        for (int r = 0; r < rounds; r += 2) {
            V128[] m = (V128[]) keepMedium[r];
            V128[] l = (V128[]) keepLarge[r];
            if (m.length != MEDIUM_MAX_LEN) GcTorture.fail("simd", "v128-medium-length", "r=" + r);
            if (l.length != LOS_MIN_LEN) GcTorture.fail("simd", "v128-large-length", "r=" + r);
            if (laneSum(m[0]) != 4 * r + 6) GcTorture.fail("simd", "v128-medium-head", "r=" + r);
            if (laneSum(m[MEDIUM_MAX_LEN - 1]) != 4 * (r + 100) + 6) {
                GcTorture.fail("simd", "v128-medium-tail", "r=" + r);
            }
            if (laneSum(l[0]) != 4 * (r + 200) + 6) GcTorture.fail("simd", "v128-large-head", "r=" + r);
            if (laneSum(l[LOS_MIN_LEN - 1]) != 4 * (r + 300) + 6) {
                GcTorture.fail("simd", "v128-large-tail", "r=" + r);
            }
            // Untouched elements default to v128.const 0, so a lane sum of
            // anything else here means something else was written into this
            // array's storage.
            if (laneSum(m[1]) != 0) GcTorture.fail("simd", "v128-medium-interior", "r=" + r);
            if (laneSum(l[1]) != 0) GcTorture.fail("simd", "v128-large-interior", "r=" + r);
            sum = sum * 31 + laneSum(m[0]) + laneSum(l[0]);
        }

        // ── the GC array <-> linear memory bounce ───────────────────────────
        // Grow first and work above the old limit: the low pages carry the
        // runtime's host-I/O staging traffic, and memory_grow returns the OLD
        // page count, so `old * 65536` is the first byte nothing else uses.
        int bounceLen = 256;
        int old = Mem.memory_grow(2);
        if (old < 0) GcTorture.fail("simd", "memory-grow", "grow returned -1");
        int base = old * 65536;

        V128[] src = new V128[bounceLen];
        for (int i = 0; i < bounceLen; i++) src[i] = pattern(i + 7);
        Mem.copyIn(src, 0, bounceLen, base);

        GcTorture.churn(n * 16);

        V128[] dst = new V128[bounceLen];
        Mem.copyOut(base, dst, 0, bounceLen);
        for (int i = 0; i < bounceLen; i++) {
            if (laneSum(dst[i]) != 4 * (i + 7) + 6) {
                GcTorture.fail("simd", "bounce-roundtrip", "i=" + i + " got=" + laneSum(dst[i]));
            }
            sum = sum * 31 + laneSum(dst[i]);
        }

        // The bounce must not have disturbed the source array either — it is a
        // GC array that stayed live across the churn above.
        for (int i = 0; i < bounceLen; i++) {
            if (laneSum(src[i]) != 4 * (i + 7) + 6) GcTorture.fail("simd", "bounce-source", "i=" + i);
        }

        // ── the bounds guard, which throws rather than trapping ─────────────
        // A Java-visible Mem access never traps the VM: it throws the catchable
        // base IndexOutOfBoundsException BEFORE the raw op. Each throw allocates
        // the exception object, so this is also GC pressure on the throw path.
        boolean threwLow = false;
        try {
            Mem.v128_load(-16);
        } catch (IndexOutOfBoundsException e) {
            threwLow = true;
        }
        if (!threwLow) GcTorture.fail("simd", "guard-negative", "v128_load(-16) did not throw");

        boolean threwHigh = false;
        int limit = Mem.memory_size() * 65536;
        try {
            Mem.v128_load(limit - 15);          // the 16-byte span runs past the end
        } catch (IndexOutOfBoundsException e) {
            threwHigh = true;
        }
        if (!threwHigh) GcTorture.fail("simd", "guard-span", "v128_load(limit-15) did not throw");

        // ...and the exact last legal address must NOT throw: an off-by-one in
        // the guard that made it conservative would pass the two checks above
        // while silently rejecting valid programs.
        V128 edge = Mem.v128_load(limit - 16);
        sum = sum * 31 + laneSum(edge);

        return sum;
    }
}
