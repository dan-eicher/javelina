// test_simd_loadext.c — SIMD widening loads (0xFD 1-6) + load32/64_zero (0xFD 92/93), both
// tiers. WASM 3.0 §4.6.8: v128.loadKxM_sx reads 8 bytes (M lanes of K bits), extend^sx each
// to 2K; v128.loadN_zero reads N/8 bytes into lane 0, zeroes the rest. Verified by storing a
// known pattern (i64.store) then extracting lane 0 (and a zero lane) of the result.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t RES_I32[1] = { WVT_I32 };
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1; cx.nmemories=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1; vm.heap=&heap;  vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -0x6BADBAD : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st);
    return r;
}
typedef struct { uint8_t b[96]; size_t n; } buf_t;
static void eb(buf_t* p, uint8_t x){ p->b[p->n++]=x; }
static void uleb(buf_t* p, uint64_t v){ do{ uint8_t b=v&0x7f; v>>=7; if(v) b|=0x80; eb(p,b);}while(v); }
static void sleb(buf_t* p, int64_t v){ int more=1; while(more){ uint8_t b=v&0x7f; v>>=7;
    if((v==0 && !(b&0x40)) || (v==-1 && (b&0x40))) more=0; else b|=0x80; eb(p,b);} }
static void i32c(buf_t* p, int32_t v){ eb(p,0x41); sleb(p,v); }
static void i64c(buf_t* p, int64_t v){ eb(p,0x42); sleb(p,v); }
static void store64(buf_t* p, int64_t v){ i32c(p,0); i64c(p,v); eb(p,0x37); eb(p,0); eb(p,0); }  // i64.store @0
static void simdload(buf_t* p, uint8_t sub){ i32c(p,0); eb(p,0xFD); uleb(p,sub); eb(p,0); eb(p,0); }
static void extract(buf_t* p, uint8_t sub, uint8_t lane){ eb(p,0xFD); uleb(p,sub); eb(p,lane); }

static int fails=0;
static void val(const char* nm, const buf_t* p, int exp){
    int i=run(p->b,p->n,0), j=run(p->b,p->n,1);
    int ok=(i==j && i==exp);
    printf("  %-38s interp=%-12d jit=%-12d exp=%-12d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    buf_t p; (void)i64c;
    #define END() eb(&p,0x0b)
    // load8x8_s: byte0=0x80 -> i16 lane0 = -128 ; _u -> 128   (i16x8.extract_lane_s = 0xFD 24)
    p.n=0; store64(&p,0x80); simdload(&p,1); extract(&p,24,0); END();
    val("v128.load8x8_s lane0 (0x80) = -128", &p, -128);
    p.n=0; store64(&p,0x80); simdload(&p,2); extract(&p,24,0); END();
    val("v128.load8x8_u lane0 (0x80) = 128", &p, 128);
    // load16x4_s: first i16 = 0x8000 -> i32 lane0 = -32768 ; _u -> 32768  (i32x4.extract_lane = 0xFD 27)
    p.n=0; store64(&p,0x8000); simdload(&p,3); extract(&p,27,0); END();
    val("v128.load16x4_s lane0 (0x8000) = -32768", &p, -32768);
    p.n=0; store64(&p,0x8000); simdload(&p,4); extract(&p,27,0); END();
    val("v128.load16x4_u lane0 (0x8000) = 32768", &p, 32768);
    // load32x2_s: first i32 = 0x80000000 -> i64 lane0 = -2^31 (full via i64.eq 0x51) (i64x2.extract = 0xFD 29)
    p.n=0; store64(&p,(int64_t)0x80000000); simdload(&p,5); extract(&p,29,0); i64c(&p,-2147483648LL); eb(&p,0x51); END();
    val("v128.load32x2_s lane0 sign-extends", &p, 1);
    p.n=0; store64(&p,(int64_t)0x80000000); simdload(&p,6); extract(&p,29,0); i64c(&p,2147483648LL); eb(&p,0x51); END();
    val("v128.load32x2_u lane0 zero-extends", &p, 1);
    // load32_zero: i32 lane0 = stored value, lane1 = 0
    p.n=0; store64(&p,0x12345678); simdload(&p,92); extract(&p,27,0); END();
    val("v128.load32_zero lane0 = value", &p, 0x12345678);
    p.n=0; store64(&p,(int64_t)0xFFFFFFFFFFFFFFFFLL); simdload(&p,92); extract(&p,27,1); END();
    val("v128.load32_zero lane1 = 0", &p, 0);
    // load64_zero: i64 lane0 = stored value (via i64.eq), lane1 = 0
    p.n=0; store64(&p,0x1122334455667788LL); simdload(&p,93); extract(&p,29,0); i64c(&p,0x1122334455667788LL); eb(&p,0x51); END();
    val("v128.load64_zero lane0 = value", &p, 1);
    p.n=0; store64(&p,(int64_t)0xFFFFFFFFFFFFFFFFLL); simdload(&p,93); extract(&p,29,1); i64c(&p,0); eb(&p,0x51); END();
    val("v128.load64_zero lane1 = 0", &p, 1);

    printf("\nSIMD widening loads (0xFD 1-6, 92, 93) interp == JIT, spec §4.6.8: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
