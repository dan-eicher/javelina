// test_i31.c — i31 unboxed references: ref.i31 wraps the low 31 bits; i31.get_s /
// i31.get_u read them back sign- / zero-extended, and trap on a null i31ref. Both
// tiers. 0xFB-prefixed (the GC prefix) — exercises the multi-byte dispatch + the
// generated transfer functions (i31 validates with no special-casing).
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NULLREF JAV_NULLREF   /* the authority (jav_frame.h) — never a local copy of the value */
static const jav_valtype_t I31[1] = { WVT_REF };
static const uint32_t      I31_T[1] = { (uint32_t)HT_I31 };
static const jav_valtype_t I32[1] = { WVT_I32 };

static int run(const uint8_t* code, size_t n, unsigned nloc, unsigned loc0, int jit, jav_status_t* st_out){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=I31; cx.local_tidx=I31_T; cx.nlocals=nloc; cx.results=I32; cx.nresults=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) { *st_out=JAV_TRAP; return -2; }
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_params=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (nloc) { vm.frame.locals[0].r = loc0; vm.frame.num_locals = nloc; }
    *st_out = jit ? jav_jit_run(&vm) : interp_run(&vm,NULL);
    int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    jav_status_t si,sj; int i=run(c,n,0,0,0,&si), j=run(c,n,0,0,1,&sj);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && i==exp && j==exp;
    printf("  %-30s interp=%-11d jit=%-11d exp=%-11d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* ref.i31(5); i31.get_s -> 5 */
    static const uint8_t a[]={ 0x41,0x05, 0xfb,0x1c, 0xfb,0x1d, 0x0b };
    val("ref.i31(5).get_s", a,sizeof a, 5);
    /* ref.i31(-1); i31.get_s -> -1  (31-bit -1 sign-extends) */
    static const uint8_t b[]={ 0x41,0x7f, 0xfb,0x1c, 0xfb,0x1d, 0x0b };
    val("ref.i31(-1).get_s", b,sizeof b, -1);
    /* ref.i31(-1); i31.get_u -> 0x7FFFFFFF  (zero-extend the 31-bit value) */
    static const uint8_t c[]={ 0x41,0x7f, 0xfb,0x1c, 0xfb,0x1e, 0x0b };
    val("ref.i31(-1).get_u", c,sizeof c, 2147483647);

    /* i31.get_s on a NULL i31ref (in local 0) traps, both tiers */
    static const uint8_t d[]={ 0x20,0x00, 0xfb,0x1d, 0x0b };
    jav_status_t si,sj; run(d,sizeof d,1,NULLREF,0,&si); run(d,sizeof d,1,NULLREF,1,&sj);
    int ok = si==JAV_TRAP && sj==JAV_TRAP;
    printf("  %-30s interp=%d jit=%d (want trap)         [%s]\n","i31.get_s(null) traps",si,sj, ok?"PASS":"FAIL");
    fails+=!ok;

    printf("\ni31 references (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
