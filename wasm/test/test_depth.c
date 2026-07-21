// test_depth.c — deep recursion hits the call-depth limit as a clean WASM trap,
// not a C-stack segfault. Both tiers; the VM (jav_call) is the single guard.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// sum(n) = n==0 ? 0 : n + sum(n-1)   (recurses on function index 0 = itself)
static const uint8_t sum_code[]={0x20,0x00, 0x45, 0x04,0x7f, 0x41,0x00, 0x05,
    0x20,0x00, 0x20,0x00, 0x41,0x01, 0x6b, 0x10,0x00, 0x6a, 0x0b, 0x0b};
static const jav_valtype_t I32[1] = { WVT_I32 };
static const jav_functype_t SUMSIG[1] = { { I32, 1, I32, 1 } };   /* sum: (i32)->(i32) */
static jav_status_t run(int n, int jit, int* out){
    jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=1; cx.results=I32; cx.nresults=1;
    cx.func_sigs=SUMSIG; cx.nfuncs=1;   /* recurses on itself (funcidx 0) */
    jav_st_entry_t* st; unsigned nst; jav_typecheck(sum_code,sizeof sum_code,&cx,&st,&nst);
    jav_func_t f[1]; memset(f,0,sizeof f);
    f[0].code=sum_code; f[0].code_len=sizeof sum_code; f[0].num_params=1; f[0].num_results=1; f[0].sidetable=st;
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    bbq_ctx_init(&vm.frame.code, sum_code, sizeof sum_code); vm.frame.sidetable=st;
    vm.frame.locals[0].i=n; vm.frame.num_locals=1;
    jav_status_t s = jit ? jav_jit_run(&vm) : interp_run(&vm,NULL);
    *out = jav_tos(&vm).i; bbq_vec_free(st); return s;
}
int main(void){
    int fails=0, ri, rj;
    jav_status_t si=run(4000,0,&ri), sj=run(4000,1,&rj);   // 1500 deep — would segfault with inline frames
    int ok1=(si==JAV_RETURN && sj==JAV_RETURN && ri==8002000 && rj==8002000);
    printf("  sum(4000)   -> interp=%d jit=%d [%s]\n", ri,rj, ok1?"PASS":"FAIL"); fails+=!ok1;
    jav_status_t di=run(100000,0,&ri), dj=run(100000,1,&rj);   // would segfault without the guard
    int ok2=(di==JAV_TRAP && dj==JAV_TRAP);
    printf("  sum(100000) -> interp_trap=%d jit_trap=%d [%s]\n", di==JAV_TRAP, dj==JAV_TRAP, ok2?"PASS":"FAIL"); fails+=!ok2;
    printf("\nstack-exhaustion trap (no segfault): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
