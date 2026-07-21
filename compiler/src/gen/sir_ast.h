/* ============================================================
 * Auto-generated from ASDL — do not edit by hand.
 * ============================================================ */
#ifndef SIR_AST_H
#define SIR_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "bbq_arena.h"
#include "bbq_vec.h"

/* ── Tag enums for sum types ────────────────────────────── */

typedef enum {
    SIR_DTBYTE,
    SIR_DTSHORT,
    SIR_DTINT,
    SIR_DTREF,
    SIR_DTCHAR,
    SIR_DTLONG,
    SIR_DTFLOAT,
    SIR_DTDOUBLE
} sir_datatype_t;

typedef enum {
    SIR_ATCLASS,
    SIR_ATBOOL,
    SIR_ATBYTE,
    SIR_ATSHORT,
    SIR_ATINT,
    SIR_ATREFARRAY,
    SIR_ATCHAR,
    SIR_ATLONG,
    SIR_ATFLOAT,
    SIR_ATDOUBLE
} sir_atype_t;

typedef enum {
    SIR_LOADCONST,
    SIR_LOADLONGCONST,
    SIR_LOADFLOATCONST,
    SIR_LOADDOUBLECONST,
    SIR_LOADNULL,
    SIR_LOADLOCAL,
    SIR_LOADTHIS,
    SIR_LOADCLASS,
    SIR_CLONECOPY,
    SIR_ADD,
    SIR_SUB,
    SIR_MUL,
    SIR_DIV,
    SIR_REM,
    SIR_NEG,
    SIR_AND,
    SIR_OR,
    SIR_XOR,
    SIR_SHL,
    SIR_SHR,
    SIR_USHR,
    SIR_LOGNOT,
    SIR_S2B,
    SIR_S2I,
    SIR_I2S,
    SIR_I2B,
    SIR_I2C,
    SIR_I2L,
    SIR_I2F,
    SIR_I2D,
    SIR_L2I,
    SIR_L2F,
    SIR_L2D,
    SIR_F2I,
    SIR_F2L,
    SIR_F2D,
    SIR_D2I,
    SIR_D2L,
    SIR_D2F,
    SIR_MOVEF2I,
    SIR_MOVEI2F,
    SIR_MOVED2L,
    SIR_MOVEL2D,
    SIR_F64SQRT,
    SIR_F64FLOOR,
    SIR_F64CEIL,
    SIR_F64NEAREST,
    SIR_EQ,
    SIR_NE,
    SIR_LT,
    SIR_LE,
    SIR_GT,
    SIR_GE,
    SIR_CLASSINSTANTIABLE,
    SIR_CLASSCONSTRUCT,
    SIR_NEW,
    SIR_INSTANCEOF,
    SIR_CHECKCAST,
    SIR_NEWARRAY,
    SIR_NEWREFARRAY,
    SIR_ARRAYLOAD,
    SIR_MEMLOAD8,
    SIR_ARRAYLENGTH,
    SIR_GETFIELD,
    SIR_GETSTATIC,
    SIR_INVOKEVIRTUAL,
    SIR_INVOKESPECIAL,
    SIR_INVOKESTATIC,
    SIR_INVOKEINTERFACE,
    SIR_STORELOCAL,
    SIR_EXPREFFECT,
    SIR_ARRAYSTORE,
    SIR_PUTFIELD,
    SIR_SETHEADER,
    SIR_ARRAYCOPY,
    SIR_PUTSTATIC,
    SIR_MEMSTORE8,
    SIR_BRANCH,
    SIR_SWITCH,
    SIR_RETURN,
    SIR_RETURNVOID,
    SIR_THROW,
    SIR_EXCEPTIONENTRY,
    SIR_TRYREGION,
    SIR_INC,
    SIR_NOP,
    SIR_CLASSREF,
    SIR_ARRAYREF,
    SIR_PRIMARRAY
} sir_node_t_tag;

/* ── Forward declarations ───────────────────────────────── */

typedef struct sir_method_t sir_method_t;
typedef struct sir_node_t sir_node_t;

/* ── Source location ────────────────────────────────────── */

typedef struct {
    const char* file;
    int line;
    int col;
} sir_srcloc;

/* ── Multi-constructor sum types (tagged unions) ────────── */

struct sir_node_t {
    sir_node_t_tag tag;
    sir_srcloc loc;
    union {
        struct {
            int32_t value;
            sir_datatype_t data_type;
        } load_const;
        struct {
            int64_t value;
        } load_long_const;
        struct {
            float value;
        } load_float_const;
        struct {
            double value;
        } load_double_const;
        struct {
            int32_t slot;
            sir_datatype_t data_type;
            sir_node_t* ref_type;
        } load_local;
        struct {
            sir_datatype_t data_type;
            int32_t class_id;
        } load_this;
        struct {
            int32_t class_id;
        } load_class;
        struct {
            int32_t class_id;
        } clone_copy;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } add;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } sub;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } mul;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } div;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } rem;
        struct {
            sir_datatype_t data_type;
            sir_node_t* operand;
        } neg;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } and_;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } or_;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } xor_;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } shl;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } shr;
        struct {
            sir_datatype_t data_type;
            sir_node_t* left;
            sir_node_t* right;
        } ushr;
        struct {
            sir_datatype_t data_type;
            sir_node_t* operand;
        } log_not;
        struct {
            sir_node_t* operand;
        } s2_b;
        struct {
            sir_node_t* operand;
        } s2_i;
        struct {
            sir_node_t* operand;
        } i2_s;
        struct {
            sir_node_t* operand;
        } i2_b;
        struct {
            sir_node_t* operand;
        } i2_c;
        struct {
            sir_node_t* operand;
        } i2_l;
        struct {
            sir_node_t* operand;
        } i2_f;
        struct {
            sir_node_t* operand;
        } i2_d;
        struct {
            sir_node_t* operand;
        } l2_i;
        struct {
            sir_node_t* operand;
        } l2_f;
        struct {
            sir_node_t* operand;
        } l2_d;
        struct {
            sir_node_t* operand;
        } f2_i;
        struct {
            sir_node_t* operand;
        } f2_l;
        struct {
            sir_node_t* operand;
        } f2_d;
        struct {
            sir_node_t* operand;
        } d2_i;
        struct {
            sir_node_t* operand;
        } d2_l;
        struct {
            sir_node_t* operand;
        } d2_f;
        struct {
            sir_node_t* operand;
        } move_f2_i;
        struct {
            sir_node_t* operand;
        } move_i2_f;
        struct {
            sir_node_t* operand;
        } move_d2_l;
        struct {
            sir_node_t* operand;
        } move_l2_d;
        struct {
            sir_node_t* operand;
        } f64_sqrt;
        struct {
            sir_node_t* operand;
        } f64_floor;
        struct {
            sir_node_t* operand;
        } f64_ceil;
        struct {
            sir_node_t* operand;
        } f64_nearest;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } eq;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } ne;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } lt;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } le;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } gt;
        struct {
            sir_node_t* left;
            sir_node_t* right;
        } ge;
        struct {
            sir_node_t* cls;
        } class_instantiable;
        struct {
            sir_node_t* cls;
        } class_construct;
        struct {
            int32_t class_id;
        } new_;
        struct {
            sir_node_t* obj;
            sir_atype_t atype;
            int32_t class_id;
        } instance_of;
        struct {
            sir_node_t* obj;
            sir_atype_t atype;
            int32_t class_id;
        } check_cast;
        struct {
            sir_atype_t elem_type;
            sir_node_t* size;
        } new_array;
        struct {
            int32_t class_id;
            sir_node_t* size;
            sir_node_t* elem_ref;
        } new_ref_array;
        struct {
            sir_datatype_t data_type;
            sir_node_t* arr;
            sir_node_t* index;
            sir_node_t* elem_ref;
        } array_load;
        struct {
            sir_node_t* addr;
        } mem_load8;
        struct {
            sir_node_t* arr;
        } array_length;
        struct {
            sir_datatype_t data_type;
            sir_node_t* obj;
            int32_t class_id;
            int32_t field_idx;
        } get_field;
        struct {
            sir_datatype_t data_type;
            int32_t class_id;
            int32_t field_idx;
        } get_static;
        struct {
            sir_node_t* obj;
            int32_t class_id;
            int32_t method_idx;
            sir_node_t** args;
            int args_count;
            sir_datatype_t return_type;
        } invoke_virtual;
        struct {
            sir_node_t* obj;
            int32_t class_id;
            int32_t method_idx;
            sir_node_t** args;
            int args_count;
            sir_datatype_t return_type;
        } invoke_special;
        struct {
            int32_t class_id;
            int32_t method_idx;
            sir_node_t** args;
            int args_count;
            sir_datatype_t return_type;
        } invoke_static;
        struct {
            sir_node_t* obj;
            int32_t class_id;
            int32_t method_idx;
            sir_node_t** args;
            int args_count;
            sir_datatype_t return_type;
        } invoke_interface;
        struct {
            int32_t slot;
            sir_datatype_t data_type;
            sir_node_t* ref_type;
            sir_node_t* value;
            sir_node_t* next;
        } store_local;
        struct {
            sir_node_t* value;
            int32_t is_void;
            sir_node_t* next;
        } expr_effect;
        struct {
            sir_datatype_t data_type;
            sir_node_t* arr;
            sir_node_t* index;
            sir_node_t* value;
            sir_node_t* next;
            sir_node_t* elem_ref;
        } array_store;
        struct {
            sir_datatype_t data_type;
            sir_node_t* obj;
            int32_t class_id;
            int32_t field_idx;
            sir_node_t* value;
            sir_node_t* next;
        } put_field;
        struct {
            sir_node_t* obj;
            sir_node_t* value;
            int32_t struct_class_id;
            sir_node_t* next;
        } set_header;
        struct {
            sir_datatype_t width;
            sir_node_t* dst;
            sir_node_t* dst_off;
            sir_node_t* src;
            sir_node_t* src_off;
            sir_node_t* len;
            sir_node_t* next;
        } array_copy;
        struct {
            sir_datatype_t data_type;
            int32_t class_id;
            int32_t field_idx;
            sir_node_t* value;
            sir_node_t* next;
        } put_static;
        struct {
            sir_node_t* addr;
            sir_node_t* value;
            sir_node_t* next;
        } mem_store8;
        struct {
            sir_node_t* cond;
            sir_node_t* on_true;
            sir_node_t* on_false;
        } branch;
        struct {
            sir_node_t* selector;
            sir_node_t** case_targets;
            int case_targets_count;
            int32_t* case_values;
            int case_values_count;
            sir_node_t* default_target;
            sir_datatype_t selector_type;
        } switch_;
        struct {
            sir_node_t* value;
            sir_datatype_t data_type;
        } return_;
        struct {
            sir_node_t* ref;
        } throw_;
        struct {
            int32_t local_slot;
            int32_t catch_class_id;
            sir_node_t* next;
        } exception_entry;
        struct {
            sir_node_t* handler;
            sir_node_t* next;
        } try_region;
        struct {
            int32_t slot;
            int32_t delta;
            sir_datatype_t data_type;
            sir_node_t* value;
            sir_node_t* next;
        } inc;
        struct {
            sir_node_t* next;
        } nop;
        struct {
            int32_t class_id;
        } class_ref;
        struct {
            int32_t class_id;
            int32_t dim;
        } array_ref;
        struct {
            sir_datatype_t width;
            int32_t dim;
        } prim_array;
    };
    sir_node_t* exc;
};

/* ── Single-constructor sum types (plain structs) ───────── */

struct sir_method_t {
    sir_srcloc loc;
    const char* name;
    int32_t class_id;
    int32_t method_id;
    int32_t max_locals;
    sir_node_t* entry;
};

/* ── Product types ──────────────────────────────────────── */

/* ── Constructor functions ──────────────────────────────── */

static inline sir_method_t* sir_method(bbq_arena* _a, const char* name, int32_t class_id, int32_t method_id, int32_t max_locals, sir_node_t* entry) {
    sir_method_t* _n = (sir_method_t*)bbq_arena_alloc(_a, sizeof(sir_method_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->name = name;
    _n->class_id = class_id;
    _n->method_id = method_id;
    _n->max_locals = max_locals;
    _n->entry = entry;
    return _n;
}

static inline sir_node_t* sir_load_const(bbq_arena* _a, int32_t value, sir_datatype_t data_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADCONST;
    _n->load_const.value = value;
    _n->load_const.data_type = data_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_long_const(bbq_arena* _a, int64_t value) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADLONGCONST;
    _n->load_long_const.value = value;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_float_const(bbq_arena* _a, float value) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADFLOATCONST;
    _n->load_float_const.value = value;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_double_const(bbq_arena* _a, double value) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADDOUBLECONST;
    _n->load_double_const.value = value;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_null(bbq_arena* _a) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADNULL;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_local(bbq_arena* _a, int32_t slot, sir_datatype_t data_type, sir_node_t* ref_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADLOCAL;
    _n->load_local.slot = slot;
    _n->load_local.data_type = data_type;
    _n->load_local.ref_type = ref_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_this(bbq_arena* _a, sir_datatype_t data_type, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADTHIS;
    _n->load_this.data_type = data_type;
    _n->load_this.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_load_class(bbq_arena* _a, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOADCLASS;
    _n->load_class.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_clone_copy(bbq_arena* _a, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_CLONECOPY;
    _n->clone_copy.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_add(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ADD;
    _n->add.data_type = data_type;
    _n->add.left = left;
    _n->add.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_sub(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_SUB;
    _n->sub.data_type = data_type;
    _n->sub.left = left;
    _n->sub.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_mul(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MUL;
    _n->mul.data_type = data_type;
    _n->mul.left = left;
    _n->mul.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_div(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_DIV;
    _n->div.data_type = data_type;
    _n->div.left = left;
    _n->div.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_rem(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_REM;
    _n->rem.data_type = data_type;
    _n->rem.left = left;
    _n->rem.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_neg(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NEG;
    _n->neg.data_type = data_type;
    _n->neg.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_and(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_AND;
    _n->and_.data_type = data_type;
    _n->and_.left = left;
    _n->and_.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_or(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_OR;
    _n->or_.data_type = data_type;
    _n->or_.left = left;
    _n->or_.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_xor(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_XOR;
    _n->xor_.data_type = data_type;
    _n->xor_.left = left;
    _n->xor_.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_shl(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_SHL;
    _n->shl.data_type = data_type;
    _n->shl.left = left;
    _n->shl.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_shr(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_SHR;
    _n->shr.data_type = data_type;
    _n->shr.left = left;
    _n->shr.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_ushr(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_USHR;
    _n->ushr.data_type = data_type;
    _n->ushr.left = left;
    _n->ushr.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_log_not(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LOGNOT;
    _n->log_not.data_type = data_type;
    _n->log_not.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_s2_b(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_S2B;
    _n->s2_b.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_s2_i(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_S2I;
    _n->s2_i.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_s(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2S;
    _n->i2_s.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_b(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2B;
    _n->i2_b.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_c(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2C;
    _n->i2_c.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_l(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2L;
    _n->i2_l.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_f(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2F;
    _n->i2_f.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_i2_d(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_I2D;
    _n->i2_d.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_l2_i(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_L2I;
    _n->l2_i.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_l2_f(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_L2F;
    _n->l2_f.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_l2_d(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_L2D;
    _n->l2_d.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f2_i(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F2I;
    _n->f2_i.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f2_l(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F2L;
    _n->f2_l.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f2_d(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F2D;
    _n->f2_d.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_d2_i(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_D2I;
    _n->d2_i.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_d2_l(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_D2L;
    _n->d2_l.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_d2_f(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_D2F;
    _n->d2_f.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_move_f2_i(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MOVEF2I;
    _n->move_f2_i.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_move_i2_f(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MOVEI2F;
    _n->move_i2_f.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_move_d2_l(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MOVED2L;
    _n->move_d2_l.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_move_l2_d(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MOVEL2D;
    _n->move_l2_d.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f64_sqrt(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F64SQRT;
    _n->f64_sqrt.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f64_floor(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F64FLOOR;
    _n->f64_floor.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f64_ceil(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F64CEIL;
    _n->f64_ceil.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_f64_nearest(bbq_arena* _a, sir_node_t* operand) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_F64NEAREST;
    _n->f64_nearest.operand = operand;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_eq(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_EQ;
    _n->eq.left = left;
    _n->eq.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_ne(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NE;
    _n->ne.left = left;
    _n->ne.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_lt(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LT;
    _n->lt.left = left;
    _n->lt.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_le(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_LE;
    _n->le.left = left;
    _n->le.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_gt(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_GT;
    _n->gt.left = left;
    _n->gt.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_ge(bbq_arena* _a, sir_node_t* left, sir_node_t* right) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_GE;
    _n->ge.left = left;
    _n->ge.right = right;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_class_instantiable(bbq_arena* _a, sir_node_t* cls) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_CLASSINSTANTIABLE;
    _n->class_instantiable.cls = cls;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_class_construct(bbq_arena* _a, sir_node_t* cls) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_CLASSCONSTRUCT;
    _n->class_construct.cls = cls;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_new(bbq_arena* _a, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NEW;
    _n->new_.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_instance_of(bbq_arena* _a, sir_node_t* obj, sir_atype_t atype, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INSTANCEOF;
    _n->instance_of.obj = obj;
    _n->instance_of.atype = atype;
    _n->instance_of.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_check_cast(bbq_arena* _a, sir_node_t* obj, sir_atype_t atype, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_CHECKCAST;
    _n->check_cast.obj = obj;
    _n->check_cast.atype = atype;
    _n->check_cast.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_new_array(bbq_arena* _a, sir_atype_t elem_type, sir_node_t* size) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NEWARRAY;
    _n->new_array.elem_type = elem_type;
    _n->new_array.size = size;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_new_ref_array(bbq_arena* _a, int32_t class_id, sir_node_t* size, sir_node_t* elem_ref) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NEWREFARRAY;
    _n->new_ref_array.class_id = class_id;
    _n->new_ref_array.size = size;
    _n->new_ref_array.elem_ref = elem_ref;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_array_load(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* arr, sir_node_t* index, sir_node_t* elem_ref) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ARRAYLOAD;
    _n->array_load.data_type = data_type;
    _n->array_load.arr = arr;
    _n->array_load.index = index;
    _n->array_load.elem_ref = elem_ref;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_mem_load8(bbq_arena* _a, sir_node_t* addr) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MEMLOAD8;
    _n->mem_load8.addr = addr;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_array_length(bbq_arena* _a, sir_node_t* arr) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ARRAYLENGTH;
    _n->array_length.arr = arr;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_get_field(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* obj, int32_t class_id, int32_t field_idx) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_GETFIELD;
    _n->get_field.data_type = data_type;
    _n->get_field.obj = obj;
    _n->get_field.class_id = class_id;
    _n->get_field.field_idx = field_idx;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_get_static(bbq_arena* _a, sir_datatype_t data_type, int32_t class_id, int32_t field_idx) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_GETSTATIC;
    _n->get_static.data_type = data_type;
    _n->get_static.class_id = class_id;
    _n->get_static.field_idx = field_idx;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_invoke_virtual(bbq_arena* _a, sir_node_t* obj, int32_t class_id, int32_t method_idx, sir_node_t** args, int args_count, sir_datatype_t return_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INVOKEVIRTUAL;
    _n->invoke_virtual.obj = obj;
    _n->invoke_virtual.class_id = class_id;
    _n->invoke_virtual.method_idx = method_idx;
    _n->invoke_virtual.args = args;
    _n->invoke_virtual.args_count = args_count;
    _n->invoke_virtual.return_type = return_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_invoke_special(bbq_arena* _a, sir_node_t* obj, int32_t class_id, int32_t method_idx, sir_node_t** args, int args_count, sir_datatype_t return_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INVOKESPECIAL;
    _n->invoke_special.obj = obj;
    _n->invoke_special.class_id = class_id;
    _n->invoke_special.method_idx = method_idx;
    _n->invoke_special.args = args;
    _n->invoke_special.args_count = args_count;
    _n->invoke_special.return_type = return_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_invoke_static(bbq_arena* _a, int32_t class_id, int32_t method_idx, sir_node_t** args, int args_count, sir_datatype_t return_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INVOKESTATIC;
    _n->invoke_static.class_id = class_id;
    _n->invoke_static.method_idx = method_idx;
    _n->invoke_static.args = args;
    _n->invoke_static.args_count = args_count;
    _n->invoke_static.return_type = return_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_invoke_interface(bbq_arena* _a, sir_node_t* obj, int32_t class_id, int32_t method_idx, sir_node_t** args, int args_count, sir_datatype_t return_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INVOKEINTERFACE;
    _n->invoke_interface.obj = obj;
    _n->invoke_interface.class_id = class_id;
    _n->invoke_interface.method_idx = method_idx;
    _n->invoke_interface.args = args;
    _n->invoke_interface.args_count = args_count;
    _n->invoke_interface.return_type = return_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_store_local(bbq_arena* _a, int32_t slot, sir_datatype_t data_type, sir_node_t* ref_type, sir_node_t* value, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_STORELOCAL;
    _n->store_local.slot = slot;
    _n->store_local.data_type = data_type;
    _n->store_local.ref_type = ref_type;
    _n->store_local.value = value;
    _n->store_local.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_expr_effect(bbq_arena* _a, sir_node_t* value, int32_t is_void, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_EXPREFFECT;
    _n->expr_effect.value = value;
    _n->expr_effect.is_void = is_void;
    _n->expr_effect.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_array_store(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* arr, sir_node_t* index, sir_node_t* value, sir_node_t* next, sir_node_t* elem_ref) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ARRAYSTORE;
    _n->array_store.data_type = data_type;
    _n->array_store.arr = arr;
    _n->array_store.index = index;
    _n->array_store.value = value;
    _n->array_store.next = next;
    _n->array_store.elem_ref = elem_ref;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_put_field(bbq_arena* _a, sir_datatype_t data_type, sir_node_t* obj, int32_t class_id, int32_t field_idx, sir_node_t* value, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_PUTFIELD;
    _n->put_field.data_type = data_type;
    _n->put_field.obj = obj;
    _n->put_field.class_id = class_id;
    _n->put_field.field_idx = field_idx;
    _n->put_field.value = value;
    _n->put_field.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_set_header(bbq_arena* _a, sir_node_t* obj, sir_node_t* value, int32_t struct_class_id, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_SETHEADER;
    _n->set_header.obj = obj;
    _n->set_header.value = value;
    _n->set_header.struct_class_id = struct_class_id;
    _n->set_header.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_array_copy(bbq_arena* _a, sir_datatype_t width, sir_node_t* dst, sir_node_t* dst_off, sir_node_t* src, sir_node_t* src_off, sir_node_t* len, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ARRAYCOPY;
    _n->array_copy.width = width;
    _n->array_copy.dst = dst;
    _n->array_copy.dst_off = dst_off;
    _n->array_copy.src = src;
    _n->array_copy.src_off = src_off;
    _n->array_copy.len = len;
    _n->array_copy.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_put_static(bbq_arena* _a, sir_datatype_t data_type, int32_t class_id, int32_t field_idx, sir_node_t* value, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_PUTSTATIC;
    _n->put_static.data_type = data_type;
    _n->put_static.class_id = class_id;
    _n->put_static.field_idx = field_idx;
    _n->put_static.value = value;
    _n->put_static.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_mem_store8(bbq_arena* _a, sir_node_t* addr, sir_node_t* value, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_MEMSTORE8;
    _n->mem_store8.addr = addr;
    _n->mem_store8.value = value;
    _n->mem_store8.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_branch(bbq_arena* _a, sir_node_t* cond, sir_node_t* on_true, sir_node_t* on_false) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_BRANCH;
    _n->branch.cond = cond;
    _n->branch.on_true = on_true;
    _n->branch.on_false = on_false;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_switch(bbq_arena* _a, sir_node_t* selector, sir_node_t** case_targets, int case_targets_count, int32_t* case_values, int case_values_count, sir_node_t* default_target, sir_datatype_t selector_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_SWITCH;
    _n->switch_.selector = selector;
    _n->switch_.case_targets = case_targets;
    _n->switch_.case_targets_count = case_targets_count;
    _n->switch_.case_values = case_values;
    _n->switch_.case_values_count = case_values_count;
    _n->switch_.default_target = default_target;
    _n->switch_.selector_type = selector_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_return(bbq_arena* _a, sir_node_t* value, sir_datatype_t data_type) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_RETURN;
    _n->return_.value = value;
    _n->return_.data_type = data_type;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_return_void(bbq_arena* _a) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_RETURNVOID;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_throw(bbq_arena* _a, sir_node_t* ref) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_THROW;
    _n->throw_.ref = ref;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_exception_entry(bbq_arena* _a, int32_t local_slot, int32_t catch_class_id, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_EXCEPTIONENTRY;
    _n->exception_entry.local_slot = local_slot;
    _n->exception_entry.catch_class_id = catch_class_id;
    _n->exception_entry.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_try_region(bbq_arena* _a, sir_node_t* handler, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_TRYREGION;
    _n->try_region.handler = handler;
    _n->try_region.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_inc(bbq_arena* _a, int32_t slot, int32_t delta, sir_datatype_t data_type, sir_node_t* value, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_INC;
    _n->inc.slot = slot;
    _n->inc.delta = delta;
    _n->inc.data_type = data_type;
    _n->inc.value = value;
    _n->inc.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_nop(bbq_arena* _a, sir_node_t* next) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_NOP;
    _n->nop.next = next;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_class_ref(bbq_arena* _a, int32_t class_id) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_CLASSREF;
    _n->class_ref.class_id = class_id;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_array_ref(bbq_arena* _a, int32_t class_id, int32_t dim) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_ARRAYREF;
    _n->array_ref.class_id = class_id;
    _n->array_ref.dim = dim;
    _n->exc = NULL;
    return _n;
}

static inline sir_node_t* sir_prim_array(bbq_arena* _a, sir_datatype_t width, int32_t dim) {
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    _n->loc = (sir_srcloc){0};   /* zero the common source location — arena_alloc doesn't; it's stamped later */
    _n->tag = SIR_PRIMARRAY;
    _n->prim_array.width = width;
    _n->prim_array.dim = dim;
    _n->exc = NULL;
    return _n;
}

/* ── Deep-copy (clone) into an arena ────────────────────── */
/* The IR is a graph: expression operands are trees, but the CPS spine has
 * shared merge points and loop back-edges. A naive recursion would loop or
 * duplicate the shared nodes, so the copy is memoised on the original pointer
 * (orig→copy). The memo is a transient linear table over a bbq_vec — method
 * bodies are small, so the O(n) probe is cheaper than a hash; the cloned
 * nodes themselves live in the caller's arena. Record the forwarding pointer
 * BEFORE recursing so a back-edge to a node mid-construction resolves. */

typedef struct { const void* k; void* v; } sir_copy_pair;
typedef struct { sir_copy_pair* pairs; } sir_copy_memo;

static inline void* sir_copy_memo_get(sir_copy_memo* _m, const void* _k) {
    for (int _i = 0; _i < bbq_vec_len(_m->pairs); _i++)
        if (_m->pairs[_i].k == _k) return _m->pairs[_i].v;
    return NULL;
}
static inline void sir_copy_memo_put(sir_copy_memo* _m, const void* _k, void* _v) {
    sir_copy_pair _p; _p.k = _k; _p.v = _v;
    bbq_vec_push(_m->pairs, _p);
}
static inline void sir_copy_memo_dispose(sir_copy_memo* _m) {
    bbq_vec_free(_m->pairs);
}

/* Forward declarations (the copy functions are mutually recursive). */
static inline sir_method_t* sir_method_copy(bbq_arena* _a, sir_copy_memo* _memo, const sir_method_t* _src);
static inline sir_node_t* sir_node_copy(bbq_arena* _a, sir_copy_memo* _memo, const sir_node_t* _src);

static inline sir_method_t* sir_method_copy(bbq_arena* _a, sir_copy_memo* _memo, const sir_method_t* _src) {
    if (!_src) return NULL;
    void* _hit = sir_copy_memo_get(_memo, _src);
    if (_hit) return (sir_method_t*)_hit;
    sir_method_t* _n = (sir_method_t*)bbq_arena_alloc(_a, sizeof(sir_method_t));
    sir_copy_memo_put(_memo, _src, _n);
    _n->loc = _src->loc;
    _n->name = _src->name;
    _n->class_id = _src->class_id;
    _n->method_id = _src->method_id;
    _n->max_locals = _src->max_locals;
    _n->entry = sir_node_copy(_a, _memo, _src->entry);
    return _n;
}

static inline sir_node_t* sir_node_copy(bbq_arena* _a, sir_copy_memo* _memo, const sir_node_t* _src) {
    if (!_src) return NULL;
    void* _hit = sir_copy_memo_get(_memo, _src);
    if (_hit) return (sir_node_t*)_hit;
    sir_node_t* _n = (sir_node_t*)bbq_arena_alloc(_a, sizeof(sir_node_t));
    sir_copy_memo_put(_memo, _src, _n);
    _n->loc = _src->loc;
    _n->tag = _src->tag;
    switch (_src->tag) {
    case SIR_LOADCONST:
        _n->load_const.value = _src->load_const.value;
        _n->load_const.data_type = _src->load_const.data_type;
        break;
    case SIR_LOADLONGCONST:
        _n->load_long_const.value = _src->load_long_const.value;
        break;
    case SIR_LOADFLOATCONST:
        _n->load_float_const.value = _src->load_float_const.value;
        break;
    case SIR_LOADDOUBLECONST:
        _n->load_double_const.value = _src->load_double_const.value;
        break;
    case SIR_LOADNULL:
        break;
    case SIR_LOADLOCAL:
        _n->load_local.slot = _src->load_local.slot;
        _n->load_local.data_type = _src->load_local.data_type;
        _n->load_local.ref_type = sir_node_copy(_a, _memo, _src->load_local.ref_type);
        break;
    case SIR_LOADTHIS:
        _n->load_this.data_type = _src->load_this.data_type;
        _n->load_this.class_id = _src->load_this.class_id;
        break;
    case SIR_LOADCLASS:
        _n->load_class.class_id = _src->load_class.class_id;
        break;
    case SIR_CLONECOPY:
        _n->clone_copy.class_id = _src->clone_copy.class_id;
        break;
    case SIR_ADD:
        _n->add.data_type = _src->add.data_type;
        _n->add.left = sir_node_copy(_a, _memo, _src->add.left);
        _n->add.right = sir_node_copy(_a, _memo, _src->add.right);
        break;
    case SIR_SUB:
        _n->sub.data_type = _src->sub.data_type;
        _n->sub.left = sir_node_copy(_a, _memo, _src->sub.left);
        _n->sub.right = sir_node_copy(_a, _memo, _src->sub.right);
        break;
    case SIR_MUL:
        _n->mul.data_type = _src->mul.data_type;
        _n->mul.left = sir_node_copy(_a, _memo, _src->mul.left);
        _n->mul.right = sir_node_copy(_a, _memo, _src->mul.right);
        break;
    case SIR_DIV:
        _n->div.data_type = _src->div.data_type;
        _n->div.left = sir_node_copy(_a, _memo, _src->div.left);
        _n->div.right = sir_node_copy(_a, _memo, _src->div.right);
        break;
    case SIR_REM:
        _n->rem.data_type = _src->rem.data_type;
        _n->rem.left = sir_node_copy(_a, _memo, _src->rem.left);
        _n->rem.right = sir_node_copy(_a, _memo, _src->rem.right);
        break;
    case SIR_NEG:
        _n->neg.data_type = _src->neg.data_type;
        _n->neg.operand = sir_node_copy(_a, _memo, _src->neg.operand);
        break;
    case SIR_AND:
        _n->and_.data_type = _src->and_.data_type;
        _n->and_.left = sir_node_copy(_a, _memo, _src->and_.left);
        _n->and_.right = sir_node_copy(_a, _memo, _src->and_.right);
        break;
    case SIR_OR:
        _n->or_.data_type = _src->or_.data_type;
        _n->or_.left = sir_node_copy(_a, _memo, _src->or_.left);
        _n->or_.right = sir_node_copy(_a, _memo, _src->or_.right);
        break;
    case SIR_XOR:
        _n->xor_.data_type = _src->xor_.data_type;
        _n->xor_.left = sir_node_copy(_a, _memo, _src->xor_.left);
        _n->xor_.right = sir_node_copy(_a, _memo, _src->xor_.right);
        break;
    case SIR_SHL:
        _n->shl.data_type = _src->shl.data_type;
        _n->shl.left = sir_node_copy(_a, _memo, _src->shl.left);
        _n->shl.right = sir_node_copy(_a, _memo, _src->shl.right);
        break;
    case SIR_SHR:
        _n->shr.data_type = _src->shr.data_type;
        _n->shr.left = sir_node_copy(_a, _memo, _src->shr.left);
        _n->shr.right = sir_node_copy(_a, _memo, _src->shr.right);
        break;
    case SIR_USHR:
        _n->ushr.data_type = _src->ushr.data_type;
        _n->ushr.left = sir_node_copy(_a, _memo, _src->ushr.left);
        _n->ushr.right = sir_node_copy(_a, _memo, _src->ushr.right);
        break;
    case SIR_LOGNOT:
        _n->log_not.data_type = _src->log_not.data_type;
        _n->log_not.operand = sir_node_copy(_a, _memo, _src->log_not.operand);
        break;
    case SIR_S2B:
        _n->s2_b.operand = sir_node_copy(_a, _memo, _src->s2_b.operand);
        break;
    case SIR_S2I:
        _n->s2_i.operand = sir_node_copy(_a, _memo, _src->s2_i.operand);
        break;
    case SIR_I2S:
        _n->i2_s.operand = sir_node_copy(_a, _memo, _src->i2_s.operand);
        break;
    case SIR_I2B:
        _n->i2_b.operand = sir_node_copy(_a, _memo, _src->i2_b.operand);
        break;
    case SIR_I2C:
        _n->i2_c.operand = sir_node_copy(_a, _memo, _src->i2_c.operand);
        break;
    case SIR_I2L:
        _n->i2_l.operand = sir_node_copy(_a, _memo, _src->i2_l.operand);
        break;
    case SIR_I2F:
        _n->i2_f.operand = sir_node_copy(_a, _memo, _src->i2_f.operand);
        break;
    case SIR_I2D:
        _n->i2_d.operand = sir_node_copy(_a, _memo, _src->i2_d.operand);
        break;
    case SIR_L2I:
        _n->l2_i.operand = sir_node_copy(_a, _memo, _src->l2_i.operand);
        break;
    case SIR_L2F:
        _n->l2_f.operand = sir_node_copy(_a, _memo, _src->l2_f.operand);
        break;
    case SIR_L2D:
        _n->l2_d.operand = sir_node_copy(_a, _memo, _src->l2_d.operand);
        break;
    case SIR_F2I:
        _n->f2_i.operand = sir_node_copy(_a, _memo, _src->f2_i.operand);
        break;
    case SIR_F2L:
        _n->f2_l.operand = sir_node_copy(_a, _memo, _src->f2_l.operand);
        break;
    case SIR_F2D:
        _n->f2_d.operand = sir_node_copy(_a, _memo, _src->f2_d.operand);
        break;
    case SIR_D2I:
        _n->d2_i.operand = sir_node_copy(_a, _memo, _src->d2_i.operand);
        break;
    case SIR_D2L:
        _n->d2_l.operand = sir_node_copy(_a, _memo, _src->d2_l.operand);
        break;
    case SIR_D2F:
        _n->d2_f.operand = sir_node_copy(_a, _memo, _src->d2_f.operand);
        break;
    case SIR_MOVEF2I:
        _n->move_f2_i.operand = sir_node_copy(_a, _memo, _src->move_f2_i.operand);
        break;
    case SIR_MOVEI2F:
        _n->move_i2_f.operand = sir_node_copy(_a, _memo, _src->move_i2_f.operand);
        break;
    case SIR_MOVED2L:
        _n->move_d2_l.operand = sir_node_copy(_a, _memo, _src->move_d2_l.operand);
        break;
    case SIR_MOVEL2D:
        _n->move_l2_d.operand = sir_node_copy(_a, _memo, _src->move_l2_d.operand);
        break;
    case SIR_F64SQRT:
        _n->f64_sqrt.operand = sir_node_copy(_a, _memo, _src->f64_sqrt.operand);
        break;
    case SIR_F64FLOOR:
        _n->f64_floor.operand = sir_node_copy(_a, _memo, _src->f64_floor.operand);
        break;
    case SIR_F64CEIL:
        _n->f64_ceil.operand = sir_node_copy(_a, _memo, _src->f64_ceil.operand);
        break;
    case SIR_F64NEAREST:
        _n->f64_nearest.operand = sir_node_copy(_a, _memo, _src->f64_nearest.operand);
        break;
    case SIR_EQ:
        _n->eq.left = sir_node_copy(_a, _memo, _src->eq.left);
        _n->eq.right = sir_node_copy(_a, _memo, _src->eq.right);
        break;
    case SIR_NE:
        _n->ne.left = sir_node_copy(_a, _memo, _src->ne.left);
        _n->ne.right = sir_node_copy(_a, _memo, _src->ne.right);
        break;
    case SIR_LT:
        _n->lt.left = sir_node_copy(_a, _memo, _src->lt.left);
        _n->lt.right = sir_node_copy(_a, _memo, _src->lt.right);
        break;
    case SIR_LE:
        _n->le.left = sir_node_copy(_a, _memo, _src->le.left);
        _n->le.right = sir_node_copy(_a, _memo, _src->le.right);
        break;
    case SIR_GT:
        _n->gt.left = sir_node_copy(_a, _memo, _src->gt.left);
        _n->gt.right = sir_node_copy(_a, _memo, _src->gt.right);
        break;
    case SIR_GE:
        _n->ge.left = sir_node_copy(_a, _memo, _src->ge.left);
        _n->ge.right = sir_node_copy(_a, _memo, _src->ge.right);
        break;
    case SIR_CLASSINSTANTIABLE:
        _n->class_instantiable.cls = sir_node_copy(_a, _memo, _src->class_instantiable.cls);
        break;
    case SIR_CLASSCONSTRUCT:
        _n->class_construct.cls = sir_node_copy(_a, _memo, _src->class_construct.cls);
        break;
    case SIR_NEW:
        _n->new_.class_id = _src->new_.class_id;
        break;
    case SIR_INSTANCEOF:
        _n->instance_of.obj = sir_node_copy(_a, _memo, _src->instance_of.obj);
        _n->instance_of.atype = _src->instance_of.atype;
        _n->instance_of.class_id = _src->instance_of.class_id;
        break;
    case SIR_CHECKCAST:
        _n->check_cast.obj = sir_node_copy(_a, _memo, _src->check_cast.obj);
        _n->check_cast.atype = _src->check_cast.atype;
        _n->check_cast.class_id = _src->check_cast.class_id;
        break;
    case SIR_NEWARRAY:
        _n->new_array.elem_type = _src->new_array.elem_type;
        _n->new_array.size = sir_node_copy(_a, _memo, _src->new_array.size);
        break;
    case SIR_NEWREFARRAY:
        _n->new_ref_array.class_id = _src->new_ref_array.class_id;
        _n->new_ref_array.size = sir_node_copy(_a, _memo, _src->new_ref_array.size);
        _n->new_ref_array.elem_ref = sir_node_copy(_a, _memo, _src->new_ref_array.elem_ref);
        break;
    case SIR_ARRAYLOAD:
        _n->array_load.data_type = _src->array_load.data_type;
        _n->array_load.arr = sir_node_copy(_a, _memo, _src->array_load.arr);
        _n->array_load.index = sir_node_copy(_a, _memo, _src->array_load.index);
        _n->array_load.elem_ref = sir_node_copy(_a, _memo, _src->array_load.elem_ref);
        break;
    case SIR_MEMLOAD8:
        _n->mem_load8.addr = sir_node_copy(_a, _memo, _src->mem_load8.addr);
        break;
    case SIR_ARRAYLENGTH:
        _n->array_length.arr = sir_node_copy(_a, _memo, _src->array_length.arr);
        break;
    case SIR_GETFIELD:
        _n->get_field.data_type = _src->get_field.data_type;
        _n->get_field.obj = sir_node_copy(_a, _memo, _src->get_field.obj);
        _n->get_field.class_id = _src->get_field.class_id;
        _n->get_field.field_idx = _src->get_field.field_idx;
        break;
    case SIR_GETSTATIC:
        _n->get_static.data_type = _src->get_static.data_type;
        _n->get_static.class_id = _src->get_static.class_id;
        _n->get_static.field_idx = _src->get_static.field_idx;
        break;
    case SIR_INVOKEVIRTUAL:
        _n->invoke_virtual.obj = sir_node_copy(_a, _memo, _src->invoke_virtual.obj);
        _n->invoke_virtual.class_id = _src->invoke_virtual.class_id;
        _n->invoke_virtual.method_idx = _src->invoke_virtual.method_idx;
        _n->invoke_virtual.args_count = _src->invoke_virtual.args_count;
        _n->invoke_virtual.args = (sir_node_t**)bbq_arena_alloc(_a, sizeof(sir_node_t*) * (size_t)(_src->invoke_virtual.args_count));
        for (int _i = 0; _i < _src->invoke_virtual.args_count; _i++)
            _n->invoke_virtual.args[_i] = sir_node_copy(_a, _memo, _src->invoke_virtual.args[_i]);
        _n->invoke_virtual.return_type = _src->invoke_virtual.return_type;
        break;
    case SIR_INVOKESPECIAL:
        _n->invoke_special.obj = sir_node_copy(_a, _memo, _src->invoke_special.obj);
        _n->invoke_special.class_id = _src->invoke_special.class_id;
        _n->invoke_special.method_idx = _src->invoke_special.method_idx;
        _n->invoke_special.args_count = _src->invoke_special.args_count;
        _n->invoke_special.args = (sir_node_t**)bbq_arena_alloc(_a, sizeof(sir_node_t*) * (size_t)(_src->invoke_special.args_count));
        for (int _i = 0; _i < _src->invoke_special.args_count; _i++)
            _n->invoke_special.args[_i] = sir_node_copy(_a, _memo, _src->invoke_special.args[_i]);
        _n->invoke_special.return_type = _src->invoke_special.return_type;
        break;
    case SIR_INVOKESTATIC:
        _n->invoke_static.class_id = _src->invoke_static.class_id;
        _n->invoke_static.method_idx = _src->invoke_static.method_idx;
        _n->invoke_static.args_count = _src->invoke_static.args_count;
        _n->invoke_static.args = (sir_node_t**)bbq_arena_alloc(_a, sizeof(sir_node_t*) * (size_t)(_src->invoke_static.args_count));
        for (int _i = 0; _i < _src->invoke_static.args_count; _i++)
            _n->invoke_static.args[_i] = sir_node_copy(_a, _memo, _src->invoke_static.args[_i]);
        _n->invoke_static.return_type = _src->invoke_static.return_type;
        break;
    case SIR_INVOKEINTERFACE:
        _n->invoke_interface.obj = sir_node_copy(_a, _memo, _src->invoke_interface.obj);
        _n->invoke_interface.class_id = _src->invoke_interface.class_id;
        _n->invoke_interface.method_idx = _src->invoke_interface.method_idx;
        _n->invoke_interface.args_count = _src->invoke_interface.args_count;
        _n->invoke_interface.args = (sir_node_t**)bbq_arena_alloc(_a, sizeof(sir_node_t*) * (size_t)(_src->invoke_interface.args_count));
        for (int _i = 0; _i < _src->invoke_interface.args_count; _i++)
            _n->invoke_interface.args[_i] = sir_node_copy(_a, _memo, _src->invoke_interface.args[_i]);
        _n->invoke_interface.return_type = _src->invoke_interface.return_type;
        break;
    case SIR_STORELOCAL:
        _n->store_local.slot = _src->store_local.slot;
        _n->store_local.data_type = _src->store_local.data_type;
        _n->store_local.ref_type = sir_node_copy(_a, _memo, _src->store_local.ref_type);
        _n->store_local.value = sir_node_copy(_a, _memo, _src->store_local.value);
        _n->store_local.next = sir_node_copy(_a, _memo, _src->store_local.next);
        break;
    case SIR_EXPREFFECT:
        _n->expr_effect.value = sir_node_copy(_a, _memo, _src->expr_effect.value);
        _n->expr_effect.is_void = _src->expr_effect.is_void;
        _n->expr_effect.next = sir_node_copy(_a, _memo, _src->expr_effect.next);
        break;
    case SIR_ARRAYSTORE:
        _n->array_store.data_type = _src->array_store.data_type;
        _n->array_store.arr = sir_node_copy(_a, _memo, _src->array_store.arr);
        _n->array_store.index = sir_node_copy(_a, _memo, _src->array_store.index);
        _n->array_store.value = sir_node_copy(_a, _memo, _src->array_store.value);
        _n->array_store.next = sir_node_copy(_a, _memo, _src->array_store.next);
        _n->array_store.elem_ref = sir_node_copy(_a, _memo, _src->array_store.elem_ref);
        break;
    case SIR_PUTFIELD:
        _n->put_field.data_type = _src->put_field.data_type;
        _n->put_field.obj = sir_node_copy(_a, _memo, _src->put_field.obj);
        _n->put_field.class_id = _src->put_field.class_id;
        _n->put_field.field_idx = _src->put_field.field_idx;
        _n->put_field.value = sir_node_copy(_a, _memo, _src->put_field.value);
        _n->put_field.next = sir_node_copy(_a, _memo, _src->put_field.next);
        break;
    case SIR_SETHEADER:
        _n->set_header.obj = sir_node_copy(_a, _memo, _src->set_header.obj);
        _n->set_header.value = sir_node_copy(_a, _memo, _src->set_header.value);
        _n->set_header.struct_class_id = _src->set_header.struct_class_id;
        _n->set_header.next = sir_node_copy(_a, _memo, _src->set_header.next);
        break;
    case SIR_ARRAYCOPY:
        _n->array_copy.width = _src->array_copy.width;
        _n->array_copy.dst = sir_node_copy(_a, _memo, _src->array_copy.dst);
        _n->array_copy.dst_off = sir_node_copy(_a, _memo, _src->array_copy.dst_off);
        _n->array_copy.src = sir_node_copy(_a, _memo, _src->array_copy.src);
        _n->array_copy.src_off = sir_node_copy(_a, _memo, _src->array_copy.src_off);
        _n->array_copy.len = sir_node_copy(_a, _memo, _src->array_copy.len);
        _n->array_copy.next = sir_node_copy(_a, _memo, _src->array_copy.next);
        break;
    case SIR_PUTSTATIC:
        _n->put_static.data_type = _src->put_static.data_type;
        _n->put_static.class_id = _src->put_static.class_id;
        _n->put_static.field_idx = _src->put_static.field_idx;
        _n->put_static.value = sir_node_copy(_a, _memo, _src->put_static.value);
        _n->put_static.next = sir_node_copy(_a, _memo, _src->put_static.next);
        break;
    case SIR_MEMSTORE8:
        _n->mem_store8.addr = sir_node_copy(_a, _memo, _src->mem_store8.addr);
        _n->mem_store8.value = sir_node_copy(_a, _memo, _src->mem_store8.value);
        _n->mem_store8.next = sir_node_copy(_a, _memo, _src->mem_store8.next);
        break;
    case SIR_BRANCH:
        _n->branch.cond = sir_node_copy(_a, _memo, _src->branch.cond);
        _n->branch.on_true = sir_node_copy(_a, _memo, _src->branch.on_true);
        _n->branch.on_false = sir_node_copy(_a, _memo, _src->branch.on_false);
        break;
    case SIR_SWITCH:
        _n->switch_.selector = sir_node_copy(_a, _memo, _src->switch_.selector);
        _n->switch_.case_targets_count = _src->switch_.case_targets_count;
        _n->switch_.case_targets = (sir_node_t**)bbq_arena_alloc(_a, sizeof(sir_node_t*) * (size_t)(_src->switch_.case_targets_count));
        for (int _i = 0; _i < _src->switch_.case_targets_count; _i++)
            _n->switch_.case_targets[_i] = sir_node_copy(_a, _memo, _src->switch_.case_targets[_i]);
        _n->switch_.case_values_count = _src->switch_.case_values_count;
        _n->switch_.case_values = (int32_t*)bbq_arena_alloc(_a, sizeof(int32_t) * (size_t)(_src->switch_.case_values_count));
        for (int _i = 0; _i < _src->switch_.case_values_count; _i++)
            _n->switch_.case_values[_i] = _src->switch_.case_values[_i];
        _n->switch_.default_target = sir_node_copy(_a, _memo, _src->switch_.default_target);
        _n->switch_.selector_type = _src->switch_.selector_type;
        break;
    case SIR_RETURN:
        _n->return_.value = sir_node_copy(_a, _memo, _src->return_.value);
        _n->return_.data_type = _src->return_.data_type;
        break;
    case SIR_RETURNVOID:
        break;
    case SIR_THROW:
        _n->throw_.ref = sir_node_copy(_a, _memo, _src->throw_.ref);
        break;
    case SIR_EXCEPTIONENTRY:
        _n->exception_entry.local_slot = _src->exception_entry.local_slot;
        _n->exception_entry.catch_class_id = _src->exception_entry.catch_class_id;
        _n->exception_entry.next = sir_node_copy(_a, _memo, _src->exception_entry.next);
        break;
    case SIR_TRYREGION:
        _n->try_region.handler = sir_node_copy(_a, _memo, _src->try_region.handler);
        _n->try_region.next = sir_node_copy(_a, _memo, _src->try_region.next);
        break;
    case SIR_INC:
        _n->inc.slot = _src->inc.slot;
        _n->inc.delta = _src->inc.delta;
        _n->inc.data_type = _src->inc.data_type;
        _n->inc.value = sir_node_copy(_a, _memo, _src->inc.value);
        _n->inc.next = sir_node_copy(_a, _memo, _src->inc.next);
        break;
    case SIR_NOP:
        _n->nop.next = sir_node_copy(_a, _memo, _src->nop.next);
        break;
    case SIR_CLASSREF:
        _n->class_ref.class_id = _src->class_ref.class_id;
        break;
    case SIR_ARRAYREF:
        _n->array_ref.class_id = _src->array_ref.class_id;
        _n->array_ref.dim = _src->array_ref.dim;
        break;
    case SIR_PRIMARRAY:
        _n->prim_array.width = _src->prim_array.width;
        _n->prim_array.dim = _src->prim_array.dim;
        break;
    default: break;
    }
    _n->exc = _src->exc;   /* attributes copy SHALLOW — stamps, not owned subtrees (previously left UNINITIALIZED on copies) */
    return _n;
}

#endif /* SIR_AST_H */
