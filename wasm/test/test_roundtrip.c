// test_roundtrip.c — P0 integration gate. Reads a .wasm through the bbqc-generated
// reader, serializes it back through the bbqc-generated writer, and asserts the
// output is byte-identical to the input. This is the inverse property (read∘write
// == identity) end-to-end, and it is exactly what the LEB-writer fix makes pass —
// before the fix the writer emitted fixed-width ints for every uleb128 field, so
// the bytes diverged immediately at the first section size/count.

#include "jav_reader.h"
#include "jav_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "add.wasm";
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc((size_t)n);
    if (fread(in, 1, (size_t)n, f) != (size_t)n) { perror("fread"); return 2; }
    fclose(f);

    bbq_ctx_t cx; bbq_ctx_init(&cx, in, (size_t)n);
    jav_module_t mod;
    if (!jav_module_read(&cx, &mod)) { fprintf(stderr, "parse failed\n"); bbq_ctx_free(&cx); return 1; }
    if (!bbq_at_end(&cx)) { fprintf(stderr, "did not consume whole file\n"); bbq_ctx_free(&cx); return 1; }
    bbq_ctx_free(&cx);

    // Canonical re-encode is the same length for canonical input; +64 slack guards
    // a buffer overrun being misreported as a content diff.
    // Writer is growable: @rest section/code sizes are COMPUTED by the writer, so
    // the output buffer must grow on demand; output is {w.data, w.pos}.
    size_t cap = (size_t)n + 64;
    bbq_write_ctx_t w; bbq_write_ctx_init_growable(&w, cap);
    if (!jav_module_write(&w, &mod)) { fprintf(stderr, "write failed\n"); bbq_write_ctx_free(&w); return 1; }
    const uint8_t *out = w.data;

    int ok = (w.pos == (size_t)n) && memcmp(in, out, (size_t)n) == 0;
    if (!ok) {
        fprintf(stderr, "round-trip DIVERGED: in=%ld bytes, out=%zu bytes\n", n, w.pos);
        size_t lim = w.pos < (size_t)n ? w.pos : (size_t)n;
        for (size_t i = 0; i < lim; i++)
            if (in[i] != out[i]) { fprintf(stderr, "  first diff at byte %zu: in=0x%02X out=0x%02X\n",
                                            i, in[i], out[i]); break; }
    }
    printf("round-trip %s: %s\n", path, ok ? "PASS (byte-identical)" : "FAIL");

    jav_module_free(&mod);
    free(in); bbq_write_ctx_free(&w);
    return ok ? 0 : 1;
}
