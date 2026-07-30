/* wasm_types.h — the WASM-GC type universe for a compiled program.
 *
 * Java's object model lowers to GC types: each class → a struct type (its
 * instance fields, with the superclass's struct as the GC supertype so
 * inheritance is structural), each array → an array type. All struct types
 * live in ONE recursive group (0x4E) so they can reference one another
 * (a field of class type, a vtable of funcref, …); array types are members
 * of the same group. Type indices: structs occupy [0, num_classes) by
 * class_id; array types follow at [num_classes, …).
 *
 * This is the FOUNDATION the rest of the object model (struct.new/get/set,
 * array.*, ref.test/cast, call_ref) builds on — and the source of the ref
 * valtypes that replace the temporary eqref placeholder. Encoding constants
 * are taken verbatim from the VM's own reader (wasm/spec/wasm.bbq): what the
 * VM reads is what we must write. */
#ifndef WASM_TYPES_H
#define WASM_TYPES_H

#include "javelina/compiler/emit_wasm.h"
#include "javelina/compiler/sema.h"
#include "gen/sir_ast.h"

/* The recorded-fact side: the import list is read from the ddcg's call-target rows. */
#include "javelina/compiler/compiler.h"

typedef struct wasm_types_t {
    const sema_ctx_t* sema;
    int        num_classes;     /* struct typeidx range [0, num_classes) */
    int        num_shared;      /* PLUGIN: # SHARED classes (library source + synthesized arrays =
                                 * jre's exact type set) — ordered first as ONE rec group byte-
                                 * identical to jre's; user-source classes trail in a 2nd group.
                                 * = num_classes in WHOLE/RUNTIME (every class is "shared"). */
    int        struct_split;    /* PLUGIN: byte offset in struct_bytes where the shared structs end
                                 * and the user structs begin (the two rec groups' boundary). */
    int*       class_pos;       /* class_id → struct typeidx, topologically ordered
                                 * (a supertype always gets a smaller index, as
                                 * §3.2.11 requires) — Java class order is arbitrary */
    uint8_t*   struct_bytes;    /* bbq_vec<u8>: concatenated class subtype encodings,
                                 * in topological (class_pos) order */
    java_type_t* arr_elems;     /* bbq_vec<java_type_t>: array element types, in TWO
                                 * regions split at num_sig_arrays. [0, num_sig_arrays)
                                 * are field/signature arrays — referenced by structs
                                 * and func types, so they sit BEFORE the func types
                                 * (typeidx = num_classes + 1 + i). [num_sig_arrays, …)
                                 * are body-local-only arrays (a `new int[]`, an array
                                 * local) — referenced by no func type, so they sit
                                 * AFTER the func types (typeidx = num_classes + 1 +
                                 * num_sig_arrays + num_functypes + (i - num_sig_arrays)).
                                 * This keeps a func type's index independent of the
                                 * body-array count, which only finalizes during codegen
                                 * (a call_ref bakes its func-type index inline). */
    int        num_sig_arrays;  /* frozen after wasm_types_build's Pass 1; -1 while
                                 * Pass 1 is still registering the signature region */
    bool       has_clinit;      /* a module initializer is emitted → the type
                                 * section appends one ()->() functype last */
    bool       has_exceptions;  /* the program uses throw/try → emit the exception
                                 * tag section (id 13) + its [ref null Throwable]->[]
                                 * functype (appended past <clinit> in the rec group) */
    bool       has_iface_helper; /* the program has interfaces → emit the synthesized
                                 * iface_instanceof helper (a (ref null root, i32)->i32
                                 * function + functype, appended past the tag functype) */
    /* The module's import list, DERIVED by build_import_list from sema's §13.1 reference set
     * and §8.4.6.1 dispatch table — not copied from sema, which publishes neither an import
     * list nor a funcidx order (both are target concepts). Position IS the funcidx.
     * bbq_vec of sema_func_ent_t. */
    sema_func_ent_t* imports;
    int        nimports;        /* referenced native/host methods become FUNCTION
                                 * IMPORTS occupying funcidx [0, nimports); defined
                                 * functions follow at nimports+. Cached from sema
                                 * (sema_import_count), which records them at call
                                 * resolution — see wasm_import_index. */
    int        mode;            /* sema_mode_t mirror (WHOLE/RUNTIME/PLUGIN); copied
                                 * from sema->mode in wasm_types_build. */
    /* (class, method) → GLOBAL VTABLE SLOT, built ONCE by build_method_slots.
     * method_slot is flat, indexed by class_method_base[class_id] + method_idx;
     * num_vtable_slots is the vtable length (the number of distinct virtual
     * signatures). Codegen asks once per virtual call site, so this must stay a
     * lookup — recomputing it per call is quadratic in the class table.
     *
     * -1 means NO SLOT (the method is not virtual, or the pair is out of range),
     * like every other index accessor here. It must not be 0: slot 0 is a real
     * slot, and a miss reported as 0 dispatches through the wrong signature's
     * funcref, which the ref.cast in VTABLE_DISPATCH_OP turns into a runtime
     * cast failure far from the mistake. */
    int32_t*   method_slot;
    int*       class_method_base;
    int        num_vtable_slots;
} wasm_types_t;

/* The import funcidx of a (class, method) that is an import (a referenced native
 * method, recorded by sema at call resolution), or -1 if it is not an import. */
int32_t wasm_import_index(const wasm_types_t* wt, int class_id, int method_idx);

/* The derived import list — the module's own, in funcidx order. Every emitter that walks the
 * imports reads THESE, not sema's list: sema publishes references, this is the target's
 * answer to what must be imported. */
int             wasm_import_count(const wasm_types_t* wt);
sema_func_ent_t wasm_import_at(const wasm_types_t* wt, int i);

/* Does jre own this class's globals and vtable? True in PLUGIN mode for a shared library
 * class; false for a class this module emits. The one predicate for that split. */
bool wasm_is_imported_class(const wasm_types_t* wt, int ci);

/* Add every call target the ddcg RECORDED for methods [0, method_count) to the import list, then
 * re-freeze nimports. Reads facts; does not inspect the SIR. */
void wasm_types_add_call_targets(wasm_types_t* wt, const sema_ctx_t* s,
                                 const sema_func_ent_t* call_targets, int n_call_targets);


/* Build the struct type for every class in `sema` (typeidx == class_id) and
 * register the array types reachable from their instance fields. */
/* `cctx` supplies the ddcg's recorded facts for methods [0, method_count), whose call targets
 * join the import list. NULL when the caller emits no code and only wants the type/index
 * tables. */
void wasm_types_build(wasm_types_t* wt, const sema_ctx_t* sema,
                      const sema_func_ent_t* call_targets, int n_call_targets);
void wasm_types_free(wasm_types_t* wt);

/* The struct typeidx of a class. */
int32_t wasm_types_class_typeidx(const wasm_types_t* wt, int class_id);

/* The array typeidx for element type `elem`, registering it (and any nested
 * array element) on first use. */
int32_t wasm_types_array_typeidx(wasm_types_t* wt, java_type_t elem);

/* The struct field base for a class: the number of instance fields its
 * superclasses contribute (its own fields begin at this index in its struct,
 * because a GC subtype's field list extends the supertype's as a prefix). A
 * field declared in class D with class-local index i sits at WASM struct index
 * wasm_types_field_base(D) + i — the absolute index struct.get/set need. */
int32_t wasm_types_field_base(const wasm_types_t* wt, int class_id);

/* The absolute WASM struct field index of a field declared in `decl_class` with
 * class-local index `local_idx`: field_base(decl_class) + the count of INSTANCE
 * fields preceding it in decl_class (static fields occupy local indices but are
 * globals, not struct members, so they are skipped). The single authority for
 * field positions — must agree with the struct field order emit_section uses. */
int32_t wasm_types_field_index(const wasm_types_t* wt, int decl_class, int local_idx);

/* The WASM global index a static field maps to: static fields become module
 * globals, numbered in (class_id, field-vec) order across the program. Keyed by
 * the field's DECLARING class and class-local index, the same way struct fields
 * are. The authority for global numbering — must agree with emit_globals. */
int32_t wasm_global_index(const wasm_types_t* wt, int decl_class, int local_idx);

/* The number of static-field module globals (one per static field). */
int32_t wasm_global_count(const wasm_types_t* wt);

/* PLUGIN: the count of IMPORTED globals (G_imp = library static fields + one Class singleton
 * per library class), occupying global indices [0, G_imp) below the defined user globals.
 * 0 in WHOLE/RUNTIME (every global is defined). */
int32_t wasm_imported_global_count(const wasm_types_t* wt);

/* The total module-global count = static-field globals followed by one vtable
 * instance global per class. The static globals keep indices [0, wasm_global_count);
 * the vtable globals occupy [wasm_global_count, +num_classes). */
int32_t wasm_total_global_count(const wasm_types_t* wt);

/* The number of distinct virtual signatures program-wide = the length of the
 * global vtable (array of funcref). Each occupied slot is a wasm_vtable_slot. */
int32_t wasm_vtable_len(const wasm_types_t* wt);

/* The module-global index of a class's vtable instance (past the static-field
 * globals). New writes this into the object's field 0; dispatch reads it back. */
int32_t wasm_vtable_global_index(const wasm_types_t* wt, int class_id);
/* The module-global index of a class's ClassDesc instance (past the vtable globals),
 * emitted in class_pos order so a class's `super` descriptor precedes it. */
int32_t wasm_class_singleton_global_index(const wasm_types_t* wt, int class_id);

/* The func typeidx of the module initializer (the trailing ()->() type). Valid
 * only when wt->has_clinit was set before type-section emission. */
int32_t wasm_clinit_functype_idx(const wasm_types_t* wt);

/* The func typeidx of the exception tag ([ref null Throwable] -> []) — the type
 * the tag section's lone tag references. Valid only when has_exceptions. */
int32_t wasm_tag_functype_idx(const wasm_types_t* wt);

/* The synthesized iface_instanceof helper, valid only when has_iface_helper. Its
 * functype ((ref null root, i32)->i32) is appended past the tag functype; its funcidx
 * is past all table functions + <clinit>. wasm_types_emit_iface_helper emits the
 * complete func body (locals vec + a single scan of obj.ClassDesc.interfaces). */
int32_t wasm_iface_helper_functype_idx(const wasm_types_t* wt);
int32_t wasm_iface_helper_funcidx(const wasm_types_t* wt);
void    wasm_types_emit_iface_helper(wasm_types_t* wt, emit_wasm_ctx* out);
/* The reflection bootstrap fixup instructions (set each Class singleton's field0 =
 * Class.class), prepended to <clinit> which runs at module start. */
void    wasm_types_emit_reflect_fixup(wasm_types_t* wt, const sema_ctx_t* s, emit_wasm_ctx* out);

/* Emit the global section CONTENT (count + per-field globaltype + default-init
 * const-expr) in the same dense order as wasm_global_index. The assembler decodes
 * these bytes through the shared jav reader, like the type section. */
void wasm_types_emit_globals_content(wasm_types_t* wt, const sema_ctx_t* s,
                                     emit_wasm_ctx* out);

/* The WASM function index of a method: methods are numbered in (class_id,
 * methods-vec) order across the program, keyed by the method's declaring class
 * and class-local index. The authority for function numbering — the module
 * assembler emits function bodies in this same order. */
int32_t wasm_func_index(const wasm_types_t* wt, int decl_class, int method_idx);

/* Virtual dispatch typeidx/slot layout. Each class has a vtable struct type
 * (typeidx num_classes + class_id) whose slots hold the method funcrefs; the
 * funcref at a method's slot is typed by that method's func type (typeidx
 * 2*num_classes + the method's global function index). InvokeVirtual loads the
 * object's vtable (struct field 0), casts to the class's vtable type, loads the
 * slot's funcref, and call_refs it. The per-class vtable globals are populated by
 * wasm_types_emit_globals_content (override-resolved ref.func into each slot). */
int32_t wasm_vtable_typeidx(const wasm_types_t* wt, int class_id);
/* The ifaceIds array ((array i32)) — a rec-group type after the vtable, held by the
 * Class struct's synthesized ifaceIds field (the transitive interface class ids the
 * runtime interface-instanceof scan reads). */
int32_t wasm_iface_array_typeidx(const wasm_types_t* wt);
/* The number of rec-group header types (vtable, ifaceIds array, factory functype). */
int32_t wasm_hdr_type_count(void);
/* §20.3.6: the shared `[] -> [(ref null Object)]` functype every `$newInstance` has, and the type
 * of a Class singleton's `factory` field. */
int32_t wasm_factory_functype_idx(const wasm_types_t* wt);
/* The synthesized trailing fields of the Class struct (dispatch reads field0.vtable;
 * interface instanceof scans field0.ifaceIds). */
int32_t wasm_class_vtable_field_index(const wasm_types_t* wt);
int32_t wasm_class_ifaceids_field_index(const wasm_types_t* wt);
int32_t wasm_class_factory_field_index(const wasm_types_t* wt);
int32_t wasm_class_reflect_typeidx(const wasm_types_t* wt);  /* java.lang.Class's struct typeidx */
int32_t wasm_vtable_slot(const wasm_types_t* wt, int class_id, int method_idx);
int32_t wasm_functype_idx(const wasm_types_t* wt, int class_id, int method_idx);

/* The typeidx of a native import's host-ABI func type (externref for every ref) —
 * the type its import descriptor names. Distinct from wasm_functype_idx, which
 * gives the import's NATURAL type (its forwarder's, with concrete struct refs). */
int32_t wasm_import_functype_idx(const wasm_types_t* wt, int class_id, int method_idx);

/* Emit the code entry (locals vec + ops + end) of a native method's marshaling
 * forwarder: load params, extern.convert_any reference args, call the externref
 * import, any.convert_extern + ref.cast a reference result. The forwarder carries
 * the method's natural func type, so it slots into the vtable and call sites like
 * any compiled method while the genuine host edge stays externref-typed. */
void wasm_emit_forwarder_body(wasm_types_t* wt, const sema_ctx_t* s,
                              int class_id, int method_idx, emit_wasm_ctx* out);

/* Emit (into a function body) the creation of a `class_id` instance: field 0 is
 * the class's vtable global; the remaining data fields take their default values;
 * struct.new consumes them. The result is a (ref $class_id) on the stack — the
 * `New` codegen's whole job. (struct.new, not struct.new_default, because field 0
 * must carry the vtable so virtual/interface dispatch reads a populated table.) */
void wasm_types_emit_new(wasm_types_t* wt, emit_wasm_ctx* e, int class_id);
void wasm_types_emit_clone_copy(wasm_types_t* wt, emit_wasm_ctx* e, int class_id);  /* §20.1.5 shallow struct copy of `this` */

/* The root class (Object) — the common struct an interface-typed receiver is
 * cast to so its vtable header (field 0) can be read for interface dispatch. */
int32_t wasm_root_class(const wasm_types_t* wt);
bool    wasm_class_is_interface(const wasm_types_t* wt, int class_id);

/* Array typeidx for a primitive element, keyed by the SIR datatype (ArrayLoad/
 * ArrayStore) or the NewArray atype. Registers the array type on first use. */
int32_t wasm_types_array_for_dt(wasm_types_t* wt, sir_datatype_t dt);
int32_t wasm_types_array_for_atype(wasm_types_t* wt, sir_atype_t at);

/* Array typeidx for a reference element. `wasm_types_array_for_class` is the
 * concrete 1-level `(array (mut (ref null $class)))`. The `nested_*` builders give
 * a `levels`-deep (multi-dimensional) array over a primitive width or a reference
 * class, registering the whole chain — the one path for multi-dim array types. */
int32_t wasm_types_array_for_class(wasm_types_t* wt, int class_id);
int32_t wasm_types_nested_prim_array(wasm_types_t* wt, sir_datatype_t width, int levels);
int32_t wasm_types_nested_ref_array(wasm_types_t* wt, int class_id, int levels);

/* §10.2 the representation typeidx of an array VALUE with element `elem`: a REFERENCE
 * element collapses to the one RefArray struct (covariance free); a primitive element
 * (and the top-ref backing) stays a concrete invariant array. The lattice owns the
 * decision (lat_array_elem_is_ref) — this is the sole consumer that maps it to a typeidx. */
int32_t wasm_types_value_array_typeidx(wasm_types_t* wt, java_type_t elem);

/* A SIR ref descriptor (ClassRef/ArrayRef/PrimArray) → its value heaptype index —
 * the single authority shared by collect_slots (slot typing) and the ref-array read
 * cast. Reference arrays resolve to the RefArray struct. -1 if not a descriptor. */
int32_t wasm_types_ref_typeidx(wasm_types_t* wt, const sir_node_t* ref);

/* Emit a nullable reference valtype: (ref null $typeidx) = 0x63 <s33 typeidx>. */
void wasm_types_emit_ref(emit_wasm_ctx* e, int32_t typeidx);

/* Emit the storage/value type for a Java field or array element type
 * (primitive → valtype byte; class/array → a ref to its struct/array type). */
void wasm_types_emit_valtype(wasm_types_t* wt, emit_wasm_ctx* e, java_type_t t);

/* Emit the complete type section (id 1, length, vec(rectype)) appended to
 * `out`: one recursive group holding structs, the vtable, arrays, then funcs. */
void wasm_types_emit_section(wasm_types_t* wt, emit_wasm_ctx* out);

/* The type-section CONTENT bytes (one rec group of every type) for the module
 * assembler to decode via the shared jav reader. */
void wasm_types_emit_typesec_content(wasm_types_t* wt, const sema_ctx_t* s,
                                     emit_wasm_ctx* out);

#endif /* WASM_TYPES_H */
