# Sema contract — javelina Java→WASM compiler

Sema is the static-semantics phase. **Contract: `AST + class environment` in →
`annotated AST + side-chain data` out**, so every later phase (ddcg→SIR, the WASM module
builder) reads sema's resolved results and never recomputes them. Grounded in the JLS
(first edition — the version this compiler implements) and the Alves-Foss/Frincke
attribute grammar for Java (see [BIBLIOGRAPHY.md](../../BIBLIOGRAPHY.md)); the
implementation descends from yoctojc's `sema.c`, minus the JavaCard linking machinery
(which existed only to wire CAP constant-pool indices to a JCVM's builtins — irrelevant
to a WASM target).

## Layering — JLS world ↔ DDCG ↔ target world (they don't mix)
Three layers, strictly separated:
- **Sema = the JLS (source) world.** Java types, name/method/field resolution, the JLS
  checks. It knows NOTHING of the target — no WASM value types, no opcode/struct encodings.
  `type_lattice.c` is pure JLS; `jtype_meta.c` centralizes the per-type-tag *spec* facts
  (JLS §5.1.2 numeric ranges, JVMS §4.3 descriptor characters) and nothing of any
  target's encoding. The `JT_* → WASM value type` map is BACKEND-only.
- **DDCG = the transformation layer.** It is the one place JLS → target happens: it consumes
  sema's JLS-typed AST + side-chain and produces the SIR (the target-bound IR), mapping Java
  types → SIR width tags and lowering control flow to the CPS graph.
- **Backend / module builder = the target (WASM) world.** Consumes the SIR + class table →
  emits `.wasm`. The WASM value-type / struct / vtable / index encoding lives ONLY here,
  speaking the vocabulary the engine's opgen + `wasm.bbq` define.

**The only sanctioned crossover** is *the data the assembler needs*: the resolved dense
`class_id` / method-idx / field-idx / local-slot, the IR width tag, and invoke-kind — values
only sema can compute (they need JLS resolution) that DDCG threads onto the SIR nodes for the
backend. That is exactly the "side-chain → bake onto SIR" crossing below. Nothing
else leaks between source and target.

## Inputs
1. **AST** — `ast_program_t` (package, imports, type decls) from the parser.
2. **Class environment = the runtime library, as real Java sources.** `compiler/lib/`
   (`java.lang`, `java.io`, `java.util`, `javelina.peg`, `javelina.simd`) is actual `.java`
   source with full Java 1.0 signatures — `java.lang`'s taken from JLS first edition
   ch. 20, the edition that still carries the API chapters — with `native` methods only at
   genuine environment edges. The library compiles through the SAME frontend
   (parser→sema), so the classes carry their true members for type-checking and double as
   the runtime contract; they are merged into the program's type decls ahead of user code
   (lowest class_ids). User code resolves against the library's *signatures* (pass 1
   below); its bodies are not re-checked per compile (`sema_ctx_t.analyze_from`).
   Threads (`Thread`/`ThreadGroup`/`Runnable`) are excluded — the target is threadless,
   the language scope is Java 1.0 minus `synchronized`.

## Two passes (JLS-mandated)
- **Pass 1 — declare (`collect_decls`):** collect every class/interface → the class table
  (super, interfaces, fields{name,type,mods,index}, methods{name,param-types,ret,throws,
  mods}); assign **dense indices** `class_id` / field-index / method-index. Checks:
  duplicate decls, modifier legality (`public/abstract/final` exclusive, not
  `abstract`+`final`, no dup modifiers), reference types defined, hierarchy acyclic,
  cannot extend `final`, override rules, abstract-method-implemented,
  blank-final/interface-field init.
- **Pass 2 — type-check bodies (`analyze_bodies`):** with `context` (current class,
  static-vs-instance, return type, throws set), `vars` (locals + slots), `env`. Sets
  per-expression `type`/`value`, resolves names/calls/fields, inserts the conversions the
  JLS requires (recorded for ddcg).

## Outputs — the side-chain (all keyed by AST node; ddcg reads these verbatim)
Annotation ON the AST: `ast_expr_t.etype` — the effective narrow type tag for literal
narrowing. Side tables (htrees on `sema_ctx_t`, all GENERIC):
- `expr_types` (full JLS type) · `data_types` (SIR width tag) · `side_effects`
- `ident_kinds` (LOCAL/PARAM/INSTANCE_FIELD/STATIC_FIELD + slot/dt/field) · `slot_allocs`
  (local var slots) · `resolved_fields` · `resolved_methods` · `resolved_ctors`
- `invoke_kinds` (STATIC/VIRTUAL/SPECIAL/INTERFACE) · `target_classes`
- `switch_infos` (validated, sorted cases + default) · `break/continue_target_depths`
- The **class table** (`classes[]` + `class_by_name`): the enriched class environment out —
  super_id, interface_ids, fields, methods, modifiers, is_interface, fq_name,
  `max_user_slots` per method. This is what the WASM module builder turns into struct/array
  types, vtables/itables, func signatures, and field/global indices.
- `diags` — the error/warning list.

### The chain & the no-recompute invariant (load-bearing)
Sema's side-chain is keyed by `ast_expr_t*` / AST node. **The AST is consumed by DDCG** —
it lowers AST → the defunctionalized CPS continuation graph (SIR), after which no AST node
(and so no AST-keyed table) survives. Therefore the side-chain is NOT disposable sema→ddcg
scaffolding: it is the **source DDCG reads (while it still holds each AST node) to BAKE the
resolved data onto the SIR nodes**. The durable carrier downstream is the **SIR node**.

Consequence — two obligations, co-designed:
1. **The side-chain is COMPLETE** — it carries everything DDCG needs, because DDCG is the
   last place the AST exists. It must not be trimmed on the theory "ddcg can recompute"
   (it can't, once it has emitted SIR).
2. **The SIR node schema (`sir.asdl`) carries every resolved attribute** any later phase
   (Click `sir_optimizer`, the WASM module builder) needs — `data_type`, `class_id`,
   `method_idx`/`field_idx`, local slot, invoke kind (in the node tag) — populated by
   DDCG from sema, never re-derived from the graph structure. The SIR schema is part of
   THIS contract; the backend maps these dense indices to struct types, vtables/itables
   and func signatures through the class table, its own world.

DDCG is the transfer point; sema → (side-chain) → DDCG → (SIR node fields) → Click/codegen.

The downstream contract is the `sema_*` query set the DDCG auxiliaries call
(declared in `compiler.ddcg`'s auxiliary block, implemented in `compiler_helpers.c`):
`sema_data_type` (and `_or`), `sema_ident_kind/_slot/_dt`, `sema_var_slot`,
`sema_field_acc_dt`, `sema_is_array_length`, `sema_array_acc_dt`,
`sema_invoke_kind/_target_class/_dt/_is_void`, `sema_new_target_class`,
`sema_break/continue_target_depth`, `sema_switch_*`, `sema_catch_class_id`,
`sema_ctor_call_target_class`, `sema_may_have_effects`. Every one of these is
target-independent.

## Checks (the ERROR set — the full list is the attribute grammar's)
Conditions boolean (`if`/`while`/`do`); switch selector integral + case const, in-range, no
dup/dup-default; `return` assignable to return type (+ missing/forbidden value); `throw`
Throwable + checked-in-`throws`; catch param Throwable + reachable ordering; break/continue
label defined + in loop; unique labels; assignment conversion (`=`) incl. §5.2 small-constant
narrowing; arithmetic/shift/bitwise/comparison/logical/unary operand typing; cast legality;
`instanceof` reference; array access/index/size; `new` (not interface/abstract, ctor
resolution, no ctor cycle); field/method resolution + overload + visibility + static-context.

## Lineage
Copied faithfully from yoctojc's `sema.c`, then extended to full Java 1.0 types
(long/float/double/char) for this compiler. The JavaCard-only machinery was not ported:
CAP constant-pool indices and token assignment, the `.exp` import loader, atype nibbles,
Shareable/firewall — all of it existed to link against a JCVM's builtins, and a WASM
target resolves against the class environment above instead.
