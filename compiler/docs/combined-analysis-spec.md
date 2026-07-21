# Combined pointer / escape / value analysis over the SIR value-flow graph

Spec for folding context-sensitive pointer analysis (Li–Cifuentes–Keynes, *Precise and
Scalable Context-Sensitive Pointer Analysis via Value Flow Graph*, ISMM'13) and
connection-graph escape analysis (Choi et al., *Escape Analysis for Java*, OOPSLA'99)
into the **existing Click combined analysis/optimization fixpoint** — as new monotone
lattice elements, **with no dominator tree anywhere**.

## 0. The unifying observation

Both papers build a **value-flow graph** and then do reachability / monotone fixpoint on
it. The Click sea-of-nodes over pure-CPS SIR *is* that graph:

| paper concept | SIR / Click |
|---|---|
| VFG object node `O` (`p = &A`, BASE) | the `struct.new`/`array.new`/`array.new_fixed`/`new_default` node; string literals; the null object |
| VFG pointer var / ASSIGN `p = q` | an SSA ref value; **copies don't exist** (subsumed) — a merge is a φ node |
| VFG value-flow edge `q → p` | a def-use edge (already in the graph) |
| escape connection-graph *deferred edge* + `ByPass(p)` | same SSA subsumption — that machinery exists only because they work over a CFG |
| escape `N_r` reference nodes | SSA ref value nodes |
| escape `N_g` static-field / global nodes | module static-field globals (in the E7 split, **jre-imported** static-field globals) |
| escape/pointer field-sensitive heap | the ONE real addition: **memory-SSA over `struct.get/set`, `array.get/set`** |
| VFG CALL / escape call graph | the **defunctionalized** `call_ref` target set (the usually-hard part, given precise) |

So: build **one** heap-value-flow overlay; run **N** monotone lattices over the combined
graph in a single fixpoint; consume the results for guard elimination + devirtualization +
scalar replacement. Everything below is per-SIR-node transfer functions.

## 1. Substrate (the graph the lattices read)

Nodes already present (Click): every SSA value; every allocation. Add:

- **Abstract objects** `Obj`: one per allocation *site* (1-limited naming, both papers) —
  `struct.new τ@site`, `array.new τ@site`, each **string literal**, plus three synthetic
  kinds:
  - `⊥null` — the null object (feeds nullability).
  - `Oext@param` — a **phantom/external** object for anything reachable from a formal
    parameter or a global (escape's *phantom nodes* = the pointer paper's *auxiliary
    variables* — same device, both bound recursion the same way: one per (site, type)).
  - `Oret@callee` — the abstract "callee returned some ref" object for bottom methods.
- **Heap cells** `O.f` (field-sensitive) and `O.elem` (arrays **monolithic** to start, per
  escape §2.2 "each array as a single object"; make index-sensitive later off the range
  lattice). `f` is the stable `fid`. This is the memory-SSA overlay: a `struct.set(p,f,x)`
  is a *def* of `O.f` for every `O ∈ pts(p)`; a `struct.get(q,f)` is a *use*. A store
  reaches a load **iff** they touch the same `O.f` and no killing store intervenes on the
  value path — a **sparse** query over the value graph, *not* a dominance query (§6).
- **The exception-handler MERGE** (added 07-13 — the first draft omitted it, and the
  omission produced a live miscompile). **JLS §11.3.1, "Exceptions are Precise":** *"when the
  transfer of control takes place, all effects of the statements executed and expressions
  evaluated **before the point from which the exception is thrown** must appear to have taken
  place."* So a region's handler receives control from — and must see the state at — **every
  excepting point inside it**. The SIR's `TryRegion → handler` edge is REACHABILITY only; read
  as an ordinary one-predecessor continuation it hands the handler the region-ENTRY state,
  which is wrong the moment the region writes a slot before excepting: the catch then sees the
  pre-`try` value. (Choi §5.1's "kill only try-block-local refs" is the flow-INSENSITIVE
  approximation of this; our memory-SSA is flow-sensitive, so we model the merge exactly and
  strictly subsume him.)

  The fix is NOT an edge annotation — this design has no CFG to annotate. In the DDCG's own
  vocabulary: a `try` gives every excepting point in its body a SECOND control destination.
  **The handler is a γ**, the same kind of thing as the recorded `Ljoin` γs §8 already names
  (double-barrelled CPS; ρ carries it as `try_frame(region)`). So the handler is a **merge
  like any other, and merges are RECORDED (§8), never derived**: the DDCG mints every
  excepting node under ρ and records it against its region id *at the site it builds it* — it
  knows what it just built. **The optimizer never classifies nodes after the fact** (a
  post-hoc "can this node throw" taxonomy would be a second effect authority, the same disease
  as a dominator walk), and a region's EXTENT is the recorded id, never walked out of the
  graph. The end state is the γ carried ON the excepting node as its exception continuation —
  a real successor, so nothing anywhere synthesizes edges from the rows.

  **What each φ input carries is §11.3.1's own split — both sentences.** The excepting
  point's SLOT state is its **in**-state: sentence two, *"no expressions, statements, or parts
  thereof that occur **after** the point from which the exception is thrown may appear to have
  been evaluated"* — in `x = f()` the store to `x` occurs after the throw point inside `f`, so
  it never happened, and the handler sees x's prior value. Its HEAP-CELL state is its
  **out**-state: sentence one — the callee's writes occurred *before* the throw point, so they
  DID happen, and handing the handler the pre-call heap would forward stores the callee
  already clobbered. The asymmetry is not a rule to remember; with the γ on the node it is
  structural (the interrupted store lives in the NORMAL continuation).

  **WHICH nodes are excepting is a JLS question, not a backend question.** The SIR is the JAVA
  side; WASM is one runtime (a `.class` backend must work off the same graph). Derive the set
  from **JLS §11.1**: the synchronous causes are the conditions *"specified as a possible result
  of an expression evaluation or statement execution"* — explicitly including *"some limitation
  on a resource is exceeded, such as **using too much memory**"*, so **allocation is an excepting
  point** (`OutOfMemoryError`), regardless of whether a given runtime currently delivers it. §11.1
  also BOUNDS the set — these exceptions are *"**not thrown at an arbitrary point**"* — and §11.3.2
  confines the any-point (asynchronous) cases to `Thread.stop` (moot: Java 1.0 minus threads) and
  `InternalError`, while expressly permitting a code generator to defer them to control transfers.
  **So: throws, calls, and allocations — not every node, and never "whatever this VM happens to
  trap on."**

Nothing here is a new SIR node — allocations/field-ops are already nodes; `Obj`/heap-cells
are analysis-side facts keyed on them, and the handler's φ is the same analysis-side φ
every recorded merge gets.

## 2. Lattice A — points-to `pts` (the base everything else derives from)

- **Domain**: `pts(v) ⊆ Obj` for each ref value `v`; `pts(O.f) ⊆ Obj` per heap cell.
- `⊥ = ∅` (unreachable/uninitialized), `join = ∪`, `⊤ = Obj` (top — used for bottom-method
  results). Monotone-increasing under the combined fixpoint.
- **Transfer (per node)** — the VFG rules (BASE/ASSIGN subsumed) + INDIRECT for the heap:
  - `v ← new O`            : `pts(v) = {O}`   (BASE; the alloc node is the object)
  - `v ← φ(a,b,…)`         : `pts(v) = ⋃ pts(inputs)`  (ASSIGN/merge; plain copies don't exist)
  - `v ← ref.cast τ (u)`   : `pts(v) = { O ∈ pts(u) | classOf(O) ≤ τ }` — the cast **filters**
    pts by type. If the filter drops an `O`, that's the ClassCast §15 guard's live case;
    if it drops nothing, the guard is dead. `br_on_cast` splits `pts(u)` along its two
    successor edges the same way (per-edge facts, SCCP-style).
  - `struct.set(p,f,x)`    : ∀ `O ∈ pts(p)`: `pts(O.f) ∪= pts(x)`.
    **Strong update** (replace, not ∪) iff `pts(p)` is a singleton `{O}` **and** flow-sensitive
    (the store's memory-SSA name kills the prior def) — the VFG paper's Rule 3 / Theorem 3.
  - `v ← struct.get(q,f)`  : `pts(v) = ⋃_{O ∈ pts(q)} pts(O.f)` (INDIRECT / LOAD).
  - `array.set/get`        : identical with `O.elem`.
  - `global.set(G,x)`      : `pts(G) ∪= pts(x)`, and every `O ∈ pts(x)` gains a
    GlobalEscape source (§5). `v ← global.get(G)`: `pts(v) = pts(G)`.
    In the E7 module split a *static-field global* imported from jre is **external**:
    `pts = {Oext}`, GlobalEscape.
  - `v ← call f(a…)`       : `pts(v) = MapsTo(summary(f).ret)` (§4); bottom method → `{Oret}`
    (⊤-ish external) unless a per-native summary says purer.

## 3. Lattice B — class/type (reuse the type lattice as the authority)

Do **not** invent a second type domain — thread the existing type lattice (the WASM-repr
authority) as a combined element: `τ̂(v) = ⨆_{O ∈ pts(v)} exactClassOf(O)`, meet/join over
the class hierarchy + array collapse. Derived, so it's free once `pts` runs.

- **Consumers**: `pts(v)` singleton with exact class ⟹ **devirtualize** the vtable
  `call_ref` to a direct call (which re-exposes constants/inlining *inside* the same
  fixpoint); ⟹ drop the `ref.test` **ClassCast** guard and the covariant-store
  **ArrayStore** guard when the element class is provably ≤ the array's component.

## 4. Lattice C — nullability

- **Domain** `{⊥, NonNull, Null, Maybe=⊤}`. `null̂(v) = Null` if `pts(v)={⊥null}`; `NonNull`
  if `⊥null ∉ pts(v) ∧ pts(v)≠∅`; `Maybe` if both.
- **Transfer**: `new/ref.func` ⟹ `NonNull`; `ref.null` ⟹ `Null`; φ ⟹ join. The **branch
  refinement** is the key: on the `true` edge out of a `ref.is_null`/`!= null` test the
  operand is `Null`, on the `false` edge `NonNull` (per-edge facts, exactly SCCP's
  executable-edge mechanism — carried on the SIR edge, no dominance).
- **Consumer**: drop the §15 **NPE** guard at any `struct.get/set`, `array.*`, or
  explicit-receiver dispatch whose receiver is `NonNull` at that node. (This is the single
  biggest win — E7 emits an NPE guard at *every* deref site.)

## 5. Lattice D — integer range

- **Domain**: interval `[lo,hi]` per int SSA value (or a small constant-range lattice),
  with widening. `⊥ = ∅`, `⊤ = [MIN,MAX]`, join = interval hull.
- **Transfer**: consts ⟹ `[c,c]`; `+ - * <<` ⟹ interval arithmetic; `array.len(a)` ⟹
  `[0, ∞)` and binds an index var to that length; branch refinement on `<`, `<=`, `==`
  narrows the taken edge. Loop back-edges **widen** — read the loop scope from the
  **sidecar** (`compiler_try_region_t`-pattern recorded loop bounds), *not* a dominator-based
  natural-loop finder.
- **Consumers**: drop §15 **ArrayIndexOutOfBounds** when `idx ⊑ [0, len)`; drop **/,%
  by-zero** (ArithmeticException) when the divisor range excludes 0; drop the `INT_MIN/-1`
  overflow-wrap guard when the range excludes that pair; drop **NegativeArraySize** when the
  size range is `≥ 0`.

## 6. Lattice E — escape (connection-graph, but riding the same graph)

- **Domain** per object site `O`: `NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥)`, meet = min
  (escape §2.1). Monotone downward.
- **Sources** (escape's reachability seed set — Fig 6):
  - `GlobalEscape`: `O` reachable from a `global.set` (static-field global — incl. jre
    imports); stored into a field of an already-`GlobalEscape` object; the receiver of a
    class that **overrides `finalize()`** (finalizer thread reaches it, §5.2). (Thread/
    `Runnable` sources are effectively moot — Java 1.0 **minus** `synchronized`.)
  - `ArgEscape`: `O` flows to a `return`; is stored into a `param`-reachable object; or is
    passed to a `call` whose **summary** marks that parameter escaping.
- **Transfer** = propagate the source states along value + heap edges to a fixpoint (escape's
  Fig-6 worklist), meet at φ. A `throw` marks its exception object escaping **only if** it
  can leave the method: when an enclosing handler provably catches the thrown class (JLS
  §11.2 subtyping over pts, against the **recorded region ids** — §1), the object never
  leaves and does **not** escape; an uncaught (re-thrown) one is `ArgEscape` via the
  method's exceptional exit.
  ⚠ **Corrected 07-13.** The first draft said *"the catch continuation is in the graph"*,
  conflating two claims: handler REACHABILITY (true — `TryRegion → handler` is an edge, and
  with recorded extent it suffices for THIS escape rule) and handler DATAFLOW (false — the
  handler is §1's recorded merge; the raw graph hands it the region-entry state). It also
  cited escape §5 as *"kill only try-block-local refs"*; Choi §5.1 actually restricts
  **kills** — *"we kill only those [locals] declared within the block"* — which is the
  flow-insensitive approximation of §1's handler merge, not a statement about exception
  objects. The recorded merge strictly subsumes it. Neither claim licenses skipping the φ.
- **Consumer**: `NoEscape` ⟹ **scalar-replace** the `struct.new` — its fields become SSA
  values / locals, **zero GC allocation** (directly relieves the E7 Immix LOS + GC pressure).
  A scalar-replaced object also erases its own null/type guards.

### 6.1 The scalar-replacement model (Stadler et al., CGO'14 — the ONE consumer authority)

The consumer's design authority is *"Partial Escape Analysis and Scalar Replacement for
Java"* (Stadler/Würthinger/Mössenböck; on disk beside the Choi paper). Its model, §4/§5:

- A replaced allocation becomes a **virtual object**: an Id plus a per-field **state**,
  *"initialized with default values"* (= JLS §12.5's defaults). A field **store** *"sets the
  field value in the corresponding VirtualState"*; a field **load** *"replaces the Load with
  the value from the corresponding field of the VirtualState"*.
- The fail-closed rule, §5.2 verbatim: *"**Any operation that is not explicitly handled is
  assumed to require an actual object reference.** Therefore, any virtual object that is
  referenced from such an operation will be **materialized**."* Our whitelist qualification
  (D4) is exactly this rule with materialization degenerated to "decline the site" — v1 is
  **total, not partial**: an object is replaced only when NoEscape *everywhere*, so no
  materialization machinery exists. (The genuinely-partial variant — per-field φ at merges,
  materialize-at-predecessor, EscapedState — is the paper's §5.3 and needs flow-sensitive
  per-point state. It is a SEPARATE, future design; nothing in v1 may pretend to it.)

**The constructor.** The paper's listings run **after inlining** (its Listing 2/5: *"the Key
constructor … inlined"*) — the ctor's `this.f = e` stores sit in the caller's scope and fold
into the virtual state like any other store. **We have no inliner**, so the ctor's effect on
the object must come from the §7 summary instead, and the JLS says exactly what that effect
is: a constructor body = the explicit/implicit `super(...)` chain, then the class's instance
**field initializers in textual order**, then the ctor's own statements (§12.5 steps 3–5); a
**default constructor** (§8.8.9) is compiler-synthesized with *body = `super();` only* — plus
those compiled-in field initializers. Hence:

- The ctor chain's effect **on the object** IS its **transitive §7.2 write set on `this`**
  (which is why the write set must be transitive — §7.2 below).
- **v1 replacement condition** (all four, fail-closed; a miss ⟹ the site DECLINES):
  1. every constructor in the chain (C up to `Object`) `is_synthetic_default`;
  2. the chain-top summary's transitive write set on `this` is **∅**;
  3. the summary marks `this` CLEAN (NoEscape);
  4. the summary exists (a bottom-method ctor declines).
  (1)∧(2) ⟹ every body in the chain is literally `super();` and nothing else ⟹ the whole
  chain is a provable NO-OP ⟹ the call may be dropped when the allocation is dropped.
- **Named insufficiencies** — each alone is a MISCOMPILE, not a conservative loss:
  - (2) without (1): a *user* no-arg ctor in the chain can have effects that never touch
    `this` (a static write, a call, a throw) — invisible to the write set.
  - (1) without (2): a synthesized-default ctor is **not empty** when any class in the chain
    has instance-field initializers (`int v = 7;` compiles `this.v = 7` INTO the ctor body,
    §12.5) — dropping the call silently re-defaults every initialized field. This shipped
    once and failed 5 exec cases; those cases are the standing falsifiers.
- **Narrow fields**: an i8/i16 struct field narrows on `struct.set` and re-extends on
  `struct.get_s`/`_u` — the STORAGE performs byte/short/char semantics. An i32 local does
  not. So the rewrite's store side must normalize: `PutField(v)` of a byte/short/char field
  becomes `StoreLocal(i2b/i2s/i2c(v))`, and the slot invariantly holds the normalized value
  (loads stay plain). A slot rewrite without this narrows nothing and miscompiles
  `byte b; b += k` / `b++` (two of the same 5 falsifiers).
- **Class initialization is untouched**: JLS §12.4.1 triggers `<clinit>` on instantiation;
  the DDCG emits the `$ensure_init` barrier as its OWN ExprEffect *outside* the ctor call,
  so removing the allocation + ctor call preserves the trigger. The barrier is load-bearing;
  no cleanup pass may fold it away with the dead ctor.

## 7. Interprocedural — summaries over the defunctionalized call graph

Both papers converge on *summary + map-at-callsite*, bottom-up in **reverse-topological
order over the call graph** (escape §4). The defunctionalized `call_ref` target set makes
this precise and finite — the VFG paper spends its whole scalability budget approximating
exactly what you already have.

Per method `f`, the **summary** is:
1. **escape**: the *NonLocalGraph* — which formals/return/globals escape and the sub-graph
   reachable from them (escape §4.2). `NoEscape` objects never appear in the summary → not
   re-analyzed per call.
2. **pointer**: pts of the return + side-effected heap cells, expressed **relative to the
   formals' pts** (the VFG paper's "represent pointer values w.r.t. function inputs" — this
   is what makes the summary reusable without cloning).
   ⚠ **The write set is TRANSITIVE (corrected 07-15).** "Side-effected heap cells" means the
   cells this method's *execution* may write on formal-reachable objects — its own stores
   **and its callees'**, through every argument it passes along. A summary built from the
   method's own stores alone under-approximates. WHERE IT BITES (established against the
   code, not guessed): store→load VALUE forwarding already dies at any call (the forwarder
   demands the load's memory input BE the store, and a call interposes its kill vnode), so
   stale constants cannot survive — but the kill's surviving rows preserve **pts**, and pts
   feeds nullability, devirt, and τ̂. `f(p){ g(p); }` with `g(q){ q.r = null; }`: the caller's
   post-call `o.r` keeps pts `{d}` with no ⊥null, nullability proves NonNull, and a null test
   folds the wrong way — a miscompile with no escape-lattice symptom (everything is CLEAN).
   The fixpoint already computes the transitive set (each MapsTo marks the callee's write set
   on the mapped objects — the clobber facts); the summary is a READOUT of those facts
   unioned with the direct stores, never a second computation.

At a call site, **`MapsTo`** (escape Fig-7) instantiates the summary into the caller:
`actualᵢ ↦ formalᵢ`, `PointsTo(formal) ↦ PointsTo(actual)`, field-following via
`O.f ↦ Ô.g` when `O ↦ Ô ∧ fid(f)=fid(g)`; propagate `GlobalEscape` from callee params to the
mapped caller actuals; instantiate the input-relative pts with the actuals' pts. A virtual
`call_ref` fans out to its finite target set — join the per-target summaries (or clone
per-target à la VFG §3.1 when their local flows differ enough to matter).

**Bottom methods** (escape §4.5) = native / abstract / not-yet-analyzed **and — key for E7 —
cross-module `jre` imports**. Use the conservative *bottom graph* from the type system
(a ref returned by a native escapes; a ref passed to a native → `ArgEscape`), refined by
per-native summaries for known-pure natives (`Math.sin`, `floatToIntBits`, …). **Natural
extension of the module split**: ship each jre export's `(escape, pts)` summary alongside
`jre.wasm`, so a plugin analyzing `String.length()` uses the summary instead of a bottom
graph — the summary is a compile artifact, exactly like the type section.

## 8. Why there is no dominator tree (the whole point)

Every lattice above is a **per-node monotone element whose transfer reads only (a) def-use
edges and (b) φ/region inputs** — that is the membership test for the Click combined fixpoint,
and it is *precisely* the shape that needs no dominators:

- "Is value X available here?" — a value **is** a node; using it **is** an edge; GVN merges
  congruent nodes globally. No availability query.
- "Does this null-check dominate this use so I can drop the guard?" — the check's result is a
  nullability **fact on the value node**, propagated to every use along edges + executable
  edges. A use is `NonNull` iff the lattice says so *at that node* — dominance never enters.
- Flow-sensitive strong update ("does this STORE reach this LOAD un-killed") — a **sparse
  reachability** over the memory-SSA value graph (VFG paper's SSA formulation), not a
  dominator query.
- Merge points are **recorded** — the γ / `Ljoin` φ-region nodes AND the exception handlers
  (§1: a handler merges every excepting point of its region, recorded off ρ) — read from
  the DDCG structure / sidecar, never recompute a dominance frontier. This premise must be
  COMPLETE for the argument to hold: a merge the DDCG fails to record does not become "not
  a merge", it becomes a wrong reaching-def answer at that node. The handler was exactly
  that omission, and it cost a shipped miscompile before it was found.
- Loop widening reads the **recorded loop scopes** from the sidecar, not a dominator-based
  natural-loop finder.
- The *one* classic use of dominators — Click's GCM scheduling of the sea-of-nodes back to a
  CFG — you already don't do: **DDCG emits from the CPS structure**. So it's zero, end to end.

## 9. Staging (each step gate-verified, suite green)

1. **Memory-SSA overlay** for `struct.*`/`array.*` (the one real modeling add) + `pts`
   (lattice A) intra-procedurally, in the Click combined pass. No consumer yet — verify pts
   against hand cases.
2. **Nullability (C) + range (D)** — cheapest, biggest payoff: wire to the §15 guard emitter
   as an *elimination* (never weaken the guard where unproven — a guard stays when the
   lattice is `Maybe`/`⊤`; see `feedback_no_spec_carveouts_ever`, this is fail-**closed**).
3. **class/type (B)** → devirtualization + ClassCast/ArrayStore guard drop.
4. **escape (E)** intra-procedural → scalar replacement of `NoEscape` allocs.
5. **Interprocedural summaries** (§7), reverse-topological over the defunctionalized call
   graph; then jre-summary artifacts for the module-split bottom methods.

Each lattice is added as an element of the *same* fixpoint (Click's "combining" thesis: they
mutually enable — devirt → constant → NonNull → dead branch → SCCP prunes a region → tighter
type/range → more devirt — which no fixed pass order captures, and dead regions never get
analyzed because the executable-edge flag gates every sub-lattice).

## 10. Where it lives

A SIR/Click-level analysis (allocations + field ops must be explicit nodes first) — **not**
sema, which stays the AST-level authority (types, definite assignment, statement
reachability). This consumes the lowered value graph + the defunctionalized call graph and
feeds back three decisions: (1) drop a §15 guard, (2) devirtualize a `call_ref`, (3)
scalar-replace an allocation. The type lattice is consulted, never duplicated
(`feedback_lattice_is_the_representation_authority`).
