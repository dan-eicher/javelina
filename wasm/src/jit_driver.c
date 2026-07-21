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
#include "jit_codebuf.h"          /* jitterator's C executable code buffer */
#include "bbq_htree.h"            /* crt radix cache — footer-pool value dedup */
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

static int find_hole(const StencilDef* def, const char* name) {
    for (int i = 0; i < def->hole_count; i++)
        if (strcmp(def->hole_names[i], name) == 0) return i;
    return -1;
}
/* Resolve a _HOLE_<native> to a spec native's address (opgen's table). NULL for
 * operand / continuation holes (baked or backpatched, not symbol-resolved). */
static void* jit_sym(const char* name) {
    for (int i = 0; i < jav_jit_symbols_count; i++)
        if (strcmp(jav_jit_symbols[i].name, name) == 0) return jav_jit_symbols[i].addr;
    return NULL;
}

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

jit_func_t* jit_compile(bbq_ctx_t code) {
    jit_func_t* fn = (jit_func_t*)malloc(sizeof *fn);
    if (!fn) return NULL;
    jit_codebuf_t buf;
    if (jcb_init(&buf, 4096) != 0) { free(fn); return NULL; }

    /* One stencil per body byte is a safe upper bound (every opcode is ≥1 byte),
     * plus the entry and halt stencils. */
    size_t code_len = code.length;
    size_t cap = code_len + 2;
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

    size_t entry_off = emit_stencil(&buf, &stencil_table[STENCIL_ENTRY], NULL, recs, &nrec);

    /* The single resync stencil: bake the (stable) offmap pointer + code length;
     * its contents are filled after finalize. Control stencils backpatch to it. */
    const StencilDef* rd = &stencil_table[STENCIL_RESYNC];
    uint64_t rvals[16] = {0};
    int rh;
    if ((rh = find_hole(rd, "_HOLE_offmap"))  >= 0) rvals[rh] = (uint64_t)(uintptr_t)offmap;
    if ((rh = find_hole(rd, "_HOLE_codelen")) >= 0) rvals[rh] = code_len;
    size_t resync_off = emit_stencil(&buf, rd, rvals, recs, &nrec);
    size_t trap_off   = emit_stencil(&buf, &stencil_table[STENCIL_TRAP], NULL, recs, &nrec);   /* guards backpatch here */

    bbq_ctx_t cur = code;             /* compile-time walk (a copy of the cursor) */
    jav_status_t status = JAV_RETURN;
    for (;;) {
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

        const StencilDef* def = &stencil_table[m.stencil];
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
                int hm = find_hole(def, "_HOLE_memidx");      if (hm >= 0) vals[hm] = mi;
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
        int hip = find_hole(def, "_HOLE_ip");               /* a control stencil's post-operand position */
        size_t ip_at_tail_start = cur.pos;
        int hpc = find_hole(def, "_HOLE_pc");               /* §7.1.8 trap-frame offset: this op's source byte offset */
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
        sids[n] = m.stencil; n++;
    }

    if (status == JAV_TRAP) { free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL; }

    /* Shared footer pool: one 8-byte slot per DISTINCT constant the function's
     * rip-relative data loads need (operands, native ptrs, const holes), deduped
     * by value (few per function). Appended after all code — BEFORE the absolute
     * branch-backpatches below, because jcb_emit may move base; the data patches
     * are rel32 (offset-relative) so they survive the move, and the backpatches
     * then read the final base. */
    /* A radix cache (crt htree) keyed on a 32-bit fold of the value maps to the head
     * of a collision chain (`chain`), so each rec dedups against ~1 prior rec instead
     * of scanning all of them. `nrec` grows with function size, so the plain nested
     * scan was O(nrec²) — quadratic JIT compilation on the RTL's big functions. On an
     * allocation failure we fall back to no dedup: each rec gets its own footer slot
     * (larger pool, still correct — dedup is size, not correctness). */
    bbq_htree* pool = bbq_htree_create();
    int* chain = nrec ? (int*)malloc((size_t)nrec * sizeof *chain) : NULL;
    for (int i = 0; i < nrec; i++) {
        uint32_t key = (uint32_t)(recs[i].value ^ (recs[i].value >> 32));
        int head = (pool && chain) ? (int)(intptr_t)bbq_htree_search(pool, key) : 0;  /* 0=none, else j+1 */
        size_t foff = (size_t)-1;
        for (int j = head - 1; j >= 0; j = chain[j])
            if (recs[j].value == recs[i].value) { foff = recs[j].foff; break; }
        if (foff == (size_t)-1) { foff = buf.size; jcb_emit(&buf, (const uint8_t*)&recs[i].value, 8); }
        recs[i].foff = foff;
        if (pool && chain) { chain[i] = head - 1; bbq_htree_insert(pool, key, (void*)(intptr_t)(i + 1)); }
    }
    free(chain);
    bbq_htree_destroy(pool);
    for (int i = 0; i < nrec; i++)
        jcb_patch32(&buf, recs[i].patch_addr,
                    (int32_t)((long)recs[i].foff - (long)(recs[i].patch_addr + 4)));

    /* Linear fall-through chain (_HOLE_cont) + every control stencil's resync
     * continuation -> the one in-buffer resync stencil. Buffer base is stable now. */
    uint8_t* base = buf.base;
    const StencilDef* ed = &stencil_table[STENCIL_ENTRY];
    backpatch(&buf, entry_off, ed, find_hole(ed, "_HOLE_cont"), (uint64_t)(base + offs[0]));
    for (size_t i = 0; i < n; i++) {
        const StencilDef* d = &stencil_table[sids[i]];
        if (i + 1 < n) {
            int hc = find_hole(d, "_HOLE_cont");
            if (hc >= 0) backpatch(&buf, offs[i], d, hc, (uint64_t)(base + offs[i + 1]));
        }
        int hr = find_hole(d, "_HOLE_resync");
        if (hr >= 0) backpatch(&buf, offs[i], d, hr, (uint64_t)(base + resync_off));
        int ht = find_hole(d, "_HOLE_trap");
        if (ht >= 0) backpatch(&buf, offs[i], d, ht, (uint64_t)(base + trap_off));
    }

    void* exec = jcb_finalize(&buf);
    if (!exec) { free(offs); free(sids); free(boffs); free(offmap); free(recs); jcb_free(&buf); free(fn); return NULL; }

    /* Populate the offmap: each opcode's byte offset -> its address; the code-length
     * slot holds the halt (the off-the-end / `return` target). */
    for (size_t i = 0; i < n; i++)
        if (boffs[i] <= code_len) offmap[boffs[i]] = (jit_addr_t)((uint8_t*)exec + offs[i]);
    offmap[code_len] = (jit_addr_t)((uint8_t*)exec + offs[n - 1]);   /* halt stamped last */

    free(offs); free(sids); free(boffs); free(recs);   /* scratch; offmap + buf belong to the handle */
    fn->buf = buf; fn->entry_off = entry_off; fn->offmap = offmap;
    return fn;
}

/* Enter a compiled function. The caller has set up vm->frame (code, locals, side-
 * table); this runs the stamped chain to its halt and reports the outcome. */
jav_status_t jit_enter(const jit_func_t* fn, vm_t* vm) {
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
    jit_func_t* fn = jit_compile(vm->frame.code);
    if (!fn) return JAV_TRAP;
    jav_status_t st = jit_enter(fn, vm);
    jit_free(fn);
    return st;
}
