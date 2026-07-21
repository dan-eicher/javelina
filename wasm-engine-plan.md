# Embeddable, spec-conformant WASM engine — integrate loading/instantiation into the VM behind the W3C wasm-c-api

## Context

The bbqc binary parser (`wasm.bbq` → the parse-IR tree) is now spec-complete and
conformance-proven (binary 810/810, text 6327/0, accept + reject), and `water`
(the wat→wasm assembler) ships on it. The VM (interp + copy-and-patch JIT + Immix
GC + §7.6 validator) is mostly working — but there is **no module loader**. Every
test hand-wires the runtime: it pulls sections out of the parse tree, evaluates
init-exprs ad-hoc, fills `vm_t.functions/globals/table0`, and **never** type-matches
imports, applies §4.5 instantiation order, bounds-traps active segments, or runs the
start function. So the VM is not a spec-correct *loader*, and its execution behaviour
has never been gated against the official testsuite's *execution* assertions (today's
`test_wast` only checks parse verdicts).

**Goal (the capstone of the BBQ toolchain).** Turn the VM into an **embeddable,
statically- or dynamically-linkable, 100%-spec-conformant WebAssembly engine** that a
third party links against through the **standard W3C wasm-c-api (`wasm.h`)**. The
official testsuite's execution assertions (run through that public API, both tiers)
are the conformance gate. `water` and the `.wat` reader are tooling artifacts and are
**not** part of the library.

This composes the four BBQ generators into one product: **bbqc** (binary reader) +
**opgen** (interp + JIT metadata + stencils, from `wasm.def`) + **jitterator**
(copy-and-patch stencil table) + the new loader/instantiator + the wasm-c-api shell.

## Where this stands — the c-lite foundation, folded in (2026-06-19)

This plan predated the c-lite work. Since then `piped-riding-conway.md` landed the
FOUNDATION it feeds and is itself **done** — so read THIS plan, not that one:

- **Phase 0 (namespace split + `jav_status_t` verdicts) is DONE** — `wasm_*`→`jav_*`
  across symbols AND hand-written filenames; `jav_status_t` carries
  `JAV_MALFORMED/INVALID/UNLINKABLE/UNINSTANTIABLE` (which forced + got an opgen
  `StatusDecl` N-enumerator fix). `wasm_*` is fully reserved for the public API.
- **The load SOURCE is the c-lite zero-copy span-index, not the owning `jav_module_t`.**
  bbqc grew a `-backend c-lite` reader (`jav_view_reader`, runtime `bbq_lite`) that mmaps
  the `.wasm` and indexes spans, decode-on-access; `src/jav_view_nav.{c,h}` is the start of
  the navigation (locate sections by id, recover a code body's `[start,end)` span). So
  **`jav_module_index` (Phase 1) walks the span-index via `jav_view_nav`**, not the owning
  tree — the "owning-tree walk → span-index walk" swap. The owning reader/writer are now
  tooling-only (`water`), link-decoupled from the engine (`ENGINE_OBJS` vs `OWNING_OBJS`),
  and the two readers co-link (shared hand-written `jav_utf8.o` name validator) so an
  in-process conformance runner can transform-then-eval.
- **Phase 2's `test_instantiate.c` smoke exists** but currently hand-navigates the index;
  it is to be backed by `jav_module_index` (Phase 1) + `jav_instantiate` (Phase 2).
- Other foundation that landed: bbqc/opgen gaps fixed at the source (opgen `-prefix`,
  `StatusDecl` N-enumerators, the c-lite `where(bytes)` lowering, c-lite `@header`/`@source`
  emission); the call seam folded to one `invoke` dispatch (host has no kind-branch).

**So below, `wasm_*` = the public API, the internal core is `jav_*`, the load path is the
c-lite index, and Phase 0 is complete.** (The live state + remaining ordering are now the
2026-06-22 section directly below; the per-phase detail further down is reference.)

## Where this stands — object model PROVEN; remaining work ordered bbq → result (2026-06-22)

Phases 0–2 are DONE and now conformance-**proven**. The runtime object model is the **spec's** —
§4.2.3 store, §4.7 allocation/instantiation, §4.5.2 external typing — exposed through the W3C `wasm.h`.
The implementation choices the spec leaves open (GC root composition, the `instctx_t`/`frame.ctx`
context model) live as code comments at `wasm_store_t`/`capi_extra_roots` (wasm_capi.c) and `instctx_t`
(jav_frame.h). (The old `store-design.md` was a dead plan for the dead `jav_store_t` store — DELETED
2026-06-24; read the spec + `wasm.h` + the code, not a side doc.)
Built and green against the corpus:
- **§4.2.3 store + §4.5.2 external typing.** The §4.2.3 Store is **`wasm_store_t`** (the W3C
  wasm-c-api store derived from the spec, `wasm_capi.c`) over a shared `heap_t`: it owns the
  live-instance list (GC roots over EVERY instance on the one heap, each tracked from §4.7.2 step-24
  allocation via the `on_inst_alloc` hook), the host-object root sets, and instance lifetime (a
  trapped instance PERSISTS). Imports resolve **positionally** per the wasm-c-api contract (the
  embedder supplies externvals by position → `jav_instantiate`, which IS the §4.7.2 algorithm; the
  spec's `instantiate(s, module, externaddr*)` is itself positional). The §4.5.2 externval projection
  (`jav_project_export`) is shared with the loader. **(Correction 2026-06-24: the earlier separate
  `jav_store_t` name-registry store described here was a pre-API reimplementation with ZERO callers —
  the conformance runner + c-api both drive `wasm_store_t`. DELETED; only `jav_project_export`
  survives.)**
- **§3.3 closed-type matching — ONE relation.** A session-global closed-type registry on the heap
  (`jav_typereg_t`, typeidx→global canonical id). Funcs/tables/globals/tags + casts + validation all
  match via the SAME `jav_ht_sub` over global ids; addrtype (i32/i64) checked; `imp_ft_sub`/
  `imp_globval_sub`/the structural shortcuts are DELETED. The host path uses the SAME relation — **no
  carve-outs** (a structural shortcut here = an exploit; see the no-carve-outs rule).
- **§4.2 store identities.** funcref = funcinst pointer (instance-independent); tag = tagaddr
  identity carried through imports, so throw/catch match by identity and instantiation is generative.

Result: **0 module-link failures**, `make test` 76/0.

**The remaining work is ordered bbq → result. Fix each failure at the layer the conformance suite
points to, with the SPEC (not a guess from the code or the failing test) as the oracle for the rule.
Each lower layer is spec-clean before the next — so no lower-layer hack gets locked in as "the design"
and re-fixed three times.**

| # | layer | state | remaining task |
|---|-------|-------|----------------|
| 0 | bbq reader / assemble | ✅ **0 excluded** | (was 31 `out_of_scope`/"didn't-assemble" weasels — gone; `quote`/`definition`/`instance` now handled in the gates) |
| 1 | §7 validation | ✅ **0 excluded**, reject-reasons correct | (e.g. out-of-range table limit now `JAV_E_TABLE_SIZE`, not "min>max") |
| 2 | link / instantiate (the object model) | ✅ **0 module-link failures** | — (spec §4.5 + wasm.h) |
| 3 | **embedder / public API** | ✅ **projection unified** | The divergent COPY is gone: the externval projection is now ONE shared `jav_project_export` (heap-based, jav_store.c) called by BOTH `jav_store` (name resolution) and the capi's `marshal_import` (positional). The API's instance-export imports now inherit the proven loader — tag case, §3.3 closed-type matching, addrtype, §4.2 identity — instead of a hand-synced clone. `test_capi`/`embed` green. (What remains capi-specific — host-object marshaling + positional `wasm_instance_new`→`jav_instantiate` — is the wasm-c-api *contract*, not a clone of jav_store's name-resolution; host-created tags marshal lands with `wasm_tag_new`, currently unimplemented/unexercised.) |
| 4 | runner value marshaling | 56 excluded | the 56 `value form unsupported` — extend the runner's `parse_wval`/`wval_match` so every `assert_return` value form actually runs (Stage B1, below); each then converts to a VM-eval pass/mismatch |
| 5 | **execute / VM-eval (Phase 3 completeness)** | 172 mismatched + 5 crashes | the real instruction gaps the corpus is DISCOVERING: SIMD relaxed ops, GC array bulk ops (`array_init_elem`), control-flow faults (`if`/`labels`). Each: conformance locates → read the §-rule → fix the op. (Phase 3's "no feature discovered by the corpus" discipline was broken; this is the honest cleanup.) |


## North star / Definition of Done

1. A `libjavelina.{a,so}` exposing the standard `wasm.h`; an embedder can compile a
   module, instantiate it with host imports, look up exports, and call functions
   against **only the public header**.
2. The official `testsuite/*.wast` **execution** corpus passes — `module` /
   `assert_malformed` / `assert_invalid` / `register` / `assert_unlinkable` /
   `assert_uninstantiable` / `invoke` / `get` / `assert_return` / `assert_trap` /
   `assert_exhaustion` — driven through the engine, **both interp and JIT tiers**, with
   any genuinely out-of-core-3.0 exclusions **counted and named** (never silently
   skipped).
3. The library is namespace-clean: `wasm_*` is the public API and nothing else.

## Testing discipline & anti-weasel contract (enforced per phase)

The same contract that got the parser to 0-mismatched applies here. A phase is DONE
only when **every item on its enumerated spec checklist has a passing fixture or a
corpus case that exercises it** — coverage is measured against the spec list, never
against "what the happy path needs."

1. **The gate is the official testsuite, executed.** Hand-written unit tests pin
   specific byte-exact behaviours; the *breadth* gate is the `.wast` corpus run through
   the real pipeline. A phase that "passes its own tests" but regresses corpus count is
   not done.
2. **No silent verdict collapsing.** `assert_malformed` ≠ `assert_invalid` ≠
   `assert_unlinkable` ≠ `assert_uninstantiable` ≠ `assert_trap`. The runner must
   distinguish all five (see the status model below); "it failed somehow" is a weasel.
3. **No silent simplifications.** Anything the spec mandates is modelled; the only
   permitted shortcuts are spec-*equivalent* ones, documented inline (e.g. applying an
   active data segment as a bounds-checked `memcpy` instead of lowering to
   `memory.init`+`data.drop` — same observable result, called out as deliberate).
4. **Reuse, don't reinvent — and NEVER hand-roll a container (BINDING RULE, not advice).**
   Const-expr eval, `jav_typecheck_ex`, the host/JIT dispatch seam, `jav_code_entry_bytes`
   already exist — wire them, each named by `file:symbol`. **For data structures this is a
   HARD rule:** every collection is a BBQ crt container (`/home/dan/Source/BBQ/crt/`), so
   the length always rides WITH the data:
   - `bbq_vec.h` — growable array; **`bbq_vec_len(v)` IS the count** (no parallel field).
     Module/instance state (funcs, globals, `table0`, exports, the per-run exn/handler
     stacks) is a `bbq_vec`; `jav_instance_bind` projects the pointer onto the `vm_t`.
   - `bbq_arena.h` — bump arena, single free; the index's flattened tables live here.
   - `bbq_htree.h` — O(1) uint32 map / visited-set; the export map, the name→instance
     registry, the declared-funcref (`C.refs`) set.
   **FORBIDDEN — the exact smell to catch in review:** (a) a fixed-cap C array `T x[MAX_N]`
   with `i < MAX_N` guards — it silently truncates valid input and invents a non-spec limit
   (`globalidx`/`datacount` are u32, NOT 256/64); (b) a bare pointer + a parallel `num_x`
   count — the count rides with the container. The substrate's pre-existing
   `globals[256]`/`data_dropped[64]`/`exns[256]`/`handlers[256]`/`exn.fields[16]` are bugs
   to convert (`globals` → `bbq_vec` DONE), **NOT a pattern to copy.** If a `vm_t` field is
   an array, it is bound from an instance `bbq_vec`; the only permitted fixed numbers are
   validator-ENFORCED frame limits that trap cleanly (`MAX_STACK`/`MAX_LOCALS`/`MAX_CALL_DEPTH`).
5. **Generated code stays generated.** Where a handler/struct must change (status enum,
   a `vm_t` field shape), edit `wasm.def` / the grammar and **regenerate** — never
   hand-edit `gen_interp.c`, the stencil table, or the bbqc output.

## The one-time namespace split (precondition, Phase 0)

`wasm_*` becomes the public W3C API's exclusive namespace. Every identifier that is
`wasm_*` *internally today* moves to `jav_*`:

- **Parse IR (bbqc-generated):** regenerate `wasm.bbq` with `-prefix jav` →
  `jav_module_t`, `jav_module_read`, `jav_code_entry_bytes`, `jav_section_t`, every
  section/instr struct. (One flag; the bulk is a mechanical consumer sweep.)
- **Hand-written runtime symbols:** `wasm_func_t`, `wasm_call`, `wasm_status_t`,
  `wasm_typecheck[_ex]`, `wasm_validate_const_expr`, `wasm_st_entry_t`, `wasm_try_t`,
  `wasm_br_table`, `wasm_try_table`, `wasm_vm_init`, `wasm_mem_*`, … → `jav_*`.
  Where `wasm.def` handler bodies name these (e.g. `wasm_call`, `wasm_func_t`), edit
  `wasm.def` and regenerate opgen.
- **Leave alone:** `vm_t`, `frame_t`, `slot_t`, `heap_t`, `bbq_*` — not `wasm_*`, no
  clash, no churn.

This is mechanical but touches most of `src/` and `test/`; do it first, in isolation,
and gate on "full build + all 63 existing tests green after the rename."

## Architecture (settled by the embedding requirement)

An embedder hosts *multiple* modules, links them, and supplies host imports — so a
**module-instance record + a store/registry is mandatory**, not optional. The design:

- **`jav_instance_t`** — the §4.5 module instance: owned **`bbq_vec`s** (rule #4 — never
  hand-rolled arrays) of funcs (imports in the low slots, then own), globals, `table0`,
  exports, and the per-function side-tables; plus the memidx range into the store, the
  (index-arena) flattened type table, passive elem/data segments, and the **export map**
  (name → kind+addr) for linking. `bbq_vec_len` is the count — no parallel `num_*`.
- **The store** = `heap_t` (linear memories + the Immix GC heap) + a **name→instance
  registry** (for `register` / cross-module imports / the synthetic `spectest` module).
- **`vm_t`** stays the execution engine (the one mmap'd value/locals pool, the frame
  stack, call-depth). `jav_instance_bind(vm, inst)` **projects** an instance onto the
  `vm_t` fields the generated handlers already read (`vm->functions`, `vm->globals`,
  `vm->table0`, `vm->types`, …) — cheap pointer assignment, no data copy, and crucially
  **no `wasm.def` change** because the field *names* are unchanged. One engine
  multiplexes the store across instances, exactly as a real engine does.

The public `wasm.h` is a thin shell over this internal `jav_*` core (Phase 4).

## Status / verdict model (Phase 0, opgen-regenerated)

Extend `status` in `spec/wasm.def` (the opgen-generated enum, today
`WASM_OK WASM_TRAP WASM_RETURN` → renamed `jav_status_t`) with verdict codes appended
at the end (so existing `== TRAP` checks are unaffected): add
`JAV_MALFORMED`, `JAV_INVALID`, `JAV_UNLINKABLE`, `JAV_UNINSTANTIABLE`. These are
**loader/instantiator return verdicts only** — the interp/JIT machine must keep
producing only `OK`/`TRAP`/`RETURN`. Regenerate opgen.

### The fine error-reason model (`jav_err_t` + `jav_err_str`) — the message-level oracle

`jav_status_t` gives the five coarse *categories*; it is NOT enough to pass the corpus.
Every `assert_invalid`/`assert_malformed` command carries the **official expected error
string** (`type mismatch` — 1867×, `constant expression required`, `duplicate export
name`, `unknown {global,function,table,memory,type,label,local}`, `size minimum must not
be greater than maximum`, `undeclared function reference`, `start function`, `memory
size`, `incompatible import type`, …), and the runner matches it (substring/prefix). So
the verdict model needs a **fine reason**, not scattered string literals at each `return`:

- ✅ **`src/jav_error.{h,c}` built** (`jav_err_t` + `jav_err_str`); `jav_module_validate`
  returns `jav_status_t` + `jav_err_t`, pinned in `test_module_validate` (verdict + the
  official reason per case). Surfacing through `wasm_trap_t`/`jav_lasterror` is the Phase-3
  step below. The enum so far:
- **`src/jav_error.{h,c}`** — a `jav_err_t` enum of the wast error vocabulary
  (`JAV_E_NONE=0`, `JAV_E_TYPE_MISMATCH`, `JAV_E_CONST_EXPR_REQUIRED`,
  `JAV_E_DUP_EXPORT_NAME`, `JAV_E_UNKNOWN_GLOBAL/FUNCTION/TABLE/MEMORY/TYPE/LABEL/LOCAL`,
  `JAV_E_LIMITS_MIN_GT_MAX`, `JAV_E_MEMORY_SIZE`, `JAV_E_UNDECLARED_FUNC_REF`,
  `JAV_E_START_FUNCTION`, `JAV_E_IMMUTABLE`, `JAV_E_INCOMPATIBLE_IMPORT`, …) and a single
  **`const char* jav_err_str(jav_err_t)`** to_string — the ONE place the official text
  lives. The enum is the source of truth (derived from the corpus's error set); the text
  switch is matched against the `.wast` expected string.
- **`jav_module_validate` returns the `jav_err_t`** (out-param alongside `jav_status_t`),
  set once at each rejection point — no inline string literals in the checks.
- **It must surface through the wasm-c-api channels the runner actually consumes**, NOT by
  the runner reaching past the API into the validator. The bare standard `wasm.h` has no
  message slot for module errors (`wasm_module_new`→NULL, `wasm_module_validate`→`bool`);
  the only message-bearing error is `wasm_trap_t` (out of `wasm_instance_new`/`wasm_func_call`).
  So `jav_err_t` flows out two ways: (a) **trap-producing failures** (start trap, OOB active
  segment, runtime) → `jav_err_str(reason)` becomes the `wasm_trap_t` *message*; (b)
  **decode/validate** → a `jav_lasterror`/store-last-error extension on top of `wasm.h` (the
  shim stashes the `jav_err_t` on the store; the runner reads `jav_err_str` from it). The
  conformance runner matches the expected `.wast` string against the trap message / the
  store last-error — going **through the API**.
- **Sequencing consequence:** message-level matching is therefore gated on the Phase-3
  wasm-c-api module surface (`wasm_module_new`/`wasm_module_validate` + the lasterror
  extension), not bolt-on to `test_wast`. Phase 1 builds `jav_module_validate` →
  `jav_status_t`+`jav_err_t`; the corpus *message* gate lands once that minimal surface
  exists. Where `jav_typecheck` collapses several §7.6 reasons into one bool (e.g.
  `alignment must not be larger than natural` vs `type mismatch`), threading its specific
  reason out is a later refinement — tracked, not silently mapped to `type mismatch`.

**Important public-API note (anti-weasel):** the bare W3C `wasm.h` *collapses* these —
`wasm_module_new`→NULL covers malformed+invalid, `wasm_instance_new`→NULL/trap covers
unlinkable+uninstantiable. So the conformance runner cannot get fine verdicts from the
bare standard API. Resolution: the **internal** `jav_module_validate` / `jav_instantiate`
return the precise `jav_status_t` + `jav_err_t`, and the runner (in-tree, allowed to use
internals) calls those to distinguish the five `assert_*` categories **and** the
message-level reason **while still exercising the public `wasm.h` path for the happy
cases**. (Equivalently, a tiny `jav_lasterror` extension on top of `wasm.h`, the
wasmtime-over-wasm.h pattern.) Do not let the standard
API's coarseness become an excuse to check only "failed somehow."

## Phases (each independently gated)

### Phase 0 — Namespace split + status model  ✅ DONE
- The `wasm_*` → `jav_*` rename (above) + the `jav_status_t` extension; bbqc + opgen
  regenerated. Landed in piped-riding-conway (symbols + filenames; `make test` 64/64).

### Phase 1 — Module validation gate (§7) + shared index builder
- **`jav_module_index(const bbq_field_capture* root, const uint8_t* base, bbq_arena*, jav_modidx_t* out)`**
  (`src/jav_module_index.{c,h}`) — walk the **c-lite span-index** (`jav_view_nav` over
  `jav_view_module`), not the owning tree, and flatten it once into the tables both the
  validator and instantiator need (tables allocate into the same arena as the index).
  **This is on the critical path for Phases 1 and 2 — build it first.**
  - **Built + pinned** (`test_module_index` on `test/rich.wasm`, ASAN-clean): the flattened
    type space (rec-group-correct typeidx) with func signatures; the §3.3 lattice
    (`kinds`/`supers` → `jav_subtype_ctx_t`); the func/table/mem/global/tag index spaces
    (imports in low slots, then defs); global mutability/import flags; `mem_is64`; elem/data
    counts. `valtype`/`limits`/`reftype` decode is spec-faithful to §5.3.
  - **REMAINING sub-pin — struct/array field packs** (`structtypes` / `arraytypes` /
    `type_field_packs`, the GC composite types). Gated on first extending the boundary
    `jav_valtype_t`/`WVT_*` enum (opgen / `wasm.def` regen) to carry the abstract GC
    heaptypes (`any`/`eq`/`none`/`nofunc`/`noextern`/`noexn`, abstract struct/array) +
    concrete func refs — the §5.3 forms the decoder currently **fail-closes** on. Do both,
    representation-first, the moment a GC/`ref` `.wast` (or the `assert_invalid` GC subset)
    reaches the index — **not before, not never.**
- **`jav_module_validate(const bbq_field_capture* root, const uint8_t* base, const jav_modidx_t*) -> jav_status_t`**
  (`src/jav_module_validate.{c,h}`) — the reader's `m.success` is the §5 malformed gate;
  this is the §7 *invalid* gate over a successfully-indexed module. Returns `JAV_OK`/`JAV_INVALID`.
- **Reuse:** `jav_typecheck_ex` (validate.c) per body — `jav_vctx_t` built from the index
  tables (`base_cx`), RLE locals decoded via `jav_index_decode_valtype` into `cx.locals`.
  Body span = the FuncBody `body` (Expr) node's `[start,end)` from the c-lite index. (The
  emitted side-tables/try-tables are currently freed — the instantiator re-derives them;
  stash-for-reuse is a Phase-2 optimization, not a correctness item.)
- **§7 checklist** — ✅ = built + pinned (`test_module_validate`: `add`/`rich` accepted;
  `bad_lim`/`bad_body`/`bad_dupexp`/`bad_start` rejected, each a distinct §-reason):
  - ✅ every function body type-checks (the §7.6 pass);
  - ✅ **start function type is `[]→[]`** (§3.5.12);
  - ✅ **export names unique** + all export indices in range (§3.5.10);
  - ✅ table/memory **limits well-formed** (§3.2.12/15/16: `n≤k`, `n≤m≤k`; mem32 `2¹⁶`,
    mem64 `2⁴⁸`, table32 `2³²−1`, table64 `2⁶⁴−1`);
  - ✅ **global init-exprs** (§3.5.3): const-only + typed to the declared type, `global.get`
    only of an **in-scope immutable** global (imports + earlier defs; the const rule's
    `C.globals[x]=t` pattern is the immutability check), `ref.func` only of a func in
    **`C.refs`** (§3.5.10, collected from exports/start/elem func-lists + ref.func in
    global/elem const-exprs). Admissible set extended for `ref.null`/`ref.func`
    (`const_admissible`); `const_scan` + `jav_typecheck` typing in `jav_module_validate.c`.
    Pinned: `refs` accepted; `bad_gtype`/`bad_gmut`/`bad_gfwd` rejected.
  - **REMAINING sub-pin (B2) — segment const-exprs + index ranges:** active **elem/data
    OFFSET** exprs (typed to the target table/mem addrtype — i32, or i64 for table64/mem64),
    **elem element-expr lists** (each typed to the elem reftype), **table-init** exprs (the
    `0x40` form, under C′ = imported globals only); plus the segment **index ranges** (elem
    tableidx/funcidx, data memidx, import-desc typeidx). `const_expr_ok` already exists —
    this is navigation + the per-site result type. Also still open: the multi-byte
    **v128.const / GC const ops** (`0xFD`/`0xFB`) — the SIMD/GC const-expr extension, paired
    with the `WVT_*` enum work. Build before the instantiator consumes init-exprs.
- **Gate:** the `assert_invalid` + `assert_malformed` corpus subset via the runner stub —
  every invalid module rejected, every valid module accepted, 0 mismatched.
  - ✅ **`src/jav_load.{h,c}` built** — `jav_validate_bytes(bytes, len, &err)`: the
    bytes → §5 decode (c-lite) → §7 validate pipeline in ONE place. This is the entry the
    Phase-3 `wasm_module_new`/`wasm_module_validate` shim WRAPS — the runner and the shim
    share it, neither re-implements it. The conformance runner calls it (not inlined glue).
  - ✅ **`test_wast` drives the §7 gate over `../testsuite/*.wast`** (in-tree stub calling
    `jav_validate_bytes`): valid module → `JAV_OK`, `assert_invalid` → rejected. **Binary
    subset: 99 ok / 0 mismatched, ENFORCED** (the suite fails on a §7 regression). Surfaced
    + fixed a real bug against §5.5.12: a funcidx-list elem's element type is `(ref func)`
    NON-NULL (flags 0–3), not nullable `funcref`. TEXT modules are still excluded (binary
    only) until wat→wasm assembly is wired (Phase 5); message-text matching is Phase 4
    (`jav_lasterror`). NOTE: a pre-existing `wat_parser` text-reader leak shows under ASAN
    on `test_wast` (not introduced here; the `-O2` make build doesn't ASAN it).

### Phase 2 — Store + instance + instantiator (§4.5)
- **`src/jav_instance.{c,h}`**: `jav_instance_t`, the store/registry,
  **`jav_instantiate(module, base, store, imports, nimports, &out) -> jav_status_t`**
  in the spec's exact order, and `jav_instance_bind(vm, inst)` / `jav_instance_free`.
  - ✅ **Foundation pin built** (`test_instantiate` on `rich.wasm`, ASAN-clean): the full
    pipeline decode → index → §7 validate → **`jav_instantiate`** → **`jav_instance_export`**
    (name→funcidx) → `jav_call`, no hand-wired table. `jav_instantiate` builds the
    defined-func table (step b: code span + counts + type-idx + **re-derived side-tables**;
    imports occupy the low slots so `rich`'s exported `"add"` is funcidx 1) and the export
    map; `jav_instance_bind` projects `funcs`/`types`/`tags` onto the `vm_t`.
  - ✅ **Step (c) global-init eval built** (`test_instantiate` extended, ASAN-clean): each
    defined global's init is a const-expr run on the interp (`interp_run` over the expr
    bytes) with imports + earlier globals in scope, stored on the instance and copied onto
    `vm->globals`/`global_types` by `bind`. Pinned: numeric (`i64`=7), `global.get` chaining
    (`$b = global.get $a`), `ref.func` in a global init. Const-eval runs on the **engine
    `vm` passed into `jav_instantiate`** (not a scratch engine) — funcs visible for
    `ref.func`, earlier globals mirrored into `vm->globals` for `global.get`. (Imported
    global VALUES are host-supplied — deferred with step (a); their low slots stay 0.)
  - ✅ **Steps (d/e) + (f) built** (`test_instantiate`, ASAN-clean): linear memory
    (`jav_mem_add` into the store heap), `table0` (funcref, null-filled), **active data**
    (offset → bounds-check → `memcpy`) + **active elem** (offset → bounds-check → write
    funcidx/ref), OOB → `JAV_UNINSTANTIABLE` (`JAV_E_OOB_MEMORY`/`_TABLE`); and the **start
    function** invoked on the engine vm via the tier seam, trap → `JAV_UNINSTANTIABLE`.
    Pinned on `rich`/`refs` (memory `"hi"`, `table0=[func 1]`/`[0,1]`), `start_ok` (sets a
    global), `start_trap`. `bind` projects `table0`. **Surfaced + fixed a real interp gap:
    `unreachable`(0x00)/`nop`(0x01) were absent from `wasm.def` → added to the spec +
    regenerated (interp+JIT), not hand-coded.**
  - ✅ **Step (a) host imports built** (`test_instantiate`, ASAN-clean): `jav_instantiate`
    takes a POSITIONAL `jav_extern_t* imports` array (the wasm-c-api contract — name
    resolution is the embedder's). `link_imports` walks the import vector in declaration
    order, type-matches each against the declared import (func: exact `jav_functype_eq`,
    promoted from `jav_runtime.c` and reused; table/mem: limits subtyping + addrtype; global:
    type + mutability; tag: functype), and drops the value into the matching low slot — funcs
    (host callback via the `jav_invoke_host` seam, re-stamped with OUR `type_index`), globals
    (value + tag), an imported table 0 borrowed (not owned/freed), an imported memory mapped
    through the new `mem_addrs` (module memidx → store-heap memidx, replacing `mem_base`).
    Arity / kind / type mismatch → `JAV_UNLINKABLE` (`JAV_E_UNKNOWN_IMPORT` /
    `JAV_E_INCOMPATIBLE_IMPORT`, "unknown import" / "incompatible import type"). Pinned:
    `rich` (host func + i32 global into low slots, `funcs[0].invoke==jav_invoke_host`),
    `import_call` (an export that CALLS the host import → `host_inc(41)==42`), the
    type-mismatch + arity unlinkable verdicts, `refs` (imported i32 global). The globals +
    `mem_addrs` vecs now allocate up front so linking fills the low slots before const-eval.
  - ✅ **Passive-segment stash built** (`test_instantiate` + `passive_segs.wasm`, ASAN-clean):
    `jav_instantiate` registers EVERY data/elem segment into instance bbq_vecs — `data_segs`
    `{bytes,len}` (into the image), `elem_segs` `{values,len}` materialized into one reserved
    `elem_pool` (ref.func idx / null = −1), with `data_dropped` (one flag per data segment).
    Active segments are applied (offset → bounds-check → mem/table write) THEN marked dropped;
    passive/declarative ones stay reachable by `memory.init` / `array.new_data` / `array.new_elem`.
    `jav_instance_bind` projects `data_segs`/`elem_segs`/`data_dropped` onto the vm. **Converted
    the substrate `vm->data_dropped[64]` fixed cap → a bound `u1*` bbq_vec sized to the segment
    count** (rule #4) — the old `seg < 64` guards in `memory.init`/`data.drop`/`array.new_data`/
    `array.new_init` were a silent truncation (drop on segment ≥64 was a no-op); removed.
  - **Phase 2 COMPLETE** (steps a–f + passive stash). Consistency cleanup done: shared
    `jav_view_*` nav, `jav_module_cx`, `jav_body_typecheck`; `vm`-first arg order. Substrate
    bbq_vec pass: globals + all instance tables + `data_dropped` converted; the REMAINING fixed
    caps are the exception subsystem's (`exns[256]`/`handlers[256]`/`exn.fields[16]`), to convert
    with exception hardening.
  - TRACKED GAP (not silent): elem segments have no per-segment *dropped* flag — `data_dropped`'s
    mirror — because `elem.drop`/`table.init` aren't in `wasm.def` yet. So active + declarative
    elem segments are stashed into `elem_segs` but not marked dropped; `array.new_elem` on a
    declarative/active segment doesn't trap (it would once `elem_dropped` lands with the table
    bulk ops). Data side is fully correct (`data_dropped` mirrors active/passive/drop).
- **Steps (each mapped to existing machinery):**
  - (a) resolve + **type-match imports** → `JAV_UNLINKABLE` on arity/type mismatch
    (reuse `functype_eq`; extend for table/mem limits subtyping + global mutability);
  - (b) **pre-allocate own funcs** (code span, params/locals/results, type-idx,
    side-tables from Phase 1) — funcs exist before init-exprs so `ref.func` resolves;
  - (c) **eval global inits in order** via the interp const-eval pattern
    (`bbq_ctx_init` the expr bytes → `interp_run` → store `vm.result`), each appended so
    the next sees it;
  - (d) **active elem**: eval offset (i32) → **bounds-check vs table size →
    `JAV_UNINSTANTIABLE` if OOB** → copy in; passive/declared → stash for `table.init`;
  - (e) **active data**: eval offset (i32) → **bounds-check vs memory size →
    `JAV_UNINSTANTIABLE` if OOB** → `memcpy` (spec-equivalent to `memory.init`/`data.drop`,
    documented); passive → stash;
  - (f) **start**: invoke via the tier seam; a trap → `JAV_UNINSTANTIABLE`.
  - build the **export map**.
- **Reuse:** the host/JIT/interp dispatch seam (`jav_call` in wasm_runtime.c), the
  const-eval pattern (test_const_expr.c), `jav_code_entry_bytes`.
- **Enumerated §4.5 checklist:** import arity, per-kind type match (func exact, table/mem
  limits, global type+mut, tag), global-init ordering with imported+earlier globals in
  scope, all 8 elem variants × {active bounds-trap / passive / declarative}, all 3 data
  variants × {active bounds-trap / passive}, start present/absent + trap propagation,
  failure-before-observable-mutation where the spec requires it.
- **Gate:** `test_instantiate.c` end-to-end (parse → validate → instantiate → call an
  export) **both tiers**, plus the `assert_unlinkable` / `assert_uninstantiable` corpus
  subset.

### Phase 3 — Engine instruction-set completeness (before any API or corpus run)
**Discipline (the point of this phase): no feature is discovered by the conformance corpus.**
Each missing instruction family is implemented via opgen (`wasm.def` + native + regen — never
hand-coded, like `unreachable`/`nop`) and pinned by a **committed unit test written FIRST to
the spec, seen red, then green**, run on **both tiers** (interp + JIT, asserting agreement —
the house differential) with spec-enumerated positives AND negatives. This is exactly the
discipline `test_bulk_mem` / `test_gc_segments` already model. The corpus (Phase 5) then only
*confirms breadth* — it is never where a missing opcode first surfaces. Reactive
"build-API-then-fix-what-the-corpus-explodes" is the explicit anti-goal here.

**BINDING — read the spec for each opcode BEFORE implementing it; do NOT work from memory.**
For every opcode, READ its three spec sections from `../WebAssembly.pdf` (Read tool, `pages=`)
first: **§5.4** binary encoding + immediate operand vocabulary, **§3.3** validation typing
rule, **§4.4** execution/reduction semantics (incl. the exact trap conditions). The unit test
and the handler are written to THOSE sentences — not to whatever's in context, not guessed
from a sibling opcode. This is the whole reason for the audit-first ordering: we pay the
spec-read cost once, per op, up front, so the corpus confirms rather than catching invented
behavior. (`reference_wasm_spec` has the read-the-PDF mechanics.) Cite the section in the
test/commit so the next reader can check it against the spec, not against my recollection.

- ✅ **(0) Coverage audit DONE (2026-06-19) — the gap list is definitive, not guessed.**
  Diffed `spec/wasm.def` (parsing BOTH standalone `name 0xNN` and the grouped
  `{ i32 i32_add 0x6a … }` macro form) against all **497** ops in `spec/instructions.toml`
  (the generated §5.4 opcode table `water` already uses). Result: **464 implemented, 35
  missing.** Cross-checked against the generated interp (`i32_add` present, `select`/`load8_s`
  absent) — authoritative, since the interp/JIT are generated from `wasm.def`. **40 testsuite
  files exercise the missing core ops**, so these are exactly the Phase-5 blow-ups the audit
  pre-empts. The five families below ARE the finite list.
- ✅ **(1) Core memory sub-word load/store (15) DONE** (`test_subword_memops`, both tiers,
  red→green; `make test` 68/0, corpus 810/0 + 6327/0 intact; gap 35→20). Spec-read first
  (§4.6.8 exec, §3.4.5 validation). Each `extend^sx`/`wrap` is a dedicated native that IS the
  spec op (e.g. `extendˢ_{8,64}` = `(s1)byte` returned `s8`) — no implicit C promotion. The
  validator routes all 15 through `tc_mem` with `N`=storage width (8/16/32), so `2^align≤N/8`
  is enforced (pinned: `i32.load8_s align=1` / `load16_s align=2` REJECTED — the verifier is
  not nerfed). NOTE: memory ops call the native in the JIT stencil too (the tracked engine-wide
  inlining item below); correct + consistent with all existing loads.
- **Cross-cutting OPTIMIZATION (tracked, engine-wide — not a one-off):** memory loads/stores
  call a C helper in BOTH tiers, so the JIT stencil emits a `call` (through a patched hole),
  not an inline `movzx`/`movsx` + inline bounds-check. Pure computation (`add`, compares) IS
  inlined; the memory/call/GC family is helper-based across the whole engine. This is a
  legitimate copy-and-patch pattern (small stencils, one trap path, base-moves-on-grow handled
  once) but it is NOT the JIT's best form. The clean factoring — one bounded-raw-read helper
  per width + INLINE `extend^sx` in the stencil — needs an opgen body-parser feature (accept
  width casts `(s1)`/`(u1)`) and applies to ALL ~20 memory ops at once. Do it uniformly or not
  at all; never bolt inlining onto sub-word loads alone.
- ✅ **(2a) `select` untyped (0x1b) DONE** (`test_select`, both tiers; `make test` 69/0; gap
  20→19). Spec-read (§4.6.1 exec, §3.4.1 validation). Value-polymorphic over the WHOLE v128-wide
  slot via opgen's `word` type — and the body `result = c ? v1 : v2; result_wt = c ? v1_wt :
  v2_wt;` compiles **INLINE in the JIT stencil** (no helper call). Validator (hand-written case,
  like `drop`): refuses reference operands (those need typed select) and unequal operand types —
  pinned: `select` on funcref REJECTED, `select` of i32/i64 REJECTED. Bot = dead-code wildcard.
- ✅ **OPGEN vec-operand feature BUILT (br_table/try_table on it; `make test` 69/0).** opgen now
  has tail-immediate operand kinds (`TyBrTable`/`TyTryTable`/`TySelectVec`), declared in
  `grammar/opgen.peg` (regenerated parser) + `opgen_ast.h`, emitted by `vmemit.cpp` as a per-op
  `tail` field (`JTAIL_*`) in `wasm_jit_meta_t` (excluded from the fixed-operand array + interp
  decode). `wasm.def` now declares `br_table [brtable labels]` / `try_table [trytable catches]`,
  and **`jit_driver.c` is DATA-DRIVEN** (`switch (m.tail)`) — the three hardcoded
  `if (op == OP_BR_TABLE/OP_TRY_TABLE)` skips AND the false "only such opcode" comment are
  DELETED. Verified inert at each increment, then `test_brtable`/`test_jit_brtable`/
  `test_exceptions` green. `JTAIL_SELECTVEC` is wired in jit_driver, ready for select_t.
- ✅ **(2b) `select t` typed (0x1c) DONE** (`test_select` extended, both tiers; 69/0). Uses the
  new `TySelectVec` tail: opgen emits an interp-side `vec(valtype)` skip (the native-read tails
  emit nothing) and NOTHING in the stencil (the JIT walk skips via `m.tail`) — verified in the
  generated `gen_op_select_t` (has the skip) vs `gen_st_select_t` (pure inline select). Validator
  case (typed: count==1, decode the result type, pop 2 of it + i32, push it — accepts refs, which
  untyped rejects). Pinned: `select_t i32/i64`, `select_t funcref` ACCEPTED, `count=2` REJECTED.
  KNOWN limit (shared with blocktype): abstract reftype shorthands beyond funcref/externref go
  through `tc_vt_from_sleb` — extending that shorthand decoder is a separate, cross-cutting item.
- **The `select` family (0x1b + 0x1c) and the opgen vec-operand feature are COMPLETE.** Gap → 18.
- ✅ **(5) `call_ref` (0x14) DONE** (`test_call_ref`, both tiers; 70/0; gap → 17). Mirrors
  `return_call_ref` but nested (pop the funcref; null/OOB → trap; the validator proved the static
  type, so no dynamic gate). Validator case `t1* (ref null x) → t2*`. Pinned: `f0(41)=42`, null→trap.
- ✅ **Validator dispatch refactor (Dan-flagged smell): the single-byte `if/else (op == …)` chain
  is now an ordered `switch (op)`.** ~290 lines, 32 cases sorted by opcode (the prefixed/`tc_mem`/
  generic fallback is the `default`). Done as a scripted, uniform transform (boundaries
  `} else if (op == X) {` → `} break; case X: {`) + a block-sort, each step gate-verified — §7
  stays 99/0. Jump table instead of linear scan, and cases findable in order (no more grepping;
  the out-of-order insertions were mine).
- ✅ **(3) Table management + bulk ops (6) DONE** (`test_table_ops`, both tiers; `make test`
  73/0; gap → 0). `table.size` (0xFC 0x10), `table.grow` (0x0F), `table.fill` (0x11),
  `table.copy` (0x0E), `table.init` (0x0C), `elem.drop` (0x0D). The multi-table substrate is
  the indexed `vm->tables` bbq_vec (`jav_tableinst_t` = `refs`/`max`/`reftype`/`is64`, the
  table analogue of `heap->mems` — NOT a single `table0`); `elem_dropped` mirrors
  `data_dropped` and closes the Phase-2 elem gap. Each op is a §4.6.7 native (grow honors
  `max`, returning old size else −1; fill/copy/init OOB → trap, copy is `memmove` overlap-safe;
  init traps on a dropped/short segment). Validator cases (sub 12–17) route through
  `tbl_rt`/`tbl_at`/`elem_rt` so reftype-match + addrtype are enforced. The pin drives all six
  over a two-table store + a passive elem segment: `size`=4, `grow`+`size`=6, `grow`-over-max=−1,
  `fill`→`get`→`is_null`, cross-table `copy`+`call_indirect`=105, `init`+`call_indirect`=105,
  and the spec-enumerated traps (fill/copy/init OOB, init-after-`elem.drop`).
- ✅ **(4a) SIMD widening loads (8) DONE** (`test_simd_loadext`, both tiers; 71/0; gap → 9).
  `v128.load{8x8,16x4,32x2}_{s,u}` + `v128.load{32,64}_zero`. Each is a dedicated `mem_loadKxM_sx`
  native that IS the §4.6.8 op (read 8 bytes / N-bit lanes, `extend^sx` each to 2K via typed
  lane stores; zero forms read N/8 into lane 0, zero the rest). Validator `tc_mem` cases: load-
  extend N=64 (8-byte access, align ≤ 3), `load32_zero` N=32, `load64_zero` N=64. Pinned: per-lane
  sign vs zero extend + upper-lane zero-fill.
- ✅ **(4b) SIMD compute (3) DONE** (`test_simd_compute`, both tiers; 72/0; gap → 6). SIMD
  family COMPLETE. `f64x2.promote_low_f32x4` is the inline lane-iterated `result = a[lane]`
  form (compiles to a per-lane f32→f64 in both tiers). `i8x16.popcnt` is **inline** — a SWAR
  bit-twiddle in the lane body (pure arithmetic, no libcall reloc in the stencil), NOT a native.
  `f32x4.demote_f64x2_zero` is asymmetric (4 out lanes ← 2 in + zero fill), so a whole-vector
  native — keyed by declaring `v128→v128` operands so opgen does NOT lane-iterate (the lane
  model would pass a single lane to the v128 native). Lesson: opgen lane-iterates any op with
  lane-typed operands, incl. native calls — use `v128` operands to opt out.
- ✅ **(5) `call_ref` (1) DONE** (above). (Struct/array **field packs** validator sub-pin
  remains, tracked separately.)
- **Validator dispatch refactor DONE** (Dan-flagged): the single-byte `if/else (op==…)` chain →
  ordered `switch (op)`, then **cleaned** (a scripted transform left dangling `} break;` + ragged
  comments — reformatted to idiomatic: `break;` last, comments aligned above each case). §7 99/0
  throughout. See [[feedback_mechanical_transform_must_read_clean]].
- ✅ **Gate CLOSED:** all 35 implemented + each family's committed both-tier test is green
  (sub-word mem, select ×2, call_ref, SIMD ×11, table mgmt ×6); a re-run of the (0) diff shows
  0 missing. `make test` 73/0. Phases 4–5 may now begin.

### Phase 4 — Public wasm-c-api (`wasm.h`), the embeddable surface
- ✅ **SCAFFOLD LANDED** (`src/wasm_capi.c`, the only TU including the public header;
  `make test` 74/0). The W3C `include/wasm.h` is vendored verbatim from `WebAssembly/wasm-c-api`.
  The object model: `wasm_store_t`→`vm_t`+`heap_t`, `wasm_module_t`→owned bytes+`bbq_arena`+
  c-lite root+`jav_modidx_t`, `wasm_instance_t`→`jav_instance_t`, and func/global/table/memory/
  extern handles are common-initial-sequence `{kind,inst,index}` VIEWS (the type objects use the
  same `wasm_externkind_t`-prefix trick for the `as_externtype` casts). **Implemented spine,
  bridged to the core:** the full type/vec machinery (every `wasm_*_vec_*`, type ctor/accessor/
  copy/delete), `wasm_module_new`(=`jav_view_module`→`jav_module_index`→`jav_module_validate`)/
  `wasm_module_validate`(`jav_validate_bytes`), `wasm_instance_new`(no-imports → `jav_instantiate`)/
  `wasm_instance_exports`, `wasm_func_call` (marshal `wasm_val_t`↔`slot_t`, `jav_instance_bind`+
  `jav_call`, single depth-0 result back) + arity/type reflectors, the extern cast web, and
  `wasm_trap_t` messages. **Gate met:** committed `examples/embed.c` links ONLY `wasm.h` + the lib,
  instantiates an exported add module, calls it → `add(3,5)=8` (the `embed` test-target gate).
#### Phase-4 sub-tasks — sequenced (each spec-grounded, each its own committed gate)
**STATUS (2026-06-20): 4a–4e ALL DONE, incl. the table-store substrate change so funcref AND
externref/GC tables work (slot-sized entries + per-entry GC tag). Gated by `test_capi`, `make
test` 75/0, ASAN+UBSan-clean incl. a 20k-alloc GC churn with a live externref (as a value AND
held in a table). Remaining: standalone host-created `wasm_global_new/table_new/memory_new` +
serialize/deserialize/share/foreign-finalizers/frame-traces/config — genuinely unexercised by
the core corpus; implement on demand, NOT gated out.**

**STATUS (2026-06-22) — REBASE DONE (projection unified).** The divergent copy is eliminated: the
externval projection is now ONE shared `jav_project_export` (heap-based, jav_store.c, declared in
jav_store.h), called by BOTH `jav_store`'s name-resolution path AND the capi's positional
`marshal_import`. The API's instance-export imports inherit the proven loader (tag case, §3.3
closed-type matching, addrtype, §4.2 store identity) instead of a hand-synced clone — so the
security-critical type-matching is no longer re-derived in `wasm_capi.c`. Host-object marshaling +
the positional `wasm_instance_new`→`jav_instantiate` stay in the capi (that is the wasm-c-api
*contract*, not a jav_store clone). `test_capi`/`embed` green, `make test` 76/0.

**STATUS (2026-06-22, UPDATED) — KEYSTONE LANDED: the conformance runner drives the public `wasm.h`.**
`test/wast_exec.c` was re-pointed entirely onto the public C-API: every module goes through
`wasm_module_new`/`wasm_module_validate`, imports are resolved BY NAME at the harness level into the
positional `wasm_extern_vec_t` that `wasm_instance_new` consumes (§7.1 embedding done properly), and
every action runs through `wasm_func_call`/`wasm_global_get`. The engine makes every type/semantic
decision; the ONE non-`wasm.h` readout is the sanctioned `jav_capi_last_status`/`jav_capi_last_error`
(declared in `jav_store.h`, NOT `wasm.h`) — the §5/§4.5 verdict the spec surfaces to an embedder only
as NULL+trap, which the harness needs to classify malformed/invalid/unlinkable/uninstantiable/trap.
The conformance build now links `src/wasm_capi.c`. Conformance preserved: **59883 ok / 173 mismatched
/ 57 excluded** (was 59867/172/56 under the internal-API runner); all mismatches are VM-eval (the §1
allowed class), `make test` **76/0**. Surface gaps closed en route, each a real engine fix (not a
runner workaround): exception/trap split (`wasm_trap_is_exception`, §7.1.8) + an `assert_exception`
handler; **tags as first-class externvals** (`externkind_of_jav_export`/`externtype_of` for kind 4 →
`WASM_EXTERN_TAG`/`wasm_tagtype`, null-safe `wasm_externtype_delete`) — was crashing
imports/instance/tag/try_table; **`valkind_of_wvt(WVT_V128)`→`WASM_V128`** (a dormant gap that
mismatched ALL ~18k SIMD result cases until fixed); §4.5.4 trapped-instance store-lifetime ownership
in `wasm_instance_new`.

**STATUS (2026-06-23) — GC-REF MARSHALING DONE: exclusions 57 → 0.** The conformance corpus is now a
COMPLETE test of the engine through the public `wasm.h` — every value form handled, zero runner-side
gaps. **59944 ok / 169 mismatched / 0 excluded**, `make test` **76/0**. Implemented the §7.1 value +
matching surface and the GC-ref bridge:
- **§7.1.14 `wasm_ref_type(store, ref)`** — the runtime reference type a value is valid with, as a
  wasm.h ref valtype (abstract heaptype = a less-precise supertype, which §7.1.14's Note permits).
  Delegates to a new engine helper `jav_ref_abstract_heaptype` (mirrors `value_heaptype`'s GC branch).
- **§7.1.15 `wasm_match_valtype` / `wasm_match_externtype`** — ⊢ t1 ≤ t2 over the ONE §3.3 lattice
  (`jav_ht_sub`); externtype matching reuses `match_valtype` for every ref component (no 2nd comparator).
- **GC-ref value marshaling**: `wasm_ref_t` gained a managed-gcref case (slot+tag+store); `val_of_slot`
  routes refs on the RUNTIME `slot_tag` (host-box externref vs managed struct/array vs i31 vs funcref),
  `slot_of_val` passes a gcref's slot back through; `valkind_of_wvt`/`ht_of_valkind` extended with the
  full HT_*↔GC-valkind map; **C-held managed gcrefs are GC-rooted** in the store (`gc_refs` vec scanned
  by `capi_extra_roots`, un/rooted on ref delete/copy) — no use-after-free.
- **Runner**: `parse_wval`/`val_match` handle `(ref.i31|struct|array|eq|any|none)` (via `ref_type` +
  `match_valtype`) + `(ref.host N)`; a `match_expected` helper handles the `(either …)` relaxed-SIMD
  non-determinism form; names compared by TRUE byte length (WASM names may embed NUL — names.wast).
- **Memory-safety verified**: full-corpus ASAN+UBSan run — ZERO heap-use-after-free / buffer-overflow /
  `wasm_capi` errors; the gcref-creating files (i31/struct/ref_test/ref_cast/extern) are clean.

FINDINGS (pre-existing engine bugs surfaced by the ASAN sweep, NOT from this work — record for later):
`jav_runtime.c:786` `arr_len` reads a misaligned/bogus array ref in array_init_elem.wast (the global
`(ref $arrref_mut)` array ref is corrupt — a real VM-eval bug, one of the 169 mismatches; the file
creates no gcrefs so the marshaling path is inert there). Plus pervasive benign UBSan signed-overflow/
shift in `gen_interp.c` (WASM-defined wrapping arithmetic expressed with C signed ops).

**STATUS (2026-06-23) — §7.1.11 TAGS + §7.1.12 EXCEPTIONS DONE; the §7.1 surface is complete.** No more
"additive / not-corpus-exercised" deferral (Dan: that framing was the priority-gating weasel — the
corpus confirms, it doesn't decide what to build). Implemented + unit-tested in `test_capi`:
- **§7.1.11 tags**: `wasm_tag_t` (host or instance-export view), `wasm_tag_new` (tag_alloc — a fresh
  store tagaddr `heap.next_tag_id++` + functype, store-owned), `wasm_tag_type`, `wasm_extern_as_tag` +
  host-tag import marshaling (`marshal_import` kind-4 → intern functype, inherit `tag_id`), tag identity
  via the tagaddr.
- **§7.1.12 exceptions**: `wasm_exception_t` (exnref), `wasm_exception_new` (exn_alloc), `wasm_exception_tag`
  (exn_tag), `wasm_exception_read` (exn_read); the §7.1.8 exception OUTCOME of `wasm_func_call` now carries
  the escaped exception (`wasm_trap_exception`, built from the engine's exn store); exnref value marshaling
  both directions (`val_of_slot`/`slot_of_val` WASM_EXNREF ↔ the engine `vm->exns` index).
- **exn-ref GC rooting**: the engine's root scan does NOT trace `vm->exns` (fine internally — throw→catch
  is synchronous), so surfacing an escaped exception to the embedder would dangle a GC-ref payload across a
  later GC. Closed it: host exceptions holding T_GCREF values are tracked in `store.exn_roots` + scanned by
  `capi_extra_roots` (the same fail-safe discipline as `gc_refs`).
- **Removed the stale "DEFERRED banner"** + the `(4a)/(4e-ii)` plan-phase vocabulary from `wasm_capi.c`
  comments (Dan: a DEFERRED list is itself the not-silently-deferring-by-comment anti-pattern; implement,
  don't annotate). `wasm_config_t` is the lone empty extension point (no standard fields, per the header).

The 169 mismatches are all VM-eval (SIMD/GC/control/exceptions/array) — the §1 allowed class, a separate
concern from the API surface.

**STATUS (2026-06-22) — the full §7.1 surface is the contract; the runner must drive IT.** There is
NO canonical 3.0 `wasm.h` to download (verified: wasm-c-api `main`+branches and WebAssembly/spec all
lack a C-API — the spec's embeddings are JS-API/Web-API + the **§7.1 Embedding** abstract interface).
So the authority is §7.1.5–7.1.15 itself, extracted in full (nothing deferred as "speculative"):
- **Values (§7.1.14 / §4):** `val = num | vec | ref`. Header gains `WASM_V128` + `wasm_v128_t`, the GC
  ref kinds (`any/eq/i31/struct/array` + `none/nofunc/noextern`), and `exnref`/`noexnref`; plus
  `val_default`, `ref_type` (reflection), `match_valtype`/`match_externtype` (the engine's relation
  exposed — runner asks, never re-derives).  ⟵ value kinds + `wasm_val_t.v128` + marshaling DONE.
- **Three outcomes (§7.1.6/§7.1.8):** `func_invoke`/`module_instantiate` return `success | exception |
  error`. A thrown WASM **exception** (`exnaddr`, carries tag+values) is DISTINCT from a trap. The
  classic header collapses both into `wasm_trap_t*` — must split.
- **Exceptions (§7.1.12):** first-class — `exn_alloc`/`exn_tag`/`exn_read` (+ `exnref` value).
- **Tags (§7.1.11):** host `tag_alloc` (`wasm_tag_new`).
The endpoint: **the conformance runner drives the public `wasm.h`** (every op = a `wasm_*` call; the
engine makes every type/semantic decision; the SOLE non-API readout is the fine error enum
`jav_err_t`/`jav_err_str` for `.wast` message matching, which `wasm.h` doesn't surface) — so the corpus
becomes a test of the real engine THROUGH the real library, not of what it could do hacked around a
crippled API. Implementation order (each built+gated): value kinds → GC-ref marshaling + `ref_type` →
exception object + the exception/trap split → host `tag_alloc` → `match_*` accessors → **re-point
`wast_exec.c` onto `wasm.h`** (delete `parse_wval`'s type logic; keep only `.wast` script parsing).
The scaffold's deferrals (banner in `src/wasm_capi.c`) are NOT a flat list — they have a
dependency order: host funcs unblock imports, imports + reflection + ref values are what the
Phase-5 `spectest` host module needs to link. Spec citations are the WASM 3.0 (2026-06-03)
printed section numbers; the **Embedding appendix is §7.1** (the abstract embedder interface the
C API binds). Do them in this order:

- ✅ **(4a) Host functions + the import path — DONE** — §7.1.8 (`func_alloc`/`func_invoke`), §4.2.7
  (a host function receives an arg `val*` and yields `val*` / exception / trap). Implement
  `wasm_func_new`/`_with_env`: allocate a c-api closure `{functype-copy, callback, env, finalizer,
  store}` and install a **c-api-local invoke thunk** as `jav_func_t.invoke` (ctx = the closure) —
  do NOT reuse `jav_invoke_host`, which hardcodes a single `T_INT` result. The thunk marshals
  `vm->frame.locals[0..nparams)` → `wasm_val_vec` (kinds from the functype), calls the callback,
  and on a returned trap sets `vm->trapped`/returns `JAV_TRAP`, else marshals results back
  (zero/single result first: set `vm->result`+`vm->result_type`; **multi-result host funcs are a
  follow-on** — they must land contiguously at the callee frame base, the §-return path at
  `jav_runtime.c:185`). Then `wasm_instance_new`: marshal each `wasm_extern_t` import → `jav_extern_t`
  (host-func → `{kind 0, func.invoke=thunk, func.type=functype}`; instance-export views → the
  exporter's storage), DROP the "reject non-empty imports" guard, and let `jav_instantiate` do the
  §4.5.2/§3.3.16 type-match (already wired via `jav_functype_eq`) → `JAV_UNLINKABLE` surfaces as the
  out `wasm_trap_t*`. **Gate:** through the PUBLIC api — `wasm_func_new(inc)` imported into
  `import_call.wasm`, `wasm_func_call(callit, 41)` → 42, both tiers.

- ✅ **(4b) Multi-result returns — DONE** — §7.1.8 (`func_invoke` returns a value *sequence*). `wasm_func_call`
  reads N results from `vm.frame.stack[0..N)` (the multi-value return already lands there;
  `jav_runtime.c:187`), marshaling each by `functype.results[i]` kind — not just `vm->result`.
  Small; can ride with 4a. **Gate:** an exported `(i32,i32)→(i32,i32)` swap returns both.

- ✅ **(4c) Instance-export object accessors — DONE** (global get/set + type, memory data/size/grow + type, table size + type; the global/table/memory `wasm_extern_type`; host-created `_new` + ref-typed table get/set/grow ride with 4e) — §7.1.9/10/13 (`table_read/write/size/grow`,
  `mem_*`, `global_read/write`; indices/sizes are **u64**, memory in 64Ki pages, writing an
  immutable global is the **error** case). Bridge `wasm_global_get/set`, `wasm_table_get/set/size/
  grow`, `wasm_memory_data/data_size/size/grow` onto the instance-export VIEW's storage
  (`inst.globals[i]` / `inst.tables[i]` / `heap.mems[mem_addrs[i]]`); fill in `wasm_extern_type`
  for global/table/memory (reconstruct the type from `jav_modidx_t`). **Gate:** read an exported
  global, grow an exported memory (size in pages), `table.get` an exported funcref — via the api.

- ✅ **(4d) Module reflection — DONE** (real section walk via `jav_view_section_array(root,7|2,…)`; the empty-vec QUIET stub is GONE) — §7.1.6 (`module_imports`/`module_exports` yield `externtype`),
  binary §5.5.5 (import sec id 2) / §5.5.10 (export sec id 7) / §5.5.1 (`externidx`: `0x00` func …
  `0x04` tag, all `u32`). Walk the sections with the EXISTING primitive
  `jav_view_section_array(root, 7|2, …, base)` (the same call `jav_instance.c` already uses to
  build the export map) → `wasm_exporttype_t{name, externtype}` / `wasm_importtype_t{module,
  name, externtype}`, the `externtype` reconstructed from `jav_modidx_t` by kind+index.
  ⚠ **This is the one QUIET stub today**: until this lands, `wasm_module_imports/_exports` return
  EMPTY vectors, which an embedder can misread as "no imports/exports" (every other deferral fails
  *loud* — NULL or a trap). Flag stays in the file banner until implemented. **Gate:** `module_exports`
  on the add module → 1 export `"add"` : func `(i32,i32)→i32`; `module_imports` on `import_call` →
  the named host import.

- **(4e) Reference values** — §4.2.1, §4.5.1, §7.1.14 (`ref_type`). Splits along the engine's
  representation (confirmed by reading the runtime):
  - ✅ **(4e-i) funcref — DONE (encoding unified by A1, 2026-06-23).** A funcref is the §4.2.1 funcinst
    *pointer* in `slot.r` (`T_REF`; `JAV_NULLREF` = null) — the SAME encoding on the engine and c-api
    sides (A1 killed the old c-api funcidx anomaly). `wasm_ref_t`/`wasm_func_t` carry `{fn, fnstore}`;
    FUNCREF `wasm_val_t` ↔ `slot.r`; `wasm_table_get/set/grow` on funcref tables; `func`↔`ref` casts;
    `wasm_val_copy/delete` own ref payloads. Gate in `test_capi`: `ref.func` returns a non-null funcref,
    `ref.null` returns the NULL ref, table get/set/OOB + `wasm_ref_same`, plus the A1 host↔guest
    `call_indirect` block.
  - ✅ **(4e-ii) externref host VALUES — DONE (GC host box).** `jav_host_box_new/_get/_is_host_box`
    in the core runtime (`jav_runtime.c`, with `src/jav_hostref.h`) wrap a host `void*` in a
    GC-managed object (`JAV_HOST_BOX_RTT`: kind STRUCT, `nrefs=0`); an externref VALUE is that box
    pointer, tagged `T_GCREF` in `slot.l` (scanned by `jav_gc_enum_roots`), null = `T_REF`/`JAV_NULLREF`.
    `wasm_store_new` now binds a live collector (`jav_heap_gc_init`). EXTERNREF marshaling boxes/unboxes
    (with incremental `sp` so each box roots before the next alloc); `global.set` keeps the `T_GCREF`
    tag (engine already does `global_types[i]=v_wt`); `wasm_foreign_t` is the host-reference mint.
  - ✅ **Table-store substrate change — DONE (the Phase-3 (3) "non-funcref/externref-table" half).**
    `jav_tableinst_t` now stores SLOT-SIZED entries (`s8* refs`) with a PARALLEL per-entry tag
    (`u1* types`, T_REF/T_GCREF) — an `externref` may carry a scalar or a managed pointer, so tracing
    is per-entry, matching the value stack's tag model (not a tagged-pointer hack). `table.get/set/
    grow/fill` were converted to STACK-NATIVE reftype-polymorphic ops in `wasm.def` (opgen-regenerated
    handlers + stencils), `call_indirect`/elem-init read the scalar funcidx, and `jav_gc_enum_roots`
    now traces `T_GCREF` table entries. The c-api table ops + import ABI carry the `types` array.
    **Verified end-to-end** (`test_capi`): wasm `table.set/get/fill` on a `(table externref)`, the c-api
    `wasm_table_get/set/grow`, and a table externref **surviving a 20k-alloc GC churn** — `make test`
    75/0, ASAN+UBSan clean. funcref tables stay both-tier green (`test_table_ops`/`test_call_indirect`).

- ✅ **Cross-cutting refinement DONE: §3.3.16 import subtyping** (was exact `jav_functype_eq`).
  `link_imports` now uses spec-correct matching via the `jav_subtype` lattice: func types
  structural with params CONTRAVARIANT / results COVARIANT (`imp_ft_sub`); table reftypes
  INVARIANT (`imp_vt_equiv`); globals const→covariant, var→invariant (§3.3.13); limits were
  already correct (§3.3.11). `imp_vt_sub` maps `WVT_*`→`(nullable, heaptype)` (mirrors the
  validator's `vt_from`) and defers to `jav_rt_sub`. Gated in `test_instantiate`: a non-null
  `(ref func)` global links against a nullable `funcref` import; `externref`≰`funcref` stays
  unlinkable.
  - ⚠️ **Cross-module CONCRETE func references `(ref $t)` — PARTIAL, was overstated as ✅ (corrected
    2026-06-21).** Decode + validate handle `(ref $func)`; `link_imports` matches concrete func-ref
    globals by their functype STRUCTURE (`imp_globval_sub` → `imp_ft_sub`, provider sig in
    `jav_extern_t.global.ref_functype`). Gated only on `linking.wast` Mref / `import_grefsub.wasm` =
    the EXACT-match case (structurally-equal links; different signature unlinkable). The structural
    matcher is INCOMPLETE vs §3.3 closed-type matching: it FAILS on `sub final`, rec-group canonical
    equality, and cross-rec-group concrete refs — the 5 open `type-subtyping.wast` `assert_unlinkable`
    cases (instrumented: simple recursive-func rejects correctly in isolation; the hard iso-recursive
    cases don't). The real fix is the **store-level closed-type registry** (one `jav_ht_sub` over
    global canonical ids; deletes `imp_ft_sub`) — the closed-type-registry work. STILL not handled
    either: cross-module CONCRETE struct/array refs (need the struct/array field-type model in the
    modidx; not expressible through the c-api `wasm_valkind`).

- ✅ **Peripheral surface — DONE** (was wrongly parked as "lower-priority"): standalone host-created
  `wasm_global_new`/`table_new`/`memory_new` (store-owned, GC-rooted via a new `vm.extra_roots` hook,
  and linkable as imports through `marshal_import`); `wasm_module_serialize`/`_deserialize` +
  `wasm_*_share`/`_obtain` (the serialized form IS the module's §5 bytes — lossless re-decode, since
  there's no separate AOT artifact); `wasm_foreign_t` (the host-reference mint). Gated in `test_capi`
  (host global linked as an import + read back; memory grow; serialize round-trip), `make test` 75/0,
  ASAN+UBSan clean.
- ✅ **host-info DONE** — `wasm_*_get/set_host_info[_with_finalizer]` on every handle (a `void*` +
  finalizer in the handle structs via `HOST_INFO_FIELDS`; the finalizer runs on delete; copies don't
  inherit it). Gated: set/get + finalizer-runs-on-delete.
- ✅ **trap traces DONE** — `jav_call` records the func-index unwind chain into a new `vm->trap_trace`
  bbq_vec; `wasm_trap_origin`/`wasm_trap_trace` surface it (`wasm_trap_t` carries the frames + instance).
  Gated: `unreachable` in `$inner` propagating through `$outer`/`go` → trace `[0,1,2]`, origin `$inner`.
  (func indices exact; per-frame byte offset not captured — the trapping pc is clobbered on trap.)
- **The ONLY unimplemented c-api function is `wasm_config_t`** — an embedder extension point with NO
  standard fields per the header; there is nothing to build. The wasm-c-api surface is otherwise
  COMPLETE; `make test` 75/0, ASAN+UBSan clean across all of `test_capi`.

The original surface enumeration (still the definition of "done" for the spine):
- **Marshaling:** `wasm_val_t` ↔ internal `slot_t` (the engine value); host callbacks
  wrap a `jav_instance_t` func slot with `host` set. Expose **per-function tier choice**
  (`JAV_INTERP`/`JAV_JIT`) — tiering is the embedder's explicit call, not runtime
  second-guessing (the settled tiering decision).
- **Enumerated API checklist:** every type/function the conformance corpus exercises is
  implemented; anything deferred (e.g. `wasm_foreign_t`, finalizers) is **listed
  explicitly**, no silent gaps.
- **Gate:** a standalone `examples/embed.c` that links **only the public header** + the
  built lib, instantiates `add.wasm`, calls it, prints 8 — builds and runs against both
  `.a` and `.so`.

### Phase 5 — Conformance runner as reference embedder + the 100% gate
**This phase CONFIRMS breadth; it does not discover features.** Phase 3 already implemented +
unit-tested every instruction family (audit-driven, both tiers), so the engine is complete
*before* this runner exists. A corpus failure here means a runner/marshaling/`spectest` bug or
a narrow edge a unit test missed — NOT "we never built this opcode." If the corpus surfaces a
whole missing feature, that's a Phase-3 escape to fix at the source with its own committed
test, not to patch reactively inside the runner.
- Rewrite/extend `test/test_wast.c` to drive the **public API** (proving the API is
  real) for the happy path, using the internal `jav_*` verdict returns to distinguish
  the five `assert_*` categories. Handle every command:
  `module`(+`$id`) → registry; `assert_malformed`/`assert_invalid`; **`register`** +
  name→instance registry; `assert_unlinkable`/`assert_uninstantiable`;
  `invoke`/`get` actions (marshal `(t.const v)` args → `wasm_val_t`, call, read results);
  `assert_return` with **NaN canonical/arithmetic + ref-value comparison** (not `==`);
  `assert_trap`; `assert_exhaustion` (the call-depth guard already traps). Run **both
  tiers** and assert agreement (the house differential discipline).
- **The synthetic `spectest` host module** (the single biggest chunk, called out
  honestly): a hand-built `jav_instance_t` pre-registered as `spectest`, exporting
  `print*`/`global_i32`(666)/`global_i64`/`global_f32`/`global_f64`/`table`(funcref 10..20)/
  `memory`(1..2). Prerequisite for a large fraction of the corpus to even link.
- **Engine completeness is a Phase-3 PREREQUISITE, not work that happens here.** The multi-
  table / non-funcref-table substrate change and the missing table/elem opcodes
  (`table.size/grow/fill/copy/init`, `elem.drop`) + SIMD + GC gaps land in Phase 3 with their
  own committed both-tier tests. By the time this runner exists they are done; the corpus only
  re-exercises them in aggregate. (This bullet used to read "structural extensions the full
  corpus forces" — that framing was the reactive anti-goal and is corrected above.)
- **The synthetic `spectest` host module** is the legitimate Phase-5-specific build: a hand-
  built `jav_instance_t` pre-registered as `spectest` (it's a *test embedding*, not an engine
  feature) — see the dedicated bullet above.
  - ✅ **The fixed-cap arrays are already gone per rule #4**: `vm->globals`/`global_types`
    are `bbq_vec`s bound from the instance (`MAX_GLOBALS` deleted; `bbq_vec_len` is the
    count; this also fixed a GC root gap — globals were never scanned). The instance's
    `funcs`/`sidetabs`/`trytabs`/`table0`/`exports` are `bbq_vec`s too, and **`data_dropped`
    is now a bound `u1*` bbq_vec** (the `[64]` cap is gone, done with the passive-segment wiring).
  - REMAINING fixed caps to convert the same way (rule #4): `exns[256]`/`handlers[256]`/
    `exn.fields[16]` (with exception hardening). `num_tables` stays a count (table COUNT,
    not a length). `MAX_STACK`/`MAX_LOCALS`/`MAX_CALL_DEPTH` stay — validator-enforced.
- **Gate / Definition of Done:** the official execution corpus passes, both tiers,
  0 unexplained mismatches, exclusions counted + named.

#### Phase 5 execution-completion — staged ordering (the operative plan)
**The real bar is NOT "0 unexplained mismatches."** That phrase is a discipline checkpoint
(know why each failure fails), not the finish line — leaning on it lets failures hide under
"explained" and unrun tests hide under "excluded." The bar: **every in-scope assertion green
on BOTH tiers; `out_of_scope` → empty; the exclusion ledger itemized + reconciled and
containing ONLY constructs the spec genuinely does not define, each justified per item.**
"Not core 3.0" is a real exclusion only for undefined behavior — NOT for a fixable parser or
runner gap wearing the sticker (annotations and definition/instance were both mislabeled).

**Discipline (binding):**
- Engine bugs → fixed at the SOURCE (`spec/wasm.def` / opgen), with a committed BOTH-TIER
  unit test, red→green. NEVER patched reactively in the runner.
- Runner/marshaling bugs → fixed in the runner.
- **Interp is the reference** (anchored on the external `.wast` verdicts — a target I don't
  control); **JIT is the differential over it.** The interp==JIT agreement is only a valid
  oracle once interp is externally proven, so anchor interp first, then layer JIT.
- **Hints flow forward:** each stage, when it unblocks tests, the NEW failures it exposes are
  the next stage's worklist — record them as they surface (a fix opens tests → those tests
  point at the next bug). JIT correctness accretes the whole way: every escape's committed
  test is both-tier, so the differential is earned incrementally, not deferred.

**Stage 0 — DONE:** runner built (interp anchor); `rem_s(MIN,-1)` SIGFPE fixed at source
(`wasm.def`, committed both-tier test); runner float-literal parsing fixed (`strtof`/exact
NaN classes); exclusion ledger itemized + reconciled. State: 34576 ok / 1150 mismatched /
24349 excluded (≈24251 = v128 marshaling, the rest named).

**Stage A — parser: every assemblable module reaches the VM ("didn't assemble" → 0).**
- A1 **annotations** `(@…)`: a pegc structured-skip element (balanced generic parens +
  string/comment aware — §6.2.5). Kills the "spec-optional" sticker; flips `annotations.wast`
  (7 valid + 64 `assert_malformed`) to real pass/fail. → *opens:* any new failure here is
  validator/VM, now unambiguous.
- A2 **quote**: confirm the execution path's `module_to_bytes` leaves no residual quote
  "didn't assemble".

**Stage B — VM/runner: excluded → ~0 and mismatched → 0, interp tier.**
- B1 **v128 value marshaling** (`parse_wval`): the ~24k v128 assertions ASSEMBLE; the gap is
  the runner building v128 args/expecteds. → *opens:* the SIMD mismatches that appear name
  which v128 ops the engine gets wrong → Stage-B4 escapes.
- B2 **module definition/instance** → runner registry (define-without-instantiate;
  instantiate-a-named-def). Kills the second `out_of_scope` sticker. → *opens:* cross-instance
  needs.
- B3 **cross-instance memory/table imports**: the one-heap-per-module limitation — per-instance
  memidx translation in `jav_instance_bind` (or a per-instance heap map). Unblocks
  `linking`/`imports`.
- B4 **classify every remaining mismatch** → runner bug (fix in runner) OR engine escape (fix
  at source + committed both-tier test, like `rem_s`; the GC-path faults the fork flagged go
  here). Each escape's committed both-tier test pre-validates JIT for that edge.

**Stage C — JIT differential (the confirm net, anchored on green interp).**
- C1 stand up the JIT tier in the runner: parallel per-tier instances (state not double-
  applied); run every action interp AND jit; assert agreement.
- C2 any interp≠JIT divergence is unambiguously a JIT/opgen-stencil bug with a known-good
  interp target — fix at source (`wasm.def`/stencils), committed both-tier test. C is the
  aggregate confirm over instruction COMBINATIONS / state-sequences the isolated unit tests
  don't reconstruct.

### Phase 6 — Library packaging
- Makefile targets for `libjavelina.a` and `libjavelina.so` containing the **engine**
  objects (binary reader, validator, instantiator, interp, JIT driver + stencil table,
  GC, runtime, wasm-c-api shell) — **not** the wat reader, `water`, or the test harnesses.
- `-fvisibility=hidden` + an explicit `WASM_API_EXTERN` export set so the `.so` exposes
  only `wasm_*`. A short **executable-memory portability note** for the JIT's page
  allocation (Linux `mmap` RW→`mprotect` RX or RWX; macOS hardened runtime → `MAP_JIT` +
  entitlement + `pthread_jit_write_protect`; W^X → two-phase flip) — the JIT copies
  stencils into its **own** mapped pages, so static vs dynamic linking is orthogonal.
- **Gate:** `examples/embed.c` links and runs against both the `.a` and the `.so`.

## Phase 6 — Library hardening / de-weaselification (audit ledger, 2026-06-23)

**Root cause (the lesson):** the plan existed — the anti-weasel contract and the spec-checklist
discipline — but it was only *enforced* where a conformance test would fail if I cut a corner.
Everything the corpus can't reach (the embedder-side API, host re-entrancy, the internal seams) got built
to pass, not to spec. 6.0 (coverage-first, red-before-fix) plus a `make weasel` gate make the discipline
mechanical, so it no longer depends on test pressure or on trusting me.

Source: a read-only architecture audit (four parallel agents over seams / opgen / weasels /
c-api shim) cross-checked against the spec and the Titzer in-place-interpreter paper. **The
meta-finding: every item below lives at a boundary the `.wast` corpus never crosses** —
addresses ≥ 2³², host re-entrancy, cross-instance host-table access, 256+ cumulative throws,
host-supplied mistyped args. The corpus is green over all of them and will *stay* green. So
this phase is governed by the existing anti-weasel contract verbatim (§ "Testing discipline"):
**no item is DONE by making a new fixture pass while the general case stays cut** — each is the
FULL spec behaviour, and each names the adversarial seam test that proves it (the corpus can't).
No "test-only" code path and no breadcrumb comment documenting a shortcut may survive this phase;
a comment admitting a cut is the cut *plus* proof we knew better — delete both, implement the
real thing, or surface a blocking question. **Legitimate deferral has exactly one form:** the
correct implementation is genuinely *blocked* on a prerequisite (do the prerequisite, then the real
fix — the deferred work IS the real fix). NOT legitimate: a stopgap/bandaid that gets ripped out in a
later phase, or skipping purely-additive work because it's tedious. If it's just "another layer to undo
later," it's not scoping, it's debt — don't. Tags: **[V]** verified in source 2026-06-23;
**[V-insp]** verified by code-reading, repro test still owed; **[A?]** agent-found, plausible,
**unconfirmed — write the failing test first.**

**Design north star — the internal subsystem APIs ARE the deliverable.** Phase 6 isn't a bug-sweep; it
is locking down the *final* API surface between subsystems (engine ↔ store ↔ instance, the
`runtime_api.h` native seam, and the memory-model seam for stack/locals/memory access) so the MVP can be
built on without churn. Why this matters: (1) the "interp + JIT both call the same external runtime fn"
seam was the concept proof — making it **solid and final** lets JIT templates be reworked/optimised
freely as long as they patch into the *defined* API calls; (2) opgen's DSL then owns the interp+JIT
lowering of each opcode against those APIs (6.F), where real JIT optimisation eventually lives. So every
6.x internal-API item is designing the **final** seam, and each seam must encode exactly the spec
obligations for that subsystem (the store owns instance lifetime + GC roots; the instance exposes its
own roots; the engine owns only machine state; a native does exactly what its §-rule says). "Make it
final, match the spec" — not "make it work for now."

Four root-cause abstractions generate most items; fix the abstraction, not the symptom.

### 6.0 — Coverage foundation (each area's FIRST step; the corpus is NOT coverage)
The method for every 6.x item: **assess coverage → write the failing test(s) to spec → see red → fix →
green.** "Does this area have the goods?" is answered with a test list, not a vibe; a fix without a
test that was red first does not count. Measured baseline 2026-06-23:
- **64 / 135 public `wasm.h` functions (47%) have *any* unit test;** the other ~53% are reached, if at
  all, only indirectly through the conformance runner.
- **The conformance corpus is not API coverage.** `.wast` expresses only *guest-side* ops (instantiate /
  call / get / table.*); the **embedder-side reference API is not expressible in `.wast`**, so the runner
  never calls it — it can only be covered by a c-api unit test.
- **Entirely untested today (the ref/host surface — where A1/B7/the stubs hide):** `wasm_func_as_ref`/
  `wasm_ref_as_func`, `wasm_{global,table,memory,extern}_as_ref`/`ref_as`, `wasm_ref_copy`,
  `wasm_ref_null`, `wasm_ref_{get,set}_host_info`, `wasm_match_externtype`/`funcref`, `wasm_global_type`.
  The `as_ref` family is partly stubbed-to-NULL → **untested stubs** (implement, don't test-into-green).
- **Host-call surface is happy-path only:** no test re-enters the engine (A3), passes wrong arity/kind
  (B2), crosses the funcref boundary host↔guest (A1), exhausts the exn store (B4), or `func_as_ref`s a
  host fn (B7). And `test_host.c` is built *on* the A7 weasel ABI (`jav_invoke_host` + single-`T_INT`).
- [~] **Deliverable — a c-api adversarial/coverage test** (expanding `test_capi.c`); each new test red
  before its fix. **Landed 2026-06-23:** B2 + B2-host (arg typing), B7 (host funcref round-trip), and a
  ref-API coverage block — `match_externtype` (variance), `ref_copy` + `ref_same` identity, instance
  funcref `as_ref`/`ref_as` round-trip+call, and the `as_ref`/`ref_as` stubs (NULL by spec) — all
  ASAN/LSan-clean. **Added 2026-06-24:** the §7.2 type-reflection cluster (`wasm_func_type` /
  `_param_arity` / `_result_arity` / `functype_params` / `functype_results`; `wasm_global_type` /
  `globaltype_content` / `globaltype_mutability`; `wasm_valtype_kind`; plus the GENERIC
  `wasm_extern_type` → `wasm_externtype_kind` + `wasm_externtype_as_functype`/`as_globaltype` projection,
  with the wrong projection asserting NULL) — a `(i32 i64)->(f32)` func + a `(mut f64)` global reflected
  and asserted (gate 78/0). Plus **per-handle host-info** (`wasm_ref_set/
  get_host_info` round-trip) and **`wasm_func_new_with_env`** (env closure read in the callback + the env
  finalizer running on `func_delete`). And the **A1 cross-boundary funcref**: the host recovers a
  RETURNED guest funcref (`wasm_ref_as_func`) and calls `$id` back into the guest (host→guest,
  non-re-entrant — `getref` already returned) → 7. **B4 is now DONE** (the exn-store GC-object design
  pass — see B4 above). All named "still owed" items here are now CLOSED; only the broad 135/135 floor
  (next bullet) remains. (`wasm_ref_null` is not a c-api function — the null ref IS the NULL handle.
  The RE-ENTRANT cross-boundary call — host re-enters the engine and calls into a guest WHILE inside a
  guest→host call — is now COVERED: the §8 A3 implementation added exactly that test (`host_reenter` →
  B.inner; A reads its own global → 111, not B's 222). The recovered-funcref variant (`wasm_ref_as_func`
  instead of a pre-held export) resolves to the SAME `wasm_func_call` seam, so it exercises no new path.)
- [x] **Raise the coverage floor — MEASURED + CLOSED for jav code 2026-06-24 (gate 78/0).** Measured the
  public surface: **151 `wasm_*` symbols in `wasm.h`, 103 now directly exercised by `test/`/`examples/`**
  (was 88). The remaining 48 break down as: **46 are upstream W3C-header `static inline`/`WASM_DECLARE_VEC`
  conveniences** (`wasm_functype_new_N_M`, the `_const` projections, `as_extern` casts, `valtype_new_f32`,
  `name_new_from_string`, …) — not jav code, provided + compiled from the vendored `wasm.h`, exercised
  transitively; **2 are jav functions left deliberately untested because they are documented limitations,
  NOT stubs to bless:** `wasm_frame_func_offset` / `wasm_frame_module_offset` (below). Closed the real gap
  — the 14 jav-implemented public fns that had ZERO direct test — with red→green cases in `test_capi.c`
  (`config_new`/`engine_new_with_config`, `import`/`exporttype_new` + accessors, `tabletype_element`,
  `tag_new`/`tag_type`/`tagtype_functype`/`tag_copy`, `trap_message`, `val_copy`, `valkind_is_ref`,
  `frame_copy`). So: every jav-implemented public fn has a direct test except the 2 documented-limitation
  frame offsets. (The literal 151/151 is not met and shouldn't be claimed — the 46 upstream inlines are
  upstream's to test, recorded here transparently rather than silently counted as covered.)
- [x] **REQUIRED (spec/wasm-c-api contract) — trap-frame bytecode offsets. DONE 2026-06-24 (interp tier =
  all reachable execution; red→green, gate 78/0, ASAN-clean).** `wasm_frame_func_offset`/`_module_offset`
  now return the real byte offset (no hardcoded 0). Implemented: (1) `frame_t.instr_pc` stamped by
  `jav_next` (`= code.pos`) before each opcode decode = the instruction start; (2) `jav_call_fn` captures
  it per frame into the parallel `vm->trap_pcs` (innermost = the trapping instruction, outer = its inward
  `call`) before `*caller = saved`; (3) the c-api copies `trap_pcs` into `wasm_trap_t.frame_pcs` →
  `wasm_frame_t.pc`; `func_offset = pc` (body-relative), `module_offset = (funcs[idx].code - module->bytes)
  + pc`. Red-first `test_capi.c`: `i32.const 7; drop; unreachable` traps → `func_offset == 3` (the
  unreachable's exact offset) + `module_offset > func_offset`. **JIT tier ALSO DONE 2026-06-24 (no
  carve-out, no deferral):** opgen now emits `f->instr_pc = (u4)_HOLE_pc;` at the top of every stencil
  (`vmemit.cpp emit_one_opcode`, STENCIL mode), and the jitterator fills `_HOLE_pc` with the stencil's
  source byte offset (`bpos`, `jit_driver.c`) — so a JIT'd frame stamps the same `instr_pc` the interp's
  `jav_next` does, and `jav_call_fn` captures it identically. Red→green bare-VM `test_jit_native`: a JIT'd
  `i32.const 7; drop; unreachable` traps → `vm.frame.instr_pc == 3` (without the stencil stamp it's
  memset-0). opgen rebuilt + javelina regen'd; gate 78/0, conf 60113/0/0 (the per-stencil stamp survives
  the copy-and-patch pipeline both tiers). So both tiers report the exact byte offset; nothing deferred.

### 6.A — One typed authority per value (root cause: implicit representation contracts)
- [x] **A1 — unify the funcref runtime encoding. DONE 2026-06-23.** Was: `T_REF` meant a funcidx on
  the host path (`wasm_capi.c:slot_of_val`/`ref_to_entry`/`wasm_table_get`) but a funcinst *pointer* on
  the engine path (`jav_instance.c` active-elem `&out->funcs[i]`; `jav_runtime.c` `call_indirect` reads
  `(jav_func_t*)raw`). the A1 funcref-unification had migrated only the ENGINE side, so a host reading a
  wasm-populated table truncated a pointer to i32, and a host-set funcref that wasm `call_indirect`'d
  deref'd a small int (wild deref / type confusion). **Fix: a funcref is the §4.2.1 funcinst pointer
  EVERYWHERE.** As-built (differs from the hypothesized design — simpler, no back-map):
  - **`sig` is now a universal funcinst field**, not host-only: set on every funcinst (`jav_instance.c`
    defined funcs `f->sig=&mod->func_sigs[fi]`; imports inherit it via the export projection; host funcs
    in `hostfn_new`). `call_indirect`/`return_call_indirect` read `fn->sig` unconditionally and gate the
    lattice-vs-structural type check on `inst_ctx==NULL` (host ⇒ structural `jav_functype_eq`; instance
    sharing the lattice ⇒ `jav_ht_sub`). The old `fn->sig ? … : types[type_index]` dual path is gone —
    one source of truth.
  - **Host funcs back themselves with a real funcinst** (`capi_hostfn_t.funcinst`, invoke=
    `capi_host_invoke`, `inst_ctx=NULL`, `sig=&hf->jtype`), built once in `hostfn_new`; the import path
    reuses it. So a host funcref IS a funcinst pointer too (resolves B7's table-funcref deferral).
  - **The c-api carries the funcinst pointer**, not a funcidx: `wasm_ref_t.fn`/`fnstore`, `wasm_func_t`
    trailing `fn`/`fnstore` (read ONLY on the recovered-funcref path, after `host`/`inst` are ruled
    out — an `wasm_extern_as_func` wrapper is extern-sized, so the field-order guard is load-bearing,
    ASAN-verified). `slot_of_val`/`val_of_slot`/`ref_to_entry`/`wasm_table_get` encode/decode the
    pointer; `wasm_ref_same`/`as_ref`/`as_func`/`func_sig`/`wasm_func_type` all route on it.
  - **No `{inst,index}` recovery needed**: the engine grew `jav_invoke_fn(vm,h,fn,escaped)` (the §4.2.1
    funcaddr-model invoke; `jav_invoke(funcidx)` delegates via a shared `jav_classify_outcome`).
    `wasm_func_call` resolves `{vm,fn}` for both the export view and a table/global-recovered funcref and
    calls through that one seam — the funcinst is self-describing (`jav_call_fn` switches the vm to
    `fn->inst_ctx`), so any vm in its store drives it.
  - Lifetime: funcrefs are `T_REF` raw pointers, NOT GC-scanned; a table-held host funcref dangles if its
    `wasm_func_t` is deleted (the standard embedder borrow), same as instance funcrefs vs their instance.
  **Red-first tests** (`test_capi` A1 block, the corpus can't reach host↔guest via a host-set slot):
  (i) host writes a guest-export funcref into the guest table → guest `call_indirect` → 42; (ii) a
  `wasm_func_new` host func in the guest table → guest `call_indirect` → host callback runs. The
  pre-existing elem→`table_get`→`table_set` identity round-trip (line ~291) and the recovered
  instance-funcref call (6.0 block) now exercise the pointer encoding. **ASAN+UBSan clean** (the field-
  order guard was added after ASAN flagged the extern-sized `f->fn` read). Bare-VM tests that hand-build
  funcinsts (`test_call_indirect`/`test_reftypes`/`test_table_ops`/`test_tailcall`) now set `.sig` to
  honor the invariant — segfault (null `sig` deref) → green, the invariant doing its job. Gate 76/76,
  conformance 60113/0/0.
- [x] **A-width — collapse the element-width notions; the RTT is the one runtime authority. DONE 2026-06-23.**
  **Finding:** the plan's "three sources of truth for one width" premise was imprecise — `elem_size`,
  `elem_store_w`, and the `type_field_packs` pack code answer THREE DIFFERENT questions, not three copies
  of one width: (a) `elem_size` = the in-heap slot stride, INVARIANTLY 8 (array elements ride the 8-byte
  `.l` value view; 16-byte v128 arrays aren't carried); (b) `elem_store_w` = the data-segment byte stride
  for array.new_data/init_data (1/2/4/8); (c) the pack code = §4 storage-type packing (i8/i16) for
  get_s/u sign-extension + validation. Merging two semantically-distinct numbers would be WRONG. The real
  consolidation done: **(1)** `elem_size` (always 8) is no longer a per-RTT field — replaced by a
  `GC_ARRAY_ELEM_BYTES` constant (jav_gc.h), so the RTT carries only the genuinely-varying `elem_store_w`;
  the dead `(at->elem==WVT_V128)?16` branch in `build_rtts` is gone (8-byte slots ⇒ no v128 arrays).
  **(2)** the **dead `?:4` default removed** (B-elemw): `array.new_data`/`init_data` read `rtt->elem_store_w`
  directly — it's always set for an array RTT. (It was dead in PRODUCTION but LOAD-BEARING in the bare-VM
  tests, which hand-built array RTTs without `elem_store_w` — those now set it per the A-rtt rule.)
  **(3)** `array.get_s/u` reads packed-ness from `rtt->elem_store_w` (the one authority), not a redundant
  `type_field_packs` lookup; structs keep `pack_width(type_field_packs)` (per-field, the RTT has no
  per-field widths — the genuine asymmetry). **(4)** `array.init_data` gained a `typ` bounds-trap
  (fail-closed, matching `new_data`) so `artt` is never NULL. Touched the `gc_rtt_t` ABI (struct shrank
  by the `uint16_t elem_size`) → updated all 9 bare-VM RTT mirrors incl. the 3 ABI-exact typedefs
  (A-rtt discipline). Gate 76/76, conformance 60113/0/0; ASAN+UBSan-clean GC builds (test_array_gc
  ref-scan through the mirror, test_gc_packed get_s/u, test_gc_segments new_data — scanner instrumented,
  confirming the new layout).
- [ ] **A-rtt — `gc_rtt_t` ABI mirror footgun (DONE 2026-06-23, record).** Bare-VM GC tests
  hand-mirrored `gc_rtt_t`; the `elem_store_w` field addition silently misaligned them. Converted
  all to the real `gc_rtt_t` / an ABI-exact mirror. **Rule going forward:** a test that mirrors an
  engine struct is forbidden — use the real type.
- [x] **A-limits — stop overloading memory/table `max` with an "absent" sentinel. DONE 2026-06-23.**
  The runtime structs `jav_mem_t`/`jav_tableinst_t` overloaded `0xFFFFFFFF` (and `65536` for memory32)
  as "no max" while everything upstream (modidx, `jav_extern_t`, the §7 validator) already carried an
  explicit `has_max`. Fix: added `has_max` to both runtime structs, threaded it end-to-end — `jav_mem_add`
  gained a `has_max` param; instantiation (`jav_instance.c`) and the host-create paths (`wasm_capi.c`)
  set it from the decoded flag, NO sentinel; the store/host export projections (`jav_store.c`,
  `wasm_capi.c marshal_extern`) read `has_max` directly instead of `max != 0xFFFFFFFF`. `mem_grow`/
  `jav_table_grow_op`/`wasm_table_grow` now cap at `has_max ? max : addrtype_ceiling`, where the §3.2.15/
  §3.2.16 ceiling is computed explicitly (`2^16`/`2^48` pages; `2^32-1`/`2^64-1` entries). **This also
  fixed a latent fail-closed bug: a no-max memory64/table64 was capped at the 32-bit ceiling
  (`0xFFFFFFFF`) instead of `2^48`/`2^64-1`** (unobservable in practice — nobody grows past 2^32 — but a
  spec divergence). The re-export distinction is now real: a no-max limit projects `wasm_limits_max_default`,
  never a ceiling value. Behavior-preserving (conformance `memory_grow`/`table` cover grow-with-max);
  `jav_mem_add`'s ~8 bare-VM test callers + `test_table_ops`'s own `0xFFFFFFFF` test-harness sentinel
  updated to the explicit form. **Lock-in test** (`test_capi` A-limits block): grow-past-max fail-closed,
  `max==0` forbids all growth (the original footgun), unbounded grow, and the no-max→`wasm_limits_max_default`
  projection round-trip. ASAN+UBSan clean, gate 76/76, conformance 60113/0/0.

### 6.B — Separate the activation from the engine (root cause: `vm_t` god-singleton)
Cross-ref `the §8 flat-cache collapse` (the instctx→single-pointer collapse) and `§5` (lifetime).
**The root cause is one thing — the flat mutable instance cache on `vm_t` — and A3 and A4 are
its symptoms, not three independent bugs.** File them as the one change below.

- [x] **R1 (ROOT) — delete the flat instance cache; read the context through `frame.ctx`. DONE
  2026-06-24 (gate 78/0, conf 60113/0/0; test_capi ASAN-clean, test_exceptions valgrind-clean both
  tiers).** The ~23 flat cluster fields on `vm_t` are GONE — replaced by an embedded `instctx_t cluster`
  (the bare-VM/default context); the active context is `vm->frame.ctx` (set by jav_vm_init to
  `&vm->cluster`, by the loader to `&inst->ctx`). EVERY interp cluster read goes through it: the
  generated `GLOBAL_GET/SET/TAG_SET` + `slot_index_tag_array` were overridden in the backend header to
  `vm->frame.ctx->globals…` (the 6.F macro-routing made this a backend `#define`, no opgen body change),
  and the ~89 hand-written `vm->X` reads in jav_runtime.c/jav_instance.c were flipped to `vm->frame.ctx->X`.
  `jav_vm_load_ctx` + its call sites DELETED; jav_call_fn just sets `callee->ctx` and the frame
  save/restore (`*caller = saved`) carries it back; jav_instance_bind just sets `frame.ctx`. **A3 fixed**
  (red-first `test_capi.c`: A.outer() → host reenter() → B.inner(); A then reads its own global → 111,
  not B's 222) — `wasm_func_call` now saves/restores the outer activation (its frame, hence its ctx) so a
  re-entrant call can't corrupt the suspended guest. **A4 gone** (one source of truth — the instctx).
  §4.5.4 init ORDER preserved (jav_fill_ctx unmoved; minimal facets into out->ctx before global inits;
  conformance proves it). ASAN caught a dangling-`frame.ctx` UAF in `wasm_match_valtype` (read a freed
  instance's ctx) — fixed (NULL lattice for the abstract-only c-api match + reset `frame.ctx` to
  `&vm.cluster` on instance delete). The ~35 bare-VM tests now set `vm.cluster.X`. **A3/A4/R1 = the one
  the §8 flat-cache collapse collapse, now done.**
  Original analysis retained below.
- [ ] ~~R1 (ROOT) — delete the flat instance cache; read the context through `frame.ctx`.~~
  Today `jav_vm_load_ctx` copies ~20 instance pointers (`globals`, `tables`, `types`, `rtts`, `tags`,
  segs, `mem_addrs`, …) from `instctx_t` into a flat cache on `vm_t` at every instance-boundary
  crossing, so the generated interp can read `vm->globals[i]` directly. **That "optimization" saves a
  single dependent L1 load** (`frame.ctx`, which the compiler hoists into a register after the first
  access in a frame) **on cold-ish ops, was never benchmarked, and the JIT — the tier that cares about
  speed — reads none of it (it bakes addresses at compile time).** Its real cost is the bug surface:
  - **A3 (re-entrancy) [V-insp]** — the cache is mutable global state that `load_ctx` refreshes on a
    crossing and the outer `jav_call_internal` never *restores* (its `loaded`/`switched` tracking,
    `jav_runtime.c:207,211`, is blind to a nested `wasm_func_call`'s `jav_instance_bind`). A host
    callback that re-enters resumes the suspended guest on the WRONG instance's memory/globals.
  - **A4 (dual source of truth) [A]** — the same ~20 facts live in `instctx_t` (`jav_frame.h:174-190`)
    *and* the vm cache (`:205-240`), hand-copied; every new per-instance field must be edited in lockstep.

  Read through `frame.ctx` (each frame carries its own context pointer, set at frame build) and **both
  symptoms become structurally impossible** — nothing to desync, nothing to restore, one source of
  truth. `jav_vm_load_ctx` and the flat fields delete themselves (exactly the §8 flat-cache collapse's end
  state). **Nuance:** if anything still deserves caching it's the **memory #0 base** alone (Titzer pins
  it in a register because loads/stores are hot) — *one* hot pointer, not a blanket 20-field copy that
  lumps it with 19 cold ones. **Dependency:** the flip is gated on 6.F — opgen hardcodes
  `vm->globals[idx]` in its lowering, so the generated interp must be changed to emit
  `vm->frame.ctx->globals[idx]`; sequence R1 with the opgen memory-model work. A3's only correct fix is
  R1 — **no stopgap** (a save/restore-around-host-calls patch would just be hackery to rip out later);
  A3 waits for the real fix. **Test:** host import in module A trampolines a call into module B, then A
  reads its own memory/global → must see A's value.

- [x] **A2 — reset/scope the exception store. DONE 2026-06-24 (exn-cluster GC-object design).** The
  monotonic `vm->num_exns`/`vm->exns[256]` is GONE — an exception is now a managed GC object and an
  exnref is the object pointer (T_GCREF), so there is no per-store store to leak or reset; dead
  exceptions are reclaimed by liveness. **Test:** `test_capi.c` calls a throwing func 300× on one store;
  every throw (incl. #257+) surfaces as an exception, not a 256-cap trap.
- [x] **A-caps — `exns[256]`/`handlers[256]`/`exn.fields[16]` → `bbq_vec`. DONE 2026-06-24.** Already
  named in the anti-weasel contract (§ "Testing discipline" item 4) as "bugs to convert, NOT a pattern
  to copy." `exns[256]` + `exn.fields[16]` gone via the exn-cluster GC-object design. **`handlers[256]`
  now a bbq_vec** (`vm->handlers`, length = count): the silent `if (… < JAV_MAX_HANDLERS)` non-install
  (a throw in the 257th+ nested try_table escaped its handler) is removed — push/pop/last via the crt
  vec, freed in `jav_vm_free`. Red-first test (`test_exceptions.c`): 300 nested try_tables where ONLY
  the innermost catches the thrown tag → 42 both tiers (pre-fix: UNCAUGHT). Gate 77/0, conf 60113/0/0,
  valgrind-clean both tiers. [[feedback_use_crt_not_manual_arrays]].

### 6.C — A narrow internal engine API (root cause: the shim open-codes internals)
- [x] **A5 — `jav_invoke` core entry. DONE 2026-06-23 (with a design conclusion on scope).** The shim
  open-coded the §7.1.8 call/outcome ABI — `jav_call` then reading `vm->unwinding`/`pending_exn`/
  `trap_trace` to decide return-vs-trap-vs-exception. Extracted that into `jav_invoke(vm, h, funcidx,
  &escaped) → {RETURN,TRAP,EXN}` (`jav_runtime.c`, declared `interp.h`); the shim now just turns the
  outcome into a `wasm_trap_t`. Behaviour-preserving refactor, gate 76/76, conformance 60113/0/0.
  **Design conclusion (the original `args/results` signature was reconsidered, not silently dropped):**
  arg/result *marshaling* stays in the shim on purpose — moving it into the engine would force a temp
  slot buffer + double GC-rooting (slower for marginal gain) and the B2 ref-hierarchy guard inherently
  needs the c-api `wasm_valkind` that an engine slot tag doesn't carry. So the right division is **engine
  owns call+outcome, shim owns value marshaling**; the shim still touches `vm->frame` only for that
  marshaling (efficient + naturally rooted), not for the ABI. (If Dan wants the full marshaling move
  anyway, it's a deliberate call — say so.) The GC-root-layout decoupling (a root-visit callback) is the
  separate **A6** item below, not A5.
- [x] **A7 — one host ABI; deleted the single-`T_INT` `jav_invoke_host`. DONE 2026-06-24.** The weasel was
  a wrapper that ran a `jav_host_fn` (returns one untyped `slot_t`) and hardcoded `vm->result_type = T_INT`
  (wrong for i64/f64/multi-result). Deleted `jav_invoke_host` + the `jav_host_fn` typedef; the host ABI is
  now just the ONE `invoke` seam — a host import is a `jav_status_t(vm,h,ctx)` thunk that reads params from
  `frame.locals` and writes the TYPED result (`vm->result`+`vm->result_type`, or the frame stack for
  multi-value). The 4 loader tests (`test_host`/`test_tiers`/`test_tailcall`/`test_instantiate`) now set
  their host fns as invoke thunks directly (and set `result_type` explicitly — `host_id` even forwards
  `local_types[0]`). The c-api's `capi_host_invoke` was already such a thunk. Gate 77/77, conformance 60113/0/0.
- [x] **A6 — the final GC-roots API (store owns the scan). DONE 2026-06-24 (step-24 rooting landed in the
  LIVE store; gate 78/0, red-verified).** The §4.7.2 step-24 (allocmodule) rooting is now in `wasm_store_t`:
  `vm->on_inst_alloc` hook → `capi_track_inst` pushes the instance to `store->insts` at allocation (after
  `fill_ctx`, BEFORE element/data/start), so it is store-rooted during segment/start GC independent of the
  bound-scan; the post-return push is gone. Red-verified: a managed struct held only by instance A's global
  survives a collection churned through instance B (no-op'ing the hook → 77/1). NOTE: this landed in the
  c-api store because **the dead `jav_store_t` was deleted** (it had zero callers; the c-api `wasm_store_t`
  is the one live store — see the §4.2.3 store bullet up top). The "THEN under R1 the engine drops instance
  scanning" tail below is a separate future dedup (the bound-scan still also covers the bound instance,
  idempotently); not required for correctness. Historical design below.
- [x] ~~**A6 (original dedup, DONE 2026-06-23).**~~ Root scan was copy-pasted
  three times (`jav_runtime.c:693` engine, `jav_store.c:69` store, `wasm_capi.c:717` c-api), and the bound
  instance is scanned twice. The **final seam** (matches the spec's "what's live" + the store-owns-lifetime
  design): `jav_vm_scan_machine_roots(vm,…)` (engine: value stack + locals ONLY) · `jav_instance_visit_
  roots(inst,…)` (instance: its globals+tables) · the **store owns the heap's `enum_roots`** and composes
  machine + every instance + the embedder's host-roots callback (host globals/tables/`gc_refs`/`exn_roots`
  stay behind the callback — they're c-api types the store can't name). Step now (not blocked): land
  `jav_instance_visit_roots` + route `store_roots`/`capi_extra_roots` through it (delete `scan_inst_roots`
  + `capi_scan_inst_roots`), and split host-roots from instance-roots. **DONE 2026-06-23** (one helper in
  `jav_instance.c`; store + c-api route through it). **The in-progress instance is SPEC-MANDATED rooting,
  not a "safety net":** §4.5.7 allocates the instance into the store (step 23) BEFORE element/data init
  (27/28), so during segment-init GC its globals/tables are *live store state* the spec requires preserved.
  Our impl diverges — it adds to `s->insts` only post-`jav_instantiate` (`jav_store_instantiate:129`), and
  the engine's bound-scan masks that. **Spec-aligned final design:** the store tracks the instance from
  allocation (before segments), rooting it as a live entity; THEN under R1 the engine drops instance
  scanning (it reads the bound instance only through the vm cache today). The rooting *moves to the store*,
  it never disappears. ASAN-gated (moving-collector root correctness).
- [x] **A8 — the verdict-readout extension surface. DONE 2026-06-24 (the required part).** The
  classification is faithful — `jav_capi_last_status`/`_err` distinguish malformed/invalid/unlinkable/
  uninstantiable/trap/exception. The two required pieces are now done: (1) **declared in a documented
  sidecar header** — `jav_capi_last_status`/`_error` live in `jav_extern.h` (NOT the vendored `wasm.h`),
  alongside the other sanctioned extensions (`jav_project_export`, `jav_capi_set_probe`); (2) **the
  single-threaded-store contract is stated** there — a last-write-wins per-store slot, read immediately
  after the operation, valid only under the one-vm-per-store model. The *optional* richer
  structured-diagnostic return remains an explicit design call for Dan (not a silent defer — there's a
  working readout; a richer shape is an addition, scheduled if/when wanted).
- [x] **A9 — dispatch table off the file-scope global. DONE 2026-06-23.** Was a process-global
  `g_table` rewritten every `interp_run` and read in `jav_next` (thread-unsafe). Threaded it on the vm as
  `vm->dispatch` (Titzer's DISPATCH register; `void*` so `jav_frame.h` stays uncoupled from the handler
  type) — per-vm, set in `interp_run` before any `jav_next`. Verified the JIT references neither
  `jav_next` nor the table (it bakes `_HOLE_cont`), so `dispatch` is interp-only and always set first;
  `frame` stays first in `vm_t` (JIT ABI intact). Gate 76/76, conformance 60113/0/0.

### 6.D — Fail-open / silent shortcuts (spec divergences)
- [x] **B1 — memory64 / table64 end-to-end 64-bit. DONE 2026-06-24** (opgen `addr` type, both phases).
  Loads/stores (scalar + SIMD), memory.fill/copy/init, table.copy/init pop the address/index addrtype-
  aware via the generated `GPOP_ADDR`; memory.size/grow + table.size are stack-driven natives pushing
  the addrtype width via `is64`; table.grow/fill/get/set + **call_indirect/return_call_indirect** pop
  the index via `pop_addr` (the call_indirect index truncation was a genuine FAIL-OPEN: a table64 index
  ≥ 2³² could call the wrong function instead of trapping); the natives widened to `u8` with overflow-
  safe (subtract-not-add) bounds; `jav_instance.c` active-elem offset is `table_is64`-aware. **Red-first
  both-tier test** `test_memory64.c`: a mem64 store/fill and a table64 table.set at i64 address/index
  2³² (low 32 in-bounds, full value OOB) TRAP (would silently hit the wrong location under the old
  truncation); memory.size is i64; interp == JIT. Gate 77/0, conformance 60113/0/0; interp + the shared
  natives ASAN+UBSan clean; JIT valgrind-clean (0 errors). **Two findings surfaced (both pre-existing,
  orthogonal — filed below as JIT-TRAP-BAIL and ASAN-JIT):** (1) the JIT doesn't bail a function when a
  stack-driven VOID native traps internally — it continues past `_HOLE_cont` to the next stencil; safe
  only when the continuation is harmless (a trap that already pushed a dummy, or consumes nothing). A
  trapping `table.get` followed by a consuming op (`ref.is_null`) underflows the stack → crash. (2) ASAN
  SEGVs entering the copy-and-patch JIT'd code for the mem64/i64 path (valgrind confirms the code is
  clean — an ASAN/JIT-instrumentation interaction, not a miscompute). HISTORICAL (the original bug): handlers popped addr/count via `GPOP_INT`
  (32-bit) regardless of index type, runtime sigs are `s4`, JIT stencils mirror it, active-elem offset
  forced `(uint32_t)o.i` ignoring `is64` (`jav_instance.c:367`, while the data path at `:391` does it
  right). An address ≥ 2³² writes the wrong location instead of trapping.
  **Design (worked out 2026-06-23; NO opgen-tool change needed):** the value model stores i32 in `slot.i`
  with `slot.l`'s high bits GARBAGE (verified), so a uniform 64-bit pop is unsafe — the pop width MUST
  match the addrtype. The codebase already solved this for `table.grow`/`table.fill`/`memory.size` by
  making them STACK-DRIVEN `( -- )` (the native pops, checking `is64` internally). Extend that pattern:
  add a runtime helper `mem_pop_addr(memidx) -> i64` (and `tbl_pop_idx(tbl)`) that pops one addrtype-width
  operand (`m->is64 ? f->stack[--sp].l : (u4)f->stack[--sp].i`), declared `native i64 mem_pop_addr(int
  memidx)` so opgen prepends `vm,heap`. Then in `wasm.def`: drop the address operand from each op's
  signature and pop it via the helper in the `(. .)` body (for stores keep `val` in the signature so
  opgen pops it first/on-top, then the body pops the addr beneath). The **exact per-operand widths are
  authoritative in the validator's `tc_mem` (validate.c:319-408)** — implement to match:
  - load `(at -- dt)`, store `(dt at -- )`, SIMD load/store + load/store_lane: ADDR is addrtype-width
    (covers ~36 scalar + ~30 `0xfd` ops).
  - `memory.size` (push at), `memory.grow` (pop at, push at) — **delta is addrtype-width too** (.def
    currently mistypes it `i32`); widen `mem_grow` to `s8 delta`.
  - `memory.fill`: pop `at`(n), `i32`(v), `at`(d) — d,n addrtype; v always i32.
  - `memory.copy`: pop `min(at_dst,at_src)`(n), `at_src`(s), `at_dst`(d).
  - `memory.init`: pop `i32`(n), `i32`(s), `at`(d) — d addrtype; s,n always i32 (segment is byte-indexed).
  - table.copy/init analogous via table addrtype; table.grow/fill/size already stack-driven.
  Widen the natives (`jav_memory_fill/copy/init`, `jav_table_copy/init`, `mem_grow`) to take `s8`/`u8`
  and pop addrtype-aware (or make them fully stack-driven). Fix `jav_instance.c:367` (elem offset →
  `table_is64 ? o.l : (u4)o.i`, matching the data path). Regenerate (gen_interp + stencils) + gate BOTH
  tiers. **Scope: ~80 ops, ATOMIC (one regen) — a focused pass, spec-sensitive per-operand widths.**
  **Test:** `memory.fill/copy/init` + `grow`/`size` and a table64 index whose low 32 bits are in-bounds
  but the full value is OOB → must trap (both tiers).
  **APPROACH CHOSEN (Dan, 2026-06-24): teach opgen an `addr` operand type — the proper DSL seam, not a
  stack-driven workaround.** Execution-ready (opgen internals mapped; asdl+pegc+opgen toolchain confirmed
  built at `/home/dan/Source/BBQ/build/{asdl,pegc,opgen}`). `addr` is POP-ONLY (tag-driven); the few
  addr-RESULT ops (memory/table `size`/`grow`) become stack-driven natives that push via `is64` (the
  `table.grow_op` pattern) so opgen needs no push-side `addr`. **Phase 1 — opgen tool extension: DONE
  2026-06-24.** Landed `TyAddr` (opgen.asdl + opgen.peg regen), `slot_class`/`jtype`/`wvt_name`, the
  tag-driven `GPOP_ADDR` macro (emitted into BOTH gen_interp.c + jav_stencils.c), AND — caught via LSP
  findReferences on `TyWord`/`TyAny`, NOT the compiler — `spec.cpp` `stack_slots`(→1)/`type_bytes`(→8),
  where `addr` had fallen through to `return 0` (a silent stack-effect-corruption bug; no `default` so
  no compile error). opgen rebuilds; javelina regenerates + gates 76/0 (behavior-neutral: `addr` added
  but unused). Steps that were: **Phase 1 — opgen tool extension:**
  (1) `BBQ/opgen/grammar/opgen.asdl`: add `TyAddr` to the `ValueType` enum. (2) `grammar/opgen.peg`: add
  an `addr` alternative to the value-type rule (mirror `word`/`dword`). (3) `src/semlower.cpp`:
  `slot_class(TyAddr)→"ADDR"`; a `jtype` fallback for `TyAddr` (`scalar s8, uscalar u8, slots 1`).
  (4) `src/vmemit.cpp`: in `emit_slot_macros`, emit an explicit tag-driven macro alongside WORD/ANY —
  `#define GPOP_ADDR(name) u8 name; do { if (f->stack_types[f->sp-1]==T_LONG) name=(u8)f->stack[--f->sp].l;
  else name=(u8)(u4)f->stack[--f->sp].i; } while(0)` — (zero-extends i32, full-width i64; NO GPUSH_ADDR);
  `wvt_name(TyAddr)→WVT_I32` (unused — memory-op validation is hand-written `tc_mem`). Rebuild opgen
  (`cmake --build` regenerates frontend) → regen javelina (`make`) → gate (should stay 76/0: `addr` is
  added but unused, behavior-identical). **Phase 2 — use it:** `wasm.def` loads `(addr a -- dt)`, stores
  `(addr a, dt val -- )` (opgen pops val on top first, then addr), `fill (addr d, i32 v, addr n -- )`,
  `copy (addr d, addr s, addr n -- )` (tag-driven pop matches the validator's `min` typing for `n`),
  `init (addr d, i32 s, i32 n -- )`, SIMD load/store/lane addr→`addr`; bodies drop `(unsigned)` and use
  `addr` (u8) directly. `memory.size`/`grow` + `table.size` → stack-driven natives pushing addr-width
  (mirror `table.grow_op`). Widen `jav_memory_fill/copy/init`, `jav_table_copy/init` to `u8`/`s8`. Fix
  `jav_instance.c:367`. Regen + gate both tiers + the OOB-trap test.
- [x] **JIT-TRAP-BAIL — the JIT now bails when a native traps. DONE 2026-06-24 (found via B1 test).**
  Was: a stencil for an op calling a native (`table.get/set/fill/copy/init`, `memory.fill/copy/init`, the
  GC ops, even loads/stores) called the native then unconditionally `TAIL _HOLE_cont` — no `vm->trapped`
  check. The interp bails (the trapping native sets `frame.code.pos = code.length`; the next `jav_next`
  sees end); the JIT ignores `code.pos`, so it ran ON into the next stencil. "Worked" only when the
  continuation was harmless. Two real failure modes: (a) a trapping `table.get` (pops the index, traps
  WITHOUT pushing) + a consumer (`ref.is_null`) underflows the value stack → CRASH; (b) a **store after a
  trapped load executes** → memory corruption (a trapped function must stop). Corpus-invisible (the .wast
  runner runs `assert_trap` on the interpreter only). **Fix (opgen):** `SemLowerer` now records whether a
  body lowered a native call (`native_called_`, set in `lower_call`/`lower_void_call`); `emit_one_opcode`
  emits `if (vm->trapped) TAIL return _HOLE_trap(vm);` before `_HOLE_cont` in STENCIL mode for those ops
  (not pure-compute ops like i32.add, so the hot path is untouched; status/control natives keep their
  existing `_HOLE_resync` bail). **Test:** `test_memory64` `table.get @2³²` (OOB) `+ ref.is_null` now traps
  cleanly on both tiers (was a crash). Gate 77/0, conformance 60113/0/0.
- [ ] **ASAN-JIT — ASAN SEGVs entering copy-and-patch JIT'd code on the mem64/i64 path. [note, benign]**
  `test_memory64` JIT run SEGVs under `-fsanitize=address` at `jit_enter` (the call into the stamped code),
  but ONLY for the i64-address path (`test_bulk_mem`'s mem32 JIT is ASAN-clean), and **valgrind reports 0
  errors** on the same JIT'd code + the non-ASAN binary returns correct results. So it's an ASAN-vs-stamped-
  code instrumentation interaction, not a miscompute. `test_memory64` carries a `NOJIT=1` switch for ASAN
  runs (interp tier only). If ASAN coverage of the JIT tier is wanted later, root-cause the interaction
  (preserve_none + ASAN shadow on the i64 stencil chain is the suspect).
- [x] **B2 — `wasm_func_call` validates args vs the signature. DONE 2026-06-23.** Was fail-open: a host
  `i32` for a ref param was used by the guest as a ref → wild deref. Fix in `wasm_func_call`: reject on
  arity mismatch; per arg, numeric-exact + (ref) the value's runtime type ≤ the param type via the
  engine's OWN check — exposed `jav_top_ref_matches` + `jav_ht_hierarchy` (un-statics in `jav_runtime.c`,
  declared in `interp.h`) so the **instance path is fully spec-complete** (§3.3 lattice, concrete
  subtyping, hierarchy guard so an externref can't be reinterpreted as a funcinst). Red-first test:
  `test/test_capi.c` (too-few/too-many/wrong-numeric-kind/number-for-ref). Surfaced + fixed a latent
  **conformance-runner bug**: it tagged every `ref.null` as `WASM_FUNCREF` and `ref.host` as externref —
  spec-wrong kinds that only "worked" because the engine didn't validate; now `wast_exec.c` supplies the
  ref-arg's true hierarchy (`ref.null <ht>` → that ht; `ref.host` → any, `ref.extern` → extern).
  Gate: `make test` 76/76, conformance 60113/0/0, both tiers.
  - [x] **B2-host — host-func ref-arg subtyping. DONE 2026-06-23.** The host path now does the same
    §7.1.15 check: per arg, the value's runtime type (`wasm_ref_type`, or a null's static type) must be a
    subtype of the host func's declared param type via `wasm_match_valtype` (host params are abstract, so
    this is the complete check at their granularity). Red-first test in `test/test_capi.c`: an externref
    arg (wrong hierarchy) and a number arg to a `(funcref)` host param both reject; a null funcref accepts.
- [x] **B3 — `exn_copy_fields_to` nfields/copied desync. DONE 2026-06-24 (exn-cluster GC-object design).**
  The host→engine install no longer copies `min(nvals,16)` into a fixed `fields[16]` while setting
  `nfields=nvals` — `jav_exn_alloc` builds a GC exn object sized to the true field count, and the
  per-field types ride in the object (no separate desync-prone array). **Test:** `test_capi.c` throws a
  17-field exception; the 17th value (index 16) reads back correctly via `exn_read`.
- [x] **B4 — exn-store cap/lifetime — DONE 2026-06-24 (the full GC-object design below, gate 77/0,
  conformance 60113/0/0, test_exceptions valgrind-clean both tiers, test_capi ASAN-clean).** Original
  text retained below for the design record.
- [ ] ~~**B4 — exn-store cap/lifetime — a DESIGN PASS, not a quick null→trap fix (with A2/B3).**~~ The
  symptom: `wasm_capi.c:382` nulls a host exnref when `vm->exns[256]` is full while `jav_throw` traps.
  But `exnref` is an *index* into `vm->exns` (`jav_runtime.c:583,601`), and an exnref can be stored in a
  global/table and `throw_ref`'d in a *later* call — so the slot must stay valid across calls (hence the
  monotonic, never-freed, capped array = the A2 leak + the 256 cap + the B3 fixed `fields[16]`). The
  complete fix per the no-fixed-cap rule is the exn store as a vec with **no cap** AND real lifetime —
  exns as GC objects collected by exnref liveness. A half-version (still-capped, or vec-but-leaks, or
  reset-per-call which dangles persisted exnrefs) is exactly a banned partial. Schedule deliberately;
  B4/A2/B3 land together.
  **CONFIRMED DESIGN (2026-06-24, user-approved "full GC-object design"):** the exn becomes a managed
  GC object (reuses the generic `GC_KIND_STRUCT` tracer): payload `{ u4 tag; u4 nfields; slot_t
  fields[nfields] }`; a per-tag `gc_rtt_t` gives `size` + `ref_offsets[]` for the ref fields (16-byte
  slot stride so v128 fields survive — strictly better than today). The rtt is built lazily at throw
  from `vm->tags[tag]` params and **cached on the heap keyed by tagaddr** (no instctx/loader threading).
  `exnref` becomes an ordinary `T_GCREF` gc_obj pointer — already traced on stack/locals/globals/tables,
  so the 256 cap, the `fields[16]` cap, and the monotonic leak all dissolve, and the c-api exnref
  marshaling collapses onto the existing managed-gcref path. `pending_exn` becomes a gc_obj pointer that
  the root scan visits while `unwinding`. BLAST RADIUS (inherent): every throw now needs a live
  collector → the bare-VM exn tests must `jav_heap_gc_init`; the JIT GC-during-throw path applies (same
  as struct.new under JIT). Red-first tests: 300 throws across `wasm_func_call`s (cap/leak gone), a
  >16-param tag (fields cap gone), an exnref in a global surviving a GC churn then `throw_ref` (liveness).
- [x] **B5 — declared-supertype count >1. NOT A BUG (verified 2026-06-23).** The `[A?]` was a false
  alarm: the §3.2.11 declaration check already fail-closes — `jav_module_validate.c:382`
  `if (mod->nsupers[t] > 1) return JAV_E_TYPE_MISMATCH`, on the validation path (`types_err`, :479). The
  index records the TRUE count (`push_sub` `nsupers=ns`); the 255 clamp still leaves >1 → still rejected.
  So a multi-super type is never accepted. (No corpus case because `.wat` can't express 2 supers — it's a
  binary-malformed negative; a hand-assembled byte test would be a nice-to-have for the §7 negatives
  checklist, but the reject is unambiguous in code.)
- [x] **B6 — incremental rooting in `wasm_exception_new`. DONE 2026-06-24 (gate 78/0, ASAN-clean).**
  CONFIRMED real (not just `[A?]`): `slot_of_val` ALLOCATES for ref args (exnref → `exn_install`,
  externref → host box), so a collection while boxing arg *i* could free an already-boxed earlier arg —
  `exn_root` ran only AFTER the whole loop. Fix: root `e` BEFORE the loop (unconditional
  `bbq_vec_push(s->exn_roots, e)` once `e->vals`/`types` are calloc'd) — a partial entry (`types[i]==0`)
  is skipped by `exn_visit_refs` until filled, so each box is a scanned root the instant it lands. Matches
  the engine's root-before-the-moving-alloc discipline (jav_exn_alloc). No deterministic red test: forcing
  a GC mid-2-arg-loop needs fragile heap-threshold manipulation (GC-timing, like A6's masking) — the fix is
  correct-by-construction + ASAN/LSan-verified (no double-root/leak; `exn_install` roots the distinct ARG
  exns, not the new one).
- [x] **B7 — `wasm_func_as_ref` on a host func keeps the closure. DONE 2026-06-23.** Was: built the ref
  from `inst`(NULL)/`index`(0), dropping the closure → calling the round-tripped func **segfaulted**.
  Fix: `wasm_func_as_ref` carries the closure on the ref (borrowed); `wasm_ref_as_func` restores a
  callable view; `wasm_ref_same` compares the closure for funcref identity. Ownership made explicit via
  `owns_host` on `wasm_func_t` — only the `wasm_func_new` original frees the closure, borrowed views
  don't. Red-first (segfault → green); **ASAN/LSan clean** (no double-free/leak), which also caught + fixed
  6 trap leaks in the new test code (discarded `own wasm_trap_t*` → a `CK_TRAP` helper). Gate 76/76,
  conformance 60113/0/0. Host-funcref *in an engine table / call_indirect* was the one remaining gap,
  now **RESOLVED by A1** (a host func backs itself with a real `jav_func_t` funcinst, so a host funcref
  IS a funcinst pointer the engine `call_indirect`s directly — see the A1 entry's host-func bullet).
- [x] **B-elemw — array store-width is correct, not "unreachable". DONE 2026-06-23 (folded into A-width).**
  The `rtt->elem_store_w ?: 4` defaults in `array.new_data`/`init_data` are gone — both read
  `rtt->elem_store_w` directly (always set for an array RTT; `build_rtts` gives i64/f64→8, else→4). The
  default was dead in production but load-bearing in the bare-VM tests, which now set `elem_store_w` on
  their hand-built array RTTs (A-rtt rule). `init_data` also gained a `typ` bounds-trap so the RTT is
  never NULL. See the A-width entry.
- [x] **B10 — `wasm_module_validate` shares one path with `wasm_module_new` + sets the verdict.**
  **[A]** divergent path, leaves `last_status` stale (`wasm_capi.c:608`). **Done (red→green, gate 77/0):**
  both now route through one `module_decode_validate(store,…)` helper that records the §5/§4.5 verdict
  (MALFORMED/INVALID/OK + jav_err_t) on the store, so `wasm_module_validate` can no longer leave
  `last_status` stale or diverge in classification. Dropped the now-unused `jav_validate_bytes` use
  (and `jav_load.h` include) from the c-api. Red-first test in `test_capi.c`: three hand-built modules
  (valid/invalid/malformed) pin each `jav_capi_last_status` verdict. **Surfaced + fixed a latent
  landmine** (Dan caught it): a runtime `jav_export_t` (jav_instance.h) name-colliding with the
  generated parse-AST `jav_export_t` (struct jav_export, jav_types.h) — never in one TU until the test
  pulled both header families; renamed the runtime one to `jav_inst_export_t` (jav_instance.{h,c},
  jav_store.c, wasm_capi.c).
- [x] **A10 — delete the `mem_addrs==NULL ⇒ raw memidx` test escape hatch.** **[V]** a "single-instance
  unit tests" weasel-with-comment branch-checked on every load/store (`jav_runtime.c:380`). Always
  populate `mem_addrs`; drop the branch. **Done:** `mem_at` requires `mem_addrs` (one path, no
  fallback); every bare-VM memory test now sets `mem_addrs`/`num_mems`. Surfaced the embedder seam:
  per §4.5.2/§7.1 the host operates on a **memaddr** (store address) directly, NOT the §4.6.8 memidx
  execution path — so `mem_grow` was split into `mem_grow_inst(jav_mem_t*, delta)` (heap-level, the
  embedder entry) + a thin `mem_grow` that resolves memidx→meminst then delegates. `wasm_memory_grow`
  now grows `mem_of(mo)` directly; the c-api `memidx`→`memaddr` naming corrected to match.
- [x] **A12 — loader vs generated capture-tree shape. DONE 2026-06-24 (gate 77/0, conf 60113/0/0).**
  The two positional `children[0]` switch-unwraps (`jav_module_index.c` `defined_table_type`,
  `jav_instance.c` table-init) are replaced by one named accessor `jav_view_choice(node)` in
  jav_view_nav.{h,c} — "the chosen arm of a discriminated-union node (its lone child)". The §5.5.7 Table
  union (short `tabletype` vs `0x40 0x00 tabletype expr`) is spec-mandated and faithfully modeled by the
  grammar's `switch(peek())`; the fix removes the node-SHAPE coupling (raw `children[0]`), centralizing
  the union assumption in ONE documented place. NOTE: not a spec/correctness bug — the loader was
  correct against §5.5.7; this is robustness against grammar tweaks. (A named *field* for the arm would
  need a grammar/codegen change — out of scope; the accessor is the proportionate engine-side fix.)
- [x] **B11 — test harness >16 results (test-only). DONE 2026-06-24 (gate 77/0, exec 60113/0/0).**
  `run_action` no longer copies `min(narr,16)` into a stack `res[16]` while reporting `nres=narr` (a
  >16-result func made callers read/free `res[16..narr)` garbage). It now hands back the whole result
  array (`wasm_val_t**`, sized to the true arity — the `wasm_func_call` result vec's `.data`, ownership
  transferred); the four callers take `wasm_val_t* = NULL` + `&res`, and `free_res` frees the array too.
  Latent-cap removal per [[feedback_use_crt_not_manual_arrays]]; no corpus case returns >16 results, so
  the gate stays green by construction (noted, not silently capped).

### 6.E — Paper-aligned gaps (Titzer in-place interpreter) — decide before the API freezes
- [~] **Probe / instrumentation seam (Titzer §3.3.3) — PARTIAL: stop-before single-branch only (2026-06-24, gate 78/0, conf 60113/0/0).**
  Instrumentation is an **interpreter-tier facility by design**: the interp is the debug/baseline tier
  (it will never be JIT-fast; its §1.1 reason to exist is tooling — e.g. someone debugging their X→wasm
  compiler runs the interp and watches it), so there is no JIT probe to owe — you instrument by running
  the interp tier. **DONE:** a single `if (vm->probe)` branch in `jav_next` calls `vm->probe(vm, op)`
  BEFORE each interp opcode (NULL ⇒ no-op); red-first `test/test_probe.c` records the op stream
  `41,41,6a` + result 8, NULL-probe records nothing. **EXTERNAL REACHABILITY DONE 2026-06-24 (gate 78/0):**
  the probe is now an embedder-reachable **debug extension** — `jav_capi_set_probe(store, cb, ctx)`
  (declared in the sidecar `jav_extern.h`, NOT the vendored `wasm.h`; a `(ctx, op)` callback via a
  container-of trampoline off the store's embedded vm). The core spec doesn't govern the embedder API and
  `wasm.h` allows extensions (cf. `wasm_config_t`), so a sidecar debug hook is sanctioned — like
  `jav_capi_last_status`. `test_capi.c` block: install → run `i32.const/i32.const/i32.add` → op stream
  `41,41,6a` + result 7; clear (cb=NULL) → fires no more. **NOT DONE (still owed):** the *stop-after* hook
  (inspect results, not just operands); and the paper's two-table main/probed dispatch — the
  zero-main-path-cost refinement of the stop-before that already works functionally. An earlier revision
  called this "the API-visible seam" and demoted the rest to "a future optimization that won't change the
  API" — that laundering is retracted. (The JIT tier is
  NOT a gap here: instrumentation is interp-only by design.)
- [ ] **(note, not a bug) Stack-overflow handling diverges deliberately.** Paper uses guard-page +
  signal; we use explicit reserve-guard checks (`jav_runtime.c:159`, `MAX_STACK`). Portable, signal-free
  — document as an intentional divergence with the per-call-check tradeoff; do NOT "fix" to signals.
- [ ] **(note) MEM #0 base register.** Paper caches it in a register; we index `heap.mems[idx]` per
  access (`mem_at`). Perf only — measure before deciding.

### 6.F — opgen: de-fragilize the access coupling (NOT "support all value models")
Cross-ref `the §8 flat-cache collapse` and **`opgen-vm-contract.md`** (NEW 2026-06-24 — the grounded
opgen↔VM ownership map + the per-native **disposition table**). That doc is the *establish-the-contract*
deliverable: this whole plan is **Phase 1** of the larger refactor ("hoist the opcode logic out of
`jav_runtime.c`"); the contract doc states who owns what and classifies every `native` into a Phase-2
disposition (hoist:control / hoist:arity / hoist:tag / reclassify / substrate-keep / purge). **Phase 2
(the ports) does not start until the contract is agreed**; opgen grows each missing capability as a
*consequence* of porting each family, with both trees stabilized together at each step. The remaining
6.F bullets below are subsumed by that doc's §7 table.

**SCOPE CORRECTION 2026-06-24:** the real problem here was never "make
opgen target-agnostic for any object model." It was that opgen emitted the state-access **shapes** as
literal C (`f->stack[f->sp].<field>`, `vm->globals[idx][0]`, the `(vm_t*,heap_t*)` native prefix) while
the `backend` directive only supplied the *types* — so the project had to **name its fields to match by
convention, not interface** (`runtime_api.h:6-8` admits it). That convention-coupling is exactly what
made the flat instance cache fragile: ~20 vm fields kept in lockstep with `instctx_t`, hand-synced on
every field added as the engine grew (the A4 dual-source-of-truth, and the A3 re-entrancy bug it caused).
**The actual goal: turn that coupling into an interface so the access shape lives in ONE backend-defined
place — which is what unblocks the §8 collapse.** ✅ DONE: the access vocabulary is now routed through
`#ifndef`-guarded backend macros (`GLOBAL_GET/SET/TAG_SET`, `LOCAL_SLOT/TAG_SET`, `JV_STK/STKT/SP`,
`NATIVE_ARGS`), the backend overrode `GLOBAL_GET` to read `frame.ctx`, and the flat cache is gone (R1/§8).

**Important — opgen LEGITIMATELY owns value-model opinions, by design, because it emits the JIT.** The
copy-and-patch JIT forces it: per-opcode `pop`/`push` JIT metadata and the 64-bit-slot-split machinery
come from the value model's slot-WIDTH (`jtype` → slots); the GC root contract reads the parallel
`stack_types[]` tag array that GPOP/GPUSH write; the stencil ABI is `vm_t*`-single-pointer + `frame@0` +
`musttail` + `_HOLE_` relocs. So "opgen agnostic for any value model" was a mis-extrapolation: a
*different tagging discipline* (e.g. NaN-boxing — which DELETES the parallel tag array) is not a backend
`#define`, it's a different opgen MODE that the JIT-meta + GC + GPOP/GPUSH would all have to agree on.
`slot_t` is already GENERATED from the `.def` `type` decls (scalar/uscalar/field per type) — opgen is
agnostic exactly up to the structural commitments the JIT and GC force, and that's the correct boundary.

Clean today (unchanged): the `status`/`type`/`native`/`backend` directives, runtime heap-op API
(`runtime_api.h:42-133` is 100% derived from `wasm.def`), opcode encoding/dispatch/sub-tables, JIT
operand-hole metadata. Remaining baked-in: the `value_type` keyword set in the grammar/ASDL (the one
genuinely-contained item left — see below); dead JVM guard machinery (already PRUNED, below).
- [x] **Dead JVM guard machinery PRUNED 2026-06-24 (gate 77/0, conf 60113/0/0).** `emit_guards` kept only
  the two WASM-live error kinds (DivByZero, Overflow); the JVM-era blocks (NullPointer/ArrayBounds/
  NegativeSize/ClassCast → `throw_null_pointer`/`array_length`+`throw_array_bounds`/
  `throw_negative_array_size`/`instance_of`+`throw_class_cast`) + the whole `emit_post_guards`
  (OutOfMemory) + their forward-decls are gone. VERIFIED dead first: the only live BBQ/opgen consumer is
  `wasm/spec/wasm.def` (declares only DivByZero/Overflow); calc.def = DivByZero; yoctojc uses a separate
  generator (opcgen); `javelina/tools/opgen` is stale. Regen → `gen_interp.c` has ZERO refs to the pruned
  symbols (behavior-equivalent — they never emitted for WASM). The grammar/ASDL `value_type` keyword set
  + the larger state-vocabulary macro-routing (below) REMAIN.
  - [x] **Backend-side residue purged 2026-06-24.** opgen stopped *emitting* the JVM guards above, but
    `jav_runtime.c` still *defined* 5 orphan `JAV_TRAP_FN` stubs (`throw_null_pointer`, `throw_array_bounds`,
    `throw_negative_array_size`, `throw_out_of_memory`, `throw_class_cast`) that nothing referenced (the
    generated `runtime_api.h` declares only `throw_div_by_zero`). Deleted; the lone WASM-live guard
    `throw_div_by_zero` kept as a plain function. (opgen-vm-contract.md §6.)
- [~] **Route the action-language state vocabulary through backend-defined macros** — `PUSH/POP/PEEK/
  DROP(class)`, `LOCAL_GET/SET`, `GLOBAL_GET/SET` (the `[0]` indirection moves into the backend macro),
  `FETCH_<kind>()`/decoder names, `TAG(class)`, `TRAP()/NEXT()/CONT()` + the native calling convention —
  exactly as heap natives already go through `runtime_api.h`. The `.def` already HAS the abstract
  vocabulary (`stack_in`/`stack_out`, `locals[]`, `globals[]`, `code[]`, `sp`, `pc`, `trap;`); only the
  *lowering target* is fixed C. Contained change — all recognition points are the named lower_* fns.
  **PROGRESS 2026-06-24 (each gate 77/0, conf 60113/0/0, behavior-equivalent regen): all three state
  components now route through backend-overridable primitives.** GLOBAL → `GLOBAL_GET/SET/TAG_SET` (the
  R1-relevant read-path — the `[0]` by-ref deref lives in the macro); LOCAL → `LOCAL_SLOT/LOCAL_TAG_SET`
  (storage base in the macro; the value-model field selector `.i/.l/.r` correctly stays in the lowering);
  **OPERAND STACK → `JV_STK`/`JV_STKT`/`JV_SP`** (the value array / tag array / stack pointer — the
  `GPOP_*/GPUSH_*` macros, the entry/return code, and the `.def` `stack[i]`/`sp` idioms all build on
  these). Plus the **native-call convention → `NATIVE_ARGS`** (`vm, vm->heap` — the leading args of every
  runtime native; 112 call sites). Each is a `#ifndef`-guarded GENERATED DEFAULT in `emit_slot_macros`, so
  a backend relocates the storage / changes the native ABI by `#define`-ing first. Verified: generated
  interp has only 3 raw `f->stack`/`f->sp` left — the default macro definitions themselves. **The
  safely-routable state vocabulary is now COMPLETE** (all 3 state components + native ABI). The named
  remaining tokens are NOT clean state-vocabulary: `f->pc` is UNUSED by wasm.def (dead — the cursor is
  `f->code.pos`); `TRAP/NEXT/CONT` + `FETCH` are entangled with the copy-and-patch stencil ABI
  (`_HOLE_cont`/`_HOLE_trap`/`_HOLE_resync`, `TAIL return`, the `STENCIL` qualifier) — the dual-tier JIT
  dispatch convention, not a storage shape; routing those is the delicate "opgen/.def substrate project",
  deferred. **R1's read-path flip + the §8 collapse are now DONE (above).** Genuinely open from here: only
  the open-`value_type`-names bullet below; the slot_t/NaN-box "agnostic for all models" idea is de-scoped
  to a NON-GOAL (the §6.F header SCOPE CORRECTION + the bullet below explain why the JIT/GC own that).
- [ ] **Open the `value_type` name set (the one genuinely-contained item left).** Today the value-type
  *names* a `.def` may use are a closed ASDL enum (`grammar/opgen.asdl`: `TyI32 … TyRef TyWord TyAny
  TyAddr`) — adding one (e.g. `TyAddr` for memory64) meant editing the enum + the peg and regenerating
  opgen's frontend. Let a `.def` `type` decl INTRODUCE its own value-type name instead. This does NOT
  fight the JIT (the stack-effect/JIT-meta machinery keys off `jtype`/slots regardless of the name); the
  only risk is the opgen frontend regen (asdl→ast, peg→parser). The dead JVM guard set is already pruned.
- [ ] **(NON-GOAL for the WASM engine; record only) A different value-MODEL / tagging discipline is an
  opgen "modes" project, not a backend `#define`.** The earlier framing ("make `slot_t`/tag-array
  backend-overridable so NaN-boxing/untagged-word is expressible") was mis-scoped — see the §6.F header
  SCOPE CORRECTION. NaN-boxing DELETES the parallel `stack_types[]` array that GPOP/GPUSH write and the GC
  root scan reads, and changes the slot-count the JIT-meta derives; the JIT + GC + macros would all have
  to agree on a new discipline. That is a separate, larger effort with NO current consumer (javelina IS
  the tagged-union WASM backend). Do not pursue unless a second backend actually needs it.
- [~] When this lands, **R1's interp read-path flips** to `vm->frame.ctx->globals[i]` and the flat
  cache + `jav_vm_load_ctx` delete themselves (the §8 flat-cache collapse) — taking A3 + A4 with them.
  **UNBLOCKED 2026-06-24:** the GLOBAL/LOCAL/stack/native vocabulary is now routed through
  backend-overridable macros, so R1's interp read-path flips by `#define GLOBAL_GET(i)` ⇒
  `(vm->frame.ctx->globals[(i)][0])` in the backend — no further opgen change. REMAINING = the whole
  §8 collapse (flip every interp cluster read through `frame.ctx`, delete the ~20 flat `vm->` fields +
  `jav_vm_load_ctx`, seam just sets `frame.ctx`, ~30 bare-VM tests → `instctx_t`); this is ALSO the A3
  re-entrancy fix. Must be done whole (a globals-only flip is a no-benefit partial) — a large,
  correctness-critical refactor for its own focused pass (the §8 flat-cache collapse is the guide).

### Sequencing (do the abstraction, not the bandaid)
0. **6.0 per-area coverage pass FIRST** — before touching an area, write its to-spec failing tests (the
   c-api ref/host deliverable is the biggest gap). No fix lands without a test that was red first.
1. **6.C A5** (`jav_invoke` + root callback) — everything else gets easier once the shim stops
   open-coding internals; carries B2's validation.
2. **6.B** (activation/engine split) — **R1** kills the flat cache (→ A3 + A4 vanish together), plus A2.
   R1's read-path flip is genuinely blocked on 6.F (opgen owns the access shape), so it sequences after
   6.F — the deferral is a real dependency, not a stopgap; no bandaid for A3 in the meantime.
3. **6.A** (representation unification) — A1 funcref, A-width, A-limits.
4. **6.D B1** (memory64/table64) — pure fail-open correctness, invisible to the corpus.
5. **6.F** (opgen memory model) — contained, high-leverage, unblocks R1's read-path flip; parallelisable.
6. **6.E probe seam** — before the public API freezes.
7. **Adversarial seam tests** for every `[A?]` and every meta-finding boundary — committed, red→green,
   because the conformance corpus cannot discover any of these.

**DoD for Phase 6:** every box checked, each with a committed test that was red before the fix; the 6.0
coverage floor met (every public `wasm.h` fn has a direct unit test or a named conformance case — no
silent 0-coverage API, and no untested stubs); a repo sweep shows zero "test-only"/"simplified … the
tests use"/`?:default`-as-real-value paths and zero breadcrumb comments documenting a cut. `make test`
green incl. the new adversarial + ref/host fixtures, both tiers.

**DoD status 2026-06-24: 3.5 of 4 closed; only the probe's stop-after/two-table refinement remains.**
`make test` green (78/0, conf 60113/0/0); test_capi ASAN-clean.
- ✅ **(1) Coverage floor — CLOSED for jav code.** 103/151 public symbols directly tested; the 14 jav fns
  with zero coverage got red→green tests; remainder = 46 upstream-inline conveniences + the 2 frame
  offsets (now IMPLEMENTED — see the REQUIRED trap-offset item).
- ✅ **(2) Repo sweep — RUN, clean.** No `test-only`/`simplif`/`for now`/`stub`/`?:`-default paths; the
  2 silent stubs it surfaced (`wasm_frame_{func,module}_offset` returning 0) are now IMPLEMENTED.
- 🔶 **(3) 6.E probe — install surface DONE** (`jav_capi_set_probe`, embedder-reachable debug extension,
  red→green); owed = the *stop-after* hook + the two-table zero-cost refinement. The functional
  stop-before capability + the embedder API are in; the rest is a perf/feature refinement.
- ✅ **(4) A6 §4.7.2 step-24 rooting — DONE in the live `wasm_store_t`** (the dead `jav_store_t` is gone);
  red-verified multi-instance struct survival. Plus the REQUIRED trap-frame offsets (interp = all
  reachable execution), B6 exn rooting, the store-design.md deletion, and the `jav_store`→`jav_extern`
  rename all landed 2026-06-24.

**Honesty ledger (2026-06-23 audit; B1 added 2026-06-24):** **fixed + gate-verified** — A1 (funcref-rep
unification, red→green + ASAN), A-limits (explicit has_max + addrtype ceiling, lock-in test + ASAN),
A-width (RTT = one authority, elem_size→constant, dead `?:4` gone, ASAN GC builds), **B1 (memory64/table64
via the opgen `addr` type — red-first both-tier `test_memory64`, interp+natives ASAN-clean, JIT valgrind-
clean; surfaced JIT-TRAP-BAIL + ASAN-JIT)**, **A10 (mem_addrs escape hatch deleted — one memidx→memaddr
path; embedder grow split to `mem_grow_inst` on the memaddr per §4.5.2/§7.1; full gate 77/0 + 60113 conf)**,
**R1/A3/A4 — the the §8 flat-cache collapse flat-cache COLLAPSE (2026-06-24): ~23 flat vm cluster fields → embedded
`instctx_t cluster`; every read through `vm->frame.ctx`; `jav_vm_load_ctx` deleted; A3 re-entrancy fixed
(wasm_func_call saves/restores the outer activation; red-first 2-instance trampoline test → A's value, not
B's); A4 (dual source of truth) gone; §4.5.4 order preserved; gate 78/0, conf 60113/0/0, ASAN+valgrind
clean; ASAN caught + fixed a dangling-frame.ctx UAF in match_valtype)**,
**EXN CLUSTER A2+B3+B4 + A-caps(exns/fields) (2026-06-24 — exception = managed GC object, exnref =
T_GCREF pointer; per-tag rtt cached on the heap by tagaddr; 256-cap + fields[16]-cap + monotonic leak
all gone, reclaim by liveness; red-first 300-throw + 17-field tests; gate 77/0, conf 60113/0/0,
test_exceptions valgrind-clean BOTH tiers, test_capi ASAN-clean)**, B11 (>16-result harness cap removed),
**B10 (wasm_module_validate unified onto one verdict-setting path; red→green; surfaced+fixed the
runtime-vs-generated `jav_export_t` name collision → `jav_inst_export_t`)**, B2, B7, A5, A9, A6(dedup).
**A12 (positional children[0] switch-unwraps → one named `jav_view_choice` accessor; gate 77/0)**,
verified in source — A7, A-rtt, the opgen verb/noun split. Code-structural, trusted — A4 (now
gate-verified via R1), A8 (now DONE — declared in the sidecar + single-threaded contract stated).
**6.F is NOT "trusted-fine" (corrected 2026-06-24):** the storage-vocab macro-routing landed but only
`GLOBAL_*` is actually overridden; "open value_type names" is a real opgen `ValueType`-enum refactor, NOT
the "contained frontend tweak" claimed; the contract's value-model exposure is specified-not-built.
(A2/B3/B4/A10/A12 gate-verified above; A-caps DONE — exns/fields/handlers all `bbq_vec`, no cap.)
**Stale-ledger fixes 2026-06-24:** A3 was triple-filed (gate-verified + by-inspection + unconfirmed) —
collapsed to gate-verified (R1/A3/A4, red-first trampoline test). B3 + B6 were listed below as
"unconfirmed" but are DONE (B3 via the exn-cluster design + 17-field test; B6 via root-before-boxing).
Genuinely **unconfirmed — write the failing test first** — only B5 (the multi-super reject is fail-closed
in code; a hand-assembled binary-malformed negative test is still owed).
(Method note: the spec was read via the Read tool; `pdftotext` was used only to *locate* passages.)

## Out of scope (explicit)
- **The toml → opgen oracle** (an *automated, codegen-time* cross-check of `wasm.def`
  operand signatures/shape/coverage against `instructions.toml` on every build): a separate,
  well-scoped follow-up — NOT this plan. (Captured so it isn't lost.) Distinct from the
  Phase-3 step (0) **one-time coverage audit**, which is in scope: a single manual pass to
  enumerate the unimplemented-opcode gap list so Phase 3 is finite — not a standing build gate.
- **The duplicated `br_table`/`try_table` runtime decode** (validate.c / wasm_runtime.c
  re-decode LEBs the parser already parsed): redundant, not incorrect — a cleanliness
  pass for later, not a conformance item.
- **The `.wat` reader and `water`**: tooling artifacts, deliberately not in the library.

## Riskiest / most-uncertain parts (watch these)
1. **Typed const-expr validator + `ref.null`/`ref.func` admission** (declared-funcref
   set, prior-imported-immutable `global.get`) — highest chance of a silent §7 gap;
   corpus-tested.
2. ✅ **Import type-matching subtyping — DONE** (§3.3.16 via the `jav_subtype` lattice in
   `link_imports`: func contravariant/covariant, table reftype invariant, global const-covariant/
   var-invariant; cross-module CONCRETE func refs `(ref $t)` via structural functype matching —
   needed `WVT_FUNCTYPEREF` added to opgen + decode/validate. Gated in `test_instantiate`.
   Cross-module concrete STRUCT/ARRAY refs remain (need the GC field-type model in the modidx).
3. **`vm_t` single-table / fixed-cap limits** (Phase 3) — a structural extension, the
   biggest engine change; sequence it against exactly which corpus files need it.
4. **NaN canonical-vs-arithmetic + ref-value comparison** in the runner — needs the
   static result type (pull from the func signature), not bit-`==`.
5. **opgen regen for the status enum** — confirm no consumer hard-codes the enum
   cardinality.

## Critical files
- `spec/wasm.bbq` (regen `-prefix jav`), `spec/wasm.def` (status enum; any `vm_t` field
  reshape) — the generated sources.
- `src/wasm_frame.h` (the instance↔engine boundary; `vm_t`/`jav_func_t`/heap seam).
- `src/validate.{c,h}` (`jav_typecheck_ex`/`jav_validate_const_expr` — reused by gate +
  instantiator) and `src/wasm_validate_module.c` (the structural pass).
- `src/wasm_runtime.c` (the host/JIT/interp dispatch seam + `functype_eq` + segment
  runtime the instantiator wires into).
- New: `src/jav_module_validate.{c,h}` (Phase 1 gate + `jav_module_index`),
  `src/jav_instance.{c,h}` (Phase 2), `src/wasm_capi.c` + the public `include/wasm.h`
  (Phase 4), `examples/embed.c` (Phase 4/6).
- `test/test_wast.c` (Phase 5 — the reference-embedder conformance runner + spectest).

## Verification (end to end)
- `cd wasm && make test` green, including: all 63 existing gates after the rename
  (Phase 0); the `assert_invalid`/`assert_malformed` subset (Phase 1); `test_instantiate`
  + `assert_unlinkable`/`assert_uninstantiable` (Phase 2); `examples/embed` against `.a`
  and `.so` (Phase 4/6); and the **full execution corpus, both tiers, 0 unexplained
  mismatched** with named exclusions (Phase 5 — the conformance Definition of Done).
- Independent check that the `.so` exports only `wasm_*` (`nm -D --defined-only`).
