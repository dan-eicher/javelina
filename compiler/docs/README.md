# compiler/docs/

Design and correctness documentation for the Java→WASM compiler. Each entry
says what the document *is* — a normative contract, design rationale, or a dated
audit record — so a reader knows how to weigh it.

| document | role | what it is |
|---|---|---|
| [combined-analysis-spec.md](combined-analysis-spec.md) | **design spec** | The theory of the combined pointer / escape / value analysis over the SIR value-flow graph — the lattices, the interprocedural summaries, and §8 "why there is no dominator tree." The design the optimizer implements. |
| [theorem-audit.md](theorem-audit.md) | **audit record** | The optimizer's rules checked against the source papers' theorems, entry by entry — correctness resting on the theorems, not on tests passing. |
| [ddcg-merge-labels.md](ddcg-merge-labels.md) | **normative spec** | Emit-once control destinations (merge labels) in the WASM backend. Implements the destination-driven-code-generation paper exactly; deviations are bugs. |
| [sema-contract.md](sema-contract.md) | **contract** | The static-semantics phase: its `AST + class environment → resolved AST` contract and the `sema_*` query surface the backend consumes. Notes the yoctojc lineage and the JavaCard/JCVM machinery deliberately *not* ported. |
| [host-abi.md](host-abi.md) | **contract** | The compiler-side view of the →HOST native contract (the imports the compiler emits). Overlaps with the engine's `docs/host-abi.md`, which is the fuller, authoritative host-ABI reference. |
| [click-reenable-audit.md](click-reenable-audit.md) | **audit record** (2026-07-11) | The burn-down of re-enabling the Click optimizer on full-Java breadth: the JCVM-envelope assumptions and shortcuts found and fixed while wiring it in. A record of that work, not a live checklist. |
