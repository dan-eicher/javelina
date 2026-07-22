/*
 * water — the WebAssembly text→binary assembler.
 *
 * Reads a `.wat` module, parses + resolves it with the real §6 grammar, and
 * writes the `.wasm` binary the §5 grammar serializes from the shared struct —
 * one encoder, the same module the binary reader would produce. It is
 * TRANSCRIBE-ONLY: it rejects what the parser + writer catch mechanically
 * (unparseable text, unencodable structure) but performs NO type validation —
 * by design, so the test pipeline can feed it modules that are well-formed text
 * yet invalid types, and watch the VM reject them downstream.
 *
 * Usage: water [options] [input.wat]
 *   input.wat        source file; omitted or "-" reads stdin
 *   -o, --output F   write binary to F; omitted or "-" writes stdout
 *   -h, --help       this message
 *
 * Pipe-friendly: `water foo.wat -o foo.wasm`, `cat foo.wat | water > foo.wasm`.
 */

#include "wat_driver.h"
#include "jav_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *PROG = "water";

static void usage(FILE *f) {
    fprintf(f,
        "usage: %s [options] [input.wat]\n"
        "  input.wat         .wat source (omit or \"-\" for stdin)\n"
        "  -o, --output F    write .wasm to F (omit or \"-\" for stdout)\n"
        "  -h, --help        show this help\n",
        PROG);
}

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* Slurp a whole stream into a malloc'd buffer (NUL-terminated). Returns the byte
 * count via *out_len, or -1 on a read error. */
static char *slurp(FILE *f, long *out_len) {
    size_t cap = 1 << 16, n = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (n + (1 << 16) > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        size_t got = fread(buf + n, 1, 1 << 16, f);
        n += got;
        if (got < (1 << 16)) {
            if (ferror(f)) { free(buf); return NULL; }
            break;
        }
    }
    buf[n] = '\0';
    *out_len = (long)n;
    return buf;
}

int main(int argc, char **argv) {
    const char *in_path = NULL, *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (streq(a, "-h") || streq(a, "--help")) { usage(stdout); return 0; }
        else if (streq(a, "-o") || streq(a, "--output")) {
            if (++i >= argc) { fprintf(stderr, "%s: %s needs an argument\n", PROG, a); return 2; }
            out_path = argv[i];
        } else if (a[0] == '-' && a[1] && !streq(a, "-")) {
            fprintf(stderr, "%s: unknown option %s\n", PROG, a);
            usage(stderr);
            return 2;
        } else {
            if (in_path) { fprintf(stderr, "%s: multiple input files\n", PROG); return 2; }
            in_path = a;
        }
    }

    /* ── read the .wat source (file or stdin) ── */
    FILE *in = (!in_path || streq(in_path, "-")) ? stdin : fopen(in_path, "rb");
    if (!in) { fprintf(stderr, "%s: cannot open %s\n", PROG, in_path); return 1; }
    long len = 0;
    char *src = slurp(in, &len);
    if (in != stdin) fclose(in);
    if (!src) { fprintf(stderr, "%s: read error\n", PROG); return 1; }

    /* ── parse + resolve → jav_module_t ── */
    int err_line = 0, err_col = 0;
    jav_module_t *mod = wat_assemble(src, (int)len, &err_line, &err_col);
    free(src);
    if (!mod) {
        fprintf(stderr, "%s: %s:%d:%d: parse error\n", PROG,
                in_path ? in_path : "<stdin>", err_line, err_col);
        return 1;
    }

    /* ── serialize → .wasm bytes (growable buffer; the writer computes @rest sizes) ── */
    bbq_write_ctx_t w;
    bbq_write_ctx_init_growable(&w, (size_t)len + 64);
    bbq_write_set_endian(&w, true);
    int wrote = jav_module_write(&w, mod);
    jav_module_free(mod);
    free(mod);
    if (!wrote) {
        fprintf(stderr, "%s: serialization failed\n", PROG);
        bbq_write_ctx_free(&w);
        return 1;
    }

    /* ── emit (file or stdout; refuse to splatter binary onto a terminal) ── */
    FILE *out;
    int to_stdout = !out_path || streq(out_path, "-");
    if (to_stdout) {
        if (isatty(fileno(stdout))) {
            fprintf(stderr, "%s: refusing to write binary to a terminal (use -o FILE or redirect)\n", PROG);
            bbq_write_ctx_free(&w);
            return 1;
        }
        out = stdout;
    } else {
        out = fopen(out_path, "wb");
        if (!out) { fprintf(stderr, "%s: cannot write %s\n", PROG, out_path); bbq_write_ctx_free(&w); return 1; }
    }
    size_t put = fwrite(w.data, 1, w.pos, out);
    if (out != stdout) fclose(out);
    bbq_write_ctx_free(&w);
    if (put != w.pos) { fprintf(stderr, "%s: write error\n", PROG); return 1; }
    return 0;
}
