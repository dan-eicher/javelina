// test_select.c — the untyped `select` (0x1b), both tiers. WASM 3.0 §4.6.1 (execution:
// pop i32 c, then keep v1 if c≠0 else v2 — value-polymorphic over the whole slot) and
// §3.4.1 (validation: t t i32 -> t, t a number/vector type; reference operands and
// mismatched operand types are REJECTED — those need the typed `select t`).
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t RES_I32[1] = { WVT_I32 };
#define VALFAIL (-999)
static int run(const uint8_t* code, size_t n, int jit){
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES_I32; cx.nresults=1;
    if (!jav_typecheck(code,n,&cx,&st,&k)) return VALFAIL;
    f[0]=(jav_func_t){.code=code,.code_len=n,.num_results=1,.sidetable=st};
    struct heap_t heap; memset(&heap,0,sizeof heap);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1; vm.heap=&heap;
    bbq_ctx_init(&vm.frame.code,code,n); vm.frame.sidetable=st;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? -0x6BADBAD : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st);
    return r;
}

typedef struct { uint8_t b[128]; size_t n; } buf_t;
static void eb(buf_t* p, uint8_t x){ p->b[p->n++]=x; }
static void uleb(buf_t* p, uint64_t v){ do{ uint8_t b=v&0x7f; v>>=7; if(v) b|=0x80; eb(p,b);}while(v); }
static void sleb(buf_t* p, int64_t v){ int more=1; while(more){ uint8_t b=v&0x7f; v>>=7;
    if((v==0 && !(b&0x40)) || (v==-1 && (b&0x40))) more=0; else b|=0x80; eb(p,b);} }
static void i32c(buf_t* p, int32_t v){ eb(p,0x41); sleb(p,v); }
static void i64c(buf_t* p, int64_t v){ eb(p,0x42); sleb(p,v); }
static void f64c(buf_t* p, double v){ eb(p,0x44); uint8_t t[8]; memcpy(t,&v,8); for(int i=0;i<8;i++) eb(p,t[i]); }
static void v128_lane0(buf_t* p, uint32_t lane0){ eb(p,0xFD); uleb(p,12);    // v128.const
    uint8_t t[16]; memset(t,0,16); memcpy(t,&lane0,4); for(int i=0;i<16;i++) eb(p,t[i]); }

static int fails=0;
static void val(const char* nm, const buf_t* p, int exp){
    int i=run(p->b,p->n,0), j=run(p->b,p->n,1);
    int ok=(i==j && i==exp);
    printf("  %-40s interp=%-12d jit=%-12d exp=%-12d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void){
    buf_t p;
    #define END() eb(&p,0x0b)
    // i32: c=1 keeps v1 (111); c=0 keeps v2 (222)
    p.n=0; i32c(&p,111); i32c(&p,222); i32c(&p,1); eb(&p,0x1b); END();
    val("select i32 c=1 -> v1 (111)", &p, 111);
    p.n=0; i32c(&p,111); i32c(&p,222); i32c(&p,0); eb(&p,0x1b); END();
    val("select i32 c=0 -> v2 (222)", &p, 222);
    // i64 (full value via i64.eq 0x51): c=1 -> v1=-5
    p.n=0; i64c(&p,-5); i64c(&p,99); i32c(&p,1); eb(&p,0x1b); i64c(&p,-5); eb(&p,0x51); END();
    val("select i64 c=1 -> v1 (-5)", &p, 1);
    // f64 (f64.eq 0x61): c=0 -> v2=2.5
    p.n=0; f64c(&p,1.5); f64c(&p,2.5); i32c(&p,0); eb(&p,0x1b); f64c(&p,2.5); eb(&p,0x61); END();
    val("select f64 c=0 -> v2 (2.5)", &p, 1);
    // v128 (whole 16-byte slot): c=1 -> v1; read lane0 via i32x4.extract_lane (0xFD 27 0)
    p.n=0; v128_lane0(&p,0xAAAAAAAA); v128_lane0(&p,0xBBBBBBBB); i32c(&p,1); eb(&p,0x1b);
           eb(&p,0xFD); uleb(&p,27); eb(&p,0); END();
    val("select v128 c=1 -> v1 lane0", &p, (int)0xAAAAAAAA);
    p.n=0; v128_lane0(&p,0xAAAAAAAA); v128_lane0(&p,0xBBBBBBBB); i32c(&p,0); eb(&p,0x1b);
           eb(&p,0xFD); uleb(&p,27); eb(&p,0); END();
    val("select v128 c=0 -> v2 lane0", &p, (int)0xBBBBBBBB);

    // ── §3.4.1 validation negatives ──
    // reference operands are REJECTED (untyped select forbids refs): ref.null func = 0xD0 0x70
    p.n=0; eb(&p,0xD0); eb(&p,0x70); eb(&p,0xD0); eb(&p,0x70); i32c(&p,0); eb(&p,0x1b); eb(&p,0x1a); END();
    val("select on funcref rejected", &p, VALFAIL);
    // mismatched operand types (i32 vs i64) REJECTED
    p.n=0; i32c(&p,1); i64c(&p,2); i32c(&p,0); eb(&p,0x1b); END();
    val("select i32/i64 mismatch rejected", &p, VALFAIL);

    // ── typed select t (0x1c): same execution, a vec(valtype) immediate the JIT walk skips and
    //    the interp handler skips inline. Encoding: 0x1c 0x01 <valtype byte>. ──
    // select_t i32 c=1 -> v1 (111)  [valtype i32 = 0x7F]
    p.n=0; i32c(&p,111); i32c(&p,222); i32c(&p,1); eb(&p,0x1c); eb(&p,0x01); eb(&p,0x7F); END();
    val("select_t i32 c=1 -> v1 (111)", &p, 111);
    // select_t i64 c=0 -> v2 (99)   [valtype i64 = 0x7E]; full value via i64.eq
    p.n=0; i64c(&p,-5); i64c(&p,99); i32c(&p,0); eb(&p,0x1c); eb(&p,0x01); eb(&p,0x7E);
           i64c(&p,99); eb(&p,0x51); END();
    val("select_t i64 c=0 -> v2 (99)", &p, 1);
    // select_t funcref — TYPED select ACCEPTS refs (untyped rejects). [valtype funcref = 0x70]
    //   ref.null func; ref.null func; i32.const 0; select_t funcref; ref.is_null -> 1
    p.n=0; eb(&p,0xD0); eb(&p,0x70); eb(&p,0xD0); eb(&p,0x70); i32c(&p,0);
           eb(&p,0x1c); eb(&p,0x01); eb(&p,0x70); eb(&p,0xD1); END();
    val("select_t funcref accepted -> ref.is_null 1", &p, 1);
    // §3.4.1: a result-type vector of length != 1 is REJECTED (3.0 allows exactly one)
    p.n=0; i32c(&p,1); i32c(&p,2); i32c(&p,0); eb(&p,0x1c); eb(&p,0x02); eb(&p,0x7F); eb(&p,0x7F); END();
    val("select_t count=2 rejected", &p, VALFAIL);

    printf("\nselect (0x1b/0x1c) interp == JIT, spec §4.6.1/§3.4.1: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
