// jav_hostref.h — host externref boxing (§4.5.1 ref.host / §4.2.1 ref.extern).
//
// An externref value the embedder passes in is an opaque host pointer. The engine's
// reference values that survive collection are GC-managed (tagged T_GCREF, an 8-byte
// pointer in slot.l), so a host externref becomes a GC-managed "host box": one object
// holding the host pointer, with NO managed fields (nrefs=0) — the collector keeps it
// alive but never traces into it. This is core runtime, NOT the wasm-c-api shim; the
// shim (wasm_capi.c) calls these so it never reaches into GC internals itself.
#ifndef JAV_HOSTREF_H
#define JAV_HOSTREF_H

#include "jav_frame.h"   // vm_t
#include "jav_gc.h"      // gc_obj_t (resolved via -Isrc/immix)

// Allocate a host box wrapping `host` (a GC object, so it must be rooted by the caller —
// store it into a T_GCREF slot/local/global before the next allocation).
gc_obj_t* jav_host_box_new(vm_t* vm, void* host);
// 1 iff `o` is a host box (vs some other managed object surfaced as externref).
int       jav_is_host_box(const gc_obj_t* o);
// The wrapped host pointer if `o` is a host box; otherwise `o` itself (opaque identity).
void*     jav_host_box_get(const gc_obj_t* o);

// §7.1.14 ref_type (internal/GC hierarchy): the ABSTRACT runtime heaptype (an HT_* code) of a
// non-null managed/i31 reference `raw`+`tag`. The c-api shim maps it to a wasm.h ref valkind.
int32_t   jav_ref_abstract_heaptype(vm_t* vm, u8 raw, u1 tag);

// §7.1.12 exception-object accessors (an exnref IS a managed gc_obj): read the object's tag identity,
// field count, a field value, and a field's runtime type tag — the c-api marshals exn values through
// these without knowing the GC layout. jav_exn_alloc builds one from host fields (the install path).
u4        jav_exn_tag(gc_obj_t* o);
u4        jav_exn_nfields(gc_obj_t* o);
slot_t    jav_exn_field(gc_obj_t* o, u4 k);
u1        jav_exn_ftype(gc_obj_t* o, u4 k);
gc_obj_t* jav_exn_alloc(vm_t* vm, u4 tagaddr, u4 nfields, const slot_t* fields, const u1* ftypes);

#endif // JAV_HOSTREF_H
