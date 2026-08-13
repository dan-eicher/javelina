// jav_module_validate.c — the §7 module validation gate over the flattened index.
// Each check returns a jav_err_t reason (JAV_E_NONE = valid); the reason is the official
// testsuite vocabulary, set once at the point of failure (no inline strings).
// Spec references are to the WebAssembly Core Specification, Release 3.0.
#include "jav_module_validate.h"
#include "jav_view_nav.h"   // jav_view_find_section
#include "jav_frame.h"      // jav_try_t (typecheck_ex output)
#include "bbq_read.h"       // bbq_ctx_t LEB decode (const-expr scan)
#include "bbq_htree.h"      // C.refs — the declared-funcref visited-set (crt, sparse)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Span-index navigation (jav_view_field / jav_view_nchild / jav_view_section_array) is
// shared from jav_view_nav.h.

// The module-level validation context (jav_module_cx) is shared from jav_module_index.h;
// each body fills in its per-function locals/results.

// ── §3.2.12/15/16 limits well-formed ──
static jav_err_t limits_range(uint64_t min, uint64_t max, int has_max, uint64_t k, jav_err_t over) {
    if (has_max && min > max) return JAV_E_SIZE_MIN_GT_MAX;
    if (min > k || (has_max && max > k)) return over;
    return JAV_E_NONE;
}
static jav_err_t limits_err(const jav_modidx_t* mod) {
    for (uint32_t i = 0; i < mod->ntables; i++) {        // §3.2.16: k = 2^|addrtype| − 1
        uint64_t k = mod->table_is64[i] ? UINT64_MAX : 0xFFFFFFFFu;
        jav_err_t e = limits_range(mod->table_min[i], mod->table_max[i], mod->table_has_max[i], k, JAV_E_TABLE_SIZE);
        if (e) return e;
    }
    for (uint32_t i = 0; i < mod->nmems; i++) {          // §3.2.15: k = 2^(|addrtype|−16)
        uint64_t k = mod->mem_is64[i] ? (UINT64_C(1) << 48) : 65536u;
        jav_err_t e = limits_range(mod->mem_min[i], mod->mem_max[i], mod->mem_has_max[i], k, JAV_E_MEMORY_SIZE);
        if (e) return e;
    }
    return JAV_E_NONE;
}

// A §5.5.5 name is a byte SPAN, not a C string — it may contain a NUL and is never
// terminated by one, so the length is part of the key. djb2, as the compiler's symbol
// tables use; the hash only chooses a bucket, so its quality costs comparisons and never
// correctness. bbq_htree reserves key 0.
static uint32_t name_key(const uint8_t* p, size_t len) {
    uint32_t h = 5381;
    for (size_t i = 0; i < len; i++) h = h * 33 + p[i];
    return h ? h : 1;
}

// ── §3.5.10 exports: names distinct, indices in range for the kind ──
static jav_err_t exports_err(const bbq_field_capture* root, const uint8_t* base, const jav_modidx_t* mod) {
    const bbq_field_capture* ex = jav_view_section_array(root, 7, "exports", base);
    uint32_t n = jav_view_nchild(ex);
    uint32_t bound[5] = { mod->nfuncs, mod->ntables, mod->nmems, mod->nglobals, mod->ntags };
    jav_err_t unk[5] = { JAV_E_UNKNOWN_FUNCTION, JAV_E_UNKNOWN_TABLE, JAV_E_UNKNOWN_MEMORY,
                         JAV_E_UNKNOWN_GLOBAL, JAV_E_UNKNOWN_TAG };
    // "Names are distinct" is a set-membership question, asked n times. Comparing each export
    // against every earlier one answers it by pairing instead, which is n(n−1)/2 comparisons —
    // 2,120,770 of them for a 2060-export module — and resolved the earlier name's span out of
    // the capture tree inside that loop, so the by-name search ran once per PAIR.
    //
    // Each name is hashed into a bucket once. A bucket hit is only a CANDIDATE: the bytes
    // decide, and every export that hashed to a bucket stays reachable through `prev`, so a
    // hash collision between two distinct names costs one memcmp and can never hide a later
    // duplicate of either. Worst case (every name colliding) is the pairing we started with;
    // expected case is one memcmp per export.
    struct name_span { size_t off, len; }* name = NULL;   // name[i] — resolved once, per export
    int32_t* prev = NULL;              // prev[i] — the previous export in i's bucket, or −1
    bbq_htree* seen = bbq_htree_create();                 // name hash → 1 + newest index
    bbq_vec_reserve(name, n); bbq_vec_reserve(prev, n);
    jav_err_t err = JAV_E_NONE;
    for (uint32_t i = 0; i < n && err == JAV_E_NONE; i++) {
        const bbq_field_capture* e = &ex->children[i];
        uint8_t k = (uint8_t)bbq_node_int(jav_view_field(e, "kind"), base);
        if (k > 4) { err = JAV_E_UNKNOWN_FUNCTION; break; }
        if ((uint32_t)bbq_node_int(jav_view_field(e, "idx"), base) >= bound[k]) { err = unk[k]; break; }
        const bbq_field_capture* bi = jav_view_field(jav_view_field(e, "name"), "bytes");
        struct name_span s = { bi->start_offset, bi->end_offset - bi->start_offset };
        uint32_t key = name_key(base + s.off, s.len);
        int32_t head = (int32_t)(intptr_t)bbq_htree_search(seen, key) - 1;   // absent → NULL → −1
        for (int32_t j = head; j >= 0; j = prev[j])
            if (name[j].len == s.len && memcmp(base + s.off, base + name[j].off, s.len) == 0) {
                err = JAV_E_DUPLICATE_EXPORT_NAME; break;
            }
        bbq_vec_push(name, s); bbq_vec_push(prev, head);
        bbq_htree_insert(seen, key, (void*)(uintptr_t)(i + 1));
    }
    bbq_htree_destroy(seen);
    bbq_vec_free(prev); bbq_vec_free(name);
    return err;
}

// ── §3.5.12 start: funcidx exists and its type is [] -> [] ──
static jav_err_t start_err(const bbq_field_capture* root, const uint8_t* base, const jav_modidx_t* mod) {
    const bbq_field_capture* s = jav_view_find_section(root, 8, base);
    if (!s) return JAV_E_NONE;
    uint32_t fi = (uint32_t)bbq_node_int(jav_view_field(jav_view_field(s, "body"), "func"), base);
    if (fi >= mod->nfuncs) return JAV_E_UNKNOWN_FUNCTION;
    return (mod->func_sigs[fi].nparams == 0 && mod->func_sigs[fi].nresults == 0)
           ? JAV_E_NONE : JAV_E_START_FUNCTION;
}

// ── §7.6 re-derive a body's side-table (shared with the instantiator) ──
int jav_body_typecheck(const jav_modidx_t* mod, const uint8_t* base,
                       const bbq_field_capture* entry, const jav_functype_t* sig,
                       const uint8_t* func_ref_declared,
                       uint32_t* out_ndecl, jav_st_entry_t** st, unsigned* nst,
                       jav_try_t** tr, unsigned* ntr, jav_err_t* out_err,
                       jav_valtype_t** out_locals) {
    if (out_err) *out_err = JAV_E_NONE;
    const bbq_field_capture* fb = jav_view_field(entry, "body");
    const bbq_field_capture* groups = jav_view_field(fb, "locals");
    const bbq_field_capture* expr = jav_view_field(fb, "body");

    uint32_t ndecl = 0;                                            // §5.5.13 RLE locals → flat
    for (uint32_t gi = 0; gi < jav_view_nchild(groups); gi++)
        ndecl += (uint32_t)bbq_node_int(jav_view_field(&groups->children[gi], "count"), base);
    if (out_ndecl) *out_ndecl = ndecl;
    // The flat locals (params then declared) are crt bbq_vecs — pushed in order, the
    // length is bbq_vec_len; reserve so the final pointer handed to the cx is stable.
    jav_valtype_t* loc = NULL; uint32_t* lox = NULL;
    bbq_vec_reserve(loc, sig->nparams + ndecl + 1); bbq_vec_reserve(lox, sig->nparams + ndecl + 1);
    for (uint16_t i = 0; i < sig->nparams; i++) {
        bbq_vec_push(loc, sig->params[i]);
        uint32_t tx = sig->param_tidx ? sig->param_tidx[i] : 0; bbq_vec_push(lox, tx);
    }
    int ok = 1;
    for (uint32_t gi = 0; gi < jav_view_nchild(groups) && ok; gi++) {
        jav_valtype_t vt; uint32_t tx;
        if (!jav_index_decode_valtype(jav_view_field(&groups->children[gi], "type"), base, mod, &vt, &tx)) { ok = 0; if (out_err) *out_err = JAV_E_UNKNOWN_TYPE; break; }   /* a local's reftype names an undefined type index */
        uint32_t c = (uint32_t)bbq_node_int(jav_view_field(&groups->children[gi], "count"), base);
        for (uint32_t ci = 0; ci < c; ci++) { bbq_vec_push(loc, vt); bbq_vec_push(lox, tx); }
    }
    *st = NULL; *nst = 0; *tr = NULL; *ntr = 0;
    if (ok) {
        jav_vctx_t cx = jav_module_cx(mod);
        cx.locals = loc; cx.local_tidx = lox; cx.nlocals = (unsigned)bbq_vec_len(loc);
        cx.nparams = sig->nparams;             /* params start initialized (§3.4.2 local-init) */
        cx.results = sig->results; cx.result_tidx = sig->result_tidx; cx.nresults = sig->nresults;
        cx.func_ref_declared = func_ref_declared;
        ok = jav_typecheck_ex(base + expr->start_offset, expr->end_offset - expr->start_offset,
                              &cx, st, nst, tr, ntr, out_err);
    }
    if (out_locals) *out_locals = loc; else bbq_vec_free(loc);
    bbq_vec_free(lox);
    return ok;
}

// ── §7.6 every defined function body type-checks, and its tables go to the module ──
// The side-table, try-table and flat locals a body's check produces are what running
// it needs, and they depend only on the bytes — so the module holds them and every
// instance reads the same ones.
static jav_err_t bodies_err(const bbq_field_capture* root, const uint8_t* base, jav_modidx_t* mod,
                            const uint8_t* declared) {
    uint32_t ndef = mod->nfuncs - mod->nimport_funcs;
    const bbq_field_capture* entries = jav_view_section_array(root, 10, "entries", base);
    if (jav_view_nchild(entries) != ndef) return JAV_E_TYPE_MISMATCH;        // count disagreement
    for (uint32_t d = 0; d < ndef; d++) {
        jav_st_entry_t* st; unsigned nst; jav_try_t* tr; unsigned ntr; jav_err_t err = JAV_E_TYPE_MISMATCH;
        uint32_t ndecl = 0; jav_valtype_t* loc = NULL;
        int ok = jav_body_typecheck(mod, base, &entries->children[d],
                                    &mod->func_sigs[mod->nimport_funcs + d], declared, &ndecl,
                                    &st, &nst, &tr, &ntr, &err, &loc);
        if (mod->nbodies) {
            mod->body_st[d] = st; mod->body_tr[d] = tr;
            mod->body_locals[d] = loc; mod->body_ndecl[d] = ndecl;
        } else { bbq_vec_free(st); bbq_vec_free(tr); bbq_vec_free(loc); }
        if (!ok) return err;                                       // the specific §7.6 reject reason
    }
    return JAV_E_NONE;
}

// ── §3.3.10 constant expressions ──────────────────────────────────────────────
static void mark_ref(bbq_htree* refs, uint32_t nf, uint32_t x) {
    if (refs && x < nf) bbq_htree_insert(refs, x, (void*)(uintptr_t)1);
}

static void scan_reffunc(const uint8_t* code, size_t len, bbq_htree* refs, uint32_t nf) {
    bbq_ctx_t c; bbq_ctx_init(&c, code, len);
    uint8_t op;
    while (bbq_read_u8(&c, &op) && op != 0x0b) {
        switch (op) {
        case 0x41: { int32_t v; bbq_read_sleb128_i32(&c, &v); break; }
        case 0x42: { int64_t v; bbq_read_sleb128_i64(&c, &v); break; }
        case 0x43: { float  v; bbq_read_f32le(&c, &v); break; }
        case 0x44: { double v; bbq_read_f64le(&c, &v); break; }
        case 0x23: { uint32_t x; bbq_read_uleb128_u32(&c, &x); break; }
        case 0xd0: { int64_t ht; bbq_read_sleb128_i64(&c, &ht); break; }
        case 0xd2: { uint32_t x; if (bbq_read_uleb128_u32(&c, &x)) mark_ref(refs, nf, x); break; }
        case 0xfb: { uint32_t sub; if (!bbq_read_uleb128_u32(&c, &sub)) return;   // skip GC const op immediates
                     if (sub == 0x00 || sub == 0x01 || sub == 0x06 || sub == 0x07) { uint32_t x; bbq_read_uleb128_u32(&c, &x); }
                     else if (sub == 0x08) { uint32_t x, n; bbq_read_uleb128_u32(&c, &x); bbq_read_uleb128_u32(&c, &n); }
                     break; }
        case 0xfd: { uint32_t sub; if (!bbq_read_uleb128_u32(&c, &sub)) return;   // v128.const: skip 16 bytes
                     if (sub == 0x0c) for (int i = 0; i < 16; i++) { uint8_t b; if (!bbq_read_u8(&c, &b)) return; }
                     break; }
        default: break;
        }
    }
}

// const-only + context scan (§3.3.10 + §3.5.10): admissible opcode; global.get of an
// in-scope IMMUTABLE global; ref.func of a declared func. JAV_E_NONE if so.
static jav_err_t const_scan(const uint8_t* code, size_t len, const jav_modidx_t* mod,
                            uint32_t nglob_scope, const bbq_htree* refs) {
    bbq_ctx_t c; bbq_ctx_init(&c, code, len);
    for (;;) {
        uint8_t op;
        if (!bbq_read_u8(&c, &op)) return JAV_E_CONST_EXPR_REQUIRED;   // no `end`
        if (op == 0x0b) return JAV_E_NONE;
        switch (op) {
        case 0x41: { int32_t v; if (!bbq_read_sleb128_i32(&c, &v)) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0x42: { int64_t v; if (!bbq_read_sleb128_i64(&c, &v)) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0x43: { float  v; if (!bbq_read_f32le(&c, &v)) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0x44: { double v; if (!bbq_read_f64le(&c, &v)) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0x6a: case 0x6b: case 0x6c:
        case 0x7c: case 0x7d: case 0x7e: break;
        case 0x23: { uint32_t x; if (!bbq_read_uleb128_u32(&c, &x)) return JAV_E_CONST_EXPR_REQUIRED;
                     if (x >= nglob_scope) return JAV_E_UNKNOWN_GLOBAL;
                     if (mod->global_mut[x]) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0xd0: { int64_t ht; if (!bbq_read_sleb128_i64(&c, &ht)) return JAV_E_CONST_EXPR_REQUIRED; break; }
        case 0xd2: { uint32_t x; if (!bbq_read_uleb128_u32(&c, &x)) return JAV_E_CONST_EXPR_REQUIRED;
                     if (x >= mod->nfuncs) return JAV_E_UNKNOWN_FUNCTION;
                     if (!bbq_htree_search(refs, x)) return JAV_E_UNDECLARED_FUNCTION_REFERENCE; break; }
        case 0xfb: {                                                   // §3-const GC ops (0xFB prefix)
            uint32_t sub; if (!bbq_read_uleb128_u32(&c, &sub)) return JAV_E_CONST_EXPR_REQUIRED;
            switch (sub) {
            case 0x00: case 0x01:                                     // struct.new / struct.new_default $x
            case 0x06: case 0x07: { uint32_t x;                       // array.new / array.new_default $x
                if (!bbq_read_uleb128_u32(&c, &x)) return JAV_E_CONST_EXPR_REQUIRED; break; }
            case 0x08: { uint32_t x, n;                               // array.new_fixed $x n
                if (!bbq_read_uleb128_u32(&c, &x) || !bbq_read_uleb128_u32(&c, &n)) return JAV_E_CONST_EXPR_REQUIRED; break; }
            case 0x1a: case 0x1b: case 0x1c: break;                   // any.convert_extern / extern.convert_any / ref.i31
            default: return JAV_E_CONST_EXPR_REQUIRED;                 // any other 0xFB op is non-constant
            }
            break;
        }
        case 0xfd: {                                                   // §3-const vector op (0xFD prefix)
            uint32_t sub; if (!bbq_read_uleb128_u32(&c, &sub)) return JAV_E_CONST_EXPR_REQUIRED;
            if (sub != 0x0c) return JAV_E_CONST_EXPR_REQUIRED;        // only v128.const is constant
            for (int i = 0; i < 16; i++) { uint8_t b; if (!bbq_read_u8(&c, &b)) return JAV_E_CONST_EXPR_REQUIRED; }
            break;
        }
        default: return JAV_E_CONST_EXPR_REQUIRED;
        }
    }
}

// A constant expression valid with result type `want`: const-only + context, AND
// well-typed against `want` (typing reused from jav_typecheck).
static jav_err_t const_expr_err(const uint8_t* code, size_t len, const jav_modidx_t* mod,
                                jav_valtype_t want, uint32_t want_tidx, uint32_t nglob_scope,
                                const bbq_htree* refs) {
    jav_err_t e = const_scan(code, len, mod, nglob_scope, refs);
    if (e) return e;
    jav_vctx_t cx = jav_module_cx(mod);
    cx.nglobals = nglob_scope;
    cx.results = &want; cx.result_tidx = &want_tidx; cx.nresults = 1;
    jav_st_entry_t* st = NULL; unsigned nst = 0; jav_try_t* tr = NULL; unsigned ntr = 0;
    jav_err_t terr = JAV_E_TYPE_MISMATCH;
    int ok = jav_typecheck_ex(code, len, &cx, &st, &nst, &tr, &ntr, &terr);
    bbq_vec_free(st); bbq_vec_free(tr);
    return ok ? JAV_E_NONE : terr;
}
static jav_err_t expr_node_err(const bbq_field_capture* e, const uint8_t* base, const jav_modidx_t* mod,
                               jav_valtype_t want, uint32_t want_tidx, uint32_t scope, const bbq_htree* refs) {
    return const_expr_err(base + e->start_offset, e->end_offset - e->start_offset, mod,
                          want, want_tidx, scope, refs);
}

// §3.5.10 C.refs = funcidx(global* table* elem* export*). The text-format reduction
// (§6.6.x) gives a start `(start x ⇒ {})`: the start function contributes NO reference,
// so a ref.func of a func that is only the start target is "undeclared".
static void collect_refs(const bbq_field_capture* root, const uint8_t* base,
                         const jav_modidx_t* mod, bbq_htree* refs) {
    uint32_t nf = mod->nfuncs;
    const bbq_field_capture* ex = jav_view_section_array(root, 7, "exports", base);
    for (uint32_t i = 0; i < jav_view_nchild(ex); i++)
        if ((uint8_t)bbq_node_int(jav_view_field(&ex->children[i], "kind"), base) == 0x00)
            mark_ref(refs, nf, (uint32_t)bbq_node_int(jav_view_field(&ex->children[i], "idx"), base));
    const bbq_field_capture* gl = jav_view_section_array(root, 6, "globals", base);
    for (uint32_t i = 0; i < jav_view_nchild(gl); i++) {
        const bbq_field_capture* in = jav_view_field(&gl->children[i], "init");
        scan_reffunc(base + in->start_offset, in->end_offset - in->start_offset, refs, nf);
    }
    const bbq_field_capture* el = jav_view_section_array(root, 9, "elems", base);
    for (uint32_t i = 0; i < jav_view_nchild(el); i++) {
        const bbq_field_capture* b = jav_view_field(&el->children[i], "body");
        const bbq_field_capture* fl = jav_view_field(b, "funcs");
        if (fl) { const bbq_field_capture* ix = jav_view_field(fl, "idxs");
            for (uint32_t j = 0; j < jav_view_nchild(ix); j++) mark_ref(refs, nf, (uint32_t)bbq_node_int(&ix->children[j], base)); }
        const bbq_field_capture* xl = jav_view_field(b, "exprs");
        if (xl) { const bbq_field_capture* ea = jav_view_field(xl, "exprs");
            for (uint32_t j = 0; j < jav_view_nchild(ea); j++) { const bbq_field_capture* e = &ea->children[j];
                scan_reffunc(base + e->start_offset, e->end_offset - e->start_offset, refs, nf); } }
    }
    const bbq_field_capture* tb = jav_view_section_array(root, 4, "tables", base);
    for (uint32_t i = 0; i < jav_view_nchild(tb); i++) {
        const bbq_field_capture* init = tb->children[i].child_count ? jav_view_field(&tb->children[i].children[0], "init") : NULL;
        if (init) scan_reffunc(base + init->start_offset, init->end_offset - init->start_offset, refs, nf);
    }
}

// §3.5.3 global init-exprs: const + typed to the declared type, seeing imports + earlier globals.
static jav_err_t globals_err(const bbq_field_capture* root, const uint8_t* base,
                             const jav_modidx_t* mod, const bbq_htree* refs) {
    const bbq_field_capture* gl = jav_view_section_array(root, 6, "globals", base);
    for (uint32_t i = 0; i < jav_view_nchild(gl); i++) {
        uint32_t gidx = mod->nimport_globals + i;
        const bbq_field_capture* in = jav_view_field(&gl->children[i], "init");
        jav_err_t e = const_expr_err(base + in->start_offset, in->end_offset - in->start_offset, mod,
                                     mod->global_types[gidx], mod->global_tidx[gidx],
                                     mod->nimport_globals + i, refs);
        if (e) return e;
    }
    return JAV_E_NONE;
}

// A boundary reftype as the lattice's (nullable, ht); 0 if not a reference type.
static int ref_to_ht(jav_valtype_t w, uint32_t tidx, int* nn, int32_t* ht) {
    switch (w) {
    case WVT_REF:          *nn = 1; *ht = (int32_t)tidx; return 1;   /* generic (ref null heaptype) */
    case WVT_REF_NN:       *nn = 0; *ht = (int32_t)tidx; return 1;   /* generic (ref heaptype) */
    default: return 0;
    }
}
static int reftype_sub(const jav_modidx_t* mod, jav_valtype_t aw, uint32_t ax,
                       jav_valtype_t bw, uint32_t bx) {
    int an, bn; int32_t ah, bh;
    if (!ref_to_ht(aw, ax, &an, &ah) || !ref_to_ht(bw, bx, &bn, &bh)) return 0;
    return jav_rt_sub(&mod->lattice, an, ah, bn, bh);
}

// ── §3.2.11 type-section validation: a declared sub type must structurally MATCH its
// supertype (§3.3.8 composite + §3.3.9 field variance). The §3.3.10 use-site relation
// (jav_subtype.c) trusts `supers[]`; this pass is what makes that trust sound.

// §3.3.4 value-type sub: numbers exact, references via the §3.3 lattice.
static int vt_sub(const jav_modidx_t* mod, jav_valtype_t aw, uint32_t ax, jav_valtype_t bw, uint32_t bx) {
    int an, bn; int32_t ah, bh;
    int aref = ref_to_ht(aw, ax, &an, &ah), bref = ref_to_ht(bw, bx, &bn, &bh);
    if (aref && bref) return jav_rt_sub(&mod->lattice, an, ah, bn, bh);
    if (!aref && !bref) return aw == bw;
    return 0;
}
// §3.3.9 storage-type sub incl. packed widths (a packed type matches only itself).
static int storage_sub(const jav_modidx_t* mod, jav_valtype_t aw, uint32_t ax, uint8_t apk,
                       jav_valtype_t bw, uint32_t bx, uint8_t bpk) {
    if (apk || bpk) return apk == bpk;
    return vt_sub(mod, aw, ax, bw, bx);
}
// §3.3.9 field-type sub: mutability invariant; immutable COVARIANT, mutable INVARIANT.
static int field_sub(const jav_modidx_t* mod, jav_valtype_t aw, uint32_t ax, uint8_t apk, uint8_t amut,
                     jav_valtype_t bw, uint32_t bx, uint8_t bpk, uint8_t bmut) {
    if (amut != bmut || !storage_sub(mod, aw, ax, apk, bw, bx, bpk)) return 0;
    return amut ? storage_sub(mod, bw, bx, bpk, aw, ax, apk) : 1;   /* mutable: also the reverse */
}
// §3.3.8 composite sub: struct width+depth, array element, func params-contra/results-co.
// Caller guarantees kinds[a] == kinds[b].
static int comptype_sub(const jav_modidx_t* mod, uint32_t a, uint32_t b) {
    const uint8_t* apk = mod->type_field_packs ? mod->type_field_packs[a] : NULL;
    const uint8_t* bpk = mod->type_field_packs ? mod->type_field_packs[b] : NULL;
    if (mod->kinds[a] == WST_STRUCT) {
        const jav_structtype_t* sa = &mod->structtypes[a]; const jav_structtype_t* sb = &mod->structtypes[b];
        if (sa->nfields < sb->nfields) return 0;                       // width: subtype keeps ≥ fields
        for (unsigned i = 0; i < sb->nfields; i++)
            if (!field_sub(mod, sa->fields[i], sa->field_tidx ? sa->field_tidx[i] : 0, apk ? apk[i] : 0,
                                sa->field_mut ? sa->field_mut[i] : 0,
                                sb->fields[i], sb->field_tidx ? sb->field_tidx[i] : 0, bpk ? bpk[i] : 0,
                                sb->field_mut ? sb->field_mut[i] : 0)) return 0;
        return 1;
    }
    if (mod->kinds[a] == WST_ARRAY) {
        const jav_arraytype_t* aa = &mod->arraytypes[a]; const jav_arraytype_t* ab = &mod->arraytypes[b];
        return field_sub(mod, aa->elem, aa->elem_tidx, apk ? apk[0] : 0, aa->elem_mut,
                              ab->elem, ab->elem_tidx, bpk ? bpk[0] : 0, ab->elem_mut);
    }
    const jav_functype_t* fa = &mod->functypes[a]; const jav_functype_t* fb = &mod->functypes[b];
    if (fa->nparams != fb->nparams || fa->nresults != fb->nresults) return 0;
    for (uint16_t i = 0; i < fa->nparams; i++)                          // params CONTRAVARIANT: super ≤ sub
        if (!vt_sub(mod, fb->params[i], fb->param_tidx ? fb->param_tidx[i] : 0,
                         fa->params[i], fa->param_tidx ? fa->param_tidx[i] : 0)) return 0;
    for (uint16_t i = 0; i < fa->nresults; i++)                         // results COVARIANT: sub ≤ super
        if (!vt_sub(mod, fa->results[i], fa->result_tidx ? fa->result_tidx[i] : 0,
                         fb->results[i], fb->result_tidx ? fb->result_tidx[i] : 0)) return 0;
    return 1;
}
// §3.2.11 "C ⊢ type ok": a reference type may only name a concrete type index that is defined.
// (Abstract heaptypes carry a negative HT_* code; only non-negative indices are bounds-checked.)
static int htref_ok(const jav_modidx_t* mod, jav_valtype_t w, uint32_t tx) {
    if (w != WVT_REF && w != WVT_REF_NN) return 1;
    int32_t ht = (int32_t)tx;
    return ht < 0 || (uint32_t)ht < mod->ntypes;
}
static jav_err_t type_closed_err(const jav_modidx_t* mod) {
    for (uint32_t t = 0; t < mod->ntypes; t++) {
        if (mod->kinds[t] == WST_STRUCT) {
            const jav_structtype_t* s = &mod->structtypes[t];
            for (unsigned i = 0; i < s->nfields; i++)
                if (!htref_ok(mod, s->fields[i], s->field_tidx ? s->field_tidx[i] : 0)) return JAV_E_UNKNOWN_TYPE;
        } else if (mod->kinds[t] == WST_ARRAY) {
            const jav_arraytype_t* a = &mod->arraytypes[t];
            if (!htref_ok(mod, a->elem, a->elem_tidx)) return JAV_E_UNKNOWN_TYPE;
        } else {
            const jav_functype_t* f = &mod->functypes[t];
            for (uint16_t i = 0; i < f->nparams; i++)
                if (!htref_ok(mod, f->params[i], f->param_tidx ? f->param_tidx[i] : 0)) return JAV_E_UNKNOWN_TYPE;
            for (uint16_t i = 0; i < f->nresults; i++)
                if (!htref_ok(mod, f->results[i], f->result_tidx ? f->result_tidx[i] : 0)) return JAV_E_UNKNOWN_TYPE;
        }
    }
    return JAV_E_NONE;
}

// §3.2.11: each declared supertype must be earlier (no cycles), non-final, same kind, and
// structurally matched; at most one supertype.
static jav_err_t types_err(const jav_modidx_t* mod) {
    jav_err_t ce = type_closed_err(mod);                   // all named type indices defined first
    if (ce) return ce;
    for (uint32_t t = 0; t < mod->ntypes; t++) {
        if (mod->nsupers[t] > 1) return JAV_E_TYPE_MISMATCH;            // |x*| ≤ 1
        int32_t sup = mod->supers[t];
        if (sup < 0) continue;
        if ((uint32_t)sup >= t) return JAV_E_UNKNOWN_TYPE;            // x < x₀: defined earlier, no cycles
        if (mod->finality[sup]) return JAV_E_SUB_TYPE;               // supertype must not be final
        if (mod->kinds[t] != mod->kinds[sup]) return JAV_E_SUB_TYPE; // same composite kind
        if (!comptype_sub(mod, t, (uint32_t)sup)) return JAV_E_SUB_TYPE;  // structural match
    }
    return JAV_E_NONE;
}

// §3.5.9 element segments. Offsets/exprs see the full context C (all globals).
static jav_err_t elems_err(const bbq_field_capture* root, const uint8_t* base,
                           const jav_modidx_t* mod, const bbq_htree* refs) {
    const bbq_field_capture* el = jav_view_section_array(root, 9, "elems", base);
    for (uint32_t i = 0; i < jav_view_nchild(el); i++) {
        const bbq_field_capture* b = jav_view_field(&el->children[i], "body");
        // §5.5.12 element type: explicit reftype (flags 5/6/7); a funcidx list (flags 0-3,
        // present as "funcs") is `(ref func)` NON-NULL; a bare expr list (flag 4) is
        // `(ref null func)` = funcref.
        const bbq_field_capture* rt = jav_view_field(b, "reftype");
        const bbq_field_capture* fl = jav_view_field(b, "funcs");
        jav_valtype_t et; uint32_t etx = 0;
        if (rt) { if (!jav_index_decode_valtype(rt, base, mod, &et, &etx)) return JAV_E_TYPE_MISMATCH; }
        else { et = fl ? WVT_REF_NN : WVT_REF; etx = (uint32_t)HT_FUNC; }   // funcidx list → (ref func), bare exprs → funcref

        if (fl) { const bbq_field_capture* ix = jav_view_field(fl, "idxs");
            for (uint32_t j = 0; j < jav_view_nchild(ix); j++)
                if ((uint32_t)bbq_node_int(&ix->children[j], base) >= mod->nfuncs) return JAV_E_UNKNOWN_FUNCTION; }
        const bbq_field_capture* xl = jav_view_field(b, "exprs");
        if (xl) { const bbq_field_capture* ea = jav_view_field(xl, "exprs");
            for (uint32_t j = 0; j < jav_view_nchild(ea); j++) {
                jav_err_t e = expr_node_err(&ea->children[j], base, mod, et, etx, mod->nglobals, refs);
                if (e) return e; } }

        const bbq_field_capture* off = jav_view_field(b, "offset");
        if (off) {
            const bbq_field_capture* tn = jav_view_field(b, "table");
            uint32_t ti = tn ? (uint32_t)bbq_node_int(tn, base) : 0;
            if (ti >= mod->ntables) return JAV_E_UNKNOWN_TABLE;
            if (!reftype_sub(mod, et, etx, mod->table_reftype[ti], mod->table_tidx[ti])) return JAV_E_TYPE_MISMATCH;
            jav_valtype_t at = mod->table_is64[ti] ? WVT_I64 : WVT_I32;
            jav_err_t e = expr_node_err(off, base, mod, at, 0, mod->nglobals, refs);
            if (e) return e;
        }
    }
    return JAV_E_NONE;
}

// §3.5.8 data segments. Active offset typed to the target memory's addrtype, under C.
static jav_err_t datas_err(const bbq_field_capture* root, const uint8_t* base,
                           const jav_modidx_t* mod, const bbq_htree* refs) {
    const bbq_field_capture* dl = jav_view_section_array(root, 11, "datas", base);
    for (uint32_t i = 0; i < jav_view_nchild(dl); i++) {
        const bbq_field_capture* b = jav_view_field(&dl->children[i], "body");
        const bbq_field_capture* off = jav_view_field(b, "offset");
        if (!off) continue;
        const bbq_field_capture* mn = jav_view_field(b, "memidx");
        uint32_t mi = mn ? (uint32_t)bbq_node_int(mn, base) : 0;
        if (mi >= mod->nmems) return JAV_E_UNKNOWN_MEMORY;
        jav_valtype_t at = mod->mem_is64[mi] ? WVT_I64 : WVT_I32;
        jav_err_t e = expr_node_err(off, base, mod, at, 0, mod->nglobals, refs);
        if (e) return e;
    }
    return JAV_E_NONE;
}

// §3.5.5 table inits (the 0x40 form): const + typed to the reftype, under C' (imports only).
static jav_err_t tables_err(const bbq_field_capture* root, const uint8_t* base,
                            const jav_modidx_t* mod, const bbq_htree* refs) {
    const bbq_field_capture* tb = jav_view_section_array(root, 4, "tables", base);
    for (uint32_t i = 0; i < jav_view_nchild(tb); i++) {
        const bbq_field_capture* init = tb->children[i].child_count ? jav_view_field(&tb->children[i].children[0], "init") : NULL;
        uint32_t ti = mod->nimport_tables + i;
        if (!init) {
            // §5.5.7 short form `table tt` decodes only if tt = at lim (ref null? ht): a non-null
            // element type has no ref.null default, so the no-init form is invalid for it.
            if (mod->table_reftype[ti] == WVT_REF_NN) return JAV_E_TYPE_MISMATCH;
            continue;
        }
        jav_err_t e = expr_node_err(init, base, mod, mod->table_reftype[ti], mod->table_tidx[ti],
                                    mod->nimport_globals, refs);
        if (e) return e;
    }
    return JAV_E_NONE;
}

// §2.5.3 / §3.2.13 a tag's defined type expands to a function type whose result type is
// empty (Wasm 3.0 has only exception tags). A non-empty result is invalid.
static jav_err_t tags_err(const jav_modidx_t* mod) {
    for (uint32_t i = 0; i < mod->ntags; i++)
        if (mod->tags[i].nresults != 0) return JAV_E_NONEMPTY_TAG_RESULT;
    return JAV_E_NONE;
}

jav_status_t jav_module_validate(const bbq_field_capture* root, const uint8_t* base,
                                 jav_modidx_t* mod, jav_err_t* err) {
    jav_err_t e = types_err(mod);                        // §3.2.11 sub types valid before anything uses them
    if (!e) e = tags_err(mod);                           // §3.2.13 tag types: empty result
    if (!e) e = limits_err(mod);
    if (!e) e = start_err(root, base, mod);
    if (!e) e = exports_err(root, base, mod);
    if (!e) {                                            // C.refs is needed by BOTH bodies (ref.func) and const-exprs
        bbq_htree* refs = bbq_htree_create();            // the declared-funcref set (§3.5.10/.13)
        collect_refs(root, base, mod, refs);
        uint8_t* declared = mod->nfuncs ? calloc(mod->nfuncs, 1) : NULL;   // C.refs as a per-funcidx bitmap
        for (uint32_t f = 0; f < mod->nfuncs; f++) if (bbq_htree_search(refs, f)) declared[f] = 1;
        e = bodies_err(root, base, mod, declared);
        if (!e) e = globals_err(root, base, mod, refs);
        if (!e) e = tables_err(root, base, mod, refs);
        if (!e) e = elems_err(root, base, mod, refs);
        if (!e) e = datas_err(root, base, mod, refs);
        free(declared);
        bbq_htree_destroy(refs);
    }
    if (err) *err = e;
    return e ? JAV_INVALID : JAV_OK;
}
