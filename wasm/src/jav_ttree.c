/*
 * jav_ttree.c — one forward pass from a function body to the trees burg tiles.
 *
 * Ertl §3.2.1: push a NODE POINTER onto the abstract stack instead of the value.
 * The operand stack then disappears from the compiled code — what is left is the
 * producer/consumer edge, which is the thing a tiling needs. Wasm has no
 * dup/over/swap/rot, so each value is consumed exactly once and each region's
 * result is a true tree.
 *
 * The walk carries §7.6's SHAPE — the operand stack and the control frames — and
 * nothing else. It re-checks no types: the module is already §7-valid, so the
 * shape is the only thing this pass has to keep, and anything it cannot answer is
 * a decline (the caller keeps the tier it had), never a diagnosis.
 */
#include "jav_ttree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jav_jit_meta.h"   /* jav_jit_meta[] — the SAME operand table tier-1 decodes with */
#include "opcodes.h"

/* Prefixed opcodes whose stack effect is not what their signature declares:
 * the GC constructors take a runtime-counted argument group, and br_on_cast
 * leaves its typing to the native. §5.4.6. */
#define SUB_STRUCT_NEW        0
#define SUB_ARRAY_NEW_FIXED   8
#define SUB_BR_ON_CAST       24
#define SUB_BR_ON_CAST_FAIL  25
#define PFX_GC             0xFB

/* One entry of the abstract stack: the node that produced the value, and the
 * class it is held in (which the node alone does not always give — a carried
 * leaf's class comes from the value it stands for). */
typedef struct { jav_tnode_t* node; uint8_t cls; } tval_t;

/* §7.6 a control frame. `height` is the operand height the frame's label resets
 * to; the param/result classes are the blocktype's, which is what `else` and
 * `end` put back on the stack. */
typedef struct {
    uint8_t        op;
    uint32_t       height;
    const uint8_t* param_class;   uint32_t nparams;
    const uint8_t* result_class;  uint32_t nresults;
    uint8_t        unreachable;
    uint8_t        entry_unreachable;   /* what `else` restores the arm to */
} tframe_t;

typedef struct {
    const jav_tctx_t* ctx;
    bbq_arena*        arena;
    tval_t*           stack;    uint32_t nstack;
    tframe_t*         frames;   uint32_t nframes;
    jav_tnode_t**     roots;    uint32_t nroots;     /* every region's, back to back */
    jav_tregion_t*    regions;  uint32_t nregions;
    uint32_t          region_first_root;
    uint32_t          nnodes;
    uint32_t          ncarried;
    uint32_t          arity_mismatches;
    uint32_t          order_breaks;
    uint8_t           cur_op,  decline_op;
    uint32_t          cur_sub, decline_sub;
    /* The flat §5 opcode ordinal of the instruction being built — §7.6's own unit,
     * "the flat sequence of opcodes as occurring in the binary format". Counted here
     * because this walk decodes that sequence directly; `end` and `else` are opcodes
     * in it and are counted like any other. */
    uint32_t          seq;
    int               ok;
} builder_t;

/* PIN B-4's accounting. A sweep over a corpus needs to say how much it built,
 * not only that nothing was wrong: an invariant over nothing holds trivially. */
static jav_ttree_stats_t g_stats;
const jav_ttree_stats_t* jav_ttree_stats(void) { return &g_stats; }
void jav_ttree_stats_reset(void) { memset(&g_stats, 0, sizeof g_stats); }

/* The offset-keyed tile map lived here: `jav_tile_pick` recorded a cache state
 * per byte offset and the byte-driven stamping walk read it back. It retired with
 * cosmic Amendment #16 — the rule action IS the stamp now (`jav_t2_stamp`, in the
 * driver that owns the emission context), so there is nothing to record and
 * nothing to join. The byte walk survives as tier-1 and the decline fallback,
 * where every stencil is the plain form and no state exists to look up. */

void jav_ttree_note_picks(uint32_t picked, uint32_t nodes) {
    g_stats.nodes_picked += picked;
    /* A carried leaf carries no instruction: nothing stamps it, so nothing picks
     * it. Everything else should have had a rule fire. */
    g_stats.nodes_unpicked += (nodes > picked) ? (nodes - picked) : 0;
}

void jav_ttree_note_code(uint64_t bytes) {
    g_stats.code_bytes += bytes;
    g_stats.code_bodies++;
}

/* The emission notes write through a SINK, because an emission can be abandoned:
 * a walk that fails mid-body re-stamps through the other one, and meters that
 * counted the abandoned half describe code that never shipped — which is what
 * the old silent retry did to every number it touched. The driver points the
 * sink at a per-compile accumulator and commits it only when that walk's output
 * is the one kept. */
static jav_ttree_stats_t* g_sink = &g_stats;

void jav_ttree_stats_sink(jav_ttree_stats_t* s) { g_sink = s ? s : &g_stats; }

void jav_ttree_stats_commit(const jav_ttree_stats_t* d) {
    g_stats.states_cached  += d->states_cached;
    g_stats.states_deep    += d->states_deep;
    g_stats.slots_cached   += d->slots_cached;
    g_stats.transitions    += d->transitions;
    g_stats.bridge_fails   += d->bridge_fails;
    g_stats.trans_spill    += d->trans_spill;
    g_stats.trans_fill     += d->trans_fill;
    g_stats.trans_boundary += d->trans_boundary;
    g_stats.mem_slots      += d->mem_slots;
    for (int i = 0; i < 9; i++) g_stats.entry_state[i] += d->entry_state[i];
    g_stats.wide_cached    += d->wide_cached;
    g_stats.regions_hot    += d->regions_hot;
}

void jav_ttree_note_stitch(int entry, int transitions, int bridge_failed) {
    if (entry > 0) g_sink->states_cached++;
    if (entry > 1) g_sink->states_deep++;
    g_sink->slots_cached += (uint64_t)(entry > 0 ? entry : 0);
    g_sink->transitions += (uint64_t)transitions;
    g_sink->bridge_fails += (uint64_t)(bridge_failed != 0);
    int b = entry < 0 ? 0 : entry;
    if (b > 8) b = 8;
    g_sink->entry_state[b]++;
}

void jav_ttree_note_wide(void) { g_sink->wide_cached++; }

void jav_ttree_note_fallback(uint8_t op, uint32_t bpos, int entry, int why) {
    g_stats.tree_fallbacks++;
    if (g_stats.have_fallback) return;
    g_stats.have_fallback = 1;
    g_stats.first_fallback_op = op;
    g_stats.first_fallback_bpos = bpos;
    g_stats.first_fallback_entry = entry;
    g_stats.first_fallback_why = why;
}

void jav_ttree_note_region_entry(int live_state) {
    if (live_state > 0) g_sink->regions_hot++;
}

void jav_ttree_note_mem(int slots) {
    if (slots > 0) g_sink->mem_slots += (uint64_t)slots;
}

void jav_ttree_note_transition(int down, int boundary) {
    if (down) g_sink->trans_spill++; else g_sink->trans_fill++;
    if (boundary) g_sink->trans_boundary++;
}

void jav_ttree_note_unbridged(uint8_t op, uint32_t off, int from, int to, int cls) {
    if (g_stats.have_unbridged) return;
    g_stats.have_unbridged = 1;
    g_stats.first_unbridged_op = op;
    g_stats.first_unbridged_off = off;
    g_stats.first_unbridged_from = from;
    g_stats.first_unbridged_to = to;
    g_stats.first_unbridged_cls = cls;
}

void jav_ttree_note_cover(int covered, int sig) {
    if (covered) { g_stats.bodies_covered++; return; }
    g_stats.bodies_uncovered++;
    if (!g_stats.have_uncovered) { g_stats.have_uncovered = 1; g_stats.first_uncovered_sig = sig; }
}

static void decline_at(builder_t* b, uint8_t op, uint32_t sub) {
    if (!b->ok) return;
    b->ok = 0;
    b->decline_op = op; b->decline_sub = sub;
}
static void decline(builder_t* b) { decline_at(b, b->cur_op, b->cur_sub); }

/* ── the class vocabulary at the edges ─────────────────────── */

/* A one-class array to point a blocktype's single result at. */
static const uint8_t kClass[JAV_SCLASS_FINAL] = {
    JSC_I32, JSC_I64, JSC_F32, JSC_F64, JSC_V128, JSC_REF, JSC_STK
};

/* §5.3.5 a valtype's encoding, as it appears in a blocktype's s33. The five
 * number types are their own classes; every other negative code in the range is
 * one of the reference forms, which share one class. Fail-closed: a code this
 * build does not know is a fact the walk is short, so it declines. */
static int blocktype_valtype_class(int32_t v, uint8_t* out) {
    switch (v) {
    case -1: *out = JSC_I32;  return 1;   /* 0x7F */
    case -2: *out = JSC_I64;  return 1;   /* 0x7E */
    case -3: *out = JSC_F32;  return 1;   /* 0x7D */
    case -4: *out = JSC_F64;  return 1;   /* 0x7C */
    case -5: *out = JSC_V128; return 1;   /* 0x7B */
    default: break;
    }
    /* §5.3.4 heaptype/reftype shorthands (0x73..0x63 → -13..-29) plus the
     * explicit (ref null? ht) forms; all of them are held as one handle. */
    if (v <= -13 && v >= -29) { *out = JSC_REF; return 1; }
    return 0;
}

/* §5.3.6 blocktype: 0x40 (-64) is empty, a negative valtype is one result and no
 * params, a non-negative index names a functype. */
static int blocktype_shape(builder_t* b, int32_t bt,
                           const uint8_t** pc, uint32_t* np,
                           const uint8_t** rc, uint32_t* nr) {
    *pc = NULL; *np = 0; *rc = NULL; *nr = 0;
    if (bt == -64) return 1;
    if (bt < 0) {
        uint8_t c;
        if (!blocktype_valtype_class(bt, &c)) return 0;
        *rc = &kClass[c]; *nr = 1;        /* kClass is indexed BY class: kClass[c]==c */
        return 1;
    }
    if ((uint32_t)bt >= b->ctx->ntypes) return 0;
    *pc = b->ctx->type_param_class[bt];  *np = b->ctx->type_nparams[bt];
    *rc = b->ctx->type_result_class[bt]; *nr = b->ctx->type_nresults[bt];
    return 1;
}

/* ── the abstract stack ────────────────────────────────────── */

static void push_val(builder_t* b, jav_tnode_t* n, uint8_t cls) {
    b->stack[b->nstack].node = n;
    b->stack[b->nstack].cls = cls;
    b->nstack++;
}

static int pop_val(builder_t* b, tval_t* out) {
    tframe_t* f = &b->frames[b->nframes - 1];
    if (b->nstack <= f->height) return 0;   /* §7.6 below the label: not ours to take */
    *out = b->stack[--b->nstack];
    return 1;
}

/* §3.3 A value that outlives its region is on the memory stack when the next one
 * starts. It cannot borrow a producer's terminal — the stitcher would stamp that
 * instruction again — so it becomes the leaf the vocabulary carries for it. */
static jav_tnode_t* carried_leaf(builder_t* b, uint8_t cls) {
    /* stands for a stack slot the region opened on, not an instruction */
    jav_tnode_t* n = (jav_tnode_t*)bbq_arena_alloc(b->arena, sizeof *n);
    if (!n) { decline(b); return NULL; }
    memset(n, 0, sizeof *n);
    n->sig = jav_carried_sig[cls];
    n->seq = JAV_TNODE_NO_SEQ;
    /* Already on the memory stack and computing nothing, so it needs room for
     * itself and no more. */
    n->need = cls < 7 ? jav_class_width[cls] : 1;
    b->nnodes++;
    b->ncarried++;
    return n;
}

static void add_root(builder_t* b, jav_tnode_t* n) { b->roots[b->nroots++] = n; }

static int is_carried(const tval_t* v) {
    return v->cls < JAV_SCLASS_FINAL - 1 && v->node->sig == jav_carried_sig[v->cls];
}

/* §3.3 Everything below the top `keep` values was consumed by nothing in this
 * region, so it is a root of its own — in stack order — and the value it leaves
 * behind is on the memory stack from here on. Called BEFORE the instruction that
 * closes the region builds its node, because that is where these ran: a root list
 * is emitted in order, and a value left on the stack was computed before the
 * branch, not after it. A leaf carried in is already where it belongs and
 * computes nothing, so it is a root of nothing. */
static void flush_below(builder_t* b, uint32_t keep) {
    uint32_t n = b->nstack > keep ? b->nstack - keep : 0;
    for (uint32_t i = 0; i < n; i++) {
        if (is_carried(&b->stack[i])) continue;
        add_root(b, b->stack[i].node);
        b->stack[i].node = carried_leaf(b, b->stack[i].cls);
    }
}

static void close_region(builder_t* b, uint32_t start) {
    flush_below(b, 0);
    jav_tregion_t* r = &b->regions[b->nregions++];
    r->roots = &b->roots[b->region_first_root];
    r->nroots = b->nroots - b->region_first_root;
    r->start = start;
    b->region_first_root = b->nroots;
}

/* ── resolution (§3.2's resolve()) ─────────────────────────── */

/* The signature this instance of the opcode actually has. The declared form's
 * resolution list holds every signature it can become; the class vectors pick
 * one. A miss is a decline, never a guess. */
static int resolve_sig(uint16_t declid, const uint8_t* pcls, const uint8_t* rcls) {
    const jav_sig_t* d = &jav_sigtab[declid];
    if (d->final) return declid;
    for (uint16_t k = 0; k < d->nresolve; k++) {
        const jav_sig_t* r = &jav_sigtab[d->resolves_to[k]];
        int hit = 1;
        for (uint8_t i = 0; i < d->nparams && hit; i++)  hit = (r->params[i] == pcls[i]);
        for (uint8_t i = 0; i < d->nresults && hit; i++) hit = (r->results[i] == rcls[i]);
        if (hit) return (int)d->resolves_to[k];
    }
    return -1;
}

/* §2.3.11 an ADDR slot is as wide as the addrtype of the entry it addresses, and
 * a slot naming two entries is 64-bit only when both are. */
static int addr_class(builder_t* b, const jav_opcode_sig_t* row,
                      const uint64_t* ent, uint8_t mask, uint8_t* out) {
    int wide = 1;
    for (uint8_t a = 0; a < row->natoms; a++) {
        if (!(mask & (1u << a))) continue;
        const jav_addr_atom_t* at = &row->atoms[a];
        uint32_t idx = (uint32_t)ent[at->operand];
        const uint8_t* flags; uint32_t n;
        /* The predicate is the spec's own — `mem_is64` / `table_is64` — so which
         * index space it reads is read off the name the spec chose, not guessed. */
        if (strncmp(at->pred, "mem", 3) == 0) { flags = b->ctx->mem_is64;   n = b->ctx->nmems; }
        else                                  { flags = b->ctx->table_is64; n = b->ctx->ntables; }
        if (idx >= n) return 0;
        wide &= flags[idx] != 0;
    }
    *out = wide ? JSC_I64 : JSC_I32;
    return 1;
}

/* The class a polymorphic RESULT takes when no operand of the same instruction
 * carries it. Each is one of §3.4's typing rules, read off the module the
 * instruction names. Everything not listed resolves from its operands and never
 * reaches here. */
static int poly_result_class(builder_t* b, uint8_t op, uint32_t sub,
                             const uint64_t* ent, uint8_t* out) {
    const jav_tctx_t* c = b->ctx;
    uint32_t i0 = (uint32_t)ent[0];
    switch (op) {
    case OP_LOCAL_GET:                                   /* §3.4.2 C.locals[x] */
        if (i0 >= c->nlocals) return 0;
        *out = c->local_class[i0]; return 1;
    case OP_GLOBAL_GET:                                  /* §3.4.3 C.globals[x] */
        if (i0 >= c->nglobals) return 0;
        *out = c->global_class[i0]; return 1;
    case OP_TABLE_GET:                                   /* §3.4.4 the table's reftype */
        *out = JSC_REF; return 1;
    case OP_MEMORY_SIZE: case OP_MEMORY_GROW:            /* §3.4.5 at -> at */
        if (i0 >= c->nmems) return 0;
        *out = c->mem_is64[i0] ? JSC_I64 : JSC_I32; return 1;
    default: break;
    }
    if (op == 0xFC) {                                    /* §5.4.4/§5.4.5 misc */
        if (sub == 15 || sub == 16) {                    /* table.grow / table.size */
            if (i0 >= c->ntables) return 0;
            *out = c->table_is64[i0] ? JSC_I64 : JSC_I32; return 1;
        }
        return 0;
    }
    if (op == PFX_GC) {                                  /* §3.4.6 aggregates */
        switch (sub) {
        case 2:                                          /* struct.get: the field's type */
            if (i0 >= c->ntypes || (uint32_t)ent[1] >= c->nfields[i0]) return 0;
            *out = c->field_class[i0][(uint32_t)ent[1]]; return 1;
        case 11:                                         /* array.get: the element's type */
            if (i0 >= c->ntypes) return 0;
            *out = c->elem_class[i0]; return 1;
        case 0: case 1: case 6: case 7: case 8: case 9: case 10:
            *out = JSC_REF; return 1;                    /* the constructors mint a reference */
        default: return 0;
        }
    }
    return 0;
}

/* ── the per-instruction step ──────────────────────────────── */

/* How many operands a variadic argument group takes off the abstract stack, and
 * — for the calls — the results the callee leaves in their place. Neither is in
 * the opcode's signature: the group's size is a property of the entry the
 * instruction names, which is exactly why opgen keeps those operands on the
 * value stack instead of pretending to a fixed arity. */
static int variadic_shape(builder_t* b, uint8_t op, uint32_t sub, const uint64_t* imm,
                          uint32_t* npop, const uint8_t** rcls, uint32_t* nres) {
    const jav_tctx_t* c = b->ctx;
    uint32_t i0 = (uint32_t)imm[0];
    uint32_t t;
    *npop = 0; *rcls = NULL; *nres = 0;
    switch (op) {
    case OP_CALL: case OP_RETURN_CALL:
        if (i0 >= c->nfuncs) return 0;
        t = c->func_type_idx[i0]; break;
    case OP_CALL_INDIRECT: case OP_RETURN_CALL_INDIRECT:
    case OP_CALL_REF:      case OP_RETURN_CALL_REF:
        t = i0; break;
    case OP_THROW:
        if (i0 >= c->ntags) return 0;
        t = c->tag_type_idx[i0]; break;
    default:
        if (op == PFX_GC && sub == SUB_STRUCT_NEW) {
            if (i0 >= c->ntypes) return 0;
            *npop = c->nfields[i0]; return 1;
        }
        if (op == PFX_GC && sub == SUB_ARRAY_NEW_FIXED) { *npop = (uint32_t)imm[1]; return 1; }
        return 0;
    }
    if (t >= c->ntypes) return 0;
    *npop = c->type_nparams[t];
    *rcls = c->type_result_class[t]; *nres = c->type_nresults[t];
    return 1;
}

/* Build the node for one instruction and apply its stack effect. */
static void build_node(builder_t* b, uint8_t op, uint32_t sub,
                       const jav_opcode_sig_t* row, const uint64_t* ent,
                       const uint8_t* pc, int cut) {
    const jav_sig_t* d = &jav_sigtab[row->sig];
    uint8_t pcls[JAV_SIG_MAX_PARAMS], rcls[JAV_SIG_MAX_RESULTS];
    tval_t kid[JAV_SIG_MAX_PARAMS];
    uint32_t vfirst = 0;

    /* §3.2 pop right to left: the last operand is on top. A variadic slot takes
     * its group off the stack as roots — those values go through memory, which
     * is how opgen already marshals them. */
    uint32_t vpop = 0; const uint8_t* vres = NULL; uint32_t nvres = 0;
    if (row->variadic >= 0 && !variadic_shape(b, op, sub, ent, &vpop, &vres, &nvres)) {
        decline(b); return;
    }
    // Anything still on the stack UNDER this instruction's own operands has to go
    // to memory before a node that will become a root — otherwise it belongs to a
    // tree that has not been built yet while this tree is already finished, and
    // the two trees interleave in the byte stream. A cover is chosen per tree, so
    // interleaved trees are two independent choices about the same registers at
    // the same program points: `c1; c2; drop; drop` builds drop(c2) and drop(c1),
    // both covers put their leaf in r0, and at run time c2 overwrites c1.
    //
    // Flushing here makes the roots' concatenated postorder EQUAL byte order,
    // which is the invariant that lets a per-tree cover be globally sound. It
    // costs nothing on the trees that matter: `local.get; i32.const; i32.add;
    // local.set` has nothing below local.set's operand, so nothing is flushed.
    // `vpop` matters for the same reason: a variadic instruction turns its
    // argument group into roots on the spot and is then PUSHED, so it joins a
    // later tree — and anything still under those args would be stranded beneath
    // a root, in a tree built afterwards. `const; const; array.new_fixed;
    // table.set` is the shape: the second const becomes a root, the first ends
    // up inside table.set's tree, and the roots then run backwards.
    if (cut || d->nresults == 0 || vpop) flush_below(b, d->nkids + vpop);
    for (int i = (int)d->nparams - 1; i >= 0; i--) {
        if (d->params[i] == JSC_STK) {
            for (uint32_t k = 0; k < vpop; k++) {
                tval_t v;
                if (!pop_val(b, &v)) { decline(b); return; }
            }
            vfirst = b->nstack;   /* the group now sits just above the new top */
            kid[i].node = NULL; kid[i].cls = JSC_STK;
            pcls[i] = JSC_STK;
            continue;
        }
        if (!pop_val(b, &kid[i])) { decline(b); return; }
        if (d->params[i] == JSC_ADDR) {
            if (!addr_class(b, row, ent, row->param_atoms[i], &pcls[i])) { decline(b); return; }
        } else if (d->params[i] == JSC_POLY) {
            pcls[i] = kid[i].cls;      /* the value on the stack knows what it is */
        } else {
            pcls[i] = d->params[i];
        }
    }
    /* The variadic group's members are consumed through memory, so they are roots
     * of this region — in stack order, ahead of the instruction that reads them. */
    for (uint32_t k = 0; k < vpop; k++) {
        tval_t* v = &b->stack[vfirst + k];
        if (!is_carried(v)) add_root(b, v->node);
    }

    for (uint8_t i = 0; i < d->nresults; i++) {
        if (d->results[i] != JSC_POLY) { rcls[i] = d->results[i]; continue; }
        int g = row->poly_group[d->nparams + i];
        int from = -1;
        for (uint8_t k = 0; k < d->nparams && from < 0; k++)
            if (row->poly_group[k] == g) from = k;
        if (from >= 0) { rcls[i] = pcls[from]; continue; }
        if (!poly_result_class(b, op, sub, ent, &rcls[i])) { decline(b); return; }
    }

    int sid = resolve_sig(row->sig, pcls, rcls);
    if (sid < 0) { decline(b); return; }

    jav_tnode_t* n = (jav_tnode_t*)bbq_arena_alloc(b->arena, sizeof *n);
    if (!n) { decline(b); return; }
    memset(n, 0, sizeof *n);
    n->sig = (uint16_t)sid;
    n->pc = pc;
    n->seq = b->seq;
    uint8_t k = 0;
    for (uint8_t i = 0; i < d->nparams; i++)
        if (d->params[i] != JSC_STK) n->kids[k++] = kid[i].node;
    n->nkids = k;
    /* PIN B-4: the children this walk actually took off the stack are the arity
     * the signature declares. The two come from the same generated table, so a
     * disagreement means the walk popped by one rule and the grammar will match
     * by another — which is a miscompile, not a missed optimization. */
    if (n->nkids != jav_sigtab[sid].nkids) {
        b->arity_mismatches++;
        decline(b);
        return;
    }
    /* The peak stack this subtree reaches, bottom-up over the kids that are
     * already built. Operands are evaluated in order and each result waits on the
     * stack for the ones after it, so kid i runs with everything before it still
     * held — the peak is the largest of those, and never less than holding all
     * the operands at once or the result itself. Leaves come out at their own
     * width, which is 2 for a v128 and 1 otherwise. */
    {
        const jav_sig_t* sg = &jav_sigtab[sid];
        unsigned held = 0, peak = 0, kid_i = 0;
        for (uint8_t i = 0; i < sg->nparams; i++) {
            if (sg->params[i] == JSC_STK) continue;
            unsigned p = held + n->kids[kid_i]->need;
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
    b->nnodes++;

    if (jav_sigtab[sid].nresults) push_val(b, n, rcls[0]);
    else add_root(b, n);                        /* §3.2.1's separate list */

    /* A call's results are not in its signature: the callee leaves them where the
     * argument group was, and the walk has to know they are there. */
    for (uint32_t i = 0; i < nvres; i++) push_val(b, carried_leaf(b, vres[i]), vres[i]);
}

/* ── the walk ──────────────────────────────────────────────── */

static int is_cut(uint8_t op, uint32_t sub) {
    switch (op) {
    case OP_UNREACHABLE: case OP_BLOCK: case OP_LOOP: case OP_IF: case OP_ELSE:
    case OP_END: case OP_BR: case OP_BR_IF: case OP_BR_TABLE: case OP_RETURN:
    case OP_TRY_TABLE: case OP_THROW: case OP_THROW_REF:
    case OP_RETURN_CALL: case OP_RETURN_CALL_INDIRECT: case OP_RETURN_CALL_REF:
    case OP_BR_ON_NULL: case OP_BR_ON_NON_NULL:
        return 1;
    default:
        return op == PFX_GC && (sub == SUB_BR_ON_CAST || sub == SUB_BR_ON_CAST_FAIL);
    }
}

/* §7.6 the instruction transfers control away: what follows is unreachable until
 * the frame's `else` or `end`. */
static int is_terminator(uint8_t op) {
    switch (op) {
    case OP_UNREACHABLE: case OP_BR: case OP_BR_TABLE: case OP_RETURN:
    case OP_THROW: case OP_THROW_REF:
    case OP_RETURN_CALL: case OP_RETURN_CALL_INDIRECT: case OP_RETURN_CALL_REF:
        return 1;
    default: return 0;
    }
}

static void push_frame(builder_t* b, uint8_t op, int32_t bt) {
    tframe_t* f = &b->frames[b->nframes];
    const uint8_t *pc, *rc; uint32_t np, nr;
    if (!blocktype_shape(b, bt, &pc, &np, &rc, &nr)) { decline(b); return; }
    if (b->nstack < np) { decline(b); return; }
    f->op = op;
    f->height = b->nstack - np;            /* §7.6 the label resets to here */
    f->param_class = pc; f->nparams = np;
    f->result_class = rc; f->nresults = nr;
    f->unreachable = f->entry_unreachable = b->frames[b->nframes - 1].unreachable;
    b->nframes++;
}

/* §7.6 reset the stack to the frame's height and put `n` values of `cls` back. */
static void reset_to(builder_t* b, uint32_t height, const uint8_t* cls, uint32_t n) {
    b->nstack = height;
    for (uint32_t i = 0; i < n; i++)
        push_val(b, carried_leaf(b, cls[i]), cls[i]);
}

/* THE invariant a per-tree cover rests on: walking the roots in order, and each
 * root's tree in postorder, visits the instructions in BYTE order. When that
 * holds, the state a cover assigns a node is the state the machine is actually
 * in when that instruction runs, because no other tree's instructions can fall
 * between a node and its parent. When it fails, two covers are choosing the same
 * registers for the same program points without either knowing about the other,
 * and the failure is silent — a value overwritten, not a crash.
 *
 * Checking it is a monotonicity test on `pc`: strictly increasing over the whole
 * region IS byte order. Carried leaves carry no instruction and are skipped. */
static int postorder_ok(const jav_tnode_t* n, const uint8_t** last) {
    for (uint8_t i = 0; i < n->nkids; i++)
        if (!postorder_ok(n->kids[i], last)) return 0;
    if (!n->pc) return 1;
    if (*last && n->pc <= *last) return 0;
    *last = n->pc;
    return 1;
}

static void dump_postorder(const jav_tnode_t* n) {
    for (uint8_t i = 0; i < n->nkids; i++) dump_postorder(n->kids[i]);
    if (n->pc) fprintf(stderr, " %p:%02x", (const void*)n->pc, *n->pc);
    else       fputs(" leaf", stderr);
}

static void check_emission_order(builder_t* b) {
    for (uint32_t r = 0; r < b->nregions; r++) {
        const uint8_t* last = NULL;
        for (uint32_t i = 0; i < b->regions[r].nroots; i++)
            if (!postorder_ok(b->regions[r].roots[i], &last)) {
                if (getenv("JAV_TTREE_DBG")) {
                    fprintf(stderr, "order break: region %u root %u/%u:", r, i,
                            b->regions[r].nroots);
                    for (uint32_t k = 0; k < b->regions[r].nroots; k++) {
                        const jav_tnode_t* rt = b->regions[r].roots[k];
                        fprintf(stderr, " [%u]", k);
                        dump_postorder(rt);
                    }
                    fputc('\n', stderr);
                }
                b->order_breaks++;
                decline_at(b, 0, 0);
                return;
            }
    }
}

int jav_ttree_build(bbq_ctx_t code, const jav_tctx_t* ctx,
                    bbq_arena* arena, jav_ttree_t* out) {
    size_t cap = code.length + 2;
    builder_t b;
    memset(&b, 0, sizeof b);
    b.ctx = ctx; b.arena = arena; b.ok = 1;
    b.stack   = (tval_t*)       bbq_arena_alloc(arena, cap * sizeof *b.stack);
    b.frames  = (tframe_t*)     bbq_arena_alloc(arena, (cap + 1) * sizeof *b.frames);
    b.roots   = (jav_tnode_t**) bbq_arena_alloc(arena, cap * sizeof *b.roots);
    b.regions = (jav_tregion_t*)bbq_arena_alloc(arena, cap * sizeof *b.regions);
    if (!b.stack || !b.frames || !b.roots || !b.regions) return 0;

    /* §7.6 the function's own frame: the body's final `end` closes it. */
    memset(&b.frames[0], 0, sizeof b.frames[0]);
    b.frames[0].op = OP_BLOCK;
    b.frames[0].result_class = ctx->result_class;
    b.frames[0].nresults = ctx->nresults;
    b.nframes = 1;

    bbq_ctx_t cur = code;
    uint32_t region_start = (uint32_t)cur.pos;
    uint32_t nseq = 0;
    int closed_body = 0;   /* the walk met frame 0's `end`, rather than running dry */
    while (b.ok) {
        uint32_t bpos = (uint32_t)cur.pos;
        uint8_t op;
        if (!bbq_read_u8(&cur, &op)) break;
        uint32_t sub = 0;
        const jav_opcode_sig_t* row;
        jav_jit_meta_t m;
        if (jav_jit_meta_sub[op]) {
            bbq_read_uleb128_u32(&cur, &sub);
            if (!jav_opcode_sig_sub[op] || sub >= jav_opcode_sig_sub_len[op]) {
                decline_at(&b, op, sub); break;
            }
            row = &jav_opcode_sig_sub[op][sub];
            m = jav_jit_meta_sub[op][sub];
        } else {
            row = &jav_opcode_sig[op];
            m = jav_jit_meta[op];
        }
        b.cur_op = op; b.cur_sub = sub;
        if (!row->present) { decline(&b); break; }
        b.seq = nseq++;                  /* this instruction's flat §5 ordinal */

        /* Decode every operand, with the SAME table and the same order tier-1
         * uses, so the two walks land on the same next instruction. `ent[k]` is
         * the module ENTRY operand k names — for a memarg that is the memory the
         * align-flag's bit 6 carries, not the offset, so an ADDR slot and a
         * plain index operand resolve through one path. */
        uint64_t ent[16]; uint32_t brtable_count = 0; int32_t blocktype = -64;
        memset(ent, 0, sizeof ent);
        for (int k = 0; k < m.operand_count && k < 16; k++) {
            switch (m.operands[k].kind) {
            case JOP_CONST: ent[k] = m.operands[k].value; break;
            case JOP_MEMARG: {   /* one decode, two facts: the memory and the offset */
                uint32_t fl = 0, mi = 0, off_lo = 0; uint64_t off = 0;
                bbq_read_uleb128_u32(&cur, &fl);
                if (fl & 0x40) bbq_read_uleb128_u32(&cur, &mi);
                bbq_read_uleb128_u64(&cur, &off);
                (void)off; (void)off_lo;
                ent[k] = mi;
                break; }
            case JOP_ULEB32: { uint32_t v = 0; bbq_read_uleb128_u32(&cur, &v); ent[k] = v; break; }
            case JOP_ULEB64: bbq_read_uleb128_u64(&cur, &ent[k]); break;
            case JOP_SLEB32: { int32_t v = 0; bbq_read_sleb128_i32(&cur, &v); ent[k] = (uint64_t)(int64_t)v; break; }
            case JOP_SLEB64: { int64_t v = 0; bbq_read_sleb128_i64(&cur, &v); ent[k] = (uint64_t)v; break; }
            case JOP_BLOCKTYPE: { int32_t v = 0; bbq_read_sleb128_i32(&cur, &v);
                if (v == -29 || v == -28) { int32_t ht = 0; bbq_read_sleb128_i32(&cur, &ht); }
                blocktype = v; break; }
            case JOP_U8: { uint8_t v = 0; bbq_read_u8(&cur, &v); ent[k] = v; break; }
            case JOP_F32: { float v = 0; bbq_read_f32le(&cur, &v); break; }
            case JOP_F64: { double v = 0; bbq_read_f64le(&cur, &v); break; }
            case JOP_BRTABLE_COUNT: bbq_read_uleb128_u32(&cur, &brtable_count);
                ent[k] = brtable_count; break;
            case JOP_NONE: default: break;
            }
        }
        switch (m.tail) {   /* the variable-length immediates opgen declares */
        case JTAIL_BRTABLE:
            for (uint32_t i = 0; i <= brtable_count; i++) { uint32_t l = 0; bbq_read_uleb128_u32(&cur, &l); }
            break;
        case JTAIL_TRYTABLE: {   /* §5.4.1 blocktype, then vec(catch) */
            bbq_read_sleb128_i32(&cur, &blocktype);
            if (blocktype == -29 || blocktype == -28) { int32_t ht = 0; bbq_read_sleb128_i32(&cur, &ht); }
            uint32_t nc = 0, x = 0; bbq_read_uleb128_u32(&cur, &nc);
            for (uint32_t i = 0; i < nc; i++) {
                uint8_t ck = 0; bbq_read_u8(&cur, &ck);
                if (ck == 0 || ck == 1) bbq_read_uleb128_u32(&cur, &x);   /* the tag */
                bbq_read_uleb128_u32(&cur, &x);                            /* the label */
            }
            break; }
        case JTAIL_SELECTVEC: {
            uint32_t nv = 0; bbq_read_uleb128_u32(&cur, &nv);
            for (uint32_t i = 0; i < nv; i++) {
                uint8_t vt = 0; bbq_read_u8(&cur, &vt);
                if (vt == 0x63 || vt == 0x64) { int32_t ht = 0; bbq_read_sleb128_i32(&cur, &ht); }
            }
            break; }
        default: break;
        }

        tframe_t* f = &b.frames[b.nframes - 1];
        const uint8_t* pc = code.data + bpos;

        /* B5 §7.6 after a transfer the operands do not exist, so there is nothing
         * to build until the frame's `else` or `end` puts the stack back. */
        if (f->unreachable) {
            if (op == OP_BLOCK || op == OP_LOOP || op == OP_IF || op == OP_TRY_TABLE) {
                push_frame(&b, op, blocktype);
            } else if (op == OP_ELSE) {
                reset_to(&b, f->height, f->param_class, f->nparams);
                f->unreachable = f->entry_unreachable;   /* the arm is as reachable as the `if` was */
                f->op = OP_ELSE;
            } else if (op == OP_END) {
                uint32_t h = f->height;
                const uint8_t* rc = f->result_class; uint32_t nr = f->nresults;
                if (b.nframes == 1) { close_region(&b, region_start); closed_body = 1; break; }
                b.nframes--;
                reset_to(&b, h, rc, nr);
            }
            if (is_cut(op, sub)) { close_region(&b, region_start); region_start = (uint32_t)cur.pos; }
            continue;
        }

        switch (op) {
        case OP_BLOCK: case OP_LOOP: case OP_IF: case OP_TRY_TABLE:
            build_node(&b, op, sub, row, ent, pc, 1);   /* `if` pops its condition here */
            push_frame(&b, op, blocktype);
            break;
        case OP_ELSE:
            build_node(&b, op, sub, row, ent, pc, 1);
            reset_to(&b, f->height, f->param_class, f->nparams);
            f->op = OP_ELSE;
            break;
        case OP_END: {
            build_node(&b, op, sub, row, ent, pc, 1);
            uint32_t h = f->height;
            const uint8_t* rc = f->result_class; uint32_t nr = f->nresults;
            if (b.nframes == 1) { close_region(&b, region_start); closed_body = 1; goto done; }
            b.nframes--;
            reset_to(&b, h, rc, nr);
            break; }
        default:
            build_node(&b, op, sub, row, ent, pc, is_cut(op, sub));
            if (is_terminator(op)) {
                b.nstack = f->height;
                f->unreachable = 1;
            }
            break;
        }
        if (!b.ok) break;
        if (is_cut(op, sub)) { close_region(&b, region_start); region_start = (uint32_t)cur.pos; }
    }
done:
    /* The walk has to have ended the way a body ends: on frame 0's `end`, with the
     * code entry's last byte consumed and no block left open. Otherwise it did not
     * see the body whole, and the caller keeps tier-1 rather than tile half a
     * function.
     *
     * This is a check on THIS FILE, not on its input. The walk decodes immediates
     * with its own table — `jav_jit_meta`, generated from wasm.def — and neither the
     * interpreter's decoder nor wasm.bbq's grammar is consulted, so if that table is
     * wrong about one opcode the cursor lands inside an immediate and every byte
     * after it is read as an opcode. Usually that meets something no row declares
     * and declines. The case worth checking is when it does not: a 0x0B sitting in
     * an immediate closes frame 0 early and the builder hands back a tree for half a
     * function, which nothing downstream can tell from a whole one. Being handed a
     * validated module says the BYTES are a body; it says nothing about whether this
     * file's table reads them. */
    if (b.ok && (!closed_body || b.nframes != 1 || cur.pos != code.length))
        decline_at(&b, b.cur_op, b.cur_sub);
    if (b.ok) check_emission_order(&b);
    g_stats.arity_mismatches += b.arity_mismatches;
    g_stats.order_breaks += b.order_breaks;
    if (!b.ok) {
        g_stats.bodies_declined++;
        g_stats.decline_op[b.decline_op]++;
        if (!g_stats.have_decline) {
            g_stats.have_decline = 1;
            g_stats.first_decline_op = b.decline_op;
            g_stats.first_decline_sub = b.decline_sub;
        }
        return 0;
    }
    if (b.region_first_root != b.nroots || b.nstack) close_region(&b, region_start);
    g_stats.bodies_built++;
    g_stats.nodes += b.nnodes;
    g_stats.nodes_carried += b.ncarried;
    out->regions = b.regions;
    out->nregions = b.nregions;
    out->nnodes = b.nnodes;
    return 1;
}
