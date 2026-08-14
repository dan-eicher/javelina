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
    jav_eqsat_body(&tree, &tcx, &a);
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
          "%s: an extraction DIFFERED from its original at zero rules", b->name);
    CHECK(es->identity_fails == s0.identity_fails,
          "%s: the pass disagreed with itself", b->name);
    CHECK(es->cap_refusals == s0.cap_refusals,
          "%s: a cap bound on a hand-sized body", b->name);
    bbq_arena_free(&a);
}

#define END 0x0b

int main(void) {
    printf("InternExtractIsIdentity: zero rules, extraction == the built tree\n");

    /* A pure tree: (1 + 2) * 3. One region (the body), one root. */
    static const uint8_t pure_tree[] = { 0x41,0x01, 0x41,0x02, 0x6a,
                                         0x41,0x03, 0x6c, END };
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
        { "pure_tree", pure_tree, sizeof pure_tree, {0}, 0, WVT_I32, 2, 2 },
        { "same_ver",  same_ver,  sizeof same_ver,  {WVT_I32}, 1, WVT_I32, 2, 2 },
        { "versions",  versions,  sizeof versions,  {WVT_I32}, 1, WVT_I32, 2, 3 },
        { "opaque",    opaque,    sizeof opaque,    {0}, 0, WVT_I32, 2, 3 },
        { "regions",   regions,   sizeof regions,   {0}, 0, WVT_I32, 5, 5 },
    };
    for (size_t i = 0; i < sizeof bodies / sizeof bodies[0]; i++)
        run_body(&bodies[i]);

    printf("eqsat identity: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
