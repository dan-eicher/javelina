// test_control_audit.c — SYSTEMATIC control-flow audit. Enumerates every control
// construct in the positions where destination-inheritance bugs live (tail of a
// loop, nested, in each if-arm / switch-case / try-handler) and runs a mechanical
// redundancy oracle over the emitted body. This exists because eyeballing one case
// when challenged is not an audit: the nested-loop $ibreak, the switch $break
// trampoline, and the if-else duplicated back-edge were all VALID-but-redundant
// (a module validator passes them) and all in tail position — so the oracle looks
// for the byte signatures redundant control leaves, across the whole matrix.
//
// Oracle flags (each with scope-kind context, so a loop back-edge is NOT confused
// with a redundant forward br):
//   E1 empty scope        — block/loop/if opened and immediately ended.
//   E2 trampoline br       — unconditional `br N` to a BLOCK whose matching `end`
//                            is the very next thing (control would fall through).
//   E3 unreachable          — code after an unconditional transfer (br/br_table/
//                            return/unreachable/throw) before the scope's end/else.
//   E4 double eqz          — `i32.eqz; i32.eqz` (a `!` that wasn't folded).
//   E5 unbalanced          — scope stack not empty / underflowed at body end.
//   E?  unknown opcode      — the disassembler can't length-decode it: a COVERAGE
//                            hole to fix, reported (never silently misparsed).
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/codegen_method.h"
#include "javelina/compiler/wasm_types.h"
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
static int emit_body(bbq_arena* a, const char* src, const uint8_t** out) {
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
        if (!methods[i]->name || strcmp(methods[i]->name, "f")) continue;
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(&cctx, i, &nsc);
        static wasm_types_t wt; wasm_types_build(&wt, &sctx);
        static burg_ctx_t bc; bc = (burg_ctx_t){0}; burg_ctx_init(&bc); bc.types = &wt;
        codegen_method_structured(methods[i], sc, nsc, &bc);
        *out = bc.emit.code;
        return (int)bbq_vec_len(bc.emit.code);
    }
    *out = NULL; return -1;
}

/* uleb / sleb advance (return value unused; just move `*i`). */
static void skip_uleb(const uint8_t* b, int n, int* i) { while (*i<n && (b[*i]&0x80)) (*i)++; if (*i<n) (*i)++; }
static void skip_sleb(const uint8_t* b, int n, int* i) { while (*i<n && (b[*i]&0x80)) (*i)++; if (*i<n) (*i)++; }

/* Disassemble one instruction at *i, advancing past its operands. Pushes/pops the
 * scope-kind stack (0=block,1=loop,2=if,3=try_table). Returns the opcode, or -1 on
 * an opcode whose length is unknown (reported by the caller). */
static int step(const uint8_t* b, int n, int* i, uint8_t* kinds, int* sd, const char** flag) {
    int op = b[(*i)++];
    switch (op) {
      case 0x00: case 0x01: break;                          /* unreachable / nop      */
      case 0x02: case 0x03: case 0x04:                      /* block / loop / if      */
        if (*i<n) {                                         /* blocktype: 0x40 void / valtype / (ref null ht) */
          uint8_t bt = b[*i];
          if (bt==0x63 || bt==0x64) { (*i)++; skip_sleb(b,n,i); } /* (ref null ht)/(ref ht): prefix + heaptype */
          else (*i)++;                                      /* 0x40 void or a single-byte valtype */
        }
        kinds[(*sd)++] = (op==0x02)?0:(op==0x03)?1:2; break;
      /* 0x1F try_table is intercepted in audit() before step() is called. */
      case 0x05: break;                                     /* else                   */
      case 0x0B: if (*sd>0) (*sd)--; else *flag="E5 end underflow"; break;  /* end    */
      case 0x0C: case 0x0D: skip_uleb(b,n,i); break;        /* br / br_if             */
      case 0x0E: { int c=0,sh=0;                            /* br_table: count+1 targets */
                   while(*i<n){ c|=(b[*i]&0x7F)<<sh; int more=b[*i]&0x80; (*i)++; if(!more)break; sh+=7; }
                   for(int k=0;k<c+1;k++) skip_uleb(b,n,i); } break;
      case 0x0F: break;                                     /* return                 */
      case 0x08: skip_uleb(b,n,i); break;                   /* throw <tag>            */
      case 0x0A: break;                                     /* throw_ref              */
      case 0x10: case 0x12: case 0x13: case 0x14:           /* call / *_call / call_ref*/
        skip_uleb(b,n,i); break;
      case 0x1A: case 0x1B: break;                          /* drop / select          */
      case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:/* local / global get/set */
        skip_uleb(b,n,i); break;
      case 0x41: case 0x42: skip_sleb(b,n,i); break;        /* i32/i64.const          */
      case 0x43: *i+=4; break;                              /* f32.const              */
      case 0x44: *i+=8; break;                              /* f64.const              */
      case 0xD0: skip_sleb(b,n,i); break;                   /* ref.null <heaptype>    */
      case 0xD1: break;                                     /* ref.is_null            */
      case 0xD2: skip_uleb(b,n,i); break;                   /* ref.func               */
      case 0xFB: {                                          /* GC prefix              */
        int sub; { int c=0,sh=0; while(*i<n){ c|=(b[*i]&0x7F)<<sh; int more=b[*i]&0x80; (*i)++; if(!more)break; sh+=7;} sub=c; }
        switch (sub) {
          case 0: case 1: skip_uleb(b,n,i); break;                 /* struct.new(_default) ty */
          case 2: case 3: case 4: case 5: skip_uleb(b,n,i); skip_uleb(b,n,i); break; /* struct get/set ty fld */
          case 6: case 7: case 8: skip_uleb(b,n,i); break;         /* array.new_ ty           */
          case 11: case 12: case 13: case 14: skip_uleb(b,n,i); break; /* array get/set ty    */
          case 15: break;                                          /* array.len               */
          case 20: case 21: case 22: case 23: skip_sleb(b,n,i); break; /* ref.test/test_null/cast/cast_null heaptype */
          default: *flag = "E? unknown FB subop"; return -1;
        } break;
      }
      default:
        if (op>=0x45 && op<=0xC4) break;                    /* eqz/cmp/arith/convert: no operand */
        *flag = "E? unknown opcode"; return -1;
    }
    return op;
}

/* Run the oracle over one body. Returns a flag string, or NULL if clean. */
static const char* audit(const uint8_t* b, int n) {
    uint8_t kinds[128]; int sd = 0;
    const char* flag = NULL;
    int prev_op = -1, prev_was_block_open = 0; uint8_t prev_block_kind = 0;
    int prev_unconditional = 0, last_br_depth = -1;
    int i = 0;
    while (i < n) {
        int start = i;
        /* try_table needs a clean catch-vector skip (its inline decode above is
         * delicate); handle it here so the scope push is reliable. */
        if (b[i] == 0x1F) {
            i++; if (i<n) i++;                       /* opcode + blocktype */
            int c=0,sh=0; while(i<n){ c|=(b[i]&0x7F)<<sh; int more=b[i]&0x80; i++; if(!more)break; sh+=7; }
            for (int k=0;k<c;k++){ int kind=b[i++]; if(kind<=1){skip_uleb(b,n,&i);skip_uleb(b,n,&i);} else skip_uleb(b,n,&i); }
            kinds[sd++] = 0;                          /* a try_table is a block-like target */
            prev_op = 0x1F; prev_was_block_open = 0; prev_unconditional = 0; continue;
        }
        uint8_t before_kinds_top = sd>0?kinds[sd-1]:255; int before_sd = sd;
        int op = step(b, n, &i, kinds, &sd, &flag);
        if (op < 0 && op != -2) { static char buf[64]; snprintf(buf,sizeof buf,"%s @%d (0x%02X)",flag?flag:"E?",start,b[start]); return buf; }

        /* E1 empty scope: an `end` that closes a scope opened by the immediately
         * preceding instruction (block/loop/if with nothing inside). */
        if (op == 0x0B && prev_was_block_open) { static char buf[64]; snprintf(buf,sizeof buf,"E1 empty scope @%d",start); return buf; }
        /* E2 trampoline br: `br 0` (unconditional, to the INNERMOST scope) when
         * that scope is a BLOCK whose `end` is the very next instr — the br
         * targets the block being closed, so control reaches the same place by
         * falling through: redundant. A `br N>0` (a switch case br'ing out over
         * sibling blocks, or a try's br over its handler) is NOT this. */
        if (op == 0x0B && prev_op == 0x0C && last_br_depth == 0 && before_kinds_top == 0) {
            static char buf[80]; snprintf(buf,sizeof buf,"E2 br0-then-end (trampoline block) @%d",start); return buf;
        }
        /* E3 unreachable: a non-end/else instruction right after an unconditional transfer. */
        if (prev_unconditional && op != 0x0B && op != 0x05) {
            static char buf[64]; snprintf(buf,sizeof buf,"E3 unreachable @%d (0x%02X)",start,op); return buf;
        }
        /* E4 double eqz. */
        if (op == 0x45 && prev_op == 0x45) { static char buf[48]; snprintf(buf,sizeof buf,"E4 double eqz @%d",start); return buf; }

        if (op == 0x0C) { int d=0,sh=0,p=start+1;       /* decode br depth */
            while(p<n){ d|=(b[p]&0x7F)<<sh; if(!(b[p]&0x80))break; p++; sh+=7; } last_br_depth=d; }
        prev_was_block_open = (op==0x02||op==0x03||op==0x04);
        prev_block_kind = before_kinds_top; (void)prev_block_kind; (void)before_sd;
        prev_unconditional = (op==0x0C||op==0x0E||op==0x0F||op==0x00||op==0x08||op==0x0A);
        prev_op = op;
    }
    if (sd != 0) return "E5 unbalanced scope stack";
    return NULL;
}

int main(void) {
    struct { const char* name; const char* src; } cases[] = {
      /* ── loops in loop-tail (back-edge inheritance) ── */
      {"while_in_while",   "class T{void f(int x){ while(x>0){ while(x>1){ x=x-1; } } }}"},
      {"for_in_while",     "class T{void f(int x){ while(x>0){ for(int i=0;i<x;i=i+1){ x=x-1; } } }}"},
      {"while_in_for",     "class T{void f(int x){ for(int i=0;i<x;i=i+1){ while(x>1){ x=x-1; } } }}"},
      {"for_in_for",       "class T{void f(int x){ for(int i=0;i<x;i=i+1){ for(int j=0;j<x;j=j+1){ x=x-1; } } }}"},
      {"dowhile_in_while", "class T{void f(int x){ while(x>0){ do { x=x-1; } while(x>5); } }}"},
      {"while_in_dowhile", "class T{void f(int x){ do { while(x>1){ x=x-1; } } while(x>0); }}"},
      /* ── if in loop-tail ── */
      {"ifelse_in_while",  "class T{void f(int x){ while(x>0){ if(x>1){ x=1; } else { x=2; } } }}"},
      {"ifnoelse_in_while","class T{void f(int x){ while(x>0){ if(x>1){ x=1; } } }}"},
      {"ifelse_in_for",    "class T{void f(int x){ for(int i=0;i<x;i=i+1){ if(x>1){ x=1; } else { x=2; } } }}"},
      {"if_in_dowhile",    "class T{void f(int x){ do { if(x>1){ x=1; } else { x=2; } } while(x>0); }}"},
      /* ── switch in loop-tail ── */
      {"switch_in_while",  "class T{void f(int x){ while(x>0){ switch(x){ case 0: x=1; break; case 1: x=2; break; default: x=x-1; } } }}"},
      {"switch_in_for",    "class T{void f(int x){ for(int i=0;i<x;i=i+1){ switch(x){ case 0: x=1; break; default: x=x-1; } } }}"},
      {"switch_nodef_in_while","class T{void f(int x){ while(x>0){ switch(x){ case 0: x=1; break; case 1: x=x-1; break; } } }}"},
      /* ── constructs in if-arms ── */
      {"while_in_then",    "class T{void f(int x){ if(x>0){ while(x>1){ x=x-1; } } }}"},
      {"while_in_else",    "class T{void f(int x){ if(x>0){ x=1; } else { while(x>1){ x=x-1; } } }}"},
      {"switch_in_then",   "class T{void f(int x){ if(x>0){ switch(x){ case 0: x=1; break; default: x=2; } } }}"},
      {"ifelse_in_then",   "class T{void f(int x){ if(x>0){ if(x>1){ x=1; } else { x=2; } } }}"},
      {"ifelse_in_else",   "class T{void f(int x){ if(x>1){ x=1; } else { if(x>2){ x=2; } else { x=3; } } }}"},
      /* ── constructs in switch-case ── */
      {"if_in_case",       "class T{void f(int x){ switch(x){ case 0: if(x>1){ x=1; } break; default: x=2; } }}"},
      {"while_in_case",    "class T{void f(int x){ switch(x){ case 0: while(x>1){ x=x-1; } break; default: x=2; } }}"},
      /* ── break / continue / labeled ── */
      {"break_in_if_in_while",   "class T{void f(int x){ while(x>0){ if(x>1){ break; } x=x-1; } }}"},
      {"continue_in_if_in_while","class T{void f(int x){ while(x>0){ if(x>1){ x=x-1; continue; } x=x-2; } }}"},
      {"labeled_break",    "class T{void f(int x){ outer: while(x>0){ while(x>1){ if(x>5){ break outer; } x=x-1; } } }}"},
      {"labeled_continue", "class T{void f(int x){ outer: while(x>0){ while(x>1){ if(x>5){ continue outer; } x=x-1; } } }}"},
      /* ── exceptions ── */
      {"trycatch_flat",    "class T{void f(int x){ try { x=x-1; } catch(Throwable e){ x=2; } }}"},
      {"trycatch_in_while","class T{void f(int x){ while(x>0){ try { x=x-1; } catch(Throwable e){ x=2; } } }}"},
      {"if_in_try",        "class T{void f(int x){ try { if(x>0){ x=1; } else { x=2; } } catch(Throwable e){ x=3; } }}"},
      {"while_in_catch",   "class T{void f(int x){ try { x=1; } catch(Throwable e){ while(x>1){ x=x-1; } } }}"},
    };
    int N = (int)(sizeof cases / sizeof cases[0]);
    int dirty = 0;
    for (int c = 0; c < N; c++) {
        printf("  .....  %-22s\r", cases[c].name); fflush(stdout);
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        const uint8_t* body = NULL; int n = emit_body(&a, cases[c].src, &body);
        const char* flag = (n > 0) ? audit(body, n) : "compile failed";
        CHECK(flag == NULL, cases[c].name);
        if (flag) {
            dirty++;
            printf("  DIRTY  %-22s %s\n    ", cases[c].name, flag);
            for (int i=0;i<n;i++) printf("%02X ", body[i]); printf("\n");
        } else {
            printf("  clean  %-22s (%d bytes)\n", cases[c].name, n);
        }
        bbq_arena_free(&a);
    }
    printf("control-flow audit: %d/%d clean\n", N - dirty, N);

    /* Linearity property (docs/ddcg-merge-labels.md §4.3). The paper's model emits
     * each label once, so code size is LINEAR in AST size — an else-if-&& chain of
     * depth k must grow by a CONSTANT per level, never doubling. This is the
     * standing guard against reintroducing the shared-continuation duplication
     * (short-circuit exits AND cast/guard diamond tails). Each level is structurally
     * identical (one instanceof-&& test + one checked cast + one call), so a constant
     * per-level delta ⟺ every shared label is emitted once. */
    {
        const char* ty[] = {"int","long","char","byte","short","float","double","boolean"};
        int sizes[9] = {0};
        for (int k = 2; k <= 8; k++) {
            char src[8192]; int p = 0;
            p += snprintf(src+p, sizeof src-p, "class T{ static int g;");
            for (int i = 0; i < k; i++)
                p += snprintf(src+p, sizeof src-p, " static void s%d(%s[] q){}", i, ty[i]);
            p += snprintf(src+p, sizeof src-p, " void f(Object a, Object b){ ");
            for (int i = 0; i < k; i++)
                p += snprintf(src+p, sizeof src-p,
                    "%sif (a instanceof %s[] && b instanceof %s[]) s%d((%s[])a); ",
                    i ? "else " : "", ty[i], ty[i], i, ty[i]);
            p += snprintf(src+p, sizeof src-p, "else g=0; } }");
            bbq_arena a; bbq_arena_init(&a, 1 << 20);
            const uint8_t* body = NULL; int n = emit_body(&a, src, &body);
            sizes[k] = n;
            bbq_arena_free(&a);
        }
        /* Per-level deltas; assert the largest is within 1.5x the smallest (linear).
         * Exponential duplication makes the top delta ~2x its predecessor. */
        int dmin = 1<<30, dmax = 0;
        for (int k = 3; k <= 8; k++) {
            int d = sizes[k] - sizes[k-1];
            if (d < dmin) dmin = d;
            if (d > dmax) dmax = d;
        }
        printf("  linearity: depth deltas");
        for (int k = 3; k <= 8; k++) printf(" %d", sizes[k]-sizes[k-1]);
        printf("  (min %d, max %d)\n", dmin, dmax);
        CHECK(dmax <= dmin * 3 / 2, "else-if-&&-cast chain is linear (constant per-level cost)");
        if (dmax > dmin * 3 / 2) {
            printf("  DIRTY  else-if-&&-cast chain NOT linear (max delta %d >> min %d) "
                   "— a shared label is re-emitted; see docs/ddcg-merge-labels.md §1\n", dmax, dmin);
        } else {
            printf("  clean  else-if-&&-cast chain is linear (constant per-level cost)\n");
        }
    }

    return TEST_SUMMARY("test_control_audit");
}
