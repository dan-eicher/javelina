# Bibliography — the literature javelina is built from

[THIRD_PARTY.md](THIRD_PARTY.md) is the ledger for *code* javelina borrows, and its
licenses. This is the ledger for the *ideas*: every specification, paper, thesis and
book the implementation answers to, what javelina takes from each, and where in the
tree that lives. Two audiences: someone owed credit, and someone trying to work out
why the pieces fit together — for most of them the answer is that the piece is a
faithful instance of a published construction, and this file says which.

**How to read an entry.** Each names the work, then:

- *Taken* — what javelina actually uses. Where the tree deliberately does **not**
  take something, that is recorded too: a declined mechanism is a design decision,
  not an omission.
- *Lives in* — the files that instantiate it.

Every bibliographic detail below was read off the work's own front matter, or
resolved against the ACM/DBLP record. Page numbers are the work's printed pages.

---

## 1. Normative specifications — the oracles

These are not "influences". Conformance to them is the correctness criterion, and
they are quoted by section number throughout the tree.

### The WebAssembly Core Specification

WebAssembly Community Group; Andreas Rossberg (editor). **WebAssembly
Specification, Release 3.0 (2026-06-03).** Jun 03, 2026.

*Taken:* the whole engine. §2 structure and §4 execution for the interpreter and
every JIT tier; §5 binary format for the reader and for `water`'s assembling
direction; §6 text format — including §6.5.11 folded form — for `water`'s
rendering direction; §7.6's single-pass validation algorithm for the validator;
§2.5.1 index spaces; the GC, SIMD (v128), exception-handling and tail-call
instruction sets. `wasm/spec/instructions.toml` is re-derived on demand by
`gen_instr_toml.py` from the spec's own artifacts — the reference interpreter's
sources joined on opcode with the §7.10 instruction index — and the committed
TOML, not either input, is what the build reads.

*Lives in:* `wasm/` throughout — `src/validate.c`, `src/jav_runtime.c`,
`spec/wasm.def`, `spec/wasm.bbq`, `spec/wat.peg`, `wasm/water`.

*Also:* the [WebAssembly test suite](https://github.com/WebAssembly/testsuite)
(submodule, pinned at `0dc0343`) is the executable form of this specification and
is treated as an untouchable oracle.

### The Java Language Specification

James Gosling, Bill Joy, Guy Steele. **The Java Language Specification.**
Addison-Wesley (Addison Wesley Longman). This first edition — not a modern
one — is the version javelina's compiler implements.

*Taken:* the source language, clause by clause. §3.3 Unicode escapes; §5
conversions and numeric promotion; §14.9 switch layout; §14.19 "can complete
normally"; §15.11.4.4 method dispatch; §15.27 constant expressions; §15.16.3
floating-point remainder; ch. 11 what can be thrown; ch. 20's API chapters for
the runtime library's signatures; §20.11 `java.lang.Math`'s required accuracy.
Coverage is tracked in `conformance/jls-ledger.tsv`.

*Lives in:* `compiler/grammar/`, `compiler/src/compiler/sema.c`,
`const_expr.c`, `type_lattice.c`, `compiler/lib/`.

### The Java Virtual Machine Specification

Tim Lindholm, Frank Yellin. **The Java™ Virtual Machine Specification.**
Addison-Wesley, September 1996 (first edition) — the class-file specification of
the JLS-1.0 era.

*Taken:* §4.3's field/method descriptor syntax, and only that. javelina emits
WebAssembly, not class files; the descriptor grammar is the one piece of the
JVMS it needs.

*Lives in:* `compiler/src/compiler/jtype_meta.c`, `descriptor.c`,
`compiler/include/javelina/compiler/jtype_meta.h`.

### The Unicode Character Database

The Unicode Consortium. **Unicode Character Database** (a Unicode 10.0-or-later
revision; the pinned file in-tree is the copy of record).

*Taken:* character categories and case mappings, generated — never hand-written —
into the runtime library. Licensed under Unicode License v3; see THIRD_PARTY.md.

*Lives in:* `compiler/tools/unicode/UnicodeData.txt` +
`compiler/tools/unicode/gen_character.c` → `compiler/lib/java/lang/CharacterData.java`.

### The WebAssembly C embedding API

WebAssembly/wasm-c-api — the community C embedding API, originated by Andreas
Rossberg. Apache-2.0; vendored, so it is also in THIRD_PARTY.md.

*Taken:* the engine's entire public embedding surface, unmodified.

*Lives in:* `wasm/include/wasm.h`, implemented by `wasm/src/wasm_capi.c`.

---

## 2. The Java front end

### A formal grammar for Java

Jim Alves-Foss and Deborah Frincke. **Formal Grammar for Java.** In *Formal Syntax
and Semantics of Java*, LNCS 1523, Springer, 1999. DOI 10.1007/3-540-48737-9_1.

*Taken:* an attribute grammar for Java (v1.1) derived from the JLS's LALR grammar,
which formalises not only syntax but the static semantics — type checking of
assignments, operands and method parameters, duplicate and undefined names — as
semantic actions on productions. That is the shape javelina's front end has:
grammar file plus attached static-semantic actions, rather than a parser followed
by a separate checker.

*Lives in:* `compiler/grammar/`, `compiler/src/gen/java_parser.c` (generated).

### Parsing expression grammars

Bryan Ford. **Parsing Expression Grammars: A Recognition-Based Syntactic
Foundation.** POPL 2004, Venice, pp. 111–122.

*Taken:* the formalism itself — ordered choice, no ambiguity, recognition rather
than derivation — which is what every `.peg` file in the tree means and what
BBQ's `pegc` generates parsers for.

*Lives in:* `wasm/spec/wat.peg`, `compiler/grammar/`, `BBQ/pegc/`.

### The PEG parsing machine

Sérgio Medeiros and Roberto Ierusalimschy. **A Parsing Machine for PEGs.**
PUC-Rio; Dynamic Languages Symposium, 2008.

*Taken:* the virtual parsing machine and its operational semantics — patterns
compile to programs for a machine rather than to a recursive-descent parser — and,
specifically, the paper's **instruction cost model**, which is the authority its
§4 reasoning rests on. The bytecode *representation* is deliberately not taken.

*Lives in:* `compiler/lib/javelina/peg/PegMachine.java`, `PegCursor.java`.

### Regexes as PEGs

Sérgio Medeiros, Fabio Mascarenhas and Roberto Ierusalimschy. **From regexes to
parsing expression grammars.** *Science of Computer Programming* 93 (2014) 3–18.
DOI 10.1016/j.scico.2012.11.006.

*Taken:* this is the algorithm authority for javelina's `java.util.regex`: the Π
transformation (Fig. 3, p. 8), `empty`/`null` (Fig. 4, p. 11), the
`f_out`/`f_in` well-formedness rewrite (§3.1, Fig. 5), the search optimisation
(§4.3, p. 14), the extensions — independent, possessive, lazy — (§6, Figs. 7–8),
FIRST sets (Fig. 9, p. 17), and Lemmas 3 and 7. Regex is not implemented as a
backtracking matcher; it is *translated* into a PEG and run on the parsing machine
above, which is why the semantics are the paper's rather than folklore.

*Lives in:* `compiler/lib/javelina/peg/RegexToPeg.java`, `RegexParse.java`,
`compiler/lib/java/util/regex/Matcher.java`, `compiler/grammar/peg.asdl`.

### Abstract syntax descriptions

Daniel C. Wang, Andrew W. Appel, Jeff L. Korn, Christopher S. Serra. **The Zephyr
Abstract Syntax Description Language.** DSL'97 (Conference on Domain-Specific
Languages), USENIX, 1997.

*Taken:* ASDL is the language BBQ's `asdl` tool implements, and every `.asdl` file
in javelina — the SIR, the PEG AST, the `water` layout IR, the tier-2 tree — is
written in it. Generated node types come from the schema; they are never
hand-maintained.

*Lives in:* `compiler/grammar/sir.asdl`, `compiler/grammar/peg.asdl`,
`wasm/spec/wat_module.asdl`, `wasm/src/gen/jav_ttree.asdl`, `BBQ/asdl/`.

---

## 3. How the SIR is emitted

### Destination-driven code generation

R. Kent Dybvig, Robert Hieb, Tom Butler. **Destination-Driven Code Generation.**
Indiana University Computer Science Department, Technical Report #302, February
1990.

*Taken:* the code-generation model in full — each node compiled with respect to a
destination and a control context, the dispatcher signature CG : E → ρ → δ → γ →
Code, ρ as the compilation environment, `Lnext` as the next-statement label, the
four binop cases of §3.2 Fig. 8, and continuation threading (p. 13). Two
consequences are load-bearing across the whole compiler: merges and loop
structure are **recorded as they are emitted**, so no later pass rediscovers
control flow, and merge labels are emitted exactly once.

*Lives in:* `compiler/grammar/compiler.ddcg`,
`compiler/src/compiler/codegen_structured.c`, `compiler/docs/ddcg-merge-labels.md`,
and the generator itself in `BBQ/ddcgc/` (whose `dybvig.ddcg` is the paper-direct
shape).

### Why the SIR is a functional term

These five justify the SIR's form — and, in particular, why dominance is recorded
structure rather than something a dominator tree recovers. They are design
authorities, cited in `compiler/docs/combined-analysis-spec.md` §0/§8 rather than
in code comments.

- Andrew W. Appel. **SSA is Functional Programming.** ACM SIGPLAN Notices, 1998
  (Functional Programming column, ed. Philip Wadler).
  *Taken:* the identification of SSA with lexically nested functional terms — the
  reason the SIR can be both.

- Manuel M. T. Chakravarty, Gabriele Keller, Patryk Zadarnowski. **A Functional
  Perspective on SSA Optimisation Algorithms.** COCV'03; ENTCS.
  *Taken:* the SSA↔ANF mapping, and the dominator tree read as scope nesting.
  Also the honest caveat javelina's analyses are written against: their §1 records
  Flanagan et al.'s result that CPS offers no dataflow advantage over ANF, so the
  lattices here are written to the ANF reading, not the "pure CPS" one.

- Cormac Flanagan, Amr Sabry, Bruce F. Duba, Matthias Felleisen. **The Essence of
  Compiling with Continuations.** PLDI 1993; read here in the *20 Years of PLDI
  (1979–1999): A Selection* (2003) reprint, retrospective first.
  *Taken:* A-normal form itself.

- Andrew Kennedy. **Compiling with Continuations, Continued.** ICFP'07, Freiburg.
  *Taken:* CPS as an intermediate language, and what it costs.

- Olivier Danvy, Lasse R. Nielsen. **Defunctionalization at Work.** BRICS Report
  Series RS-01-23, June 2001.
  *Taken:* defunctionalization as the reason the `call_ref` target set is precise —
  the usually-hard part of a call graph, given for free by construction.

---

## 4. The optimizer

### The combined fixpoint

Clifford Noel Click, Jr. **Combining Analyses, Combining Optimizations.** PhD
thesis, Rice University, February 1995.

*Taken:* chapter 4 in javelina's terms, and it is the single largest debt in the
tree. §4.2 partition refinement, which unifies conditional constant propagation,
unreachable-code elimination and global value numbering into one optimistic pass;
ch. 2's optimism (start at ⊤ and descend); §3.3's lattices and §3.6–3.7's
executable edges and mixing function; §4.1.2 control inputs; §4.4.1's TOP-exit
re-arm; §4.4.2 one-time fact initialisation; §4.7's Leader/Follower machinery,
including §4.7.1's once-only Follower⇒Leader transition and §4.7.5's `cprop`
lists; §4.8's revert steps; §4.10's apply-by-walking-the-solved-partitioning; §4.5
ranges and strides; §8/§8.1.1's pointer and memory analysis. The "combining"
thesis — all lattices in *one* fixpoint, never a pass pipeline — is the
architectural rule every later analysis in this file is added under.

Declined, on the thesis's own terms: nothing here rediscovers control flow. Click's
one classic use of dominators (GCM, scheduling the sea-of-nodes back to a CFG) is
not needed because the DDCG recorded the structure on the way in.

*Lives in:* `compiler/src/compiler/sir_optimizer.c` and
`compiler/include/javelina/compiler/sir_optimizer.h`.

### The dataflow substrate

Gary A. Kildall. **A Unified Approach to Global Program Optimization.** POPL 1973
(1st ACM SIGACT-SIGPLAN Symposium).

*Taken:* the worklist to fixpoint, and the meet-at-a-join-point rule.

*Lives in:* `compiler/include/javelina/compiler/analyses.h`,
`compiler/src/compiler/sir_optimizer.c` (backward slot liveness; the φ-meet).

Flemming Nielson, Hanne Riis Nielson, Chris Hankin. **Principles of Program
Analysis.** Springer.

*Taken:* §2.3's meet at join points; §4.2's widening, including the bounded
widening-chain argument that pins how many steps a lattice may take per vnode;
ch. 6's Condition Propagation, which is what javelina's path-sensitive refinement
(per-edge Refines) *is*.

*Lives in:* `compiler/src/compiler/sir_optimizer.c`,
`compiler/include/javelina/compiler/jbound.h`.

### Escape analysis

Jong-Deok Choi, Manish Gupta, Mauricio Serrano, Vugranam C. Sreedhar, Sam Midkiff.
**Escape Analysis for Java.** OOPSLA '99, Denver, CO.

*Taken:* the connection graph (Def. 2.4 — object/reference/field/static nodes;
points-to, deferred and field edges; 1-limited naming per `new`), Fig. 6's
intraprocedural statements and Fig. 7's interprocedural ones — including
statement 32's caller-created object nodes and §4.4's MapsTo rule quoted verbatim
in the code — §4.2's summary computed from the connection graph at method exit,
§4.5's bottom graph for unanalysable and native callees, and §4's
reverse-topological iterate-to-convergence over the call graph. Explicitly
declined, and recorded as such: SCC/Tarjan and dominators — §7's bottom methods
already handle cycles, and "we ignore back edges" falls out of the traversal
state.

*Lives in:* `compiler/src/compiler/sir_optimizer.c` (the `cp_summarize` chain),
`compiler/include/javelina/compiler/compiler.h`, `sir_optimizer.h`,
`compiler/src/compiler/wasm_module.c`.

### Pointer analysis

Lian Li, Cristina Cifuentes, Nathan Keynes (Oracle Labs). **Precise and Scalable
Context-Sensitive Pointer Analysis via Value Flow Graph.** ISMM'13, Seattle.

*Taken:* the value-flow-graph formulation, folded into the Click fixpoint as
further lattice elements rather than run as a separate analysis. The mapping is
worked out in full in `compiler/docs/combined-analysis-spec.md` §0: the
sea-of-nodes over the SIR already *is* the VFG — object nodes are allocation
nodes, assignments do not exist (copies are subsumed), value-flow edges are def-use
edges — so the only real addition is memory-SSA over `struct.get/set` and
`array.get/set`.

*Lives in:* `compiler/docs/combined-analysis-spec.md`,
`compiler/src/compiler/sir_optimizer.c`.

### Scalar replacement

Lukas Stadler, Thomas Würthinger, Hanspeter Mössenböck. **Partial Escape Analysis
and Scalar Replacement for Java.** CGO'14, Orlando, FL.

*Taken:* the consumer side — what a NoEscape result is *for*. Scalar replacement
of an object's fields by local values, applied on the compiler's IR rather than on
bytecode.

*Lives in:* the S6/PEA work in `compiler/src/compiler/sir_optimizer.c`.

### Strong updates

The authority for when a store may kill rather than merge: the strong-update gate
demands one concrete, most-recent abstract object (`cp_obj_is_concrete`, the
concrete-singleton condition of `combined-analysis-spec.md` §2), and arrays stay
weak per Chase–Wegman–Zadeck, cited at that site in `sir_optimizer.c`.

- Ondřej Lhoták, Kwok-Chiang Andrew Chung. **Points-to Analysis with Efficient
  Strong Updates.** POPL 2011, Austin, TX.
- David R. Chase, Mark N. Wegman, F. Kenneth Zadeck. **Analysis of Pointers and
  Structures.** PLDI '90, June 1990.

---

## 5. Bounds-check elimination

### The constraint semantics

Rastislav Bodík, Rajiv Gupta, Vivek Sarkar. **ABCD: Eliminating Array Bounds
Checks on Demand.** PLDI 2000, Vancouver, BC.

*Taken:* the constraint *semantics*, not the algorithm's machinery. Table 1 (p. 6)
and Definition 2 / Eq. 1 (pp. 5–6) are the authority for what facts exist; §3's
π-nodes are javelina's per-edge Refines; φ vertices as MAX nodes bounded by the
weakest in-edge is the φ-meet with bound canonicalisation; §7.1's on-demand
consultation of value numbering becomes a *recorded premise that re-arms on split*,
because javelina's partitions are optimistic mid-solve where ABCD's numbering is
final; §7.3's Java aliasing argument ("no statement can change the size of an
array") licenses the arraylength-per-array value numbering.

Declined, with the paper's own reasons: the demand-driven traversal prover (Fig. 5)
compensates for a JIT budget javelina does not have — it runs an exhaustive
fixpoint, and a demand-time graph walk here would be the forbidden re-discovery.
§6's PRE of partially redundant checks is out on grounds §6.2 concedes: traps
cannot move, and JLS precise exceptions pin the guard's location.

*Lives in:* `compiler/src/compiler/sir_optimizer.c`,
`compiler/include/javelina/compiler/jbound.h`.

### Why the no-motion fence is total

Rastislav Bodík, Rajiv Gupta, Mary Lou Soffa. **Complete Removal of Redundant
Expressions.** PLDI 1998; read here in the *20 Years of PLDI (1979–1999): A
Selection* (2003) reprint, with the authors' retrospective at pp. 596–597.

*Taken:* the negative result that makes the fence principled rather than
conservative. Complete PRE needs code motion, speculation, or restructuring; the
retrospective states that side-effecting expressions "such as exceptions" block
speculation, leaving only restructuring (path duplication / do-until conversion) —
which is itself out in this compiler. So guards are never moved, and no
compensation checks are ever inserted.

### Array-content invariants

Mikel Luján, John R. Gurd, T. L. Freeman, José Miguel. **Elimination of Java Array
Bounds Checks in the Presence of Indirection.** *Concurrency and Computation:
Practice and Experience* 16:1 (2004), John Wiley & Sons.

*Taken:* the `foo[B[k]]` indirection shape — checking `foo` requires the *contents*
of `B` to be bounded — and the paper's answer of making the library carry the
invariant in specially shaped, one-class-at-a-time-verifiable classes (§4.1
immutability via private + final + non-escaping; §4.2 a synchronized
mutable/immutable state). Design authority for the cross-field invariant tier; a
planning source, with no in-code citation yet.

### Read as a contrast

Matias Demare, Guillermo Polito, Nahuel Palumbo, Javier Pimás. **PiNodes in the
Druid Meta-Compiler.** IWST 2025 (International Workshop on Smalltalk
Technologies), Gdańsk. CEUR-WS Vol-4139.

*Taken:* nothing — it is cited as the counter-example that justifies *not*
reifying constraints into the IR. They make the π a real instruction and pay the
full compensation kit (insertion, dominance renaming, breaking critical edges,
deletion before lowering, §3.2), still lose constraints at merges (§6), answer
queries by per-π use-def walks, and report that ABCD-alone grew the IR until stack
spills crashed their VM (§5.3). Same constraint content as javelina's per-edge
Refines, at that price.

---

## 6. Instruction selection

The tree's matchers are BURS matchers, generated by BBQ's `burgc` from `.burg`
rule files. Four papers, in dependency order.

### The dynamic-programming formulation

Alfred V. Aho, Mahadevan Ganapathi, Steven W. K. Tjiang. **Code Generation Using
Tree Matching and Dynamic Programming.** ACM TOPLAS 11(4), October 1989.

*Taken:* the tree-translation-scheme model (twig) that generated matchers
implement: tree pattern matching combined with dynamic programming to select a
least-cost cover.

*Lives in:* `BBQ/burgc/` (stated in its README as the model the generated matchers
implement).

### The cost model and goal-directed reduction

Christopher W. Fraser, David R. Hanson, Todd A. Proebsting. **Engineering a
Simple, Efficient Code Generator Generator.** ACM Letters on Programming Languages
and Systems 1(3), September 1992, pp. 213–226. (This is `iburg`; cited in the tree
as FHP91.)

*Taken:* two rules the tree treats as non-negotiable. **Costs must be the truth** —
p. 4, "each C sums the costs of the non-terminals on the right-hand side", and
p. 7 Fig. 4's `state()` — so every child is costed through the non-terminal it is
demanded as, not through whatever it happens to reduce to. And **goal-directed
reduction** — p. 8's `rule(state, goalnt)` — is the formalism for entry into a
cover at a demanded non-terminal.

*Lives in:* `compiler/grammar/codegen_wasm.burg`, `wasm/src/gen/jav_tile.burg`,
`BBQ/burgc/`.

### The automaton

Todd A. Proebsting. **BURS Automata Generation.** ACM TOPLAS 17(3), May 1995,
pp. 461–486. (A preliminary version appeared at PLDI 1992.)

*Taken:* the table-generation algorithm — §3.3's representer states and triangle
trimming, Fig. 5/11/12's `Project()` and `ComputeTransitions()` worklist, and
p. 464–465 Fig. 4's `RuleTable(state, goal)` reduction. This is what keeps the
generated automaton small enough to build; a test in `burgc` pins that the build
scales like this algorithm rather than like naive tuple enumeration.

*Lives in:* `BBQ/burgc/src/completeness.cpp`, `completeness.h`, and its tests.

### The state-minimisation ancestor

David R. Chase. **An Improvement to Bottom-Up Tree Pattern Matching.** POPL '87
(14th ACM SIGACT-SIGPLAN Symposium), Munich. DOI 10.1145/41625.41640 — cited
through Proebsting §3.3, whose representer-state construction is "after Chase".

### BURS theory

Eduardo Pelegrí-Llopart, Susan L. Graham. **Optimal Code Generation for Expression
Trees: An Application of BURS Theory.** POPL '88 (15th ACM SIGACT-SIGPLAN
Symposium). DOI 10.1145/73560.73586 — the tree's PLG88, which reaches javelina
through Ertl's citation of it (see below) as the tree-pattern-matching formulation
that makes lookahead-aware code generation linear-time.

### Graph-based selection — consulted, not adopted

Sebastian Buchwald, Andreas Zwinkau. **Instruction Selection by Graph
Transformation.** CASES'10, Scottsdale, AZ.

*Taken:* Definition 4's φ-splitting, and only that. PBQP itself — the Partitioned
Boolean Quadratic Problem formulation the paper builds on — is deliberately out of
scope; javelina's selection stays a tree-matching BURS.

---

## 7. The engine

### The in-place interpreter

Ben L. Titzer. **A Fast In-Place Interpreter for WebAssembly.** *Proc. ACM Program.
Lang.* 6, OOPSLA2, Article 148 (October 2022), 27 pages. DOI 10.1145/3563311.

*Taken:* interpreting the wasm bytes *in place* — no rewrite, no second internal
format — which is only possible given the side-table §3.1 and Listing 1 describe:
one ⟨Δip, Δstp, vals, pop⟩ entry per branch, distilled during validation, plus
`doControlTransferFromSTP` on a taken branch. §3.2.1's unboxed reference
representation, and §3.3.3's per-opcode debug probe, are taken too. The side-table
falls out of the validator's own control frames, so validation and the table are
one single pass.

*Lives in:* `wasm/src/validate.c`, `wasm/src/jav_frame.h`, `wasm/src/jav_runtime.c`,
`wasm/src/jav_extern.h`, `wasm/src/wasm_capi.c`.

### The JIT

Haoran Xu, Fredrik Kjolstad. **Copy-and-Patch Compilation: A fast compilation
algorithm for high-level languages and bytecode.** OOPSLA 2021;
arXiv:2011.13127v3, 15 Sep 2021.

*Taken:* the whole tier-1 strategy — stitching pre-compiled binary stencils with
holes patched at code-generation time — including the constraint (p. 7) on what
can and cannot be a baked immediate, which decides where a loop variable may ride.
javelina does the copy-and-patch itself: the stencils are generated from the same
declarative opcode spec that generates the interpreter, so both tiers execute
identical semantics by construction.

*Lives in:* `wasm/src/jit_driver.c`, `wasm/src/gen/`, and the generator and stencil
extractor in `BBQ/opgen/` and `BBQ/jitterator/`.

### Stack caching

Martin Anton Ertl. **Implementation of Stack-Based Languages on Register
Machines.** Dissertation, Technische Universität Wien, April 1996.

*Taken:* tier 2, essentially entire. §2.3's **minimal** cache organisation — one
state per *number* of cached items, n+1 states — with the paper's own justification
for minimal at small n; §2.5's control-flow convention, which is the construction
rather than a pass over it; §3.2.1's refunctionalization, pushing a *node pointer*
onto the abstract stack instead of a value, which is what the tier-2 tree builder
is; and §2.6 (p. 36) the **cost model** — cycles, weighted 1 for a memory
access — which is the currency extraction prices in, rather than bytes. Ertl's own
Forth measurements (7–11% for one cached item; the curve turning over as extra
states multiply) are recorded alongside javelina's numbers, and the tree records
where the analogy is imperfect: Ertl's transition cost is a mispredicted branch on
an R3000, javelina's is a `musttail` in a copy-and-patch stencil.

*Lives in:* `wasm/tools/gen_tile_burg.c`, `wasm/src/gen/jav_tile.burg`,
`wasm/src/jav_ttree.c`, `jav_ttree.h`, `wasm/src/jit_driver.c`,
`wasm/src/jav_extern.h`.

### Equality saturation

Ross Tate, Michael Stepp, Zachary Tatlock, Sorin Lerner. **Equality Saturation: A
New Approach to Optimization.** *Logical Methods in Computer Science*; an earlier
version appeared at POPL 2009.

*Taken:* tier 3's premise. The representation only ever **grows** (§1.3) — nothing
is deleted, which is what makes rewriting order-independent and no rule able to
disable another. That single property is why the phase-ordering hardening the
Click optimizer needs (re-arm on the fact, phase rules) is out by construction
here.

Max Willsey, Chandrakana Nandi, Yisu Remy Wang, Oliver Flatt, Zachary Tatlock,
Pavel Panchekha. **egg: Fast and Extensible Equality Saturation.** *Proc. ACM
Program. Lang.* 5, POPL, Article 23 (January 2021), 29 pages. DOI 10.1145/3434304.

*Taken:* the engineering that makes it practical — **e-class analyses** (§4.1),
whose `make`/`join`/`modify` interface is implemented directly, so constant
folding is an analysis that puts the constant in the class rather than a rewrite
rule per operator pair; and §4.3's AST-size tiebreak as the extraction default.
Termination stays the caller's responsibility, as the paper has it.

*Lives in:* `wasm/src/jav_eqsat.c`, `wasm/src/jav_eqsat.h`,
`BBQ/burgc/runtime/egraph.c`, `egraph.h`.

---

## 8. The garbage collector

Stephen M. Blackburn, Kathryn S. McKinley. **Immix: A Mark-Region Garbage
Collector with Space Efficiency, Fast Collection, and Mutator Performance.**
PLDI'08, June 7–13, 2008, Tucson, AZ.

*Taken:* the mark-region collector — allocate and reclaim contiguously at block
grain when possible and at line grain otherwise — plus §3.2/§3.2.1's opportunistic
defragmentation, including the mark state that block selection depends on and that
`clear_marks` therefore deliberately does not clear, and §3.2's headroom of free
blocks the allocator cannot see. A large-object space sits alongside it.

*Lives in:* `wasm/src/immix/` — `immix_space.c`, `immix_block.c`,
`immix_line_map.c`, `immix_block_allocator.c`, with the wasm-facing shell in
`jav_gc.c` / `jav_gc.h`. The C here is a port of the C++ Immix in **AiPL** (below),
with per-type `mark()` replaced by generated tracing.

---

## 9. Ported code and in-house ancestry

Licenses for the first two are in [THIRD_PARTY.md](THIRD_PARTY.md); the rest are
sibling projects of this author's, credited here because they are where several
pieces actually came from.

- **Sun fdlibm** — the transcendental implementations, ported routine by routine
  (`e_exp`, `e_pow`, `e_log`, `s_sin`/`s_cos`/`s_tan` with `__ieee754_rem_pio2`
  and the Payne–Hanek `__kernel_rem_pio2`, `e_asin`/`e_acos`/`e_atan`/`e_atan2`,
  `e_fmod`, `e_remainder`, `copysign`, `scalbn`). `f32.rem`/`f64.rem` do not exist
  in wasm, so Java's `%` on floating point desugars to a call to fdlibm `fmod`.
  *Lives in:* `compiler/lib/java/lang/Math.java`.

- **OpenJDK** — `FloatingDecimal` (both directions), `ASCIIToBinaryBuffer`,
  `FDBigInteger`, `FdLibm`'s word-access helpers. The binary→decimal path is
  Dragon-style shortest-digit generation with `FDBigInteger` for the hard case.
  *Lives in:* `compiler/lib/java/lang/{FloatingDecimal,BinaryToASCIIBuffer,ASCIIToBinaryBuffer,FDBigInteger}.java`.

- **AiPL** — the author's C++ CEK machine, and the origin of the Immix engine:
  `wasm/src/immix/*` is a pure-C port of its `include/immix/` and `src/immix/`.

- **yoctojc** — the author's Java Card compiler, javelina's direct ancestor. Its
  `patterns/codegen.burg` and `grammar/compiler.ddcg` are the working analogs
  several stages of this compiler were built against.

- **Java Card 2.1 Virtual Machine Specification.** Sun Microsystems, Final
  Revision 1.0, March 3, 1999. The spec yoctojc implemented; its 16-bit-cell
  envelope is an ancestry hazard rather than an authority here, and citations to
  it have been purged from live code — only lineage comments remain.

- **Peggy** — the author's C PEG virtual machine (`peg-vm.c`), used as a
  cross-check reference for LPeg opcode semantics while building the PEG machine,
  not as a port target. LPeg itself is Ierusalimschy's, and is the tool the two
  Medeiros papers above describe.
