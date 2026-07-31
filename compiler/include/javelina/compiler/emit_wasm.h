/* emit_wasm.h — WASM instruction emitter for the codegen_wasm.burg actions.
 *
 * The substantial encoding (opcodes, LEB128) lives here, NOT in the .burg
 * preamble — the burg's inline section stays tiny selection predicates only
 * (yoctojc's grammar-embedded helpers ballooned; we don't repeat that).
 *
 * This is the value-instruction emitter: it appends WASM bytecode to a growable
 * byte buffer. The structured control (loop/if/br_table) is the structurer's
 * job, not emitted here. WASM-GC object encodings (struct.new, ref.cast, …) get
 * added as the burg grows; this is the arithmetic/conversion/const/local core. */
#ifndef EMIT_WASM_H
#define EMIT_WASM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>     /* memcpy — ew_bytes */
#include "bbq_vec.h"
#include "gen/wasm_ops.h"   /* WOP_* + wasm_op_enc[] — the spec-derived emit authority */

/* Opcodes are NOT defined here — they come from wasm_ops.h, generated from
 * spec/instructions.toml (single source of truth for the bytes we write).
 * What stays here is encoding the toml's opcode column doesn't cover: valtypes
 * and the functype tag (structural §5.3 bytes, not instructions). */
enum {
    W_VT_I32 = 0x7F, W_VT_I64 = 0x7E, W_VT_F32 = 0x7D, W_VT_F64 = 0x7C, W_VT_V128 = 0x7B, /* §5.3.5 valtypes */
    W_FUNCTYPE_FORM = 0x60,                                            /* §5.3.6 functype tag */
    /* Structured-control framing bytes — §5.4.1 terminators, excluded from the
     * instruction table (they delimit, they don't compute). */
    W_END = 0x0B, W_ELSE = 0x05,
};

/* Emit context — a growable byte buffer (bbq_vec of uint8_t). */
typedef struct { uint8_t* code; } emit_wasm_ctx;

static inline void ew_byte(emit_wasm_ctx* e, uint8_t b) { bbq_vec_push(e->code, b); }

/* Bulk append — one reserve, one memcpy. The byte-at-a-time loops this replaces
 * pushed whole method bodies (and once, the whole serialized module) through
 * ew_byte's per-byte growth check. */
static inline void ew_bytes(emit_wasm_ctx* e, const uint8_t* p, size_t n) {
    if (!n) return;
    int old = bbq_vec_len(e->code);
    bbq_vec_reserve(e->code, old + (int)n);
    memcpy(e->code + old, p, n);
    bbq__vec_hdr(e->code)->len = old + (int)n;
}

/* Unsigned LEB128. */
static inline void ew_u32(emit_wasm_ctx* e, uint32_t v) {
    do { uint8_t b = v & 0x7F; v >>= 7; if (v) b |= 0x80; ew_byte(e, b); } while (v);
}

/* Emit an instruction by identity: a single-byte opcode, or a prefix byte
 * followed by the uleb sub-opcode (the 0xFC/0xFD families). The encoding comes
 * from wasm_op_enc[] — emitters never hardcode opcode bytes. */
static inline void ew_emit(emit_wasm_ctx* e, wasm_op_t op) {
    wasm_op_enc_t enc = wasm_op_enc[op];
    if (enc.prefix) { ew_byte(e, enc.prefix); ew_u32(e, enc.opcode); }
    else            { ew_byte(e, (uint8_t)enc.opcode); }
}

/* How many bytes ew_emit writes for `op`, from the same wasm_op_enc[] row it
 * encodes from. The selection grammar needs this: a tile whose opcode is a node
 * PAYLOAD (the SIMD families) emits 2 bytes for a sub-opcode below 0x80 and 3
 * above, so one static cost cannot be its byte count — the width has to pick the
 * rule. Deriving it here rather than restating it in the grammar keeps one
 * authority for what an instruction weighs. */
static inline int ew_op_width(wasm_op_t op) {
    wasm_op_enc_t enc = wasm_op_enc[op];
    if (!enc.prefix) return 1;
    int n = 1;
    for (uint32_t v = enc.opcode; ; v >>= 7) { n++; if (!(v >> 7)) break; }
    return n;
}
/* Signed LEB128 (i32 / i64). */
static inline void ew_i64(emit_wasm_ctx* e, int64_t v) {
    for (;;) {
        uint8_t b = v & 0x7F; v >>= 7;
        bool done = (v == 0 && !(b & 0x40)) || (v == -1 && (b & 0x40));
        if (!done) b |= 0x80;
        ew_byte(e, b);
        if (done) break;
    }
}
static inline void ew_i32(emit_wasm_ctx* e, int32_t v) { ew_i64(e, (int64_t)v); }

/* IEEE little-endian constants. */
static inline void ew_f32(emit_wasm_ctx* e, float v) {
    union { float f; uint32_t u; } x = { .f = v };
    for (int i = 0; i < 4; i++) ew_byte(e, (uint8_t)(x.u >> (8 * i)));
}
static inline void ew_f64(emit_wasm_ctx* e, double v) {
    union { double d; uint64_t u; } x = { .d = v };
    for (int i = 0; i < 8; i++) ew_byte(e, (uint8_t)(x.u >> (8 * i)));
}

/* A function type (§5.3.6): 0x60, vec(param valtypes), vec(result valtypes). */
static inline void ew_functype(emit_wasm_ctx* e,
                               const uint8_t* params, uint32_t np,
                               const uint8_t* results, uint32_t nr) {
    ew_byte(e, W_FUNCTYPE_FORM);
    ew_u32(e, np); for (uint32_t i = 0; i < np; i++) ew_byte(e, params[i]);
    ew_u32(e, nr); for (uint32_t i = 0; i < nr; i++) ew_byte(e, results[i]);
}

/* A code-body local declaration (§5.4.5): vec of (count, valtype), run-length
 * compressing consecutive equal valtypes. `v` is one valtype per local. */
static inline void ew_locals(emit_wasm_ctx* e, const uint8_t* v, uint32_t n) {
    uint32_t runs = 0;
    for (uint32_t i = 0; i < n; ) { uint32_t j = i; while (j < n && v[j] == v[i]) j++; runs++; i = j; }
    ew_u32(e, runs);
    for (uint32_t i = 0; i < n; ) {
        uint32_t j = i; while (j < n && v[j] == v[i]) j++;
        ew_u32(e, j - i); ew_byte(e, v[i]); i = j;
    }
}

#endif /* EMIT_WASM_H */
