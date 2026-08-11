/*
 * validate.h — the single-pass validator that builds the side-table.
 *
 * Walks a function body once and distills the branch info WASM bytecode leaves
 * implicit (Titzer §3.1): for each branch (if/else/br/br_if) a side-table entry
 * ⟨delta_ip, delta_stp, vals, pop⟩. block/loop/end are structural (no entries).
 * The interpreter and JIT consume the side-table for O(1) branches.
 */
#ifndef JAV_VALIDATE_H
#define JAV_VALIDATE_H

#include "runtime_api.h"   /* jav_st_entry_t (via the substrate) */
#include "jav_valtype.h"  /* jav_valtype_t — opgen-generated value-type tags */
#include "jav_subtype.h"  /* the §3.3 subtype lattice the validator consults */
#include "jav_error.h"    /* jav_err_t — the specific §7.6 reject reason threaded out */

/* ── The §7.6 type-checking validator (jav_typecheck below) ──────────────────
 * A function type as actual value-type SEQUENCES, for full type checking of block
 * types, calls, and the function signature. */
typedef struct jav_functype {
    const jav_valtype_t* params;  uint16_t nparams;
    const jav_valtype_t* results; uint16_t nresults;
    const uint32_t* param_tidx;    /* parallel: concrete typeidx of a (ref $t) param (else 0); NULL = none */
    const uint32_t* result_tidx;   /* parallel: concrete typeidx of a (ref $t) result (else 0); NULL = none */
} jav_functype_t;

/* Structural equality of two module function types (MVP exact match). The dynamic
 * call_indirect gate and §4.5.2 import linking share it (defined in jav_runtime.c). */
int jav_functype_eq(const jav_functype_t* a, const jav_functype_t* b);

/* A struct type: its field value types (for struct.new/get/set type checking). A field
 * that is itself a reference is WVT_REF / WVT_REF_NN, its heaptype (HT_* code or referenced
 * typeidx) in `field_tidx` (parallel). */
typedef struct {
    const jav_valtype_t* fields;       /* field value types (unpacked: a packed field is i32) */
    const uint32_t*       field_tidx;   /* parallel: heaptype of a WVT_REF field (else 0) */
    unsigned nfields;
    const uint8_t*        field_mut;     /* parallel: §2.3.9 mutability (1 = mut, 0 = const); NULL ⇒ all const */
} jav_structtype_t;

/* An array type: its element value type (for array.new/get/set). */
typedef struct {
    jav_valtype_t elem;       /* element value type (unpacked) */
    uint32_t       elem_tidx;  /* heaptype of a WVT_REF element (HT_* code or referenced typeidx) */
    uint8_t        elem_mut;    /* §2.3.9 mutability (1 = mut, 0 = const) */
} jav_arraytype_t;

/* The validation context: everything an instruction's transfer function needs to
 * resolve an index to a type (§7.6 "Context"). Any array may be NULL when its
 * count is 0. globals is the type per global (mutability checking is a later
 * refinement). locals is this function's locals (params first, then declared). */
typedef struct {
    const jav_functype_t* types;     unsigned ntypes;     /* block-type typeidx, call_indirect */
    const jav_functype_t* func_sigs; unsigned nfuncs;     /* call funcidx -> signature */
    const jav_valtype_t*  globals;   unsigned nglobals;   /* global.get/set */
    const uint32_t*        global_tidx;                    /* parallel: concrete typeidx of a (ref $t) global (else 0) */
    const uint8_t*         global_mut;                     /* parallel: 1 if global i is mutable (global.set §3.4.3); NULL ⇒ all immutable */
    const uint32_t*        func_type_idx;                  /* funcidx -> the func's defined typeidx (ref.func -> (ref $t)) */
    const uint8_t*         func_ref_declared;               /* funcidx -> 1 if in C.refs (§3.4.6 ref.func); NULL ⇒ check skipped */
    const jav_valtype_t*  locals;    unsigned nlocals;    /* local.get/set/tee */
    unsigned nparams;                                      /* the first nparams locals are params: always initialized (§3.4.2 local init) */
    const uint32_t*        local_tidx;                     /* parallel to locals: typeidx of a concrete-ref local (else 0) */
    const jav_valtype_t*  results;   unsigned nresults;   /* return / implicit outer block */
    const uint32_t*        result_tidx;                    /* parallel: concrete typeidx of a (ref $t) result (else 0) */
    unsigned ntables;                                      /* table / call_indirect tableidx bound */
    const jav_valtype_t*  table_reftype;                   /* parallel: element reftype per table; NULL ⇒ all funcref */
    const uint32_t*        table_tidx;                      /* parallel: concrete typeidx of a (ref $t) table's reftype (else 0) */
    const uint8_t*         table_is64;                      /* parallel: 1 if table i is 64-bit (addr type i64), else i32; NULL ⇒ all i32 */
    unsigned nmemories;                                    /* memidx bound (§3.4.5); 0 ⇒ no memory declared */
    const uint8_t* mem_is64;                               /* parallel: 1 if memory i is 64-bit (addr type i64), else i32; NULL ⇒ all i32 */
    const jav_structtype_t* structtypes; unsigned nstructtypes;  /* struct.new/get/set, by typeidx */
    const jav_arraytype_t*  arraytypes;  unsigned narraytypes;   /* array.new/get/set, by typeidx */
    /* §5.5.15's note: "The data count section occurs before the code section, so a
     * single-pass validator can use this count instead of deferring validation." So a
     * body's data indices are bounded by the DATA COUNT section, not by the data
     * section — which is also what makes §5.5.17's `n? != eps \/ dataidx(func*) = eps`
     * fall out: with no data count section this is 0 and every data index is out of
     * range. `have_datacount` only distinguishes the two reasons. */
    unsigned ndatas;                                       /* §5.5.15 data COUNT (0 when the section is absent) */
    uint8_t  have_datacount;                               /* §5.5.17 was a data count section present at all */
    unsigned nelems;                                       /* element-segment count (array.new_elem/init_elem bound) */
    const jav_valtype_t* elem_reftype;                     /* parallel: reftype per element segment (table.init rt2<:rt1); NULL ⇒ all funcref */
    const uint32_t*       elem_tidx;                        /* parallel: concrete typeidx of a (ref $t) elem reftype (else 0) */
    const uint8_t* const* type_field_packs; unsigned num_type_field_packs;  /* typeidx -> field/elem storage widths (for get_s/u packed check) */
    const jav_functype_t* tags; unsigned ntags;   /* tagidx -> tag type (params -> ε); throw / try_table catch */
    const jav_subtype_ctx_t* lattice;             /* per-typeidx {kind, supertype} for the §3.3 concrete subtype relation */
} jav_vctx_t;

/* jav_try_t (per-try_table runtime metadata) is defined in the backend types
 * (jav_frame.h) so the frame can carry it; the validator fills it. */

/* Type-check a function body against `cx` and emit the side-table as a by-product
 * (the WASM 3.0 §7.6 reference algorithm: a single forward pass with an operand-
 * TYPE stack `vals`, a control-frame stack `ctrls`, and stack-polymorphic Bot for
 * dead code after unreachable/br/return). Returns 1 if the body is well-typed and
 * well-structured, 0 otherwise (type mismatch, stack underflow, unknown opcode,
 * out-of-range index, unbalanced control, peak depth over MAX_STACK). On success,
 * *out_st / *out_n give the malloc'd side-table (caller frees). */
int jav_typecheck(const uint8_t* code, size_t len, const jav_vctx_t* cx,
                   jav_st_entry_t** out_st, unsigned* out_n);

/* As jav_typecheck, but also emits the per-try_table metadata (malloc'd, caller
 * frees) for exception handling. out_try/out_ntry may be NULL (no try-table wanted).
 * jav_typecheck is this with NULL try outputs. `out_err` (may be NULL) gets the specific
 * §7.6 reject reason on failure (JAV_E_NONE on success) — the testsuite-vocabulary detail. */
int jav_typecheck_ex(const uint8_t* code, size_t len, const jav_vctx_t* cx,
                      jav_st_entry_t** out_st, unsigned* out_n,
                      jav_try_t** out_try, unsigned* out_ntry, jav_err_t* out_err);

/* Validate a CONSTANT EXPRESSION (a global / element-offset / data-offset
 * initializer): only the "extended-const" instruction set is admissible —
 * t.const, global.get (of a prior immutable import), and i32/i64 add/sub/mul —
 * terminated by `end` with exactly one result on the stack. Returns 1 if `code`
 * is a well-formed extended-const expression, 0 otherwise. Evaluation itself is
 * the ordinary generated interpreter (a constant expression is just a tiny body);
 * this is only the admissibility gate the spec layers on top. */
int jav_validate_const_expr(const uint8_t* code, size_t len);

#endif /* JAV_VALIDATE_H */
