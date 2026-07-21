// test_jit_libm.c — #35 libm path: float intrinsics (sqrt/floor) JIT via
// _HOLE_<libmfn> fn-pointer holes (the last external-call surface). interp == JIT.
#include "interp.h"
#include "jit_driver.h"
#include <stdio.h>
#include <string.h>
static float fi(const uint8_t* c, size_t n){ vm_t v; memset(&v,0,sizeof v); jav_vm_init(&v); bbq_ctx_init(&v.frame.code,c,n); interp_run(&v,NULL); return jav_tos(&v).f; }
static float fj(const uint8_t* c, size_t n){ vm_t v; memset(&v,0,sizeof v); jav_vm_init(&v); bbq_ctx_init(&v.frame.code,c,n); jav_jit_run(&v); return jav_tos(&v).f; }
int main(void){
    int fails=0;
    uint8_t sq[]={0x43,0x00,0x00,0x80,0x40, 0x91, 0x0b};      // f32.const 4.0; f32.sqrt -> 2
    float si=fi(sq,sizeof sq), sj=fj(sq,sizeof sq);
    printf("  f32.sqrt(4)     interp=%g jit=%g  [%s]\n", si,sj,(si==sj&&si==2.0f)?"PASS":"FAIL"); fails+=!(si==sj&&si==2.0f);
    uint8_t fl[]={0x43,0x00,0x00,0x60,0xc0, 0x8e, 0x0b};      // f32.const -3.5; f32.floor -> -4
    float li=fi(fl,sizeof fl), lj=fj(fl,sizeof fl);
    printf("  f32.floor(-3.5) interp=%g jit=%g  [%s]\n", li,lj,(li==lj&&li==-4.0f)?"PASS":"FAIL"); fails+=!(li==lj&&li==-4.0f);
    printf("\nJIT libm pointer-holes (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
