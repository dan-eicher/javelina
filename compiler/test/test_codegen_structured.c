// test_codegen_structured.c — destination-driven structured WASM emit for
// control flow. Compile real `if` methods end-to-end (parse→sema→ddcg→sidecar→
// structured emit) and pin the body bytes: the cond tiles via the burg, the
// branch frames a native WASM if/else/end. This is the plan's step-7 gate
// (if → valid body bytes); while/loops land next.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/codegen_method.h"
#include "javelina/compiler/emit_wasm.h"   /* the REAL encoder builds the tail marker */
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

/* §7.3 per-unit parse (see jtest_units.h) — the flat program still feeds
 * compiler_compile; sema gets the unit list via jtest_analyze. */
#include "jtest_units.h"
#define build_program jtest_build_flat

/* Compile `src`, structured-emit method `name`'s body into `out` (returns len). */
/* The backend's own diagnostic from the last emit_body call, or NULL. The
 * structurer enforces emit-once and no-inlined-label internally, so THIS is the
 * direct oracle for duplication — a byte count is a proxy that a truncated body
 * can still satisfy. Every fixture asserts it. */
static const char* emit_body_error;

static int emit_body(bbq_arena* a, const char* src, const char* name, const uint8_t** out) {
    ast_program_t* prog = build_program(src, a);
    /* The context is reused across calls, so the PREVIOUS one's 31 htrees are
     * released here — re-initialising over them just abandoned them. */
    static sema_ctx_t sctx; static bool sctx_live = false;
    if (sctx_live) sema_destroy(&sctx);
    sema_init(&sctx, a); sctx_live = true; jtest_analyze(&sctx);
    static compiler_ctx_t cctx; compiler_init(&cctx, a, &sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < jtest_last_nlib) continue;   /* user snippet only */
        if (!methods[i]->name || strcmp(methods[i]->name, name)) continue;
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(&cctx, i, &nsc);
        /* Field layout, so a fixture may use objects and arrays and not just
         * ints in locals — the control shapes worth pinning at depth are the
         * ones whose operands carry bounds guards. */
        static wasm_types_t wt; wasm_types_build(&wt, &sctx, NULL, 0);
        static burg_ctx_t bc; bc = (burg_ctx_t){0}; burg_ctx_init(&bc); bc.types = &wt;
        codegen_method_structured(methods[i], sc, nsc, &bc);
        emit_body_error = bc.burg_error_msg;
        *out = bc.emit.code;
        return (int)bbq_vec_len(bc.emit.code);
    }
    *out = NULL; emit_body_error = NULL; return -1;
}

static void check_bytes(const char* m, const uint8_t* got, int n,
                        const uint8_t* want, int wn) {
    if (n != wn || memcmp(got, want, (size_t)wn) != 0) {
        printf("  FAIL  %s\n    want:", m);
        for (int i=0;i<wn;i++) printf(" %02X", want[i]);
        printf("\n    got: ");
        for (int i=0;i<n;i++) printf(" %02X", got[i]);
        printf("\n"); TEST_FAILED();
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
            0x0B,                                   /* end if                       */
            /* Both arms return, so the point after the `end` is unreachable in the
             * SIR — but §7.6 resets reachability after every `end`, and the
             * function declares a result, so the epilogue caps it. This byte was
             * absent from the expectation only because emit_body used to leave
             * burg_ctx_t.types NULL, and method_returns_value reads the result
             * type through it: the harness could not see that `f` returns a
             * value. The shipped pipeline always supplies types. */
            0x00,
            0x0B };                                 /* end body                     */
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
            0x0B,                                   /* end if                       */
            0x00,                                   /* §7.6 epilogue cap — see above */
            0x0B };                                 /* end body                     */
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

    /* ── The branch context, end to end ───────────────────────────────────────
     *
     * The tiles themselves are pinned in test_codegen_wasm; what has to be proved
     * HERE is the half the matcher cannot do: that the structurer honours the
     * polarity a tile hands back. Each pin below is a whole compiled method, so a
     * mishandled flag shows up as an inverted branch — the arms in the wrong
     * order, or a br_if going the wrong way — not as a byte count.
     *
     * Falsification runs, all 2026-07-30. Removing the `cond` context rules reds
     * four of these (the two `while` pins and the two that show a condition tiling
     * to nothing). The remaining two need their own falsifiers, because removing a
     * rule merely stops any inversion happening — so they were falsified against
     * the mechanism they actually pin: deleting emit_cond's `cond_neg` reset reds
     * "polarity does not leak" and nothing else, and making the swap
     * unconditional reds "one-armed inverted if restores polarity".
     *
     * `if (x != 0)`: the condition needs NO code. Before this, the compare
     * materialised (i32.const 0; i32.ne, or after B2 an eqz pair) and the `if`
     * tested the result. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ if (x != 0) { return 1; } return 0; } }", "f", &body);
        const uint8_t want[] = { 0x20,0x01,                     /* local.get x — and that IS the condition */
                                 0x04,0x40,                     /* if void          */
                                 0x41,0x01, 0x0F,               /* then: return 1   */
                                 0x0B,                          /* end (if)         */
                                 0x41,0x00, 0x0F,               /* return 0         */
                                 0x0B };
        CHECK(n > 0, "if (x != 0): emitted");
        check_bytes("if (x != 0) tiles the operand alone", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* `if (x == 0)` with ONE arm: the tile inverts, but exchanging the arms here
     * would put the body in an `else` behind an empty then — the same bytes as
     * restoring the polarity, and a worse shape. So the eqz comes back and the
     * arms stay in source order. This pins the decision, not just the size. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { int f(int x){ if (x == 0) { return 1; } return 2; } }", "f", &body);
        const uint8_t want[] = { 0x20,0x01,                     /* local.get x            */
                                 0x45,                          /* eqz — polarity restored */
                                 0x04,0x40,                     /* if void                */
                                 0x41,0x01, 0x0F,               /* then: return 1         */
                                 0x0B,                          /* end (if)               */
                                 0x41,0x02, 0x0F,               /* return 2               */
                                 0x0B };
        CHECK(n > 0, "if (x == 0): emitted");
        check_bytes("one-armed inverted if restores polarity rather than emptying the then",
                    body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* `if (x == 0) … else …` with BOTH arms real: now the exchange is free, so it
     * happens — no eqz, and `return 1` (the x==0 arm) comes out in the ELSE, after
     * the 0x05. Get the flag wrong here and the method returns the wrong value for
     * every input; the two pins together cover both sides of the swap decision. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a,
            "class T { int f(int x){ int r; if (x == 0) { r = 1; } else { r = 2; } return r; } }",
            "f", &body);
        const uint8_t want[] = { 0x20,0x01,                     /* local.get x — no eqz   */
                                 0x04,0x40,                     /* if void                */
                                 0x41,0x02, 0x21,0x02,          /* then (x nonzero): r=2  */
                                 0x05,                          /* else                   */
                                 0x41,0x01, 0x21,0x02,          /* else  (x==0):     r=1  */
                                 0x0B,                          /* end (if)               */
                                 0x20,0x02, 0x0F,               /* return r               */
                                 0x0B };
        CHECK(n > 0, "if (x == 0) else: emitted");
        check_bytes("two-armed inverted if exchanges the arms and drops the eqz",
                    body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* A while test reaches the OTHER kind of site: it branches on FALSE, so it
     * already emitted an i32.eqz to invert. An inverting tile has done that
     * inversion already and the eqz must be DROPPED — emitting both would restore
     * the original polarity and loop exactly when it should exit. Compare with the
     * `while (x > 0)` pin above, which still carries its 0x45. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x != 0) { x = x - 1; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x03,0x40,                    /* block $break; loop $top     */
            0x20,0x01,                               /* local.get x = the condition */
            0x45,                                    /* eqz — the site's OWN inversion, kept:
                                                        the tile did not invert here      */
            0x0D,0x01,                               /* br_if 1 → $break            */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01,   /* x = x - 1                   */
            0x0C,0x00, 0x0B, 0x0B,                   /* br $top; end loop; end block */
            0x0F, 0x0B };
        CHECK(n > 0, "while (x != 0): emitted");
        check_bytes("while (x != 0) keeps the site's own eqz", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* …and the inverting twin at that same site: `while (x == 0)` tiles to the
     * same operand with the polarity flipped, so the site's eqz CANCELS and the
     * whole condition is one local.get plus the br_if. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a, "class T { void f(int x){ while (x == 0) { x = x - 1; } } }", "f", &body);
        const uint8_t want[] = {
            0x02,0x40, 0x03,0x40,                    /* block $break; loop $top     */
            0x20,0x01,                               /* local.get x — NO eqz: the tile
                                                        inverted, cancelling the site's */
            0x0D,0x01,                               /* br_if 1 → $break            */
            0x20,0x01, 0x41,0x01, 0x6B, 0x21,0x01,   /* x = x - 1                   */
            0x0C,0x00, 0x0B, 0x0B,
            0x0F, 0x0B };
        CHECK(n > 0, "while (x == 0): emitted");
        check_bytes("while (x == 0) cancels the site's eqz", body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* Two branches back to back, the first inverting and the second not. If the
     * polarity leaked from one condition to the next, the second `if` would come
     * out with its arms exchanged. This is the reset discipline, end to end. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a,
            "class T { int f(int x, int y){ if (x == 0) { return 1; } if (y > 3) { return 2; } return 3; } }",
            "f", &body);
        const uint8_t want[] = {
            0x20,0x01, 0x45,                    /* local.get x; eqz (one-armed, so restored) */
            0x04,0x40,                          /* if void                           */
              0x41,0x01, 0x0F,                  /* then: return 1                    */
            0x0B,                               /* end (outer if)                    */
            0x20,0x02, 0x41,0x03, 0x4A,         /* y > 3 — a NON-inverting condition */
            0x04,0x40,                          /* if void                           */
              0x41,0x02, 0x0F,                  /*   return 2 stays the THEN arm     */
            0x0B,
            0x41,0x03, 0x0F,                    /* return 3                          */
            0x0B };
        CHECK(n > 0, "back-to-back branches: emitted");
        check_bytes("polarity does not leak from one branch to the next",
                    body, n, want, (int)sizeof want);
        bbq_arena_free(&a);
    }

    /* ── a SPILLED condition's if-join: the tail is emitted ONCE ────────────────
     * `record_scope(test, Ljoin, 0)` keys the if-join on the condition's HEAD. When
     * the condition needs a temp (`v.length` — the arraylength spills), that head is
     * the spill StoreLocal, not the Branch, and emit_spine must CARRY the recorded
     * join from the keyed node to the Branch that consumes it (`pending_join`, the
     * 07-27 fix). If the carry breaks, `ljoin` falls back to the region end and both
     * arms emit the whole method tail — 2^k for k such ifs.
     *
     * The oracle is the TAIL'S OWN EMISSION COUNT, at this level, in this method:
     * `x * 12345` appears once in source, so its i32.const bytes appear once in the
     * body — or 8 times (2^3) with the carry broken. This replaces the module-LENGTH
     * pin in test_wasm_module, which read green through the wide-literal and
     * Click-orphan variants of this same disease for months: an e2e size delta with a
     * deliberately loose bound says "not catastrophic", not "correct". */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        /* The spilling condition must stay pure scalar: this harness runs with
         * types=NULL, so anything touching a reference (v.length's null guard news
         * the exception object) cannot emit here. `(x+y)*3 != 1` spills the same
         * way — a complex compare LHS is Figure 8 case 3, captured to a temp — with
         * no guard in sight. */
        int n = emit_body(&a,
            "class T { int f(int y, int x){"
            "  if ((x + y) * 3 != 1) x = x + 1;"
            "  if ((x + y) * 5 != 2) x = x + 2;"
            "  if ((x + y) * 7 != 3) x = x + 4;"
            "  return x * 12345; } }", "f", &body);
        CHECK(n > 0, "spilled-condition ifs: emitted");
        uint8_t mark[8]; int ml = 0;                 /* i32.const 12345, real encoder */
        { emit_wasm_ctx m = {0};
          ew_emit(&m, WOP_I32_CONST); ew_i32(&m, 12345);
          ml = (int)bbq_vec_len(m.code); memcpy(mark, m.code, (size_t)ml);
          bbq_vec_free(m.code); }
        int seen = 0;
        for (int i = 0; i + ml <= n; i++)
            if (memcmp(body + i, mark, (size_t)ml) == 0) seen++;
        if (seen != 1)
            printf("        tail marker emitted %d times (want 1: 8 = the 2^3 join loss)\n", seen);
        CHECK(seen == 1, "a spilled condition keeps its if-join: the tail is emitted ONCE");
        bbq_arena_free(&a);
    }

    /* ── every numeric conversion emits its opcode ─────────────────────────────
     * `cg_promote` matches nine cast kinds and ends `_other => t` — returning the
     * value UNCONVERTED. Sema can produce thirty (SEMA_CAST_KIND_*), so seventeen
     * of them fall into that default, including D2L, F2L and D2F, for which the SIR
     * has nodes (SIR_D2L / SIR_F2L / SIR_D2F) that would then never be built. A
     * dropped conversion is a wrong value, not a crash, so nothing downstream
     * necessarily complains.
     *
     * §5.1's conversions each have exactly one WASM opcode, so the pin is direct:
     * do the cast, assert the opcode is in the bytes. Widenings included as
     * controls — if those fail the fixture is wrong, not the compiler. */
    {
        /* float→integer narrowing SATURATES in Java (§5.1.3: NaN → 0, out of range
         * → MIN/MAX), so the conformant opcode is the 0xFC-prefixed trunc_sat form;
         * the one-byte i32.trunc_f64_s would TRAP on exactly those inputs. Values
         * above 0xFF below are that two-byte sequence. */
        static const struct { const char* name; const char* src; uint16_t op; } convs[] = {
          { "L2I  i32.wrap_i64",        "class T { int f(long x){ return (int) x; } }",            0xA7 },
          { "D2I  i32.trunc_sat_f64_s", "class T { int f(double x){ return (int) x; } }",        0xFC02 },
          { "F2I  i32.trunc_sat_f32_s", "class T { int f(float x){ return (int) x; } }",         0xFC00 },
          { "I2L  i64.extend_i32_s",    "class T { long f(int x){ return (long) x; } }",           0xAC },
          { "D2L  i64.trunc_sat_f64_s", "class T { long f(double x){ return (long) x; } }",      0xFC06 },
          { "F2L  i64.trunc_sat_f32_s", "class T { long f(float x){ return (long) x; } }",       0xFC04 },
          { "I2F  f32.convert_i32_s",   "class T { float f(int x){ return (float) x; } }",         0xB2 },
          { "L2F  f32.convert_i64_s",   "class T { float f(long x){ return (float) x; } }",        0xB4 },
          { "D2F  f32.demote_f64",      "class T { float f(double x){ return (float) x; } }",      0xB6 },
          { "I2D  f64.convert_i32_s",   "class T { double f(int x){ return (double) x; } }",       0xB7 },
          { "L2D  f64.convert_i64_s",   "class T { double f(long x){ return (double) x; } }",      0xB9 },
          { "F2D  f64.promote_f32",     "class T { double f(float x){ return (double) x; } }",     0xBB },
        };
        for (size_t k = 0; k < sizeof convs / sizeof convs[0]; k++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            const uint8_t* body = NULL;
            int n = emit_body(&a, convs[k].src, "f", &body);
            int seen = 0, two = convs[k].op > 0xFF;
            for (int i = 0; n > 0 && i + (two ? 1 : 0) < n; i++)
                if (two ? (body[i] == (convs[k].op >> 8)
                           && body[i + 1] == (convs[k].op & 0xFF))
                        : (body[i] == convs[k].op)) seen++;
            if (n <= 0 || !seen)
                printf("        %-28s body=%d bytes, opcode %04X not emitted\n",
                       convs[k].name, n, convs[k].op);
            CHECK(n > 0 && seen >= 1, convs[k].name);
            bbq_arena_free(&a);
        }
    }

    /* ── every §1 site keeps its join: the tail is emitted ONCE ────────────────
     * docs/ddcg-merge-labels.md §1 enumerates the places the ddcg hands ONE
     * destination to two consumers. §2's rule is that such a node is a label whose
     * code is emitted once. That is one property, so it gets one table rather than
     * one hand-written case per construct — a construct absent from the table is a
     * construct nothing pins, which is how `ASCIIToBinaryBuffer.doubleValue` came
     * to duplicate in the shipped prelude with a full-green suite.
     *
     * Oracle: the tail's OWN emission count in this method (the §1 fixtures each
     * repeat the construct 3×, so a lost join shows as 8, not 2). A module-size
     * bound would read green through exactly the variants that broke here.
     *
     * Reference-typed constructs (ref cast, array cast, null guards) are NOT here:
     * this harness runs with types=NULL, so anything that news an exception object
     * cannot emit. They are pinned at the sidecar level instead. */
    {
        static const struct { const char* name; const char* src; } sites[] = {
          { "value &&",
            "class T { int f(int x, int y){ boolean p = x>0 && y>0; boolean q = x>1 && y>1;"
            " boolean s = x>2 && y>2; if (p) x=x+1; if (q) x=x+2; if (s) x=x+4;"
            " return x * 24680; } }" },
          { "value ||",
            "class T { int f(int x, int y){ boolean p = x>0 || y>0; boolean q = x>1 || y>1;"
            " boolean s = x>2 || y>2; if (p) x=x+1; if (q) x=x+2; if (s) x=x+4;"
            " return x * 24680; } }" },
          { "guarded int div",
            "class T { int f(int x, int y){ int a = x / y; int b = (x+1) / y; int c = (x+2) / y;"
            " return (a+b+c) + x * 24680; } }" },
          { "guarded int rem",
            "class T { int f(int x, int y){ int a = x % y; int b = (x+1) % y; int c = (x+2) % y;"
            " return (a+b+c) + x * 24680; } }" },
          { "guarded long div",
            "class T { int f(long x, long y){ long a = x / y; long b = (x+1) / y; long c = (x+2) / y;"
            " return (int)(a+b+c) + (int)x * 24680; } }" },
          { "ternary, simple cond",
            "class T { int f(int x, boolean b){ x = x + (b ? 0-1 : 1); x = x + (b ? 0-2 : 2);"
            " x = x + (b ? 0-3 : 3); return x * 24680; } }" },
          { "ternary, spilled cond",
            "class T { int f(int x, int y){ x = x + ((x+y)*3 != 1 ? 0-1 : 1);"
            " x = x + ((x+y)*5 != 2 ? 0-2 : 2); x = x + ((x+y)*7 != 3 ? 0-3 : 3);"
            " return x * 24680; } }" },
          { "ternary, nested",
            "class T { int f(int x, boolean b){ x = x + (b ? (x>0 ? 1 : 2) : 3);"
            " x = x + (b ? (x>1 ? 4 : 5) : 6); x = x + (b ? (x>2 ? 7 : 8) : 9);"
            " return x * 24680; } }" },
          { "ternary in && cond",
            "class T { int f(int x, int y, boolean b){ if ((b ? x : y) > 0 && x > 0) x=x+1;"
            " if ((b ? x : y) > 1 && x > 1) x=x+2; if ((b ? x : y) > 2 && x > 2) x=x+4;"
            " return x * 24680; } }" },
          /* DECOMPOSITION — the failing fixtures below combine three things at once
           * (a ternary, a comparison, and an if test), so on their own they cannot
           * say which is required. These four vary one thing at a time. */
          { "decomp: compare in if, no ternary",
            "class T { int f(int x){ if (x > 0) x=x+1; if (x > 1) x=x+2;"
            " if (x > 2) x=x+4; return x * 24680; } }" },
          { "decomp: ternary in value ctx, no if",
            "class T { int f(int x, int y, boolean b){ int r = b ? x : y;"
            " int s = b ? y : x; int t = b ? x : y; return (r+s+t) + x * 24680; } }" },
          { "decomp: ternary compared, no if",
            "class T { int f(int x, int y, boolean b){ boolean c = (b ? x : y) > 0;"
            " boolean d = (b ? y : x) > 1; return (c ? 1 : 0) + (d ? 2 : 0) + x * 24680; } }" },
          { "decomp: ternary IS the if cond",
            "class T { int f(int x, boolean p, boolean q, boolean b){ if (b ? p : q) x=x+1;"
            " if (b ? q : p) x=x+2; return x * 24680; } }" },

          /* LEFTMOST-ness is the prediction. The collision needs the ternary's head
           * to BE the condition's head, which happens only when the ternary is the
           * first thing evaluated. As the RHS of the compare, or to the right of a
           * short-circuit, the condition's head belongs to something else and the
           * two BLOCK rows land on different keys. If these fail too, "leftmost" is
           * wrong and so is the diagnosis. */
          { "decomp: ternary as RHS of compare in if",
            "class T { int f(int x, int y, boolean b){ if (0 < (b ? x : y)) x=x+1;"
            " if (1 < (b ? y : x)) x=x+2; return x * 24680; } }" },
          /* `0 < (b?x:y)` reaches binary_arith_sc only because a LITERAL lhs is
           * `is_constant_operand`. With a LOCAL on the left neither sc (needs a
           * constant lhs) nor cs (needs a simple rhs) matches, so it falls to
           * Figure 8 case 4 — binop_spilled, both operands spilled. That is a
           * different path, and it is the ordinary Java shape. */
          { "decomp: ternary as RHS, local LHS (case 4)",
            "class T { int f(int x, int y, boolean b){ if (x > (b ? x : y)) x=x+1;"
            " if (y > (b ? y : x)) x=x+2; return x * 24680; } }" },
          { "decomp: ternary as LHS, local RHS (case 4)",
            "class T { int f(int x, int y, boolean b){ if ((b ? x : y) > x) x=x+1;"
            " if ((b ? y : x) > y) x=x+2; return x * 24680; } }" },
          { "decomp: ternary right of && in if",
            "class T { int f(int x, int y, boolean b){ if (x > 0 && (b ? x : y) > 0) x=x+1;"
            " if (x > 1 && (b ? y : x) > 1) x=x+2; return x * 24680; } }" },

          /* The ternary-in-condition family. `rule ternary` and `rule if_stmt` both
           * record a BLOCK on their test head, so when the ternary is LEFTMOST in an
           * enclosing condition the two joins collide on one key. Which enclosing
           * condition it is should not matter — plain, &&, ||, or a ternary on both
           * sides — so all four are pinned rather than the one shape that happened
           * to reproduce. */
          { "ternary in plain cond",
            "class T { int f(int x, int y, boolean b){ if ((b ? x : y) > 0) x=x+1;"
            " if ((b ? x : y) > 1) x=x+2; if ((b ? x : y) > 2) x=x+4;"
            " return x * 24680; } }" },
          { "ternary in || cond",
            "class T { int f(int x, int y, boolean b){ if ((b ? x : y) > 0 || x > 0) x=x+1;"
            " if ((b ? x : y) > 1 || x > 1) x=x+2; if ((b ? x : y) > 2 || x > 2) x=x+4;"
            " return x * 24680; } }" },
          { "ternary on both sides of &&",
            "class T { int f(int x, int y, boolean b){"
            " if ((b ? x : y) > 0 && (b ? y : x) > 0) x=x+1;"
            " if ((b ? x : y) > 1 && (b ? y : x) > 1) x=x+2;"
            " return x * 24680; } }" },
          { "ternary, long operands",
            "class T { int f(long x, boolean b){ x = x + (b ? 0-1L : 1L); x = x + (b ? 0-2L : 2L);"
            " x = x + (b ? 0-3L : 3L); return (int)x * 24680; } }" },
          /* A condition that is a bare FIELD read spills like any other complex
           * condition, so its join is keyed on the spill StoreLocal and has to
           * reach the Branch through the carry. Every spilled-condition fixture
           * here and at line 498 uses an ARITHMETIC spill; a GETFIELD spill is a
           * different key and was never covered. `ASCIIToBinaryBuffer.doubleValue`
           * is full of them (`if (isNegative)`, `overvalue ? … : …`). */
          { "if (field) x3",
            "class T { boolean b;"
            " int f(int x){ if (b) x=x+1; if (b) x=x+2; if (b) x=x+4;"
            " return x * 24680; } }" },
          { "ternary on field x3",
            "class T { boolean b;"
            " int f(int x){ x = x + (b ? 1 : 2); x = x + (b ? 3 : 4); x = x + (b ? 5 : 6);"
            " return x * 24680; } }" },
          /* `ASCIIToBinaryBuffer.doubleValue`'s actual shape: a LABELLED loop left by
           * several `break label` sites buried in nested ifs, with a tail after the
           * loop. The break target is the post-loop code; if a break cannot resolve
           * it as a br-depth, `transfer()` falls through and re-emits that tail
           * inline at the break site — once per break. The trace says the duplicated
           * node there is first emitted at sd=10 (deep, inside the loop) and only
           * then at sd=6 (its own level), which is exactly that inline re-emission.
           * Every loop fixture in this file is a bare `while`/`for` with no labelled
           * break, so this shape was never emitted here. */
          { "labelled loop, 3 breaks + tail",
            "class T { int f(int x, int y){ int r = 0;"
            " outer: while (r < y) {"
            "   if (x > 0) break outer;"
            "   if (x > 1) { if (x > 2) break outer; r = r + 1; continue; }"
            "   if (x > 3) break outer;"
            "   r = r + 2; }"
            " if (r > 0) r = r + 1;"
            " return r * 24680; } }" },
          /* The rows above are almost all `int`. Every §1 site is width-parametric,
           * and the method this table exists for (ASCIIToBinaryBuffer.doubleValue)
           * is float-heavy, so the same shapes are repeated at f32/f64/i64 — a
           * shared path that only ever ran at one width is a path with one width's
           * worth of evidence. */
          { "value && on doubles",
            "class T { int f(double a, double b, int x){"
            " boolean p = a > 0.0 && b > 0.0; boolean q = a > 1.0 && b > 1.0;"
            " boolean s = a > 2.0 && b > 2.0; if (p) x=x+1; if (q) x=x+2; if (s) x=x+4;"
            " return x * 24680; } }" },
          { "value || on floats",
            "class T { int f(float a, float b, int x){"
            " boolean p = a > 0.0f || b > 0.0f; boolean q = a > 1.0f || b > 1.0f;"
            " boolean s = a > 2.0f || b > 2.0f; if (p) x=x+1; if (q) x=x+2; if (s) x=x+4;"
            " return x * 24680; } }" },
          { "if (float cmp) x3",
            "class T { int f(float a, int x){ if (a > 0.0f) x=x+1; if (a > 1.0f) x=x+2;"
            " if (a > 2.0f) x=x+4; return x * 24680; } }" },
          { "ternary, float operands",
            "class T { int f(float a, boolean b, int x){ a = a + (b ? 0-1.0f : 1.0f);"
            " a = a + (b ? 0-2.0f : 2.0f); a = a + (b ? 0-3.0f : 3.0f);"
            " return (a > 0.0f ? 1 : 0) + x * 24680; } }" },
          /* The exact shape of the surviving duplication in
           * ASCIIToBinaryBuffer.doubleValue:301 — a ONE-ARMED if whose body is a
           * compound assignment to an INSTANCE FIELD. compound_field_instance
           * spills three temps and delivers through cg_deliver_loaded, so the if's
           * Ljoin is referenced twice: once as that delivery's continuation and
           * once as the if's own false edge. Every field fixture above uses a plain
           * assignment to a local, which has neither. */
          { "one-armed if, compound field",
            "class T { boolean neg; long bits;"
            " int f(long x, int r){ bits = x; if (neg) { bits |= 1L; }"
            " if (neg) { bits |= 2L; } if (neg) { bits |= 4L; }"
            " return (int) bits + r * 24680; } }" },
          { "one-armed if, compound array elem",
            "class T { boolean neg;"
            " int f(int[] v, int r){ if (neg) { v[0] |= 1; } if (neg) { v[1] |= 2; }"
            " if (neg) { v[2] |= 4; } return v[0] + r * 24680; } }" },
          { "guarded long rem",
            "class T { int f(long x, long y){ long a = x % y; long b = (x+1) % y;"
            " long c = (x+2) % y; return (int)(a+b+c) + (int)x * 24680; } }" },
          { "ternary, double operands",
            "class T { int f(double d, boolean b, int x){ d = d + (b ? 0-1.0 : 1.0);"
            " d = d + (b ? 0-2.0 : 2.0); d = d + (b ? 0-3.0 : 3.0);"
            " return (d > 0.0 ? 1 : 0) + x * 24680; } }" },

          /* §14.9 SWITCH FALL-THROUGH. Of the four ways out of a case group only
           * fall-through carries no jump: break/continue/return each become a `br`
           * the backend resolves by scope depth, while falling out of one group into
           * the next is pure layout adjacency on the chain the frontend built. So the
           * groups have to be laid out in SOURCE order — the ascending-by-value order
           * the br_table wants is a different question — and each group head has to be
           * the SAME node the br_table names, or the fall-through edge points at the
           * raw head while the table points at a label wrapping it, and the group is
           * emitted twice. Every existing switch fixture breaks out of every group,
           * which is exactly the shape that hides this. */
          { "switch fall-through, ascending",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: r = 1; case 2: r = r + 2; break; case 3: r = 3; }"
            " return r + x * 24680; } }" },
          { "switch fall-through, descending source order",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 45: r = 1; case 43: r = r + 2; break; }"
            " return r + x * 24680; } }" },
          { "switch, default not last",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: r = 1; break; default: r = 9; case 2: r = r + 2; break; }"
            " return r + x * 24680; } }" },
          { "switch fall-through into a group that also brs out",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: r = 1; case 2: if (r > 0) break; r = 5; case 3: r = r + 8; }"
            " return r + x * 24680; } }" },
          /* …and the shape the fixtures above still cannot reach: a group whose ONLY statement
           * is `break`. §14.9's four exits again — break carries a jump, so an empty group's
           * jump is its whole body, and the frontend elides a Goto whose target is the next node
           * on the chain. The case target is then the switch EXIT itself. An exit is not a group:
           * laying one out gives it a block and emits the switch's continuation as if it were a
           * case body, and the continuation is emitted a second time when the switch finishes.
           * Needs a LATER group, because as the last group the exit and the fall-out coincide. */
          { "switch, a case whose only statement is break, with a later group",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: break; default: r = 9; }"
            " return r + x * 24680; } }" },
          { "switch, empty break case before a case group",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: break; case 2: r = 2; break; default: r = 9; }"
            " return r + x * 24680; } }" },
          /* The rest of the (body × exit × position × default) cross-product for an EMPTY group,
           * because the axis above was the one 16 fixtures never varied. `default: break;` gets
           * its own cell: the default's own target is then the exit, so the br_table's default
           * depth cannot be the default group's index — there is no such group. */
          { "switch, default: break; with a real case group",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: r = 1; break; default: break; }"
            " return r + x * 24680; } }" },
          { "switch, empty break case and NO default",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: break; case 2: r = 2; }"
            " return r + x * 24680; } }" },
          { "switch, EMPTY group falling through to the next (two labels, one body)",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: case 2: r = 2; break; default: r = 9; }"
            " return r + x * 24680; } }" },
          { "switch, empty group LAST (label with no body, falls out)",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { default: r = 9; break; case 3: }"
            " return r + x * 24680; } }" },
          { "switch, group exits by RETURN not break",
            "class T { int f(int x){"
            " switch (x) { case 1: return 1; default: break; }"
            " return x * 24680; } }" },
          { "switch in a loop, group exits by CONTINUE",
            "class T { int f(int x){ int r = 0;"
            " for (int i = 0; i < 3; i++) { switch (i) { case 1: continue; default: r = r + i; } r = r + 10; }"
            " return r + x * 24680; } }" },
          /* The remaining jump targets an empty group can name: a labelled block's exit, a
           * labelled loop's continue, and an inner switch's exit. Each is an ENCLOSING scope
           * rather than this switch's own, which is the half `lbreak`-by-identity cannot see. */
          { "switch, empty group exits by break to a LABELLED BLOCK",
            "class T { int f(int x){ int r = 0;"
            " L: { switch (x) { case 1: break L; default: r = 9; } r = r + 1; }"
            " return r + x * 24680; } }" },
          { "switch, empty group exits by LABELLED CONTINUE",
            "class T { int f(int x){ int r = 0;"
            " L: for (int i = 0; i < 3; i++) { switch (i) { case 1: continue L; default: r = r + i; } r = r + 10; }"
            " return r + x * 24680; } }" },
          { "NESTED switch, inner group is an empty break",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { case 1: switch (x + 1) { case 2: break; default: r = 5; } r = r + 3; break;"
            "              default: r = 9; }"
            " return r + x * 24680; } }" },
          { "switch, empty break group LAST with default before it",
            "class T { int f(int x){ int r = 0;"
            " switch (x) { default: r = 9; break; case 3: break; }"
            " return r + x * 24680; } }" },
          /* The no-`default:` landing is the one framing site with no already-a-scope guard: with
           * no default label the switch's default target IS its exit, so when that exit is an
           * INHERITED scope (the switch is the last statement of an enclosing region) the landing
           * frames a node that is already framed, and a `break` resolves to the inner copy. Needs
           * all three at once — no default, a break, and an inherited exit. */
          { "switch with NO default, a break, and an INHERITED exit (last stmt of a loop body)",
            "class T { int f(int x){ int r = 0;"
            " for (int i = 0; i < 3; i++) { switch (i) { case 1: break; case 2: r = r + 2; } }"
            " return r + x * 24680; } }" },
          { "switch with NO default and an inherited exit, inside a labelled block",
            "class T { int f(int x){ int r = 0;"
            " L: { switch (x) { case 1: break; case 2: r = r + 2; } }"
            " return r + x * 24680; } }" },

          /* §14.6 a labelled BLOCK. The frontend's ρ frame tells `break L` which node
           * to transfer to; the backend needs a scope record for the same label or it
           * frames nothing, no br-depth resolves, and every break emits the exit's
           * code inline. Two breaks, two copies of the method tail. */
          { "labelled block, two breaks",
            "class T { int f(int x){ int r = 0;"
            " L: { if (x == 0) break L; r = 1; if (x == 1) break L; r = 2; }"
            " return r + x * 24680; } }" },
          { "labelled block round a try, break out",
            "class T { int f(int x){ int r = 0;"
            " L: try { if (x == 0) break L; r = 2; } catch (RuntimeException e) { }"
            " return r + x * 24680; } }" },

          /* §14.14 the CONTINUE target — a for's update, a do-while's tail test — is
           * reached by the body's fall-through AND by every continue, so it is a label
           * and must be framed. It is one only when a continue exists; without one it
           * has a single reference and the paper emits no code for an unreferenced
           * label, so a frame round every for-update would be pure loss. */
          { "for with continue",
            "class T { int f(int x){ int s = 0;"
            " for (int i = 0; i < x; i++) { if (i == 2) continue; s = s + i; }"
            " return s + x * 24680; } }" },
          { "for with continue and break",
            "class T { int f(int x){ int s = 0;"
            " for (int i = 0; i < x; i++) { if (i == 2) continue; if (i == 4) break; s = s + i; }"
            " return s + x * 24680; } }" },
          { "do-while with continue",
            "class T { int f(int x){ int s = 0, i = 0;"
            " do { i++; if (i == 2) continue; s = s + i; } while (i < x);"
            " return s + x * 24680; } }" },
          { "nested for, continue in the inner",
            "class T { int f(int x){ int s = 0;"
            " for (int i = 0; i < x; i++) { for (int j = 0; j < x; j++) {"
            "   if (j == 1) continue; s = s + j; } }"
            " return s + x * 24680; } }" },

          /* §15.24 in a BOOLEAN-CONTROL position: the conditional inherits γ = pair,
           * so each arm ends in its own branch to the shared Lt/Lf. Those two shared
           * destinations are labels like any other (Fig. 7's Lf/Lt) and get records. */
          { "ternary IS the while condition",
            "class T { int f(int x, boolean p, boolean q, boolean b){ int n = 0;"
            " while (b ? p : q) { n = n + 1; if (n > 2) p = false; }"
            " return n + x * 24680; } }" },
          { "ternary under ! in an if condition",
            "class T { int f(int x, boolean p, boolean q, boolean b){ int r = 0;"
            " if (!(b ? p : q)) r = 1; return r + x * 24680; } }" },
          { "ternary either side of ||",
            "class T { int f(int x, boolean p, boolean q, boolean b){ int r = 0;"
            " if ((b ? p : q) || (b ? q : p)) r = 1; return r + x * 24680; } }" },

          /* Fig. 7's shared exit in a ONE-ARMED if: the `&&` chain's Lf IS the if's
           * join, so one node carries a MERGE row and a BLOCK row naming it. Collapse
           * them to one label and the head looks like it carries only the plain-if
           * row — then the compound condition emits as a native `if` whose then-arm
           * is the rest of the CONDITION. */
          { "one-armed if (a && (b || c))",
            "class T { int f(int x){ int r = 0;"
            " if (x > 0 && (x > 5 || x < 3)) r = 1; if (r == 0) r = 9;"
            " return r + x * 24680; } }" },
          { "one-armed if (a || (b && c))",
            "class T { int f(int x){ int r = 0;"
            " if (x > 0 || (x > 5 && x < 3)) r = 1; if (r == 0) r = 9;"
            " return r + x * 24680; } }" },

          /* The breakable-block idiom the generated PEG parsers use. */
          { "do-while(false) breakable block in for(;;)",
            "class T { int f(int x){ int acc = 0, i = 0;"
            " for (;;) { int m = i; boolean ok = false;"
            "   do { if (i >= x) break; i++; if (i == 3) break; acc = acc + i; ok = true; }"
            "   while (false);"
            "   if (!ok) { i = m; break; } }"
            " return acc + i + x * 24680; } }" },
        };
        uint8_t mark[8]; int ml = 0;                 /* i32.const 24680, real encoder */
        { emit_wasm_ctx m = {0};
          ew_emit(&m, WOP_I32_CONST); ew_i32(&m, 24680);
          ml = (int)bbq_vec_len(m.code); memcpy(mark, m.code, (size_t)ml);
          bbq_vec_free(m.code); }
        for (size_t k = 0; k < sizeof sites / sizeof sites[0]; k++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            const uint8_t* body = NULL;
            int n = emit_body(&a, sites[k].src, "f", &body);
            int seen = 0;
            for (int i = 0; n > 0 && i + ml <= n; i++)
                if (memcmp(body + i, mark, (size_t)ml) == 0) seen++;
            if (n <= 0 || seen != 1 || emit_body_error)
                printf("        %-24s body=%d bytes, tail marker ×%d (want ×1)%s%s\n",
                       sites[k].name, n, seen,
                       emit_body_error ? " — backend: " : "",
                       emit_body_error ? emit_body_error : "");
            CHECK(n > 0 && seen == 1 && !emit_body_error, sites[k].name);
            bbq_arena_free(&a);
        }
    }

    /* ── a VALUE-context ternary keeps its join: the tail is emitted ONCE ───────
     * The paper has no ternary: a conditional delivering to a location is Figure 5's
     * two-armed if with δ = that location (p.12's `E_bool ⇒ if E_bool (int 1)
     * (int 0)`), and both arms are `CG_store O A L` (p.13) — same location, same L.
     * L is a merge; the frontend records it (test_scope_sidecar pins 1 BLOCK for
     * this shape). If the backend cannot find that record, `ljoin` falls back to the
     * region end and each arm absorbs the tail — and at method top level the region
     * end is the whole method, so k ternaries cost 2^k.
     *
     * Same oracle as the spilled-condition pin above: the tail's own emission count
     * in this method, not a module size. `ASCIIToBinaryBuffer.doubleValue` is the
     * real instance (`ieeeBits += overvalue ? -1 : 1;`), duplicating in the shipped
     * prelude. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a,
            "class T { int f(int x, boolean b){"
            "  x = x + (b ? 0 - 1 : 1);"
            "  x = x + (b ? 0 - 2 : 2);"
            "  x = x + (b ? 0 - 3 : 3);"
            "  return x * 54321; } }", "f", &body);
        CHECK(n > 0, "value ternaries: emitted");
        uint8_t mark[8]; int ml = 0;                 /* i32.const 54321, real encoder */
        { emit_wasm_ctx m = {0};
          ew_emit(&m, WOP_I32_CONST); ew_i32(&m, 54321);
          ml = (int)bbq_vec_len(m.code); memcpy(mark, m.code, (size_t)ml);
          bbq_vec_free(m.code); }
        int seen = 0;
        for (int i = 0; i + ml <= n; i++)
            if (memcmp(body + i, mark, (size_t)ml) == 0) seen++;
        if (seen != 1)
            printf("        tail marker emitted %d times (want 1; 8 = the 2^3 join loss)\n", seen);
        CHECK(seen == 1, "a value ternary keeps its join: the tail is emitted ONCE");
        bbq_arena_free(&a);
    }

    /* ── a value ternary whose condition SPILLS ────────────────────────────────
     * The two halves above pass on their own: a spilled condition keeps its join
     * (07-27's carry), and a value ternary keeps its join. `doubleValue` is both at
     * once — `ieeeBits += overvalue ? -1 : 1;` reads a FIELD, so the condition
     * spills and the record is keyed on the spill StoreLocal, while the construct is
     * a ternary rather than an if statement. The carry is cleared by the first
     * Branch that arrives ("this branch ends the condition it belongs to"), which
     * holds for an if and is what this fixture tests for a ternary. Same tail-count
     * oracle. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL;
        int n = emit_body(&a,
            "class T { int f(int x, int y){"
            "  x = x + ((x + y) * 3 != 1 ? 0 - 1 : 1);"
            "  x = x + ((x + y) * 5 != 2 ? 0 - 2 : 2);"
            "  x = x + ((x + y) * 7 != 3 ? 0 - 3 : 3);"
            "  return x * 24680; } }", "f", &body);
        CHECK(n > 0, "spilled-condition ternaries: emitted");
        uint8_t mark[8]; int ml = 0;
        { emit_wasm_ctx m = {0};
          ew_emit(&m, WOP_I32_CONST); ew_i32(&m, 24680);
          ml = (int)bbq_vec_len(m.code); memcpy(mark, m.code, (size_t)ml);
          bbq_vec_free(m.code); }
        int seen = 0;
        for (int i = 0; i + ml <= n; i++)
            if (memcmp(body + i, mark, (size_t)ml) == 0) seen++;
        if (seen != 1)
            printf("        tail marker emitted %d times (want 1; 8 = the 2^3 join loss)\n", seen);
        CHECK(seen == 1, "a spilled-condition ternary keeps its join: the tail is emitted ONCE");
        bbq_arena_free(&a);
    }

    /* ── A nested loop is framed however DEEP the scope stack gets ─────────────
     *
     * The scope stack used to be a fixed 64-entry C array. Past it, pushes were
     * skipped silently: br_depth could no longer find the target, so control
     * that should have become a `br` re-emitted the target region inline. For a
     * loop that means the `loop` framing is never emitted and the back edge is
     * gone — the body runs once and falls through. nbody's energy() did exactly
     * that, summing only the first pair of each outer iteration.
     *
     * Depth is not driven by source nesting: every array access contributes a
     * bounds-check branch whose merge label frames a block, so the trigger is
     * how much work the outer body does. Both fixtures below are the same two
     * loops; only the size of the statement ahead of the inner loop differs, and
     * both must emit two `loop void` framings. */
    {
        static const uint8_t LOOP_VOID[] = { 0x03, 0x40 };
        struct { const char* src; const char* what; } lf[] = {
          { "class B { double x,y,z,vx,vy,vz,mass; }"
            "class T { static double f(B[] a){ double e=0;"
            "  for (int i=0;i<a.length;++i){ e += 1.0;"
            "    for (int j=i+1;j<a.length;++j) e -= a[j].mass; }"
            "  return e; } }", "shallow outer body" },
          { "class B { double x,y,z,vx,vy,vz,mass; }"
            "class T { static double f(B[] a){ double dx,dy,dz,d; double e=0;"
            "  for (int i=0;i<a.length;++i){"
            "    e += 0.5*a[i].mass*( a[i].vx*a[i].vx + a[i].vy*a[i].vy"
            "                       + a[i].vz*a[i].vz );"
            "    for (int j=i+1;j<a.length;++j){"
            "      dx=a[i].x-a[j].x; dy=a[i].y-a[j].y; dz=a[i].z-a[j].z;"
            "      d=Math.sqrt(dx*dx+dy*dy+dz*dz);"
            "      e -= (a[i].mass*a[j].mass)/d; } }"
            "  return e; } }", "deep outer body (energy()'s shape)" },
        };
        for (int k = 0; k < 2; k++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            const uint8_t* body = NULL;
            int n = emit_body(&a, lf[k].src, "f", &body);
            int loops = 0;
            for (int i = 0; body && i + 2 <= n; i++)
                if (memcmp(body + i, LOOP_VOID, 2) == 0) loops++;
            char lbl[128];
            snprintf(lbl, sizeof lbl,
                     "%s: both loops are framed (got %d `loop void`)", lf[k].what, loops);
            CHECK(n > 0 && loops == 2, lbl);
            bbq_arena_free(&a);
        }
    }

    return TEST_SUMMARY("test_codegen_structured");
}
