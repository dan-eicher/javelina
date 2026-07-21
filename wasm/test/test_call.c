// test_call.c — call + frames, BOTH tiers via one nested jav_call. interp == JIT.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct { const uint8_t* code; size_t len; uint32_t np, nl, nr; } fdef_t;
static const jav_valtype_t I32[8] = { WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32 };
static int run(fdef_t* d, int n, int main_idx, int jit){
    jav_func_t* funcs = calloc(n, sizeof *funcs);
    jav_st_entry_t** sts = calloc(n, sizeof *sts);
    jav_functype_t* sigs = calloc(n, sizeof *sigs);     /* funcidx -> (i32^np)->(i32^nr) */
    for (int i=0;i<n;i++){ sigs[i].params=I32; sigs[i].nparams=(uint16_t)d[i].np;
        sigs[i].results=I32; sigs[i].nresults=(uint16_t)d[i].nr; }
    for (int i=0;i<n;i++){ unsigned nst;
        jav_vctx_t cx = {0}; cx.locals=I32; cx.nlocals=d[i].np+d[i].nl;
        cx.results=I32; cx.nresults=d[i].nr; cx.func_sigs=sigs; cx.nfuncs=(unsigned)n;
        jav_typecheck(d[i].code,d[i].len,&cx,&sts[i],&nst);
        funcs[i].code=d[i].code; funcs[i].code_len=d[i].len; funcs[i].num_params=d[i].np;
        funcs[i].num_locals=d[i].nl; funcs[i].num_results=d[i].nr; funcs[i].sidetable=sts[i]; }
    free(sigs);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=funcs; vm.cluster.num_functions=n;
    bbq_ctx_init(&vm.frame.code,d[main_idx].code,d[main_idx].len);
    vm.frame.sidetable=sts[main_idx]; vm.frame.num_locals=d[main_idx].np+d[main_idx].nl;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    int r=jav_tos(&vm).i;
    for (int i=0;i<n;i++) bbq_vec_free(sts[i]); free(funcs); free(sts); return r;
}
static int fails=0;
#define CK(label, defs, nf, expect) do { int i=run(defs,nf,0,0), j=run(defs,nf,0,1); \
  int ok=(i==j && i==(expect)); printf("  %-18s interp=%d jit=%d [%s]\n",label,i,j,ok?"PASS":"FAIL"); fails+=!ok; } while(0)
int main(void){
    static const uint8_t m_add[]={0x41,0x03, 0x41,0x05, 0x10,0x01, 0x0b};
    static const uint8_t add[]  ={0x20,0x00, 0x20,0x01, 0x6a, 0x0b};
    fdef_t p1[]={ {m_add,sizeof m_add,0,0,1}, {add,sizeof add,2,0,1} };
    CK("call add(3,5)", p1, 2, 8);
    static const uint8_t m_sum[]={0x41,0x05, 0x10,0x01, 0x0b};
    static const uint8_t sum[]  ={0x20,0x00, 0x45, 0x04,0x7f, 0x41,0x00, 0x05,
        0x20,0x00, 0x20,0x00, 0x41,0x01, 0x6b, 0x10,0x01, 0x6a, 0x0b, 0x0b};
    fdef_t p2[]={ {m_sum,sizeof m_sum,0,0,1}, {sum,sizeof sum,1,0,1} };
    CK("recursive sum(5)", p2, 2, 15);
    printf("\ncall + frames (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
