// test_tiers.c — mixed-tier coexistence: one VM, functions of different tiers
// calling each other through the index-based seam, every caller oblivious.
//   main(JIT) -> doubler(JIT, compiled once) -> [back in main] -> tripler(interp) -> triple(host)
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static jav_status_t triple(vm_t* vm, heap_t* h, void* ctx){ (void)h;(void)ctx;   // host import = invoke thunk
    vm->frame.stack[0].i=vm->frame.locals[0].i*3;          // results on the frame stack
    vm->frame.stack_types[0]=T_INT; vm->frame.sp=1; return JAV_RETURN; }
static const uint8_t doubler[]      = {0x20,0x00, 0x20,0x00, 0x6a, 0x0b};       // x -> x+x
static const uint8_t tripler[]      = {0x20,0x00, 0x10,0x00, 0x0b};            // x -> call host 0 (triple)
static const uint8_t main_code[]    = {0x41,0x05, 0x10,0x01, 0x10,0x02, 0x0b}; // 5 -> double -> triple
static int run(int jit, jit_func_t* h1, jav_st_entry_t* sd, jav_st_entry_t* st, jav_st_entry_t* sm){
    jav_func_t f[3]; memset(f,0,sizeof f);
    f[0].invoke=triple; f[0].num_params=1; f[0].num_results=1;  // host (the thunk IS the invoke)
    f[1].code=doubler; f[1].code_len=sizeof doubler; f[1].num_params=1; f[1].num_results=1;
    f[1].sidetable=sd; f[1].invoke=jit_invoke; f[1].invoke_ctx=h1;                             // JITed
    f[2].code=tripler; f[2].code_len=sizeof tripler; f[2].num_params=1; f[2].num_results=1; f[2].sidetable=st;  // interp
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=3;
    bbq_ctx_init(&vm.frame.code, main_code, sizeof main_code); vm.frame.sidetable=sm;
    if(jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    return jav_tos(&vm).i;
}
int main(void){
    jav_st_entry_t *sd,*st,*sm; unsigned n;
    static const jav_valtype_t I32[1] = { WVT_I32 };
    static const jav_functype_t sigs[3] = { {I32,1,I32,1}, {I32,1,I32,1}, {I32,1,I32,1} }; /* triple,doubler,tripler */
    jav_vctx_t cd = {0}; cd.locals=I32; cd.nlocals=1; cd.results=I32; cd.nresults=1; cd.func_sigs=sigs; cd.nfuncs=3;
    jav_vctx_t ct = {0}; ct.locals=I32; ct.nlocals=1; ct.results=I32; ct.nresults=1; ct.func_sigs=sigs; ct.nfuncs=3;
    jav_vctx_t cm = {0}; cm.results=I32; cm.nresults=1; cm.func_sigs=sigs; cm.nfuncs=3;  /* main: no params */
    jav_typecheck(doubler,sizeof doubler,&cd,&sd,&n);
    jav_typecheck(tripler,sizeof tripler,&ct,&st,&n);
    jav_typecheck(main_code,sizeof main_code,&cm,&sm,&n);
    bbq_ctx_t dc; bbq_ctx_init(&dc, doubler, sizeof doubler);
    jit_func_t* h1 = jit_compile(dc);   // compile the doubler ONCE; re-entered each call
    int i=run(0,h1,sd,st,sm), j=run(1,h1,sd,st,sm); int ok=(i==j && i==30);
    printf("  main->dbl(JIT)->trpl(interp)->host  interp=%d jit=%d [%s]\n", i,j, ok?"PASS":"FAIL");
    jit_free(h1); free(sd); bbq_vec_free(st); free(sm);
    printf("\nmixed-tier coexistence (interp == JIT): %s\n", ok?"ALL PASS":"FAIL");
    return ok?0:1;
}
