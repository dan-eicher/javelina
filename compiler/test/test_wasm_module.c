// test_wasm_module.c — assemble a compiled program into a .wasm module via the
// shared jav_module_write path (wasm_assemble_program). Asserts the assembler
// succeeds (every func body passed the jav_func_body_read spec-grammar gate), the
// module carries the magic/version, and each signature shape encodes the right
// valtypes. Execution is covered by test_exec; this pins the byte-level assembly.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/wasm_module.h"
#include "bbq_arena.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}
static ast_program_t* parse_src(const char* src) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16); pc->result = NULL; pc->file = NULL;
    peg_state p; java_parser_init(&p, src, (int)strlen(src)); p.user_data = pc;
    return java_parser_parse(&p) ? pc->result : NULL;
}
static ast_program_t* build_program(const char* user_src, bbq_arena* arena, int* nlib_out) {
    ast_type_decl_t** t = NULL; int tc = 0, cap = 0;
    #define PUSH(td) do { if(tc==cap){cap=cap?cap*2:64;t=realloc(t,(size_t)cap*sizeof(*t));} t[tc++]=(td);}while(0)
    /* java.lang.System depends on java.io (System.out/err/in), which depends on java.util — load all. */
    const char* dirs[] = { "lib/java/lang", "lib/java/util", "lib/java/io" };
    for (int di = 0; di < 3; di++) {
        DIR* d = opendir(dirs[di]);
        if (d) { struct dirent* e;
            while ((e = readdir(d))) { size_t L=strlen(e->d_name);
                if (L<6 || strcmp(e->d_name+L-5,".java")) continue;
                char path[512]; snprintf(path,sizeof path,"%s/%s",dirs[di],e->d_name);
                char* s = read_file(path); if(!s) continue;
                ast_program_t* p = parse_src(s); if(!p) continue;
                for (int i=0;i<p->types_count;i++) PUSH(p->types[i]);
            } closedir(d);
        }
    }
    if (nlib_out) *nlib_out = tc;          /* library class count (lowest class_ids) */
    ast_program_t* up = parse_src(user_src);
    if (up) for (int i=0;i<up->types_count;i++) PUSH(up->types[i]);
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr,t,(size_t)tc*sizeof(*arr)); free(t);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
    #undef PUSH
}

/* Compile + assemble `src` into `out`; returns the assembler's ok flag. */
static bool assemble(bbq_arena* a, const char* src, emit_wasm_ctx* out) {
    int nlib = 0;
    ast_program_t* prog = build_program(src, a, &nlib);
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = nlib; sema_analyze(sctx, prog);
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    wasm_types_t wt; wasm_types_build(&wt, sctx);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out);
    wasm_types_free(&wt);
    sema_destroy(sctx);              /* 31 htrees/vecs, none of them arena-backed */
    free(sctx); free(cctx);
    return ok;
}

static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}

/* Assemble `src`, assert ok + magic/version, and that `sig` bytes appear (the
 * functype encoding for the method's signature). */
static void check_sig(const char* src, const uint8_t* sig, int siglen, const char* label) {
    bbq_arena a; bbq_arena_init(&a, 1 << 18);
    emit_wasm_ctx mod = {0};
    bool ok = assemble(&a, src, &mod);
    CHECK(ok, label);
    int n = (int)bbq_vec_len(mod.code);
    const uint8_t magic[] = { 0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00 };
    CHECK(ok && n >= 8 && !memcmp(mod.code, magic, 8), "module begins with \\0asm + version 1");
    CHECK(ok && contains(mod.code, n, sig, siglen), label);
    bbq_vec_free(mod.code);
    bbq_arena_free(&a);
}

int main(void) {
    /* (i32,i32)->i32 functype: 0x60 02 7F 7F 01 7F */
    { const uint8_t s[] = { 0x60,0x02,0x7F,0x7F,0x01,0x7F };
      check_sig("class T { static int add(int a, int b){ return a+b; } }", s, 6,
                "i32 functype (i32,i32)->i32"); }
    /* void return → zero results: 0x60 00 00 */
    { const uint8_t s[] = { 0x60,0x00,0x00 };
      check_sig("class T { static void f(){ return; } }", s, 3, "void functype (60 00 00)"); }
    /* instance method → `this` is a concrete (ref null $T): 0x60 01 63 <heaptype>…
     * (the typeidx LEB depends on the topo class order, so pin just the shape —
     * func, 1 param, a nullable concrete ref — proving the eqref placeholder is gone). */
    { const uint8_t s[] = { 0x60,0x01,0x63 };
      check_sig("class T { int f(){ return 0; } }", s, 3, "instance functype prepends concrete `this` ref"); }
    /* long → i64: 0x60 01 7E 01 7E */
    { const uint8_t s[] = { 0x60,0x01,0x7E,0x01,0x7E };
      check_sig("class T { static long f(long a){ return a; } }", s, 5, "long functype i64->i64"); }
    /* double → f64: 0x60 01 7C 01 7C */
    { const uint8_t s[] = { 0x60,0x01,0x7C,0x01,0x7C };
      check_sig("class T { static double f(double a){ return a; } }", s, 5, "double functype f64->f64"); }
    /* a body local → the code section carries an i32 locals run (01 01 7F). */
    { bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
      bool ok = assemble(&a, "class T { static int f(int x){ int y = x + 1; return y; } }", &mod);
      CHECK(ok, "locals: assembled f");
      const uint8_t run[] = { 0x01, 0x01, 0x7F };
      CHECK(ok && contains(mod.code, (int)bbq_vec_len(mod.code), run, 3),
            "locals: code section carries the i32 locals run");
      bbq_vec_free(mod.code); bbq_arena_free(&a); }

    return TEST_SUMMARY("test_wasm_module");
}
