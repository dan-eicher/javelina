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
//   - no library calls — the measurement is the generated code, not the jre.
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
        String[] names = { "arith", "lmix", "sieve", "fib", "mat", "alloc", "virt", "tailrec" };
        /* Full scales are CALIBRATED from measured quick-mode data (~1 ms / 1000 interpreted
         * iterations on the O0 interpreter, the slowest config), targeting ~2-4 s per rep there
         * so the whole 4-config matrix lands in minutes — while keeping the JIT's reps in the
         * tens of ms, above the ms clock's floor. The first guess (20M iters) put single
         * interpreter reps at ~20 s and the matrix at ~45 min: a benchmark nobody re-runs.
         * One scale per workload, shared by every config — checksums must stay comparable. */
        int[] full  = { 4000000, 4000000, 1000000, 28, 140, 1000000, 3000000, 2000000 };
        int[] small = { 1000, 1000, 1000, 10, 8, 1000, 1000, 100000 };   /* tailrec quick = 100K:
                                       still far past any frame-stack depth, so even the quick
                                       gate proves return_call semantics, not just its byte */
        long[] t = new long[reps];
        for (int w = 0; w < names.length; w++) {
            int scale = quick ? small[w] : full[w];
            int check = run(w, scale);                       // warmup, and the reference answer
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
