// test_jit_native.c — #35 gate: native-calling opcodes (globals/memory) JIT via
// _HOLE_<name> fn-pointer holes filled from opgen's symbol table. interp == JIT.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "bbq_vec.h"   // vm->globals is a bbq_vec (length rides with it)
#include <stdio.h>
#include <string.h>
// One global slot, BY REFERENCE — both tiers index vm->globals[0][0] (a slot pointer).
#define WITH_GLOBAL(vm) slot_t* gstore=NULL; slot_t** gv=NULL; u1* gt=NULL; slot_t z={0}; \
    bbq_vec_push(gstore,z); bbq_vec_push(gv,&gstore[0]); bbq_vec_push(gt,(u1)0); (vm).cluster.globals=gv; (vm).cluster.global_types=gt
#define FREE_GLOBAL() bbq_vec_free(gstore); bbq_vec_free(gv); bbq_vec_free(gt)
static int interp(const uint8_t* c, size_t n, heap_t* h) {
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code, c, n);
    WITH_GLOBAL(vm); vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;   // identity memidx→heap map
    interp_run(&vm, h); int r = jav_tos(&vm).i; FREE_GLOBAL(); jav_vm_free(&vm); return r;
}
static int jit(const uint8_t* c, size_t n, heap_t* h) {
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code, c, n);
    WITH_GLOBAL(vm); vm.heap = h; vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    jav_jit_run(&vm); int r = jav_tos(&vm).i; FREE_GLOBAL(); jav_vm_free(&vm); return r;
}
int main(void) {
    struct heap_t heap; memset(&heap, 0, sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);
    int fails = 0;
    uint8_t g[] = {0x41,0x07, 0x24,0x00, 0x23,0x00, 0x0b};   // const 7; global.set 0; global.get 0
    int gi = interp(g, sizeof g, &heap), gj = jit(g, sizeof g, &heap);
    printf("  global set/get  interp=%d jit=%d  [%s]\n", gi, gj, (gi==gj&&gi==7)?"PASS":"FAIL"); fails += !(gi==gj&&gi==7);
    uint8_t m[] = {0x41,0x00, 0x41,0x2a, 0x36,0x02,0x00, 0x41,0x00, 0x28,0x02,0x00, 0x0b}; // store 42@0; load@0
    int mi = interp(m, sizeof m, &heap), mj = jit(m, sizeof m, &heap);
    printf("  mem store/load  interp=%d jit=%d  [%s]\n", mi, mj, (mi==mj&&mi==42)?"PASS":"FAIL"); fails += !(mi==mj&&mi==42);
    // §7.1.8 JIT trap-frame offset: the JIT now stamps frame.instr_pc per opcode (the interp does it in
    // jav_next), so a trap leaves the trapping instruction's byte offset in instr_pc. Without the opgen
    // stencil stamp this stays memset-0 ≠ 3 (the latent JIT-frame-offset gap that was deferred).
    {
        uint8_t t[] = {0x41,0x07, 0x1a, 0x00, 0x0b};   // i32.const 7 @0; drop @2; unreachable @3; end @4
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); bbq_ctx_init(&vm.frame.code, t, sizeof t);
        vm.heap = &heap;
        jav_status_t st = jav_jit_run(&vm);
        int ok = (st == JAV_TRAP && vm.frame.instr_pc == 3);
        printf("  JIT trap pc     instr_pc=%u (want 3) status=%d  [%s]\n", vm.frame.instr_pc, (int)st, ok?"PASS":"FAIL");
        fails += !ok;
        jav_vm_free(&vm);
    }
    printf("\nJIT native pointer-holes (interp == JIT): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
