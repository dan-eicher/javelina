// jav_instance.h — the §4.5 module instance + instantiator.
//
// jav_instantiate turns a validated module (the flattened index over the c-lite image)
// into a runnable jav_instance_t: the function table (imports in the low slots, then the
// defined funcs with their re-derived side-tables), the type table, and the export map.
// jav_instance_bind PROJECTS the instance onto the vm_t fields the generated handlers
// already read (vm->functions / vm->types / …) — a cheap pointer assignment, no copy.
#ifndef JAV_INSTANCE_H
#define JAV_INSTANCE_H

#include "jav_module_index.h"   // jav_modidx_t (+ validate.h / runtime_api.h)
#include "jav_frame.h"          // jav_func_t / vm_t / jav_st_entry_t / jav_try_t
#include "jav_error.h"          // jav_err_t

// One runtime instance export (§4.5): the name (a span borrowed from the module image) and what it
// denotes. Distinct from the generated parse-AST `jav_export_t` (struct jav_export, jav_types.h) —
// that is the wat/binary reader's export node; this is the instantiated export descriptor.
typedef struct { const char* name; uint32_t name_len; uint8_t kind; uint32_t index; } jav_inst_export_t;

// A host-supplied import value (§4.5.2 externval). The embedder passes an array of these,
// POSITIONAL to the module's import vector (the wasm-c-api contract — name resolution is
// the embedder's job); jav_instantiate type-matches each against the declared import and
// drops it into the matching low slot. `kind`: 0 func, 1 table, 2 mem, 3 global, 4 tag.
typedef struct jav_extern {
    uint8_t kind;
    union {
        struct { jav_func_t func; const jav_functype_t* type; } func;   // 0: an external function
        struct { int64_t* data; uint8_t* types; uint32_t size;          // 1: a table (slot-sized refs +
                 uint8_t has_max; uint32_t max; jav_valtype_t reftype;  //  parallel tags, shared w/ exporter)
                 int32_t reftype_ht; const int32_t* gcanon; uint8_t is64; } table;  // element heaptype; gcanon: typeidx→global id; is64: §3.3.15 addrtype
        struct { uint32_t memidx;                                       // 2: a memory (a store memidx)
                 uint64_t min; uint8_t has_max; uint64_t max; uint8_t is64; } mem;
        struct { slot_t* slot; jav_valtype_t type; int32_t type_ht; uint8_t mut;  // 3: a global BY REFERENCE — the
                 const int32_t* gcanon; uint8_t tag; } global;  //  exporter's slot (mutable share); type_ht = ref heaptype;
                                          //  gcanon: provider typeidx→global id (§4.5.2); tag = exporter's runtime value tag
                                          //  (T_GCREF for a managed ref) so the importer scans/dispatches it correctly
        struct { const jav_functype_t* type; const int32_t* gcanon; uint32_t typeidx; uint32_t tag_id; } tag;  // 4: a tag — §3.3.12 deftype-invariant; tag_id = provider's store tagaddr identity (§4.2)
    } u;
} jav_extern_t;

typedef struct {
    const bbq_field_capture* root;   // borrowed: the c-lite index
    const uint8_t*           base;   // borrowed: the module image
    const jav_modidx_t*      mod;    // borrowed: the flattened index

    // All of these are crt bbq_vecs — the length rides with each (bbq_vec_len), no parallel
    // count fields. funcs: imports low then defined; sidetabs/trytabs: one per defined func.
    jav_func_t*       funcs;
    jav_st_entry_t**  sidetabs;     // owned per-defined-func side-tables
    jav_try_t**       trytabs;       // owned per-defined-func try-tables
    slot_t**          globals;       // globalinst BY REFERENCE (imports low, then defined): globals[i] aliases the
                                     //   defining instance's slot — defined → &global_store[d], imported → exporter's slot
    slot_t*           global_store;  // backing slots for THIS instance's defined globals (the pointees for defined entries)
    uint8_t*          global_types;  // parallel runtime type tag (T_INT/T_LONG/…)
    jav_tableinst_t*      tables;        // module tables (imports low, then defined), indexed by tableidx
    uint8_t*          table_borrowed; // [ntables] per-table: refs owned by an importing exporter
    jav_inst_export_t* exports;      // the export map (name → kind+index)
    uint32_t*         mem_addrs;     // module memidx → store-heap memidx (imports + defined)
    int32_t*          gcanon;        // §4.5.2 [ntypes] module typeidx → session-global canonical id (bbq_vec; the heap registry's id space)
    const struct gc_rtt** interned_rtts; // §4.5.2 [ntypes] the store-owned interned rtt per type (bbq_vec; gcanon[t] → the
                                     //   registry's persistent rtt). ctx.struct_rtts points HERE, not mod->rtts, so a live
                                     //   object's o->rtt outlives this module's arena (survives wasm_module_delete).
    uint32_t*         tag_ids;       // §4.2 [ntags] tagidx → store tagaddr identity (imports low = exporter's id, then defined = fresh)

    // Segment stashes (§4.5.6/.8): every segment is registered so memory.init / array.new_data
    // / array.new_elem can reach passive ones by index; active ones are applied then marked
    // dropped. All bbq_vecs — sized to the segment count, never a fixed cap.
    jav_data_seg_t*   data_segs;     // [ndatas] {bytes (into the image), len}
    uint8_t*          data_dropped;  // [ndatas] per-segment dropped flag (active → 1 after init)
    jav_elem_seg_t*   elem_segs;     // [nelems] {values (into elem_pool), types (into elem_tag_pool), len}
    uint8_t*          elem_dropped;  // [nelems] per-segment dropped flag (active → 1 after init; elem.drop)
    int64_t*          elem_pool;     // materialized elem ref values; elem_segs[i].values points in
    uint8_t*          elem_tag_pool; // parallel runtime tag per pooled value (T_REF | T_GCREF) — a
                                     //   T_GCREF entry in a NON-dropped segment is a GC root (§4.2.12)

    // §4.2 the engine-level execution context for this instance: the loader fills it (fill_ctx) and
    // points every defined funcinst's inst_ctx at it, so a cross-instance call switches the frame's ctx
    // to the callee's instance. §8: the engine reads cluster state straight through `frame.ctx` — this
    // struct IS the context, not a source for a separate vm cache.
    instctx_t         ctx;

    // Compiled bodies of this instance's defined funcs, when the engine was configured for the JIT
    // tier (vm->jit_enabled). A bbq_vec of jit_func_t*, one per function actually compiled; each
    // funcinst's `invoke_ctx` points at its entry. Freed with the instance (jit_free).
    struct jit_func_s** jitfns;
} jav_instance_t;

// Instantiate a module already proven §7-valid. `vm` is the engine the instantiator runs
// init-exprs on (and the start function) — the const-eval pattern, not a private scratch
// engine. `imports` is a POSITIONAL array (length `nimports`) of host-supplied externvals,
// in the module's import-vector order; each is type-matched against its declared import and
// dropped into the matching low slot. (`vm` stands in for the plan's `store`; the heap joins
// it with linear memory.) Returns JAV_OK; JAV_UNLINKABLE on an import arity/type mismatch
// (with JAV_E_INCOMPATIBLE_IMPORT/_UNKNOWN_IMPORT); JAV_UNINSTANTIABLE on an out-of-bounds
// active segment or a start trap. *err is set on failure. The image must outlive the instance.
jav_status_t jav_instantiate(vm_t* vm, const bbq_field_capture* root, const uint8_t* base,
                             const jav_modidx_t* mod, const jav_extern_t* imports, uint32_t nimports,
                             jav_instance_t* out, jav_err_t* err);

// Point the engine at this instance (vm->functions / num_functions / types / num_types …).
void jav_instance_bind(vm_t* vm, const jav_instance_t* inst);

// Look up an export by name + kind (0 func, 1 table, 2 mem, 3 global, 4 tag). -1 if absent.
int32_t jav_instance_export(const jav_instance_t* inst, const char* name, uint8_t kind);

void jav_instance_free(jav_instance_t* inst);

// Visit this instance's GC roots (its globals + tables holding T_GCREF managed objects). The store —
// the single root-scan authority over the shared heap — roots every tracked instance through this one
// definition (the engine's enum_roots covers only machine state + its currently-bound instance).
void jav_instance_visit_roots(jav_instance_t* inst, jav_root_visit_fn visit, void* ctx);

#endif // JAV_INSTANCE_H
