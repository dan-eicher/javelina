// test_click_backend.c — #31. Click (the SIR optimizer) run THROUGH the backend.
//
// SCOPE, stated honestly: this is an INTEGRATION SMOKE test, not a validity
// test. It checks (a) one real PROPERTY — Click actually rewrote a value (the
// foldable 2+3 is gone, a const 5 is there), proving the optimizer ran in the
// codegen path; and (b) that running Click before the burg over EVERY family
// (if/while/switch/try/field/call/array) neither crashes nor drops the family's
// control-flow framing / key op — i.e. its in-place value rewrite + slot repack
// don't dangle the scope sidecar's node pointers.
//
// What it deliberately does NOT do: assert the bytes are a VALID module, or pin
// Click's exact output. Pinning current output is a golden-master change-detector
// that would enshrine any optimizer bug as "correct". The optimizer's CORRECTNESS
// is tested the yoctojc way — by value-graph PROPERTIES (congruence, CSE, const/
// branch fold, packing) in test_click_partition.c (93 cases). Byte VALIDITY of
// the optimized lowering is the VM's §7.6 validator at conformance (#34).
//
// The SIR is ANF, so value deps are already sir_child and the spine is sir_succ:
// no CFG, no dominator tree, no re-derived dependency graph is built here.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/codegen_method.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/sir_optimizer.h"
#include "bbq_arena.h"
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
static ast_program_t* build_program(const char* user_src, bbq_arena* arena) {
    ast_type_decl_t** t = NULL; int tc = 0, cap = 0;
    #define PUSH(td) do { if(tc==cap){cap=cap?cap*2:64;t=realloc(t,(size_t)cap*sizeof(*t));} t[tc++]=(td);}while(0)
    DIR* d = opendir("lib/java/lang");
    if (d) { struct dirent* e;
        while ((e = readdir(d))) { size_t L=strlen(e->d_name);
            if (L<6 || strcmp(e->d_name+L-5,".java")) continue;
            char path[512]; snprintf(path,sizeof path,"lib/java/lang/%s",e->d_name);
            char* s = read_file(path); if(!s) continue;
            ast_program_t* p = parse_src(s); if(!p) continue;
            for (int i=0;i<p->types_count;i++) PUSH(p->types[i]);
        } closedir(d);
    }
    ast_program_t* up = parse_src(user_src);
    if (up) for (int i=0;i<up->types_count;i++) PUSH(up->types[i]);
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr,t,(size_t)tc*sizeof(*arr)); free(t);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
    #undef PUSH
}
static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}

/* Compile `name`, optionally run Click, structured-emit, return the body. The
 * scopes come from the fact table (recorded during ddcg, BEFORE Click). */
static const uint8_t* emit(bbq_arena* a, const char* src, const char* name,
                           bool click, int* out_len) {
    ast_program_t* prog = build_program(src, a);
    /* The context is reused across calls, so the PREVIOUS one's 31 htrees are
     * released here — re-initialising over them just abandoned them. */
    static sema_ctx_t sctx; static bool sctx_live = false;
    if (sctx_live) sema_destroy(&sctx);
    sema_init(&sctx, a); sctx_live = true; sema_analyze(&sctx, prog);
    static compiler_ctx_t cctx; compiler_init(&cctx, a, &sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (!methods[i]->name || strcmp(methods[i]->name, name)) continue;
        if (click) sir_optimize(&cctx, i);   /* in-place value rewrite + slot repack */
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(&cctx, i, &nsc);
        static wasm_types_t wt; wasm_types_build(&wt, &sctx);
        static burg_ctx_t bc; bc = (burg_ctx_t){0}; burg_ctx_init(&bc); bc.types = &wt;
        codegen_method_structured(methods[i], sc, nsc, &bc);
        *out_len = (int)bbq_vec_len(bc.emit.code);
        return bc.emit.code;
    }
    *out_len = 0; return NULL;
}

int main(void) {
    /* y = 2 + 3 is a constant Click folds + propagates; the while gives the
     * sidecar something to preserve across Click's mutation. */
    const char* SRC =
        "class T { int f(int x){ int y = 2 + 3; while (x > 0) { x = x - 1; } return y; } }";

    /* without Click: the addition is still there (2; 3; i32.add). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0; const uint8_t* body = emit(&a, SRC, "f", false, &n);
        const uint8_t add23[] = { 0x41,0x02, 0x41,0x03, 0x6A };  /* 2; 3; i32.add */
        CHECK(body != NULL, "no-Click compiled");
        CHECK(contains(body, n, add23, 5), "no-Click: the 2+3 add is present (unoptimized)");
        bbq_arena_free(&a);
    }
    /* with Click: 2+3 folds to 5 (the add is gone), AND the while still frames
     * (block+loop) — the scope sidecar survived Click's in-place rewrite. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0; const uint8_t* body = emit(&a, SRC, "f", true, &n);
        const uint8_t add23[] = { 0x41,0x02, 0x41,0x03, 0x6A };
        const uint8_t five[]  = { 0x41,0x05 };                   /* i32.const 5 (folded) */
        const uint8_t loop[]  = { 0x02,0x40, 0x03,0x40 };        /* block $break (loop $top */
        CHECK(body != NULL, "Click compiled");
        CHECK(!contains(body, n, add23, 5), "Click: the 2+3 add was folded away");
        CHECK(contains(body, n, five, 2),   "Click: folded to i32.const 5");
        CHECK(contains(body, n, loop, 4),   "Click: the while sidecar survived (block+loop framed)");
        bbq_arena_free(&a);
    }

    /* Click ON must not break ANY family: every control-flow construct keeps its
     * landing-pad Nops (sidecar) and every value family keeps lowering. Compile
     * each WITH Click and assert the family's framing / key op survives. */
    struct { const char* src; const char* m; const uint8_t* op; int opn; const char* label; } fam[] = {
      { "class T { int f(int x){ if (x > 0) { return 1; } return 0; } }", "f",
        (const uint8_t[]){0x04,0x40}, 2, "Click+if: if/end framing survives" },
      { "class T { void f(int x){ int r; switch (x){ case 0: r=1; break; case 1: r=2; break; default: r=3; } } }", "f",
        (const uint8_t[]){0x0E}, 1, "Click+switch: br_table survives" },
      { "class T { void f(int[] a){ try { a[0]=1; } catch (Throwable e) { } } }", "f",
        (const uint8_t[]){0x1F,0x40}, 2, "Click+try: try_table survives" },
      { "class Obj { int x; int f(Obj o){ return o.x; } }", "f",
        (const uint8_t[]){0xFB,0x02}, 2, "Click+field: struct.get survives" },
      { "class T { static int g(){ return 1; } static int f(){ return g(); } }", "f",
        (const uint8_t[]){0x12}, 1, "Click+call: tail call (return_call) survives" },
      { "class T { int f(int[] a){ return a[0]; } }", "f",
        (const uint8_t[]){0xFB,0x0B}, 2, "Click+array: array.get survives" },
      { "class T { int f(int x){ while (x > 0) { x = x - 1; } return x; } }", "f",
        (const uint8_t[]){0x03,0x40}, 2, "Click+while: loop framing survives" },
    };
    for (int k = 0; k < (int)(sizeof fam / sizeof fam[0]); k++) {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0; const uint8_t* body = emit(&a, fam[k].src, fam[k].m, true, &n);
        CHECK(body && n > 0, fam[k].label);
        CHECK(body && contains(body, n, fam[k].op, fam[k].opn), fam[k].label);
        bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_click_backend");
}
