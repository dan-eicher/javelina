// test_carrier_addr_tag.c — the `any` CARRIER vs the tag-driven addrtype pop.
//
// `array.get`/`struct.get` declare `-- any result` (a CARRIER signature: arity is honest, the slot
// type is not). Their runtime tag is therefore computed by ARRAY_GET/STRUCT_GET (jav_frame.h) as
// `elem_is_ref ? T_GCREF : T_INT` — a BINARY ref/non-ref classification that cannot express i64.
//
// GPOP_ADDR (gen_interp.c) dispatches on that tag: `if (tag == T_LONG) full 64-bit else (u4) trunc`.
// So an i64 that reaches an addrtype operand through a carrier arrives tagged T_INT and is
// TRUNCATED to 32 bits — the exact truncation test_memory64.c exists to forbid.
//
// test_memory64.c pins this only for `i64.const` addresses; its own header says so ("Each address
// rides on i64.const so the value carries the i64 (T_LONG) tag the addrtype-aware pop reads"). Every
// other i64 producer is unpinned. These cases route the SAME address through a carrier instead.
//
// Each case first stores 0xDEAD at address 0, so a truncating implementation does not merely differ
// — it returns 0xDEAD where the spec requires a trap, which is the signal.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = array<i64> (elements are NOT refs); $1 = struct { i64 } */
static const gc_rtt_t RTT_ARR = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY,
                                  .elem_is_ref = 0, .elem_store_w = 8 };
static const gc_rtt_t RTT_STR = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT };
static const gc_rtt_t* RTTS[2] = { &RTT_ARR, &RTT_STR };

static const jav_valtype_t F_I64[1] = { WVT_I64 };  static const uint32_t T_I64[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {0}, {F_I64, T_I64, 1} };   /* [1] = struct { i64 } */
static const jav_arraytype_t  ARRAYTYPES[2]  = { { WVT_I64, 0 }, {0} };      /* [0] = array<i64>   */

static const uint8_t LKINDS[2] = { WST_ARRAY, WST_STRUCT };
static const int32_t LSUP[2]   = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static const jav_valtype_t RES_I32[1] = { WVT_I32 };
static const uint8_t MEM64[1] = { 1 };          /* memory 0 is 64-bit */

#define TRAP (-0x6BADBAD)
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
                        cx.nmemories=1; cx.mem_is64=MEM64;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=2;
                        cx.arraytypes=ARRAYTYPES;   cx.narraytypes=2;  cx.lattice=&LAT;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap);
    jav_mem_add(&heap, 1, 0, 0, 1);                       /* one-page memory64, no max */
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=2;
    vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); jav_heap_free_mems(&heap); jav_vm_free(&vm); bbq_vec_free(st);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=getenv("NOJIT") ? i : run(c,n,1);   /* NOJIT=1: interp only (ASAN can't enter JIT'd code) */
    int ok=(i==j && i==exp);
    printf("  %-52s interp=%-11d jit=%-11d exp=%-11d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

/* i32.store 0xDEAD @ i64 address 0 — the value a truncating pop would wrongly return */
#define POISON  0x42,0x00, 0x41,0xAD,0xBD,0x03, 0x36,0x02,0x00
#define ADDR_2P32   0x42,0x80,0x80,0x80,0x80,0x10   /* i64.const 2^32: low 32 bits are IN bounds */

int main(void){
    /* Control: the same address as an i64.const traps. Pins that the address itself is OOB, so a
       carrier case that does NOT trap differs only in how the value was produced. */
    static const uint8_t a[]={ POISON, ADDR_2P32, 0x28,0x02,0x00, 0x0b };
    val("i64.const addr @2^32 traps (control)", a,sizeof a, TRAP);

    /* Positive control: an IN-BOUNDS address through the array carrier still loads. Pins that the
       array.new/array.get plumbing works, so case 3 failing is about the TAG, not the plumbing. */
    static const uint8_t b[]={ POISON, 0x42,0x00, 0x41,0x01, 0xfb,0x06,0x00,
                               0x41,0x00, 0xfb,0x0b,0x00, 0x28,0x02,0x00, 0x0b };
    val("array.get addr @0 loads 0xDEAD (control)", b,sizeof b, 0xDEAD);

    /* The carrier cases: identical address to the control, produced through `-- any result`. */
    static const uint8_t c[]={ POISON, ADDR_2P32, 0x41,0x01, 0xfb,0x06,0x00,
                               0x41,0x00, 0xfb,0x0b,0x00, 0x28,0x02,0x00, 0x0b };
    val("array.get addr @2^32 traps (no truncation)", c,sizeof c, TRAP);

    static const uint8_t d[]={ POISON, ADDR_2P32, 0xfb,0x00,0x01,
                               0xfb,0x02,0x01,0x00, 0x28,0x02,0x00, 0x0b };
    val("struct.get addr @2^32 traps (no truncation)", d,sizeof d, TRAP);

    printf("\ncarrier `any` result vs addrtype tag (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
