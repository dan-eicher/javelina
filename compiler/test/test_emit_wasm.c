// test_emit_wasm.c — byte-exact encoding of the WASM value emitter (emit_wasm.h):
// opcodes, unsigned/signed LEB128, and IEEE little-endian f32/f64 constants.
// These bytes are what the whole backend serializes, so they're pinned exactly.
#include "javelina/compiler/emit_wasm.h"
#include "javelina/compiler/codegen_wasm.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

static int eq(emit_wasm_ctx* e, const uint8_t* exp, int n) {
    return bbq_vec_len(e->code) == n && memcmp(e->code, exp, (size_t)n) == 0;
}
#define CASE(emit, m, ...) do { \
    emit_wasm_ctx e = {0}; emit; \
    const uint8_t want[] = { __VA_ARGS__ }; \
    CHECK(eq(&e, want, (int)sizeof want), m); \
    bbq_vec_free(e.code); } while (0)

int main(void) {
    /* opcodes — by identity, encoded from the generated wasm_ops.h table */
    CASE(ew_emit(&e, WOP_I32_ADD), "i32.add opcode", 0x6A);
    CASE(ew_emit(&e, WOP_F64_MUL), "f64.mul opcode", 0xA2);
    /* a 0xFC-prefixed op: prefix byte then uleb sub-opcode (the dispatch-view
     * opcodes.h can't express this; instructions.toml can) */
    CASE(ew_emit(&e, WOP_I32_TRUNC_SAT_F64_S), "i32.trunc_sat_f64_s", 0xFC, 0x02);

    /* signed LEB128 (i32 / i64 share the encoder) */
    CASE(ew_i32(&e, 5),    "sleb 5",    0x05);
    CASE(ew_i32(&e, -1),   "sleb -1",   0x7F);
    CASE(ew_i32(&e, 127),  "sleb 127",  0xFF, 0x00);
    CASE(ew_i32(&e, -128), "sleb -128", 0x80, 0x7F);
    CASE(ew_i64(&e, 300),  "sleb 300",  0xAC, 0x02);
    CASE(ew_i64(&e, 0),    "sleb 0",    0x00);

    /* unsigned LEB128 (local/func indices) */
    CASE(ew_u32(&e, 0),   "uleb 0",   0x00);
    CASE(ew_u32(&e, 128), "uleb 128", 0x80, 0x01);
    CASE(ew_u32(&e, 624485), "uleb 624485", 0xE5, 0x8E, 0x26);

    /* IEEE 754 little-endian constants */
    CASE(ew_f32(&e, 1.5f), "f32 1.5",  0x00, 0x00, 0xC0, 0x3F);          /* 0x3FC00000 */
    CASE(ew_f64(&e, 2.25), "f64 2.25", 0,0,0,0,0,0, 0x02, 0x40);         /* 0x4002000000000000 */

    /* datatype → valtype map (byte/short/char/int → i32; long/float/double; ref) */
    CHECK(wasm_valtype(SIR_DTINT)    == W_VT_I32, "int → i32");
    CHECK(wasm_valtype(SIR_DTBYTE)   == W_VT_I32, "byte → i32");
    CHECK(wasm_valtype(SIR_DTSHORT)  == W_VT_I32, "short → i32");
    CHECK(wasm_valtype(SIR_DTCHAR)   == W_VT_I32, "char → i32");
    CHECK(wasm_valtype(SIR_DTLONG)   == W_VT_I64, "long → i64");
    CHECK(wasm_valtype(SIR_DTFLOAT)  == W_VT_F32, "float → f32");
    CHECK(wasm_valtype(SIR_DTDOUBLE) == W_VT_F64, "double → f64");

    /* functype encoding §5.3.6 */
    { emit_wasm_ctx e = {0};
      uint8_t p[] = { W_VT_I32, W_VT_I32 }, r[] = { W_VT_I32 };
      ew_functype(&e, p, 2, r, 1);
      const uint8_t w[] = { 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F };
      CHECK(eq(&e, w, (int)sizeof w), "functype (i32,i32)->i32"); bbq_vec_free(e.code); }
    { emit_wasm_ctx e = {0};
      uint8_t r[] = { W_VT_F64 };
      ew_functype(&e, NULL, 0, r, 1);      /* ()->f64 */
      const uint8_t w[] = { 0x60, 0x00, 0x01, 0x7C };
      CHECK(eq(&e, w, (int)sizeof w), "functype ()->f64"); bbq_vec_free(e.code); }

    /* locals vec §5.4.5 (run-length) */
    { emit_wasm_ctx e = {0};
      ew_locals(&e, NULL, 0);
      const uint8_t w[] = { 0x00 };
      CHECK(eq(&e, w, 1), "no locals → 0 runs"); bbq_vec_free(e.code); }
    { emit_wasm_ctx e = {0};
      uint8_t v[] = { W_VT_I32, W_VT_I32, W_VT_I64 };
      ew_locals(&e, v, 3);
      const uint8_t w[] = { 0x02, 0x02, 0x7F, 0x01, 0x7E };  /* (2×i32)(1×i64) */
      CHECK(eq(&e, w, (int)sizeof w), "locals RLE [i32,i32,i64]"); bbq_vec_free(e.code); }

    if (fails) { printf("test_emit_wasm: %d FAILED\n", fails); return 1; }
    printf("test_emit_wasm: OK\n");
    return 0;
}
