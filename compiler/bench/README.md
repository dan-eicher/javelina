# bench — the optimization-level benchmark matrix

MEASURED costs, not felt ones. One Java program (`java/Bench.java`) runs the same
workloads under the compiler's and VM's two real knobs, and the numbers price
compiler and backend decisions: a codegen change moves the kernel that covers its
tree shapes and leaves the rest still, which is what makes a delta attributable.

## How to run

    make bench          # full scales, ~15 min: compiles jre+Bench at -O0 and -O,
                        #   runs all four configs, prints the comparison table
    make test-bench     # tiny scales, ~2 min: correctness gate only (checksums
                        #   must agree across configs). Runs as a leg of `make test`.
    sh bench/bench.sh [--quick]   # the same two, directly

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
