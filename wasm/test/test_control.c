#include "interp.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const jav_valtype_t I32[4] = { WVT_I32, WVT_I32, WVT_I32, WVT_I32 };
static int run(const uint8_t* code, size_t n, int32_t p, int nloc){
    jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=(unsigned)nloc; cx.results=I32; cx.nresults=1;
    jav_st_entry_t* st; unsigned nst; if(!jav_typecheck(code,n,&cx,&st,&nst)){printf("VALIDATE FAIL\n");return -999;}
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,code,n);
    vm.frame.sidetable=st; vm.frame.locals[0].i=p; vm.frame.num_locals=nloc;
    interp_run(&vm,NULL); int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}
// Run with a CALLER-SUPPLIED side-table instead of jav_typecheck's, so the
// interpreter's consumption of the table can be pinned independently of the
// validator's production of it.
static int run_st(const uint8_t* code, size_t n, const jav_st_entry_t* st, int32_t p, int nloc){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,code,n);
    vm.frame.sidetable=st; vm.frame.locals[0].i=p; vm.frame.num_locals=nloc;
    interp_run(&vm,NULL); return jav_tos(&vm).i;
}
int main(void){
    int fails=0;
    uint8_t ie[]={0x20,0x00, 0x04,0x7f, 0x41,0x0a, 0x05, 0x41,0x14, 0x0b, 0x0b};
    int t=run(ie,sizeof ie,1,1), f=run(ie,sizeof ie,0,1);
    printf("  if/else: 1->%d 0->%d [%s]\n",t,f,(t==10&&f==20)?"PASS":"FAIL"); fails+=!(t==10&&f==20);
    // The same if/else against a hand-written side-table: [0]=if (false -> else
    // body), [1]=else (then -> end). Were the validator and the interpreter ever
    // to drift together onto a consistent but wrong encoding, the case above
    // would still pass and this one would not.
    static const jav_st_entry_t st_hand[] = { {3,1,0,0}, {2,1,1,0} };
    int ht=run_st(ie,sizeof ie,st_hand,1,1), hf=run_st(ie,sizeof ie,st_hand,0,1);
    printf("  if/else (hand-written side-table): 1->%d 0->%d [%s]\n",
           ht,hf,(ht==10&&hf==20)?"PASS":"FAIL"); fails+=!(ht==10&&hf==20);
    uint8_t br[]={0x02,0x7f, 0x41,0x2a, 0x0c,0x00, 0x0b, 0x0b};
    int b=run(br,sizeof br,0,1); printf("  block/br -> %d [%s]\n",b,b==42?"PASS":"FAIL"); fails+=(b!=42);
    // loop: i=5; acc=0; loop{ acc+=i; i-=1; if i!=0 loop } return acc  => 15
    uint8_t lp[]={0x41,0x05,0x21,0x00, 0x41,0x00,0x21,0x01, 0x03,0x40,
                  0x20,0x01,0x20,0x00,0x6a,0x21,0x01, 0x20,0x00,0x41,0x01,0x6b,0x21,0x00,
                  0x20,0x00,0x0d,0x00, 0x0b, 0x20,0x01, 0x0b};
    int l=run(lp,sizeof lp,0,2); printf("  loop sum 5..1 -> %d [%s]\n",l,l==15?"PASS":"FAIL"); fails+=(l!=15);
    printf("\ncontrol (if/else, block/br, loop+br_if): %s\n",fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
