// test_jit_sweep.c — broad interp==JIT differential over the surfaces not yet
// exercised in the JIT: conversions, f64, comparisons, bit-ops. The differential
// flushes JIT-specific bugs (like the f32.const reinterpret bug).
#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
static slot_t run(const uint8_t* c, size_t n, int jit) {
    vm_t v; memset(&v, 0, sizeof v); jav_vm_init(&v); bbq_ctx_init(&v.frame.code, c, n);
    if (jit) jav_jit_run(&v); else interp_run(&v, NULL);
    return jav_tos(&v);
}
static int fails = 0;
#define CK(label, fld, fmt, ...) do { uint8_t b[] = {__VA_ARGS__}; \
    slot_t i = run(b,sizeof b,0), j = run(b,sizeof b,1); int ok = (i.fld == j.fld); \
    printf("  %-22s interp=" fmt " jit=" fmt " [%s]\n", label, i.fld, j.fld, ok?"PASS":"FAIL"); fails += !ok; } while (0)
int main(void) {
    printf("-- conversions --\n");
    CK("i32.wrap_i64",    i, "%d",   0x42,0x07, 0xa7, 0x0b);                 // i64.const 7; wrap
    CK("i64.extend_i32_s",l, "%ld", 0x41,0x7f, 0xac, 0x0b);                 // i32.const -1; extend_s
    CK("i64.extend_i32_u",l, "%ld", 0x41,0x7f, 0xad, 0x0b);                 // -> 0xFFFFFFFF
    CK("i32.extend8_s",   i, "%d",   0x41,0x80,0x01, 0xc0, 0x0b);            // i32.const 128; -> -128
    CK("f32.convert_i32_s",f,"%g",   0x41,0x05, 0xb2, 0x0b);                 // 5 -> 5.0f
    CK("f64.convert_i32_s",d,"%g",   0x41,0x05, 0xb7, 0x0b);                 // 5 -> 5.0
    CK("f32.demote_f64",  f, "%g",   0x44,0,0,0,0,0,0,0x0c,0x40, 0xb6, 0x0b);// 3.5 -> 3.5f
    CK("f64.promote_f32", d, "%g",   0x43,0,0,0x20,0x40, 0xbb, 0x0b);        // 2.5f -> 2.5
    CK("i32.reinterpret_f32",i,"%d", 0x43,0,0,0x80,0x3f, 0xbc, 0x0b);        // 1.0f -> 0x3F800000
    CK("f32.reinterpret_i32",f,"%g", 0x41,0x80,0x80,0x80,0xfc,0x03, 0xbe, 0x0b); // 0x3F800000 -> 1.0f
    printf("-- f64 arithmetic --\n");
    CK("f64.add",  d, "%g", 0x44,0,0,0,0,0,0,0x08,0x40, 0x44,0,0,0,0,0,0,0,0x40, 0xa0, 0x0b); // 3.0+2.0
    CK("f64.lt",   i, "%d", 0x44,0,0,0,0,0,0,0,0x40, 0x44,0,0,0,0,0,0,0x08,0x40, 0x63, 0x0b); // 2.0<3.0 ->1
    printf("-- comparisons --\n");
    CK("i32.eq(5,5)",   i, "%d", 0x41,0x05, 0x41,0x05, 0x46, 0x0b);
    CK("i32.lt_s(3,5)", i, "%d", 0x41,0x03, 0x41,0x05, 0x48, 0x0b);
    CK("i64.eq(7,7)",   i, "%d", 0x42,0x07, 0x42,0x07, 0x51, 0x0b);
    CK("f32.lt(1,2)",   i, "%d", 0x43,0,0,0x80,0x3f, 0x43,0,0,0,0x40, 0x5d, 0x0b);
    printf("-- bit ops --\n");
    CK("i32.clz(1)",    i, "%d", 0x41,0x01, 0x67, 0x0b);                     // -> 31
    CK("i32.popcnt(7)", i, "%d", 0x41,0x07, 0x69, 0x0b);                     // -> 3
    CK("i32.rotl(1,4)", i, "%d", 0x41,0x01, 0x41,0x04, 0x77, 0x0b);          // -> 16
    CK("i32.shl(1,4)",  i, "%d", 0x41,0x01, 0x41,0x04, 0x74, 0x0b);          // -> 16
    printf("\nbroad interp == JIT sweep: %s\n", fails?"FAIL":"ALL PASS");
    return fails ? 1 : 0;
}
