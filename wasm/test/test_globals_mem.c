#include "interp.h"
#include "heap.h"
#include "bbq_vec.h"   // vm->globals is a bbq_vec (length rides with it)
#include <stdio.h>
#include <string.h>
static int run(const uint8_t* c, size_t n, heap_t* h, jav_status_t* s){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code,c,n);
    slot_t* gstore=NULL; slot_t** gv=NULL; u1* gt=NULL; slot_t z={0};   // one global slot, BY REFERENCE
    bbq_vec_push(gstore,z); bbq_vec_push(gv,&gstore[0]); bbq_vec_push(gt,(u1)0);
    vm.cluster.globals=gv; vm.cluster.global_types=gt; vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;   // identity memidx→heap map
    *s=interp_run(&vm,h);
    int r=jav_tos(&vm).i; bbq_vec_free(gstore); bbq_vec_free(gv); bbq_vec_free(gt); jav_vm_free(&vm); return r;
}
int main(void){
    struct heap_t heap; memset(&heap,0,sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);
    jav_status_t s; int fails=0;
    uint8_t g[]={0x41,0xe3,0x00, 0x24,0x00, 0x23,0x00, 0x0b};  // i32.const 99 (sleb E3 00); global.set 0; global.get 0
    int gv=run(g,sizeof g,&heap,&s); printf("  global set/get -> %d [%s]\n",gv,gv==99?"PASS":"FAIL"); fails+=(gv!=99);
    uint8_t m[]={0x41,0x00, 0x41,0x2a, 0x36,0x02,0x00, 0x41,0x00, 0x28,0x02,0x00, 0x0b}; // store 42 @0; load @0
    int mv=run(m,sizeof m,&heap,&s); printf("  mem store/load -> %d [%s]\n",mv,mv==42?"PASS":"FAIL"); fails+=(mv!=42);
    uint8_t oob[]={0x41,0xc0,0x84,0x3d, 0x28,0x02,0x00, 0x0b};  // load @1000000 -> OOB
    run(oob,sizeof oob,&heap,&s); printf("  OOB load -> status=%d [%s]\n",s,s==JAV_TRAP?"PASS":"FAIL"); fails+=(s!=JAV_TRAP);
    printf("\nglobals + memory (via heap_t seam): %s\n",fails?"FAIL":"ALL PASS");
    jav_heap_free_mems(&heap);
    return fails?1:0;
}
