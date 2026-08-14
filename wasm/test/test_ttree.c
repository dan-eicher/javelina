// test_ttree.c — the tier-2 tree builder.
//
// Each case is a body's bytes and the exact tree it must yield. The signature
// ids are looked up BY SHAPE (find_sig below) rather than written as numbers:
// an id is an index into a generated table and moves whenever the vocabulary
// does, but "the root is the (i32)->() signature" is the claim being made.
#include "jav_ttree.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
static void CK(const char* msg, long got, long want) {
    int ok = (got == want);
    printf("  %-56s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

/* The final signature with exactly these classes, or -1. */
static int find_sig(const uint8_t* p, int np, const uint8_t* r, int nr) {
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        const jav_sig_t* s = &jav_sigtab[i];
        if (!s->final || s->nparams != np || s->nresults != nr) continue;
        if (jav_sigtab[i].name[0] != 'S') continue;   /* not a carried leaf */
        int hit = 1;
        for (int k = 0; k < np && hit; k++) hit = (s->params[k] == p[k]);
        for (int k = 0; k < nr && hit; k++) hit = (s->results[k] == r[k]);
        if (hit) return i;
    }
    return -1;
}
#define SIG(pv, rv) find_sig(pv, (int)(sizeof pv), rv, (int)(sizeof rv))
#define SIG_IN(pv)  find_sig(pv, (int)(sizeof pv), NULL, 0)
#define SIG_OUT(rv) find_sig(NULL, 0, rv, (int)(sizeof rv))

/* Every case below is a WHOLE body, final `end` included. `jav_ttree_build` sets up
 * frame 0 as the function's own — "the body's final `end` closes it" — and the VM
 * hands it `f->code, f->code_len`, which is exactly one code entry. A fragment is an
 * input it can never receive, and one that cannot exercise the frame-0 close at all.
 * The `end` is an opcode like any other, so it builds a node and joins the last
 * region's roots; where a count below moved because of it, the comment says so. */

/* One i32 local, no results — the shape every case below needs. */
static const uint8_t L_I32[1] = { JSC_I32 };
static jav_tctx_t ctx_i32_local(void) {
    jav_tctx_t c;
    memset(&c, 0, sizeof c);
    c.local_class = L_I32; c.nlocals = 1;
    return c;
}

static int build(const uint8_t* code, size_t n, const jav_tctx_t* c,
                 bbq_arena* a, jav_ttree_t* t) {
    bbq_ctx_t cur;
    bbq_ctx_init(&cur, code, n);
    memset(t, 0, sizeof *t);
    return jav_ttree_build(cur, c, a, t);
}

/* PIN B-1 — AddFoldsToOneTree */
static void add_folds_to_one_tree(void) {
    printf("AddFoldsToOneTree: local.get 0; i32.const 1; i32.add; local.set 0\n");
    static const uint8_t code[] = { 0x20,0x00, 0x41,0x01, 0x6a, 0x21,0x00, 0x0b };
    static const uint8_t II[2] = { JSC_I32, JSC_I32 }, I[1] = { JSC_I32 };
    jav_tctx_t c = ctx_i32_local();
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;
    CK("builds", build(code, sizeof code, &c, &a, &t), 1);
    CK("regions", t.nregions, 1);
    if (t.nregions == 1) {
        /* Two: the local.set tree, then the body's `end` — which computes nothing
         * and is consumed by nothing, so it is a root of its own. */
        CK("roots in region 0", t.regions[0].nroots, 2);
        if (t.regions[0].nroots == 2) {
            CK("the second root is the `end`", *t.regions[0].roots[1]->pc, 0x0b);
            const jav_tnode_t* root = t.regions[0].roots[0];
            CK("root sig is (i32)->()", root->sig, SIG_IN(I));
            CK("root nkids", root->nkids, 1);
            const jav_tnode_t* add = root->kids[0];
            CK("kids[0] is (i32 i32)->(i32)", add->sig, find_sig(II, 2, I, 1));
            CK("the add's nkids", add->nkids, 2);
            CK("its kids[0] is ()->(i32)", add->kids[0]->sig, SIG_OUT(I));
            CK("its kids[1] is ()->(i32)", add->kids[1]->sig, SIG_OUT(I));
            /* local.get and i32.const share a signature; the payload separates
             * them, which is the whole reason `pc` is on the node. */
            CK("the two leaves are different instructions",
               add->kids[0]->pc != add->kids[1]->pc, 1);
            CK("kids[0] is the local.get", *add->kids[0]->pc, 0x20);
            CK("kids[1] is the i32.const", *add->kids[1]->pc, 0x41);
        }
    }
    bbq_arena_free(&a);
}

/* PIN B-2 — LiveAcrossCutBecomesRoot */
static void live_across_cut_becomes_root(void) {
    printf("LiveAcrossCutBecomesRoot: i32.const 1; block; nop; end; drop\n");
    static const uint8_t code[] = { 0x41,0x01, 0x02,0x40, 0x01, 0x0b, 0x1a, 0x0b };
    jav_tctx_t c = ctx_i32_local();
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;
    CK("builds", build(code, sizeof code, &c, &a, &t), 1);
    /* The i32.const is a root of region 0 — nothing inside region 0 consumed it. */
    int found = 0;
    if (t.nregions) {
        for (uint32_t i = 0; i < t.regions[0].nroots; i++)
            if (*t.regions[0].roots[i]->pc == 0x41) found = 1;
    }
    CK("i32.const is a root of region 0", found, 1);
    /* ...and NOT a child of anything in a later region: the drop takes the leaf
     * that stands for the memory slot, which carries no instruction at all. */
    int drop_child_is_const = 0, drop_seen = 0;
    for (uint32_t r = 1; r < t.nregions; r++)
        for (uint32_t i = 0; i < t.regions[r].nroots; i++) {
            const jav_tnode_t* n = t.regions[r].roots[i];
            if (*n->pc != 0x1a) continue;
            drop_seen = 1;
            if (n->nkids == 1 && n->kids[0]->pc != NULL) drop_child_is_const = 1;
        }
    CK("the drop is in a later region", drop_seen, 1);
    CK("its child is a carried leaf, not the const", drop_child_is_const, 0);
    bbq_arena_free(&a);
}

/* PIN B-3 — DeadCodeBuildsNothing */
static void dead_code_builds_nothing(void) {
    printf("DeadCodeBuildsNothing: unreachable; i32.const 1; i32.add\n");
    static const uint8_t code[] = { 0x00, 0x41,0x01, 0x6a, 0x0b };
    jav_tctx_t c = ctx_i32_local();
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;
    CK("builds", build(code, sizeof code, &c, &a, &t), 1);
    /* The claim is about ROOTS, not regions. `unreachable` is a cut, so it closes
     * region 0, and the body's `end` — met with the frame unreachable, so building
     * nothing — closes a second one with nothing in it. Every body whose last
     * instruction is a terminator has that trailing empty region; only a fixture
     * missing its `end` ever saw one region here. */
    long roots = 0; const jav_tnode_t* first = NULL;
    for (uint32_t r = 0; r < t.nregions; r++)
        for (uint32_t i = 0; i < t.regions[r].nroots; i++, roots++)
            if (!first) first = t.regions[r].roots[i];
    CK("roots over the whole body", roots, 1);
    CK("and it is the unreachable", first ? *first->pc : -1, 0x00);
    bbq_arena_free(&a);
}

/* The resolution the whole vocabulary turns on: the same opcode takes a
 * different terminal because the local it names has a different type. */
static void poly_resolves_from_the_module(void) {
    printf("PolyResolvesFromTheModule: local.get 0 with an f64 local\n");
    static const uint8_t code[] = { 0x20,0x00, 0x1a, 0x0b };   /* local.get 0; drop */
    static const uint8_t F[1] = { JSC_F64 }, I[1] = { JSC_I32 };
    static const uint8_t LF[1] = { JSC_F64 };
    jav_tctx_t c; memset(&c, 0, sizeof c);
    c.local_class = LF; c.nlocals = 1;
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;
    CK("builds", build(code, sizeof code, &c, &a, &t), 1);
    /* By opcode, not by position: the last root is now the body's `end`. */
    const jav_tnode_t* drop = NULL;
    for (uint32_t i = 0; t.nregions && i < t.regions[0].nroots; i++)
        if (*t.regions[0].roots[i]->pc == 0x1a) drop = t.regions[0].roots[i];
    CK("the drop is (f64)->()", drop ? drop->sig : -1, SIG_IN(F));
    CK("and NOT (i32)->()", drop && drop->sig == (uint16_t)SIG_IN(I), 0);
    bbq_arena_free(&a);
}

/* PIN B-4's invariant, stated where it is cheap to check: a node's kid count is
 * its signature's declared arity. The corpus sweep runs the same check inside
 * the builder over every body it sees. */
static void arity_matches_the_signature(void) {
    printf("TreeArityMatchesOpsig: over the cases above\n");
    static const uint8_t code[] = { 0x20,0x00, 0x41,0x01, 0x6a, 0x21,0x00,
                                    0x02,0x40, 0x01, 0x0b, 0x41,0x02, 0x1a, 0x0b };
    jav_tctx_t c = ctx_i32_local();
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;
    CK("builds", build(code, sizeof code, &c, &a, &t), 1);
    long bad = 0, seen = 0;
    for (uint32_t r = 0; r < t.nregions; r++)
        for (uint32_t i = 0; i < t.regions[r].nroots; i++) {
            const jav_tnode_t* stack[64]; int sp = 0;
            stack[sp++] = t.regions[r].roots[i];
            while (sp) {
                const jav_tnode_t* n = stack[--sp];
                seen++;
                if (n->nkids != jav_sigtab[n->sig].nkids) bad++;
                for (int k = 0; k < n->nkids && sp < 64; k++) stack[sp++] = n->kids[k];
            }
        }
    CK("nodes walked", seen > 0, 1);
    CK("nodes whose nkids is not their signature's arity", bad, 0);
    bbq_arena_free(&a);
}

/* PIN B-5 — WalkMustConsumeTheWholeBody
 *
 * The builder decodes immediates with its OWN table (`jav_jit_meta`, from wasm.def),
 * so a table that is wrong about one opcode desynchronises the cursor and every byte
 * after it is read as an opcode. Being handed a validated module does not make the
 * table right, and the corpus cannot say so either: 0 declines is equally what a
 * correct table and a silently-truncating one produce.
 *
 * Both ways it goes wrong, as bodies the walk must REFUSE:
 *
 *   (a) it ends early — a 0x0B reached with bytes left over, which is what a `0x0B`
 *       sitting inside an immediate looks like from here. The tree would cover half
 *       a function and nothing downstream could tell.
 *   (b) it runs dry — the bytes end without frame 0's `end`, which is what an
 *       immediate read as too WIDE looks like: the real `end` was stepped over.
 *
 * A decline, not a crash: tier-2 is an optimization over tier-1, and a body it will
 * not take keeps the tier below (D8). What must not happen is a `1` return.
 */
static void walk_must_consume_the_whole_body(void) {
    printf("WalkMustConsumeTheWholeBody: a partial decode is a decline\n");
    jav_tctx_t c = ctx_i32_local();
    bbq_arena a; bbq_arena_init(&a, 4096);
    jav_ttree_t t;

    /* whole: local.get 0; drop; end — the shape both cases below deviate from. */
    static const uint8_t whole[] = { 0x20,0x00, 0x1a, 0x0b };
    CK("the whole body builds", build(whole, sizeof whole, &c, &a, &t), 1);

    /* (a) the same bytes with a second instruction AFTER the end. The walk closes
     * frame 0 at index 3 and leaves 2 bytes unread. */
    static const uint8_t early[] = { 0x20,0x00, 0x1a, 0x0b, 0x41,0x01 };
    CK("ending before the last byte declines", build(early, sizeof early, &c, &a, &t), 0);

    /* (b) the same bytes with the end removed. Frames balance — there are no nested
     * blocks to leave open — so the pre-existing nframes check says nothing. */
    static const uint8_t dry[] = { 0x20,0x00, 0x1a };
    CK("running dry before the end declines", build(dry, sizeof dry, &c, &a, &t), 0);

    /* ...and the case the pre-existing nframes check already covered still declines
     * for its own reason: a block never closed at all. */
    static const uint8_t open[] = { 0x02,0x40, 0x01 };          /* block; nop */
    CK("a block left open still declines", build(open, sizeof open, &c, &a, &t), 0);
    bbq_arena_free(&a);
}

int main(void) {
    add_folds_to_one_tree();
    live_across_cut_becomes_root();
    dead_code_builds_nothing();
    poly_resolves_from_the_module();
    arity_matches_the_signature();
    walk_must_consume_the_whole_body();
    printf("%s\n", fails ? "  FAILED" : "  ok");
    return fails != 0;
}
