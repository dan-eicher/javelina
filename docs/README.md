# docs/

Engine-facing documentation. Each entry below says what the document *is* and
how current it is, so a reader knows whether it is a live contract or a dated
record before trusting its details.

| document | role | what it is |
|---|---|---|
| [host-abi.md](host-abi.md) | **contract** (current) | The complete set of host imports an embedder must supply for a compiled program to run — the engine's →HOST boundary. This is the authoritative host-ABI reference; the shorter `compiler/docs/host-abi.md` is the compiler-phase view of the same boundary. |
| [test-baseline.md](test-baseline.md) | **record** (snapshot, 2026-07-21) | The test-count / suite-timing / on-disk-size baseline captured at commit `3f04480`. A deliberately frozen reference point — its numbers are as-of that date, not live (the compiler suite has grown since), so read it as a historical parity oracle, not a current tally. |
| [water.md](water.md) | **contract** (current) | WATER(1): the `.wat` ⇄ `.wasm` converter — the two directions' opposite contracts (transcribe-only in, validate-first folded rendering out), the §6.5.11 folding model, §7.7 round-tripping, and the corpus gates that hold each claim. |
