// test_jit_tier2.c — the stitcher, at its own level.
//
// Every other JIT test calls jit_compile(code, NULL), which disarms the tiling:
// the cover never runs, no variant is ever selected, and no transition is ever
// stamped. So Part D's variant family and Part E's stitcher had exactly one
// exercise between them — the .wast corpus — and three defects landed in that
// gap and were found there. A bug only the corpus can see is a hole in this
// suite, so this file compiles WITH a context and states the claims separately:
//
//   E-1 TieredEqualsInterp        the interpreter is the oracle, per body
//   E-2 EveryGapIsBridged         the state the machine is in when an instruction
//                                 runs is the state its tile assumed
//   E-3 RegionBoundsAreCanonical  a region opens and closes at state 0
//   E-4 TargetsAreCanonical       every branch target is at state 0
//   E-5 CapacityBoundHolds        transitions are not covered by "one stencil per
//                                 body byte", so the walk must not overrun
//
// Built at TIER2_N=0 the cache is empty at every point and all five hold
// trivially — which is the point of E-0: it reports what was actually exercised,
// so a green at n=0 cannot be read as a green for the mechanism.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include "jav_ttree.h"
#include "jav_jit_meta.h"
#include "bbq_arena.h"
#include "bbq_vec.h"             // the side-table is a bbq_vec: free() on it is an interior pointer
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures, checks;

/* How many values the `spill` fixture holds live at once. The pins over that body
 * reason about displacement, which happens exactly when the cache has fewer slots
 * than this — not at any particular cache size. */
#define SPILL_LIVE 3

#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    failures++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ── one body, both tiers ───────────────────────────────────
 *
 * The body is typechecked exactly as a real one is (the sidetable a branch reads
 * is a typecheck output, not something a fixture can invent), then run twice on
 * a fresh vm: once interpreted, once through the stamped code. */
typedef struct {
    const char*      name;
    const uint8_t*   code;
    size_t           len;
    jav_valtype_t    locals[4];  uint32_t nlocals;
    jav_valtype_t    result;     uint32_t result_tidx;   /* heaptype when result is WVT_REF */
    slot_t           args[4];
    uint8_t          argt[4];
    /* PIN C-7: at n>=2 this body must run an instruction with a v128 IN A
     * REGISTER. Only set on bodies whose every cached value is a v128, so a
     * green cannot be produced by the scalar half of a mixed body. */
    int              wide;
} body_t;

/* Function 0 is `$dummy` — `(func)`, void to void, left interpreted. A fixture
 * calls it to put a CALL between two instructions, which br_table.wast's
 * as-block-value does and which nothing else here does: a call is a control
 * stencil, so it resyncs through the offmap, and that is a different way of
 * arriving at an instruction than falling into it. */
static const uint8_t dummy_code[] = { 0x0b };
static jav_func_t g_funcs[1];
static const jav_functype_t g_sigs[1] = { { NULL, 0, NULL, 0 } };

/* $dummy is COMPILED, not interpreted. On the real path every defined function
 * goes on the JIT tier, so a callee left interpreted here would make the call a
 * different instruction than the one that ships — the caller's stamped code
 * would resync through an interpreter frame instead of another stamped one. */
static jit_func_t* g_dummy_jit;

static void funcs_init(void) {
    memset(g_funcs, 0, sizeof g_funcs);
    g_funcs[0].code = dummy_code; g_funcs[0].code_len = sizeof dummy_code;
    g_funcs[0].num_params = 0; g_funcs[0].num_results = 0;
    if (!g_dummy_jit) {
        bbq_ctx_t dc; bbq_ctx_init(&dc, dummy_code, sizeof dummy_code);
        jav_tctx_t dt = {0};
        g_dummy_jit = jit_compile(dc, &dt);
    }
    if (g_dummy_jit) { g_funcs[0].invoke = jit_invoke; g_funcs[0].invoke_ctx = g_dummy_jit; }
}

static void vm_setup(vm_t* vm, const body_t* b, jav_st_entry_t* st) {
    memset(vm, 0, sizeof *vm);
    jav_vm_init(vm);
    funcs_init();
    vm->cluster.functions = g_funcs; vm->cluster.num_functions = 1;
    bbq_ctx_init(&vm->frame.code, b->code, b->len);
    vm->frame.sidetable = st;
    for (uint32_t i = 0; i < b->nlocals; i++) {
        vm->frame.locals[i] = b->args[i];
        vm->frame.local_types[i] = b->argt[i];
    }
}

/* Two results, compared at the RESULT'S OWN WIDTH. A slot is 16 bytes and only
 * the low `width` of them are the value; the rest is whatever the last occupant
 * left, which the two tiers have no reason to agree about and no obligation to.
 * Comparing 64 bits of it read an i32 body as a mismatch because a v128 had been
 * in that slot on one tier and not the other — and, the other way round, compared
 * a v128 result on its low half alone, which is a differential that cannot see
 * three quarters of what it tests. */
static int slot_eq(slot_t a, slot_t b, jav_valtype_t t) {
    switch (t) {
    case WVT_I32: case WVT_F32: return a.i == b.i;
    case WVT_V128:              return memcmp(a.v.u8, b.v.u8, 16) == 0;
    default:                    return a.l == b.l;
    }
}

/* What the emitter did, per body: the entry-state histogram delta. The offset
 * map this used to dump retired with #16 — the rule actions stamp directly — so
 * the cover's decisions surface as the states the stamps ran at, which is also
 * the quantity every pin below is stated in. */
static void dump_states(const jav_ttree_stats_t* before) {
    const jav_ttree_stats_t* s = jav_ttree_stats();
    printf("       entry states:");
    for (int k = 0; k < 9; k++)
        if (s->entry_state[k] > before->entry_state[k])
            printf(" [%d]x%llu", k,
                   (unsigned long long)(s->entry_state[k] - before->entry_state[k]));
    printf("  transitions +%llu  wide +%llu\n",
           (unsigned long long)(s->transitions - before->transitions),
           (unsigned long long)(s->wide_cached - before->wide_cached));
}

/* PIN E-1. The interpreter is the oracle and it does not call the routine under
 * test: two engines, one body, one answer. This is the check that fails when the
 * grammar and the stencil family disagree about where a result went — the tile
 * says reg0, the stencil pushed, and the spill in between moves a slot that was
 * never written. It reads as a wrong VALUE, which is the only symptom that
 * mechanism has. */
static void run_body(const body_t* b) {
    int failures0 = failures;
    jav_valtype_t res[1] = { b->result };
    jav_vctx_t c = {0};
    c.locals = b->locals; c.nlocals = b->nlocals;
    uint32_t rtidx[1] = { b->result_tidx };
    c.results = res; c.nresults = 1; c.result_tidx = rtidx;
    c.func_sigs = g_sigs; c.nfuncs = 1;      /* index 0 is $dummy */
    jav_st_entry_t* st = NULL; unsigned nst = 0;
    if (!jav_typecheck(b->code, b->len, &c, &st, &nst)) {
        failures++; checks++;
        printf("  FAIL %s: fixture does not typecheck\n", b->name);
        return;
    }

    /* The tier-2 context this body needs: the classes the tree builder reads to
     * resolve a `word` slot. Everything else is empty because these fixtures use
     * no memory, table, global or composite type. */
    uint8_t lcls[4], rcls[1];
    for (uint32_t i = 0; i < b->nlocals; i++) lcls[i] = jav_sclass_of_valtype(b->locals[i]);
    rcls[0] = jav_sclass_of_valtype(b->result);
    jav_tctx_t tcx = {0};
    tcx.local_class = lcls; tcx.nlocals = b->nlocals;
    tcx.result_class = rcls; tcx.nresults = 1;
    /* $dummy's type, so the builder can resolve a `call`. Without it the build
     * DECLINES on the call, the map is never armed, and the body quietly runs
     * tier-1 — a fixture that looks like it exercises the stitcher and does not. */
    static const uint32_t fti[1] = { 0 };
    static const uint32_t tnp[1] = { 0 }, tnr[1] = { 0 };
    static const uint8_t* const tpc[1] = { NULL };
    static const uint8_t* const trc[1] = { NULL };
    tcx.func_type_idx = fti; tcx.nfuncs = 1;
    tcx.type_param_class = tpc; tcx.type_nparams = tnp;
    tcx.type_result_class = trc; tcx.type_nresults = tnr;
    tcx.ntypes = 1;

    vm_t vm;
    vm_setup(&vm, b, st);
    /* The STATUS is part of the answer. An `unreachable` body traps and leaves no
     * result, so the slot the two tiers hand back holds whatever the last value
     * to occupy it left — comparing that is comparing noise, and it agreed only
     * while no fixture put a v128 there. Compare what the body did; compare what
     * it produced only when it produced something. */
    jav_status_t swant = interp_run(&vm, NULL);
    slot_t want = jav_tos(&vm);

    /* A fixture that was never tiled proves nothing about the stitcher, and it
     * looks exactly like one that was: the map disarms and the walk stamps
     * tier-1, which is correct and silent. So the cover's own verdict is checked
     * per body rather than only in aggregate. */
    const jav_ttree_stats_t* s = jav_ttree_stats();
    jav_ttree_stats_t s0 = *s;   /* the whole snapshot: every pin below is a delta */
    uint64_t built0 = s->bodies_built, cov0 = s->bodies_covered;
    uint64_t bf0 = s->bridge_fails, tr0 = s->transitions;

    bbq_ctx_t cc; bbq_ctx_init(&cc, b->code, b->len);
    jit_func_t* h = jit_compile(cc, &tcx);
    if (!h) {
        /* A decline is legal — tier-2 falls back — but a fixture written to
         * exercise the stitcher that quietly declines proves nothing, so it is a
         * failure HERE even though it is not one in the engine. */
        failures++; checks++;
        printf("  FAIL %s: jit_compile declined the body\n", b->name);
        bbq_vec_free(st); return;
    }
    CHECK(s->bodies_built > built0, "E-0 %s: the tree builder declined it", b->name);
    CHECK(s->bodies_covered > cov0, "E-0 %s: the cover declined it", b->name);
    /* PIN C-7 — WideValueCaches. Whether SIMD operands reach registers is a
     * question about the GRAMMAR, so it is asked of a body whose only cacheable
     * values are v128 and answered by this build's own cover. `wide_cached`
     * counts stamps whose RULE named a v128 in a register on either side — the
     * class axis `states_cached` is blind to (a scalar in the same body moves
     * that one; a cached v128 RESULT does not). The fixtures are v128-only, so
     * the counter cannot be satisfied by anything scalar. */
    if (b->wide && JAV_TIER2_N >= 2) {
        CHECK(s->wide_cached > s0.wide_cached,
              "C-7 %s: no rule ever named a v128 in a cache slot", b->name);
        /* …and at n>=4, the case that only exists there: TWO v128s in registers
         * at once, which is what a binary SIMD op needs to run fully cached. Four
         * slots, so no smaller cache can be asked the question. In a v128-only
         * body an entry state of 4 IS two vectors in registers — nothing else in
         * the body could fill four slots. */
        if (JAV_TIER2_N >= 4 && !strcmp(b->name, "v128_bin")) {
            uint64_t deep4 = 0;
            for (int k = 4; k < 9; k++)
                deep4 += s->entry_state[k] - s0.entry_state[k];
            CHECK(deep4 > 0, "C-7 %s: no instruction ran at state >= 4, so no "
                             "binary SIMD op ever had both operands in registers — "
                             "the whole of what n>=4 buys SIMD", b->name);
        }
    }
    /* PIN E-2, per body. A gap the stitcher cannot close drops that body to
     * tier-1: the answer stays right, so the aggregate count was the only
     * evidence and it names no body. This one does, and it is the check that a
     * body written to exercise the cache is still on the tier it was written for. */
    CHECK(s->bridge_fails == bf0, "E-2 %s: the stitcher could not bridge a gap "
          "(state %d -> %d, class %d after op 0x%02x @%u)", b->name,
          s->first_unbridged_from, s->first_unbridged_to, s->first_unbridged_cls,
          s->first_unbridged_op, s->first_unbridged_off);
    /* ENTER THE HANDLE COMPILED ABOVE. jav_jit_run() would look like the right
     * call and is not: it does jit_compile(frame.code, NULL) internally, which
     * disarms the tiling and stamps tier-1. Using it here made this differential
     * compare the interpreter against the tier that was already covered, and
     * every fixture passed for that reason rather than for a good one. */
    vm_setup(&vm, b, st);
    jav_status_t sgot = jit_enter(h, &vm);
    slot_t got = jav_tos(&vm);

    CHECK(sgot == swant, "E-1 %s: interp status %d, jit status %d", b->name,
          (int)swant, (int)sgot);
    if (swant == JAV_RETURN)
        CHECK(slot_eq(want, got, b->result), "E-1 %s: interp=%lld jit=%lld", b->name,
              (long long)want.l, (long long)got.l);
    /* A transition is the mechanism failing to be invisible, so which BODY needed
     * one is the work list. Named here rather than only totalled, because a total
     * says the cover paid somewhere and not where. */
    if (s->transitions > tr0) {
        printf("  %-16s %llu transition(s)\n", b->name,
               (unsigned long long)(s->transitions - tr0));
        dump_states(&s0);
    }
    if (failures > failures0) dump_states(&s0);
    jit_free(h);

    /* PIN E-1 — NoCoverFallsBackToTier1. A body the tiling cannot speak for still
     * runs ON THE JIT and answers what tier-1 answers. Red if it lands on the
     * interpreter, and red if it lands on tier-2 stencils with no cover behind
     * them: with a cache configured, "state 0" is NOT the uncached form — the
     * state-0 variant still leaves its result in a register — so a walk that
     * reads the disarmed map as state 0 stamps caching stencils that nothing
     * spills or reads. That is what this pin exists to catch and it is exactly
     * what happened when it was not written. */
    jit_func_t* h1 = jit_compile(cc, NULL);
    CHECK(h1 != NULL, "E-1 %s: no cover left the body off the JIT entirely", b->name);
    if (h1) {
        vm_setup(&vm, b, st);
        jav_status_t st1 = jit_enter(h1, &vm);
        slot_t t1 = jav_tos(&vm);
        CHECK(st1 == swant, "E-1 %s: uncovered JIT status %d, interp status %d",
              b->name, (int)st1, (int)swant);
        if (swant == JAV_RETURN)
            CHECK(slot_eq(want, t1, b->result), "E-1 %s: uncovered JIT=%lld, interp=%lld",
                  b->name, (long long)t1.l, (long long)want.l);
        jit_free(h1);
    }
    bbq_vec_free(st);
}

/* ── the state claims, over the tiled map ──────────────────
 *
 * These read the cover's answer rather than the stamped code: a state is a
 * property of a program POINT, so what has to hold is a statement about offsets,
 * and it is checkable without running anything. The tree is rebuilt here because
 * the regions are what "a region opens at state 0" is about, and jit_compile
 * frees its own. */
static void state_claims(const body_t* b) {
    int failures0 = failures;
    jav_valtype_t res[1] = { b->result };
    jav_vctx_t c = {0};
    c.locals = b->locals; c.nlocals = b->nlocals;
    uint32_t rtidx[1] = { b->result_tidx };
    c.results = res; c.nresults = 1; c.result_tidx = rtidx;
    c.func_sigs = g_sigs; c.nfuncs = 1;      /* index 0 is $dummy */
    jav_st_entry_t* st = NULL; unsigned nst = 0;
    if (!jav_typecheck(b->code, b->len, &c, &st, &nst)) return;

    uint8_t lcls[4], rcls[1];
    for (uint32_t i = 0; i < b->nlocals; i++) lcls[i] = jav_sclass_of_valtype(b->locals[i]);
    rcls[0] = jav_sclass_of_valtype(b->result);
    jav_tctx_t tcx = {0};
    tcx.local_class = lcls; tcx.nlocals = b->nlocals;
    tcx.result_class = rcls; tcx.nresults = 1;
    /* $dummy's type, so the builder can resolve a `call`. Without it the build
     * DECLINES on the call, the map is never armed, and the body quietly runs
     * tier-1 — a fixture that looks like it exercises the stitcher and does not. */
    static const uint32_t fti[1] = { 0 };
    static const uint32_t tnp[1] = { 0 }, tnr[1] = { 0 };
    static const uint8_t* const tpc[1] = { NULL };
    static const uint8_t* const trc[1] = { NULL };
    tcx.func_type_idx = fti; tcx.nfuncs = 1;
    tcx.type_param_class = tpc; tcx.type_nparams = tnp;
    tcx.type_result_class = trc; tcx.type_nresults = tnr;
    tcx.ntypes = 1;

    /* Compile: the emitter's own accounting is what every claim below reads,
     * as a delta across exactly this compile. */
    const jav_ttree_stats_t* s = jav_ttree_stats();
    jav_ttree_stats_t s0 = *s;
    bbq_ctx_t cc; bbq_ctx_init(&cc, b->code, b->len);
    jit_func_t* h = jit_compile(cc, &tcx);
    if (!h) { bbq_vec_free(st); return; }

    bbq_arena ta; bbq_arena_init(&ta, 16 * 1024);
    jav_ttree_t tree;
    if (jav_ttree_build(cc, &tcx, &ta, &tree)) {
        /* PIN E-3 / E-4. A region opens with nothing carried, and every cut that
         * closes one — end, br, br_if, br_table, return — pushes nothing, so a
         * region's first instruction runs at state 0.
         *
         * That is also E-4, and deliberately: a branch reads its target from the
         * sidetable as a DELTA, so there is no target list to walk here, but in
         * structured control flow every target is a block/loop/end boundary and
         * every one of those is a cut. A region entered with the machine's cache
         * non-empty would run its head in a state an arrival cannot be in — and
         * would leave the offmap pointing at that head's fill instead of the
         * instruction. The emitter counts exactly that (`regions_hot`), so one
         * number covers both defects, for every region of every fixture. */
        CHECK(s->regions_hot == s0.regions_hot,
              "E-3 %s: %llu region(s) entered with the cache non-empty", b->name,
              (unsigned long long)(s->regions_hot - s0.regions_hot));
        /* PIN F-1 MergeIsCanonical and PIN F-2 LoopBackEdgeIsCanonical are
         * INSTANCES of the check above, not separate machinery: a merge is an
         * offset control can arrive at from more than one place, and in
         * structured control flow those are exactly the region boundaries — the
         * `end` an if/else's two arms both reach, and the loop header a back
         * edge returns to. Naming them here so the plan's pin names are greppable
         * and so the claim is stated where it is checked.
         *
         * A body with one region proves neither, so the fixtures that carry them
         * assert they actually split. */
        if (!strncmp(b->name, "if_else.", 8))
            CHECK(tree.nregions > 1, "F-1 %s: one region — no merge to be canonical",
                  b->name);
        if (!strcmp(b->name, "loop") || !strncmp(b->name, "loop_carry", 10))
            CHECK(tree.nregions > 1, "F-2 %s: one region — no back edge to be canonical",
                  b->name);

        /* PIN C-6 BothOperandsRideAtNTwo. The canonical case, and the reason the
         * cache exists: `i32.const A; i32.const B; i32.add` with two slots should
         * touch memory NOT AT ALL. A goes to reg0; B's own variant shifts A to
         * reg1 as it takes reg0, which is free because the stencil performs the
         * shift anyway; the add then reads both from registers.
         *
         * Stated as what the tiling must DO, not as a bypass that forces it. A
         * body constructed so that one tiling is obviously right tests the whole
         * path at once — the grammar can express the state, the DP prefers it,
         * the stitcher stamps it, and the answer is correct — and it fails
         * whichever of those is wrong. Forcing the state instead would have
         * proved the bookkeeping while leaving the two reasons it never runs
         * untested. */
        if (!strcmp(b->name, "const_add") && JAV_TIER2_N >= 2) {
            /* The arithmetic, from the weights, so the expected answer is DERIVED
             * and a change to the model forces this to be redone rather than the
             * number nudged to match whatever the DP now does:
             *
             *   both cached   two constants to registers, add reads both,
             *                 result to a register, nothing touches memory
             *                 = 0
             *   both memory   two stores + two loads + the add's own sp update
             *                 = 5
             *
             * No transition either way — the shift is free — so caching wins by
             * 5 under any weighting where memory access costs anything at all.
             *
             * The add running at state 2 IS both constants reaching registers:
             * nothing else in the body can fill two slots, so the one histogram
             * bucket carries the whole of the old three-part claim. */
            int cost_cached = 0;
            int cost_memory = 4 * JAV_COST_MEM + JAV_COST_MEM;
            uint64_t at2 = s->entry_state[2] - s0.entry_state[2];
            CHECK(cost_cached < cost_memory && at2 == 1,
                  "C-6: the add did not run at state 2 (%llu did) though caching "
                  "both operands costs %d against %d for memory — the DP is not "
                  "obeying its own cost model",
                  (unsigned long long)at2, cost_cached, cost_memory);
            CHECK(s->transitions == s0.transitions,
                  "C-6: %llu transition(s) in a body whose winning cover needs none",
                  (unsigned long long)(s->transitions - s0.transitions));
        }

        /* PIN C-2 CachedBeatsMemory. The plan's wording: "for (i32 i32)->(i32)
         * with both operands in registers, the winning rule is the cached variant
         * and its cost is strictly below the all-mem cover."
         *
         * Recorded BLOCKED while only n=1 existed, because one slot cannot hold
         * both operands. n=2 can, so it is live — and stale-BLOCKED is how a pin
         * quietly stops being owed.
         *
         *   both in registers   no access, no sp update, no transition   = 0
         *   all memory          two loads + one store + the sp update    = 4
         *
         * `const_add` is that shape: two i32 producers feeding one i32.add. */
        if (!strcmp(b->name, "const_add") && JAV_TIER2_N >= 2) {
            int cost_cached = 0;
            int cost_allmem = 3 * JAV_COST_MEM + JAV_COST_MEM;
            CHECK(cost_cached < cost_allmem,
                  "C-2: the cost model does not separate a cached (i32 i32)->i32 "
                  "from the all-memory cover (%d vs %d)", cost_cached, cost_allmem);
            CHECK(s->entry_state[2] - s0.entry_state[2] == 1,
                  "C-2: the cheaper cover did not win — nothing stamped at state 2");
        }

        /* PIN D-2 NoSpUpdateWhenFullyCached. The plan's wording: "the fully-cached
         * i32.add variant contains no store to the frame's sp."
         *
         * Also recorded BLOCKED at n=1, for a reason that expired when n=2 built:
         * a two-operand instruction could not be fully cached with one slot. It
         * can with two. §2.3: "the stack pointer need not be updated in
         * instruction implementations that can access all stack items in
         * registers."
         *
         * WHAT THIS ACTUALLY CHECKS, stated because it is less than the pin says:
         * the fully-cached variant is strictly smaller than the one-cached form,
         * which is strictly smaller than the uncached one. The sp store is one of
         * the things that disappears across that ordering, and rule_cost prices it
         * on the same condition the emitter omits it on — but neither is a
         * disassembly, and "contains no store to sp" is an assertion about
         * instructions this suite cannot read. The size ordering is the strongest
         * available statement; calling it the pin would be the weasel. */
        if (JAV_TIER2_N >= 2 && !strcmp(b->name, "const_add")) {
            int v0 = jav_variant[STENCIL_GEN_ST_I32_ADD][0];
            int v1 = jav_variant[STENCIL_GEN_ST_I32_ADD][1];
            int v2 = jav_variant[STENCIL_GEN_ST_I32_ADD][2];
            if (v0 >= 0 && v1 >= 0 && v2 >= 0) {
                uint32_t s0 = stencil_table[v0].code_size,
                         s1 = stencil_table[v1].code_size,
                         s2 = stencil_table[v2].code_size;
                CHECK(s2 < s1 && s1 < s0,
                      "D-1/D-2: i32.add is %u bytes uncached, %u with one operand "
                      "cached, %u fully cached — each state should strictly shrink "
                      "as another access and finally the sp update fall away",
                      s0, s1, s2);
            }
        }

        /* PIN C-3 SpillIsAChainRule. `spill` is i32.const x3 feeding two adds, so
         * three values are live at the peak and the cache holds one. The (n+1)th
         * has to go somewhere and the grammar's chain rule is where — the claim
         * is that such a body COVERS rather than failing to tile. */
        if (!strcmp(b->name, "spill"))
            CHECK(tree.nregions >= 1 && s->bodies_covered > s0.bodies_covered,
                  "C-3 spill: a body needing n+1 live values did not cover");

        /* PIN C-4 TransitionIsNotFree. The converse of C-2, and the one that says
         * whether the cost model can DECLINE. In `spill` the first two constants
         * are each displaced from reg0 by the next constant, so caching them buys
         * nothing and costs a spill; C3 prices that chain rule at the spill
         * stencil's measured bytes precisely so the DP can see it. Their tiles
         * must therefore choose the memory form — out class JSC_COUNT — while the
         * last constant, consumed immediately by the add above it, caches.
         *
         * Red if the winner pays for a round trip it did not need, which would
         * also be the explanation for the corpus reading 4.3 transitions per
         * cached use. */
        if (!strcmp(b->name, "spill") && JAV_TIER2_N == 1) {
            /* n=1 only: with two slots the first constant survives in reg1 and
             * the trade below does not arise. With none there is nowhere but
             * memory and nothing to weigh.
             *
             * The arithmetic, so the expectation is derived rather than observed.
             * @0 is displaced from reg0 by @2 before anything reads it:
             *
             *   straight to memory   1 store + 1 sp update            = 2
             *   cache then spill     0 to produce + a spill (1 + 4)   = 5
             *
             * A transition costs its memory access AND its dispatch, which is the
             * whole of why it must lose here — it does the same store the direct
             * form does, and pays a dispatch on top. Red under a model that
             * cannot see dispatches, which is the byte model this was written
             * against. */
            int cost_direct = JAV_COST_MEM + JAV_COST_MEM;
            int cost_round  = JAV_COST_TRANSITION;
            /* The whole claim in the emitter's own quantities. @0 and @2 straight
             * to memory and @4 cached means NO transition anywhere (a cached-
             * then-spilled constant would stamp one), and exactly the two adds
             * run with a cached operand — the inner one over @4's value, the
             * outer one over the inner's result. */
            CHECK(cost_direct < cost_round && s->transitions == s0.transitions,
                  "C-4 spill: %llu transition(s) — a constant was cached then "
                  "spilled, costing %d, where going straight to memory costs %d",
                  (unsigned long long)(s->transitions - s0.transitions),
                  cost_round, cost_direct);
            CHECK(s->entry_state[1] - s0.entry_state[1] == 2,
                  "C-4 spill: %llu stamp(s) at state 1, not the two adds — either "
                  "@4 went to memory (caching it costs 0 and saves %d) or a "
                  "displaced constant stayed cached",
                  (unsigned long long)(s->entry_state[1] - s0.entry_state[1]),
                  2 * JAV_COST_MEM);
        }
        /* …and the same question at n>=2, which is where it can actually fail.
         * The guard above is `== 1`, and n=1 stamps no transitions at all, so the
         * one pin that says "do not cache a value only to spill it" has been
         * passing on a cache size where the situation cannot arise. Every one of
         * the corpus's 2506 transitions is at n>=2 and none of them is at a region
         * boundary, so they are round trips inside a tree — exactly what this
         * pin exists to forbid, going unchecked.
         *
         * `spill` is the same body: three constants, two adds. At n=2 the cache
         * holds two, so @0 is displaced by @2 and @4 before anything reads it, and
         * the arithmetic is what it was at n=1 —
         *
         *   straight to memory   1 store + 1 sp update            = 2
         *   cache then spill     0 to produce + a spill (1 + 4)   = 5
         *
         * …and the second add, at @7, must take @0's value from MEMORY rather than
         * fill it back: its memory-operand form loads it once, where a fill loads
         * it once AND pays a dispatch.
         *
         *   operand from memory  1 load                           = 1
         *   fill then read reg   a fill (1 + 4)                   = 5 */
        /* …wherever the fixture's three simultaneously-live values do NOT fit.
         * That is the condition the lemma is about — a value displaced before its
         * use should never have been cached — and not a cache size: with three
         * slots or more nothing is displaced and caching @0 is simply right. */
        if (!strcmp(b->name, "spill") && JAV_TIER2_N >= 2 && SPILL_LIVE > JAV_TIER2_N) {
            /* Same claim, the n>=2 shape: @0 to memory and no fill-back means
             * ZERO transitions; the inner add runs at state 2 (both its
             * operands cached) and the outer at state 1 (the inner's result
             * cached, @0's value read from memory where it sits). */
            CHECK(s->transitions == s0.transitions,
                  "C-4 spill: %llu transition(s) — either @0 was cached then "
                  "spilled (%d against %d for going straight to memory) or @7 "
                  "filled its deep operand back (%d against %d for reading it)",
                  (unsigned long long)(s->transitions - s0.transitions),
                  JAV_COST_TRANSITION, 2 * JAV_COST_MEM,
                  JAV_COST_TRANSITION, JAV_COST_MEM);
            CHECK(s->entry_state[2] - s0.entry_state[2] == 1
                  && s->entry_state[1] - s0.entry_state[1] == 1,
                  "C-4 spill: expected the inner add at state 2 and the outer at "
                  "state 1; got +%llu at 2, +%llu at 1",
                  (unsigned long long)(s->entry_state[2] - s0.entry_state[2]),
                  (unsigned long long)(s->entry_state[1] - s0.entry_state[1]));
        }
    }
    if (failures > failures0) dump_states(&s0);   /* which states this compile ran */
    bbq_arena_free(&ta);

    jit_free(h); bbq_vec_free(st);
    (void)nst;
}

/* ── the fixtures ──────────────────────────────────────────
 *
 * Chosen by the SHAPE of the state sequence they force, not by opcode. At n=1
 * there are only four gaps a body can put between two instructions, and each
 * needs a body that reaches it. */
#define L0 0x20,0x00            /* local.get 0 */
#define L1 0x20,0x01            /* local.get 1 */
#define END 0x0b

/* ── the control cross-product ─────────────────────────────
 *
 * The bodies above are the ones that broke, which is not the same as the ones
 * that exist. What decides whether the cache is handled right at a control edge
 * is a cross-product, so it is enumerated rather than sampled:
 *
 *   CUT     how control LEAVES: fall out of a block, br, br_if, br_table,
 *           if/else, return, unreachable — every form that ends a region
 *   CLASS   what it CARRIES: the four cacheable classes, plus a reference,
 *           which must never reach a slot and so is a negative case
 *   DEAD    whether unreachable code follows the cut, which builds no tree and
 *           therefore has no state to read
 *
 * Each body is assembled here instead of hand-written as bytes: 5 classes x 7
 * cuts x 2 is 70 arrays, and a hand-typed sleb is a bug that reads as a fixture.
 * The interpreter is the oracle for all of them, so none needs an expected
 * value written down either. */
typedef enum {
    CUT_FALL, CUT_BR, CUT_BRIF, CUT_BRTABLE, CUT_IF, CUT_RETURN, CUT_UNREACH,
    CUT_COUNT
} cut_t;
static const char* kCutName[CUT_COUNT] = {
    "fall", "br", "br_if", "br_table", "if_else", "return", "unreachable"
};

/* The five carried classes, with the blocktype byte each needs. `ref` is here to
 * be REFUSED a slot, not to ride in one. */
/* A reference is WVT_REF plus a heaptype carried alongside — the validator has
 * no `funcref` tag of its own — so the row carries the typeidx too. */
typedef struct { const char* name; jav_valtype_t vt; uint8_t bt; uint32_t tidx; } carried_t;
static const carried_t kCarried[] = {
    { "i32", WVT_I32, 0x7f, 0 },
    { "i64", WVT_I64, 0x7e, 0 },
    { "f32", WVT_F32, 0x7d, 0 },
    { "f64", WVT_F64, 0x7c, 0 },
    { "ref", WVT_REF, 0x70, (uint32_t)HT_FUNC },
    /* Two slots wide, so it only caches at n>=2 and it is the one class whose
     * transitions move the state by two. Added after the fact: v128 caching was
     * built and shipped green on the corpus alone, because this table had every
     * class except the one the feature was about. */
    { "v128", WVT_V128, 0x7b, 0 },
};
#define NCARRIED ((int)(sizeof kCarried / sizeof kCarried[0]))

/* A constant of the carried class. Two distinct values per class, so a body that
 * takes the wrong arm of a diamond reads as a wrong ANSWER rather than a
 * coincidence. A funcref has only one constant this harness can make. */
static size_t emit_const(uint8_t* p, jav_valtype_t vt, int which) {
    size_t k = 0;
    switch (vt) {
    case WVT_I32: p[k++] = 0x41; p[k++] = which ? 0x2a : 0x11; break;   /* 42 / 17 */
    case WVT_I64: p[k++] = 0x42; p[k++] = which ? 0x2a : 0x11; break;
    case WVT_F32: p[k++] = 0x43; p[k++] = 0; p[k++] = 0; p[k++] = which ? 0x28 : 0x88;
                  p[k++] = which ? 0x42 : 0x41; break;                  /* 42.0 / 17.0 */
    case WVT_F64: p[k++] = 0x44; for (int i = 0; i < 6; i++) p[k++] = 0;
                  p[k++] = which ? 0x45 : 0x31; p[k++] = which ? 0x40 : 0x40; break;
    case WVT_V128:                                                      /* v128.const */
        p[k++] = 0xfd; p[k++] = 0x0c;
        for (int i = 0; i < 16; i++) p[k++] = (uint8_t)(which ? 0x2a + i : 0x11 + i);
        break;
    default:      p[k++] = 0xd0; p[k++] = 0x70; break;                  /* ref.null func */
    }
    return k;
}

/* One body for a (cut, class, dead) triple. Param 0 is always i32 — the
 * condition a br_if or an if needs — and the result is the carried class, so
 * whatever crosses the edge is what the function returns and the differential
 * sees it. */
static size_t emit_cut_body(uint8_t* p, cut_t cut, const carried_t* c, int dead) {
    size_t k = 0;
    if (cut != CUT_RETURN) { p[k++] = 0x02; p[k++] = c->bt; }   /* block (result T) */
    switch (cut) {
    case CUT_FALL:                                              /* arrive by falling out */
        k += emit_const(p + k, c->vt, 1);
        break;
    case CUT_BR:
        k += emit_const(p + k, c->vt, 1);
        p[k++] = 0x0c; p[k++] = 0x00;                           /* br 0 */
        break;
    case CUT_BRIF:                                              /* the DIAMOND: two arrivals */
        k += emit_const(p + k, c->vt, 1);
        p[k++] = 0x20; p[k++] = 0x00;                           /* local.get 0 */
        p[k++] = 0x0d; p[k++] = 0x00;                           /* br_if 0 — carries it */
        p[k++] = 0x1a;                                          /* drop */
        k += emit_const(p + k, c->vt, 0);                       /* …or produce the other */
        break;
    case CUT_BRTABLE:
        k += emit_const(p + k, c->vt, 1);
        p[k++] = 0x41; p[k++] = 0x00;                           /* i32.const 0 (index) */
        p[k++] = 0x0e; p[k++] = 0x01; p[k++] = 0x00; p[k++] = 0x00;
        break;
    case CUT_IF:                                                /* the other diamond */
        p[k++] = 0x20; p[k++] = 0x00;                           /* local.get 0 */
        p[k++] = 0x04; p[k++] = c->bt;                          /* if (result T) */
        k += emit_const(p + k, c->vt, 1);
        p[k++] = 0x05;                                          /* else */
        k += emit_const(p + k, c->vt, 0);
        p[k++] = 0x0b;                                          /* end if */
        break;
    case CUT_RETURN:
        k += emit_const(p + k, c->vt, 1);
        p[k++] = 0x0f;                                          /* return */
        break;
    case CUT_UNREACH:
        p[k++] = 0x00;                                          /* unreachable */
        break;
    default: break;
    }
    /* §7.6 dead code: builds no tree, so it has no cache state to read. Only
     * after a cut that actually ends the flow — a fall-through or a diamond has
     * no unreachable tail. */
    if (dead && (cut == CUT_BR || cut == CUT_BRTABLE || cut == CUT_RETURN || cut == CUT_UNREACH))
        k += emit_const(p + k, c->vt, 0);
    if (cut != CUT_RETURN) p[k++] = 0x0b;                       /* end block */
    p[k++] = 0x0b;                                              /* end func */
    return k;
}

/* 0->0: both operands from memory. */
static const uint8_t f_mem_mem[]  = { L0, L1, 0x6a, END };                /* x + y */
/* a producer that caches, consumed straight away: 0->1->0. */
static const uint8_t f_const_add[] = { 0x41,0x07, 0x41,0x05, 0x6a, END };  /* 7 + 5 */
/* a cached value forced to memory by a second producer under it — the SPILL. */
static const uint8_t f_spill[]    = { 0x41,0x03, 0x41,0x04, 0x41,0x05, 0x6a, 0x6a, END };
/* a memory value a consumer wants in a register — the FILL. */
static const uint8_t f_fill[]     = { L0, L1, 0x6a, 0x41,0x02, 0x6a, END };
/* i64 and f64: the transition stencils are per CLASS, so one body each or the
 * spill/fill table is only ever proved for i32. */
static const uint8_t f_i64[]      = { 0x42,0x09, 0x42,0x04, 0x7c, END };  /* 9 + 4 */
static const uint8_t f_f64[]      = { 0x44,0,0,0,0,0,0,0x22,0x40,        /* 9.0 */
                                      0x44,0,0,0,0,0,0,0x10,0x40,        /* 4.0 */
                                      0xa0, END };
/* a `word` mover feeding an arithmetic consumer: local.get is declared `word`,
 * has no class to cache AS, and so leaves its result in memory — the case where
 * the grammar and the family disagreed. */
static const uint8_t f_word[]     = { L0, 0x41,0x01, 0x6a, L1, 0x6a, END };
/* control: a branch target has to be canonical, and a loop's back edge arrives
 * at one too. */
static const uint8_t f_branch[]   = { 0x02,0x7f,                          /* block (result i32) */
                                        L0, 0x41,0x0a, 0x6a,
                                        L1, 0x0d,0x00,                    /* br_if 0 */
                                        0x41,0x63, 0x6a,
                                      END, END };
/* br_table: the multi-target branch. Its labels are the offsets the machine can
 * arrive at from a table lookup rather than a fall-through, so it is the widest
 * version of E-4 there is — one instruction, many targets. */
static const uint8_t f_brtable[]  = { 0x02,0x40,                          /* block  (label 1) */
                                        0x02,0x40,                        /*   block  (label 0) */
                                          L0,
                                          0x0e,0x01,0x00,0x01,            /*     br_table [0] default 1 */
                                        END,
                                      END,
                                      L1, END };
/* loop: a BACK edge. The target is behind the branch, so the state the machine
 * carries round the loop has to match the state the loop head was tiled at —
 * the one place a fall-through argument says nothing about. */
static const uint8_t f_loop[]     = { 0x02,0x40,                          /* block */
                                        0x03,0x40,                        /*   loop */
                                          L0, 0x45,                       /*     local.get 0; i32.eqz */
                                          0x0d,0x01,                      /*     br_if 1 (exit) */
                                          L0, 0x41,0x01, 0x6b,            /*     local.get 0; 1; sub */
                                          0x21,0x00,                      /*     local.set 0 */
                                          0x0c,0x00,                      /*     br 0 (back edge) */
                                        END,
                                      END,
                                      L1, END };
/* A branch that CARRIES A VALUE, with dead code behind it — br_table.wast's
 * `as-loop-first`, verbatim:
 *     (loop (result i32) (br_table 1 1 (i32.const 3) (i32.const 0)) (i32.const 1))
 * The fixtures above all branch with an empty operand stack, which is the case
 * where a cut trivially leaves the cache empty. Here the branch has a live
 * operand at the moment it is taken, so what the cache holds when control leaves
 * is a real question rather than an arithmetic identity. */
static const uint8_t f_loop_carry[] = { 0x03,0x7f,                        /* loop (result i32) */
                                          0x41,0x03,                      /*   i32.const 3  (carried) */
                                          0x41,0x00,                      /*   i32.const 0  (index) */
                                          0x0e,0x01,0x01,0x01,            /*   br_table [1] default 1 */
                                          0x41,0x01,                      /*   dead */
                                        END, END };
/* A FORWARD branch carrying a value to a block end — br_table.wast's
 * `as-block-value`, less its call:
 *     (block (result i32) (nop) (br_table 0 0 0 (i32.const 2) (i32.const 0)))
 * The target is the block's end rather than a following instruction, so this is
 * the arrival the fall-through argument says nothing about, with a live operand
 * at the moment control leaves. */
static const uint8_t f_block_carry[] = { 0x02,0x7f,                       /* block (result i32) */
                                           0x01,                          /*   nop */
                                           0x10,0x00,                      /*   call $dummy */
                                           0x41,0x02,                      /*   i32.const 2 (carried) */
                                           0x41,0x00,                      /*   i32.const 0 (index) */
                                           0x0e,0x02,0x00,0x00,0x00,       /*   br_table [0,0] default 0 */
                                         END, END };
/* E-5: many gaps in few bytes. Every 1-byte add can imply a transition, so a
 * body of them is where "one stencil per body byte" stops covering the walk. */
static const uint8_t f_dense[]    = { 0x41,0x01, 0x41,0x01, 0x6a, 0x41,0x01, 0x6a,
                                      0x41,0x01, 0x6a, 0x41,0x01, 0x6a, 0x41,0x01,
                                      0x6a, 0x41,0x01, 0x6a, 0x41,0x01, 0x6a, END };

/* ── a value TWO SLOTS wide ─────────────────────────────────
 *
 * Everything above carries one-slot values, where "how many items an instruction
 * pops" and "how many cache slots it frees" are the same number and nothing can
 * tell them apart. A v128 is the class where they part company, so these are the
 * bodies that ask the walk which unit it is counting in.
 *
 * `V128C(b)` is a v128.const whose 16 bytes start at b, so two of them differ in
 * every lane and a body that reads the wrong one reads as a wrong ANSWER. */
#define V128C(b) 0xfd,0x0c, (b)+0,(b)+1,(b)+2,(b)+3,(b)+4,(b)+5,(b)+6,(b)+7, \
                            (b)+8,(b)+9,(b)+10,(b)+11,(b)+12,(b)+13,(b)+14,(b)+15
/* Nothing but v128 in the whole body: a cached state here IS a cached v128, which
 * is what makes this the one that can carry PIN C-7. THREE unary ops, not one,
 * because the region's result has to end in memory: a single `v128.not` pays 3 to
 * take its operand from memory and 6 to spill the result back, against 5 for the
 * all-memory form, and the DP declines — correctly, and that is C-4's rule, not a
 * failure of the mechanism. From the second op on, the chain rule that keeps a
 * value in reg0 is free, so the cached cover wins 12 to 18. */
static const uint8_t f_v128_uni[]  = { V128C(0x11), 0xfd,0x4d, 0xfd,0x4d, 0xfd,0x4d, END };
/* Two of them, so at n=2 one has to go to memory and come back — the spill and
 * fill of a value whose transition moves the state by TWO. At n>=4 both fit and
 * this is the only fixture that can reach a binary SIMD op with BOTH operands in
 * registers: a v128 spends two slots, so four is the first cache size at which
 * that is arithmetically possible, and below it the case cannot be tested at any
 * price rather than merely failing to arise. */
static const uint8_t f_v128_bin[]  = { V128C(0x11), V128C(0x21), 0xfd,0x4e,   /* v128.and */
                                       0xfd,0x1b,0x02, END };                 /* i32x4.extract_lane 2 */
/* A cached v128 consumed into a NARROWER class: two slots go in, one comes out,
 * and the arithmetic after it runs in whatever the walk thinks slot 0 now holds. */
static const uint8_t f_v128_lane[] = { V128C(0x11), 0xfd,0x1b,0x00,           /* extract_lane 0 */
                                       0x41,0x05, 0x6a, END };
/* …and the same with a one-slot SURVIVOR underneath it, which is the shape where
 * a shift counted in items lands on the v128's second half instead of the value
 * below it. */
static const uint8_t f_v128_surv[] = { 0x41,0x03, V128C(0x21), 0xfd,0x1b,0x01,
                                       0x6a, END };

/* ── the operand-class cross-product ───────────────────────
 *
 * The control matrix above varies what a body BRANCHES with; this one varies
 * what an instruction CONSUMES. Both operands of every arithmetic fixture are
 * the same class, and the conformance residue is exactly the shapes where they
 * are not: `i64.store(i32 addr, i64 value)`, `f32.store`, `f64.store`,
 * `i64.store16/32` all mismatch while `i32.store(i32, i32)` passes. In state k
 * the top k operands are cached, so a mixed pair is where the slot's class and
 * the instruction's class part company — and nothing sampled that.
 *
 * `select` gives the shape with no memory to set up: (T, T, i32) is a mixed pair
 * for every T but i32, and its condition is the topmost operand. */
static size_t emit_select_body(uint8_t* p, const carried_t* c) {
    size_t k = 0;
    k += emit_const(p + k, c->vt, 1);                   /* v1 : T          */
    k += emit_const(p + k, c->vt, 0);                   /* v2 : T          */
    p[k++] = 0x20; p[k++] = 0x00;                       /* cond : i32 — TOP */
    /* §4.4: bare `select` is numeric-only — a reference operand needs the typed
     * form, which names the operand type in a vector. Two encodings, one shape. */
    if (c->vt == WVT_REF) { p[k++] = 0x1c; p[k++] = 0x01; p[k++] = c->bt; }
    else                    p[k++] = 0x1b;
    p[k++] = 0x0b;
    return k;
}

/* And the same question for a conversion, whose one operand is a DIFFERENT class
 * from its result: what goes into the slot and what comes out of it disagree. */
static size_t emit_convert_body(uint8_t* p) {
    size_t k = 0;
    p[k++] = 0x42; p[k++] = 0x2a;                       /* i64.const 42     */
    p[k++] = 0xb9;                                      /* f64.convert_i64_s */
    p[k++] = 0x44; for (int i = 0; i < 6; i++) p[k++] = 0;
    p[k++] = 0x45; p[k++] = 0x40;                       /* f64.const 42.0   */
    p[k++] = 0xa0;                                      /* f64.add          */
    p[k++] = 0x0b;
    return k;
}

#define B(nm, arr, nl, rt) { nm, arr, sizeof arr, {WVT_I32,WVT_I32}, nl, rt, 0, {{0}}, {0}, 0 }
/* …and one whose every cacheable value is a v128, so PIN C-7 can read a cached
 * state as a cached v128 rather than as "something scalar in the same body". */
#define BW(nm, arr, nl, rt) { nm, arr, sizeof arr, {WVT_I32,WVT_I32}, nl, rt, 0, {{0}}, {0}, 1 }

int main(void) {
    body_t bodies[] = {
        B("mem_mem",   f_mem_mem,   2, WVT_I32),
        B("const_add", f_const_add, 0, WVT_I32),
        B("spill",     f_spill,     0, WVT_I32),
        B("fill",      f_fill,      2, WVT_I32),
        B("i64",       f_i64,       0, WVT_I64),
        B("f64",       f_f64,       0, WVT_F64),
        B("word",      f_word,      2, WVT_I32),
        B("branch",    f_branch,    2, WVT_I32),
        B("brtable",   f_brtable,   2, WVT_I32),
        B("loop",      f_loop,      2, WVT_I32),
        B("loop_carry", f_loop_carry, 0, WVT_I32),
        B("block_carry", f_block_carry, 0, WVT_I32),
        B("dense",     f_dense,     0, WVT_I32),
        BW("v128_uni", f_v128_uni,  0, WVT_V128),
        BW("v128_bin", f_v128_bin,  0, WVT_I32),
        B("v128_lane", f_v128_lane, 0, WVT_I32),
        B("v128_surv", f_v128_surv, 0, WVT_I32),
    };
    /* Arguments the two-local bodies run with, so a wrong operand is a wrong
     * ANSWER: distinct, non-zero, and not equal to each other's sum. */
    for (size_t i = 0; i < sizeof bodies / sizeof bodies[0]; i++) {
        bodies[i].args[0].i = 11; bodies[i].argt[0] = T_INT;
        bodies[i].args[1].i = 29; bodies[i].argt[1] = T_INT;
    }

    printf("tier-2 stitcher (TIER2_N=%d)\n", JAV_TIER2_N);

    /* PIN D-3 — Tier1Unchanged. With the tier off the table IS tier-1's: the only
     * form of any opcode is the plain stencil, nothing is ever left in a register,
     * and there are no transitions to stamp. That is what makes TIER2_N a knob
     * rather than a fork, and the way it stops being true is a variant leaking
     * into slot 0 — the entry the driver indexes by default. */
    if (JAV_TIER2_N == 0) {
        int nrow = (int)(sizeof jav_variant / sizeof jav_variant[0]);
        for (int s = 0; s < nrow; s++) {
            /* A row for a stencil that is not an opcode base is zero-filled; the
             * populated ones must name THEMSELVES and leave nothing cached. */
            CHECK(jav_variant[s][0] == 0 || jav_variant[s][0] == s,
                  "D-3: stencil %d has a variant (%d) with the tier off", s, jav_variant[s][0]);
            CHECK(jav_variant_fs[s][0] <= 0,
                  "D-3: stencil %d leaves state %d with the tier off", s, jav_variant_fs[s][0]);
        }
        for (unsigned c = 0; c + 1 < JAV_SCLASS_FINAL; c++)
            for (int k = 0; k < (JAV_TIER2_N ? JAV_TIER2_N : 1); k++) {
                CHECK(jav_spill[c][k] < 0, "D-3: a spill stencil exists with the tier off");
                CHECK(jav_fill[c][k]  < 0, "D-3: a fill stencil exists with the tier off");
            }
    }

    /* PIN C-5 — DeeperSlotIsReachable. A value reaches slot k+1 by STAYING in the
     * cache while something is pushed above it — the survivor shift the consuming
     * variant already performs (`CACHE_R1 = CACHE_R0`). It costs nothing, because
     * the stencil that does it was going to run anyway.
     *
     * Without a rule saying so, the only edge into a deeper slot is the fill from
     * memory, and the DP's sole route to state 2 is to spill a value and refill
     * it one slot down — a round trip it correctly refuses. Every state above 1 is
     * then unreachable, the variants for them are dead code, and the meters read
     * identical at n=1 and n=2 because nothing ever entered the state.
     *
     * Checked after the fixtures have run — it is an aggregate over what they
     * did, and the table-level pins around here fire before any body compiles. */

    /* PIN D-5 — PolyResultCaches. `local.get` is declared `( -- word result)`: the
     * SIGNATURE carries no storage class, because the class is a property of the
     * tile and not of the opcode. That is not a reason to leave the value in
     * memory — it is a slot either way, and the class the tile resolved is what
     * picks the spill that re-tags it. Leaving the most common opcode in wasm
     * outside the cache is what made the tier engage on under 5% of instructions
     * and emit more transitions than uses. */
    if (JAV_TIER2_N > 0) {
        CHECK(jav_variant_fs[STENCIL_GEN_ST_LOCAL_GET][0] == 1,
              "D-5: local.get leaves its result in memory (exit state %d), so the "
              "most common opcode in wasm never enters the cache",
              jav_variant_fs[STENCIL_GEN_ST_LOCAL_GET][0]);
        /* At n=1 there is no state-1 variant and there must not be: local.get
         * takes nothing and produces one value, so a cache already holding an
         * item would need two slots. That is Ertl's OVERFLOW and §2.5's omission
         * rule — the tiler reaches the state through a transition instead — so
         * the assertion is that the family is honest about it, not that the
         * variant exists. It becomes a real variant at n>=2. */
        CHECK((jav_variant[STENCIL_GEN_ST_LOCAL_GET][1] >= 0) == (JAV_TIER2_N >= 2),
              "D-5: local.get's state-1 variant should exist exactly when the cache "
              "has room for two items (n=%d)", JAV_TIER2_N);
    }

    for (size_t i = 0; i < sizeof bodies / sizeof bodies[0]; i++) {
        run_body(&bodies[i]);
        state_claims(&bodies[i]);
    }

    /* Operands of MIXED class: select's (T, T, i32), and a conversion whose
     * operand and result are different classes. */
    for (int ci = 0; ci < NCARRIED; ci++) {
        static uint8_t code[64];
        char nm[48];
        body_t b; memset(&b, 0, sizeof b);
        b.len = emit_select_body(code, &kCarried[ci]);
        b.code = code;
        b.locals[0] = WVT_I32; b.nlocals = 1;
        b.result = kCarried[ci].vt; b.result_tidx = kCarried[ci].tidx;
        b.args[0].i = 1; b.argt[0] = T_INT;
        snprintf(nm, sizeof nm, "select.%s", kCarried[ci].name);
        b.name = nm;
        run_body(&b);
        state_claims(&b);
    }
    {
        static uint8_t code[64];
        body_t b; memset(&b, 0, sizeof b);
        b.len = emit_convert_body(code);
        b.code = code; b.name = "convert.i64_to_f64";
        b.result = WVT_F64;
        run_body(&b);
        state_claims(&b);
    }

    /* The cross-product: every cut form, carrying every class, with and without
     * an unreachable tail. Assembled rather than listed, so adding a cut form is
     * a case in emit_cut_body and not seventy more byte arrays. */
    for (int cut = 0; cut < CUT_COUNT; cut++)
        for (int ci = 0; ci < NCARRIED; ci++)
            for (int dead = 0; dead < 2; dead++) {
                static uint8_t code[64];
                char nm[48];
                body_t b; memset(&b, 0, sizeof b);
                b.len = emit_cut_body(code, (cut_t)cut, &kCarried[ci], dead);
                b.code = code;
                b.locals[0] = WVT_I32; b.nlocals = 1;   /* the branch/if condition */
                b.result = kCarried[ci].vt; b.result_tidx = kCarried[ci].tidx;
                b.args[0].i = 1; b.argt[0] = T_INT;     /* take the branch */
                snprintf(nm, sizeof nm, "%s.%s%s", kCutName[cut], kCarried[ci].name,
                         dead ? ".dead" : "");
                b.name = nm;
                run_body(&b);
                state_claims(&b);
                /* …and again not taking it, so a diamond is entered from BOTH
                 * sides rather than only the one the first argument picks. */
                if (cut == CUT_BRIF || cut == CUT_IF) {
                    char nm2[52];
                    snprintf(nm2, sizeof nm2, "%s.alt", nm);
                    b.name = nm2; b.args[0].i = 0;
                    run_body(&b);
                    state_claims(&b);
                }
            }

    /* PIN E-0. At n=0 every state is 0 and the four claims above are true of a
     * machine with no cache — green, and about nothing. So what the walk did is
     * gated, not merely printed: at n>0 these fixtures must have run
     * instructions with a value in a register AND stamped transitions between
     * them, or the suite is reporting a mechanism it never engaged. */
    const jav_ttree_stats_t* s = jav_ttree_stats();
    printf("  exercised: %llu cached state(s) (%llu deeper than slot 0), "
           "%llu transition(s), %llu unbridged\n",
           (unsigned long long)s->states_cached,
           (unsigned long long)s->states_deep,
           (unsigned long long)s->transitions,
           (unsigned long long)s->bridge_fails);
    if (s->have_unbridged)
        printf("  first unbridged: op 0x%02x @%u, state %d -> %d, class %d\n",
               s->first_unbridged_op, s->first_unbridged_off,
               s->first_unbridged_from, s->first_unbridged_to,
               s->first_unbridged_cls);
    CHECK(s->bridge_fails == 0, "E-2: %llu gap(s) the stitcher could not bridge",
          (unsigned long long)s->bridge_fails);
    if (JAV_TIER2_N > 0)
        CHECK(s->states_cached > 0, "E-0: no instruction ran with a cached operand");
    /* `transitions > 0` was here too, as a second vacuity check, and it was
     * WRONG: it assumed a working cache must spill and fill. It must not. A
     * transition is the mechanism failing to be invisible — the access it would
     * have done inline, plus a jump — so zero of them over a fixture set is the
     * best possible result, not an unexercised one. D7s made that reachable by
     * giving a cached state a memory-result form, and the check started failing
     * on success. `states_cached` alone says the cache was used. */
    /* PIN C-5 — DeeperSlotIsReachable. A value reaches slot k+1 by STAYING in the
     * cache while something is pushed above it — the shift the consuming variant
     * already performs, which is free because that stencil runs anyway. Without a
     * rule saying so the only edge into a deeper slot is a fill from memory, the
     * DP's sole route to state 2 is to spill a value and refill it one slot down,
     * and it declines that round trip: every slot above the first goes dead while
     * its variants are still generated. */
    if (JAV_TIER2_N >= 2)
        CHECK(s->states_deep > 0,
              "C-5: nothing ran above slot 0 across any fixture, so every slot "
              "above the first is dead");

    printf("\ntier-2 stitcher: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
