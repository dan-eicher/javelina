// wasm_capi.c — the W3C wasm-c-api (`wasm.h`) over the internal jav_* core.
#include <stdio.h>
//
// This is the ONE translation unit that includes the public header; every other
// file speaks the internal `jav_*` vocabulary. The bridge is thin: the public
// objects are handles that wrap (or view) the engine's own structures —
//   wasm_store_t   → a vm_t + heap_t (the execution engine + linear-memory store)
//   wasm_module_t  → owned binary + bbq_arena + c-lite index root + jav_modidx_t
//   wasm_instance_t→ a jav_instance_t (the §4.5 instance)
//   wasm_extern_t  → a {kind, instance, index} VIEW onto an instance export
// — and the runtime calls (decode/validate/instantiate/call) delegate straight to
// jav_module_index / jav_module_validate / jav_instantiate / jav_call. The value
// marshaling (wasm_val_t ↔ slot_t) is the only real translation layer.
//
// The surface is the classic wasm-c-api plus the WASM 3.0 §7.1 embedding additions it predates:
// v128 + GC/exnref values, the exception/trap split (§7.1.8), ref_type + match_valtype/externtype
// (§7.1.14/15), tags (§7.1.11), and exception objects (§7.1.12). wasm_config_t carries no standard
// fields, so it is an empty extension point.
#include "wasm.h"

#include "jav_view_nav.h"        // jav_view_module (bytes → c-lite index)
#include "jav_module_index.h"    // jav_module_index, jav_modidx_t
#include "jav_module_validate.h" // jav_module_validate
#include "jav_subtype.h"         // HT_FUNC / HT_NOFUNC heaptype codes
#include "jav_instance.h"        // jav_instantiate / jav_instance_*
#include "jav_extern.h"           // jav_project_export — the ONE externval projection (shared with the loader)
#include "heap.h"                // heap_t / jav_heap_free_mems / jav_heap_gc_init
#include "jav_hostref.h"         // host externref boxing (jav_host_box_new/_get)
#include "interp.h"              // vm_t / jav_vm_init / jav_vm_free
#include "jav_ttree.h"           // jav_ttree_stats — what the stitcher did, for the readout
#include "jav_eqsat.h"           // jav_eqsat_stats — what tier 3 rewrote, for the readout
#include "runtime_api.h"         // slot_t, T_*, jav_status_t, jav_call
#include "bbq_arena.h"
#include "bbq_vec.h"

#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// Vectors — the WASM_DECLARE_VEC machinery, defined once per element flavor.
// PTR vecs own an array of element POINTERS (each element separately owned, so
// copy/delete recurse through wasm_<name>_copy / _delete). VAL vecs own an inline
// array of element VALUES.

#define DEFINE_VEC_PTR(name) \
  void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) { out->size = 0; out->data = NULL; } \
  void wasm_##name##_vec_new_uninitialized(wasm_##name##_vec_t* out, size_t n) { \
    out->size = n; out->data = n ? calloc(n, sizeof(wasm_##name##_t*)) : NULL; } \
  void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t n, wasm_##name##_t* const src[]) { \
    wasm_##name##_vec_new_uninitialized(out, n); \
    for (size_t i = 0; i < n; i++) out->data[i] = src[i]; } \
  void wasm_##name##_vec_copy(wasm_##name##_vec_t* out, const wasm_##name##_vec_t* v) { \
    wasm_##name##_vec_new_uninitialized(out, v->size); \
    for (size_t i = 0; i < v->size; i++) out->data[i] = v->data[i] ? wasm_##name##_copy(v->data[i]) : NULL; } \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t* v) { \
    for (size_t i = 0; i < v->size; i++) if (v->data[i]) wasm_##name##_delete(v->data[i]); \
    free(v->data); v->size = 0; v->data = NULL; }

#define DEFINE_VEC_VAL(name, copy_elem, delete_elem) \
  void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) { out->size = 0; out->data = NULL; } \
  void wasm_##name##_vec_new_uninitialized(wasm_##name##_vec_t* out, size_t n) { \
    out->size = n; out->data = n ? calloc(n, sizeof(wasm_##name##_t)) : NULL; } \
  void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t n, const wasm_##name##_t src[]) { \
    wasm_##name##_vec_new_uninitialized(out, n); \
    for (size_t i = 0; i < n; i++) { copy_elem(&out->data[i], &src[i]); } } \
  void wasm_##name##_vec_copy(wasm_##name##_vec_t* out, const wasm_##name##_vec_t* v) { \
    wasm_##name##_vec_new(out, v->size, v->data); } \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t* v) { \
    for (size_t i = 0; i < v->size; i++) { delete_elem(&v->data[i]); } \
    free(v->data); v->size = 0; v->data = NULL; }

static inline void copy_byte(wasm_byte_t* d, const wasm_byte_t* s) { *d = *s; }
static inline void noop_delete(void* p) { (void)p; }

DEFINE_VEC_VAL(byte, copy_byte, noop_delete)

///////////////////////////////////////////////////////////////////////////////
// Type representations.
//
// Every extern-able type begins with a wasm_externkind_t so the wasm_X_as_externtype
// / wasm_externtype_as_X casts are a common-initial-sequence reinterpret (the
// canonical wasm-c-api trick — no allocation, the cast result is borrowed). valtype
// is the one type NOT extern-able, so it carries no kind prefix.

struct wasm_valtype_t { wasm_valkind_t kind; };

struct wasm_functype_t   { wasm_externkind_t ek; wasm_valtype_vec_t params, results; };
struct wasm_globaltype_t { wasm_externkind_t ek; wasm_valtype_t* content; wasm_mutability_t mut; };
struct wasm_tabletype_t  { wasm_externkind_t ek; wasm_valtype_t* element; wasm_limits_t limits; };
struct wasm_memorytype_t { wasm_externkind_t ek; wasm_limits_t limits; };
struct wasm_tagtype_t    { wasm_externkind_t ek; wasm_functype_t* type; };
struct wasm_externtype_t { wasm_externkind_t ek; };

struct wasm_importtype_t { wasm_name_t module, name; wasm_externtype_t* type; };
struct wasm_exporttype_t { wasm_name_t name; wasm_externtype_t* type; };

// — valtype —
wasm_valtype_t* wasm_valtype_new(wasm_valkind_t k) {
    wasm_valtype_t* t = malloc(sizeof *t); t->kind = k; return t;
}
wasm_valkind_t wasm_valtype_kind(const wasm_valtype_t* t) { return t->kind; }
wasm_valtype_t* wasm_valtype_copy(const wasm_valtype_t* t) { return wasm_valtype_new(t->kind); }
void wasm_valtype_delete(wasm_valtype_t* t) { free(t); }
DEFINE_VEC_PTR(valtype)

// — functype —
wasm_functype_t* wasm_functype_new(wasm_valtype_vec_t* params, wasm_valtype_vec_t* results) {
    wasm_functype_t* t = malloc(sizeof *t);
    t->ek = WASM_EXTERN_FUNC; t->params = *params; t->results = *results;
    params->size = 0; params->data = NULL; results->size = 0; results->data = NULL;  // ownership taken
    return t;
}
const wasm_valtype_vec_t* wasm_functype_params(const wasm_functype_t* t) { return &t->params; }
const wasm_valtype_vec_t* wasm_functype_results(const wasm_functype_t* t) { return &t->results; }
wasm_functype_t* wasm_functype_copy(const wasm_functype_t* t) {
    wasm_valtype_vec_t p, r; wasm_valtype_vec_copy(&p, &t->params); wasm_valtype_vec_copy(&r, &t->results);
    return wasm_functype_new(&p, &r);
}
void wasm_functype_delete(wasm_functype_t* t) {
    wasm_valtype_vec_delete(&t->params); wasm_valtype_vec_delete(&t->results); free(t);
}
DEFINE_VEC_PTR(functype)

// — globaltype —
wasm_globaltype_t* wasm_globaltype_new(wasm_valtype_t* content, wasm_mutability_t mut) {
    wasm_globaltype_t* t = malloc(sizeof *t); t->ek = WASM_EXTERN_GLOBAL; t->content = content; t->mut = mut; return t;
}
const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t* t) { return t->content; }
wasm_mutability_t wasm_globaltype_mutability(const wasm_globaltype_t* t) { return t->mut; }
wasm_globaltype_t* wasm_globaltype_copy(const wasm_globaltype_t* t) {
    return wasm_globaltype_new(wasm_valtype_copy(t->content), t->mut);
}
void wasm_globaltype_delete(wasm_globaltype_t* t) { wasm_valtype_delete(t->content); free(t); }
DEFINE_VEC_PTR(globaltype)

// — tabletype —
wasm_tabletype_t* wasm_tabletype_new(wasm_valtype_t* element, const wasm_limits_t* limits) {
    wasm_tabletype_t* t = malloc(sizeof *t); t->ek = WASM_EXTERN_TABLE; t->element = element; t->limits = *limits; return t;
}
const wasm_valtype_t* wasm_tabletype_element(const wasm_tabletype_t* t) { return t->element; }
const wasm_limits_t* wasm_tabletype_limits(const wasm_tabletype_t* t) { return &t->limits; }
wasm_tabletype_t* wasm_tabletype_copy(const wasm_tabletype_t* t) {
    return wasm_tabletype_new(wasm_valtype_copy(t->element), &t->limits);
}
void wasm_tabletype_delete(wasm_tabletype_t* t) { wasm_valtype_delete(t->element); free(t); }
DEFINE_VEC_PTR(tabletype)

// — memorytype —
wasm_memorytype_t* wasm_memorytype_new(const wasm_limits_t* limits) {
    wasm_memorytype_t* t = malloc(sizeof *t); t->ek = WASM_EXTERN_MEMORY; t->limits = *limits; return t;
}
const wasm_limits_t* wasm_memorytype_limits(const wasm_memorytype_t* t) { return &t->limits; }
wasm_memorytype_t* wasm_memorytype_copy(const wasm_memorytype_t* t) { return wasm_memorytype_new(&t->limits); }
void wasm_memorytype_delete(wasm_memorytype_t* t) { free(t); }
DEFINE_VEC_PTR(memorytype)

// — tagtype —
wasm_tagtype_t* wasm_tagtype_new(wasm_functype_t* type) {
    wasm_tagtype_t* t = malloc(sizeof *t); t->ek = WASM_EXTERN_TAG; t->type = type; return t;
}
const wasm_functype_t* wasm_tagtype_functype(const wasm_tagtype_t* t) { return t->type; }
wasm_tagtype_t* wasm_tagtype_copy(const wasm_tagtype_t* t) { return wasm_tagtype_new(wasm_functype_copy(t->type)); }
void wasm_tagtype_delete(wasm_tagtype_t* t) { wasm_functype_delete(t->type); free(t); }
DEFINE_VEC_PTR(tagtype)

// — externtype — the up/down casts ride the shared leading wasm_externkind_t.
wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t* t) { return t->ek; }
wasm_externtype_t* wasm_externtype_copy(const wasm_externtype_t* t) {
    switch (t->ek) {
        case WASM_EXTERN_FUNC:   return (wasm_externtype_t*)wasm_functype_copy((const wasm_functype_t*)t);
        case WASM_EXTERN_GLOBAL: return (wasm_externtype_t*)wasm_globaltype_copy((const wasm_globaltype_t*)t);
        case WASM_EXTERN_TABLE:  return (wasm_externtype_t*)wasm_tabletype_copy((const wasm_tabletype_t*)t);
        case WASM_EXTERN_MEMORY: return (wasm_externtype_t*)wasm_memorytype_copy((const wasm_memorytype_t*)t);
        case WASM_EXTERN_TAG:    return (wasm_externtype_t*)wasm_tagtype_copy((const wasm_tagtype_t*)t);
    }
    return NULL;
}
void wasm_externtype_delete(wasm_externtype_t* t) {
    if (!t) return;
    switch (t->ek) {
        case WASM_EXTERN_FUNC:   wasm_functype_delete((wasm_functype_t*)t); break;
        case WASM_EXTERN_GLOBAL: wasm_globaltype_delete((wasm_globaltype_t*)t); break;
        case WASM_EXTERN_TABLE:  wasm_tabletype_delete((wasm_tabletype_t*)t); break;
        case WASM_EXTERN_MEMORY: wasm_memorytype_delete((wasm_memorytype_t*)t); break;
        case WASM_EXTERN_TAG:    wasm_tagtype_delete((wasm_tagtype_t*)t); break;
    }
}
DEFINE_VEC_PTR(externtype)

#define EXTERNTYPE_CASTS(name, kindval) \
  wasm_externtype_t* wasm_##name##_as_externtype(wasm_##name##_t* t) { return (wasm_externtype_t*)t; } \
  const wasm_externtype_t* wasm_##name##_as_externtype_const(const wasm_##name##_t* t) { return (const wasm_externtype_t*)t; } \
  wasm_##name##_t* wasm_externtype_as_##name(wasm_externtype_t* t) { \
    return t->ek == kindval ? (wasm_##name##_t*)t : NULL; } \
  const wasm_##name##_t* wasm_externtype_as_##name##_const(const wasm_externtype_t* t) { \
    return t->ek == kindval ? (const wasm_##name##_t*)t : NULL; }

EXTERNTYPE_CASTS(functype,   WASM_EXTERN_FUNC)
EXTERNTYPE_CASTS(globaltype, WASM_EXTERN_GLOBAL)
EXTERNTYPE_CASTS(tabletype,  WASM_EXTERN_TABLE)
EXTERNTYPE_CASTS(memorytype, WASM_EXTERN_MEMORY)
EXTERNTYPE_CASTS(tagtype,    WASM_EXTERN_TAG)

// — importtype / exporttype —
wasm_importtype_t* wasm_importtype_new(wasm_name_t* module, wasm_name_t* name, wasm_externtype_t* type) {
    wasm_importtype_t* t = malloc(sizeof *t);
    t->module = *module; t->name = *name; t->type = type;
    module->size = 0; module->data = NULL; name->size = 0; name->data = NULL;
    return t;
}
const wasm_name_t* wasm_importtype_module(const wasm_importtype_t* t) { return &t->module; }
const wasm_name_t* wasm_importtype_name(const wasm_importtype_t* t) { return &t->name; }
const wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t* t) { return t->type; }
wasm_importtype_t* wasm_importtype_copy(const wasm_importtype_t* t) {
    wasm_name_t m, n; wasm_name_copy(&m, &t->module); wasm_name_copy(&n, &t->name);
    return wasm_importtype_new(&m, &n, wasm_externtype_copy(t->type));
}
void wasm_importtype_delete(wasm_importtype_t* t) {
    wasm_name_delete(&t->module); wasm_name_delete(&t->name); wasm_externtype_delete(t->type); free(t);
}
DEFINE_VEC_PTR(importtype)

wasm_exporttype_t* wasm_exporttype_new(wasm_name_t* name, wasm_externtype_t* type) {
    wasm_exporttype_t* t = malloc(sizeof *t);
    t->name = *name; t->type = type; name->size = 0; name->data = NULL;
    return t;
}
const wasm_name_t* wasm_exporttype_name(const wasm_exporttype_t* t) { return &t->name; }
const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t* t) { return t->type; }
wasm_exporttype_t* wasm_exporttype_copy(const wasm_exporttype_t* t) {
    wasm_name_t n; wasm_name_copy(&n, &t->name);
    return wasm_exporttype_new(&n, wasm_externtype_copy(t->type));
}
void wasm_exporttype_delete(wasm_exporttype_t* t) {
    wasm_name_delete(&t->name); wasm_externtype_delete(t->type); free(t);
}
DEFINE_VEC_PTR(exporttype)

///////////////////////////////////////////////////////////////////////////////
// Value marshaling: wasm_val_t ↔ the engine's slot_t (+ runtime type tag).

// A generic (ref null? heaptype) carries its heaptype in the parallel *_tidx slot; the
// c-api valkind is coarse, so the func hierarchy surfaces as funcref and everything else
// (extern + the any hierarchy + abstract refs) as externref. `ht` is ignored for non-refs.
static wasm_valkind_t valkind_of_ht(int32_t ht);   // §3.3.3 heaptype → ref valkind (defined below)
static wasm_valkind_t valkind_of_wvt(jav_valtype_t w, int32_t ht) {
    switch (w) {
        case WVT_I32: return WASM_I32;
        case WVT_I64: return WASM_I64;
        case WVT_F32: return WASM_F32;
        case WVT_F64: return WASM_F64;
        case WVT_V128: return WASM_V128;   // §4.2.2 the 16-byte lane vector
        case WVT_REF: case WVT_REF_NN:
            return valkind_of_ht(ht);      // §3.3.3 abstract heaptype → ref valkind (concrete typeidx → a GC ref)
        default: return WASM_EXTERNREF;
    }
}

// A wasm.h valkind decodes to the generic ref + its abstract heaptype (host-minted refs).
static jav_valtype_t wvt_of_valkind(wasm_valkind_t k) {
    switch (k) {
        case WASM_I32: return WVT_I32;
        case WASM_I64: return WVT_I64;
        case WASM_F32: return WVT_F32;
        case WASM_F64: return WVT_F64;
        case WASM_V128: return WVT_V128;
        default: return WVT_REF;   // a ref kind → generic (ref null heaptype); the heaptype via ht_of_valkind
    }
}
// §3.3.3 abstract heaptype code ↔ ref valkind (the two halves of the ref-type ↔ wasm.h bridge).
static wasm_valkind_t valkind_of_ht(int32_t ht) {
    switch (ht) {
        case HT_FUNC:     return WASM_FUNCREF;
        case HT_NOFUNC:   return WASM_NULLFUNCREF;
        case HT_EXTERN:   return WASM_EXTERNREF;
        case HT_NOEXTERN: return WASM_NULLEXTERNREF;
        case HT_ANY:      return WASM_ANYREF;
        case HT_EQ:       return WASM_EQREF;
        case HT_I31:      return WASM_I31REF;
        case HT_STRUCT:   return WASM_STRUCTREF;
        case HT_ARRAY:    return WASM_ARRAYREF;
        case HT_NONE:     return WASM_NULLREF;
        case HT_EXN:      return WASM_EXNREF;
        case HT_NOEXN:    return WASM_NULLEXNREF;
        default:          return WASM_ANYREF;   // a concrete typeidx: a managed GC ref (precise dynamic type via ref_type)
    }
}
static int32_t ht_of_valkind(wasm_valkind_t k) {
    switch (k) {
        case WASM_FUNCREF:       return HT_FUNC;
        case WASM_NULLFUNCREF:   return HT_NOFUNC;
        case WASM_EXTERNREF:     return HT_EXTERN;
        case WASM_NULLEXTERNREF: return HT_NOEXTERN;
        case WASM_ANYREF:        return HT_ANY;
        case WASM_EQREF:         return HT_EQ;
        case WASM_I31REF:        return HT_I31;
        case WASM_STRUCTREF:     return HT_STRUCT;
        case WASM_ARRAYREF:      return HT_ARRAY;
        case WASM_NULLREF:       return HT_NONE;
        case WASM_EXNREF:        return HT_EXN;
        case WASM_NULLEXNREF:    return HT_NOEXN;
        default:                 return HT_EXTERN;
    }
}

// A reference VALUE (§4.2.1). A funcref is the funcinst pointer it names (`fn`); a NULL
// wasm_ref_t* IS the null reference per the c-api. externref host values extend this with a
// GC host-box; an exnref/managed-GC ref each carry their own payload below.
// Embedder host-info (§ WASM_DECLARE_REF): an opaque void* + optional finalizer attached to a
// handle, returned/cleared via the *_host_info ops and run on delete. Per-handle (a fresh
// instance-export view is a distinct handle — same as the reference impl's wrapper identity).
#define HOST_INFO_FIELDS void* host_info; void (*host_fin)(void*);
// Run a handle's host-info finalizer (called from every delete path).
#define RUN_HOST_FIN(o) do { if ((o) && (o)->host_fin) (o)->host_fin((o)->host_info); } while (0)

struct wasm_ref_t {
    wasm_externkind_t kind;        // WASM_EXTERN_FUNC for a funcref
    uint8_t           is_extern;   // 1 ⇒ externref (host value), 0 ⇒ funcref
    uint8_t           is_exn;      // 1 ⇒ an exnref (host = the wasm_exception_t)
    uint8_t           is_gc;       // 1 ⇒ a managed GC reference (i31 / struct / array — §3.3.3 internal hierarchy)
    wasm_instance_t*  inst;        // (externref/exn host wrapping) the owning instance; funcrefs ride on `fn`
    const jav_func_t* fn;          // §4.2.1 a funcref IS this funcinst pointer — the SAME encoding the engine puts in a slot/table
    wasm_store_t*     fnstore;     //   + the store whose vm drives a call of it (recovered from wherever the funcref was obtained)
    void*             host;        // externref host value (or, for a host funcref, the capi_hostfn_t closure)
    // Managed GC reference (is_gc): the raw slot + its tag (T_GCREF managed object, or T_REF i31
    // scalar) + the store that roots it. `gc_raw` is scanned/updated by the collector when T_GCREF.
    int64_t           gc_raw;
    uint8_t           gc_tag;
    wasm_store_t*     gc_store;
    HOST_INFO_FIELDS
};

// An exnref result is built from the escaped/caught exn OBJECT; the arg direction installs a host
// exception back as a fresh managed exn object (both defined with the exceptions section below).
static wasm_exception_t* exn_from_engine(wasm_store_t* s, gc_obj_t* exnobj, wasm_instance_t* inst);
static gc_obj_t*         exn_install(vm_t* vm, const wasm_exception_t* e);
static wasm_valkind_t valkind_of_runtime_tag(uint8_t t);                                          // T_* → wasm valkind

// Build a funcref handle over a funcinst pointer (the §4.2.1 funcref value); a null funcref is the NULL pointer.
// A host funcinst (inst_ctx==NULL) carries its capi_hostfn_t closure in invoke_ctx, surfaced as ->host.
static wasm_ref_t* funcref_make(wasm_store_t* store, const jav_func_t* fn) {
    if (!fn) return NULL;
    wasm_ref_t* r = calloc(1, sizeof *r);
    r->kind = WASM_EXTERN_FUNC; r->fn = fn; r->fnstore = store;
    r->host = fn->inst_ctx ? NULL : fn->invoke_ctx;   // a wasm_func_new closure (host func) vs an instance func
    return r;
}
// Build an externref handle over an opaque host value.
static wasm_ref_t* externref_make(void* host) {
    wasm_ref_t* r = calloc(1, sizeof *r);
    r->is_extern = 1; r->host = host;
    return r;
}
// Build a managed GC reference handle over a live slot value (T_GCREF object or T_REF i31 scalar)
// and register it as a store GC root, so the held object survives collection for the embedder's
// lifetime of the handle (the §7.1.4 valid-runtime-object post-condition). gcref_unroot reverses it.
static void gcref_root(wasm_store_t* store, wasm_ref_t* r);   // defined with the store (gc_refs vec)
static void gcref_unroot(wasm_ref_t* r);
static wasm_ref_t* gcref_make(wasm_store_t* store, int64_t raw, uint8_t tag) {
    wasm_ref_t* r = calloc(1, sizeof *r);
    r->is_gc = 1; r->gc_raw = raw; r->gc_tag = tag; r->gc_store = store;
    if (store && tag == T_GCREF) gcref_root(store, r);   // a managed object → must be rooted
    return r;
}

// wasm_val_t → slot_t (returns the runtime tag in *tag). `vm` is needed to box a non-null
// externref (a GC host box). The box is rooted by the CALLER the instant it lands in a
// T_GCREF slot — so the caller must advance sp/store it before any further allocation.
static slot_t slot_of_val(const wasm_val_t* v, uint8_t* tag, vm_t* vm) {
    slot_t s; memset(&s, 0, sizeof s);
    switch (v->kind) {
        case WASM_I32: s.i = v->of.i32; *tag = T_INT;    break;
        case WASM_I64: s.l = v->of.i64; *tag = T_LONG;   break;
        case WASM_F32: s.f = v->of.f32; *tag = T_FLOAT;  break;
        case WASM_F64: s.d = v->of.f64; *tag = T_DOUBLE; break;
        case WASM_V128: memcpy(&s.v, v->of.v128.bytes, 16); *tag = T_V128; break;   // §4.2.2 the 16-byte lane vector
        case WASM_FUNCREF: case WASM_NULLFUNCREF: {      // §4.2.1 funcref = the funcinst pointer (the engine's slot encoding)
            wasm_ref_t* r = (wasm_ref_t*)v->of.ref;
            s.r = (r && r->fn) ? (ref_t)(uintptr_t)r->fn : (ref_t)JAV_NULLREF;
            *tag = T_REF; break;
        }
        case WASM_EXNREF: case WASM_NULLEXNREF: {        // §7.1.12 install the host exception as a managed exn object
            wasm_ref_t* r = (wasm_ref_t*)v->of.ref;
            wasm_exception_t* e = r ? wasm_ref_as_exception(r) : NULL;
            if (!e) { s.r = (ref_t)JAV_NULLREF; *tag = T_REF; break; }            // null exnref
            s.l = (s8)(uintptr_t)exn_install(vm, e); *tag = T_GCREF; break;       // rooted by the caller on store, like a host box
        }
        default: {                                       // externref / managed GC ref / i31
            wasm_ref_t* r = (wasm_ref_t*)v->of.ref;
            if (!r) { s.r = (ref_t)JAV_NULLREF; *tag = T_REF; }       // the null reference
            else if (r->is_gc) { s.l = r->gc_raw; *tag = r->gc_tag; } // managed object / i31 — passthrough its slot+tag
            else { s.l = (s8)(uintptr_t)jav_host_box_new(vm, r->host); *tag = T_GCREF; }   // externref → host box
            break;
        }
    }
    return s;
}

// slot_t → wasm_val_t under the declared result valkind. `inst` gives a funcref result its owning
// instance (and the store that roots a managed GC ref). The reference cases route on `slot_tag` —
// the value's RUNTIME representation — so a (ref eq) result that dynamically holds a struct, an
// i31, or a host externref is each marshaled faithfully (its precise type is then read by ref_type).
static void val_of_slot(wasm_val_t* out, slot_t s, wasm_valkind_t k, wasm_instance_t* inst, uint8_t slot_tag, wasm_store_t* store) {
    out->kind = k;
    switch (k) {
        case WASM_I32: out->of.i32 = s.i; break;
        case WASM_I64: out->of.i64 = s.l; break;
        case WASM_F32: out->of.f32 = s.f; break;
        case WASM_F64: out->of.f64 = s.d; break;
        case WASM_V128: memcpy(out->of.v128.bytes, &s.v, 16); break;   // §4.2.2 the 16-byte lane vector
        case WASM_FUNCREF: case WASM_NULLFUNCREF:                       // §4.2.1 funcref = the funcinst pointer in slot.r (null → JAV_NULLREF)
            out->of.ref = (struct wasm_ref_t*)funcref_make(store, JAV_REF_ISNULL(s.r) ? NULL : (const jav_func_t*)(uintptr_t)s.r); break;
        case WASM_EXNREF: case WASM_NULLEXNREF:                         // §7.1.12 exnref = a managed exn object (T_GCREF), null = T_REF
            out->of.ref = (slot_tag != T_GCREF || !s.l || !store) ? NULL
                : (struct wasm_ref_t*)wasm_exception_as_ref(exn_from_engine(store, (gc_obj_t*)(uintptr_t)s.l, inst));
            break;
        default: {                                                     // externref / managed GC ref / i31
            if (slot_tag == T_GCREF) {                                 // a managed object: host box OR a real aggregate
                gc_obj_t* o = (gc_obj_t*)(uintptr_t)s.l;
                out->of.ref = jav_is_host_box(o)
                    ? (struct wasm_ref_t*)externref_make(jav_host_box_get(o))     // host externref
                    : (struct wasm_ref_t*)gcref_make(store, s.l, T_GCREF);        // struct / array
            } else {                                                   // T_REF scalar: the null ref, or an i31
                out->of.ref = JAV_REF_ISNULL(s.r) ? NULL
                    : (struct wasm_ref_t*)gcref_make(store, s.l, T_REF);          // i31
            }
            break;
        }
    }
}

void wasm_val_delete(wasm_val_t* v) {
    if (wasm_valkind_is_ref(v->kind) && v->of.ref) wasm_ref_delete(v->of.ref);
}
void wasm_val_copy(wasm_val_t* out, const wasm_val_t* v) {
    *out = *v;
    if (wasm_valkind_is_ref(v->kind) && v->of.ref) out->of.ref = wasm_ref_copy(v->of.ref);
}
DEFINE_VEC_VAL(val, wasm_val_copy, wasm_val_delete)

///////////////////////////////////////////////////////////////////////////////
// Runtime environment: engine + store.

// Host-created (standalone) store objects — store-owned storage, kept alive for GC rooting
// and freed at store_delete (store-lifetime). Each carries its store so externref ops can
// box. A global's `tag` is its value's runtime tag (so a managed externref is GC-scanned).
typedef struct { wasm_store_t* store; slot_t val; uint8_t tag; jav_valtype_t wvt; int32_t wvt_ht; uint8_t mut; } capi_global_t;
typedef struct { wasm_store_t* store; jav_tableinst_t tab; } capi_table_t;
typedef struct { wasm_store_t* store; uint32_t memaddr; } capi_memory_t;  // memaddr: store-heap index (§4.5.2 external address)
// A host-created tag (§7.1.11 tag_alloc): its store tagaddr identity + the tag's functype, kept in
// both wasm.h form (for tag_type) and internal form (the §4.5.2 import-match key, like a host func).
typedef struct {
    wasm_store_t*    store;
    uint32_t         tag_id;      // §4.2 the fresh store tagaddr identity
    wasm_functype_t* type;        // owned: the tag's functype (params = exn value types; no results)
    jav_valtype_t*   jparams;     // owned: internal param kinds
    jav_functype_t   jtype;       // {jparams, nparams, NULL, 0} — the import-match key
} capi_tag_t;

// The embedder-options object wasm.h leaves opaque and field-less. javelina's options are the
// execution tier and the heap checker; an engine carries the choice, and every store built on it
// hands it to the instantiator, which places each defined function on the copy-and-patch tier
// (jav_extern.h). `verify_heap` runs gc_verify at the end of every collection and aborts on a
// violation — the only point at which corruption can still be attributed to the cycle that caused
// it. An option rather than a build flag: it is the release binaries the corpus runs.
struct wasm_config_t { uint8_t jit, verify_heap; };
struct wasm_engine_t { uint8_t jit, verify_heap; };
struct wasm_store_t  {
    wasm_engine_t* engine; vm_t vm; heap_t heap;
    capi_global_t** host_globals;   // bbq_vecs: host-created store objects, kept for GC rooting
    capi_table_t**  host_tables;    //   + freed at store_delete (store-lifetime)
    capi_memory_t** host_mems;      // host memories (no GC content; just freed at store_delete)
    capi_tag_t**    host_tags;      // host-created tags (wasm_tag_new) — store-lifetime, no GC content
    wasm_ref_t**     gc_refs;       // live C-held managed GC references (is_gc, T_GCREF) — GC roots
    wasm_exception_t** exn_roots;   // live host exceptions whose value payload holds managed refs — GC roots
    wasm_instance_t** insts;        // every live instance — ONE shared heap ⇒ GC must root through EVERY
                                    // instance's globals+tables, not just the vm's currently-bound one
    wasm_instance_t** trapped;      // §4.5.4: instances whose instantiation TRAPPED — they allocated
                                    // funcinsts (reachable from shared tables) so they persist for the
                                    // store's lifetime, owned here (no embedder handle was returned).
    // The §5/§4.5 verdict of the most recent decode/validate/instantiate on this store. NOT part
    // of the wasm-c-api (the spec surfaces failure only as NULL + a trap); a conformance harness
    // reads it via jav_capi_last_* to classify malformed/invalid/unlinkable/uninstantiable/trap
    // against what the .wast asserts — the ONE sanctioned non-API readout of the engine's outcome.
    jav_status_t last_status;
    jav_err_t    last_err;
    // §3.3.3 (Titzer) debug-extension probe: an embedder callback invoked before each interp opcode
    // (jav_capi_set_probe). NOT part of wasm.h — a sanctioned sidecar debug extension, like last_status.
    // Observes interp-tier execution (the debug tier); NULL = off.
    void (*probe_cb)(void* ctx, uint8_t op);
    void*  probe_ctx;
};

// Trampoline: the engine's vm-level probe (vm_t*, op) → the embedder's (ctx, op). Recovers the store
// from its embedded vm (container-of), so the embedder never sees vm_t.
static void capi_probe_tramp(vm_t* vm, uint8_t op) {
    wasm_store_t* s = (wasm_store_t*)((char*)vm - offsetof(wasm_store_t, vm));
    if (s->probe_cb) s->probe_cb(s->probe_ctx, op);
}
// §3.3.3 debug extension: install (or clear, cb=NULL) a per-opcode probe on the store's interp tier.
void jav_capi_set_probe(wasm_store_t* s, void (*cb)(void* ctx, uint8_t op), void* ctx) {
    s->probe_cb = cb; s->probe_ctx = ctx;
    s->vm.probe = cb ? capi_probe_tramp : NULL;
}
// The sanctioned non-API verdict readout (declared in jav_extern.h, never in wasm.h).
jav_status_t jav_capi_last_status(const wasm_store_t* s) { return s->last_status; }
jav_err_t    jav_capi_last_error (const wasm_store_t* s) { return s->last_err; }

// Root / unroot a C-held managed GC reference. The collector may MOVE the object, so it scans
// (and updates) `&r->gc_raw` directly; unroot swap-removes the handle from the live set.
static void gcref_root(wasm_store_t* store, wasm_ref_t* r) { bbq_vec_push(store->gc_refs, r); }
static void gcref_unroot(wasm_ref_t* r) {
    wasm_store_t* s = r->gc_store; if (!s) return;
    for (size_t i = 0, n = bbq_vec_len(s->gc_refs); i < n; i++)
        if (s->gc_refs[i] == r) { s->gc_refs[i] = s->gc_refs[n - 1]; bbq_vec_pop(s->gc_refs); return; }
}

// Root/unroot a host exception whose value payload holds managed refs (a §7.1.12 exn that escaped or
// was allocated with GC-ref values) — the engine doesn't scan its own exn store, so the c-api keeps
// the embedder-held copy alive. exn_visit_refs scans the payload (defined with the exception).
static void exn_visit_refs(wasm_exception_t* e, jav_root_visit_fn visit, void* vctx);
static int  exn_holds_refs(const wasm_exception_t* e);
static void capi_inst_roots(wasm_instance_t* in, jav_root_visit_fn visit, void* vctx);  // bridges to jav_instance_visit_roots (defined after the wasm_instance_t struct)
static void capi_track_inst(void* ctx, void* jav_inst);  // §4.7.2 step-24 alloc hook (defined after the wasm_instance_t struct)

// Extra GC roots the engine can't see: host globals/tables + C-held managed refs + host exceptions.
static void capi_extra_roots(void* ctx, jav_root_visit_fn visit, void* vctx) {
    wasm_store_t* s = ctx;
    for (size_t i = 0, n = bbq_vec_len(s->host_globals); i < n; i++)
        if (s->host_globals[i]->tag == T_GCREF) visit((struct gc_obj**)&s->host_globals[i]->val.l, vctx);
    for (size_t i = 0, n = bbq_vec_len(s->host_tables); i < n; i++) {
        jav_tableinst_t* t = &s->host_tables[i]->tab;
        for (size_t j = 0, m = bbq_vec_len(t->refs); j < m; j++)
            if (t->types[j] == T_GCREF) visit((struct gc_obj**)&t->refs[j], vctx);
    }
    for (size_t i = 0, n = bbq_vec_len(s->gc_refs); i < n; i++)
        visit((struct gc_obj**)&s->gc_refs[i]->gc_raw, vctx);   // all entries here are T_GCREF managed objects
    for (size_t i = 0, n = bbq_vec_len(s->exn_roots); i < n; i++)
        exn_visit_refs(s->exn_roots[i], visit, vctx);
    for (size_t i = 0, n = bbq_vec_len(s->insts); i < n; i++)   // every instance's globals+tables (shared heap)
        capi_inst_roots(s->insts[i], visit, vctx);
}
static void exn_root(wasm_store_t* s, wasm_exception_t* e) { if (s && exn_holds_refs(e)) bbq_vec_push(s->exn_roots, e); }
static void exn_unroot(wasm_store_t* s, wasm_exception_t* e) {
    if (!s) return;
    for (size_t i = 0, n = bbq_vec_len(s->exn_roots); i < n; i++)
        if (s->exn_roots[i] == e) { s->exn_roots[i] = s->exn_roots[n - 1]; bbq_vec_pop(s->exn_roots); return; }
}

wasm_config_t* wasm_config_new(void) { return calloc(1, sizeof(wasm_config_t)); }
void wasm_config_delete(wasm_config_t* c) { free(c); }
/* A LEVEL, not a switch: 0 interprets, 1 compiles without caching operands in
 * registers, 2 compiles with the tier-2 tiling, 3 runs the eq-sat rewrite in
 * front of the same tiling (tier-3 IS tier-2 with saturation between build
 * and reduce; zero rules make them byte-identical). Every compiled form is
 * present in the same binary — the plain stencil is the uncached form at every
 * cache size and the __sN variants sit beside it — so which one a store uses
 * is a runtime choice and only the cache SIZE is a build-time one. That is
 * what lets one binary run the same corpus four ways and compare, instead of
 * comparing builds and hoping nothing else moved between them. */
void jav_config_set_jit(wasm_config_t* c, int jit) {
    if (c) c->jit = (uint8_t)(jit < 0 ? 0 : jit > 3 ? 3 : jit);
}
void jav_config_set_verify_heap(wasm_config_t* c, int on) { if (c) c->verify_heap = on ? 1 : 0; }

wasm_engine_t* wasm_engine_new(void) { return calloc(1, sizeof(wasm_engine_t)); }
wasm_engine_t* wasm_engine_new_with_config(wasm_config_t* c) {   // consumes c (wasm.h: `own`)
    wasm_engine_t* e = wasm_engine_new();
    if (e && c) { e->jit = c->jit; e->verify_heap = c->verify_heap; }
    wasm_config_delete(c);
    return e;
}
void wasm_engine_delete(wasm_engine_t* e) { free(e); }

uint32_t jav_capi_jit_count(const wasm_store_t* s) { return s->vm.jit_compiled; }
uint32_t jav_capi_jit_declined(const wasm_store_t* s) { return s->vm.jit_declined; }

void jav_jit_cache_stats_reset(void) { jav_ttree_stats_reset(); }

uint64_t jav_capi_eqsat_rewritten(void) { return jav_eqsat_stats()->rewritten; }

int jav_capi_eqsat_rules(const char* const** names, const unsigned long long** fires) {
    return jav_eqsat_rule_stats(names, fires);
}

/* The stitch meters an embedder can read. mem_slots is the one that prices the
 * mechanism — the operand-stack accesses the cache removed — and it was the one
 * this readout did not carry, which is how the caller came to derive a figure
 * from the others instead of reporting a measured one. */
void jav_jit_cache_stats(uint64_t* cached_ops, uint64_t* deep, uint64_t* occupancy,
                         uint64_t* transitions, uint64_t* spills, uint64_t* fills,
                         uint64_t* mem_slots) {
    const jav_ttree_stats_t* t = jav_ttree_stats();
    if (cached_ops)  *cached_ops  = t->states_cached;
    if (deep)        *deep        = t->states_deep;
    if (occupancy)   *occupancy   = t->slots_cached;
    if (transitions) *transitions = t->transitions;
    if (spills)      *spills      = t->trans_spill;
    if (fills)       *fills       = t->trans_fill;
    if (mem_slots)   *mem_slots   = t->mem_slots;
}

wasm_store_t* wasm_store_new(wasm_engine_t* engine) {
    wasm_store_t* s = calloc(1, sizeof *s);
    s->engine = engine;
    jav_vm_init(&s->vm);
    s->vm.jit_enabled = engine ? engine->jit : 0;   // the embedder's tier choice, per §7.1 store creation
    s->vm.heap = &s->heap;
    s->vm.extra_roots = capi_extra_roots; s->vm.extra_roots_ctx = s;   // root host-created objects
    s->vm.on_inst_alloc = capi_track_inst; s->vm.on_inst_alloc_ctx = s;   // §4.7.2 step 24: root each instance from allocation
    jav_heap_gc_init(&s->heap, &s->vm);   // a live collector — GC modules + host externref boxes
    if (engine && engine->verify_heap) jav_heap_gc_verify(&s->heap, &s->vm, 1);
    return s;
}
void wasm_store_delete(wasm_store_t* s) {
    s->vm.extra_roots = NULL;             // stop scanning host objects before tearing them down
    s->vm.on_inst_alloc = NULL;
    for (size_t i = 0, n = bbq_vec_len(s->host_globals); i < n; i++) free(s->host_globals[i]);
    bbq_vec_free(s->host_globals);
    for (size_t i = 0, n = bbq_vec_len(s->host_tables); i < n; i++) {
        bbq_vec_free(s->host_tables[i]->tab.refs); bbq_vec_free(s->host_tables[i]->tab.types); free(s->host_tables[i]);
    }
    bbq_vec_free(s->host_tables);
    for (size_t i = 0, n = bbq_vec_len(s->host_mems); i < n; i++) free(s->host_mems[i]);
    bbq_vec_free(s->host_mems);
    for (size_t i = 0, n = bbq_vec_len(s->host_tags); i < n; i++) {
        wasm_functype_delete(s->host_tags[i]->type); free(s->host_tags[i]->jparams); free(s->host_tags[i]);
    }
    bbq_vec_free(s->host_tags);
    for (size_t i = 0, n = bbq_vec_len(s->trapped); i < n; i++) wasm_instance_delete(s->trapped[i]);
    bbq_vec_free(s->trapped);
    bbq_vec_free(s->gc_refs);   // entries are embedder-owned handles (deleted via wasm_ref_delete); free the index
    bbq_vec_free(s->exn_roots); // ditto — embedder-owned exceptions (deleted via wasm_exception_delete)
    bbq_vec_free(s->insts);     // trapped freed above; OK instances are embedder-owned — free the index
    jav_vm_free(&s->vm);
    jav_heap_gc_destroy(&s->heap);
    jav_heap_free_mems(&s->heap);
    free(s);
}

///////////////////////////////////////////////////////////////////////////////
// Modules: decode + validate (wasm_module_new), validate-only (wasm_module_validate).
// A module owns a private copy of the binary, the arena holding the c-lite index +
// flattened jav_modidx_t, and the index root — everything jav_instantiate consumes.

struct wasm_module_t {
    wasm_store_t* store;
    uint8_t* bytes; size_t nbytes;
    bbq_arena arena;
    const bbq_field_capture* root;
    jav_modidx_t mod;
    HOST_INFO_FIELDS
};

// §7.1.4 module_decode then §7.1.5 module_validate over `bytes`, recording the §5/§4.5 verdict on
// the store for the sanctioned readout (decode failure → MALFORMED, type-check failure → INVALID
// with the precise jav_err_t reason, success → OK). On JAV_OK the freshly built {arena, mod, root}
// belong to the caller (handed back via *root); on failure the caller frees the arena. ONE path so
// module_new and module_validate cannot diverge or leave the verdict stale.
static jav_status_t module_decode_validate(wasm_store_t* store, const uint8_t* bytes, size_t nbytes,
                                           bbq_arena* arena, jav_modidx_t* mod,
                                           const bbq_field_capture** root) {
    bbq_capture_metadata meta = jav_view_module(bytes, nbytes, arena);
    jav_status_t st; jav_err_t err = JAV_E_NONE;
    if (!meta.success)                                          st = JAV_MALFORMED;
    else if (!jav_module_index(meta.root, bytes, arena, mod))   st = JAV_MALFORMED;
    else st = jav_module_validate(meta.root, bytes, mod, &err);
    store->last_status = st; store->last_err = err;
    if (st == JAV_OK) *root = meta.root;
    return st;
}

wasm_module_t* wasm_module_new(wasm_store_t* store, const wasm_byte_vec_t* binary) {
    wasm_module_t* m = calloc(1, sizeof *m);
    m->store = store;
    m->nbytes = binary->size;
    m->bytes = malloc(binary->size ? binary->size : 1);
    memcpy(m->bytes, binary->data, binary->size);
    bbq_arena_init(&m->arena, 0);
    // Failure → NULL per the c-api; the §5/§4.5 verdict is recorded on the store by the shared helper.
    if (module_decode_validate(store, m->bytes, m->nbytes, &m->arena, &m->mod, &m->root) != JAV_OK) {
        bbq_arena_free(&m->arena); free(m->bytes); free(m);
        return NULL;
    }
    return m;
}

bool wasm_module_validate(wasm_store_t* store, const wasm_byte_vec_t* binary) {
    // Validate-only: run the SAME decode+validate path (so last_status is set identically to
    // module_new), then discard the built index.
    bbq_arena a; bbq_arena_init(&a, 0);
    /* Zeroed: a module that fails to decode never reaches the index, and the free
     * below still has to be able to read it. */
    jav_modidx_t mod = {0}; const bbq_field_capture* root = NULL;
    jav_status_t st = module_decode_validate(store,
        binary->data ? (const uint8_t*)binary->data : (const uint8_t*)"", binary->size, &a, &mod, &root);
    jav_modidx_free_bodies(&mod);   /* validate-only: nothing will instantiate these */
    bbq_arena_free(&a);
    return st == JAV_OK;
}

void wasm_module_delete(wasm_module_t* m) {
    /* The §7.6 tables validation kept are malloc'd, so they do not ride the arena
     * the rest of the index does. Every instance of this module BORROWS them, so
     * deleting the module while an instance lives is the same use-after-free that
     * deleting it out from under `mod.func_sigs` already is. */
    RUN_HOST_FIN(m); jav_modidx_free_bodies(&m->mod);
    bbq_arena_free(&m->arena); free(m->bytes); free(m);
}

// Reflectors (§7.1.6) — walk the export (id 7) / import (id 2) sections of the c-lite
// index for the NAMES, and reconstruct each externtype from jav_modidx_t by kind+index.
// The walk reuses jav_view_section_array, exactly as jav_instance.c builds its export map.

// A wasm_functype_t mirroring an engine functype (params/results, GC type indices preserved).
static wasm_functype_t* functype_new_from(const jav_functype_t* ft) {
    wasm_valtype_vec_t p, r;
    wasm_valtype_vec_new_uninitialized(&p, ft->nparams);
    for (uint16_t i = 0; i < ft->nparams; i++) p.data[i] = wasm_valtype_new(valkind_of_wvt(ft->params[i], ft->param_tidx ? (int32_t)ft->param_tidx[i] : 0));
    wasm_valtype_vec_new_uninitialized(&r, ft->nresults);
    for (uint16_t i = 0; i < ft->nresults; i++) r.data[i] = wasm_valtype_new(valkind_of_wvt(ft->results[i], ft->result_tidx ? (int32_t)ft->result_tidx[i] : 0));
    return wasm_functype_new(&p, &r);
}

// The externtype for an externidx (§5.5.1) kind at `idx` in that kind's index space.
static wasm_externtype_t* externtype_of(const jav_modidx_t* m, uint8_t kind, uint32_t idx) {
    switch (kind) {
    case 0:                                            // func
        return wasm_functype_as_externtype(functype_new_from(&m->func_sigs[idx]));
    case 1: {                                          // table
        wasm_limits_t lim = { (uint32_t)m->table_min[idx],
                              m->table_has_max[idx] ? (uint32_t)m->table_max[idx] : wasm_limits_max_default };
        return wasm_tabletype_as_externtype(wasm_tabletype_new(wasm_valtype_new(valkind_of_wvt(m->table_reftype[idx], m->table_tidx ? (int32_t)m->table_tidx[idx] : 0)), &lim));
    }
    case 2: {                                          // memory
        wasm_limits_t lim = { (uint32_t)m->mem_min[idx],
                              m->mem_has_max[idx] ? (uint32_t)m->mem_max[idx] : wasm_limits_max_default };
        return wasm_memorytype_as_externtype(wasm_memorytype_new(&lim));
    }
    case 3:                                            // global
        return wasm_globaltype_as_externtype(
            wasm_globaltype_new(wasm_valtype_new(valkind_of_wvt(m->global_types[idx], m->global_tidx ? (int32_t)m->global_tidx[idx] : 0)),
                                m->global_mut[idx] ? WASM_VAR : WASM_CONST));
    case 4:                                            // tag (§5.5.16): carries a functype (exn signature)
        return wasm_tagtype_as_externtype(wasm_tagtype_new(functype_new_from(&m->tags[idx])));
    default: return NULL;
    }
}

static void name_of_span(wasm_name_t* out, const uint8_t* base, const bbq_field_capture* node, const char* field) {
    const bbq_field_capture* bn = jav_view_field(jav_view_field(node, field), "bytes");
    wasm_name_new(out, (size_t)(bn->end_offset - bn->start_offset), (const char*)(base + bn->start_offset));
}

void wasm_module_exports(const wasm_module_t* mod, wasm_exporttype_vec_t* out) {
    const bbq_field_capture* ex = jav_view_section_array(mod->root, 7, "exports", mod->bytes);
    uint32_t n = ex ? jav_view_nchild(ex) : 0;
    wasm_exporttype_vec_new_uninitialized(out, n);
    for (uint32_t i = 0; i < n; i++) {
        const bbq_field_capture* e = &ex->children[i];
        wasm_name_t name; name_of_span(&name, mod->bytes, e, "name");
        uint8_t kind = (uint8_t)bbq_node_int(jav_view_field(e, "kind"), mod->bytes);
        uint32_t idx = (uint32_t)bbq_node_int(jav_view_field(e, "idx"), mod->bytes);
        out->data[i] = wasm_exporttype_new(&name, externtype_of(&mod->mod, kind, idx));
    }
}

void wasm_module_imports(const wasm_module_t* mod, wasm_importtype_vec_t* out) {
    const bbq_field_capture* im = jav_view_section_array(mod->root, 2, "imports", mod->bytes);
    uint32_t n = im ? jav_view_nchild(im) : 0;
    wasm_importtype_vec_new_uninitialized(out, n);
    uint32_t cur[5] = {0};   // per-kind low-slot cursors: func, table, mem, global, tag
    for (uint32_t i = 0; i < n; i++) {
        const bbq_field_capture* e = &im->children[i];
        wasm_name_t modn, namn;
        name_of_span(&modn, mod->bytes, e, "module");
        name_of_span(&namn, mod->bytes, e, "field");
        uint8_t kind = (uint8_t)bbq_node_int(jav_view_field(jav_view_field(e, "desc"), "kind"), mod->bytes);
        uint32_t idx = (kind < 5) ? cur[kind]++ : 0;
        out->data[i] = wasm_importtype_new(&modn, &namn, externtype_of(&mod->mod, kind, idx));
    }
}

///////////////////////////////////////////////////////////////////////////////
// Externals + the runtime objects they view. A func/global/table/memory handle is
// a borrowed VIEW into an instance: {kind, instance, index}. All five structs share
// the same leading layout so the wasm_X_as_extern / wasm_extern_as_X casts are a
// common-initial-sequence reinterpret (mirrors the type-object trick above).

// A handle is either an instance-export VIEW (inst!=NULL, index into it) or a host-created
// object (host!=NULL — for funcs, the wasm_func_new closure). `host` is NULL for views.
#define OBJ_VIEW_FIELDS wasm_externkind_t kind; wasm_instance_t* inst; uint32_t index; void* host; HOST_INFO_FIELDS
struct wasm_extern_t { OBJ_VIEW_FIELDS };
// owns_host: 1 ⇒ this handle owns the host closure (frees it on delete); 0 ⇒ a borrowed view (wasm_ref_as_func).
// fn/fnstore: set ONLY when this func came from a funcref recovered off a table/global (host==NULL && inst==NULL) —
// it is then called by funcinst reference (jav_invoke_fn). An extern-cast func (wasm_extern_as_func) is allocated
// extern-sized, so these trailing fields are read ONLY on that third path, which such a func never reaches.
struct wasm_func_t   { OBJ_VIEW_FIELDS uint8_t owns_host; const jav_func_t* fn; wasm_store_t* fnstore; };
struct wasm_global_t { OBJ_VIEW_FIELDS };
struct wasm_table_t  { OBJ_VIEW_FIELDS };
struct wasm_memory_t { OBJ_VIEW_FIELDS };
struct wasm_tag_t    { OBJ_VIEW_FIELDS };   // host (host=capi_tag_t) or instance-export view {inst,index}

struct wasm_instance_t {
    wasm_store_t*        store;
    const wasm_module_t* module;   // borrowed — for func signatures via mod.func_sigs
    jav_instance_t       inst;
    HOST_INFO_FIELDS
};

// §4.2.3: one shared GC heap ⇒ every instance's globals+tables are roots (engine scans only the bound
// one). Bridge the c-api wrapper to the single per-instance scan defined in jav_instance.c.
static void capi_inst_roots(wasm_instance_t* in, jav_root_visit_fn visit, void* vctx) {
    jav_instance_visit_roots(&in->inst, visit, vctx);
}

// §4.7.2 step 24 (allocmodule): jav_instantiate calls this the moment the instance is fully allocated —
// BEFORE active element/data segments + start run (steps 27-29) — so it is a GC root (via capi_inst_roots
// over s->insts) during any collection those steps trigger. The hook hands back the jav_instance_t*;
// recover its wasm_instance_t wrapper (inst is embedded by value) to track. A trapped instance is thus
// already rooted here → it PERSISTS; a link error returns before step 24, so it never registers.
static void capi_track_inst(void* ctx, void* jav_inst) {
    wasm_store_t* s = ctx;
    wasm_instance_t* in = (wasm_instance_t*)((char*)jav_inst - offsetof(wasm_instance_t, inst));
    bbq_vec_push(s->insts, in);
}

// The shared functype of func `index`, as a fresh wasm_functype_t.
static wasm_functype_t* functype_of(const wasm_instance_t* in, uint32_t index) {
    const jav_functype_t* ft = &in->module->mod.func_sigs[index];
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, ft->nparams);
    for (uint16_t i = 0; i < ft->nparams; i++) params.data[i] = wasm_valtype_new(valkind_of_wvt(ft->params[i], ft->param_tidx ? (int32_t)ft->param_tidx[i] : 0));
    wasm_valtype_vec_new_uninitialized(&results, ft->nresults);
    for (uint16_t i = 0; i < ft->nresults; i++) results.data[i] = wasm_valtype_new(valkind_of_wvt(ft->results[i], ft->result_tidx ? (int32_t)ft->result_tidx[i] : 0));
    return wasm_functype_new(&params, &results);
}

///////////////////////////////////////////////////////////////////////////////
// Host functions (§7.1.8 func_alloc, §4.2.7 a host function maps an argument val* to a
// result val* / trap). A wasm_func_new closure rides the engine's ONE dispatch seam:
// it is installed as a jav_func_t.invoke, so the VM calls it exactly like any other
// function (no special-casing in the core). The thunk is fully type-faithful — it reads
// the param/result kinds from the functype rather than assuming a single i32 result.
// It is one instance of the engine's single host ABI: a host import is just an `invoke`
// thunk (`jav_status_t(vm,h,ctx)`) that reads frame.locals and writes the typed result.

typedef struct {
    wasm_store_t*   store;
    wasm_functype_t* type;        // owned: param/result kinds for marshaling
    jav_valtype_t*  jparams;      // owned: the internal-functype param/result arrays
    jav_valtype_t*  jresults;     //   (so jav_instantiate can type-match the import)
    uint32_t*       jparam_tidx;  // owned: parallel heaptype of each ref param/result — the abstract
    uint32_t*       jresult_tidx; //   HT_* code (negative), WITHOUT which the closed type is incomplete
    jav_functype_t  jtype;        // {jparams,nparams,jresults,nresults,jparam_tidx,jresult_tidx} — §4.5.2 key
    jav_func_t      funcinst;     // §4.2.1 the canonical funcinst backing this host func: a funcref IS &hf->funcinst,
                                  //   and it is what an import/table entry holds, so funcref encoding is uniform with instance funcs
    wasm_func_callback_t          cb;       // wasm_func_new
    wasm_func_callback_with_env_t cb_env;   // wasm_func_new_with_env
    void* env;
    void (*finalizer)(void*);
} capi_hostfn_t;

static jav_status_t capi_host_invoke(vm_t* vm, heap_t* h, void* ctx);   // the host thunk (defined below; the funcinst's invoke)

static capi_hostfn_t* hostfn_new(wasm_store_t* store, const wasm_functype_t* type) {
    capi_hostfn_t* hf = calloc(1, sizeof *hf);
    hf->store = store;
    hf->type = wasm_functype_copy(type);
    uint16_t np = (uint16_t)hf->type->params.size, nr = (uint16_t)hf->type->results.size;
    hf->jparams  = np ? calloc(np, sizeof(jav_valtype_t)) : NULL;
    hf->jresults = nr ? calloc(nr, sizeof(jav_valtype_t)) : NULL;
    hf->jparam_tidx  = np ? calloc(np, sizeof(uint32_t)) : NULL;   // 0 for non-refs (unused)
    hf->jresult_tidx = nr ? calloc(nr, sizeof(uint32_t)) : NULL;
    for (uint16_t i = 0; i < np; i++) {
        wasm_valkind_t k = wasm_valtype_kind(hf->type->params.data[i]);
        hf->jparams[i] = wvt_of_valkind(k);                        // a ref's abstract heaptype rides param_tidx,
        if (wasm_valkind_is_ref(k)) hf->jparam_tidx[i] = (uint32_t)ht_of_valkind(k);   // so the closed type is complete
    }
    for (uint16_t i = 0; i < nr; i++) {
        wasm_valkind_t k = wasm_valtype_kind(hf->type->results.data[i]);
        hf->jresults[i] = wvt_of_valkind(k);
        if (wasm_valkind_is_ref(k)) hf->jresult_tidx[i] = (uint32_t)ht_of_valkind(k);
    }
    hf->jtype = (jav_functype_t){ hf->jparams, np, hf->jresults, nr, hf->jparam_tidx, hf->jresult_tidx };
    hf->funcinst = (jav_func_t){ .invoke = capi_host_invoke, .invoke_ctx = hf, .num_params = np,
                                 .num_results = nr, .sig = &hf->jtype, .inst_ctx = NULL };   // §4.6.2 sig = the type check
    return hf;
}
static void hostfn_delete(capi_hostfn_t* hf) {
    if (!hf) return;
    if (hf->finalizer) hf->finalizer(hf->env);
    wasm_functype_delete(hf->type);
    free(hf->jparams); free(hf->jresults); free(hf->jparam_tidx); free(hf->jresult_tidx); free(hf);
}

// Run the host callback as a VM function: args are this frame's locals (the engine copied
// the params there); marshal them to wasm_val_t, invoke, then deliver results the way any
// callee does — contiguous frame-stack slots from the base, for EVERY arity (the return
// path in jav_runtime.c reads exactly there). A returned trap aborts the run.
static jav_status_t capi_host_invoke(vm_t* vm, heap_t* h, void* ctx) {
    (void)h;
    capi_hostfn_t* hf = ctx;
    uint16_t np = (uint16_t)hf->type->params.size, nr = (uint16_t)hf->type->results.size;

    wasm_val_t* abuf = np ? calloc(np, sizeof(wasm_val_t)) : NULL;
    for (uint16_t i = 0; i < np; i++)
        val_of_slot(&abuf[i], vm->frame.locals[i], wasm_valtype_kind(hf->type->params.data[i]), NULL,
                    vm->frame.local_types[i], NULL);   // host-call args: already rooted as live stack slots
    wasm_val_t* rbuf = nr ? calloc(nr, sizeof(wasm_val_t)) : NULL;
    wasm_val_vec_t args = { np, abuf }, results = { nr, rbuf };

    wasm_trap_t* trap = hf->cb ? hf->cb(&args, &results) : hf->cb_env(hf->env, &args, &results);
    for (uint16_t i = 0; i < np; i++) wasm_val_delete(&abuf[i]);   // engine owns the marshaled-in args (a ref's externref handle)
    free(abuf);
    if (trap) {
        for (uint16_t i = 0; i < nr; i++) wasm_val_delete(&rbuf[i]);
        /* Keep the cause before the trap dies. The engine raises traps by reason code, so the
         * object cannot travel; its message is the only part that names why the host refused. */
        wasm_message_t hm = WASM_EMPTY_VEC;
        wasm_trap_message(trap, &hm);          /* the accessor: the struct is defined below this */
        if (hm.size && hm.data) {
            size_t n = hm.size;
            if (n >= sizeof vm->host_trap) n = sizeof vm->host_trap - 1;
            memcpy(vm->host_trap, hm.data, n);
            vm->host_trap[n] = '\0';
        }
        if (hm.size) wasm_byte_vec_delete(&hm);
        wasm_trap_delete(trap); free(rbuf);
        vm->trapped = 1; vm->frame.code.pos = vm->frame.code.length;
        return JAV_TRAP;
    }
    // Results land contiguously at the frame stack base — the SAME place a wasm callee
    // leaves them — advancing sp so each box is a GC root before the next is built.
    // One path for every arity: a single result is not a special case (that was the
    // single-return-value shape inherited from JCVM, which WASM's multi-value broke).
    for (uint16_t i = 0; i < nr; i++) {
        uint8_t tag; vm->frame.stack[i] = slot_of_val(&results.data[i], &tag, vm);
        vm->frame.stack_types[i] = tag; vm->frame.sp = (u4)(i + 1);
    }
    for (uint16_t i = 0; i < nr; i++) wasm_val_delete(&rbuf[i]);   // results consumed into engine slots; free the host-produced handles
    free(rbuf);
    return JAV_RETURN;
}

///////////////////////////////////////////////////////////////////////////////
// Traps — a trap owns a (null-terminated) message string.

struct wasm_trap_t {
    wasm_message_t message;
    wasm_instance_t* inst;      // the instance the trap occurred in (for frame_instance)
    uint32_t* frames; size_t nframes;   // func indices, innermost-first (the trap stack trace)
    uint32_t* frame_pcs;                // parallel to frames: each frame's byte offset (§7.1.8 func_offset)
    uint8_t is_exn;             // §7.1.8: 1 ⇒ an uncaught WASM exception escaped (the `exception` outcome), not a trap
    wasm_exception_t* exn;      // §7.1.12: when is_exn, the escaped exception (tag + values), owned by the trap
    HOST_INFO_FIELDS
};

static wasm_trap_t* trap_make(const char* msg) {
    wasm_trap_t* t = calloc(1, sizeof *t);
    wasm_name_new_from_string_nt(&t->message, msg);
    return t;
}
wasm_trap_t* wasm_trap_new(wasm_store_t* store, const wasm_message_t* message) {
    (void)store;
    wasm_trap_t* t = calloc(1, sizeof *t);
    wasm_name_copy(&t->message, message);
    return t;
}
void wasm_trap_message(const wasm_trap_t* t, wasm_message_t* out) { wasm_name_copy(out, &t->message); }
bool wasm_trap_is_exception(const wasm_trap_t* t) { return t && t->is_exn; }   // §7.1.8 the `exception` outcome vs a trap
static wasm_frame_t* capi_frame_make(wasm_instance_t* inst, uint32_t func_index, uint32_t pc);  // (defined with wasm_frame_t)
wasm_frame_t* wasm_trap_origin(const wasm_trap_t* t) {            // the innermost (trapping) frame
    return (t && t->nframes) ? capi_frame_make(t->inst, t->frames[0], t->frame_pcs[0]) : NULL;
}
void wasm_trap_trace(const wasm_trap_t* t, wasm_frame_vec_t* out) {   // innermost-first
    size_t n = t ? t->nframes : 0;
    wasm_frame_vec_new_uninitialized(out, n);
    for (size_t i = 0; i < n; i++) out->data[i] = capi_frame_make(t->inst, t->frames[i], t->frame_pcs[i]);
}
void wasm_trap_delete(wasm_trap_t* t) {
    if (t) { RUN_HOST_FIN(t); wasm_name_delete(&t->message); free(t->frames); free(t->frame_pcs); wasm_exception_delete(t->exn); free(t); }
}
// §7.1.8/§7.1.12: the exception object of an `exception` outcome (NULL for a plain trap). Borrowed —
// owned by the trap, valid until wasm_trap_delete.
wasm_exception_t* wasm_trap_exception(const wasm_trap_t* t) { return t ? t->exn : NULL; }

///////////////////////////////////////////////////////////////////////////////
// Instances: jav_instantiate over positional, type-matched host imports.

static uint8_t externkind_of_jav_export(uint8_t k) {
    switch (k) {
        case 0: return WASM_EXTERN_FUNC;
        case 1: return WASM_EXTERN_TABLE;
        case 2: return WASM_EXTERN_MEMORY;
        case 3: return WASM_EXTERN_GLOBAL;
        case 4: return WASM_EXTERN_TAG;                // §5.5.16 tags ARE a first-class externval
        default: return WASM_EXTERN_FUNC;
    }
}

// Marshal one public import handle → the positional jav_extern_t the instantiator matches
// against the module's declared import (§4.5.2). Host funcs install the capi thunk; the
// other kinds re-export an instance view's underlying storage. Returns 0 on a handle we
// can't marshal (→ caller reports unlinkable rather than mis-linking).
static int marshal_import(const wasm_extern_t* e, jav_extern_t* out) {
    memset(out, 0, sizeof *out);
    // Re-export of a module instance's export → the ONE shared projection (§4.5.2):
    // reads the live entity off the exporting instance, INCLUDING tags + closed-type/identity work,
    // so the API path is the proven loader, not a divergent clone.
    if (e->inst && !e->host) {
        uint8_t jk = e->kind == WASM_EXTERN_FUNC ? 0 : e->kind == WASM_EXTERN_TABLE ? 1
                   : e->kind == WASM_EXTERN_MEMORY ? 2 : e->kind == WASM_EXTERN_GLOBAL ? 3 : 4;
        jav_project_export(&e->inst->store->heap, &e->inst->inst, &e->inst->module->mod, jk, e->index, out);
        return 1;
    }
    switch (e->kind) {   // host-created externs: built from the capi host object (no defining instance to project)
    case WASM_EXTERN_FUNC: {
        capi_hostfn_t* hf = e->host; if (!hf) return 0;   // a wasm_func_new closure
        out->kind = 0;
        out->u.func.type = &hf->jtype;
        out->u.func.func = hf->funcinst;   // the canonical host funcinst (carries sig for call_indirect of an imported host func)
        return 1;
    }
    case WASM_EXTERN_GLOBAL: {
        capi_global_t* cg = e->host; if (!cg) return 0;   // host-created global (own slot, abstract type)
        out->kind = 3;
        out->u.global.slot = &cg->val; out->u.global.type = cg->wvt;
        out->u.global.type_ht = cg->wvt_ht; out->u.global.mut = cg->mut;
        out->u.global.tag = cg->tag;   // the host value's runtime tag (managed refs → T_GCREF)
        return 1;
    }
    case WASM_EXTERN_TABLE: {
        capi_table_t* ct = e->host; if (!ct) return 0;
        jav_tableinst_t* ti = &ct->tab;
        out->kind = 1;
        out->u.table.data = ti->refs; out->u.table.types = ti->types;
        out->u.table.size = (uint32_t)bbq_vec_len(ti->refs);
        out->u.table.reftype = ti->reftype; out->u.table.reftype_ht = ti->reftype_ht;
        out->u.table.is64 = ti->is64;                                  // §3.3.15 addrtype
        out->u.table.has_max = ti->has_max; out->u.table.max = ti->max;
        return 1;
    }
    case WASM_EXTERN_MEMORY: {
        capi_memory_t* cm = e->host; if (!cm) return 0;
        jav_mem_t* m = &cm->store->heap.mems[cm->memaddr];
        out->kind = 2;
        out->u.mem.memidx = cm->memaddr;
        out->u.mem.min = m->size / MEMORY_PAGE_SIZE;
        out->u.mem.has_max = m->has_max; out->u.mem.max = m->max / MEMORY_PAGE_SIZE;
        out->u.mem.is64 = m->is64;
        return 1;
    }
    case WASM_EXTERN_TAG: {
        capi_tag_t* ct = e->host; if (!ct) return 0;      // host tag (wasm_tag_new)
        out->kind = 4;
        out->u.tag.type = &ct->jtype;                     // §4.5.2 host → intern the functype (the SAME relation)
        out->u.tag.gcanon = NULL; out->u.tag.typeidx = 0;
        out->u.tag.tag_id = ct->tag_id;                   // §4.2 the tag's store identity
        return 1;
    }
    default: return 0;
    }
}

wasm_instance_t* wasm_instance_new(wasm_store_t* store, const wasm_module_t* module,
                                   const wasm_extern_vec_t* imports, wasm_trap_t** trap_out) {
    if (trap_out) *trap_out = NULL;
    uint32_t nimports = imports ? (uint32_t)imports->size : 0;

    jav_extern_t* externs = nimports ? calloc(nimports, sizeof(jav_extern_t)) : NULL;
    for (uint32_t i = 0; i < nimports; i++) {
        if (!marshal_import(imports->data[i], &externs[i])) {
            store->last_status = JAV_UNLINKABLE; store->last_err = JAV_E_NONE;
            if (trap_out) *trap_out = trap_make("unlinkable: unsupported import value");
            free(externs); return NULL;
        }
    }

    wasm_instance_t* in = calloc(1, sizeof *in);
    in->store = store;
    in->module = module;
    jav_err_t err = JAV_E_NONE;
    jav_status_t s = jav_instantiate(&store->vm, module->root, module->bytes, &module->mod,
                                     externs, nimports, &in->inst, &err);
    free(externs);
    store->last_status = s; store->last_err = err;
    if (s != JAV_OK) {
        /* Name the §7.10 reason the start function raised, as the invoke path does. Reporting a
         * bare "instantiation trapped" hides which check failed, and §4.5.4's two failure modes
         * (a segment out of bounds, the start function trapping) are told apart by it. */
        if (trap_out) {
            wasm_trap_t* t = trap_make(
                s != JAV_UNINSTANTIABLE  ? "unlinkable module"
              : store->vm.engine_fault   ? store->vm.engine_fault
              : store->vm.exhausted      ? store->vm.exhausted
              : jav_trap_reason_str((jav_trap_reason_t)store->vm.trap_reason));
            /* §7.1.8 the frame trace, as the invoke path attaches it: the start function is a
             * call like any other, and the funcidx/offset chain is what says WHERE it trapped.
             * The frames name funcs of THIS instance, so they need it — and it is only still
             * alive for UNINSTANTIABLE, where §4.5.4 keeps the trapped instance in the store. */
            if (s == JAV_UNINSTANTIABLE) t->inst = in;
            size_t n = (size_t)bbq_vec_len(store->vm.trap_trace);
            t->nframes = n; t->frames = n ? malloc(n * sizeof(uint32_t)) : NULL;
            t->frame_pcs = n ? malloc(n * sizeof(uint32_t)) : NULL;
            for (size_t i = 0; i < n; i++) {
                t->frames[i] = store->vm.trap_trace[i]; t->frame_pcs[i] = store->vm.trap_pcs[i];
            }
            *trap_out = t;
        }
        // §4.5.4: a trapped instantiation (UNINSTANTIABLE) still ALLOCATED its instance + ran the
        // active segments preceding the trap — those funcinsts may be reachable from a shared table,
        // so the instance persists for the store's lifetime (no embedder handle was returned, so the
        // store owns it). UNLINKABLE allocated nothing persistent → free now.
        if (s == JAV_UNINSTANTIABLE) { bbq_vec_push(store->trapped, in); return NULL; }   // already in store->insts (step-24 hook) → persists
        jav_instance_free(&in->inst); free(in);
        return NULL;
    }
    return in;   // already tracked in store->insts by the §4.7.2 step-24 hook (capi_track_inst)
}

void wasm_instance_exports(const wasm_instance_t* in, wasm_extern_vec_t* out) {
    size_t n = (size_t)bbq_vec_len(in->inst.exports);
    wasm_extern_vec_new_uninitialized(out, n);
    for (size_t i = 0; i < n; i++) {
        const jav_inst_export_t* e = &in->inst.exports[i];
        wasm_extern_t* x = calloc(1, sizeof *x);
        x->kind = externkind_of_jav_export(e->kind);
        x->inst = (wasm_instance_t*)in;
        x->index = e->index;
        out->data[i] = x;
    }
}

void wasm_instance_delete(wasm_instance_t* in) {
    if (!in) return;
    wasm_store_t* s = in->store;                              // stop rooting it before its storage goes
    for (size_t i = 0, n = bbq_vec_len(s->insts); i < n; i++)
        if (s->insts[i] == in) { s->insts[i] = s->insts[n - 1]; bbq_vec_pop(s->insts); break; }
    if (s->vm.frame.ctx == &in->inst.ctx) s->vm.frame.ctx = &s->vm.cluster;  // §8: don't leave frame.ctx dangling
    RUN_HOST_FIN(in);
    jav_instance_free(&in->inst);
    free(in);
}

///////////////////////////////////////////////////////////////////////////////
// Functions: type reflection + call (marshal args → frame, jav_call, marshal back).

// The func's signature: a host closure, a funcref recovered from a table/global, or an instance-export view.
static const jav_functype_t* func_sig(const wasm_func_t* f) {
    if (f->host) return &((capi_hostfn_t*)f->host)->jtype;
    if (f->inst) return &f->inst->module->mod.func_sigs[f->index];   // export view (extern-sized: must not read fn)
    return f->fn->sig;                                   // §4.5.2 recovered funcref — the funcinst carries its functype
}

wasm_functype_t* wasm_func_type(const wasm_func_t* f) {
    if (f->host) return wasm_functype_copy(((capi_hostfn_t*)f->host)->type);
    if (f->inst) return functype_of(f->inst, f->index);
    return functype_new_from(f->fn->sig);
}
size_t wasm_func_param_arity(const wasm_func_t* f)  { return func_sig(f)->nparams; }
size_t wasm_func_result_arity(const wasm_func_t* f) { return func_sig(f)->nresults; }

// §7.1.8 invoke typing: an argument value's type must be a subtype of the parameter type. The
// path-independent half: a numeric param needs exactly its kind; a ref param needs a ref value
// (never a number — that is the type-confusion / wild-deref case the host entry must fail closed on).
static bool arg_kind_admissible(jav_valtype_t pw, wasm_valkind_t got) {
    switch (pw) {
        case WVT_I32: case WVT_I64: case WVT_F32: case WVT_F64: case WVT_V128:
            return got == valkind_of_wvt(pw, 0);
        default:  // WVT_REF / WVT_REF_NN
            return wasm_valkind_is_ref(got);
    }
}
// The top hierarchy (0 internal/any, 1 func, 2 extern, 3 exn) of a ref value's wasm_valkind, so a
// ref from the wrong hierarchy is rejected before the §3.3 lattice check reinterprets its bits.
static int valkind_ref_hier(wasm_valkind_t k) {
    switch (k) {
        case WASM_FUNCREF:   case WASM_NULLFUNCREF:   return 1;
        case WASM_EXTERNREF: case WASM_NULLEXTERNREF: return 2;
        case WASM_EXNREF:    case WASM_NULLEXNREF:    return 3;
        default:                                       return 0;   // any/eq/i31/struct/array/none
    }
}

wasm_trap_t* wasm_func_call(const wasm_func_t* f, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    const jav_functype_t* ft = func_sig(f);
    if (args->size != (size_t)ft->nparams)                 // §7.1.8: the argument count must match
        return trap_make("wrong number of arguments");

    // A host-created func (wasm_func_new) is called directly through its callback — no module
    // instance is involved. Its param types are the embedder's declared (abstract) valtypes, so
    // §7.1.15 subtyping uses the c-api's own ref_type + match_valtype.
    if (f->host) {
        capi_hostfn_t* hf = f->host;
        for (size_t i = 0; i < args->size; i++) {
            const wasm_valtype_t* pt = hf->type->params.data[i];   // the declared parameter type
            wasm_valkind_t pk = wasm_valtype_kind(pt), gk = args->data[i].kind;
            if (!wasm_valkind_is_ref(pk)) {                        // numeric: the kind must match exactly
                if (gk != pk) return trap_make("argument type mismatch");
                continue;
            }
            if (!wasm_valkind_is_ref(gk))                          // ref param: a number is never a ref
                return trap_make("argument type mismatch");
            // §7.1.15: the value's runtime type (a null ref's static type) must be ≤ the parameter type.
            wasm_valtype_t* at = args->data[i].of.ref ? wasm_ref_type(hf->store, (wasm_ref_t*)args->data[i].of.ref)
                                                       : wasm_valtype_new(gk);
            bool ok = wasm_match_valtype(hf->store, at, pt);
            wasm_valtype_delete(at);
            if (!ok) return trap_make("argument type mismatch");
        }
        wasm_trap_t* trap = hf->cb ? hf->cb(args, results) : hf->cb_env(hf->env, args, results);
        return trap;
    }

    // Resolve the engine driver + the funcinst to call. An instance-export view binds THIS instance
    // and calls its funcidx; a funcref recovered off a table/global has only the §4.2.1 funcinst
    // pointer — but the funcinst is self-describing (jav_call_fn switches the vm to its defining
    // instance), so binding fn->inst_ctx as the root frame and invoking by reference subsumes both.
    vm_t* vm; const jav_func_t* fn; wasm_instance_t* in; wasm_store_t* store; frame_t outer;
    if (f->inst) {   // instance-export view (extern-sized: must not read the trailing fn field)
        in = f->inst; store = in->store; vm = &store->vm; fn = &in->inst.funcs[f->index];
        outer = vm->frame;                            // §8/A3: save the (possibly RE-ENTRANT) caller's
        jav_instance_bind(vm, &in->inst);             //   activation BEFORE we repoint the frame's ctx +
    } else {         // a funcref recovered off a table/global: drive the call on its store's vm
        store = f->fnstore; vm = &store->vm; fn = f->fn; in = NULL;
        outer = vm->frame;                            //   overwrite its operand stack — restored at every exit,
        if (fn->inst_ctx) vm->frame.ctx = fn->inst_ctx;  // so a host callback re-entering the engine can't
    }                                                 //   corrupt the suspended guest's context (frame.ctx rides the frame).

    // A heap the checker has already condemned does not get to run guest code again. Refusing
    // here — before any argument is boxed into it — is what keeps a detected corruption from
    // becoming an exploited one, and it costs the host nothing but a dead store.
    if (vm->engine_fault) { vm->frame = outer; return trap_make(vm->engine_fault); }

    // Push args as the initial operand stack (params occupy the low slots). sp advances per
    // arg so an externref box already placed is a GC root before the next box is allocated.
    vm->frame.sp = 0;
    vm->frame.num_locals = 0;
    for (size_t i = 0; i < args->size; i++) {
        jav_valtype_t pw = ft->params[i];
        if (!arg_kind_admissible(pw, args->data[i].kind))     // numeric-exact + reject number-for-ref
            { vm->frame = outer; return trap_make("argument type mismatch"); }
        uint8_t tag;
        vm->frame.stack[i] = slot_of_val(&args->data[i], &tag, vm);
        vm->frame.stack_types[i] = tag;
        vm->frame.sp = (u4)(i + 1);
        if (pw == WVT_REF || pw == WVT_REF_NN) {              // §3.3: the value type must be ≤ (ref null? ht)
            s4 ht = ft->param_tidx ? (s4)ft->param_tidx[i] : (s4)HT_ANY;
            if (valkind_ref_hier(args->data[i].kind) != jav_ht_hierarchy(vm, ht) ||
                !jav_top_ref_matches(vm, vm->heap, ht, pw == WVT_REF))
                { vm->frame = outer; return trap_make("argument type mismatch"); }
        }
    }

    // The engine owns the call ABI + §7.1.8 outcome classification (jav_invoke_fn); the shim only turns
    // the outcome into a wasm_trap_t and marshals results — it no longer reads vm->unwinding/pending_exn.
    gc_obj_t* escaped = NULL;
    switch (jav_invoke_fn(vm, vm->heap, fn, &escaped)) {
    case JAV_INVOKE_TRAP: {                               // capture the trap's func-index trace
        // The message is the §7.10 reason the raise site named, which is what a
        // conformance runner matches an assert_trap string against. Reasons the
        // engine does not carry yet fall back to the bare "trap".
        // An engine fault outranks the §7.10 reason: the program did nothing wrong, the collector
        // did. It is carried as the trap's MESSAGE (as "uncaught exception" below is) rather than
        // as a trap_reason, because that vocabulary is generated from the spec's table and no spec
        // trap describes a broken heap.
        // Precedence: a broken engine outranks a resource limit outranks the §7.10 reason. The
        // middle one exists because the frame guards raise a trap the spec's generated
        // vocabulary cannot name, and without it they fell through to the bare "trap".
        /* …and a HOST callback's own message, below the two engine-side overrides (a broken
         * collector or an exhausted frame pool describes the run, not the embedder's refusal)
         * but above the generated vocabulary, which has no reason for "the host said no". */
        wasm_trap_t* t = trap_make(vm->engine_fault  ? vm->engine_fault
                                 : vm->exhausted     ? vm->exhausted
                                 : vm->host_trap[0]  ? vm->host_trap
                                 : jav_trap_reason_str((jav_trap_reason_t)vm->trap_reason));
        vm->host_trap[0] = '\0';          /* consumed — the next trap names its own cause */
        t->inst = in;
        size_t n = (size_t)bbq_vec_len(vm->trap_trace);
        t->nframes = n; t->frames = n ? malloc(n * sizeof(uint32_t)) : NULL;
        t->frame_pcs = n ? malloc(n * sizeof(uint32_t)) : NULL;
        for (size_t i = 0; i < n; i++) { t->frames[i] = vm->trap_trace[i]; t->frame_pcs[i] = vm->trap_pcs[i]; }
        vm->frame = outer; return t;
    }
    case JAV_INVOKE_EXN: {                                // §7.1.8 an uncaught WASM exception escaped
        wasm_trap_t* t = trap_make("uncaught exception"); t->inst = in; t->is_exn = 1;
        t->exn = exn_from_engine(store, escaped, in);       // §7.1.12 the escaped exception (tag + values)
        vm->frame = outer; return t;
    }
    case JAV_INVOKE_RETURN: break;                        // results sit at frame.stack[0..]; marshalled below
    }

    // Marshal the result sequence (§7.1.8): the N results sit contiguously at the base of
    // the frame stack after the call (jav_runtime.c return path); marshal each by its kind.
    for (uint16_t i = 0; i < ft->nresults && results && i < results->size; i++)
        val_of_slot(&results->data[i], vm->frame.stack[i],
                    valkind_of_wvt(ft->results[i], ft->result_tidx ? (int32_t)ft->result_tidx[i] : 0), in,
                    vm->frame.stack_types[i], store);     // a managed GC result is rooted in the store
    vm->frame = outer;   // §8/A3: restore the (re-entrant) caller's activation now the results are marshalled
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Externs: kind + the cast web (common-initial-sequence reinterprets).

wasm_externkind_t wasm_extern_kind(const wasm_extern_t* e) { return e->kind; }
wasm_externtype_t* wasm_extern_type(const wasm_extern_t* e) {
    switch (e->kind) {
        case WASM_EXTERN_FUNC:   return wasm_functype_as_externtype(functype_of(e->inst, e->index));
        case WASM_EXTERN_GLOBAL: return wasm_globaltype_as_externtype(wasm_global_type((const wasm_global_t*)e));
        case WASM_EXTERN_TABLE:  return wasm_tabletype_as_externtype(wasm_table_type((const wasm_table_t*)e));
        case WASM_EXTERN_MEMORY: return wasm_memorytype_as_externtype(wasm_memory_type((const wasm_memory_t*)e));
        case WASM_EXTERN_TAG:    return wasm_tagtype_as_externtype(wasm_tag_type((const wasm_tag_t*)e));
    }
    return NULL;
}

#define EXTERN_CASTS(name, kindval) \
  wasm_extern_t* wasm_##name##_as_extern(wasm_##name##_t* o) { return (wasm_extern_t*)o; } \
  const wasm_extern_t* wasm_##name##_as_extern_const(const wasm_##name##_t* o) { return (const wasm_extern_t*)o; } \
  wasm_##name##_t* wasm_extern_as_##name(wasm_extern_t* e) { return e->kind == kindval ? (wasm_##name##_t*)e : NULL; } \
  const wasm_##name##_t* wasm_extern_as_##name##_const(const wasm_extern_t* e) { return e->kind == kindval ? (const wasm_##name##_t*)e : NULL; }

EXTERN_CASTS(func,   WASM_EXTERN_FUNC)
EXTERN_CASTS(global, WASM_EXTERN_GLOBAL)
EXTERN_CASTS(table,  WASM_EXTERN_TABLE)
EXTERN_CASTS(memory, WASM_EXTERN_MEMORY)
EXTERN_CASTS(tag,    WASM_EXTERN_TAG)

void wasm_extern_delete(wasm_extern_t* e) { RUN_HOST_FIN(e); free(e); }   // a borrowed view: free only the wrapper
DEFINE_VEC_PTR(extern)

///////////////////////////////////////////////////////////////////////////////
// Tags (§7.1.11). A tag is a store object: a fresh store tagaddr identity + a functype. A host tag
// (wasm_tag_new) is store-owned and links as an import (marshal_import); an instance-export tag is a
// borrowed view. Handles free only their wrapper.

wasm_tag_t* wasm_tag_new(wasm_store_t* s, const wasm_tagtype_t* tt) {
    const wasm_functype_t* ft = wasm_tagtype_functype(tt);
    capi_tag_t* t = calloc(1, sizeof *t);
    t->store = s;
    t->tag_id = s->heap.next_tag_id++;                    // §7.1.11 tag_alloc: a fresh store tagaddr
    t->type = wasm_functype_copy(ft);
    uint16_t np = (uint16_t)t->type->params.size;
    t->jparams = np ? calloc(np, sizeof(jav_valtype_t)) : NULL;
    for (uint16_t i = 0; i < np; i++) t->jparams[i] = wvt_of_valkind(wasm_valtype_kind(t->type->params.data[i]));
    t->jtype = (jav_functype_t){ t->jparams, np, NULL, 0, NULL, NULL };
    bbq_vec_push(s->host_tags, t);                        // store-owned: freed at store_delete
    wasm_tag_t* h = calloc(1, sizeof *h); h->kind = WASM_EXTERN_TAG; h->host = t;
    return h;
}
wasm_tagtype_t* wasm_tag_type(const wasm_tag_t* t) {      // §7.1.11 tag_type
    if (t->host) return wasm_tagtype_new(wasm_functype_copy(((capi_tag_t*)t->host)->type));
    if (!t->inst) return NULL;
    return wasm_tagtype_new(functype_new_from(&t->inst->module->mod.tags[t->index]));
}
void wasm_tag_delete(wasm_tag_t* t) { RUN_HOST_FIN(t); free(t); }   // the host capi_tag is store-owned
wasm_tag_t* wasm_tag_copy(const wasm_tag_t* t) {
    if (!t) return NULL;
    wasm_tag_t* c = malloc(sizeof *c); *c = *t; c->host_info = NULL; c->host_fin = NULL; return c;
}
// The store tagaddr a tag handle denotes (host: its minted id; view: the instance's tagaddr).
static uint32_t tag_identity(const wasm_tag_t* t) {
    if (t->host) return ((capi_tag_t*)t->host)->tag_id;
    return (t->inst && t->inst->inst.tag_ids) ? t->inst->inst.tag_ids[t->index] : 0;
}
bool wasm_tag_same(const wasm_tag_t* a, const wasm_tag_t* b) {       // §4.2 identity is the tagaddr
    if (a == b) return true;
    if (!a || !b) return false;
    return tag_identity(a) == tag_identity(b);
}

///////////////////////////////////////////////////////////////////////////////
// Exceptions (§7.1.12). An exception instance carries a tag (its store identity) and the thrown
// values; an exnref reference value denotes one. exn_alloc creates one host-side; the §7.1.8
// exception outcome of func_invoke surfaces the one that escaped (built from the engine's exn store).

struct wasm_exception_t {
    wasm_store_t*    store;
    uint32_t         tag_id;        // §7.1.12 exn_tag — the tagaddr identity
    uint32_t         nvals;
    slot_t*          vals;          // marshaled value slots
    uint8_t*         types;         // parallel runtime tags (T_INT/…/T_GCREF)
    wasm_valkind_t*  kinds;         // parallel wasm valkinds (exn_read marshaling)
    wasm_instance_t* inst;          // for re-wrapping funcref values (NULL for host-alloc)
    HOST_INFO_FIELDS
};

// A runtime tag T_* → a wasm valkind (numbers exact; a T_REF scalar is most commonly a funcref, a
// T_GCREF a managed any-ref — refined by ref_type if the embedder needs the precise kind).
static wasm_valkind_t valkind_of_runtime_tag(uint8_t t) {
    switch (t) {
        case T_INT: return WASM_I32; case T_LONG: return WASM_I64;
        case T_FLOAT: return WASM_F32; case T_DOUBLE: return WASM_F64;
        case T_V128: return WASM_V128; case T_GCREF: return WASM_ANYREF;
        default: return WASM_FUNCREF;
    }
}
static int exn_holds_refs(const wasm_exception_t* e) {
    for (uint32_t i = 0; i < e->nvals; i++) if (e->types[i] == T_GCREF) return 1;
    return 0;
}
static void exn_visit_refs(wasm_exception_t* e, jav_root_visit_fn visit, void* vctx) {
    for (uint32_t i = 0; i < e->nvals; i++)
        if (e->types[i] == T_GCREF) visit((struct gc_obj**)&e->vals[i].l, vctx);   // keep + relocate the payload
}
// Build a host exception (a copy of the fields) from an engine exn OBJECT — the §7.1.8 escaped
// exception or a caught exnref. The host copy outlives the object, so its ref payload is re-rooted.
static wasm_exception_t* exn_from_engine(wasm_store_t* s, gc_obj_t* exnobj, wasm_instance_t* inst) {
    wasm_exception_t* e = calloc(1, sizeof *e);
    e->store = s; e->tag_id = jav_exn_tag(exnobj); e->nvals = jav_exn_nfields(exnobj); e->inst = inst;
    if (e->nvals) {
        e->vals = calloc(e->nvals, sizeof(slot_t)); e->types = calloc(e->nvals, 1);
        e->kinds = calloc(e->nvals, sizeof(wasm_valkind_t));
        for (uint32_t i = 0; i < e->nvals; i++) {
            e->vals[i] = jav_exn_field(exnobj, i); e->types[i] = jav_exn_ftype(exnobj, i);
            e->kinds[i] = valkind_of_runtime_tag(e->types[i]);
        }
    }
    exn_root(s, e);   // a GC-ref payload now lives in this host copy too → root it independently
    return e;
}
// Install a host exception back into the engine as a fresh managed exn object (the arg direction).
static gc_obj_t* exn_install(vm_t* vm, const wasm_exception_t* e) {
    return jav_exn_alloc(vm, e->tag_id, e->nvals, e->vals, e->types);
}

wasm_exception_t* wasm_exception_new(wasm_store_t* s, const wasm_tag_t* tag, const wasm_val_vec_t* args) {
    wasm_exception_t* e = calloc(1, sizeof *e);
    e->store = s; e->tag_id = tag_identity(tag);
    e->nvals = args ? (uint32_t)args->size : 0;
    if (e->nvals) {
        e->vals = calloc(e->nvals, sizeof(slot_t)); e->types = calloc(e->nvals, 1);
        e->kinds = calloc(e->nvals, sizeof(wasm_valkind_t));
    }
    // B6: root e BEFORE boxing the args. slot_of_val can ALLOCATE (exnref → exn_install, externref → host
    // box) and trigger a collection that would free an already-boxed earlier arg. e->vals/types are
    // calloc'd, so a partial entry (types[i]==0, not T_GCREF) is skipped by exn_visit_refs until filled —
    // so each box is a scanned root the moment it lands. (Unconditional here, vs exn_root's holds-refs
    // optimization: an exn under construction may yet take a ref; exn_unroot on delete removes it either way.)
    if (s) bbq_vec_push(s->exn_roots, e);
    for (uint32_t i = 0; i < e->nvals; i++) {
        e->kinds[i] = args->data[i].kind;
        e->vals[i] = slot_of_val(&args->data[i], &e->types[i], &s->vm);
    }
    return e;
}
wasm_tag_t* wasm_exception_tag(const wasm_exception_t* e) {   // §7.1.12 exn_tag → a tag handle for the id
    capi_tag_t* t = calloc(1, sizeof *t);                     // a store-owned host tag carrying the id + a functype
    t->store = e->store; t->tag_id = e->tag_id;
    wasm_valtype_vec_t p; wasm_valtype_vec_new_uninitialized(&p, e->nvals);
    for (uint32_t i = 0; i < e->nvals; i++) p.data[i] = wasm_valtype_new(e->kinds[i]);
    wasm_valtype_vec_t r; wasm_valtype_vec_new_empty(&r);
    t->type = wasm_functype_new(&p, &r);
    uint16_t np = (uint16_t)e->nvals;
    t->jparams = np ? calloc(np, sizeof(jav_valtype_t)) : NULL;
    for (uint16_t i = 0; i < np; i++) t->jparams[i] = wvt_of_valkind(e->kinds[i]);
    t->jtype = (jav_functype_t){ t->jparams, np, NULL, 0, NULL, NULL };
    bbq_vec_push(e->store->host_tags, t);
    wasm_tag_t* h = calloc(1, sizeof *h); h->kind = WASM_EXTERN_TAG; h->host = t;
    return h;
}
void wasm_exception_read(const wasm_exception_t* e, wasm_val_vec_t* out) {   // §7.1.12 exn_read
    wasm_val_vec_new_uninitialized(out, e->nvals);
    for (uint32_t i = 0; i < e->nvals; i++)
        val_of_slot(&out->data[i], e->vals[i], e->kinds[i], e->inst, e->types[i], e->store);
}
void wasm_exception_delete(wasm_exception_t* e) {
    if (!e) return; RUN_HOST_FIN(e);
    exn_unroot(e->store, e);
    free(e->vals); free(e->types); free(e->kinds); free(e);
}
wasm_exception_t* wasm_exception_copy(const wasm_exception_t* e) {
    if (!e) return NULL;
    wasm_exception_t* c = calloc(1, sizeof *c); *c = *e; c->host_info = NULL; c->host_fin = NULL;
    if (e->nvals) {
        c->vals  = malloc(e->nvals * sizeof(slot_t));        memcpy(c->vals,  e->vals,  e->nvals * sizeof(slot_t));
        c->types = malloc(e->nvals);                         memcpy(c->types, e->types, e->nvals);
        c->kinds = malloc(e->nvals * sizeof(wasm_valkind_t)); memcpy(c->kinds, e->kinds, e->nvals * sizeof(wasm_valkind_t));
    }
    exn_root(c->store, c);   // the copy holds the same payload refs → root it independently
    return c;
}
bool wasm_exception_same(const wasm_exception_t* a, const wasm_exception_t* b) { return a == b; }
void* wasm_exception_get_host_info(const wasm_exception_t* e) { return e->host_info; }
void wasm_exception_set_host_info(wasm_exception_t* e, void* i) { e->host_info = i; e->host_fin = NULL; }
void wasm_exception_set_host_info_with_finalizer(wasm_exception_t* e, void* i, void (*f)(void*)) { e->host_info = i; e->host_fin = f; }
wasm_ref_t* wasm_exception_as_ref(wasm_exception_t* e) {
    if (!e) return NULL;
    wasm_ref_t* r = calloc(1, sizeof *r); r->is_exn = 1; r->host = e; return r;
}
wasm_exception_t* wasm_ref_as_exception(wasm_ref_t* r) { return (r && r->is_exn) ? (wasm_exception_t*)r->host : NULL; }
const wasm_ref_t* wasm_exception_as_ref_const(const wasm_exception_t* e) { return wasm_exception_as_ref((wasm_exception_t*)e); }
const wasm_exception_t* wasm_ref_as_exception_const(const wasm_ref_t* r) { return wasm_ref_as_exception((wasm_ref_t*)r); }

///////////////////////////////////////////////////////////////////////////////
// Reference base ops (the WASM_DECLARE_REF macro surface). The runtime-object handles
// are borrowed instance views, so delete frees only the wrapper (after running any
// host-info finalizer); copy/same operate on wrapper identity; host-info is per-handle.

// The ref ops minus delete (some types supply their own delete, e.g. instance frees the
// owned jav_instance_t; the view handles free only their wrapper). A copy is a fresh
// handle with NO host-info (host-info is per-object, never shared, to avoid double-finalize).
#define REF_OPS_NO_DELETE(name) \
  wasm_##name##_t* wasm_##name##_copy(const wasm_##name##_t* o) { \
    wasm_##name##_t* c = malloc(sizeof *c); *c = *o; c->host_info = NULL; c->host_fin = NULL; return c; } \
  bool wasm_##name##_same(const wasm_##name##_t* a, const wasm_##name##_t* b) { return a == b; } \
  void* wasm_##name##_get_host_info(const wasm_##name##_t* o) { return o->host_info; } \
  void wasm_##name##_set_host_info(wasm_##name##_t* o, void* i) { o->host_info = i; o->host_fin = NULL; } \
  void wasm_##name##_set_host_info_with_finalizer(wasm_##name##_t* o, void* i, void (*f)(void*)) { o->host_info = i; o->host_fin = f; }

#define REF_BASE_OPS(name) \
  void wasm_##name##_delete(wasm_##name##_t* o) { RUN_HOST_FIN(o); free(o); } \
  REF_OPS_NO_DELETE(name)

// func: a host-created handle owns its wasm_func_new closure; free it on delete.
void wasm_func_delete(wasm_func_t* f) { RUN_HOST_FIN(f); if (f && f->host && f->owns_host) hostfn_delete(f->host); free(f); }
REF_OPS_NO_DELETE(func)
REF_BASE_OPS(global)
REF_BASE_OPS(table)
REF_BASE_OPS(memory)
REF_OPS_NO_DELETE(instance)
REF_OPS_NO_DELETE(extern)   // extern has its own (wrapper-only) delete, above
REF_OPS_NO_DELETE(trap)     // trap has its own (message-freeing) delete, above

///////////////////////////////////////////////////////////////////////////////
// Frames — captured at trap time: jav_call records the func-index unwind chain into vm->trap_trace
// and the parallel byte offset into vm->trap_pcs. BOTH tiers stamp frame.instr_pc per opcode (the interp
// via jav_next; the JIT via the opgen-baked _HOLE_pc in each stencil), and jav_call_fn captures it per
// frame — innermost = the trapping instruction, outer = its inward call. wasm_trap_origin/trace build
// wasm_frame_t from those. func_offset is the body-relative pc; module_offset adds the function body's
// position in the module image. So the offsets are exact on either tier.

struct wasm_frame_t { wasm_instance_t* inst; uint32_t func_index; uint32_t pc; };
static wasm_frame_t* capi_frame_make(wasm_instance_t* inst, uint32_t func_index, uint32_t pc) {
    wasm_frame_t* fr = calloc(1, sizeof *fr); fr->inst = inst; fr->func_index = func_index; fr->pc = pc; return fr;
}
void wasm_frame_delete(wasm_frame_t* fr) { free(fr); }
wasm_frame_t* wasm_frame_copy(const wasm_frame_t* fr) {
    wasm_frame_t* c = malloc(sizeof *c); *c = *fr; return c;
}
struct wasm_instance_t* wasm_frame_instance(const wasm_frame_t* fr) { return fr->inst; }
uint32_t wasm_frame_func_index(const wasm_frame_t* fr) { return fr->func_index; }
size_t wasm_frame_func_offset(const wasm_frame_t* fr)   { return fr->pc; }   // body-relative byte offset
size_t wasm_frame_module_offset(const wasm_frame_t* fr) {                    // + the func body's position in the module image
    // A frame can come from an instantiation that TRAPPED (§4.5.4), whose instance was never
    // finished — there is no function table to index, and the body-relative pc is all there is.
    if (!fr->inst || !fr->inst->module || !fr->inst->inst.funcs
        || fr->func_index >= (uint32_t)bbq_vec_len(fr->inst->inst.funcs)) return fr->pc;
    const jav_func_t* f = &fr->inst->inst.funcs[fr->func_index];
    if (!f->code) return fr->pc;
    return (size_t)(f->code - fr->inst->module->bytes) + fr->pc;
}
DEFINE_VEC_PTR(frame)

///////////////////////////////////////////////////////////////////////////////
// §7.1.14 Values (ref_type) + §7.1.15 Matching (match_valtype / match_externtype). The bare
// wasm-c-api omits these; §7.1 mandates them as the embedder interface — the embedder ASKS the
// engine for a reference's runtime type and for the §3.3 subtype relation, never re-deriving them.

// §7.1.14 ref_type(store, ref): the reference type `ref` is valid with, as a wasm.h ref valtype.
// For a managed/i31 ref this is the ABSTRACT runtime heaptype (a less-precise supertype, which the
// §7.1.14 Note explicitly permits at the wasm.h valtype granularity).
wasm_valtype_t* wasm_ref_type(const wasm_store_t* store, const wasm_ref_t* ref) {
    if (!ref) return wasm_valtype_new(WASM_NULLREF);                       // the null reference
    if (ref->is_gc)
        return wasm_valtype_new(valkind_of_ht(jav_ref_abstract_heaptype((vm_t*)&store->vm, (u8)ref->gc_raw, ref->gc_tag)));
    if (ref->is_extern) return wasm_valtype_new(WASM_EXTERNREF);
    return wasm_valtype_new(WASM_FUNCREF);
}

// §7.1.15 match_valtype(store, t1, t2) = ⊢ t1 ≤ t2. Numbers/vectors match iff identical; references
// defer to the ONE §3.3 lattice (jav_ht_sub over their heaptypes) — never a second relation.
bool wasm_match_valtype(const wasm_store_t* store, const wasm_valtype_t* t1, const wasm_valtype_t* t2) {
    (void)store;
    wasm_valkind_t k1 = wasm_valtype_kind(t1), k2 = wasm_valtype_kind(t2);
    if (wasm_valkind_is_ref(k1) && wasm_valkind_is_ref(k2))
        // a wasm.h valtype is an ABSTRACT heaptype (no concrete typeidx), so the §3.3 lattice is never
        // consulted by jav_ht_sub here — pass NULL rather than deref a frame.ctx that may be stale after
        // its instance was deleted (no flat per-vm lattice exists post-§8).
        return jav_ht_sub(NULL, ht_of_valkind(k1), ht_of_valkind(k2));
    return k1 == k2;
}

// §7.1.15 match_externtype(store, et1, et2) = ⊢ et1 ≤ et2 (§3.3.16). Same kind required; every
// component's ref decision goes through match_valtype → the one lattice (no divergent comparator).
bool wasm_match_externtype(const wasm_store_t* store, const wasm_externtype_t* et1, const wasm_externtype_t* et2) {
    wasm_externkind_t ek = wasm_externtype_kind(et1);
    if (ek != wasm_externtype_kind(et2)) return false;
    switch (ek) {
    case WASM_EXTERN_FUNC: {                                               // params contravariant, results covariant
        const wasm_functype_t* a = wasm_externtype_as_functype_const(et1);
        const wasm_functype_t* b = wasm_externtype_as_functype_const(et2);
        const wasm_valtype_vec_t *pa = wasm_functype_params(a), *pb = wasm_functype_params(b);
        const wasm_valtype_vec_t *ra = wasm_functype_results(a), *rb = wasm_functype_results(b);
        if (pa->size != pb->size || ra->size != rb->size) return false;
        for (size_t i = 0; i < pa->size; i++) if (!wasm_match_valtype(store, pb->data[i], pa->data[i])) return false;
        for (size_t i = 0; i < ra->size; i++) if (!wasm_match_valtype(store, ra->data[i], rb->data[i])) return false;
        return true;
    }
    case WASM_EXTERN_GLOBAL: {
        const wasm_globaltype_t* a = wasm_externtype_as_globaltype_const(et1);
        const wasm_globaltype_t* b = wasm_externtype_as_globaltype_const(et2);
        wasm_mutability_t mb = wasm_globaltype_mutability(b);
        if (wasm_globaltype_mutability(a) != mb) return false;
        const wasm_valtype_t* ca = wasm_globaltype_content(a), *cb = wasm_globaltype_content(b);
        return mb == WASM_VAR                                             // var: invariant; const: covariant
            ? (wasm_match_valtype(store, ca, cb) && wasm_match_valtype(store, cb, ca))
            : wasm_match_valtype(store, ca, cb);
    }
    case WASM_EXTERN_TABLE: {
        const wasm_tabletype_t* a = wasm_externtype_as_tabletype_const(et1);
        const wasm_tabletype_t* b = wasm_externtype_as_tabletype_const(et2);
        const wasm_limits_t* la = wasm_tabletype_limits(a); const wasm_limits_t* lb = wasm_tabletype_limits(b);
        if (!(la->min >= lb->min && la->max <= lb->max)) return false;    // §3.3.13 limits
        const wasm_valtype_t* ea = wasm_tabletype_element(a), *eb = wasm_tabletype_element(b);
        return wasm_match_valtype(store, ea, eb) && wasm_match_valtype(store, eb, ea);   // reftype invariant
    }
    case WASM_EXTERN_MEMORY: {
        const wasm_memorytype_t* a = wasm_externtype_as_memorytype_const(et1);
        const wasm_memorytype_t* b = wasm_externtype_as_memorytype_const(et2);
        const wasm_limits_t* la = wasm_memorytype_limits(a); const wasm_limits_t* lb = wasm_memorytype_limits(b);
        return la->min >= lb->min && la->max <= lb->max;
    }
    case WASM_EXTERN_TAG: {                                               // §3.3.14: tag types match iff functypes equal
        const wasm_functype_t* fa = wasm_tagtype_functype(wasm_externtype_as_tagtype_const(et1));
        const wasm_functype_t* fb = wasm_tagtype_functype(wasm_externtype_as_tagtype_const(et2));
        wasm_externtype_t* ea = wasm_functype_as_externtype((wasm_functype_t*)fa);
        wasm_externtype_t* eb = wasm_functype_as_externtype((wasm_functype_t*)fb);
        return wasm_match_externtype(store, ea, eb) && wasm_match_externtype(store, eb, ea);
    }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Reference values + casts, host-created globals/tables/memories, foreign objects, and shared
// modules — the wasm_ref_t value type and the wasm_X_as_ref / ref_as_X cast web.

// wasm_ref_t (the value type, defined at the top of the file). A funcref is the funcinst
// pointer `fn`; copy duplicates it; same compares funcref identity (the pointer). externref
// host values extend this.
void wasm_ref_delete(wasm_ref_t* r) {
    if (r && r->is_gc && r->gc_tag == T_GCREF) gcref_unroot(r);   // stop rooting before freeing the handle
    RUN_HOST_FIN(r); free(r);
}
wasm_ref_t* wasm_ref_copy(const wasm_ref_t* r) {
    if (!r) return NULL;
    wasm_ref_t* c = malloc(sizeof *c); *c = *r; c->host_info = NULL; c->host_fin = NULL;
    if (c->is_gc && c->gc_tag == T_GCREF && c->gc_store) gcref_root(c->gc_store, c);   // the copy roots its own object
    return c;
}
bool wasm_ref_same(const wasm_ref_t* a, const wasm_ref_t* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->is_gc || b->is_gc) return a->is_gc && b->is_gc && a->gc_raw == b->gc_raw;   // managed-ref identity = the object
    if (a->is_extern != b->is_extern) return false;
    if (a->is_extern) return a->host == b->host;                 // externref identity = host value
    return a->fn == b->fn;                                        // §4.2.1 funcref identity = the funcinst pointer
}
void* wasm_ref_get_host_info(const wasm_ref_t* r) { return r->host_info; }
void wasm_ref_set_host_info(wasm_ref_t* r, void* i) { r->host_info = i; r->host_fin = NULL; }
void wasm_ref_set_host_info_with_finalizer(wasm_ref_t* r, void* i, void (*f)(void*)) { r->host_info = i; r->host_fin = f; }

// funcref <-> ref: a funcref VALUE is a §4.2.1 funcinst pointer — the same encoding the engine
// stores in a table/global slot, so funcrefs cross the host↔guest boundary without re-encoding.
wasm_ref_t* wasm_func_as_ref(wasm_func_t* f) {
    if (!f) return NULL;
    if (f->host)        // a wasm_func_new closure: the canonical host funcinst backs the funcref
        return funcref_make(((capi_hostfn_t*)f->host)->store, &((capi_hostfn_t*)f->host)->funcinst);
    if (f->inst)        // an instance-export view (extern-sized: must not read the trailing fn field)
        return funcref_make(f->inst->store, &f->inst->inst.funcs[f->index]);
    return funcref_make(f->fnstore, f->fn);   // a func recovered from a funcref (table/global): re-wrap the same funcinst
}
wasm_func_t* wasm_ref_as_func(wasm_ref_t* r) {
    if (!r || r->is_extern || r->kind != WASM_EXTERN_FUNC || !r->fn) return NULL;
    wasm_func_t* f = calloc(1, sizeof *f);
    f->kind = WASM_EXTERN_FUNC;
    if (r->host) f->host = r->host;          // host funcref: a borrowed view of the closure (owns_host stays 0)
    else { f->fn = r->fn; f->fnstore = r->fnstore; }   // instance funcref: call by funcinst reference (host==NULL && inst==NULL)
    return f;
}
const wasm_ref_t* wasm_func_as_ref_const(const wasm_func_t* f) { return wasm_func_as_ref((wasm_func_t*)f); }
const wasm_func_t* wasm_ref_as_func_const(const wasm_ref_t* r) { return wasm_ref_as_func((wasm_ref_t*)r); }

// global/table/memory handles are not reference VALUES (only func/extern/GC/exn refs are), so their
// as_ref / ref_as casts are NULL by construction — the spec has no such ref.
#define REF_AS_STUBS(name) \
  wasm_ref_t* wasm_##name##_as_ref(wasm_##name##_t* o) { (void)o; return NULL; } \
  wasm_##name##_t* wasm_ref_as_##name(wasm_ref_t* r) { (void)r; return NULL; } \
  const wasm_ref_t* wasm_##name##_as_ref_const(const wasm_##name##_t* o) { (void)o; return NULL; } \
  const wasm_##name##_t* wasm_ref_as_##name##_const(const wasm_ref_t* r) { (void)r; return NULL; }

REF_AS_STUBS(global)
REF_AS_STUBS(table)
REF_AS_STUBS(memory)
REF_AS_STUBS(extern)
REF_AS_STUBS(instance)
REF_AS_STUBS(trap)
REF_AS_STUBS(module)

// Instance-export object accessors — read/write the storage of an export VIEW, plus the
// standalone host-created `_new` objects (store-owned, GC-rooted, linkable as imports).

// — Globals (§7.1.13): an instance-export VIEW reads/writes `inst.globals[index]`; a
//   host-created (standalone) global owns store-tracked storage. Writing an immutable
//   global is the error case (no-op, matching `global_write → error`). —
wasm_global_t* wasm_global_new(wasm_store_t* s, const wasm_globaltype_t* gt, const wasm_val_t* v) {
    capi_global_t* g = calloc(1, sizeof *g);
    g->store = s;
    wasm_valkind_t gk = wasm_valtype_kind(wasm_globaltype_content(gt));
    g->wvt = wvt_of_valkind(gk); g->wvt_ht = ht_of_valkind(gk);
    g->mut = (wasm_globaltype_mutability(gt) == WASM_VAR);
    g->val = slot_of_val(v, &g->tag, &s->vm);
    bbq_vec_push(s->host_globals, g);                 // store-owned: GC-rooted + freed at store_delete
    wasm_global_t* h = calloc(1, sizeof *h); h->kind = WASM_EXTERN_GLOBAL; h->host = g;
    return h;
}
wasm_globaltype_t* wasm_global_type(const wasm_global_t* g) {
    if (g->host) { capi_global_t* cg = g->host;
        return wasm_globaltype_new(wasm_valtype_new(valkind_of_wvt(cg->wvt, cg->wvt_ht)), cg->mut ? WASM_VAR : WASM_CONST); }
    if (!g->inst) return NULL;
    const jav_modidx_t* m = &g->inst->module->mod;
    return wasm_globaltype_new(wasm_valtype_new(valkind_of_wvt(m->global_types[g->index], m->global_tidx ? (int32_t)m->global_tidx[g->index] : 0)),
                               m->global_mut[g->index] ? WASM_VAR : WASM_CONST);
}
void wasm_global_get(const wasm_global_t* g, wasm_val_t* out) {
    if (g->host) { capi_global_t* cg = g->host;
        val_of_slot(out, cg->val, valkind_of_wvt(cg->wvt, cg->wvt_ht), NULL, cg->tag, cg->store); return; }
    if (!g->inst) { out->kind = WASM_I32; out->of.i64 = 0; return; }
    const jav_modidx_t* m = &g->inst->module->mod;
    val_of_slot(out, *g->inst->inst.globals[g->index], valkind_of_wvt(m->global_types[g->index], m->global_tidx ? (int32_t)m->global_tidx[g->index] : 0),
                (wasm_instance_t*)g->inst, g->inst->inst.global_types[g->index], g->inst->store);
}
void wasm_global_set(wasm_global_t* g, const wasm_val_t* v) {
    if (g->host) { capi_global_t* cg = g->host;
        if (cg->mut) cg->val = slot_of_val(v, &cg->tag, &cg->store->vm);
        return; }
    if (!g->inst || !g->inst->module->mod.global_mut[g->index]) return;  // immutable → §7.1.13 error
    uint8_t tag;
    *g->inst->inst.globals[g->index] = slot_of_val(v, &tag, &g->inst->store->vm);
    g->inst->inst.global_types[g->index] = tag;   // keep the runtime tag in sync (GC root scan)
}

// Slot-sized tables: a T_GCREF entry holds a gc box pointer (externref), a T_REF entry a
// §4.2.1 funcinst pointer (JAV_NULLREF = null). Encode a wasm_ref_t into an (entry, tag) pair.
static void ref_to_entry(wasm_store_t* s, wasm_ref_t* r, s8* out_v, u1* out_ty) {
    if (r && r->is_extern) { *out_v = (s8)(uintptr_t)jav_host_box_new(&s->vm, r->host); *out_ty = T_GCREF; }
    else                   { *out_v = (r && r->fn) ? (s8)(uintptr_t)r->fn : (s8)(u4)JAV_NULLREF; *out_ty = T_REF; }
}
// The underlying table inst + owning store, whether an instance-export view or a host table.
static jav_tableinst_t* table_of(const wasm_table_t* t) {
    return t->inst ? &t->inst->inst.tables[t->index] : (t->host ? &((capi_table_t*)t->host)->tab : NULL);
}
static wasm_store_t* table_store_of(const wasm_table_t* t) {
    return t->inst ? t->inst->store : (t->host ? ((capi_table_t*)t->host)->store : NULL);
}

// — Tables (§7.1.9): instance-export view OR a host-created (standalone) table. —
wasm_table_t* wasm_table_new(wasm_store_t* s, const wasm_tabletype_t* tt, wasm_ref_t* init) {
    capi_table_t* ct = calloc(1, sizeof *ct); ct->store = s;
    const wasm_limits_t* lim = wasm_tabletype_limits(tt);
    wasm_valkind_t ek = wasm_valtype_kind(wasm_tabletype_element(tt));
    ct->tab.reftype = (uint8_t)wvt_of_valkind(ek); ct->tab.reftype_ht = ht_of_valkind(ek);
    ct->tab.has_max = (lim->max != wasm_limits_max_default); ct->tab.max = ct->tab.has_max ? lim->max : 0;
    bbq_vec_push(s->host_tables, ct);                 // register BEFORE filling (boxes become roots)
    s8 v; u1 ty; ref_to_entry(s, init, &v, &ty);      // one init box, reused across the min slots
    for (uint32_t i = 0; i < lim->min; i++) { bbq_vec_push(ct->tab.refs, v); bbq_vec_push(ct->tab.types, ty); }
    wasm_table_t* h = calloc(1, sizeof *h); h->kind = WASM_EXTERN_TABLE; h->host = ct;
    return h;
}
wasm_tabletype_t* wasm_table_type(const wasm_table_t* t) {
    jav_tableinst_t* ti = table_of(t);
    if (t->host) {
        wasm_limits_t lim = { (uint32_t)bbq_vec_len(ti->refs),
                              ti->has_max ? ti->max : wasm_limits_max_default };
        return wasm_tabletype_new(wasm_valtype_new(valkind_of_wvt(ti->reftype, ti->reftype_ht)), &lim);
    }
    if (!t->inst) return NULL;
    const jav_modidx_t* m = &t->inst->module->mod;
    wasm_limits_t lim = { (uint32_t)m->table_min[t->index],
                          m->table_has_max[t->index] ? (uint32_t)m->table_max[t->index] : wasm_limits_max_default };
    return wasm_tabletype_new(wasm_valtype_new(valkind_of_wvt(m->table_reftype[t->index], m->table_tidx ? (int32_t)m->table_tidx[t->index] : 0)), &lim);
}
wasm_ref_t* wasm_table_get(const wasm_table_t* t, wasm_table_size_t i) {
    jav_tableinst_t* ti = table_of(t);
    s8 raw; u1 tag;
    if (!ti || !jav_tableinst_read(ti, (u8)i, &raw, &tag)) return NULL;   // §7.1.9 table_read; OOB → null
    if (tag == T_GCREF) return externref_make(jav_host_box_get((gc_obj_t*)(uintptr_t)raw));
    // §4.2.1 a T_REF entry is a funcinst pointer (JAV_NULLREF = null)
    return funcref_make(table_store_of(t), JAV_REF_ISNULL(raw) ? NULL : (const jav_func_t*)(uintptr_t)raw);
}
bool wasm_table_set(wasm_table_t* t, wasm_table_size_t i, wasm_ref_t* r) {
    jav_tableinst_t* ti = table_of(t);
    if (!ti) return false;
    s8 v; u1 ty; ref_to_entry(table_store_of(t), r, &v, &ty);
    return jav_tableinst_write(ti, (u8)i, v, ty);                         // §7.1.9 table_write; OOB → error
}
wasm_table_size_t wasm_table_size(const wasm_table_t* t) {
    jav_tableinst_t* ti = table_of(t);
    return ti ? (wasm_table_size_t)bbq_vec_len(ti->refs) : 0;
}
bool wasm_table_grow(wasm_table_t* t, wasm_table_size_t delta, wasm_ref_t* init) {
    jav_tableinst_t* ti = table_of(t);
    if (!ti) return false;
    s8 v; u1 ty; ref_to_entry(table_store_of(t), init, &v, &ty);          // one box reused for the fill
    return jav_tableinst_grow(ti, (u8)delta, v, ty) >= 0;                 // §7.1.9 table_grow; over-cap → false
}

// — Memories (§7.1.10): instance-export view OR a host-created (standalone) memory; both
//   are store-heap memories (the heap owns the bytes, freed with the store). —
static wasm_store_t* mem_store_of(const wasm_memory_t* mo) {
    return mo->inst ? mo->inst->store : (mo->host ? ((capi_memory_t*)mo->host)->store : NULL);
}
// §4.5.2 external address: resolve to the store-heap memaddr — instance export maps its module
// memidx through the bound memaddr table; a host-created memory carries its memaddr directly.
static uint32_t mem_addr_of(const wasm_memory_t* mo) {
    return mo->inst ? mo->inst->inst.mem_addrs[mo->index] : ((capi_memory_t*)mo->host)->memaddr;
}
static jav_mem_t* mem_of(const wasm_memory_t* mo) {
    wasm_store_t* s = mem_store_of(mo);
    return s ? &s->heap.mems[mem_addr_of(mo)] : NULL;
}
wasm_memory_t* wasm_memory_new(wasm_store_t* s, const wasm_memorytype_t* mt) {
    const wasm_limits_t* lim = wasm_memorytype_limits(mt);
    int has_max = (lim->max != wasm_limits_max_default);
    uint32_t maxp = has_max ? lim->max : 0;
    capi_memory_t* cm = calloc(1, sizeof *cm);
    cm->store = s; cm->memaddr = (uint32_t)jav_mem_add(&s->heap, lim->min, maxp, has_max, 0);
    bbq_vec_push(s->host_mems, cm);                   // store-owned: freed at store_delete
    wasm_memory_t* h = calloc(1, sizeof *h); h->kind = WASM_EXTERN_MEMORY; h->host = cm;
    return h;
}
wasm_memorytype_t* wasm_memory_type(const wasm_memory_t* mo) {
    if (mo->host) { jav_mem_t* m = mem_of(mo);
        wasm_limits_t lim = { (uint32_t)(m->size / MEMORY_PAGE_SIZE),
                              m->has_max ? (uint32_t)(m->max / MEMORY_PAGE_SIZE) : wasm_limits_max_default };
        return wasm_memorytype_new(&lim); }
    if (!mo->inst) return NULL;
    const jav_modidx_t* m = &mo->inst->module->mod;
    wasm_limits_t lim = { (uint32_t)m->mem_min[mo->index],
                          m->mem_has_max[mo->index] ? (uint32_t)m->mem_max[mo->index] : wasm_limits_max_default };
    return wasm_memorytype_new(&lim);
}
byte_t* wasm_memory_data(wasm_memory_t* mo) { jav_mem_t* m = mem_of(mo); return m ? (byte_t*)m->data : NULL; }
size_t wasm_memory_data_size(const wasm_memory_t* mo) { jav_mem_t* m = mem_of(mo); return m ? (size_t)m->size : 0; }
wasm_memory_pages_t wasm_memory_size(const wasm_memory_t* mo) { jav_mem_t* m = mem_of(mo); return m ? (wasm_memory_pages_t)(m->size / MEMORY_PAGE_SIZE) : 0; }
bool wasm_memory_grow(wasm_memory_t* mo, wasm_memory_pages_t delta) {
    jav_mem_t* m = mem_of(mo);                       // the host holds a memaddr → grow the meminst directly (§7.1)
    return m && mem_grow_inst(m, (s8)delta) != -1;
}

// Host-defined functions (§7.1.8 func_alloc): a handle carrying the callback closure; it
// links into a module as an import via marshal_import (capi_host_invoke on the dispatch seam).
static wasm_func_t* host_func_handle(capi_hostfn_t* hf) {
    wasm_func_t* f = calloc(1, sizeof *f);
    f->kind = WASM_EXTERN_FUNC; f->inst = NULL; f->index = 0; f->host = hf; f->owns_host = 1;
    return f;
}
wasm_func_t* wasm_func_new(wasm_store_t* s, const wasm_functype_t* t, wasm_func_callback_t cb) {
    capi_hostfn_t* hf = hostfn_new(s, t); hf->cb = cb;
    return host_func_handle(hf);
}
wasm_func_t* wasm_func_new_with_env(wasm_store_t* s, const wasm_functype_t* t,
                                    wasm_func_callback_with_env_t cb, void* env, void (*fin)(void*)) {
    capi_hostfn_t* hf = hostfn_new(s, t); hf->cb_env = cb; hf->env = env; hf->finalizer = fin;
    return host_func_handle(hf);
}

// Foreign objects (the c-api host-reference mechanism): an opaque host object whose identity
// IS its pointer. wasm_foreign_as_ref hands it to wasm code as an externref value (the host
// box wraps this pointer), so a round-trip preserves identity.
struct wasm_foreign_t { wasm_store_t* store; HOST_INFO_FIELDS };
wasm_foreign_t* wasm_foreign_new(wasm_store_t* s) {
    wasm_foreign_t* f = calloc(1, sizeof *f); f->store = s; return f;
}
REF_BASE_OPS(foreign)
wasm_ref_t* wasm_foreign_as_ref(wasm_foreign_t* f) { return f ? externref_make(f) : NULL; }
wasm_foreign_t* wasm_ref_as_foreign(wasm_ref_t* r) { return (r && r->is_extern) ? (wasm_foreign_t*)r->host : NULL; }
const wasm_ref_t* wasm_foreign_as_ref_const(const wasm_foreign_t* f) { return wasm_foreign_as_ref((wasm_foreign_t*)f); }
const wasm_foreign_t* wasm_ref_as_foreign_const(const wasm_ref_t* r) { return wasm_ref_as_foreign((wasm_ref_t*)r); }

// Module serialization + sharing. Our serialized form IS the module's own §5 binary (the
// embedder gets back a byte-identical, re-decodable image); a shared module holds a byte
// copy that any store can re-decode via wasm_module_new. Both are lossless round-trips.
void wasm_module_serialize(const wasm_module_t* m, wasm_byte_vec_t* out) {
    wasm_byte_vec_new(out, m->nbytes, (const wasm_byte_t*)m->bytes);
}
wasm_module_t* wasm_module_deserialize(wasm_store_t* s, const wasm_byte_vec_t* b) {
    return wasm_module_new(s, b);
}
REF_OPS_NO_DELETE(module)   // module has its own (arena-freeing) delete, above
struct wasm_shared_module_t { uint8_t* bytes; size_t n; };
void wasm_shared_module_delete(wasm_shared_module_t* m) { if (m) { free(m->bytes); free(m); } }
wasm_shared_module_t* wasm_module_share(const wasm_module_t* m) {
    wasm_shared_module_t* sm = calloc(1, sizeof *sm);
    sm->n = m->nbytes; sm->bytes = malloc(m->nbytes ? m->nbytes : 1);
    memcpy(sm->bytes, m->bytes, m->nbytes);
    return sm;
}
wasm_module_t* wasm_module_obtain(wasm_store_t* s, const wasm_shared_module_t* sm) {
    wasm_byte_vec_t b = { sm->n, (wasm_byte_t*)sm->bytes };
    return wasm_module_new(s, &b);
}
