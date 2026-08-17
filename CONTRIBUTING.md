# Contributing

The short version: build it, run the gate, and read the rules below — most of
them are about *where* a change belongs, and they are not guessable from the
diff you are about to write.

```sh
git submodule update --init
make all
make test          # the whole gate
```

While iterating, run the suite you are affecting rather than the whole thing —
`make test-one T=test_gc_los`, `make test-vm`, `make test-compiler` — and the
full gate once before you propose the change.

## Generated code is not edited

Most of this tree is generated. The opcode semantics, the interpreter, every JIT
stencil, the `.wat` reader, the readers and writers, the Java grammar's parser
and the Unicode tables all come out of declarative sources:

| you want to change | edit |
|---|---|
| opcode semantics, any tier | `wasm/spec/wasm.def` |
| the module container | `wasm/spec/wasm.bbq` |
| the `.wat` text grammar | `wasm/spec/wat.peg` |
| the renderer's layout IR | `wasm/spec/wat_module.asdl`, `instructions.toml` |
| the Java grammar, AST→SIR, codegen | `compiler/grammar/` |
| the Unicode character tables | `compiler/tools/unicode/gen_character.c` |

Anything under a `src/gen/` directory is output. If output is wrong, the
generator is wrong; a hand-edit there survives until the next build and then
silently disappears, which is worse than the bug.

## `spec/` carries no work markers

`TODO`, `FIXME`, `XXX` and `HACK` are gated out of `wasm/spec/` (a leg of `make
test-vm`). Those files are the contract every generated form is derived from, so
a comment in them is read as a statement about the system. Two sat in `wat.peg`
claiming type deduplication and `(rec …)` parsing were unwritten, long after both
worked — each an invitation to reimplement something already there.

Work that has not happened belongs in a plan where it can be scheduled. Work
that has belongs in a comment that says what the code does.

## The testsuite is an oracle, not a fixture

`testsuite/` is the official WebAssembly conformance suite, pinned as a
submodule. It is never edited, and our own tests never go in it — a suite you can
adjust is not measuring anything. Regression fixtures live beside our tests
(`wasm/test/regress_*.wast`).

The same applies to a red test generally: a failing test is evidence, and the
fix goes in the code it is testing. Tests marked expected-to-fail stay red until
the thing they describe actually works.

## A spec claim names its spec

javelina answers to four or five authorities in the same files — the JLS, the
WebAssembly core spec, Unicode, IEEE 754, and the papers in
[BIBLIOGRAPHY.md](BIBLIOGRAPHY.md). A bare `§4.7` in a comment is ambiguous
between them, so write `JLS §4.7` or `WASM §4.7`. `conformance/check-deferrals.sh`
fails the gate on an unattributed one, and reconciles JLS citations against
`conformance/jls-ledger.tsv`: a comment cannot defer a section the ledger claims
as covered.

If you add behaviour the spec dictates, cite the clause. If you cannot find the
clause, that is worth knowing before the code is written.

## Commits

One cohesive body of work per commit — not one per file, per step, or per
review comment. The message says what is now true, in the present tense, and why
the change was necessary if that is not obvious from the diff.

## Scope

The engine is the artifact. The Java compiler exists to stress it (see the
README) — improvements there are welcome when they exercise the engine harder or
fix something wrong, but it is not on its way to being a production Java
toolchain, and it is not the place to add language features for their own sake.

Threads are out of scope in both: the engine has no shared memory or atomics,
and `synchronized` and JLS §20.15–21 are deliberately unimplemented.
