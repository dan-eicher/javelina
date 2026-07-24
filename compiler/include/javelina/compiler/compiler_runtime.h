/* compiler_runtime.h — dest-type definitions consumed by ddcgc.
 *
 * ddcgc references delta_t / gamma_t / rho_t / label_t by name but
 * doesn't generate the struct/enum definitions. They live here.
 * Tag constants are <DEST>_<VARIANT> uppercase; constructors are
 * lowercase variant names so DSL rule bodies can write `effect()`.
 */
#ifndef JAVELINA_COMPILER_RUNTIME_H
#define JAVELINA_COMPILER_RUNTIME_H

#include "gen/sir_ast.h"     /* sir_node_t, sir_datatype_t */
#include "gen/java_ast.h"    /* ast_stmt_t, ast_expr_t */
#include "javelina/compiler/sema.h"  /* sema_ctx_t (referenced in MEMBERS) */

/* asdl emits type aliases as textual substitution; ddcgc-generated
 * code references alias names as C types, so typedef them. */
typedef const char* ident;
typedef float  float32;
typedef double float64;

/* SIR labels ARE sir_node_t pointers (typically Nops in the chain). */
typedef sir_node_t* label_t;


/* Forward decl so dest constructors can take ctx as first arg
 * (ddcgc convention — see chained-rho-pangolin Phase 1). */
struct ddcg_ctx;

/* δ ∈ Data Destination: effect | loc(slot, dt) | locref(slot, ref). A primitive
 * slot carries its width (dt); a reference slot carries its referent descriptor
 * (ref = ClassRef/ArrayRef/PrimArray) so the StoreLocal states the slot's type. */
typedef enum { DELTA_EFFECT, DELTA_LOC, DELTA_LOCREF } delta_tag_t;
typedef struct {
    delta_tag_t tag;
    int32_t slot;             /* DELTA_LOC / DELTA_LOCREF: target slot */
    sir_datatype_t dt;        /* DELTA_LOC: SIR data type tag */
    sir_node_t* ref;          /* DELTA_LOCREF: reference-type descriptor */
} delta_t;
static inline delta_t effect(struct ddcg_ctx* ctx) {
    (void)ctx; delta_t d = { DELTA_EFFECT, 0, 0, NULL }; return d;
}
static inline delta_t loc(struct ddcg_ctx* ctx, int32_t slot, sir_datatype_t dt) {
    (void)ctx; delta_t d = { DELTA_LOC, slot, dt, NULL }; return d;
}
static inline delta_t locref(struct ddcg_ctx* ctx, int32_t slot, sir_node_t* ref) {
    (void)ctx; delta_t d = { DELTA_LOCREF, slot, SIR_DTREF, ref }; return d;
}

/* γ ∈ Control Destination: single(L) | pair(Lt, Lf) | ret. */
typedef enum { GAMMA_SINGLE, GAMMA_PAIR, GAMMA_RET } gamma_tag_t;
typedef struct {
    gamma_tag_t tag;
    sir_node_t* L;            /* GAMMA_SINGLE */
    sir_node_t* Lt;           /* GAMMA_PAIR true target */
    sir_node_t* Lf;           /* GAMMA_PAIR false target */
} gamma_t;
static inline gamma_t single(struct ddcg_ctx* ctx, sir_node_t* L) {
    (void)ctx; gamma_t g = { GAMMA_SINGLE, L, NULL, NULL }; return g;
}
static inline gamma_t pair(struct ddcg_ctx* ctx, sir_node_t* Lt, sir_node_t* Lf) {
    (void)ctx; gamma_t g = { GAMMA_PAIR, NULL, Lt, Lf }; return g;
}
static inline gamma_t ret(struct ddcg_ctx* ctx) {
    (void)ctx; gamma_t g = { GAMMA_RET, NULL, NULL, NULL }; return g;
}

/* ρ ∈ Env Destination — recursive variant chain encoding lexical
 * scope. Each scope-introducing rule (loop, labeled block, future
 * try/finally) wraps body's ρ with a fresh frame variant; break/
 * continue walk the chain to find their target. The recursion's
 * own host-C call stack carries the scope, so there's no push/pop
 * mutation; ρ is a value, not a stack data structure.
 *
 * Layout: kitchensink-style flat struct (tag + all variant fields
 * at top level). The recursive `parent` field is rho_t* because C
 * structs can't recurse by-value; ddcgc auto-derefs at match-arm
 * bind time so DSL code sees rho-by-value throughout. */
typedef enum { RHO_RHO_ROOT, RHO_LOOP_FRAME, RHO_FINALLY_FRAME, RHO_TRY_FRAME } rho_tag_t;
struct rho;
typedef struct rho rho_t;
struct rho {
    rho_tag_t   tag;
    /* loop_frame fields */
    sir_node_t* break_target;
    sir_node_t* continue_target;
    /* finally_frame fields */
    ast_stmt_t* body;
    /* try_frame + finally_frame: the REGION ID of the try whose handlers protect this
     * frame (spec §6). Every Throw minted under it records a row naming this id, and the
     * try's handler rows carry the same id — the escape lattice joins the two. An ID and
     * not the handler nodes because the handlers are built AFTER the body they protect,
     * so they do not exist when a Throw inside the body is minted.
     *
     * -1 means "this frame carries no handlers": the finally_frame a CATCH body runs
     * under. A catch body sits past the try_table, so its own try cannot catch what it
     * throws; claiming otherwise would be fail-OPEN. */
    int         region;
    rho_t*      parent;
};

static inline rho_t rho_root(struct ddcg_ctx* ctx) {
    (void)ctx;
    rho_t r;
    r.tag = RHO_RHO_ROOT;
    r.break_target = NULL;
    r.continue_target = NULL;
    r.body = NULL;
    r.region = -1;
    r.parent = NULL;
    return r;
}

/* Forward decl of arena-alloc helper; defined in compiler_helpers.c
 * since it needs the full ddcg_ctx_t struct from compiler_compile.h. */
struct rho* ddcg_rho_alloc_parent(struct ddcg_ctx* ctx, rho_t parent);

static inline rho_t loop_frame(struct ddcg_ctx* ctx,
                                sir_node_t* break_target,
                                sir_node_t* continue_target,
                                rho_t parent) {
    rho_t r;
    r.tag = RHO_LOOP_FRAME;
    r.break_target = break_target;
    r.continue_target = continue_target;
    r.body = NULL;
    r.region = -1;                 /* a loop is not a handler */
    r.parent = ddcg_rho_alloc_parent(ctx, parent);
    return r;
}

static inline rho_t finally_frame(struct ddcg_ctx* ctx,
                                   ast_stmt_t* body, int region, rho_t parent) {
    rho_t r;
    r.tag = RHO_FINALLY_FRAME;
    r.break_target = NULL;
    r.continue_target = NULL;
    r.body = body;
    r.region = region;
    r.parent = ddcg_rho_alloc_parent(ctx, parent);
    return r;
}

/* A try body with handlers but no finally: nothing to run on the way out, so no payload
 * beyond the region id — the frame tells `return` that it sits inside this frame's
 * try_table, and tells the §6 throw rule which handlers cover a Throw built under it. */
static inline rho_t try_frame(struct ddcg_ctx* ctx, int region, rho_t parent) {
    rho_t r;
    r.tag = RHO_TRY_FRAME;
    r.break_target = NULL;
    r.continue_target = NULL;
    r.body = NULL;
    r.region = region;
    r.parent = ddcg_rho_alloc_parent(ctx, parent);
    return r;
}

/* User-defined `sum tree_op` — defunctionalises the single-operand
 * spill-or-simple pattern. Built like a dest type (tag + per-variant
 * fields) but doesn't slot into the dispatcher signature; passed as
 * a value through helper calls. Field names mirror the DSL decl. */
/* Tag constants follow the asdl-c convention: TO_UPPER of the bare
 * variant ctor name with the module prefix. ddcgc's c_tag_value
 * doesn't word-split the ctor name (matching asdl), so a CamelCase
 * variant `ArrayLengthOp` becomes `TREE_OP_ARRAYLENGTHOP`, not
 * `TREE_OP_ARRAY_LENGTH_OP`. */
typedef enum {
    TREE_OP_ARRAYLENGTHOP, TREE_OP_GETFIELDOP, TREE_OP_NEWARRAYOP,
    TREE_OP_NEWREFARRAYOP, TREE_OP_CHECKCASTOP, TREE_OP_INSTANCEOFOP,
    TREE_OP_S2IOP, TREE_OP_I2SOP, TREE_OP_S2BOP, TREE_OP_I2BOP,
    TREE_OP_I2COP, TREE_OP_I2LOP, TREE_OP_I2FOP, TREE_OP_I2DOP,
    TREE_OP_L2IOP, TREE_OP_L2FOP, TREE_OP_L2DOP,
    TREE_OP_F2IOP, TREE_OP_F2LOP, TREE_OP_F2DOP,
    TREE_OP_D2IOP, TREE_OP_D2LOP, TREE_OP_D2FOP,
    TREE_OP_MOVEF2IOP, TREE_OP_MOVEI2FOP, TREE_OP_MOVED2LOP, TREE_OP_MOVEL2DOP,
    TREE_OP_F64SQRTOP, TREE_OP_F64FLOOROP, TREE_OP_F64CEILOP, TREE_OP_F64NEARESTOP,
    TREE_OP_CLASSINSTANTIABLEOP, TREE_OP_CLASSCONSTRUCTOP,
    TREE_OP_L2BOP, TREE_OP_L2SOP, TREE_OP_L2COP,
    TREE_OP_F2BOP, TREE_OP_F2SOP, TREE_OP_F2COP,
    TREE_OP_D2BOP, TREE_OP_D2SOP, TREE_OP_D2COP
} tree_op_tag_t;
typedef struct {
    tree_op_tag_t tag;
    sir_datatype_t dt;        /* GetFieldOp */
    int32_t cls_id;           /* GetFieldOp */
    int32_t cp;               /* GetFieldOp / CheckCastOp / InstanceOfOp */
    sir_atype_t at;           /* NewArrayOp / CheckCastOp / InstanceOfOp */
    sir_node_t* eref;         /* NewRefArrayOp: the created array's element referent */
} tree_op_t;
static inline tree_op_t ArrayLengthOp(struct ddcg_ctx* ctx) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_ARRAYLENGTHOP }; return o;
}
static inline tree_op_t GetFieldOp(struct ddcg_ctx* ctx,
                                    sir_datatype_t dt, int32_t cls_id, int32_t cp) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_GETFIELDOP, .dt = dt, .cls_id = cls_id, .cp = cp }; return o;
}
static inline tree_op_t NewArrayOp(struct ddcg_ctx* ctx, sir_atype_t at) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_NEWARRAYOP, .at = at }; return o;
}
static inline tree_op_t NewRefArrayOp(struct ddcg_ctx* ctx, sir_node_t* eref) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_NEWREFARRAYOP, .eref = eref }; return o;
}
static inline tree_op_t CheckCastOp(struct ddcg_ctx* ctx, sir_atype_t at, int32_t cp) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_CHECKCASTOP, .cp = cp, .at = at }; return o;
}
static inline tree_op_t InstanceOfOp(struct ddcg_ctx* ctx, sir_atype_t at, int32_t cp) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_INSTANCEOFOP, .cp = cp, .at = at }; return o;
}
static inline tree_op_t S2IOp(struct ddcg_ctx* ctx) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_S2IOP }; return o;
}
static inline tree_op_t I2SOp(struct ddcg_ctx* ctx) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_I2SOP }; return o;
}
static inline tree_op_t S2BOp(struct ddcg_ctx* ctx) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_S2BOP }; return o;
}
static inline tree_op_t I2BOp(struct ddcg_ctx* ctx) {
    (void)ctx; tree_op_t o = { .tag = TREE_OP_I2BOP }; return o;
}
/* §5.1 conversion tree-ops — all nullary (the operand is `value`). */
#define CONV_TREE_OP(Name, TAG) \
    static inline tree_op_t Name(struct ddcg_ctx* ctx) { \
        (void)ctx; tree_op_t o = { .tag = TAG }; return o; }
CONV_TREE_OP(I2COp, TREE_OP_I2COP) CONV_TREE_OP(I2LOp, TREE_OP_I2LOP)
CONV_TREE_OP(I2FOp, TREE_OP_I2FOP) CONV_TREE_OP(I2DOp, TREE_OP_I2DOP)
CONV_TREE_OP(L2IOp, TREE_OP_L2IOP) CONV_TREE_OP(L2FOp, TREE_OP_L2FOP)
CONV_TREE_OP(L2DOp, TREE_OP_L2DOP) CONV_TREE_OP(F2IOp, TREE_OP_F2IOP)
CONV_TREE_OP(F2LOp, TREE_OP_F2LOP) CONV_TREE_OP(F2DOp, TREE_OP_F2DOP)
CONV_TREE_OP(D2IOp, TREE_OP_D2IOP) CONV_TREE_OP(D2LOp, TREE_OP_D2LOP)
CONV_TREE_OP(D2FOp, TREE_OP_D2FOP)
CONV_TREE_OP(MoveF2IOp, TREE_OP_MOVEF2IOP) CONV_TREE_OP(MoveI2FOp, TREE_OP_MOVEI2FOP)
CONV_TREE_OP(MoveD2LOp, TREE_OP_MOVED2LOP) CONV_TREE_OP(MoveL2DOp, TREE_OP_MOVEL2DOP)
CONV_TREE_OP(F64SqrtOp, TREE_OP_F64SQRTOP) CONV_TREE_OP(F64FloorOp, TREE_OP_F64FLOOROP)
CONV_TREE_OP(F64CeilOp, TREE_OP_F64CEILOP) CONV_TREE_OP(F64NearestOp, TREE_OP_F64NEARESTOP)
CONV_TREE_OP(ClassInstantiableOp, TREE_OP_CLASSINSTANTIABLEOP)
CONV_TREE_OP(ClassConstructOp, TREE_OP_CLASSCONSTRUCTOP)
CONV_TREE_OP(L2BOp, TREE_OP_L2BOP)
CONV_TREE_OP(L2SOp, TREE_OP_L2SOP) CONV_TREE_OP(L2COp, TREE_OP_L2COP)
CONV_TREE_OP(F2BOp, TREE_OP_F2BOP) CONV_TREE_OP(F2SOp, TREE_OP_F2SOP)
CONV_TREE_OP(F2COp, TREE_OP_F2COP) CONV_TREE_OP(D2BOp, TREE_OP_D2BOP)
CONV_TREE_OP(D2SOp, TREE_OP_D2SOP) CONV_TREE_OP(D2COp, TREE_OP_D2COP)
#undef CONV_TREE_OP

/* User-defined `sum delivery_kind` — Pure (cg_store) vs Effectful
 * (cg_deliver_effectful). Picks the delivery primitive in the
 * unified spill_or_simple helper. */
typedef enum {
    DELIVERY_KIND_PURE, DELIVERY_KIND_EFFECTFUL
} delivery_kind_tag_t;
typedef struct { delivery_kind_tag_t tag; } delivery_kind_t;
static inline delivery_kind_t Pure(struct ddcg_ctx* ctx) {
    (void)ctx; delivery_kind_t k = { DELIVERY_KIND_PURE }; return k;
}
static inline delivery_kind_t Effectful(struct ddcg_ctx* ctx) {
    (void)ctx; delivery_kind_t k = { DELIVERY_KIND_EFFECTFUL }; return k;
}

/* User-defined `sum sema_cast_kind` — sema's JLS-level cast
 * classification. CastClass / CastArrayRef carry the CP index for
 * the class operand; primitive-conv variants are nullary. */
typedef enum {
    SEMA_CAST_KIND_CASTIDENTITY,
    SEMA_CAST_KIND_CASTS2I, SEMA_CAST_KIND_CASTI2S,
    SEMA_CAST_KIND_CASTS2B, SEMA_CAST_KIND_CASTI2B,
    SEMA_CAST_KIND_CASTI2C, SEMA_CAST_KIND_CASTI2L,
    SEMA_CAST_KIND_CASTI2F, SEMA_CAST_KIND_CASTI2D,
    SEMA_CAST_KIND_CASTL2I, SEMA_CAST_KIND_CASTL2F, SEMA_CAST_KIND_CASTL2D,
    SEMA_CAST_KIND_CASTF2I, SEMA_CAST_KIND_CASTF2L, SEMA_CAST_KIND_CASTF2D,
    SEMA_CAST_KIND_CASTD2I, SEMA_CAST_KIND_CASTD2L, SEMA_CAST_KIND_CASTD2F,
    SEMA_CAST_KIND_CASTL2B, SEMA_CAST_KIND_CASTL2S, SEMA_CAST_KIND_CASTL2C,
    SEMA_CAST_KIND_CASTF2B, SEMA_CAST_KIND_CASTF2S, SEMA_CAST_KIND_CASTF2C,
    SEMA_CAST_KIND_CASTD2B, SEMA_CAST_KIND_CASTD2S, SEMA_CAST_KIND_CASTD2C,
    SEMA_CAST_KIND_CASTCLASS,
    SEMA_CAST_KIND_CASTARRAYREFLECT,   /* §15.19.2 reference/multi-dim array: cp = its §10.8 array Class */
    SEMA_CAST_KIND_CASTNOOP
} sema_cast_kind_tag_t;
typedef struct {
    sema_cast_kind_tag_t tag;
    int32_t cp;          /* CastClass / CastArrayRef */
} sema_cast_kind_t;
static inline sema_cast_kind_t CastIdentity(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTIDENTITY, 0 }; return k;
}
static inline sema_cast_kind_t CastS2I(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTS2I, 0 }; return k;
}
/* §5.1 conversion cast-kinds — nullary (no CP operand). */
#define CONV_CAST_KIND(Name, TAG) \
    static inline sema_cast_kind_t Name(struct ddcg_ctx* ctx) { \
        (void)ctx; sema_cast_kind_t k = { TAG, 0 }; return k; }
CONV_CAST_KIND(CastI2C, SEMA_CAST_KIND_CASTI2C) CONV_CAST_KIND(CastI2L, SEMA_CAST_KIND_CASTI2L)
CONV_CAST_KIND(CastI2F, SEMA_CAST_KIND_CASTI2F) CONV_CAST_KIND(CastI2D, SEMA_CAST_KIND_CASTI2D)
CONV_CAST_KIND(CastL2I, SEMA_CAST_KIND_CASTL2I) CONV_CAST_KIND(CastL2F, SEMA_CAST_KIND_CASTL2F)
CONV_CAST_KIND(CastL2D, SEMA_CAST_KIND_CASTL2D) CONV_CAST_KIND(CastF2I, SEMA_CAST_KIND_CASTF2I)
CONV_CAST_KIND(CastF2L, SEMA_CAST_KIND_CASTF2L) CONV_CAST_KIND(CastF2D, SEMA_CAST_KIND_CASTF2D)
CONV_CAST_KIND(CastD2I, SEMA_CAST_KIND_CASTD2I) CONV_CAST_KIND(CastD2L, SEMA_CAST_KIND_CASTD2L)
CONV_CAST_KIND(CastD2F, SEMA_CAST_KIND_CASTD2F) CONV_CAST_KIND(CastL2B, SEMA_CAST_KIND_CASTL2B)
CONV_CAST_KIND(CastL2S, SEMA_CAST_KIND_CASTL2S) CONV_CAST_KIND(CastL2C, SEMA_CAST_KIND_CASTL2C)
CONV_CAST_KIND(CastF2B, SEMA_CAST_KIND_CASTF2B) CONV_CAST_KIND(CastF2S, SEMA_CAST_KIND_CASTF2S)
CONV_CAST_KIND(CastF2C, SEMA_CAST_KIND_CASTF2C) CONV_CAST_KIND(CastD2B, SEMA_CAST_KIND_CASTD2B)
CONV_CAST_KIND(CastD2S, SEMA_CAST_KIND_CASTD2S) CONV_CAST_KIND(CastD2C, SEMA_CAST_KIND_CASTD2C)
#undef CONV_CAST_KIND
static inline sema_cast_kind_t CastI2S(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTI2S, 0 }; return k;
}
static inline sema_cast_kind_t CastS2B(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTS2B, 0 }; return k;
}
static inline sema_cast_kind_t CastI2B(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTI2B, 0 }; return k;
}
static inline sema_cast_kind_t CastClass(struct ddcg_ctx* ctx, int32_t cp) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTCLASS, cp }; return k;
}
static inline sema_cast_kind_t CastArrayReflect(struct ddcg_ctx* ctx, int32_t cp) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTARRAYREFLECT, cp }; return k;
}
static inline sema_cast_kind_t CastNoop(struct ddcg_ctx* ctx) {
    (void)ctx; sema_cast_kind_t k = { SEMA_CAST_KIND_CASTNOOP, 0 }; return k;
}

/* User-defined `sum sema_instanceof_kind` — instanceof has one
 * shape per atype + cp_index combination. NameType / QualifiedName
 * collapse into InstanceOfClass at sema time. */
typedef enum {
    SEMA_INSTANCEOF_KIND_INSTANCEOFCLASS,
    SEMA_INSTANCEOF_KIND_INSTANCEOFARRAYREFLECT,   /* §15.19.2 reference/multi-dim array: cp = its §10.8 array Class */
    SEMA_INSTANCEOF_KIND_INSTANCEOFALWAYSFALSE
} sema_instanceof_kind_tag_t;
typedef struct {
    sema_instanceof_kind_tag_t tag;
    int32_t cp;
} sema_instanceof_kind_t;
static inline sema_instanceof_kind_t InstanceOfClass(struct ddcg_ctx* ctx, int32_t cp) {
    (void)ctx; sema_instanceof_kind_t k = { SEMA_INSTANCEOF_KIND_INSTANCEOFCLASS, cp }; return k;
}
static inline sema_instanceof_kind_t InstanceOfArrayReflect(struct ddcg_ctx* ctx, int32_t cp) {
    (void)ctx; sema_instanceof_kind_t k = { SEMA_INSTANCEOF_KIND_INSTANCEOFARRAYREFLECT, cp }; return k;
}
static inline sema_instanceof_kind_t InstanceOfAlwaysFalse(struct ddcg_ctx* ctx) {
    (void)ctx; sema_instanceof_kind_t k = { SEMA_INSTANCEOF_KIND_INSTANCEOFALWAYSFALSE, 0 }; return k;
}

/* User-defined `sum sema_new_array_kind` — newarray atype is
 * picked by element kind; ref-array uses cp_index for the element. */
typedef enum {
    SEMA_NEW_ARRAY_KIND_NEWARRAYBOOL, SEMA_NEW_ARRAY_KIND_NEWARRAYBYTE,
    SEMA_NEW_ARRAY_KIND_NEWARRAYSHORT, SEMA_NEW_ARRAY_KIND_NEWARRAYINT,
    SEMA_NEW_ARRAY_KIND_NEWARRAYCHAR, SEMA_NEW_ARRAY_KIND_NEWARRAYLONG,
    SEMA_NEW_ARRAY_KIND_NEWARRAYFLOAT, SEMA_NEW_ARRAY_KIND_NEWARRAYDOUBLE,
    SEMA_NEW_ARRAY_KIND_NEWARRAYREF,
    SEMA_NEW_ARRAY_KIND_NEWARRAYINVALID
} sema_new_array_kind_tag_t;
typedef struct {
    sema_new_array_kind_tag_t tag;
    int32_t cp;
} sema_new_array_kind_t;
static inline sema_new_array_kind_t NewArrayBool(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYBOOL, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayByte(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYBYTE, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayShort(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYSHORT, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayInt(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYINT, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayChar(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYCHAR, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayLong(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYLONG, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayFloat(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYFLOAT, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayDouble(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYDOUBLE, 0 }; return k;
}
static inline sema_new_array_kind_t NewArrayRef(struct ddcg_ctx* ctx, int32_t cp) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYREF, cp }; return k;
}
static inline sema_new_array_kind_t NewArrayInvalid(struct ddcg_ctx* ctx) {
    (void)ctx; sema_new_array_kind_t k = { SEMA_NEW_ARRAY_KIND_NEWARRAYINVALID, 0 }; return k;
}

#endif /* JAVELINA_COMPILER_RUNTIME_H */
