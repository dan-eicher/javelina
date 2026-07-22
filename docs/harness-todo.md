# Harness work — remaining, audited

Audited against the tree on 2026-07-22, not from memory. Each line says how to
verify it is done. Plan: `~/.claude/plans/brisk-uniting-harness.md`.

Everything in the plan's coverage-preservation table not listed here is closed
and was verified: `exec.h` kept, `test-cli` and `test-bench` in the root gate,
Unity gone, the three orphaned tests folded and removed, vgcores and stale ELFs
gone, `adversarial_corpus` gone, `import_grefsub.wasm` gone.

## 1. water: generate the mnemonic table; drop the runtime TOML and the flag

`wat_assemble(src, len, toml_path, …)` resolves mnemonics by reading
`spec/instructions.toml` **at runtime**. That is why `water` carries
`-i/--instrs`, why ten test binaries carry a file dependency, and why the VM
gates must run with `cd test &&` for the relative path to resolve.

`instructions.toml` is already consumed at BUILD time twice — `gen_wasm_ops.py`
→ the compiler's `wasm_ops.h`, and `gen_trap_reasons` → `jav_trap_reason.h`. The
mnemonic table is the one consumer that stayed runtime. Generate it the same
way.

- [x] Generate a mnemonic→opcode table header from `spec/instructions.toml`
      (`tools/gen_wat_mnemonics.py` → `src/gen/wat_mnemonics.h`, 497 entries)
- [x] `wat_driver` uses it; `toml_path` parameter removed
- [x] `water` becomes standalone; `-i/--instrs` and `DEFAULT_INSTRS` deleted
- [x] Drop the now-unneeded TOML link/dependency from the affected gates
- [x] Verify: run from /tmp with no arguments, emits a module with `\0asm`

Keeps a runtime TOML read: `test_instr`, `test_align` and `test_wat`'s
completeness gate, which walk the table as DATA (every instruction: synthesize →
decode → round-trip). Those are legitimate.

Fell out of this: `test_wast` had built a workaround for the runtime load's cost
("NOT wat_assemble, which reloads instructions.toml per module — 5000× = a
multi-minute hang"); the persistent context and `g_toml_ok` gating are gone. And
`toml_parser.o`/`toml_doc.o` turned out to be the only objects compiled without
`-MMD`, so they never rebuilt on a header change — that stale pair segfaulted
`gen_trap_reasons`, invisible because it only RUNS when relinked.

## 2. jav_module_wf: five of its six checks have no test

Wiring it into `wasm_assemble_program` pins **section order** only — that is what
caught the tag-section bug. Nothing anywhere feeds it a deliberately malformed
module, so these five cannot go red:

- [x] unknown section id
- [x] function and code sections have inconsistent lengths
- [x] data count and data section have inconsistent lengths
- [x] too many locals
- [x] data count section required

Each needs a hand-built `jav_module_t` that violates exactly one rule, asserted
to be rejected with that reason. Verify each RED by removing the corresponding
check.

## 3. `g_wat_excl` is a gauge that cannot move

`test_wast.c:32` declares it, `:463` prints it in the .wat conformance summary as
"N excluded (non-core-3.0)", and **nothing increments it**. The number is
structurally always 0.

- [x] Either increment it where text modules are genuinely skipped, or delete it
      and the word "excluded" from that summary line

## 4. Fixture provenance

These four had sources. They were put in `testsuite/` — the upstream oracle,
which is exactly where our own files must never go — and were lost. Nothing is
recoverable from git: the clone's reflog holds a single `clone:` entry, no local
commits, no dangling objects. They have to be rewritten, in `wasm/test/`.

- [x] Rewrite `.wat` sources for `add.wasm`, `div.wasm`, `div0.wasm`,
      `import_gsub.wasm`, verified by assembling each and comparing to the
      committed `.wasm` byte-for-byte
- [x] State the rule in `wasm/test/README`: every fixture has a source, or a
      written reason it cannot — and OUR cases live in `wasm/test/`, never in
      `testsuite/` ([[feedback_never_touch_the_official_testsuite]])

## 5. compiler/build strays (a P0 item never executed)

Eight files: `javelina.asan`, `javelinac.asan`, `javelinac.msan`,
`javelinac.prof`, `test_exec_asan`, `tcp`, `jre_O.wasm`, `.cov.*`.

- [x] Delete them
- [x] Make sanitizer builds real make targets so they stop being hand-rolled
      artifacts nothing can reproduce

## 6. `wasm/tools/*.ml` provenance

Twelve OCaml files, no README, referenced by nothing in the build. Provenance is
known (Dan, 2026-07-22): they are **from the WebAssembly spec authors** — the
reference interpreter. So they stay; they just need to say so.

- [x] `wasm/tools/README`: these are the spec reference interpreter's OCaml
      sources, kept as the offline oracle that `spec/instructions.toml` was
      derived from (via `gen_instr_toml.py`). Not built, not linked.

## 7. Plan corrections (my errors, recorded so they are not re-derived)

- [x] `gen_wasm_ops.py` is listed in the plan as "unreferenced offline oracle
      tooling". It is not: `compiler/Makefile:99-100` runs it to generate
      `wasm_ops.h`. Same bad sweep that produced the spurious `gen_character`
      item. Plan row corrected.
- [x] Withdraw the "enumerate water's flag surface" P4 item — item 1 deletes the
      flag rather than testing it. Plan item struck with the reason.

## Gate for all of it

Compiler 17/0, VM 90/0, conformance 810/0 binary + 6364/0 text + 60113/0/0
execution, CLI 38/0 — verified by name-diffing the gate lists, not by totals.
