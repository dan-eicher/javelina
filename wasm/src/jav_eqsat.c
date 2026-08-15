/* jav_eqsat.c — tier-3's equality saturation over the tier-2 tree.
 *
 * Per region (a region is one e-graph; a value that crosses a cut arrives
 * as a carried leaf, and only its proven CONSTANT-ness follows it — the
 * producer-link fact channel below):
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
 *             structurally. A differing extraction goes through the rebuild
 *             and its fences; any refusal keeps the original, counted —
 *             fail closed, the engine never aborts.
 */
#include "jav_eqsat.h"

#include <stdlib.h>
#include <string.h>

#include "opcodes.h"
#include "bbq_hmap.h"
#include "jav_eqsat_ops.h"
#include "jav_jit_meta.h"    /* the 0xFD sub-table's operand kinds (lane immediates) */
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
    /* §4.3.4 Conversions, printed 110: "wrap_{M,N}(i) = i mod 2^N";
     * "extend_u_{M,N}(i) = i"; extend_s through the signed interpretation —
     * total, no partiality. */
    case OP_I32_WRAP_I64:   if (a->kind == 2) { f->kind = 1; f->v = (int64_t)(int32_t)a->v; } return;
    case OP_I64_EXTEND_I32_S: if (a->kind == 1) { f->kind = 2; f->v = a->v; } return;
    case OP_I64_EXTEND_I32_U: if (a->kind == 1) { f->kind = 2; f->v = (int64_t)(uint32_t)a->v; } return;
    /* §4.3.2 relational operators, printed 97/98 — every result is an i32
     * bool: "ieq_N(i1,i2) = bool(i1 = i2)" · "ine" bool(i1 ≠ i2) ·
     * "ilt_u = bool(i1 < i2)" and ilt_s/igt/ile/ige through the signed
     * interpretation where the suffix says so. The unsigned forms compare
     * the representation values, which is what the uint casts spell. */
    case OP_I32_EQ:   if (k1) { f->kind = 1; f->v = a32 == b32; } return;
    case OP_I32_NE:   if (k1) { f->kind = 1; f->v = a32 != b32; } return;
    case OP_I32_LT_S: if (k1) { f->kind = 1; f->v = a32 < b32; }  return;
    case OP_I32_LT_U: if (k1) { f->kind = 1; f->v = (uint32_t)a32 < (uint32_t)b32; } return;
    case OP_I32_GT_S: if (k1) { f->kind = 1; f->v = a32 > b32; }  return;
    case OP_I32_GT_U: if (k1) { f->kind = 1; f->v = (uint32_t)a32 > (uint32_t)b32; } return;
    case OP_I32_LE_S: if (k1) { f->kind = 1; f->v = a32 <= b32; } return;
    case OP_I32_LE_U: if (k1) { f->kind = 1; f->v = (uint32_t)a32 <= (uint32_t)b32; } return;
    case OP_I32_GE_S: if (k1) { f->kind = 1; f->v = a32 >= b32; } return;
    case OP_I32_GE_U: if (k1) { f->kind = 1; f->v = (uint32_t)a32 >= (uint32_t)b32; } return;
    case OP_I64_EQ:   if (k2) { f->kind = 1; f->v = a64 == b64; } return;
    case OP_I64_NE:   if (k2) { f->kind = 1; f->v = a64 != b64; } return;
    case OP_I64_LT_S: if (k2) { f->kind = 1; f->v = a64 < b64; }  return;
    case OP_I64_LT_U: if (k2) { f->kind = 1; f->v = (uint64_t)a64 < (uint64_t)b64; } return;
    case OP_I64_GT_S: if (k2) { f->kind = 1; f->v = a64 > b64; }  return;
    case OP_I64_GT_U: if (k2) { f->kind = 1; f->v = (uint64_t)a64 > (uint64_t)b64; } return;
    case OP_I64_LE_S: if (k2) { f->kind = 1; f->v = a64 <= b64; } return;
    case OP_I64_LE_U: if (k2) { f->kind = 1; f->v = (uint64_t)a64 <= (uint64_t)b64; } return;
    case OP_I64_GE_S: if (k2) { f->kind = 1; f->v = a64 >= b64; } return;
    case OP_I64_GE_U: if (k2) { f->kind = 1; f->v = (uint64_t)a64 >= (uint64_t)b64; } return;
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
    if (f->kind != 1 && f->kind != 2) return;   /* only the scalar consts intern */
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
static int jav_eq_has_const32(egraph* g, eg_id c);
static int jav_eq_has_const64(egraph* g, eg_id c);
static int jav_eq_is_v128_zero(egraph* g, eg_id c);
static int jav_eq_is_v128_ones(egraph* g, eg_id c);
static int jav_eq_is_v128_lane1(egraph* g, eg_id c, int lanebytes);
static int jav_eq_shuffle_is_id(egraph* g, eg_id c, int which);
static int jav_eq_is_pow2_32(egraph* g, eg_id c);
static int jav_eq_is_pow2_64(egraph* g, eg_id c);
static int jav_eq_is_v128_splat_pow2(egraph* g, eg_id c, int lanebytes);

#include "jav_rewrite.h"   /* the generated matchers + installer; ONE TU only */

static int jav_eq_is_const32(egraph* g, eg_id c, int32_t v) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 1 && (int32_t)f->v == v;
}
static int jav_eq_is_const64(egraph* g, eg_id c, int64_t v) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 2 && f->v == v;
}
static int jav_eq_has_const32(egraph* g, eg_id c) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 1;
}
static int jav_eq_has_const64(egraph* g, eg_id c) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 2;
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
/* bool(true) for the self-compare identities — a comparison's result is an
 * i32 whatever its operands' width (§4.3.2 Boolean Interpretation). */
eg_id jav_eq_one32(egraph* g, eg_id self) {
    (void)self;
    return eg_add(g, OP_I32_CONST, 1, NULL, 0);
}
/* (the vector auxiliaries — zero_v128, the log2s — live below the region
 * context they read; the generated header declares them extern, so the
 * definition site is free) */

/* The refold auxiliaries: called only under has_const guards, each returns
 * the folded constant's class at its OWN op's §4.3.2 equation — one per
 * operator, the analog's law, so an opcode is never read back out of a
 * class. A guard raced false would fall back to unknown facts; folding 0s
 * then would still be an equality nothing asserted, so fail closed to the
 * left operand's class (a no-op merge). */
#define REFOLD32(name, fn) \
    eg_id name(egraph* g, eg_id b, eg_id c) { \
        const jav_eq_fact_t* fb = (const jav_eq_fact_t*)eg_class_data(g, b); \
        const jav_eq_fact_t* fc = (const jav_eq_fact_t*)eg_class_data(g, c); \
        if (!fb || !fc || fb->kind != 1 || fc->kind != 1) return b; \
        return eg_add(g, OP_I32_CONST, \
                      (int64_t)fn((int32_t)fb->v, (int32_t)fc->v), NULL, 0); \
    }
#define REFOLD64(name, fn) \
    eg_id name(egraph* g, eg_id b, eg_id c) { \
        const jav_eq_fact_t* fb = (const jav_eq_fact_t*)eg_class_data(g, b); \
        const jav_eq_fact_t* fc = (const jav_eq_fact_t*)eg_class_data(g, c); \
        if (!fb || !fc || fb->kind != 2 || fc->kind != 2) return b; \
        return eg_add(g, OP_I64_CONST, fn(fb->v, fc->v), NULL, 0); \
    }
REFOLD32(jav_eq_refold_add32, f_add32)
REFOLD32(jav_eq_refold_mul32, f_mul32)
REFOLD32(jav_eq_refold_and32, f_and32)
REFOLD32(jav_eq_refold_or32,  f_or32)
REFOLD32(jav_eq_refold_xor32, f_xor32)
REFOLD64(jav_eq_refold_add64, f_add64)
REFOLD64(jav_eq_refold_mul64, f_mul64)
REFOLD64(jav_eq_refold_and64, f_and64)
REFOLD64(jav_eq_refold_or64,  f_or64)
REFOLD64(jav_eq_refold_xor64, f_xor64)

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
    bbq_hmap*      facts;       /* cross-region: producer node -> jav_eq_fact_t* (body-wide) */
    const uint32_t* snap;       /* version snapshot at the CURRENT root's entry */
    /* kept original subtrees, in the new tree's postorder — the order fence */
    const jav_tnode_t* kept[64];
    int            nkept;
    int            refuse;      /* a fence fired mid-rebuild */
    const jav_tnode_t* rb_old;  /* the root being rebuilt: the pool an
                                 * extraction's unspellable leaf (a 16-byte
                                 * const, a shuffle) is recovered from — the
                                 * ORIGINAL subtree that interned as it */
    /* 16-byte immediates (v128.const values, shuffle lane patterns), deduped
     * by CONTENT so a blob index equality IS value equality within the
     * region — the discriminant `data` carries the index. */
    uint8_t      (*blobs)[16];
    uint32_t       nblobs, blobcap;
} ictx_t;

/* The region currently saturating, for the guard auxiliaries: a `where`
 * clause receives classes and the graph, and the 16-byte immediates it needs
 * to inspect live here. Single-threaded like the emitter's own context. */
static ictx_t* g_cur;

static int64_t blob_intern(ictx_t* c, const uint8_t* p) {
    for (uint32_t i = 0; i < c->nblobs; i++)
        if (memcmp(c->blobs[i], p, 16) == 0) return (int64_t)i;
    if (c->nblobs == c->blobcap) {
        uint32_t ncap = c->blobcap ? c->blobcap * 2 : 8;
        uint8_t (*nb)[16] = (uint8_t(*)[16])bbq_arena_alloc(c->arena, (size_t)ncap * 16);
        if (!nb) return -1;
        if (c->nblobs) memcpy(nb, c->blobs, (size_t)c->nblobs * 16);
        c->blobs = nb; c->blobcap = ncap;
    }
    memcpy(c->blobs[c->nblobs], p, 16);
    return (int64_t)c->nblobs++;
}

/* Class-inspection guards for the vector rules: iterate the class's own
 * e-nodes for a v128.const / shuffle and test its blob. Direct reads of the
 * graph — no analysis domain carries 16-byte facts in v1. */
static int v128_const_blob(egraph* g, eg_id c, const uint8_t** out) {
    int n = eg_class_nodes(g, c);
    for (int i = 0; i < n; i++)
        if (eg_class_node_op(g, c, i) == (int)JAV_EQ_OP_V128_CONST) {
            int64_t bi = eg_class_node_data(g, c, i);
            if (g_cur && bi >= 0 && (uint32_t)bi < g_cur->nblobs) {
                *out = g_cur->blobs[bi];
                return 1;
            }
        }
    return 0;
}
static int jav_eq_is_v128_zero(egraph* g, eg_id c) {
    const uint8_t* b;
    if (!v128_const_blob(g, c, &b)) return 0;
    for (int i = 0; i < 16; i++) if (b[i] != 0x00) return 0;
    return 1;
}
static int jav_eq_is_v128_ones(egraph* g, eg_id c) {
    const uint8_t* b;
    if (!v128_const_blob(g, c, &b)) return 0;
    for (int i = 0; i < 16; i++) if (b[i] != 0xff) return 0;
    return 1;
}
/* A one in every lane of the given byte width — imul's identity vector. */
static int jav_eq_is_v128_lane1(egraph* g, eg_id c, int lanebytes) {
    const uint8_t* b;
    if (!v128_const_blob(g, c, &b)) return 0;
    for (int i = 0; i < 16; i++)
        if (b[i] != (i % lanebytes == 0 ? 0x01 : 0x00)) return 0;
    return 1;
}
/* Does the MATCHED class hold a shuffle whose pattern is the identity over
 * operand `which` (0: lanes 0..15, 1: lanes 16..31)? Sound whatever else the
 * class holds: the class's value IS that shuffle's value, which is that
 * operand. */
static int jav_eq_shuffle_is_id(egraph* g, eg_id c, int which) {
    int n = eg_class_nodes(g, c);
    for (int i = 0; i < n; i++) {
        if (eg_class_node_op(g, c, i) != (int)JAV_EQ_OP_I8X16_SHUFFLE) continue;
        int64_t bi = eg_class_node_data(g, c, i);
        if (!g_cur || bi < 0 || (uint32_t)bi >= g_cur->nblobs) continue;
        const uint8_t* b = g_cur->blobs[bi];
        int base = which ? 16 : 0, ok = 1;
        for (int k = 0; k < 16 && ok; k++) if (b[k] != base + k) ok = 0;
        if (ok) return 1;
    }
    return 0;
}

/* The all-zero vector, for the lane-wise self-erasers: isub/ixor at i1 = i2
 * are exactly 0 in every lane, and all-zero BITS spell lane-zero at every
 * lane width at once. Interned through the region's blob table so it keys
 * like any other v128 immediate. */
eg_id jav_eq_zero_v128(egraph* g, eg_id self) {
    (void)self;
    static const uint8_t zeros[16] = {0};
    if (!g_cur) return self;              /* no region context: change nothing */
    int64_t bi = blob_intern(g_cur, zeros);
    if (bi < 0) return self;
    return eg_add(g, (int)JAV_EQ_OP_V128_CONST, bi, NULL, 0);
}
/* Strength reduction's shift distances: log2 of a known power-of-two
 * constant, as the shift op's own distance type (the scalar shifts take
 * their own width; the vector shifts all take an i32). Guarded call sites
 * only — a raced guard falls back to the matched operand, a merge that
 * changes nothing. */
static int ilog2_u64(uint64_t v) { int k = 0; while (v >>= 1) k++; return k; }
eg_id jav_eq_log2_32(egraph* g, eg_id p) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, p);
    if (!f || f->kind != 1) return p;
    return eg_add(g, OP_I32_CONST, ilog2_u64((uint64_t)(uint32_t)f->v), NULL, 0);
}
eg_id jav_eq_log2_64(egraph* g, eg_id p) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, p);
    if (!f || f->kind != 2) return p;
    return eg_add(g, OP_I64_CONST, ilog2_u64((uint64_t)f->v), NULL, 0);
}
eg_id jav_eq_v128_log2(egraph* g, eg_id p) {
    const uint8_t* b;
    if (!v128_const_blob(g, p, &b)) return p;
    uint64_t lane0;
    memcpy(&lane0, b, 8);
    return eg_add(g, OP_I32_CONST, ilog2_u64(lane0 ? lane0 : 1), NULL, 0);
}

/* Power-of-two guards for the strength rules: a known constant ≥ 2 whose
 * bit pattern has one set bit. The vector form additionally demands every
 * lane EQUAL (a splat) so one shift distance serves all lanes. */
static int is_pow2_u64(uint64_t v) { return v >= 2 && (v & (v - 1)) == 0; }
static int jav_eq_is_pow2_32(egraph* g, eg_id c) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 1 && is_pow2_u64((uint64_t)(uint32_t)f->v);
}
static int jav_eq_is_pow2_64(egraph* g, eg_id c) {
    const jav_eq_fact_t* f = (const jav_eq_fact_t*)eg_class_data(g, c);
    return f && f->kind == 2 && is_pow2_u64((uint64_t)f->v);
}
static int jav_eq_is_v128_splat_pow2(egraph* g, eg_id c, int lanebytes) {
    const uint8_t* b;
    if (!v128_const_blob(g, c, &b)) return 0;
    uint64_t lane0 = 0;
    memcpy(&lane0, b, (size_t)lanebytes);
    if (!is_pow2_u64(lane0)) return 0;
    for (int i = lanebytes; i < 16; i += lanebytes)
        if (memcmp(b, b + i, (size_t)lanebytes) != 0) return 0;
    return 1;
}

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
     * pop it stands for is owed). The builder linked it to its
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
    /* The 0xFD vector family: the e-node key is the composite
     * JAV_EQ_OP_FD(sub), disjoint from every byte. v128.const and
     * i8x16.shuffle carry a 16-byte immediate — content-deduped into the
     * region's blob table, index as `data`, so equal vectors share a class
     * and unequal ones cannot collide. A lane op's one-byte immediate rides
     * `data` directly, read by the op's own meta. Anything the fence does
     * not admit falls through to opaque. */
    if (op == 0xFD) {
        bbq_ctx_t ic; bbq_ctx_init(&ic, n->pc + 1, 5);
        uint32_t sub = 0;
        if (bbq_read_uleb128_u32(&ic, &sub) && sub <= 255 && jav_eqsat_pure_fd[sub]) {
            const uint8_t* imm = n->pc + 1 + ic.pos;
            if (sub == 0x0c || sub == 0x0d) {          /* v128.const / shuffle */
                int64_t bi = blob_intern(c, imm);
                if (bi < 0) { c->failed = 1; return 0; }
                return intern_as(c, n, (int)JAV_EQ_OP_FD(sub), bi, kids, nkids);
            }
            const jav_jit_meta_t* fm = jav_jit_meta_sub[0xFD]
                                     ? &jav_jit_meta_sub[0xFD][sub] : NULL;
            if (fm && fm->operand_count == 0)
                return intern_as(c, n, (int)JAV_EQ_OP_FD(sub), 0, kids, nkids);
            if (fm && fm->operand_count == 1 && fm->operands[0].kind == JOP_U8)
                return intern_as(c, n, (int)JAV_EQ_OP_FD(sub), imm[0], kids, nkids);
            /* an immediate shape v1 does not carry: opaque below */
        }
    }
    if (op != 0xFD && jav_eqsat_pure[op])
        return intern_as(c, n, op, 0, kids, nkids);

    /* Unadmitted: opaque by identity — the tree keeps its place. */
    return intern_as(c, n, JAV_EQ_OP_OPAQUE, (int64_t)(uintptr_t)n, NULL, 0);
}

/* ── the rebuild (C3): a differing extraction becomes a tree ─────
 *
 * Three fences, each a refusal that keeps the original (fail closed — the
 * engine never aborts and never guesses), each counted:
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

/* An original subtree of the root being rebuilt whose intern record is
 * exactly (op, data) — how an extraction leaf with no synth spelling (a
 * 16-byte constant, a shuffle pattern) is recovered as the concrete tree it
 * came from. Matching the RECORD is what makes this safe: the record's data
 * is a content-deduped blob index, so an equal record is an equal value. */
static const jav_tnode_t* rb_find_original(ictx_t* c, const jav_tnode_t* n,
                                           int op, int64_t data) {
    if (!n) return NULL;
    const eq_rec_t* rec = rec_of(c, n);
    if (rec && rec->op == op && rec->data == data) return n;
    for (int i = 0; i < n->nkids; i++) {
        const jav_tnode_t* hit = rb_find_original(c, n->kids[i], op, data);
        if (hit) return hit;
    }
    return NULL;
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

static jav_tnode_t* rb_synth(ictx_t* c, uint8_t op, uint32_t sub, int prefixed,
                             int64_t imm, int64_t imm2, int sig,
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
    sr->op = op; sr->prefixed = (uint8_t)prefixed; sr->sub = sub;
    sr->imm = imm; sr->imm2 = imm2;
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
        return rb_synth(c, OP_LOCAL_GET, 0, 0, (int64_t)slot, 0,
                        rb_local_sig(c, slot), NULL, 0);
    }
    if (op == OP_I32_CONST || op == OP_I64_CONST)
        return rb_synth(c, (uint8_t)op, 0, 0, data, 0, jav_opcode_sig[op].sig, NULL, 0);
    if (op <= 255 && jav_eqsat_pure[op]) {
        jav_tnode_t* kids[JAV_SIG_MAX_KIDS];
        int nk = r->nkids[xi];
        if (nk > JAV_SIG_MAX_KIDS) { c->refuse = 1; return NULL; }
        for (int i = 0; i < nk; i++) {
            kids[i] = rb_node(c, r, r->kids[r->kid_off[xi] + i]);
            if (c->refuse) return NULL;
        }
        return rb_synth(c, (uint8_t)op, 0, 0, 0, 0, jav_opcode_sig[op].sig, kids, nk);
    }
    /* A prefixed vector op: spelled through the sub-table's own signature
     * when its immediate fits the record. A 16-byte immediate (v128.const,
     * i8x16.shuffle) has no synth channel — but the extraction usually wants
     * one because a RULE selected that very operand, and the operand exists
     * as an ORIGINAL subtree under the root being rebuilt: recover it by its
     * intern record and splice the original. Only a genuinely synthesized
     * 16-byte immediate (nothing in the old root interned as it) refuses. */
    if (op >= 0x10000) {
        uint32_t sub = (uint32_t)(op & 0xffff);
        if (sub > 255 || !jav_eqsat_pure_fd[sub]) { c->refuse = 1; return NULL; }
        if (sub == 0x0c || sub == 0x0d) {
            /* Prefer splicing the original tree the immediate came from;
             * a genuinely manufactured 16-byte immediate (a rule's zero
             * vector) stamps through the record's two raw-8-byte halves,
             * little-endian exactly as the byte decode feeds the holes. */
            const jav_tnode_t* orig = rb_find_original(c, c->rb_old, op, data);
            if (orig)
                return (jav_tnode_t*)(uintptr_t)rb_original(c, (int64_t)(uintptr_t)orig);
            if (data < 0 || (uint32_t)data >= c->nblobs) { c->refuse = 1; return NULL; }
            const uint8_t* b = c->blobs[data];
            int64_t lo, hi;
            memcpy(&lo, b, 8); memcpy(&hi, b + 8, 8);
            const jav_opcode_sig_t* crow = jav_opcode_sig_sub[0xFD]
                ? &jav_opcode_sig_sub[0xFD][sub] : NULL;
            if (!crow || !crow->present) { c->refuse = 1; return NULL; }
            jav_tnode_t* kids[JAV_SIG_MAX_KIDS];
            int nk = r->nkids[xi];
            if (nk > JAV_SIG_MAX_KIDS) { c->refuse = 1; return NULL; }
            for (int i = 0; i < nk; i++) {
                kids[i] = rb_node(c, r, r->kids[r->kid_off[xi] + i]);
                if (c->refuse) return NULL;
            }
            return rb_synth(c, 0xFD, sub, 1, lo, hi, crow->sig, kids, nk);
        }
        const jav_opcode_sig_t* row = jav_opcode_sig_sub[0xFD]
            ? &jav_opcode_sig_sub[0xFD][sub] : NULL;
        if (!row || !row->present) { c->refuse = 1; return NULL; }
        jav_tnode_t* kids[JAV_SIG_MAX_KIDS];
        int nk = r->nkids[xi];
        if (nk > JAV_SIG_MAX_KIDS) { c->refuse = 1; return NULL; }
        for (int i = 0; i < nk; i++) {
            kids[i] = rb_node(c, r, r->kids[r->kid_off[xi] + i]);
            if (c->refuse) return NULL;
        }
        return rb_synth(c, 0xFD, sub, 1, data, 0, row->sig, kids, nk);
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
    c->nkept = 0; c->refuse = 0; c->rb_old = old;
    jav_tnode_t* nu = rb_node(c, r, r->root);
    if (c->refuse || !nu) return NULL;
    if (!rb_drops_pure(c, old)) return NULL;
    for (int i = 1; i < c->nkept; i++)
        if (c->kept[i - 1]->pc && c->kept[i]->pc
            && c->kept[i - 1]->pc > c->kept[i]->pc) return NULL;
    *dnodes = (int32_t)rb_count(nu) - (int32_t)rb_count(old);
    return nu;
}

/* Extract-and-splice the subtree at *slot when its class is a PURE one whose
 * cheapest spelling differs; otherwise descend into the kids and try theirs.
 *
 * A STATEMENT root — local.set, drop, a store, a call — interns as an opaque
 * atom keyed by its own address, so extraction from ITS class can never
 * differ, however much saturation merged the value classes underneath; the
 * descent is what connects those classes to the splice. Extraction from a
 * pure class returns the whole min-cost TERM, kids included, so a kept pure
 * subtree needs no further descent — if the spelling matched, everything
 * under it was already minimal.
 *
 * Returns 1 if anything under *slot was spliced. A node the intern holds no
 * record for keeps its subtree untouched and counts an identity_fail. */
static int same_term(ictx_t* c, const eg_extract_result* r, int xi,
                     const jav_tnode_t* n);
static int eq_cost(int op, int64_t data, void* user);

static int rw_tree(ictx_t* c, jav_ttree_t* tree, jav_tnode_t** slot) {
    jav_tnode_t* n = *slot;
    const eq_rec_t* rr = rec_of(c, n);
    if (!rr) { g_eq.identity_fails++; return 0; }
    if (rr->op != JAV_EQ_OP_OPAQUE) {
        eg_extract_result ex;
        if (!eg_extract(c->g, rr->id, eq_cost, NULL, &ex)) {
            g_eq.identity_fails++;        /* no finite term: keep the original */
            return 0;
        }
        int changed = 0;
        if (!same_term(c, &ex, ex.root, n)) {
            int32_t dn = 0;
            jav_tnode_t* nu = rb_root(c, &ex, n, &dn);
            if (nu) {
                *slot = nu;
                tree->nnodes = (uint32_t)((int64_t)tree->nnodes + dn);
                changed = 1;
            } else {
                g_eq.rebuild_refusals++;  /* original stands, counted, never guessed */
            }
        }
        eg_extract_free(&ex);
        return changed;
    }
    /* An opaque spine: a carried leaf's nkids is 0 (its kids[0] is the
     * producer link, not a subtree), so this walks only real children. */
    int changed = 0;
    for (uint8_t k = 0; k < n->nkids; k++)
        if (n->kids[k] && rw_tree(c, tree, &n->kids[k])) changed = 1;
    return changed;
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

/* Extraction prices in the ENGINE'S units — Ertl's cycles (§2.6, printed
 * 36: loads, stores, moves and sp updates cost one; dispatches four), the
 * same constants the tiling grammar already prices its rules in. On a
 * copy-and-patch tier every e-node the extraction picks becomes one stencil,
 * hence one dispatch; where its operands live (register or memory) is the
 * COVER's decision made after this pass, so pricing traffic here would
 * count it twice. A size-first model (the JCVM analog's, where bytecode
 * bytes are the artifact) crept in here once and mispriced a v128.const at
 * one byte-unit; encoded size is only the TIEBREAK now, scaled under one
 * dispatch so bytes can never outvote a cycle. An opaque node's price never
 * decides anything — its address-keyed data makes it the sole member of its
 * class. */
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
    /* Sub-dispatch stencil-body weight: an integer multiply's latency
     * exceeds a shift's on every shipping micro-architecture, and a loop
     * body runs millions of times — without this the model calls
     * `x * 2^k` and `x << k` equal and strength reduction can never win. */
    int micro = (op == OP_I32_MUL || op == OP_I64_MUL
                 || op == (int)JAV_EQ_OP_I16X8_MUL
                 || op == (int)JAV_EQ_OP_I32X4_MUL
                 || op == (int)JAV_EQ_OP_I64X2_MUL) ? 2 : 0;
    if (op == OP_I32_CONST || op == OP_I64_CONST) bytes = 1 + sleb_len(data);
    else if (op == JAV_EQ_OP_LOCAL) bytes = 1 + leb_len_u((uint64_t)(uint32_t)data);
    else if (op >= 0x10000) {
        uint32_t sub = (uint32_t)(op & 0xffff);
        bytes = 2 + (sub > 127 ? 1 : 0) + ((sub == 0x0c || sub == 0x0d) ? 16 : 0);
    } else bytes = 1;
    return JAV_COST_DISPATCH * 256 + micro * 32 + bytes;
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
    uint32_t* snaps = NULL;               /* per-root local-version snapshot: the
                                           * splice check compares an extracted
                                           * local against the version live HERE */
    ictx_t c = {0};
    c.g = &g; c.arena = a; c.recs = &recs; c.nlocals = nlocals;
    c.tcx = tcx; c.synth = synth; c.facts = facts;
    g_cur = &c;                    /* the guards' window onto this region's blobs */
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
        g_eq.cap_refusals++;              /* originals stand, counted, never guessed */
        g_cur = NULL;
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
        c.snap = nlocals ? snaps + (size_t)i * nlocals : NULL;
        if (rw_tree(&c, tree, &reg->roots[i])) g_eq.rewritten++;
    }

    g_cur = NULL;
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
