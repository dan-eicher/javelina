# Merge labels: emit-once control destinations in the WASM backend

Status: NORMATIVE spec. Implements the paper exactly; deviations are bugs.
Authority: Dybvig/Hieb/Butler, *Destination-Driven Code Generation* (TR #302),
`/home/dan/Documents/Destination-Driven Code Generation.pdf`. Read Figures 5–8 and
p.13 (CG_branch / CG_jump) before writing any code.

## 1. The defect (audited, verified)

The paper's target is a linear stream with labels: `CG` runs once per AST node and
every label is **defined exactly once**; any number of transfers reference it
(`CG_jump L ⇒ jbr L`). Code size is therefore linear in AST size *by construction*.
The paper's worst admitted output defect (p.15) is a redundant jump — never
duplicated code.

The defunctionalized frontend (`grammar/compiler.ddcg`) is conformant: labels are
SIR node pointers, `Lnext` is the `.next` edge, destinations are shared by pointer
(Fig. 7 `and/or` share `Lf`/`Lt`; guard diamonds share their continuation).

The backend (`src/compiler/codegen_structured.c`, `emit_spine`) violates the
emit-once rule: it emits a node once **only if a sidecar scope record frames it**
(loop header, if-join, switch break, try join). Any *other* node with in-degree
> 1 is re-emitted by recursive descent from each referencing branch. Duplication
compounds through nesting: measured ×2.0 per `else if (… && …)` level
(16,318 → 65,522 bytes going 6 → 8 levels), 305 KB for `Date.civil` (8 sequential
guarded divisions), 166 KB for `System.arraycopy`, 307 KB for `dtoa`.

Verified sites where the ddcg hands one destination to ≥ 2 consumers without a
record (each read and confirmed):

| site | shared destination |
|---|---|
| `shortcircuit_pair` (Fig. 7) | `Lf` (`&&`) / `Lt` (`\|\|`) — in an else-if chain this is the next level's Branch head |
| `shortcircuit_value` | `true_arm`/`false_arm` both continue to the same `cg_jump(γ, Lnext)` continuation |
| `binary_intdiv_guarded` | Neg-arm and Div-arm stores share their continuation |
| `emit_ref_cast` / `emit_array_cast` | `ok_null` and `ok_inst` share the γ continuation |
| `if_stmt` / `ternary` joins | already recorded (BLOCK) — the same rule, already applied |

Guards with terminating throw arms (`null_guard`, `bounds_guard`,
`neg_size_guard`, `arraycopy_bounds`) share nothing and are NOT affected.

## 2. The design (one rule, no cases)

**A node referenced by more than one control transfer is a label. A label's code
is emitted exactly once. Every other reference is a `br`.** In WASM: one `block`
frame ending where the label's code begins (back-edges already use `loop`).

### 2.1 Frontend: record the fact at the site that creates it

One new scope kind: `COMPILER_SCOPE_MERGE`. The ddcg records `(head, X, MERGE)`
at **exactly** the point where a rule hands an already-minted destination `X` to
a second consumer — the sites in §1's table. `head` is the SIR node the rule
returns (the branch head the backend will encounter top-down); `X` is the shared
label. This is the existing `record_scope` sidecar discipline (the backend reads
facts the frontend knew; nothing is recomputed) — kinds BLOCK/LOOP/SWITCH and the
try records are unchanged.

Records land in **build order, which is inner-first** — a rule keyed on the test
head can only record after `gen(test)` has run, and `gen(test)` is what records
the condition's inner merges (`Lf`/`Lt`). So for `if (a && b) …`, `Lf` (from
`shortcircuit_pair`, inside `gen(test)`) is recorded before the if-join `Ljoin`
(recorded by `if_stmt` after, keyed on the now-known test head). This matches the
existing "recorded inside-out" sidecar convention. The backend derives nesting,
not the record order (§2.2): the BLOCK if-join is outermost by definition, and
MERGE exits nest inner, in reverse record order. A destination that is the rule's
own `Lnext` fall-through and referenced once is not a merge — do not record it
(no Nop spam; minimal labels by design).

**Record the node that is ACTUALLY shared — never one synthesized from γ.** `X`
must be a real SIR node that ≥ 2 consumers point at, read off the graph the rule
just built. This matters where a rule's arms deliver a value through `cg_store`,
because whether they share depends on γ (`cg_store`, l.497):

- γ = `single(L)`: both arms continue to the *same* `L` (`cg_store single ⇒
  StoreLocal(…, value, L)`). `L` is the shared label → record `(head, L, MERGE)`.
- γ = `ret`: each arm mints its **own** `Return` (`cg_store ret ⇒ build
  ir.Return(...)`). The arms share no node — each terminates (`CG_jump return ⇒
  ret`, return is not a label). Record nothing.
- γ = `pair(Lt, Lf)` **does not arise** for these three constructs. An int-div/rem
  result is `int` and a ref cast yields a reference; neither is a boolean control
  destination in Java, so the divide/cast is never gen'd in a paired (test) γ — it
  is always delivered to a location (`single`) or returned (`ret`). `single`/`ret`
  is therefore exhaustive here; matching them is complete, not a restriction that
  might skip a shared `pair` merge.

So `binary_intdiv_guarded`, `emit_ref_cast`, and `emit_array_cast` match γ and
record the `single(L)` case. Do **not** write `record_scope(head, cg_jump(gamma,
Lnext), MERGE)`: on the `ret` path `cg_jump` fabricates a fresh `ReturnVoid` no arm
points at, which miscompiles (verified — it broke 15 `test_exec` cases). For
`shortcircuit_pair`/`shortcircuit_value` the shared `Lf`/`Lt`/arm nodes are already
real nodes on the graph, recorded directly.

**Completeness of per-site recording** rests on one assumption: every SIR node
with in-degree > 1 is minted inside the single rule that shares it (true for the
structured DDCG output here — there are no cross-rule joins other than the
break/continue/try targets already recorded as LOOP/try scopes). The §4.3
linearity property test is the standing guard: if some share is ever created
across rules and missed, the size delta stops being constant and the test fails
loudly. Do NOT replace per-site recording with an emit-time predecessor walk to
"be safe" — that is backend recomputation; if the assumption ever breaks, add the
missing record at its mint site.

### 2.2 Backend: frame recorded merges, then get out of the way

In `emit_spine`, at a `SIR_BRANCH` node `B`:

1. Collect `B`'s MERGE records in **reverse** sidecar order (records are
   inner-first per §2.1, so reversing gives outer→inner). These are real labels —
   the condition's branches transfer to them (e.g. `$Lf`) — so they are always
   framed for a compound condition. **Filter**: drop any `X` that already resolves
   (`X == stop`, or `br_depth(stack, sd, X) >= 0` — e.g. a loop-exit `Lf` already
   framed as a LOOP scope). No other filter; `X` being itself a `SIR_BRANCH` (the
   else-if chain) is framed like anything else.
   Then prepend the BLOCK record's `exit` (the if-join) as the outermost bound
   **only when the join is itself a label** — i.e. a MERGE survives (compound
   condition) AND some arm reaches the join by fall-through (completes normally,
   JLS §14.21) and so must `br` *over* the else. When every arm of the compound
   `if` terminates (return/throw), the join is referenced by nothing: per the
   paper an unreferenced label emits no code, so it is **not** framed — no `block`.
   "An arm reaches the join" is precisely the fall-through `emit_spine` reports;
   the join bound is conditioned on it (a read-only arm-completion check, never a
   merge-find or in-degree walk). A plain `if` (BLOCK record, no MERGE — a simple
   single-`Branch` condition) collects nothing here and falls to step 3's native
   `if/else`; its pinned bytes must not change.
2. If bounds `[X_0 … X_{n-1}]` (outer→inner) survive:
   open `block void` for each in order, pushing `X_i` on the scope stack; emit
   from `B` bounded at `X_{n-1}`; then for `i = n-1 … 1`: `end`, pop, emit
   `X_i`'s code bounded at `X_{i-1}`; finally `end`, pop, continue at `X_0`.
3. If nothing survives, the existing paths run unchanged: back-edge `br_if`,
   break-on-false/true (`dt`/`df`), native `if/else` for plain ifs (their pinned
   bytes must not change).

Step 2 is self-stabilizing: re-entering `emit_spine` at `B` after framing, every
recorded `X` now resolves on the stack, so the plain `dt`/`df` transfer paths
emit `cond; (eqz;) br_if <depth>` — the conformant CG_branch. No new emission
logic for the branch itself, no flags, no re-entry guard.

Worked shape (`if (a && b) T else E`; merges `[Ljoin, Lf]`):

```
block $Ljoin
  block $Lf
    a; eqz; br_if $Lf
    b; eqz; br_if $Lf
    T
    br $Ljoin
  end        ;; Lf defined once — E follows
  E
end          ;; Ljoin defined once
```

Here `T` completes normally, so it `br`s over `E` and the join `$Ljoin` is a real
label — framed. When both arms terminate (`if (a && b) return 1; else return 2;`)
nothing reaches the join, so `$Ljoin` is **not** framed — only `$Lf`:

```
block $Lf
  a; eqz; br_if $Lf
  b; eqz; br_if $Lf
  return 1              ;; terminates — no br to a join
end                     ;; Lf defined once
return 2
```

An else-if chain frames `$Lf_k`, closes it, and emits level k+1 (the next head)
*after* the `end` — sequential frames, linear size in chain depth.

### 2.3 Rip-outs (the two prior partial patches are subsumed)

- `COMPILER_SCOPE_CONDELSE` and `COMPILER_SCOPE_GUARD` kinds, `cond_scope_at`,
  `guard_scope_at`, and both special-case emit paths in `codegen_structured.c`.
- Their `record_scope(…, 3)` / `record_scope(…, 4)` calls in `compiler.ddcg`
  (`shortcircuit_pair`, `binary_intdiv_guarded`) — replaced by MERGE records
  per §2.1.
- The `JAV_NODEDUP` getenv toggle and the `<stdlib.h>` include it dragged in.
- After acceptance: the `JAV_SIZEDBG` `[bigfunc]`/`[size]` instrumentation in
  `wasm_module.c` (it was diagnostic scaffolding for this defect).

Purge cleanly — no comments about what used to be there.

## 3. No-carve-out clause

Every §1 site gets a MERGE record. If a shape seems hard (mixed `&&`/`||`,
merge-that-is-a-Branch, cast diamonds inside call arguments), the design above
already covers it; "skip when the exit is a Branch" and similar exclusions are
exactly the defect this spec removes — the else-if chain (the dominant real-world
case) lives in that exclusion. If a genuinely new shape appears, it is a new
`(head, X, MERGE)` record, never a new backend case. A test that pins pre-fix
duplicated bytes samples the bug, not the oracle — rewrite it to the paper's
layout (as was done for the old `&&` test that pinned `return 2` twice); never
weaken the design to keep such a test green.

## 4. Test plan (failing tests FIRST, each at its own level)

1. `test_scope_sidecar`: MERGE records exist, correctly keyed and ordered, for:
   `&&` in if-else, `||`, else-if chain (record per level), value-context
   boolean, guarded int/long div in `single(L)` γ, ref cast, array cast. Write
   these red against the current tree.
2. `test_codegen_structured`: byte-pins from the §2.2 layout for: `if (a&&b)`
   with both-return arms; fall-through arms (then `br $Ljoin`, shared else once);
   `||` mirror (br_if on true, no eqz); an int-div statement (guard arms fall
   through to one continuation); a 3-level else-if chain (each level's tail
   bytes appear exactly once). Existing pinned bytes for plain if/while/switch/
   try must not change.
3. `test_control_audit`: matrix rows for `&&`/`||` in loop tests, in ifs, casts
   and divisions in arms — oracle stays clean. Add the linearity property:
   compile k and k+1 else-if levels, assert the size delta is constant (this is
   the invariant the paper gives for free; it is the regression guard against
   reintroducing duplication).
4. `make test-exec` green — the real embedder path.
5. Full `make test` (once, at the end), then `make test-cli`.
6. Observation (not a gate): jre.wasm size and `[bigfunc]` list; expect
   `System.arraycopy` and `Date.civil` to leave the list and the module to drop
   well below the 937 KB the two partial patches reached (pristine baseline:
   1,504 KB).

Build discipline: one build at a time, through make targets; run the affected
suite per step, full suite once at the end.
