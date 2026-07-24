// jav_module_index.c — flatten the c-lite span index into the shared §7/§4.5 tables.
// Spec references are to the WebAssembly Core Specification, Release 3.0.
#include "jav_module_index.h"
#include "jav_view_nav.h"   // jav_view_find_section
#include "bbq_vec.h"        // crt growable array (member collection)
#include "jav_gc.h"         // gc_rtt_t — the collector's object-layout descriptor (built below)
#include "heap.h"           // heap_t — jav_heap_typereg lazily creates the heap's shared registry
#include <string.h>
#include <stdlib.h>         // calloc/free — the typereg owns its own allocation

// Span-index navigation (jav_view_field / jav_view_nchild / jav_view_section_array) is
// shared from jav_view_nav.h — this TU adds only the table-specific shape below.
// §5.5.7 `Table = switch(peek())` is a discriminated union (short `tabletype` vs `0x40 0x00 tabletype
// expr`), so a table-section element wraps the chosen arm (TablePlain/TableInit) in an extra node; both
// arms carry `type: TableType`. (Import tables come through ExternType's field-switch, which is unwrapped.)
static const bbq_field_capture* defined_table_type(const bbq_field_capture* elem) {
    return jav_view_field(jav_view_choice(elem), "type");
}

// ── §5.3.5 valtype → the validator's (jav_valtype_t, concrete typeidx) view ────
// A reference whose heap type has a boundary tag maps to that WVT_*; the abstract
// GC heaptypes (any/eq/none/nofunc/noextern/noexn, bare struct/array) and concrete
// func refs have no boundary tag yet — fail closed rather than invent a value.
// §5.3.5: decode a heap type to the GENERIC reftype representation — WVT_REF / WVT_REF_NN
// plus the heap type stored on the parallel `tidx` array: a concrete typeidx (≥0) OR an
// abstract HT_* code (negative, kept as its signed bit-pattern). One shape for every reftype.
static int map_ref(int64_t code, int nn, const uint8_t* kinds, uint32_t ntypes,
                   jav_valtype_t* w, uint32_t* tidx) {
    (void)kinds;
    if (code >= 0) {                                   // §5.3.3: a concrete typeidx ((ref $t), any $t)
        if ((uint32_t)code >= ntypes) return 0;
        *tidx = (uint32_t)code;
    } else switch ((int32_t)code) {                    // §5.3.4: an abstract heap type (an HT_* code)
        case HT_ANY: case HT_EQ:  case HT_I31:   case HT_STRUCT: case HT_ARRAY: case HT_NONE:
        case HT_FUNC: case HT_NOFUNC: case HT_EXTERN: case HT_NOEXTERN: case HT_EXN: case HT_NOEXN:
            *tidx = (uint32_t)(int32_t)code; break;    // store the signed HT_* bit-pattern
        default: return 0;
    }
    *w = nn ? WVT_REF_NN : WVT_REF;
    return 1;
}

static int decode_valtype(const bbq_field_capture* vt, const uint8_t* buf,
                          const uint8_t* kinds, uint32_t ntypes,
                          jav_valtype_t* w, uint32_t* tidx) {
    const bbq_field_capture* hn = jav_view_field(vt, "head");
    if (!hn) return 0;
    uint8_t h = (uint8_t)bbq_node_int(hn, buf);
    *tidx = 0;
    switch (h) {
    case 0x7F: *w = WVT_I32;  return 1;                 // §5.3.1
    case 0x7E: *w = WVT_I64;  return 1;
    case 0x7D: *w = WVT_F32;  return 1;
    case 0x7C: *w = WVT_F64;  return 1;
    case 0x7B: *w = WVT_V128; return 1;                 // §5.3.2
    case 0x63: case 0x64: {                             // §5.3.4: (ref [null] ht)
        const bbq_field_capture* htn = jav_view_field(vt, "ht");
        if (!htn) return 0;
        int64_t x = bbq_node_int(jav_view_field(htn, "x"), buf);   // s33: <0 abstract, ≥0 typeidx
        return map_ref(x, h == 0x64, kinds, ntypes, w, tidx);
    }
    default:                                            // §5.3.4 short form: absheaptype ⇒ ref null ht
        if (h >= 0x69 && h <= 0x74)
            return map_ref((int)h - 128, 0, kinds, ntypes, w, tidx);
        return 0;
    }
}

// ── §5.3.7 storagetype = valtype | packtype (0x78 i8, 0x77 i16) ────────────────
// A packed field's UNPACKED type (what struct.get / array.get and new/set see) is i32;
// `*pack` records the storage width (1 = i8, 2 = i16, 0 = unpacked) for the get_s/u check.
static int decode_storagetype(const bbq_field_capture* st, const uint8_t* buf,
                              const uint8_t* kinds, uint32_t ntypes,
                              jav_valtype_t* w, uint32_t* tidx, uint8_t* pack) {
    const bbq_field_capture* hn = jav_view_field(st, "head");
    if (!hn) return 0;
    uint8_t h = (uint8_t)bbq_node_int(hn, buf);
    if (h == 0x78 || h == 0x77) { *w = WVT_I32; *tidx = 0; *pack = (h == 0x78) ? 1 : 2; return 1; }
    *pack = 0;
    return decode_valtype(st, buf, kinds, ntypes, w, tidx);   // StorageType shares ValType's shape
}

// ── §5.3.6 a resulttype (vec(valtype)) → parallel jav_valtype_t / tidx arrays ──
static int decode_resulttype(const bbq_field_capture* arr, const uint8_t* buf,
                             const uint8_t* kinds, uint32_t ntypes, bbq_arena* arena,
                             const jav_valtype_t** out_ts, const uint32_t** out_tx,
                             uint16_t* out_n) {
    int n = arr ? arr->child_count : 0;
    *out_n = (uint16_t)n;
    if (n == 0) { *out_ts = NULL; *out_tx = NULL; return 1; }
    jav_valtype_t* ts = bbq_arena_alloc(arena, (size_t)n * sizeof *ts);
    uint32_t*      tx = bbq_arena_alloc(arena, (size_t)n * sizeof *tx);
    for (int i = 0; i < n; i++)
        if (!decode_valtype(&arr->children[i], buf, kinds, ntypes, &ts[i], &tx[i]))
            return 0;
    *out_ts = ts; *out_tx = tx;
    return 1;
}

// ── §2.3.9 struct/array composite types → the validator's field tables ──────────
// Decode one FieldType {storage, mut} into parallel value-type / heaptype / pack-width
// slots. Returns 1, or 0 fail-closed on a malformed storage type.
static int decode_field(const bbq_field_capture* ft, const uint8_t* buf,
                        const uint8_t* kinds, uint32_t ntypes,
                        jav_valtype_t* w, uint32_t* tidx, uint8_t* pack, uint8_t* mut) {
    const bbq_field_capture* st = jav_view_field(ft, "storage");
    if (!st || !decode_storagetype(st, buf, kinds, ntypes, w, tidx, pack)) return 0;
    if (mut) *mut = (uint8_t)bbq_node_int(jav_view_field(ft, "mut"), buf);
    return 1;
}
static int decode_structtype(const bbq_field_capture* payload, const uint8_t* buf,
                             const uint8_t* kinds, uint32_t ntypes, bbq_arena* arena,
                             jav_structtype_t* out, const uint8_t** out_packs) {
    const bbq_field_capture* fa = jav_view_field(payload, "fields");
    int n = fa ? fa->child_count : 0;
    out->nfields = (unsigned)n;
    if (n == 0) { out->fields = NULL; out->field_tidx = NULL; out->field_mut = NULL; *out_packs = NULL; return 1; }
    jav_valtype_t* fs = bbq_arena_alloc(arena, (size_t)n * sizeof *fs);
    uint32_t*      fx = bbq_arena_alloc(arena, (size_t)n * sizeof *fx);
    uint8_t*       mu = bbq_arena_alloc(arena, (size_t)n * sizeof *mu);
    uint8_t*       pk = bbq_arena_alloc(arena, (size_t)n * sizeof *pk);
    for (int i = 0; i < n; i++)
        if (!decode_field(&fa->children[i], buf, kinds, ntypes, &fs[i], &fx[i], &pk[i], &mu[i])) return 0;
    out->fields = fs; out->field_tidx = fx; out->field_mut = mu; *out_packs = pk;
    return 1;
}
static int decode_arraytype(const bbq_field_capture* payload, const uint8_t* buf,
                            const uint8_t* kinds, uint32_t ntypes, bbq_arena* arena,
                            jav_arraytype_t* out, const uint8_t** out_packs) {
    const bbq_field_capture* f = jav_view_field(payload, "field");
    if (!f) return 0;
    uint8_t* pk = bbq_arena_alloc(arena, sizeof *pk);
    if (!decode_field(f, buf, kinds, ntypes, &out->elem, &out->elem_tidx, &pk[0], &out->elem_mut)) return 0;
    *out_packs = pk;
    return 1;
}

typedef struct typemember typemember_t;   // defined below (the flattened type member)
// ── §3.3.10 / §3.1.3 type canonicalization (closure) ───────────────────────────
// Two defined types match iff their CLOSED forms are syntactically equal (§3.3.10).
// We roll up each rec group: intra-group type refs become positional rec.i, inter-group
// refs become the referenced type's already-computed canonical id; then serialize the
// whole group and intern per (group-structure, position) → a canonical id. So
// canon[a] == canon[b] ⇔ clos(a) = clos(b), exactly the spec's iso-recursive equality.
static void cpush(uint8_t** b, uint8_t v) { bbq_vec_push(*b, v); }
static void cpush32(uint8_t** b, uint32_t v) { for (int i = 0; i < 4; i++) { uint8_t x = (uint8_t)(v >> (i*8)); bbq_vec_push(*b, x); } }
// A heaptype reference: abstract HT_* code 'A'; intra-rec-group concrete → 'R' rec.(pos);
// inter-group concrete → 'C' canon[j] (j is in an earlier group, already canonicalized).
static void canon_ref(uint8_t** b, int32_t ht, uint32_t g, uint32_t gn, const int32_t* canon) {
    if (ht < 0)                                          { cpush(b, 'A'); cpush32(b, (uint32_t)ht); }
    else if ((uint32_t)ht >= g && (uint32_t)ht < g + gn) { cpush(b, 'R'); cpush32(b, (uint32_t)ht - g); }
    else                                                 { cpush(b, 'C'); cpush32(b, (uint32_t)canon[ht]); }
}
static void canon_vt(uint8_t** b, jav_valtype_t w, uint32_t tidx, uint32_t g, uint32_t gn, const int32_t* canon) {
    cpush(b, (uint8_t)w);
    if (w == WVT_REF || w == WVT_REF_NN) canon_ref(b, (int32_t)tidx, g, gn, canon);
}
static void canon_member(uint8_t** b, const jav_modidx_t* o, uint32_t t, uint32_t g, uint32_t gn, const int32_t* canon) {
    cpush(b, o->kinds[t]); cpush(b, o->finality[t]);
    if (o->supers[t] >= 0) { cpush(b, 1); canon_ref(b, o->supers[t], g, gn, canon); } else cpush(b, 0);
    const uint8_t* pk = o->type_field_packs ? o->type_field_packs[t] : NULL;
    if (o->kinds[t] == WST_STRUCT) {
        const jav_structtype_t* st = &o->structtypes[t];
        cpush32(b, st->nfields);
        for (unsigned i = 0; i < st->nfields; i++) {
            cpush(b, st->field_mut ? st->field_mut[i] : 0); cpush(b, pk ? pk[i] : 0);
            canon_vt(b, st->fields[i], st->field_tidx ? st->field_tidx[i] : 0, g, gn, canon);
        }
    } else if (o->kinds[t] == WST_ARRAY) {
        const jav_arraytype_t* a = &o->arraytypes[t];
        cpush(b, a->elem_mut); cpush(b, pk ? pk[0] : 0);
        canon_vt(b, a->elem, a->elem_tidx, g, gn, canon);
    } else {
        const jav_functype_t* ft = &o->functypes[t];
        cpush32(b, ft->nparams);
        for (uint16_t i = 0; i < ft->nparams; i++) canon_vt(b, ft->params[i], ft->param_tidx ? ft->param_tidx[i] : 0, g, gn, canon);
        cpush32(b, ft->nresults);
        for (uint16_t i = 0; i < ft->nresults; i++) canon_vt(b, ft->results[i], ft->result_tidx ? ft->result_tidx[i] : 0, g, gn, canon);
    }
}
typedef struct { const uint8_t* k; uint32_t n; } canonkey_t;
static int32_t canon_intern(canonkey_t** keys, const uint8_t* k, uint32_t n, bbq_arena* arena) {
    for (uint32_t i = 0; i < (uint32_t)bbq_vec_len(*keys); i++)
        if ((*keys)[i].n == n && memcmp((*keys)[i].k, k, n) == 0) return (int32_t)i;
    uint8_t* copy = bbq_arena_alloc(arena, n ? n : 1); memcpy(copy, k, n);
    canonkey_t e = { copy, n }; bbq_vec_push(*keys, e);
    return (int32_t)bbq_vec_len(*keys) - 1;
}

// ── §5.3.7/5.3.8 flatten the type section into one member per typeidx ──────────
// Each rec-group member becomes a typeidx in module order: its comptype discriminant
// byte, the comptype payload node (params/results live under it for a func), and its
// declared supertype (or -1).
// `group_start` = the typeidx of the first member of this type's rec group (§2.5.10: a bare
// comptype/sub type is a singleton rec group; a 0x4E group's members share one) — for the
// §3.3.10 closure (intra-group refs roll up to positional rec.i).
struct typemember { uint8_t disc; const bbq_field_capture* payload; int32_t super; uint8_t final; uint8_t nsupers; uint32_t group_start; };

static uint8_t kind_of(uint8_t disc) {
    return disc == 0x60 ? WST_FUNC : disc == 0x5F ? WST_STRUCT : WST_ARRAY;
}
static void push_sub(typemember_t** vec, const bbq_field_capture* sub, const uint8_t* buf, uint8_t final, uint32_t gstart) {
    const bbq_field_capture* sup = jav_view_field(sub, "supers");          // §5.3.8 sub type
    uint8_t ns = (sup && sup->child_count > 0) ? (uint8_t)(sup->child_count > 255 ? 255 : sup->child_count) : 0;
    int32_t s = ns ? (int32_t)bbq_node_int(&sup->children[0], buf) : -1;
    const bbq_field_capture* ct = jav_view_field(sub, "body");            // CompType {head, body}
    typemember_t m = { (uint8_t)bbq_node_int(jav_view_field(ct, "head"), buf), jav_view_field(ct, "body"), s, final, ns, gstart };
    bbq_vec_push(*vec, m);
}
// Collect every type-section member into *vec; 1 on success, 0 fail-closed.
static int collect_types(const bbq_field_capture* types, const uint8_t* buf, typemember_t** vec) {
    int count = types ? types->child_count : 0;
    for (int i = 0; i < count; i++) {
        const bbq_field_capture* rt = &types->children[i];     // §5.5.4 rectype
        uint8_t h = (uint8_t)bbq_node_int(jav_view_field(rt, "head"), buf);
        const bbq_field_capture* body = jav_view_field(rt, "body");
        uint32_t gstart = (uint32_t)bbq_vec_len(*vec);          // this top-level entry = one rec group
        if (h == 0x5E || h == 0x5F || h == 0x60) {             // bare comptype shorthand = sub final, no supers
            typemember_t m = { h, body, -1, 1, 0, gstart }; bbq_vec_push(*vec, m);
        } else if (h == 0x4F || h == 0x50) {                   // explicit sub type (0x4F final / 0x50 open)
            push_sub(vec, body, buf, h == 0x4F, gstart);
        } else if (h == 0x4E) {                                // recursive group
            const bbq_field_capture* members = jav_view_field(body, "members");
            int mc = members ? members->child_count : 0;
            for (int j = 0; j < mc; j++) {
                const bbq_field_capture* rm = &members->children[j];
                uint8_t mh = (uint8_t)bbq_node_int(jav_view_field(rm, "head"), buf);
                const bbq_field_capture* mb = jav_view_field(rm, "body");
                if (mh == 0x4F || mh == 0x50) push_sub(vec, mb, buf, mh == 0x4F, gstart);
                else { typemember_t m = { mh, mb, -1, 1, 0, gstart }; bbq_vec_push(*vec, m); }
            }
        } else return 0;                                       // unknown leading byte
    }
    return 1;
}

// Compute the §3.3.10 canonical id per typeidx (placed here: needs the complete typemember).
static void compute_canon(jav_modidx_t* out, const typemember_t* mem, bbq_arena* arena) {
    uint32_t nt = out->ntypes;
    if (!nt) return;
    int32_t* canon = bbq_arena_alloc(arena, nt * sizeof *canon);
    canonkey_t* keys = NULL; uint8_t* gbuf = NULL; uint8_t* mbuf = NULL;
    for (uint32_t t = 0; t < nt; ) {
        uint32_t g = mem[t].group_start, gn = 0;
        while (g + gn < nt && mem[g + gn].group_start == g) gn++;
        bbq_vec_clear(gbuf);
        for (uint32_t m = 0; m < gn; m++) canon_member(&gbuf, out, g + m, g, gn, canon);  // roll up the whole group
        for (uint32_t m = 0; m < gn; m++) {                                                // intern per (group, position)
            bbq_vec_clear(mbuf);
            for (uint32_t i = 0; i < (uint32_t)bbq_vec_len(gbuf); i++) bbq_vec_push(mbuf, gbuf[i]);
            cpush32(&mbuf, m);
            canon[g + m] = canon_intern(&keys, mbuf, (uint32_t)bbq_vec_len(mbuf), arena);
        }
        t = g + gn;
    }
    bbq_vec_free(gbuf); bbq_vec_free(mbuf); bbq_vec_free(keys);
    out->canon = canon;
}

// ── §4.5.2 session-global closed-type registry ──────────────────────────────────
// The same closure serialization as compute_canon, but interning into a SHARED table so closed
// types from different modules land on one id space. The back-ref array passed to canon_member is
// the GLOBAL gcanon (not a module-local canon), so a 'C' inter-group ref serializes to the
// referent's global id — making structurally-equal closed types byte-identical ACROSS modules.
struct jav_typereg {
    canonkey_t* keys;    // bbq_vec — global canonical id = index into this table
    uint8_t*    kinds;   // bbq_vec[gid] — WST_* of the canonical type
    int32_t*    supers;  // bbq_vec[gid] — global id of the declared supertype, or -1
    struct gc_rtt** rtts;// bbq_vec[gid] — the interned, store-owned rtt for this type (NULL for func types).
                         // Objects carry o->rtt into this table, which outlives any module's arena — so a
                         // retained object's rtt stays valid after wasm_module_delete frees the module.
    bbq_arena   arena;   // owns the interned key bytes AND the interned rtts (outlives any module's arena)
};

jav_typereg_t* jav_typereg_new(void) {
    jav_typereg_t* r = calloc(1, sizeof *r);
    if (r) bbq_arena_init(&r->arena, 0);
    return r;
}
// Lazily create the heap's shared registry (the loader owns this — keeps the engine teardown free
// of any jav_typereg_* symbol; it calls back through heap->typereg_free). Declared in heap.h.
/* §4.5.2 callbacks the interp calls for a cross-instance/host funcref closed-type match, wired
 * onto the heap so jav_runtime.o links no jav_typereg_* symbol (the one-way arrow). */
static int      typereg_gid_sub_cb(jav_typereg_t* r, int32_t prov, int32_t req) {
    jav_subtype_ctx_t gl = jav_typereg_lattice(r);
    return jav_ht_sub(&gl, prov, req);
}
static int32_t  typereg_intern_ft_cb(jav_typereg_t* r, const struct jav_functype* ft) {
    return jav_typereg_intern_functype(r, ft);
}
jav_typereg_t* jav_heap_typereg(heap_t* heap) {
    if (!heap->typereg) {
        heap->typereg = jav_typereg_new(); heap->typereg_free = jav_typereg_free;
        heap->typereg_gid_sub = typereg_gid_sub_cb; heap->typereg_intern_ft = typereg_intern_ft_cb;
    }
    return heap->typereg;
}
void jav_typereg_free(jav_typereg_t* r) {
    if (!r) return;
    bbq_vec_free(r->keys); bbq_vec_free(r->kinds); bbq_vec_free(r->supers); bbq_vec_free(r->rtts);
    bbq_arena_free(&r->arena);   // frees the interned key bytes AND the interned rtt copies
    free(r);
}
jav_subtype_ctx_t jav_typereg_lattice(const jav_typereg_t* r) {
    // canon = NULL ⇒ concrete equality is global-id identity (the ids ARE the canonical forms).
    jav_subtype_ctx_t cx = { r->kinds, r->supers, (uint32_t)bbq_vec_len(r->keys), NULL };
    return cx;
}
// The persistent, store-owned rtt for a closed-type global id (NULL for func types / out of range).
// An instance points its ctx.struct_rtts entries here so o->rtt survives the defining module's teardown.
const struct gc_rtt* jav_typereg_rtt(const jav_typereg_t* r, int32_t gid) {
    return (gid >= 0 && gid < (int32_t)bbq_vec_len(r->rtts)) ? r->rtts[gid] : NULL;
}

void jav_typereg_absorb(jav_typereg_t* r, const jav_modidx_t* mod, int32_t* gcanon) {
    uint32_t nt = mod->ntypes;
    uint8_t* gbuf = NULL; uint8_t* mbuf = NULL;
    for (uint32_t t = 0; t < nt; ) {
        uint32_t g = mod->group_start[t], gn = 0;
        while (g + gn < nt && mod->group_start[g + gn] == g) gn++;
        bbq_vec_clear(gbuf);
        for (uint32_t m = 0; m < gn; m++) canon_member(&gbuf, mod, g + m, g, gn, gcanon);  // back-refs = GLOBAL ids
        for (uint32_t m = 0; m < gn; m++) {
            bbq_vec_clear(mbuf);
            for (uint32_t i = 0; i < (uint32_t)bbq_vec_len(gbuf); i++) bbq_vec_push(mbuf, gbuf[i]);
            cpush32(&mbuf, m);
            int32_t before = (int32_t)bbq_vec_len(r->keys);
            int32_t gid = canon_intern(&r->keys, mbuf, (uint32_t)bbq_vec_len(mbuf), &r->arena);
            gcanon[g + m] = gid;
            if (gid == before) {   // newly interned → record its lattice metadata (kind + GLOBAL super)
                bbq_vec_push(r->kinds, mod->kinds[g + m]);
                bbq_vec_push(r->supers, mod->supers[g + m] >= 0 ? gcanon[mod->supers[g + m]] : (int32_t)-1);
                // Intern this type's rtt into the store-owned arena (persistent). Objects carry o->rtt
                // into this table, so it must outlive any module's arena; the flexible ref_offsets[]
                // trails the struct contiguously, so one memcpy of size+nrefs copies it whole. Kept
                // parallel to r->keys (pushed only on a new gid), so gid indexes it directly.
                const gc_rtt_t* src = mod->rtts ? (const gc_rtt_t*)mod->rtts[g + m] : NULL;
                gc_rtt_t* rc = NULL;
                if (src) {
                    size_t nb = sizeof(gc_rtt_t) + (src->kind == GC_KIND_STRUCT ? (size_t)src->nrefs : 0) * sizeof(uint32_t);
                    rc = bbq_arena_alloc(&r->arena, nb);
                    memcpy(rc, src, nb);
                    /* field_off points into the SOURCE module's arena (doomed) —
                     * deep-copy it into the store arena alongside the rtt. */
                    if (src->field_off) {
                        size_t ob = ((size_t)src->nfields + 1) * sizeof(uint32_t);
                        uint32_t* oc = bbq_arena_alloc(&r->arena, ob);
                        memcpy(oc, src->field_off, ob);
                        rc->field_off = oc;
                    }
                    rc->gid = gid;
                }
                bbq_vec_push(r->rtts, rc);
            }
        }
        t = g + gn;
    }
    bbq_vec_free(gbuf); bbq_vec_free(mbuf);
}

int32_t jav_typereg_intern_functype(jav_typereg_t* r, const jav_functype_t* ft) {
    for (uint16_t i = 0; i < ft->nparams; i++)   // host funcs carry no concrete refs (no module type space)
        if ((ft->params[i] == WVT_REF || ft->params[i] == WVT_REF_NN) &&
            (int32_t)(ft->param_tidx ? ft->param_tidx[i] : 0) >= 0) return -1;
    for (uint16_t i = 0; i < ft->nresults; i++)
        if ((ft->results[i] == WVT_REF || ft->results[i] == WVT_REF_NN) &&
            (int32_t)(ft->result_tidx ? ft->result_tidx[i] : 0) >= 0) return -1;
    // Replicate compute_canon's serialization of a singleton `sub final (func …)` rec group so a
    // host functype matches a wasm bare-comptype func import byte-for-byte → same global id.
    uint8_t* m = NULL;
    cpush(&m, (uint8_t)WST_FUNC); cpush(&m, 1); cpush(&m, 0);   // kind, final, no super
    cpush32(&m, ft->nparams);
    for (uint16_t i = 0; i < ft->nparams; i++) canon_vt(&m, ft->params[i], ft->param_tidx ? ft->param_tidx[i] : 0, 0, 0, NULL);
    cpush32(&m, ft->nresults);
    for (uint16_t i = 0; i < ft->nresults; i++) canon_vt(&m, ft->results[i], ft->result_tidx ? ft->result_tidx[i] : 0, 0, 0, NULL);
    cpush32(&m, 0);   // singleton group position
    int32_t before = (int32_t)bbq_vec_len(r->keys);
    int32_t gid = canon_intern(&r->keys, m, (uint32_t)bbq_vec_len(m), &r->arena);
    if (gid == before) { bbq_vec_push(r->kinds, (uint8_t)WST_FUNC); bbq_vec_push(r->supers, (int32_t)-1); }
    bbq_vec_free(m);
    return gid;
}

// §3.5.1: types are validated incrementally — when the rec group at index x is checked,
// the context holds types [0, x+gn). A concrete reference to a typeidx ≥ the END of the
// referencing type's own rec group points at a not-yet-defined (later) group, which is
// out of scope. Returns 1 if every reference is in scope, 0 on a forward reference.
static int concrete_ref(jav_valtype_t w, uint32_t tx) {
    return (w == WVT_REF || w == WVT_REF_NN) && (int32_t)tx >= 0;
}
static int type_refs_in_scope(const jav_modidx_t* out, const typemember_t* mem, uint32_t nt) {
    for (uint32_t t = 0; t < nt; t++) {
        uint32_t g = mem[t].group_start, gend = g;
        while (gend < nt && mem[gend].group_start == g) gend++;     // exclusive end of t's rec group
        if (out->kinds[t] == WST_FUNC) {
            const jav_functype_t* ft = &out->functypes[t];
            for (uint16_t i = 0; i < ft->nparams; i++)
                if (concrete_ref(ft->params[i], ft->param_tidx[i]) && ft->param_tidx[i] >= gend) return 0;
            for (uint16_t i = 0; i < ft->nresults; i++)
                if (concrete_ref(ft->results[i], ft->result_tidx[i]) && ft->result_tidx[i] >= gend) return 0;
        } else if (out->kinds[t] == WST_STRUCT) {
            const jav_structtype_t* s = &out->structtypes[t];
            for (unsigned i = 0; i < s->nfields; i++)
                if (s->field_tidx && concrete_ref(s->fields[i], s->field_tidx[i]) && s->field_tidx[i] >= gend) return 0;
        } else {
            const jav_arraytype_t* a = &out->arraytypes[t];
            if (concrete_ref(a->elem, a->elem_tidx) && a->elem_tidx >= gend) return 0;
        }
    }
    return 1;
}

// §5.3.9 limits: flag byte (bit0 = has max, bit2 = i64 address type), min, then max.
static void decode_limits(const bbq_field_capture* lim, const uint8_t* buf,
                          uint64_t* min, uint64_t* max, uint8_t* has_max, uint8_t* is64) {
    uint8_t flag = (uint8_t)bbq_node_int(jav_view_field(lim, "flag"), buf);
    *min     = (uint64_t)bbq_node_int(jav_view_field(lim, "min"), buf);
    *has_max = (flag & 0x01) != 0;
    *max     = *has_max ? (uint64_t)bbq_node_int(jav_view_field(lim, "max"), buf) : 0;
    if (is64) *is64 = (flag & 0x04) != 0;
}

int jav_index_decode_valtype(const bbq_field_capture* vt, const uint8_t* base,
                             const jav_modidx_t* mod, jav_valtype_t* w, uint32_t* tidx) {
    return decode_valtype(vt, base, mod->kinds, mod->ntypes, w, tidx);
}

// The §3.1.6 module-level validation context — a projection of the flattened index, shared
// by the validator (§7 body checks) and the instantiator (re-deriving each body's
// side-table). Per-function locals/results are filled per body by the caller.
jav_vctx_t jav_module_cx(const jav_modidx_t* mod) {
    jav_vctx_t cx = {0};
    cx.types = mod->functypes; cx.ntypes = mod->ntypes;
    cx.func_sigs = mod->func_sigs; cx.nfuncs = mod->nfuncs;
    cx.func_type_idx = mod->func_type_idx;
    cx.globals = mod->global_types; cx.global_tidx = mod->global_tidx; cx.nglobals = mod->nglobals;
    cx.global_mut = mod->global_mut;
    cx.ntables = mod->ntables; cx.table_reftype = mod->table_reftype;
    cx.table_tidx = mod->table_tidx; cx.table_is64 = mod->table_is64;
    cx.nmemories = mod->nmems; cx.mem_is64 = mod->mem_is64;
    cx.tags = mod->tags; cx.ntags = mod->ntags;
    cx.ndatas = mod->ndatas; cx.nelems = mod->nelems;
    cx.elem_reftype = mod->elem_reftype; cx.elem_tidx = mod->elem_tidx;
    cx.structtypes = mod->structtypes; cx.nstructtypes = mod->ntypes;
    cx.arraytypes = mod->arraytypes;   cx.narraytypes = mod->ntypes;
    cx.type_field_packs = mod->type_field_packs; cx.num_type_field_packs = mod->ntypes;
    cx.lattice = &mod->lattice;
    return cx;
}

// §4.5.3 lower the (validated) composite types into the collector's per-typeidx rtt: a struct's
// ref-field byte offsets + size, an array's element shape (elem_is_ref, 8-byte slot stride). The
// generic GC consumes these for tracing/sizing; func typeidx ⇒ NULL. Arena-owned (module arena).
/* Is a (w, heaptype) ref field/element a GC-HEAP reference the tracer must follow? Derived from
 * §4.2.1's value production per heaptype, with NO default arm — the previous catch-all default is
 * exactly how `i31` was silently misclassified (traced as a pointer -> VM SIGSEGV), so every
 * heaptype answers here explicitly or the build fails.
 *
 * The rule: traced iff the type can be INHABITED by a value carrying a gc_obj_t address.
 * javelina's representation contract for slots in the any/extern hierarchies (§2.3.4: the two are
 * "inhabited by an isomorphic set of values" and interconverted by IDENTITY here — wasm.def's
 * extern.convert_any is `result = a;`) is that every non-null inhabitant is one of:
 *   - a gc_obj_t*            (ref.struct / ref.array / ref.exn, or ref.extern wrapping one)
 *   - a HOST BOX, also a gc_obj_t* (jav_host_box_new — the ONLY way a §4.2.1 ref.host enters the
 *     VM; the c-api boxes at every entry point, wasm_capi.c val_to_slot / global write)
 *   - a tagged ref.i31       ((v << 1) | 1 per §2.3.4's reserved bit — odd, never 8-aligned)
 *   - ref.null               (0)
 * gc_mark1 skips the last two by value (null / alignment), so tracing a union-typed slot is safe
 * for every inhabitant. A RAW host pointer in such a slot would violate the boxing contract and
 * is not a state any supported entry produces. */
static int rtt_field_is_ref(jav_valtype_t w, uint32_t tidx, const uint8_t* kinds, uint32_t ntypes) {
    if (w != WVT_REF && w != WVT_REF_NN) return 0;
    int32_t ht = (int32_t)tidx;
    if (ht >= 0)   /* concrete typeidx: a GC ref iff it names a struct/array, not a func type */
        return (uint32_t)ht < ntypes && (kinds[ht] == WST_STRUCT || kinds[ht] == WST_ARRAY);
    switch (ht) {
        case HT_ANY: case HT_EQ:           /* admit ref.struct/ref.array (+ ref.i31, skipped by value) */
        case HT_STRUCT: case HT_ARRAY:     /* structaddr / arrayaddr */
        case HT_EXN:                       /* exnaddr — an exn instance is a heap object here */
        case HT_EXTERN:                    /* ref.extern wrapping an aggregate (identity conversion),
                                            * or a host box — BOTH gc_obj_t. Returning 0 here was a
                                            * use-after-free: a live struct behind extern.convert_any
                                            * was never traced. Pinned by test_gc_refforms. */
            return 1;
        case HT_I31:                       /* ref.i31 — a 31-bit integer, never an address */
        case HT_FUNC: case HT_NOFUNC:      /* ref.func — a funcaddr (&ctx->functions[i]), not gc */
        case HT_NONE: case HT_NOEXTERN:    /* bottom types — only ref.null inhabits */
        case HT_NOEXN:
            return 0;
        default:                           /* a heaptype this function does not classify is a BUG,
                                            * not a guess — fail loudly at module-index time */
            return -1;
    }
}
/* Returns 0 (refuse the module) if any field/element names a heaptype rtt_field_is_ref does not
 * classify — an unclassified heaptype must never fall back to a guessed trace bit. */
static int build_rtts(jav_modidx_t* out, bbq_arena* arena) {
    out->rtts = bbq_arena_alloc(arena, out->ntypes * sizeof *out->rtts);
    for (uint32_t t = 0; t < out->ntypes; t++) {
        gc_rtt_t* r = NULL;
        if (out->kinds[t] == WST_STRUCT) {
            const jav_structtype_t* st = &out->structtypes[t];
            uint32_t nrefs = 0; int has_wide = 0;
            for (unsigned i = 0; i < st->nfields; i++) {
                int fr = rtt_field_is_ref(st->fields[i], st->field_tidx ? st->field_tidx[i] : 0, out->kinds, out->ntypes);
                if (fr < 0) return 0;              /* unclassified heaptype: refuse the module */
                if (fr) nrefs++;
                if (st->fields[i] == WVT_V128) has_wide = 1;
            }
            r = bbq_arena_alloc(arena, sizeof(gc_rtt_t) + (size_t)nrefs * sizeof(uint32_t));
            r->nfields = (uint16_t)st->nfields;
            /* Field layout: 8-byte cells, except a v128 field takes 16. The offset
             * table is materialized only when a wide field exists — NULL keeps the
             * uniform-cell fast path for every pre-v128 struct. */
            uint32_t* off = NULL;
            if (has_wide) {
                off = bbq_arena_alloc(arena, ((size_t)st->nfields + 1) * sizeof(uint32_t));
                uint32_t o2 = 0;
                for (unsigned i = 0; i < st->nfields; i++) {
                    off[i] = o2;
                    /* cell = the 8-byte slot floor, widened per the ONE size map
                     * (jav_valtype_size: v128 = 16, everything else ≤ 8). */
                    uint32_t w = jav_valtype_size(st->fields[i]);
                    o2 += w > 8u ? w : 8u;
                }
                off[st->nfields] = o2;
            }
            r->field_off = off;
            r->size = (uint32_t)sizeof(gc_obj_t) + (off ? off[st->nfields] : st->nfields * 8u);
            r->nrefs = nrefs; r->kind = GC_KIND_STRUCT; r->elem_is_ref = 0;
            r->elem_store_w = 0; r->elem_heap_w = 0;
            uint32_t k = 0;
            for (unsigned i = 0; i < st->nfields; i++)
                if (rtt_field_is_ref(st->fields[i], st->field_tidx ? st->field_tidx[i] : 0, out->kinds, out->ntypes) > 0)
                    r->ref_offsets[k++] = (uint32_t)sizeof(gc_obj_t) + (off ? off[i] : (uint32_t)i * 8u);
        } else if (out->kinds[t] == WST_ARRAY) {
            const jav_arraytype_t* at = &out->arraytypes[t];
            int er = rtt_field_is_ref(at->elem, at->elem_tidx, out->kinds, out->ntypes);
            if (er < 0) return 0;                  /* unclassified heaptype: refuse the module */
            r = bbq_arena_alloc(arena, sizeof(gc_rtt_t));
            r->size = (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET;   // base; per-new grows by len*elem_heap_w
            r->nrefs = 0; r->nfields = 0; r->kind = GC_KIND_ARRAY; r->field_off = NULL;
            r->elem_is_ref = (uint8_t)er;
            uint8_t pk = (out->type_field_packs && out->type_field_packs[t]) ? out->type_field_packs[t][0] : 0;
            r->elem_store_w = pk ? pk                                  // packed i8 (1) / i16 (2) — the data-segment stride
                : (uint8_t)(at->elem == WVT_V128 ? 16                  // §3.4.7 array.new_data accepts vectype: 16-byte stride
                : (at->elem == WVT_I64 || at->elem == WVT_F64) ? 8 : 4);
            /* heap cell = the 8-byte slot floor, widened per the ONE size map. */
            { uint32_t w = jav_valtype_size(at->elem);
              r->elem_heap_w = (uint8_t)(w > 8u ? w : GC_ARRAY_ELEM_BYTES); }
        }
        if (r) r->gid = -1;   // §4.5.2 store-global id, assigned when this module is absorbed (jav_instance)
        out->rtts[t] = (struct gc_rtt*)r;   // NULL for a func typeidx
    }
    return 1;
}

int jav_module_index(const bbq_field_capture* root, const uint8_t* base,
                     bbq_arena* arena, jav_modidx_t* out) {
    memset(out, 0, sizeof *out);

    // ── type section (id 1): flatten the typeidx space, then decode func sigs ──
    const bbq_field_capture* tsec = jav_view_find_section(root, 1, base);
    const bbq_field_capture* types = tsec ? jav_view_field(jav_view_field(tsec, "body"), "types") : NULL;
    typemember_t* mem = NULL;
    if (!collect_types(types, base, &mem)) { bbq_vec_free(mem); return 0; }
    uint32_t nt = (uint32_t)bbq_vec_len(mem);
    out->ntypes = nt;
    if (nt) {
        out->kinds     = bbq_arena_alloc(arena, nt * sizeof *out->kinds);
        out->supers    = bbq_arena_alloc(arena, nt * sizeof *out->supers);
        out->finality  = bbq_arena_alloc(arena, nt * sizeof *out->finality);
        out->nsupers   = bbq_arena_alloc(arena, nt * sizeof *out->nsupers);
        uint32_t* gstart = bbq_arena_alloc(arena, nt * sizeof *gstart); out->group_start = gstart;
        out->functypes = bbq_arena_alloc(arena, nt * sizeof *out->functypes);
        out->structtypes = bbq_arena_alloc(arena, nt * sizeof *out->structtypes);
        out->arraytypes  = bbq_arena_alloc(arena, nt * sizeof *out->arraytypes);
        out->type_field_packs = bbq_arena_alloc(arena, nt * sizeof *out->type_field_packs);
        memset(out->functypes, 0, nt * sizeof *out->functypes);
        memset(out->structtypes, 0, nt * sizeof *out->structtypes);
        memset(out->arraytypes, 0, nt * sizeof *out->arraytypes);
        memset(out->type_field_packs, 0, nt * sizeof *out->type_field_packs);
        for (uint32_t t = 0; t < nt; t++) {
            out->kinds[t]    = kind_of(mem[t].disc);
            out->supers[t]   = mem[t].super;
            out->finality[t] = mem[t].final;
            out->nsupers[t]  = mem[t].nsupers;
            gstart[t]        = mem[t].group_start;
        }
        // kinds are complete now — decode each composite type (concrete refs resolve).
        for (uint32_t t = 0; t < nt; t++) {
            if (out->kinds[t] == WST_FUNC) {
                jav_functype_t* ft = &out->functypes[t];
                if (!decode_resulttype(jav_view_field(mem[t].payload, "params"), base, out->kinds, nt, arena,
                                       &ft->params, &ft->param_tidx, &ft->nparams) ||
                    !decode_resulttype(jav_view_field(mem[t].payload, "results"), base, out->kinds, nt, arena,
                                       &ft->results, &ft->result_tidx, &ft->nresults)) {
                    bbq_vec_free(mem); return 0;
                }
            } else if (out->kinds[t] == WST_STRUCT) {
                if (!decode_structtype(mem[t].payload, base, out->kinds, nt, arena,
                                       &out->structtypes[t], &out->type_field_packs[t])) { bbq_vec_free(mem); return 0; }
            } else {   // WST_ARRAY
                if (!decode_arraytype(mem[t].payload, base, out->kinds, nt, arena,
                                      &out->arraytypes[t], &out->type_field_packs[t])) { bbq_vec_free(mem); return 0; }
            }
        }
        if (!type_refs_in_scope(out, mem, nt)) { bbq_vec_free(mem); return 0; }   // §3.5.1 no forward refs
        compute_canon(out, mem, arena);   // §3.3.10 closure: canon[] over the full (now-decoded) type space
        if (!build_rtts(out, arena)) return 0;   // §4.5.3 GC rtt descriptors; an unclassifiable heaptype refuses the module
    }
    bbq_vec_free(mem);

    // ── the five index spaces: imports fill the low slots, then defining sections ──
    const bbq_field_capture* isec = jav_view_find_section(root, 2, base);
    const bbq_field_capture* imports = isec ? jav_view_field(jav_view_field(isec, "body"), "imports") : NULL;
    const bbq_field_capture* d_func = jav_view_section_array(root, 3, "type_indices", base);
    const bbq_field_capture* d_tab  = jav_view_section_array(root, 4, "tables", base);
    const bbq_field_capture* d_mem  = jav_view_section_array(root, 5, "mems", base);
    const bbq_field_capture* d_glob = jav_view_section_array(root, 6, "globals", base);
    const bbq_field_capture* d_tag  = jav_view_section_array(root, 13, "tags", base);
    uint32_t ic = imports ? (uint32_t)imports->child_count : 0;

    // count imports per kind (§5.5.5 desc: 0 func, 1 table, 2 mem, 3 global, 4 tag)
    uint32_t ni[5] = {0,0,0,0,0};
    for (uint32_t i = 0; i < ic; i++) {
        uint8_t k = (uint8_t)bbq_node_int(jav_view_field(jav_view_field(&imports->children[i], "desc"), "kind"), base);
        if (k <= 4) ni[k]++;
    }
    out->nimport_funcs   = ni[0];  out->nfuncs   = ni[0] + jav_view_nchild(d_func);
    out->nimport_tables  = ni[1];  out->ntables  = ni[1] + jav_view_nchild(d_tab);
    out->nimport_mems    = ni[2];  out->nmems    = ni[2] + jav_view_nchild(d_mem);
    out->nimport_globals = ni[3];  out->nglobals = ni[3] + jav_view_nchild(d_glob);
    out->nimport_tags    = ni[4];  out->ntags    = ni[4] + jav_view_nchild(d_tag);

    #define ALLOC(field, n) (out->field = bbq_arena_alloc(arena, (n) * sizeof *out->field))
    if (out->nfuncs)   { ALLOC(func_type_idx, out->nfuncs); ALLOC(func_sigs, out->nfuncs); }
    if (out->ntables)  { ALLOC(table_reftype, out->ntables); ALLOC(table_tidx, out->ntables);
                         ALLOC(table_min, out->ntables); ALLOC(table_max, out->ntables);
                         ALLOC(table_has_max, out->ntables); ALLOC(table_is64, out->ntables); }
    if (out->nmems)    { ALLOC(mem_min, out->nmems); ALLOC(mem_max, out->nmems);
                         ALLOC(mem_has_max, out->nmems); ALLOC(mem_is64, out->nmems); }
    if (out->nglobals) { ALLOC(global_types, out->nglobals); ALLOC(global_tidx, out->nglobals);
                         ALLOC(global_mut, out->nglobals); ALLOC(global_is_import, out->nglobals); }
    if (out->ntags)    { ALLOC(tags, out->ntags); ALLOC(tag_typeidx, out->ntags); }
    #undef ALLOC

    // import-decl decoders, writing into a given slot of each space.
    uint32_t f = 0, t_ = 0, m = 0, g = 0, e = 0;  // per-space fill cursors
    #define GLOBAL_AT(slot, gt, imp) do {                                                  \
        if (!decode_valtype(jav_view_field(gt, "type"), base, out->kinds, nt,                          \
                            &out->global_types[slot], &out->global_tidx[slot])) return 0;   \
        out->global_mut[slot] = (uint8_t)bbq_node_int(jav_view_field(gt, "mut"), base);                \
        out->global_is_import[slot] = (imp); } while (0)
    #define TABLE_AT(slot, tt) do {                                                         \
        if (!decode_valtype(jav_view_field(tt, "reftype"), base, out->kinds, nt,                       \
                            &out->table_reftype[slot], &out->table_tidx[slot])) return 0;   \
        decode_limits(jav_view_field(tt, "limits"), base, &out->table_min[slot], &out->table_max[slot],\
                      &out->table_has_max[slot], &out->table_is64[slot]); } while (0)
    #define MEM_AT(slot, lim) decode_limits(lim, base, &out->mem_min[slot], &out->mem_max[slot], \
                                            &out->mem_has_max[slot], &out->mem_is64[slot])
    #define TAG_AT(slot, tt) (out->tag_typeidx[slot] = (uint32_t)bbq_node_int(jav_view_field(tt, "type"), base))

    for (uint32_t i = 0; i < ic; i++) {                // imports → low slots, in order
        const bbq_field_capture* desc = jav_view_field(&imports->children[i], "desc");
        const bbq_field_capture* body = jav_view_field(desc, "body");
        switch ((uint8_t)bbq_node_int(jav_view_field(desc, "kind"), base)) {
        case 0x00: out->func_type_idx[f++] = (uint32_t)bbq_node_int(jav_view_field(body, "x"), base); break;
        case 0x01: TABLE_AT(t_, body); t_++; break;    // body = TableType
        case 0x02: MEM_AT(m, body); m++; break;        // body = Limits
        case 0x03: GLOBAL_AT(g, body, 1); g++; break;  // body = GlobalType
        case 0x04: TAG_AT(e, body); e++; break;        // body = TagType
        default: return 0;
        }
    }
    for (uint32_t i = 0; i < jav_view_nchild(d_func); i++)       // defined funcs
        out->func_type_idx[f++] = (uint32_t)bbq_node_int(&d_func->children[i], base);
    for (uint32_t i = 0; i < jav_view_nchild(d_tab); i++)        // defined tables (through the switch-arm wrapper)
        { TABLE_AT(t_, defined_table_type(&d_tab->children[i])); t_++; }
    for (uint32_t i = 0; i < jav_view_nchild(d_mem); i++)        // defined mems
        { MEM_AT(m, jav_view_field(&d_mem->children[i], "limits")); m++; }
    for (uint32_t i = 0; i < jav_view_nchild(d_glob); i++)       // defined globals (init expr is Phase 2)
        { GLOBAL_AT(g, jav_view_field(&d_glob->children[i], "type"), 0); g++; }
    for (uint32_t i = 0; i < jav_view_nchild(d_tag); i++)        // defined tags
        { TAG_AT(e, &d_tag->children[i]); e++; }
    #undef GLOBAL_AT
    #undef TABLE_AT
    #undef MEM_AT
    #undef TAG_AT

    for (uint32_t i = 0; i < out->nfuncs; i++) {       // funcidx → functype (§3.2)
        uint32_t ti = out->func_type_idx[i];
        if (ti >= nt || out->kinds[ti] != WST_FUNC) return 0;
        out->func_sigs[i] = out->functypes[ti];        // flat value copy
    }
    for (uint32_t i = 0; i < out->ntags; i++) {        // tagidx → functype (results must be ε)
        uint32_t ti = out->tag_typeidx[i];
        if (ti >= nt || out->kinds[ti] != WST_FUNC) return 0;
        out->tags[i] = out->functypes[ti];
    }

    // ── segment counts (§5.5.12 / §5.5.14) and the §3.3 lattice ──
    const bbq_field_capture* elsec = jav_view_section_array(root, 9, "elems", base);
    out->nelems = jav_view_nchild(elsec);
    if (out->nelems) {
        out->elem_reftype = bbq_arena_alloc(arena, out->nelems * sizeof *out->elem_reftype);
        out->elem_tidx    = bbq_arena_alloc(arena, out->nelems * sizeof *out->elem_tidx);
        for (uint32_t i = 0; i < out->nelems; i++) {   // Elem5/6/7 carry an explicit RefType; Elem0..4 are funcref
            const bbq_field_capture* b = jav_view_field(&elsec->children[i], "body");
            const bbq_field_capture* rt = jav_view_field(b, "reftype");
            if (rt) { if (!decode_valtype(rt, base, out->kinds, nt, &out->elem_reftype[i], &out->elem_tidx[i])) return 0; }
            else {   // §5.5.12: a funcidx list (Elem0-3, `funcs` present) is (ref func) NON-NULL; a bare-expr list (Elem4) is funcref
                out->elem_reftype[i] = jav_view_field(b, "funcs") ? WVT_REF_NN : WVT_REF;
                out->elem_tidx[i] = (uint32_t)HT_FUNC;
            }
        }
    }
    out->ndatas = jav_view_nchild(jav_view_section_array(root, 11, "datas", base));
    out->lattice = (jav_subtype_ctx_t){ out->kinds, out->supers, nt, out->canon };
    return 1;
}
