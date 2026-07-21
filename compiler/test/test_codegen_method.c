// test_codegen_method.c — the whole front-to-back pipeline for a scalar method:
// parse → sema → ddcg (SIR) → burgc matcher → WASM function-body bytes. This is
// the S5.9 end-state in miniature minus the module wrapper: compile
// `static int add(int a,int b){return a+b;}` and pin its body to
// local.get 0; local.get 1; i32.add; return; end. If this is right, the only
// thing between here and add(3,5)==8 is the module framing (types/func/export/
// code sections) + the c-api load.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/codegen_method.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

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
// java.lang stubs + user source merged (sema needs Object as implicit super).
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
    if (!up) { printf("  FAIL parse user source\n"); fails++; }
    else for (int i=0;i<up->types_count;i++) PUSH(up->types[i]);
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr,t,(size_t)tc*sizeof(*arr)); free(t);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
    #undef PUSH
}

int main(void) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    ast_program_t* prog = build_program(
        "class T { static int add(int a, int b){ return a+b; } }", &arena);
    sema_ctx_t sctx; sema_init(&sctx, &arena);
    if (!sema_analyze(&sctx, prog)) printf("  (note: sema reported errors)\n");
    compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);

    sir_method_t* add = NULL;
    for (int i = 0; i < mc; i++)
        if (methods[i]->name && strcmp(methods[i]->name, "add") == 0) add = methods[i];
    CHECK(add != NULL, "found compiled method `add`");

    if (add) {
        /* Raw SIR (Click off) — the gate path. */
        burg_ctx_t ctx = {0}; burg_ctx_init(&ctx);
        codegen_method_body(add, &ctx);
        CHECK(!burg_has_error(&ctx), "no burg error tiling add");
        /* static int add(int,int): params are slots 0,1 (no `this`). */
        const uint8_t want[] = {
            0x20, 0x00,   /* local.get 0 */
            0x20, 0x01,   /* local.get 1 */
            0x6A,         /* i32.add     */
            0x0F,         /* return      */
            0x0B,         /* end         */
        };
        int n = (int)bbq_vec_len(ctx.emit.code);
        CHECK(n == (int)sizeof want && memcmp(ctx.emit.code, want, sizeof want) == 0,
              "add body = local.get 0; local.get 1; i32.add; return; end");
        if (n != (int)sizeof want || memcmp(ctx.emit.code, want, (size_t)n) != 0) {
            printf("  got %d bytes:", n);
            for (int i = 0; i < n; i++) printf(" %02X", ctx.emit.code[i]);
            printf("\n");
        }
        bbq_vec_free(ctx.emit.code); burg_ctx_free(&ctx);
    }

    if (fails) { printf("test_codegen_method: %d FAILED\n", fails); return 1; }
    printf("test_codegen_method: OK\n");
    return 0;
}
