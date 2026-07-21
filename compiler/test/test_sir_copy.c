// test_sir_copy.c — the generated deep-copy (sir_node_copy) from the forked
// asdl template. The SIR is a graph, not a tree: the CPS spine has shared
// merge points (a node reached by two predecessors) and loop back-edges
// (a successor pointing at an ancestor). A correct clone must (a) terminate
// on cycles, (b) give every original node exactly one copy, (c) preserve
// sharing — two edges to the same original become two edges to the same
// copy — and (d) be deep: operands and sequence elements are fresh copies,
// not aliases of the source. This is the foundation the SIR→SIR optimizer
// passes (inlining, unrolling) clone with, so its graph-fidelity is pinned
// here before anything builds on it.
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 4096);
    bbq_arena* a = &arena;

    /* ── A loop: header is reached both as the entry and via the branch's
     *    true-arm back-edge (so it has two predecessors → shared + cyclic).
     *    The false-arm exits to a distinct Nop.
     *
     *        header(Nop) ──next──▶ branch(Branch)
     *           ▲                    │  on_true  (back-edge)
     *           └────────────────────┘
     *                                │  on_false
     *                                ▼
     *                              exit(Nop → NULL)
     *      cond = Lt(LoadLocal 0, LoadConst 10)
     */
    sir_node_t* cond   = sir_lt(a, sir_load_local(a, 0, SIR_DTINT, NULL),
                                   sir_load_const(a, 10, SIR_DTINT));
    sir_node_t* exit_  = sir_nop(a, NULL);
    sir_node_t* branch = sir_branch(a, cond, NULL, NULL);
    sir_node_t* header = sir_nop(a, branch);
    branch->branch.on_true  = header;   /* back-edge — the cycle */
    branch->branch.on_false = exit_;

    sir_copy_memo memo = {0};
    sir_node_t* H = sir_node_copy(a, &memo, header);

    /* (a)+(d) distinct: every reachable copy is a different object. */
    CHECK(H && H != header, "header copied to a fresh node");
    sir_node_t* B = H->nop.next;
    CHECK(B && B != branch, "branch copied to a fresh node");
    CHECK(B->tag == SIR_BRANCH, "branch tag preserved");
    CHECK(B->branch.cond && B->branch.cond != cond, "cond copied (fresh)");

    /* (b)+(c) sharing + cycle: the branch's back-edge resolves to the SAME
     *  copied header (one copy of header, two edges into it), not a second
     *  clone and not the original. Reaching this line at all proves the
     *  cycle terminated. */
    CHECK(B->branch.on_true == H, "back-edge points at the one header copy (shared+cyclic)");
    CHECK(B->tag == SIR_BRANCH && B->branch.on_false && B->branch.on_false != exit_,
          "false-arm copied to a fresh exit node");
    CHECK(B->branch.on_false->tag == SIR_NOP && B->branch.on_false->nop.next == NULL,
          "exit structure preserved (Nop → NULL)");

    /* (d) deep: the comparison's operand sub-tree is cloned with values intact. */
    sir_node_t* lt = B->branch.cond;
    CHECK(lt->tag == SIR_LT, "Lt tag preserved");
    CHECK(lt->lt.left->tag == SIR_LOADLOCAL && lt->lt.left->load_local.slot == 0,
          "Lt.left = LoadLocal slot 0 (deep copy)");
    CHECK(lt->lt.right->tag == SIR_LOADCONST && lt->lt.right->load_const.value == 10
          && lt->lt.right->load_const.data_type == SIR_DTINT,
          "Lt.right = LoadConst 10:int (deep copy)");
    CHECK(lt->lt.left != cond->lt.left && lt->lt.right != cond->lt.right,
          "operands are fresh copies, not aliases");
    sir_copy_memo_dispose(&memo);

    /* ── Sequence fields: a node-array (args) deep-copies element-by-element;
     *    scalar leaves of the new widths copy by value. ── */
    sir_node_t* args[2] = { sir_load_const(a, 1, SIR_DTINT),
                            sir_load_const(a, 2, SIR_DTINT) };
    sir_node_t* call = sir_invoke_static(a, 7, 3, args, 2, SIR_DTINT);
    sir_copy_memo memo2 = {0};
    sir_node_t* C = sir_node_copy(a, &memo2, call);
    CHECK(C != call && C->tag == SIR_INVOKESTATIC, "call copied");
    CHECK(C->invoke_static.args_count == 2 && C->invoke_static.class_id == 7 &&
          C->invoke_static.method_idx == 3,
          "both args_count (sequence) and scalar leaves copied");
    CHECK(C->invoke_static.args != call->invoke_static.args, "args array is a fresh allocation");
    CHECK(C->invoke_static.args[0] != args[0] && C->invoke_static.args[1] != args[1],
          "each arg element is a fresh copy");
    CHECK(C->invoke_static.args[0]->load_const.value == 1
          && C->invoke_static.args[1]->load_const.value == 2,
          "arg element values preserved");
    CHECK(C->invoke_static.class_id == 7 && C->invoke_static.method_idx == 3
          && C->invoke_static.return_type == SIR_DTINT, "call scalars + enum preserved");
    sir_copy_memo_dispose(&memo2);

    /* The width-carrying leaves (int64/f32/f64 by value). */
    sir_copy_memo memo3 = {0};
    sir_node_t* lng = sir_node_copy(a, &memo3, sir_load_long_const(a, 0x1122334455667788LL));
    sir_node_t* flt = sir_node_copy(a, &memo3, sir_load_float_const(a, 1.5f));
    sir_node_t* dbl = sir_node_copy(a, &memo3, sir_load_double_const(a, 2.25));
    CHECK(lng->tag == SIR_LOADLONGCONST && lng->load_long_const.value == 0x1122334455667788LL,
          "LoadLongConst i64 value preserved");
    CHECK(flt->tag == SIR_LOADFLOATCONST && flt->load_float_const.value == 1.5f,
          "LoadFloatConst f32 value preserved");
    CHECK(dbl->tag == SIR_LOADDOUBLECONST && dbl->load_double_const.value == 2.25,
          "LoadDoubleConst f64 value preserved");
    sir_copy_memo_dispose(&memo3);

    bbq_arena_free(&arena);
    if (fails) { printf("test_sir_copy: %d FAILED\n", fails); return 1; }
    printf("test_sir_copy: OK\n");
    return 0;
}
