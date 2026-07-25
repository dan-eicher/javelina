/* sir_optimizer.c — Click §4.2 partition-refinement engine, slot
 * bin-packing post-pass, public sir_optimize entry point. Single-
 * file optimizer: value graph (cp_build), partition refinement to
 * fixpoint (cp_resolve/cp_refine), one-shot rewrite (cp_rewrite),
 * slot pack (cp_pack), public facade. */
#include "javelina/compiler/sir_optimizer.h"
#include "javelina/compiler/sir_op_gamma.h"
#include "javelina/compiler/sir_support.h"
#include "javelina/compiler/jbound.h"   /* the bound-arithmetic core */
#include "bbq_vec.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Exact pointer-keyed map (see sir_optimizer.h) ─────────────
 *
 * `node → the index we gave it` — asked millions of times per compile, against keys we
 * minted ourselves. It is one `bbq_hmap` probe.
 *
 * It used to be THREE structures: a 64→32 pointer hash (because bbq_htree's key is 32-bit),
 * a nibble-trie descent (up to 8 DEPENDENT pointer loads — a chain of cache misses that
 * cannot be prefetched), and then a collision chain of the map's own, walked comparing the
 * real pointers, because the truncating hash could collide. Three layers to answer one
 * question, and 20% of the compile. bbq_htree is the right structure for a SPARSE integer
 * key space nobody controls (the memory-cell keys still use it); it is the wrong one for a
 * dense pointer→index lookup. */

static void cp_pmap_init(cp_pmap_t* m) {
    bbq_hmap_init(&m->map, 0);
}

static void cp_pmap_free(cp_pmap_t* m) {
    bbq_hmap_free(&m->map);
}

static void* cp_pmap_get(const cp_pmap_t* m, const void* k) {
    return bbq_hmap_get(&m->map, (uint64_t)(uintptr_t)k);
}

static void cp_pmap_put(cp_pmap_t* m, const void* k, void* v) {
    bbq_hmap_put(&m->map, (uint64_t)(uintptr_t)k, v);
}

/* ── Representation compatibility ────────────────────────────── *
 * Value congruence says two nodes compute the SAME VALUE; it does NOT
 * say their WASM representations agree (an int 5 and a long 5 are
 * congruent facts but live in i32 vs i64 locals). Any rewrite that
 * substitutes one node's EMISSION for another's must also require the
 * same lowered valtype — lat_dt_valtype, the ONE dt→valtype authority
 * — and, for refs, the same interned referent Type (gamma_ref_to_type,
 * the ONE descriptor→Type authority).
 *
 * May `lex` (a pure leaf: LoadLocal/LoadConst/LoadNull/LoadThis) be
 * emitted in place of the leaf-read `e`? */
static bool cp_leaf_substitutable(type_pool_t* pool,
                                  const sir_node_t* e, const sir_node_t* lex) {
    sir_datatype_t edt =
        e->tag == SIR_LOADLOCAL ? e->load_local.data_type :
        e->tag == SIR_LOADCONST ? e->load_const.data_type : SIR_DTREF;
    sir_datatype_t ldt =
        lex->tag == SIR_LOADLOCAL ? lex->load_local.data_type :
        lex->tag == SIR_LOADCONST ? lex->load_const.data_type : SIR_DTREF;
    if (lat_dt_valtype(edt) != lat_dt_valtype(ldt)) return false;
    if (edt != SIR_DTREF) return true;
    /* Ref-for-ref: LoadNull fits any ref use; LoadThis's tile casts to
     * its own class (a subtype of anything the value was stored as);
     * a LoadLocal must carry the identical referent. */
    if (lex->tag == SIR_LOADNULL || lex->tag == SIR_LOADTHIS) return true;
    if (lex->tag != SIR_LOADLOCAL || e->tag != SIR_LOADLOCAL) return false;
    const Type* et = gamma_ref_to_type(e->load_local.ref_type, pool);
    const Type* lt = gamma_ref_to_type(lex->load_local.ref_type, pool);
    return et->kind != TK_BOTTOM && et == lt;
}

/* γ-driven result valtype class of an expression, or -1 when γ can't
 * name it (GT_SEMA / GT_BOTTOM — bail conservative at substitution
 * sites). */
static int cp_expr_result_vtclass(const sir_node_t* e) {
    if (!e || e->tag < 0 || e->tag >= SIR_TAG_COUNT) return -1;
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];
    switch (g->type_kind) {
        case GT_PRIM_DT:    return (int)lat_dt_valtype(g->type_prim_dt(e));
        case GT_PRIM_FIXED: return (int)lat_dt_valtype(g->type_prim_fixed_dt);
        case GT_VIA_INPUT:  return e->tag == SIR_LOADLOCAL
                                 ? (int)lat_dt_valtype(e->load_local.data_type)
                                 : -1;
        case GT_REF: case GT_ARRAY: case GT_NULL:
        case GT_CHECKCAST: case GT_PRIM_ARRAY:
                            return (int)LAT_VT_REF;
        case GT_ARRAY_ELEM: return (int)lat_dt_valtype(e->array_load.data_type);
        default:            return -1;
    }
}

/* Slots are NOT SSA: a LoadLocal leaf may stand in for a congruent
 * value only where its slot STILL holds that value — the copy source
 * may be reassigned between the copy and the use (Hashtable.rehash's
 * `e = old; old = old.next; …e…`). The slot_in dominance proxy is the
 * gate: at the current rewrite point, the slot's reaching def must sit
 * in the same partition as the value being substituted. Non-LoadLocal
 * leaves (consts, null, `this` — never reassigned) pass trivially. */
static bool cp_slot_still_holds(cp_engine_t* eng, const sir_node_t* lex,
                                int want_partition) {
    if (lex->tag != SIR_LOADLOCAL) return true;
    int s = lex->load_local.slot;
    if (!eng->slot_in || eng->rewrite_spine_idx < 0
            || eng->rewrite_spine_idx >= eng->slot_in_rows
            || s < 0 || s >= eng->slot_count) return false;
    int rd = eng->slot_in[eng->rewrite_spine_idx][s];
    return rd >= 0 && rd < eng->vnode_count
        && eng->vnodes[rd]->partition == want_partition;
}

/* ── Value-node allocation ───────────────────────────────────── */

/* Append a zeroed value node to the table, returning its pointer
 * and, via out_idx, its index. The node lives in the arena, so the
 * pointer stays valid across further appends; the bbq_vec only
 * holds pointers. */
/* Cell-key encoding for the per-(class, field) and per-data_type
 * reaching-stores table. High 2 bits: kind (00=field, 01=static,
 * 10=array). Field/static: class_id in bits 16-29, field_idx in
 * bits 0-15. Array: data_type in bits 0-15. */
#define CP_CELL_KIND_FIELD   (0u  << 30)
#define CP_CELL_KIND_STATIC  (1u  << 30)
#define CP_CELL_KIND_ARRAY   (2u  << 30)
#define CP_CELL_KIND_MEMSIZE (3u  << 30)   /* memory.size — ONE cell: read by MemSize,
                                            * written by MemGrow and calls (CP_CELL_ALL) */
#define CP_CELL_KIND_MASK    (3u  << 30)

/* class_id occupies 14 key bits. Beyond 16383 classes the key wraps:
 * distinct classes share a cell, which only over-invalidates (a store
 * kills more loads than it must) — congruence still matches the real
 * class_id on the nodes, so no wrong CSE. Loud, not silent. */
static uint32_t cp_cell_class_bits(int class_id) {
    if ((uint32_t)class_id > 0x3FFFu) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr, "sir_optimizer: class_id %d exceeds the 14-bit "
                    "memory-cell key; cells will over-invalidate (sound, "
                    "less precise)\n", class_id);
        }
    }
    return ((uint32_t)class_id & 0x3FFF) << 16;
}

static uint32_t cp_cell_key_field(int class_id, int field_idx) {
    return CP_CELL_KIND_FIELD
         | cp_cell_class_bits(class_id)
         | ((uint32_t)field_idx & 0xFFFF);
}
static uint32_t cp_cell_key_static(int class_id, int field_idx) {
    return CP_CELL_KIND_STATIC
         | cp_cell_class_bits(class_id)
         | ((uint32_t)field_idx & 0xFFFF);
}
static uint32_t cp_cell_key_array(int data_type) {
    return CP_CELL_KIND_ARRAY | ((uint32_t)data_type & 0xFFFF);
}
static uint32_t cp_cell_key_memsize(void) {
    return CP_CELL_KIND_MEMSIZE;               /* one memory, one size cell */
}

/* The cell that a SIR expression node touches, or 0xFFFFFFFF if the
 * node has no specific cell (non-memory ops). */
static uint32_t cp_cell_key_for_expr(const sir_node_t* e) {
    switch (e->tag) {
        case SIR_GETFIELD:   return cp_cell_key_field(e->get_field.class_id,
                                                       e->get_field.field_idx);
        case SIR_GETSTATIC:  return cp_cell_key_static(e->get_static.class_id,
                                                        e->get_static.field_idx);
        case SIR_ARRAYLOAD:  return cp_cell_key_array((int)e->array_load.data_type);
        /* memory.size is a memory-dependent read: stable between grows, so
         * congruence keyed by its cell's reaching writer is exactly right
         * (this is what lets consecutive Mem bounds guards over one region merge). */
        case SIR_MEMSIZE:    return cp_cell_key_memsize();
        default:             return 0xFFFFFFFFu;
    }
}

/* The cell that a spine node defines, or 0xFFFFFFFE for "all cells"
 * (invokes — conservative wide-write), 0xFFFFFFFF for "no cell". */
#define CP_CELL_ALL  0xFFFFFFFEu
#define CP_CELL_NONE 0xFFFFFFFFu

static uint32_t cp_cell_key_for_spine(const sir_node_t* e) {
    switch (e->tag) {
        case SIR_PUTFIELD:        return cp_cell_key_field(e->put_field.class_id,
                                                            e->put_field.field_idx);
        case SIR_PUTSTATIC:       return cp_cell_key_static(e->put_static.class_id,
                                                              e->put_static.field_idx);
        case SIR_ARRAYSTORE:      return cp_cell_key_array((int)e->array_store.data_type);
        /* array.copy writes the destination's element cells — a writer
         * of its width's array cell, same as ArrayStore. */
        case SIR_ARRAYCOPY:       return cp_cell_key_array((int)e->array_copy.width);
        /* SetHeader writes the synthesized field-0 Class header, which
         * PutField/GetField can never address (sir.asdl §10.8 note) —
         * no GC-field cell. MemStore8 writes linear memory; MemLoad8
         * is never congruent, so no cell is needed there either. */
        case SIR_SETHEADER:       return CP_CELL_NONE;
        /* Linear-memory stores: linear memory is not a GC-field cell, and
         * the Mem loads are never congruent — no cell needed. */
        case SIR_SIMDMEMSTORE:    return CP_CELL_NONE;
        case SIR_SIMDMEMSTORELANE:return CP_CELL_NONE;
        case SIR_MEMSTOREI:       return CP_CELL_NONE;
        case SIR_MEMSTOREL:       return CP_CELL_NONE;
        case SIR_MEMSTOREF:       return CP_CELL_NONE;
        case SIR_MEMSTORED:       return CP_CELL_NONE;
        case SIR_MEMFILL:         return CP_CELL_NONE;
        case SIR_MEMCOPY:         return CP_CELL_NONE;
        case SIR_INVOKEVIRTUAL:   /* fallthrough */
        case SIR_INVOKESPECIAL:   /* fallthrough */
        case SIR_INVOKESTATIC:    /* fallthrough */
        case SIR_INVOKEINTERFACE: return CP_CELL_ALL;
        case SIR_EXPREFFECT: {
            /* Wraps an arbitrary expression for its side effect. If
             * the wrapped value is an invoke, this spine node has
             * its side-effect — wide-write. A wrapped MemGrow writes
             * exactly the memsize cell. Otherwise no memory effect. */
            sir_node_t* v = e->expr_effect.value;
            if (v && (v->tag == SIR_INVOKEVIRTUAL || v->tag == SIR_INVOKESPECIAL
                  ||  v->tag == SIR_INVOKESTATIC  || v->tag == SIR_INVOKEINTERFACE))
                return CP_CELL_ALL;
            if (v && v->tag == SIR_MEMGROW) return cp_cell_key_memsize();
            return CP_CELL_NONE;
        }
        case SIR_STORELOCAL: {
            /* Same unwrap for a value STORED to a local: `x = grow(1)` must
             * advance the memsize cell (and a stored call is a wide-write,
             * classified here for the same reason). */
            sir_node_t* v = e->store_local.value;
            if (v && (v->tag == SIR_INVOKEVIRTUAL || v->tag == SIR_INVOKESPECIAL
                  ||  v->tag == SIR_INVOKESTATIC  || v->tag == SIR_INVOKEINTERFACE))
                return CP_CELL_ALL;
            if (v && v->tag == SIR_MEMGROW) return cp_cell_key_memsize();
            return CP_CELL_NONE;
        }
        default:                  return CP_CELL_NONE;
    }
}

static cp_vnode_t* cp_alloc_vnode(cp_engine_t* eng, int* out_idx) {
    int idx = eng->vnode_count;
    cp_vnode_t* v = (cp_vnode_t*)bbq_arena_alloc(eng->arena, sizeof *v);
    memset(v, 0, sizeof *v);
    v->phi_slot      = -1;
    v->phi_cell      = -1;
    v->parent_spine  = -1;
    v->seed_slot     = -1;
    v->partition     = -1;
    v->part_prev     = -1;
    v->part_next     = -1;
    v->leader        = -1;
    v->follower_next = -1;
    v->follower_prev = -1;
    v->cprop_next    = -1;
    v->cprop_prev    = -1;
    v->in_cprop      = false;
    bbq_vec_push(eng->vnodes, v);
    eng->vnode_count++;
    *out_idx = idx;
    return v;
}

/* An opaque source value — its own congruence class, no inputs. */
static int cp_new_opaque(cp_engine_t* eng) {
    int idx;
    cp_vnode_t* v = cp_alloc_vnode(eng, &idx);
    v->kind = CP_VN_OPAQUE;
    v->op   = CP_OP_OPAQUE;
    return idx;
}

/* The value a node ultimately IS, through copies only (defined below). */
static int cp_ultimate_value(cp_engine_t* eng, int vi);

/* A Refine's IDENTITY is its whole content: the value it reads plus the fact it
 * asserts. Two branches that refine one value with one predicate state the same
 * fact about the same value — spec §8, "a value IS a node … GVN merges congruent
 * nodes globally" — so they must BE one node. Compared field-wise, not by memcmp:
 * the predicate is written with designated initializers, whose padding bytes are
 * unspecified. Floats by their BITS, so ±0.0 stay distinct and a NaN predicate
 * matches itself. */

static bool cp_refine_pred_eq(const cp_vnode_t* v, cp_const_t p,
                              cp_refine_pts_t pts, sir_atype_t atype, int class_id) {
    if (v->kind != CP_VN_REFINE) return false;
    if (v->refine_pts != pts || v->refine_atype != atype || v->refine_class != class_id)
        return false;
    cp_const_t q = v->refine_predicate;
    uint32_t pf, qf; uint64_t pd, qd;
    memcpy(&pf, &p.fvalue, sizeof pf); memcpy(&qf, &q.fvalue, sizeof qf);
    memcpy(&pd, &p.dvalue, sizeof pd); memcpy(&qd, &q.dvalue, sizeof qd);
    return p.state == q.state && p.cwidth == q.cwidth && p.value == q.value
        && p.lvalue == q.lvalue && pf == qf && pd == qd
        && p.lo == q.lo && p.hi == q.hi && p.stride == q.stride
        && p.hi_vn1 == q.hi_vn1 && p.hi_vn_incl == q.hi_vn_incl
        && p.lo_vn1 == q.lo_vn1 && p.lo_vn_incl == q.lo_vn_incl
        && p.ref_kind == q.ref_kind && p.ref_id == q.ref_id;
}

static bool cp_refine_content_eq(const cp_vnode_t* v, int input_vn, cp_const_t p,
                                 cp_refine_pts_t pts, sir_atype_t atype, int class_id) {
    return v->kind == CP_VN_REFINE && v->input_count == 1 && v->inputs[0] == input_vn
        && cp_refine_pred_eq(v, p, pts, atype, class_id);
}

static uint64_t cp_refine_content_hash(int input_vn, cp_const_t p,
                                       cp_refine_pts_t pts, sir_atype_t atype,
                                       int class_id) {
    uint32_t fb; uint64_t db;
    memcpy(&fb, &p.fvalue, sizeof fb);
    memcpy(&db, &p.dvalue, sizeof db);
    uint64_t parts[] = {
        (uint32_t)input_vn, (uint64_t)p.state, (uint64_t)p.cwidth, (uint32_t)p.value,
        (uint64_t)p.lvalue, fb, db, (uint64_t)p.lo, (uint64_t)p.hi, (uint64_t)p.stride,
        (uint32_t)p.hi_vn1, (uint32_t)p.hi_vn_incl,
        (uint32_t)p.lo_vn1, (uint32_t)p.lo_vn_incl,
        (uint64_t)p.ref_kind, (uint64_t)p.ref_id,
        (uint64_t)pts, (uint64_t)atype, (uint32_t)class_id,
    };
    uint64_t h = 1469598103934665603ULL;                  /* FNV-1a, 64-bit */
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        h ^= parts[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* A Refine vnode: takes one input (the value being refined) and
 * holds a static predicate (the per-arm intersection from a Branch
 * Cmp). cp_node_const returns input.constant ⊓ predicate. Used for
 * path-sensitive lattice refinement (PoPA Ch.6) — LoadLocal vnodes
 * in the arm subtree are rewired to read this Refine, so Click §4.7
 * COPY-Follower works with Refine as the Leader.
 *
 * INTERNED on the full content, so a fact is one node however many branches
 * assert it. Sharing is safe because a Refine is immutable once built: the
 * three constructors below are the only writers of the refine_* fields, and
 * pass B's rewiring only ever re-points an EXPR LoadLocal's input, never a
 * Refine's. On a hash collision (verified field-wise on hit) this mints a
 * fresh unshared node — correct, just not canonical, which costs a fold and
 * never soundness. */
static int cp_new_refine_full(cp_engine_t* eng, int input_vn, cp_const_t predicate,
                              cp_refine_pts_t pts, sir_atype_t atype, int class_id) {
    /* §1: COPIES DON'T EXIST. A range refine names a fact about a VALUE, and a
     * LoadLocal chain is not a distinct value — refining `t`'s spilled copy and
     * refining `t` state the same thing, so both must land on the same node.
     * Without this the key is a NODE key and the arm refines (already minted on
     * the ultimate, from cp_cmp_operand_ultimate) come out incongruent with the
     * ones composed over a spilled slot's state — which is exactly how a
     * re-spilled argument loses a proven bound between two guards.
     *
     * NOT for a pts refine. That one is deliberately OUTSIDE value identity — it
     * is a §4.7 COPY Follower so that a pts fact can never move a partition — and
     * its place IN THE CHAIN is what carries the connection-graph edge (spec §6
     * rides the same graph). Re-pointing it at the root skips the node the edge
     * runs through: a caught exception stored into a static stopped reaching
     * GlobalEscape, because the leak was applied to the root instead of to the
     * landing slot's value. Range facts have no such edge to sever. */
    if (pts == CP_REFINE_PTS_NONE) input_vn = cp_ultimate_value(eng, input_vn);
    /* IDEMPOTENCE. Asserting a fact the input already asserts adds nothing: ⊓ is
     * idempotent, so Refine(Refine(x,P),P).constant = (x.c ⊓ P) ⊓ P = Refine(x,P)
     * .constant, and a pts filter applied twice is the same filter. The stacked node
     * would be a DISTINCT value naming an identical fact — §1's "copies don't exist",
     * which is why a trivial φ is subsumed rather than built. Nested guards make this
     * the common case: two adjacent range guards refine len ≥ 0 twice, and without
     * this the inner one's bound stops being congruent with the outer one's. */
    if (input_vn >= 0 && input_vn < eng->vnode_count
            && cp_refine_pred_eq(eng->vnodes[input_vn], predicate, pts, atype, class_id))
        return input_vn;
    uint64_t key = cp_refine_content_hash(input_vn, predicate, pts, atype, class_id);
    void* hit = bbq_hmap_get(&eng->refine_intern, key);
    bool collision = false;
    if (hit) {
        int prev = (int)((uintptr_t)hit - 1);
        if (prev >= 0 && prev < eng->vnode_count
                && cp_refine_content_eq(eng->vnodes[prev], input_vn, predicate,
                                        pts, atype, class_id))
            return prev;
        collision = true;
    }
    int idx;
    cp_vnode_t* v = cp_alloc_vnode(eng, &idx);
    v->kind = CP_VN_REFINE;
    v->op   = CP_OP_REFINE;
    int* in = (int*)bbq_arena_alloc(eng->arena, sizeof(int));
    in[0] = input_vn;
    v->inputs = in;
    v->input_count = 1;
    v->refine_predicate = predicate;
    v->refine_pts       = pts;
    v->refine_atype     = atype;
    v->refine_class     = class_id;
    if (!collision) bbq_hmap_put(&eng->refine_intern, key, (void*)(uintptr_t)(idx + 1));
    return idx;
}

static int cp_new_refine(cp_engine_t* eng, int input_vn, cp_const_t predicate) {
    return cp_new_refine_full(eng, input_vn, predicate, CP_REFINE_PTS_NONE, 0, 0);
}

/* A Refine over a REFERENCE: it narrows the points-to set, not the constant
 * lattice, so its constant predicate is the identity (BOTTOM = "no refinement").
 * Spec §4's nullability lives entirely in pts, so this is all a null test needs. */
static int cp_new_refine_pts(cp_engine_t* eng, int input_vn, cp_refine_pts_t filter) {
    return cp_new_refine_full(eng, input_vn, (cp_const_t){ .state = CP_C_BOTTOM },
                              filter, 0, 0);
}

/* …and the class flavour (spec §2's `br_on_cast`), which carries the type it tested. */
static int cp_new_refine_isa(cp_engine_t* eng, int input_vn, cp_refine_pts_t filter,
                             sir_atype_t atype, int class_id) {
    return cp_new_refine_full(eng, input_vn, (cp_const_t){ .state = CP_C_BOTTOM },
                              filter, atype, class_id);
}

/* ── The sidecar, read ───────────────────────────────────────
 *
 * EVERY structural fact this engine uses comes through these two lookups, out of the
 * ONE table the DDCG recorded (compiler.h's PAYLOAD TABLE). There is deliberately no
 * other source: the optimizer does not walk the SIR to recover structure the frontend
 * already knew — that is a second authority for the same fact, and spec §8 ("Why
 * there is no dominator tree") rules it out. If a lattice needs something the DDCG
 * knows, RECORD IT (one enum value, one payload row, one record_* call in the
 * grammar) — never rediscover it here.
 *
 * These are pre-solve INDEX builders' helpers, not transfer functions: a transfer is
 * O(its inputs) and may not scan a table. Anything a transfer needs is resolved ONCE
 * into a per-node/per-object array (obj_concrete, is_loop_header) by an index pass. */
static const compiler_fact_t* cp_fact_for(const cp_engine_t* eng, int kind,
                                          const sir_node_t* key) {
    for (int i = 0; i < eng->fact_count; i++)
        if (eng->facts[i].kind == kind && eng->facts[i].key == key)
            return &eng->facts[i];
    return NULL;
}


/* ── Spine collection ────────────────────────────────────────── */

static int cp_spine_index(cp_engine_t* eng, const sir_node_t* n) {
    if (!n) return -1;
    void* v = cp_pmap_get(&eng->spine_idx, n);
    return v ? (int)((uintptr_t)v - 1) : -1;
}

/* The engine's spine + its index, off THE collector (sir_collect_spine, sir_support.h —
 * pinned by test_sir §35). This used to have its own DFS; so did cp_pack. Two copies of
 * "follow the continuation edges" with no test between them is what made writing a THIRD
 * cheaper than reusing either, when the summary driver needed a spine. */
static void cp_collect_spine(cp_engine_t* eng, sir_node_t* entry) {
    sir_node_t** list = sir_collect_spine(entry);
    for (int i = 0; i < (int)bbq_vec_len(list); i++) {
        sir_node_t* n = list[i];
        int idx = (int)bbq_vec_len(eng->spine);
        bbq_vec_push(eng->spine, n);
        cp_pmap_put(&eng->spine_idx, n, (void*)(uintptr_t)(idx + 1));
    }
    bbq_vec_free(list);
    eng->spine_count = (int)bbq_vec_len(eng->spine);
}

/* ── Value-node enumeration ──────────────────────────────────── */

/* Enumerate the expression tree rooted at `e`, returning its
 * value-node index. Idempotent — re-enumerating a node returns its
 * existing index, collapsing any shared subtree to one value node. */
static int cp_enum_expr(cp_engine_t* eng, sir_node_t* e) {
    void* found = cp_pmap_get(&eng->expr_idx, e);
    if (found) return (int)((uintptr_t)found - 1);

    int idx;
    cp_vnode_t* v = cp_alloc_vnode(eng, &idx);
    v->kind = CP_VN_EXPR;
    v->op   = (int)e->tag;
    v->expr = e;
    cp_pmap_put(&eng->expr_idx, e,
                     (void*)(uintptr_t)(idx + 1));

    /* A LoadLocal's operand is its reaching definition, not a tree
     * child — its single input is left CP_INPUT_UNRESOLVED for
     * cp_resolve. Other nodes take their tree children as inputs;
     * sir_arity / sir_child also cover the variadic invoke argument
     * lists. */
    if (e->tag == SIR_LOADLOCAL) {
        int* in = (int*)bbq_arena_alloc(eng->arena, sizeof(int));
        in[0] = CP_INPUT_UNRESOLVED;
        v->inputs = in;
        v->input_count = 1;
    } else {
        int n = sir_arity(e);
        /* Memory-reading ops (Click §8.1.1) get an additional input
         * slot for the reaching-store vnode. cp_resolve fills it
         * from the unified state table (slot_in[spine][slot_count +
         * cell]) so two reads of the same cell with the same writer
         * share inputs and the partition-refinement engine
         * collapses them via the existing follower rule. */
        bool needs_mem = (e->tag == SIR_GETFIELD
                       || e->tag == SIR_GETSTATIC
                       || e->tag == SIR_ARRAYLOAD
                       || e->tag == SIR_MEMSIZE);
        int n_total = n + (needs_mem ? 1 : 0);
        int* in = n_total > 0 ? (int*)bbq_arena_alloc(eng->arena,
                                                      (size_t)n_total * sizeof(int))
                              : NULL;
        for (int i = 0; i < n; i++) {
            sir_node_t* child = sir_child(e, i);
            in[i] = child ? cp_enum_expr(eng, child)
                          : CP_INPUT_UNRESOLVED;
        }
        if (needs_mem) in[n] = CP_INPUT_UNRESOLVED;
        v->inputs = in;
        v->input_count = n_total;
    }
    return idx;
}

/* Enumerate every expression carried by each spine node. sir_arity /
 * sir_child yield a spine node's data children — a StoreLocal's
 * value, a Branch's condition — while the continuation edges are
 * not children and are walked only by cp_collect_spine. */
static void cp_enumerate(cp_engine_t* eng) {
    for (int i = 0; i < eng->spine_count; i++) {
        sir_node_t* n = eng->spine[i];
        int arity = sir_arity(n);
        for (int j = 0; j < arity; j++) {
            sir_node_t* child = sir_child(n, j);
            if (child) cp_enum_expr(eng, child);
        }
    }
}

/* ── Slot types (the ONE slot dt/referent scanner) ───────────── *
 * First dt seen per slot, plus the interned referent Type for ref
 * slots (NULL until seen; ref_uniq on a conflict). Reads the def side
 * of StoreLocal/Inc/ExceptionEntry and every data child generically.
 * cp_build routes the result into the OPAQUE slot seeds (so the type
 * lattice, not BOTTOM, is what φs meet over); cp_pack routes it into
 * the width pools and the ref-coalescing gate. */
static void cp_slot_note_ref(cp_slot_types_t* st, int s, int sc, const Type* t) {
    if (s < 0 || s >= sc || !t || t->kind == TK_BOTTOM) return;
    if (!st->ref[s])          st->ref[s] = t;
    else if (st->ref[s] != t) st->ref_uniq[s] = true;
}

static void cp_slot_scan_expr(const sir_node_t* e, cp_slot_types_t* st,
                              int sc, type_pool_t* pool) {
    if (!e) return;
    if (e->tag == SIR_LOADLOCAL) {
        int s = e->load_local.slot;
        if (s >= 0 && s < sc && !st->seen[s]) {
            st->dt[s]   = e->load_local.data_type;
            st->seen[s] = true;
        }
        if (e->load_local.data_type == SIR_DTREF)
            cp_slot_note_ref(st, s, sc,
                             gamma_ref_to_type(e->load_local.ref_type, pool));
        return;
    }
    int n = sir_arity(e);
    for (int i = 0; i < n; i++)
        cp_slot_scan_expr(sir_child(e, i), st, sc, pool);
}

static void cp_scan_slot_types(sir_node_t* const* spine, int nn, int sc,
                               cp_slot_types_t* st, type_pool_t* pool,
                               bbq_arena* arena) {
    st->dt       = (sir_datatype_t*)bbq_arena_alloc(arena, (size_t)sc * sizeof(sir_datatype_t));
    st->seen     = (bool*)bbq_arena_alloc(arena, (size_t)sc * sizeof(bool));
    st->ref      = (const Type**)bbq_arena_alloc(arena, (size_t)sc * sizeof(const Type*));
    st->ref_uniq = (bool*)bbq_arena_alloc(arena, (size_t)sc * sizeof(bool));
    memset(st->dt,       0, (size_t)sc * sizeof(sir_datatype_t));
    memset(st->seen,     0, (size_t)sc * sizeof(bool));
    memset(st->ref,      0, (size_t)sc * sizeof(const Type*));
    memset(st->ref_uniq, 0, (size_t)sc * sizeof(bool));
    for (int i = 0; i < nn; i++) {
        sir_node_t* n = spine[i];
        if (!n) continue;
        switch (n->tag) {
            case SIR_STORELOCAL: {
                int slot = n->store_local.slot;
                if (slot >= 0 && slot < sc && !st->seen[slot]) {
                    st->dt[slot]   = n->store_local.data_type;
                    st->seen[slot] = true;
                }
                if (n->store_local.data_type == SIR_DTREF)
                    cp_slot_note_ref(st, slot, sc,
                        gamma_ref_to_type(n->store_local.ref_type, pool));
                break;
            }
            case SIR_INC: {
                int slot = n->inc.slot;
                if (slot >= 0 && slot < sc && !st->seen[slot]) {
                    st->dt[slot]   = n->inc.data_type;
                    st->seen[slot] = true;
                }
                break;
            }
            case SIR_EXCEPTIONENTRY: {
                int slot = n->exception_entry.local_slot;
                if (slot >= 0 && slot < sc && !st->seen[slot]) {
                    st->dt[slot]   = SIR_DTREF;
                    st->seen[slot] = true;
                }
                if (n->exception_entry.catch_class_id >= 0)
                    cp_slot_note_ref(st, slot, sc, type_make_ref(pool,
                        n->exception_entry.catch_class_id));
                break;
            }
            default: break;
        }
        int k = sir_arity(n);
        for (int j = 0; j < k; j++)
            cp_slot_scan_expr(sir_child(n, j), st, sc, pool);
    }
}

/* The lattice type an untyped slot value (an OPAQUE seed) enters the
 * fixpoint with: prim(width) for a primitive slot, the interned
 * referent for a uniformly-described ref slot, NULL (→ BOTTOM) when
 * the scan saw nothing usable. */
static const Type* cp_slot_seed_type(cp_engine_t* eng, int s) {
    cp_slot_types_t* st = &eng->slot_types;
    if (s < 0 || s >= eng->slot_count || !st->seen || !st->seen[s]) return NULL;
    if (st->dt[s] == SIR_DTREF)
        return (st->ref[s] && !st->ref_uniq[s]) ? st->ref[s] : NULL;
    return type_make_prim(&eng->pool, st->dt[s]);
}

/* ── Reaching definitions and φ nodes ────────────────────────── */

/* Reaching-definition scratch, valid only for one cp_resolve call.
 * Predecessors are held as a compressed reverse-edge array:
 * pred_list[pred_off[N] .. pred_off[N] + pred_cnt[N]). */
/* State index space unifies slots and memory cells (Click §8.1.1 on
 * the CPS spine): indices 0..slot_count-1 are locals; indices
 * slot_count..slot_count+mem_cell_count-1 are memory cells. One
 * reaching-defs pass populates both — predecessor / merge / φ
 * infrastructure is shared; only the per-spine `define` differs. */
typedef struct {
    int*  pred_off;
    int*  pred_cnt;
    int*  pred_list;
    int*  def_slot;         /* slot defined by spine node, or -1 */
    int*  def_vnode;        /* value defining that slot */
    int*  def_mem_cell;     /* mem cell defined (cp_cell_idx), CP_CELL_ALL for wide-writes, -1 none */
    int*  def_mem_vnode;    /* opaque writer vnode for the memory effect */
    int** def_mem_wide;     /* wide writer (invoke): per cell, its OWN killed version (-1 =
                             * immutable, not killed). NULL for a non-wide node. */
    const bool* cell_immutable;  /* per cell: no code can write it after the allocation */
    bool* is_merge;
    int** phi_of;           /* per merge: state -> φ value-node index */
    int** in_state;          /* per spine node: state -> entry value-node */
    bool* in_done;
    int*  seed;             /* state -> the method's initial value for it */
    int   entry_idx;
    int   state_count;      /* slot_count + mem_cell_count */
} cp_rd_t;

/* The value-node on exit from spine node `pred` for state `s`:
 * - s < slot_count: slot reaching-def — its slot store value or entry.
 * - s >= slot_count: cell reaching-def — its memory writer or entry.
 *   Wide-writers (invokes) define every cell; their writer opaque
 *   shadows in_state for any cell at that spine point.
 *
 * …EXCEPT an IMMUTABLE cell, which a wide write does not kill. Spec §1: a store reaches a
 * load iff they touch the same `O.f` and no KILLING STORE intervenes — and no store to
 * this cell can intervene, because no code can write it. The array overlay's backing-store
 * field is written once, at the allocation, and is unnameable from Java (§10.7 gives an
 * array only `length`), so no callee can touch it. Letting an invoke shadow it was simply
 * a wrong kill: it made a reference array's own backing store unknown after ANY call, so
 * `pts` lost the array object and `a.length` lost its value across a call — which is
 * exactly what the range and type consumers need. lat_is_array_data_cell (the type
 * lattice, §10: consulted, never duplicated) is the one place that says which cells these
 * are. */
static int cp_out_state(const cp_rd_t* rd, int pred, int s, int slot_count) {
    if (s < slot_count) {
        if (rd->def_slot[pred] == s) return rd->def_vnode[pred];
        return rd->in_state[pred][s];
    }
    int cell = s - slot_count;
    int dc = rd->def_mem_cell[pred];
    if (dc == cell) return rd->def_mem_vnode[pred];         /* a real store to this cell */
    if (dc == (int)CP_CELL_ALL
            && !(rd->cell_immutable && rd->cell_immutable[cell])) {
        /* A call kills every MUTABLE cell — but each under ITS OWN name, so that the killed
         * version can keep the rows of the objects the callee cannot reach (§7's bottom
         * graph: it can only touch what it was handed). */
        int w = rd->def_mem_wide[pred] ? rd->def_mem_wide[pred][cell] : -1;
        if (w >= 0) return w;
    }
    return rd->in_state[pred][s];
}

/* The state an EXCEPTING node hands its region's handler — the φ contributor on an
 * EXCEPTIONAL edge (spec §1). It is NOT cp_out_state, and the difference is the whole
 * correctness of the thing.
 *
 * JLS §11.3.1, exceptions are precise: *"all effects of the statements executed and
 * expressions evaluated BEFORE the point from which the exception is thrown must appear
 * to have taken place. No expressions, statements, or parts thereof that occur AFTER the
 * point from which the exception is thrown may appear to have been evaluated."* So:
 *
 *   SLOTS — the value BEFORE this node's own def commits, i.e. `in_state`. `x = f();`
 *     that throws inside f does NOT assign x: the handler must see x's PRIOR value.
 *     Handing it `out_state` would give it f's result — a value that was never stored,
 *     from a call that never returned. That is precisely the E.f miscompile.
 *
 *   CELLS — the value AFTER, i.e. `out_state` (the wide kill). A call that throws may
 *     have written the heap before it threw, and §11.3.1 says those effects DID take
 *     place. Handing the handler the pre-call heap would be UNSOUND — it would let a load
 *     in the catch block forward a store the callee has already clobbered.
 *
 * The asymmetry is not a heuristic: the node's own slot-def is an effect that occurs AT
 * the excepting point (so it did not happen), while a callee's heap writes occur BEFORE
 * it (so they did). */
static int cp_except_state(const cp_rd_t* rd, int pred, int s, int slot_count) {
    if (s < slot_count) return rd->in_state[pred][s];
    return cp_out_state(rd, pred, s, slot_count);
}

/* Compute in_state for `start` and every node on the predecessor
 * chain back to the nearest merge or the entry. Iterative: a merge
 * breaks every spine cycle, so the single-predecessor chain from a
 * non-merge node always reaches a base without looping. */
static void cp_ensure_in(cp_engine_t* eng, cp_rd_t* rd, int start) {
    (void)eng;
    int* chain = NULL;
    int cur = start;
    while (!rd->in_done[cur] && !rd->is_merge[cur]
           && cur != rd->entry_idx) {
        bbq_vec_push(chain, cur);
        cur = rd->pred_list[rd->pred_off[cur]];   /* the single pred */
    }
    if (!rd->in_done[cur]) {
        /* A merge takes the φ values; the non-merge entry takes the
         * seeds. Iterates the unified state space (slots + cells). */
        for (int s = 0; s < rd->state_count; s++)
            rd->in_state[cur][s] = rd->is_merge[cur] ? rd->phi_of[cur][s]
                                                     : rd->seed[s];
        rd->in_done[cur] = true;
    }
    for (int k = (int)bbq_vec_len(chain) - 1; k >= 0; k--) {
        int node = chain[k];
        int pred = rd->pred_list[rd->pred_off[node]];
        for (int s = 0; s < rd->state_count; s++)
            rd->in_state[node][s] = cp_out_state(rd, pred, s, eng->slot_count);
        rd->in_done[node] = true;
    }
    bbq_vec_free(chain);
}

/* Fill operand inputs from reaching state: LoadLocal reads its slot,
 * GetField / GetStatic / ArrayLoad read their memory cell. Both
 * inputs come from the same in_state table — slot indices for slots,
 * slot_count+cell for memory cells. Also records the consuming spine
 * node on each LoadLocal so cp_node_const can apply path-sensitive
 * refinements from enclosing Branch conds. */
static void cp_resolve_loads(cp_engine_t* eng, const int* in_state,
                             int spine_idx, sir_node_t* e) {
    if (!e) return;
    if (e->tag == SIR_LOADLOCAL) {
        int s = e->load_local.slot;
        if (s >= 0 && s < eng->slot_count) {
            void* f = cp_pmap_get(&eng->expr_idx, e);
            if (f) {
                int vi = (int)((uintptr_t)f - 1);
                eng->vnodes[vi]->inputs[0]    = in_state[s];
                eng->vnodes[vi]->parent_spine = spine_idx;
            }
        }
        return;
    }
    if (e->tag == SIR_GETFIELD || e->tag == SIR_GETSTATIC
            || e->tag == SIR_ARRAYLOAD || e->tag == SIR_MEMSIZE) {
        uint32_t key = cp_cell_key_for_expr(e);
        if (key != CP_CELL_NONE) {
            void* cf = bbq_htree_search(eng->mem_cell_idx, (uint32_t)(key + 1));
            int cell = cf ? (int)((uintptr_t)cf - 1) : -1;
            void* g = cp_pmap_get(&eng->expr_idx, e);
            if (g && cell >= 0) {
                int vi = (int)((uintptr_t)g - 1);
                cp_vnode_t* v = eng->vnodes[vi];
                int mem_slot = v->input_count - 1;
                v->inputs[mem_slot] = in_state[eng->slot_count + cell];
            }
        }
        /* fall through to walk children */
    }
    int n = sir_arity(e);
    for (int i = 0; i < n; i++)
        cp_resolve_loads(eng, in_state, spine_idx, sir_child(e, i));
}

/* φ at a merge for state index s. Slot-PHIs (s < slot_count) carry
 * phi_slot=s; cell-PHIs (s >= slot_count) carry phi_cell=s-slot_count
 * with phi_slot=-1. Inputs start CP_INPUT_UNRESOLVED. */
static int cp_new_phi_state(cp_engine_t* eng, sir_node_t* merge, int s,
                             int slot_count, int npreds) {
    int idx;
    cp_vnode_t* v = cp_alloc_vnode(eng, &idx);
    v->kind      = CP_VN_PHI;
    v->op        = CP_OP_PHI;
    v->phi_merge = merge;
    if (s < slot_count) v->phi_slot = s;
    else                v->phi_cell = s - slot_count;
    int* in = (int*)bbq_arena_alloc(eng->arena, (size_t)npreds * sizeof(int));
    int* pp = (int*)bbq_arena_alloc(eng->arena, (size_t)npreds * sizeof(int));
    for (int i = 0; i < npreds; i++) { in[i] = CP_INPUT_UNRESOLVED; pp[i] = -1; }
    v->inputs = in;
    v->phi_pred = pp;
    v->input_count = npreds;
    return idx;
}

/* Compute reaching definitions over the spine, place a φ node at
 * every control-flow merge, and wire each LoadLocal / GetField /
 * GetStatic / ArrayLoad input to the value node that reaches it.
 *
 * Unified state space: slots 0..slot_count-1 carry per-local
 * reaching-defs (StoreLocal / Inc / ExceptionEntry as writers);
 * cells slot_count..slot_count+mem_cell_count-1 carry per-memory-
 * cell reaching-stores (PutField / PutStatic / ArrayStore as
 * writers, Invoke* as wide-writes invalidating every cell). The
 * predecessor / merge / φ infrastructure is shared per Click-
 * Design.md's "spine plus reaching-defs gives placement" — no
 * separate dominator / memory-graph machinery. */
/* ── The EXCEPTIONAL successor edges (spec §1 / JLS §11.3.1) ──
 *
 * Which spine node contains this recorded node? A `Throw` IS a spine node; an `Invoke*`
 * or a `New` is an expression inside one. So scan each spine node's OWN expression tree.
 *
 * THIS IS NOT A CONTROL-FLOW WALK. It reads each node's own operands — the same thing
 * cp_resolve_loads and cp_enum_cells_in_expr already do — and never follows a successor.
 * The structure (which nodes except, and into which region) is READ from the recorded
 * rows; only the expr→spine containment is resolved here, and that is a property of the
 * node itself. */
/* The exception continuation of this spine node, read off THE GRAPH ITSELF (spec §1):
 * a Throw carries `exc` on the node; an Invoke/New carries it on the expression inside
 * the node's tree — the DDCG stamped it when the try rule closed its handler chain.
 * Walks the node's own operands, never a successor; reads only node fields — no facts,
 * no index, nothing derived. */
static sir_node_t* cp_tree_exc(sir_node_t* e) {
    if (!e) return NULL;
    if (e->exc) return e->exc;
    int n = sir_arity(e);
    for (int i = 0; i < n; i++) {
        sir_node_t* c = cp_tree_exc(sir_child(e, i));
        if (c) return c;
    }
    return NULL;
}

/* THE exceptional-edge join, and the ONLY place it is computed.
 *
 * Pair the DDCG's EXCEPT_REGION rows (this node can throw, into region R) with its
 * TRY_REGION rows (region R's handlers) on the region id. Nothing is derived: the
 * frontend said which nodes except — it minted them under ρ — and which handlers a
 * region has. This only pairs the two.
 *
 * Written against a NODE LIST rather than the engine, because cp_pack runs after the
 * engine is freed and re-collects its own list from the post-rewrite spine. Both callers
 * therefore read the SAME recorded rows and cannot end up with different CFGs.
 *
 * ONE pass over each node's own tree, reading only the `exc` FIELDS the DDCG stamped —
 * no facts, no index, nothing derived. (Two earlier cuts of this died in review: a
 * whole-graph expr→spine "owner map", then a fact-row join — each a second authority
 * for edges the graph itself now carries.) Shared verbatim by the engine and cp_pack
 * (which re-collects the post-rewrite spine after the engine is freed), so the two
 * CANNOT disagree: both read the same graph. */
static void cp_hoist_exc_edges(sir_node_t** nodes, int nn,
                               bbq_arena* arena, int*** out_exc) {
    int** exc = (int**)bbq_arena_alloc(arena, (size_t)(nn > 0 ? nn : 1) * sizeof(int*));
    for (int i = 0; i < nn; i++) exc[i] = NULL;
    *out_exc = exc;

    /* The handlers, found by TAG in the same pass — an ExceptionEntry IS a spine node. */
    typedef struct { const sir_node_t* h; int idx; } handler_pos_t;
    handler_pos_t* hp = NULL;
    for (int i = 0; i < nn; i++)
        if (nodes[i]->tag == SIR_EXCEPTIONENTRY) {
            handler_pos_t e = { nodes[i], i };
            bbq_vec_push(hp, e);
        }

    for (int i = 0; i < nn; i++) {
        sir_node_t* chain = cp_tree_exc(nodes[i]);
        if (!chain) continue;
        /* Expand the region's handler set from the TryRegion chain's own `.handler`
         * fields (outermost-first, ends past the chain) — a node's own structure,
         * bounded by the region's handler count. Not a traversal. */
        for (sir_node_t* tr = chain; tr && tr->tag == SIR_TRYREGION;
             tr = tr->try_region.next) {
            const sir_node_t* h = tr->try_region.handler;
            for (int k = 0; k < (int)bbq_vec_len(hp); k++)
                if (hp[k].h == h) {
                    bool dup = false;
                    for (int d = 0; d < (int)bbq_vec_len(exc[i]); d++)
                        if (exc[i][d] == hp[k].idx) { dup = true; break; }
                    if (!dup) bbq_vec_push(exc[i], hp[k].idx);
                    break;
                }
        }
    }
    bbq_vec_free(hp);
}

static void cp_index_except_edges(cp_engine_t* eng) {
    eng->exc_succ_rows = eng->spine_count;
    cp_hoist_exc_edges(eng->spine, eng->spine_count, eng->arena, &eng->exc_succ);
}

/* The exceptional successors of spine node i (NULL/0 when it cannot throw into a handler
 * of this method). THE one accessor — every CFG consumer goes through it, so none of them
 * can end up with a different idea of the control-flow graph. */
static int cp_exc_succ_count(const cp_engine_t* eng, int i) {
    if (!eng->exc_succ || i < 0 || i >= eng->exc_succ_rows) return 0;
    return (int)bbq_vec_len(eng->exc_succ[i]);
}

static int cp_exc_succ(const cp_engine_t* eng, int i, int k) {
    return eng->exc_succ[i][k];
}

/* Is the edge pred → succ an EXCEPTIONAL one? The φ contributor for such an edge takes a
 * different state than a normal edge (see cp_except_state). */
static bool cp_is_exc_edge(const cp_engine_t* eng, int pred, int succ) {
    for (int k = 0; k < cp_exc_succ_count(eng, pred); k++)
        if (cp_exc_succ(eng, pred, k) == succ) return true;
    return false;
}

static void cnt_add_exc(const cp_engine_t* eng, int n, int* cnt) {
    for (int k = 0; k < cp_exc_succ_count(eng, n); k++)
        cnt[cp_exc_succ(eng, n, k)]++;
}

/* Build the predecessor CSR over the CURRENT spine: the predecessors of
 * spine[i] are pred_list[pred_off[i] .. pred_off[i]+pred_cnt[i]). Arena-
 * allocated, sized to eng->spine_count at call time. The spine grows when
 * CSE splices lifted stores, so this is a builder, not a cached table —
 * each caller (cp_resolve, cp_compute_liveness, cp_compute_loop_depth)
 * builds against the spine as it stands then.
 *
 * INCLUDES THE EXCEPTIONAL EDGES (spec §1): a handler is a successor of every excepting
 * node in its region, so it has >1 predecessor and becomes a MERGE. Every CFG consumer
 * builds through here, so they cannot disagree about the graph. */
static void cp_build_pred_csr(cp_engine_t* eng, int** pred_off,
                              int** pred_cnt, int** pred_list) {
    bbq_arena* a = eng->arena;
    int sn = eng->spine_count;
    int* off = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    int* cnt = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    memset(cnt, 0, (size_t)sn * sizeof(int));
    for (int n = 0; n < sn; n++) {
        int sct = sir_succ_count(eng->spine[n]);
        for (int i = 0; i < sct; i++) {
            int si = cp_spine_index(eng, sir_succ(eng->spine[n], i));
            if (si >= 0) cnt[si]++;
        }
        cnt_add_exc(eng, n, cnt);                  /* the handler edges */
    }
    int total = 0;
    for (int n = 0; n < sn; n++) { off[n] = total; total += cnt[n]; }
    int* list = (int*)bbq_arena_alloc(a, (size_t)(total > 0 ? total : 1) * sizeof(int));
    int* cursor = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    memset(cursor, 0, (size_t)sn * sizeof(int));
    for (int n = 0; n < sn; n++) {
        int sct = sir_succ_count(eng->spine[n]);
        for (int i = 0; i < sct; i++) {
            int si = cp_spine_index(eng, sir_succ(eng->spine[n], i));
            if (si >= 0) list[off[si] + cursor[si]++] = n;
        }
        for (int k = 0; k < cp_exc_succ_count(eng, n); k++) {
            int hi = cp_exc_succ(eng, n, k);
            list[off[hi] + cursor[hi]++] = n;
        }
    }
    *pred_off = off; *pred_cnt = cnt; *pred_list = list;
}

/* Subsume the copies — spec §1.
 *
 * The lattices are specified over a graph in which "plain copies DON'T EXIST
 * (subsumed) — a merge is a φ node". We build the reaching-def overlay over
 * SLOTS, not SSA values, so the natural construction puts a φ at every merge for
 * every slot and every cell, whether or not anything actually merges there. Most
 * of those φs are copies: every predecessor carries the same value in. A loop
 * gets one for every slot the loop does NOT write.
 *
 * Left in, they are not merely noise — they are WRONG in the sense that matters:
 * a φ is a distinct node in a distinct congruence partition, so a loop-invariant
 * value read inside the loop is not congruent to the same value read outside it,
 * and no expression over it can be CSEd or compared across the loop boundary.
 * That is what an `a.length` bound in a loop header runs into.
 *
 * So remove them here, at construction, as the spec says — not by teaching every
 * consumer to see through them. A φ is TRIVIAL when its contributors, ignoring
 * the ones that are the φ itself, are all one value; it then IS that value.
 * Removing one can make its φ users trivial in turn, so it iterates — and that
 * recursion is what handles a CYCLE of φs (a loop header's φ feeding the body's
 * join φ feeding it back), which is the shape a loop-invariant slot actually
 * takes. Braun et al., "Simple and Efficient Construction of SSA" §2.1 — no
 * dominance, no loop finding, no SCC pass: purely local, iterated to a fixpoint.
 *
 * Runs before cp_build_defuse, so a subsumed φ has no def-use edges and takes no
 * part in the partition machinery at all. */
static int cp_subst_of(const int* subst, int v) {
    while (v >= 0 && subst[v] >= 0) v = subst[v];
    return v;
}

static void cp_subsume_trivial_phis(cp_engine_t* eng, cp_rd_t* rd,
                                    int sn, int state_count) {
    bbq_arena* a = eng->arena;
    int vc = eng->vnode_count;
    int* subst = (int*)bbq_arena_alloc(a, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    for (int v = 0; v < vc; v++) subst[v] = -1;

    /* φ → the φs that take it as a contributor, so removing one re-examines
     * exactly those (CSR: count, then fill). */
    int* ucnt = (int*)bbq_arena_alloc(a, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    memset(ucnt, 0, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    int* phis = NULL;
    for (int n = 0; n < sn; n++) {
        if (!rd->is_merge[n]) continue;
        for (int s = 0; s < state_count; s++) {
            int p = rd->phi_of[n][s];
            bbq_vec_push(phis, p);
            cp_vnode_t* pv = eng->vnodes[p];
            for (int i = 0; i < pv->input_count; i++) {
                int in = pv->inputs[i];
                if (in >= 0 && in < vc && eng->vnodes[in]->kind == CP_VN_PHI) ucnt[in]++;
            }
        }
    }
    int* uoff = (int*)bbq_arena_alloc(a, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    int total = 0;
    for (int v = 0; v < vc; v++) { uoff[v] = total; total += ucnt[v]; }
    int* users = (int*)bbq_arena_alloc(a, (size_t)(total > 0 ? total : 1) * sizeof(int));
    int* cur = (int*)bbq_arena_alloc(a, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    memset(cur, 0, (size_t)(vc > 0 ? vc : 1) * sizeof(int));
    for (int k = 0; k < (int)bbq_vec_len(phis); k++) {
        int p = phis[k];
        cp_vnode_t* pv = eng->vnodes[p];
        for (int i = 0; i < pv->input_count; i++) {
            int in = pv->inputs[i];
            if (in >= 0 && in < vc && eng->vnodes[in]->kind == CP_VN_PHI)
                users[uoff[in] + cur[in]++] = p;
        }
    }

    int* work = NULL;
    for (int k = 0; k < (int)bbq_vec_len(phis); k++) bbq_vec_push(work, phis[k]);
    while (bbq_vec_len(work)) {
        int v = work[bbq_vec_len(work) - 1];
        bbq__vec_hdr(work)->len--;
        if (subst[v] >= 0) continue;                  /* already subsumed */
        cp_vnode_t* pv = eng->vnodes[v];
        int same = -1;
        bool trivial = true;
        for (int i = 0; i < pv->input_count && trivial; i++) {
            int u = cp_subst_of(subst, pv->inputs[i]);
            if (u == v) continue;                     /* itself: says nothing */
            if (u < 0) { trivial = false; break; }    /* an operand we can't name */
            if (same < 0)      same = u;
            else if (same != u) trivial = false;      /* a REAL merge — keep it */
        }
        if (!trivial || same < 0) continue;
        subst[v] = same;
        for (int k = uoff[v]; k < uoff[v] + ucnt[v]; k++) bbq_vec_push(work, users[k]);
    }

    /* Rewrite every reference: surviving φs' contributors, and the reaching-def
     * table the loads are about to be wired from (and which the memory side
     * table then reads). Then strip each subsumed φ of its inputs — nothing
     * points at it, and with no inputs it has no def-use edges, so it is inert. */
    for (int k = 0; k < (int)bbq_vec_len(phis); k++) {
        cp_vnode_t* pv = eng->vnodes[phis[k]];
        for (int i = 0; i < pv->input_count; i++)
            pv->inputs[i] = cp_subst_of(subst, pv->inputs[i]);
    }
    for (int n = 0; n < sn; n++)
        for (int s = 0; s < state_count; s++)
            rd->in_state[n][s] = cp_subst_of(subst, rd->in_state[n][s]);
    for (int n = 0; n < sn; n++) {
        if (rd->def_vnode[n] >= 0)     rd->def_vnode[n]     = cp_subst_of(subst, rd->def_vnode[n]);
        if (rd->def_mem_vnode[n] >= 0) rd->def_mem_vnode[n] = cp_subst_of(subst, rd->def_mem_vnode[n]);
    }
    for (int k = 0; k < (int)bbq_vec_len(phis); k++) {
        int p = phis[k];
        if (subst[p] < 0) continue;
        cp_vnode_t* pv = eng->vnodes[p];
        pv->kind        = CP_VN_OPAQUE;
        pv->op          = CP_OP_OPAQUE;
        pv->inputs      = NULL;
        pv->input_count = 0;
        pv->phi_pred    = NULL;
        pv->phi_merge   = NULL;
        pv->phi_slot    = -1;
        pv->phi_cell    = -1;
    }
    bbq_vec_free(phis);
    bbq_vec_free(work);
}

static int cp_vnode_of(cp_engine_t* eng, const sir_node_t* e);

/* Does anything actually throw into this handler? (Its exceptional predecessors ARE the
 * region's excepting points — spec §1's recorded merge.) */
static bool cp_has_exc_pred(const cp_engine_t* eng, const cp_rd_t* rd, int n) {
    for (int k = 0; k < rd->pred_cnt[n]; k++)
        if (cp_is_exc_edge(eng, rd->pred_list[rd->pred_off[n] + k], n)) return true;
    return false;
}

/* The value delivered into a handler's LANDING SLOT along the exceptional edge from spine
 * node `p` — spec §1's "a handler merges every excepting point of its region", read as a
 * value and not merely as control.
 *
 * THE VALUE. A THROW delivers the object it throws — the value-flow edge whose absence
 * severed the caught reference from the thrown one. Anything else (an excepting CALL, an
 * allocation's OOM) delivers an object the callee or the runtime minted and we cannot
 * name: OPAQUE.
 *
 * THE FILTER — and it is not optional. An excepting node's exceptional edges reach EVERY
 * handler of its region, but JLS §11.3 dispatches to the FIRST clause whose parameter type
 * covers the thrown class. Deliver the raw value to all of them and a `catch (E e)` AND
 * the region's re-throwing catch-all both receive it — so an exception that was genuinely
 * caught still "escapes" out of the catch-all's rethrow. The delivered value is therefore
 * pts(thrown) FILTERED by what this handler actually catches, which is spec §2's cast
 * filter — "pts(v) = { O ∈ pts(u) | classOf(O) ≤ τ }" — applied per-edge as §4 describes.
 *
 *   a TYPED clause (catch_class_id ≥ 0) ⟹ ISA its declared type.
 *   the CATCH-ALL (catch_class_id -1: no declared type — it is the propagation path, JLS
 *     §11.3) ⟹ what NO typed clause of this region matched: NOT_ISA of each of them.
 *
 * The filters are tri-state (cp_obj_isa): only a PROVEN answer drops an object, so an
 * object of unknown class reaches every handler. Fail-closed in both directions.
 *
 * Reads the predecessor NODE and the recorded exceptional edges (cp_index_except_edges).
 * No walk, no fact lookup, no second subtype rule. */
static int cp_landed_value(cp_engine_t* eng, int p, int hidx, const sir_node_t* handler) {
    int cc = handler->exception_entry.catch_class_id;

    sir_node_t* pn = eng->spine[p];
    int t = (pn->tag == SIR_THROW) ? cp_vnode_of(eng, pn->throw_.ref) : -1;
    if (t < 0) {
        t = cp_new_opaque(eng);
        if (cc >= 0)
            eng->vnodes[t]->opaque_type = type_make_ref(&eng->pool, cc);
    }

    if (cc >= 0)                                  /* a typed clause: what it covers */
        return cp_new_refine_isa(eng, t, CP_REFINE_PTS_ISA, SIR_ATCLASS, cc);

    /* The catch-all: what no typed clause of this region caught. */
    for (int k = 0; k < cp_exc_succ_count(eng, p); k++) {
        int h2 = cp_exc_succ(eng, p, k);
        if (h2 == hidx || h2 < 0 || h2 >= eng->spine_count) continue;
        const sir_node_t* hn = eng->spine[h2];
        if (hn->tag != SIR_EXCEPTIONENTRY) continue;
        int c2 = hn->exception_entry.catch_class_id;
        if (c2 < 0) continue;                     /* another catch-all: catches nothing */
        t = cp_new_refine_isa(eng, t, CP_REFINE_PTS_NOT_ISA, SIR_ATCLASS, c2);
    }
    return t;
}

static void cp_resolve(cp_engine_t* eng) {
    int sn = eng->spine_count;
    int sc = eng->slot_count;
    int mc = eng->mem_cell_count;
    int state_count = sc + mc;
    if (sn == 0) return;
    bbq_arena* a = eng->arena;

    cp_rd_t rd;
    memset(&rd, 0, sizeof rd);
    rd.entry_idx  = cp_spine_index(eng, eng->method->entry);
    rd.state_count = state_count;

    /* Predecessors, as a compressed reverse-edge array. */
    cp_build_pred_csr(eng, &rd.pred_off, &rd.pred_cnt, &rd.pred_list);

    /* Per-spine writers, both slot and memory. */
    rd.def_slot      = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    rd.def_vnode     = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    rd.def_mem_cell  = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    rd.def_mem_vnode = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    rd.def_mem_wide  = (int**)bbq_arena_alloc(a, (size_t)sn * sizeof(int*));
    rd.cell_immutable = eng->cell_immutable;   /* a call does not kill these (cp_out_state) */
    for (int n = 0; n < sn; n++) {
        rd.def_slot[n]      = -1;
        rd.def_vnode[n]     = CP_INPUT_UNRESOLVED;
        rd.def_mem_cell[n]  = -1;
        rd.def_mem_vnode[n] = CP_INPUT_UNRESOLVED;
        rd.def_mem_wide[n]  = NULL;
        sir_node_t* node = eng->spine[n];
        if (node->tag == SIR_STORELOCAL) {
            int s = node->store_local.slot;
            void* f = cp_pmap_get(&eng->expr_idx, node->store_local.value);
            if (s >= 0 && s < sc && f) {
                rd.def_slot[n]  = s;
                rd.def_vnode[n] = (int)((uintptr_t)f - 1);
            }
        } else if (node->tag == SIR_INC) {
            int s = node->inc.slot;
            if (s >= 0 && s < sc) {
                rd.def_slot[n]  = s;
                /* §5-D induction: `i = i + delta` is an ANALYSIS TRANSFER, not opaque —
                 * an opaque makes every counter [MIN,MAX]. Kept opaque-KIND (so the
                 * transform never folds it) but carries the input value + delta; the
                 * OPAQUE case in cp_node_const computes range(input) + delta. */
                int idx = cp_new_opaque(eng);
                cp_vnode_t* iv = eng->vnodes[idx];
                iv->opaque_type = type_make_prim(&eng->pool, node->inc.data_type);
                void* f = cp_pmap_get(&eng->expr_idx, node->inc.value);
                if (f) {
                    int* in = (int*)bbq_arena_alloc(a, sizeof(int));
                    in[0] = (int)((uintptr_t)f - 1);
                    iv->inputs = in;
                    iv->input_count = 1;
                    iv->inc_delta = node->inc.delta;
                }
                rd.def_vnode[n] = idx;
            }
        } else if (node->tag == SIR_EXCEPTIONENTRY) {
            /* THE LANDING SLOT IS DEFINED BY THE HANDLER'S φ, NOT BY A DEF HERE.
             *
             * Spec §1: the handler IS a merge — "a handler merges every excepting point of
             * its region" — and the value each exceptional edge carries into the landing
             * slot is the object THROWN on that edge. That φ already exists (the handler
             * has an exceptional predecessor per excepting point, so it is a merge like any
             * other) and its contributors are wired below.
             *
             * Defining the slot HERE with a blanket opaque overrode that φ, so the CAUGHT
             * reference was a different value from the THROWN object: pts could not connect
             * them, and a catch body that leaked the exception (`return e;`, a store to a
             * static) was invisible to the escape lattice. That is §6/JLS §11.3's owed row.
             *
             * FAIL-CLOSED: a handler nothing can throw into has no exceptional predecessor
             * and therefore no φ to define its slot — it is dead code still reachable via
             * the TryRegion's reachability edge. Keep the opaque there. */
            int s = node->exception_entry.local_slot;
            if (s >= 0 && s < sc && !cp_has_exc_pred(eng, &rd, n)) {
                rd.def_slot[n]  = s;
                rd.def_vnode[n] = cp_new_opaque(eng);
                if (node->exception_entry.catch_class_id >= 0)
                    eng->vnodes[rd.def_vnode[n]]->opaque_type =
                        type_make_ref(&eng->pool,
                                      node->exception_entry.catch_class_id);
            }
        }
        /* Memory writer for this spine node, if any. Wide-writes
         * (invokes) record CP_CELL_ALL — cp_out_state shadows every
         * cell with the writer opaque at this point. */
        uint32_t key = cp_cell_key_for_spine(node);
        if (key == CP_CELL_ALL) {
            /* An invoke kills each cell SEPARATELY, not all of them under one name.
             *
             * The kill is not total: §7's bottom graph says a bottom method can only touch
             * what it was HANDED (and what is reachable from that, or from a global), so a
             * NoEscape object's cells survive the call untouched — which is the whole reason
             * §9 schedules escape (stage 4) before summaries. Preserving those rows means a
             * killed version must hold "cell c's row for object O", and a memory vnode holds
             * ONE `Obj ↦ pts` matrix, not `Cell × Obj` — so ONE shared name physically cannot
             * carry it for more than one cell. Hence one killed version per cell, each
             * remembering the version of ITS cell that the call interrupted (mem_prev). */
            rd.def_mem_cell[n]  = (int)CP_CELL_ALL;
            rd.def_mem_vnode[n] = cp_new_opaque(eng);   /* the CP_CELL_ALL marker */
            rd.def_mem_wide[n]  = (int*)bbq_arena_alloc(a,
                                      (size_t)(mc > 0 ? mc : 1) * sizeof(int));
            for (int c = 0; c < mc; c++)
                rd.def_mem_wide[n][c] =
                    (eng->cell_immutable && eng->cell_immutable[c])
                        ? -1                            /* not killed at all — see cp_out_state */
                        : cp_new_opaque(eng);
        } else if (key != CP_CELL_NONE) {
            void* f = bbq_htree_search(eng->mem_cell_idx, (uint32_t)(key + 1));
            if (f) {
                rd.def_mem_cell[n]  = (int)((uintptr_t)f - 1);
                rd.def_mem_vnode[n] = cp_new_opaque(eng);
            }
        }
    }

    /* The method's initial value for each state — opaque per slot
     * (uninitialized local) and per memory cell (the field's
     * pre-method value). */
    rd.seed = (int*)bbq_arena_alloc(a, (size_t)state_count * sizeof(int));
    for (int s = 0; s < state_count; s++) {
        int seed = cp_new_opaque(eng);
        /* Slot seeds enter the fixpoint typed from the lattice (memory
         * cells stay untyped — their reads carry γ types of their own). */
        if (s < sc) {
            eng->vnodes[seed]->opaque_type = cp_slot_seed_type(eng, s);
            eng->vnodes[seed]->seed_slot = s;   /* names this slot's §1 phantom */
        }
        rd.seed[s] = seed;
    }

    /* A node is a merge when its in-state has more than one source:
     * two-plus predecessors, or the entry reached by a back-edge.
     * Each merge gets one φ per state (slot or cell). */
    rd.is_merge = (bool*)bbq_arena_alloc(a, (size_t)sn * sizeof(bool));
    rd.phi_of   = (int**)bbq_arena_alloc(a, (size_t)sn * sizeof(int*));
    memset(rd.phi_of, 0, (size_t)sn * sizeof(int*));
    for (int n = 0; n < sn; n++) {
        bool merge = (n == rd.entry_idx) ? (rd.pred_cnt[n] >= 1)
                                         : (rd.pred_cnt[n] >= 2);
        rd.is_merge[n] = merge;
        if (merge) {
            int npreds = rd.pred_cnt[n] + (n == rd.entry_idx ? 1 : 0);
            int* row = (int*)bbq_arena_alloc(a, (size_t)state_count * sizeof(int));
            for (int s = 0; s < state_count; s++)
                row[s] = cp_new_phi_state(eng, eng->spine[n], s, sc, npreds);
            rd.phi_of[n] = row;
        }
    }

    /* Entry value-node per (spine node, state). */
    rd.in_state = (int**)bbq_arena_alloc(a, (size_t)sn * sizeof(int*));
    rd.in_done  = (bool*)bbq_arena_alloc(a, (size_t)sn * sizeof(bool));
    memset(rd.in_done, 0, (size_t)sn * sizeof(bool));
    for (int n = 0; n < sn; n++)
        rd.in_state[n] = (int*)bbq_arena_alloc(a, (size_t)state_count * sizeof(int));
    for (int n = 0; n < sn; n++) cp_ensure_in(eng, &rd, n);

    /* Each φ contributor is the value the matching predecessor edge
     * carries into the merge; the entry merge's first contributor is
     * the method-start seed. */
    for (int n = 0; n < sn; n++) {
        if (!rd.is_merge[n]) continue;
        sir_node_t* mn = eng->spine[n];
        /* A handler's LANDING SLOT is not an ordinary state: no predecessor "carries" its
         * prior value into the merge — each exceptional edge DELIVERS a freshly thrown
         * object into it (spec §1). Handled per-edge below; -1 elsewhere. */
        int land = (mn->tag == SIR_EXCEPTIONENTRY) ? mn->exception_entry.local_slot : -1;
        for (int s = 0; s < state_count; s++) {
            cp_vnode_t* phi = eng->vnodes[rd.phi_of[n][s]];
            int i = 0;
            if (n == rd.entry_idx) {
                phi->inputs[i]   = rd.seed[s];
                phi->phi_pred[i] = -1;       /* method-start edge */
                i++;
            }
            for (int k = 0; k < rd.pred_cnt[n]; k++) {
                int p = rd.pred_list[rd.pred_off[n] + k];
                bool exc = cp_is_exc_edge(eng, p, n);
                int in;
                if (s == land) {
                    /* The thrown object on an exceptional edge. On the NORMAL edge
                     * (TryRegion → handler) nothing is thrown at all: spec §6 —
                     * "handler REACHABILITY (true) … handler DATAFLOW (false)" — so that
                     * edge delivers NO value here. An unnameable input contributes nothing
                     * to the pts join and BOTTOM to the constant join: fail-closed, and it
                     * keeps the region-entry contents of a fresh catch temp out of the
                     * caught reference's pts. */
                    in = exc ? cp_landed_value(eng, p, n, mn) : CP_INPUT_UNRESOLVED;
                } else {
                    /* An EXCEPTIONAL edge carries a different state than a normal one: the
                     * excepting node's own slot-def never committed (JLS §11.3.1). This is
                     * what makes a handler's φ correct — see cp_except_state. */
                    in = exc ? cp_except_state(&rd, p, s, sc)
                             : cp_out_state(&rd, p, s, sc);
                }
                phi->inputs[i]   = in;
                phi->phi_pred[i] = p;
                i++;
            }
        }
    }

    cp_subsume_trivial_phis(eng, &rd, sn, state_count);

    /* Point every LoadLocal / GetField / GetStatic / ArrayLoad at
     * its reaching definition or reaching store. */
    for (int n = 0; n < sn; n++) {
        sir_node_t* node = eng->spine[n];
        int ar = sir_arity(node);
        for (int j = 0; j < ar; j++)
            cp_resolve_loads(eng, rd.in_state[n], n, sir_child(node, j));
    }

    /* Publish the unified state table. slot_in[i][s] for s < slot_count
     * is the slot reaching def; for s >= slot_count it's the
     * (s - slot_count) memory cell's reaching store. Spine nodes
     * spliced AFTER this point (CSE lifts) have no row. */
    eng->slot_in = rd.in_state;
    eng->slot_in_rows = eng->spine_count;

    /* ── Memory-state side table (lattice A's heap, stage 1b) ──
     * A store's state vnode is an input-less opaque, so the heap transfer
     * cannot read the store's operands or its prior state off the vnode. Record
     * them here, where the reaching state is already in hand. */
    int vc = eng->vnode_count;
    eng->mem_rows = vc;
    eng->mem_kind = (signed char*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(signed char));
    eng->mem_prev = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    eng->mem_obj  = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    eng->mem_val  = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    eng->mem_cell = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    eng->mem_elem = (bool*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(bool));
    for (int v = 0; v < vc; v++) {
        eng->mem_kind[v] = CP_MEM_NONE;
        eng->mem_prev[v] = eng->mem_obj[v] = eng->mem_val[v] = eng->mem_cell[v] = -1;
        eng->mem_elem[v] = false;
    }
    /* Each cell's pre-method contents are unknown → its seed is a SEED. */
    for (int c = 0; c < mc; c++) {
        int sv = rd.seed[sc + c];
        if (sv >= 0 && sv < vc) { eng->mem_kind[sv] = CP_MEM_SEED; eng->mem_cell[sv] = c; }
    }
    for (int n = 0; n < sn; n++) {
        int mv = rd.def_mem_vnode[n];
        if (mv < 0 || mv >= vc) continue;
        sir_node_t* node = eng->spine[n];
        if (rd.def_mem_cell[n] == (int)CP_CELL_ALL) {
            eng->mem_kind[mv] = CP_MEM_WIDE;      /* the CP_CELL_ALL marker itself */
            /* Each cell's OWN killed version: it names cell c, and remembers the version of
             * cell c that this call interrupted. The transfer (cp_update_heap, CP_MEM_KILL)
             * keeps the rows the callee cannot reach and clobbers the rest. */
            if (rd.def_mem_wide[n])
                for (int c = 0; c < mc; c++) {
                    int kv = rd.def_mem_wide[n][c];
                    if (kv < 0 || kv >= vc) continue;
                    eng->mem_kind[kv] = CP_MEM_KILL;
                    eng->mem_cell[kv] = c;
                    eng->mem_prev[kv] = rd.in_state[n][sc + c];
                }
            continue;
        }
        int cell = rd.def_mem_cell[n];
        if (cell < 0) continue;
        sir_node_t* obj = NULL;
        sir_node_t* val = NULL;
        switch (node->tag) {
            case SIR_PUTFIELD:  obj = node->put_field.obj;  val = node->put_field.value;  break;
            case SIR_PUTSTATIC: obj = NULL;                 val = node->put_static.value; break;
            case SIR_ARRAYSTORE:obj = node->array_store.arr;val = node->array_store.value;break;
            default: break;     /* ArrayCopy &c.: a writer we do not model precisely */
        }
        eng->mem_kind[mv] = CP_MEM_STORE;
        eng->mem_cell[mv] = cell;
        eng->mem_elem[mv] = (node->tag == SIR_ARRAYSTORE);   /* element cell: never strong (see .h) */
        eng->mem_prev[mv] = rd.in_state[n][sc + cell];
        if (obj) { void* f = cp_pmap_get(&eng->expr_idx, obj);
                   if (f) eng->mem_obj[mv] = (int)((uintptr_t)f - 1); }
        if (val) { void* f = cp_pmap_get(&eng->expr_idx, val);
                   if (f) eng->mem_val[mv] = (int)((uintptr_t)f - 1); }
        /* A writer we cannot decompose (ArrayCopy) must be treated as a kill of
         * its cell, not as a precise store — fail closed. */
        if (!val) eng->mem_kind[mv] = CP_MEM_WIDE;
    }
    /* Reverse index: a store has no def-use edges (no inputs), so nothing would
     * ever revisit it when what it depends on changes. A store's value is a
     * function of THREE things — its receiver's pts, its stored value's pts, and
     * the memory version REACHING it (mem_prev). All three must be here.
     *
     * `prev` is the one I left out, and it is the fourth instance of the bug class
     * this index exists for: a store whose reaching version fills in later never
     * recomputed, so its own value stayed empty, the cell-φ downstream saw only
     * the seed, and a load of a field that HAD been written read as provably null.
     * A fact reaching a node by a path the def-use graph does not model must be
     * re-armed explicitly — every time, for every such path. */
    /* A CP_MEM_KILL is in here for the SAME reason, and leaving it out was the SEVENTH
     * instance of this bug class: it too has no inputs, and its value is a function of the
     * version this call interrupted (mem_prev) — which fills in later. Unregistered, it was
     * computed once while that version was still empty, kept the empty row it deduced, and was
     * never revisited. (Its other dependency, the ESCAPE state, reaches it by no def-use path
     * either; cp_solve re-arms it on that explicitly.) */
    int* cnt = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    memset(cnt, 0, (size_t)(vc ? vc : 1) * sizeof(int));
    for (int v = 0; v < vc; v++) {
        if (eng->mem_kind[v] != CP_MEM_STORE && eng->mem_kind[v] != CP_MEM_KILL) continue;
        if (eng->mem_obj[v]  >= 0) cnt[eng->mem_obj[v]]++;
        if (eng->mem_val[v]  >= 0) cnt[eng->mem_val[v]]++;
        if (eng->mem_prev[v] >= 0) cnt[eng->mem_prev[v]]++;
    }
    eng->mem_dep_off = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    eng->mem_dep_cnt = cnt;
    int total = 0;
    for (int v = 0; v < vc; v++) { eng->mem_dep_off[v] = total; total += cnt[v]; }
    eng->mem_dep_list = (int*)bbq_arena_alloc(a, (size_t)(total ? total : 1) * sizeof(int));
    int* cur = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    memset(cur, 0, (size_t)(vc ? vc : 1) * sizeof(int));
    for (int v = 0; v < vc; v++) {
        if (eng->mem_kind[v] != CP_MEM_STORE && eng->mem_kind[v] != CP_MEM_KILL) continue;
        int o = eng->mem_obj[v], x = eng->mem_val[v], p = eng->mem_prev[v];
        if (o >= 0) eng->mem_dep_list[eng->mem_dep_off[o] + cur[o]++] = v;
        if (x >= 0) eng->mem_dep_list[eng->mem_dep_off[x] + cur[x]++] = v;
        if (p >= 0) eng->mem_dep_list[eng->mem_dep_off[p] + cur[p]++] = v;
    }
}

/* ── Path-sensitive refinement (PoPA Ch.6 Condition Propagation) ─ *
 *
 * A refinement is a PER-EDGE FACT (spec §4: "carried on the SIR edge, no dominance").
 * PHASE R — a linear scan of spine[] for SIR_BRANCH — parses each branch's condition
 * ONCE into per-branch edge Refines: `(rt, rf, underlying)`, per-edge data stored at
 * its source node, the same nature as a merge's φs. PASS B then re-derives the slot
 * states with the per-edge rule: a value CARRIED across a refining branch's edge
 * becomes the edge's Refine vnode; a value the predecessor RE-DEFINED does not (a def
 * kills refinement). Every LoadLocal is rewired to the derived state — a no-op where
 * nothing refined (same vnode), Click §4.7 COPY-Follower of the Refine where it did.
 *
 * At the post-arm merge the pass-B base row is the pass-A (unrefined) state, so
 * refinement drops at joins — the φ meets unrefined contributors, exactly the old
 * semantics. There is NO arm marking, NO program-point table, and NO post-hoc
 * ultimate-matched rewiring pass: the set of nodes that used to be "marked" is
 * precisely the single-pred chain region below the edge, which is what the chain
 * fill visits by construction. (The old cp_mark_arm_subtree DFS'd successors from
 * the arm entry stopping at pred_cnt>1 — the set of nodes dominated by the arm edge,
 * i.e. a dominance region computed by traversal. Deleted; spec §8.)
 *
 * COMPOSITION is input-chaining: an inner branch's Refine takes, as its input, the
 * refine flowing at its own edge (set once, on first crossing) — the lattice already
 * evaluates a Refine as input ⊓ predicate, so chains compose in the transfers with
 * no lookup machinery. */

#define CP_BRANCH_ARM_THEN 0
#define CP_BRANCH_ARM_ELSE 1

/* The refinement on the tested operand inside an arm of
 * Branch(Cmp(L, R)). For arm_bit = THEN: L op R was taken. For
 * ELSE: the negation. Returns the cp_const_t predicate stored on
 * the Refine vnode. CP_C_BOTTOM means "no useful refinement"
 * (no Refine vnode is created for this arm). */
static bool cp_cmp_to_jbound(int op, jbound_cmp_t* out) {
    switch (op) {
        case SIR_LT: *out = JB_LT; return true;
        case SIR_LE: *out = JB_LE; return true;
        case SIR_GT: *out = JB_GT; return true;
        case SIR_GE: *out = JB_GE; return true;
        case SIR_EQ: *out = JB_EQ; return true;
        case SIR_NE: *out = JB_NE; return true;
        default:     return false;
    }
}

static cp_const_t cp_branch_predicate_refine(int op, int64_t k, cp_cwidth_t w,
                                              bool tested_on_left, int arm_bit) {
    cp_const_t bot = { .state = CP_C_BOTTOM };  /* "no refinement" sentinel */
    jbound_cmp_t c;
    if (!cp_cmp_to_jbound(op, &c)) return bot;
    /* The tested operand on the RIGHT states the flipped comparison; the else
     * arm states the negation. Both are jbound.h's normalisation, shared with
     * the linter's twin table. */
    if (!tested_on_left)               c = jbound_cmp_flip(c);
    if (arm_bit == CP_BRANCH_ARM_ELSE) c = jbound_cmp_negate(c);
    /* This lattice compares against a CONSTANT, which is the kernel's k_lo ==
     * k_hi case; the interval form is what the linter's twin passes. */
    jbound_t r = jbound_narrow_by_cmp(c, k, k, cp_width_min(w), cp_width_max(w));
    if (!r.ok) return bot;
    if (r.lo == r.hi)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                             .value = (int32_t)r.lo, .lvalue = r.lo };
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = r.lo,
                         .hi = r.hi, .stride = 1 };
}

/* Intersection on the constant lattice (greatest lower bound on
 * value sets). The dual of cp_const_meet: meet is the join
 * (union of value sets), intersect is the meet (intersection).
 * BOTTOM here is the "no refinement" identity (no claim on values).
 * TOP is absorbing — intersecting with "no possible value" is empty.
 * Returns BOTTOM when the lattice has no representation for the
 * intersection (e.g., disjoint singletons, REF vs primitive). */
static bool cp_const_eq(cp_const_t a, cp_const_t b);

cp_const_t cp_const_intersect(cp_const_t a, cp_const_t b) {
    /* BOTTOM here means "no refinement" — identity. */
    if (a.state == CP_C_BOTTOM) return b;
    if (b.state == CP_C_BOTTOM) return a;
    /* TOP means "no value" (optimistic) — preserve. */
    if (a.state == CP_C_TOP) return a;
    if (b.state == CP_C_TOP) return b;
    /* REF vs anything non-REF: the refinement doesn't apply (the
     * lattice doesn't carry ref-vs-range crossings); preserve a. */
    if (a.state == CP_C_REF || b.state == CP_C_REF) return a;
    /* Floats have no range lattice — exact-or-BOTTOM. */
    if (a.cwidth >= CP_W_F32 || b.cwidth >= CP_W_F32)
        return cp_const_eq(a, b) ? a : (cp_const_t){ .state = CP_C_BOTTOM };
    cp_cwidth_t w = a.cwidth;  /* i32/i64 — same width on both (typed operands) */
    /* Normalize to ranges. */
    int64_t a_lo = (a.state == CP_C_KNOWN) ? cp_known_i64(a) : a.lo;
    int64_t a_hi = (a.state == CP_C_KNOWN) ? cp_known_i64(a) : a.hi;
    int64_t b_lo = (b.state == CP_C_KNOWN) ? cp_known_i64(b) : b.lo;
    int64_t b_hi = (b.state == CP_C_KNOWN) ? cp_known_i64(b) : b.hi;
    int64_t lo = a_lo > b_lo ? a_lo : b_lo;
    int64_t hi = a_hi < b_hi ? a_hi : b_hi;
    if (lo > hi) {
        /* Empty intersection — caller's path is unreachable. Return
         * BOTTOM (claim retraction; the engine treats it as "no info"). */
        return (cp_const_t){ .state = CP_C_BOTTOM };
    }
    if (lo == hi)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                             .value = (int32_t)lo, .lvalue = lo };
    /* Stride: take the wider input's stride (more conservative).
     * Strided ranges with mismatched bases mod stride aren't combined
     * here — for first pass, just preserve dense. */
    int64_t a_s = (a.state == CP_C_RANGE) ? a.stride : 1;
    int64_t b_s = (b.state == CP_C_RANGE) ? b.stride : 1;
    int64_t stride = (a_s == b_s) ? a_s : 1;
    /* Carry the symbolic upper bound through: it is an ADDITIONAL fact ("also less than
     * the value of that node"), independent of the interval, so an intersection keeps it.
     * If both sides name a bound and they disagree, keep neither — two unrelated bounds
     * are not intersectable here, and claiming one would be a fact we have not proved.
     *
     * THE INCLUSIVITY TRAVELS WITH THE BOUND. Carrying `hi_vn1` and letting `hi_vn_incl`
     * zero-fill turns `i <= B` into `i < B` — a STRONGER claim than we hold, and one that
     * would delete a bounds guard on `new int[n]` + `i <= n`, where i really does reach
     * len. When both sides name the SAME bound, the STRICT one is the stronger fact and
     * wins. */
    int sym = 0, sym_incl = 0;
    if (a.state == CP_C_RANGE && b.state == CP_C_RANGE && a.hi_vn1 && b.hi_vn1) {
        if (a.hi_vn1 == b.hi_vn1) {
            sym = a.hi_vn1;
            sym_incl = (a.hi_vn_incl && b.hi_vn_incl) ? 1 : 0;   /* strict wins */
        }
        /* different bounds: keep neither */
    } else if (a.state == CP_C_RANGE && a.hi_vn1) {
        sym = a.hi_vn1; sym_incl = a.hi_vn_incl;
    } else if (b.state == CP_C_RANGE && b.hi_vn1) {
        sym = b.hi_vn1; sym_incl = b.hi_vn_incl;
    }
    /* The symbolic LOWER bound rides through the same way. Strict still wins:
     * `x > B` (⟹ x ≥ B+1) is a stronger lower bound than `x >= B`. */
    int lsym = 0, lsym_incl = 0;
    if (a.state == CP_C_RANGE && b.state == CP_C_RANGE && a.lo_vn1 && b.lo_vn1) {
        if (a.lo_vn1 == b.lo_vn1) {
            lsym = a.lo_vn1;
            lsym_incl = (a.lo_vn_incl && b.lo_vn_incl) ? 1 : 0;   /* strict wins */
        }
        /* different bounds: keep neither */
    } else if (a.state == CP_C_RANGE && a.lo_vn1) {
        lsym = a.lo_vn1; lsym_incl = a.lo_vn_incl;
    } else if (b.state == CP_C_RANGE && b.lo_vn1) {
        lsym = b.lo_vn1; lsym_incl = b.lo_vn_incl;
    }
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = lo, .hi = hi,
                         .stride = stride, .hi_vn1 = sym, .hi_vn_incl = sym_incl,
                         .lo_vn1 = lsym, .lo_vn_incl = lsym_incl };
}

static int cp_ultimate_value(cp_engine_t* eng, int vi);
/* …and through Followers and Refines as well: the VALUE this node ultimately IS. */
static int cp_value_leader(cp_engine_t* eng, int vi);

/* The vnode id for expression `e`, or -1 if it has none. */
static int cp_vnode_of(cp_engine_t* eng, const sir_node_t* e) {
    if (!e) return -1;
    void* f = cp_pmap_get(&eng->expr_idx, e);
    return f ? (int)((uintptr_t)f - 1) : -1;
}

/* For a Cmp operand, identify whether it's a constant (after temp-
 * chain following) and yield the constant's value. Returns true if
 * the operand resolves to a constant via direct LoadConst or
 * LoadLocal-of-LoadConst chain. */
static bool cp_const_leaf_value(const sir_node_t* e, int64_t* out_k, cp_cwidth_t* out_w) {
    if (e->tag == SIR_LOADCONST)     { *out_k = e->load_const.value;      *out_w = CP_W_I32; return true; }
    if (e->tag == SIR_LOADLONGCONST) { *out_k = e->load_long_const.value; *out_w = CP_W_I64; return true; }
    return false;
}

static bool cp_cmp_operand_const(cp_engine_t* eng, sir_node_t* opnd,
                                  int64_t* out_k, cp_cwidth_t* out_w) {
    if (!opnd) return false;
    if (cp_const_leaf_value(opnd, out_k, out_w)) return true;
    if (opnd->tag != SIR_LOADLOCAL) return false;
    void* f = cp_pmap_get(&eng->expr_idx, opnd);
    if (!f) return false;
    int vn = (int)((uintptr_t)f - 1);
    if (eng->vnodes[vn]->input_count < 1) return false;
    int ult = cp_ultimate_value(eng, eng->vnodes[vn]->inputs[0]);
    if (ult < 0 || ult >= eng->vnode_count) return false;
    cp_vnode_t* uvn = eng->vnodes[ult];
    if (uvn->kind != CP_VN_EXPR || !uvn->expr) return false;
    return cp_const_leaf_value(uvn->expr, out_k, out_w);
}

/* For a Cmp operand that's a LoadLocal-style read of a non-constant
 * value, return the ultimate vnode index it reads. -1 if the operand
 * isn't of that shape (e.g., it's a constant or an arithmetic
 * expression that the simple chain-walk doesn't follow). */
static int cp_cmp_operand_ultimate(cp_engine_t* eng, sir_node_t* opnd) {
    if (!opnd || opnd->tag != SIR_LOADLOCAL) return -1;
    void* f = cp_pmap_get(&eng->expr_idx, opnd);
    if (!f) return -1;
    int vn = (int)((uintptr_t)f - 1);
    if (eng->vnodes[vn]->input_count < 1) return -1;
    return cp_ultimate_value(eng, eng->vnodes[vn]->inputs[0]);
}

/* The refine-transparent ultimate: like cp_ultimate_value, but a Refine is a window
 * onto its input's value, so the walk continues through it. Pass B's edge-match test
 * needs it because a carried state may already BE a Refine (an enclosing branch's). */
static int cp_ultimate_thru(cp_engine_t* eng, int vi) {
    for (int hops = 0; hops < 256; hops++) {
        if (vi < 0 || vi >= eng->vnode_count) return vi;
        cp_vnode_t* v = eng->vnodes[vi];
        int next;
        if (v->kind == CP_VN_REFINE && v->input_count >= 1 && v->inputs[0] >= 0)
            next = v->inputs[0];
        else if (v->kind == CP_VN_EXPR && v->expr && v->expr->tag == SIR_LOADLOCAL
                 && v->input_count == 1 && v->inputs[0] >= 0)
            next = v->inputs[0];
        else return vi;
        if (next == vi) return vi;
        vi = next;
    }
    return vi;
}

/* Compose an enclosing refinement with an arm refine, PURELY. `r` is the arm's pre-built
 * Refine (input = the branch's raw underlying); `v2` is the value already flowing in,
 * itself a Refine of that same underlying (an enclosing branch's fact). The composite is a
 * Refine carrying r's predicate but reading v2, so its constant intersects BOTH predicates
 * (PoPA Ch.6 refinement stacked; cp_const_intersect carries the symbolic bound through).
 *
 * A pure function of its inputs, minting each composite ONCE, so no existing vnode is ever
 * mutated and re-derivation across sweeps returns the same node (monotone). This is the
 * discipline the removed in-place input-rebasing violated — that rewrote a shared arm
 * Refine's input during the fixpoint (§8/§3.2.1). A name here is a function of its inputs,
 * as it must be — which is now the CONSTRUCTOR's rule (cp_new_refine_full interns on the
 * content), so composing needs no table of its own: (r's content, v2) IS the key. */
static int cp_compose_refine(cp_engine_t* eng, int r, int v2) {
    /* Read the arm's fields into locals BEFORE minting — cp_new_refine may grow (realloc)
     * the vnode array, invalidating the `rv` pointer. */
    cp_vnode_t* rv = eng->vnodes[r];
    cp_const_t      pred  = rv->refine_predicate;
    cp_refine_pts_t pts   = rv->refine_pts;
    sir_atype_t     atype = rv->refine_atype;
    int             cls   = rv->refine_class;
    return (pts != CP_REFINE_PTS_NONE)
        ? cp_new_refine_isa(eng, v2, pts, atype, cls)
        : cp_new_refine(eng, v2, pred);
}

/* ── Pass B's PURE refinement state (Click ch.2 §2.3) ─────────────────────────────
 *
 * The optimistic sweep iterates on STATES, never on graph nodes. §2.3, verbatim: stopping
 * top-down short of the gfp leaves "elements in the set that do not have a corresponding
 * rule. Optimizations using these elements can be incorrect", and only "bottom-up methods
 * can transform as they analyze". Minting a composed Refine VNODE mid-iteration is a
 * transformation based on an intermediate top-down solution — after a retraction the node
 * survives as an unproven element observable by the shared partition/def-use machinery
 * (the recorded E3 miscompile channel). So composition during the sweep is an INTERNED
 * PAIR (arm-refine, state) in a side table; Refine vnodes are materialized only from the
 * CONVERGED rows, which is the bottom-up act §2.3 licenses.
 *
 * A state is: CP_R_TOP (edge not yet reached — the join identity), a vnode id ≥ 0 (the
 * unrefined value, or a phase-R arm refine, each of which always has its rule), -1 (no
 * def), or an encoded pair ≤ CP_R_PAIR0 denoting compose(r, state). */
#define CP_R_TOP   (-2)
#define CP_R_PAIR0 (-3)
typedef struct {
    int*      r;      /* bbq_vec: the arm-refine vnode of pair i    */
    int*      in;     /* bbq_vec: the inner state of pair i         */
    bbq_hmap  memo;   /* (r, state) → pair index + 1 (interning)    */
} cp_pb_pairs_t;

static int cp_pb_pair(cp_pb_pairs_t* P, int r, int s) {
    uint64_t key = ((uint64_t)(uint32_t)r << 32) | (uint32_t)s;
    void* hit = bbq_hmap_get(&P->memo, key);
    if (hit) return CP_R_PAIR0 - ((int)((uintptr_t)hit - 1));
    int idx = (int)bbq_vec_len(P->r);
    bbq_vec_push(P->r, r);
    bbq_vec_push(P->in, s);
    bbq_hmap_put(&P->memo, key, (void*)(uintptr_t)(idx + 1));
    return CP_R_PAIR0 - idx;
}

/* The chain a state denotes: refinement heads, shallowest first, over an ultimate root.
 * Descends pairs, then the vnode's own refine chain — a def-use walk, bounded by nesting. */
static int cp_pb_chain(const cp_engine_t* eng, const cp_pb_pairs_t* P,
                       int s, int* out, int max) {
    int n = 0;
    for (int hops = 0; hops < 256; hops++) {
        if (s <= CP_R_PAIR0) {
            int i = CP_R_PAIR0 - s;
            if (n < max) out[n] = P->r[i];
            n++;
            s = P->in[i];
        } else if (s >= 0 && s < eng->vnode_count
                   && eng->vnodes[s]->kind == CP_VN_REFINE
                   && eng->vnodes[s]->input_count >= 1) {
            if (n < max) out[n] = s;
            n++;
            s = eng->vnodes[s]->inputs[0];
        } else break;
    }
    return n;   /* the root is whatever `s` descended to; callers use cp_pb_root */
}

/* The ultimate root value a state refines (for the edge-match test). */
static int cp_pb_root(cp_engine_t* eng, const cp_pb_pairs_t* P, int s) {
    for (int hops = 0; hops < 256 && s <= CP_R_PAIR0; hops++)
        s = P->in[CP_R_PAIR0 - s];
    return (s >= 0) ? cp_ultimate_thru(eng, s) : s;
}

/* The MEET of two states (Kildall/PoPA §2.3: a join point's value is the meet — the
 * weakest fact implied by EVERY incoming edge). Two chains over one root meet at their
 * longest COMMON SUFFIX: `meet(compose(a,R), compose(b,R)) = R` (both arms carry R), a
 * diamond's `meet(x<5, x>=5)` has an empty suffix and drops to base. TOP is identity.
 * Rebuild: suffix elements that are pair-heads re-intern over the running state; a
 * suffix element that IS a refine vnode whose input equals the running state is that
 * vnode itself. Monotone: chains only shorten, so the sweep descends and terminates. */
#define CP_PB_MAXCHAIN 64
static int cp_pb_meet(cp_engine_t* eng, cp_pb_pairs_t* P, int a, int b, int base) {
    if (a == CP_R_TOP) return b;
    if (b == CP_R_TOP) return a;
    if (a == b) return a;
    if (a == base || b == base) return base;
    int ca[CP_PB_MAXCHAIN], cb[CP_PB_MAXCHAIN];
    int na = cp_pb_chain(eng, P, a, ca, CP_PB_MAXCHAIN);
    int nb = cp_pb_chain(eng, P, b, cb, CP_PB_MAXCHAIN);
    if (na > CP_PB_MAXCHAIN || nb > CP_PB_MAXCHAIN) return base;   /* overdeep: drop */
    if (cp_pb_root(eng, P, a) != cp_pb_root(eng, P, b)) return base;
    int k = 0;                       /* longest common suffix length */
    while (k < na && k < nb && ca[na - 1 - k] == cb[nb - 1 - k]) k++;
    if (k == 0) return base;
    if (k == na) return a;           /* a IS the common suffix */
    if (k == nb) return b;
    /* Rebuild the suffix as a state, deepest first. The deepest run of suffix elements
     * that are REAL refine vnodes chained by inputs is representable as the shallowest
     * such vnode id; every pair-head above it re-interns. */
    int s;
    int deep = na - k;               /* ca[deep..na-1] is the suffix */
    int i = na - 1;
    if (ca[i] >= 0 && ca[i] < eng->vnode_count
            && eng->vnodes[ca[i]]->kind == CP_VN_REFINE) {
        /* extend the vnode run upward while each element is the next's direct input */
        s = ca[i];
        while (i - 1 >= deep && ca[i - 1] >= 0 && ca[i - 1] < eng->vnode_count
               && eng->vnodes[ca[i - 1]]->kind == CP_VN_REFINE
               && eng->vnodes[ca[i - 1]]->input_count >= 1
               && eng->vnodes[ca[i - 1]]->inputs[0] == s) {
            i--;
            s = ca[i];
        }
    } else return base;              /* suffix not rooted in a real refine: no state */
    for (i--; i >= deep; i--)
        s = cp_pb_pair(P, ca[i], s);
    return s;
}

/* PASS B's per-edge rule: the value `v2` (a STATE) carried across the edge `p → cur`. If
 * p is a refining Branch, the crossing state whose ultimate root matches the branch's
 * underlying takes the arm's fact — spec §4's fact ON the edge. When the state already
 * refines that underlying (an enclosing branch), the facts STACK as an interned PAIR —
 * pure set-state per Click ch.2 §2.3, never a node minted mid-iteration. */
static int cp_edge_refined(cp_engine_t* eng, const int* b_rt, const int* b_rf,
                           const int* b_und, cp_pb_pairs_t* P, int p, int cur, int v2) {
    if (b_und[p] < 0 || v2 == CP_R_TOP || v2 == -1) return v2;
    sir_node_t* pn = eng->spine[p];
    /* Both successors on one node: the "edge" proves nothing (neither arm was
     * taken exclusively), so no refinement may be claimed for it. (Inert on the
     * jre — control-measured 07-17; kept because the claim would be wrong.) */
    if (pn->branch.on_true == pn->branch.on_false) return v2;
    int r = (eng->spine[cur] == pn->branch.on_true)  ? b_rt[p]
          : (eng->spine[cur] == pn->branch.on_false) ? b_rf[p] : -1;
    if (r < 0) return v2;
    if (cp_pb_root(eng, P, v2) != b_und[p]) return v2;
    if (v2 != b_und[p]) return cp_pb_pair(P, r, v2);
    return r;
}

/* The verdict fact a crossing edge p → cur contributes: taking a branch's arm
 * decides its condition's VALUE on that edge (1 on the true arm, 0 on the false
 * arm) — sets fact bit 2*ord+bit into `dst`. A pred that is not a fact-bearing
 * branch contributes nothing. */
static void cp_verdict_edge_or(cp_engine_t* eng, uint64_t* dst, int p, int cur) {
    int ord = eng->branch_fact_ord[p];
    if (ord < 0) return;
    sir_node_t* pn = eng->spine[p];
    int bit_idx;
    if      (eng->spine[cur] == pn->branch.on_true)  bit_idx = 2 * ord + 1;
    else if (eng->spine[cur] == pn->branch.on_false) bit_idx = 2 * ord;
    else return;
    dst[bit_idx >> 6] |= (uint64_t)1 << (bit_idx & 63);
}

/* PHASE R + PASS B — see the block comment above CP_BRANCH_ARM_THEN. Phase R parses
 * every Branch's condition once into per-branch edge Refines (a LINEAR scan — not a
 * traversal); pass B re-derives the slot states per edge and rewires the loads. */
static void cp_compute_branch_refinements(cp_engine_t* eng) {
    int sn = eng->spine_count;
    if (sn == 0) return;
    bbq_arena* a = eng->arena;
    /* Per-branch edge facts: the true-edge Refine, the false-edge Refine, and the
     * underlying value the branch tests. Per-EDGE data at its source node — the same
     * nature as a merge's φs. Transient to this pass; only the vnodes outlive it. */
    int* b_rt  = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    int* b_rf  = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    int* b_und = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    for (int i = 0; i < sn; i++) { b_rt[i] = -1; b_rf[i] = -1; b_und[i] = -1; }
    bool any = false;
    /* Condition-verdict fact numbering (channel (a) — see the header's
     * verdict_words comment): every two-successor branch whose condition
     * has a vnode contributes a fact pair. Recorded here, in phase R's
     * single branch scan, so the vnode ids are pass-A wiring. */
    eng->branch_fact_ord = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    eng->branch_cond_vn  = (int*)bbq_arena_alloc(a, (size_t)sn * sizeof(int));
    for (int i = 0; i < sn; i++) {
        eng->branch_fact_ord[i] = -1;
        eng->branch_cond_vn[i]  = -1;
    }
    int nfacts = 0;

    /* ── PHASE R: parse each Branch's condition once. cp_ultimate_value reads the
     * pass-A wiring, so every branch is parsed BEFORE any pass-B rewiring. */
    for (int b = 0; b < sn; b++) {
        sir_node_t* sb = eng->spine[b];
        if (sb->tag != SIR_BRANCH) continue;
        sir_node_t* cmp = sb->branch.cond;
        if (cmp && sb->branch.on_true != sb->branch.on_false) {
            void* cf = cp_pmap_get(&eng->expr_idx, cmp);
            if (cf) {
                eng->branch_cond_vn[b]  = (int)((uintptr_t)cf - 1);
                eng->branch_fact_ord[b] = nfacts++;
            }
        }

        /* ── Spec §2's `br_on_cast`: "splits pts(u) along its two successor edges the
         * same way" the cast filters it. The condition IS the InstanceOf node — the
         * DDCG's γ=pair(Lt,Lf) with δ=effect emits `Branch(value, Lt, Lf)` directly, so
         * a boolean-valued expression is the branch condition, not a Cmp against 0.
         *
         * BOTH edges. `if (x instanceof A) …` tells us what x is on the way in AND what
         * it is not on the way past, and the second half is free: the same Refine device,
         * the other predicate. It is also what a source-level cast rides on — the
         * compiler lowers `(A) x` to `x == null ? x : x instanceof A ? x : throw CCE`
         * (JLS §5.5), so this refinement is what makes the value on the OK arm carry the
         * narrowed pts, and what §3's consumer will read to delete the guard entirely. */
        if (cmp && cmp->tag == SIR_INSTANCEOF) {
            int tested = cp_cmp_operand_ultimate(eng, cmp->instance_of.obj);
            if (tested < 0) tested = cp_vnode_of(eng, cmp->instance_of.obj);
            if (tested < 0) continue;
            sir_atype_t at = cmp->instance_of.atype;
            int cls = cmp->instance_of.class_id;
            b_rt[b]  = cp_new_refine_isa(eng, tested, CP_REFINE_PTS_ISA, at, cls);
            b_rf[b]  = cp_new_refine_isa(eng, tested, CP_REFINE_PTS_NOT_ISA, at, cls);
            b_und[b] = tested;
            any = true;
            continue;
        }

        if (!sir_is_cmp(cmp)) continue;
        sir_node_t* lhs = sir_child(cmp, 0);
        sir_node_t* rhs = sir_child(cmp, 1);

        /* ── Spec §4: the NULL test. "The branch refinement is the key: on the
         * true edge out of a ref.is_null / != null test the operand is Null, on
         * the false edge NonNull (per-edge facts, exactly SCCP's executable-edge
         * mechanism — carried on the SIR edge, no dominance)."
         *
         * This is the mechanism the whole nullability element rests on, and it is
         * why an NPE guard can go: E7 emits `if (x == null) throw` at EVERY deref,
         * so surviving one such branch is precisely the evidence that x is NonNull
         * from there on — the SECOND and every later deref of x then folds. Note
         * `null` is a LoadNull, not a LoadConst, which is why the constant path
         * below never sees it. */
        if ((cmp->tag == SIR_EQ || cmp->tag == SIR_NE)
                && (lhs->tag == SIR_LOADNULL || rhs->tag == SIR_LOADNULL)) {
            sir_node_t* opnd = lhs->tag == SIR_LOADNULL ? rhs : lhs;
            int tested = cp_cmp_operand_ultimate(eng, opnd);
            if (tested < 0) tested = cp_vnode_of(eng, opnd);
            if (tested < 0) continue;
            /* `x == null` true ⟹ Null, false ⟹ NonNull; `!=` is the mirror. */
            bool eq = (cmp->tag == SIR_EQ);
            cp_refine_pts_t on_t = eq ? CP_REFINE_PTS_NULL    : CP_REFINE_PTS_NONNULL;
            cp_refine_pts_t on_f = eq ? CP_REFINE_PTS_NONNULL : CP_REFINE_PTS_NULL;
            b_rt[b]  = cp_new_refine_pts(eng, tested, on_t);
            b_rf[b]  = cp_new_refine_pts(eng, tested, on_f);
            b_und[b] = tested;
            any = true;
            continue;
        }

        /* Identify which side is the variable (non-constant) and which
         * is the constant. */
        int64_t kval = 0;
        cp_cwidth_t kw = CP_W_I32;
        bool tested_on_left;
        sir_node_t* tested_opnd;
        if (cp_cmp_operand_const(eng, rhs, &kval, &kw)) {
            tested_opnd = lhs; tested_on_left = true;
        } else if (cp_cmp_operand_const(eng, lhs, &kval, &kw)) {
            tested_opnd = rhs; tested_on_left = false;
        } else {
            /* Neither side is a constant. Spec §5 still wants a refinement here
             * for the one shape that matters: `i < <some value>` BINDS i to that
             * value as a symbolic upper bound. No interval can say this — the
             * bound is a value — so the range carries the bounding vnode itself,
             * and the existing arm rewiring puts the fact on i's uses in the
             * body. That is what makes the §15 upper-bounds guard fall. */
            /* `x == y` (neither constant), TRUE edge: x inherits y as BOTH an
             * inclusive upper and lower symbolic bound — x <= y AND x >= y. The
             * twin of the `i < B` mint below; the lower half feeds lo_vn1, which
             * the consumer reads to fold a lower guard on x exactly as hi_vn1
             * folds an upper one. Inclusive on both sides is the sound choice: the
             * SIR_GE consumer only folds `x >= L` when `L ≡ B+1`, so `x == y` does
             * not wrongly fold `x >= y`. The FALSE edge (x != y) binds nothing. */
            if (cmp->tag == SIR_EQ) {
                int xvn = cp_cmp_operand_ultimate(eng, lhs);
                if (xvn < 0) xvn = cp_vnode_of(eng, lhs);
                int yvn = cp_cmp_operand_ultimate(eng, rhs);
                if (yvn < 0) yvn = cp_vnode_of(eng, rhs);
                if (xvn >= 0 && yvn >= 0 && xvn != yvn) {
                    cp_const_t sym = { .state = CP_C_RANGE, .cwidth = CP_W_I32,
                                       .lo = INT32_MIN, .hi = INT32_MAX, .stride = 1,
                                       .hi_vn1 = yvn + 1, .hi_vn_incl = 1,
                                       .lo_vn1 = yvn + 1, .lo_vn_incl = 1 };
                    b_rt[b]  = cp_new_refine(eng, xvn, sym);
                    b_und[b] = xvn;
                    any = true;
                    continue;
                }
            }
            int tested_vn = -1, bound_vn = -1;
            if (cmp->tag == SIR_LT || cmp->tag == SIR_LE) {
                tested_vn = cp_cmp_operand_ultimate(eng, lhs);   /* i < B */
                bound_vn  = cp_cmp_operand_ultimate(eng, rhs);
                if (bound_vn < 0) bound_vn = cp_vnode_of(eng, rhs);
            } else if (cmp->tag == SIR_GT || cmp->tag == SIR_GE) {
                tested_vn = cp_cmp_operand_ultimate(eng, rhs);   /* B > i  ⟹ i < B */
                bound_vn  = cp_cmp_operand_ultimate(eng, lhs);
                if (bound_vn < 0) bound_vn = cp_vnode_of(eng, lhs);
            }
            if (tested_vn >= 0 && bound_vn >= 0 && tested_vn != bound_vn) {
                /* Spec §5 names BOTH `<` and `<=`. The carrier records which: strict (`i < B`)
                 * or inclusive (`i <= B`, i.e. `i < B+1`). `<=` used to bind nothing at all —
                 * the carrier could only say "strictly less than" — so an inclusive loop bound
                 * was thrown away. What it is worth is the consumer's business, not this
                 * transfer's: recording a weaker fact is not the same as recording none. */
                bool incl = (cmp->tag == SIR_LE || cmp->tag == SIR_GE);
                cp_const_t sym = { .state = CP_C_RANGE, .cwidth = CP_W_I32,
                                   .lo = INT32_MIN, .hi = INT32_MAX, .stride = 1,
                                   .hi_vn1 = bound_vn + 1, .hi_vn_incl = incl ? 1 : 0 };
                /* Composition with an enclosing refinement is pass B's input-chaining. */
                b_rt[b]  = cp_new_refine(eng, tested_vn, sym);
                b_und[b] = tested_vn;
                any = true;
                continue;
            }
            /* The FALSE-edge half of the §5 rule: the fall-through of `l > r`
             * carries l ≤ r — an INCLUSIVE symbolic upper bound on l. This is
             * the Mem hi-guard's shape (`(long)addr > limit`), where the true-
             * edge arm above cannot fire (the bound side is an expression, not
             * a slot read). One I2L descent: the guard compares the slot's
             * long view, and I2L is order-preserving, so the bound is a fact
             * about the slot's value; the §15 consumer reads back through the
             * I2L. Only the inclusive GT arm is recorded — the strict mirrors
             * (GE-false, LE-false) would invite §5's strict tightening, which
             * is stated in the COMPARE's width and is unsound stamped onto the
             * i32 slot carrier when the compare is the widened i64. Fires only
             * when the true-edge arm did not: b_und holds ONE underlying. */
            if (cmp->tag == SIR_GT) {
                sir_node_t* l_op = (lhs->tag == SIR_I2L) ? lhs->i2_l.operand : lhs;
                if (l_op && l_op->tag == SIR_LOADLOCAL
                        && l_op->load_local.data_type == SIR_DTINT) {
                    int t   = cp_cmp_operand_ultimate(eng, l_op);
                    int bnd = cp_cmp_operand_ultimate(eng, rhs);
                    if (bnd < 0) bnd = cp_vnode_of(eng, rhs);
                    if (t >= 0 && bnd >= 0 && t != bnd) {
                        cp_const_t symf = { .state = CP_C_RANGE, .cwidth = CP_W_I32,
                                            .lo = INT32_MIN, .hi = INT32_MAX, .stride = 1,
                                            .hi_vn1 = bnd + 1, .hi_vn_incl = 1 };
                        b_rf[b]  = cp_new_refine(eng, t, symf);
                        b_und[b] = t;
                        any = true;
                    }
                }
            }
            continue;
        }
        /* Range refinement only applies to the integer widths (floats have
         * no range lattice; their comparisons don't refine). */
        if (kw >= CP_W_F32) continue;
        int underlying = cp_cmp_operand_ultimate(eng, tested_opnd);
        if (underlying < 0) continue;
        cp_const_t pred_then = cp_branch_predicate_refine(cmp->tag, kval, kw,
                                                          tested_on_left,
                                                          CP_BRANCH_ARM_THEN);
        cp_const_t pred_else = cp_branch_predicate_refine(cmp->tag, kval, kw,
                                                          tested_on_left,
                                                          CP_BRANCH_ARM_ELSE);
        /* `if (i >= 0) { if (i < 10) … }` must keep BOTH facts — composition is pass B's
         * input-chaining (the inner Refine re-bases onto the outer one as it flows in). */
        if (pred_then.state != CP_C_BOTTOM) {
            b_rt[b] = cp_new_refine(eng, underlying, pred_then);
            b_und[b] = underlying;
            any = true;
        }
        if (pred_else.state != CP_C_BOTTOM) {
            b_rf[b] = cp_new_refine(eng, underlying, pred_else);
            b_und[b] = underlying;
            any = true;
        }
    }
    if (!any && nfacts == 0) return;

    /* ── PASS B: re-derive the slot states with the per-edge rule, then rewire.
     *
     * A non-merge node's state comes from its single predecessor's, refined when
     * the crossing edge says so; a slot the predecessor RE-DEFINED takes the pass-A
     * value unrefined (a def kills refinement; the test is
     * `slot_in[n][s] != slot_in[pred][s]`, so pass A's own opaque def vnodes — Inc,
     * ExceptionEntry — are REUSED, never re-minted). A MERGE row is SCCP's join —
     * see the rule at the merge case below. (History: this pass first shipped with
     * merges reset verbatim to pass-A — plan §R.1's deliberate strict-parity choice,
     * "refinement drops at joins" — whose recorded cost was String.replace's
     * IDX_HIGH −1; the join rule below is that deferred upgrade, landed as its own
     * census-visible change.)
     *
     * `slot_in` itself is NOT touched: downstream consumers keep pass A's view; the
     * refined states exist to wire the LOADS, which is all the old phase 2 did. */
    int sc = eng->slot_count;
    int *p_off, *p_cnt, *p_list;
    cp_build_pred_csr(eng, &p_off, &p_cnt, &p_list);
    int entry_idx = cp_spine_index(eng, eng->method->entry);
    int** in2  = (int**)bbq_arena_alloc(a, (size_t)sn * sizeof(int*));
    for (int n = 0; n < sn; n++)
        in2[n] = (int*)bbq_arena_alloc(a, (size_t)(sc > 0 ? sc : 1) * sizeof(int));
    /* The pure pair-state table (Click ch.2 §2.3 — see the cluster's block comment) and
     * the materialization memo used only AFTER convergence. */
    cp_pb_pairs_t pairs;
    pairs.r = NULL; pairs.in = NULL;
    bbq_hmap_init(&pairs.memo, 0);

    /* Verdict fact state (channel (a)) — rides the SAME optimistic sweep as the
     * slot rows: per node a bitset over fact ids, TOP (= "unreached", the
     * intersection identity) everywhere but the entry, met by AND at merges and
     * extended by the crossing edge's own fact. Value facts have no kills, so
     * the transfer is pure OR-then-AND; bits only descend from TOP, so it
     * converges with the slot rows. */
    int vstride = (2 * nfacts + 63) / 64;
    uint64_t* vw   = NULL;
    bool*     vtop = NULL;
    uint64_t* vtmp = NULL;
    uint64_t* vacc = NULL;
    if (nfacts > 0) {
        vw   = (uint64_t*)bbq_arena_alloc(a, (size_t)sn * vstride * sizeof(uint64_t));
        vtop = (bool*)bbq_arena_alloc(a, (size_t)sn * sizeof(bool));
        vtmp = (uint64_t*)bbq_arena_alloc(a, (size_t)vstride * sizeof(uint64_t));
        vacc = (uint64_t*)bbq_arena_alloc(a, (size_t)vstride * sizeof(uint64_t));
        memset(vw, 0, (size_t)sn * vstride * sizeof(uint64_t));
        for (int n = 0; n < sn; n++) vtop[n] = (n != entry_idx);
        eng->fact_branch = (int*)bbq_arena_alloc(a, (size_t)nfacts * sizeof(int));
        for (int b = 0; b < sn; b++)
            if (eng->branch_fact_ord[b] >= 0)
                eng->fact_branch[eng->branch_fact_ord[b]] = b;
    }

    /* OPTIMISTIC iterated sweeps — SCCP's executable-edge fixpoint (§4: branch refinements
     * are "per-edge facts, exactly SCCP's executable-edge mechanism"). Every non-entry row
     * starts at CP_R_TOP ("edge not yet reached", the join identity); the entry row starts at
     * its pass-A base. A node's row is the MEET over its RECORDED preds of each edge's refined
     * state (cp_pb_meet — Kildall's join-point rule). Optimism is what carries a loop-invariant
     * fact: the back edge begins at TOP, so on sweep 1 a header holds its forward refinement,
     * which propagates to the latch and (for an invariant slot) rejoins itself — a real
     * fixpoint, no back-edge identification. A redefined slot (counter) has a header φ whose
     * preds' base differs, so it meets to base; a diamond's disagreeing arms meet to base.
     * Chains only shorten (monotone descent), so it terminates. */
    for (int n = 0; n < sn; n++)
        for (int s = 0; s < sc; s++)
            in2[n][s] = (n == entry_idx) ? eng->slot_in[n][s] : CP_R_TOP;
    int max_sweeps = 2 * sn + 4;
    for (int sweep = 0; ; sweep++) {
        if (sweep >= max_sweeps) {
            fprintf(stderr, "cp_compute_branch_refinements: pass B failed to "
                            "converge in %d sweeps\n", max_sweeps);
            abort();
        }
        bool changed = false;
        for (int n = 0; n < sn; n++) {
            if (n == entry_idx) continue;
            if (p_cnt[n] == 1) {
                int p = p_list[p_off[n]];
                for (int s = 0; s < sc; s++) {
                    int pv = in2[p][s];
                    int nv;
                    if (pv == CP_R_TOP) nv = CP_R_TOP;              /* pred not reached yet */
                    else if (eng->slot_in[n][s] != eng->slot_in[p][s])
                        nv = eng->slot_in[n][s];                    /* def at p: fresh, unrefined */
                    else nv = cp_edge_refined(eng, b_rt, b_rf, b_und,
                                              &pairs, p, n, pv);
                    if (nv != in2[n][s]) { in2[n][s] = nv; changed = true; }
                }
                if (vw && !vtop[p]) {
                    memcpy(vtmp, vw + (size_t)p * vstride,
                           (size_t)vstride * sizeof(uint64_t));
                    cp_verdict_edge_or(eng, vtmp, p, n);
                    uint64_t* row = vw + (size_t)n * vstride;
                    if (vtop[n] || memcmp(row, vtmp,
                                          (size_t)vstride * sizeof(uint64_t)) != 0) {
                        memcpy(row, vtmp, (size_t)vstride * sizeof(uint64_t));
                        vtop[n] = false;
                        changed = true;
                    }
                }
            } else if (p_cnt[n] >= 2) {
                /* A merge's row is the MEET over its recorded preds (Kildall; §4's SCCP
                 * executable-edge rule). Optimistic TOP-init lets a loop-invariant refinement
                 * survive (the back edge starts at TOP = identity); cp_pb_meet keeps the
                 * longest common chain suffix — the weakest fact EVERY reached edge implies —
                 * and drops to base on disagreement (a diamond). A pred that REDEFINED the
                 * slot delivers the fresh base; the meet then drops to base — a counter
                 * (header φ) never keeps a branch refinement. */
                for (int s = 0; s < sc; s++) {
                    int base = eng->slot_in[n][s];
                    int acc = CP_R_TOP;
                    for (int pi = 0; pi < p_cnt[n]; pi++) {
                        int p = p_list[p_off[n] + pi];
                        int pv = in2[p][s];
                        if (pv == CP_R_TOP) continue;               /* unreached: identity */
                        int vp = (eng->slot_in[p][s] != base)
                            ? base                                  /* redefined on this edge */
                            : cp_edge_refined(eng, b_rt, b_rf, b_und,
                                              &pairs, p, n, pv);
                        acc = cp_pb_meet(eng, &pairs, acc, vp, base);
                    }
                    int nv = (acc == CP_R_TOP) ? in2[n][s] : acc;   /* no pred reached: unchanged */
                    if (nv != in2[n][s]) { in2[n][s] = nv; changed = true; }
                }
                if (vw) {
                    bool got = false;
                    for (int pi = 0; pi < p_cnt[n]; pi++) {
                        int p = p_list[p_off[n] + pi];
                        if (vtop[p]) continue;                      /* unreached: identity */
                        memcpy(vtmp, vw + (size_t)p * vstride,
                               (size_t)vstride * sizeof(uint64_t));
                        cp_verdict_edge_or(eng, vtmp, p, n);
                        if (!got) {
                            memcpy(vacc, vtmp, (size_t)vstride * sizeof(uint64_t));
                            got = true;
                        } else {
                            for (int w = 0; w < vstride; w++) vacc[w] &= vtmp[w];
                        }
                    }
                    if (got) {
                        uint64_t* row = vw + (size_t)n * vstride;
                        if (vtop[n] || memcmp(row, vacc,
                                              (size_t)vstride * sizeof(uint64_t)) != 0) {
                            memcpy(row, vacc, (size_t)vstride * sizeof(uint64_t));
                            vtop[n] = false;
                            changed = true;
                        }
                    }
                }
            }
        }
        if (!changed) break;
    }
    if (vw) {
        /* A row still TOP is unreached by the sweep: publish it as NO facts —
         * fail-closed, so a consumer never folds on a vacuous verdict. */
        for (int n = 0; n < sn; n++)
            if (vtop[n])
                memset(vw + (size_t)n * vstride, 0,
                       (size_t)vstride * sizeof(uint64_t));
        eng->verdict_words     = vw;
        eng->verdict_stride    = vstride;
        eng->verdict_rows      = sn;
        eng->branch_fact_count = nfacts;
    }

    /* Rewire every slot load to its CONVERGED state. Pair-states materialize into Refine
     * vnodes HERE and only here — every minted node corresponds to a converged (gfp) row,
     * which is Click ch.2 §2.3's licensed bottom-up transform ("bottom-up methods can
     * transform as they analyze"; a top-down intermediate may not). Transients never
     * became nodes, so no unproven element exists in the shared vnode space. */
    int total_vn = eng->vnode_count;
    for (int vi = 0; vi < total_vn; vi++) {
        cp_vnode_t* v = eng->vnodes[vi];
        if (v->kind != CP_VN_EXPR || !v->expr) continue;
        if (v->expr->tag != SIR_LOADLOCAL) continue;
        int sp = v->parent_spine;
        if (sp < 0 || sp >= sn) continue;
        if (v->input_count < 1 || v->inputs[0] < 0) continue;
        int slot = v->expr->load_local.slot;
        if (slot < 0 || slot >= sc) continue;
        int st = in2[sp][slot];
        if (st <= CP_R_PAIR0) {
            /* Materialize the pair chain innermost-first (memoized ⟹ canonical). */
            int depth = 0, chain[CP_PB_MAXCHAIN];
            while (st <= CP_R_PAIR0 && depth < CP_PB_MAXCHAIN) {
                int i = CP_R_PAIR0 - st;
                chain[depth++] = pairs.r[i];
                st = pairs.in[i];
            }
            if (st < 0) continue;                 /* overdeep/degenerate: keep pass-A */
            for (int d = depth - 1; d >= 0; d--)
                st = cp_compose_refine(eng, chain[d], st);
        }
        if (st >= 0) v->inputs[0] = st;   /* TOP(-2)/no-def(-1) keep pass-A */
    }
    bbq_vec_free(pairs.r);
    bbq_vec_free(pairs.in);
    bbq_hmap_free(&pairs.memo);
}

/* ── Reverse def-use index ───────────────────────────────────── */

/* Look up or assign the cell index for a packed key. Returns the
 * cell index; inserts into eng->mem_cell_idx if new. */
static int cp_cell_intern(cp_engine_t* eng, uint32_t key) {
    void* found = bbq_htree_search(eng->mem_cell_idx, (uint32_t)(key + 1));
    if (found) return (int)((uintptr_t)found - 1);
    int idx = eng->mem_cell_count++;
    bbq_vec_push(eng->mem_cell_keys, key);
    bbq_htree_insert(eng->mem_cell_idx, (uint32_t)(key + 1),
                     (void*)(uintptr_t)(idx + 1));
    return idx;
}

/* Walk the spine + expression trees once, collecting every distinct
 * memory cell that gets touched by a get / put / load / store. */
static void cp_enum_cells_in_expr(cp_engine_t* eng, sir_node_t* e);
static void cp_enum_cells_in_expr(cp_engine_t* eng, sir_node_t* e) {
    if (!e) return;
    uint32_t key = cp_cell_key_for_expr(e);
    if (key != CP_CELL_NONE) cp_cell_intern(eng, key);
    int n = sir_arity(e);
    for (int i = 0; i < n; i++) cp_enum_cells_in_expr(eng, sir_child(e, i));
}

static void cp_enumerate_memory_cells(cp_engine_t* eng) {
    eng->mem_cell_idx   = bbq_htree_create();
    eng->mem_cell_count = 0;
    eng->mem_cell_keys  = NULL;
    for (int n = 0; n < eng->spine_count; n++) {
        sir_node_t* sn = eng->spine[n];
        /* The spine node's own cell, if it's a writer. */
        uint32_t key = cp_cell_key_for_spine(sn);
        if (key != CP_CELL_NONE && key != CP_CELL_ALL)
            cp_cell_intern(eng, key);
        /* Cells touched by expression children (reads). */
        int ar = sir_arity(sn);
        for (int j = 0; j < ar; j++)
            cp_enum_cells_in_expr(eng, sir_child(sn, j));
    }

    /* Which cells can no code write after the allocation? A call kills every cell it
     * COULD write, and it cannot write these — so it must not shadow them (see
     * cp_out_state). The type lattice is the one authority for which cells they are;
     * the key is decoded with the same packing that built it. */
    eng->cell_immutable = (bool*)bbq_arena_alloc(eng->arena,
                              (size_t)(eng->mem_cell_count > 0 ? eng->mem_cell_count : 1)
                              * sizeof(bool));
    for (int c = 0; c < eng->mem_cell_count; c++) {
        eng->cell_immutable[c] = false;
        if (!eng->sema) continue;
        uint32_t k = eng->mem_cell_keys[c];
        if ((k & CP_CELL_KIND_MASK) != CP_CELL_KIND_FIELD) continue;
        int class_id = (int)((k >> 16) & 0x3FFF);
        int field_ix = (int)(k & 0xFFFF);
        eng->cell_immutable[c] = lat_is_array_data_cell(eng->sema, class_id, field_ix);
    }
}

/* Build the def-use index: for each value node, the users that take
 * it as an operand and the operand position at which they do. */
static void cp_build_defuse(cp_engine_t* eng) {
    int vc = eng->vnode_count;
    if (vc == 0) return;
    bbq_arena* a = eng->arena;
    eng->du_off = (int*)bbq_arena_alloc(a, (size_t)vc * sizeof(int));
    eng->du_cnt = (int*)bbq_arena_alloc(a, (size_t)vc * sizeof(int));
    memset(eng->du_cnt, 0, (size_t)vc * sizeof(int));
    for (int v = 0; v < vc; v++) {
        cp_vnode_t* node = eng->vnodes[v];
        for (int i = 0; i < node->input_count; i++) {
            int in = node->inputs[i];
            if (in >= 0 && in < vc) eng->du_cnt[in]++;
        }
    }
    int total = 0;
    for (int v = 0; v < vc; v++) {
        eng->du_off[v] = total;
        total += eng->du_cnt[v];
    }
    eng->du_user  = (int*)bbq_arena_alloc(a, (size_t)(total > 0 ? total : 1)
                                             * sizeof(int));
    eng->du_input = (int*)bbq_arena_alloc(a, (size_t)(total > 0 ? total : 1)
                                             * sizeof(int));
    int* cursor = (int*)bbq_arena_alloc(a, (size_t)vc * sizeof(int));
    memset(cursor, 0, (size_t)vc * sizeof(int));
    for (int v = 0; v < vc; v++) {
        cp_vnode_t* node = eng->vnodes[v];
        for (int i = 0; i < node->input_count; i++) {
            int in = node->inputs[i];
            if (in >= 0 && in < vc) {
                int k = eng->du_off[in] + cursor[in]++;
                eng->du_user[k]  = v;
                eng->du_input[k] = i;
            }
        }
    }
}

/* ── Congruence partitions ───────────────────────────────────── */

/* Create a new empty partition, returning its id. */
static int cp_part_new(cp_engine_t* eng) {
    int id = eng->partition_count;
    cp_partition_t* p = (cp_partition_t*)bbq_arena_alloc(eng->arena, sizeof *p);
    p->head          = -1;
    p->count         = 0;
    p->leader_count  = 0;
    p->on_worklist   = false;
    p->touched_head  = -1;
    p->touched_count = 0;
    p->cprop_head    = -1;
    p->on_cprop_wl   = false;
    p->type          = NULL;
    p->constant.state = CP_C_TOP;
    p->constant.value = 0;
    bbq_vec_push(eng->partitions, p);
    eng->partition_count++;
    return id;
}

/* Click §4.7.5 X.cprop maintenance: enqueue vnode `v` onto its
 * partition's local cprop worklist (and the partition onto the
 * global cprop worklist if it isn't already), marking that v's
 * type needs recomputation. Idempotent — repeated calls are no-ops
 * via the in_cprop membership flag. */
static void cp_cprop_enqueue(cp_engine_t* eng, int v) {
    cp_vnode_t* vn = eng->vnodes[v];
    if (vn->in_cprop) return;
    if (vn->partition < 0) return;
    cp_partition_t* p = eng->partitions[vn->partition];
    vn->cprop_prev = -1;
    vn->cprop_next = p->cprop_head;
    if (p->cprop_head >= 0) eng->vnodes[p->cprop_head]->cprop_prev = v;
    p->cprop_head = v;
    vn->in_cprop = true;
    if (!p->on_cprop_wl) {
        p->on_cprop_wl = true;
        bbq_vec_push(eng->cprop_worklist, vn->partition);
    }
}

/* Remove vnode v from its partition's cprop list. Caller must ensure
 * v->in_cprop is true on entry; the partition stays on the global
 * cprop worklist until its cprop list drains (handled by PROPAGATE). */
static void cp_cprop_dequeue(cp_engine_t* eng, int v) {
    cp_vnode_t* vn = eng->vnodes[v];
    if (!vn->in_cprop) return;
    cp_partition_t* p = eng->partitions[vn->partition];
    if (vn->cprop_prev >= 0)
        eng->vnodes[vn->cprop_prev]->cprop_next = vn->cprop_next;
    else if (p->cprop_head == v)
        p->cprop_head = vn->cprop_next;
    if (vn->cprop_next >= 0)
        eng->vnodes[vn->cprop_next]->cprop_prev = vn->cprop_prev;
    vn->cprop_prev = -1;
    vn->cprop_next = -1;
    vn->in_cprop = false;
}

/* Link value node `v` onto the front of partition `pid`. */
static void cp_part_add(cp_engine_t* eng, int pid, int v) {
    cp_partition_t* p = eng->partitions[pid];
    cp_vnode_t* node = eng->vnodes[v];
    node->partition = pid;
    node->part_prev = -1;
    node->part_next = p->head;
    if (p->head >= 0) eng->vnodes[p->head]->part_prev = v;
    p->head = v;
    p->count++;
    if (node->leader == -1) p->leader_count++;
}

/* Unlink value node `v` from its current partition's member list. */
/* Returns true when the node had a PENDING cprop recompute — the
 * caller moving it to a new partition MUST re-enqueue it there
 * (cp_cprop_enqueue after cp_part_add), or the recompute is silently
 * dropped and the node's fact goes stale (a loop accumulator stuck at
 * its optimistic entry constant). */
static bool cp_part_remove(cp_engine_t* eng, int v) {
    cp_vnode_t* node = eng->vnodes[v];
    cp_partition_t* p = eng->partitions[node->partition];
    bool was_pending = node->in_cprop;
    /* If this vnode is on partition's cprop list, unlink it FIRST —
     * cp_cprop_dequeue reads vnode->partition, so it must still point
     * at this old partition. */
    if (node->in_cprop) {
        if (node->cprop_prev >= 0)
            eng->vnodes[node->cprop_prev]->cprop_next = node->cprop_next;
        else if (p->cprop_head == v)
            p->cprop_head = node->cprop_next;
        if (node->cprop_next >= 0)
            eng->vnodes[node->cprop_next]->cprop_prev = node->cprop_prev;
        node->cprop_prev = -1;
        node->cprop_next = -1;
        node->in_cprop   = false;
    }
    if (node->part_prev >= 0)
        eng->vnodes[node->part_prev]->part_next = node->part_next;
    else
        p->head = node->part_next;
    if (node->part_next >= 0)
        eng->vnodes[node->part_next]->part_prev = node->part_prev;
    p->count--;
    if (node->leader == -1) p->leader_count--;
    return was_pending;
}

/* Enqueue partition `pid` as a splitter, unless already pending. */
static void cp_wl_push(cp_engine_t* eng, int pid) {
    if (eng->partitions[pid]->on_worklist) return;
    eng->partitions[pid]->on_worklist = true;
    bbq_vec_push(eng->worklist, pid);
}

/* Click §4.7.4 F.def_use list maintenance. The Followers of a Leader
 * vnode l form a doubly-linked list rooted at eng->follower_head[l].
 * cp_follower_link / cp_follower_unlink keep the list in sync with
 * vnode->leader at every leader change so cp_split / the §4.7.2 race
 * can walk a Leader's Followers in O(|F|). */
static void cp_follower_grow(cp_engine_t* eng, int up_to) {
    if (up_to < eng->follower_head_cap) return;
    int new_cap = eng->follower_head_cap > 0 ? eng->follower_head_cap : 16;
    while (new_cap <= up_to) new_cap *= 2;
    int* nh = (int*)bbq_arena_alloc(eng->arena, (size_t)new_cap * sizeof(int));
    for (int i = 0; i < eng->follower_head_cap; i++) nh[i] = eng->follower_head[i];
    for (int i = eng->follower_head_cap; i < new_cap; i++) nh[i] = -1;
    eng->follower_head = nh;
    eng->follower_head_cap = new_cap;
}

static void cp_follower_link(cp_engine_t* eng, int follower, int leader) {
    cp_follower_grow(eng, leader);
    cp_vnode_t* fv = eng->vnodes[follower];
    int head = eng->follower_head[leader];
    fv->follower_prev = -1;
    fv->follower_next = head;
    if (head >= 0) eng->vnodes[head]->follower_prev = follower;
    eng->follower_head[leader] = follower;
}

static void cp_follower_unlink(cp_engine_t* eng, int follower, int leader) {
    if (leader < 0 || leader >= eng->follower_head_cap) return;
    cp_vnode_t* fv = eng->vnodes[follower];
    if (fv->follower_prev >= 0)
        eng->vnodes[fv->follower_prev]->follower_next = fv->follower_next;
    else if (eng->follower_head[leader] == follower)
        eng->follower_head[leader] = fv->follower_next;
    if (fv->follower_next >= 0)
        eng->vnodes[fv->follower_next]->follower_prev = fv->follower_prev;
    fv->follower_prev = -1;
    fv->follower_next = -1;
}

/* Whether nodes of this opcode may start in a shared partition: a
 * pure operation whose result is a function of its inputs alone.
 * Trapping, allocating, effectful, and memory-reading opcodes are
 * each their own congruence class. */
static bool cp_congruent_op(int tag) {
    if (tag < 0 || tag >= SIR_TAG_COUNT) return false;
    return sir_op_gamma[tag].is_congruent;
}

/* Whether `e` is a commutative binary op — `a + b ≡ b + a`. Used at
 * refinement to fold input positions 0 and 1 into one effective
 * position, so per-position congruence matches the multiset
 * semantics (§8.1.4). γ-table-driven: rows hold a bool for static
 * cases (ADD / MUL / AND / OR / XOR) and an optional fn-pointer for
 * discriminator-dependent cases (CMP commutative only on EQ / NE). */
static bool cp_op_is_commutative(const sir_node_t* e) {
    if (!e) return false;
    if (e->tag < 0 || e->tag >= SIR_TAG_COUNT) return false;
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];
    return g->is_commutative_fn ? g->is_commutative_fn(e) : g->is_commutative;
}

/* The key two pure nodes must share to start in one partition: the
 * SIR tag, plus — for opcodes whose computed function depends on an
 * operator immediate (γ bucket_discriminator; GetField/GetStatic by
 * (class_id, field_idx)) — the exact discriminator value, looked up
 * two-level so no packing can collide two distinct fields into one
 * bucket. Constant values are not a key here; separating them is the
 * constant-propagation step. */
static uint32_t (*cp_opcode_disc_fn(const cp_vnode_t* v))(const sir_node_t*) {
    if (v->op < 0 || v->op >= SIR_TAG_COUNT || !v->expr) return NULL;
    return sir_op_gamma[v->op].bucket_discriminator;
}

/* Place every value node in an initial congruence partition. Pure
 * expression nodes computing the same operation start together —
 * the optimistic assumption partition refinement then narrows by
 * input. φ nodes share one partition; opaque and effectful nodes
 * each get a singleton. */
static void cp_partition_init(cp_engine_t* eng) {
    bbq_htree* by_key = bbq_htree_create();        /* op+1 -> pid+1 */
    bbq_htree* by_op_disc = bbq_htree_create();    /* op+1 -> inner htree */
    bbq_htree** inners = NULL;                     /* for destroy */
    cp_pmap_t phi_by_merge;                        /* merge node -> pid+1 */
    cp_pmap_init(&phi_by_merge);
    /* Pass 1: non-LOADLOCAL vnodes by opcode bucket. LOADLOCAL is
     * deferred to Pass 2 because it is a §4.7 COPY Follower of its
     * reaching definition (its one input). Placing LOADLOCAL nodes
     * into a shared opcode bucket lets refinement spuriously split
     * them by reaching-def chain rewrites, breaking downstream
     * congruences. */
    for (int v = 0; v < eng->vnode_count; v++) {
        cp_vnode_t* node = eng->vnodes[v];
        if (node->kind == CP_VN_EXPR && node->op == SIR_LOADLOCAL) continue;
        if (node->kind == CP_VN_REFINE
                && node->refine_pts != CP_REFINE_PTS_NONE) continue;  /* Pass 2 */
        int pid;
        if (node->kind == CP_VN_PHI) {
            /* A φ's MERGE POINT is part of its identity — in Click's
             * sea-of-nodes the Region is the φ's input 0. Here the merge is a
             * field, not an input, so CAUSE_SPLITS (which splits only by input
             * partition) cannot tell two φs apart when their contributors line
             * up positionally. Every `c ? 1 : 0` merges {LoadConst 1,
             * LoadConst 0}, so a single φ bucket made all of them congruent:
             * the peer-φ canonicalization then rewrote every read onto the
             * first, the other diamonds went dead, and their branches — with
             * the CALLS in their conditions — were deleted
             * (`(g(3)?1:0)*100 + (g(65)?1:0)*10 + (g(4)?1:0)` → `s*111`).
             * Bucketing by merge restores the Region-as-input semantics; φs at
             * the SAME merge still share a bucket, which is what §4.10's
             * peer-φ (same merge, different slots) collapse needs. */
            void* f = cp_pmap_get(&phi_by_merge, node->phi_merge);
            if (f) {
                pid = (int)((uintptr_t)f - 1);
            } else {
                pid = cp_part_new(eng);
                cp_pmap_put(&phi_by_merge, node->phi_merge,
                            (void*)(uintptr_t)(pid + 1));
            }
        } else if (node->kind == CP_VN_EXPR && cp_congruent_op(node->op)) {
            uint32_t okey = (uint32_t)node->op + 1;   /* +1: never 0 */
            uint32_t (*disc_fn)(const sir_node_t*) = cp_opcode_disc_fn(node);
            bbq_htree* level = by_key;
            if (disc_fn) {
                /* Two-level: op picks the inner map, the exact
                 * discriminator picks the bucket. */
                bbq_htree* inner =
                    (bbq_htree*)bbq_htree_search(by_op_disc, okey);
                if (!inner) {
                    inner = bbq_htree_create();
                    bbq_vec_push(inners, inner);
                    bbq_htree_insert(by_op_disc, okey, inner);
                }
                level = inner;
                okey = disc_fn(node->expr) + 1;
            }
            void* found = bbq_htree_search(level, okey);
            if (found) {
                pid = (int)((uintptr_t)found - 1);
            } else {
                pid = cp_part_new(eng);
                bbq_htree_insert(level, okey, (void*)(uintptr_t)(pid + 1));
            }
        } else {
            pid = cp_part_new(eng);
        }
        cp_part_add(eng, pid, v);
    }
    bbq_htree_destroy(by_key);
    for (int i = 0; i < (int)bbq_vec_len(inners); i++)
        bbq_htree_destroy(inners[i]);
    bbq_vec_free(inners);
    bbq_htree_destroy(by_op_disc);
    cp_pmap_free(&phi_by_merge);
    /* Pass 2: LOADLOCAL as §4.7 COPY Follower — "x.opcode = COPY" is
     * Click's prototypical Follower (§4.7.1, §4.7.4). A LOADLOCAL with
     * one input is
     * a pass-through of its reaching def; pin it as a Follower so it
     * lives in the reaching def's partition from the start. With no
     * resolved input (orphan), fall back to a singleton. */
    /* A Refine that narrows only POINTS-TO is a §4.7 COPY Follower too. It
     * computes nothing: same value as its input, same constant (its constant
     * predicate is BOTTOM = the intersect identity), and pts is explicitly NOT
     * value identity — the engine's rule is that a pts fact can never move a
     * partition. Leaving it a Leader in a partition of its own would make every
     * expression over a null-checked reference incongruent with the same
     * expression over the reference itself — i.e. the entire graph downstream of
     * every deref. A RANGE refine, by contrast, carries a real constant predicate
     * and stays a Leader so it can hold it.
     *
     * A Follower's Leader must already have a partition, and the arm rewiring
     * points LoadLocals at Refines that were created later, so index order is not
     * dependency order — settle it as a fixpoint instead. */
    for (bool progress = true; progress; ) {
        progress = false;
        for (int v = 0; v < eng->vnode_count; v++) {
            cp_vnode_t* node = eng->vnodes[v];
            if (node->partition >= 0) continue;
            bool pts_only_refine = (node->kind == CP_VN_REFINE
                                    && node->refine_pts != CP_REFINE_PTS_NONE);
            if (!(node->kind == CP_VN_EXPR && node->op == SIR_LOADLOCAL)
                    && !pts_only_refine) continue;
            int rd = (node->input_count == 1) ? node->inputs[0] : -1;
            if (rd < 0 || rd >= eng->vnode_count) {
                cp_part_add(eng, cp_part_new(eng), v);   /* orphan: singleton */
                progress = true;
                continue;
            }
            if (eng->vnodes[rd]->partition < 0) continue;   /* Leader not placed yet */
            node->leader = rd;
            cp_part_add(eng, eng->vnodes[rd]->partition, v);
            cp_follower_link(eng, v, rd);
            progress = true;
        }
    }
    /* A copy chain that closes on itself has no Leader outside it; give each
     * remaining node a partition of its own rather than leave it unplaced. */
    for (int v = 0; v < eng->vnode_count; v++)
        if (eng->vnodes[v]->partition < 0) cp_part_add(eng, cp_part_new(eng), v);
}

/* ── Partition refinement ────────────────────────────────────── */

/* After a node MOVES partitions (split / split-by-facts), its def-use
 * users' partition-derived state is stale: a φ-Follower's "all live
 * inputs in ONE partition" invariant and the peer-φ checks read input
 * PARTITIONS, and their revert notifications may have been consumed
 * BEFORE the move (Click §4.7.5 orders SPLIT before apply/revert for
 * exactly this reason). Re-enqueue the users so they re-examine
 * against the post-move partitions. Bounded: each of the ≤ n-1 moves
 * enqueues its users once. */
static void cp_notify_users_of_move(cp_engine_t* eng, int v) {
    for (int k = eng->du_off[v]; k < eng->du_off[v] + eng->du_cnt[v]; k++)
        cp_cprop_enqueue(eng, eng->du_user[k]);
}

/* SPLIT (Click thesis §4.2): move partition `z`'s currently-touched
 * members into a fresh partition. If z is already pending, the new
 * partition is enqueued; otherwise the smaller of the two is — the
 * rule that bounds total work to O(n log n). */
static void cp_move_followers(cp_engine_t* eng, int leader_idx, int target_part);

static void cp_split(cp_engine_t* eng, int z) {
    int zp = cp_part_new(eng);
    int y = eng->partitions[z]->touched_head;
    while (y >= 0) {
        int next = eng->vnodes[y]->touched_next;
        bool pend = cp_part_remove(eng, y);
        cp_part_add(eng, zp, y);
        if (pend) cp_cprop_enqueue(eng, y);
        cp_notify_users_of_move(eng, y);
        /* Click §4.7.2: when a Leader moves to zp, its Followers must
         * follow — Followers live in their Leader's partition by
         * definition (§4.7.1). Walking eng->follower_head[y]'s chain
         * is the Followers-only edge index that L/F segregation makes
         * cheap (§4.7.4). */
        cp_move_followers(eng, y, zp);
        y = next;
    }
    if (eng->partitions[z]->on_worklist) {
        cp_wl_push(eng, zp);
    } else {
        cp_wl_push(eng, eng->partitions[z]->count <= eng->partitions[zp]->count
                        ? z : zp);
    }
}

/* Walk leader_idx's Followers chain (and their Followers, transitively)
 * and move each to target_part. The chain is maintained in O(1) by
 * cp_follower_link / cp_follower_unlink at every leader change. */
static void cp_move_followers(cp_engine_t* eng, int leader_idx, int target_part) {
    if (leader_idx < 0 || leader_idx >= eng->follower_head_cap) return;
    int f = eng->follower_head[leader_idx];
    while (f >= 0) {
        int next = eng->vnodes[f]->follower_next;
        if (eng->vnodes[f]->partition != target_part) {
            bool pend = cp_part_remove(eng, f);
            cp_part_add(eng, target_part, f);
            if (pend) cp_cprop_enqueue(eng, f);
        }
        cp_move_followers(eng, f, target_part);
        f = next;
    }
}

/* Is input position `pos` of vnode `y` the memory edge of a read whose CELL is
 * immutable (lat_is_array_data_cell: the array overlay's backing store, written
 * once at allocation and unnameable from Java)? Such a read's value is a pure
 * function of its receiver — the memory version says nothing about it. The edge
 * still exists, because the points-to transfer reads the heap map through it;
 * it just is not part of the value's IDENTITY, so CAUSE_SPLITS must not split
 * on it. Without this, `a.length` before and after ANY unrelated field store are
 * two incongruent values, and a bounds guard can never see its loop's bound. */
static bool cp_input_is_immutable_mem_edge(const cp_engine_t* eng,
                                           const cp_vnode_t* y, int pos) {
    if (!eng->sema || y->kind != CP_VN_EXPR || !y->expr) return false;
    if (y->expr->tag != SIR_GETFIELD) return false;
    if (pos != y->input_count - 1) return false;      /* the memory edge is last */
    return lat_is_array_data_cell(eng->sema, y->expr->get_field.class_id,
                                  y->expr->get_field.field_idx);
}

/* CAUSE_SPLITS (Click thesis §4.2): partition `x` may distinguish
 * other partitions. For each effective input position, gather the
 * nodes that use an x-member at that position, grouped by partition;
 * a partition only partly covered is split — its members no longer
 * agree on that input.
 *
 * Effective positions fold real positions 0 and 1 of commutative
 * binary users into one (§8.1.4), so `a + b` and `b + a` are not
 * separated by per-position scanning. A per-vnode epoch marker
 * (`touched_gen`) dedups when a commutative user references an
 * x-member at both real positions (e.g. `x + x`). */
static bool cp_phi_input_live(const cp_engine_t* eng,
                              const cp_vnode_t* v, int i);

static void cp_cause_splits(cp_engine_t* eng, int x, int max_inputs) {
    for (int eff = 0; eff < max_inputs; eff++) {
        int gen = ++eng->touched_gen;
        int* touched_parts = NULL;
        for (int n = eng->partitions[x]->head; n >= 0;
             n = eng->vnodes[n]->part_next) {
            for (int k = eng->du_off[n];
                 k < eng->du_off[n] + eng->du_cnt[n]; k++) {
                int y = eng->du_user[k];
                cp_vnode_t* yv = eng->vnodes[y];
                /* §4.7.4: CAUSE_SPLITS walks only L.def_use edges —
                 * Followers are excluded from the split machinery. */
                if (yv->leader >= 0) continue;
                int y_eff = eng->du_input[k];
                if (cp_input_is_immutable_mem_edge(eng, yv, y_eff)) continue;
                if (yv->kind == CP_VN_EXPR && yv->input_count == 2
                        && cp_op_is_commutative(yv->expr))
                    y_eff = 0;
                if (y_eff != eff) continue;
                /* Click §4.7.5 line 33-34: enqueue users whose fact
                 * depends on input partition (not just value). SUB/CMP
                 * fold via §4.6 (sub/cmp-of-congruent). PHI's §4.9
                 * Follower-status depends on input partitions — its
                 * revert check (line 6.1) fires inside PROPAGATE. */
                if (yv->kind == CP_VN_PHI
                        || (yv->kind == CP_VN_EXPR
                            && (yv->op == SIR_SUB || sir_tag_is_cmp(yv->op))))
                    cp_cprop_enqueue(eng, y);
                /* Click §4.4.1 line 6.5: skip TOP partition + skip
                 * PHI with dead input. Both conditions required. */
                if (!yv->type
                        || yv->type == type_top(&eng->pool)) continue;
                if (yv->kind == CP_VN_PHI
                        && !cp_phi_input_live(eng, yv, y_eff)) continue;
                if (yv->touched_gen == gen) continue;
                yv->touched_gen = gen;
                cp_partition_t* z = eng->partitions[yv->partition];
                if (z->touched_count == 0)
                    bbq_vec_push(touched_parts, yv->partition);
                yv->touched_next = z->touched_head;
                z->touched_head = y;
                z->touched_count++;
            }
        }
        for (int j = 0; j < (int)bbq_vec_len(touched_parts); j++) {
            int z = touched_parts[j];
            /* §4.7.5 step 43: split when touched != |Z.Leader|. */
            if (eng->partitions[z]->touched_count
                    != eng->partitions[z]->leader_count) {
                cp_split(eng, z);
            }
            eng->partitions[z]->touched_count = 0;
            eng->partitions[z]->touched_head  = -1;
        }
        bbq_vec_free(touched_parts);
    }
}

/* Refine the initial partitions into congruence classes. Every
 * SPLIT adds exactly one partition and there can be at most n, so
 * there are at most n-1 splits and the worklist drains — no
 * iteration cap, no abort. */
static void cp_refine(cp_engine_t* eng) {
    /* Click §4.2 / §4.4.2: CAUSE_SPLITS drains eng->worklist. Initial
     * seeding (all partitions) happens once via cp_init_facts; further
     * additions come from cp_split's smaller-half enqueue and from
     * §4.8 line 16.5's apply→worklist push. Re-seeding here would
     * cause cp_cause_splits to re-process every partition each outer
     * iteration — combined with the §4.7.5 line 33-34 cross-enqueue
     * (cause_splits → cprop), neither worklist would ever drain. */
    int max_inputs = 0;
    for (int v = 0; v < eng->vnode_count; v++)
        if (eng->vnodes[v]->input_count > max_inputs)
            max_inputs = eng->vnodes[v]->input_count;
    while (bbq_vec_len(eng->worklist)) {
        int x = eng->worklist[bbq_vec_len(eng->worklist) - 1];
        bbq__vec_hdr(eng->worklist)->len--;
        eng->partitions[x]->on_worklist = false;
        cp_cause_splits(eng, x, max_inputs);
    }
}

/* ── Fact propagation ────────────────────────────────────────── */

/* Whether φ node `v`'s input `i` arrives along a reachable path —
 * φ facts ignore contributors from dead arms (§4.3 / §4.4.1). The
 * predecessor's node-level reachability is necessary but not enough:
 * a Branch or Switch with a KNOWN-constant selector reaches φ's
 * merge from one specific successor edge only, so the other edges
 * from the same predecessor node are dead even though the node
 * itself is reachable. Check the predecessor's branch/switch fact
 * to decide whether THIS specific edge to v->phi_merge is live. */
static cp_const_t cp_cond_const(cp_engine_t* eng, sir_node_t* cond);
static bool cp_phi_input_live(const cp_engine_t* eng,
                              const cp_vnode_t* v, int i) {
    if (!eng->reachable || !v->phi_pred) return true;
    int p = v->phi_pred[i];
    if (p < 0) return true;                 /* method-start edge */
    if (!eng->reachable[p]) return false;
    sir_node_t* pred  = eng->spine[p];
    sir_node_t* merge = v->phi_merge;
    if (pred->tag == SIR_BRANCH) {
        cp_const_t c = cp_cond_const((cp_engine_t*)eng, pred->branch.cond);
        if (c.state == CP_C_TOP)   return false;     /* optimistic */
        if (c.state == CP_C_BOTTOM) return true;     /* both arms live */
        bool this_is_true_arm  = (pred->branch.on_true  == merge);
        bool this_is_false_arm = (pred->branch.on_false == merge);
        bool true_taken = (c.value != 0);
        return (this_is_true_arm  &&  true_taken)
            || (this_is_false_arm && !true_taken);
    }
    if (pred->tag == SIR_SWITCH) {
        cp_const_t c = cp_cond_const((cp_engine_t*)eng, pred->switch_.selector);
        if (c.state == CP_C_TOP)   return false;
        if (c.state == CP_C_BOTTOM) return true;
        int nc = pred->switch_.case_targets_count;
        int chosen = nc;                    /* default if no match */
        for (int j = 0; j < nc; j++)
            if (pred->switch_.case_values[j] == c.value) { chosen = j; break; }
        sir_node_t* live_succ = (chosen < nc)
                              ? pred->switch_.case_targets[chosen]
                              : pred->switch_.default_target;
        return live_succ == merge;
    }
    return true;
}

/* γ_T — the type-lattice fact for value node `v`, given the current
 * types of its inputs. Optimistic prelude (leader / opaque / φ meet) +
 * engine-specials (CHECKCAST discriminator-dependent, LoadLocal §4.7
 * COPY-Follower) + table-driven type_kind dispatch. The engine does
 * not switch on sir_node_t_tag for γ work outside the engine-special
 * set; the structural-traversal switches further down the file
 * (cp_expr_is_pure / cp_expr_uses / cp_rewrite_expr / cp_cse_walk)
 * read sir.asdl child-field layout, not γ classifications. */
static const Type* cp_node_type(cp_engine_t* eng, const cp_vnode_t* v) {
    /* §4.8 Follower: takes the Leader's type. */
    if (v->leader >= 0)
        return eng->vnodes[v->leader]->type;
    if (v->kind == CP_VN_OPAQUE)
        return v->opaque_type ? v->opaque_type : type_bottom(&eng->pool);
    if (v->kind == CP_VN_REFINE) {
        /* Refine forwards its input's type — refinement only narrows
         * the value lattice, not the type lattice. */
        return (v->input_count > 0 && v->inputs[0] >= 0)
             ? eng->vnodes[v->inputs[0]]->type
             : type_bottom(&eng->pool);
    }
    if (v->kind == CP_VN_PHI) {
        /* §4.9 PHI meet over live inputs — engine-special (no SIR tag). */
        const Type* t = type_top(&eng->pool);
        for (int i = 0; i < v->input_count; i++) {
            if (!cp_phi_input_live(eng, v, i)) continue;
            int in = v->inputs[i];
            if (in >= 0 && in < eng->vnode_count)
                t = type_meet(eng->sema, t, eng->vnodes[in]->type, &eng->pool);
        }
        return t;
    }
    sir_node_t* e = v->expr;
    if (!e || e->tag < 0 || e->tag >= SIR_TAG_COUNT)
        return type_bottom(&eng->pool);
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];

    /* GT_VIA_INPUT — LoadLocal §4.7 COPY-Follower forwards input[0]'s
     * type. The engine reads it from the vnode's resolved input. */
    if (g->type_kind == GT_VIA_INPUT) {
        int in = v->input_count > 0 ? v->inputs[0] : -1;
        return (in >= 0 && in < eng->vnode_count)
             ? eng->vnodes[in]->type
             : type_bottom(&eng->pool);
    }

    /* Everything else is pure γ_T — depends only on the node's
     * carried fields and sema. */
    return gamma_type_for_node(eng->sema, e, &eng->pool);
}

/* GCD on absolute values; gcd(x, 0) = |x|. Used by RANGE stride
 * computation in cp_const_meet — Click §4.5's "ranges with strides"
 * lattice is closed under meet via gcd of source strides and the
 * inter-base offset. */
static int64_t cp_gcd_i64(int64_t a, int64_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { int64_t t = a % b; a = b; b = t; }
    return a;
}

/* Snap `*hi` upward so (*hi - lo) % stride == 0, preserving the
 * over-approximation. Returns false if snap-up would overflow
 * INT32_MAX — caller drops to dense (stride = 1). */
static bool cp_range_snap_hi(int64_t lo, int64_t* hi, int64_t stride, cp_cwidth_t w) {
    if (stride <= 1) return true;
    int64_t span = *hi - lo;
    int64_t rem  = span % stride;
    if (rem == 0) return true;
    int64_t bump = stride - rem;
    if (*hi > cp_width_max(w) - bump) return false;
    *hi += bump;
    return true;
}

/* Meet on the constant lattice: TOP is the identity, BOTTOM
 * absorbing. KNOWN ∩ KNOWN = same value or expand to a strided
 * 2-point RANGE; KNOWN ∩ RANGE = the range expanded to include the
 * value with stride snapped accordingly; RANGE ∩ RANGE = pointwise
 * extremum with stride gcd-of-strides-and-offset (Click §4.5). REF
 * ∩ REF preserves the identity when matched and is BOTTOM otherwise
 * (no inclusion lattice between distinct refs, no widening). REF ∩
 * any other non-TOP is BOTTOM — primitives and references share no
 * lattice element. Widening is *not* applied here — cp_const_widen
 * wraps this meet at PHI joins where the value lattice's infinite
 * ascending chains would otherwise prevent termination. */
cp_const_t cp_const_meet(cp_const_t a, cp_const_t b) {
    if (a.state == CP_C_TOP) return b;
    if (b.state == CP_C_TOP) return a;
    if (a.state == CP_C_BOTTOM || b.state == CP_C_BOTTOM)
        return (cp_const_t){ .state = CP_C_BOTTOM };

    /* REF: identity-matching preserves; mismatch (or mixing with a
     * numeric state) drops to BOTTOM. */
    if (a.state == CP_C_REF || b.state == CP_C_REF) {
        if (a.state == CP_C_REF && b.state == CP_C_REF
            && a.ref_kind == b.ref_kind && a.ref_id == b.ref_id)
            return a;
        return (cp_const_t){ .state = CP_C_BOTTOM };
    }

    /* Floats have no range lattice: the meet is exact-or-BOTTOM. i64 shares
     * the range lattice with i32 (bounds keyed by cwidth), so it flows through
     * the same range join below. (Reachable only via same-typed operands.) */
    if (a.cwidth >= CP_W_F32 || b.cwidth >= CP_W_F32) {
        if (a.state == CP_C_KNOWN && b.state == CP_C_KNOWN && cp_const_eq(a, b))
            return a;
        return (cp_const_t){ .state = CP_C_BOTTOM };
    }
    cp_cwidth_t w = a.cwidth;  /* i32 or i64 */

    /* Normalize: read each input as a [lo, hi] range. KNOWN(k) is
     * [k, k] with no inherent stride; RANGE carries its bounds and
     * stride directly. stride_or_0 == 0 for KNOWN ("any stride
     * works"); cp_gcd treats 0 as identity. */
    int64_t a_lo = (a.state == CP_C_KNOWN) ? cp_known_i64(a) : a.lo;
    int64_t a_hi = (a.state == CP_C_KNOWN) ? cp_known_i64(a) : a.hi;
    int64_t b_lo = (b.state == CP_C_KNOWN) ? cp_known_i64(b) : b.lo;
    int64_t b_hi = (b.state == CP_C_KNOWN) ? cp_known_i64(b) : b.hi;
    int64_t a_s  = (a.state == CP_C_RANGE) ? a.stride : 0;
    int64_t b_s  = (b.state == CP_C_RANGE) ? b.stride : 0;

    int64_t lo = a_lo < b_lo ? a_lo : b_lo;
    int64_t hi = a_hi > b_hi ? a_hi : b_hi;

    /* Stride: gcd of source strides and the inter-base offset (which, at the
     * width extremes, can overflow — fall to dense then). */
    int64_t off;
    int64_t stride = __builtin_sub_overflow(a_lo, b_lo, &off)
                   ? 1 : cp_gcd_i64(cp_gcd_i64(a_s, b_s), off);
    if (stride <= 1) stride = 1;

    if (lo == hi)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                             .value = (int32_t)lo, .lvalue = lo };

    /* Canonicalize: snap hi upward to the stride boundary. If that
     * overflows, drop to dense. */
    if (!cp_range_snap_hi(lo, &hi, stride, w)) stride = 1;
    /* The symbolic upper bound survives a JOIN only if BOTH sides carry the same
     * one — the fact must hold on every incoming path. Otherwise it is dropped:
     * we would be claiming `< len` on a path that never proved it. Fail-closed;
     * losing the bound only means a guard we decline to eliminate.
     *
     * And a join takes the WEAKER inclusivity: if one path proved `i < B` and the other
     * only `i <= B`, all that holds afterwards is `i <= B`. (Intersection is the mirror —
     * there the STRICT one wins, because both facts hold at once.) */
    bool both = (a.state == CP_C_RANGE && b.state == CP_C_RANGE
                 && a.hi_vn1 && a.hi_vn1 == b.hi_vn1);
    int sym      = both ? a.hi_vn1 : 0;
    int sym_incl = both ? ((a.hi_vn_incl || b.hi_vn_incl) ? 1 : 0) : 0;
    /* The symbolic lower bound joins the same way — weaker inclusivity wins. */
    bool lboth = (a.state == CP_C_RANGE && b.state == CP_C_RANGE
                  && a.lo_vn1 && a.lo_vn1 == b.lo_vn1);
    int lsym      = lboth ? a.lo_vn1 : 0;
    int lsym_incl = lboth ? ((a.lo_vn_incl || b.lo_vn_incl) ? 1 : 0) : 0;
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = lo, .hi = hi,
                         .stride = stride, .hi_vn1 = sym, .hi_vn_incl = sym_incl,
                         .lo_vn1 = lsym, .lo_vn_incl = lsym_incl };
}

/* Static K-set: type boundaries the bitwidth folds care about, plus
 * 0 / ±1. Per-method literal K (the method's integer constants) is
 * unioned with this set at cp_build time and stored on eng->widen_k.
 * |K_static| = 9; total |K| grows with the method's literal count. */
static const int32_t cp_widen_k_static[] = {
    INT32_MIN, INT16_MIN, INT8_MIN, -1, 0, 1, INT8_MAX, INT16_MAX, INT32_MAX
};
#define CP_WIDEN_K_STATIC_N (sizeof(cp_widen_k_static) / sizeof(cp_widen_k_static[0]))

static int cp_int32_compare(const void* a, const void* b) {
    int32_t x = *(const int32_t*)a, y = *(const int32_t*)b;
    return (x > y) - (x < y);
}

/* Build the engine's widening K-set: union of static type-boundary
 * constants and the method's integer literals (every LoadConst's
 * value). Runs once after cp_enumerate — per Click §3.7 this is a
 * phase-ordering case (analysis reads K; nothing during analysis
 * mutates the LoadConst set), so the K can be precomputed and held
 * stable for the entire solve. */
static void cp_build_widen_k(cp_engine_t* eng) {
    int max_size = (int)CP_WIDEN_K_STATIC_N + eng->vnode_count;
    int32_t* k = (int32_t*)bbq_arena_alloc(eng->arena,
                                           (size_t)max_size * sizeof(int32_t));
    int n = 0;
    for (size_t i = 0; i < CP_WIDEN_K_STATIC_N; i++) k[n++] = cp_widen_k_static[i];
    for (int i = 0; i < eng->vnode_count; i++) {
        cp_vnode_t* v = eng->vnodes[i];
        if (v->kind == CP_VN_EXPR && v->expr && v->expr->tag == SIR_LOADCONST)
            k[n++] = v->expr->load_const.value;
    }
    qsort(k, (size_t)n, sizeof(int32_t), cp_int32_compare);
    int unique = 0;
    for (int i = 0; i < n; i++)
        if (i == 0 || k[i] != k[i-1]) k[unique++] = k[i];
    eng->widen_k = k;
    eng->widen_k_count = unique;
}

/* The best K element ≤ candidate (snaps downward); INT32_MIN if none
 * fits. Reads from the engine's merged K (static ∪ per-method
 * literals). */
static int32_t cp_widen_lower(const cp_engine_t* eng, int32_t candidate) {
    int32_t best = INT32_MIN;
    for (int i = 0; i < eng->widen_k_count; i++)
        if (eng->widen_k[i] <= candidate && eng->widen_k[i] > best)
            best = eng->widen_k[i];
    return best;
}

/* The best K element ≥ candidate (snaps upward); INT32_MAX if none
 * fits. */
static int32_t cp_widen_upper(const cp_engine_t* eng, int32_t candidate) {
    int32_t best = INT32_MAX;
    for (int i = 0; i < eng->widen_k_count; i++)
        if (eng->widen_k[i] >= candidate && eng->widen_k[i] < best)
            best = eng->widen_k[i];
    return best;
}

/* Widening operator at PHI meet: given the PHI's previous constant
 * fact and the freshly-meet'd value over its live inputs, produces a
 * K-bounded result that preserves termination on infinite ascending
 * chains. Per PoPA §4.2: if `new` doesn't exceed `old`, keep `old`;
 * if any bound of `new` exceeds the corresponding bound of `old`,
 * snap that bound to the next K element beyond the new bound (or to
 * ±∞ if no such K element exists). Cardinality of K bounds chain
 * length per vnode. */
cp_const_t cp_const_widen(const cp_engine_t* eng,
                          cp_const_t old, cp_const_t new_val) {
    if (old.state == CP_C_TOP) return new_val;
    if (new_val.state == CP_C_TOP) return old;
    if (old.state == CP_C_BOTTOM || new_val.state == CP_C_BOTTOM)
        return (cp_const_t){ .state = CP_C_BOTTOM };

    /* REF: identity-matching preserves; any other combination at PHI
     * drops to BOTTOM. REF doesn't widen — there is no inclusion
     * lattice between distinct identities. */
    if (old.state == CP_C_REF || new_val.state == CP_C_REF) {
        if (old.state == CP_C_REF && new_val.state == CP_C_REF
            && old.ref_kind == new_val.ref_kind
            && old.ref_id == new_val.ref_id)
            return old;
        return (cp_const_t){ .state = CP_C_BOTTOM };
    }

    /* Floats are KNOWN-or-BOTTOM (height-2, no ascending chains) — keep on
     * agreement, else BOTTOM. i64 shares the range lattice and is widened
     * below (its K-snap goes straight to the width extremes). */
    if (old.cwidth >= CP_W_F32 || new_val.cwidth >= CP_W_F32) {
        if (old.state == CP_C_KNOWN && new_val.state == CP_C_KNOWN
                && cp_const_eq(old, new_val))
            return new_val;
        return (cp_const_t){ .state = CP_C_BOTTOM };
    }
    cp_cwidth_t w = old.cwidth;  /* i32 or i64 */

    int64_t o_lo = (old.state == CP_C_KNOWN) ? cp_known_i64(old) : old.lo;
    int64_t o_hi = (old.state == CP_C_KNOWN) ? cp_known_i64(old) : old.hi;
    int64_t n_lo = (new_val.state == CP_C_KNOWN) ? cp_known_i64(new_val) : new_val.lo;
    int64_t n_hi = (new_val.state == CP_C_KNOWN) ? cp_known_i64(new_val) : new_val.hi;
    int64_t o_s  = (old.state == CP_C_RANGE) ? old.stride : 0;
    int64_t n_s  = (new_val.state == CP_C_RANGE) ? new_val.stride : 0;

    /* Lower: if new doesn't push lower than old, keep old.lo; otherwise snap
     * to the next K below n_lo — the int32 K-set for i32, the width extreme
     * for i64 (a coarser but still-terminating widening). Symmetric for hi. */
    int64_t lo = (o_lo <= n_lo) ? o_lo
               : (w == CP_W_I64 ? INT64_MIN : cp_widen_lower(eng, (int32_t)n_lo));
    int64_t hi = (n_hi <= o_hi) ? o_hi
               : (w == CP_W_I64 ? INT64_MAX : cp_widen_upper(eng, (int32_t)n_hi));

    /* Stride survives only when no bound moved and both inputs
     * agreed. A K-snap discards stride alignment — the K-set is
     * type-boundary literals, unlikely to land on stride boundaries
     * — so any actual widening collapses stride to 1. */
    bool widened = (lo != o_lo) || (hi != o_hi);
    int64_t stride = (!widened && o_s == n_s && o_s > 0) ? o_s : 1;

    if (lo == hi)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = w,
                             .value = (int32_t)lo, .lvalue = lo };
    if (!cp_range_snap_hi(lo, &hi, stride, w)) stride = 1;
    return (cp_const_t){ .state = CP_C_RANGE, .cwidth = w, .lo = lo, .hi = hi,
                         .stride = stride };
}

static bool cp_const_eq(cp_const_t a, cp_const_t b) {
    if (a.state != b.state) return false;
    if (a.state == CP_C_KNOWN) {
        if (a.cwidth != b.cwidth) return false;
        switch (a.cwidth) {
            case CP_W_I64: return a.lvalue == b.lvalue;
            /* Bit-compare floats: congruent ⇔ identical bits, so +0.0/-0.0
             * stay distinct and same-bit NaNs stay congruent (sound for VN).
             * Each width compares its own carrier — an f32's bits live in
             * fvalue, and NaN payloads differ below the double's resolution. */
            case CP_W_F32: return memcmp(&a.fvalue, &b.fvalue, sizeof a.fvalue) == 0;
            case CP_W_F64: return memcmp(&a.dvalue, &b.dvalue, sizeof a.dvalue) == 0;
            default:       return a.value == b.value;
        }
    }
    if (a.state == CP_C_RANGE)
        return a.lo == b.lo && a.hi == b.hi && a.stride == b.stride
            && a.hi_vn1 == b.hi_vn1 && a.lo_vn1 == b.lo_vn1;   /* both symbolic bounds are part of the fact */
    if (a.state == CP_C_REF)
        return a.ref_kind == b.ref_kind && a.ref_id == b.ref_id;
    return true;  /* TOP / BOTTOM */
}

/* Conservative purity classification — pure means the result is a
 * function of inputs alone, with no traps, allocations, or memory
 * reads. Carried from sir_opt.c's expr_is_pure verbatim. */
static bool cp_expr_is_pure(const sir_node_t* e) {
    if (!e) return true;
    if (e->tag < 0 || e->tag >= SIR_TAG_COUNT) return false;
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];
    if (g->is_leaf_pure) return true;
    if (!g->is_pure_if_children_pure) return false;
    /* Children-pure: recurse into every operand via the generic child
     * accessor — γ classifies the op, sir_support carries the shape. */
    int n = sir_arity(e);
    for (int i = 0; i < n; i++)
        if (!cp_expr_is_pure(sir_child(e, i))) return false;
    return true;
}

/* Absorbing constant for a binary opcode: when one operand is `k`,
 * the result is the value placed in `*out` regardless of the other
 * operand. The other operand's side effects (if any) must still
 * execute — the caller gates this on purity. yoctojc composition,
 * not vanilla Click §4.8 (the row's absorbing_side / _k / _result
 * carry the per-opcode data). */
static bool cp_absorbing_const(int op, int32_t k, int32_t* out) {
    if (op < 0 || op >= SIR_TAG_COUNT) return false;
    const sir_op_gamma_t* g = &sir_op_gamma[op];
    if (g->absorbing_side == GS_NONE) return false;
    if (k != g->absorbing_k) return false;
    *out = g->absorbing_result;
    return true;
}

/* Walk a LoadLocal's reaching-def chain to its ultimate producing
 * vnode — matches sir_opt.c's slot_vn-based reflexivity: a LoadLocal
 * has the same "value" as whatever its current reaching-def is, even
 * across opcode buckets (e.g. LoadLocal-reaching-LoadNull). Used at
 * the §4.6 fold site so the partition machinery doesn't need to be
 * mutated for every slot-collapse opportunity. */
static int cp_ultimate_value(cp_engine_t* eng, int vi) {
    for (int hops = 0; hops < 128; hops++) {
        if (vi < 0 || vi >= eng->vnode_count) return vi;
        cp_vnode_t* v = eng->vnodes[vi];
        if (v->kind != CP_VN_EXPR || !v->expr) return vi;
        if (v->expr->tag != SIR_LOADLOCAL || v->input_count != 1) return vi;
        int pi = v->inputs[0];
        if (pi == vi) return vi;
        vi = pi;
    }
    return vi;
}

/* The constant of an input value node. */
static cp_const_t cp_input_const(cp_engine_t* eng, int in) {
    cp_const_t bot = { .state = CP_C_BOTTOM };
    return (in >= 0 && in < eng->vnode_count) ? eng->vnodes[in]->constant : bot;
}

/* γ_K — the constant-propagation fact for value node `v`, given the
 * current constants of its inputs. Reads top-to-bottom in Click's
 * §-order: optimistic prelude (leader / opaque / φ), value-bearing
 * leaves (LoadConst / LoadLocal), §4.8 absorbing-with-purity, §4.6
 * fold-of-congruent, §3.2.1 vanilla fold. Per-opcode facts come from
 * sir_op_gamma[]; the engine never switches on sir_node_t_tag for γ
 * work outside the engine-special set (PHI, LoadConst, LoadLocal). */
/* SIR data-type tag → constant-lattice carrier width. v128 deliberately has NO
 * carrier: no γ_K fold produces a v128 KNOWN (SIMD ops fold nothing — a wrong
 * fold is a miscompile), SimdConst carries no fold row and is not congruent, so
 * a v128 cell only ever holds TOP/BOTTOM and its width is never read. If SIMD
 * folding ever lands, it needs a real 128-bit carrier here FIRST — lanes never
 * ride a narrower one (the f32-in-double lesson). */
static cp_cwidth_t cp_dt_cwidth(sir_datatype_t dt) {
    switch (dt) {
        case SIR_DTLONG:   return CP_W_I64;
        case SIR_DTFLOAT:  return CP_W_F32;
        case SIR_DTDOUBLE: return CP_W_F64;
        default:           return CP_W_I32;   /* byte/short/char/int/ref */
    }
}

/* Fold a wide-typed (i64 / f32 / f64) node from KNOWN wide operands —
 * the value-model counterpart of the int32 γ-folds. Arithmetic/bitwise/
 * shift results keep the operand width; the six comparisons read wide
 * operands and yield a KNOWN i32 0/1 (C's float relations already give
 * Java's NaN rule: all false except != → true). Defined-wraparound for
 * integer arith, IEEE for float (so float /0 → ±inf/NaN, not a fold
 * failure). Returns BOTTOM where the op doesn't fold (i64 /0 or the
 * INT64_MIN/-1 overflow). */
static cp_const_t cp_fold_wide(int tag, cp_const_t a, cp_const_t b) {
    cp_const_t bot = { .state = CP_C_BOTTOM };
    cp_cwidth_t w = a.cwidth;

    switch (tag) {
        case SIR_EQ: case SIR_NE: case SIR_LT:
        case SIR_LE: case SIR_GT: case SIR_GE: {
            int r;
            if (w == CP_W_I64) {
                int64_t x = a.lvalue, y = b.lvalue;
                r = tag==SIR_EQ ? x==y : tag==SIR_NE ? x!=y : tag==SIR_LT ? x<y :
                    tag==SIR_LE ? x<=y : tag==SIR_GT ? x>y : x>=y;
            } else if (w == CP_W_F32) {
                float x = cp_known_f32(a), y = cp_known_f32(b);
                r = tag==SIR_EQ ? x==y : tag==SIR_NE ? x!=y : tag==SIR_LT ? x<y :
                    tag==SIR_LE ? x<=y : tag==SIR_GT ? x>y : x>=y;
            } else {
                double x = cp_known_f64(a), y = cp_known_f64(b);
                r = tag==SIR_EQ ? x==y : tag==SIR_NE ? x!=y : tag==SIR_LT ? x<y :
                    tag==SIR_LE ? x<=y : tag==SIR_GT ? x>y : x>=y;
            }
            return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_I32, .value = r };
        }
        default: break;
    }

    if (w == CP_W_I64) {
        int64_t x = a.lvalue, y = b.lvalue, r;
        switch (tag) {
            case SIR_ADD: r = (int64_t)((uint64_t)x + (uint64_t)y); break;
            case SIR_SUB: r = (int64_t)((uint64_t)x - (uint64_t)y); break;
            case SIR_MUL: r = (int64_t)((uint64_t)x * (uint64_t)y); break;
            /* §15.16.2/.3: /0 THROWS (no fold); MIN/-1 does NOT — it is the
             * dividend, and MIN%-1 is 0 (both C UB: fold by the spec's stated
             * values, never by computing). */
            case SIR_DIV: if (y==0) return bot;
                          r = (x==INT64_MIN && y==-1) ? INT64_MIN : x/y; break;
            case SIR_REM: if (y==0) return bot;
                          r = (x==INT64_MIN && y==-1) ? 0 : x%y; break;
            case SIR_AND: r = x & y; break;
            case SIR_OR:  r = x | y; break;
            case SIR_XOR: r = x ^ y; break;
            case SIR_SHL: r = (int64_t)((uint64_t)x << ((uint64_t)y & 63)); break;
            case SIR_SHR: r = x >> ((uint64_t)y & 63); break;
            case SIR_USHR:r = (int64_t)((uint64_t)x >> ((uint64_t)y & 63)); break;
            case SIR_NEG: r = (int64_t)(0u - (uint64_t)x); break;
            default: return bot;
        }
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_I64, .lvalue = r };
    }

    if (w == CP_W_F32) {
        /* JLS §15.17: float arithmetic is float-precision. Computing in double
         * and rounding back double-rounds (e.g. division), and would launder a
         * NaN's payload through the wider format — so compute in float. */
        float x = cp_known_f32(a), y = cp_known_f32(b), r;
        switch (tag) {
            case SIR_ADD: r = x + y; break;
            case SIR_SUB: r = x - y; break;
            case SIR_MUL: r = x * y; break;
            case SIR_DIV: r = x / y; break;
            case SIR_REM: r = fmodf(x, y); break;  /* Java frem = truncated remainder */
            case SIR_NEG: r = -x; break;
            default: return bot;
        }
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_F32, .fvalue = r };
    }

    {  /* f64 */
        double x = cp_known_f64(a), y = cp_known_f64(b), r;
        switch (tag) {
            case SIR_ADD: r = x + y; break;
            case SIR_SUB: r = x - y; break;
            case SIR_MUL: r = x * y; break;
            case SIR_DIV: r = x / y; break;
            case SIR_REM: r = fmod(x, y); break;   /* Java drem = truncated remainder */
            case SIR_NEG: r = -x; break;
            default: return bot;
        }
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_F64, .dvalue = r };
    }
}

/* §2's `classOf(O) ≤ τ`, tri-state — defined with the other Obj queries, below. */
static int cp_obj_isa(const cp_engine_t* eng, int o, sir_atype_t atype, int target);

/* Nullability (lattice C) → a VALUE fact, which is what makes the §15 NPE guards
 * fall out for free. A null test whose answer pts already knows folds to that
 * answer, and from there the machinery that was always here does the rest:
 * cp_rewrite_branch_fold re-tags a constant-condition Branch as a Nop to the
 * taken successor, the throw arm becomes unreachable, and its exception
 * allocation and Throw are dropped. No new rewrite, no guard-shape matching.
 *
 * Note this does NOT violate the rule that pts must not feed congruence: what reaches
 * cp_split_by_facts is the CONSTANT, and the constant is a true value fact — the
 * comparison really does evaluate to 0. pts itself is never a congruence key.
 *
 * Monotone: pts only grows. ∅ ⟹ TOP (no information yet — stay optimistic); a
 * set without ⊥null ⟹ NonNull; exactly {⊥null} ⟹ Null; anything else ⟹ BOTTOM.
 * If ⊥null later joins the set, the fact descends KNOWN → BOTTOM, which is the
 * lattice's own direction of travel. */
static cp_const_t cp_null_compare_const(cp_engine_t* eng, const cp_vnode_t* v,
                                         bool* handled) {
    *handled = false;
    cp_const_t r = { .state = CP_C_TOP };
    if (v->kind != CP_VN_EXPR || !v->expr || v->input_count != 2) return r;
    int tag = v->expr->tag;
    if (tag != SIR_EQ && tag != SIR_NE) return r;
    sir_node_t* l = sir_child(v->expr, 0);
    sir_node_t* rr = sir_child(v->expr, 1);
    if (!l || !rr) return r;
    int other;
    if (l->tag == SIR_LOADNULL && rr->tag != SIR_LOADNULL)      other = v->inputs[1];
    else if (rr->tag == SIR_LOADNULL && l->tag != SIR_LOADNULL) other = v->inputs[0];
    else return r;                       /* null==null, or neither side is null */
    if (other < 0 || other >= eng->vnode_count) return r;

    cp_pts_t p = eng->vnodes[other]->pts;
    if (cp_pts_empty(eng, p)) return r;                  /* ⊥: not yet known */
    *handled = true;
    bool may_be_null = cp_pts_has(eng, p, CP_OBJ_NULL);
    bool must_be_null = may_be_null && cp_pts_count(eng, p) == 1;
    int eq;                                              /* the value of `x == null` */
    if (!may_be_null)      eq = 0;                       /* provably NON-null */
    else if (must_be_null) eq = 1;                       /* provably null */
    else { r.state = CP_C_BOTTOM; return r; }            /* may be either */
    r.state  = CP_C_KNOWN;
    r.cwidth = CP_W_I32;
    r.value  = (tag == SIR_EQ) ? eq : !eq;
    return r;
}

/* The class element (lattice B) → a VALUE fact, the same way nullability is one.
 *
 * `x instanceof τ` is decided by pts: if EVERY object x may name is provably a τ, the
 * test is 1; if every one provably is not, it is 0. Then the machinery that was always
 * here does the rest — a constant condition folds the Branch, the dead arm becomes
 * unreachable, and its ClassCastException allocation and Throw are dropped.
 *
 * THIS IS WHY IT IS A TRANSFER AND NOT A REWRITE PASS. A folded branch is not a
 * conclusion, it is a FACT: it kills an edge, and killing an edge inside the fixpoint
 * drops the values that arm contributed to every downstream φ — which makes more things
 * constant, more things NonNull, more calls monomorphic, and so on around. That mutual
 * enabling IS the combined analysis (spec §9; Click's thesis). Proving the same thing
 * after the solve has finished throws every bit of it away — the fixpoint never learns
 * the arm is dead. `cp_null_compare_const` above is the same shape, and the DIV_OVERFLOW
 * arm needed no consumer AT ALL because the range lattice's transfer already answered it.
 *
 * That rule is intact for the same reason it is there: what reaches cp_split_by_facts is the
 * CONSTANT — a true value fact, since the comparison really does evaluate to that — and
 * never pts itself.
 *
 * Monotone: pts only grows, so this can only descend KNOWN → BOTTOM (a new object of an
 * unrelated class joining the set), which is the lattice's own direction of travel. ∅ ⟹
 * TOP: no information yet, stay optimistic. */
static cp_const_t cp_instanceof_const(cp_engine_t* eng, const cp_vnode_t* v,
                                       bool* handled) {
    *handled = false;
    cp_const_t r = { .state = CP_C_TOP };
    if (v->kind != CP_VN_EXPR || !v->expr || v->expr->tag != SIR_INSTANCEOF) return r;
    if (v->input_count < 1 || v->inputs[0] < 0) return r;
    cp_pts_t p = eng->vnodes[v->inputs[0]]->pts;
    if (cp_pts_empty(eng, p)) return r;            /* ⊥: nothing known yet */

    *handled = true;
    sir_atype_t at = v->expr->instance_of.atype;
    int cls = v->expr->instance_of.class_id;
    bool any_yes = false, any_no = false;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            int isa = cp_obj_isa(eng, o, at, cls);   /* 1 yes, 0 no, -1 unknown */
            if (isa < 0) { r.state = CP_C_BOTTOM; return r; }   /* unknown ⟹ undecidable */
            if (isa) any_yes = true; else any_no = true;
            if (any_yes && any_no) { r.state = CP_C_BOTTOM; return r; }
        }
    }
    r.state  = CP_C_KNOWN;
    r.cwidth = CP_W_I32;
    r.value  = any_yes ? 1 : 0;    /* §15.20.2 gives a boolean; ⊥null answers 0 */
    return r;
}

/* The range element's SYMBOLIC bound (lattice D) → a VALUE fact. The §15 upper-bounds
 * guard tests `i >= a.length`; the enclosing `i < a.length` put a symbolic upper bound on
 * i (the bounding node itself — no interval can say "less than that value"), and if that
 * bound is THE SAME VALUE as the length this guard reads, `i >= len` is false.
 *
 * "The same value" is GVN's question and GVN answers it: one partition. No dominance, no
 * walk — the two `a.length` reads are congruent because the array is congruent and the
 * cell is immutable (§10.7).
 *
 * A TRANSFER, not a pass, for the reason spec §9 gives: "dead regions never get analyzed
 * because the executable-edge flag gates every sub-lattice". Folding this after the solve
 * leaves the AIOOBE throw arm LIVE for the whole solve — its `new ArrayIndexOutOfBounds`
 * is then a real allocation site in pts, and its values join every downstream φ, poisoning
 * the very facts the other consumers need. Proving it here kills the arm inside the
 * fixpoint, and nothing in it is ever analyzed.
 *
 * PARTITION-DEPENDENCE — the trap, and it is the re-arm bug class again. This fact is not
 * a function of this node's def-use INPUTS alone: the bounding node arrives through the
 * range's `hi_vn1`, which is OFF the def-use graph, and the answer depends on PARTITIONS,
 * which refine as the solve runs. Partitions start coarse and SPLIT, so an early "same
 * partition" can become false — and a KNOWN 0 left standing on a split-apart pair is
 * UNSOUND, not merely stale. cp_solve therefore re-arms every comparison whenever the
 * partition count moves (see cp_rearm_partition_consumers). Monotone either way: the fact
 * can only descend KNOWN → BOTTOM, and a revived arm only ever ADDS reachability. */
static cp_const_t cp_symbolic_bound_const(cp_engine_t* eng, const cp_vnode_t* v,
                                           bool* handled) {
    *handled = false;
    cp_const_t r = { .state = CP_C_TOP };
    if (v->kind != CP_VN_EXPR || !v->expr) return r;
    if (v->expr->tag == SIR_GT) {
        /* The Mem hi-guard's shape: `x > lim` with x carrying a symbolic
         * bound B. Either strictness refutes it when B ≡ lim — x ≤ B = lim
         * and x < B = lim both deny x > lim — so no inclusive +1 dance.
         * fold_convert leaves RANGE operands unfolded (BOTTOM), so when x is
         * the I2L the guard compares through, the bound lives on the
         * conversion's OPERAND vnode. The two `lim` expressions are congruent
         * across adjacent guards through the MemSize memory-input keying —
         * and NOT congruent across a grow/call kill, which is what keeps the
         * soundness negatives standing. */
        if (v->input_count != 2 || v->inputs[0] < 0 || v->inputs[1] < 0) return r;
        const cp_vnode_t* xv = eng->vnodes[v->inputs[0]];
        cp_const_t xc = xv->constant;
        if (!(xc.state == CP_C_RANGE && xc.hi_vn1 != 0)
                && xv->kind == CP_VN_EXPR && xv->expr && xv->expr->tag == SIR_I2L
                && xv->input_count >= 1 && xv->inputs[0] >= 0
                && xv->inputs[0] < eng->vnode_count)
            xc = eng->vnodes[xv->inputs[0]]->constant;
        if (xc.state != CP_C_RANGE || xc.hi_vn1 == 0) return r;
        int gbound = cp_ultimate_value(eng, xc.hi_vn1 - 1);
        int glim   = cp_ultimate_value(eng, v->inputs[1]);
        if (gbound < 0 || gbound >= eng->vnode_count) return r;
        if (glim   < 0 || glim   >= eng->vnode_count) return r;
        int gbp = eng->vnodes[gbound]->partition;
        int glp = eng->vnodes[glim]->partition;
        if (gbp < 0 || glp < 0 || gbp != glp) return r;
        *handled = true;
        r.state  = CP_C_KNOWN;
        r.cwidth = CP_W_I32;
        r.value  = 0;
        return r;
    }
    if (v->expr->tag == SIR_LT) {
        /* The mirror of the SIR_GT case, on the symbolic LOWER bound: `x < lim`
         * with x carrying lo_vn1 = B. Either strictness refutes it when B ≡ lim —
         * x >= B = lim and x > B = lim both deny x < lim — so no inclusive dance.
         * This is what `x == y` feeds: on the true edge x >= y, so a `x < y` guard
         * (or any `x < L` with L ≡ y) is false. Same partition-dependence, same
         * re-arm — it is one more read of the carried fact, not a walk. */
        if (v->input_count != 2 || v->inputs[0] < 0 || v->inputs[1] < 0) return r;
        cp_const_t xc = eng->vnodes[v->inputs[0]]->constant;
        if (xc.state != CP_C_RANGE || xc.lo_vn1 == 0) return r;
        int gbound = cp_ultimate_value(eng, xc.lo_vn1 - 1);
        int glim   = cp_ultimate_value(eng, v->inputs[1]);
        if (gbound < 0 || gbound >= eng->vnode_count) return r;
        if (glim   < 0 || glim   >= eng->vnode_count) return r;
        int gbp = eng->vnodes[gbound]->partition;
        int glp = eng->vnodes[glim]->partition;
        if (gbp < 0 || glp < 0 || gbp != glp) return r;
        *handled = true;
        r.state  = CP_C_KNOWN;
        r.cwidth = CP_W_I32;
        r.value  = 0;
        return r;
    }
    if (v->expr->tag != SIR_GE) return r;
    if (v->input_count != 2 || v->inputs[0] < 0 || v->inputs[1] < 0) return r;

    const cp_vnode_t* iv = eng->vnodes[v->inputs[0]];          /* the index  */
    if (iv->constant.state != CP_C_RANGE || iv->constant.hi_vn1 == 0) return r;

    int bound_vn = cp_ultimate_value(eng, iv->constant.hi_vn1 - 1);
    int len_vn   = cp_ultimate_value(eng, v->inputs[1]);
    if (bound_vn < 0 || bound_vn >= eng->vnode_count) return r;
    if (len_vn   < 0 || len_vn   >= eng->vnode_count) return r;
    int bp = eng->vnodes[bound_vn]->partition;
    int lp = eng->vnodes[len_vn]->partition;
    if (bp < 0 || lp < 0) return r;

    if (!iv->constant.hi_vn_incl) {
        /* STRICT: `i < B` and `B ≡ len` ⟹ `i >= len` is FALSE. */
        if (bp != lp) return r;
    } else {
        /* INCLUSIVE: `i <= B` is `i < B+1`, so this proves `i >= len` false exactly when
         * `len ≡ B+1` — NOT when `len ≡ B`, where i reaches len and the read is genuinely
         * out of bounds (`new int[n]` with `i <= n` must keep its guard).
         *
         * Ask the VALUE GRAPH whether len IS B+1: len's own definition, congruent to an
         * Add of the bound and 1. In the shape this exists for — `new int[n+1]` with
         * `i <= n` — the array's length IS that Add node (the §10.7 array-length identity
         * gives `(new T[k]).length ≡ k`), so GVN has already put them in one partition.
         * This is arithmetic on the value graph, not a search for a lowering's shape. The
         * LEADER, not the ultimate copy: `a.length` is a FOLLOWER of the allocation's size
         * expression (§10.7's identity), and that expression is the Add. */
        int len_leader = cp_value_leader(eng, v->inputs[1]);
        if (len_leader < 0 || len_leader >= eng->vnode_count) return r;
        const sir_node_t* le = eng->vnodes[len_leader]->expr;
        if (!le || le->tag != SIR_ADD) return r;
        int a0 = cp_vnode_of(eng, sir_child((sir_node_t*)le, 0));
        int a1 = cp_vnode_of(eng, sir_child((sir_node_t*)le, 1));
        if (a0 < 0 || a1 < 0) return r;
        const cp_const_t c0 = eng->vnodes[a0]->constant;
        const cp_const_t c1 = eng->vnodes[a1]->constant;
        int other = -1;
        if (c1.state == CP_C_KNOWN && c1.cwidth == CP_W_I32 && c1.value == 1)
            other = cp_ultimate_value(eng, a0);
        else if (c0.state == CP_C_KNOWN && c0.cwidth == CP_W_I32 && c0.value == 1)
            other = cp_ultimate_value(eng, a1);
        if (other < 0 || other >= eng->vnode_count) return r;
        if (eng->vnodes[other]->partition != bp) return r;   /* len is not B+1 ⟹ keep */
    }

    *handled = true;
    r.state  = CP_C_KNOWN;
    r.cwidth = CP_W_I32;
    r.value  = 0;
    return r;
}

static bool cp_invoke_ret_const(cp_engine_t* eng, const sir_node_t* call, cp_const_t* out);

static cp_const_t cp_node_const(cp_engine_t* eng, const cp_vnode_t* v) {
    cp_const_t top = { .state = CP_C_TOP };
    cp_const_t bot = { .state = CP_C_BOTTOM };
    /* §4.8 Follower: takes the Leader's constant. */
    if (v->leader >= 0)
        return eng->vnodes[v->leader]->constant;
    {   /* The reference lattices, decided as VALUE facts so the fixpoint can use them:
         * nullability (§4) and the class element (§3). Both fold their guard's Branch
         * from inside the solve, which is what lets the dead arm prune the graph while
         * the other lattices are still moving. */
        bool handled = false;
        cp_const_t nc = cp_null_compare_const(eng, v, &handled);
        if (handled) return nc;
        cp_const_t ic = cp_instanceof_const(eng, v, &handled);
        if (handled) return ic;
        cp_const_t sb = cp_symbolic_bound_const(eng, v, &handled);
        if (handled) return sb;
    }
    if (v->kind == CP_VN_OPAQUE) {
        /* §5-D induction: an inc-def opaque carries `input(inputs[0]) + inc_delta` as a
         * pure analysis transfer. A plain seed opaque (no input) is BOTTOM, as before.
         * Type-general via SIR_ADD's cwidth-keyed range fold (i32 and i64 alike). */
        if (v->input_count > 0 && v->inputs[0] >= 0) {
            cp_const_t a = cp_input_const(eng, v->inputs[0]);
            if (a.state == CP_C_TOP) return top;
            if (a.state == CP_C_KNOWN) {
                int64_t k = cp_known_i64(a);
                a.state = CP_C_RANGE; a.lo = k; a.hi = k; a.stride = 1;
            }
            if (a.state == CP_C_RANGE) {
                cp_const_t d; memset(&d, 0, sizeof d);
                d.state = CP_C_RANGE; d.cwidth = a.cwidth;
                d.lo = v->inc_delta; d.hi = v->inc_delta; d.stride = 1;
                const sir_op_gamma_t* g = &sir_op_gamma[SIR_ADD];
                if (g->fold_binary_range) {
                    cp_const_t r = g->fold_binary_range(a, d);
                    /* The inc's value is a RANGE fact for the range lattice
                     * (guard elimination), never a scalar constant. The shared
                     * interval fold collapses a singleton [k,k] to CP_C_KNOWN;
                     * re-expand it so cp_const_subst never value-substitutes an
                     * inc-defined slot read — that would detach the counter from
                     * its slot and break the BURG Inc(LoadLocal) lowering. */
                    if (r.state == CP_C_KNOWN) {
                        int64_t k = cp_known_i64(r);
                        cp_const_t rr; memset(&rr, 0, sizeof rr);
                        rr.state = CP_C_RANGE; rr.cwidth = r.cwidth;
                        rr.lo = k; rr.hi = k; rr.stride = 1;
                        return rr;
                    }
                    return r;
                }
            }
        }
        return bot;
    }
    if (v->kind == CP_VN_REFINE) {
        /* PoPA Ch.6 refinement: intersect the underlying value's
         * constant with the per-arm predicate. The rewired LoadLocal
         * Followers in the arm subtree read this Refine's constant
         * via Click §4.7 COPY-Follower. */
        if (v->input_count == 0) return bot;
        cp_const_t base = cp_input_const(eng, v->inputs[0]);
        cp_const_t pred = v->refine_predicate;
        /* §5 "branch refinement on < narrows the taken edge": a STRICT `i < B` on a width-W
         * integer gives i ≤ W_MAX - 1 REGARDLESS of B, because B ≤ W_MAX. That is a pure type
         * CONSTANT — no read of B's fact (which would be a §8 off-def-use-graph read) — and it
         * is exactly what keeps a widened counter's `i+1` from wrapping W_MAX to W_MIN (the wrap
         * that drags lo to MIN). Inclusive `i <= B` can reach W_MAX, so it gets no numeric
         * tightening. hi_vn1 is preserved for the §15 guard consumer (a consumer may read the
         * bound's partition; this transfer may not). */
        if (pred.state == CP_C_RANGE && pred.hi_vn1 > 0 && !pred.hi_vn_incl) {
            int64_t nhi = (pred.cwidth == CP_W_I64) ? INT64_MAX - 1 : INT32_MAX - 1;
            if (nhi < pred.hi) pred.hi = nhi;
        }
        return cp_const_intersect(base, pred);
    }
    if (v->kind == CP_VN_PHI) {
        /* Meet over live inputs gives the natural join. Widening against the previous
         * value bounds the infinite ascending chains the RANGE state introduces (PoPA
         * §4.2: widening at the joining step makes the ascending chains finite, ≤ |K|+1
         * per bound, so PROPAGATE terminates).
         *
         * Spec §5/§8: widening belongs on the LOOP BACK EDGE, and which merges are loop
         * headers is READ FROM THE SIDECAR — the DDCG built the loop and recorded its
         * header — never recovered with a dominator-based natural-loop finder.
         *
         * Only a cycle can ascend forever, and every cycle's entry is a loop header, so
         * widening at the headers alone still terminates. Widening at an ORDINARY join
         * as well (which is what this did) is sound but throws precision away for
         * nothing: `c ? 1 : 100` would snap to the K bounds although it can never grow.
         *
         * FAIL-CLOSED in every direction where the record does not answer: no scopes
         * recorded at all (a hand-built SIR — the unit harness, which has loops but no
         * DDCG), or a merge with no row, and we widen as before. Never assume a cycle is
         * absent because nobody mentioned it. */
        cp_const_t fresh = top;
        for (int i = 0; i < v->input_count; i++)
            if (cp_phi_input_live(eng, v, i))
                fresh = cp_const_meet(fresh, cp_input_const(eng, v->inputs[i]));
        int m = v->phi_merge ? cp_spine_index((cp_engine_t*)eng, v->phi_merge) : -1;
        bool widen = !eng->any_scope_recorded         /* nothing recorded: widen */
                  || m < 0 || m >= eng->merge_rows    /* unrecorded merge: widen */
                  || !eng->is_loop_header             /* not indexed yet:  widen */
                  || eng->is_loop_header[m];          /* the spec's rule */
        return widen ? cp_const_widen(eng, v->constant, fresh) : fresh;
    }
    sir_node_t* e = v->expr;
    if (e->tag == SIR_LOADCONST) {
        cp_const_t k = { .state = CP_C_KNOWN, .value = e->load_const.value };
        return k;
    }
    if (e->tag == SIR_LOADLONGCONST)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_I64,
                             .lvalue = e->load_long_const.value };
    if (e->tag == SIR_LOADFLOATCONST)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_F32,
                             .fvalue = e->load_float_const.value };
    if (e->tag == SIR_LOADDOUBLECONST)
        return (cp_const_t){ .state = CP_C_KNOWN, .cwidth = CP_W_F64,
                             .dvalue = e->load_double_const.value };
    if (e->tag == SIR_LOADLOCAL)
        return v->input_count > 0 ? cp_input_const(eng, v->inputs[0]) : bot;
    /* An array's length is never negative (§15.10.1 traps a negative dimension
     * before the array exists), so `a.length` is [0, INT_MAX]. */
    if (e->tag == SIR_ARRAYLENGTH)
        return (cp_const_t){ .state = CP_C_RANGE, .cwidth = CP_W_I32,
                             .lo = 0, .hi = INT32_MAX, .stride = 1 };
    /* Pointer-constant identity sources (Click thesis §8). New gives a
     * unique identity per allocation site (the vnode's own index).
     * GetStatic on a final-static field gives a stable identity per
     * (class, field) since the field can't be reassigned. Non-final
     * statics can be rewritten at runtime, so no stable REF. Consumer
     * is aliasing precision inside the memory analysis. */
    if (e->tag == SIR_NEW) {
        void* f = cp_pmap_get(&eng->expr_idx, e);
        return (cp_const_t){ .state = CP_C_REF, .ref_kind = CP_REF_NEW,
                             .ref_id = (uint32_t)((uintptr_t)f - 1) };
    }
    if (e->tag == SIR_GETSTATIC && eng->sema) {
        const sema_class_t* cls = sema_get_class(eng->sema, e->get_static.class_id);
        int fi = e->get_static.field_idx;
        if (cls && fi >= 0 && fi < (int)bbq_vec_len((void*)cls->fields)
                && (cls->fields[fi].modifiers & ACC_FINAL)
                && (cls->fields[fi].modifiers & ACC_STATIC)) {
            return (cp_const_t){ .state = CP_C_REF, .ref_kind = CP_REF_STATIC,
                                 .ref_id = ((uint32_t)e->get_static.class_id << 16)
                                         | (uint32_t)(fi & 0xFFFF) };
        }
    }
    /* §7.2's VALUE half: a call's result carries its callee-set's exported return
     * const — the summary is fixed during this solve (a constant transfer, monotone). */
    if (e->tag == SIR_INVOKESTATIC || e->tag == SIR_INVOKESPECIAL
            || e->tag == SIR_INVOKEVIRTUAL || e->tag == SIR_INVOKEINTERFACE) {
        cp_const_t rc;
        if (cp_invoke_ret_const((cp_engine_t*)eng, e, &rc)) return rc;
    }

    /* Absorbing-element fold (sir_opt.c behavior, NOT §4.8): one
     * operand is the absorbing constant for `op` and the other is
     * pure → result is the absorbing constant. The purity gate
     * keeps an impure operand executing. */
    if (v->input_count == 2 && e) {
        int li = v->inputs[0], ri = v->inputs[1];
        cp_const_t lc = cp_input_const(eng, li);
        cp_const_t rc = cp_input_const(eng, ri);
        int32_t absorb;
        if (lc.state == CP_C_KNOWN && lc.cwidth == CP_W_I32
                && cp_absorbing_const(e->tag, lc.value, &absorb)
                && ri >= 0 && cp_expr_is_pure(eng->vnodes[ri]->expr)) {
            cp_const_t k = { .state = CP_C_KNOWN, .value = absorb }; return k;
        }
        if (rc.state == CP_C_KNOWN && rc.cwidth == CP_W_I32
                && cp_absorbing_const(e->tag, rc.value, &absorb)
                && li >= 0 && cp_expr_is_pure(eng->vnodes[li]->expr)) {
            cp_const_t k = { .state = CP_C_KNOWN, .value = absorb }; return k;
        }
    }

    /* §4.6 fold-of-congruent. Two operands compute the same value when
     * their ultimate reaching-defs match (slot-collapse aliasing) or
     * when their partitions agree (full congruence). The opcode's row
     * declares the cong-fold shape (ZERO for SUB; CMP_REFLEXIVE for
     * CMP — engine reads e->cmp.op to pick 0/1). Monotone: partitions
     * only ever refine, so "congruent" can only turn false and the
     * fact only falls.
     *
     * Partition agreement alone is not enough when the partition
     * hasn't been refined yet: every LoadConst starts life in the
     * same LOADCONST opcode-bucket (cp_opcode_key keys only by tag),
     * so e.g. Cmp(LoadConst 5, LoadConst 3) would transiently see
     * its operands as "same partition" and §4.6-fold to 0 — wrong.
     * cp_split_by_facts separates LoadConsts by value on the next
     * iteration, but cp_compute_facts runs first. If the operands'
     * KNOWN constants disagree, they can't be congruent at convergence;
     * reject the fold so the fall-through binary fold computes the
     * correct value. */
    {
        const sir_op_gamma_t* gcong = &sir_op_gamma[e->tag];
        if (gcong->cong_fold != GC_NONE && v->input_count == 2) {
            int li = cp_ultimate_value(eng, v->inputs[0]);
            int ri = cp_ultimate_value(eng, v->inputs[1]);
            bool same = (li >= 0 && ri >= 0 &&
                         (li == ri ||
                          (li < eng->vnode_count && ri < eng->vnode_count &&
                           eng->vnodes[li]->partition == eng->vnodes[ri]->partition)));
            if (same && li != ri) {
                cp_const_t lc = eng->vnodes[li]->constant;
                cp_const_t rc = eng->vnodes[ri]->constant;
                /* Premature-fold guard: see comment above. */
                if (lc.state == CP_C_TOP || rc.state == CP_C_TOP)
                    same = false;
                else if (lc.state == CP_C_KNOWN && rc.state == CP_C_KNOWN
                        && !cp_const_eq(lc, rc))
                    same = false;
            }
            /* §15.20.1/§15.19.1: NaN breaks reflexivity — NaN != NaN
             * is TRUE, NaN == NaN false, NaN - NaN is NaN. The x⊙x
             * folds are sound only for integral/ref operands; skip
             * float/double (and unknown) outright. */
            if (same) {
                int vc0 = cp_expr_result_vtclass(sir_child(e, 0));
                if (vc0 < 0 || vc0 == (int)LAT_VT_F32 || vc0 == (int)LAT_VT_F64)
                    same = false;
            }
            if (same) {
                cp_const_t k = { .state = CP_C_KNOWN, .value = 0 };
                if (gcong->cong_fold == GC_CMP_REFLEXIVE) {
                    /* CMP's reflexive result is 1 for EQ / LE / GE, 0 for
                     * NE / LT / GT — engine reads the discriminator field
                     * (sir.asdl shape fact, gated by the slot). Always i32. */
                    k.value = (e->tag == SIR_EQ || e->tag == SIR_LE
                                                 || e->tag == SIR_GE);
                } else {
                    /* GC_ZERO (x - x, x ^ x): a zero of the node's own
                     * width, read through the row's type slot — the
                     * representation authority — not a union-arm pun. */
                    k.cwidth = cp_dt_cwidth(gcong->type_prim_dt(e));
                }
                return k;
            }
        }
    }

    /* Primitive conversions (I2L, F2I, …) change width, so they fold via the
     * row's fold_convert (KNOWN operand → KNOWN result of the target width)
     * rather than the same-width fold_unary. Handled before the generic
     * machinery so a wide operand doesn't get routed to the i32/wide arith
     * folds. A non-KNOWN operand leaves the conversion unfolded (BOTTOM). */
    {
        const sir_op_gamma_t* gc = &sir_op_gamma[e->tag];
        if (gc->fold_convert) {
            cp_const_t a = v->input_count > 0 ? cp_input_const(eng, v->inputs[0]) : bot;
            if (a.state == CP_C_TOP) return top;
            if (a.state == CP_C_KNOWN) return gc->fold_convert(a);
            return bot;
        }
    }

    /* Unary and binary data opcodes fold once every operand is
     * known; a TOP operand keeps the result TOP, a BOTTOM one
     * BOTTOM. */
    /* γ_K fold dispatch: row arity selects unary vs binary; SIR_CMP
     * has its own fold_cmp slot (reads e->cmp.op discriminator). Rows
     * for non-foldable opcodes (NEW / INSTANCEOF / GETFIELD / ...)
     * have all three slots NULL → fall through to BOTTOM. */
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];
    if (!g->fold_unary && !g->fold_binary && !g->fold_cmp) return bot;
    cp_const_t a = v->input_count > 0 ? cp_input_const(eng, v->inputs[0]) : bot;
    cp_const_t b = v->input_count > 1 ? cp_input_const(eng, v->inputs[1]) : bot;
    bool binary = (g->arity == 2);
    if (a.state == CP_C_BOTTOM || (binary && b.state == CP_C_BOTTOM))
        return bot;
    if (a.state == CP_C_TOP || (binary && b.state == CP_C_TOP))
        return top;
    /* REF in a numeric arithmetic position has no value-lattice
     * meaning — sema rejects mixing references with arithmetic, so
     * reaching here means a malformed SIR (defensively BOTTOM). */
    if (a.state == CP_C_REF || (binary && b.state == CP_C_REF))
        return bot;
    /* RANGE-aware dispatch: when any input is RANGE, route to the row's
     * range-fold slot — these now handle i32 and i64 ranges alike (bounds
     * keyed by cwidth). Opcodes without a range-fold rule (bitwise, shifts)
     * return BOTTOM — no useful range arithmetic at this resolution. Checked
     * before the wide-KNOWN fold so an i64 RANGE operand routes here. */
    if (a.state == CP_C_RANGE || (binary && b.state == CP_C_RANGE)) {
        if (g->fold_cmp_range) return g->fold_cmp_range(e->tag, a, b);
        if (g->arity == 1 && g->fold_unary_range)  return g->fold_unary_range(a);
        if (g->arity == 2 && g->fold_binary_range) return g->fold_binary_range(a, b);
        return bot;
    }
    /* Wide (i64/f32/f64) KNOWN operands carry their value in lvalue/fvalue/dvalue,
     * outside the i32 KNOWN path — fold them at their own width. */
    if (a.cwidth != CP_W_I32 || (binary && b.cwidth != CP_W_I32))
        return cp_fold_wide(e->tag, a, b);
    /* every needed operand is CP_C_KNOWN */
    cp_const_t k = { .state = CP_C_KNOWN, .value = 0 };
    if (g->fold_cmp) {                          /* SIR_CMP */
        k.value = g->fold_cmp(e->tag, a.value, b.value);
        return k;
    }
    int32_t folded;
    bool ok = (g->arity == 1)
            ? g->fold_unary (a.value, &folded)
            : g->fold_binary(a.value, b.value, &folded);
    if (!ok) return bot;
    k.value = folded;
    return k;
}

/* Propagate type and constant facts to a fixed point. Every node
 * starts optimistic — type TK_TOP, constant CP_C_TOP — and is
 * recomputed when an input fact changes. Both lattices have finite
 * height, so this terminates. */
static bool cp_apply_identity_follower(cp_engine_t* eng, int v_idx);
static bool cp_apply_phi_follower(cp_engine_t* eng, int v_idx);
static bool cp_apply_load_follower(cp_engine_t* eng, int v_idx);
static bool cp_apply_arraylen_follower(cp_engine_t* eng, int v_idx);
static bool cp_apply_same_input_follower(cp_engine_t* eng, int v_idx);
static bool cp_revert_identity_follower(cp_engine_t* eng, int v_idx);
static bool cp_revert_phi_follower(cp_engine_t* eng, int v_idx);
static bool cp_revert_load_follower(cp_engine_t* eng, int v_idx);

/* ── Lattice A: points-to ────────────────────────────────────
 *
 * Obj naming is SYNTACTIC — one abstract object per allocation site — so it is
 * fixed here at build time and never revised by the fixpoint. Ids 0/1 are the
 * null object and the single external phantom (see the header).
 *
 * The sets are bitsets over that finite id space; join is ∪ and every transfer
 * is monotone, so the ascending chain terminates without widening. pts is read
 * ONLY by consumers — it never reaches cp_split_by_facts (a derived property is
 * not value identity; splitting on it would over-split). */

static bool cp_is_alloc_tag(int tag) {
    return tag == SIR_NEW || tag == SIR_NEWARRAY || tag == SIR_NEWREFARRAY;
}

/* Was `o` allocated by THIS method? (⊥null, the catch-all, and every §1 phantom
 * name something that already existed when the method was entered.) This is the
 * question the cell SEED asks: a fresh object's fields hold their §12.5 defaults,
 * a pre-existing one's hold anything. */
static bool cp_obj_is_local_alloc(const cp_engine_t* eng, int o) {
    return o >= eng->obj_first_site && o < eng->obj_count;
}

/* Is `o` ONE concrete runtime object? Spec §2's strong update turns on this, and it
 * is NOT "the pts set has one element" (VFG Rule 3 / Theorem 3). Two things are not
 * concrete:
 *   - a PHANTOM: it stands for an unknown object, and two phantoms may alias;
 *   - an allocation site INSIDE A LOOP: Obj naming is 1-limited, so one site names
 *     every object it ever produces. Killing "its" field would kill the field of the
 *     previous iteration's object, which may still be live.
 * Only a concrete object may be strongly updated; weak is always sound.
 *
 * Which sites loop is READ from what the DDCG recorded — it is the stage that knows
 * (it lowers the loop, and for a rectangular multi-dim `new` it emits the fill loop
 * itself). The optimizer would have to recompute control flow to work it out.
 *
 * FAIL-CLOSED: if the DDCG recorded any site for this method, one that is missing
 * from the record is a SUMMARY. A hand-built SIR (the unit harness) has no record at
 * all — and no lowering, hence no loop — so there a site is concrete. */
static bool cp_obj_is_concrete(const cp_engine_t* eng, int o) {
    return o >= 0 && o < eng->obj_count && eng->obj_concrete
        && eng->obj_concrete[o];
}

/* The CALLEE a node invokes, and whether it returns a reference. `method_idx` is an
 * index WITHIN a class (codegen resolves a call as wasm_func_index(class_id,
 * method_idx)), so the callee is the PAIR — `C.m3` and `D.m3` are different callees.
 * A virtual/interface call names its STATIC target: an override returns an object we
 * cannot see into either, and one phantom for the whole family is exactly the
 * "bottom method" §1 is talking about. */
static bool cp_callee_of(const sir_node_t* e, int* cls, int* mi) {
    switch (e->tag) {
        case SIR_INVOKEVIRTUAL:
            *cls = e->invoke_virtual.class_id;   *mi = e->invoke_virtual.method_idx;
            return e->invoke_virtual.return_type == SIR_DTREF;
        case SIR_INVOKESPECIAL:
            *cls = e->invoke_special.class_id;   *mi = e->invoke_special.method_idx;
            return e->invoke_special.return_type == SIR_DTREF;
        case SIR_INVOKESTATIC:
            *cls = e->invoke_static.class_id;    *mi = e->invoke_static.method_idx;
            return e->invoke_static.return_type == SIR_DTREF;
        case SIR_INVOKEINTERFACE:
            *cls = e->invoke_interface.class_id; *mi = e->invoke_interface.method_idx;
            return e->invoke_interface.return_type == SIR_DTREF;
        default: return false;
    }
}

/* Packed like a cell key, and degrading the same way: past 14 bits of class two
 * callees share one key, hence one `Oret`. That merges two unknowns into one unknown
 * — sound (both are ⊤-carriers), just coarser — and it says so out loud. */
static uint32_t cp_callee_key(int class_id, int method_idx) {
    return cp_cell_class_bits(class_id) | ((uint32_t)method_idx & 0xFFFF);
}

/* The memory cell an expression reads/writes, or -1. Lookup only — every cell was
 * interned before the solve (cp_enumerate_memory_cells). */
static int cp_cell_of_expr(const cp_engine_t* eng, const sir_node_t* e) {
    uint32_t key = cp_cell_key_for_expr(e);
    if (key == CP_CELL_NONE || key == CP_CELL_ALL) return -1;
    void* f = bbq_htree_search(eng->mem_cell_idx, (uint32_t)(key + 1));
    return f ? (int)((uintptr_t)f - 1) : -1;
}

/* A STATIC has no receiver object, so its cell's Obj↦pts map is degenerate: ONE
 * location, hence one row. That row is the cell's own object id — the cell is the
 * name of the storage, which is exactly what §2's `G` is. Every other row of a static
 * cell is unused (no GetField/PutField can address a static cell — different key), so
 * nothing else reads or writes it.
 *
 * The seed then falls out for free: row `obj_of_cell[c]` is not a local allocation, so
 * the seed already fills it with that cell's phantom and ⊥null — "whatever some other
 * method left there, possibly null", which is what makes reading a static sound without
 * an interprocedural summary. */
static int cp_static_row(const cp_engine_t* eng, int cell) {
    if (cell < 0 || cell >= eng->mem_cell_count || !eng->obj_of_cell) return -1;
    return eng->obj_of_cell[cell];
}

/* The EXACT class of an abstract object, or -1 for "not known".
 *
 * Only an allocation SITE has one: `new C()` produces an object of class C exactly — not
 * a subclass. A phantom, ⊥null, the catch-all and an `Oret` stand for objects we have not
 * seen allocated, so their class is unknown, and -1 means "prove nothing" (every filter
 * keeps the object). This is spec §1's `τ@site`: the object carries its type. */
static int cp_exact_class_of(const cp_engine_t* eng, int o) {
    if (!cp_obj_is_local_alloc(eng, o)) return -1;
    int v = eng->vnode_of_obj[o];
    const sir_node_t* site = (v >= 0 && v < eng->vnode_count) ? eng->vnodes[v]->expr
                                                              : NULL;
    if (site && site->tag == SIR_NEW) return site->new_.class_id;
    return -1;
}

/* The COMPONENT class of an abstract array object, or -1 for "not known" — spec §1's
 * `array.new τ@site` read back off the site (§3's "array collapse").
 *
 * The object is the BACKING array: the DDCG indirects every reference-array op through
 * `GetField(data)`, because WASM-GC array types are INVARIANT while Java's are covariant
 * (§10.2) — so the backing is one shared top-ref array type and the Java identity lives in
 * the wrapper struct. `NewRefArray.class_id` IS the element class (codegen ignores it and
 * always emits the top-ref type; γ types the node from it). It is the ACTUAL component,
 * from the allocation — never the declared one, which is precisely what covariance makes
 * a lie: `Object[] o = new A[1]` declares Object and allocates A. */
static int cp_array_component_of(const cp_engine_t* eng, int o) {
    if (!cp_obj_is_local_alloc(eng, o)) return -1;
    int v = eng->vnode_of_obj[o];
    const sir_node_t* site = (v >= 0 && v < eng->vnode_count) ? eng->vnodes[v]->expr
                                                              : NULL;
    if (site && site->tag == SIR_NEWREFARRAY && site->new_ref_array.class_id >= 0)
        return site->new_ref_array.class_id;
    return -1;
}

/* Spec §2's `classOf(O) ≤ τ`, as a TRI-STATE — because both answers drop objects and a
 * guess in either direction is a miscompile:
 *
 *    1  provably IS   an instance of τ   → the NOT_ISA arm may drop it
 *    0  provably NOT  an instance of τ   → the cast, and the ISA arm, may drop it
 *   -1  unknown                          → NOBODY may drop it
 *
 * The subtype question is JLS §4.10.2 and goes to sema_ref_is_subtype, the one place
 * that knows the class table. Asking the EXTENDS CHAIN instead (sema_is_subclass_of)
 * answers "not a subtype" for every interface — and this filter DROPS on that answer,
 * so a cast to an interface would delete every object that implements it.
 *
 * An exact class makes this precise in both directions: the site allocates C and only
 * C, so `C ≤ τ` decides it. Without one, -1. */
static int cp_obj_isa(const cp_engine_t* eng, int o, sir_atype_t atype, int target) {
    if (atype != SIR_ATCLASS) return -1;    /* an array test: not modelled — keep */
    if (o == CP_OBJ_NULL) return 0;         /* `null instanceof τ` is FALSE (JLS §15.20.2) */
    int c = cp_exact_class_of(eng, o);
    if (c < 0 || !eng->sema) return -1;
    return sema_ref_is_subtype(eng->sema, c, target) ? 1 : 0;
}

static void cp_enumerate_objects(cp_engine_t* eng) {
    int n_at_enum = eng->vnode_count;      /* later passes append vnodes (Refine, CSE) */
    eng->obj_of_vnode = (int*)bbq_arena_alloc(eng->arena,
                            (size_t)(n_at_enum > 0 ? n_at_enum : 1) * sizeof(int));
    for (int v = 0; v < n_at_enum; v++) eng->obj_of_vnode[v] = -1;

    /* PHANTOMS first (spec §1's `Oext@param`, one per (site, type)), so that
     * "o >= obj_first_site" IS the concrete/summary test.
     *
     * The site of an incoming reference is the SLOT whose entry value is its seed.
     * JLS §16's definite assignment means a non-parameter local can never be read
     * before it is written, so a seeded REF slot is a formal parameter — no
     * param_count is needed, and none exists on sir_method_t. A slot that is never
     * read from its seed simply has a phantom nobody names. */
    int next = CP_OBJ_FIRST_PHANTOM;
    eng->obj_of_slot = (int*)bbq_arena_alloc(eng->arena,
                           (size_t)(eng->slot_count > 0 ? eng->slot_count : 1) * sizeof(int));
    for (int s = 0; s < eng->slot_count; s++) {
        eng->obj_of_slot[s] = -1;
        cp_slot_types_t* st = &eng->slot_types;
        if (st->seen && st->seen[s] && st->dt[s] == SIR_DTREF)
            eng->obj_of_slot[s] = next++;          /* one phantom per (slot, type) */
    }
    /* `this` is slot 0 of an instance method (sema_param_slot's base = `this` ? 1 : 0), and the
     * DDCG compiles a `this.f` receiver as LoadLocal(0) — so `this` and the receiver slot are ONE
     * runtime object. Unify their phantoms. Otherwise the summary reads this_escape / this_obj
     * off an obj_this that a compiled body never references (spuriously NoEscape, empty write
     * set), while every `this.f = …` store lands on the slot-0 phantom instead — which let a
     * field-initializer ctor look like a no-op and get dropped (the no-op-ctor-drop miscompile). A synthetic
     * method that names `this` only via LoadThis (internalClone) never reads slot 0 as a local,
     * so obj_of_slot[0] is -1 there and `this` keeps its own phantom. */
    bool inst_method = false;
    if (eng->method && eng->sema) {
        const sema_class_t* mc = sema_get_class(eng->sema, eng->method->class_id);
        if (mc && eng->method->method_id >= 0
            && eng->method->method_id < (int)bbq_vec_len((void*)mc->methods))
            inst_method = (mc->methods[eng->method->method_id].modifiers & ACC_STATIC) == 0;
    }
    eng->obj_this = (inst_method && eng->slot_count > 0 && eng->obj_of_slot[0] >= 0)
                    ? eng->obj_of_slot[0]          /* `this` IS the receiver slot */
                    : next++;                      /* LoadThis-only / static: its own phantom */

    /* The other half of "reachable from a formal parameter or a global": what those
     * objects HOLD. A field of an unknown object is itself an unknown object, and
     * `p.f` and `p.g` are not the same unknown — one phantom for both is what made
     * every store through any unknown visible at every load through any other.
     *
     * The name is the memory CELL: syntactic (the (class, field) pairs this method
     * mentions), finite, and known before the solve — which is what an
     * object NAME must be, pts being a fixpoint RESULT. It also bounds the recursion: cell f's
     * SEED row for a pre-existing object holds cell f's phantom, so `p.f.f` names the
     * same phantom as `p.f`, at any depth, with no k-limiting rule to pick. */
    eng->obj_first_cell = next;
    eng->obj_of_cell = (int*)bbq_arena_alloc(eng->arena,
                           (size_t)(eng->mem_cell_count > 0 ? eng->mem_cell_count : 1)
                           * sizeof(int));
    for (int c = 0; c < eng->mem_cell_count; c++) eng->obj_of_cell[c] = next++;
    eng->obj_first_ret = next;

    /* Spec §1's `Oret@callee`: the abstract "the callee returned SOME ref" object, for
     * a method whose body this analysis cannot see (every method, today — there is no
     * interprocedural summary). It is named by the CALLEE, not by the call site: that
     * is what keeps the object set finite in a loop that calls the same method, and it
     * still separates `a.foo()` from `b.bar()`, which the one shared catch-all did not.
     *
     * A phantom, not a site: the callee may hand back an object that already existed —
     * possibly one WE handed it — so it is never concrete (no strong update), it may be
     * null, and its fields are unknown. */
    eng->callee_idx = bbq_htree_create();
    for (int v = 0; v < n_at_enum; v++) {
        cp_vnode_t* n = eng->vnodes[v];
        if (n->kind != CP_VN_EXPR || !n->expr) continue;
        int cls = 0, mi = 0;
        if (!cp_callee_of(n->expr, &cls, &mi)) continue;   /* not a ref-returning call */
        uint32_t key = cp_callee_key(cls, mi) + 1;         /* 0 is htree's "absent" */
        void* found = bbq_htree_search(eng->callee_idx, key);
        int o;
        if (found) {
            o = (int)((uintptr_t)found - 1);
        } else {
            o = next++;
            bbq_htree_insert(eng->callee_idx, key, (void*)(uintptr_t)(o + 1));
        }
        eng->obj_of_vnode[v] = o;
    }

    eng->obj_first_site = next;

    for (int v = 0; v < n_at_enum; v++) {
        cp_vnode_t* n = eng->vnodes[v];
        if (n->kind != CP_VN_EXPR || !n->expr) continue;
        if (!cp_is_alloc_tag(n->expr->tag)) continue;
        eng->obj_of_vnode[v] = next++;
    }
    eng->obj_count = next;
    eng->obj_words = (eng->obj_count + 63) / 64;
    /* The inverse: which vnode allocated object O. Built here, where the numbering
     * happens — the only place that knows it. Meaningful ONLY for an allocation SITE,
     * which is the one kind of object a single vnode owns: an `Oret` is shared by every
     * call to its callee, so it would land on an arbitrary one of them. Ask it about
     * `o >= obj_first_site` and nothing else. */
    eng->vnode_of_obj = (int*)bbq_arena_alloc(eng->arena,
                            (size_t)eng->obj_count * sizeof(int));
    for (int o = 0; o < eng->obj_count; o++) eng->vnode_of_obj[o] = -1;
    for (int v = 0; v < n_at_enum; v++)
        if (eng->obj_of_vnode[v] >= 0) eng->vnode_of_obj[eng->obj_of_vnode[v]] = v;

}

/* WHAT KIND of abstract object is `o`? THE authority.
 *
 * cp_enumerate_objects hands out ids in kind order, so it already decides this — it just
 * used to throw the answer away, leaving every caller to reconstruct a slice of the
 * taxonomy for itself (is it a local alloc? does it have an exact class? is it a static's
 * row?). That is how the escape seed came to re-decode raw cell-key bits to work out which
 * phantom named a static: a sixth private copy of a fact the naming pass already knew.
 * One accessor, read by everyone; the ranges are recorded where they are assigned. */
static cp_obj_kind_t cp_obj_kind(const cp_engine_t* eng, int o) {
    if (o < 0 || o >= eng->obj_count)      return CP_OBJK_NONE;
    if (o == CP_OBJ_NULL)                  return CP_OBJK_NULL;
    if (o == CP_OBJ_EXT)                   return CP_OBJK_CATCHALL;
    if (o >= eng->obj_first_site)          return CP_OBJK_SITE;
    if (o >= eng->obj_first_ret)           return CP_OBJK_RET;
    if (o >= eng->obj_first_cell)          return CP_OBJK_CELL;
    return CP_OBJK_PARAM;                  /* the slot phantoms and `this` */
}

/* CONCRETE vs SUMMARY, resolved ONCE — §2's strong update asks this on every store
 * recompute, and a transfer must be O(its inputs) (it must not scan a list).
 *
 * A phantom is never concrete: two unknowns may alias at runtime. An allocation site
 * is concrete iff the DDCG recorded it as NOT looping — the DDCG is the stage that
 * knows, and it knows more than the spine shows: for a rectangular multi-dim `new` it
 * emits the fill loop itself, so the inner levels are summaries even when the source
 * has no loop.
 *
 * FAIL-CLOSED: when the DDCG recorded anything at all for this method, a site missing
 * from the record is a SUMMARY. Only a hand-built SIR (the unit harness — no DDCG,
 * hence no lowering and no loop) takes an unrecorded site as concrete.
 *
 * Runs after the recorded facts are attached, before the solve. */
static void cp_index_concrete_objects(cp_engine_t* eng) {
    eng->obj_concrete = (bool*)bbq_arena_alloc(eng->arena,
                            (size_t)(eng->obj_count > 0 ? eng->obj_count : 1) * sizeof(bool));
    /* "NO DDCG RAN" IS `fact_count == 0`, NOT "no ALLOC rows".
     *
     * A hand-built SIR (the unit harness) records NOTHING — no scopes, no guards, no
     * regions — and only there may an unrecorded site be taken as concrete: no loop
     * can have been lowered, because no lowering happened.
     *
     * A method the DDCG *did* compile which happens to record no ALLOC row is a very
     * different animal, and reading it as "no DDCG" is FAIL-OPEN: `$ensure_init` and
     * `$main` are built by hand inside a real compile (compiler.c), and they mint an
     * exception object that no record_alloc ever saw. Calling that site CONCRETE
     * licenses a strong update through it — a kill of a field of an object that may
     * not be the one being stored to. Every OTHER kind of row is still recorded for
     * those methods (their try region, their scopes), so `fact_count` sees them and
     * the site falls back to SUMMARY, which is the fail-CLOSED answer §2 demands. */
    bool no_ddcg_at_all = (eng->fact_count == 0);
    for (int o = 0; o < eng->obj_count; o++) {
        if (!cp_obj_is_local_alloc(eng, o)) { eng->obj_concrete[o] = false; continue; }
        if (no_ddcg_at_all)                 { eng->obj_concrete[o] = true;  continue; }
        int v = eng->vnode_of_obj[o];
        const sir_node_t* site = (v >= 0 && v < eng->vnode_count) ? eng->vnodes[v]->expr
                                                                  : NULL;
        /* ALLOC row: `a` = 1 iff the site can run more than once (a SUMMARY). An
         * unrecorded site is a summary — fail-closed. */
        const compiler_fact_t* f = site ? cp_fact_for(eng, COMPILER_FACT_ALLOC, site)
                                        : NULL;
        eng->obj_concrete[o] = (f && f->a == 0);
    }
}

/* A fresh empty set. Allocated lazily — ∅ is a NULL bits pointer, so an engine
 * over a method with no refs pays nothing. */
static cp_pts_t cp_pts_new(const cp_engine_t* eng) {
    cp_pts_t s;
    s.bits = (uint64_t*)bbq_arena_alloc(eng->arena,
                 (size_t)(eng->obj_words > 0 ? eng->obj_words : 1) * sizeof(uint64_t));
    memset(s.bits, 0, (size_t)(eng->obj_words > 0 ? eng->obj_words : 1) * sizeof(uint64_t));
    return s;
}

static void cp_pts_add(const cp_engine_t* eng, cp_pts_t* s, int obj) {
    if (obj < 0 || obj >= eng->obj_count) return;
    s->bits[obj >> 6] |= (uint64_t)1 << (obj & 63);
}

static void cp_pts_remove(const cp_engine_t* eng, cp_pts_t* s, int obj) {
    if (!s->bits || obj < 0 || obj >= eng->obj_count) return;
    s->bits[obj >> 6] &= ~((uint64_t)1 << (obj & 63));
}

static void cp_pts_union(const cp_engine_t* eng, cp_pts_t* dst, cp_pts_t src) {
    if (!src.bits) return;
    for (int i = 0; i < eng->obj_words; i++) dst->bits[i] |= src.bits[i];
}

bool cp_pts_has(const cp_engine_t* eng, cp_pts_t s, int obj) {
    if (!s.bits || obj < 0 || obj >= eng->obj_count) return false;
    return (s.bits[obj >> 6] >> (obj & 63)) & 1;
}

/* §6's answer, for the consumer and the tests. FAIL-CLOSED: anything this engine cannot
 * name is GlobalEscape, so a caller that asks about a nonexistent object never gets a
 * license to scalar-replace it. */
cp_escape_t cp_escape_of(const cp_engine_t* eng, int obj) {
    if (!eng || !eng->escape || obj < 0 || obj >= eng->obj_count) return CP_ESC_GLOBAL;
    return eng->escape[obj];
}

/* …of the object an allocation EXPRESSION names. The Obj is the site's, via the syntactic
 * naming — not a pts query, because an allocation IS its object (a syntactic name, not a pts result). Goes through
 * vnode_of_obj, the documented inverse: obj_of_vnode is sized to the vnode count AT
 * ENUMERATION, and later passes append vnodes. */
cp_escape_t cp_escape_of_expr(const cp_engine_t* eng, const sir_node_t* alloc) {
    if (!eng || !alloc || !eng->vnode_of_obj) return CP_ESC_GLOBAL;
    int vn = cp_vnode_of((cp_engine_t*)eng, alloc);
    if (vn < 0) return CP_ESC_GLOBAL;
    for (int o = eng->obj_first_site; o < eng->obj_count; o++)
        if (eng->vnode_of_obj[o] == vn) return cp_escape_of(eng, o);
    return CP_ESC_GLOBAL;                       /* not an allocation site — fail closed */
}

bool cp_pts_empty(const cp_engine_t* eng, cp_pts_t s) {
    if (!s.bits) return true;
    for (int i = 0; i < eng->obj_words; i++) if (s.bits[i]) return false;
    return true;
}

int cp_pts_count(const cp_engine_t* eng, cp_pts_t s) {
    if (!s.bits) return 0;
    int n = 0;
    for (int i = 0; i < eng->obj_words; i++) {
        uint64_t w = s.bits[i];
        while (w) { w &= w - 1; n++; }
    }
    return n;
}

static bool cp_pts_eq(const cp_engine_t* eng, cp_pts_t a, cp_pts_t b) {
    for (int i = 0; i < eng->obj_words; i++) {
        uint64_t x = a.bits ? a.bits[i] : 0;
        uint64_t y = b.bits ? b.bits[i] : 0;
        if (x != y) return false;
    }
    return true;
}

int cp_obj_of(const cp_engine_t* eng, const sir_node_t* alloc) {
    void* f = cp_pmap_get(&eng->expr_idx, alloc);
    if (!f) return -1;
    return eng->obj_of_vnode[(int)((uintptr_t)f - 1)];
}

/* ── §3, Lattice B: the class/type element ─────────────────────────────────
 *
 * `τ̂(v) = ⨆_{O ∈ pts(v)} exactClassOf(O)`, joined over the class hierarchy.
 *
 * DERIVED — a QUERY over pts, never a stored field. A stored copy IS the "second type
 * domain" §3 forbids: it would have to be kept in step with pts by hand, and it would
 * drift the moment a cast filter refines pts. Free once pts runs, which is the point.
 *
 * `exactClassOf(O)` is READ from the allocation node's own lattice type — the type
 * lattice already computed it (γ gives `New` a REF of its class, `NewArray` a PRIM_ARRAY,
 * `NewRefArray` an ARRAY), and it is the authority (§10: consulted, never duplicated).
 * The join is `type_meet`, the lattice's own LUB. Nothing here re-derives a hierarchy.
 *
 * EXACT, not "may be": `new C` allocates a C and never a subclass, which is what makes
 * this stronger than the static type and lets a cast or a virtual dispatch be decided.
 *
 * FAIL-CLOSED: an object whose class we do not know — a phantom, an `Oret`, the
 * catch-all — yields BOTTOM, the absorbing element, so one unknown in the set poisons
 * the whole join. Every consumer's `⊑ τ` question then answers NO and the guard stays.
 * ⊥null is TK_NULL, which JLS §4.10.2 makes a subtype of every reference type, so it
 * joins away: a maybe-null C is still a C. Its NPE guard is §4's business, not §3's.
 *
 * ∅ (nothing reaches here) is TOP, the join identity. TOP is the lattice MINIMUM, so
 * `TOP ⊑ anything` is TRUE — a consumer that asked only "τ̂ ⊑ target" would happily drop
 * a guard on an unreached node. Every consumer must therefore ALSO require pts to name
 * at least one object; `cp_tau_can_prove` is the one place that pairs the two. */
const Type* cp_tau_of_vnode(cp_engine_t* eng, int vi) {
    if (vi < 0 || vi >= eng->vnode_count) return type_bottom(&eng->pool);
    cp_pts_t p = eng->vnodes[vi]->pts;
    if (!p.bits) return type_top(&eng->pool);
    const Type* t = type_top(&eng->pool);
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            const Type* ot;
            if (o == CP_OBJ_NULL) {
                ot = type_null(&eng->pool);
            } else if (!cp_obj_is_local_alloc(eng, o)) {
                return type_bottom(&eng->pool);      /* unknown class — poisons the join */
            } else {
                int av = eng->vnode_of_obj[o];
                ot = (av >= 0 && av < eng->vnode_count) ? eng->vnodes[av]->type : NULL;
                if (!ot) return type_bottom(&eng->pool);
            }
            t = type_meet(eng->sema, t, ot, &eng->pool);
        }
    }
    return t;
}

const Type* cp_tau_of_expr(cp_engine_t* eng, const sir_node_t* e) {
    void* f = cp_pmap_get(&eng->expr_idx, e);
    if (!f) return type_bottom(&eng->pool);
    return cp_tau_of_vnode(eng, (int)((uintptr_t)f - 1));
}

/* The class of the ONE object a reference names — or -1 when it names more than one, or
 * when that object's class is unknown. This is §3's "pts(v) singleton with exact class",
 * the question DEVIRTUALIZATION asks.
 *
 * NOT `cp_obj_is_concrete`. That asks about IDENTITY (is this one runtime object, so may
 * its field be strongly updated), and a site inside a loop answers NO. This asks about
 * the CLASS, and a loop's site answers YES: it is a summary of many objects, but every
 * one of them is a `new C`, hence a C. Conflating the two is what miscompiled
 * `new int[2][2][2]`, in the other direction.
 *
 * ⊥null in the set answers -1: a vtable dispatch on null traps AT the dispatch, while a
 * direct call would enter the body with a null `this`. */
/* ── §0's DEFUNCTIONALIZED CALL_REF TARGET SET ──────────────────────────────
 *
 * "the defunctionalized `call_ref` target set (the usually-hard part, given precise) …
 * the VFG paper spends its whole scalability budget approximating exactly what you
 * already have."
 *
 * The set of methods a call site can actually invoke. DERIVED, like everything else: the
 * receiver's possible CLASSES come from lattice A, and each is resolved through JLS §8.4.8
 * by `sema_resolve_virtual` — the same authority the WASM vtable builder fills its slots
 * with. Devirtualize iff the SET is a singleton.
 *
 * §3's "pts singleton with exact class" is the special case where the set has one element
 * because there is one OBJECT, and it is strictly weaker. Two receiver classes that share
 * one implementation are ONE target; a `final` method has one target whatever the receiver
 * is. Both are sound in any mode, because both are facts about the program's own classes.
 *
 * Returns true and fills (out_class, out_midx) iff exactly one implementation is possible.
 *
 * NOT DONE, and it must not be: CHA — "only one implementation exists program-wide". That
 * needs a CLOSED WORLD. In RUNTIME/PLUGIN mode a plugin loaded later can subclass a jre
 * class and override its method, so devirtualizing on "nobody overrides it here" would call
 * the wrong method. It is sound only under SEMA_MODE_WHOLE, and the fail-closed pin for the
 * open world is in test_sir §26. */
static bool cp_call_target_set(cp_engine_t* eng, int recv_vn, int decl_cls, int decl_midx,
                               int* out_class, int* out_midx) {
    if (!eng->sema || decl_cls < 0) return false;

    /* A vtable dispatch on null traps AT the dispatch (it reads the object's ClassDesc); a
     * direct call would enter the body with a null `this`. So a maybe-null receiver keeps
     * its dispatch, whatever the target set says. */
    if (recv_vn < 0 || recv_vn >= eng->vnode_count) return false;
    cp_pts_t p = eng->vnodes[recv_vn]->pts;
    if (cp_pts_empty(eng, p) || cp_pts_has(eng, p, CP_OBJ_NULL)) return false;

    /* THE FINAL CASE: a final method, or a method of a final class, cannot be overridden —
     * so the declared resolution is the ONLY target, and the receiver's class need not be
     * known at all. (§8.4.3.3: a final method may not be overridden; §8.1.1.2: a final class
     * may not be subclassed.) */
    const sema_class_t* dc = sema_get_class(eng->sema, decl_cls);
    if (dc && decl_midx >= 0 && decl_midx < (int)bbq_vec_len(dc->methods)) {
        const sema_method_t* dm = &dc->methods[decl_midx];
        if (((dm->modifiers & ACC_FINAL) || (dc->modifiers & ACC_FINAL))
                && sema_method_is_defined(eng->sema, decl_cls, dm)) {
            *out_class = decl_cls;
            *out_midx  = decl_midx;
            return true;
        }
    }

    /* THE GENERAL CASE: resolve every class the receiver may be. One implementation across
     * all of them ⟹ one target. An object whose class we do not know (a phantom, an `Oret`)
     * makes the class set unknown, and an unknown class could resolve anywhere. */
    int impl_c = -1, impl_m = -1;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            int exact = cp_exact_class_of(eng, o);
            if (exact < 0) return false;                    /* unknown class ⟹ unknown set */
            int c = -1, m = -1;
            if (!sema_resolve_virtual(eng->sema, exact, decl_cls, decl_midx, &c, &m))
                return false;                               /* abstract / not resolvable */
            if (impl_c < 0) { impl_c = c; impl_m = m; }
            else if (impl_c != c || impl_m != m) return false;   /* MORE THAN ONE TARGET */
        }
    }
    if (impl_c < 0) return false;
    *out_class = impl_c;
    *out_midx  = impl_m;
    return true;
}

/* The FULL defunctionalized target set of a virtual/interface call — spec §7's "a virtual
 * call_ref fans out to its finite target set; join the per-target summaries." Returns true and
 * fills classes[]/midxs[] (deduped, count ≤ max) iff the set is FULLY KNOWN: every receiver
 * object has an exact class that resolves. A phantom / `Oret` receiver (unknown class), an
 * abstract/unresolvable target, or an overflow past `max` ⟹ false, and the caller falls back to
 * §7's bottom graph. Null is skipped — a null receiver TRAPS at the dispatch, so it names no
 * target — but (unlike devirt) a MAYBE-null receiver does not disqualify the set: the escape
 * effect is applied assuming the call executes, which is the conservative direction. */
static bool cp_virtual_target_set(cp_engine_t* eng, int recv_vn, int decl_cls, int decl_midx,
                                  int* classes, int* midxs, int max, int* count) {
    *count = 0;
    if (!eng->sema || decl_cls < 0 || recv_vn < 0 || recv_vn >= eng->vnode_count) return false;
    const sema_class_t* dc = sema_get_class(eng->sema, decl_cls);
    if (dc && decl_midx >= 0 && decl_midx < (int)bbq_vec_len(dc->methods)) {
        const sema_method_t* dm = &dc->methods[decl_midx];
        if (((dm->modifiers & ACC_FINAL) || (dc->modifiers & ACC_FINAL))
                && sema_method_is_defined(eng->sema, decl_cls, dm)) {
            classes[0] = decl_cls; midxs[0] = decl_midx; *count = 1; return true;   /* one target */
        }
    }
    cp_pts_t p = eng->vnodes[recv_vn]->pts;
    if (cp_pts_empty(eng, p)) return false;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            int exact = cp_exact_class_of(eng, o);
            if (exact < 0) return false;
            int c = -1, m = -1;
            if (!sema_resolve_virtual(eng->sema, exact, decl_cls, decl_midx, &c, &m)) return false;
            bool dup = false;
            for (int t = 0; t < *count; t++) if (classes[t] == c && midxs[t] == m) { dup = true; break; }
            if (dup) continue;
            if (*count >= max) return false;
            classes[*count] = c; midxs[*count] = m; (*count)++;
        }
    }
    return *count > 0;
}

cp_pts_t cp_pts_of_expr(const cp_engine_t* eng, const sir_node_t* e) {
    cp_pts_t empty = { NULL };
    void* f = cp_pmap_get(&eng->expr_idx, e);
    if (!f) return empty;
    return eng->vnodes[(int)((uintptr_t)f - 1)]->pts;
}

/* An UNKNOWN reference: some external object, and possibly null. CP_OBJ_EXT
 * alone means "some unknown NON-null object" — a value the method cannot see
 * into but that is known to exist (`this`, a Class object). Anything genuinely
 * unknown — a formal parameter, a static, a call result, the contents of a cell
 * we have not tracked — MAY BE NULL, and must say so, or nullability wrongly
 * proves it NonNull and deletes a guard that was doing its job. (Caught by
 * test_sir §12's parameter case, which is why that case exists.) */
static cp_pts_t cp_pts_unknown_ref(const cp_engine_t* eng) {
    cp_pts_t s = cp_pts_new(eng);
    cp_pts_add(eng, &s, CP_OBJ_EXT);
    cp_pts_add(eng, &s, CP_OBJ_NULL);
    return s;
}

/* An unknown reference that entered the method through `phantom` — spec §1's
 * `Oext@param`. Still unknown (it may be any object of any class) and still
 * possibly null; the ONLY thing the phantom buys is that two different incoming
 * references are two different objects, so a store through one does not have to
 * be assumed to change the other's fields. */
static cp_pts_t cp_pts_phantom_ref(const cp_engine_t* eng, int phantom) {
    cp_pts_t s = cp_pts_new(eng);
    cp_pts_add(eng, &s, phantom);
    cp_pts_add(eng, &s, CP_OBJ_NULL);
    return s;
}

/* A cell's contents: one pts-set per abstract object. Allocated ONCE per memory
 * vnode and thereafter recomputed IN PLACE — the transfer reads this node's
 * inputs and nothing else, which is what makes it a dataflow transfer rather
 * than a graph traversal. (Both of my earlier attempts got this wrong: one
 * reallocated the whole map on every recompute, the other walked the memory
 * chain from each load. Either way the cost was O(graph) inside a function the
 * fixpoint calls thousands of times, and the jre build went from seconds to
 * minutes.) */
/* The map's rows live in ONE contiguous bit-matrix, so copying the reaching
 * state, clearing, and comparing are single word-parallel primitives (memcpy /
 * memset / memcmp) rather than a walk over the objects. Row o starts at word
 * o * obj_words; h[0].bits is the base of the whole buffer. */
static size_t cp_heap_bytes(const cp_engine_t* eng) {
    int n = eng->obj_count ? eng->obj_count : 1;
    int w = eng->obj_words ? eng->obj_words : 1;
    return (size_t)n * (size_t)w * sizeof(uint64_t);
}

static cp_pts_t* cp_heap_new(const cp_engine_t* eng) {
    int n = eng->obj_count ? eng->obj_count : 1;
    int w = eng->obj_words ? eng->obj_words : 1;
    cp_pts_t* h = (cp_pts_t*)bbq_arena_alloc(eng->arena, (size_t)n * sizeof(cp_pts_t));
    uint64_t* flat = (uint64_t*)bbq_arena_alloc(eng->arena, cp_heap_bytes(eng));
    memset(flat, 0, cp_heap_bytes(eng));
    for (int i = 0; i < n; i++) h[i].bits = flat + (size_t)i * (size_t)w;
    return h;
}

/* Do these two cell-maps differ? One memcmp over the bit-matrix. */
static bool cp_heap_differs(const cp_engine_t* eng, const cp_pts_t* a,
                            const cp_pts_t* b) {
    return memcmp(a[0].bits, b[0].bits, cp_heap_bytes(eng)) != 0;
}

/* The heap transfer — spec §2's `pts(O.f)`.
 *
 * A memory-state vnode NAMES a version of one cell; its value is that version's
 * contents as `Obj ↦ pts`. Spec §2 licenses the strong update because "the store's
 * memory-SSA NAME kills the prior def" — the kill is in the NAMING. So a name's
 * value is a FUNCTION OF ITS INPUTS, recomputed each time:
 *   - a cell-φ  ⟹ the union of the versions reaching it;
 *   - a store   ⟹ the version reaching it, with this store applied:
 *       `pts(p)` a singleton `{O}` ⟹ STRONG: row O is REPLACED by what was stored;
 *       otherwise                  ⟹ weak: `∪=` into every row `pts(p)` may name;
 *   - a seed / an invoke's wide kill ⟹ contents we cannot see.
 *
 * Accumulating into the name's own previous value instead — which is what this
 * did — makes the kill look like a RETRACTION, and the cell-φ that unions it can
 * never take that back. That is not a monotonicity problem to be guarded around:
 * it is simply not the transfer the spec describes. Recomputed, nothing in the
 * lattice ever shrinks, and §2's "replace, not ∪" and "monotone-increasing" agree.
 *
 * Computed into an engine-wide scratch matrix and committed only if it differs, so
 * a shrinking row is detected (memcmp, not a population count) and its users are
 * re-armed. Zero allocation; the whole-map steps are memcpy/memset/word-OR over the
 * contiguous bit-matrix, and the store touches only the rows its receiver names. */
/* The escape half of "a call preserves object o's cells" — o was handed to nobody that could
 * write it (Choi §4.5 bottom graph): NoEscape is never handed out; an ArgEscape object survives
 * only if the method makes no bottom/native call (else the caller may have globalized it, §4.5)
 * and it is not transitively reachable from a bottom-passed operand (`obj_bottom`, Choi Fig 6);
 * GlobalEscape is never safe. The ONE authority — cp_update_heap's kill transfer AND Gate 5's
 * value-forwarding follower both read it, so they cannot disagree about what a call preserves. */
static bool cp_obj_survives_call(const cp_engine_t* eng, int o) {
    if (o < 0 || o >= eng->obj_count) return false;
    return (eng->escape[o] == CP_ESC_NONE)
        || (eng->escape[o] == CP_ESC_ARG && !eng->has_bottom_call
            && eng->obj_bottom && !eng->obj_bottom[o]);
}

static bool cp_update_heap(cp_engine_t* eng, int vi) {
    cp_vnode_t* v = eng->vnodes[vi];
    bool is_cell_phi = (v->kind == CP_VN_PHI && v->phi_cell >= 0);
    signed char k = (vi < eng->mem_rows) ? eng->mem_kind[vi] : CP_MEM_NONE;
    if (!is_cell_phi && k == CP_MEM_NONE) return false;
    if (!is_cell_phi && k != CP_MEM_SEED && k != CP_MEM_WIDE && k != CP_MEM_STORE
                     && k != CP_MEM_KILL)
        return false;

    if (!v->heap) v->heap = cp_heap_new(eng);
    if (!eng->heap_scratch) eng->heap_scratch = cp_heap_new(eng);
    cp_pts_t* s      = eng->heap_scratch;
    size_t    bytes  = cp_heap_bytes(eng);
    int       words  = eng->obj_words ? eng->obj_words : 1;
    memset(s[0].bits, 0, bytes);              /* a fresh name holds nothing */

    if (is_cell_phi) {
        for (int i = 0; i < v->input_count; i++) {
            int in = v->inputs[i];
            if (in < 0 || in >= eng->vnode_count) continue;
            const cp_pts_t* ih = eng->vnodes[in]->heap;
            if (!ih) continue;
            uint64_t* dst = s[0].bits;
            const uint64_t* src = ih[0].bits;
            for (size_t q = 0; q < bytes / sizeof(uint64_t); q++) dst[q] |= src[q];
        }
    } else if (k == CP_MEM_SEED) {
        /* A cell's contents on entry. An object THIS method allocated does not exist
         * yet at entry, and §12.5 gives its fields their default — null. An object
         * that already existed — the catch-all, and every §1 phantom (a parameter,
         * `this`, another cell) — holds an unknown object.
         *
         * WHICH unknown is the point of §1's "one per (site, type)": THIS CELL's
         * phantom. `p.f` and `p.g` are then two different unknowns — with one shared
         * `Oext` they were the same object, so a store through anything reached by
         * `.f` was visible at every load of every other field of every other unknown.
         * And because cell f's seed row for a phantom holds cell f's own phantom, the
         * naming closes on itself: `p.f.f.f` is `p.f`. Finite, at any depth. */
        int cell = (vi < eng->mem_rows) ? eng->mem_cell[vi] : -1;
        int ph   = (cell >= 0 && cell < eng->mem_cell_count) ? eng->obj_of_cell[cell]
                                                             : CP_OBJ_EXT;
        for (int o = 0; o < eng->obj_count; o++) {
            if (!cp_obj_is_local_alloc(eng, o)) cp_pts_add(eng, &s[o], ph);
            cp_pts_add(eng, &s[o], CP_OBJ_NULL);
        }
    } else if (k == CP_MEM_KILL) {
        /* ONE cell, killed by a call — §7's bottom graph, made precise by §6.
         *
         * A bottom method can only touch what it was HANDED, and what is reachable from that
         * or from a global. An object that is still NoEscape was handed to nobody: it was
         * never passed as an argument or receiver (§7's rule lowers every call operand to
         * ArgEscape), never returned, never stored into an object that escaped, never thrown.
         * So the callee holds no reference to it and CANNOT have written its fields — its row
         * of this cell survives the call untouched. Every other row is whatever the callee
         * left there, which we cannot see.
         *
         * The killed rows keep the CATCH-ALL rather than this cell's phantom: the callee may
         * have stored an object WE handed it (any ArgEscape one), and a phantom does not stand
         * for those. CP_OBJ_EXT does — every consumer reads it as "prove nothing".
         *
         * OPTIMISM + RE-ARM: escape starts at ⊤ (NoEscape) and only descends, so a row kept
         * here may have to be killed on a later round. That fact reaches this transfer through
         * the ESCAPE array, which is NOT a def-use input — so cp_solve re-arms every kill node
         * explicitly when escape moves. This is the sixth instance of that bug class; without
         * the re-arm the fixpoint converges to a stale, UNSOUND answer. */
        int prev = eng->mem_prev[vi];
        int cell = eng->mem_cell[vi];
        const cp_pts_t* ph = (prev >= 0 && prev < eng->vnode_count)
                             ? eng->vnodes[prev]->heap : NULL;
        for (int o = 0; o < eng->obj_count; o++) {
            size_t base = (cell >= 0 && cell < eng->mem_cell_count)
                          ? (size_t)o * eng->mem_cell_count + cell : (size_t)-1;
            bool clob = (base != (size_t)-1) && eng->clobbered[base];
            if (clob) {
                /* §42 — the callee wrote this cell. If the write is fully captured (not
                 * INCOMPLETE), REPLACE the conservative CP_OBJ_EXT with the mapped value
                 * `inject[o,cell]` (∪ the pre-call value, sound for a conditional write). An
                 * INCOMPLETE write — a bottom sub-call could also have touched it — keeps EXT. */
                if (eng->inject_bad && eng->inject_bad[base]) {
                    cp_pts_add(eng, &s[o], CP_OBJ_EXT);
                    cp_pts_add(eng, &s[o], CP_OBJ_NULL);
                } else {
                    if (ph) for (int q = 0; q < words; q++) s[o].bits[q] |= ph[o].bits[q];
                    const uint64_t* inj = &eng->inject[base * (size_t)words];
                    for (int q = 0; q < words; q++) s[o].bits[q] |= inj[q];
                }
                continue;
            }
            /* Not clobbered. Gate 5 — the cell survives if the callee could not have written it:
             * NoEscape (handed to nobody), OR an ArgEscape object passed only to FULLY-CAPTURED
             * calls (bodied Java, whose summaries list every write; not a bottom method or native)
             * and not reachable from any bottom-passed operand (`obj_bottom` is transitive over
             * heap edges, Choi Fig 6 — a ref array's backing behind its overlay is covered).
             * A GlobalEscape object (a static, a global-reachable heap object) is NEVER safe: any
             * callee can reach and write it WITHOUT being handed it, so obj_bottom cannot see the
             * write — it must always drop to EXT (test_pts_static_is_killed_by_a_call). And once
             * this method makes ANY bottom/native call, an ArgEscape object may be reachable from
             * a global through the caller's disposition this method cannot see (Choi §4.5, the
             * bottom graph is only args-reachable), so no ArgEscape cell survives it either
             * (test_pts_call_kills_the_cell). NoEscape is never handed out, so it stays safe. */
            bool survives = cp_obj_survives_call(eng, o);
            if (survives) {
                /* ⊥ now, re-armed when the interrupted version's heap appears (mem_dep). */
                if (ph) for (int q = 0; q < words; q++) s[o].bits[q] |= ph[o].bits[q];
                continue;
            }
            cp_pts_add(eng, &s[o], CP_OBJ_EXT);    /* whatever the callee left there */
            cp_pts_add(eng, &s[o], CP_OBJ_NULL);
        }
    } else if (k == CP_MEM_WIDE) {
        /* An invoke kills every cell of every object: a local object may have been
         * passed to the callee, and without the escape lattice (stage 4) we cannot
         * say it was not. ONE name shadows ALL cells here, so this row cannot name a
         * per-cell phantom — the catch-all is what CP_OBJ_EXT is for, and every
         * consumer reads it as "unknown". A call therefore coarsens the heap back to
         * the catch-all; the cell phantoms sharpen what a method is HANDED. */
        for (int o = 0; o < eng->obj_count; o++) {
            cp_pts_add(eng, &s[o], CP_OBJ_EXT);
            cp_pts_add(eng, &s[o], CP_OBJ_NULL);
        }
    } else {                                   /* CP_MEM_STORE */
        int prev = eng->mem_prev[vi], ov = eng->mem_obj[vi], xv = eng->mem_val[vi];
        const cp_pts_t* ph = (prev >= 0 && prev < eng->vnode_count)
                             ? eng->vnodes[prev]->heap : NULL;
        cp_pts_t stored = (xv >= 0 && xv < eng->vnode_count) ? eng->vnodes[xv]->pts
                                                             : (cp_pts_t){ NULL };
        cp_pts_t target = (ov >= 0 && ov < eng->vnode_count) ? eng->vnodes[ov]->pts
                                                             : (cp_pts_t){ NULL };
        if (ph) memcpy(s[0].bits, ph[0].bits, bytes);   /* the version reaching it */

        if (ov < 0) {
            /* A STATIC (spec §2's `global.set(G,x)`). No receiver: the cell IS the
             * location, so there is exactly one row to write — and the write is STRONG.
             * `S.f = x` overwrites the whole global; there is no second object it might
             * really have been, which is the only reason a field store has to stay weak.
             * (§2 writes it as `pts(G) ∪= pts(x)` because its `G` is a whole-program
             * fact with no memory-SSA version; here the version reaching this store is
             * already in hand, so the kill is both sound and stronger.) */
            int row = cp_static_row(eng, vi < eng->mem_rows ? eng->mem_cell[vi] : -1);
            if (row >= 0) {
                memset(s[row].bits, 0, (size_t)words * sizeof(uint64_t));
                cp_pts_union(eng, &s[row], stored);
            } else {                           /* no cell: fail closed, kill everything */
                for (int o = 0; o < eng->obj_count; o++) cp_pts_union(eng, &s[o], stored);
            }
        } else if (target.bits) {
            /* The receiver's OBJECTS — ⊥null is not one of them. Storing through null
             * writes no field, it throws (JLS §15.11), so the null object must never
             * be a write target. It is in the receiver's pts because the receiver MAY
             * be null; that is a nullability fact, not a store target. (With one
             * shared `Oext` this was invisible: every unknown aliased every other, so
             * polluting row ⊥null changed nothing observable. With a phantom per
             * parameter it is immediately visible — a store through p reappears at a
             * load through q, through the null row they share.) */
            int nn_count = cp_pts_count(eng, target)
                         - (cp_pts_has(eng, target, CP_OBJ_NULL) ? 1 : 0);
            int nn_first = -1;
            for (int w = 0; w < words && nn_first < 0; w++) {
                uint64_t word = target.bits[w];
                if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
                if (word) nn_first = (w << 6) + __builtin_ctzll(word);
            }
            /* §2's strong update: "iff pts(p) is a singleton {O}" — O is ONE CONCRETE
             * RUNTIME OBJECT (the VFG paper's Rule 3 / Theorem 3), NOT merely a set of
             * size one. A phantom names an unknown object and two phantoms may alias
             * at runtime, so overwriting one's field while leaving the other's stale
             * would be unsound. Weak is always sound; strong needs a concrete object.
             *
             * AND ONE LOCATION. A concrete object gives a field store one location
             * (O.f); an ARRAY-ELEMENT cell is keyed by element type and summarizes
             * every index of O, so an ArrayStore's target is many locations behind one
             * row and must stay WEAK even on a concrete array (mem_elem; CWZ PLDI'90).
             * The strong reading here made s[0]=a; s[1]=b; s[2]=c keep only {c}: a
             * false singleton that devirtualized a rotating dispatch into a failing
             * ref.cast (bench/VirtRepro.java). */
            bool strong = nn_count == 1 && nn_first >= 0
                       && cp_obj_is_concrete(eng, nn_first)
                       && !(vi < eng->mem_rows && eng->mem_elem[vi]);
            for (int w = 0; w < words; w++) {  /* only the rows the receiver names */
                uint64_t word = target.bits[w];
                if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
                while (word) {
                    int o = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (strong)                /* §2: replace, not ∪ */
                        memset(s[o].bits, 0, (size_t)words * sizeof(uint64_t));
                    cp_pts_union(eng, &s[o], stored);
                }
            }
        }
    }

    if (!cp_heap_differs(eng, v->heap, s)) return false;
    memcpy(v->heap[0].bits, s[0].bits, bytes);
    return true;
}

static bool cp_spine_reachable(const cp_engine_t* eng, int i);

/* ── §6, LATTICE E: ESCAPE ──────────────────────────────────────────────────
 *
 * `NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥)`, meet = min, MONOTONE DOWNWARD.
 *
 * IN THE ONE FIXPOINT, NOT BESIDE IT. §6 describes the transfer as "escape's Fig-6
 * worklist", and taken alone that reads like a standalone pass. §8's membership test says
 * otherwise: "every lattice is a per-node monotone element whose transfer reads only (a)
 * def-use edges and (b) φ/region inputs", and §9 puts every lattice in the SAME fixpoint.
 * So the FACT is keyed on the abstract object (§6's own domain — an array over Obj, not a
 * vnode field), and the TRANSFER is per-node: each node LOWERS the state of every object
 * its operands name. It re-runs inside cp_solve as pts grows. pts only grows, escape only
 * descends, both are bounded — so the combined loop still terminates.
 *
 * The `meet at φ` §6 asks for is free: a φ's pts is the union of its inputs', so an object
 * that reaches an escaping use through ANY path is lowered by that use. No φ handling of
 * its own.
 */
static void cp_escape_lower(cp_engine_t* eng, int obj, cp_escape_t to) {
    if (obj < 0 || obj >= eng->obj_count) return;
    if (eng->escape[obj] < to) eng->escape[obj] = to;   /* min — descend only */
}

/* Lower every object a value may name. The pts set IS the "value edge" §6 propagates
 * along; ⊥null is not an object and cannot escape. */
static void cp_escape_lower_pts(cp_engine_t* eng, int vn, cp_escape_t to) {
    if (vn < 0 || vn >= eng->vnode_count) return;
    cp_pts_t p = eng->vnodes[vn]->pts;
    if (!p.bits) return;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            cp_escape_lower(eng, o, to);
        }
    }
}

/* The WORST state of anything a receiver may name — what a store THROUGH it confers on the
 * value stored. GlobalEscape if the receiver may be a global-reachable object, and so on.
 * An empty receiver set confers nothing (the store cannot execute). */
static cp_escape_t cp_escape_of_pts(cp_engine_t* eng, int vn) {
    cp_escape_t worst = CP_ESC_NONE;
    if (vn < 0 || vn >= eng->vnode_count) return worst;
    cp_pts_t p = eng->vnodes[vn]->pts;
    if (!p.bits) return worst;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            if (eng->escape[o] > worst) worst = eng->escape[o];
        }
    }
    return worst;
}

/* SEEDS. An EXTERNAL object — a phantom (`Oext@param`), an `Oret@callee`, the catch-all —
 * is already reachable from outside the method, so it IS ArgEscape. That is not a special
 * case bolted on: it is what makes §6's "stored into a `param`-reachable object" fall out of
 * the ordinary heap rule below, with no rule of its own.
 *
 * §6: "the receiver of a class that OVERRIDES finalize()" is GlobalEscape — the finalizer
 * thread reaches it. That is a property of the allocation's CLASS, so it is seeded here;
 * sema owns the override question (JLS §8.4.8), this does not re-derive it. */
static void cp_escape_seed(cp_engine_t* eng) {
    for (int o = 0; o < eng->obj_count; o++) {
        switch (cp_obj_kind(eng, o)) {
        case CP_OBJK_NULL:
            eng->escape[o] = CP_ESC_NONE;       /* ⊥null is not an object; it cannot escape */
            break;

        /* §6's ArgEscape IS "reachable from a formal". A param phantom is the formal, so it
         * seeds exactly there — and not lower: §7's MapsTo is what sinks it to GlobalEscape
         * in a caller whose actual was itself global. Seeding it GlobalEscape here would
         * throw away the distinction the summary exists to carry. */
        case CP_OBJK_PARAM:
            eng->escape[o] = CP_ESC_ARG;
            break;

        /* A CELL phantom is "what some pre-existing object holds in cell c". §1 says a
         * phantom stands for anything reachable from a formal OR A GLOBAL, and this one name
         * covers both — we cannot tell which object owns the cell. §2 pins the static case
         * outright ("a static-field global … is external: pts = {Oext}, GlobalEscape"), and a
         * field cell's owner may equally be a global's contents. FAIL-CLOSED. */
        case CP_OBJK_CELL:
            eng->escape[o] = CP_ESC_GLOBAL;
            break;

        /* §7: "a ref returned by a native escapes". The callee's body is not ours to see, so
         * it may already have stored what it hands back into a global. */
        case CP_OBJK_RET:
            eng->escape[o] = CP_ESC_GLOBAL;
            break;

        /* THE CATCH-ALL. It stands for ANY pre-existing object we have no better name for —
         * and an invoke's wide kill re-names every heap cell to exactly this (cp_node_heap's
         * CP_MEM_WIDE). So it subsumes the statics, and cannot be less escaped than the worst
         * object it stands for. Seeding it ArgEscape is what made a store through a static
         * confer ArgEscape where §6 requires GlobalEscape. */
        case CP_OBJK_CATCHALL:
            eng->escape[o] = CP_ESC_GLOBAL;
            break;

        /* An allocation of OURS starts at ⊤ and is lowered only by a real source. */
        case CP_OBJK_SITE: {
            eng->escape[o] = CP_ESC_NONE;
            int cls = cp_exact_class_of(eng, o);
            if (cls >= 0 && eng->sema && sema_class_overrides_finalize(eng->sema, cls))
                eng->escape[o] = CP_ESC_GLOBAL;   /* the finalizer thread reaches it */
            break;
        }
        case CP_OBJK_NONE:
            eng->escape[o] = CP_ESC_GLOBAL;       /* unnameable ⟹ fail closed */
            break;
        }
    }
}

/* §7's BOTTOM GRAPH, applied to every call in an expression tree.
 *
 * §6's ArgEscape source is "passed to a `call` whose SUMMARY marks that parameter escaping".
 * There are no summaries yet — those are §7, staged at §9.5 — so every callee is a §7 BOTTOM
 * METHOD ("native / abstract / NOT-YET-ANALYZED"), and §7's bottom graph is explicit: "a ref
 * passed to a native → ArgEscape". Receiver included: it is argument 0 of the dispatch.
 *
 * This is why an object built by a DECLARED constructor is ArgEscape at this stage — the
 * ctor call takes it as a receiver, and the ctor has no summary yet. That is the spec's own
 * staging, not a limitation to route around: §7's summaries are what let a ctor that does not
 * leak `this` keep its object NoEscape. Sites with no ctor call (the §10.7 array wrappers, an
 * implicit default ctor) are already scalar-replaceable today. */
static unsigned char cp_callee_param_class(compiler_ctx_t* ctx, int callee,
                                           bool is_this, int slot);

/* key → this method's cell index, or -1 if this method has no such cell (nobody loads it,
 * so nothing to clobber). Lookup-only — never interns. */
static int cp_cell_lookup(cp_engine_t* eng, uint32_t key) {
    void* f = bbq_htree_search(eng->mem_cell_idx, (uint32_t)(key + 1));
    return f ? (int)((uintptr_t)f - 1) : -1;
}

/* §7's MapsTo ROOT escape lowering at ONE arg/receiver (Choi Fig 7): the callee's summary's
 * per-parameter class decides how much the actual escapes — GlobalEscape propagates, ArgEscape
 * is the conservative bottom, CLEAN does not lower it (the ctor-that-doesn't-leak win). The
 * §7.2 written-cell clobber and the deep GlobalEscape ride cp_mapsto_graph over the sub-graph,
 * which subsumes the direct-formal-write case (a root is a sub-graph object too). */
/* Gate 5 (§42) — mark every object `actual` may name as passed to a BOTTOM method: its summary
 * cannot list what it writes, so the callee could mutate ANY of the object's cells, and an
 * un-clobbered row of it can no longer survive a call. inject_moved re-arms the kills (obj_bottom
 * is not a def-use input, exactly like escape and clobbered). */
static void cp_mark_bottom(cp_engine_t* eng, sir_node_t* actual) {
    if (!eng->obj_bottom || !actual) return;
    int vi = cp_vnode_of(eng, actual);
    if (vi < 0 || vi >= eng->vnode_count) return;
    cp_pts_t p = eng->vnodes[vi]->pts;
    if (!p.bits) return;
    /* Choi Fig 6 reachability: a bottom method holds `actual` AND everything reachable from it via
     * HEAP EDGES — so it could write any of their cells. Marking only the direct arg misses, e.g.,
     * a ref array's BACKING behind its overlay, and Gate 5 then survives a cell the callee wrote.
     * Seed the arg's pts, then BFS the heap (union over versions; obj_bottom is the visited set). */
    int* wl = NULL;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            if (o == CP_OBJ_EXT || eng->obj_bottom[o]) continue;
            eng->obj_bottom[o] = true; eng->inject_moved = true;
            bbq_vec_push(wl, o);
        }
    }
    for (int h = 0; h < (int)bbq_vec_len(wl); h++) {
        int o = wl[h];
        for (int v = 0; v < eng->mem_rows; v++) {
            const cp_pts_t* heap = eng->vnodes[v]->heap;
            if (!heap || !heap[o].bits) continue;
            for (int w = 0; w < eng->obj_words; w++) {
                uint64_t word = heap[o].bits[w];
                if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
                while (word) {
                    int o2 = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (o2 == CP_OBJ_EXT || eng->obj_bottom[o2]) continue;
                    eng->obj_bottom[o2] = true; eng->inject_moved = true;
                    bbq_vec_push(wl, o2);
                }
            }
        }
    }
    bbq_vec_free(wl);
}

/* A NATIVE callee's summary is a HAND-written approximation, not read out of a body — so its
 * clobber set cannot be trusted COMPLETE the way a bodied Java method's is (Gate 5 needs
 * completeness). Treat its operands as bottom-leaked, exactly like an unsummarized method. */
static bool cp_callee_is_native(compiler_ctx_t* ctx, int callee) {
    if (!ctx || !ctx->sema || !ctx->methods || callee < 0 || callee >= ctx->method_count) return false;
    const sir_method_t* m = ctx->methods[callee];
    if (!m) return false;
    const sema_class_t* sc = sema_get_class(ctx->sema, m->class_id);
    if (!sc || m->method_id < 0 || m->method_id >= (int)bbq_vec_len((void*)sc->methods)) return false;
    return (sc->methods[m->method_id].modifiers & ACC_NATIVE) != 0;
}

static void cp_mapsto_arg(cp_engine_t* eng, compiler_ctx_t* ctx, int callee,
                          sir_node_t* actual, bool is_this, int slot) {
    const compiler_summary_t* sm = compiler_method_summary(ctx, callee);
    if (!sm || !sm->computed || cp_callee_is_native(ctx, callee))
        cp_mark_bottom(eng, actual);                         /* Gate 5: freely-mutable operand */
    unsigned char c = cp_callee_param_class(ctx, callee, is_this, slot);
    cp_escape_t esc = (c == COMPILER_ESC_GLOBAL) ? CP_ESC_GLOBAL
                    : (c == COMPILER_ESC_NONE)   ? CP_ESC_NONE    /* CLEAN — do not lower */
                                                 : CP_ESC_ARG;
    if (esc > CP_ESC_NONE) cp_escape_lower_pts(eng, cp_vnode_of(eng, actual), esc);
}

/* Grow the MapsToObj scratch to hold `need` summary objects — one alloc per new high-water mark,
 * reused across every later call site and escape sweep (the hot fixpoint allocates nothing). */
static void cp_mapsto_reserve(cp_engine_t* eng, int need) {
    if (need <= eng->mto_cap) return;
    eng->mto     = (cp_pts_t*)bbq_arena_alloc(eng->arena, (size_t)need * sizeof(cp_pts_t));
    eng->mto_wl  = (int*)bbq_arena_alloc(eng->arena, (size_t)need * sizeof(int));
    eng->mto_inq = (bool*)bbq_arena_alloc(eng->arena, (size_t)need * sizeof(bool));
    for (int k = 0; k < need; k++) { eng->mto[k] = cp_pts_new(eng); eng->mto_inq[k] = false; }
    if (!eng->mto_tgt.bits) eng->mto_tgt = cp_pts_new(eng);
    eng->mto_cap = need;
}

/* Union `src` into `*dst`; true if `*dst` grew. */
static bool cp_pts_union_grew(const cp_engine_t* eng, cp_pts_t* dst, cp_pts_t src) {
    if (!src.bits || !dst->bits) return false;
    bool grew = false;
    for (int i = 0; i < eng->obj_words; i++) {
        uint64_t before = dst->bits[i];
        uint64_t after  = before | src.bits[i];
        if (after != before) { dst->bits[i] = after; grew = true; }
    }
    return grew;
}

/* pts of `{from}.key` in the CALLER — Fig 7's `n̂_o ∈ PointsTo(f_er)` for the caller field node
 * matching fid `key`. Over-approximated by the union of EVERY version of the caller's cell `key`
 * (its heaps are stable within one escape sub-fixpoint; unioning versions can only reach MORE
 * objects, which is sound for a clobber). The caller having no such cell ⟹ it can observe
 * nothing through this field ⟹ nothing to follow, nothing to clobber. */
/* cell → memory-SSA rows, inverted ONCE (mem_cell[] is fixed after the stage-1b
 * builder). cp_follow_field ran a full mem_rows scan per call inside the mapsto
 * worklist — the profile's second-largest churn. */
static void cp_cell_rows_build(cp_engine_t* eng) {
    bbq_arena* a = eng->arena;
    int mcc = eng->mem_cell_count > 0 ? eng->mem_cell_count : 1;
    eng->cell_row_off = (int*)bbq_arena_alloc(a, (size_t)mcc * sizeof(int));
    eng->cell_row_cnt = (int*)bbq_arena_alloc(a, (size_t)mcc * sizeof(int));
    memset(eng->cell_row_cnt, 0, (size_t)mcc * sizeof(int));
    for (int v = 0; v < eng->mem_rows; v++) {
        int c = eng->mem_cell[v];
        if (c >= 0 && c < eng->mem_cell_count) eng->cell_row_cnt[c]++;
    }
    int total = 0;
    for (int c = 0; c < eng->mem_cell_count; c++) {
        eng->cell_row_off[c] = total;
        total += eng->cell_row_cnt[c];
    }
    eng->cell_row_list = (int*)bbq_arena_alloc(a, (size_t)(total > 0 ? total : 1) * sizeof(int));
    int* cur = (int*)bbq_arena_alloc(a, (size_t)mcc * sizeof(int));
    memset(cur, 0, (size_t)mcc * sizeof(int));
    for (int v = 0; v < eng->mem_rows; v++) {
        int c = eng->mem_cell[v];
        if (c >= 0 && c < eng->mem_cell_count)
            eng->cell_row_list[eng->cell_row_off[c] + cur[c]++] = v;
    }
}

static void cp_follow_field(cp_engine_t* eng, int c, cp_pts_t from, cp_pts_t* out) {
    if (c < 0 || c >= eng->mem_cell_count || !from.bits) return;
    if (!eng->cell_row_off) cp_cell_rows_build(eng);
    for (int ri = eng->cell_row_off[c]; ri < eng->cell_row_off[c] + eng->cell_row_cnt[c]; ri++) {
        int v = eng->cell_row_list[ri];
        const cp_pts_t* h = eng->vnodes[v]->heap;
        if (!h) continue;
        for (int w = 0; w < eng->obj_words; w++) {
            uint64_t word = from.bits[w];
            if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
            while (word) {
                int o = (w << 6) + __builtin_ctzll(word);
                word &= word - 1;
                cp_pts_union(eng, out, h[o]);
            }
        }
    }
}

/* Fig 7 (UpdateCallerNodes / UpdateNodes) at ONE call site: instantiate the callee summary's
 * NonLocalGraph into the caller. Seed MapsToObj(root) = PointsTo(actual) for each formal
 * (Statements 27-28), then walk field edges in lock-step with the caller's own heap
 * (Statements 30-40, `MapsToObj` grows monotonically, so the visited guard is the grow test).
 * For every mapped caller object it (a) clobbers the cells the callee wrote on that summary
 * object — §7.2, so the memory KILL will not preserve a `p.child.x` write's stale pre-call
 * value — and (b) propagates GlobalEscape (§4.4: ONLY GlobalEscape propagates to the images;
 * ArgEscape is the caller's own to decide, and cp_mapsto_arg's root lowering already covers the
 * conservative case). Terminates: MapsToObj only grows, bounded by obj_count × n_obj. */
static void cp_mapsto_graph(cp_engine_t* eng, compiler_ctx_t* ctx, int callee,
                            sir_node_t* receiver, sir_node_t** args, int args_count) {
    const compiler_summary_t* s = compiler_method_summary(ctx, callee);
    /* A BOTTOM or NATIVE callee (Choi §4.5) touches args-reachable objects + globals, and — via
     * the caller's own disposition it cannot see — potentially any ArgEscape object. So once this
     * method makes such a call, Gate 5 must not survive an ArgEscape cell (test_pts_call_kills). */
    if (!s || !s->computed || cp_callee_is_native(ctx, callee)) {
        if (!eng->has_bottom_call) { eng->has_bottom_call = true; eng->inject_moved = true; }
    }
    if (!s || s->n_obj == 0) return;   /* NOT gated on mem_cell_count: a pass-through method has
                                        * no cells, yet must still RELAY the callee's writes into
                                        * its own summary (clobx below) and propagate GlobalEscape. */
    /* The summary's key arrays translated to LOCAL cells ONCE per callee (wcells
     * first, then edges): the keys and the caller's interning are both fixed for
     * this engine's lifetime, and the loops below revisit them per object bit per
     * solver visit — re-searching there was 124.9M htree probes per jre build. */
    int nw = s->wcell_off[s->n_obj], ne = s->edge_off[s->n_obj];
    if (!eng->mapsto_tr) {
        int nm = ctx->method_count > 0 ? ctx->method_count : 1;
        eng->mapsto_tr = (int**)bbq_arena_alloc(eng->arena, (size_t)nm * sizeof(int*));
        memset(eng->mapsto_tr, 0, (size_t)nm * sizeof(int*));
    }
    int* tr = (callee >= 0 && callee < ctx->method_count) ? eng->mapsto_tr[callee] : NULL;
    if (!tr) {
        tr = (int*)bbq_arena_alloc(eng->arena,
                                   (size_t)(nw + ne > 0 ? nw + ne : 1) * sizeof(int));
        for (int i = 0; i < nw; i++)
            tr[i] = (eng->mem_cell_count > 0) ? cp_cell_lookup(eng, s->wcell_key[i]) : -1;
        for (int i = 0; i < ne; i++)
            tr[nw + i] = (eng->mem_cell_count > 0) ? cp_cell_lookup(eng, s->edge_key[i]) : -1;
        if (callee >= 0 && callee < ctx->method_count) eng->mapsto_tr[callee] = tr;
    }
    cp_mapsto_reserve(eng, s->n_obj);
    int words = eng->obj_words ? eng->obj_words : 1;
    for (int k = 0; k < s->n_obj; k++) memset(eng->mto[k].bits, 0, (size_t)words * sizeof(uint64_t));
    int wn = 0;
    /* base case: MapsToObj(root) = PointsTo(actual). The receiver maps to `this`; arg i maps to
     * the callee's parameter i — the summary's slot_obj/slot_escape are PARAMETER-indexed (the
     * producer resolved each through sema_param_slot), so no `this`/slot arithmetic here. */
    struct { int sid; sir_node_t* e; } roots[256];
    int nr = 0;
    if (s->this_obj >= 0 && receiver) { roots[nr].sid = s->this_obj; roots[nr].e = receiver; nr++; }
    for (int i = 0; i < args_count && i < s->slot_count && nr < 256; i++)
        if (s->slot_obj[i] >= 0 && args[i]) { roots[nr].sid = s->slot_obj[i]; roots[nr].e = args[i]; nr++; }
    for (int r = 0; r < nr; r++) {
        int sid = roots[r].sid, vi = cp_vnode_of(eng, roots[r].e);
        if (vi < 0 || vi >= eng->vnode_count) continue;
        if (cp_pts_union_grew(eng, &eng->mto[sid], eng->vnodes[vi]->pts) && !eng->mto_inq[sid]) {
            eng->mto_inq[sid] = true; eng->mto_wl[wn++] = sid;
        }
    }
    /* recursion (Fig 7 Statements 30-40). */
    while (wn > 0) {
        int k = eng->mto_wl[--wn];
        eng->mto_inq[k] = false;
        cp_pts_t m = eng->mto[k];
        for (int w = 0; w < eng->obj_words; w++) {
            uint64_t word = m.bits[w];
            if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
            while (word) {
                int co = (w << 6) + __builtin_ctzll(word);
                word &= word - 1;
                for (int wi = s->wcell_off[k]; wi < s->wcell_off[k + 1]; wi++) {
                    int c = tr[wi];
                    if (c < 0 || c >= eng->mem_cell_count) {
                        /* No local cell for this key — THIS method never mentions the field, so
                         * its own kills need nothing, but its SUMMARY must relay the write
                         * (spec §7.2 transitive). Dropping it here is the §37f miscompile. */
                        bbq_vec_push(eng->clobx_obj, co);
                        bbq_vec_push(eng->clobx_key, s->wcell_key[wi]);
                        continue;
                    }
                    bool* cl = &eng->clobbered[(size_t)co * eng->mem_cell_count + c];
                    if (!*cl) { *cl = true; eng->clobbered_moved = true; }
                }
                if (s->obj_escape[k] == CP_ESC_GLOBAL) cp_escape_lower(eng, co, CP_ESC_GLOBAL);
            }
        }
        for (int e = s->edge_off[k]; e < s->edge_off[k + 1]; e++) {
            memset(eng->mto_tgt.bits, 0, (size_t)words * sizeof(uint64_t));
            cp_follow_field(eng, tr[nw + e], eng->mto[k], &eng->mto_tgt);
            int d = s->edge_dst[e];
            if (d >= 0 && d < s->n_obj && cp_pts_union_grew(eng, &eng->mto[d], eng->mto_tgt)
                && !eng->mto_inq[d]) { eng->mto_inq[d] = true; eng->mto_wl[wn++] = d; }
        }
    }
    /* §42 (Fig 7 "Updating Caller Edges") — mto is FINAL for this call now. Inject each callee
     * edge `k.cell = d` back into the caller: every caller object o ∈ mto[k] may hold mto[d] in
     * o.cell, so the memory KILL can replace CP_OBJ_EXT with it. The wcell flags add null and
     * mark INCOMPLETE cells (keep EXT). Monotone; inject_moved re-arms the kills like
     * clobbered_moved. */
    if (eng->inject && eng->mem_cell_count > 0) {
        int mcc = eng->mem_cell_count;
        for (int k = 0; k < s->n_obj; k++) {
            if (!eng->mto[k].bits) continue;
            for (int ei = s->edge_off[k]; ei < s->edge_off[k + 1]; ei++) {
                int c = tr[nw + ei];
                int d = s->edge_dst[ei];
                if (c < 0 || c >= mcc || d < 0 || d >= s->n_obj || !eng->mto[d].bits) continue;
                for (int w = 0; w < eng->obj_words; w++) {
                    uint64_t caller = eng->mto[k].bits[w];
                    if (w == 0) caller &= ~((uint64_t)1 << CP_OBJ_NULL);
                    while (caller) {
                        int o = (w << 6) + __builtin_ctzll(caller);
                        caller &= caller - 1;
                        if (o == CP_OBJ_EXT) continue;
                        size_t base = (size_t)o * mcc + c;
                        uint64_t* inj = &eng->inject[base * (size_t)words];
                        bool had_ext = false;
                        for (int q = 0; q < words; q++) {
                            uint64_t src = eng->mto[d].bits[q];
                            if (q == 0 && (src & ((uint64_t)1 << CP_OBJ_EXT))) {
                                had_ext = true; src &= ~((uint64_t)1 << CP_OBJ_EXT);
                            }
                            uint64_t nw = inj[q] | src;
                            if (nw != inj[q]) { inj[q] = nw; eng->inject_moved = true; }
                        }
                        /* Oext in the mapped value = the callee's write is not fully captured
                         * (the entry value, or a bottom sub-call) — keep the conservative EXT. */
                        if (had_ext && !eng->inject_bad[base]) {
                            eng->inject_bad[base] = true; eng->inject_moved = true;
                        }
                    }
                }
            }
            /* Completeness (plan: "receiver CLEAN ⟹ no bottom sub-call can write that cell") is
             * PER-OBJECT: if the callee leaked object k to a bottom method, a bottom sub-call could
             * have written any of k's cells with something the summary cannot see, so every caller
             * object o ∈ mto[k] keeps CP_OBJ_EXT for k's written cells. Otherwise the mapped edge is
             * the precise value (+null if MAYBE_NULL). */
            bool leaked = s->obj_leaked && s->obj_leaked[k];
            for (int wi = s->wcell_off[k]; wi < s->wcell_off[k + 1]; wi++) {
                int c = tr[wi];
                if (c < 0 || c >= mcc) continue;
                unsigned char f = s->wcell_flags ? s->wcell_flags[wi] : 0;
                for (int w = 0; w < eng->obj_words; w++) {
                    uint64_t caller = eng->mto[k].bits[w];
                    if (w == 0) caller &= ~((uint64_t)1 << CP_OBJ_NULL);
                    while (caller) {
                        int o = (w << 6) + __builtin_ctzll(caller);
                        caller &= caller - 1;
                        if (o == CP_OBJ_EXT) continue;
                        size_t base = (size_t)o * mcc + c;
                        if ((f & COMPILER_WCELL_MAYBE_NULL)) {
                            uint64_t* inj = &eng->inject[base * (size_t)words];
                            uint64_t nb = inj[0] | ((uint64_t)1 << CP_OBJ_NULL);
                            if (nb != inj[0]) { inj[0] = nb; eng->inject_moved = true; }
                        }
                        /* Keep EXT if the receiver leaked to a bottom method, OR this is a
                         * TRANSITIVE write whose precise value this callee never saw. */
                        if ((leaked || (f & COMPILER_WCELL_TRANSITIVE)) && !eng->inject_bad[base]) {
                            eng->inject_bad[base] = true; eng->inject_moved = true;
                        }
                    }
                }
            }
        }
    }
}

static void cp_escape_walk_calls(cp_engine_t* eng, sir_node_t* e) {
    if (!e) return;
    compiler_ctx_t* ctx = eng->ctx;
    switch (e->tag) {
    /* A VIRTUAL/INTERFACE site fans out to its finite defunctionalized target set (spec §7):
     * MapsTo each resolved target and JOIN — the escape lowerings meet (cp_escape_lower is
     * monotone-min, so running all targets is the sound join) and the clobbers union. If the set
     * is not fully known (a phantom / `Oret` receiver, an unresolvable target, an overflow), fall
     * back to §7's bottom graph: ArgEscape every operand (the memory wide-kill then drops their
     * cells, so no §7.2 entry is needed). */
    case SIR_INVOKEVIRTUAL: {
        int cls[16], mid[16], n = 0;
        if (ctx && cp_virtual_target_set(eng, cp_vnode_of(eng, e->invoke_virtual.obj),
                e->invoke_virtual.class_id, e->invoke_virtual.method_idx, cls, mid, 16, &n)) {
            for (int t = 0; t < n; t++) {
                int ce = compiler_method_index(ctx, cls[t], mid[t]);
                cp_mapsto_arg(eng, ctx, ce, e->invoke_virtual.obj, true, -1);
                for (int i = 0; i < e->invoke_virtual.args_count; i++)
                    cp_mapsto_arg(eng, ctx, ce, e->invoke_virtual.args[i], false, i);
                cp_mapsto_graph(eng, ctx, ce, e->invoke_virtual.obj,
                                e->invoke_virtual.args, e->invoke_virtual.args_count);
            }
        } else {
            cp_escape_lower_pts(eng, cp_vnode_of(eng, e->invoke_virtual.obj), CP_ESC_ARG);
            cp_mark_bottom(eng, e->invoke_virtual.obj);
            for (int i = 0; i < e->invoke_virtual.args_count; i++) {
                cp_escape_lower_pts(eng, cp_vnode_of(eng, e->invoke_virtual.args[i]), CP_ESC_ARG);
                cp_mark_bottom(eng, e->invoke_virtual.args[i]);
            }
        }
        break;
    }
    case SIR_INVOKEINTERFACE: {
        int cls[16], mid[16], n = 0;
        if (ctx && cp_virtual_target_set(eng, cp_vnode_of(eng, e->invoke_interface.obj),
                e->invoke_interface.class_id, e->invoke_interface.method_idx, cls, mid, 16, &n)) {
            for (int t = 0; t < n; t++) {
                int ce = compiler_method_index(ctx, cls[t], mid[t]);
                cp_mapsto_arg(eng, ctx, ce, e->invoke_interface.obj, true, -1);
                for (int i = 0; i < e->invoke_interface.args_count; i++)
                    cp_mapsto_arg(eng, ctx, ce, e->invoke_interface.args[i], false, i);
                cp_mapsto_graph(eng, ctx, ce, e->invoke_interface.obj,
                                e->invoke_interface.args, e->invoke_interface.args_count);
            }
        } else {
            cp_escape_lower_pts(eng, cp_vnode_of(eng, e->invoke_interface.obj), CP_ESC_ARG);
            cp_mark_bottom(eng, e->invoke_interface.obj);
            for (int i = 0; i < e->invoke_interface.args_count; i++) {
                cp_escape_lower_pts(eng, cp_vnode_of(eng, e->invoke_interface.args[i]), CP_ESC_ARG);
                cp_mark_bottom(eng, e->invoke_interface.args[i]);
            }
        }
        break;
    }
    /* SPECIAL / STATIC name ONE target ⟹ MapsTo consults its summary. The ctor path:
     * `new C()` lowers to InvokeSpecial(obj, C.<init>). */
    case SIR_INVOKESPECIAL: {
        int ce = ctx ? compiler_method_index(ctx, e->invoke_special.class_id,
                                             e->invoke_special.method_idx) : -1;
        /* The summary is parameter-indexed, so arg i is param i for every call kind. */
        cp_mapsto_arg(eng, ctx, ce, e->invoke_special.obj, true, -1);
        for (int i = 0; i < e->invoke_special.args_count; i++)
            cp_mapsto_arg(eng, ctx, ce, e->invoke_special.args[i], false, i);
        /* Fig 7's field-following: a REACHABLE object's written cell (p.child.x) + deep
         * GlobalEscape, which cp_mapsto_arg's per-root lowering cannot reach. */
        cp_mapsto_graph(eng, ctx, ce, e->invoke_special.obj,
                        e->invoke_special.args, e->invoke_special.args_count);
        break;
    }
    case SIR_INVOKESTATIC: {
        int ce = ctx ? compiler_method_index(ctx, e->invoke_static.class_id,
                                             e->invoke_static.method_idx) : -1;
        for (int i = 0; i < e->invoke_static.args_count; i++)
            cp_mapsto_arg(eng, ctx, ce, e->invoke_static.args[i], false, i);
        cp_mapsto_graph(eng, ctx, ce, NULL,
                        e->invoke_static.args, e->invoke_static.args_count);
        break;
    }
    default: break;
    }
    int n = sir_arity(e);
    for (int i = 0; i < n; i++) cp_escape_walk_calls(eng, sir_child(e, i));
}

/* Is the exception thrown at spine node `i` CAUGHT in this method? §6's refinement.
 *
 * The structural half (which regions enclose this throw, and which of their handlers are
 * real catch clauses) is indexed pre-solve by cp_index_try_regions — the DDCG's re-throwing
 * catch-all is not one of them (cp_handler_catches). This is the part that belongs in the
 * fixpoint: WHICH objects the thrown value may name is pts, and whether a catch handles
 * their class is JLS §11.2 — `sema_ref_is_subtype`, the one authority.
 *
 * ⚠ RESIDUAL, and it is ledger row §6/JLS §11.3's, not this rule's: a real catch clause that
 * itself LEAKS the exception (`catch (Error e) { throw e; }` — $ensure_init does exactly
 * this) still reads as containing it, because the handler's landing slot is opaque, so pts
 * cannot connect the caught object to the re-throw. Closing that is the exception-VALUE flow
 * (the handler's slot def becomes a φ of the region's thrown values), which is owed before
 * stage 5. This rule closes the SYNTHESIZED catch-all — the case that made every throw in
 * every try look contained — and does not pretend to close that one.
 *
 * FAIL-CLOSED, and every "no" here means the object escapes: an object whose exact class we
 * cannot name is not provably caught; a value that may name an object no enclosing handler
 * covers is not caught. Only "EVERY object it may name is caught right here" earns the
 * refinement. ⊥null is not an object — throwing null raises an NPE, which is a different
 * object with its own throw. */
static bool cp_throw_is_caught(cp_engine_t* eng, int i, int vn) {
    if (i < 0 || i >= eng->spine_count || !eng->throw_catches) return false;
    const int* catches = eng->throw_catches[i];
    if (!catches || !eng->sema) return false;
    if (vn < 0 || vn >= eng->vnode_count) return false;
    cp_pts_t p = eng->vnodes[vn]->pts;
    if (cp_pts_empty(eng, p)) return false;

    int ncatch = (int)bbq_vec_len((void*)catches);
    for (int w = 0; w < eng->obj_words; w++) {       /* O(|pts|), not O(|Obj|) */
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            int cls = cp_exact_class_of(eng, o);
            if (cls < 0) return false;           /* unknown class ⟹ prove nothing */
            bool caught = false;
            for (int k = 0; k < ncatch && !caught; k++)
                if (sema_ref_is_subtype(eng->sema, cls, catches[k])) caught = true;
            if (!caught) return false;           /* this one gets out */
        }
    }
    return true;
}

/* ONE sweep of the per-node transfer. Every node that lets a reference out of this method
 * LOWERS the state of every object that reference may name. Returns true if anything moved.
 *
 * The sweep is total (every reachable spine node, every round), which is what makes it immune
 * to the re-arm bug class that has bitten this file five times: a fact here reaches a node
 * through pts, not along a def-use edge, so a worklist keyed on def-use would silently miss
 * it. Nothing is enqueued, so nothing can be forgotten. */
static bool cp_escape_sweep(cp_engine_t* eng) {
    memcpy(eng->escape_prev, eng->escape, (size_t)eng->obj_count * sizeof(cp_escape_t));

    for (int i = 0; i < eng->spine_count; i++) {
        if (!cp_spine_reachable(eng, i)) continue;   /* §9: a dead region is never analyzed */
        sir_node_t* n = eng->spine[i];
        switch (n->tag) {
        /* §6: "O flows to a `return`". */
        case SIR_RETURN:
            cp_escape_lower_pts(eng, cp_vnode_of(eng, n->return_.value), CP_ESC_ARG);
            break;
        /* §6: "reachable from a `global.set` (static-field global)" ⟹ GlobalEscape. */
        case SIR_PUTSTATIC:
            cp_escape_lower_pts(eng, cp_vnode_of(eng, n->put_static.value), CP_ESC_GLOBAL);
            break;
        /* §6: "a throw marks its exception object escaping ONLY IF it can leave the method —
         * a throw caught by an enclosing try_table in the same method does NOT escape; an
         * uncaught (re-thrown) one is ArgEscape via the method's exceptional exit." */
        case SIR_THROW:
            if (!cp_throw_is_caught(eng, i, cp_vnode_of(eng, n->throw_.ref)))
                cp_escape_lower_pts(eng, cp_vnode_of(eng, n->throw_.ref), CP_ESC_ARG);
            break;
        /* THE HEAP RULE, and it is BOTH of §6's "stored into …" sources at once. A store
         * confers the RECEIVER's state on the value stored: into an already-GlobalEscape
         * object ⟹ GlobalEscape; into a param-reachable one ⟹ ArgEscape (a phantom seeds
         * there, which is the whole reason that source needs no rule of its own); into a
         * local that has not escaped ⟹ nothing at all. That last clause is what keeps the
         * lattice from collapsing to "everything escapes". */
        case SIR_PUTFIELD: {
            cp_escape_t r = cp_escape_of_pts(eng, cp_vnode_of(eng, n->put_field.obj));
            if (r > CP_ESC_NONE)
                cp_escape_lower_pts(eng, cp_vnode_of(eng, n->put_field.value), r);
            break;
        }
        case SIR_ARRAYSTORE: {
            cp_escape_t r = cp_escape_of_pts(eng, cp_vnode_of(eng, n->array_store.arr));
            if (r > CP_ESC_NONE)
                cp_escape_lower_pts(eng, cp_vnode_of(eng, n->array_store.value), r);
            break;
        }
        default: break;
        }
        /* §7's bottom graph. Calls sit in expression position, so this walks the node's
         * expression trees rather than switching on the spine tag. */
        int ar = sir_arity(n);
        for (int j = 0; j < ar; j++)
            cp_escape_walk_calls(eng, sir_child(n, j));
    }

    for (int o = 0; o < eng->obj_count; o++)
        if (eng->escape_prev[o] != eng->escape[o]) return true;
    return false;
}

/* Lattice E's domain is the Obj set, which is SYNTACTIC — so it is fixed here, before
 * the solve, exactly like pts's. */
static void cp_escape_init(cp_engine_t* eng) {
    int n = eng->obj_count > 0 ? eng->obj_count : 1;
    eng->escape      = (cp_escape_t*)bbq_arena_alloc(eng->arena, (size_t)n * sizeof(cp_escape_t));
    eng->escape_prev = (cp_escape_t*)bbq_arena_alloc(eng->arena, (size_t)n * sizeof(cp_escape_t));
    size_t cm = (size_t)n * (size_t)(eng->mem_cell_count > 0 ? eng->mem_cell_count : 1);
    eng->clobbered = (bool*)bbq_arena_alloc(eng->arena, cm * sizeof(bool));
    memset(eng->clobbered, 0, cm * sizeof(bool));
    eng->clobbered_moved = false;
    int words = eng->obj_words > 0 ? eng->obj_words : 1;
    eng->inject = (uint64_t*)bbq_arena_alloc(eng->arena, cm * (size_t)words * sizeof(uint64_t));
    memset(eng->inject, 0, cm * (size_t)words * sizeof(uint64_t));
    eng->inject_bad = (bool*)bbq_arena_alloc(eng->arena, cm * sizeof(bool));
    memset(eng->inject_bad, 0, cm * sizeof(bool));
    eng->obj_bottom = (bool*)bbq_arena_alloc(eng->arena, (size_t)n * sizeof(bool));
    memset(eng->obj_bottom, 0, (size_t)n * sizeof(bool));
    eng->inject_moved = false;
    eng->has_bottom_call = false;
    cp_escape_seed(eng);
}

/* Saturate lattice E against the CURRENT pts. Called from cp_solve's loop — not beside it:
 * cp_solve does not exit while this is still moving, so escape is an element of the one
 * combined fixpoint, exactly as §9 requires. Escape only descends and pts only grows, both
 * over finite domains, so the combined loop still terminates. */
static bool cp_escape_update(cp_engine_t* eng) {
    if (eng->obj_count <= 0 || !eng->escape) return false;
    eng->clobbered_moved = false;      /* accumulates over the sweeps below */
    eng->inject_moved = false;         /* §42 inject / obj_bottom growth — also re-arms kills */
    bool any = false;
    int guard = 0;
    while (cp_escape_sweep(eng)) {      /* runs the sweep ≥ once, so §7.2 clobber marks are set */
        any = true;
        /* Each sweep that moves lowers at least one object one step in a 3-high lattice. */
        if (++guard > 3 * eng->obj_count + 8) {
            fprintf(stderr, "cp_escape: failed to converge — monotone-descent invariant "
                    "broken (obj_count=%d)\n", eng->obj_count);
            abort();
        }
    }
    /* A §7.2 clobber that appeared this round reaches the memory KILL through no def-use edge,
     * exactly like an escape descent — so report it, and cp_solve re-arms the kills. Monotone
     * (clobbered only grows, bounded by obj×cell), so the outer loop still terminates. The §42
     * injection matrix / obj_bottom reach the KILL the same non-def-use way — inject_moved. */
    return any || eng->clobbered_moved || eng->inject_moved;
}

/* §7.2 return-pts: does the call `call` resolve to a callee (or a whole virtual target set) whose
 * summary says it returns a FORMAL? Then the RESULT ALIASES that actual — its pts is the actual's
 * (plus null if a return could be null), not the opaque `Oret`. Virtual/interface: every target
 * must agree on the same formal, else conservative (false). A not-yet-summarized callee (bottom,
 * or a back-edge before convergence) has no summary ⟹ false ⟹ Oret, exactly as before. */
/* E1 (JLS §15.9.4) — is EVERY target of this call a FRESH-returning callee? Then its result is
 * NonNull (a `new` never returns null), so the Oret result drops ⊥null. Static/special: one callee.
 * Virtual/interface: ALL targets must be FRESH, else conservative (keep maybe-null). A bottom /
 * not-yet-summarized callee has no summary ⟹ false ⟹ maybe-null, exactly as before. */
static bool cp_invoke_ret_fresh(cp_engine_t* eng, const sir_node_t* call) {
    compiler_ctx_t* ctx = eng->ctx;
    if (!ctx) return false;
    switch (call->tag) {
    case SIR_INVOKESPECIAL: {
        const compiler_summary_t* s = compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_special.class_id, call->invoke_special.method_idx));
        return s && (s->ret_kind == COMPILER_RET_FRESH || s->ret_kind == COMPILER_RET_NONNULL);
    }
    case SIR_INVOKESTATIC: {
        const compiler_summary_t* s = compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_static.class_id, call->invoke_static.method_idx));
        return s && (s->ret_kind == COMPILER_RET_FRESH || s->ret_kind == COMPILER_RET_NONNULL);
    }
    case SIR_INVOKEVIRTUAL: case SIR_INVOKEINTERFACE: {
        bool iface = (call->tag == SIR_INVOKEINTERFACE);
        sir_node_t* recv = iface ? call->invoke_interface.obj : call->invoke_virtual.obj;
        int dc = iface ? call->invoke_interface.class_id  : call->invoke_virtual.class_id;
        int dm = iface ? call->invoke_interface.method_idx : call->invoke_virtual.method_idx;
        int cls[16], mid[16], n = 0;
        if (!cp_virtual_target_set(eng, cp_vnode_of(eng, recv), dc, dm, cls, mid, 16, &n)) return false;
        for (int t = 0; t < n; t++) {
            const compiler_summary_t* s = compiler_method_summary(ctx, compiler_method_index(ctx, cls[t], mid[t]));
            if (!s || (s->ret_kind != COMPILER_RET_FRESH
                       && s->ret_kind != COMPILER_RET_NONNULL)) return false;
        }
        return n > 0;
    }
    default: return false;
    }
}

/* §7.2's VALUE half, the consumer: rebuild a callee's exported return const. The
 * summary is FIXED during this method's solve, so the transfer is a constant function —
 * trivially monotone; across convergence passes cp_summary_differ re-runs the loop. */
static bool cp_summary_ret_const(const compiler_summary_t* s, cp_const_t* out) {
    if (!s || !s->computed || s->ret_cstate == COMPILER_RETC_UNKNOWN) return false;
    memset(out, 0, sizeof *out);
    out->cwidth = (cp_cwidth_t)s->ret_cwidth;
    if (s->ret_cstate == COMPILER_RETC_KNOWN) {
        out->state = CP_C_KNOWN;
        switch (out->cwidth) {
            case CP_W_I32: out->value  = (int32_t)s->ret_clo;
                           out->lvalue = s->ret_clo;               break;
            case CP_W_I64: out->lvalue = s->ret_clo;               break;
            case CP_W_F32: { uint32_t b = (uint32_t)s->ret_clo;
                             memcpy(&out->fvalue, &b, sizeof b);   break; }
            case CP_W_F64: { uint64_t b = (uint64_t)s->ret_clo;
                             memcpy(&out->dvalue, &b, sizeof b);   break; }
        }
    } else {
        out->state  = CP_C_RANGE;
        out->lo     = s->ret_clo;
        out->hi     = s->ret_chi;
        out->stride = 1;
    }
    return true;
}

/* The call's result value: static/special read the one summary; virtual/interface MEET
 * every defunctionalized target's export (KNOWN 5 ⊓ KNOWN 7 = the 2-point range — free
 * precision), and ANY bottom/unexporting target withdraws the claim (fail-closed). */
static bool cp_invoke_ret_const(cp_engine_t* eng, const sir_node_t* call, cp_const_t* out) {
    compiler_ctx_t* ctx = eng->ctx;
    if (!ctx) return false;
    switch (call->tag) {
    case SIR_INVOKESPECIAL:
        return cp_summary_ret_const(compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_special.class_id,
                                  call->invoke_special.method_idx)), out);
    case SIR_INVOKESTATIC:
        return cp_summary_ret_const(compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_static.class_id,
                                  call->invoke_static.method_idx)), out);
    case SIR_INVOKEVIRTUAL: case SIR_INVOKEINTERFACE: {
        bool iface = (call->tag == SIR_INVOKEINTERFACE);
        sir_node_t* recv = iface ? call->invoke_interface.obj : call->invoke_virtual.obj;
        int dc = iface ? call->invoke_interface.class_id   : call->invoke_virtual.class_id;
        int dm = iface ? call->invoke_interface.method_idx : call->invoke_virtual.method_idx;
        int cls[16], mid[16], n = 0;
        if (!cp_virtual_target_set(eng, cp_vnode_of(eng, recv), dc, dm, cls, mid, 16, &n))
            return false;
        cp_const_t acc = { .state = CP_C_TOP };
        for (int t = 0; t < n; t++) {
            cp_const_t c;
            if (!cp_summary_ret_const(compiler_method_summary(ctx,
                    compiler_method_index(ctx, cls[t], mid[t])), &c)) return false;
            acc = cp_const_meet(acc, c);
        }
        if (n == 0 || (acc.state != CP_C_KNOWN && acc.state != CP_C_RANGE)) return false;
        *out = acc;
        return true;
    }
    default: return false;
    }
}

static bool cp_invoke_return_alias(cp_engine_t* eng, const sir_node_t* call, cp_pts_t* out) {
    compiler_ctx_t* ctx = eng->ctx;
    if (!ctx) return false;
    sir_node_t* recv = NULL; sir_node_t** args = NULL; int nargs = 0;
    int rp = 0; bool rmn = false; bool ok = false;
    switch (call->tag) {
    case SIR_INVOKESPECIAL: {
        recv = call->invoke_special.obj; args = call->invoke_special.args;
        nargs = call->invoke_special.args_count;
        const compiler_summary_t* s = compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_special.class_id, call->invoke_special.method_idx));
        if (s && s->ret_kind == COMPILER_RET_FORMAL) { rp = s->ret_param; rmn = s->ret_maybe_null; ok = true; }
        break;
    }
    case SIR_INVOKESTATIC: {
        args = call->invoke_static.args; nargs = call->invoke_static.args_count;
        const compiler_summary_t* s = compiler_method_summary(ctx,
            compiler_method_index(ctx, call->invoke_static.class_id, call->invoke_static.method_idx));
        if (s && s->ret_kind == COMPILER_RET_FORMAL) { rp = s->ret_param; rmn = s->ret_maybe_null; ok = true; }
        break;
    }
    case SIR_INVOKEVIRTUAL: case SIR_INVOKEINTERFACE: {
        bool iface = (call->tag == SIR_INVOKEINTERFACE);
        recv  = iface ? call->invoke_interface.obj : call->invoke_virtual.obj;
        args  = iface ? call->invoke_interface.args : call->invoke_virtual.args;
        nargs = iface ? call->invoke_interface.args_count : call->invoke_virtual.args_count;
        int dc = iface ? call->invoke_interface.class_id  : call->invoke_virtual.class_id;
        int dm = iface ? call->invoke_interface.method_idx : call->invoke_virtual.method_idx;
        int cls[16], mid[16], n = 0;
        if (!cp_virtual_target_set(eng, cp_vnode_of(eng, recv), dc, dm, cls, mid, 16, &n)) return false;
        for (int t = 0; t < n; t++) {
            const compiler_summary_t* s = compiler_method_summary(ctx, compiler_method_index(ctx, cls[t], mid[t]));
            if (!s || s->ret_kind != COMPILER_RET_FORMAL) return false;   /* a target keeps Oret */
            if (t == 0) { rp = s->ret_param; rmn = s->ret_maybe_null; }
            else if (rp != s->ret_param) return false;                    /* targets disagree */
            else rmn = rmn || s->ret_maybe_null;
        }
        ok = (n > 0);
        break;
    }
    default: break;
    }
    if (!ok) return false;
    sir_node_t* actual = (rp == -1) ? recv : (rp >= 0 && rp < nargs ? args[rp] : NULL);
    if (!actual) return false;
    int av = cp_vnode_of(eng, actual);
    if (av < 0 || av >= eng->vnode_count) return false;
    *out = cp_pts_new(eng);
    cp_pts_union(eng, out, eng->vnodes[av]->pts);
    if (rmn) cp_pts_add(eng, out, CP_OBJ_NULL);
    return true;
}

/* The pts transfer function (spec §2), over the REAL SIR tags. */
static cp_pts_t cp_node_pts(cp_engine_t* eng, int vi) {
    cp_vnode_t* v = eng->vnodes[vi];
    cp_pts_t s = cp_pts_new(eng);

    /* An OPAQUE seed stands for an incoming value: a formal parameter, or a slot
     * whose def we cannot see. Either way it may name anything external, and it
     * may be null. Never ∅ — ∅ reads as "unreachable" and would license
     * optimization.
     *
     * Spec §1 gives such a reference its OWN phantom ("one per (site, type)"), and
     * the site is the SLOT: a seeded ref slot is a formal parameter, because JLS
     * §16 forbids reading a local before it is assigned. One phantom per parameter
     * is what stops two parameters from being forced to alias — which in turn is
     * what lets a store through one of them not clobber the other's fields. */
    if (v->kind == CP_VN_OPAQUE) {
        int slot = v->seed_slot;
        if (slot >= 0 && slot < eng->slot_count && eng->obj_of_slot[slot] >= 0)
            return cp_pts_phantom_ref(eng, eng->obj_of_slot[slot]);
        return cp_pts_unknown_ref(eng);
    }
    /* φ: join the contributors (spec §2 ASSIGN — plain copies don't exist). */
    if (v->kind == CP_VN_PHI || v->kind == CP_VN_REFINE) {
        for (int k = 0; k < v->input_count; k++) {
            int in = v->inputs[k];
            if (in >= 0 && in < eng->vnode_count)
                cp_pts_union(eng, &s, eng->vnodes[in]->pts);
        }
        /* Spec §2/§4: a refinement FILTERS pts along its arm. The value reached
         * this arm only by surviving the test, so on the NonNull arm it cannot be
         * the null object, and on the Null arm it can be nothing else. Nullability
         * is derived from pts (§4: `⊥null ∈ pts` ⟺ may-be-null), so this IS the
         * nullability refinement — there is no second lattice to update. */
        if (v->kind == CP_VN_REFINE) {
            if (v->refine_pts == CP_REFINE_PTS_NONNULL) {
                cp_pts_remove(eng, &s, CP_OBJ_NULL);
            } else if (v->refine_pts == CP_REFINE_PTS_NULL) {
                /* Keep only ⊥null: mask, in place. No second set to allocate. */
                s.bits[0] &= (uint64_t)1 << CP_OBJ_NULL;
                for (int w = 1; w < eng->obj_words; w++) s.bits[w] = 0;
            } else if (v->refine_pts == CP_REFINE_PTS_ISA
                    || v->refine_pts == CP_REFINE_PTS_NOT_ISA) {
                /* Spec §2's `br_on_cast`: "splits pts(u) along its two successor edges
                 * the same way". On the TRUE edge the value passed `instanceof τ`, so it
                 * is neither null (JLS §15.20.2) nor an object of a class that is not a
                 * τ. On the FALSE edge it failed, so it is not an object we can PROVE is
                 * a τ — null goes here, and so does every object whose class we do not
                 * know. Only a proven answer drops anything (cp_obj_isa's tri-state). */
                bool want = (v->refine_pts == CP_REFINE_PTS_ISA);
                for (int w = 0; w < eng->obj_words; w++) {
                    uint64_t word = s.bits[w];
                    while (word) {
                        int o = (w << 6) + __builtin_ctzll(word);
                        word &= word - 1;
                        int isa = cp_obj_isa(eng, o, v->refine_atype, v->refine_class);
                        if (isa < 0) continue;                 /* unknown: keep on both */
                        if (want ? (isa == 0) : (isa == 1))
                            cp_pts_remove(eng, &s, o);
                    }
                }
            }
        }
        return s;
    }
    if (v->kind != CP_VN_EXPR || !v->expr) return s;

    switch (v->expr->tag) {
        case SIR_NEW: case SIR_NEWARRAY: case SIR_NEWREFARRAY:
            cp_pts_add(eng, &s, eng->obj_of_vnode[vi]);   /* BASE */
            return s;
        case SIR_LOADNULL:
            cp_pts_add(eng, &s, CP_OBJ_NULL);
            return s;
        case SIR_LOADLOCAL:                                /* copy: follow the def */
            if (v->input_count > 0 && v->inputs[0] >= 0)
                cp_pts_union(eng, &s, eng->vnodes[v->inputs[0]]->pts);
            return s;
        /* Spec §2: `v ← ref.cast τ(u)` ⟹ `{ O ∈ pts(u) | classOf(O) ≤ τ }`. The cast
         * FILTERS: an object of a class that is not a τ cannot be here, because the
         * cast would have thrown (JLS §15.16). ⊥null is the exception — `(τ) null`
         * SUCCEEDS and yields null — so it passes, and the value stays Maybe-null. */
        case SIR_CHECKCAST: {
            if (v->input_count < 1 || v->inputs[0] < 0) return s;
            cp_pts_t in = eng->vnodes[v->inputs[0]]->pts;
            if (!in.bits) return s;
            for (int w = 0; w < eng->obj_words; w++) {
                uint64_t word = in.bits[w];
                while (word) {
                    int o = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (o == CP_OBJ_NULL                     /* null passes any cast */
                        || cp_obj_isa(eng, o, v->expr->check_cast.atype,
                                      v->expr->check_cast.class_id) != 0)
                        cp_pts_add(eng, &s, o);
                }
            }
            return s;
        }
        case SIR_CLONECOPY:
            if (v->input_count > 0 && v->inputs[0] >= 0)
                cp_pts_union(eng, &s, eng->vnodes[v->inputs[0]]->pts);
            return s;
        /* INDIRECT / LOAD (spec §2): read the cell's Obj↦pts map at the objects
         * the receiver may name. The memory input is the node's LAST input,
         * wired by cp_resolve_loads. */
        /* INDIRECT / LOAD (spec §2): pts(v) = ⋃_{O ∈ pts(q)} pts(O.f) — read the
         * cell's map at the objects the receiver may name. The memory input is
         * this node's LAST input, wired by cp_resolve_loads. */
        case SIR_GETFIELD: case SIR_ARRAYLOAD: {
            if (v->input_count < 2) return cp_pts_unknown_ref(eng);
            int recv = v->inputs[0];
            int mem  = v->inputs[v->input_count - 1];
            if (recv < 0 || mem < 0 || mem >= eng->vnode_count)
                return cp_pts_unknown_ref(eng);
            const cp_pts_t* h = eng->vnodes[mem]->heap;
            if (!h) return s;                        /* cell not computed yet: ⊥ */
            cp_pts_t recv_pts = eng->vnodes[recv]->pts;
            if (!recv_pts.bits) return s;
            /* Only the objects the receiver can actually name — walk the SET BITS
             * of the bitset, not every abstract object. A receiver names one or
             * two objects; obj_count is the whole method's allocation sites.
             *
             * ⊥null is NOT one of them: reading a field of null throws (JLS §15.11),
             * it does not produce a value. It is in the receiver's pts because the
             * receiver MAY be null — a nullability fact, not a source of values. The
             * store side is symmetric (it never writes the null row). */
            for (int w = 0; w < eng->obj_words; w++) {
                uint64_t word = recv_pts.bits[w];
                if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
                while (word) {
                    int o = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;                /* clear the low set bit */
                    cp_pts_union(eng, &s, h[o]);
                }
            }
            return s;
        }
        /* A CALL RESULT (spec §1's `Oret@callee`): the callee is a bottom method — we
         * have no summary for it — but it still returns some ONE object, and §1 names
         * that object by the callee. So this is not the catch-all: `a.foo()` and
         * `b.bar()` are different unknowns, and a store through one is not read back
         * through the other. Maybe-null, because a callee may return null. */
        case SIR_INVOKEVIRTUAL: case SIR_INVOKESPECIAL:
        case SIR_INVOKESTATIC:  case SIR_INVOKEINTERFACE: {
            int o = eng->obj_of_vnode ? eng->obj_of_vnode[vi] : -1;
            if (o < 0) return cp_pts_unknown_ref(eng);   /* not ref-returning, or a
                                                          * vnode minted after
                                                          * enumeration: fail closed */
            /* §7.2 return-pts: a FORMAL-returning callee aliases the result to that actual. */
            cp_pts_t alias;
            if (v->expr && cp_invoke_return_alias(eng, v->expr, &alias)) return alias;
            cp_pts_add(eng, &s, o);
            /* E1: a FRESH-returning callee's result is NonNull (JLS §15.9.4) — keep the Oret
             * identity (its site is unmintable here) but do NOT add ⊥null. */
            if (!(v->expr && cp_invoke_ret_fresh(eng, v->expr)))
                cp_pts_add(eng, &s, CP_OBJ_NULL);
            return s;
        }
        /* Spec §2: `v ← global.get(G)` ⟹ `pts(v) = pts(G)`. A static has had its own
         * memory cell all along, and the reaching version of that cell is already wired
         * into this node's last input — so the store that reaches this read is known,
         * and answering "some unknown object" was throwing it away. The seed keeps it
         * sound when no store reaches: an unwritten static holds whatever another method
         * left there (unknown, maybe null), and any CALL kills the cell. */
        case SIR_GETSTATIC: {
            int cell = cp_cell_of_expr(eng, v->expr);
            int row  = cp_static_row(eng, cell);
            int mem  = v->input_count > 0 ? v->inputs[v->input_count - 1] : -1;
            if (row < 0 || mem < 0 || mem >= eng->vnode_count)
                return cp_pts_unknown_ref(eng);          /* fail closed */
            const cp_pts_t* h = eng->vnodes[mem]->heap;
            if (!h) return s;                            /* cell not computed yet: ⊥ */
            cp_pts_union(eng, &s, h[row]);
            return s;
        }
        /* A Class object: still unnameable. Sound: may be any external object. */
        case SIR_CLASSCONSTRUCT:
            return cp_pts_unknown_ref(eng);   /* a returned/loaded ref may be null */
        /* `this` and a Class object are unknown to us but NEVER null (JLS: the
         * receiver of an instance method always exists). `this` gets its own §1
         * phantom, so it does not alias the parameters. */
        case SIR_LOADTHIS:
            cp_pts_add(eng, &s, eng->obj_this);
            return s;
        case SIR_LOADCLASS:
            cp_pts_add(eng, &s, CP_OBJ_EXT);
            return s;
        default:
            return s;                                      /* not a ref value: ∅ */
    }
}

/* Click §4.7.5 PROPAGATE (Figure 4.7) — per-partition fact
 * propagation. Drains the global cprop_worklist;
 * for each partition X drains X.cprop, recomputing each member's
 * type/constant facts and enqueueing users on their own partitions'
 * cprop when a fact changes. Same fixpoint outcome as a flat
 * per-vnode worklist (the old cp_compute_facts), but the per-
 * partition order is what §4.7.5 lines 16-22 hinge on for Follower
 * transitions to fire before SPLIT_BY on the same partition. The
 * Follower transitions themselves are still in cp_follower_sweep at
 * this point; integrating them into PROPAGATE proper is the next
 * step. */
static void cp_compute_facts(cp_engine_t* eng) {
    int vc = eng->vnode_count;
    if (vc == 0) return;
    /* Click §4.7.5 PROPAGATE drain. Init happens once via cp_init_facts
     * (called by cp_solve before the outer loop); subsequent calls
     * drain only the cprop entries that CAUSE_SPLITS / reachability /
     * Follower transitions added since the last call. The Follower-apply
     * pass is deferred to cp_apply_followers_pass (called AFTER
     * cp_split_by_facts) per Click §4.7.5 lines 14-22: SPLIT(X,fallen)
     * precedes apply so apply reads post-split partitions. */
    if (eng->fallen) bbq__vec_hdr(eng->fallen)->len = 0;
    int outer_guard = 0;
    while (bbq_vec_len(eng->cprop_worklist) > 0) {
        if (++outer_guard > 1000000) {
            /* The lattices are finite-height and every transfer is
             * monotone, so non-convergence is a broken invariant, not
             * an input property. Breaking out here would ship non-
             * monotone (unsound) facts to the rewrite — fail closed. */
            fprintf(stderr, "cp_propagate: outer loop failed to converge, "
                    "|cprop_wl|=%d — monotone-fixpoint invariant broken\n",
                    (int)bbq_vec_len(eng->cprop_worklist));
            abort();
        }
        int x_pid = eng->cprop_worklist[bbq_vec_len(eng->cprop_worklist) - 1];
        bbq__vec_hdr(eng->cprop_worklist)->len--;
        cp_partition_t* X = eng->partitions[x_pid];
        X->on_cprop_wl = false;
        int inner_guard = 0;
        while (X->cprop_head >= 0) {
            if (++inner_guard > 1000000) {
                fprintf(stderr, "cp_propagate: inner drain failed to converge, "
                        "x_pid=%d head=%d — monotone-fixpoint invariant "
                        "broken\n", x_pid, X->cprop_head);
                abort();
            }
            int xv = X->cprop_head;
            cp_cprop_dequeue(eng, xv);
            /* Click §4.7.5 line 6.1: revert check before recompute.
             * A Follower whose identity-condition no longer holds must
             * return to Leader so subsequent SPLIT_BY puts it in a
             * partition that matches its actual (post-revert) shape. */
            if (eng->vnodes[xv]->leader >= 0) {
                if (cp_revert_identity_follower(eng, xv)
                        || cp_revert_phi_follower(eng, xv)
                        || cp_revert_load_follower(eng, xv))
                    bbq_vec_push(eng->fallen, xv);
            }
            const Type* t = cp_node_type(eng, eng->vnodes[xv]);
            cp_const_t  c = cp_node_const(eng, eng->vnodes[xv]);
            bool changed = t != eng->vnodes[xv]->type
                        || !cp_const_eq(c, eng->vnodes[xv]->constant);
            /* Lattice A rides the SAME worklist — but a pts change must reach
             * only the node's USERS, never `fallen`. `fallen` drives
             * cp_split_by_facts and the Follower-apply pass, i.e. CONGRUENCE;
             * pts is a derived property, not value identity. Keeping it
             * out of `fallen` is what makes "adding pts cannot move a
             * partition" true by construction rather than by luck. */
            cp_pts_t np = cp_node_pts(eng, xv);
            bool pts_changed = !cp_pts_eq(eng, np, eng->vnodes[xv]->pts);
            if (pts_changed) eng->vnodes[xv]->pts = np;
            bool heap_changed = cp_update_heap(eng, xv);
            /* Click §4.4.1 re-arm. CAUSE_SPLITS deliberately skips
             * users still at TOP (optimism — an unreached node must
             * not cause splits), so the skipped touch has to be
             * RETRIED once this node leaves TOP: re-enqueue every
             * input's partition on the split worklist. Without this,
             * two φs distinguished only by inputs whose touches
             * landed while the φs were TOP stay merged forever (the
             * initProperties i ≡ count loop-counter merge). Fires at
             * most once per vnode — types only descend from TOP. */
            bool left_top = eng->vnodes[xv]->type == type_top(&eng->pool)
                         && t != type_top(&eng->pool);
            eng->vnodes[xv]->type     = t;
            eng->vnodes[xv]->constant = c;
            if (left_top) {
                for (int k = 0; k < eng->vnodes[xv]->input_count; k++) {
                    int in = eng->vnodes[xv]->inputs[k];
                    if (in >= 0 && in < eng->vnode_count)
                        cp_wl_push(eng, eng->vnodes[in]->partition);
                }
            }
            /* A store has NO inputs, so no def-use edge would ever bring it back
             * when what it depends on changes. The reverse index (built in
             * cp_resolve) is what re-arms it — the same class of bug as the
             * SPLIT_BY_FACTS worklist re-arm: a fact that reaches a node by a
             * path the def-use graph doesn't model must be re-armed explicitly,
             * or the fixpoint converges to a stale answer.
             *
             * BOTH kinds of change must fire it: an operand's PTS (receiver,
             * stored value) and the reaching memory version's HEAP (mem_prev). */
            if ((pts_changed || heap_changed) && xv < eng->mem_rows) {
                for (int k = eng->mem_dep_off[xv];
                     k < eng->mem_dep_off[xv] + eng->mem_dep_cnt[xv]; k++)
                    cp_cprop_enqueue(eng, eng->mem_dep_list[k]);
            }
            if (!changed && !pts_changed && !heap_changed) continue;
            if (changed) bbq_vec_push(eng->fallen, xv);
            /* A FOLLOWER's stored constant arrives through the leader LINK, not a def-use
             * edge — the same off-graph class as the store re-arm above (its 6th instance).
             * When the leader's constant changes, every follower must recompute (the
             * transfer reads through the leader; the STORED field is what the rewrite
             * reads) — or a follower keeps a stale optimistic constant past the fixpoint:
             * the arraylen follower of a count φ kept KNOWN 0 after the φ fell to BOTTOM,
             * folding `.length` to 0 and the §15 IDX guard to ALWAYS-THROW. */
            if (changed && xv < eng->follower_head_cap) {
                for (int f = eng->follower_head[xv]; f >= 0;
                     f = eng->vnodes[f]->follower_next)
                    cp_cprop_enqueue(eng, f);
            }
            for (int k = eng->du_off[xv]; k < eng->du_off[xv] + eng->du_cnt[xv]; k++) {
                int u = eng->du_user[k];
                cp_cprop_enqueue(eng, u);
            }
        }
    }
}

/* Click §4.7.5 lines 16-21: post-split Follower-apply pass. Runs after
 * cp_split_by_facts so apply reads post-split partitions — applying
 * before split causes incorrect Follower transitions when an input has
 * just fallen but hasn't yet been split out of its prior partition. */
static void cp_apply_followers_pass(cp_engine_t* eng) {
    /* The memory identities key off POINTS-TO, and a pts change does not put a
     * node on `fallen` (only a fallen type/constant does), so they are swept
     * rather than driven off that list. Each fires at most once per node — a
     * Follower is skipped — so the sweep costs one scan per solve iteration. */
    for (int v = 0; v < eng->vnode_count; v++) {
        if (eng->vnodes[v]->leader >= 0) continue;
        if (cp_apply_load_follower(eng, v)) continue;
        if (cp_apply_same_input_follower(eng, v)) continue;
        cp_apply_arraylen_follower(eng, v);
    }
    if (!eng->fallen) return;
    int n = (int)bbq_vec_len(eng->fallen);
    for (int i = 0; i < n; i++) {
        int yv = eng->fallen[i];
        if (yv < 0 || yv >= eng->vnode_count) continue;
        if (eng->vnodes[yv]->leader >= 0) continue;
        if (cp_apply_identity_follower(eng, yv)) continue;
        cp_apply_phi_follower(eng, yv);
    }
    bbq__vec_hdr(eng->fallen)->len = 0;
}

/* ── §4.8 algebraic, 1-constant identity (Follower) ─────────── */

/* Click §4.8 algebraic 1-constant identity. The row's identity_side
 * declares which side(s) the constant can appear on (EITHER for
 * commutative ops; RIGHT only for the non-commutative ones — SUB,
 * DIV, SHL, SHR, USHR); identity_k is the constant that makes the
 * op an identity on the other operand. */
static bool cp_is_identity_const(int op, int side, int64_t k) {
    if (op < 0 || op >= SIR_TAG_COUNT) return false;
    const sir_op_gamma_t* g = &sir_op_gamma[op];
    if (g->identity_side == GS_NONE) return false;
    if (k != (int64_t)g->identity_k) return false;
    switch (g->identity_side) {
        case GS_EITHER: return true;
        case GS_LEFT:   return side == 0;
        case GS_RIGHT:  return side == 1;
        case GS_NONE:   /* handled above */
        default:        return false;
    }
}

/* §4.8 step: if value-node `v` is `Op(x, k)` with `k` matching the
 * identity element for `Op`, transition v from Leader to Follower of
 * `x`'s ultimate Leader. Returns true if a transition occurred. */
static bool cp_apply_identity_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader >= 0) return false;             /* already a Follower */
    if (v->kind != CP_VN_EXPR || v->input_count != 2) return false;
    for (int side = 0; side < 2; side++) {
        int const_idx  = v->inputs[side];
        int leader_idx = v->inputs[1 - side];
        if (const_idx < 0 || leader_idx < 0) continue;
        cp_const_t cc = eng->vnodes[const_idx]->constant;
        if (cc.state != CP_C_KNOWN) continue;
        /* Integer 1-constant identities (x+0, x*1, x&-1, x<<0, …) are exact at
         * both i32 and i64. Floats are excluded: x+0.0 is not the identity for
         * x = -0.0 (IEEE), so folding it would be wrong. Read the carrier that
         * matches the width — the i32 `value` field is unset (0) for wides. */
        int64_t kval;
        if (cc.cwidth == CP_W_I32)      kval = cc.value;
        else if (cc.cwidth == CP_W_I64) kval = cc.lvalue;
        else continue;
        if (!cp_is_identity_const(v->op, side, kval)) continue;
        /* Don't path-compress: per Click §4.10 the representative pick
         * happens at rewrite time. Walking past an intermediate Follower
         * (e.g., §4.7 COPY LOADLOCAL) loses the only emittable-leaf
         * candidate when the ultimate Leader is an OPAQUE seed. */
        if (leader_idx == v_idx) return false;     /* self-loop guard */
        /* Transition: Leader → Follower of leader_idx. Refresh the
         * stored facts so cp_split_by_facts (which runs in the same
         * iteration) sees the Follower's facts matching its Leader's
         * — otherwise the stale Leader-era facts would split the new
         * Leader back out of the partition. */
        bool pend = cp_part_remove(eng, v_idx);    /* v.leader still -1 */
        v->leader = leader_idx;                    /* now a Follower */
        int new_part = eng->vnodes[leader_idx]->partition;
        cp_part_add(eng, new_part, v_idx);
        if (pend) cp_cprop_enqueue(eng, v_idx);
        cp_follower_link(eng, v_idx, leader_idx);
        v->type     = eng->vnodes[leader_idx]->type;
        v->constant = eng->vnodes[leader_idx]->constant;
        /* Click §4.8 line 16.5: add new_part to CAUSE_SPLITS worklist
         * so its new Follower's def-use classification gets propagated. */
        cp_wl_push(eng, new_part);
        /* Pre-existing Followers of v (e.g. §4.7 COPY LOADLOCAL Followers
         * anchored before v itself became a Follower) stay chained to v
         * — just shift them into the new partition so the partition-
         * membership invariant holds. NO path compression: if v later
         * reverts (cp_revert_*_follower), its dependents must re-evaluate
         * against v's now-BOTTOM constant via the existing chain. Re-
         * anchoring here would orphan them past the revert. */
        cp_move_followers(eng, v_idx, new_part);
        return true;
    }
    return false;
}

/* The node that IS this value — the walk an Identity rule must do before it can
 * say "v is that". Three kinds of node compute nothing of their own:
 *   - a LoadLocal copy (cp_ultimate_value),
 *   - a Follower (§4.7: it is its Leader's value, by construction),
 *   - a Refine (PoPA Ch.6: its input's value, described more narrowly on one arm
 *     — it has its own partition only so it can hold that narrower fact).
 * Used to pick the LEADER an identity attaches to, so GVN itself ends up knowing
 * the identity; consumers never do this walk, they just compare partitions. */
static int cp_value_leader(cp_engine_t* eng, int vi) {
    for (int hops = 0; hops < 256; hops++) {
        int next = cp_ultimate_value(eng, vi);
        if (next < 0 || next >= eng->vnode_count) return next;
        cp_vnode_t* v = eng->vnodes[next];
        if (v->leader >= 0)                                   next = v->leader;
        else if (v->kind == CP_VN_REFINE && v->input_count >= 1
                 && v->inputs[0] >= 0)                        next = v->inputs[0];
        if (next == vi) return vi;
        vi = next;
    }
    return vi;
}

/* Make v a Follower of `leader_idx` — the transition cp_apply_identity_follower
 * performs, factored out so the memory identities below share it. */
static bool cp_become_follower(cp_engine_t* eng, int v_idx, int leader_idx) {
    if (leader_idx < 0 || leader_idx >= eng->vnode_count || leader_idx == v_idx)
        return false;
    cp_vnode_t* v = eng->vnodes[v_idx];
    bool pend = cp_part_remove(eng, v_idx);
    v->leader = leader_idx;
    int new_part = eng->vnodes[leader_idx]->partition;
    cp_part_add(eng, new_part, v_idx);
    if (pend) cp_cprop_enqueue(eng, v_idx);
    cp_follower_link(eng, v_idx, leader_idx);
    v->type     = eng->vnodes[leader_idx]->type;
    v->constant = eng->vnodes[leader_idx]->constant;
    cp_wl_push(eng, new_part);                 /* §4.8 line 16.5 */
    cp_move_followers(eng, v_idx, new_part);
    return true;
}

/* ── Memory identities (spec §1 / §8) ────────────────────────
 *
 * §1: "a store reaches a load **iff** they touch the same `O.f` and no killing
 * store intervenes on the value path — a **sparse** query over the value graph,
 * *not* a dominance query." The memory-SSA overlay already answers it: the
 * load's memory input IS its reaching store. So —
 *
 * LOAD-AFTER-STORE. A load whose reaching store wrote the SAME single object's
 * SAME cell reads exactly what was stored: the load *is* the stored value. Being
 * the reaching def is what rules out an intervening kill (any other store to the
 * cell would itself be the reaching def), and the singleton receiver on both
 * sides is what rules out a different object. So it becomes a Follower of the
 * stored value, and GVN sees one value where the graph had two nodes — §8's "a
 * value IS a node; using it IS an edge". */
/* The one abstract object a pts names (ignoring ⊥null and the catch-all Oext), or -1 if it
 * names zero or more than one. Gate 5's preserve check needs THE object whose cell a call might
 * write; anything else (Oext, a two-object pts) is not a determinable single receiver. */
static int cp_pts_sole_obj(const cp_engine_t* eng, cp_pts_t s) {
    if (!s.bits) return -1;
    int found = -1;
    for (int o = 0; o < eng->obj_count; o++) {
        if (o == CP_OBJ_NULL || o == CP_OBJ_EXT) continue;
        if (cp_pts_has(eng, s, o)) { if (found >= 0) return -1; found = o; }
    }
    return found;
}

/* Gate 5 (VFG ISMM'13 §4.1) — does a call PROVABLY not write cell `cell` of object `o`? Then the
 * value a store left in `o.cell` survives the call untouched, so a load after the call reaches the
 * store before it. Proof obligation (Choi §7.2 + Fig 7): `clobbered` is a SUPERSET of the cells the
 * call writes on `o` (instantiated from the callee's transitive wcell via MapsTo), and
 * cp_obj_survives_call excludes every object a bottom method could reach without being handed it —
 * so `¬clobbered ∧ survives` SOUNDLY implies the call leaves `o.cell` untouched. */
static bool cp_call_preserves_cell(const cp_engine_t* eng, int o, int cell) {
    if (o < 0 || o >= eng->obj_count) return false;
    if (cell >= 0 && cell < eng->mem_cell_count && eng->clobbered
            && eng->clobbered[(size_t)o * eng->mem_cell_count + cell]) return false;
    return cp_obj_survives_call(eng, o);
}

/* The value a GetField forwards to — its reaching store's value — walking the reaching version
 * THROUGH any call that provably preserves the receiver's cell (Gate 5, VFG §4.1). Returns -1 if
 * it does not forward. ONE authority for both apply and revert, so they cannot disagree about the
 * kill-walk condition (Click §4.7.5: a follower and its revert must test the same predicate). */
static int cp_load_forward_target(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->kind != CP_VN_EXPR || !v->expr || v->input_count < 2) return -1;
    /* An ArrayLoad's cell is monolithic (spec §1), so a store to a[j] says nothing about a[i] —
     * only the field case forwards. */
    if (v->expr->tag != SIR_GETFIELD) return -1;
    int recv = v->inputs[0];
    int mem  = v->inputs[v->input_count - 1];
    if (recv < 0 || mem < 0 || mem >= eng->mem_rows) return -1;
    /* THE object the load reads — needed to ask whether a call preserved its cell. If the receiver
     * names more than one object (or Oext), we cannot ask, so we do not forward across a call. */
    int robj = cp_pts_sole_obj(eng, eng->vnodes[recv]->pts);
    while (mem >= 0 && mem < eng->mem_rows && eng->mem_kind[mem] == CP_MEM_KILL) {
        if (robj < 0) return -1;
        if (!cp_call_preserves_cell(eng, robj, eng->mem_cell[mem])) return -1;
        mem = eng->mem_prev[mem];           /* the version the call interrupted */
    }
    if (mem < 0 || mem >= eng->mem_rows || eng->mem_kind[mem] != CP_MEM_STORE) return -1;
    int sobj = eng->mem_obj[mem], sval = eng->mem_val[mem];
    if (sobj < 0 || sval < 0) return -1;
    /* Value identity, not merely equal pts: Obj naming is 1-limited (one per SITE), so equal
     * singleton pts does not mean the same object (the `new int[2][2][2]` trap). GVN answers "same
     * value": one partition. */
    if (eng->vnodes[recv]->partition != eng->vnodes[sobj]->partition) return -1;
    return sval;
}

static bool cp_apply_load_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader >= 0 || v->kind != CP_VN_EXPR || !v->expr) return false;
    if (v->expr->tag != SIR_GETFIELD) return false;
    int sval = cp_load_forward_target(eng, v_idx);
    if (sval < 0) return false;
    return cp_become_follower(eng, v_idx, sval);
}

/* `(new T[n]).length` IS `n` — §15.10.1 gives the array exactly the evaluated
 * dimension and §10.7 makes the length final. Click's ArrayLengthNode::Identity.
 * The test is on the VALUE, not on points-to: the operand must BE the allocation
 * (through copies, which cp_ultimate_value follows — but never through a φ, so
 * an array carried across a loop back-edge, whose site may have run with a
 * different size, is correctly not identified). */
static bool cp_apply_arraylen_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader >= 0 || v->kind != CP_VN_EXPR || !v->expr) return false;
    if (v->expr->tag != SIR_ARRAYLENGTH || v->input_count < 1) return false;
    int arr = cp_value_leader(eng, v->inputs[0]);   /* the forwarded load lands here */
    if (arr < 0 || arr >= eng->vnode_count) return false;
    sir_node_t* a = eng->vnodes[arr]->expr;
    if (!a) return false;
    sir_node_t* size = a->tag == SIR_NEWARRAY    ? a->new_array.size
                     : a->tag == SIR_NEWREFARRAY ? a->new_ref_array.size
                     : NULL;
    if (!size) return false;
    return cp_become_follower(eng, v_idx, cp_value_leader(eng, cp_vnode_of(eng, size)));
}

/* §4.8's same-input IDEMPOTENT identity (x & x = x, x | x = x), declared by
 * the row's same_in_follower. Fires when both inputs carry ONE reaching name
 * (cp_ultimate_value identity — stable pass-A wiring, so the transition fires
 * at most once and never needs a revert). Identity ONLY, never partitions: a
 * follower link riding a transient coarse partition is exactly the class the
 * cong_fold re-arm walks back, and follower links don't get walked back.
 * Follows the INPUT, not the ultimate — §4.10's representative pick walks
 * the chain at rewrite time (the identity follower's rule). */
static bool cp_apply_same_input_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader >= 0 || v->kind != CP_VN_EXPR || !v->expr) return false;
    if (!sir_op_gamma[v->expr->tag].same_in_follower) return false;
    if (v->input_count != 2) return false;
    int li = v->inputs[0], ri = v->inputs[1];
    if (li < 0 || ri < 0) return false;
    int lu = cp_ultimate_value(eng, li);
    if (lu < 0 || lu != cp_ultimate_value(eng, ri)) return false;
    return cp_become_follower(eng, v_idx, li);
}

/* §4.7.5 step 6.1: a Follower whose identity condition no longer
 * holds must return to Leader. cp_apply_identity_follower fires optimistically
 * during the iterative fixpoint — e.g. a loop body's Add(sum, i)
 * sees `sum = KNOWN 0` transiently (before the back-edge contributor
 * is processed) and becomes Follower of `i`. When the φ for `sum`
 * later drops to BOTTOM the identity no longer holds; without the
 * reverse transition the Follower locks in incorrectly. */
/* Click §4.7.5 step 6.3: "Move x from X.Follower to X.Leader." The
 * reverted node STAYS in the same partition X — it just moves from the
 * Follower set to the Leader set. SPLIT_BY (driven by the fallen set
 * cp_compute_facts builds from these reverts) handles the eventual
 * split naturally because x's facts now differ from X's other Leaders.
 * Creating a fresh singleton here would destroy peer-congruence: two
 * positionally-congruent Followers reverting independently would end
 * up in separate singletons that refinement can never merge. */
static bool cp_revert_identity_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader == -1) return false;
    if (v->kind != CP_VN_EXPR || v->input_count != 2) return false;
    for (int side = 0; side < 2; side++) {
        int const_idx = v->inputs[side];
        if (const_idx < 0 || const_idx >= eng->vnode_count) continue;
        cp_const_t cc = eng->vnodes[const_idx]->constant;
        if (cc.state == CP_C_KNOWN
                && (cc.cwidth == CP_W_I32 || cc.cwidth == CP_W_I64)) {
            int64_t kval = (cc.cwidth == CP_W_I64) ? cc.lvalue : (int64_t)cc.value;
            if (cp_is_identity_const(v->op, side, kval))
                return false;   /* identity still active — Follower stays */
        }
    }
    /* Click §4.7.5 step 6.3: keep v in its current partition; just
     * flip Leader status. SPLIT_BY downstream catches the divergence
     * via the fallen-set machinery. */
    int old_leader = v->leader;
    cp_follower_unlink(eng, v_idx, old_leader);
    v->leader = -1;
    /* v rejoins the Leader set in-place — restore the Leader count the
     * matching cp_apply_identity_follower decremented (via cp_part_remove).
     * Without this, |Z.Leader| reads one short and CAUSE_SPLITS'
     * `touched != leader_count` test never fires, leaving non-congruent
     * nodes merged (under-refinement). */
    eng->partitions[v->partition]->leader_count++;
    cp_wl_push(eng, v->partition);
    return true;
}

/* Gate 5 (Click §4.7.5 step 6.3) — a load-follower that forwarded ACROSS a call goes stale when
 * the call's preserve condition descends (escape falls, or MapsTo grows `clobbered`): the kill-walk
 * no longer reaches the store, so the load is no longer the stored value and must return to Leader.
 * This is what keeps the optimistic apply SOUND (VFG §4.1 holds only at the fixpoint, where
 * `preserve` has finished descending). Re-armed for free: the load's memory input IS the call's kill
 * vnode (an input, hence a def-use edge), so when the kill re-arms on escape movement (cp_solve) and
 * its heap changes, the load — a user — is recomputed and this check fires. Restricted to a follower
 * whose memory input is a KILL: the direct-store follower's guard is structural and never un-holds,
 * so it keeps its no-revert invariant untouched. */
static bool cp_revert_load_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader < 0 || v->kind != CP_VN_EXPR || !v->expr) return false;
    if (v->expr->tag != SIR_GETFIELD || v->input_count < 2) return false;
    int mem = v->inputs[v->input_count - 1];
    if (mem < 0 || mem >= eng->mem_rows || eng->mem_kind[mem] != CP_MEM_KILL) return false;
    if (cp_load_forward_target(eng, v_idx) == v->leader) return false;   /* still valid — stays */
    int old_leader = v->leader;
    cp_follower_unlink(eng, v_idx, old_leader);
    v->leader = -1;
    eng->partitions[v->partition]->leader_count++;               /* rejoin the Leader set in-place */
    cp_wl_push(eng, v->partition);
    return true;
}

/* Click §4.9 PHI Follower transition: a PHI whose all-live inputs are
 * in one partition is a Follower of that partition (PhiNode::Identity,
 * thesis §4.9). The sweep runs after PROPAGATE + SPLIT_BY + cp_refine
 * so all live inputs already sit in their converged partitions. */
static bool cp_apply_phi_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader >= 0) return false;
    if (v->kind != CP_VN_PHI) return false;
    if (!v->type || v->type == type_top(&eng->pool)) return false;
    int leader_idx = -1;
    int leader_part = -1;
    for (int i = 0; i < v->input_count; i++) {
        if (!cp_phi_input_live(eng, v, i)) continue;
        int in = v->inputs[i];
        if (in < 0 || in >= eng->vnode_count) return false;
        cp_vnode_t* iv = eng->vnodes[in];
        if (!iv->type || iv->type == type_top(&eng->pool)) return false;
        int p = iv->partition;
        if (leader_idx < 0) { leader_idx = in; leader_part = p; }
        else if (p != leader_part) return false;
    }
    if (leader_idx < 0) return false;              /* all inputs dead */
    /* Don't path-compress (same rationale as cp_apply_identity_follower):
     * representative selection runs at rewrite time so an intermediate
     * §4.7 COPY LOADLOCAL Follower stays reachable as an emittable
     * leaf. */
    if (leader_idx == v_idx) return false;
    bool pend2 = cp_part_remove(eng, v_idx);
    v->leader = leader_idx;
    int new_part = eng->vnodes[leader_idx]->partition;
    cp_part_add(eng, new_part, v_idx);
    if (pend2) cp_cprop_enqueue(eng, v_idx);
    cp_follower_link(eng, v_idx, leader_idx);
    v->type     = eng->vnodes[leader_idx]->type;
    v->constant = eng->vnodes[leader_idx]->constant;
    /* Click §4.8 line 16.5: add new_part to CAUSE_SPLITS worklist. */
    cp_wl_push(eng, new_part);
    /* Pre-existing Followers-of-v stay chained to v (no path compression)
     * — only their partition shifts so the partition-membership invariant
     * holds after v moved. Keeping the chain intact is what lets a later
     * cp_revert_phi_follower on v auto-propagate to its dependents: they
     * still point at v through .leader, so once v's facts fall (cp_node_const
     * forwards through v->leader = -1 case to v's PHI inputs), their
     * constants follow. */
    cp_move_followers(eng, v_idx, new_part);
    return true;
}

/* §4.7.5 step 6.1 for §4.9: when a contributor of a Follower PHI
 * splits out so the all-same-partition invariant no longer holds,
 * revert the PHI to a Leader so cp_refine can re-bucket it correctly. */
static bool cp_revert_phi_follower(cp_engine_t* eng, int v_idx) {
    cp_vnode_t* v = eng->vnodes[v_idx];
    if (v->leader == -1) return false;
    if (v->kind != CP_VN_PHI) return false;
    int leader_part = -1;
    for (int i = 0; i < v->input_count; i++) {
        if (!cp_phi_input_live(eng, v, i)) continue;
        int in = v->inputs[i];
        if (in < 0 || in >= eng->vnode_count) continue;
        int p = eng->vnodes[in]->partition;
        if (leader_part < 0) leader_part = p;
        else if (p != leader_part) {
            /* Click §4.7.5 step 6.3: keep v in current partition; just
             * flip from Follower to Leader. SPLIT_BY handles eventual
             * divergence — preserving partition equality is critical
             * for peer-PHI congruence (two PHIs reverting independently
             * must stay in the SAME partition until their inputs prove
             * them distinct). */
            int old_leader = v->leader;
            cp_follower_unlink(eng, v_idx, old_leader);
            v->leader = -1;
            /* Restore the Leader count cp_apply_phi_follower decremented;
             * see cp_revert_identity_follower for the under-refinement
             * this prevents. */
            eng->partitions[v->partition]->leader_count++;
            cp_wl_push(eng, v->partition);
            /* Re-arm the INPUT partitions too: a reverted φ's divergence
             * is positional (its inputs' partitions differ), which
             * cp_split_by_facts cannot see — two loop counters both
             * typed prim(int)/BOTTOM carry identical FACTS while
             * computing different VALUES (initProperties' i vs count).
             * Only re-running positional CAUSE_SPLITS over the inputs'
             * partitions separates the peer φs. */
            for (int k = 0; k < v->input_count; k++) {
                int rin = v->inputs[k];
                if (rin >= 0 && rin < eng->vnode_count)
                    cp_wl_push(eng, eng->vnodes[rin]->partition);
            }
            return true;
        }
    }
    return false;
}


/* ── Reachability (UCE) ──────────────────────────────────────── */

/* Channel (a) consumer: the verdict for the branch at spine index `i` with
 * condition `cond`, or -1 when no path-invariant fact decides it. Matching is
 * by cp_value_leader IDENTITY only — the engine's designed value walk (copies,
 * Followers, Refines) — never by partition membership and never through
 * vnode->constant (see the header's verdict_words comment for both whys).
 * Fail-closed at every step: a spliced spine node (index ≥ verdict_rows), a
 * condition with no vnode (cp_rewrite may have minted it), or an empty row all
 * answer "no verdict". Sound mid-solve: cp_value_leader reads CURRENT follower
 * links, which may transiently claim an identity that later reverts — the same
 * optimistic dynamics as a KNOWN falling to BOTTOM, and reachability (the only
 * in-solve caller) is recomputed from scratch every round. */
static int cp_branch_verdict(cp_engine_t* eng, int i, sir_node_t* cond) {
    if (!eng->verdict_words || i < 0 || i >= eng->verdict_rows || !cond)
        return -1;
    void* f = cp_pmap_get(&eng->expr_idx, cond);
    if (!f) return -1;
    int lv = cp_value_leader(eng, (int)((uintptr_t)f - 1));
    const uint64_t* row = eng->verdict_words + (size_t)i * eng->verdict_stride;
    for (int w = 0; w < eng->verdict_stride; w++) {
        uint64_t bits = row[w];
        while (bits) {
            int fid = w * 64 + __builtin_ctzll(bits);
            bits &= bits - 1;
            int fb  = eng->fact_branch[fid >> 1];
            int cvn = eng->branch_cond_vn[fb];
            if (cvn >= 0 && cp_value_leader(eng, cvn) == lv)
                return fid & 1;
        }
    }
    return -1;
}

/* The constant a branch condition currently carries.
 *
 * A condition the engine has NO VNODE FOR is BOTTOM (unknown), never TOP.
 *
 * TOP is the optimistic "not yet decided" of the solve, and the caller answers it by
 * marking NEITHER arm reachable (Click §4.4.1). That is right while the fixpoint runs —
 * every condition then has a vnode, and TOP genuinely means "no value has arrived yet".
 * It is CATASTROPHIC afterwards: cp_rewrite REPLACES expression children with fresh
 * nodes (a folded constant, a CSE substitution), and a fresh node is not in expr_idx.
 * Reading that as TOP made the post-rewrite reachability pass declare BOTH arms of a
 * perfectly live branch unreachable — so liveness never visited the code below it, its
 * LoadLocals were never counted as uses, DSE deleted the StoreLocals that fed them, and
 * codegen (which walks from entry and has never heard of reachable[]) emitted loads of
 * slots with no definition. A ref local then reached the assembler with no threaded
 * descriptor (fail-loud, module rejected); an int one just returned garbage (silent).
 *
 * "I have never seen this node" is not "this node has no value". It is "I know nothing
 * about it" — BOTTOM — and the arms stay live. Fail-closed. */
static cp_const_t cp_cond_const(cp_engine_t* eng, sir_node_t* cond) {
    cp_const_t unknown = { .state = CP_C_BOTTOM };
    if (!cond) return unknown;
    void* f = cp_pmap_get(&eng->expr_idx, cond);
    if (!f) return unknown;
    return eng->vnodes[(int)((uintptr_t)f - 1)]->constant;
}

/* Mark the spine nodes reachable from the entry, pruning the dead
 * arm of any Branch / Switch whose condition is a known constant
 * (§4.3 UCE). Click §3.7 combined CCP+UCE optimistic reading:
 *   - cond TOP  → not yet decided; propagate to NEITHER arm. Holding
 *                 the merge unreachable keeps its PHI's input set
 *                 empty (fact stays at TOP), so the PHI's fact can
 *                 fall monotonically as reachability narrows.
 *   - cond KNOWN k → only the matching arm reachable.
 *   - cond BOTTOM → both arms reachable.
 * Click §4.4.1: "branches are not evaluated until the test value
 * drops below ⊤." Propagating eagerly on TOP poisons PHI meets with
 * inputs that may never materialize, then the BOTTOM can't be
 * walked back when reachability later narrows. */
static void cp_compute_reachability(cp_engine_t* eng) {
    int sn = eng->spine_count;
    if (sn == 0) { eng->reach_count = 0; return; }
    if (!eng->reachable || eng->reachable_rows < sn) {
        eng->reachable = (bool*)bbq_arena_alloc(eng->arena,
                                                (size_t)sn * sizeof(bool));
        eng->reachable_rows = sn;
    }
    for (int i = 0; i < sn; i++) eng->reachable[i] = false;
    int* wl = NULL;
    int entry = cp_spine_index(eng, eng->method->entry);
    if (entry >= 0) { eng->reachable[entry] = true; bbq_vec_push(wl, entry); }
    while (bbq_vec_len(wl)) {
        int ni = wl[bbq_vec_len(wl) - 1];
        bbq__vec_hdr(wl)->len--;
        sir_node_t* n = eng->spine[ni];
        int sc = sir_succ_count(n);
        for (int i = 0; i < sc; i++) {
            int si = cp_spine_index(eng, sir_succ(n, i));
            if (si < 0) continue;
            /* Branch (§3.7 optimistic): TOP cond → propagate neither
             * arm; KNOWN cond → only the selected arm; BOTTOM cond →
             * both arms. */
            if (n->tag == SIR_BRANCH) {
                cp_const_t c = cp_cond_const(eng, n->branch.cond);
                if (c.state == CP_C_TOP) continue;
                if (c.state == CP_C_KNOWN
                        && ((i == 0) != (c.value != 0))) continue;
                /* Channel (a): a condition the fold can't decide may still be
                 * decided by a path-invariant verdict. KNOWN wins when present
                 * (handled above); the verdict fills in for BOTTOM/RANGE. */
                if (c.state != CP_C_KNOWN) {
                    int vd = cp_branch_verdict(eng, ni, n->branch.cond);
                    if (vd >= 0 && ((i == 0) != (vd != 0))) continue;
                }
            }
            /* Switch (§3.7 optimistic): TOP selector → no arms;
             * KNOWN → only matching case (or default if no match);
             * BOTTOM → all arms. */
            if (n->tag == SIR_SWITCH) {
                cp_const_t c = cp_cond_const(eng, n->switch_.selector);
                if (c.state == CP_C_TOP) continue;
                if (c.state == CP_C_KNOWN) {
                    int nc = n->switch_.case_targets_count, live = nc;
                    for (int j = 0; j < nc; j++)
                        if (n->switch_.case_values[j] == c.value) {
                            live = j;
                            break;
                        }
                    if (i != live) continue;
                }
            }
            if (!eng->reachable[si]) {
                eng->reachable[si] = true;
                bbq_vec_push(wl, si);
            }
        }
    }
    bbq_vec_free(wl);
    int rc = 0;
    for (int i = 0; i < sn; i++) if (eng->reachable[i]) rc++;
    /* Click §3.7 combined CCP+UCE: when reachability changes, every
     * PHI whose live-input set may have changed needs to re-evaluate
     * its fact (its meet excludes inputs from now-dead predecessor
     * edges). Enqueue every PHI onto cprop — PROPAGATE will recompute
     * cp_node_const(phi) with the updated cp_phi_input_live filter,
     * cascading any fact falls to users. */
    if (rc != eng->reach_count) {
        for (int v = 0; v < eng->vnode_count; v++)
            if (eng->vnodes[v]->kind == CP_VN_PHI)
                cp_cprop_enqueue(eng, v);
    }
    eng->reach_count = rc;
}

/* The ONE spine-reachability accessor. A spine node spliced after
 * reachable[] was computed (a CSE lift's StoreLocal — index past
 * reachable_rows) has no entry; it is reachable by construction
 * (lifts splice into live flow only). */
static bool cp_spine_reachable(const cp_engine_t* eng, int i) {
    if (!eng->reachable) return true;
    if (i < 0) return false;
    if (i >= eng->reachable_rows) return true;
    return eng->reachable[i];
}

/* ── Backward slot liveness (Kildall, P4 prelude) ────────────── */

/* Slots an expression reads — same conservative scan as sir_opt.c's
 * expr_uses; written directly here per the design's "leaf helpers
 * are duplicated, old engine dies at P5" rule. */
static void cp_expr_uses(const sir_node_t* e, bool* uses, int sc) {
    if (!e) return;
    switch (e->tag) {
        case SIR_LOADLOCAL: {
            int s = e->load_local.slot;
            if (s >= 0 && s < sc) uses[s] = true;
            return;
        }
        case SIR_ADD: case SIR_SUB: case SIR_MUL:
        case SIR_DIV: case SIR_REM:
        case SIR_AND: case SIR_OR:  case SIR_XOR:
        case SIR_SHL: case SIR_SHR: case SIR_USHR:
            cp_expr_uses(e->add.left,  uses, sc);
            cp_expr_uses(e->add.right, uses, sc);
            return;
        case SIR_NEG:    cp_expr_uses(e->neg.operand,     uses, sc); return;
        case SIR_LOGNOT: cp_expr_uses(e->log_not.operand, uses, sc); return;
        SIR_CONV_CASES   cp_expr_uses(sir_child(e, 0), uses, sc); return;
        SIR_CMP_CASES
            cp_expr_uses(sir_child(e, 0), uses, sc);
            cp_expr_uses(sir_child(e, 1), uses, sc);
            return;
        case SIR_INSTANCEOF: cp_expr_uses(e->instance_of.obj,    uses, sc); return;
        case SIR_CHECKCAST:  cp_expr_uses(e->check_cast.obj,     uses, sc); return;
        case SIR_ARRAYLOAD:
            cp_expr_uses(e->array_load.arr,   uses, sc);
            cp_expr_uses(e->array_load.index, uses, sc);
            return;
        case SIR_ARRAYLENGTH:  cp_expr_uses(e->array_length.arr,    uses, sc); return;
        case SIR_GETFIELD:     cp_expr_uses(e->get_field.obj,        uses, sc); return;
        case SIR_NEWARRAY:     cp_expr_uses(e->new_array.size,       uses, sc); return;
        case SIR_NEWREFARRAY:  cp_expr_uses(e->new_ref_array.size,   uses, sc); return;
        case SIR_INVOKEVIRTUAL:
            cp_expr_uses(e->invoke_virtual.obj, uses, sc);
            for (int i = 0; i < e->invoke_virtual.args_count; i++)
                cp_expr_uses(e->invoke_virtual.args[i], uses, sc);
            return;
        case SIR_INVOKESPECIAL:
            cp_expr_uses(e->invoke_special.obj, uses, sc);
            for (int i = 0; i < e->invoke_special.args_count; i++)
                cp_expr_uses(e->invoke_special.args[i], uses, sc);
            return;
        case SIR_INVOKEINTERFACE:
            cp_expr_uses(e->invoke_interface.obj, uses, sc);
            for (int i = 0; i < e->invoke_interface.args_count; i++)
                cp_expr_uses(e->invoke_interface.args[i], uses, sc);
            return;
        case SIR_INVOKESTATIC:
            for (int i = 0; i < e->invoke_static.args_count; i++)
                cp_expr_uses(e->invoke_static.args[i], uses, sc);
            return;
        default: {
            /* Fail-closed: an opcode this switch doesn't name still
             * contributes every child's reads — a silently-skipped use
             * would let DSE delete a live store. */
            int n = sir_arity(e);
            for (int i = 0; i < n; i++)
                cp_expr_uses(sir_child((sir_node_t*)e, i), uses, sc);
            return;
        }
    }
}

/* Slots a spine node reads (gen set for liveness). Inc's read of its
 * slot is carried by inc.value = LoadLocal(slot); walking it is the
 * same shape as every other expression-bearing kont. */
static void cp_node_uses(const sir_node_t* n, bool* uses, int sc) {
    if (!n) return;
    int k = sir_arity(n);
    for (int i = 0; i < k; i++) cp_expr_uses(sir_child(n, i), uses, sc);
}

/* Slot a spine node defines (kill set), or -1 for none. */
static int cp_node_def_slot(const sir_node_t* n) {
    if (!n) return -1;
    switch (n->tag) {
        case SIR_STORELOCAL:     return n->store_local.slot;
        case SIR_INC:            return n->inc.slot;
        case SIR_EXCEPTIONENTRY: return n->exception_entry.local_slot;
        default:                 return -1;
    }
}

void cp_compute_liveness(cp_engine_t* eng) {
    int sn = eng->spine_count;
    int sc = eng->slot_count;
    if (sn == 0) return;
    size_t row = (size_t)sc * sizeof(bool);
    eng->live_in  = (bool**)bbq_arena_alloc(eng->arena, (size_t)sn * sizeof(bool*));
    eng->live_out = (bool**)bbq_arena_alloc(eng->arena, (size_t)sn * sizeof(bool*));
    for (int i = 0; i < sn; i++) {
        eng->live_in[i]  = (bool*)bbq_arena_alloc(eng->arena, row);
        eng->live_out[i] = (bool*)bbq_arena_alloc(eng->arena, row);
        memset(eng->live_in[i],  0, row);
        memset(eng->live_out[i], 0, row);
    }

    /* Reverse-edge index: for re-queuing predecessors when live_in changes. */
    int *pred_off, *pred_cnt, *pred_list;
    cp_build_pred_csr(eng, &pred_off, &pred_cnt, &pred_list);

    /* Kildall backward worklist. Queue all reachable spine nodes
     * initially; iterate until live_in stabilises. */
    int* wl = NULL;
    bool* queued = (bool*)bbq_arena_alloc(eng->arena, (size_t)sn * sizeof(bool));
    memset(queued, 0, (size_t)sn * sizeof(bool));
    for (int i = 0; i < sn; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        queued[i] = true;
        bbq_vec_push(wl, i);
    }
    bool* new_out = (bool*)bbq_arena_alloc(eng->arena, row);
    bool* new_in  = (bool*)bbq_arena_alloc(eng->arena, row);
    bool* exc_out = (bool*)bbq_arena_alloc(eng->arena, row);
    while (bbq_vec_len(wl)) {
        int i = wl[bbq_vec_len(wl) - 1];
        bbq__vec_hdr(wl)->len--;
        queued[i] = false;
        sir_node_t* n = eng->spine[i];

        /* live_out[i] = ⋃ live_in[succ] over reachable NORMAL successors …
         * … and ⋃ live_in[handler] over the EXCEPTIONAL ones (spec §1). A slot the catch
         * block reads is LIVE at every excepting node of the region — without these edges
         * a store whose only consumer is a catch block looks dead, and DSE deletes it. */
        memset(new_out, 0, row);
        memset(exc_out, 0, row);
        int k = sir_succ_count(n);
        for (int j = 0; j < k; j++) {
            int si = cp_spine_index(eng, sir_succ(n, j));
            if (si < 0) continue;
            if (!cp_spine_reachable(eng, si)) continue;
            for (int s = 0; s < sc; s++)
                if (eng->live_in[si][s]) new_out[s] = true;
        }
        for (int j = 0; j < cp_exc_succ_count(eng, i); j++) {
            int hi = cp_exc_succ(eng, i, j);
            if (!cp_spine_reachable(eng, hi)) continue;
            for (int s = 0; s < sc; s++)
                if (eng->live_in[hi][s]) { exc_out[s] = true; new_out[s] = true; }
        }
        /* live_in[i] = ((live_out_normal[i] − kill[i]) ∪ live_out_exc[i]) ∪ gen[i].
         *
         * THE KILL DOES NOT APPLY ON THE EXCEPTIONAL EDGE. `x = f()` that throws inside f
         * never assigns x (JLS §11.3.1), so on that edge x still holds its PRIOR value —
         * the old definition is live into the handler, and killing it here would let DSE
         * delete the store the catch block actually reads. Same asymmetry as
         * cp_except_state, seen from the other direction. */
        memcpy(new_in, new_out, row);
        int killed = cp_node_def_slot(n);
        if (killed >= 0 && killed < sc && !exc_out[killed]) new_in[killed] = false;
        cp_node_uses(n, new_in, sc);

        if (memcmp(new_in,  eng->live_in[i],  row) != 0
                || memcmp(new_out, eng->live_out[i], row) != 0) {
            memcpy(eng->live_in[i],  new_in,  row);
            memcpy(eng->live_out[i], new_out, row);
            /* Re-queue reachable predecessors. */
            for (int p = 0; p < pred_cnt[i]; p++) {
                int pi = pred_list[pred_off[i] + p];
                if (!cp_spine_reachable(eng, pi)) continue;
                if (!queued[pi]) { queued[pi] = true; bbq_vec_push(wl, pi); }
            }
        }
    }
    bbq_vec_free(wl);
}

/* ── Fact-driven partition splitting ─────────────────────────── */

/* §4.3.2 invariant: within a partition every node agrees on type and
 * constant. Split each partition by its members' (type, constant)
 * facts; returns true if any split occurred. */
static bool cp_split_by_facts(cp_engine_t* eng) {
    bool any = false;
    int snapshot = eng->partition_count;
    int* members = NULL;   /* Leaders only */
    int* groups  = NULL;
    for (int p = 0; p < snapshot; p++) {
        if (members) bbq__vec_hdr(members)->len = 0;
        /* Per Click §4.7.1, Followers live in their Leader's partition.
         * SPLIT_BY by (type, constant) operates on Leaders only — skip
         * Followers. They follow their Leader via cp_move_followers
         * when cp_part_remove fires below (the cp_move_followers helper
         * is invoked by cp_split's split path; here we do raw remove +
         * add, so propagate Followers explicitly below). */
        for (int n = eng->partitions[p]->head; n >= 0;
             n = eng->vnodes[n]->part_next)
            if (eng->vnodes[n]->leader == -1)
                bbq_vec_push(members, n);
        int mc = (int)bbq_vec_len(members);
        if (mc <= 1) continue;
        const Type* keep_t = eng->vnodes[members[0]]->type;
        cp_const_t  keep_c = eng->vnodes[members[0]]->constant;
        if (groups) bbq__vec_hdr(groups)->len = 0;
        for (int i = 1; i < mc; i++) {
            int n = members[i];
            const Type* t = eng->vnodes[n]->type;
            cp_const_t  c = eng->vnodes[n]->constant;
            if (t == keep_t && cp_const_eq(c, keep_c)) continue;
            int target = -1;
            for (int j = 0; j < (int)bbq_vec_len(groups); j++) {
                int g = groups[j], gh = eng->partitions[g]->head;
                if (gh >= 0 && eng->vnodes[gh]->type == t
                            && cp_const_eq(eng->vnodes[gh]->constant, c)) {
                    target = g;
                    break;
                }
            }
            if (target < 0) {
                target = cp_part_new(eng);
                bbq_vec_push(groups, target);
            }
            bool pend = cp_part_remove(eng, n);
            cp_part_add(eng, target, n);
            if (pend) cp_cprop_enqueue(eng, n);
            cp_notify_users_of_move(eng, n);
            /* Followers of n must follow n to the new partition. */
            cp_move_followers(eng, n, target);
            any = true;
        }
        /* Click §4.2's SPLIT contract: a partition that splits goes BACK on
         * the CAUSE_SPLITS worklist, together with the partitions carved out
         * of it. cp_notify_users_of_move only re-enqueues the users' FACTS
         * (cprop); it does not re-examine them by input POSITION — and for
         * users whose facts are identical, position is the only thing that
         * can tell them apart. `x >>> 24`, `x >>> 16`, `x >>> 8` all carry
         * BOTTOM (x unknown) and differ solely in which constant partition
         * feeds input 1: without this re-arm they stayed congruent and CSE
         * collapsed the three into one (DataOutputStream.writeInt wrote the
         * top byte four times). Enqueueing every resulting partition is
         * sound; Hopcroft's "all but the largest" is only a constant-factor
         * refinement of it, and refinement is monotone, so termination (at
         * most n-1 splits) is unaffected. */
        for (int j = 0; j < (int)bbq_vec_len(groups); j++)
            cp_wl_push(eng, groups[j]);
        if (bbq_vec_len(groups) > 0) cp_wl_push(eng, p);
    }
    bbq_vec_free(members);
    bbq_vec_free(groups);
    return any;
}

/* ── P4 rewrite (one-shot, off the converged engine) ─────────── */

/* The value-node attached to expression `e`, or NULL if cp_enumerate
 * didn't see it (defensive — every reachable expression should have
 * one). */
static cp_vnode_t* cp_find_vnode(cp_engine_t* eng, sir_node_t* e) {
    if (!e) return NULL;
    void* f = cp_pmap_get(&eng->expr_idx, e);
    return f ? eng->vnodes[(int)((uintptr_t)f - 1)] : NULL;
}

/* ── Redundant narrowing-conversion elimination ──
 *
 * A narrowing conversion (s2b / i2s / i2b) truncates to the target width
 * and sign-extends back. When the operand's lattice range already fits
 * that width, the conversion is the identity and is dropped at rewrite
 * (cp_rewrite_expr). On WASM that also removes the i32 mask the conversion
 * would otherwise lower to. The value stays i32 — byte/short/char/int all
 * compute as i32, so there is no narrower arithmetic width to retag to. */

static bool cp_const_fits_byte(cp_const_t c) {
    if (c.state == CP_C_KNOWN) return c.value >= INT8_MIN  && c.value <= INT8_MAX;
    if (c.state == CP_C_RANGE) return c.lo    >= INT8_MIN  && c.hi    <= INT8_MAX;
    return false;
}

static bool cp_const_fits_short(cp_const_t c) {
    if (c.state == CP_C_KNOWN) return c.value >= INT16_MIN && c.value <= INT16_MAX;
    if (c.state == CP_C_RANGE) return c.lo    >= INT16_MIN && c.hi    <= INT16_MAX;
    return false;
}

static sir_datatype_t cp_replace_width(const cp_vnode_t* v, sir_node_t* e) {
    if (v && v->type && v->type->kind == TK_PRIM) return v->type->prim.width;
    if (!e || e->tag < 0 || e->tag >= SIR_TAG_COUNT) return SIR_DTINT;
    const sir_op_gamma_t* g = &sir_op_gamma[e->tag];
    switch (g->type_kind) {
        case GT_PRIM_DT:    return g->type_prim_dt(e);
        case GT_PRIM_FIXED: return g->type_prim_fixed_dt;
        default:            return SIR_DTINT;
    }
}

/* THE test for "the rewrite replaces this node with a LoadConst" — which also means
 * "and DISCARDS its entire subtree", since cp_rewrite_expr returns the constant in
 * place of `e`. §6's scalar-replacement sweep asks the same question when deciding
 * whether an occurrence of a value is still a USE (a `LoadLocal(o)` under an
 * already-proven `Eq(o, LoadNull)` guard is not — that guard is about to fold), so
 * the test lives in ONE function and both callers ask IT. A second copy is how the
 * sweep and the rewrite come to disagree about what is still in the graph.
 *
 * The representation gate is part of the test: a KNOWN i32 fact on a long-valued
 * node must not emit an i32.const, so such a node is NOT substituted and its
 * subtree survives. */
static bool cp_const_subst_applies(cp_engine_t* eng, sir_node_t* e) {
    const cp_vnode_t* v = cp_find_vnode(eng, e);
    if (!v || v->constant.state != CP_C_KNOWN) return false;
    int ec = cp_expr_result_vtclass(e);
    int cc = v->constant.cwidth == CP_W_I64 ? 1
           : v->constant.cwidth == CP_W_F32 ? 2
           : v->constant.cwidth == CP_W_F64 ? 3 : 0;
    return ec >= 0 && ec == cc;
}

/* Recursively rewrite each child of `e`, then substitute `e` itself
 * per the converged engine's facts. Mutates `e`'s children in place
 * and returns the replacement node (which may be a fresh LoadConst,
 * the original `e`, or — later sub-slices — another existing node). */
static sir_node_t* cp_rewrite_expr(cp_engine_t* eng, sir_node_t* e) {
    if (!e) return NULL;

    /* §6's scalar replacement: a GetField of a replaced object's field IS the LoadLocal of
     * that field's slot. Consulted BEFORE the recursion — the object is gone, so there is
     * nothing left in this node's subtree worth rewriting (its receiver was the ref). */
    void* sub = cp_pmap_get(&eng->scalar_subst, e);
    if (sub) return (sir_node_t*)sub;

    /* Children first so inner facts propagate outward. */
    switch (e->tag) {
        case SIR_ADD: case SIR_SUB: case SIR_MUL: case SIR_DIV: case SIR_REM:
        case SIR_AND: case SIR_OR:  case SIR_XOR:
        case SIR_SHL: case SIR_SHR: case SIR_USHR:
            e->add.left  = cp_rewrite_expr(eng, e->add.left);
            e->add.right = cp_rewrite_expr(eng, e->add.right);
            break;
        case SIR_NEG:    e->neg.operand     = cp_rewrite_expr(eng, e->neg.operand);     break;
        case SIR_LOGNOT: e->log_not.operand = cp_rewrite_expr(eng, e->log_not.operand); break;
        /* Narrowing conversions are identity when the operand's lattice
         * range already fits the target width — the truncate-and-sign-
         * extend changes nothing, so drop the conversion (and, on WASM,
         * the i32 mask it would lower to). The value stays i32; all of
         * byte/short/char/int compute as i32, so no width retag is needed. */
        case SIR_S2B: {
            cp_vnode_t* ov = cp_find_vnode(eng, e->s2_b.operand);
            if (ov && cp_const_fits_byte(ov->constant))
                return cp_rewrite_expr(eng, e->s2_b.operand);
            e->s2_b.operand = cp_rewrite_expr(eng, e->s2_b.operand);
            break;
        }
        case SIR_I2S: {
            cp_vnode_t* ov = cp_find_vnode(eng, e->i2_s.operand);
            if (ov && cp_const_fits_short(ov->constant))
                return cp_rewrite_expr(eng, e->i2_s.operand);
            e->i2_s.operand = cp_rewrite_expr(eng, e->i2_s.operand);
            break;
        }
        case SIR_I2B: {
            cp_vnode_t* ov = cp_find_vnode(eng, e->i2_b.operand);
            if (ov && cp_const_fits_byte(ov->constant))
                return cp_rewrite_expr(eng, e->i2_b.operand);
            e->i2_b.operand = cp_rewrite_expr(eng, e->i2_b.operand);
            break;
        }
        case SIR_S2I:    e->s2_i.operand    = cp_rewrite_expr(eng, e->s2_i.operand);    break;
        /* The width-changing conversions (plus the Move* reinterprets
         * and f64 intrinsics): recurse into the operand; the conversion
         * itself never drops (a float→int etc. isn't redundant). */
        case SIR_I2C: case SIR_I2L: case SIR_I2F: case SIR_I2D:
        case SIR_L2I: case SIR_L2F: case SIR_L2D:
        case SIR_F2I: case SIR_F2L: case SIR_F2D:
        case SIR_D2I: case SIR_D2L: case SIR_D2F:
        case SIR_MOVEF2I: case SIR_MOVEI2F: case SIR_MOVED2L: case SIR_MOVEL2D:
        case SIR_F64SQRT: case SIR_F64FLOOR: case SIR_F64CEIL: case SIR_F64NEAREST: {
            sir_node_t** s = sir_conv_operand_slot(e);
            *s = cp_rewrite_expr(eng, *s);
            break;
        }
        SIR_CMP_CASES
            *sir_cmp_child_slot(e, 0) = cp_rewrite_expr(eng, sir_child(e, 0));
            *sir_cmp_child_slot(e, 1) = cp_rewrite_expr(eng, sir_child(e, 1));
            break;
        case SIR_INSTANCEOF: e->instance_of.obj = cp_rewrite_expr(eng, e->instance_of.obj); break;
        case SIR_CHECKCAST:  e->check_cast.obj  = cp_rewrite_expr(eng, e->check_cast.obj);  break;
        case SIR_ARRAYLOAD:
            e->array_load.arr   = cp_rewrite_expr(eng, e->array_load.arr);
            e->array_load.index = cp_rewrite_expr(eng, e->array_load.index);
            break;
        case SIR_ARRAYLENGTH:  e->array_length.arr   = cp_rewrite_expr(eng, e->array_length.arr);   break;
        case SIR_GETFIELD:     e->get_field.obj      = cp_rewrite_expr(eng, e->get_field.obj);      break;
        case SIR_NEWARRAY:     e->new_array.size     = cp_rewrite_expr(eng, e->new_array.size);     break;
        case SIR_NEWREFARRAY:  e->new_ref_array.size = cp_rewrite_expr(eng, e->new_ref_array.size); break;
        case SIR_INVOKEVIRTUAL:
            e->invoke_virtual.obj = cp_rewrite_expr(eng, e->invoke_virtual.obj);
            for (int i = 0; i < e->invoke_virtual.args_count; i++)
                e->invoke_virtual.args[i] = cp_rewrite_expr(eng, e->invoke_virtual.args[i]);
            break;
        case SIR_INVOKESPECIAL:
            e->invoke_special.obj = cp_rewrite_expr(eng, e->invoke_special.obj);
            for (int i = 0; i < e->invoke_special.args_count; i++)
                e->invoke_special.args[i] = cp_rewrite_expr(eng, e->invoke_special.args[i]);
            break;
        case SIR_INVOKEINTERFACE:
            e->invoke_interface.obj = cp_rewrite_expr(eng, e->invoke_interface.obj);
            for (int i = 0; i < e->invoke_interface.args_count; i++)
                e->invoke_interface.args[i] = cp_rewrite_expr(eng, e->invoke_interface.args[i]);
            break;
        case SIR_INVOKESTATIC:
            for (int i = 0; i < e->invoke_static.args_count; i++)
                e->invoke_static.args[i] = cp_rewrite_expr(eng, e->invoke_static.args[i]);
            break;
        default: break;
    }

    cp_vnode_t* v = cp_find_vnode(eng, e);
    if (!v) return e;

    /* Constant substitution. Skip if `e` is already that LoadConst
     * — otherwise the rewrite loops, regenerating equivalent nodes. */
    if (cp_const_subst_applies(eng, e)) {
        switch (v->constant.cwidth) {
            case CP_W_I64:
                if (e->tag == SIR_LOADLONGCONST
                        && e->load_long_const.value == v->constant.lvalue) return e;
                return sir_load_long_const(eng->arena, v->constant.lvalue);
            case CP_W_F32: {
                /* Bit-compare, not ==: a NaN is never == itself, and ±0.0 are
                 * == but distinct values. Reuse the node iff its bits match. */
                float k = cp_known_f32(v->constant);
                if (e->tag == SIR_LOADFLOATCONST
                        && memcmp(&e->load_float_const.value, &k, sizeof k) == 0) return e;
                return sir_load_float_const(eng->arena, k);
            }
            case CP_W_F64: {
                double k = cp_known_f64(v->constant);
                if (e->tag == SIR_LOADDOUBLECONST
                        && memcmp(&e->load_double_const.value, &k, sizeof k) == 0) return e;
                return sir_load_double_const(eng->arena, k);
            }
            default:
                if (e->tag == SIR_LOADCONST && e->load_const.value == v->constant.value)
                    return e;
                return sir_load_const(eng->arena, v->constant.value,
                                      cp_replace_width(v, e));
        }
    }

    /* Fallback for partitions with no emittable representative (e.g.
     * an OPAQUE seed's partition holds the seed plus LOADLOCAL COPY
     * Followers — the seed has no expr and Followers don't qualify
     * as reps). Walk the Follower chain to the first emittable leaf
     * and emit it: the LOADLOCAL of the seed's slot is the natural
     * "load-immediate" stand-in for OPAQUE seeds, matching §4.10's
     * "representative Node can be a load-immediate of the constant"
     * for the OPAQUE analog. Restricting to pure leaves (LoadLocal /
     * LoadConst / LoadNull / LoadThis) is critical: emitting an
     * arbitrary StoreLocal-value expression (New, ArrayLoad, Invoke)
     * would duplicate side effects or allocations at every read site. */
    if (v->leader >= 0) {
        int lid = v->leader;
        while (lid >= 0 && lid < eng->vnode_count) {
            sir_node_t* lex = eng->vnodes[lid]->expr;
            if (lex && lex != e &&
                (lex->tag == SIR_LOADLOCAL || lex->tag == SIR_LOADCONST ||
                 lex->tag == SIR_LOADNULL  || lex->tag == SIR_LOADTHIS)) {
                /* Representation gate: congruent value, but the leaf's
                 * emission must carry e's lowered valtype (and, for
                 * refs, e's referent) — see cp_leaf_substitutable. */
                bool ok;
                if (e->tag == SIR_LOADLOCAL || e->tag == SIR_LOADCONST) {
                    ok = cp_leaf_substitutable(&eng->pool, e, lex);
                } else {
                    int ec = cp_expr_result_vtclass(e);
                    int lc = cp_expr_result_vtclass(lex);
                    ok = ec >= 0 && ec == lc
                      && (ec != (int)LAT_VT_REF || lex->tag == SIR_LOADNULL
                                  || lex->tag == SIR_LOADTHIS);
                }
                if (ok && cp_slot_still_holds(eng, lex, v->partition))
                    return cp_rewrite_expr(eng, lex);
            }
            if (eng->vnodes[lid]->leader < 0) break;
            lid = eng->vnodes[lid]->leader;
        }
    }

    /* ── §3's DEVIRTUALIZATION consumer, over §0's TARGET SET ────────────
     *
     * Devirtualize iff the call site's TARGET SET is a singleton (cp_call_target_set —
     * lattice A's classes, resolved through JLS §8.4.8). §3's "pts singleton with exact
     * class" is only the case where that set has one element because there is one OBJECT;
     * two receiver classes sharing one implementation, and a `final` method on an unknown
     * receiver, are each ONE target too.
     *
     * The direct-call node is InvokeSpecial, not InvokeStatic: a static call has no
     * receiver, and an instance method needs `this`. InvokeSpecial tiles to a plain `call`
     * — a dispatch with the vtable lookup removed, which is the thing. */
    if ((e->tag == SIR_INVOKEVIRTUAL || e->tag == SIR_INVOKEINTERFACE) && eng->sema) {
        bool iface     = (e->tag == SIR_INVOKEINTERFACE);
        sir_node_t* obj = iface ? e->invoke_interface.obj      : e->invoke_virtual.obj;
        int decl_cls    = iface ? e->invoke_interface.class_id : e->invoke_virtual.class_id;
        int decl_midx   = iface ? e->invoke_interface.method_idx
                                : e->invoke_virtual.method_idx;
        int impl_cls = -1, impl_midx = -1;
        if (cp_call_target_set(eng, cp_vnode_of(eng, obj), decl_cls, decl_midx,
                               &impl_cls, &impl_midx)) {
            eng->devirt_count++;
            return sir_invoke_special(eng->arena, obj, impl_cls, impl_midx,
                                      iface ? e->invoke_interface.args
                                            : e->invoke_virtual.args,
                                      iface ? e->invoke_interface.args_count
                                            : e->invoke_virtual.args_count,
                                      iface ? e->invoke_interface.return_type
                                            : e->invoke_virtual.return_type);
        }
    }

    /* CHECKCAST elim: when the TYPE LATTICE proves the operand's tracked type is already
     * ⊑ the cast target, the check always succeeds — return obj directly.
     *
     * ASK THE LATTICE, not a subtype predicate of our own (§10: consulted, never
     * duplicated). This used to call `sema_is_subclass_of` — the EXTENDS CHAIN — twice,
     * which is a second comparator for a question the lattice's ⊑ already answers, and
     * which silently missed every cast to an INTERFACE (an interface is in nobody's
     * extends chain). */
    if (e->tag == SIR_CHECKCAST && e->check_cast.atype == SIR_ATCLASS) {
        cp_vnode_t* ov = cp_find_vnode(eng, e->check_cast.obj);
        const Type* tgt = type_make_ref(&eng->pool, e->check_cast.class_id);
        if (ov && ov->type && type_leq(eng->sema, ov->type, tgt)) {
            /* Value-provably safe — but dropping the cast replaces the EMITTED type with
             * the operand's STATIC type (`Object o = new Box(); (Box) o` would store an
             * Object-typed local into a Box slot). Drop only when the operand's static
             * type is ALREADY ⊑ the target, so the representation cannot change. */
            const sir_node_t* obj = e->check_cast.obj;
            const Type* stat = NULL;
            if (obj->tag == SIR_LOADTHIS)
                stat = type_make_ref(&eng->pool, obj->load_this.class_id);
            else if (obj->tag == SIR_NEW)
                stat = type_make_ref(&eng->pool, obj->new_.class_id);
            else if (obj->tag == SIR_LOADLOCAL && obj->load_local.ref_type)
                stat = gamma_ref_to_type(obj->load_local.ref_type, &eng->pool);
            if (stat && type_leq(eng->sema, stat, tgt))
                return e->check_cast.obj;
        }
    }

    /* GVN slot-collapse: a LoadLocal whose reaching-def is a cheap
     * leaf (another LoadLocal / LoadConst / LoadNull / LoadThis) is
     * a copy — forward to the producer. Combined with DSE, the now-
     * dead StoreLocal drops. The reaching-def may itself be a §4.9
     * PHI Follower; walk the Follower chain to the ultimate Leader
     * before checking for leaf. */
    if (e->tag == SIR_LOADLOCAL && v->input_count == 1) {
        int pi = v->inputs[0];
        while (pi >= 0 && pi < eng->vnode_count
               && eng->vnodes[pi]->leader >= 0)
            pi = eng->vnodes[pi]->leader;
        if (pi >= 0 && pi < eng->vnode_count) {
            sir_node_t* pex = eng->vnodes[pi]->expr;
            if (pex && pex != e &&
                (pex->tag == SIR_LOADLOCAL || pex->tag == SIR_LOADCONST ||
                 pex->tag == SIR_LOADNULL  || pex->tag == SIR_LOADTHIS) &&
                cp_leaf_substitutable(&eng->pool, e, pex) &&
                cp_slot_still_holds(eng, pex, v->partition))
                return cp_rewrite_expr(eng, pex);
            /* Click §4.10 peer-PHI slot-collapse. When the reaching def
             * is a PHI in a partition holding a canonical peer-PHI at
             * a different slot, synthesize a fresh LoadLocal(canon_slot)
             * — gated on the canonical slot's reaching def at THIS use
             * point being in the same partition (slot_in dominance
             * proxy: a partition is in scope at a spine point iff every
             * reaching def of its slots reaches that point in the same
             * partition). Synthesizing a leaf-only sir_load_local
             * avoids the SIR cycles that direct node substitution
             * creates. */
            cp_vnode_t* pv = eng->vnodes[pi];
            if (pv->kind == CP_VN_PHI && pv->partition >= 0
                    && pv->partition < eng->part_canon_phi_cap
                    && eng->part_canon_phi) {
                int canon = eng->part_canon_phi[pv->partition];
                if (canon >= 0 && canon != pi
                        && eng->slot_in
                        && eng->rewrite_spine_idx >= 0
                        && eng->rewrite_spine_idx < eng->slot_in_rows) {
                    int canon_slot = eng->vnodes[canon]->phi_slot;
                    /* Representation gate: only collapse PRIMITIVE
                     * slots whose canonical φ's lattice width lowers to
                     * e's valtype — congruent VALUES can live in
                     * differently-typed locals (int vs long, refs of
                     * different classes), and a cross-valtype local.get
                     * is ill-typed. */
                    const Type* ct = eng->vnodes[canon]->type;
                    if (canon_slot >= 0 && canon_slot < eng->slot_count
                            && canon_slot != e->load_local.slot
                            && e->load_local.data_type != SIR_DTREF
                            && ct && ct->kind == TK_PRIM
                            && lat_dt_valtype(ct->prim.width)
                               == lat_dt_valtype(e->load_local.data_type)) {
                        int rd = eng->slot_in[eng->rewrite_spine_idx][canon_slot];
                        if (rd >= 0 && rd < eng->vnode_count
                                && eng->vnodes[rd]->partition == pv->partition)
                            return sir_load_local(eng->arena, canon_slot,
                                                  e->load_local.data_type,
                                                  e->load_local.ref_type);
                    }
                }
            }
        }
    }
    return e;
}

/* Rewrite the expression children of spine node `n` in place. */
static void cp_rewrite_spine_node(cp_engine_t* eng, sir_node_t* n) {
    if (!n) return;
    /* Inc's value child is the slot-read pin (ddcg invariant:
     * value.load_local.slot == inc.slot); substituting it would break
     * the Inc(LoadLocal) fusion — leave it alone. */
    if (n->tag == SIR_INC) return;
    int k = sir_arity(n);
    for (int i = 0; i < k; i++) {
        sir_node_t** slot = sir_child_slot(n, i);
        if (slot) *slot = cp_rewrite_expr(eng, *slot);
    }
}

/* DSE — drop a StoreLocal / Inc whose target slot is dead at exit.
 * Pure value → Nop; impure value → ExprEffect (the side effect must
 * still run). Mirrors sir_opt.c's click_dse, in-place re-tagging the
 * SIR union node so the spine's predecessors still reach it. */
static void cp_rewrite_dse(cp_engine_t* eng) {
    for (int i = 0; i < eng->spine_count; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];
        if (n->tag == SIR_STORELOCAL) {
            int s = n->store_local.slot;
            if (s < 0 || s >= eng->slot_count) continue;
            if (eng->live_out[i][s]) continue;
            sir_node_t* val = n->store_local.value;
            sir_node_t* nxt = n->store_local.next;
            if (cp_expr_is_pure(val)) {
                n->tag = SIR_NOP;
                n->nop.next = nxt;
            } else {
                n->tag = SIR_EXPREFFECT;
                n->expr_effect.value   = val;
                n->expr_effect.is_void = 0;
                n->expr_effect.next    = nxt;
            }
        } else if (n->tag == SIR_INC) {
            int s = n->inc.slot;
            if (s < 0 || s >= eng->slot_count) continue;
            if (eng->live_out[i][s]) continue;
            sir_node_t* nxt = n->inc.next;
            n->tag = SIR_NOP;
            n->nop.next = nxt;
        }
    }
}

/* UCE-driven branch / switch folding: when the condition or selector
 * is a literal LoadConst (which it will be by here if its constant
 * was KNOWN — cp_rewrite_expr already substituted it), pick the
 * sole live successor and re-tag the spine node as a Goto. */
static void cp_rewrite_branch_fold(cp_engine_t* eng) {
    for (int i = 0; i < eng->spine_count; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];
        if (n->tag == SIR_BRANCH) {
            sir_node_t* cond = n->branch.cond;
            if (!cond) continue;
            if (cond->tag != SIR_LOADCONST) {
                /* Channel (a): a condition the fold left standing whose VALUE a
                 * path-invariant verdict decided. The links are converged here,
                 * so the identity match is final. */
                int vd = cp_branch_verdict(eng, i, cond);
                if (vd < 0) continue;
                n->tag = SIR_NOP;
                n->nop.next = vd ? n->branch.on_true : n->branch.on_false;
                continue;
            }
            sir_node_t* target = (cond->load_const.value != 0)
                               ? n->branch.on_true
                               : n->branch.on_false;
            n->tag = SIR_NOP;
            n->nop.next = target;
        } else if (n->tag == SIR_SWITCH) {
            sir_node_t* sel = n->switch_.selector;
            if (!sel || sel->tag != SIR_LOADCONST) continue;
            int nc = n->switch_.case_targets_count;
            sir_node_t* target = n->switch_.default_target;
            for (int j = 0; j < nc; j++)
                if (n->switch_.case_values[j] == sel->load_const.value) {
                    target = n->switch_.case_targets[j];
                    break;
                }
            n->tag = SIR_NOP;
            n->nop.next = target;
        }
    }
}


/* Walk forward through Nop links to reach the next "real" spine
 * node — used to detect Branches whose arms both converge to the
 * same point through trivial Nop chains (which DSE often creates
 * by retagging dead StoreLocals as Nops). */
static sir_node_t* cp_follow_nops_gotos(sir_node_t* n) {
    for (int g = 0; n && g < 128; g++) {
        if (n->tag == SIR_NOP) n = n->nop.next;
        else break;
    }
    return n;
}

/* Skip linear NOP chains for `.next` walks, preserving merge / jump-
 * target NOPs (their PCs are emit_label targets). */
static sir_node_t* cp_follow_nops_keep_merges(sir_node_t* n,
                                               const bool* is_merge_spine,
                                               cp_engine_t* eng) {
    for (int g = 0; n && g < 128; g++) {
        if (n->tag != SIR_NOP) break;
        int idx = cp_spine_index(eng, n);
        if (idx >= 0 && is_merge_spine[idx]) break;
        n = n->nop.next;
    }
    return n;
}

/* For Branch arm / Switch case targets: follow NOP chains to
 * compress jump-to-label sequences. Preserve merge / jump-target
 * NOPs so the chain stops at a node with a registered PC. */
static sir_node_t* cp_follow_nops_gotos_keep_merges(sir_node_t* n,
                                                     const bool* is_merge_spine,
                                                     cp_engine_t* eng) {
    for (int g = 0; n && g < 128; g++) {
        if (n->tag != SIR_NOP) break;
        int idx = cp_spine_index(eng, n);
        if (idx >= 0 && is_merge_spine[idx]) break;
        n = n->nop.next;
    }
    return n;
}

/* Empty-branch fold: when a Branch's on_true and on_false converge
 * to the same node, retag as Nop whose .next is that converged node.
 * The Branch becomes a no-op; control falls through to the merge. */
static void cp_rewrite_empty_branch(cp_engine_t* eng) {
    for (int i = 0; i < eng->spine_count; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];
        if (n->tag != SIR_BRANCH) continue;
        sir_node_t* t = cp_follow_nops_gotos(n->branch.on_true);
        sir_node_t* f = cp_follow_nops_gotos(n->branch.on_false);
        if (t && t == f) {
            n->tag = SIR_NOP;
            n->nop.next = t;
        }
    }
}

/* Splice out Nop placeholders and short-chain Goto-to-Goto sequences.
 * ddcg emits a Nop at every branch target (so emit_backpatch can
 * resolve labels) and a Goto for every control-flow jump; cp_rewrite_
 * dse converts dead StoreLocals into more Nops. None of these carry
 * runtime work — they're spine plumbing. Re-point every successor
 * edge through cp_follow_nops_gotos so the orphaned plumbing nodes
 * drop out of the next spine collection (cp_pack's DFS). */
static void cp_rewrite_compact_nops_gotos(cp_engine_t* eng) {
    /* "Guerrilla phi node" NOPs — those serving as control-flow
     * landing pads or stack-depth merge points. emit_backpatch needs
     * the registered PC; the verifier joins stack-depth at them.
     * Two distinct reasons to preserve a NOP:
     *  (a) merge: 2+ predecessors via any edge (loop headers, joins).
     *  (b) jump target: reached via a Branch arm / Goto / Switch case
     *      — predecessor's edge is a jump opcode that backpatches to
     *      the NOP's PC, not a linear fall-through.
     * Linear .next chains over NOPs are safe to skip; those NOPs
     * carry no codegen output and have no opcode targeting them. */
    int* pred_cnt = (int*)bbq_arena_alloc(eng->arena,
                          (size_t)eng->spine_count * sizeof(int));
    bool* is_jump_target = (bool*)bbq_arena_alloc(eng->arena,
                          (size_t)eng->spine_count * sizeof(bool));
    memset(pred_cnt, 0, (size_t)eng->spine_count * sizeof(int));
    memset(is_jump_target, 0, (size_t)eng->spine_count * sizeof(bool));
    int entry_idx = cp_spine_index(eng, eng->method->entry);
    if (entry_idx >= 0) pred_cnt[entry_idx]++;
    for (int i = 0; i < eng->spine_count; i++) {
        sir_node_t* n = eng->spine[i];
        if (!n) continue;
        int sc = sir_succ_count(n);
        for (int j = 0; j < sc; j++) {
            int si = cp_spine_index(eng, sir_succ(n, j));
            if (si >= 0 && si < eng->spine_count) pred_cnt[si]++;
        }
        /* Mark jump-emitting predecessors' targets as landing pads.
         * TryRegion.handler is an exception entry — the exception
         * table references its PC; ExceptionEntry also emits_label
         * (codegen.burg line 562). Both need their PC preserved. */
        switch (n->tag) {
            case SIR_BRANCH: {
                int t = cp_spine_index(eng, n->branch.on_true);
                int f = cp_spine_index(eng, n->branch.on_false);
                if (t >= 0 && t < eng->spine_count) is_jump_target[t] = true;
                if (f >= 0 && f < eng->spine_count) is_jump_target[f] = true;
                break;
            }
            case SIR_SWITCH: {
                for (int j = 0; j < n->switch_.case_targets_count; j++) {
                    int si = cp_spine_index(eng, n->switch_.case_targets[j]);
                    if (si >= 0 && si < eng->spine_count) is_jump_target[si] = true;
                }
                int dt = cp_spine_index(eng, n->switch_.default_target);
                if (dt >= 0 && dt < eng->spine_count) is_jump_target[dt] = true;
                break;
            }
            case SIR_TRYREGION: {
                /* Exception table needs both start_pc (the body's
                 * entry NOP — TryRegion.next per the compiler's
                 * compiler_try_region_t struct) and handler_pc. Each
                 * finally / catch handler is its own SIR_TRYREGION
                 * targeting a distinct handler entry; preserving
                 * .next AND .handler covers all of them. */
                int ti = cp_spine_index(eng, n->try_region.next);
                int hi = cp_spine_index(eng, n->try_region.handler);
                if (ti >= 0 && ti < eng->spine_count) is_jump_target[ti] = true;
                if (hi >= 0 && hi < eng->spine_count) is_jump_target[hi] = true;
                break;
            }
            default: break;
        }
    }
    bool* is_merge = (bool*)bbq_arena_alloc(eng->arena,
                          (size_t)eng->spine_count * sizeof(bool));
    for (int i = 0; i < eng->spine_count; i++)
        is_merge[i] = pred_cnt[i] >= 2 || is_jump_target[i];

    /* NEXT walks only NOPs (preserving Gotos as control transfers).
     * JUMP walks NOPs + Gotos (compressing jump-to-jump chains). */
    #define NEXT(p) cp_follow_nops_keep_merges((p), is_merge, eng)
    #define JUMP(p) cp_follow_nops_gotos_keep_merges((p), is_merge, eng)
    if (eng->method->entry)
        eng->method->entry = NEXT(eng->method->entry);
    for (int i = 0; i < eng->spine_count; i++) {
        sir_node_t* n = eng->spine[i];
        if (!n) continue;
        switch (n->tag) {
            case SIR_STORELOCAL: n->store_local.next = NEXT(n->store_local.next); break;
            case SIR_EXPREFFECT: n->expr_effect.next = NEXT(n->expr_effect.next); break;
            case SIR_ARRAYSTORE: n->array_store.next = NEXT(n->array_store.next); break;
            case SIR_ARRAYCOPY:  n->array_copy.next  = NEXT(n->array_copy.next);  break;
            case SIR_SETHEADER:  n->set_header.next  = NEXT(n->set_header.next);  break;
            case SIR_SIMDMEMSTORE:
                n->simd_mem_store.next = NEXT(n->simd_mem_store.next); break;
            case SIR_SIMDMEMSTORELANE:
                n->simd_mem_store_lane.next = NEXT(n->simd_mem_store_lane.next); break;
            case SIR_MEMSTOREI:  n->mem_store_i.next = NEXT(n->mem_store_i.next); break;
            case SIR_MEMSTOREL:  n->mem_store_l.next = NEXT(n->mem_store_l.next); break;
            case SIR_MEMSTOREF:  n->mem_store_f.next = NEXT(n->mem_store_f.next); break;
            case SIR_MEMSTORED:  n->mem_store_d.next = NEXT(n->mem_store_d.next); break;
            case SIR_MEMFILL:    n->mem_fill.next    = NEXT(n->mem_fill.next);    break;
            case SIR_MEMCOPY:    n->mem_copy.next    = NEXT(n->mem_copy.next);    break;
            case SIR_PUTFIELD:   n->put_field.next   = NEXT(n->put_field.next);   break;
            case SIR_PUTSTATIC:  n->put_static.next  = NEXT(n->put_static.next);  break;
            case SIR_INC:        n->inc.next         = NEXT(n->inc.next);         break;
            case SIR_EXCEPTIONENTRY:
                n->exception_entry.next = NEXT(n->exception_entry.next); break;
            case SIR_TRYREGION:  n->try_region.next  = NEXT(n->try_region.next);  break;
            /* Merge-NOPs (landing pads) stay in the spine, but their
             * `.next` should still skip past intermediate non-merge
             * NOPs — without this, a Goto-to-merge sequence carries
             * a dead Nop tail after the merge. */
            case SIR_NOP:        n->nop.next         = NEXT(n->nop.next);         break;
            case SIR_BRANCH:
                n->branch.on_true  = JUMP(n->branch.on_true);
                n->branch.on_false = JUMP(n->branch.on_false);
                break;
            case SIR_SWITCH:
                for (int j = 0; j < n->switch_.case_targets_count; j++)
                    n->switch_.case_targets[j] = JUMP(n->switch_.case_targets[j]);
                n->switch_.default_target = JUMP(n->switch_.default_target);
                break;
            default: break;
        }
    }
    #undef NEXT
    #undef JUMP
}

/* Click §4.10 peer-PHI slot-collapse: for each partition holding ≥2
 * peer PHIs at different (merge, slot) keys, designate a canonical
 * PHI. Reads of the non-canonical PHIs' slots get rewritten to reads
 * of the canonical slot when the canonical slot is in scope at the
 * use point (slot_in dominance proxy). Populates eng->part_canon_phi
 * once before rewrite. */
static void cp_select_canonical_phis(cp_engine_t* eng) {
    int pc = eng->partition_count;
    size_t alloc_pc = (size_t)(pc > 0 ? pc : 1);
    eng->part_canon_phi = (int*)bbq_arena_alloc(eng->arena, alloc_pc * sizeof(int));
    eng->part_canon_phi_cap = pc;
    for (int p = 0; p < pc; p++) eng->part_canon_phi[p] = -1;
    /* Pass 1: pick the smallest-vnode-idx Leader PHI per partition. */
    for (int v = 0; v < eng->vnode_count; v++) {
        cp_vnode_t* vn = eng->vnodes[v];
        if (vn->kind != CP_VN_PHI) continue;
        if (vn->leader >= 0) continue;
        if (vn->phi_slot < 0) continue;
        if (vn->partition < 0 || vn->partition >= pc) continue;
        if (eng->part_canon_phi[vn->partition] < 0)
            eng->part_canon_phi[vn->partition] = v;
    }
    /* Pass 2: drop the canonical mark for partitions where every PHI
     * is at the same slot — no slot-collapse to do. */
    for (int p = 0; p < pc; p++) {
        int canon = eng->part_canon_phi[p];
        if (canon < 0) continue;
        int canon_slot = eng->vnodes[canon]->phi_slot;
        bool has_other_slot = false;
        for (int v = 0; v < eng->vnode_count; v++) {
            cp_vnode_t* vn = eng->vnodes[v];
            if (vn->partition != p) continue;
            if (vn->kind != CP_VN_PHI) continue;
            if (vn->phi_slot >= 0 && vn->phi_slot != canon_slot) {
                has_other_slot = true;
                break;
            }
        }
        if (!has_other_slot) eng->part_canon_phi[p] = -1;
    }
}

/* ── §3's ArrayStore consumer (JLS §10.10) ──────────────────────────
 *
 * "…and the covariant-store ArrayStore guard when the element class is provably ≤ the
 * array's component."
 *
 * §10.2 array covariance means `Object[] o = new String[1]` is legal, so `o[i] = v` must
 * check v against the array's ACTUAL component at runtime and throw ArrayStoreException
 * if it does not fit. The DDCG emits that as a CALL — `Class.arrayStoreCheck(a.elemClass,
 * v)` in an ExprEffect — because the target is a runtime Class, which no static ref.test
 * can name. That shape is why this guard was never recorded and never counted.
 *
 * A REWRITE and not a transfer, and the distinction matters (see cp_instanceof_const):
 * there is no branch here, so there is no edge to kill and no fact for the fixpoint to
 * propagate. Removing the call is a graph edit made FROM facts the solve already has —
 * the same class as devirt's node swap, or DCE. Nothing downstream would learn anything
 * new from it during the solve.
 *
 * THE COMPONENT CLASS IS A FACT, NOT A PATTERN. The elemClass field is written at
 * allocation with `LoadClass(C)`, and this guard reads it back out; the load-after-store
 * identity (§1/§8) forwards that read to the LoadClass node itself. So we ask the value
 * graph what the check is testing against — no shape matching on the allocation.
 *
 * FAIL-CLOSED: an array whose component we cannot name (a parameter — it may be an array
 * of anything), or a stored value with one object of unknown class, keeps its check.
 * ⊥null is always safe: storing null into a reference array never throws (§10.10). */
/* ── §6's CONSUMER — scalar replacement: QUALIFICATION ──────────────────────
 *
 * §6: "NoEscape ⟹ scalar-replace the struct.new — its fields become SSA values /
 * LOCALS". A site qualifies iff EVERY surviving occurrence of its value is a
 * position the rewrite knows how to remove. The whitelist is the whole safety
 * argument, so it is fail-closed: a position not named below kills the site.
 *
 * WHAT A "USE" IS. §8: "a value IS a node; using it IS an edge." An occurrence is a
 * (parent node, child slot) pair, and the parent tells us everything — so the sweep
 * walks each reachable spine node's data children with the generic sir_arity /
 * sir_child accessors, exactly as cp_enumerate does. It asks no where-question and
 * no order-question: there is no reverse index, no parent table, no dominance.
 *
 * WHY IT MIRRORS THE REWRITE'S RECURSION. cp_rewrite_expr recurses children-first and
 * then, if the node is KNOWN, returns a LoadConst — DISCARDING the node and its whole
 * subtree. So an occurrence under a to-be-substituted ancestor is not a use: it is
 * about to cease to exist. This is load-bearing, not a nicety. Every source-level
 * `o.f` carries an NPE guard whose condition `Eq(LoadLocal(o), LoadNull)` is already
 * proven KNOWN-false by pts; enumerating occurrences off the raw def-use edges would
 * see a live REF-COMPARE on every object and decline every site — a consumer that
 * fires on nothing while looking correct. cp_const_subst_applies is therefore THE
 * shared predicate: the sweep stops descending exactly where the rewrite stops
 * keeping. */
typedef enum {
    /* Recognized — the rewrite removes these. */
    CP_SR_DEF = 0,     /* the New itself, as a StoreLocal's value: the def site */
    CP_SR_GET,         /* GetField.obj,  pts == {O} exactly */
    CP_SR_PUT,         /* PutField.obj,  pts == {O} exactly */
    CP_SR_HDR,         /* SetHeader.obj, pts == {O} exactly */
    CP_SR_CTOR,        /* receiver of the object's own PROVABLY-NO-OP ctor call (spec §6.1's
                        * four-condition predicate, cp_sr_ctor_droppable) — the rewrite drops
                        * the call with the allocation */
    CP_SR_COPY,        /* the ref stored into a LOCAL — a copy, not an observation */
    /* Everything below DISQUALIFIES, and is counted so the decline has a reason. */
    CP_SR_D_MULTI,     /* a recognized position, but pts is not exactly {O} */
    CP_SR_D_CALL,      /* receiver or argument of an Invoke */
    CP_SR_D_TYPECHK,   /* InstanceOf / CheckCast */
    CP_SR_D_CMP,       /* a comparison — ref identity is observable */
    CP_SR_D_ARRAY,     /* ArrayLoad / ArrayStore / ArrayLength / ArrayCopy */
    CP_SR_D_STOREHEAP, /* the ref stored into the HEAP (a field, a static, an array) */
    CP_SR_D_EXIT,      /* Return / Throw */
    CP_SR_D_OTHER,     /* any parent we do not name — fail-closed */
    CP_SR_POS_COUNT
} cp_sr_pos_t;

static const char* const cp_sr_pos_name[CP_SR_POS_COUNT] = {
    "def", "get", "put", "hdr", "ctor", "copy",
    "MULTI", "CALL", "TYPECHK", "CMP", "ARRAY", "STOREHEAP", "EXIT", "OTHER"
};

/* The parent decides. `exact` = the occurrence's pts is exactly {O}; `is_alloc` = the
 * occurrence IS O's allocation node. Fail-closed default. */
static cp_sr_pos_t cp_sr_classify(const sir_node_t* parent, const sir_node_t* e,
                                  bool exact, bool is_alloc) {
    switch (parent->tag) {
        case SIR_GETFIELD:
            if (parent->get_field.obj == e) return exact ? CP_SR_GET : CP_SR_D_MULTI;
            return CP_SR_D_OTHER;
        case SIR_PUTFIELD:
            if (parent->put_field.obj == e)   return exact ? CP_SR_PUT : CP_SR_D_MULTI;
            if (parent->put_field.value == e) return CP_SR_D_STOREHEAP;
            return CP_SR_D_OTHER;
        case SIR_SETHEADER:
            if (parent->set_header.obj == e)   return exact ? CP_SR_HDR : CP_SR_D_MULTI;
            if (parent->set_header.value == e) return CP_SR_D_STOREHEAP;
            return CP_SR_D_OTHER;
        /* The def site is a QUALIFICATION condition (the default-inits re-tag this
         * node), so the New must BE a StoreLocal's value. A New anywhere else — under
         * an ExprEffect, nested as a receiver — has no def site to re-tag, and looking
         * for "the spine node containing this expression" is the banned parent index.
         * It declines instead.
         *
         * Any OTHER StoreLocal of the ref is a COPY between locals, and a copy is not
         * an observation: a local is method-private (it is why the escape lattice's own
         * heap rule confers nothing on a store into a non-escaped local). Every READ of
         * that local is a LoadLocal whose pts is still {O} and whose parent is
         * classified here in its own right, so nothing slips through — and once the
         * field ops are rewritten, the copy's slot has no readers and DSE removes it. */
        case SIR_STORELOCAL:
            if (parent->store_local.value != e) return CP_SR_D_OTHER;
            return is_alloc ? CP_SR_DEF : (exact ? CP_SR_COPY : CP_SR_D_MULTI);
        case SIR_PUTSTATIC:      return CP_SR_D_STOREHEAP;
        case SIR_ARRAYLOAD:  case SIR_ARRAYSTORE:
        case SIR_ARRAYLENGTH:case SIR_ARRAYCOPY:   return CP_SR_D_ARRAY;
        case SIR_INSTANCEOF: case SIR_CHECKCAST:   return CP_SR_D_TYPECHK;
        case SIR_INVOKEVIRTUAL: case SIR_INVOKESPECIAL:
        case SIR_INVOKESTATIC:  case SIR_INVOKEINTERFACE: return CP_SR_D_CALL;
        case SIR_RETURN: case SIR_THROW:           return CP_SR_D_EXIT;
        SIR_CMP_CASES                              return CP_SR_D_CMP;
        default:                                   return CP_SR_D_OTHER;
    }
}

/* The slot a qualified site's field lives in. Keyed by the field's IDENTITY —
 * (owner class_id, field_idx) — which is exactly what GetField/PutField carry. */
typedef struct { int obj, class_id, field_idx, slot; } cp_sr_slot_t;

static int cp_sr_slot_of(const cp_sr_slot_t* rows, int n, int obj, int cid, int fi) {
    for (int i = 0; i < n; i++)
        if (rows[i].obj == obj && rows[i].class_id == cid && rows[i].field_idx == fi)
            return rows[i].slot;
    return -1;
}

/* Spec §6.1 — a scalar-replaced object is TORN APART into per-field slots, so its constructor
 * has no `this` and no heap cell to write: its effect is its FIELD INITIALIZERS, materialized
 * onto those slots. A synthesized-default ctor (JLS §8.8.9) is `super();` then the class's
 * instance-field initializers in textual order (§12.5); the DDCG compiles that to a straight
 * line of `StoreLocal(t, E)` (its own temps) and `PutField(this, f, E)`. We INLINE that line:
 * `this.f = E` → `StoreLocal(slot_f, E')`, the ctor's own temps → fresh slots past
 * method->max_locals, and inside E' every `this.g` read → LoadLocal(slot_g) and every temp
 * read `LoadLocal(t)` → `LoadLocal(base+t)`. One traversal (the ctor's own spine, via the
 * sanctioned collector), driven by the ctor call node — no walker over the caller, no
 * dominator. The functions are the ONE source used by both the qualifier (does the object
 * qualify?) and the rewrite (emit the stores), so they cannot diverge. The $ensure_init
 * barrier is a SEPARATE ExprEffect (DDCG-emitted outside the ctor call) — JLS §12.4.1 init is
 * untouched. */

/* `this` inside a method body has TWO forms: `LoadLocal(0)` (the DDCG's synthesized field-init
 * stores) and `LoadThis` (user-written ctor bodies). Both denote the receiver — the same duality
 * the §7 summary had to unify. Every materialization check keys off THIS, never one form. */
static bool cp_is_this(const sir_node_t* e) {
    return e && ((e->tag == SIR_LOADLOCAL && e->load_local.slot == 0) || e->tag == SIR_LOADTHIS);
}

/* Is E a value we can lift onto a slot: a constant, a ctor temp read (rebased), a read of one
 * of `this`'s own fields (→ a slot load), or a pure function of such (γ's
 * is_pure_if_children_pure — arithmetic / conversions, no throw / read / alloc)? Fail-closed on
 * anything else — a call, an allocation, a div/rem (throws), a bare `this` — so the object
 * simply materializes instead. */
static bool cp_init_expr_ok(const sir_node_t* e) {
    if (!e) return false;
    switch (e->tag) {
        case SIR_LOADCONST: case SIR_LOADLONGCONST: case SIR_LOADFLOATCONST:
        case SIR_LOADDOUBLECONST: case SIR_LOADNULL:
            return true;
        case SIR_LOADLOCAL:                    /* a ctor temp (rebased); bare `this` (slot 0) is not */
            return e->load_local.slot != 0;
        case SIR_GETFIELD:                     /* only this.g — becomes a slot load */
            return cp_is_this(e->get_field.obj);
        default: break;
    }
    int ar = sir_arity(e);
    if (ar == 0 || !sir_op_gamma[e->tag].is_pure_if_children_pure) return false;
    for (int i = 0; i < ar; i++)
        if (!cp_init_expr_ok(sir_child(e, i))) return false;
    return true;
}

/* `n`'s wrapped invoke, iff it is a chained constructor call (the ctor's own super()/this()). */
static const sir_node_t* cp_effect_ctor_call(cp_engine_t* eng, const sir_node_t* n) {
    if (!n || n->tag != SIR_EXPREFFECT) return NULL;
    const sir_node_t* v = n->expr_effect.value;
    if (!v || v->tag != SIR_INVOKESPECIAL) return NULL;
    const sema_class_t* sc = sema_get_class(eng->sema, v->invoke_special.class_id);
    int mi = v->invoke_special.method_idx;
    if (!sc || mi < 0 || mi >= (int)bbq_vec_len((void*)sc->methods)) return NULL;
    return sc->methods[mi].is_constructor ? v : NULL;
}

/* Deep-copy E, rebasing the ctor's own locals: `LoadLocal(s≥1)` → `LoadLocal(base+s)` and
 * `this.g` reads → `LoadLocal(slot_g)`. Seed the memo for those leaves, then sir_node_copy. */
static void cp_clone_seed(cp_engine_t* eng, sir_node_t* e, sir_copy_memo* memo, int base,
                          const cp_sr_slot_t* rows, int nrows, int o) {
    if (!e || sir_copy_memo_get(memo, e)) return;
    if (e->tag == SIR_LOADLOCAL && e->load_local.slot != 0) {
        sir_copy_memo_put(memo, e, sir_load_local(eng->arena, base + e->load_local.slot,
                                                  e->load_local.data_type, e->load_local.ref_type));
        return;
    }
    if (e->tag == SIR_GETFIELD && cp_is_this(e->get_field.obj)) {
        int slot = cp_sr_slot_of(rows, nrows, o, e->get_field.class_id, e->get_field.field_idx);
        if (slot >= 0) {
            const sema_class_t* sc = sema_get_class(eng->sema, e->get_field.class_id);
            sir_datatype_t dt = e->get_field.data_type;
            sir_node_t* ref = (dt == SIR_DTREF && sc)
                ? sir_ref_descriptor(eng->arena, sc->fields[e->get_field.field_idx].type) : NULL;
            sir_copy_memo_put(memo, e, sir_load_local(eng->arena, slot, dt, ref));
            return;                                      /* subtree replaced — do not recurse */
        }
    }
    for (int i = 0; i < sir_arity(e); i++)
        cp_clone_seed(eng, sir_child(e, i), memo, base, rows, nrows, o);
}

static sir_node_t* cp_clone_rebased(cp_engine_t* eng, sir_node_t* e, int base,
                                    const cp_sr_slot_t* rows, int nrows, int o) {
    sir_copy_memo memo = { NULL };
    cp_clone_seed(eng, e, &memo, base, rows, nrows, o);
    sir_node_t* copy = sir_node_copy(eng->arena, &memo, e);
    sir_copy_memo_dispose(&memo);
    return copy;
}

/* Materialize the ctor chain rooted at `ctor` (JLS §12.5 order, super's inits first) onto the
 * torn-apart object's slots. Synthesized-default OR user-written — the only requirement is a
 * straight line of liftable stores. The ctor's own locals rebase to fresh slots past
 * method->max_locals; its PARAMETERS bind to the call's actual arguments (`call_args`),
 * evaluated ONCE into the rebased param slots. A `super(args)`/`this(args)` with actual
 * arguments is declined (its own param binding is a further step). With `out` non-NULL:
 * appends the inlined StoreLocal chain (forward order, .next unset — threaded by the caller)
 * and bumps method->max_locals. With `out`/`method` NULL: validates only. False ⟹ the object
 * materializes. */
static bool cp_ctor_emit(cp_engine_t* eng, const sir_method_t* ctor,
                         sir_node_t** call_args, int call_argc,
                         const cp_sr_slot_t* rows, int nrows, int o,
                         sir_method_t* method, sir_node_t*** out) {
    if (!ctor || !ctor->entry) return false;
    const sema_class_t* sc = sema_get_class(eng->sema, ctor->class_id);
    if (!sc || ctor->method_id < 0 || ctor->method_id >= (int)bbq_vec_len((void*)sc->methods)
        || !sc->methods[ctor->method_id].is_constructor) return false;
    const sema_method_t* cm = &sc->methods[ctor->method_id];
    int pcount = cm->param_count;
    int base = method ? method->max_locals : 0;
    if (method && ctor->max_locals > 0) method->max_locals += ctor->max_locals;
    /* Bind params first: `StoreLocal(base + 1 + i, arg_i)` — slot 1+i is param i (this = slot 0),
     * typed by the declared param type; evaluated once, then the rebased param reads
     * (LoadLocal(base+1+i)) pick them up. */
    if (out && call_args && cm->param_types)
        for (int i = 0; i < pcount && i < call_argc; i++) {
            java_type_t pt = cm->param_types[i];
            sir_datatype_t pdt = lat_tag_to_dt(pt.tag);
            bbq_vec_push(*out, sir_store_local(eng->arena, base + 1 + i, pdt,
                                               sir_ref_descriptor(eng->arena, pt),
                                               call_args[i], NULL));
        }
    sir_node_t** spine = sir_collect_spine(ctor->entry);
    bool ok = true;
    for (int i = 0; i < (int)bbq_vec_len(spine) && ok; i++) {
        sir_node_t* n = spine[i];
        const sir_node_t* superc = cp_effect_ctor_call(eng, n);
        if (superc) {                                    /* super()/this() — recurse FIRST */
            if (superc->invoke_special.class_id == sema_object_id(eng->sema)) continue;
            if (superc->invoke_special.args_count > 0) { ok = false; break; }  /* super(args): decline */
            int gi = compiler_method_index(eng->ctx, superc->invoke_special.class_id,
                                           superc->invoke_special.method_idx);
            const sir_method_t* sm = (gi >= 0 && eng->ctx->methods) ? eng->ctx->methods[gi] : NULL;
            if (!cp_ctor_emit(eng, sm, NULL, 0, rows, nrows, o, method, out)) ok = false;
            continue;
        }
        switch (n->tag) {
            case SIR_STORELOCAL: {                       /* a ctor local / temp def, rebased */
                int s = n->store_local.slot;
                if (s == 0 || !cp_init_expr_ok(n->store_local.value)) { ok = false; break; }
                if (out)
                    bbq_vec_push(*out, sir_store_local(eng->arena, base + s,
                        n->store_local.data_type, n->store_local.ref_type,
                        cp_clone_rebased(eng, n->store_local.value, base, rows, nrows, o), NULL));
                break;
            }
            case SIR_PUTFIELD: {                         /* this.f = E  →  StoreLocal(slot_f, E') */
                if (!cp_is_this(n->put_field.obj)
                    || !cp_init_expr_ok(n->put_field.value)) { ok = false; break; }
                if (out) {
                    int slot = cp_sr_slot_of(rows, nrows, o, n->put_field.class_id,
                                             n->put_field.field_idx);
                    if (slot < 0) { ok = false; break; }
                    sir_datatype_t dt = n->put_field.data_type;
                    const sema_class_t* fc = sema_get_class(eng->sema, n->put_field.class_id);
                    sir_node_t* ref = (dt == SIR_DTREF && fc)
                        ? sir_ref_descriptor(eng->arena, fc->fields[n->put_field.field_idx].type)
                        : NULL;
                    sir_node_t* v = sir_narrow_to_storage(eng->arena, dt,
                        cp_clone_rebased(eng, n->put_field.value, base, rows, nrows, o));
                    bbq_vec_push(*out, sir_store_local(eng->arena, slot, dt, ref, v, NULL));
                }
                break;
            }
            case SIR_NOP: case SIR_RETURN: case SIR_RETURNVOID:
                break;
            default:                                     /* a branch / throw / other call */
                ok = false;
                break;
        }
    }
    bbq_vec_free(spine);
    return ok;
}

/* Qualifier side of the ONE source: the object's ctor is materializable iff its summary is
 * CLEAN + computed and its whole synthesized-default chain is a straight line of liftable
 * stores. */
static bool cp_sr_ctor_materializable(cp_engine_t* eng, const sir_node_t* call) {
    if (!eng->ctx || !eng->sema || !call || call->tag != SIR_INVOKESPECIAL) return false;
    int gi = compiler_method_index(eng->ctx, call->invoke_special.class_id,
                                   call->invoke_special.method_idx);
    const compiler_summary_t* sm = compiler_method_summary(eng->ctx, gi);
    if (!sm || sm->this_escape != COMPILER_ESC_NONE) return false;    /* CLEAN + summary exists */
    const sir_method_t* ctor = (gi >= 0 && eng->ctx->methods) ? eng->ctx->methods[gi] : NULL;
    return cp_ctor_emit(eng, ctor, NULL, 0, NULL, 0, -1, NULL, NULL); /* validate only */
}

/* One EDGE, classified: `child` sits in a slot of `parent`, and `ci` is child's vnode.
 * Also answers a question the census got WRONG (fixed 07-14): is this site's allocation
 * actually IN the graph? `eng->obj_alloced[o]` is set when O's own New is met on a
 * surviving edge. A site in a proven-dead region (an eliminated guard's throw arm —
 * `new NullPointerException`) is never met, and it never escapes either, because a dead
 * region's values hold ∅ pts. It is NOT an allocation anyone can remove; guard
 * elimination already removed it. */
static void cp_sr_edge(cp_engine_t* eng, const sir_node_t* parent, const sir_node_t* child,
                       int ci, const int* obj_of_alloc, const bool* cand,
                       int* pos, bool* disq, int* alloc_uses) {
    /* WHICH site does this vnode ALLOCATE (not: point to)? Off `obj_of_alloc`, built
     * from `vnode_of_obj` — the documented inverse. NOT off `eng->obj_of_vnode`: that
     * array is sized to the vnode count AT ENUMERATION, and the solve has since grown
     * vnode_count with φs / Refines / opaques, so indexing it here reads past its end. */
    int self = obj_of_alloc[ci];
    if (self >= eng->obj_first_site) eng->obj_alloced[self] = true;

    cp_pts_t s = eng->vnodes[ci]->pts;
    int n = cp_pts_count(eng, s);
    if (n <= 0) return;
    /* The RECEIVER edge of a provably-no-op ctor call is not an observation — the
     * rewrite drops the call with the allocation. Exactness (n == 1) is load-bearing: with a
     * multi-object receiver the rewrite's cp_sr_recv_site would refuse the Nop while the
     * qualifier had excused the edge, and the kept call would run on a deleted allocation. */
    bool ctor_ok = parent->tag == SIR_INVOKESPECIAL && parent->invoke_special.obj == child
                   && n == 1 && cp_sr_ctor_materializable(eng, parent);
    for (int o = eng->obj_first_site; o < eng->obj_count; o++) {
        if (!cand[o] || !cp_pts_has(eng, s, o)) continue;
        bool is_alloc = (o == self);
        cp_sr_pos_t p = cp_sr_classify(parent, child, n == 1, is_alloc);
        if (p == CP_SR_D_CALL && ctor_ok) p = CP_SR_CTOR;
        pos[o * CP_SR_POS_COUNT + p]++;
        if (p > CP_SR_COPY) disq[o] = true;
        if (is_alloc) alloc_uses[o]++;
    }
}

/* Can every instance field of C (INCLUDING INHERITED) be given a slot the SIR can TYPE?
 *
 * The rewrite types each new slot with the field's ref descriptor. `sir_ref_descriptor`
 * names an OVERLAY (ClassRef | ArrayRef | PrimArray) and returns NULL for anything it
 * cannot name — notably the CONCRETE BACKING of an array overlay (`JT_ARRAY_RAW`), which
 * is `(array W)`, not a ref to a struct. A REF field the descriptor cannot name has no
 * nameable slot type, so the site DECLINES (fail-closed).
 *
 * This is what the array-wrapper overlays are: the jre's only ctor-less structs, whose
 * `data` field IS that backing. Splitting their fields also splits an ARRAY, which §1
 * forbids outright ("arrays monolithic — no fields to split"). Both roads lead here. */
static bool cp_sr_fields_nameable(cp_engine_t* eng, int class_id) {
    for (int c = class_id; c >= 0; ) {
        const sema_class_t* sc = sema_get_class(eng->sema, c);
        if (!sc) return false;                      /* unknown class: fail closed */
        for (int f = 0; f < (int)bbq_vec_len((void*)sc->fields); f++) {
            if (sc->fields[f].modifiers & ACC_STATIC) continue;
            java_type_t ft = sc->fields[f].type;
            if (lat_tag_to_dt(ft.tag) != SIR_DTREF) continue;   /* primitives are fine */
            if (!sir_ref_descriptor(eng->arena, ft)) return false;
        }
        c = sc->super_id;
    }
    return true;
}

/* Qualify every NoEscape struct site in this method. Returns the number qualified;
 * `pos`/`disq` carry the per-site evidence for the probe (JAVELINA_SCALAR_CENSUS=1). */
static int cp_scalar_qualify(cp_engine_t* eng, bool* cand, int* pos, bool* disq) {
    int oc = eng->obj_count;
    int vc = eng->vnode_count;
    int* alloc_uses = (int*)bbq_arena_alloc(eng->arena, (size_t)(oc ? oc : 1) * sizeof(int));
    memset(alloc_uses, 0, (size_t)(oc ? oc : 1) * sizeof(int));
    eng->obj_alloced = (bool*)bbq_arena_alloc(eng->arena, (size_t)(oc ? oc : 1) * sizeof(bool));
    memset(eng->obj_alloced, 0, (size_t)(oc ? oc : 1) * sizeof(bool));

    /* vnode → the site it ALLOCATES, sized to the CURRENT vnode count. */
    int* obj_of_alloc = (int*)bbq_arena_alloc(eng->arena, (size_t)(vc ? vc : 1) * sizeof(int));
    for (int v = 0; v < vc; v++) obj_of_alloc[v] = -1;
    for (int o = eng->obj_first_site; o < oc; o++) {
        int vn = eng->vnode_of_obj[o];
        if (vn >= 0 && vn < vc) obj_of_alloc[vn] = o;
    }

    for (int o = 0; o < oc; o++) {
        cand[o] = false;
        disq[o] = false;
        if (o < eng->obj_first_site) continue;
        if (cp_escape_of(eng, o) != CP_ESC_NONE) continue;
        /* §2: a SUMMARY site (the recorded ALLOC fact says it can run more than
         * once) is declined in v1 — one set of slots cannot stand for objects the
         * site mints on different iterations. */
        if (!cp_obj_is_concrete(eng, o)) continue;
        int vn = eng->vnode_of_obj[o];
        if (vn < 0 || !eng->vnodes[vn]->expr) continue;
        if (eng->vnodes[vn]->expr->tag != SIR_NEW) continue;   /* §1: an array's cell is
                                                                * monolithic — no fields */
        /* Every field must have a slot type the SIR can NAME, or the site declines. */
        if (!eng->sema
         || !cp_sr_fields_nameable(eng, eng->vnodes[vn]->expr->new_.class_id)) continue;
        cand[o] = true;
    }

    /* ONE LINEAR SWEEP over the edges the engine already has — no recursion, no
     * tree walker. Worklist reachability over the expression DAG: each vnode is pushed
     * at most once (`survives` is the visited flag), each edge classified exactly once
     * when its parent is expanded — O(edges), independent of sharing.
     *
     * Roots are each REACHABLE spine node's direct data children (the spine node is the
     * classification parent — StoreLocal / PutField / Branch …). The "stop where
     * the rewrite stops keeping": cp_rewrite_expr replaces a KNOWN node with a LoadConst
     * and discards its subtree, so a KNOWN child is neither classified (a constant has
     * no pts) nor expanded — the SAME shared predicate decides for sweep and rewrite. */
    bool* survives = (bool*)bbq_arena_alloc(eng->arena, (size_t)(vc ? vc : 1) * sizeof(bool));
    memset(survives, 0, (size_t)(vc ? vc : 1) * sizeof(bool));
    int* wl = (int*)bbq_arena_alloc(eng->arena, (size_t)(vc ? vc : 1) * sizeof(int));
    int wl_tail = 0;
    for (int i = 0; i < eng->spine_count; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];
        int a = sir_arity(n);
        for (int j = 0; j < a; j++) {
            sir_node_t* child = sir_child(n, j);
            if (!child) continue;
            int ci = cp_vnode_of(eng, child);
            if (ci < 0 || ci >= vc) continue;
            if (cp_const_subst_applies(eng, child)) continue;  /* discarded subtree */
            cp_sr_edge(eng, n, child, ci, obj_of_alloc, cand, pos, disq, alloc_uses);
            if (!survives[ci]) { survives[ci] = true; wl[wl_tail++] = ci; }
        }
    }
    for (int wl_head = 0; wl_head < wl_tail; wl_head++) {
        sir_node_t* e = eng->vnodes[wl[wl_head]]->expr;
        if (!e || eng->vnodes[wl[wl_head]]->kind != CP_VN_EXPR) continue;
        int a = sir_arity(e);
        for (int j = 0; j < a; j++) {
            sir_node_t* child = sir_child(e, j);
            if (!child) continue;
            int ci = cp_vnode_of(eng, child);
            if (ci < 0 || ci >= vc) continue;
            if (cp_const_subst_applies(eng, child)) continue;
            cp_sr_edge(eng, e, child, ci, obj_of_alloc, cand, pos, disq, alloc_uses);
            if (!survives[ci]) { survives[ci] = true; wl[wl_tail++] = ci; }
        }
    }

    int qualified = 0;
    for (int o = eng->obj_first_site; o < eng->obj_count; o++) {
        if (!cand[o]) continue;
        /* The New occurs EXACTLY ONCE, and that occurrence is the def. */
        if (alloc_uses[o] != 1 || pos[o * CP_SR_POS_COUNT + CP_SR_DEF] != 1)
            disq[o] = true;
        if (disq[o]) { cand[o] = false; continue; }
        qualified++;
    }
    return qualified;
}

static void cp_scalar_probe(cp_engine_t* eng,
                            const bool* cand, const int* pos, const bool* disq) {
    const sir_method_t* method = eng->method;
    for (int o = eng->obj_first_site; o < eng->obj_count; o++) {
        int vn = eng->vnode_of_obj[o];
        if (vn < 0 || !eng->vnodes[vn]->expr) continue;
        if (eng->vnodes[vn]->expr->tag != SIR_NEW) continue;
        if (cp_escape_of(eng, o) != CP_ESC_NONE) continue;
        if (!eng->obj_alloced[o]) continue;   /* a dead region's site: nothing to remove */
        int cls = eng->vnodes[vn]->expr->new_.class_id;
        const sema_class_t* sc = eng->sema ? sema_get_class(eng->sema, cls) : NULL;
        fprintf(stderr, "scalar-probe: %s obj%d %s%s %s",
                method && method->name ? method->name : "?", o,
                sc && sc->name ? sc->name : "?",
                cp_obj_is_concrete(eng, o) ? "" : " [SUMMARY]",
                cand[o] ? "QUALIFIED" : (disq[o] ? "declined" : "declined(summary)"));
        for (int p = 0; p < CP_SR_POS_COUNT; p++) {
            int c = pos[o * CP_SR_POS_COUNT + p];
            if (c) fprintf(stderr, " | %s %d", cp_sr_pos_name[p], c);
        }
        fprintf(stderr, "\n");
    }
}

/* ── §6's CONSUMER — scalar replacement: THE REWRITE ────────────────────────
 *
 * §6: "NoEscape ⟹ scalar-replace the struct.new — its fields become SSA values / LOCALS,
 * zero GC allocation." A post-solve REWRITE, which §9 permits ("a post-solve rewrite of
 * the graph … is fine; a post-solve PROOF is not") — the PROOF was the fixpoint's: the
 * escape lattice ran inside cp_solve and gated its exit.
 *
 * THERE IS NO φ STEP, and that is the whole reason this is cheap. §6 says the fields
 * become LOCALS: slots are mutable program state, and the spine's own order carries the
 * dataflow. The φs are the ANALYSIS's view of slots, and the SSA construction rebuilds
 * them from the RECORDED merges (§8) on the next run — including R.2b's handler merge, so
 * a field written in a try and read in the catch gets JLS §11.3.1's answer for free.
 * Nothing here places a φ, asks which store reaches a load, or wants a dominator. */

/* JLS §4.12.5's initial value for a field of this type. */
static sir_node_t* cp_sr_default(bbq_arena* a, sir_datatype_t dt) {
    switch (dt) {
        case SIR_DTREF:    return sir_load_null(a);
        case SIR_DTLONG:   return sir_load_long_const(a, 0);
        case SIR_DTFLOAT:  return sir_load_float_const(a, 0.0f);
        case SIR_DTDOUBLE: return sir_load_double_const(a, 0.0);
        case SIR_DTV128:   return sir_simd_const(a, 0, 0);   /* the default arm would
                                                              * build an i32 zero tagged
                                                              * v128 — a miscompile the
                                                              * first time PEA scalar-
                                                              * replaces a v128 field */
        default:           return sir_load_const(a, 0, dt);
    }
}

/* The receiver of a field op, as a qualified site — or -1. The test is the whitelist's:
 * pts is EXACTLY {O} (§2's strong-update singleton, and ⊥null is not in it). */
static int cp_sr_recv_site(cp_engine_t* eng, const bool* cand, sir_node_t* obj) {
    int vi = cp_vnode_of(eng, obj);
    if (vi < 0) return -1;
    cp_pts_t s = eng->vnodes[vi]->pts;
    if (cp_pts_count(eng, s) != 1) return -1;
    for (int o = eng->obj_first_site; o < eng->obj_count; o++)
        if (cand[o] && cp_pts_has(eng, s, o)) return o;
    return -1;
}

/* ── §6.1 (Stadler et al. §5.1–5.4) — cp_pea ────────────────────────────────────────
 *
 * The paper's contribution over whole-method escape: an object is VIRTUAL per BRANCH and
 * MATERIALIZES at the escape point ("perform optimizations such as Scalar Replacement in
 * branches where the object does not escape"). The engine translation keeps the paper's
 * shape with three simplifications, each grounded:
 *  - ALIASES (paper §5.1's map) = the SOLVED singleton pts: a use belongs to the site iff
 *    its pts is EXACTLY {O} (`cp_sr_recv_site`'s own rule). Non-exact ⟹ fail-closed.
 *  - FIELD φs (paper §5.3's Phi insertion) don't exist: §6's own design carries field
 *    values in SLOTS ("the fields become LOCALS… THERE IS NO φ STEP"), so a merge of
 *    virtual states costs nothing and a merged materialized value is the object's own
 *    original slot.
 *  - LOOPS (paper §5.4's speculative iteration) fall out of the iterated row sweeps: the
 *    per-row state only DESCENDS (unvisited → virtual → escaped), so the sweep fixpoint
 *    IS the paper's stabilized speculation. V1 additionally bans COPY uses (`q = e`) —
 *    the one-slot materialization carrier depends on the object having ONE name — and
 *    that ban also closes the loop-identity hazard (a 1-limited site whose ref crosses
 *    the back edge could alias two runtime objects; without copies the ref cannot).
 *
 * Analyze-then-apply per Click ch.2 §2.3: the rows are PURE state iterated to their
 * fixpoint; graph edits (splices, retags) happen only afterwards, from converged rows —
 * a bottom-up application whose every step has its rule. THE struct pass: the
 * whole-method NoEscape site is the nesc == 0 case of the same machinery. */
typedef struct {
    int obj;                 /* the site */
    int class_id;
    int def_row;             /* spine idx of `StoreLocal(e_slot, New)` */
    sir_node_t* def_store;   /* that node — e_slot + ref_type reused as the carrier */
    sir_node_t* hdr;         /* the SetHeader node (its value expr is replayed) */
    bool fatal;              /* a non-sinkable position seen ⟹ not a candidate */
    int  fatal_pos;          /* probe: the cp_sr_pos_t (or -1) that declined it */
    int  fatal_tag;          /* probe: the parent node tag at the decline */
    int  nesc;               /* sinkable escaping uses seen */
    int  carrier;            /* fresh appended slot the materialization stores through */
    bool summary;            /* multi-visit site (in-loop alloc): §5.4 per-iteration */
} cp_pea_cand_t;

#define CP_PEA_UNSEEN  0
#define CP_PEA_VIRTUAL 1
#define CP_PEA_ESCAPED 2

/* Write `to` in place of successor `oldn` on `p` — the recorded edge, no traversal.
 * Returns false for a spine tag whose successor arm is not named here (fail-closed:
 * the caller then declines the site rather than splicing blind). */
static bool cp_pea_set_succ(sir_node_t* p, sir_node_t* oldn, sir_node_t* to) {
    switch (p->tag) {
        case SIR_NOP:        if (p->nop.next == oldn)         { p->nop.next = to;         return true; } break;
        case SIR_STORELOCAL: if (p->store_local.next == oldn) { p->store_local.next = to; return true; } break;
        case SIR_PUTFIELD:   if (p->put_field.next == oldn)   { p->put_field.next = to;   return true; } break;
        case SIR_PUTSTATIC:  if (p->put_static.next == oldn)  { p->put_static.next = to;  return true; } break;
        case SIR_SETHEADER:  if (p->set_header.next == oldn)  { p->set_header.next = to;  return true; } break;
        case SIR_EXPREFFECT: if (p->expr_effect.next == oldn) { p->expr_effect.next = to; return true; } break;
        case SIR_INC:        if (p->inc.next == oldn)         { p->inc.next = to;         return true; } break;
        case SIR_ARRAYSTORE: if (p->array_store.next == oldn) { p->array_store.next = to; return true; } break;
        case SIR_BRANCH:
            if (p->branch.on_true == oldn)  { p->branch.on_true = to;  return true; }
            if (p->branch.on_false == oldn) { p->branch.on_false = to; return true; }
            break;
        default: break;
    }
    return false;
}

/* The materialization chain for candidate `pc` (paper §5.2's Materialize): allocate,
 * replay the header, store every field slot back, and write EVERY must-alias slot
 * (paper §5.1's aliases — with copies, the object has several local names, and each
 * surviving read's slot is provably in the must-set, its exact pts excluding ⊥null).
 * Threaded onto `cont`; DSE cleans the aliases nothing reads. */
/* The materialization CARRIER is a FRESH slot appended past the original max_locals —
 * the engine's liveness/DSE guard `s >= slot_count` makes it untouchable, which is the
 * point: the spliced chain's reads are invisible to the spine-sized liveness, and a
 * carrier in an OLD slot had its (visible) def store DSE'd out from under the chain.
 * The old alias slots are then written FROM the carrier; their own readers are visible
 * and keep them honest. `carrier` is allocated once per candidate by the caller. */
static sir_node_t* cp_pea_materialize(cp_engine_t* eng, const cp_pea_cand_t* pc,
                                      int carrier,
                                      const cp_sr_slot_t* rows, int nrows,
                                      const uint64_t* alias, int aw,
                                      sir_node_t* cont) {
    bbq_arena* a = eng->arena;
    sir_node_t* eref = pc->def_store->store_local.ref_type;
    sir_node_t* head = cont;
    /* Alias copies LAST (they read the carrier). */
    for (int w = aw - 1; w >= 0; w--) {
        uint64_t word = alias[w];
        while (word) {
            int b = 63 - __builtin_clzll(word);
            word &= ~((uint64_t)1 << b);
            int q = (w << 6) + b;
            if (q >= eng->slot_count) continue;
            head = sir_store_local(a, q, SIR_DTREF, eref,
                                   sir_load_local(a, carrier, SIR_DTREF, eref), head);
        }
    }
    for (int r = nrows - 1; r >= 0; r--) {
        if (rows[r].obj != pc->obj) continue;
        const sema_class_t* sc = sema_get_class(eng->sema, rows[r].class_id);
        if (!sc) continue;
        java_type_t ft = sc->fields[rows[r].field_idx].type;
        sir_datatype_t dt = lat_tag_to_dt(ft.tag);
        head = sir_put_field(a, dt,
                             sir_load_local(a, carrier, SIR_DTREF, eref),
                             rows[r].class_id, rows[r].field_idx,
                             sir_load_local(a, rows[r].slot, dt, sir_ref_descriptor(a, ft)),
                             head);
    }
    if (pc->hdr)
        head = sir_set_header(a, sir_load_local(a, carrier, SIR_DTREF, eref),
                              pc->hdr->set_header.value,
                              pc->hdr->set_header.struct_class_id, head);
    return sir_store_local(a, carrier, SIR_DTREF, eref,
                           sir_new(a, pc->class_id), head);
}

/* Is spine node `n` a COPY of candidate `pc` (StoreLocal whose value's pts is exactly
 * the site)? Returns the target slot, or -1. */
static int cp_pea_copy_slot(cp_engine_t* eng, const cp_pea_cand_t* pc, sir_node_t* n) {
    if (n->tag != SIR_STORELOCAL || !n->store_local.value) return -1;
    sir_node_t* v = n->store_local.value;
    if (v->tag != SIR_LOADLOCAL) return -1;
    int vi = cp_vnode_of(eng, v);
    if (vi < 0) return -1;
    cp_pts_t p = eng->vnodes[vi]->pts;
    if (cp_pts_count(eng, p) != 1 || !cp_pts_has(eng, p, pc->obj)) return -1;
    return n->store_local.slot;
}

static void cp_pea(cp_engine_t* eng, const bool* sr_cand) {
    /* No ctx (a unit-built engine) is NOT a decline: only the ctor REPLAY needs it,
     * and cp_sr_ctor_materializable already fails closed without it — a candidate
     * with a ctor then classifies D_OTHER and declines by itself. */
    if (!eng->sema) return;
    /* Synthetic methods ($newInstance / $ensure_init …) are HAND-BUILT SIR without the
     * full DDCG threading (ref descriptors on every destination) the splices re-emit.
     * Fail-closed: partial escape is for compiled source. */
    if (eng->method && eng->method->name && eng->method->name[0] == '$') return;
    int oc = eng->obj_count, vc = eng->vnode_count, sn = eng->spine_count;
    if (oc <= eng->obj_first_site || sn == 0) return;
    bbq_arena* a = eng->arena;

    /* ── Candidates: cp_scalar_qualify's gates MINUS the whole-method escape gate. ── */
    cp_pea_cand_t* pcs = NULL;
    int* pea_of_obj = (int*)bbq_arena_alloc(a, (size_t)oc * sizeof(int));
    for (int o = 0; o < oc; o++) pea_of_obj[o] = -1;
    for (int o = eng->obj_first_site; o < oc; o++) {
        if (sr_cand[o]) continue;                      /* whole-method replacement took it */
        int vn = eng->vnode_of_obj[o];
        if (vn < 0 || !eng->vnodes[vn]->expr) continue;
        if (eng->vnodes[vn]->expr->tag != SIR_NEW) continue;
        if (!cp_sr_fields_nameable(eng, eng->vnodes[vn]->expr->new_.class_id)) continue;
        cp_pea_cand_t pc; memset(&pc, 0, sizeof pc);
        pc.obj = o; pc.class_id = eng->vnodes[vn]->expr->new_.class_id;
        pc.summary = !cp_obj_is_concrete(eng, o);      /* §5.4: per-visit, gated below */
        pc.def_row = -1;
        pea_of_obj[o] = (int)bbq_vec_len(pcs);
        bbq_vec_push(pcs, pc);
    }
    int np = (int)bbq_vec_len(pcs);
    if (np == 0) { bbq_vec_free(pcs); return; }

    /* ── The use sweep (qualify's own structure: spine children + surviving expression
     * edges, the shared subst predicate deciding descent). Each surviving use of a
     * candidate classifies via THE authority (cp_sr_classify); handled kinds pass,
     * D_STOREHEAP/D_CALL/D_EXIT are SINKABLE (row marked), everything else is fatal. */
    unsigned char** esc_row = (unsigned char**)bbq_arena_alloc(a, (size_t)sn * sizeof(void*));
    for (int i = 0; i < sn; i++) {
        esc_row[i] = (unsigned char*)bbq_arena_alloc(a, (size_t)np);
        memset(esc_row[i], 0, (size_t)np);
    }
    /* The OWNING spine row of each surviving expression vnode (parent_spine exists only
     * on LoadLocals). First-wins; a vnode reached from TWO rows cannot be attributed —
     * classification then declines the candidate (fail-closed via row = -1). */
    int* vrow = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
    for (int v = 0; v < vc; v++) vrow[v] = -1;
    bool* vrow_conflict = (bool*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(bool));
    memset(vrow_conflict, 0, (size_t)(vc ? vc : 1) * sizeof(bool));
    {
        int* obj_of_alloc = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
        for (int v = 0; v < vc; v++) obj_of_alloc[v] = -1;
        for (int o = eng->obj_first_site; o < oc; o++) {
            int vn = eng->vnode_of_obj[o];
            if (vn >= 0 && vn < vc) obj_of_alloc[vn] = o;
        }
        bool* survives = (bool*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(bool));
        memset(survives, 0, (size_t)(vc ? vc : 1) * sizeof(bool));
        int* wl = (int*)bbq_arena_alloc(a, (size_t)(vc ? vc : 1) * sizeof(int));
        int wl_tail = 0;
        /* Classify one (parent, child) pair against every candidate the child names. */
        #define CP_PEA_CLASSIFY(par, ch, ci, row) do {                                      \
            cp_pts_t _p = eng->vnodes[ci]->pts;                                             \
            for (int _k = 0; _k < np; _k++) {                                               \
                cp_pea_cand_t* _pc = &pcs[_k];                                              \
                if (_pc->fatal) continue;                                                   \
                if (!cp_pts_has(eng, _p, _pc->obj)) continue;                               \
                bool _exact = (cp_pts_count(eng, _p) == 1);                                 \
                bool _isalloc = (obj_of_alloc[ci] == _pc->obj);                             \
                cp_sr_pos_t _r = cp_sr_classify((par), (ch), _exact, _isalloc);             \
                /* Mirrors cp_sr_edge: the receiver of the object's OWN materializable \
                 * ctor is not an observation — the ctor's field inits replay onto slots. */ \
                if (_r == CP_SR_D_CALL && (par)->tag == SIR_INVOKESPECIAL                   \
                        && (par)->invoke_special.obj == (ch)) {                             \
                    /* The candidate's OWN ctor is CTOR (its field inits replay onto the     \
                     * slots) or FATAL — never a sinkable escape: virtualizing the def       \
                     * while the ctor writes a real object leaves the slots holding the      \
                     * §4.12.5 defaults (the src-1 slot-10 read-unwritten miscompile). */    \
                    _r = (_exact && cp_sr_ctor_materializable(eng, (par)))                  \
                        ? CP_SR_CTOR : CP_SR_D_OTHER;                                       \
                }                                                                           \
                switch (_r) {                                                               \
                    case CP_SR_DEF:                                                         \
                        if (_pc->def_row >= 0 || (par)->tag != SIR_STORELOCAL)              \
                            { _pc->fatal = true; _pc->fatal_pos = _r;                       \
                              _pc->fatal_tag = (int)(par)->tag; break; }                    \
                        _pc->def_row = (row); _pc->def_store = (sir_node_t*)(par); break;   \
                    case CP_SR_GET: case CP_SR_PUT: case CP_SR_CTOR: break;                 \
                    case CP_SR_HDR: _pc->hdr = (sir_node_t*)(par); break;                   \
                    case CP_SR_COPY: break;   /* an alias (§5.1) — the row aliases track it */ \
                    case CP_SR_D_STOREHEAP: case CP_SR_D_CALL: case CP_SR_D_EXIT:           \
                        if ((row) < 0) { _pc->fatal = true; _pc->fatal_pos = _r;            \
                                         _pc->fatal_tag = (int)(par)->tag; break; }        \
                        esc_row[(row)][_k] = 1; _pc->nesc++; break;                         \
                    default:                                                                \
                        _pc->fatal = true; _pc->fatal_pos = _r;                             \
                        _pc->fatal_tag = (int)(par)->tag; break;                            \
                }                                                                           \
            }                                                                               \
        } while (0)
        for (int i = 0; i < sn; i++) {
            if (!cp_spine_reachable(eng, i)) continue;
            sir_node_t* n = eng->spine[i];
            int ar = sir_arity(n);
            for (int j = 0; j < ar; j++) {
                sir_node_t* child = sir_child(n, j);
                if (!child) continue;
                int ci = cp_vnode_of(eng, child);
                if (ci < 0 || ci >= vc) continue;
                if (cp_const_subst_applies(eng, child)) continue;
                CP_PEA_CLASSIFY(n, child, ci, i);
                if (vrow[ci] < 0) vrow[ci] = i;
                else if (vrow[ci] != i) vrow_conflict[ci] = true;
                if (!survives[ci]) { survives[ci] = true; wl[wl_tail++] = ci; }
            }
        }
        for (int wh = 0; wh < wl_tail; wh++) {
            sir_node_t* pe = eng->vnodes[wl[wh]]->expr;
            if (!pe || eng->vnodes[wl[wh]]->kind != CP_VN_EXPR) continue;
            int prow = vrow_conflict[wl[wh]] ? -1 : vrow[wl[wh]];
            int ar = sir_arity(pe);
            for (int j = 0; j < ar; j++) {
                sir_node_t* child = sir_child(pe, j);
                if (!child) continue;
                int ci = cp_vnode_of(eng, child);
                if (ci < 0 || ci >= vc) continue;
                if (cp_const_subst_applies(eng, child)) continue;
                CP_PEA_CLASSIFY(pe, child, ci, prow);
                if (prow >= 0) {
                    if (vrow[ci] < 0) vrow[ci] = prow;
                    else if (vrow[ci] != prow) vrow_conflict[ci] = true;
                } else vrow_conflict[ci] = true;
                if (!survives[ci]) { survives[ci] = true; wl[wl_tail++] = ci; }
            }
        }
        #undef CP_PEA_CLASSIFY
    }
    /* A candidate must have a def with a TYPED carrier (the materialization re-threads
     * the slot's ref descriptor — a def store without one, e.g. a synthetic method's
     * hand-built SIR, cannot be re-typed: fail-closed), no fatal use, and a per-kind
     * escape count. Concrete: any — nesc == 0 is the whole-method NoEscape case
     * (full virtualization, cp_sr's struct parity), nesc ≥ 1 the partial one.
     * Summary (multi-visit): EXACTLY 0 — the def-row reset does not lower ESCAPED,
     * so an in-loop escape flows around the back edge and arrives back at the def as
     * ESCAPED, leaving the next visit's post-def rows reading real-object slots no
     * materialization on that visit wrote. With zero escape rows ESCAPED is
     * unreachable outright. */
    int live = 0;
    for (int k = 0; k < np; k++) {
        if (pcs[k].fatal || pcs[k].def_row < 0
                || (pcs[k].summary && pcs[k].nesc != 0)
                || !pcs[k].def_store || !pcs[k].def_store->store_local.ref_type)
            pcs[k].fatal = true;
        else live++;
    }
    if (getenv("JAVELINA_SCALAR_CENSUS"))
        for (int k = 0; k < np; k++)
            fprintf(stderr, "pea-probe: %s obj%d %s def=%d rt=%d nesc=%d fpos=%d ftag=%d\n",
                    eng->method && eng->method->name ? eng->method->name : "?",
                    pcs[k].obj, pcs[k].fatal ? "FATAL" : "live",
                    pcs[k].def_row,
                    (pcs[k].def_store && pcs[k].def_store->store_local.ref_type) ? 1 : 0,
                    pcs[k].nesc, pcs[k].fatal_pos, pcs[k].fatal_tag);
    if (live == 0) { bbq_vec_free(pcs); return; }

    /* ── Rows: forward sweeps to the fixpoint. Two coupled states per (row, cand):
     * the object state (UNSEEN→VIRTUAL→ESCAPED, ascends) and the MUST-alias slot set
     * (paper §5.1's aliases: starts FULL, grows on copies along a path, killed by
     * overwrites, INTERSECTED at merges — descends). Both monotone ⟹ terminates. ── */
    int *p_off, *p_cnt, *p_list;
    cp_build_pred_csr(eng, &p_off, &p_cnt, &p_list);
    int aw = (eng->slot_count + 63) >> 6;
    if (aw < 1) aw = 1;
    unsigned char** st = (unsigned char**)bbq_arena_alloc(a, (size_t)sn * sizeof(void*));
    uint64_t** al = (uint64_t**)bbq_arena_alloc(a, (size_t)sn * sizeof(void*));
    for (int i = 0; i < sn; i++) {
        st[i] = (unsigned char*)bbq_arena_alloc(a, (size_t)np);
        memset(st[i], CP_PEA_UNSEEN, (size_t)np);
        al[i] = (uint64_t*)bbq_arena_alloc(a, (size_t)np * aw * sizeof(uint64_t));
        memset(al[i], 0xFF, (size_t)np * aw * sizeof(uint64_t));   /* must-TOP: all */
    }
    /* OUT of node p for cand k — the shared transfer (sweep, feasibility, splice). */
    #define CP_PEA_OUT(p, k, out_st, out_al) do {                                        \
        (out_st) = st[(p)][k];                                                           \
        for (int _w = 0; _w < aw; _w++) (out_al)[_w] = al[(p)][(size_t)(k) * aw + _w];   \
        sir_node_t* _pn = eng->spine[(p)];                                               \
        if ((p) == pcs[k].def_row) {                                                     \
            if ((out_st) < CP_PEA_VIRTUAL) (out_st) = CP_PEA_VIRTUAL;                    \
            for (int _w = 0; _w < aw; _w++) (out_al)[_w] = 0;                            \
            int _s = pcs[k].def_store->store_local.slot;                                 \
            (out_al)[_s >> 6] |= (uint64_t)1 << (_s & 63);                               \
        } else {                                                                         \
            int _cs = cp_pea_copy_slot(eng, &pcs[k], _pn);                               \
            if (_cs >= 0) (out_al)[_cs >> 6] |= (uint64_t)1 << (_cs & 63);               \
            else if (_pn->tag == SIR_STORELOCAL)                                         \
                (out_al)[_pn->store_local.slot >> 6]                                     \
                    &= ~((uint64_t)1 << (_pn->store_local.slot & 63));                   \
            else if (_pn->tag == SIR_EXCEPTIONENTRY                                      \
                     && _pn->exception_entry.local_slot >= 0)                            \
                (out_al)[_pn->exception_entry.local_slot >> 6]                           \
                    &= ~((uint64_t)1 << (_pn->exception_entry.local_slot & 63));         \
        }                                                                                \
        if (esc_row[(p)][k] && (out_st) != CP_PEA_UNSEEN) (out_st) = CP_PEA_ESCAPED;     \
    } while (0)
    uint64_t* scratch = (uint64_t*)bbq_arena_alloc(a, (size_t)aw * sizeof(uint64_t));
    uint64_t* acc     = (uint64_t*)bbq_arena_alloc(a, (size_t)aw * sizeof(uint64_t));
    int max_sweeps = 2 * sn + 4;
    for (int sweep = 0; ; sweep++) {
        if (sweep >= max_sweeps) return;               /* cannot converge ⟹ do nothing */
        bool changed = false;
        for (int n = 0; n < sn; n++) {
            for (int k = 0; k < np; k++) {
                if (pcs[k].fatal) continue;
                unsigned char in = CP_PEA_UNSEEN;
                for (int w = 0; w < aw; w++) acc[w] = ~(uint64_t)0;
                bool any = false;
                for (int pi = 0; pi < p_cnt[n]; pi++) {
                    int p = p_list[p_off[n] + pi];
                    unsigned char pst; CP_PEA_OUT(p, k, pst, scratch);
                    if (pst == CP_PEA_UNSEEN && p != pcs[k].def_row) continue;
                    any = true;
                    if (pst > in) in = pst;
                    for (int w = 0; w < aw; w++) acc[w] &= scratch[w];
                }
                if (!any) continue;
                if (in > st[n][k]) { st[n][k] = in; changed = true; }
                uint64_t* row = &al[n][(size_t)k * aw];
                for (int w = 0; w < aw; w++) {
                    uint64_t nv = row[w] & acc[w];     /* must-sets only descend */
                    if (nv != row[w]) { row[w] = nv; changed = true; }
                }
            }
        }
        if (!changed) break;
    }

    /* ── Feasibility: every needed splice edge writable AND its alias set non-empty
     * (an unnameable materialization has nowhere to put the object). Fail-closed:
     * the site reverts wholesale. ── */
    for (int k = 0; k < np; k++) {
        if (pcs[k].fatal) continue;
        for (int n = 0; n < sn && !pcs[k].fatal; n++) {
            /* Use-site splices need a nameable IN set. */
            if (esc_row[n][k] && st[n][k] == CP_PEA_VIRTUAL) {
                uint64_t anyb = 0;
                for (int w = 0; w < aw; w++) anyb |= al[n][(size_t)k * aw + w];
                if (!anyb) { pcs[k].fatal = true; break; }
            }
            if (st[n][k] != CP_PEA_ESCAPED || p_cnt[n] < 2) continue;
            for (int pi = 0; pi < p_cnt[n]; pi++) {
                int p = p_list[p_off[n] + pi];
                unsigned char pst; CP_PEA_OUT(p, k, pst, scratch);
                if (pst != CP_PEA_VIRTUAL) continue;
                uint64_t anyb = 0;
                for (int w = 0; w < aw; w++) anyb |= scratch[w];
                sir_node_t tmp = *eng->spine[p];
                if (!anyb || !cp_pea_set_succ(&tmp, eng->spine[n], eng->spine[n]))
                    { pcs[k].fatal = true; break; }
            }
        }
    }

    /* ── Copy validation: a VIRTUAL-row copy will be NOPed, so wherever its target slot
     * is still LIVE at a materialization point it must be REWRITTEN there (∈ the
     * must-alias set) — the must-∩ legitimately under-approximates at merges, and a
     * dropped-but-live name would leave a NOPed writer with live readers (the
     * Integer.valueOf slot-3 shape). A DEAD name needs nothing (per-statement staging
     * temps overwritten later — the f/h shapes). A may-set write would CLOBBER on paths
     * where the slot holds another value — unsound. Fail-closed: the site declines. ── */
    cp_compute_liveness(eng);   /* pre-mutation SIR; cp_rewrite recomputes later anyway */
    for (int k = 0; k < np; k++) {
        if (pcs[k].fatal) continue;
        for (int i = 0; i < sn && !pcs[k].fatal; i++) {
            if (!cp_spine_reachable(eng, i)) continue;
            if (st[i][k] != CP_PEA_VIRTUAL || esc_row[i][k]) continue;
            int q = cp_pea_copy_slot(eng, &pcs[k], eng->spine[i]);
            if (q < 0) continue;
            /* Check q at every materialization point where it is still live. */
            for (int n = 0; n < sn && !pcs[k].fatal; n++) {
                if (esc_row[n][k] && st[n][k] == CP_PEA_VIRTUAL) {
                    if (eng->live_in && eng->live_in[n][q]
                            && !(al[n][(size_t)k * aw + (q >> 6)] & ((uint64_t)1 << (q & 63))))
                        pcs[k].fatal = true;
                }
                if (st[n][k] == CP_PEA_ESCAPED && p_cnt[n] >= 2) {
                    for (int pi = 0; pi < p_cnt[n] && !pcs[k].fatal; pi++) {
                        int p = p_list[p_off[n] + pi];
                        unsigned char pst; CP_PEA_OUT(p, k, pst, scratch);
                        if (pst != CP_PEA_VIRTUAL) continue;
                        if (eng->live_in && eng->live_in[n][q]
                                && !(scratch[q >> 6] & ((uint64_t)1 << (q & 63))))
                            pcs[k].fatal = true;
                    }
                }
            }
        }
    }

    /* ── The per-visit reset condition (§5.4): a summary site's def re-runs each
     * visit (LoadNull + §4.12.5 defaults + ctor replay re-initialize the slots), so
     * virtualization is sound iff NO name of the previous visit's object is live
     * entering the def — a ref surviving the back edge would read the next visit's
     * slots. Names are static: the def slot + every copy target (scanned without
     * the VIRTUAL-row filter — the fail-closed direction). Liveness absent ⟹
     * decline. ── */
    for (int k = 0; k < np; k++) {
        if (pcs[k].fatal || !pcs[k].summary) continue;
        if (!eng->live_in) { pcs[k].fatal = true; continue; }
        int dr = pcs[k].def_row;
        int ds = pcs[k].def_store->store_local.slot;
        if (ds < eng->slot_count && eng->live_in[dr][ds]) { pcs[k].fatal = true; continue; }
        for (int i = 0; i < sn && !pcs[k].fatal; i++) {
            if (!cp_spine_reachable(eng, i)) continue;
            int q = cp_pea_copy_slot(eng, &pcs[k], eng->spine[i]);
            if (q >= 0 && q < eng->slot_count && eng->live_in[dr][q])
                pcs[k].fatal = true;
        }
    }

    /* ── Slots for the live candidates (the cp_sr slot builder, same walk). ── */
    cp_sr_slot_t* rows = NULL;
    for (int k = 0; k < np; k++) {
        if (pcs[k].fatal) continue;
        pcs[k].carrier = eng->method->max_locals++;   /* the DSE-immune materialization slot */
        for (int c = pcs[k].class_id; c >= 0; ) {
            const sema_class_t* sc = sema_get_class(eng->sema, c);
            if (!sc) break;
            for (int f = 0; f < (int)bbq_vec_len((void*)sc->fields); f++) {
                if (sc->fields[f].modifiers & ACC_STATIC) continue;
                cp_sr_slot_t r = { pcs[k].obj, c, f, eng->method->max_locals++ };
                bbq_vec_push(rows, r);
            }
            c = sc->super_id;
        }
    }
    int nrows = (int)bbq_vec_len(rows);

    /* ── Apply, from converged rows only. ── */
    bool* pea_ok = (bool*)bbq_arena_alloc(a, (size_t)oc * sizeof(bool));
    memset(pea_ok, 0, (size_t)oc * sizeof(bool));
    for (int k = 0; k < np; k++) if (!pcs[k].fatal) pea_ok[pcs[k].obj] = true;

    /* Splices run over the WHOLE spine before any node is re-tagged: a splice finds
     * its pred's successor edge by pointer identity, so every pred must still carry
     * its original next when the splice looks at it. */
    for (int i = 0; i < sn; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];

        /* (a) Materialize BEFORE a sinkable escaping use reached VIRTUAL, writing the
         * row's must-alias slots. */
        for (int k = 0; k < np; k++) {
            if (pcs[k].fatal || !esc_row[i][k]) continue;
            if (st[i][k] != CP_PEA_VIRTUAL) continue;
            for (int pi = 0; pi < p_cnt[i]; pi++) {
                int p = p_list[p_off[i] + pi];
                unsigned char pst; CP_PEA_OUT(p, k, pst, scratch);
                if (pst == CP_PEA_UNSEEN) continue;
                if (getenv("JAVELINA_SCALAR_CENSUS"))
                    fprintf(stderr, "pea-mat: %s obj%d USE row=%d(tag %d) pred=%d eslot=%d al0=%llx\n",
                            eng->method->name ? eng->method->name : "?", pcs[k].obj, i,
                            (int)eng->spine[i]->tag, p,
                            pcs[k].def_store->store_local.slot,
                            (unsigned long long)al[i][(size_t)k * aw]);
                sir_node_t* chain = cp_pea_materialize(eng, &pcs[k], pcs[k].carrier,
                                                       rows, nrows,
                                                       &al[i][(size_t)k * aw], aw, n);
                cp_pea_set_succ(eng->spine[p], n, chain);
            }
        }
        /* (b) Mixed merges: materialize on the VIRTUAL pred edges, with THAT pred's
         * out-aliases (the edge's own names). */
        if (p_cnt[i] >= 2) {
            for (int k = 0; k < np; k++) {
                if (pcs[k].fatal || st[i][k] != CP_PEA_ESCAPED) continue;
                for (int pi = 0; pi < p_cnt[i]; pi++) {
                    int p = p_list[p_off[i] + pi];
                    unsigned char pst; CP_PEA_OUT(p, k, pst, scratch);
                    if (pst != CP_PEA_VIRTUAL) continue;
                    sir_node_t* chain = cp_pea_materialize(eng, &pcs[k], pcs[k].carrier,
                                                           rows, nrows,
                                                           scratch, aw, n);
                    cp_pea_set_succ(eng->spine[p], n, chain);
                }
            }
        }
    }

    for (int i = 0; i < sn; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];

        /* (c) VIRTUAL-row field ops re-tag exactly as whole-method replacement does. */
        if (n->tag == SIR_PUTFIELD) {
            int o = cp_sr_recv_site(eng, pea_ok, n->put_field.obj);
            int k = o >= 0 ? pea_of_obj[o] : -1;
            if (k >= 0 && st[i][k] == CP_PEA_VIRTUAL && !esc_row[i][k]) {
                int slot = cp_sr_slot_of(rows, nrows, o, n->put_field.class_id,
                                         n->put_field.field_idx);
                if (slot >= 0) {
                    sir_datatype_t dt = n->put_field.data_type;
                    sir_node_t* val  = sir_narrow_to_storage(a, dt, n->put_field.value);
                    sir_node_t* next = n->put_field.next;
                    const sema_class_t* sc = sema_get_class(eng->sema, n->put_field.class_id);
                    sir_node_t* ref = (dt == SIR_DTREF && sc)
                        ? sir_ref_descriptor(a, sc->fields[n->put_field.field_idx].type) : NULL;
                    n->tag = SIR_STORELOCAL;
                    n->store_local.slot      = slot;
                    n->store_local.data_type = dt;
                    n->store_local.ref_type  = ref;
                    n->store_local.value     = val;
                    n->store_local.next      = next;
                }
            }
        } else if (n->tag == SIR_SETHEADER) {
            int o = cp_sr_recv_site(eng, pea_ok, n->set_header.obj);
            int k = o >= 0 ? pea_of_obj[o] : -1;
            if (k >= 0 && st[i][k] == CP_PEA_VIRTUAL) {
                sir_node_t* next = n->set_header.next;
                n->tag = SIR_NOP;
                n->nop.next = next;
            }
        } else if (n->tag == SIR_EXPREFFECT && n->expr_effect.value
                   && n->expr_effect.value->tag == SIR_INVOKESPECIAL) {
            /* The object's own materializable ctor in a VIRTUAL row: replay its field
             * inits onto the slots (the cp_sr mechanism verbatim). */
            sir_node_t* eff = n->expr_effect.value;
            int o = cp_sr_recv_site(eng, pea_ok, eff->invoke_special.obj);
            int k = o >= 0 ? pea_of_obj[o] : -1;
            if (k >= 0 && st[i][k] == CP_PEA_VIRTUAL && !esc_row[i][k]
                    && cp_sr_ctor_materializable(eng, eff)) {
                int gi = compiler_method_index(eng->ctx, eff->invoke_special.class_id,
                                               eff->invoke_special.method_idx);
                const sir_method_t* ctor = eng->ctx->methods[gi];
                sir_node_t** stores = NULL;
                if (cp_ctor_emit(eng, ctor, eff->invoke_special.args,
                                 eff->invoke_special.args_count,
                                 rows, nrows, o, eng->method, &stores)) {
                    sir_node_t* head = n->expr_effect.next;
                    for (int r2 = (int)bbq_vec_len(stores) - 1; r2 >= 0; r2--) {
                        stores[r2]->store_local.next = head;
                        head = stores[r2];
                    }
                    n->tag = SIR_NOP;
                    n->nop.next = head;
                }
                bbq_vec_free(stores);
            }
        } else if (n->tag == SIR_STORELOCAL && n->store_local.value
                   && n->store_local.value->tag == SIR_LOADLOCAL) {
            /* A COPY of a virtual object carries no runtime value — NOP it; the alias
             * set remembers the name and materialization rewrites it (paper §5.1). */
            for (int k = 0; k < np; k++) {
                if (pcs[k].fatal) continue;
                if (st[i][k] != CP_PEA_VIRTUAL || esc_row[i][k]) continue;
                if (cp_pea_copy_slot(eng, &pcs[k], n) < 0) continue;
                sir_node_t* next = n->store_local.next;
                n->tag = SIR_NOP;
                n->nop.next = next;
                break;
            }
        } else if (n->tag == SIR_STORELOCAL && n->store_local.value
                   && n->store_local.value->tag == SIR_NEW) {
            int vi = cp_vnode_of(eng, n->store_local.value);
            int o = -1;
            for (int k2 = eng->obj_first_site; vi >= 0 && k2 < oc; k2++)
                if (pea_ok[k2] && eng->vnode_of_obj[k2] == vi) { o = k2; break; }
            if (o >= 0) {
                if (getenv("JAVELINA_SCALAR_CENSUS")) {
                    const sema_class_t* mc = eng->method->class_id >= 0
                        ? sema_get_class(eng->sema, eng->method->class_id) : NULL;
                    fprintf(stderr, "pea-def: %s.%s obj%d slot=%d node=%p cap=%p\n",
                            mc && mc->name ? mc->name : "?",
                            eng->method->name ? eng->method->name : "?",
                            o, n->store_local.slot, (void*)n,
                            (void*)(pea_of_obj[o] >= 0 ? pcs[pea_of_obj[o]].def_store : NULL));
                }
                /* The def: §4.12.5 default-init chain onto the field slots, and the
                 * object slot itself goes null until (unless) materialization. */
                sir_node_t* next = n->store_local.next;
                sir_node_t* head = next;
                for (int r = nrows - 1; r >= 0; r--) {
                    if (rows[r].obj != o) continue;
                    const sema_class_t* sc = sema_get_class(eng->sema, rows[r].class_id);
                    if (!sc) continue;
                    java_type_t ft = sc->fields[rows[r].field_idx].type;
                    sir_datatype_t dt = lat_tag_to_dt(ft.tag);
                    head = sir_store_local(a, rows[r].slot, dt, sir_ref_descriptor(a, ft),
                                           cp_sr_default(a, dt), head);
                }
                n->store_local.value = sir_load_null(a);
                n->store_local.next  = head;
            }
        }
    }

    /* (d) VIRTUAL-row GetFields substitute to slot loads (the shared subst map). */
    for (int v = 0; v < vc; v++) {
        sir_node_t* e = eng->vnodes[v]->expr;
        if (!e || e->tag != SIR_GETFIELD) continue;
        int o = cp_sr_recv_site(eng, pea_ok, e->get_field.obj);
        int k = o >= 0 ? pea_of_obj[o] : -1;
        int prow = vrow_conflict[v] ? -1 : vrow[v];
        if (k < 0 || prow < 0 || prow >= sn) continue;
        if (st[prow][k] != CP_PEA_VIRTUAL || esc_row[prow][k]) continue;
        int slot = cp_sr_slot_of(rows, nrows, o, e->get_field.class_id,
                                 e->get_field.field_idx);
        if (slot < 0) continue;
        const sema_class_t* sc = sema_get_class(eng->sema, e->get_field.class_id);
        sir_datatype_t dt = e->get_field.data_type;
        sir_node_t* ref = (dt == SIR_DTREF && sc)
            ? sir_ref_descriptor(a, sc->fields[e->get_field.field_idx].type) : NULL;
        cp_pmap_put(&eng->scalar_subst, e, sir_load_local(a, slot, dt, ref));
    }

    #undef CP_PEA_OUT
    /* scalar_count = FULLY-virtualized sites (the allocation is gone). A partial
     * site (nesc ≥ 1) sinks its allocation to the escape point — it is not a
     * scalar replacement and has its own probes. */
    for (int k = 0; k < np; k++)
        if (!pcs[k].fatal && pcs[k].nesc == 0) eng->scalar_count++;
    bbq_vec_free(rows);
    bbq_vec_free(pcs);
}

/* The pass: qualify, then rewrite. */
static void cp_scalar_replace(cp_engine_t* eng) {
    int oc = eng->obj_count;
    if (oc <= eng->obj_first_site) return;
    bool* cand = (bool*)bbq_arena_alloc(eng->arena, (size_t)oc * sizeof(bool));
    bool* disq = (bool*)bbq_arena_alloc(eng->arena, (size_t)oc * sizeof(bool));
    int*  pos  = (int*)bbq_arena_alloc(eng->arena,
                                       (size_t)oc * CP_SR_POS_COUNT * sizeof(int));
    memset(pos, 0, (size_t)oc * CP_SR_POS_COUNT * sizeof(int));

    eng->scalar_count = cp_scalar_qualify(eng, cand, pos, disq);

    if (getenv("JAVELINA_SCALAR_CENSUS"))
        cp_scalar_probe(eng, cand, pos, disq);

    /* §6.1 — ONE consumer authority (Stadler): cp_pea IS the struct pass; the
     * whole-method NoEscape site is its nesc == 0 case, the escaping-branch site
     * its partial case, the in-loop site its §5.4 case. The qualify sweep above
     * remains the census's classification (and pins §32's QUALIFY facts); its cand
     * set — always SIR_NEW sites, an overlay's backing declines via D4 — is
     * cleared so every site enters cp_pea untaken, and scalar_count is re-tallied
     * there from what actually fully virtualizes. */
    memset(cand, 0, (size_t)oc * sizeof(bool));
    eng->scalar_count = 0;
    cp_pea(eng, cand);
}

static void cp_rewrite_array_store_guards(cp_engine_t* eng) {
    if (!eng->facts || eng->fact_count == 0 || !eng->sema) return;
    for (int g = 0; g < eng->fact_count; g++) {
        const compiler_fact_t* gd = &eng->facts[g];
        if (gd->kind != COMPILER_FACT_GUARD) continue;
        if (gd->a != COMPILER_GUARD_ARRAY_STORE) continue;
        sir_node_t* eff = gd->key;                       /* the ExprEffect, not a Branch */
        if (!eff || eff->tag != SIR_EXPREFFECT) continue;            /* already gone */
        sir_node_t* call = eff->expr_effect.value;
        if (!call || call->tag != SIR_INVOKESTATIC
                  || call->invoke_static.args_count != 2) continue;

        /* WHICH ARRAY is this? The check guards the store that is its own continuation
         * (the DDCG builds `arraystore_guard(t_a, t_v, astore)`), and that store's array
         * operand is `GetField(RefArray.data)` — so its pts names the BACKING array
         * object, whose site is the NewRefArray carrying the element class (§1's
         * `array.new τ@site`). That is the ACTUAL component, from the allocation — never
         * the DECLARED one, which covariance makes a lie: `Object[] o = new A[1]` declares
         * Object and allocates A, and reading the declared type would delete the very
         * check that must throw.
         *
         * One array, one component. Two, or an array we never saw allocated (a parameter
         * — it may be an array of anything), and we prove nothing and keep the check. */
        sir_node_t* store = eff->expr_effect.next;
        if (!store || store->tag != SIR_ARRAYSTORE) continue;
        int av = cp_vnode_of(eng, store->array_store.arr);
        if (av < 0) continue;
        cp_pts_t ap = eng->vnodes[av]->pts;
        if (cp_pts_count(eng, ap) != 1 || cp_pts_has(eng, ap, CP_OBJ_NULL)) continue;
        int comp = -1;
        for (int w = 0; w < eng->obj_words && comp < 0; w++)
            if (ap.bits[w])
                comp = cp_array_component_of(eng, (w << 6) + __builtin_ctzll(ap.bits[w]));
        if (comp < 0) continue;                          /* component unknown ⟹ keep */

        int vv = cp_vnode_of(eng, call->invoke_static.args[1]);      /* the stored value */
        if (vv < 0) continue;
        cp_pts_t p = eng->vnodes[vv]->pts;
        if (cp_pts_empty(eng, p)) continue;              /* ∅ proves nothing */

        bool all_fit = true;
        for (int w = 0; w < eng->obj_words && all_fit; w++) {
            uint64_t word = p.bits[w];
            while (word) {
                int o = (w << 6) + __builtin_ctzll(word);
                word &= word - 1;
                if (o == CP_OBJ_NULL) continue;          /* null never throws (§10.10) */
                if (cp_obj_isa(eng, o, SIR_ATCLASS, comp) != 1) { all_fit = false; break; }
            }
        }
        if (!all_fit) continue;                          /* incl. any unknown class */

        /* Every object this value may name is assignable to the array's real component:
         * the check cannot throw. Drop the call — the ExprEffect becomes a Nop to its
         * own continuation, and the GetField of elemClass dies with it (DCE). Read the
         * continuation BEFORE the re-tag: the tag selects the union arm. */
        sir_node_t* next = eff->expr_effect.next;
        eff->tag = SIR_NOP;
        eff->nop.next = next;
    }
}

void cp_rewrite(cp_engine_t* eng) {
    /* Expression rewrites and constant-cond branch folding don't
     * read liveness, so they run first; they mutate the SIR's slot-
     * read pattern, which is why liveness is (re)computed *after*
     * them — any prior live_in would be stale by here. */
    cp_select_canonical_phis(eng);
    /* Before the expression rewrite: it replaces operands with new nodes that have no
     * vnode, and these read the solved facts off the original operands. */
    cp_rewrite_array_store_guards(eng);
    cp_scalar_replace(eng);
    for (int i = 0; i < eng->spine_count; i++) {
        eng->rewrite_spine_idx = i;
        cp_rewrite_spine_node(eng, eng->spine[i]);
    }
    cp_rewrite_branch_fold(eng);
    /* THE CSE LIFT IS GONE (spec §8). It was Click's GCM — its own comments said so
     * ("schedule-early", "schedule-late", "the most-control-dependent placement that still
     * DOMINATES every use") — and §8 is titled "Why there is no dominator tree (the whole
     * point)": *"the ONE classic use of dominators — Click's GCM scheduling of the
     * sea-of-nodes back to a CFG — you already don't do: DDCG emits from the CPS
     * structure."* It asked "is this value available here?", which §8 answers with *"a
     * value IS a node; using it IS an edge; GVN merges congruent nodes globally. No
     * availability query."*
     *
     * MEASURED before removing, on the whole jre: it made the module 3 bytes BIGGER
     * (634,262 → 634,259 without it), changed the guard census by NOTHING, and cost ~0.7s
     * of the 7.7s compile. It also had a miscompile to its name (parseLong's I2L(digit)
     * lifted into a branch arm and read uninitialized on the path around it) — which is
     * what the dominance machinery had been added to paper over.
     *
     * GVN itself stays, and is load-bearing: congruence is what proves `i < a.length`
     * against the guard's own `a.length` (the bounds consumer), and what lets a load
     * forward to its reaching store. What is gone is materializing a congruent expression
     * into a local, which requires choosing a program point — a scheduling question the
     * DDCG already answered when it emitted from the CPS structure. */
    /* The CSE lift splices new StoreLocal nodes into the spine (now
     * registered in the spine index by cp_splice_before). reachable[] is
     * sized to the pre-lift spine, so reset and recompute it — Click §6.3
     * schedules over the complete optimized graph, and liveness/DSE skip
     * nodes flagged unreachable. */
    eng->reachable = NULL;
    cp_compute_reachability(eng);
    cp_compute_liveness(eng);
    cp_rewrite_dse(eng);
    cp_rewrite_empty_branch(eng);
    cp_rewrite_compact_nops_gotos(eng);
}

/* ── Entry points ────────────────────────────────────────────── */

/* Index the DDCG's recorded LOOP scopes by spine node: which nodes did the frontend
 * build as loop headers? That is the ONE thing the optimizer needs from the recorded
 * control flow — spec §5/§8: range widening happens on the back edge, and the loop is
 * LOOKED UP, never rediscovered.
 *
 * This used to build a predecessor/merge graph as well (`is_merge`, `npred`, `pred_of`).
 * Nothing reads it any more: its only consumer was the CSE lift's dominance walk, which
 * is gone (§8 — "No availability query"). The tables went with it rather than being left
 * as a graph for the next dominance query to find lying around. */
/* §6's throw rule, indexed ONCE: for each `Throw` on the spine, the catch classes of the
 * regions that enclose it IN THIS METHOD.
 *
 * A LOOKUP, not a walk. The exceptional edges (cp_index_except_edges) already say which
 * handlers each excepting node can reach — that IS the enclosure, joined from the DDCG's
 * recorded rows. So the catch classes come off those handlers' own ExceptionEntry nodes.
 *
 * This used to DFS the spine from each region's body, stopping at an `Ljoin` recovered
 * from a COMPILER_SCOPE_BLOCK row — region containment recovered by traversal, i.e. a
 * dominance query with the word left out, and a second authority for an extent the
 * frontend owns. Spec §8. Gone.
 *
 * Structural and pre-solve, like every other Obj/scope naming (syntactic, pre-solve): which regions cover a
 * throw cannot depend on the fixpoint. WHICH of them actually catches it does — that is
 * the transfer, and it reads pts (cp_throw_is_caught). */
static void cp_index_try_regions(cp_engine_t* eng) {
    int sn = eng->spine_count;
    eng->throw_catches = (int**)bbq_arena_alloc(eng->arena,
                             (size_t)(sn > 0 ? sn : 1) * sizeof(int*));
    for (int i = 0; i < sn; i++) eng->throw_catches[i] = NULL;

    for (int i = 0; i < sn; i++) {
        if (eng->spine[i]->tag != SIR_THROW) continue;
        for (int k = 0; k < cp_exc_succ_count(eng, i); k++) {
            sir_node_t* h = eng->spine[cp_exc_succ(eng, i, k)];
            if (h->tag != SIR_EXCEPTIONENTRY) continue;
            /* ONLY A CATCH CLAUSE CATCHES. The region's catch-all runs finally and
             * RE-THROWS — JLS §11.3: an exception matching no catch clause LEAVES the try,
             * and the finally runs "during propagation of the exception". It declares no
             * type, and the asdl says so: catch_class_id = -1, "no type info (finally /
             * throwable-catch)". Reading it as a catch of Throwable made EVERY throw inside
             * ANY try look contained, because sema_ref_is_subtype(cls, Throwable) is
             * always true.
             *
             * The node answers it. That field exists for precisely this reader — the asdl:
             * "the sema class id of the declared catch type, so downstream passes (Click
             * type lattice, verifier hints) don't have to rediscover it". No fact lookup,
             * no reading the handler's BODY to see if it re-throws. */
            if (h->exception_entry.catch_class_id < 0) continue;
            bbq_vec_push(eng->throw_catches[i], h->exception_entry.catch_class_id);
        }
    }
}

static void cp_index_recorded_merges(cp_engine_t* eng) {
    int sn = eng->spine_count;
    size_t nb = (size_t)(sn > 0 ? sn : 1);
    eng->merge_rows = sn;
    eng->is_loop_header = (bool*)bbq_arena_alloc(eng->arena, nb * sizeof(bool));
    memset(eng->is_loop_header, 0, nb * sizeof(bool));

    eng->any_scope_recorded = false;
    for (int i = 0; i < eng->fact_count; i++) {
        const compiler_fact_t* s = &eng->facts[i];
        if (s->kind != COMPILER_FACT_SCOPE) continue;
        eng->any_scope_recorded = true;
        if (s->a != COMPILER_SCOPE_LOOP) continue;
        int h = cp_spine_index(eng, s->key);             /* Ltop: entry + back edge */
        if (h >= 0) eng->is_loop_header[h] = true;
    }
}

/* The compiler's entry: everything comes out of the ONE context. Each fact the DDCG
 * recorded is TAKEN from it here — never threaded in as a parameter, so adding the
 * next fact does not change a signature. */
/* The engine's SCRATCH (vnodes, heaps, the §42 inject matrix, partitions) is allocated from
 * `eng_arena`, which need not be `ctx->arena`: a SUMMARIZE-only build (sir_summarize) hands in a
 * private arena it frees straight after cp_summarize, because the summary is the only output and it
 * is copied wholesale into ctx->arena. A REWRITE build (sir_optimize) hands in ctx->arena, because
 * cp_rewrite mints new SIR that must outlive the engine. Without this the convergence loop's
 * (mc+8)×methods engines all accumulate in ctx->arena — gigabytes for a large module. */
static cp_engine_t* cp_build_ctx_in(compiler_ctx_t* ctx, sir_method_t* method,
                                    const compiler_fact_t* facts, int fact_count,
                                    bbq_arena* eng_arena) {
    cp_engine_t* eng = cp_build_no_solve(method, ctx ? ctx->sema : NULL,
                                         eng_arena,
                                         facts, fact_count);
    if (!eng) return NULL;
    eng->ctx        = ctx;
    cp_index_recorded_merges(eng);
    cp_index_try_regions(eng);      /* needs the facts, so: after they attach */
    cp_index_concrete_objects(eng); /* needs the facts, so: after they attach */
    cp_init_facts(eng);
    cp_solve(eng);
    return eng;
}

cp_engine_t* cp_build_ctx(compiler_ctx_t* ctx, sir_method_t* method,
                          const compiler_fact_t* facts, int fact_count) {
    return cp_build_ctx_in(ctx, method, facts, fact_count, ctx ? ctx->arena : NULL);
}

/* The unit harness's entry: a hand-built sir_method_t, for which there is no context
 * and therefore no recorded DDCG facts. */
cp_engine_t* cp_build(sir_method_t* method, const sema_ctx_t* sema,
                       bbq_arena* arena,
                       const compiler_fact_t* facts, int fact_count) {
    cp_engine_t* eng = cp_build_no_solve(method, sema, arena, facts, fact_count);
    if (!eng) return NULL;
    cp_index_recorded_merges(eng);
    cp_index_try_regions(eng);      /* needs the facts, so: after they attach */
    cp_index_concrete_objects(eng);
    cp_init_facts(eng);
    cp_solve(eng);
    return eng;
}

/* Test-only: build engine without running the outer solve. Lets unit
 * tests inspect the engine in its pre-init state (partitions assigned,
 * facts uninitialized). */
cp_engine_t* cp_build_no_solve(sir_method_t* method, const sema_ctx_t* sema,
                                bbq_arena* arena,
                                const compiler_fact_t* facts, int fact_count) {
    if (!method || !method->entry) return NULL;
    cp_engine_t* eng = (cp_engine_t*)bbq_arena_alloc(arena, sizeof *eng);
    memset(eng, 0, sizeof *eng);
    eng->arena      = arena;
    eng->method     = method;
    eng->sema       = sema;
    /* The facts attach HERE, not after the build: cp_resolve places the φs, and a
     * handler's φ exists only because the recorded EXCEPT_REGION rows make it a merge
     * (spec §1). Attaching them later would resolve the whole method against a CFG that
     * is missing every exception edge. */
    eng->facts      = facts;
    eng->fact_count = fact_count;
    eng->slot_count = method->max_locals > 0 ? method->max_locals : 1;
    type_pool_init(&eng->pool, arena);
    cp_pmap_init(&eng->spine_idx);
    cp_pmap_init(&eng->expr_idx);
    cp_pmap_init(&eng->scalar_subst);
    bbq_hmap_init(&eng->refine_intern, 0);
    cp_collect_spine(eng, method->entry);
    cp_index_except_edges(eng);     /* the exceptional CFG edges — before cp_resolve */
    cp_scan_slot_types(eng->spine, eng->spine_count, eng->slot_count,
                       &eng->slot_types, &eng->pool, arena);
    cp_enumerate(eng);
    cp_enumerate_memory_cells(eng);
    cp_enumerate_objects(eng);      /* Obj naming is syntactic — fixed before the solve */
    cp_escape_init(eng);            /* …so lattice E's domain is fixed here too (§6) */
    /* K-set for cp_const_widen: precomputed once after vnodes exist,
     * before any solve. Click §3.7 phase-ordering — analysis reads K,
     * nothing during analysis mutates the LoadConst set. */
    cp_build_widen_k(eng);
    cp_resolve(eng);
    cp_compute_branch_refinements(eng);
    cp_build_defuse(eng);
    cp_partition_init(eng);
    return eng;
}

/* Click §4.2 + §4.4.2 one-time init. Sets every vnode's type to TOP
 * and constant to CP_C_TOP; seeds every vnode onto its partition's
 * cprop (Click seeds just START; we seed all for simplicity); and
 * seeds every partition onto the CAUSE_SPLITS worklist (per §4.2:
 * "Place all partitions on worklist"). Subsequent cp_refine calls
 * drain the worklist; cp_split / apply re-enqueue as new work arises. */
void cp_init_facts(cp_engine_t* eng) {
    int vc = eng->vnode_count;
    if (vc == 0) return;
    cp_const_t ctop = { .state = CP_C_TOP };
    cp_pts_t   pbot = { NULL };            /* lattice A starts at ⊥ = ∅ and ascends */
    for (int v = 0; v < vc; v++) {
        eng->vnodes[v]->type     = type_top(&eng->pool);
        eng->vnodes[v]->constant = ctop;
        eng->vnodes[v]->pts      = pbot;
        eng->vnodes[v]->heap     = NULL;   /* ⊥; allocated once, on first update */
        cp_cprop_enqueue(eng, v);
    }
    for (int p = 0; p < eng->partition_count; p++)
        cp_wl_push(eng, p);
}

/* Run the outer PROPAGATE+CAUSE_SPLITS solve loop until convergence.
 * Click §4.4.2: "Do until worklist and cprop are empty {
 *     PROPAGATE(cprop);
 *     If worklist not empty: CAUSE_SPLITS(worklist);
 * }"
 * Termination signaled by both worklists empty after a full pass.
 * Our impl interleaves reachability (UCE+CCP, §3.7/§4.3.2) and uses
 * cp_split_by_facts as a SPLIT_BY proxy before cp_refine. Each loop
 * iteration must drain cprop_worklist via cp_compute_facts before
 * cp_refine (which may re-enqueue cprop entries via §4.7.5 line 33-34).
 * Loop terminates only when neither worklist nor cprop has pending
 * work AND reach_count is stable. */
/* THE RE-ARM (the recurring bug class, and the plan predicted a fourth — this is the
 * fifth, and the cong_fold arm below is the seventh). `cp_symbolic_bound_const` reads a
 * PARTITION: whether the bound that refined the
 * index and the length this compare reads are the same value. That fact does not arrive
 * over a def-use edge, so nothing in PROPAGATE will ever revisit the compare when it
 * changes — and it DOES change, because partitions start coarse and SPLIT as the solve
 * refines. A `KNOWN 0` left standing on a pair that has since split apart is not stale,
 * it is UNSOUND: a bounds guard deleted on a proof that no longer holds.
 *
 * The §4.6 cong_fold engine block reads partitions the same off-graph way, and its
 * premature-fold guard covers only HALF the transient: TOP operands and disagreeing
 * KNOWNs are rejected, but two BOTTOM loads still sharing their initial opcode bucket
 * (a[i] and b[i] before CAUSE_SPLITS tells their arrays apart) pass it and fold —
 * BitSet.xor's `bits[i] ^ set.bits[i]` folded to 0. The fold is fine as an OPTIMISTIC
 * transient; what was missing is exactly this re-arm, so the split that separates the
 * loads re-runs the fold and the KNOWN falls to the γ answer. So: every vnode whose row
 * carries a cong_fold goes back on cprop too (SUB / XOR / the six compares).
 *
 * Gated on the partition count actually moving — re-arming unconditionally would keep
 * the worklist non-empty and cp_solve would never terminate. The count only grows and
 * is bounded by the vnode count, so this terminates. */
static void cp_rearm_partition_consumers(cp_engine_t* eng) {
    for (int v = 0; v < eng->vnode_count; v++) {
        const cp_vnode_t* vn = eng->vnodes[v];
        if (vn->kind != CP_VN_EXPR || !vn->expr) continue;
        int t = vn->expr->tag;
        if (t == SIR_GE || sir_op_gamma[t].cong_fold != GC_NONE)
            cp_cprop_enqueue(eng, v);
    }
}

void cp_solve(cp_engine_t* eng) {
    for (;;) {
        int prev_rc = eng->reach_count;
        int prev_pc = eng->partition_count;
        cp_compute_reachability(eng);
        cp_compute_facts(eng);
        cp_split_by_facts(eng);
        if (eng->partition_count != prev_pc) cp_rearm_partition_consumers(eng);
        /* Click §4.7.5 line 16-22: Follower-apply runs AFTER SPLIT so
         * the all-inputs-in-same-partition check reads post-split
         * partitions, not the stale pre-split groupings. */
        cp_apply_followers_pass(eng);
        cp_refine(eng);
        /* §6's lattice E is an ELEMENT of this fixpoint, not a pass after it (§9: "each
         * lattice is added as an element of the SAME fixpoint"). It reads pts, which the
         * round above may have grown, and it gates the exit below — so the loop cannot
         * finish while escape is still descending. */
        bool esc_moved = cp_escape_update(eng);
        /* THE RE-ARM (6th instance of the class). A CP_MEM_KILL node's transfer reads the
         * ESCAPE state to decide which rows the call may have clobbered — and escape reaches
         * it through no def-use edge at all. Escape only descends, so a row this round kept
         * as "the callee cannot reach it" may have to be killed next round. Without an
         * explicit re-arm the heap keeps an optimistic value nothing ever revisits, and the
         * fixpoint settles on an UNSOUND answer. Gated on escape actually moving, or cp_solve
         * would never terminate. */
        if (esc_moved)
            for (int vi = 0; vi < eng->mem_rows; vi++)
                if (eng->mem_kind[vi] == CP_MEM_KILL) cp_cprop_enqueue(eng, vi);
        /* Click §4.4.2: terminate when both worklists are empty (no
         * pending work) AND reachability is stable. Reachability is
         * tracked separately from the worklists since the §3.7 UCE+CCP
         * combined behavior triggers cp_compute_reachability outside
         * the cprop/worklist machinery. */
        if (bbq_vec_len(eng->cprop_worklist) == 0
                && bbq_vec_len(eng->worklist) == 0
                && eng->reach_count == prev_rc
                && !esc_moved) break;
    }
}

void cp_free(cp_engine_t* eng) {
    if (!eng) return;
    bbq_vec_free(eng->spine);
    bbq_vec_free(eng->vnodes);
    bbq_vec_free(eng->partitions);
    bbq_vec_free(eng->worklist);
    cp_pmap_free(&eng->spine_idx);
    cp_pmap_free(&eng->expr_idx);
    cp_pmap_free(&eng->scalar_subst);
    bbq_hmap_free(&eng->refine_intern);
    if (eng->mem_cell_idx) bbq_htree_destroy(eng->mem_cell_idx);
    if (eng->callee_idx)   bbq_htree_destroy(eng->callee_idx);
    bbq_vec_free(eng->mem_cell_keys);
    bbq_vec_free(eng->clobx_obj);
    bbq_vec_free(eng->clobx_key);
    type_pool_destroy(&eng->pool);
}

/* ── Slot bin-packing post-pass ────────────────────────────────── */

static int cp_pack_node_index(const cp_pmap_t* idx, const sir_node_t* n) {
    if (!n) return -1;
    void* v = cp_pmap_get(idx, n);
    return v ? (int)((uintptr_t)v - 1) : -1;
}

void cp_pack(sir_method_t* method, const sema_ctx_t* sema,
              bbq_arena* arena, int initial_max_locals) {
    if (!method || !method->entry) return;
    int sc = method->max_locals > 0 ? method->max_locals : 1;

    /* The post-rewrite spine, off THE collector (sir_support.h). cp_rewrite has mutated the
     * SIR (dead branches pruned, nodes re-tagged, …) so the engine's spine is stale by here
     * — hence a fresh collection, not a fresh COLLECTOR. */
    sir_node_t** nodes = sir_collect_spine(method->entry);
    cp_pmap_t node_idx_map; cp_pmap_init(&node_idx_map);
    cp_pmap_t* node_idx = &node_idx_map;
    for (int i = 0; i < (int)bbq_vec_len(nodes); i++)
        cp_pmap_put(node_idx, nodes[i], (void*)(uintptr_t)(i + 1));
    int nn = bbq_vec_len(nodes);
    if (nn == 0 || sc == 0) {
        bbq_vec_free(nodes);
        cp_pmap_free(node_idx);
        return;
    }

    /* Slot dts + referent Types via the ONE slot-type scanner (the same
     * scanner that types the engine's opaque seeds), over the POST-
     * rewrite spine — the pack pools by what actually survives. */
    type_pool_t ref_pool; type_pool_init(&ref_pool, arena);
    cp_slot_types_t sts;
    cp_scan_slot_types(nodes, nn, sc, &sts, &ref_pool, arena);
    sir_datatype_t* slot_dt = sts.dt;

    /* The EXCEPTIONAL edges, hoisted over THIS node list from the graph's own `exc`
     * fields — the same one function the engine used (spec §1). The engine is freed by
     * here and the pack re-collects the post-rewrite spine, so the indices differ, but
     * the GRAPH does not: the packer cannot see different edges than the analysis. */
    int** exc = NULL;
    cp_hoist_exc_edges(nodes, nn, arena, &exc);

    /* Backward slot liveness over the post-rewrite spine. Every
     * collected node is reachable from method->entry by construction
     * (DFS from entry), so no per-node reachability filter is needed. */
    size_t row = (size_t)sc * sizeof(bool);
    bool** live_in  = (bool**)bbq_arena_alloc(arena, (size_t)nn * sizeof(bool*));
    bool** live_out = (bool**)bbq_arena_alloc(arena, (size_t)nn * sizeof(bool*));
    for (int i = 0; i < nn; i++) {
        live_in[i]  = (bool*)bbq_arena_alloc(arena, row);
        live_out[i] = (bool*)bbq_arena_alloc(arena, row);
        memset(live_in[i],  0, row);
        memset(live_out[i], 0, row);
    }
    bool* new_in  = (bool*)bbq_arena_alloc(arena, row);
    bool* new_out = (bool*)bbq_arena_alloc(arena, row);
    bool* exc_out = (bool*)bbq_arena_alloc(arena, row);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = nn - 1; i >= 0; i--) {
            sir_node_t* n = nodes[i];
            memset(new_out, 0, row);
            memset(exc_out, 0, row);
            int sc_cap = sir_succ_count(n);
            for (int j = 0; j < sc_cap; j++) {
                int si = cp_pack_node_index(node_idx, sir_succ(n, j));
                if (si >= 0)
                    for (int s = 0; s < sc; s++)
                        if (live_in[si][s]) new_out[s] = true;
            }
            /* The EXCEPTIONAL edges — the same recorded rows the engine's liveness read.
             * Without them the packer thinks a slot whose only consumer is a catch block
             * is dead here, and coalesces another slot onto it. */
            for (int j = 0; j < (int)bbq_vec_len(exc[i]); j++) {
                int hi = exc[i][j];
                for (int s = 0; s < sc; s++)
                    if (live_in[hi][s]) { exc_out[s] = true; new_out[s] = true; }
            }
            memcpy(new_in, new_out, row);
            int def = cp_node_def_slot(n);
            /* No kill on the exceptional edge: the def never committed (JLS §11.3.1). */
            if (def >= 0 && def < sc && !exc_out[def]) new_in[def] = false;
            cp_node_uses(n, new_in, sc);
            if (memcmp(new_in,  live_in[i],  row) != 0 ||
                memcmp(new_out, live_out[i], row) != 0) {
                memcpy(live_in[i],  new_in,  row);
                memcpy(live_out[i], new_out, row);
                changed = true;
            }
        }
    }

    /* Anchor: caller-set parameter cells [0, args_cells) — these
     * keep their original positions. Everything else gets packed. */
    int args_cells = initial_max_locals;
    if (sema) {
        const sema_class_t* cls = sema_get_class(sema, method->class_id);
        if (cls && method->method_id >= 0 &&
            method->method_id < (int32_t)bbq_vec_len(cls->methods)) {
            const sema_method_t* sm = &cls->methods[method->method_id];
            /* One cell per param regardless of type: sema numbers one
             * slot per param, and a WASM local is one index (width-1
             * pools below) — plus one for `this` on instance methods. */
            args_cells = ((sm->modifiers & ACC_STATIC) ? 0 : 1)
                       + sm->param_count;
        }
    }
    bool* is_param = (bool*)bbq_arena_alloc(arena, row);
    memset(is_param, 0, row);
    for (int s = 0; s < args_cells && s < sc; s++) is_param[s] = true;

    /* slot_used: any slot referenced anywhere in the SIR. Skipped
     * unused slots stay unassigned so they don't pad the frame. */
    bool* slot_used = (bool*)bbq_arena_alloc(arena, row);
    memset(slot_used, 0, row);
    for (int i = 0; i < nn; i++) {
        sir_node_t* n = nodes[i];
        if (!n) continue;
        int defs = cp_node_def_slot(n);
        if (defs >= 0 && defs < sc) slot_used[defs] = true;
        cp_node_uses(n, slot_used, sc);
    }

    /* Interference, both halves of Chaitin's rule: (a) two slots co-live in
     * some live_out[i] interfere; (b) the slot a node DEFINES interferes with
     * every OTHER slot live-out at that node — even when the def itself is
     * dead. (a) alone matches (b) only when every store is live: a dead store
     * builds no live range, shows disjoint from everything, and coalescing it
     * into a live range turns the dead write into a clobber of the merged
     * cell. Entry-time interferences propagate through predecessor live_out
     * (live_in[i] ⊆ live_out[pred]). A use-then-def at one instruction
     * (slot_u in live_in only) stays correctly non-interfering under both
     * halves: the def may take over the dying slot's cell. */
    bool* interferes = (bool*)bbq_arena_alloc(arena,
                                               (size_t)sc * (size_t)sc * sizeof(bool));
    memset(interferes, 0, (size_t)sc * (size_t)sc * sizeof(bool));
    for (int i = 0; i < nn; i++) {
        for (int s1 = 0; s1 < sc; s1++) {
            if (!live_out[i][s1]) continue;
            for (int s2 = s1 + 1; s2 < sc; s2++) {
                if (live_out[i][s2]) {
                    interferes[s1 * sc + s2] = true;
                    interferes[s2 * sc + s1] = true;
                }
            }
        }
        int d = cp_node_def_slot(nodes[i]);
        if (d >= 0 && d < sc)
            for (int s = 0; s < sc; s++)
                if (s != d && live_out[i][s]) {
                    interferes[d * sc + s] = true;
                    interferes[s * sc + d] = true;
                }
    }

    /* Width-class pools by WASM valtype: byte/short/char/int all lower to
     * i32, so they share pool 0 and coalesce freely; long→i64, float→f32,
     * double→f64, ref each get their own pool. A WASM local is one index
     * regardless of valtype (no two-cell wides), so every pool is
     * width 1. */
    /* Pool id = the lattice's lowered valtype (the ONE dt→valtype
     * authority): i32, i64, f32, f64, ref — in lat_valtype_t order. */
    #define POOL_OF(dt) ((int)lat_dt_valtype(dt))
    #define POOL_WIDTH(p) (1)
    /* One pool per WASM valtype, sized FROM the lattice enum — a literal here
     * was the eighth copy of the "N-wide" disease: it stayed 5 when
     * LAT_VT_V128 became the sixth valtype, so v128 slots indexed pool_max[]
     * out of bounds and renamed past max_locals ("unknown local" at load). */
    #define POOL_COUNT ((int)LAT_VT_V128 + 1)

    int* new_slot = (int*)bbq_arena_alloc(arena, (size_t)sc * sizeof(int));
    int* color    = (int*)bbq_arena_alloc(arena, (size_t)sc * sizeof(int));
    int  pool_max[POOL_COUNT] = {0};
    for (int s = 0; s < sc; s++) { new_slot[s] = -1; color[s] = -1; }

    /* Anchor params at their original cells. */
    int anchor_cells = 0;
    for (int s = 0; s < sc; s++) {
        if (is_param[s]) {
            new_slot[s] = s;
            int end = s + POOL_WIDTH(POOL_OF(slot_dt[s]));
            if (end > anchor_cells) anchor_cells = end;
        }
    }

    /* Sort non-param packables by live-range length DESC. */
    int* order = (int*)bbq_arena_alloc(arena, (size_t)sc * sizeof(int));
    int* range = (int*)bbq_arena_alloc(arena, (size_t)sc * sizeof(int));
    int packable = 0;
    for (int s = 0; s < sc; s++) {
        if (is_param[s] || !slot_used[s]) continue;
        order[packable] = s;
        int first_def = -1, last_use = -1;
        for (int i = 0; i < nn; i++) {
            bool defs_here = (cp_node_def_slot(nodes[i]) == s);
            bool live_here = live_in[i][s] || live_out[i][s];
            if (defs_here && first_def < 0) first_def = i;
            if (live_here) last_use = i;
        }
        range[packable] = (first_def < 0 || last_use < 0) ? 0 : (last_use - first_def);
        packable++;
    }
    for (int i = 1; i < packable; i++) {
        int ki = order[i], ri = range[i], j = i;
        while (j > 0 && range[j - 1] < ri) {
            order[j] = order[j - 1];
            range[j] = range[j - 1];
            j--;
        }
        order[j] = ki;
        range[j] = ri;
    }

    /* Greedy color each packable slot within its pool. */
    for (int k = 0; k < packable; k++) {
        int s = order[k];
        int p = POOL_OF(slot_dt[s]);
        for (int c_color = 0; c_color <= pool_max[p]; c_color++) {
            bool clean = true;
            bool any_used = false;
            for (int s2 = 0; s2 < sc; s2++) {
                if (is_param[s2]) continue;
                if (POOL_OF(slot_dt[s2]) != p) continue;
                if (color[s2] != c_color) continue;
                /* Same pool ⇒ same WASM valtype, so a shared local index is
                 * well-typed — byte/short/char/int coalesce as one i32 local.
                 * EXCEPT refs: a ref local is TYPED by its referent, so two
                 * ref slots share an index only when their threaded
                 * descriptors agree exactly (never when one is descriptor-
                 * less or carries conflicting descriptors). */
                any_used = true;
                if (p == (int)LAT_VT_REF && (sts.ref_uniq[s] || sts.ref_uniq[s2]
                            || !sts.ref[s]
                            || sts.ref[s] != sts.ref[s2])) {
                    clean = false; break;
                }
                if (interferes[s * sc + s2]) { clean = false; break; }
            }
            if (clean && (any_used || c_color == pool_max[p])) {
                color[s] = c_color;
                if (c_color == pool_max[p]) pool_max[p]++;
                break;
            }
        }
    }

    /* Lay the valtype pools out as consecutive local-index ranges after
     * the anchored params: i32, then i64, f32, f64, ref. */
    int pool_base[POOL_COUNT];
    pool_base[0] = anchor_cells;
    for (int p = 1; p < POOL_COUNT; p++)
        pool_base[p] = pool_base[p - 1] + pool_max[p - 1] * POOL_WIDTH(p - 1);
    int total_cells = pool_base[POOL_COUNT - 1]
                    + pool_max[POOL_COUNT - 1] * POOL_WIDTH(POOL_COUNT - 1);

    for (int s = 0; s < sc; s++) {
        if (new_slot[s] >= 0) continue;       /* already a param */
        if (color[s] < 0)     continue;       /* unused */
        int p = POOL_OF(slot_dt[s]);
        new_slot[s] = pool_base[p] + color[s] * POOL_WIDTH(p);
    }

    /* Rewrite every slot reference in the SIR. cp_rewrite's GVN-driven
     * LoadLocal forwarding can leave a single LoadLocal sir_node_t
     * referenced from multiple parents (a value DAG); a naive tree-
     * walk would chain new_slot[] lookups per visit. Flat-iterate
     * every LoadLocal exactly once via a renamed-set. */
    for (int i = 0; i < nn; i++) {
        sir_node_t* n = nodes[i];
        if (!n) continue;
        switch (n->tag) {
            case SIR_STORELOCAL: {
                int s = n->store_local.slot;
                if (s >= 0 && s < sc && new_slot[s] >= 0)
                    n->store_local.slot = new_slot[s];
                break;
            }
            case SIR_INC: {
                int s = n->inc.slot;
                if (s >= 0 && s < sc && new_slot[s] >= 0)
                    n->inc.slot = new_slot[s];
                break;
            }
            case SIR_EXCEPTIONENTRY: {
                int s = n->exception_entry.local_slot;
                if (s >= 0 && s < sc && new_slot[s] >= 0)
                    n->exception_entry.local_slot = new_slot[s];
                break;
            }
            default: break;
        }
    }
    cp_pmap_t renamed; cp_pmap_init(&renamed);
    sir_node_t** stack = NULL;
    for (int i = 0; i < nn; i++) {
        sir_node_t* n = nodes[i];
        if (!n) continue;
        int k = sir_arity(n);
        for (int j = 0; j < k; j++) bbq_vec_push(stack, sir_child(n, j));
    }
    while (bbq_vec_len(stack)) {
        sir_node_t* e = stack[bbq_vec_len(stack) - 1];
        bbq__vec_hdr(stack)->len--;
        if (!e) continue;
        if (cp_pmap_get(&renamed, e)) continue;
        cp_pmap_put(&renamed, e, (void*)1);
        if (e->tag == SIR_LOADLOCAL) {
            int s = e->load_local.slot;
            if (s >= 0 && s < sc && new_slot[s] >= 0)
                e->load_local.slot = new_slot[s];
            continue;
        }
        int ar = sir_arity(e);
        for (int j = 0; j < ar; j++)
            bbq_vec_push(stack, sir_child(e, j));
    }
    bbq_vec_free(stack);
    cp_pmap_free(&renamed);

    method->max_locals = total_cells;
    #undef POOL_OF
    #undef POOL_WIDTH
    #undef POOL_COUNT

    bbq_vec_free(nodes);
    cp_pmap_free(node_idx);
}

/* ── Public entry point ──────────────────────────────────────────
 *
 * Builds the partition engine, runs the converged rewrite, then bin-
 * packs slots. The engine is freed before cp_pack — cp_pack collects
 * its own (post-rewrite) spine, so there's no stale-spine hazard.
 */
static void cp_debug_dump_spine(sir_method_t* method, const char* cls, const char* phase) {
    const char* dump = getenv("JAVELINA_DUMP_SPINE");
    if (!dump || !method->name || !strstr(method->name, dump)) return;
    fprintf(stderr, "[spine-%s] == %s.%s ==\n", phase, cls ? cls : "?", method->name);
    sir_node_t** stack = NULL;
    cp_pmap_t seen; cp_pmap_init(&seen);
    bbq_vec_push(stack, method->entry);
    while (bbq_vec_len(stack)) {
        sir_node_t* n = stack[bbq_vec_len(stack) - 1];
        bbq__vec_hdr(stack)->len--;
        if (!n || cp_pmap_get(&seen, n)) continue;
        cp_pmap_put(&seen, n, (void*)1);
        if (n->tag == SIR_INC)
            fprintf(stderr, "[spine-%s] %p Inc slot=%d next=%p(next_tag=%d)\n",
                    phase, (void*)n, n->inc.slot, (void*)n->inc.next,
                    n->inc.next ? (int)n->inc.next->tag : -1);
        if (n->tag == SIR_BRANCH)
            fprintf(stderr, "[spine-%s] %p Branch t=%p(tag %d) f=%p(tag %d)\n",
                    phase, (void*)n,
                    (void*)n->branch.on_true, n->branch.on_true ? (int)n->branch.on_true->tag : -1,
                    (void*)n->branch.on_false, n->branch.on_false ? (int)n->branch.on_false->tag : -1);
        if (n->tag == SIR_NOP)
            fprintf(stderr, "[spine-%s] %p Nop next=%p\n", phase,
                    (void*)n, (void*)n->nop.next);
        if (n->tag == SIR_STORELOCAL) {
            sir_node_t* sv = n->store_local.value;
            fprintf(stderr, "[spine-%s] %p Store slot=%d rt=%d val=%p(tag %d%s%d)\n", phase,
                    (void*)n, n->store_local.slot,
                    n->store_local.ref_type ? 1 : 0, (void*)sv,
                    sv ? (int)sv->tag : -1,
                    sv && sv->tag == SIR_LOADLOCAL ? " rdslot=" : " x",
                    sv && sv->tag == SIR_LOADLOCAL ? sv->load_local.slot : 0);
        }
        /* Any NewArray in this node's expression trees: its size child's tag (+ const
         * payload when it IS a constant) — who feeds the allocation, before and after. */
        for (int i = 0; i < sir_arity(n); i++) {
            sir_node_t* stk2[64]; int sp2 = 0;
            if (sir_child(n, i)) stk2[sp2++] = sir_child(n, i);
            while (sp2 > 0) {
                sir_node_t* c = stk2[--sp2];
                if (c->tag == SIR_NEWARRAY) {
                    sir_node_t* szn = c->new_array.size;
                    int aux = szn ? (szn->tag == SIR_LOADCONST ? szn->load_const.value
                                   : szn->tag == SIR_LOADLOCAL ? szn->load_local.slot : 0) : 0;
                    fprintf(stderr, "[spine-%s] %p NewArray size=%p(tag %d %s=%d)\n",
                            phase, (void*)c, (void*)szn, szn ? (int)szn->tag : -1,
                            szn && szn->tag == SIR_LOADCONST ? "val"
                              : szn && szn->tag == SIR_LOADLOCAL ? "slot" : "x", aux);
                }
                for (int k = 0; k < sir_arity(c) && sp2 < 60; k++)
                    if (sir_child(c, k)) stk2[sp2++] = sir_child(c, k);
            }
        }
        for (int i = 0; i < sir_succ_count(n); i++)
            bbq_vec_push(stack, sir_succ(n, i));
    }
    bbq_vec_free(stack);
    cp_pmap_free(&seen);
}

/* §7's per-method escape SUMMARY, produced as a pure READOUT of the solved escape
 * lattice — the same domain the census reads, no mutation. Per ref formal (keyed by slot,
 * `this` separately since it is LOADTHIS not a slot): its post-solve escape state. Stored on
 * ctx, keyed by method index, for a caller's later solve to consume (the MapsTo mapping).
 *
 * WHAT THIS DOES NOT YET CARRY: the reachable sub-graph edges (§7 / Fig 7's cross-parameter
 * mapping) and the return's pts (the pointer half); the full Fig 7 is not built here.
 * This first cut is the per-formal escape STATE, which is exactly what Fig 7 propagates
 * (GlobalEscape only) at a call site. */
/* Is object `o` among the objects `e` (a value expression) may point to? */
static bool cp_value_pts_has(cp_engine_t* eng, const sir_node_t* e, int o) {
    if (!e) return false;
    int vi = cp_vnode_of(eng, e);
    if (vi < 0 || vi >= eng->vnode_count) return false;
    return cp_pts_has(eng, eng->vnodes[vi]->pts, o);
}

/* A callee summary's classification for one of its params (Fig 7 reads this at the call
 * site). A missing summary is a §7 BOTTOM METHOD ⟹ conservative ArgEscape. */
static unsigned char cp_callee_param_class(compiler_ctx_t* ctx, int callee, bool is_this, int slot) {
    const compiler_summary_t* s = compiler_method_summary(ctx, callee);
    if (!s) return COMPILER_ESC_ARG;                 /* bottom method */
    unsigned char c = is_this ? s->this_escape
                     : (slot >= 0 && slot < s->slot_count) ? s->slot_escape[slot]
                                                           : COMPILER_ESC_NA;
    return (c == COMPILER_ESC_NA) ? COMPILER_ESC_ARG : c;
}

/* How a formal (its phantom object `o`) escapes the CALLEE — the summary a caller consumes
 * (Choi Fig 7), NOT the seed-polluted intra escape state. A forward reachability of a REAL
 * source from the formal, over the reachable spine:
 *   GLOBAL  stored into a static / a Global object, or passed to a param a callee marks Global;
 *   ARG     returned, thrown-uncaught, stored into a non-global heap object, or passed to a
 *           non-CLEAN / bottom callee — the callee makes it caller-reachable;
 *   NONE    (CLEAN) none of the above — used purely locally.
 * Monotone: GLOBAL dominates ARG dominates NONE. */
static unsigned char cp_class_max(unsigned char a, unsigned char b) { return a > b ? a : b; }

static unsigned char cp_call_arg_class(cp_engine_t* eng, const sir_node_t* call, int o) {
    compiler_ctx_t* ctx = eng->ctx;
    unsigned char worst = COMPILER_ESC_NONE;
    /* An arg/receiver whose pts holds `o` contributes its callee-param classification. A
     * VIRTUAL/INTERFACE site has a target SET; without joining every target's summary here
     * it is conservatively a bottom callee (ARG). Special/static name one target. */
    switch (call->tag) {
    case SIR_INVOKESPECIAL: {
        int ce = ctx ? compiler_method_index(ctx, call->invoke_special.class_id,
                                             call->invoke_special.method_idx) : -1;
        if (cp_value_pts_has(eng, call->invoke_special.obj, o))
            worst = cp_class_max(worst, cp_callee_param_class(ctx, ce, true, -1));
        for (int i = 0; i < call->invoke_special.args_count; i++)
            if (cp_value_pts_has(eng, call->invoke_special.args[i], o))
                worst = cp_class_max(worst, cp_callee_param_class(ctx, ce, false, i));
        break;
    }
    case SIR_INVOKESTATIC: {
        int ce = ctx ? compiler_method_index(ctx, call->invoke_static.class_id,
                                             call->invoke_static.method_idx) : -1;
        for (int i = 0; i < call->invoke_static.args_count; i++)
            if (cp_value_pts_has(eng, call->invoke_static.args[i], o))
                worst = cp_class_max(worst, cp_callee_param_class(ctx, ce, false, i));
        break;
    }
    case SIR_INVOKEVIRTUAL:
        if (cp_value_pts_has(eng, call->invoke_virtual.obj, o)) worst = COMPILER_ESC_ARG;
        for (int i = 0; i < call->invoke_virtual.args_count; i++)
            if (cp_value_pts_has(eng, call->invoke_virtual.args[i], o)) worst = COMPILER_ESC_ARG;
        break;
    case SIR_INVOKEINTERFACE:
        if (cp_value_pts_has(eng, call->invoke_interface.obj, o)) worst = COMPILER_ESC_ARG;
        for (int i = 0; i < call->invoke_interface.args_count; i++)
            if (cp_value_pts_has(eng, call->invoke_interface.args[i], o)) worst = COMPILER_ESC_ARG;
        break;
    default: break;
    }
    int n = sir_arity(call);
    for (int i = 0; i < n; i++)
        worst = cp_class_max(worst, cp_call_arg_class(eng, sir_child(call, i), o));
    return worst;
}

static unsigned char cp_formal_classify(cp_engine_t* eng, int o) {
    if (o < 0) return COMPILER_ESC_NA;

    /* GLOBAL comes from the escape FIXPOINT, which already propagated transitively (a formal
     * stored into a local that later reaches a static is GlobalEscape here). A one-pass scan
     * cannot see that delayed escape — trusting the solved lattice is what makes GLOBAL
     * sound. A formal seeds ArgEscape, so `!= Global` is the real signal. */
    if (cp_escape_of(eng, o) == CP_ESC_GLOBAL) return COMPILER_ESC_GLOBAL;

    /* Not Global. It is ARG (not CLEAN) iff the callee makes it caller-reachable: stored
     * into ANY heap object (if that object reached Global the fixpoint above would have
     * caught it, so here it is at most Arg — conservatively ARG), returned, thrown-uncaught,
     * or passed to a non-CLEAN / bottom callee. Otherwise CLEAN — used purely locally. */
    unsigned char cls = COMPILER_ESC_NONE;
    for (int i = 0; i < eng->spine_count && cls < COMPILER_ESC_ARG; i++) {
        if (!cp_spine_reachable(eng, i)) continue;
        sir_node_t* n = eng->spine[i];
        switch (n->tag) {
        case SIR_RETURN:
            if (cp_value_pts_has(eng, n->return_.value, o)) cls = COMPILER_ESC_ARG;
            break;
        case SIR_THROW:
            if (!cp_throw_is_caught(eng, i, cp_vnode_of(eng, n->throw_.ref))
                && cp_value_pts_has(eng, n->throw_.ref, o))
                cls = COMPILER_ESC_ARG;
            break;
        /* Stored into the heap — ANY receiver. Not a receiver-escape test: the value becomes
         * reachable from the receiver, whose escape the caller's own analysis will decide. */
        case SIR_PUTFIELD:
            if (cp_value_pts_has(eng, n->put_field.value, o)) cls = COMPILER_ESC_ARG;
            break;
        case SIR_ARRAYSTORE:
            if (cp_value_pts_has(eng, n->array_store.value, o)) cls = COMPILER_ESC_ARG;
            break;
        case SIR_SETHEADER:
            if (cp_value_pts_has(eng, n->set_header.value, o)) cls = COMPILER_ESC_ARG;
            break;
        case SIR_PUTSTATIC:
            if (cp_value_pts_has(eng, n->put_static.value, o)) cls = COMPILER_ESC_ARG;
            break;
        default: break;
        }
        int ar = sir_arity(n);
        for (int j = 0; j < ar && cls < COMPILER_ESC_ARG; j++)
            if (cp_call_arg_class(eng, sir_child(n, j), o) > COMPILER_ESC_NONE)
                cls = COMPILER_ESC_ARG;
    }
    return cls;
}

/* Is `o` a cell phantom for an INSTANCE field / array element (not a static)? Cell phantoms are
 * numbered sequentially from obj_first_cell, so o's cell is (o - obj_first_cell). Such a phantom
 * is `Oext@(formal.f)` — reachable FROM A FORMAL, hence ArgEscape; the escape lattice seeds it
 * GlobalEscape only FAIL-CLOSED (its owner MIGHT be a global), which is a caller-side concern the
 * summary must not inherit — or every formal's field reads GlobalEscape and MapsTo over-escalates
 * every caller actual. A STATIC cell phantom, by contrast, IS genuinely global. */
static bool cp_obj_is_field_phantom(const cp_engine_t* eng, int o) {
    if (o < eng->obj_first_cell || o >= eng->obj_first_ret) return false;
    int c = o - eng->obj_first_cell;
    if (c < 0 || c >= eng->mem_cell_count) return false;
    uint32_t kind = eng->mem_cell_keys[c] & CP_CELL_KIND_MASK;
    return kind == CP_CELL_KIND_FIELD || kind == CP_CELL_KIND_ARRAY;
}

/* Storing a value INTO `o` confers GlobalEscape on that value (§6's heap rule) — `o` is genuinely
 * reachable from outside the method (a static, the catch-all, a native return, a finalizer), and
 * is NOT an instance-field/array cell phantom whose GlobalEscape is only the fail-closed seed. */
static bool cp_obj_is_global_container(const cp_engine_t* eng, int o) {
    return cp_escape_of(eng, o) == CP_ESC_GLOBAL && !cp_obj_is_field_phantom(eng, o);
}

static bool cp_pts_any_container(cp_engine_t* eng, cp_pts_t p) {
    if (!p.bits) return false;
    for (int w = 0; w < eng->obj_words; w++) {
        uint64_t word = p.bits[w];
        if (w == 0) word &= ~((uint64_t)1 << CP_OBJ_NULL);
        while (word) {
            int o = (w << 6) + __builtin_ctzll(word);
            word &= word - 1;
            if (cp_obj_is_global_container(eng, o)) return true;
        }
    }
    return false;
}

/* Did a recomputed summary CHANGE — Choi §4's convergence test. A NULL vs non-NULL array
 * (first computation) counts as a change; equal counts + byte-equal arrays do not. */
static bool cp_arr_differ(const void* a, const void* b, size_t bytes) {
    if (bytes == 0) return false;
    if (!a || !b) return a != b;
    return memcmp(a, b, bytes) != 0;
}
static bool cp_summary_differ(const compiler_summary_t* a, const compiler_summary_t* b) {
    if (a->computed != b->computed || a->this_escape != b->this_escape
        || a->ret_escape != b->ret_escape || a->slot_count != b->slot_count
        || a->this_obj != b->this_obj || a->n_obj != b->n_obj
        || a->n_edge != b->n_edge || a->n_wcell != b->n_wcell
        || a->ret_kind != b->ret_kind || a->ret_param != b->ret_param
        || a->ret_maybe_null != b->ret_maybe_null
        || a->ret_cstate != b->ret_cstate || a->ret_cwidth != b->ret_cwidth
        || a->ret_clo != b->ret_clo || a->ret_chi != b->ret_chi) return true;
    if (cp_arr_differ(a->slot_escape, b->slot_escape, (size_t)a->slot_count)) return true;
    if (cp_arr_differ(a->slot_obj, b->slot_obj, (size_t)a->slot_count * sizeof(int))) return true;
    if (cp_arr_differ(a->obj_escape, b->obj_escape, (size_t)a->n_obj)) return true;
    if (cp_arr_differ(a->edge_off, b->edge_off, (size_t)(a->n_obj + 1) * sizeof(int))) return true;
    if (cp_arr_differ(a->edge_key, b->edge_key, (size_t)a->n_edge * sizeof(unsigned int))) return true;
    if (cp_arr_differ(a->edge_dst, b->edge_dst, (size_t)a->n_edge * sizeof(int))) return true;
    if (cp_arr_differ(a->wcell_off, b->wcell_off, (size_t)(a->n_obj + 1) * sizeof(int))) return true;
    if (cp_arr_differ(a->wcell_key, b->wcell_key, (size_t)a->n_wcell * sizeof(unsigned int))) return true;
    if (cp_arr_differ(a->wcell_flags, b->wcell_flags, (size_t)a->n_wcell)) return true;
    if (cp_arr_differ(a->obj_leaked, b->obj_leaked, (size_t)a->n_obj * sizeof(bool))) return true;
    return false;
}

static void cp_summarize(compiler_ctx_t* ctx, int method_idx,
                         const sir_method_t* method, cp_engine_t* e) {
    if (!ctx || !e || method_idx < 0 || method_idx >= ctx->method_count) return;
    if (!ctx->summaries) {
        ctx->summaries = (compiler_summary_t*)bbq_arena_alloc(ctx->arena,
            (size_t)(ctx->method_count > 0 ? ctx->method_count : 1) * sizeof(compiler_summary_t));
        memset(ctx->summaries, 0,
            (size_t)(ctx->method_count > 0 ? ctx->method_count : 1) * sizeof(compiler_summary_t));
    }
    compiler_summary_t* sm = &ctx->summaries[method_idx];
    compiler_summary_t old = *sm;          /* Choi §4 convergence: compare after recompute */
    sm->computed   = true;
    sm->ret_escape = COMPILER_ESC_NA;      /* the pointer half (return pts) is Fig 7's, not built here */

    /* `this` is a formal ONLY for an instance method; a static method's obj_this is a spurious
     * seed nothing loads, so report NA there. The per-parameter arrays below are indexed by
     * PARAMETER POSITION, so the consumer maps arg i → param i with no `this`/slot arithmetic:
     * the ONE authority for a parameter's local slot is sema_param_slot (base = `this` ? 1 : 0),
     * not a `+1` open-coded here or in the consumer. */
    bool is_static = true;
    const sema_method_t* smeth = NULL;
    if (ctx->sema && method && method->class_id >= 0) {
        const sema_class_t* c = sema_get_class(ctx->sema, method->class_id);
        if (c && method->method_id >= 0 && method->method_id < (int)bbq_vec_len((void*)c->methods)) {
            smeth = &c->methods[method->method_id];
            is_static = (smeth->modifiers & ACC_STATIC) != 0;
        }
    }
    sm->this_escape = (is_static || e->obj_this < 0) ? COMPILER_ESC_NA
                        : cp_formal_classify(e, e->obj_this);

    /* Parameter phantom for param position `i`, or -1 if it is not a seeded ref (a primitive
     * param, or a ref never read). No sema ⟹ a hand-built method: fall back to raw slot = i. */
    int nparam = smeth ? smeth->param_count : e->slot_count;
    sm->slot_count  = nparam;
    sm->slot_escape = (unsigned char*)bbq_arena_alloc(ctx->arena, (size_t)(nparam > 0 ? nparam : 1));
    for (int i = 0; i < nparam; i++) {
        int slot = smeth ? sema_param_slot(smeth, i) : i;
        int ph = (e->obj_of_slot && slot >= 0 && slot < e->slot_count) ? e->obj_of_slot[slot] : -1;
        sm->slot_escape[i] = (ph >= 0) ? cp_formal_classify(e, ph) : COMPILER_ESC_NA;
    }

    /* §7.2 RETURN classification (the pointer half). FORMAL iff EVERY reachable `return` yields
     * the SAME formal (its phantom) — `return this` / `return arg`, possibly also null. Then a
     * caller aliases the result to that actual instead of the opaque `Oret`. Anything else keeps
     * UNKNOWN (Oret). Parameter index recovered via the same authority as slot_escape:
     * param i ↔ slot base+i, so a param phantom at slot s is param (s − base). */
    sm->ret_kind = COMPILER_RET_UNKNOWN;
    sm->ret_param = 0;
    sm->ret_maybe_null = false;
    {
        int base = is_static ? 0 : 1;
        int ret_formal = -2;      /* -2 unset, -1 this, ≥0 param, -3 disqualified */
        bool maybe_null = false, saw = false;
        for (int i = 0; i < e->spine_count && ret_formal != -3; i++) {
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            if (n->tag != SIR_RETURN || !n->return_.value) continue;
            int vi = cp_vnode_of(e, n->return_.value);
            if (vi < 0) { ret_formal = -3; break; }
            cp_pts_t p = e->vnodes[vi]->pts;
            if (!p.bits) continue;                 /* a non-ref / ∅-pts return — ignore */
            saw = true;
            int here = -2;                         /* the formal THIS return names */
            for (int w = 0; w < e->obj_words && here != -3; w++) {
                uint64_t word = p.bits[w];
                while (word) {
                    int o = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (o == CP_OBJ_NULL) { maybe_null = true; continue; }
                    int f = -3;
                    if (!is_static && o == e->obj_this) f = -1;
                    else for (int s = base; s < e->slot_count; s++)
                        if (e->obj_of_slot && e->obj_of_slot[s] == o) { f = s - base; break; }
                    if (f == -3) { here = -3; break; }        /* not a formal ⟹ disqualify */
                    if (here == -2) here = f;
                    else if (here != f) { here = -3; break; } /* two different formals */
                }
            }
            if (here == -2) continue;              /* only null this return — pins no formal */
            if (here == -3) { ret_formal = -3; break; }
            if (ret_formal == -2) ret_formal = here;
            else if (ret_formal != here) { ret_formal = -3; break; }
        }
        if (saw && ret_formal >= -1) {
            sm->ret_kind = COMPILER_RET_FORMAL;
            sm->ret_param = ret_formal;
            sm->ret_maybe_null = maybe_null;
        }
    }
    /* VFG Rule 1 return half / JLS §15.9.4 — a FRESH return: every reachable ref-`return` names only
     * concrete allocation SITES this method owns (`o >= obj_first_site`) and never null. The object
     * identity is NOT mintable at the caller (Obj naming is per-site), so it stays `Oret`; but a `new`
     * never returns null, and THAT the caller can use (drop the NPE on the result). Sound: one Oext /
     * phantom / null in any return ⟹ not fresh ⟹ UNKNOWN. */
    if (sm->ret_kind == COMPILER_RET_UNKNOWN) {
        bool fresh = true, saw2 = false;
        for (int i = 0; i < e->spine_count && fresh; i++) {
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            if (n->tag != SIR_RETURN || !n->return_.value) continue;
            int vi = cp_vnode_of(e, n->return_.value);
            if (vi < 0) { fresh = false; break; }
            cp_pts_t p = e->vnodes[vi]->pts;
            if (!p.bits) continue;                 /* non-ref / ∅-pts return — ignore */
            saw2 = true;
            for (int w = 0; w < e->obj_words && fresh; w++) {
                uint64_t word = p.bits[w];
                while (word) {
                    int o = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (o == CP_OBJ_NULL || o < e->obj_first_site) { fresh = false; break; }
                }
            }
        }
        if (saw2 && fresh) { sm->ret_kind = COMPILER_RET_FRESH; sm->ret_maybe_null = false; }
    }
    /* E1's TRANSITIVE half — not FRESH (the objects are a callee's `Oret`, not this method's
     * sites) but provably NEVER NULL: every reachable ref-return's pts excludes ⊥null. That is
     * the only fact the FRESH consumer ever used, so a caller drops the NPE on the result the
     * same way; identity stays Oret. Established across depth by the convergence loop:
     * pass 1 marks the leaf factory FRESH, pass 2 sees this method's return null-free through
     * cp_invoke_ret_fresh and lands here — `run(){ return m(); }` at any nesting. A retraction
     * (a later pass re-introducing ⊥null) flips the summary back and the loop re-runs until
     * stable, exactly the FORMAL/FRESH dynamics. */
    if (sm->ret_kind == COMPILER_RET_UNKNOWN) {
        bool nn = true, saw3 = false;
        for (int i = 0; i < e->spine_count && nn; i++) {
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            if (n->tag != SIR_RETURN || !n->return_.value) continue;
            int vi = cp_vnode_of(e, n->return_.value);
            if (vi < 0) { nn = false; break; }
            cp_pts_t p = e->vnodes[vi]->pts;
            if (!p.bits) continue;                 /* non-ref / ∅-pts return — ignore */
            saw3 = true;
            if (cp_pts_has(e, p, CP_OBJ_NULL)) nn = false;
        }
        if (saw3 && nn) { sm->ret_kind = COMPILER_RET_NONNULL; sm->ret_maybe_null = false; }
    }
    /* §7.2's VALUE half (lattice D made interprocedural, completing Click's
     * "combined" across the call graph): the MEET over every reachable numeric return's
     * SOLVED constant (the genuine fact, never a seed). EXPORTABLE facts only: KNOWN or
     * RANGE at i32/i64/f32/f64; a symbolic bound (`hi_vn1`) is a per-method vnode id and
     * is stripped by exporting the numeric lo/hi alone; stride drops to dense (a sound
     * superset); a REF const is a per-method site id and disqualifies; TOP/BOTTOM export
     * nothing. Floats export KNOWN by bit pattern (raw bits — §20.9.18 observability). */
    sm->ret_cstate = COMPILER_RETC_UNKNOWN;
    sm->ret_cwidth = 0;
    sm->ret_clo = 0;
    sm->ret_chi = 0;
    {
        cp_const_t acc = { .state = CP_C_TOP };
        bool sawc = false, okc = true;
        for (int i = 0; i < e->spine_count && okc; i++) {
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            if (n->tag != SIR_RETURN || !n->return_.value) continue;
            int vi = cp_vnode_of(e, n->return_.value);
            if (vi < 0) { okc = false; break; }
            cp_const_t c = e->vnodes[vi]->constant;
            if (c.state != CP_C_KNOWN && c.state != CP_C_RANGE) { okc = false; break; }
            sawc = true;
            acc = cp_const_meet(acc, c);           /* TOP is the meet identity */
        }
        if (sawc && okc && acc.state == CP_C_KNOWN) {
            sm->ret_cstate = COMPILER_RETC_KNOWN;
            sm->ret_cwidth = (unsigned char)acc.cwidth;
            switch (acc.cwidth) {
                case CP_W_I32: sm->ret_clo = acc.value;  break;
                case CP_W_I64: sm->ret_clo = acc.lvalue; break;
                case CP_W_F32: { uint32_t b; memcpy(&b, &acc.fvalue, sizeof b);
                                 sm->ret_clo = (int64_t)b; break; }
                case CP_W_F64: { uint64_t b; memcpy(&b, &acc.dvalue, sizeof b);
                                 sm->ret_clo = (int64_t)b; break; }
            }
            sm->ret_chi = sm->ret_clo;
        } else if (sawc && okc && acc.state == CP_C_RANGE
                   && acc.cwidth <= CP_W_I64) {    /* floats have no range lattice */
            sm->ret_cstate = COMPILER_RETC_RANGE;
            sm->ret_cwidth = (unsigned char)acc.cwidth;
            sm->ret_clo = acc.lo;
            sm->ret_chi = acc.hi;
        }
    }

    /* Fig-7 NonLocalGraph (Choi §4.2): the formal-reachable sub-graph, so a caller can follow
     * `O.f ↦ Ô.g` to a REACHABLE object's written cell (a deep `p.child.x`), not only a direct
     * `p.x`. Dense-number every object reachable from a formal over field edges; record each
     * one's escape, its field edges (read off the solved cell maps), and the cells written on
     * it. A NoEscape object is unreachable from a formal, so it never enters — matching Choi's
     * "NoEscape objects never appear in the summary" by construction. */
    int oc = e->obj_count;
    int* sid = NULL;                         /* eng obj -> summary id (bbq_vec, obj_count long) */
    for (int o = 0; o < oc; o++) bbq_vec_push(sid, -1);
    int* eobj = NULL;                        /* summary id -> eng obj */
    int* q    = NULL;                        /* BFS worklist of eng objs */
    int* ef_src = NULL; unsigned int* ef_key = NULL; int* ef_dst = NULL;   /* edges, parallel */
    /* §42 per-(engine obj, local cell) value flags — null/Oext seen in the SAME heap scan that
     * builds the edges, so a caller can tell a fully-captured write from one it must keep EXT for. */
    size_t cfsz = (size_t)oc * (e->mem_cell_count > 0 ? e->mem_cell_count : 1);
    unsigned char* cellflag = (unsigned char*)bbq_arena_alloc(e->arena, cfsz);  /* scratch, not persisted */
    memset(cellflag, 0, cfsz);
#define SG_SEED(O) do { int _o = (O); if (_o >= 0 && _o < oc && sid[_o] < 0) {          \
        sid[_o] = (int)bbq_vec_len(eobj); bbq_vec_push(eobj, _o); bbq_vec_push(q, _o); } } while (0)
    if (!is_static && e->obj_this >= 0) SG_SEED(e->obj_this);
    for (int s = 0; s < e->slot_count; s++) {
        int ph = e->obj_of_slot ? e->obj_of_slot[s] : -1;
        if (ph >= 0) SG_SEED(ph);
    }
    for (int qh = 0; qh < (int)bbq_vec_len(q); qh++) {
        int O = q[qh];
        for (int v = 0; v < e->mem_rows; v++) {
            int c = e->mem_cell[v];
            if (c < 0 || c >= e->mem_cell_count) continue;
            const cp_pts_t* h = e->vnodes[v]->heap;
            if (!h || !h[O].bits) continue;
            uint32_t key = e->mem_cell_keys[c];
            unsigned char* cf = &cellflag[(size_t)O * e->mem_cell_count + c];
            for (int w = 0; w < e->obj_words; w++) {
                uint64_t word = h[O].bits[w];
                if (w == 0) {
                    if (word & ((uint64_t)1 << CP_OBJ_NULL)) *cf |= COMPILER_WCELL_MAYBE_NULL;
                    word &= ~((uint64_t)1 << CP_OBJ_NULL);
                }
                while (word) {
                    int o2 = (w << 6) + __builtin_ctzll(word);
                    word &= word - 1;
                    if (o2 == CP_OBJ_EXT) continue;   /* the catch-all names no object to clobber */
                    SG_SEED(o2);
                    bbq_vec_push(ef_src, sid[O]);
                    bbq_vec_push(ef_key, key);
                    bbq_vec_push(ef_dst, sid[o2]);
                }
            }
        }
    }
#undef SG_SEED
    int nobj = (int)bbq_vec_len(eobj);
    sm->n_obj = nobj;
    if (nobj > 0) {
        /* written cells per summary object (a PutField/ArrayStore whose receiver pts names it). */
        int* wc_obj = NULL; unsigned int* wc_key = NULL; unsigned char* wc_flag = NULL;  /* parallel */
        /* §42 flag for (engine obj OBJ, cell KEY): its MAYBE_NULL bit from the edge scan (0 for a
         * cell not in this method's own table — the completeness guard is per-object obj_leaked). */
        #define WC_FLAG(OBJ, KEY) ({ int _c = (e->mem_cell_count > 0) ? cp_cell_lookup(e,(KEY)) : -1; \
            (unsigned char)((_c >= 0 && _c < e->mem_cell_count)                                       \
                ? cellflag[(size_t)(OBJ) * e->mem_cell_count + _c] : 0); })
        for (int i = 0; i < e->spine_count; i++) {
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            sir_node_t* recv = (n->tag == SIR_PUTFIELD)   ? n->put_field.obj
                             : (n->tag == SIR_ARRAYSTORE) ? n->array_store.arr
                             : (n->tag == SIR_ARRAYCOPY)  ? n->array_copy.dst : NULL;
            if (!recv) continue;
            uint32_t key = cp_cell_key_for_spine(n);   /* ARRAYCOPY writes the dest's element cell —
                                                        * capturing it keeps the clobber COMPLETE,
                                                        * which Gate 5's kill-narrowing depends on. */
            if (key == CP_CELL_ALL || key == CP_CELL_NONE) continue;
            int vi = cp_vnode_of(e, recv);
            if (vi < 0) continue;
            cp_pts_t p = e->vnodes[vi]->pts;
            if (!p.bits) continue;
            for (int si = 0; si < nobj; si++)
                if (cp_pts_has(e, p, eobj[si])) {
                    bbq_vec_push(wc_obj, si); bbq_vec_push(wc_key, key);
                    bbq_vec_push(wc_flag, WC_FLAG(eobj[si], key));
                }
        }
        /* TRANSITIVE (spec §7.2): the cells this method's CALLEES write on formal-reachable
         * objects. The solve already computed them — every MapsTo marked the callee's write set
         * into the clobber matrix (or the clobx overflow, for a cell this method never mentions)
         * — so this is a READOUT, never a second computation. Without it `f(p){ g(p); }` exports
         * "writes nothing", and a caller's kill preserves a NoEscape receiver's row that g
         * overwrote at runtime (§37f, the pts/nullability miscompile). */
        if (e->clobbered && e->mem_cell_count > 0) {
            for (int si = 0; si < nobj; si++) {
                int o2 = eobj[si];
                for (int c = 0; c < e->mem_cell_count; c++)
                    if (e->clobbered[(size_t)o2 * e->mem_cell_count + c]) {
                        bbq_vec_push(wc_obj, si);
                        bbq_vec_push(wc_key, e->mem_cell_keys[c]);
                        bbq_vec_push(wc_flag, COMPILER_WCELL_TRANSITIVE);  /* via a sub-call */
                    }
            }
        }
        /* …and the overflow pairs (deduped here — the sweeps re-record the same pair). */
        for (int x = 0; x < (int)bbq_vec_len(e->clobx_obj); x++) {
            int si = (e->clobx_obj[x] >= 0 && e->clobx_obj[x] < oc) ? sid[e->clobx_obj[x]] : -1;
            if (si < 0) continue;                        /* not formal-reachable: no summary row */
            bool dup = false;
            for (int j = 0; j < (int)bbq_vec_len(wc_obj) && !dup; j++)
                if (wc_obj[j] == si && wc_key[j] == e->clobx_key[x]) dup = true;
            if (!dup) { bbq_vec_push(wc_obj, si); bbq_vec_push(wc_key, e->clobx_key[x]);
                        bbq_vec_push(wc_flag, COMPILER_WCELL_TRANSITIVE); }  /* via a sub-call */
        }
        /* obj_escape (Choi §4.2 reachability, GENUINE sources only). Every sub-graph object is
         * ArgEscape by construction (formal-reachable); it is GlobalEscape only where the callee
         * GENUINELY leaks it — stored to a static, into a genuine-global container, a native
         * return / finalizer, or reachable from such via a field edge. Reading a formal's field
         * does NOT leak it. Using the raw cp_escape_of here is the bug that inherits the fail-
         * closed field-phantom seed, marking every formal's field GlobalEscape and over-escalating
         * every mapped caller actual. */
        sm->obj_escape = (unsigned char*)bbq_arena_alloc(ctx->arena, (size_t)nobj);
        for (int si = 0; si < nobj; si++) sm->obj_escape[si] = (unsigned char)CP_ESC_ARG;
        /* §42 completeness readout: object si was passed to a bottom method in THIS callee (its
         * own obj_bottom, computed for Gate 5) ⟹ a bottom sub-call could write its cells, so a
         * caller must keep CP_OBJ_EXT for them. */
        sm->obj_leaked = (bool*)bbq_arena_alloc(ctx->arena, (size_t)nobj * sizeof(bool));
        for (int si = 0; si < nobj; si++)
            sm->obj_leaked[si] = e->obj_bottom ? e->obj_bottom[eobj[si]] : false;
        int* gg = NULL; for (int si = 0; si < nobj; si++) bbq_vec_push(gg, 0);
        int* gwl = NULL;
        for (int si = 0; si < nobj; si++)                     /* the object is itself a container */
            if (cp_obj_is_global_container(e, eobj[si]) && !gg[si]) { gg[si] = 1; bbq_vec_push(gwl, si); }
        for (int i = 0; i < e->spine_count; i++) {            /* stores that confer GlobalEscape */
            if (!cp_spine_reachable(e, i)) continue;
            sir_node_t* n = e->spine[i];
            sir_node_t* val = NULL; bool confers = false;
            if (n->tag == SIR_PUTSTATIC) { val = n->put_static.value; confers = true; }
            else if (n->tag == SIR_PUTFIELD) { val = n->put_field.value;
                int rv = cp_vnode_of(e, n->put_field.obj);
                confers = rv >= 0 && cp_pts_any_container(e, e->vnodes[rv]->pts); }
            else if (n->tag == SIR_ARRAYSTORE) { val = n->array_store.value;
                int rv = cp_vnode_of(e, n->array_store.arr);
                confers = rv >= 0 && cp_pts_any_container(e, e->vnodes[rv]->pts); }
            if (!confers || !val) continue;
            int vv = cp_vnode_of(e, val); if (vv < 0) continue;
            cp_pts_t vp = e->vnodes[vv]->pts;
            for (int si = 0; si < nobj; si++)
                if (!gg[si] && cp_pts_has(e, vp, eobj[si])) { gg[si] = 1; bbq_vec_push(gwl, si); }
        }
        for (int gi = 0; gi < (int)bbq_vec_len(gwl); gi++) {  /* forward: a global's fields global */
            int k = gwl[gi], ne0 = (int)bbq_vec_len(ef_src);
            for (int i = 0; i < ne0; i++)
                if (ef_src[i] == k && ef_dst[i] >= 0 && ef_dst[i] < nobj && !gg[ef_dst[i]]) {
                    gg[ef_dst[i]] = 1; bbq_vec_push(gwl, ef_dst[i]); }
        }
        for (int si = 0; si < nobj; si++) if (gg[si]) sm->obj_escape[si] = (unsigned char)CP_ESC_GLOBAL;
        bbq_vec_free(gg); bbq_vec_free(gwl);
        sm->this_obj = (!is_static && e->obj_this >= 0) ? sid[e->obj_this] : -1;
        sm->slot_obj = (int*)bbq_arena_alloc(ctx->arena, (size_t)(nparam > 0 ? nparam : 1) * sizeof(int));
        for (int i = 0; i < nparam; i++) {          /* PARAMETER-indexed, via sema_param_slot */
            int slot = smeth ? sema_param_slot(smeth, i) : i;
            int ph = (e->obj_of_slot && slot >= 0 && slot < e->slot_count) ? e->obj_of_slot[slot] : -1;
            sm->slot_obj[i] = (ph >= 0) ? sid[ph] : -1;
        }
        /* edges → CSR by counting sort on the source object. */
        int ne = (int)bbq_vec_len(ef_src);
        sm->n_edge   = ne;
        sm->edge_off = (int*)bbq_arena_alloc(ctx->arena, (size_t)(nobj + 1) * sizeof(int));
        memset(sm->edge_off, 0, (size_t)(nobj + 1) * sizeof(int));
        for (int i = 0; i < ne; i++) sm->edge_off[ef_src[i] + 1]++;
        for (int k = 1; k <= nobj; k++) sm->edge_off[k] += sm->edge_off[k - 1];
        sm->edge_key = (unsigned int*)bbq_arena_alloc(ctx->arena, (size_t)(ne > 0 ? ne : 1) * sizeof(unsigned int));
        sm->edge_dst = (int*)bbq_arena_alloc(ctx->arena, (size_t)(ne > 0 ? ne : 1) * sizeof(int));
        int* cur = NULL; for (int k = 0; k < nobj; k++) bbq_vec_push(cur, sm->edge_off[k]);
        for (int i = 0; i < ne; i++) { int k = ef_src[i], pos = cur[k]++;
            sm->edge_key[pos] = ef_key[i]; sm->edge_dst[pos] = ef_dst[i]; }
        bbq_vec_free(cur);
        /* written cells → CSR by counting sort on the object. */
        int nw = (int)bbq_vec_len(wc_obj);
        sm->n_wcell   = nw;
        sm->wcell_off = (int*)bbq_arena_alloc(ctx->arena, (size_t)(nobj + 1) * sizeof(int));
        memset(sm->wcell_off, 0, (size_t)(nobj + 1) * sizeof(int));
        for (int i = 0; i < nw; i++) sm->wcell_off[wc_obj[i] + 1]++;
        for (int k = 1; k <= nobj; k++) sm->wcell_off[k] += sm->wcell_off[k - 1];
        sm->wcell_key = (unsigned int*)bbq_arena_alloc(ctx->arena, (size_t)(nw > 0 ? nw : 1) * sizeof(unsigned int));
        sm->wcell_flags = (unsigned char*)bbq_arena_alloc(ctx->arena, (size_t)(nw > 0 ? nw : 1));
        int* curw = NULL; for (int k = 0; k < nobj; k++) bbq_vec_push(curw, sm->wcell_off[k]);
        for (int i = 0; i < nw; i++) { int k = wc_obj[i], pos = curw[k]++;
            sm->wcell_key[pos] = wc_key[i]; sm->wcell_flags[pos] = wc_flag[i]; }
        bbq_vec_free(curw);
        bbq_vec_free(wc_obj); bbq_vec_free(wc_key); bbq_vec_free(wc_flag);
        #undef WC_FLAG
    }
    bbq_vec_free(sid); bbq_vec_free(eobj); bbq_vec_free(q);
    bbq_vec_free(ef_src); bbq_vec_free(ef_key); bbq_vec_free(ef_dst);

    if (cp_summary_differ(&old, sm)) {
        ctx->summary_changed = true;
        if (ctx->sum_changed && method_idx >= 0 && method_idx < ctx->method_count)
            ctx->sum_changed[method_idx] = true;
    }
}

/* Build + solve + summarize ONE method WITHOUT rewriting it — the summarize-only step Choi's
 * iterate-to-convergence loop runs over the call graph. Never mutates the SIR (no cp_rewrite /
 * cp_pack) and does not touch the census (that stays with sir_optimize's single rewrite pass). */
void sir_summarize(compiler_ctx_t* ctx, int method_idx) {
    if (!ctx) return;
    sir_method_t* method;
    const compiler_fact_t* facts;
    int fact_count;
    if (method_idx == SIR_OPT_CLINIT) {
        method = ctx->clinit; facts = ctx->clinit_facts; fact_count = ctx->clinit_fact_count;
    } else {
        if (method_idx < 0 || method_idx >= ctx->method_count) return;
        method = ctx->methods ? ctx->methods[method_idx] : NULL;
        facts  = compiler_get_facts(ctx, method_idx, &fact_count);
    }
    if (!method || !method->entry) return;
    /* Engine scratch goes in a PRIVATE arena freed right here — the summary (the only thing that
     * must persist) is copied into ctx->arena by cp_summarize. This is what keeps the convergence
     * loop's (mc+8)×methods engines from piling up in ctx->arena. cp_free first (releases the
     * malloc-backed vecs/hmaps/htrees/type_pool `e` points at), THEN the arena (releases `e` and
     * every arena allocation it made — vnodes, heaps, inject). */
    bbq_arena scratch;
    bbq_arena_init(&scratch, 1 << 18);
    cp_engine_t* e = cp_build_ctx_in(ctx, method, facts, fact_count, &scratch);
    if (e) { cp_summarize(ctx, method_idx, method, e); cp_free(e); }
    bbq_arena_free(&scratch);
}

/* Choi §4: iterate the reverse-topological summarize pass until no summary changes — so a
 * back-edge callee (a cycle) refines to a fixpoint instead of reading as a bottom method. Each
 * pass is SOUND on its own (every summary is a valid image of the current callee summaries), so
 * the guard is a pure termination backstop, not a soundness crutch: an acyclic graph converges in
 * one confirming pass, a cycle in a few. The caller then runs the REWRITE pass (sir_optimize)
 * once with the converged summaries. FORBIDDEN and not needed (Choi): SCC/Tarjan/dominators. */
void compiler_summarize_to_convergence(compiler_ctx_t* ctx) {
    if (!ctx) return;
    compiler_build_callgraph(ctx);
    int mc = ctx->method_count;
    int* order = (int*)bbq_arena_alloc(ctx->arena, (size_t)(mc > 0 ? mc : 1) * sizeof(int));
    int no = compiler_analysis_order(ctx, order);
    /* callee → CALLERS, inverted once from the callgraph CSR: a round re-summarizes
     * only the callers of methods whose summary moved last round. Same fixpoint as
     * the all-methods sweep (a summary is a deterministic monotone image of the SIR
     * + callee summaries, so an unmoved-callee method recomputes identically) —
     * without the dead re-solves, which were 2 of the 4 engine builds per method on
     * the jre (profile, 07-18). */
    int nm = mc > 0 ? mc : 1;
    int* cr_cnt = (int*)bbq_arena_alloc(ctx->arena, (size_t)nm * sizeof(int));
    int* cr_off = (int*)bbq_arena_alloc(ctx->arena, (size_t)nm * sizeof(int));
    memset(cr_cnt, 0, (size_t)nm * sizeof(int));
    for (int m = 0; m < mc; m++)
        for (int e = ctx->cg_off[m]; e < ctx->cg_off[m] + ctx->cg_cnt[m]; e++) {
            int cal = ctx->cg_edge[e];
            if (cal >= 0 && cal < mc) cr_cnt[cal]++;
        }
    int total = 0;
    for (int m = 0; m < mc; m++) { cr_off[m] = total; total += cr_cnt[m]; }
    int* cr_list = (int*)bbq_arena_alloc(ctx->arena,
                                         (size_t)(total > 0 ? total : 1) * sizeof(int));
    int* cur = (int*)bbq_arena_alloc(ctx->arena, (size_t)nm * sizeof(int));
    memset(cur, 0, (size_t)nm * sizeof(int));
    for (int m = 0; m < mc; m++)
        for (int e = ctx->cg_off[m]; e < ctx->cg_off[m] + ctx->cg_cnt[m]; e++) {
            int cal = ctx->cg_edge[e];
            if (cal >= 0 && cal < mc) cr_list[cr_off[cal] + cur[cal]++] = m;
        }

    bool* need = (bool*)bbq_arena_alloc(ctx->arena, (size_t)nm * sizeof(bool));
    memset(need, 1, (size_t)nm * sizeof(bool));       /* round 1: everyone */
    ctx->sum_changed = (bool*)bbq_arena_alloc(ctx->arena, (size_t)nm * sizeof(bool));
    int guard = mc + 8;
    bool moved;
    do {
        ctx->summary_changed = false;
        memset(ctx->sum_changed, 0, (size_t)nm * sizeof(bool));
        for (int oi = 0; oi < no; oi++)
            if (order[oi] >= 0 && order[oi] < mc && need[order[oi]])
                sir_summarize(ctx, order[oi]);
        memset(need, 0, (size_t)nm * sizeof(bool));
        moved = false;
        for (int m = 0; m < mc; m++) {
            if (!ctx->sum_changed[m]) continue;
            moved = true;
            for (int ci = cr_off[m]; ci < cr_off[m] + cr_cnt[m]; ci++)
                need[cr_list[ci]] = true;
        }
    } while (moved && --guard > 0);
    ctx->sum_changed = NULL;   /* disarm: the rewrite pass's summarize is not a round */
    if (moved)
        fprintf(stderr, "compiler: interprocedural summaries did not converge in %d passes "
                "(non-monotone?) — summaries are sound but may be imprecise\n", mc + 8);
}

void sir_optimize(compiler_ctx_t* ctx, int method_idx) {
    if (!ctx) return;
    /* Everything comes out of the ONE context — the method, the sema, the arena,
     * and the method's fact table (scopes, §15 guards, allocation sites, regions). */
    sir_method_t* method;
    const compiler_fact_t* facts;
    int fact_count;
    if (method_idx == SIR_OPT_CLINIT) {
        method     = ctx->clinit;
        facts      = ctx->clinit_facts;
        fact_count = ctx->clinit_fact_count;
    } else {
        if (method_idx < 0 || method_idx >= ctx->method_count) return;
        method = ctx->methods ? ctx->methods[method_idx] : NULL;
        facts  = compiler_get_facts(ctx, method_idx, &fact_count);
    }
    if (!method || !method->entry) return;
    const sema_ctx_t* sema = ctx->sema;
    bbq_arena* arena = ctx->arena;

    /* Snapshot frame size before any lift; cp_pack uses this as the
     * args_cells fallback when sema is NULL or the method's
     * registration can't be looked up. */
    int initial_max_locals = method->max_locals > 0 ? method->max_locals : 1;
    const sema_class_t* dump_cls = (sema && method->class_id >= 0)
        ? sema_get_class(sema, method->class_id) : NULL;
    const char* dump_cn = dump_cls ? dump_cls->name : NULL;
    cp_debug_dump_spine(method, dump_cn, "pre");

    cp_engine_t* e = cp_build_ctx(ctx, method, facts, fact_count);
    if (e && getenv("JAVELINA_DUMP_PHIS") && method->name
          && strstr(method->name, getenv("JAVELINA_DUMP_PHIS"))) {
        for (int v = 0; v < e->vnode_count; v++) {
            cp_vnode_t* vn = e->vnodes[v];
            if (vn->kind != CP_VN_PHI) continue;
            fprintf(stderr, "[phi] vn%d merge=%p slot=%d cell=%d part=%d leader=%d type=%d "
                            "const{st=%d v=%d lo=%lld hi=%lld} ins:",
                    v, (void*)vn->phi_merge, vn->phi_slot, vn->phi_cell,
                    vn->partition, vn->leader,
                    vn->type ? (int)vn->type->kind : -1,
                    (int)vn->constant.state, vn->constant.value,
                    (long long)vn->constant.lo, (long long)vn->constant.hi);
            for (int k = 0; k < vn->input_count; k++)
                fprintf(stderr, " vn%d/p%d", vn->inputs[k],
                        vn->inputs[k] >= 0 ? e->vnodes[vn->inputs[k]]->partition : -1);
            fprintf(stderr, "\n");
        }
        /* The refine view, same gate: each Refine's input chain, predicate, and its
         * SOLVED constant — the wiring pass-B installed and what the fixpoint made of it. */
        for (int v = 0; v < e->vnode_count; v++) {
            cp_vnode_t* vn = e->vnodes[v];
            if (vn->kind != CP_VN_REFINE) continue;
            const cp_const_t* p = &vn->refine_predicate;
            const cp_const_t* c = &vn->constant;
            fprintf(stderr, "[refine] vn%d in=vn%d pts=%d pred{st=%d lo=%lld hi=%lld hi_vn1=%d lo_vn1=%d} "
                            "const{st=%d v=%d lo=%lld hi=%lld}\n",
                    v, vn->input_count >= 1 ? vn->inputs[0] : -1, (int)vn->refine_pts,
                    (int)p->state, (long long)p->lo, (long long)p->hi, p->hi_vn1, p->lo_vn1,
                    (int)c->state, c->value, (long long)c->lo, (long long)c->hi);
        }
        /* …every ArrayLength's follower linkage: its leader chain and solved const (§10.7's
         * `(new T[k]).length ≡ k` identity — a wrong leader here folds .length wrongly). */
        for (int v = 0; v < e->vnode_count; v++) {
            cp_vnode_t* vn = e->vnodes[v];
            if (vn->kind != CP_VN_EXPR || !vn->expr || vn->expr->tag != SIR_ARRAYLENGTH) continue;
            int ld = vn->leader;
            fprintf(stderr, "[arraylen] vn%d part=%d leader=vn%d(kind=%d exprtag=%d "
                            "ldconst{st=%d v=%d}) const{st=%d v=%d lo=%lld hi=%lld}\n",
                    v, vn->partition, ld,
                    ld >= 0 ? (int)e->vnodes[ld]->kind : -1,
                    ld >= 0 && e->vnodes[ld]->expr ? (int)e->vnodes[ld]->expr->tag : -1,
                    ld >= 0 ? (int)e->vnodes[ld]->constant.state : -1,
                    ld >= 0 ? e->vnodes[ld]->constant.value : 0,
                    (int)vn->constant.state, vn->constant.value,
                    (long long)vn->constant.lo, (long long)vn->constant.hi);
        }
        /* …every NewArray's size operand and its solved const (who feeds the allocation). */
        for (int v = 0; v < e->vnode_count; v++) {
            cp_vnode_t* vn = e->vnodes[v];
            if (vn->kind != CP_VN_EXPR || !vn->expr || vn->expr->tag != SIR_NEWARRAY) continue;
            int sz = vn->input_count >= 1 ? vn->inputs[0] : -1;
            const cp_const_t* sc2 = sz >= 0 ? &e->vnodes[sz]->constant : NULL;
            fprintf(stderr, "[newarray] vn%d size=vn%d szconst{st=%d v=%d lo=%lld hi=%lld} "
                            "self{st=%d}\n",
                    v, sz, sc2 ? (int)sc2->state : -1, sc2 ? sc2->value : 0,
                    sc2 ? (long long)sc2->lo : 0, sc2 ? (long long)sc2->hi : 0,
                    (int)vn->constant.state);
        }
        /* …and every LOADLOCAL whose input is a Refine (the rewired loads). */
        for (int v = 0; v < e->vnode_count; v++) {
            cp_vnode_t* vn = e->vnodes[v];
            if (vn->kind != CP_VN_EXPR || !vn->expr || vn->expr->tag != SIR_LOADLOCAL) continue;
            if (vn->input_count < 1 || vn->inputs[0] < 0) continue;
            if (e->vnodes[vn->inputs[0]]->kind != CP_VN_REFINE) continue;
            fprintf(stderr, "[load-ref] vn%d slot=%d sp=%d -> vn%d const{st=%d v=%d}\n",
                    v, vn->expr->load_local.slot, vn->parent_spine, vn->inputs[0],
                    (int)vn->constant.state, vn->constant.value);
        }
    }
    if (e) {
        cp_rewrite(e);
        ctx->devirt_total += e->devirt_count;   /* the census reads it off the context */
        ctx->scalar_total += e->scalar_count;
        /* §6's yield, on the SAME census — how many allocation SITES this method proved
         * method-local. Counted over the local sites (the phantoms and `Oret`s are external
         * by construction and are not allocations of ours to remove).
         *
         * ONLY SITES THE OPTIMIZED GRAPH STILL ALLOCATES (fixed 07-14). The first cut
         * counted every site, and 947 of the jre's 959 "NoEscape" sites turned out to be
         * the exception objects of guard arms the solve had PROVEN DEAD — the class
         * histogram was the guard census itself (587 NullPointerException, 46
         * ArithmeticException, 36 NegativeArraySizeException). A dead region's values hold
         * ∅ pts, so nothing ever lowers those objects out of ⊤/NoEscape. Reporting them as
         * allocations §6 could remove double-counts a win §15's guard elimination already
         * took, and it made a 6-site opportunity read as a 959-site mandate. */
        for (int o = e->obj_first_site; o < e->obj_count; o++) {
            if (e->obj_alloced && !e->obj_alloced[o]) continue;
            ctx->alloc_total++;
            if (cp_escape_of(e, o) != CP_ESC_NONE) continue;
            ctx->noescape_total++;
            int av = e->vnode_of_obj[o];
            const sir_node_t* an = (av >= 0) ? e->vnodes[av]->expr : NULL;
            if (an && an->tag == SIR_NEW)            ctx->noescape_struct++;
            else if (an && (an->tag == SIR_NEWARRAY
                         || an->tag == SIR_NEWREFARRAY)) ctx->noescape_array++;
        }
        cp_summarize(ctx, method_idx, method, e);
        cp_free(e);
    }
    cp_debug_dump_spine(method, dump_cn, "mid");   /* post-rewrite, PRE-pack slot numbers */
    cp_pack(method, sema, arena, initial_max_locals);

    cp_debug_dump_spine(method, dump_cn, "post");

}

