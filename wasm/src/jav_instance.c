// jav_instance.c — the §4.5 instantiator (first pin: defined-func table + type table +
// export map). Spec references are to the WebAssembly Core Specification, Release 3.0.
#include "jav_instance.h"
#include "jav_view_nav.h"        // shared span-index navigation
#include "jav_module_validate.h" // jav_body_typecheck (shared side-table re-derivation)
#include "interp.h"              // interp_run (const-expr evaluation)
#include "heap.h"           // heap_t / jav_mem_t / jav_mem_add (linear memory)
#include "jit_driver.h"     // jit_compile / jit_invoke / jit_free (the tier-1 seam)
#include "bbq_vec.h"        // bbq_vec_len (the heap's memory vector)
#include <stdlib.h>
#include <string.h>

static void jav_fill_ctx(jav_instance_t* out);   // §4.2 populate out->ctx (defined below; used in jav_instantiate)

// Evaluate a constant expression (init/offset) on the engine vm; returns the result slot.
static slot_t eval_const(vm_t* vm, const uint8_t* base, const bbq_field_capture* e) {
    bbq_ctx_init(&vm->frame.code, base + e->start_offset, e->end_offset - e->start_offset);
    vm->frame.sp = 0; vm->frame.num_locals = 0; vm->frame.sidetable = NULL;
    interp_run(vm, vm->heap);
    return jav_tos(vm);          /* the value the expression left on the stack */
}

// Span-index navigation (jav_view_field / jav_view_nchild / jav_view_section_array) is
// shared from jav_view_nav.h.

// A §5.3.5 value type → the runtime value tag carried alongside a global slot.
static uint8_t valtype_tag(jav_valtype_t w) {
    switch (w) {
    case WVT_I32:  return T_INT;
    case WVT_I64:  return T_LONG;
    case WVT_F32:  return T_FLOAT;
    case WVT_F64:  return T_DOUBLE;
    case WVT_V128: return T_V128;
    default:       return T_REF;   // funcref / externref / … (GC refs become T_GCREF later)
    }
}

// §4.5.2 import resolution. Walk the module's import vector in declaration order and match
// each declared import against the positionally-corresponding supplied externval (the wasm-
// c-api contract), dropping the value into its low slot. Returns JAV_OK, or JAV_UNLINKABLE
// with *err on an arity / kind / type mismatch. The funcs/globals/mem_addrs vecs are already
// allocated (low slots zeroed) by the caller; table 0 is borrowed from an imported table.
// §3.3.16 import matching: a provided externval must be a SUBTYPE of the declared import
// type. A reference is GENERIC: WVT_REF (nullable) / WVT_REF_NN (non-null) + a heaptype on the
// parallel array (an abstract HT_* code, negative, OR a concrete typeidx, ≥0). The §3.3 lattice
// (jav_rt_sub) does the rest.
static int imp_vt_is_num(jav_valtype_t w) {
    return w == WVT_I32 || w == WVT_I64 || w == WVT_F32 || w == WVT_F64 || w == WVT_V128;
}
// (valtype w, heaptype/typeidx ht) in a module's index space → (nullable, GLOBAL heap id) for the
// §4.5.2 session registry lattice: an abstract heaptype keeps its universal code; a concrete typeidx
// maps through THAT module's gcanon to its session-global canonical id (so two modules' concrete
// refs are comparable). gcanon NULL (host / no session types) ⇒ pass the raw value (abstract only).
static void imp_ref_global(jav_valtype_t w, int32_t ht, const int32_t* gcanon, int* nn, int32_t* g) {
    *nn = (w == WVT_REF);                                  // WVT_REF nullable, WVT_REF_NN non-null
    *g  = (ht < 0) ? ht : (gcanon ? gcanon[ht] : ht);      // abstract code, or concrete typeidx → global id
}
// provided value type (pv, pht via pg) <: required (rv, rht via rg), across modules (§3.3.5): numbers
// exact; references via the §3.3 lattice over GLOBAL ids — i.e. §4.5.2 closed-type matching, the one
// relation that also drives func imports (jav_ht_sub) and validation casts.
static int imp_vt_sub_g(jav_typereg_t* reg,
                        jav_valtype_t pv, int32_t pht, const int32_t* pg,
                        jav_valtype_t rv, int32_t rht, const int32_t* rg) {
    if (imp_vt_is_num(pv) || imp_vt_is_num(rv)) return pv == rv;
    if (!reg) return 0;
    int pn, rn; int32_t pgi, rgi;
    imp_ref_global(pv, pht, pg, &pn, &pgi); imp_ref_global(rv, rht, rg, &rn, &rgi);
    jav_subtype_ctx_t gl = jav_typereg_lattice(reg);
    return jav_rt_sub(&gl, pn, pgi, rn, rgi);
}

static jav_status_t link_imports(jav_instance_t* out, const jav_modidx_t* mod,
                                 const bbq_field_capture* root, const uint8_t* base,
                                 const jav_extern_t* imports, uint32_t nimports,
                                 jav_typereg_t* reg, jav_err_t* err) {
    uint32_t ndecl = mod->nimport_funcs + mod->nimport_tables + mod->nimport_mems
                   + mod->nimport_globals + mod->nimport_tags;
    if (nimports != ndecl) { if (err) *err = JAV_E_UNKNOWN_IMPORT; return JAV_UNLINKABLE; }
    if (ndecl == 0) return JAV_OK;
    const bbq_field_capture* isec = jav_view_find_section(root, 2, base);
    const bbq_field_capture* imps = isec ? jav_view_field(jav_view_field(isec, "body"), "imports") : NULL;
    uint32_t fi = 0, ti = 0, mi = 0, gi = 0, tgi = 0;  // per-kind low-slot cursors
    for (uint32_t i = 0; i < ndecl; i++) {
        const jav_extern_t* x = &imports[i];
        const bbq_field_capture* desc = jav_view_field(&imps->children[i], "desc");
        uint8_t kind = (uint8_t)bbq_node_int(jav_view_field(desc, "kind"), base);
        if (x->kind != kind) { if (err) *err = JAV_E_INCOMPATIBLE_IMPORT; return JAV_UNLINKABLE; }
        switch (kind) {
        case 0x00: {                                   // func (§4.5.2): provided closed type ≤ import closed type
            const jav_func_t* pf = &x->u.func.func;
            int32_t req_gid  = out->gcanon ? out->gcanon[mod->func_type_idx[fi]] : -1;   // import's closed type id
            int32_t prov_gid = (pf->inst_ctx && pf->inst_ctx->gcanon)                    // wasm: read funcinst.type off the
                             ? pf->inst_ctx->gcanon[pf->type_index]                      //   defining instance's gcanon
                             : (reg ? jav_typereg_intern_functype(reg, x->u.func.type) : -1);  // host: intern its functype
            if (!reg || req_gid < 0 || prov_gid < 0) goto incompatible;
            jav_subtype_ctx_t gl = jav_typereg_lattice(reg);   // the ONE relation, over global ids = §3.3.10 closed-type match
            if (!jav_ht_sub(&gl, prov_gid, req_gid)) goto incompatible;
            out->funcs[fi] = x->u.func.func;   // §4.7.1 the funcinst keeps its DEFINER's {type, module}:
            fi++; break; }                     // type_index must index inst_ctx->types, so do NOT rewrite it
        case 0x01: {                                   // table (§3.3.15): addrtype match + reftype INVARIANT + limits <:
            int32_t rrt = mod->table_tidx ? (int32_t)mod->table_tidx[ti] : 0;
            if (x->u.table.is64 != mod->table_is64[ti] ||                                          // addrtype (i32 vs i64) must match
                !imp_vt_sub_g(reg, x->u.table.reftype, x->u.table.reftype_ht, x->u.table.gcanon,   // provided reftype ≤ import
                                   mod->table_reftype[ti], rrt, out->gcanon) ||
                !imp_vt_sub_g(reg, mod->table_reftype[ti], rrt, out->gcanon,                       // ... AND import ≤ provided (invariant)
                                   x->u.table.reftype, x->u.table.reftype_ht, x->u.table.gcanon) ||
                x->u.table.size < mod->table_min[ti] ||
                (mod->table_has_max[ti] && (!x->u.table.has_max || x->u.table.max > mod->table_max[ti])))
                goto incompatible;
            out->tables[ti].refs = x->u.table.data; out->tables[ti].types = x->u.table.types;
            out->table_borrowed[ti] = 1;   // borrow the exporter's slot-sized refs + tags
            ti++; break; }
        case 0x02:                                     // memory (§3.3.14): limits <: + addrtype match
            if (x->u.mem.is64 != mod->mem_is64[mi] ||
                x->u.mem.min < mod->mem_min[mi] ||
                (mod->mem_has_max[mi] && (!x->u.mem.has_max || x->u.mem.max > mod->mem_max[mi])))
                goto incompatible;
            out->mem_addrs[mi] = x->u.mem.memidx;
            mi++; break;
        case 0x03:                                     // global (§3.3.13): const → covariant, var → invariant
            if (x->u.global.mut != mod->global_mut[gi]) goto incompatible;
            { int32_t pt = x->u.global.type_ht, rt = mod->global_tidx ? (int32_t)mod->global_tidx[gi] : 0;
              int ok = imp_vt_sub_g(reg, x->u.global.type, pt, x->u.global.gcanon,                // provided ≤ import
                                         mod->global_types[gi], rt, out->gcanon);
              if (mod->global_mut[gi])                                                            // mutable: also import ≤ provided
                  ok = ok && imp_vt_sub_g(reg, mod->global_types[gi], rt, out->gcanon,
                                               x->u.global.type, pt, x->u.global.gcanon);
              if (!ok) goto incompatible; }
            out->globals[gi] = x->u.global.slot;   // §4.2.5 alias the exporter's slot (mutable imported global is shared)
            out->global_types[gi] = x->u.global.tag;   // inherit the exporter's runtime value tag: a managed-ref import
            gi++; break;                               // must read T_GCREF (not the valtype default T_REF), else a
                                                        // global.get→ref.cast/dispatch of it wrongly traps (i31 lattice)
        case 0x04: {                                   // tag (§3.3.12: deftype INVARIANT — both directions, closed types)
            if (!x->u.tag.type || !reg) goto incompatible;
            int32_t req_gid  = out->gcanon ? out->gcanon[mod->tag_typeidx[tgi]] : -1;
            int32_t prov_gid = x->u.tag.gcanon ? x->u.tag.gcanon[x->u.tag.typeidx]   // wasm: provider's closed tag-type id
                             : jav_typereg_intern_functype(reg, x->u.tag.type);      // host: intern its functype (SAME relation)
            if (req_gid < 0 || prov_gid < 0) goto incompatible;
            jav_subtype_ctx_t gl = jav_typereg_lattice(reg);   // the ONE relation, wasm + host alike: prov ≤ req AND req ≤ prov
            if (!jav_ht_sub(&gl, prov_gid, req_gid) || !jav_ht_sub(&gl, req_gid, prov_gid)) goto incompatible;
            out->tag_ids[tgi] = x->u.tag.tag_id;   // §4.2 inherit the exporter's tagaddr identity
            tgi++; break; }
        default: goto incompatible;
        }
    }
    return JAV_OK;
incompatible:
    if (err) *err = JAV_E_INCOMPATIBLE_IMPORT;
    return JAV_UNLINKABLE;
}

jav_status_t jav_instantiate(vm_t* vm, const bbq_field_capture* root, const uint8_t* base,
                             const jav_modidx_t* mod, const jav_extern_t* imports, uint32_t nimports,
                             jav_instance_t* out, jav_err_t* err) {
    memset(out, 0, sizeof *out);
    if (err) *err = JAV_E_NONE;
    out->root = root; out->base = base; out->mod = mod;   // mod->rtts (§4.5.3) are module-level, bound to the vm below
    // §4.5.2 absorb this module's closed types into the session-global registry (on the shared
    // heap) → out->gcanon[typeidx] = global canonical id. This is the common id space cross-module
    // import matching compares in; built before link_imports so required types resolve to gids.
    if (mod->ntypes && vm->heap) {
        jav_typereg_t* reg = jav_heap_typereg(vm->heap);
        bbq_vec_reserve(out->gcanon, mod->ntypes);
        for (uint32_t i = 0; i < mod->ntypes; i++) bbq_vec_push(out->gcanon, (int32_t)0);
        jav_typereg_absorb(reg, mod, out->gcanon);
        // §4.5.2: resolve each type to its store-owned interned rtt (keyed by the closed-type gid, and
        // stamped with it). Objects this instance allocates carry o->rtt into this PERSISTENT table, so a
        // retained object's runtime type survives the module's teardown (wasm_module_delete frees mod->arena
        // and mod->rtts with it); the shared rtt also gives cross-instance ref.test/ref.cast one canonical
        // identity per closed type. ctx.struct_rtts points here (below), not at the doomed mod->rtts.
        bbq_vec_reserve(out->interned_rtts, mod->ntypes);
        for (uint32_t t = 0; t < mod->ntypes; t++)
            bbq_vec_push(out->interned_rtts, jav_typereg_rtt(reg, out->gcanon[t]));
    }
    uint32_t nfuncs = mod->nfuncs, ndef = mod->nfuncs - mod->nimport_funcs;
    // Instance tables are crt bbq_vecs (the length rides with each via bbq_vec_len — no
    // parallel count field, no fixed cap): reserve to the count so the pointer is stable,
    // then push to set the length.
    bbq_vec_reserve(out->funcs, nfuncs ? nfuncs : 1);
    { jav_func_t z = {0}; for (uint32_t i = 0; i < nfuncs; i++) bbq_vec_push(out->funcs, z); }
    if (ndef) {
        bbq_vec_reserve(out->sidetabs, ndef); bbq_vec_reserve(out->trytabs, ndef);
        for (uint32_t i = 0; i < ndef; i++) {
            jav_st_entry_t* s = NULL; jav_try_t* t = NULL;
            bbq_vec_push(out->sidetabs, s); bbq_vec_push(out->trytabs, t);
        }
    }
    // Globals + their runtime tags (imports low, then defined) and the memidx map are
    // allocated up front so import linking can fill the low slots and const-eval (step c)
    // sees imported globals in scope. Point the engine at this instance's funcs/globals.
    if (mod->nglobals) {
        uint32_t nimp = mod->nimport_globals, ndef = mod->nglobals - nimp;
        bbq_vec_reserve(out->global_store, ndef ? ndef : 1);   // stable backing for DEFINED globals (pointees below)
        bbq_vec_reserve(out->globals, mod->nglobals);
        bbq_vec_reserve(out->global_types, mod->nglobals);
        slot_t zero = {0};
        for (uint32_t d = 0; d < ndef; d++) bbq_vec_push(out->global_store, zero);
        for (uint32_t i = 0; i < mod->nglobals; i++) {
            slot_t* p = i >= nimp ? &out->global_store[i - nimp] : NULL;  // defined → own backing; imported → filled by link_imports
            bbq_vec_push(out->globals, p);
            bbq_vec_push(out->global_types, valtype_tag(mod->global_types[i]));
        }
    }
    if (mod->nmems) { bbq_vec_reserve(out->mem_addrs, mod->nmems);
                      for (uint32_t i = 0; i < mod->nmems; i++) { uint32_t z = 0; bbq_vec_push(out->mem_addrs, z); } }
    // Tables (imports low, then defined): one jav_tableinst_t per tableidx, type set now; refs are
    // filled by link_imports (imported → borrowed) and the defined-table build below.
    if (mod->ntables) {
        bbq_vec_reserve(out->tables, mod->ntables); bbq_vec_reserve(out->table_borrowed, mod->ntables);
        for (uint32_t ti = 0; ti < mod->ntables; ti++) {
            jav_tableinst_t t = {0};
            t.reftype = (uint8_t)mod->table_reftype[ti]; t.is64 = mod->table_is64[ti];
            t.reftype_ht = mod->table_tidx ? (int32_t)mod->table_tidx[ti] : 0;
            t.has_max = mod->table_has_max[ti]; t.max = t.has_max ? (uint32_t)mod->table_max[ti] : 0;
            bbq_vec_push(out->tables, t); uint8_t z = 0; bbq_vec_push(out->table_borrowed, z);
        }
    }
    // Tags (imports low, then defined): §4.2 each tag has a store identity. A DEFINED tag gets a
    // fresh session-unique id; an IMPORTED tag inherits the exporter's (filled by link_imports).
    if (mod->ntags) {
        bbq_vec_reserve(out->tag_ids, mod->ntags);
        for (uint32_t i = 0; i < mod->ntags; i++) {
            uint32_t id = (i >= mod->nimport_tags && vm->heap) ? vm->heap->next_tag_id++ : 0;  // imported slots filled by link
            bbq_vec_push(out->tag_ids, id);
        }
    }
    // §8: populate the MINIMAL facets the const-expr global inits below need (they allocate via
    // array.new / struct.new → require functions/struct_rtts/§3.3 lattice live) into THIS instance's
    // context, and point the engine at it. jav_fill_ctx fills the rest at the end of §4.7.2; the
    // §4.5.4 init ORDER is unchanged — the same facets become available at the same points, only read
    // through frame.ctx now rather than a flat cache. (rtts are module-level — a pointer copy.)
    out->ctx.functions = out->funcs; out->ctx.num_functions = (u4)bbq_vec_len(out->funcs);
    out->ctx.globals = out->globals; out->ctx.global_types = out->global_types;
    // Prefer the store-owned interned rtts (persistent past module teardown); fall back to the module's
    // own rtts only when there's no session registry (single-module / no shared heap — nothing outlives it).
    out->ctx.struct_rtts = out->interned_rtts ? (const struct gc_rtt* const*)out->interned_rtts
                                              : (const struct gc_rtt* const*)mod->rtts;
    out->ctx.num_struct_rtts = mod->ntypes;
    out->ctx.lattice = &mod->lattice;
    out->ctx.mem_addrs = out->mem_addrs;   // §4.2.3 memidx → store memaddr (pointer stable; filled by link + the defined-mem loop)
    out->ctx.num_mems = mod->nmems;
    vm->frame.ctx = &out->ctx;             // the engine now executes const-exprs in THIS instance's context

    // ── (a) resolve + type-match host imports → low slots (JAV_UNLINKABLE on a mismatch) ──
    jav_status_t ls = link_imports(out, mod, root, base, imports, nimports,
                                   vm->heap ? jav_heap_typereg(vm->heap) : NULL, err);
    if (ls != JAV_OK) { jav_instance_free(out); return ls; }

    // ── (b) the defined-func table: each body's side-table re-derived via the shared
    // jav_body_typecheck (module already §7-valid). Imports occupy the low slots
    // [0, nimport_funcs); host wiring is a later pin.
    const bbq_field_capture* entries = jav_view_section_array(root, 10, "entries", base);
    for (uint32_t d = 0; d < ndef; d++) {
        uint32_t fi = mod->nimport_funcs + d;
        const jav_functype_t* sig = &mod->func_sigs[fi];
        const bbq_field_capture* entry = &entries->children[d];
        uint32_t ndecl; jav_st_entry_t* st; unsigned nst; jav_try_t* tr; unsigned ntr;
        jav_body_typecheck(mod, base, entry, sig, NULL, &ndecl, &st, &nst, &tr, &ntr, NULL);   // re-derive (already validated)
        out->sidetabs[d] = st; out->trytabs[d] = tr;
        const bbq_field_capture* expr = jav_view_field(jav_view_field(entry, "body"), "body");

        jav_func_t* f = &out->funcs[fi];
        f->code = base + expr->start_offset; f->code_len = expr->end_offset - expr->start_offset;
        f->num_params = sig->nparams; f->num_locals = ndecl; f->num_results = sig->nresults;
        f->type_index = mod->func_type_idx[fi];
        f->sig = sig;                                         // §4.5.2 every funcinst carries its functype (uniform with host funcs)
        f->sidetable = st; f->trytable = tr; f->ntry = ntr;
        f->invoke = NULL; f->invoke_ctx = NULL;               // interp tier (JIT is a pointer-swap)
        f->inst_ctx = &out->ctx;                              // §4.2.6 this func's defining instance (contents filled below)

        // Tier selection (the embedder's wasm_config_t choice): compile the body ONCE here and swap
        // `invoke` — every caller still dispatches through the same seam. A body the JIT declines
        // (jit_compile → NULL) simply stays interpreted.
        if (vm->jit_enabled) {
            bbq_ctx_t code; bbq_ctx_init(&code, f->code, f->code_len);
            jit_func_t* h = jit_compile(code);
            if (h) { f->invoke = jit_invoke; f->invoke_ctx = h; bbq_vec_push(out->jitfns, h); vm->jit_compiled++; }
        }
    }

    // ── (c) evaluate defined-global inits in order (§4.5.4): each init is a const-expr run
    // on the interp, with imports + EARLIER globals already in scope so later global.get sees
    // them. The globals vec + the engine pointers were set up before import linking; const-
    // eval writes straight into out->globals[gidx] (no copy).
    if (mod->nglobals) {
        const bbq_field_capture* gl = jav_view_section_array(root, 6, "globals", base);
        for (uint32_t d = 0; d < jav_view_nchild(gl); d++) {
            uint32_t gidx = mod->nimport_globals + d;
            const bbq_field_capture* in = jav_view_field(&gl->children[d], "init");
            bbq_ctx_init(&vm->frame.code, base + in->start_offset, in->end_offset - in->start_offset);
            vm->frame.sp = 0; vm->frame.num_locals = 0; vm->frame.sidetable = NULL;
            interp_run(vm, vm->heap);
            *out->globals[gidx] = jav_tos(vm);  // write through the pointer into this instance's backing slot
            // §4.5.4: a ref global's tag defaults to T_REF (valtype_tag); a managed init value makes it
            // T_GCREF — the promised "later" upgrade (mirrors the element-segment path). Without it a
            // global.get of a GC ref (e.g. a Class singleton) pushes a T_REF-tagged value that fails
            // ref.cast/dispatch AND is skipped as a GC root (a use-after-free hole under collection).
            if (jav_tos_type(vm) == T_GCREF) out->global_types[gidx] = T_GCREF;
        }
    }

    // ── the export map (§4.5.11): a bbq_vec of name span (borrowed) → kind + index ──
    const bbq_field_capture* ex = jav_view_section_array(root, 7, "exports", base);
    for (uint32_t i = 0; i < jav_view_nchild(ex); i++) {
        const bbq_field_capture* e = &ex->children[i];
        const bbq_field_capture* bn = jav_view_field(jav_view_field(e, "name"), "bytes");
        jav_inst_export_t en = { (const char*)(base + bn->start_offset),
                            (uint32_t)(bn->end_offset - bn->start_offset),
                            (uint8_t)bbq_node_int(jav_view_field(e, "kind"), base),
                            (uint32_t)bbq_node_int(jav_view_field(e, "idx"), base) };
        bbq_vec_push(out->exports, en);
    }

    // ── (d/e) §4.5.5/.7 linear memory + table 0 + active segments ──
    // Defined memories are appended to the store heap; mem_addrs[mi] maps the module memidx
    // (imports already filled by link) to its store-heap index, so a segment indexes uniformly.
    const bbq_field_capture* ms = jav_view_section_array(root, 5, "mems", base);
    for (uint32_t d = 0; d < jav_view_nchild(ms); d++) {
        uint32_t mi = mod->nimport_mems + d;
        uint32_t maxp = mod->mem_has_max[mi] ? (uint32_t)mod->mem_max[mi] : 0;
        out->mem_addrs[mi] = (uint32_t)jav_mem_add(vm->heap, (u4)mod->mem_min[mi], maxp,
                                                   mod->mem_has_max[mi], mod->mem_is64[mi]);
    }
    // Defined tables (§4.5.3): allocate each one's refs to its minimum size. The default is the null
    // reference (−1/T_REF); the §5.5.7 `0x40 0x00 …` form carries an explicit init const-expr — eval
    // it (imports + earlier globals/funcs are in scope) and fill every element with that ref value.
    const bbq_field_capture* tsec = jav_view_section_array(root, 4, "tables", base);
    for (uint32_t ti = mod->nimport_tables; ti < mod->ntables; ti++) {
        uint32_t sz = (uint32_t)mod->table_min[ti];
        s8 fill = (s8)(u4)JAV_NULLREF; u1 ft = T_REF;             // default: the null reference (ONE authority — not a literal)
        uint32_t d = ti - mod->nimport_tables;
        const bbq_field_capture* tinit = NULL;                   // present only for the §5.5.7 TableInit (0x40) form
        if (tsec && d < jav_view_nchild(tsec))
            tinit = jav_view_field(jav_view_choice(&tsec->children[d]), "init");   // the chosen Table-union arm
        if (tinit) {
            bbq_ctx_init(&vm->frame.code, base + tinit->start_offset, tinit->end_offset - tinit->start_offset);
            vm->frame.sp = 0; vm->frame.num_locals = 0; vm->frame.sidetable = NULL;
            interp_run(vm, vm->heap);
            fill = (s8)jav_tos(vm).l; ft = jav_tos_type(vm);
        }
        for (uint32_t i = 0; i < sz; i++) {
            bbq_vec_push(out->tables[ti].refs, fill);
            bbq_vec_push(out->tables[ti].types, ft);
        }
    }
    // §4.7.2 step 23 (allocmodule) is now complete — snapshot the structural facets (funcs/
    // globals/tables/types/rtts/mem_addrs) into out->ctx so EVERY funcinst carries its full
    // defining-instance context BEFORE segment init (steps 27-28). A module that traps in a
    // later active segment thus still leaves valid, callable funcinsts (e.g. referenced from a
    // shared table). Re-run after the segments below to capture the segment stashes too.
    jav_fill_ctx(out);

    // §4.7.2 step 24: the instance is now allocated — hand it to the store owner so it is rooted as live
    // store state for the active-segment + start GC below (steps 27-29; start runs arbitrary allocating
    // code). A trapped (UNINSTANTIABLE) instance is thus already store-tracked → it PERSISTS; a link error
    // returns above (before here) → it never registers. Bare-VM path: hook NULL, the bound scan covers it.
    if (vm->on_inst_alloc) vm->on_inst_alloc(vm->on_inst_alloc_ctx, out);

    // §4.7.2 step 27 — ELEMENT segments (§4.5.8) run BEFORE data segments (step 28): materialize
    // every segment's ref values into elem_pool and register elem_segs {values,len}; an active
    // segment is applied to its table (offset → bounds-check → write), a passive/declarative one
    // stays available for array.new_elem. Order matters: an in-bounds active elem applied here
    // PERSISTS even if a later data segment traps OOB (linking.wast).
    const bbq_field_capture* el = jav_view_section_array(root, 9, "elems", base);
    uint32_t nelems = jav_view_nchild(el);
    if (nelems) {
        bbq_vec_reserve(out->elem_segs, nelems); bbq_vec_reserve(out->elem_dropped, nelems);
        uint32_t total = 0;                                   // reserve the value pool so the per-segment
        for (uint32_t i = 0; i < nelems; i++) {               // values pointers stay stable across pushes
            const bbq_field_capture* b = jav_view_field(&el->children[i], "body");
            const bbq_field_capture* fl = jav_view_field(b, "funcs");
            const bbq_field_capture* xl = jav_view_field(b, "exprs");
            total += jav_view_nchild(fl ? jav_view_field(fl, "idxs") : (xl ? jav_view_field(xl, "exprs") : NULL));
        }
        bbq_vec_reserve(out->elem_pool, total ? total : 1);
        bbq_vec_reserve(out->elem_tag_pool, total ? total : 1);
    }
    for (uint32_t i = 0; i < nelems; i++) {
        const bbq_field_capture* b = jav_view_field(&el->children[i], "body");
        const bbq_field_capture* fl = jav_view_field(b, "funcs");
        const bbq_field_capture* xl = jav_view_field(b, "exprs");
        const bbq_field_capture* items = fl ? jav_view_field(fl, "idxs") : (xl ? jav_view_field(xl, "exprs") : NULL);
        uint32_t cnt = jav_view_nchild(items), start = (uint32_t)bbq_vec_len(out->elem_pool);
        for (uint32_t j = 0; j < cnt; j++) {
            // §4.2.1 a funcref is a funcinst REFERENCE (pointer), not a funcidx — so an entry put
            // into a (possibly shared) table is callable from any instance. `func i` → &funcs[i].
            // An expr item is a full §3-const expression — which in 3.0 includes GC ALLOCATIONS
            // (struct.new/array.new*, admitted by const_scan) — so its RUNTIME TAG must be kept
            // beside the value: T_GCREF entries in a live segment are GC roots (§4.2.12 eleminst
            // holds refs), and table.init/active application must write an honestly-tagged slot.
            int64_t v; uint8_t tg;
            if (fl) { v = (int64_t)(uintptr_t)&out->funcs[(uint32_t)bbq_node_int(&items->children[j], base)]; tg = T_REF; }
            else    { v = (int64_t)eval_const(vm, base, &items->children[j]).l; tg = jav_tos_type(vm); }
            bbq_vec_push(out->elem_pool, v);
            bbq_vec_push(out->elem_tag_pool, tg);
        }
        jav_elem_seg_t seg = { cnt ? &out->elem_pool[start] : NULL,
                               cnt ? &out->elem_tag_pool[start] : NULL, cnt };
        bbq_vec_push(out->elem_segs, seg);
        const bbq_field_capture* off = jav_view_field(b, "offset");
        const bbq_field_capture* flagn = jav_view_field(&el->children[i], "flag");
        uint32_t flag = flagn ? (uint32_t)bbq_node_int(flagn, base) : 0;
        int declarative = (flag == 3 || flag == 7);           // §5.5.12 flags 3/7; ε at runtime (never usable by table.init/array.new_elem)
        uint8_t dropped = (off || declarative) ? 1 : 0;       // active → dropped once applied; declarative → born dropped
        bbq_vec_push(out->elem_dropped, dropped);
        if (!off) continue;                                   // passive / declarative
        const bbq_field_capture* tn = jav_view_field(b, "table");
        uint32_t tx = tn ? (uint32_t)bbq_node_int(tn, base) : 0;   // active elem's target table (default 0); §5.5.12 flag 2/6 carry it
        s8* refs = out->tables[tx].refs;
        u1* rtys = out->tables[tx].types;
        slot_t o = eval_const(vm, base, off);
        uint64_t addr = mod->table_is64[tx] ? (uint64_t)o.l : (uint32_t)o.i;   // §3.3.16 active-elem offset is the table's addrtype (i64 for table64)
        uint64_t tlen = bbq_vec_len(refs);                    // overflow-safe (subtract): a table64 offset ≥ 2⁶⁴⁻ⁿ traps, not wraps
        if (addr > tlen || cnt > tlen - addr) { if (err) *err = JAV_E_OOB_TABLE; return JAV_UNINSTANTIABLE; }  // §4.5.4: earlier elem segments (+ the funcinsts they wrote into shared tables) PERSIST
        // the active write carries each value's RUNTIME tag — a GC allocation lands as a scanned
        // T_GCREF table slot, a funcref/null as T_REF. (This previously hardcoded T_REF: a GC
        // value written by an active elem was invisible to the root scan — freed while live.)
        for (uint32_t j = 0; j < cnt; j++) { refs[addr + j] = out->elem_pool[start + j]; rtys[addr + j] = out->elem_tag_pool[start + j]; }
    }
    // §4.7.2 step 28 — DATA segments (§4.5.6) run AFTER element segments: register every segment
    // {bytes,len}; an active segment is applied to memory (offset → bounds-check → memcpy) then
    // marked dropped, a passive one stays available for memory.init / array.new_data.
    const bbq_field_capture* dl = jav_view_section_array(root, 11, "datas", base);
    uint32_t ndatas = jav_view_nchild(dl);
    if (ndatas) { bbq_vec_reserve(out->data_segs, ndatas); bbq_vec_reserve(out->data_dropped, ndatas); }
    for (uint32_t i = 0; i < ndatas; i++) {
        const bbq_field_capture* b = jav_view_field(&dl->children[i], "body");
        const bbq_field_capture* bn = jav_view_field(jav_view_field(b, "data"), "bytes");
        uint64_t len = bn->end_offset - bn->start_offset;
        jav_data_seg_t seg = { base + bn->start_offset, (u4)len };
        bbq_vec_push(out->data_segs, seg);
        const bbq_field_capture* off = jav_view_field(b, "offset");
        uint8_t dropped = off ? 1 : 0;                        // active → dropped once applied
        bbq_vec_push(out->data_dropped, dropped);
        if (!off) continue;                                   // passive: nothing to apply now
        const bbq_field_capture* mn = jav_view_field(b, "memidx");
        uint32_t mi = mn ? (uint32_t)bbq_node_int(mn, base) : 0;
        jav_mem_t* mem = &vm->heap->mems[out->mem_addrs[mi]];
        slot_t o = eval_const(vm, base, off);
        uint64_t addr = mod->mem_is64[mi] ? (uint64_t)o.l : (uint32_t)o.i;
        if (addr + len > mem->size) { if (err) *err = JAV_E_OOB_MEMORY; return JAV_UNINSTANTIABLE; }  // §4.5.4 trapped: the instance + already-applied segments PERSIST (caller owns/frees it)
        if (len) memcpy(mem->data + addr, base + bn->start_offset, len);
    }

    // Re-snapshot now that the data/elem stashes exist (memory.init / array.new_* read them);
    // structural facets are unchanged from the pre-segment fill above.
    jav_fill_ctx(out);

    // ── (f) start function (§4.5.10): invoke [] -> [] via the tier seam; a trap is a
    // failed instantiation. Bind the full instance first so start sees globals/table/memory.
    const bbq_field_capture* ss = jav_view_find_section(root, 8, base);
    if (ss) {
        uint32_t sf = (uint32_t)bbq_node_int(jav_view_field(jav_view_field(ss, "body"), "func"), base);
        jav_instance_bind(vm, out);
        vm->frame.sp = 0; vm->frame.num_locals = 0;
        if (jav_call(vm, vm->heap, (s4)sf) == JAV_TRAP) { return JAV_UNINSTANTIABLE; }  // §4.5.4 start trapped: instance + its applied effects PERSIST (caller owns/frees it)
    }
    return JAV_OK;
}

// §4.2 capture the instance's built facets into its engine context (out->ctx) — one snapshot of
// stable pointers (the per-instance vecs are fixed-count, never grown post-instantiation). The
// funcinsts' inst_ctx already point at &out->ctx; this fills the contents. Called once, at the
// end of instantiation.
static void jav_fill_ctx(jav_instance_t* out) {
    instctx_t* c = &out->ctx;
    c->functions = out->funcs;          c->num_functions = (u4)bbq_vec_len(out->funcs);
    c->globals   = out->globals;        c->global_types  = out->global_types;
    c->tables    = out->tables;
    c->types     = out->mod->functypes; c->num_types     = out->mod->ntypes;
    c->struct_rtts = out->interned_rtts ? (const struct gc_rtt* const*)out->interned_rtts
                                        : (const struct gc_rtt* const*)out->mod->rtts;  c->num_struct_rtts = out->mod->ntypes;
    c->type_field_packs = out->mod->type_field_packs; c->num_type_field_packs = out->mod->ntypes;   // packed struct/array widths
    c->lattice   = &out->mod->lattice;
    c->gcanon    = out->gcanon;         // §4.5.2 the session-global id per typeidx (cross-module match)
    c->mem_addrs = out->mem_addrs;      c->num_mems      = out->mod->nmems;
    c->tags      = out->mod->tags;      c->num_tags      = out->mod->ntags;
    c->tag_ids   = out->tag_ids;        // §4.2 tagaddr identities (throw/catch match)
    c->data_segs = out->data_segs;      c->num_data_segs = (u4)bbq_vec_len(out->data_segs);  c->data_dropped = out->data_dropped;
    c->elem_segs = out->elem_segs;      c->num_elem_segs = (u4)bbq_vec_len(out->elem_segs);  c->elem_dropped = out->elem_dropped;
}

void jav_instance_bind(vm_t* vm, const jav_instance_t* inst) {
    vm->frame.ctx = &inst->ctx;        // §8: the root frame executes in this instance — frame.ctx IS the context
    // linear memory is the store heap on vm->heap (already set by the caller / instantiation).
}

// The one per-instance root scan (was copy-pasted in the engine + the c-api). A managed-ref global
// or table slot (T_GCREF) holds a gc object the moving collector may relocate, so it visits the slot.
void jav_instance_visit_roots(jav_instance_t* in, jav_root_visit_fn visit, void* ctx) {
    for (size_t g = 0, n = bbq_vec_len(in->globals); g < n; g++)
        if (in->global_types[g] == T_GCREF) visit((struct gc_obj**)&in->globals[g]->l, ctx);
    for (size_t t = 0, nt = bbq_vec_len(in->tables); t < nt; t++)
        for (size_t e = 0, ne = bbq_vec_len(in->tables[t].refs); e < ne; e++)
            if (in->tables[t].types[e] == T_GCREF) visit((struct gc_obj**)&in->tables[t].refs[e], ctx);
    /* §4.2.12 eleminst holds refs: a GC-allocated const-expr item parked in a LIVE (non-dropped)
     * segment is reachable — table.init / array.new_elem can still materialize it — so it is a
     * root. A dropped segment is ε: its values are unreachable and must NOT be kept alive. The
     * visit targets the mutable pool slot so an evacuating collector can rewrite it. */
    for (size_t s = 0, ns = bbq_vec_len(in->elem_segs); s < ns; s++) {
        if (in->elem_dropped[s]) continue;
        const jav_elem_seg_t* es = &in->elem_segs[s];
        for (u4 j = 0; j < es->len; j++)
            if (es->types[j] == T_GCREF) visit((struct gc_obj**)(uintptr_t)&es->values[j], ctx);
    }
}

int32_t jav_instance_export(const jav_instance_t* inst, const char* name, uint8_t kind) {
    size_t L = strlen(name);
    for (size_t i = 0, n = bbq_vec_len(inst->exports); i < n; i++)
        if (inst->exports[i].kind == kind && inst->exports[i].name_len == L &&
            memcmp(inst->exports[i].name, name, L) == 0)
            return (int32_t)inst->exports[i].index;
    return -1;
}

void jav_instance_free(jav_instance_t* inst) {
    for (size_t d = 0, n = bbq_vec_len(inst->sidetabs); d < n; d++) bbq_vec_free(inst->sidetabs[d]);
    for (size_t d = 0, n = bbq_vec_len(inst->trytabs); d < n; d++) bbq_vec_free(inst->trytabs[d]);
    bbq_vec_free(inst->sidetabs); bbq_vec_free(inst->trytabs);
    bbq_vec_free(inst->funcs); bbq_vec_free(inst->exports);
    bbq_vec_free(inst->globals); bbq_vec_free(inst->global_store); bbq_vec_free(inst->global_types);
    bbq_vec_free(inst->mem_addrs); bbq_vec_free(inst->gcanon); bbq_vec_free(inst->tag_ids);
    bbq_vec_free(inst->interned_rtts);   // frees only the pointer array; the rtts themselves are registry-owned
    bbq_vec_free(inst->data_segs); bbq_vec_free(inst->data_dropped);
    bbq_vec_free(inst->elem_segs); bbq_vec_free(inst->elem_dropped); bbq_vec_free(inst->elem_pool);
    bbq_vec_free(inst->elem_tag_pool);
    for (size_t t = 0, n = bbq_vec_len(inst->tables); t < n; t++)
        if (!inst->table_borrowed[t]) {                                     // imported tables' storage is the exporter's
            bbq_vec_free(inst->tables[t].refs); bbq_vec_free(inst->tables[t].types);
        }
    bbq_vec_free(inst->tables); bbq_vec_free(inst->table_borrowed);
    for (size_t d = 0, n = bbq_vec_len(inst->jitfns); d < n; d++) jit_free(inst->jitfns[d]);
    bbq_vec_free(inst->jitfns);
    // mod->rtts are module-level (the module arena owns them) — nothing per-instance to free.

    // linear memories live in the store heap, freed with it
    memset(inst, 0, sizeof *inst);
}
