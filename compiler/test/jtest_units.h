/* jtest_units.h — the ONE prelude+user parse for compiler tests, §7.3-shaped.
 *
 * Replaces the per-test `build_program` flatten (which discarded each file's
 * package declaration and import list — the §7 information sema now needs).
 * jtest_build parses every prelude source as its OWN compilation unit, then
 * the user source as the final unit, and returns:
 *
 *   .units / .nunits   one ast_program_t* per §7.3 unit → sema_analyze_units
 *   .flat              one merged program (type decls only) → compiler_compile
 *   .nlib              prelude TYPE count → ctx->num_library_classes
 *
 * Parsing is FRESH per call: sema desugars into its input AST from the
 * caller's arena (see the test_exec prelude-cache notes), so a shared cached
 * AST is only safe under that test's stricter arena discipline — suites that
 * want the cache keep their own builder. Everything allocated here leaks for
 * the test's lifetime (the established convention in these suites).
 *
 * User snippets are ordinary §7.4.2 unnamed-package units: names outside
 * java.lang need their §7.5 import (e.g. `import javelina.simd.*;`) or a
 * fully qualified name — exactly what a real user writes. */
#ifndef JTEST_UNITS_H
#define JTEST_UNITS_H

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "java_parser.h"
#include "javelina/compiler/java_source.h"   /* §3.2 step 1 — the ONE parse entry (see header) */
#include "javelina/compiler/sema.h"
#include "javelina_test.h"

typedef struct {
    ast_program_t** units;   /* bbq_vec */
    int             nunits;
    ast_program_t*  flat;
    int             nlib;
} jtest_program_t;

static char* jtest_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}

static ast_program_t* jtest_parse(const char* src, const char* file) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof *pc);
    bbq_arena_init(&pc->arena, 1 << 16);
    pc->result = NULL; pc->file = file;
    peg_state p; char* tsrc = NULL; const char* terr = NULL;
    if (!java_source_init(&p, src, &tsrc, &terr)) return NULL;   /* §3.2 step 1 */
    p.user_data = pc;
    bool ok = java_parser_parse(&p);
    free(tsrc);
    return ok ? pc->result : NULL;
}

/* The prelude package roots, in registration order (java.lang lowest ids). */
static const char* JTEST_LIB_DIRS[] = {
    "lib/java/lang", "lib/java/util", "lib/java/io", "lib/javelina/simd",
};
#define JTEST_NLIB_DIRS 4

static bool jtest_build(const char* user_src, bbq_arena* arena, jtest_program_t* out) {
    memset(out, 0, sizeof *out);
    ast_type_decl_t** types = NULL;   /* bbq_vec — the flat decl list */
    for (int di = 0; di < JTEST_NLIB_DIRS; di++) {
        DIR* d = opendir(JTEST_LIB_DIRS[di]);
        if (!d) {
            printf("  FAIL  cannot open %s (run from compiler/)\n", JTEST_LIB_DIRS[di]);
            TEST_FAILED(); return false;
        }
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            size_t L = strlen(e->d_name);
            if (L < 6 || strcmp(e->d_name + L - 5, ".java") != 0) continue;
            char path[512];
            snprintf(path, sizeof path, "%s/%s", JTEST_LIB_DIRS[di], e->d_name);
            char* src = jtest_read_file(path);
            if (!src) { printf("  FAIL  read %s\n", path); TEST_FAILED(); continue; }
            char* pathdup = (char*)malloc(strlen(path) + 1);   /* diag file name; test-lifetime */
            strcpy(pathdup, path);
            ast_program_t* p = jtest_parse(src, pathdup);
            free(src);
            if (!p) { printf("  FAIL  parse %s\n", path); TEST_FAILED(); continue; }
            bbq_vec_push(out->units, p);
            for (int i = 0; i < p->types_count; i++) bbq_vec_push(types, p->types[i]);
        }
        closedir(d);
    }
    out->nlib = (int)bbq_vec_len(types);

    if (user_src) {
        ast_program_t* up = jtest_parse(user_src, NULL);
        if (!up) { printf("  FAIL  parse user source\n"); TEST_FAILED(); }
        else {
            bbq_vec_push(out->units, up);
            for (int i = 0; i < up->types_count; i++) bbq_vec_push(types, up->types[i]);
        }
    }

    int tc = (int)bbq_vec_len(types);
    ast_type_decl_t** arr = (ast_type_decl_t**)bbq_arena_alloc(arena, (size_t)tc * sizeof(*arr));
    memcpy(arr, types, (size_t)tc * sizeof(*arr));
    bbq_vec_free(types);
    out->flat = ast_program(arena, NULL, NULL, 0, arr, tc);
    out->nunits = (int)bbq_vec_len(out->units);
    return true;
}

/* ── Corpus snippet imports ────────────────────────────────────────────────
 * Corpus snippets are §7.4.2 unnamed-package units; names outside java.lang
 * need their §7.5 import. Execution-corpus harnesses splice this standard
 * header on so a thousand snippet strings don't each repeat it — the splice
 * is HERE, visible, not implicit in sema. The no-import-required negatives
 * live in test_sema's §7 block, which does NOT use the splice. */
#define JTEST_STD_IMPORTS "import java.util.*; import java.io.*; import javelina.simd.*; "

static inline const char* jtest_with_imports(const char* imports, const char* src) {
    if (!src) return NULL;
    size_t li = strlen(imports), ls = strlen(src);
    char* s = (char*)malloc(li + ls + 1);
    memcpy(s, imports, li); memcpy(s + li, src, ls + 1);
    return s;   /* leaked: test-lifetime */
}

/* ── Drop-ins for the old per-test flatten ─────────────────────────────────
 * jtest_build_flat matches the old `build_program(src, arena)` shape (the
 * FLAT program feeds compiler_compile); the parsed UNITS are stashed for
 * jtest_analyze, the sema_analyze drop-in that runs the §7.3-correct entry. */
static ast_program_t** jtest_last_units JT_UNUSED = NULL;
static int jtest_last_nunits JT_UNUSED = 0;
static int jtest_last_nlib   JT_UNUSED = 0;   /* prelude TYPE count → num_library_classes */

static inline ast_program_t* jtest_build_flat(const char* user_src, bbq_arena* arena) {
    jtest_program_t jp;
    if (!jtest_build(user_src, arena, &jp)) return NULL;
    jtest_last_units = jp.units;   /* leaked: test-lifetime, the suites' convention */
    jtest_last_nunits = jp.nunits;
    jtest_last_nlib = jp.nlib;
    return jp.flat;
}

static inline bool jtest_analyze(sema_ctx_t* ctx) {
    return sema_analyze_units(ctx, jtest_last_units, jtest_last_nunits);
}

#endif /* JTEST_UNITS_H */
