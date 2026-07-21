// test_jit_trap.c — synthesized-guard traps bail THROUGH the JITed machine (the
// _HOLE_trap continuation -> trap stencil), matching the interpreter. No SIGFPE.
#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
static int trap(const uint8_t* c, size_t n, int jit){
    vm_t v; memset(&v,0,sizeof v); jav_vm_init(&v); bbq_ctx_init(&v.frame.code,c,n);
    jav_status_t s = jit ? jav_jit_run(&v) : interp_run(&v,NULL);
    return (s==JAV_TRAP && v.trapped);
}
static int fails=0;
#define CK(label, ...) do{ uint8_t b[]={__VA_ARGS__}; int i=trap(b,sizeof b,0), j=trap(b,sizeof b,1); \
  int ok=(i==j && i==1); printf("  %-22s interp_trap=%d jit_trap=%d [%s]\n",label,i,j,ok?"PASS":"FAIL"); fails+=!ok; }while(0)
int main(void){
    CK("i32.div_s 10/0",   0x41,0x0a, 0x41,0x00, 0x6d, 0x0b);
    CK("i64.div_s 10/0",   0x42,0x0a, 0x42,0x00, 0x7f, 0x0b);
    CK("i32.div_s MIN/-1", 0x41,0x80,0x80,0x80,0x80,0x78, 0x41,0x7f, 0x6d, 0x0b);  // overflow guard
    printf("\nJIT traps-as-continuations (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
