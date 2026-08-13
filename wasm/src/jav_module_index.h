// jav_module_index.h — the flattened module index (§5 container → the tables §7
// validation and §4.5 instantiation share). The parse tree, here the c-lite zero-copy
// span index, is distilled ONCE into index-keyed tables so neither the validator nor
// the instantiator re-walks the tree: the flattened type space (one entry per typeidx
// across rec groups), the §3.3 lattice (kind + declared supertype per type), and the
// function signature table (imports in the low slots, then defined funcs).
//
// Allocations live in the caller's arena — the same one that owns the c-lite index —
// so the whole index frees together (the zero-copy read model).
#ifndef JAV_MODULE_INDEX_H
#define JAV_MODULE_INDEX_H

#include "bbq_lite.h"     // bbq_field_capture (the span-index node)
#include "bbq_arena.h"    // bbq_arena (table storage)
#include "validate.h"     // jav_functype_t / jav_valtype_t (the validator's view)
#include "jav_subtype.h"  // WST_* kinds (the §3.3 lattice context)
#include "jav_ttree.h"    // jav_tctx_t — the tier-2 walk's view of the same index

typedef struct {
    // Flattened composite-type space: one entry per typeidx, in module order across
    // every rec group. A WST_FUNC entry carries its full signature in `functypes[t]`;
    // WST_STRUCT/WST_ARRAY entries carry their fields in structtypes/arraytypes below.
    // `kinds`/`supers` feed jav_subtype_ctx_t.
    jav_functype_t* functypes;  // [ntypes] — params/results meaningful iff kinds[t]==WST_FUNC
    uint8_t*        kinds;       // [ntypes] — WST_STRUCT/ARRAY/FUNC
    int32_t*        supers;      // [ntypes] — declared supertype typeidx (the first), or -1
    uint8_t*        finality;    // [ntypes] — 1 if the type is `final` (no subtypes allowed)
    uint8_t*        nsupers;     // [ntypes] — declared supertype count (§3.2.11: must be ≤ 1)
    const int32_t*  canon;       // [ntypes] — §3.3.10 canonical id (closure): canon[a]==canon[b] ⇔ clos(a)=clos(b)
    const uint32_t* group_start; // [ntypes] — typeidx of the first member of t's rec group (closure groups; for jav_typereg_absorb)
    uint32_t        ntypes;

    // §2.3.9 composite structure (the GC pin): per typeidx, the unpacked field/element
    // value types + their heaptypes (parallel), and the packed storage width per field
    // (1 = i8, 2 = i16, 0 = unpacked) for the struct.get_s/u + array.get_s/u packed check.
    // Meaningful iff kinds[t] is WST_STRUCT / WST_ARRAY; func entries are zeroed/NULL.
    jav_structtype_t* structtypes;     // [ntypes]
    jav_arraytype_t*  arraytypes;      // [ntypes]
    const uint8_t**   type_field_packs; // [ntypes] — per-type pack-width array (NULL if none packed)
    // §4.5.3 GC run-time types: the collector's per-typeidx object-layout descriptor (struct field
    // ref map / array element shape), a deterministic lowering of structtypes/arraytypes. Built
    // once here (module-level, arena-owned, shared by every instance); NULL for func typeidx.
    struct gc_rtt**   rtts;            // [ntypes]

    // Every index space below is in the spec's order: the imported entries fill the low
    // slots [0, nimport_*), then the defining section's entries (§2.5.10). The flat
    // parallel arrays are the exact shapes jav_vctx_t reads.

    // funcidx → signature (a value per funcidx, the shape jav_vctx_t.func_sigs expects).
    jav_functype_t* func_sigs;      // [nfuncs] — func_sigs[f] = functypes[func_type_idx[f]]
    uint32_t*       func_type_idx;  // [nfuncs] funcidx → defining typeidx
    uint32_t        nfuncs;
    uint32_t        nimport_funcs;

    // globalidx → type (§5.5.9). init exprs (defined globals) are a Phase-2 concern.
    jav_valtype_t*  global_types;     // [nglobals] → cx.globals
    uint32_t*       global_tidx;      // [nglobals] → cx.global_tidx (concrete ref typeidx)
    uint8_t*        global_mut;       // [nglobals] §5.3.11 mutability (const-expr/instantiate)
    uint8_t*        global_is_import; // [nglobals] §7 const-expr admits only prior imports
    uint32_t        nglobals;
    uint32_t        nimport_globals;

    // tableidx → reftype + limits (§5.5.7).
    jav_valtype_t*  table_reftype;    // [ntables]
    uint32_t*       table_tidx;       // [ntables]
    uint64_t*       table_min;        // [ntables]
    uint64_t*       table_max;        // [ntables] (valid iff table_has_max)
    uint8_t*        table_has_max;    // [ntables]
    uint8_t*        table_is64;       // [ntables] §5.3.9 flag bit 2 (table64 addrtype)
    uint32_t        ntables;
    uint32_t        nimport_tables;

    // memidx → limits (§5.5.8). mem_is64 is parallel for cx.mem_is64.
    uint64_t*       mem_min;          // [nmems]
    uint64_t*       mem_max;          // [nmems] (valid iff mem_has_max)
    uint8_t*        mem_has_max;      // [nmems]
    uint8_t*        mem_is64;         // [nmems] §5.3.9 flag bit 2 (memory64) → cx.mem_is64
    uint32_t        nmems;
    uint32_t        nimport_mems;

    // tagidx → tag type (§5.5.16). A tag's type is the functype at tag_typeidx (results
    // must be empty); `tags` is FLAT for cx.tags.
    jav_functype_t* tags;             // [ntags] — tags[t] = functypes[tag_typeidx[t]]
    uint32_t*       tag_typeidx;      // [ntags]
    uint32_t        ntags;
    uint32_t        nimport_tags;

    // segment counts (§5.5.12 / §5.5.14) — the array.new_elem/init_elem + data bounds.
    uint32_t        nelems;
    jav_valtype_t*  elem_reftype;     // [nelems] each element segment's reference type (table.init rt2<:rt1)
    uint32_t*       elem_tidx;        // [nelems] concrete typeidx of a (ref $t) elem reftype (else 0)
    uint32_t        ndatas;          // §5.5.14 the data SECTION's segment count (instantiation)
    uint32_t        datacnt;         // §5.5.15 the data COUNT section's value (validation reads this)
    uint8_t         have_datacount;  // §5.5.17 was a data count section present at all

    // the §3.3 lattice over the flattened type space (borrows kinds/supers above).
    jav_subtype_ctx_t lattice;

    // §7.6's output, kept. Validating a body produces its side-table, try-table
    // and flat locals; all three are a function of the BYTES, so they belong to
    // the module and every instance of it reads the same ones.
    //
    // Indexed by DEFINED function (0 .. nfuncs-nimport_funcs), not by funcidx.
    // These are crt bbq_vecs — malloc'd — so unlike everything above they do NOT
    // ride the arena, and jav_modidx_free_bodies is what releases them. NULL
    // entries are normal: validation stops at the first body that fails.
    jav_st_entry_t** body_st;         // [nbodies] side-table
    jav_try_t**      body_tr;         // [nbodies] try-table
    jav_valtype_t**  body_locals;     // [nbodies] flat locals (params then declared)
    uint32_t*        body_ndecl;      // [nbodies] declared-local count
    uint32_t         nbodies;         // 0 ⇒ nothing kept; producers free on the spot
} jav_modidx_t;

// Release the §7.6 by-products above. Idempotent, and safe on a partially filled
// index: the arena owns the four arrays, this owns what they point at.
void jav_modidx_free_bodies(jav_modidx_t* mod);

// Flatten the c-lite index rooted at `root` over image `base` into `*out`. Returns 1
// on success, 0 on a structural inconsistency OR a construct whose representation is
// not yet built (fail-closed — never a silent wrong value). All table memory is
// allocated from `arena`.
int jav_module_index(const bbq_field_capture* root, const uint8_t* base,
                     bbq_arena* arena, jav_modidx_t* out);

// ── §4.5.2 session-global closed-type registry ──────────────────────────────────
// Per-module canon (mod->canon above) is a per-MODULE id space; cross-module import matching
// (§4.5.2 external typing, xt'≤xt) needs closed types from DIFFERENT modules to be comparable.
// The registry is the one session-wide closed-type interner: every module's rec groups are
// canonicalized into it, so equal closed types — even across modules — share one GLOBAL id.
// Lives on the shared heap (every instance reaches it via vm->heap), so there is ONE id space
// per session, not per store. Opaque; built/owned by jav_typereg_new/free.
typedef struct jav_typereg jav_typereg_t;

jav_typereg_t* jav_typereg_new(void);
void           jav_typereg_free(jav_typereg_t* r);

// Absorb `mod`'s closed types into `r`, writing each typeidx's GLOBAL canonical id into
// gcanon[0..mod->ntypes) (caller-owned). Idempotent: a closed type already interned (by this or
// any other module) maps to its existing id. Records each new id's kind + global supertype so the
// registry lattice (below) can walk cross-module supertype chains.
void jav_typereg_absorb(jav_typereg_t* r, const jav_modidx_t* mod, int32_t* gcanon);

// The §3.3 subtype lattice over GLOBAL ids: kinds/supers indexed by global id, canon = identity.
// jav_ht_sub(&this, provider_global_id, required_global_id) IS the §4.5.2 closed-type match.
jav_subtype_ctx_t jav_typereg_lattice(const jav_typereg_t* r);

// The persistent, store-owned rtt interned for a closed-type global id (absorb copies each module's
// rtt in), or NULL for func types / out-of-range. An instance points its ctx.struct_rtts entries here
// so a live object's o->rtt survives its defining module's teardown (wasm_module_delete).
const struct gc_rtt* jav_typereg_rtt(const jav_typereg_t* r, int32_t gid);

// Intern a standalone host functype (abstract-only heaptype refs) → its global id, so a host
// funcinst (no defining module / canon) matches imports through the same lattice. Returns -1 if
// the functype carries a concrete typeidx ref (host funcs don't — they have no module type space).
int32_t jav_typereg_intern_functype(jav_typereg_t* r, const jav_functype_t* ft);

// Decode one §5.3.5 valtype node against the index's flattened type space — for callers
// that walk a valtype sequence the index doesn't pre-flatten (e.g. a body's RLE locals).
// 1 on success; 0 fail-closed on a form with no boundary representation yet.
int jav_index_decode_valtype(const bbq_field_capture* vt, const uint8_t* base,
                             const jav_modidx_t* mod, jav_valtype_t* w, uint32_t* tidx);

// The §3.1.6 validation context projected from the index (types/func_sigs/globals/limits/
// lattice). The validator and instantiator both build a body's cx from this, then fill in
// the per-function locals/results. Shared so the projection lives in one place.
jav_vctx_t jav_module_cx(const jav_modidx_t* mod);

// The tier-2 tree builder's context, projected from the same index — everything it
// resolves is a storage class, so this is jav_module_cx's counterpart for the other
// walk. Class arrays are allocated from `arena`. `local_class`/`result_class` are
// per-FUNCTION and left for the caller to fill.
jav_tctx_t jav_module_tctx(const jav_modidx_t* mod, bbq_arena* arena);

#endif // JAV_MODULE_INDEX_H
