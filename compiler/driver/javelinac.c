/* javelinac — the javelina Java→WASM compiler CLI.
 *
 * A thin driver over the compiler library: parse the JLS §20-22 prelude (the
 * java.lang/util/io interface the compiler owns) plus the user sources, type-check
 * (sema), compile to SIR, and assemble a `.wasm` module. It is the first user of
 * `compiler_compile` outside the test harness — the same pipeline test_exec drives,
 * exposed as a command.
 *
 *   javelinac [options] File.java [More.java ...]
 *   javelinac --mode jre -o jre.wasm          (compile the prelude to the runtime)
 *
 * Modes (sema_mode_t): plugin (default) imports java.lang from a jre.wasm; whole is
 * self-contained (prelude compiled in); jre is the runtime module itself (prelude
 * only, every library function/global exported). Fail-closed: sema errors abort with
 * a non-zero exit and no output — a real toolchain must not emit code for a program
 * that failed type-checking. */
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/wasm_module.h"
#include "jav_load.h"    /* jav_validate_bytes — the fail-closed §7 gate before writing */
#include "jav_error.h"   /* jav_err_str */
#include "bbq_arena.h"
#include "bbq_vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define JAVELINA_VERSION "0.1.0"

static const char* prog_name = "javelinac";

static int usage(FILE* f, int code) {
    fprintf(f,
        "usage: %s [options] File.java [File.java | dir ...]\n"
        "\n"
        "Compile Java 1.0 (minus synchronized) source to a WebAssembly module. Inputs are\n"
        ".java files and/or directories (walked recursively for *.java).\n"
        "\n"
        "options:\n"
        "  -o FILE          write the module to FILE (default: <first-input>.wasm,\n"
        "                   or jre.wasm in --mode jre; '-' writes to stdout)\n"
        "  --mode MODE      plugin (default): imports java.lang from a jre.wasm\n"
        "                   whole: self-contained (prelude compiled in)\n"
        "                   jre:   compile the prelude itself into the runtime module\n"
        "  -O, --optimize   run the Click optimizer (SCCP+GVN+DCE, slot repack)\n"
        "                   on every method before codegen — THE DEFAULT\n"
        "  -O0, --no-optimize  disable the optimizer (debugging / bisection)\n"
        "  --libdir DIR     the java.lang/util/io prelude root (default: lib/java)\n"
        "  --version        print the version and exit\n"
        "  -h, --help       print this help and exit\n"
        "\n"
        "debug environment (introspection to stderr; no effect on the module):\n"
        "  JAVELINA_SCALAR_CENSUS=1   scalar-replacement / partial-escape probes\n"
        "  JAVELINA_GUARD_CENSUS=1    implicit-exception guard-elimination census\n"
        "  JAVELINA_CLICK_ONLY=C.m    optimize ONLY the named method (bisection)\n"
        "  JAVELINA_DUMP_SPINE=NAME   pre/mid/post spine dump for matching methods\n"
        "  JAVELINA_DUMP_PHIS=NAME    solved-phi dump for matching methods\n",
        prog_name);
    return code;
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "%s: cannot open '%s'\n", prog_name, path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}

/* Parse `src` (attributed to `file` for diagnostics) into a fresh context whose
 * arena owns the AST; the context is recorded in `*ctxs` for the caller to free
 * after compilation. Idents/literals are duplicated into the arena, so `src` need
 * not survive. */
static ast_program_t* parse_src(const char* src, const char* file, java_parse_ctx_t*** ctxs) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16); pc->result = NULL; pc->file = file;
    peg_state p; java_parser_init(&p, src, (int)strlen(src)); p.user_data = pc;
    ast_program_t* prog = java_parser_parse(&p) ? pc->result : NULL;
    if (!prog) fprintf(stderr, "%s: parse error in '%s'\n", prog_name, file ? file : "<stdin>");
    bbq_vec_push(*ctxs, pc);
    return prog;
}

/* Glob one prelude package dir, parsing every *.java into `types`. The per-file
 * `package` decl (not the directory) drives package resolution in sema. */
static void glob_dir(const char* dir, ast_type_decl_t*** types, java_parse_ctx_t*** ctxs) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        size_t L = strlen(e->d_name);
        if (L < 6 || strcmp(e->d_name + L - 5, ".java")) continue;
        char* path = (char*)malloc(strlen(dir) + L + 2);
        sprintf(path, "%s/%s", dir, e->d_name);
        char* s = read_file(path);
        if (s) {
            ast_program_t* p = parse_src(s, path, ctxs);   /* path outlives via the ctx list? no — dup */
            free(s);
            if (p) for (int i = 0; i < p->types_count; i++) bbq_vec_push(*types, p->types[i]);
        }
        /* the ctx keeps a pointer to `path` as its diag file — keep it alive by leaking
         * intentionally (process-lifetime; freed at exit). Small and bounded. */
    }
    closedir(d);
}

static int is_java_name(const char* n) { size_t L = strlen(n); return L >= 6 && !strcmp(n + L - 5, ".java"); }
static char* str_dup(const char* s) { size_t n = strlen(s) + 1; char* p = (char*)malloc(n); if (p) memcpy(p, s, n); return p; }

/* Parse one .java file into `types`. The path is duplicated (process-lifetime) because the
 * parse context keeps it for diagnostics; a caller's transient buffer must not back it. */
static bool add_java_file(const char* path, ast_type_decl_t*** types, java_parse_ctx_t*** ctxs) {
    char* s = read_file(path);
    if (!s) return false;
    ast_program_t* p = parse_src(s, str_dup(path), ctxs);
    free(s);
    if (!p) return false;
    for (int j = 0; j < p->types_count; j++) bbq_vec_push(*types, p->types[j]);
    return true;
}

/* Add a compilation input: a .java file, or a directory (walked recursively for *.java,
 * so a package-nested source tree can be passed as one argument). */
static bool add_input(const char* path, ast_type_decl_t*** types, java_parse_ctx_t*** ctxs) {
    struct stat st;
    if (stat(path, &st) != 0) { fprintf(stderr, "%s: cannot stat '%s'\n", prog_name, path); return false; }
    if (!S_ISDIR(st.st_mode)) return add_java_file(path, types, ctxs);
    DIR* d = opendir(path);
    if (!d) { fprintf(stderr, "%s: cannot open directory '%s'\n", prog_name, path); return false; }
    bool ok = true;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;                     /* skip ., .., hidden */
        char* child = (char*)malloc(strlen(path) + strlen(e->d_name) + 2);
        sprintf(child, "%s/%s", path, e->d_name);
        struct stat cst;
        if (stat(child, &cst) == 0 && (S_ISDIR(cst.st_mode) || is_java_name(e->d_name)))
            ok = add_input(child, types, ctxs) && ok;         /* recurse dirs; parse *.java */
        free(child);
    }
    closedir(d);
    return ok;
}

int main(int argc, char** argv) {
    const char* out_path  = NULL;
    bool        optimize  = true;    /* the DEFAULT: users get optimized code;
                                      * -O0 is the explicit debugging escape */
    const char* libdir    = "lib/java";
    sema_mode_t mode      = SEMA_MODE_PLUGIN;
    const char** inputs   = (const char**)calloc((size_t)argc, sizeof(char*));
    int ninputs = 0;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if      (!strcmp(a, "-h") || !strcmp(a, "--help")) { free(inputs); return usage(stdout, 0); }
        else if (!strcmp(a, "--version")) { printf("javelinac %s\n", JAVELINA_VERSION); free(inputs); return 0; }
        else if (!strcmp(a, "-o")) { if (++i >= argc) { free(inputs); return usage(stderr, 2); } out_path = argv[i]; }
        else if (!strcmp(a, "--libdir")) { if (++i >= argc) { free(inputs); return usage(stderr, 2); } libdir = argv[i]; }
        else if (!strcmp(a, "--mode")) {
            if (++i >= argc) { free(inputs); return usage(stderr, 2); }
            if      (!strcmp(argv[i], "plugin"))  mode = SEMA_MODE_PLUGIN;
            else if (!strcmp(argv[i], "whole"))   mode = SEMA_MODE_WHOLE;
            else if (!strcmp(argv[i], "jre"))     mode = SEMA_MODE_RUNTIME;
            else { fprintf(stderr, "%s: unknown mode '%s'\n", prog_name, argv[i]); free(inputs); return 2; }
        }
        else if (!strcmp(a, "-O") || !strcmp(a, "--optimize")) { optimize = true; }
        else if (!strcmp(a, "-O0") || !strcmp(a, "--no-optimize")) { optimize = false; }
        else if (a[0] == '-' && a[1] && strcmp(a, "-")) {
            fprintf(stderr, "%s: unknown option '%s'\n", prog_name, a); free(inputs); return usage(stderr, 2);
        }
        else inputs[ninputs++] = a;
    }

    if (mode != SEMA_MODE_RUNTIME && ninputs == 0) {
        fprintf(stderr, "%s: no input files\n", prog_name); free(inputs); return usage(stderr, 2);
    }
    if (mode == SEMA_MODE_RUNTIME && ninputs > 0) {
        fprintf(stderr, "%s: --mode jre takes no input files (it compiles the prelude)\n", prog_name);
        free(inputs); return 2;
    }

    /* ── Parse: prelude (the java.lang interface) first, then user sources. Library
     * classes get the lowest class ids; num_library_classes tells sema which are
     * host/library. ── */
    bbq_arena arena; bbq_arena_init(&arena, 1 << 20);
    java_parse_ctx_t** ctxs = NULL;
    ast_type_decl_t**  types = NULL;

    char pkgdir[512];
    snprintf(pkgdir, sizeof pkgdir, "%s/lang", libdir); glob_dir(pkgdir, &types, &ctxs);
    snprintf(pkgdir, sizeof pkgdir, "%s/util", libdir); glob_dir(pkgdir, &types, &ctxs);
    snprintf(pkgdir, sizeof pkgdir, "%s/io",   libdir); glob_dir(pkgdir, &types, &ctxs);
    /* javelina.simd — the v128 value type + lane intrinsic classes. A sibling
     * package root of java/, so it lives beside (not under) the java tree;
     * glob_dir skips it silently when a custom --libdir has no simd library. */
    snprintf(pkgdir, sizeof pkgdir, "%s/../javelina/simd", libdir); glob_dir(pkgdir, &types, &ctxs);
    int nlib = (int)bbq_vec_len(types);
    if (nlib == 0) {
        fprintf(stderr, "%s: no prelude classes found under '%s' (wrong --libdir?)\n", prog_name, libdir);
        return 2;
    }

    bool parse_ok = true;
    for (int i = 0; i < ninputs; i++)
        if (!add_input(inputs[i], &types, &ctxs)) parse_ok = false;   /* file or directory tree */
    if (!parse_ok) { bbq_vec_free(types); return 2; }

    int tc = (int)bbq_vec_len(types);
    ast_type_decl_t** arr = bbq_arena_alloc(&arena, (size_t)tc * sizeof(*arr));
    memcpy(arr, types, (size_t)tc * sizeof(*arr));
    bbq_vec_free(types);
    /* The flat program feeds the DDCG (it only walks bodies; every name is
     * resolved by then). Sema gets the PER-UNIT programs (§7.3): each file's
     * package declaration and import list drive §6.5.4 name resolution. */
    ast_program_t* prog = ast_program(&arena, NULL, NULL, 0, arr, tc);
    ast_program_t** units = NULL;
    for (int i = 0; i < (int)bbq_vec_len(ctxs); i++)
        if (ctxs[i]->result) bbq_vec_push(units, ctxs[i]->result);

    /* ── Sema (mode-aware). Fail-closed on any error. ── */
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, &arena);
    sctx->num_library_classes = nlib;
    sctx->mode = mode;
    sema_analyze_units(sctx, units, (int)bbq_vec_len(units));
    bbq_vec_free(units);

    int nd = 0; const sema_diag_t* diags = sema_diags(sctx, &nd);
    for (int i = 0; i < nd; i++) {
        const sema_diag_t* d = &diags[i];
        fprintf(stderr, "%s:%d:%d: %s: %s\n",
                d->loc.file ? d->loc.file : "<input>", d->loc.line, d->loc.col,
                d->level == DIAG_ERROR ? "error" : "warning", d->message);
    }
    if (sema_error_count(sctx) > 0) {
        fprintf(stderr, "%s: compilation failed (%d error%s)\n",
                prog_name, sema_error_count(sctx), sema_error_count(sctx) == 1 ? "" : "s");
        sema_destroy(sctx); free(sctx); return 1;
    }

    /* ── Compile + assemble. ── */
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, &arena, sctx);
    cctx->optimize = optimize;
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);

    wasm_types_t wt; wasm_types_build(&wt, sctx);
    emit_wasm_ctx out = {0};
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, &out);
    wasm_types_free(&wt);
    compiler_destroy(cctx); free(cctx);

    if (!ok) {
        fprintf(stderr, "%s: assembly failed (backend produced an invalid module)\n", prog_name);
        sema_destroy(sctx); free(sctx); return 1;
    }

    /* ── Fail-closed §7 gate: run the VM's OWN validator over the bytes
     * BEFORE writing. The per-body grammar gate above catches shape bugs;
     * this catches everything else (it once let an "unknown local" ship
     * with exit 0 — the Click pool-count miscompile). An artifact this
     * compiler writes is an artifact the VM will load. ── */
    size_t len = bbq_vec_len(out.code);
    {
        jav_err_t verr = JAV_E_NONE;
        if (jav_validate_bytes(out.code, len, &verr) != JAV_OK) {
            fprintf(stderr, "%s: emitted module FAILS validation: %s — refusing to write\n",
                    prog_name, jav_err_str(verr));
            sema_destroy(sctx); free(sctx); return 1;
        }
    }
    char derived[512] = {0};
    if (!out_path) {
        if (mode == SEMA_MODE_RUNTIME) out_path = "jre.wasm";
        else {
            /* <first-input> with .java → .wasm, basename only */
            const char* in = inputs[0];
            const char* slash = strrchr(in, '/'); const char* base = slash ? slash + 1 : in;
            size_t bl = strlen(base);
            const char* dot = strrchr(base, '.');
            size_t stem = (dot && !strcmp(dot, ".java")) ? (size_t)(dot - base) : bl;
            snprintf(derived, sizeof derived, "%.*s.wasm", (int)stem, base);
            out_path = derived;
        }
    }

    FILE* of;
    if (!strcmp(out_path, "-")) {
        if (isatty(1)) { fprintf(stderr, "%s: refusing to write a module to a terminal\n", prog_name);
                         sema_destroy(sctx); free(sctx); return 2; }
        of = stdout;
    } else {
        of = fopen(out_path, "wb");
        if (!of) { fprintf(stderr, "%s: cannot write '%s'\n", prog_name, out_path);
                   sema_destroy(sctx); free(sctx); return 2; }
    }
    if (len && fwrite(out.code, 1, len, of) != len) {
        fprintf(stderr, "%s: short write to '%s'\n", prog_name, out_path);
        if (of != stdout) fclose(of);
        sema_destroy(sctx); free(sctx); return 2;
    }
    if (of != stdout) fclose(of);

    sema_destroy(sctx); free(sctx);
    return 0;
}
