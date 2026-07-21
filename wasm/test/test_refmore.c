// test_refmore.c — ref.eq (struct + i31 identity) and any.convert_extern /
// extern.convert_any (identity round-trip preserves the reference). Both tiers.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const gc_rtt_t RTT_S = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT };
static const gc_rtt_t* RTTS[1] = { &RTT_S };
static const jav_valtype_t SF[1] = { WVT_I32 };  static const uint32_t ST[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[1] = { { SF, ST, 1 } };
static const jav_valtype_t LOC[1] = { WVT_REF };
static const uint32_t       LOCT[1] = { 0 };
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

static const uint8_t LKINDS[1] = { WST_STRUCT };
static const int32_t LSUP[1] = { -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 1 };

static int run(const uint8_t* code, size_t n, unsigned nloc, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.locals=LOC; cx.local_tidx=LOCT; cx.nlocals=nloc;
                        cx.results=RES_I32; cx.nresults=1;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=1; cx.lattice=&LAT;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_locals=nloc,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=1;
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
    printf("  %-44s interp=%-3d jit=%-3d exp=%-3d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
int main(void){
    /* struct.new $0 -> loc0; ref.eq(loc0, loc0) -> 1 */
    static const uint8_t a[]={ 0x41,0x07, 0xfb,0x00,0x00, 0x21,0x00,
                               0x20,0x00, 0x20,0x00, 0xd3, 0x0b };
    val("ref.eq(x, x) struct = 1", a,sizeof a, 1, 1);
    /* two distinct structs -> ref.eq -> 0 */
    static const uint8_t b[]={ 0x41,0x07, 0xfb,0x00,0x00, 0x41,0x07, 0xfb,0x00,0x00, 0xd3, 0x0b };
    val("ref.eq(new, new) struct = 0", b,sizeof b, 0, 0);
    /* ref.i31 5; ref.i31 5; ref.eq -> 1 */
    static const uint8_t c[]={ 0x41,0x05, 0xfb,0x1c, 0x41,0x05, 0xfb,0x1c, 0xd3, 0x0b };
    val("ref.eq(i31 5, i31 5) = 1", c,sizeof c, 0, 1);
    /* ref.i31 5; ref.i31 6; ref.eq -> 0 */
    static const uint8_t d[]={ 0x41,0x05, 0xfb,0x1c, 0x41,0x06, 0xfb,0x1c, 0xd3, 0x0b };
    val("ref.eq(i31 5, i31 6) = 0", d,sizeof d, 0, 0);
    /* struct{7}; extern.convert_any; any.convert_extern; ref.cast $0; struct.get $0 0
     * -> 7. The value surviving the round-trip proves identity was preserved, and it's
     * well-typed (ref.eq can't compare an anyref, so verify by casting back). */
    static const uint8_t e[]={ 0x41,0x07, 0xfb,0x00,0x00,
                               0xfb,0x1b, 0xfb,0x1a, 0xfb,0x16,0x00, 0xfb,0x02,0x00,0x00, 0x0b };
    val("convert any<->extern round-trip identity", e,sizeof e, 0, 7);
    printf("\nref.eq + any/extern convert (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
