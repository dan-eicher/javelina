/* sir_support.h — BURS node interface + CPS continuation access */
#ifndef SIR_SUPPORT_H
#define SIR_SUPPORT_H

#include "gen/sir_ast.h"

/* ── CPS continuation get/set ─────────────────────────────── */

static inline sir_node_t* sir_get_next(const sir_node_t* n) {
    if (!n) return NULL;
    switch (n->tag) {
    case SIR_STORELOCAL:     return n->store_local.next;
    case SIR_EXPREFFECT:     return n->expr_effect.next;
    case SIR_ARRAYSTORE:     return n->array_store.next;
    case SIR_ARRAYCOPY:      return n->array_copy.next;
    case SIR_SIMDMEMSTORE:   return n->simd_mem_store.next;
    case SIR_SIMDMEMSTORELANE: return n->simd_mem_store_lane.next;
    case SIR_MEMSTOREI:      return n->mem_store_i.next;
    case SIR_MEMSTOREL:      return n->mem_store_l.next;
    case SIR_MEMSTOREF:      return n->mem_store_f.next;
    case SIR_MEMSTORED:      return n->mem_store_d.next;
    case SIR_MEMFILL:        return n->mem_fill.next;
    case SIR_MEMCOPY:        return n->mem_copy.next;
    case SIR_PUTFIELD:       return n->put_field.next;
    case SIR_SETHEADER:      return n->set_header.next;
    case SIR_PUTSTATIC:      return n->put_static.next;
    case SIR_EXCEPTIONENTRY: return n->exception_entry.next;
    case SIR_INC:            return n->inc.next;
    case SIR_NOP:            return n->nop.next;
    case SIR_TRYREGION:      return n->try_region.next;
    default: return NULL;
    }
}

static inline void sir_set_next(sir_node_t* n, sir_node_t* next) {
    if (!n) return;
    switch (n->tag) {
    case SIR_STORELOCAL:     n->store_local.next = next; break;
    case SIR_EXPREFFECT:     n->expr_effect.next = next; break;
    case SIR_ARRAYSTORE:     n->array_store.next = next; break;
    case SIR_ARRAYCOPY:      n->array_copy.next = next; break;
    case SIR_SIMDMEMSTORE:   n->simd_mem_store.next = next; break;
    case SIR_SIMDMEMSTORELANE: n->simd_mem_store_lane.next = next; break;
    case SIR_MEMSTOREI:      n->mem_store_i.next = next; break;
    case SIR_MEMSTOREL:      n->mem_store_l.next = next; break;
    case SIR_MEMSTOREF:      n->mem_store_f.next = next; break;
    case SIR_MEMSTORED:      n->mem_store_d.next = next; break;
    case SIR_MEMFILL:        n->mem_fill.next = next; break;
    case SIR_MEMCOPY:        n->mem_copy.next = next; break;
    case SIR_PUTFIELD:       n->put_field.next = next; break;
    case SIR_SETHEADER:      n->set_header.next = next; break;
    case SIR_PUTSTATIC:      n->put_static.next = next; break;
    case SIR_EXCEPTIONENTRY: n->exception_entry.next = next; break;
    case SIR_INC:            n->inc.next = next; break;
    case SIR_NOP:            n->nop.next = next; break;
    case SIR_TRYREGION:      n->try_region.next = next; break;
    default: break;
    }
}

/* True for the six distinct comparison nodes (Eq/Ne/Lt/Le/Gt/Ge) — the
 * operator is the tag, so this is the "is a comparison" test. */
static inline bool sir_is_cmp(const sir_node_t* n) {
    if (!n) return false;
    switch (n->tag) {
        case SIR_EQ: case SIR_NE: case SIR_LT:
        case SIR_LE: case SIR_GT: case SIR_GE: return true;
        default: return false;
    }
}
static inline bool sir_tag_is_cmp(int tag) {
    switch (tag) {
        case SIR_EQ: case SIR_NE: case SIR_LT:
        case SIR_LE: case SIR_GT: case SIR_GE: return true;
        default: return false;
    }
}

/* Case-label list so optimizer switches handle all six comparison nodes
 * uniformly (they share the {left,right} shape). */
#define SIR_CMP_CASES case SIR_EQ: case SIR_NE: case SIR_LT: \
                      case SIR_LE: case SIR_GT: case SIR_GE:

/* Mutable i-th child slot of a comparison node — restores the uniform
 * left/right access the optimizer's rewrite walk needs (the distinct nodes
 * have per-tag union members). */
static inline sir_node_t** sir_cmp_child_slot(sir_node_t* n, int i) {
    switch (n->tag) {
        case SIR_EQ: return i == 0 ? &n->eq.left : &n->eq.right;
        case SIR_NE: return i == 0 ? &n->ne.left : &n->ne.right;
        case SIR_LT: return i == 0 ? &n->lt.left : &n->lt.right;
        case SIR_LE: return i == 0 ? &n->le.left : &n->le.right;
        case SIR_GT: return i == 0 ? &n->gt.left : &n->gt.right;
        case SIR_GE: return i == 0 ? &n->ge.left : &n->ge.right;
        default: return NULL;
    }
}

/* All conversion-SHAPED nodes — unary, pure-if-operand-pure, operand
 * in `.operand`: the §5.1 primitive conversions plus the bit-preserving
 * Move* reinterprets and the f64 math intrinsics. One list so a new
 * conversion node is wired into every optimizer traversal by adding it
 * here + to sir_conv_operand_slot, not by hand in each switch. */
#define SIR_CONV_CASES \
    case SIR_S2B: case SIR_S2I: case SIR_I2S: case SIR_I2B: \
    case SIR_I2C: case SIR_I2L: case SIR_I2F: case SIR_I2D: \
    case SIR_L2I: case SIR_L2F: case SIR_L2D: \
    case SIR_F2I: case SIR_F2L: case SIR_F2D: \
    case SIR_D2I: case SIR_D2L: case SIR_D2F: \
    case SIR_MOVEF2I: case SIR_MOVEI2F: case SIR_MOVED2L: case SIR_MOVEL2D: \
    case SIR_F64SQRT: case SIR_F64FLOOR: case SIR_F64CEIL: case SIR_F64NEAREST:

static inline sir_node_t** sir_conv_operand_slot(sir_node_t* n) {
    switch (n->tag) {
        case SIR_S2B: return &n->s2_b.operand;
        case SIR_S2I: return &n->s2_i.operand;
        case SIR_I2S: return &n->i2_s.operand;
        case SIR_I2B: return &n->i2_b.operand;
        case SIR_I2C: return &n->i2_c.operand;
        case SIR_I2L: return &n->i2_l.operand;
        case SIR_I2F: return &n->i2_f.operand;
        case SIR_I2D: return &n->i2_d.operand;
        case SIR_L2I: return &n->l2_i.operand;
        case SIR_L2F: return &n->l2_f.operand;
        case SIR_L2D: return &n->l2_d.operand;
        case SIR_F2I: return &n->f2_i.operand;
        case SIR_F2L: return &n->f2_l.operand;
        case SIR_F2D: return &n->f2_d.operand;
        case SIR_D2I: return &n->d2_i.operand;
        case SIR_D2L: return &n->d2_l.operand;
        case SIR_D2F: return &n->d2_f.operand;
        case SIR_MOVEF2I: return &n->move_f2_i.operand;
        case SIR_MOVEI2F: return &n->move_i2_f.operand;
        case SIR_MOVED2L: return &n->move_d2_l.operand;
        case SIR_MOVEL2D: return &n->move_l2_d.operand;
        case SIR_F64SQRT: return &n->f64_sqrt.operand;
        case SIR_F64FLOOR: return &n->f64_floor.operand;
        case SIR_F64CEIL: return &n->f64_ceil.operand;
        case SIR_F64NEAREST: return &n->f64_nearest.operand;
        default: return NULL;
    }
}

/* ── BURS data children ───────────────────────────────────── */

static inline int sir_arity(const sir_node_t* n) {
    if (!n) return 0;
    switch (n->tag) {
    /* 0 children */
    case SIR_LOADCONST: case SIR_LOADNULL: case SIR_LOADLOCAL:
    case SIR_LOADCLASS:
    case SIR_NEW: case SIR_GETSTATIC:
    case SIR_RETURNVOID: case SIR_NOP:
    case SIR_EXCEPTIONENTRY: case SIR_TRYREGION:
    case SIR_ARRAYNEWDATA:  /* segment/offset/count are immediates — the tile emits the operands */
        return 0;
    /* 1 child */
    case SIR_NEG: case SIR_LOGNOT:
    case SIR_S2B: case SIR_S2I: case SIR_I2S: case SIR_I2B:
    case SIR_I2C: case SIR_I2L: case SIR_I2F: case SIR_I2D:
    case SIR_L2I: case SIR_L2F: case SIR_L2D:
    case SIR_F2I: case SIR_F2L: case SIR_F2D:
    case SIR_D2I: case SIR_D2L: case SIR_D2F:
    case SIR_MOVEF2I: case SIR_MOVEI2F: case SIR_MOVED2L: case SIR_MOVEL2D:
    case SIR_F64SQRT: case SIR_F64FLOOR: case SIR_F64CEIL: case SIR_F64NEAREST:
    case SIR_INSTANCEOF: case SIR_CHECKCAST:
    case SIR_ARRAYLENGTH:
    case SIR_NEWARRAY: case SIR_NEWREFARRAY:
    case SIR_GETFIELD:  /* only `obj` is a tree child */
    case SIR_STORELOCAL: case SIR_EXPREFFECT: case SIR_PUTSTATIC:
    case SIR_BRANCH: case SIR_RETURN: case SIR_THROW:
    case SIR_SWITCH:  /* only the selector is a tree child */
    case SIR_INC:     /* `value` is the LoadLocal expression for the read side */
    case SIR_SIMDMEMLOAD:  /* the linear-memory address */
    case SIR_MEMLOADI: case SIR_MEMLOADL: case SIR_MEMLOADF: case SIR_MEMLOADD:
    case SIR_MEMGROW:   /* the page count */
    case SIR_CLASSINSTANTIABLE: case SIR_CLASSCONSTRUCT:  /* the Class reference */
    case SIR_SIMDUN: case SIR_SIMDTESTI:
    case SIR_SIMDSPLATI: case SIR_SIMDSPLATL: case SIR_SIMDSPLATF: case SIR_SIMDSPLATD:
    case SIR_SIMDEXTRACTI: case SIR_SIMDEXTRACTL: case SIR_SIMDEXTRACTF: case SIR_SIMDEXTRACTD:
        return 1;
    /* 2 children */
    case SIR_ADD: case SIR_SUB: case SIR_MUL: case SIR_DIV: case SIR_REM:
    case SIR_AND: case SIR_OR: case SIR_XOR: case SIR_SHL: case SIR_SHR: case SIR_USHR:
    case SIR_EQ: case SIR_NE: case SIR_LT: case SIR_LE: case SIR_GT: case SIR_GE:
    case SIR_ARRAYLOAD:
    case SIR_PUTFIELD:
    case SIR_SETHEADER:
    case SIR_SIMDMEMSTORE:  /* addr, value (next is the spine successor) */
    case SIR_MEMSTOREI: case SIR_MEMSTOREL: case SIR_MEMSTOREF: case SIR_MEMSTORED:
    case SIR_SIMDMEMLOADLANE:  /* addr, vec */
    case SIR_SIMDMEMSTORELANE: /* addr, vec (next is the spine successor) */
    case SIR_SIMDBIN: case SIR_SIMDSHIFT: case SIR_SIMDSHUFFLE:
    case SIR_SIMDREPLACEI: case SIR_SIMDREPLACEL: case SIR_SIMDREPLACEF: case SIR_SIMDREPLACED:
        return 2;
    /* 3 children */
    case SIR_ARRAYSTORE:
    case SIR_SIMDTERN:
    case SIR_MEMFILL:   /* dst, value, len (next is the spine successor) */
    case SIR_MEMCOPY:   /* dst, src, len (next is the spine successor) */
        return 3;
    /* 5 children (dst, dst_off, src, src_off, len) */
    case SIR_ARRAYCOPY:
        return 5;
    /* Variable — child count uses args_count (the ASDL-generated
     * length of the *args array). */
    case SIR_INVOKEVIRTUAL:   return 1 + n->invoke_virtual.args_count;
    case SIR_INVOKESPECIAL:   return 1 + n->invoke_special.args_count;
    case SIR_INVOKESTATIC:    return n->invoke_static.args_count;
    case SIR_INVOKEINTERFACE: return 1 + n->invoke_interface.args_count;
    default: return 0;
    }
}

static inline sir_node_t* sir_child(const sir_node_t* n, int i) {
    if (!n) return NULL;
    switch (n->tag) {
    /* Unary */
    case SIR_NEG:    return n->neg.operand;
    case SIR_LOGNOT: return n->log_not.operand;
    case SIR_S2B: return n->s2_b.operand;
    case SIR_S2I: return n->s2_i.operand;
    case SIR_I2S: return n->i2_s.operand;
    case SIR_I2B: return n->i2_b.operand;
    case SIR_I2C: return n->i2_c.operand;
    case SIR_I2L: return n->i2_l.operand;
    case SIR_I2F: return n->i2_f.operand;
    case SIR_I2D: return n->i2_d.operand;
    case SIR_L2I: return n->l2_i.operand;
    case SIR_L2F: return n->l2_f.operand;
    case SIR_L2D: return n->l2_d.operand;
    case SIR_F2I: return n->f2_i.operand;
    case SIR_F2L: return n->f2_l.operand;
    case SIR_F2D: return n->f2_d.operand;
    case SIR_D2I: return n->d2_i.operand;
    case SIR_D2L: return n->d2_l.operand;
    case SIR_D2F: return n->d2_f.operand;
    case SIR_MOVEF2I: return n->move_f2_i.operand;
    case SIR_MOVEI2F: return n->move_i2_f.operand;
    case SIR_MOVED2L: return n->move_d2_l.operand;
    case SIR_MOVEL2D: return n->move_l2_d.operand;
    case SIR_F64SQRT: return n->f64_sqrt.operand;
    case SIR_F64FLOOR: return n->f64_floor.operand;
    case SIR_F64CEIL: return n->f64_ceil.operand;
    case SIR_F64NEAREST: return n->f64_nearest.operand;
    case SIR_INSTANCEOF: return n->instance_of.obj;
    case SIR_CHECKCAST: return n->check_cast.obj;
    case SIR_ARRAYLENGTH: return n->array_length.arr;
    case SIR_NEWARRAY: return n->new_array.size;
    case SIR_NEWREFARRAY: return n->new_ref_array.size;
    /* Binary */
    case SIR_ADD: return i == 0 ? n->add.left : n->add.right;
    case SIR_SUB: return i == 0 ? n->sub.left : n->sub.right;
    case SIR_MUL: return i == 0 ? n->mul.left : n->mul.right;
    case SIR_DIV: return i == 0 ? n->div.left : n->div.right;
    case SIR_REM: return i == 0 ? n->rem.left : n->rem.right;
    case SIR_AND: return i == 0 ? n->and_.left : n->and_.right;
    case SIR_OR:  return i == 0 ? n->or_.left  : n->or_.right;
    case SIR_XOR: return i == 0 ? n->xor_.left : n->xor_.right;
    case SIR_SHL: return i == 0 ? n->shl.left : n->shl.right;
    case SIR_SHR: return i == 0 ? n->shr.left : n->shr.right;
    case SIR_USHR: return i == 0 ? n->ushr.left : n->ushr.right;
    case SIR_EQ: return i == 0 ? n->eq.left : n->eq.right;
    case SIR_NE: return i == 0 ? n->ne.left : n->ne.right;
    case SIR_LT: return i == 0 ? n->lt.left : n->lt.right;
    case SIR_LE: return i == 0 ? n->le.left : n->le.right;
    case SIR_GT: return i == 0 ? n->gt.left : n->gt.right;
    case SIR_GE: return i == 0 ? n->ge.left : n->ge.right;
    case SIR_ARRAYLOAD: return i == 0 ? n->array_load.arr : n->array_load.index;
    case SIR_GETFIELD: return n->get_field.obj;
    case SIR_CLASSINSTANTIABLE: return n->class_instantiable.cls;
    case SIR_CLASSCONSTRUCT:    return n->class_construct.cls;
    /* SIMD families */
    case SIR_SIMDBIN:      return i == 0 ? n->simd_bin.left : n->simd_bin.right;
    case SIR_SIMDUN:       return n->simd_un.operand;
    case SIR_SIMDSHIFT:    return i == 0 ? n->simd_shift.vec : n->simd_shift.count;
    case SIR_SIMDTERN:
        if (i == 0) return n->simd_tern.a;
        if (i == 1) return n->simd_tern.b;
        return n->simd_tern.c;
    case SIR_SIMDTESTI:    return n->simd_test_i.operand;
    case SIR_SIMDSPLATI:   return n->simd_splat_i.operand;
    case SIR_SIMDSPLATL:   return n->simd_splat_l.operand;
    case SIR_SIMDSPLATF:   return n->simd_splat_f.operand;
    case SIR_SIMDSPLATD:   return n->simd_splat_d.operand;
    case SIR_SIMDEXTRACTI: return n->simd_extract_i.vec;
    case SIR_SIMDEXTRACTL: return n->simd_extract_l.vec;
    case SIR_SIMDEXTRACTF: return n->simd_extract_f.vec;
    case SIR_SIMDEXTRACTD: return n->simd_extract_d.vec;
    case SIR_SIMDREPLACEI: return i == 0 ? n->simd_replace_i.vec : n->simd_replace_i.val;
    case SIR_SIMDREPLACEL: return i == 0 ? n->simd_replace_l.vec : n->simd_replace_l.val;
    case SIR_SIMDREPLACEF: return i == 0 ? n->simd_replace_f.vec : n->simd_replace_f.val;
    case SIR_SIMDREPLACED: return i == 0 ? n->simd_replace_d.vec : n->simd_replace_d.val;
    case SIR_SIMDSHUFFLE:  return i == 0 ? n->simd_shuffle.left : n->simd_shuffle.right;
    case SIR_SIMDMEMLOAD:     return n->simd_mem_load.addr;
    case SIR_SIMDMEMLOADLANE: return i == 0 ? n->simd_mem_load_lane.addr : n->simd_mem_load_lane.vec;
    case SIR_MEMLOADI: return n->mem_load_i.addr;
    case SIR_MEMLOADL: return n->mem_load_l.addr;
    case SIR_MEMLOADF: return n->mem_load_f.addr;
    case SIR_MEMLOADD: return n->mem_load_d.addr;
    case SIR_MEMGROW:  return n->mem_grow.pages;
    /* Stmt data children (NOT the continuation) */
    case SIR_STORELOCAL: return n->store_local.value;
    case SIR_EXPREFFECT: return n->expr_effect.value;
    case SIR_PUTSTATIC:  return n->put_static.value;
    case SIR_INC:        return n->inc.value;
    case SIR_PUTFIELD:   return i == 0 ? n->put_field.obj : n->put_field.value;
    case SIR_SETHEADER:  return i == 0 ? n->set_header.obj : n->set_header.value;
    case SIR_SIMDMEMSTORE:     return i == 0 ? n->simd_mem_store.addr : n->simd_mem_store.value;
    case SIR_SIMDMEMSTORELANE: return i == 0 ? n->simd_mem_store_lane.addr : n->simd_mem_store_lane.vec;
    case SIR_MEMSTOREI: return i == 0 ? n->mem_store_i.addr : n->mem_store_i.value;
    case SIR_MEMSTOREL: return i == 0 ? n->mem_store_l.addr : n->mem_store_l.value;
    case SIR_MEMSTOREF: return i == 0 ? n->mem_store_f.addr : n->mem_store_f.value;
    case SIR_MEMSTORED: return i == 0 ? n->mem_store_d.addr : n->mem_store_d.value;
    case SIR_MEMFILL:
        if (i == 0) return n->mem_fill.dst;
        if (i == 1) return n->mem_fill.value;
        return n->mem_fill.len;
    case SIR_MEMCOPY:
        if (i == 0) return n->mem_copy.dst;
        if (i == 1) return n->mem_copy.src;
        return n->mem_copy.len;
    case SIR_ARRAYSTORE:
        if (i == 0) return n->array_store.arr;
        if (i == 1) return n->array_store.index;
        return n->array_store.value;
    case SIR_ARRAYCOPY:
        if (i == 0) return n->array_copy.dst;
        if (i == 1) return n->array_copy.dst_off;
        if (i == 2) return n->array_copy.src;
        if (i == 3) return n->array_copy.src_off;
        return n->array_copy.len;
    case SIR_BRANCH:     return n->branch.cond;
    case SIR_RETURN:     return n->return_.value;
    case SIR_THROW:      return n->throw_.ref;
    case SIR_SWITCH:     return n->switch_.selector;
    /* Invoke */
    case SIR_INVOKEVIRTUAL:
        return i == 0 ? n->invoke_virtual.obj : n->invoke_virtual.args[i-1];
    case SIR_INVOKESPECIAL:
        return i == 0 ? n->invoke_special.obj : n->invoke_special.args[i-1];
    case SIR_INVOKESTATIC: return n->invoke_static.args[i];
    case SIR_INVOKEINTERFACE:
        return i == 0 ? n->invoke_interface.obj : n->invoke_interface.args[i-1];
    default: return NULL;
    }
}

/* Writable slot of data child i — sir_child's exact mirror, for
 * rewrites that substitute a child in place. The optimizer's generic
 * walkers (child rewrite, CSE substitution) use this so a new opcode
 * is wired in by extending sir_arity/sir_child/sir_child_slot ONCE,
 * never by hand in each engine switch. */
static inline sir_node_t** sir_child_slot(sir_node_t* n, int i) {
    if (!n) return NULL;
    switch (n->tag) {
    case SIR_NEG:    return &n->neg.operand;
    case SIR_LOGNOT: return &n->log_not.operand;
    SIR_CONV_CASES   return sir_conv_operand_slot(n);
    case SIR_INSTANCEOF: return &n->instance_of.obj;
    case SIR_CHECKCAST: return &n->check_cast.obj;
    case SIR_ARRAYLENGTH: return &n->array_length.arr;
    case SIR_NEWARRAY: return &n->new_array.size;
    case SIR_NEWREFARRAY: return &n->new_ref_array.size;
    case SIR_ADD: return i == 0 ? &n->add.left : &n->add.right;
    case SIR_SUB: return i == 0 ? &n->sub.left : &n->sub.right;
    case SIR_MUL: return i == 0 ? &n->mul.left : &n->mul.right;
    case SIR_DIV: return i == 0 ? &n->div.left : &n->div.right;
    case SIR_REM: return i == 0 ? &n->rem.left : &n->rem.right;
    case SIR_AND: return i == 0 ? &n->and_.left : &n->and_.right;
    case SIR_OR:  return i == 0 ? &n->or_.left  : &n->or_.right;
    case SIR_XOR: return i == 0 ? &n->xor_.left : &n->xor_.right;
    case SIR_SHL: return i == 0 ? &n->shl.left : &n->shl.right;
    case SIR_SHR: return i == 0 ? &n->shr.left : &n->shr.right;
    case SIR_USHR: return i == 0 ? &n->ushr.left : &n->ushr.right;
    SIR_CMP_CASES return sir_cmp_child_slot(n, i);
    case SIR_ARRAYLOAD: return i == 0 ? &n->array_load.arr : &n->array_load.index;
    case SIR_GETFIELD: return &n->get_field.obj;
    case SIR_CLASSINSTANTIABLE: return &n->class_instantiable.cls;
    case SIR_CLASSCONSTRUCT:    return &n->class_construct.cls;
    /* SIMD families */
    case SIR_SIMDBIN:      return i == 0 ? &n->simd_bin.left : &n->simd_bin.right;
    case SIR_SIMDUN:       return &n->simd_un.operand;
    case SIR_SIMDSHIFT:    return i == 0 ? &n->simd_shift.vec : &n->simd_shift.count;
    case SIR_SIMDTERN:
        if (i == 0) return &n->simd_tern.a;
        if (i == 1) return &n->simd_tern.b;
        return &n->simd_tern.c;
    case SIR_SIMDTESTI:    return &n->simd_test_i.operand;
    case SIR_SIMDSPLATI:   return &n->simd_splat_i.operand;
    case SIR_SIMDSPLATL:   return &n->simd_splat_l.operand;
    case SIR_SIMDSPLATF:   return &n->simd_splat_f.operand;
    case SIR_SIMDSPLATD:   return &n->simd_splat_d.operand;
    case SIR_SIMDEXTRACTI: return &n->simd_extract_i.vec;
    case SIR_SIMDEXTRACTL: return &n->simd_extract_l.vec;
    case SIR_SIMDEXTRACTF: return &n->simd_extract_f.vec;
    case SIR_SIMDEXTRACTD: return &n->simd_extract_d.vec;
    case SIR_SIMDREPLACEI: return i == 0 ? &n->simd_replace_i.vec : &n->simd_replace_i.val;
    case SIR_SIMDREPLACEL: return i == 0 ? &n->simd_replace_l.vec : &n->simd_replace_l.val;
    case SIR_SIMDREPLACEF: return i == 0 ? &n->simd_replace_f.vec : &n->simd_replace_f.val;
    case SIR_SIMDREPLACED: return i == 0 ? &n->simd_replace_d.vec : &n->simd_replace_d.val;
    case SIR_SIMDSHUFFLE:  return i == 0 ? &n->simd_shuffle.left : &n->simd_shuffle.right;
    case SIR_SIMDMEMLOAD:     return &n->simd_mem_load.addr;
    case SIR_SIMDMEMLOADLANE: return i == 0 ? &n->simd_mem_load_lane.addr : &n->simd_mem_load_lane.vec;
    case SIR_MEMLOADI: return &n->mem_load_i.addr;
    case SIR_MEMLOADL: return &n->mem_load_l.addr;
    case SIR_MEMLOADF: return &n->mem_load_f.addr;
    case SIR_MEMLOADD: return &n->mem_load_d.addr;
    case SIR_MEMGROW:  return &n->mem_grow.pages;
    case SIR_STORELOCAL: return &n->store_local.value;
    case SIR_EXPREFFECT: return &n->expr_effect.value;
    case SIR_PUTSTATIC:  return &n->put_static.value;
    case SIR_INC:        return &n->inc.value;
    case SIR_PUTFIELD:   return i == 0 ? &n->put_field.obj : &n->put_field.value;
    case SIR_SETHEADER:  return i == 0 ? &n->set_header.obj : &n->set_header.value;
    case SIR_SIMDMEMSTORE:     return i == 0 ? &n->simd_mem_store.addr : &n->simd_mem_store.value;
    case SIR_SIMDMEMSTORELANE: return i == 0 ? &n->simd_mem_store_lane.addr : &n->simd_mem_store_lane.vec;
    case SIR_MEMSTOREI: return i == 0 ? &n->mem_store_i.addr : &n->mem_store_i.value;
    case SIR_MEMSTOREL: return i == 0 ? &n->mem_store_l.addr : &n->mem_store_l.value;
    case SIR_MEMSTOREF: return i == 0 ? &n->mem_store_f.addr : &n->mem_store_f.value;
    case SIR_MEMSTORED: return i == 0 ? &n->mem_store_d.addr : &n->mem_store_d.value;
    case SIR_MEMFILL:
        if (i == 0) return &n->mem_fill.dst;
        if (i == 1) return &n->mem_fill.value;
        return &n->mem_fill.len;
    case SIR_MEMCOPY:
        if (i == 0) return &n->mem_copy.dst;
        if (i == 1) return &n->mem_copy.src;
        return &n->mem_copy.len;
    case SIR_ARRAYSTORE:
        if (i == 0) return &n->array_store.arr;
        if (i == 1) return &n->array_store.index;
        return &n->array_store.value;
    case SIR_ARRAYCOPY:
        if (i == 0) return &n->array_copy.dst;
        if (i == 1) return &n->array_copy.dst_off;
        if (i == 2) return &n->array_copy.src;
        if (i == 3) return &n->array_copy.src_off;
        return &n->array_copy.len;
    case SIR_BRANCH:     return &n->branch.cond;
    case SIR_RETURN:     return &n->return_.value;
    case SIR_THROW:      return &n->throw_.ref;
    case SIR_SWITCH:     return &n->switch_.selector;
    case SIR_INVOKEVIRTUAL:
        return i == 0 ? &n->invoke_virtual.obj : &n->invoke_virtual.args[i-1];
    case SIR_INVOKESPECIAL:
        return i == 0 ? &n->invoke_special.obj : &n->invoke_special.args[i-1];
    case SIR_INVOKESTATIC: return &n->invoke_static.args[i];
    case SIR_INVOKEINTERFACE:
        return i == 0 ? &n->invoke_interface.obj : &n->invoke_interface.args[i-1];
    default: return NULL;
    }
}

/* ── BURS successors ──────────────────────────────────────── */

static inline int sir_succ_count(const sir_node_t* n) {
    if (!n) return 0;
    switch (n->tag) {
    case SIR_RETURN: case SIR_RETURNVOID: case SIR_THROW:
        return 0;
    case SIR_BRANCH: return 2;
    case SIR_SWITCH: return n->switch_.case_targets_count + 1;
    case SIR_TRYREGION: return 2;  /* handler, next */
    default:         return sir_get_next(n) ? 1 : 0;
    }
}

static inline sir_node_t* sir_succ(const sir_node_t* n, int i) {
    if (!n) return NULL;
    switch (n->tag) {
    case SIR_RETURN: case SIR_RETURNVOID: case SIR_THROW:
        return NULL;
    case SIR_BRANCH:
        return i == 0 ? n->branch.on_true : n->branch.on_false;
    case SIR_SWITCH:
        if (i < n->switch_.case_targets_count)
            return n->switch_.case_targets[i];
        if (i == n->switch_.case_targets_count)
            return n->switch_.default_target;
        return NULL;
    case SIR_TRYREGION:
        /* succ[0]=handler, succ[1]=next. DFS visits handler first,
         * next (→body) last. Body finishes last → appears first in
         * RPO → emitted first (spec-ordered bytecode layout). */
        return i == 0 ? n->try_region.handler
             : i == 1 ? n->try_region.next : NULL;
    default:
        return i == 0 ? sir_get_next(n) : NULL;
    }
}

/* Write successor `i` of `n` — the write mirror of sir_succ, one switch, so an
 * edit that must catch EVERY edge into a node (a pre-header splice re-pointing
 * all outside predecessors) has one authority to route through instead of a
 * per-tag field list that rots. */
static inline void sir_set_succ(sir_node_t* n, int i, sir_node_t* v) {
    if (!n) return;
    switch (n->tag) {
    case SIR_RETURN: case SIR_RETURNVOID: case SIR_THROW:
        return;
    case SIR_BRANCH:
        if (i == 0) n->branch.on_true = v; else n->branch.on_false = v;
        return;
    case SIR_SWITCH:
        if (i < n->switch_.case_targets_count) n->switch_.case_targets[i] = v;
        else if (i == n->switch_.case_targets_count) n->switch_.default_target = v;
        return;
    case SIR_TRYREGION:
        if (i == 0) n->try_region.handler = v;
        else if (i == 1) n->try_region.next = v;
        return;
    default:
        if (i == 0) sir_set_next(n, v);
        return;
    }
}

/* ── Value accessors ──────────────────────────────────────── */

static inline int32_t sir_const_val(const sir_node_t* n) {
    return (n && n->tag == SIR_LOADCONST) ? n->load_const.value : 0;
}

static inline int32_t sir_local_slot(const sir_node_t* n) {
    if (!n) return -1;
    if (n->tag == SIR_LOADLOCAL)  return n->load_local.slot;
    if (n->tag == SIR_STORELOCAL) return n->store_local.slot;
    return -1;
}

/* ── THE spine collector ──────────────────────────────────────
 *
 * Every spine node reachable from `entry`, exactly once, as a bbq_vec<sir_node_t*> the
 * caller frees.
 *
 * THIS IS THE ONE PLACE THE CONTINUATION EDGES ARE FOLLOWED — "a linear scan of spine[] is
 * not a traversal; following a successor is", and this is the function allowed to cross
 * that line. Every other consumer reads the LIST.
 *
 * It is one function because it was two (the Click engine's and cp_pack's), untested, so
 * the next consumer that needed a spine found it cheaper to write a third than to reuse
 * one. Those are the economics that produce walkers. Pinned: test_sir §35 — both Branch
 * arms, a TryRegion's HANDLER (not just `.next`), a shared join exactly once, a loop back
 * edge terminating, and nothing from inside an expression tree.
 *
 * The order is a DFS preorder. It carries NO meaning: it is a node LIST, not a schedule,
 * and says nothing about dominance or about which node "comes first". */
sir_node_t** sir_collect_spine(sir_node_t* entry);

/* ── BURG_NODE macros ─────────────────────────────────────── */

#define BURG_NODE_TYPE           sir_node_t*
#define BURG_NODE_OP(n)          ((n)->tag)
#define BURG_NODE_ARITY(n)       (sir_arity(n))
#define BURG_NODE_CHILD(n,i)     (sir_child((n),(i)))
#define BURG_NODE_ID(n)          ((void*)(n))
#define BURG_NODE_SUCC_COUNT(n)  (sir_succ_count(n))
#define BURG_NODE_SUCC(n,i)      (sir_succ((n),(i)))

#endif /* SIR_SUPPORT_H */
