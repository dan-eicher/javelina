// test_table_ops.c — bulk table management (§4.6.7), both tiers. table.size /
// table.grow / table.fill / table.copy / table.init / elem.drop over a real
// multi-table store: size reports the entry count, grow appends (honoring max),
// fill/copy/init move funcref ranges, and a filled/copied/inited slot dispatches
// through call_indirect. Spec-enumerated trap negatives (OOB, dropped segment)
// confirm the verifier-passed-but-runtime-trapping paths. interp == JIT throughout.
#include "interp.h"
#include "jit_driver.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define HUGE_MAX 0xFFFFFFFFu   // a `run`/`val` param sentinel for "table 0 is unbounded" (test-harness only, not an engine value)

static const jav_valtype_t I32[1] = { WVT_I32 };
static const jav_functype_t TYPES[1] = { {I32,1,I32,1} };          /* type 0: i32->i32 */
static const jav_functype_t SIGS[2]  = { {NULL,0,I32,1}, {I32,1,I32,1} };  /* main, f_target */
static const uint8_t f_target[] = { 0x20,0x00, 0x41,0xe4,0x00, 0x6a, 0x0b };   /* x -> x+100 (sleb 100 = E4 00) */

/* run main `code` against a fresh store: two funcref tables (4 null slots each;
 * table 0's maximum is `max0`) and one passive element segment holding funcidx 1
 * (f_target). Everything is rebuilt per run because the bulk ops mutate it. */
static jav_status_t run(const uint8_t* code, size_t n, int jit, int* out, unsigned max0){
    jav_tableinst_t tinst[2]; memset(tinst, 0, sizeof tinst);   // §4.2.4 tables are shared by pointer now
    jav_tableinst_t** tabs=NULL;
    for (int ti=0; ti<2; ti++){
        s8* refs=NULL; u1* rtys=NULL;
        for (int i=0;i<4;i++){ bbq_vec_push(refs,(s8)-1); bbq_vec_push(rtys,(u1)T_REF); }
        tinst[ti].refs=refs; tinst[ti].types=rtys; tinst[ti].reftype=WVT_REF; tinst[ti].reftype_ht=(int32_t)HT_FUNC;
        if (ti==0 && max0 != HUGE_MAX) { tinst[ti].has_max = 1; tinst[ti].max = max0; }   // table 1 + the HUGE_MAX param = unbounded (has_max 0)
        bbq_vec_push(tabs, &tinst[ti]);
    }
    s8 elemvals[1];                                      /* element segment 0 = [ funcref f_target ]; filled once f exists */
    u1 elemtags[1] = { T_REF };                          /* funcref handles — not GC-traced (§4.2.12 tag row) */
    jav_elem_seg_t eseg = { elemvals, elemtags, 1 };
    u1 dropped[1] = { 0 };

    jav_func_t f[2]; memset(f,0,sizeof f);
    jav_st_entry_t *s0,*s1; unsigned k;
    jav_vctx_t cm={0}; cm.results=I32; cm.nresults=1; cm.types=TYPES; cm.ntypes=1;
                        cm.ntables=2; cm.nelems=1; cm.func_sigs=SIGS; cm.nfuncs=2;
    jav_vctx_t ct={0}; ct.locals=I32; ct.nlocals=1; ct.results=I32; ct.nresults=1;
    if (!jav_typecheck(code,n,&cm,&s0,&k) ||
        !jav_typecheck(f_target,sizeof f_target,&ct,&s1,&k)) {
        *out=-1;
        for (int ti=0; ti<2; ti++){ bbq_vec_free(tinst[ti].refs); bbq_vec_free(tinst[ti].types); }
        bbq_vec_free(tabs); return JAV_TRAP;
    }
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.type_index=0,.sig=&SIGS[0],.sidetable=s0};
    f[1]=(jav_func_t){.code=f_target,.code_len=sizeof f_target,.num_params=1,.num_results=1,.type_index=0,.sig=&SIGS[1],.sidetable=s1};
    elemvals[0] = (s8)(uintptr_t)&f[1];                   /* §4.2.1: a funcref is a funcinst pointer, not a funcidx */
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    vm.cluster.functions=f; vm.cluster.num_functions=2;
    vm.cluster.tables=tabs; vm.cluster.types=TYPES; vm.cluster.num_types=1;
    vm.cluster.elem_segs=&eseg; vm.cluster.num_elem_segs=1; vm.cluster.elem_dropped=dropped;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=s0; vm.frame.num_locals=0;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm,NULL);
    *out=jav_tos(&vm).i;
    for (int ti=0; ti<2; ti++) { bbq_vec_free(tinst[ti].refs); bbq_vec_free(tinst[ti].types); }
    bbq_vec_free(tabs); free(s0); free(s1); return st;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp, unsigned max0){
    int ri=0,rj=0; jav_status_t si=run(c,n,0,&ri,max0), sj=run(c,n,1,&rj,max0);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && ri==exp && rj==exp;
    printf("  %-34s interp=%-4d jit=%-4d exp=%-4d [%s]\n", nm,ri,rj,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
static void traps(const char* nm, const uint8_t* c, size_t n){
    int ri=0,rj=0; jav_status_t si=run(c,n,0,&ri,0xFFFFFFFFu), sj=run(c,n,1,&rj,0xFFFFFFFFu);
    int ok = si==JAV_TRAP && sj==JAV_TRAP;
    printf("  %-34s interp=%d jit=%d (want trap)        [%s]\n", nm,si,sj, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* table.size 0 -> 4 (the initial entry count) */
    static const uint8_t a[]={ 0xfc,0x10,0x00, 0x0b };
    val("table.size", a,sizeof a, 4, HUGE_MAX);

    /* table.grow 0 (init=null, delta=2) -> old size 4 */
    static const uint8_t b[]={ 0xd0,0x70, 0x41,0x02, 0xfc,0x0f,0x00, 0x0b };
    val("table.grow returns old size", b,sizeof b, 4, HUGE_MAX);

    /* grow by 2, drop the old size, then table.size -> 6 (the growth took effect) */
    static const uint8_t c[]={ 0xd0,0x70, 0x41,0x02, 0xfc,0x0f,0x00, 0x1a, 0xfc,0x10,0x00, 0x0b };
    val("table.grow then size", c,sizeof c, 6, HUGE_MAX);

    /* grow past the maximum (size 4, max 4, delta 1) -> -1, no trap */
    static const uint8_t d[]={ 0xd0,0x70, 0x41,0x01, 0xfc,0x0f,0x00, 0x0b };
    val("table.grow over max -> -1", d,sizeof d, -1, 4);

    /* table.fill 0 [0,2) = ref.func 1; table.get 0; ref.is_null -> 0 (slot is non-null) */
    static const uint8_t e[]={ 0x41,0x00, 0xd2,0x01, 0x41,0x02, 0xfc,0x11,0x00,
                               0x41,0x00, 0x25,0x00, 0xd1, 0x0b };
    val("table.fill + get is_null", e,sizeof e, 0, HUGE_MAX);

    /* table 1[0] = ref.func 1; table.copy 0<-1 [0,1); call_indirect[0](5) via table 0 -> 105 */
    static const uint8_t f[]={ 0x41,0x00, 0xd2,0x01, 0x26,0x01,
                               0x41,0x00, 0x41,0x00, 0x41,0x01, 0xfc,0x0e,0x00,0x01,
                               0x41,0x05, 0x41,0x00, 0x11,0x00,0x00, 0x0b };
    val("table.copy + call_indirect", f,sizeof f, 105, HUGE_MAX);

    /* table.init 0 from elem seg 0 [0,1); call_indirect[0](5) via table 0 -> 105 */
    static const uint8_t g[]={ 0x41,0x00, 0x41,0x00, 0x41,0x01, 0xfc,0x0c,0x00,0x00,
                               0x41,0x05, 0x41,0x00, 0x11,0x00,0x00, 0x0b };
    val("table.init + call_indirect", g,sizeof g, 105, HUGE_MAX);

    /* table.fill 0 at i=3 n=5 (3+5 > 4) -> trap (i32.const tail keeps it well-typed) */
    static const uint8_t h[]={ 0x41,0x03, 0xd0,0x70, 0x41,0x05, 0xfc,0x11,0x00, 0x41,0x00, 0x0b };
    traps("table.fill OOB traps", h,sizeof h);

    /* table.copy d=2 n=5 (2+5 > 4) -> trap */
    static const uint8_t i[]={ 0x41,0x02, 0x41,0x00, 0x41,0x05, 0xfc,0x0e,0x00,0x01, 0x41,0x00, 0x0b };
    traps("table.copy OOB traps", i,sizeof i);

    /* table.init n=5 (> elem-seg length 1) -> trap */
    static const uint8_t j[]={ 0x41,0x00, 0x41,0x00, 0x41,0x05, 0xfc,0x0c,0x00,0x00, 0x41,0x00, 0x0b };
    traps("table.init OOB traps", j,sizeof j);

    /* elem.drop 0; table.init from the dropped segment -> trap */
    static const uint8_t k[]={ 0xfc,0x0d,0x00,
                               0x41,0x00, 0x41,0x00, 0x41,0x01, 0xfc,0x0c,0x00,0x00, 0x41,0x00, 0x0b };
    traps("table.init after elem.drop traps", k,sizeof k);

    printf("\nbulk table ops (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
