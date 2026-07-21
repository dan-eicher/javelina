#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
static int run_interp(const uint8_t* c, size_t n, int32_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=interp_run(&vm,NULL); *r=jav_tos(&vm).i; return s==JAV_RETURN;
}
static int run_jit(const uint8_t* c, size_t n, int32_t* r){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    jav_status_t s=jav_jit_run(&vm); *r=jav_tos(&vm).i; return s==JAV_RETURN;
}
static int fails=0;
struct P { const char* nm; uint8_t code[16]; size_t n; int32_t exp; };
int main(void){
    struct P ps[] = {
        {"i32.add 3+5",     {0x41,0x03,0x41,0x05,0x6a,0x0b},6, 8},
        {"i32.mul 6*7",     {0x41,0x06,0x41,0x07,0x6c,0x0b},6, 42},
        {"i32.and 15&9",    {0x41,0x0f,0x41,0x09,0x71,0x0b},6, 9},
        {"i32.xor 15^9",    {0x41,0x0f,0x41,0x09,0x73,0x0b},6, 6},
        {"i32.shl 1<<5",    {0x41,0x01,0x41,0x05,0x74,0x0b},6, 32},
        {"i32.shr_u -1>>>1",{0x41,0x7f,0x41,0x01,0x76,0x0b},6, 2147483647},
        {"i32.clz(1)",      {0x41,0x01,0x67,0x0b},4, 31},
        {"i32.popcnt(255)", {0x41,0xff,0x01,0x69,0x0b},5, 8},
        {"i32.div_u big/2", {0x41,0x7f,0x41,0x02,0x6e,0x0b},6, 2147483647},
        {"i32.rem_s 7%3",   {0x41,0x07,0x41,0x03,0x6f,0x0b},6, 1},
        {"i32.rem_s -7%3",  {0x41,0x79,0x41,0x03,0x6f,0x0b},6, -1},
        {"i32.rem_s MIN%-1",{0x41,0x80,0x80,0x80,0x80,0x78,0x41,0x7f,0x6f,0x0b},10, 0},  // §4.3 no trap → 0; raw a%b SIGFPEs
        {"i32.lt_u 1<big",  {0x41,0x01,0x41,0x7f,0x49,0x0b},6, 1},
        {"i32.ge_s 5>=5",   {0x41,0x05,0x41,0x05,0x4e,0x0b},6, 1},
    };
    for (size_t i=0;i<sizeof ps/sizeof*ps;i++){
        int32_t ri=0, rj=0; run_interp(ps[i].code,ps[i].n,&ri); run_jit(ps[i].code,ps[i].n,&rj);
        int ok = (ri==ps[i].exp && rj==ps[i].exp && ri==rj);
        printf("  %-20s interp=%-11d jit=%-11d [%s]\n", ps[i].nm, ri, rj, ok?"agree":"DIVERGE"); fails+=!ok;
    }
    printf("\ninterp == JIT across numeric ops: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
