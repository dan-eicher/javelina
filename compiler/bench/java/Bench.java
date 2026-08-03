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
//
// The OPCODE-FAMILY kernels (see the block comment above dmix): one instruction family
// each, unrolled in the family so the time is attributable to it rather than to the loop.
// They exist because the workloads above cover 70 of the 129 opcodes a bench run executes.
//   dmix   f64 add/sub/mul/div, f64 compares, i32↔f64 converts
//   fmix   f32 arithmetic + compares + the promote/demote pair
//   fmath  Math.sqrt/floor/ceil/rint — the four f64 unary INTRINSICS (not jre calls)
//   refeq  ref.eq / ref.is_null / ref.test, both polarities — the burg cond/ncond family
//   lcmp   i64 eqz/eq/lt_s + div_s/rem_s (lmix covers i64 arithmetic only)
//   narrow (byte)/(short)/(char) casts, packed ARRAY elements, packed FIELDS
//   memops memory.fill / memory.copy — bulk linear memory
//   vshuf  v128.const + i8x16.shuffle — the two immediate-operand SIMD opcodes
//   itail  virtual tail dispatch — return_call_ref (tailrec covers the static return_call)
//   coo    sparse mvm, coordinate storage (Luján 2004 Fig. 1): y[indx[k]] += v[k]*x[jndx[k]]
//          — the INDIRECTION family; two indirect IDX_HIGH per iteration, the number
//          array-content invariants would move. RAW parameter arrays: the unverifiable
//          shape, so its checks all stay — the BASELINE
//   cooc   coo's CLASS-SHAPED twin (Fig. 11's spirit): a Coo class owns private final
//          index arrays, ctor fills them through checked stores — the verifiable shape;
//          the cooc/coo delta at -O prices the array-content tier
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

    /* ── The opcode-family kernels: one instruction family per workload ──
     *
     * These exist because the application-shaped kernels above leave families with NO
     * coverage at all — measured 2026-07-31 by counting distinct opcodes through ew_emit:
     * the workloads above emit 70 of the 129 a bench run executes, and the f32/f64 family,
     * ref.eq, i64.eqz, the narrowing/packed family and the bulk-memory ops appeared in
     * ZERO of them (the last four opcodes here appeared in no compiled code at all, jre
     * included). A cost or a jit× for those families was not "small" — it did not exist.
     *
     * Construction differs from the workloads above on purpose: an application kernel is a
     * realistic MIX, which is what makes it a good end-to-end regression gate and a bad
     * per-family ruler. These loops are UNROLLED IN THE FAMILY — each iteration performs
     * many ops of one family against one loop back-edge — so the family, not the loop
     * overhead, dominates the time and the jit× is attributable to it. They are still real
     * computations with accumulated checksums; nothing here is a spin loop.
     *
     * Math.sqrt/floor/ceil/rint are stamped INTRINSICS (sema stamp_math_kind), lowered to
     * single f64 opcodes — fmath is generated code, not a jre call, so the "no library
     * calls" rule above holds for this family too.
     *
     * NOT covered, and why: memory.grow. It is the one emittable opcode with no
     * deterministic benchmark — it mutates process-wide state, so the reps disagree by
     * construction (rep 2 sees rep 1's pages and returns a different old-size) and the
     * checksum gate rejects the run. Timing it would need a harness that re-instantiates
     * per rep, which is a different measurement than this one. */

    /* f64 scalar arithmetic: add/sub/mul/div + compares + i32↔f64 converts. The logistic
     * map is the bounded generator — x stays in [0,1] for r=3.9, so no iteration count can
     * reach inf/NaN and the checksum is exact. It is also chaotic, which makes this the
     * bench's most sensitive detector of a float-semantics divergence between configs
     * (excess precision, contracted multiply-add): any one-ulp difference amplifies until
     * the checksum gate sees it. */
    static int dmix(int n) {
        double x = 0.5, y = 0.25, s = 0.0;
        int h = 0;
        for (int i = 0; i < n; i++) {
            x = 3.9 * x * (1.0 - x);
            y = (y + x) * 0.5;
            double d = (x + 1.0) / (y + 1.0);
            if (d > 1.0) s = s + d; else s = s - d;
            if (d == 1.0) h += 1;                    // f64.eq
            if (d != 1.0) h += 2;                    // f64.ne
            if (x <= y) h += 4;                      // f64.le
            if (x >= y) h += 8;                      // f64.ge
            s = s + (-x);                            // f64.neg
            long li = (long) (x * 1.0e6);            // i64.trunc_sat_f64_s
            h ^= (int) ((double) li * 1.0e-3);       // f64.convert_i64_s
            if (s > 1.0e9) s = s - 1.0e9;
            if (s < -1.0e9) s = s + 1.0e9;
        }
        return h ^ (int) (s * 1000.0) ^ (int) (x * 1.0e9) ^ (int) (y * 1.0e9);
    }

    /* f32 arithmetic — the same shape at single width, plus the promote/demote pair that
     * only appears where float and double meet. */
    static int fmix(int n) {
        float x = 0.5f, y = 0.25f;
        int h = 0;
        for (int i = 0; i < n; i++) {
            x = 3.9f * x * (1.0f - x);
            y = (y + x) * 0.5f;
            float d = (x + 1.0f) / (y + 1.0f);
            if (d > 1.0f) h += 1; else h -= 1;
            if (x < 0.5f) h ^= i;
            if (d == 1.0f) h += 1;              // f32.eq
            if (d != 1.0f) h += 2;              // f32.ne
            if (x <= y) h += 4;                 // f32.le
            if (x >= y) h += 8;                 // f32.ge
            double w = (double) x * 0.5;        // f64.promote_f32
            y = y + (float) w;                  // f32.demote_f64
            float ci = (float) i;               // f32.convert_i32_s
            float cl = (float) ((long) i * 3L); // f32.convert_i64_s
            long tl = (long) (ci * 0.5f);       // i64.trunc_sat_f32_s
            h ^= (int) tl + (int) (-x * 100.0f) + (int) (cl * 0.001f);   // f32.neg
        }
        return h ^ (int) (x * 1.0e6f) ^ (int) (y * 1.0e6f);
    }

    /* The f64 unary intrinsics: sqrt/floor/ceil/nearest, four opcodes per iteration. */
    static int fmath(int n) {
        int h = 0;
        for (int i = 0; i < n; i++) {
            double v = (double) (i & 1023) + 0.5;
            double s = Math.sqrt(v) + Math.floor(v * 0.25)
                     + Math.ceil(v * 0.125) + Math.rint(v * 0.5);
            h = h * 31 + (int) s;
        }
        return h;
    }

    /* Reference identity, null tests and type tests: ref.eq, ref.is_null, ref.test — the
     * family the burg's cond/ncond branch-polarity rules serve, and the one with the
     * widest gap between how often it is COMPILED (null tests are the single commonest
     * comparison in the jre) and how often it was MEASURED (never). Both polarities of
     * each test appear, so a rule that only fires on one shows up as a partial win. */
    static int refeq(int n) {
        Node[] a = new Node[16];
        for (int i = 0; i < 16; i++) a[i] = ((i & 3) == 0) ? null : new Node(i);
        Object[] os = new Object[4];
        os[0] = new Sq(3); os[1] = new Ci(4); os[2] = new Tr(5); os[3] = a[1];
        int h = 0;
        for (int i = 0; i < n; i++) {
            Node x = a[i & 15], y = a[(i >> 2) & 15];
            if (x == y) h += 2;                       // ref.eq
            if (x != y) h -= 1;                       // ref.eq, opposite polarity
            if (x == null) h += 3;                    // ref.is_null
            if (x != null) h += x.v;                  // ref.is_null + a guarded field load
            if (y != null && y.next == null) h += 5;  // short-circuit over two null tests
            Object o = os[i & 3];
            if (o instanceof Sq) h += 7;              // ref.test, exact
            if (o instanceof Shape) h += 11;          // ref.test, up the hierarchy
            h = h * 31 + i;
        }
        return h;
    }

    /* i64 compares and division — lmix covers i64 ARITHMETIC and nothing else, so eqz/eq/
     * lt_s and the div_s/rem_s pair had no kernel. The divisor is forced positive and odd
     * (`>>> 1 | 1`), which keeps it out of both wasm i64 traps: never zero, and never the
     * MIN_VALUE/-1 overflow. */
    static int lcmp(int n) {
        long a = 1L;
        int h = 0;
        for (int i = 0; i < n; i++) {
            a = a * 6364136223846793005L + 1442695040888963407L;
            long b = a >> 17;
            if (b == 0L) h += 1;                      // i64.eqz
            if (b != 0L) h += 2;                      // i64.eqz, opposite polarity
            if (a == b) h += 3;                       // i64.eq
            if (a < b) h -= 1;                        // i64.lt_s
            if (a != b) h += 1;                       // i64.ne
            if (a <= b) h += 2;                       // i64.le_s
            if (a >= b) h += 4;                       // i64.ge_s
            long d = (b >>> 1) | 1L;                  // positive, odd — trap-free
            h += (int) (a / d);                       // i64.div_s
            h ^= (int) (a % d);                       // i64.rem_s
            long m = a & 0x00FFFFFFFFFFFFFFL;         // i64.and
            h += (int) (m >>> 40);
            /* The i32 relational/bitwise leftovers: both operands are variables, so these
             * are i32.eq / i32.le_s and NOT the eqz the zero-compare rules would pick. */
            int p = (int) b, q = i * 3;
            if (p == q) h += 8;                       // i32.eq
            if (p <= q) h += 16;                      // i32.le_s
            h |= (p & 7);                             // i32.or
        }
        return h;
    }

    /* Narrowing and packed storage: the (byte)/(short)/(char) casts (i32.extend8_s /
     * extend16_s), packed ARRAY elements (array.get_s/get_u at i8 and i16), and packed
     * FIELDS (struct.get_s/get_u) — three different sign-extension paths that the int-only
     * kernels above never touch. */
    static int narrow(int n) {
        byte[] ba = new byte[64];
        short[] sa = new short[64];
        char[] ca = new char[64];
        for (int i = 0; i < 64; i++) {
            ba[i] = (byte) (i * 7); sa[i] = (short) (i * 1013); ca[i] = (char) (i * 999);
        }
        Packed p = new Packed();
        int h = 0;
        for (int i = 0; i < n; i++) {
            int k = i & 63;
            h += ba[k];                     // array.get_s  i8
            h ^= sa[k];                     // array.get_s  i16
            h += ca[k];                     // array.get_u  i16
            ba[k] = (byte) (h + i);
            sa[k] = (short) (h ^ i);
            p.b = (byte) i; p.s = (short) (i * 3); p.c = (char) (i * 5);
            h += p.b + p.s + p.c;           // struct.get_s / struct.get_u
            h += (byte) h;                  // i32.extend8_s
            h += (short) h;                 // i32.extend16_s
            h ^= (char) h;                  // zero-extend to 16 bits
        }
        return h;
    }

    /* Bulk linear memory: memory.fill and memory.copy. Every iteration refills before it
     * reads, so the result never depends on residue — which is what makes it deterministic
     * across reps even though the runtime's own host-I/O staging shares the low pages. */
    static int memops(int n) {
        int h = 0;
        for (int r = 0; r < n; r++) {
            Mem.memory_fill(256, r & 255, 64);
            Mem.memory_copy(384, 256, 64);
            h += Mem.i32_load8_u(384 + (r & 63));
            h = h * 31 + Mem.i32_load8_u(256);
        }
        return h;
    }

    /* v128.const and i8x16.shuffle — the two SIMD opcodes whose operands are 16-byte
     * IMMEDIATES, so they are the two the sdot/memv kernels structurally cannot reach
     * (those build their vectors with splat). Both masks are sema-validated constants. */
    static int vshuf(int n) {
        V128 k = V128.const_(0x0706050403020100L, 0x0F0E0D0C0B0A0908L);
        int acc = 0;
        for (int r = 0; r < n; r++) {
            V128 a = I32x4.splat(r);
            V128 b = I8x16.shuffle(a, k, 0x0F0D0B0907050301L, 0x1E1C1A1816141210L);
            acc += I32x4.extract_lane(b, 0) ^ I32x4.extract_lane(b, 3);
        }
        return acc;
    }

    /* Virtual tail dispatch — wasm return_call_ref, the one call mechanism with no kernel.
     * tailrec covers return_call (a STATIC tail call, a known callee); this covers the
     * vtable-dispatched form, where the callee is a loaded funcref.
     *
     * The two implementations alternate through each other's `peer`, so the receiver is
     * genuinely polymorphic — a single implementation would let Click devirtualize on a
     * singleton and emit the static return_call, silently measuring tailrec again. Like
     * tailrec, the workload is its own falsifier: at this scale it is millions of frames
     * deep unless the tail shape survives, so a rewrite that breaks it exhausts the stack
     * loudly rather than quietly costing more. */
    static int itail(int n) {
        WA a = new WA(); WB b = new WB();
        a.peer = b; b.peer = a;
        return a.go(n, 0);
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

    /* Sparse matrix-vector multiplication, coordinate storage (Luján/Gurd/Freeman/
     * Miguel 2004, Fig. 1 — their kernel verbatim): y[indx[k]] += value[k] * x[jndx[k]].
     * The INDIRECTION shape: every iteration carries two indirect IDX_HIGH checks
     * (y through indx, x through jndx) that no direct-index analysis can touch —
     * the number array-content invariants would move. Values are small ints in
     * f64, so the arithmetic is exact and the checksum reproducible. */
    static void mvmCOO(int indx[], int jndx[],
                       double value[], double y[], double x[]) {
        for (int k = 0; k < value.length; k++)
            y[indx[k]] += value[k] * x[jndx[k]];
    }

    static int coo(int n) {
        int rows = 200, nnz = 2000;
        int[] indx = new int[nnz];
        int[] jndx = new int[nnz];
        double[] value = new double[nnz];
        double[] x = new double[rows], y = new double[rows];
        int seed = 12345;
        for (int k = 0; k < nnz; k++) {
            seed = seed * 1103515245 + 12345;
            indx[k] = (seed >>> 16) % rows;
            seed = seed * 1103515245 + 12345;
            jndx[k] = (seed >>> 16) % rows;
            value[k] = (k % 7) + 1;
        }
        for (int i = 0; i < rows; i++) x[i] = (i % 5) + 1;
        for (int it = 0; it < n; it++) mvmCOO(indx, jndx, value, y, x);
        int h = 0;
        for (int i = 0; i < rows; i++) h = h * 31 + (int) y[i];
        return h;
    }

    /* coo's CLASS-SHAPED twin (Luján 2004 Fig. 11's spirit): the same six-line
     * kernel over arrays a class OWNS — private final indirection arrays,
     * filled by the constructor through checked stores. This is the shape the
     * array-content invariant verifies: the two indirect IDX_HIGH per
     * iteration fold, where coo's (raw parameter arrays — the shape the
     * paper's design exists to escape) must stay. coo is the baseline; the
     * cooc/coo delta at -O prices the tier. Same geometry, same LCG, same
     * computation — the checked fill rejects nothing, so the checksums MATCH
     * coo's, which makes the pair a referee as well as a ratio (sdot/dot's
     * pattern). */
    static int cooc(int n) {
        Coo c = new Coo();
        for (int it = 0; it < n; it++) c.mvm();
        int h = 0;
        for (int i = 0; i < 200; i++) h = h * 31 + (int) c.y[i];
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
            case 21: return dmix(scale);
            case 22: return fmix(scale);
            case 23: return fmath(scale);
            case 24: return refeq(scale);
            case 25: return lcmp(scale);
            case 26: return narrow(scale);
            case 27: return memops(scale);
            case 28: return vshuf(scale);
            case 29: return itail(scale);
            case 30: return coo(scale);
            case 31: return cooc(scale);
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
                           "schars", "sconv",
                           "dmix", "fmix", "fmath", "refeq", "lcmp", "narrow", "memops",
                           "vshuf", "itail", "coo", "cooc" };
        /* Full scales are CALIBRATED from measured quick-mode data (~1 ms / 1000 interpreted
         * iterations on the O0 interpreter, the slowest config), targeting ~2-4 s per rep there
         * so the whole 4-config matrix lands in minutes — while keeping the JIT's reps in the
         * tens of ms, above the ms clock's floor. The first guess (20M iters) put single
         * interpreter reps at ~20 s and the matrix at ~45 min: a benchmark nobody re-runs.
         * One scale per workload, shared by every config — checksums must stay comparable. */
        int[] full  = { 4000000, 4000000, 1000000, 28, 140, 1000000, 3000000, 2000000,
                        30000, 30000, 1000000, 2000000,
                        10000, 17000, 15000, 45000, 9000, 30000, 6000,
                        11000, 50000,
                        750000, 600000, 1500000, 400000, 400000, 350000, 750000,
                        3000000, 2000000, 300, 300 };   /* coo/cooc, same scale — the
                                       delta between them at -O IS the measurement.
                                       coo: 2000 nnz × 300 mvm passes,
                                       ~5 array ops per k-iteration → ~3 s per O0-interp
                                       rep by the same per-1000-iteration reading. */
                                    /* the opcode-family scales are calibrated
                                       the same way: measured O0-interp quick-mode cost
                                       (2026-07-31, per 1000 iterations — dmix 4 ms, fmix 5,
                                       fmath 2, refeq 7, lcmp 7, narrow 9, memops 4, vshuf 1;
                                       itail 153 ms per 100000), each scaled to ~3 s per rep
                                       on that config. These loops are unrolled in their
                                       family, so an iteration is worth several of arith's —
                                       reading across from arith's scale would have set dmix
                                       and lcmp 3-5x too high.
                                       sdot==dot scale: the referee needs
                                       identical inputs, so identical scales. The s* scales
                                       are CALIBRATED from measured quick-mode O0-interp
                                       times (2026-07-25: 60-480 µs per round — the jre
                                       char loops are 100-500x an arith iteration), each
                                       targeting ~3 s per O0-interp rep. */
        int[] small = { 1000, 1000, 1000, 10, 8, 1000, 1000, 100000,
                        100, 100, 1000, 1000,
                        200, 200, 200, 200, 200, 200, 200,
                        200, 200,
                        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
                        100000, 3, 3 };   /* itail quick = 100K, for tailrec's reason: past any
                                       frame-stack depth, so even the quick gate proves the
                                       VIRTUAL tail call, not just its byte.
                                       tailrec quick = 100K:
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

/* Packed fields — byte/short/char members are the struct.get_s/get_u source; an int-only
 * class cannot produce those opcodes however it is used. */
class Packed {
    byte b; short s; char c;
}

/* itail's alternating pair: each one's tail call dispatches through the OTHER's vtable, so
 * neither call site sees a single receiver type. */
abstract class Walker {
    Walker peer;
    abstract int go(int n, int acc);
}
class WA extends Walker {
    int go(int n, int acc) {
        if (n == 0) return acc;
        return peer.go(n - 1, acc + n);
    }
}
class WB extends Walker {
    int go(int n, int acc) {
        if (n == 0) return acc;
        return peer.go(n - 1, acc ^ n);
    }
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

/* cooc's indirection owner: fixed geometry so every length is KNOWN, private
 * final index arrays that never leave the class, checked fills — the exact
 * shape the array-content verifier proves one class at a time. */
class Coo {
    private final int[] indx = new int[2000];
    private final int[] jndx = new int[2000];
    final double[] value = new double[2000];
    final double[] y = new double[200];
    final double[] x = new double[200];

    Coo() {
        int seed = 12345;
        for (int k = 0; k < 2000; k++) {
            seed = seed * 1103515245 + 12345;
            int a = (seed >>> 16) % 200;
            if (a >= 0 && a < y.length) indx[k] = a;
            seed = seed * 1103515245 + 12345;
            int b = (seed >>> 16) % 200;
            if (b >= 0 && b < x.length) jndx[k] = b;
            value[k] = (k % 7) + 1;
        }
        for (int i = 0; i < 200; i++) x[i] = (i % 5) + 1;
    }

    void mvm() {
        for (int k = 0; k < value.length; k++)
            y[indx[k]] += value[k] * x[jndx[k]];
    }
}
