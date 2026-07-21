// test_call_indirect.c — indirect dispatch through a funcref table, both tiers.
// Proves: correct dynamic dispatch by table index, and the three traps —
// out-of-bounds index, null funcref, and a function whose type doesn't match the
// call_indirect's typeidx (the dynamic type gate). interp == JIT throughout.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t I32[2] = { WVT_I32, WVT_I32 };
static const jav_valtype_t I64[1] = { WVT_I64 };
static const jav_functype_t TYPES[2] = { {I32,1,I32,1}, {I64,1,I64,1} };   /* 0: i32->i32, 1: i64->i64 */

/* main(arg, slot): local.get0; local.get1; call_indirect type0 table0; end */
static const uint8_t main_code[] = { 0x20,0x00, 0x20,0x01, 0x11,0x00,0x00, 0x0b };
static const uint8_t f_double[]  = { 0x20,0x00, 0x20,0x00, 0x6a, 0x0b };       /* x -> x+x   (i32) */
static const uint8_t f_triple[]  = { 0x20,0x00, 0x41,0x03, 0x6c, 0x0b };       /* x -> x*3   (i32) */
static const uint8_t f_long[]    = { 0x20,0x00, 0x0b };                        /* x -> x     (i64) */
/* funcidx per slot: double, triple, long, JAV_NULLREF-null, then a 0-rep null. A funcref
 * is null in TWO reps — the JAV_NULLREF sentinel (explicit ref.null) AND a bare 0 (a zero-
 * initialised defaultable slot, e.g. a funcref array element from array.new_default). The
 * call_indirect null-guard must reject BOTH before dereferencing the funcinst pointer;
 * -2 marks the 0-rep slot. */
static int32_t TABLE[5]          = { 1, 2, 3, -1, -2 };

static jav_status_t run(int32_t arg, int32_t slot, int jit, int* out){
    jav_func_t f[4]; memset(f,0,sizeof f);
    jav_st_entry_t *s0,*s1,*s2,*s3; unsigned n;
    /* validate each function with its own context (main uses call_indirect -> types+ntables) */
    jav_vctx_t cm={0}; cm.locals=I32; cm.nlocals=2; cm.results=I32; cm.nresults=1;
                        cm.types=TYPES; cm.ntypes=2; cm.ntables=1;
    jav_vctx_t ci={0}; ci.locals=I32; ci.nlocals=1; ci.results=I32; ci.nresults=1;
    jav_vctx_t cl={0}; cl.locals=I64; cl.nlocals=1; cl.results=I64; cl.nresults=1;
    if (!jav_typecheck(main_code,sizeof main_code,&cm,&s0,&n) ||
        !jav_typecheck(f_double,sizeof f_double,&ci,&s1,&n) ||
        !jav_typecheck(f_triple,sizeof f_triple,&ci,&s2,&n) ||
        !jav_typecheck(f_long,sizeof f_long,&cl,&s3,&n)) { *out=-1; return JAV_TRAP; }
    f[0]=(jav_func_t){.code=main_code,.code_len=sizeof main_code,.num_params=2,.num_results=1,.type_index=0,.sig=&TYPES[0],.sidetable=s0};
    f[1]=(jav_func_t){.code=f_double,.code_len=sizeof f_double,.num_params=1,.num_results=1,.type_index=0,.sig=&TYPES[0],.sidetable=s1};
    f[2]=(jav_func_t){.code=f_triple,.code_len=sizeof f_triple,.num_params=1,.num_results=1,.type_index=0,.sig=&TYPES[0],.sidetable=s2};
    f[3]=(jav_func_t){.code=f_long,  .code_len=sizeof f_long,  .num_params=1,.num_results=1,.type_index=1,.sig=&TYPES[1],.sidetable=s3};

    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    vm.cluster.functions=f; vm.cluster.num_functions=4;
    s8* refs=NULL; u1* rtys=NULL;
    // funcref = a funcinst POINTER (§4.2.1), not a funcidx. Both negative TABLE entries are null:
    // there is now ONE null representation (JAV_NULLREF == 0). The engine previously had two — an
    // all-ones sentinel AND bare-0 zero-init — and this test covered both; that duality was
    // collapsed when §2.3.4's reserved i31 tag bit forced null to be even. Both entries still trap.
    for (int i=0;i<5;i++){ s8 r = TABLE[i] < 0 ? (s8)(u4)JAV_NULLREF : (s8)(uintptr_t)&f[TABLE[i]];
                           bbq_vec_push(refs,r); bbq_vec_push(rtys,(u1)T_REF); }
    jav_tableinst_t tt={0}; tt.refs=refs; tt.types=rtys; tt.max=5; jav_tableinst_t* tabs=NULL; bbq_vec_push(tabs, tt);
    vm.cluster.tables=tabs; vm.cluster.types=TYPES; vm.cluster.num_types=2;
    bbq_ctx_init(&vm.frame.code, main_code, sizeof main_code); vm.frame.sidetable=s0;
    vm.frame.locals[0].i=arg; vm.frame.locals[1].i=slot; vm.frame.num_locals=2;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm,NULL);
    *out = jav_tos(&vm).i;
    bbq_vec_free(refs); bbq_vec_free(rtys); bbq_vec_free(tabs);
    free(s0); free(s1); free(s2); free(s3); return st;
}

static int fails=0;
static void val(const char* nm, int32_t arg, int32_t slot, int exp){
    int ri=0,rj=0; jav_status_t si=run(arg,slot,0,&ri), sj=run(arg,slot,1,&rj);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && ri==exp && rj==exp;
    printf("  %-34s interp=%-5d jit=%-5d exp=%-5d [%s]\n", nm,ri,rj,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
static void trap(const char* nm, int32_t arg, int32_t slot){
    int ri=0,rj=0; jav_status_t si=run(arg,slot,0,&ri), sj=run(arg,slot,1,&rj);
    int ok = si==JAV_TRAP && sj==JAV_TRAP;
    printf("  %-34s interp=%d jit=%d (want trap)        [%s]\n", nm,si,sj, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    val("call_indirect slot0 double(5)", 5,0, 10);
    val("call_indirect slot1 triple(5)", 5,1, 15);
    trap("slot2 long: type mismatch",    5,2);
    trap("slot3: null funcref",          5,3);
    trap("slot4: 0-rep null funcref",    5,4);
    trap("slot7: out of bounds",         5,7);
    printf("\ncall_indirect (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
