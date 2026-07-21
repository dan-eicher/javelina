// test_leb.c — P0 gate for the LEB128 write primitives (bbq_write_uleb128/sleb128),
// the inverse of the existing readers. Grounded in WebAssembly §5.2.2 "Integers":
//   - encodings are LSB-first base-128;
//   - a producer emits the CANONICAL (minimal-length) form;
//   - read∘write must be the identity.
// Tests: exact canonical byte sequences for known values, minimal length, and the
// round-trip property over boundary values for u32/u64/i32/i64.

#include "bbq_runtime.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static int fails = 0;
static void check(const char *name, int ok) {
    printf("  %-44s [%s]\n", name, ok ? "PASS" : "FAIL");
    if (!ok) fails++;
}

// ── Exact-encoding helpers ────────────────────────────────────────────────
static int uleb_is(uint64_t v, int width, const uint8_t *exp, size_t n) {
    uint8_t buf[16]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    bool ok = (width == 64) ? bbq_write_uleb128_u64(&w, v)
                            : bbq_write_uleb128_u32(&w, (uint32_t)v);
    return ok && w.pos == n && memcmp(buf, exp, n) == 0;
}
static int sleb_is(int64_t v, int width, const uint8_t *exp, size_t n) {
    uint8_t buf[16]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    bool ok = (width == 64) ? bbq_write_sleb128_i64(&w, v)
                            : bbq_write_sleb128_i32(&w, (int32_t)v);
    return ok && w.pos == n && memcmp(buf, exp, n) == 0;
}

// ── Round-trip helpers (write, then read back with the strict reader) ──────
static int rt_u32(uint32_t v) {
    uint8_t buf[8]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    if (!bbq_write_uleb128_u32(&w, v)) return 0;
    bbq_ctx_t c; bbq_ctx_init(&c, buf, w.pos);
    uint32_t got; if (!bbq_read_uleb128_u32(&c, &got)) return 0;
    return got == v && c.pos == w.pos;        // value preserved AND all bytes consumed
}
static int rt_u64(uint64_t v) {
    uint8_t buf[16]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    if (!bbq_write_uleb128_u64(&w, v)) return 0;
    bbq_ctx_t c; bbq_ctx_init(&c, buf, w.pos);
    uint64_t got; if (!bbq_read_uleb128_u64(&c, &got)) return 0;
    return got == v && c.pos == w.pos;
}
static int rt_i32(int32_t v) {
    uint8_t buf[8]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    if (!bbq_write_sleb128_i32(&w, v)) return 0;
    bbq_ctx_t c; bbq_ctx_init(&c, buf, w.pos);
    int32_t got; if (!bbq_read_sleb128_i32(&c, &got)) return 0;
    return got == v && c.pos == w.pos;
}
static int rt_i64(int64_t v) {
    uint8_t buf[16]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, buf, sizeof buf);
    if (!bbq_write_sleb128_i64(&w, v)) return 0;
    bbq_ctx_t c; bbq_ctx_init(&c, buf, w.pos);
    int64_t got; if (!bbq_read_sleb128_i64(&c, &got)) return 0;
    return got == v && c.pos == w.pos;
}

int main(void) {
    // Canonical ULEB encodings (LSB-first base-128).
    check("uleb 0   = 00",        uleb_is(0,   32, (uint8_t[]){0x00}, 1));
    check("uleb 1   = 01",        uleb_is(1,   32, (uint8_t[]){0x01}, 1));
    check("uleb 127 = 7F (1 byte, minimal)",
                                  uleb_is(127, 32, (uint8_t[]){0x7F}, 1));
    check("uleb 128 = 80 01",     uleb_is(128, 32, (uint8_t[]){0x80,0x01}, 2));
    check("uleb 624485 = E5 8E 26",
                                  uleb_is(624485, 32, (uint8_t[]){0xE5,0x8E,0x26}, 3));
    check("uleb UINT32_MAX = FF FF FF FF 0F",
                                  uleb_is(0xFFFFFFFFu, 32,
                                          (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0x0F}, 5));
    check("uleb UINT64_MAX = FF*9 01",
                                  uleb_is(0xFFFFFFFFFFFFFFFFull, 64,
                                          (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,
                                                      0xFF,0xFF,0xFF,0xFF,0x01}, 10));

    // Canonical SLEB encodings (sign-extended, minimal).
    check("sleb 0   = 00",        sleb_is(0,   32, (uint8_t[]){0x00}, 1));
    check("sleb -1  = 7F",        sleb_is(-1,  32, (uint8_t[]){0x7F}, 1));
    check("sleb 63  = 3F",        sleb_is(63,  32, (uint8_t[]){0x3F}, 1));
    check("sleb 64  = C0 00 (sign bit forces 2 bytes)",
                                  sleb_is(64,  32, (uint8_t[]){0xC0,0x00}, 2));
    check("sleb -64 = 40",        sleb_is(-64, 32, (uint8_t[]){0x40}, 1));
    check("sleb -123456 = C0 BB 78",
                                  sleb_is(-123456, 32, (uint8_t[]){0xC0,0xBB,0x78}, 3));
    check("sleb -2 = 7E (the §5.2.2 s16 example)",
                                  sleb_is(-2,  32, (uint8_t[]){0x7E}, 1));

    // Round-trip read∘write == identity over boundary values.
    int u32_ok = 1;
    uint32_t u32v[] = {0,1,63,64,127,128,255,256,16383,16384,
                       0x7FFFFFFFu,0x80000000u,0xFFFFFFFFu};
    for (size_t i = 0; i < sizeof u32v/sizeof *u32v; i++) u32_ok &= rt_u32(u32v[i]);
    check("uleb32 round-trip over boundaries", u32_ok);

    int u64_ok = 1;
    uint64_t u64v[] = {0,1,127,128,0xFFFFFFFFull,0x100000000ull,
                       0x7FFFFFFFFFFFFFFFull,0x8000000000000000ull,0xFFFFFFFFFFFFFFFFull};
    for (size_t i = 0; i < sizeof u64v/sizeof *u64v; i++) u64_ok &= rt_u64(u64v[i]);
    check("uleb64 round-trip over boundaries", u64_ok);

    int i32_ok = 1;
    int32_t i32v[] = {0,1,-1,63,-64,64,-65,127,-128,128,
                      2147483647,(-2147483647-1)};
    for (size_t i = 0; i < sizeof i32v/sizeof *i32v; i++) i32_ok &= rt_i32(i32v[i]);
    check("sleb32 round-trip over boundaries", i32_ok);

    int i64_ok = 1;
    int64_t i64v[] = {0,1,-1,63,-64,0x7FFFFFFFLL,-0x80000000LL,
                      0x7FFFFFFFFFFFFFFFLL,(-0x7FFFFFFFFFFFFFFFLL-1)};
    for (size_t i = 0; i < sizeof i64v/sizeof *i64v; i++) i64_ok &= rt_i64(i64v[i]);
    check("sleb64 round-trip over boundaries", i64_ok);

    printf("\nP0 LEB codec: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
