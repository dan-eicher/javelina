/* wasm_types.c — WASM-GC type universe (see wasm_types.h).
 *
 * Encoding (wasm/spec/wasm.bbq): type section = vec(rectype); 0x4E = recursive
 * group; struct member subtype 0x50 (open) = vec(typeidx) supertypes + comptype;
 * comptype 0x5F struct / 0x5E array; reftype 0x63 (ref null) + s33 heaptype. */
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/type_lattice.h"   /* lat_value_class / lat_root_class */
#include "bbq_vec.h"
#include <string.h>
#include <stdlib.h>

#define WT_REC_GROUP   0x4E
#define WT_SUB_OPEN    0x50
#define WT_STRUCT      0x5F
#define WT_ARRAY       0x5E
#define WT_FUNC        0x60
#define WT_REF_NULL    0x63
#define WT_FIELD_MUT   0x01
#define WT_PACK_I8     0x78   /* §5.3.7 packtype i8 (storage only) */
#define WT_PACK_I16    0x77   /* §5.3.7 packtype i16 (storage only) */
#define WT_FUNCREF     0x70   /* funcref valtype shorthand (the vtable element) */
#define WT_I32         0x7F   /* i32 valtype (the ClassDesc classId field) */
#define WASM_HDR_TYPES 2      /* rec-group types after the class structs, before the signature
                               * arrays: vtable [+0], ifaceIds array [+1]. (Class IS the runtime
                               * type — its struct is among the class structs, not a header type.) */
#define WT_STRUCTREF   0x6B   /* (ref null struct) — the generic vtable header */
#define WT_ANYREF      0x6E   /* (ref null any) — the top reference: the RefArray backing element
                              * type, so a reference array may hold objects AND nested arrays (§10.1) */
#define WT_EXTERNREF   0x6F   /* externref — the host-ABI type for a native's refs. The embedder
                               * holds host references as externref (§ embedding), so a host func
                               * provided for an import speaks externref; an anyref import is not
                               * representable on the host side. `any` and `extern` are DISJOINT
                               * cones (§3.3.3), bridged at the boundary by extern.convert_any /
                               * any.convert_extern — the forwarder converts a GC ref to externref
                               * before the host call and the externref result back to a GC ref. */

void wasm_types_emit_ref(emit_wasm_ctx* e, int32_t typeidx) {
    ew_byte(e, WT_REF_NULL);
    ew_i32(e, typeidx);          /* concrete heaptype: s33 */
}

/* Is `t` a reference Java type (an object/array/null)? Such a value crosses the
 * host boundary as externref; primitives cross as their natural valtype. */
static bool jt_is_ref(java_type_t t) {
    return t.tag == JT_CLASS || t.tag == JT_ARRAY || t.tag == JT_NULL;
}

/* Where the header types (vtable, ifaceIds) + signature arrays sit — right after the structs
 * of the FIRST rec group. WHOLE/RUNTIME: after all structs (num_classes; RUNTIME's num_classes
 * IS the library count). PLUGIN: after the L LIBRARY structs only — the library types form one
 * rec group byte-identical to jre's (so gcanon unifies them cross-module), and user structs
 * trail in a SECOND rec group. If the header/arrays sat past the user structs too, every
 * library struct's closed-type serialization would shift and none would unify. */
static int wasm_hdr_base(const wasm_types_t* wt) {
    return wt->num_shared;   /* = num_classes in WHOLE/RUNTIME; = the shared prefix in PLUGIN */
}

int32_t wasm_types_class_typeidx(const wasm_types_t* wt, int class_id) {
    if (class_id < 0 || class_id >= wt->num_classes) return class_id;
    int p = wt->class_pos[class_id];   /* topological struct order (super < sub); shared classes [0,num_shared) */
    /* A user-source struct (class_pos past the shared prefix) trails the library rec group —
     * past the shared structs, the 2 header types, and the S signature arrays. */
    if (p >= wt->num_shared) return wt->num_shared + WASM_HDR_TYPES + wt->num_sig_arrays + (p - wt->num_shared);
    return p;
}

/* The count of func types in the rec layout: one per defined/forwarded function,
 * plus <clinit>, the exception tag, and one per native import. All are known from
 * sema before any body is codegen'd, so the body-array region's base is stable. */
static int wasm_num_functypes(const wasm_types_t* wt) {
    return sema_func_count(wt->sema)
         + (wt->has_clinit ? 1 : 0)
         + (wt->has_exceptions ? 1 : 0)
         + (wt->has_iface_helper ? 1 : 0)
         + sema_import_count(wt->sema);
}

/* The typeidx of arr_elems[i]: the signature/field region [0, num_sig_arrays) sits
 * before the func types (typeidx num_classes+1+i); the body-local region sits after
 * them. While Pass 1 is still filling the signature region (num_sig_arrays < 0),
 * everything is in that region. See wasm_types.h. */
static int32_t arr_typeidx_at(const wasm_types_t* wt, int i) {
    int s = wt->num_sig_arrays;
    if (s < 0 || i < s) return wasm_hdr_base(wt) + WASM_HDR_TYPES + i;   /* signature region (library rec group) */
    /* body-local arrays trail the func types (func base = num_classes+HDR+s regardless of mode) */
    return wt->num_classes + WASM_HDR_TYPES + s + wasm_num_functypes(wt) + (i - s);
}

int32_t wasm_types_array_typeidx(wasm_types_t* wt, java_type_t elem) {
    int n = (int)bbq_vec_len(wt->arr_elems);
    for (int i = 0; i < n; i++)
        if (jt_eq(wt->arr_elems[i], elem))
            return arr_typeidx_at(wt, i);
    /* an array-of-array references its element's array type — register it first */
    if (elem.tag == JT_ARRAY) wasm_types_array_typeidx(wt, *elem.element);
    int i = (int)bbq_vec_len(wt->arr_elems);
    bbq_vec_push(wt->arr_elems, elem);
    return arr_typeidx_at(wt, i);
}

int32_t wasm_types_array_for_dt(wasm_types_t* wt, sir_datatype_t dt) {
    java_type_t e;
    switch (dt) {
        case SIR_DTLONG:   e = jt_prim(JT_LONG);   break;
        case SIR_DTFLOAT:  e = jt_prim(JT_FLOAT);  break;
        case SIR_DTDOUBLE: e = jt_prim(JT_DOUBLE); break;
        case SIR_DTCHAR:   e = jt_prim(JT_CHAR);   break;
        case SIR_DTBYTE:   e = jt_prim(JT_BYTE);   break;
        case SIR_DTSHORT:  e = jt_prim(JT_SHORT);  break;
        case SIR_DTREF:    e = jt_null();          break;  /* one covariant ref-array type */
        default:           e = jt_prim(JT_INT);    break;
    }
    return wasm_types_array_typeidx(wt, e);
}

/* The array typeidx whose element is a reference to `class_id` — the concrete
 * `(array (mut (ref null $class)))`, NOT the covariant structref collapse. */
int32_t wasm_types_array_for_class(wasm_types_t* wt, int class_id) {
    java_type_t e = { .tag = JT_CLASS, .class_id = class_id, .element = NULL };
    return wasm_types_array_typeidx(wt, e);
}

/* §10.2/§10.7 the REPRESENTATION typeidx of an array-typed value/field `arr` — the lattice's
 * collapse authority mapped to a typeidx (never re-decided here). An overlay class (RefArray
 * for a reference element, per-width PrimArray for a primitive element) → its struct typeidx;
 * a concrete backing (the lattice returns -1: a JT_ARRAY_RAW overlay data field, or a JT_NULL
 * top-ref element) → its concrete (array W)/(array anyref) typeidx. Every value position —
 * valtype, field/global default, the overlay's own backing field — routes through this ONE
 * function, so a backing is never re-overlaid and a value is never left concrete. */
int32_t wasm_types_value_array_typeidx(wasm_types_t* wt, java_type_t arr) {
    int32_t ov = lat_array_overlay_class(wt->sema, arr);
    if (ov >= 0) return wasm_types_class_typeidx(wt, ov);
    java_type_t elem = arr.element ? *arr.element : jt_null();
    return wasm_types_array_typeidx(wt, elem);   /* the concrete (array W)/(array anyref) backing */
}

/* A SIR reference descriptor (ClassRef/ArrayRef/PrimArray) → its VALUE heaptype index
 * — the ONE authority (collect_slots' slot typing AND the ref-array element cast read
 * it, never a second copy): a class → its struct (interface→root); a class[] → the
 * RefArray struct (§10.2); a 1-dim primitive array → its concrete array; a multi-dim
 * primitive array (int[][]) holds array references → RefArray. -1 if not a descriptor. */
int32_t wasm_types_ref_typeidx(wasm_types_t* wt, const sir_node_t* ref) {
    if (!ref) return -1;
    switch (ref->tag) {
        case SIR_CLASSREF:
            return wasm_types_class_typeidx(wt, lat_value_class(wt->sema, ref->class_ref.class_id));
        case SIR_ARRAYREF:
            return wasm_types_class_typeidx(wt, lat_refarray_class(wt->sema));
        case SIR_PRIMARRAY:
            /* int[] value → PrimArray struct; int[][] (holds int[] refs) → RefArray. The
             * concrete (array W) is only the PrimArray backing, not the value type. */
            return ref->prim_array.dim > 1
                 ? wasm_types_class_typeidx(wt, lat_refarray_class(wt->sema))
                 : wasm_types_class_typeidx(wt, lat_primarray_class(wt->sema, ref->prim_array.width));
        default: return -1;
    }
}

/* SIR primitive datatype → its java_type. */
static java_type_t jt_of_dt(sir_datatype_t dt) {
    switch (dt) {
        case SIR_DTBYTE:   return jt_prim(JT_BYTE);
        case SIR_DTSHORT:  return jt_prim(JT_SHORT);
        case SIR_DTCHAR:   return jt_prim(JT_CHAR);
        case SIR_DTLONG:   return jt_prim(JT_LONG);
        case SIR_DTFLOAT:  return jt_prim(JT_FLOAT);
        case SIR_DTDOUBLE: return jt_prim(JT_DOUBLE);
        default:           return jt_prim(JT_INT);  /* byte/short/int/char unpack to i32 */
    }
}

/* The typeidx of the `levels`-deep array (levels >= 1) over `base`, registering
 * the whole nesting chain (the arena-owned `element` pointers live in the sema
 * arena). The single builder for nested/multi-dim array types — so no path
 * silently mis-types a multi-dimensional array (the prior structural-lookup
 * fallback could). */
static int32_t nested_array_typeidx(wasm_types_t* wt, java_type_t base, int levels) {
    bbq_arena* a = wt->sema->arena;
    java_type_t cur = base;
    for (int i = 0; i < levels; i++) {
        java_type_t* ep = (java_type_t*)bbq_arena_alloc(a, sizeof *ep);
        *ep = cur;
        cur = jt_array(ep);
    }
    return wasm_types_array_typeidx(wt, *cur.element);  /* array whose element is the (levels-1) array */
}

/* Nested array typeidx by element descriptor components: a `levels`-deep array
 * over a primitive width / over a reference class. */
int32_t wasm_types_nested_prim_array(wasm_types_t* wt, sir_datatype_t width, int levels) {
    return nested_array_typeidx(wt, jt_of_dt(width), levels);
}
int32_t wasm_types_nested_ref_array(wasm_types_t* wt, int class_id, int levels) {
    return nested_array_typeidx(wt, jt_class(class_id), levels);
}

int32_t wasm_types_array_for_atype(wasm_types_t* wt, sir_atype_t at) {
    java_type_t e;
    switch (at) {
        case SIR_ATBOOL:   e = jt_prim(JT_BYTE);   break;  /* boolean folds to the byte (i8) backing overlay */
        case SIR_ATBYTE:   e = jt_prim(JT_BYTE);   break;
        case SIR_ATSHORT:  e = jt_prim(JT_SHORT);  break;
        case SIR_ATCHAR:   e = jt_prim(JT_CHAR);   break;
        case SIR_ATLONG:   e = jt_prim(JT_LONG);   break;
        case SIR_ATFLOAT:  e = jt_prim(JT_FLOAT);  break;
        case SIR_ATDOUBLE: e = jt_prim(JT_DOUBLE); break;
        default:           e = jt_prim(JT_INT);    break;  /* ATINT (ATCLASS/ATREFARRAY: ref arrays) */
    }
    return wasm_types_array_typeidx(wt, e);
}

void wasm_types_emit_valtype(wasm_types_t* wt, emit_wasm_ctx* e, java_type_t t) {
    switch (t.tag) {
        case JT_LONG:   ew_byte(e, W_VT_I64); break;
        case JT_FLOAT:  ew_byte(e, W_VT_F32); break;
        case JT_DOUBLE: ew_byte(e, W_VT_F64); break;
        case JT_CLASS:  /* lat_value_class: interface → root object representation */
            wasm_types_emit_ref(e, wasm_types_class_typeidx(wt, lat_value_class(wt->sema, t.class_id)));
            break;
        case JT_ARRAY:  wasm_types_emit_ref(e, wasm_types_value_array_typeidx(wt, t)); break;  /* array → overlay struct or concrete backing */
        case JT_NULL:   ew_byte(e, WT_ANYREF); break;  /* the top reference (RefArray backing element) */
        /* byte/short/int/char/bool → i32 (the unpacked stack valtype). */
        default:        ew_byte(e, W_VT_I32); break;
    }
}

/* §5.3.7 storagetype for a STRUCT field / ARRAY element: sub-int Java types pack
 * into packtype storage (byte/bool → i8, short/char → i16); everything else is
 * its valtype. The packed store truncates and the read (struct/array.get_s|u)
 * sign-/zero-extends — value positions (params/locals/returns/globals) stay i32
 * and must use wasm_types_emit_valtype instead. */
static void wasm_types_emit_storagetype(wasm_types_t* wt, emit_wasm_ctx* e, java_type_t t) {
    switch (t.tag) {
        case JT_BYTE:
        case JT_BOOL:   ew_byte(e, WT_PACK_I8);  break;
        case JT_SHORT:
        case JT_CHAR:   ew_byte(e, WT_PACK_I16); break;
        default:        wasm_types_emit_valtype(wt, e, t); break;
    }
}

/* A class's struct fields are its superclass's fields (a GC subtype must extend
 * the supertype's field prefix) followed by its own — so walk base-most first.
 * Static fields are globals, not struct members. */
/* A class's struct fields are: the vtable header (field 0, contributed once by
 * the root), then the superclass's fields, then its own — so the data fields of
 * every class begin one past the vtable. */
static int count_instance_fields(const sema_ctx_t* sema, int class_id) {
    if (class_id < 0) return 0;
    const sema_class_t* cls = sema_get_class(sema, class_id);
    int n = (cls->super_id < 0) ? 1 : count_instance_fields(sema, cls->super_id); /* root: the Class header */
    for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++)
        if (!(cls->fields[i].modifiers & ACC_STATIC)) n++;
    if (class_id == sema_class_reflect_id(sema)) n += 3;  /* Class's synthesized trailing vtable + ifaceIds + factory */
    return n;
}

/* The synthesized trailing fields of the Class struct: [.. , vtable, ifaceIds, factory]. vtable
 * (funcref[]) is what dispatch reads via field0; ifaceIds ((array i32), the transitive interface
 * class ids) is what interface instanceof scans; factory (funcref) is the class's synthesized
 * `$newInstance`, or null when it cannot be instantiated (§20.3.6). */
int32_t wasm_class_vtable_field_index(const wasm_types_t* wt) {
    return count_instance_fields(wt->sema, sema_class_reflect_id(wt->sema)) - 3;
}
int32_t wasm_class_ifaceids_field_index(const wasm_types_t* wt) {
    return count_instance_fields(wt->sema, sema_class_reflect_id(wt->sema)) - 2;
}
int32_t wasm_class_factory_field_index(const wasm_types_t* wt) {
    return count_instance_fields(wt->sema, sema_class_reflect_id(wt->sema)) - 1;
}

/* The struct typeidx of java.lang.Class — every object's field 0 is a (ref null Class). */
int32_t wasm_class_reflect_typeidx(const wasm_types_t* wt) {
    return wasm_types_class_typeidx(wt, sema_class_reflect_id(wt->sema));
}

static void emit_instance_fields(wasm_types_t* wt, emit_wasm_ctx* e, int class_id) {
    if (class_id < 0) return;
    const sema_class_t* cls = sema_get_class(wt->sema, class_id);
    if (cls->super_id < 0) {                 /* root (Object): the Class header (field 0) */
        wasm_types_emit_ref(e, wasm_types_class_typeidx(wt, sema_class_reflect_id(wt->sema)));  /* (ref null Class) */
        ew_byte(e, WT_FIELD_MUT);            /* mutable: set at `new` / the reflect_init fixup */
    } else {
        emit_instance_fields(wt, e, cls->super_id);   /* inherited (incl the header) */
    }
    for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++) {
        if (cls->fields[i].modifiers & ACC_STATIC) continue;
        wasm_types_emit_storagetype(wt, e, cls->fields[i].type);   /* overlays' raw `data` → concrete via the lattice */
        ew_byte(e, WT_FIELD_MUT);
    }
    if (class_id == sema_class_reflect_id(wt->sema)) {   /* Class's synthesized trailing fields */
        wasm_types_emit_ref(e, wasm_vtable_typeidx(wt, 0));     ew_byte(e, WT_FIELD_MUT);  /* vtable  (funcref[]) */
        wasm_types_emit_ref(e, wasm_iface_array_typeidx(wt));   ew_byte(e, WT_FIELD_MUT);  /* ifaceIds ((array i32)) */
        ew_byte(e, WT_FUNCREF);                                 ew_byte(e, WT_FIELD_MUT);  /* factory (funcref) */
    }
}

int32_t wasm_types_field_base(const wasm_types_t* wt, int class_id) {
    if (class_id < 0) return 0;
    const sema_class_t* cls = sema_get_class(wt->sema, class_id);
    /* The root's own instance fields start AFTER its header (field 0); a non-root's start
     * after all inherited fields (count_instance_fields(super) already counts the header). */
    return (cls->super_id < 0) ? 1 : count_instance_fields(wt->sema, cls->super_id);
}

int32_t wasm_types_field_index(const wasm_types_t* wt, int decl_class, int local_idx) {
    if (decl_class < 0) return local_idx;
    int base = wasm_types_field_base(wt, decl_class);
    const sema_class_t* c = sema_get_class(wt->sema, decl_class);
    int pos = 0;
    for (int i = 0; i < local_idx && i < (int)bbq_vec_len(c->fields); i++)
        if (!(c->fields[i].modifiers & ACC_STATIC)) pos++;
    return base + pos;
}

static int count_static_fields(const sema_ctx_t* sema, int class_id) {
    const sema_class_t* c = sema_get_class(sema, class_id);
    int n = 0;
    for (int i = 0; i < (int)bbq_vec_len(c->fields); i++)
        if (c->fields[i].modifiers & ACC_STATIC) n++;
    return n;
}

static bool wasm_is_imported_class(const wasm_types_t* wt, int ci);  /* PLUGIN shared-class predicate; below */
static int shared_static_before(const wasm_types_t* wt, int ci);     /* below */
static int user_static_before(const wasm_types_t* wt, int ci);       /* below */

int32_t wasm_global_index(const wasm_types_t* wt, int decl_class, int local_idx) {
    if (decl_class < 0) return local_idx;
    const sema_class_t* c = sema_get_class(wt->sema, decl_class);
    int off = 0;                          /* static fields before local_idx in this class */
    for (int i = 0; i < local_idx && i < (int)bbq_vec_len(c->fields); i++)
        if (c->fields[i].modifiers & ACC_STATIC) off++;
    if (wt->mode == SEMA_MODE_PLUGIN) {
        if (wasm_is_imported_class(wt, decl_class))                        /* imported: [0, lib_static) */
            return shared_static_before(wt, decl_class) + off;
        return wasm_imported_global_count(wt) + user_static_before(wt, decl_class) + off;   /* defined */
    }
    int base = 0;
    for (int ci = 0; ci < decl_class; ci++) base += count_static_fields(wt->sema, ci);
    return base + off;
}

/* The module function index of a method = its position in sema's emitted-function
 * table (the single authority, computed where it is fully known). -1 = a library
 * method (host import), which the backend must not call as a defined function. */
int32_t wasm_import_index(const wasm_types_t* wt, int class_id, int method_idx) {
    int n = sema_import_count(wt->sema);
    for (int i = 0; i < n; i++) {
        sema_func_ent_t e = sema_import_at(wt->sema, i);
        if (e.class_id == class_id && e.method_id == method_idx) return i;
    }
    return -1;
}

int32_t wasm_func_index(const wasm_types_t* wt, int decl_class, int method_idx) {
    /* A method's call target is its DEFINED function when it has one — a compiled
     * body OR a native's marshaling forwarder (sema lists both in its function
     * table). Only a primitive-signature native (no ref to marshal) has no
     * forwarder; it is called as the host import directly at funcidx [0,nimports). */
    int32_t d = sema_func_index(wt->sema, decl_class, method_idx);
    if (d >= 0) return wt->nimports + d;            /* defined: offset past the import range   */
    return wasm_import_index(wt, decl_class, method_idx);  /* primitive native: direct import   */
}

int32_t wasm_vtable_typeidx(const wasm_types_t* wt, int class_id) {
    (void)class_id;
    return wasm_hdr_base(wt);   /* one global vtable type, right after the library structs */
}

/* The rec-group types that follow the class structs, before the signature arrays:
 * the vtable (funcref array), the ClassDesc struct, and the interfaces array. Their
 * count is the base offset the signature-array / func-type regions sit past — every
 * such offset uses WASM_HDR_TYPES so adding a header type can't miss a site. */
int32_t wasm_iface_array_typeidx(const wasm_types_t* wt) { return wasm_hdr_base(wt) + 1; }
/* The count of rec-group header types. The ONE authority — byte pins read it rather than carrying
 * a magic constant that silently rots when a header type is added. */
int32_t wasm_hdr_type_count(void) { return WASM_HDR_TYPES; }
/* §20.3.6: the `[] -> [(ref null Object)]` functype a `$newInstance` carries. Every synthesized
 * factory has the same signature, and identical STANDALONE functypes are the same defined type
 * (equality holds iff their closures are syntactically equivalent), so any of them serves as the
 * ref.cast / call_ref target. It must be a standalone functype, not a rec-group member: a member's
 * reference to Object is a RECURSIVE reference, which the spec distinguishes from a reference to a
 * previously defined type — the two functypes would not be equal, and the module would not validate.
 * Returns -1 when the module synthesizes no factory (then nothing lowers ClassConstruct either). */
int32_t wasm_factory_functype_idx(const wasm_types_t* wt) {
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(wt->sema, ci);
        for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
            if (c->methods[mi].is_synthetic_new_instance) return wasm_functype_idx(wt, ci, mi);
    }
    return -1;
}

/* forward: the slot/virtuality predicates are defined just below. */
static bool first_sig_occurrence(const sema_ctx_t* sema, int ci, int j);

/* A virtual method occupies a vtable slot: instance, not private (those are
 * invokespecial), not a constructor. */
/* JLS §8.4.8's two predicates live in SEMA, which owns the class table — the optimizer
 * devirtualizes with the same ones, and a second signature comparator here is how the
 * vtable and the devirtualizer would silently come to disagree about what an override is.
 * These are the local names for them; the rule is sema's. */
#define is_virtual_method(m)  sema_is_virtual_method(m)
#define same_sig(a, b)        sema_same_vsig((a), (b))

/* Is (ci, j) the FIRST occurrence of its virtual signature in (class, method)
 * enumeration order? Distinct signatures get successive global slots. */
static bool first_sig_occurrence(const sema_ctx_t* sema, int ci, int j) {
    const sema_method_t* m = &sema_get_class(sema, ci)->methods[j];
    for (int pci = 0; pci <= ci; pci++) {
        const sema_class_t* pc = sema_get_class(sema, pci);
        int jmax = (pci == ci) ? j : (int)bbq_vec_len(pc->methods);
        for (int pj = 0; pj < jmax; pj++)
            if (is_virtual_method(&pc->methods[pj]) && same_sig(&pc->methods[pj], m))
                return false;
    }
    return true;
}

/* The GLOBAL vtable slot of a method: the index of its (name, parameter-types)
 * signature in the program-wide enumeration of distinct virtual signatures.
 * Overrides AND interface implementations of the same signature share a slot,
 * so one mechanism (struct.get vtable; struct.get slot; call_ref) dispatches
 * both virtual and interface calls regardless of the receiver's concrete type.
 *
 * A LOOKUP in the table built once by build_method_slots. It used to RESCAN every class
 * and every method — and call first_sig_occurrence, itself a rescan — on EVERY CALL, and
 * codegen calls it once per virtual call site: 42 MILLION sema_is_virtual_method calls on
 * the jre. The same blowup was found once before and fixed only for the slot TABLE (see
 * build_vtable_slots' "computed ONCE per assembly, not per slot"); the per-call-site query
 * was left quadratic. Now neither rescans. */
int32_t wasm_vtable_slot(const wasm_types_t* wt, int class_id, int method_idx) {
    if (!wt->method_slot || class_id < 0 || class_id >= wt->num_classes) return 0;
    int base = wt->class_method_base[class_id];
    int n    = (int)bbq_vec_len(sema_get_class(wt->sema, class_id)->methods);
    if (method_idx < 0 || method_idx >= n) return 0;
    return wt->method_slot[base + method_idx];
}

/* The vtable length = the number of distinct virtual signatures program-wide (each
 * first-occurrence virtual method contributes one slot) — counted once, with the table. */
int32_t wasm_vtable_len(const wasm_types_t* wt) {
    return wt->num_vtable_slots;
}

/* Build (class, method) → vtable slot ONCE, in the order wasm_vtable_slot used to
 * enumerate: class order, then method order, one slot per FIRST occurrence of a virtual
 * signature. Every later method sharing that signature — an override, or an interface
 * implementation — gets the same slot, which is exactly what makes one dispatch mechanism
 * serve both.
 *
 * ONE pass to number the first occurrences, then one pass matching each remaining virtual
 * method against the numbered signatures. Quadratic in (methods × slots) ONCE, instead of
 * quadratic in (classes × methods) on EVERY call from codegen. */
static void build_method_slots(wasm_types_t* wt) {
    const sema_ctx_t* sema = wt->sema;
    int total = 0;
    wt->class_method_base = (int*)bbq_arena_alloc(
        sema->arena, (size_t)(wt->num_classes > 0 ? wt->num_classes : 1) * sizeof(int));
    for (int ci = 0; ci < wt->num_classes; ci++) {
        wt->class_method_base[ci] = total;
        total += (int)bbq_vec_len(sema_get_class(sema, ci)->methods);
    }
    wt->method_slot = (int32_t*)bbq_arena_alloc(
        sema->arena, (size_t)(total > 0 ? total : 1) * sizeof(int32_t));
    for (int i = 0; i < total; i++) wt->method_slot[i] = 0;

    /* Pass 1 — number the distinct signatures. `first_sig_occurrence` is not needed: a
     * signature is new iff it does not match one already numbered. */
    int* sig_cls = NULL;   /* bbq_vec: slot → the class of its first occurrence  */
    int* sig_mid = NULL;   /* bbq_vec: slot → the method index of that occurrence */
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(sema, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!is_virtual_method(&c->methods[j])) continue;
            bool seen = false;
            for (int s = 0; s < (int)bbq_vec_len(sig_cls) && !seen; s++)
                if (same_sig(&c->methods[j],
                             &sema_get_class(sema, sig_cls[s])->methods[sig_mid[s]]))
                    seen = true;
            if (seen) continue;
            bbq_vec_push(sig_cls, ci);
            bbq_vec_push(sig_mid, j);
        }
    }
    wt->num_vtable_slots = (int)bbq_vec_len(sig_cls);

    /* Pass 2 — every virtual method takes the slot of its signature. */
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(sema, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!is_virtual_method(&c->methods[j])) continue;
            for (int s = 0; s < wt->num_vtable_slots; s++)
                if (same_sig(&c->methods[j],
                             &sema_get_class(sema, sig_cls[s])->methods[sig_mid[s]])) {
                    wt->method_slot[wt->class_method_base[ci] + j] = s;
                    break;
                }
        }
    }
    bbq_vec_free(sig_cls);
    bbq_vec_free(sig_mid);
}

/* The program's vtable slot table: slot s ↔ the (class, method) of the
 * first-occurrence virtual signature, in the SAME order wasm_vtable_slot numbers
 * (class order, then method order). Computed ONCE per assembly (not per slot —
 * the per-slot recompute is an O(n⁶) blowup over the bundled library classes).
 * Appends to the bbq_vecs *slot_cls / *slot_mid (caller frees them). */
static void build_vtable_slots(const wasm_types_t* wt, int** slot_cls, int** slot_mid) {
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(wt->sema, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++)
            if (is_virtual_method(&c->methods[j]) && first_sig_occurrence(wt->sema, ci, j))
                { bbq_vec_push(*slot_cls, ci); bbq_vec_push(*slot_mid, j); }
    }
}

/* The typeidx of a native import's HOST-ABI func type (externref refs): the
 * import functypes follow defined funcs + <clinit> + tag in the rec group. This
 * is the type the import DESC names — distinct from the import's natural func
 * type (the forwarder's), which wasm_functype_idx returns via its defined slot. */
int32_t wasm_import_functype_idx(const wasm_types_t* wt, int class_id, int method_idx) {
    int32_t imp = wasm_import_index(wt, class_id, method_idx);
    if (imp < 0) return -1;
    int32_t base = wt->num_classes + WASM_HDR_TYPES + wt->num_sig_arrays;  /* func types follow the signature-array region */
    int nf = sema_func_count(wt->sema);
    int extra = (wt->has_clinit ? 1 : 0) + (wt->has_exceptions ? 1 : 0)
              + (wt->has_iface_helper ? 1 : 0);
    return base + nf + extra + imp;
}

/* A virtual method's vtable slot has NO defined or imported occupant in THIS module: an
 * abstract/interface method whose implementors (if any) all live in other modules — e.g. the
 * jre declaring `Observer` and dispatching `Observer.update` with every implementor in a
 * plugin. Such a slot cannot borrow a functype from an occupant, so it gets a synthesized
 * abstract functype. Mirrors the occupant search in wasm_functype_idx. */
static bool vtable_slot_unoccupied(const wasm_types_t* wt, int class_id, int method_idx) {
    /* Two virtual methods share a vtable slot IFF they have the same signature (that is how
     * wasm_vtable_slot/first_sig_occurrence assign slots), so compare signatures directly —
     * NOT nested wasm_vtable_slot calls, which recompute first_sig_occurrence per candidate
     * and make this O(n²) inside an O(n²) caller. */
    const sema_method_t* target = &sema_get_class(wt->sema, class_id)->methods[method_idx];
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(wt->sema, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!is_virtual_method(&c->methods[j])) continue;
            if (!same_sig(target, &c->methods[j])) continue;             /* same slot ⟺ same signature */
            if (sema_func_index(wt->sema, ci, j) >= 0) return false;      /* a DEFINED occupant   */
            if (wasm_import_functype_idx(wt, ci, j) >= 0) return false;   /* an IMPORTED occupant */
        }
    }
    return true;
}

/* Position of `(class_id, method_idx)` among the module's unoccupied abstract vtable
 * introducers, in (class, method) order; -1 if it is not one. The abstract functypes are
 * emitted in this SAME order trailing the body-local arrays (wasm_types_emit_typesec_content),
 * so an introducer's ordinal is its offset into that region. `*count_out` (optional) gets the
 * total count. */
static int32_t abstract_functype_ord(const wasm_types_t* wt, int class_id, int method_idx, int* count_out) {
    int32_t ord = -1; int n = 0;
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(wt->sema, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!is_virtual_method(&c->methods[j])) continue;
            if (sema_func_index(wt->sema, ci, j) >= 0) continue;          /* has a body → not abstract here */
            if (!vtable_slot_unoccupied(wt, ci, j)) continue;
            if (ci == class_id && j == method_idx) ord = n;
            n++;
        }
    }
    if (count_out) *count_out = n;
    return ord;
}

/* Count of the module's synthesized abstract functypes (see abstract_functype_ord). */
static int wasm_num_abstract_functypes(const wasm_types_t* wt) {
    int n = 0; (void)abstract_functype_ord(wt, -1, -1, &n); return n;
}

int32_t wasm_functype_idx(const wasm_types_t* wt, int class_id, int method_idx) {
    /* One rec group, laid out: structs [0,N), the global vtable [N], array types
     * [N+1, N+1+A), then func types: defined funcs [base, base+nf), <clinit>, tag,
     * then the IMPORTS. NOTE the functype block is indexed by a function's DEFINED
     * position (sema_func_index), NOT its funcidx — the typeidx space does not
     * prepend imports (§2.5.1), so it stays dense over defined funcs while
     * wasm_func_index offsets the separate funcidx space. A native's NATURAL func
     * type lives at its forwarder's defined slot (sema_func_index), so virtual and
     * non-virtual natives resolve through the ordinary paths below. */
    int32_t base = wt->num_classes + WASM_HDR_TYPES + wt->num_sig_arrays;  /* func types follow the signature-array region */
    const sema_method_t* m = &sema_get_class(wt->sema, class_id)->methods[method_idx];
    if (is_virtual_method(m)) {
        /* Every occupant of a vtable slot (the introducer, its overrides, an
         * interface's implementors) shares ONE func type so that the funcref the
         * vtable holds, the dispatch ref.cast, and call_ref all agree by EXACT
         * type index — no reliance on cross-index structural canonicalization,
         * and a slot whose introducer isn't emitted (interface/abstract) still
         * resolves. The slot's first EMITTED occupant's func type is the canon. */
        int32_t imp_ft = -1;                       /* PLUGIN: first IMPORTED occupant's functype (fallback) */
        for (int ci = 0; ci < wt->num_classes; ci++) {
            const sema_class_t* c = sema_get_class(wt->sema, ci);
            for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
                if (!is_virtual_method(&c->methods[j])) continue;
                /* Same vtable slot ⟺ same signature — compare directly, NOT a per-candidate
                 * wasm_vtable_slot recompute (that made this O(n²) per call, and it is called
                 * per dispatch site during codegen → quadratic-in-prelude blowup). */
                if (!same_sig(m, &c->methods[j])) continue;
                int32_t efi = sema_func_index(wt->sema, ci, j);   /* defined-position into the functype block */
                if (efi >= 0) return base + efi;   /* the slot's canonical func type (a DEFINED occupant) */
                if (imp_ft < 0) {                  /* PLUGIN: an inherited library method the plugin only IMPORTS */
                    int32_t ift = wasm_import_functype_idx(wt, ci, j);   /* iterated low→high ci ⇒ the introducer's */
                    if (ift >= 0) imp_ft = ift;
                }
            }
        }
        if (imp_ft >= 0) return imp_ft;   /* slot occupied only by imports (natural functype; §3.3 struct-matches call_ref) */
        /* No occupant: an abstract/interface introducer with no implementor in this module.
         * Point the dispatch at the synthesized abstract functype — canonical (ref root, params,
         * result), which §3.3.10 canonical equality matches to any (cross-module) implementor's
         * funcref. These trail the body-local arrays, so their base is past the func-type block
         * (wasm_num_functypes) AND the body arrays (num_arrays − num_sig). */
        int32_t aord = abstract_functype_ord(wt, class_id, method_idx, NULL);
        if (aord >= 0) {
            int32_t num_body = (int32_t)bbq_vec_len(wt->arr_elems) - wt->num_sig_arrays;
            if (num_body < 0) num_body = 0;
            return base + wasm_num_functypes(wt) + num_body + aord;
        }
        return base;   /* genuinely unreachable */
    }
    int32_t pos = sema_func_index(wt->sema, class_id, method_idx);   /* non-virtual: own type, by defined-position */
    return base + (pos >= 0 ? pos : 0);
}

/* The body of a native method's bridging FORWARDER (natural func type → so it sits
 * uniformly in a vtable slot and at every call site). It loads each param, bridges
 * references to the externref-typed host import, and bridges a reference result
 * back: `any` and `extern` are disjoint cones (§3.3.3), so a GC ref is carried to
 * the host with extern.convert_any and an externref result is brought back with
 * any.convert_extern then narrowed with ref.cast to the concrete return type.
 * Primitives pass through unconverted. Emits the full code entry (locals vec, op
 * bytes, terminating `end`) the structured backend would. */
void wasm_emit_forwarder_body(wasm_types_t* wt, const sema_ctx_t* s,
                              int class_id, int method_idx, emit_wasm_ctx* out) {
    const sema_method_t* m = &sema_get_class(s, class_id)->methods[method_idx];
    bool is_static = (m->modifiers & ACC_STATIC) != 0;
    ew_u32(out, 0);                              /* locals vec: none beyond the params */
    /* §20.1.1 Object.getClass(): return `this`'s Class — field 0, the unified runtime-type
     * header — not a host call. (getClass is native, so it has a forwarder slot.) */
    if (class_id == wasm_root_class(wt) && method_idx == sema_getclass_method_id(s)) {
        ew_emit(out, WOP_LOCAL_GET);  ew_u32(out, 0);
        ew_emit(out, WOP_STRUCT_GET); ew_u32(out, (uint32_t)wasm_types_class_typeidx(wt, wasm_root_class(wt)));
        ew_u32(out, 0);
        ew_byte(out, 0x0B);                      /* end */
        return;
    }
    int slot = 0;
    if (!is_static) {                            /* receiver `this`: always a ref → externref */
        ew_emit(out, WOP_LOCAL_GET); ew_u32(out, 0);
        ew_emit(out, WOP_EXTERN_CONVERT_ANY);
        slot = 1;
    }
    for (int i = 0; i < m->param_count; i++) {
        ew_emit(out, WOP_LOCAL_GET); ew_u32(out, (uint32_t)(slot + i));
        if (jt_is_ref(m->param_types[i])) ew_emit(out, WOP_EXTERN_CONVERT_ANY);
    }
    ew_emit(out, WOP_CALL);
    ew_u32(out, (uint32_t)wasm_import_index(wt, class_id, method_idx));
    if (jt_is_ref(m->return_type)) {             /* externref result → GC ref, then narrow */
        ew_emit(out, WOP_ANY_CONVERT_EXTERN);
        ew_emit(out, WOP_REF_CAST_NULL);
        int32_t ti = (m->return_type.tag == JT_ARRAY)
            ? wasm_types_array_typeidx(wt, *m->return_type.element)
            : wasm_types_class_typeidx(wt, lat_value_class(s, m->return_type.class_id));
        ew_i32(out, ti);
    }
    ew_byte(out, 0x0B);                          /* §5.4.1 function body terminating `end` */
}

int32_t wasm_root_class(const wasm_types_t* wt) {
    return lat_root_class(wt->sema);   /* java.lang.Object — the one root authority */
}

/* Is the target class an interface? Interface refs type as the root, so a structural
 * ref.test can't distinguish implementors — instanceof/checkcast route through the
 * iface_instanceof helper instead. */
bool wasm_class_is_interface(const wasm_types_t* wt, int class_id) {
    return sema_get_class(wt->sema, class_id)->is_interface;
}

/* Topologically order the classes so a supertype always precedes its subtypes
 * (§3.2.11: a struct's declared super must have a smaller type index). Java's
 * class order is arbitrary, so compute class_pos[class_id] = struct typeidx and
 * order[pos] = class_id. Any class whose super is dangling/cyclic is appended in
 * declaration order (it can't satisfy the constraint, but won't be referenced). */
/* Topological class order into the bbq_vecs `wt->class_pos` (class_id → typeidx,
 * pre-sized to -1) and `order` (typeidx → class_id, appended). */
/* A user-SOURCE class (has an AST body, not from an imported package). Its struct is the
 * plugin's OWN — it trails into the second rec group. Library-source and synthesized (array
 * overlay / array-Class, ast_node == NULL) classes are SHARED with jre → the first group. */
static bool is_user_source(const sema_ctx_t* sema, int ci) {
    const sema_class_t* c = sema_get_class(sema, ci);
    return c->import_pkg < 0 && c->ast_node != NULL;
}
static bool is_array_class(const sema_ctx_t* sema, int ci);   /* §10.8 array Class; defined below */
static bool is_glocal(const sema_ctx_t* sema, int ci);        /* locally-owned (user OR array Class); below */

static void compute_class_order(wasm_types_t* wt, const sema_ctx_t* sema, int** order) {
    int n = wt->num_classes;
    for (int i = 0; i < n; i++) bbq_vec_push(wt->class_pos, -1);
    /* RUNTIME + PLUGIN place structs in TWO phases: the SHARED set (library-source + array value
     * OVERLAYS — jre's exact group A) first, then the LOCALLY-owned set (user-source + §10.8
     * per-exact-type array Classes) trailing into a SECOND rec group. This makes group A byte-
     * identical between jre and every plugin — array Classes differ per module (each synthesizes
     * only the ones its code uses), so they must NOT sit in the shared group. WHOLE keeps a single
     * group (its whole-program byte shape is pinned). */
    bool split = (wt->mode != SEMA_MODE_WHOLE);
    int nphases = split ? 2 : 1;
    for (int phase = 0; phase < nphases; phase++) {
        int progress = 1;
        while (progress) {
            progress = 0;
            for (int ci = 0; ci < n; ci++) {
                if (wt->class_pos[ci] >= 0) continue;
                if (split && is_glocal(sema, ci) != (phase == 1)) continue;  /* 0 = shared, 1 = local (user + array Class) */
                int sup = sema_get_class(sema, ci)->super_id;
                if (sup < 0 || sup >= n || wt->class_pos[sup] >= 0) {
                    wt->class_pos[ci] = (int)bbq_vec_len(*order); bbq_vec_push(*order, ci); progress = 1;
                }
            }
        }
        if (phase == 0) wt->num_shared = (int)bbq_vec_len(*order);   /* end of the shared prefix */
    }
    for (int ci = 0; ci < n; ci++)   /* dangling/cyclic super: append in declaration order */
        if (wt->class_pos[ci] < 0) { wt->class_pos[ci] = (int)bbq_vec_len(*order); bbq_vec_push(*order, ci); }
    if (!split) wt->num_shared = n;   /* every class is shared (WHOLE) */
}

void wasm_types_build(wasm_types_t* wt, const sema_ctx_t* sema) {
    wt->sema = sema;
    wt->mode = (int)sema->mode;              /* WHOLE/RUNTIME/PLUGIN — mode-aware index authorities read this */
    wt->num_classes = (int)bbq_vec_len(sema->classes);
    wt->nimports = sema_import_count(sema);  /* native targets → function imports at funcidx [0, nimports) */
    wt->struct_bytes = NULL;
    wt->arr_elems = NULL;
    wt->num_sig_arrays = -1;                 /* unfrozen: Pass 1 fills the signature region */
    wt->class_pos = NULL;
    build_method_slots(wt);                  /* (class, method) → vtable slot, once */
    int* order = NULL;                       /* bbq_vec<int>: typeidx → class_id */
    compute_class_order(wt, sema, &order);

    /* Pass 1 — register every array type reachable from an instance field OR a
     * method signature (param/result). These are the only array types a struct or
     * func type can reference, so they form the signature region that precedes the
     * func types. Array types appearing ONLY inside a body (a `new int[]`, an
     * array-typed local) are registered later during codegen and land in the
     * body-local region PAST the func types (see wasm_types_array_typeidx), so they
     * never shift a func-type index a call_ref has already baked. */
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* cls = sema_get_class(sema, ci);
        for (int fi = 0; fi < (int)bbq_vec_len(cls->fields); fi++) {
            if (cls->fields[fi].modifiers & ACC_STATIC) continue;
            java_type_t t = cls->fields[fi].type;
            /* Register the type the FIELD actually references — via the lattice's collapse authority,
             * NOT the raw element array. A reference array (Box[]) collapses to the covariant RefArray
             * overlay (a shared class); registering (array (ref Box)) instead would forward-reference a
             * user class from the shared rec group (an illegal §3.5.1 forward ref). */
            if (t.tag == JT_ARRAY) wasm_types_value_array_typeidx(wt, t);
        }
        for (int mi = 0; mi < (int)bbq_vec_len(cls->methods); mi++) {
            const sema_method_t* m = &cls->methods[mi];
            if (m->return_type.tag == JT_ARRAY) wasm_types_value_array_typeidx(wt, m->return_type);
            for (int pi = 0; pi < m->param_count; pi++)
                if (m->param_types[pi].tag == JT_ARRAY)
                    wasm_types_value_array_typeidx(wt, m->param_types[pi]);
        }
    }
    wt->num_sig_arrays = (int)bbq_vec_len(wt->arr_elems);   /* freeze: everything past here is body-local */

    /* Pass 2 — encode each class's struct subtype, in topological (typeidx)
     * order so each encoding lands at its class_pos and supers come first. */
    emit_wasm_ctx sb = { wt->struct_bytes };
    wt->struct_split = 0;
    for (int pos = 0; pos < wt->num_classes; pos++) {
        if (pos == wt->num_shared) wt->struct_split = (int)bbq_vec_len(sb.code);  /* shared|user struct boundary */
        int ci = order[pos];
        const sema_class_t* cls = sema_get_class(sema, ci);
        ew_byte(&sb, WT_SUB_OPEN);
        if (cls->super_id >= 0 && cls->super_id < wt->num_classes) {
            ew_u32(&sb, 1);
            ew_u32(&sb, (uint32_t)wasm_types_class_typeidx(wt, cls->super_id));
        } else {
            ew_u32(&sb, 0);                 /* Object / dangling super: no supertype */
        }
        ew_byte(&sb, WT_STRUCT);
        ew_u32(&sb, (uint32_t)count_instance_fields(sema, ci));
        emit_instance_fields(wt, &sb, ci);
    }
    wt->struct_bytes = sb.code;
    bbq_vec_free(order);
}

/* One func comptype member (0x60 + params + results) for emitted function `fi`,
 * concrete reftypes throughout. Param 0 is the receiver `this`: for a VIRTUAL
 * method it is typed at the ROOT class, so every override of a signature has the
 * one identical func type the shared vtable slot needs (WASM-GC canonicalizes
 * structurally equal func types — funcref subtyping is contravariant in the
 * receiver, so an (ref Sub) receiver could NOT sit in a slot typed (ref Super)).
 * Non-virtual instance methods (ctors/private — invokespecial, not dispatched)
 * keep their own-class receiver. The body re-narrows `this` (LoadThis). */
/* `host_abi` selects the type for a native method's IMPORT (the linkable host
 * edge): every reference — receiver, params, return — becomes anyref, the common
 * supertype of all GC objects (§3.3.3). It is abstract, so a host can express it
 * without naming a module struct (§3.3.10), and concrete refs subtype into/out of
 * it. With it false this is the NATURAL func type the forwarder/vtable use. */
static void emit_functype_for(wasm_types_t* wt, const sema_ctx_t* s,
                              emit_wasm_ctx* out, int class_id, int method_id,
                              bool host_abi) {
    const sema_class_t*  cls = sema_get_class(s, class_id);
    const sema_method_t* m   = &cls->methods[method_id];
    int e_class_id = class_id;                   /* receiver type uses the declaring class */
    bool is_static = (m->modifiers & ACC_STATIC) != 0;
    ew_byte(out, WT_FUNC);
    ew_u32(out, (uint32_t)(m->param_count + (is_static ? 0 : 1)));
    if (!is_static) {                            /* the receiver `this` is always a ref */
        if (host_abi) ew_byte(out, WT_EXTERNREF);
        else {
            int recv = is_virtual_method(m) ? wasm_root_class(wt) : e_class_id;
            wasm_types_emit_ref(out, wasm_types_class_typeidx(wt, recv));
        }
    }
    for (int i = 0; i < m->param_count; i++) {
        if (host_abi && jt_is_ref(m->param_types[i])) ew_byte(out, WT_EXTERNREF);
        else wasm_types_emit_valtype(wt, out, m->param_types[i]);
    }
    int nr = (m->return_type.tag != JT_VOID) ? 1 : 0;
    ew_u32(out, (uint32_t)nr);
    if (nr) {
        if (host_abi && jt_is_ref(m->return_type)) ew_byte(out, WT_EXTERNREF);
        else wasm_types_emit_valtype(wt, out, m->return_type);
    }
}

/* One array comptype member (bare shorthand): (array (mut storagetype(elem))). */
static void emit_array_comptype(wasm_types_t* wt, emit_wasm_ctx* out, java_type_t elem) {
    ew_byte(out, WT_ARRAY);
    wasm_types_emit_storagetype(wt, out, elem);
    ew_byte(out, WT_FIELD_MUT);
}

/* The type-section CONTENT (the body the module writer frames with id+size). Layout:
 * structs [0,N), vtable [N], the SIGNATURE arrays [N+1, N+1+S), then the func types,
 * then the BODY-LOCAL arrays. The single authority for type layout; the assembler
 * decodes these bytes with the shared jav reader (the spec-grammar verification gate)
 * into the module's type section. */
void wasm_types_emit_typesec_content(wasm_types_t* wt, const sema_ctx_t* s,
                                     emit_wasm_ctx* out) {
    int num_arrays = (int)bbq_vec_len(wt->arr_elems);
    int num_sig    = wt->num_sig_arrays;     /* field/signature arrays — the rec-group region */
    int num_body   = num_arrays - num_sig;   /* body-local arrays — standalone, past the func types */
    int nf = sema_func_count(s);
    int nclinit = wt->has_clinit ? 1 : 0;
    int ntag = wt->has_exceptions ? 1 : 0;  /* the exception tag's functype */
    int niface = wt->has_iface_helper ? 1 : 0;  /* the iface_instanceof helper's functype */
    int nimp = sema_import_count(s);         /* one functype per native import, appended last */
    int nfunctypes = nf + nclinit + ntag + niface + nimp;
    int nabstract  = wasm_num_abstract_functypes(wt);   /* trailing functypes for unoccupied abstract slots */
    /* The mutually-recursive GC types (structs, vtable, signature arrays) form a rec group;
     * the func types follow as STANDALONE top-level types (each its own singleton rec group).
     * A func type buried in a multi-member rec group would, per §3.3.10 closed-type matching,
     * be a DISTINCT closed type from a host's structurally equal standalone functype — i.e.
     * unlinkable at import. Standalone makes them match. The body-local arrays trail as
     * standalone types so the func-type indices never depend on the body-array count.
     *
     * PLUGIN splits the GC types into TWO rec groups: group A = the SHARED types (library +
     * synthesized structs + vtable + ifaceIds + sig arrays), emitted byte-identically to jre's
     * sole rec group so gcanon interns them to the SAME global ids (a member's closed-type key
     * embeds its whole group, so the shared group must stand alone); group B = the plugin's
     * user structs, which reference group A by typeidx (inter-group → stable global id). */
    int nuser   = wt->num_classes - wt->num_shared;                 /* trailing (local) structs; 0 in WHOLE */
    int ngroups = (nuser > 0) ? 2 : 1;                              /* RUNTIME + PLUGIN split off the local group */
    ew_u32(out, (uint32_t)(ngroups + nfunctypes + num_body + nabstract));
    /* group A — the shared GC types */
    ew_byte(out, WT_REC_GROUP);
    ew_u32(out, (uint32_t)(wt->num_shared + WASM_HDR_TYPES + num_sig));
    int shared_len = (ngroups == 2) ? wt->struct_split : (int)bbq_vec_len(wt->struct_bytes);
    for (int i = 0; i < shared_len; i++)
        ew_byte(out, wt->struct_bytes[i]);
    /* the global vtable: (array (mut funcref)) at typeidx num_shared */
    ew_byte(out, WT_ARRAY); ew_byte(out, WT_FUNCREF); ew_byte(out, WT_FIELD_MUT);
    /* the ifaceIds array, at typeidx num_shared+1: (array i32) — a class's transitive
     * interface class ids, held by the Class struct's synthesized ifaceIds field; the
     * runtime interface-instanceof scan reads it (mirrors sema_is_subclass_of). */
    ew_byte(out, WT_ARRAY);
    ew_byte(out, WT_I32);                                   ew_byte(out, 0x00);
    for (int i = 0; i < num_sig; i++)        /* signature arrays: rec-group members */
        emit_array_comptype(wt, out, wt->arr_elems[i]);
    /* group B — the plugin's user structs (a second rec group, referencing group A) */
    if (ngroups == 2) {
        ew_byte(out, WT_REC_GROUP);
        ew_u32(out, (uint32_t)nuser);
        for (int i = wt->struct_split; i < (int)bbq_vec_len(wt->struct_bytes); i++)
            ew_byte(out, wt->struct_bytes[i]);
    }
    for (int fi = 0; fi < nf; fi++) {
        sema_func_ent_t e = sema_func_at(s, fi);
        emit_functype_for(wt, s, out, e.class_id, e.method_id, false);  /* natural: forwarders + compiled bodies */
    }
    if (nclinit)                            /* the <clinit> signature: () -> () */
        { ew_byte(out, WT_FUNC); ew_u32(out, 0); ew_u32(out, 0); }
    if (ntag) {                             /* the exception tag's functype: [ref null Throwable] -> [] */
        int thr = s->wk.throwable_id;
        ew_byte(out, WT_FUNC);
        ew_u32(out, 1);                     /* one param: the thrown Throwable ref */
        wasm_types_emit_ref(out, wasm_types_class_typeidx(wt, thr));
        ew_u32(out, 0);                     /* no results (WASM tags must be () -> ε) */
    }
    if (wt->has_iface_helper) {             /* iface_instanceof: (ref null root, i32) -> i32 */
        ew_byte(out, WT_FUNC);
        ew_u32(out, 2);
        wasm_types_emit_ref(out, wasm_types_class_typeidx(wt, wasm_root_class(wt)));
        ew_byte(out, WT_I32);
        ew_u32(out, 1);
        ew_byte(out, WT_I32);
    }
    for (int i = 0; i < nimp; i++) {        /* imports: host natives → host-ABI (externref refs → §3.3.10
                                             * linkable). PLUGIN's java.lang imports come from jre NATURAL-typed;
                                             * but a PLUGIN's USER natives are genuine host externs → host-ABI too. */
        sema_func_ent_t e = sema_import_at(s, i);
        const sema_class_t* ic = sema_get_class(s, e.class_id);
        bool jre_import = (wt->mode == SEMA_MODE_PLUGIN && ic->import_pkg >= 0 && ic->ast_node);
        emit_functype_for(wt, s, out, e.class_id, e.method_id, !jre_import);
    }
    for (int i = num_sig; i < num_arrays; i++)   /* body-local arrays: standalone, past the func types */
        emit_array_comptype(wt, out, wt->arr_elems[i]);
    /* abstract functypes: one canonical (ref root, params, result) per unoccupied abstract vtable
     * introducer, in the SAME (class, method) order abstract_functype_ord counts — trailing the
     * body arrays so no earlier type index shifts (WHOLE/RUNTIME/PLUGIN all agree). */
    for (int ci = 0; ci < wt->num_classes; ci++) {
        const sema_class_t* c = sema_get_class(s, ci);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!is_virtual_method(&c->methods[j])) continue;
            if (sema_func_index(s, ci, j) >= 0) continue;
            if (!vtable_slot_unoccupied(wt, ci, j)) continue;
            emit_functype_for(wt, s, out, ci, j, false);
        }
    }
}

/* The func typeidx of the module initializer: past structs, vtable, the signature
 * arrays, and the method func types. Valid only when has_clinit. */
int32_t wasm_clinit_functype_idx(const wasm_types_t* wt) {
    return wt->num_classes + WASM_HDR_TYPES + wt->num_sig_arrays + sema_func_count(wt->sema);
}

/* The func typeidx of the exception tag ([ref null Throwable] -> []), appended in
 * the rec group right after the <clinit> functype. Valid only when has_exceptions. */
int32_t wasm_tag_functype_idx(const wasm_types_t* wt) {
    return wasm_clinit_functype_idx(wt) + (wt->has_clinit ? 1 : 0);
}

/* The iface_instanceof helper's func typeidx: appended right after the tag functype
 * (and before the import functypes). Valid only when has_iface_helper. */
int32_t wasm_iface_helper_functype_idx(const wasm_types_t* wt) {
    return wasm_tag_functype_idx(wt) + (wt->has_exceptions ? 1 : 0);
}

/* The iface_instanceof helper's funcidx: past the import range and all defined
 * table functions and <clinit>. Valid only when has_iface_helper. */
int32_t wasm_iface_helper_funcidx(const wasm_types_t* wt) {
    return wt->nimports + sema_func_count(wt->sema) + (wt->has_clinit ? 1 : 0);
}

/* Total static fields across the program = the static-field module globals. */
int32_t wasm_global_count(const wasm_types_t* wt) {
    int n = 0;
    for (int ci = 0; ci < wt->num_classes; ci++)
        n += count_static_fields(wt->sema, ci);
    return n;
}

/* ── PLUGIN global partition. SHARED classes (library source + synthesized) are jre's — their
 * static-field + Class-singleton globals are IMPORTED, occupying [0, G_imp); user-source classes'
 * globals (static, vtable, singleton) are DEFINED after. Shared/user is the is_user_source
 * PREDICATE, NOT a class-id range: synthesized shared classes carry HIGHER ids than user classes
 * (ci order: library src, user src, synthesized), so all counts iterate + test the predicate.
 * G_imp = shared static fields + one singleton per shared class. (WHOLE/RUNTIME: none imported.) */
static bool is_user_source(const sema_ctx_t* sema, int ci);   /* STRUCT-group partition; defined above */

/* A §10.8 per-exact-type array Class (NOT a RefArray/PrimArray value overlay). jre only
 * synthesizes the array types java.lang uses, so a plugin that uses long[]/Box[]/int[][] must own
 * those array Classes' GLOBALS locally. Their STRUCT stays shared/group A — every empty
 * `sub Object{}` array-Class struct canonicalizes together, so jre's set covers them structurally. */
static bool is_array_class(const sema_ctx_t* sema, int ci) { return sema_array_class_overlay(sema, ci) >= 0; }

/* GLOBAL-layout partition: a class whose singleton/static/vtable globals are DEFINED locally
 * rather than imported from jre. = user-source OR array Class. DISTINCT from is_user_source (the
 * STRUCT-group partition), which keeps array Classes shared/group A. Overlays + library-source
 * classes are the imported (global-shared) set — jre defines & exports their globals. */
static bool is_glocal(const sema_ctx_t* sema, int ci) {
    return is_user_source(sema, ci) || is_array_class(sema, ci);
}
static bool wasm_is_imported_class(const wasm_types_t* wt, int ci) {   /* are ci's globals imported from jre? */
    return wt->mode == SEMA_MODE_PLUGIN && !is_glocal(wt->sema, ci);
}
static int shared_classes_before(const wasm_types_t* wt, int ci) {
    int n = 0; for (int cj = 0; cj < ci; cj++) if (!is_glocal(wt->sema, cj)) n++; return n;
}
static int user_classes_before(const wasm_types_t* wt, int ci) {   /* locally-defined-global classes before ci */
    int n = 0; for (int cj = 0; cj < ci; cj++) if (is_glocal(wt->sema, cj)) n++; return n;
}
static int shared_static_before(const wasm_types_t* wt, int ci) {
    int n = 0; for (int cj = 0; cj < ci; cj++) if (!is_glocal(wt->sema, cj)) n += count_static_fields(wt->sema, cj); return n;
}
static int user_static_before(const wasm_types_t* wt, int ci) {
    int n = 0; for (int cj = 0; cj < ci; cj++) if (is_glocal(wt->sema, cj)) n += count_static_fields(wt->sema, cj); return n;
}
static int wasm_lib_static_count(const wasm_types_t* wt) {   /* total imported (global-shared) static fields */
    return (wt->mode == SEMA_MODE_PLUGIN) ? shared_static_before(wt, wt->num_classes) : 0;
}
static int wasm_gshared_class_count(const wasm_types_t* wt) {   /* # classes whose globals are imported */
    return (wt->mode == SEMA_MODE_PLUGIN) ? shared_classes_before(wt, wt->num_classes) : wt->num_classes;
}
int32_t wasm_imported_global_count(const wasm_types_t* wt) {   /* G_imp = shared static + one singleton per shared class */
    return (wt->mode == SEMA_MODE_PLUGIN) ? wasm_lib_static_count(wt) + wasm_gshared_class_count(wt) : 0;
}

/* The DEFINED (module-owned) global count = the global section length. WHOLE/RUNTIME emit every
 * static field + every vtable + every singleton; PLUGIN emits only the LOCALLY-owned ones
 * (user-source + array Classes). */
int32_t wasm_total_global_count(const wasm_types_t* wt) {
    if (wt->mode == SEMA_MODE_PLUGIN) {
        int num_local = wt->num_classes - wasm_gshared_class_count(wt);
        return (wasm_global_count(wt) - wasm_lib_static_count(wt)) + 2 * num_local;   /* local static + vtable + singleton */
    }
    return wasm_global_count(wt) + 2 * wt->num_classes;
}

/* Defined globals (PLUGIN) are laid out [local static fields, local vtables, local singletons]. */
int32_t wasm_vtable_global_index(const wasm_types_t* wt, int class_id) {
    if (wt->mode == SEMA_MODE_PLUGIN)   /* locally-owned classes only (a shared class's vtable is jre's) */
        return wasm_imported_global_count(wt) + (wasm_global_count(wt) - wasm_lib_static_count(wt))
             + user_classes_before(wt, class_id);
    return wasm_global_count(wt) + class_id;
}

int32_t wasm_class_singleton_global_index(const wasm_types_t* wt, int class_id) {
    if (wt->mode == SEMA_MODE_PLUGIN) {
        if (wasm_is_imported_class(wt, class_id))                                 /* imported: [lib_static, G_imp) */
            return wasm_lib_static_count(wt) + shared_classes_before(wt, class_id);
        int num_local = wt->num_classes - wasm_gshared_class_count(wt);           /* defined: after local static + vtables */
        return wasm_imported_global_count(wt) + (wasm_global_count(wt) - wasm_lib_static_count(wt))
             + num_local + user_classes_before(wt, class_id);
    }
    return wasm_global_count(wt) + wt->num_classes + class_id;
}

/* A static field's default-value const-expr (§4.5.4): the zero/null of its type,
 * terminated by the 0x0B end byte. Declaration-site initializers run later from
 * the module start function. Raw opcode bytes (single-byte numeric/ref consts). */
static void emit_global_default(wasm_types_t* wt, emit_wasm_ctx* out, java_type_t t) {
    switch (t.tag) {
        case JT_LONG:   ew_byte(out, 0x42); ew_i64(out, 0);    break;  /* i64.const 0 */
        case JT_FLOAT:  ew_byte(out, 0x43); ew_f32(out, 0.0f); break;  /* f32.const 0 */
        case JT_DOUBLE: ew_byte(out, 0x44); ew_f64(out, 0.0);  break;  /* f64.const 0 */
        case JT_CLASS:  ew_byte(out, 0xD0); ew_i64(out, wasm_types_class_typeidx(wt, t.class_id)); break;
        case JT_ARRAY:  ew_byte(out, 0xD0); ew_i64(out, wasm_types_value_array_typeidx(wt, t)); break;
        default:        ew_byte(out, 0x41); ew_i32(out, 0);    break;  /* i32.const 0 (byte/short/int/char/bool) */
    }
    ew_byte(out, 0x0B);                                                /* end */
}

/* The global section: one mutable global per static field, in (class, field-vec)
 * order — the same dense numbering wasm_global_index produces (one authority).
 * The assembler decodes these bytes with the shared jav reader. */
/* Resolve `class_id`'s vtable into fn[0..n) (each entry a func index, -1 = none).
 * Walk the superclass chain SUPER-FIRST so a derived override overwrites its
 * super's slot (JLS §8.4.8). A virtual method's slot is found by matching its
 * signature against the precomputed slot table — same_sig is transitive, so the
 * first-occurrence canonical method identifies the slot. */
static void resolve_slots(const wasm_types_t* wt, int class_id, int n,
                          const int* slot_cls, const int* slot_mid, int32_t* fn) {
    if (class_id < 0) return;
    const sema_class_t* cls = sema_get_class(wt->sema, class_id);
    resolve_slots(wt, cls->super_id, n, slot_cls, slot_mid, fn);   /* super first */
    for (int j = 0; j < (int)bbq_vec_len(cls->methods); j++) {
        if (!is_virtual_method(&cls->methods[j])) continue;
        for (int s = 0; s < n; s++)
            if (same_sig(&cls->methods[j],
                         &sema_get_class(wt->sema, slot_cls[s])->methods[slot_mid[s]])) {
                fn[s] = wasm_func_index(wt, class_id, j);   /* -1 if abstract/host */
                break;
            }
    }
}

/* A class's vtable instance global: an immutable (ref $globalvtable) initialized
 * to array.new_fixed of N funcref slots — ref.func of the override-resolved method
 * at each slot, ref.null func where the class has no method. ref.func in a global
 * init self-declares the function in C.refs (§3.5.10), so no element segment is
 * needed. (§3.3.10 admits array.new_fixed / ref.func / ref.null as constant.) */
static void emit_vtable_global(wasm_types_t* wt, emit_wasm_ctx* out, int class_id,
                               int n, const int* slot_cls, const int* slot_mid) {
    int32_t vt = wasm_vtable_typeidx(wt, class_id);
    wasm_types_emit_ref(out, vt);           /* globaltype: (ref null $globalvtable) */
    ew_byte(out, 0x00);                      /*             mut = const */
    int32_t* fn = NULL;                       /* bbq_vec<int32_t>: slot → func index */
    for (int s = 0; s < n; s++) bbq_vec_push(fn, (int32_t)-1);
    resolve_slots(wt, class_id, n, slot_cls, slot_mid, fn);
    for (int slot = 0; slot < n; slot++) {   /* init const-expr: the N funcref slots */
        if (fn[slot] >= 0) { ew_byte(out, 0xD2); ew_u32(out, (uint32_t)fn[slot]); } /* ref.func */
        else               { ew_byte(out, 0xD0); ew_i64(out, -16); }               /* ref.null func */
    }
    bbq_vec_free(fn);
    ew_byte(out, 0xFB); ew_u32(out, 0x08);   /* array.new_fixed $globalvtable n */
    ew_u32(out, (uint32_t)vt); ew_u32(out, (uint32_t)n);
    ew_byte(out, 0x0B);                      /* end */
}

/* The class id of a class pointer (sema owns the contiguous class table). */
static int class_id_of(const sema_ctx_t* s, int n, const sema_class_t* p) {
    for (int i = 0; i < n; i++) if (sema_get_class(s, i) == p) return i;
    return -1;
}

/* A class's ClassDesc instance global: an immutable (ref null ClassDesc) initialized to
 * struct.new { i32 classId; super ClassDesc | null; (array i32) transitive interface ids;
 * the class's vtable global }. super is a global.get of the super's ClassDesc (topological
 * order guarantees it precedes); the interface ids come from sema_transitive_interfaces
 * (§6.9.2.3 — the authority), so the runtime subtype query mirrors sema_is_subclass_of. */
/* A class's Class-object singleton global (the unified runtime type). const-init the
 * value fields — name (char[] of the fq name), iface, vtable (global.get the vtable
 * global), ifaceIds ((array i32) transitive closure for instanceof) — and NULL the ref
 * fields field0/superclass/interfaces, which reflect_init sets at start (their targets
 * are other Class singletons, so const-init can't express the self/forward references).
 * Field order matches the Class struct: [0]field0 [1]name [2]superclass [3]iface
 * [4]interfaces [5]vtable [6]ifaceIds. */
static void emit_class_singleton(wasm_types_t* wt, const sema_ctx_t* s,
                                 emit_wasm_ctx* out, int ci,
                                 const sema_class_t** ifbuf, int ifcap) {
    const sema_class_t* c = sema_get_class(s, ci);
    int32_t cls_ty  = wasm_types_class_typeidx(wt, sema_class_reflect_id(s));
    int32_t ifaces_ty = wasm_types_class_typeidx(wt, sema_refarray_id(s));  /* Class[] is a ref array → RefArray */
    wasm_types_emit_ref(out, cls_ty);                        /* globaltype: (ref null Class) */
    ew_byte(out, 0x00);                                      /* const global (fields mutated by reflect_init) */
    ew_byte(out, 0xD0); ew_i64(out, cls_ty);                 /* [0] field0     = null (reflect_init sets it) */
    ew_byte(out, 0x41); ew_i32(out, 0);                      /* [1] hash        = 0 (inherited Object.hash; lazy) */
    /* [2] name = char[] → the CharArray overlay { null header (internal; reflect_init does
     * not fixup name), hash 0, char-array backing }. A raw (array char) would mismatch the
     * now-overlaid field type. Header is null since const-init can't forward-ref CharArray's
     * own Class singleton (a later global). */
    const char* nm = c->fq_name ? c->fq_name : c->name;
    int nl = (int)strlen(nm);
    int32_t chararr_ty = wasm_types_class_typeidx(wt, sema_primarray_id(s, 2));  /* char = storage index 2 */
    ew_byte(out, 0xD0); ew_i64(out, cls_ty);                 /* CharArray [0] header = null */
    ew_byte(out, 0x41); ew_i32(out, 0);                      /* CharArray [1] hash = 0 */
    for (int k = 0; k < nl; k++) { ew_byte(out, 0x41); ew_i32(out, (int32_t)(unsigned char)nm[k]); }
    ew_byte(out, 0xFB); ew_u32(out, 0x08);
    ew_u32(out, (uint32_t)wasm_types_array_typeidx(wt, jt_prim(JT_CHAR))); ew_u32(out, (uint32_t)nl);  /* the char backing */
    ew_byte(out, 0xFB); ew_u32(out, 0x00); ew_u32(out, (uint32_t)chararr_ty);  /* struct.new CharArray */
    ew_byte(out, 0xD0); ew_i64(out, cls_ty);                 /* [2] superclass  = null (reflect_init) */
    ew_byte(out, 0x41); ew_i32(out, c->is_interface ? 1 : 0);/* [3] iface */
    ew_byte(out, 0xD0); ew_i64(out, ifaces_ty);              /* [4] interfaces  = null RefArray (reflect_init) */
    ew_byte(out, 0xD0); ew_i64(out, cls_ty);                 /* [5] componentType = null (reflect_init sets it for array Classes) */
    ew_byte(out, 0xD0); ew_i64(out, cls_ty);                 /* [6] next = null (reflect_init links the §20.3.8 registry chain) */
    ew_byte(out, 0x23); ew_u32(out, (uint32_t)wasm_vtable_global_index(wt, ci)); /* [7] vtable */
    int nif = sema_transitive_interfaces(s, c, ifbuf, ifcap);/* [6] ifaceIds    = transitive interface ids */
    for (int i = 0; i < nif; i++) { ew_byte(out, 0x41); ew_i32(out, class_id_of(s, wt->num_classes, ifbuf[i])); }
    ew_byte(out, 0xFB); ew_u32(out, 0x08);
    ew_u32(out, (uint32_t)wasm_iface_array_typeidx(wt)); ew_u32(out, (uint32_t)nif);
    /* factory: `ref.func $newInstance` for an instantiable class, else `ref.null func`. The field is
     * a plain funcref, so any function reference is assignable; ClassConstruct ref.casts it to the
     * factory functype before call_ref, exactly as the vtable dispatch does. (ref.func in a global
     * init self-declares the function in C.refs, §3.5.10 — no element segment.) */
    int factory_midx = -1;
    for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
        if (c->methods[mi].is_synthetic_new_instance) { factory_midx = mi; break; }
    if (factory_midx >= 0) { ew_byte(out, 0xD2); ew_u32(out, (uint32_t)wasm_func_index(wt, ci, factory_midx)); }
    else                   { ew_byte(out, 0xD0); ew_i64(out, -16); }   /* ref.null func */
    ew_byte(out, 0xFB); ew_u32(out, 0x00); ew_u32(out, (uint32_t)cls_ty);  /* struct.new Class */
    ew_byte(out, 0x0B);                                      /* end */
}

void wasm_types_emit_globals_content(wasm_types_t* wt, const sema_ctx_t* s,
                                     emit_wasm_ctx* out) {
    /* PLUGIN emits only USER-source globals; SHARED classes' static fields + singletons live in
     * jre.wasm (imported), their vtables reached via the imported singletons. Shared/user is the
     * is_user_source predicate (NOT a ci range — synthesized shared classes carry higher ids than
     * user ones). WHOLE/RUNTIME: every class emitted. Order MUST match the global-index
     * authorities: user static fields, then user vtables, then user singletons. */
    ew_u32(out, (uint32_t)wasm_total_global_count(wt));
    for (int ci = 0; ci < wt->num_classes; ci++) {   /* static-field globals first */
        if (wasm_is_imported_class(wt, ci)) continue;
        const sema_class_t* c = sema_get_class(s, ci);
        for (int i = 0; i < (int)bbq_vec_len(c->fields); i++) {
            if (!(c->fields[i].modifiers & ACC_STATIC)) continue;
            java_type_t t = c->fields[i].type;
            wasm_types_emit_valtype(wt, out, t);   /* globaltype: valtype */
            ew_byte(out, 0x01);                     /*             mut = var */
            emit_global_default(wt, out, t);        /* init const-expr + end */
        }
    }
    int n = wasm_vtable_len(wt);                      /* the slot table — built ONCE */
    int* slot_cls = NULL;                             /* bbq_vec<int> */
    int* slot_mid = NULL;                             /* bbq_vec<int> */
    build_vtable_slots(wt, &slot_cls, &slot_mid);
    for (int ci = 0; ci < wt->num_classes; ci++)     /* then one vtable per user class */
        if (!wasm_is_imported_class(wt, ci)) emit_vtable_global(wt, out, ci, n, slot_cls, slot_mid);
    bbq_vec_free(slot_cls); bbq_vec_free(slot_mid);
    /* then one Class-object singleton per user class, in class id order (const-init has no
     * cross-Class refs — the ref fields are null-then-fixed by reflect_init). */
    const sema_class_t** ifbuf =
        (const sema_class_t**)bbq_arena_alloc(wt->sema->arena, sizeof(*ifbuf) * (size_t)wt->num_classes);
    for (int ci = 0; ci < wt->num_classes; ci++)
        if (!wasm_is_imported_class(wt, ci)) emit_class_singleton(wt, s, out, ci, ifbuf, wt->num_classes);
}

/* The default value of a struct DATA field as a body opcode sequence (mirrors
 * emit_global_default's const-expr, but via the WOP_* emit path for a body). */
static void emit_field_default(wasm_types_t* wt, emit_wasm_ctx* e, java_type_t t) {
    switch (t.tag) {
        case JT_LONG:   ew_emit(e, WOP_I64_CONST); ew_i64(e, 0);    break;
        case JT_FLOAT:  ew_emit(e, WOP_F32_CONST); ew_f32(e, 0.0f); break;
        case JT_DOUBLE: ew_emit(e, WOP_F64_CONST); ew_f64(e, 0.0);  break;
        case JT_CLASS:  ew_emit(e, WOP_REF_NULL);  ew_i64(e, wasm_types_class_typeidx(wt, lat_value_class(wt->sema, t.class_id))); break;  /* interface → root, matching the field decl (emit_valtype) */
        case JT_ARRAY:  ew_emit(e, WOP_REF_NULL);  ew_i64(e, wasm_types_value_array_typeidx(wt, t)); break;
        default:        ew_emit(e, WOP_I32_CONST); ew_i32(e, 0);    break;  /* byte/short/int/char/bool */
    }
}

/* Push each DATA field's default in struct-field order (inherited base-first,
 * then own) — the operands struct.new consumes after the vtable. Mirrors
 * emit_instance_fields' walk, skipping the root's vtable header (set separately). */
static void emit_field_defaults(wasm_types_t* wt, emit_wasm_ctx* e, int class_id) {
    if (class_id < 0) return;
    const sema_class_t* cls = sema_get_class(wt->sema, class_id);
    if (cls->super_id >= 0) emit_field_defaults(wt, e, cls->super_id);   /* root: vtable only */
    for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++) {
        if (cls->fields[i].modifiers & ACC_STATIC) continue;
        emit_field_default(wt, e, cls->fields[i].type);   /* overlays' raw `data` → ref.null concrete via the lattice */
    }
    /* Class's synthesized trailing fields have no java_type_t, so they are not in `fields` — but
     * struct.new needs an operand for every field. `new Class()` is the only way to reach this
     * (a Class singleton is const-initialized, not `new`ed), and it is reachable: java.lang.Class
     * is an ordinary instantiable class. All three are nullable, hence defaultable. */
    if (class_id == sema_class_reflect_id(wt->sema)) {
        ew_byte(e, 0xD0); ew_i64(e, wasm_vtable_typeidx(wt, 0));    /* vtable   = ref.null $vtable  */
        ew_byte(e, 0xD0); ew_i64(e, wasm_iface_array_typeidx(wt));  /* ifaceIds = ref.null $ifaceIds */
        ew_byte(e, 0xD0); ew_i64(e, -16);                            /* factory  = ref.null func     */
    }
}

/* The reflection bootstrap fixup (emitted into <clinit>, which runs at module start):
 * set each Class singleton's REF fields — the self/mutual references a const-init can't
 * express. field 0 = Class.class (Class.class's own field 0 is itself); superclass
 * (field 2, §20.3.4) = the super's Class or null; interfaces (field 4, §20.3.5) = a
 * Class[] of the DIRECT interfaces. Field indices track java.lang.Class's declaration
 * order (header, name, superclass, iface, interfaces, +synth). Instructions only. */
void wasm_types_emit_reflect_fixup(wasm_types_t* wt, const sema_ctx_t* s, emit_wasm_ctx* out) {
    int32_t cls_ty  = wasm_types_class_typeidx(wt, sema_class_reflect_id(s));
    int32_t ra_ty   = wasm_types_class_typeidx(wt, sema_refarray_id(s));               /* Class[] → RefArray */
    int32_t ra_glob = wasm_class_singleton_global_index(wt, sema_refarray_id(s));      /* RefArray's Class singleton */
    int32_t backing = wasm_types_array_for_dt(wt, SIR_DTREF);                          /* RefArray.data (anyref[]) */
    int     cc      = sema_class_reflect_id(s);
    /* Class's own fields (past the inherited header + Object fields), in declaration
     * order: name [+0], superclass [+1], iface [+2], interfaces [+3]. field_base keeps
     * this robust if the header/Object fields ever change. */
    int32_t fb          = wasm_types_field_base(wt, cc);
    uint32_t f_super    = (uint32_t)(fb + 1);
    uint32_t f_ifaces   = (uint32_t)(fb + 3);
    uint32_t f_component = (uint32_t)(fb + 4);
    uint32_t f_next     = (uint32_t)(fb + 5);
    /* Class.registry — the §20.3.8 chain head, a static field, so a module global. Found by name
     * because it is one of Class's own declared members, like `name` and `superclass` above. */
    int32_t f_registry = -1;
    {
        const sema_class_t* cls = sema_get_class(s, cc);
        for (int i = 0; i < (int)bbq_vec_len(cls->fields); i++)
            if ((cls->fields[i].modifiers & ACC_STATIC) && cls->fields[i].name
                && strcmp(cls->fields[i].name, "registry") == 0) {
                f_registry = wasm_global_index(wt, cc, i);
                break;
            }
    }
    /* PLUGIN fixes up only USER-source Class singletons; SHARED (library + synthesized) singletons
     * were already wired by jre's start (imported here, read-only). A user singleton's superclass/
     * interface/componentType global.gets resolve to imported shared singletons or local user ones. */
    for (int ci = 0; ci < wt->num_classes; ci++) {
        if (wasm_is_imported_class(wt, ci)) continue;
        const sema_class_t* c = sema_get_class(s, ci);
        int32_t self = wasm_class_singleton_global_index(wt, ci);
        /* field 0 = Class.class */
        ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);
        ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)wasm_class_singleton_global_index(wt, cc));
        ew_emit(out, WOP_STRUCT_SET); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, 0);
        /* superclass = super's Class | null */
        ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);
        if (c->super_id >= 0) { ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)wasm_class_singleton_global_index(wt, c->super_id)); }
        else                  { ew_byte(out, 0xD0); ew_i64(out, cls_ty); }              /* ref.null Class */
        ew_emit(out, WOP_STRUCT_SET); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, f_super);
        /* interfaces = a RefArray (Class[] is a reference array §10.2) wrapping the direct
         * interfaces' Class singletons: struct.new RefArray{ header, elementClass=null,
         * data = array.new_fixed backing<Class singletons> }. The Class refs widen to the
         * anyref backing. */
        ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);          /* struct.set receiver */
        ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)ra_glob);       /* RefArray [0] Class header */
        ew_byte(out, 0x41); ew_i32(out, 0);                                /* RefArray [1] hash (inherited Object.hash) = 0 */
        ew_byte(out, 0xD0); ew_i64(out, cls_ty);                           /* RefArray [2] elementClass = null */
        for (int k = 0; k < c->interface_count; k++) {
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)wasm_class_singleton_global_index(wt, c->interface_ids[k]));
        }
        ew_emit(out, WOP_ARRAY_NEW_FIXED); ew_u32(out, (uint32_t)backing); ew_u32(out, (uint32_t)c->interface_count);  /* [2] data */
        ew_emit(out, WOP_STRUCT_NEW); ew_u32(out, (uint32_t)ra_ty);         /* → the interfaces RefArray */
        ew_emit(out, WOP_STRUCT_SET); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, f_ifaces);
        /* §10.2 componentType: an array Class points at its component's Class (the covariance
         * link assignableFrom recurses on); non-arrays / primitive-component arrays leave null. */
        int comp = sema_array_component_class(s, ci);
        if (comp >= 0) {
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)wasm_class_singleton_global_index(wt, comp));
            ew_emit(out, WOP_STRUCT_SET); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, f_component);
        }
        /* §20.3.8 registry: link this singleton onto the head of the chain —
         *     this.next = Class.registry;  Class.registry = this;
         * A plugin's bootstrap runs after jre's and prepends onto the chain it imports, so
         * `forName` walking from the head sees the plugin's classes and then the library's. */
        if (f_registry >= 0) {
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)f_registry);
            ew_emit(out, WOP_STRUCT_SET); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, f_next);
            ew_emit(out, WOP_GLOBAL_GET); ew_u32(out, (uint32_t)self);
            ew_emit(out, WOP_GLOBAL_SET); ew_u32(out, (uint32_t)f_registry);
        }
    }
}

/* §20.1.5 the shallow copy of `this` (local 0) as class_id: struct.new class_id with every
 * field read from `this`. The receiver is root-typed (Object) per emit_functype_for, so each
 * read casts it to the concrete struct first (redundant casts, no scratch local needed). Field
 * 0 (the Class header) copies too, so the clone reports the same runtime class. */
void wasm_types_emit_clone_copy(wasm_types_t* wt, emit_wasm_ctx* e, int class_id) {
    int32_t tyidx = wasm_types_class_typeidx(wt, class_id);
    int nf = count_instance_fields(wt->sema, class_id);
    for (int i = 0; i < nf; i++) {
        ew_emit(e, WOP_LOCAL_GET); ew_u32(e, 0);          /* this (ref null Object) */
        ew_emit(e, WOP_REF_CAST);  ew_i32(e, tyidx);      /* → (ref class_id) */
        ew_emit(e, WOP_STRUCT_GET); ew_u32(e, (uint32_t)tyidx); ew_u32(e, (uint32_t)i);
    }
    ew_emit(e, WOP_STRUCT_NEW); ew_u32(e, (uint32_t)tyidx);
}

void wasm_types_emit_new(wasm_types_t* wt, emit_wasm_ctx* e, int class_id) {
    ew_emit(e, WOP_GLOBAL_GET);                          /* field 0 = the class's Class singleton */
    ew_u32(e, (uint32_t)wasm_class_singleton_global_index(wt, class_id));
    emit_field_defaults(wt, e, class_id);                /* data fields = defaults */
    ew_emit(e, WOP_STRUCT_NEW);
    ew_u32(e, (uint32_t)wasm_types_class_typeidx(wt, class_id));
}

/* The synthesized iface_instanceof helper's complete func body (locals vec + code +
 * end). It is `(param $obj (ref null root)) (param $target i32) (result i32)`: returns
 * 1 iff the object's runtime class implements the target interface, else 0 — a single
 * scan of obj.ClassDesc.interfaces (the full transitive closure, so no super-chain
 * walk). Locals: 2 = $ifaces (ref null $ifaces), 3 = $n, 4 = $i, 5 = $found (all i32
 * except $ifaces). Control flows via those locals, so every block/loop/if is ()->(). */
void wasm_types_emit_iface_helper(wasm_types_t* wt, emit_wasm_ctx* out) {
    int32_t root_ty = wasm_types_class_typeidx(wt, wasm_root_class(wt));
    int32_t cls_ty  = wasm_class_reflect_typeidx(wt);            /* field 0 is a (ref null Class) */
    int32_t ifidx   = wasm_class_ifaceids_field_index(wt);       /* Class.ifaceIds field */
    int32_t if_ty   = wasm_iface_array_typeidx(wt);
    /* locals vec: 1×(ref null $ifaces), then 3×i32 */
    ew_u32(out, 2);
    ew_u32(out, 1); wasm_types_emit_ref(out, if_ty);
    ew_u32(out, 3); ew_byte(out, WT_I32);
    ew_byte(out, 0x41); ew_i32(out, 0); ew_byte(out, 0x21); ew_u32(out, 5);   /* $found = 0 */
    ew_byte(out, 0x02); ew_byte(out, 0x40);                                   /* block $skip */
      ew_byte(out, 0x20); ew_u32(out, 0);                                     /*   local.get $obj */
      ew_byte(out, 0xD1);                                                     /*   ref.is_null */
      ew_byte(out, 0x0D); ew_u32(out, 0);                                     /*   br_if $skip (obj null → found 0) */
      ew_byte(out, 0x20); ew_u32(out, 0);                                     /*   local.get $obj */
      ew_byte(out, 0xFB); ew_u32(out, 0x02); ew_u32(out, (uint32_t)root_ty); ew_u32(out, 0);  /* struct.get root .Class (field0) */
      ew_byte(out, 0xFB); ew_u32(out, 0x02); ew_u32(out, (uint32_t)cls_ty); ew_u32(out, (uint32_t)ifidx);  /* struct.get Class .ifaceIds */
      ew_byte(out, 0x21); ew_u32(out, 2);                                     /*   local.set $ifaces */
      ew_byte(out, 0x20); ew_u32(out, 2); ew_byte(out, 0xFB); ew_u32(out, 0x0F); /* array.len */
      ew_byte(out, 0x21); ew_u32(out, 3);                                     /*   local.set $n */
      ew_byte(out, 0x41); ew_i32(out, 0); ew_byte(out, 0x21); ew_u32(out, 4); /*   $i = 0 */
      ew_byte(out, 0x02); ew_byte(out, 0x40);                                 /*   block $done */
        ew_byte(out, 0x03); ew_byte(out, 0x40);                               /*     loop $L */
          ew_byte(out, 0x20); ew_u32(out, 4); ew_byte(out, 0x20); ew_u32(out, 3); /* $i, $n */
          ew_byte(out, 0x4E);                                                 /*       i32.ge_s */
          ew_byte(out, 0x0D); ew_u32(out, 1);                                 /*       br_if $done ($i >= $n) */
          ew_byte(out, 0x20); ew_u32(out, 2); ew_byte(out, 0x20); ew_u32(out, 4); /* $ifaces, $i */
          ew_byte(out, 0xFB); ew_u32(out, 0x0B); ew_u32(out, (uint32_t)if_ty); /*       array.get $ifaces */
          ew_byte(out, 0x20); ew_u32(out, 1);                                 /*       local.get $target */
          ew_byte(out, 0x46);                                                 /*       i32.eq */
          ew_byte(out, 0x04); ew_byte(out, 0x40);                             /*       if (match) */
            ew_byte(out, 0x41); ew_i32(out, 1); ew_byte(out, 0x21); ew_u32(out, 5); /* $found = 1 */
            ew_byte(out, 0x0C); ew_u32(out, 2);                               /*         br $done */
          ew_byte(out, 0x0B);                                                 /*       end if */
          ew_byte(out, 0x20); ew_u32(out, 4); ew_byte(out, 0x41); ew_i32(out, 1); ew_byte(out, 0x6A); /* $i + 1 */
          ew_byte(out, 0x21); ew_u32(out, 4);                                 /*       local.set $i */
          ew_byte(out, 0x0C); ew_u32(out, 0);                                 /*       br $L */
        ew_byte(out, 0x0B);                                                   /*     end loop */
      ew_byte(out, 0x0B);                                                     /*   end block $done */
    ew_byte(out, 0x0B);                                                       /* end block $skip */
    ew_byte(out, 0x20); ew_u32(out, 5);                                       /* local.get $found */
    ew_byte(out, 0x0B);                                                       /* end (function) */
}

void wasm_types_emit_section(wasm_types_t* wt, emit_wasm_ctx* out) {
    emit_wasm_ctx content = {0};
    wasm_types_emit_typesec_content(wt, wt->sema, &content);
    ew_byte(out, 0x01);                     /* section id 1 */
    ew_u32(out, (uint32_t)bbq_vec_len(content.code));
    for (int i = 0; i < (int)bbq_vec_len(content.code); i++)
        ew_byte(out, content.code[i]);
    bbq_vec_free(content.code);
}

void wasm_types_free(wasm_types_t* wt) {
    bbq_vec_free(wt->struct_bytes);
    bbq_vec_free(wt->arr_elems);
    bbq_vec_free(wt->class_pos);
    wt->struct_bytes = NULL;
    wt->arr_elems = NULL;
    wt->class_pos = NULL;
}
