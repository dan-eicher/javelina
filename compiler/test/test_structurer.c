// test_structurer.c — SIR-spine scope identification (structurer.h).
// Three canonical control shapes, each pinned: straight-line (no headers, no
// joins), an if-diamond (one forward join, no back-edge), and a while loop
// (one loop header + one back-edge, no forward join). This is the structure
// the WASM emit recursion consumes to place block/loop scopes and br depths,
// so its classification of every node is pinned before that recursion is built.
#include "javelina/compiler/structurer.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

/* Convenience: is node `n` flagged as a loop header / merge in `s`? */
static bool is_header(const wasm_structure_t* s, const sir_node_t* n) {
    int i = wasm_structure_index(s, n);
    return i >= 0 && s->is_loop_header[i];
}
static bool is_merge(const wasm_structure_t* s, const sir_node_t* n) {
    int i = wasm_structure_index(s, n);
    return i >= 0 && s->is_merge[i];
}

int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 4096);
    bbq_arena* a = &arena;

    /* ── 1. Straight line:  n0 ─▶ n1 ─▶ ret  ───────────────────────────────
     * No back-edges, no node with two predecessors. */
    {
        sir_node_t* ret = sir_return_void(a);
        sir_node_t* n1  = sir_nop(a, ret);
        sir_node_t* n0  = sir_nop(a, n1);

        wasm_structure_t s;
        wasm_structure_build(&s, a, n0);

        CHECK(s.count == 3, "straight-line: 3 reachable nodes");
        CHECK(s.back_edge_count == 0, "straight-line: no back-edges");
        CHECK(!is_header(&s, n0) && !is_header(&s, n1) && !is_header(&s, ret),
              "straight-line: no loop headers");
        CHECK(!is_merge(&s, n0) && !is_merge(&s, n1) && !is_merge(&s, ret),
              "straight-line: no merges");
        /* RPO follows the chain: entry first, return last. */
        CHECK(wasm_structure_index(&s, n0) == 0, "straight-line: entry is RPO[0]");
        CHECK(wasm_structure_index(&s, n0) < wasm_structure_index(&s, n1)
              && wasm_structure_index(&s, n1) < wasm_structure_index(&s, ret),
              "straight-line: RPO respects the chain order");
        wasm_structure_free(&s);
    }

    /* ── 2. If-diamond:   branch ─┬─▶ t ─┐
     *                             └─▶ f ─┴─▶ join ─▶ ret
     * `join` has two forward predecessors → a merge; no back-edges. */
    {
        sir_node_t* ret    = sir_return_void(a);
        sir_node_t* join   = sir_nop(a, ret);
        sir_node_t* t      = sir_nop(a, join);
        sir_node_t* f      = sir_nop(a, join);
        sir_node_t* cond   = sir_load_local(a, 0, SIR_DTINT, NULL);
        sir_node_t* branch = sir_branch(a, cond, t, f);

        wasm_structure_t s;
        wasm_structure_build(&s, a, branch);

        CHECK(s.count == 5, "if-diamond: 5 reachable nodes");
        CHECK(s.back_edge_count == 0, "if-diamond: no back-edges");
        CHECK(is_merge(&s, join), "if-diamond: join is a merge (2 fwd preds)");
        CHECK(!is_merge(&s, t) && !is_merge(&s, f) && !is_merge(&s, branch)
              && !is_merge(&s, ret), "if-diamond: only join is a merge");
        CHECK(!is_header(&s, join) && !is_header(&s, branch),
              "if-diamond: no loop headers");
        int ji = wasm_structure_index(&s, join);
        CHECK(s.fwd_preds[ji] == 2, "if-diamond: join in-degree is 2");
        /* The merge sits after both arms in RPO. */
        CHECK(wasm_structure_index(&s, t) < ji
              && wasm_structure_index(&s, f) < ji,
              "if-diamond: both arms precede the join in RPO");
        wasm_structure_free(&s);
    }

    /* ── 3. While loop:   header ─▶ branch ─┬─(true, back-edge)─▶ header
     *                        ▲              └─(false)──────────▶ exit
     * `header` is a back-edge target (loop header); the branch's true arm is
     * the lone back-edge. No forward joins (header's only forward pred is the
     * loop entry; the back-edge is excluded from the in-degree). */
    {
        sir_node_t* exit_  = sir_return_void(a);
        sir_node_t* cond   = sir_lt(a, sir_load_local(a, 0, SIR_DTINT, NULL),
                                       sir_load_const(a, 10, SIR_DTINT));
        sir_node_t* branch = sir_branch(a, cond, NULL, exit_);
        sir_node_t* header = sir_nop(a, branch);
        branch->branch.on_true = header;   /* the back-edge */

        wasm_structure_t s;
        wasm_structure_build(&s, a, header);

        CHECK(s.count == 3, "while: 3 reachable nodes (header, branch, exit)");
        CHECK(s.back_edge_count == 1, "while: exactly one back-edge");
        CHECK(is_header(&s, header), "while: header is a loop header");
        CHECK(!is_header(&s, branch) && !is_header(&s, exit_),
              "while: only header is a loop header");
        CHECK(!is_merge(&s, header) && !is_merge(&s, branch) && !is_merge(&s, exit_),
              "while: no forward merges (back-edge excluded from in-degree)");
        /* The recorded back-edge runs branch → header. */
        CHECK(s.back_edges[0].from == wasm_structure_index(&s, branch)
              && s.back_edges[0].to == wasm_structure_index(&s, header),
              "while: back-edge is branch → header");
        int hi = wasm_structure_index(&s, header);
        CHECK(s.fwd_preds[hi] == 0, "while: header has no forward predecessors (it is the entry)");
        wasm_structure_free(&s);
    }

    bbq_arena_free(&arena);
    if (fails) { printf("test_structurer: %d FAILED\n", fails); return 1; }
    printf("test_structurer: OK\n");
    return 0;
}
