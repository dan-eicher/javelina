#include "interp.h"
#include <stdio.h>
#include <string.h>
// (func (param i32)(result i32) local.get 0; if(i32) i32.const 10 else i32.const 20 end)
//  0:20 1:00 2:04 3:7F 4:41 5:0A 6:05 7:41 8:14 9:0B 10:0B
static const uint8_t code[] = {0x20,0x00, 0x04,0x7f, 0x41,0x0a, 0x05, 0x41,0x14, 0x0b, 0x0b};
// hand-built side-table: [0]=if(cond false->else body), [1]=else(then->end)
static const jav_st_entry_t st[] = { {3,1,0,0}, {2,1,1,0} };
static int run(int32_t cond){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, sizeof code);
    vm.frame.sidetable = st; vm.frame.locals[0].i = cond; vm.frame.num_locals=1;
    interp_run(&vm,NULL); return jav_tos(&vm).i;
}
int main(void){
    int t = run(1), f = run(0), fails=0;
    printf("  if(cond=1) -> %d  [%s]\n", t, t==10?"PASS":"FAIL"); fails+=(t!=10);
    printf("  if(cond=0) -> %d  [%s]\n", f, f==20?"PASS":"FAIL"); fails+=(f!=20);
    printf("\nif/else control machinery: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
