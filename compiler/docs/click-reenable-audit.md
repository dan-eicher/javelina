# Click re-enable audit — JCVM-envelope + shortcut checklist

Systematic audit of the Click machinery before wiring it back into the compile pipeline,
against the full Java-1.0 (−`synchronized`) + WASM-GC value model. Ran three parallel
file auditors over `sir_optimizer.c` (Click proper), `sir_op_gamma.c` (γ opcode metadata /
type accessors), `type_lattice.c`+`jtype_meta.c` (the representation authority), and
`analyses.c` (the sema-side *diagnostic* lattices — adjacent, not Click). Each finding was
graded by tracing its consume path, not by pattern-match alone.

## Framing

**Click is not "broken" — it is unplugged.** `sir_optimize` (`sir_optimizer.c:4265`) is
invoked *only* by tests (`test_sir.c` opt-gated, `test_click_backend.c`); nothing in
`compiler.c` / `compiler_compile.c` / `wasm_module.c` / `codegen_structured.c` calls it. So
**re-enable = wire `sir_optimize` into the driver between SIR construction and codegen** —
and this audit is the correctness gate, because its unit tests pass on what they cover while
the JCVM residue hides on untested full-Java breadth.

**Headline verdict: the core is honest.** The feared failure — Click value-numbering `SIR_DTREF`
refs *flat* (ignoring GC type) — is **absent**: congruence and meet consult the lattice
(`type_meet` `sir_optimizer.c:1408`, `cp_split_by_facts:2619`, `sema_is_subclass_of:2839`),
ref constants use allocation identity (`:1476`). `long` is first-class 64-bit everywhere (no
2-cell), `byte/short/char/int` compute as i32 with narrowing only at explicit casts, switch
is fully general, and `type_lattice.c` covers the full kind set + GC-ref model. The JCVM
residue is small and localized.

---

## MUST-FIX — live correctness bugs (before wiring in)

- [x] **`sir_optimizer.c:4038` — JavaCard "int = 2 cells" in the slot-pack anchor.**
  `ac += (sm->param_types[i].tag == JT_INT) ? 2 : 1;` counts each `int` param as two cells
  and `long`/`double` as one — the JCVM 16-bit-cell envelope, and it **contradicts this
  file's own width-1 pool model** (`POOL_WIDTH(p) == 1`, `:4086`). `args_cells` is the anchor
  that pins parameter slots during bin-packing (`:4030,:4044,:4099`), so any method with
  int/long/double params gets a wrong param/packed boundary → packed locals laid over a param
  cell or a gap. **Fix:** count every param as one cell. This bites the instant the packer runs.

- [x] **`sir_optimizer.c:4207/:4245` — fixed `sir_node_t* stack[4096]` with silent-truncating guard.**
  The slot-rename DFS pushes under `if (sp<4096)` and `for (…; sp<4096; …)`; a method whose
  expression DAG exceeds 4096 pending nodes silently drops `LoadLocal` renames → stale /
  out-of-range local indices after `method->max_locals = total_cells` (`:4250`). Correctness,
  not precision; unlogged; and inconsistent with the `bbq_vec` DFS stacks the same file uses
  elsewhere (`:160,:3900`). **Fix:** growable `bbq_vec`.

## FIX-BEFORE-WIRING — latent (becomes real once a width/narrow reads the γ Type)

These are precision-lattice facts today (γ `Type*` feeds congruence only; range width and
stackmap come from the SIR datatype, which is i32) — **but the combined-analysis type element
(spec §3) derives from exactly this γ/lattice Type**, so a `short` arraylength would poison
it. Fix as a prerequisite to the analyses, not just to re-enabling Click as-is.

- [x] **`sir_op_gamma.c:696` — `SIR_ARRAYLENGTH → GT_PRIM_SHORT`.** `arr.length` is `int` (to
  `INT_MAX`) in full Java; this is the textbook JavaCard 16-bit-length holdover. → `GT_PRIM_INT`.
- [x] **`sir_op_gamma.c:637-668` (7 sites) — `instanceof` + the six comparisons typed
  `GT_PRIM_SHORT`.** Value-safe (0/1) but kind-wrong and inconsistent with this compiler's own
  `JT_BOOL→SIR_DTBYTE`. → int (JVM `instanceof`/compare push int) or the byte-boolean convention.

## HARDEN — fail-open → fail-closed / logged (no silent degradation)

- [x] **`sir_optimizer.c:2068/:2079` — fixpoint runaway valves print to stderr and `break`.**
  If they fire, `cp_propagate` exits before convergence, leaving facts non-monotone/unsound —
  a silent-degradation escape hatch, not a hard error. The surrounding comments claim
  termination is guaranteed, so make it fail-**closed** (assert/abort), not stderr-and-continue.
- [x] **`sir_optimizer.c:43` — `class_id` packed into 14 bits (`& 0x3FFF`), unlogged.**
  >16383 classes collide cell keys → over-invalidation (conservative, sound) but an unlogged
  hardcoded cap. Log the truncation or widen the key.

## PRECISION / SUBSET — sound today, improve for the combined analysis

- [x] **`sir_op_gamma.c:419-426` — `GT_PRIM_ARRAY` newarray accessor covers only
  BOOL/BYTE/SHORT/INT; char/long/float/double fall to BOTTOM.** `char[]` is ubiquitous (String).
  Sound (BOTTOM = no info) but incomplete — completes the type element's array precision.
- [x] **`type_lattice.c` `TK_PRIM_ARRAY` carries width but no dim** → `int[][]` has no distinct
  lattice Type (collapses to BOTTOM). Sound precision loss; the real authority is
  `lat_array_overlay_class`.
- [ ] **`sir_optimizer.c:1722` — copy-chain follow capped at 128 hops** (also NOP-walk caps
  `:3524,:3536,:3551`). Bails conservatively (stops folding) → precision-only.

## NOTED-FINE — documented/sound, no action (listed so they aren't re-flagged)

- `sir_optimizer.c:3369` — whole-method CSE disabled when a `try_region`/`exception_entry` is
  present. Sound carve-out (finally-duplication would make mutually-exclusive copies look
  congruent). Skips an optimization, not correctness.
- `sir_optimizer.c:51` — array memory cells keyed by datatype only (all ref-arrays share one
  cell). Conservative (more invalidation); congruence still requires matching `arr`/`index`
  operands, so never a wrong CSE.
- `type_lattice.c:234` — REF/unknown dt → default i32 storage index; callers pre-filter, so
  fail-open only if a future caller doesn't. `:224` — stale storage-index doc comment (says 6
  slots; code has 7 after the distinct char slot) — fix the comment.

## SEMA-DIAGNOSTIC (`analyses.c` — adjacent, not Click; full-Java diagnostic correctness)

The AST-level diagnostic lattices (nullability/interval/definite-assignment/exception-flow).
Clean on JCVM-WIDTH (interval lattice is `int64` end-to-end) and switch; three items:

- [x] **`analyses.c:2264` — div/rem `ArithmeticException` seed omits `JT_LONG`** (only
  byte/short/int). `5L/0L` throws; a `catch(ArithmeticException)` over long-only division is
  spuriously flagged unreachable.
- [x] **`analyses.c:1291` — definite-assignment is a fixed 256-slot bitmap that fails OPEN.**
  A method with >256 locals (legal to 65535) silently loses uninitialized-variable errors.
- [x] **`analyses.c:2327` — `to_remove[64]` nested-try type set silently drops the 65th+**,
  skewing the enclosing-try exception subtraction. Unlogged → `bbq_vec`.

---

## Burn-down order for re-enable

1. The two MUST-FIX (`:4038`, `:4207`) — Click produces wrong slot layouts without them.
2. The HARDEN valves (`:2068/:2079`) — so a re-enable can't silently ship unsound facts.
3. Wire `sir_optimize` into the driver behind a flag; run the WHOLE `test_exec` corpus +
   byte-pins with Click ON; diff.
4. The FIX-BEFORE-WIRING γ Type kinds (`:696`, `:637-668`) — required before the
   combined-analysis type element (see `combined-analysis-spec.md` §3) can trust the γ Type.
5. PRECISION/SUBSET + SEMA-DIAGNOSTIC items as the analyses land.

**Cross-reference:** items in FIX-BEFORE-WIRING and PRECISION/SUBSET are prerequisites for
`combined-analysis-spec.md` — its type element derives from the γ/lattice Type, so the `short`
holdovers must be int-correct before the pointer/nullability/range/escape lattices ride Click.

---

## Re-enable burn-down — found & fixed while wiring in (2026-07-11)

The corpus with Click ON (`make test-exec-click`, the committed gate) surfaced six
MISCOMPILE classes the unit suite's coverage never reached. All fixed, each with a
partition-suite test where unit-expressible; the corpus is the end-to-end gate:

- [x] **Post-yoctojc spine ops invisible to the engine.** `ArrayCopy` / `SetHeader` /
  `MemStore8` appeared in NO engine switch (uses, rewrite, CSE roots, repoint, nop
  compaction, pack scan/rename, memory-cell writers). Fixed structurally: every walker
  now uses the generated `sir_arity`/`sir_child` (+ new `sir_child_slot`) accessors, so
  a new opcode is wired by extending sir_support once. ArrayCopy is an array-cell
  writer; `cp_expr_uses` fails closed (generic default recursion).
- [x] **Ref slots coalesced across referents.** cp_pack pooled ALL refs together; a WASM
  local is typed, so `String()V` reused a String cell for an exception ref. Ref slots
  now coalesce only on identical interned referent Type.
- [x] **Cross-valtype load substitution.** GVN forwarding / peer-φ collapse / constant
  substitution materialized an i32 local.get where i64/ref was consumed (value-congruent
  ≠ representation-compatible). All substitution sites now gate on `lat_dt_valtype`
  (+ interned referent equality for refs).
- [x] **Untyped opaque seeds.** Slot seeds entered the fixpoint typed BOTTOM, so an int φ
  and a long φ carried identical no-information type facts and could share a partition.
  Seeds are now typed from the ONE slot-type scanner (prim width / interned referent).
- [x] **32-bit pointer-hash identity maps.** `spine_idx`/`expr_idx`/rename/host keyed
  bbq_htree by a compressed pointer hash — silent collisions at jre scale (stale slot
  renames). Replaced with `cp_pmap_t` (hash-bucketed exact-pointer chains).
- [x] **CSE lifted pure LEAVES.** The eligibility exclusion was a stale tag list;
  `LoadClass` (γ `is_leaf_pure`, type GT_BOTTOM) got lifted into an int-typed temp
  (Class ref in an i32 local). Exclusion is now γ-driven (`is_leaf_pure`).
- [x] **Burg receiver-reload contract.** V/IV/TV/TIVCALL read `obj->load_local.slot`
  unconditionally; Click legitimately forwards a receiver load to `LoadThis`/`LoadNull`.
  `RECV_RELOAD` re-emits by leaf kind.

Routing (the point of the exercise): `lat_dt_valtype()` is THE dt→valtype map (pack
pools, substitution gates, locals emitter, burg `DT_IS_I32`); `gamma_ref_to_type()` is
THE descriptor→Type map (referent identity = interned pointer equality);
`cp_scan_slot_types` is THE slot dt/referent scanner (engine seeds + pack pools).

## Execution-tier burn-down (2026-07-11, after validation went clean)

The optimized jre VALIDATED but three ENGINE soundness bugs surfaced at runtime
(found via the per-method `JAVELINA_CLICK_ONLY` bisection hook + `javelina` runner
probes with timeouts — seconds per iteration):

- [x] **Copy-forward past the source's redefinition.** GVN forwarded a load of `e`
  to the copy-source SLOT (`e = old; old = old.next; …e…` → reads of e emitted
  `local.get old`) — slots are not SSA. Both leaf-forward paths now gate on the
  slot_in dominance proxy (`cp_slot_still_holds`): the slot's reaching def at the
  REWRITE point must sit in the substituted value's partition. Broke
  Hashtable.rehash's chain walk (put#2 NPE).
- [x] **Missing Click §4.4.1 TOP-exit re-arm.** CAUSE_SPLITS skips users still at
  TOP (optimism); the skipped touch was never retried, so φs distinguished only by
  inputs seen while they were TOP stayed merged. PROPAGATE now re-enqueues every
  input's partition when a node's type leaves TOP (≤ once per node).
- [x] **φ-follower revert kept diverged φs merged.** cp_revert_phi_follower flips
  Follower→Leader in place, relying on split_by_facts — which sees only type/const
  FACTS, not positional input divergence. Two lockstep-then-diverging loop counters
  (initProperties' `i`/`count`: both prim(int), both BOTTOM) carried identical facts
  → the loop condition read the WRONG counter → infinite loop. The revert now
  re-arms the φ's input partitions so positional CAUSE_SPLITS separates the peers.

Unit pins: `test_cp_no_copy_forward_past_source_redef`,
`test_cp_lockstep_counters_not_congruent` (the exact shared-latch ddcg shape).

### Second execution wave (same day, found via the streamed-corpus FAIL list + probes)

- [x] **NaN reflexivity.** The §4.6 x⊙x folds (NE(x,x)→0, EQ→1, SUB(x,x)→0) are wrong
  for float/double (NaN != NaN is TRUE; NaN-NaN is NaN) — `Float.isNaN` is exactly
  `v != v` and folded to false. Gated off float/double/unknown operand valtypes.
- [x] **Pending recomputes dropped on partition moves.** cp_part_remove unlinked a
  queued cprop entry and cleared in_cprop; every remove→add mover (split,
  split-by-facts, follower apply/move) now re-enqueues a pending member.
- [x] **No post-move re-examination.** A φ-Follower's revert notification could be
  consumed BEFORE the input's partition split out; nothing re-enqueued the users
  after the MOVE, so φ(s) stayed a follower of the entry constant —
  `for(i..) s+=i; return s` compiled to `return 0`. Movers now notify the moved
  node's def-use users (cp_notify_users_of_move) so follower invariants are
  re-checked against post-split partitions (Click §4.7.5's SPLIT-before-apply order).
- [x] **CHECKCAST elim ignored representation.** Dropping a value-provably-safe cast
  replaces the emitted type with the operand's STATIC type (`Object o = new Box();
  (Box)o` → Object-typed local stored into a Box slot → §7.6 reject). Elim now also
  requires the operand's static class ⊑ target.
- [x] **CSE lift store self-assignment.** On the next lift iteration the walk
  revisited the spliced Store(temp, expr) and substituted its value (an
  "occurrence") with Load(temp) → the computation vanished (String.substring's
  `endIndex-beginIndex`, Long.parseLong, Integer.toString radix cluster). The
  canonical copy in the lift store is now exempt from substitution.

Unit pins: test_cp_no_reflexive_fold_on_floats, test_cp_no_sub_self_fold_on_doubles,
test_cp_loop_accumulator_not_constant, test_cp_checkcast_kept_when_static_type_wider
(+ dropped-when-matching positive), test_cp_cse_lift_store_keeps_its_expr.

### Third wave (2026-07-11, the BitSet/StringTokenizer stragglers)

Drilled pin-first (unit → probe → corpus-once). The BitSet failure turned out NOT
to be Click at all; the tokenizer cluster was two distinct Click bugs.

- [x] **`~long` frontend miscompile + silent burg truncation (NOT Click).** The ddcg's
  BitNot built `LoadConst(-1, dt)` even for dt=long; the burg's only LoadConst tile is
  `i32.const`, so the Xor spine had NO cover — and the generated `burg_rewrite`
  **silently skipped** uncovered trees, leaving downstream reads on a never-written
  spill temp (always 0). `BitSet.clear` computed `w & 0`; the corpus probe's check
  order masked it at baseline (bits 3,5 checked BEFORE the clear wiped the word), so
  it presented as an opt-only failure. Fixes: (1) ddcg BitNot routes through
  `neg_one_const(dt)` (LoadLongConst for long); (2) burgc (BOTH C and C++ backends)
  now sets "start nonterminal has no rule at root" for uncovered roots in the fast
  path AND the RPO reduce loop (burgc_tests pin ×2); (3) the driver checks
  `burg_has_error` after codegen and refuses to ship a truncated body (fail-loud,
  same pattern as the §7.6 grammar gate). Pins: test_sir §8 (~long → LoadLongConst,
  BitSet shape incl. the %64 guard), test_cp_long_not_survives (wide-const identity).
- [x] **Path-refinement leaked through an entry-headed loop (`cp_mark_arm_subtree`).**
  The arm walk stops at merges via `pred_cnt > 1`, but the METHOD ENTRY reached by a
  back-edge has one explicit pred (its method-start edge is implicit — the same
  special case cp_resolve's is_merge already handles). For a `while` headed at the
  method entry (ddcg's leading-while shape: StringTokenizer.skipDelimiters), the
  then-arm walk ran the back edge AROUND the loop, marked the branch itself as its
  own arm, and rewired the condition's operand to a Refine carrying the branch's own
  predicate → the test folded constant-true → the tokenizer consumed every char.
  Fix: the arm walk also stops at the entry spine node. Pins:
  test_cp_field_loop_and_test_survives (exact minimal shape),
  test_sir §9 (real-pipeline skipDelimiters keeps its rem-test branch).
- [x] **GetFields of DISTINCT fields value-congruent (`cp_partition_init`).** After an
  invoke (wide memory write shadows every cell with ONE opaque), `GetField(cur)` and
  `GetField(max)` carried identical (obj, memory) inputs — and the initial buckets
  keyed on opcode alone, so input-driven refinement could never separate them:
  `cur < max` folded reflexively to false (hasMoreTokens returned constant false).
  The field is an OPERATOR IMMEDIATE, not an input. Fix: γ `bucket_discriminator`
  rows for GetField/GetStatic (exact (class_id, field_idx)) + partition-init bucket
  lookup made two-level exact (no <<16 packing loss). Pin:
  test_cp_getfields_of_distinct_fields_not_congruent.

Tooling added: env-gated SIR spine dumper in test_sir (SIR_DUMP / SIR_DUMP_RAW /
SIR_DUMP_SRC / SIR_DUMP_METHOD) — dumps the optimized or raw spine of any method
compiled through the real pipeline; this is what localized both tokenizer bugs.

### Fourth wave (2026-07-11) — the remaining nine corpus FAILs, two root causes

The nine looked like eight unrelated features (Math.pow, BitSet, three java.io
round-trips, RandomAccessFile, the Character BMP checksum). They were two bugs.

- [x] **SPLIT_BY_FACTS never re-armed the CAUSE_SPLITS worklist** (`cp_split_by_facts`).
  Click §4.2's SPLIT contract: a partition that splits goes back on the worklist,
  along with the partitions carved out of it, so users get re-examined **by input
  position**. cp_split_by_facts only called cp_notify_users_of_move — which
  re-enqueues the users' FACTS (cprop) and nothing else. For users whose facts are
  identical, position is the ONLY thing that can separate them: `(v >>> 24)`,
  `(v >>> 16)`, `(v >>> 8)` all carry BOTTOM (v unknown) and differ solely in which
  constant partition feeds input 1. The shared LoadConst partition split by value,
  the Ushr users were never re-examined, they stayed congruent, and CSE collapsed
  all three into one — `DataOutputStream.writeInt` wrote the top byte four times
  (`>>> 0` escaped only because shift-by-zero folds to identity). This one bug
  accounted for writeInt/writeLong (→ both DataStream round-trips, since readFloat
  is intBitsToFloat(readInt()), and RandomAccessFile), the fdlibm `Math.pow`
  (`(hx >> 20) & 0x7ff` shapes), the Math+String aggregate, and the Character BMP
  binary-search if-trees. Pin: test_cp_same_op_distinct_const_operands_not_congruent
  (verified RED without the re-arm, GREEN with it).
- [x] **f32 constants were carried in a `double`** (`cp_const_t`). "An f32 is held
  exactly in the double" is true for *numeric* values and false for *bit patterns*:
  widening a signaling NaN to double sets the mantissa MSB (quieting it) and
  narrowing back keeps it set, so raw `0x7F800001` folded to `0x7FC00001` —
  §20.9.18 `Float.floatToRawIntBits` is explicitly the non-canonicalising accessor,
  so the payload is observable. f32 now has its own exact carrier (`fvalue`) with
  width-aware accessors (`cp_known_f32` / `cp_known_f64`, matching the existing
  `cp_known_i64` idiom); every γ fold reads the carrier its operand width names;
  f32 arithmetic folds in float, not double (JLS §15.17 — a double detour also
  double-rounds division). Pins: test_cp_move_f2i_preserves_raw_nan_bits,
  test_cp_f32_const_keeps_exact_bits. NB: no test pins the NaN payload an
  *arithmetic* op produces — JLS §4.2.3 leaves that unspecified; only the
  reinterpret and copy paths owe bit-exactness.

### Fifth wave (2026-07-11) — the last two: a φ's merge is part of its identity

- [x] **All φs shared ONE initial partition** (`cp_partition_init`). CAUSE_SPLITS
  splits only by input partition, and every `c ? 1 : 0` merges the same two
  contributors (LoadConst 1, LoadConst 0) in the same positions — so φs at
  completely different merges were congruent, and nothing could ever separate
  them. The §4.10 peer-φ canonicalization then rewrote every read onto the first
  φ, the other diamonds' stores went dead, their branches collapsed, and the
  CALLS inside those branch conditions were deleted along with them:
  `(g(3)?1:0)*100 + (g(65)?1:0)*10 + (g(4)?1:0)` compiled to `s*111`. That is
  both remaining failures — BitSet set/get across a word boundary (which is
  exactly that expression) and the Character FULL BMP checksum (whose body is
  `h*31 + (isLetter(c) ? 1 : 0)` ×4 per code point).
  A φ's merge point is part of its identity: in Click's sea-of-nodes the Region
  IS the φ's input 0. Javelina keeps the merge as a *field*, so partition-init
  now buckets φs by merge — φs at the SAME merge still share a bucket, which is
  what peer-φ collapse (same merge, different slots) needs. Pin: test_sir §10
  (real pipeline — a hand-built partition-level shape did not reproduce; the
  ddcg's actual lowering was load-bearing, so the pipeline pin is the honest one).

**The generalizable rule behind three of these five waves:** congruence is decided
by "opcode + input partitions", so anything that distinguishes two nodes but is
NOT an input must be folded into the initial bucket — the field a GetField reads,
the merge a φ sits at. Ask of every vnode kind: *what does this node's result
depend on that is not one of its inputs?*
