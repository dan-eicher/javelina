// test_array.c — array.new / get / set / len on a GC-allocated array, both tiers.
// $0 = array of i32; new fills with one init value, get/set are bounds-checked.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = array<i32>: header + u4 length; elements are slot-sized (8B), not refs */
static const gc_rtt_t RTT_ARR = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY, .elem_store_w = 4 };
static const gc_rtt_t* RTTS[1] = { &RTT_ARR };

static const jav_arraytype_t ARRAYTYPES[1] = { { WVT_I32, 0, 1 } };   /* mutable element (array.set) */
static const uint8_t LKINDS[1] = { WST_ARRAY };  static const int32_t LSUP[1] = { -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 1 };
static const jav_valtype_t I32[1] = { WVT_I32 };
static const jav_valtype_t LOC[1] = { WVT_REF };   /* local 0: (ref $0) */
static const uint32_t      LOC_T[1] = { 0 };

static int run(const uint8_t* code, size_t n, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOC; cx.local_tidx=LOC_T; cx.nlocals=nloc; cx.results=I32; cx.nresults=1;
                        cx.arraytypes=ARRAYTYPES; cx.narraytypes=1; cx.lattice=&LAT;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_locals=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=1;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.num_locals=nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r=jav_tos(&vm).i; jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, unsigned nloc, int exp){
    int i=run(c,n,nloc,0), j=run(c,n,nloc,1);
    int ok=(i==j && i==exp);
    printf("  %-34s interp=%-5d jit=%-5d exp=%-5d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* array.new $0 (init 7, len 3); local.set0; local.get0; idx1; val99; array.set $0;
       local.get0; idx1; array.get $0 -> 99 */
    static const uint8_t a[]={ 0x41,0x07, 0x41,0x03, 0xfb,0x06,0x00, 0x21,0x00,
                               0x20,0x00, 0x41,0x01, 0x41,0xe3,0x00, 0xfb,0x0e,0x00,
                               0x20,0x00, 0x41,0x01, 0xfb,0x0b,0x00, 0x0b };
    val("array set[1]=99 then get[1]", a,sizeof a, 1, 99);
    /* array.new $0 (init 7, len 5); array.len -> 5 */
    static const uint8_t b[]={ 0x41,0x07, 0x41,0x05, 0xfb,0x06,0x00, 0xfb,0x0f, 0x0b };
    val("array.new(len 5).len", b,sizeof b, 0, 5);
    /* array.new (len 3); get[0] -> the init value 7 */
    static const uint8_t c[]={ 0x41,0x07, 0x41,0x03, 0xfb,0x06,0x00, 0x41,0x00, 0xfb,0x0b,0x00, 0x0b };
    val("array.new init fills: get[0]", c,sizeof c, 0, 7);
    printf("\nGC arrays (array.new/get/set/len, interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
