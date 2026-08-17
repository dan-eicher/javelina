# Changelog

## 0.1.0 — 2026-08-16

First release.

### The engine

- WebAssembly Core 3.0, with exception handling, tail calls, the GC proposal
  (struct/array/i31/ref types) and SIMD (v128).
- Four execution levels in one binary, selected per engine at runtime
  (`jav_config_set_jit`, 0–3): interpreter, copy-and-patch JIT, JIT with
  BURG-tiled static stack caching, and JIT with an e-graph equality-saturation
  rewrite ahead of the tiling. One declarative opcode spec generates every one of
  them, so the tiers run identical semantics by construction rather than by
  agreement.
- Immix garbage collector; single-pass §7.6 validator.
- Embedding through the standard [WebAssembly C API](https://github.com/WebAssembly/wasm-c-api):
  `make lib` produces `libjavelina.a`, which with `wasm.h` is the entire surface.
- 60113 execution cases from the official test suite pass at every level, with no
  mismatches. Full table in the README.

### water

- Converts between the §6 text and §5 binary formats in both directions.
  Assembling is transcribe-only, so invalid fixtures reach the engine's validator
  intact; rendering validates first and emits §6.5.11-folded, identifier-carrying
  text that round-trips byte-identically — custom sections and the name section
  included.

### The Java compiler

- Java 1.0 minus `synchronized`, targeting WebAssembly-GC, with a runtime library
  that is real Java compiled to wasm rather than host shims. It exists to stress
  the engine; see the README on why it is here and what it is not.

### Known at release

- The JIT computes SIMD lanes scalar-ly: v128 is correct at every tier, but a
  copy-and-patch stencil must be relocation-free, so the stencils keep the lane
  loops scalar. The interpreter still vectorizes.
- x86-64 and Linux only, and the build requires a compiler with
  `__attribute__((musttail))`.
- No threads, in either the engine or the language the compiler accepts.
