/* type_lattice.h — hash-consed Type lattice for click's dataflow.
 *
 * Pool-allocated Types are interned: structural equality is pointer
 * equality. A single canonical Type per (kind, fields...) tuple lives
 * in the pool; type_make_* returns the existing pointer when the
 * structural fields match.
 *
 * Partial-order convention:
 *   TK_TOP    is the MINIMUM — meet identity (optimistic "unvisited")
 *   TK_BOTTOM is the MAXIMUM — meet absorbing (over-constrained)
 *   For TK_REF:   child class ⊑ parent class
 *   For TK_ARRAY: same dim required; dim mismatch meets to TK_BOTTOM
 *                 (there is no name for an Object-of-arrays-of-
 *                  arbitrary-rank that could subsume both).
 *
 * The meet-semilattice axioms — idempotent, commutative,
 * associative, TOP identity, BOTTOM absorbing (Click thesis
 * §3.2.1) — are the lattice contract; test/unit/test_lattice.c
 * pins them. */

#ifndef YOCTOJC_COMPILER_TYPE_LATTICE_H
#define YOCTOJC_COMPILER_TYPE_LATTICE_H

#include "bbq_arena.h"
#include "gen/sir_ast.h"
#include "javelina/compiler/sema.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TK_TOP    = 0,   /* meet identity, partial-order minimum */
    TK_BOTTOM = 1,   /* meet absorbing, partial-order maximum */
    TK_PRIM   = 2,   /* primitive value of given width */
    TK_REF    = 3,   /* scalar reference of a class */
    TK_ARRAY  = 4,   /* array of given dim containing a class element */
    TK_NULL   = 5,   /* the null reference — subtype of every
                        REF and ARRAY; meets to whichever specific
                        reference / array type it joins with. JLS
                        defines null as assignment-compatible with
                        any reference type. */
    TK_PRIM_ARRAY = 6, /* array of a primitive element width, of a
                          given dim (`[I` = dim 1, `[[I` = dim 2, …).
                          Widths are siblings — distinct widths (or
                          dims) meet to BOTTOM, like PRIM/ARRAY. */
} type_kind_t;

typedef struct {
    sir_datatype_t width;   /* the full Java 1.0 width set (byte…double) */
} PrimLat;

typedef struct {
    int32_t class_id;
} RefLat;

typedef struct {
    int32_t dim;
    int32_t class_id;   /* element class */
} ArrayLat;

typedef struct {
    int32_t        dim;    /* array rank — int[] is dim 1, int[][] dim 2 */
    sir_datatype_t width;  /* base element width (full Java 1.0 set) */
} PrimArrayLat;

typedef struct Type {
    type_kind_t kind;
    union {
        PrimLat      prim;        /* valid iff kind == TK_PRIM */
        RefLat       ref;         /* valid iff kind == TK_REF */
        ArrayLat     array;       /* valid iff kind == TK_ARRAY */
        PrimArrayLat prim_array;  /* valid iff kind == TK_PRIM_ARRAY */
    };
} Type;

/* Intern pool. Owns no memory of its own — all Types are allocated
 * from `arena`. The pool lives as long as the arena; freeing the
 * arena invalidates every Type the pool issued.
 *
 * Intern table is a bbq_vec + linear scan. The methods we compile are
 * small (unique Types per method typically 10–50 even with all planned
 * sub-lattice extensions), so the O(N) lookup is cache-friendly and
 * beats bbq_htree's structural-hash + collision-walk overhead at
 * this scale. If profiling later shows otherwise, the swap is local
 * to type_make_*() in type_lattice.c. */
typedef struct {
    bbq_arena* arena;
    Type**     interned;   /* bbq_vec of every interned Type* */
    Type*      top;        /* canonical singletons, allocated in init */
    Type*      bottom;
    Type*      null_;      /* the null reference */
} type_pool_t;

void type_pool_init(type_pool_t* pool, bbq_arena* arena);
void type_pool_destroy(type_pool_t* pool);

const Type* type_top(const type_pool_t* pool);
const Type* type_bottom(const type_pool_t* pool);
const Type* type_null(const type_pool_t* pool);

/* Hash-consed constructors. Two calls with the same arguments
 * return the same pointer (structural-equality = pointer-equality). */
const Type* type_make_prim(type_pool_t* pool, sir_datatype_t width);
const Type* type_make_ref(type_pool_t* pool, int32_t class_id);
const Type* type_make_array(type_pool_t* pool, int32_t dim, int32_t class_id);
const Type* type_make_prim_array(type_pool_t* pool, int32_t dim,
                                  sir_datatype_t width);

/* Control-flow merge (LUB in lattice terms; named "meet" per
 * dataflow tradition). Mirrors sir_opt_type_meet's semantics on
 * the int32_t encoding. */
const Type* type_meet(const sema_ctx_t* sema, const Type* a, const Type* b,
                       type_pool_t* pool);

/* Partial order: true iff `low` ⊑ `high`. TOP is the minimum;
 * BOTTOM is the maximum; child class ⊑ parent class. */
bool type_leq(const sema_ctx_t* sema, const Type* low, const Type* high);

/* ── JLS conversion authority (§5.1.2/§5.1.3/§5.6) ──────────────────────────
 * The single source of truth for type→dt and primitive conversions over the
 * Java/SIR domain (the SIR stays Java-typed until burg, so the whole frontend +
 * optimizer share this). sema, the ddcg cast lowering, and the module assembler
 * all call these — no second copy. test_lattice pins the tables. */

/* Reference-type REPRESENTATION authority (the GC object model). The root class
 * (java.lang.Object) is the unique non-interface class with no superclass — every
 * object's struct transitively extends it, so it is the common supertype for
 * vtable receivers and interface-receiver casts. lat_value_class gives the class a
 * VALUE of a reference type is represented as: an interface collapses to the root
 * (no interface object is instantiated — an interface value is just an object),
 * every other class is itself. The one map for value reftypes; a consumer turns it
 * into a concrete struct typeidx (wasm_types_class_typeidx), never re-deciding. */
int32_t lat_root_class(const sema_ctx_t* sema);
int32_t lat_value_class(const sema_ctx_t* sema, int32_t class_id);

/* A HANDLER's LANDING class — the class the caught reference is represented as where it
 * lands: the try_table block's result type, and the catch variable's slot. A typed catch
 * lands as its DECLARED class. The region's catch-all has no declared type — `sir.asdl`
 * says `catch_class_id` = -1 means "no type info (finally / throwable-catch)" — and it
 * lands as Throwable, since JLS §11.3's propagation path carries an arbitrary throwable.
 *
 * This is a REPRESENTATION question, so it is answered HERE and asked by every consumer
 * (the structured emit's block result, the assembler's slot typing). It exists because the
 * DDCG used to answer it by writing Throwable into the SEMANTIC field, which erased the
 * -1 that says "this handler catches nothing" — the one thing §6's escape rule needs. */
int32_t lat_handler_landing_class(const sema_ctx_t* sema, int32_t catch_class_id);

/* §10.2 array REPRESENTATION authority (same shape as lat_value_class). Every
 * REFERENCE array — element is a class or a nested array — is represented by the ONE
 * synthesized RefArray struct, so String[] and Object[] are the same WASM type and
 * covariance is an identity assignment. A PRIMITIVE array (and RefArray's own backing,
 * whose element is the top reference = JT_NULL) stays a concrete, invariant array.
 * lat_array_elem_is_ref is the collapse predicate; lat_refarray_class is the class a
 * consumer (wasm_types emit / burg) turns into a struct typeidx — it never re-decides. */
bool    lat_array_elem_is_ref(java_type_t elem);
int32_t lat_refarray_class(const sema_ctx_t* sema);
int     lat_prim_storage_index(sir_datatype_t dt);              /* 0..6 = the WASM backing width */
int32_t lat_primarray_class(const sema_ctx_t* sema, sir_datatype_t dt);  /* the per-width PrimArray overlay */
/* The overlay class an array-typed value `arr` is represented by (RefArray for a reference
 * element, per-width PrimArray for a primitive element), or -1 when `arr` is the concrete
 * backing of an overlay (a JT_ARRAY_RAW-marked array, or a JT_NULL element). The single
 * authority; consumers turn it into a struct/array typeidx and never re-decide. */
int32_t lat_array_overlay_class(const sema_ctx_t* sema, java_type_t arr);

/* Is (class_id, field_idx) the array overlay's BACKING-STORE field — RefArray.data or a
 * PrimArray.data? That cell is IMMUTABLE: the overlay writes it once, at allocation, and
 * no Java program can name it (§10.7 gives an array only `length`, which is final). So a
 * read of it is a pure function of the array reference: two reads of `a.data` are the same
 * value however much unrelated memory was written in between. The optimizer is the
 * consumer — it keeps the memory edge for points-to, but leaves it out of value identity. */
bool    lat_is_array_data_cell(const sema_ctx_t* sema, int32_t class_id, int field_idx);

/* SIR datatype → lowered WASM valtype class. THE one map — the optimizer's
 * slot pools, its substitution/coalescing gates, the locals-vec emitter and
 * the burg's i32-family test all read this; none re-decides. byte/short/
 * char/int share i32; long/float/double/ref each stand alone. */
typedef enum {
    LAT_VT_I32 = 0, LAT_VT_I64, LAT_VT_F32, LAT_VT_F64, LAT_VT_REF
} lat_valtype_t;
lat_valtype_t lat_dt_valtype(sir_datatype_t dt);

/* JLS type tag (JT_*) → SIR datatype. Complete across Java 1.0. The one map;
 * replaces sema's type_tag_to_dt and compiler.c's jt_data_type. */
sir_datatype_t lat_tag_to_dt(int32_t jt_tag);
/* JLS type tag → SIR array-element type (atype). The one map; replaces the
 * duplicated JT_*→SIR_AT* switches in compiler_helpers. */
sir_atype_t    lat_tag_to_atype(int32_t jt_tag);
/* SIR array-element type (atype) → element width — the inverse of
 * lat_tag_to_atype over the primitive kinds (boolean packs as byte; no
 * separate bool storage width). SIR_DTREF for ATCLASS/ATREFARRAY. */
sir_datatype_t lat_atype_to_dt(sir_atype_t atype);

/* §5.1.2 widening order rank (byte<short=char<int<long<float<double); 0 = not
 * a numeric type. char ranks with short but is a disjoint chain (see _prim). */
int  lat_widen_rank(java_type_tag_t t);
/* §5.1.2 widening primitive conversion: is `from` widened to `to` implicitly? */
bool lat_is_widening_prim(java_type_t from, java_type_t to);
/* §5.1.3 narrowing primitive conversion: numeric→numeric, not widening, not id. */
bool lat_is_narrowing_prim(java_type_t from, java_type_t to);
/* §5.6.2 binary numeric promotion: the common type tag of two numeric operands. */
java_type_tag_t lat_promote(java_type_t a, java_type_t b);

/* §5.6.2 binary / §5.6.1 unary numeric promotion at the SIR datatype level — the
 * computation type a binop's operands are converted to before the operation.
 * `lat_promote_dt` is the common type of two operands (refs pass through, never
 * promoted); `lat_unary_promote_dt` widens byte/short/char to int. */
sir_datatype_t lat_promote_dt(sir_datatype_t a, sir_datatype_t b);
sir_datatype_t lat_unary_promote_dt(sir_datatype_t a);

/* The single (from_dt → to_dt) primitive-conversion decision — the realization
 * every consumer translates from (no consumer re-decides). Covers widening AND
 * narrowing; CONV_IDENTITY = same valtype-space, no node (e.g. byte→int);
 * CONV_NONE = not a numeric conversion (ref/array/identity-of-ref). The variants
 * mirror the SIR conversion nodes (composites = via-int two-node trees). */
typedef enum {
    LAT_CONV_NONE = 0, LAT_CONV_IDENTITY,
    LAT_CONV_I2L, LAT_CONV_I2F, LAT_CONV_I2D,
    LAT_CONV_L2I, LAT_CONV_L2F, LAT_CONV_L2D,
    LAT_CONV_F2I, LAT_CONV_F2L, LAT_CONV_F2D,
    LAT_CONV_D2I, LAT_CONV_D2L, LAT_CONV_D2F,
    LAT_CONV_I2B, LAT_CONV_I2S, LAT_CONV_I2C, LAT_CONV_S2I, LAT_CONV_S2B,
    LAT_CONV_L2B, LAT_CONV_L2S, LAT_CONV_L2C,
    LAT_CONV_F2B, LAT_CONV_F2S, LAT_CONV_F2C,
    LAT_CONV_D2B, LAT_CONV_D2S, LAT_CONV_D2C,
} lat_conv_t;

lat_conv_t lat_num_conv(sir_datatype_t from, sir_datatype_t to);

#endif /* YOCTOJC_COMPILER_TYPE_LATTICE_H */
