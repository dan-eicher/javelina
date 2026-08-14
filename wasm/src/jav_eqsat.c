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

/* ── the e-class analysis (egg §4.1): constant-ness ─────────
 * `make` computes the fact per e-node — a const IS its value, and a pure
 * operator over const-fact kids folds by its own §4.3.2 equation — and
 * `modify` puts the folded constant INTO the class as an e-node, which is
 * what lets extraction pick it. The declaration lives beside the rules in
 * jav_axioms.burg; the generated installer (jav_rewrite.h) sizes and wires
 * the struct, so no consumer assembles an eg_analysis by hand. */
typedef struct {
    uint8_t kind;      /* 0 = unknown, 1 = const i32, 2 = const i64 */
    int64_t v;         /* kind 1: the value as a sign-extended int32 */
} jav_eq_fact_t;

/* The §4.3.2 folds, one per operator (the analog's law: the OPERATOR is the
 * rule's, never read back out of a class), each the printed equation in
 * modular C. Verbatim, printed page 94: "iadd_N(i1,i2) = (i1 + i2) mod 2^N" ·
 * "isub_N(i1,i2) = (i1 − i2 + 2^N) mod 2^N" · "imul_N(i1,i2) = (i1 · i2) mod
 * 2^N"; page 95/96: iand/ior/ixor are the pointwise bit operations; the
 * shifts take "k be i2 modulo N"; page 97: "ieqz_N(i) = bool(i = 0)".
 * Unsigned C arithmetic IS "mod 2^N", so each body is the equation itself. */
static int32_t f_add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t f_sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t f_mul32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t f_and32(int32_t a, int32_t b) { return a & b; }
static int32_t f_or32 (int32_t a, int32_t b) { return a | b; }
static int32_t f_xor32(int32_t a, int32_t b) { return a ^ b; }
static int32_t f_shl32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a << ((uint32_t)b & 31)); }
static int32_t f_shru32(int32_t a, int32_t b){ return (int32_t)((uint32_t)a >> ((uint32_t)b & 31)); }
static int32_t f_shrs32(int32_t a, int32_t b){
    /* ishr_s: "extended with the most significant bit" — arithmetic shift,
     * spelled without C's implementation-defined signed >>. */
    uint32_t k = (uint32_t)b & 31, u = (uint32_t)a;
    uint32_t r = u >> k;
    if (a < 0 && k) r |= ~0u << (32 - k);
    return (int32_t)r;
}
static int64_t f_add64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
static int64_t f_sub64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
static int64_t f_mul64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }
static int64_t f_and64(int64_t a, int64_t b) { return a & b; }
static int64_t f_or64 (int64_t a, int64_t b) { return a | b; }
static int64_t f_xor64(int64_t a, int64_t b) { return a ^ b; }
static int64_t f_shl64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a << ((uint64_t)b & 63)); }
static int64_t f_shru64(int64_t a, int64_t b){ return (int64_t)((uint64_t)a >> ((uint64_t)b & 63)); }
static int64_t f_shrs64(int64_t a, int64_t b){
    uint64_t k = (uint64_t)b & 63, u = (uint64_t)a;
    uint64_t r = u >> k;
    if (a < 0 && k) r |= ~0ull << (64 - k);
    return (int64_t)r;
}

void jav_eq_make(egraph* g, int op, int64_t data,
                 const eg_id* kids, int nkids, void* out, void* user) {
    (void)user;
    jav_eq_fact_t* f = (jav_eq_fact_t*)out;
    f->kind = 0;
    if (op == OP_I32_CONST) { f->kind = 1; f->v = data; return; }
    if (op == OP_I64_CONST) { f->kind = 2; f->v = data; return; }
    if (op > 255 || !jav_eqsat_pure[op] || nkids < 1 || nkids > 2) return;
    const jav_eq_fact_t* a = (const jav_eq_fact_t*)eg_class_data(g, kids[0]);
    const jav_eq_fact_t* b = nkids == 2
        ? (const jav_eq_fact_t*)eg_class_data(g, kids[1]) : NULL;
    if (!a || !a->kind || (nkids == 2 && (!b || !b->kind))) return;
    int32_t a32 = (int32_t)a->v, b32 = b ? (int32_t)b->v : 0;
    int64_t a64 = a->v,          b64 = b ? b->v : 0;
    int k1 = a->kind == 1 && (!b || b->kind == 1);
    int k2 = a->kind == 2 && (!b || b->kind == 2);
    switch (op) {
    /* the i32 family — every operand fact must be kind 1 */
    case OP_I32_ADD:   if (k1) { f->kind = 1; f->v = f_add32(a32, b32); } return;
    case OP_I32_SUB:   if (k1) { f->kind = 1; f->v = f_sub32(a32, b32); } return;
    case OP_I32_MUL:   if (k1) { f->kind = 1; f->v = f_mul32(a32, b32); } return;
    case OP_I32_AND:   if (k1) { f->kind = 1; f->v = f_and32(a32, b32); } return;
    case OP_I32_OR:    if (k1) { f->kind = 1; f->v = f_or32(a32, b32); }  return;
    case OP_I32_XOR:   if (k1) { f->kind = 1; f->v = f_xor32(a32, b32); } return;
    case OP_I32_SHL:   if (k1) { f->kind = 1; f->v = f_shl32(a32, b32); } return;
    case OP_I32_SHR_S: if (k1) { f->kind = 1; f->v = f_shrs32(a32, b32); } return;
    case OP_I32_SHR_U: if (k1) { f->kind = 1; f->v = f_shru32(a32, b32); } return;
    /* §4.3.2 p97: "ieqz_N(i) = bool(i = 0)" */
    case OP_I32_EQZ:   if (a->kind == 1) { f->kind = 1; f->v = a32 == 0; } return;
    case OP_I64_EQZ:   if (a->kind == 2) { f->kind = 1; f->v = a->v == 0; } return;
    /* the i64 family */
    case OP_I64_ADD:   if (k2) { f->kind = 2; f->v = f_add64(a64, b64); } return;
    case OP_I64_SUB:   if (k2) { f->kind = 2; f->v = f_sub64(a64, b64); } return;
    case OP_I64_MUL:   if (k2) { f->kind = 2; f->v = f_mul64(a64, b64); } return;
    case OP_I64_AND:   if (k2) { f->kind = 2; f->v = f_and64(a64, b64); } return;
    case OP_I64_OR:    if (k2) { f->kind = 2; f->v = f_or64(a64, b64); }  return;
    case OP_I64_XOR:   if (k2) { f->kind = 2; f->v = f_xor64(a64, b64); } return;
    case OP_I64_SHL:   if (k2) { f->kind = 2; f->v = f_shl64(a64, b64); } return;
    case OP_I64_SHR_S: if (k2) { f->kind = 2; f->v = f_shrs64(a64, b64); } return;
    case OP_I64_SHR_U: if (k2) { f->kind = 2; f->v = f_shru64(a64, b64); } return;
    /* §4.3.6 conversions with no partiality: wrap discards the upper half
     * (the value mod 2^32); the extends embed exactly. */
    case OP_I32_WRAP_I64:   if (a->kind == 2) { f->kind = 1; f->v = (int64_t)(int32_t)a->v; } return;
    case OP_I64_EXTEND_I32_S: if (a->kind == 1) { f->kind = 2; f->v = a->v; } return;
    case OP_I64_EXTEND_I32_U: if (a->kind == 1) { f->kind = 2; f->v = (int64_t)(uint32_t)a->v; } return;
    default: return;
    }
}

/* INTERSECTION, never union: two e-nodes in one class denote the SAME value,
 * so the combined fact is the strongest one both are consistent with.
 * "Unknown" is NO information (the whole value set), and intersecting a
 * known constant with it keeps the constant — an opaque member merged into
 * a proven-constant class must not erase the proof. Two DIFFERENT known
 * constants would be a contradiction (one value, two proofs); fail closed
 * to unknown rather than pick a side. */
void jav_eq_join(const void* a, const void* b, void* out, void* user) {
    (void)user;
    const jav_eq_fact_t* fa = (const jav_eq_fact_t*)a;
    const jav_eq_fact_t* fb = (const jav_eq_fact_t*)b;
    jav_eq_fact_t* fo = (jav_eq_fact_t*)out;
    if (!fa->kind)                                    *fo = *fb;
    else if (!fb->kind)                               *fo = *fa;
    else if (fa->kind == fb->kind && fa->v == fb->v)  *fo = *fa;
    else                                              fo->kind = 0;
}

/* Put the fact's constant INTO the class as an e-node, so extraction can
 * pick it. Guarded on the class holding a node the fence admitted (a real
 * opcode: opaque and local keys sit above the byte range) — the analog's
 * law: "equating one with a constant would delete the effect, not fold it".
 * Idempotent: interning an existing term returns its class and the merge of
 * a class with itself learns nothing. */
void jav_eq_modify(egraph* g, eg_id c, const void* d, void* user) {
    (void)user;
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)d;
    if (!f->kind) return;
    int held_pure = 0, n = eg_class_nodes(g, c);
    for (int i = 0; i < n && !held_pure; i++)
        if (eg_class_node_op(g, c, i) <= 255) held_pure = 1;
    if (!held_pure) return;
    eg_id k = eg_add(g, f->kind == 1 ? OP_I32_CONST : OP_I64_CONST, f->v, NULL, 0);
    eg_merge(g, c, k);
}

/* Guard helpers the rule file's `where` clauses call (verbatim C inside the
 * generated matchers, so the prototypes come before the include). A guard
 * reads the ANALYSIS fact — the value vocabulary rules discriminate on. */
static int jav_eq_is_const32(egraph* g, eg_id c, int32_t v);
static int jav_eq_is_const64(egraph* g, eg_id c, int64_t v);

#include "jav_rewrite.h"   /* the generated matchers + installer; ONE TU only */

static int jav_eq_is_const32(egraph* g, eg_id c, int32_t v) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 1 && (int32_t)f->v == v;
}
static int jav_eq_is_const64(egraph* g, eg_id c, int64_t v) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 2 && f->v == v;
}

/* Template auxiliaries (AUXILIARIES in jav_axioms.burg): each takes classes,
 * returns a class. The zero-likes give `x − x` and `x ^ x` their result —
 * §4.3.2's equations at i1 = i2 yield 0 exactly, at either width. */
eg_id jav_eq_zero32(egraph* g, eg_id self) {
    (void)self;
    return eg_add(g, OP_I32_CONST, 0, NULL, 0);
}
eg_id jav_eq_zero64(egraph* g, eg_id self) {
    (void)self;
    return eg_add(g, OP_I64_CONST, 0, NULL, 0);
}

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
    /* rebuild inputs */
    const jav_tctx_t* tcx;
    bbq_hmap*      synth;       /* the emitter's sidecar: node -> jav_synth_t* */
    bbq_hmap*      facts;       /* Part F: producer node -> jav_eq_fact_t* (body-wide) */
    const uint32_t* snap;       /* version snapshot at the CURRENT root's entry */
    /* kept original subtrees, in the new tree's postorder — the order fence */
    const jav_tnode_t* kept[64];
    int            nkept;
    int            refuse;      /* a fence fired mid-rebuild */
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
     * region opened on, opaque by identity — which keeps it undroppable (the
     * pop it stands for is owed). Part F: the builder linked it to its
     * PRODUCER (kids[0], dead storage at nkids 0), and if the producer's
     * region proved its class constant, the leaf's class merges with that
     * constant — the fact crosses the cut. Extraction still prefers the
     * leaf (an opaque costs less than a spelled constant), so the rules
     * that can cash the fact are exactly the ones that KEEP the carried
     * operand; every other form drops it and the rebuild fence refuses. */
    if (!n->pc) {
        eg_id id = intern_as(c, n, JAV_EQ_OP_OPAQUE, (int64_t)(uintptr_t)n, NULL, 0);
        if (!c->failed && c->facts && n->nkids == 0 && n->kids[0]) {
            const jav_eq_fact_t* pf = (const jav_eq_fact_t*)
                bbq_hmap_get(c->facts, (uint64_t)(uintptr_t)n->kids[0]);
            if (pf && pf->kind) {
                eg_id k = eg_add(c->g, pf->kind == 1 ? OP_I32_CONST : OP_I64_CONST,
                                 pf->v, NULL, 0);
                eg_merge(c->g, id, k);
                eg_rebuild(c->g);
            }
        }
        return id;
    }
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

/* ── the rebuild (C3): a differing extraction becomes a tree ─────
 *
 * Three fences, each a refusal that keeps the original (D7), each counted:
 *
 *   splice version   an extracted LOCAL must carry the version current at
 *                    THIS root's entry — the analog's law: "only the version
 *                    live where the tree is being spliced is that slot's
 *                    current value". A get hoisted across a set is refused,
 *                    not misread.
 *   pure drops only  every node of the OLD root that the new tree does not
 *                    keep must have interned as PURE or LOCAL. An opaque
 *                    node outside the kept set is an effect (a call) or a
 *                    carried stack value the region still owes a pop —
 *                    deleting either changes behavior, so the root refuses.
 *   original order   the kept original subtrees must appear in the new
 *                    tree's postorder in their own byte order: the stamper's
 *                    position tracking (dead-code gaps) reads pcs, and
 *                    reordered spans would false-trigger a live reset.
 *
 * A synthesized node gets `pc == NULL`, its (op, imm) in the emitter's
 * sidecar, its `sig` from the opcode's own concrete signature (every v1
 * vocabulary op is concrete-typed; an extracted local resolves through the
 * declared row to the final of its slot's class), and the builder's own
 * `need` formula. */
static const jav_tnode_t* rb_original(ictx_t* c, int64_t data) {
    const jav_tnode_t* n = (const jav_tnode_t*)(uintptr_t)data;
    if (c->nkept < (int)(sizeof c->kept / sizeof c->kept[0])) c->kept[c->nkept++] = n;
    else c->refuse = 1;                       /* too many to fence; keep original */
    return n;
}

/* The final signature an extracted local.get resolves to: the declared row's
 * resolution list, filtered to the slot's class — the same answer the
 * builder's resolve gives, read from the same generated table. */
static int rb_local_sig(const ictx_t* c, uint32_t slot) {
    uint8_t cls = c->tcx->local_class[slot];
    const jav_sig_t* d = &jav_sigtab[jav_opcode_sig[OP_LOCAL_GET].sig];
    for (uint16_t i = 0; i < d->nresolve; i++) {
        const jav_sig_t* s = &jav_sigtab[d->resolves_to[i]];
        if (s->nresults == 1 && s->results[0] == cls) return d->resolves_to[i];
    }
    return -1;
}

static jav_tnode_t* rb_synth(ictx_t* c, uint8_t op, int64_t imm, int sig,
                             jav_tnode_t** kids, int nkids) {
    if (sig < 0 || nkids > JAV_SIG_MAX_KIDS) { c->refuse = 1; return NULL; }
    jav_tnode_t* n = (jav_tnode_t*)bbq_arena_alloc(c->arena, sizeof *n);
    jav_synth_t* sr = (jav_synth_t*)bbq_arena_alloc(c->arena, sizeof *sr);
    if (!n || !sr) { c->refuse = 1; return NULL; }
    memset(n, 0, sizeof *n);
    n->sig = (uint16_t)sig;
    n->seq = JAV_TNODE_NO_SEQ;
    n->nkids = (uint8_t)nkids;
    /* the builder's own peak formula, over the signature's operand widths */
    {
        const jav_sig_t* sg = &jav_sigtab[sig];
        unsigned held = 0, peak = 0, kid_i = 0;
        for (uint8_t i = 0; i < sg->nparams; i++) {
            if (sg->params[i] == JSC_STK) continue;
            unsigned p = held + kids[kid_i]->need;
            if (p > peak) peak = p;
            held += jav_class_width[sg->params[i]];
            kid_i++;
        }
        if (held > peak) peak = held;
        if (sg->nresults) {
            unsigned rw = jav_class_width[sg->results[0]];
            if (rw > peak) peak = rw;
        }
        n->need = peak > 255 ? 255 : (uint8_t)peak;
    }
    for (int i = 0; i < nkids; i++) n->kids[i] = kids[i];
    sr->op = op; sr->imm = imm;
    bbq_hmap_put(c->synth, (uint64_t)(uintptr_t)n, sr);
    return n;
}

static jav_tnode_t* rb_node(ictx_t* c, const eg_extract_result* r, int xi) {
    if (c->refuse) return NULL;
    int op = r->ops[xi];
    int64_t data = r->data[xi];
    if (op == JAV_EQ_OP_OPAQUE)
        return (jav_tnode_t*)(uintptr_t)rb_original(c, data);
    if (op == JAV_EQ_OP_LOCAL) {
        uint32_t slot = (uint32_t)(data & 0xffffffff);
        uint32_t ver  = (uint32_t)((uint64_t)data >> 32);
        if (slot >= c->nlocals || ver != c->snap[slot]) { c->refuse = 1; return NULL; }
        return rb_synth(c, OP_LOCAL_GET, (int64_t)slot, rb_local_sig(c, slot), NULL, 0);
    }
    if (op == OP_I32_CONST || op == OP_I64_CONST)
        return rb_synth(c, (uint8_t)op, data, jav_opcode_sig[op].sig, NULL, 0);
    if (op <= 255 && jav_eqsat_pure[op]) {
        jav_tnode_t* kids[JAV_SIG_MAX_KIDS];
        int nk = r->nkids[xi];
        if (nk > JAV_SIG_MAX_KIDS) { c->refuse = 1; return NULL; }
        for (int i = 0; i < nk; i++) {
            kids[i] = rb_node(c, r, r->kids[r->kid_off[xi] + i]);
            if (c->refuse) return NULL;
        }
        return rb_synth(c, (uint8_t)op, 0, jav_opcode_sig[op].sig, kids, nk);
    }
    c->refuse = 1;                            /* an op the rebuild cannot spell */
    return NULL;
}

/* Every OLD node outside the kept subtrees must have interned pure-or-local
 * — the drop fence. Kept subtrees are skipped whole (they stamp whole). */
static int rb_kept_root(const ictx_t* c, const jav_tnode_t* n) {
    for (int i = 0; i < c->nkept; i++) if (c->kept[i] == n) return 1;
    return 0;
}
static int rb_drops_pure(ictx_t* c, const jav_tnode_t* n) {
    if (rb_kept_root(c, n)) return 1;
    const eq_rec_t* rec = rec_of(c, n);
    if (!rec || rec->op == JAV_EQ_OP_OPAQUE) return 0;
    for (int i = 0; i < n->nkids; i++)
        if (!rb_drops_pure(c, n->kids[i])) return 0;
    return 1;
}

static uint32_t rb_count(const jav_tnode_t* n) {
    uint32_t k = 1;
    for (int i = 0; i < n->nkids; i++) k += rb_count(n->kids[i]);
    return k;
}

/* Rebuild one root from its extraction, or return NULL with every original
 * intact. On success the tree's node count moves by what the swap changed,
 * so the picks identity (unpicked == carried) stays an identity. */
static jav_tnode_t* rb_root(ictx_t* c, const eg_extract_result* r,
                            const jav_tnode_t* old, int32_t* dnodes) {
    c->nkept = 0; c->refuse = 0;
    jav_tnode_t* nu = rb_node(c, r, r->root);
    if (c->refuse || !nu) return NULL;
    if (!rb_drops_pure(c, old)) return NULL;
    for (int i = 1; i < c->nkept; i++)
        if (c->kept[i - 1]->pc && c->kept[i]->pc
            && c->kept[i - 1]->pc > c->kept[i]->pc) return NULL;
    *dnodes = (int32_t)rb_count(nu) - (int32_t)rb_count(old);
    return nu;
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

static void eqsat_region(jav_ttree_t* tree, jav_tregion_t* reg,
                         const jav_tctx_t* tcx, bbq_arena* a, bbq_hmap* synth,
                         bbq_hmap* facts) {
    g_eq.regions++;
    egraph g; eg_init(&g);
    jav_eqsat_set_analysis(&g, NULL);
    bbq_hmap recs; bbq_hmap_init(&recs, 0);
    uint32_t nlocals = tcx ? tcx->nlocals : 0;
    uint32_t* version = NULL;
    uint32_t* snaps = NULL;               /* per-root version snapshot (D3a splice) */
    ictx_t c = {0};
    c.g = &g; c.arena = a; c.recs = &recs; c.nlocals = nlocals;
    c.tcx = tcx; c.synth = synth; c.facts = facts;
    if (nlocals) {
        /* A region can be EMPTY (zero roots — a cut immediately followed by
         * another); bbq_arena_alloc(0) is NULL by contract, which is not a
         * failure when there is nothing to snapshot. */
        version = (uint32_t*)bbq_arena_alloc(a, nlocals * sizeof *version);
        if (reg->nroots)
            snaps = (uint32_t*)bbq_arena_alloc(a, (size_t)reg->nroots * nlocals
                                                  * sizeof *snaps);
        if (!version || (reg->nroots && !snaps)) c.failed = 1;
        else memset(version, 0, nlocals * sizeof *version);
    }
    c.version = version;

    for (uint32_t i = 0; i < reg->nroots && !c.failed; i++) {
        if (nlocals) memcpy(snaps + (size_t)i * nlocals, version,
                            nlocals * sizeof *version);
        intern_node(&c, reg->roots[i]);
    }

    if (c.failed || eg_node_count(&g) > EQ_NODE_BUDGET) {
        g_eq.cap_refusals++;              /* originals stand, counted (D7) */
        bbq_hmap_free(&recs); eg_free(&g);
        return;
    }

    eg_caps caps = { EQ_ROUNDS, EQ_NODE_BUDGET };
    jav_eqsat_rewrite_region(&g, caps);
    {
        size_t nn = eg_node_count(&g);
        if (nn > g_eq.enodes_peak) g_eq.enodes_peak = nn;
    }

    /* Part F, the producing side: every VALUE root's post-saturation fact is
     * recorded against the ORIGINAL node — the pointer the next region's
     * carried leaf was linked to — before any rebuild replaces it. */
    if (facts)
        for (uint32_t i = 0; i < reg->nroots; i++) {
            const jav_tnode_t* rt = reg->roots[i];
            if (!jav_sigtab[rt->sig].nresults) continue;
            const eq_rec_t* rr = rec_of(&c, rt);
            if (!rr) continue;
            const jav_eq_fact_t* f = (const jav_eq_fact_t*)
                eg_class_data(&g, eg_find(&g, rr->id));
            if (!f || !f->kind) continue;
            jav_eq_fact_t* keep = (jav_eq_fact_t*)bbq_arena_alloc(a, sizeof *keep);
            if (!keep) break;
            *keep = *f;
            bbq_hmap_put(facts, (uint64_t)(uintptr_t)rt, keep);
        }

    for (uint32_t i = 0; i < reg->nroots; i++) {
        g_eq.roots++;
        const eq_rec_t* rr = rec_of(&c, reg->roots[i]);
        if (!rr) { g_eq.identity_fails++; continue; }
        eg_extract_result ex;
        if (!eg_extract(&g, rr->id, eq_cost, NULL, &ex)) {
            g_eq.identity_fails++;        /* no finite term: keep the original */
            continue;
        }
        if (!same_term(&c, &ex, ex.root, reg->roots[i])) {
            c.snap = nlocals ? snaps + (size_t)i * nlocals : NULL;
            int32_t dn = 0;
            jav_tnode_t* nu = rb_root(&c, &ex, reg->roots[i], &dn);
            if (nu) {
                reg->roots[i] = nu;
                tree->nnodes = (uint32_t)((int64_t)tree->nnodes + dn);
                g_eq.rewritten++;
            } else {
                g_eq.rebuild_refusals++;  /* original stands, counted (D7) */
            }
        }
        eg_extract_free(&ex);
    }

    bbq_hmap_free(&recs);
    eg_free(&g);
}

void jav_eqsat_body(jav_ttree_t* tree, const jav_tctx_t* tcx, bbq_arena* a,
                    bbq_hmap* synth) {
    g_eq.bodies++;
    /* Part F: region-entry facts. Regions run in order, so a producer's
     * post-saturation constant is on record before the region whose carried
     * leaf links back to it interns — the map is the cut the facts cross. */
    bbq_hmap facts; bbq_hmap_init(&facts, 0);
    for (uint32_t r = 0; r < tree->nregions; r++)
        eqsat_region(tree, &tree->regions[r], tcx, a, synth, &facts);
    bbq_hmap_free(&facts);
}

int jav_eqsat_rule_stats(const char* const** names,
                         const unsigned long long** fires) {
    *names = jav_eqsat_rule_names;
    *fires = jav_eqsat_rule_fires;
    return (int)jav_eqsat_NRULES;
}
