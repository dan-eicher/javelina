# compiler/docs/

Design and correctness documentation for the Java→WASM compiler. Each entry
says what the document *is* — a normative spec, design rationale, or a phase
contract — so a reader knows how to weigh it. The literature everything here
answers to is catalogued in the repo-root [BIBLIOGRAPHY.md](../../BIBLIOGRAPHY.md).

| document | role | what it is |
|---|---|---|
| [combined-analysis-spec.md](combined-analysis-spec.md) | **design spec** | The theory of the combined pointer / escape / value analysis over the SIR value-flow graph — the lattices, the interprocedural summaries, and §8 "why there is no dominator tree." The design the optimizer implements. |
| [ddcg-merge-labels.md](ddcg-merge-labels.md) | **normative spec** | Emit-once control destinations (merge labels) in the WASM backend. Implements the destination-driven-code-generation paper exactly; deviations are bugs. |
| [sema-contract.md](sema-contract.md) | **contract** | The static-semantics phase: its `AST + class environment → annotated AST + side-chain` contract, the JLS/DDCG/target layering, and the `sema_*` query surface the backend consumes. |
