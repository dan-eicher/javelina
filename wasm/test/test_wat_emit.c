// test_wat_emit.c — the §6 text, against the string the spec itself prints.
//
// PIN C-0a/C-0b. Authored with PIN A-1 (test_wat_fold.c), before any of Part A's
// code, because it is the pin that makes "just dump the instructions" fail: a flat
// listing is valid §6 text and would pass every other gate in this tree.
#include "wat_emit.h"
#include "wat_check.h"
#include "wat_driver.h"
#include "jav_reader.h"
#include "wat_mnemonics.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

// Assemble text, render it back at `width`. Both module objects stay live in
// the caller's slots so instruction pointers remain valid for bit compares.
static const char *assemble_and_emit(const char *src, int width,
                                     jav_module_t **mod_out, bbq_arena *a) {
    int line = 0, col = 0;
    jav_module_t *m = wat_assemble(src, (int)strlen(src), &line, &col);
    if (!m) { printf("      assemble failed at %d:%d\n", line, col); return NULL; }
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *wcx = wat_check_ctx_build(m, a, &err);
    if (!wcx) { printf("      ctx refused: %s\n", jav_err_str(err)); return NULL; }
    const char *out = NULL; size_t len = 0;
    if (!wat_emit_module(m, wcx, width, a, &out, &len)) {
        const char *stage = "";
        err = wat_emit_last_error(&stage);
        printf("      emit refused: stage=%s err=%s\n", stage, jav_err_str(err));
        return NULL;
    }
    *mod_out = m;
    return out;
}

/* ── PIN C-2 — TokensAreSeparated ───────────────────────────────────────────
 *
 * §6.2.2: "all tokens must be separated by either parentheses, white space, or
 * comments", and its Note names the failure: two adjoined tokens read back as ONE
 * reserved token. So the check re-lexes the emitted text by §6.2.2's own classes
 * and requires every idchar-run token to BE something — a known keyword, a §6.3
 * literal, an id, a memarg key — because a missing separator between two such
 * tokens produces a run that is none of them. Parens self-delimit, so only
 * run-run adjacency can merge, which is exactly what this classifies. */
static int tok_is_number(const char *t, size_t n) {
    size_t i = 0;
    if (i < n && (t[i] == '+' || t[i] == '-')) i++;
    if (i < n && !strncmp(t + i, "inf", 3) && i + 3 == n) return 1;
    if (i < n && !strncmp(t + i, "nan", 3)) {
        i += 3;
        if (i == n) return 1;
        if (t[i] != ':' || i + 3 > n || t[i + 1] != '0' || t[i + 2] != 'x') return 0;
        for (i += 3; i < n; i++)
            if (!isxdigit((unsigned char)t[i]) && t[i] != '_') return 0;
        return 1;
    }
    int hex = 0;
    if (i + 1 < n && t[i] == '0' && t[i + 1] == 'x') { hex = 1; i += 2; }
    int digits = 0, dot = 0, exp = 0;
    for (; i < n; i++) {
        char c = t[i];
        if (c == '_') continue;
        if (hex ? isxdigit((unsigned char)c) : isdigit((unsigned char)c)) { digits = 1; continue; }
        if (c == '.' && !dot && !exp) { dot = 1; continue; }
        if (!exp && digits && ((hex && (c == 'p' || c == 'P')) ||
                               (!hex && (c == 'e' || c == 'E')))) {
            exp = 1;
            if (i + 1 < n && (t[i + 1] == '+' || t[i + 1] == '-')) i++;
            hex = 0;   /* the exponent is decimal digits */
            continue;
        }
        return 0;
    }
    return digits;
}

static int tok_is_keyword(const char *t, size_t n) {
    static const char *const structural[] = {
        "module", "type", "func", "param", "result", "local", "global", "table",
        "memory", "tag", "import", "export", "start", "elem", "data", "declare",
        "item", "offset", "field", "mut", "rec", "sub", "final", "then", "else",
        "@custom", "before", "after", "first", "last",
        "i32", "i64", "f32", "f64", "v128", "i8", "i16", "i8x16", "i16x8",
        "i32x4", "i64x2", "f32x4", "f64x2",
        "funcref", "externref", "anyref", "eqref", "i31ref", "structref",
        "arrayref", "nullref", "nullexternref", "nullfuncref", "exnref",
        "nullexnref", "ref", "null", "extern", "any", "eq", "i31", "struct",
        "array", "none", "noextern", "nofunc", "exn", "noexn",
        "catch", "catch_ref", "catch_all", "catch_all_ref",
    };
    for (size_t k = 0; k < sizeof structural / sizeof structural[0]; k++)
        if (strlen(structural[k]) == n && !strncmp(structural[k], t, n)) return 1;
    for (size_t k = 0; k < sizeof wat_mnemonics / sizeof wat_mnemonics[0]; k++)
        if (strlen(wat_mnemonics[k].name) == n && !strncmp(wat_mnemonics[k].name, t, n))
            return 1;
    return 0;
}

static int tok_valid(const char *t, size_t n) {
    if (t[0] == '$') return n > 1;
    if (!strncmp(t, "offset=", 7)) return tok_is_number(t + 7, n - 7);
    if (!strncmp(t, "align=", 6)) return tok_is_number(t + 6, n - 6);
    if (!strncmp(t, "(;", 2) || !strncmp(t, ";;", 2)) return 0;   /* we never emit comments */
    return tok_is_number(t, n) || tok_is_keyword(t, n);
}

/* Count invalid idchar-run tokens in `txt`; report the first few. */
static int relex_bad_tokens(const char *txt) {
    static const char idchar_extra[] = "!#$%&'*+-./:<=>?@\\^_`|~";
    int bad = 0;
    for (const char *p = txt; *p; ) {
        if (*p == ' ' || *p == '\n') { p++; continue; }
        if (*p == '(' || *p == ')') { p++; continue; }
        if (*p == '"') {
            p++;
            while (*p && *p != '"') p += (*p == '\\') ? 2 : 1;
            if (*p) p++;
            continue;
        }
        const char *start = p;
        while (*p && (isalnum((unsigned char)*p) || strchr(idchar_extra, *p))) p++;
        if (p == start) { p++; continue; }   /* an un-lexable byte would be its own red */
        if (!tok_valid(start, (size_t)(p - start))) {
            if (bad < 5) printf("      bad token: %.*s\n", (int)(p - start), start);
            bad++;
        }
    }
    return bad;
}

static void tokens_are_separated(void) {
    printf("TokensAreSeparated: §6.2.2 re-lexed, every token classifies\n");
    /* Every adjacency hazard the emitter has: memarg pairs, negative and hex
     * float literals, label lists, lane lists, select's result, escaped
     * strings, a v128 const, packed fields, catches, and folded operands. */
    static const char *src =
        "(module\n"
        "  (type $s (struct (field (mut i8)) (field i16)))\n"
        "  (tag $e (param i32))\n"
        "  (memory 1)\n"
        "  (table 4 funcref)\n"
        "  (data (i32.const 0) \"a\\\"b\\\\c \\00\\ff\")\n"
        "  (elem (i32.const 0) func 0)\n"
        "  (func (param i32 i32) (result i32)\n"
        "    local.get 0\n"
        "    i32.load offset=4 align=2\n"
        "    i32.const -1\n"
        "    i32.and\n"
        "    block (result i32)\n"
        "      local.get 0\n"
        "      local.get 1\n"
        "      br_table 0 0 0\n"
        "    end\n"
        "    i32.add\n"
        "    f64.const -0x1.8p+3\n"
        "    i32.trunc_f64_s\n"
        "    i32.add\n"
        "    i32.const 9\n"
        "    local.get 0 local.get 1 i32.lt_s\n"
        "    select (result i32)\n"
        "    try_table (result i32) (catch $e 0)\n"
        "      i32.const 7\n"
        "    end\n"
        "    i32.add)\n"
        "  (func $v (result v128)\n"
        "    v128.const i32x4 0xdeadbeef 1 2 3\n"
        "    v128.const i32x4 4 5 6 7\n"
        "    i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))\n";
    jav_module_t *m = NULL;
    bbq_arena a;
    bbq_arena_init(&a, 1 << 20);
    const char *txt = assemble_and_emit(src, 100, &m, &a);
    CK("hazard module renders", txt != NULL, 1);
    if (txt) CK("invalid tokens after re-lexing", relex_bad_tokens(txt), 0);
    bbq_arena_free(&a);
    if (m) { jav_module_free(m); free(m); }
}

/* ── PIN C-3 — ImportsPrecedeDefinitions ────────────────────────────────────
 *
 * §6.6.13 ordered(decl*): no import may follow the first definition of a tag,
 * global, memory, table or function. The emitter renders sections in the
 * binary's own section order, which makes this hold by construction — this is
 * the gate that keeps it a fact rather than an intention. Scanned at the
 * module's own indent so an `(import …)` inside an export or a descriptor
 * never miscounts. */
static void imports_precede_definitions(void) {
    printf("ImportsPrecedeDefinitions: §6.6.13 ordered(decl*)\n");
    static const char *src =
        "(module\n"
        "  (import \"m\" \"f\" (func (param i32)))\n"
        "  (import \"m\" \"g\" (global i32))\n"
        "  (import \"m\" \"t\" (table 1 funcref))\n"
        "  (func (result i32) i32.const 1)\n"
        "  (table 2 funcref)\n"
        "  (memory 1)\n"
        "  (global i32 (i32.const 5))\n"
        "  (export \"one\" (func 1))\n"
        "  (export \"gg\" (global 1)))\n";
    jav_module_t *m = NULL;
    bbq_arena a;
    bbq_arena_init(&a, 1 << 20);
    const char *txt = assemble_and_emit(src, 100, &m, &a);
    CK("import module renders", txt != NULL, 1);
    if (txt) {
        int seen_def = 0, late_imports = 0, imports = 0;
        for (const char *p = txt; (p = strstr(p, "\n  (")) != NULL; p += 4) {
            const char *head = p + 4;
            if (!strncmp(head, "import", 6)) {
                imports++;
                if (seen_def) late_imports++;
            } else if (!strncmp(head, "func", 4) || !strncmp(head, "table", 5) ||
                       !strncmp(head, "memory", 6) || !strncmp(head, "global", 6) ||
                       !strncmp(head, "tag", 3)) {
                seen_def = 1;
            }
        }
        CK("imports found at module indent", imports, 3);
        CK("imports after the first definition", late_imports, 0);
    }
    bbq_arena_free(&a);
    if (m) { jav_module_free(m); free(m); }
}

/* ── PIN C-4 — FloatsRoundTripExactly ───────────────────────────────────────
 *
 * §6.3.2: "Rounding can be prevented by using hexadecimal notation" — and the
 * emitter takes that as the contract: hex floats, `inf`, `nan`, `nan:0x…`. The
 * check is a REAL round trip: assemble the fixture, render it, re-assemble the
 * rendering through the same reader the corpus uses, and compare every
 * f32/f64 immediate's BITS. NaN payloads, signed zeros and subnormals
 * included. Falsified by forcing decimal in the spelling. */
static void collect_float_bits(const jav_instr_t *items, size_t count,
                               uint64_t *bits, int *n, int cap) {
    for (size_t i = 0; i < count; i++) {
        const jav_instr_t *in = &items[i];
        if (in->op == 0x43 && *n < cap) {
            uint32_t b;
            memcpy(&b, &in->body.u.case_21.v, 4);
            bits[(*n)++] = b;
        } else if (in->op == 0x44 && *n < cap) {
            uint64_t b;
            memcpy(&b, &in->body.u.case_22.v, 8);
            bits[(*n)++] = b;
        } else if (in->op == 0x02 || in->op == 0x03) {
            collect_float_bits(in->body.u.case_1.instrs.items,
                               in->body.u.case_1.instrs.count, bits, n, cap);
        }
    }
}

static int module_float_bits(const jav_module_t *m, uint64_t *bits, int cap) {
    int n = 0;
    for (size_t s = 0; s < m->sections.count; s++) {
        if (m->sections.items[s].id != 10) continue;
        const jav_code_section_t *cs = &m->sections.items[s].body.u.case_10;
        for (size_t k = 0; k < cs->entries.count; k++)
            collect_float_bits(cs->entries.items[k].body.body.instrs.items,
                               cs->entries.items[k].body.body.instrs.count,
                               bits, &n, cap);
    }
    return n;
}

static void floats_round_trip_exactly(void) {
    printf("FloatsRoundTripExactly: §6.3.2, bits through the real reader\n");
    static const char *src =
        "(module (func\n"
        "  f64.const nan          drop\n"
        "  f64.const -nan         drop\n"
        "  f64.const nan:0x123abc drop\n"
        "  f64.const inf          drop\n"
        "  f64.const -inf         drop\n"
        "  f64.const -0x0p+0      drop\n"
        "  f64.const 0x0.0000000000001p-1022 drop\n"
        "  f64.const 0x1.fffffffffffffp+1023 drop\n"
        "  f64.const 1.5          drop\n"
        "  f32.const nan:0x1      drop\n"
        "  f32.const -nan         drop\n"
        "  f32.const 0x1p-149     drop\n"
        "  f32.const 0x1.fffffep+127 drop\n"
        "  f32.const -0x0p+0      drop))\n";
    jav_module_t *m1 = NULL, *m2 = NULL;
    bbq_arena a;
    bbq_arena_init(&a, 1 << 20);
    const char *txt = assemble_and_emit(src, 100, &m1, &a);
    CK("float module renders", txt != NULL, 1);
    if (txt) {
        int line = 0, col = 0;
        m2 = wat_assemble(txt, (int)strlen(txt), &line, &col);
        CK("rendering re-assembles", m2 != NULL, 1);
        if (!m2) printf("      at %d:%d in:\n%s", line, col, txt);
        if (m2) {
            uint64_t b1[32], b2[32];
            int n1 = module_float_bits(m1, b1, 32);
            int n2 = module_float_bits(m2, b2, 32);
            CK("float count", n1, 14);
            CK("float count matches", n2, n1);
            int diffs = 0;
            for (int i = 0; i < n1 && i < n2; i++)
                if (b1[i] != b2[i]) {
                    if (diffs < 4)
                        printf("      float %d: 0x%016llx != 0x%016llx\n", i,
                               (unsigned long long)b1[i], (unsigned long long)b2[i]);
                    diffs++;
                }
            CK("bit divergences", diffs, 0);
        }
    }
    bbq_arena_free(&a);
    if (m1) { jav_module_free(m1); free(m1); }
    if (m2) { jav_module_free(m2); free(m2); }
}

int main(void) {
    spec_fold_example();
    tokens_are_separated();
    imports_precede_definitions();
    floats_round_trip_exactly();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
