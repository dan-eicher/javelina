#include "interp.h"
#include <stdio.h>
#include <string.h>
// parse locals-vec (groups of count+type), set num_locals, leave cursor at expr
static void enter(vm_t* vm, const uint8_t* code, size_t n){
    memset(vm,0,sizeof *vm); jav_vm_init(vm); bbq_ctx_init(&vm->frame.code,code,n);
    uint32_t ngroups=0; bbq_read_uleb128_u32(&vm->frame.code,&ngroups);
    uint32_t total=0;
    for(uint32_t g=0; g<ngroups; g++){ uint32_t c=0; uint8_t ty; bbq_read_uleb128_u32(&vm->frame.code,&c); bbq_read_u8(&vm->frame.code,&ty); total+=c; }
    vm->frame.num_locals=total;
}
static int fails=0;
static void ck(const char* nm, int ok){ printf("  %-30s [%s]\n",nm,ok?"PASS":"FAIL"); fails+=!ok; }
int main(void){
    vm_t vm;
    // (local i32) i32.const 42; local.set 0; local.get 0  -> 42
    { uint8_t c[]={0x01,0x01,0x7f, 0x41,0x2a, 0x21,0x00, 0x20,0x00, 0x0b};
      enter(&vm,c,sizeof c); interp_run(&vm,NULL); ck("set 0=42; get 0 == 42", jav_tos(&vm).i==42); }
    // (local i32) i32.const 7; local.tee 0; local.get 0; i32.add  -> 14 (tee leaves 7, get 7, add)
    { uint8_t c[]={0x01,0x01,0x7f, 0x41,0x07, 0x22,0x00, 0x20,0x00, 0x6a, 0x0b};
      enter(&vm,c,sizeof c); interp_run(&vm,NULL); ck("tee 0=7 keeps 7; +get0 == 14", jav_tos(&vm).i==14); }
    printf("\nlocals: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
