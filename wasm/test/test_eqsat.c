/* test_eqsat.c — PIN B-2: InternExtractIsIdentity.
 *
 * Hand-encoded bodies through the REAL pass (jav_eqsat_body over the tree
 * jav_ttree_build built): with zero rules, every root's extraction must spell
 * its original subtree node-for-node — versioned locals and opaque leaves
 * included — which the pass itself verifies (same_term) and reports through
 * its counters. The pin reads those counters as EXACT deltas per body, so a
 * body that quietly interned fewer regions or roots than its shape demands is
 * a red, not a smaller green.
 *
 * The corpus gate (test_wast --tier=3) holds the same identity over 10,606
 * bodies; this suite holds it over bodies whose SHAPES are chosen — a pure
 * tree, same-version local reuse (hashcons must merge, extraction must still
 * match), a version bump through local.set and a tee mid-tree, an opaque
 * consumer, and a multi-region body whose carried leaf round-trips by
 * identity.
 */
#include <stdio.h>
#include <string.h>

#include "validate.h"
#include "jav_ttree.h"
#include "jav_eqsat.h"
#include "bbq_arena.h"
#include "runtime_api.h"
#include "interp.h"
#include "jit_driver.h"

static int checks, failures;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    failures++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct {
    const char*    name;
    const uint8_t* code; size_t len;
    jav_valtype_t  locals[2]; uint32_t nlocals;
    jav_valtype_t  result;
    /* the shape's own counts: how many regions and roots the pass must see */
    uint64_t       want_regions, want_roots;
} body_t;

/* ── PIN C-1: one fixture per axiom family ──────────────────
 *
 * Each is input bytes plus what the RULES must do to them: `want_rewritten`
 * roots change (RED until the family's rule lands — with zero rules the
 * counter stays 0), and the stamped code still answers what the interpreter
 * answers (the net for an UNSOUND rule, which is the failure a fixture like
 * this exists to catch — WASM §4.3.2's equalities hold at every value,
 * overflow included, so a differing answer is a wrong rule, never a wrong
 * input). Locals are ARGUMENTS here (nparams = nlocals) so a rule cannot
 * fold what the fixture wants opaque. */
typedef struct {
    const char*    name;
    const uint8_t* code; size_t len;
    uint32_t       nlocals;         /* i32 params, argument values below */
    int32_t        arg0;
    uint64_t       want_rewritten;  /* roots the family's rule must change */
    int32_t        want;            /* the §4.3.2 answer, stated in the fixture */
} rule_fix_t;

static void run_rule_fix(const rule_fix_t* f) {
    jav_valtype_t locals[1] = { WVT_I32 };
    jav_valtype_t res[1] = { WVT_I32 };
    jav_vctx_t c = {0};
    c.locals = locals; c.nlocals = f->nlocals; c.nparams = f->nlocals;
    c.results = res; c.nresults = 1;
    jav_st_entry_t* st = NULL; unsigned nst = 0;
    if (!jav_typecheck(f->code, f->len, &c, &st, &nst)) {
        failures++; checks++;
        printf("  FAIL %s: fixture does not typecheck\n", f->name);
        return;
    }

    uint8_t lcls[1] = { JSC_I32 }, rcls[1] = { JSC_I32 };
    jav_tctx_t tcx = {0};
    tcx.local_class = lcls; tcx.nlocals = f->nlocals;
    tcx.result_class = rcls; tcx.nresults = 1;
    tcx.tier = 3;

    /* The interpreter is the oracle (it never sees the rewrite). */
    static vm_t vm;
    jav_vm_free(&vm);
    memset(&vm, 0, sizeof vm);
    if (jav_vm_init(&vm) != 0) { fprintf(stderr, "vm pool failed\n"); exit(2); }
    bbq_ctx_init(&vm.frame.code, f->code, f->len);
    vm.frame.sidetable = st;
    vm.frame.locals[0].i = f->arg0; vm.frame.local_types[0] = T_INT;
    jav_status_t swant = interp_run(&vm, NULL);
    slot_t want = jav_tos(&vm);
    CHECK(swant == JAV_RETURN && want.i == f->want,
          "%s: the fixture's own answer is wrong (interp %d, stated %d)",
          f->name, (int)want.i, (int)f->want);

    const jav_eqsat_stats_t* es = jav_eqsat_stats();
    uint64_t rw0 = es->rewritten, if0 = es->identity_fails;
    bbq_ctx_t cc; bbq_ctx_init(&cc, f->code, f->len);
    jit_func_t* h = jit_compile(cc, &tcx);
    CHECK(h != NULL, "%s: tier-3 declined the body", f->name);
    CHECK(es->rewritten - rw0 == f->want_rewritten,
          "%s: %llu root(s) rewritten, the family's rule must change %llu",
          f->name, (unsigned long long)(es->rewritten - rw0),
          (unsigned long long)f->want_rewritten);
    CHECK(es->identity_fails == if0, "%s: the pass disagreed with itself", f->name);
    if (h) {
        jav_vm_free(&vm);
        memset(&vm, 0, sizeof vm);
        if (jav_vm_init(&vm) != 0) { fprintf(stderr, "vm pool failed\n"); exit(2); }
        bbq_ctx_init(&vm.frame.code, f->code, f->len);
        vm.frame.sidetable = st;
        vm.frame.locals[0].i = f->arg0; vm.frame.local_types[0] = T_INT;
        jav_status_t sgot = jit_enter(h, &vm);
        slot_t got = jav_tos(&vm);
        CHECK(sgot == swant && got.i == want.i,
              "%s: rewritten code answers %d, the interpreter %d — the rule is "
              "not the §4.3.2 equality it claims", f->name, (int)got.i, (int)want.i);
        jit_free(h);
    }
    jav_vm_free(&vm);
    bbq_vec_free(st);
}

static void run_body(const body_t* b) {
    /* The fixture must be a valid body — an invalid one would prove nothing
     * about the pass and everything about the author. */
    jav_valtype_t res[1] = { b->result };
    jav_vctx_t c = {0};
    c.locals = b->locals; c.nlocals = b->nlocals;
    c.results = res; c.nresults = 1;
    jav_st_entry_t* st = NULL; unsigned nst = 0;
    if (!jav_typecheck(b->code, b->len, &c, &st, &nst)) {
        failures++; checks++;
        printf("  FAIL %s: fixture does not typecheck\n", b->name);
        return;
    }
    bbq_vec_free(st);

    uint8_t lcls[2], rcls[1];
    for (uint32_t i = 0; i < b->nlocals; i++) lcls[i] = jav_sclass_of_valtype(b->locals[i]);
    rcls[0] = jav_sclass_of_valtype(b->result);
    jav_tctx_t tcx = {0};
    tcx.local_class = lcls; tcx.nlocals = b->nlocals;
    tcx.result_class = rcls; tcx.nresults = 1;
    tcx.tier = 3;

    bbq_arena a; bbq_arena_init(&a, 16 * 1024);
    jav_ttree_t tree;
    bbq_ctx_t cc; bbq_ctx_init(&cc, b->code, b->len);
    if (!jav_ttree_build(cc, &tcx, &a, &tree)) {
        failures++; checks++;
        printf("  FAIL %s: the tree builder declined the fixture\n", b->name);
        bbq_arena_free(&a);
        return;
    }

    const jav_eqsat_stats_t* es = jav_eqsat_stats();
    jav_eqsat_stats_t s0 = *es;
    bbq_hmap synth; bbq_hmap_init(&synth, 0);
    jav_eqsat_body(&tree, &tcx, &a, &synth);
    bbq_hmap_free(&synth);
    CHECK(es->bodies == s0.bodies + 1, "%s: the pass did not run", b->name);
    CHECK(es->regions - s0.regions == b->want_regions,
          "%s: %llu region(s) interned, the shape has %llu", b->name,
          (unsigned long long)(es->regions - s0.regions),
          (unsigned long long)b->want_regions);
    CHECK(es->roots - s0.roots == b->want_roots,
          "%s: %llu root(s) extracted, the shape has %llu", b->name,
          (unsigned long long)(es->roots - s0.roots),
          (unsigned long long)b->want_roots);
    CHECK(es->rewritten == s0.rewritten,
          "%s: an extraction DIFFERED — no fixture in this suite matches a rule",
          b->name);
    CHECK(es->identity_fails == s0.identity_fails,
          "%s: the pass disagreed with itself", b->name);
    CHECK(es->cap_refusals == s0.cap_refusals,
          "%s: a cap bound on a hand-sized body", b->name);
    bbq_arena_free(&a);
}

#define END 0x0b

int main(void) {
    printf("InternExtractIsIdentity: extraction == the built tree wherever no rule fires\n");

    /* A pure tree NO RULE matches: (x + 3) * x — no zero/one/self operand,
     * nothing const-foldable. (The (1+2)*3 body this held before Part C now
     * belongs to the fold fixtures below, where its rewrite is the claim.) */
    static const uint8_t pure_tree[] = { 0x20,0x00, 0x41,0x03, 0x6a,
                                         0x20,0x00, 0x6c, END };
    /* Same-version reuse: local.get 0 twice under one add. Hashcons interns
     * ONE leaf for both (same slot, same version) and extraction must still
     * spell the original two-child tree. */
    static const uint8_t same_ver[]  = { 0x20,0x00, 0x20,0x00, 0x6a, END };
    /* The version thread: a set bumps, a tee mid-tree bumps again, and every
     * get interns against the version current at ITS seq position. Roots:
     * the set (opaque stmt) and the final add. */
    static const uint8_t versions[]  = { 0x41,0x05, 0x21,0x00,          /* set: v0 -> v1 */
                                         0x20,0x00,                     /* get @v1 */
                                         0x41,0x07, 0x22,0x00,          /* tee: v1 -> v2 */
                                         0x6a, END };
    /* An opaque consumer: drop is not in the fence, so it interns by
     * identity with its pure subtree still graphed. Two roots: the drop and
     * the result const. */
    static const uint8_t opaque[]    = { 0x41,0x01, 0x41,0x02, 0x6a, 0x1a,
                                         0x41,0x09, END };
    /* Two regions: a block whose br cuts, and the fall-through result whose
     * operand crosses the cut as a CARRIED leaf (opaque by identity). */
    static const uint8_t regions[]   = { 0x41,0x2a,                     /* i32.const 42 */
                                         0x02,0x40, 0x0c,0x00, END,    /* block br 0 end */
                                         0x41,0x01, 0x6a, END };

    /* Region/root counts are the builder's cut structure: every control
     * marker — the function's closing `end` included — is a cut, so the END
     * that consumes the result is a root in its own region and every body
     * carries one region/root beyond its value trees. `regions` cuts at
     * block, br, the block's end and the function's end: five regions, five
     * roots (const42 · block-entry · br · the add · END), the const's value
     * crossing into the add's region as a carried leaf. */
    const body_t bodies[] = {
        { "pure_tree", pure_tree, sizeof pure_tree, {WVT_I32}, 1, WVT_I32, 2, 2 },
        { "same_ver",  same_ver,  sizeof same_ver,  {WVT_I32}, 1, WVT_I32, 2, 2 },
        { "versions",  versions,  sizeof versions,  {WVT_I32}, 1, WVT_I32, 2, 3 },
        { "opaque",    opaque,    sizeof opaque,    {0}, 0, WVT_I32, 2, 3 },
        { "regions",   regions,   sizeof regions,   {0}, 0, WVT_I32, 5, 5 },
    };
    for (size_t i = 0; i < sizeof bodies / sizeof bodies[0]; i++)
        run_body(&bodies[i]);

    /* ── PIN C-1, family by family. RED until each family's rule lands.
     * `strength ×2^k → <<k` is EXCLUDED from v1, with the reason in the
     * plan: under D4's size-first cost the two forms are byte-equal, so the
     * rule can never change an extraction and no fixture can go red for it. */
    printf("AxiomFamilies: each fixture red until its rule lands\n");
    static const uint8_t fx_fold[]     = { 0x41,0x02, 0x41,0x03, 0x6a, END };
    static const uint8_t fx_fold64[]   = { 0x42,0x28, 0x42,0x02, 0x7c, 0xa7, END };
    static const uint8_t fx_addz[]     = { 0x20,0x00, 0x41,0x00, 0x6a, END };
    static const uint8_t fx_addz_c[]   = { 0x41,0x00, 0x20,0x00, 0x6a, END };
    static const uint8_t fx_subz[]     = { 0x20,0x00, 0x41,0x00, 0x6b, END };
    static const uint8_t fx_subself[]  = { 0x20,0x00, 0x20,0x00, 0x6b, END };
    static const uint8_t fx_mulone[]   = { 0x20,0x00, 0x41,0x01, 0x6c, END };
    static const uint8_t fx_mulzero[]  = { 0x20,0x00, 0x41,0x00, 0x6c, END };
    static const uint8_t fx_andself[]  = { 0x20,0x00, 0x20,0x00, 0x71, END };
    static const uint8_t fx_andneg1[]  = { 0x20,0x00, 0x41,0x7f, 0x71, END };
    static const uint8_t fx_andzero[]  = { 0x20,0x00, 0x41,0x00, 0x71, END };
    static const uint8_t fx_orself[]   = { 0x20,0x00, 0x20,0x00, 0x72, END };
    static const uint8_t fx_orzero[]   = { 0x20,0x00, 0x41,0x00, 0x72, END };
    static const uint8_t fx_xorself[]  = { 0x20,0x00, 0x20,0x00, 0x73, END };
    static const uint8_t fx_xorzero[]  = { 0x20,0x00, 0x41,0x00, 0x73, END };
    static const uint8_t fx_shlz[]     = { 0x20,0x00, 0x41,0x00, 0x74, END };
    static const uint8_t fx_shrsz[]    = { 0x20,0x00, 0x41,0x00, 0x75, END };
    static const uint8_t fx_shruz[]    = { 0x20,0x00, 0x41,0x00, 0x76, END };
    static const uint8_t fx_eqz[]      = { 0x41,0x00, 0x45, END };
    static const uint8_t fx_reassoc[]  = { 0x20,0x00, 0x41,0x03, 0x6a,
                                           0x41,0x04, 0x6a, END };
    /* i32.const 0 ; block ; br 0 ; end ; local.get 0 ; i32.mul ; end */
    static const uint8_t fx_cross[]    = { 0x41,0x00, 0x02,0x40, 0x0c,0x00, END,
                                           0x20,0x00, 0x6c, END };
    const rule_fix_t fixes[] = {
        /* family: const-fold via the analysis (i32, i64, and wrap refolds) */
        { "fold.i32",      fx_fold,    sizeof fx_fold,    0, 0,  1, 5 },
        { "fold.i64_wrap", fx_fold64,  sizeof fx_fold64,  0, 0,  1, 42 },
        /* family: additive identity — §4.3.2 iadd/isub mod 2^N */
        { "add.zero",      fx_addz,    sizeof fx_addz,    1, 42, 1, 42 },
        { "add.zero_comm", fx_addz_c,  sizeof fx_addz_c,  1, 42, 1, 42 },
        { "sub.zero",      fx_subz,    sizeof fx_subz,    1, 42, 1, 42 },
        { "sub.self",      fx_subself, sizeof fx_subself, 1, 42, 1, 0 },
        /* family: multiplicative identity and absorber */
        { "mul.one",       fx_mulone,  sizeof fx_mulone,  1, 42, 1, 42 },
        { "mul.zero",      fx_mulzero, sizeof fx_mulzero, 1, 42, 1, 0 },
        /* family: bitwise identities, absorbers, self-inverse */
        { "and.self",      fx_andself, sizeof fx_andself, 1, 42, 1, 42 },
        { "and.neg1",      fx_andneg1, sizeof fx_andneg1, 1, 42, 1, 42 },
        { "and.zero",      fx_andzero, sizeof fx_andzero, 1, 42, 1, 0 },
        { "or.self",       fx_orself,  sizeof fx_orself,  1, 42, 1, 42 },
        { "or.zero",       fx_orzero,  sizeof fx_orzero,  1, 42, 1, 42 },
        { "xor.self",      fx_xorself, sizeof fx_xorself, 1, 42, 1, 0 },
        { "xor.zero",      fx_xorzero, sizeof fx_xorzero, 1, 42, 1, 42 },
        /* family: shift-by-zero — §4.3.2 k = i2 mod N, k = 0 moves nothing */
        { "shl.zero",      fx_shlz,    sizeof fx_shlz,    1, 42, 1, 42 },
        { "shr_s.zero",    fx_shrsz,   sizeof fx_shrsz,   1, 42, 1, 42 },
        { "shr_u.zero",    fx_shruz,   sizeof fx_shruz,   1, 42, 1, 42 },
        /* family: test folds — §4.3.2 ieqz = bool(i = 0) */
        { "eqz.fold",      fx_eqz,     sizeof fx_eqz,     0, 0,  1, 1 },
        /* family: reassociate-and-refold — (x+3)+4 ≡ x+(3+4), then the
         * analysis folds 7 and extraction takes the smaller term */
        { "reassoc.refold", fx_reassoc, sizeof fx_reassoc, 1, 35, 1, 42 },
        /* Part F: a constant proven in one region reaches the next. The
         * i32.const 0 crosses the block's cut as a CARRIED leaf; mul_zero in
         * the next region can only fire because the fact crossed with it —
         * and its extraction KEEPS the carried operand (the rule returns $z),
         * which is the one shape the drop fence admits: the local.get is
         * dropped (pure), the owed pop stays owed, and the region's answer
         * is the 0 already on the stack. */
        { "fact.crosses_cut", fx_cross, sizeof fx_cross, 1, 42, 1, 0 },
    };
    for (size_t i = 0; i < sizeof fixes / sizeof fixes[0]; i++)
        run_rule_fix(&fixes[i]);

    printf("eqsat identity: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
