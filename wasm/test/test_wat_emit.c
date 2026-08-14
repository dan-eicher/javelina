// test_wat_emit.c — the §6 text, against the string the spec itself prints.
//
// PIN C-0a/C-0b. Authored with PIN A-1 (test_wat_fold.c), before any of Part A's
// code, because it is the pin that makes "just dump the instructions" fail: a flat
// listing is valid §6 text and would pass every other gate in this tree.
#include "wat_emit.h"
#include "wat_check.h"
#include "jav_reader.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
static void CK(const char *msg, long got, long want) {
    int ok = (got == want);
    printf("  %-58s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

#define PRE 0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00   // magic + version

// (module (type (func (param i32) (result i32)))
//         (func (type 0) local.get 0  i32.const 2  i32.add  i32.const 3  i32.mul))
#define SPEC_EXAMPLE_MODULE                                     \
    PRE,                                                        \
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,             \
    0x03, 0x02, 0x01, 0x00,                                     \
    0x0a, 0x0c, 0x01, 0x0a, 0x00,                               \
          0x20, 0x00, 0x41, 0x02, 0x6a, 0x41, 0x03, 0x6c, 0x0b

// Render `bytes`; returns the text (from `a`) or NULL if §7.6 rejected the module.
static const char *emit(const uint8_t *bytes, size_t n, int width,
                        jav_module_t *mod, bbq_arena *a) {
    bbq_ctx_t cx;
    bbq_ctx_init(&cx, bytes, n);
    memset(mod, 0, sizeof *mod);
    if (!jav_module_read(&cx, mod)) { bbq_ctx_free(&cx); return NULL; }
    bbq_ctx_free(&cx);
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *wcx = wat_check_ctx_build(mod, a, &err);
    if (!wcx) return NULL;
    const char *out = NULL; size_t len = 0;
    if (!wat_emit_module(mod, wcx, width, a, &out, &len)) return NULL;
    return out;
}

/* ── PIN C-0a / C-0b — SpecFoldExample ──────────────────────────────────────────
 *
 * §6.5.11's Note (printed 233), verbatim:
 *
 *   "For example, the instruction sequence
 *        (local.get $x) (i32.const 2) i32.add (i32.const 3) i32.mul
 *    can be folded into
 *        (i32.mul (i32.add (local.get $x) (i32.const 2)) (i32.const 3))"
 *
 * C-0a is that binary rendered with §6.6.1's numeric indices, which is all a `.wasm`
 * carries. C-0b is the same binary with a §7.7.1 name section binding local 0 of
 * func 0 to "x", which is where `$x` comes from — §6.6.1: "Indices can be given
 * either in raw numeric form or as symbolic identifiers when bound by a respective
 * construct." C-0b is the spec's string exactly, and closes Part D.
 *
 * Asserted as a substring: the surrounding module layout is PIN C-1's and PIN C-3's
 * subject, this pin's subject is the fold.
 */
static void spec_fold_example(void) {
    printf("SpecFoldExample: §6.5.11's Note, rendered\n");
    const char *want_a = "(i32.mul (i32.add (local.get 0) (i32.const 2)) (i32.const 3))";
    const char *want_b = "(i32.mul (i32.add (local.get $x) (i32.const 2)) (i32.const 3))";

    // C-0a — numeric indices (closes Part C).
    {
        static const uint8_t wasm[] = { SPEC_EXAMPLE_MODULE };
        jav_module_t mod; bbq_arena a;
        bbq_arena_init(&a, 8192);
        const char *txt = emit(wasm, sizeof wasm, 100, &mod, &a);
        CK("C-0a renders", txt != NULL, 1);
        if (txt) {
            int hit = strstr(txt, want_a) != NULL;
            if (!hit) printf("      want: %s\n      got:  %s\n", want_a, txt);
            CK("C-0a is the spec's fold, numeric", hit, 1);
        }
        bbq_arena_free(&a);
    }

    // C-0b — the same binary plus a name section (closes Part D).
    //   custom "name": localnamesubsec (id 2) { func 0 { local 0 -> "x" } }
    {
        static const uint8_t wasm[] = {
            SPEC_EXAMPLE_MODULE,
            0x00, 0x0d, 0x04, 'n', 'a', 'm', 'e',          // custom section "name"
                  0x02, 0x06,                              //   subsec 2 (local names), 6 bytes
                        0x01, 0x00,                        //     1 entry: func 0
                        0x01, 0x00, 0x01, 'x',             //       1 name: local 0 -> "x"
        };
        jav_module_t mod; bbq_arena a;
        bbq_arena_init(&a, 8192);
        const char *txt = emit(wasm, sizeof wasm, 100, &mod, &a);
        CK("C-0b renders", txt != NULL, 1);
        if (txt) {
            int hit = strstr(txt, want_b) != NULL;
            if (!hit) printf("      want: %s\n      got:  %s\n", want_b, txt);
            CK("C-0b is the spec's fold, verbatim", hit, 1);
            // §7.7.3: the name section itself rides in a @custom annotation, never
            // as @name — emitting both would create two sections.
            CK("...and the name section is preserved as @custom",
               strstr(txt, "(@custom \"name\"") != NULL, 1);
            CK("...and no @name is emitted", strstr(txt, "(@name") == NULL, 1);
        }
        bbq_arena_free(&a);
    }
}

int main(void) {
    spec_fold_example();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
