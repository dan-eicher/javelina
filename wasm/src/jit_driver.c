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

/* …and whether the family can still land the result WHERE THE TILE ASKED if this
 * instruction runs at state `st`. The cover's one real choice for an operand-less
 * rule is mem-or-reg0 for its result, and reading the machine's state must not
 * quietly overturn it: at state 0 the memory form is the plain stencil and always
 * exists, but above it that form is D7s' `__sKm`, which a state need not have. */
static int agnostic_entry_ok(const jav_jit_meta_t* m, uint32_t off, int st) {
    if (st == 0) return 1;
    if (m->push && jav_tile_out_class_at(off, 0) >= (int)(JAV_SCLASS_FINAL - 1))
        return jav_variant_m[m->stencil][st] >= 0;
    return jav_variant[m->stencil][st] >= 0 && jav_variant_fs[m->stencil][st] >= 0;
}

jit_func_t* jit_compile(bbq_ctx_t code, const jav_tctx_t* tcx) {
    jit_func_t* fn = (jit_func_t*)malloc(sizeof *fn);
    if (!fn) return NULL;

    /* Tier-2's input, and the cover over it. ONE jav_tile_burg_ctx_t for the whole
     * body with a single check at the end — the rewrite latches the first error and
     * short-circuits, so clearing between regions would let a later region wipe
     * an earlier region's no-cover and the body would pass its own check.
     *
     * A no-cover is not a failure: tier-2 is an optimization on top of tier-1,
     * so losing it leaves the default, and the tier-1 walk below runs either way
     * (D8). What the tiling decides is which VARIANT each node stamps, which is
     * the next thing to wire. */
    if (tcx) {
        bbq_arena ta; bbq_arena_init(&ta, 16 * 1024);
        jav_ttree_t tree;
        if (jav_ttree_build(code, tcx, &ta, &tree)) {
            jav_tile_burg_ctx_t bc;
            jav_tile_burg_ctx_init(&bc);
            jav_tile_begin(code.data, (uint32_t)code.length);
            for (uint32_t r = 0; r < tree.nregions; r++)
                for (uint32_t i = 0; i < tree.regions[r].nroots; i++)
                    jav_tile_burg_rewrite(tree.regions[r].roots[i], &bc);
            int covered = !jav_tile_burg_has_error(&bc);
            /* A cover that fired no action on some instruction would leave that
             * offset reading 0 and the walk would stamp tier-1 there — silently
             * correct, and silently unoptimized. Counting the picks against the
             * nodes is what stops "covered" from meaning less than it says. */
            jav_ttree_note_picks(jav_tile_picked(), tree.nnodes);
            jav_ttree_note_cover(covered, jav_tile_burg_get_error_arg(&bc));
            /* A body the cover rejected keeps nothing: the map is disarmed so
             * every offset answers 0 and the walk below stamps tier-1 (D8). */
            if (!covered) jav_tile_begin(NULL, 0);
            jav_tile_burg_ctx_free(&bc);
        }
        bbq_arena_free(&ta);
    }
    jit_codebuf_t buf;
    if (jcb_init(&buf, 4096) != 0) { free(fn); return NULL; }

    /* One stencil per body byte is a safe upper bound (every opcode is ≥1 byte),
     * plus the entry and halt stencils — and then the cache transitions, which
     * are stamped BETWEEN instructions and so are not bounded by the byte count:
     * a 1-byte `i32.add` can emit itself and a spill. An instruction moves the
     * state by at most the cache size, so n+1 per byte covers it. */
    size_t code_len = code.length;
    size_t cap = (code_len + 2) * (size_t)(JAV_TIER2_N + 1);
    size_t* offs  = (size_t*)malloc(cap * sizeof *offs);   /* buffer offsets, exec order */
    int*    sids  = (int*)   malloc(cap * sizeof *sids);
    size_t* boffs = (size_t*)malloc(cap * sizeof *boffs);  /* byte offset each stencil came from */
    jit_addr_t* offmap = (jit_addr_t*)calloc(code_len + 1, sizeof *offmap);  /* IP -> address; [code_len]=halt */
    data_hole_t* recs = (data_hole_t*)malloc((cap + 4) * 8 * sizeof *recs);  /* deferred rip-rel data loads */
    int nrec = 0;
    size_t  n = 0;
    if (!offs || !sids || !boffs || !offmap || !recs) {
        free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL;
    }

    /* The stamping walk, which may run TWICE. A cover can be complete and still
     * leave a gap the stitcher cannot bridge — no transition stencil for that
     * class, or nothing that names the class in reg0 — and the answer to that is
     * D8's: keep the tier below. Below tier-2 is tier-1, not "no JIT at all", so
     * the retry disarms the tiling and stamps the plain stencils rather than
     * declining the body. Giving up outright left a function interpreted that
     * tier-1 had compiled for the whole life of the JIT. */
    size_t entry_off = 0, resync_off = 0, trap_off = 0;
    bbq_ctx_t cur;
    jav_status_t status;
    /* The class in each cache slot, slot 0 being the top. Carried along the walk
     * rather than asked of a rule: a rule names what it CONSUMES and what it
     * PRODUCES, and a value that merely survived an instruction was put there by
     * some earlier one the rule has never seen. At n=1 this was a single class
     * and the distinction did not arise. */
    int live_cls[JAV_TIER2_N > 0 ? JAV_TIER2_N : 1];
    /* Slots the machine actually has cached here. For every rule that names an
     * operand this equals the tile's recorded state; see `state_agnostic` for the
     * rules where the tile cannot say. */
    int live_st = 0;
    int retried = 0;
    const StencilDef* rd = &stencil_table[STENCIL_RESYNC];
  stamp:
    n = 0; nrec = 0;
    jcb_reset(&buf);
    memset(offmap, 0, (code_len + 1) * sizeof *offmap);
    entry_off = emit_stencil(&buf, &stencil_table[STENCIL_ENTRY], NULL, recs, &nrec);

    /* The single resync stencil: bake the (stable) offmap pointer + code length;
     * its contents are filled after finalize. Control stencils backpatch to it. */
    {
        uint64_t rvals[16] = {0};
        int rh;
        if ((rh = rd->h_offmap)  >= 0) rvals[rh] = (uint64_t)(uintptr_t)offmap;
        if ((rh = rd->h_codelen) >= 0) rvals[rh] = code_len;
        resync_off = emit_stencil(&buf, rd, rvals, recs, &nrec);
    }
    trap_off = emit_stencil(&buf, &stencil_table[STENCIL_TRAP], NULL, recs, &nrec);   /* guards backpatch here */

    cur = code;                       /* compile-time walk (a copy of the cursor) */
    status = JAV_RETURN;
    live_st = 0;
    for (int j = 0; j < JAV_TIER2_N; j++) live_cls[j] = (int)JSC_COUNT;   /* empty cache */
    for (;;) {
        /* The bound above is an argument, and an argument that stops holding is
         * a heap overflow rather than a wrong answer — so it is also a check.
         * Room for this instruction and the transitions it can imply. */
        if (n + (size_t)JAV_TIER2_N + 1 > cap) { status = JAV_TRAP; break; }
        size_t bpos = cur.pos;          /* this opcode's start — the IP branches resolve to */
        uint8_t op;
        if (!bbq_read_u8(&cur, &op)) {  /* off the end: the function's halt */
            boffs[n] = bpos;
            offs[n] = emit_stencil(&buf, &stencil_table[STENCIL_GEN_ST_HALT], NULL, recs, &nrec);
            sids[n] = STENCIL_GEN_ST_HALT; n++;
            break;
        }
        jav_jit_meta_t m;
        if (jav_jit_meta_sub[op]) {                        /* prefixed (0xFC misc &c.) */
            uint32_t sub = 0; bbq_read_uleb128_u32(&cur, &sub);
            m = jav_jit_meta_sub[op][sub];
        } else m = jav_jit_meta[op];
        /* A defensive net for a `flag:no_jit` opcode (no stencil emitted): bail so the
         * tier falls back to the interpreter. NO opcode is currently no_jit — every one
         * is JITed (br_table/call included) — so this is presently unreachable; it is
         * NOT a license to leave opcodes interp-only. */
        if (m.stencil < 0) { status = JAV_TRAP; break; }

        /* Which FORM of this instruction: the tiling said what the cache holds
         * when it runs, and jav_variant maps that to the stencil. With the tier
         * off, or on a body the cover declined, every offset reads state 0 and
         * this is the tier-1 stencil — D8's fallback as an absence rather than a
         * branch. A picked state with no variant would be the grammar and the
         * family disagreeing, which is a decline, not something to paper over. */
        /* No cover for this body — a decline, or a caller with no context to give
         * — means tier-1, and tier-1 is the PLAIN stencil, which is what the meta
         * names. Reading state 0 off a disarmed map and stamping its variant is
         * not the same thing: that variant caches its result, and with no tiling
         * behind it nothing would ever spill or read the register again. */
        int armed = jav_tile_armed() && jav_tile_picked_at((uint32_t)bpos);
        int entry = armed ? jav_tile_state_at((uint32_t)bpos) : 0;
        if (armed && state_agnostic(&m, entry) && live_st > entry) {
            /* The deepest state this instruction can still run in. Walking DOWN
             * rather than back to the tile's minimum is Ertl's minimal overflow
             * (§2.3): when the cache is full the deepest value goes to memory and
             * the rest stay, so the gap costs one spill, not a flush. */
            int st = live_st;
            while (st > entry && !agnostic_entry_ok(&m, (uint32_t)bpos, st)) st--;
            entry = st;
        }
        int chosen = armed ? jav_variant[m.stencil][entry] : m.stencil;
        /* …unless the tile asked for the result in MEMORY. A variant caches what
         * it produces, and for a class with no *_reg0 rule — v128, ref — that is
         * not what the rule reduced at. The plain stencil is the form that pushes,
         * so the tile's answer selects between them; `out_cls` is JSC_COUNT
         * exactly when the rule's left side was `X_mem`. */
        int plain = !armed;
        if (armed && m.push
            && jav_tile_out_class_at((uint32_t)bpos, 0) >= (int)(JAV_SCLASS_FINAL - 1)) {
            /* The tile asked for the result in MEMORY. At entry 0 that form is the
             * plain stencil; above it, D7s' `__sKm` — same cached operands, result
             * pushed inline. Either way the exit state is what survived, which is
             * 0 wherever these forms exist. */
            int mv = entry == 0 ? m.stencil : jav_variant_m[m.stencil][entry];
            if (mv >= 0) { chosen = mv; plain = 1; }
        }
        if (chosen < 0) { status = JAV_TRAP; break; }
        const StencilDef* def = &stencil_table[chosen];
        uint64_t vals[16] = {0};
        fill_native_holes(def, vals);
        for (int k = 0; k < m.operand_count; k++) {         /* decode EVERY operand, advancing the walk */
            /* A memarg yields TWO stencil holes from one decode: the offset (this
             * operand's hole) and the memidx (the active memory). Decode once, fill both. */
            if (m.operands[k].kind == JOP_MEMARG) {
                uint32_t fl = 0, mi = 0; bbq_read_uleb128_u32(&cur, &fl);
                if (fl & 0x40) bbq_read_uleb128_u32(&cur, &mi);
                uint64_t off = 0; bbq_read_uleb128_u64(&cur, &off);
                int ho = find_hole(def, m.operands[k].hole); if (ho >= 0) vals[ho] = off;
                int hm = def->h_memidx;                       if (hm >= 0) vals[hm] = mi;
                continue;
            }
            /* JOP_CONST is a synthesized constant (e.g. a guard's range bound):
             * its value rides in the meta — no bytecode, no cursor advance. */
            uint64_t imm = (m.operands[k].kind == JOP_CONST)
                         ? m.operands[k].value
                         : decode_operand(&cur, m.operands[k].kind);
            int h = find_hole(def, m.operands[k].hole);
            if (h >= 0) vals[h] = imm;
        }
        int hip = def->h_ip;                         /* a control stencil's post-operand position */
        size_t ip_at_tail_start = cur.pos;
        int hpc = def->h_pc;                         /* §7.1.8 trap-frame offset: this op's source byte offset */
        if (hpc >= 0) vals[hpc] = bpos;
        /* A variable-length trailing immediate the op's native reads at runtime (a `vec(...)`):
         * the compile-time walk must step over it to place the next stencil. Which kind is
         * DATA-DRIVEN from the opcode's meta (m.tail), declared in wasm.def — no per-opcode
         * special-casing here.
         *
         * _HOLE_ip placement depends on whether the BODY still reads the tail at runtime.
         * A tail the body reads (try_table) needs ip at the tail's START, where the native
         * begins reading. A tail opgen has fully consumed at compile time (br_table: the
         * count is a baked operand and the labels are dead — the targets live in the
         * side-table) needs ip AFTER it, because the side-table's delta_ip is relative to
         * the cursor past the whole immediate. */
        int ip_after_tail = 0;
        switch (m.tail) {
        case JTAIL_BRTABLE: {            /* §5.4.2 vec(labelidx) + a default labelidx */
            /* The COUNT is a fixed operand now (JOP_BRTABLE_COUNT, decoded above and
             * baked into the stencil), so only the labels remain to skip. */
            uint32_t count = g_last_brtable_count, lbl;
            for (uint32_t i = 0; i <= count; i++) bbq_read_uleb128_u32(&cur, &lbl);
            ip_after_tail = 1;
            break;
        }
        case JTAIL_TRYTABLE: {           /* §5.4.1 blocktype, then vec(catch) */
            int32_t bt = 0; bbq_read_sleb128_i32(&cur, &bt);
            if (bt == -29 || bt == -28) { int32_t ht = 0; bbq_read_sleb128_i32(&cur, &ht); }  /* (ref null? ht) blocktype: trailing heaptype */
            uint32_t nc = 0, tmp; bbq_read_uleb128_u32(&cur, &nc);
            for (uint32_t i = 0; i < nc; i++) {
                uint8_t ck = 0; bbq_read_u8(&cur, &ck);
                if (ck == 0 || ck == 1) bbq_read_uleb128_u32(&cur, &tmp);   /* tag for catch / catch_ref */
                bbq_read_uleb128_u32(&cur, &tmp);                            /* label */
            }
            break;
        }
        case JTAIL_SELECTVEC: {          /* §5.4 vec(valtype) — select t's result-type vector */
            uint32_t nt = 0; bbq_read_uleb128_u32(&cur, &nt);
            for (uint32_t i = 0; i < nt; i++) {
                uint8_t vt = 0; bbq_read_u8(&cur, &vt);
                if (vt == 0x63 || vt == 0x64) { int32_t ht = 0; bbq_read_sleb128_i32(&cur, &ht); }  /* (ref null? ht) */
            }
            break;
        }
        case JTAIL_NONE: default: break;
        }
        if (hip >= 0) vals[hip] = ip_after_tail ? cur.pos : ip_at_tail_start;
        boffs[n] = bpos;
        offs[n] = emit_stencil(&buf, def, vals, recs, &nrec);
        sids[n] = chosen; n++;

        /* The transitions. A cache state is a property of a PROGRAM POINT, so
         * where this instruction leaves the cache and where the next one expects
         * it are two facts about the same point — and when they disagree, the
         * difference IS the transition. Nothing here reads which chain rule
         * fired: it reads what the stamped variant SAYS it left behind, which is
         * the same number the grammar costed with.
         *
         * That number is published rather than recomputed because the arity
         * cannot produce it. A `word` result — `local.get`, and every other
         * polymorphic mover — pops and pushes exactly like an `i32` one and yet
         * leaves the cache empty, having no storage class to cache AS.
         *
         * Stamped in the gap, so they belong to the fall-through path. A branch
         * target is at the canonical state, so no transition ever precedes one
         * and the offmap still resolves an arriving IP to the instruction. */
        int exit_st = plain ? 0 : jav_variant_fs[m.stencil][entry];
        int next_armed = jav_tile_armed() && jav_tile_picked_at((uint32_t)cur.pos);
        int next_st = next_armed ? jav_tile_state_at((uint32_t)cur.pos) : 0;
        /* The same question `entry` asks, for the instruction this gap leads to.
         * If its rule takes no operand from the cache its recorded state is not a
         * fact about the machine and there is nothing to bridge to — leave the
         * cache as it is, PROVIDED the family has a form for it there. Where it
         * does not, the cache is full and that is Ertl's OVERFLOW (§2.3): the
         * spill is real and this gap is where it belongs. */
        if (exit_st >= 0 && next_armed && next_st != exit_st) {
            bbq_ctx_t pk = cur; uint8_t nxop;
            if (bbq_read_u8(&pk, &nxop)) {
                jav_jit_meta_t nm;
                if (jav_jit_meta_sub[nxop]) {
                    uint32_t nsub = 0; bbq_read_uleb128_u32(&pk, &nsub);
                    nm = jav_jit_meta_sub[nxop][nsub];
                } else nm = jav_jit_meta[nxop];
                if (nm.stencil >= 0 && state_agnostic(&nm, next_st)
                    && exit_st > next_st) {
                    int st = exit_st;
                    while (st > next_st && !agnostic_entry_ok(&nm, (uint32_t)cur.pos, st)) st--;
                    next_st = st;
                }
            }
        }
        if (exit_st < 0) { status = JAV_TRAP; break; }
        /* Move the slot classes the way the stencil moved the values. The variant
         * consumed the cached operands off the top, its results took slot 0, and
         * whatever survived underneath closed up behind them — the same shift the
         * emitted body does with CACHE_R<nout+j> = CACHE_R<a+j>. Survivors keep
         * their classes because nothing about them changed; only their depth did.
         *
         * In SLOTS, which is the unit `live_cls` and the state are in — a two-slot
         * result names a class in both of them, and the item counts would name
         * one and leave the other claiming nothing for the next spill to move. */
        {
            int a = m.pop_slots;
            int left = entry > a ? entry - a : 0;
            int r = exit_st > left ? m.push_slots : 0;
            /* THE MEASURE. In state `entry` the top `entry` slots are in
             * registers, so this stencil GPOPs the operands below them and GPUSHes
             * its result unless it kept it. Counted here because this is the one
             * place that knows which form was actually stamped. */
            jav_ttree_note_mem((a > entry ? a - entry : 0) + (r ? 0 : m.push_slots));
            for (int j = 0; j < left; j++) live_cls[r + j] = live_cls[a + j];
            for (int j = 0; j < r; j++)
                live_cls[j] = jav_tile_out_class_at((uint32_t)bpos, j);
            for (int j = exit_st; j < JAV_TIER2_N; j++) live_cls[j] = (int)JSC_COUNT;
        }
        int ntrans = 0, unbridged = 0;
        while (exit_st != next_st) {
            int down = exit_st > next_st;
            /* A spill moves the DEEPEST cached value because the ones remaining
             * must still be the top of the stack; the minimal organization leaves
             * no choice about which. A fill loads what the next instruction wants
             * at the slot it is about to occupy.
             *
             * A value is one slot or TWO, so the step is its WIDTH — a v128 moves
             * both halves in one transition and the state changes by two. Stepping
             * by one tried to fill a v128 into the last slot, where the other half
             * would not fit, and the gap could not be bridged. */
            int cls = down ? live_cls[exit_st - 1]
                           : jav_tile_in_class_at((uint32_t)cur.pos, exit_st);
            if (cls >= (int)(JAV_SCLASS_FINAL - 1)) {
                unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, exit_st, next_st, cls);
                break;
            }
            int w = jav_class_width[cls];
            int slot = down ? exit_st - w : exit_st;
            if (slot < 0 || slot + w > JAV_TIER2_N) {
                unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, exit_st, next_st, cls);
                break;
            }
            int tid = down ? jav_spill[cls][slot] : jav_fill[cls][slot];
            if (tid < 0) {
                unbridged = 1;
                jav_ttree_note_unbridged(op, (uint32_t)bpos, exit_st, next_st, cls);
                break;
            }
            /* Which instruction a transition BELONGS to, which is what decides
             * the IP it is stamped under and therefore whether an arrival runs
             * it. A spill disposes of what THIS instruction produced, so it is
             * this one's. A fill prepares what the NEXT instruction expects to
             * find, so it is the next one's — and it has to be, because "the gap
             * after an instruction" is only the fall-through path. A call ends
             * `TAIL return _HOLE_resync(...)`: it resumes by mapping the IP after
             * it through the offmap, not by falling into the next stencil. A fill
             * left under the call's offset is then never executed, and the
             * consumer reads a register nothing wrote — which is a call's result
             * arriving as zero. */
            boffs[n] = down ? bpos : cur.pos;
            offs[n] = emit_stencil(&buf, &stencil_table[tid], NULL, recs, &nrec);
            sids[n] = tid; n++; ntrans++;
            /* Which kind, and whether any rule had a say. `next_st` came off a
             * tile only if the next instruction was covered; otherwise it is the
             * default 0 and this transition is a region boundary the cost model
             * never saw. */
            jav_ttree_note_transition(down,
                !(jav_tile_armed() && jav_tile_picked_at((uint32_t)cur.pos)));
            jav_ttree_note_mem(w);      /* its GPUSH or GPOP, same as any other */
            /* The transition moves ONE value in or out of the deepest live slot,
             * and neither direction renumbers: reg0 stays the top either way and
             * only the count changes. */
            for (int q = 0; q < w; q++)
                live_cls[slot + q] = down ? (int)JSC_COUNT : cls;
            exit_st += down ? -w : w;
        }
        /* Every claim about cache states holds trivially of a machine with no
         * cache, so what the walk DID is recorded and gated on: a green over
         * zero cached states is a green about nothing. */
        live_st = exit_st;              /* what the machine holds, for the next one */
        jav_ttree_note_stitch(entry, ntrans, unbridged);
        /* A gap that cannot be bridged drops this body to tier-1 — recorded, so
         * it is a visible loss of coverage rather than a silent one. */
        if (unbridged) { status = JAV_TRAP; break; }
        if (status != JAV_RETURN) break;
    }

    if (status == JAV_TRAP) {
        /* One retry, without the tiling: the plain stencils have no states to
         * disagree about, so a second failure is a genuine decline. */
        if (!retried && jav_tile_armed()) {
            retried = 1;
            jav_tile_begin(NULL, 0);
            goto stamp;
        }
        free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL;
    }

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
