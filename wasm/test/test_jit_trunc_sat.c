#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Saturating float->int truncations (the whole 0xFC 0..7 prefix family):
 * differential interp == JIT across both tiers, exercising the generated
 * multi-byte opcode dispatch (prefix -> ULEB subopcode -> sub-table) over
 * sparse subops. Saturating clamps to the integer min/max and maps NaN to 0
 * instead of trapping. Results are read at the slot's signed width, so an
 * unsigned-max saturation reads back as -1 (the all-ones bit pattern). */
#define I32MAX  2147483647
#define I32MIN  (-2147483647 - 1)
#define I64MAX  9223372036854775807LL
#define I64MIN  (-9223372036854775807LL - 1)
#define U32MAX  ((int64_t)(int32_t)0xFFFFFFFFu)             /* reads as -1 */
#define U64MAX  ((int64_t)0xFFFFFFFFFFFFFFFFull)            /* reads as -1 */

static jav_status_t run_interp(const uint8_t* c, size_t n, int is64, int64_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=interp_run(&vm,NULL); *r = is64 ? jav_tos(&vm).l : (int64_t)jav_tos(&vm).i; return s;
}
static jav_status_t run_jit(const uint8_t* c, size_t n, int is64, int64_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=jav_jit_run(&vm); *r = is64 ? jav_tos(&vm).l : (int64_t)jav_tos(&vm).i; return s;
}
/* f32.const v ; 0xFC sub ; end  — sub is a single-byte ULEB (< 128). */
static size_t mk32(uint8_t* b, float  v, uint8_t sub){ b[0]=0x43; memcpy(b+1,&v,4); b[5]=0xFC; b[6]=sub; b[7]=0x0b; return 8; }
static size_t mk64(uint8_t* b, double v, uint8_t sub){ b[0]=0x44; memcpy(b+1,&v,8); b[9]=0xFC; b[10]=sub; b[11]=0x0b; return 12; }

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int is64, int64_t exp){
    int64_t ri=0,rj=0; jav_status_t si=run_interp(c,n,is64,&ri), sj=run_jit(c,n,is64,&rj);
    int ok = si==JAV_RETURN && sj==JAV_RETURN && ri==exp && rj==exp;
    printf("  %-30s interp=%-21lld jit=%-21lld exp=%-21lld [%s]\n",
           nm,(long long)ri,(long long)rj,(long long)exp, ok?"ok":"FAIL"); fails+=!ok;
}

int main(void){
    uint8_t b[16]; size_t n;
    /* subop 0: i32.trunc_sat_f32_s */
    n=mk32(b, 3.7f,     0); val("i32.tsat_f32_s 3.7",   b,n,0, 3);
    n=mk32(b, NAN,      0); val("i32.tsat_f32_s NaN",   b,n,0, 0);
    n=mk32(b, INFINITY, 0); val("i32.tsat_f32_s +inf",  b,n,0, I32MAX);
    n=mk32(b,-INFINITY, 0); val("i32.tsat_f32_s -inf",  b,n,0, I32MIN);
    /* subop 1: i32.trunc_sat_f32_u */
    n=mk32(b, 3.7f,     1); val("i32.tsat_f32_u 3.7",   b,n,0, 3);
    n=mk32(b,-2.5f,     1); val("i32.tsat_f32_u -2.5",  b,n,0, 0);
    n=mk32(b, NAN,      1); val("i32.tsat_f32_u NaN",   b,n,0, 0);
    n=mk32(b, 1.0e30f,  1); val("i32.tsat_f32_u 1e30",  b,n,0, U32MAX);
    /* subop 2: i32.trunc_sat_f64_s */
    n=mk64(b, 2.5,      2); val("i32.tsat_f64_s 2.5",   b,n,0, 2);
    n=mk64(b, 1.0e30,   2); val("i32.tsat_f64_s 1e30",  b,n,0, I32MAX);
    n=mk64(b,-1.0e30,   2); val("i32.tsat_f64_s -1e30", b,n,0, I32MIN);
    /* subop 3: i32.trunc_sat_f64_u */
    n=mk64(b, 4.0e9,    3); val("i32.tsat_f64_u 4e9",   b,n,0, (int64_t)(int32_t)4000000000u); /* u32, reads signed */
    n=mk64(b,-1.0,      3); val("i32.tsat_f64_u -1.0",  b,n,0, 0);
    n=mk64(b, 1.0e30,   3); val("i32.tsat_f64_u 1e30",  b,n,0, U32MAX);
    /* subop 4: i64.trunc_sat_f32_s */
    n=mk32(b, 100.9f,   4); val("i64.tsat_f32_s 100.9", b,n,1, 100);
    n=mk32(b, NAN,      4); val("i64.tsat_f32_s NaN",   b,n,1, 0);
    n=mk32(b, INFINITY, 4); val("i64.tsat_f32_s +inf",  b,n,1, I64MAX);
    n=mk32(b,-INFINITY, 4); val("i64.tsat_f32_s -inf",  b,n,1, I64MIN);
    /* subop 5: i64.trunc_sat_f32_u (f->u64 via native) */
    n=mk32(b, 100.9f,   5); val("i64.tsat_f32_u 100.9", b,n,1, 100);
    n=mk32(b,-1.0f,     5); val("i64.tsat_f32_u -1.0",  b,n,1, 0);
    n=mk32(b, 1.0e30f,  5); val("i64.tsat_f32_u 1e30",  b,n,1, U64MAX);
    /* subop 6: i64.trunc_sat_f64_s */
    n=mk64(b, 123456789.0, 6); val("i64.tsat_f64_s 1.23e8", b,n,1, 123456789);
    n=mk64(b, 1.0e300,  6); val("i64.tsat_f64_s 1e300", b,n,1, I64MAX);
    n=mk64(b,-1.0e300,  6); val("i64.tsat_f64_s -1e300",b,n,1, I64MIN);
    /* subop 7: i64.trunc_sat_f64_u (f->u64 via native) */
    n=mk64(b, 123456789.0, 7); val("i64.tsat_f64_u 1.23e8", b,n,1, 123456789);
    n=mk64(b,-5.0,      7); val("i64.tsat_f64_u -5.0",  b,n,1, 0);
    n=mk64(b, 1.0e300,  7); val("i64.tsat_f64_u 1e300", b,n,1, U64MAX);
    printf("\nsaturating truncations (0xFC 0..7) interp == JIT: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
