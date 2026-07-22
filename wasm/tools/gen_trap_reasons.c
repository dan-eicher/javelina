/* gen_trap_reasons.c — the execution-trap reason vocabulary, from the spec.
 *
 * Joins the two authorities and emits src/gen/jav_trap_reason.h:
 *
 *   spec/instructions.toml  `traps`  — the canonical message text, extracted from
 *                                      the reference interpreter's raise sites.
 *                                      This is the ONLY place trap text comes from.
 *   spec/wasm.def           `error:` — which reasons the spec actually declares a
 *                                      guard for. opgen turns each name into
 *                                      OPGEN_ERR_<Name>; the emitted bridge maps
 *                                      those back to a reason.
 *
 * A guard name is the message in CamelCase ("integer divide by zero" ->
 * IntegerDivideByZero) — mechanical, so the two files cannot drift silently. A
 * declared name with no matching message is a hard error, applied at
 * generation time.
 *
 * Reasons raised by the substrate rather than by a declared guard (the natives in
 * jav_runtime.c / jav_mem.h) still get an enum member — the vocabulary is the
 * spec's complete set, not the subset opgen happens to declare today.
 */
#include "toml/toml_doc.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REASONS 64
#define MAX_DECLARED 64

static char  g_msg[MAX_REASONS][128];   /* canonical text, first-appearance order */
static char  g_name[MAX_REASONS][128];  /* CamelCase of the same */
static int   g_nreasons;
static char  g_decl[MAX_DECLARED][128]; /* names wasm.def declares a guard for */
static int   g_ndecl;

/* "out of bounds memory access" -> "OutOfBoundsMemoryAccess". Word chars only;
 * the digits in "null i31 reference" ride through, giving NullI31Reference. */
static void camel(const char* s, char* out, size_t cap) {
    size_t o = 0; int start = 1;
    for (const char* p = s; *p && o + 1 < cap; p++) {
        if (*p == ' ') { start = 1; continue; }
        char c = *p;
        if (start && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[o++] = c; start = 0;
    }
    out[o] = 0;
}

static int reason_index(const char* msg) {
    for (int i = 0; i < g_nreasons; i++) if (!strcmp(g_msg[i], msg)) return i;
    return -1;
}

static char* slurp(const char* path, int* len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gen_trap_reasons: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "gen_trap_reasons: cannot read %s\n", path); exit(1);
    }
    b[n] = 0; fclose(f); *len = (int)n; return b;
}

/* Collect the distinct trap messages across every [[instr]], in the order the
 * spec table lists them. */
static void collect_reasons(const char* toml_path, bbq_arena* ar) {
    int len; char* src = slurp(toml_path, &len);
    toml_doc_t* doc = toml_parse(src, len, ar);
    if (toml_doc_has_errors(doc)) {
        fprintf(stderr, "gen_trap_reasons: %s: %s\n", toml_path,
                toml_doc_error_at(doc, 0)->message);
        exit(1);
    }
    const toml_val_t* instrs = toml_tbl_get(toml_doc_root(doc), "instr");
    int n = toml_val_array_count(instrs);
    if (n == 0) { fprintf(stderr, "gen_trap_reasons: no [[instr]] entries\n"); exit(1); }
    for (int i = 0; i < n; i++) {
        const toml_tbl_t* t = toml_val_as_table(toml_val_array_at(instrs, i));
        if (!t) continue;
        const toml_val_t* traps = toml_tbl_get(t, "traps");
        if (traps) for (int j = 0; j < toml_val_array_count(traps); j++) {
            const char* msg;
            if (!toml_val_as_string(toml_val_array_at(traps, j), &msg)) continue;
            if (reason_index(msg) >= 0) continue;
            if (g_nreasons == MAX_REASONS) {
                fprintf(stderr, "gen_trap_reasons: more than %d reasons\n", MAX_REASONS);
                exit(1);
            }
            snprintf(g_msg[g_nreasons], sizeof g_msg[0], "%s", msg);
            camel(msg, g_name[g_nreasons], sizeof g_name[0]);
            g_nreasons++;
        }
    }
    free(src);
}

/* Scan wasm.def for `error: (. cond .) -> Name` and record Name. */
static void collect_declared(const char* def_path) {
    int len; char* src = slurp(def_path, &len);
    for (char* p = src; (p = strstr(p, "error:")) != NULL; ) {
        char* arrow = strstr(p, "->");
        char* eol   = strchr(p, '\n');
        if (!arrow || (eol && arrow > eol)) { p += 6; continue; }
        char* q = arrow + 2;
        while (*q == ' ' || *q == '\t') q++;
        char name[128]; size_t o = 0;
        while (o + 1 < sizeof name &&
               ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                (*q >= '0' && *q <= '9'))) name[o++] = *q++;
        name[o] = 0;
        if (o) {
            int dup = 0;
            for (int i = 0; i < g_ndecl; i++) if (!strcmp(g_decl[i], name)) dup = 1;
            if (!dup) {
                if (g_ndecl == MAX_DECLARED) {
                    fprintf(stderr, "gen_trap_reasons: more than %d declared guards\n",
                            MAX_DECLARED);
                    exit(1);
                }
                snprintf(g_decl[g_ndecl++], sizeof g_decl[0], "%s", name);
            }
        }
        p = arrow;
    }
    free(src);
}

static int name_index(const char* name) {
    for (int i = 0; i < g_nreasons; i++) if (!strcmp(g_name[i], name)) return i;
    return -1;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <instructions.toml> <wasm.def> <out.h>\n", argv[0]);
        return 1;
    }
    bbq_arena ar; bbq_arena_init(&ar, 0);
    collect_reasons(argv[1], &ar);
    collect_declared(argv[2]);

    /* The cross-check: every declared guard name must name a spec trap message. */
    int bad = 0;
    for (int i = 0; i < g_ndecl; i++) {
        if (name_index(g_decl[i]) < 0) {
            fprintf(stderr, "gen_trap_reasons: wasm.def declares `-> %s`, which is not a "
                            "trap message in %s\n", g_decl[i], argv[1]);
            bad = 1;
        }
    }
    if (bad) return 1;

    FILE* o = fopen(argv[3], "wb");
    if (!o) { fprintf(stderr, "gen_trap_reasons: cannot write %s\n", argv[3]); return 1; }

    fprintf(o,
"/* GENERATED by tools/gen_trap_reasons.c from spec/instructions.toml + spec/wasm.def.\n"
" * Do not edit. The message text is the reference interpreter's, carried by the\n"
" * `traps` column of the spec table; this header is the single place it lives on\n"
" * the execution side, as jav_err_str is on the validation side. */\n"
"#ifndef JAV_TRAP_REASON_H\n"
"#define JAV_TRAP_REASON_H\n"
"\n"
"/* X(Name, \"message\") over the spec's complete trap vocabulary, in table order. */\n"
"#define JAV_TRAP_REASONS(X) \\\n");
    for (int i = 0; i < g_nreasons; i++)
        fprintf(o, "    X(%s, \"%s\")%s\n", g_name[i], g_msg[i],
                i + 1 < g_nreasons ? " \\" : "");

    fprintf(o,
"\n"
"typedef enum {\n"
"    JAV_TRAP_NONE = 0,\n"
"#define X(n, s) JAV_TRAP_##n,\n"
"    JAV_TRAP_REASONS(X)\n"
"#undef X\n"
"    JAV_TRAP_REASON_COUNT\n"
"} jav_trap_reason_t;\n"
"\n"
"static inline const char* jav_trap_reason_str(jav_trap_reason_t r) {\n"
"    switch (r) {\n"
"#define X(n, s) case JAV_TRAP_##n: return s;\n"
"    JAV_TRAP_REASONS(X)\n"
"#undef X\n"
"    case JAV_TRAP_NONE: case JAV_TRAP_REASON_COUNT: break;\n"
"    }\n"
"    return \"trap\";\n"
"}\n"
"\n"
"/* Bridge from opgen's guard code to a reason. A MACRO, for two reasons that both\n"
" * bite: it expands inside the generated handler/stencil bodies, i.e. AFTER\n"
" * runtime_api.h has declared OPGEN_ERR_* — so the mapping is written over the\n"
" * NAMES and never assumes opgen's declaration-order numbering — and because `e`\n"
" * is a literal at every guard site the whole chain folds to a constant. A real\n"
" * function would be an unresolved symbol in a copy-and-patch stencil (jitterator\n"
" * rejects any non-_HOLE_ relocation). */\n");
    if (g_ndecl == 0) {
        fprintf(o, "#define JAV_REASON_OF_OPGEN(e) ((void)(e), JAV_TRAP_NONE)\n");
    } else {
        fprintf(o, "#define JAV_REASON_OF_OPGEN(e) ( \\\n");
        for (int i = 0; i < g_ndecl; i++)
            fprintf(o, "    (e) == OPGEN_ERR_%s ? JAV_TRAP_%s : \\\n", g_decl[i], g_decl[i]);
        fprintf(o, "    JAV_TRAP_NONE)\n");
    }
    fprintf(o, "\n#endif /* JAV_TRAP_REASON_H */\n");
    fclose(o);

    fprintf(stderr, "gen_trap_reasons: %d spec trap reasons, %d declared by wasm.def\n",
            g_nreasons, g_ndecl);
    bbq_arena_free(&ar);
    return 0;
}
