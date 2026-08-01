/* test_click_partition.c — value-graph construction for the Click
 * partition-refinement engine that lives in src/compiler/sir_optimizer.c.
 *
 * Each test hand-builds a minimal SIR method, so the correct value
 * graph is known exactly, and checks cp_build against the structure
 * the Click thesis §4.1 defines: Nodes with ordered input edges, a
 * φ node at each control-flow merge, and a def-use index that is the
 * exact inverse of the input edges. */
#include "javelina_test.h"

/* This suite is written in Unity's assertion vocabulary. Unity is gone; the
 * vocabulary is defined here in terms of the shared CHECK.
 *
 * The one semantic Unity supplied that CHECK does not is ABORT: a failed
 * assertion ended that test case (longjmp) rather than falling through. Every
 * case here is a `static void test_*(void)`, so the same thing is spelled
 * `return` — the assertion stops its own case and the runner moves on, which
 * is what the 186 not-null guards below depend on. Each condition is evaluated
 * exactly once. */
#define JT_ASSERT_(ok, label)                                                  \
    do { int jt_ok_ = (ok) ? 1 : 0; CHECK(jt_ok_, (label)); if (!jt_ok_) return; } while (0)

#define TEST_ASSERT_TRUE(c)                    JT_ASSERT_((c), #c)
#define TEST_ASSERT_TRUE_MESSAGE(c, m)         JT_ASSERT_((c), (m))
#define TEST_ASSERT_FALSE_MESSAGE(c, m)        JT_ASSERT_(!(c), (m))
#define TEST_ASSERT_MESSAGE(c, m)              JT_ASSERT_((c), (m))
#define TEST_ASSERT_NOT_NULL(p)                JT_ASSERT_((p) != NULL, #p " != NULL")
#define TEST_ASSERT_NOT_NULL_MESSAGE(p, m)     JT_ASSERT_((p) != NULL, (m))
#define TEST_ASSERT_NULL_MESSAGE(p, m)         JT_ASSERT_((p) == NULL, (m))
#define TEST_ASSERT_EQUAL_INT(e, a)            JT_ASSERT_((long)(e) == (long)(a), #a " == " #e)
#define TEST_ASSERT_EQUAL_INT_MESSAGE(e, a, m) JT_ASSERT_((long)(e) == (long)(a), (m))
#define TEST_ASSERT_EQUAL_HEX32_MESSAGE(e, a, m)                               \
    JT_ASSERT_((uint32_t)(e) == (uint32_t)(a), (m))
#define TEST_ASSERT_EQUAL_PTR(e, a)            JT_ASSERT_((const void*)(e) == (const void*)(a), #a " == " #e)
#define TEST_ASSERT_EQUAL_PTR_MESSAGE(e, a, m) JT_ASSERT_((const void*)(e) == (const void*)(a), (m))

static int jt_cases = 0;
#define RUN_TEST(fn) do { jt_cases++; fn(); } while (0)

#include "javelina/compiler/sir_optimizer.h"
#include "javelina/compiler/sir_op_gamma.h"
#include "javelina/compiler/sir_support.h"
#include "gen/wasm_ops.h"   /* WOP_* — the SIMD nodes' op payload vocabulary */

#include <math.h>
#include <stdbool.h>
#include <string.h>

/* These tests hand-build the SIR, so THE TEST IS THE FRONTEND for it: it is the
 * thing that knows where the merges are, and it records them — the optimizer
 * reads that record and never goes looking. `scopes` is the test's record; for a
 * straight-line method it is EMPTY, and empty is the complete record, not
 * "unknown".
 *
 * The compiler context is the one object the whole compiler passes around; a
 * test builds a one-method one for its hand-made SIR. */
static compiler_ctx_t tcx_storage;
static sir_method_t*  tcx_method;
static compiler_fact_t* tcx_facts;
static int tcx_fact_count;

/* ONE fact table, as the DDCG would have recorded it. A test hands the rows it is
 * pinning (SCOPEs, ALLOCs, …) and the engine reads them exactly as it reads the
 * real compiler's — there is no test-only path into the optimizer's structure. */
static compiler_ctx_t* tcx(bbq_arena* a, sir_method_t* m,
                            compiler_fact_t* facts, int nfacts) {
    memset(&tcx_storage, 0, sizeof tcx_storage);
    tcx_method     = m;
    tcx_facts      = facts;
    tcx_fact_count = nfacts;
    tcx_storage.arena            = a;
    tcx_storage.sema             = NULL;
    tcx_storage.methods          = &tcx_method;
    tcx_storage.method_count     = 1;
    tcx_storage.all_facts        = &tcx_facts;
    tcx_storage.all_fact_counts  = &tcx_fact_count;
    return &tcx_storage;
}

/* Build over the ONE context with the facts the DDCG would have RECORDED — including
 * each allocation SITE's "can this run more than once" flag, which is what tells a
 * CONCRETE object (§2's strong update is sound) from a SUMMARY (weak only). The
 * optimizer READS it; it never recomputes control flow to find it. */
static cp_engine_t* tbuild_facts(bbq_arena* a, sir_method_t* m,
                                 compiler_fact_t* facts, int nfacts) {
    compiler_ctx_t* c = tcx(a, m, facts, nfacts);
    return cp_build_ctx(c, m, facts, nfacts);
}

/* Straight-line hand-built SIR: nothing to record. */
static void topt(sir_method_t* m, bbq_arena* a) {
    sir_optimize(tcx(a, m, NULL, 0), 0);
}

/* The first value node with the given opcode, or NULL. */
static cp_vnode_t* cp_find_op(cp_engine_t* e, int op) {
    for (int i = 0; i < e->vnode_count; i++)
        if (e->vnodes[i]->op == op) return e->vnodes[i];
    return NULL;
}

/* The sole φ node, or NULL; fails if there is more than one. */
static cp_vnode_t* cp_only_phi(cp_engine_t* e) {
    cp_vnode_t* phi = NULL;
    for (int i = 0; i < e->vnode_count; i++) {
        if (e->vnodes[i]->kind != CP_VN_PHI) continue;
        /* Spelled out rather than via TEST_ASSERT_*: this helper returns a
         * pointer, so the vocabulary's bare `return` would not compile. */
        CHECK(phi == NULL, "expected at most one phi node");
        if (phi != NULL) return NULL;
        phi = e->vnodes[i];
    }
    return phi;
}

/* Number of φ nodes in the graph. */
static int cp_phi_count(cp_engine_t* e) {
    int n = 0;
    for (int i = 0; i < e->vnode_count; i++)
        if (e->vnodes[i]->kind == CP_VN_PHI) n++;
    return n;
}

/* The value node built from a given SIR expression node. */
static cp_vnode_t* cp_vnode_for(cp_engine_t* e, sir_node_t* expr) {
    for (int i = 0; i < e->vnode_count; i++)
        if (e->vnodes[i]->expr == expr) return e->vnodes[i];
    return NULL;
}

/* The φ node merging `slot` at the given merge spine node. */
static cp_vnode_t* cp_find_phi(cp_engine_t* e, sir_node_t* merge, int slot) {
    for (int i = 0; i < e->vnode_count; i++) {
        cp_vnode_t* v = e->vnodes[i];
        if (v->kind == CP_VN_PHI && v->phi_merge == merge && v->phi_slot == slot)
            return v;
    }
    return NULL;
}

/* §4.1.2: `1 + 2` enumerates an ADD Node whose two ordered input
 * edges point at the two LoadConst Nodes. A straight-line method has
 * no merge, hence no φ; the graph is exactly those three expression
 * nodes plus one slot seed. */
static void test_cp_enumeration_exact(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c1  = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* add = sir_add(&a, SIR_DTSHORT, c1, c2);
    sir_node_t* ret = sir_return(&a, add, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    cp_vnode_t* vadd = cp_find_op(e, SIR_ADD);
    TEST_ASSERT_NOT_NULL(vadd);
    TEST_ASSERT_EQUAL_INT(2, vadd->input_count);
    TEST_ASSERT_EQUAL_PTR(c1, e->vnodes[vadd->inputs[0]]->expr);
    TEST_ASSERT_EQUAL_PTR(c2, e->vnodes[vadd->inputs[1]]->expr);
    TEST_ASSERT_EQUAL_INT(0, cp_phi_count(e));
    TEST_ASSERT_EQUAL_INT(4, e->vnode_count);

    cp_free(e);
    bbq_arena_free(&a);
}

/* Reaching definitions: a LoadLocal resolves to the exact value
 * node defining its slot — here the constant the StoreLocal wrote. */
static void test_cp_reaching_def_exact(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c7  = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ld  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret = sir_return(&a, ld, SIR_DTSHORT);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTSHORT, NULL, c7, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    cp_vnode_t* vld = cp_find_op(e, SIR_LOADLOCAL);
    TEST_ASSERT_NOT_NULL(vld);
    TEST_ASSERT_EQUAL_INT(1, vld->input_count);
    TEST_ASSERT_EQUAL_PTR(c7, e->vnodes[vld->inputs[0]]->expr);

    cp_free(e);
    bbq_arena_free(&a);
}

/* φ placement at a forward merge: two branch arms each define slot 0,
 * the join carries one φ whose contributors are exactly the two arm
 * values, and a LoadLocal past the join reads that φ. */
static void test_cp_phi_at_forward_merge(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ld   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret  = sir_return(&a, ld, SIR_DTSHORT);
    sir_node_t* join = sir_nop(&a, ret);
    sir_node_t* v10  = sir_load_const(&a, 10, SIR_DTSHORT);
    sir_node_t* v20  = sir_load_const(&a, 20, SIR_DTSHORT);
    sir_node_t* st_t = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v10, join);
    sir_node_t* st_f = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v20, join);
    sir_node_t* cond = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* br   = sir_branch(&a, cond, st_t, st_f);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    cp_vnode_t* phi = cp_only_phi(e);
    TEST_ASSERT_NOT_NULL(phi);
    TEST_ASSERT_EQUAL_PTR(join, phi->phi_merge);
    TEST_ASSERT_EQUAL_INT(0, phi->phi_slot);
    TEST_ASSERT_EQUAL_INT(2, phi->input_count);
    sir_node_t* in0 = e->vnodes[phi->inputs[0]]->expr;
    sir_node_t* in1 = e->vnodes[phi->inputs[1]]->expr;
    TEST_ASSERT_TRUE_MESSAGE((in0 == v10 && in1 == v20) ||
                             (in0 == v20 && in1 == v10),
                             "phi contributors must be the two arm values");
    cp_vnode_t* vld = cp_find_op(e, SIR_LOADLOCAL);
    TEST_ASSERT_NOT_NULL(vld);
    TEST_ASSERT_EQUAL_INT(CP_VN_PHI, e->vnodes[vld->inputs[0]]->kind);

    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.1.3: the def-use index is the exact inverse of the input edges
 * — y is in x.def_use_i exactly when y[i] = x. Checked on the
 * branch/merge graph, which has expression, φ, and opaque nodes. */
static void test_cp_defuse_inverts_inputs(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ld   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret  = sir_return(&a, ld, SIR_DTSHORT);
    sir_node_t* join = sir_nop(&a, ret);
    sir_node_t* v10  = sir_load_const(&a, 10, SIR_DTSHORT);
    sir_node_t* v20  = sir_load_const(&a, 20, SIR_DTSHORT);
    sir_node_t* st_t = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v10, join);
    sir_node_t* st_f = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v20, join);
    sir_node_t* cond = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* br   = sir_branch(&a, cond, st_t, st_f);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    /* Forward: every def-use entry of x records a real user of x. */
    for (int x = 0; x < e->vnode_count; x++) {
        for (int k = e->du_off[x]; k < e->du_off[x] + e->du_cnt[x]; k++) {
            int y = e->du_user[k], i = e->du_input[k];
            TEST_ASSERT_TRUE(y >= 0 && y < e->vnode_count);
            TEST_ASSERT_TRUE(i >= 0 && i < e->vnodes[y]->input_count);
            TEST_ASSERT_EQUAL_INT(x, e->vnodes[y]->inputs[i]);
        }
    }
    /* Reverse: every input edge appears once in its target's chain. */
    for (int y = 0; y < e->vnode_count; y++) {
        cp_vnode_t* vy = e->vnodes[y];
        for (int i = 0; i < vy->input_count; i++) {
            int x = vy->inputs[i];
            TEST_ASSERT_TRUE(x >= 0 && x < e->vnode_count);
            int found = 0;
            for (int k = e->du_off[x]; k < e->du_off[x] + e->du_cnt[x]; k++)
                if (e->du_user[k] == y && e->du_input[k] == i) found++;
            TEST_ASSERT_EQUAL_INT(1, found);
        }
    }

    cp_free(e);
    bbq_arena_free(&a);
}

/* φ at a loop header: the back-edge makes the header a merge. Its φ
 * for slot 0 takes the loop body's stored value and the slot's entry
 * value (an opaque seed) as contributors. */
static void test_cp_phi_at_loop_header(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);            /* next patched below */
    sir_node_t* v1     = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* body   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v1, header);
    sir_set_next(header, body);
    sir_node_t* entry  = sir_nop(&a, header);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 1, entry);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    cp_vnode_t* phi = cp_only_phi(e);
    TEST_ASSERT_NOT_NULL(phi);
    TEST_ASSERT_EQUAL_PTR(header, phi->phi_merge);
    TEST_ASSERT_EQUAL_INT(2, phi->input_count);
    cp_vnode_t* a0 = e->vnodes[phi->inputs[0]];
    cp_vnode_t* a1 = e->vnodes[phi->inputs[1]];
    bool seen_v1   = (a0->expr == v1) || (a1->expr == v1);
    bool seen_seed = (a0->kind == CP_VN_OPAQUE) || (a1->kind == CP_VN_OPAQUE);
    TEST_ASSERT_TRUE_MESSAGE(seen_v1,   "loop phi must take the body's value");
    TEST_ASSERT_TRUE_MESSAGE(seen_seed, "loop phi must take the entry seed");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.7 LOADLOCAL-as-COPY: a LoadLocal is a one-input pass-through
 * of its reaching definition — the prototypical COPY Follower (thesis
 * §4.7.1 / §4.7.4 ll. 2451-2453). Two reads of an unmodified slot at
 * different program points have reaching defs that converge to one
 * partition (one direct seed, one via a trivial PHI that itself becomes
 * Follower-of-seed). The two LoadLocal vnodes must share a partition;
 * otherwise every downstream expression that uses both splits apart,
 * which is the proximate cause of c?x:y peer PHIs failing to unify. */
static void test_cp_loadlocal_copy_through_trivial_phi(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* ld_pre reads slot 0 before the branch: reaching def = seed_0.
     * ld_post reads slot 0 after a join where neither arm touched
     * slot 0: reaching def = trivial PHI(seed_0, seed_0), which
     * §4.9 must collapse to a Follower of seed_0. Both LoadLocals
     * must converge to a single partition. */
    sir_node_t* ld_post = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret     = sir_return(&a, ld_post, SIR_DTSHORT);
    sir_node_t* join    = sir_nop(&a, ret);
    sir_node_t* arm_t   = sir_nop(&a, join);
    sir_node_t* arm_f   = sir_nop(&a, join);
    sir_node_t* ld_pre  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br      = sir_branch(&a, ld_pre, arm_t, arm_f);
    sir_method_t* m     = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_pre  = cp_vnode_for(e, ld_pre);
    cp_vnode_t* v_post = cp_vnode_for(e, ld_post);
    TEST_ASSERT_NOT_NULL(v_pre);
    TEST_ASSERT_NOT_NULL(v_post);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_pre->partition, v_post->partition,
        "two LoadLocal(s) reading an unmodified slot must share a "
        "partition (§4.7 LOADLOCAL-as-COPY through Follower-of-seed PHI)");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Congruence soundness inside a loop: two structurally-different Sub
 * expressions must NOT share a partition. With i a loop PHI:
 *   inner = Sub(Add(a,b), 1)   (stored to slot 6, loop-invariant)
 *   outer = Sub(Load(s6), i)   (depends on the loop variable)
 * They differ in both operands, so refinement must split them. If merged,
 * CSE collapses `outer` to a copy of `inner`, dropping the `- i` term —
 * the echo-reverse miscompile. The straight-line case splits correctly;
 * the bug only appears when the differing operand is a loop PHI. */
static void test_cp_distinct_subs_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);          /* next patched below */
    /* Mirror echo's slot threading: Add→s5, inner Sub→s6, outer Sub→s7. */
    sir_node_t* addv  = sir_add(&a, SIR_DTSHORT, sir_load_local(&a, 0, SIR_DTSHORT, NULL),
                                                 sir_load_local(&a, 1, SIR_DTSHORT, NULL));
    sir_node_t* inner = sir_sub(&a, SIR_DTSHORT,
        sir_load_local(&a, 5, SIR_DTSHORT, NULL), sir_load_const(&a, 1, SIR_DTSHORT));
    sir_node_t* outer = sir_sub(&a, SIR_DTSHORT,
        sir_load_local(&a, 6, SIR_DTSHORT, NULL),
        sir_load_local(&a, 2, SIR_DTSHORT, NULL));
    sir_node_t* iinc = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT, sir_load_local(&a, 2, SIR_DTSHORT, NULL),
                                 sir_load_const(&a, 1, SIR_DTSHORT)), header);
    sir_node_t* s7 = sir_store_local(&a, 7, SIR_DTSHORT, NULL, outer, iinc);
    sir_node_t* s6 = sir_store_local(&a, 6, SIR_DTSHORT, NULL, inner, s7);
    sir_node_t* s5 = sir_store_local(&a, 5, SIR_DTSHORT, NULL, addv, s6);
    sir_set_next(header, s5);
    sir_node_t* init = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_load_const(&a, 0, SIR_DTSHORT), header);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 8, init);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vi = cp_vnode_for(e, inner);
    cp_vnode_t* vo = cp_vnode_for(e, outer);
    TEST_ASSERT_NOT_NULL(vi);
    TEST_ASSERT_NOT_NULL(vo);
    TEST_ASSERT_TRUE_MESSAGE(vi->partition != vo->partition,
        "Sub(Add(a,b),1) and Sub(x,i) differ in both operands and must "
        "land in different congruence partitions");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SIMD family identity: the `op` payload is part of a Simd node's VALUE
 * identity (γ bucket_discriminator — same mechanism as GetField's
 * (class,field)). Two i32x4.adds over the same operands are one value; an
 * i32x4.sub over the SAME operands is not. And SimdConst is deliberately
 * NOT congruent (128 payload bits cannot ride an exact 32-bit
 * discriminator; a hashed bucket would merge two DIFFERENT constants
 * forever, since a leaf's partition can never be split by refinement) —
 * so two identical constants stay in separate partitions: the safe
 * direction, pinned here so nobody "optimizes" it into a miscompile. */
static void test_cp_simd_op_payload_is_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* add1 = sir_simd_bin(&a, WOP_I32X4_ADD,
        sir_load_local(&a, 0, SIR_DTV128, NULL), sir_load_local(&a, 1, SIR_DTV128, NULL));
    sir_node_t* add2 = sir_simd_bin(&a, WOP_I32X4_ADD,
        sir_load_local(&a, 0, SIR_DTV128, NULL), sir_load_local(&a, 1, SIR_DTV128, NULL));
    sir_node_t* sub  = sir_simd_bin(&a, WOP_I32X4_SUB,
        sir_load_local(&a, 0, SIR_DTV128, NULL), sir_load_local(&a, 1, SIR_DTV128, NULL));
    sir_node_t* la0  = sir_simd_extract_i(&a, WOP_I32X4_EXTRACT_LANE, 0,
        sir_load_local(&a, 0, SIR_DTV128, NULL));
    sir_node_t* la3  = sir_simd_extract_i(&a, WOP_I32X4_EXTRACT_LANE, 3,
        sir_load_local(&a, 0, SIR_DTV128, NULL));
    sir_node_t* k1   = sir_simd_const(&a, 0x1111, 0x2222);
    sir_node_t* k2   = sir_simd_const(&a, 0x1111, 0x2222);
    /* straight-line spine: each value stored to its own slot */
    sir_node_t* s9 = sir_store_local(&a, 9, SIR_DTV128, NULL, k2,   NULL);
    sir_node_t* s8 = sir_store_local(&a, 8, SIR_DTV128, NULL, k1,   s9);
    sir_node_t* s7 = sir_store_local(&a, 7, SIR_DTINT,  NULL, la3,  s8);
    sir_node_t* s6 = sir_store_local(&a, 6, SIR_DTINT,  NULL, la0,  s7);
    sir_node_t* s5 = sir_store_local(&a, 5, SIR_DTV128, NULL, sub,  s6);
    sir_node_t* s4 = sir_store_local(&a, 4, SIR_DTV128, NULL, add2, s5);
    sir_node_t* s3 = sir_store_local(&a, 3, SIR_DTV128, NULL, add1, s4);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 10, s3);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, add1);
    cp_vnode_t* v2 = cp_vnode_for(e, add2);
    cp_vnode_t* vs = cp_vnode_for(e, sub);
    TEST_ASSERT_NOT_NULL(v1); TEST_ASSERT_NOT_NULL(v2); TEST_ASSERT_NOT_NULL(vs);
    TEST_ASSERT_TRUE_MESSAGE(v1->partition == v2->partition,
        "two i32x4.adds over the same operands are ONE value (GVN merges)");
    TEST_ASSERT_TRUE_MESSAGE(v1->partition != vs->partition,
        "i32x4.sub over the SAME operands is NOT the add (op payload is identity)");
    cp_vnode_t* ve0 = cp_vnode_for(e, la0);
    cp_vnode_t* ve3 = cp_vnode_for(e, la3);
    TEST_ASSERT_NOT_NULL(ve0); TEST_ASSERT_NOT_NULL(ve3);
    TEST_ASSERT_TRUE_MESSAGE(ve0->partition != ve3->partition,
        "extract_lane 0 and 3 of the same vector are DIFFERENT values (lane is identity)");
    cp_vnode_t* vk1 = cp_vnode_for(e, k1);
    cp_vnode_t* vk2 = cp_vnode_for(e, k2);
    TEST_ASSERT_NOT_NULL(vk1); TEST_ASSERT_NOT_NULL(vk2);
    TEST_ASSERT_TRUE_MESSAGE(vk1->partition != vk2->partition,
        "identical SimdConsts stay SEPARATE (not congruent by design — the "
        "128-bit payload cannot ride an exact discriminator, and a leaf "
        "partition can never be split back)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.7 def-use uniformity across branch arms: two LoadLocal
 * reads of the same slot (neither slot ever overwritten between them)
 * in the arms of two consecutive branches must end up in the same
 * partition after refinement converges. Click's def-use walk doesn't
 * differentiate "branch-arm edge" from "straight-line edge"; if my
 * Dim 5 / §4.9 / §4.7.2 race: LL(s)@arm1 and LL(s)@arm2 must unify
 * into one partition when both arms' reaching def for slot s is the
 * seed. */
static void test_cp_branch_arm_load_locals_unify(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* Two back-to-back branches on the same cond. Each branch's arms
     * read slot 1 (and 2) into the result slot. Slot 1 and slot 2 are
     * never written; their reads in branch-2 arms must end up in the
     * same partition as the reads in branch-1 arms. */
    sir_node_t* ll_a   = sir_load_local(&a, 3, SIR_DTSHORT, NULL);
    sir_node_t* ll_b   = sir_load_local(&a, 4, SIR_DTSHORT, NULL);
    sir_node_t* add    = sir_add(&a, SIR_DTSHORT, ll_a, ll_b);
    sir_node_t* ret    = sir_return(&a, add, SIR_DTSHORT);
    sir_node_t* m2     = sir_nop(&a, ret);
    sir_node_t* ll_x2  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* ll_y2  = sir_load_local(&a, 2, SIR_DTSHORT, NULL);
    sir_node_t* st_t2  = sir_store_local(&a, 4, SIR_DTSHORT, NULL, ll_x2, m2);
    sir_node_t* st_f2  = sir_store_local(&a, 4, SIR_DTSHORT, NULL, ll_y2, m2);
    sir_node_t* cond2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br2    = sir_branch(&a, cond2, st_t2, st_f2);
    sir_node_t* m1     = sir_nop(&a, br2);
    sir_node_t* ll_x1  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* ll_y1  = sir_load_local(&a, 2, SIR_DTSHORT, NULL);
    sir_node_t* st_t1  = sir_store_local(&a, 3, SIR_DTSHORT, NULL, ll_x1, m1);
    sir_node_t* st_f1  = sir_store_local(&a, 3, SIR_DTSHORT, NULL, ll_y1, m1);
    sir_node_t* cond1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br1    = sir_branch(&a, cond1, st_t1, st_f1);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 5, br1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vx1 = cp_vnode_for(e, ll_x1);
    cp_vnode_t* vx2 = cp_vnode_for(e, ll_x2);
    cp_vnode_t* vy1 = cp_vnode_for(e, ll_y1);
    cp_vnode_t* vy2 = cp_vnode_for(e, ll_y2);
    TEST_ASSERT_NOT_NULL(vx1);
    TEST_ASSERT_NOT_NULL(vx2);
    TEST_ASSERT_NOT_NULL(vy1);
    TEST_ASSERT_NOT_NULL(vy2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(vx1->partition, vx2->partition,
        "LL(s1) in branch-1 arm and branch-2 arm must share partition "
        "(slot never written; reaching defs converge through trivial "
        "PHI Follower-of-seed_s1)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(vy1->partition, vy2->partition,
        "LL(s2) in branch-1 arm and branch-2 arm must share partition");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.7.2 Follower fixup on SPLIT: when a Leader moves from
 * partition X to a new partition X' during refinement, every Follower
 * whose Leader is now in X' must also move to X' — Followers live in
 * their Leader's partition by §4.7.1, so a Follower stranded in X
 * with a Leader in X' violates the invariant and the §4.10 rewrite
 * reads stale partition data.
 *
 * Fixture: a method body where partition refinement is forced to
 * SPLIT a Leader's bucket, with a Follower riding along. After
 * cp_build's refinement converges, the Follower must be in its
 * Leader's (post-split) partition. */
static void test_cp_split_follower_follows_leader(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* Two LOADLOCAL reads of the same slot (slot 0). After Dim 5,
     * both are §4.7 COPY Followers of slot 0's seed. Add a second
     * slot (slot 1) with its own LOADLOCAL pair (both Followers of
     * slot 1's seed). CAUSE_SPLITS will split the LOADLOCAL bucket
     * by reaching-def partition — Followers must follow. */
    sir_node_t* la1 = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb1 = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add1 = sir_add(&a, SIR_DTSHORT, la1, lb1);
    sir_node_t* ret = sir_return(&a, add1, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vla = cp_vnode_for(e, la1);
    cp_vnode_t* vlb = cp_vnode_for(e, lb1);
    TEST_ASSERT_NOT_NULL(vla);
    TEST_ASSERT_NOT_NULL(vlb);
    TEST_ASSERT_TRUE_MESSAGE(vla->leader >= 0,
        "LL(s0) must be a §4.7 COPY Follower of slot 0's seed");
    TEST_ASSERT_TRUE_MESSAGE(vlb->leader >= 0,
        "LL(s1) must be a §4.7 COPY Follower of slot 1's seed");
    /* After refinement, Follower's partition == its Leader's
     * partition. If cp_split's Follower-fixup is missing, the
     * Followers stay stranded in their initial opcode bucket while
     * their Leaders (the seeds) sit in singletons. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        e->vnodes[vla->leader]->partition, vla->partition,
        "LL(s0) Follower must share its Leader (seed_0)'s partition "
        "after cp_split moved them apart (§4.7.2 Follower fixup)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        e->vnodes[vlb->leader]->partition, vlb->partition,
        "LL(s1) Follower must share its Leader (seed_1)'s partition");
    /* And the two seeds (different slots) must be in DIFFERENT
     * partitions — they're independent opaques. */
    TEST_ASSERT_TRUE_MESSAGE(
        e->vnodes[vla->leader]->partition != e->vnodes[vlb->leader]->partition,
        "seed_0 and seed_1 must be in different partitions (independent opaques)");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.7.1/§4.7.4 Follower-reachability invariant: every Follower is reachable from
 * its Leader through EDGES — the du window (every input-linked kind: the leader IS an
 * input) or the discovered-edge overflow (LOAD/ARRLEN, §4.7.4's other.def_use) — because
 * the edges ARE the notification structure now (Fig 4.7 lines 10-11 + the overflow):
 * a follower not edge-reachable from its leader keeps a stale fact forever. Conversely,
 * every LIVE overflow entry's user follows that segment's owner (a revert must remove
 * its entry — §4.7.5 line 6.4's re-segregation). Fixture: parallel inductive
 * recurrences, which exercise the §4.9 apply + revert sweeps inside cp_build's outer
 * fixpoint. */
static void test_cp_follower_list_invariant(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* Two parallel recurrences i, j (matches test_cp_refine_parallel_
     * recurrences). The header PHIs fire §4.9 transitions during the
     * fixpoint; if any apply/revert path skips cp_follower_link /
     * cp_follower_unlink the chains end up inconsistent. */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* lci    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* li     = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* addi   = sir_add(&a, SIR_DTSHORT, li, lci);
    sir_node_t* lcj    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* lj     = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* addj   = sir_add(&a, SIR_DTSHORT, lj, lcj);
    sir_node_t* body_j = sir_store_local(&a, 1, SIR_DTSHORT, NULL, addj, header);
    sir_node_t* body_i = sir_store_local(&a, 0, SIR_DTSHORT, NULL, addi, body_j);
    sir_set_next(header, body_i);
    sir_node_t* lc1j   = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* st_j   = sir_store_local(&a, 1, SIR_DTSHORT, NULL, lc1j, header);
    sir_node_t* lc1i   = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* st_i   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, lc1i, st_j);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 2, st_i);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    /* Forward direction: every Follower must be EDGE-reachable from its Leader — as a
     * du user of it (input-linked kinds), or through the discovered-edge overflow. */
    for (int v = 0; v < e->vnode_count; v++) {
        int leader = e->vnodes[v]->leader;
        if (leader < 0) continue;
        bool found = false;
        for (int k = e->du_off[leader];
             !found && k < e->du_off[leader] + e->du_cnt[leader]; k++)
            if (e->du_user[k] == v) found = true;
        if (!found && e->du_ov_head)
            for (int o = e->du_ov_head[leader]; !found && o >= 0; o = e->du_ov_next[o])
                if (e->du_ov_user[o] == v) found = true;
        char msg[160];
        snprintf(msg, sizeof(msg),
            "vnode %d with leader=%d must be edge-reachable from it (du window or "
            "overflow) — an unreachable follower's fact goes stale forever",
            v, leader);
        TEST_ASSERT_TRUE_MESSAGE(found, msg);
    }
    /* Reverse direction: every LIVE overflow entry's user still follows that segment's
     * owner — a revert must have removed its entry (line 6.4's re-segregation). */
    if (e->du_ov_head)
        for (int leader = 0; leader < e->vnode_count; leader++)
            for (int o = e->du_ov_head[leader]; o >= 0; o = e->du_ov_next[o]) {
                int u = e->du_ov_user[o];
                if (u < 0) continue;                      /* removed at revert */
                char msg[120];
                snprintf(msg, sizeof(msg),
                    "live overflow entry %d under vnode %d but user %d's leader=%d",
                    o, leader, u, e->vnodes[u]->leader);
                TEST_ASSERT_EQUAL_INT_MESSAGE(leader, e->vnodes[u]->leader, msg);
            }

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.4.2: one-time fact init. cp_init_facts seeds every vnode
 * with type=TOP, constant=CP_C_TOP, and enqueues every vnode onto
 * its partition's cprop. The subsequent solve loop drains from there.
 *
 * Input contract: cp_build_no_solve has run — partitions assigned,
 * vnodes allocated, but no facts initialized (vnode.type == NULL).
 * Output contract: every vnode has type == TOP and constant == TOP;
 * eng->cprop_worklist non-empty (every populated partition seeded);
 * every vnode is on its partition's cprop list. */
static void test_cp_init_facts_contract(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* lc = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ret = sir_return(&a, lc, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build_no_solve(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    /* Input precondition: facts uninitialized. */
    for (int v = 0; v < e->vnode_count; v++) {
        TEST_ASSERT_NULL_MESSAGE(e->vnodes[v]->type,
            "pre-init: every vnode's type must be NULL/unset");
    }

    cp_init_facts(e);

    /* Output postcondition. */
    const Type* top = type_top(&e->pool);
    for (int v = 0; v < e->vnode_count; v++) {
        TEST_ASSERT_EQUAL_PTR_MESSAGE(top, e->vnodes[v]->type,
            "post-init: every vnode's type must be TOP");
        TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_TOP, e->vnodes[v]->constant.state,
            "post-init: every vnode's constant.state must be TOP");
        TEST_ASSERT_TRUE_MESSAGE(e->vnodes[v]->in_cprop,
            "post-init: every vnode must be on its partition's cprop");
    }
    TEST_ASSERT_TRUE_MESSAGE(bbq_vec_len(e->cprop_worklist) > 0,
        "post-init: cprop_worklist must be non-empty");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.7.5 PROPAGATE drain contract.
 *
 * Input contract: partition X with non-empty X.cprop (set up by
 * cp_init_facts); vnodes carry their pre-drain types/constants.
 * Output contract: after cp_compute_facts returns,
 *   (a) eng->cprop_worklist is empty;
 *   (b) every populated partition's X.cprop_head is -1;
 *   (c) for every vnode v, v->type equals cp_node_type(v) given its
 *       current inputs (fixpoint).
 *
 * Without per-partition drain, (a)/(b) would leave residual entries
 * after the first pop; with the drain, the chain empties. Failure
 * mode (c) catches when fact propagation halts before fixpoint — e.g.,
 * a Cmp(KNOWN 5, KNOWN 3) whose stored constant remained TOP because
 * its inputs were processed AFTER it in worklist order without a
 * re-enqueue cascade. */
static void test_cp_propagate_drain_contract(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* lc5 = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* lc3 = sir_load_const(&a, 3, SIR_DTSHORT);
    sir_node_t* cmp = sir_gt(&a, lc5, lc3);
    sir_node_t* ret = sir_return(&a, cmp, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    /* Output (a): cprop_worklist empty after solve converged. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)bbq_vec_len(e->cprop_worklist),
        "cprop_worklist must be empty after PROPAGATE drains");

    /* Output (b): every partition's local cprop is empty. */
    for (int p = 0; p < e->partition_count; p++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(-1, e->partitions[p]->cprop_head,
            "partition's X.cprop_head must be -1 post-drain");
    }

    /* Output (c): facts at fixpoint. LoadConst(5) carries KNOWN 5,
     * LoadConst(3) carries KNOWN 3, Cmp(5,3,GT) folds to KNOWN 1. */
    cp_vnode_t* v_lc5 = cp_vnode_for(e, lc5);
    cp_vnode_t* v_lc3 = cp_vnode_for(e, lc3);
    cp_vnode_t* v_cmp = cp_vnode_for(e, cmp);
    TEST_ASSERT_NOT_NULL(v_lc5);
    TEST_ASSERT_NOT_NULL(v_lc3);
    TEST_ASSERT_NOT_NULL(v_cmp);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v_lc5->constant.state,
        "LoadConst(5).constant must be KNOWN post-drain");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, v_lc5->constant.value,
        "LoadConst(5).constant.value must be 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v_lc3->constant.state,
        "LoadConst(3).constant must be KNOWN post-drain");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, v_lc3->constant.value,
        "LoadConst(3).constant.value must be 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v_cmp->constant.state,
        "Cmp(5,3,GT).constant must be KNOWN post-drain (fact propagated)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, v_cmp->constant.value,
        "Cmp(5>3) folds to 1");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.4.2 outer loop convergence contract.
 *
 * Input contract: a method whose IR exercises every cross-worklist
 * enqueue path — cprop seed at init, fact-fall enqueue users,
 * §4.7.5 line 33-34 CAUSE_SPLITS→cprop, §4.8 line 16.5 apply→worklist.
 * Output contract: cp_solve terminates (no infinite loop); after
 * termination both eng->cprop_worklist and eng->worklist are empty
 * (per §4.4.2: "Do until worklist and cprop are empty").
 *
 * Fixture: a small IR with a branch (CAUSE_SPLITS work), a constant
 * (cprop work), and a trivial PHI at the merge (§4.9 apply, triggering
 * the apply→worklist cross-enqueue). All three worklist drivers
 * exercised; both must drain to empty at convergence. */
static void test_cp_solve_worklists_drain(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_val = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret = sir_return(&a, ret_val, SIR_DTSHORT);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* t_arm = sir_nop(&a, merge);
    sir_node_t* f_arm = sir_nop(&a, merge);
    sir_node_t* cond = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br = sir_branch(&a, cond, t_arm, f_arm);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)bbq_vec_len(e->cprop_worklist),
        "cp_solve must terminate with cprop_worklist empty");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)bbq_vec_len(e->worklist),
        "cp_solve must terminate with eng->worklist empty");

    cp_free(e);
    bbq_arena_free(&a);
}

/* A loop-header φ's solve must be AT a fixpoint, which is stronger than the drained worklists
 * asserted above: a transfer reading a fact that reaches it by no def-use edge leaves no edge
 * to be pending, so the worklists drain while the value is stale.
 *
 * Click §4.9 (printed p.62) makes a φ a Follower when all its LIVE inputs are congruent,
 * testing liveness as `(*region)[i]→type = Type_Reach`. While the back edge is unreachable the
 * live set is the entry seed alone and the φ follows it; when the loop test settles and the
 * back edge opens the premise breaks, with the header reachable throughout. */
static void test_cp_loop_phi_reaches_fixpoint(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* header: loop merge. body stores st+1 back to slot 0 and jumps to header.
     * The exit test compares slot 0 against an opaque bound, so its condition is not a
     * literal — it settles to BOTTOM only after the seed's fact falls. */
    sir_node_t* header = sir_nop(&a, NULL);                      /* patched below */
    sir_node_t* ret_v  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret    = sir_return(&a, ret_v, SIR_DTSHORT);
    sir_node_t* st     = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* one    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* inc    = sir_add(&a, SIR_DTSHORT, st, one);
    sir_node_t* body   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, inc, header);
    sir_node_t* cnd    = sir_load_local(&a, 1, SIR_DTSHORT, NULL);   /* opaque bound */
    sir_node_t* br     = sir_branch(&a, cnd, body, ret);
    sir_set_next(header, br);
    sir_node_t* seed   = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
                                         sir_load_const(&a, 0, SIR_DTSHORT), header);
    sir_method_t* m    = sir_method(&a, "trimlike", 0, 0, 2, seed);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(cp_at_fixpoint(e),
        "cp_solve must return AT a fixpoint: a loop-header phi whose back edge opens as the "
        "loop test settles must have been re-armed (Click §4.9 reads liveness as the region "
        "input's Type_Reach, i.e. a def-use input)");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Same property for the identity-Follower path: a LoadLocal following a store holds its fact
 * through the leader LINK, not a def-use edge, so a descending leader constant
 * (CP_C_REF → CP_C_BOTTOM once an aliasing store is proven to reach it) must still recompute
 * the follower's stored fact. */
static void test_cp_load_follower_reaches_fixpoint(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* merge  = sir_nop(&a, NULL);
    sir_node_t* r_v    = sir_load_local(&a, 0, SIR_DTREF, NULL);
    sir_node_t* ret    = sir_return(&a, r_v, SIR_DTREF);
    sir_set_next(merge, ret);
    /* two arms storing DIFFERENT refs into slot 0, so the merge phi's fact must fall */
    sir_node_t* a0     = sir_store_local(&a, 0, SIR_DTREF, NULL,
                                         sir_load_local(&a, 1, SIR_DTREF, NULL), merge);
    sir_node_t* a1     = sir_store_local(&a, 0, SIR_DTREF, NULL,
                                         sir_load_local(&a, 2, SIR_DTREF, NULL), merge);
    sir_node_t* cnd    = sir_load_local(&a, 3, SIR_DTSHORT, NULL);
    sir_node_t* br     = sir_branch(&a, cnd, a0, a1);
    sir_method_t* m    = sir_method(&a, "trimtosizelike", 0, 0, 4, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(cp_at_fixpoint(e),
        "cp_solve must return AT a fixpoint: a load Follower whose Leader's constant descends "
        "must be recomputed (the leader LINK is not a def-use edge)");

    cp_free(e);
    bbq_arena_free(&a);
}

/* W5 T2 — the in-drain transitions TERMINATE on the oscillation shape, because of Click
 * §4.7.1: "Nodes can make the Follower ⇒ Leader transition only once."
 *
 * `sum = 0; while (?) { sum = sum + i; i = i + 1; }` — while the seed's KNOWN 0 is the
 * only contributor, Add(sum, i) is a §4.8 identity on i and becomes its Follower; when
 * the back edge opens and sum's φ falls to BOTTOM the premise breaks and it reverts.
 * With the applies running at the dequeue (Fig 4.7 lines 17-21 + the operand-side
 * attempt), a node whose premise oscillates mid-solve re-forms at every dequeue — the
 * loop keeps it enqueued — and the outer drain never empties (SEEN: the 1M-iteration
 * guard abort, |cprop_wl|=57, pairs of φs pending in shared partitions). One-way is the
 * paper's own termination argument for this placement, not merely its O(n) cost bound.
 * The loop also feeds a SUB of two moving partitions, so the §4.6 lines 33-34
 * cross-enqueue (CAUSE_SPLITS → cprop) runs on a loop-heavy method and must converge —
 * NOT to be confused with cp_refine's whole-partition re-seeding, a different operation.
 *
 * COVERAGE, not a falsifier: this shape's identity premise (KNOWN 0 operand) descends
 * monotonically, so it terminates even without the gate — verified by running it with
 * the gate disabled. The gate's falsifier is SUITE-level: disabling f2l_once aborts
 * test_sir's jre-derived methods on the convergence guard (run 07-30, twice). */
static void test_cp_in_drain_transitions_terminate(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);                      /* patched below */
    sir_node_t* ret_v  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret    = sir_return(&a, ret_v, SIR_DTSHORT);
    /* body: d = sum - i; sum = sum + i; i = i + 1 */
    sir_node_t* sum    = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* iv     = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add    = sir_add(&a, SIR_DTSHORT, sum, iv);
    sir_node_t* sub    = sir_sub(&a, SIR_DTSHORT,
                                 sir_load_local(&a, 0, SIR_DTSHORT, NULL),
                                 sir_load_local(&a, 1, SIR_DTSHORT, NULL));
    sir_node_t* inc    = sir_add(&a, SIR_DTSHORT,
                                 sir_load_local(&a, 1, SIR_DTSHORT, NULL),
                                 sir_load_const(&a, 1, SIR_DTSHORT));
    sir_node_t* st_i   = sir_store_local(&a, 1, SIR_DTSHORT, NULL, inc, header);
    sir_node_t* st_s   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, add, st_i);
    sir_node_t* st_d   = sir_store_local(&a, 3, SIR_DTSHORT, NULL, sub, st_s);
    sir_node_t* cnd    = sir_load_local(&a, 2, SIR_DTSHORT, NULL);   /* opaque bound */
    sir_node_t* br     = sir_branch(&a, cnd, st_d, ret);
    sir_set_next(header, br);
    sir_node_t* seed   = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
                                         sir_load_const(&a, 0, SIR_DTSHORT), header);
    sir_method_t* m    = sir_method(&a, "sumloop", 0, 0, 4, seed);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);   /* non-termination = abort here */
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)bbq_vec_len(e->cprop_worklist),
        "§4.4.2: the solve exits on worklist emptiness — cprop drained");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)bbq_vec_len(e->worklist),
        "§4.4.2: the solve exits on worklist emptiness — splitter drained");
    TEST_ASSERT_TRUE_MESSAGE(cp_at_fixpoint(e),
        "the oscillated identity's final state is a true fixpoint (burned Leader, "
        "facts = f(inputs))");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The φ-PAIR shape from the jre livelock's dump (pairs of PHIs pending in shared
 * partitions when the 1M guard tripped): two loop-carried slots whose header φs are
 * each other's back-edge contributor. Each φ's §4.9 premise ("all live inputs in one
 * partition") is NON-monotone — the peer's apply re-forms it, the peer's revert
 * re-breaks it. COVERAGE: this two-φ distillation terminates even with the §4.7.1 gate
 * disabled (verified by running it so) — the jre cycle needs more context than the
 * bare pair — but the pair premise-chasing is the mechanism the dump named, so the
 * shape stays pinned for termination + fixpoint. */
static void test_cp_phi_pair_terminates(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);                      /* patched below */
    sir_node_t* ret_v  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret    = sir_return(&a, ret_v, SIR_DTSHORT);
    /* body: t = a; a = b; b = t (swap) */
    sir_node_t* la     = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb     = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* st_b   = sir_store_local(&a, 1, SIR_DTSHORT, NULL, la, header);
    sir_node_t* st_a   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, lb, st_b);
    sir_node_t* cnd    = sir_load_local(&a, 2, SIR_DTSHORT, NULL);   /* opaque bound */
    sir_node_t* br     = sir_branch(&a, cnd, st_a, ret);
    sir_set_next(header, br);
    /* seeds from two DIFFERENT params, so the pair's congruence is transient */
    sir_node_t* st1    = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
                                         sir_load_local(&a, 4, SIR_DTSHORT, NULL), header);
    sir_node_t* st0    = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
                                         sir_load_local(&a, 3, SIR_DTSHORT, NULL), st1);
    sir_method_t* m    = sir_method(&a, "swaploop", 0, 0, 5, st0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);   /* non-termination = abort here */
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(cp_at_fixpoint(e),
        "the phi pair settles: burned or premise-true Followers, facts = f(inputs)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* W6-Q1 pin — TWO reverted identity Followers of the same Leader, different opcodes,
 * must not stay claimed-congruent to each other. §4.7.5 line 6.3 keeps a reverted node
 * in the partition as a Leader, so Leaders can mix opcodes; §4.8 p.60 puts the revert
 * in `fallen` because it lacks "the same type OR OPCODE as the other Leaders", and
 * SPLIT_BY's OPCODE level (p.47's cascade) is what separates the mix. A LONE revert is
 * already carved out by line 15's SPLIT (the other Leaders didn't fall in that drain —
 * verified by running the lone shape with the opcode level disabled: still separates);
 * the exposure is a PAIR reverting in the SAME drain: both land in the fallen suffix,
 * SPLIT carves them into one Y together, and with equal BOTTOM facts only the opcode
 * key tells an ADD from a SUB — `x + k ≡ x - k` with k unknown is a miscompile.
 *
 * Shape: x opaque (slot 0, never stored); k loop-carried (slot 1), seeded 0, back edge
 * stores an opaque q. After the loop, y1 = x + k and y2 = x - k. While the back edge
 * is unlit, k is KNOWN 0 and both are §4.8 identities on x (ADD either-side 0; SUB
 * right-side 0), Followers toward the slot-0 seed's partition. When the back edge
 * opens, k falls and both revert.
 *
 * COVERAGE, not a falsifier: run with the opcode level disabled (forced §4.5
 * exemption, 07-30), this still passes — the traced runs show the pair reverting in
 * DIFFERENT partitions (line-7 fact refresh + per-drain line-15 carves separate them
 * before SPLIT_BY ever sees them together). The opcode key is the paper's own backstop
 * (p.47's cascade; §4.8 p.60's "same type or opcode") for orderings no construction
 * reached; this pin holds the CLASS invariant so any future dynamics that do strand
 * the pair go red here. */
static void test_cp_reverted_identity_pair_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);                      /* patched below */
    sir_node_t* y1     = sir_add(&a, SIR_DTSHORT,
                                 sir_load_local(&a, 0, SIR_DTSHORT, NULL),
                                 sir_load_local(&a, 1, SIR_DTSHORT, NULL));
    sir_node_t* y2     = sir_sub(&a, SIR_DTSHORT,
                                 sir_load_local(&a, 0, SIR_DTSHORT, NULL),
                                 sir_load_local(&a, 1, SIR_DTSHORT, NULL));
    sir_node_t* ret    = sir_return(&a, sir_add(&a, SIR_DTSHORT, y1, y2), SIR_DTSHORT);
    sir_node_t* body   = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
                                         sir_load_local(&a, 3, SIR_DTSHORT, NULL), header);
    sir_node_t* cnd    = sir_load_local(&a, 2, SIR_DTSHORT, NULL);   /* opaque bound */
    sir_node_t* br     = sir_branch(&a, cnd, body, ret);
    sir_set_next(header, br);
    sir_node_t* seed   = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
                                         sir_load_const(&a, 0, SIR_DTSHORT), header);
    sir_method_t* m    = sir_method(&a, "revertmix", 0, 0, 4, seed);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    int p1 = -1, p2 = -1;
    for (int i = 0; i < e->vnode_count; i++) {
        cp_vnode_t* v = e->vnodes[i];
        if (v->kind != CP_VN_EXPR || !v->expr) continue;
        if (v->expr == y1) p1 = i;
        if (v->expr == y2) p2 = i;
    }
    TEST_ASSERT_TRUE_MESSAGE(p1 >= 0 && p2 >= 0, "y1 and y2 have vnodes");
    TEST_ASSERT_TRUE_MESSAGE(e->vnodes[p1]->leader == -1 && e->vnodes[p2]->leader == -1,
        "the identity premises broke (k fell to BOTTOM) — both reverted to Leader");
    TEST_ASSERT_TRUE_MESSAGE(
        e->vnodes[p1]->partition != e->vnodes[p2]->partition,
        "a reverted x+k is NOT a reverted x-k: equal BOTTOM facts must not keep "
        "different opcodes congruent (SPLIT_BY's opcode level, p.47)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* W5 T3 — cp_at_fixpoint's families over a MEMORY shape: an allocation, two stores on
 * diverging arms, a load at the merge (a cell-φ). With SPLIT/SPLIT_BY, the Follower
 * transitions AND the edge marking all inside the drain, "worklists empty" is the whole
 * exit — this asserts the heap/pts families really are f(inputs) there, not kept correct
 * by a deleted round pass re-running them. */
static void test_cp_memory_shape_reaches_fixpoint(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ret   = sir_return(&a, sir_get_field(&a, SIR_DTINT,
                                        sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0),
                                    SIR_DTINT);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* s2    = sir_put_field(&a, SIR_DTINT,
                                      sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0,
                                      sir_load_const(&a, 2, SIR_DTINT), merge);
    sir_node_t* s1    = sir_put_field(&a, SIR_DTINT,
                                      sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0,
                                      sir_load_const(&a, 1, SIR_DTINT), merge);
    sir_node_t* cnd   = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* br    = sir_branch(&a, cnd, s1, s2);
    sir_node_t* st0   = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 7), br);
    sir_method_t* m   = sir_method(&a, "memdiamond", 0, 0, 2, st0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(cp_at_fixpoint(e),
        "heap/pts are fixpoint families like any other: the merge's cell-phi and the "
        "load reading it must be f(inputs) when the worklists empty");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.6 SUB/CMP-of-congruent fold.
 *
 * Input contract: SUB(a, b) or CMP(a, b) where a.partition == b.partition
 * (their ultimate reaching defs are equal under Click's congruence
 * relation). a's and b's individual constant states may be anything.
 * Output contract: SUB folds to KNOWN 0; CMP folds based on op
 * (EQ/LE/GE → 1, NE/LT/GT → 0). The fold is partition-aware, not
 * value-aware — applies even when the inputs aren't KNOWN constants.
 *
 * Fixture: two LoadLocals reading the SAME slot (under Dim 5, both
 * become Followers of the same seed, so partition-congruent). Build
 * SUB(LL(0), LL(0)) and CMP(LL(0), LL(0)) with op=EQ. After solve,
 * SUB.constant = KNOWN 0, CMP.constant = KNOWN 1. */
static void test_cp_sub_of_congruent_folds_to_zero(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* sub = sir_sub(&a, SIR_DTSHORT, la, lb);
    sir_node_t* ret = sir_return(&a, sub, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vsub = cp_vnode_for(e, sub);
    TEST_ASSERT_NOT_NULL(vsub);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, vsub->constant.state,
        "SUB(LL(0), LL(0)) must fold to KNOWN 0 (§4.6 sub-of-congruent)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, vsub->constant.value,
        "SUB(x, x).value == 0");
    cp_free(e);
    bbq_arena_free(&a);
}

static void test_cp_cmp_eq_of_congruent_folds_to_one(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* cmp = sir_eq(&a, la, lb);
    sir_node_t* ret = sir_return(&a, cmp, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vcmp = cp_vnode_for(e, cmp);
    TEST_ASSERT_NOT_NULL(vcmp);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, vcmp->constant.state,
        "CMP(LL(0), LL(0)) EQ must fold to KNOWN 1 (§4.6 cmp-of-congruent)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, vcmp->constant.value,
        "CMP(x, x, EQ).value == 1");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Click §4.3.2 invariant: after PROPAGATE converges, every member of
 * a partition has the same type. The §4.7.5 per-partition PROPAGATE
 * algorithm maintains this — within a partition, fact propagation
 * drains X.cprop completely before SPLIT separates fallen members.
 * Global batched PROPAGATE (compute all facts, then SPLIT_BY) can
 * break this between iterations when Follower transitions are
 * deferred. Property test: walk every partition, every member, assert
 * type matches the first member's. Fixture: peer-PHI shape from the
 * structural test. */
static void test_cp_partition_type_invariant(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll_a   = sir_load_local(&a, 3, SIR_DTSHORT, NULL);
    sir_node_t* ll_b   = sir_load_local(&a, 4, SIR_DTSHORT, NULL);
    sir_node_t* add    = sir_add(&a, SIR_DTSHORT, ll_a, ll_b);
    sir_node_t* ret    = sir_return(&a, add, SIR_DTSHORT);
    sir_node_t* m2     = sir_nop(&a, ret);
    sir_node_t* ll_x2  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* ll_y2  = sir_load_local(&a, 2, SIR_DTSHORT, NULL);
    sir_node_t* st_t2  = sir_store_local(&a, 4, SIR_DTSHORT, NULL, ll_x2, m2);
    sir_node_t* st_f2  = sir_store_local(&a, 4, SIR_DTSHORT, NULL, ll_y2, m2);
    sir_node_t* cond2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br2    = sir_branch(&a, cond2, st_t2, st_f2);
    sir_node_t* m1     = sir_nop(&a, br2);
    sir_node_t* ll_x1  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* ll_y1  = sir_load_local(&a, 2, SIR_DTSHORT, NULL);
    sir_node_t* st_t1  = sir_store_local(&a, 3, SIR_DTSHORT, NULL, ll_x1, m1);
    sir_node_t* st_f1  = sir_store_local(&a, 3, SIR_DTSHORT, NULL, ll_y1, m1);
    sir_node_t* cond1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br1    = sir_branch(&a, cond1, st_t1, st_f1);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 5, br1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);

    for (int p = 0; p < e->partition_count; p++) {
        const Type* part_type = NULL;
        bool seen = false;
        for (int v = e->partitions[p]->head; v >= 0;
             v = e->vnodes[v]->part_next) {
            const Type* t = e->vnodes[v]->type;
            if (!seen) { part_type = t; seen = true; continue; }
            char msg[160];
            snprintf(msg, sizeof(msg),
                "partition %d member v%d has type that differs from "
                "the first member's — §4.3.2 invariant violated",
                p, v);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(part_type, t, msg);
        }
    }

    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.2: two structurally identical pure computations on congruent
 * inputs refine into a single congruence class. */
static void test_cp_refine_merges_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb1  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add1 = sir_add(&a, SIR_DTSHORT, la1, lb1);
    sir_node_t* la2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb2  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add2 = sir_add(&a, SIR_DTSHORT, la2, lb2);
    sir_node_t* ret  = sir_return_void(&a);
    sir_node_t* ee2  = sir_expr_effect(&a, add2, 1, ret);
    sir_node_t* ee1  = sir_expr_effect(&a, add1, 1, ee2);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, ee1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, add1);
    cp_vnode_t* v2 = cp_vnode_for(e, add2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v1->partition, v2->partition,
        "two computations of a + b must refine to one class");

    cp_free(e);
    bbq_arena_free(&a);
}

/* Refinement separates computations that differ in an input: a + b
 * and a + c share their left operand but not their right. */
static void test_cp_refine_separates_noncongruent(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb   = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add1 = sir_add(&a, SIR_DTSHORT, la1, lb);
    sir_node_t* la2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lc   = sir_load_local(&a, 2, SIR_DTSHORT, NULL);
    sir_node_t* add2 = sir_add(&a, SIR_DTSHORT, la2, lc);
    sir_node_t* ret  = sir_return_void(&a);
    sir_node_t* ee2  = sir_expr_effect(&a, add2, 1, ret);
    sir_node_t* ee1  = sir_expr_effect(&a, add1, 1, ee2);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, ee1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, add1);
    cp_vnode_t* v2 = cp_vnode_for(e, add2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v1->partition != v2->partition,
        "a + b and a + c must stay in distinct classes");

    cp_free(e);
    bbq_arena_free(&a);
}

/* §0 contract: on an inductive loop (i = i + 1), refinement
 * terminates — the test reaching this point proves it — and stays
 * within the n-1 split bound (partitions never exceed nodes). */
static void test_cp_refine_loop_terminates(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* lc1    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* li     = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* addi   = sir_add(&a, SIR_DTSHORT, li, lc1);
    sir_node_t* body   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, addi, header);
    sir_set_next(header, body);
    sir_node_t* entry  = sir_nop(&a, header);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 1, entry);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(e->partition_count <= e->vnode_count,
        "partition count must stay within the n-1 split bound");

    cp_free(e);
    bbq_arena_free(&a);
}

/* The flagship: two parallel recurrences — i and j each start at 1
 * and increment by 1 every iteration — are congruent. Optimistic
 * refinement keeps the circular congruence (φ_i ≡ φ_j because their
 * increments are congruent because the φs are) that a bottom-up
 * value-numbering cannot establish. */
static void test_cp_refine_parallel_recurrences(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* lci    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* li     = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* addi   = sir_add(&a, SIR_DTSHORT, li, lci);
    sir_node_t* lcj    = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* lj     = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* addj   = sir_add(&a, SIR_DTSHORT, lj, lcj);
    sir_node_t* body_j = sir_store_local(&a, 1, SIR_DTSHORT, NULL, addj, header);
    sir_node_t* body_i = sir_store_local(&a, 0, SIR_DTSHORT, NULL, addi, body_j);
    sir_set_next(header, body_i);
    sir_node_t* lc1j   = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* st_j   = sir_store_local(&a, 1, SIR_DTSHORT, NULL, lc1j, header);
    sir_node_t* lc1i   = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* st_i   = sir_store_local(&a, 0, SIR_DTSHORT, NULL, lc1i, st_j);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 2, st_i);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* phi_i = cp_find_phi(e, header, 0);
    cp_vnode_t* phi_j = cp_find_phi(e, header, 1);
    TEST_ASSERT_NOT_NULL(phi_i);
    TEST_ASSERT_NOT_NULL(phi_j);
    TEST_ASSERT_EQUAL_INT_MESSAGE(phi_i->partition, phi_j->partition,
        "parallel recurrences i and j must refine to one class");

    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.4 CCP: constants fold through the value graph — 2 + 3 yields a
 * value node carrying the known constant 5. */
static void test_cp_constant_folds(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* c3  = sir_load_const(&a, 3, SIR_DTSHORT);
    sir_node_t* add = sir_add(&a, SIR_DTSHORT, c2, c3);
    sir_node_t* ret = sir_return(&a, add, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vadd = cp_vnode_for(e, add);
    TEST_ASSERT_NOT_NULL(vadd);
    TEST_ASSERT_EQUAL_INT(CP_C_KNOWN, vadd->constant.state);
    TEST_ASSERT_EQUAL_INT(5, vadd->constant.value);

    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.6: x − x folds to 0 — even when x is a non-constant value —
 * because the two operands refine into one congruence class. This
 * needs the interleaved solve: it is congruence (a partition fact)
 * feeding the constant fact. */
static void test_cp_sub_of_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* l1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* l2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* sub = sir_sub(&a, SIR_DTSHORT, l1, l2);
    sir_node_t* ret = sir_return(&a, sub, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vsub = cp_vnode_for(e, sub);
    TEST_ASSERT_NOT_NULL(vsub);
    TEST_ASSERT_EQUAL_INT(CP_C_KNOWN, vsub->constant.state);
    TEST_ASSERT_EQUAL_INT(0, vsub->constant.value);

    cp_free(e);
    bbq_arena_free(&a);
}

/* §3.7 combined UCE + CCP: a Branch with a constant-false condition
 * makes its true arm unreachable, so the φ at the join ignores that
 * contributor and merges to the live arm's constant. Each fact here
 * is derivable only with the other — the combined-analysis win. */
static void test_cp_phi_skips_dead_arm(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ld   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret  = sir_return(&a, ld, SIR_DTSHORT);
    sir_node_t* join = sir_nop(&a, ret);
    sir_node_t* v7   = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* v5   = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* st_t = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v7, join);
    sir_node_t* st_f = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v5, join);
    sir_node_t* cond = sir_load_const(&a, 0, SIR_DTSHORT);   /* false */
    sir_node_t* br   = sir_branch(&a, cond, st_t, st_f);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* phi = cp_find_phi(e, join, 0);
    TEST_ASSERT_NOT_NULL(phi);
    TEST_ASSERT_EQUAL_INT(CP_C_KNOWN, phi->constant.state);
    TEST_ASSERT_EQUAL_INT(5, phi->constant.value);

    cp_free(e);
    bbq_arena_free(&a);
}

/* Loop counter PHI must NOT collapse to KNOWN-init when the back-edge
 * contributor is OPAQUE. Click's optimistic algorithm starts with the
 * back-edge predecessor unreachable; PHI's meet would be just the init
 * (KNOWN 0). But by fixpoint, the loop body must be proven reachable
 * (via the non-constant loop condition), the OPAQUE back-edge
 * contributor joins, and meet(KNOWN 0, BOTTOM) = BOTTOM.
 *
 * If PHI(i) stays KNOWN 0 at convergence, cp_rewrite folds every
 * LoadLocal(i) to LoadConst(0) and the loop counter init is dropped
 * — leaving a SIR_INC on a slot with no SHORT StoreLocal defining
 * it, i.e. a read of a slot before it is defined, which a validator
 * would reject as uninitialized. */
static void test_cp_loop_counter_phi_not_constant(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* Build:
     *   StoreLocal(i=0) -> Goto -> loop_check (merge nop) ->
     *   Branch(LoadLocal(i) < LoadLocal(n)) true->body false->exit_ret
     *   body: Inc(i, +1) -> Goto(loop_check)
     *   exit_ret: Return(LoadLocal(n))
     *
     * Slot 0 = n (param, opaque), slot 1 = i (loop counter). */
    sir_node_t* exit_ret = sir_return(&a,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* loop_check = sir_nop(&a, NULL);
    /* Branch's true arm: Inc whose .next is the back-edge to loop_check. */
    sir_node_t* inc = sir_inc(&a, 1, 1, SIR_DTSHORT,
        sir_load_local(&a, 1, SIR_DTSHORT, NULL), loop_check);
    /* Branch condition: i < n. */
    sir_node_t* cmp = sir_lt(&a,
        sir_load_local(&a, 1, SIR_DTSHORT, NULL),
        sir_load_local(&a, 0, SIR_DTSHORT, NULL));
    sir_node_t* branch = sir_branch(&a, cmp, inc, exit_ret);
    sir_set_next(loop_check, branch);
    /* Entry chain: store i=0, then fall through to loop_check. */
    sir_node_t* init = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_load_const(&a, 0, SIR_DTSHORT), loop_check);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, init);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* phi = cp_find_phi(e, loop_check, 1);
    TEST_ASSERT_NOT_NULL_MESSAGE(phi,
        "loop_check merge must have a φ for slot 1 (the loop counter)");
    TEST_ASSERT_TRUE_MESSAGE(phi->constant.state != CP_C_KNOWN,
        "loop counter PHI must NOT be KNOWN — the OPAQUE back-edge "
        "contributor makes meet(KNOWN 0, BOTTOM) = BOTTOM. KNOWN here "
        "means cp_phi_input_live filtered out the back-edge whose "
        "predecessor never became reachable.");

    cp_free(e);
    bbq_arena_free(&a);
}

/* UCE through a Switch: a known selector matching a case makes the
 * default arm unreachable, so the φ at the join ignores it. */
static void test_cp_switch_prunes_default(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ld   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret  = sir_return(&a, ld, SIR_DTSHORT);
    sir_node_t* join = sir_nop(&a, ret);
    sir_node_t* v9   = sir_load_const(&a, 9, SIR_DTSHORT);
    sir_node_t* v4   = sir_load_const(&a, 4, SIR_DTSHORT);
    sir_node_t* st_c = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v9, join);
    sir_node_t* st_d = sir_store_local(&a, 0, SIR_DTSHORT, NULL, v4, join);
    sir_node_t* sel  = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* cases[1] = { st_c };
    int32_t     vals[1]  = { 1 };
    sir_node_t* sw   = sir_switch(&a, sel, cases, 1, vals, 1, st_d,
                                  SIR_DTSHORT);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, sw);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* phi = cp_find_phi(e, join, 0);
    TEST_ASSERT_NOT_NULL(phi);
    TEST_ASSERT_EQUAL_INT(CP_C_KNOWN, phi->constant.state);
    TEST_ASSERT_EQUAL_INT(9, phi->constant.value);

    cp_free(e);
    bbq_arena_free(&a);
}

/* §8.1.4 commutative VN: `a + b` and `b + a` are congruent. The
 * thesis flags this as a gap in the basic engine — without explicit
 * handling, the per-position refinement separates them. */
static void test_cp_commutative_vn(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb   = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add1 = sir_add(&a, SIR_DTSHORT, la, lb);   /* a + b */
    sir_node_t* la2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* lb2  = sir_load_local(&a, 1, SIR_DTSHORT, NULL);
    sir_node_t* add2 = sir_add(&a, SIR_DTSHORT, lb2, la2); /* b + a */
    sir_node_t* ret  = sir_return(&a, add2, SIR_DTSHORT);
    sir_node_t* st   = sir_store_local(&a, 2, SIR_DTSHORT, NULL, add1, ret);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, add1);
    cp_vnode_t* v2 = cp_vnode_for(e, add2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v1->partition, v2->partition,
                                  "a+b and b+a must share a partition");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Absorbing-element with purity (sir_opt.c behavior, NOT §4.8):
 * `Mul(x, 0)` for pure `x` folds to constant 0. */
static void test_cp_absorb_mul_zero_pure(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* mul = sir_mul(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, mul, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vm = cp_vnode_for(e, mul);
    TEST_ASSERT_NOT_NULL(vm);
    TEST_ASSERT_EQUAL_INT(CP_C_KNOWN, vm->constant.state);
    TEST_ASSERT_EQUAL_INT(0, vm->constant.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Purity gate: `Mul(div, 0)` is NOT folded — the impure Div must
 * still execute. Without the gate this would fold to 0. */
static void test_cp_absorb_mul_zero_impure_preserved(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c1  = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* div = sir_div(&a, SIR_DTSHORT, ll, c1);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* mul = sir_mul(&a, SIR_DTSHORT, div, c0);
    sir_node_t* ret = sir_return(&a, mul, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vm = cp_vnode_for(e, mul);
    TEST_ASSERT_NOT_NULL(vm);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_BOTTOM, vm->constant.state,
                                  "impure operand must keep Mul live");
    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.8 algebraic 1-constant identity: `Add(x, 0)` becomes a Follower
 * of `x` and ends up in `x`'s partition (Click thesis §4.8). */
static void test_cp_identity_add_zero(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* add = sir_add(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, add, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* va = cp_vnode_for(e, add);
    cp_vnode_t* vl = cp_vnode_for(e, ll);
    TEST_ASSERT_NOT_NULL(va);
    TEST_ASSERT_NOT_NULL(vl);
    TEST_ASSERT_TRUE_MESSAGE(va->leader >= 0, "Add(x,0) must be a Follower");
    TEST_ASSERT_EQUAL_PTR(vl, e->vnodes[va->leader]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(vl->partition, va->partition,
                                  "Follower lives in its Leader's partition");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Find a spine node's index, or -1. */
static int cp_spine_idx_of(cp_engine_t* e, sir_node_t* n) {
    for (int i = 0; i < e->spine_count; i++)
        if (e->spine[i] == n) return i;
    return -1;
}

/* Backward liveness: `StoreLocal 0 = 5; Return 7` — slot 0 is dead
 * at exit, killed by the store, never re-read. live_out[store][0]
 * and live_in[store][0] are both false. */
static void test_cp_liveness_dead_store(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c7  = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ret = sir_return(&a, c7, SIR_DTSHORT);
    sir_node_t* c5  = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTSHORT, NULL, c5, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_compute_liveness(e);
    TEST_ASSERT_NOT_NULL(e->live_in);
    TEST_ASSERT_NOT_NULL(e->live_out);

    int st_i  = cp_spine_idx_of(e, st);
    int ret_i = cp_spine_idx_of(e, ret);
    TEST_ASSERT_TRUE(st_i >= 0 && ret_i >= 0);
    TEST_ASSERT_FALSE_MESSAGE(e->live_out[st_i][0],  "store's slot 0 is dead-out");
    TEST_ASSERT_FALSE_MESSAGE(e->live_in[st_i][0],   "store's slot 0 is dead-in");
    TEST_ASSERT_FALSE_MESSAGE(e->live_out[ret_i][0], "return's slot 0 is dead-out");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Backward liveness: `StoreLocal 0 = 5; Return LoadLocal 0` — slot 0
 * is read by the return, so it's live coming out of the store. */
static void test_cp_liveness_live_store(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* ret = sir_return(&a, ll, SIR_DTSHORT);
    sir_node_t* c5  = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTSHORT, NULL, c5, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_compute_liveness(e);

    int st_i  = cp_spine_idx_of(e, st);
    int ret_i = cp_spine_idx_of(e, ret);
    TEST_ASSERT_TRUE(st_i >= 0 && ret_i >= 0);
    TEST_ASSERT_TRUE_MESSAGE(e->live_in[ret_i][0],  "return reads slot 0");
    TEST_ASSERT_TRUE_MESSAGE(e->live_out[st_i][0], "slot 0 is live across store");
    /* The store itself kills slot 0; its own live-in is dead. */
    TEST_ASSERT_FALSE_MESSAGE(e->live_in[st_i][0], "store's live-in kills slot 0");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — DSE drops a StoreLocal whose target slot is dead-out
 * and whose value is pure. The store becomes a Nop. */
static void test_cp_rewrite_dse_pure(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c7  = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ret = sir_return(&a, c7, SIR_DTSHORT);
    sir_node_t* c5  = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTSHORT, NULL, c5, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    /* `st` was rewritten in place into a Nop; its successor is the
     * original Return, unchanged. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, st->tag,
                                  "dead StoreLocal with pure value → Nop");
    TEST_ASSERT_EQUAL_PTR(ret, st->nop.next);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — DSE on a dead StoreLocal with an impure value converts
 * it to ExprEffect: the slot store is dropped but the expression
 * runs for its side effects (Div can trap). */
static void test_cp_rewrite_dse_impure(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c7  = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ret = sir_return(&a, c7, SIR_DTSHORT);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* div = sir_div(&a, SIR_DTSHORT, ll, c2);   /* impure */
    sir_node_t* st  = sir_store_local(&a, 1, SIR_DTSHORT, NULL, div, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_EXPREFFECT, st->tag,
                                  "dead StoreLocal with impure value → ExprEffect");
    TEST_ASSERT_EQUAL_PTR(div, st->expr_effect.value);
    TEST_ASSERT_EQUAL_PTR(ret, st->expr_effect.next);
    cp_free(e);
    bbq_arena_free(&a);
}

/* THE CSE LIFT IS GONE (spec §8), and with it the tests that pinned its PLACEMENT —
 * "CSE lifts to method entry", "schedule-late clusters reuse", "Schedule-Early proxy
 * (thesis §6.3.3)". They pinned GCM: the one use of dominance this design does not have.
 * §8: "a value IS a node; using it IS an edge; GVN merges congruent nodes globally. No
 * availability query." Measured on the whole jre before removing: the lift made the module
 * 3 bytes BIGGER, moved not one guard in the census, and cost ~0.7s of a 7.7s compile.
 *
 * GVN's CONGRUENCE is untouched and pinned all through this suite — it proves `i <
 * a.length` against a guard's own `a.length`, and forwards a load to its reaching store.
 * Only materializing a congruent expression into a local went, and that required choosing
 * a program point, which is a scheduling question the DDCG already answered. */

/* Rewrite — CHECKCAST elim: `(Class) new Class()` rewrites to the
 * `new` directly. The type lattice proves the cast is redundant. */
static void test_cp_rewrite_checkcast_elim(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* obj  = sir_new(&a, 42);                            /* new C() */
    sir_node_t* cast = sir_check_cast(&a, obj, SIR_ATCLASS, 42);    /* (C) obj */
    sir_node_t* ret  = sir_return(&a, cast, SIR_DTREF);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(obj, ret->return_.value,
                                  "CHECKCAST to exact-class → obj");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — GVN slot-collapse via copy propagation: `int y = a;
 * return y + 1` forwards `LoadLocal y` to `LoadLocal a`, leaving
 * StoreLocal y dead and DSE drops it. */
static void test_cp_rewrite_gvn_slot_collapse(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* la   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);  /* a */
    sir_node_t* ly   = sir_load_local(&a, 1, SIR_DTSHORT, NULL);  /* y */
    sir_node_t* c1   = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* add  = sir_add(&a, SIR_DTSHORT, ly, c1);    /* y + 1 */
    sir_node_t* ret  = sir_return(&a, add, SIR_DTSHORT);
    sir_node_t* st_y = sir_store_local(&a, 1, SIR_DTSHORT, NULL, la, ret); /* y = a */
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, st_y);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, add->add.left->tag,
                                  "y forwarded to a LoadLocal");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, add->add.left->load_local.slot,
                                  "y forwarded to slot 0 (a)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, st_y->tag,
                                  "dead StoreLocal y dropped");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — empty-branch fold: a Branch whose two arms converge to
 * the same node (here, both directly to `ret`) collapses to a Goto. */
static void test_cp_rewrite_empty_branch_fold(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c5  = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* ret = sir_return(&a, c5, SIR_DTSHORT);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br  = sir_branch(&a, ll, ret, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, br->tag,
        "branch with identical arms folds to Nop (control falls through .next to the merge)");
    TEST_ASSERT_EQUAL_PTR(ret, br->nop.next);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — UCE branch folding: a Branch with a constant-false
 * condition collapses to a Nop whose .next is the on-false arm. */
static void test_cp_rewrite_branch_fold(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c5    = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* ret_t = sir_return(&a, c5, SIR_DTSHORT);
    sir_node_t* c7    = sir_load_const(&a, 7, SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a, c7, SIR_DTSHORT);
    sir_node_t* cond  = sir_load_const(&a, 0, SIR_DTSHORT);  /* false */
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, br->tag,
        "constant-false Branch folds to Nop whose .next is the on_false arm");
    TEST_ASSERT_EQUAL_PTR(ret_f, br->nop.next);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — §4.8 Follower emit: `Add(x, 0)` rewrites to `x`. The
 * return's value should be the LoadLocal node, not an Add. */
static void test_cp_rewrite_follower_emit(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* add = sir_add(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, add, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, v->tag,
                                  "Add(x, 0) must rewrite to x");
    TEST_ASSERT_EQUAL_PTR(ll, v);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Rewrite — constant substitution: `2 + 3` folds to `LoadConst 5`,
 * so after cp_rewrite the return's value is a LoadConst, not an Add. */
static void test_cp_rewrite_constant_fold(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* c3  = sir_load_const(&a, 3, SIR_DTSHORT);
    sir_node_t* add = sir_add(&a, SIR_DTSHORT, c2, c3);
    sir_node_t* ret = sir_return(&a, add, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, v->tag,
                                  "Add(2,3) must rewrite to LoadConst");
    TEST_ASSERT_EQUAL_INT(5, v->load_const.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── WASM value model: i64 / f32 / f64 constant folding + value numbering.
 * long/float/double exercise the wide value model; the lattice carries the
 * wide value and folds at its own width. ── */

static void test_cp_fold_long_add(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* add = sir_add(&a, SIR_DTLONG,
        sir_load_long_const(&a, 0x100000000LL), sir_load_long_const(&a, 5));
    sir_node_t* ret = sir_return(&a, add, SIR_DTLONG);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLONGCONST, v->tag,
        "Add of two i64 consts folds to LoadLongConst (full 64-bit value)");
    TEST_ASSERT_TRUE(v->load_long_const.value == 0x100000005LL);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_double_mul(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* mul = sir_mul(&a, SIR_DTDOUBLE,
        sir_load_double_const(&a, 2.5), sir_load_double_const(&a, 4.0));
    sir_node_t* ret = sir_return(&a, mul, SIR_DTDOUBLE);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADDOUBLECONST, v->tag,
        "2.5 * 4.0 folds to LoadDoubleConst");
    TEST_ASSERT_TRUE(v->load_double_const.value == 10.0);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_float_add_rounds_to_f32(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* add = sir_add(&a, SIR_DTFLOAT,
        sir_load_float_const(&a, 1.5f), sir_load_float_const(&a, 2.25f));
    sir_node_t* ret = sir_return(&a, add, SIR_DTFLOAT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADFLOATCONST, v->tag,
        "1.5f + 2.25f folds to LoadFloatConst");
    TEST_ASSERT_TRUE(v->load_float_const.value == 3.75f);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_long_cmp_to_i32(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* A long comparison yields an i32 boolean. */
    sir_node_t* lt  = sir_lt(&a, sir_load_long_const(&a, 3),
                                 sir_load_long_const(&a, 5));
    sir_node_t* ret = sir_return(&a, lt, SIR_DTINT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, v->tag,
        "3L < 5L folds to an i32 LoadConst");
    TEST_ASSERT_EQUAL_INT(1, v->load_const.value);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_long_propagates_through_local(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* long x = 7; return x + 5;  →  12L after propagation through slot 0. */
    sir_node_t* add = sir_add(&a, SIR_DTLONG,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), sir_load_long_const(&a, 5));
    sir_node_t* ret = sir_return(&a, add, SIR_DTLONG);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTLONG, NULL,
        sir_load_long_const(&a, 7), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLONGCONST, v->tag,
        "i64 constant propagates through a local, then folds");
    TEST_ASSERT_TRUE(v->load_long_const.value == 12);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_long_sub_of_self_is_zero_l(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* (x - x) for a long x folds to 0L — a zero of the operand's width,
     * not an i32 zero (the §4.6 cong-fold width fix). */
    sir_node_t* sub = sir_sub(&a, SIR_DTLONG,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), sir_load_local(&a, 0, SIR_DTLONG, NULL));
    sir_node_t* ret = sir_return(&a, sub, SIR_DTLONG);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLONGCONST, v->tag,
        "long x - x folds to a 0L (i64), not an i32 zero");
    TEST_ASSERT_TRUE(v->load_long_const.value == 0);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_long_add_zero_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* x + 0L  →  x  for a non-constant long x (the §4.8 identity, width-aware
     * at i64 — the integer additive identity is exact). x = param at slot 0. */
    sir_node_t* add = sir_add(&a, SIR_DTLONG,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), sir_load_long_const(&a, 0));
    sir_node_t* ret = sir_return(&a, add, SIR_DTLONG);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, v->tag,
        "long x + 0L folds to x (i64 1-constant identity)");
    TEST_ASSERT_EQUAL_INT(0, v->load_local.slot);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_float_add_zero_not_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* x + 0.0f is NOT the identity in IEEE: for x = -0.0f the result is +0.0f.
     * So the Add must survive — proving float identities are deliberately off,
     * not folded like the integer ones. */
    sir_node_t* add = sir_add(&a, SIR_DTFLOAT,
        sir_load_local(&a, 0, SIR_DTFLOAT, NULL), sir_load_float_const(&a, 0.0f));
    sir_node_t* ret = sir_return(&a, add, SIR_DTFLOAT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_ADD, ret->return_.value->tag,
        "float x + 0.0 must NOT fold to x (IEEE -0.0 case)");
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_float_rem(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* rem = sir_rem(&a, SIR_DTFLOAT,
        sir_load_float_const(&a, 5.5f), sir_load_float_const(&a, 2.0f));
    sir_node_t* ret = sir_return(&a, rem, SIR_DTFLOAT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADFLOATCONST, v->tag,
        "5.5f rem 2.0f folds (frem = truncated remainder)");
    TEST_ASSERT_TRUE(v->load_float_const.value == 1.5f);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_double_rem(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* rem = sir_rem(&a, SIR_DTDOUBLE,
        sir_load_double_const(&a, 7.5), sir_load_double_const(&a, 2.0));
    sir_node_t* ret = sir_return(&a, rem, SIR_DTDOUBLE);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);

    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADDOUBLECONST, v->tag,
        "7.5 rem 2.0 folds (drem)");
    TEST_ASSERT_TRUE(v->load_double_const.value == 1.5);
    cp_free(e); bbq_arena_free(&a);
}

/* Range refinement: in the then-arm of `if (x < 10)`, x is refined to
 * [MIN, 9], so a downstream `x < 20` folds to true (1). Built once for i32
 * (the control — proves the int64 widening didn't regress) and once for i64
 * (proves long-variable range refinement now works). */
static void range_refine_case(sir_datatype_t dt, bbq_arena* a) {
    /* then-arm: return (x < 20) */
    sir_node_t* c20 = (dt == SIR_DTLONG) ? sir_load_long_const(a, 20)
                                         : sir_load_const(a, 20, dt);
    sir_node_t* inner = sir_lt(a, sir_load_local(a, 0, dt, NULL), c20);
    sir_node_t* t_ret = sir_return(a, inner, SIR_DTINT);
    /* else-arm: return 0 */
    sir_node_t* e_ret = sir_return(a, sir_load_const(a, 0, SIR_DTINT), SIR_DTINT);
    /* if (x < 10) */
    sir_node_t* c10 = (dt == SIR_DTLONG) ? sir_load_long_const(a, 10)
                                         : sir_load_const(a, 10, dt);
    sir_node_t* outer = sir_lt(a, sir_load_local(a, 0, dt, NULL), c10);
    sir_node_t* br = sir_branch(a, outer, t_ret, e_ret);
    sir_method_t* m = sir_method(a, "f", 0, 0, 1, br);
    cp_engine_t* e = cp_build(m, NULL, a, NULL, 0); cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, t_ret->return_.value->tag,
        "in then-arm x < 10, the downstream x < 20 folds via range refinement");
    TEST_ASSERT_EQUAL_INT(1, t_ret->return_.value->load_const.value);
    cp_free(e);
}

static void test_cp_range_refine_i32(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    range_refine_case(SIR_DTINT, &a);
    bbq_arena_free(&a);
}

static void test_cp_range_refine_i64(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    range_refine_case(SIR_DTLONG, &a);
    bbq_arena_free(&a);
}

/* ── Stacked congruent guards: the SECOND folds too ───────────────────────
 *
 * Three congruent-but-distinct ArrayLength reads of one array (the bound,
 * guard 1's own re-read, guard 2's — every Java mention of `a.length` is a
 * fresh read). Guard 2 is CONGRUENT to guard 1 (same op, partition-equal
 * inputs), so GVN itself carries the KNOWN-false verdict to it. A regression
 * here means either the symbolic-bound fold or the compare congruence broke.
 * (This is NOT the merge case — see the next test; a straight-line pin like
 * this one is exactly what let drop-at-joins go unpinned.) */
static void test_cp_refine_keeps_incumbent_symbolic_bound(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len3 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len3);
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* ge1  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);
    sir_node_t* br1  = sir_branch(&a, ge1,
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),
        br2);                               /* guard 1's ok edge → guard 2 */
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);
    sir_node_t* br0  = sir_branch(&a, lt0, br1,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 2, br0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, ge1);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v1->constant.state,
        "control: on `i < a.length`'s true edge a congruent re-read folds — "
        "the symbolic-bound machinery itself works");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v1->constant.value,
        "control: `i >= a.length` is FALSE on that edge");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the second, congruent guard folds too — GVN carries guard 1's "
        "KNOWN-false to it (same op, partition-equal inputs)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "guard 2 is FALSE, not merely narrowed");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The Mem hi-guard's condition, exactly as mem_bounds_guard emits it:
 * `(long)addr > (long)memory.size*64Ki − width`. The tested side rides the
 * addr slot through one I2L; the bound is an addr-free expression. Each call
 * mints FRESH MemSize/const reads, the way every Java mention re-reads. */
static sir_node_t* mk_mem_hi_guard(bbq_arena* a, int addr_slot, int width) {
    sir_node_t* limit = sir_sub(a, SIR_DTLONG,
        sir_mul(a, SIR_DTLONG, sir_i2_l(a, sir_mem_size(a)),
                               sir_load_long_const(a, 65536)),
        sir_load_long_const(a, width));
    return sir_gt(a, sir_i2_l(a, sir_load_local(a, addr_slot, SIR_DTINT, NULL)), limit);
}

/* ── The Mem hi-guard's MemSize symbolic bound: the SECOND guard folds ─────
 *
 * The array-length pin above proves the symbolic-bound machinery for
 * a.length; this is its MemSize twin — the shape mem_bounds_guard emits,
 * which until now was pinned ONLY at e2e (test_sir's "Mem guard merging"),
 * so a reshape that leans on this consumer had no owning-level lock. Two
 * adjacent hi-guards on one addr: guard 1's FALSE edge records
 * `addr ≤ memory.size*64Ki − width` (the GT-false mint), and guard 2's
 * bound is congruent through the MemSize memory-input keying, so guard 2's
 * GT folds KNOWN-false. Falsify by neutering the GT-false mint (§ near the
 * SIR_GT arm of cp_symbolic_bound_const / its phase-R producer): v2 drops to
 * BOTTOM and this goes red. */
static void test_cp_memsize_symbolic_bound_second_guard_folds(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* g2  = mk_mem_hi_guard(&a, 0, 4);
    sir_node_t* br2 = sir_branch(&a, g2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* g1  = mk_mem_hi_guard(&a, 0, 4);
    sir_node_t* br1 = sir_branch(&a, g1,
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),  /* throw on TRUE */
        br2);                                                         /* ok edge → guard 2 */
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, g2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the second MemSize hi-guard folds: guard 1's FALSE edge records "
        "addr <= memory.size*64Ki - width, and the two bounds are congruent "
        "through the MemSize memory-input keying");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "`(long)addr > limit` is FALSE on guard 1's ok edge");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The mem_range_guard (fill/copy) hi-guard, exactly as reshaped:
 * `(long)base > (long)memory.size*64Ki − (long)len` — identical to the
 * bounds-guard shape except the bound subtracts a LEN SLOT (through I2L)
 * instead of a constant width. Same base+len slots on both calls. */
static sir_node_t* mk_mem_range_hi_guard(bbq_arena* a, int base_slot, int len_slot) {
    sir_node_t* limit = sir_sub(a, SIR_DTLONG,
        sir_mul(a, SIR_DTLONG, sir_i2_l(a, sir_mem_size(a)),
                               sir_load_long_const(a, 65536)),
        sir_i2_l(a, sir_load_local(a, len_slot, SIR_DTINT, NULL)));
    return sir_gt(a, sir_i2_l(a, sir_load_local(a, base_slot, SIR_DTINT, NULL)), limit);
}

/* ── The RANGE hi-guard (fill/copy) with a variable len: does the second
 * fold? ─────────────────────────────────────────────────────────────────
 *
 * The reshape put mem_range_guard on the same array shape as the load guard
 * above; the ONLY difference is the bound subtracts `(long)len` (a slot read
 * through I2L) instead of a constant width. Same base and len slots on both
 * guards, no memory write between them (memory.fill does not advance the
 * memsize cell), so by the load pin above the second guard should fold
 * KNOWN-false. This isolates the variable-len bound — the exact thing the
 * e2e fill/copy count could not tell apart from a re-arm gap. */
static void test_cp_memrange_symbolic_bound_second_guard_folds(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* g2  = mk_mem_range_hi_guard(&a, 0, 1);
    sir_node_t* br2 = sir_branch(&a, g2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* g1  = mk_mem_range_hi_guard(&a, 0, 1);
    sir_node_t* br1 = sir_branch(&a, g1,
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),  /* throw on TRUE */
        br2);                                                         /* ok edge → guard 2 */
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, br1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, g2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the second RANGE hi-guard folds too — the variable-len bound is "
        "congruent across the two guards exactly as the constant-width one is");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "`(long)base > memory.size*64Ki - (long)len` is FALSE on guard 1's ok edge");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Memory DSE at the level that decides it (W9d) ─────────────────────────
 *
 * Click §A.5.3: "We treat memory like any other value, and call it the STORE …
 * STORE Nodes take in a STORE, an address, and a value and produce a new STORE.
 * PHI Nodes merge the STORE like other values." So a dead store is a STORE VALUE
 * WITH NO USERS, and the question is a def-use query, not a traversal. These pin
 * the decision on hand-built SIR — the e2e counts in test_sir confirm it through
 * the whole pipeline, but they cannot say WHY a store did or did not go.
 *
 * The cell is keyed by (class, field) — Click names the per-variable STORE split
 * as the better design and this is it — so two receivers SHARE a cell and the
 * must-alias test is what stands between this and deleting a live store. */
static sir_node_t* mk_two_field_stores(bbq_arena* a, int obj_slot_1, int obj_slot_2,
                                       sir_node_t** out_first) {
    sir_node_t* ret = sir_return_void(a);
    sir_node_t* s2  = sir_put_field(a, SIR_DTINT,
                                    sir_load_local(a, obj_slot_2, SIR_DTREF, NULL),
                                    7 /*class*/, 0 /*field*/,
                                    sir_load_const(a, 2, SIR_DTINT), ret);
    sir_node_t* s1  = sir_put_field(a, SIR_DTINT,
                                    sir_load_local(a, obj_slot_1, SIR_DTREF, NULL),
                                    7, 0, sir_load_const(a, 1, SIR_DTINT), s2);
    *out_first = s1;
    return s1;
}

static void test_cp_mem_dse_overwritten_field_store_is_dead(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* first = NULL;
    sir_node_t* entry = mk_two_field_stores(&a, 0, 0, &first);   /* same receiver slot */
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, entry);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_rewrite(e);                      /* the transform runs on CONVERGED rows */
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, first->tag,
        "the overwritten store is retagged NOP: its STORE value's only user is the "
        "store that supersedes it at the same location");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS: two receivers share the (class, field) cell but not the LOCATION. */
static void test_cp_mem_dse_distinct_receivers_both_live(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* first = NULL;
    sir_node_t* entry = mk_two_field_stores(&a, 0, 1, &first);   /* a.f then b.f */
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, entry);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_PUTFIELD, first->tag,
        "SOUNDNESS: a.f and b.f share a cell but not a location — the first store "
        "overwrites nothing and must stay");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS: a load between them reads the first value — it is a USER of that STORE. */
static void test_cp_mem_dse_intervening_load_keeps_store(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ret = sir_return(&a, sir_get_field(&a, SIR_DTINT,
                                     sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0),
                                 SIR_DTINT);
    sir_node_t* s2  = sir_put_field(&a, SIR_DTINT,
                                    sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0,
                                    sir_load_const(&a, 2, SIR_DTINT), ret);
    sir_node_t* mid = sir_store_local(&a, 1, SIR_DTINT, NULL,
                                      sir_get_field(&a, SIR_DTINT,
                                        sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0), s2);
    sir_node_t* s1  = sir_put_field(&a, SIR_DTINT,
                                    sir_load_local(&a, 0, SIR_DTREF, NULL), 7, 0,
                                    sir_load_const(&a, 1, SIR_DTINT), mid);
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, s1);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_PUTFIELD, s1->tag,
        "SOUNDNESS: a load between the stores is a user of the first STORE — it stays");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── A Refine's identity is its CONTENT, not the branch that minted it ─────
 *
 * Spec §8: "a value IS a node … GVN merges congruent nodes globally." An EXPR
 * node gets that from cp_partition_init's opcode buckets; a Refine is not a SIR
 * op and has no bucket, so its identity has to come from the constructor. Two
 * PARALLEL arms of a diamond each prove `len <= 100` then `len >= 0` about the
 * same value: same fact, same input, so the composite each arm's load reads must
 * be ONE node. Parallel (not sequential) is the point — neither arm sees the
 * other's refinement, so nothing collapses by idempotence and only canonical
 * construction can make them equal. Without it the two are distinct singleton
 * partitions and every expression over them is incongruent — the disease the
 * pts-refine Follower comment names ("the entire graph downstream of every
 * deref"), which is why this must hold for range refines too. */
static void test_cp_refine_identity_is_canonical(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = the diamond selector, slot 1 = len */
    sir_node_t* loadA = sir_load_local(&a, 1, SIR_DTINT, NULL);
    sir_node_t* loadB = sir_load_local(&a, 1, SIR_DTINT, NULL);
    sir_node_t* armA  = sir_branch(&a,
        sir_gt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), sir_load_const(&a, 100, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_branch(&a,
            sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), sir_load_const(&a, 0, SIR_DTINT)),
            sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT),
            sir_return(&a, loadA, SIR_DTINT)));
    sir_node_t* armB  = sir_branch(&a,
        sir_gt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), sir_load_const(&a, 100, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),
        sir_branch(&a,
            sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), sir_load_const(&a, 0, SIR_DTINT)),
            sir_return(&a, sir_load_const(&a, 4, SIR_DTINT), SIR_DTINT),
            sir_return(&a, loadB, SIR_DTINT)));
    sir_node_t* top = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), sir_load_const(&a, 0, SIR_DTINT)),
        armA, armB);
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, top);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* va = cp_vnode_for(e, loadA);
    cp_vnode_t* vb = cp_vnode_for(e, loadB);
    TEST_ASSERT_NOT_NULL(va);
    TEST_ASSERT_NOT_NULL(vb);
    TEST_ASSERT_TRUE_MESSAGE(va->input_count == 1 && vb->input_count == 1,
        "each load reads exactly one reaching state");
    TEST_ASSERT_TRUE_MESSAGE(va->inputs[0] >= 0
            && e->vnodes[va->inputs[0]]->kind == CP_VN_REFINE,
        "the load is rewired to the refined state, not the raw parameter");
    TEST_ASSERT_EQUAL_INT_MESSAGE(va->inputs[0], vb->inputs[0],
        "the same fact about the same value is ONE node, whichever arm proved it "
        "(spec §8: GVN merges congruent nodes globally)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Emit a full mem_range_guard chain (lo: base<0, ln: len<0, hi) reaching `ok`,
 * exactly as the ddcg does, and hand back the hi CONDITION node so a test can
 * read its folded state. Each arm reads the base/len slots; the hi arm is the
 * §15 array shape. */
static sir_node_t* mk_mem_range_guard_chain(bbq_arena* a, int base_slot, int len_slot,
                                            sir_node_t* ok, sir_node_t** out_hi) {
    sir_node_t* hi = mk_mem_range_hi_guard(a, base_slot, len_slot);
    *out_hi = hi;
    sir_node_t* br_hi = sir_branch(a, hi,
        sir_return(a, sir_load_const(a, 9, SIR_DTINT), SIR_DTINT), ok);
    sir_node_t* br_ln = sir_branch(a,
        sir_lt(a, sir_load_local(a, len_slot, SIR_DTINT, NULL), sir_load_const(a, 0, SIR_DTINT)),
        sir_return(a, sir_load_const(a, 8, SIR_DTINT), SIR_DTINT), br_hi);
    sir_node_t* br_lo = sir_branch(a,
        sir_lt(a, sir_load_local(a, base_slot, SIR_DTINT, NULL), sir_load_const(a, 0, SIR_DTINT)),
        sir_return(a, sir_load_const(a, 7, SIR_DTINT), SIR_DTINT), br_ln);
    return br_lo;
}

/* ── Two full range-guard chains: the SECOND hi folds (fill/copy adjacency) ─
 *
 * The reshape put mem_range_guard on the array shape and the isolated hi folds
 * above — but the REAL chain runs each guard as lo (base<0), ln (len<0), hi,
 * so the len slot is refined (len >= 0) before EACH hi, and the hi bound
 * `... − (long)len` reads it. Two adjacent guards on the same base+len — the
 * shape of `Mem.memory_fill(x,0,n); Mem.memory_fill(x,0,n)` — must fold the
 * second hi just as two array accesses do: the len-refine before guard 2's hi
 * is the SAME fact on the SAME value as before guard 1's, so the two bounds
 * are congruent and the consumer's partition test passes. This is the
 * owning-level pin for the fill/copy fold; it is RED until identical range
 * refines are made congruent (GVN over refine nodes), which is the whole
 * point — a refine minted twice for one value+predicate is one value. */
static void test_cp_memrange_second_full_chain_folds(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* end = sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT);
    sir_node_t* hi2 = NULL;
    sir_node_t* g2  = mk_mem_range_guard_chain(&a, 0, 1, end, &hi2);
    sir_node_t* hi1 = NULL;
    sir_node_t* g1  = mk_mem_range_guard_chain(&a, 0, 1, g2, &hi1);   /* g1 ok → guard 2 */
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, g1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, hi2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the second full range guard's hi folds: the len-refine before it is the "
        "same fact on the same value as before the first, so the bounds are congruent");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "`(long)base > memory.size*64Ki - (long)len` is FALSE on the first guard's ok edge");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── A re-spilled argument still folds the second guard ────────────────────
 *
 * The shape the ddcg actually emits for two adjacent fills: base and len are
 * spilled into fresh temps before EACH call, so a re-store sits between the two
 * guard chains and the second chain's slots start unrefined. That much is fine —
 * the second chain re-proves base >= 0 and len >= 0 with its own lo/ln arms. What
 * matters is WHERE those re-proved facts land: a refine names a fact about a
 * VALUE, and a spilled copy is not a distinct value (§1), so the fact has to land
 * on the same node the first chain refined. Otherwise the second chain's bound
 * `limit - (long)len` reads a different node, the two Subs stop being congruent,
 * and the hi cannot fold however it is emitted. */
static void test_cp_memrange_folds_across_a_respill(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slots: 0 = base param, 1 = len param, 2 = base temp, 3 = len temp */
    sir_node_t* end = sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT);
    sir_node_t* hi2 = NULL;
    sir_node_t* g2  = mk_mem_range_guard_chain(&a, 2, 3, end, &hi2);
    sir_node_t* rs1 = sir_store_local(&a, 3, SIR_DTINT, NULL,
                                      sir_load_local(&a, 1, SIR_DTINT, NULL), g2);
    sir_node_t* rs0 = sir_store_local(&a, 2, SIR_DTINT, NULL,
                                      sir_load_local(&a, 0, SIR_DTINT, NULL), rs1);
    sir_node_t* hi1 = NULL;
    sir_node_t* g1  = mk_mem_range_guard_chain(&a, 2, 3, rs0, &hi1);
    sir_node_t* s1  = sir_store_local(&a, 3, SIR_DTINT, NULL,
                                      sir_load_local(&a, 1, SIR_DTINT, NULL), g1);
    sir_node_t* s0  = sir_store_local(&a, 2, SIR_DTINT, NULL,
                                      sir_load_local(&a, 0, SIR_DTINT, NULL), s1);
    sir_method_t* m = sir_method(&a, "f", 0, 4, 4, s0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, hi2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the second chain folds across the re-spill: the re-proved fact lands on the "
        "same node, because a spilled copy is not a distinct value");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "the second hi is FALSE, not merely narrowed");
    cp_free(e);
    bbq_arena_free(&a);
}


/* ── Refinement survives an all-agree interior merge (spec §4 = SCCP) ─────
 *
 * Spec §4: the branch refinement is "per-edge facts, exactly SCCP's
 * executable-edge mechanism". SCCP's join is the MEET OF THE EDGE VALUES —
 * a join whose every in-edge carries the SAME refined value keeps it (the
 * trivial-φ rule applied to refinement). Pass B's strict-parity choice
 * ("refinement drops at joins", plan §R.1 item 3 — deliberate, upgrade
 * deferred as its own census-visible change) reset EVERY merge to the
 * unrefined base, so any diamond inside a refined region — a ternary in a
 * loop body — wiped the loop bound for everything after it. String.replace:
 * `value[i]`'s guard (before the ternary) folded, `buf[i]`'s (after it)
 * survived — the IDX_HIGH −1 the R.1 gate table recorded.
 *
 * Shape: `if (i < a.length) { if (c) {} else {} ; guard: i >= a.length }`.
 * The diamond tests an UNRELATED slot; both its arms carry i's refinement
 * into the join unchanged, so the post-merge guard must still fold. The
 * guard is the only GE in the graph — no congruent sibling can rescue it,
 * so this red-first pin isolates the join transfer and nothing else. */
static void test_cp_refine_survives_all_agree_merge(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (int), slot 1 = a (ref), slot 2 = c (int) — all params */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* join = sir_nop(&a, br2);            /* the interior merge */
    sir_node_t* armT = sir_nop(&a, join);
    sir_node_t* armF = sir_nop(&a, join);
    sir_node_t* brC  = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        armT, armF);                                /* unrelated diamond */
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);
    sir_node_t* br0  = sir_branch(&a, lt0, brC,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m  = sir_method(&a, "f", 0, 3, 3, br0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "past a diamond whose every arm carries `i < a.length` unchanged, "
        "the guard still folds — SCCP's join is the meet of the EDGE values, "
        "not a reset to the unrefined base (spec §4)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value,
        "`i >= a.length` is FALSE after the merge");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The join's SOUNDNESS negative: where the paths genuinely DISAGREE the claim must go.
 * After `if (i < a.length) {} else {}` MERGES, `i >= a.length` is reachable-true (the
 * else path), so the post-merge guard must NOT fold — keeping the true-arm's refinement
 * there would delete a live bounds check. The all-agree keep must not overreach. */
static void test_cp_refine_dropped_when_merge_paths_disagree(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* join = sir_nop(&a, br2);
    sir_node_t* armT = sir_nop(&a, join);
    sir_node_t* armF = sir_nop(&a, join);          /* the UNREFINED else path */
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);
    sir_node_t* br0  = sir_branch(&a, lt0, armT, armF);
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 2, br0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v2->constant.state != CP_C_KNOWN,
        "the else path reaches this merge WITHOUT `i < a.length` — the refinement "
        "must un-refine at the join; folding here deletes a live bounds check");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The join's order-blind SOUNDNESS negative: BOTH arms refined, APART. `if (i < 10)`
 * refines the then-arm to [MIN,9] and the else-arm to [10,MAX]; past the merge NEITHER
 * holds, so neither `i >= 10` (else's fact) nor `i < 10` (then's fact) may fold. The
 * mirrored asserts make a "keep some pred's value" mis-instantiation red REGARDLESS of
 * predecessor order — the base-vs-refined negatives above can miss it when the
 * unrefined arm happens to be predecessor 0 (spine DFS visits false arms first). */
static void test_cp_refine_dropped_when_merge_arms_refine_apart(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* The two probes sit on PARALLEL arms of an unrelated-slot diamond after the
     * join — sequential probes would refine each other (surviving `i >= 10`'s
     * false edge legitimately proves `i < 10`, which is not the bug hunted). */
    sir_node_t* geA  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                              sir_load_const(&a, 10, SIR_DTINT));   /* else's fact  */
    sir_node_t* ltB  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                              sir_load_const(&a, 10, SIR_DTINT));   /* then's fact  */
    sir_node_t* brA  = sir_branch(&a, geA,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* brB  = sir_branch(&a, ltB,
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 4, SIR_DTINT), SIR_DTINT));
    sir_node_t* brC  = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), brA, brB);
    sir_node_t* join = sir_nop(&a, brC);
    sir_node_t* armT = sir_nop(&a, join);
    sir_node_t* armF = sir_nop(&a, join);
    sir_node_t* br0  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)), armT, armF);
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 2, br0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vA = cp_vnode_for(e, geA);
    cp_vnode_t* vB = cp_vnode_for(e, ltB);
    TEST_ASSERT_NOT_NULL(vA);
    TEST_ASSERT_NOT_NULL(vB);
    TEST_ASSERT_TRUE_MESSAGE(vA->constant.state != CP_C_KNOWN,
        "past the merge the else-arm's [10,MAX] must be gone — folding `i >= 10` "
        "means the join kept ONE arm's refinement (pred-order-dependent bug)");
    TEST_ASSERT_TRUE_MESSAGE(vB->constant.state != CP_C_KNOWN,
        "past the merge the then-arm's [MIN,9] must be gone — the mirror assert, "
        "so a keep-EITHER-pred bug is red regardless of predecessor order");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The join's second SOUNDNESS negative: a DEF on one path kills the refinement.
 * `if (i < a.length) { if (c) i = 1000000; ; guard }` — past the inner merge, i may
 * be the redefined value, far beyond a.length; folding `i >= a.length` there is a
 * miscompile (an out-of-bounds read runs unguarded). */
static void test_cp_refine_dropped_when_a_merge_path_redefines(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* join = sir_nop(&a, br2);
    sir_node_t* sDef = sir_store_local(&a, 0, SIR_DTINT, NULL,
        sir_load_const(&a, 1000000, SIR_DTINT), join);       /* i = 1000000 */
    sir_node_t* armF = sir_nop(&a, join);
    sir_node_t* brC  = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), sDef, armF);
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);
    sir_node_t* br0  = sir_branch(&a, lt0, brC,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m  = sir_method(&a, "f", 0, 3, 3, br0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v2->constant.state != CP_C_KNOWN,
        "one path redefines i past the bound — the join must not keep `i < a.length`; "
        "folding the guard here reads out of bounds unguarded");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The LOOP-BOUND fold at L0 — `for (i=0; i<a.length; i++) { guard: i >= a.length }`
 * folds the guard: the header's true edge binds i's symbolic bound, and both
 * ArrayLength reads are congruent. This family was pinned only at L1 (test_sir),
 * which is how a pass-B change once broke it while this suite stayed green. */
static void test_cp_loop_bound_guard_folds(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = a (ref param), slot 1 = i (local) */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), len2);
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);     /* i++; back edge */
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);                                                /* the in-body guard */
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), len1);
    sir_node_t* brL  = sir_branch(&a, lt0, br2,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* init = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);           /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, init);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "inside `for (i=0; i<a.length; i++)` the guard `i >= a.length` folds — "
        "the header's true edge binds i's symbolic bound to the loop's length read");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* …and the same fold with the REAL lowering's interposed IDX_LOW guard: the DDCG
 * emits `if (i < 0) throw` before the high-bound check, so the high guard's index
 * arrives through a COMPOSED refine ([0,∞) over the header's `i < a.length`). This
 * is the composition step the plain pin above skips — the exact seam where the
 * per-(r,base) instance machinery must preserve the symbolic bound. */
static void test_cp_loop_bound_guard_folds_past_low_guard(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = a (ref param), slot 1 = i (local) */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), len2);
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);     /* i++; back edge */
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);                                                /* the high guard */
    sir_node_t* brLo = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT),
        br2);                                                /* IDX_LOW, ok → high */
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL), len1);
    sir_node_t* brL  = sir_branch(&a, lt0, brLo,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* init = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);           /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, init);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "the high guard folds THROUGH the interposed IDX_LOW ok-edge — composition "
        "must carry the header's symbolic bound, not discard it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* THE REAL loop-bound shape, distilled from the DDCG dump of
 * `for(i=0;i<n;i++) s+=a[i]` (test_sir le[2]): the guard's index is a SLOT COPY of
 * the bound-refined counter, and an IDX_LOW guard's ok-edge refine sits BETWEEN the
 * copy and the high guard. The bound (`i < n`), the copy (`j = i`), the low-guard
 * compose (`j >= 0`), then the high guard (`j >= a.length`, with `a.length ≡ n`).
 * This is the shape my two earlier L0 reproducers OMITTED (no copy) — which is why
 * they passed while test_sir failed. It folds under once-only chaining; the E3
 * composition rewrites must keep it folding. */
static void test_cp_loop_bound_folds_through_copy_and_low_guard(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n, slot 1 = a, slot 2 = i, slot 3 = j */
    sir_node_t* alen = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* ge   = sir_ge(&a, sir_load_local(&a, 3, SIR_DTINT, NULL), alen);
    sir_node_t* brHi = sir_branch(&a, ge,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),   /* throw arm */
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));  /* ok arm    */
    sir_node_t* brLo = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT),   /* j<0 throw  */
        brHi);                                                        /* ok → high  */
    sir_node_t* cpy  = sir_store_local(&a, 3, SIR_DTINT, NULL,        /* j = i      */
        sir_load_local(&a, 2, SIR_DTINT, NULL), brLo);
    sir_node_t* brB  = sir_branch(&a,                                 /* i < n      */
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),
        cpy,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* mk   = sir_store_local(&a, 1, SIR_DTREF, NULL,        /* a = new int[n] */
        sir_new_array(&a, SIR_ATINT, sir_load_local(&a, 0, SIR_DTINT, NULL)), brB);
    sir_method_t* m  = sir_method(&a, "f", 0, 3, 4, mk);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, ge);
    TEST_ASSERT_NOT_NULL(v);
    /* LOCK-DOWN chain: each invariant the fold rests on, ordered so the FIRST
     * failure names the broken link. cp_symbolic_bound_const reads the guard
     * index's RANGE+hi_vn1 and compares the bound's partition to a.length's. */
    cp_vnode_t* nvn = cp_vnode_for(e, sir_child(brB->branch.cond, 1));  /* n in `i<n` */
    cp_vnode_t* av  = cp_vnode_for(e, alen);
    TEST_ASSERT_NOT_NULL(av);
    TEST_ASSERT_NOT_NULL(nvn);
    TEST_ASSERT_EQUAL_INT_MESSAGE(av->partition, nvn->partition,
        "LOCK 1 (array-length identity): a.length ≡ n — same partition, so the "
        "symbolic bound `n` and the guard's length are one value");
    cp_vnode_t* idx = e->vnodes[v->inputs[0]];
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, idx->constant.state,
        "LOCK 2 (bound survives copy+compose): the guard's index still carries a "
        "RANGE after `j = i` and the `j >= 0` refine — a KNOWN/BOTTOM here means "
        "composition dropped the `i < n` fact");
    TEST_ASSERT_TRUE_MESSAGE(idx->constant.hi_vn1 != 0,
        "LOCK 3 (symbolic bound present): that range names its SYMBOLIC upper "
        "bound (hi_vn1) — 0 means the compose kept the interval but lost the "
        "value-bound the array-length fold needs");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "LOCK 4 (fold): `j >= a.length` folds — all three above ⟹ the guard is dead");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Bounds join MODULO PARTITION at a φ, and the agreement RETRACTS ───────
 *
 * lastIndexOf's seed distilled: `i = from >= a.length ? a.length - 1 : from`,
 * then the guard `i >= a.length`. Every Java mention of `a.length` is its own
 * ArrayLength node, so the false arm's GE-false refine names read 1 as i's
 * bound while the true arm's Sub mints read 2 — two ids for one value. The
 * reads are CONGRUENT, and the meet counts two bound ids as agreeing when
 * their vnodes share a partition, so the bound survives the join and the guard
 * folds.
 *
 * That agreement is OPTIMISTIC. Initial partitions are opcode buckets, so two
 * ArrayLength reads of DIFFERENT arrays start congruent and are separated only
 * as CAUSE_SPLITS refines — an agreement believed early can become false. The
 * φ therefore RECORDS both bound vnodes as §4.7.4 other.def_use premises, and
 * cp_split's move notification re-arms it; the re-run meet then finds the
 * partitions unequal and drops the bound. Falsify by deleting the premise
 * recording in cp_node_const's φ arm: the agreement half stays green and the
 * retraction half goes red with a stale bound the split has already refuted.
 *
 * bound_slot picks the array the TRUE arm's length reads: slot 0 is the one
 * the condition and the guard read (congruent — agreement), slot 1 is another
 * parameter (never congruent — the bound must be gone once they split). */
static cp_engine_t* build_two_read_ternary_seed(bbq_arena* a, int bound_slot,
                                                sir_node_t** out_guard) {
    /* slot 0 = a, 1 = b (ref params), 2 = from (int param), 3 = i (seeded local) */
    sir_node_t* len_g = sir_array_length(a, sir_load_local(a, 0, SIR_DTREF, NULL));
    sir_node_t* guard = sir_ge(a, sir_load_local(a, 3, SIR_DTINT, NULL), len_g);
    sir_node_t* brG   = sir_branch(a, guard,
        sir_return(a, sir_load_const(a, 1, SIR_DTINT), SIR_DTINT),   /* throw arm */
        sir_return(a, sir_load_const(a, 2, SIR_DTINT), SIR_DTINT));  /* ok arm    */
    sir_node_t* merge = sir_nop(a, brG);
    sir_node_t* len_t = sir_array_length(a,
        sir_load_local(a, bound_slot, SIR_DTREF, NULL));
    sir_node_t* stT   = sir_store_local(a, 3, SIR_DTINT, NULL,       /* i = len - 1 */
        sir_sub(a, SIR_DTINT, len_t, sir_load_const(a, 1, SIR_DTINT)), merge);
    sir_node_t* stF   = sir_store_local(a, 3, SIR_DTINT, NULL,       /* i = from    */
        sir_load_local(a, 2, SIR_DTINT, NULL), merge);
    sir_node_t* len_c = sir_array_length(a, sir_load_local(a, 0, SIR_DTREF, NULL));
    sir_node_t* brC   = sir_branch(a,
        sir_ge(a, sir_load_local(a, 2, SIR_DTINT, NULL), len_c), stT, stF);
    sir_method_t* m   = sir_method(a, "f", 0, 3, 4, brC);
    *out_guard = guard;
    return cp_build(m, NULL, a, NULL, 0);
}

static void test_cp_phi_joins_bounds_of_congruent_reads(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* guard = NULL;
    cp_engine_t* e = build_two_read_ternary_seed(&a, 0, &guard);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* idx = e->vnodes[v->inputs[0]];
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, idx->constant.state,
        "the seeded index still carries a RANGE out of the join");
    TEST_ASSERT_TRUE_MESSAGE(idx->constant.hi_vn1 != 0,
        "the join KEEPS a symbolic upper bound although the arms named it "
        "through two distinct congruent length reads — the meet's id "
        "agreement is partition membership, not raw id equality");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "…so `i >= a.length` folds");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* …and the seed then enters a DOWN-COUNTING loop, which is where the two-read
 * shape actually lives (lastIndexOf). The loop header widens, and widening's id
 * comparison is the same one the meet makes: while the solve is optimistic the
 * published id MOVES (an arm with no bound yet gains one, and the accumulated
 * id becomes a congruent sibling), so a raw comparison reads that as "the bound
 * changed" and drops it. The decrement then has nothing to preserve and mints
 * `< i` against the counter itself, which no later join can agree with — a
 * stable pessimal fixpoint reachable only through the two-read seed, which is
 * why the straight-line pin above cannot see it. */
static void test_cp_phi_two_read_bound_survives_the_loop(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = a (ref param), 1 = b (unused here), 2 = from, 3 = i */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* len_g  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* guard  = sir_ge(&a, sir_load_local(&a, 3, SIR_DTINT, NULL), len_g);
    sir_node_t* dec    = sir_store_local(&a, 3, SIR_DTINT, NULL,      /* i = i - 1 */
        sir_sub(&a, SIR_DTINT, sir_load_local(&a, 3, SIR_DTINT, NULL),
                               sir_load_const(&a, 1, SIR_DTINT)), header);
    sir_node_t* brG    = sir_branch(&a, guard,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),  /* throw arm */
        dec);                                                        /* ok → latch */
    sir_node_t* brL    = sir_branch(&a,                               /* while (i >= 0) */
        sir_ge(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        brG, sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* tmerge = sir_nop(&a, header);                         /* ternary join */
    sir_node_t* len_t  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* stT    = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_sub(&a, SIR_DTINT, len_t, sir_load_const(&a, 1, SIR_DTINT)), tmerge);
    sir_node_t* stF    = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_load_local(&a, 2, SIR_DTINT, NULL), tmerge);
    sir_node_t* len_c  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* brC    = sir_branch(&a,
        sir_ge(&a, sir_load_local(&a, 2, SIR_DTINT, NULL), len_c), stT, stF);
    sir_method_t* m    = sir_method(&a, "f", 0, 3, 4, brC);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* idx = e->vnodes[v->inputs[0]];
    TEST_ASSERT_TRUE_MESSAGE(idx->constant.hi_vn1 != 0,
        "the bound survives the loop header's WIDENING although the id the join "
        "publishes moved between congruent siblings mid-solve");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "…so the in-body `i >= a.length` folds");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

static void test_cp_phi_bound_agreement_retracts_on_split(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* guard = NULL;
    cp_engine_t* e = build_two_read_ternary_seed(&a, 1, &guard);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* idx = e->vnodes[v->inputs[0]];
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, idx->constant.hi_vn1,
        "b.length and a.length start congruent (opcode buckets) and CAUSE_SPLITS "
        "separates them — the φ's recorded premise re-arms it and the re-run "
        "meet DROPS the bound; a surviving id is a proof the split refuted");
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "and the guard on a's length stays: b's length bounds nothing here");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── The SUBTRACTED id is a premise too, and it RETRACTS ───────────────────
 *
 * A guard on a sum bounds an addend by a DIFFERENCE: `t + p <= L` gives
 * `t ≤ L − p`, a bound naming TWO values. The subtracted id is a premise on
 * exactly the terms the base id is — when the value it names stops being the
 * one the composition matched, the bound must go, or a guard is eliminated on
 * a proof that no longer holds.
 *
 * Fixture: the sum guard binds `t` against `p`, but the loop's counter is
 * bounded by a DIFFERENT value (a second slot), so the composition's id
 * agreement fails. The index must reach the guard with no bound, and the guard
 * must stay. Falsify by matching the composition on anything weaker than value
 * identity — the guard folds and the pin goes red. */
/* The positive twin: same shape, but the counter is bounded by the SAME p the
 * sum guard subtracted. `t ≤ L − p` composes with `i < p` to `t + i < L` and
 * the guard folds. This is the owning level for the composition — the e2e
 * fixture adds the real lowering (spilled index temps, a loop header φ) on top,
 * so a failure there and a pass here says the machinery works and the lowering
 * is what loses it. */
static void test_cp_difference_bound_composes(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = v (ref), 1 = t, 2 = p, 3 = i */
    sir_node_t* len_g = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* idx   = sir_add(&a, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL),
        sir_load_local(&a, 3, SIR_DTINT, NULL));                 /* t + i */
    sir_node_t* guard = sir_ge(&a, idx, len_g);
    sir_node_t* brG   = sir_branch(&a, guard,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_node_t* brI   = sir_branch(&a,                            /* i < p */
        sir_lt(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_local(&a, 2, SIR_DTINT, NULL)),
        brG, sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* brI0  = sir_branch(&a,                            /* i >= 0 */
        sir_lt(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 4, SIR_DTINT), SIR_DTINT), brI);
    /* The fences come BEFORE the sum guard, exactly as the source shape does
     * (`toffset >= 0 && pn >= 0 && toffset + pn <= value.length`): the mint
     * needs both addends already known non-negative. */
    sir_node_t* len_c = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* brS   = sir_branch(&a,                            /* t + p <= len */
        sir_le(&a, sir_add(&a, SIR_DTINT,
                       sir_load_local(&a, 1, SIR_DTINT, NULL),
                       sir_load_local(&a, 2, SIR_DTINT, NULL)), len_c),
        brI0, sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT));
    sir_node_t* brP0  = sir_branch(&a,                            /* p >= 0 */
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 6, SIR_DTINT), SIR_DTINT), brS);
    sir_node_t* brT0  = sir_branch(&a,                            /* t >= 0 */
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 5, SIR_DTINT), SIR_DTINT), brP0);
    sir_method_t* m = sir_method(&a, "compose", 0, 4, 4, brT0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* ix = e->vnodes[v->inputs[0]];
    /* The lemma this shape turns on, pinned by name because it is the one that
     * is easy to get wrong: both addends are [0, MAX], so the INTERVAL fold of
     * `t + i` overflows and correctly refuses. The symbolic proof does not rest
     * on the interval — t ≥ 0, i ≥ 0, t ≤ L − p, i < p give t + i ≤ L − 1,
     * which bounds the sum AND shows the add cannot wrap. A composition gated
     * on the numeric fold succeeding silently does nothing here. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, ix->constant.state,
        "the composed sum is a RANGE even though its interval fold refused");
    TEST_ASSERT_TRUE_MESSAGE(ix->constant.hi_vn1 != 0,
        "the sum carries a bound: `t ≤ L − p` composed with `i < p` gives "
        "`t + i < L` — 0 means the composition never fired");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ix->constant.hi_vn_incl,
        "…and it is STRICT: t + i ≤ L − 1, never `≤ L`");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "…so `t + i >= v.length` folds");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* …and the same composition with `i` arriving through a LOOP-HEADER φ, which is
 * the shape the source actually has (`for (i = 0; i < pn; i++)`). The step
 * between this and the straight-line pin above is the header's meet-and-widen:
 * the difference bound on t must survive as a whole triple, and i's `< p` must
 * survive the back edge. Pinned separately so a loss at the header is not
 * mistaken for a broken transfer. */
static void test_cp_difference_bound_composes_through_loop(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = v (ref), 1 = t, 2 = p, 3 = i */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* len_g  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* idx    = sir_add(&a, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL),
        sir_load_local(&a, 3, SIR_DTINT, NULL));                  /* t + i */
    sir_node_t* guard  = sir_ge(&a, idx, len_g);
    sir_node_t* inc    = sir_inc(&a, 3, 1, SIR_DTINT,
        sir_load_local(&a, 3, SIR_DTINT, NULL), header);          /* i++; back edge */
    sir_node_t* brG    = sir_branch(&a, guard,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT), inc);
    sir_node_t* brL    = sir_branch(&a,                            /* i < p */
        sir_lt(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_local(&a, 2, SIR_DTINT, NULL)),
        brG, sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* init   = sir_store_local(&a, 3, SIR_DTINT, NULL,   /* i = 0 */
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* len_c  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* brS    = sir_branch(&a,                            /* t + p <= len */
        sir_le(&a, sir_add(&a, SIR_DTINT,
                       sir_load_local(&a, 1, SIR_DTINT, NULL),
                       sir_load_local(&a, 2, SIR_DTINT, NULL)), len_c),
        init, sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT));
    sir_node_t* brP0   = sir_branch(&a,                            /* p >= 0 */
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 6, SIR_DTINT), SIR_DTINT), brS);
    sir_node_t* brT0   = sir_branch(&a,                            /* t >= 0 */
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 5, SIR_DTINT), SIR_DTINT), brP0);
    sir_method_t* m = sir_method(&a, "looped", 0, 3, 4, brT0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* ix = e->vnodes[v->inputs[0]];
    TEST_ASSERT_TRUE_MESSAGE(ix->constant.hi_vn1 != 0,
        "the difference bound survives the loop header: t is invariant and i's "
        "`< p` rides the back edge, so `t + i < L` still holds in the body");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "…so the in-body `t + i >= v.length` folds");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* …and now with the REAL lowering's two extra links between the Add and the
 * high guard: the index is spilled to a temp (`j = t + i`) and the IDX_LOW
 * guard (`j < 0`) sits on the way. This is the same seam
 * test_cp_loop_bound_folds_through_copy_and_low_guard covers for the plain
 * bound — the composed sum's bound has to ride a copy and a compose, not just
 * be produced. */
static void test_cp_difference_bound_composes_through_copy_and_low_guard(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = v (ref), 1 = t, 2 = p, 3 = i, 4 = j (the spilled index) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* len_g  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* guard  = sir_ge(&a, sir_load_local(&a, 4, SIR_DTINT, NULL), len_g);
    sir_node_t* inc    = sir_inc(&a, 3, 1, SIR_DTINT,
        sir_load_local(&a, 3, SIR_DTINT, NULL), header);
    sir_node_t* brG    = sir_branch(&a, guard,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT), inc);
    sir_node_t* brLo   = sir_branch(&a,                            /* j < 0 */
        sir_lt(&a, sir_load_local(&a, 4, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT), brG);
    sir_node_t* spill  = sir_store_local(&a, 4, SIR_DTINT, NULL,   /* j = t + i */
        sir_add(&a, SIR_DTINT, sir_load_local(&a, 1, SIR_DTINT, NULL),
                               sir_load_local(&a, 3, SIR_DTINT, NULL)), brLo);
    sir_node_t* brL    = sir_branch(&a,                            /* i < p */
        sir_lt(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_local(&a, 2, SIR_DTINT, NULL)),
        spill, sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* init   = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* len_c  = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* brS    = sir_branch(&a,
        sir_le(&a, sir_add(&a, SIR_DTINT,
                       sir_load_local(&a, 1, SIR_DTINT, NULL),
                       sir_load_local(&a, 2, SIR_DTINT, NULL)), len_c),
        init, sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT));
    sir_node_t* brP0   = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 6, SIR_DTINT), SIR_DTINT), brS);
    sir_node_t* brT0   = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 5, SIR_DTINT), SIR_DTINT), brP0);
    sir_method_t* m = sir_method(&a, "spilled", 0, 3, 5, brT0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* ix = e->vnodes[v->inputs[0]];
    TEST_ASSERT_TRUE_MESSAGE(ix->constant.hi_vn1 != 0,
        "the composed bound rides the spill copy and the IDX_LOW compose — this "
        "is the shape the lowering actually emits");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "…so the high guard folds");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

static void test_cp_difference_bound_second_id_must_agree(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = v (ref), 1 = t, 2 = p, 3 = q (the OTHER bound), 4 = i */
    sir_node_t* len_g = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* idx   = sir_add(&a, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL),
        sir_load_local(&a, 4, SIR_DTINT, NULL));                 /* t + i */
    sir_node_t* guard = sir_ge(&a, idx, len_g);
    sir_node_t* brG   = sir_branch(&a, guard,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    /* i < q — bounded by q, NOT by the p the sum guard subtracted. */
    sir_node_t* brI   = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 4, SIR_DTINT, NULL),
                   sir_load_local(&a, 3, SIR_DTINT, NULL)),
        brG, sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    /* t + p <= v.length */
    sir_node_t* len_c = sir_array_length(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
    sir_node_t* brS   = sir_branch(&a,
        sir_le(&a, sir_add(&a, SIR_DTINT,
                       sir_load_local(&a, 1, SIR_DTINT, NULL),
                       sir_load_local(&a, 2, SIR_DTINT, NULL)), len_c),
        brI, sir_return(&a, sir_load_const(&a, 3, SIR_DTINT), SIR_DTINT));
    sir_method_t* m = sir_method(&a, "f", 0, 4, 5, brS);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, guard);
    TEST_ASSERT_NOT_NULL(v);
    cp_vnode_t* ix = e->vnodes[v->inputs[0]];
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ix->constant.hi_vn1,
        "the counter is bounded by q while the sum guard subtracted p — the "
        "composition must find NO id agreement and leave the sum unbounded");
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "…so `t + i >= v.length` does not fold");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Loop back-edges WIDEN — THE loop-carried lemma the fabricated "optimistic
 * header-keep" was faking. i is loop-invariant (the latch bumps c, not i), so it carries
 * `i < a.length` on BOTH header edges. §5's mechanism is widening, and widening a
 * loop-invariant value is that value (`x ▽ x = x`), so the bound survives the back-edge
 * and the in-body guard `i >= a.length` folds. No optimistic sweep, no graph mutation —
 * a pure monotone lattice operation. RED until Lattice D widens the symbolic bound across
 * the loop instead of dropping it at the header. */
static void test_cp_range_invariant_bound_survives_loop(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (invariant param), 1 = a (int[]), 2 = c (loop counter) */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);  /* i >= a.length */
    sir_node_t* inc  = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), header);     /* c++ (NOT i); back edge */
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brL  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),                  /* while c < 10 */
        br2,
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);  /* i < a.length (entry) */
    sir_node_t* br0  = sir_branch(&a, lt0, header,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* initC = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), br0);              /* c = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 3, initC);

    /* The DDCG records the loop header (COMPILER_SCOPE_LOOP, key=Ltop): it is the ONE
     * sanctioned source for "this merge is a loop header", so the widening's forward-edge keep
     * fires only where a real back-edge exists — never a diamond's late-linearized arm. */
    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v2->constant.state,
        "i is loop-invariant with `i < a.length` on entry; widening preserves an "
        "invariant bound across the back-edge, so `i >= a.length` folds in the loop");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v2->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS NEGATIVE (the mirror of the invariant-bound pin): the SAME shape, but the
 * probed slot i is INCREMENTED in the loop — a counter, not invariant. `i < a.length` proven
 * on entry does NOT survive: i climbs past a.length, so `i >= a.length` MUST NOT fold. The
 * forward-edge keep may keep a bound ONLY for a genuinely-invariant slot; a slot with a
 * header φ (a counter) has slot_in ≠ base on its forward edge and must bail. A false KNOWN
 * here is a bound kept for a value that changes — the exact class of the negative-index
 * miscompile (a stale bound folding a guard the loop can violate). */
static void test_cp_range_counter_entry_bound_not_kept(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (COUNTER, i++ in the loop), 1 = a (int[]), 2 = c (loop-condition counter) */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* len2 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* ge2  = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len2);  /* i >= a.length */
    sir_node_t* inc  = sir_inc(&a, 0, 1, SIR_DTINT,
        sir_load_local(&a, 0, SIR_DTINT, NULL), header);     /* i++ (the probed slot!); back edge */
    sir_node_t* br2  = sir_branch(&a, ge2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brL  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),                  /* while c < 10 */
        br2,
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);  /* i < a.length (entry) */
    sir_node_t* br0  = sir_branch(&a, lt0, header,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* initI = sir_store_local(&a, 0, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), br0);              /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 3, initI);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v2 = cp_vnode_for(e, ge2);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v2->constant.state != CP_C_KNOWN,
        "i is INCREMENTED in the loop — the entry `i < a.length` cannot survive, so "
        "`i >= a.length` must NOT fold; keeping the stale bound is the negative-index bug");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS NEGATIVE (an UPPER bound does not imply a lower one): invariant i with
 * `i < a.length` kept across the loop by the forward-edge keep — but `i < a.length` says
 * NOTHING about i >= 0 (i may be negative). So `i < 0` MUST NOT fold. A false KNOWN here is
 * an invented lower bound = the negative-index miscompile: an in-loop `a[i]` would drop its
 * IDX_LOW guard although i can be negative. */
static void test_cp_range_kept_upper_bound_does_not_imply_nonneg(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (invariant param), 1 = a (int[]), 2 = c (loop counter) */
    sir_node_t* len1 = sir_array_length(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* lo2  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                  sir_load_const(&a, 0, SIR_DTINT));    /* i < 0 (probe in body) */
    sir_node_t* inc  = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), header);               /* c++ (NOT i); back edge */
    sir_node_t* br2  = sir_branch(&a, lo2,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brL  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),                  /* while c < 10 */
        br2,
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* lt0  = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL), len1);  /* i < a.length (entry) */
    sir_node_t* br0  = sir_branch(&a, lt0, header,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_node_t* initC = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), br0);              /* c = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 3, initC);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, lo2);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "`i < a.length` kept across the loop must NOT make `i < 0` fold — an upper bound "
        "is not a lower bound; folding it invents non-negativity (the negative-index bug)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening keeps the numeric lower bound: a counter from 0 that only increments
 * has range `[0, ∞)` — widening preserves lo=0 while the upper bound is lost. So the
 * IDX_LOW guard `i < 0` folds to FALSE (i >= 0) — the induction lower bound the analysis uses to
 * drop IDX_LOW / NegativeArraySize. */
static void test_cp_range_counter_lower_bound_from_widening(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = i (local counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* loTest = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                    sir_load_const(&a, 0, SIR_DTINT));       /* i < 0 */
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);      /* i++; back edge */
    sir_node_t* brLo = sir_branch(&a, loTest,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),   /* i<0 throw */
        inc);                                                         /* ok -> i++ */
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),            /* while i < n */
        brLo,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, initI);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, loTest);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v->constant.state,
        "a counter from 0 that only increments is >= 0 (widening keeps lo=0), so the "
        "IDX_LOW guard `i < 0` folds to FALSE");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, v->constant.value, "…to FALSE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS: an unbounded param used as a loop index is NOT claimed non-negative.
 * i enters as a param (any int) and the loop never establishes i >= 0, so the IDX_LOW
 * guard `i < 0` must NOT fold — widening may not invent a lower bound. */
static void test_cp_range_unbounded_param_not_claimed_nonneg(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = i (param — unbounded), 2 = c (counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* loTest = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                    sir_load_const(&a, 0, SIR_DTINT));       /* i < 0 */
    sir_node_t* inc  = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), header);      /* c++; back edge */
    sir_node_t* brLo = sir_branch(&a, loTest,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),               /* while c < n */
        brLo,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initC = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* c = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 3, initC);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, loTest);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "i is an unbounded param and the loop never establishes i >= 0, so the IDX_LOW "
        "guard `i < 0` must not fold — widening may not invent a lower bound");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening ANALYSIS pin (asserts the RANGE VALUE, not the guard fold): a counter from 0
 * that only increments must have `i`'s interval widen to `[0, +∞)` — widening keeps the
 * lower bound (0 never decreases) and loses the upper. This tests the widening TRANSFER
 * directly, the lattice fact the guard consumer later reads. */
static void test_cp_range_counter_widens_lo0(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = i (local counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* iload  = sir_load_local(&a, 1, SIR_DTINT, NULL);       /* grab i in the body */
    sir_node_t* loTest = sir_lt(&a, iload, sir_load_const(&a, 0, SIR_DTINT));   /* i < 0 */
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);      /* i++; back edge */
    sir_node_t* brLo = sir_branch(&a, loTest,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),            /* while i < n */
        brLo,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, initI);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vi = cp_vnode_for(e, iload);
    TEST_ASSERT_NOT_NULL(vi);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, vi->constant.state,
        "i is a widened counter — its lattice value must be a RANGE");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)vi->constant.lo,
        "widening keeps the lower bound: i's range starts at 0");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS NEGATIVE (overflow): a counter from 0 that increments with NO upper
 * bound on ITSELF (the loop runs on a DIFFERENT counter) overflows INT_MAX → INT_MIN, so
 * it becomes negative. `i < 0` therefore MUST NOT fold — claiming i >= 0 here is the
 * overflow negative-index bug. This is the exact partner of the lower-bound pin above,
 * where the `i < n` bound keeps i below MAX so `i+1` cannot wrap and the fold IS sound. */
static void test_cp_range_counter_unbounded_can_overflow_negative(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = i (counter, NO upper bound), 2 = c (loop counter on `c < n`) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* iload  = sir_load_local(&a, 1, SIR_DTINT, NULL);       /* grab i in the body */
    sir_node_t* loTest = sir_lt(&a, iload, sir_load_const(&a, 0, SIR_DTINT));   /* i < 0 */
    sir_node_t* inc_c = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), header);      /* c++; back edge */
    sir_node_t* inc_i = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), inc_c);       /* i++ (unbounded) then c++ */
    sir_node_t* brLo = sir_branch(&a, loTest,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc_i);
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),            /* while c < n (NOT i) */
        brLo,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initC = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* c = 0 */
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), initC);             /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 3, initI);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, loTest);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "an unbounded incrementing counter overflows to negative — `i < 0` must not fold");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS (the plain counter loop WITH a recorded header, i.e. skip_back ACTIVE —
 * the case every other counter pin misses by passing no scope fact): `for(i=0;i<n;i++)`.
 * The loop condition `i < n` MUST stay non-constant — folding it collapses the loop (the
 * counter Inc goes dead), which is exactly the initProperties loop-collapse miscompile. */
static void test_cp_range_counter_loop_condition_survives_skipback(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = i (counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* cond = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                  sir_load_local(&a, 0, SIR_DTINT, NULL));   /* i < n */
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);                     /* i++; back edge */
    sir_node_t* brCond = sir_branch(&a, cond, inc,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* i = 0 */
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, initI);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, cond);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "the counter loop condition `i < n` must NOT fold — folding it collapses the loop "
        "(dead Inc), the initProperties loop-collapse miscompile skip_back triggers");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS (initProperties shape): the loop is guarded by `if (total <= 0) return`,
 * so `total` enters the loop refined `>= 1` and the forward-edge keep carries it across the
 * header. The counter loop condition `i < total` must STILL NOT fold — the kept invariant
 * `total >= 1` says nothing about i vs total. This mirrors the exact shape whose loops the
 * dump showed collapsing (branches + counter Incs deleted) under skip_back. */
static void test_cp_range_counter_loop_under_kept_bound_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = total (param), 1 = i (counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* cond = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                  sir_load_local(&a, 0, SIR_DTINT, NULL));   /* i < total */
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);                     /* i++; back edge */
    sir_node_t* brCond = sir_branch(&a, cond, inc,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* i = 0 */
    /* outer guard: if (total <= 0) return 0; else initI…  → refines total >= 1 on entry */
    sir_node_t* le0  = sir_le(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                  sir_load_const(&a, 0, SIR_DTINT));   /* total <= 0 */
    sir_node_t* br0  = sir_branch(&a, le0,
        sir_return(&a, sir_load_const(&a, 9, SIR_DTINT), SIR_DTINT),
        initI);
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, br0);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, cond);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "`i < total` must NOT fold even with `total >= 1` kept across the header — the "
        "invariant lower bound on total says nothing about i, and folding collapses the loop");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS (initProperties loop 1 shape, faithfully): outer `if (total<=0) return`,
 * then `for(i=0;i<total;i++) if (p!=0) count++;` — a counter i AND a CONDITIONAL counter
 * count inside an inner branch (its own inner merge), header recorded (skip_back active).
 * The loop condition `i < total` must NOT fold. This is the exact shape the SIR dump showed
 * collapsing (branches + counter Incs deleted) under skip_back. */
static void test_cp_range_conditional_counter_loop_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = total (param), 1 = i (counter), 2 = count (conditional counter), 3 = p (param) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* cond = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                  sir_load_local(&a, 0, SIR_DTINT, NULL));   /* i < total */
    sir_node_t* incI = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);                     /* i++; back edge */
    sir_node_t* merge = sir_nop(&a, incI);
    sir_node_t* incCount = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), merge);                      /* count++ then merge */
    sir_node_t* innerBr = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),                        /* if (p != 0) */
        incCount, merge);
    sir_node_t* brCond = sir_branch(&a, cond, innerBr,
        sir_return(&a, sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT));  /* return count */
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);            /* i = 0 */
    sir_node_t* initCount = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), initI);             /* count = 0 */
    sir_node_t* le0  = sir_le(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                  sir_load_const(&a, 0, SIR_DTINT));   /* total <= 0 */
    sir_node_t* br0  = sir_branch(&a, le0,
        sir_return(&a, sir_load_const(&a, 9, SIR_DTINT), SIR_DTINT),
        initCount);
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 4, br0);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, cond);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "the conditional-counter loop condition `i < total` must NOT fold — folding it is the "
        "initProperties loop collapse (the SIR dump's deleted branches + counter Incs)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Widening SOUNDNESS NEGATIVE (the spine back-edge heuristic must fail safe): the loop header
 * is entered from BOTH arms of `if (x < 5)` — the true arm proves x<5, the FALSE arm proves
 * x>=5. sir_collect_spine visits false arms first, so the TRUE-arm entry is linearized AFTER
 * the header; its edge into the header has spine index > header, exactly like a back edge.
 * The forward-edge keep must NOT skip it as a back edge — if it does, it keeps ONE arm's
 * refinement (unsound at the other arm) and `x >= 5` folds. x is invariant. */
static void test_cp_range_diamond_loop_header_fails_safe(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = x (invariant param), 1 = c (loop counter param) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* ge5 = sir_ge(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                 sir_load_const(&a, 5, SIR_DTINT));   /* x >= 5 (probe) */
    sir_node_t* inc = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);             /* c++; back edge */
    sir_node_t* brGe = sir_branch(&a, ge5,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),               /* while c < 10 */
        brGe,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* trueArm  = sir_nop(&a, header);   /* pathA: x < 5  */
    sir_node_t* falseArm = sir_nop(&a, header);   /* pathB: x >= 5 */
    sir_node_t* outerBr = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 5, SIR_DTINT)),               /* if (x < 5) */
        trueArm, falseArm);
    sir_method_t* m = sir_method(&a, "f", 0, 2, 2, outerBr);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, ge5);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "loop entered from BOTH arms of `if (x<5)`: the other arm proves x>=5, so `x>=5` must "
        "NOT fold — the back-edge skip must not treat a late-linearized forward entry as a back edge");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Stage-2c lemma ladder: the properties the optimistic pass-B must satisfy, each
 * isolated (see the plan). L-VALUE and L-XFORM cover the two layers the analysis pins
 * stop short of: the rewire's value identity and the transform's consumption. ── */

/* Walk a value chain through the two window kinds — Refine and copy-LoadLocal — to its
 * root. A def-use walk (inputs[0]), never a CFG walk. */
static int cp_ut_chain_root(cp_engine_t* e, int x) {
    for (int hops = 0; hops < 256 && x >= 0 && x < e->vnode_count; hops++) {
        cp_vnode_t* v = e->vnodes[x];
        int next = -1;
        if (v->kind == CP_VN_REFINE && v->input_count >= 1) next = v->inputs[0];
        else if (v->kind == CP_VN_EXPR && v->expr && v->expr->tag == SIR_LOADLOCAL
                 && v->input_count == 1) next = v->inputs[0];
        else return x;
        if (next == x || next < 0) return x;
        x = next;
    }
    return x;
}

/* L-VALUE (Click §4.7: a Refine is a WINDOW onto its input, never a different value).
 * Every LoadLocal's possibly-rewired input chain must root at the same ultimate value as
 * pass-A's reaching def for that (spine, slot). A violation makes the load read the WRONG
 * VALUE — wrong results, not traps. */
static void cp_ut_assert_loads_value_preserved(cp_engine_t* e) {
    for (int vi = 0; vi < e->vnode_count; vi++) {
        cp_vnode_t* v = e->vnodes[vi];
        if (v->kind != CP_VN_EXPR || !v->expr || v->expr->tag != SIR_LOADLOCAL) continue;
        if (v->input_count < 1 || v->inputs[0] < 0) continue;
        int sp   = v->parent_spine;
        int slot = v->expr->load_local.slot;
        if (sp < 0 || sp >= e->slot_in_rows || slot < 0 || slot >= e->slot_count) continue;
        int passA = e->slot_in[sp][slot];
        if (passA < 0) continue;
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            cp_ut_chain_root(e, passA), cp_ut_chain_root(e, v->inputs[0]),
            "L-VALUE: a rewired LoadLocal must root at pass-A's reaching value — a Refine "
            "is a window, never a different value");
    }
}

/* L-JOIN-NEST: `if (x<100) { if (x<5) A else B; M: }` — past M the OUTER fact still holds
 * (both arms flowed through it: each delivers compose(inner-arm, R_outer), and the join of
 * two chains sharing the R_outer suffix is R_outer), the INNER fact does not. A join that
 * returns base here is SOUND but fails the lemma (the weakest COMMON fact is R_outer); a
 * join that keeps either INNER fact is UNSOUND. */
static void test_cp_join_nested_outer_kept_inner_dropped(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = x (param) */
    sir_node_t* p1 = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                sir_load_const(&a, 100, SIR_DTINT));  /* probe: x < 100 */
    sir_node_t* p2 = sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                sir_load_const(&a, 5, SIR_DTINT));    /* probe: x < 5   */
    sir_node_t* ret  = sir_return(&a, sir_add(&a, SIR_DTINT, p1, p2), SIR_DTINT);
    sir_node_t* M    = sir_nop(&a, ret);
    sir_node_t* armA = sir_nop(&a, M);
    sir_node_t* armB = sir_nop(&a, M);
    sir_node_t* inner = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 5, SIR_DTINT)), armA, armB);
    sir_node_t* outer = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 100, SIR_DTINT)),
        inner,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, outer);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_ut_assert_loads_value_preserved(e);
    cp_vnode_t* v1 = cp_vnode_for(e, p1);
    cp_vnode_t* v2 = cp_vnode_for(e, p2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v2->constant.state != CP_C_KNOWN,
        "L-JOIN-NEST soundness: the INNER fact (x<5) must NOT survive its own join");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, v1->constant.state,
        "L-JOIN-NEST: the OUTER fact (x<100) survives the inner join — both arms carry it "
        "(the join of two chains sharing the R_outer suffix is R_outer)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, v1->constant.value, "…and x<100 folds TRUE");
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-XFORM (initProperties loop-1 shape): guard → counter loop with an inner conditional
 * counter. After cp_rewrite the LIVE loop structure must survive: the loop Branch and the
 * inner Branch are still Branches, both Incs still Incs. The skip_back miscompile collapsed
 * exactly these (branches folded, counter Incs deleted) — this pins that layer, which no
 * analysis-level test reaches. */
static void test_cp_xform_conditional_counter_loop_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = total (param), 1 = i, 2 = count, 3 = p (param) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* cond = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                  sir_load_local(&a, 0, SIR_DTINT, NULL));
    sir_node_t* incI = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);
    sir_node_t* merge = sir_nop(&a, incI);
    sir_node_t* incCount = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), merge);
    sir_node_t* innerBr = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        incCount, merge);
    sir_node_t* brCond = sir_branch(&a, cond, innerBr,
        sir_return(&a, sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* initCount = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), initI);
    sir_node_t* le0  = sir_le(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                                  sir_load_const(&a, 0, SIR_DTINT));
    sir_node_t* br0  = sir_branch(&a, le0,
        sir_return(&a, sir_load_const(&a, 9, SIR_DTINT), SIR_DTINT),
        initCount);
    sir_method_t* m  = sir_method(&a, "f", 0, 2, 4, br0);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_ut_assert_loads_value_preserved(e);
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, br0->tag,
        "L-XFORM: the guard branch (total<=0, unbounded param) must not fold");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, brCond->tag,
        "L-XFORM: the loop condition (i<total) must survive the rewrite");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, innerBr->tag,
        "L-XFORM: the inner branch (p!=0, unbounded param) must survive");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, incI->tag,
        "L-XFORM: the loop counter Inc must survive (i is live via the condition)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, incCount->tag,
        "L-XFORM: the conditional counter Inc must survive (count is returned)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-XFORM (hostProperty shape): `if (len<0) return; for (i=0; i<len; i++) sum+=i;` — the
 * ok-edge refines len>=0 (invariant, kept across the header by the optimistic join). The
 * rewrite must keep the guard, the loop condition, and the Inc. */
static void test_cp_xform_guarded_counter_loop_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = len (param), 1 = i, 2 = sum */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* cond = sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                  sir_load_local(&a, 0, SIR_DTINT, NULL));
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);
    sir_node_t* body = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT, sir_load_local(&a, 2, SIR_DTINT, NULL),
                    sir_load_local(&a, 1, SIR_DTINT, NULL)), inc);
    sir_node_t* brCond = sir_branch(&a, cond, body,
        sir_return(&a, sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initI = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* initSum = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), initI);
    sir_node_t* guard = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT),
        initSum);
    sir_method_t* m = sir_method(&a, "f", 0, 1, 3, guard);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_ut_assert_loads_value_preserved(e);
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, guard->tag,
        "L-XFORM: the len<0 guard (unbounded param) must not fold");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, brCond->tag,
        "L-XFORM: the loop condition (i<len) must survive the rewrite");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, inc->tag,
        "L-XFORM: the counter Inc must survive (i live via the condition)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-TRANSIENT (Click ch.2 §2.3, verbatim: stopping top-down short of the gfp leaves
 * "elements in the set that do not have a corresponding rule. Optimizations using these
 * elements can be incorrect"; only BOTTOM-UP methods may "transform as they analyze").
 * A COMPOSED refine minted while the optimistic sweep transiently held a fact the header
 * later RETRACTS is exactly such an element — and minting it into the SHARED vnode space
 * mid-iteration is a transformation based on an intermediate top-down solution. After
 * convergence, every composed refine must be REFERENCED by the converged wiring (some
 * vnode's input); an orphan is an unproven optimistic artifact observable by GVN's
 * partition/def-use machinery — the E3/07-16 miscompile channel (same 6 exec failures). */
static void test_cp_no_unproven_transient_refines(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* The RETRACTION shape: entry `if (x<100)` refines invariant x into the loop; the body
     * has an INNER DIAMOND on x (`if (x<5) A else B; M`). Sweep 1 holds R1 at the header
     * (latch TOP) and mints compose(r_then,R1) / compose(r_else,R1) on the arms; the inner
     * merge M joins them; if the join under-approximates the meet (drops to base instead of
     * the common suffix R1), the latch delivers base, the header RETRACTS on sweep 2, and
     * the sweep-1 composes become ORPHANS — ch.2's unproven elements. */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* incC = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);          /* c++; back edge */
    sir_node_t* M    = sir_nop(&a, incC);
    sir_node_t* armA = sir_nop(&a, M);
    sir_node_t* armB = sir_nop(&a, M);
    sir_node_t* brX  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 5, SIR_DTINT)), armA, armB); /* if (x<5) — on x! */
    sir_node_t* brL  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),             /* while c < 10 */
        brX,
        sir_return(&a, sir_load_const(&a, 2, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brL);
    sir_node_t* initC = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* br0  = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 100, SIR_DTINT)),            /* if (x<100) enter */
        initC,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, br0);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    /* Collect every vnode id referenced as anyone's input. */
    bool* referenced = (bool*)calloc((size_t)e->vnode_count, sizeof(bool));
    for (int vi = 0; vi < e->vnode_count; vi++) {
        cp_vnode_t* v = e->vnodes[vi];
        for (int k = 0; k < v->input_count; k++) {
            int in = v->inputs[k];
            if (in >= 0 && in < e->vnode_count) referenced[in] = true;
        }
    }
    /* Every COMPOSED refine (a refine whose input is itself a refine) must be referenced. */
    for (int vi = 0; vi < e->vnode_count; vi++) {
        cp_vnode_t* v = e->vnodes[vi];
        if (v->kind != CP_VN_REFINE) continue;
        if (v->input_count < 1 || v->inputs[0] < 0) continue;
        if (e->vnodes[v->inputs[0]]->kind != CP_VN_REFINE) continue;   /* not composed */
        TEST_ASSERT_TRUE_MESSAGE(referenced[vi],
            "L-TRANSIENT (Click ch.2 §2.3): an unreferenced COMPOSED refine is an "
            "optimistic element with no corresponding rule, observable by the shared "
            "partition/def-use machinery — it must not survive convergence");
    }
    free(referenced);
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-REF-NONNULL-LOOP (hostProperty's `key`): a REF param proven NonNull before a loop it is
 * invariant through — the null probe inside the body folds FALSE (the pts refine kept across
 * the header, same join as the range case; the property path runs on REF slots and no other
 * pin covers a kept pts refine). Post-rewrite the loop must survive. */
static void test_cp_ref_nonnull_kept_across_loop(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = k (ref param, invariant), 1 = c (counter) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* probe = sir_eq(&a, sir_load_local(&a, 0, SIR_DTREF, NULL),
                                   sir_load_null(&a));            /* k == null (in body) */
    sir_node_t* inc  = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);          /* c++; back edge */
    sir_node_t* brP  = sir_branch(&a, probe,
        sir_return(&a, sir_load_const(&a, 9, SIR_DTINT), SIR_DTINT),
        inc);
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),            /* while c < 10 */
        brP,
        sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initC = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);
    sir_node_t* guard = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_null(&a)),
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT),
        initC);                                                   /* ok-edge: k NonNull */
    sir_method_t* m = sir_method(&a, "f", 0, 1, 2, guard);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_ut_assert_loads_value_preserved(e);
    cp_vnode_t* vp = cp_vnode_for(e, probe);
    TEST_ASSERT_NOT_NULL(vp);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, vp->constant.state,
        "L-REF: k proven NonNull before the loop, invariant through it — the in-body null "
        "probe folds (the kept pts refine, the REF twin of the invariant range bound)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, vp->constant.value, "…to FALSE");
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, guard->tag,
        "L-REF: the entry null guard (unbounded param) must not fold");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, brCond->tag,
        "L-REF: the loop condition must survive the rewrite");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, inc->tag,
        "L-REF: the counter Inc must survive");
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-REF-REDEF-LOOP (Hashtable.get's `e = e.next` chain walk): a REF slot REDEFINED in the
 * loop whose null test IS the loop condition. The condition must never fold (the φ can be
 * null or not); the in-body probe folds FALSE via the condition's own true edge (single-pred
 * refinement — pre-existing sound behavior); the loop survives the rewrite. */
static void test_cp_ref_redefined_in_loop_condition_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = e0 (ref param), 1 = e2 (ref param), 2 = e (ref local, redefined) */
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* probe = sir_eq(&a, sir_load_local(&a, 2, SIR_DTREF, NULL),
                                   sir_load_null(&a));            /* e == null (in body) */
    sir_node_t* redef = sir_store_local(&a, 2, SIR_DTREF, NULL,
        sir_load_local(&a, 1, SIR_DTREF, NULL), header);          /* e = e2; back edge */
    sir_node_t* brP  = sir_branch(&a, probe,
        sir_return(&a, sir_load_const(&a, 9, SIR_DTINT), SIR_DTINT),
        redef);
    sir_node_t* cond = sir_ne(&a, sir_load_local(&a, 2, SIR_DTREF, NULL),
                                  sir_load_null(&a));             /* while (e != null) */
    sir_node_t* brCond = sir_branch(&a, cond, brP,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_set_next(header, brCond);
    sir_node_t* initE = sir_store_local(&a, 2, SIR_DTREF, NULL,
        sir_load_local(&a, 0, SIR_DTREF, NULL), header);          /* e = e0 */
    sir_method_t* m = sir_method(&a, "g", 0, 2, 3, initE);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_ut_assert_loads_value_preserved(e);
    cp_vnode_t* vc = cp_vnode_for(e, cond);
    cp_vnode_t* vp = cp_vnode_for(e, probe);
    TEST_ASSERT_NOT_NULL(vc);
    TEST_ASSERT_NOT_NULL(vp);
    TEST_ASSERT_TRUE_MESSAGE(vc->constant.state != CP_C_KNOWN,
        "L-REF: the loop condition on a REDEFINED ref slot must never fold");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, vp->constant.state,
        "L-REF: inside the true arm of `e != null` the probe folds via the edge refinement");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, vp->constant.value, "…to FALSE");
    cp_rewrite(e);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, brCond->tag,
        "L-REF: the chain-walk loop must survive the rewrite");
    cp_free(e);
    bbq_arena_free(&a);
}

/* L-REARM-6 (the 6th instance of the recorded re-arm class — a fact arriving OFF the
 * def-use graph must be re-armed EXPLICITLY): a §10.7 identity FOLLOWER (`(new T[k]).length
 * ≡ k`) is linked to its leader, not def-use-wired to it; when the leader's constant
 * DESCENDS (the count φ: KNOWN 0 transiently → BOTTOM), the follower's STORED constant
 * must descend with it. The stale KNOWN 0 is what the REWRITE consumes (it reads the
 * stored field) — folding `.length` to 0 and the §15 IDX guard to ALWAYS-THROW: the
 * initProperties miscompile. Shape: a conditional counter sizes an array; the arraylen
 * must NOT be KNOWN. */
static void test_cp_arraylen_follower_rearms_on_leader_descent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), 1 = count (conditional counter), 2 = p (param), 3 = arr */
    sir_node_t* alen = sir_array_length(&a, sir_load_local(&a, 3, SIR_DTREF, NULL));
    sir_node_t* ret  = sir_return(&a, alen, SIR_DTINT);
    sir_node_t* mkarr = sir_store_local(&a, 3, SIR_DTREF, NULL,
        sir_new_array(&a, SIR_ATINT, sir_load_local(&a, 1, SIR_DTINT, NULL)), ret);
    sir_node_t* header = sir_nop(&a, NULL);
    sir_node_t* incC = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), header);         /* count++; back edge */
    sir_node_t* M    = sir_nop(&a, incC);
    sir_node_t* brP  = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        incC, M);                                                /* if (p != 0) count++ */
    sir_node_t* brCond = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 10, SIR_DTINT)),           /* while n < 10-ish */
        brP, mkarr);
    sir_set_next(header, brCond);
    sir_node_t* initC = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), header);               /* count = 0 */
    sir_method_t* m = sir_method(&a, "f", 0, 3, 4, initC);

    compiler_fact_t facts[1] = {
        (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = header,
                           .aux = header, .a = COMPILER_SCOPE_LOOP },
    };
    cp_engine_t* e = cp_build(m, NULL, &a, facts, 1);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, alen);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->constant.state != CP_C_KNOWN,
        "the arraylen follower's STORED constant kept a stale optimistic KNOWN after its "
        "leader (the count φ) descended — the un-re-armed off-graph fact (L-REARM-6)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* γ_K FAIL-CLOSED pins (Click Fig 3.2/3.3: f(⊤,·)=⊤ — "we do not propagate information
 * until all the facts are known" — and BOTTOM means NO INFORMATION, never [0,0]). The
 * shared bounds accessor read BOTTOM's zeroed payload as a real [0,0] interval, so
 * `GE(0, BOTTOM)` folded KNOWN TRUE — which folded a §15 IDX_HIGH guard to ALWAYS-THROW
 * (the initProperties miscompile: the property set's builder trapped, every lookup fell
 * to miss/default). A fold may claim a verdict ONLY from KNOWN/RANGE operands. */
static void test_cp_unit_fold_bottom_and_top_are_not_facts(void) {
    cp_const_t bot; memset(&bot, 0, sizeof bot); bot.state = CP_C_BOTTOM;
    cp_const_t top; memset(&top, 0, sizeof top); top.state = CP_C_TOP;
    cp_const_t k0; memset(&k0, 0, sizeof k0);
    k0.state = CP_C_KNOWN; k0.cwidth = CP_W_I32; k0.value = 0; k0.lvalue = 0;

    /* The exact miscompiled fold: 0 >= BOTTOM must be NO CLAIM, never TRUE. */
    cp_const_t r = sir_op_gamma[SIR_GE].fold_cmp_range(SIR_GE, k0, bot);
    TEST_ASSERT_TRUE_MESSAGE(r.state != CP_C_KNOWN,
        "GE(0, BOTTOM) folded to a KNOWN verdict — BOTTOM's payload read as [0,0]; "
        "this is the always-throw guard fold");
    r = sir_op_gamma[SIR_LT].fold_cmp_range(SIR_LT, bot, k0);
    TEST_ASSERT_TRUE_MESSAGE(r.state != CP_C_KNOWN,
        "LT(BOTTOM, 0) must not fold — no operand fact, no verdict");
    /* Arithmetic: BOTTOM in ⟹ BOTTOM out; TOP in ⟹ TOP out (C2's convention). */
    r = sir_op_gamma[SIR_ADD].fold_binary_range(bot, k0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_BOTTOM, r.state,
        "ADD(BOTTOM, 0) must be BOTTOM — never a value computed from the zeroed payload");
    r = sir_op_gamma[SIR_ADD].fold_binary_range(top, k0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_TOP, r.state,
        "ADD(TOP, 0) must be TOP — do not propagate until all facts are known");
    r = sir_op_gamma[SIR_GE].fold_cmp_range(SIR_GE, top, k0);
    TEST_ASSERT_TRUE_MESSAGE(r.state != CP_C_KNOWN && r.state != CP_C_BOTTOM,
        "CMP with a TOP operand must stay TOP — a BOTTOM claim here would defeat optimism");
}

/* ── §5-D unit pins: each range-lattice operator tested in ISOLATION ──
 * (PoPA §4.2 pieces — join/meet/widen — so a failure names the exact broken
 * operator instead of a whole-loop black box.) */
static cp_const_t cp_ut_R(int64_t lo, int64_t hi) {
    cp_const_t c; memset(&c, 0, sizeof c);
    c.state = CP_C_RANGE; c.cwidth = CP_W_I32; c.lo = lo; c.hi = hi; c.stride = 1;
    return c;
}
static cp_const_t cp_ut_K(int32_t v) {
    cp_const_t c; memset(&c, 0, sizeof c);
    c.state = CP_C_KNOWN; c.cwidth = CP_W_I32; c.value = v; c.lvalue = v;
    return c;
}

/* PIECE 1 — the JOIN is the interval hull. */
static void test_cp_unit_meet_is_interval_hull(void) {
    cp_const_t r = cp_const_meet(cp_ut_K(0), cp_ut_K(1));
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, r.state, "hull of 0 and 1 is a range");
    TEST_ASSERT_TRUE_MESSAGE(r.lo == 0 && r.hi == 1, "[0,0] ⊔ [1,1] = [0,1]");
    r = cp_const_meet(cp_ut_R(0, 5), cp_ut_R(3, 10));
    TEST_ASSERT_TRUE_MESSAGE(r.lo == 0 && r.hi == 10, "[0,5] ⊔ [3,10] = [0,10]");
    cp_const_t top; memset(&top, 0, sizeof top); top.state = CP_C_TOP;
    cp_const_t bot; memset(&bot, 0, sizeof bot); bot.state = CP_C_BOTTOM;
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_KNOWN, cp_const_meet(top, cp_ut_K(5)).state,
        "TOP is the join identity: TOP ⊔ 5 = 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_BOTTOM, cp_const_meet(bot, cp_ut_K(5)).state,
        "BOTTOM absorbs in this engine's convention");
}

/* PIECE 2 — the NARROWING meet is intersection (branch refinement uses it). */
static void test_cp_unit_intersect_narrows(void) {
    cp_const_t r = cp_const_intersect(cp_ut_R(0, 10), cp_ut_R(5, INT32_MAX));
    TEST_ASSERT_TRUE_MESSAGE(r.lo == 5 && r.hi == 10,
        "[0,10] ⊓ [5,MAX] = [5,10] — the lower bound is narrowed up");
}

/* PIECE 3 — WIDENING keeps a lower bound that does not drop (PoPA §4.2.1). This is the
 * operator the counter's `i>=0` rests on; a failure here is the widen, a pass here means
 * the counter bug is upstream (the value fed INTO widen), not the operator. */
static void test_cp_unit_widen_keeps_lower_bound(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_const_t r = cp_const_widen(e, cp_ut_R(0, 0), cp_ut_R(0, 1));
    TEST_ASSERT_TRUE_MESSAGE(r.lo == 0,
        "widen([0,0],[0,1]) keeps lo=0 — new.lo does not drop below old.lo");
    cp_const_t r2 = cp_const_widen(e, cp_ut_R(0, 100), cp_ut_R(0, 200));
    TEST_ASSERT_TRUE_MESSAGE(r2.lo == 0,
        "ascending only on the high side keeps lo=0");
    /* And the CONVERSE, so the pin isn't vacuous: a genuine drop DOES widen down. */
    cp_const_t r3 = cp_const_widen(e, cp_ut_R(0, 5), cp_ut_R(-3, 5));
    TEST_ASSERT_TRUE_MESSAGE(r3.lo < 0,
        "widen([0,5],[-3,5]) drops lo (new.lo below old.lo) — widening is real");
    cp_free(e);
    bbq_arena_free(&a);
}

/* PIECE 4 — the branch refinement produces a bounded RANGE on a body load:
 * on the `i < 5` true edge, a load of i must carry a range with hi = 4. This is the
 * value the INC's input reads; if it's not bounded here, the increment overflows. */
static void test_cp_unit_branch_refine_bounds_load(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (param) */
    sir_node_t* iload = sir_load_local(&a, 0, SIR_DTINT, NULL);   /* i in the true arm */
    sir_node_t* br = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 5, SIR_DTINT)),               /* i < 5 */
        sir_return(&a, iload, SIR_DTINT),                          /* true: i refined `< 5` */
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vi = cp_vnode_for(e, iload);
    TEST_ASSERT_NOT_NULL(vi);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, vi->constant.state,
        "on the `i < 5` true edge, i is a bounded RANGE");
    TEST_ASSERT_TRUE_MESSAGE(vi->constant.hi == 4,
        "`i < 5` refines i to hi = 4 (the upper bound the increment needs)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* PIECE 5 — the SAME operators on i64 (long), NOT just i32: no JavaCard int-only
 * assumption. Bounds beyond the i32 range must survive; widen must key on cwidth. */
static cp_const_t cp_ut_R64(int64_t lo, int64_t hi) {
    cp_const_t c; memset(&c, 0, sizeof c);
    c.state = CP_C_RANGE; c.cwidth = CP_W_I64; c.lo = lo; c.hi = hi; c.stride = 1;
    return c;
}
static void test_cp_unit_i64_range_ops(void) {
    /* hull on i64, with bounds beyond INT32 range */
    cp_const_t r = cp_const_meet(cp_ut_R64(0, 5000000000LL), cp_ut_R64(3, 10));
    TEST_ASSERT_TRUE_MESSAGE(r.lo == 0 && r.hi == 5000000000LL,
        "i64 hull keeps a bound past INT32_MAX (no i32 truncation)");
    /* narrowing on i64 */
    cp_const_t n = cp_const_intersect(cp_ut_R64(0, 5000000000LL),
                                      cp_ut_R64(1000000000LL, 9000000000LL));
    TEST_ASSERT_TRUE_MESSAGE(n.lo == 1000000000LL && n.hi == 5000000000LL,
        "i64 intersect narrows with >INT32 bounds");
    /* widen on i64 keeps lo, grows hi — must not collapse to an i32 extreme */
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1,
        sir_return(&a, sir_load_long_const(&a, 0), SIR_DTLONG));
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_const_t w = cp_const_widen(e, cp_ut_R64(0, 100), cp_ut_R64(0, 5000000000LL));
    TEST_ASSERT_TRUE_MESSAGE(w.lo == 0, "i64 widen keeps lo=0");
    TEST_ASSERT_TRUE_MESSAGE(w.hi >= 5000000000LL,
        "i64 widen grows hi past INT32_MAX (to the i64 extreme), not truncated");
    cp_free(e);
    bbq_arena_free(&a);
}

/* PIECE 6 — the INC TRANSFER in isolation (§5-D induction, no loop yet): with `i < 5`
 * refining the input to hi=4, `i++` computes hi=5. Tests `range(input)+delta` as a pure
 * analysis transfer — i32 and i64, so no int-only assumption. */
static void test_cp_unit_inc_transfer_i32(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = i (param); on the `i < 5` true edge, i++ = refined-i + 1 */
    sir_node_t* iafter = sir_load_local(&a, 0, SIR_DTINT, NULL);   /* i AFTER the inc */
    sir_node_t* ret = sir_return(&a, iafter, SIR_DTINT);
    sir_node_t* inc = sir_inc(&a, 0, 1, SIR_DTINT,
        sir_load_local(&a, 0, SIR_DTINT, NULL), ret);             /* i++; next = ret */
    sir_node_t* br = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 5, SIR_DTINT)),             /* i < 5 */
        inc,
        sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT));
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vi = cp_vnode_for(e, iafter);
    TEST_ASSERT_NOT_NULL(vi);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, vi->constant.state,
        "post-inc i is a RANGE (the transfer tracked it, not opaque)");
    TEST_ASSERT_TRUE_MESSAGE(vi->constant.hi == 5,
        "i<5 refines input to hi=4, so i++ has hi=5");
    cp_free(e);
    bbq_arena_free(&a);
}
static void test_cp_unit_inc_transfer_i64(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* iafter = sir_load_local(&a, 0, SIR_DTLONG, NULL);
    sir_node_t* ret = sir_return(&a, iafter, SIR_DTLONG);
    sir_node_t* inc = sir_inc(&a, 0, 1, SIR_DTLONG,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), ret);
    sir_node_t* br = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 0, SIR_DTLONG, NULL),
                   sir_load_long_const(&a, 5)),                   /* i < 5L */
        inc,
        sir_return(&a, sir_load_long_const(&a, 0), SIR_DTLONG));
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* vi = cp_vnode_for(e, iafter);
    TEST_ASSERT_NOT_NULL(vi);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CP_C_RANGE, vi->constant.state,
        "post-inc long i is a RANGE — the transfer is cwidth-general, not int-only");
    TEST_ASSERT_TRUE_MESSAGE(vi->constant.hi == 5,
        "long i<5 refines to hi=4, so i++ has hi=5");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── §5.1 primitive-conversion constant folding ────────────────────── */

static void test_cp_fold_i2l(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret = sir_return(&a, sir_i2_l(&a, sir_load_const(&a, 5, SIR_DTINT)), SIR_DTLONG);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);
    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLONGCONST, v->tag, "(long)5 folds to 5L");
    TEST_ASSERT_TRUE(v->load_long_const.value == 5);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_l2i_truncates(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* (int)0x1_0000_0005L = 5 (low 32 bits). */
    sir_node_t* ret = sir_return(&a, sir_l2_i(&a, sir_load_long_const(&a, 0x100000005LL)), SIR_DTINT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);
    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, v->tag, "(int)long folds to i32");
    TEST_ASSERT_EQUAL_INT(5, v->load_const.value);
    cp_free(e); bbq_arena_free(&a);
}

static void test_cp_fold_i2d_and_f2d(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* r1 = sir_return(&a, sir_i2_d(&a, sir_load_const(&a, 3, SIR_DTINT)), SIR_DTDOUBLE);
    sir_method_t* m1 = sir_method(&a, "f", 0, 0, 1, r1);
    cp_engine_t* e1 = cp_build(m1, NULL, &a, NULL, 0); cp_rewrite(e1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADDOUBLECONST, r1->return_.value->tag, "(double)3 folds");
    TEST_ASSERT_TRUE(r1->return_.value->load_double_const.value == 3.0);
    cp_free(e1);
    sir_node_t* r2 = sir_return(&a, sir_f2_d(&a, sir_load_float_const(&a, 1.5f)), SIR_DTDOUBLE);
    sir_method_t* m2 = sir_method(&a, "f", 0, 0, 1, r2);
    cp_engine_t* e2 = cp_build(m2, NULL, &a, NULL, 0); cp_rewrite(e2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADDOUBLECONST, r2->return_.value->tag, "(double)1.5f folds");
    TEST_ASSERT_TRUE(r2->return_.value->load_double_const.value == 1.5);
    cp_free(e2); bbq_arena_free(&a);
}

static void test_cp_fold_i2c_masks(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* (char)65537 = 1 (zero-extend low 16). */
    sir_node_t* ret = sir_return(&a, sir_i2_c(&a, sir_load_const(&a, 65537, SIR_DTINT)), SIR_DTCHAR);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0); cp_rewrite(e);
    sir_node_t* v = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, v->tag, "(char)i folds to i32");
    TEST_ASSERT_EQUAL_INT(1, v->load_const.value);
    cp_free(e); bbq_arena_free(&a);
}

/* JLS §5.1.3 double→int: NaN→0, ≥MAX/+inf→INT_MAX, ≤MIN/-inf→INT_MIN,
 * else round toward zero — NOT a C cast. */
static int32_t fold_d2i(bbq_arena* a, double x) {
    sir_node_t* ret = sir_return(a, sir_d2_i(a, sir_load_double_const(a, x)), SIR_DTINT);
    sir_method_t* m = sir_method(a, "f", 0, 0, 1, ret);
    cp_engine_t* e = cp_build(m, NULL, a, NULL, 0); cp_rewrite(e);
    sir_node_t* v = ret->return_.value;
    int32_t r = (v->tag == SIR_LOADCONST) ? v->load_const.value : 0x7fffffff;
    cp_free(e);
    return r;
}
static void test_cp_fold_d2i_jls_narrowing(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    TEST_ASSERT_EQUAL_INT_MESSAGE(3,           fold_d2i(&a, 3.7),    "trunc toward zero");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-3,          fold_d2i(&a, -3.7),   "trunc toward zero (neg)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2147483647,  fold_d2i(&a, 1e30),   "overflow → INT_MAX");
    TEST_ASSERT_EQUAL_INT_MESSAGE((int32_t)-2147483648LL, fold_d2i(&a, -1e30), "underflow → INT_MIN");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2147483647,  fold_d2i(&a, INFINITY),  "+inf → INT_MAX");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0,           fold_d2i(&a, NAN),    "NaN → 0");
    bbq_arena_free(&a);
}

/* ── UCE branch / switch fold completeness (cp_rewrite_branch_fold) ─ */

/* Click §4.3 UCE: a Branch whose condition's constant is KNOWN-true
 * collapses to a Goto pointing at the true arm — symmetric to the
 * existing KNOWN-false test (test_cp_rewrite_branch_fold). */
static void test_cp_rewrite_branch_fold_true_arm(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a,
        sir_load_const(&a, 5, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a,
        sir_load_const(&a, 7, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond  = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 0, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, br->tag,
        "constant-true Branch folds to Nop whose .next is the on_true arm");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ret_t, br->nop.next,
        "fold target is the true arm");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Negative pin — an opaque condition cannot be folded; the Branch
 * stays a Branch. A pass that folds opaque branches would be
 * unsound (picks an arm without proving the other is dead). */
static void test_cp_rewrite_opaque_branch_preserved(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a,
        sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a,
        sir_load_const(&a, 2, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);  /* opaque */
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_BRANCH, br->tag,
        "opaque-condition Branch must NOT fold");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Transitive constant — a slot stored with a KNOWN value: the
 * LoadLocal reading that slot resolves to the constant via the
 * reaching-def edge, the cond fact propagates, the Branch folds.
 * Tests cp_compute_facts → cp_rewrite_expr → cp_rewrite_branch_fold. */
static void test_cp_rewrite_branch_fold_transitive_constant(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a,
        sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a,
        sir_load_const(&a, 2, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_node_t* st    = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
                            sir_load_const(&a, 1, SIR_DTSHORT), br);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, br->tag,
        "Branch on LoadLocal of known-constant slot folds to Nop");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ret_t, br->nop.next,
        "fold target matches the constant's truth value");
    cp_free(e);
    bbq_arena_free(&a);
}

/* UCE through Switch: a KNOWN selector matching a case value
 * makes the other cases and the default unreachable; the Switch
 * folds to a Goto pointing at the matching case. */
static void test_cp_rewrite_switch_fold_known_selector(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret0 = sir_return(&a,
        sir_load_const(&a, 10, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret1 = sir_return(&a,
        sir_load_const(&a, 11, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* retD = sir_return(&a,
        sir_load_const(&a, 99, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t** cases = (sir_node_t**)bbq_arena_alloc(&a,
                            2 * sizeof(sir_node_t*));
    cases[0] = ret0; cases[1] = ret1;
    int32_t* vals = (int32_t*)bbq_arena_alloc(&a, 2 * sizeof(int32_t));
    vals[0] = 0; vals[1] = 1;
    sir_node_t* sel = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* sw  = sir_switch(&a, sel, cases, 2, vals, 2, retD,
                                 SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 0, sw);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, sw->tag,
        "Switch with KNOWN selector matching a case folds to Nop");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ret1, sw->nop.next,
        "fold target is the matching case arm");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── §4.8 algebraic 1-constant identities (OUTPUT) ─────────────── */

/* Mul(x, 1) is the identity on x (Click §4.8 — commutative both
 * sides; Mul(1, x) folds the same way). */
static void test_cp_rewrite_mul_one_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c1  = sir_load_const(&a, 1, SIR_DTSHORT);
    sir_node_t* mul = sir_mul(&a, SIR_DTSHORT, ll, c1);
    sir_node_t* ret = sir_return(&a, mul, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL,
        ret->return_.value->tag, "Mul(x, 1) must rewrite to x");
    TEST_ASSERT_EQUAL_PTR(ll, ret->return_.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* And(x, -1) is the identity on x (all-ones mask preserves all
 * bits). Click §4.8 — commutative. */
static void test_cp_rewrite_and_neg_one_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* cN1 = sir_load_const(&a, -1, SIR_DTSHORT);
    sir_node_t* an  = sir_and(&a, SIR_DTSHORT, ll, cN1);
    sir_node_t* ret = sir_return(&a, an, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL,
        ret->return_.value->tag, "And(x, -1) must rewrite to x");
    TEST_ASSERT_EQUAL_PTR(ll, ret->return_.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Sub(x, 0) is the right-side-only identity on x — Sub is NOT
 * commutative, so Sub(0, x) is NOT an identity (it's −x and must
 * NOT fold to x; that case is for unary negation, not §4.8). */
static void test_cp_rewrite_sub_zero_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* su  = sir_sub(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, su, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL,
        ret->return_.value->tag, "Sub(x, 0) must rewrite to x");
    TEST_ASSERT_EQUAL_PTR(ll, ret->return_.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Shl(x, 0) is the right-side-only identity (shift by zero
 * preserves all bits). Click §4.8 — shifts list a one-sided
 * identity at the shift-amount position. */
static void test_cp_rewrite_shl_zero_identity(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* sh  = sir_shl(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, sh, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL,
        ret->return_.value->tag, "Shl(x, 0) must rewrite to x");
    TEST_ASSERT_EQUAL_PTR(ll, ret->return_.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* §4.6 sub-of-congruent at the OUTPUT level: Sub(x, y) with x ≡ y
 * folds to LoadConst 0. Two LoadLocals of the same slot are
 * congruent via the slot-collapse path (cp_ultimate_value), so
 * cp_node_const yields KNOWN 0 and the rewrite substitutes a
 * LoadConst. (The state-level test_cp_sub_of_congruent pins the
 * engine fact; this pins the OUTPUT.) */
static void test_cp_rewrite_sub_of_congruent_to_zero(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* l1  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* l2  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* su  = sir_sub(&a, SIR_DTSHORT, l1, l2);
    sir_node_t* ret = sir_return(&a, su, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST,
        ret->return_.value->tag,
        "Sub(x, x) must rewrite to LoadConst 0 (§4.6 sub-of-congruent)");
    TEST_ASSERT_EQUAL_INT(0, ret->return_.value->load_const.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Absorbing constant with purity (yoctojc, not vanilla §4.8):
 * Mul(x, 0) where x is pure rewrites to LoadConst 0. The purity
 * gate distinguishes this from Mul(impure_x, 0) where the side
 * effect of x must still run — that case is pinned by the existing
 * test_cp_absorb_mul_zero_impure_preserved (state side). */
static void test_cp_rewrite_absorb_mul_zero_pure_to_const(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ll  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* c0  = sir_load_const(&a, 0, SIR_DTSHORT);
    sir_node_t* mu  = sir_mul(&a, SIR_DTSHORT, ll, c0);
    sir_node_t* ret = sir_return(&a, mu, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST,
        ret->return_.value->tag,
        "Mul(x, 0) with pure x must rewrite to LoadConst 0");
    TEST_ASSERT_EQUAL_INT(0, ret->return_.value->load_const.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Constant fold depth (cp_compute_facts cascade) ────────────── */

/* Nested constant Add: cp_compute_facts is a monotone fixpoint
 * on a finite-height lattice, so cascading folds settle in one
 * solve — Add(Add(2,3), 4) → 9 with no rewrite re-iteration. */
static void test_cp_rewrite_nested_constant_fold(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* c3  = sir_load_const(&a, 3, SIR_DTSHORT);
    sir_node_t* c4  = sir_load_const(&a, 4, SIR_DTSHORT);
    sir_node_t* in  = sir_add(&a, SIR_DTSHORT, c2, c3);
    sir_node_t* out = sir_add(&a, SIR_DTSHORT, in, c4);
    sir_node_t* ret = sir_return(&a, out, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 0, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST,
        ret->return_.value->tag,
        "nested constant Add must fold to a single LoadConst");
    TEST_ASSERT_EQUAL_INT(9, ret->return_.value->load_const.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* Constant fold through Cmp: with both operands KNOWN, the Cmp
 * folds to its boolean result (0/1). Validates that cp_fold_cmp
 * is reached via cp_node_const for compare opcodes. */
static void test_cp_rewrite_constant_fold_cmp(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* c2  = sir_load_const(&a, 2, SIR_DTSHORT);
    sir_node_t* c3  = sir_load_const(&a, 3, SIR_DTSHORT);
    sir_node_t* c5  = sir_load_const(&a, 5, SIR_DTSHORT);
    sir_node_t* ad  = sir_add(&a, SIR_DTSHORT, c2, c3);
    sir_node_t* cm  = sir_eq(&a, ad, c5);
    sir_node_t* ret = sir_return(&a, cm, SIR_DTSHORT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 0, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST,
        ret->return_.value->tag,
        "Cmp(KNOWN, KNOWN) must fold to its boolean LoadConst");
    TEST_ASSERT_EQUAL_INT(1, ret->return_.value->load_const.value);
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── NOP / Goto compaction (cp_rewrite_compact_nops_gotos) ─────── */

/* A linear NOP chain — single predecessors, no jump opcode
 * referencing them — is plumbing left by ddcg's backpatch
 * landing pads. Compaction reroutes the predecessor's .next
 * through the chain so post-rewrite spine DFS bypasses them. */
static void test_cp_rewrite_compact_linear_nop_chain(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* nop3 = sir_nop(&a, ret);
    sir_node_t* nop2 = sir_nop(&a, nop3);
    sir_node_t* nop1 = sir_nop(&a, nop2);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, nop1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(ret, nop1->nop.next,
        "linear non-merge NOPs must splice through to next real node");
    cp_free(e);
    bbq_arena_free(&a);
}

/* A NOP with 2+ predecessors is a control-flow merge — its PC
 * is the join the verifier checks stack-depth at. cp_rewrite_
 * compact_nops_gotos must NOT splice it out. */
static void test_cp_rewrite_compact_preserves_merge_nop(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret = sir_return(&a,
        sir_load_local(&a, 1, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* st_t  = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
                            sir_load_const(&a, 5, SIR_DTSHORT), merge);
    sir_node_t* st_f  = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
                            sir_load_const(&a, 7, SIR_DTSHORT), merge);
    sir_node_t* cond  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br    = sir_branch(&a, cond, st_t, st_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, merge->tag,
        "merge NOP (2+ predecessors) must NOT be spliced");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ret, merge->nop.next,
        "merge NOP's own .next is still preserved through Return");
    cp_free(e);
    bbq_arena_free(&a);
}

/* A Branch arm pointing at Goto1 → Goto2 → Return: jump compaction
 * follows the chain via cp_follow_nops_gotos_keep_merges and re-
 * points br.on_true at Return directly, so codegen emits one jump
 * opcode instead of three. */
/* A NOP referenced by a Branch arm (or Switch case) is the backpatch
 * destination — the jump's offset resolves to its position. Even with
 * a single predecessor, the NOP must be preserved so the destination
 * stays stable. */
static void test_cp_rewrite_compact_preserves_jump_target_nop(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret    = sir_return(&a,
        sir_load_const(&a, 0, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* target = sir_nop(&a, ret);
    sir_node_t* other  = sir_return(&a,
        sir_load_const(&a, 9, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br     = sir_branch(&a, cond, target, other);
    sir_method_t* m    = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, target->tag,
        "NOP referenced by a jump opcode must be preserved "
        "(backpatch landing pad)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* TryRegion.handler is the exception-table target — the JVM
 * consults the table on throw and lands at the handler's PC.
 * TryRegion.next is the try-body entry, also exception-table-
 * referenced via the table's start_pc. Both NOPs must survive. */
static void test_cp_rewrite_compact_preserves_tryregion_handler(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_h = sir_return(&a,
        sir_load_const(&a, 0, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_b = sir_return(&a,
        sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* handler_nop = sir_nop(&a, ret_h);
    sir_node_t* body_nop    = sir_nop(&a, ret_b);
    sir_node_t* tr = sir_try_region(&a, handler_nop, body_nop);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 0, tr);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, handler_nop->tag,
        "TryRegion.handler NOP must be preserved");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NOP, body_nop->tag,
        "TryRegion.next NOP must be preserved");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(handler_nop, tr->try_region.handler,
        "TryRegion.handler edge intact");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(body_nop, tr->try_region.next,
        "TryRegion.next edge intact");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Slot bin-packing (cp_pack) ────────────────────────────────── */

/* Two same-dt locals with disjoint live ranges must share one
 * frame cell after packing. cp_pack with sema=NULL uses the
 * fourth arg as args_cells fallback — passing 1 anchors slot 0
 * (the param) and leaves slots 1, 2 packable. */
static void test_cp_pack_coalesces_disjoint_same_dt(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* slot 0 = p; slot 1 = a = p+1 (dies at slot-2 init);
     * slot 2 = b = a+2 (live to Return). */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* st_b = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 1, SIR_DTSHORT, NULL),
            sir_load_const(&a, 2, SIR_DTSHORT)),
        ret);
    sir_node_t* st_a = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_const(&a, 1, SIR_DTSHORT)),
        st_b);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st_a);

    cp_pack(m, NULL, &a, 1);   /* args_cells = 1 (slot 0 anchored) */

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, m->max_locals,
        "two disjoint same-dt locals must coalesce to one cell "
        "(1 param + 1 packed local)");
    bbq_arena_free(&a);
}

/* Two locals whose live ranges overlap interfere — both must
 * survive packing. With the live-at-Return assertion present
 * the lifetimes overlap by construction. */
static void test_cp_pack_preserves_overlapping_same_dt(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* a + b returned → both slots live to Return. */
    sir_node_t* sum = sir_add(&a, SIR_DTSHORT,
                         sir_load_local(&a, 1, SIR_DTSHORT, NULL),
                         sir_load_local(&a, 2, SIR_DTSHORT, NULL));
    sir_node_t* ret = sir_return(&a, sum, SIR_DTSHORT);
    sir_node_t* st_b = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_const(&a, 1, SIR_DTSHORT)),
        ret);
    sir_node_t* st_a = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), st_b);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st_a);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, m->max_locals,
        "overlapping locals must NOT coalesce");
    bbq_arena_free(&a);
}

/* Chaitin's DEF rule (Chaitin '82): the slot a store DEFINES interferes with
 * every slot live-out at that store — even when the store itself is DEAD.
 * live_out×live_out alone misses it: a dead store builds no live range, so
 * its slot shows disjoint from everything, coalesces into a live range, and
 * the dead WRITE becomes a live clobber of the merged cell. The shape:
 *   slot1 ← p+1        a := ...
 *   slot2 ← 0          DEAD (shadowed below before any read) — a live-across
 *   slot3 ← slot1+1    a's last use
 *   slot2 ← p+2        the real def of slot2 (a is dead by here)
 *   return slot3+slot2
 * slot2's LIVE range is disjoint from slot1's, but the dead store writes
 * inside it: packing them together turns `a` into 0 at the slot3 read. */
static void test_cp_pack_dead_store_still_interferes(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret  = sir_return(&a,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 3, SIR_DTSHORT, NULL),
            sir_load_local(&a, 2, SIR_DTSHORT, NULL)), SIR_DTSHORT);
    sir_node_t* st2b = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_const(&a, 2, SIR_DTSHORT)),
        ret);
    sir_node_t* st3  = sir_store_local(&a, 3, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 1, SIR_DTSHORT, NULL),
            sir_load_const(&a, 1, SIR_DTSHORT)),
        st2b);
    sir_node_t* st2a = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_load_const(&a, 0, SIR_DTSHORT),
        st3);
    sir_node_t* st1  = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_const(&a, 1, SIR_DTSHORT)),
        st2a);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 4, st1);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_MESSAGE(
        st1->store_local.slot != st2a->store_local.slot,
        "a DEAD store's slot must still interfere with slots live-out at the "
        "store (Chaitin's def rule) — coalescing it into a live range turns "
        "the dead write into a clobber of the merged cell");
    bbq_arena_free(&a);
}

/* WASM lowers byte/short/char/int all to i32, so disjoint locals of
 * those types coalesce into one i32 local. Two slots used as byte and
 * short respectively, with disjoint live ranges, share a single i32
 * local. */
static void test_cp_pack_byte_short_coalesce_as_i32(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* slot 0 = short param; slot 1 = byte; slot 2 = short. slot 1 dies
     * before slot 2 is stored (disjoint live ranges) → the two coalesce
     * into one i32 local: param + one shared cell = 2. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* st_s = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_const(&a, 1, SIR_DTSHORT)),
        ret);
    /* Last use of slot 1 as byte. */
    sir_node_t* ee_b = sir_expr_effect(&a,
        sir_load_local(&a, 1, SIR_DTBYTE, NULL), 1, st_s);
    sir_node_t* st_b = sir_store_local(&a, 1, SIR_DTBYTE, NULL,
        sir_load_const(&a, 7, SIR_DTBYTE), ee_b);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 3, st_b);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, m->max_locals,
        "byte and short are both i32 on WASM — disjoint locals coalesce");
    bbq_arena_free(&a);
}

/* The rename walk must reach EVERY node of the value DAG, however deep:
 * a LoadLocal at the bottom of a chain with >4096 pending nodes still
 * gets its packed-slot rename. A bounded DFS stack silently drops the
 * deep renames, leaving a stale local index past max_locals. */
static void test_cp_pack_renames_deep_value_chain(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 20);
    /* slot 0 = param; slot 5 = the only local (packs to cell 1). The
     * chain is right-deep — Add(const, Add(const, …(LoadLocal 5)…)) —
     * so the pending-constant side of the DFS grows past 4096. */
    enum { DEPTH = 6000 };
    sir_node_t* leaf = sir_load_local(&a, 5, SIR_DTSHORT, NULL);
    sir_node_t* e = leaf;
    for (int i = 0; i < DEPTH; i++)
        e = sir_add(&a, SIR_DTSHORT, sir_load_const(&a, 1, SIR_DTSHORT), e);
    sir_node_t* ret = sir_return(&a, e, SIR_DTSHORT);
    sir_node_t* st  = sir_store_local(&a, 5, SIR_DTSHORT, NULL,
                                      sir_load_const(&a, 7, SIR_DTSHORT), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 6, st);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, m->max_locals,
        "one anchored param + one packed local");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, leaf->load_local.slot,
        "the deep LoadLocal is renamed into the packed frame (slot 5 -> 1)");
    bbq_arena_free(&a);
}

/* cp_pack must rename slot references under EVERY spine op — the
 * ArrayCopy / SetHeader / MemStore8 operands included (the arraycopy
 * intrinsic broke exactly here: its operand loads kept stale pre-pack
 * indices, so the jre's System.ac* family emitted unfixable locals). */
static void test_cp_pack_renames_arraycopy_operands(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = param; ref temps at 4 and 5 (1-3 unused, so the packer
     * moves them down); the ArrayCopy operands are their only reads. */
    sir_node_t* ld_dst = sir_load_local(&a, 4, SIR_DTREF, NULL);
    sir_node_t* ld_src = sir_load_local(&a, 5, SIR_DTREF, NULL);
    sir_node_t* ret = sir_return_void(&a);
    sir_node_t* ac  = sir_array_copy(&a, SIR_DTINT,
                                     ld_dst, sir_load_const(&a, 0, SIR_DTINT),
                                     ld_src, sir_load_const(&a, 0, SIR_DTINT),
                                     sir_load_const(&a, 3, SIR_DTINT), ret);
    sir_node_t* st5 = sir_store_local(&a, 5, SIR_DTREF, NULL,
                                      sir_load_local(&a, 0, SIR_DTREF, NULL), ac);
    sir_node_t* st4 = sir_store_local(&a, 4, SIR_DTREF, NULL,
                                      sir_load_local(&a, 0, SIR_DTREF, NULL), st5);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 6, st4);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_TRUE_MESSAGE(ld_dst->load_local.slot < m->max_locals,
        "ArrayCopy dst operand renamed into the packed frame");
    TEST_ASSERT_TRUE_MESSAGE(ld_src->load_local.slot < m->max_locals,
        "ArrayCopy src operand renamed into the packed frame");
    bbq_arena_free(&a);
}

/* A v128 slot must land INSIDE the packed frame. The pack pools slots by
 * lat_dt_valtype — six valtypes since LAT_VT_V128 — so a pool table sized to
 * five is an OOB write and renames v128 slots past max_locals ("unknown
 * local" at VM load; found by the Click-ON gate over Mem.copyIn(V128[])). */
static void test_cp_pack_v128_slot_in_frame(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ld_v    = sir_load_local(&a, 4, SIR_DTV128, NULL);
    sir_node_t* ld_addr = sir_load_local(&a, 5, SIR_DTINT, NULL);
    sir_node_t* ret = sir_return_void(&a);
    sir_node_t* ms  = sir_simd_mem_store(&a, WOP_V128_STORE, 4, ld_addr, ld_v, ret);
    sir_node_t* st5 = sir_store_local(&a, 5, SIR_DTINT, NULL,
                                      sir_load_const(&a, 64, SIR_DTINT), ms);
    sir_node_t* st4 = sir_store_local(&a, 4, SIR_DTV128, NULL,
                                      sir_simd_splat_i(&a, WOP_I32X4_SPLAT,
                                          sir_load_const(&a, 7, SIR_DTINT)), st5);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 6, st4);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_TRUE_MESSAGE(ld_v->load_local.slot < m->max_locals,
        "v128 operand slot renamed INSIDE the packed frame");
    TEST_ASSERT_TRUE_MESSAGE(ld_addr->load_local.slot < m->max_locals,
        "addr operand slot renamed inside the packed frame");
    bbq_arena_free(&a);
}

static void test_cp_pack_renames_setheader_memstore_operands(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ld_obj  = sir_load_local(&a, 4, SIR_DTREF, NULL);
    sir_node_t* ld_addr = sir_load_local(&a, 5, SIR_DTINT, NULL);
    sir_node_t* ret = sir_return_void(&a);
    sir_node_t* ms  = sir_mem_store_i(&a, WOP_I32_STORE8, 0, ld_addr,
                                      sir_load_const(&a, 7, SIR_DTINT), ret);
    sir_node_t* sh  = sir_set_header(&a, ld_obj, sir_load_local(&a, 0, SIR_DTREF, NULL), 3, ms);
    sir_node_t* st5 = sir_store_local(&a, 5, SIR_DTINT, NULL,
                                      sir_load_const(&a, 64, SIR_DTINT), sh);
    sir_node_t* st4 = sir_store_local(&a, 4, SIR_DTREF, NULL,
                                      sir_load_local(&a, 0, SIR_DTREF, NULL), st5);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 6, st4);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_TRUE_MESSAGE(ld_obj->load_local.slot < m->max_locals,
        "SetHeader obj operand renamed into the packed frame");
    TEST_ASSERT_TRUE_MESSAGE(ld_addr->load_local.slot < m->max_locals,
        "MemStoreI addr operand renamed into the packed frame");
    bbq_arena_free(&a);
}

/* A slot whose ONLY use is an ArrayCopy operand is live — liveness/DSE
 * must see the use, or the defining store gets deleted and the copy
 * reads an uninitialized slot. */
static void test_cp_arraycopy_operand_keeps_store_live(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ret = sir_return_void(&a);
    sir_node_t* ac  = sir_array_copy(&a, SIR_DTINT,
                                     sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT),
                                     sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT),
                                     sir_load_local(&a, 1, SIR_DTINT, NULL), ret);
    /* len temp: an opaque non-leaf value (never copy-forwarded). */
    sir_node_t* st1 = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT,
                sir_load_local(&a, 2, SIR_DTINT, NULL),
                sir_load_const(&a, 1, SIR_DTINT)), ac);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 3, st1);

    topt(m, &a);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_STORELOCAL, (int)st1->tag,
        "the store feeding an ArrayCopy operand must survive DSE");
    bbq_arena_free(&a);
}

/* ArrayCopy WRITES the array cells of its width: two otherwise-congruent
 * ArrayLoads straddling it read different memory states and must NOT
 * collapse to one partition. */
static void test_cp_arraycopy_invalidates_array_cell(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ld1 = sir_array_load(&a, SIR_DTINT,
                                     sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT), NULL);
    sir_node_t* ld2 = sir_array_load(&a, SIR_DTINT,
                                     sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT), NULL);
    sir_node_t* ret = sir_return(&a, ld2, SIR_DTINT);
    sir_node_t* ac  = sir_array_copy(&a, SIR_DTINT,
                                     sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT),
                                     sir_load_local(&a, 1, SIR_DTREF, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT),
                                     sir_load_const(&a, 3, SIR_DTINT), ret);
    sir_node_t* ee1 = sir_expr_effect(&a, ld1, 1, ac);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, ee1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v1 = cp_vnode_for(e, ld1);
    cp_vnode_t* v2 = cp_vnode_for(e, ld2);
    TEST_ASSERT_NOT_NULL(v1);
    TEST_ASSERT_NOT_NULL(v2);
    TEST_ASSERT_TRUE_MESSAGE(v1->partition != v2->partition,
        "reads straddling an ArrayCopy of the same width must not merge");

    cp_free(e);
    bbq_arena_free(&a);
}

/* A WASM local is TYPED: two ref slots may share one local only when
 * their threaded referent descriptors agree. Distinct referents must
 * stay in distinct cells even with disjoint live ranges (String()V
 * broke here: a String-typed cell was reused for an exception ref). */
static void test_cp_pack_refs_coalesce_only_same_referent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = param; slots 1, 2 = ref locals with DISJOINT ranges but
     * different classes (class 3 vs class 4). */
    sir_node_t* ref3a = sir_class_ref(&a, 3);
    sir_node_t* ref3b = sir_class_ref(&a, 3);
    sir_node_t* ref4  = sir_class_ref(&a, 4);
    sir_node_t* ret  = sir_return_void(&a);
    sir_node_t* ee2  = sir_expr_effect(&a,
        sir_load_local(&a, 2, SIR_DTREF, ref4), 1, ret);
    sir_node_t* st2  = sir_store_local(&a, 2, SIR_DTREF, ref4,
        sir_load_local(&a, 0, SIR_DTREF, ref3a), ee2);
    sir_node_t* ee1  = sir_expr_effect(&a,
        sir_load_local(&a, 1, SIR_DTREF, ref3b), 1, st2);
    sir_node_t* st1  = sir_store_local(&a, 1, SIR_DTREF, ref3b,
        sir_load_local(&a, 0, SIR_DTREF, ref3a), ee1);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st1);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, m->max_locals,
        "ref slots with DIFFERENT referents must not coalesce");
    bbq_arena_free(&a);
}

static void test_cp_pack_refs_coalesce_same_referent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* Same shape, but both locals are class 3 — they may share a cell. */
    sir_node_t* ref3 = sir_class_ref(&a, 3);
    sir_node_t* ret  = sir_return_void(&a);
    sir_node_t* ee2  = sir_expr_effect(&a,
        sir_load_local(&a, 2, SIR_DTREF, ref3), 1, ret);
    sir_node_t* st2  = sir_store_local(&a, 2, SIR_DTREF, ref3,
        sir_load_local(&a, 0, SIR_DTREF, ref3), ee2);
    sir_node_t* ee1  = sir_expr_effect(&a,
        sir_load_local(&a, 1, SIR_DTREF, ref3), 1, st2);
    sir_node_t* st1  = sir_store_local(&a, 1, SIR_DTREF, ref3,
        sir_load_local(&a, 0, SIR_DTREF, ref3), ee1);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st1);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, m->max_locals,
        "ref slots with the SAME referent coalesce into one cell");
    bbq_arena_free(&a);
}

/* Two loop counters that advance in LOCKSTEP on one arm but diverge on
 * the other are NOT congruent — initProperties' counting loop:
 *     for (i = 0; i < total; i++) if (pred) count++;
 * φ(i) and φ(count) must end in different partitions, the two Inc
 * nodes must keep distinct slots through the whole pipeline, and the
 * count read after the loop must not be rewritten into i. */
static void test_cp_lockstep_counters_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = total (param), slot 3 = pred (param);
     * slot 1 = i, slot 2 = count. ONE shared latch Inc(i) — the merge
     * point of both arms, exactly the ddcg `for` shape. */
    sir_node_t* ret   = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT);
    sir_node_t* head  = sir_nop(&a, NULL);
    sir_node_t* inc_i = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), head);
    sir_node_t* inc_c = sir_inc(&a, 2, 1, SIR_DTINT,
        sir_load_local(&a, 2, SIR_DTINT, NULL), inc_i);
    sir_node_t* cond_i = sir_load_local(&a, 1, SIR_DTINT, NULL);
    sir_node_t* br2 = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 3, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)),
        inc_c, inc_i);
    sir_node_t* br1 = sir_branch(&a,
        sir_lt(&a, cond_i,
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),
        br2, ret);
    sir_set_next(head, br1);
    sir_node_t* st_c = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), head);
    sir_node_t* st_i = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), st_c);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 4, st_i);

    topt(m, &a);

    TEST_ASSERT_TRUE_MESSAGE(inc_i->inc.slot != inc_c->inc.slot,
        "i++ and count++ keep distinct slots");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, (int)br1->branch.cond->lt.left->tag,
        "loop condition still reads a local");
    TEST_ASSERT_EQUAL_INT_MESSAGE(inc_i->inc.slot,
        br1->branch.cond->lt.left->load_local.slot,
        "the loop condition reads i's slot, never count's");
    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, (int)rv->tag,
        "return reads a local");
    TEST_ASSERT_EQUAL_INT_MESSAGE(inc_c->inc.slot, rv->load_local.slot,
        "the post-loop count read stays count, not i");
    bbq_arena_free(&a);
}

/* StringTokenizer.skipDelimiters shape: a while whose && chain re-reads a
 * FIELD the body advances —
 *     while (cur < max && (cur % 3) == 0) cur = cur + 1;
 * Both condition arms read GetField(cur); the body's PutField writes it.
 * Every in-loop read of that cell must observe the loop's memory φ. If
 * CSE/forwarding materializes ONE pre-loop read into a temp and any arm
 * reads the temp, that arm is frozen at iteration 0 — the jre tokenizer
 * consumed every char as a delimiter. Pins: no StoreLocal on the entry
 * chain (before the loop header) captures a read of the written field. */
static bool tree_reads_field(const sir_node_t* e, int field_idx) {
    if (!e) return false;
    if (e->tag == SIR_GETFIELD && e->get_field.field_idx == field_idx)
        return true;
    for (int i = 0; i < sir_arity(e); i++)
        if (tree_reads_field(sir_child((sir_node_t*)e, i), field_idx))
            return true;
    return false;
}

/* The EXACT ddcg lowering of that while (dumped from the frontend): field
 * reads are spilled through temp SLOTS and the same temp (s1) is reused for
 * `cur` in every arm —
 *     head: if (ret) exit
 *           s1=GF(cur); s2=GF(max); if (!(s1<s2)) exit
 *           s1=GF(cur);             if (!((s1%3)==0)) exit
 *           s1=GF(cur); s1=s1+1; PF(cur)=s1; goto head
 * Click folded the rem-eq branch to constant TRUE (the tokenizer consumed
 * every char). The rem test must survive optimization. */
static bool spine_has_branch_with(const sir_node_t* n, int tag, int depth) {
    for (; n && depth > 0; depth--) {
        if (n->tag == SIR_BRANCH) {
            const sir_node_t* stk[32]; int sp = 0;
            stk[sp++] = n->branch.cond;
            while (sp > 0) {
                const sir_node_t* e = stk[--sp];
                if (!e) continue;
                if ((int)e->tag == tag) return true;
                for (int i = 0; i < sir_arity(e) && sp < 30; i++)
                    stk[sp++] = sir_child((sir_node_t*)e, i);
            }
            if (spine_has_branch_with(sir_succ((sir_node_t*)n, 0), tag, depth - 1))
                return true;
            n = sir_succ((sir_node_t*)n, 1);
            continue;
        }
        if (sir_succ_count(n) == 0) return false;
        n = sir_succ((sir_node_t*)n, 0);
    }
    return false;
}

/* The MINIMAL fold repro (dumped raw from the frontend):
 *     while ((cur & 1) == 0) { cur = cur + 1; }     // cur a FIELD
 * lowers to
 *     head: s4=GF(cur); s3=And(s4,1); if (s3==0) body else exit
 *     body: s2=GF(cur); s1=s2+1; PF(cur)=s1; goto head
 * Click folded the branch to constant TRUE → an unconditional infinite
 * loop. The test reads a field the body advances — it must survive. */
static void test_cp_field_loop_and_test_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    #define THIS sir_load_this(&a, SIR_DTREF, 1)
    #define GF() sir_get_field(&a, SIR_DTINT, THIS, 1, 0)
    #define LL(s) sir_load_local(&a, (s), SIR_DTINT, NULL)
    sir_node_t* exitn = sir_nop(&a, sir_nop(&a, sir_return_void(&a)));
    sir_node_t* head  = sir_nop(&a, NULL);
    sir_node_t* pf    = sir_put_field(&a, SIR_DTINT, THIS, 1, 0, LL(1), head);
    sir_node_t* s1st  = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT, LL(2), sir_load_const(&a, 1, SIR_DTINT)), pf);
    sir_node_t* s2st  = sir_store_local(&a, 2, SIR_DTINT, NULL, GF(), s1st);
    sir_node_t* barm  = sir_nop(&a, s2st);
    sir_node_t* br    = sir_branch(&a,
        sir_eq(&a, LL(3), sir_load_const(&a, 0, SIR_DTINT)), barm, exitn);
    sir_node_t* s3st  = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_and(&a, SIR_DTINT, LL(4), sir_load_const(&a, 1, SIR_DTINT)), br);
    sir_node_t* s4st  = sir_store_local(&a, 4, SIR_DTINT, NULL, GF(), s3st);
    sir_set_next(head, s4st);
    #undef LL
    #undef GF
    #undef THIS
    sir_method_t* m = sir_method(&a, "skip", 0, 0, 5, head);

    topt(m, &a);

    TEST_ASSERT_TRUE_MESSAGE(spine_has_branch_with(m->entry, SIR_EQ, 64),
        "the (cur & 1) == 0 loop test must survive — cur is a field the "
        "body advances");
    bbq_arena_free(&a);
}

static void test_cp_tokenizer_rem_branch_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = this; slots 1,2 = ddcg spill temps. Class 1 fields:
     * f0 = cur, f1 = max, f2 = ret. */
    #define THIS sir_load_this(&a, SIR_DTREF, 1)
    #define GF(f, dt) sir_get_field(&a, (dt), THIS, 1, (f))
    #define LL(s) sir_load_local(&a, (s), SIR_DTINT, NULL)
    sir_node_t* exitn = sir_nop(&a, sir_return_void(&a));
    sir_node_t* head  = sir_nop(&a, NULL);
    /* body: s1=GF(cur); s1=s1+1; PF(cur)=s1 → head */
    sir_node_t* pf    = sir_put_field(&a, SIR_DTINT, THIS, 1, 0, LL(1), head);
    sir_node_t* sadd  = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT, LL(1), sir_load_const(&a, 1, SIR_DTINT)), pf);
    sir_node_t* sldb  = sir_store_local(&a, 1, SIR_DTINT, NULL,
        GF(0, SIR_DTINT), sadd);
    /* arm 3: s1=GF(cur); if ((s1 % 3) == 0) body else exit */
    sir_node_t* br3   = sir_branch(&a,
        sir_eq(&a, sir_rem(&a, SIR_DTINT, LL(1), sir_load_const(&a, 3, SIR_DTINT)),
                   sir_load_const(&a, 0, SIR_DTINT)),
        sldb, exitn);
    sir_node_t* sld3  = sir_store_local(&a, 1, SIR_DTINT, NULL,
        GF(0, SIR_DTINT), br3);
    /* arm 2: s1=GF(cur); s2=GF(max); if (s1 < s2) arm3 else exit */
    sir_node_t* br2   = sir_branch(&a, sir_lt(&a, LL(1), LL(2)), sld3, exitn);
    sir_node_t* sld2b = sir_store_local(&a, 2, SIR_DTINT, NULL,
        GF(1, SIR_DTINT), br2);
    sir_node_t* sld2a = sir_store_local(&a, 1, SIR_DTINT, NULL,
        GF(0, SIR_DTINT), sld2b);
    /* arm 1: if (ret) exit else arm2 */
    sir_node_t* br1   = sir_branch(&a, GF(2, SIR_DTBYTE), exitn, sld2a);
    sir_set_next(head, br1);
    sir_node_t* entry = sir_nop(&a, head);
    #undef LL
    #undef GF
    #undef THIS
    sir_method_t* m = sir_method(&a, "skip", 0, 0, 3, entry);

    topt(m, &a);

    TEST_ASSERT_TRUE_MESSAGE(spine_has_branch_with(m->entry, SIR_REM, 64),
        "the (cur %% 3) == 0 loop-condition arm must survive — cur is a "
        "field the loop body advances; the test is not loop-invariant");
    bbq_arena_free(&a);
}

static void test_cp_field_loop_cond_arm_not_frozen(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = this. Fields of class 1: 0 = cur, 1 = max. */
    #define THIS sir_load_local(&a, 0, SIR_DTREF, NULL)
    #define GF(f) sir_get_field(&a, SIR_DTINT, THIS, 1, (f))
    sir_node_t* ret  = sir_return(&a, GF(0), SIR_DTINT);
    sir_node_t* head = sir_nop(&a, NULL);
    sir_node_t* body = sir_put_field(&a, SIR_DTINT, THIS, 1, 0,
        sir_add(&a, SIR_DTINT, GF(0), sir_load_const(&a, 1, SIR_DTINT)), head);
    sir_node_t* br2 = sir_branch(&a,
        sir_eq(&a, sir_rem(&a, SIR_DTINT, GF(0), sir_load_const(&a, 3, SIR_DTINT)),
                   sir_load_const(&a, 0, SIR_DTINT)),
        body, ret);
    sir_node_t* br1 = sir_branch(&a, sir_lt(&a, GF(0), GF(1)), br2, ret);
    sir_set_next(head, br1);
    sir_node_t* entry = sir_nop(&a, head);
    #undef GF
    #undef THIS
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, entry);

    topt(m, &a);

    /* Walk the straight entry chain up to the loop header: no spliced
     * StoreLocal there may capture a read of field 0 (`cur`) — inside
     * the loop that temp would be iteration-0's value forever. */
    for (sir_node_t* n = m->entry; n && n != head; n = sir_get_next(n)) {
        if (n->tag == SIR_STORELOCAL)
            TEST_ASSERT_FALSE_MESSAGE(
                tree_reads_field(n->store_local.value, 0),
                "a pre-loop temp must not capture GetField(cur) — "
                "in-loop reads would freeze at iteration 0");
        if (n->tag == SIR_BRANCH) break;   /* left the entry chain */
    }
    bbq_arena_free(&a);
}

/* Two GetFields of DIFFERENT fields after a call: the invoke's wide
 * memory write shadows every cell with ONE opaque, so both loads carry
 * identical (obj, memory) inputs — but they read different fields and
 * are never value-equal. Field identity is part of the operator; the
 * initial buckets must separate on it or `cur < max` folds reflexively
 * to false (StringTokenizer.hasMoreTokens returned constant false). */
static void test_cp_getfields_of_distinct_fields_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    #define THIS sir_load_this(&a, SIR_DTREF, 1)
    #define LL(s) sir_load_local(&a, (s), SIR_DTINT, NULL)
    sir_node_t* ret  = sir_return(&a, sir_lt(&a, LL(1), LL(2)), SIR_DTBYTE);
    sir_node_t* s2st = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_get_field(&a, SIR_DTINT, THIS, 1, 1), ret);
    sir_node_t* s1st = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_get_field(&a, SIR_DTINT, THIS, 1, 0), s2st);
    sir_node_t* call = sir_expr_effect(&a,
        sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, s1st);
    #undef LL
    #undef THIS
    sir_method_t* m = sir_method(&a, "more", 0, 0, 3, call);

    topt(m, &a);

    /* The compare must survive — under the bug it folded to `return 0`. */
    TEST_ASSERT_TRUE_MESSAGE(ret->return_.value->tag != SIR_LOADCONST,
        "GetField(cur) < GetField(max) must not fold — distinct fields "
        "are distinct values even with identical memory inputs");
    bbq_arena_free(&a);
}

/* Same op, same variable operand, DIFFERENT constant operand — the shape of
 * every big-endian byte splitter (`(v >>> 24) & 0xFF`, `(v >>> 16) & 0xFF`,
 * …). All three Ushr nodes carry the same FACT (BOTTOM: v is unknown), so
 * cp_split_by_facts_one can never separate them; only CAUSE_SPLITS, splitting by
 * the partition feeding input 1, can. When a fact-driven split of the shared
 * LoadConst partition failed to re-arm the CAUSE_SPLITS worklist, the three
 * stayed congruent and CSE collapsed them into one — DataOutputStream.writeInt
 * emitted the top byte four times. Pins that the three shifts stay distinct. */
/* ── Lattice A: points-to (combined-analysis stage 1) ───────────────
 *
 * pts names one abstract object per allocation SITE. These pin the value-flow
 * transfer rules from combined-analysis-spec.md §2, over the REAL SIR tags. */

/* `C a = new C(); C b = a;` — a copy carries the allocation's object, and the
 * set is a SINGLETON (which is what licenses a strong update downstream). */
static void test_pts_new_flows_through_copy(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* alloc = sir_new(&a, 1);
    sir_node_t* ret = sir_return(&a, sir_load_local(&a, 1, SIR_DTREF, NULL), SIR_DTREF);
    sir_node_t* s1  = sir_store_local(&a, 1, SIR_DTREF, NULL,
        sir_load_local(&a, 0, SIR_DTREF, NULL), ret);          /* b = a */
    sir_node_t* s0  = sir_store_local(&a, 0, SIR_DTREF, NULL, alloc, s1);  /* a = new C */
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, s0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int o = cp_obj_of(e, alloc);
    TEST_ASSERT_TRUE_MESSAGE(o >= e->obj_first_site,
        "the New names an abstract object (an allocation SITE, past the phantoms)");
    cp_pts_t p = cp_pts_of_expr(e, ret->return_.value);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, o),
        "pts(b) contains the object a was allocated from");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, cp_pts_count(e, p),
        "pts(b) is a SINGLETON — one allocation site reaches it");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "a freshly allocated ref is never null");
    cp_free(e);
    bbq_arena_free(&a);
}

/* `C x = c ? new C() : new C();` — a φ JOINS its inputs' pts (spec §2 ASSIGN),
 * so the merge sees BOTH sites and is no longer a singleton: no strong update
 * through x, but still provably non-null. */
static void test_pts_phi_joins_both_allocations(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* n1 = sir_new(&a, 1);
    sir_node_t* n2 = sir_new(&a, 1);
    sir_node_t* ret = sir_return(&a, sir_load_local(&a, 1, SIR_DTREF, NULL), SIR_DTREF);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* t = sir_store_local(&a, 1, SIR_DTREF, NULL, n1, merge);
    sir_node_t* f = sir_store_local(&a, 1, SIR_DTREF, NULL, n2, merge);
    sir_node_t* br = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), t, f);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, ret->return_.value);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, n1)), "phi sees the then-arm's object");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, n2)), "phi sees the else-arm's object");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, cp_pts_count(e, p),
        "a merge of two allocations is NOT a singleton — no strong update through it");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "both arms allocate, so the merge is still non-null");
    cp_free(e);
    bbq_arena_free(&a);
}

/* `C x = c ? new C() : null;` — ⊥null is an object like any other, and its
 * presence in pts is EXACTLY the nullability question (spec §4). */
static void test_pts_null_is_an_object(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* n1 = sir_new(&a, 1);
    sir_node_t* ret = sir_return(&a, sir_load_local(&a, 1, SIR_DTREF, NULL), SIR_DTREF);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* t = sir_store_local(&a, 1, SIR_DTREF, NULL, n1, merge);
    sir_node_t* f = sir_store_local(&a, 1, SIR_DTREF, NULL, sir_load_null(&a), merge);
    sir_node_t* br = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), t, f);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, ret->return_.value);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "the null arm puts the null object in pts — the ref is Maybe-null");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, n1)),
        "the allocating arm is still there too");
    cp_free(e);
    bbq_arena_free(&a);
}

/* A formal PARAMETER points to an EXTERNAL object — not to ∅ (which would read as
 * "unreachable" and wrongly license optimization) and not to any allocation site
 * in this method. Spec §1 gives it its OWN phantom ("one per (site, type)"), so
 * the object it names is external but is NOT the shared catch-all. */
static void test_pts_param_is_external(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ret = sir_return(&a, sir_load_local(&a, 0, SIR_DTREF, NULL), SIR_DTREF);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, ret->return_.value);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, e->obj_of_slot[0]),
        "an incoming parameter names ITS OWN external phantom (spec §1 Oext@param)");
    TEST_ASSERT_TRUE_MESSAGE(e->obj_of_slot[0] < e->obj_first_site,
        "…and that phantom is EXTERNAL — it is not an allocation site of this method");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_empty(e, p),
        "a parameter's pts is NOT empty — empty means unreachable, which would "
        "wrongly license optimization");
    /* SOUNDNESS. An incoming ref may be null, so ⊥null must be in its pts —
     * otherwise nullability reads it as NonNull and deletes the NPE guard that
     * was doing its job. CP_OBJ_EXT alone means "some unknown NON-null object";
     * anything genuinely unknown must carry ⊥null too. */
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "a parameter MAY BE NULL — its pts must contain the null object, or its "
        "NPE guard gets wrongly eliminated");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Spec §1: "`Oext@param` — a phantom/external object for anything reachable from a
 * formal parameter or a global … ONE PER (SITE, TYPE)."
 *
 * THE point of a phantom per parameter: two DIFFERENT incoming references are two
 * different abstract objects. With one shared `Oext` they were the same object, so
 * every unknown was forced to alias every other unknown, and nothing downstream
 * could tell two parameters apart. */
static void test_pts_two_params_are_distinct_objects(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ld_p = sir_load_local(&a, 0, SIR_DTREF, NULL);
    sir_node_t* ld_q = sir_load_local(&a, 1, SIR_DTREF, NULL);
    sir_node_t* ret  = sir_return(&a, ld_q, SIR_DTREF);
    sir_node_t* st   = sir_store_local(&a, 2, SIR_DTREF, NULL, ld_p, ret);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t pp = cp_pts_of_expr(e, ld_p);
    cp_pts_t pq = cp_pts_of_expr(e, ld_q);
    TEST_ASSERT_TRUE_MESSAGE(e->obj_of_slot[0] != e->obj_of_slot[1],
        "two parameters get two DIFFERENT phantoms (one per site — §1)");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, pp, e->obj_of_slot[0])
                          && !cp_pts_has(e, pp, e->obj_of_slot[1]),
        "param p names p's phantom and NOT q's — two unknowns are not forced to alias");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, pq, e->obj_of_slot[1])
                          && !cp_pts_has(e, pq, e->obj_of_slot[0]),
        "and q names q's");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The consequence, and the reason the phantom is worth having: a store through one
 * parameter must not be seen by a load through ANOTHER parameter. With one shared
 * `Oext` this was impossible to express — both were the same object, so the store
 * polluted the load. */
static void test_pts_store_through_one_param_not_seen_through_another(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);                          /* p.f = new X() */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 0);          /* …then read q.f */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, ret);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, put);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "a store through param p is NOT seen by a load through param q — distinct "
        "phantoms are what makes that expressible (§1)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS, and the rule the phantoms force: §2's strong update fires "iff pts(p)
 * is a singleton {O}" where O is ONE CONCRETE OBJECT (VFG Rule 3 / Theorem 3) — NOT
 * merely a set of size one. pts(param) is now a singleton phantom, but two params
 * MAY ALIAS at runtime, so strongly updating one while leaving the other stale
 * would be unsound. A store through a phantom must stay WEAK. */
static void test_pts_store_through_param_is_weak_not_strong(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* y    = sir_new(&a, 4);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);          /* p.f, after both */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, ret);  /* p.f = y */
    sir_node_t* put1 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2); /* p.f = x */
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, put1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)),
        "the last store's value is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "and so is the FIRST one: a store through a PHANTOM must be WEAK. The set is "
        "a singleton, but a phantom is not one concrete object — another parameter "
        "may alias it, so killing the prior value would be unsound (§2, VFG Thm 3)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS — this pins a LATENT MISCOMPILE that the single shared `Oext` had, and
 * that only doing §1's phantoms properly exposed.
 *
 * `this` used to be `pts = {CP_OBJ_EXT}` — a set of size ONE — so `this.f = x` took
 * the STRONG path and KILLED `heap[EXT]`. But `EXT` was the one object that every
 * parameter, static and call result ALSO named. So a store through `this` wiped the
 * field contents of every unknown object in the method, and a later `p.f` (param p)
 * read back `{x}` instead of "unknown". The analysis would then call `p.f` NON-NULL
 * and delete its NPE guard — and if `p != this` at runtime with `p.f == null`, the
 * WASM traps where Java must throw NullPointerException.
 *
 * A may-analysis may only ever OVER-approximate. Killing a field of an object that
 * stands for many objects loses possibilities, and losing possibilities is exactly
 * the direction that licenses a wrong optimization. */
static void test_pts_store_through_this_does_not_kill_a_params_field(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);                          /* this.f = new X() */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);          /* …then read p.f   */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_this(&a, SIR_DTREF, 2), 1, 0, x, ret);
    sir_method_t* m  = sir_method(&a, "f", 2, 0, 1, put);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "p.f may still be NULL after a store through `this` — a store through one "
        "unknown must not kill another unknown's field, or the NPE guard on p.f gets "
        "deleted and the WASM traps where Java must throw");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_count(e, p) == 1 && cp_pts_has(e, p, cp_obj_of(e, x)),
        "…and p.f is certainly not PROVABLY the object stored through `this`");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS — §2's strong update, the other kind of non-concrete object.
 *
 * "Strong update (replace, not ∪) iff pts(p) is a singleton {O}" (VFG Rule 3 /
 * Theorem 3) means O is ONE CONCRETE RUNTIME OBJECT. Obj naming is 1-LIMITED — one
 * per allocation SITE — so a site INSIDE A LOOP is a SUMMARY of every object it
 * ever produces. pts through it is a set of size one, but that one name stands for
 * many objects, and killing "its" field kills the field of every object the site
 * ever made — including the previous iteration's, which may still be live.
 *
 *   while (…) { d = new D(); d.f = x; escape(d); }   // d.f = x must be WEAK
 *
 * Here: the site is in a loop, and the SAME site is stored into twice. If the
 * second store strong-updates, the first store's value is gone — but at runtime
 * those are two DIFFERENT objects, and the first one still holds x.
 *
 * Which sites are "in a loop" comes from the DDCG's RECORDED loop scopes — spec §8:
 * "read the loop scope from the sidecar, not a dominator-based natural-loop finder."
 * No loop finder. No dominance. */
static void test_pts_store_through_loop_site_is_weak_not_strong(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x     = sir_new(&a, 3);
    sir_node_t* y     = sir_new(&a, 4);
    sir_node_t* alloc = sir_new(&a, 2);                       /* d = new D() — IN the loop */
    sir_node_t* load  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);        /* d.f, after the loop       */
    sir_node_t* ret   = sir_return(&a, load, SIR_DTREF);
    sir_node_t* exit_ = sir_nop(&a, ret);
    sir_node_t* dec   = sir_store_local(&a, 1, SIR_DTINT, NULL,
                          sir_sub(&a, SIR_DTINT, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                      sir_load_const(&a, 1, SIR_DTINT)), NULL);
    sir_node_t* put2  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, dec);   /* d.f = y */
    sir_node_t* put1  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2);  /* d.f = x */
    sir_node_t* body  = sir_store_local(&a, 0, SIR_DTREF, NULL, alloc, put1);
    sir_node_t* head  = sir_branch(&a,
                          sir_ne(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT)),
                          body, exit_);
    dec->store_local.next = head;                                /* the back edge */
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, head);

    /* The facts the DDCG would have RECORDED: the loop SCOPE, and each allocation
     * site with a = 1 (can run more than once) because it is lowering the loop's
     * body. The optimizer READS both; it never derives either. */
    compiler_fact_t facts[4];
    memset(facts, 0, sizeof facts);
    facts[0] = (compiler_fact_t){ .kind = COMPILER_FACT_SCOPE, .key = head,
                                  .aux = exit_, .a = COMPILER_SCOPE_LOOP };
    facts[1] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = alloc, .a = 1 };
    facts[2] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = x,     .a = 1 };
    facts[3] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = y,     .a = 1 };

    cp_engine_t* e = tbuild_facts(&a, m, facts, 4);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)),
        "the last store's value is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "and so is the FIRST one: the allocation site is INSIDE A LOOP, so it is a "
        "SUMMARY of many objects — a strong update through it would kill a field of "
        "an object that is not the one being stored to (§2, VFG Thm 3)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Fail-closed for the above: a site OUTSIDE any loop executes at most once, so it IS
 * one concrete object and the strong update must still fire. Losing this would make
 * the whole heap analysis weak, and every store would pile up forever. */
static void test_pts_store_through_non_loop_site_is_still_strong(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x     = sir_new(&a, 3);
    sir_node_t* y     = sir_new(&a, 4);
    sir_node_t* alloc = sir_new(&a, 2);                       /* d = new D() — NO loop */
    sir_node_t* load  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret   = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, ret);
    sir_node_t* put1  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2);
    sir_node_t* d     = sir_store_local(&a, 0, SIR_DTREF, NULL, alloc, put1);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, d);

    /* Recorded by the DDCG with a = 0: this site runs at most once. */
    compiler_fact_t facts[3];
    memset(facts, 0, sizeof facts);
    facts[0] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = alloc, .a = 0 };
    facts[1] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = x,     .a = 0 };
    facts[2] = (compiler_fact_t){ .kind = COMPILER_FACT_ALLOC, .key = y,     .a = 0 };

    cp_engine_t* e = tbuild_facts(&a, m, facts, 3);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)),
        "the last store's value is there");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "and the FIRST one is GONE: a site outside any loop runs at most once, so it "
        "is one concrete object and §2's strong update applies");
    cp_free(e);
    bbq_arena_free(&a);
}

/* SOUNDNESS — the ⊥null row. Reading or writing a field of `null` THROWS (JLS
 * §15.11); it does not produce a value and it does not write one. So the null object
 * is never a store TARGET and never a load SOURCE, even though it sits in the pts of
 * every may-be-null reference (that is a nullability fact, not an object).
 *
 * Get this wrong and every may-be-null receiver shares one channel: a store through
 * `p` lands in `heap[⊥null]` and a load through an unrelated `q` reads it straight
 * back out. The single shared `Oext` hid this for as long as it existed — everything
 * aliased everything, so polluting the null row changed nothing observable. */
static void test_pts_null_is_never_a_store_target_or_load_source(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* Both p and q are parameters, so both are MAY-BE-NULL: pts = {phantom, ⊥null}.
     * The only object they share is ⊥null. If that row carried values, q.f would see
     * what was stored through p.f. */
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 0);          /* q.f */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, ret);  /* p.f = x */
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, put);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    /* The receivers really do share ⊥null — otherwise this pin proves nothing. */
    cp_pts_t pp = cp_pts_of_expr(e, sir_child(put, 0));
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, pp, CP_OBJ_NULL),
        "precondition: the store's receiver is a param, so it MAY be null");
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "a store through a may-be-null p must NOT reach a load through a may-be-null "
        "q: ⊥null is not an object you can store into (JLS §15.11 — it throws)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The IMMUTABLE-CELL rule (`lat_is_array_data_cell`) CANNOT be pinned here, and the
 * attempt to do so was VACUOUS — recording that, because a vacuous pin is worse than
 * no pin: it reports a rule as covered when nothing tests it.
 *
 * The rule drops the memory edge of a read of the array overlay's backing-store field
 * from VALUE IDENTITY (that field is written once, at allocation, and no Java program
 * can name it — §10.7 gives an array only `length`, which is final). It is the most
 * dangerous rule in the analysis, because it deliberately makes two loads congruent
 * ACROSS AN INTERVENING STORE.
 *
 * It cannot live in this suite for two reasons, both structural:
 *   - the real lowering is `ArrayLength(GetField(data, a))`, and a hand-built SIR that
 *     says `ArrayLength(a)` has no memory edge at all, so the rule is not exercised;
 *   - the rule asks the TYPE LATTICE which cell is the backing store, and this harness
 *     builds engines with `sema = NULL`, so it can never fire.
 *
 * It IS pinned, at the level where it can be: `test_sir` §13's bounds pins compile real
 * Java, and disabling the rule turns `r[i] = a[i]` with `r = new int[a.length]` RED.
 * The fail-closed direction — a MUTABLE field must NOT be congruent across a store to
 * it — is pinned in `test_sir` §15. */

/* Spec §1, the OTHER half of `Oext@param`: "a phantom/external object for anything
 * REACHABLE FROM a formal parameter or a global … one per (site, type)."
 *
 * Each PARAMETER got its own phantom. It did not give one to what those
 * parameters POINT AT — a phantom's fields still read as the single shared catch-all,
 * so `p.f` and `p.g` and every other unknown field were one object again: the very
 * collapse the parameter phantoms were fixing, one level down.
 *
 * Two reads of DIFFERENT fields of an unknown object are different unknowns. */
static void test_pts_fields_of_an_unknown_are_distinct_unknowns(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* p is a parameter (unknown). Read p.f and p.g — two different cells. */
    sir_node_t* rf  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);          /* p.f */
    sir_node_t* rg  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 1);          /* p.g */
    sir_node_t* ret = sir_return(&a, rg, SIR_DTREF);
    sir_node_t* st  = sir_store_local(&a, 1, SIR_DTREF, NULL, rf, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t pf = cp_pts_of_expr(e, rf);
    cp_pts_t pg = cp_pts_of_expr(e, rg);

    /* Both are unknown and both may be null — that part must not regress. */
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, pf, CP_OBJ_NULL)
                          && cp_pts_has(e, pg, CP_OBJ_NULL),
        "a field of an unknown object MAY BE NULL — its NPE guard must survive");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_empty(e, pf),
        "…and it is not ∅, which would read as unreachable");

    /* …but they are not the SAME unknown. Today both are the shared catch-all, so
     * `p.f` and `p.g` alias, and a store through one is seen through the other. */
    bool same = true;
    for (int o = 0; o < e->obj_count; o++)
        if (o != CP_OBJ_NULL && cp_pts_has(e, pf, o) != cp_pts_has(e, pg, o)) same = false;
    TEST_ASSERT_FALSE_MESSAGE(same,
        "two DIFFERENT fields of an unknown object are two DIFFERENT unknowns — §1's "
        "phantom is per (site, type), not one object for everything reachable");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The device BOUNDS THE RECURSION — that is what "one per (site, type)" is FOR. A
 * chain `p.f.f.f` must not mint a new phantom per link, or the object set is
 * unbounded and Obj naming stops being finite (which the whole lattice relies on:
 * pts is a bitset over Obj, and it terminates because Obj is finite).
 *
 * Reading `f` of the phantom that `p.f` named must yield THE SAME phantom. */
static void test_pts_phantom_recursion_is_bounded(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* r1  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);          /* p.f      */
    sir_node_t* r2  = sir_get_field(&a, SIR_DTREF, r1, 1, 0);   /* (p.f).f  */
    sir_node_t* r3  = sir_get_field(&a, SIR_DTREF, r2, 1, 0);   /* ((p.f).f).f */
    sir_node_t* ret = sir_return(&a, r3, SIR_DTREF);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p1 = cp_pts_of_expr(e, r1);
    cp_pts_t p3 = cp_pts_of_expr(e, r3);
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_empty(e, p3),
        "a chained read of an unknown is still an unknown, not ∅");
    for (int o = 0; o < e->obj_count; o++)
        TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p1, o) == cp_pts_has(e, p3, o),
            "p.f and p.f.f.f name the SAME phantom — one per (site, type) is what "
            "bounds the recursion and keeps the Obj set finite");
    cp_free(e);
    bbq_arena_free(&a);
}

/* FAIL-CLOSED: a phantom is still an UNKNOWN object. It may be null, and it must
 * never be strongly updated — two unknowns may alias at runtime. */
static void test_pts_store_through_a_field_phantom_is_weak(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* y    = sir_new(&a, 4);
    /* q = p.f  (an unknown);  q.g = x;  q.g = y;  return q.g; */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 1);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 1, y, ret);
    sir_node_t* put1 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 1, x, put2);
    sir_node_t* q    = sir_store_local(&a, 1, SIR_DTREF, NULL,
        sir_get_field(&a, SIR_DTREF, sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0),
        put1);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, q);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)),
        "the last store's value is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "and so is the FIRST: a store through a PHANTOM stays WEAK — a phantom is an "
        "unknown object, not one concrete object, and another unknown may alias it");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Spec §1's `Oret@callee` — "the abstract *callee returned some ref* object for
 * bottom methods". A call whose body this analysis cannot see still returns SOME
 * object, and the spec names it BY THE CALLEE. Two calls to the same callee name the
 * same object; calls to different callees name different ones. Without it every call
 * result is the single catch-all, so `a.foo()` and `b.bar()` alias each other and a
 * store through one is read back through the other.
 *
 * (This is a pts fact, never a value fact. `foo()` twice is not the same VALUE,
 * and the congruence pins below still hold.) */
static void test_pts_two_calls_to_the_same_callee_name_the_same_object(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* c1  = sir_invoke_static(&a, 7, 3, NULL, 0, SIR_DTREF);
    sir_node_t* c2  = sir_invoke_static(&a, 7, 3, NULL, 0, SIR_DTREF);
    sir_node_t* ret = sir_return(&a, c2, SIR_DTREF);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTREF, NULL, c1, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    int o1 = cp_obj_of(e, c1), o2 = cp_obj_of(e, c2);
    TEST_ASSERT_TRUE_MESSAGE(o1 >= 0,
        "a call result NAMES an object — §1's Oret@callee, not the shared catch-all");
    TEST_ASSERT_EQUAL_INT_MESSAGE(o1, o2,
        "Oret is named by the CALLEE: two calls to the same callee are the same "
        "abstract object, which is what keeps the Obj set finite");
    cp_free(e);
    bbq_arena_free(&a);
}

static void test_pts_calls_to_different_callees_name_different_objects(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* c1  = sir_invoke_static(&a, 7, 3, NULL, 0, SIR_DTREF);   /* C7.m3 */
    sir_node_t* c2  = sir_invoke_static(&a, 7, 4, NULL, 0, SIR_DTREF);   /* C7.m4 */
    sir_node_t* c3  = sir_invoke_static(&a, 8, 3, NULL, 0, SIR_DTREF);   /* C8.m3 */
    sir_node_t* ret = sir_return(&a, c3, SIR_DTREF);
    sir_node_t* s2  = sir_store_local(&a, 1, SIR_DTREF, NULL, c2, ret);
    sir_node_t* s1  = sir_store_local(&a, 0, SIR_DTREF, NULL, c1, s2);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, s1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    int o1 = cp_obj_of(e, c1), o2 = cp_obj_of(e, c2), o3 = cp_obj_of(e, c3);
    TEST_ASSERT_TRUE_MESSAGE(o1 >= 0 && o2 >= 0 && o3 >= 0, "each call names an object");
    TEST_ASSERT_TRUE_MESSAGE(o1 != o2,
        "a different METHOD of the same class is a different callee");
    TEST_ASSERT_TRUE_MESSAGE(o1 != o3,
        "the same method index on a different CLASS is a different callee");
    cp_free(e);
    bbq_arena_free(&a);
}

/* FAIL-CLOSED, both directions. An Oret is an UNKNOWN object: the callee may return
 * null (so the NPE guard must survive) and may return an object we already hold (so
 * two Orets, or an Oret and a parameter, may alias — never a strong update). */
static void test_pts_a_call_result_is_maybe_null_and_never_strongly_updated(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* y    = sir_new(&a, 4);
    /* q = foo();  q.f = x;  q.f = y;  return q.f; */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, ret);
    sir_node_t* put1 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2);
    sir_node_t* call = sir_invoke_static(&a, 7, 3, NULL, 0, SIR_DTREF);
    sir_node_t* q    = sir_store_local(&a, 0, SIR_DTREF, NULL, call, put1);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, q);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, cp_pts_of_expr(e, call), CP_OBJ_NULL),
        "a callee we cannot see into MAY RETURN NULL — its NPE guard must survive");
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)), "the last store is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "and so is the FIRST: an Oret is an unknown object, so the store is WEAK — the "
        "callee may have handed back an object something else already points at");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Spec §2 on globals: "`global.set(G,x)`: `pts(G) ∪= pts(x)`. `v ← global.get(G)`:
 * `pts(v) = pts(G)`."
 *
 * A static has had its own memory CELL all along (cp_cell_key_static), and the
 * memory-SSA overlay already tracks the store that reaches a given read of it — the
 * read was just throwing that away and answering "some unknown object". Reading the
 * cell is the same rule GetField follows; only the SEED differs (below). */
static void test_pts_static_load_sees_the_store_that_reaches_it(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x   = sir_new(&a, 3);
    sir_node_t* rd  = sir_get_static(&a, SIR_DTREF, 9, 0);
    sir_node_t* ret = sir_return(&a, rd, SIR_DTREF);
    sir_node_t* st  = sir_put_static(&a, SIR_DTREF, 9, 0, x, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, rd);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "a read of a static sees the object the reaching store put there — §2's "
        "`pts(v) = pts(G)`, not the catch-all");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "…and it is NOT null: the store wrote a fresh object over the whole cell");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Cell-SENSITIVE: a store to one static is not a store to another. */
static void test_pts_static_store_does_not_reach_a_different_static(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x   = sir_new(&a, 3);
    sir_node_t* rd  = sir_get_static(&a, SIR_DTREF, 9, 1);       /* a DIFFERENT field */
    sir_node_t* ret = sir_return(&a, rd, SIR_DTREF);
    sir_node_t* st  = sir_put_static(&a, SIR_DTREF, 9, 0, x, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, cp_pts_of_expr(e, rd), cp_obj_of(e, x)),
        "a store to S.a is not a store to S.b — a static's cell is its own");
    cp_free(e);
    bbq_arena_free(&a);
}

/* FAIL-CLOSED: with no store in THIS method, the static holds whatever some other
 * method left there — an unknown object, possibly null. This is the seed, and it is
 * what keeps reading the cell sound without an interprocedural summary. */
static void test_pts_static_with_no_store_is_unknown_and_maybe_null(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* rd  = sir_get_static(&a, SIR_DTREF, 9, 0);
    sir_node_t* ret = sir_return(&a, rd, SIR_DTREF);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, rd);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "an unwritten static MAY BE NULL — its NPE guard must stand");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_empty(e, p),
        "…and it is not ∅, which would read as unreachable and license anything");
    cp_free(e);
    bbq_arena_free(&a);
}

/* FAIL-CLOSED: a CALL kills every static — the callee may assign any of them, and
 * there is no summary that says it did not. (The wide-write already does this; the pin
 * is here because reading the cell is exactly what makes it matter.) */
static void test_pts_static_is_killed_by_a_call(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* rd   = sir_get_static(&a, SIR_DTREF, 9, 0);
    sir_node_t* ret  = sir_return(&a, rd, SIR_DTREF);
    sir_node_t* call = sir_expr_effect(&a,
        sir_invoke_static(&a, 7, 3, NULL, 0, SIR_DTINT), 1, ret);
    sir_node_t* st   = sir_put_static(&a, SIR_DTREF, 9, 0, x, call);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, rd);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "after a call the static may hold anything the callee put there, INCLUDING "
        "null — a store before the call does not survive it");
    cp_free(e);
    bbq_arena_free(&a);
}

/* pts must not disturb congruence. The partition suite as a whole is the
 * real check, but pin the invariant explicitly — a New is still its own
 * partition and pts changes nothing about who is congruent with whom. */
static void test_pts_does_not_change_partitions(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* Two congruent Adds: they must STILL share a partition with pts running. */
    sir_node_t* ret = sir_return(&a,
        sir_add(&a, SIR_DTINT, sir_load_local(&a, 2, SIR_DTINT, NULL),
                               sir_load_local(&a, 3, SIR_DTINT, NULL)), SIR_DTINT);
    sir_node_t* s1 = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT, sir_load_local(&a, 2, SIR_DTINT, NULL),
                               sir_load_local(&a, 3, SIR_DTINT, NULL)), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 4, s1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int pa = -1, pb = -1;
    for (int i = 0; i < e->vnode_count; i++) {
        cp_vnode_t* v = e->vnodes[i];
        if (v->kind != CP_VN_EXPR || !v->expr || v->expr->tag != SIR_ADD) continue;
        if (pa < 0) pa = v->partition; else pb = v->partition;
    }
    TEST_ASSERT_TRUE_MESSAGE(pa >= 0 && pb >= 0, "both Adds have vnodes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(pa, pb,
        "congruent Adds still share a partition — pts must never feed "
        "cp_split_by_facts_one (it is a derived property, not value identity)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Lattice A: the heap (stage 1b) ────────────────────────────────
 *
 * `c.f = new D(); return c.f;` — the load must see the object the store put
 * there, NOT the external phantom. This is the whole point of the memory
 * overlay: a field read is a value-flow edge from the store, through the cell,
 * to the load. */
static void test_pts_load_sees_the_store(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* alloc = sir_new(&a, 2);                       /* new D() */
    sir_node_t* c     = sir_load_local(&a, 0, SIR_DTREF, NULL);
    sir_node_t* load  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);        /* c.f */
    sir_node_t* ret   = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put   = sir_put_field(&a, SIR_DTREF, c, 1, 0, alloc, ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, put);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, alloc)),
        "the field load sees the object the store placed in the cell");
    cp_free(e);
    bbq_arena_free(&a);
}

/* An intervening CALL kills every cell (the wide-write): after it, a load must
 * NOT still claim to see the stored object — the callee could have overwritten
 * the field. Fail-closed is the point; this pins that the kill is real. */
static void test_pts_call_kills_the_cell(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* alloc = sir_new(&a, 2);
    sir_node_t* c     = sir_load_local(&a, 0, SIR_DTREF, NULL);
    sir_node_t* load  = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret   = sir_return(&a, load, SIR_DTREF);
    sir_node_t* call  = sir_expr_effect(&a,
        sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, ret);
    sir_node_t* put   = sir_put_field(&a, SIR_DTREF, c, 1, 0, alloc, call);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, put);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_EXT),
        "after a call, the cell may hold anything the callee stored");
    cp_free(e);
    bbq_arena_free(&a);
}

/* STRONG update: `d.f = x; d.f = y; return d.f;` where d is a SINGLETON (one
 * allocation). The second store must KILL the first — the load sees only y.
 * A weak update here would keep x alive and lose all precision. */
static void test_pts_strong_update_kills_prior_store(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* d  = sir_new(&a, 2);                       /* d = new D()  */
    sir_node_t* x  = sir_new(&a, 3);                       /* x = new X()  */
    sir_node_t* y  = sir_new(&a, 4);                       /* y = new Y()  */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, ret);
    sir_node_t* put1 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2);
    sir_node_t* s0 = sir_store_local(&a, 0, SIR_DTREF, NULL, d, put1);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, s0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)),
        "the load sees the value of the LAST store");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "a STRONG update (the receiver names exactly one object) kills the "
        "prior store — x must be gone");
    cp_free(e);
    bbq_arena_free(&a);
}

/* WEAK update: the same two stores through a receiver that may name EITHER of
 * two allocations. Neither store is guaranteed to overwrite the other's cell,
 * so both values must survive — killing here would be unsound. */
static void test_pts_weak_update_keeps_both(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* d1 = sir_new(&a, 2);
    sir_node_t* d2 = sir_new(&a, 2);
    sir_node_t* x  = sir_new(&a, 3);
    sir_node_t* y  = sir_new(&a, 4);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put2 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, y, ret);
    sir_node_t* put1 = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, put2);
    sir_node_t* merge = sir_nop(&a, put1);
    sir_node_t* t = sir_store_local(&a, 0, SIR_DTREF, NULL, d1, merge);
    sir_node_t* f = sir_store_local(&a, 0, SIR_DTREF, NULL, d2, merge);
    sir_node_t* br = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), t, f);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)), "the last store's value is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "the receiver may name EITHER object, so neither store is guaranteed to "
        "overwrite the other — a strong update here would be UNSOUND");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ARRAY-ELEMENT stores are ALWAYS WEAK, even on a concrete array. §2's strong update
 * needs one runtime LOCATION; a concrete receiver supplies that for a FIELD (one O.f
 * per object), but an element cell is keyed by element type and summarizes EVERY index
 * of the array (CWZ PLDI'90 — no strong array updates absent index must-alias). The
 * strong reading made s[0]=x; s[1]=y keep only {y}: a false singleton that
 * devirtualized a rotating dispatch into a failing ref.cast (bench/VirtRepro.java —
 * the e2e face of this pin). */
static void test_pts_arraystore_weak_on_concrete_array(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x  = sir_new(&a, 3);                       /* x = new X()  */
    sir_node_t* y  = sir_new(&a, 4);                       /* y = new Y()  */
    sir_node_t* load = sir_array_load(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_local(&a, 1, SIR_DTINT, NULL), NULL);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* st2 = sir_array_store(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_const(&a, 1, SIR_DTINT), y, ret, NULL);
    sir_node_t* st1 = sir_array_store(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_const(&a, 0, SIR_DTINT), x, st2, NULL);
    sir_node_t* d  = sir_store_local(&a, 0, SIR_DTREF, NULL,
        sir_new_array(&a, SIR_ATCLASS, sir_load_const(&a, 2, SIR_DTINT)), st1);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, y)), "the second store's value is there");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "the element cell summarizes every index — s[1]=y must NOT kill s[0]=x, "
        "even though the array object is concrete");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Spec §1: "plain copies DON'T EXIST (subsumed) — a merge is a φ node" ──
 *
 * The reaching-def overlay is built over SLOTS, so the natural construction puts
 * a φ at every merge for every slot — including slots a loop never writes. Those
 * φs are copies, and the spec's graph has none. Left in, they are not just noise:
 * a φ is a node in a partition of its own, so a value read INSIDE a loop stops
 * being congruent with the same value read outside it, and no expression over it
 * can be CSEd or compared across the loop boundary.
 *
 * `p` is a slot the loop never writes. Its read before the loop and its read in
 * the loop body must be ONE value. The loop's own φ for the counter must survive
 * (it merges two different values) — that is the next pin.
 *
 * The loop body MUST contain a join, or this pin is vacuous: with a straight-line
 * body the header φ's contributors are (seed, seed) and §4.9's existing Follower
 * rule already collapses it. The shape that actually breaks is the CYCLE — header
 * φ ← body-join φ ← header φ — where no contributor is the seed, so "all inputs in
 * one partition" never holds and the φ stays a Leader through CAUSE_SPLITS. */
static void test_cp_loop_invariant_slot_needs_no_phi(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* while (i != 0) { if (c) {} else {}; use(p); i = i - 1; } — p never written. */
    sir_node_t* ld_in   = sir_load_local(&a, 0, SIR_DTREF, NULL);   /* p, in-loop  */
    sir_node_t* ret     = sir_return(&a, sir_load_local(&a, 0, SIR_DTREF, NULL),
                                     SIR_DTREF);
    sir_node_t* exit_   = sir_nop(&a, ret);
    sir_node_t* dec     = sir_store_local(&a, 1, SIR_DTINT, NULL,
                              sir_sub(&a, SIR_DTINT,
                                          sir_load_local(&a, 1, SIR_DTINT, NULL),
                                          sir_load_const(&a, 1, SIR_DTINT)), NULL);
    sir_node_t* use     = sir_store_local(&a, 2, SIR_DTREF, NULL, ld_in, dec);
    sir_node_t* join    = sir_nop(&a, use);              /* the body's own merge */
    sir_node_t* arm_t   = sir_nop(&a, join);
    sir_node_t* arm_f   = sir_nop(&a, join);
    sir_node_t* body    = sir_branch(&a,
                              sir_ne(&a, sir_load_local(&a, 4, SIR_DTINT, NULL),
                                         sir_load_const(&a, 0, SIR_DTINT)),
                              arm_t, arm_f);
    sir_node_t* header  = sir_branch(&a,
                              sir_ne(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                         sir_load_const(&a, 0, SIR_DTINT)),
                              body, exit_);
    dec->store_local.next = header;                       /* the back edge */
    sir_node_t* ld_pre  = sir_load_local(&a, 0, SIR_DTREF, NULL);
    sir_node_t* pre     = sir_store_local(&a, 3, SIR_DTREF, NULL, ld_pre, header);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 5, pre);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_pre = cp_vnode_for(e, ld_pre);
    cp_vnode_t* v_in  = cp_vnode_for(e, ld_in);
    TEST_ASSERT_NOT_NULL(v_pre);
    TEST_ASSERT_NOT_NULL(v_in);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_pre->partition, v_in->partition,
        "a slot the loop never writes must NOT get a loop φ: the read inside the "
        "loop is the same value as the read before it (spec §1 — copies subsumed)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Fail-closed for the above: a slot the loop DOES write is a real merge, and its
 * φ must survive. Subsuming it would make the counter congruent with its initial
 * value — the loop would compute nothing. */
static void test_cp_loop_carried_slot_keeps_its_phi(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ld_in   = sir_load_local(&a, 1, SIR_DTINT, NULL);   /* i, in-loop */
    sir_node_t* ret     = sir_return(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                     SIR_DTINT);
    sir_node_t* exit_   = sir_nop(&a, ret);
    sir_node_t* dec     = sir_store_local(&a, 1, SIR_DTINT, NULL,
                              sir_sub(&a, SIR_DTINT, ld_in,
                                          sir_load_const(&a, 1, SIR_DTINT)),
                              NULL);
    sir_node_t* header  = sir_branch(&a,
                              sir_ne(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                                         sir_load_const(&a, 0, SIR_DTINT)),
                              dec, exit_);
    dec->store_local.next = header;                       /* the back edge */
    sir_node_t* ld_pre  = sir_load_local(&a, 1, SIR_DTINT, NULL);
    sir_node_t* pre     = sir_store_local(&a, 2, SIR_DTINT, NULL, ld_pre, header);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 3, pre);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_pre = cp_vnode_for(e, ld_pre);
    cp_vnode_t* v_in  = cp_vnode_for(e, ld_in);
    TEST_ASSERT_NOT_NULL(v_pre);
    TEST_ASSERT_NOT_NULL(v_in);
    TEST_ASSERT_TRUE_MESSAGE(v_pre->partition != v_in->partition,
        "a slot the loop DOES write is a REAL merge — its φ must survive, or the "
        "loop-carried value would be congruent with its initial value");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Spec §1: "a store reaches a load iff they touch the same O.f and no killing
 * store intervenes on the value path — a sparse query over the value graph, not
 * a dominance query."
 *
 * The reaching store IS the load's memory input, so the load of a field whose
 * reaching store wrote THAT SAME receiver is the stored value. Pinned as a
 * congruence: GVN must place the load and the stored value in one partition. */
static void test_cp_load_forwards_to_the_reaching_store(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);                       /* x = new X() */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);       /* d.f, after  */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, ret);   /* d.f = x  */
    sir_node_t* d    = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), put);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_x->partition, v_load->partition,
        "a load whose reaching store wrote the same receiver IS the stored value "
        "(spec §1 store→load reachability)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Fail-closed, and the pin that would have caught the `new int[2][2][2]` miscompile
 * at unit level instead of in the e2e corpus:
 *
 * Obj naming is 1-LIMITED — one abstract object per allocation SITE — so a site
 * inside a loop is a SUMMARY of every object it ever produces. Two receivers with
 * the SAME singleton pts are therefore NOT necessarily the same object, and a load
 * through one must NOT be forwarded to a store through the other. Here `d1` and
 * `d2` are two distinct objects (two sites); a load through d2 must not see the
 * value stored through d1. */
static void test_cp_load_does_not_forward_across_receivers(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);                        /* x = new X()   */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 0);        /* d2.f — other! */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, ret);/* d1.f = x      */
    sir_node_t* s2   = sir_store_local(&a, 1, SIR_DTREF, NULL, sir_new(&a, 2), put);
    sir_node_t* s1   = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), s2);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, s1);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_TRUE_MESSAGE(v_x->partition != v_load->partition,
        "a load through a DIFFERENT receiver must not forward to the store — Obj "
        "naming is per-SITE, so equal pts does not mean the same object");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Gate 5 (VFG ISMM'13 §4.1) — value forwarding ACROSS a call the callee provably does not
 * write. `d = new D(); d.f = x; g(); return d.f;` where `g` cannot write `d.f` (here: `d` is
 * NoEscape — it is never handed to `g`). §4.1: a call kills only the cells it side-effects, so an
 * un-written cell's store reaches the load across the call — the load IS the stored value `x`.
 * This is the value half the pts-only `survives` cannot deliver: the load-follower must walk its
 * reaching version THROUGH the call's kill to the store it interrupted. RED until the kill-walk. */
static void test_cp_load_forwards_across_a_preserving_call(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);                       /* x = new X()          */
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);       /* d.f, AFTER the call  */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* call = sir_expr_effect(&a,
        sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, ret);   /* g() — d not passed */
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, call);     /* d.f = x            */
    sir_node_t* d    = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), put); /* d = new D() */
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_x->partition, v_load->partition,
        "d.f = x; g(); load d.f — g cannot write d.f (d is NoEscape), so the store reaches "
        "the load across the call and the load IS x (VFG ISMM'13 §4.1)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Gate 5 soundness (the pin that must never go red): the SAME shape but `d` is handed to the
 * call, so the callee MAY write `d.f`. The load after the call must NOT forward to `x` — it reads
 * whatever the call left. Forwarding here would be the exact optimistic-unsound miscompile the
 * revocable follower exists to prevent (escape descends `d` to ArgEscape under a bottom call). */
static void test_cp_load_does_not_forward_across_an_escaping_call(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);       /* d.f, AFTER the call  */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t** args = (sir_node_t**)bbq_arena_alloc(&a, sizeof(sir_node_t*));
    args[0] = sir_load_local(&a, 0, SIR_DTREF, NULL);        /* g(d) — d IS passed   */
    sir_node_t* call = sir_expr_effect(&a,
        sir_invoke_static(&a, 1, 0, args, 1, SIR_DTINT), 1, ret);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, call);     /* d.f = x            */
    sir_node_t* d    = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), put);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_TRUE_MESSAGE(v_x->partition != v_load->partition,
        "d handed to g() may be written by it — the load after the call must NOT forward to x "
        "(the call does not provably preserve d.f)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Gate 5 branch — the kill-walk is MULTI-HOP. `d.f = x; g(); h(); load d.f` with two calls that
 * both preserve d.f (d NoEscape) must still forward: the `while` walks its reaching version through
 * BOTH kills to the store. Pins the N>1 case of `cp_load_forward_target` (happy path was N=1). */
static void test_cp_load_forwards_across_two_preserving_calls(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* h    = sir_expr_effect(&a, sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, ret);
    sir_node_t* g    = sir_expr_effect(&a, sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, h);
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, x, g);
    sir_node_t* d    = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), put);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_x->partition, v_load->partition,
        "the kill-walk forwards through TWO non-writing calls (multi-hop) to the store");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Gate 5 fail-closed branch — the kill-walk needs THE object (`cp_pts_sole_obj`): a receiver
 * that may name TWO objects cannot ask "did the call preserve o.f", so a load after a call must
 * not forward even though neither object is handed to the call. (Improvable soundly by asking
 * preserve() of EVERY pts member — until someone does that deliberately, this pins the
 * fail-closed answer; going red here means that decision is being made, so make it on purpose.) */
static void test_cp_load_does_not_forward_across_a_call_two_object_receiver(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* x    = sir_new(&a, 3);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 0);       /* d.f, AFTER the call  */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* call = sir_expr_effect(&a,
        sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, ret);   /* g() — d not passed */
    sir_node_t* put  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 1, 0, x, call);     /* d.f = x            */
    /* d = c ? new D1() : new D2() — a genuinely two-object pts via the diamond's φ */
    sir_node_t* join = sir_nop(&a, put);
    sir_node_t* sA   = sir_store_local(&a, 1, SIR_DTREF, NULL, sir_new(&a, 2), join);
    sir_node_t* sB   = sir_store_local(&a, 1, SIR_DTREF, NULL, sir_new(&a, 4), join);
    sir_node_t* br   = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), sA, sB);
    sir_method_t* m  = sir_method(&a, "f", 0, 1, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, load);
    cp_vnode_t* v_x    = cp_vnode_for(e, x);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_x);
    TEST_ASSERT_TRUE_MESSAGE(v_x->partition != v_load->partition,
        "a two-object receiver has no THE object to ask preserve() about — the load "
        "must not forward across the call (fail-closed, cp_pts_sole_obj)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Gate 5 soundness branch — an ARRAY load must NEVER forward, even across a preserving call and
 * even for a NoEscape array: §1 makes an array cell MONOLITHIC (a store to a[i] says nothing about
 * a[j]). `cp_load_forward_target` returns −1 for any non-GetField, so the store-of-5 must not reach
 * the load. If Gate 5's refactor ever let arrays forward, this goes red here, not in exec. */
static void test_cp_array_load_does_not_forward_across_a_call(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* c5    = sir_load_const(&a, 5, SIR_DTINT);
    sir_node_t* aload = sir_array_load(&a, SIR_DTINT,
        sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_const(&a, 0, SIR_DTINT), NULL);
    sir_node_t* ret   = sir_return(&a, aload, SIR_DTINT);
    sir_node_t* call  = sir_expr_effect(&a, sir_invoke_static(&a, 1, 0, NULL, 0, SIR_DTINT), 1, ret);
    sir_node_t* astore= sir_array_store(&a, SIR_DTINT,
        sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_const(&a, 0, SIR_DTINT), c5, call, NULL);
    sir_node_t* d     = sir_store_local(&a, 0, SIR_DTREF, NULL,
        sir_new_array(&a, SIR_ATINT, sir_load_const(&a, 2, SIR_DTINT)), astore);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_load = cp_vnode_for(e, aload);
    cp_vnode_t* v_c5   = cp_vnode_for(e, c5);
    TEST_ASSERT_NOT_NULL(v_load);
    TEST_ASSERT_NOT_NULL(v_c5);
    TEST_ASSERT_TRUE_MESSAGE(v_c5->partition != v_load->partition,
        "an array load is monolithic (§1) and must NOT forward across a call — Gate 5 forwards "
        "GetField only");
    cp_free(e);
    bbq_arena_free(&a);
}

/* §15.10.1 + §10.7: `(new T[n]).length` IS `n` — the array is created with exactly
 * the evaluated dimension, and the length is final. Click's ArrayLengthNode
 * ::Identity, pinned as a congruence. */
static void test_cp_arraylen_of_fresh_array_is_its_size(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* n    = sir_load_local(&a, 0, SIR_DTINT, NULL);        /* the size */
    sir_node_t* arr  = sir_new_array(&a, SIR_ATINT, n);
    sir_node_t* len  = sir_array_length(&a,
                          sir_load_local(&a, 1, SIR_DTREF, NULL));    /* a.length */
    sir_node_t* ret  = sir_return(&a, len, SIR_DTINT);
    sir_node_t* st   = sir_store_local(&a, 1, SIR_DTREF, NULL, arr, ret);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 2, st);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v_len = cp_vnode_for(e, len);
    cp_vnode_t* v_n   = cp_vnode_for(e, n);
    TEST_ASSERT_NOT_NULL(v_len);
    TEST_ASSERT_NOT_NULL(v_n);
    TEST_ASSERT_EQUAL_INT_MESSAGE(v_n->partition, v_len->partition,
        "(new T[n]).length IS n — §15.10.1 gives the array exactly the evaluated "
        "dimension and §10.7 makes the length final");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Spec §4: "The branch refinement is the key: on the true edge out of a
 * ref.is_null / != null test the operand is Null, on the false edge NonNull
 * (per-edge facts … carried on the SIR edge, no dominance)."
 *
 * Nullability is DERIVED from pts (`⊥null ∈ pts` ⟺ may-be-null), so the refinement
 * has to narrow pts itself. `p` is a parameter — Maybe on entry. After surviving
 * `if (p == null) …`, its use on the false arm must not name the null object; on
 * the true arm it must name NOTHING BUT the null object. */
static void test_cp_null_test_refines_pts_on_both_arms(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* use_t = sir_load_local(&a, 0, SIR_DTREF, NULL);   /* p, == null  */
    sir_node_t* use_f = sir_load_local(&a, 0, SIR_DTREF, NULL);   /* p, != null  */
    sir_node_t* ret_t = sir_return(&a, use_t, SIR_DTREF);
    sir_node_t* ret_f = sir_return(&a, use_f, SIR_DTREF);
    sir_node_t* br    = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_null(&a)),
        ret_t, ret_f);
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p_t = cp_pts_of_expr(e, use_t);
    cp_pts_t p_f = cp_pts_of_expr(e, use_f);
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p_f, CP_OBJ_NULL),
        "on the FALSE arm of `p == null` the value cannot be null — this is what "
        "lets the NEXT deref of p drop its NPE guard (spec §4)");
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p_t, CP_OBJ_NULL),
        "on the TRUE arm of `p == null` the value IS null");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p_t, CP_OBJ_EXT),
        "on the TRUE arm it is null and NOTHING else — the external object is gone");
    cp_free(e);
    bbq_arena_free(&a);
}

/* The new lattices must never move a partition. A Refine that narrows only
 * pts computes nothing — same value, same constant — so it is a §4.7 COPY
 * Follower of its input. If it were a Leader in its own partition, every
 * expression over a null-checked reference would be incongruent with the same
 * expression over the reference itself: the entire graph below every deref. */
static void test_cp_null_refine_does_not_move_partitions(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* use_f = sir_load_local(&a, 0, SIR_DTREF, NULL);   /* p, != null */
    sir_node_t* ret_t = sir_return(&a, sir_load_null(&a), SIR_DTREF);
    sir_node_t* ret_f = sir_return(&a, use_f, SIR_DTREF);
    sir_node_t* br    = sir_branch(&a,
        sir_eq(&a, sir_load_local(&a, 0, SIR_DTREF, NULL), sir_load_null(&a)),
        ret_t, ret_f);
    sir_method_t* m = sir_method(&a, "f", 0, 1, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_vnode_t* v = cp_vnode_for(e, use_f);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE_MESSAGE(v->leader >= 0,
        "a refined use is a COPY Follower — pts is a derived property, never value "
        "identity (adding a lattice cannot move a partition)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(e->vnodes[v->leader]->partition, v->partition,
        "a Follower lives in its Leader's partition");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Spec §2: the strong update is licensed because "the store's memory-SSA NAME kills
 * the prior def" — the kill is in the NAMING. A name's value is a FUNCTION OF ITS
 * INPUTS, so it must be RECOMPUTED, never accumulated onto its own previous value.
 * Accumulating makes the kill look like a retraction that the cell-φ downstream can
 * never take back, and every workaround for THAT (a ⊥-guard on the receiver, say)
 * silences the store entirely — its value never reaches the load, the load reads
 * only the seed's null, and an NPE guard "proves" a non-null object null.
 *
 * The `new int[2][2][2]` miscompile itself is pinned at the compiler level, in
 * test_sir §14 (the optimizer must not conclude a method that cannot throw always
 * throws) — it needs the real lowering to reproduce. This pins the lattice property
 * that underlies it: a store whose RECEIVER comes from a load still reaches its own
 * load, which requires the reverse index to carry the store's reaching version. */
static void test_pts_store_through_loaded_receiver_reaches_its_load(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /*   d   = new D();
     *   d.f = new M();          // store 1 — receiver is a local
     *   m   = d.f;              // a LOAD…
     *   m.f = new X();          // store 2 — …whose result is THIS store's receiver
     *   return d.f.f;           // must see X, not "provably null"
     *
     * Store 2 is the shape that matters: its receiver's pts is not known until the
     * heap has flowed through store 1 and the load. A store is re-armed only by the
     * reverse index, so if that index does not carry the store's reaching memory
     * version, store 2 computes once against an empty heap and is NEVER revisited —
     * its value stays ∅, the load downstream sees only the seed's null, and an NPE
     * guard "proves" a non-null object null. That is the `new int[2][2][2]` bug,
     * whose every array store writes through a receiver loaded from the array. */
    sir_node_t* x     = sir_new(&a, 4);                       /* the deep value    */
    sir_node_t* outer = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);        /* d.f               */
    sir_node_t* deep  = sir_get_field(&a, SIR_DTREF, outer, 2, 0);  /* (d.f).f     */
    sir_node_t* ret   = sir_return(&a, deep, SIR_DTREF);
    sir_node_t* exit_ = sir_nop(&a, ret);
    /* …and the store lives in a LOOP, so the memory version reaching it is a cell-φ
     * that only fills in on a later iterate — after the store was first computed.
     * Without the re-arm the store never recomputes and its value stays ∅. */
    sir_node_t* dec   = sir_store_local(&a, 2, SIR_DTINT, NULL,
                          sir_sub(&a, SIR_DTINT, sir_load_local(&a, 2, SIR_DTINT, NULL),
                                      sir_load_const(&a, 1, SIR_DTINT)), NULL);
    sir_node_t* put2  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 1, SIR_DTREF, NULL), 2, 0, x, dec);     /* m.f = x     */
    sir_node_t* m_st  = sir_store_local(&a, 1, SIR_DTREF, NULL,
        sir_get_field(&a, SIR_DTREF,
            sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0), put2);  /* m = d.f     */
    sir_node_t* head  = sir_branch(&a,
                          sir_ne(&a, sir_load_local(&a, 2, SIR_DTINT, NULL),
                                     sir_load_const(&a, 0, SIR_DTINT)),
                          m_st, exit_);
    dec->store_local.next = head;                                  /* the back edge */
    sir_node_t* put1  = sir_put_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0, sir_new(&a, 3), head);
    sir_node_t* d     = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), put1);
    sir_method_t* mm  = sir_method(&a, "f", 0, 0, 3, d);

    cp_engine_t* e = cp_build(mm, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, deep);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, cp_obj_of(e, x)),
        "a store whose RECEIVER comes from a load must still reach its own load — "
        "the reverse index has to re-arm a store when the memory version reaching "
        "it changes, not only when its operands' pts do");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_count(e, p) == 1
                              && cp_pts_has(e, p, CP_OBJ_NULL),
        "and it must NOT read as provably-null — that is the NPE miscompile");
    cp_free(e);
    bbq_arena_free(&a);
}

/* §12.5: a new object's fields start at their DEFAULTS. An object this method
 * allocates does not exist at method entry, so the cell's entry contents say
 * nothing about it — claiming "anything external" there is not just imprecise, it
 * is STICKY: a store's weak path unions the entry contents in before pts(recv)
 * has converged to a singleton, and the cell-φ that consumes it can never take it
 * back. The read below has no store reaching it, so it sees the seed. */
static void test_pts_fresh_object_field_is_null_at_entry(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* load = sir_get_field(&a, SIR_DTREF,
        sir_load_local(&a, 0, SIR_DTREF, NULL), 1, 0);      /* d.f, never stored */
    sir_node_t* ret  = sir_return(&a, load, SIR_DTREF);
    sir_node_t* d    = sir_store_local(&a, 0, SIR_DTREF, NULL, sir_new(&a, 2), ret);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, d);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_pts_t p = cp_pts_of_expr(e, load);
    TEST_ASSERT_TRUE_MESSAGE(cp_pts_has(e, p, CP_OBJ_NULL),
        "an unwritten field of a FRESH object is null (§12.5 defaults)");
    TEST_ASSERT_FALSE_MESSAGE(cp_pts_has(e, p, CP_OBJ_EXT),
        "it is NOT 'anything external' — the object did not exist at entry, so the "
        "cell's entry contents cannot describe it");
    cp_free(e);
    bbq_arena_free(&a);
}

static const sir_node_t* find_first_tag(const sir_node_t* e, int tag) {
    if (!e) return NULL;
    if ((int)e->tag == tag) return e;
    for (int i = 0; i < sir_arity(e); i++) {
        const sir_node_t* r = find_first_tag(sir_child((sir_node_t*)e, i), tag);
        if (r) return r;
    }
    return NULL;
}

/* Two INDEPENDENT ternaries: `x = p ? 1 : 0; y = q ? 1 : 0;`. Their merges each
 * take contributors {LoadConst 1, LoadConst 0} — positionally IDENTICAL — so
 * CAUSE_SPLITS (which splits only by input partition) can never tell the two φs
 * apart. A φ's merge point is part of its identity: in Click's sea-of-nodes the
 * Region IS the φ's input 0. Without that, the two φs were congruent, the peer-φ
 * canonicalization rewrote every read of y onto x, y's arms went dead, and the
 * whole second diamond — including the CALL in its condition — was deleted.
 * `(g(3)?1:0)*100 + (g(65)?1:0)*10 + (g(4)?1:0)` compiled to `s*111`. */
static void test_cp_phis_at_distinct_merges_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = p, slot 1 = q (params); slot 2 = x, slot 3 = y. */
    sir_node_t* ret = sir_return(&a,
        sir_add(&a, SIR_DTINT,
            sir_mul(&a, SIR_DTINT, sir_load_local(&a, 2, SIR_DTINT, NULL),
                                   sir_load_const(&a, 10, SIR_DTINT)),
            sir_load_local(&a, 3, SIR_DTINT, NULL)), SIR_DTINT);
    sir_node_t* m2 = sir_nop(&a, ret);
    sir_node_t* y1 = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_load_const(&a, 1, SIR_DTINT), m2);
    sir_node_t* y0 = sir_store_local(&a, 3, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), m2);
    sir_node_t* br2 = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), y1, y0);
    sir_node_t* m1 = sir_nop(&a, br2);
    sir_node_t* x1 = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 1, SIR_DTINT), m1);
    sir_node_t* x0 = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), m1);
    sir_node_t* br1 = sir_branch(&a,
        sir_ne(&a, sir_load_local(&a, 0, SIR_DTINT, NULL),
                   sir_load_const(&a, 0, SIR_DTINT)), x1, x0);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 4, br1);

    topt(m, &a);

    /* x and y are live at the same time and hold different values, so they must
     * still be two distinct slots — under the bug both reads collapsed to one. */
    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_ADD, (int)rv->tag, "the return still adds");
    const sir_node_t* xl = find_first_tag(rv->add.left, SIR_LOADLOCAL);
    const sir_node_t* yl = rv->add.right;
    TEST_ASSERT_TRUE_MESSAGE(xl && yl && yl->tag == SIR_LOADLOCAL,
        "both ternary results are still local reads");
    TEST_ASSERT_TRUE_MESSAGE(xl->load_local.slot != yl->load_local.slot,
        "two ternaries at DIFFERENT merges are different values — their phis "
        "must not be congruent just because both merge {1, 0}");
    bbq_arena_free(&a);
}

static void test_cp_same_op_distinct_const_operands_not_congruent(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    #define LL0 sir_load_local(&a, 0, SIR_DTINT, NULL)
    #define BYTE_AT(k) sir_and(&a, SIR_DTINT, \
        sir_ushr(&a, SIR_DTINT, LL0, sir_load_const(&a, (k), SIR_DTINT)), \
        sir_load_const(&a, 0xFF, SIR_DTINT))
    sir_node_t* ret = sir_return_void(&a);
    sir_node_t* s3  = sir_store_local(&a, 3, SIR_DTINT, NULL, BYTE_AT(8),  ret);
    sir_node_t* s2  = sir_store_local(&a, 2, SIR_DTINT, NULL, BYTE_AT(16), s3);
    sir_node_t* s1  = sir_store_local(&a, 1, SIR_DTINT, NULL, BYTE_AT(24), s2);
    #undef BYTE_AT
    #undef LL0
    sir_method_t* m = sir_method(&a, "writeInt", 0, 0, 4, s1);

    topt(m, &a);

    /* Each store must still compute its OWN shift amount. Collect the shift
     * constants that survive; all three (24, 16, 8) must be present. */
    bool seen24 = false, seen16 = false, seen8 = false;
    sir_node_t* sts[3] = { s1, s2, s3 };
    for (int i = 0; i < 3; i++) {
        const sir_node_t* sh = find_first_tag(sts[i]->store_local.value, SIR_USHR);
        if (!sh) continue;
        const sir_node_t* k = sir_child((sir_node_t*)sh, 1);
        if (!k || k->tag != SIR_LOADCONST) continue;
        if (k->load_const.value == 24) seen24 = true;
        if (k->load_const.value == 16) seen16 = true;
        if (k->load_const.value == 8)  seen8  = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(seen24 && seen16 && seen8,
        "(v>>>24), (v>>>16), (v>>>8) are three DIFFERENT values — a shared "
        "constant partition must not make them congruent");
    bbq_arena_free(&a);
}

/* §20.9.18 raw float bits: `floatToRawIntBits(intBitsToFloat(k))` folds to
 * MoveF2I(MoveI2F(LoadConst k)) — the RAW bit pattern must survive, payload
 * and signaling bit included. Holding an f32 constant in a double silently
 * quiets a signaling NaN (f32→f64 sets the mantissa MSB; f64→f32 keeps it),
 * so 0x7F800001 came back 0x7FC00001. `raw` means raw. */
static void test_cp_move_f2i_preserves_raw_nan_bits(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* ret = sir_return(&a,
        sir_move_f2_i(&a, sir_move_i2_f(&a,
            sir_load_const(&a, 0x7F800001, SIR_DTINT))), SIR_DTINT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    topt(m, &a);

    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADCONST, (int)rv->tag,
        "the Move round-trip folds to a constant");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x7F800001, (uint32_t)rv->load_const.value,
        "floatToRawIntBits(intBitsToFloat(0x7F800001)) keeps the raw sNaN bits");
    bbq_arena_free(&a);
}

/* The same obligation one level down, on the COPY path: an f32 KNOWN carried
 * through the constant lattice and re-materialized by constant substitution
 * (`float f = <sNaN>; return f;`) must come back bit-identical. Only the
 * non-arithmetic paths owe this — JLS §4.2.3 leaves the NaN a float
 * *computation* produces unspecified, so no test may pin arithmetic payloads;
 * a reinterpret (§20.9.18 floatToRawIntBits) and a copy are exact. */
static void test_cp_f32_const_keeps_exact_bits(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    float snan;
    uint32_t bits = 0x7F800001;
    memcpy(&snan, &bits, 4);
    sir_node_t* ret = sir_return(&a,
        sir_load_local(&a, 0, SIR_DTFLOAT, NULL), SIR_DTFLOAT);
    sir_node_t* st  = sir_store_local(&a, 0, SIR_DTFLOAT, NULL,
        sir_load_float_const(&a, snan), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, st);

    topt(m, &a);

    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADFLOATCONST, (int)rv->tag,
        "the f32 constant is substituted for the local read");
    uint32_t got;
    memcpy(&got, &rv->load_float_const.value, 4);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x7F800001, got,
        "an f32 KNOWN keeps its exact bits through the lattice");
    bbq_arena_free(&a);
}

/* §15.15.5 long complement: `x & ~(1L << s)` — the XOR with -1L (the
 * lowering of ~) must survive. A wide constant keeps its payload in
 * .lvalue; any fold reading the i32 .value field sees 0 and treats
 * -1L as an XOR identity, silently deleting the NOT (BitSet.clear). */
static void test_cp_long_not_survives(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = x (long param), slot 1 = s (int param). */
    sir_node_t* shl = sir_shl(&a, SIR_DTLONG,
        sir_load_long_const(&a, 1),
        sir_i2_l(&a, sir_load_local(&a, 1, SIR_DTINT, NULL)));
    sir_node_t* inv = sir_xor(&a, SIR_DTLONG, shl,
        sir_load_long_const(&a, -1));
    sir_node_t* and_ = sir_and(&a, SIR_DTLONG,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), inv);
    sir_node_t* ret = sir_return(&a, and_, SIR_DTLONG);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, ret);

    topt(m, &a);

    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_AND, (int)rv->tag, "AND survives");
    int has_xor = rv->and_.left->tag == SIR_XOR || rv->and_.right->tag == SIR_XOR;
    TEST_ASSERT_TRUE_MESSAGE(has_xor,
        "the ~ (xor -1L) must survive — a wide -1 is not the i32 identity 0");
    bbq_arena_free(&a);
}

/* CHECKCAST elimination must respect REPRESENTATION, not just value
 * facts: `Object o = new Box(); Box b = (Box)o;` — the value provably
 * IS a Box, but the operand's static type is the Object-typed local,
 * so dropping the ref.cast emits an ill-typed store. Only drop when
 * the operand's STATIC type already ⊑ the target. (No sema here, so
 * even same-class must keep the cast when the descriptor differs.) */
static void test_cp_checkcast_kept_when_static_type_wider(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 1 : (ref C7) — the OBJECT-typed local; cast target C3. */
    sir_node_t* obj = sir_load_local(&a, 1, SIR_DTREF, sir_class_ref(&a, 7));
    sir_node_t* cc  = sir_check_cast(&a, obj, SIR_ATCLASS, 3);
    sir_node_t* ret = sir_return(&a, cc, SIR_DTREF);
    sir_node_t* st  = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, 7),
        sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, 3)), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, st);

    topt(m, &a);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_CHECKCAST, (int)ret->return_.value->tag,
        "cast of a WIDER-typed local must survive even when the value "
        "provably fits (representation, not just value)");
    bbq_arena_free(&a);
}

static void test_cp_checkcast_dropped_when_static_type_matches(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 1 : (ref C3) — already the target's type; value provably C3. */
    sir_node_t* obj = sir_load_local(&a, 1, SIR_DTREF, sir_class_ref(&a, 3));
    sir_node_t* cc  = sir_check_cast(&a, obj, SIR_ATCLASS, 3);
    sir_node_t* ret = sir_return(&a, cc, SIR_DTREF);
    sir_node_t* st  = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, 3),
        sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, 3)), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, st);

    topt(m, &a);

    TEST_ASSERT_TRUE_MESSAGE(ret->return_.value->tag != SIR_CHECKCAST,
        "same-static-type cast of a provably-typed value is dropped");
    bbq_arena_free(&a);
}

/* Optimistic SCCP over a loop-carried accumulator: φ(s) = [0, s+i]
 * starts at the entry constant but MUST fall once the back edge is
 * live and i is non-constant — `for (i=0;i<n;i++) s+=i; return s`
 * is not "return 0". */
static void test_cp_loop_accumulator_not_constant(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = n (param), slot 1 = i, slot 2 = s. */
    sir_node_t* ret   = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT);
    sir_node_t* head  = sir_nop(&a, NULL);
    sir_node_t* inc_i = sir_inc(&a, 1, 1, SIR_DTINT,
        sir_load_local(&a, 1, SIR_DTINT, NULL), head);
    sir_node_t* st_s2 = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT,
                sir_load_local(&a, 2, SIR_DTINT, NULL),
                sir_load_local(&a, 1, SIR_DTINT, NULL)), inc_i);
    sir_node_t* br1 = sir_branch(&a,
        sir_lt(&a, sir_load_local(&a, 1, SIR_DTINT, NULL),
                   sir_load_local(&a, 0, SIR_DTINT, NULL)),
        st_s2, ret);
    sir_set_next(head, br1);
    sir_node_t* st_i = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), head);
    sir_node_t* st_s = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 0, SIR_DTINT), st_i);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 3, st_s);

    topt(m, &a);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, (int)ret->return_.value->tag,
        "the accumulator result must NOT fold to a constant");
    bbq_arena_free(&a);
}

/* §15.21.1 / §15.18.2: NaN breaks reflexivity — the §4.6 x⊙x folds
 * (NE(x,x)→0, EQ(x,x)→1, SUB(x,x)→0) are UNSOUND for float/double
 * operands (NaN != NaN is true; NaN - NaN is NaN). Float.isNaN is
 * exactly `v != v` and must survive. */
static void test_cp_no_reflexive_fold_on_floats(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* l1 = sir_load_local(&a, 0, SIR_DTFLOAT, NULL);
    sir_node_t* l2 = sir_load_local(&a, 0, SIR_DTFLOAT, NULL);
    sir_node_t* ne = sir_ne(&a, l1, l2);
    sir_node_t* ret = sir_return(&a, ne, SIR_DTINT);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    topt(m, &a);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_NE, (int)ret->return_.value->tag,
        "float x != x must NOT fold (NaN != NaN is true)");
    bbq_arena_free(&a);
}

static void test_cp_no_sub_self_fold_on_doubles(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    sir_node_t* l1 = sir_load_local(&a, 0, SIR_DTDOUBLE, NULL);
    sir_node_t* l2 = sir_load_local(&a, 0, SIR_DTDOUBLE, NULL);
    sir_node_t* sub = sir_sub(&a, SIR_DTDOUBLE, l1, l2);
    sir_node_t* ret = sir_return(&a, sub, SIR_DTDOUBLE);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 1, ret);

    topt(m, &a);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_SUB, (int)ret->return_.value->tag,
        "double x - x must NOT fold to 0 (NaN/Inf operands)");
    bbq_arena_free(&a);
}

/* Slots are NOT SSA: forwarding a load of `e` to the copy-source slot
 * is sound only where that slot STILL holds the copied value. The
 * canonical shape is Hashtable.rehash's chain walk:
 *     e = old; old = old.next; …use e…
 * — after the reassignment, a load of e must NOT read old's slot. */
static void test_cp_no_copy_forward_past_source_redef(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    /* slot 0 = x (param); slot 1 = e.
     *   e = x;            (copy)
     *   x = x + 1;        (source reassigned)
     *   return e;         (must read e's slot, not x's) */
    sir_node_t* ld_e = sir_load_local(&a, 1, SIR_DTINT, NULL);
    sir_node_t* ret  = sir_return(&a, ld_e, SIR_DTINT);
    sir_node_t* st_x = sir_store_local(&a, 0, SIR_DTINT, NULL,
        sir_add(&a, SIR_DTINT,
                sir_load_local(&a, 0, SIR_DTINT, NULL),
                sir_load_const(&a, 1, SIR_DTINT)), ret);
    sir_node_t* st_e = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_local(&a, 0, SIR_DTINT, NULL), st_x);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 2, st_e);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    TEST_ASSERT_NOT_NULL(e);
    cp_rewrite(e);
    cp_free(e);

    sir_node_t* rv = ret->return_.value;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, (int)rv->tag,
        "return still reads a local");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, rv->load_local.slot,
        "the load of e must NOT be forwarded to x's reassigned slot");
    bbq_arena_free(&a);
}

/* By the calling convention, caller-set parameter cells occupy
 * [0, args_cells). cp_pack must NOT reassign them. */
static void test_cp_pack_anchors_params(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* 2 params (slots 0, 1) + 1 local (slot 2). */
    sir_node_t* ret = sir_return(&a,
        sir_add(&a, SIR_DTSHORT,
            sir_load_local(&a, 0, SIR_DTSHORT, NULL),
            sir_load_local(&a, 1, SIR_DTSHORT, NULL)),
        SIR_DTSHORT);
    sir_node_t* st2 = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_load_const(&a, 0, SIR_DTSHORT), ret);
    sir_method_t* m = sir_method(&a, "f", 0, 0, 3, st2);

    cp_pack(m, NULL, &a, 2);   /* args_cells = 2 */

    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
        ret->return_.value->add.left->load_local.slot,
        "param at slot 0 must remain at slot 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1,
        ret->return_.value->add.right->load_local.slot,
        "param at slot 1 must remain at slot 1");
    bbq_arena_free(&a);
}

/* int and short share the i32 valtype pool on WASM, so disjoint int and
 * short locals coalesce into one i32 local. (long/float/double/ref get
 * their own pools — covered by the width-class test below.) */
static void test_cp_pack_int_short_share_i32_pool(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* slot 0 = short param; slot 1 = int local; slot 3 = short
     * local. Live ranges of slots 1 and 3 are disjoint. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 3, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* st3  = sir_store_local(&a, 3, SIR_DTSHORT, NULL,
        sir_load_const(&a, 5, SIR_DTSHORT), ret);
    sir_node_t* ee1  = sir_expr_effect(&a,
        sir_load_local(&a, 1, SIR_DTINT, NULL), 1, st3);
    sir_node_t* st1  = sir_store_local(&a, 1, SIR_DTINT, NULL,
        sir_load_const(&a, 99, SIR_DTINT), ee1);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 4, st1);

    cp_pack(m, NULL, &a, 1);

    /* param at slot 0 + one shared i32 cell = 2. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, m->max_locals,
        "int and short are both i32 on WASM — disjoint locals coalesce");
    bbq_arena_free(&a);
}

/* The valtype pools are distinct: i32 (byte/short/char/int), i64 (long),
 * f32 (float), f64 (double), ref. Disjoint locals of *different* valtype
 * classes must NOT coalesce, even though all share index space. Here a
 * long, a double and an int all have disjoint live ranges; they land in
 * three separate locals. */
static void test_cp_pack_distinct_valtypes_dont_coalesce(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* slot 0 = long; slot 1 = double; slot 2 = int. Each stored then
     * consumed before the next is stored — fully disjoint. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTINT, NULL), SIR_DTINT);
    sir_node_t* st2  = sir_store_local(&a, 2, SIR_DTINT, NULL,
        sir_load_const(&a, 7, SIR_DTINT), ret);
    sir_node_t* eed  = sir_expr_effect(&a,
        sir_load_local(&a, 1, SIR_DTDOUBLE, NULL), 1, st2);
    sir_node_t* st1  = sir_store_local(&a, 1, SIR_DTDOUBLE, NULL,
        sir_load_double_const(&a, 2.5), eed);
    sir_node_t* eel  = sir_expr_effect(&a,
        sir_load_local(&a, 0, SIR_DTLONG, NULL), 1, st1);
    sir_node_t* st0  = sir_store_local(&a, 0, SIR_DTLONG, NULL,
        sir_load_long_const(&a, 9), eel);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st0);

    cp_pack(m, NULL, &a, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, m->max_locals,
        "long/double/int are distinct WASM valtypes — must not coalesce");
    bbq_arena_free(&a);
}

/* ── §4.9 PHI Follower (Click thesis §4.9 — currently disabled) ── */

/* A PHI whose all-live inputs come from a single partition is a
 * Follower of that partition (Click §4.9 PhiNode::Identity). For
 * two branch arms each storing LoadLocal(p) into slot 1, the PHI
 * merging slot-1 at the join has both contributors in p's partition
 * — so the post-merge LoadLocal(1) should resolve to LoadLocal(0)
 * (p) directly.
 *
 * This test asserts the spec-correct behavior; cp_apply_phi_follower is
 * currently void-cast disabled in cp_follower_sweep — when the
 * §4.9 wiring is restored, this test goes green. Cardinal-rule
 * safe: the assertion compares against the optimal output, not
 * the current buggy output. */
static void test_cp_apply_phi_follower_same_partition_contributors(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret   = sir_return(&a,
        sir_load_local(&a, 1, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* merge = sir_nop(&a, ret);
    sir_node_t* st_t  = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), merge);
    sir_node_t* st_f  = sir_store_local(&a, 1, SIR_DTSHORT, NULL,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), merge);
    sir_node_t* cond  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br    = sir_branch(&a, cond, st_t, st_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 2, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL,
        ret->return_.value->tag,
        "post-§4.9 Return reads a slot directly (no merge-then-read)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
        ret->return_.value->load_local.slot,
        "Return must read slot 0 (param p) — the PHI's "
        "all-same-partition contributors mean slot 1's merged value "
        "IS p, so the read resolves to p");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── cp_compute_reachability contract ──────────────────────────── */

/* TOP-cond Branch: Click §4.4.1 optimistic reading — no proof yet of
 * dead arm, so BOTH arms stay reachable. Pinning this directly because
 * without it the loop-counter PHI saw its back-edge predecessor as
 * "not reachable" in early fixpoint iterations and collapsed to the
 * entry contributor's constant. */
static void test_cp_reachability_top_cond_both_arms_live(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a, sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a, sir_load_const(&a, 2, SIR_DTSHORT), SIR_DTSHORT);
    /* TOP-cond: a LoadLocal of an opaque param. cp_cond_const will
     * never derive a KNOWN value for it. */
    sir_node_t* cond  = sir_load_local(&a, 0, SIR_DTSHORT, NULL);
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int it = cp_spine_idx_of(e, ret_t);
    int ifa = cp_spine_idx_of(e, ret_f);
    TEST_ASSERT_TRUE(it >= 0 && ifa >= 0);
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[it],
        "TOP-cond Branch: on_true arm must stay reachable (no proof of dead arm)");
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[ifa],
        "TOP-cond Branch: on_false arm must stay reachable (no proof of dead arm)");
    cp_free(e);
    bbq_arena_free(&a);
}

/* KNOWN-true cond: only on_true arm reachable. on_false's chain is
 * dead per Click §4.3 UCE. */
static void test_cp_reachability_known_true_cond_prunes_false_arm(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a, sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a, sir_load_const(&a, 2, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond  = sir_load_const(&a, 1, SIR_DTSHORT);  /* KNOWN true */
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int it = cp_spine_idx_of(e, ret_t);
    int ifa = cp_spine_idx_of(e, ret_f);
    TEST_ASSERT_TRUE(it >= 0 && ifa >= 0);
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[it],
        "KNOWN-true Branch: on_true arm must be reachable");
    TEST_ASSERT_FALSE_MESSAGE(e->reachable[ifa],
        "KNOWN-true Branch: on_false arm must be pruned unreachable");
    cp_free(e);
    bbq_arena_free(&a);
}

/* KNOWN-false cond: symmetric. Only on_false reachable. */
static void test_cp_reachability_known_false_cond_prunes_true_arm(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_t = sir_return(&a, sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_f = sir_return(&a, sir_load_const(&a, 2, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* cond  = sir_load_const(&a, 0, SIR_DTSHORT);  /* KNOWN false */
    sir_node_t* br    = sir_branch(&a, cond, ret_t, ret_f);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, br);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int it = cp_spine_idx_of(e, ret_t);
    int ifa = cp_spine_idx_of(e, ret_f);
    TEST_ASSERT_TRUE(it >= 0 && ifa >= 0);
    TEST_ASSERT_FALSE_MESSAGE(e->reachable[it],
        "KNOWN-false Branch: on_true arm must be pruned unreachable");
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[ifa],
        "KNOWN-false Branch: on_false arm must be reachable");
    cp_free(e);
    bbq_arena_free(&a);
}

/* TOP-selector Switch: all case arms AND default stay reachable. The
 * symmetric of the Branch TOP case for Switch nodes. */
static void test_cp_reachability_top_switch_all_arms_live(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    sir_node_t* ret_c = sir_return(&a, sir_load_const(&a, 1, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* ret_d = sir_return(&a, sir_load_const(&a, 9, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* sel   = sir_load_local(&a, 0, SIR_DTSHORT, NULL);   /* TOP */
    sir_node_t* cases[1] = { ret_c };
    int32_t     vals[1]  = { 5 };
    sir_node_t* sw    = sir_switch(&a, sel, cases, 1, vals, 1, ret_d, SIR_DTSHORT);
    sir_method_t* m   = sir_method(&a, "f", 0, 0, 1, sw);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    int ic = cp_spine_idx_of(e, ret_c);
    int id = cp_spine_idx_of(e, ret_d);
    TEST_ASSERT_TRUE(ic >= 0 && id >= 0);
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[ic],
        "TOP-selector Switch: case arm must stay reachable");
    TEST_ASSERT_TRUE_MESSAGE(e->reachable[id],
        "TOP-selector Switch: default arm must stay reachable");
    cp_free(e);
    bbq_arena_free(&a);
}

/* ── Inc behaves like LoadLocal — slot-pin invariants ─────────── */

/* Inc.value = LoadLocal(inc.slot) must rename in lockstep with inc.slot
 * when cp_pack moves the slot. Without the inc-walking case in cp_pack's
 * rename pass, inc.slot becomes new_slot[s] but inc.value->load_local.slot
 * stays as the original s — Inc then writes one cell and reads another. */
static void test_cp_inc_value_renamed_with_inc_slot(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* slot 0 = short param p; slot 2 = a short loop-counter cell, with
     * slot 1 deliberately unused so cp_pack must pack slot 2 to a
     * lower-numbered cell. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 2, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* inc  = sir_inc(&a, 2, 1, SIR_DTSHORT,
        sir_load_local(&a, 2, SIR_DTSHORT, NULL), ret);
    sir_node_t* st0  = sir_store_local(&a, 2, SIR_DTSHORT, NULL,
        sir_load_const(&a, 0, SIR_DTSHORT), inc);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 3, st0);

    cp_pack(m, NULL, &a, 1);   /* args_cells = 1 (slot 0 anchored) */

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, inc->tag, "node retagged?");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, inc->inc.value->tag,
        "inc.value must remain a LoadLocal (the slot-pin)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(inc->inc.slot,
        inc->inc.value->load_local.slot,
        "inc.slot and inc.value->load_local.slot must agree post-cp_pack — "
        "Inc writes one cell and reads another otherwise");
    bbq_arena_free(&a);
}

/* Inc must keep its target slot live going in: backward liveness sees
 * the slot read via inc.value's LoadLocal child. live_out[StoreLocal][s]
 * is what DSE consults; without the read visible to liveness the init
 * StoreLocal looks dead and the slot becomes uninitialized at the Inc. */
static void test_cp_inc_keeps_slot_live_via_value_child(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* Drop the post-Inc Return-LoadLocal so the ONLY reader of slot 0
     * downstream of the StoreLocal is the Inc itself. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_const(&a, 7, SIR_DTSHORT), SIR_DTSHORT);
    sir_node_t* inc  = sir_inc(&a, 0, 1, SIR_DTSHORT,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), ret);
    sir_node_t* st0  = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
        sir_load_const(&a, 5, SIR_DTSHORT), inc);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, st0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_compute_liveness(e);

    int st_i  = cp_spine_idx_of(e, st0);
    int inc_i = cp_spine_idx_of(e, inc);
    TEST_ASSERT_TRUE(st_i >= 0 && inc_i >= 0);
    TEST_ASSERT_TRUE_MESSAGE(e->live_in[inc_i][0],
        "Inc reads slot 0 via inc.value LoadLocal — must be live-in at Inc");
    TEST_ASSERT_TRUE_MESSAGE(e->live_out[st_i][0],
        "slot 0 must be live across the init StoreLocal — its only "
        "downstream reader is the Inc's value-child LoadLocal");
    cp_free(e);
    bbq_arena_free(&a);
}

/* Inc.value's LoadLocal is the slot-pin: cp_rewrite must NOT fold it to
 * LoadConst even when the reaching def proves the slot is KNOWN. Folding
 * would (a) break the BURG Inc(LoadLocal) match and (b) detach Inc from
 * slot tracking, leaving the slot looking unread by everything except
 * the Inc's write-side. */
static void test_cp_inc_value_not_constant_folded(void) {
    bbq_arena a; bbq_arena_init(&a, 16384);
    /* StoreLocal(s0, Const(5)) → Inc(s0, +1, value=LoadLocal(s0)) →
     * Return(LoadLocal(s0)). Slot 0 reaches KNOWN(5) at the Inc, no PHI
     * in the way; without the pin cp_rewrite_expr would fold inc.value's
     * LoadLocal to LoadConst(5). The post-Inc Return-LoadLocal keeps
     * slot 0 live so DSE doesn't drop the Inc independently. */
    sir_node_t* ret  = sir_return(&a,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), SIR_DTSHORT);
    sir_node_t* inc  = sir_inc(&a, 0, 1, SIR_DTSHORT,
        sir_load_local(&a, 0, SIR_DTSHORT, NULL), ret);
    sir_node_t* st0  = sir_store_local(&a, 0, SIR_DTSHORT, NULL,
        sir_load_const(&a, 5, SIR_DTSHORT), inc);
    sir_method_t* m  = sir_method(&a, "f", 0, 0, 1, st0);

    cp_engine_t* e = cp_build(m, NULL, &a, NULL, 0);
    cp_rewrite(e);

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_INC, inc->tag,
        "Inc node retagged by rewrite — should not happen");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIR_LOADLOCAL, inc->inc.value->tag,
        "inc.value must remain LoadLocal after cp_rewrite — folding to "
        "LoadConst breaks BURG Inc(LoadLocal) match and detaches Inc "
        "from slot tracking");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
        inc->inc.value->load_local.slot,
        "inc.value LoadLocal still reads slot 0 (the Inc's target)");
    cp_free(e);
    bbq_arena_free(&a);
}

void test_click_partition_suite(void);
void test_click_partition_suite(void) {
    RUN_TEST(test_cp_enumeration_exact);
    RUN_TEST(test_cp_reaching_def_exact);
    RUN_TEST(test_cp_phi_at_forward_merge);
    RUN_TEST(test_cp_defuse_inverts_inputs);
    RUN_TEST(test_cp_phi_at_loop_header);
    RUN_TEST(test_cp_loadlocal_copy_through_trivial_phi);
    RUN_TEST(test_cp_distinct_subs_not_congruent);
    RUN_TEST(test_cp_simd_op_payload_is_identity);
    RUN_TEST(test_cp_branch_arm_load_locals_unify);
    RUN_TEST(test_cp_init_facts_contract);
    RUN_TEST(test_cp_propagate_drain_contract);
    RUN_TEST(test_cp_solve_worklists_drain);
    RUN_TEST(test_cp_in_drain_transitions_terminate);
    RUN_TEST(test_cp_phi_pair_terminates);
    RUN_TEST(test_cp_reverted_identity_pair_not_congruent);
    RUN_TEST(test_cp_memory_shape_reaches_fixpoint);
    RUN_TEST(test_cp_loop_phi_reaches_fixpoint);
    RUN_TEST(test_cp_load_follower_reaches_fixpoint);
    RUN_TEST(test_cp_sub_of_congruent_folds_to_zero);
    RUN_TEST(test_cp_cmp_eq_of_congruent_folds_to_one);
    RUN_TEST(test_cp_partition_type_invariant);
    RUN_TEST(test_cp_split_follower_follows_leader);
    RUN_TEST(test_cp_follower_list_invariant);
    RUN_TEST(test_cp_refine_merges_congruent);
    RUN_TEST(test_cp_refine_separates_noncongruent);
    RUN_TEST(test_cp_refine_loop_terminates);
    RUN_TEST(test_cp_refine_parallel_recurrences);
    RUN_TEST(test_cp_constant_folds);
    RUN_TEST(test_cp_sub_of_congruent);
    RUN_TEST(test_cp_phi_skips_dead_arm);
    RUN_TEST(test_cp_loop_counter_phi_not_constant);
    RUN_TEST(test_cp_switch_prunes_default);
    RUN_TEST(test_cp_commutative_vn);
    RUN_TEST(test_cp_identity_add_zero);
    RUN_TEST(test_cp_absorb_mul_zero_pure);
    RUN_TEST(test_cp_absorb_mul_zero_impure_preserved);
    RUN_TEST(test_cp_liveness_dead_store);
    RUN_TEST(test_cp_liveness_live_store);
    RUN_TEST(test_cp_rewrite_constant_fold);
    RUN_TEST(test_cp_fold_long_add);
    RUN_TEST(test_cp_fold_double_mul);
    RUN_TEST(test_cp_fold_float_add_rounds_to_f32);
    RUN_TEST(test_cp_fold_long_cmp_to_i32);
    RUN_TEST(test_cp_fold_long_propagates_through_local);
    RUN_TEST(test_cp_fold_long_sub_of_self_is_zero_l);
    RUN_TEST(test_cp_fold_long_add_zero_identity);
    RUN_TEST(test_cp_float_add_zero_not_identity);
    RUN_TEST(test_cp_fold_float_rem);
    RUN_TEST(test_cp_fold_double_rem);
    RUN_TEST(test_cp_range_refine_i32);
    RUN_TEST(test_cp_range_refine_i64);
    RUN_TEST(test_cp_refine_keeps_incumbent_symbolic_bound);
    RUN_TEST(test_cp_phi_joins_bounds_of_congruent_reads);
    RUN_TEST(test_cp_phi_two_read_bound_survives_the_loop);
    RUN_TEST(test_cp_phi_bound_agreement_retracts_on_split);
    RUN_TEST(test_cp_difference_bound_composes);
    RUN_TEST(test_cp_difference_bound_composes_through_loop);
    RUN_TEST(test_cp_difference_bound_composes_through_copy_and_low_guard);
    RUN_TEST(test_cp_difference_bound_second_id_must_agree);
    RUN_TEST(test_cp_mem_dse_overwritten_field_store_is_dead);
    RUN_TEST(test_cp_mem_dse_distinct_receivers_both_live);
    RUN_TEST(test_cp_mem_dse_intervening_load_keeps_store);
    RUN_TEST(test_cp_refine_identity_is_canonical);
    RUN_TEST(test_cp_memsize_symbolic_bound_second_guard_folds);
    RUN_TEST(test_cp_memrange_symbolic_bound_second_guard_folds);
    RUN_TEST(test_cp_memrange_second_full_chain_folds);
    RUN_TEST(test_cp_memrange_folds_across_a_respill);
    RUN_TEST(test_cp_refine_survives_all_agree_merge);
    RUN_TEST(test_cp_refine_dropped_when_merge_paths_disagree);
    RUN_TEST(test_cp_refine_dropped_when_merge_arms_refine_apart);
    RUN_TEST(test_cp_refine_dropped_when_a_merge_path_redefines);
    RUN_TEST(test_cp_loop_bound_guard_folds);
    RUN_TEST(test_cp_loop_bound_guard_folds_past_low_guard);
    RUN_TEST(test_cp_loop_bound_folds_through_copy_and_low_guard);
    RUN_TEST(test_cp_range_invariant_bound_survives_loop);
    RUN_TEST(test_cp_range_counter_entry_bound_not_kept);
    RUN_TEST(test_cp_range_kept_upper_bound_does_not_imply_nonneg);
    RUN_TEST(test_cp_range_counter_lower_bound_from_widening);
    RUN_TEST(test_cp_range_unbounded_param_not_claimed_nonneg);
    RUN_TEST(test_cp_range_counter_widens_lo0);
    RUN_TEST(test_cp_range_counter_unbounded_can_overflow_negative);
    RUN_TEST(test_cp_range_counter_loop_condition_survives_skipback);
    RUN_TEST(test_cp_range_counter_loop_under_kept_bound_survives);
    RUN_TEST(test_cp_range_conditional_counter_loop_survives);
    RUN_TEST(test_cp_range_diamond_loop_header_fails_safe);
    RUN_TEST(test_cp_join_nested_outer_kept_inner_dropped);
    RUN_TEST(test_cp_xform_conditional_counter_loop_survives);
    RUN_TEST(test_cp_xform_guarded_counter_loop_survives);
    RUN_TEST(test_cp_no_unproven_transient_refines);
    RUN_TEST(test_cp_ref_nonnull_kept_across_loop);
    RUN_TEST(test_cp_ref_redefined_in_loop_condition_survives);
    RUN_TEST(test_cp_arraylen_follower_rearms_on_leader_descent);
    RUN_TEST(test_cp_unit_fold_bottom_and_top_are_not_facts);
    RUN_TEST(test_cp_unit_meet_is_interval_hull);
    RUN_TEST(test_cp_unit_intersect_narrows);
    RUN_TEST(test_cp_unit_widen_keeps_lower_bound);
    RUN_TEST(test_cp_unit_branch_refine_bounds_load);
    RUN_TEST(test_cp_unit_i64_range_ops);
    RUN_TEST(test_cp_unit_inc_transfer_i32);
    RUN_TEST(test_cp_unit_inc_transfer_i64);
    RUN_TEST(test_cp_fold_i2l);
    RUN_TEST(test_cp_fold_l2i_truncates);
    RUN_TEST(test_cp_fold_i2d_and_f2d);
    RUN_TEST(test_cp_fold_i2c_masks);
    RUN_TEST(test_cp_fold_d2i_jls_narrowing);
    RUN_TEST(test_cp_rewrite_follower_emit);
    RUN_TEST(test_cp_rewrite_dse_pure);
    RUN_TEST(test_cp_rewrite_dse_impure);
    RUN_TEST(test_cp_rewrite_branch_fold);
    RUN_TEST(test_cp_rewrite_empty_branch_fold);
    RUN_TEST(test_cp_rewrite_gvn_slot_collapse);
    RUN_TEST(test_cp_rewrite_checkcast_elim);
    /* Branch / switch fold completeness. */
    RUN_TEST(test_cp_rewrite_branch_fold_true_arm);
    RUN_TEST(test_cp_rewrite_opaque_branch_preserved);
    RUN_TEST(test_cp_rewrite_branch_fold_transitive_constant);
    RUN_TEST(test_cp_rewrite_switch_fold_known_selector);
    /* §4.8 algebraic identities (OUTPUT). */
    RUN_TEST(test_cp_rewrite_mul_one_identity);
    RUN_TEST(test_cp_rewrite_and_neg_one_identity);
    RUN_TEST(test_cp_rewrite_sub_zero_identity);
    RUN_TEST(test_cp_rewrite_shl_zero_identity);
    RUN_TEST(test_cp_rewrite_sub_of_congruent_to_zero);
    RUN_TEST(test_cp_rewrite_absorb_mul_zero_pure_to_const);
    /* Constant fold depth. */
    RUN_TEST(test_cp_rewrite_nested_constant_fold);
    RUN_TEST(test_cp_rewrite_constant_fold_cmp);
    /* NOP compaction. */
    RUN_TEST(test_cp_rewrite_compact_linear_nop_chain);
    RUN_TEST(test_cp_rewrite_compact_preserves_merge_nop);
    RUN_TEST(test_cp_rewrite_compact_preserves_jump_target_nop);
    RUN_TEST(test_cp_rewrite_compact_preserves_tryregion_handler);
    /* Slot bin-packing. */
    RUN_TEST(test_cp_pack_coalesces_disjoint_same_dt);
    RUN_TEST(test_cp_pack_preserves_overlapping_same_dt);
    RUN_TEST(test_cp_pack_dead_store_still_interferes);
    RUN_TEST(test_cp_pack_byte_short_coalesce_as_i32);
    RUN_TEST(test_cp_pack_renames_deep_value_chain);
    RUN_TEST(test_cp_pack_renames_arraycopy_operands);
    RUN_TEST(test_cp_pack_v128_slot_in_frame);
    RUN_TEST(test_cp_pack_renames_setheader_memstore_operands);
    RUN_TEST(test_cp_arraycopy_operand_keeps_store_live);
    RUN_TEST(test_cp_arraycopy_invalidates_array_cell);
    RUN_TEST(test_cp_lockstep_counters_not_congruent);
    RUN_TEST(test_cp_field_loop_cond_arm_not_frozen);
    RUN_TEST(test_cp_tokenizer_rem_branch_survives);
    RUN_TEST(test_cp_field_loop_and_test_survives);
    RUN_TEST(test_cp_getfields_of_distinct_fields_not_congruent);
    RUN_TEST(test_pts_new_flows_through_copy);
    RUN_TEST(test_pts_phi_joins_both_allocations);
    RUN_TEST(test_pts_null_is_an_object);
    RUN_TEST(test_pts_param_is_external);
    RUN_TEST(test_pts_does_not_change_partitions);
    RUN_TEST(test_pts_load_sees_the_store);
    RUN_TEST(test_pts_call_kills_the_cell);
    RUN_TEST(test_pts_strong_update_kills_prior_store);
    RUN_TEST(test_pts_arraystore_weak_on_concrete_array);
    RUN_TEST(test_pts_weak_update_keeps_both);
    RUN_TEST(test_pts_fresh_object_field_is_null_at_entry);
    RUN_TEST(test_pts_two_params_are_distinct_objects);
    RUN_TEST(test_pts_store_through_one_param_not_seen_through_another);
    RUN_TEST(test_pts_store_through_param_is_weak_not_strong);
    RUN_TEST(test_pts_store_through_this_does_not_kill_a_params_field);
    RUN_TEST(test_pts_store_through_loop_site_is_weak_not_strong);
    RUN_TEST(test_pts_store_through_non_loop_site_is_still_strong);
    RUN_TEST(test_pts_null_is_never_a_store_target_or_load_source);
    RUN_TEST(test_pts_fields_of_an_unknown_are_distinct_unknowns);
    RUN_TEST(test_pts_two_calls_to_the_same_callee_name_the_same_object);
    RUN_TEST(test_pts_calls_to_different_callees_name_different_objects);
    RUN_TEST(test_pts_a_call_result_is_maybe_null_and_never_strongly_updated);
    RUN_TEST(test_pts_static_load_sees_the_store_that_reaches_it);
    RUN_TEST(test_pts_static_store_does_not_reach_a_different_static);
    RUN_TEST(test_pts_static_with_no_store_is_unknown_and_maybe_null);
    RUN_TEST(test_pts_static_is_killed_by_a_call);
    RUN_TEST(test_pts_phantom_recursion_is_bounded);
    RUN_TEST(test_pts_store_through_a_field_phantom_is_weak);
    RUN_TEST(test_pts_store_through_loaded_receiver_reaches_its_load);
    RUN_TEST(test_cp_loop_invariant_slot_needs_no_phi);
    RUN_TEST(test_cp_loop_carried_slot_keeps_its_phi);
    RUN_TEST(test_cp_load_forwards_to_the_reaching_store);
    RUN_TEST(test_cp_load_does_not_forward_across_receivers);
    RUN_TEST(test_cp_load_forwards_across_a_preserving_call);
    RUN_TEST(test_cp_load_does_not_forward_across_an_escaping_call);
    RUN_TEST(test_cp_load_forwards_across_two_preserving_calls);
    RUN_TEST(test_cp_load_does_not_forward_across_a_call_two_object_receiver);
    RUN_TEST(test_cp_array_load_does_not_forward_across_a_call);
    RUN_TEST(test_cp_arraylen_of_fresh_array_is_its_size);
    RUN_TEST(test_cp_null_test_refines_pts_on_both_arms);
    RUN_TEST(test_cp_null_refine_does_not_move_partitions);
    RUN_TEST(test_cp_same_op_distinct_const_operands_not_congruent);
    RUN_TEST(test_cp_phis_at_distinct_merges_not_congruent);
    RUN_TEST(test_cp_move_f2i_preserves_raw_nan_bits);
    RUN_TEST(test_cp_f32_const_keeps_exact_bits);
    RUN_TEST(test_cp_long_not_survives);
    RUN_TEST(test_cp_checkcast_kept_when_static_type_wider);
    RUN_TEST(test_cp_checkcast_dropped_when_static_type_matches);
    RUN_TEST(test_cp_loop_accumulator_not_constant);
    RUN_TEST(test_cp_no_reflexive_fold_on_floats);
    RUN_TEST(test_cp_no_sub_self_fold_on_doubles);
    RUN_TEST(test_cp_no_copy_forward_past_source_redef);
    RUN_TEST(test_cp_pack_refs_coalesce_only_same_referent);
    RUN_TEST(test_cp_pack_refs_coalesce_same_referent);
    RUN_TEST(test_cp_pack_anchors_params);
    RUN_TEST(test_cp_pack_int_short_share_i32_pool);
    RUN_TEST(test_cp_pack_distinct_valtypes_dont_coalesce);
    /* cp_compute_reachability contract — Click §4.3/§4.4.1 optimistic
     * reading: TOP-cond/selector leaves all arms live; only KNOWN
     * prunes. */
    RUN_TEST(test_cp_reachability_top_cond_both_arms_live);
    RUN_TEST(test_cp_reachability_known_true_cond_prunes_false_arm);
    RUN_TEST(test_cp_reachability_known_false_cond_prunes_true_arm);
    RUN_TEST(test_cp_reachability_top_switch_all_arms_live);
    /* Inc slot-pin invariants. */
    RUN_TEST(test_cp_inc_value_renamed_with_inc_slot);
    RUN_TEST(test_cp_inc_keeps_slot_live_via_value_child);
    RUN_TEST(test_cp_inc_value_not_constant_folded);
    /* §4.9 PHI Follower (currently disabled; asserts spec-correct
     * behavior — fails until the void-cast in cp_follower_sweep is
     * lifted). */
    RUN_TEST(test_cp_apply_phi_follower_same_partition_contributors);
}

int main(void) {
    test_click_partition_suite();
    printf("\n  %d cases run\n", jt_cases);
    return TEST_SUMMARY("test_click_partition");
}
