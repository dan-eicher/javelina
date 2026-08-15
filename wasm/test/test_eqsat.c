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
     * nothing const-foldable. (The (1+2)*3 body this once held moved to the
     * fold fixtures below, where its rewrite is the claim.) */
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
    /* C4: the §4.3.4 cancellations and associativity beyond add */
    static const uint8_t fx_wraps[]    = { 0x20,0x00, 0xac, 0xa7, END };
    static const uint8_t fx_wrapu[]    = { 0x20,0x00, 0xad, 0xa7, END };
    static const uint8_t fx_mulas[]    = { 0x20,0x00, 0x41,0x03, 0x6c,
                                           0x41,0x05, 0x6c, END };
    static const uint8_t fx_andas[]    = { 0x20,0x00, 0x41,0x03, 0x71,
                                           0x41,0x05, 0x71, END };
    static const uint8_t fx_oras[]     = { 0x20,0x00, 0x41,0x03, 0x72,
                                           0x41,0x05, 0x72, END };
    static const uint8_t fx_xoras[]    = { 0x20,0x00, 0x41,0x03, 0x73,
                                           0x41,0x05, 0x73, END };
    static const uint8_t fx_as64[]     = { 0x20,0x00, 0xad, 0x42,0x03, 0x7c,
                                           0x42,0x04, 0x7c, 0xa7, END };
    /* C7: comparison folds (analysis) — the sign-split pairs are the ones a
     * wrong reading would flip */
    static const uint8_t fc_eq[]   = { 0x41,0x02, 0x41,0x03, 0x46, END };
    static const uint8_t fc_ne[]   = { 0x41,0x02, 0x41,0x03, 0x47, END };
    static const uint8_t fc_lts[]  = { 0x41,0x7f, 0x41,0x01, 0x48, END };
    static const uint8_t fc_ltu[]  = { 0x41,0x7f, 0x41,0x01, 0x49, END };
    static const uint8_t fc_gts[]  = { 0x41,0x01, 0x41,0x7f, 0x4a, END };
    static const uint8_t fc_gtu[]  = { 0x41,0x01, 0x41,0x7f, 0x4b, END };
    static const uint8_t fc_les[]  = { 0x41,0x7f, 0x41,0x7f, 0x4c, END };
    static const uint8_t fc_leu[]  = { 0x41,0x05, 0x41,0x05, 0x4d, END };
    static const uint8_t fc_ges[]  = { 0x41,0x7e, 0x41,0x7f, 0x4e, END };
    static const uint8_t fc_geu[]  = { 0x41,0x7e, 0x41,0x7f, 0x4f, END };
    static const uint8_t fc_eq64[] = { 0x42,0x05, 0x42,0x05, 0x51, END };
    static const uint8_t fc_lts64[]= { 0x42,0x7f, 0x42,0x01, 0x53, END };
    static const uint8_t fc_ltu64[]= { 0x42,0x7f, 0x42,0x01, 0x54, END };
    static const uint8_t fc_geu64[]= { 0x42,0x00, 0x42,0x7f, 0x5a, END };
    /* C6: the vector families. SPL = splat the i32 argument (any v128 value
     * works for every lane width — the rules key on the OPCODE); XT0 =
     * i32x4.extract_lane 0 collapses the answer to the harness's i32. Subs
     * at 0x80+ are uleb, hence the trailing 0x01 bytes. */
    #define SPL  0x20,0x00, 0xfd,0x11
    #define XT0  0xfd,0x1b,0x00
    #define VZERO 0xfd,0x0c, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    #define VONES 0xfd,0x0c, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,\
                             0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    #define VONE16 0xfd,0x0c, 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0
    #define VONE32 0xfd,0x0c, 1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0
    #define VONE64 0xfd,0x0c, 1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0
    static const uint8_t v_and_self[]  = { SPL, SPL, 0xfd,0x4e, XT0, END };
    static const uint8_t v_and_zero[]  = { SPL, VZERO, 0xfd,0x4e, XT0, END };
    static const uint8_t v_and_ones[]  = { SPL, VONES, 0xfd,0x4e, XT0, END };
    static const uint8_t v_and_zcomm[] = { VZERO, SPL, 0xfd,0x4e, XT0, END };
    static const uint8_t v_or_self[]   = { SPL, SPL, 0xfd,0x50, XT0, END };
    static const uint8_t v_or_zero[]   = { SPL, VZERO, 0xfd,0x50, XT0, END };
    static const uint8_t v_or_ones[]   = { SPL, VONES, 0xfd,0x50, XT0, END };
    static const uint8_t v_or_zcomm[]  = { VZERO, SPL, 0xfd,0x50, XT0, END };
    static const uint8_t v_xor_zero[]  = { SPL, VZERO, 0xfd,0x51, XT0, END };
    static const uint8_t v_xor_ones[]  = { SPL, VONES, 0xfd,0x51, XT0, END };
    static const uint8_t v_xor_zcomm[] = { VZERO, SPL, 0xfd,0x51, XT0, END };
    static const uint8_t v_not_not[]   = { SPL, 0xfd,0x4d, 0xfd,0x4d, XT0, END };
    static const uint8_t v_andnot_z[]  = { SPL, VZERO, 0xfd,0x4f, XT0, END };
    static const uint8_t v_bsel_ones[] = { SPL, VZERO, VONES, 0xfd,0x52, XT0, END };
    static const uint8_t v_bsel_zero[] = { SPL, VZERO, VZERO, 0xfd,0x52, XT0, END };
    static const uint8_t v_add8_z[]    = { SPL, VZERO, 0xfd,0x6e, XT0, END };
    static const uint8_t v_add8_zc[]   = { VZERO, SPL, 0xfd,0x6e, XT0, END };
    static const uint8_t v_add16_z[]   = { SPL, VZERO, 0xfd,0x8e,0x01, XT0, END };
    static const uint8_t v_add16_zc[]  = { VZERO, SPL, 0xfd,0x8e,0x01, XT0, END };
    static const uint8_t v_add32_z[]   = { SPL, VZERO, 0xfd,0xae,0x01, XT0, END };
    static const uint8_t v_add32_zc[]  = { VZERO, SPL, 0xfd,0xae,0x01, XT0, END };
    static const uint8_t v_add64_z[]   = { SPL, VZERO, 0xfd,0xce,0x01, XT0, END };
    static const uint8_t v_add64_zc[]  = { VZERO, SPL, 0xfd,0xce,0x01, XT0, END };
    static const uint8_t v_sub8_z[]    = { SPL, VZERO, 0xfd,0x71, XT0, END };
    static const uint8_t v_sub16_z[]   = { SPL, VZERO, 0xfd,0x91,0x01, XT0, END };
    static const uint8_t v_sub32_z[]   = { SPL, VZERO, 0xfd,0xb1,0x01, XT0, END };
    static const uint8_t v_sub64_z[]   = { SPL, VZERO, 0xfd,0xd1,0x01, XT0, END };
    static const uint8_t v_mul16_o[]   = { SPL, VONE16, 0xfd,0x95,0x01, XT0, END };
    static const uint8_t v_mul16_oc[]  = { VONE16, SPL, 0xfd,0x95,0x01, XT0, END };
    static const uint8_t v_mul16_z[]   = { SPL, VZERO, 0xfd,0x95,0x01, XT0, END };
    static const uint8_t v_mul32_o[]   = { SPL, VONE32, 0xfd,0xb5,0x01, XT0, END };
    static const uint8_t v_mul32_oc[]  = { VONE32, SPL, 0xfd,0xb5,0x01, XT0, END };
    static const uint8_t v_mul32_z[]   = { SPL, VZERO, 0xfd,0xb5,0x01, XT0, END };
    static const uint8_t v_mul64_o[]   = { SPL, VONE64, 0xfd,0xd5,0x01, XT0, END };
    static const uint8_t v_mul64_oc[]  = { VONE64, SPL, 0xfd,0xd5,0x01, XT0, END };
    static const uint8_t v_mul64_z[]   = { SPL, VZERO, 0xfd,0xd5,0x01, XT0, END };
    #define VSHIFT0(sub_bytes...) { SPL, 0x41,0x00, 0xfd,sub_bytes, XT0, END }
    static const uint8_t v_shl8[]   = VSHIFT0(0x6b);
    static const uint8_t v_shrs8[]  = VSHIFT0(0x6c);
    static const uint8_t v_shru8[]  = VSHIFT0(0x6d);
    static const uint8_t v_shl16[]  = VSHIFT0(0x8b,0x01);
    static const uint8_t v_shrs16[] = VSHIFT0(0x8c,0x01);
    static const uint8_t v_shru16[] = VSHIFT0(0x8d,0x01);
    static const uint8_t v_shl32[]  = VSHIFT0(0xab,0x01);
    static const uint8_t v_shrs32[] = VSHIFT0(0xac,0x01);
    static const uint8_t v_shru32[] = VSHIFT0(0xad,0x01);
    static const uint8_t v_shl64[]  = VSHIFT0(0xcb,0x01);
    static const uint8_t v_shrs64[] = VSHIFT0(0xcc,0x01);
    static const uint8_t v_shru64[] = VSHIFT0(0xcd,0x01);
    static const uint8_t v_shuf_a[] = { SPL, VZERO, 0xfd,0x0d,
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, XT0, END };
    static const uint8_t v_shuf_b[] = { SPL, VZERO, 0xfd,0x0d,
        16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31, XT0, END };
    /* strength: ×2^k rewrites to a shift — the multiplier's latency exceeds
     * the shifter's on every shipping core, a stencil-body fact the
     * cycle-cost's micro weights make visible */
    static const uint8_t st_mul32[]  = { 0x20,0x00, 0x41,0x08, 0x6c, END };
    static const uint8_t st_mul64[]  = { 0x20,0x00, 0xad, 0x42,0x08, 0x7e, 0xa7, END };
    #define VSPLAT32_8 0xfd,0x0c, 8,0,0,0, 8,0,0,0, 8,0,0,0, 8,0,0,0
    #define VSPLAT16_8 0xfd,0x0c, 8,0, 8,0, 8,0, 8,0, 8,0, 8,0, 8,0, 8,0
    #define VSPLAT64_8 0xfd,0x0c, 8,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0
    static const uint8_t st_vmul16[] = { SPL, VSPLAT16_8, 0xfd,0x95,0x01, XT0, END };
    static const uint8_t st_vmul32[] = { SPL, VSPLAT32_8, 0xfd,0xb5,0x01, XT0, END };
    static const uint8_t st_vmul64[] = { SPL, VSPLAT64_8, 0xfd,0xd5,0x01, XT0, END };
    /* the self-erasing vector forms, now that a zero vector can be stamped */
    static const uint8_t v_xor_self[] = { SPL, SPL, 0xfd,0x51, XT0, END };
    static const uint8_t v_sub8_s[]   = { SPL, SPL, 0xfd,0x71, XT0, END };
    static const uint8_t v_sub16_s[]  = { SPL, SPL, 0xfd,0x91,0x01, XT0, END };
    static const uint8_t v_sub32_s[]  = { SPL, SPL, 0xfd,0xb1,0x01, XT0, END };
    static const uint8_t v_sub64_s[]  = { SPL, SPL, 0xfd,0xd1,0x01, XT0, END };
    /* scalar commutativity retro-pins: the const on the LEFT, so only the
     * comm rule lets the identity see it */
    static const uint8_t s_mul_oc[]  = { 0x41,0x01, 0x20,0x00, 0x6c, END };
    static const uint8_t s_and_oc[]  = { 0x41,0x7f, 0x20,0x00, 0x71, END };
    static const uint8_t s_or_zc[]   = { 0x41,0x00, 0x20,0x00, 0x72, END };
    static const uint8_t s_xor_zc[]  = { 0x41,0x00, 0x20,0x00, 0x73, END };
    static const uint8_t s_add64_zc[]= { 0x42,0x00, 0x20,0x00, 0xad, 0x7c, 0xa7, END };
    static const uint8_t s_mul64_oc[]= { 0x42,0x01, 0x20,0x00, 0xad, 0x7e, 0xa7, END };
    static const uint8_t s_and64_oc[]= { 0x42,0x7f, 0x20,0x00, 0xad, 0x83, 0xa7, END };
    static const uint8_t s_or64_zc[] = { 0x42,0x00, 0x20,0x00, 0xad, 0x84, 0xa7, END };
    static const uint8_t s_xor64_zc[]= { 0x42,0x00, 0x20,0x00, 0xad, 0x85, 0xa7, END };
    /* C7: self-comparison rules — same local twice (one e-class by hashcons);
     * the i64 rows go through extend_s twice, which interns once. */
    #define SELF32(op) { 0x20,0x00, 0x20,0x00, op, END }
    #define SELF64(op) { 0x20,0x00, 0xac, 0x20,0x00, 0xac, op, END }
    static const uint8_t fs_eq[]   = SELF32(0x46);
    static const uint8_t fs_ne[]   = SELF32(0x47);
    static const uint8_t fs_lts[]  = SELF32(0x48);
    static const uint8_t fs_ltu[]  = SELF32(0x49);
    static const uint8_t fs_gts[]  = SELF32(0x4a);
    static const uint8_t fs_gtu[]  = SELF32(0x4b);
    static const uint8_t fs_les[]  = SELF32(0x4c);
    static const uint8_t fs_leu[]  = SELF32(0x4d);
    static const uint8_t fs_ges[]  = SELF32(0x4e);
    static const uint8_t fs_geu[]  = SELF32(0x4f);
    static const uint8_t fs_eq64[] = SELF64(0x51);
    static const uint8_t fs_ne64[] = SELF64(0x52);
    static const uint8_t fs_lts64[]= SELF64(0x53);
    static const uint8_t fs_ltu64[]= SELF64(0x54);
    static const uint8_t fs_gts64[]= SELF64(0x55);
    static const uint8_t fs_gtu64[]= SELF64(0x56);
    static const uint8_t fs_les64[]= SELF64(0x57);
    static const uint8_t fs_leu64[]= SELF64(0x58);
    static const uint8_t fs_ges64[]= SELF64(0x59);
    static const uint8_t fs_geu64[]= SELF64(0x5a);
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
        /* Cross-region facts: a constant proven in one region reaches the next. The
         * i32.const 0 crosses the block's cut as a CARRIED leaf; mul_zero in
         * the next region can only fire because the fact crossed with it —
         * and its extraction KEEPS the carried operand (the rule returns $z),
         * which is the one shape the drop fence admits: the local.get is
         * dropped (pure), the owed pop stays owed, and the region's answer
         * is the 0 already on the stack. */
        { "fact.crosses_cut", fx_cross, sizeof fx_cross, 1, 42, 1, 0 },
        /* C4: §4.3.4's exact cancellations — wrap∘extend_s and wrap∘extend_u
         * are the identity at every value (extend_u "just reinterprets the
         * same value"; wrap is mod 2^32 and extend_s preserves the low bits),
         * so the pair extracts to the bare operand. The signed one runs a
         * NEGATIVE argument, where a wrong reading of extend_s would show. */
        { "wrap.extend_s", fx_wraps, sizeof fx_wraps, 1, -42, 1, -42 },
        { "wrap.extend_u", fx_wrapu, sizeof fx_wrapu, 1, 42,  1, 42 },
        /* C4: associativity beyond add — (x∘3)∘5 refolds to x∘(3∘5), exact
         * mod 2^N for mul and pointwise for the bit ops. */
        { "assoc.mul",     fx_mulas, sizeof fx_mulas, 1, 2, 1, 30 },
        { "assoc.and",     fx_andas, sizeof fx_andas, 1, 7, 1, 1 },
        { "assoc.or",      fx_oras,  sizeof fx_oras,  1, 8, 1, 15 },
        { "assoc.xor",     fx_xoras, sizeof fx_xoras, 1, 1, 1, 7 },
        /* …and at 64 bits, through extend/wrap so the harness's i32 result
         * carries it: wrap((ext_u(x) + 3) + 4) refolds to wrap(ext_u(x) + 7). */
        { "assoc.i64",     fx_as64,  sizeof fx_as64,  1, 1, 1, 8 },
        /* C7 analysis: relational folds — §4.3.2 pp97/98, sign splits live */
        { "cmp.eq",     fc_eq,    sizeof fc_eq,    0, 0, 1, 0 },
        { "cmp.ne",     fc_ne,    sizeof fc_ne,    0, 0, 1, 1 },
        { "cmp.lt_s",   fc_lts,   sizeof fc_lts,   0, 0, 1, 1 },
        { "cmp.lt_u",   fc_ltu,   sizeof fc_ltu,   0, 0, 1, 0 },
        { "cmp.gt_s",   fc_gts,   sizeof fc_gts,   0, 0, 1, 1 },
        { "cmp.gt_u",   fc_gtu,   sizeof fc_gtu,   0, 0, 1, 0 },
        { "cmp.le_s",   fc_les,   sizeof fc_les,   0, 0, 1, 1 },
        { "cmp.le_u",   fc_leu,   sizeof fc_leu,   0, 0, 1, 1 },
        { "cmp.ge_s",   fc_ges,   sizeof fc_ges,   0, 0, 1, 0 },
        { "cmp.ge_u",   fc_geu,   sizeof fc_geu,   0, 0, 1, 0 },
        { "cmp.eq64",   fc_eq64,  sizeof fc_eq64,  0, 0, 1, 1 },
        { "cmp.lt_s64", fc_lts64, sizeof fc_lts64, 0, 0, 1, 1 },
        { "cmp.lt_u64", fc_ltu64, sizeof fc_ltu64, 0, 0, 1, 0 },
        { "cmp.ge_u64", fc_geu64, sizeof fc_geu64, 0, 0, 1, 0 },
        /* C7 rules: self-comparison — reflexivity is total on integers */
        { "self.eq",     fs_eq,    sizeof fs_eq,    1, 7, 1, 1 },
        { "self.ne",     fs_ne,    sizeof fs_ne,    1, 7, 1, 0 },
        { "self.lt_s",   fs_lts,   sizeof fs_lts,   1, 7, 1, 0 },
        { "self.lt_u",   fs_ltu,   sizeof fs_ltu,   1, 7, 1, 0 },
        { "self.gt_s",   fs_gts,   sizeof fs_gts,   1, 7, 1, 0 },
        { "self.gt_u",   fs_gtu,   sizeof fs_gtu,   1, 7, 1, 0 },
        { "self.le_s",   fs_les,   sizeof fs_les,   1, 7, 1, 1 },
        { "self.le_u",   fs_leu,   sizeof fs_leu,   1, 7, 1, 1 },
        { "self.ge_s",   fs_ges,   sizeof fs_ges,   1, 7, 1, 1 },
        { "self.ge_u",   fs_geu,   sizeof fs_geu,   1, 7, 1, 1 },
        { "self.eq64",   fs_eq64,  sizeof fs_eq64,  1, -3, 1, 1 },
        { "self.ne64",   fs_ne64,  sizeof fs_ne64,  1, -3, 1, 0 },
        { "self.lt_s64", fs_lts64, sizeof fs_lts64, 1, -3, 1, 0 },
        { "self.lt_u64", fs_ltu64, sizeof fs_ltu64, 1, -3, 1, 0 },
        { "self.gt_s64", fs_gts64, sizeof fs_gts64, 1, -3, 1, 0 },
        { "self.gt_u64", fs_gtu64, sizeof fs_gtu64, 1, -3, 1, 0 },
        { "self.le_s64", fs_les64, sizeof fs_les64, 1, -3, 1, 1 },
        { "self.le_u64", fs_leu64, sizeof fs_leu64, 1, -3, 1, 1 },
        { "self.ge_s64", fs_ges64, sizeof fs_ges64, 1, -3, 1, 1 },
        { "self.ge_u64", fs_geu64, sizeof fs_geu64, 1, -3, 1, 1 },
        /* C6: the vector families (arg 5 splatted; extract lane 0 answers) */
        { "v.and_self",  v_and_self, sizeof v_and_self, 1, 5, 1, 5 },
        { "v.and_zero",  v_and_zero, sizeof v_and_zero, 1, 5, 1, 0 },
        { "v.and_ones",  v_and_ones, sizeof v_and_ones, 1, 5, 1, 5 },
        { "v.and_zcomm", v_and_zcomm, sizeof v_and_zcomm, 1, 5, 1, 0 },
        { "v.or_self",   v_or_self,  sizeof v_or_self,  1, 5, 1, 5 },
        { "v.or_zero",   v_or_zero,  sizeof v_or_zero,  1, 5, 1, 5 },
        { "v.or_ones",   v_or_ones,  sizeof v_or_ones,  1, 5, 1, -1 },
        { "v.or_zcomm",  v_or_zcomm, sizeof v_or_zcomm, 1, 5, 1, 5 },
        { "v.xor_zero",  v_xor_zero, sizeof v_xor_zero, 1, 5, 1, 5 },
        { "v.xor_ones",  v_xor_ones, sizeof v_xor_ones, 1, 5, 1, -6 },
        { "v.xor_zcomm", v_xor_zcomm, sizeof v_xor_zcomm, 1, 5, 1, 5 },
        { "v.not_not",   v_not_not,  sizeof v_not_not,  1, 5, 1, 5 },
        { "v.andnot_z",  v_andnot_z, sizeof v_andnot_z, 1, 5, 1, 5 },
        { "v.bsel_ones", v_bsel_ones, sizeof v_bsel_ones, 1, 5, 1, 5 },
        { "v.bsel_zero", v_bsel_zero, sizeof v_bsel_zero, 1, 5, 1, 0 },
        { "v.add8_z",    v_add8_z,   sizeof v_add8_z,   1, 5, 1, 5 },
        { "v.add8_zc",   v_add8_zc,  sizeof v_add8_zc,  1, 5, 1, 5 },
        { "v.add16_z",   v_add16_z,  sizeof v_add16_z,  1, 5, 1, 5 },
        { "v.add16_zc",  v_add16_zc, sizeof v_add16_zc, 1, 5, 1, 5 },
        { "v.add32_z",   v_add32_z,  sizeof v_add32_z,  1, 5, 1, 5 },
        { "v.add32_zc",  v_add32_zc, sizeof v_add32_zc, 1, 5, 1, 5 },
        { "v.add64_z",   v_add64_z,  sizeof v_add64_z,  1, 5, 1, 5 },
        { "v.add64_zc",  v_add64_zc, sizeof v_add64_zc, 1, 5, 1, 5 },
        { "v.sub8_z",    v_sub8_z,   sizeof v_sub8_z,   1, 5, 1, 5 },
        { "v.sub16_z",   v_sub16_z,  sizeof v_sub16_z,  1, 5, 1, 5 },
        { "v.sub32_z",   v_sub32_z,  sizeof v_sub32_z,  1, 5, 1, 5 },
        { "v.sub64_z",   v_sub64_z,  sizeof v_sub64_z,  1, 5, 1, 5 },
        { "v.mul16_one", v_mul16_o,  sizeof v_mul16_o,  1, 5, 1, 5 },
        { "v.mul16_oc",  v_mul16_oc, sizeof v_mul16_oc, 1, 5, 1, 5 },
        { "v.mul16_z",   v_mul16_z,  sizeof v_mul16_z,  1, 5, 1, 0 },
        { "v.mul32_one", v_mul32_o,  sizeof v_mul32_o,  1, 5, 1, 5 },
        { "v.mul32_oc",  v_mul32_oc, sizeof v_mul32_oc, 1, 5, 1, 5 },
        { "v.mul32_z",   v_mul32_z,  sizeof v_mul32_z,  1, 5, 1, 0 },
        { "v.mul64_one", v_mul64_o,  sizeof v_mul64_o,  1, 5, 1, 5 },
        { "v.mul64_oc",  v_mul64_oc, sizeof v_mul64_oc, 1, 5, 1, 5 },
        { "v.mul64_z",   v_mul64_z,  sizeof v_mul64_z,  1, 5, 1, 0 },
        { "v.shl8_0",    v_shl8,   sizeof v_shl8,   1, 5, 1, 5 },
        { "v.shrs8_0",   v_shrs8,  sizeof v_shrs8,  1, 5, 1, 5 },
        { "v.shru8_0",   v_shru8,  sizeof v_shru8,  1, 5, 1, 5 },
        { "v.shl16_0",   v_shl16,  sizeof v_shl16,  1, 5, 1, 5 },
        { "v.shrs16_0",  v_shrs16, sizeof v_shrs16, 1, 5, 1, 5 },
        { "v.shru16_0",  v_shru16, sizeof v_shru16, 1, 5, 1, 5 },
        { "v.shl32_0",   v_shl32,  sizeof v_shl32,  1, 5, 1, 5 },
        { "v.shrs32_0",  v_shrs32, sizeof v_shrs32, 1, 5, 1, 5 },
        { "v.shru32_0",  v_shru32, sizeof v_shru32, 1, 5, 1, 5 },
        { "v.shl64_0",   v_shl64,  sizeof v_shl64,  1, 5, 1, 5 },
        { "v.shrs64_0",  v_shrs64, sizeof v_shrs64, 1, 5, 1, 5 },
        { "v.shru64_0",  v_shru64, sizeof v_shru64, 1, 5, 1, 5 },
        { "v.shuffle_a", v_shuf_a, sizeof v_shuf_a, 1, 5, 1, 5 },
        { "v.shuffle_b", v_shuf_b, sizeof v_shuf_b, 1, 5, 1, 0 },
        /* strength ×2^k → shift (arg 5; ×8 = 40) */
        { "strength.mul32",  st_mul32,  sizeof st_mul32,  1, 5, 1, 40 },
        { "strength.mul64",  st_mul64,  sizeof st_mul64,  1, 5, 1, 40 },
        { "strength.vmul16", st_vmul16, sizeof st_vmul16, 1, 5, 1, 40 },
        { "strength.vmul32", st_vmul32, sizeof st_vmul32, 1, 5, 1, 40 },
        { "strength.vmul64", st_vmul64, sizeof st_vmul64, 1, 5, 1, 40 },
        /* vector self-erasers through the two-immediate synth channel */
        { "v.xor_self",  v_xor_self, sizeof v_xor_self, 1, 5, 1, 0 },
        { "v.sub8_self",  v_sub8_s,  sizeof v_sub8_s,  1, 5, 1, 0 },
        { "v.sub16_self", v_sub16_s, sizeof v_sub16_s, 1, 5, 1, 0 },
        { "v.sub32_self", v_sub32_s, sizeof v_sub32_s, 1, 5, 1, 0 },
        { "v.sub64_self", v_sub64_s, sizeof v_sub64_s, 1, 5, 1, 0 },
        /* scalar comm retro-pins (const on the LEFT) */
        { "comm.mul_one",  s_mul_oc,  sizeof s_mul_oc,  1, 7, 1, 7 },
        { "comm.and_ones", s_and_oc,  sizeof s_and_oc,  1, 7, 1, 7 },
        { "comm.or_zero",  s_or_zc,   sizeof s_or_zc,   1, 7, 1, 7 },
        { "comm.xor_zero", s_xor_zc,  sizeof s_xor_zc,  1, 7, 1, 7 },
        { "comm.add64",    s_add64_zc, sizeof s_add64_zc, 1, 7, 1, 7 },
        { "comm.mul64",    s_mul64_oc, sizeof s_mul64_oc, 1, 7, 1, 7 },
        { "comm.and64",    s_and64_oc, sizeof s_and64_oc, 1, 7, 1, 7 },
        { "comm.or64",     s_or64_zc,  sizeof s_or64_zc,  1, 7, 1, 7 },
        { "comm.xor64",    s_xor64_zc, sizeof s_xor64_zc, 1, 7, 1, 7 },
    };
    for (size_t i = 0; i < sizeof fixes / sizeof fixes[0]; i++)
        run_rule_fix(&fixes[i]);

    printf("eqsat identity: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
