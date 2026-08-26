// test_memory64.c — memory64 / table64 end-to-end 64-bit addressing, both tiers (interp == JIT).
// The corpus-invisible truncation cases: an address / table index whose LOW 32 bits are in-bounds
// but whose FULL 64-bit value is out of bounds must TRAP — it must NOT wrap to the low half and
// silently hit the wrong (in-bounds) location. Each address rides on i64.const so the value carries
// the i64 (T_LONG) tag the addrtype-aware pop (GPOP_ADDR / pop_addr) reads.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t RES_I32[1] = { WVT_I32 };
static const uint8_t MEM64[1] = { 1 };          /* memory 0 is 64-bit */
static const uint8_t TBL64[1] = { 1 };          /* table 0 is 64-bit  */

#define TRAP (-0x6BADBAD)
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
                        cx.nmemories=1; cx.mem_is64=MEM64;
                        cx.ntables=1;   cx.table_is64=TBL64;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap);
    jav_mem_add(&heap, 1, 0, 0, 1);                       /* one-page memory64, no max */
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.heap=&heap;  vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    s8* refs=NULL; u1* rtys=NULL;                         /* one-entry table64 (the lone slot is null) */
    bbq_vec_push(refs,(s8)-1); bbq_vec_push(rtys,(u1)T_REF);
    jav_tableinst_t tt={0}; tt.refs=refs; tt.types=rtys; tt.reftype=WVT_REF; tt.is64=1;
    jav_tableinst_t** tabs=NULL; bbq_vec_push(tabs, &tt); vm.cluster.tables=tabs;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(refs); bbq_vec_free(rtys); bbq_vec_free(tabs); bbq_vec_free(st);
    return r;
}
static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=getenv("NOJIT") ? i : run(c,n,1);   /* NOJIT=1: interp only (ASAN can't enter copy-and-patch JIT'd code) */
    int ok=(i==j && i==exp);
    printf("  %-44s interp=%-11d jit=%-11d exp=%-11d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
int main(void){
    /* mem64 round-trip: i32.store 7 @ i64 addr 8; i32.load @8 -> 7 (the address is i64) */
    static const uint8_t a[]={ 0x42,0x08, 0x41,0x07, 0x36,0x02,0x00,
                               0x42,0x08, 0x28,0x02,0x00, 0x0b };
    val("mem64 store/load round-trip = 7", a,sizeof a, 7);
    /* mem64 OOB: store @ i64 2^32 (low32 = 0 in-bounds, full value OOB) -> TRAP, not a write @0 */
    static const uint8_t b[]={ 0x42,0x80,0x80,0x80,0x80,0x10, 0x41,0x07, 0x36,0x02,0x00,
                               0x41,0x00, 0x0b };
    val("mem64 store @2^32 traps (no truncation)", b,sizeof b, TRAP);
    /* mem64 memory.fill @ i64 2^32 -> TRAP (d/n are addrtype-width) */
    static const uint8_t c[]={ 0x42,0x80,0x80,0x80,0x80,0x10, 0x41,0x00, 0x42,0x04, 0xfc,0x0b,0x00,
                               0x41,0x00, 0x0b };
    val("mem64 fill @2^32 traps", c,sizeof c, TRAP);
    /* mem64 memory.size pushes i64 (1 page); i32.wrap_i64 -> 1. The wrap only TYPE-CHECKS if size
       pushed i64 — so this also pins the addrtype-width result tag. */
    static const uint8_t d[]={ 0x3f,0x00, 0xa7, 0x0b };
    val("mem64 memory.size is i64 = 1", d,sizeof d, 1);
    /* table64 OOB: table.set (ref.null) @ i64 index 2^32 (low32 = 0, full OOB) -> TRAP, not a write
       to entry 0. The old truncating pop would index entry 0 and succeed. */
    static const uint8_t e[]={ 0x42,0x80,0x80,0x80,0x80,0x10, 0xd0,0x70, 0x26,0x00, 0x41,0x00, 0x0b };
    val("table64 table.set @2^32 traps", e,sizeof e, TRAP);
    /* table64 table.get @ i64 index 2^32 -> TRAP, then ref.is_null CONSUMES the (missing) result.
       This is the JIT-TRAP-BAIL regression: pre-fix the JIT ran past the internal trap into ref.is_null
       and underflowed the stack -> crash. The stencil now bails to _HOLE_trap when the native sets
       vm->trapped, so it traps cleanly on both tiers. */
    static const uint8_t g[]={ 0x42,0x80,0x80,0x80,0x80,0x10, 0x25,0x00, 0xd1, 0x0b };
    val("table64 table.get @2^32 traps (JIT bails)", g,sizeof g, TRAP);
    printf("\nmemory64/table64 64-bit addressing (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
