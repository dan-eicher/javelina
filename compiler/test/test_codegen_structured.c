// test_codegen_structured.c — S5.7b. Destination-driven structured WASM emit for
// control flow. Compile real `if` methods end-to-end (parse→sema→ddcg→sidecar→
// structured emit) and pin the body bytes: the cond tiles via the burg, the
// branch frames a native WASM if/else/end. This is the plan's step-7 gate
// (if → valid body bytes); while/loops land next.
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

/* Compile `src`, structured-emit method `name`'s body into `out` (returns len). */
static int emit_body(bbq_arena* a, const char* src, const char* name, const uint8_t** out) {
    ast_program_t* prog = build_program(src, a);
    static sema_ctx_t sctx; sema_init(&sctx, a); sema_analyze(&sctx, prog);
    static compiler_ctx_t cctx; compiler_init(&cctx, a, &sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (!methods[i]->name || strcmp(methods[i]->name, name)) continue;
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(&cctx, i, &nsc);
        static burg_ctx_t bc; bc = (burg_ctx_t){0}; burg_ctx_init(&bc);
        codegen_method_structured(methods[i], sc, nsc, &bc);
        *out = bc.emit.code;
        return (int)bbq_vec_len(bc.emit.code);
    }
    *out = NULL; return -1;
}

static void check_bytes(const char* m, const uint8_t* got, int n,
                        const uint8_t* want, int wn) {
    if (n != wn || memcmp(got, want, (size_t)wn) != 0) {
        printf("  FAIL  %s\n    want:", m);
        for (int i=0;i<wn;i++) printf(" %02X", want[i]);
        printf("\n    got: ");
        for (int i=0;i<n;i++) printf(" %02X", got[i]);
        printf("\n"); fails++;
    }
}

int main(void) {
    /* if-no-else: then-arm returns, false arm is the join.
     * cond x>0: local.get 0; i32.const 0; i32.gt_s
     * if void; then: i32.const 1; return; end; join: i32.const 0; return; end */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ if (x > 0) { return 1; } return 0; } }", "f", &body);
        /* `f` is an instance method → `this` is slot 0, `x` is slot 1. */
        const uint8_t want[] = { 0x20,0x01, 0x41,0x00, 0x4A,   /* x>0 (local.get 1) */
                                 0x04,0x40,                     /* if void */
                                 0x41,0x01, 0x0F,               /* return 1 */
                                 0x0B,                          /* end (if) */
                                 0x41,0x00, 0x0F,               /* return 0 */
                                 0x0B };                        /* end (body) */
        CHECK(n > 0, "if-no-else: emitted");
        check_bytes("if-no-else body", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* if-else: both arms store then join, join returns the local.
     * if void; then: const 1; local.set; else; const 2; local.set; end; ... */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ int r; if (x > 0) { r = 1; } else { r = 2; } return r; } }", "f", &body);
        /* `f` instance → this=slot0, x=slot1, r=slot2. Both arms store r and
         * converge at the if/end; the join then returns r. */
        const uint8_t want[] = {
            0x20,0x01, 0x41,0x00, 0x4A,   /* x>0 : local.get 1; i32.const 0; i32.gt_s */
            0x04,0x40,                    /* if void                 */
            0x41,0x01, 0x21,0x02,         /* then: r = 1             */
            0x05,                         /* else                    */
            0x41,0x02, 0x21,0x02,         /* else: r = 2             */
            0x0B,                         /* end (if)                */
            0x20,0x02, 0x0F,              /* return r                */
            0x0B };                       /* end (body)              */
        CHECK(n > 0, "if-else: emitted");
        check_bytes("if-else body", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* while loop: block $break (loop $top  cond; eqz; br_if 1; body; br 0  end end)
     * void f(int x){ while(x>0){ x = x-1; } }  — x is slot 1 (instance method). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x > 0) { x = x - 1; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40,                       /* block void ($break)         */
            0x03,0x40,                       /* loop void ($top)            */
            0x20,0x01, 0x41,0x00, 0x4A,      /* x>0 : local.get 1; i32.const 0; i32.gt_s */
            0x45,                            /* i32.eqz (break if !cond)    */
            0x0D,0x01,                       /* br_if 1 → $break            */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01, /* x-1; local.set 1      */
            0x0C,0x00,                       /* br 0 → $top (back-edge)     */
            0x0B,                            /* end (loop)                  */
            0x0B,                            /* end (block)                 */
            0x0F,                            /* return (void)               */
            0x0B };                          /* end (body)                  */
        CHECK(n > 0, "while: emitted");
        check_bytes("while body", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* Control-destination inheritance (Dybvig §3): a nested loop in tail position
     * of the outer body inherits the outer's control destination. The inner loop
     * gets NO $break wrapper and its exit brs straight to the outer loop ($otop)
     * — one jump, not the jump-into-an-empty-$ibreak-block-then-br-$otop the
     * paper warns about (and which a goto-machine would clean up with a peephole). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x > 0) { while (x > 1) { x = x - 1; } } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40,                              /* block $obreak               */
            0x03,0x40,                              /* loop $otop                  */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x01,  /* x>0; eqz; br_if 1 → $obreak */
            0x03,0x40,                              /* loop $itop (NO $ibreak block) */
            0x20,0x01, 0x41,0x01, 0x4A, 0x45, 0x0D,0x01,  /* x>1; eqz; br_if 1 → $otop (INHERITED) */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01,  /* x = x - 1                   */
            0x0C,0x00,                              /* br 0 → $itop (back-edge)    */
            0x0B,                                   /* end (inner loop)            */
            0x0B,                                   /* end (outer loop)            */
            0x0B,                                   /* end (block $obreak)         */
            0x0F, 0x0B };                           /* return; end (body)          */
        CHECK(n > 0, "nested-loop inheritance: emitted");
        check_bytes("nested-loop inheritance (one jump, no $ibreak wrapper)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* Same inheritance for a switch in tail position (Dybvig §3, the switch case).
     * The switch's $break here is a pure collect-and-forward trampoline: all three
     * arms br to it, then it brs once to $otop. In tail position that wrapper is
     * elided — each case break and the default exit br straight to $otop. Three
     * stacked blocks ($default + 2 cases), NOT four; no $break end + forwarding br. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x > 0) { switch (x) { case 0: x = 1; break; case 1: x = 2; break; default: x = x - 1; } } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40,                              /* block $obreak               */
            0x03,0x40,                              /* loop $otop                  */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x01,  /* x>0; eqz; br_if 1 → $obreak */
            0x20,0x01, 0x21,0x02,                   /* spill selector to t2        */
            0x02,0x40, 0x02,0x40, 0x02,0x40,        /* block $default, $case1, $case0 (NO $break) */
            0x20,0x02,                              /* local.get 2 (selector)      */
            0x0E, 0x02, 0x00,0x01, 0x02,            /* br_table [0,1] default 2    */
            0x0B,                                   /* end $case0                  */
            0x41,0x01, 0x21,0x01, 0x0C,0x02,        /* x=1; br 2 → $otop           */
            0x0B,                                   /* end $case1                  */
            0x41,0x02, 0x21,0x01, 0x0C,0x01,        /* x=2; br 1 → $otop           */
            0x0B,                                   /* end $default                */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01,  /* x = x - 1                   */
            0x0C,0x00,                              /* br 0 → $otop (default exit) */
            0x0B,                                   /* end (outer loop)            */
            0x0B,                                   /* end (block $obreak)         */
            0x0F, 0x0B };                           /* return; end (body)          */
        CHECK(n > 0, "switch-in-tail inheritance: emitted");
        check_bytes("switch-in-tail inheritance (no $break trampoline)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* `!` in predicate position — Dybvig Fig 7's destination swap (done in the
     * ddcg): no materialized boolean (no temp, no LogNot/i32.eqz), the true/
     * false control destinations are swapped and E recurses. Composes through
     * the structured backend; `!!` double-swaps back to the original. */
    {   /* while(!b): the test is a plain `b; br_if break` — no spill, and the
         * LogNot eqz that would double-cancel the break-on-false eqz is gone. */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(boolean b){ while (!b) { b = true; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x03,0x40,                   /* block $break; loop $top      */
            0x20,0x01, 0x0D,0x01,                   /* local.get b; br_if 1 → $break */
            0x41,0x01, 0x21,0x01,                   /* b = true                     */
            0x0C,0x00, 0x0B, 0x0B,                  /* br 0; end loop; end block    */
            0x0F, 0x0B };                           /* return; end body             */
        check_bytes("!E loop test (swap: b; br_if, no eqz/spill)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }
    {   /* if(!b) A else B: arms swapped at the branch, eqz dropped. */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(boolean b){ if (!b) { return 1; } else { return 2; } } }", "f", &body);
        const uint8_t want[] = {
            0x20,0x01, 0x04,0x40,                   /* local.get b; if              */
            0x41,0x02, 0x0F,                        /* then (b true): return 2      */
            0x05,                                   /* else                         */
            0x41,0x01, 0x0F,                        /* else (b false): return 1     */
            0x0B, 0x0B };                           /* end if; end body             */
        check_bytes("!E if-else (swap arms, no eqz)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }
    {   /* if(!!b): two swaps cancel — branch on b directly, no eqz at all. */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(boolean b){ if (!!b) { return 1; } else { return 2; } } }", "f", &body);
        const uint8_t want[] = {
            0x20,0x01, 0x04,0x40,                   /* local.get b; if              */
            0x41,0x01, 0x0F, 0x05, 0x41,0x02, 0x0F, /* return 1; else; return 2     */
            0x0B, 0x0B };
        check_bytes("!!E cancels to E (no eqz, no swap)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* if-else in TAIL position of a loop body. Both arms inherit the if's own
     * control destination (Dybvig §3 — the recorded Ljoin) and fall through to
     * the structured if/else `end`; ONE shared back-edge brs to $top after the
     * end. NOT a back-edge duplicated into each arm (which a graph-walk merge
     * that crosses the loop back-edge would produce). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x > 0) { if (x > 1) { x = 1; } else { x = 2; } } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x03,0x40,                   /* block $break; loop $top      */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x01,  /* x>0; eqz; br_if 1 → $break */
            0x20,0x01, 0x41,0x01, 0x4A,             /* x>1                          */
            0x04,0x40,                              /* if                           */
            0x41,0x01, 0x21,0x01,                   /* then: x = 1 (falls to end)   */
            0x05,                                   /* else                         */
            0x41,0x02, 0x21,0x01,                   /* else: x = 2 (falls to end)   */
            0x0B,                                   /* end if                       */
            0x0C,0x00,                              /* br 0 → $top (ONE back-edge)  */
            0x0B, 0x0B,                             /* end loop; end block          */
            0x0F, 0x0B };                           /* return; end body             */
        check_bytes("if-else in loop tail (arms inherit Ljoin, one shared back-edge)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }
    /* Short-circuit && shares ONE else (Fig. 7): the ddcg records the shared exit
     * as a MERGE label; the structurer frames it as `block $else` so BOTH condition
     * branches `eqz; br_if $else` to it and the else arm is emitted exactly ONCE
     * (the old lowering nested a native if per && term and re-emitted the else in
     * each — an if-else-if chain then exploded 2^depth; now linear). Both arms
     * RETURN, so nothing reaches the if-join: per the paper it is not a label and
     * gets no block (docs/ddcg-merge-labels.md §2.2) — only $else is framed. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ if (x > 0 && x < 9) { return 1; } else { return 2; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40,                              /* block $else                  */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x00,  /* x>0; eqz; br_if 0 → $else */
            0x20,0x01, 0x41,0x09, 0x48, 0x45, 0x0D,0x00,  /* x<9; eqz; br_if 0 → $else */
            0x41,0x01, 0x0F,                        /* return 1 (terminates — no join) */
            0x0B,                                   /* end $else                    */
            0x41,0x02, 0x0F,                        /* return 2 (the shared arm, ONCE) */
            0x0B };                                 /* end body                     */
        check_bytes("if (a && b) both return: shared else once, no if-join block", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }
    /* Short-circuit && with FALL-THROUGH arms: the then arm brs over the shared
     * else to $join; the else arm falls through the $join end. Both stores of `r`
     * appear once. This is the shape an if-else-if chain builds — verifying the
     * then arm's `br $join` and the single shared else together. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ int r; if (x > 0 && x < 9) { r = 1; } else { r = 2; } return r; } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x02,0x40,                   /* block $join; block $else     */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x00,  /* x>0; eqz; br_if 0 → $else */
            0x20,0x01, 0x41,0x09, 0x48, 0x45, 0x0D,0x00,  /* x<9; eqz; br_if 0 → $else */
            0x41,0x01, 0x21,0x02,                   /* r = 1 (then arm)             */
            0x0C,0x01,                              /* br 1 → $join (skip else)     */
            0x0B,                                   /* end $else                    */
            0x41,0x02, 0x21,0x02,                   /* r = 2 (shared else, ONCE)    */
            0x0B,                                   /* end $join                    */
            0x20,0x02, 0x0F,                        /* return r                     */
            0x0B };                                 /* end body                     */
        check_bytes("if (a && b) fall-through arms: shared else once, then brs to $join", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }
    /* Short-circuit || is the mirror: the ddcg records the shared THEN as a MERGE
     * label, so each branch `br_if $then` on TRUE (no eqz), the else arm is the
     * fall-through inside the block, and the then arm is emitted ONCE after. Both
     * arms return → nothing reaches the if-join, so no if-join block (§2.2). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ if (x < 0 || x > 9) { return 1; } else { return 2; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40,                              /* block $then                   */
            0x20,0x01, 0x41,0x00, 0x48, 0x0D,0x00,  /* x<0; br_if 0 → $then (on true) */
            0x20,0x01, 0x41,0x09, 0x4A, 0x0D,0x00,  /* x>9; br_if 0 → $then (on true) */
            0x41,0x02, 0x0F,                        /* return 2 (else, fall-through)  */
            0x0B,                                   /* end $then                      */
            0x41,0x01, 0x0F,                        /* return 1 (shared then, ONCE)   */
            0x0B };                                 /* end body                       */
        check_bytes("if (a || b) both return: shared then once, no if-join block", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* do-while in a loop tail. The do-while test is at the loop TAIL (br_if back
     * to $itop), so its false case FALLS THROUGH the inner `end` — and must then
     * br to the outer back-edge ($otop), NOT fall out through the outer `end`.
     * (Do-while had never been wired into the backend; it hung codegen until the
     * loop sidecar + tail-test handling landed. Regression guard for that.) */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x > 0) { do { x = x - 1; } while (x > 5); } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x03,0x40,                   /* block $obreak; loop $otop    */
            0x20,0x01, 0x41,0x00, 0x4A, 0x45, 0x0D,0x01,  /* x>0; eqz; br_if 1 → $obreak */
            0x03,0x40,                              /* loop $itop (do-while)        */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01,  /* x = x - 1                    */
            0x20,0x01, 0x41,0x05, 0x4A,             /* x > 5                        */
            0x0D,0x00,                              /* br_if 0 → $itop (repeat)     */
            0x0B,                                   /* end $itop                    */
            0x0C,0x00,                              /* br 0 → $otop (inner exit → outer re-test) */
            0x0B, 0x0B,                             /* end $otop; end $obreak       */
            0x0F, 0x0B };                           /* return; end body             */
        check_bytes("do-while in loop tail (false-exit brs to outer back-edge)", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    if (fails) { printf("test_codegen_structured: %d FAILED\n", fails); return 1; }
    printf("test_codegen_structured: OK\n");
    return 0;
}
