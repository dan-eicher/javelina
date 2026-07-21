// test_malformed.c — P5 (parser tier). Proves BBQ rejects ill-ENCODED / locally
// ill-formed bytes during the parse, using values it finds while parsing:
// magic/version constraints, the strict LEB readers (overlong/over-wide), reads
// past the section/EOF bound, and the fail-closed discriminant switches +
// field `where` clauses. (Whole-module CONFORMANCE — section order, func==code
// count, datacount match — is the next level up, the validator's job, not the
// parser's, so it is intentionally not tested here.)

#include "jav_reader.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;

// Each case is a (possibly malformed) module image that jav_module_read MUST reject.
static int rejected(const uint8_t *b, size_t n) {
    bbq_ctx_t c; bbq_ctx_init(&c, b, n);
    jav_module_t m; memset(&m, 0, sizeof m);
    int rej = jav_module_read(&c, &m) == false;
    jav_module_free(&m);                 // free the partial tree from the rejected parse
    bbq_ctx_free(&c);
    return rej;
}
#define CASE(name, ...) do { \
    static const uint8_t b[] = __VA_ARGS__; \
    int ok = rejected(b, sizeof b); \
    printf("  reject %-34s [%s]\n", name, ok ? "PASS" : "FAIL"); \
    if (!ok) fails++; \
} while (0)

// Accept counterpart: a module image that MUST parse (boundary-valid encodings).
static int accepted(const uint8_t *b, size_t n) {
    bbq_ctx_t c; bbq_ctx_init(&c, b, n);
    jav_module_t m; memset(&m, 0, sizeof m);
    int ok = jav_module_read(&c, &m) && bbq_at_end(&c);
    jav_module_free(&m);
    bbq_ctx_free(&c);
    return ok;
}
#define ACCEPT(name, ...) do { \
    static const uint8_t b[] = __VA_ARGS__; \
    int ok = accepted(b, sizeof b); \
    printf("  accept %-34s [%s]\n", name, ok ? "PASS" : "FAIL"); \
    if (!ok) fails++; \
} while (0)

// magic + version preamble bytes, reused below.
#define PRE 0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00

int main(void) {
    // Preamble / framing.
    CASE("bad magic",            {0x00,0x61,0x73,0x6E, 0x01,0x00,0x00,0x00});
    CASE("bad version",          {0x00,0x61,0x73,0x6D, 0x02,0x00,0x00,0x00});
    CASE("truncated preamble",   {0x00,0x61,0x73,0x6D, 0x01,0x00});
    // Section size claims 10 bytes; only 2 present -> read runs off the end.
    CASE("section size past EOF", {PRE, 0x01,0x0A, 0x60,0x00});
    // Section size as a 6-byte ULEB exceeds the u32 carrier -> strict reader rejects.
    CASE("overlong uleb32 size",  {PRE, 0x01, 0xFF,0xFF,0xFF,0xFF,0xFF,0x7F});

    // Local structural / discriminant rejects (fail-closed grammar).
    CASE("unknown section id 14", {PRE, 0x0E,0x00});
    CASE("invalid valtype 0x00",  {PRE, 0x01,0x05, 0x01, 0x60,0x01,0x00,0x00}); // type: func param 0x00
    CASE("invalid rectype 0x61",  {PRE, 0x01,0x02, 0x01, 0x61});                // type: rectype head 0x61
    CASE("bad limits flag 0x08",  {PRE, 0x05,0x03, 0x01, 0x08,0x01});           // memory: flag bit3
    CASE("bad import kind 0x05",  {PRE, 0x02,0x04, 0x01, 0x00,0x00,0x05});      // import "" "" kind 5
    CASE("bad elem flag 8",       {PRE, 0x09,0x02, 0x01, 0x08});                // element: segment flag 8
    // global init expr with no terminating `end` (0x0B) -> Expr never closes.
    CASE("unterminated init expr", {PRE, 0x06,0x05, 0x01, 0x7F,0x01, 0x41,0x00});

    // §5.4.5 memarg: the align/flags field must be < 2^7 (bit 6 = the multi-memory
    // memidx flag; bits 7+ are malformed — align 2**128 / 2**256 in the suite).
    CASE("memop flags 0x80",     {PRE, 0x01,0x04,0x01,0x60,0x00,0x00, 0x03,0x02,0x01,0x00,
                                  0x05,0x03,0x01,0x00,0x01,
                                  0x0A,0x0B,0x01, 0x09,0x00, 0x41,0x00, 0x28,0x80,0x01,0x00, 0x1A,0x0B});
    CASE("memop flags 0x100",    {PRE, 0x01,0x04,0x01,0x60,0x00,0x00, 0x03,0x02,0x01,0x00,
                                  0x05,0x03,0x01,0x00,0x01,
                                  0x0A,0x0B,0x01, 0x09,0x00, 0x41,0x00, 0x28,0x80,0x02,0x00, 0x1A,0x0B});

    // §5.5.2 section framing: `size` declares an exact window. A body that consumes
    // LESS than size (1 type declared, 2 encoded) is a size mismatch; a size that
    // extends past the enclosing bound (custom section claiming 0x26 with fewer
    // bytes present) is out of bounds, never silently clamped.
    CASE("section size mismatch (extra entry)", {PRE, 0x01,0x07,0x01, 0x60,0x00,0x00, 0x60,0x00,0x00});
    CASE("section size past EOF (custom)", {PRE, 0x00,0x26,0x10,
                                  'a',' ','c','u','s','t','o','m',' ','s','e','c','t','i','o','n',
                                  't','h','i','s',' ','i','s',' ','t','h','e',' ','p','a','y','l','o','a','d'});

    // ── §5.2.4 names: must be valid UTF-8 (the spec's `utf8` grammar — Unicode
    // scalar values only: no truncation, no stray/bad continuation, no overlongs,
    // no surrogates, nothing above U+10FFFF, no 5/6-byte forms). Each invalid
    // class below is one fixture, carried in the smallest Name-bearing container
    // (a custom section: id 0, size, name, no payload); one import and one export
    // fixture prove the same Name production guards every section that names.
    CASE("utf8: stray continuation 0x80",   {PRE, 0x00,0x02, 0x01,0x80});
    CASE("utf8: truncated 2-byte (C2)",     {PRE, 0x00,0x02, 0x01,0xC2});
    CASE("utf8: bad continuation (C2 20)",  {PRE, 0x00,0x03, 0x02,0xC2,0x20});
    CASE("utf8: overlong 2-byte (C0 80)",   {PRE, 0x00,0x03, 0x02,0xC0,0x80});
    CASE("utf8: overlong 2-byte (C1 BF)",   {PRE, 0x00,0x03, 0x02,0xC1,0xBF});
    CASE("utf8: truncated 3-byte (E2 82)",  {PRE, 0x00,0x03, 0x02,0xE2,0x82});
    CASE("utf8: overlong 3-byte (E0 80 80)",{PRE, 0x00,0x04, 0x03,0xE0,0x80,0x80});
    CASE("utf8: overlong 3-byte (E0 9F BF)",{PRE, 0x00,0x04, 0x03,0xE0,0x9F,0xBF});
    CASE("utf8: surrogate (ED A0 80)",      {PRE, 0x00,0x04, 0x03,0xED,0xA0,0x80});
    CASE("utf8: surrogate (ED BF BF)",      {PRE, 0x00,0x04, 0x03,0xED,0xBF,0xBF});
    CASE("utf8: truncated 4-byte (F0 9F 92)",{PRE, 0x00,0x04, 0x03,0xF0,0x9F,0x92});
    CASE("utf8: overlong 4-byte (F0 80 80 80)",{PRE, 0x00,0x05, 0x04,0xF0,0x80,0x80,0x80});
    CASE("utf8: overlong 4-byte (F0 8F BF BF)",{PRE, 0x00,0x05, 0x04,0xF0,0x8F,0xBF,0xBF});
    CASE("utf8: above U+10FFFF (F4 90 80 80)",{PRE, 0x00,0x05, 0x04,0xF4,0x90,0x80,0x80});
    CASE("utf8: invalid first byte F5",     {PRE, 0x00,0x05, 0x04,0xF5,0x80,0x80,0x80});
    CASE("utf8: invalid first byte F8 (5-byte)",{PRE, 0x00,0x02, 0x01,0xF8});
    CASE("utf8: invalid first byte FF",     {PRE, 0x00,0x02, 0x01,0xFF});
    CASE("utf8: bad import module name",    {PRE, 0x02,0x07, 0x01, 0x02,0xC0,0x80, 0x00, 0x00,0x00});
    CASE("utf8: bad export name",           {PRE, 0x07,0x06, 0x01, 0x02,0xC0,0x80, 0x00,0x00});
    // Boundary-VALID encodings the validator must not over-reject (first/last
    // code point of each width; ED 9F BF = U+D7FF, EE 80 80 = U+E000 bracket the
    // surrogate hole; F4 8F BF BF = U+10FFFF).
    ACCEPT("utf8: ascii edge 7F",            {PRE, 0x00,0x02, 0x01,0x7F});
    ACCEPT("utf8: 2-byte edges (C2 80, DF BF)",{PRE, 0x00,0x05, 0x04,0xC2,0x80,0xDF,0xBF});
    ACCEPT("utf8: 3-byte edges (E0 A0 80, ED 9F BF, EE 80 80, EF BF BF)",
           {PRE, 0x00,0x0D, 0x0C,0xE0,0xA0,0x80,0xED,0x9F,0xBF,0xEE,0x80,0x80,0xEF,0xBF,0xBF});
    ACCEPT("utf8: 4-byte edges (F0 90 80 80, F4 8F BF BF)",
           {PRE, 0x00,0x09, 0x08,0xF0,0x90,0x80,0x80,0xF4,0x8F,0xBF,0xBF});

    printf("\nP5 malformed (parser tier): %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
