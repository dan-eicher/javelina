// test_module_validate.c — Phase 1: the §7 gate accepts valid modules and rejects
// invalid ones with the precise jav_err_t reason (whose jav_err_str is the official
// testsuite error string). These are byte-exact PINS, not the breadth gate — the breadth
// gate is the .wast corpus driven through the wasm-c-api (Phase 4). water is only the
// assembler, so it emits these structurally-fine but §7-invalid modules unchanged.
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>

static jav_status_t validate_file(const char* path, jav_err_t* err) {
    FILE* fp = fopen(path, "rb");
    if (!fp) { perror(path); exit(2); }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { perror("fread"); exit(2); }
    fclose(fp);
    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &a);
    jav_status_t r = JAV_MALFORMED; *err = JAV_E_NONE;
    if (m.success) {
        jav_modidx_t mod;
        if (jav_module_index(m.root, buf, &a, &mod)) {
            r = jav_module_validate(m.root, buf, &mod, err);
            jav_modidx_free_bodies(&mod);   // §7.6 side-tables stored in mod (freed by the C-API's module_delete)
        } else r = JAV_INVALID;
    }
    bbq_arena_free(&a); free(buf);
    return r;
}

int main(void) {
    struct { const char* path; jav_status_t status; jav_err_t err; } cases[] = {
        { "add.wasm",        JAV_OK,      JAV_E_NONE },
        { "rich.wasm",       JAV_OK,      JAV_E_NONE },
        { "refs.wasm",       JAV_OK,      JAV_E_NONE },
        { "bad_lim.wasm",    JAV_INVALID, JAV_E_SIZE_MIN_GT_MAX },           // §3.2.15 min(5) > max(2)
        { "bad_body.wasm",   JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §7.6 i64 where i32
        { "bad_dupexp.wasm", JAV_INVALID, JAV_E_DUPLICATE_EXPORT_NAME },    // §3.5.10 dup "a"
        { "bad_dupexp2.wasm",JAV_INVALID, JAV_E_DUPLICATE_EXPORT_NAME },    // §3.5.10 dup "a" at (0,3), not adjacent
        { "bad_dupexp3.wasm",JAV_INVALID, JAV_E_DUPLICATE_EXPORT_NAME },    // §3.5.10 dup "ad" behind a hash-colliding "bC"
        { "bad_start.wasm",  JAV_INVALID, JAV_E_START_FUNCTION },           // §3.5.12 start not [] -> []
        { "bad_gtype.wasm",  JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §3.5.3 init i64 != i32
        { "bad_gmut.wasm",   JAV_INVALID, JAV_E_CONST_EXPR_REQUIRED },      // §3.3.10 global.get MUTABLE
        { "bad_gfwd.wasm",   JAV_INVALID, JAV_E_UNKNOWN_GLOBAL },           // §3.5.3 global.get not-yet-defined
        { "bad_doff.wasm",   JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §3.5.8 data offset i64 != i32
        { "bad_doff2.wasm",  JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §3.5.8 data offset funcref
        { "bad_eoff.wasm",   JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §3.5.9 elem offset i64 != i32
        { "bad_ematch.wasm", JAV_INVALID, JAV_E_TYPE_MISMATCH },            // §3.5.9 funcref elem, externref table
        { "bad_efunc.wasm",  JAV_INVALID, JAV_E_UNKNOWN_FUNCTION },         // §3.5.9 elem func idx 9 OOR
    };
    int fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        jav_err_t got_err; jav_status_t got = validate_file(cases[i].path, &got_err);
        if (got != cases[i].status || got_err != cases[i].err) {
            printf("  FAIL %-16s want=(%d,\"%s\") got=(%d,\"%s\")\n", cases[i].path,
                   cases[i].status, jav_err_str(cases[i].err), got, jav_err_str(got_err));
            fails++;
        }
    }
    printf("module_validate: §7 gate verdict + official jav_err_t reason  [%s]\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
