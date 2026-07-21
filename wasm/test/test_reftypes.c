// test_reftypes.c — reference values + table read/write, both tiers. ref.func /
// ref.null / ref.is_null produce and test funcrefs; table.set stores one and
// call_indirect dispatches through it; table.get reads a null slot back. The full
// "funcref as a first-class value" loop, interp == JIT.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t I32[1] = { WVT_I32 };
static const jav_functype_t TYPES[1] = { {I32,1,I32,1} };          /* type 0: i32->i32 */
static const jav_functype_t SIGS[2]  = { {NULL,0,I32,1}, {I32,1,I32,1} };  /* main, f_target */
static const uint8_t f_target[] = { 0x20,0x00, 0x41,0xe4,0x00, 0x6a, 0x0b };   /* x -> x+100 (sleb 100 = E4 00) */

static jav_status_t run(const uint8_t* code, size_t n, int jit, int* out){
    s8* refs=NULL; u1* rtys=NULL;                                      /* fresh per run (table.set mutates) */
    for (int i=0;i<4;i++){ bbq_vec_push(refs,(s8)(u4)JAV_NULLREF); bbq_vec_push(rtys,(u1)T_REF); }   /* null funcref slots (the authority, not a literal) */
    jav_tableinst_t tt={0}; tt.refs=refs; tt.types=rtys; tt.max=4; jav_tableinst_t* tabs=NULL; bbq_vec_push(tabs, tt);
    jav_func_t f[2]; memset(f,0,sizeof f);
    jav_st_entry_t *s0,*s1; unsigned k;
    jav_vctx_t cm={0}; cm.results=I32; cm.nresults=1; cm.types=TYPES; cm.ntypes=1;
                        cm.ntables=1; cm.func_sigs=SIGS; cm.nfuncs=2;
    jav_vctx_t ct={0}; ct.locals=I32; ct.nlocals=1; ct.results=I32; ct.nresults=1;
    if (!jav_typecheck(code,n,&cm,&s0,&k) ||
        !jav_typecheck(f_target,sizeof f_target,&ct,&s1,&k)) { *out=-1; return JAV_TRAP; }
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.type_index=0,.sig=&SIGS[0],.sidetable=s0};
    f[1]=(jav_func_t){.code=f_target,.code_len=sizeof f_target,.num_params=1,.num_results=1,.type_index=0,.sig=&SIGS[1],.sidetable=s1};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    vm.cluster.functions=f; vm.cluster.num_functions=2;
    vm.cluster.tables=tabs; vm.cluster.types=TYPES; vm.cluster.num_types=1;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=s0; vm.frame.num_locals=0;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm,NULL);
    *out=jav_tos(&vm).i; bbq_vec_free(refs); bbq_vec_free(rtys); bbq_vec_free(tabs); free(s0); free(s1); return st;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int ri=0,rj=0; jav_status_t si=run(c,n,0,&ri), sj=run(c,n,1,&rj);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && ri==exp && rj==exp;
    printf("  %-34s interp=%-4d jit=%-4d exp=%-4d [%s]\n", nm,ri,rj,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* ref.is_null(ref.func 1) -> 0 (a non-null funcref) */
    static const uint8_t a[]={ 0xd2,0x01, 0xd1, 0x0b };
    val("ref.is_null(ref.func)", a,sizeof a, 0);
    /* ref.is_null(ref.null func) -> 1 */
    static const uint8_t b[]={ 0xd0,0x70, 0xd1, 0x0b };
    val("ref.is_null(ref.null)", b,sizeof b, 1);
    /* table.set 0 = ref.func 1; call_indirect[0](5) -> f_target(5) = 105 */
    static const uint8_t c[]={ 0x41,0x00, 0xd2,0x01, 0x26,0x00,
                               0x41,0x05, 0x41,0x00, 0x11,0x00,0x00, 0x0b };
    val("table.set + call_indirect", c,sizeof c, 105);
    /* table.get 3 (a null slot); ref.is_null -> 1 */
    static const uint8_t d[]={ 0x41,0x03, 0x25,0x00, 0xd1, 0x0b };
    val("table.get(null) is_null", d,sizeof d, 1);
    /* ref.as_non_null(ref.func 1) passes through; ref.is_null -> 0 */
    static const uint8_t e[]={ 0xd2,0x01, 0xd4, 0xd1, 0x0b };
    val("ref.as_non_null(funcref)", e,sizeof e, 0);
    /* ref.as_non_null(ref.null) traps */
    static const uint8_t g[]={ 0xd0,0x70, 0xd4, 0xd1, 0x0b };
    { int ri=0,rj=0; jav_status_t si=run(g,sizeof g,0,&ri), sj=run(g,sizeof g,1,&rj);
      int ok=(si==JAV_TRAP && sj==JAV_TRAP);
      printf("  %-34s interp=%d jit=%d (want trap)        [%s]\n","ref.as_non_null(null) traps",si,sj,ok?"PASS":"FAIL");
      fails+=!ok; }
    printf("\nreference types + tables (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
