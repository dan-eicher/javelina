// test_gc_segments.c — array.new_data / init_data (raw bytes -> numeric elements)
// and array.new_elem / init_elem (element segment -> refs). Both tiers.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const gc_rtt_t RTT_I32 = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY, .elem_store_w = 4 };
static const gc_rtt_t RTT_REF = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY, .elem_is_ref = 1, .elem_store_w = 4 };
static const gc_rtt_t* RTTS[2] = { &RTT_I32, &RTT_REF };

/* $0 = array<i32>, $1 = array<funcref> */
static const jav_arraytype_t ARRAYTYPES[2] = { { WVT_I32, 0, 1 }, { WVT_REF, (uint32_t)HT_FUNC, 1 } };  /* both mutable (init_data/elem) */
static const uint8_t PK0[1] = { 0 };                 /* $0 element i32 = unpacked (pack code 0) */
static const uint8_t* const PACKS[2] = { PK0, NULL };
/* data segment: three i32 little-endian: 10, 20, 30 */
static const uint8_t DBYTES[12] = { 10,0,0,0, 20,0,0,0, 30,0,0,0 };
static const jav_data_seg_t DSEGS[1] = { { DBYTES, 12 } };
/* element segment: two funcref values (funcidx 0, 1) */
static const int64_t EVALS[2] = { 0, 1 };
static const uint8_t ETAGS[2] = { T_REF, T_REF };   /* funcref handles — not GC-traced (§4.2.12 tag row) */
static const jav_elem_seg_t ESEGS[1] = { { EVALS, ETAGS, 2 } };
static const jav_functype_t FSIGS[2] = { { NULL,0,NULL,0 }, { NULL,0,NULL,0 } };
static const jav_valtype_t LOCI[1] = { WVT_REF };
static const uint32_t       LOCI_T[1] = { 0 };
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

static const uint8_t LKINDS[2] = { WST_ARRAY, WST_ARRAY };
static const int32_t LSUP[2] = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static int run(const uint8_t* code, size_t n, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOCI; cx.local_tidx=LOCI_T; cx.nlocals=nloc;
                        cx.results=RES_I32; cx.nresults=1;
                        cx.arraytypes=ARRAYTYPES; cx.narraytypes=2; cx.lattice=&LAT;
                        cx.func_sigs=FSIGS; cx.nfuncs=2;
                        cx.ndatas=1; cx.nelems=1;
                        cx.type_field_packs=PACKS; cx.num_type_field_packs=2;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_locals=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=2; vm.cluster.type_field_packs=PACKS; vm.cluster.num_type_field_packs=2;
    u1 dropped[1]={0}; u1 edropped[1]={0};
    vm.cluster.data_segs=DSEGS; vm.cluster.num_data_segs=1; vm.cluster.data_dropped=dropped;
    vm.cluster.elem_segs=ESEGS; vm.cluster.num_elem_segs=1; vm.cluster.elem_dropped=edropped;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.num_locals=nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -999999 : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}
static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, unsigned nloc, int exp){
    int i=run(c,n,nloc,0), j=run(c,n,nloc,1);
    int ok=(i==j && i==exp);
    printf("  %-40s interp=%-5d jit=%-5d exp=%-5d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
int main(void){
    /* new_data $0 seg0 (offset 0, count 3); get[2] -> 30 */
    static const uint8_t a[]={ 0x41,0x00, 0x41,0x03, 0xfb,0x09,0x00,0x00, 0x41,0x02, 0xfb,0x0b,0x00, 0x0b };
    val("array.new_data get[2]=30", a,sizeof a, 0, 30);
    /* new_default(3) -> loc0; init_data loc0 [d=0, s=4 bytes, count=2]; get[1] -> 30 (seg byte 8) */
    static const uint8_t b[]={ 0x41,0x03, 0xfb,0x07,0x00, 0x21,0x00,
                               0x20,0x00, 0x41,0x00, 0x41,0x04, 0x41,0x02, 0xfb,0x12,0x00,0x00,
                               0x20,0x00, 0x41,0x01, 0xfb,0x0b,0x00, 0x0b };
    val("array.init_data [0]<-seg[1]=20...get[1]", b,sizeof b, 1, 30);
    /* new_elem $1 seg0 (offset 0, count 2); array.len -> 2. (Element values copy via the
     * same s8 path as new_data above, which is value-verified; len proves the segment
     * plumbing; a funcref element can't be compared by ref.eq — it isn't an eqref.) */
    static const uint8_t c[]={ 0x41,0x00, 0x41,0x02, 0xfb,0x0a,0x01,0x00, 0xfb,0x0f, 0x0b };
    val("array.new_elem then array.len = 2", c,sizeof c, 0, 2);
    printf("\ndata/element segment array init (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
