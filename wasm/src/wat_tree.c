/*
 * wat_tree.c — see wat_tree.h. The builder decides everything the text will
 * SAY; the emitter decides only where the whitespace goes.
 *
 * Spelling decisions made here, each §6-cited at its site:
 *   - every §6.1.2 abbreviation whose expansion the module's own layout
 *     reproduces is applied (omitted defaults, `sub final` elision, bare
 *     comptypes); the ones that would move a declaration are not;
 *   - floats print as hex floats (§6.3.2: "Rounding can be prevented by
 *     using hexadecimal notation"), `inf`/`nan`/`nan:0x…` for the specials —
 *     bit-exactness is the contract, and test_wat_emit pins it;
 *   - a fold is the admissible k wat_check computed, except past the depth
 *     cap, where an instruction keeps its operands as preceding statements
 *     instead (folding less is always §6.5.11-legal; failing is not).
 */
#include "wat_tree.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bbq_vec.h"
#include "wat_mnemonics.h"
#include "wat_render.h"

/* A spine deeper than this hoists instead — recursion in the labeller and
 * the emitter tracks expression depth, and a hoist only costs a named
 * intermediate line, never correctness. */
#define WAT_FOLD_CAP 200

typedef struct {
    const jav_module_t*    m;
    const wat_check_ctx_t* cx;
    bbq_arena*             a;
    char*                  pool;     /* bbq_vec */
    wat_atom_t*            atoms;    /* bbq_vec */
    wat_tnode_t**          roots;    /* bbq_vec */
    uint32_t*              decls;    /* bbq_vec */
    char*                  sc;       /* bbq_vec: the token being rendered */
    const wat_body_t*      rows;     /* the current body's §7.6 product */
    /* typeidx → what a spelled type needs: the functype for echoes and call
     * arities in const exprs, the field count for struct.new. */
    const jav_func_type_t** ftype;
    uint32_t*               nfields;
    uint32_t                ntypes;
    /* (prefix, op) → node tag, unpacked from wat_instr_tags once. */
    uint16_t tag0[256];
    uint16_t tagfb[64];
    uint16_t tagfc[64];
    uint16_t tagfd[512];
    jav_err_t err;
    int       failed;
} wb_t;

/* ── text accumulation ──────────────────────────────────────────────────── */

static void sc_raw(wb_t* w, const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) bbq_vec_push(w->sc, s[i]);
}
static void sc(wb_t* w, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;
    sc_raw(w, buf, (size_t)n);
}

/* Freeze the scratch into the pool; returns the offset, width in *wid. */
static uint32_t pool_take(wb_t* w, uint32_t* wid) {
    uint32_t off = (uint32_t)bbq_vec_len(w->pool);
    *wid = (uint32_t)bbq_vec_len(w->sc);
    for (size_t i = 0; i < bbq_vec_len(w->sc); i++) bbq_vec_push(w->pool, w->sc[i]);
    bbq_vec_push(w->pool, '\0');
    bbq_vec_clear(w->sc);
    return off;
}

static void set_ptxt(wb_t* w, wat_tnode_t* n) {
    if (bbq_vec_len(w->sc) == 0) return;
    n->ptxt = pool_take(w, &n->pw);
}

/* Freeze the scratch as one atom; a no-op scratch still makes an atom (the
 * caller asked for a token). */
static void atom_take(wb_t* w) {
    wat_atom_t at;
    at.off = pool_take(w, &at.w);
    bbq_vec_push(w->atoms, at);
}

static wat_tnode_t* node_new(wb_t* w, uint16_t tag) {
    wat_tnode_t* n = bbq_arena_alloc(w->a, sizeof *n);
    memset(n, 0, sizeof *n);
    n->tag = tag;
    n->ptxt = WAT_TNODE_NONE;
    return n;
}

static uint32_t root_push(wb_t* w, wat_tnode_t* n) {
    uint32_t idx = (uint32_t)bbq_vec_len(w->roots);
    bbq_vec_push(w->roots, n);
    return idx;
}

/* Append a locally-built list of roots contiguously; record the span. */
static void span_commit(wb_t* w, wat_tnode_t** list, uint32_t* off, uint32_t* n) {
    *off = (uint32_t)bbq_vec_len(w->roots);
    *n = (uint32_t)bbq_vec_len(list);
    for (size_t i = 0; i < bbq_vec_len(list); i++) bbq_vec_push(w->roots, list[i]);
}

static void fail_at(wb_t* w, jav_err_t e, int line) {
    if (!w->failed) {
        w->failed = 1;
        w->err = e;
        if (getenv("WAT_TREE_VV"))
            fprintf(stderr, "wat_tree: refused at line %d (err %d)\n", line, (int)e);
    }
}
#define fail(w, e) fail_at((w), (e), __LINE__)

static void av_bytes(wb_t* w, const uint8_t* p, size_t len);

/* ── spelling: types, names, numbers ────────────────────────────────────── */

/* §6.4.3. Negative = an abstract heap type (the byte, s33-decoded);
 * non-negative = a type index. */
static void sp_heap(wb_t* w, int64_t ht) {
    if (ht >= 0) { sc(w, "%" PRId64, ht); return; }
    switch ((int)ht) {
    case -16: sc(w, "func");     return;
    case -17: sc(w, "extern");   return;
    case -18: sc(w, "any");      return;
    case -19: sc(w, "eq");       return;
    case -20: sc(w, "i31");      return;
    case -21: sc(w, "struct");   return;
    case -22: sc(w, "array");    return;
    case -15: sc(w, "none");     return;
    case -14: sc(w, "noextern"); return;
    case -13: sc(w, "nofunc");   return;
    case -23: sc(w, "exn");      return;
    case -12: sc(w, "noexn");    return;
    default:  fail(w, JAV_E_TYPE_MISMATCH); return;
    }
}

/* §6.4.4, including the twelve shorthands. */
static void sp_reftype(wb_t* w, const jav_ref_type_t* rt) {
    switch (rt->head) {
    case 0x63: sc(w, "(ref null "); sp_heap(w, rt->ht.value.x); sc(w, ")"); return;
    case 0x64: sc(w, "(ref ");      sp_heap(w, rt->ht.value.x); sc(w, ")"); return;
    case 0x70: sc(w, "funcref");       return;
    case 0x6f: sc(w, "externref");     return;
    case 0x6e: sc(w, "anyref");        return;
    case 0x6d: sc(w, "eqref");         return;
    case 0x6c: sc(w, "i31ref");        return;
    case 0x6b: sc(w, "structref");     return;
    case 0x6a: sc(w, "arrayref");      return;
    case 0x71: sc(w, "nullref");       return;
    case 0x72: sc(w, "nullexternref"); return;
    case 0x73: sc(w, "nullfuncref");   return;
    case 0x69: sc(w, "exnref");        return;
    case 0x74: sc(w, "nullexnref");    return;
    default:
        if (getenv("WAT_TREE_VV")) fprintf(stderr, "wat_tree: reftype head 0x%02x\n", rt->head);
        fail(w, JAV_E_TYPE_MISMATCH);
        return;
    }
}

/* §6.4.5. */
static void sp_valtype(wb_t* w, const jav_val_type_t* vt) {
    switch (vt->head) {
    case 0x7f: sc(w, "i32");  return;
    case 0x7e: sc(w, "i64");  return;
    case 0x7d: sc(w, "f32");  return;
    case 0x7c: sc(w, "f64");  return;
    case 0x7b: sc(w, "v128"); return;
    default: {
        jav_ref_type_t rt;
        rt.head = vt->head;
        rt.ht.has_value = vt->ht.has_value;
        rt.ht.value = vt->ht.has_value ? vt->ht.value : (jav_heap_type_t){0};
        sp_reftype(w, &rt);
        return;
    }
    }
}

/* §6.4.6 storage types add the two packed forms. */
static void sp_storage(wb_t* w, const jav_storage_type_t* st) {
    if (st->head == 0x78) { sc(w, "i8");  return; }
    if (st->head == 0x77) { sc(w, "i16"); return; }
    jav_val_type_t vt;
    vt.head = st->head;
    vt.ht.has_value = st->ht.has_value;
    vt.ht.value = st->ht.has_value ? st->ht.value : (jav_heap_type_t){0};
    sp_valtype(w, &vt);
}

/* §6.4.9 / §6.4.8: the addrtype prefix only when it is not the i32 default. */
static void sp_limits(wb_t* w, const jav_limits_t* l) {
    if (l->flag & 0x04) sc(w, "i64 ");
    sc(w, "%" PRIu64, l->min);
    if (l->max.has_value) sc(w, " %" PRIu64, l->max.value);
}

/* §6.3.3 strings: printable ASCII stays itself, the named escapes for the
 * bytes that have one, \hh for the rest. */
static void sp_string(wb_t* w, const uint8_t* p, size_t n) {
    sc_raw(w, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        uint8_t c = p[i];
        if (c == '"')       sc_raw(w, "\\\"", 2);
        else if (c == '\\') sc_raw(w, "\\\\", 2);
        else if (c == '\t') sc_raw(w, "\\t", 2);
        else if (c == '\n') sc_raw(w, "\\n", 2);
        else if (c == '\r') sc_raw(w, "\\r", 2);
        else if (c >= 0x20 && c < 0x7f) sc_raw(w, (const char*)&c, 1);
        else sc(w, "\\%02x", c);
    }
    sc_raw(w, "\"", 1);
}
static void sp_name(wb_t* w, const jav_name_t* nm) {
    sp_string(w, nm->bytes.data, nm->bytes.length);
}

/* §6.3.2. Hex floats round-trip every bit pattern; the specials get their
 * §6.3.2 spellings, with the payload spelled only when it is not the
 * canonical quiet one. */
static void sp_f64(wb_t* w, double v) {
    uint64_t bits;
    memcpy(&bits, &v, 8);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) {
        if (bits & 0x8000000000000000ull) sc(w, "-");
        uint64_t m = bits & 0x000fffffffffffffull;
        if (m == 0) sc(w, "inf");
        else if (m == 0x0008000000000000ull) sc(w, "nan");
        else sc(w, "nan:0x%" PRIx64, m);
        return;
    }
    sc(w, "%a", v);
}
static void sp_f32(wb_t* w, float v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    if ((bits & 0x7f800000u) == 0x7f800000u) {
        if (bits & 0x80000000u) sc(w, "-");
        uint32_t m = bits & 0x007fffffu;
        if (m == 0) sc(w, "inf");
        else if (m == 0x00400000u) sc(w, "nan");
        else sc(w, "nan:0x%x", m);
        return;
    }
    sc(w, "%a", (double)v);
}

/* ── the type index: what a spelled reference to types[i] needs ─────────── */

static const jav_comp_type_t* member_comp(const jav_rec_member_t* m, jav_comp_type_t* tmp) {
    switch (m->head) {
    case 0x4f: tmp->head = m->body.u.case_0.body.head; return &m->body.u.case_0.body;
    case 0x50: return &m->body.u.case_1.body;
    case 0x5e: tmp->head = 0x5e; tmp->body.tag = 0; tmp->body.u.case_0 = m->body.u.case_2; return tmp;
    case 0x5f: tmp->head = 0x5f; tmp->body.tag = 1; tmp->body.u.case_1 = m->body.u.case_3; return tmp;
    case 0x60: tmp->head = 0x60; tmp->body.tag = 2; tmp->body.u.case_2 = m->body.u.case_4; return tmp;
    default: return NULL;
    }
}
static const jav_comp_type_t* rectype_comp(const jav_rec_type_t* r, jav_comp_type_t* tmp) {
    switch (r->head) {
    case 0x4f: return &r->body.u.case_1.body;
    case 0x50: return &r->body.u.case_2.body;
    case 0x5e: tmp->head = 0x5e; tmp->body.tag = 0; tmp->body.u.case_0 = r->body.u.case_3; return tmp;
    case 0x5f: tmp->head = 0x5f; tmp->body.tag = 1; tmp->body.u.case_1 = r->body.u.case_4; return tmp;
    case 0x60: tmp->head = 0x60; tmp->body.tag = 2; tmp->body.u.case_2 = r->body.u.case_5; return tmp;
    default: return NULL;
    }
}

static void index_one_comp(wb_t* w, uint32_t i, const jav_comp_type_t* c) {
    if (!c) { fail(w, JAV_E_TYPE_MISMATCH); return; }
    switch (c->head) {
    case 0x60: w->ftype[i] = &c->body.u.case_2; break;
    case 0x5f: w->nfields[i] = (uint32_t)c->body.u.case_1.fields.count; break;
    case 0x5e: w->nfields[i] = 1; break;
    default: break;
    }
}

static void build_type_index(wb_t* w) {
    /* Count first (a rec group is several indices), then fill. */
    const jav_type_section_t* ts = NULL;
    for (size_t i = 0; i < w->m->sections.count; i++)
        if (w->m->sections.items[i].id == 1) ts = &w->m->sections.items[i].body.u.case_1;
    uint32_t n = 0;
    if (ts)
        for (size_t i = 0; i < ts->types.count; i++) {
            const jav_rec_type_t* r = &ts->types.items[i];
            n += (r->head == 0x4e) ? (uint32_t)r->body.u.case_0.members.count : 1;
        }
    w->ntypes = n;
    w->ftype = bbq_arena_alloc(w->a, (n ? n : 1) * sizeof *w->ftype);
    w->nfields = bbq_arena_alloc(w->a, (n ? n : 1) * sizeof *w->nfields);
    memset(w->ftype, 0, (n ? n : 1) * sizeof *w->ftype);
    memset(w->nfields, 0, (n ? n : 1) * sizeof *w->nfields);
    if (!ts) return;
    uint32_t at = 0;
    for (size_t i = 0; i < ts->types.count; i++) {
        const jav_rec_type_t* r = &ts->types.items[i];
        if (r->head == 0x4e) {
            const jav_rec_group_t* g = &r->body.u.case_0;
            for (size_t k = 0; k < g->members.count; k++) {
                jav_comp_type_t* tmp = bbq_arena_alloc(w->a, sizeof *tmp);
                index_one_comp(w, at++, member_comp(&g->members.items[k], tmp));
            }
        } else {
            jav_comp_type_t* tmp = bbq_arena_alloc(w->a, sizeof *tmp);
            index_one_comp(w, at++, rectype_comp(r, tmp));
        }
    }
}

/* ── typeuse and comptype groups ────────────────────────────────────────── */

static void av_params_results(wb_t* w, const jav_func_type_t* ft,
                              uint32_t* av, uint32_t* nav) {
    *av = (uint32_t)bbq_vec_len(w->atoms);
    for (size_t i = 0; i < ft->params.count; i++) {
        sc(w, "(param ");
        sp_valtype(w, &ft->params.items[i]);
        sc(w, ")");
        atom_take(w);
    }
    for (size_t i = 0; i < ft->results.count; i++) {
        sc(w, "(result ");
        sp_valtype(w, &ft->results.items[i]);
        sc(w, ")");
        atom_take(w);
    }
    *nav = (uint32_t)bbq_vec_len(w->atoms) - *av;
}

/* §6.4.15: `(type x)` plus the echo, which is printed FROM the indexed type
 * so the section it expands into is the module's own. */
static wat_tnode_t* build_typeuse(wb_t* w, uint32_t typeidx) {
    wat_tnode_t* n = node_new(w, WAT_TT_W_TYPEUSE);
    sc(w, "(type %u)", typeidx);
    set_ptxt(w, n);
    if (typeidx < w->ntypes && w->ftype[typeidx])
        av_params_results(w, w->ftype[typeidx], &n->av, &n->nav);
    return n;
}

static wat_tnode_t* build_comptype(wb_t* w, const jav_comp_type_t* c) {
    switch (c->head) {
    case 0x60: {
        wat_tnode_t* n = node_new(w, WAT_TT_W_COMP_FUNC);
        av_params_results(w, &c->body.u.case_2, &n->av, &n->nav);
        return n;
    }
    case 0x5f: {
        wat_tnode_t* n = node_new(w, WAT_TT_W_COMP_STRUCT);
        const jav_struct_type_t* st = &c->body.u.case_1;
        n->av = (uint32_t)bbq_vec_len(w->atoms);
        for (size_t i = 0; i < st->fields.count; i++) {
            const jav_field_type_t* f = &st->fields.items[i];
            sc(w, "(field ");
            if (f->mut) sc(w, "(mut ");
            sp_storage(w, &f->storage);
            if (f->mut) sc(w, ")");
            sc(w, ")");
            atom_take(w);
        }
        n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
        return n;
    }
    case 0x5e: {
        wat_tnode_t* n = node_new(w, WAT_TT_W_COMP_ARRAY);
        const jav_field_type_t* f = &c->body.u.case_0.field;
        if (f->mut) sc(w, "(mut ");
        sp_storage(w, &f->storage);
        if (f->mut) sc(w, ")");
        set_ptxt(w, n);
        return n;
    }
    default:
        fail(w, JAV_E_TYPE_MISMATCH);
        return node_new(w, WAT_TT_W_COMP_FUNC);
    }
}

/* §6.4.7. `sub final` with no supertypes is elided (W_sub, the bare
 * comptype); anything else spells itself (W_subx). */
static wat_tnode_t* build_subtype(wb_t* w, int is_final, const uint32_t* supers,
                                  size_t nsupers, const jav_comp_type_t* c) {
    wat_tnode_t* comp = build_comptype(w, c);
    uint32_t ci = root_push(w, comp);
    wat_tnode_t* n;
    if (is_final && nsupers == 0) {
        n = node_new(w, WAT_TT_W_SUB);
    } else {
        n = node_new(w, WAT_TT_W_SUBX);
        if (is_final) sc(w, "final");
        for (size_t i = 0; i < nsupers; i++)
            sc(w, "%s%u", (i || is_final) ? " " : "", supers[i]);
        set_ptxt(w, n);
    }
    n->r1 = ci;
    n->nr1 = 1;
    return n;
}

static wat_tnode_t* build_type_entry(wb_t* w, const jav_rec_member_t* mem,
                                     const jav_rec_type_t* rec) {
    jav_comp_type_t* tmp = bbq_arena_alloc(w->a, sizeof *tmp);
    wat_tnode_t* sub = NULL;
    if (mem) {
        switch (mem->head) {
        case 0x4f: sub = build_subtype(w, 1, mem->body.u.case_0.supers.items,
                                       mem->body.u.case_0.supers.count, &mem->body.u.case_0.body); break;
        case 0x50: sub = build_subtype(w, 0, mem->body.u.case_1.supers.items,
                                       mem->body.u.case_1.supers.count, &mem->body.u.case_1.body); break;
        default:   sub = build_subtype(w, 1, NULL, 0, member_comp(mem, tmp)); break;
        }
    } else {
        switch (rec->head) {
        case 0x4f: sub = build_subtype(w, 1, rec->body.u.case_1.supers.items,
                                       rec->body.u.case_1.supers.count, &rec->body.u.case_1.body); break;
        case 0x50: sub = build_subtype(w, 0, rec->body.u.case_2.supers.items,
                                       rec->body.u.case_2.supers.count, &rec->body.u.case_2.body); break;
        default:   sub = build_subtype(w, 1, NULL, 0, rectype_comp(rec, tmp)); break;
        }
    }
    wat_tnode_t* n = node_new(w, WAT_TT_W_TYPE);
    n->r1 = root_push(w, sub);
    n->nr1 = 1;
    return n;
}

/* ── instructions ───────────────────────────────────────────────────────── */

static void tag_tables_init(wb_t* w) {
    for (int i = 0; i < 256; i++) w->tag0[i] = 0xffff;
    for (int i = 0; i < 64; i++)  w->tagfb[i] = w->tagfc[i] = 0xffff;
    for (int i = 0; i < 512; i++) w->tagfd[i] = 0xffff;
    for (int i = 0; i < WAT_N_INSTR; i++) {
        const wat_instr_tag_t* t = &wat_instr_tags[i];
        if (t->prefix == 0 && t->op < 256)        w->tag0[t->op] = t->tag;
        else if (t->prefix == 0xfb && t->op < 64)  w->tagfb[t->op] = t->tag;
        else if (t->prefix == 0xfc && t->op < 64)  w->tagfc[t->op] = t->tag;
        else if (t->prefix == 0xfd && t->op < 512) w->tagfd[t->op] = t->tag;
    }
}

static uint16_t tag_of(wb_t* w, const jav_instr_t* in) {
    uint32_t sub;
    switch (in->op) {
    case 0xfb: sub = in->body.u.case_29.sub; return sub < 64 ? w->tagfb[sub] : 0xffff;
    case 0xfc: sub = in->body.u.case_30.sub; return sub < 64 ? w->tagfc[sub] : 0xffff;
    case 0xfd: sub = in->body.u.case_31.sub; return sub < 512 ? w->tagfd[sub] : 0xffff;
    default:   return w->tag0[in->op];
    }
}

/* §6.5.3 blocktype: nothing when empty, a single `(result t)` for a valtype,
 * a full typeuse spelling for an index. */
static void sp_blocktype(wb_t* w, const jav_block_type_t* bt) {
    if (bt->bt == -64) return;                      /* 0x40: empty */
    if (bt->bt < 0) {
        jav_val_type_t vt;
        vt.head = (uint8_t)(bt->bt + 0x80);
        vt.ht.has_value = bt->ht.has_value;
        if (bt->ht.has_value) vt.ht.value = bt->ht.value;
        sc(w, "(result ");
        sp_valtype(w, &vt);
        sc(w, ")");
        return;
    }
    sc(w, "(type %" PRId64 ")", bt->bt);
    uint32_t ti = (uint32_t)bt->bt;
    if (ti < w->ntypes && w->ftype[ti]) {
        const jav_func_type_t* ft = w->ftype[ti];
        for (size_t i = 0; i < ft->params.count; i++) {
            sc(w, " (param ");
            sp_valtype(w, &ft->params.items[i]);
            sc(w, ")");
        }
        for (size_t i = 0; i < ft->results.count; i++) {
            sc(w, " (result ");
            sp_valtype(w, &ft->results.items[i]);
            sc(w, ")");
        }
    }
}

/* §6.5.6 memarg: memidx when flagged, offset= and align= only off their
 * defaults. The align field is the RAW §5.4.5 byte; bit 6 is the memidx
 * flag, the low six bits the exponent, and the default is the instruction's
 * natural alignment from the mnemonic table. */
static void sp_memarg(wb_t* w, const jav_mem_arg_t* ma, const wat_mnemonic_t* mn) {
    if (ma->memidx.has_value && ma->memidx.value != 0) sc(w, "%u", ma->memidx.value);
    if (ma->offset) sc(w, "%soffset=%" PRIu64, bbq_vec_len(w->sc) ? " " : "", ma->offset);
    uint32_t align = ma->align & 0x3f;
    if ((int)align != mn->align)
        sc(w, "%salign=%u", bbq_vec_len(w->sc) ? " " : "", 1u << align);
}

static const wat_mnemonic_t* mn_of(const jav_instr_t* in) {
    uint32_t prefix = 0, op = in->op;
    if (in->op == 0xfb) { prefix = 0xfb; op = in->body.u.case_29.sub; }
    else if (in->op == 0xfc) { prefix = 0xfc; op = in->body.u.case_30.sub; }
    else if (in->op == 0xfd) { prefix = 0xfd; op = in->body.u.case_31.sub; }
    for (size_t i = 0; i < sizeof wat_mnemonics / sizeof wat_mnemonics[0]; i++)
        if (wat_mnemonics[i].prefix == prefix && wat_mnemonics[i].op == op)
            return &wat_mnemonics[i];
    return NULL;
}

static wat_tnode_t** build_seq(wb_t* w, const jav_instr_t* items, size_t count, int depth);

/* The one instruction group: immediates spelled into ptxt/atoms, bodies
 * recursed into r1/r2, the operand spine left empty for build_seq to fill. */
static wat_tnode_t* build_instr(wb_t* w, const jav_instr_t* in, int depth) {
    uint16_t tag = tag_of(w, in);
    if (tag == 0xffff) { fail(w, JAV_E_TYPE_MISMATCH); tag = WAT_TAG_I_FIRST; }
    wat_tnode_t* n = node_new(w, tag);
    n->nkids = 1;
    n->kids[0] = node_new(w, WAT_TAG_OP_NIL);

    /* The union's tag is the raw discriminant (op, or the prefix's sub), and
     * every union member of one C type aliases at offset 0 — so the SHAPE
     * column of the mnemonic table picks the member type, and the op picks
     * the §6 text order where the two differ. */
    const wat_mnemonic_t* mn = mn_of(in);
    if (!mn) { fail(w, JAV_E_TYPE_MISMATCH); return n; }
    switch (mn->shape) {
    case WSH_BLOCK: {   /* block / loop */
        const jav_block_t* b = &in->body.u.case_1;
        sp_blocktype(w, &b->bt);
        set_ptxt(w, n);
        wat_tnode_t** body = build_seq(w, b->instrs.items, b->instrs.count, depth + 1);
        span_commit(w, body, &n->r1, &n->nr1);
        bbq_vec_free(body);
        break;
    }
    case WSH_IF: {
        const jav_if_t* b = &in->body.u.case_2;
        sp_blocktype(w, &b->bt);
        set_ptxt(w, n);
        wat_tnode_t** thenb = build_seq(w, b->then_body.items, b->then_body.count, depth + 1);
        span_commit(w, thenb, &n->r1, &n->nr1);
        bbq_vec_free(thenb);
        if (b->else_body.has_value) {
            wat_tnode_t** elseb = build_seq(w, b->else_body.value.instrs.items,
                                            b->else_body.value.instrs.count, depth + 1);
            span_commit(w, elseb, &n->r2, &n->nr2);
            bbq_vec_free(elseb);
        }
        break;
    }
    case WSH_TRYTABLE: {
        const jav_try_table_t* b = &in->body.u.case_15;
        sp_blocktype(w, &b->bt);
        set_ptxt(w, n);
        n->av = (uint32_t)bbq_vec_len(w->atoms);
        for (size_t i = 0; i < b->catches.count; i++) {
            const jav_catch_t* c = &b->catches.items[i];
            switch (c->kind) {
            case 0: sc(w, "(catch %u %u)", c->tag.value, c->label); break;
            case 1: sc(w, "(catch_ref %u %u)", c->tag.value, c->label); break;
            case 2: sc(w, "(catch_all %u)", c->label); break;
            default: sc(w, "(catch_all_ref %u)", c->label); break;
            }
            atom_take(w);
        }
        n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
        wat_tnode_t** body = build_seq(w, b->instrs.items, b->instrs.count, depth + 1);
        span_commit(w, body, &n->r1, &n->nr1);
        bbq_vec_free(body);
        break;
    }
    case WSH_IDX: {
        uint32_t x = (in->op == 0xfb) ? in->body.u.case_29.body.u.case_0.x
                   : (in->op == 0xfc) ? in->body.u.case_30.body.u.case_2.x
                                      : in->body.u.case_3.x;
        /* §6.5.5/§6.5.6: table and memory indices default to 0 and are
         * omitted there; every other index space always spells itself. */
        if (!(x == 0 && (mn->sp0 == SP_TABLE || mn->sp0 == SP_MEM)))
            sc(w, "%u", x);
        set_ptxt(w, n);
        break;
    }
    case WSH_IDX2: {
        const jav_idx2_imm_t* im = (in->op == 0xfb) ? &in->body.u.case_29.body.u.case_1
                                 : (in->op == 0xfc) ? &in->body.u.case_30.body.u.case_1
                                                    : &in->body.u.case_9;
        if (in->op == 0x11 || in->op == 0x13) {
            /* §6.5.3: `call_indirect tableidx? typeuse`, table omitted at 0. */
            if (im->y) sc(w, "%u ", im->y);
            sc(w, "(type %u)", im->x);
        } else if (in->op == 0xfc && (in->body.u.case_30.sub == 8 ||
                                      in->body.u.case_30.sub == 12)) {
            /* §6.5.5/§6.5.6: memory.init / table.init spell the destination
             * first and omit it at 0; the binary order is (segment, dest). */
            if (im->y) sc(w, "%u ", im->y);
            sc(w, "%u", im->x);
        } else if (in->op == 0xfc && (in->body.u.case_30.sub == 10 ||
                                      in->body.u.case_30.sub == 14)) {
            /* memory.copy / table.copy: both indices or neither. */
            if (im->x || im->y) sc(w, "%u %u", im->x, im->y);
        } else {
            sc(w, "%u %u", im->x, im->y);
        }
        set_ptxt(w, n);
        break;
    }
    case WSH_BRTABLE: {   /* a fill list of labels, default included */
        const jav_br_table_t* bt = &in->body.u.case_6;
        n->av = (uint32_t)bbq_vec_len(w->atoms);
        for (size_t i = 0; i < bt->targets.count; i++) {
            sc(w, "%u", bt->targets.items[i]);
            atom_take(w);
        }
        sc(w, "%u", bt->default_target);
        atom_take(w);
        n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
        break;
    }
    case WSH_SELECTT: {  /* select (result t*) */
        const jav_select_t_t* st = &in->body.u.case_14;
        for (size_t i = 0; i < st->types.count; i++) {
            if (i) sc(w, " ");
            sc(w, "(result ");
            sp_valtype(w, &st->types.items[i]);
            sc(w, ")");
        }
        set_ptxt(w, n);
        break;
    }
    case WSH_MEMARG: {
        const jav_mem_arg_t* ma = (in->op == 0xfd) ? &in->body.u.case_31.body.u.case_0
                                                   : &in->body.u.case_17;
        sp_memarg(w, ma, mn);
        set_ptxt(w, n);
        break;
    }
    case WSH_I32: sc(w, "%" PRId32, in->body.u.case_19.v); set_ptxt(w, n); break;
    case WSH_I64: sc(w, "%" PRId64, in->body.u.case_20.v); set_ptxt(w, n); break;
    case WSH_F32: sp_f32(w, in->body.u.case_21.v); set_ptxt(w, n); break;
    case WSH_F64: sp_f64(w, in->body.u.case_22.v); set_ptxt(w, n); break;
    case WSH_HEAP: {
        if (in->op == 0xfb) {   /* ref.test / ref.cast: nullness is the odd sub */
            const jav_gc_instr_t* g = &in->body.u.case_29;
            sc(w, (g->sub & 1) ? "(ref null " : "(ref ");
            sp_heap(w, g->body.u.case_8.ht);
            sc(w, ")");
        } else {
            sp_heap(w, in->body.u.case_24.ht);
        }
        set_ptxt(w, n);
        break;
    }
    case WSH_BRONCAST: {   /* br_on_cast / br_on_cast_fail */
        const jav_br_on_cast_t* c = &in->body.u.case_29.body.u.case_9;
        sc(w, "%u ", c->label);
        sc(w, (c->flags & 1) ? "(ref null " : "(ref ");
        sp_heap(w, c->ht1);
        sc(w, ") ");
        sc(w, (c->flags & 2) ? "(ref null " : "(ref ");
        sp_heap(w, c->ht2);
        sc(w, ")");
        set_ptxt(w, n);
        break;
    }
    case WSH_V128: {   /* both 16-byte immediates share the shape (the toml's
                        * v128 rows): v128.const spells four hex lanes read LE,
                        * i8x16.shuffle (sub 13) spells sixteen fill atoms. */
        const jav_simd_instr_t* si = &in->body.u.case_31;
        const uint8_t* b = si->body.u.case_1.bytes.data;
        if (si->sub == 13) {
            n->av = (uint32_t)bbq_vec_len(w->atoms);
            for (int k = 0; k < 16; k++) {
                sc(w, "%u", b[k]);
                atom_take(w);
            }
            n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
        } else {
            sc(w, "i32x4");
            for (int k = 0; k < 4; k++) {
                uint32_t lane = (uint32_t)b[4 * k] | ((uint32_t)b[4 * k + 1] << 8) |
                                ((uint32_t)b[4 * k + 2] << 16) | ((uint32_t)b[4 * k + 3] << 24);
                sc(w, " 0x%08x", lane);
            }
            set_ptxt(w, n);
        }
        break;
    }
    case WSH_LANE:
        sc(w, "%u", in->body.u.case_31.body.u.case_3.lane);
        set_ptxt(w, n);
        break;
    case WSH_MEMLANE: {
        const jav_mem_lane_imm_t* ml = &in->body.u.case_31.body.u.case_5;
        sp_memarg(w, &ml->mem, mn);
        sc(w, "%s%u", bbq_vec_len(w->sc) ? " " : "", ml->lane);
        set_ptxt(w, n);
        break;
    }
    default:
        break;   /* WSH_NONE */
    }
    return n;
}

/* Chain the last `f` available groups into a spine, leftmost operand first. */
static void attach_spine(wb_t* w, wat_tnode_t* n, wat_tnode_t** avail, uint32_t f) {
    size_t len = bbq_vec_len(avail);
    wat_tnode_t* chain = n->kids[0];   /* the nil */
    for (size_t j = 0; j < f; j++) {
        wat_tnode_t* cons = node_new(w, WAT_TAG_OP_CONS);
        cons->nkids = 2;
        cons->kids[0] = avail[len - 1 - j];
        cons->kids[1] = chain;
        chain = cons;
    }
    n->kids[0] = chain;
}

/* One instruction sequence → its statement groups, folding per the rows. */
static wat_tnode_t** build_seq(wb_t* w, const jav_instr_t* items, size_t count, int depth) {
    wat_tnode_t** avail = NULL;
    for (size_t i = 0; i < count && !w->failed; i++) {
        const jav_instr_t* in = &items[i];
        wat_tnode_t* n = build_instr(w, in, depth);
        const wat_info_t* row = w->rows ? wat_info(w->rows, in) : NULL;
        uint32_t f = row ? row->fold : 0;
        if (f > bbq_vec_len(avail)) f = (uint32_t)bbq_vec_len(avail);
        if (depth + (int)f > WAT_FOLD_CAP) f = 0;   /* hoist past the cap */
        attach_spine(w, n, avail, f);
        for (uint32_t j = 0; j < f; j++) bbq_vec_pop(avail);
        bbq_vec_push(avail, n);
    }
    return avail;
}

/* A constant expression (§3.4's set): straight-line producers, so every
 * instruction's operands are the immediately preceding trees and the fold is
 * its full arity. Anything outside the set cannot appear in a validated
 * module, so hitting one is builder-visible breakage, not a rendering path. */
static uint32_t const_arity(wb_t* w, const jav_instr_t* in) {
    switch (in->op) {
    case 0x41: case 0x42: case 0x43: case 0x44:   /* consts */
    case 0x23: case 0xd0: case 0xd2:              /* global.get, ref.null, ref.func */
        return 0;
    case 0x6a: case 0x6b: case 0x6c:              /* i32 add/sub/mul */
    case 0x7c: case 0x7d: case 0x7e:              /* i64 add/sub/mul */
        return 2;
    case 0xfd:                                    /* v128.const */
        return 0;
    case 0xfb: {
        const jav_gc_instr_t* g = &in->body.u.case_29;
        switch (g->sub) {
        case 0:  return g->body.u.case_0.x < w->ntypes ? w->nfields[g->body.u.case_0.x] : 0;
        case 1:  return 0;                        /* struct.new_default */
        case 6:  return 2;                        /* array.new: init + length */
        case 7:  return 1;                        /* array.new_default: length */
        case 8:  return in->body.u.case_29.body.u.case_3.y;   /* array.new_fixed x n */
        case 28: return 1;                        /* ref.i31 */
        case 26: case 27: return 1;               /* any/extern.convert */
        default: fail(w, JAV_E_CONST_EXPR_REQUIRED); return 0;
        }
    }
    default:
        fail(w, JAV_E_CONST_EXPR_REQUIRED);
        return 0;
    }
}

static wat_tnode_t** build_const_seq(wb_t* w, const jav_expr_t* e) {
    wat_tnode_t** avail = NULL;
    for (size_t i = 0; i < e->instrs.count && !w->failed; i++) {
        const jav_instr_t* in = &e->instrs.items[i];
        wat_tnode_t* n = build_instr(w, in, 0);
        uint32_t f = const_arity(w, in);
        if (f > bbq_vec_len(avail)) f = (uint32_t)bbq_vec_len(avail);
        attach_spine(w, n, avail, f);
        for (uint32_t j = 0; j < f; j++) bbq_vec_pop(avail);
        bbq_vec_push(avail, n);
    }
    return avail;
}

static void commit_const_expr(wb_t* w, const jav_expr_t* e, uint32_t* off, uint32_t* n) {
    wat_tnode_t** seq = build_const_seq(w, e);
    span_commit(w, seq, off, n);
    bbq_vec_free(seq);
}

/* ── declarations ───────────────────────────────────────────────────────── */

static void decl_push(wb_t* w, wat_tnode_t* n) {
    bbq_vec_push(w->decls, root_push(w, n));
}

static void build_import(wb_t* w, const jav_import_t* imp) {
    wat_tnode_t* n = node_new(w, WAT_TT_W_IMPORT);
    sp_name(w, &imp->module);
    sc(w, " ");
    sp_name(w, &imp->field);
    set_ptxt(w, n);
    wat_tnode_t* d;
    switch (imp->desc.kind) {
    case 0x00:
        d = node_new(w, WAT_TT_W_ED_FUNC);
        d->r1 = root_push(w, build_typeuse(w, imp->desc.body.u.case_0.x));
        d->nr1 = 1;
        break;
    case 0x01: {
        d = node_new(w, WAT_TT_W_ED_TABLE);
        const jav_table_type_t* tt = &imp->desc.body.u.case_1;
        sp_limits(w, &tt->limits);
        sc(w, " ");
        sp_reftype(w, &tt->reftype);
        set_ptxt(w, d);
        break;
    }
    case 0x02:
        d = node_new(w, WAT_TT_W_ED_MEM);
        sp_limits(w, &imp->desc.body.u.case_2);
        set_ptxt(w, d);
        break;
    case 0x03: {
        d = node_new(w, WAT_TT_W_ED_GLOBAL);
        const jav_global_type_t* gt = &imp->desc.body.u.case_3;
        if (gt->mut) sc(w, "(mut ");
        sp_valtype(w, &gt->type);
        if (gt->mut) sc(w, ")");
        set_ptxt(w, d);
        break;
    }
    default:
        d = node_new(w, WAT_TT_W_ED_TAG);
        d->r1 = root_push(w, build_typeuse(w, imp->desc.body.u.case_4.type));
        d->nr1 = 1;
        break;
    }
    n->r1 = root_push(w, d);
    n->nr1 = 1;
    decl_push(w, n);
}

static uint32_t imported_funcs(wb_t* w) {
    uint32_t n = 0;
    for (size_t i = 0; i < w->m->sections.count; i++) {
        const jav_section_t* s = &w->m->sections.items[i];
        if (s->id != 2) continue;
        const jav_import_section_t* is = &s->body.u.case_2;
        for (size_t k = 0; k < is->imports.count; k++)
            if (is->imports.items[k].desc.kind == 0x00) n++;
    }
    return n;
}

static void build_funcs(wb_t* w, const jav_function_section_t* fs) {
    const jav_code_section_t* cs = NULL;
    for (size_t i = 0; i < w->m->sections.count; i++)
        if (w->m->sections.items[i].id == 10)
            cs = &w->m->sections.items[i].body.u.case_10;
    if (fs->type_indices.count == 0 && !cs) return;   /* both absent-or-empty: legal */
    if (!cs || cs->entries.count != fs->type_indices.count) {
        fail(w, JAV_E_FUNC_CODE_LENGTHS);
        return;
    }
    uint32_t base = imported_funcs(w);
    for (size_t k = 0; k < fs->type_indices.count && !w->failed; k++) {
        const jav_func_body_t* body = &cs->entries.items[k].body;
        wat_tnode_t* n = node_new(w, WAT_TT_W_FUNC);
        n->r2 = root_push(w, build_typeuse(w, fs->type_indices.items[k]));
        n->nr2 = 1;
        /* §6.6.7: one (local …) per RLE run, so the reassembled runs are the
         * module's own. */
        n->av = (uint32_t)bbq_vec_len(w->atoms);
        for (size_t L = 0; L < body->locals.count; L++) {
            const jav_locals_t* loc = &body->locals.items[L];
            sc(w, "(local");
            for (uint32_t r = 0; r < loc->count; r++) {
                sc(w, " ");
                sp_valtype(w, &loc->type);
            }
            sc(w, ")");
            atom_take(w);
        }
        n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
        /* The body: §7.6's rows drive the folding. */
        wat_body_t rows;
        if (!wat_check_body(w->cx, base + (uint32_t)k, body, w->a, &rows)) {
            if (getenv("WAT_TREE_VV"))
                fprintf(stderr, "wat_tree: func %u refused op 0x%02x (err %d)\n",
                        base + (uint32_t)k, rows.fail ? rows.fail->op : 0,
                        (int)rows.err);
            fail(w, rows.err);
            return;
        }
        w->rows = &rows;
        wat_tnode_t** stmts = build_seq(w, body->body.instrs.items, body->body.instrs.count, 0);
        w->rows = NULL;
        span_commit(w, stmts, &n->r1, &n->nr1);
        bbq_vec_free(stmts);
        decl_push(w, n);
    }
}

/* §7.7.3: a custom section rides an @custom annotation whose place names the
 * neighbouring section. `tag` is not in the place vocabulary, so a custom
 * section beside it anchors on the section AFTER it instead. */
static const char* secname(uint8_t id) {
    switch (id) {
    case 1: return "type";   case 2: return "import"; case 3: return "func";
    case 4: return "table";  case 5: return "memory"; case 6: return "global";
    case 7: return "export"; case 8: return "start";  case 9: return "elem";
    case 10: return "code";  case 11: return "data";  case 12: return "datacount";
    default: return NULL;
    }
}

static void build_custom(wb_t* w, const jav_custom_section_t* cs, size_t at) {
    wat_tnode_t* n = node_new(w, WAT_TT_W_CUSTOM);
    sp_name(w, &cs->name);
    const char* anchor = NULL;
    int before = 0;
    for (size_t i = at; i-- > 0 && !anchor;)
        anchor = secname(w->m->sections.items[i].id);
    if (!anchor) {
        before = 1;
        for (size_t i = at + 1; i < w->m->sections.count && !anchor; i++)
            anchor = secname(w->m->sections.items[i].id);
    }
    if (anchor) sc(w, " (%s %s)", before ? "before" : "after", anchor);
    else        sc(w, " (before first)");
    set_ptxt(w, n);
    n->av = (uint32_t)bbq_vec_len(w->atoms);
    av_bytes(w, cs->data.data, cs->data.length);
    n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
    decl_push(w, n);
}

static void av_push_str(wb_t* w, const char* s) {
    sc(w, "%s", s);
    atom_take(w);
}

/* §6.6.8/§7.7.3 datastrings are string*, so the byte payload splits into as
 * many string pieces as keep each ESCAPED piece narrow enough to lay out —
 * chunking by raw bytes let a run of \hh escapes quadruple past the width. */
static void av_bytes(wb_t* w, const uint8_t* p, size_t len) {
    size_t at = 0;
    do {
        size_t end = at, ew = 0;
        while (end < len && ew < 64) {
            uint8_t c = p[end];
            ew += (c == '"' || c == '\\' || c == '\t' || c == '\n' || c == '\r') ? 2
                : (c >= 0x20 && c < 0x7f) ? 1 : 3;
            end++;
        }
        sp_string(w, p + at, end - at);
        atom_take(w);
        at = end;
    } while (at < len);
}

static void build_elem(wb_t* w, const jav_elem_t* e) {
    wat_tnode_t* n = node_new(w, WAT_TT_W_ELEM);
    const jav_idx_vec_t* funcs = NULL;
    const jav_expr_vec_t* exprs = NULL;
    const jav_expr_t* off = NULL;
    const jav_ref_type_t* rt = NULL;
    const char* kind = "func";
    switch (e->flag) {
    case 0: off = &e->body.u.case_0.offset; funcs = &e->body.u.case_0.funcs; break;
    case 1: funcs = &e->body.u.case_1.funcs; break;
    case 2: sc(w, "(table %u)", e->body.u.case_2.table);
            off = &e->body.u.case_2.offset; funcs = &e->body.u.case_2.funcs; break;
    case 3: sc(w, "declare"); funcs = &e->body.u.case_3.funcs; break;
    case 4: off = &e->body.u.case_4.offset; exprs = &e->body.u.case_4.exprs; break;
    case 5: rt = &e->body.u.case_5.reftype; exprs = &e->body.u.case_5.exprs; break;
    case 6: sc(w, "(table %u)", e->body.u.case_6.table);
            off = &e->body.u.case_6.offset; rt = &e->body.u.case_6.reftype;
            exprs = &e->body.u.case_6.exprs; break;
    default: sc(w, "declare"); rt = &e->body.u.case_7.reftype;
             exprs = &e->body.u.case_7.exprs; break;
    }
    set_ptxt(w, n);
    if (off) commit_const_expr(w, off, &n->r1, &n->nr1);
    n->av = (uint32_t)bbq_vec_len(w->atoms);
    if (funcs) {
        av_push_str(w, kind);
        for (size_t i = 0; i < funcs->idxs.count; i++) {
            sc(w, "%u", funcs->idxs.items[i]);
            atom_take(w);
        }
    } else if (rt) {
        sp_reftype(w, rt);
        atom_take(w);
    } else {
        av_push_str(w, "funcref");   /* flag 4: implicit funcref, expr items */
    }
    n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
    if (exprs) {
        wat_tnode_t** items = NULL;
        for (size_t i = 0; i < exprs->exprs.count && !w->failed; i++) {
            wat_tnode_t** one = build_const_seq(w, &exprs->exprs.items[i]);
            if (bbq_vec_len(one) != 1) {
                if (getenv("WAT_TREE_VV"))
                    fprintf(stderr, "wat_tree: elem item %zu made %zu roots, %zu instrs, op0 0x%02x\n",
                            i, (size_t)bbq_vec_len(one), exprs->exprs.items[i].instrs.count,
                            exprs->exprs.items[i].instrs.count
                                ? exprs->exprs.items[i].instrs.items[0].op : 0);
                fail(w, JAV_E_CONST_EXPR_REQUIRED);
            }
            if (bbq_vec_len(one)) bbq_vec_push(items, one[0]);
            bbq_vec_free(one);
        }
        span_commit(w, items, &n->r2, &n->nr2);
        bbq_vec_free(items);
    }
    decl_push(w, n);
}

static void build_data(wb_t* w, const jav_data_t* d) {
    wat_tnode_t* n = node_new(w, WAT_TT_W_DATA);
    const jav_byte_vec_t* bytes = NULL;
    const jav_expr_t* off = NULL;
    switch (d->flag) {
    case 0: off = &d->body.u.case_0.offset; bytes = &d->body.u.case_0.data; break;
    case 1: bytes = &d->body.u.case_1.data; break;
    default:
        sc(w, "(memory %u)", d->body.u.case_2.memidx);
        off = &d->body.u.case_2.offset;
        bytes = &d->body.u.case_2.data;
        break;
    }
    set_ptxt(w, n);
    if (off) commit_const_expr(w, off, &n->r1, &n->nr1);
    n->av = (uint32_t)bbq_vec_len(w->atoms);
    av_bytes(w, bytes->bytes.data, bytes->bytes.length);
    n->nav = (uint32_t)bbq_vec_len(w->atoms) - n->av;
    decl_push(w, n);
}

/* ── the module walk ────────────────────────────────────────────────────── */

int wat_tree_build(const jav_module_t* m, const wat_check_ctx_t* cx,
                   bbq_arena* a, wat_forest_t* out, jav_err_t* err) {
    wb_t w;
    memset(&w, 0, sizeof w);
    w.m = m;
    w.cx = cx;
    w.a = a;
    tag_tables_init(&w);
    build_type_index(&w);

    for (size_t si = 0; si < m->sections.count && !w.failed; si++) {
        const jav_section_t* s = &m->sections.items[si];
        switch (s->id) {
        case 0:
            build_custom(&w, &s->body.u.case_0, si);
            break;
        case 1: {
            const jav_type_section_t* ts = &s->body.u.case_1;
            for (size_t i = 0; i < ts->types.count; i++) {
                const jav_rec_type_t* r = &ts->types.items[i];
                if (r->head == 0x4e) {
                    const jav_rec_group_t* g = &r->body.u.case_0;
                    wat_tnode_t** members = NULL;
                    for (size_t k = 0; k < g->members.count; k++)
                        bbq_vec_push(members, build_type_entry(&w, &g->members.items[k], NULL));
                    wat_tnode_t* n = node_new(&w, WAT_TT_W_REC);
                    span_commit(&w, members, &n->r1, &n->nr1);
                    bbq_vec_free(members);
                    decl_push(&w, n);
                } else {
                    decl_push(&w, build_type_entry(&w, NULL, r));
                }
            }
            break;
        }
        case 2: {
            const jav_import_section_t* is = &s->body.u.case_2;
            for (size_t i = 0; i < is->imports.count; i++)
                build_import(&w, &is->imports.items[i]);
            break;
        }
        case 3:
            build_funcs(&w, &s->body.u.case_3);
            break;
        case 4: {
            const jav_table_section_t* ts = &s->body.u.case_4;
            for (size_t i = 0; i < ts->tables.count; i++) {
                const jav_table_t* t = &ts->tables.items[i];
                wat_tnode_t* n = node_new(&w, WAT_TT_W_TABLE);
                /* §5.5.9: the explicit-init form is marked 0x40 0x00 and the
                 * discriminant carries that marker byte. */
                const jav_table_type_t* tt =
                    (t->tag == 0x40) ? &t->u.case_0.type : &t->u.default_val.type;
                sp_limits(&w, &tt->limits);
                sc(&w, " ");
                sp_reftype(&w, &tt->reftype);
                set_ptxt(&w, n);
                if (t->tag == 0x40)
                    commit_const_expr(&w, &t->u.case_0.init, &n->r1, &n->nr1);
                decl_push(&w, n);
            }
            break;
        }
        case 5: {
            const jav_memory_section_t* ms = &s->body.u.case_5;
            for (size_t i = 0; i < ms->mems.count; i++) {
                wat_tnode_t* n = node_new(&w, WAT_TT_W_MEMORY);
                sp_limits(&w, &ms->mems.items[i].limits);
                set_ptxt(&w, n);
                decl_push(&w, n);
            }
            break;
        }
        case 13: {
            const jav_tag_section_t* gs = &s->body.u.case_13;
            for (size_t i = 0; i < gs->tags.count; i++) {
                wat_tnode_t* n = node_new(&w, WAT_TT_W_TAG);
                n->r1 = root_push(&w, build_typeuse(&w, gs->tags.items[i].type));
                n->nr1 = 1;
                decl_push(&w, n);
            }
            break;
        }
        case 6: {
            const jav_global_section_t* gs = &s->body.u.case_6;
            for (size_t i = 0; i < gs->globals.count; i++) {
                const jav_global_t* g = &gs->globals.items[i];
                wat_tnode_t* n = node_new(&w, WAT_TT_W_GLOBAL);
                if (g->type.mut) sc(&w, "(mut ");
                sp_valtype(&w, &g->type.type);
                if (g->type.mut) sc(&w, ")");
                set_ptxt(&w, n);
                commit_const_expr(&w, &g->init, &n->r1, &n->nr1);
                decl_push(&w, n);
            }
            break;
        }
        case 7: {
            const jav_export_section_t* es = &s->body.u.case_7;
            static const char* const kindname[] = { "func", "table", "memory", "global", "tag" };
            for (size_t i = 0; i < es->exports.count; i++) {
                const jav_export_t* e = &es->exports.items[i];
                wat_tnode_t* n = node_new(&w, WAT_TT_W_EXPORT);
                sp_name(&w, &e->name);
                sc(&w, " (%s %u)", e->kind <= 4 ? kindname[e->kind] : "?", e->idx);
                set_ptxt(&w, n);
                decl_push(&w, n);
            }
            break;
        }
        case 8: {
            wat_tnode_t* n = node_new(&w, WAT_TT_W_START);
            sc(&w, "%u", s->body.u.case_8.func);
            set_ptxt(&w, n);
            decl_push(&w, n);
            break;
        }
        case 9: {
            const jav_element_section_t* es = &s->body.u.case_9;
            for (size_t i = 0; i < es->elems.count && !w.failed; i++)
                build_elem(&w, &es->elems.items[i]);
            break;
        }
        case 11: {
            const jav_data_section_t* ds = &s->body.u.case_11;
            for (size_t i = 0; i < ds->datas.count && !w.failed; i++)
                build_data(&w, &ds->datas.items[i]);
            break;
        }
        default:
            break;   /* function/code pairing is driven from id 3; 12 has no text */
        }
    }

    if (w.failed) {
        bbq_vec_free(w.pool); bbq_vec_free(w.atoms);
        bbq_vec_free(w.roots); bbq_vec_free(w.decls); bbq_vec_free(w.sc);
        if (err) *err = w.err;
        return 0;
    }

    /* Freeze the vecs into the caller's arena. */
    out->nroots = (uint32_t)bbq_vec_len(w.roots);
    out->ndecls = (uint32_t)bbq_vec_len(w.decls);
    out->natoms = (uint32_t)bbq_vec_len(w.atoms);
    wat_tnode_t** roots = bbq_arena_alloc(a, (out->nroots ? out->nroots : 1) * sizeof *roots);
    memcpy(roots, w.roots, out->nroots * sizeof *roots);
    uint32_t* decls = bbq_arena_alloc(a, (out->ndecls ? out->ndecls : 1) * sizeof *decls);
    memcpy(decls, w.decls, out->ndecls * sizeof *decls);
    wat_atom_t* atoms = bbq_arena_alloc(a, (out->natoms ? out->natoms : 1) * sizeof *atoms);
    memcpy(atoms, w.atoms, out->natoms * sizeof *atoms);
    char* pool = bbq_arena_alloc(a, bbq_vec_len(w.pool) + 1);
    memcpy(pool, w.pool, bbq_vec_len(w.pool));
    pool[bbq_vec_len(w.pool)] = 0;
    out->roots = roots;
    out->decls = decls;
    out->atoms = atoms;
    out->pool = pool;
    bbq_vec_free(w.pool); bbq_vec_free(w.atoms);
    bbq_vec_free(w.roots); bbq_vec_free(w.decls); bbq_vec_free(w.sc);
    return 1;
}
