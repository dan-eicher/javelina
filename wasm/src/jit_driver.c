/*
 * jit_driver.c — copy-and-patch JIT (tier 1), pure C.
 *
 * Walks the function body once, and for EACH opcode looks up opgen's generated
 * metadata (jav_jit_meta: opcode -> stencil id + operand-decode kind), decodes
 * the immediate with the SAME bbq_read_* readers the container + interpreter use,
 * and stamps the opgen-generated stencil with that immediate baked into its data
 * hole. The stencils tail-call their linear-chained successor (_HOLE_cont), so
 * the JITed code is the CEK continuation graph run natively — interp == JIT by
 * construction (both run the same generated bodies). No per-opcode logic here;
 * the map is data from opgen. The executable buffer is jitterator's C runtime
 * (jit_codebuf.h), so the whole tier links pure C into the VM.
 */
#include "runtime_api.h"          /* vm_t, slot_t, jav_status_t  (-DJAVELINA_BACKEND_TYPES) */
#include "jit_driver.h"
#include "opcodes.h"              /* OP_END / OP_RETURN */
#include "jav_stencil_table.h"   /* stencil_table[], STENCIL_*, StencilDef, PatchEntry */
#include "jav_jit_meta.h"        /* jav_jit_meta[256], jit_operand_kind_t */
#include "jav_jit_symbols.h"     /* jav_jit_symbols[]: _HOLE_<native> -> address */
#include "jav_tile.h"            /* burgc's matcher over the tier-2 tiling grammar */
#include "jit_codebuf.h"          /* jitterator's C executable code buffer */
#include "bbq_hmap.h"             /* crt flat map — footer-pool value dedup */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A deferred rip-relative data load: where the load is, and the value it needs.
 * Collected during stamping; resolved against the one shared footer pool after
 * all code is laid out (so identical constants dedup to a single slot). */
typedef struct { size_t patch_addr; uint64_t value; size_t foff; } data_hole_t;

static size_t emit_stencil(jit_codebuf_t* buf, const StencilDef* stencil,
                           const uint64_t* values, data_hole_t* recs, int* nrec) {
    size_t code_base = buf->size;
    jcb_emit(buf, stencil->code, stencil->code_size);
    for (uint32_t i = 0; i < stencil->patch_count; i++) {
        const PatchEntry* p = &stencil->patches[i];
        size_t patch_addr = code_base + p->offset;
        switch (p->type) {
        case PATCH_REL_BRANCH: {
            uint64_t target = values ? values[p->hole_index] : 0;
            if (target != 0) {
                uintptr_t pa = (uintptr_t)buf->base + patch_addr;
                jcb_patch32(buf, patch_addr, (int32_t)(target - (pa + 4)));
            }
            break;
        }
        case PATCH_REL_DATA:   /* defer to the function's shared footer pool */
            recs[*nrec].patch_addr = patch_addr;
            recs[*nrec].value = values ? values[p->hole_index] : 0;
            (*nrec)++;
            break;
        case PATCH_ABS64:  jcb_patch64(buf, patch_addr, values ? values[p->hole_index] : 0); break;
        case PATCH_ABS32S: jcb_patch32(buf, patch_addr, (int32_t)(values ? values[p->hole_index] : 0)); break;
        }
    }
    return code_base;
}

static void backpatch(jit_codebuf_t* buf, size_t stencil_base,
                      const StencilDef* stencil, int hole_index, uint64_t target) {
    for (uint32_t i = 0; i < stencil->patch_count; i++) {
        const PatchEntry* p = &stencil->patches[i];
        if ((int)p->hole_index != hole_index || p->type != PATCH_REL_BRANCH) continue;
        size_t patch_addr = stencil_base + p->offset;
        uintptr_t pa = (uintptr_t)buf->base + patch_addr;
        jcb_patch32(buf, patch_addr, (int32_t)(target - (pa + 4)));
    }
}

/* The holes this walk resolves on every instruction — _HOLE_ip, _HOLE_pc,
 * _HOLE_cont and the rest — are generated onto the stencil itself by jitterator
 * (def->h_ip, def->h_pc, …), so reading one is a field access. This remains for
 * the OPERAND holes, whose name comes from the opcode's meta and so varies per
 * opcode rather than per stencil. */
static int find_hole(const StencilDef* def, const char* name) {
    for (int i = 0; i < def->hole_count; i++)
        if (strcmp(def->hole_names[i], name) == 0) return i;
    return -1;
}
/* Resolve a _HOLE_<native> to a spec native's address. NULL for operand /
 * continuation holes (baked or backpatched, not symbol-resolved) — which is most
 * of them, and is why the search is opgen's sorted one rather than a scan here. */
#define jit_sym(name) jav_jit_sym(name)

/* Fill a stencil's fn-pointer data holes with native addresses from the symbol
 * table. Operand holes are filled separately (per the meta's operand list);
 * continuation holes (_HOLE_cont/_HOLE_trap) are PATCH_REL_BRANCH, stay 0 here
 * and are backpatched once the buffer base is fixed. */
static void fill_native_holes(const StencilDef* def, uint64_t* vals) {
    for (int i = 0; i < def->hole_count; i++) {
        void* a = jit_sym(def->hole_names[i]);
        if (a) vals[i] = (uint64_t)(uintptr_t)a;
    }
}

/* Decode one instruction-stream operand, advancing the walk cursor — the SAME
 * bbq_read_* readers the container + interpreter use. Float bits returned raw. */
/* br_table's label count, carried from the operand decode to the tail skip of the
 * SAME instruction (the operand loop runs immediately before the tail switch). */
static uint32_t g_last_brtable_count;

static uint64_t decode_operand(bbq_ctx_t* cur, jit_operand_kind_t kind) {
    switch (kind) {
    case JOP_ULEB32: { uint32_t v = 0; bbq_read_uleb128_u32(cur, &v); return v; }
    case JOP_ULEB64: { uint64_t v = 0; bbq_read_uleb128_u64(cur, &v); return v; }
    case JOP_SLEB32: { int32_t v = 0;  bbq_read_sleb128_i32(cur, &v); return (uint64_t)(uint32_t)v; }
    case JOP_SLEB64: { int64_t v = 0;  bbq_read_sleb128_i64(cur, &v); return (uint64_t)v; }
    /* §5.3.3 blocktype (s33): 0x40/valtype/typeidx; the (ref null? ht) forms 0x63/0x64 carry a
     * TRAILING heaptype that must be consumed so the compile-walk lands on the next instruction. */
    case JOP_BLOCKTYPE: { int32_t v = 0; bbq_read_sleb128_i32(cur, &v);
        if (v == -29 || v == -28) { int32_t ht = 0; bbq_read_sleb128_i32(cur, &ht); }
        return (uint64_t)(uint32_t)v; }
    case JOP_U8:     { uint8_t v = 0; bbq_read_u8(cur, &v); return v; }
    case JOP_F32:    { float v = 0;  bbq_read_f32le(cur, &v); uint32_t b; memcpy(&b, &v, 4); return b; }
    case JOP_F64:    { double v = 0; bbq_read_f64le(cur, &v); uint64_t b; memcpy(&b, &v, 8); return b; }
    /* memarg: align-flags (bit 6 ⇒ a memidx u32 follows), then the u64 offset — the only
     * part the stencil bakes (_HOLE_offset); the memidx is validated, not stamped. */
    case JOP_MEMARG: { uint32_t fl = 0; bbq_read_uleb128_u32(cur, &fl);
                       if (fl & 0x40) { uint32_t mi = 0; bbq_read_uleb128_u32(cur, &mi); }
                       uint64_t off = 0; bbq_read_uleb128_u64(cur, &off); return off; }
    /* br_table's vec(labelidx) LENGTH. Read here so the stencil can bake it; the
     * JTAIL_BRTABLE walk below then skips only the count+1 labels. Stashed because
     * the tail switch runs after the operand loop and needs the same value. */
    case JOP_BRTABLE_COUNT: { uint32_t n = 0; bbq_read_uleb128_u32(cur, &n);
                              g_last_brtable_count = n; return n; }
    case JOP_NONE: default: return 0;
    }
}

/* The byte-offset -> stamped-stencil-address map the resync STENCIL reads (its
 * _HOLE_offmap data hole points here; index code_len holds the halt). An array of
 * preserve_none entry addresses — stored as plain pointers; resync casts them. */
typedef void* jit_addr_t;

/* A compiled function: the executable buffer + its entry offset + the offmap the
 * resync stencil reads. Produced once by jit_compile, re-entered per call
 * (jit_enter) — this is the JIT cache unit. */
struct jit_func_s { jit_codebuf_t buf; size_t entry_off; jit_addr_t* offmap; };

/* Can this instruction's rule depend on the cache STATE at all?
 *
 * §3.4: a nonterminal is (class x location), so a rule says WHERE ITS OPERANDS
 * SIT and nothing else. States that place the operands identically produce the
 * same rule, the generator emits it once at the cheapest of them, and the
 * recorded state is then the MINIMUM that produces that rule rather than the
 * machine's.
 *
 * Two states place operands identically exactly when the extra slots hold
 * SURVIVORS — values this instruction never touches. That is every state at or
 * above its operand slots, which is what this asks. Below that the operands move,
 * each state is its own rule, and the recorded state is a fact to bridge to.
 *
 * Taking the machine's state for those is a readout, not an override: `rule_cost`
 * for an operand-less signature is 0 operands loaded plus the result
 * stored-or-not, which does not mention the state, so the DP has no preference
 * between the collapsed rules. Its one real choice, mem or reg0 for the result,
 * is still its own and is honoured below.
 *
 * Asked of the INSTRUCTION, never of the tile. The tile answers `no cached
 * operand` for an `i32.add` the cover placed entirely in memory — a rule that
 * does depend on the state — and the carried state there would select the
 * both-operands-in-registers variant for values that are on the stack. */
static int state_agnostic(const jav_jit_meta_t* m, int tile_st) {
    return tile_st >= m->pop_slots;
}

/* …and whether the family can still land the result WHERE THE RULE ASKED if this
 * instruction runs at state `st`. The cover's one real choice for an operand-less
 * rule is mem-or-reg0 for its result — `out0` is the rule's slot-0 out class, and
 * JSC_COUNT there means the rule reduced at `X_mem` — and reading the machine's
 * state must not quietly overturn it: at state 0 the memory form is the plain
 * stencil and always exists, but above it that form is D7s' `__sKm`, which a
 * state need not have. */
static int agnostic_entry_ok(const jav_jit_meta_t* m, int out0, int st) {
    if (st == 0) return 1;
    if (m->push && out0 >= (int)(JAV_SCLASS_FINAL - 1))
        return jav_variant_m[m->stencil][st] >= 0;
    return jav_variant[m->stencil][st] >= 0 && jav_variant_fs[m->stencil][st] >= 0;
}

/* ── the emission context ────────────────────────────────────
 *
 * ONE body is compiled at a time, and the reduce's actions have no argument to
 * carry a context through burg — so the driver arms this before it walks and the
 * generated `jav_t2_stamp` calls land here. `tiled` says which of the two walks
 * is running: the reduce over the tree (tier-2, states real), or the plain byte
 * walk (tier-1 and the decline fallback, every stencil the plain form). */
typedef struct {
    int            active;
    int            tiled;
    bbq_ctx_t      code;        /* the body; a node's pc positions a cursor in it */
    jit_codebuf_t* buf;
    size_t*        offs;        /* buffer offsets, emission order */
    int*           sids;
    size_t*        boffs;       /* byte offset each stencil belongs to */
    size_t         n, cap;
    data_hole_t*   recs;
    int            nrec;
    /* The class in each cache slot, slot 0 the top — carried along the emission
     * rather than asked of a rule: a rule names what it consumes and produces,
     * and a survivor was put there by an instruction the rule has never seen. */
    int            live_cls[JAV_TIER2_N > 0 ? JAV_TIER2_N : 1];
    int            live_st;
    size_t         last_bpos;   /* the previous stamp's byte offset — spills carry it */
    /* Where the previous instruction's bytes ended. A stamp that begins past it
     * means dead code was SKIPPED — it builds no tree — and in the byte walk that
     * stretch stamped plain forms whose exit state is 0, so the cache does not
     * survive it. The reset here is that same fact without the dead stencils. */
    size_t         next_pos;
    int            region_first;/* the next stamp opens a region (for regions_hot) */
    uint32_t       stamped;     /* primaries stamped by rule actions == the picks */
    jav_status_t   status;
    int            unbridged;
    /* Why the walk stopped, for the fallback meter's first-failure diagnostics. */
    uint8_t        fail_op;
    uint32_t       fail_bpos;
    int            fail_entry, fail_why;
    /* This walk's emission meters, committed only if its output ships. */
    jav_ttree_stats_t acc;
} emit_ctx_t;

static emit_ctx_t g_e;

static void emit_reset_live(void) {
    g_e.live_st = 0;
    for (int j = 0; j < JAV_TIER2_N; j++) g_e.live_cls[j] = (int)JSC_COUNT;
}

/* One instruction, from wherever `cur` points: decode it with the same readers
 * the interpreter uses, pick the stencil form, bridge the machine's cache state
 * to this rule's, stamp, and account. Shared by both walks — `g_e.tiled` is the
 * only difference, and on the plain walk every state below is zero.
 *
 * Bridging happens at the CONSUMER: the transitions between instruction i and
 * i+1 are stamped when i+1 is entered, spills carrying i's byte offset and fills
 * i+1's — the same offsets the gap-side stitcher recorded, because a fill is the
 * entered instruction's preparation (a call resumes through the offmap at the IP
 * after it, and the fill must be what that arrival runs) while a spill disposes
 * of what the previous one produced. */
static void stamp_instr(bbq_ctx_t* cur, int entry_req,
                        uint32_t in_pack, uint32_t out_pack) {
    if (g_e.status != JAV_RETURN) return;
    /* Room for this instruction and every transition it can imply — the same
     * bound the buffer was sized with, checked so an argument that stops holding
     * is a decline rather than a heap overflow. */
    if (g_e.n + (size_t)JAV_TIER2_N + 1 > g_e.cap) {
        g_e.fail_why = 1; g_e.fail_bpos = (uint32_t)cur->pos;
        g_e.status = JAV_TRAP; return;
    }
    size_t bpos = cur->pos;
    uint8_t op;
    if (!bbq_read_u8(cur, &op)) {
        g_e.fail_why = 2; g_e.fail_bpos = (uint32_t)bpos;
        g_e.status = JAV_TRAP; return;
    }
    jav_jit_meta_t m;
    if (jav_jit_meta_sub[op]) {
        uint32_t sub = 0; bbq_read_uleb128_u32(cur, &sub);
        m = jav_jit_meta_sub[op][sub];
    } else m = jav_jit_meta[op];
    if (m.stencil < 0) {
        g_e.fail_why = 3; g_e.fail_op = op; g_e.fail_bpos = (uint32_t)bpos;
        g_e.status = JAV_TRAP; return;
    }

    int entry = 0, plain = 1, chosen = m.stencil, ntrans = 0;
    if (g_e.tiled) {
        entry = entry_req;
        int out0 = (int)(out_pack & JAV_TILE_CLS_MASK);
        int out0mem = m.push && out0 >= (int)(JAV_SCLASS_FINAL - 1);
        /* A state-agnostic rule recorded the MINIMUM state that produces it; the
         * machine may be deeper. Run at the deepest state the family still has a
         * form for — Ertl's minimal overflow (§2.3): the gap left, if any, costs
         * one spill below rather than a flush. */
        if (state_agnostic(&m, entry) && g_e.live_st > entry) {
            int st = g_e.live_st;
            while (st > entry && !agnostic_entry_ok(&m, out0, st)) st--;
            entry = st;
        }
        /* The family is per OPCODE and a rule is per SIGNATURE, so a member need
         * not offer every form its terminal's rule named — and the grammar's own
         * contract (gen_tile_burg's C5 note) is that the emitter "reaches its
         * state by transition". Descend to the nearest state this member
         * provides: the bridge below spills the operands the rule wanted cached,
         * and the lower form reads them from memory, values intact. State 0 is
         * the plain stencil and always exists, so this terminates. The spill is a
         * cost the cover never priced — the known coarseness of a per-signature
         * grammar over a per-opcode family, counted rather than hidden: the old
         * byte walk TRAPPED here and silently re-stamped the whole body at
         * tier-1, 1,545 bodies of this corpus. */
        while (entry > 0) {
            int ok = out0mem
                   ? jav_variant_m[m.stencil][entry] >= 0
                   : (jav_variant[m.stencil][entry] >= 0
                      && jav_variant_fs[m.stencil][entry] >= 0);
            if (ok) break;
            entry--;
        }
        /* The bridge. A cache state is a property of a program point: where the
         * previous instruction left the cache and where this one expects it are
         * two facts about the same point, and the difference IS the transition. */
        while (g_e.live_st != entry) {
            int down = g_e.live_st > entry;
            /* A spill moves the DEEPEST cached value — the ones remaining must
             * still be the top of the stack. A fill loads what THIS instruction
             * wants at the slot it is about to occupy, which is its own in-pack.
             * A value is one slot or TWO, so the step is its width. */
            int cls = down ? g_e.live_cls[g_e.live_st - 1]
                           : (int)((in_pack >> (g_e.live_st * JAV_TILE_CLS_BITS))
                                   & JAV_TILE_CLS_MASK);
            if (cls >= (int)(JAV_SCLASS_FINAL - 1)) {
                g_e.unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, g_e.live_st, entry, cls);
                break;
            }
            int w = jav_class_width[cls];
            int slot = down ? g_e.live_st - w : g_e.live_st;
            if (slot < 0 || slot + w > JAV_TIER2_N) {
                g_e.unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, g_e.live_st, entry, cls);
                break;
            }
            int tid = down ? jav_spill[cls][slot] : jav_fill[cls][slot];
            if (tid < 0) {
                g_e.unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, g_e.live_st, entry, cls);
                break;
            }
            g_e.boffs[g_e.n] = down ? g_e.last_bpos : bpos;
            g_e.offs[g_e.n] = emit_stencil(g_e.buf, &stencil_table[tid], NULL,
                                           g_e.recs, &g_e.nrec);
            g_e.sids[g_e.n] = tid; g_e.n++; ntrans++;
            /* Bridged INTO a tiled instruction, so a rule costed this seam. */
            jav_ttree_note_transition(down, 0);
            jav_ttree_note_mem(w);
            for (int q = 0; q < w; q++)
                g_e.live_cls[slot + q] = down ? (int)JSC_COUNT : cls;
            g_e.live_st += down ? -w : w;
        }
        jav_ttree_note_stitch(entry, ntrans, g_e.unbridged);
        if (g_e.unbridged) {
            g_e.fail_why = 6; g_e.fail_op = op; g_e.fail_bpos = (uint32_t)bpos;
            g_e.fail_entry = entry;
            g_e.status = JAV_TRAP; return;
        }
        plain = 0;
        chosen = jav_variant[m.stencil][entry];
        /* …unless the rule asked for the result in MEMORY. A variant caches what
         * it produces; for a class with no *_reg0 rule that is not what the rule
         * reduced at. At entry 0 the memory form is the plain stencil; above it,
         * D7s' `__sKm` — same cached operands, result pushed inline. */
        if (out0mem) {
            int mv = entry == 0 ? m.stencil : jav_variant_m[m.stencil][entry];
            if (mv >= 0) { chosen = mv; plain = 1; }
        }
        if (chosen < 0) {
            g_e.fail_why = 4; g_e.fail_op = op; g_e.fail_bpos = (uint32_t)bpos;
            g_e.fail_entry = entry;
            g_e.status = JAV_TRAP; return;
        }
    } else {
        jav_ttree_note_stitch(0, 0, 0);
    }

    const StencilDef* def = &stencil_table[chosen];
    uint64_t vals[16] = {0};
    fill_native_holes(def, vals);
    for (int k = 0; k < m.operand_count; k++) {
        /* A memarg yields TWO stencil holes from one decode: the offset (this
         * operand's hole) and the memidx (the active memory). */
        if (m.operands[k].kind == JOP_MEMARG) {
            uint32_t fl = 0, mi = 0; bbq_read_uleb128_u32(cur, &fl);
            if (fl & 0x40) bbq_read_uleb128_u32(cur, &mi);
            uint64_t off = 0; bbq_read_uleb128_u64(cur, &off);
            int ho = find_hole(def, m.operands[k].hole); if (ho >= 0) vals[ho] = off;
            int hm = def->h_memidx;                       if (hm >= 0) vals[hm] = mi;
            continue;
        }
        /* JOP_CONST is a synthesized constant (e.g. a guard's range bound):
         * its value rides in the meta — no bytecode, no cursor advance. */
        uint64_t imm = (m.operands[k].kind == JOP_CONST)
                     ? m.operands[k].value
                     : decode_operand(cur, m.operands[k].kind);
        int h = find_hole(def, m.operands[k].hole);
        if (h >= 0) vals[h] = imm;
    }
    int hip = def->h_ip;
    size_t ip_at_tail_start = cur->pos;
    int hpc = def->h_pc;
    if (hpc >= 0) vals[hpc] = bpos;
    /* A variable-length trailing immediate the op's native reads at runtime: the
     * walk steps over it to land on the next instruction; _HOLE_ip placement
     * depends on whether the BODY still reads the tail at runtime (see wasm.def's
     * m.tail declarations). */
    int ip_after_tail = 0;
    switch (m.tail) {
    case JTAIL_BRTABLE: {
        uint32_t count = g_last_brtable_count, lbl;
        for (uint32_t i = 0; i <= count; i++) bbq_read_uleb128_u32(cur, &lbl);
        ip_after_tail = 1;
        break;
    }
    case JTAIL_TRYTABLE: {
        int32_t bt = 0; bbq_read_sleb128_i32(cur, &bt);
        if (bt == -29 || bt == -28) { int32_t ht = 0; bbq_read_sleb128_i32(cur, &ht); }
        uint32_t nc = 0, tmp; bbq_read_uleb128_u32(cur, &nc);
        for (uint32_t i = 0; i < nc; i++) {
            uint8_t ck = 0; bbq_read_u8(cur, &ck);
            if (ck == 0 || ck == 1) bbq_read_uleb128_u32(cur, &tmp);
            bbq_read_uleb128_u32(cur, &tmp);
        }
        break;
    }
    case JTAIL_SELECTVEC: {
        uint32_t nt = 0; bbq_read_uleb128_u32(cur, &nt);
        for (uint32_t i = 0; i < nt; i++) {
            uint8_t vt = 0; bbq_read_u8(cur, &vt);
            if (vt == 0x63 || vt == 0x64) { int32_t ht = 0; bbq_read_sleb128_i32(cur, &ht); }
        }
        break;
    }
    case JTAIL_NONE: default: break;
    }
    if (hip >= 0) vals[hip] = ip_after_tail ? cur->pos : ip_at_tail_start;
    g_e.boffs[g_e.n] = bpos;
    g_e.offs[g_e.n] = emit_stencil(g_e.buf, def, vals, g_e.recs, &g_e.nrec);
    g_e.sids[g_e.n] = chosen; g_e.n++;

    /* Move the slot classes the way the stencil moved the values, and count the
     * operand-stack traffic this form actually performs — the one place that
     * knows which form was stamped. */
    int exit_st = plain ? 0 : jav_variant_fs[m.stencil][entry];
    if (exit_st < 0) {
        g_e.fail_why = 5; g_e.fail_op = op; g_e.fail_bpos = (uint32_t)bpos;
        g_e.fail_entry = entry;
        g_e.status = JAV_TRAP; return;
    }
    {
        int a = m.pop_slots;
        int left = entry > a ? entry - a : 0;
        int r = exit_st > left ? m.push_slots : 0;
        jav_ttree_note_mem((a > entry ? a - entry : 0) + (r ? 0 : m.push_slots));
        for (int j = 0; j < left; j++) g_e.live_cls[r + j] = g_e.live_cls[a + j];
        for (int j = 0; j < r; j++)
            g_e.live_cls[j] = (int)((out_pack >> (j * JAV_TILE_CLS_BITS))
                                    & JAV_TILE_CLS_MASK);
        for (int j = exit_st; j < JAV_TIER2_N; j++) g_e.live_cls[j] = (int)JSC_COUNT;
    }
    g_e.live_st = exit_st;
    g_e.last_bpos = bpos;
    g_e.next_pos = cur->pos;
}

/* THE generated rule action: burg matched this node, the rule said which state
 * it runs in and where its operands and result sit, and this is where the
 * stencil is stamped — the reduce IS the emitter (#16). */
void jav_t2_stamp(const jav_tnode_t* n, int state, uint32_t in_pack, uint32_t out_pack) {
    if (!g_e.active || !g_e.tiled || g_e.status != JAV_RETURN || !n->pc) return;
    bbq_ctx_t cur = g_e.code;
    cur.pos = (size_t)(n->pc - g_e.code.data);
    /* Dead code builds no tree, so the reduce never meets it; the byte walk
     * stamped it in plain forms whose exit state is 0, so the cache never
     * survived the stretch. Same fact, stated instead of stamped. */
    if (cur.pos != g_e.next_pos) emit_reset_live();
    if (g_e.region_first) {
        jav_ttree_note_region_entry(g_e.live_st);
        g_e.region_first = 0;
    }
    if (JAV_TIER2_N > 0) {
        /* A rule that names a v128 in a register, on either side — the class
         * axis the state counters cannot see. */
        int wide = 0;
        for (int s = 0; s < 32 / JAV_TILE_CLS_BITS && !wide; s++) {
            if (((in_pack  >> (s * JAV_TILE_CLS_BITS)) & JAV_TILE_CLS_MASK) == JSC_V128) wide = 1;
            if (((out_pack >> (s * JAV_TILE_CLS_BITS)) & JAV_TILE_CLS_MASK) == JSC_V128) wide = 1;
        }
        if (wide) jav_ttree_note_wide();
    }
    g_e.stamped++;
    stamp_instr(&cur, state, in_pack, out_pack);
}

/* The per-body prologue: the entry stencil, the one resync stencil (its offmap
 * pointer and code length baked; control stencils backpatch to it) and the trap
 * stencil. Shared by both walks, so a fallback re-stamps the same skeleton. */
static void begin_body(jit_addr_t* offmap, size_t code_len,
                       size_t* entry_off, size_t* resync_off, size_t* trap_off) {
    const StencilDef* rd = &stencil_table[STENCIL_RESYNC];
    jcb_reset(g_e.buf);
    memset(offmap, 0, (code_len + 1) * sizeof *offmap);
    g_e.n = 0; g_e.nrec = 0;
    g_e.status = JAV_RETURN;
    g_e.unbridged = 0;
    g_e.stamped = 0;
    g_e.last_bpos = 0;
    g_e.next_pos = g_e.code.pos;
    g_e.region_first = 0;
    memset(&g_e.acc, 0, sizeof g_e.acc);
    jav_ttree_stats_sink(&g_e.acc);
    emit_reset_live();
    *entry_off = emit_stencil(g_e.buf, &stencil_table[STENCIL_ENTRY], NULL,
                              g_e.recs, &g_e.nrec);
    {
        uint64_t rvals[16] = {0};
        int rh;
        if ((rh = rd->h_offmap)  >= 0) rvals[rh] = (uint64_t)(uintptr_t)offmap;
        if ((rh = rd->h_codelen) >= 0) rvals[rh] = code_len;
        *resync_off = emit_stencil(g_e.buf, rd, rvals, g_e.recs, &g_e.nrec);
    }
    *trap_off = emit_stencil(g_e.buf, &stencil_table[STENCIL_TRAP], NULL,
                             g_e.recs, &g_e.nrec);
}

/* The plain byte walk: tier-1, and the fallback for a body the cover or the
 * bridge declined (D8). Every stencil is the plain form the meta names — with no
 * tiling behind it, a variant that cached its result would leave a register
 * nothing ever spills or reads. Dead code is stamped here like anything else,
 * which is exactly right for it: it is unreachable, and tier-1 is its form. */
static void byte_walk(void) {
    bbq_ctx_t cur = g_e.code;
    for (;;) {
        if (g_e.status != JAV_RETURN) return;
        if (g_e.n + (size_t)JAV_TIER2_N + 1 > g_e.cap) { g_e.status = JAV_TRAP; return; }
        size_t bpos = cur.pos;
        bbq_ctx_t pk = cur; uint8_t op;
        if (!bbq_read_u8(&pk, &op)) {   /* off the end: the function's halt */
            g_e.boffs[g_e.n] = bpos;
            g_e.offs[g_e.n] = emit_stencil(g_e.buf, &stencil_table[STENCIL_GEN_ST_HALT],
                                           NULL, g_e.recs, &g_e.nrec);
            g_e.sids[g_e.n] = STENCIL_GEN_ST_HALT; g_e.n++;
            return;
        }
        stamp_instr(&cur, 0, 0, 0);
    }
}

/* The reduce-driven walk: regions in order, roots in order, and the generated
 * rule actions stamp in postorder — which IS byte order over the live
 * instructions, the invariant the order-break meter holds at zero. ONE burg
 * context for the whole body with a single check at the end: the rewrite latches
 * the first error and short-circuits, so clearing between regions would let a
 * later region wipe an earlier region's no-cover. */
static int tree_walk(const jav_ttree_t* tree, jav_tile_burg_ctx_t* bc) {
    for (uint32_t r = 0; r < tree->nregions; r++) {
        g_e.region_first = 1;
        for (uint32_t i = 0; i < tree->regions[r].nroots; i++)
            jav_tile_burg_rewrite(tree->regions[r].roots[i], bc);
    }
    if (jav_tile_burg_has_error(bc)) return 0;
    if (g_e.status != JAV_RETURN || g_e.unbridged) return 0;
    /* The function's halt — the off-the-end / `return` target the offmap's
     * code-length slot resolves to. */
    if (g_e.n + 1 > g_e.cap) { g_e.status = JAV_TRAP; return 0; }
    g_e.boffs[g_e.n] = g_e.code.length;
    g_e.offs[g_e.n] = emit_stencil(g_e.buf, &stencil_table[STENCIL_GEN_ST_HALT],
                                   NULL, g_e.recs, &g_e.nrec);
    g_e.sids[g_e.n] = STENCIL_GEN_ST_HALT; g_e.n++;
    return 1;
}

jit_func_t* jit_compile(bbq_ctx_t code, const jav_tctx_t* tcx) {
    jit_func_t* fn = (jit_func_t*)malloc(sizeof *fn);
    if (!fn) return NULL;

    jit_codebuf_t buf;
    if (jcb_init(&buf, 4096) != 0) { free(fn); return NULL; }

    /* One stencil per body byte is a safe upper bound (every opcode is ≥1 byte),
     * plus the entry and halt stencils — and then the cache transitions, which
     * are stamped BETWEEN instructions and so are not bounded by the byte count:
     * a 1-byte `i32.add` can imply itself and a spill. An instruction moves the
     * state by at most the cache size, so n+1 per byte covers it. */
    size_t code_len = code.length;
    size_t cap = (code_len + 2) * (size_t)(JAV_TIER2_N + 1);
    size_t* offs  = (size_t*)malloc(cap * sizeof *offs);   /* buffer offsets, exec order */
    int*    sids  = (int*)   malloc(cap * sizeof *sids);
    size_t* boffs = (size_t*)malloc(cap * sizeof *boffs);  /* byte offset each stencil belongs to */
    jit_addr_t* offmap = (jit_addr_t*)calloc(code_len + 1, sizeof *offmap);  /* IP -> address; [code_len]=halt */
    data_hole_t* recs = (data_hole_t*)malloc((cap + 4) * 8 * sizeof *recs);  /* deferred rip-rel data loads */
    if (!offs || !sids || !boffs || !offmap || !recs) {
        free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL;
    }

    memset(&g_e, 0, sizeof g_e);
    g_e.active = 1;
    g_e.code = code;
    g_e.buf = &buf;
    g_e.offs = offs; g_e.sids = sids; g_e.boffs = boffs;
    g_e.cap = cap; g_e.recs = recs;

    size_t entry_off = 0, resync_off = 0, trap_off = 0;
    int done = 0;

    /* Tier-2: build the tree and let the reduce stamp it. A no-cover, an
     * unbridgeable gap or a capacity miss is a decline, not a failure — tier-2 is
     * an optimization on top of tier-1, so the body falls to the plain byte walk
     * below rather than off the JIT (D8). Giving up outright once left a function
     * interpreted that tier-1 had compiled for the whole life of the JIT. */
    if (tcx) {
        bbq_arena ta; bbq_arena_init(&ta, 16 * 1024);
        jav_ttree_t tree;
        if (jav_ttree_build(code, tcx, &ta, &tree)) {
            jav_tile_burg_ctx_t bc;
            jav_tile_burg_ctx_init(&bc);
            g_e.tiled = 1;
            begin_body(offmap, code_len, &entry_off, &resync_off, &trap_off);
            done = tree_walk(&tree, &bc);
            jav_ttree_stats_sink(NULL);
            if (done) {
                /* This walk's output ships: its meters are the body's. The
                 * actions ARE the picks — one primary per non-carried node — so
                 * counting the stamps against the nodes is what stops "covered"
                 * from meaning less than it says. */
                jav_ttree_stats_commit(&g_e.acc);
                jav_ttree_note_picks(g_e.stamped, tree.nnodes);
                jav_ttree_note_cover(1, 0);
            } else if (jav_tile_burg_has_error(&bc)) {
                /* A genuine no-cover: the grammar had no rule. Counted as
                 * uncovered, exactly as before; the body ships tier-1. */
                jav_ttree_note_cover(0, jav_tile_burg_get_error_arg(&bc));
            } else {
                /* Covered but unstampable — the walk failed mid-emission. The
                 * body ships tier-1 and NOTHING from the abandoned walk is
                 * counted except the fallback itself, first failure named:
                 * meters describe shipped code, and the old silent retry is the
                 * lie this line replaces. */
                jav_ttree_note_fallback(g_e.fail_op, g_e.fail_bpos,
                                        g_e.fail_entry, g_e.fail_why);
            }
            jav_tile_burg_ctx_free(&bc);
        }
        bbq_arena_free(&ta);
    }
    if (!done) {
        g_e.tiled = 0;
        begin_body(offmap, code_len, &entry_off, &resync_off, &trap_off);
        byte_walk();
        jav_ttree_stats_sink(NULL);
        if (g_e.status != JAV_RETURN) {
            g_e.active = 0;
            free(offs); free(sids); free(boffs); free(offmap); free(recs);
            jcb_free(&buf); free(fn); return NULL;
        }
        jav_ttree_stats_commit(&g_e.acc);
    }
    size_t n = g_e.n;
    int nrec = g_e.nrec;
    g_e.active = 0;

    /* Shared footer pool: one 8-byte slot per DISTINCT constant the function's
     * rip-relative data loads need (operands, native ptrs, const holes), deduped
     * by value (few per function). Appended after all code — BEFORE the absolute
     * branch-backpatches below, because jcb_emit may move base; the data patches
     * are rel32 (offset-relative) so they survive the move, and the backpatches
     * then read the final base. */
    /* The value IS the key, so the map answers directly: `nrec` grows with function
     * size and the plain nested scan was O(nrec²) — quadratic JIT compilation on
     * the RTL's big functions.
     *
     * bbq_hmap rather than bbq_htree because the key is a full 64-bit value we
     * minted. The trie takes a 32-bit key, so it needed the value folded down, a
     * `chain` array to hold the collision list the fold created, and a compare
     * against the real value to walk it — three structures for one question, and
     * eight dependent loads per descent. Here a lookup is mix, index, compare.
     *
     * On an allocation failure the map stays empty-but-valid and every lookup
     * misses, so each rec gets its own footer slot: a larger pool, still correct,
     * because dedup is size and not correctness. */
    bbq_hmap pool;
    bbq_hmap_init(&pool, (size_t)nrec);
    for (int i = 0; i < nrec; i++) {
        size_t foff;
        void* hit = bbq_hmap_get(&pool, recs[i].value);
        if (hit) foff = (size_t)((uintptr_t)hit - 1);          /* stored as foff+1: foff 0 is real */
        else {
            foff = buf.size;
            jcb_emit(&buf, (const uint8_t*)&recs[i].value, 8);
            bbq_hmap_put(&pool, recs[i].value, (void*)(uintptr_t)(foff + 1));
        }
        recs[i].foff = foff;
    }
    bbq_hmap_free(&pool);
    for (int i = 0; i < nrec; i++)
        jcb_patch32(&buf, recs[i].patch_addr,
                    (int32_t)((long)recs[i].foff - (long)(recs[i].patch_addr + 4)));

    /* Linear fall-through chain (_HOLE_cont) + every control stencil's resync
     * continuation -> the one in-buffer resync stencil. Buffer base is stable now. */
    uint8_t* base = buf.base;
    const StencilDef* ed = &stencil_table[STENCIL_ENTRY];
    backpatch(&buf, entry_off, ed, ed->h_cont, (uint64_t)(base + offs[0]));
    for (size_t i = 0; i < n; i++) {
        const StencilDef* d = &stencil_table[sids[i]];
        if (i + 1 < n) {
            int hc = d->h_cont;
            if (hc >= 0) backpatch(&buf, offs[i], d, hc, (uint64_t)(base + offs[i + 1]));
        }
        int hr = d->h_resync;
        if (hr >= 0) backpatch(&buf, offs[i], d, hr, (uint64_t)(base + resync_off));
        int ht = d->h_trap;
        if (ht >= 0) backpatch(&buf, offs[i], d, ht, (uint64_t)(base + trap_off));
    }

    void* exec = jcb_finalize(&buf);
    if (!exec) { free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL; }

    /* Populate the offmap: each opcode's byte offset -> its address; the code-length
     * slot holds the halt (the off-the-end / `return` target).
     *
     * FIRST write wins, because more than one stencil is stamped at the same byte
     * offset: a cache transition belongs to the gap AFTER an instruction and
     * carries that instruction's offset, so a last-write-wins map resolved the IP
     * to the spill rather than to the instruction. Arriving there from a branch
     * then skipped the instruction entirely — and on a LOOP back edge that skips
     * the body's own decrement, so the loop never terminates. An IP means where
     * its instruction BEGINS; nothing stamped afterwards may claim it. */
    for (size_t i = 0; i < n; i++)
        if (boffs[i] <= code_len && !offmap[boffs[i]])
            offmap[boffs[i]] = (jit_addr_t)((uint8_t*)exec + offs[i]);
    offmap[code_len] = (jit_addr_t)((uint8_t*)exec + offs[n - 1]);   /* halt stamped last */

    free(offs); free(sids); free(boffs); free(recs);   /* scratch; offmap + buf belong to the handle */
    fn->buf = buf; fn->entry_off = entry_off; fn->offmap = offmap;
    jav_ttree_note_code(buf.size);
    return fn;
}

/* Enter a compiled function. The caller has set up vm->frame (code, locals, side-
 * table); this runs the stamped chain to its halt and reports the outcome.
 *
 * The trap state is cleared here for the same reason interp_run clears it: the
 * outcome is READ off vm->trapped afterwards, so a flag left standing from an
 * earlier run is reported as this run's trap. Without it the first trap poisoned
 * the store — every later call returned JAV_TRAP with its results discarded,
 * whatever the stamped code actually computed. Nothing caught it because a trap
 * is the last thing a trap fixture does and a benchmark never traps at all. */
int jav_jit_cache_slots(void) { return JAV_TIER2_N; }

jav_status_t jit_enter(const jit_func_t* fn, vm_t* vm) {
    vm->trapped = 0;
    vm->trap_reason = JAV_TRAP_NONE;
    vm->exhausted = NULL;
    vm->frame.stp = 0;
    ((void (*)(vm_t*))((uint8_t*)fn->buf.base + fn->entry_off))(vm);
    return vm->trapped ? JAV_TRAP : JAV_RETURN;
}

void jit_free(jit_func_t* fn) {
    if (!fn) return;
    jcb_free(&fn->buf);
    free(fn->offmap);
    free(fn);
}

/* The function-table invoke seam: a JITed callee's table entry stores its compiled
 * handle as ctx and this as `invoke`, so jav_call dispatches into JITed code
 * without the runtime knowing anything about the JIT. */
jav_status_t jit_invoke(vm_t* vm, heap_t* h, void* ctx) {
    (void)h;
    return jit_enter((const jit_func_t*)ctx, vm);
}

/* Top-level: compile the current frame's function, run it once, free it. */
jav_status_t jav_jit_run(vm_t* vm) {
    jit_func_t* fn = jit_compile(vm->frame.code, NULL);
    if (!fn) return JAV_TRAP;
    jav_status_t st = jit_enter(fn, vm);
    jit_free(fn);
    return st;
}
