# conformance/gen — the stitching generator

A Java 1.0 program that writes Java 1.0 programs, together with the exact stdout each one
must produce. It is compiled by the shipped `javelinac` and run by the shipped `javelina`,
like everything else in `conformance/`:

```sh
javelinac --libdir compiler/lib/java -O0 conformance/gen -o gen.wasm
javelina  --jre compiler/build/jre.wasm --root <parent> gen.wasm <outdir> [depth] [cap] [perCase]
```

It writes `Case<N>.java` / `Case<N>.expected` pairs and a `CAP-DROPS.txt` into `<outdir>`.

## The contract

```java
interface Snippet {
    String   id();                     // stable dotted id, e.g. "conv.widen.int2long"
    String[] sections();               // JLS sections exercised, e.g. {"5.1.2"}
    String   type();                   // Java type its expression yields; "void" = statement
    String[] holeTypes();              // required type of each hole (empty array = leaf)
    String   render(String[] holes);   // the Java source text, holes already rendered in
    Val      expect(Val[] holes);      // the expected value, COMPOSED from the holes'
}
```

Two properties carry the whole design, and both are structural rather than checked
afterwards.

**Validity is an invariant of the generator.** A hole typed `"int"` is only ever filled by a
Snippet whose `type()` is `"int"`. `Stitching`'s constructor enforces it at the point the
tree is built, so every stitched program type-checks *by construction*.

**The expectation is composed, never observed.** `expect()` folds up the hole tree. Nothing
in this directory runs a generated case to learn an answer. A generator that had to execute
the case would produce a snapshot of whatever `javelinac` does today — including a
miscompile — and a snapshot is not an oracle.

### A generated case that fails to compile is a bug — never an expectation

It is a bug in the generator (a snippet rendered ill-typed source, or declared a `type()` it
does not yield) or a bug in `javelinac`. There is no third possibility and no allowance for
one. Do not "fix" it by deleting the snippet, narrowing its inputs, or adding a case to the
enumerator: that routes around the defect and buys silence. Minimise it, and pin it at the
level that owns it.

The first generation off this engine did exactly that — see **What it has found**, below.

## Val — the composed value

`Val` carries a kind, the payload, and the Java type, because the printed form depends on
the type as well as the kind:

| kind | Java types | `display()` |
|---|---|---|
| `LONG` | byte, short, char, int, long | `Long.toString`, or the character itself for `char` |
| `DOUBLE` | float, double | `Float.toString` or `Double.toString` — **different functions** |
| `BOOLEAN` | boolean | `"true"` / `"false"` |
| `STRING` | String | the text, or `"null"` |
| `REF` | any other reference | the value's `println` text |
| `THROWS` | — | the exception's fully-qualified class name |

`display()` is exactly the bytes `System.out.println` emits, because that string *is* the
`.expected` line.

`float` and `double` share a kind but not a printing rule (`Float.toString(0.1f)` is `"0.1"`;
`Double.toString` of the same value is `"0.10000000149011612"`), and so do `char` and `int`.
`Stitching.expect()` therefore rejects a snippet whose `LONG`/`DOUBLE`/`BOOLEAN` Val does not
carry the type the snippet advertises. **Do float arithmetic in `float` locals** and only
then call `Val.ofFloat` — computing in double and narrowing at the end rounds once where Java
rounds twice.

For `STRING` and `REF` the Val's `type()` is the value's **run-time class**, or `"null"` for
the null reference (§4.1). That is what a §5.1.5 cast snippet needs in order to decide
whether the cast throws, and it is legitimately narrower than the snippet's static type, so
it is not type-checked.

`THROWS` is a value like any other. A stitching that throws has a known outcome; its
`display()` is the fully-qualified class name, which is what `Class.getName()` returns
(§20.3.2) and what the emitted `catch` clause prints.

## Adding a snippet library

A library is a class with one static method:

```java
public class ConvSnippets {
    public static void install(Registry r) {
        r.register(new SnWidenByteToInt());
        // ...
    }
}
```

and one line in `GenMain.main`:

```java
BootSnippets.install(reg);
ConvSnippets.install(reg);      // <- here
```

Nothing scans and nothing reflects, so the enumeration is a function of this list and of each
library's registration order — which is why two runs write byte-identical files.

Java 1.0 has no anonymous or nested classes, so each Snippet is a package-private top-level
class in the library's file. `BootSnippets.java` is the worked example; copy its shape.

### Rules a snippet must obey

- **`render()` returns a parenthesised expression**, so it composes into any operator without
  precedence surprises — or, when `type()` is `"void"`, a complete statement.
- **A `"void"` snippet prints exactly one line itself, or throws before printing anything.**
  `Emit` does not wrap it in a `println`; its expected line is its `expect().display()`.
- **`expect()` composes from the holes and from the cited rule.** Where the spec states a
  result outright, state it — do not re-derive it by performing the very operation
  `javelinac` is under test for. `arith.mul.int` computes in `long` and truncates to the low
  32 bits (§15.16.1); `arith.div.int` names the `MIN_VALUE / -1` overflow (§15.16.2).
- **Propagate `THROWS` deliberately.** `Val.firstThrow(holes)` is the left-to-right rule
  (§15.6) for strict operators. Short-circuiting operators (§15.22, §15.23, §15.24) must
  *not* use it — deciding operand by operand is the rule they exist to exercise.
- **Cite a section.** `Registry.register` rejects a snippet with an empty `sections()`; the
  coverage join reads the `// JLS <section>` markers `Emit` derives from them.
- **`sections()` and `holeTypes()` are built with `Strs.of(...)`.** JLS 1.0 §15.9 array
  creation has no initializer form — `new String[]{...}` is Java 1.1 syntax and `javelinac`
  correctly rejects it.
- **The rendered source must be ASCII.** Render a non-ASCII character as a `\uXXXX` escape
  (§3.3, which `javelinac` handles in-grammar). `Emit` fails at generation time rather than
  leaving the compiler to trip over it.

## The cap, and its record

`Stitcher` takes a per-`(type, depth)` cap so the case count stays finite. **A truncated
enumeration that says nothing reads as full coverage**, so every cut is written to
`CAP-DROPS.txt` beside the cases — the type, the depth, how many stitchings were enumerable,
how many were kept, and what each snippet kept of what it offered. The file is written even
when nothing was cut, because the absence of a record is indistinguishable from a complete
enumeration.

The cap is a **fair share, not a prefix**. Truncating the concatenated list gives the whole
budget to whichever snippet was registered first: at depth 2 the boot library's
`arith.mul.int` took all 250 of `int`'s slots and `arith.div.int` — which holds every
`ArithmeticException` case — contributed **zero**. A snippet's coverage must not depend on
where in `install()` it happens to sit, so the budget is water-filled (equal share up to a
level, snippets offering less keep all they have, remainder round-robin in registration
order).

## Determinism

Fixed, and verified byte-identical across repeated runs and across execution tiers:

1. snippets are visited in **registration** order;
2. a snippet's holes are filled in **odometer** order, hole 0 varying slowest — the last hole
   is the inner loop;
3. when the cap bites, each snippet's quota is filled from the **front** of that order.

Nothing iterates a `Hashtable`; the memo table is only ever looked up by an explicit key.

## Encoding

The `.expected` file is a byte-for-byte model of stdout, so it is written the way
`PrintStream` writes: **each char as its low 8 bits**, per
`java.io.PrintStream.writeString` — *"Each char is written as its low 8 bits (JLS 1.0
§22.14 — byte streams, no charset)"*. UTF-8 here would silently disagree with the program for
every char above `0x7F`, and the boot library already produces those: `((char)(int))`
narrowing yields 29 lines containing NUL and 9 lines with a byte above 127 in a single
879-line run.

## What it has found

The first generation off this engine produced two `javelinac` defects, both caught by the
compiler's own fail-loud gates rather than by a wrong answer:

- **A cast of the `null` literal never types its spill slot.** `(T)null` for any reference
  `T` — `System.out.println(((Object)null));` is enough — fails assembly with
  *"body-local slot N holds a reference whose type was never threaded through its DDCG
  destination"*. `emit_ref_cast` (`compiler/grammar/compiler.ddcg`) spills the operand with
  `locref(t, expr_ref(op))`, and `expr_ref` is nil for a `null` literal — exactly the case
  `chain_call_args`, five lines below, already handles for arguments: *"a `null` literal has
  none (expr_ref = nil) — then the slot is still a param-type reference, so type it from the
  param (the (dcl, cp) authority)."* The cast has the same authority available in its target
  type and does not consult it. `-O0` and `-O` alike.
- **`-O` emits an invalid module when a divisor folds to zero.**
  `System.out.println((0) / ((0) / (-3)));` compiles at `-O0` and fails at `-O` with
  *"emitted module FAILS validation: type mismatch"*. `(0)/(0)` written directly is fine, so
  it needs the divisor to become zero by folding.

Neither is reachable from a hand-written kernel in `conformance/src`; both fall out of an
enumeration in the first six case files.
