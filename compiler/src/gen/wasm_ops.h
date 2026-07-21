/* AUTO-GENERATED from spec/instructions.toml by tools/gen_wasm_ops.py — do not edit.
 * The EMIT-side opcode authority: each WASM instruction's full byte encoding
 * (prefix + opcode), single-byte and 0xFC/0xFD-prefixed uniformly. */
#ifndef WASM_OPS_H
#define WASM_OPS_H

#include <stdint.h>

typedef enum {
    WOP_UNREACHABLE,  /* unreachable */
    WOP_NOP,  /* nop */
    WOP_BLOCK,  /* block */
    WOP_LOOP,  /* loop */
    WOP_IF,  /* if */
    WOP_THROW,  /* throw */
    WOP_THROW_REF,  /* throw_ref */
    WOP_BR,  /* br */
    WOP_BR_IF,  /* br_if */
    WOP_BR_TABLE,  /* br_table */
    WOP_RETURN,  /* return */
    WOP_CALL,  /* call */
    WOP_CALL_INDIRECT,  /* call_indirect */
    WOP_RETURN_CALL,  /* return_call */
    WOP_RETURN_CALL_INDIRECT,  /* return_call_indirect */
    WOP_CALL_REF,  /* call_ref */
    WOP_RETURN_CALL_REF,  /* return_call_ref */
    WOP_DROP,  /* drop */
    WOP_SELECT,  /* select */
    WOP_SELECT_T,  /* select */
    WOP_TRY_TABLE,  /* try_table */
    WOP_LOCAL_GET,  /* local.get */
    WOP_LOCAL_SET,  /* local.set */
    WOP_LOCAL_TEE,  /* local.tee */
    WOP_GLOBAL_GET,  /* global.get */
    WOP_GLOBAL_SET,  /* global.set */
    WOP_TABLE_GET,  /* table.get */
    WOP_TABLE_SET,  /* table.set */
    WOP_I32_LOAD,  /* i32.load */
    WOP_I64_LOAD,  /* i64.load */
    WOP_F32_LOAD,  /* f32.load */
    WOP_F64_LOAD,  /* f64.load */
    WOP_I32_LOAD8_S,  /* i32.load8_s */
    WOP_I32_LOAD8_U,  /* i32.load8_u */
    WOP_I32_LOAD16_S,  /* i32.load16_s */
    WOP_I32_LOAD16_U,  /* i32.load16_u */
    WOP_I64_LOAD8_S,  /* i64.load8_s */
    WOP_I64_LOAD8_U,  /* i64.load8_u */
    WOP_I64_LOAD16_S,  /* i64.load16_s */
    WOP_I64_LOAD16_U,  /* i64.load16_u */
    WOP_I64_LOAD32_S,  /* i64.load32_s */
    WOP_I64_LOAD32_U,  /* i64.load32_u */
    WOP_I32_STORE,  /* i32.store */
    WOP_I64_STORE,  /* i64.store */
    WOP_F32_STORE,  /* f32.store */
    WOP_F64_STORE,  /* f64.store */
    WOP_I32_STORE8,  /* i32.store8 */
    WOP_I32_STORE16,  /* i32.store16 */
    WOP_I64_STORE8,  /* i64.store8 */
    WOP_I64_STORE16,  /* i64.store16 */
    WOP_I64_STORE32,  /* i64.store32 */
    WOP_MEMORY_SIZE,  /* memory.size */
    WOP_MEMORY_GROW,  /* memory.grow */
    WOP_I32_CONST,  /* i32.const */
    WOP_I64_CONST,  /* i64.const */
    WOP_F32_CONST,  /* f32.const */
    WOP_F64_CONST,  /* f64.const */
    WOP_I32_EQZ,  /* i32.eqz */
    WOP_I32_EQ,  /* i32.eq */
    WOP_I32_NE,  /* i32.ne */
    WOP_I32_LT_S,  /* i32.lt_s */
    WOP_I32_LT_U,  /* i32.lt_u */
    WOP_I32_GT_S,  /* i32.gt_s */
    WOP_I32_GT_U,  /* i32.gt_u */
    WOP_I32_LE_S,  /* i32.le_s */
    WOP_I32_LE_U,  /* i32.le_u */
    WOP_I32_GE_S,  /* i32.ge_s */
    WOP_I32_GE_U,  /* i32.ge_u */
    WOP_I64_EQZ,  /* i64.eqz */
    WOP_I64_EQ,  /* i64.eq */
    WOP_I64_NE,  /* i64.ne */
    WOP_I64_LT_S,  /* i64.lt_s */
    WOP_I64_LT_U,  /* i64.lt_u */
    WOP_I64_GT_S,  /* i64.gt_s */
    WOP_I64_GT_U,  /* i64.gt_u */
    WOP_I64_LE_S,  /* i64.le_s */
    WOP_I64_LE_U,  /* i64.le_u */
    WOP_I64_GE_S,  /* i64.ge_s */
    WOP_I64_GE_U,  /* i64.ge_u */
    WOP_F32_EQ,  /* f32.eq */
    WOP_F32_NE,  /* f32.ne */
    WOP_F32_LT,  /* f32.lt */
    WOP_F32_GT,  /* f32.gt */
    WOP_F32_LE,  /* f32.le */
    WOP_F32_GE,  /* f32.ge */
    WOP_F64_EQ,  /* f64.eq */
    WOP_F64_NE,  /* f64.ne */
    WOP_F64_LT,  /* f64.lt */
    WOP_F64_GT,  /* f64.gt */
    WOP_F64_LE,  /* f64.le */
    WOP_F64_GE,  /* f64.ge */
    WOP_I32_CLZ,  /* i32.clz */
    WOP_I32_CTZ,  /* i32.ctz */
    WOP_I32_POPCNT,  /* i32.popcnt */
    WOP_I32_ADD,  /* i32.add */
    WOP_I32_SUB,  /* i32.sub */
    WOP_I32_MUL,  /* i32.mul */
    WOP_I32_DIV_S,  /* i32.div_s */
    WOP_I32_DIV_U,  /* i32.div_u */
    WOP_I32_REM_S,  /* i32.rem_s */
    WOP_I32_REM_U,  /* i32.rem_u */
    WOP_I32_AND,  /* i32.and */
    WOP_I32_OR,  /* i32.or */
    WOP_I32_XOR,  /* i32.xor */
    WOP_I32_SHL,  /* i32.shl */
    WOP_I32_SHR_S,  /* i32.shr_s */
    WOP_I32_SHR_U,  /* i32.shr_u */
    WOP_I32_ROTL,  /* i32.rotl */
    WOP_I32_ROTR,  /* i32.rotr */
    WOP_I64_CLZ,  /* i64.clz */
    WOP_I64_CTZ,  /* i64.ctz */
    WOP_I64_POPCNT,  /* i64.popcnt */
    WOP_I64_ADD,  /* i64.add */
    WOP_I64_SUB,  /* i64.sub */
    WOP_I64_MUL,  /* i64.mul */
    WOP_I64_DIV_S,  /* i64.div_s */
    WOP_I64_DIV_U,  /* i64.div_u */
    WOP_I64_REM_S,  /* i64.rem_s */
    WOP_I64_REM_U,  /* i64.rem_u */
    WOP_I64_AND,  /* i64.and */
    WOP_I64_OR,  /* i64.or */
    WOP_I64_XOR,  /* i64.xor */
    WOP_I64_SHL,  /* i64.shl */
    WOP_I64_SHR_S,  /* i64.shr_s */
    WOP_I64_SHR_U,  /* i64.shr_u */
    WOP_I64_ROTL,  /* i64.rotl */
    WOP_I64_ROTR,  /* i64.rotr */
    WOP_F32_ABS,  /* f32.abs */
    WOP_F32_NEG,  /* f32.neg */
    WOP_F32_CEIL,  /* f32.ceil */
    WOP_F32_FLOOR,  /* f32.floor */
    WOP_F32_TRUNC,  /* f32.trunc */
    WOP_F32_NEAREST,  /* f32.nearest */
    WOP_F32_SQRT,  /* f32.sqrt */
    WOP_F32_ADD,  /* f32.add */
    WOP_F32_SUB,  /* f32.sub */
    WOP_F32_MUL,  /* f32.mul */
    WOP_F32_DIV,  /* f32.div */
    WOP_F32_MIN,  /* f32.min */
    WOP_F32_MAX,  /* f32.max */
    WOP_F32_COPYSIGN,  /* f32.copysign */
    WOP_F64_ABS,  /* f64.abs */
    WOP_F64_NEG,  /* f64.neg */
    WOP_F64_CEIL,  /* f64.ceil */
    WOP_F64_FLOOR,  /* f64.floor */
    WOP_F64_TRUNC,  /* f64.trunc */
    WOP_F64_NEAREST,  /* f64.nearest */
    WOP_F64_SQRT,  /* f64.sqrt */
    WOP_F64_ADD,  /* f64.add */
    WOP_F64_SUB,  /* f64.sub */
    WOP_F64_MUL,  /* f64.mul */
    WOP_F64_DIV,  /* f64.div */
    WOP_F64_MIN,  /* f64.min */
    WOP_F64_MAX,  /* f64.max */
    WOP_F64_COPYSIGN,  /* f64.copysign */
    WOP_I32_WRAP_I64,  /* i32.wrap_i64 */
    WOP_I32_TRUNC_F32_S,  /* i32.trunc_f32_s */
    WOP_I32_TRUNC_F32_U,  /* i32.trunc_f32_u */
    WOP_I32_TRUNC_F64_S,  /* i32.trunc_f64_s */
    WOP_I32_TRUNC_F64_U,  /* i32.trunc_f64_u */
    WOP_I64_EXTEND_I32_S,  /* i64.extend_i32_s */
    WOP_I64_EXTEND_I32_U,  /* i64.extend_i32_u */
    WOP_I64_TRUNC_F32_S,  /* i64.trunc_f32_s */
    WOP_I64_TRUNC_F32_U,  /* i64.trunc_f32_u */
    WOP_I64_TRUNC_F64_S,  /* i64.trunc_f64_s */
    WOP_I64_TRUNC_F64_U,  /* i64.trunc_f64_u */
    WOP_F32_CONVERT_I32_S,  /* f32.convert_i32_s */
    WOP_F32_CONVERT_I32_U,  /* f32.convert_i32_u */
    WOP_F32_CONVERT_I64_S,  /* f32.convert_i64_s */
    WOP_F32_CONVERT_I64_U,  /* f32.convert_i64_u */
    WOP_F32_DEMOTE_F64,  /* f32.demote_f64 */
    WOP_F64_CONVERT_I32_S,  /* f64.convert_i32_s */
    WOP_F64_CONVERT_I32_U,  /* f64.convert_i32_u */
    WOP_F64_CONVERT_I64_S,  /* f64.convert_i64_s */
    WOP_F64_CONVERT_I64_U,  /* f64.convert_i64_u */
    WOP_F64_PROMOTE_F32,  /* f64.promote_f32 */
    WOP_I32_REINTERPRET_F32,  /* i32.reinterpret_f32 */
    WOP_I64_REINTERPRET_F64,  /* i64.reinterpret_f64 */
    WOP_F32_REINTERPRET_I32,  /* f32.reinterpret_i32 */
    WOP_F64_REINTERPRET_I64,  /* f64.reinterpret_i64 */
    WOP_I32_EXTEND8_S,  /* i32.extend8_s */
    WOP_I32_EXTEND16_S,  /* i32.extend16_s */
    WOP_I64_EXTEND8_S,  /* i64.extend8_s */
    WOP_I64_EXTEND16_S,  /* i64.extend16_s */
    WOP_I64_EXTEND32_S,  /* i64.extend32_s */
    WOP_REF_NULL,  /* ref.null */
    WOP_REF_IS_NULL,  /* ref.is_null */
    WOP_REF_FUNC,  /* ref.func */
    WOP_REF_EQ,  /* ref.eq */
    WOP_REF_AS_NON_NULL,  /* ref.as_non_null */
    WOP_BR_ON_NULL,  /* br_on_null */
    WOP_BR_ON_NON_NULL,  /* br_on_non_null */
    WOP_STRUCT_NEW,  /* struct.new */
    WOP_STRUCT_NEW_DEFAULT,  /* struct.new_default */
    WOP_STRUCT_GET,  /* struct.get */
    WOP_STRUCT_GET_S,  /* struct.get_s */
    WOP_STRUCT_GET_U,  /* struct.get_u */
    WOP_STRUCT_SET,  /* struct.set */
    WOP_ARRAY_NEW,  /* array.new */
    WOP_ARRAY_NEW_DEFAULT,  /* array.new_default */
    WOP_ARRAY_NEW_FIXED,  /* array.new_fixed */
    WOP_ARRAY_NEW_DATA,  /* array.new_data */
    WOP_ARRAY_NEW_ELEM,  /* array.new_elem */
    WOP_ARRAY_GET,  /* array.get */
    WOP_ARRAY_GET_S,  /* array.get_s */
    WOP_ARRAY_GET_U,  /* array.get_u */
    WOP_ARRAY_SET,  /* array.set */
    WOP_ARRAY_LEN,  /* array.len */
    WOP_ARRAY_FILL,  /* array.fill */
    WOP_ARRAY_COPY,  /* array.copy */
    WOP_ARRAY_INIT_DATA,  /* array.init_data */
    WOP_ARRAY_INIT_ELEM,  /* array.init_elem */
    WOP_REF_TEST,  /* ref.test */
    WOP_REF_TEST_NULL,  /* ref.test */
    WOP_REF_CAST,  /* ref.cast */
    WOP_REF_CAST_NULL,  /* ref.cast */
    WOP_BR_ON_CAST,  /* br_on_cast */
    WOP_BR_ON_CAST_FAIL,  /* br_on_cast_fail */
    WOP_ANY_CONVERT_EXTERN,  /* any.convert_extern */
    WOP_EXTERN_CONVERT_ANY,  /* extern.convert_any */
    WOP_REF_I31,  /* ref.i31 */
    WOP_I31_GET_S,  /* i31.get_s */
    WOP_I31_GET_U,  /* i31.get_u */
    WOP_I32_TRUNC_SAT_F32_S,  /* i32.trunc_sat_f32_s */
    WOP_I32_TRUNC_SAT_F32_U,  /* i32.trunc_sat_f32_u */
    WOP_I32_TRUNC_SAT_F64_S,  /* i32.trunc_sat_f64_s */
    WOP_I32_TRUNC_SAT_F64_U,  /* i32.trunc_sat_f64_u */
    WOP_I64_TRUNC_SAT_F32_S,  /* i64.trunc_sat_f32_s */
    WOP_I64_TRUNC_SAT_F32_U,  /* i64.trunc_sat_f32_u */
    WOP_I64_TRUNC_SAT_F64_S,  /* i64.trunc_sat_f64_s */
    WOP_I64_TRUNC_SAT_F64_U,  /* i64.trunc_sat_f64_u */
    WOP_MEMORY_INIT,  /* memory.init */
    WOP_DATA_DROP,  /* data.drop */
    WOP_MEMORY_COPY,  /* memory.copy */
    WOP_MEMORY_FILL,  /* memory.fill */
    WOP_TABLE_INIT,  /* table.init */
    WOP_ELEM_DROP,  /* elem.drop */
    WOP_TABLE_COPY,  /* table.copy */
    WOP_TABLE_GROW,  /* table.grow */
    WOP_TABLE_SIZE,  /* table.size */
    WOP_TABLE_FILL,  /* table.fill */
    WOP_V128_LOAD,  /* v128.load */
    WOP_V128_LOAD8X8_S,  /* v128.load8x8_s */
    WOP_V128_LOAD8X8_U,  /* v128.load8x8_u */
    WOP_V128_LOAD16X4_S,  /* v128.load16x4_s */
    WOP_V128_LOAD16X4_U,  /* v128.load16x4_u */
    WOP_V128_LOAD32X2_S,  /* v128.load32x2_s */
    WOP_V128_LOAD32X2_U,  /* v128.load32x2_u */
    WOP_V128_LOAD8_SPLAT,  /* v128.load8_splat */
    WOP_V128_LOAD16_SPLAT,  /* v128.load16_splat */
    WOP_V128_LOAD32_SPLAT,  /* v128.load32_splat */
    WOP_V128_LOAD64_SPLAT,  /* v128.load64_splat */
    WOP_V128_STORE,  /* v128.store */
    WOP_V128_CONST,  /* v128.const */
    WOP_I8X16_SHUFFLE,  /* i8x16.shuffle */
    WOP_I8X16_SWIZZLE,  /* i8x16.swizzle */
    WOP_I8X16_SPLAT,  /* i8x16.splat */
    WOP_I16X8_SPLAT,  /* i16x8.splat */
    WOP_I32X4_SPLAT,  /* i32x4.splat */
    WOP_I64X2_SPLAT,  /* i64x2.splat */
    WOP_F32X4_SPLAT,  /* f32x4.splat */
    WOP_F64X2_SPLAT,  /* f64x2.splat */
    WOP_I8X16_EXTRACT_LANE_S,  /* i8x16.extract_lane_s */
    WOP_I8X16_EXTRACT_LANE_U,  /* i8x16.extract_lane_u */
    WOP_I8X16_REPLACE_LANE,  /* i8x16.replace_lane */
    WOP_I16X8_EXTRACT_LANE_S,  /* i16x8.extract_lane_s */
    WOP_I16X8_EXTRACT_LANE_U,  /* i16x8.extract_lane_u */
    WOP_I16X8_REPLACE_LANE,  /* i16x8.replace_lane */
    WOP_I32X4_EXTRACT_LANE,  /* i32x4.extract_lane */
    WOP_I32X4_REPLACE_LANE,  /* i32x4.replace_lane */
    WOP_I64X2_EXTRACT_LANE,  /* i64x2.extract_lane */
    WOP_I64X2_REPLACE_LANE,  /* i64x2.replace_lane */
    WOP_F32X4_EXTRACT_LANE,  /* f32x4.extract_lane */
    WOP_F32X4_REPLACE_LANE,  /* f32x4.replace_lane */
    WOP_F64X2_EXTRACT_LANE,  /* f64x2.extract_lane */
    WOP_F64X2_REPLACE_LANE,  /* f64x2.replace_lane */
    WOP_I8X16_EQ,  /* i8x16.eq */
    WOP_I8X16_NE,  /* i8x16.ne */
    WOP_I8X16_LT_S,  /* i8x16.lt_s */
    WOP_I8X16_LT_U,  /* i8x16.lt_u */
    WOP_I8X16_GT_S,  /* i8x16.gt_s */
    WOP_I8X16_GT_U,  /* i8x16.gt_u */
    WOP_I8X16_LE_S,  /* i8x16.le_s */
    WOP_I8X16_LE_U,  /* i8x16.le_u */
    WOP_I8X16_GE_S,  /* i8x16.ge_s */
    WOP_I8X16_GE_U,  /* i8x16.ge_u */
    WOP_I16X8_EQ,  /* i16x8.eq */
    WOP_I16X8_NE,  /* i16x8.ne */
    WOP_I16X8_LT_S,  /* i16x8.lt_s */
    WOP_I16X8_LT_U,  /* i16x8.lt_u */
    WOP_I16X8_GT_S,  /* i16x8.gt_s */
    WOP_I16X8_GT_U,  /* i16x8.gt_u */
    WOP_I16X8_LE_S,  /* i16x8.le_s */
    WOP_I16X8_LE_U,  /* i16x8.le_u */
    WOP_I16X8_GE_S,  /* i16x8.ge_s */
    WOP_I16X8_GE_U,  /* i16x8.ge_u */
    WOP_I32X4_EQ,  /* i32x4.eq */
    WOP_I32X4_NE,  /* i32x4.ne */
    WOP_I32X4_LT_S,  /* i32x4.lt_s */
    WOP_I32X4_LT_U,  /* i32x4.lt_u */
    WOP_I32X4_GT_S,  /* i32x4.gt_s */
    WOP_I32X4_GT_U,  /* i32x4.gt_u */
    WOP_I32X4_LE_S,  /* i32x4.le_s */
    WOP_I32X4_LE_U,  /* i32x4.le_u */
    WOP_I32X4_GE_S,  /* i32x4.ge_s */
    WOP_I32X4_GE_U,  /* i32x4.ge_u */
    WOP_F32X4_EQ,  /* f32x4.eq */
    WOP_F32X4_NE,  /* f32x4.ne */
    WOP_F32X4_LT,  /* f32x4.lt */
    WOP_F32X4_GT,  /* f32x4.gt */
    WOP_F32X4_LE,  /* f32x4.le */
    WOP_F32X4_GE,  /* f32x4.ge */
    WOP_F64X2_EQ,  /* f64x2.eq */
    WOP_F64X2_NE,  /* f64x2.ne */
    WOP_F64X2_LT,  /* f64x2.lt */
    WOP_F64X2_GT,  /* f64x2.gt */
    WOP_F64X2_LE,  /* f64x2.le */
    WOP_F64X2_GE,  /* f64x2.ge */
    WOP_V128_NOT,  /* v128.not */
    WOP_V128_AND,  /* v128.and */
    WOP_V128_ANDNOT,  /* v128.andnot */
    WOP_V128_OR,  /* v128.or */
    WOP_V128_XOR,  /* v128.xor */
    WOP_V128_BITSELECT,  /* v128.bitselect */
    WOP_V128_ANY_TRUE,  /* v128.any_true */
    WOP_V128_LOAD8_LANE,  /* v128.load8_lane */
    WOP_V128_LOAD16_LANE,  /* v128.load16_lane */
    WOP_V128_LOAD32_LANE,  /* v128.load32_lane */
    WOP_V128_LOAD64_LANE,  /* v128.load64_lane */
    WOP_V128_STORE8_LANE,  /* v128.store8_lane */
    WOP_V128_STORE16_LANE,  /* v128.store16_lane */
    WOP_V128_STORE32_LANE,  /* v128.store32_lane */
    WOP_V128_STORE64_LANE,  /* v128.store64_lane */
    WOP_V128_LOAD32_ZERO,  /* v128.load32_zero */
    WOP_V128_LOAD64_ZERO,  /* v128.load64_zero */
    WOP_F32X4_DEMOTE_F64X2_ZERO,  /* f32x4.demote_f64x2_zero */
    WOP_F64X2_PROMOTE_LOW_F32X4,  /* f64x2.promote_low_f32x4 */
    WOP_I8X16_ABS,  /* i8x16.abs */
    WOP_I8X16_NEG,  /* i8x16.neg */
    WOP_I8X16_POPCNT,  /* i8x16.popcnt */
    WOP_I8X16_ALL_TRUE,  /* i8x16.all_true */
    WOP_I8X16_BITMASK,  /* i8x16.bitmask */
    WOP_I8X16_NARROW_I16X8_S,  /* i8x16.narrow_i16x8_s */
    WOP_I8X16_NARROW_I16X8_U,  /* i8x16.narrow_i16x8_u */
    WOP_F32X4_CEIL,  /* f32x4.ceil */
    WOP_F32X4_FLOOR,  /* f32x4.floor */
    WOP_F32X4_TRUNC,  /* f32x4.trunc */
    WOP_F32X4_NEAREST,  /* f32x4.nearest */
    WOP_I8X16_SHL,  /* i8x16.shl */
    WOP_I8X16_SHR_S,  /* i8x16.shr_s */
    WOP_I8X16_SHR_U,  /* i8x16.shr_u */
    WOP_I8X16_ADD,  /* i8x16.add */
    WOP_I8X16_ADD_SAT_S,  /* i8x16.add_sat_s */
    WOP_I8X16_ADD_SAT_U,  /* i8x16.add_sat_u */
    WOP_I8X16_SUB,  /* i8x16.sub */
    WOP_I8X16_SUB_SAT_S,  /* i8x16.sub_sat_s */
    WOP_I8X16_SUB_SAT_U,  /* i8x16.sub_sat_u */
    WOP_F64X2_CEIL,  /* f64x2.ceil */
    WOP_F64X2_FLOOR,  /* f64x2.floor */
    WOP_I8X16_MIN_S,  /* i8x16.min_s */
    WOP_I8X16_MIN_U,  /* i8x16.min_u */
    WOP_I8X16_MAX_S,  /* i8x16.max_s */
    WOP_I8X16_MAX_U,  /* i8x16.max_u */
    WOP_F64X2_TRUNC,  /* f64x2.trunc */
    WOP_I8X16_AVGR_U,  /* i8x16.avgr_u */
    WOP_I16X8_EXTADD_PAIRWISE_I8X16_S,  /* i16x8.extadd_pairwise_i8x16_s */
    WOP_I16X8_EXTADD_PAIRWISE_I8X16_U,  /* i16x8.extadd_pairwise_i8x16_u */
    WOP_I32X4_EXTADD_PAIRWISE_I16X8_S,  /* i32x4.extadd_pairwise_i16x8_s */
    WOP_I32X4_EXTADD_PAIRWISE_I16X8_U,  /* i32x4.extadd_pairwise_i16x8_u */
    WOP_I16X8_ABS,  /* i16x8.abs */
    WOP_I16X8_NEG,  /* i16x8.neg */
    WOP_I16X8_Q15MULR_SAT_S,  /* i16x8.q15mulr_sat_s */
    WOP_I16X8_ALL_TRUE,  /* i16x8.all_true */
    WOP_I16X8_BITMASK,  /* i16x8.bitmask */
    WOP_I16X8_NARROW_I32X4_S,  /* i16x8.narrow_i32x4_s */
    WOP_I16X8_NARROW_I32X4_U,  /* i16x8.narrow_i32x4_u */
    WOP_I16X8_EXTEND_LOW_I8X16_S,  /* i16x8.extend_low_i8x16_s */
    WOP_I16X8_EXTEND_HIGH_I8X16_S,  /* i16x8.extend_high_i8x16_s */
    WOP_I16X8_EXTEND_LOW_I8X16_U,  /* i16x8.extend_low_i8x16_u */
    WOP_I16X8_EXTEND_HIGH_I8X16_U,  /* i16x8.extend_high_i8x16_u */
    WOP_I16X8_SHL,  /* i16x8.shl */
    WOP_I16X8_SHR_S,  /* i16x8.shr_s */
    WOP_I16X8_SHR_U,  /* i16x8.shr_u */
    WOP_I16X8_ADD,  /* i16x8.add */
    WOP_I16X8_ADD_SAT_S,  /* i16x8.add_sat_s */
    WOP_I16X8_ADD_SAT_U,  /* i16x8.add_sat_u */
    WOP_I16X8_SUB,  /* i16x8.sub */
    WOP_I16X8_SUB_SAT_S,  /* i16x8.sub_sat_s */
    WOP_I16X8_SUB_SAT_U,  /* i16x8.sub_sat_u */
    WOP_F64X2_NEAREST,  /* f64x2.nearest */
    WOP_I16X8_MUL,  /* i16x8.mul */
    WOP_I16X8_MIN_S,  /* i16x8.min_s */
    WOP_I16X8_MIN_U,  /* i16x8.min_u */
    WOP_I16X8_MAX_S,  /* i16x8.max_s */
    WOP_I16X8_MAX_U,  /* i16x8.max_u */
    WOP_I16X8_AVGR_U,  /* i16x8.avgr_u */
    WOP_I16X8_EXTMUL_LOW_I8X16_S,  /* i16x8.extmul_low_i8x16_s */
    WOP_I16X8_EXTMUL_HIGH_I8X16_S,  /* i16x8.extmul_high_i8x16_s */
    WOP_I16X8_EXTMUL_LOW_I8X16_U,  /* i16x8.extmul_low_i8x16_u */
    WOP_I16X8_EXTMUL_HIGH_I8X16_U,  /* i16x8.extmul_high_i8x16_u */
    WOP_I32X4_ABS,  /* i32x4.abs */
    WOP_I32X4_NEG,  /* i32x4.neg */
    WOP_I32X4_ALL_TRUE,  /* i32x4.all_true */
    WOP_I32X4_BITMASK,  /* i32x4.bitmask */
    WOP_I32X4_EXTEND_LOW_I16X8_S,  /* i32x4.extend_low_i16x8_s */
    WOP_I32X4_EXTEND_HIGH_I16X8_S,  /* i32x4.extend_high_i16x8_s */
    WOP_I32X4_EXTEND_LOW_I16X8_U,  /* i32x4.extend_low_i16x8_u */
    WOP_I32X4_EXTEND_HIGH_I16X8_U,  /* i32x4.extend_high_i16x8_u */
    WOP_I32X4_SHL,  /* i32x4.shl */
    WOP_I32X4_SHR_S,  /* i32x4.shr_s */
    WOP_I32X4_SHR_U,  /* i32x4.shr_u */
    WOP_I32X4_ADD,  /* i32x4.add */
    WOP_I32X4_SUB,  /* i32x4.sub */
    WOP_I32X4_MUL,  /* i32x4.mul */
    WOP_I32X4_MIN_S,  /* i32x4.min_s */
    WOP_I32X4_MIN_U,  /* i32x4.min_u */
    WOP_I32X4_MAX_S,  /* i32x4.max_s */
    WOP_I32X4_MAX_U,  /* i32x4.max_u */
    WOP_I32X4_DOT_I16X8_S,  /* i32x4.dot_i16x8_s */
    WOP_I32X4_EXTMUL_LOW_I16X8_S,  /* i32x4.extmul_low_i16x8_s */
    WOP_I32X4_EXTMUL_HIGH_I16X8_S,  /* i32x4.extmul_high_i16x8_s */
    WOP_I32X4_EXTMUL_LOW_I16X8_U,  /* i32x4.extmul_low_i16x8_u */
    WOP_I32X4_EXTMUL_HIGH_I16X8_U,  /* i32x4.extmul_high_i16x8_u */
    WOP_I64X2_ABS,  /* i64x2.abs */
    WOP_I64X2_NEG,  /* i64x2.neg */
    WOP_I64X2_ALL_TRUE,  /* i64x2.all_true */
    WOP_I64X2_BITMASK,  /* i64x2.bitmask */
    WOP_I64X2_EXTEND_LOW_I32X4_S,  /* i64x2.extend_low_i32x4_s */
    WOP_I64X2_EXTEND_HIGH_I32X4_S,  /* i64x2.extend_high_i32x4_s */
    WOP_I64X2_EXTEND_LOW_I32X4_U,  /* i64x2.extend_low_i32x4_u */
    WOP_I64X2_EXTEND_HIGH_I32X4_U,  /* i64x2.extend_high_i32x4_u */
    WOP_I64X2_SHL,  /* i64x2.shl */
    WOP_I64X2_SHR_S,  /* i64x2.shr_s */
    WOP_I64X2_SHR_U,  /* i64x2.shr_u */
    WOP_I64X2_ADD,  /* i64x2.add */
    WOP_I64X2_SUB,  /* i64x2.sub */
    WOP_I64X2_MUL,  /* i64x2.mul */
    WOP_I64X2_EQ,  /* i64x2.eq */
    WOP_I64X2_NE,  /* i64x2.ne */
    WOP_I64X2_LT_S,  /* i64x2.lt_s */
    WOP_I64X2_GT_S,  /* i64x2.gt_s */
    WOP_I64X2_LE_S,  /* i64x2.le_s */
    WOP_I64X2_GE_S,  /* i64x2.ge_s */
    WOP_I64X2_EXTMUL_LOW_I32X4_S,  /* i64x2.extmul_low_i32x4_s */
    WOP_I64X2_EXTMUL_HIGH_I32X4_S,  /* i64x2.extmul_high_i32x4_s */
    WOP_I64X2_EXTMUL_LOW_I32X4_U,  /* i64x2.extmul_low_i32x4_u */
    WOP_I64X2_EXTMUL_HIGH_I32X4_U,  /* i64x2.extmul_high_i32x4_u */
    WOP_F32X4_ABS,  /* f32x4.abs */
    WOP_F32X4_NEG,  /* f32x4.neg */
    WOP_F32X4_SQRT,  /* f32x4.sqrt */
    WOP_F32X4_ADD,  /* f32x4.add */
    WOP_F32X4_SUB,  /* f32x4.sub */
    WOP_F32X4_MUL,  /* f32x4.mul */
    WOP_F32X4_DIV,  /* f32x4.div */
    WOP_F32X4_MIN,  /* f32x4.min */
    WOP_F32X4_MAX,  /* f32x4.max */
    WOP_F32X4_PMIN,  /* f32x4.pmin */
    WOP_F32X4_PMAX,  /* f32x4.pmax */
    WOP_F64X2_ABS,  /* f64x2.abs */
    WOP_F64X2_NEG,  /* f64x2.neg */
    WOP_F64X2_SQRT,  /* f64x2.sqrt */
    WOP_F64X2_ADD,  /* f64x2.add */
    WOP_F64X2_SUB,  /* f64x2.sub */
    WOP_F64X2_MUL,  /* f64x2.mul */
    WOP_F64X2_DIV,  /* f64x2.div */
    WOP_F64X2_MIN,  /* f64x2.min */
    WOP_F64X2_MAX,  /* f64x2.max */
    WOP_F64X2_PMIN,  /* f64x2.pmin */
    WOP_F64X2_PMAX,  /* f64x2.pmax */
    WOP_I32X4_TRUNC_SAT_F32X4_S,  /* i32x4.trunc_sat_f32x4_s */
    WOP_I32X4_TRUNC_SAT_F32X4_U,  /* i32x4.trunc_sat_f32x4_u */
    WOP_F32X4_CONVERT_I32X4_S,  /* f32x4.convert_i32x4_s */
    WOP_F32X4_CONVERT_I32X4_U,  /* f32x4.convert_i32x4_u */
    WOP_I32X4_TRUNC_SAT_F64X2_S_ZERO,  /* i32x4.trunc_sat_f64x2_s_zero */
    WOP_I32X4_TRUNC_SAT_F64X2_U_ZERO,  /* i32x4.trunc_sat_f64x2_u_zero */
    WOP_F64X2_CONVERT_LOW_I32X4_S,  /* f64x2.convert_low_i32x4_s */
    WOP_F64X2_CONVERT_LOW_I32X4_U,  /* f64x2.convert_low_i32x4_u */
    WOP_I8X16_RELAXED_SWIZZLE,  /* i8x16.relaxed_swizzle */
    WOP_I32X4_RELAXED_TRUNC_F32X4_S,  /* i32x4.relaxed_trunc_f32x4_s */
    WOP_I32X4_RELAXED_TRUNC_F32X4_U,  /* i32x4.relaxed_trunc_f32x4_u */
    WOP_I32X4_RELAXED_TRUNC_F64X2_S_ZERO,  /* i32x4.relaxed_trunc_f64x2_s_zero */
    WOP_I32X4_RELAXED_TRUNC_F64X2_U_ZERO,  /* i32x4.relaxed_trunc_f64x2_u_zero */
    WOP_F32X4_RELAXED_MADD,  /* f32x4.relaxed_madd */
    WOP_F32X4_RELAXED_NMADD,  /* f32x4.relaxed_nmadd */
    WOP_F64X2_RELAXED_MADD,  /* f64x2.relaxed_madd */
    WOP_F64X2_RELAXED_NMADD,  /* f64x2.relaxed_nmadd */
    WOP_I8X16_RELAXED_LANESELECT,  /* i8x16.relaxed_laneselect */
    WOP_I16X8_RELAXED_LANESELECT,  /* i16x8.relaxed_laneselect */
    WOP_I32X4_RELAXED_LANESELECT,  /* i32x4.relaxed_laneselect */
    WOP_I64X2_RELAXED_LANESELECT,  /* i64x2.relaxed_laneselect */
    WOP_F32X4_RELAXED_MIN,  /* f32x4.relaxed_min */
    WOP_F32X4_RELAXED_MAX,  /* f32x4.relaxed_max */
    WOP_F64X2_RELAXED_MIN,  /* f64x2.relaxed_min */
    WOP_F64X2_RELAXED_MAX,  /* f64x2.relaxed_max */
    WOP_I16X8_RELAXED_Q15MULR_S,  /* i16x8.relaxed_q15mulr_s */
    WOP_I16X8_RELAXED_DOT_I8X16_I7X16_S,  /* i16x8.relaxed_dot_i8x16_i7x16_s */
    WOP_I32X4_RELAXED_DOT_I8X16_I7X16_ADD_S,  /* i32x4.relaxed_dot_i8x16_i7x16_add_s */
    WOP__COUNT
} wasm_op_t;

/* prefix == 0 → a single-byte opcode; else emit `prefix` then `opcode` as uleb. */
typedef struct { uint8_t prefix; uint32_t opcode; } wasm_op_enc_t;

static const wasm_op_enc_t wasm_op_enc[WOP__COUNT] = {
    [WOP_UNREACHABLE] = { 0x00, 0x00 },
    [WOP_NOP] = { 0x00, 0x01 },
    [WOP_BLOCK] = { 0x00, 0x02 },
    [WOP_LOOP] = { 0x00, 0x03 },
    [WOP_IF] = { 0x00, 0x04 },
    [WOP_THROW] = { 0x00, 0x08 },
    [WOP_THROW_REF] = { 0x00, 0x0A },
    [WOP_BR] = { 0x00, 0x0C },
    [WOP_BR_IF] = { 0x00, 0x0D },
    [WOP_BR_TABLE] = { 0x00, 0x0E },
    [WOP_RETURN] = { 0x00, 0x0F },
    [WOP_CALL] = { 0x00, 0x10 },
    [WOP_CALL_INDIRECT] = { 0x00, 0x11 },
    [WOP_RETURN_CALL] = { 0x00, 0x12 },
    [WOP_RETURN_CALL_INDIRECT] = { 0x00, 0x13 },
    [WOP_CALL_REF] = { 0x00, 0x14 },
    [WOP_RETURN_CALL_REF] = { 0x00, 0x15 },
    [WOP_DROP] = { 0x00, 0x1A },
    [WOP_SELECT] = { 0x00, 0x1B },
    [WOP_SELECT_T] = { 0x00, 0x1C },
    [WOP_TRY_TABLE] = { 0x00, 0x1F },
    [WOP_LOCAL_GET] = { 0x00, 0x20 },
    [WOP_LOCAL_SET] = { 0x00, 0x21 },
    [WOP_LOCAL_TEE] = { 0x00, 0x22 },
    [WOP_GLOBAL_GET] = { 0x00, 0x23 },
    [WOP_GLOBAL_SET] = { 0x00, 0x24 },
    [WOP_TABLE_GET] = { 0x00, 0x25 },
    [WOP_TABLE_SET] = { 0x00, 0x26 },
    [WOP_I32_LOAD] = { 0x00, 0x28 },
    [WOP_I64_LOAD] = { 0x00, 0x29 },
    [WOP_F32_LOAD] = { 0x00, 0x2A },
    [WOP_F64_LOAD] = { 0x00, 0x2B },
    [WOP_I32_LOAD8_S] = { 0x00, 0x2C },
    [WOP_I32_LOAD8_U] = { 0x00, 0x2D },
    [WOP_I32_LOAD16_S] = { 0x00, 0x2E },
    [WOP_I32_LOAD16_U] = { 0x00, 0x2F },
    [WOP_I64_LOAD8_S] = { 0x00, 0x30 },
    [WOP_I64_LOAD8_U] = { 0x00, 0x31 },
    [WOP_I64_LOAD16_S] = { 0x00, 0x32 },
    [WOP_I64_LOAD16_U] = { 0x00, 0x33 },
    [WOP_I64_LOAD32_S] = { 0x00, 0x34 },
    [WOP_I64_LOAD32_U] = { 0x00, 0x35 },
    [WOP_I32_STORE] = { 0x00, 0x36 },
    [WOP_I64_STORE] = { 0x00, 0x37 },
    [WOP_F32_STORE] = { 0x00, 0x38 },
    [WOP_F64_STORE] = { 0x00, 0x39 },
    [WOP_I32_STORE8] = { 0x00, 0x3A },
    [WOP_I32_STORE16] = { 0x00, 0x3B },
    [WOP_I64_STORE8] = { 0x00, 0x3C },
    [WOP_I64_STORE16] = { 0x00, 0x3D },
    [WOP_I64_STORE32] = { 0x00, 0x3E },
    [WOP_MEMORY_SIZE] = { 0x00, 0x3F },
    [WOP_MEMORY_GROW] = { 0x00, 0x40 },
    [WOP_I32_CONST] = { 0x00, 0x41 },
    [WOP_I64_CONST] = { 0x00, 0x42 },
    [WOP_F32_CONST] = { 0x00, 0x43 },
    [WOP_F64_CONST] = { 0x00, 0x44 },
    [WOP_I32_EQZ] = { 0x00, 0x45 },
    [WOP_I32_EQ] = { 0x00, 0x46 },
    [WOP_I32_NE] = { 0x00, 0x47 },
    [WOP_I32_LT_S] = { 0x00, 0x48 },
    [WOP_I32_LT_U] = { 0x00, 0x49 },
    [WOP_I32_GT_S] = { 0x00, 0x4A },
    [WOP_I32_GT_U] = { 0x00, 0x4B },
    [WOP_I32_LE_S] = { 0x00, 0x4C },
    [WOP_I32_LE_U] = { 0x00, 0x4D },
    [WOP_I32_GE_S] = { 0x00, 0x4E },
    [WOP_I32_GE_U] = { 0x00, 0x4F },
    [WOP_I64_EQZ] = { 0x00, 0x50 },
    [WOP_I64_EQ] = { 0x00, 0x51 },
    [WOP_I64_NE] = { 0x00, 0x52 },
    [WOP_I64_LT_S] = { 0x00, 0x53 },
    [WOP_I64_LT_U] = { 0x00, 0x54 },
    [WOP_I64_GT_S] = { 0x00, 0x55 },
    [WOP_I64_GT_U] = { 0x00, 0x56 },
    [WOP_I64_LE_S] = { 0x00, 0x57 },
    [WOP_I64_LE_U] = { 0x00, 0x58 },
    [WOP_I64_GE_S] = { 0x00, 0x59 },
    [WOP_I64_GE_U] = { 0x00, 0x5A },
    [WOP_F32_EQ] = { 0x00, 0x5B },
    [WOP_F32_NE] = { 0x00, 0x5C },
    [WOP_F32_LT] = { 0x00, 0x5D },
    [WOP_F32_GT] = { 0x00, 0x5E },
    [WOP_F32_LE] = { 0x00, 0x5F },
    [WOP_F32_GE] = { 0x00, 0x60 },
    [WOP_F64_EQ] = { 0x00, 0x61 },
    [WOP_F64_NE] = { 0x00, 0x62 },
    [WOP_F64_LT] = { 0x00, 0x63 },
    [WOP_F64_GT] = { 0x00, 0x64 },
    [WOP_F64_LE] = { 0x00, 0x65 },
    [WOP_F64_GE] = { 0x00, 0x66 },
    [WOP_I32_CLZ] = { 0x00, 0x67 },
    [WOP_I32_CTZ] = { 0x00, 0x68 },
    [WOP_I32_POPCNT] = { 0x00, 0x69 },
    [WOP_I32_ADD] = { 0x00, 0x6A },
    [WOP_I32_SUB] = { 0x00, 0x6B },
    [WOP_I32_MUL] = { 0x00, 0x6C },
    [WOP_I32_DIV_S] = { 0x00, 0x6D },
    [WOP_I32_DIV_U] = { 0x00, 0x6E },
    [WOP_I32_REM_S] = { 0x00, 0x6F },
    [WOP_I32_REM_U] = { 0x00, 0x70 },
    [WOP_I32_AND] = { 0x00, 0x71 },
    [WOP_I32_OR] = { 0x00, 0x72 },
    [WOP_I32_XOR] = { 0x00, 0x73 },
    [WOP_I32_SHL] = { 0x00, 0x74 },
    [WOP_I32_SHR_S] = { 0x00, 0x75 },
    [WOP_I32_SHR_U] = { 0x00, 0x76 },
    [WOP_I32_ROTL] = { 0x00, 0x77 },
    [WOP_I32_ROTR] = { 0x00, 0x78 },
    [WOP_I64_CLZ] = { 0x00, 0x79 },
    [WOP_I64_CTZ] = { 0x00, 0x7A },
    [WOP_I64_POPCNT] = { 0x00, 0x7B },
    [WOP_I64_ADD] = { 0x00, 0x7C },
    [WOP_I64_SUB] = { 0x00, 0x7D },
    [WOP_I64_MUL] = { 0x00, 0x7E },
    [WOP_I64_DIV_S] = { 0x00, 0x7F },
    [WOP_I64_DIV_U] = { 0x00, 0x80 },
    [WOP_I64_REM_S] = { 0x00, 0x81 },
    [WOP_I64_REM_U] = { 0x00, 0x82 },
    [WOP_I64_AND] = { 0x00, 0x83 },
    [WOP_I64_OR] = { 0x00, 0x84 },
    [WOP_I64_XOR] = { 0x00, 0x85 },
    [WOP_I64_SHL] = { 0x00, 0x86 },
    [WOP_I64_SHR_S] = { 0x00, 0x87 },
    [WOP_I64_SHR_U] = { 0x00, 0x88 },
    [WOP_I64_ROTL] = { 0x00, 0x89 },
    [WOP_I64_ROTR] = { 0x00, 0x8A },
    [WOP_F32_ABS] = { 0x00, 0x8B },
    [WOP_F32_NEG] = { 0x00, 0x8C },
    [WOP_F32_CEIL] = { 0x00, 0x8D },
    [WOP_F32_FLOOR] = { 0x00, 0x8E },
    [WOP_F32_TRUNC] = { 0x00, 0x8F },
    [WOP_F32_NEAREST] = { 0x00, 0x90 },
    [WOP_F32_SQRT] = { 0x00, 0x91 },
    [WOP_F32_ADD] = { 0x00, 0x92 },
    [WOP_F32_SUB] = { 0x00, 0x93 },
    [WOP_F32_MUL] = { 0x00, 0x94 },
    [WOP_F32_DIV] = { 0x00, 0x95 },
    [WOP_F32_MIN] = { 0x00, 0x96 },
    [WOP_F32_MAX] = { 0x00, 0x97 },
    [WOP_F32_COPYSIGN] = { 0x00, 0x98 },
    [WOP_F64_ABS] = { 0x00, 0x99 },
    [WOP_F64_NEG] = { 0x00, 0x9A },
    [WOP_F64_CEIL] = { 0x00, 0x9B },
    [WOP_F64_FLOOR] = { 0x00, 0x9C },
    [WOP_F64_TRUNC] = { 0x00, 0x9D },
    [WOP_F64_NEAREST] = { 0x00, 0x9E },
    [WOP_F64_SQRT] = { 0x00, 0x9F },
    [WOP_F64_ADD] = { 0x00, 0xA0 },
    [WOP_F64_SUB] = { 0x00, 0xA1 },
    [WOP_F64_MUL] = { 0x00, 0xA2 },
    [WOP_F64_DIV] = { 0x00, 0xA3 },
    [WOP_F64_MIN] = { 0x00, 0xA4 },
    [WOP_F64_MAX] = { 0x00, 0xA5 },
    [WOP_F64_COPYSIGN] = { 0x00, 0xA6 },
    [WOP_I32_WRAP_I64] = { 0x00, 0xA7 },
    [WOP_I32_TRUNC_F32_S] = { 0x00, 0xA8 },
    [WOP_I32_TRUNC_F32_U] = { 0x00, 0xA9 },
    [WOP_I32_TRUNC_F64_S] = { 0x00, 0xAA },
    [WOP_I32_TRUNC_F64_U] = { 0x00, 0xAB },
    [WOP_I64_EXTEND_I32_S] = { 0x00, 0xAC },
    [WOP_I64_EXTEND_I32_U] = { 0x00, 0xAD },
    [WOP_I64_TRUNC_F32_S] = { 0x00, 0xAE },
    [WOP_I64_TRUNC_F32_U] = { 0x00, 0xAF },
    [WOP_I64_TRUNC_F64_S] = { 0x00, 0xB0 },
    [WOP_I64_TRUNC_F64_U] = { 0x00, 0xB1 },
    [WOP_F32_CONVERT_I32_S] = { 0x00, 0xB2 },
    [WOP_F32_CONVERT_I32_U] = { 0x00, 0xB3 },
    [WOP_F32_CONVERT_I64_S] = { 0x00, 0xB4 },
    [WOP_F32_CONVERT_I64_U] = { 0x00, 0xB5 },
    [WOP_F32_DEMOTE_F64] = { 0x00, 0xB6 },
    [WOP_F64_CONVERT_I32_S] = { 0x00, 0xB7 },
    [WOP_F64_CONVERT_I32_U] = { 0x00, 0xB8 },
    [WOP_F64_CONVERT_I64_S] = { 0x00, 0xB9 },
    [WOP_F64_CONVERT_I64_U] = { 0x00, 0xBA },
    [WOP_F64_PROMOTE_F32] = { 0x00, 0xBB },
    [WOP_I32_REINTERPRET_F32] = { 0x00, 0xBC },
    [WOP_I64_REINTERPRET_F64] = { 0x00, 0xBD },
    [WOP_F32_REINTERPRET_I32] = { 0x00, 0xBE },
    [WOP_F64_REINTERPRET_I64] = { 0x00, 0xBF },
    [WOP_I32_EXTEND8_S] = { 0x00, 0xC0 },
    [WOP_I32_EXTEND16_S] = { 0x00, 0xC1 },
    [WOP_I64_EXTEND8_S] = { 0x00, 0xC2 },
    [WOP_I64_EXTEND16_S] = { 0x00, 0xC3 },
    [WOP_I64_EXTEND32_S] = { 0x00, 0xC4 },
    [WOP_REF_NULL] = { 0x00, 0xD0 },
    [WOP_REF_IS_NULL] = { 0x00, 0xD1 },
    [WOP_REF_FUNC] = { 0x00, 0xD2 },
    [WOP_REF_EQ] = { 0x00, 0xD3 },
    [WOP_REF_AS_NON_NULL] = { 0x00, 0xD4 },
    [WOP_BR_ON_NULL] = { 0x00, 0xD5 },
    [WOP_BR_ON_NON_NULL] = { 0x00, 0xD6 },
    [WOP_STRUCT_NEW] = { 0xFB, 0x00 },
    [WOP_STRUCT_NEW_DEFAULT] = { 0xFB, 0x01 },
    [WOP_STRUCT_GET] = { 0xFB, 0x02 },
    [WOP_STRUCT_GET_S] = { 0xFB, 0x03 },
    [WOP_STRUCT_GET_U] = { 0xFB, 0x04 },
    [WOP_STRUCT_SET] = { 0xFB, 0x05 },
    [WOP_ARRAY_NEW] = { 0xFB, 0x06 },
    [WOP_ARRAY_NEW_DEFAULT] = { 0xFB, 0x07 },
    [WOP_ARRAY_NEW_FIXED] = { 0xFB, 0x08 },
    [WOP_ARRAY_NEW_DATA] = { 0xFB, 0x09 },
    [WOP_ARRAY_NEW_ELEM] = { 0xFB, 0x0A },
    [WOP_ARRAY_GET] = { 0xFB, 0x0B },
    [WOP_ARRAY_GET_S] = { 0xFB, 0x0C },
    [WOP_ARRAY_GET_U] = { 0xFB, 0x0D },
    [WOP_ARRAY_SET] = { 0xFB, 0x0E },
    [WOP_ARRAY_LEN] = { 0xFB, 0x0F },
    [WOP_ARRAY_FILL] = { 0xFB, 0x10 },
    [WOP_ARRAY_COPY] = { 0xFB, 0x11 },
    [WOP_ARRAY_INIT_DATA] = { 0xFB, 0x12 },
    [WOP_ARRAY_INIT_ELEM] = { 0xFB, 0x13 },
    [WOP_REF_TEST] = { 0xFB, 0x14 },
    [WOP_REF_TEST_NULL] = { 0xFB, 0x15 },
    [WOP_REF_CAST] = { 0xFB, 0x16 },
    [WOP_REF_CAST_NULL] = { 0xFB, 0x17 },
    [WOP_BR_ON_CAST] = { 0xFB, 0x18 },
    [WOP_BR_ON_CAST_FAIL] = { 0xFB, 0x19 },
    [WOP_ANY_CONVERT_EXTERN] = { 0xFB, 0x1A },
    [WOP_EXTERN_CONVERT_ANY] = { 0xFB, 0x1B },
    [WOP_REF_I31] = { 0xFB, 0x1C },
    [WOP_I31_GET_S] = { 0xFB, 0x1D },
    [WOP_I31_GET_U] = { 0xFB, 0x1E },
    [WOP_I32_TRUNC_SAT_F32_S] = { 0xFC, 0x00 },
    [WOP_I32_TRUNC_SAT_F32_U] = { 0xFC, 0x01 },
    [WOP_I32_TRUNC_SAT_F64_S] = { 0xFC, 0x02 },
    [WOP_I32_TRUNC_SAT_F64_U] = { 0xFC, 0x03 },
    [WOP_I64_TRUNC_SAT_F32_S] = { 0xFC, 0x04 },
    [WOP_I64_TRUNC_SAT_F32_U] = { 0xFC, 0x05 },
    [WOP_I64_TRUNC_SAT_F64_S] = { 0xFC, 0x06 },
    [WOP_I64_TRUNC_SAT_F64_U] = { 0xFC, 0x07 },
    [WOP_MEMORY_INIT] = { 0xFC, 0x08 },
    [WOP_DATA_DROP] = { 0xFC, 0x09 },
    [WOP_MEMORY_COPY] = { 0xFC, 0x0A },
    [WOP_MEMORY_FILL] = { 0xFC, 0x0B },
    [WOP_TABLE_INIT] = { 0xFC, 0x0C },
    [WOP_ELEM_DROP] = { 0xFC, 0x0D },
    [WOP_TABLE_COPY] = { 0xFC, 0x0E },
    [WOP_TABLE_GROW] = { 0xFC, 0x0F },
    [WOP_TABLE_SIZE] = { 0xFC, 0x10 },
    [WOP_TABLE_FILL] = { 0xFC, 0x11 },
    [WOP_V128_LOAD] = { 0xFD, 0x00 },
    [WOP_V128_LOAD8X8_S] = { 0xFD, 0x01 },
    [WOP_V128_LOAD8X8_U] = { 0xFD, 0x02 },
    [WOP_V128_LOAD16X4_S] = { 0xFD, 0x03 },
    [WOP_V128_LOAD16X4_U] = { 0xFD, 0x04 },
    [WOP_V128_LOAD32X2_S] = { 0xFD, 0x05 },
    [WOP_V128_LOAD32X2_U] = { 0xFD, 0x06 },
    [WOP_V128_LOAD8_SPLAT] = { 0xFD, 0x07 },
    [WOP_V128_LOAD16_SPLAT] = { 0xFD, 0x08 },
    [WOP_V128_LOAD32_SPLAT] = { 0xFD, 0x09 },
    [WOP_V128_LOAD64_SPLAT] = { 0xFD, 0x0A },
    [WOP_V128_STORE] = { 0xFD, 0x0B },
    [WOP_V128_CONST] = { 0xFD, 0x0C },
    [WOP_I8X16_SHUFFLE] = { 0xFD, 0x0D },
    [WOP_I8X16_SWIZZLE] = { 0xFD, 0x0E },
    [WOP_I8X16_SPLAT] = { 0xFD, 0x0F },
    [WOP_I16X8_SPLAT] = { 0xFD, 0x10 },
    [WOP_I32X4_SPLAT] = { 0xFD, 0x11 },
    [WOP_I64X2_SPLAT] = { 0xFD, 0x12 },
    [WOP_F32X4_SPLAT] = { 0xFD, 0x13 },
    [WOP_F64X2_SPLAT] = { 0xFD, 0x14 },
    [WOP_I8X16_EXTRACT_LANE_S] = { 0xFD, 0x15 },
    [WOP_I8X16_EXTRACT_LANE_U] = { 0xFD, 0x16 },
    [WOP_I8X16_REPLACE_LANE] = { 0xFD, 0x17 },
    [WOP_I16X8_EXTRACT_LANE_S] = { 0xFD, 0x18 },
    [WOP_I16X8_EXTRACT_LANE_U] = { 0xFD, 0x19 },
    [WOP_I16X8_REPLACE_LANE] = { 0xFD, 0x1A },
    [WOP_I32X4_EXTRACT_LANE] = { 0xFD, 0x1B },
    [WOP_I32X4_REPLACE_LANE] = { 0xFD, 0x1C },
    [WOP_I64X2_EXTRACT_LANE] = { 0xFD, 0x1D },
    [WOP_I64X2_REPLACE_LANE] = { 0xFD, 0x1E },
    [WOP_F32X4_EXTRACT_LANE] = { 0xFD, 0x1F },
    [WOP_F32X4_REPLACE_LANE] = { 0xFD, 0x20 },
    [WOP_F64X2_EXTRACT_LANE] = { 0xFD, 0x21 },
    [WOP_F64X2_REPLACE_LANE] = { 0xFD, 0x22 },
    [WOP_I8X16_EQ] = { 0xFD, 0x23 },
    [WOP_I8X16_NE] = { 0xFD, 0x24 },
    [WOP_I8X16_LT_S] = { 0xFD, 0x25 },
    [WOP_I8X16_LT_U] = { 0xFD, 0x26 },
    [WOP_I8X16_GT_S] = { 0xFD, 0x27 },
    [WOP_I8X16_GT_U] = { 0xFD, 0x28 },
    [WOP_I8X16_LE_S] = { 0xFD, 0x29 },
    [WOP_I8X16_LE_U] = { 0xFD, 0x2A },
    [WOP_I8X16_GE_S] = { 0xFD, 0x2B },
    [WOP_I8X16_GE_U] = { 0xFD, 0x2C },
    [WOP_I16X8_EQ] = { 0xFD, 0x2D },
    [WOP_I16X8_NE] = { 0xFD, 0x2E },
    [WOP_I16X8_LT_S] = { 0xFD, 0x2F },
    [WOP_I16X8_LT_U] = { 0xFD, 0x30 },
    [WOP_I16X8_GT_S] = { 0xFD, 0x31 },
    [WOP_I16X8_GT_U] = { 0xFD, 0x32 },
    [WOP_I16X8_LE_S] = { 0xFD, 0x33 },
    [WOP_I16X8_LE_U] = { 0xFD, 0x34 },
    [WOP_I16X8_GE_S] = { 0xFD, 0x35 },
    [WOP_I16X8_GE_U] = { 0xFD, 0x36 },
    [WOP_I32X4_EQ] = { 0xFD, 0x37 },
    [WOP_I32X4_NE] = { 0xFD, 0x38 },
    [WOP_I32X4_LT_S] = { 0xFD, 0x39 },
    [WOP_I32X4_LT_U] = { 0xFD, 0x3A },
    [WOP_I32X4_GT_S] = { 0xFD, 0x3B },
    [WOP_I32X4_GT_U] = { 0xFD, 0x3C },
    [WOP_I32X4_LE_S] = { 0xFD, 0x3D },
    [WOP_I32X4_LE_U] = { 0xFD, 0x3E },
    [WOP_I32X4_GE_S] = { 0xFD, 0x3F },
    [WOP_I32X4_GE_U] = { 0xFD, 0x40 },
    [WOP_F32X4_EQ] = { 0xFD, 0x41 },
    [WOP_F32X4_NE] = { 0xFD, 0x42 },
    [WOP_F32X4_LT] = { 0xFD, 0x43 },
    [WOP_F32X4_GT] = { 0xFD, 0x44 },
    [WOP_F32X4_LE] = { 0xFD, 0x45 },
    [WOP_F32X4_GE] = { 0xFD, 0x46 },
    [WOP_F64X2_EQ] = { 0xFD, 0x47 },
    [WOP_F64X2_NE] = { 0xFD, 0x48 },
    [WOP_F64X2_LT] = { 0xFD, 0x49 },
    [WOP_F64X2_GT] = { 0xFD, 0x4A },
    [WOP_F64X2_LE] = { 0xFD, 0x4B },
    [WOP_F64X2_GE] = { 0xFD, 0x4C },
    [WOP_V128_NOT] = { 0xFD, 0x4D },
    [WOP_V128_AND] = { 0xFD, 0x4E },
    [WOP_V128_ANDNOT] = { 0xFD, 0x4F },
    [WOP_V128_OR] = { 0xFD, 0x50 },
    [WOP_V128_XOR] = { 0xFD, 0x51 },
    [WOP_V128_BITSELECT] = { 0xFD, 0x52 },
    [WOP_V128_ANY_TRUE] = { 0xFD, 0x53 },
    [WOP_V128_LOAD8_LANE] = { 0xFD, 0x54 },
    [WOP_V128_LOAD16_LANE] = { 0xFD, 0x55 },
    [WOP_V128_LOAD32_LANE] = { 0xFD, 0x56 },
    [WOP_V128_LOAD64_LANE] = { 0xFD, 0x57 },
    [WOP_V128_STORE8_LANE] = { 0xFD, 0x58 },
    [WOP_V128_STORE16_LANE] = { 0xFD, 0x59 },
    [WOP_V128_STORE32_LANE] = { 0xFD, 0x5A },
    [WOP_V128_STORE64_LANE] = { 0xFD, 0x5B },
    [WOP_V128_LOAD32_ZERO] = { 0xFD, 0x5C },
    [WOP_V128_LOAD64_ZERO] = { 0xFD, 0x5D },
    [WOP_F32X4_DEMOTE_F64X2_ZERO] = { 0xFD, 0x5E },
    [WOP_F64X2_PROMOTE_LOW_F32X4] = { 0xFD, 0x5F },
    [WOP_I8X16_ABS] = { 0xFD, 0x60 },
    [WOP_I8X16_NEG] = { 0xFD, 0x61 },
    [WOP_I8X16_POPCNT] = { 0xFD, 0x62 },
    [WOP_I8X16_ALL_TRUE] = { 0xFD, 0x63 },
    [WOP_I8X16_BITMASK] = { 0xFD, 0x64 },
    [WOP_I8X16_NARROW_I16X8_S] = { 0xFD, 0x65 },
    [WOP_I8X16_NARROW_I16X8_U] = { 0xFD, 0x66 },
    [WOP_F32X4_CEIL] = { 0xFD, 0x67 },
    [WOP_F32X4_FLOOR] = { 0xFD, 0x68 },
    [WOP_F32X4_TRUNC] = { 0xFD, 0x69 },
    [WOP_F32X4_NEAREST] = { 0xFD, 0x6A },
    [WOP_I8X16_SHL] = { 0xFD, 0x6B },
    [WOP_I8X16_SHR_S] = { 0xFD, 0x6C },
    [WOP_I8X16_SHR_U] = { 0xFD, 0x6D },
    [WOP_I8X16_ADD] = { 0xFD, 0x6E },
    [WOP_I8X16_ADD_SAT_S] = { 0xFD, 0x6F },
    [WOP_I8X16_ADD_SAT_U] = { 0xFD, 0x70 },
    [WOP_I8X16_SUB] = { 0xFD, 0x71 },
    [WOP_I8X16_SUB_SAT_S] = { 0xFD, 0x72 },
    [WOP_I8X16_SUB_SAT_U] = { 0xFD, 0x73 },
    [WOP_F64X2_CEIL] = { 0xFD, 0x74 },
    [WOP_F64X2_FLOOR] = { 0xFD, 0x75 },
    [WOP_I8X16_MIN_S] = { 0xFD, 0x76 },
    [WOP_I8X16_MIN_U] = { 0xFD, 0x77 },
    [WOP_I8X16_MAX_S] = { 0xFD, 0x78 },
    [WOP_I8X16_MAX_U] = { 0xFD, 0x79 },
    [WOP_F64X2_TRUNC] = { 0xFD, 0x7A },
    [WOP_I8X16_AVGR_U] = { 0xFD, 0x7B },
    [WOP_I16X8_EXTADD_PAIRWISE_I8X16_S] = { 0xFD, 0x7C },
    [WOP_I16X8_EXTADD_PAIRWISE_I8X16_U] = { 0xFD, 0x7D },
    [WOP_I32X4_EXTADD_PAIRWISE_I16X8_S] = { 0xFD, 0x7E },
    [WOP_I32X4_EXTADD_PAIRWISE_I16X8_U] = { 0xFD, 0x7F },
    [WOP_I16X8_ABS] = { 0xFD, 0x80 },
    [WOP_I16X8_NEG] = { 0xFD, 0x81 },
    [WOP_I16X8_Q15MULR_SAT_S] = { 0xFD, 0x82 },
    [WOP_I16X8_ALL_TRUE] = { 0xFD, 0x83 },
    [WOP_I16X8_BITMASK] = { 0xFD, 0x84 },
    [WOP_I16X8_NARROW_I32X4_S] = { 0xFD, 0x85 },
    [WOP_I16X8_NARROW_I32X4_U] = { 0xFD, 0x86 },
    [WOP_I16X8_EXTEND_LOW_I8X16_S] = { 0xFD, 0x87 },
    [WOP_I16X8_EXTEND_HIGH_I8X16_S] = { 0xFD, 0x88 },
    [WOP_I16X8_EXTEND_LOW_I8X16_U] = { 0xFD, 0x89 },
    [WOP_I16X8_EXTEND_HIGH_I8X16_U] = { 0xFD, 0x8A },
    [WOP_I16X8_SHL] = { 0xFD, 0x8B },
    [WOP_I16X8_SHR_S] = { 0xFD, 0x8C },
    [WOP_I16X8_SHR_U] = { 0xFD, 0x8D },
    [WOP_I16X8_ADD] = { 0xFD, 0x8E },
    [WOP_I16X8_ADD_SAT_S] = { 0xFD, 0x8F },
    [WOP_I16X8_ADD_SAT_U] = { 0xFD, 0x90 },
    [WOP_I16X8_SUB] = { 0xFD, 0x91 },
    [WOP_I16X8_SUB_SAT_S] = { 0xFD, 0x92 },
    [WOP_I16X8_SUB_SAT_U] = { 0xFD, 0x93 },
    [WOP_F64X2_NEAREST] = { 0xFD, 0x94 },
    [WOP_I16X8_MUL] = { 0xFD, 0x95 },
    [WOP_I16X8_MIN_S] = { 0xFD, 0x96 },
    [WOP_I16X8_MIN_U] = { 0xFD, 0x97 },
    [WOP_I16X8_MAX_S] = { 0xFD, 0x98 },
    [WOP_I16X8_MAX_U] = { 0xFD, 0x99 },
    [WOP_I16X8_AVGR_U] = { 0xFD, 0x9B },
    [WOP_I16X8_EXTMUL_LOW_I8X16_S] = { 0xFD, 0x9C },
    [WOP_I16X8_EXTMUL_HIGH_I8X16_S] = { 0xFD, 0x9D },
    [WOP_I16X8_EXTMUL_LOW_I8X16_U] = { 0xFD, 0x9E },
    [WOP_I16X8_EXTMUL_HIGH_I8X16_U] = { 0xFD, 0x9F },
    [WOP_I32X4_ABS] = { 0xFD, 0xA0 },
    [WOP_I32X4_NEG] = { 0xFD, 0xA1 },
    [WOP_I32X4_ALL_TRUE] = { 0xFD, 0xA3 },
    [WOP_I32X4_BITMASK] = { 0xFD, 0xA4 },
    [WOP_I32X4_EXTEND_LOW_I16X8_S] = { 0xFD, 0xA7 },
    [WOP_I32X4_EXTEND_HIGH_I16X8_S] = { 0xFD, 0xA8 },
    [WOP_I32X4_EXTEND_LOW_I16X8_U] = { 0xFD, 0xA9 },
    [WOP_I32X4_EXTEND_HIGH_I16X8_U] = { 0xFD, 0xAA },
    [WOP_I32X4_SHL] = { 0xFD, 0xAB },
    [WOP_I32X4_SHR_S] = { 0xFD, 0xAC },
    [WOP_I32X4_SHR_U] = { 0xFD, 0xAD },
    [WOP_I32X4_ADD] = { 0xFD, 0xAE },
    [WOP_I32X4_SUB] = { 0xFD, 0xB1 },
    [WOP_I32X4_MUL] = { 0xFD, 0xB5 },
    [WOP_I32X4_MIN_S] = { 0xFD, 0xB6 },
    [WOP_I32X4_MIN_U] = { 0xFD, 0xB7 },
    [WOP_I32X4_MAX_S] = { 0xFD, 0xB8 },
    [WOP_I32X4_MAX_U] = { 0xFD, 0xB9 },
    [WOP_I32X4_DOT_I16X8_S] = { 0xFD, 0xBA },
    [WOP_I32X4_EXTMUL_LOW_I16X8_S] = { 0xFD, 0xBC },
    [WOP_I32X4_EXTMUL_HIGH_I16X8_S] = { 0xFD, 0xBD },
    [WOP_I32X4_EXTMUL_LOW_I16X8_U] = { 0xFD, 0xBE },
    [WOP_I32X4_EXTMUL_HIGH_I16X8_U] = { 0xFD, 0xBF },
    [WOP_I64X2_ABS] = { 0xFD, 0xC0 },
    [WOP_I64X2_NEG] = { 0xFD, 0xC1 },
    [WOP_I64X2_ALL_TRUE] = { 0xFD, 0xC3 },
    [WOP_I64X2_BITMASK] = { 0xFD, 0xC4 },
    [WOP_I64X2_EXTEND_LOW_I32X4_S] = { 0xFD, 0xC7 },
    [WOP_I64X2_EXTEND_HIGH_I32X4_S] = { 0xFD, 0xC8 },
    [WOP_I64X2_EXTEND_LOW_I32X4_U] = { 0xFD, 0xC9 },
    [WOP_I64X2_EXTEND_HIGH_I32X4_U] = { 0xFD, 0xCA },
    [WOP_I64X2_SHL] = { 0xFD, 0xCB },
    [WOP_I64X2_SHR_S] = { 0xFD, 0xCC },
    [WOP_I64X2_SHR_U] = { 0xFD, 0xCD },
    [WOP_I64X2_ADD] = { 0xFD, 0xCE },
    [WOP_I64X2_SUB] = { 0xFD, 0xD1 },
    [WOP_I64X2_MUL] = { 0xFD, 0xD5 },
    [WOP_I64X2_EQ] = { 0xFD, 0xD6 },
    [WOP_I64X2_NE] = { 0xFD, 0xD7 },
    [WOP_I64X2_LT_S] = { 0xFD, 0xD8 },
    [WOP_I64X2_GT_S] = { 0xFD, 0xD9 },
    [WOP_I64X2_LE_S] = { 0xFD, 0xDA },
    [WOP_I64X2_GE_S] = { 0xFD, 0xDB },
    [WOP_I64X2_EXTMUL_LOW_I32X4_S] = { 0xFD, 0xDC },
    [WOP_I64X2_EXTMUL_HIGH_I32X4_S] = { 0xFD, 0xDD },
    [WOP_I64X2_EXTMUL_LOW_I32X4_U] = { 0xFD, 0xDE },
    [WOP_I64X2_EXTMUL_HIGH_I32X4_U] = { 0xFD, 0xDF },
    [WOP_F32X4_ABS] = { 0xFD, 0xE0 },
    [WOP_F32X4_NEG] = { 0xFD, 0xE1 },
    [WOP_F32X4_SQRT] = { 0xFD, 0xE3 },
    [WOP_F32X4_ADD] = { 0xFD, 0xE4 },
    [WOP_F32X4_SUB] = { 0xFD, 0xE5 },
    [WOP_F32X4_MUL] = { 0xFD, 0xE6 },
    [WOP_F32X4_DIV] = { 0xFD, 0xE7 },
    [WOP_F32X4_MIN] = { 0xFD, 0xE8 },
    [WOP_F32X4_MAX] = { 0xFD, 0xE9 },
    [WOP_F32X4_PMIN] = { 0xFD, 0xEA },
    [WOP_F32X4_PMAX] = { 0xFD, 0xEB },
    [WOP_F64X2_ABS] = { 0xFD, 0xEC },
    [WOP_F64X2_NEG] = { 0xFD, 0xED },
    [WOP_F64X2_SQRT] = { 0xFD, 0xEF },
    [WOP_F64X2_ADD] = { 0xFD, 0xF0 },
    [WOP_F64X2_SUB] = { 0xFD, 0xF1 },
    [WOP_F64X2_MUL] = { 0xFD, 0xF2 },
    [WOP_F64X2_DIV] = { 0xFD, 0xF3 },
    [WOP_F64X2_MIN] = { 0xFD, 0xF4 },
    [WOP_F64X2_MAX] = { 0xFD, 0xF5 },
    [WOP_F64X2_PMIN] = { 0xFD, 0xF6 },
    [WOP_F64X2_PMAX] = { 0xFD, 0xF7 },
    [WOP_I32X4_TRUNC_SAT_F32X4_S] = { 0xFD, 0xF8 },
    [WOP_I32X4_TRUNC_SAT_F32X4_U] = { 0xFD, 0xF9 },
    [WOP_F32X4_CONVERT_I32X4_S] = { 0xFD, 0xFA },
    [WOP_F32X4_CONVERT_I32X4_U] = { 0xFD, 0xFB },
    [WOP_I32X4_TRUNC_SAT_F64X2_S_ZERO] = { 0xFD, 0xFC },
    [WOP_I32X4_TRUNC_SAT_F64X2_U_ZERO] = { 0xFD, 0xFD },
    [WOP_F64X2_CONVERT_LOW_I32X4_S] = { 0xFD, 0xFE },
    [WOP_F64X2_CONVERT_LOW_I32X4_U] = { 0xFD, 0xFF },
    [WOP_I8X16_RELAXED_SWIZZLE] = { 0xFD, 0x100 },
    [WOP_I32X4_RELAXED_TRUNC_F32X4_S] = { 0xFD, 0x101 },
    [WOP_I32X4_RELAXED_TRUNC_F32X4_U] = { 0xFD, 0x102 },
    [WOP_I32X4_RELAXED_TRUNC_F64X2_S_ZERO] = { 0xFD, 0x103 },
    [WOP_I32X4_RELAXED_TRUNC_F64X2_U_ZERO] = { 0xFD, 0x104 },
    [WOP_F32X4_RELAXED_MADD] = { 0xFD, 0x105 },
    [WOP_F32X4_RELAXED_NMADD] = { 0xFD, 0x106 },
    [WOP_F64X2_RELAXED_MADD] = { 0xFD, 0x107 },
    [WOP_F64X2_RELAXED_NMADD] = { 0xFD, 0x108 },
    [WOP_I8X16_RELAXED_LANESELECT] = { 0xFD, 0x109 },
    [WOP_I16X8_RELAXED_LANESELECT] = { 0xFD, 0x10A },
    [WOP_I32X4_RELAXED_LANESELECT] = { 0xFD, 0x10B },
    [WOP_I64X2_RELAXED_LANESELECT] = { 0xFD, 0x10C },
    [WOP_F32X4_RELAXED_MIN] = { 0xFD, 0x10D },
    [WOP_F32X4_RELAXED_MAX] = { 0xFD, 0x10E },
    [WOP_F64X2_RELAXED_MIN] = { 0xFD, 0x10F },
    [WOP_F64X2_RELAXED_MAX] = { 0xFD, 0x110 },
    [WOP_I16X8_RELAXED_Q15MULR_S] = { 0xFD, 0x111 },
    [WOP_I16X8_RELAXED_DOT_I8X16_I7X16_S] = { 0xFD, 0x112 },
    [WOP_I32X4_RELAXED_DOT_I8X16_I7X16_ADD_S] = { 0xFD, 0x113 },
};

#endif /* WASM_OPS_H */
