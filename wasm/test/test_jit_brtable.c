// test_jit_brtable.c — br_table JITs via resync (vector skipped in the walk).
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const uint8_t code[] = {0x02,0x40, 0x02,0x40, 0x20,0x00, 0x0e,0x01,0x00,0x01,
                               0x0b, 0x41,0x0b, 0x0f, 0x0b, 0x41,0x16, 0x0b};
static const jav_valtype_t I32[1] = { WVT_I32 };
static int run(int32_t key, int jit){
    jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=1; cx.results=I32; cx.nresults=1;
    jav_st_entry_t* st; unsigned n; jav_typecheck(code,sizeof code,&cx,&st,&n);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,code,sizeof code);
    vm.frame.sidetable=st; vm.frame.locals[0].i=key; vm.frame.num_locals=1;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}
static int fails=0;
#define CK(k,exp) do{int i=run(k,0),j=run(k,1);int ok=(i==j&&i==(exp)); \
  printf("  br_table key=%-2d interp=%d jit=%d [%s]\n",k,i,j,ok?"PASS":"FAIL");fails+=!ok;}while(0)
int main(void){ CK(0,11); CK(1,22); CK(5,22);
  printf("\nJIT br_table (interp == JIT): %s\n",fails?"FAIL":"ALL PASS"); return fails?1:0; }
