# The opgen ↔ VM contract (WASM backend)

**Status: Phase 1 — establishing the contract.** This document is the deliverable of
the "solidify the API" / substrate-project groundwork. It states *who owns what* between
opgen (the generator) and the Javelina VM (the backend), grounded in the code as it
stands today, and it classifies every runtime service into a disposition that the
*later* porting phase executes.

Javelina is the first real use of opgen beyond the `calc` example, so this is also
where we name the WASM-/JVM-specific assumptions that leaked into opgen under pressure
and decide which are real opgen concepts vs. residue to purge.

> **Phasing.** Phase 1 (this doc + the JCVM-residue purge) establishes the contract.
> Phase 2 ports opcode logic out of `jav_runtime.c` into the `.def` DSL *per the
> disposition table below*; opgen grows each missing capability as a **consequence** of
> the port, and the two codebases stabilize together. The whole `wasm-engine-plan.md`
> is Phase 1 of that larger refactor. **Do not start Phase 2 ports before the contract
> here is agreed.**

---

## 1. Two domains

| | **opgen owns** (the abstract machine) | **the VM provides** (the substrate) |
|---|---|---|
| structure | the value model: 16-byte uniform `slot_t`, the parallel tag array, `addr`-width discipline (`spec.cpp`, `vmemit.cpp:emit_value_model`) | the concrete storage: `frame_t`/`vm_t`/`heap_t` (`jav_frame.h`, `backend "jav_frame.h"` at `wasm.def:11`) |
| behavior | stack discipline (pop/push from typed signatures), structured control (the Titzer side-table format), dispatch, the copy-and-patch JIT ABI | service implementations — the `native`s declared in `wasm.def` and implemented in `jav_runtime.c` (heap, memory, tables, GC) |
| seam | what the backend must supply: storage-access macros + `native` prototypes → generated `src/gen/runtime_api.h` | the `#define` overrides + the `.c` bodies behind those prototypes |

The principle: **opgen owns the abstract machine; the VM owns the substrate.** The line
is "coupling by convention → coupling by interface." Today the value model is owned by
opgen *structurally* but not *exposed*, which is the root of the debt in §4.

---

## 2. The clean pattern (already realized)

For memory, arithmetic, local/global, the contract already works as intended — the
`.def` declares a typed stack effect, opgen generates the pop/push + JIT stencil + the
validator transfer function, and the body calls a typed `native`:

```
i32_load 0x28 [memarg offset] (addr addr -- i32 result)
(.  result = mem_load_i32(memidx, addr + offset);  .)        # wasm.def:170

ref_func 0xd2 [uleb32 func]   ( -- funcref result )
(.  result = jav_ref_func(func);  .)                         # wasm.def:735
```

`mem_load_i32` / `jav_ref_func` take typed args, return a typed value; opgen owns the
stack. **This is the target shape for everything in §4.**

---

## 3. The debt: opcodes that lie to opgen with `( -- )`

A large class of opcodes declare an **empty** stack effect `( -- )` and then reach
directly into `f->stack` / `f->sp` / `f->stack_types` inside the native — invisible to
opgen. The signature is a lie (e.g. `struct.new` really is `(field… -- ref)`), which
means opgen cannot generate their pop/push, cannot reason about them in the JIT, and
cannot derive their validator transfer function. They hand-roll the value model.

```
struct_new 0xfb 0 [uleb32 type] ( -- )   (.  jav_struct_new(type);  .)   # wasm.def:763
table_get  0x25    [uleb32 table]( -- )   (.  jav_table_get_op(table); .) # wasm.def:864
call       0x10    [uleb32 func] ( -- )   (.  jav_call(func);  .)        # wasm.def:680
br         0x0c    [uleb32 label]( -- )   (.  jav_br();  .)              # wasm.def:664
throw      0x08    [uleb32 tag]  ( -- )   (.  jav_throw(tag);  .)        # wasm.def:269
ref_test   0xfb 20 [sleb64 ht]   ( -- )   (.  jav_ref_test(ht, 0);  .)   # wasm.def:842
```

They are `( -- )` for **four distinct reasons**, each a separate integration into opgen:

### (a) Control transfer — *split-brain to heal*
`do_transfer` (`jav_runtime.c:33`) walks `f->sidetable[f->stp]`. **opgen builds the
side-table; the backend walks it.** The format lives in both trees. Natives:
`jav_br`, `jav_br_if`, `jav_if`, `jav_else`, `jav_br_table`, `jav_return`,
`jav_br_on_null`, `jav_br_on_non_null`, `jav_br_on_cast`, `jav_br_on_cast_fail`
(`jav_runtime.c:49–84, 57–69, 1030–1042`). **Disposition: opgen should *generate*
`do_transfer` + the branch dispatch** (it owns the side-table). ~10 natives leave the
backend. Cleanest hoist; no new opgen concept, just moving the walk to its owner.

**BUILT 2026-07-19** — see the §7 row. The split-brain is healed: one generated walk,
inlined in both tiers, shared by the branch ops and by `do_throw`.

### (b) Dynamic arity — *new opgen signature form*
The pop/push count comes from an immediate or a runtime type, which opgen's fixed-arity
`(a b -- c)` cannot express: `call`/`call_indirect`/`call_ref`/`return_call*` pop
`fn.num_params` push `fn.num_results`; `struct.new` pops `nfields`; `array.new_fixed`
pops `n`; `throw` pops the tag's field count (`jav_runtime.c:113, 868, 1103, 707`).
**Disposition: add an opgen stack-effect form for immediate-/type-driven arity**
("variadic by immediate"). Then the signature stops lying and opgen owns the marshaling.

**BUILT 2026-07-19, pop side complete** — see the §7 rows. The mechanical evidence: the
signature-honesty meter (`gen_trap_reasons`, checked against `instructions.toml`'s §7.10
`type` column on every build) reads **0 LIAR**, down from 11; the eleven moved into
`carrier` (78 → 89), i.e. arity now matches and only the slot spelling is a carrier type.
**Push side deliberately NOT built.** `call`'s results cannot be declared — the op is
terminal, so opgen has nowhere to emit post-call work — and a variadic-result emitter was
built, proved dead, and removed rather than left as an unused mechanism waiting to be
misused. Result marshaling stays in `jav_call_fn` **by design, not as debt.**

### (c) Dynamic tag — *value-model exposure*
The *result's tag* (`T_REF` scalar handle vs `T_GCREF` managed pointer) is
runtime-dependent: `table.get` reads `t->types[idx]`; `struct.get`/`array.get` decide
from `rtt->nrefs`/`elem_is_ref` (`jav_runtime.c:367, 881, 923`). opgen gives each result
a *static* type, so these hand-roll the tag write. **Disposition: expose a value-model
primitive for setting a result's runtime tag** (a tagged-push the body can call, or let
a `native` return a tagged value) — see §5.

### (d) Could-be-clean — *just give them a signature*
Fixed arity, static result type, hand-rolled only out of expedience:
`ref.test`/`ref.cast` are `(ref -- i32)` / `(ref -- ref)`; `ref.eq` is `(ref ref -- i32)`;
`array.len` is `(ref -- i32)` (`jav_runtime.c:1018, 1025, 1215, 936`).
**Disposition: reclassify to typed signatures** like §2; the natives become typed
(`int jav_ref_test_v(ref, ht, nul)`), opgen owns the stack. Low-hanging.

---

## 4. The value-model contract (the crux)

opgen owns the value model *structurally* (slot width, the parallel tag array, which
tags are GC-managed, the `addr` runtime width) — but **does not expose it**, so every
stack-driven native re-implements it. Two concrete duplications:

- **`pop_addr`/`push_addr` (`jav_runtime.c:20–25`) duplicate opgen's `GPOP_ADDR`** —
  the same addr-width stack discipline, hand-written in the backend.
- **Every stack-driven native hand-rolls** `f->stack[f->sp].l = …; f->stack_types[f->sp]
  = T_GCREF; f->sp++` instead of a shared push.

**Deliverable: opgen exposes the value model as a vocabulary the backend uses** —
so the tag/addr discipline has exactly one owner (opgen) and natives stop re-implementing it.
**BUILT 2026-06-24 (gate 78/0, conf 60113/0/0, both tiers).** opgen now emits, into
`runtime_api.h` (the header the natives include — `emit_runtime_api_h`, after the backend
substrate is in scope):
- **`JV_NPOP_ADDR(f, name)`** (tag-driven addr pop) + **`JV_NPUSH_ADDR(f, val, is64)`** (addr push)
  — emitted from the i32/i64 jrows, no hand-coded fields/tags;
- **`JV_NPUSH(f, slot, tag)`** + **`JV_NPOP(f, sv, tv)`** — the generic tagged push/pop primitive
  (whole slot + parallel tag + sp), tag-agnostic so a native supplies its own tag (incl. the
  backend's `T_GCREF`).
**Migrated (the hand-rolling deleted):** `pop_addr`/`push_addr` are GONE — all 13 addr sites
(call_indirect/return_call_indirect, table.get/set, table.size/grow/fill, memory.size/grow) use
the `JV_N*_ADDR` ops; the 7 identical gcref pushes (struct.new/_default, array.new/_default/_fixed/
_data/_elem) now `JV_NPUSH(f, (slot_t){.l = …}, T_GCREF)`. **What this paragraph originally claimed — that the push/pop vocabulary was the seam — was an
over-claim, and it is now moot.** `JV_NPUSH`/`JV_NPOP` only let a *native* move slots+tags without
poking `f->stack`; at the time the opcode body still called a per-opcode wrapper that hand-rolled
the reduction, which is the `any_t`-carrier hack [[feedback_subsystem_api_is_spec_7_1_embedding]]
warns against rather than the design's target. **The real deliverable landed 2026-07-19**
(`tender-dancing-rabbit.md` Stages 0–3): the bodies compose the access macros and spec §7.1 ops, the
wrappers are dissolved, and the c-api shares the same authority. See the §7 rows. The residue is
§3(c)'s `any_t` result carrier on the getters, counted in §8's carrier meter.

---

## 5. Irreducible substrate (stays `native`, by design)

These are not debt; they are the genuine runtime API and must remain backend services.
Each with *why it is irreducible*:

| service | files | why it stays |
|---|---|---|
| linear-memory **STORAGE LAYER** — buffer alloc, `memory.grow` (`realloc`), `memory.init` (data-segment read) | `jav_runtime.c`, `jav_mem.h` | the heap owns the byte buffer's lifetime/growth — a genuine boundary |
| ~~linear-memory access~~ — **CORRECTED 2026-06-25 (Dan): the bytecode does the §4.6.8 access INLINE.** `mem_load/store_*` are now `inline native` (`jav_mem.h`, `static inline`) — bounds-check + the byte read/write fold into the handler/stencil, no extern. §5 had over-classified this as substrate; the split is *storage layer = heap (substrate)* vs *access reduction = bytecode (inline)*, same as tables (`tableinst` grow substrate, `table.get` inline) and GC (`jav_gc_new` substrate, field access inline). Proven on `i32.load`/`store`; the rest are mechanical. |
| heap allocation (`jav_gc_new`), GC root scan (`jav_gc_enum_roots`), collector binding | `jav_runtime.c:789–845` | the GC↔VM root protocol; reads the tag arrays opgen owns but the heap is the VM's |
| object layout (struct/array/exn rtts, `exn_obj`, `jav_host_box`) | `jav_runtime.c:612–651, 824–845` | concrete GC object shape; opgen never models the heap |
| the call **frame machinery** (`jav_call_fn` nested-run, frame save/restore, tail-call loop, depth limit, the `fn->invoke` tier seam) | `jav_runtime.c:108–221` | the "no linker" core + the interp/JIT seam; the *opcode* parts (param/result marshaling, trap conditions) hoist via §3(b), the *frame* machinery stays |
| §3.3 subtype lattice service (`jav_ht_sub`, `jav_ht_hierarchy`, `value_heaptype`) | `jav_runtime.c:953–1016` | a type-system query the validator/casts call; data lives in the instance |
| copy-and-patch **escape-hatch numerics** (`convert_u64_f64`, `trunc_u64_f32/f64`) | `jav_runtime.c:757–769` | clang synthesizes a `.rodata` constant for u64↔fp that **cannot ride a stencil** — a *permanent, principled* native category, not a hack |

The last row is reason #2 the backend exists at all (the other being "no linker"): some
opcode logic *must* be a native because its codegen is incompatible with the
copy-and-patch ABI. The contract should **name this category**, not pretend it away.

---

## 6. JCVM residue to purge (Phase 1)

opgen's guard vocabulary is named after JVM exceptions; WASM has only traps. opgen now
emits **only** `throw_div_by_zero` (`vmemit.cpp:1000` `throws[] = {"throw_div_by_zero"}`,
`emit_guards` at `:47`), and the generated `runtime_api.h:138` declares only that one —
but the backend still defines **5 orphan stubs** that nothing references:
`throw_null_pointer`, `throw_array_bounds`, `throw_negative_array_size`,
`throw_out_of_memory`, `throw_class_cast` (`jav_runtime.c:777–781`). **Purge them**
(done in the change that lands with this doc). `pop_addr`/`push_addr` (§4) are the other
residue — folded into the value-model exposure rather than deleted outright.

---

## 7. Disposition table (the Phase-2 work list)

| native family | files | disposition |
|---|---|---|
| `jav_br*`, `jav_if`, `jav_else`, `jav_return`, `jav_br_table`, `jav_br_on_*`, `do_transfer` | `jav_runtime.c:33–84, 1030–1042` | **DONE (2026-07-19).** opgen generates the walk as `opgen_do_transfer`, a `static inline` in `runtime_api.h` emitted from the side-table format opgen owns; the backend supplies only the cursor macros (`OPGEN_ST_TABLE`/`OPGEN_ST_PTR`/`OPGEN_IP`). It inlines into BOTH tiers — no extern, no `_HOLE_`, verified by `objdump -r` on `jav_stencils.o`. All ten branch wrappers are deleted and `do_throw` shares the same routine (`jav_runtime.c:559`) rather than forking it. |
| `jav_call*`, `jav_return_call*`, `jav_call_indirect/ref` (opcode logic) | `jav_runtime.c:224–352` | **DONE (2026-07-19), params only.** All six opcodes declare their param count (`[jav_func_nparams(func)]` / `[jav_type_nparams(type)]`) with `flag: pops_first`. **One atomic change across seven entries**: `jav_call_fn` no longer pops — it documents a *precondition* that params are already popped, and the two non-opcode entries (`jav_call` by funcidx, `jav_invoke_fn` for the c-api) drop sp themselves. Declaring any single opcode alone double-pops for that one and under-pops for the other six — that is the 30-minute suite hang of 2026-07-19. The **result half stays in `jav_call_fn` and is not declarable**: `jav_invoke_ref` returns `status` so its lowering tail-returns, leaving opgen nowhere to emit post-call work. The four indirect/ref forms take arity from the TYPE immediate, not the funcinst, so the declared effect is identical to the one the validator derives. |
| `jav_struct_new*`, `jav_array_new*`, `jav_throw` (arity) | `jav_runtime.c:707, 858, 901, 1094` | **DONE (2026-07-19).** Variadic signatures + inline `struct_store`/`array_store` in the body; `jav_obj_set_field`/`jav_obj_set_elem` deleted. The allocators return a `ref` (not an `any_t` carrier) so the constructor writes go through the SAME inline macro `struct.set`/`array.set` use — that carrier mismatch, not the arity form, was the actual blocker. `throw_ref` likewise declares its exnref operand and its null trap (`error:` → `NullExceptionReference`); `jav_throw_ref` no longer reaches into `f->stack`. |
| `jav_table_get`, `jav_struct_get`, `jav_array_get*` (tag) | `jav_runtime.c` | **DONE (2026-07-19).** The reduction moved into the `.def` bodies via the indexed-accessor + access-macro capability: `tables[t][i]` / `struct[…]` / `array[…]` lower to `TABLE_GET/SET`, `STRUCT_GET/SET`, `ARRAY_GET/SET` backend macros (`jav_frame.h`), bounds and null guards declared as `error:` clauses. `jav_table_get/set/fill/copy`, `jav_struct_get/set(_packed)`, `jav_array_get/set(_packed)`, `jav_array_len` are all deleted — none remain in `jav_runtime.c`. **One impl shared with the c-api:** both the opcode and `jav_tableinst_read/write` (the c-api's entry) go through `TABLEINST_GET/SET`; the c-api entry adds only the bounds check an embedder's unvalidated index needs, while the opcode's bounds are already proved by its declared guard. The two-authorities split is closed. The getters still carry their result out through `any_t.kind` — that is §3(c)'s carrier, tracked below, not a wrapper. |
| `jav_ref_test/cast`, `jav_ref_eq`, `jav_array_len` | `jav_runtime.c` | **DONE (2026-06-25).** `ref.eq`/`array.len` reclassified to typed signatures — the reduction is in the `.def` body, wrapper gone (`ref_eq` → `result = (a==b)?1:0;`). `ref.test`/`ref.cast` pop/push hoisted via an `any` operand; their residual native (`jav_ref_test`/`jav_ref_cast`) is a §3.3 **lattice query** = legitimate §5 substrate, not a wrapper to dissolve. |
| memory, heap alloc, GC scan, object layout, lattice, escape-hatch numerics | §5 rows | **substrate-keep** |
| `pop_addr`/`push_addr` | `jav_runtime.c:20–25` | **fold** into value-model exposure (§4) |
| 5 JVM guard stubs | `jav_runtime.c:777–781` | **purge** (§6, Phase 1) |

---

## 8. What "done with Phase 1" means

1. This contract is agreed (ownership + dispositions).
2. The JCVM guard residue is purged (§6).
3. The **native-callable value-model vocabulary (§4) is BUILT** (2026-06-24) — opgen emits
   `JV_NPOP_ADDR`/`JV_NPUSH_ADDR`/`JV_NPUSH`/`JV_NPOP` into `runtime_api.h`; the addr discipline
   (`pop_addr`/`push_addr` deleted) and the gcref pushes are migrated. **This is NOT "Phase 1 done bar
   mechanics."** The subsystem-API seam itself — `.def` bodies composing spec §7.1 ops, wrappers
   dissolved, one impl shared with the c-api — is **unstarted** (Phase 2 / `tender-dancing-rabbit.md`).
   The dynamic-arity / control-transfer / tag forms (§3a/b/c) remain *specified, not built*. As of
   2026-06-25 the only opcodes actually hoisted-to-spirit are `ref.eq` and `array.len` (§7); the
   `any_t` carrier on `table.get`/`struct.get`/`array.get` made their signatures honest but left the
   reduction in the wrappers — do not read those as ported.

**Status update 2026-07-19.** The paragraph above is now out of date on two of its three counts:
§3(a) control transfer and §3(b) dynamic arity (pop side) are **built**, per the §7 rows —
`tender-dancing-rabbit.md` Stages 1–3. What that leaves, stated so it is not re-read as complete:

- **§3(c)'s "native returns a tagged value" hatch is still a hatch.** It is the reason the
  constructors could not simply call the inline store: `STRUCT_SET` casts its object operand to a
  pointer, and a freshly-allocated object arrived as an `any_t` carrier. Routing the allocators
  through a `ref` return dissolved it *for the constructors*; the getters still carry their result
  out through `any_t.kind`.
- ~~The getter/setter wrappers are still wrappers~~ — **dissolved (2026-07-19)**, see the §7 row.
  The reductions are in the `.def` bodies behind the access macros, and the c-api shares the same
  `TABLEINST_GET/SET` authority. What survives of §3(c) is the getters' `any_t` result carrier.

- **Carrier signatures are the remaining §6.3 debt, and the count went UP.** `gen_trap_reasons`
  reports 385 exact / **89 carrier** / 0 liar / 1 variadic / 22 polymorphic. The arity fix that took
  liars 11 → 0 moved all eleven into the carrier bucket (78 → 89) rather than to exact: `memory.size`
  and `table.size` declare `( -- word result )`, `any.convert_extern`/`extern.convert_any` declare
  `( any a -- any result )`, `array.new` declares `( any init, i32 len -- word result )`. Arity is now
  honest — which is what the validator's transfer function and a VC backend's stack-effect obligation
  need — but the slot types are not the spec's. Do not read "0 liars" as "arity debt discharged."

- **A carrier result is a CORRECTNESS defect, not precision loss — falsified 2026-07-20.**
  `test/test_carrier_addr_tag.c`. The chain is forced at every link: a `-- any result` signature gives
  opgen no static type, so it emits `GPUSH_ANY` writing a **runtime** `.kind`; so `ARRAY_GET`/
  `STRUCT_GET` must compute that field per access as `elem_is_ref ? T_GCREF : T_INT` — an RTT chase
  plus a branch, recovering what the type section already stated statically; and that test is
  **binary**, so i64/f64/v128 all collapse to `T_INT`. `GPOP_ADDR` then dispatches on the tag
  (`T_LONG` ⇒ 64-bit, else truncate to 32). **Result: an i64 reaching an addrtype operand through a
  carrier is truncated.** With 0xDEAD at address 0 of a one-page memory64, address 2^32 traps as an
  `i64.const` but `array.get`/`struct.get` return 0xDEAD where §4.6.8 requires a trap — identically on
  both tiers, so it is the shared tag model. Corpus-invisible: the testsuite never crosses memory64
  with GC aggregates, so 60113/0 passes over it, and `test_memory64.c` pins the no-truncation rule
  only for `i64.const` addresses (its own header states this).

  A concrete signature is not simply available: `array.get $t`'s result type depends on the type
  immediate, which is why the carrier exists. Two composable fixes — **(a)** carry the element's full
  valtype in the RTT (`elem_tag`) instead of deriving a binary `elem_is_ref`, making the tag lossless;
  **(c)** a result-type-from-immediate signature form, the type-directed analogue of §3(b)'s `[count]`
  variadic-arity form — in the stencil the type immediate is already a patched constant, so the tag
  becomes patch-time and the runtime branch disappears entirely.

- **Tier asymmetry on trapping natives (latent, recorded per `merry-crunching-fog`).** `gen_interp.c`
  contains **zero** `vm->trapped` checks after native calls: it relies on the trap macro also setting
  `frame.code.pos = length`, so the next dispatch hits EOF. The JIT does not rely on that — it emits
  96 explicit `if (vm->trapped)` checks. Harmless today because both tiers reach a trap state, but the
  interpreter will push a garbage result after a trapping native where the JIT will not. Any future
  native whose trap does not also terminate the code cursor breaks the interpreter tier only.
- ~~57 instructions still carry their trap cause inside a native~~ — **native-hidden is now 0**
  (2026-07-19). All 17 spec trap reasons are declared by `wasm.def`, 95 instructions have full
  `error:` coverage, and the memory accessors in `jav_mem.h` are raw: their precondition is the
  declared guard, cross-checked against the spec's natural alignment on every build.
  `salty-proving-woodpecker.md` §7's completion meter reads zero.
