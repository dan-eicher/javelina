// test_refcast.c — ref.test / ref.cast downcasts on aggregate refs, both tiers.
// A struct ref tests positive against (ref struct) and its own concrete typeidx,
// negative against (ref array); ref.cast passes a matching ref through and traps
// on a mismatch. Heap-type immediates: -21 struct, -22 array, >=0 a typeidx.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = struct{ i32 } (a leaf); $1 = array<i32> (to test a negative cast) */
static const gc_rtt_t RTT_S = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT };
static const gc_rtt_t RTT_A = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY, .elem_store_w = 4 };
static const gc_rtt_t* RTTS[2] = { &RTT_S, &RTT_A };

static const jav_valtype_t F_S[1] = { WVT_I32 };  static const uint32_t T_S[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {F_S,T_S,1}, {0} };
static const jav_arraytype_t  ARRAYTYPES[2]  = { {0}, { WVT_I32, 0 } };
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

#define TRAP_SENTINEL (-0x7FFFFFF)

static const uint8_t LKINDS[2] = { WST_STRUCT, WST_ARRAY };
static const int32_t LSUP[2] = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=2;
                        cx.arraytypes=ARRAYTYPES; cx.narraytypes=2; cx.lattice=&LAT;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=2; vm.cluster.lattice=&LAT;   // runtime ref.test/cast consults the §3.3 lattice
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP_SENTINEL : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=run(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-40s interp=%-10d jit=%-10d exp=%-10d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* const 42; struct.new $0; ref.test (ref struct) -> 1 */
    static const uint8_t a[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x14,0x6b, 0x0b };
    val("ref.test struct vs (ref struct)", a,sizeof a, 1);
    /* const 42; struct.new $0; ref.test (ref array) -> 0 (a struct is not an array) */
    static const uint8_t b[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x14,0x6a, 0x0b };
    val("ref.test struct vs (ref array)", b,sizeof b, 0);
    /* const 42; struct.new $0; ref.test (ref $0) -> 1 (concrete typeidx identity) */
    static const uint8_t cc[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x14,0x00, 0x0b };
    val("ref.test struct vs (ref $0)", cc,sizeof cc, 1);
    /* const 42; struct.new $0; ref.cast (ref struct); struct.get $0 0 -> 42 */
    static const uint8_t d[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x16,0x00, 0xfb,0x02,0x00,0x00, 0x0b };
    val("ref.cast $0 ok then struct.get", d,sizeof d, 42);
    /* const 42; struct.new $0; ref.cast (ref array) -> TRAP (a struct is not an array) */
    static const uint8_t e[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x16,0x6a, 0x1a, 0x41,0x00, 0x0b };
    val("ref.cast struct to array traps", e,sizeof e, TRAP_SENTINEL);
    printf("\nref.test / ref.cast downcasts (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
