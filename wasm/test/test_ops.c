#include "interp.h"
#include <stdio.h>
#include <string.h>
static int run(const uint8_t* code, size_t n, slot_t* r, u1* t){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, n);
    jav_status_t s = interp_run(&vm,NULL);
    *r=jav_tos(&vm); *t=jav_tos_type(&vm); return s==JAV_RETURN;
}
static int fails=0;
static void ck(const char* nm,int ok){ printf("  %-26s [%s]\n",nm,ok?"PASS":"FAIL"); fails+=!ok; }
int main(void){
    slot_t r; u1 t;
    // i32: 1 << 5 = 32   (shl 0x74)
    { uint8_t c[]={0x41,0x01,0x41,0x05,0x74,0x0b}; run(c,sizeof c,&r,&t); ck("i32 1<<5 == 32", r.i==32); }
    // i32: -1 >>> 1 = 0x7FFFFFFF  (shr_u 0x76)
    { uint8_t c[]={0x41,0x7f,0x41,0x01,0x76,0x0b}; run(c,sizeof c,&r,&t); ck("i32 -1>>>1 == 2147483647", (uint32_t)r.i==2147483647u); }
    // i32.clz(1) = 31   (0x67)
    { uint8_t c[]={0x41,0x01,0x67,0x0b}; run(c,sizeof c,&r,&t); ck("i32.clz(1) == 31", r.i==31); }
    // i32.popcnt(255) = 8   (0x69)
    { uint8_t c[]={0x41,0xff,0x01,0x69,0x0b}; run(c,sizeof c,&r,&t); ck("i32.popcnt(255) == 8", r.i==8); }
    // (unsigned)-1 / 2 = 0x7FFFFFFF   (div_u 0x6e)
    { uint8_t c[]={0x41,0x7f,0x41,0x02,0x6e,0x0b}; run(c,sizeof c,&r,&t); ck("i32 0xFFFFFFFF/u 2", (uint32_t)r.i==2147483647u); }
    // i32: (unsigned)1 < (unsigned)-1 ? = 1   (lt_u 0x49)
    { uint8_t c[]={0x41,0x01,0x41,0x7f,0x49,0x0b}; run(c,sizeof c,&r,&t); ck("i32 1 <u 0xFFFFFFFF == 1", r.i==1); }
    // f64.const 16.0; f64.sqrt -> 4.0   (const 0x44, sqrt 0x9f)
    { uint8_t c[]={0x44,0,0,0,0,0,0,0x30,0x40,0x9f,0x0b}; run(c,sizeof c,&r,&t); ck("f64.sqrt(16.0) == 4.0", r.d==4.0 && t==T_DOUBLE); }
    // f64.const 2.0; f64.const 3.0; f64.add -> 5.0   (add 0xa0)
    { uint8_t c[]={0x44,0,0,0,0,0,0,0,0x40, 0x44,0,0,0,0,0,0,0x08,0x40, 0xa0,0x0b}; run(c,sizeof c,&r,&t); ck("f64 2.0+3.0 == 5.0", r.d==5.0); }
    // i64.extend_i32_u then wrap: i32.const -1; i64.extend_i32_u -> 0xFFFFFFFF (low bits)  (0xad)
    { uint8_t c[]={0x41,0x7f,0xad,0x0b}; run(c,sizeof c,&r,&t); ck("i64.extend_i32_u(-1)", (uint64_t)r.l==0xFFFFFFFFull && t==T_LONG); }
    printf("\nnew-machinery execution: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
