# bench — the optimization-level benchmark matrix

MEASURED costs, not felt ones. One Java program (`java/Bench.java`) runs the same
workloads under the compiler's and VM's two real knobs, and the numbers price
compiler and backend decisions: a codegen change moves the kernel that covers its
tree shapes and leaves the rest still, which is what makes a delta attributable.

## How to run

    make bench          # full scales, BUDGETED under 10 min: compiles jre+Bench at
                        #   -O0 and -O, runs all four configs, prints the table
    make test-bench     # tiny scales, ~2 min: correctness gate only (checksums
                        #   must agree across configs). Runs as a leg of `make test`.
    sh bench/bench.sh [--quick]   # the same two, directly

The full matrix has a WALL-CLOCK BUDGET (600 s, printed and checked at the end
of every run): a benchmark that takes half an hour stops being run, and a
benchmark nobody runs measures nothing. Scales are calibrated to ~1 s per
O0-interp rep (the slowest config), min-of-3 reps — the between-process ±7%
floor below limits every claim anyway, so more reps or bigger scales buy
precision the floor then eats. When compiler speedups drift the O0 floor, the
budget warning fires and the fix is re-deriving `full[]` in Bench.java by its
documented formula — not deleting kernels, and not living with the warning.

Expected build noise: exactly one compiler warning —
`method 'fib' participates in a recursion cycle through a non-tail call` —
fib IS non-tail recursion; that is what the kernel measures. Anything else is new.

## The matrix

Four configs, two knobs:

    O0-interp   javelinac -O0, javelina -nojit     the floor everything is ratioed to
    O0-jit      javelinac -O0, javelina -jit       what the copy-and-patch JIT buys alone
    O-interp    javelinac -O,  javelina -nojit     what Click (SCCP+GVN+DCE+PEA) buys alone
    O-jit       javelinac -O,  javelina -jit       both

The jre is compiled at the SAME level as the bench artifact in each leg.

## Reading the output

Per config, one line per kernel:

    RESULT <name> checksum=<c> min_ms=<m> median_ms=<d>

- `checksum` — accumulated through the whole computation, so DCE cannot delete
  the work. The harness DIFFS checksums across all four configs and fails the
  run on any disagreement: a config that computes a different answer is WRONG,
  not slow. Within one config, every rep must reproduce the warmup's checksum.
- `min_ms` — the noise floor; the number that prices a codegen decision.
- `median_ms` — the number to expect in practice.

The final table adds ratios vs O0-interp: `jit×` (O0-jit), `opt×` (O-interp),
`both×` (O-jit).

SENSITIVITY FLOOR (measured 2026-07-20): between-process variance is ~±7%.
One run supports the big ratios; it does NOT support cross-config deltas under
~20%. For a small-delta claim, rerun the matrix several times and compare
distributions — or dump and diff the opcode streams first.

## Using it for backend (burg) cost analysis

The raw per-config outputs land in `build/bench-<config>.out` with stable RESULT
lines. The workflow: save them, change the backend (a burg rule cost, a stencil
shape, a tile), rerun, diff. Each kernel leans on a distinct tree-shape family
(below), so the kernel that moved names the rule family that moved it.

## The kernels

Scalar/VM family (no library calls — the measurement is generated code):

| kernel  | leans on |
|---|---|
| arith   | scalar int loop: mul/add/xor/shifts — SCCP/GVN food; shift/mask/imm fusion |
| lmix    | i64 adds/muls/shifts — the i64 rule family |
| sieve   | boolean[] flags: packed i8 stores/loads, bounds checks, strided inner loop |
| fib     | naive recursion: call/return overhead, frame machinery |
| mat     | n×n int matmul over a 1-D array — address-arithmetic folding, loop nests |
| alloc   | object churn, 1-in-8 escapes — allocator + GC + PEA sensitivity |
| virt    | rotating 3-class dispatch — vtable load + indirect call, honest megamorphic |
| tailrec | `return f(...)` self-call — the wasm return_call machinery (O(1) stack) |
| sdot    | V128[] i32x4 dot product — the inline v128 codegen path |
| dot     | sdot's scalar twin, same inputs — the harness FAILS if their checksums differ |
| memv    | v128 round-trips through Mem (D5 bounds guards included) |
| memb    | byte round-trips through Mem — the per-access guard-cost yardstick |

String family (the point IS the jre — §20.12/§20.13/§20.7 compiled char loops;
the baseline any host-string design, e.g. the wasm js-string builtin namespace,
has to beat):

| kernel  | leans on |
|---|---|
| sbuild  | StringBuffer append growth + toString — the `"…" + x` workhorse |
| sconcat | String.concat chains — alloc + two bulk copies, no growth machinery |
| scmp    | equals/compareTo/equalsIgnoreCase, shared-prefix same-length strings |
| shash   | hashCode — uncached 31·h+c loops |
| sidx    | indexOf(char)/indexOf(String)/lastIndexOf scans |
| ssub    | copying substring + startsWith/endsWith |
| scase   | toLower/toUpper/replace/trim through the GENERATED Character if-trees |
| schars  | toCharArray/getChars/new String(char[]) bulk copies |
| sconv   | Integer.toString / parseInt round-trips + Long.toString digit loops |

Opcode-family kernels — one instruction family each, added 2026-07-31 because the
two families above cover only **70** of the opcodes a bench run emits. These take
it to **132**; the union with the jre is 138. Where the kernels above are
application-shaped (a realistic mix, which makes them a good end-to-end gate and a
poor per-family ruler), these are unrolled IN THE FAMILY — many ops of one family
per loop back-edge — so the time, and the jit×, attribute to the family:

| kernel | leans on |
|---|---|
| dmix   | f64 add/sub/mul/div/neg, all six compares, i32↔i64↔f64 converts |
| fmix   | f32 arithmetic + compares + neg + the promote/demote pair |
| fmath  | Math.sqrt/floor/ceil/rint — the four f64 unary INTRINSICS, not jre calls |
| refeq  | ref.eq / ref.is_null / ref.test, both polarities — the burg cond/ncond family |
| lcmp   | i64 eqz/eq/ne/lt/le/ge + div_s/rem_s + the i32 compare leftovers (lmix is arithmetic only) |
| narrow | (byte)/(short)/(char) casts, packed ARRAY elements, packed FIELDS (struct.get_s/u) |
| memops | memory.fill / memory.copy — bulk linear memory |
| vshuf  | v128.const + i8x16.shuffle — the two immediate-operand SIMD opcodes |
| itail  | virtual tail dispatch — return_call_ref (tailrec covers the static return_call) |

Invariant-tier family — each row is one CLASS-INVARIANT tier the optimizer
proves, paired against the shape it cannot prove, so the -O delta (and the
guard census over the bench compile) prices the tier:

| kernel | leans on |
|---|---|
| coo    | Luján Fig. 1 sparse mvm over RAW parameter arrays — nothing folds; the surviving bounds pairs are ALSO the merged-unsigned-compare (§7.2 GeU) input |
| cooc   | coo's class-shaped twin: private final indirection arrays, ctor-checked fills — the count/data PAIR and array-CONTENT invariants fold the indirect checks; same inputs, so the pair referees checksums like sdot/dot |
| fixdiv | division by an ESTABLISHED field (`final int scale = 16`) — the unary RANGE invariant folds the §15.17 by-zero and -1-wrap guards; the receiver is static so scalar replacement cannot turn it into constant division |

Every invariant tier the optimizer gains lands with its kernel here (or names
in this table why it cannot have one) — the same rule the opcode-family
kernels enforce for the backend: coverage is declared, not assumed.

Two construction notes that are load-bearing, not decoration. `itail`'s two
implementations dispatch through each other's `peer` so the receiver is genuinely
polymorphic: with a single implementation Click devirtualizes on a singleton and
emits the *static* return_call, and the kernel silently measures `tailrec` twice.
`lcmp`'s divisor is `(b >>> 1) | 1L` — positive and odd, so it clears both wasm
i64 traps; the obvious `b | 1` still hits the MIN_VALUE/-1 overflow.

`dmix` doubles as a float-semantics differ. The logistic map is bounded in [0,1]
so no iteration count reaches inf/NaN, but it is chaotic — a one-ulp divergence
between configs (excess precision, a contracted multiply-add) amplifies until the
checksum gate catches it. If dmix ever fails that gate, it is a real bug.

Not covered, and why: **memory.grow**, the one emittable opcode with no
deterministic benchmark. It mutates process-wide state, so rep 2 sees rep 1's
pages and returns a different old-size — the reps disagree by construction and the
checksum gate rejects the run. Timing it needs a harness that re-instantiates per
rep, which is a different measurement. Six more opcodes stay jre-only by
construction: the four `reinterpret` ops (reachable only through Float/Double bit
methods, so the opcode is emitted jre-side) and the two `extern` converts (the
host boundary).

String.intern is deliberately not benched: its table persists across reps, so
its timing drifts with table state — a lookup benchmark, not a string benchmark.

## History worth knowing

This harness's first run caught a real Click -O miscompile (array-element strong
updates → false devirtualization; `VirtRepro.java` is the 15-line pin). The -O
output had no e2e coverage anywhere before this gate existed. The string
baseline (2026-07-25): jit× is 3.1–4.4x for the s* family vs 5.4–6.8x scalar —
string time is allocation/GC/call/guard dominated; best-config per-char costs
are ~0.9 µs compared and ~0.6 µs hashed, 2–3 orders of magnitude above a native
memcmp/hash — the js-string decision numbers.
