// test_wat_custom.c — §7.7: nothing is lost, and names become identifiers.
//
// CustomSectionSurvives — a module with custom sections at NAMED positions
// (§7.7.3 placement directives) goes wasm → wat → wasm BYTE-IDENTICALLY.
// Falsified by dropping the placement directive: the sections come back in
// the wrong order, and the bytes say so.
//
// NamesBecomeIdentifiers — a §7.7.1 name section turns into §6.6.1
// identifiers at every binding and use site, the name section itself rides
// its @custom annotation (never @name — §7.7.3's note: emitting both would
// create two sections), the sanitizer holds (a non-idchar name and a
// duplicate name both fall back to numerals), and the round trip is STILL
// byte-identical — identifiers are erased by re-parsing, which is what makes
// readability free.
#include "jav_reader.h"
#include "jav_writer.h"
#include "wat_check.h"
#include "wat_driver.h"
#include "wat_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void CK(const char *msg, long got, long want) {
    int ok = (got == want);
    printf("  %-58s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

#define PRE 0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00

// Render `bytes`; NULL if reading or §7 refused.
static const char *emit(const uint8_t *bytes, size_t n, jav_module_t *mod,
                        bbq_arena *a, size_t *tlen) {
    bbq_ctx_t cx;
    bbq_ctx_init(&cx, bytes, n);
    memset(mod, 0, sizeof *mod);
    if (!jav_module_read(&cx, mod)) { bbq_ctx_free(&cx); return NULL; }
    bbq_ctx_free(&cx);
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *wcx = wat_check_ctx_build(mod, a, &err);
    if (!wcx || !wat_check_module(wcx, a, &err)) return NULL;
    const char *out = NULL;
    if (!wat_emit_module(mod, wcx, 100, a, &out, tlen)) return NULL;
    return out;
}

// wat → wasm through the real assembler + writer; 1 if the bytes match.
static int reassembles_identically(const char *txt, size_t tlen,
                                   const uint8_t *want, size_t wantlen) {
    int line = 0, col = 0;
    jav_module_t *m = wat_assemble(txt, (int)tlen, &line, &col);
    if (!m) { printf("      re-assemble failed at %d:%d\n", line, col); return 0; }
    bbq_write_ctx_t w;
    bbq_write_ctx_init_growable(&w, wantlen + 64);
    bbq_write_set_endian(&w, true);
    int wrote = jav_module_write(&w, m);
    jav_module_free(m);
    free(m);
    if (!wrote) { bbq_write_ctx_free(&w); printf("      re-write failed\n"); return 0; }
    int same = (w.pos == wantlen) && memcmp(w.data, want, wantlen) == 0;
    if (!same) {
        printf("      %zu bytes back vs %zu original", (size_t)w.pos, wantlen);
        for (size_t i = 0; i < w.pos && i < wantlen; i++)
            if (w.data[i] != want[i]) { printf(", first diff at %zu", i); break; }
        printf("\n");
    }
    bbq_write_ctx_free(&w);
    return same;
}

/* ── PIN D-1 — CustomSectionSurvives ──────────────────────────────────── */
static void custom_section_survives(void) {
    printf("CustomSectionSurvives: §7.7.3 placement, byte-exact\n");
    static const uint8_t wasm[] = {
        PRE,
        0x00, 0x06, 0x03, 'p','r','e', 0xaa, 0xbb,             /* custom "pre" before type   */
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,              /* (type (func (result i32))) */
        0x03, 0x02, 0x01, 0x00,                                /* func section               */
        0x00, 0x05, 0x03, 'm','i','d', 0x11,                   /* custom "mid" before code   */
        0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x07, 0x0b,        /* code: i32.const 7          */
        0x00, 0x05, 0x04, 'l','a','s','t',                     /* custom "last", trailing    */
    };
    jav_module_t mod;
    bbq_arena a;
    bbq_arena_init(&a, 1 << 16);
    size_t tlen = 0;
    const char *txt = emit(wasm, sizeof wasm, &mod, &a, &tlen);
    CK("renders", txt != NULL, 1);
    if (txt) {
        CK("three @custom annotations",
           (strstr(txt, "(@custom \"pre\"") != NULL) +
           (strstr(txt, "(@custom \"mid\"") != NULL) +
           (strstr(txt, "(@custom \"last\"") != NULL), 3);
        CK("placements are explicit",
           strstr(txt, "(@custom \"pre\" (before ") != NULL &&
           strstr(txt, "(@custom \"mid\" (after func)") != NULL, 1);
        CK("round-trips byte-exact",
           reassembles_identically(txt, tlen, wasm, sizeof wasm), 1);
    }
    bbq_arena_free(&a);
    jav_module_free(&mod);
}

/* ── PIN D-3 — NamesBecomeIdentifiers ─────────────────────────────────── */
static void names_become_identifiers(void) {
    printf("NamesBecomeIdentifiers: §7.7.1 names, sanitized, byte-exact\n");
    /* (type (func (param i32) (result i32))), two funcs, and a name section:
     * module "m"; funcs: 0 -> "compute", 1 -> "compute" (DUPLICATE: only the
     * first binds), local names: func 0 local 0 -> "n", func 1 local 0 ->
     * "bad name" (space: not §6.3.5-clean, falls back numeric). */
    static const uint8_t wasm[] = {
        PRE,
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
        0x03, 0x03, 0x02, 0x00, 0x00,
        0x0a, 0x10, 0x02,
              0x07, 0x00, 0x20, 0x00, 0x41, 0x02, 0x6a, 0x0b, /* local.get 0; i32.const 2; add */
              0x06, 0x01, 0x02, 0x7e, 0x20, 0x00, 0x0b,       /* (local i64 i64); local.get 0  */
        0x00, 0x35, 0x04, 'n','a','m','e',
              0x00, 0x02, 0x01, 'm',                          /* module name "m"           */
              0x01, 0x13, 0x02,                               /* func names, 2 entries     */
                    0x00, 0x07, 'c','o','m','p','u','t','e',
                    0x01, 0x07, 'c','o','m','p','u','t','e',  /* duplicate spelling        */
              0x02, 0x15, 0x02,                               /* local names, 2 funcs      */
                    0x00, 0x01, 0x00, 0x01, 'n',
                    0x01, 0x02, 0x00, 0x08, 'b','a','d',' ','n','a','m','e',
                                0x01, 0x01, 'L',              /* names a local INSIDE a 2-run */
    };
    jav_module_t mod;
    bbq_arena a;
    bbq_arena_init(&a, 1 << 16);
    size_t tlen = 0;
    const char *txt = emit(wasm, sizeof wasm, &mod, &a, &tlen);
    CK("renders", txt != NULL, 1);
    if (txt) {
        CK("(module $m", strstr(txt, "(module $m") != NULL, 1);
        CK("(func $compute (type 0) (param $n i32)",
           strstr(txt, "(func $compute (type 0) (param $n i32)") != NULL, 1);
        CK("uses spell the id: (local.get $n)",
           strstr(txt, "(local.get $n)") != NULL, 1);
        const char* first = strstr(txt, "$compute");
        CK("duplicate name falls back to a numeral (one $compute only)",
           first != NULL && strstr(first + 8, "$compute") == NULL, 1);
        CK("non-idchar name falls back: (local.get 0) in func 1",
           strstr(txt, "$bad") == NULL, 1);
        CK("a name inside a >1 RLE run is dropped (bytes over ids)",
           strstr(txt, "$L") == NULL && strstr(txt, "(local i64 i64)") != NULL, 1);
        CK("the name section rides @custom",
           strstr(txt, "(@custom \"name\"") != NULL, 1);
        CK("no @name is emitted", strstr(txt, "(@name") == NULL, 1);
        CK("round-trips byte-exact",
           reassembles_identically(txt, tlen, wasm, sizeof wasm), 1);
        if (fails) printf("----\n%s----\n", txt);
    }
    bbq_arena_free(&a);
    jav_module_free(&mod);
}

int main(void) {
    custom_section_survives();
    names_become_identifiers();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
