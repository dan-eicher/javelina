/* codegen_wasm.h — SIR-aware WASM codegen support (the burg's preamble + the
 * structurer/emit use these). emit_wasm.h is pure WASM byte encoding; this layer
 * knows the SIR, so the JLS→WASM-valtype map lives here, not in emit_wasm.h. */
#ifndef CODEGEN_WASM_H
#define CODEGEN_WASM_H

#include "gen/sir_ast.h"
#include "javelina/compiler/emit_wasm.h"

/* JLS width → WASM valtype (§5.3.5). byte/short/char/int all compute as i32 (the
 * value-model work made this the optimizer's model too); long→i64, float→f32,
 * double→f64. ref → a GC ref — left as a forward placeholder (eqref) until the
 * WASM-GC object model (S5.10) assigns concrete struct types per class. */
static inline uint8_t wasm_valtype(sir_datatype_t dt) {
    switch (dt) {
        case SIR_DTLONG:   return W_VT_I64;
        case SIR_DTFLOAT:  return W_VT_F32;
        case SIR_DTDOUBLE: return W_VT_F64;
        case SIR_DTV128:   return W_VT_V128;
        case SIR_DTREF:    return 0x6D;   /* eqref (§5.3.4) — placeholder pending GC types */
        default:           return W_VT_I32;  /* byte / short / char / int */
    }
}

#endif /* CODEGEN_WASM_H */
