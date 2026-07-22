# Test baseline — the parity oracle

Captured 2026-07-21 on the harness-unification branch, at commit `3f04480`
(the initial import — i.e. the tree exactly as it stood before any harness work).

**This file is the gate.** Every phase of the test-harness unification must
reproduce these verdicts exactly. A phase that changes a number here has either
found a real bug or broken something; either way it stops and gets explained
before it lands. Restructuring work does not get to "fix" a count.

Reproduce with:

    make -C wasm test
    make -C compiler test

## VM (`wasm/`)

    ── 90 passed, 0 failed ──

Wall 229.87 s (3:50), peak RSS 117,624 KB (115 MB) for the whole `make` process.

Conformance, driven by `test_wast` over the pinned `testsuite/` submodule
(0dc0343, 2026-05-27) plus the three in-tree `test/regress_*.wast`:

| gate | result |
|---|---|
| binary-module conformance | 810 ok, 0 mismatched (260 files) |
| §7 validation gate (Phase 1) | 99 ok, 0 mismatched |
| §7 validation gate (text modules) | 5135 ok, 0 mismatched, 0 excluded |
| §7 reject-reason vs `.wast` string | 0 rejected for the WRONG reason |
| text-module (.wat reader) conformance | 6364 ok, 0 mismatched, 0 excluded |
| **execution conformance** | **60113 ok, 0 mismatched, 0 excluded** |
| trap-reason vs `.wast` string | 0 trapped for the WRONG reason |

## Compiler (`compiler/`)

    compiler tests: 20 passed, 0 failed

Wall 729.65 s (12:10), peak RSS 10,251,984 KB (**10.25 GB**).

Per-suite measurements (binaries run individually, same machine, same day):

| suite | wall | peak RSS |
|---|---|---|
| test_sir | 248 s | **9.81 GB** |
| test_exec | 285 s | 3.38 GB |
| test_sema | 38 s | 4.63 GB |
| test_lattice | 0.7 s | 64 MB |

## What these numbers already tell us

- **The memory problem is entirely compiler-side.** The VM suite runs ~90
  binaries in 115 MB; the compiler runs 20 in 10.25 GB. Nothing about the
  harness style causes this — it is what the compiler suites *do* per case
  (a full sema+compile of the 31k-line prelude), not how they assert.
- **`test_lattice` is the control.** Same harness, same project, 64 MB and
  under a second, because it does not recompile the prelude. It is the shape
  every compiler suite should approach.
- **Wall time has grown past the folklore.** The working assumption was ~8 min
  for `make test`; it is 12:10. Phase 2 (build once, link many) and Phase 3
  (sema-once) are both aimed at this.
- Peak RSS for a `make` run is the max over its children, so the 10.25 GB
  figure is essentially `test_sir` alone (9.81 GB) plus make's own overhead.

## 2026-07-21 — after the harness work (P0–P2, and P3's arena fix)

Verdicts unchanged and gate-set identical to the pin above: VM **90 passed,
0 failed** with conformance 810/0 binary, 6364/0 text, **60113/0/0 execution**;
compiler **20 passed, 0 failed**. Gate lists were diffed by name, not compared
by total, in both directions.

What moved:

| | before | after |
|---|---|---|
| whole compiler suite, peak RSS | 10.25 GB | **3.38 GB** |
| `test_sir` peak RSS | 9.81 GB | **1.18 GB** |
| `test_sema` peak RSS | 4.41 GB | **1.09 GB** |
| `test_exec` peak RSS | 3.38 GB | 3.23 GB |
| one unchanged suite, e.g. `make test-lattice` | full relink every time | **0.8 s** |
| VM `wasm/test` on disk | 214 MB | **1.1 MB** |

Compiler suite count is 17, not 20: `codegen_wasm.c` and `structurer.c` were
deleted as dead paths with no production caller, and the three tests that
existed only to exercise them went with them. No coverage was lost.

**Where the memory actually was.** `sema_destroy` releases 31 hash trees and
vectors — `expr_types`, `resolved_methods`, `resolved_fields`, `slot_allocs`,
`local_types`, `invoke_kinds`, `target_classes`, the scope stack and more — all
`bbq_htree`/`bbq_vec`, so all malloc'd OUTSIDE the arena. `test_sir` called
`sema_init` 73 times and `sema_destroy` zero times; eight other suites did the
same. That is ~50 MB abandoned per compile.

Found by sampling the inferior's `VmRSS` at every compile: 1.9 MB at the first,
279 MB by the sixth, 4.0 GB by the eightieth — dead linear accumulation. An
earlier revision of this file claimed the opposite (that one prelude compile
inherently cost 8.4 GB) because it reasoned from `ru_maxrss`, a high-water mark
that cannot fall no matter what is freed. "The peak did not drop when I freed
everything" was never evidence of anything.

## Against the plan's provisional targets

Those numbers were guesses written before anyone knew where the memory was, and
the plan labelled the 1 GB one provisional. Recorded as measurements, not verdicts.

- **Peak RSS per suite:** test_sema 1.09 GB, test_sir 1.17 GB, test_exec 3.23 GB.
  Whole suite 10.25 GB → 3.38 GB. The guessed 1 GB line is close for two suites;
  test_exec is now the outlier and the next place to look.
- **Wall time:** 729 s baseline → 767 s after `sema_destroy` (freeing 31 htrees
  per compile costs time) → 692 s once the prelude was parsed once and shared →
  **535 s** once library bodies stopped being re-checked and re-lowered.

`test_sir` alone, across the whole sequence:

| | wall | peak RSS |
|---|---|---|
| session start | 4:08 | 9.81 GB |
| leaks fixed, arenas torn down | 4:13 | 7.45 GB |
| `sema_destroy` paired | 4:13 | 1.18 GB |
| prelude parsed once | 3:05 | 1.18 GB |
| **library bodies not re-analyzed** | **1:08** | **0.187 GB** |

The last step is `sema_ctx_t.analyze_from` (default 0, so every other caller is
unchanged): user code resolves against the prelude's SIGNATURES, which
`collect_decls` builds, so type-checking ~450 library method BODIES per compile
was pure waste. `analyze_bodies`, the three appending `synth_*` passes and
`compiler_compile` all start from that bound — one bound, one source of truth,
or lowering walks a sema gap.

Nine of test_sir's 73 compile sites keep `analyze_from = 0` and say why: the
escape-analysis and scalar-replacement checks (§43/§44/§46/§47p) read §7
call-graph summaries through the java.lang constructor chain, so they need the
prelude compiled.

`test_exec` is now the whole suite's peak at 3.2 GB, legitimately — it RUNS the
compiled code, so it needs the prelude emitted.

## Recording new baselines

When a phase deliberately changes a number (Phase 3 is expected to move wall
time and RSS, and only those), append a dated section below rather than editing
the table above. The 2026-07-21 numbers stay as the reference point.
