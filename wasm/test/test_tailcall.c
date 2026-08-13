// test_tailcall.c — return_call / return_call_indirect / return_call_ref, BOTH tiers.
//
// A tail call REPLACES the current frame rather than nesting one (jav_call's loop
// rebuilds the callee in the same frame base), so deep tail recursion runs in O(1)
// C-stack: sum_acc(50000) completes, where the SAME recursion as a normal `call`
// would blow past the call-depth limit and trap (cf. test_depth). Each variant runs
// interp == JIT, with the recursive callee JIT-compiled so the return_call STENCIL is
// exercised — not merely bailed to the interpreter.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define N       50000
#define EXPECT  1250025000   /* N*(N+1)/2, fits a signed i32 */

static const jav_valtype_t I32x2[2] = { WVT_I32, WVT_I32 };
static const jav_functype_t SIG2    = { I32x2, 2, I32x2, 1 };   /* (i32,i32)->(i32) */

// entry (funcidx 0): i32.const N; i32.const 0; call 1   — launches the accumulator
static const uint8_t entry[] = { 0x41,0xD0,0x86,0x03, 0x41,0x00, 0x10,0x01, 0x0b };

// sum_acc(n, acc) = n==0 ? acc : TAIL sum_acc(n-1, acc+n). The three variants differ
// only in how the tail call to funcidx 1 is expressed:
//   local.get0; i32.eqz; if i32; local.get1; else; (n-1); (acc+n); <TAIL>; end; end
static const uint8_t sum_rc[] = {   /* return_call 1 */
    0x20,0x00, 0x45, 0x04,0x7f, 0x20,0x01, 0x05,
    0x20,0x00, 0x41,0x01, 0x6b, 0x20,0x01, 0x20,0x00, 0x6a, 0x12,0x01, 0x0b, 0x0b };
static const uint8_t sum_rci[] = {  /* i32.const 1 (table slot holding funcidx 1); return_call_indirect 0 0 */
    0x20,0x00, 0x45, 0x04,0x7f, 0x20,0x01, 0x05,
    0x20,0x00, 0x41,0x01, 0x6b, 0x20,0x01, 0x20,0x00, 0x6a, 0x41,0x01, 0x13,0x00,0x00, 0x0b, 0x0b };
static const uint8_t sum_rcr[] = {  /* ref.func 1; return_call_ref 0 */
    0x20,0x00, 0x45, 0x04,0x7f, 0x20,0x01, 0x05,
    0x20,0x00, 0x41,0x01, 0x6b, 0x20,0x01, 0x20,0x00, 0x6a, 0xd2,0x01, 0x15,0x00, 0x0b, 0x0b };

typedef enum { V_DIRECT, V_INDIRECT, V_REF } variant_t;

static int run(const uint8_t* rec, size_t rlen, variant_t v, int jit, jav_status_t* st) {
    static const jav_functype_t sigs[2] = { SIG2, SIG2 };   /* funcidx -> signature */
    static const uint32_t ftidx[2] = { 0, 0 };               /* funcidx -> defined typeidx */
    static const jav_functype_t types[1] = { SIG2 };        /* typeidx 0 = (i32,i32)->(i32) */

    // Validate the recursive callee (funcidx 1) under the full reference context.
    jav_vctx_t cr = {0};
    cr.locals = I32x2; cr.nlocals = 2; cr.results = I32x2; cr.nresults = 1;
    cr.func_sigs = sigs; cr.nfuncs = 2; cr.func_type_idx = ftidx;
    cr.types = types; cr.ntypes = 1; cr.ntables = 1;
    jav_st_entry_t* sr; unsigned nsr;
    jav_typecheck(rec, rlen, &cr, &sr, &nsr);

    // Validate the entry (funcidx 0): no params, one i32 result.
    jav_vctx_t ce = {0};
    ce.results = I32x2; ce.nresults = 1; ce.func_sigs = sigs; ce.nfuncs = 2;
    jav_st_entry_t* se; unsigned nse;
    jav_typecheck(entry, sizeof entry, &ce, &se, &nse);

    // The recursive callee is JIT-compiled (so the return_call stencil runs) when jit.
    bbq_ctx_t rc; bbq_ctx_init(&rc, rec, rlen);
    jit_func_t* h = jit ? jit_compile(rc, NULL) : NULL;

    jav_func_t f[2]; memset(f, 0, sizeof f);
    f[0].code = entry; f[0].code_len = sizeof entry; f[0].num_results = 1; f[0].sidetable = se;
    f[0].type_index = 0; f[0].sig = &sigs[0];
    f[1].code = rec;   f[1].code_len = rlen; f[1].num_params = 2; f[1].num_results = 1;
    f[1].sidetable = sr; f[1].trytable = NULL; f[1].type_index = 0; f[1].sig = &sigs[1];
    if (jit) { f[1].invoke = jit_invoke; f[1].invoke_ctx = h; }

    s8* refs=NULL; u1* rtys=NULL;                                       /* slot 1 -> funcinst &f[1] (the recursive fn) */
    bbq_vec_push(refs,(s8)-1); bbq_vec_push(rtys,(u1)T_REF); bbq_vec_push(refs,(s8)(uintptr_t)&f[1]); bbq_vec_push(rtys,(u1)T_REF);
    jav_tableinst_t tt={0}; tt.refs=refs; tt.types=rtys; tt.max=2; jav_tableinst_t* tabs=NULL; bbq_vec_push(tabs, tt);

    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    vm.cluster.functions = f; vm.cluster.num_functions = 2;
    vm.cluster.types = types; vm.cluster.num_types = 1;
    vm.cluster.tables = tabs;
    (void)v;
    bbq_ctx_init(&vm.frame.code, entry, sizeof entry); vm.frame.sidetable = se;
    *st = jit ? jav_jit_run(&vm) : interp_run(&vm, NULL);
    int r = jav_tos(&vm).i;
    bbq_vec_free(refs); bbq_vec_free(rtys); bbq_vec_free(tabs);
    jav_vm_free(&vm); jit_free(h); bbq_vec_free(sr); bbq_vec_free(se);
    return r;
}

static int fails = 0;
static void ck(const char* label, const uint8_t* rec, size_t rlen, variant_t v) {
    jav_status_t si, sj;
    int i = run(rec, rlen, v, 0, &si), j = run(rec, rlen, v, 1, &sj);
    int ok = (si == JAV_RETURN && sj == JAV_RETURN && i == j && i == EXPECT);
    printf("  %-22s interp=%d jit=%d [%s]\n", label, i, j, ok ? "PASS" : "FAIL");
    fails += !ok;
}

// A host import as the tail target: entry -> call 1; fn1 does `return_call 0` where
// funcidx 0 is a host that returns acc. Proves jav_call's loop hands a host tail
// result back correctly (the host branch of the loop).
static jav_status_t host_id(vm_t* vm, heap_t* h, void* ctx) { (void)h; (void)ctx;   // host import = invoke thunk
    vm->frame.stack[0] = vm->frame.locals[0];                  // results on the frame stack
    vm->frame.stack_types[0] = vm->frame.local_types[0]; vm->frame.sp = 1; return JAV_RETURN; }
static const uint8_t entry_h[]  = { 0x41,0x07, 0x10,0x01, 0x0b };           /* push 7; call 1 */
static const uint8_t fn_to_host[] = { 0x20,0x00, 0x12,0x00, 0x0b };          /* local.get0; return_call 0(host) */
static void ck_host(int jit) {
    static const jav_valtype_t I32[1] = { WVT_I32 };
    static const jav_functype_t sigs[2] = { { I32,1,I32,1 }, { I32,1,I32,1 } };
    jav_vctx_t cf = {0}; cf.locals=I32; cf.nlocals=1; cf.results=I32; cf.nresults=1; cf.func_sigs=sigs; cf.nfuncs=2;
    jav_st_entry_t* sf; unsigned n; jav_typecheck(fn_to_host, sizeof fn_to_host, &cf, &sf, &n);
    jav_vctx_t ce = {0}; ce.results=I32; ce.nresults=1; ce.func_sigs=sigs; ce.nfuncs=2;
    jav_st_entry_t* se; jav_typecheck(entry_h, sizeof entry_h, &ce, &se, &n);
    jav_func_t f[2]; memset(f,0,sizeof f);
    f[0].invoke = host_id; f[0].num_params = 1; f[0].num_results = 1;   // host (the thunk IS the invoke)
    f[1].code = fn_to_host; f[1].code_len = sizeof fn_to_host; f[1].num_params = 1; f[1].num_results = 1; f[1].sidetable = sf;
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=2;
    bbq_ctx_init(&vm.frame.code, entry_h, sizeof entry_h); vm.frame.sidetable = se;
    jav_status_t s = jit ? jav_jit_run(&vm) : interp_run(&vm, NULL);
    int ok = (s == JAV_RETURN && jav_tos(&vm).i == 7);
    if (!jit) printf("  %-22s ", "host tail target");
    printf("%s=%d ", jit ? "jit" : "interp", jav_tos(&vm).i);
    if (jit) printf("[%s]\n", ok ? "PASS" : "FAIL");
    fails += !ok;
    jav_vm_free(&vm); bbq_vec_free(sf); bbq_vec_free(se);
}

// The verifier must REJECT a return_call whose callee results don't match the current
// function's (§3.4.8: C.return = t'2*, C ⊢ t2* ≤ t'2*). Returns the typecheck verdict.
static int validates(const uint8_t* code, size_t n, const jav_functype_t* sigs, unsigned nf,
                     const jav_valtype_t* res, unsigned nres) {
    jav_vctx_t cx = {0};
    cx.results = res; cx.nresults = nres;
    cx.func_sigs = sigs; cx.nfuncs = nf; cx.types = sigs; cx.ntypes = nf; cx.ntables = 1;
    jav_st_entry_t* st = NULL; unsigned k;
    int ok = jav_typecheck(code, n, &cx, &st, &k);
    bbq_vec_free(st);
    return ok;
}
static void ckv(const char* label, int got, int want) {
    int ok = (got == want);
    printf("  %-30s %s [%s]\n", label, got ? "accept" : "reject", ok ? "PASS" : "FAIL");
    fails += !ok;
}

int main(void) {
    printf("tail calls — O(1) stack, interp == JIT:\n");
    ck("return_call",          sum_rc,  sizeof sum_rc,  V_DIRECT);
    ck("return_call_indirect", sum_rci, sizeof sum_rci, V_INDIRECT);
    ck("return_call_ref",      sum_rcr, sizeof sum_rcr, V_REF);
    ck_host(0); ck_host(1);

    // Verifier reject cases for the result-type rule.
    static const jav_valtype_t I32[1] = { WVT_I32 }, F32[1] = { WVT_F32 };
    static const uint8_t rc1[] = { 0x12,0x01, 0x0b };        /* return_call 1; (func returns i32) */
    static const jav_functype_t mism[2] = { {NULL,0,I32,1}, {NULL,0,F32,1} };   /* callee returns f32 */
    ckv("rejects result-TYPE mismatch", validates(rc1, sizeof rc1, mism, 2, I32, 1), 0);
    static const jav_functype_t arity[2] = { {NULL,0,I32,1}, {NULL,0,I32x2,2} }; /* callee returns 2 */
    ckv("rejects result-ARITY mismatch", validates(rc1, sizeof rc1, arity, 2, I32, 1), 0);
    static const jav_functype_t match[2] = { {NULL,0,I32,1}, {NULL,0,I32,1} };
    ckv("accepts matching result type", validates(rc1, sizeof rc1, match, 2, I32, 1), 1);
    printf("\ntail calls (%d-deep, would trap as normal calls): %s\n",
           N, fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
