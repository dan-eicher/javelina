/*
 * validate.c — the single-pass side-table builder (Titzer §3.1 + Listing 1).
 *
 * One forward pass over the function body, simulating the abstract control +
 * value stacks (heights from opgen's per-opcode pop/push metadata). Each branch
 * (if/else/br/br_if) appends a side-table entry; forward targets (to a block/if
 * end) are backpatched when the matching `end` is seen; loop targets are
 * backward (known immediately). A final pass turns recorded target IPs into the
 * delta_ip / delta_stp the interpreter and JIT consume.
 */
#include "validate.h"
#include "jav_jit_meta.h"   /* jav_jit_meta: operand kind + pop/push per opcode */
#include "jav_type_meta.h"  /* jav_opsig[]: per-opcode value-type signature (§7.6 transfer fns) */
#include "bbq_runtime.h"     /* the shared LEB readers */
#include "bbq_vec.h"          /* the crt growable array — the side-table/try-table accumulators */
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH   1024

/* ════════════════════════════════════════════════════════════════════════════
 * jav_typecheck — the WASM 3.0 §7.6 reference validation algorithm.
 *
 * A SINGLE forward pass (no worklist: WASM is structured, so there are no CFG
 * merge points to iterate — this is "Leroy's abstract interpreter minus the
 * dataflow fixpoint"). It maintains an operand-TYPE stack `vals` and a control-
 * frame stack `ctrls`; an instruction's transfer function pops its argument types
 * and pushes its result types (the ordinary opcodes' signatures come from opgen's
 * generated jav_opsig[], single-sourced with execution). Code after an
 * unconditional transfer (unreachable/br/return) is stack-polymorphic: the frame
 * is marked unreachable and pop_val yields the Bot type instead of underflowing,
 * so the dead code is still type-checked but matches any expected type.
 *
 * The Titzer side-table ⟨Δip,Δstp,vals,pop⟩ falls out of the control frames as a
 * by-product (a branch keeps |label_types(frame)| values and unwinds to the
 * frame's val_height) — the type checker SUBSUMES the old height-only builder.
 * ════════════════════════════════════════════════════════════════════════════ */

/* An operand type on the value stack: a number (num = WVT_I32..V128), a reference
 * (num = VT_K_REF, with nullability + a §3.3 heaptype), or Bot (dead code, matches
 * anything). This replaces the flat enum + vtidx so EVERY ref carries (nullable, ht)
 * and matching goes through the real lattice — no exact-match shortcuts. */
enum { VT_K_REF = -1, VT_K_BOT = -2 };
typedef struct { int16_t num; uint8_t nullable; int32_t ht; } vtype_t;
#define VT_BOT       ((vtype_t){ VT_K_BOT, 0, 0 })
#define VT_NUM(w)    ((vtype_t){ (int16_t)(w), 0, 0 })
#define VT_REF(n,h)  ((vtype_t){ VT_K_REF, (uint8_t)(n), (int32_t)(h) })
static int vt_is_ref(vtype_t t) { return t.num == VT_K_REF; }
static int vt_is_bot(vtype_t t) { return t.num == VT_K_BOT; }

/* Convert a boundary value type (the flat enum + optional concrete typeidx) to the
 * internal vtype. Boundary struct/array/exn refs are NULLABLE (declared slots);
 * ops that yield a non-null ref (struct.new, ref.cast (ref ht)) build VT_REF(0,…). */
static vtype_t vt_from(jav_valtype_t w, uint32_t tidx) {
    switch (w) {
    case WVT_I32: case WVT_I64: case WVT_F32: case WVT_F64: case WVT_V128: return VT_NUM(w);
    case WVT_REF:          return VT_REF(1, (int32_t)tidx);   /* generic (ref null heaptype) */
    case WVT_REF_NN:       return VT_REF(0, (int32_t)tidx);   /* generic (ref heaptype) */
    default:               return VT_BOT;
    }
}

/* §4.2.1 defaultable: a number/vector or a NULLABLE reference (non-null refs have no
 * default). Gates struct.new_default / array.new_default. A packed field is stored
 * unpacked (i32) here, so it is always defaultable — exactly the spec's unpack(zt). */
static int vt_defaultable(jav_valtype_t w) {
    switch (w) {
    case WVT_I32: case WVT_I64: case WVT_F32: case WVT_F64: case WVT_V128: case WVT_REF: return 1;
    default: return 0;                                  /* WVT_REF_NN / WVT_BOT */
    }
}
/* §2.3.2 numtype | vectype — array.new_data / init_data require a non-reference element. */
static int vt_is_numvec(jav_valtype_t w) {
    return w == WVT_I32 || w == WVT_I64 || w == WVT_F32 || w == WVT_F64 || w == WVT_V128;
}
/* a <: e as a pure relation (numbers exact, refs via the §3.3 lattice; no Bot here). */
static int vt_sub(const jav_subtype_ctx_t* lat, vtype_t a, vtype_t e) {
    if (vt_is_ref(a) && vt_is_ref(e)) return jav_rt_sub(lat, a.nullable, a.ht, e.nullable, e.ht);
    if (!vt_is_ref(a) && !vt_is_ref(e)) return a.num == e.num;
    return 0;
}
/* §3.3.9 storage matching incl. packed widths: a's storage <: b's storage. A packed type
 * matches only an identical packed type (i8↔i8, i16↔i16); never a value type. */
static int storage_sub(const jav_subtype_ctx_t* lat,
                       jav_valtype_t aw, uint32_t ax, uint8_t apk,
                       jav_valtype_t bw, uint32_t bx, uint8_t bpk) {
    if (apk || bpk) return apk == bpk;
    return vt_sub(lat, vt_from(aw, ax), vt_from(bw, bx));
}

typedef struct {
    uint8_t  opcode;        /* 0x02 block, 0x03 loop, 0x04 if, 0x05 else, 0x00 = func body */
    const jav_valtype_t* start; const uint32_t* start_tidx; uint16_t nstart;  /* input types  (loop label / entry params) */
    const jav_valtype_t* end;   const uint32_t* end_tidx;   uint16_t nend;    /* output types (block/if label / results)  */
    int      val_height;    /* vals height at frame entry (below the block's own operands) */
    int      init_height;   /* inits-stack height at frame entry: locals set deeper are rolled back at pop (§3.4.2) */
    int      unreachable;   /* the rest of this block is dead (stack-polymorphic) */
    size_t   loop_start;    /* loop backward-branch target */
    int*     fixups;        /* side-table entries branching to this frame's END (forward) — a bbq_vec,
                             * grown without a cap (a br_table can target one block from any number of arms) */
    int      else_e;        /* the if's pending side-table entry, or -1 */
    int      try_idx;       /* index into the try-table metadata if this is a try_table frame, else -1 */
    jav_valtype_t single[1];   /* backing store for a single-valtype block result */
    uint32_t single_tidx[1];   /* its heaptype, when that single valtype is a reference */
} cframe_t;

typedef struct {
    vtype_t* vals; int nvals;            /* the operand-type stack */
    cframe_t* ctrls;      int nctrls;        /* the control-frame stack (top = ctrls[nctrls-1]) */
    int ok;
    jav_err_t err;       /* §7.6 reject reason; defaults to JAV_E_TYPE_MISMATCH, refined at specific sites */
    const jav_subtype_ctx_t* lat;       /* the §3.3 subtype context (cx->lattice) */
    jav_st_entry_t* st;  size_t* bpos; size_t* tip;   /* side-table accumulation — bbq_vecs, grown without a cap */
    int*     eskip;      /* per-entry EXTRA delta_stp (an if-false entry skips its co-located else entry) */
    jav_try_t* trytab;   /* per-try_table metadata accumulation — a bbq_vec (one entry per try_table) */
    uint8_t* locals_init;   /* §3.4.2 per-local init flag (1 = set); params + defaultable start set */
    uint32_t* inits;        /* a bbq_vec stack of localidx set since their frame's entry, for pop-rollback */
} tcst;

/* Append a zero-initialized side-table slot (st/bpos/tip grow together) and return its
 * index. §7.6 emits one entry per branch/control instruction — unbounded by anything but
 * code size, so this is a growable vec, never a fixed cap. */
static unsigned st_push(tcst* s) {
    jav_st_entry_t z; memset(&z, 0, sizeof z); size_t zp = 0; int zk = 0;
    bbq_vec_push(s->st, z); bbq_vec_push(s->bpos, zp); bbq_vec_push(s->tip, zp); bbq_vec_push(s->eskip, zk);
    return (unsigned)bbq_vec_len(s->st) - 1;
}

static cframe_t* tc_top(tcst* s) { return &s->ctrls[s->nctrls - 1]; }

static void tc_push_vt(tcst* s, vtype_t t) {
    if (s->nvals < MAX_STACK) s->vals[s->nvals++] = t; else s->ok = 0;
}
static void tc_push(tcst* s, jav_valtype_t w) { tc_push_vt(s, vt_from(w, 0)); }
/* push a reference of a given nullability + heaptype (abstract code or concrete typeidx) */
static void tc_push_ref(tcst* s, int nullable, int32_t ht) { tc_push_vt(s, VT_REF(nullable, ht)); }

/* §7.6 pop_val: below the current frame's height, yield Bot if unreachable
 * (stack-polymorphism), else underflow. */
static vtype_t tc_pop(tcst* s) {
    cframe_t* f = tc_top(s);
    if (s->nvals <= f->val_height) { if (!f->unreachable) s->ok = 0; return VT_BOT; }
    return s->vals[--s->nvals];
}
/* a <: e under the module's §3.3 lattice (Bot matches anything either way). */
static int tc_matches(tcst* s, vtype_t a, vtype_t e) {
    if (vt_is_bot(a) || vt_is_bot(e)) return 1;
    if (vt_is_ref(a) && vt_is_ref(e)) return jav_rt_sub(s->lat, a.nullable, a.ht, e.nullable, e.ht);
    if (!vt_is_ref(a) && !vt_is_ref(e)) return a.num == e.num;   /* numbers: exact */
    return 0;
}
static vtype_t tc_pop_e(tcst* s, jav_valtype_t w) {
    vtype_t a = tc_pop(s);
    if (!tc_matches(s, a, vt_from(w, 0))) s->ok = 0;
    return a;
}
/* pop, requiring a subtype of (ref null? ht) — the real check, no exact-match. */
static void tc_pop_ref_ht(tcst* s, int nullable, int32_t ht) {
    vtype_t a = tc_pop(s);
    if (!tc_matches(s, a, VT_REF(nullable, ht))) s->ok = 0;
}
/* pop, requiring a subtype of a full vtype (number exact, ref by §3.3 subtyping). */
static void tc_pop_vt(tcst* s, vtype_t e) {
    vtype_t a = tc_pop(s);
    if (!tc_matches(s, a, e)) s->ok = 0;
}
static void tc_push_vals(tcst* s, const jav_valtype_t* ts, const uint32_t* tx, int n) {
    for (int i = 0; i < n; i++) tc_push_vt(s, vt_from(ts[i], tx ? tx[i] : 0));
}
static void tc_pop_vals(tcst* s, const jav_valtype_t* ts, const uint32_t* tx, int n) {
    for (int i = n - 1; i >= 0; i--) {                 /* reverse: top of stack first */
        vtype_t a = tc_pop(s);
        if (!tc_matches(s, a, vt_from(ts[i], tx ? tx[i] : 0))) s->ok = 0;
    }
}
/* pop any reference type (or Bot); return it so callers can refine nullability. */
static vtype_t tc_pop_ref(tcst* s) {
    vtype_t a = tc_pop(s);
    if (!vt_is_ref(a) && !vt_is_bot(a)) s->ok = 0;
    return a;
}
/* A table's static type (§3.4.4 `C.tables[x] = at lim rt`), drawn from the context's
 * parallel arrays — NOT the funcref/i32 the single `table0` model used to assume.
 * addrtype: i64 iff a table64 (else i32); reftype: the table's element type as a vtype. */
static jav_valtype_t tbl_at(const jav_vctx_t* cx, uint32_t t) {
    return (cx->table_is64 && t < cx->ntables && cx->table_is64[t]) ? WVT_I64 : WVT_I32;
}
static vtype_t tbl_rt(const jav_vctx_t* cx, uint32_t t) {
    if (!cx->table_reftype) return VT_REF(1, HT_FUNC);            /* defensive default: funcref */
    return vt_from(cx->table_reftype[t], cx->table_tidx ? cx->table_tidx[t] : 0);
}
/* An element segment's reference type (§3.4.4 `C.elems[y] = rt`); default funcref. */
static vtype_t elem_rt(const jav_vctx_t* cx, uint32_t y) {
    if (!cx->elem_reftype) return VT_REF(1, HT_FUNC);
    return vt_from(cx->elem_reftype[y], cx->elem_tidx ? cx->elem_tidx[y] : 0);
}
static cframe_t* tc_push_ctrl(tcst* s, uint8_t opcode,
                              const jav_valtype_t* start, const uint32_t* start_tidx, int nstart,
                              const jav_valtype_t* end, const uint32_t* end_tidx, int nend) {
    cframe_t* f = &s->ctrls[s->nctrls++];
    f->opcode = opcode; f->start = start; f->start_tidx = start_tidx; f->nstart = (uint16_t)nstart;
    f->end = end; f->end_tidx = end_tidx; f->nend = (uint16_t)nend;
    f->val_height = s->nvals; f->unreachable = 0;
    f->init_height = (int)bbq_vec_len(s->inits);
    f->fixups = NULL; f->else_e = -1; f->loop_start = 0; f->try_idx = -1;
    tc_push_vals(s, start, start_tidx, nstart);
    return f;
}
/* §3.4.2 roll the inits stack back to `height`, marking each rolled-off local unset
 * again: a local set inside a block isn't guaranteed set once the block is left. */
static void tc_reset_locals(tcst* s, int height) {
    for (int n = (int)bbq_vec_len(s->inits); n > height; n--)
        s->locals_init[s->inits[n - 1]] = 0;
    bbq_vec_truncate(s->inits, height);
}
static void tc_unreachable(tcst* s) {
    cframe_t* f = tc_top(s);
    s->nvals = f->val_height; f->unreachable = 1;
}
/* a branch's target label keeps the loop's INPUT or the block/if's OUTPUT types */
static const jav_valtype_t* tc_label(cframe_t* f, const uint32_t** tx, int* n) {
    if (f->opcode == 0x03) { *n = f->nstart; *tx = f->start_tidx; return f->start; }
    *n = f->nend; *tx = f->end_tidx; return f->end;
}

/* Map a negative single-valtype blocktype/select encoding to its value type; for the
 * funcref/externref ref shorthands the heaptype is returned via *ht (else *ht is 0). */
static jav_valtype_t tc_vt_from_sleb(int32_t bt, int32_t* ht) {
    *ht = 0;
    switch (bt) {
    case -1:  return WVT_I32;  case -2:  return WVT_I64;
    case -3:  return WVT_F32;  case -4:  return WVT_F64;
    case -5:  return WVT_V128;
    default:
        /* §5.3.4 abstract-heaptype reference shorthands (0x69 exn … 0x74 noexn): the sleb
         * value IS the HT_* code; each denotes a NULLABLE (ref null absheaptype). */
        if (bt >= HT_EXN && bt <= HT_NOEXN) { *ht = bt; return WVT_REF; }   /* [-23, -12] */
        return WVT_BOT;   /* invalid encoding */
    }
}
/* Resolve an encoded blocktype (§5.4) into the frame's start/end type sequences
 * (+ the parallel concrete-typeidx arrays, NULL when none). */
static void tc_blocktype(int32_t bt, const jav_vctx_t* cx, cframe_t* f,
                         const jav_valtype_t** start, const uint32_t** start_tidx, int* nstart,
                         const jav_valtype_t** end, const uint32_t** end_tidx, int* nend, int* ok,
                         jav_err_t* errp) {
    *start_tidx = NULL; *end_tidx = NULL;
    if (bt == -64) { *start = NULL; *nstart = 0; *end = NULL; *nend = 0; }     /* empty */
    else if (bt < 0) {                                                        /* single valtype */
        int32_t ht = 0;
        jav_valtype_t t = tc_vt_from_sleb(bt, &ht);
        if (t == WVT_BOT) *ok = 0;
        f->single[0] = t; f->single_tidx[0] = (uint32_t)ht;
        *start = NULL; *nstart = 0; *end = f->single; *end_tidx = f->single_tidx; *nend = 1;
    } else if (cx->types && (unsigned)bt < cx->ntypes) {                      /* typeidx — §3.2.8: must expand to a FUNC type */
        if (cx->lattice && cx->lattice->kinds && cx->lattice->kinds[bt] != WST_FUNC) { *ok = 0; *start = NULL; *nstart = 0; *end = NULL; *nend = 0; return; }
        const jav_functype_t* ft = &cx->types[bt];
        *start = ft->params;  *start_tidx = ft->param_tidx;  *nstart = ft->nparams;
        *end   = ft->results; *end_tidx   = ft->result_tidx; *nend  = ft->nresults;
    } else { *ok = 0; if (errp) *errp = JAV_E_UNKNOWN_TYPE; *start = NULL; *nstart = 0; *end = NULL; *nend = 0; }    /* bad typeidx */
}

/* Read + resolve a §5.4 blocktype from the stream. The s33 form covers 0x40 (empty),
 * a single numtype/vectype/abstract-shorthand valtype, and a typeidx. The 0x63 / 0x64
 * constructors (-29 / -28 as s33) introduce a two-byte (ref null? ht) valtype whose
 * heaptype byte must also be consumed — a single SLEB read would mis-decode it. */
static void tc_read_blocktype(bbq_ctx_t* c, const jav_vctx_t* cx, cframe_t* f,
                              const jav_valtype_t** start, const uint32_t** start_tidx, int* nstart,
                              const jav_valtype_t** end, const uint32_t** end_tidx, int* nend, int* ok,
                              jav_err_t* errp) {
    int32_t bt = 0; if (!bbq_read_sleb128_s33(c, &bt)) { *ok = 0; if (errp) *errp = JAV_E_UNKNOWN_TYPE; }   /* §5.3.3 s33 blocktype */
    if (bt == -28 || bt == -29) {                         /* (ref ht) = 0x64 / (ref null ht) = 0x63 */
        int32_t htx = 0; if (!bbq_read_sleb128_s33(c, &htx)) { *ok = 0; if (errp) *errp = JAV_E_UNKNOWN_TYPE; }   /* §5.3.3 s33 */
        if (htx >= 0) { if ((unsigned)htx >= cx->ntypes) { *ok = 0; if (errp) *errp = JAV_E_UNKNOWN_TYPE; } }   /* concrete typeidx in range */
        else if (htx < HT_EXN || htx > HT_NOEXN) { *ok = 0; if (errp) *errp = JAV_E_UNKNOWN_TYPE; }             /* a known abstract heaptype code */
        f->single[0] = (bt == -28) ? WVT_REF_NN : WVT_REF; f->single_tidx[0] = (uint32_t)htx;
        *start = NULL; *start_tidx = NULL; *nstart = 0;
        *end = f->single; *end_tidx = f->single_tidx; *nend = 1;
        return;
    }
    tc_blocktype(bt, cx, f, start, start_tidx, nstart, end, end_tidx, nend, ok, errp);
}

/* Skip an ordinary opcode's bytecode immediates (it still type-checks via the
 * opsig). Operand encodings come from jav_jit_meta; m may be the prefixed row. */
static void tc_skip_operands(bbq_ctx_t* c, jav_jit_meta_t m) {
    for (int k = 0; k < m.operand_count; k++) {
        switch (m.operands[k].kind) {
        /* br_table's label count reads like any uleb32 here; the label vector itself
         * is the tail, skipped by the caller per meta.tail. */
        case JOP_BRTABLE_COUNT:
        case JOP_ULEB32: { uint32_t v; bbq_read_uleb128_u32(c, &v); break; }
        case JOP_ULEB64: { uint64_t v; bbq_read_uleb128_u64(c, &v); break; }
        case JOP_SLEB32: { int32_t  v; bbq_read_sleb128_i32(c, &v); break; }
        case JOP_SLEB64: { int64_t  v; bbq_read_sleb128_i64(c, &v); break; }
        case JOP_F32:    { float    v; bbq_read_f32le(c, &v); break; }
        case JOP_F64:    { double   v; bbq_read_f64le(c, &v); break; }
        case JOP_U8:     { uint8_t  v; bbq_read_u8(c, &v); break; }
        case JOP_MEMARG: { uint32_t fl=0; bbq_read_uleb128_u32(c,&fl);
                           if (fl & 0x40) { uint32_t mi=0; bbq_read_uleb128_u32(c,&mi); }
                           uint64_t off=0; bbq_read_uleb128_u64(c,&off); break; }
        case JOP_BLOCKTYPE: { int32_t v=0; bbq_read_sleb128_i32(c,&v);   /* §5.3.3 blocktype s33 */
                              if (v==-29||v==-28){ int32_t ht=0; bbq_read_sleb128_i32(c,&ht); } break; }   /* (ref null? ht): trailing heaptype */
        case JOP_CONST: case JOP_NONE: break;
        }
    }
}

/* pop/push a single (valtype, concrete-typeidx) — uniform over numbers and refs via
 * vt_from, so a struct field / array element of any type needs no special-casing. */
static void tc_pop1(tcst* s, jav_valtype_t w, uint32_t tx) {
    vtype_t a = tc_pop(s);
    if (!tc_matches(s, a, vt_from(w, tx))) s->ok = 0;
}
static void tc_push1(tcst* s, jav_valtype_t w, uint32_t tx) { tc_push_vt(s, vt_from(w, tx)); }

/* Which §5 reason an out-of-range data index carries. cx->ndatas is the data COUNT
 * section's value (§5.5.15), so with no such section it is 0 and EVERY data index is
 * already out of range — §5.5.17's `n? != eps \/ dataidx(func*) = eps` is enforced by
 * the ordinary bound check, and this only says which of the two conditions failed. */
static jav_err_t data_err(const jav_vctx_t* cx) {
    return cx->have_datacount ? JAV_E_UNKNOWN_DATA : JAV_E_DATA_COUNT_REQUIRED;
}

/* §3.4.5 memory-instruction validation (the security boundary): decode the memidx
 * (default 0, or bit 6 of the memarg align-flags), bound it against nmemories, check
 * the memarg (2^align ≤ N/8, and offset < 2^|at|), bound the lane immediate, and type
 * the address operands at the target memory's index type `at` (i32 / i64 — memory64).
 * Returns 1 if `op`/`sub` is a memory instruction (now fully handled), 0 otherwise. */
static int tc_mem(tcst* s, const jav_vctx_t* cx, uint8_t op, int sub, bbq_ctx_t* c) {
    enum { LD, ST, LDL, STL, SZ, GR, FILL, COPY, INIT, NONE } k = NONE;
    int N = 0; jav_valtype_t dt = WVT_I32;
    if (op != 0xfd && op != 0xfc) {
        switch (op) {
        case 0x28: k=LD; N=32; dt=WVT_I32; break;  case 0x29: k=LD; N=64; dt=WVT_I64; break;
        case 0x2a: k=LD; N=32; dt=WVT_F32; break;  case 0x2b: k=LD; N=64; dt=WVT_F64; break;
        /* §3.4.5 sub-word loads iN.loadK_sx : at -> iN, memarg valid for K (the STORAGE width). */
        case 0x2c: k=LD; N=8;  dt=WVT_I32; break;  case 0x2d: k=LD; N=8;  dt=WVT_I32; break;  /* i32.load8_s/_u  */
        case 0x2e: k=LD; N=16; dt=WVT_I32; break;  case 0x2f: k=LD; N=16; dt=WVT_I32; break;  /* i32.load16_s/_u */
        case 0x30: k=LD; N=8;  dt=WVT_I64; break;  case 0x31: k=LD; N=8;  dt=WVT_I64; break;  /* i64.load8_s/_u  */
        case 0x32: k=LD; N=16; dt=WVT_I64; break;  case 0x33: k=LD; N=16; dt=WVT_I64; break;  /* i64.load16_s/_u */
        case 0x34: k=LD; N=32; dt=WVT_I64; break;  case 0x35: k=LD; N=32; dt=WVT_I64; break;  /* i64.load32_s/_u */
        case 0x36: k=ST; N=32; dt=WVT_I32; break;  case 0x37: k=ST; N=64; dt=WVT_I64; break;
        case 0x38: k=ST; N=32; dt=WVT_F32; break;  case 0x39: k=ST; N=64; dt=WVT_F64; break;
        /* §3.4.5 sub-word stores iN.storeK : at iN -> ε, memarg valid for K. */
        case 0x3a: k=ST; N=8;  dt=WVT_I32; break;  case 0x3b: k=ST; N=16; dt=WVT_I32; break;  /* i32.store8/16 */
        case 0x3c: k=ST; N=8;  dt=WVT_I64; break;  case 0x3d: k=ST; N=16; dt=WVT_I64; break;  /* i64.store8/16 */
        case 0x3e: k=ST; N=32; dt=WVT_I64; break;                                             /* i64.store32   */
        case 0x3f: k=SZ; break;                    case 0x40: k=GR; break;
        default: return 0;
        }
    } else if (op == 0xfd) {
        switch (sub) {
        case 0:  k=LD; N=128; dt=WVT_V128; break;  case 11: k=ST; N=128; dt=WVT_V128; break;
        /* §3.4.5 v128.loadKxM_sx: the access is K·M = 64 bits (8 bytes) → align ≤ 3. */
        case 1: case 2: case 3: case 4: case 5: case 6: k=LD; N=64; dt=WVT_V128; break;
        case 92: k=LD; N=32; dt=WVT_V128; break;   /* v128.load32_zero */
        case 93: k=LD; N=64; dt=WVT_V128; break;   /* v128.load64_zero */
        case 7:  k=LD; N=8;  dt=WVT_V128; break;   case 8:  k=LD; N=16; dt=WVT_V128; break;
        case 9:  k=LD; N=32; dt=WVT_V128; break;   case 10: k=LD; N=64; dt=WVT_V128; break;
        case 84: k=LDL; N=8; break;  case 85: k=LDL; N=16; break;
        case 86: k=LDL; N=32; break; case 87: k=LDL; N=64; break;
        case 88: k=STL; N=8; break;  case 89: k=STL; N=16; break;
        case 90: k=STL; N=32; break; case 91: k=STL; N=64; break;
        default: return 0;
        }
    } else {  /* 0xfc */
        switch (sub) { case 11: k=FILL; break; case 10: k=COPY; break; case 8: k=INIT; break; default: return 0; }
    }
    #define AT64(mi) (cx->mem_is64 && (mi) < cx->nmemories && cx->mem_is64[mi])
    if (k==LD || k==ST || k==LDL || k==STL) {
        uint32_t flags = 0, memidx = 0; bbq_read_uleb128_u32(c, &flags);
        if (flags & 0x40) bbq_read_uleb128_u32(c, &memidx);
        uint64_t off = 0; bbq_read_uleb128_u64(c, &off);
        uint8_t lane = 0; if (k==LDL || k==STL) bbq_read_u8(c, &lane);
        if (memidx >= cx->nmemories) { s->err = JAV_E_UNKNOWN_MEMORY; s->ok = 0; return 1; }
        int at64 = AT64(memidx);
        uint32_t align = (flags & 0x40) ? flags - 64 : flags;
        int maxa = N==8?0 : N==16?1 : N==32?2 : N==64?3 : 4;       /* 2^align ≤ N/8 */
        if (align > (uint32_t)maxa) { s->err = JAV_E_ALIGNMENT; s->ok = 0; return 1; }
        if (!at64 && (off >> 32) != 0) { s->err = JAV_E_OFFSET_OUT_OF_RANGE; s->ok = 0; return 1; }    /* offset < 2^|at| (i32) */
        if ((k==LDL || k==STL) && lane >= (uint8_t)(128 / N)) { s->err = JAV_E_INVALID_LANE; s->ok = 0; return 1; }
        jav_valtype_t at = at64 ? WVT_I64 : WVT_I32;
        if      (k==LD)  { tc_pop_e(s, at); tc_push(s, dt); }
        else if (k==ST)  { tc_pop_e(s, dt); tc_pop_e(s, at); }
        else if (k==LDL) { tc_pop_e(s, WVT_V128); tc_pop_e(s, at); tc_push(s, WVT_V128); }
        else             { tc_pop_e(s, WVT_V128); tc_pop_e(s, at); }   /* STL */
        return 1;
    }
    if (k==SZ || k==GR) {
        uint32_t memidx = 0; bbq_read_uleb128_u32(c, &memidx);
        if (memidx >= cx->nmemories) { s->err = JAV_E_UNKNOWN_MEMORY; s->ok = 0; return 1; }
        jav_valtype_t at = AT64(memidx) ? WVT_I64 : WVT_I32;
        if (k==SZ) tc_push(s, at); else { tc_pop_e(s, at); tc_push(s, at); }
        return 1;
    }
    if (k==FILL) {
        uint32_t memidx = 0; bbq_read_uleb128_u32(c, &memidx);
        if (memidx >= cx->nmemories) { s->err = JAV_E_UNKNOWN_MEMORY; s->ok = 0; return 1; }
        jav_valtype_t at = AT64(memidx) ? WVT_I64 : WVT_I32;
        tc_pop_e(s, at); tc_pop_e(s, WVT_I32); tc_pop_e(s, at);     /* n, val, addr */
        return 1;
    }
    if (k==COPY) {
        uint32_t d = 0, srcm = 0; bbq_read_uleb128_u32(c, &d); bbq_read_uleb128_u32(c, &srcm);
        if (d >= cx->nmemories || srcm >= cx->nmemories) { s->err = JAV_E_UNKNOWN_MEMORY; s->ok = 0; return 1; }
        int a1 = AT64(d), a2 = AT64(srcm);
        jav_valtype_t at1 = a1?WVT_I64:WVT_I32, at2 = a2?WVT_I64:WVT_I32;
        jav_valtype_t atmin = (a1 && a2) ? WVT_I64 : WVT_I32;      /* min(at1,at2) */
        tc_pop_e(s, atmin); tc_pop_e(s, at2); tc_pop_e(s, at1);     /* n, src, dst */
        return 1;
    }
    /* INIT */
    uint32_t seg = 0, memidx = 0; bbq_read_uleb128_u32(c, &seg); bbq_read_uleb128_u32(c, &memidx);
    if (seg >= cx->ndatas || memidx >= cx->nmemories) { s->err = (memidx >= cx->nmemories) ? JAV_E_UNKNOWN_MEMORY : data_err(cx); s->ok = 0; return 1; }
    jav_valtype_t at = AT64(memidx) ? WVT_I64 : WVT_I32;
    tc_pop_e(s, WVT_I32); tc_pop_e(s, WVT_I32); tc_pop_e(s, at);    /* n, src, addr */
    #undef AT64
    return 1;
}

int jav_typecheck(const uint8_t* code, size_t len, const jav_vctx_t* cx,
                   jav_st_entry_t** out_st, unsigned* out_n) {
    return jav_typecheck_ex(code, len, cx, out_st, out_n, NULL, NULL, NULL);
}

int jav_typecheck_ex(const uint8_t* code, size_t len, const jav_vctx_t* cx,
                      jav_st_entry_t** out_st, unsigned* out_n,
                      jav_try_t** out_try, unsigned* out_ntry, jav_err_t* out_err) {
    tcst s;
    s.vals   = malloc(MAX_STACK * sizeof *s.vals);   /* operand-type stack: MAX_STACK is the engine's */
    s.ctrls  = malloc(MAX_DEPTH * sizeof *s.ctrls);  /* enforced height/depth limit (runtime reserves it) */
    s.st = NULL; s.bpos = NULL; s.tip = NULL; s.eskip = NULL; s.trytab = NULL;   /* bbq_vecs, grown on demand */
    s.nvals = 0; s.nctrls = 0; s.ok = 1; s.err = JAV_E_TYPE_MISMATCH;   /* refined at specific reject sites */
    s.lat = cx->lattice;                            /* the §3.3 subtype context */
    int max_height = 0;

    /* §3.4.2 local-init seed: params (the first nparams locals) and defaultable locals
     * start initialized; a declared non-null reference local starts uninitialized. */
    s.inits = NULL;
    s.locals_init = cx->nlocals ? malloc(cx->nlocals) : NULL;
    for (unsigned i = 0; i < cx->nlocals; i++)
        s.locals_init[i] = (i < cx->nparams) || vt_defaultable(cx->locals[i]);

    /* the implicit outermost block: result type = the function's results, so a
     * branch to the outermost label (a `return`) keeps the function results. */
    tc_push_ctrl(&s, 0x02, NULL, NULL, 0, cx->results, cx->result_tidx, (int)cx->nresults);

    bbq_ctx_t c; bbq_ctx_init(&c, code, len);
    while (s.ok) {
        uint8_t op;
        if (!bbq_read_u8(&c, &op)) break;
        if (s.nctrls == 0) { s.ok = 0; break; }   /* instruction after the outermost end */

        switch (op) {
        /* unreachable */
        case 0x00: {
            tc_unreachable(&s);
            break;
        }
        /* nop */
        case 0x01: {
            /* no stack effect */
            break;
        }
        /* block / loop */
        case 0x02: case 0x03: {
            if (s.nctrls >= MAX_DEPTH) { s.ok = 0; break; }
            const jav_valtype_t *st_t, *en_t; const uint32_t *st_x, *en_x; int nst_t, nen_t;
            cframe_t scratch;
            tc_read_blocktype(&c, cx, &scratch, &st_t, &st_x, &nst_t, &en_t, &en_x, &nen_t, &s.ok, &s.err);
            tc_pop_vals(&s, st_t, st_x, nst_t);          /* consume the block's params */
            cframe_t* f = tc_push_ctrl(&s, op, st_t, st_x, nst_t, en_t, en_x, nen_t);
            if (en_t == scratch.single) {                /* single-valtype: repoint to the frame's own backing */
                f->single[0] = scratch.single[0]; f->end = f->single;
                f->single_tidx[0] = scratch.single_tidx[0]; f->end_tidx = f->single_tidx;
            }
            f->loop_start = c.pos;                       /* loop's backward target */
            break;
        }
        /* if */
        case 0x04: {
            const jav_valtype_t *st_t, *en_t; const uint32_t *st_x, *en_x; int nst_t, nen_t;
            cframe_t scratch;
            tc_read_blocktype(&c, cx, &scratch, &st_t, &st_x, &nst_t, &en_t, &en_x, &nen_t, &s.ok, &s.err);
            tc_pop_e(&s, WVT_I32);                        /* the condition */
            tc_pop_vals(&s, st_t, st_x, nst_t);          /* the if's params */
            unsigned e = st_push(&s);
            s.bpos[e] = c.pos; s.st[e].vals = 0; s.st[e].pop = 0;
            cframe_t* f = tc_push_ctrl(&s, 0x04, st_t, st_x, nst_t, en_t, en_x, nen_t);
            if (en_t == scratch.single) {
                f->single[0] = scratch.single[0]; f->end = f->single;
                f->single_tidx[0] = scratch.single_tidx[0]; f->end_tidx = f->single_tidx;
            }
            f->else_e = (int)e;
            break;
        }
        /* else */
        case 0x05: {
            cframe_t* cc = tc_top(&s);
            if (cc->opcode != 0x04) { s.ok = 0; break; }
            /* close the then-arm: pop its results, reset to the if's params */
            tc_pop_vals(&s, cc->end, cc->end_tidx, cc->nend);
            if (s.nvals != cc->val_height) { s.ok = 0; break; }
            s.tip[cc->else_e] = c.pos;                    /* if-false → else body */
            s.eskip[cc->else_e] = 1;                      /* …and skip the else's own (co-located) exit entry */
            unsigned e2 = st_push(&s);
            s.bpos[e2] = c.pos; s.st[e2].vals = (uint16_t)cc->nend; s.st[e2].pop = 0;
            bbq_vec_push(cc->fixups, (int)e2);             /* then-arm end → block end */
            tc_reset_locals(&s, cc->init_height);          /* §3.4.2: the else arm doesn't see the then arm's sets */
            cc->else_e = -1; cc->opcode = 0x05; cc->unreachable = 0;
            tc_push_vals(&s, cc->start, cc->start_tidx, cc->nstart);  /* else body starts with the params */
            break;
        }
        /* throw $tag : [tag params] -> ⊥ (stack-polymorphic) */
        case 0x08: {
            uint32_t x = 0; bbq_read_uleb128_u32(&c, &x);
            if (!cx->tags || x >= cx->ntags) { s.err = JAV_E_UNKNOWN_TAG; s.ok = 0; break; }
            const jav_functype_t* tg = &cx->tags[x];
            tc_pop_vals(&s, tg->params, tg->param_tidx, tg->nparams);
            tc_unreachable(&s);
            break;
        }
        /* throw_ref : [(ref null exn)] -> ⊥ */
        case 0x0a: {
            tc_pop_ref_ht(&s, 1, HT_EXN);
            tc_unreachable(&s);
            break;
        }
        /* end */
        case 0x0b: {
            cframe_t f = *tc_top(&s);
            if (f.try_idx >= 0) s.trytab[f.try_idx].end_pc = (uint32_t)c.pos;  /* try_table exit point */
            tc_pop_vals(&s, f.end, f.end_tidx, f.nend);   /* the block's results must be on top */
            if (s.nvals != f.val_height) s.ok = 0;
            if (f.opcode == 0x04 && f.else_e >= 0) {      /* if with no else: params must equal results */
                if (f.nstart != f.nend) s.ok = 0;
                s.tip[f.else_e] = c.pos;
            }
            for (int i = 0; i < (int)bbq_vec_len(f.fixups); i++) s.tip[f.fixups[i]] = c.pos;
            bbq_vec_free(s.ctrls[s.nctrls - 1].fixups);   /* the frame is done — release its fixup list (f is a shallow copy) */
            tc_reset_locals(&s, f.init_height);           /* §3.4.2: locals set inside this block are unset on exit */
            s.nctrls--;
            tc_push_vals(&s, f.end, f.end_tidx, f.nend);  /* the block's results flow out */
            break;
        }
        /* br / br_if */
        case 0x0c: case 0x0d: {
            uint32_t label = 0; bbq_read_uleb128_u32(&c, &label);
            if (label >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
            cframe_t* tgt = &s.ctrls[s.nctrls - 1 - label];
            int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
            unsigned e = st_push(&s);
            if (op == 0x0d) tc_pop_e(&s, WVT_I32);        /* br_if pops the condition */
            s.bpos[e] = c.pos;
            int pop = s.nvals - tgt->val_height - nlab;
            s.st[e].vals = (uint16_t)nlab;
            s.st[e].pop  = (uint16_t)(pop > 0 ? pop : 0);
            if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start;       /* backward, known */
            else bbq_vec_push(tgt->fixups, (int)e);                    /* forward, backpatch at end */
            tc_pop_vals(&s, lab, lab_x, nlab);            /* the branch consumes the label types */
            if (op == 0x0d) tc_push_vals(&s, lab, lab_x, nlab);  /* br_if: fall-through restores them */
            else tc_unreachable(&s);                      /* br: rest is dead */
            break;
        }
        /* br_table */
        case 0x0e: {
            uint32_t count = 0; bbq_read_uleb128_u32(&c, &count);
            tc_pop_e(&s, WVT_I32);                         /* the table index */
            uint32_t* labels = malloc((count + 1) * sizeof *labels);
            for (uint32_t i = 0; i <= count; i++) bbq_read_uleb128_u32(&c, &labels[i]);
            size_t post_vec = c.pos;
            int dn = -1; const jav_valtype_t* dlab = NULL; const uint32_t* dlab_x = NULL;   /* default arity */
            for (uint32_t i = 0; i <= count && s.ok; i++) {
                if (labels[i] >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
                cframe_t* tgt = &s.ctrls[s.nctrls - 1 - labels[i]];
                int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
                if (dn < 0) { dn = nlab; dlab = lab; dlab_x = lab_x; }
                else if (nlab != dn) { s.ok = 0; break; }   /* all labels must share arity (§3.4.2) */
                for (int j = 0; j < nlab; j++) {            /* the operands t* must be ≤ EVERY target's label types */
                    int idx = (int)s.nvals - nlab + j;
                    vtype_t a = (idx >= tc_top(&s)->val_height) ? s.vals[idx] : VT_BOT;
                    if (!tc_matches(&s, a, vt_from(lab[j], lab_x ? lab_x[j] : 0))) { s.ok = 0; break; }
                }
                if (!s.ok) break;
                unsigned e = st_push(&s);
                s.bpos[e] = post_vec;
                int pop = s.nvals - tgt->val_height - nlab;
                s.st[e].vals = (uint16_t)nlab;
                s.st[e].pop  = (uint16_t)(pop > 0 ? pop : 0);
                if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start;
                else bbq_vec_push(tgt->fixups, (int)e);
            }
            free(labels);
            if (s.ok && dn >= 0) tc_pop_vals(&s, dlab, dlab_x, dn);   /* consume the shared label types */
            tc_unreachable(&s);                                /* rest is dead */
            break;
        }
        /* return */
        case 0x0f: {
            tc_pop_vals(&s, cx->results, cx->result_tidx, (int)cx->nresults);
            tc_unreachable(&s);
            break;
        }
        /* call */
        case 0x10: {
            uint32_t fidx = 0; bbq_read_uleb128_u32(&c, &fidx);
            if (!cx->func_sigs || fidx >= cx->nfuncs) { s.err = JAV_E_UNKNOWN_FUNCTION; s.ok = 0; break; }
            const jav_functype_t* ft = &cx->func_sigs[fidx];
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults);
            break;
        }
        /* call_indirect type table (§3.4.8): the table's rt must be <: funcref; t1* at -> t2* */
        case 0x11: {
            uint32_t y = 0, x = 0;
            bbq_read_uleb128_u32(&c, &y); bbq_read_uleb128_u32(&c, &x);   /* binary: type, table */
            if (x >= cx->ntables || !cx->types || y >= cx->ntypes) { s.err = (x >= cx->ntables) ? JAV_E_UNKNOWN_TABLE : JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
            if (!tc_matches(&s, tbl_rt(cx, x), VT_REF(1, HT_FUNC))) { s.ok = 0; break; }
            const jav_functype_t* ft = &cx->types[y];
            tc_pop_e(&s, tbl_at(cx, x));                  /* the index (the table's addrtype) */
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);     /* then the callee's params */
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults);
            break;
        }
        /* return_call x — typed as `call x; return` */
        case 0x12: {
            uint32_t fidx = 0; bbq_read_uleb128_u32(&c, &fidx);
            if (!cx->func_sigs || fidx >= cx->nfuncs) { s.err = JAV_E_UNKNOWN_FUNCTION; s.ok = 0; break; }
            const jav_functype_t* ft = &cx->func_sigs[fidx];
            if (ft->nresults != cx->nresults) { s.ok = 0; break; }        /* §3.4.8: callee results = C.return */
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);      /* pop the callee's params */
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults);  /* callee results <:           */
            tc_pop_vals(&s, cx->results, cx->result_tidx, (int)cx->nresults); /* ...this function's results */
            tc_unreachable(&s);                                            /* rest is dead */
            break;
        }
        /* return_call_indirect type table — call_indirect's rt<:funcref + the return-arity rule */
        case 0x13: {
            uint32_t y = 0, x = 0;
            bbq_read_uleb128_u32(&c, &y); bbq_read_uleb128_u32(&c, &x);
            if (x >= cx->ntables || !cx->types || y >= cx->ntypes) { s.err = (x >= cx->ntables) ? JAV_E_UNKNOWN_TABLE : JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
            if (!tc_matches(&s, tbl_rt(cx, x), VT_REF(1, HT_FUNC))) { s.ok = 0; break; }
            const jav_functype_t* ft = &cx->types[y];
            if (ft->nresults != cx->nresults) { s.ok = 0; break; }
            tc_pop_e(&s, tbl_at(cx, x));                                  /* the index (the table's addrtype) */
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults);
            tc_pop_vals(&s, cx->results, cx->result_tidx, (int)cx->nresults);
            tc_unreachable(&s);
            break;
        }
        /* call_ref type (§3.4): t1* (ref null x) -> t2* */
        case 0x14: {
            uint32_t y = 0; bbq_read_uleb128_u32(&c, &y);
            if (!cx->types || y >= cx->ntypes) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
            const jav_functype_t* ft = &cx->types[y];
            tc_pop_ref_ht(&s, 1, (int32_t)y);                             /* pop the (ref null $type) callee (top) */
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);     /* then the params below it */
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults); /* push the results */
            break;
        }
        /* return_call_ref type */
        case 0x15: {
            uint32_t y = 0; bbq_read_uleb128_u32(&c, &y);
            if (!cx->types || y >= cx->ntypes) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
            const jav_functype_t* ft = &cx->types[y];
            if (ft->nresults != cx->nresults) { s.ok = 0; break; }
            tc_pop_ref_ht(&s, 1, (int32_t)y);                             /* pop the (ref null $type) callee */
            tc_pop_vals(&s, ft->params, ft->param_tidx, ft->nparams);
            tc_push_vals(&s, ft->results, ft->result_tidx, ft->nresults);
            tc_pop_vals(&s, cx->results, cx->result_tidx, (int)cx->nresults);
            tc_unreachable(&s);
            break;
        }
        /* drop — value-polymorphic pop */
        case 0x1a: {
            tc_pop(&s);
            break;
        }
        /* select (untyped, §3.4.1): t t i32 -> t */
        case 0x1b: {
            tc_pop_e(&s, WVT_I32);                         /* the i32 condition (top) */
            vtype_t a = tc_pop(&s);                        /* operand 2 */
            vtype_t b = tc_pop(&s);                        /* operand 1 */
            /* untyped select forbids reference operands (those require `select t`); the two
             * operands must share ONE number/vector type. Bot = dead-code wildcard. */
            if (vt_is_ref(a) || vt_is_ref(b)) { s.ok = 0; break; }
            if (!vt_is_bot(a) && !vt_is_bot(b) && a.num != b.num) { s.ok = 0; break; }
            tc_push_vt(&s, vt_is_bot(a) ? b : a);          /* result = the common (known) type */
            break;
        }
        /* select t (typed, §3.4.1): t t i32 -> t */
        case 0x1c: {
            uint32_t ntypes = 0; bbq_read_uleb128_u32(&c, &ntypes);
            if (ntypes != 1) { s.err = JAV_E_INVALID_RESULT_ARITY; s.ok = 0; break; }          /* WASM 3.0: exactly one result type */
            int32_t enc = 0; if (!bbq_read_sleb128_s33(&c, &enc)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }   /* §5.3.3 s33 blocktype */
            vtype_t t;
            if (enc == -28 || enc == -29) {                /* 0x64 (ref ht) / 0x63 (ref null ht) */
                int32_t ht = 0; if (!bbq_read_sleb128_s33(&c, &ht)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }   /* §5.3.3 s33 */
                if (ht < 0) { if (ht < HT_EXN || ht > HT_NOEXN) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; } }  /* abstract heaptype */
                else if (!s.lat || (uint32_t)ht >= s.lat->ntypes) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }  /* concrete typeidx */
                t = VT_REF(enc == -29 ? 1 : 0, ht);
            } else {                                       /* numtype / vectype / funcref / externref shorthand */
                int32_t ht = 0;
                jav_valtype_t w = tc_vt_from_sleb(enc, &ht);
                if (w == WVT_BOT) { s.ok = 0; break; }
                t = vt_from(w, (uint32_t)ht);
            }
            tc_pop_e(&s, WVT_I32);                          /* the i32 condition (top) */
            vtype_t a = tc_pop(&s); if (!tc_matches(&s, a, t)) { s.ok = 0; break; }  /* operands <: t */
            vtype_t b = tc_pop(&s); if (!tc_matches(&s, b, t)) { s.ok = 0; break; }
            tc_push_vt(&s, t);                             /* typed select pushes exactly t */
            break;
        }
        /* try_table bt catch* instr* */
        case 0x1f: {
            if (s.nctrls >= MAX_DEPTH) { s.ok = 0; break; }
            const jav_valtype_t *st_t, *en_t; const uint32_t *st_x, *en_x; int nst_t, nen_t;
            cframe_t scratch;
            tc_read_blocktype(&c, cx, &scratch, &st_t, &st_x, &nst_t, &en_t, &en_x, &nen_t, &s.ok, &s.err);
            tc_pop_vals(&s, st_t, st_x, nst_t);          /* consume the try's params */
            int try_base = s.nvals;                       /* operand height at the try base = the catch restore point */
            uint32_t ncatch = 0; bbq_read_uleb128_u32(&c, &ncatch);
            unsigned catch_stp = (unsigned)bbq_vec_len(s.st);
            for (uint32_t ci = 0; ci < ncatch && s.ok; ci++) {  /* one side-table entry per catch */
                uint8_t ck = 0; bbq_read_u8(&c, &ck);     /* 0 catch, 1 catch_ref, 2 catch_all, 3 catch_all_ref */
                uint32_t tagx = 0;
                if (ck == 0 || ck == 1) bbq_read_uleb128_u32(&c, &tagx);
                uint32_t label = 0; bbq_read_uleb128_u32(&c, &label);
                if (ck > 3 || label >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
                cframe_t* tgt = &s.ctrls[s.nctrls - 1 - label];
                int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
                const jav_functype_t* tg = NULL;
                if (ck == 0 || ck == 1) {                 /* catch / catch_ref carry the tag's params */
                    if (!cx->tags || tagx >= cx->ntags) { s.err = JAV_E_UNKNOWN_TAG; s.ok = 0; break; }
                    tg = &cx->tags[tagx];
                }
                int ntagp = tg ? tg->nparams : 0;
                int has_ref = (ck == 1 || ck == 3);       /* the _ref forms also carry the exnref */
                if (ntagp + (has_ref ? 1 : 0) != nlab) { s.ok = 0; break; }   /* arity must match the label */
                for (int i = 0; i < ntagp; i++)           /* tag params <: label types */
                    if (!tc_matches(&s, vt_from(tg->params[i], tg->param_tidx ? tg->param_tidx[i] : 0),
                                        vt_from(lab[i], lab_x ? lab_x[i] : 0))) { s.ok = 0; break; }
                if (has_ref && s.ok &&                     /* the trailing (ref exn) <: the label's last type */
                    !tc_matches(&s, VT_REF(0, HT_EXN), vt_from(lab[nlab-1], lab_x ? lab_x[nlab-1] : 0))) s.ok = 0;
                unsigned e = st_push(&s);
                int pop = try_base - tgt->val_height;     /* unwind from the try base to the target */
                s.st[e].vals = (uint16_t)nlab; s.st[e].pop = (uint16_t)(pop > 0 ? pop : 0);
                if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start; else bbq_vec_push(tgt->fixups, (int)e);
            }
            if (!s.ok) break;
            size_t try_pc = c.pos;                        /* body start = handler install point; catch entries branch from here */
            for (unsigned e = catch_stp; e < (unsigned)bbq_vec_len(s.st); e++) s.bpos[e] = try_pc;
            jav_try_t tt = (jav_try_t){ (uint32_t)try_pc, catch_stp, ncatch, 0 };
            bbq_vec_push(s.trytab, tt);
            unsigned ti = (unsigned)bbq_vec_len(s.trytab) - 1;
            cframe_t* f = tc_push_ctrl(&s, 0x1f, st_t, st_x, nst_t, en_t, en_x, nen_t);
            if (en_t == scratch.single) { f->single[0] = scratch.single[0]; f->end = f->single;
                f->single_tidx[0] = scratch.single_tidx[0]; f->end_tidx = f->single_tidx; }
            f->try_idx = (int)ti;
            break;
        }
        /* local.get/set/tee */
        case 0x20: case 0x21: case 0x22: {
            uint32_t idx = 0; bbq_read_uleb128_u32(&c, &idx);
            if (idx >= cx->nlocals) { s.err = JAV_E_UNKNOWN_LOCAL; s.ok = 0; break; }
            jav_valtype_t lt = cx->locals[idx];
            uint32_t lti = cx->local_tidx ? cx->local_tidx[idx] : 0;   /* concrete ref local keeps its typeidx */
            if (op == 0x20) {                             /* get: §3.4.2 the local must be initialized */
                if (!s.locals_init[idx]) { s.err = JAV_E_UNINITIALIZED_LOCAL; s.ok = 0; break; }
                tc_push1(&s, lt, lti);
            } else {                                      /* set / tee: marks the local initialized */
                tc_pop1(&s, lt, lti);
                if (!s.locals_init[idx]) { s.locals_init[idx] = 1; bbq_vec_push(s.inits, idx); }
                if (op == 0x22) tc_push1(&s, lt, lti);
            }
            break;
        }
        /* global.get/set */
        case 0x23: case 0x24: {
            uint32_t idx = 0; bbq_read_uleb128_u32(&c, &idx);
            if (idx >= cx->nglobals) { s.err = JAV_E_UNKNOWN_GLOBAL; s.ok = 0; break; }
            jav_valtype_t gt = cx->globals[idx];
            uint32_t gtx = cx->global_tidx ? cx->global_tidx[idx] : 0;   /* concrete ref global keeps its typeidx */
            if (op == 0x23) tc_push1(&s, gt, gtx);
            else {                                                       /* global.set: §3.4.3 requires a MUTABLE global */
                if (!cx->global_mut || !cx->global_mut[idx]) { s.err = JAV_E_IMMUTABLE_GLOBAL; s.ok = 0; break; }
                tc_pop1(&s, gt, gtx);
            }
            break;
        }
        /* table.get x (§3.4.4): at -> rt */
        case 0x25: {
            uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
            if (t >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
            tc_pop_e(&s, tbl_at(cx, t));                   /* the index (the table's addrtype) */
            tc_push_vt(&s, tbl_rt(cx, t));                 /* push the table's element reftype */
            break;
        }
        /* table.set x (§3.4.4): at rt -> ε */
        case 0x26: {
            uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
            if (t >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
            tc_pop_vt(&s, tbl_rt(cx, t));                  /* value (top) must be <: the table's reftype */
            tc_pop_e(&s, tbl_at(cx, t));                   /* then the index (the table's addrtype) */
            break;
        }
        /* ref.null ht */
        case 0xd0: {
            int32_t ht = 0; if (!bbq_read_sleb128_s33(&c, &ht)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }   /* §5.3.3 s33 */
            if (ht < 0) { if (ht < HT_EXN || ht > HT_NOEXN) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; } }  /* abstract heaptype */
            else if (!s.lat || (uint32_t)ht >= s.lat->ntypes) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }  /* concrete typeidx */
            tc_push_ref(&s, 1, ht);                       /* (ref null ht) */
            break;
        }
        /* ref.is_null */
        case 0xd1: {
            tc_pop_ref(&s); tc_push(&s, WVT_I32);
            break;
        }
        /* ref.func x : -> (ref $type-of-func-x) */
        case 0xd2: {
            uint32_t fidx = 0; bbq_read_uleb128_u32(&c, &fidx);
            if (fidx >= cx->nfuncs ) { s.err = JAV_E_UNKNOWN_FUNCTION; s.ok = 0; break; }
            if (cx->func_ref_declared && !cx->func_ref_declared[fidx]) { s.err = JAV_E_UNDECLARED_FUNCTION_REFERENCE; s.ok = 0; break; }   /* §3.4.6: x ∈ C.refs */
            tc_push_ref(&s, 0, cx->func_type_idx ? (int32_t)cx->func_type_idx[fidx] : HT_FUNC);
            break;
        }
        /* ref.eq : eqref eqref -> i32 */
        case 0xd3: {
            tc_pop_ref_ht(&s, 1, HT_EQ); tc_pop_ref_ht(&s, 1, HT_EQ); tc_push(&s, WVT_I32);
            break;
        }
        /* ref.as_non_null */
        case 0xd4: {
            vtype_t a = tc_pop_ref(&s);
            tc_push_ref(&s, 0, vt_is_bot(a) ? HT_BOT : a.ht);   /* refine to non-null; §7.6 pop_ref yields (ref bot) when polymorphic */
            break;
        }
        /* br_on_null label */
        case 0xd5: {
            uint32_t label = 0; bbq_read_uleb128_u32(&c, &label);
            if (label >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
            cframe_t* tgt = &s.ctrls[s.nctrls - 1 - label];
            int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
            vtype_t a = tc_pop_ref(&s);                   /* pop the (ref null ht); branch drops it */
            int32_t ht = vt_is_bot(a) ? HT_BOT : a.ht;    /* §7.6 pop_ref yields bot heaptype when polymorphic */
            unsigned e = st_push(&s); s.bpos[e] = c.pos;
            int pop = s.nvals - tgt->val_height - nlab;
            s.st[e].vals = (uint16_t)nlab; s.st[e].pop = (uint16_t)(pop > 0 ? pop : 0);
            if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start; else bbq_vec_push(tgt->fixups, (int)e);
            tc_pop_vals(&s, lab, lab_x, nlab);            /* the label types t* (branch keeps these) */
            tc_push_vals(&s, lab, lab_x, nlab);           /* fall-through keeps t* ... */
            tc_push_ref(&s, 0, ht);                       /* ... plus the now-non-null ref (same heaptype) */
            break;
        }
        /* br_on_non_null label (§3.4): C.labels[l] = t* (ref ht); on branch pass the non-null ref + t*. */
        case 0xd6: {
            uint32_t label = 0; bbq_read_uleb128_u32(&c, &label);
            if (label >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
            cframe_t* tgt = &s.ctrls[s.nctrls - 1 - label];
            int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
            /* §3.4: C.labels[l] = t* (ref null? ht); instr type t* (ref null ht) -> t*. The
             * fall-through OUTPUT is the label's leading t* (not the actual operands), so a
             * later use sees the label type — pop the operand + leading t*, push back t*. */
            if (nlab < 1) { s.ok = 0; break; }
            vtype_t llt = vt_from(lab[nlab-1], lab_x ? lab_x[nlab-1] : 0);
            if (!vt_is_ref(llt)) { s.ok = 0; break; }
            unsigned e = st_push(&s); s.bpos[e] = c.pos;
            int pop = s.nvals - tgt->val_height - nlab;
            s.st[e].vals = (uint16_t)nlab; s.st[e].pop = (uint16_t)(pop > 0 ? pop : 0);
            if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start; else bbq_vec_push(tgt->fixups, (int)e);
            tc_pop_ref_ht(&s, 1, llt.ht);          /* the operand ≤ (ref null ht), ht from the label's trailing ref */
            tc_pop_vals(&s, lab, lab_x, nlab - 1); /* the leading t* (actual ≤ label) */
            tc_push_vals(&s, lab, lab_x, nlab - 1);/* fall-through re-types them to the label's t* */
            break;
        }
        /* ordinary opcode: opsig transfer fn */
        default: {
            jav_opsig_t sig = {0}; jav_jit_meta_t m = {0};
            int handled = 0;
            if (jav_opsig_sub[op]) {                     /* prefixed (0xFC misc, 0xFB GC, …) */
                uint32_t sub = 0; bbq_read_uleb128_u32(&c, &sub);
                /* GC aggregate ops: concrete (ref $t) operands + dynamic arity. Refs pop
                 * as (ref null $t) [struct.get/array.get trap on null at runtime]; ops that
                 * produce a fresh aggregate push (ref $t) non-null. Field/element types flow
                 * through vt_from (tc_pop1/tc_push1) — number or ref, uniformly. */
                if (op == 0xfb && (sub == 0 || sub == 2 || sub == 5)) {        /* struct.new / get / set */
                    uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                    if (!cx->structtypes || t >= cx->nstructtypes || (s.lat && s.lat->kinds[t] != WST_STRUCT)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                    const jav_structtype_t* st_ = &cx->structtypes[t];
                    const uint8_t* pk = (cx->type_field_packs && t < cx->num_type_field_packs) ? cx->type_field_packs[t] : NULL;
                    if (sub == 0) {                       /* struct.new $t : [unpack(field)*] -> (ref $t) */
                        for (int fi = (int)st_->nfields - 1; fi >= 0; fi--)
                            tc_pop1(&s, st_->fields[fi], st_->field_tidx ? st_->field_tidx[fi] : 0);
                        tc_push_ref(&s, 0, (int32_t)t);
                    } else {
                        uint32_t fidx = 0; bbq_read_uleb128_u32(&c, &fidx);
                        if (fidx >= st_->nfields) { s.ok = 0; break; }
                        jav_valtype_t ft = st_->fields[fidx];
                        uint32_t ftx = st_->field_tidx ? st_->field_tidx[fidx] : 0;
                        if (sub == 2) {                   /* struct.get : INVALID on a packed field (§3.4.7) */
                            if (pk && pk[fidx]) { s.ok = 0; break; }
                            tc_pop_ref_ht(&s, 1, (int32_t)t); tc_push1(&s, ft, ftx);
                        } else {                          /* struct.set : field must be mutable */
                            if (!st_->field_mut || !st_->field_mut[fidx]) { s.err = JAV_E_IMMUTABLE_FIELD; s.ok = 0; break; }
                            tc_pop1(&s, ft, ftx); tc_pop_ref_ht(&s, 1, (int32_t)t);
                        }
                    }
                    handled = 1;
                } else if (op == 0xfb && (sub == 6 || sub == 11 || sub == 14 || sub == 15)) {
                    if (sub == 15) { tc_pop_ref_ht(&s, 1, HT_ARRAY); tc_push(&s, WVT_I32); handled = 1; }  /* array.len */
                    else {
                        uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                        if (!cx->arraytypes || t >= cx->narraytypes || (s.lat && s.lat->kinds[t] != WST_ARRAY)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                        jav_valtype_t et = cx->arraytypes[t].elem; uint32_t etx = cx->arraytypes[t].elem_tidx;
                        uint8_t epk = (cx->type_field_packs && t < cx->num_type_field_packs && cx->type_field_packs[t]) ? cx->type_field_packs[t][0] : 0;
                        if (sub == 6) {                   /* array.new $t : [unpack(elem), i32] -> (ref $t) */
                            tc_pop_e(&s, WVT_I32); tc_pop1(&s, et, etx); tc_push_ref(&s, 0, (int32_t)t);
                        } else if (sub == 11) {           /* array.get $t : INVALID on a packed element (§3.4.7) */
                            if (epk) { s.ok = 0; break; }
                            tc_pop_e(&s, WVT_I32); tc_pop_ref_ht(&s, 1, (int32_t)t); tc_push1(&s, et, etx);
                        } else {                          /* array.set $t : element must be mutable */
                            if (!cx->arraytypes[t].elem_mut) { s.err = JAV_E_IMMUTABLE_ARRAY; s.ok = 0; break; }
                            tc_pop1(&s, et, etx); tc_pop_e(&s, WVT_I32); tc_pop_ref_ht(&s, 1, (int32_t)t);
                        }
                        handled = 1;
                    }
                } else if (op == 0xfb && sub == 1) {      /* struct.new_default $t -> (ref $t): fields must be defaultable */
                    uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                    if (!cx->structtypes || t >= cx->nstructtypes || (s.lat && s.lat->kinds[t] != WST_STRUCT)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                    const jav_structtype_t* st_ = &cx->structtypes[t];
                    int bad = 0;
                    for (unsigned fi = 0; fi < st_->nfields; fi++) if (!vt_defaultable(st_->fields[fi])) bad = 1;
                    if (bad) { s.ok = 0; break; }
                    tc_push_ref(&s, 0, (int32_t)t);
                    handled = 1;
                } else if (op == 0xfb && (sub == 3 || sub == 4)) {   /* struct.get_s/u $t $f : (ref null $t) -> i32 */
                    uint32_t t = 0, fi = 0;
                    bbq_read_uleb128_u32(&c, &t); bbq_read_uleb128_u32(&c, &fi);
                    if (!cx->structtypes || t >= cx->nstructtypes || (s.lat && s.lat->kinds[t] != WST_STRUCT)
                        || fi >= cx->structtypes[t].nfields) { s.ok = 0; break; }
                    const uint8_t* pk = (cx->type_field_packs && t < cx->num_type_field_packs) ? cx->type_field_packs[t] : NULL;
                    if (!pk || (pk[fi] != 1 && pk[fi] != 2)) { s.ok = 0; break; }   /* get_s/u requires a packed field */
                    tc_pop_ref_ht(&s, 1, (int32_t)t); tc_push(&s, WVT_I32);
                    handled = 1;
                } else if (op == 0xfb && (sub == 7 || sub == 8 || sub == 9 || sub == 10)) {
                    uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                    if (!cx->arraytypes || t >= cx->narraytypes || (s.lat && s.lat->kinds[t] != WST_ARRAY)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                    jav_valtype_t et = cx->arraytypes[t].elem; uint32_t etx = cx->arraytypes[t].elem_tidx;
                    if (sub == 7) {                                      /* array.new_default : [i32] -> (ref $t), elem defaultable */
                        if (!vt_defaultable(et)) { s.ok = 0; break; }
                        tc_pop_e(&s, WVT_I32);
                    } else if (sub == 8) {                               /* array.new_fixed n : [unpack(elem)^n] -> (ref $t) */
                        uint32_t n = 0; bbq_read_uleb128_u32(&c, &n);
                        for (uint32_t i = 0; i < n; i++) tc_pop1(&s, et, etx);
                    } else if (sub == 9) {                               /* array.new_data seg : element must be num/vec */
                        uint32_t seg = 0; bbq_read_uleb128_u32(&c, &seg);
                        if (seg >= cx->ndatas || !vt_is_numvec(et)) { s.err = (seg >= cx->ndatas) ? data_err(cx) : JAV_E_ARRAY_NOT_NUMERIC; s.ok = 0; break; }
                        tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32);
                    } else {                                             /* array.new_elem seg : element a ref ⊒ the segment's reftype */
                        uint32_t seg = 0; bbq_read_uleb128_u32(&c, &seg);
                        if (seg >= cx->nelems || (et != WVT_REF && et != WVT_REF_NN)) { s.err = (seg >= cx->nelems) ? JAV_E_UNKNOWN_ELEM : JAV_E_TYPE_MISMATCH; s.ok = 0; break; }
                        jav_valtype_t sw = cx->elem_reftype ? cx->elem_reftype[seg] : WVT_REF;
                        uint32_t sx = cx->elem_tidx ? cx->elem_tidx[seg] : (uint32_t)HT_FUNC;
                        if (!vt_sub(s.lat, vt_from(sw, sx), vt_from(et, etx))) { s.ok = 0; break; }
                        tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32);
                    }
                    tc_push_ref(&s, 0, (int32_t)t);
                    handled = 1;
                } else if (op == 0xfb && (sub == 12 || sub == 13)) {  /* array.get_s/u $t : (ref null $t) i32 -> i32 */
                    uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                    if (!cx->arraytypes || t >= cx->narraytypes || (s.lat && s.lat->kinds[t] != WST_ARRAY)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                    uint8_t epk = (cx->type_field_packs && t < cx->num_type_field_packs && cx->type_field_packs[t]) ? cx->type_field_packs[t][0] : 0;
                    if (epk != 1 && epk != 2) { s.ok = 0; break; }       /* get_s/u requires a packed element */
                    tc_pop_e(&s, WVT_I32); tc_pop_ref_ht(&s, 1, (int32_t)t); tc_push(&s, WVT_I32);
                    handled = 1;
                } else if (op == 0xfb && sub == 16) {        /* array.fill $t : (ref null $t) i32 elem i32 -> [], element mutable */
                    uint32_t t = 0; bbq_read_uleb128_u32(&c, &t);
                    if (!cx->arraytypes || t >= cx->narraytypes || (s.lat && s.lat->kinds[t] != WST_ARRAY)
                        || !cx->arraytypes[t].elem_mut) {
                        s.err = (cx->arraytypes && t < cx->narraytypes && (!s.lat || s.lat->kinds[t] == WST_ARRAY))
                                ? JAV_E_IMMUTABLE_ARRAY : JAV_E_UNKNOWN_TYPE;
                        s.ok = 0; break;
                    }
                    jav_valtype_t et = cx->arraytypes[t].elem; uint32_t etx = cx->arraytypes[t].elem_tidx;
                    tc_pop_e(&s, WVT_I32); tc_pop1(&s, et, etx); tc_pop_e(&s, WVT_I32); tc_pop_ref_ht(&s, 1, (int32_t)t);
                    handled = 1;
                } else if (op == 0xfb && sub == 17) {        /* array.copy $d $s : (ref null $d) i32 (ref null $s) i32 i32 -> [] */
                    uint32_t dt = 0, srct = 0;
                    bbq_read_uleb128_u32(&c, &dt); bbq_read_uleb128_u32(&c, &srct);
                    if (!cx->arraytypes || dt >= cx->narraytypes || srct >= cx->narraytypes
                        || (s.lat && (s.lat->kinds[dt] != WST_ARRAY || s.lat->kinds[srct] != WST_ARRAY))
                        || !cx->arraytypes[dt].elem_mut) {                      /* dest mutable; src zt ≤ dst zt */
                        s.err = (cx->arraytypes && dt < cx->narraytypes && srct < cx->narraytypes
                                 && (!s.lat || (s.lat->kinds[dt] == WST_ARRAY && s.lat->kinds[srct] == WST_ARRAY)))
                                ? JAV_E_IMMUTABLE_ARRAY : JAV_E_UNKNOWN_TYPE;
                        s.ok = 0; break;
                    }
                    uint8_t dpk = (cx->type_field_packs && cx->type_field_packs[dt])   ? cx->type_field_packs[dt][0]   : 0;
                    uint8_t spk = (cx->type_field_packs && cx->type_field_packs[srct]) ? cx->type_field_packs[srct][0] : 0;
                    if (!storage_sub(s.lat, cx->arraytypes[srct].elem, cx->arraytypes[srct].elem_tidx, spk,
                                            cx->arraytypes[dt].elem,   cx->arraytypes[dt].elem_tidx,   dpk)) { s.err = JAV_E_ARRAY_TYPES_MISMATCH; s.ok = 0; break; }
                    tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32);
                    tc_pop_ref_ht(&s, 1, (int32_t)srct); tc_pop_e(&s, WVT_I32);
                    tc_pop_ref_ht(&s, 1, (int32_t)dt);
                    handled = 1;
                } else if (op == 0xfb && (sub == 18 || sub == 19)) {  /* array.init_data/elem $t seg : (ref null $t) i32 i32 i32 -> [] */
                    uint32_t t = 0, seg = 0;
                    bbq_read_uleb128_u32(&c, &t); bbq_read_uleb128_u32(&c, &seg);
                    if (!cx->arraytypes || t >= cx->narraytypes || (s.lat && s.lat->kinds[t] != WST_ARRAY)
                        || !cx->arraytypes[t].elem_mut) {                       /* element must be mutable */
                        s.err = (cx->arraytypes && t < cx->narraytypes && (!s.lat || s.lat->kinds[t] == WST_ARRAY))
                                ? JAV_E_IMMUTABLE_ARRAY : JAV_E_UNKNOWN_TYPE;
                        s.ok = 0; break;
                    }
                    jav_valtype_t et = cx->arraytypes[t].elem; uint32_t etx = cx->arraytypes[t].elem_tidx;
                    if (sub == 18) {                                     /* init_data : element num/vec, segment in range */
                        if (seg >= cx->ndatas || !vt_is_numvec(et)) { s.err = (seg >= cx->ndatas) ? data_err(cx) : JAV_E_ARRAY_NOT_NUMERIC; s.ok = 0; break; }
                    } else {                                             /* init_elem : element a ref ⊒ the segment's reftype */
                        if (seg >= cx->nelems || (et != WVT_REF && et != WVT_REF_NN)) { s.err = (seg >= cx->nelems) ? JAV_E_UNKNOWN_ELEM : JAV_E_TYPE_MISMATCH; s.ok = 0; break; }
                        jav_valtype_t sw = cx->elem_reftype ? cx->elem_reftype[seg] : WVT_REF;
                        uint32_t sx = cx->elem_tidx ? cx->elem_tidx[seg] : (uint32_t)HT_FUNC;
                        if (!vt_sub(s.lat, vt_from(sw, sx), vt_from(et, etx))) { s.ok = 0; break; }
                    }
                    tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32);
                    tc_pop_ref_ht(&s, 1, (int32_t)t);
                    handled = 1;
                } else if (op == 0xfc && tc_mem(&s, cx, op, sub, &c)) {  /* memory.init / .copy / .fill (memidx-bounded, §3.4.5) */
                    handled = 1;
                } else if (op == 0xfc && sub == 9) {      /* data.drop seg : [] -> [] */
                    uint32_t y = 0; bbq_read_uleb128_u32(&c, &y);
                    if (y >= cx->ndatas) { s.err = data_err(cx); s.ok = 0; break; }
                    handled = 1;
                } else if (op == 0xfc && sub == 12) {     /* table.init x y (§3.4.4): at i32 i32 -> ε, elem rt2 <: rt1 */
                    uint32_t y = 0, x = 0; bbq_read_uleb128_u32(&c, &y); bbq_read_uleb128_u32(&c, &x);   /* elemseg, table */
                    if (x >= cx->ntables || y >= cx->nelems) { s.err = (x >= cx->ntables) ? JAV_E_UNKNOWN_TABLE : JAV_E_UNKNOWN_ELEM; s.ok = 0; break; }
                    if (!tc_matches(&s, elem_rt(cx, y), tbl_rt(cx, x))) { s.ok = 0; break; }
                    tc_pop_e(&s, WVT_I32); tc_pop_e(&s, WVT_I32); tc_pop_e(&s, tbl_at(cx, x));   /* n, src(elem off), dst */
                    handled = 1;
                } else if (op == 0xfc && sub == 13) {     /* elem.drop x : [] -> [] */
                    uint32_t y = 0; bbq_read_uleb128_u32(&c, &y);
                    if (y >= cx->nelems) { s.err = JAV_E_UNKNOWN_ELEM; s.ok = 0; break; }
                    handled = 1;
                } else if (op == 0xfc && sub == 14) {     /* table.copy x y (§3.4.4): at1 at2 min -> ε, rt2 <: rt1 */
                    uint32_t x = 0, y = 0; bbq_read_uleb128_u32(&c, &x); bbq_read_uleb128_u32(&c, &y);   /* dst, src */
                    if (x >= cx->ntables || y >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
                    if (!tc_matches(&s, tbl_rt(cx, y), tbl_rt(cx, x))) { s.ok = 0; break; }
                    jav_valtype_t at1 = tbl_at(cx, x), at2 = tbl_at(cx, y);
                    jav_valtype_t atmin = (at1 == WVT_I64 && at2 == WVT_I64) ? WVT_I64 : WVT_I32;
                    tc_pop_e(&s, atmin); tc_pop_e(&s, at2); tc_pop_e(&s, at1);   /* n, src, dst */
                    handled = 1;
                } else if (op == 0xfc && sub == 15) {     /* table.grow x : rt at -> at */
                    uint32_t x = 0; bbq_read_uleb128_u32(&c, &x);
                    if (x >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
                    tc_pop_e(&s, tbl_at(cx, x)); tc_pop_vt(&s, tbl_rt(cx, x)); tc_push(&s, tbl_at(cx, x));   /* n(top), init -> oldsize */
                    handled = 1;
                } else if (op == 0xfc && sub == 16) {     /* table.size x : ε -> at */
                    uint32_t x = 0; bbq_read_uleb128_u32(&c, &x);
                    if (x >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
                    tc_push(&s, tbl_at(cx, x));
                    handled = 1;
                } else if (op == 0xfc && sub == 17) {     /* table.fill x : at rt at -> ε */
                    uint32_t x = 0; bbq_read_uleb128_u32(&c, &x);
                    if (x >= cx->ntables) { s.err = JAV_E_UNKNOWN_TABLE; s.ok = 0; break; }
                    tc_pop_e(&s, tbl_at(cx, x)); tc_pop_vt(&s, tbl_rt(cx, x)); tc_pop_e(&s, tbl_at(cx, x));   /* n, val, i */
                    handled = 1;
                } else if (op == 0xfb && (sub == 26 || sub == 27)) {  /* any.convert_extern / extern.convert_any */
                    /* §3.4.9: any.convert_extern : (ref null? extern) -> (ref null? any); extern.convert_any
                     * the reverse. Operand must be in the source hierarchy; nullability preserved. */
                    int32_t want = (sub == 26) ? HT_EXTERN : HT_ANY;
                    vtype_t a = tc_pop_ref(&s);
                    if (!vt_is_bot(a) && !jav_ht_sub(s.lat, a.ht, want)) { s.ok = 0; break; }
                    tc_push_ref(&s, vt_is_bot(a) ? 0 : a.nullable, sub == 26 ? HT_ANY : HT_EXTERN);
                    handled = 1;
                } else if (op == 0xfb && sub == 28) {         /* ref.i31 : i32 -> (ref i31) NON-NULL (§3.4.8) */
                    tc_pop_e(&s, WVT_I32); tc_push_ref(&s, 0, HT_I31);
                    handled = 1;
                } else if (op == 0xfb && (sub == 20 || sub == 21 || sub == 22 || sub == 23)) {
                    /* ref.test rt / ref.cast rt (§3.4.6): operand rt' and the target ht must
                     * share a hierarchy (∃ common supertype rt'). test -> i32; cast -> (ref ht). */
                    int32_t ht = 0; if (!bbq_read_sleb128_s33(&c, &ht)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }   /* §5.3.3 s33 */
                    /* the heaptype immediate must be VALID unconditionally (even in dead code) — §3.4.6 rt:ok */
                    if (!(ht >= 0 ? (s.lat && (uint32_t)ht < s.lat->ntypes) : (ht >= HT_EXN && ht <= HT_NOEXN))) { s.ok = 0; break; }
                    vtype_t src = tc_pop_ref(&s);
                    if (!vt_is_bot(src) && !jav_ht_compatible(s.lat, ht, src.ht)) { s.ok = 0; break; }
                    int nn = (sub == 20 || sub == 22) ? 0 : 1;   /* non-null variant */
                    if (sub <= 21) tc_push(&s, WVT_I32);          /* ref.test */
                    else tc_push_ref(&s, nn, ht);                 /* ref.cast -> (ref null? ht) */
                    handled = 1;
                } else if (op == 0xfb && (sub == 24 || sub == 25)) {
                    /* br_on_cast l rt1 rt2 (§3.4): [t* rt1] -> [t* (rt1\rt2)]; branch carries [t* rt2].
                     * br_on_cast_fail: [t* rt1] -> [t* rt2]; branch carries [t* (rt1\rt2)]. Premises:
                     * rt1 ok, rt2 ok, rt2<:rt1, and the branch reftype <: the label's trailing reftype. */
                    uint8_t flags = 0; bbq_read_u8(&c, &flags);   /* bit0 = rt1 nullable, bit1 = rt2 nullable */
                    uint32_t label = 0; bbq_read_uleb128_u32(&c, &label);
                    int32_t ht1 = 0, ht2 = 0;   /* §5.3.3 s33 heaptypes — reject malformed/out-of-range, don't clamp */
                    if (!bbq_read_sleb128_s33(&c, &ht1) || !bbq_read_sleb128_s33(&c, &ht2)) { s.err = JAV_E_UNKNOWN_TYPE; s.ok = 0; break; }
                    if (label >= (uint32_t)s.nctrls) { s.err = JAV_E_UNKNOWN_LABEL; s.ok = 0; break; }
                    int rt1n = flags & 1, rt2n = (flags >> 1) & 1;
                    /* rt1 ok, rt2 ok */
                    if (!(ht1 >= 0 ? (s.lat && (uint32_t)ht1 < s.lat->ntypes) : (ht1 >= HT_EXN && ht1 <= HT_NOEXN)) ||
                        !(ht2 >= 0 ? (s.lat && (uint32_t)ht2 < s.lat->ntypes) : (ht2 >= HT_EXN && ht2 <= HT_NOEXN))) { s.ok = 0; break; }
                    if (!jav_rt_sub(s.lat, rt2n, ht2, rt1n, ht1)) { s.ok = 0; break; }   /* rt2 <: rt1 (with nullability) */
                    cframe_t* tgt = &s.ctrls[s.nctrls - 1 - label];
                    int nlab; const uint32_t* lab_x; const jav_valtype_t* lab = tc_label(tgt, &lab_x, &nlab);
                    if (nlab < 1 || !vt_is_ref(vt_from(lab[nlab-1], lab_x ? lab_x[nlab-1] : 0))) { s.ok = 0; break; }
                    int diffn = rt1n && !rt2n;                     /* rt1\rt2 nullability (§3.1.1): non-null when rt2 is nullable */
                    vtype_t branch_ref = (sub == 24) ? VT_REF(rt2n, ht2) : VT_REF(diffn, ht1);
                    vtype_t fall_ref   = (sub == 24) ? VT_REF(diffn, ht1) : VT_REF(rt2n, ht2);
                    if (!tc_matches(&s, branch_ref, vt_from(lab[nlab-1], lab_x ? lab_x[nlab-1] : 0))) { s.ok = 0; break; }
                    /* side-table FIRST (operand still on the stack): the branch transfers nlab values */
                    unsigned e = st_push(&s); s.bpos[e] = c.pos;
                    int pop = s.nvals - tgt->val_height - nlab;
                    s.st[e].vals = (uint16_t)nlab; s.st[e].pop = (uint16_t)(pop > 0 ? pop : 0);
                    if (tgt->opcode == 0x03) s.tip[e] = tgt->loop_start; else bbq_vec_push(tgt->fixups, (int)e);
                    tc_pop_ref_ht(&s, rt1n, ht1);                 /* operand <: rt1 */
                    tc_pop_vals(&s, lab, lab_x, nlab - 1);        /* the leading t* (actual ≤ label) */
                    tc_push_vals(&s, lab, lab_x, nlab - 1);       /* fall-through re-types them to the label's t* */
                    if (!s.ok) break;
                    tc_push_vt(&s, fall_ref);                     /* then the residual reftype on top */
                    handled = 1;
                } else if (op == 0xfd && sub >= 21 && sub <= 34) {   /* SIMD extract_lane / replace_lane */
                    /* §3.4.11: the laneidx immediate must satisfy `lane < dim(shape)`. dim by
                     * subop (21..34): i8x16=16, i16x8=8, i32x4=4, i64x2=2, f32x4=4, f64x2=2. */
                    static const uint8_t lane_dim[] = {16,16,16,8,8,8,4,4,2,2,4,4,2,2};
                    uint8_t lane = 0; bbq_read_u8(&c, &lane);
                    if (lane >= lane_dim[sub - 21]) { s.err = JAV_E_INVALID_LANE; s.ok = 0; break; }
                    const jav_opsig_t* sg = &jav_opsig_sub[op][sub];
                    tc_pop_vals(&s, sg->pops, NULL, sg->npop);
                    tc_push_vals(&s, sg->pushes, NULL, sg->npush);
                    handled = 1;
                } else if (op == 0xfd && sub == 13) {   /* i8x16.shuffle: 16 laneidx bytes, each < 32 */
                    int bad = 0;
                    for (int i = 0; i < 16; i++) { uint8_t li = 0; bbq_read_u8(&c, &li); if (li >= 32) bad = 1; }
                    if (bad) { s.err = JAV_E_INVALID_LANE; s.ok = 0; break; }
                    const jav_opsig_t* sg = &jav_opsig_sub[op][sub];
                    tc_pop_vals(&s, sg->pops, NULL, sg->npop);
                    tc_push_vals(&s, sg->pushes, NULL, sg->npush);
                    handled = 1;
                } else if (op == 0xfd && tc_mem(&s, cx, op, sub, &c)) {   /* SIMD memory ops: load/store, loadN_splat, load/store_lane */
                    handled = 1;
                } else { sig = jav_opsig_sub[op][sub]; m = jav_jit_meta_sub[op][sub]; }
            } else if (tc_mem(&s, cx, op, -1, &c)) {   /* scalar load/store, memory.size/grow (§3.4.5) */
                handled = 1;
            } else { sig = jav_opsig[op]; m = jav_jit_meta[op]; }
            if (!handled) {
                if (!sig.present) { s.ok = 0; break; }    /* unknown opcode → reject */
                tc_skip_operands(&c, m);
                /* a ref operand's abstract heaptype rides the parallel pop_ht/push_ht column */
                tc_pop_vals(&s, sig.pops, (const uint32_t*)sig.pop_ht, sig.npop);
                tc_push_vals(&s, sig.pushes, (const uint32_t*)sig.push_ht, sig.npush);
            }
            break;
        }
        }
        if (s.nvals > max_height) max_height = s.nvals;
    }

    if (s.nctrls != 0) s.ok = 0;            /* unbalanced control (missing end) */
    if (max_height > MAX_STACK) s.ok = 0;

    /* delta_ip / delta_stp post-pass. delta_stp needs, per entry, the count of
     * entries whose branch position precedes this entry's target. bpos[] is
     * append-ordered by the walk, hence sorted, so that count is a lower
     * bound — a binary search. The first spelling rescanned linearly from 0
     * for every entry (faithful to the legacy builder, quadratic with it):
     * one line, 34% of a corpus-wide profile. */
    unsigned nst = (unsigned)bbq_vec_len(s.st);
    for (unsigned e = 0; e < nst; e++) {
        s.st[e].delta_ip = (int32_t)((long)s.tip[e] - (long)s.bpos[e]);
        unsigned lo = 0, hi = nst;
        while (lo < hi) {
            unsigned mid = lo + (hi - lo) / 2;
            if (s.bpos[mid] < s.tip[e]) lo = mid + 1; else hi = mid;
        }
        s.st[e].delta_stp = (int32_t)lo - (int32_t)e + s.eskip[e];   /* +1 for an if-false → else target */
    }

    for (int i = 0; i < s.nctrls; i++) bbq_vec_free(s.ctrls[i].fixups);   /* frames still open on an early exit */
    free(s.vals); free(s.ctrls); free(s.locals_init); bbq_vec_free(s.inits);
    bbq_vec_free(s.bpos); bbq_vec_free(s.tip); bbq_vec_free(s.eskip);
    if (!s.ok) { if (out_err) *out_err = s.err; bbq_vec_free(s.st); bbq_vec_free(s.trytab); return 0; }
    if (out_err) *out_err = JAV_E_NONE;
    *out_st = s.st; *out_n = nst;
    if (out_try) { *out_try = s.trytab; *out_ntry = (unsigned)bbq_vec_len(s.trytab); }
    else bbq_vec_free(s.trytab);
    return 1;
}

/* The extended-const admissible set, named by opgen's generated OP_* constants
 * (the same by-opcode policy style as the structural cases above — the opcode
 * SEMANTICS stay in the generated interpreter; this is only the policy gate). */
static int const_admissible(uint8_t op) {
    switch (op) {
    case OP_I32_CONST: case OP_I64_CONST: case OP_F32_CONST: case OP_F64_CONST:
    case OP_GLOBAL_GET:
    case OP_REF_NULL: case OP_REF_FUNC:                /* §3.3.10 reference const-exprs */
    case OP_I32_ADD: case OP_I32_SUB: case OP_I32_MUL:
    case OP_I64_ADD: case OP_I64_SUB: case OP_I64_MUL:
        return 1;
    default: return 0;
    }
}

int jav_validate_const_expr(const uint8_t* code, size_t len) {
    bbq_ctx_t c; bbq_ctx_init(&c, code, len);
    int height = 0, ok = 1, ended = 0;
    for (;;) {
        uint8_t op;
        if (!bbq_read_u8(&c, &op)) break;          /* off the end with no `end` */
        if (op == 0x0b) { ended = 1; break; }      /* `end` terminates the expr */
        if (!const_admissible(op)) { ok = 0; break; }
        jav_jit_meta_t m = jav_jit_meta[op];     /* skip operands generically */
        for (int k = 0; k < m.operand_count; k++) {
            switch (m.operands[k].kind) {
            case JOP_BRTABLE_COUNT:
            case JOP_ULEB32: { uint32_t v; bbq_read_uleb128_u32(&c, &v); break; }
            case JOP_ULEB64: { uint64_t v; bbq_read_uleb128_u64(&c, &v); break; }
            case JOP_SLEB32: { int32_t  v; bbq_read_sleb128_i32(&c, &v); break; }
            case JOP_SLEB64: { int64_t  v; bbq_read_sleb128_i64(&c, &v); break; }
            case JOP_F32:    { float    v; bbq_read_f32le(&c, &v); break; }
            case JOP_F64:    { double   v; bbq_read_f64le(&c, &v); break; }
            case JOP_U8:     { uint8_t  v; bbq_read_u8(&c, &v); break; }
            case JOP_MEMARG: { uint32_t fl=0; bbq_read_uleb128_u32(&c,&fl);
                               if (fl & 0x40) { uint32_t mi=0; bbq_read_uleb128_u32(&c,&mi); }
                               uint64_t off=0; bbq_read_uleb128_u64(&c,&off); break; }
            case JOP_BLOCKTYPE: { int32_t v=0; bbq_read_sleb128_i32(&c,&v);
                                  if (v==-29||v==-28){ int32_t ht=0; bbq_read_sleb128_i32(&c,&ht); } break; }
            case JOP_CONST: case JOP_NONE: break;
            }
        }
        height += (int)m.push - (int)m.pop;
    }
    return ok && ended && height == 1;   /* well-formed, exactly one result */
}
