/*
 * water — the WebAssembly text ⇄ binary converter.
 *
 * The two directions carry OPPOSITE contracts, on purpose, and each exists
 * because of the other's:
 *
 *   text → binary (the default) ASSEMBLES with the real §6 grammar and, by
 *   default, VERIFIES the result (§7.6 bodies + §3.5 module rules) before
 *   writing — a person assembling a module wants to hear it is invalid here,
 *   not from whatever engine loads it next. `--no-verify` restores the pure
 *   transcription: the parser and writer still reject what they catch
 *   mechanically, but no type rule runs — which is how the conformance
 *   pipeline produces its deliberately-invalid fixtures and watches the
 *   engine reject them for the right reason. The LIBRARY entry
 *   (`wat_assemble`) is transcribe-only always; the default verification is
 *   this binary's, so no fixture path can break by forgetting a flag.
 *
 *   binary → text (`-d`) VALIDATES FIRST and only ever writes valid text. A
 *   folded rendering is a claim about the data flow, and on a body whose
 *   operand stack is wrong that claim would be a lie — so a rejected module
 *   produces a diagnostic (which function, which instruction, what the walk
 *   saw) and no output file at all. The § contracts of the rendering itself
 *   — folding, width, @custom, identifiers — live in docs/water.md and the
 *   gates that hold them.
 *
 * Usage: water [options] [input]
 *   input             .wat source, or with -d a .wasm module ("-" = stdin)
 *   -d                disassemble: .wasm in, folded .wat out
 *   -o, --output F    output file (omit or "-": stdout)
 *   --width N         -d line budget (default 100)
 *   --no-verify       assemble without §7 verification (fixture pipeline)
 *   -h, --help        this message
 */

#include "jav_writer.h"
#include "wat_check.h"
#include "wat_driver.h"
#include "wat_emit.h"
#include "wat_mnemonics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *PROG = "water";

static void usage(FILE *f) {
    fprintf(f,
        "usage: %s [options] [input]\n"
        "  input             .wat source, or with -d a .wasm module (\"-\" = stdin)\n"
        "  -d                disassemble: .wasm in, folded .wat out\n"
        "  -o, --output F    output file (omit or \"-\" for stdout)\n"
        "  --width N         -d line budget (default 100)\n"
        "  --no-verify       assemble without §7 verification\n"
        "  -h, --help        show this help\n",
        PROG);
}

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* Slurp a whole stream into a malloc'd buffer (NUL-terminated). */
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

static const char *mnemonic_of(const jav_instr_t *in) {
    uint32_t prefix = 0, op = in->op;
    if (in->op == 0xfb) { prefix = 0xfb; op = in->body.u.case_29.sub; }
    else if (in->op == 0xfc) { prefix = 0xfc; op = in->body.u.case_30.sub; }
    else if (in->op == 0xfd) { prefix = 0xfd; op = in->body.u.case_31.sub; }
    for (size_t i = 0; i < sizeof wat_mnemonics / sizeof wat_mnemonics[0]; i++)
        if (wat_mnemonics[i].prefix == prefix && wat_mnemonics[i].op == op)
            return wat_mnemonics[i].name;
    return "?";
}

/* §7 over `mod`, reporting the first rejection in §6 coordinates: the
 * declaration, the instruction's flat ordinal within its body, the mnemonic,
 * the reason, and the operand stack as the walk saw it. Returns 1 iff valid.
 * The context is handed back for the -d path to render with. */
static int verify(const jav_module_t *mod, bbq_arena *a, wat_check_ctx_t **cx_out) {
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *cx = wat_check_ctx_build(mod, a, &err);
    if (!cx) {
        fprintf(stderr, "%s: invalid module: %s\n", PROG, jav_err_str(err));
        return 0;
    }
    if (!wat_check_module(cx, a, &err)) {
        fprintf(stderr, "%s: invalid module: %s\n", PROG, jav_err_str(err));
        return 0;
    }
    uint32_t nimp = 0;
    for (size_t i = 0; i < mod->sections.count; i++)
        if (mod->sections.items[i].id == 2) {
            const jav_import_section_t *is = &mod->sections.items[i].body.u.case_2;
            for (size_t k = 0; k < is->imports.count; k++)
                if (is->imports.items[k].desc.kind == 0x00) nimp++;
        }
    for (size_t i = 0; i < mod->sections.count; i++) {
        if (mod->sections.items[i].id != 10) continue;
        const jav_code_section_t *cs = &mod->sections.items[i].body.u.case_10;
        for (size_t k = 0; k < cs->entries.count; k++) {
            wat_body_t b;
            if (!wat_check_body(cx, nimp + (uint32_t)k, &cs->entries.items[k].body, a, &b)) {
                fprintf(stderr, "%s: invalid module: func %u, instruction %u (%s): %s\n"
                                "%s:   operand stack: %s\n",
                        PROG, nimp + (uint32_t)k, b.fail_seq,
                        b.fail ? mnemonic_of(b.fail) : "at end of body",
                        jav_err_str(b.err), PROG, b.fail_stack);
                return 0;
            }
        }
    }
    if (cx_out) *cx_out = cx;
    return 1;
}

/* Open the output last, so a rejection can never leave a partial file. */
static FILE *open_out(const char *out_path, int binary) {
    if (!out_path || streq(out_path, "-")) {
        if (binary && isatty(fileno(stdout))) {
            fprintf(stderr, "%s: refusing to write binary to a terminal (use -o FILE or redirect)\n", PROG);
            return NULL;
        }
        return stdout;
    }
    FILE *out = fopen(out_path, "wb");
    if (!out) fprintf(stderr, "%s: cannot write %s\n", PROG, out_path);
    return out;
}

int main(int argc, char **argv) {
    const char *in_path = NULL, *out_path = NULL;
    int disassemble = 0, no_verify = 0, width = 100;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (streq(a, "-h") || streq(a, "--help")) { usage(stdout); return 0; }
        else if (streq(a, "-d")) disassemble = 1;
        else if (streq(a, "--no-verify")) no_verify = 1;
        else if (streq(a, "--width")) {
            if (++i >= argc) { fprintf(stderr, "%s: --width needs an argument\n", PROG); return 2; }
            width = atoi(argv[i]);
            if (width < 20) { fprintf(stderr, "%s: --width must be at least 20\n", PROG); return 2; }
        } else if (streq(a, "-o") || streq(a, "--output")) {
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
    if (disassemble && no_verify) {
        fprintf(stderr, "%s: --no-verify applies to assembling; -d only ever writes valid text\n", PROG);
        return 2;
    }

    FILE *in = (!in_path || streq(in_path, "-")) ? stdin : fopen(in_path, "rb");
    if (!in) { fprintf(stderr, "%s: cannot open %s\n", PROG, in_path); return 1; }
    long len = 0;
    char *src = slurp(in, &len);
    if (in != stdin) fclose(in);
    if (!src) { fprintf(stderr, "%s: read error\n", PROG); return 1; }

    if (disassemble) {
        /* ── .wasm → .wat: validate, then render; no output on rejection ── */
        bbq_ctx_t c;
        bbq_ctx_init(&c, (const uint8_t *)src, (size_t)len);
        jav_module_t mod;
        memset(&mod, 0, sizeof mod);
        if (!jav_module_read(&c, &mod) || !bbq_at_end(&c)) {
            fprintf(stderr, "%s: %s: malformed module\n", PROG, in_path ? in_path : "<stdin>");
            bbq_ctx_free(&c);
            free(src);
            return 1;
        }
        bbq_ctx_free(&c);
        bbq_arena a;
        bbq_arena_init(&a, 1 << 20);
        wat_check_ctx_t *cx = NULL;
        if (!verify(&mod, &a, &cx)) {
            bbq_arena_free(&a);
            jav_module_free(&mod);
            free(src);
            return 1;
        }
        const char *txt = NULL;
        size_t tlen = 0;
        if (!wat_emit_module(&mod, cx, width, &a, &txt, &tlen)) {
            fprintf(stderr, "%s: rendering failed\n", PROG);
            bbq_arena_free(&a);
            jav_module_free(&mod);
            free(src);
            return 1;
        }
        FILE *out = open_out(out_path, 0);
        if (!out) { bbq_arena_free(&a); jav_module_free(&mod); free(src); return 1; }
        size_t put = fwrite(txt, 1, tlen, out);
        if (out != stdout) fclose(out);
        int bad = (put != tlen);
        bbq_arena_free(&a);
        jav_module_free(&mod);
        free(src);
        if (bad) { fprintf(stderr, "%s: write error\n", PROG); return 1; }
        return 0;
    }

    /* ── .wat → .wasm: assemble, verify unless told not to, then write ── */
    int err_line = 0, err_col = 0;
    jav_module_t *mod = wat_assemble(src, (int)len, &err_line, &err_col);
    free(src);
    if (!mod) {
        fprintf(stderr, "%s: %s:%d:%d: parse error\n", PROG,
                in_path ? in_path : "<stdin>", err_line, err_col);
        return 1;
    }
    if (!no_verify) {
        bbq_arena a;
        bbq_arena_init(&a, 1 << 20);
        int ok = verify(mod, &a, NULL);
        bbq_arena_free(&a);
        if (!ok) {
            jav_module_free(mod);
            free(mod);
            return 1;
        }
    }
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
    FILE *out = open_out(out_path, 1);
    if (!out) { bbq_write_ctx_free(&w); return 1; }
    size_t put = fwrite(w.data, 1, w.pos, out);
    if (out != stdout) fclose(out);
    bbq_write_ctx_free(&w);
    if (put != w.pos) { fprintf(stderr, "%s: write error\n", PROG); return 1; }
    return 0;
}
