# conformance/ — the Java e2e corpus

Real `.java` programs, compiled by the shipped `javelinac` and run by the shipped
`javelina`, at both optimisation levels and on both execution tiers. The root
`Makefile` has reserved this slot since E7.4 and runs it as the last leg of
`make test`:

```
make test-java-conformance      # or just `make test`
sh conformance/run.sh --full    # full scales, by hand
sh conformance/run.sh --seed N  # replay a failing seed
```

`run.sh`'s exit code is the whole contract. It builds its own binaries, so it
stands alone.

## Why this exists, next to `testsuite/*.wast`

The official WebAssembly testsuite is 60113 execution cases and it is the oracle,
but it **confirms** — it never **discovers**. Every GC bug this project has found
came from a hand-written program someone happened to write. This corpus is that,
done on purpose: the largest real program the toolchain runs, built to have the
shapes a collector gets wrong. There is no Oracle JCK here and none is assumed.

It paid for itself on its first compile — see "What it has found" below.

## The oracle

Two independent ones, and the program is deterministic (a seeded LCG, printed on
line 1; no clock, no `Math.random`, no identity-hash iteration order):

1. **Structural invariants**, checked in-program. Every node carries `mix(id)`
   fixed at construction; every link is checked for object **identity**, not
   value. On a violation the program prints which invariant broke, on which
   object, and exits 1. It never prints a mystery.
2. **Cross-config agreement.** `{-O0,-O} × {interp,jit}`, star-diffed against
   `O0-interp`. A disagreement names a config that is *wrong*, not slow. This is
   the leg that catches a miscompile rather than a collector bug.

`run.sh` also asserts the RESULT-line **count**. A star-diff alone passes
vacuously when every config prints nothing, which is the same defect as an
exclusion counter stuck at zero.

3. **The compiler's own memory**, checked by running `javelinac -O` over this
   corpus under valgrind. It has to be here because this is the only place the
   gate compiles a real non-RTL program, and it has to be valgrind: `bbq_arena`
   bump-allocates sub-ranges inside one `malloc`'d block, so an over-read of a
   sub-array never crosses a redzone and **ASAN cannot see it** — `test-exec-asan`
   is not a substitute. It earns its ~6 minutes: the optimizer's DSE indexed
   `mem_kind`/`mem_elem`/`mem_spine` to `vnode_count` when they are sized
   `mem_rows`, and that shipped green through every other leg because reading past
   an arena array lands on zeroes.

## Coverage, and its limits

Stated as measured, not as intended. Where a shape is not reachable it says so
and names the stage that can reach it, because a harness that claims a property
it cannot observe is worse than one that omits it.

| shape | kernel | status |
|---|---|---|
| deep chains (200k nodes, one worklist) | `chain` | observed |
| cycles: self, 2-cycle, ring of n | `cycles` | observed |
| cross-links between subgraphs | `cross` | observed |
| wide fan-out, 25·w objects from one root | `fanout` | observed |
| arrays of refs, heterogeneous + nested | `refarr` | observed |
| §10.10 covariant `ArrayStoreException` | `refarr` | observed |
| 4-level subtype hierarchy, fields at every level | `hier` | observed |
| interned `Class` identity + `getClass()` header | `hier` | observed |
| LOS boundary straddled exactly (4012 / 4013) | `los` | observed |
| objects outliving every other kernel's churn | `survive` | observed |
| fields nulled, and re-stored, between collections | `nulled` | observed |
| v128 fields, `V128[]`, `Mem` bounce, guard throws | `simd` | observed |
| mixed lifetimes across 22 collections, with evacuation | `evac` | observed |
| **non-nullable references** | — | **unreachable, see below** |

### Evacuation: found dead, fixed, now covered

The `evac` kernel was written to force evacuation and, on its first run, proved
that evacuation **never happened at all**: instrumenting `gc_collect` over a
full-scale run gave **22 collections, 176 attempts, 0 objects moved**.

Immix §3.2 requires a reserve — *"immix sets aside a small number of free blocks
that it never returns to the global allocator and only ever uses for
evacuating"*. We computed the 2.5% figure and set nothing aside, drawing targets
from the live free list at the top of a collection, which the mutator has drained
by then; that is *why* the collection fired. Candidates had the same defect,
taken from the recyclable vector the allocator also drains. §3.2.1 instead uses
statistics recorded at the previous sweep, and our `hole_count` was already being
computed and read by nobody.

Both halves are now implemented, and `test_gc_evac_stress` — whose header always
claimed "each collection defragments and MOVES live nodes" while checking no such
thing — now asserts it: **64 of 64 rooted heads relocate**, where it measured 0
before. Deliberately breaking the forwarding-pointer update in `gc_mark1` now
fails this corpus; before the fix it changed no checksum at all.

### Non-nullable references

`javelinac` only ever emits `0x63` (`WT_REF_NULL`); `0x64` appears nowhere in
`compiler/src`, and `wasm_types.c:1263` says it outright — *"All three are
nullable, hence defaultable."* So no Java program can exercise a non-null ref,
and this corpus does not pretend to. It belongs to the stage that emits raw
modules.

### What a Java-level oracle cannot do

Breaking the tracer so it skips the last reference field of every struct makes
this corpus **segfault** rather than name an invariant — a dangling reference
faults before any Java check can run. The gate still fails, loudly, but that is
the ceiling of an in-program oracle and it is the gap a heap-invariant checker on
the C side is for.

## What it has found

- **`(V128[]) obj` emitted an invalid module.** The cast classifier spelled the
  primitive-array rule as "numeric or boolean" (`compiler_helpers.c`), and
  `JT_V128` is neither, so `V128[]` fell through to the reference-array path and
  was structurally `ref.cast` to `RefArray`. Caught on the corpus's *first*
  compile, by the compiler's own validator. Fixed by routing both the cast and
  the `instanceof` twin through the §4.2 predicate and the lattice; pinned in
  `test_exec.c`.
- **`gc_obj_size`'s v128 element stride was unpinned.** No test asserted that a
  16-byte-element array is sized with `elem_heap_w`. Now pinned in
  `wasm/test/test_gc.c`, at both strides and both LOS boundary lengths.
- **Evacuation never ran** (above) — two unimplemented halves of Immix §3.2/§3.2.1,
  now fixed, with the movement itself pinned in `test_gc_evac_stress`.
- **`-O` spends ~40s on interprocedural summary convergence** for this ~800-line
  program — more than the whole jre — and `JAVELINA_CLICK_ONLY` does not gate it,
  because `compiler_summarize_to_convergence` runs before that hook
  (`wasm_module.c:258`). The documented bisection knob cannot isolate the
  dominant cost.

## Adding a case

Kernels live in `conformance/src` (a directory input, walked recursively). To add
one: write `static int name(int scale)` returning a checksum folded from what it
walked, add it to `GcTorture.NAMES`, `SCALE_FULL`, `SCALE_QUICK` and `run()`
— all index-aligned — and bump `EXPECT_KERNELS` in `run.sh`.

Two rules, both learned the hard way:

- **Falsify it.** Break the code the check names and watch it go red. A check
  that cannot go red is not a check — two in the first draft of this file
  survived deliberate collector sabotage untouched and were rewritten or moved to
  the level that owns them.
- **Never assert what you cannot observe.** State the citation for what a shape
  is *meant* to provoke, and separately state what is actually checked.
