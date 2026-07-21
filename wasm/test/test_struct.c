// test_struct.c — struct.new / struct.get / struct.set on a GC-allocated struct,
// both tiers. A struct type $0 = {i32, i32}; field access + mutation. The struct
// is a real managed object (jav_gc_new); GC survival is covered by test_gc_roots.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* struct $0 = { i32, i32 }: header + 2 slot-sized fields, no reference fields */
static const gc_rtt_t RTT0 = { .size = (uint32_t)sizeof(gc_obj_t) + 16, .kind = GC_KIND_STRUCT };
static const gc_rtt_t* STRUCT_RTTS[1] = { &RTT0 };

static const jav_valtype_t FIELDS[2] = { WVT_I32, WVT_I32 };
static const uint32_t FTIDX[2] = { 0, 0 };
static const uint8_t FMUT[2] = { 1, 1 };   /* mutable fields (struct.set) */
static const jav_structtype_t STRUCTTYPES[1] = { { FIELDS, FTIDX, 2, FMUT } };
static const jav_valtype_t I32[1] = { WVT_I32 };
static const jav_valtype_t LOC[1] = { WVT_REF };   /* local 0: a (ref $0) */
static const uint32_t      LOC_T[1] = { 0 };

static int run(const uint8_t* code, size_t n, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOC; cx.local_tidx=LOC_T; cx.nlocals=nloc; cx.results=I32; cx.nresults=1;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_locals=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=STRUCT_RTTS; vm.cluster.num_struct_rtts=1;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.num_locals=nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);   /* interp_run sets vm->heap = h */
    int r=jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, unsigned nloc, int exp){
    int i=run(c,n,nloc,0), j=run(c,n,nloc,1);
    int ok=(i==j && i==exp);
    printf("  %-34s interp=%-5d jit=%-5d exp=%-5d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* struct.new(10,20); struct.get $0 1 -> 20 */
    static const uint8_t a[]={ 0x41,0x0a, 0x41,0x14, 0xfb,0x00,0x00, 0xfb,0x02,0x00,0x01, 0x0b };
    val("struct.new(10,20).get[1]", a,sizeof a, 0, 20);
    /* struct.new(10,20); local.set0; local.get0; const99; struct.set $0 0; local.get0; struct.get $0 0 -> 99 */
    static const uint8_t b[]={ 0x41,0x0a, 0x41,0x14, 0xfb,0x00,0x00, 0x21,0x00,
                               0x20,0x00, 0x41,0xe3,0x00, 0xfb,0x05,0x00,0x00,
                               0x20,0x00, 0xfb,0x02,0x00,0x00, 0x0b };
    val("struct.set[0]=99 then get[0]", b,sizeof b, 1, 99);
    printf("\nGC structs (struct.new/get/set, interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
