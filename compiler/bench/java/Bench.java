// Bench.java — the optimization-level benchmark suite, pure Java 1.0.
//
// Purpose: MEASURED costs, not felt ones. The matrix is the compiler's two real knobs —
// javelinac -O (Click: SCCP+GVN+DCE+PEA, slot repack) off/on × javelina -nojit/-jit — and the
// numbers also price BACKEND decisions (burg rule costs, stencil shapes): each workload leans on
// a distinct tree-shape family, so a codegen change moves the bench that covers it and leaves the
// rest still, which is what makes a delta attributable.
//
// Rules of construction:
//   - every workload returns a CHECKSUM accumulated through the whole computation, so DCE cannot
//     delete the work and a wrong answer invalidates the timing (the four configs must agree);
//   - `scale` arrives at runtime through the export, so nothing constant-folds away;
//   - no library calls — the measurement is the generated code, not the jre — EXCEPT the s*
//     string family below, whose point IS the jre: it prices the compiled §20.12/§20.13/§20.7
//     char-loop surface, the baseline any host-string design (e.g. the standardized wasm
//     js-string builtin namespace) has to beat, and the burg cost surface for the i16-array
//     load/store + bounds-guard + per-char-call tree shapes.
//
// What each workload covers:
//   arith  scalar int loop: mul/add/xor/shifts — SCCP/GVN food; burg's shift/mask/imm fusion
//   lmix   long arithmetic: i64 adds/muls/shifts — the i64 rule family, register-pair costs
//   sieve  boolean[] flags: packed i8 stores/loads, bounds checks, inner strided loop
//   fib    naive recursion: call/return overhead, frame machinery (inlining's future yardstick)
//   mat    n×n int matmul, 1-D indexed a[i*n+k]: burg's address-arithmetic folding, loop nests
//   alloc  object churn, 1-in-8 escape: allocator + GC + PEA sensitivity (escaping mix defeats
//          whole-bench scalarization while leaving PEA meat)
//   virt   rotating 3-class virtual dispatch: vtable-load + indirect-call cost, honest megamorphic
//   sdot   V128[] dot product (i32x4 mul/add, lane extracts): the inline v128 codegen path
//   dot    sdot's SCALAR TWIN over int[] — identical inputs and checksum; main() FAILS the
//          run if the two disagree, so the vector path is refereed against scalar Java
//          in every config (E8.6's referee), and the time ratio prices what simd buys
//   memv   v128 16-byte round-trips through Mem (store/load, D5 bounds guards included)
//   memb   byte round-trips through Mem — the per-access guard-cost yardstick (the number
//          guard-elimination work would move)
//   sbuild StringBuffer append growth + toString: ensureCapacity doubling, arraycopy, the
//          `"…" + x` workhorse shape
//   sconcat String.concat chains: alloc + two bulk copies per op, no growth machinery
//   scmp   equals/compareTo/equalsIgnoreCase over shared-prefix same-length strings — the
//          char-compare loops a host `equals`/`compare` builtin would collapse to memcmp
//   shash  hashCode (uncached 31·h+c loops) — the host `hash` builtin's twin
//   sidx   indexOf(char)/indexOf(String)/lastIndexOf scans over a 64-char haystack
//   ssub   substring (copying) + startsWith/endsWith prefix compares
//   scase  toLowerCase/toUpperCase/replace/trim — per-char transforms through the
//          GENERATED Character if-trees (§20.5)
//   schars toCharArray/getChars/new String(char[]) — the bulk-copy family a host
//          fromCharCodeArray/copyToCharCodeArray builtin pair would replace
//   sconv  Integer.toString / Integer.parseInt round-trips + Long.toString — the
//          digit-loop family (stays compiled under any host-string design)
//   (String.intern is deliberately NOT benched: its table persists across reps, so its
//   timing drifts with table state — a lookup benchmark, not a string benchmark.)
import javelina.simd.*;

public class Bench {

    static int arith(int n) {
        int a = 1, b = 7;
        for (int i = 0; i < n; i++) {
            a = a * 31 + b;
            b = (b ^ a) + (a >>> 3) - (b << 1);
        }
        return a ^ b;
    }

    static int lmix(int n) {
        long a = 0x9E3779B97F4A7C15L, b = 3;
        for (int i = 0; i < n; i++) {
            a = a * 6364136223846793005L + b;
            b = (b << 13) ^ (a >>> 7) ^ (b >> 17);
        }
        return (int) (a ^ (a >>> 32) ^ b);
    }

    static int sieve(int n) {
        boolean[] c = new boolean[n];
        int cnt = 0;
        for (int p = 2; p < n; p++) {
            if (!c[p]) {
                cnt++;
                for (int m = p + p; m < n; m += p) c[m] = true;
            }
        }
        return cnt;
    }

    static int fib(int k) {
        return k < 2 ? k : fib(k - 1) + fib(k - 2);
    }

    static int mat(int n) {
        int[] a = new int[n * n], b = new int[n * n], c = new int[n * n];
        for (int i = 0; i < n * n; i++) { a[i] = i; b[i] = i + 1; }
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                int s = 0;
                for (int k = 0; k < n; k++) s += a[i * n + k] * b[k * n + j];
                c[i * n + j] = s;
            }
        int h = 0;
        for (int i = 0; i < n * n; i++) h = h * 31 + c[i];
        return h;
    }

    static int alloc(int n) {
        Node head = null;
        int h = 0;
        for (int i = 0; i < n; i++) {
            Node x = new Node(i);
            if ((i & 7) == 0) { x.next = head; head = x; }   // 1-in-8 escapes to the list
            h += x.v;
        }
        while (head != null) { h = h * 31 + head.v; head = head.next; }
        return h;
    }

    /* Tail recursion: `return sumTo(...)` is a tail call — burg's Return(tail) rules emit wasm
     * return_call, which reuses the frame. At scale ~2M this is 2M frames deep WITHOUT tail
     * calls, so every config either runs it in O(1) stack or dies — the workload is itself the
     * falsifier that -O preserves the tail shape (a Click rewrite that breaks the Return(tail)
     * match turns this into stack exhaustion, loudly). The number it produces prices the
     * return_call machinery against arith's plain loop. */
    static int sumTo(int n, int acc) {
        if (n == 0) return acc;
        return sumTo(n - 1, acc + n);
    }
    static int tailrec(int n) {
        return sumTo(n, 0);
    }

    static int virt(int n) {
        Shape[] s = new Shape[3];
        s[0] = new Sq(3); s[1] = new Ci(4); s[2] = new Tr(5);
        int h = 0;
        for (int i = 0; i < n; i++) h += s[i % 3].area(i);
        return h;
    }

    /* sdot/dot: the same dot product — 64 V128s of i32x4 lanes vs the flat
     * 256-int twin. Lane i carries (i+1)*(3i+2) in both, so the checksums are
     * EQUAL by construction; main() enforces it. */
    static int sdot(int n) {
        int len = 64;
        V128[] a = new V128[len]; V128[] b = new V128[len];
        for (int i = 0; i < len; i++) { a[i] = I32x4.splat(i + 1); b[i] = I32x4.splat(i * 3 + 2); }
        int acc = 0;
        for (int r = 0; r < n; r++) {
            V128 s = I32x4.splat(0);
            for (int i = 0; i < len; i++) s = I32x4.add(s, I32x4.mul(a[i], b[i]));
            acc += I32x4.extract_lane(s, 0) + I32x4.extract_lane(s, 1)
                 + I32x4.extract_lane(s, 2) + I32x4.extract_lane(s, 3);
        }
        return acc;
    }
    static int dot(int n) {
        int[] a = new int[256]; int[] b = new int[256];
        for (int i = 0; i < 256; i++) { a[i] = i / 4 + 1; b[i] = (i / 4) * 3 + 2; }
        int acc = 0;
        for (int r = 0; r < n; r++) {
            int s = 0;
            for (int i = 0; i < 256; i++) s += a[i] * b[i];
            acc += s;
        }
        return acc;
    }

    static int memv(int n) {
        int acc = 0;
        for (int r = 0; r < n; r++) {
            Mem.v128_store(64, I32x4.splat(r));
            acc += I32x4.extract_lane(Mem.v128_load(64), 3);
        }
        return acc;
    }
    static int memb(int n) {
        int acc = 0;
        for (int r = 0; r < n; r++) {
            Mem.i32_store8(64 + (r & 63), r);
            acc += Mem.i32_load8_u(64 + (r & 63));
        }
        return acc;
    }

    /* ── The s* string family (see the header): the jre IS the measurement here. ── */

    static int sbuild(int n) {
        int h = 0;
        for (int r = 0; r < n; r++) {
            StringBuffer b = new StringBuffer();
            for (int i = 0; i < 8; i++) {
                b.append("ab");
                b.append((char) ('0' + (r & 7)));
            }
            String s = b.toString();
            h = h * 31 + s.length() + s.charAt(r & 15);
        }
        return h;
    }

    static int sconcat(int n) {
        String a = "abcdef", b = "ghijkl";
        int h = 0;
        for (int r = 0; r < n; r++) {
            String s = a.concat(b).concat((r & 1) == 0 ? a : b);
            h = h * 31 + s.length() + s.charAt(r % 18);
        }
        return h;
    }

    static int scmp(int n) {
        /* Same length, long shared prefix — equals cannot early-out on length,
         * compareTo walks the whole prefix: the loops are the measurement. */
        String[] c = new String[8];
        for (int i = 0; i < 8; i++) {
            StringBuffer b = new StringBuffer("prefix_prefix_prefix_");
            b.append((char) ('a' + i));
            c[i] = b.toString();
        }
        int h = 0;
        for (int r = 0; r < n; r++) {
            String x = c[r & 7], y = c[(r >> 3) & 7];
            if (x.equals(y)) h++;
            h += x.compareTo(y);
            if (x.equalsIgnoreCase(y)) h += 3;
        }
        return h;
    }

    static int shash(int n) {
        String[] c = new String[8];
        StringBuffer b = new StringBuffer("seed");
        for (int i = 0; i < 8; i++) {
            b.append("_grow").append((char) ('a' + i));
            c[i] = b.toString();                 /* lengths 10, 16, 22, … 52 */
        }
        int h = 0;
        for (int r = 0; r < n; r++) h ^= c[r & 7].hashCode() + r;
        return h;
    }

    static int sidx(int n) {
        String hay = "abcabdabeabfabgabhabiabjabkablabmabnaboabpabqabrabsabtabuabvxyz";
        int h = 0;
        for (int r = 0; r < n; r++) {
            h += hay.indexOf('z') + hay.indexOf("xyz") + hay.lastIndexOf('a')
               + hay.indexOf((char) ('a' + (r & 7)));
        }
        return h;
    }

    static int ssub(int n) {
        String hay = "abcabdabeabfabgabhabiabjabkablabmabnaboabpabqabrabsabtabuabvxyz";
        int h = 0;
        for (int r = 0; r < n; r++) {
            String s = hay.substring(r & 15, 16 + (r & 15));
            h = h * 31 + s.length() + (hay.startsWith(s, r & 15) ? 1 : 0)
              + (hay.endsWith(s) ? 2 : 0) + s.charAt(0);
        }
        return h;
    }

    static int scase(int n) {
        String mixed = "  Hello World FOO bar  ";
        int h = 0;
        for (int r = 0; r < n; r++) {
            h = h * 31 + mixed.toLowerCase().charAt(r % 20)
              + mixed.toUpperCase().charAt(r % 20)
              + mixed.replace('o', (char) ('a' + (r & 7))).charAt(9)
              + mixed.trim().length();
        }
        return h;
    }

    static int schars(int n) {
        String hay = "abcabdabeabfabgabhabiabjabkablabmabnaboabpabqabrabsabtabuabvxyz";
        char[] dst = new char[64];
        int h = 0;
        for (int r = 0; r < n; r++) {
            char[] a = hay.toCharArray();
            a[r % 63] = (char) (a[r % 63] + 1);
            hay.getChars(0, 32, dst, 16 + (r & 15));
            String s = new String(a, r & 15, 16);
            h = h * 31 + s.charAt(0) + dst[31];
        }
        return h;
    }

    static int sconv(int n) {
        int h = 0;
        for (int r = 0; r < n; r++) {
            int v = (r & 1) == 0 ? r ^ 0x5555 : -(r ^ 0x5555);   /* both sign paths */
            String d = Integer.toString(v);
            h += Integer.parseInt(d) + d.length();
            if ((r & 15) == 0) h += Long.toString(((long) r << 21) - 1).length();
        }
        return h;
    }

    static int run(int which, int scale) {
        switch (which) {
            case 0: return arith(scale);
            case 1: return lmix(scale);
            case 2: return sieve(scale);
            case 3: return fib(scale);
            case 4: return mat(scale);
            case 5: return alloc(scale);
            case 6: return virt(scale);
            case 7: return tailrec(scale);
            case 8: return sdot(scale);
            case 9: return dot(scale);
            case 10: return memv(scale);
            case 11: return memb(scale);
            case 12: return sbuild(scale);
            case 13: return sconcat(scale);
            case 14: return scmp(scale);
            case 15: return shash(scale);
            case 16: return sidx(scale);
            case 17: return ssub(scale);
            case 18: return scase(scale);
            case 19: return schars(scale);
            case 20: return sconv(scale);
        }
        return -1;
    }

    /* ── The measurement harness — IN Java, so all four configs run the identical methodology ──
     *
     * Per workload: 1 untimed warmup, then REPS timed repetitions bracketing run() with
     * System.currentTimeMillis() (the runner wires a real clock). Reported: MIN (the noise
     * floor — the number that prices a codegen decision, e.g. a burg rule cost) and MEDIAN
     * (the number to expect in practice). Full scales put the slowest config's rep in the
     * hundreds of ms, so the ms clock's granularity is well under 1% of the measurement.
     *
     * Every rep must reproduce the warmup's checksum — a workload that computes different
     * answers across reps has no timing, and the process exits 1 so no table gets printed
     * over a broken run. Cross-CONFIG agreement is the outer harness's diff of the RESULT
     * lines (this process cannot see the other three configs).
     *
     * Any argv argument selects QUICK scales: same code, tiny inputs — the correctness gate
     * the test suite runs so this file cannot silently rot. */
    public static void main(String[] args) {
        boolean quick = args.length > 0;
        int reps = quick ? 3 : 7;
        String[] names = { "arith", "lmix", "sieve", "fib", "mat", "alloc", "virt", "tailrec",
                           "sdot", "dot", "memv", "memb",
                           "sbuild", "sconcat", "scmp", "shash", "sidx", "ssub", "scase",
                           "schars", "sconv" };
        /* Full scales are CALIBRATED from measured quick-mode data (~1 ms / 1000 interpreted
         * iterations on the O0 interpreter, the slowest config), targeting ~2-4 s per rep there
         * so the whole 4-config matrix lands in minutes — while keeping the JIT's reps in the
         * tens of ms, above the ms clock's floor. The first guess (20M iters) put single
         * interpreter reps at ~20 s and the matrix at ~45 min: a benchmark nobody re-runs.
         * One scale per workload, shared by every config — checksums must stay comparable. */
        int[] full  = { 4000000, 4000000, 1000000, 28, 140, 1000000, 3000000, 2000000,
                        30000, 30000, 1000000, 2000000,
                        10000, 17000, 15000, 45000, 9000, 30000, 6000,
                        11000, 50000 };   /* sdot==dot scale: the referee needs
                                       identical inputs, so identical scales. The s* scales
                                       are CALIBRATED from measured quick-mode O0-interp
                                       times (2026-07-25: 60-480 µs per round — the jre
                                       char loops are 100-500x an arith iteration), each
                                       targeting ~3 s per O0-interp rep. */
        int[] small = { 1000, 1000, 1000, 10, 8, 1000, 1000, 100000,
                        100, 100, 1000, 1000,
                        200, 200, 200, 200, 200, 200, 200,
                        200, 200 };   /* tailrec quick = 100K:
                                       still far past any frame-stack depth, so even the quick
                                       gate proves return_call semantics, not just its byte */
        long[] t = new long[reps];
        int[] checks = new int[names.length];
        for (int w = 0; w < names.length; w++) {
            int scale = quick ? small[w] : full[w];
            int check = run(w, scale);                       // warmup, and the reference answer
            checks[w] = check;
            for (int r = 0; r < reps; r++) {
                long t0 = System.currentTimeMillis();
                int v = run(w, scale);
                t[r] = System.currentTimeMillis() - t0;
                if (v != check) {
                    System.out.println("NONDETERMINISTIC " + names[w] + " rep=" + r);
                    System.exit(1);
                }
            }
            for (int i = 1; i < reps; i++) {                 // insertion sort: min + median
                long x = t[i]; int j = i - 1;
                while (j >= 0 && t[j] > x) { t[j + 1] = t[j]; j--; }
                t[j + 1] = x;
            }
            System.out.println("RESULT " + names[w] + " checksum=" + check
                               + " min_ms=" + t[0] + " median_ms=" + t[reps / 2]);
        }
        /* The E8.6 referee: the vector dot product and its scalar twin ran the
         * SAME inputs — a differing checksum is a v128 codegen bug, and no
         * table gets printed over one. (Indices: 8 = sdot, 9 = dot.) */
        if (checks[8] != checks[9]) {
            System.out.println("VECTOR/SCALAR DISAGREE sdot=" + checks[8] + " dot=" + checks[9]);
            System.exit(1);
        }
    }
}

class Node {
    int v; Node next;
    Node(int v) { this.v = v; }
}

abstract class Shape {
    abstract int area(int x);
}
class Sq extends Shape {
    int s; Sq(int s) { this.s = s; }
    int area(int x) { return s * s + x; }
}
class Ci extends Shape {
    int r; Ci(int r) { this.r = r; }
    int area(int x) { return 3 * r * r - x; }
}
class Tr extends Shape {
    int b; Tr(int b) { this.b = b; }
    int area(int x) { return (b * x) >> 1; }
}
