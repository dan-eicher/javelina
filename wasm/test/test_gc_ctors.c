// test_gc_ctors.c — struct.new_default, array.new_default/new_fixed, array.fill,
// array.copy. Both tiers. $0 = struct{i32,i32}; $1 = array<i32>.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const gc_rtt_t RTT_STRUCT = { .size = (uint32_t)sizeof(gc_obj_t) + 16, .kind = GC_KIND_STRUCT };
static const gc_rtt_t RTT_ARRAY  = { .size = (uint32_t)sizeof(gc_obj_t) + 8,  .kind = GC_KIND_ARRAY, .elem_store_w = 4 };
static const gc_rtt_t* RTTS[2] = { &RTT_STRUCT, &RTT_ARRAY };

static const jav_valtype_t SFIELDS[2] = { WVT_I32, WVT_I32 };
static const uint32_t       STIDX[2]   = { 0, 0 };
static const jav_structtype_t STRUCTTYPES[2] = { { SFIELDS, STIDX, 2 }, {0} };
static const jav_arraytype_t  ARRAYTYPES[2]  = { {0}, { WVT_I32, 0, 1 } };   /* array $1 mutable (fill/copy) */
static const jav_valtype_t LOC[2] = { WVT_REF, WVT_REF };
static const uint32_t       LOC_TIDX[2] = { 1, 1 };   /* both locals are (ref $1) = array */
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

#define TRAP (-0x7FFFFFF)
static const uint8_t LKINDS[2]={WST_STRUCT,WST_ARRAY}; static const int32_t LSUP[2]={-1,-1};
static const jav_subtype_ctx_t LAT={LKINDS,LSUP,2};

static int run(const uint8_t* code, size_t n, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOC; cx.local_tidx=LOC_TIDX; cx.nlocals=nloc; cx.results=RES_I32; cx.nresults=1; cx.lattice=&LAT;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=2;
                        cx.arraytypes=ARRAYTYPES; cx.narraytypes=2;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_locals=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=2;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st; vm.frame.num_locals=nloc;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, unsigned nloc, int exp){
    int i=run(c,n,nloc,0), j=run(c,n,nloc,1);
    int ok=(i==j && i==exp);
    printf("  %-38s interp=%-6d jit=%-6d exp=%-6d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    /* struct.new_default $0; struct.get $0 1 -> 0 (default) */
    static const uint8_t a[]={ 0xfb,0x01,0x00, 0xfb,0x02,0x00,0x01, 0x0b };
    val("struct.new_default field=0", a,sizeof a, 0, 0);
    /* const 3; array.new_default $1; const 2; array.get $1 -> 0 */
    static const uint8_t b[]={ 0x41,0x03, 0xfb,0x07,0x01, 0x41,0x02, 0xfb,0x0b,0x01, 0x0b };
    val("array.new_default elem=0", b,sizeof b, 0, 0);
    /* const 10; const 20; const 30; array.new_fixed $1 3; const 1; array.get $1 -> 20 */
    static const uint8_t c[]={ 0x41,0x0a, 0x41,0x14, 0x41,0x1e, 0xfb,0x08,0x01,0x03,
                               0x41,0x01, 0xfb,0x0b,0x01, 0x0b };
    val("array.new_fixed get[1]=20", c,sizeof c, 0, 20);
    /* len5 default array in local0; fill [1,4)=7; get[2] -> 7 */
    static const uint8_t d[]={ 0x41,0x05, 0xfb,0x07,0x01, 0x21,0x00,
                               0x20,0x00, 0x41,0x01, 0x41,0x07, 0x41,0x03, 0xfb,0x10,0x01,
                               0x20,0x00, 0x41,0x02, 0xfb,0x0b,0x01, 0x0b };
    val("array.fill [1,4)=7 then get[2]", d,sizeof d, 1, 7);
    /* fill leaves get[0] untouched (=0) */
    static const uint8_t e[]={ 0x41,0x05, 0xfb,0x07,0x01, 0x21,0x00,
                               0x20,0x00, 0x41,0x01, 0x41,0x07, 0x41,0x03, 0xfb,0x10,0x01,
                               0x20,0x00, 0x41,0x00, 0xfb,0x0b,0x01, 0x0b };
    val("array.fill leaves get[0]=0", e,sizeof e, 1, 0);
    /* src=new_fixed[5,6,7] in loc0; dst=new_default(3) in loc1; copy dst[0..2)<-src[1..3);
       get dst[1] -> 7 */
    static const uint8_t g[]={ 0x41,0x05, 0x41,0x06, 0x41,0x07, 0xfb,0x08,0x01,0x03, 0x21,0x00,
                               0x41,0x03, 0xfb,0x07,0x01, 0x21,0x01,
                               0x20,0x01, 0x41,0x00, 0x20,0x00, 0x41,0x01, 0x41,0x02, 0xfb,0x11,0x01,0x01,
                               0x20,0x01, 0x41,0x01, 0xfb,0x0b,0x01, 0x0b };
    val("array.copy dst[1]<-src[2]=7", g,sizeof g, 2, 7);
    printf("\nGC constructors + bulk (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
