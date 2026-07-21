// test_gc_packed.c — packed storage fields/elements: struct.get_s/u, array.get_s/u
// sign- vs zero-extend i8 AND i16 storage; plus store-time truncation (the write
// path struct.new/array.new_fixed share with struct.set/array.set). Both tiers.
// Types: $0 struct{i8}  $1 array<i8>  $2 struct{i16}  $3 array<i16>.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define A_SZ ((uint32_t)sizeof(gc_obj_t) + 8)
static const gc_rtt_t RTT_S8  = { .size = A_SZ, .kind = GC_KIND_STRUCT };
static const gc_rtt_t RTT_A8  = { .size = A_SZ, .kind = GC_KIND_ARRAY, .elem_store_w = 1 };
static const gc_rtt_t RTT_S16 = { .size = A_SZ, .kind = GC_KIND_STRUCT };
static const gc_rtt_t RTT_A16 = { .size = A_SZ, .kind = GC_KIND_ARRAY, .elem_store_w = 2 };
static const gc_rtt_t* RTTS[4] = { &RTT_S8, &RTT_A8, &RTT_S16, &RTT_A16 };

static const jav_valtype_t SF[1] = { WVT_I32 };  static const uint32_t ST[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[4] = { {SF,ST,1}, {SF,ST,1}, {SF,ST,1}, {SF,ST,1} };
static const jav_arraytype_t  ARRAYTYPES[4]  = { {WVT_I32,0}, {WVT_I32,0}, {WVT_I32,0}, {WVT_I32,0} };
/* packing widths: $0/$1 = i8 (1), $2/$3 = i16 (2) */
static const uint8_t PK1[1] = { 1 };  static const uint8_t PK2[1] = { 2 };
static const uint8_t* const PACKS[4] = { PK1, PK1, PK2, PK2 };
static const jav_valtype_t RES_I32[1] = { WVT_I32 };

static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=4;
                        cx.arraytypes=ARRAYTYPES; cx.narraytypes=4;
                        cx.type_field_packs=PACKS; cx.num_type_field_packs=4;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=4; vm.cluster.type_field_packs=PACKS; vm.cluster.num_type_field_packs=4;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -999999 : jav_tos(&vm).i;
    jav_heap_gc_destroy(&heap); bbq_vec_free(st);
    return r;
}
static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp){
    int i=run(c,n,0), j=run(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-44s interp=%-6d jit=%-6d exp=%-6d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}
int main(void){
    /* ── i8 ($0 struct, $1 array): sign vs zero extend ── */
    static const uint8_t a[]={ 0x41,0xff,0x01, 0xfb,0x00,0x00, 0xfb,0x03,0x00,0x00, 0x0b };
    val("struct.get_s i8(0xFF) = -1", a,sizeof a, -1);
    static const uint8_t b[]={ 0x41,0xff,0x01, 0xfb,0x00,0x00, 0xfb,0x04,0x00,0x00, 0x0b };
    val("struct.get_u i8(0xFF) = 255", b,sizeof b, 255);
    static const uint8_t c[]={ 0x41,0xff,0x01, 0xfb,0x08,0x01,0x01, 0x41,0x00, 0xfb,0x0c,0x01, 0x0b };
    val("array.get_s i8(0xFF) = -1", c,sizeof c, -1);
    static const uint8_t d[]={ 0x41,0xff,0x01, 0xfb,0x08,0x01,0x01, 0x41,0x00, 0xfb,0x0d,0x01, 0x0b };
    val("array.get_u i8(0xFF) = 255", d,sizeof d, 255);

    /* ── i16 ($2 struct, $3 array): the width the compiler emits for short/char ── */
    /* 0xFFFF: get_s sign-extends to -1, get_u zero-extends to 65535 */
    static const uint8_t e[]={ 0x41,0xff,0xff,0x03, 0xfb,0x00,0x02, 0xfb,0x03,0x02,0x00, 0x0b };
    val("struct.get_s i16(0xFFFF) = -1", e,sizeof e, -1);
    static const uint8_t g[]={ 0x41,0xff,0xff,0x03, 0xfb,0x00,0x02, 0xfb,0x04,0x02,0x00, 0x0b };
    val("struct.get_u i16(0xFFFF) = 65535", g,sizeof g, 65535);
    /* 0x7FFF in range: both extensions give 32767 */
    static const uint8_t h[]={ 0x41,0xff,0xff,0x01, 0xfb,0x00,0x02, 0xfb,0x03,0x02,0x00, 0x0b };
    val("struct.get_s i16(0x7FFF) = 32767", h,sizeof h, 32767);
    static const uint8_t p[]={ 0x41,0xff,0xff,0x03, 0xfb,0x08,0x03,0x01, 0x41,0x00, 0xfb,0x0c,0x03, 0x0b };
    val("array.get_s i16(0xFFFF) = -1", p,sizeof p, -1);
    static const uint8_t q[]={ 0x41,0xff,0xff,0x03, 0xfb,0x08,0x03,0x01, 0x41,0x00, 0xfb,0x0d,0x03, 0x0b };
    val("array.get_u i16(0xFFFF) = 65535", q,sizeof q, 65535);

    /* ── store-time truncation (the pack-write path the compiler relies on) ── */
    /* i8: store 0x1FF (511) → low 8 bits 0xFF → get_u 255 */
    static const uint8_t r[]={ 0x41,0xff,0x03, 0xfb,0x00,0x00, 0xfb,0x04,0x00,0x00, 0x0b };
    val("struct.new i8 trunc 0x1FF -> get_u 255", r,sizeof r, 255);
    /* i16: store 0x1FFFF (131071) → low 16 bits 0xFFFF → get_u 65535 */
    static const uint8_t s[]={ 0x41,0xff,0xff,0x07, 0xfb,0x00,0x02, 0xfb,0x04,0x02,0x00, 0x0b };
    val("struct.new i16 trunc 0x1FFFF -> get_u 65535", s,sizeof s, 65535);
    /* i16 array store-time truncation via array.new_fixed */
    static const uint8_t t[]={ 0x41,0xff,0xff,0x07, 0xfb,0x08,0x03,0x01, 0x41,0x00, 0xfb,0x0d,0x03, 0x0b };
    val("array.new_fixed i16 trunc 0x1FFFF -> get_u 65535", t,sizeof t, 65535);

    printf("\npacked storage get_s/get_u i8+i16 + store-trunc (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
