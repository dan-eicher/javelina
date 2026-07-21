# Sema contract — javelina Java→WASM compiler

Sema is the static-semantics phase. **Contract: `AST + class environment` in →
`annotated AST + side-chain data` out**, so every later phase (ddcg→SIR, the WASM module
builder) reads sema's resolved results and never recomputes them. Grounded in the JLS
(`~/Documents/java-langspec-2.0.{pdf,txt}`) and the Alves-Foss/Frincke attribute grammar
(`Formal Grammar for Java.pdf`); copied faithfully from yoctojc's `sema.c`, minus the
JavaCard linking machinery (which existed only to wire CAP constant-pool indices to a JCVM's
builtins — irrelevant to a WASM target).

## Layering — JLS world ↔ DDCG ↔ target world (they don't mix)
Three layers, strictly separated:
- **Sema = the JLS (source) world.** Java types, name/method/field resolution, the JLS
  checks. It knows NOTHING of the target — no WASM value types, no opcode/struct encodings.
  (`type_lattice.c` is pure JLS and stays; `jtype_meta.c`'s tables are JCVM *target* encoding
  — stack cells, CAP field widths, descriptor nibbles — that sat next to sema by accident;
  they are DROPPED. Their WASM analogue, the `JT_* → WASM value type` map, is BACKEND-only.)
- **DDCG = the transformation layer.** It is the one place JLS → target happens: it consumes
  sema's JLS-typed AST + side-chain and produces the SIR (the target-bound IR), mapping Java
  types → SIR width tags and lowering control flow to the CPS graph.
- **Backend / module builder = the target (WASM) world.** Consumes the SIR + class table →
  emits `.wasm`. The WASM value-type / struct / vtable / index encoding lives ONLY here,
  speaking the vocabulary the engine's opgen + `wasm.bbq` define.

**The only sanctioned crossover** is *the data the assembler needs*: the resolved dense
`class_id` / method-idx / field-idx / local-slot, the IR width tag, and invoke-kind — values
only sema can compute (they need JLS resolution) that DDCG threads onto the SIR nodes for the
backend. That is exactly the "side-chain → bake onto SIR" crossing below, now named. Nothing
else leaks between source and target.

## Inputs
1. **AST** — `ast_program_t` (package, imports, type decls) from the parser.
2. **Class environment ("class stuff") = `java.lang` as real Java stub sources.** NOT C
   stubs and NOT a token minimum (Dan: "do what the spec says, not the minimum"). `java.lang`
   is authored as actual `.java` declarations with **full Java 1.0 signatures taken verbatim
   from JLS 1.0 ch. 20** (`java-langspec-1.0.pdf`, the edition that still carries the API
   chapters; 2.0 dropped them), `native` methods for runtime-provided bodies. They compile
   through the SAME frontend (parser→sema) — so the classes carry their true members for
   type-checking and double as the runtime contract — and are merged into the program's type
   decls ahead of user code (lowest class_ids). `java.lang` is **interdependent** (`Object`
   alone references `Class`/`String`/`Throwable`/exceptions), so it's authored as a coherent
   set. The `.exp` loader (`sema_import.c`) is DROPPED (it only linked CAP indices to JCVM
   builtins). Source tree: `compiler/lib/java/lang/*.java`.
   - **Set (threadless WASM target):** `Object` (§20.1, DONE), `Cloneable`, `Class`,
     `Boolean`, `Character`, `Number`, `Integer`, `Long`, `Float`, `Double`, `Math`, `String`
     (§20.12), `StringBuffer` (§20.13), `System` (§20.18), `Throwable`+subclasses (§20.22).
   - **Excluded:** `Thread`/`ThreadGroup`/`Runnable` (no threads), `Process`/`Runtime`/
     `SecurityManager`/`ClassLoader` (OS → host imports).
   - **Page map (book p, PDF ≈ p+26):** Object 458, Cloneable 465, Class 466, Boolean 469,
     Character 471, Number/Integer/Long/Float/Double 487–516, Math 517, String 531,
     StringBuffer 548, System 579, Throwable+subclasses 611.

## Two passes (JLS-mandated)
- **Pass 1 — declare:** collect every class/interface → the class table (super, interfaces,
  fields{name,type,mods,index}, methods{name,param-types,ret,throws,mods}); assign **dense
  indices** `class_id` / field-index / method-index (NOT JCVM tokens). Checks: duplicate
  decls, modifier legality (`public/abstract/final` exclusive, not `abstract`+`final`, no dup
  modifiers), reference types defined, hierarchy acyclic, cannot extend `final`, override
  rules, abstract-method-implemented, blank-final/interface-field init.
- **Pass 2 — type-check bodies:** with `context` (current class, static-vs-instance, return
  type, throws set), `vars` (locals + slots), `env`. Sets per-expression `type`/`value`,
  resolves names/calls/fields, inserts the conversions the JLS requires (recorded for ddcg).

## Outputs — the side-chain (all keyed by AST node; ddcg reads these verbatim)
Annotation ON the AST: `ast_expr_t.etype` — the effective narrow type tag for literal
narrowing. Side tables (htrees on `sema_ctx_t`, all GENERIC — keep):
- `expr_types` (full JLS type) · `data_types` (SIR width tag) · `side_effects`
- `ident_kinds` (LOCAL/PARAM/INSTANCE_FIELD/STATIC_FIELD + slot/dt/field) · `slot_allocs`
  (local var slots) · `resolved_fields` · `resolved_methods` · `resolved_ctors`
- `invoke_kinds` (STATIC/VIRTUAL/SPECIAL/INTERFACE) · `target_classes`
- `switch_infos` (validated, sorted cases + default) · `break/continue_target_depths`
- The **class table** (`classes[]` + `class_by_name`): the enriched "class stuff" out —
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
1. **The side-chain must be COMPLETE** — carry everything DDCG needs, because DDCG is the
   last place the AST exists. Don't trim it on the theory "ddcg can recompute" (it can't,
   once it has emitted SIR).
2. **The SIR node schema (`sir.asdl`) must carry every resolved attribute** any later phase
   (Click `sir_optimizer`, the WASM module builder) needs — `data_type`, `class_id`,
   `method`/`field` index, local slot, invoke-kind, target class, etc. — so nothing is
   recomputed from the graph structure. `sir.asdl`'s comment already states this for
   `data_type`; the SIR schema is part of THIS contract. **For WASM-GC we extend the SIR
   nodes** with the resolved bits the backend needs (struct-type index, field index,
   vtable/itable slot, func-signature index) — populated by DDCG from sema, never re-derived.

DDCG is the transfer point; sema → (side-chain) → DDCG → (SIR node fields) → Click/codegen.

The downstream contract is the `sema_*` query set the DDCG auxiliaries call
(`compiler.ddcg:100-199`, impl `compiler_helpers.c`): `sema_data_type`, `sema_ident_kind/
_slot/_dt`, `sema_var_slot`, `sema_field_acc_dt/_is_static/_obj_class_id`,
`sema_is_array_length`, `sema_array_acc_dt`, `sema_invoke_kind/_target_class/_dt/_is_void`,
`sema_new_target_class`, `sema_break/continue_target_depth`, `sema_switch_*`,
`sema_catch_class_id`, `sema_ctor_call_target_class`, `sema_may_have_effects`. Every one of
these is target-independent and stays.

## Checks (the ERROR set — full list in the JLS attribute grammar)
Conditions boolean (`if`/`while`/`do`); switch selector integral + case const, in-range, no
dup/dup-default; `return` assignable to return type (+ missing/forbidden value); `throw`
Throwable + checked-in-`throws`; catch param Throwable + reachable ordering; break/continue
label defined + in loop; unique labels; assignment conversion (`=`) incl. §5.2 small-constant
narrowing; arithmetic/shift/bitwise/comparison/logical/unary operand typing; cast legality;
`instanceof` reference; array access/index/size; `new` (not interface/abstract, ctor
resolution, no ctor cycle); field/method resolution + overload + visibility + static-context.
(yoctojc implements all ~51; copy them.)

## Dropped (JavaCard-only)
`cp_indices`, `cp_entries`, `sema_cp_entry_t`, `invoke_tokens`, `array_init_elem_types`
(atype nibbles), `import_pkgs`, `class_token`/`import_pkg` fields, the token-assignment passes,
`sema_import.c`, static-final export inlining (optional), Shareable/firewall.

## Extends (for full Java 1.0 + WASM-GC) — couples with the SIR section
- Type tags: add `JT_LONG`/`JT_FLOAT`/`JT_DOUBLE`/`JT_CHAR` to `java_type_tag_t` (today
  byte/short/int/bool/void/class/array/null/error). `char` is its own 16-bit type.
- SIR width tag `data_types`: add `LONG`/`FLOAT`/`DOUBLE` (today BYTE/SHORT/INT/REF). WASM
  width mapping (backend): byte/short/char/int→`i32`, long→`i64`, float→`f32`, double→`f64`.
- Binary/unary numeric promotion (`jcvm_promote` → JLS §5.6) extended for long/float/double.
- `array_init` element type: generic element-type, not the JCVM atype nibble.

## Build order (incremental, each tested under `wasm/test`)
1. Copy the closure (`sema`, `descriptor`, `type_lattice`, `jtype_meta`, `analyses`); wire the
   Makefile; get it COMPILING with `.exp`/CP/token code excised and built-ins stubbed.
2. Programmatic `java.lang` registration → sema type-checks a self-contained class.
3. Port the check families (red→green tests per family, against the JLS list).
4. Extend the type system to `long/float/double/char` (with the SIR section).
