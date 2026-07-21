/* structurer.h — control-flow scope identification over the SIR spine.
 *
 * The SIR spine is the cg_jump continuation graph (a CFG: nodes linked by
 * sir_succ). To lower it to WASM structured control (block/loop/if + br) the
 * backend must first recover scopes from the edge structure. This is the
 * first half (Ramsey "Beyond Relooper" / Hecht-Ullman reducibility): one DFS
 * recovering reverse-postorder, the back-edge set, loop headers (back-edge
 * targets), and forward-join points (≥2 non-back-edge predecessors).
 *
 * NOT a dataflow analysis — no lattice/fixpoint (that's analyses.c's Kildall
 * framework). This is a pure graph walk; the nesting recursion that consumes
 * it (and emits the actual block/loop/br) is the structured-emit step. */
#ifndef STRUCTURER_H
#define STRUCTURER_H

#include "javelina/compiler/sir_support.h"
#include "bbq_arena.h"
#include "bbq_htree.h"
#include "bbq_vec.h"

/* A retreating edge: from spine node `from` (its successor slot `succ_slot`)
 * back to loop header `to`. Endpoints are reverse-postorder indices. */
typedef struct {
    int from;
    int to;
    int succ_slot;
} wasm_back_edge_t;

/* The recovered scope structure. Per-node arrays are indexed by reverse-
 * postorder position (0..count-1); map a node to its index with
 * wasm_structure_index. Arrays are arena-allocated; the vecs/htree are owned
 * and released by wasm_structure_free. */
typedef struct {
    bbq_arena*        arena;
    sir_node_t**      rpo;             /* reverse-postorder node list (vec) */
    int               count;
    bbq_htree*        index;           /* node ptr → (rpo index + 1) */
    bool*             is_loop_header;  /* [count]: target of ≥1 back-edge */
    bool*             is_merge;        /* [count]: ≥2 forward predecessors */
    int*              fwd_preds;       /* [count]: non-back-edge in-degree */
    wasm_back_edge_t* back_edges;      /* vec */
    int               back_edge_count;
} wasm_structure_t;

/* Walk the spine rooted at `entry`, filling `s`. Safe for entry == NULL
 * (yields an empty structure). */
void wasm_structure_build(wasm_structure_t* s, bbq_arena* arena, sir_node_t* entry);

/* Release owned vecs/htree (the arena-allocated arrays are the caller's). */
void wasm_structure_free(wasm_structure_t* s);

/* Reverse-postorder index of `n`, or -1 if unreachable from entry. */
int wasm_structure_index(const wasm_structure_t* s, const sir_node_t* n);

/* ── cg_jump transfer classification ──────────────────────────────────────
 * Dybvig's destination-driven cg_jump decides, per continuation edge, how to
 * reach the successor. The WASM lowering chooses the structured equivalents
 * below. The *kind* is fixed
 * by the graph + layout (computed here); the concrete br *depth* is resolved
 * later by the emit recursion, which owns the scope stack. */
typedef enum {
    WT_FALLTHROUGH,  /* successor is the next node in layout — emit nothing   */
    WT_RETURN,       /* continuation is the function return — emit ret inline */
    WT_BR_LOOP,      /* retreating edge — br to the enclosing loop header      */
    WT_BR_FORWARD,   /* forward edge to a non-adjacent join — br to its block  */
} wasm_transfer_kind_t;

typedef struct {
    wasm_transfer_kind_t kind;
    int                  to;   /* RPO index of the successor (-1 if none) */
} wasm_transfer_t;

/* Is out-edge (rpo node `from`, successor slot `succ_slot`) a back-edge? */
bool wasm_edge_is_back(const wasm_structure_t* s, int from, int succ_slot);

/* Classify out-edge (rpo node `from`, successor slot `succ_slot`). Precedence:
 * back-edge → WT_BR_LOOP; layout-adjacent → WT_FALLTHROUGH; target is a
 * function-return terminator → WT_RETURN; otherwise → WT_BR_FORWARD. */
wasm_transfer_t wasm_classify_edge(const wasm_structure_t* s, int from, int succ_slot);

#endif /* STRUCTURER_H */
