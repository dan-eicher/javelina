# Theorem audit — the combined analysis against its papers

**Purpose.** Correctness of the optimizer rests on the papers' theorems, not on tests passing. A
green test only fails to disprove a bug on the cases written; the *correctness argument* is that the
code is a faithful **instance** of a proven construction and **discharges that construction's
obligations**. This document is the fixed denominator: **every** named result across the four
sources, each with (statement → code that instantiates it → its obligations → verdict). No entry is
"done" on tests alone; a ledger row that is test-green but whose obligation is not proven-and-
instantiated is a **GAP**, not a pass.

**Sources.** VFG = "Precise and Scalable Context-Sensitive Pointer Analysis via Value Flow Graph"
(ISMM'13). Choi = "Escape Analysis for Java". Click = "Combining Analyses, Combining Optimizations".
SU = the strong-update chain (Lhoták–Chung POPL'11; Chase–Wegman–Zadeck PLDI'90), cited not local.

**Verdicts.** `DISCHARGED` = instantiated + obligation verified + a falsifier pins it. `N/A` =
deliberately not instantiated, with the architecture reason + spec citation. `GAP` = should hold but
the obligation is unproven / only test-green / a branch is uncovered. `PENDING` = not yet audited.

**Status:** denominator fixed at **26** entries (A1–A9, B1–B9, C1–C7, D1) + the E backlog. First pass
worked all 26 to a verdict. Discharge tiers are marked honestly: `[chk]` = re-verified against the code
*in this audit*; `[led]` = meets the bar via the §0 ledger row's own verbatim citation + RED-first
falsifier, spot-checked not re-derived. **No new soundness gap was found**; the residual is
written-obligation debt, not a suspected miscompile. The ceiling not reached is *exhaustive
re-derivation* of every `[led]` proof — **07-16 reached it for the whole Choi B-section (B1–B9:
Def 2.4 mapped element-by-element, Fig 6 statements 1–26, Fig 7 statements 27–43, §4.5 bottom
graph, §5.1 against the paper's own m0 example, §5.2 finalizers) and the whole Click C-section
(ch. 2 optimism verbatim; §3.3 lattice figures; §3.6–3.7 executable edges + the mixing function;
Fig 4.7 lines 26–48 incl. step 43 verbatim; §4.8 revert steps 6.1–6.4 + 16.5 verbatim).**
Remaining spot-check tier: A5/A9 (VFG §4.1 — read in full at Gate 5's landing, rows carry its
citations), D1 (cited, sources not local). No paper/implementation mismatch was found in any
re-derivation; the recorded residue is two precision notes (Fig 3.4's zero-multiply against
BOTTOM; Choi's collapse-to-bottom-node summary compression) and the performance-only divergences
(no Race, no L/F edge segregation, total escape sweep instead of two-phase worklists).

---

## How this drives the work (the layering)

This inventory is the correctness spec AND the build backlog. Every entry has an **owning layer** —
where its falsifier lives — and the layers are verified **bottom-up**:

- **L0 unit / producer** — the algorithm in isolation (`test_click_partition`, `cp_summarize` producer
  pins). The lemma's own falsifier, incl. the SOUNDNESS NEGATIVES (the "must NOT" cases).
- **L1 integration** — the lemma through the real pipeline (`test_sir`: source → summary → analyze).
- **L2 e2e** — `test-exec-click`; CONFIRMATION only. The jre census is a yardstick, never an oracle.

A `GAP-UNBUILT` entry carries a **RED (xfail) falsifier at L0** stating the owed behavior, kept red on
purpose (see `feedback_keep_xfail_tests_red`) until implemented — so the backlog is executable, not prose.
A lemma is `DISCHARGED` only when: implemented ∧ its L0 falsifier green ∧ the lemmas it depends on
discharged. Then L1, then L2. **You never reach for an e2e test to decide whether the algorithm is right.**

## A. VFG (ISMM'13) — the pointer / value-flow half (spec header cite)

Our architecture adopts VFG's **summary representation** (input-relative value flows) but **not** its
context-sensitivity machinery — no function cloning, no CFL-reachability, no auxiliary variables. We
use Choi bottom-up over the **defunctionalized** call graph instead (spec §7: "the VFG paper spends
its whole scalability budget approximating exactly what you already have"). So the CS results are
`N/A` **by design**, and that is a verdict, not a skip.

| # | result (verbatim gist) | verdict | where / why |
|---|---|---|---|
| A1 | **Rule 1** — introduce aux vars `fp*in`/`ap*in` per function input for CS aliasing | `N/A` | no aux vars / no CFL; Choi bottom-up defunctionalized (spec §7) |
| A2 | **Rule 2** — bound aux vars by distinct pts | `N/A` | Rule 1 not used |
| A3 | **Theorem 1** — Rule 2 is sound + precise | `N/A` | Rule 2 not used |
| A4 | **Lemma 1** — aux vars = inlining, sound+precise | `N/A` | Rule 1 not used |
| A5 | **Rule 3** — a STORE `*p=x` reaches a LOAD `y=*q` iff the indirect value-flow path exists (no intervening kill) | `DISCHARGED` | the memory-SSA overlay: a load's memory input IS its reaching store; `cp_apply_load_follower` forwards. Falsifier: `test_click_partition` load-forwards / does-not-across-receivers; §1 store→load. **Gate 5** extends it across a non-writing call (kill-walk). |
| A6 | **Theorem 2** — Rule 3 CS is sound + precise | `N/A` | the CS half is not instantiated (no cloning); the intraprocedural Rule-3 soundness is A5, the interprocedural substitute is Choi Fig 7/§4.5 = B7/B8 (both DISCHARGED). Theorem 2 as such does not apply. |
| A7 | **Theorem 3** — cloning-free-ness condition (same inputs ⇒ same local flows) | `N/A` | no cloning |
| A8 | **Property 1** — local flows of callee included in caller summary | `DISCHARGED [chk]` | §7.2 transitive write set: `wcell(k) = direct spine writes ∪ {c : clobbered[eobj[k]][c]}` (`cp_summarize` ~8438-8483) — verified this session. Falsifier: §37c/§37f (the `f(p){g(p)}` chain, RED first) |
| A9 | **§4.1** — a STORE strongly kills O's prior values iff **O is a singleton address and p points only to O** | `DISCHARGED` | strong update gated on a CONCRETE singleton object (`cp_obj_is_concrete`, spec §2 / SU below); the follower requires singleton receiver (`cp_pts_sole_obj`) + value identity. Falsifier: `test_pts_strong_update_kills_prior_store` / `_weak_update_keeps_both`. |

## B. Choi — the escape half (spec header cite)

| # | result | verdict | where / why |
|---|---|---|---|
| B1 | **Def 2.1/2.2** `Escapes(O,M)` / `Escapes(O,T)` | `DISCHARGED [chk]` | method-escape = lattice E; thread-escape is **degenerate** — verified: `sir_optimizer.c` has ZERO thread/`synchronized`/monitor path (Javelina is Java 1.0 −`synchronized`), so `Escapes(O,T)` never fires. Falsifier: §27/§37 |
| B2 | **Prop 2.3** `¬Escapes(O,M) ⇒ ¬Escapes(O,T)` | `N/A` | single-threaded; T-escape degenerate |
| B3 | **Def 2.4** connection graph (object/ref/field/static nodes; points-to/deferred/field edges; 1-limited per `new`) | `DISCHARGED [chk]` (re-derived from the paper 07-16) | Def 2.4 read verbatim and mapped element-by-element. **N_o** ("at most one object node per statement", fn 2: 1-limited) = Obj per allocation SITE ✓ exact, incl. phantoms (§3's `p.f=q` U=∅ rule: "create a phantom object node O_ph … we also use a 1-limited scheme for creating phantom nodes" = `Oext@param` cell phantoms; "during interprocedural analysis, the phantom nodes will be mapped back" = MapsTo). **N_r + E_d (deferred edges) are instantiated BY ELIMINATION**: E_d exists to make variable copies cheap, and in this substrate copies don't exist (§1/Braun; LoadLocal = copy-follower) — Def 2.5's "deferred path terminating in exactly one points-to edge" IS `cp_ultimate_value`; **ByPass(p) is free under SSA** (a new def is a new value; nothing is redirected in place). **N_f/E_f**: Choi's per-object field-node trees with shared `fid(f)` per class = our `(class,field)` cell whose VALUE maps Obj↦pts — per-object precision carried in the value, `fid` sharing = the cell key (pinned by the γ bucket-discriminator test). **N_g** = the static rows, seeded GlobalEscape ✓ ("the initial state for each node in N_g is GlobalEscape"). **E_r** = vnode pts bitsets. "We shall view each array as a single, monolithic object" (§2.2) — the arrays-monolithic rule is Choi's own sentence, not our simplification. ONE stronger-than-paper divergence, cited: Choi cannot kill field stores in general ("even for flow-sensitive analysis, we cannot in general kill whatever p.f was pointing to"); we strong-update iff the receiver names exactly one CONCRETE object — licensed by the SU chain (D1, Lhoták–Chung). Pins: the `test_pts_*` family per element. No gap found. |
| B4 | **Def 2.5/2.6** points-to path / PointsTo(m) | `DISCHARGED [chk]` (re-derived 07-16) | Def 2.5 ("a sequence of deferred edges terminating in exactly one points-to edge") is the copy-chain walk — `cp_ultimate_value` to the defining value, whose pts IS Def 2.6's PointsTo(m); with copies subsumed (§1) most paths are length one by construction. Falsifier: the copy-chain / follower pins + `test_pts_phantom_recursion_is_bounded` |
| B5 | **escape lattice** `NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥)`, meet `A∧NoEscape=A`, `A∧GlobalEscape=GlobalEscape` | `DISCHARGED [chk]` (paper p.3 re-read 07-16) | paper verbatim: "GlobalEscape < ArgEscape < NoEscape … A∧NoEscape = A, and A∧GlobalEscape = GlobalEscape" — `cp_escape_t` + `cp_escape_lower` (monotone-min) is that meet exactly; initial states verbatim too (N_g GlobalEscape; params ArgEscape; thread/finalizer GlobalEscape — the S4.a seeds). Falsifier: `test_sir` §27/§37/§38 |
| B6 | **Fig 6** reachability propagation (escape flows along value + field edges) | `DISCHARGED [chk]` (listing transcribed 07-16, statements 1–26) | Fig 6 is a TWO-PHASE worklist (GlobalEscape sweep from N_g/Runnable, statements 1–13; then ArgEscape from the phantom actuals, 14–26), propagating along OUTGOING edges with a monotone lower. Ours is a TOTAL sweep inside the ONE fixpoint (`cp_escape_update` gates `cp_solve`'s exit) over the same edge kinds — order-equivalent because the escape meet is commutative-monotone (two-phase is Choi's efficiency trick, not semantics), and total-not-worklist is the recorded re-arm-bug-class defense. Seeds verbatim (§4.1: formal ⟹ phantom actual anchor at ArgEscape = our param/phantom ArgEscape seeds; Runnable degenerate — no threads). Choi's collapse-of-GlobalEscape-into-one-bottom-node is a summary-SIZE optimization we do not take (per-obj states kept; no soundness content). The three-subgraph partition invariant ("only edges from LocalGraph to NonLocalGraph") = S5.2's "a NoEscape object never enters the summary, by construction". The top-down GlobalEscape marking pass: the paper's own "can be performed after the completion … in a top down pass" — the S5.3 skip-with-reason default is now cited against the source. Falsifiers: §37b (transitive escape — the fixpoint, not one pass), §27's table, exec ref-array cases |
| B7 | **Fig 7** MapsTo / UpdateCallerNodes (instantiate summary at call site) | `DISCHARGED [chk]` (listing diffed line-by-line 07-16) | Fig 7 statements 27–29 (per actual/formal pair, seed) = `cp_mapsto_graph`'s roots loop (receiver↦this_obj, arg i↦slot_obj[i], PARAMETER-indexed); statements 30–40 (the MapsToObj recursion, fid-matched field following through the CALLER's PointsTo) = the worklist over `edge_off/edge_dst` with `cp_follow_field` reading the caller's own cell rows — statement 33's visited guard IS the monotone-grow test; "the escape state of the nodes in MapsToObj(n) is marked GlobalEscape if the escape state of n is GlobalEscape" = the one escape line in the loop (ONLY GlobalEscape propagates ✓). The paper's "if there is no MapsTo node in the caller CG, we create one with escape state NoEscape" is pre-materialized by our cell-phantom seeds (a mentioned cell always has its `Oext@(cell)` row); unmentioned cells relay via `clobx` (the §37f transitive fix). **"Updating Caller Edges" (p.11) is the §42 injection**, verbatim rule in the plan's §42 note — `inject` + the `inject_bad` REPLACE-vs-EXT side-condition the paper doesn't need (its graphs only grow; ours must not let EXT swamp). Falsifiers: §37d/§38/§42p/§42/§42b/§42d |
| B8 | **§4.5** bottom methods / bottom graph (touch args-reachable + globals) | `DISCHARGED [chk]` (paper p.11 re-read 07-16) | verbatim: "one node for each class … a points-to edge from N₁ to N₂ if C₁ contains a field that is a reference to C₂ … a deferred edge if C₂ is a sub-type of C₁ … the most conservative connection graph of the program allowed under Java's type system. … Examples of bottom methods are native methods" — ours is coarser still (`has_bottom_call` + wide-kill + `cp_obj_survives_call`, no type-based narrowing), which is sound in the conservative direction and documented. Falsifier: `test_pts_call_kills_the_cell`, `_static_is_killed`, Gate-5 escaping-call negative |
| B9 | **§5.1** kill restriction (kill only block-local refs) — the exception-handler merge | `DISCHARGED [chk]` (re-derived from the paper 07-16) | §5.1 read verbatim. The paper's requirement: info holding at a throw point "will not be killed by statements after the exception throwing statement" — their fix is a KILL RESTRICTION ("amongst those local variables within a try block, we kill only those that are declared within the block") because their flow-sensitive ByPass kills IN PLACE, and the m0 example (store `x = new T1()` AFTER the throw point must not hide `x→O1` from the handler) shows what an unrestricted kill loses. **Our instantiation is structural, not a restriction: nothing is ever killed in place.** The handler is a RECORDED MERGE over exceptional EDGES (`cnt_add_exc` in the pred CSR + the handler φs), each edge carrying the state AT ITS OWN throw point — a store after a throw point is invisible to that edge by SSA construction. The m0 shape is pinned: E.f (the R.2b miscompile's corpus pin, `NPE 0/1` kept) + `test_sir` §33.1–.5 (per-edge thrown-value delivery, JLS §11.3 first-matching-clause filtering, the DDCG `-1` premise). §5.2 finalization also re-checked verbatim: "marking each object of the class overriding the finalizer as GlobalEscape" = the S4.a seed via `sema_resolve_virtual` ✓. Note for pass B's merge rule: its agree-join runs over the SAME pred CSR incl. exceptional preds, and an excepting node's IN-state is the state at its throw for slots (calls/throws write no slots) — the §5.1 obligation holds there too. |

## C. Click — the combined fixpoint (spec header cite)

| # | result | verdict | where / why |
|---|---|---|---|
| C1 | **ch. 2** optimistic assumption (start at top-of-lattice, descend) | `DISCHARGED [chk]` (ch. 2 read verbatim 07-16) | verbatim: "⊤ represents an optimistic choice that **must be proven to be correct**" — an early stop leaves values "potentially incorrect", while a pessimistic (⊥-start) early stop is "correct, but conservative", and "there exist programs for which the pessimal approach will *never* do as well". Instantiated: partitions start maximally congruent, constants TOP, escape NoEscape; `cp_solve`'s exit is convergence-gated, so the optimistic claim is always proven. **E3 is this chapter's dichotomy made flesh**: pass B runs pessimistic precisely so any stopping state is sound, and the E3 row records what happened when optimism ran without its proof obligation (the once-only chaining broke retraction). Falsifier: `cp_init_facts` contract test + the convergence pins |
| C2 | **§3.3 / Fig 3.2–3.4** constant propagation as a lattice | `DISCHARGED [chk]` (figures read verbatim 07-16) | L_c = ⊤/ℤ/⊥ with meet `(c0 = c1) ? c0 : ⊥` (Fig 3.3) — `cp_const_t` is the RANGE-extended refinement (interval hull is a monotone refinement of ⊥, K-widened for the finite-descending-chain property §3.2.1 demands of constant-height lattices; the trade is C3's recorded termination substitute). Transfer convention verbatim: `f_op(⊤,⊥) = ⊤` — "we do not propagate information until all the facts are known" = the engine's TOP-absorbing transfers and CAUSE_SPLITS' ⊤-skip. **Precision note, not a gap:** Fig 3.4's `f_*(0,⊥) = 0` (zero times anything) holds here only when the zero meets a RANGE (interval arithmetic gives [0,0] free), not against BOTTOM — a possible future increment, soundness unaffected |
| C3 | **monotone-framework convergence** (finite lattice + monotone transfers ⇒ terminates) | `DISCHARGED [chk]` | verified: finite domains (pts `∪`-grows, escape descends, partitions only split, `clobbered` only sets), range widening K-bounded (termination), 30 documented monotone-direction invariants; a non-monotone transfer HANGS and is caught by the `cp_propagate` inner-guard `abort()`. Falsifier: the widening/convergence pins |
| C4 | **partition refinement / GVN** (§4.2 + Fig 4.7 CAUSE_SPLITS) | `DISCHARGED [chk]` (Fig 4.7 diffed line-by-line 07-16) | Fig 4.7 lines 26–48 vs `cp_cause_splits`: line 30 (walk X.Leader **+** X.Follower's out-edges) ✓; line 31 (follow only edges TO Leaders — Followers excluded from splitting) = the `leader >= 0` skip ✓; lines 33–34 (constant SUB/COMPARE users → cprop) = the code's cited enqueue, engine EXTENDS it to φ (documented: §4.9 Follower-status depends on input partitions); lines 35–36 (skip ⊤, skip φ-dead-input) ✓; **line 43 (`\|Z.Leader\| ≠ \|Z.touched\|` ⟹ SPLIT) verbatim** = `touched_count != leader_count`. PERFORMANCE divergences, semantics-free: no §4.7.2 Race (smaller-half discovery by alternate BFS) and no §4.7.4 L/F-segregated def-use subarrays — ours filters at walk time; the smaller-half worklist rule itself (SPLIT lines 13–14) is kept. Falsifiers: the congruence tests + the E2 join family |
| C5 | **§4.8 follower / revert** (Click's own optimistic identity: "these congruences remain until the algebraic identity breaks when we lose the constant" ⟹ Follower ⇒ Leader) | `DISCHARGED [chk]` (steps read verbatim 07-16) | §4.8's PROPAGATE amendments — steps **6.1** ("If x is a Follower and is not an identity then…"), **6.2** (add to *fallen* ⟹ SPLIT_BY), **6.3** ("Move x from X.Follower to X.Leader" — quoted verbatim in `cp_revert_identity_follower`, which keeps the node in-partition exactly as the step says), **6.4** (re-segregate L/F edges = `cp_follower_unlink`), **16.5** ("Add Y to worklist" = the `cp_wl_push` in `cp_become_follower`) — all instantiated; the code's "§4.7.5 step 6.x" cites are these amendments TO §4.7.5's Figure 4.7 listing, defensible as written. Engine EXTENSIONS beyond the paper, each with its own falsifier: pts-refine followers (`test_cp_null_refine_does_not_move_partitions`), memory-identity followers (store→load, arraylen — spec §1/§10.7 pins), and the REVOCABLE kill-walk load-follower whose guard is monotone-DESCENDING (Gate 5 — the paper's identities break only by LOSING a constant, ours also by escape/clobber movement; falsifier: the escaping-call apply-then-revert negative). E3's failed optimism is the recorded boundary of this machinery: the once-only input-chaining is sound only under non-retracting presentation. |
| C6 | **§3.6 / Fig 3.6–3.7** unreachable code elimination (executable edges; dead region never analyzed) | `DISCHARGED [chk]` (section read verbatim 07-16) | the {U, R} two-element lattice with Or/And (Fig 3.6) and per-statement equations `S_i = S_pred · (edge condition)` (Fig 3.7) = `cp_spine_reachable` + the executable-edge gating (a KNOWN branch condition reaches one arm; `cp_phi_input_live` ignores dead contributors — §3.6's φ rule). §3.7's mixing function `≤ : L_c × ℤ → L_u` ("takes an input from the L_c framework and returns a result in the L_u framework") IS the branch-verdict→reachability wiring — the combining seam itself, which is why every sub-lattice reads reachability and why a dead guard arm's exception allocation is never analyzed (the census's dead-region lesson). Falsifiers: `test_cp_reachability_*` ×4 |
| C7 | **"combining ≥ any fixed pass order"** (the thesis result — why ONE fixpoint) | `DISCHARGED [chk]` | discharge = every consumer is a TRANSFER, no post-solve PROOF. Verified: the only post-`cp_solve` steps are `cp_rewrite` (computes ZERO facts — `grep`'d) + `cp_pack` (both rewrites); the two former post-solve proofs (`cp_rewrite_bounds_guards`/`_cast_guards`) were DELETED (ledger §9). Falsifier: moving a consumer into the fixpoint is module-byte-identical (the fact was already computed there) |

## D. Strong update (SU) — cited, not local

| # | result | verdict | where / why |
|---|---|---|---|
| D1 | **Lhoták–Chung POPL'11 / CWZ** — strong update permitted iff exactly one abstract object AND it is "most-recent" (site runs once, not in a loop) | `DISCHARGED` | `cp_obj_is_concrete` reads the DDCG's per-site "can run >1" flag; a loop site is a summary (weak only). Falsifier: `test_pts_strong_update` vs `_weak_update`; the `new int[2][2][2]` pin |

## E. Unbuilt / owed — `GAP-UNBUILT` (the backlog, with lemmas)

These are governed by the same papers but are **not yet instantiated**. Each needs a RED L0 falsifier
written first (kept red until built), then implemented bottom-up. Listed so the inventory is the whole
map, not just what exists.

| # | result / lemma | owning layer | verdict | what's owed |
|---|---|---|---|---|
| E1 | **RETURN pts** — VFG Rule 1 return half + Choi Fig 7 return mapping | L0 producer (`cp_summarize`) + L1 consumer (`cp_node_pts`) + L2 exec | `DISCHARGED [chk]` (non-null half) / object identity `N/A` | **FORMAL** return (alias to actual) was already done. **FRESH** return (`return new C()`) LANDED 07-16: `COMPILER_RET_FRESH` — the object identity is `N/A` (not mintable at the caller, per-site naming) but the result is **NonNull** (JLS §15.9.4). Falsifiers: §45p (producer classifies FRESH), §45 (consumer drops ⊥null); exec confirms. Sound: any null/param/Oext return ⇒ not fresh ⇒ UNKNOWN. **Transitive increment LANDED 07-16:** `COMPILER_RET_NONNULL` — every reachable ref-return's pts excludes ⊥null (the only fact FRESH's consumer used; identity stays Oret); `run(){ return m(); }` classifies NONNULL at any depth (the reverse-topo driver delivers DAG chains in one pass; cycles via the S5.1 loop, `cp_summary_differ` covers `ret_kind`). Pins: `test_sir` §45n producer + consumer, seen RED first. **Census: +1 NPE (616→617), module 623190→623161** — ⚠ the first write-up claimed byte-identical from a census run on a STALE `javelinac` (the gate chain never rebuilt it — the exact stale-binary trap `feedback_no_parallel_builds` records); re-measured 07-17 on fresh bits and control-isolated (both other candidate deltas measured inert): the jre DOES contain one transitively-non-null factory-result NPE guard, and the increment folds it. |
| E2 | **Refinement survives an all-agree join** — spec §4 ("exactly SCCP's executable-edge mechanism"): a join's value is the MEET OF THE EDGE VALUES, so a merge whose every in-edge delivers the same refined value keeps it | L0 `test_click_partition` `test_cp_refine_survives_all_agree_merge` (diamond inside a refined region); L1 `test_sir` §46 | `DISCHARGED [chk] 07-16` | **The 07-16 first write-up of this row was WRONG TWICE — wrong guard, wrong mechanism — and the record is kept because HOW it was wrong is the lesson.** Traced on the actual String.replace (post-solve vnode dump, not guessed): `value[i]`'s guard FOLDS (its chain composes: ll→refine→refine(+bound)→φ); **`buf[i]`'s** is the −1, and its chain skipped the header refine because pass B's strict-parity choice (plan §R.1 item 3, deliberate, upgrade deferred) reset EVERY merge row to the unrefined pass-A base — the ternary between the guards wiped the bound. Falsified along the way: (a) the plan's `cp_value_leader` swap; (b) "ArrayLength-receiver refine-transparency" (`cp_arraylen_same_array`, built from diagnosis-by-guess, never fired, deleted — a NONNULL Refine is ALREADY a congruence-transparent copy-follower, `cp_partition_init` pass 2, pinned by `test_cp_null_refine_does_not_move_partitions`); (c) "cp_const_intersect drops disagreeing bounds" (it does, but the trace shows the bound never reached that intersect). Fix: pass B derives rows by iterated forward sweeps; a merge row is the SCCP join — kept iff every pred delivers the SAME edge-refined value, else the pass-A base (fail-closed: back edges, defs, disagreement). Four synthetic reproducers were vacuous before the trace; the pin was distilled FROM the trace. Gates: L0 red→green; `test_sir` OK incl. §46 `gone=2`; census/module deltas reported in the plan's E2 note. |

| E3 | **Optimistic header-keep** — SCCP is optimistic (Click C1: start at top, descend): a loop-INVARIANT slot's refinement holds on both header edges, so the join should keep it and an in-body guard on it folds every iteration | L0 `test_click_partition` (`test_cp_range_invariant_bound_survives_loop` + the 07-18 lemma ladder) + L1 `test_sir` + L2 exec (`keep-iso` pins) | `DISCHARGED 07-18 (pending final L1/L2 gate runs)` | **Built as ch.2 §2.3 demands, after the 07-18 rebuild first REPRODUCED the 07-16 failure (same 6 exec cases) and the papers named it.** §2.3 verbatim: stopping top-down short of the gfp leaves "elements in the set that do not have a corresponding rule. Optimizations using these elements can be incorrect"; only "bottom-up methods can transform as they analyze". The 07-16/07-18 miscompiles were BOTH this sentence: the optimistic state was materialized into the SHARED VALUE GRAPH mid-iteration (07-16: in-place input-chaining; 07-18: composed Refine vnodes minted per sweep) — after a retraction the artifact survives, observable by partition/def-use. The interlock found 07-18: the join was NOT the meet (nested composes over a common suffix dropped to base instead of the suffix), and that under-approximation is what MANUFACTURED the retractions. Discharge: pass-B iterates on PURE INTERNED PAIR-STATE (`cp_pb_pairs_t`) with the merge as the true MEET (longest common chain suffix — Kildall); Refine vnodes MATERIALIZE only from converged rows at the rewire (the licensed bottom-up act). Falsifiers, all RED-first: `test_cp_join_nested_outer_kept_inner_dropped` (meet), `test_cp_no_unproven_transient_refines` (no unproven element in shared space, on the retraction shape), `test_cp_range_invariant_bound_survives_loop` (the feature), plus the soundness negatives (diamond-fail-safe, counter-not-kept, upper-bound-not-nonneg, L-VALUE value-identity sweep, L-XFORM structure pins, REF-slot pins). **The L2 miscompile (the SAME 6 property-path exec cases under FOUR keep-implementations with all L0/L1 pins green) was then isolated by ladder — CLICK_ONLY bisect → `System.initProperties` → its body transplanted as a plugin `keep-iso` exec pin (RED) → reduction ladder → per-layer verification (analysis clean, rewrite clean, pack consistent, bytes) — to TWO latent engine defects the keep newly exposes, both pinned RED-first and fixed 07-18:** (1) **γ_K payload read** — `cp_range_bounds`'s else-branch read BOTTOM/TOP's zeroed payload as a real [0,0], so `GE(0, BOTTOM)` folded KNOWN TRUE; gated fail-closed per Fig 3.2/3.3 (TOP dominates; non-fact ⟹ BOTTOM) in every fold (`test_cp_unit_fold_bottom_and_top_are_not_facts`). (2) **L-REARM-6, the miscompile** — a §10.7 identity FOLLOWER (arraylen ≡ size) is linked to its leader OFF the def-use graph; when the leader's constant descended (count φ: transient KNOWN 0 → BOTTOM) the follower's STORED constant was never re-armed, and the REWRITE (which reads the stored field, `cp_const_subst_applies`) consumed the stale optimistic KNOWN 0 — folding `.length` to 0 and the §15 IDX guard to ALWAYS-THROW inside `initProperties` (the property set's builder trapped; every lookup fell to miss/default). Fix: the follower-list re-arm in `cp_propagate`'s changed-branch, the exact mirror of the store re-arm above it — the recorded re-arm class's 6th instance (`test_cp_arraylen_follower_rearms_on_leader_descent`, RED-first). All pinned reproducers now compute correctly under -O. |

*(Excluded by Dan for now, but belongs on the map: the module-split summary artifact — ship each jre
export's `(escape,pts)` beside `jre.wasm`; its lemma is Choi §4.5's "a bottom-method summary is a valid
image", same as B8, applied cross-module.)*

---

## Result of the first pass

Every one of the 26 has a verdict. **No new soundness gap surfaced** — the ledger's own discipline
(every §-obligation carries a verbatim citation + a RED-first falsifier) already meets the audit bar,
and this pass confirmed it row-by-row rather than trusting the ✅ marks. The two things this pass
*added* were written obligations that had been left implicit: A6 reclassified `N/A` (Theorem 2's CS
half is not instantiated; its content lives in A5 + B7/B8), and C3/C7 given explicit discharges
(monotone directions + termination backstop; consumer-is-a-transfer verified by `cp_rewrite` computing
zero facts).

**Count (built, A–D): 19 `DISCHARGED` (13 `[chk]` re-verified here + 6 `[led]` via the ledger's
citation+falsifier), 7 `N/A` by documented architecture choice, 0 `PENDING`. Backlog (E): E1
`DISCHARGED` incl. the transitive-non-null increment (07-16); E2 `DISCHARGED` (07-16); E3
(optimistic header-keep) `GAP-UNBUILT` with its labeled red L0 pin and the failure analysis
recorded in its row.** The audit is now the complete map — what's proven, what's N/A, and
what's owed — not just what exists.

## What is honestly NOT established

- The `[led]` discharges are **spot-checked, not exhaustively re-derived** — I confirmed each carries a
  citation + a falsifier, not that I re-ran the full proof. The ceiling is re-deriving each `[led]` proof
  against the paper end-to-end.
- **Branch coverage of the falsifiers is uneven.** A discharge means "carries *a* falsifier," not "every
  branch of the instance is pinned." Gate 5 (A5/C5) was the live example; its four named branches are
  now all pinned (07-16): multi-hop `test_cp_load_forwards_across_two_preserving_calls`,
  array-no-forward `test_cp_array_load_does_not_forward_across_a_call`, non-singleton-receiver
  `test_cp_load_does_not_forward_across_a_call_two_object_receiver` (break-verified — deliberately
  breaking `cp_pts_sole_obj` reds ONLY this pin, so it is the sole guard on that fail-closure),
  write-a-different-field `test_sir` §44c. E2's join transfer likewise carries its negatives
  (disagree / refine-apart with order-blind mirrored asserts / def-on-a-path / latch-redefines),
  break-verified against a "join keeps one pred's value" mis-instantiation. The unevenness caveat
  still applies to the REST of the table.
- The RTL is a **yardstick, never an oracle**: byte-identical confirms no regression, never correctness.

**So: "verified" would mean all 19 re-derived `[chk]` and every instance branch pinned. Today it is
"complete inventory, every result carries a citation + a falsifier, no gap found" — which is the floor
the ledger was already at, now made explicit and cross-checked against the papers, not the tests.**
