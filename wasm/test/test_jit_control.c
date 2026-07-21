// test_jit_control.c — control flow JITs via IP-driven resync (the JVM branch
// pattern). if/else, block/br, loop+br_if: interp == JIT.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const jav_valtype_t I32[4] = { WVT_I32, WVT_I32, WVT_I32, WVT_I32 };
static int run(const uint8_t* code, size_t n, int32_t p, int nloc, int jit){
    jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=(unsigned)nloc; cx.results=I32; cx.nresults=1;
    jav_st_entry_t* st; unsigned nst; if(!jav_typecheck(code,n,&cx,&st,&nst)) return -999;
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,code,n);
    vm.frame.sidetable=st; vm.frame.locals[0].i=p; vm.frame.num_locals=nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}
static int fails=0;
#define CK(label, expect, p, nloc, ...) do { uint8_t b[]={__VA_ARGS__}; \
    int i=run(b,sizeof b,p,nloc,0), j=run(b,sizeof b,p,nloc,1); int ok=(i==j && i==(expect)); \
    printf("  %-16s interp=%d jit=%d [%s]\n",label,i,j,ok?"PASS":"FAIL"); fails+=!ok; } while(0)
int main(void){
    CK("if(1)->10", 10, 1,1, 0x20,0x00, 0x04,0x7f, 0x41,0x0a, 0x05, 0x41,0x14, 0x0b, 0x0b);
    CK("if(0)->20", 20, 0,1, 0x20,0x00, 0x04,0x7f, 0x41,0x0a, 0x05, 0x41,0x14, 0x0b, 0x0b);
    CK("block/br->42", 42, 0,1, 0x02,0x7f, 0x41,0x2a, 0x0c,0x00, 0x0b, 0x0b);
    CK("loop sum->15", 15, 0,2, 0x41,0x05,0x21,0x00, 0x41,0x00,0x21,0x01, 0x03,0x40,
        0x20,0x01,0x20,0x00,0x6a,0x21,0x01, 0x20,0x00,0x41,0x01,0x6b,0x21,0x00, 0x20,0x00,0x0d,0x00, 0x0b, 0x20,0x01, 0x0b);
    printf("\nJIT control (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
