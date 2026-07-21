#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Trapping float->int truncations: differential interp == JIT, plus the exact
 * power-of-two boundaries (which prove the >= vs > bound) and NaN/inf/range
 * traps. The guard is synthesized identically into both tiers. */
static jav_status_t run_interp(const uint8_t* c, size_t n, int is64, int64_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=interp_run(&vm,NULL); *r = is64 ? jav_tos(&vm).l : (int64_t)jav_tos(&vm).i; return s;
}
static jav_status_t run_jit(const uint8_t* c, size_t n, int is64, int64_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=jav_jit_run(&vm); *r = is64 ? jav_tos(&vm).l : (int64_t)jav_tos(&vm).i; return s;
}
static size_t mk32(uint8_t* b, float  v, uint8_t op){ b[0]=0x43; memcpy(b+1,&v,4); b[5]=op; b[6]=0x0b; return 7; }
static size_t mk64(uint8_t* b, double v, uint8_t op){ b[0]=0x44; memcpy(b+1,&v,8); b[9]=op; b[10]=0x0b; return 11; }

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int is64, int64_t exp){
    int64_t ri=0,rj=0; jav_status_t si=run_interp(c,n,is64,&ri), sj=run_jit(c,n,is64,&rj);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && ri==exp && rj==exp;
    printf("  %-26s interp=%-12lld jit=%-12lld exp=%-12lld [%s]\n",
           nm,(long long)ri,(long long)rj,(long long)exp, ok?"ok":"FAIL"); fails+=!ok;
}
static void trap(const char* nm, const uint8_t* c, size_t n, int is64){
    int64_t ri=0,rj=0; jav_status_t si=run_interp(c,n,is64,&ri), sj=run_jit(c,n,is64,&rj);
    int ok = si==JAV_TRAP && sj==JAV_TRAP;
    printf("  %-26s interp=%d jit=%d (want trap)            [%s]\n", nm,si,sj, ok?"ok":"FAIL"); fails+=!ok;
}

int main(void){
    uint8_t b[16]; size_t n;
    /* conversions */
    n=mk32(b, 3.7f,         0xa8); val("i32.trunc_f32_s 3.7",    b,n,0, 3);
    n=mk32(b,-2.9f,         0xa8); val("i32.trunc_f32_s -2.9",   b,n,0,-2);
    n=mk32(b,-2147483648.0f,0xa8); val("i32.trunc_f32_s -2^31",  b,n,0,(int64_t)(int32_t)(-2147483647-1));
    n=mk32(b, 3.0e9f,       0xa9); val("i32.trunc_f32_u 3e9",    b,n,0,(int64_t)(int32_t)3000000000u);
    n=mk64(b, 2.5,          0xaa); val("i32.trunc_f64_s 2.5",    b,n,0, 2);
    n=mk64(b, 4.0e9,        0xab); val("i32.trunc_f64_u 4e9",    b,n,0,(int64_t)(int32_t)4000000000u);
    n=mk32(b, 100.9f,       0xae); val("i64.trunc_f32_s 100.9",  b,n,1, 100);
    n=mk64(b, 123456789.0,  0xb0); val("i64.trunc_f64_s 1.23e8", b,n,1, 123456789);
    /* traps: NaN, inf, out-of-range, exact-boundary, unsigned-negative */
    n=mk32(b, NAN,          0xa8); trap("i32.trunc_f32_s NaN",   b,n,0);
    n=mk32(b, INFINITY,     0xa8); trap("i32.trunc_f32_s +inf",  b,n,0);
    n=mk32(b, 1.0e30f,      0xa8); trap("i32.trunc_f32_s 1e30",  b,n,0);
    n=mk32(b, 2147483648.0f,0xa8); trap("i32.trunc_f32_s 2^31",  b,n,0);
    n=mk32(b,-1.0f,         0xa9); trap("i32.trunc_f32_u -1.0",  b,n,0);
    n=mk64(b, 1.0e30,       0xaa); trap("i32.trunc_f64_s 1e30",  b,n,0);
    n=mk64(b,-5.0,          0xb1); trap("i64.trunc_f64_u -5.0",  b,n,1);
    n=mk64(b, 1.0e30,       0xb0); trap("i64.trunc_f64_s 1e30",  b,n,1);
    printf("\ntrapping truncations interp == JIT: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
