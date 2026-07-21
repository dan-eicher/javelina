// test_host.c — a WASM function calls a HOST import (a C function), both tiers.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* A host import IS an invoke thunk: read params from frame.locals, leave the results
 * on the frame stack — the same place a wasm callee leaves them. */
static jav_status_t triple(vm_t* vm, heap_t* h, void* ctx){ (void)h;(void)ctx;
    vm->frame.stack[0].i = vm->frame.locals[0].i * 3;
    vm->frame.stack_types[0] = T_INT; vm->frame.sp = 1; return JAV_RETURN; }
static int run(int jit){
    static const uint8_t main_code[] = {0x41,0x07, 0x10,0x00, 0x0b};   // i32.const 7; call 0 (host); end
    jav_func_t funcs[2]; memset(funcs,0,sizeof funcs);
    funcs[0].invoke = triple;      // function 0 = host import (the thunk IS the invoke)
    funcs[0].num_params = 1; funcs[0].num_results = 1;
    static const jav_valtype_t I32[1] = { WVT_I32 };
    static const jav_functype_t sigs[2] = { {I32,1,I32,1}, {NULL,0,I32,1} };  /* [0]=triple, [1]=main */
    jav_vctx_t cx = {0}; cx.results=I32; cx.nresults=1; cx.func_sigs=sigs; cx.nfuncs=2;
    jav_st_entry_t* st; unsigned nst; jav_typecheck(main_code, sizeof main_code, &cx, &st, &nst);
    funcs[1].code = main_code; funcs[1].code_len = sizeof main_code; funcs[1].num_results = 1; funcs[1].sidetable = st;
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions = funcs; vm.cluster.num_functions = 2;
    bbq_ctx_init(&vm.frame.code, main_code, sizeof main_code); vm.frame.sidetable = st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm, NULL);
    int r = jav_tos(&vm).i; bbq_vec_free(st); return r;
}
int main(void){
    int i=run(0), j=run(1); int ok=(i==j && i==21);
    printf("  call host triple(7)  interp=%d jit=%d [%s]\n", i, j, ok?"PASS":"FAIL");
    printf("\nhost imports (interp == JIT): %s\n", ok?"ALL PASS":"FAIL");
    return ok?0:1;
}
