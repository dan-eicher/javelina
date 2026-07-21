// test_br_on_cast.c — br_on_cast / br_on_cast_fail: a conditional downcast that
// branches on cast success/failure, carrying the ref to the label. The block's
// type (a functype result of structref) gives the label its ref type. Both tiers.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const gc_rtt_t RTT_S = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT };
static const gc_rtt_t* RTTS[1] = { &RTT_S };

/* A consistent type space (as a real module's jav_module_cx would project): $0 a struct,
 * $1 the block's func type [] -> [(ref $0)]. kinds[] and the func table agree on each index. */
static const jav_valtype_t F_S[1] = { WVT_I32 };  static const uint32_t T_S[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {F_S,T_S,1}, {0} };          /* $0 struct; $1 (func) n/a */
static const jav_valtype_t BT_RES[1] = { WVT_REF };
static const uint32_t       BT_RES_T[1] = { 0 };
static const jav_functype_t TYPES[2]  = { {0}, { NULL,0, BT_RES,1, NULL, BT_RES_T } };  /* $1 = [] -> [(ref $0)] */
static const jav_valtype_t RES_I32[1] = { WVT_I32 };
static const uint8_t LKINDS[2] = { WST_STRUCT, WST_FUNC };  static const int32_t LSUP[2] = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
                        cx.types=TYPES; cx.ntypes=2;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=2; cx.lattice=&LAT;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=1;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -1 : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=run(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-44s interp=%-3d jit=%-3d exp=%-3d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* block (type $0 -> (ref $0)):
     *   const 42; struct.new $0;        ;; (ref $0) on the stack
     *   br_on_cast 0 (ref any) (ref $0) ;; success => branch to end carrying (ref $0)
     *   unreachable                     ;; fall-through is rt1\rt2 = (ref any) — dead here (§3.4)
     * end
     * struct.get $0 0                    ;; -> 42  (cast succeeded, ref survived the branch)
     * flags=0 (both non-null), label=0, ht1=any(0x6e), ht2=$0(0x00) */
    static const uint8_t a[]={ 0x02,0x01,
                                 0x41,0x2a, 0xfb,0x00,0x00,
                                 0xfb,0x18, 0x00, 0x00, 0x6e, 0x00,
                                 0x00,                          /* unreachable: the (ref any) fall-through */
                               0x0b,
                               0xfb,0x02,0x00,0x00, 0x0b };
    val("br_on_cast struct branches, ref survives", a,sizeof a, 42);

    /* br_on_cast_fail (sub 25 = 0x19) with rt1=rt2=(ref $0): the cast always succeeds, so it
     * NEVER branches; the ref falls through as (ref $0), the block ends with it, struct.get -> 42.
     * (Both the branch's rt1\rt2 and the fall-through's rt2 are (ref $0), so the block stays valid.) */
    static const uint8_t b[]={ 0x02,0x01,
                                 0x41,0x2a, 0xfb,0x00,0x00,
                                 0xfb,0x19, 0x00, 0x00, 0x00, 0x00,
                               0x0b,
                               0xfb,0x02,0x00,0x00, 0x0b };
    val("br_on_cast_fail falls through on success", b,sizeof b, 42);

    printf("\nbr_on_cast / br_on_cast_fail (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
