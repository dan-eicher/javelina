// test_typecheck.c — the §7.6 type-checking validator (jav_typecheck): proves it
// ACCEPTS well-typed bodies and REJECTS the ill-typed / malformed ones the old
// height-only validator waved through. This is the security boundary; the negative
// cases are the point. Validation only (no execution) — so it can exercise opcodes
// the runtime doesn't implement yet (unreachable).
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;

static void expect(const char* nm, int want_ok, const uint8_t* code, size_t n,
                   const jav_vctx_t* cx){
    jav_st_entry_t* st = NULL; unsigned nst = 0;
    int got = jav_typecheck(code, n, cx, &st, &nst);
    int ok = (got == want_ok);
    printf("  %-38s got=%s want=%s [%s]\n", nm, got?"accept":"reject",
           want_ok?"accept":"reject", ok?"PASS":"FAIL");
    if (got) bbq_vec_free(st);
    fails += !ok;
}

int main(void){
    static const jav_valtype_t two_i32[] = { WVT_I32, WVT_I32 };
    static const jav_valtype_t one_i32[] = { WVT_I32 };
    static const jav_valtype_t one_i64[] = { WVT_I64 };

    /* ── ACCEPT: well-typed local.get 0; local.get 1; i32.add; end ── */
    { jav_vctx_t cx = {0}; cx.locals=two_i32; cx.nlocals=2; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x20,0x00, 0x20,0x01, 0x6a, 0x0b };
      expect("accept: add(i32,i32)->i32", 1, c,sizeof c, &cx); }

    /* ── REJECT: TYPE CONFUSION — i32.add fed an f32 (the height-only hole) ── */
    { jav_vctx_t cx = {0}; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x41,0x01, 0x43,0,0,0x80,0x3f, 0x6a, 0x0b }; /* i32.const 1; f32.const 1.0; i32.add */
      expect("reject: i32.add on f32 (type conf)", 0, c,sizeof c, &cx); }

    /* ── REJECT: stack underflow (i32.add with nothing on the stack) ── */
    { jav_vctx_t cx = {0}; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x6a, 0x0b };
      expect("reject: i32.add underflow", 0, c,sizeof c, &cx); }

    /* ── REJECT: unknown opcode (0x1a drop — not in this engine's set) ── */
    { jav_vctx_t cx = {0};
      static const uint8_t c[]={ 0x1a, 0x0b };
      expect("reject: unknown opcode 0x1a", 0, c,sizeof c, &cx); }

    /* ── REJECT: local index out of range (local.get 5, only 2 locals) ── */
    { jav_vctx_t cx = {0}; cx.locals=two_i32; cx.nlocals=2; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x20,0x05, 0x0b };
      expect("reject: local.get out of range", 0, c,sizeof c, &cx); }

    /* ── REJECT: global index out of range (global.get 3, only 1 global) ── */
    { jav_vctx_t cx = {0}; static const jav_valtype_t g[]={WVT_I32};
      cx.globals=g; cx.nglobals=1; cx.results=one_i64; cx.nresults=1;
      static const uint8_t c[]={ 0x23,0x03, 0x0b };
      expect("reject: global.get out of range", 0, c,sizeof c, &cx); }

    /* ── REJECT: unbalanced control (no terminating end) ── */
    { jav_vctx_t cx = {0}; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x41,0x01 };   /* i32.const 1, no end */
      expect("reject: missing end", 0, c,sizeof c, &cx); }

    /* ── ACCEPT: dead code polymorphically satisfies the result (unreachable; end) ── */
    { jav_vctx_t cx = {0}; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x00, 0x0b };   /* unreachable; end */
      expect("accept: unreachable; end ->i32", 1, c,sizeof c, &cx); }

    /* ── REJECT: spec's dead-code invalid (unreachable; i32.const; i64.add) ──
     * even unreachable, the concrete i32 from i32.const is popped by i64.add. */
    { jav_vctx_t cx = {0}; cx.results=one_i64; cx.nresults=1;
      static const uint8_t c[]={ 0x00, 0x41,0x00, 0x7c, 0x0b };  /* unreachable; i32.const 0; i64.add */
      expect("reject: unreachable;i32.const;i64.add", 0, c,sizeof c, &cx); }

    /* ── ACCEPT: multi-value block [i32 i32]->[i32 i32] then i32.add ── */
    { jav_vctx_t cx = {0};
      static const jav_functype_t ft = { two_i32, 2, two_i32, 2 };
      cx.types=&ft; cx.ntypes=1; cx.results=one_i32; cx.nresults=1;
      static const uint8_t c[]={ 0x41,0x0a, 0x41,0x04, 0x02,0x00, 0x0b, 0x6a, 0x0b };
      expect("accept: multi-value block 2->2", 1, c,sizeof c, &cx); }

    printf("\n§7.6 type-checking validator: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
