// test_wat_cli.c — the water BINARY's two contracts, held from outside.
//
// InvalidWritesNothing — pointing -d at a §7-invalid module leaves NO output
// file, exits non-zero, and names the instruction. A partial or
// "structurally fine" file appearing is the failure this pin exists for.
//
// NoVerifyStillEmitsBadFixtures — assembling a structurally-fine, §7-invalid
// .wat REFUSES by default (the person at the keyboard hears it here), and
// with --no-verify writes byte-for-byte what the transcribe-only library
// path produces — the conformance pipeline's contract, held against the
// library itself so no drift between the two can hide.
#include "jav_writer.h"
#include "wat_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void CK(const char *msg, long got, long want) {
    int ok = (got == want);
    printf("  %-58s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

static int write_file(const char *path, const void *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t put = fwrite(data, 1, n, f);
    fclose(f);
    return put == n;
}

static long read_file(const char *path, unsigned char *buf, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return (long)n;
}

/* A structurally-fine module whose one body returns i64 from an i32 result. */
static const unsigned char BAD_WASM[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x42, 0x01, 0x0b,
};
static const char *BAD_WAT =
    "(module (func (export \"bad\") (result i32) i64.const 1))";

static void invalid_writes_nothing(void) {
    printf("InvalidWritesNothing: -d refuses whole, names the instruction\n");
    CK("fixture written",
       write_file("../build/.cli_bad.wasm", BAD_WASM, sizeof BAD_WASM), 1);
    remove("../build/.cli_out.wat");
    int rc = system("../build/water -d ../build/.cli_bad.wasm -o ../build/.cli_out.wat"
                    " 2> ../build/.cli_err.txt");
    CK("exit is non-zero", rc != 0, 1);
    FILE *f = fopen("../build/.cli_out.wat", "rb");
    CK("no output file appears", f == NULL, 1);
    if (f) fclose(f);
    unsigned char err[512];
    long n = read_file("../build/.cli_err.txt", err, sizeof err - 1);
    err[n > 0 ? n : 0] = 0;
    CK("the diagnostic names func and instruction",
       strstr((char *)err, "func 0") != NULL &&
       strstr((char *)err, "instruction") != NULL, 1);
}

static void no_verify_still_emits_bad_fixtures(void) {
    printf("NoVerifyStillEmitsBadFixtures: the pipeline's escape hatch\n");
    CK("fixture written",
       write_file("../build/.cli_bad.wat", BAD_WAT, strlen(BAD_WAT)), 1);
    remove("../build/.cli_out.wasm");
    int rc = system("../build/water ../build/.cli_bad.wat -o ../build/.cli_out.wasm"
                    " 2> /dev/null");
    CK("default verifies: exit non-zero", rc != 0, 1);
    FILE *f = fopen("../build/.cli_out.wasm", "rb");
    CK("default verifies: no output file", f == NULL, 1);
    if (f) fclose(f);

    rc = system("../build/water --no-verify ../build/.cli_bad.wat"
                " -o ../build/.cli_out.wasm 2> /dev/null");
    CK("--no-verify: exit zero", rc, 0);

    /* The reference bytes: the transcribe-only LIBRARY path. */
    int line = 0, col = 0;
    jav_module_t *m = wat_assemble(BAD_WAT, (int)strlen(BAD_WAT), &line, &col);
    CK("library assembles the fixture", m != NULL, 1);
    if (!m) return;
    bbq_write_ctx_t w;
    bbq_write_ctx_init_growable(&w, 256);
    bbq_write_set_endian(&w, true);
    CK("library serializes it", jav_module_write(&w, m), 1);
    jav_module_free(m);
    free(m);
    unsigned char got[512];
    long n = read_file("../build/.cli_out.wasm", got, sizeof got);
    CK("--no-verify bytes == the library's, exactly",
       n == (long)w.pos && memcmp(got, w.data, w.pos) == 0, 1);
    bbq_write_ctx_free(&w);
}

int main(void) {
    invalid_writes_nothing();
    no_verify_still_emits_bad_fixtures();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
