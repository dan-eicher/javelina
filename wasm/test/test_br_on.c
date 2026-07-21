// test_br_on.c — br_on_null / br_on_non_null, both null and non-null inputs, both
// tiers. The ref lives in local 0; the function reduces each path to a checkable
// i32. Exercises the conditional-branch-on-null machinery + the side-table.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NULLREF JAV_NULLREF   /* the authority (jav_frame.h) — never a local copy of the value */
#define NONNULL 0xFFFFFFFEu   /* caller marker: a genuine non-null funcref = &f[0] (a funcinst pointer,
                                the §4.2.1 non-null rep). NOT 0 — bare 0 is itself a null rep. */
static const jav_valtype_t LOC[2] = { WVT_REF, WVT_I32 };
static const uint32_t      LOC_T[2] = { (uint32_t)HT_FUNC, 0 };
static const jav_valtype_t I32[1] = { WVT_I32 };

static int run(const uint8_t* code, size_t n, unsigned ref, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOC; cx.local_tidx=LOC_T; cx.nlocals=nloc; cx.results=I32; cx.nresults=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -2;     /* validation failed */
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_params=1,.num_locals=nloc-1,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    vm.frame.locals[0].r = (ref == NONNULL) ? (s8)(uintptr_t)&f[0] : (s8)(u4)ref; vm.frame.num_locals = nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,NULL);
    int r=jav_tos(&vm).i; bbq_vec_free(st); return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, unsigned ref, unsigned nloc, int exp){
    int i=run(c,n,ref,nloc,0), j=run(c,n,ref,nloc,1);
    int ok = (i==j && i==exp);
    printf("  %-36s interp=%-3d jit=%-3d exp=%-3d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* br_on_non_null: block(result funcref){ local.get0; br_on_non_null 0; ref.null }; ref.is_null
     * non-null -> branch (result = the ref) -> is_null 0; null -> fall-through default null -> is_null 1 */
    static const uint8_t nn[]={ 0x02,0x70, 0x20,0x00, 0xd6,0x00, 0xd0,0x70, 0x0b, 0xd1, 0x0b };
    val("br_on_non_null(funcref) is_null", nn,sizeof nn, NONNULL,   1, 0);
    val("br_on_non_null(null) is_null",    nn,sizeof nn, NULLREF,   1, 1);

    /* br_on_null: block{ local.get0; br_on_null 0; drop; i32.const1; local.set1 }; local.get1
     * null -> branch out (marker stays 0); non-null -> drop the ref, set marker 1 */
    static const uint8_t bn[]={ 0x02,0x40, 0x20,0x00, 0xd5,0x00, 0x1a, 0x41,0x01, 0x21,0x01,
                                0x0b, 0x20,0x01, 0x0b };
    val("br_on_null(funcref) -> marker 1", bn,sizeof bn, NONNULL,   2, 1);
    val("br_on_null(null) -> marker 0",    bn,sizeof bn, NULLREF,   2, 0);

    printf("\nbr_on_null / br_on_non_null (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
