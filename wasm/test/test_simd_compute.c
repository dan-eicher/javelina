// test_simd_compute.c — the 3 SIMD compute ops, both tiers. WASM 3.0 §4.6:
//   f64x2.promote_low_f32x4 (0xFD 95): low 2 f32 lanes -> f64x2
//   f32x4.demote_f64x2_zero (0xFD 94): 2 f64 lanes -> f32 (lanes 0,1); lanes 2,3 = 0
//   i8x16.popcnt            (0xFD 98): per-lane population count
// Inputs are built with v128.const; results checked by extracting a lane (+ a zero lane).
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
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return -999;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1; vm.heap=&heap;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -0x6BADBAD : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st);
    return r;
}
typedef struct { uint8_t b[64]; size_t n; } buf_t;
static void eb(buf_t* p, uint8_t x){ p->b[p->n++]=x; }
static void uleb(buf_t* p, uint64_t v){ do{ uint8_t b=v&0x7f; v>>=7; if(v) b|=0x80; eb(p,b);}while(v); }
static void v128c(buf_t* p, const uint8_t b16[16]){ eb(p,0xFD); uleb(p,12); for(int i=0;i<16;i++) eb(p,b16[i]); }
static void op(buf_t* p, uint8_t sub){ eb(p,0xFD); uleb(p,sub); }
static void extract(buf_t* p, uint8_t sub, uint8_t lane){ eb(p,0xFD); uleb(p,sub); eb(p,lane); }
static void f32c(buf_t* p, float v){ eb(p,0x43); uint8_t t[4]; memcpy(t,&v,4); for(int i=0;i<4;i++) eb(p,t[i]); }
static void f64c(buf_t* p, double v){ eb(p,0x44); uint8_t t[8]; memcpy(t,&v,8); for(int i=0;i<8;i++) eb(p,t[i]); }

static int fails=0;
static void val(const char* nm, const buf_t* p, int exp){
    int i=run(p->b,p->n,0), j=run(p->b,p->n,1);
    int ok=(i==j && i==exp);
    printf("  %-40s interp=%-8d jit=%-8d exp=%-8d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    buf_t p;
    #define END() eb(&p,0x0b)
    uint8_t v[16];
    // promote: f32 lane0 = 1.5 -> f64 lane0 = 1.5  (f64x2.extract_lane = 33, f64.eq = 0x61)
    memset(v,0,16); { float a=1.5f, b=2.5f; memcpy(v,&a,4); memcpy(v+4,&b,4); }
    p.n=0; v128c(&p,v); op(&p,95); extract(&p,33,0); f64c(&p,1.5); eb(&p,0x61); END();
    val("f64x2.promote_low_f32x4 lane0 = 1.5", &p, 1);
    p.n=0; v128c(&p,v); op(&p,95); extract(&p,33,1); f64c(&p,2.5); eb(&p,0x61); END();
    val("f64x2.promote_low_f32x4 lane1 = 2.5", &p, 1);
    // demote: f64 lanes [2.5, 3.5] -> f32 lanes 0,1 = 2.5,3.5 ; lanes 2,3 = 0
    memset(v,0,16); { double a=2.5, b=3.5; memcpy(v,&a,8); memcpy(v+8,&b,8); }
    p.n=0; v128c(&p,v); op(&p,94); extract(&p,31,0); f32c(&p,2.5f); eb(&p,0x5b); END();  // f32x4.extract=31, f32.eq=0x5b
    val("f32x4.demote_f64x2_zero lane0 = 2.5", &p, 1);
    p.n=0; v128c(&p,v); op(&p,94); extract(&p,31,2); f32c(&p,0.0f); eb(&p,0x5b); END();
    val("f32x4.demote_f64x2_zero lane2 = 0", &p, 1);
    // popcnt: byte0=0xFF -> 8 ; byte1=0x0F -> 4   (i8x16.extract_lane_s = 21)
    memset(v,0,16); v[0]=0xFF; v[1]=0x0F; v[2]=0x01;
    p.n=0; v128c(&p,v); op(&p,98); extract(&p,21,0); END();
    val("i8x16.popcnt lane0 (0xFF) = 8", &p, 8);
    p.n=0; v128c(&p,v); op(&p,98); extract(&p,21,1); END();
    val("i8x16.popcnt lane1 (0x0F) = 4", &p, 4);
    p.n=0; v128c(&p,v); op(&p,98); extract(&p,21,2); END();
    val("i8x16.popcnt lane2 (0x01) = 1", &p, 1);

    printf("\nSIMD compute (promote/demote/popcnt) interp == JIT, spec §4.6: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
