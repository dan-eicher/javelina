/*
 * wat_check.c — WASM 3.0 §7.6 "Validation Algorithm", transcribed.
 *
 * §7.6: "This section sketches the skeleton of a sound and complete algorithm for
 * effectively validating code … the algorithm is expressed over the flat sequence of
 * opcodes as occurring in the binary format, and performs only a single pass over it.
 * Consequently, it can be integrated directly into a decoder. The algorithm is
 * expressed in typed pseudo code whose semantics is intended to be self-explanatory."
 *
 * So this file is a transcription and not a design. Every function §7.6.1 prints has
 * a function of the same name below, in the order the spec prints them, and §7.6.2's
 * switch is the switch. Where §7.6.2 says "Other instructions are checked in a
 * similar manner", the transfer comes from opgen's generated jav_opsig[] — the same
 * per-opcode signature table the engine's own checker reads, generated from
 * spec/wasm.def, so the two cannot disagree about an instruction's arity or operand
 * types unless the generator changes under both.
 *
 * The one difference from the printed algorithm is its input. The spec walks "the
 * flat sequence of opcodes"; here the opcodes are already decoded — wasm.bbq's Expr
 * is `array<Instr>(none, until(peek() == 0x0B))` and the owning reader built the
 * tree, with block/loop/if/try_table carrying their bodies as nested arrays. So the
 * walk recurses over jav_instr_t instead of advancing a cursor, and nothing here
 * re-decodes a LEB. The `end` and `else` delimiters are fields of those structs
 * rather than instructions, so their cases are reached structurally.
 *
 * The SECOND product of the one walk is the producer edge per operand — see
 * wat_check.h. It costs one field on the value stack: push_val records the
 * instruction, pop_val hands it back with the type it checked.
 */
#include "wat_check.h"

#include "bbq_hmap.h"
#include "bbq_vec.h"
#include "jav_subtype.h"
#include "jav_type_meta.h"   /* jav_opsig[] / jav_opsig_sub[] — opgen's transfer table */
#include "wat_mnemonics.h"   /* §3.4.5's natural alignment, from instructions.toml */

#include <stdlib.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 * §7.6.1 Data Structures — Types
 * ════════════════════════════════════════════════════════════════════════════ */

/*   type val_type = num_type | vec_type | ref_type | Bot
 *
 * A number and a vector are their WVT_ tag; a reference is (null, heap) with the
 * heap type an HT_ abstract code or a concrete typeidx; Bot is its own tag. */
enum { VT_K_REF = -1, VT_K_BOT = -2 };
typedef struct { int16_t num; uint8_t null; int32_t heap; } val_type;

#define VT_BOT        ((val_type){ VT_K_BOT, 0, 0 })
#define VT_NUM(w)     ((val_type){ (int16_t)(w), 0, 0 })
#define VT_REF(n, h)  ((val_type){ VT_K_REF, (uint8_t)(n), (int32_t)(h) })

/*   func is_num(t) = t = I32 || t = I64 || t = F32 || t = F64 || t = Bot
 *   func is_vec(t) = t = V128 || t = Bot
 *   func is_ref(t) = not (is_num t || is_vec t) || t = Bot                     */
static int is_num(val_type t) {
    return t.num == WVT_I32 || t.num == WVT_I64 || t.num == WVT_F32 ||
           t.num == WVT_F64 || t.num == VT_K_BOT;
}
static int is_vec(val_type t) { return t.num == WVT_V128 || t.num == VT_K_BOT; }
static int is_ref(val_type t) { return !(is_num(t) || is_vec(t)) || t.num == VT_K_BOT; }
static int is_bot(val_type t) { return t.num == VT_K_BOT; }

/*   type pack_type = I8 | I16
 *   type field_type = Field(val : val_type | pack_type, mut : bool)
 *
 * A field's storage is kept UNPACKED in `t` with `pack` recording whether it was
 * packed, because unpack_field is the only reader of the distinction. */
/* `pack` is 0 for a value type, else the PACKED WIDTH in bits (8 or 16). The width is
 * kept because §3.3.9 says "The packed type packtype matches only itself" — collapsing
 * both to a bare "packed" flag makes i8 and i16 match, which is what array.copy
 * between (array (mut i8)) and (array i16) turns on. */
typedef struct { val_type t; uint8_t mut; uint8_t pack; } field_type;

/*   func unpack_field(t : field_type) : val_type =
 *     if (it = I8 || t = I16) return I32
 *     return t                                                                */
static val_type unpack_field(field_type f) { return f.pack ? VT_NUM(WVT_I32) : f.t; }

/*   type comp_type = struct_type | array_type | func_type
 *
 * expand_def is the identity here: the reader hands back a rec group's members
 * already indexed by typeidx, so the projection below stores the EXPANDED composite
 * type per index and there is no `t.rec.types[t.proj].body` indirection left to do. */
typedef struct {
    uint8_t     kind;        /* WST_STRUCT | WST_ARRAY | WST_FUNC */
    uint8_t     final;       /* §3.2.11: a FINAL type may not be a supertype */
    field_type* fields;      /* struct fields, or the single array field */
    uint32_t    nfields;
    val_type*   params;      uint32_t nparams;    /* func only */
    val_type*   results;     uint32_t nresults;
} def_type;

static int is_func(const def_type* t)   { return t->kind == WST_FUNC; }
static int is_struct(const def_type* t) { return t->kind == WST_STRUCT; }
static int is_array(const def_type* t)  { return t->kind == WST_ARRAY; }

/*   func top_heap_type(t : heap_type) : heap_type
 *
 * The least precise supertype of a heap type. Concrete types resolve through their
 * structural kind, which is what the lattice context already records. */
static int32_t top_heap_type(const jav_subtype_ctx_t* lat, int32_t t) {
    switch (t) {
    case HT_ANY: case HT_EQ: case HT_I31: case HT_STRUCT: case HT_ARRAY: case HT_NONE:
        return HT_ANY;
    case HT_FUNC: case HT_NOFUNC:       return HT_FUNC;
    case HT_EXTERN: case HT_NOEXTERN:   return HT_EXTERN;
    case HT_EXN: case HT_NOEXN:         return HT_EXN;
    case HT_BOT:                        return HT_BOT;   /* CannotOccurInSource */
    default: break;
    }
    if (t >= 0 && lat && (uint32_t)t < lat->ntypes) {
        switch (lat->kinds[t]) {
        case WST_STRUCT: case WST_ARRAY: return HT_ANY;
        case WST_FUNC:                   return HT_FUNC;
        default: break;
        }
    }
    return HT_BOT;
}

/* §3.1.1's Convention, verbatim:
 *
 *   "The *difference* rt1 \ rt2 between two reference types is defined as follows:
 *      (ref null1? ht1) \ (ref null ht2)  =  (ref ht1)
 *      (ref null1? ht1) \ (ref ht2)       =  (ref null1? ht1)"
 *
 * ...with the Note that it "computes an approximation … Since the type system does not
 * have general union types, the definition only affects the presence of null and
 * cannot express the absence of other values." So the heap type never moves: a
 * nullable rt2 removes null from rt1, and a non-null rt2 changes nothing. */
static val_type diff_ref(val_type rt1, val_type rt2) {
    return rt2.null ? VT_REF(0, rt1.heap) : rt1;
}

/*   func matches_val(t1 : val_type, t2 : val_type) : bool
 *
 * Numbers and vectors match exactly; references go through the §3.3 lattice, which
 * is jav_subtype.c — the one part of the verifier that is pure logic over a finite
 * domain and has its own edge-by-edge test, so re-deriving it here would be a second
 * reading with no upside. Bot matches everything (the caller handles that). */
static int matches_val(const jav_subtype_ctx_t* lat, val_type a, val_type b) {
    if (is_bot(a)) return 1;
    if (is_bot(b)) return 1;
    if (a.num == VT_K_REF && b.num == VT_K_REF)
        return jav_rt_sub(lat, a.null, a.heap, b.null, b.heap);
    if (a.num == VT_K_REF || b.num == VT_K_REF) return 0;
    return a.num == b.num;
}

/* ════════════════════════════════════════════════════════════════════════════
 * §7.6.1 Data Structures — Context
 *
 *   var return_type : list(val_type)     var globals : array(global_type)
 *   var types       : array(def_type)    var funcs   : array(func_type)
 *   var locals      : array(val_type)    var tables  : array(table_type)
 *   var locals_init : array(bool)        var mems    : array(mem_type)
 *
 * The module-level half is projected once from jav_module_t; the per-function half
 * (return_type, locals, locals_init) is built per body by wat_check_body.
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct { val_type rt; uint8_t is64; jav_limits_t lim; } table_type;
typedef struct { val_type t;  uint8_t mut;  } global_type;
typedef struct { uint8_t is64; jav_limits_t lim; } mem_type;

struct wat_check_ctx {
    const jav_module_t* m;
    bbq_arena*          a;

    def_type*    types;     uint32_t ntypes;
    uint8_t*     kinds;     int32_t* supers;      /* the §3.3 lattice's backing */
    jav_subtype_ctx_t lat;

    /* §3.1.3 rolling: which recursive group each type index belongs to, as the first
     * index of the group and its size. A reference INSIDE the group is `rec.i`; one
     * outside is a type index that closing substitutes. */
    uint32_t*    grp_first;  uint32_t* grp_n;

    uint32_t*    func_type; uint32_t nfuncs;      /* funcidx -> typeidx */
    table_type*  tables;    uint32_t ntables;
    mem_type*    mems;      uint32_t nmems;
    uint8_t*     mem_is64;                        /* parallel to mems, for the hot path */
    global_type* globals;   uint32_t nglobals;
    /* §3.4's Note on constant expressions: a global's initializer sees imported and
     * previously defined globals; a table's sees IMPORTED ones only. Both are
     * "enforced in the validation rule for modules by constraining the context C". */
    uint32_t     nimport_globals;
    uint32_t*    tag_type;  uint32_t ntags;       /* tagidx -> typeidx */
    val_type*    elem_rt;   uint32_t nelems;
    uint32_t     ndatas;    uint8_t  have_datacount;
    uint8_t*     func_ref;  /* §3.4.6 C.refs: funcidx declared by an elem/global/export */
};

/* §7.6.1 assumes these:
 *
 *   func validate_val_type(t : val_type)
 *   func validate_ref_type(t : ref_type)
 *
 * and §7.6.2 calls validate_ref_type before ref.test and br_on_cast. §3.2.5 says a
 * reference type is valid if its heap type is, and §3.2.3 says a concrete heap type —
 * a type use — is valid if "The type C.types[typeidx] exists". An abstract heap type
 * (§3.2.4) is always valid. So this is the index bound that keeps a body, or a
 * declaration, from naming a type the module never defined. */
static int heap_type_ok(const wat_check_ctx_t* cx, int32_t ht) {
    return ht < 0 || (uint32_t)ht < cx->ntypes;
}
static int val_type_ok(const wat_check_ctx_t* cx, val_type t) {
    return t.num != VT_K_REF || heap_type_ok(cx, t.heap);
}

/*   §3.3.9 The storage type storagetype1 matches storagetype2 if either both are value
 *   types that match, or both are packed types that match — and "The packed type
 *   packtype matches only itself."                                             */
static int storage_matches(const wat_check_ctx_t* cx, field_type f1, field_type f2) {
    if (f1.pack || f2.pack) return f1.pack == f2.pack;          /* only itself */
    return matches_val(&cx->lat, f1.t, f2.t);
}

/* ── arena helpers ─────────────────────────────────────────────────────────── */
#define ANEW(a, T, n) ((T*)arena_zero((a), sizeof(T) * (size_t)((n) ? (n) : 1)))
static void* arena_zero(bbq_arena* a, size_t n) {
    void* p = bbq_arena_alloc(a, n);
    if (p) memset(p, 0, n);
    return p;
}

/* ── §5.3 value types, as the reader hands them over ───────────────────────── */
/* jav_val_type is the §5.3.5 byte plus an optional heaptype for (ref null? ht). */
static int vt_from_reader(const jav_val_type_t* v, val_type* out) {
    switch (v->head) {
    case 0x7f: *out = VT_NUM(WVT_I32);  return 1;
    case 0x7e: *out = VT_NUM(WVT_I64);  return 1;
    case 0x7d: *out = VT_NUM(WVT_F32);  return 1;
    case 0x7c: *out = VT_NUM(WVT_F64);  return 1;
    case 0x7b: *out = VT_NUM(WVT_V128); return 1;
    /* §6.4.4's shorthands are BINARY shorthands too (§5.3.4): a bare abstract
     * heaptype byte is the nullable reference to it. */
    case 0x70: *out = VT_REF(1, HT_FUNC);     return 1;
    case 0x6f: *out = VT_REF(1, HT_EXTERN);   return 1;
    case 0x6e: *out = VT_REF(1, HT_ANY);      return 1;
    case 0x6d: *out = VT_REF(1, HT_EQ);       return 1;
    case 0x6c: *out = VT_REF(1, HT_I31);      return 1;
    case 0x6b: *out = VT_REF(1, HT_STRUCT);   return 1;
    case 0x6a: *out = VT_REF(1, HT_ARRAY);    return 1;
    case 0x69: *out = VT_REF(1, HT_EXN);      return 1;
    case 0x71: *out = VT_REF(1, HT_NONE);     return 1;
    case 0x73: *out = VT_REF(1, HT_NOFUNC);   return 1;
    case 0x72: *out = VT_REF(1, HT_NOEXTERN); return 1;
    case 0x74: *out = VT_REF(1, HT_NOEXN);    return 1;
    /* §5.3.5 (ref null? heaptype) — 0x63 nullable, 0x64 non-null. */
    case 0x63: case 0x64:
        if (!v->ht.has_value) return 0;
        *out = VT_REF(v->head == 0x63, (int32_t)v->ht.value.x);
        return 1;
    default: return 0;
    }
}

/* valtype, reftype, storagetype and blocktype all carry {head, ht?} but the reader
 * gives each its own ANONYMOUS struct type, so the heaptype is copied field-wise
 * rather than assigned. This is the one place that knows. */
static jav_val_type_t vt_wrap(uint8_t head, bool has, jav_heap_type_t v) {
    jav_val_type_t t;
    t.head = head; t.ht.has_value = has; t.ht.value = v;
    return t;
}

static int st_from_reader(const jav_storage_type_t* s, field_type* out, uint8_t mut) {
    /* §5.3.6 storagetype ::= valtype | packedtype (i8 = 0x78, i16 = 0x77). */
    if (s->head == 0x78 || s->head == 0x77) {
        out->t = VT_NUM(WVT_I32); out->pack = (s->head == 0x78) ? 8 : 16; out->mut = mut;
        return 1;
    }
    jav_val_type_t v = vt_wrap(s->head, s->ht.has_value, s->ht.value);
    if (!vt_from_reader(&v, &out->t)) return 0;
    out->pack = 0; out->mut = mut;
    return 1;
}

static int rt_from_reader(const jav_ref_type_t* r, val_type* out) {
    jav_val_type_t v = vt_wrap(r->head, r->ht.has_value, r->ht.value);
    return vt_from_reader(&v, out);
}

/* ── §3.5's C.refs, collected from the decoded tree ────────────────────────── */
static void mark_ref(wat_check_ctx_t* cx, uint32_t x) {
    if (x < cx->nfuncs) cx->func_ref[x] = 1;
}
/* funcidx(expr): every ref.func in a constant expression. The reader already decoded
 * the instructions, so this is a walk and not a second LEB scan. */
static void scan_reffunc(wat_check_ctx_t* cx, const jav_expr_t* e) {
    for (size_t i = 0; i < e->instrs.count; i++)
        if (e->instrs.items[i].op == 0xd2) mark_ref(cx, e->instrs.items[i].body.u.case_3.x);
}

/* ── the sections, walked once ─────────────────────────────────────────────── */
static const jav_section_t* section(const jav_module_t* m, uint8_t id) {
    for (size_t i = 0; i < m->sections.count; i++)
        if (m->sections.items[i].id == id) return &m->sections.items[i];
    return NULL;
}

/* §5.5.4 type ::= rectype. A type-section entry is either an explicit rec group
 * (0x4E) or a singleton shorthand, and the index space counts MEMBERS. */
static uint32_t count_types(const jav_type_section_t* ts) {
    uint32_t n = 0;
    for (size_t i = 0; i < ts->types.count; i++) {
        const jav_rec_type_t* r = &ts->types.items[i];
        n += (r->head == 0x4e) ? (uint32_t)r->body.u.case_0.members.count : 1;
    }
    return n;
}

static int project_comp(bbq_arena* a, const jav_comp_type_t* c, def_type* d) {
    switch (c->head) {
    case 0x5e: {   /* array */
        d->kind = WST_ARRAY;
        d->fields = ANEW(a, field_type, 1); d->nfields = 1;
        if (!d->fields) return 0;
        const jav_array_type_t* at = &c->body.u.case_0;
        return st_from_reader(&at->field.storage, &d->fields[0], at->field.mut);
    }
    case 0x5f: {   /* struct */
        d->kind = WST_STRUCT;
        const jav_struct_type_t* st = &c->body.u.case_1;
        d->nfields = (uint32_t)st->fields.count;
        d->fields = ANEW(a, field_type, d->nfields);
        if (!d->fields) return 0;
        for (uint32_t i = 0; i < d->nfields; i++)
            if (!st_from_reader(&st->fields.items[i].storage, &d->fields[i],
                                st->fields.items[i].mut)) return 0;
        return 1;
    }
    case 0x60: {   /* func */
        d->kind = WST_FUNC;
        const jav_func_type_t* ft = &c->body.u.case_2;
        d->nparams  = (uint32_t)ft->params.count;
        d->nresults = (uint32_t)ft->results.count;
        d->params  = ANEW(a, val_type, d->nparams);
        d->results = ANEW(a, val_type, d->nresults);
        if (!d->params || !d->results) return 0;
        for (uint32_t i = 0; i < d->nparams; i++)
            if (!vt_from_reader(&ft->params.items[i], &d->params[i])) return 0;
        for (uint32_t i = 0; i < d->nresults; i++)
            if (!vt_from_reader(&ft->results.items[i], &d->results[i])) return 0;
        return 1;
    }
    default: return 0;
    }
}

/* One SubType: its composite body, plus the declared supertype the §3.3 lattice
 * needs. §3.2.11 admits at most one supertype. */
static int project_sub(bbq_arena* a, const jav_sub_type_t* s, def_type* d, int32_t* super) {
    if (s->supers.count > 1) return 0;
    *super = s->supers.count ? (int32_t)s->supers.items[0] : -1;
    return project_comp(a, &s->body, d);
}

/* The composite shorthands. §5.3.8: a bare comptype (0x5E/0x5F/0x60) is `sub final`
 * with no supertypes, so the only difference from a SubType entry is where the body
 * sits. RecMember and RecType each have their own case ordering (wasm.bbq's switch
 * order), which is why these are two wrappers and not one. */
static int project_rec_member(bbq_arena* a, const jav_rec_member_t* m,
                              def_type* d, int32_t* super) {
    *super = -1;
    /* wasm.bbq: "0x4F = final, 0x50 = open"; a bare comptype shorthand is `sub final`
     * with no supertypes, so everything but 0x50 is final. */
    d->final = (m->head != 0x50);
    switch (m->head) {
    case 0x4f: return project_sub(a, &m->body.u.case_0, d, super);
    case 0x50: return project_sub(a, &m->body.u.case_1, d, super);
    case 0x5e: d->kind = WST_ARRAY;
               d->fields = ANEW(a, field_type, 1); d->nfields = 1;
               return d->fields && st_from_reader(&m->body.u.case_2.field.storage,
                                                  &d->fields[0], m->body.u.case_2.field.mut);
    case 0x5f: { jav_comp_type_t c; c.head = 0x5f; c.body.tag = 1;
                 c.body.u.case_1 = m->body.u.case_3; return project_comp(a, &c, d); }
    case 0x60: { jav_comp_type_t c; c.head = 0x60; c.body.tag = 2;
                 c.body.u.case_2 = m->body.u.case_4; return project_comp(a, &c, d); }
    default: return 0;
    }
}

static int project_rec_type(bbq_arena* a, const jav_rec_type_t* r,
                            def_type* d, int32_t* super) {
    *super = -1;
    d->final = (r->head != 0x50);
    switch (r->head) {
    case 0x4f: return project_sub(a, &r->body.u.case_1, d, super);
    case 0x50: return project_sub(a, &r->body.u.case_2, d, super);
    case 0x5e: d->kind = WST_ARRAY;
               d->fields = ANEW(a, field_type, 1); d->nfields = 1;
               return d->fields && st_from_reader(&r->body.u.case_3.field.storage,
                                                  &d->fields[0], r->body.u.case_3.field.mut);
    case 0x5f: { jav_comp_type_t c; c.head = 0x5f; c.body.tag = 1;
                 c.body.u.case_1 = r->body.u.case_4; return project_comp(a, &c, d); }
    case 0x60: { jav_comp_type_t c; c.head = 0x60; c.body.tag = 2;
                 c.body.u.case_2 = r->body.u.case_5; return project_comp(a, &c, d); }
    default: return 0;
    }
}

/* ── §3.1.3 + §3.1.6: the canonical closure ────────────────────────────────────
 *
 * §3.3.10 makes two defined types equivalent when their CLOSURES are syntactically
 * equal, and §3.1.3's Note says why that is decidable by comparison alone: rolling a
 * recursive group replaces the type indices internal to it with recursive type
 * indices, and "this representation ensures that types with equivalent recursive
 * structure are also syntactically equal, hence allowing a simple equality check on
 * (closed) types."
 *
 * So: serialise each recursive GROUP with intra-group references written as `rec.i`
 * and every other reference written as the already-computed canonical id of the type
 * it names (§3.1.6's clos* closes C.types in order, so a reference out of the group
 * points at one already closed). Interning that serialisation gives ids with exactly
 * the property `jav_subtype_ctx_t.canon` documents:
 *
 *     "clos(a)=clos(b) <=> canon[a]==canon[b]"
 *
 * Without it the lattice falls back to "concrete equivalence = same index", which is
 * what type-equivalence.wast exists to catch.
 *
 * A reference FORWARD to a later group is not closable — clos* has not reached it —
 * so such a group is given a unique id rather than a shared one. That can only cause
 * two types to compare unequal, never equal, so it is fail-closed; and the
 * differential is what would report it if a valid module ever hit it. */
enum { CANON_REC = 0x40000000, CANON_ABS = 0x20000000, CANON_OPEN = 0x10000000 };

static void canon_u32(uint8_t** v, uint32_t x) {
    for (int i = 0; i < 4; i++) bbq_vec_push(*v, (uint8_t)(x >> (i * 8)));
}
/* One heap-type reference, relative to the group being serialised. */
static void canon_heap(const wat_check_ctx_t* cx, const int32_t* canon,
                       uint32_t first, uint32_t n, uint8_t** v, int32_t ht, int* open) {
    if (ht < 0) { canon_u32(v, CANON_ABS | (uint32_t)(-ht)); return; }
    uint32_t t = (uint32_t)ht;
    if (t >= first && t < first + n) { canon_u32(v, CANON_REC | (t - first)); return; }  /* rec.i */
    if (t < first && t < cx->ntypes) { canon_u32(v, (uint32_t)canon[t]); return; }
    *open = 1;                                    /* forward or out of range */
    canon_u32(v, CANON_OPEN | t);
}
static void canon_val(const wat_check_ctx_t* cx, const int32_t* canon,
                      uint32_t first, uint32_t n, uint8_t** v, val_type t, int* open) {
    canon_u32(v, (uint32_t)(uint16_t)t.num);
    if (t.num == VT_K_REF) {
        canon_u32(v, t.null);
        canon_heap(cx, canon, first, n, v, t.heap, open);
    }
}

static int32_t* build_canon(wat_check_ctx_t* cx, bbq_arena* a) {
    if (!cx->ntypes) return NULL;
    int32_t* canon = ANEW(a, int32_t, cx->ntypes);
    if (!canon) return NULL;
    /* The intern table: one entry per DISTINCT (group shape, position) seen, with the
     * id it was given. `ids` is parallel to `shapes` and NOT the same counter as
     * `next_id` — a group with a forward reference consumes an id without interning a
     * shape, so indexing `shapes` by an id runs off the end. */
    uint8_t** shapes = NULL; int32_t* ids = NULL; int32_t next_id = 0;

    for (uint32_t first = 0; first < cx->ntypes; ) {
        uint32_t n = cx->grp_n[first] ? cx->grp_n[first] : 1;
        uint8_t* v = NULL; int open = 0;
        canon_u32(&v, n);
        for (uint32_t j = 0; j < n; j++) {
            const def_type* d = &cx->types[first + j];
            canon_u32(&v, d->kind);
            canon_u32(&v, cx->supers[first + j] < 0 ? 0u : 1u);
            if (cx->supers[first + j] >= 0)
                canon_heap(cx, canon, first, n, &v, cx->supers[first + j], &open);
            switch (d->kind) {
            case WST_FUNC:
                canon_u32(&v, d->nparams);
                for (uint32_t i = 0; i < d->nparams; i++)
                    canon_val(cx, canon, first, n, &v, d->params[i], &open);
                canon_u32(&v, d->nresults);
                for (uint32_t i = 0; i < d->nresults; i++)
                    canon_val(cx, canon, first, n, &v, d->results[i], &open);
                break;
            default:   /* struct and array are both a field list */
                canon_u32(&v, d->nfields);
                for (uint32_t i = 0; i < d->nfields; i++) {
                    canon_u32(&v, d->fields[i].mut);
                    canon_u32(&v, d->fields[i].pack);
                    canon_val(cx, canon, first, n, &v, d->fields[i].t, &open);
                }
                break;
            }
        }
        /* one id per member: two groups match member-for-member, so the position is
         * part of the identity. */
        for (uint32_t j = 0; j < n; j++) {
            if (open) { canon[first + j] = next_id++; continue; }
            uint8_t* key = NULL;
            bbq_vec_reserve(key, bbq_vec_len(v) + 4);
            for (size_t b = 0; b < bbq_vec_len(v); b++) bbq_vec_push(key, v[b]);
            canon_u32(&key, j);
            int32_t hit = -1;
            for (size_t s = 0; s < bbq_vec_len(shapes) && hit < 0; s++) {
                if (bbq_vec_len(shapes[s]) != bbq_vec_len(key)) continue;
                if (memcmp(shapes[s], key, bbq_vec_len(key)) == 0) hit = ids[s];
            }
            if (hit >= 0) { canon[first + j] = hit; bbq_vec_free(key); }
            else {
                canon[first + j] = next_id;
                bbq_vec_push(shapes, key); bbq_vec_push(ids, next_id); next_id++;
            }
        }
        bbq_vec_free(v);
        first += n;
    }
    for (size_t i = 0; i < bbq_vec_len(shapes); i++) bbq_vec_free(shapes[i]);
    bbq_vec_free(shapes); bbq_vec_free(ids);
    return canon;
}

wat_check_ctx_t* wat_check_ctx_build(const jav_module_t* m, bbq_arena* a, jav_err_t* err) {
    if (err) *err = JAV_E_NONE;
    wat_check_ctx_t* cx = ANEW(a, wat_check_ctx_t, 1);
    if (!cx) return NULL;
    cx->m = m; cx->a = a;

    /* ── types ── */
    const jav_section_t* s = section(m, 1);
    if (s) {
        const jav_type_section_t* ts = &s->body.u.case_1;
        cx->ntypes = count_types(ts);
        cx->types  = ANEW(a, def_type, cx->ntypes);
        cx->kinds  = ANEW(a, uint8_t,  cx->ntypes);
        cx->supers = ANEW(a, int32_t,  cx->ntypes);
        cx->grp_first = ANEW(a, uint32_t, cx->ntypes);
        cx->grp_n     = ANEW(a, uint32_t, cx->ntypes);
        if (!cx->types || !cx->kinds || !cx->supers || !cx->grp_first || !cx->grp_n) return NULL;
        uint32_t k = 0;
        for (size_t i = 0; i < ts->types.count; i++) {
            const jav_rec_type_t* r = &ts->types.items[i];
            int32_t sup; int ok;
            uint32_t first = k;
            if (r->head == 0x4e) {
                const jav_rec_group_t* g = &r->body.u.case_0;
                for (size_t j = 0; j < g->members.count; j++) {
                    ok = project_rec_member(a, &g->members.items[j], &cx->types[k], &sup);
                    if (!ok) { if (err) *err = JAV_E_UNKNOWN_TYPE; return NULL; }
                    cx->kinds[k] = cx->types[k].kind; cx->supers[k] = sup; k++;
                }
            } else {
                ok = project_rec_type(a, r, &cx->types[k], &sup);
                if (!ok) { if (err) *err = JAV_E_UNKNOWN_TYPE; return NULL; }
                cx->kinds[k] = cx->types[k].kind; cx->supers[k] = sup; k++;
            }
            for (uint32_t j = first; j < k; j++) { cx->grp_first[j] = first; cx->grp_n[j] = k - first; }
        }
    }
    cx->lat.kinds = cx->kinds; cx->lat.supers = cx->supers;
    cx->lat.ntypes = cx->ntypes;
    cx->lat.canon = build_canon(cx, a);
    if (cx->ntypes && !cx->lat.canon) return NULL;

    /* ── the import section contributes to every index space, and its entries come
     * FIRST in each (§5.5.5). ── */
    uint32_t nif = 0, nit = 0, nim = 0, nig = 0, nitag = 0;
    const jav_section_t* si = section(m, 2);
    if (si) {
        const jav_import_section_t* is = &si->body.u.case_2;
        for (size_t i = 0; i < is->imports.count; i++)
            switch (is->imports.items[i].desc.kind) {
            case 0x00: nif++; break; case 0x01: nit++; break;
            case 0x02: nim++; break; case 0x03: nig++; break;
            case 0x04: nitag++; break; default: break;
            }
    }

    const jav_section_t* sf = section(m, 3);
    uint32_t ndef = sf ? (uint32_t)sf->body.u.case_3.type_indices.count : 0;
    cx->nfuncs = nif + ndef;
    cx->func_type = ANEW(a, uint32_t, cx->nfuncs);

    const jav_section_t* stb = section(m, 4);
    cx->ntables = nit + (stb ? (uint32_t)stb->body.u.case_4.tables.count : 0);
    cx->tables  = ANEW(a, table_type, cx->ntables);

    const jav_section_t* sm = section(m, 5);
    cx->nmems = nim + (sm ? (uint32_t)sm->body.u.case_5.mems.count : 0);
    cx->mems     = ANEW(a, mem_type, cx->nmems);
    cx->mem_is64 = ANEW(a, uint8_t,  cx->nmems);

    const jav_section_t* sg = section(m, 6);
    cx->nglobals = nig + (sg ? (uint32_t)sg->body.u.case_6.globals.count : 0);
    cx->globals  = ANEW(a, global_type, cx->nglobals);

    const jav_section_t* stg = section(m, 13);
    cx->ntags = nitag + (stg ? (uint32_t)stg->body.u.case_13.tags.count : 0);
    cx->tag_type = ANEW(a, uint32_t, cx->ntags);

    if (!cx->func_type || !cx->tables || !cx->mems || !cx->mem_is64 || !cx->globals || !cx->tag_type)
        return NULL;
    cx->nimport_globals = nig;

    /* imports first, in section order, each into its own space */
    uint32_t fi = 0, ti = 0, mi = 0, gi = 0, gti = 0;
    if (si) {
        const jav_import_section_t* is = &si->body.u.case_2;
        for (size_t i = 0; i < is->imports.count; i++) {
            const jav_extern_type_t* d = &is->imports.items[i].desc;
            switch (d->kind) {
            case 0x00: cx->func_type[fi++] = d->body.u.case_0.x; break;
            case 0x01: {
                const jav_table_type_t* tt = &d->body.u.case_1;
                if (!rt_from_reader(&tt->reftype, &cx->tables[ti].rt)) { if (err) *err = JAV_E_TYPE_MISMATCH; return NULL; }
                cx->tables[ti].is64 = (tt->limits.flag & 0x04) != 0;
                cx->tables[ti].lim  = tt->limits; ti++;
                break;
            }
            case 0x02:
                cx->mems[mi].is64 = (d->body.u.case_2.flag & 0x04) != 0;
                cx->mems[mi].lim  = d->body.u.case_2;
                cx->mem_is64[mi] = cx->mems[mi].is64; mi++;
                break;
            case 0x03: {
                const jav_global_type_t* gt = &d->body.u.case_3;
                if (!vt_from_reader(&gt->type, &cx->globals[gi].t)) { if (err) *err = JAV_E_TYPE_MISMATCH; return NULL; }
                cx->globals[gi].mut = gt->mut; gi++;
                break;
            }
            case 0x04: cx->tag_type[gti++] = d->body.u.case_4.type; break;
            default: if (err) *err = JAV_E_TYPE_MISMATCH; return NULL;
            }
        }
    }
    if (sf) {
        const jav_function_section_t* fs = &sf->body.u.case_3;
        for (size_t i = 0; i < fs->type_indices.count; i++)
            cx->func_type[fi++] = fs->type_indices.items[i];
    }
    if (stb) {
        const jav_table_section_t* tsx = &stb->body.u.case_4;
        for (size_t i = 0; i < tsx->tables.count; i++) {
            const jav_table_t* t = &tsx->tables.items[i];
            const jav_table_type_t* tt = (t->tag == 0x40) ? &t->u.case_0.type : &t->u.default_val.type;
            if (!rt_from_reader(&tt->reftype, &cx->tables[ti].rt)) { if (err) *err = JAV_E_TYPE_MISMATCH; return NULL; }
            cx->tables[ti].is64 = (tt->limits.flag & 0x04) != 0;
            cx->tables[ti].lim  = tt->limits; ti++;
        }
    }
    if (sm) {
        const jav_memory_section_t* ms = &sm->body.u.case_5;
        for (size_t i = 0; i < ms->mems.count; i++) {
            cx->mems[mi].is64 = (ms->mems.items[i].limits.flag & 0x04) != 0;
            cx->mems[mi].lim  = ms->mems.items[i].limits;
            cx->mem_is64[mi] = cx->mems[mi].is64; mi++;
        }
    }
    if (sg) {
        const jav_global_section_t* gs = &sg->body.u.case_6;
        for (size_t i = 0; i < gs->globals.count; i++) {
            if (!vt_from_reader(&gs->globals.items[i].type.type, &cx->globals[gi].t)) { if (err) *err = JAV_E_TYPE_MISMATCH; return NULL; }
            cx->globals[gi].mut = gs->globals.items[i].type.mut; gi++;
        }
    }
    if (stg) {
        const jav_tag_section_t* gts = &stg->body.u.case_13;
        for (size_t i = 0; i < gts->tags.count; i++)
            cx->tag_type[gti++] = gts->tags.items[i].type;
    }

    /* ── element segments: their reftype bounds table.init / array.new_elem ──
     * §5.5.12's eight forms, and the nullability is NOT uniform across them:
     *   0            ⇒ elem (ref func) …          NON-null
     *   1 / 2 / 3    ⇒ elemkind, and `elemkind ::= 0x00 ⇒ ref func`   NON-null
     *   4            ⇒ elem (ref null func) …     NULLABLE
     *   5 / 6 / 7    ⇒ the declared reftype
     * Treating the funcidx-list forms as `funcref` makes an active segment fail its
     * §3.5.9 `rt ≤ rt'` check against a table of non-null `(ref func)`. */
    const jav_section_t* se = section(m, 9);
    if (se) {
        const jav_element_section_t* es = &se->body.u.case_9;
        cx->nelems = (uint32_t)es->elems.count;
        cx->elem_rt = ANEW(a, val_type, cx->nelems);
        if (!cx->elem_rt) return NULL;
        for (uint32_t i = 0; i < cx->nelems; i++) {
            const jav_elem_t* e = &es->elems.items[i];
            const jav_ref_type_t* rt = NULL;
            switch (e->body.tag) {
            case 5: rt = &e->body.u.case_5.reftype; break;
            case 6: rt = &e->body.u.case_6.reftype; break;
            case 7: rt = &e->body.u.case_7.reftype; break;
            default: break;
            }
            cx->elem_rt[i] = VT_REF(e->body.tag == 4, HT_FUNC);
            if (rt && !rt_from_reader(rt, &cx->elem_rt[i])) { if (err) *err = JAV_E_TYPE_MISMATCH; return NULL; }
        }
    }

    /* §5.5.15's note: "The data count section occurs before the code section, so a
     * single-pass validator can use this count instead of deferring validation." */
    const jav_section_t* sdc = section(m, 12);
    if (sdc) { cx->have_datacount = 1; cx->ndatas = sdc->body.u.case_12.count; }

    /* ── §3.5's module rule: C.refs ──────────────────────────────────────────
     *
     *   "The function index sequence x* is of the form
     *    funcidx(global* mem* table* elem* start? export*)."
     *
     * Note what is NOT in that list: func*. A function index that occurs only inside
     * a function body does not declare itself, which is the whole point of §3.4.6's
     * "x is contained in C.refs" — and it is why `ref.func` needs a module-level pass
     * before any body is walked.
     *
     * `start?` IS in that list in Release 3.0 (2026-06-03) AND IT IS A SPEC BUG. It
     * arrived with spec PR #2162; WebAssembly/spec issue #2200 (opened 2026-06-24,
     * closed 2026-07-07 as completed) reports exactly
     *
     *     (module (start $f) (func $f (drop (ref.func $f))))
     *
     * as "supposed to fail validation, due to undeclared function reference", and the
     * spec's own editor answers that the start index should NOT be among the implicit
     * ref declarations, notes the testsuite and reference interpreter both agree, and
     * asks for a revert. So the start function is skipped here, matching
     * WebAssembly/testsuite ref_func.wast and the engine.
     *
     * Because the reader hands over decoded instructions, "funcidx(expr)" is a walk
     * over jav_instr_t looking for ref.func, not a second byte scan. */
    cx->func_ref = ANEW(a, uint8_t, cx->nfuncs ? cx->nfuncs : 1);
    if (!cx->func_ref) return NULL;

    if (sg) {
        const jav_global_section_t* gs = &sg->body.u.case_6;
        for (size_t i = 0; i < gs->globals.count; i++)
            scan_reffunc(cx, &gs->globals.items[i].init);
    }
    if (stb) {
        const jav_table_section_t* tsx = &stb->body.u.case_4;
        for (size_t i = 0; i < tsx->tables.count; i++)
            if (tsx->tables.items[i].tag == 0x40)
                scan_reffunc(cx, &tsx->tables.items[i].u.case_0.init);
    }
    if (se) {
        const jav_element_section_t* es = &se->body.u.case_9;
        for (size_t i = 0; i < es->elems.count; i++) {
            const jav_elem_t* e = &es->elems.items[i];
            const jav_idx_vec_t* fv = NULL; const jav_expr_vec_t* ev = NULL;
            const jav_expr_t* off = NULL;
            switch (e->body.tag) {
            case 0: fv = &e->body.u.case_0.funcs; off = &e->body.u.case_0.offset; break;
            case 1: fv = &e->body.u.case_1.funcs; break;
            case 2: fv = &e->body.u.case_2.funcs; off = &e->body.u.case_2.offset; break;
            case 3: fv = &e->body.u.case_3.funcs; break;
            case 4: ev = &e->body.u.case_4.exprs; off = &e->body.u.case_4.offset; break;
            case 5: ev = &e->body.u.case_5.exprs; break;
            case 6: ev = &e->body.u.case_6.exprs; off = &e->body.u.case_6.offset; break;
            case 7: ev = &e->body.u.case_7.exprs; break;
            default: break;
            }
            if (off) scan_reffunc(cx, off);
            if (fv) for (size_t j = 0; j < fv->idxs.count; j++) mark_ref(cx, fv->idxs.items[j]);
            if (ev) for (size_t j = 0; j < ev->exprs.count; j++) scan_reffunc(cx, &ev->exprs.items[j]);
        }
    }
    /* (no mark_ref for the start section — see above) */
    const jav_section_t* sx = section(m, 7);
    if (sx) {
        const jav_export_section_t* xs = &sx->body.u.case_7;
        for (size_t i = 0; i < xs->exports.count; i++)
            if (xs->exports.items[i].kind == 0x00) mark_ref(cx, xs->exports.items[i].idx);
    }
    return cx;
}

/* ════════════════════════════════════════════════════════════════════════════
 * §7.6.1 Data Structures — Stacks
 *
 *   type val_stack  = stack(val_type)
 *   type init_stack = stack(u32)
 *   type ctrl_stack = stack(ctrl_frame)
 *   type ctrl_frame = { opcode, start_types, end_types, val_height,
 *                       init_height, unreachable }
 *
 *   var vals : val_stack   var inits : init_stack   var ctrls : ctrl_stack
 *
 * "The notation stack[i] is meant to index the stack from the top, so that, e.g.,
 * ctrls[0] accesses the element pushed last."  ctrls_at(s, i) is that notation.
 * ════════════════════════════════════════════════════════════════════════════ */

/* One value-stack entry: the type §7.6 tracks, plus the instruction that pushed it.
 * That second field is the whole of this file's second product. */
typedef struct { val_type t; const jav_instr_t* prod; } vslot;

typedef struct {
    uint8_t         opcode;
    const val_type* start_types; uint32_t nstart;
    const val_type* end_types;   uint32_t nend;
    uint32_t        val_height;
    uint32_t        init_height;
    int             unreachable;
} ctrl_frame;

#define VALS_MAX  8192
#define CTRL_MAX  1024
#define POP_MAX   1024

/* SPAN_NONE marks an instruction whose producing run is not self-contained, so no
 * parent may fold it. See fold_of(). */
#define SPAN_NONE UINT32_MAX

typedef struct {
    const wat_check_ctx_t* cx;
    bbq_arena*       a;

    vslot*      vals;   uint32_t nvals;
    ctrl_frame* ctrls;  uint32_t nctrls;
    uint32_t*   inits;  uint32_t ninits;

    uint8_t*    locals_init;
    val_type*   locals;      uint32_t nlocals, nparams;
    const val_type* return_type; uint32_t nreturn;

    int         ok;
    jav_err_t   err;
    const jav_instr_t* fail;

    /* the rows, in walk order; `seq` is the index, so a row IS its sequence number */
    wat_info_t* info;   uint32_t ninfo, capinfo;
    uint32_t*   hi;     /* per row: the last seq this instruction's subtree covers */
    uint32_t*   self_lo;/* per row: where its self-contained run starts, or SPAN_NONE */
    bbq_hmap    map;    /* jav_instr_t* -> row index + 1 */

    /* The operands popped by the instruction currently being checked, in pop order.
     * One buffer, reset by row_open — so a row that stays open across a nested walk
     * (block, loop, if, try_table) must close on it BEFORE the frame opens, since
     * every instruction inside, and pop_ctrl's own pop_vals, pops through here. */
    const jav_instr_t* popbuf[POP_MAX];
    uint32_t           npop;
    const jav_instr_t* cur;
    uint32_t           cur_row;
    /* §5's flat opcode ordinal. The struct tree has no `end` or `else` instruction —
     * they are fields of the block — so they are counted HERE, where a walk over the
     * bytes would meet them, and the two orderings stay the same sequence. */
    uint32_t           nseq;
} tc;

static void fail_at(tc* s, jav_err_t e) {
    if (!s->ok) return;
    s->ok = 0; s->err = e;
    s->fail = s->cur;
}
#define error_if(s, cond, e) do { if (cond) { fail_at((s), (e)); return; } } while (0)
#define error_if_v(s, cond, e, v) do { if (cond) { fail_at((s), (e)); return (v); } } while (0)

static ctrl_frame* ctrls_at(tc* s, uint32_t i) { return &s->ctrls[s->nctrls - 1 - i]; }

/* ── §7.6.1: the value stack ───────────────────────────────────────────────── */

/*   func push_val(type : val_type) = vals.push(type)
 *
 * ...and record the instruction that pushed it. */
static void push_val(tc* s, val_type t) {
    if (!s->ok) return;
    if (s->nvals >= VALS_MAX) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
    s->vals[s->nvals].t = t;
    s->vals[s->nvals].prod = s->cur;
    s->nvals++;
}

/*   func pop_val() : val_type =
 *     if (vals.size() = ctrls[0].val_height && ctrls[0].unreachable) return Bot
 *     error_if(vals.size() = ctrls[0].val_height)
 *     return vals.pop()
 *
 * "a polymorphic stack cannot underflow, but instead generates Bot types as needed" —
 * and a Bot has no producer, because nothing pushed it. */
static val_type pop_val(tc* s) {
    if (!s->ok) return VT_BOT;
    if (s->nctrls == 0) { fail_at(s, JAV_E_TYPE_MISMATCH); return VT_BOT; }
    ctrl_frame* f = ctrls_at(s, 0);
    if (s->nvals == f->val_height && f->unreachable) {
        if (s->npop < POP_MAX) s->popbuf[s->npop++] = NULL;
        return VT_BOT;
    }
    if (s->nvals == f->val_height) { fail_at(s, JAV_E_TYPE_MISMATCH); return VT_BOT; }
    s->nvals--;
    if (s->npop < POP_MAX) s->popbuf[s->npop++] = s->vals[s->nvals].prod;
    return s->vals[s->nvals].t;
}

/*   func pop_val(expect : val_type) : val_type =
 *     let actual = pop_val()
 *     error_if(not matches_val(actual, expect))
 *     return actual                                                            */
static val_type pop_val_e(tc* s, val_type expect) {
    val_type actual = pop_val(s);
    if (!s->ok) return actual;
    if (!matches_val(&s->cx->lat, actual, expect)) fail_at(s, JAV_E_TYPE_MISMATCH);
    return actual;
}

/*   func pop_num() : num_type | Bot =
 *     let actual = pop_val()
 *     error_if(not is_num(actual))
 *     return actual                                                            */
static val_type pop_num(tc* s) {
    val_type actual = pop_val(s);
    if (s->ok && !is_num(actual)) fail_at(s, JAV_E_TYPE_MISMATCH);
    return actual;
}

/*   func pop_ref() : ref_type =
 *     let actual = pop_val()
 *     error_if(not is_ref(actual))
 *     if (actual = Bot) return Ref(Bot, false)
 *     return actual                                                            */
static val_type pop_ref(tc* s) {
    val_type actual = pop_val(s);
    if (!s->ok) return VT_REF(0, HT_BOT);
    if (!is_ref(actual)) { fail_at(s, JAV_E_TYPE_MISMATCH); return VT_REF(0, HT_BOT); }
    if (is_bot(actual)) return VT_REF(0, HT_BOT);
    return actual;
}

/*   func push_vals(types : list(val_type)) = foreach (t in types) push_val(t)   */
static void push_vals(tc* s, const val_type* t, uint32_t n) {
    for (uint32_t i = 0; i < n && s->ok; i++) push_val(s, t[i]);
}

/*   func pop_vals(types : list(val_type)) : list(val_type) =
 *     var popped := []
 *     foreach (t in reverse(types)) popped.prepend(pop_val(t))
 *     return popped
 *
 * The RETURN VALUE is load-bearing in exactly one place — br_table's
 * `push_vals(pop_vals(label_types(ctrls[n])))`, which puts back the types that were
 * actually on the stack, not the label's declared ones. In reachable code the two
 * coincide; under stack-polymorphism they do not, because pop_val yielded Bot and
 * pushing a declared type back would make the stack MORE specific than the program
 * proved, so a later arm with a different-but-compatible label type is rejected.
 * `popped` may be NULL where the caller does not need it. */
static void pop_vals_into(tc* s, const val_type* t, uint32_t n, val_type* popped) {
    for (uint32_t i = n; i-- > 0 && s->ok; ) {
        val_type a = pop_val_e(s, t[i]);
        if (popped) popped[i] = a;
    }
}
static void pop_vals(tc* s, const val_type* t, uint32_t n) {
    pop_vals_into(s, t, n, NULL);
}

/* ── §7.6.1: the initialization stack ──────────────────────────────────────── */

/*   func get_local(idx : u32) = error_if(not locals_init[idx])                  */
static void get_local(tc* s, uint32_t idx) {
    error_if(s, idx >= s->nlocals, JAV_E_UNKNOWN_LOCAL);
    error_if(s, !s->locals_init[idx], JAV_E_TYPE_MISMATCH);
}

/*   func set_local(idx : u32) =
 *     if (not locals_init[idx]) { inits.push(idx); locals_init[idx] := true }   */
static void set_local(tc* s, uint32_t idx) {
    error_if(s, idx >= s->nlocals, JAV_E_UNKNOWN_LOCAL);
    if (!s->locals_init[idx]) {
        if (s->ninits < s->nlocals) s->inits[s->ninits++] = idx;
        s->locals_init[idx] = 1;
    }
}

/*   func reset_locals(height : nat) =
 *     while (inits.size() > height) locals_init[inits.pop()] := false           */
static void reset_locals(tc* s, uint32_t height) {
    while (s->ninits > height) s->locals_init[s->inits[--s->ninits]] = 0;
}

/* ── §7.6.1: the control stack ─────────────────────────────────────────────── */

/*   func push_ctrl(opcode, in : list(val_type), out : list(val_type)) =
 *     let frame = ctrl_frame(opcode, in, out, vals.size(), inits.size(), false)
 *     ctrls.push(frame)
 *     push_vals(in)                                                            */
static void push_ctrl(tc* s, uint8_t opcode,
                      const val_type* in, uint32_t nin,
                      const val_type* out, uint32_t nout) {
    if (!s->ok) return;
    if (s->nctrls >= CTRL_MAX) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
    ctrl_frame* f = &s->ctrls[s->nctrls++];
    f->opcode = opcode;
    f->start_types = in;  f->nstart = nin;
    f->end_types   = out; f->nend   = nout;
    f->val_height  = s->nvals;
    f->init_height = s->ninits;
    f->unreachable = 0;
    push_vals(s, in, nin);
}

/*   func pop_ctrl() : ctrl_frame =
 *     error_if(ctrls.is_empty())
 *     let frame = ctrls[0]
 *     pop_vals(frame.end_types)
 *     error_if(vals.size() =/= frame.val_height)
 *     reset_locals(frame.init_height)
 *     ctrls.pop()
 *     return frame                                                             */
static ctrl_frame pop_ctrl(tc* s) {
    ctrl_frame z; memset(&z, 0, sizeof z);
    error_if_v(s, s->nctrls == 0, JAV_E_TYPE_MISMATCH, z);
    ctrl_frame frame = *ctrls_at(s, 0);
    pop_vals(s, frame.end_types, frame.nend);
    if (!s->ok) return frame;
    error_if_v(s, s->nvals != frame.val_height, JAV_E_TYPE_MISMATCH, frame);
    reset_locals(s, frame.init_height);
    s->nctrls--;
    return frame;
}

/*   func label_types(frame : ctrl_frame) : list(val_types) =
 *     return (if (frame.opcode = loop) frame.start_types else frame.end_types)  */
static const val_type* label_types(const ctrl_frame* f, uint32_t* n) {
    if (f->opcode == 0x03 /* loop */) { *n = f->nstart; return f->start_types; }
    *n = f->nend; return f->end_types;
}

/*   func unreachable() =
 *     vals.resize(ctrls[0].val_height)
 *     ctrls[0].unreachable := true                                             */
static void unreachable_(tc* s) {
    if (!s->ok || s->nctrls == 0) return;
    ctrl_frame* f = ctrls_at(s, 0);
    s->nvals = f->val_height;
    f->unreachable = 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * The rows, and §6.5.11's fold admissibility
 * ════════════════════════════════════════════════════════════════════════════ */

static uint32_t row_of(tc* s, const jav_instr_t* in) {
    void* v = bbq_hmap_get(&s->map, (uint64_t)(uintptr_t)in);
    return v ? (uint32_t)(uintptr_t)v - 1 : SPAN_NONE;
}

/* Open a row for `in` and make it the instruction the helpers attribute to. */
static uint32_t row_open(tc* s, const jav_instr_t* in) {
    if (s->ninfo >= s->capinfo) return SPAN_NONE;
    uint32_t r = s->ninfo++;
    s->info[r].in = in;
    s->info[r].seq = s->nseq++;
    s->info[r].producer = NULL;
    s->info[r].noperands = 0;
    s->info[r].fold = 0;
    s->hi[r] = r;
    s->self_lo[r] = SPAN_NONE;
    bbq_hmap_put(&s->map, (uint64_t)(uintptr_t)in, (void*)(uintptr_t)(r + 1));
    s->cur = in;
    s->cur_row = r;
    s->npop = 0;
    return r;
}

/* Close the row: reverse the pop order into operand order, then decide the fold.
 *
 * §3.6 of the plan: the admissible fold depth is the largest k such that the run
 * producing the last k operands contains EXACTLY those operands' subtrees. Walking
 * the operands from last to first, each one must (a) have a producer, (b) be itself
 * self-contained, and (c) end immediately before where the previous step began. The
 * first failure stops the count — which is how a `nop` between two operands bounds
 * the fold without any special case, and how a Bot operand (dead code below the
 * frame height, with no producer at all) bounds it too. */
static void row_close(tc* s, uint32_t r) {
    if (r == SPAN_NONE || r >= s->ninfo) return;
    uint32_t n = s->npop;
    const jav_instr_t** ops = ANEW(s->a, const jav_instr_t*, n);
    if (!ops && n) return;
    for (uint32_t i = 0; i < n; i++) ops[i] = s->popbuf[n - 1 - i];   /* pop order -> operand order */
    s->info[r].producer  = ops;
    s->info[r].noperands = n;
    uint8_t in_op = s->info[r].in->op;

    /* §6.5.11 prints five foldedinstr productions and `foldedinstr*` appears in exactly
     * one of them — `'(' 'if' label blocktype foldedinstr* '(' 'then' … ')' … ')'`.
     * `block`, `loop` and `try_table` have no such slot, so a blocktype PARAMETER is
     * emitted before them and can never be folded in. §3.6's `fold` is what §6.5.11
     * admits, so for those three it is 0 — and self_lo falls out right with it: a
     * parameterised block is not one self-contained group, its operand being outside
     * the parentheses. */
    uint32_t cap = (in_op == 0x02 || in_op == 0x03 || in_op == 0x1f) ? 0 : n;

    uint32_t k = 0, expect_hi = r ? r - 1 : SPAN_NONE;
    for (uint32_t j = n; k < cap && j-- > 0; ) {
        if (!ops[j] || expect_hi == SPAN_NONE) break;
        uint32_t pr = row_of(s, ops[j]);
        if (pr == SPAN_NONE || s->self_lo[pr] == SPAN_NONE) break;
        if (s->hi[pr] != expect_hi) break;
        k++;
        expect_hi = s->self_lo[pr] ? s->self_lo[pr] - 1 : SPAN_NONE;
    }
    s->info[r].fold = k;
    /* Self-contained exactly when every operand folded: then the whole run from the
     * first operand's start to here computes this value and nothing else. */
    s->self_lo[r] = (k == n) ? (n ? s->self_lo[row_of(s, ops[0])] : r) : SPAN_NONE;
}

/* ════════════════════════════════════════════════════════════════════════════
 * §7.6.2 Validation of Opcode Sequences
 * ════════════════════════════════════════════════════════════════════════════ */

static void check_instrs(tc* s, const jav_instr_t* items, uint32_t n);

/* §5.3.10 blocktype: 0x40 empty, a valtype, or a positive s33 typeidx. */
static int block_type(tc* s, const jav_block_type_t* bt,
                      const val_type** in, uint32_t* nin,
                      const val_type** out, uint32_t* nout) {
    *in = NULL; *nin = 0; *out = NULL; *nout = 0;
    if (bt->bt == -0x40) return 1;                       /* empty */
    if (bt->bt >= 0) {                                   /* typeidx */
        uint32_t x = (uint32_t)bt->bt;
        if (x >= s->cx->ntypes) { fail_at(s, JAV_E_UNKNOWN_TYPE); return 0; }
        const def_type* t = &s->cx->types[x];
        if (!is_func(t)) { fail_at(s, JAV_E_TYPE_MISMATCH); return 0; }
        *in = t->params; *nin = t->nparams;
        *out = t->results; *nout = t->nresults;
        return 1;
    }
    /* a single value type, encoded as its own negative byte */
    jav_val_type_t v = vt_wrap((uint8_t)(bt->bt & 0x7f), bt->ht.has_value, bt->ht.value);
    val_type* one = ANEW(s->a, val_type, 1);
    if (!one || !vt_from_reader(&v, one)) { fail_at(s, JAV_E_TYPE_MISMATCH); return 0; }
    if (!val_type_ok(s->cx, *one)) { fail_at(s, JAV_E_UNKNOWN_TYPE); return 0; }
    *out = one; *nout = 1;
    return 1;
}

/* An opgen signature slot -> val_type. WVT_BOT in the table means "the context
 * decides" (local.get's result, drop's operand), and those opcodes never reach the
 * table-driven path below. */
static val_type vt_of_slot(jav_valtype_t w, const int32_t* ht, uint32_t i) {
    switch (w) {
    case WVT_I32: case WVT_I64: case WVT_F32: case WVT_F64: case WVT_V128:
        return VT_NUM(w);
    case WVT_REF:    return VT_REF(1, ht ? ht[i] : HT_ANY);
    case WVT_REF_NN: return VT_REF(0, ht ? ht[i] : HT_ANY);
    default:         return VT_BOT;
    }
}

/* The generated transfer for an ordinary opcode: "Other instructions are checked in
 * a similar manner" — and the manner is exactly the stack signature, which opgen
 * already derived from wasm.def. */
static int opsig_for(const jav_instr_t* in, jav_opsig_t* out) {
    if (in->op == 0xfb || in->op == 0xfc || in->op == 0xfd) {
        const jav_opsig_t* tbl = jav_opsig_sub[in->op];
        if (!tbl) return 0;
        uint32_t sub;
        switch (in->op) {
        case 0xfb: sub = in->body.u.case_29.sub; break;
        case 0xfc: sub = in->body.u.case_30.sub; break;
        default:   sub = in->body.u.case_31.sub; break;
        }
        uint32_t lim = (in->op == 0xfb) ? 31u : (in->op == 0xfc) ? 18u : 276u;
        if (sub >= lim || !tbl[sub].present) return 0;
        *out = tbl[sub];
        return 1;
    }
    if (!jav_opsig[in->op].present) return 0;
    *out = jav_opsig[in->op];
    return 1;
}

static void check_ordinary(tc* s, const jav_instr_t* in) {
    jav_opsig_t sig;
    if (!opsig_for(in, &sig)) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
    for (uint32_t i = sig.npop; i-- > 0 && s->ok; )
        pop_val_e(s, vt_of_slot(sig.pops[i], sig.pop_ht, i));
    for (uint32_t i = 0; i < sig.npush && s->ok; i++)
        push_val(s, vt_of_slot(sig.pushes[i], sig.push_ht, i));
}

/* ── the context, bounds-checked ───────────────────────────────────────────── */

static const def_type* type_at(tc* s, uint32_t x) {
    if (x >= s->cx->ntypes) { fail_at(s, JAV_E_UNKNOWN_TYPE); return NULL; }
    return &s->cx->types[x];
}
/* §7.6.2 uses expand_def(types[x]) then is_func/is_struct/is_array; the projection
 * already expanded, so this is the kind check with its index bound. */
static const def_type* type_of_kind(tc* s, uint32_t x, uint8_t kind) {
    const def_type* t = type_at(s, x);
    if (!t) return NULL;
    if (t->kind != kind) { fail_at(s, JAV_E_TYPE_MISMATCH); return NULL; }
    return t;
}
static const def_type* func_at(tc* s, uint32_t funcidx) {
    if (funcidx >= s->cx->nfuncs) { fail_at(s, JAV_E_UNKNOWN_FUNCTION); return NULL; }
    return type_of_kind(s, s->cx->func_type[funcidx], WST_FUNC);
}
/* §2.3.11 addrtype ::= i32 | i64 — a memory's or table's declared address type. */
static val_type addr_type(int is64) { return VT_NUM(is64 ? WVT_I64 : WVT_I32); }
/* §2.3.11's convention: "The minimum of two address types is defined as the address
 * type whose bit width is the minimum of the two." */
static int addr_min(int a, int b) { return a && b; }

/* §3.4.4: table.copy / table.init require rt2 ≤ rt1. */
static void tables_match(tc* s, uint32_t x1, uint32_t x2) {
    if (!s->ok) return;
    if (!matches_val(&s->cx->lat, s->cx->tables[x2].rt, s->cx->tables[x1].rt))
        fail_at(s, JAV_E_TYPE_MISMATCH);
}

/* The natural alignment of a memory access, in log2 bytes, and its lane count —
 * both columns of spec/instructions.toml, reached through the table opgen's sibling
 * generator emits from it. `align` is -1 for a non-memory instruction. */
static const wat_mnemonic_t* mnemonic_of(const jav_instr_t* in) {
    uint8_t prefix = (in->op == 0xfb || in->op == 0xfc || in->op == 0xfd) ? in->op : 0;
    uint32_t op = in->op;
    if (prefix == 0xfb) op = in->body.u.case_29.sub;
    else if (prefix == 0xfc) op = in->body.u.case_30.sub;
    else if (prefix == 0xfd) op = in->body.u.case_31.sub;
    for (size_t i = 0; i < sizeof wat_mnemonics / sizeof wat_mnemonics[0]; i++)
        if (wat_mnemonics[i].prefix == prefix && wat_mnemonics[i].op == op)
            return &wat_mnemonics[i];
    return NULL;
}

/*   §3.4.5 memarg
 *     {align n, offset m} is valid for at and N if:
 *       - 2^n is less than or equal to N/8.
 *       - m is less than 2^|at|.
 *
 * `align` and `natural` are both log2 bytes, so "2^n ≤ N/8" is "n ≤ natural". */
static int memarg_ok(tc* s, const jav_mem_arg_t* ma, int natural, uint32_t* out_mem) {
    /* §5.4.5: `align` is the RAW field — bit 6 flags the presence of the memidx that
     * follows (wasm.bbq: `memidx: optional<uleb128> where (align & 0x40) != 0`), so
     * the log2 alignment §3.4.5 bounds is the low six bits. Comparing the raw field
     * rejects every explicit-memidx access, which is the form the text assembler
     * emits — so it rejected essentially every memory op that arrived through .wat. */
    uint32_t n = ma->align & 0x3f;
    uint32_t x = ma->memidx.has_value ? ma->memidx.value : 0;
    if (x >= s->cx->nmems) { fail_at(s, JAV_E_UNKNOWN_MEMORY); return 0; }
    if (natural >= 0 && (int)n > natural) { fail_at(s, JAV_E_ALIGNMENT); return 0; }
    if (!s->cx->mem_is64[x] && ma->offset >= (uint64_t)1 << 32) {
        fail_at(s, JAV_E_OFFSET_OUT_OF_RANGE); return 0;
    }
    *out_mem = x;
    return 1;
}

/* §3.4.5's lane bound, "i is less than 128/N". For a memlane access N comes from the
 * natural alignment (N = 8 << align); for extract/replace it is the shape in the
 * mnemonic, whose lane count is the number after the 'x'. Both are generated data. */
static int lane_count(const wat_mnemonic_t* m) {
    if (!m) return 0;
    if (m->align >= 0) return 16 >> m->align;
    const char* x = m->name;
    for (; *x && *x != '.'; x++)
        if (*x == 'x') {
            int n = 0;
            for (const char* p = x + 1; *p >= '0' && *p <= '9'; p++) n = n * 10 + (*p - '0');
            return n;
        }
    return 0;
}

/* ── the families whose transfer needs the context, not just the table ─────── */

/* §3.4.5: every memory access is `at -> ...`, but opgen's table has the address
 * pre-flattened to i32 because the engine's slots are untyped there. So the memarg is
 * validated here, the address is popped with the memory's own address type, and the
 * REST of the signature still comes from the generated table. */
static void check_memory_access(tc* s, const jav_instr_t* in, const wat_mnemonic_t* m,
                                 const jav_mem_arg_t* ma, int has_lane, uint8_t lane) {
    uint32_t memidx;
    if (!memarg_ok(s, ma, m->align, &memidx)) return;
    if (has_lane) {
        int lanes = lane_count(m);
        error_if(s, lanes == 0 || lane >= lanes, JAV_E_INVALID_LANE);
    }
    jav_opsig_t sig;
    if (!opsig_for(in, &sig)) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
    val_type at = addr_type(s->cx->mem_is64[memidx]);
    /* the address is the DEEPEST operand, so it is popped last */
    for (uint32_t i = sig.npop; i-- > 0 && s->ok; )
        pop_val_e(s, i == 0 ? at : vt_of_slot(sig.pops[i], sig.pop_ht, i));
    for (uint32_t i = 0; i < sig.npush && s->ok; i++)
        push_val(s, vt_of_slot(sig.pushes[i], sig.push_ht, i));
}

/* §3.4.11's lane bound on the extract/replace forms, which carry no memarg. */
static void check_lane_imm(tc* s, const jav_instr_t* in, const wat_mnemonic_t* m, uint8_t lane) {
    int lanes = lane_count(m);
    error_if(s, lanes == 0 || lane >= lanes, JAV_E_INVALID_LANE);
    check_ordinary(s, in);
}

static void check_bulk(tc* s, const jav_instr_t* in);
static void check_aggregate(tc* s, const jav_instr_t* in);

static void check_prefixed(tc* s, const jav_instr_t* in, uint32_t r) {
    (void)r;
    const wat_mnemonic_t* m = mnemonic_of(in);
    if (!m) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }   /* not an instruction */

    switch (m->shape) {
    case WSH_MEMARG:
        check_memory_access(s, in, m,
                            (in->op == 0xfd) ? &in->body.u.case_31.body.u.case_0
                                             : &in->body.u.case_17, 0, 0);
        return;
    case WSH_MEMLANE: {
        const jav_mem_lane_imm_t* ml = &in->body.u.case_31.body.u.case_5;
        check_memory_access(s, in, m, &ml->mem, 1, ml->lane);
        return;
    }
    case WSH_LANE:
        check_lane_imm(s, in, m, in->body.u.case_31.body.u.case_3.lane);
        return;
    case WSH_V128:
        /*   §3.4.11 i8x16.shuffle laneidx^16
         *     For all i in i*: The lane index i is less than 2 · dim(sh).
         *
         * TWICE the dimension, because shuffle selects from the concatenation of both
         * operands — so 32 for i8x16, not the 16 that extract_lane and replace_lane
         * are bounded by. It is shape v128, not shape lane, so the lane check above
         * never sees it. */
        if (in->body.u.case_31.sub == 13) {
            const bbq_bytes_t* b = &in->body.u.case_31.body.u.case_1.bytes;
            int lanes = lane_count(m);                 /* dim(sh) from the mnemonic */
            for (size_t i = 0; i < b->length && s->ok; i++)
                error_if(s, b->data[i] >= 2 * lanes, JAV_E_INVALID_LANE);
        }
        check_ordinary(s, in);
        return;
    default: break;
    }

    if (in->op == 0xfc) { check_bulk(s, in); return; }
    if (in->op == 0xfb) { check_aggregate(s, in); return; }
    check_ordinary(s, in);
}

/* §3.4.4 / §3.4.5's bulk operations. Every one of them takes its address operands
 * from a memory's or table's declared address type, which is why none can come off
 * the generated table unchanged. */
static void check_bulk(tc* s, const jav_instr_t* in) {
    uint32_t sub = in->body.u.case_30.sub;
    const jav_misc_instr_t* mi = &in->body.u.case_30;
    switch (sub) {
    case 8: {   /* memory.init x y : at i32 i32 -> eps  (x = dataidx, y = memidx) */
        uint32_t y = mi->body.u.case_1.y, x = mi->body.u.case_1.x;
        error_if(s, y >= s->cx->nmems, JAV_E_UNKNOWN_MEMORY);
        error_if(s, !s->cx->have_datacount, JAV_E_DATA_COUNT_REQUIRED);
        error_if(s, x >= s->cx->ndatas, JAV_E_UNKNOWN_DATA);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, addr_type(s->cx->mem_is64[y]));
        break;
    }
    case 9:     /* data.drop x */
        error_if(s, !s->cx->have_datacount, JAV_E_DATA_COUNT_REQUIRED);
        error_if(s, mi->body.u.case_2.x >= s->cx->ndatas, JAV_E_UNKNOWN_DATA);
        break;
    case 10: {  /* memory.copy x y : at1 at2 min(at1,at2) -> eps */
        uint32_t x = mi->body.u.case_3.x, y = mi->body.u.case_3.y;
        error_if(s, x >= s->cx->nmems || y >= s->cx->nmems, JAV_E_UNKNOWN_MEMORY);
        int a1 = s->cx->mem_is64[x], a2 = s->cx->mem_is64[y];
        pop_val_e(s, addr_type(addr_min(a1, a2)));
        pop_val_e(s, addr_type(a2));
        pop_val_e(s, addr_type(a1));
        break;
    }
    case 11: {  /* memory.fill x : at i32 at -> eps */
        uint32_t x = mi->body.u.case_4.x;
        error_if(s, x >= s->cx->nmems, JAV_E_UNKNOWN_MEMORY);
        val_type at = addr_type(s->cx->mem_is64[x]);
        pop_val_e(s, at);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, at);
        break;
    }
    case 12: {  /* table.init x y : at i32 i32 -> eps  (x = elemidx, y = tableidx) */
        uint32_t x = mi->body.u.case_5.x, y = mi->body.u.case_5.y;
        error_if(s, y >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        error_if(s, x >= s->cx->nelems, JAV_E_UNKNOWN_ELEM);
        error_if(s, !matches_val(&s->cx->lat, s->cx->elem_rt[x], s->cx->tables[y].rt),
                 JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, addr_type(s->cx->tables[y].is64));
        break;
    }
    case 13:    /* elem.drop x */
        error_if(s, mi->body.u.case_6.x >= s->cx->nelems, JAV_E_UNKNOWN_ELEM);
        break;
    case 14: {  /* table.copy x1 x2 : at1 at2 min(at1,at2) -> eps */
        uint32_t x1 = mi->body.u.case_7.x, x2 = mi->body.u.case_7.y;
        error_if(s, x1 >= s->cx->ntables || x2 >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        tables_match(s, x1, x2);
        int a1 = s->cx->tables[x1].is64, a2 = s->cx->tables[x2].is64;
        pop_val_e(s, addr_type(addr_min(a1, a2)));
        pop_val_e(s, addr_type(a2));
        pop_val_e(s, addr_type(a1));
        break;
    }
    case 15: {  /* table.grow x : rt at -> at */
        uint32_t x = mi->body.u.case_8.x;
        error_if(s, x >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        val_type at = addr_type(s->cx->tables[x].is64);
        pop_val_e(s, at);
        pop_val_e(s, s->cx->tables[x].rt);
        push_val(s, at);
        break;
    }
    case 16: {  /* table.size x : eps -> at */
        uint32_t x = mi->body.u.case_8.x;
        error_if(s, x >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        push_val(s, addr_type(s->cx->tables[x].is64));
        break;
    }
    case 17: {  /* table.fill x : at rt at -> eps */
        uint32_t x = mi->body.u.case_8.x;
        error_if(s, x >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        val_type at = addr_type(s->cx->tables[x].is64);
        pop_val_e(s, at);
        pop_val_e(s, s->cx->tables[x].rt);
        pop_val_e(s, at);
        break;
    }
    default:
        check_ordinary(s, in);   /* the numeric conversions (sub 0..7) */
        break;
    }
}

/* §3.4.7 Aggregate Reference Instructions, and §3.4.6's casts. */
static void check_aggregate(tc* s, const jav_instr_t* in) {
    const jav_gc_instr_t* g = &in->body.u.case_29;
    uint32_t sub = g->sub;

    /* the struct/array forms all name a typeidx in their first immediate */
    uint32_t x = 0, y = 0;
    switch (sub) {
    case 0: case 1: case 6: case 7: case 11: case 12: case 13: case 14: case 16:
        x = g->body.u.case_0.x; break;
    case 2: case 3: case 4: case 5:
        x = g->body.u.case_1.x; y = g->body.u.case_1.y; break;
    case 8: case 9: case 10: case 17: case 18: case 19:
        x = g->body.u.case_3.x; y = g->body.u.case_3.y; break;
    default: break;
    }

    switch (sub) {
    /*   case (struct.new x)
     *     let t = expand_def(types[x])
     *     error_if(not is_struct(t))
     *     for (ti in reverse(t.fields)) pop_val(unpack_field(ti))
     *     push_val(Ref(Def(types[x])))                                           */
    case 0: {
        const def_type* t = type_of_kind(s, x, WST_STRUCT);
        if (!t) break;
        for (uint32_t i = t->nfields; i-- > 0 && s->ok; ) pop_val_e(s, unpack_field(t->fields[i]));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 struct.new_default x : eps -> (ref x), every field defaultable */
    case 1: {
        const def_type* t = type_of_kind(s, x, WST_STRUCT);
        if (!t) break;
        for (uint32_t i = 0; i < t->nfields; i++)
            error_if(s, t->fields[i].t.num == VT_K_REF && !t->fields[i].t.null && !t->fields[i].pack,
                     JAV_E_TYPE_MISMATCH);
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 struct.get_sx? x i : (ref null x) -> unpack(zt); "The signedness sx? is
     * present if and only if zt is a packed type." */
    case 2: case 3: case 4: {
        const def_type* t = type_of_kind(s, x, WST_STRUCT);
        if (!t) break;
        error_if(s, y >= t->nfields, JAV_E_TYPE_MISMATCH);
        error_if(s, (sub == 2) == (t->fields[y].pack != 0), JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_REF(1, (int32_t)x));
        push_val(s, unpack_field(t->fields[y]));
        break;
    }
    /*   case (struct.set x n)
     *     let t = expand_def(types[x])
     *     error_if(not is_struct(t) || n >= t.fields.len())
     *     pop_val(Ref(Def(types[x])))
     *     pop_val(unpack_field(st.fields[n]))
     *
     * ...and §3.4.7 requires the field to be of the form (mut zt). The spec's algorithm
     * pops the reference FIRST, which is the deeper operand — the value is on top. */
    case 5: {
        const def_type* t = type_of_kind(s, x, WST_STRUCT);
        if (!t) break;
        error_if(s, y >= t->nfields, JAV_E_TYPE_MISMATCH);
        error_if(s, !t->fields[y].mut, JAV_E_IMMUTABLE_FIELD);
        pop_val_e(s, unpack_field(t->fields[y]));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        break;
    }
    /* §3.4.7 array.new x : unpack(zt) i32 -> (ref x) */
    case 6: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, unpack_field(t->fields[0]));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 array.new_default x : i32 -> (ref x) */
    case 7: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, t->fields[0].t.num == VT_K_REF && !t->fields[0].t.null && !t->fields[0].pack,
                 JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_NUM(WVT_I32));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 array.new_fixed x n : unpack(zt)^n -> (ref x) */
    case 8: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        for (uint32_t i = 0; i < y && s->ok; i++) pop_val_e(s, unpack_field(t->fields[0]));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 array.new_data x y : i32 i32 -> (ref x); "unpack(zt) is of the form
     * numtype or vectype" */
    case 9: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, unpack_field(t->fields[0]).num == VT_K_REF, JAV_E_ARRAY_NOT_NUMERIC);
        error_if(s, !s->cx->have_datacount, JAV_E_DATA_COUNT_REQUIRED);
        error_if(s, y >= s->cx->ndatas, JAV_E_UNKNOWN_DATA);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 array.new_elem x y : i32 i32 -> (ref x); C.elems[y] matches rt */
    case 10: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, y >= s->cx->nelems, JAV_E_UNKNOWN_ELEM);
        error_if(s, !matches_val(&s->cx->lat, s->cx->elem_rt[y], t->fields[0].t),
                 JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        push_val(s, VT_REF(0, (int32_t)x));
        break;
    }
    /* §3.4.7 array.get_sx? x : (ref null x) i32 -> unpack(zt) */
    case 11: case 12: case 13: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, (sub == 11) == (t->fields[0].pack != 0), JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        push_val(s, unpack_field(t->fields[0]));
        break;
    }
    /* §3.4.7 array.set x : (ref null x) i32 unpack(zt) -> eps, field mutable */
    case 14: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, !t->fields[0].mut, JAV_E_IMMUTABLE_ARRAY);
        pop_val_e(s, unpack_field(t->fields[0]));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        break;
    }
    /* §3.4.7 array.len : (ref null array) -> i32 */
    case 15:
        pop_val_e(s, VT_REF(1, HT_ARRAY));
        push_val(s, VT_NUM(WVT_I32));
        break;
    /* §3.4.7 array.fill x : (ref null x) i32 unpack(zt) i32 -> eps */
    case 16: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, !t->fields[0].mut, JAV_E_IMMUTABLE_ARRAY);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, unpack_field(t->fields[0]));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        break;
    }
    /* §3.4.7 array.copy x y : (ref null x) i32 (ref null y) i32 i32 -> eps */
    case 17: {
        const def_type* d = type_of_kind(s, x, WST_ARRAY);
        const def_type* srcs = type_of_kind(s, y, WST_ARRAY);
        if (!d || !srcs || !d->nfields || !srcs->nfields) { if (d && srcs) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, !d->fields[0].mut, JAV_E_IMMUTABLE_ARRAY);
        /* The STORAGE types must match, not their unpacked forms: unpack(i8) and
         * unpack(i16) are both i32, so comparing unpacked lets an (array i16) copy
         * into an (array (mut i8)). */
        error_if(s, !storage_matches(s->cx, srcs->fields[0], d->fields[0]),
                 JAV_E_ARRAY_TYPES_MISMATCH);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)y));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        break;
    }
    /* §3.4.7 array.init_data / array.init_elem x y : (ref null x) i32 i32 i32 -> eps */
    case 18: case 19: {
        const def_type* t = type_of_kind(s, x, WST_ARRAY);
        if (!t || !t->nfields) { if (t) fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, !t->fields[0].mut, JAV_E_IMMUTABLE_ARRAY);
        if (sub == 18) {
            error_if(s, unpack_field(t->fields[0]).num == VT_K_REF, JAV_E_ARRAY_NOT_NUMERIC);
            error_if(s, !s->cx->have_datacount, JAV_E_DATA_COUNT_REQUIRED);
            error_if(s, y >= s->cx->ndatas, JAV_E_UNKNOWN_DATA);
        } else {
            error_if(s, y >= s->cx->nelems, JAV_E_UNKNOWN_ELEM);
            error_if(s, !matches_val(&s->cx->lat, s->cx->elem_rt[y], t->fields[0].t),
                     JAV_E_TYPE_MISMATCH);
        }
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, VT_REF(1, (int32_t)x));
        break;
    }
    /*   case (ref.test rt)
     *     validate_ref_type(rt)
     *     pop_val(Ref(top_heap_type(rt), true))
     *     push_val(I32)
     *
     * sub 20 is `ref.test (ref ht)`, 21 is `ref.test (ref null ht)`; §3.4.6's rt'
     * "liberty to pick a supertype" is top_heap_type, exactly as §7.6.2 prints it.
     * ref.cast (22/23) is the same input with rt as the result. */
    case 20: case 21: case 22: case 23: {
        int32_t ht = (int32_t)g->body.u.case_8.ht;
        int nullable = (sub == 21 || sub == 23);
        if (!heap_type_ok(s->cx, ht)) { fail_at(s, JAV_E_UNKNOWN_TYPE); break; }
        pop_val_e(s, VT_REF(1, top_heap_type(&s->cx->lat, ht)));
        if (sub <= 21) push_val(s, VT_NUM(WVT_I32));
        else           push_val(s, VT_REF(nullable, ht));
        break;
    }
    /*   §3.4.2 br_on_cast l rt1 rt2      : t* rt1 -> t* (rt1 \ rt2)
     *        br_on_cast_fail l rt1 rt2 : t* rt1 -> t* rt2
     *
     * both with C.labels[l] = t* rt and rt2 <= rt1; the branch arm carries rt2 for
     * br_on_cast and (rt1 \ rt2) for br_on_cast_fail, so that is what must match rt.
     *
     * §5.4's castop packs both nullabilities in one byte — bit 0 for rt1, bit 1 for
     * rt2 (`flags = n1 | (n2 << 1)`, the same composition the text reader writes). */
    case 24: case 25: {
        const jav_br_on_cast_t* c = &g->body.u.case_9;
        uint32_t n = c->label;
        error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        val_type rt1 = VT_REF((c->flags & 1) != 0, (int32_t)c->ht1);
        val_type rt2 = VT_REF((c->flags & 2) != 0, (int32_t)c->ht2);
        error_if(s, !val_type_ok(s->cx, rt1) || !val_type_ok(s->cx, rt2), JAV_E_UNKNOWN_TYPE);
        error_if(s, !matches_val(&s->cx->lat, rt2, rt1), JAV_E_TYPE_MISMATCH);
        uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
        error_if(s, nl == 0, JAV_E_TYPE_MISMATCH);
        val_type onbranch = (sub == 24) ? rt2 : diff_ref(rt1, rt2);
        error_if(s, !matches_val(&s->cx->lat, onbranch, l[nl - 1]), JAV_E_TYPE_MISMATCH);
        pop_val_e(s, rt1);              /* the reference, on top of the label's t* */
        pop_vals(s, l, nl - 1);
        push_vals(s, l, nl - 1);
        push_val(s, (sub == 24) ? diff_ref(rt1, rt2) : rt2);
        break;
    }
    /* §3.4.6 any.convert_extern : (ref null extern) -> (ref null any)
     *        extern.convert_any : (ref null any)    -> (ref null extern)
     * Nullability is preserved, so the popped type decides the pushed one. */
    case 26: case 27: {
        val_type a = pop_ref(s);
        if (!s->ok) break;
        push_val(s, VT_REF(a.null, sub == 26 ? HT_ANY : HT_EXTERN));
        break;
    }
    /* §3.4.7 ref.i31 : i32 -> (ref i31); i31.get_sx : (ref null i31) -> i32 */
    case 28:
        pop_val_e(s, VT_NUM(WVT_I32));
        push_val(s, VT_REF(0, HT_I31));
        break;
    case 29: case 30:
        pop_val_e(s, VT_REF(1, HT_I31));
        push_val(s, VT_NUM(WVT_I32));
        break;
    default:
        fail_at(s, JAV_E_TYPE_MISMATCH);
        break;
    }
}

static void check_instr(tc* s, const jav_instr_t* in) {
    uint32_t r = row_open(s, in);

    switch (in->op) {

    /*   case (unreachable)
     *     unreachable()                                                        */
    case 0x00: unreachable_(s); break;

    case 0x01: break;   /* nop */

    /*   case (block t1*->t2*)      case (loop t1*->t2*)
     *     pop_vals([t1*])            pop_vals([t1*])
     *     push_ctrl(block, ...)      push_ctrl(loop, ...)
     *
     * The nested body and the `end` are struct fields here, so the frame opens,
     * the body is walked, and pop_ctrl runs where the `end` case would. */
    case 0x02: case 0x03: {
        const jav_block_t* b = &in->body.u.case_1;
        const val_type *in_t, *out_t; uint32_t nin, nout;
        if (!block_type(s, &b->bt, &in_t, &nin, &out_t, &nout)) break;
        pop_vals(s, in_t, nin);
        row_close(s, r);                  /* its own pops end here; the frame's begin */
        push_ctrl(s, in->op, in_t, nin, out_t, nout);
        s->hi[r] = r;
        check_instrs(s, b->instrs.items, (uint32_t)b->instrs.count);
        s->cur = in;                      /* the `end` is attributed to the block */
        s->nseq++;                        /* ...and it IS an opcode in the byte stream */
        ctrl_frame f = pop_ctrl(s);
        if (s->ok) push_vals(s, f.end_types, f.nend);
        s->hi[r] = s->ninfo ? s->ninfo - 1 : r;
        return;
    }

    /*   case (if t1*->t2*)
     *     pop_val(I32)
     *     pop_vals([t1*])
     *     push_ctrl(if, [t1*], [t2*])
     *   case (else)
     *     let frame = pop_ctrl()
     *     error_if(frame.opcode =/= if)
     *     push_ctrl(else, frame.start_types, frame.end_types)                   */
    case 0x04: {
        const jav_if_t* b = &in->body.u.case_2;
        const val_type *in_t, *out_t; uint32_t nin, nout;
        if (!block_type(s, &b->bt, &in_t, &nin, &out_t, &nout)) break;
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_vals(s, in_t, nin);
        row_close(s, r);                  /* its own pops end here; the frame's begin */
        push_ctrl(s, 0x04, in_t, nin, out_t, nout);
        check_instrs(s, b->then_body.items, (uint32_t)b->then_body.count);
        s->cur = in;
        if (b->else_body.has_value) {
            s->nseq++;                    /* the `else` opcode */
            ctrl_frame f = pop_ctrl(s);
            if (!s->ok) return;
            if (f.opcode != 0x04) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
            push_ctrl(s, 0x05, f.start_types, f.nstart, f.end_types, f.nend);
            check_instrs(s, b->else_body.value.instrs.items,
                         (uint32_t)b->else_body.value.instrs.count);
            s->cur = in;
        } else {
            /* §3.4.7: an `if` with no else must have equal parameters and results,
             * which is what makes the empty else well-typed. */
            if (nin != nout) { fail_at(s, JAV_E_TYPE_MISMATCH); return; }
        }
        s->nseq++;                        /* the `end` */
        ctrl_frame f = pop_ctrl(s);
        if (s->ok) push_vals(s, f.end_types, f.nend);
        s->hi[r] = s->ninfo ? s->ninfo - 1 : r;
        return;
    }

    /*   case (br n)
     *     error_if(ctrls.size() < n)
     *     pop_vals(label_types(ctrls[n]))
     *     unreachable()                                                        */
    case 0x0c: {
        uint32_t n = in->body.u.case_5.x;
        error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
        pop_vals(s, l, nl);
        unreachable_(s);
        break;
    }

    /*   case (br_if n)
     *     error_if(ctrls.size() < n)
     *     pop_val(I32)
     *     pop_vals(label_types(ctrls[n]))
     *     push_vals(label_types(ctrls[n]))                                     */
    case 0x0d: {
        uint32_t n = in->body.u.case_5.x;
        error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        pop_val_e(s, VT_NUM(WVT_I32));
        uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
        pop_vals(s, l, nl);
        push_vals(s, l, nl);
        break;
    }

    /*   case (br_table n* m)
     *     pop_val(I32)
     *     error_if(ctrls.size() < m)
     *     let arity = label_types(ctrls[m]).size()
     *     foreach (n in n*)
     *       error_if(ctrls.size() < n)
     *       error_if(label_types(ctrls[n]).size() =/= arity)
     *       push_vals(pop_vals(label_types(ctrls[n])))
     *     pop_vals(label_types(ctrls[m]))
     *     unreachable()                                                        */
    case 0x0e: {
        const jav_br_table_t* bt = &in->body.u.case_6;
        pop_val_e(s, VT_NUM(WVT_I32));
        uint32_t m = bt->default_target;
        error_if(s, m >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        uint32_t arity; const val_type* dl = label_types(ctrls_at(s, m), &arity);
        /* `push_vals(pop_vals(...))` — the ACTUAL types back, per pop_vals above. */
        val_type* actual = ANEW(s->a, val_type, arity ? arity : 1);
        if (!actual) { fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        for (size_t i = 0; i < bt->targets.count && s->ok; i++) {
            uint32_t n = bt->targets.items[i];
            error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
            uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
            error_if(s, nl != arity, JAV_E_TYPE_MISMATCH);
            pop_vals_into(s, l, nl, actual);
            push_vals(s, actual, nl);
        }
        pop_vals(s, dl, arity);
        unreachable_(s);
        break;
    }

    /*   case (return)
     *     pop_vals(return_types)
     *     unreachable()                                                        */
    case 0x0f:
        pop_vals(s, s->return_type, s->nreturn);
        unreachable_(s);
        break;

    /*   case (drop)
     *     pop_val()                                                            */
    case 0x1a: pop_val(s); break;

    /*   case (select)
     *     pop_val(I32)
     *     let t1 = pop_val()
     *     let t2 = pop_val()
     *     error_if(not (is_num(t1) && is_num(t2) || is_vec(t1) && is_vec(t2)))
     *     error_if(t1 =/= t2 && t1 =/= Bot && t2 =/= Bot)
     *     push_val(if (t1 = Bot) t2 else t1)                                   */
    case 0x1b: {
        pop_val_e(s, VT_NUM(WVT_I32));
        val_type t1 = pop_val(s);
        val_type t2 = pop_val(s);
        if (!s->ok) break;
        error_if(s, !((is_num(t1) && is_num(t2)) || (is_vec(t1) && is_vec(t2))),
                 JAV_E_TYPE_MISMATCH);
        error_if(s, t1.num != t2.num && !is_bot(t1) && !is_bot(t2), JAV_E_TYPE_MISMATCH);
        push_val(s, is_bot(t1) ? t2 : t1);
        break;
    }

    /*   case (select t)
     *     pop_val(I32)
     *     pop_val(t)
     *     pop_val(t)
     *     push_val(t)                                                          */
    case 0x1c: {
        const jav_select_t_t* st = &in->body.u.case_14;
        error_if(s, st->types.count != 1, JAV_E_TYPE_MISMATCH);   /* §3.4.4: exactly one */
        val_type t;
        if (!vt_from_reader(&st->types.items[0], &t)) { fail_at(s, JAV_E_TYPE_MISMATCH); break; }
        error_if(s, !val_type_ok(s->cx, t), JAV_E_UNKNOWN_TYPE);
        pop_val_e(s, VT_NUM(WVT_I32));
        pop_val_e(s, t);
        pop_val_e(s, t);
        push_val(s, t);
        break;
    }

    /*   case (local.get x)      case (local.set x)
     *     get_local(x)            pop_val(locals[x])
     *     push_val(locals[x])     set_local(x)                                 */
    case 0x20: {
        uint32_t x = in->body.u.case_3.x;
        get_local(s, x);
        if (s->ok) push_val(s, s->locals[x]);
        break;
    }
    case 0x21: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->nlocals, JAV_E_UNKNOWN_LOCAL);
        pop_val_e(s, s->locals[x]);
        set_local(s, x);
        break;
    }
    /* local.tee — "checked in a similar manner": pop, set, push back. */
    case 0x22: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->nlocals, JAV_E_UNKNOWN_LOCAL);
        pop_val_e(s, s->locals[x]);
        set_local(s, x);
        if (s->ok) push_val(s, s->locals[x]);
        break;
    }
    case 0x23: {   /* global.get */
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->nglobals, JAV_E_UNKNOWN_GLOBAL);
        push_val(s, s->cx->globals[x].t);
        break;
    }
    case 0x24: {   /* global.set — §3.4.3 requires the global be mutable */
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->nglobals, JAV_E_UNKNOWN_GLOBAL);
        error_if(s, !s->cx->globals[x].mut, JAV_E_IMMUTABLE_GLOBAL);
        pop_val_e(s, s->cx->globals[x].t);
        break;
    }

    /* §3.4.2 call x — the functype comes from the context, not from a table. */
    case 0x10: {
        const def_type* t = func_at(s, in->body.u.case_3.x);
        if (!t) break;
        pop_vals(s, t->params, t->nparams);
        push_vals(s, t->results, t->nresults);
        break;
    }

    /*   case (return_call x)
     *
     * §3.4.2: "The result type C.return is of the form t'2*. The result type t2*
     * matches the result type t'2*." Then stack-polymorphic. */
    case 0x12: {
        const def_type* t = func_at(s, in->body.u.case_3.x);
        if (!t) break;
        pop_vals(s, t->params, t->nparams);
        error_if(s, t->nresults != s->nreturn, JAV_E_TYPE_MISMATCH);
        push_vals(s, t->results, t->nresults);
        pop_vals(s, s->return_type, s->nreturn);
        unreachable_(s);
        break;
    }

    /* §3.4.2 call_indirect x y / return_call_indirect: x is the TYPE index and y the
     * TABLE index (instructions.toml: operands = ["typeidx", "tableidx"]). The table's
     * reference type must match (ref null func), and the popped index is the table's
     * declared ADDRESS type, not i32. */
    case 0x11: case 0x13: {
        uint32_t x = in->body.u.case_9.x, y = in->body.u.case_9.y;
        error_if(s, y >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        error_if(s, !matches_val(&s->cx->lat, s->cx->tables[y].rt, VT_REF(1, HT_FUNC)),
                 JAV_E_TYPE_MISMATCH);
        const def_type* t = type_of_kind(s, x, WST_FUNC);
        if (!t) break;
        pop_val_e(s, addr_type(s->cx->tables[y].is64));
        pop_vals(s, t->params, t->nparams);
        if (in->op == 0x13) {
            error_if(s, t->nresults != s->nreturn, JAV_E_TYPE_MISMATCH);
            push_vals(s, t->results, t->nresults);
            pop_vals(s, s->return_type, s->nreturn);
            unreachable_(s);
        } else {
            push_vals(s, t->results, t->nresults);
        }
        break;
    }

    /*   case (call_ref x)
     *     let t = expand_def(types[x])
     *     error_if(not is_func(t))
     *     pop_vals(t.params)
     *     pop_val(Ref(Def(types[x])))
     *     push_vals(t.results)
     *
     *   case (return_call_ref x)
     *     ...as above, then
     *     error_if(t.results.len() =/= return_types.len())
     *     push_vals(t.results); pop_vals(return_types); unreachable()             */
    case 0x14: case 0x15: {
        uint32_t x = in->body.u.case_3.x;
        const def_type* t = type_of_kind(s, x, WST_FUNC);
        if (!t) break;
        pop_val_e(s, VT_REF(1, (int32_t)x));
        pop_vals(s, t->params, t->nparams);
        if (in->op == 0x15) {
            error_if(s, t->nresults != s->nreturn, JAV_E_TYPE_MISMATCH);
            push_vals(s, t->results, t->nresults);
            pop_vals(s, s->return_type, s->nreturn);
            unreachable_(s);
        } else {
            push_vals(s, t->results, t->nresults);
        }
        break;
    }

    /*   case (throw x)
     *     pop_vals(tags[x].type.params)
     *     unreachable()                                                          */
    case 0x08: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->ntags, JAV_E_UNKNOWN_TAG);
        const def_type* t = type_of_kind(s, s->cx->tag_type[x], WST_FUNC);
        if (!t) break;
        /* §3.2.13: "The result type is empty for exception tags." */
        error_if(s, t->nresults != 0, JAV_E_NONEMPTY_TAG_RESULT);
        pop_vals(s, t->params, t->nparams);
        unreachable_(s);
        break;
    }

    /* §3.4.2 throw_ref : t1* (ref null exn) -> t2* */
    case 0x0a:
        pop_val_e(s, VT_REF(1, HT_EXN));
        unreachable_(s);
        break;

    /*   case (try_table t1*->t2* handler*)
     *     pop_vals([t1*])
     *     foreach (handler in handler*)
     *       error_if(ctrls.size() < handler.label)
     *       push_ctrl(catch, [], label_types(ctrls[handler.label]))
     *       switch (handler.clause)
     *         case (catch x)        push_vals(tags[x].type.params)
     *         case (catch_ref x)    push_vals(tags[x].type.params); push_val(Exnref)
     *         case (catch_all)      skip
     *         case (catch_all_ref)  push_val(Exnref)
     *       pop_ctrl()
     *     push_ctrl(try_table, [t1*], [t2*])                                     */
    case 0x1f: {
        const jav_try_table_t* b = &in->body.u.case_15;
        const val_type *in_t, *out_t; uint32_t nin, nout;
        if (!block_type(s, &b->bt, &in_t, &nin, &out_t, &nout)) break;
        pop_vals(s, in_t, nin);
        row_close(s, r);                  /* its own pops end here; the handlers' begin */
        for (size_t i = 0; i < b->catches.count && s->ok; i++) {
            const jav_catch_t* h = &b->catches.items[i];
            error_if(s, h->label >= s->nctrls, JAV_E_UNKNOWN_LABEL);
            uint32_t nl; const val_type* l = label_types(ctrls_at(s, h->label), &nl);
            push_ctrl(s, 0x1f, NULL, 0, l, nl);
            if (h->kind == 0 || h->kind == 1) {          /* catch / catch_ref */
                error_if(s, !h->tag.has_value || h->tag.value >= s->cx->ntags, JAV_E_UNKNOWN_TAG);
                const def_type* t = type_of_kind(s, s->cx->tag_type[h->tag.value], WST_FUNC);
                if (!t) break;
                error_if(s, t->nresults != 0, JAV_E_NONEMPTY_TAG_RESULT);
                push_vals(s, t->params, t->nparams);
            }
            if (h->kind == 1 || h->kind == 3)            /* catch_ref / catch_all_ref */
                push_val(s, VT_REF(0, HT_EXN));
            pop_ctrl(s);
        }
        if (!s->ok) return;
        push_ctrl(s, 0x1f, in_t, nin, out_t, nout);
        check_instrs(s, b->instrs.items, (uint32_t)b->instrs.count);
        s->cur = in;
        s->nseq++;                        /* the `end` */
        ctrl_frame f = pop_ctrl(s);
        if (s->ok) push_vals(s, f.end_types, f.nend);
        s->hi[r] = s->ninfo ? s->ninfo - 1 : r;
        return;
    }

    /*   case (br_on_null n)
     *     error_if(ctrls.size() < n)
     *     let rt = pop_ref()
     *     pop_vals(label_types(ctrls[n]))
     *     push_vals(label_types(ctrls[n]))
     *     push_val(Ref(rt.heap, false))                                          */
    case 0xd5: {
        uint32_t n = in->body.u.case_5.x;
        error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        val_type rt = pop_ref(s);
        uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
        pop_vals(s, l, nl);
        push_vals(s, l, nl);
        if (s->ok) push_val(s, VT_REF(0, rt.heap));
        break;
    }

    /* §3.4.2 br_on_non_null l : t* (ref null ht) -> t*, where C.labels[l] = t* (ref
     * null? ht) — the branch carries the non-null reference, so the label's last type
     * supplies the heap type and the fall-through drops it. */
    case 0xd6: {
        uint32_t n = in->body.u.case_5.x;
        error_if(s, n >= s->nctrls, JAV_E_UNKNOWN_LABEL);
        uint32_t nl; const val_type* l = label_types(ctrls_at(s, n), &nl);
        error_if(s, nl == 0, JAV_E_TYPE_MISMATCH);
        error_if(s, l[nl - 1].num != VT_K_REF, JAV_E_TYPE_MISMATCH);
        pop_val_e(s, VT_REF(1, l[nl - 1].heap));
        pop_vals(s, l, nl - 1);
        push_vals(s, l, nl - 1);
        break;
    }

    /* §3.4.6 ref.null ht : eps -> (ref null ht), "The heap type ht is valid." */
    case 0xd0: {
        int32_t ht = (int32_t)in->body.u.case_24.ht;
        error_if(s, !heap_type_ok(s->cx, ht), JAV_E_UNKNOWN_TYPE);
        push_val(s, VT_REF(1, ht));
        break;
    }

    /*   case (ref.is_null)
     *     pop_ref(); push_val(I32)                                               */
    case 0xd1: pop_ref(s); push_val(s, VT_NUM(WVT_I32)); break;

    /* §3.4.6 ref.func x : eps -> (ref dt), where "x is contained in C.refs". */
    case 0xd2: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->nfuncs, JAV_E_UNKNOWN_FUNCTION);
        error_if(s, !s->cx->func_ref[x], JAV_E_UNDECLARED_FUNCTION_REFERENCE);
        push_val(s, VT_REF(0, (int32_t)s->cx->func_type[x]));
        break;
    }

    /* §3.4.6 ref.eq : (ref null eq) (ref null eq) -> i32 */
    case 0xd3:
        pop_val_e(s, VT_REF(1, HT_EQ));
        pop_val_e(s, VT_REF(1, HT_EQ));
        push_val(s, VT_NUM(WVT_I32));
        break;

    /*   case (ref.as_non_null)
     *     let rt = pop_ref(); push_val(Ref(rt.heap, false))                      */
    case 0xd4: {
        val_type rt = pop_ref(s);
        if (s->ok) push_val(s, VT_REF(0, rt.heap));
        break;
    }

    /* §3.4.4 table.get x : at -> rt   /   table.set x : at rt -> eps */
    case 0x25: case 0x26: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->ntables, JAV_E_UNKNOWN_TABLE);
        if (in->op == 0x26) pop_val_e(s, s->cx->tables[x].rt);
        pop_val_e(s, addr_type(s->cx->tables[x].is64));
        if (in->op == 0x25) push_val(s, s->cx->tables[x].rt);
        break;
    }

    /* §3.4.5 memory.size x : eps -> at   /   memory.grow x : at -> at */
    case 0x3f: case 0x40: {
        uint32_t x = in->body.u.case_3.x;
        error_if(s, x >= s->cx->nmems, JAV_E_UNKNOWN_MEMORY);
        val_type at = addr_type(s->cx->mem_is64[x]);
        if (in->op == 0x40) pop_val_e(s, at);
        push_val(s, at);
        break;
    }

    default:
        check_prefixed(s, in, r);
        break;
    }

    row_close(s, r);
}

static void check_instrs(tc* s, const jav_instr_t* items, uint32_t n) {
    for (uint32_t i = 0; i < n && s->ok; i++) check_instr(s, &items[i]);
}

/* ════════════════════════════════════════════════════════════════════════════
 * The entry point
 * ════════════════════════════════════════════════════════════════════════════ */

/* Count the instructions a body holds, nested bodies included — the row array's
 * size, known before the walk so the rows never move and a row index is stable. */
static uint32_t count_instrs(const jav_instr_t* items, uint32_t n) {
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        const jav_instr_t* in = &items[i];
        k++;
        switch (in->op) {
        case 0x02: case 0x03:
            k += count_instrs(in->body.u.case_1.instrs.items,
                              (uint32_t)in->body.u.case_1.instrs.count);
            break;
        case 0x04: {
            const jav_if_t* b = &in->body.u.case_2;
            k += count_instrs(b->then_body.items, (uint32_t)b->then_body.count);
            if (b->else_body.has_value)
                k += count_instrs(b->else_body.value.instrs.items,
                                  (uint32_t)b->else_body.value.instrs.count);
            break;
        }
        case 0x1f:
            k += count_instrs(in->body.u.case_15.instrs.items,
                              (uint32_t)in->body.u.case_15.instrs.count);
            break;
        default: break;
        }
    }
    return k;
}

int wat_check_body(const wat_check_ctx_t* cx, uint32_t funcidx,
                   const jav_func_body_t* body, bbq_arena* a, wat_body_t* out) {
    memset(out, 0, sizeof *out);
    out->err = JAV_E_TYPE_MISMATCH;
    if (!cx || funcidx >= cx->nfuncs) return 0;
    uint32_t tx = cx->func_type[funcidx];
    if (tx >= cx->ntypes || !is_func(&cx->types[tx])) return 0;
    const def_type* ft = &cx->types[tx];

    tc s; memset(&s, 0, sizeof s);
    s.cx = cx; s.a = a; s.ok = 1; s.err = JAV_E_NONE;
    /* Pure stacks: push_val writes both vslot fields and push_ctrl writes
     * every ctrl_frame field before anything reads them, so the worst-case
     * backing takes no zeroing — clearing these per body was 6% of a
     * corpus-wide profile, mostly over ten-instruction bodies. */
    s.vals  = (vslot*)bbq_arena_alloc(a, sizeof(vslot) * VALS_MAX);
    s.ctrls = (ctrl_frame*)bbq_arena_alloc(a, sizeof(ctrl_frame) * CTRL_MAX);
    if (!s.vals || !s.ctrls) return 0;

    /* §7.6.1: locals = params ++ the declared ones, RLE-decoded (§5.5.13).
     *
     * The sum is accumulated in 64 bits and bounded before anything is allocated.
     * §5.5.13's own condition is |(+) loc**| < 2^32; accumulating in 32 bits WRAPS on
     * a module that declares more (the malformed corpus has one), which sizes the
     * array small while the copy loop still runs the declared counts — an overrun,
     * found by pointing the differential at the deliberately-bad fixtures.
     *
     * The second bound is an implementation limitation, which §7.3 leaves to the
     * engine: water will not materialise more than 2^24 locals. It is far above
     * anything runnable — MAX_LOCALS is 1024 and `jav_limits.h` calls a module past it
     * "well-formed but unrunnable HERE" — and it exists so a bad count cannot make
     * this process allocate and touch gigabytes. Unlike MAX_LOCALS it is NOT a
     * validation rule, so if a real module ever sat between the two, the §7
     * differential is what would say so. */
    uint64_t ndecl = 0;
    for (size_t i = 0; i < body->locals.count; i++) ndecl += body->locals.items[i].count;
    uint64_t total = (uint64_t)ft->nparams + ndecl;
    if (total >= ((uint64_t)1 << 32)) { out->err = JAV_E_TOO_MANY_LOCALS; return 0; }
    if (total > ((uint64_t)1 << 24)) { out->err = JAV_E_TOO_MANY_LOCALS; return 0; }
    s.nparams = ft->nparams;
    s.nlocals = (uint32_t)total;
    s.locals      = ANEW(a, val_type, s.nlocals);
    s.locals_init = ANEW(a, uint8_t,  s.nlocals);
    s.inits       = ANEW(a, uint32_t, s.nlocals);
    if (!s.locals || !s.locals_init || !s.inits) return 0;
    for (uint32_t i = 0; i < ft->nparams; i++) s.locals[i] = ft->params[i];
    uint32_t li = ft->nparams;
    for (size_t g = 0; g < body->locals.count; g++) {
        val_type t;
        if (!vt_from_reader(&body->locals.items[g].type, &t) || !val_type_ok(cx, t)) {
            out->err = JAV_E_UNKNOWN_TYPE; return 0;      /* §3.5.7 the local type is valid */
        }
        for (uint32_t c = 0; c < body->locals.items[g].count; c++) s.locals[li++] = t;
    }
    /* §3.4.2 local-init: parameters and every DEFAULTABLE local start initialized;
     * a declared non-null reference local does not. */
    for (uint32_t i = 0; i < s.nlocals; i++)
        s.locals_init[i] = (i < s.nparams) ||
                           !(s.locals[i].num == VT_K_REF && !s.locals[i].null);

    s.return_type = ft->results; s.nreturn = ft->nresults;

    s.capinfo = count_instrs(body->body.instrs.items, (uint32_t)body->body.instrs.count) + 1;
    s.info    = ANEW(a, wat_info_t, s.capinfo);
    s.hi      = ANEW(a, uint32_t,   s.capinfo);
    s.self_lo = ANEW(a, uint32_t,   s.capinfo);
    if (!s.info || !s.hi || !s.self_lo) return 0;
    bbq_hmap_init(&s.map, s.capinfo * 2);

    /* "Because every function has an implicit outermost label that corresponds to an
     * implicit block frame, it is an invariant of the validation algorithm that there
     * always is at least one frame on the control stack." */
    push_ctrl(&s, 0x02, NULL, 0, ft->results, ft->nresults);

    check_instrs(&s, body->body.instrs.items, (uint32_t)body->body.instrs.count);

    /* The body's own `end` closes the implicit frame. */
    if (s.ok) {
        s.cur = NULL;
        ctrl_frame f = pop_ctrl(&s);
        (void)f;
        if (s.ok && s.nctrls != 0) fail_at(&s, JAV_E_TYPE_MISMATCH);
    }

    out->info  = s.info;
    out->ninfo = s.ninfo;
    out->ok    = s.ok;
    out->err   = s.ok ? JAV_E_NONE : s.err;
    out->fail  = s.ok ? NULL : s.fail;
    bbq_hmap_free(&s.map);
    return s.ok;
}

/* ════════════════════════════════════════════════════════════════════════════
 * §3.5 Modules — the half of §7 that no body carries
 *
 * §7.6 is the algorithm for "sequences of instructions"; its own sentence adds
 * "(Other aspects of validation are straightforward to implement.)" This is those
 * aspects. water needs them because it writes only valid wat: a module can be
 * §7-invalid with every body well-typed.
 * ════════════════════════════════════════════════════════════════════════════ */

/*   §3.3.9 The field type (mut1? zt1) matches (mut2? zt2) if:
 *     - The storage type zt1 matches zt2.
 *     - Either both mut? are absent, or both are `mut` and zt2 matches zt1.
 *   ...and "The packed type packtype matches only itself."                      */
static int field_matches(const wat_check_ctx_t* cx, field_type f1, field_type f2) {
    if (!storage_matches(cx, f1, f2)) return 0;
    if (!f1.mut != !f2.mut) return 0;
    if (f1.mut && !storage_matches(cx, f2, f1)) return 0;       /* mut is invariant */
    return 1;
}

/*   §3.3.8 comptype1 matches comptype2 if either:
 *     struct ft1* ft1'*  ≤  struct ft2*   with ft1 matching ft2 pointwise — so the
 *       subtype may have EXTRA fields, but the shared prefix must match;
 *     array ft1          ≤  array ft2     with ft1 matching ft2;
 *     func t11*->t12*    ≤  func t21*->t22*  with t21* ≤ t11* (contravariant inputs)
 *       and t12* ≤ t22* (covariant outputs).                                     */
static int comptype_matches(const wat_check_ctx_t* cx, const def_type* d1, const def_type* d2) {
    if (d1->kind != d2->kind) return 0;
    switch (d1->kind) {
    case WST_STRUCT:
        if (d1->nfields < d2->nfields) return 0;
        for (uint32_t i = 0; i < d2->nfields; i++)
            if (!field_matches(cx, d1->fields[i], d2->fields[i])) return 0;
        return 1;
    case WST_ARRAY:
        if (!d1->nfields || !d2->nfields) return 0;
        return field_matches(cx, d1->fields[0], d2->fields[0]);
    default:
        if (d1->nparams != d2->nparams || d1->nresults != d2->nresults) return 0;
        for (uint32_t i = 0; i < d1->nparams; i++)
            if (!matches_val(&cx->lat, d2->params[i], d1->params[i])) return 0;   /* contra */
        for (uint32_t i = 0; i < d1->nresults; i++)
            if (!matches_val(&cx->lat, d1->results[i], d2->results[i])) return 0; /* co */
        return 1;
    }
}

/*   §3.2.12 The limits range [n .. m?] is valid within k if:
 *     n is less than or equal to k.
 *     If m is defined, then n <= m and m <= k.                                 */
static int limits_ok(const jav_limits_t* l, uint64_t k) {
    if (l->min > k) return 0;
    if (l->max.has_value && (l->min > l->max.value || l->max.value > k)) return 0;
    return 1;
}
/* §3.2.15 memtype: within 2^(|addrtype| - 16).  §3.2.16 tabletype: within
 * 2^|addrtype| - 1. |addrtype| is 32 or 64. */
static int memtype_ok(const mem_type* mt) {
    return limits_ok(&mt->lim, mt->is64 ? ((uint64_t)1 << 48) : ((uint64_t)1 << 16));
}
static int tabletype_ok(const table_type* tt) {
    return limits_ok(&tt->lim, tt->is64 ? UINT64_MAX : (((uint64_t)1 << 32) - 1));
}

/* §3.4's constant-expression set, and its Note: a global's initializer may name
 * imported or previously defined globals, a table's or segment's only imported ones.
 * `scope` is how many globals are in reach; the global must also be immutable. */
static int const_instr_ok(const wat_check_ctx_t* cx, const jav_instr_t* in, uint32_t scope,
                          jav_err_t* err) {
    switch (in->op) {
    case 0x41: case 0x42: case 0x43: case 0x44:            /* nt.const */
    case 0x6a: case 0x6b: case 0x6c:                       /* i32.add/sub/mul */
    case 0x7c: case 0x7d: case 0x7e:                       /* i64.add/sub/mul */
    case 0xd0: case 0xd2:                                  /* ref.null, ref.func */
        return 1;
    case 0x23: {                                           /* global.get x */
        uint32_t x = in->body.u.case_3.x;
        if (x >= scope) { *err = JAV_E_UNKNOWN_GLOBAL; return 0; }
        if (cx->globals[x].mut) { *err = JAV_E_CONST_EXPR_REQUIRED; return 0; }
        return 1;
    }
    case 0xfd:                                             /* v128.const only */
        if (in->body.u.case_31.sub == 12) return 1;
        *err = JAV_E_CONST_EXPR_REQUIRED; return 0;
    case 0xfb:
        switch (in->body.u.case_29.sub) {
        case 0: case 1:            /* struct.new, struct.new_default */
        case 6: case 7: case 8:    /* array.new, array.new_default, array.new_fixed */
        case 26: case 27:          /* any.convert_extern, extern.convert_any */
        case 28:                   /* ref.i31 */
            return 1;
        default: *err = JAV_E_CONST_EXPR_REQUIRED; return 0;
        }
    default:
        *err = JAV_E_CONST_EXPR_REQUIRED; return 0;
    }
}

/* An expression that must be valid with `want` and constant. The typing half runs the
 * SAME §7.6 machinery the bodies use — §3.5.3's "The expression expr is valid with the
 * value type t" is the ordinary instruction-sequence judgement — so there is no second
 * type checker for initializers. */
static int check_const_expr(const wat_check_ctx_t* cx, const jav_expr_t* e, val_type want,
                            uint32_t scope, bbq_arena* a, jav_err_t* err) {
    for (size_t i = 0; i < e->instrs.count; i++)
        if (!const_instr_ok(cx, &e->instrs.items[i], scope, err)) return 0;

    tc s; memset(&s, 0, sizeof s);
    s.cx = cx; s.a = a; s.ok = 1; s.err = JAV_E_NONE;
    /* Pure stacks: push_val writes both vslot fields and push_ctrl writes
     * every ctrl_frame field before anything reads them, so the worst-case
     * backing takes no zeroing — clearing these per body was 6% of a
     * corpus-wide profile, mostly over ten-instruction bodies. */
    s.vals  = (vslot*)bbq_arena_alloc(a, sizeof(vslot) * VALS_MAX);
    s.ctrls = (ctrl_frame*)bbq_arena_alloc(a, sizeof(ctrl_frame) * CTRL_MAX);
    s.capinfo = count_instrs(e->instrs.items, (uint32_t)e->instrs.count) + 1;
    s.info    = ANEW(a, wat_info_t, s.capinfo);
    s.hi      = ANEW(a, uint32_t,   s.capinfo);
    s.self_lo = ANEW(a, uint32_t,   s.capinfo);
    if (!s.vals || !s.ctrls || !s.info || !s.hi || !s.self_lo) { *err = JAV_E_TYPE_MISMATCH; return 0; }
    bbq_hmap_init(&s.map, s.capinfo * 2);

    val_type* res = ANEW(a, val_type, 1);
    if (!res) { bbq_hmap_free(&s.map); *err = JAV_E_TYPE_MISMATCH; return 0; }
    res[0] = want;
    s.return_type = res; s.nreturn = 1;
    push_ctrl(&s, 0x02, NULL, 0, res, 1);
    check_instrs(&s, e->instrs.items, (uint32_t)e->instrs.count);
    if (s.ok) { s.cur = NULL; pop_ctrl(&s); }
    bbq_hmap_free(&s.map);
    if (!s.ok) { *err = s.err; return 0; }
    return 1;
}

/* Does any instruction in this expression name a data index? §5.5.17 requires the data
 * count section to be PRESENT if one does, which is what makes memory.init/data.drop
 * checkable in a single pass. */
static int uses_dataidx(const jav_instr_t* items, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        const jav_instr_t* in = &items[i];
        switch (in->op) {
        case 0xfc: {
            uint32_t sub = in->body.u.case_30.sub;
            if (sub == 8 || sub == 9) return 1;                 /* memory.init, data.drop */
            break;
        }
        case 0xfb: {
            uint32_t sub = in->body.u.case_29.sub;
            if (sub == 9 || sub == 18) return 1;                /* array.new_data, array.init_data */
            break;
        }
        case 0x02: case 0x03:
            if (uses_dataidx(in->body.u.case_1.instrs.items,
                             (uint32_t)in->body.u.case_1.instrs.count)) return 1;
            break;
        case 0x04: {
            const jav_if_t* b = &in->body.u.case_2;
            if (uses_dataidx(b->then_body.items, (uint32_t)b->then_body.count)) return 1;
            if (b->else_body.has_value &&
                uses_dataidx(b->else_body.value.instrs.items,
                             (uint32_t)b->else_body.value.instrs.count)) return 1;
            break;
        }
        case 0x1f:
            if (uses_dataidx(in->body.u.case_15.instrs.items,
                             (uint32_t)in->body.u.case_15.instrs.count)) return 1;
            break;
        default: break;
        }
    }
    return 0;
}

int wat_check_module(const wat_check_ctx_t* cx, bbq_arena* a, jav_err_t* err) {
    jav_err_t e = JAV_E_NONE;
    const jav_module_t* m = cx->m;
    if (err) *err = JAV_E_NONE;
    #define REJECT(code) do { if (err) *err = (code); return 0; } while (0)

    /*   §5.5.17, verbatim: "Custom sections may be inserted at any place in this
     *   sequence, while other sections must occur at most once and in the prescribed
     *   order."
     *
     * BBQ guarantees the grammar and the intervals; a cross-section rule is the
     * consumer's, against the tree it was handed. The prescribed order is the module
     * grammar's own and is NOT id order — the tag section (13) sits between the memory
     * and global sections. */
    static const uint8_t ORDER[] = { 1, 2, 3, 4, 5, 13, 6, 7, 8, 9, 12, 10, 11 };
    {
        int seen[14] = {0};
        size_t at = 0;
        for (size_t i = 0; i < m->sections.count; i++) {
            uint8_t id = m->sections.items[i].id;
            if (id == 0) continue;                              /* custom: anywhere */
            if (id > 13) REJECT(JAV_E_SECTION_ORDER);
            if (seen[id]) REJECT(JAV_E_SECTION_ORDER);          /* at most once */
            seen[id] = 1;
            size_t k = at;
            while (k < sizeof ORDER && ORDER[k] != id) k++;
            if (k >= sizeof ORDER) REJECT(JAV_E_SECTION_ORDER); /* out of order */
            at = k + 1;
        }
    }

    /*   "The lengths of lists produced by the (possibly empty) function and code
     *   section must match up."                                                    */
    {
        const jav_section_t* sf = section(m, 3);
        const jav_section_t* sc = section(m, 10);
        size_t nf = sf ? sf->body.u.case_3.type_indices.count : 0;
        size_t nc = sc ? sc->body.u.case_10.entries.count : 0;
        if (nf != nc) REJECT(JAV_E_FUNC_CODE_LENGTHS);

        /*   "Similarly, the optional data count must match the length of the data
         *   segment list. Furthermore, it must be present if any data index occurs in
         *   the code section."                                                     */
        const jav_section_t* sdc = section(m, 12);
        const jav_section_t* sda = section(m, 11);
        size_t nd = sda ? sda->body.u.case_11.datas.count : 0;
        if (sdc && sdc->body.u.case_12.count != nd) REJECT(JAV_E_DATA_COUNT_LENGTHS);
        if (!sdc && sc)
            for (size_t i = 0; i < nc; i++)
                if (uses_dataidx(sc->body.u.case_10.entries.items[i].body.body.instrs.items,
                                 (uint32_t)sc->body.u.case_10.entries.items[i].body.body.instrs.count))
                    REJECT(JAV_E_DATA_COUNT_REQUIRED);
    }

    /*   §3.2.11 The sub type (sub final? x* comptype) is valid for the type index x0 if:
     *     - The length of x* is less than or equal to 1.        [project_sub]
     *     - For all x in x*:
     *         The index x is less than x0.
     *         The type C.types[x] exists.
     *         The sub type unroll(C.types[x]) is of the form (sub y* comptype')  — i.e.
     *           NOT final.
     *     - The composite type comptype MATCHES the composite type comptype'.
     *
     * That last one is §3.3.8's structural match, not the declared-supertype walk: a
     * type is trivially "a subtype of" its own declared supertype, so asking the
     * lattice checks nothing. */
    for (uint32_t x = 0; x < cx->ntypes; x++) {
        int32_t sup = cx->supers[x];
        if (sup < 0) continue;
        if ((uint32_t)sup >= x) REJECT(JAV_E_SUB_TYPE);         /* "The index x is less than x0" */
        if (cx->types[sup].final) REJECT(JAV_E_SUB_TYPE);
        if (!comptype_matches(cx, &cx->types[x], &cx->types[sup])) REJECT(JAV_E_SUB_TYPE);
    }

    /* §3.5.4 / §3.2.15, §3.5.5 / §3.2.16 */
    for (uint32_t i = 0; i < cx->nmems; i++)
        if (!memtype_ok(&cx->mems[i]))
            REJECT(cx->mems[i].lim.max.has_value && cx->mems[i].lim.min > cx->mems[i].lim.max.value
                   ? JAV_E_SIZE_MIN_GT_MAX : JAV_E_MEMORY_SIZE);
    for (uint32_t i = 0; i < cx->ntables; i++)
        if (!tabletype_ok(&cx->tables[i]))
            REJECT(cx->tables[i].lim.max.has_value && cx->tables[i].lim.min > cx->tables[i].lim.max.value
                   ? JAV_E_SIZE_MIN_GT_MAX : JAV_E_TABLE_SIZE);

    /* §3.2.5/§3.2.3 — every value type reaching the context from a DECLARATION must be
     * valid, i.e. a concrete heap type must name a type the module defines. §3.5.3's
     * "The global type globaltype is valid", §3.5.5's tabletype, §3.5.9's elemtype. */
    for (uint32_t i = 0; i < cx->nglobals; i++)
        if (!val_type_ok(cx, cx->globals[i].t)) REJECT(JAV_E_UNKNOWN_TYPE);
    for (uint32_t i = 0; i < cx->ntables; i++)
        if (!val_type_ok(cx, cx->tables[i].rt)) REJECT(JAV_E_UNKNOWN_TYPE);
    for (uint32_t i = 0; i < cx->nelems; i++)
        if (!val_type_ok(cx, cx->elem_rt[i])) REJECT(JAV_E_UNKNOWN_TYPE);
    /* ...and the same inside every DEFINED type, but with a TIGHTER bound than
     * `< ntypes`. §3.5.1: "The sequence of types defined in a module is validated
     * INCREMENTALLY", each type definition under a context holding only the types
     * before it — so a reference may reach an earlier recursive group or its OWN
     * group (which is what `rec` exists for), never a later one.
     *
     *   (type (func (param (ref 1)))) (type (func))   is "unknown type"
     *   (rec (type $a …(ref $b)…) (type $b …))        is fine
     *
     * `< ntypes` alone accepts the first, which is the whole of type-rec.wast's point. */
    for (uint32_t x = 0; x < cx->ntypes; x++) {
        const def_type* d = &cx->types[x];
        uint32_t lim = cx->grp_first[x] + cx->grp_n[x];        /* own group inclusive */
        #define TY_OK(t) do { \
            if ((t).num == VT_K_REF && (t).heap >= 0 && (uint32_t)(t).heap >= lim) \
                REJECT(JAV_E_UNKNOWN_TYPE); \
        } while (0)
        for (uint32_t i = 0; i < d->nfields;  i++) TY_OK(d->fields[i].t);
        for (uint32_t i = 0; i < d->nparams;  i++) TY_OK(d->params[i]);
        for (uint32_t i = 0; i < d->nresults; i++) TY_OK(d->results[i]);
        #undef TY_OK
    }

    /* §3.5.2 tags expand to a function type, and §3.2.13's result type is empty. */
    for (uint32_t i = 0; i < cx->ntags; i++) {
        uint32_t tx = cx->tag_type[i];
        if (tx >= cx->ntypes) REJECT(JAV_E_UNKNOWN_TYPE);
        if (cx->types[tx].kind != WST_FUNC) REJECT(JAV_E_TYPE_MISMATCH);
        if (cx->types[tx].nresults != 0) REJECT(JAV_E_NONEMPTY_TAG_RESULT);
    }

    /* §3.5.6's index bound (the bodies themselves are wat_check_body's). */
    for (uint32_t i = 0; i < cx->nfuncs; i++) {
        uint32_t tx = cx->func_type[i];
        if (tx >= cx->ntypes) REJECT(JAV_E_UNKNOWN_TYPE);
        if (cx->types[tx].kind != WST_FUNC) REJECT(JAV_E_TYPE_MISMATCH);
    }

    /* §3.5.3 globals, incrementally: global i sees the imports plus globals before it. */
    const jav_section_t* sg = section(m, 6);
    if (sg) {
        const jav_global_section_t* gs = &sg->body.u.case_6;
        for (size_t i = 0; i < gs->globals.count; i++) {
            uint32_t gi = cx->nimport_globals + (uint32_t)i;
            if (!check_const_expr(cx, &gs->globals.items[i].init, cx->globals[gi].t, gi, a, &e))
                REJECT(e);
        }
    }

    /* §3.5.5 a table's initializer is const and typed by its element type; §3.4's Note
     * limits its global.get to IMPORTED globals. */
    const jav_section_t* stb = section(m, 4);
    if (stb) {
        const jav_table_section_t* ts = &stb->body.u.case_4;
        uint32_t ti = cx->ntables - (uint32_t)ts->tables.count;
        for (size_t i = 0; i < ts->tables.count; i++, ti++) {
            if (ts->tables.items[i].tag == 0x40) {
                if (!check_const_expr(cx, &ts->tables.items[i].u.case_0.init,
                                      cx->tables[ti].rt, cx->nimport_globals, a, &e))
                    REJECT(e);
            } else {
                /* The plain form carries no initializer, and §5.5.6 gives it the
                 * implicit `ref.null ht` of §6.6.6's abbreviation. That expression has
                 * type (ref null ht), so it only satisfies §3.5.5's "expr is valid with
                 * the value type rt" when rt is NULLABLE — a table of non-null (ref $t)
                 * must state its own initializer. */
                if (cx->tables[ti].rt.num == VT_K_REF && !cx->tables[ti].rt.null)
                    REJECT(JAV_E_TYPE_MISMATCH);
            }
        }
    }

    /* §3.5.10 the start function expands to func eps -> eps. */
    const jav_section_t* sst = section(m, 8);
    if (sst) {
        uint32_t x = sst->body.u.case_8.func;
        if (x >= cx->nfuncs) REJECT(JAV_E_UNKNOWN_FUNCTION);
        const def_type* t = &cx->types[cx->func_type[x]];
        if (t->nparams != 0 || t->nresults != 0) REJECT(JAV_E_START_FUNCTION);
    }

    /* §3.5.12 exports: the external index must be in range, and §3.5.13's `nm* disjoint`. */
    const jav_section_t* sx = section(m, 7);
    if (sx) {
        const jav_export_section_t* xs = &sx->body.u.case_7;
        for (size_t i = 0; i < xs->exports.count; i++) {
            const jav_export_t* x = &xs->exports.items[i];
            switch (x->kind) {
            case 0x00: if (x->idx >= cx->nfuncs)   REJECT(JAV_E_UNKNOWN_FUNCTION); break;
            case 0x01: if (x->idx >= cx->ntables)  REJECT(JAV_E_UNKNOWN_TABLE);    break;
            case 0x02: if (x->idx >= cx->nmems)    REJECT(JAV_E_UNKNOWN_MEMORY);   break;
            case 0x03: if (x->idx >= cx->nglobals) REJECT(JAV_E_UNKNOWN_GLOBAL);   break;
            case 0x04: if (x->idx >= cx->ntags)    REJECT(JAV_E_UNKNOWN_TAG);      break;
            default: REJECT(JAV_E_TYPE_MISMATCH);
            }
            for (size_t j = 0; j < i; j++) {
                const jav_name_t* b1 = &xs->exports.items[j].name;
                if (b1->count == x->name.count &&
                    memcmp(b1->bytes.data, x->name.bytes.data, b1->count) == 0)
                    REJECT(JAV_E_DUPLICATE_EXPORT_NAME);
            }
        }
    }

    /* §3.5.8 data segments: an active one names a memory and its offset is const,
     * typed by that memory's ADDRESS type. */
    const jav_section_t* sd = section(m, 11);
    if (sd) {
        const jav_data_section_t* ds = &sd->body.u.case_11;
        for (size_t i = 0; i < ds->datas.count; i++) {
            const jav_data_t* d = &ds->datas.items[i];
            uint32_t mx; const jav_expr_t* off;
            if (d->body.tag == 0)      { mx = 0;                      off = &d->body.u.case_0.offset; }
            else if (d->body.tag == 2) { mx = d->body.u.case_2.memidx; off = &d->body.u.case_2.offset; }
            else continue;                                            /* passive */
            if (mx >= cx->nmems) REJECT(JAV_E_UNKNOWN_MEMORY);
            /* §3.5.13 validates data and elem under C, not C' — only the global and
             * table sequences are restricted to the imported globals. */
            if (!check_const_expr(cx, off, addr_type(cx->mems[mx].is64), cx->nglobals, a, &e))
                REJECT(e);
        }
    }

    /* §3.5.9 element segments: every init expression is const and typed by the
     * element type; an active one names a table with rt <= rt' and a const offset. */
    const jav_section_t* se = section(m, 9);
    if (se) {
        const jav_element_section_t* es = &se->body.u.case_9;
        for (uint32_t i = 0; i < (uint32_t)es->elems.count; i++) {
            const jav_elem_t* el = &es->elems.items[i];
            const jav_expr_vec_t* ev = NULL; const jav_expr_t* off = NULL;
            const jav_idx_vec_t* fv = NULL;
            uint32_t tx = 0; int active = 0;
            switch (el->body.tag) {
            case 0: off = &el->body.u.case_0.offset; active = 1; fv = &el->body.u.case_0.funcs; break;
            case 1: fv = &el->body.u.case_1.funcs; break;
            case 2: off = &el->body.u.case_2.offset; tx = el->body.u.case_2.table; active = 1;
                    fv = &el->body.u.case_2.funcs; break;
            case 3: fv = &el->body.u.case_3.funcs; break;
            case 4: off = &el->body.u.case_4.offset; active = 1; ev = &el->body.u.case_4.exprs; break;
            case 6: off = &el->body.u.case_6.offset; tx = el->body.u.case_6.table; active = 1;
                    ev = &el->body.u.case_6.exprs; break;
            case 5: ev = &el->body.u.case_5.exprs; break;
            case 7: ev = &el->body.u.case_7.exprs; break;
            default: break;
            }
            if (active) {
                if (tx >= cx->ntables) REJECT(JAV_E_UNKNOWN_TABLE);
                if (!matches_val(&cx->lat, cx->elem_rt[i], cx->tables[tx].rt)) REJECT(JAV_E_TYPE_MISMATCH);
                if (!check_const_expr(cx, off, addr_type(cx->tables[tx].is64), cx->nglobals, a, &e))
                    REJECT(e);
            }
            if (ev)
                for (size_t j = 0; j < ev->exprs.count; j++)
                    if (!check_const_expr(cx, &ev->exprs.items[j], cx->elem_rt[i], cx->nglobals, a, &e))
                        REJECT(e);
            /* The funcidx-list forms (0/1/2/3) expand to `(ref.func y)*`, so every y is
             * a §3.4.6 ref.func and "The function C.funcs[x] exists" applies to it. */
            if (fv)
                for (size_t j = 0; j < fv->idxs.count; j++)
                    if (fv->idxs.items[j] >= cx->nfuncs) REJECT(JAV_E_UNKNOWN_FUNCTION);
        }
    }
    #undef REJECT
    return 1;
}

const wat_info_t* wat_info(const wat_body_t* b, const jav_instr_t* in) {
    if (!b || !b->info) return NULL;
    for (uint32_t i = 0; i < b->ninfo; i++)
        if (b->info[i].in == in) return &b->info[i];
    return NULL;
}
