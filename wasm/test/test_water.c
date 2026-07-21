// test_water.c — the P6 assembler gate, oracle-free: a hand-authored `.wat`
// module and the hand-encoded `.wasm` bytes for the SAME module (both derived
// from the spec by hand, no external tool), asserting that the real assemble
// driver — text → wat reader → jav_module_t → bbq writer → bytes — reproduces
// the fixture exactly. This composes the two already-gated halves (text==binary
// struct equivalence, and read∘write byte identity) through the actual water
// code path, which nothing else exercises end to end.

#include "wat_driver.h"
#include "jav_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int fails = 0;

// Assemble `wat`, serialize, and compare to `expect` (the hand-encoded bytes).
static void check(const char *name, const char *wat, const uint8_t *expect, size_t en) {
    int line = 0, col = 0;
    jav_module_t *m = wat_assemble(wat, (int)strlen(wat), "../spec/instructions.toml", &line, &col);
    int ok = m != NULL;
    if (!ok) { printf("  %-40s [FAIL parse %d:%d]\n", name, line, col); fails++; return; }

    bbq_write_ctx_t w;
    bbq_write_ctx_init_growable(&w, en + 64);
    bbq_write_set_endian(&w, true);
    ok = jav_module_write(&w, m) && w.pos == en && memcmp(w.data, expect, en) == 0;
    if (!ok) {
        printf("  %-40s [FAIL: in=%zu out=%zu]\n", name, en, w.pos);
        size_t lim = w.pos < en ? w.pos : en;
        for (size_t i = 0; i < lim; i++)
            if (w.data[i] != expect[i]) { printf("      first diff at byte %zu: got 0x%02X want 0x%02X\n",
                                                 i, w.data[i], expect[i]); break; }
        fails++;
    } else {
        printf("  %-40s [PASS %zu bytes]\n", name, en);
    }
    bbq_write_ctx_free(&w);
    jav_module_free(m);
    free(m);
}

#define PRE 0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00   // magic + version

int main(void) {
    // (1) empty module — just the 8-byte preamble.
    {
        static const uint8_t b[] = { PRE };
        check("empty module", "(module)", b, sizeof b);
    }

    // (2) one func [i32]->[i32] returning its param. Sections, in binary id order:
    //   type(1): 01 | size 06 | count 01 | func 60 | 01 7f (param i32) | 01 7f (result i32)
    //   func(3): 03 | size 02 | count 01 | typeidx 00
    //   code(10): 0a | size 06 | count 01 | entry: size 04 | locals 00 | 20 00 (local.get 0) | 0b
    {
        static const uint8_t b[] = {
            PRE,
            0x01, 0x06, 0x01, 0x60, 0x01,0x7f, 0x01,0x7f,
            0x03, 0x02, 0x01, 0x00,
            0x0a, 0x06, 0x01, 0x04, 0x00, 0x20,0x00, 0x0b,
        };
        check("func: identity i32->i32",
              "(module (func (param i32) (result i32) local.get 0))", b, sizeof b);
    }

    // (3) typeuse + export + memory + active data — exercises multiple sections,
    //   the $id resolution, and an init expr through the writer.
    //   type(1):   01 06 01 60 02 7f 7f 01 7f                 (i32 i32) -> i32
    //   func(3):   03 02 01 00
    //   mem(5):    05 03 01 00 01                              limits {min 1}
    //   export(7): 07 0e 02
    //                07 "memory"(6) 02 00                      "memory" -> mem 0... wait below
    //   We keep names short: export "m"(mem 0) and "a"(func 0).
    //   code(10):  0a 09 01 07 00 20 00 20 01 6a 0b           local.get 0/1, i32.add
    //   data(11):  0b 07 01 00 41 00 0b 01 78                 active mem0, (i32.const 0), "x"
    {
        static const uint8_t b[] = {
            PRE,
            0x01, 0x07, 0x01, 0x60, 0x02,0x7f,0x7f, 0x01,0x7f,
            0x03, 0x02, 0x01, 0x00,
            0x05, 0x03, 0x01, 0x00, 0x01,
            0x07, 0x09, 0x02, 0x01,0x6d, 0x02,0x00, 0x01,0x61, 0x00,0x00,
            0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,0x00, 0x20,0x01, 0x6a, 0x0b,
            0x0b, 0x07, 0x01, 0x00, 0x41,0x00, 0x0b, 0x01, 0x78,
        };
        check("func+mem+export+data",
              "(module"
              " (type $bin (func (param i32 i32) (result i32)))"
              " (memory (export \"m\") 1)"
              " (func (export \"a\") (type $bin) local.get 0 local.get 1 i32.add)"
              " (data (i32.const 0) \"x\"))",
              b, sizeof b);
    }

    printf("\nP6 water assembler: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
