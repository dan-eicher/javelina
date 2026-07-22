/* sir_op_gamma.h — γ per-opcode table for the Click partition-refinement
 * optimizer.
 *
 * One row per SIR opcode. Each row collects every property of that opcode
 * the engine consults during PROPAGATE: classifications (commutative,
 * congruent, purity, arity), type and constant transfer, the §4.6
 * fold-of-congruent hook, the §4.8 algebraic 1-constant identity hook
 * (driving Leader → Follower transitions), and the yoctojc absorbing-
 * with-purity composition. cp_node_type / cp_node_const / cp_replace_width
 * / cp_is_identity_const read rows; engine code does not dispatch on
 * sir_node_t_tag for γ work outside the engine-special set (PHI,
 * LoadLocal, CHECKCAST, LoadConst — each structurally distinct from
 * "one opcode, fixed properties").
 *
 * Shape mirrors src/vm/interp.c's dispatch[256] — designated-initializer
 * indexed by opcode, one row per opcode. Adding a new opcode is one row. */
#ifndef JAVELINA_COMPILER_SIR_OP_GAMMA_H
#define JAVELINA_COMPILER_SIR_OP_GAMMA_H

#include "gen/sir_ast.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"

#include <stdbool.h>
#include <stdint.h>

#define SIR_TAG_COUNT (SIR_PRIMARRAY + 1)   /* PrimArray is the last node tag */

/* Type kinds — how a row produces the lattice Type for its opcode.
 * GT_NONE marks rows that don't carry a type (spine opcodes). */
typedef enum {
    GT_NONE = 0,
    GT_PRIM_DT,          /* prim(node-carried data_type field) */
    GT_PRIM_FIXED,       /* prim(row-carried fixed dt) — conversions,
                          * ArrayLength (int, §10.7), the boolean-valued
                          * ops (comparisons / InstanceOf — byte, the
                          * JT_BOOL convention) */
    GT_REF,              /* ref(class_id field) — New, LoadThis */
    GT_ARRAY,            /* array(1, class_id field) — NewRefArray */
    GT_CHECKCAST,        /* per-discriminator (engine-special) */
    GT_VIA_INPUT,        /* propagate input[0]'s type — LoadLocal §4.7
                          * COPY-Follower */
    GT_BOTTOM,           /* lattice precision gap (e.g. LoadClass — no
                          * sema at γ level to name Class's id) */
    GT_SEMA,             /* sema-driven type — row's type_sema fn looks
                          * up method return type or field type. Used by
                          * INVOKE* (method return) and GETFIELD /
                          * GETSTATIC (field declared type). */
    GT_ARRAY_ELEM,       /* ArrayLoad — PRIM of element width for non-ref
                          * element types; BOTTOM for ref-element arrays
                          * (which would need an element class_id the
                          * opcode doesn't carry). */
    GT_NULL,             /* the null reference — LoadNull, lattice
                          * singleton TK_NULL. */
    GT_PRIM_ARRAY,       /* dim-1 array of a primitive element — NewArray
                          * of any primitive atype. The elem_type maps to
                          * its width via lat_atype_to_dt (the one
                          * atype→dt authority; boolean packs as byte). */
} gt_type_kind_t;

/* §4.8 identity side / absorbing side. GS_EITHER iff the operation is
 * commutative on the slot's role. */
typedef enum {
    GS_NONE = 0,
    GS_LEFT,
    GS_RIGHT,
    GS_EITHER,
} gt_side_t;

/* §4.6 fold-of-congruent shape — what value the opcode produces when its
 * operands occupy the same partition. */
typedef enum {
    GC_NONE = 0,
    GC_ZERO,             /* §4.6: x − x → 0  (SIR_SUB) */
    GC_CMP_REFLEXIVE,    /* §4.6: x ⊙ x by comparison node tag (engine reads e->tag) */
} gt_cong_t;

/* Forward — helpers take a sir_node_t pointer to read carried fields
 * (data_type, class_id, atype, cmp.op). Declared but not defined here. */
struct sir_node_t;

typedef struct sir_op_gamma {
    sir_node_t_tag     tag;
    const char*        mnemonic;

    /* §3.2.1 classification slots. Drive cp_op_is_commutative,
     * cp_congruent_op, the per-tag arm of cp_expr_is_pure, cp_opcode_key.
     *   is_pure_if_children_pure: binary / unary opcode whose result is
     *     purely a function of inputs (no throw, no allocation, no read).
     *     The recursive walk over children stays in cp_expr_is_pure
     *     (sir.asdl-shape fact, not γ).
     *   is_leaf_pure: leaf opcode (no children) that is unconditionally
     *     pure — LoadConst, LoadNull, LoadLocal, LoadThis.
     *   is_commutative_fn: optional override for opcodes whose
     *     commutativity depends on a discriminator field — currently
     *     SIR_CMP (commutative only for EQ / NE). Engine reads the fn
     *     pointer when set; otherwise the bool.
     *   bucket_discriminator: optional EXACT discriminator for the
     *     partition-init bucket, for opcodes whose computed function
     *     depends on an operator immediate rather than an input —
     *     GetField/GetStatic bucket by (class_id, field_idx): two
     *     loads of DIFFERENT fields are never value-equal even with
     *     identical (obj, memory) inputs. Nodes with different
     *     discriminator values never share an initial partition
     *     (partition-init keys on the full 32-bit value, no packing
     *     loss). NULL → the opcode alone is the bucket. */
    bool               is_commutative;
    bool               (*is_commutative_fn)(const struct sir_node_t* e);
    bool               is_congruent;
    bool               is_pure_if_children_pure;
    bool               is_leaf_pure;
    int                arity;            /* 0 / 1 / 2 for fold dispatch */
    uint32_t           (*bucket_discriminator)(const struct sir_node_t* e);

    /* §3.2.1 type transfer — γ_T. type_class_id / type_prim_dt are read
     * for the matching type_kind; the unused ones stay NULL. type_sema
     * is the sema-driven path (method return type, field type). */
    gt_type_kind_t     type_kind;
    int                (*type_class_id)(const struct sir_node_t* e);
    sir_datatype_t     (*type_prim_dt)(const struct sir_node_t* e);
    sir_datatype_t     type_prim_fixed_dt;
    const Type*        (*type_sema)(const sema_ctx_t* sema,
                                     const struct sir_node_t* e,
                                     type_pool_t* pool);

    /* §3.2.1 constant transfer — γ_K. fold_unary for arity 1,
     * fold_binary for arity 2, fold_cmp for SIR_CMP specifically.
     * The fold returns false if it can't produce a value (DIV by 0,
     * INT32_MIN / -1); otherwise *out gets the int32 result. */
    bool               (*fold_unary)(int32_t a, int32_t* out);
    bool               (*fold_binary)(int32_t l, int32_t r, int32_t* out);
    int32_t            (*fold_cmp)(int op, int32_t l, int32_t r);  /* op = comparison node tag */

    /* Range-aware γ_K variants. Called when any input is CP_C_RANGE
     * (and TOP/BOTTOM cases have been handled by the dispatcher).
     * Each accepts and returns cp_const_t directly — KNOWN inputs
     * normalize to [k, k] internally. Overflow / undefined cases
     * return CP_C_BOTTOM. NULL slot means the opcode has no
     * meaningful range arithmetic at this lattice resolution
     * (bitwise / shift / div / rem) — that's a design choice, not
     * a placeholder. */
    struct cp_const_t  (*fold_unary_range)(struct cp_const_t a);
    struct cp_const_t  (*fold_binary_range)(struct cp_const_t l, struct cp_const_t r);
    struct cp_const_t  (*fold_cmp_range)(int op,  /* comparison node tag */
                                          struct cp_const_t l,
                                          struct cp_const_t r);

    /* §5.1.2/§5.1.3 primitive conversion: a KNOWN operand of the source
     * width → a KNOWN result of the target width (the operand and result
     * carriers differ, so this can't go through the i32 fold_unary). The
     * function carries the JLS narrowing rules (float/double→int: NaN→0,
     * overflow→clamp, round toward zero). NULL for non-conversion ops. */
    struct cp_const_t  (*fold_convert)(struct cp_const_t a);

    /* §4.8 algebraic 1-constant identity. When one input carries
     * identity_k on identity_side, the opcode is a Follower of the
     * other input. cp_is_identity_const reads these slots. */
    gt_side_t          identity_side;
    int32_t            identity_k;

    /* yoctojc absorbing-with-purity (composes §3.2.1 fold with a purity
     * gate on the other operand — not vanilla §4.8). When one input
     * carries absorbing_k on absorbing_side and the other is pure, the
     * opcode's constant is absorbing_result. */
    gt_side_t          absorbing_side;
    int32_t            absorbing_k;
    int32_t            absorbing_result;

    /* §4.6 fold-of-congruent. cp_node_const reads this slot, queries
     * partition equality through the engine, and produces the cong
     * result (0 for GC_ZERO; per-op for GC_CMP_REFLEXIVE). */
    gt_cong_t          cong_fold;

    /* Rewrite-time width: not its own slot. The width an opcode emits
     * at rewrite is its γ_T width — derivable from type_kind +
     * type_prim_dt / type_prim_fixed_dt. cp_replace_width's engine glue
     * reads type_kind directly. */
} sir_op_gamma_t;

extern const sir_op_gamma_t sir_op_gamma[SIR_TAG_COUNT];

/* Convert a Java declared type (from sema's method / field tables) to
 * a lattice Type. Strips array dimensions; JT_CLASS at depth 0 → REF,
 * at depth ≥ 1 → ARRAY; anything else → BOTTOM. */
const Type* gamma_jt_to_type(java_type_t jt, type_pool_t* pool);
/* Threaded ref-descriptor node (ClassRef/ArrayRef/PrimArray) → interned
 * lattice Type; referent identity = Type-pointer equality. */
const Type* gamma_ref_to_type(const struct sir_node_t* ref, type_pool_t* pool);

/* The INVERSE, and the ONE builder: a Java reference type → the SIR ref descriptor node
 * (ClassRef / ArrayRef / PrimArray) a LoadLocal/StoreLocal carries; NULL for a primitive
 * (whose data_type fully describes it). `ddcg_ref_descriptor` is a thin wrapper over it,
 * so the DDCG and the optimizer's scalar replacement mint byte-identical descriptors. */
struct sir_node_t* sir_ref_descriptor(bbq_arena* arena, java_type_t t);

/* The ONE dt → storage-narrowing authority: wrap `value` in the conversion that
 * re-establishes `dt`'s field-storage semantics (an i8/i16 struct field narrows on
 * struct.set and re-extends on struct.get_s/_u; an i32 local performs none), or return
 * it unchanged for a full-width dt. Same one-builder discipline as sir_ref_descriptor:
 * a second open-coded dt→op table is how the rewrite and the DDCG's cast lowering come
 * to disagree about what narrows. */
struct sir_node_t* sir_narrow_to_storage(bbq_arena* arena, sir_datatype_t dt,
                                         struct sir_node_t* value);

/* Compute γ_T for a SIR node whose type depends only on its carried
 * fields plus sema lookups — i.e. everything except GT_VIA_INPUT
 * (LoadLocal, which needs caller-specific input access).
 * Centralizes the table-driven type dispatch; the engine's
 * cp_node_type handles GT_VIA_INPUT against vnode input chains and
 * delegates the rest here. */
const Type* gamma_type_for_node(const sema_ctx_t* sema,
                                 const struct sir_node_t* e,
                                 type_pool_t* pool);

#endif /* JAVELINA_COMPILER_SIR_OP_GAMMA_H */
