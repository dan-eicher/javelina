// test_transfer.c — cg_jump per-edge transfer classification (structurer.h).
// Given the recovered scope structure + RPO layout, each continuation edge is
// one of: fall-through (emit nothing), inline return, br-to-loop (back-edge),
// or br-forward (jump to a join's block end). These four are the WASM analogue
// of Dybvig's destination-driven cg_jump (ret / nothing / goto). Pinned on the
// same control shapes the structurer test uses, plus an early-return shape that
// forces the inline-return case.
#include "javelina/compiler/structurer.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

/* Classify the edge from `from` to `to` by locating its successor slot. */
static wasm_transfer_t edge(const wasm_structure_t* s, sir_node_t* from, sir_node_t* to) {
    int fi = wasm_structure_index(s, from);
    int sc = sir_succ_count(from);
    for (int slot = 0; slot < sc; slot++)
        if (sir_succ(from, slot) == to)
            return wasm_classify_edge(s, fi, slot);
    wasm_transfer_t none = { WT_BR_FORWARD, -1 };
    return none;
}

int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 4096);
    bbq_arena* a = &arena;

    /* ── 1. Straight line: both edges are layout-adjacent → fall-through. The
     *    return node is reached by falling into it (it emits its own ret). */
    {
        sir_node_t* ret = sir_return_void(a);
        sir_node_t* n1  = sir_nop(a, ret);
        sir_node_t* n0  = sir_nop(a, n1);
        wasm_structure_t s; wasm_structure_build(&s, a, n0);

        CHECK(edge(&s, n0, n1).kind == WT_FALLTHROUGH, "straight: n0→n1 fall-through");
        CHECK(edge(&s, n1, ret).kind == WT_FALLTHROUGH, "straight: n1→ret fall-through (adjacent return)");
        wasm_structure_free(&s);
    }

    /* ── 2. If-diamond: one arm is adjacent (fall-through), the other branches
     *    forward; the off-layout arm→join is a forward branch, the in-layout
     *    arm→join falls through; join→ret is adjacent. RPO = branch,f,t,join,ret. */
    {
        sir_node_t* ret    = sir_return_void(a);
        sir_node_t* join   = sir_nop(a, ret);
        sir_node_t* t      = sir_nop(a, join);
        sir_node_t* f      = sir_nop(a, join);
        sir_node_t* branch = sir_branch(a, sir_load_local(a, 0, SIR_DTINT, NULL), t, f);
        wasm_structure_t s; wasm_structure_build(&s, a, branch);

        /* on_true (t) is explored first so it lands later in RPO → forward br;
         * on_false (f) is the layout-adjacent arm → fall-through. */
        CHECK(edge(&s, branch, t).kind == WT_BR_FORWARD,   "diamond: branch→t forward br");
        CHECK(edge(&s, branch, f).kind == WT_FALLTHROUGH,  "diamond: branch→f fall-through");
        CHECK(edge(&s, t, join).kind == WT_FALLTHROUGH,    "diamond: t→join fall-through");
        CHECK(edge(&s, f, join).kind == WT_BR_FORWARD,     "diamond: f→join forward br");
        CHECK(edge(&s, join, ret).kind == WT_FALLTHROUGH,  "diamond: join→ret fall-through");
        /* the forward-branch targets the join. */
        CHECK(edge(&s, f, join).to == wasm_structure_index(&s, join),
              "diamond: forward br target is the join");
        wasm_structure_free(&s);
    }

    /* ── 3. While loop: the true arm is the back-edge → br-to-loop; the body
     *    edges are adjacent fall-throughs. RPO = header,branch,exit. */
    {
        sir_node_t* exit_  = sir_return_void(a);
        sir_node_t* cond   = sir_lt(a, sir_load_local(a, 0, SIR_DTINT, NULL),
                                       sir_load_const(a, 10, SIR_DTINT));
        sir_node_t* branch = sir_branch(a, cond, NULL, exit_);
        sir_node_t* header = sir_nop(a, branch);
        branch->branch.on_true = header;   /* back-edge */
        wasm_structure_t s; wasm_structure_build(&s, a, header);

        CHECK(edge(&s, header, branch).kind == WT_FALLTHROUGH, "while: header→branch fall-through");
        CHECK(edge(&s, branch, header).kind == WT_BR_LOOP,     "while: branch→header br-to-loop (back-edge)");
        CHECK(edge(&s, branch, header).to == wasm_structure_index(&s, header),
              "while: loop br targets the header");
        CHECK(edge(&s, branch, exit_).kind == WT_FALLTHROUGH,  "while: branch→exit fall-through");
        wasm_structure_free(&s);
    }

    /* ── 4. Early return: the true arm jumps forward to an early return node
     *    that is NOT layout-adjacent → inline-return (no forward br to a shared
     *    return block). RPO = branch,join,ret,ret_early. */
    {
        sir_node_t* ret_early = sir_return_void(a);
        sir_node_t* ret       = sir_return_void(a);
        sir_node_t* join      = sir_nop(a, ret);
        sir_node_t* branch    = sir_branch(a, sir_load_local(a, 0, SIR_DTINT, NULL), ret_early, join);
        wasm_structure_t s; wasm_structure_build(&s, a, branch);

        CHECK(edge(&s, branch, ret_early).kind == WT_RETURN,
              "early-return: branch→ret_early inline return (non-adjacent return target)");
        CHECK(edge(&s, branch, join).kind == WT_FALLTHROUGH,
              "early-return: branch→join fall-through");
        CHECK(edge(&s, join, ret).kind == WT_FALLTHROUGH,
              "early-return: join→ret fall-through (adjacent)");
        wasm_structure_free(&s);
    }

    bbq_arena_free(&arena);
    if (fails) { printf("test_transfer: %d FAILED\n", fails); return 1; }
    printf("test_transfer: OK\n");
    return 0;
}
