# Third-party material in `compiler/clbg/`

Everything under `clbg/java/` derives from **The Computer Language Benchmarks
Game**, <https://salsa.debian.org/benchmarksgame-team/benchmarksgame>, which is
distributed under the **BSD 3-Clause License**. The reference outputs in
`clbg/ref/` and the published times in `clbg/published.csv` come from the same
repository (`public/download/`, `public/data/data.csv`).

The game's rules bind the **algorithm and the output**, not the source text, so
a port may differ from its published variant where the dialect requires it.
Every such difference is listed below and stated again in the file's own header.
Where a program is a rewrite rather than a transliteration, that is said plainly.

| program | derived from | contributor(s) | changes made here |
|---|---|---|---|
| `binarytrees.java` | `binarytrees-javaxint-2` | — | hoisted the nested `TreeNode` (nested classes are Java 1.1) |
| `fannkuchredux.java` | `fannkuchredux-javaxint-2` | — | none |
| `mandelbrot.java` | `mandelbrot-javaxint-8` | Greg Buchholz (C original) | none |
| `mandelbrot_simd.java` | `mandelbrot-javaxint-8` | Greg Buchholz (C original) | escape iteration carried two pixels at a time in a `javelina.simd` f64x2; algorithm, iteration cap, limit and bit packing unchanged |
| `spectralnorm.java` | `spectralnorm-javaxint-1` | — | `DecimalFormat` (java.text, Java 1.1) replaced by a 9-decimal formatter |
| `spectralnorm_simd.java` | `spectralnorm-javaxint-1` | — | inner products carried in an f64x2; loads are not vectorised (no vector load from a Java array) and the file says so |
| `nbody.java` | `nbody-javaxint-1` | Mark C. Lewis | `NumberFormat` (java.text) replaced by `fmt9`; `new Body[]{…}` replaced by a declared array (JLS 1.0 §15.8 has no ArrayInitializer in a creation expression) |
| `fasta.java` | `fasta-javaxint-2` | Mehmet D. AKIN | hoisted the nested `frequency`; `new frequency[]{…}` replaced by element assignment; `String.getBytes()` (1.1) replaced by the 1.0 four-argument form |
| `revcomp.java` | `revcomp-javaxint-4` | Anthony Donnefort; Razii | hoisted the nested `ReversibleByteArray`, and the complement table with it (a top-level class has no implicit outer reference) |
| `regexredux.java` | `regexredux-javaxint-6` | Francois Green | **rewrite.** The published entry uses `var`, Streams, `CompletableFuture` and `parallelStream`; none is available and the comparison here is against single-threaded rows. Same regexes, same order, same replacements, same output |
| `knucleotide.java` | task description | — | **rewrite.** Every published Java entry is built on the collections framework, generics and streams. Written to the description, using `java.util.Hashtable` per its "built-in or library hash table" rule |
| `pidigits.java` | `pidigits-javaxint-2` | Mike Pall; Java port by Stefan Krause | **algorithm kept, arithmetic replaced.** The published entry calls GMP through JNI (`System.loadLibrary("jgmplib")`); the other Java entry uses `java.math.BigInteger`, which this runtime does not ship. The spigot, the 2×2 matrix and the output format are unchanged; `Big` supplies the five operations the published `GmpInteger` wrapper exposes |

## Reference outputs

`clbg/ref/*-output.txt` are the game's own expected outputs, fetched from
`public/download/`. Each program is verified byte-identical against its
reference in all four configurations (`-O0`/`-O` × interpreter/JIT) by
`clbg/clbg.sh`. `nbody` is compared exactly rather than with the task's
`ndiff -abserr 1.0e-8`, because it matches exactly.

`regexredux-output.txt` is stored here rather than downloaded at run time; it is
the file the task description links, for the 10 KB subject that `fasta` N=1000
generates.

## Fair comparison

`published.csv` keeps only single-threaded rows — `cpu` within 15% of `elapsed`,
and `cpu > 0` — because javelina has no threads. Comparing against a
multi-threaded entry would not be a measurement of the same thing.

Two published rows are worth naming for what they measure rather than what they
are labelled: `pidigits javaxint 2` times GMP through JNI, not Java arithmetic,
and the `spectralnorm` and `mandelbrot` C entries are hand-written SSE2 — which
is why both tasks ship a scalar and a `javelina.simd` row here.
