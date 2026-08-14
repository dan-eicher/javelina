/* jav_eqsat.c — tier-3's equality saturation over the tier-2 tree.
 *
 * Per region (a region is one e-graph; values do not flow across cuts —
 * Part F threads region-entry facts later, as a PART, not a deferral):
 *
 *   intern    pure subtrees become e-nodes keyed by the plain opcode byte;
 *             `i32.const`/`i64.const` carry their decoded value as `data`
 *             (egraph.h: "data is part of the KEY"); `local.get` interns as
 *             (JAV_EQ_OP_LOCAL, slot | version<<32) — the D3a versioned
 *             leaf, versions threaded in the walk's own postorder (== seq
 *             order, the order invariant) and bumped at local.set/tee;
 *             everything the generated fence (jav_eqsat_ops.h) does not
 *             admit is OPAQUE — (JAV_EQ_OP_OPAQUE, the node's address),
 *             its own congruence class, no inputs, pure kids still graphed.
 *   saturate  jav_eqsat_rewrite_region (src/gen/jav_rewrite.h, generated
 *             from spec/jav_axioms.burg) under the analog's caps.
 *   extract   per root, cheapest term (cost below); an extraction IDENTICAL
 *             to the original keeps the ORIGINAL subtree, pointer and all —
 *             with zero rules that is every root, and tier-3 is tier-2
 *             structurally (PIN B-1). A differing extraction is Part C3's
 *             rebuild; until it lands, differing keeps the original too
 *             (D7: fail closed, counted).
 */
#include "jav_eqsat.h"

#include <stdlib.h>
#include <string.h>

#include "opcodes.h"
#include "bbq_hmap.h"
#include "jav_eqsat_ops.h"
#include "egraph.h"

/* ── the e-class analysis (egg §4.1) ────────────────────────
 * Constant-ness. Part C2 gives `make` the const facts and `modify` the fold;
 * until then the domain is inert: every fact is "unknown", join of unknowns
 * is unknown, and modify adds nothing. The SHAPE ships now because the
 * generated installer (jav_rewrite.h) sizes and wires it — the declaration
 * lives beside the rules in jav_axioms.burg, so no consumer assembles an
 * eg_analysis by hand. */
typedef struct {
    uint8_t kind;      /* 0 = unknown, 1 = const i32, 2 = const i64 (Part C2) */
    int64_t v;
} jav_eq_fact_t;

void jav_eq_make(egraph* g, int op, int64_t data,
                 const eg_id* kids, int nkids, void* out, void* user) {
    (void)g; (void)op; (void)data; (void)kids; (void)nkids; (void)user;
    ((jav_eq_fact_t*)out)->kind = 0;
}

/* INTERSECTION, never union: two e-nodes in one class denote the SAME value,
 * so the combined fact is what both agree on. */
void jav_eq_join(const void* a, const void* b, void* out, void* user) {
    (void)user;
    const jav_eq_fact_t* fa = (const jav_eq_fact_t*)a;
    const jav_eq_fact_t* fb = (const jav_eq_fact_t*)b;
    jav_eq_fact_t* fo = (jav_eq_fact_t*)out;
    if (fa->kind && fb->kind && fa->kind == fb->kind && fa->v == fb->v) *fo = *fa;
    else fo->kind = 0;
}

void jav_eq_modify(egraph* g, eg_id c, const void* d, void* user) {
    (void)g; (void)c; (void)d; (void)user;   /* Part C2: fold the constant in */
}

#include "jav_rewrite.h"   /* the generated matchers + installer; ONE TU only */

/* ── caps (D5: the analog's, until measured on this corpus) ── */
enum { EQ_ROUNDS = 12, EQ_NODE_BUDGET = 8192 };

static jav_eqsat_stats_t g_eq;
const jav_eqsat_stats_t* jav_eqsat_stats(void) { return &g_eq; }
void jav_eqsat_stats_reset(void) { memset(&g_eq, 0, sizeof g_eq); }

/* What intern assigned one tree node, remembered for the identity compare
 * (and, in Part C3, for the rebuild): the e-node key and its class. */
typedef struct { int op; int64_t data; eg_id id; } eq_rec_t;

typedef struct {
    egraph*        g;
    bbq_arena*     arena;
    bbq_hmap*      recs;        /* jav_tnode_t* -> eq_rec_t* */
    uint32_t*      version;     /* per local slot, D3a */
    uint32_t       nlocals;
    int            failed;      /* an intern step could not proceed; region refused */
} ictx_t;

static eq_rec_t* rec_of(ictx_t* c, const jav_tnode_t* n) {
    return (eq_rec_t*)bbq_hmap_get(c->recs, (uint64_t)(uintptr_t)n);
}

static eg_id intern_as(ictx_t* c, const jav_tnode_t* n, int op, int64_t data,
                       const eg_id* kids, int nkids) {
    eq_rec_t* r = (eq_rec_t*)bbq_arena_alloc(c->arena, sizeof *r);
    if (!r) { c->failed = 1; return 0; }
    r->op = op; r->data = data;
    r->id = eg_add(c->g, op, data, kids, nkids);
    bbq_hmap_put(c->recs, (uint64_t)(uintptr_t)n, r);
    return r->id;
}

/* Postorder over one subtree — which is seq order, so the version thread
 * reads exactly the store order the bytes execute in. */
static eg_id intern_node(ictx_t* c, jav_tnode_t* n) {
    if (c->failed) return 0;

    /* Kids first, whatever the parent turns out to be: an opaque parent's
     * pure children still get graphs (the analog's law), and a set/tee's
     * operand is interned BEFORE the version bumps — the stored value was
     * computed against the old version. */
    eg_id kids[JAV_SIG_MAX_KIDS];
    int nkids = n->nkids;
    for (int i = 0; i < nkids; i++) kids[i] = intern_node(c, n->kids[i]);
    if (c->failed) return 0;

    /* A carried leaf has no instruction (pc == NULL): a stack slot the
     * region opened on, opaque by identity. */
    if (!n->pc) return intern_as(c, n, JAV_EQ_OP_OPAQUE,
                                 (int64_t)(uintptr_t)n, NULL, 0);
    uint8_t op = n->pc[0];

    if (op == OP_LOCAL_GET) {
        bbq_ctx_t ic; bbq_ctx_init(&ic, n->pc + 1, 5);
        uint32_t slot = 0;
        if (!bbq_read_uleb128_u32(&ic, &slot) || slot >= c->nlocals) {
            c->failed = 1; return 0;
        }
        int64_t data = (int64_t)slot | ((int64_t)c->version[slot] << 32);
        return intern_as(c, n, JAV_EQ_OP_LOCAL, data, NULL, 0);
    }
    if (op == OP_LOCAL_SET || op == OP_LOCAL_TEE) {
        bbq_ctx_t ic; bbq_ctx_init(&ic, n->pc + 1, 5);
        uint32_t slot = 0;
        if (!bbq_read_uleb128_u32(&ic, &slot) || slot >= c->nlocals) {
            c->failed = 1; return 0;
        }
        c->version[slot]++;               /* D3a: the store starts a new version */
        return intern_as(c, n, JAV_EQ_OP_OPAQUE, (int64_t)(uintptr_t)n, NULL, 0);
    }
    if (op == OP_I32_CONST || op == OP_I64_CONST) {
        bbq_ctx_t ic; bbq_ctx_init(&ic, n->pc + 1, 10);
        int64_t v = 0;
        if (op == OP_I32_CONST) {
            int32_t v32 = 0;
            if (!bbq_read_sleb128_i32(&ic, &v32)) { c->failed = 1; return 0; }
            v = v32;
        } else if (!bbq_read_sleb128_i64(&ic, &v)) { c->failed = 1; return 0; }
        return intern_as(c, n, op, v, NULL, 0);
    }
    if (jav_eqsat_pure[op])
        return intern_as(c, n, op, 0, kids, nkids);

    /* Unadmitted: opaque by identity — the tree keeps its place. */
    return intern_as(c, n, JAV_EQ_OP_OPAQUE, (int64_t)(uintptr_t)n, NULL, 0);
}

/* Does the extracted term rooted at r[xi] spell the original subtree `n`
 * node-for-node? Compared against what intern RECORDED, so the walk cannot
 * disagree with itself about versions consumed mid-region. */
static int same_term(ictx_t* c, const eg_extract_result* r, int xi,
                     const jav_tnode_t* n) {
    const eq_rec_t* rec = rec_of(c, n);
    if (!rec) return 0;
    if (r->ops[xi] != rec->op || r->data[xi] != rec->data) return 0;
    /* An opaque or leaf e-node has no kids whatever the tree node had:
     * the subtree lives UNDER the identity, not in the graph's arity. */
    if (rec->op == JAV_EQ_OP_OPAQUE || rec->op == JAV_EQ_OP_LOCAL
        || rec->op == OP_I32_CONST || rec->op == OP_I64_CONST)
        return r->nkids[xi] == 0;
    if (r->nkids[xi] != n->nkids) return 0;
    for (int i = 0; i < n->nkids; i++)
        if (!same_term(c, r, r->kids[r->kid_off[xi] + i], n->kids[i])) return 0;
    return 1;
}

/* D4's shape: 256·bytes + 1 per node (size first, AST count as tiebreak).
 * Exact where the choice can exist — a const's LEB width varies by value, a
 * local re-encodes as get+index — and immaterial where it cannot: an OPAQUE
 * node is the only member of its class by construction (its data is its own
 * address, which nothing else can equal), so its price never decides. */
static int leb_len_u(uint64_t v) { int n = 1; while (v >>= 7) n++; return n; }
static int sleb_len(int64_t v) {
    int n = 0;
    for (;;) {
        uint8_t b = (uint8_t)(v & 0x7f);
        v >>= 7;
        n++;
        if ((v == 0 && !(b & 0x40)) || (v == -1 && (b & 0x40))) return n;
    }
}
static int eq_cost(int op, int64_t data, void* user) {
    (void)user;
    int bytes;
    if (op == OP_I32_CONST || op == OP_I64_CONST) bytes = 1 + sleb_len(data);
    else if (op == JAV_EQ_OP_LOCAL) bytes = 1 + leb_len_u((uint64_t)(uint32_t)data);
    else bytes = 1;
    return 256 * bytes + 1;
}

static void eqsat_region(const jav_tregion_t* reg, const jav_tctx_t* tcx,
                         bbq_arena* a) {
    g_eq.regions++;
    egraph g; eg_init(&g);
    jav_eqsat_set_analysis(&g, NULL);
    bbq_hmap recs; bbq_hmap_init(&recs, 0);
    uint32_t nlocals = tcx ? tcx->nlocals : 0;
    uint32_t* version = NULL;
    ictx_t c = { &g, a, &recs, NULL, nlocals, 0 };
    if (nlocals) {
        version = (uint32_t*)bbq_arena_alloc(a, nlocals * sizeof *version);
        if (!version) c.failed = 1;
        else memset(version, 0, nlocals * sizeof *version);
    }
    c.version = version;

    for (uint32_t i = 0; i < reg->nroots && !c.failed; i++)
        intern_node(&c, reg->roots[i]);

    if (c.failed || eg_node_count(&g) > EQ_NODE_BUDGET) {
        g_eq.cap_refusals++;              /* originals stand, counted (D7) */
        bbq_hmap_free(&recs); eg_free(&g);
        return;
    }

    eg_caps caps = { EQ_ROUNDS, EQ_NODE_BUDGET };
    jav_eqsat_rewrite_region(&g, caps);

    for (uint32_t i = 0; i < reg->nroots; i++) {
        g_eq.roots++;
        const eq_rec_t* rr = rec_of(&c, reg->roots[i]);
        if (!rr) { g_eq.identity_fails++; continue; }
        eg_extract_result ex;
        if (!eg_extract(&g, rr->id, eq_cost, NULL, &ex)) {
            g_eq.identity_fails++;        /* no finite term: keep the original */
            continue;
        }
        if (!same_term(&c, &ex, ex.root, reg->roots[i]))
            g_eq.rewritten++;             /* Part C3 rebuilds here; until then
                                           * the original stands (D7) */
        eg_extract_free(&ex);
    }

    bbq_hmap_free(&recs);
    eg_free(&g);
}

void jav_eqsat_body(const jav_ttree_t* tree, const jav_tctx_t* tcx, bbq_arena* a) {
    g_eq.bodies++;
    for (uint32_t r = 0; r < tree->nregions; r++)
        eqsat_region(&tree->regions[r], tcx, a);
}
