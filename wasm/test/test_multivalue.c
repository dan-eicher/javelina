// test_multivalue.c — the multi-value proposal: typeidx block types, multi-result
// blocks/branches, multi-result function returns, and call arity from the callee's
// type. Each case reduces to one final result (via i32.sub) so the multi-value-ness
// is internal; verified interp == JIT. Validation goes through jav_typecheck
// with the module's block-type table + per-function arities.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct { const uint8_t* code; size_t len; uint32_t np, nl, nr; } fdef_t;

static const jav_valtype_t I32[8] = { WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32,WVT_I32 };
static int run(fdef_t* d, int n, int main_idx,
               const jav_functype_t* types, unsigned ntypes, int jit){
    jav_func_t* funcs = calloc(n, sizeof *funcs);
    jav_st_entry_t** sts = calloc(n, sizeof *sts);
    jav_functype_t* sigs = calloc(n, sizeof *sigs);     /* funcidx -> (i32^np)->(i32^nr) */
    for (int i=0;i<n;i++){ sigs[i].params=I32; sigs[i].nparams=(uint16_t)d[i].np;
        sigs[i].results=I32; sigs[i].nresults=(uint16_t)d[i].nr; }
    for (int i=0;i<n;i++){ unsigned nst;
        jav_vctx_t cx = {0}; cx.types=types; cx.ntypes=ntypes; cx.func_sigs=sigs; cx.nfuncs=(unsigned)n;
        cx.locals=I32; cx.nlocals=d[i].np+d[i].nl; cx.results=I32; cx.nresults=d[i].nr;
        jav_typecheck(d[i].code,d[i].len, &cx, &sts[i],&nst);
        funcs[i].code=d[i].code; funcs[i].code_len=d[i].len; funcs[i].num_params=d[i].np;
        funcs[i].num_locals=d[i].nl; funcs[i].num_results=d[i].nr; funcs[i].sidetable=sts[i]; }
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=funcs; vm.cluster.num_functions=n;
    bbq_ctx_init(&vm.frame.code,d[main_idx].code,d[main_idx].len);
    vm.frame.sidetable=sts[main_idx]; vm.frame.num_locals=d[main_idx].np+d[main_idx].nl;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    int r=jav_tos(&vm).i;
    for (int i=0;i<n;i++) bbq_vec_free(sts[i]); free(funcs); free(sts); free(sigs); return r;
}

static int fails=0;
static void ck(const char* label, fdef_t* d, int n, const jav_functype_t* ty, unsigned nty, int expect){
    int i=run(d,n,0,ty,nty,0), j=run(d,n,0,ty,nty,1);
    int ok=(i==j && i==expect);
    printf("  %-30s interp=%-5d jit=%-5d exp=%-5d [%s]\n", label,i,j,expect, ok?"PASS":"FAIL");
    fails+=!ok;
}

int main(void){
    /* 1. Multi-result function RETURN: swap(7,3) -> (3,7); caller sub -> -4. */
    static const uint8_t swap[]={ 0x20,0x01, 0x20,0x00, 0x0b };           /* local.get1; local.get0 */
    static const uint8_t m_swap[]={ 0x41,0x07, 0x41,0x03, 0x10,0x01, 0x6b, 0x0b }; /* 7;3;call swap;sub */
    fdef_t p1[]={ {m_swap,sizeof m_swap,0,0,1}, {swap,sizeof swap,2,0,2} };
    ck("multi-result return swap", p1, 2, NULL,0, -4);

    /* 2. Multi-value BLOCK (type 0: [i32,i32]->[i32,i32], identity): 10;4;block;end;sub -> 6. */
    static const jav_functype_t ty2[]={ {I32,2,I32,2} };   /* [i32,i32]->[i32,i32] */
    static const uint8_t blk[]={ 0x41,0x0a, 0x41,0x04, 0x02,0x00, 0x0b, 0x6b, 0x0b };
    fdef_t p2[]={ {blk,sizeof blk,0,0,1} };
    ck("multi-value block (2->2)", p2, 1, ty2,1, 6);

    /* 3. Multi-result forward BRANCH: block(type0: ->[i32,i32]){ 1;2;br 0 } end; sub -> -1. */
    static const jav_functype_t ty3[]={ {NULL,0,I32,2} };   /* []->[i32,i32] */
    static const uint8_t brm[]={ 0x02,0x00, 0x41,0x01, 0x41,0x02, 0x0c,0x00, 0x0b, 0x6b, 0x0b };
    fdef_t p3[]={ {brm,sizeof brm,0,0,1} };
    ck("multi-result forward br", p3, 1, ty3,1, -1);

    /* 4. Call ARITY from the callee's type: pair()->(11,4); caller sub -> 7. With a
     *    0-param/2-result callee the height delta (+2) differs from a fixed guess. */
    static const uint8_t pair[]={ 0x41,0x0b, 0x41,0x04, 0x0b };           /* 11; 4 */
    static const uint8_t m_pair[]={ 0x10,0x01, 0x6b, 0x0b };              /* call pair; sub */
    fdef_t p4[]={ {m_pair,sizeof m_pair,0,0,1}, {pair,sizeof pair,0,0,2} };
    ck("call arity (0->2 result)", p4, 2, NULL,0, 7);

    printf("\nmulti-value (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
