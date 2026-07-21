// test_bulk_mem.c — memory.fill / copy / init and data.drop, both tiers. A load
// after each op reads back the effect; bounds violations and use-after-drop trap.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const uint8_t DBYTES[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
static const jav_data_seg_t DSEGS[1] = { { DBYTES, 4 } };
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

#define TRAP (-0x6BADBAD)
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.ndatas=1; cx.nmemories=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    u1 dropped[1]={0};
    vm.heap=&heap;  vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1; vm.cluster.data_segs=DSEGS; vm.cluster.num_data_segs=1; vm.cluster.data_dropped=dropped;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st);
    return r;
}
static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=run(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-38s interp=%-11d jit=%-11d exp=%-11d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
int main(void){
    /* fill mem[0..4)=1; i32.load @0 -> 0x01010101 */
    static const uint8_t a[]={ 0x41,0x00, 0x41,0x01, 0x41,0x04, 0xfc,0x0b,0x00,
                               0x41,0x00, 0x28,0x02,0x00, 0x0b };
    val("memory.fill then load = 0x01010101", a,sizeof a, 0x01010101);
    /* store 42 @0; copy [0..4)->[8..12); load @8 -> 42 */
    static const uint8_t b[]={ 0x41,0x00, 0x41,0x2a, 0x36,0x02,0x00,
                               0x41,0x08, 0x41,0x00, 0x41,0x04, 0xfc,0x0a,0x00,0x00,
                               0x41,0x08, 0x28,0x02,0x00, 0x0b };
    val("memory.copy then load = 42", b,sizeof b, 42);
    /* memory.init seg0 (d=0,s=0,n=4); load @0 -> 0xDDCCBBAA */
    static const uint8_t c[]={ 0x41,0x00, 0x41,0x00, 0x41,0x04, 0xfc,0x08,0x00,0x00,
                               0x41,0x00, 0x28,0x02,0x00, 0x0b };
    val("memory.init then load = 0xDDCCBBAA", c,sizeof c, (int)0xDDCCBBAA);
    /* data.drop seg0; memory.init seg0 -> trap */
    static const uint8_t d[]={ 0xfc,0x09,0x00,
                               0x41,0x00, 0x41,0x00, 0x41,0x04, 0xfc,0x08,0x00,0x00,
                               0x41,0x00, 0x28,0x02,0x00, 0x0b };
    val("memory.init after data.drop traps", d,sizeof d, TRAP);
    /* fill past the end -> OOB trap (d=65536 == mem_bytes, n=4) */
    static const uint8_t e[]={ 0x41,0x80,0x80,0x04, 0x41,0x00, 0x41,0x04,
                               0xfc,0x0b,0x00, 0x41,0x00, 0x28,0x02,0x00, 0x0b };
    val("memory.fill OOB traps", e,sizeof e, TRAP);
    printf("\nbulk memory (memory.fill/copy/init, data.drop) interp == JIT: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
