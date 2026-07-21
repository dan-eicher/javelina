#include "interp.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// block $a { block $b { local.get 0; br_table 0 1 } end$b; i32.const 11; return } end$a; i32.const 22
static const uint8_t code[] = {0x02,0x40, 0x02,0x40, 0x20,0x00, 0x0e,0x01,0x00,0x01,
                               0x0b, 0x41,0x0b, 0x0f, 0x0b, 0x41,0x16, 0x0b};
static const jav_valtype_t I32[1] = { WVT_I32 };
static int run(int32_t key){
    jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=1; cx.results=I32; cx.nresults=1;
    jav_st_entry_t* st; unsigned n; jav_typecheck(code,sizeof code,&cx,&st,&n);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,code,sizeof code);
    vm.frame.sidetable=st; vm.frame.locals[0].i=key; vm.frame.num_locals=1;
    interp_run(&vm,NULL); int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}
int main(void){
    int k0=run(0), k1=run(1), k5=run(5), fails=0;
    printf("  br_table key=0 -> %d [%s]\n",k0,k0==11?"PASS":"FAIL"); fails+=(k0!=11);
    printf("  br_table key=1 -> %d [%s]\n",k1,k1==22?"PASS":"FAIL"); fails+=(k1!=22);
    printf("  br_table key=5 (default) -> %d [%s]\n",k5,k5==22?"PASS":"FAIL"); fails+=(k5!=22);
    printf("\nbr_table: %s\n",fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
