// test_probe.c — §3.3.3 instrumentation seam: vm->probe fires before each interp opcode with the op
// about to run, so an embedder can stop/inspect state per bytecode (the §1.1 reason for an interp tier).
// A branchless const/const/add body needs no side-table; the probe records the op stream. The seam is
// opt-in — a NULL probe leaves the normal run untouched.
#include "interp.h"
#include "heap.h"
#include <stdio.h>
#include <string.h>

static u1 g_ops[64]; static int g_nops = 0;
static void rec_probe(vm_t* vm, u1 op) { (void)vm; if (g_nops < 64) g_ops[g_nops++] = op; }

int main(void) {
    int fails = 0;
    static const uint8_t code[] = { 0x41,0x05, 0x41,0x03, 0x6a };   // i32.const 5; i32.const 3; i32.add
    struct heap_t heap; memset(&heap, 0, sizeof heap);

    // (1) probe installed: it fires before const, const, add — in order — and the run still yields 8.
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, sizeof code);
    vm.probe = rec_probe;
    interp_run(&vm, &heap);
    int ok = (jav_tos(&vm).i == 8) && g_nops == 3 && g_ops[0] == 0x41 && g_ops[1] == 0x41 && g_ops[2] == 0x6a;
    printf("  probe fires before each opcode  result=%d nops=%d ops=%02x,%02x,%02x [%s]\n",
           jav_tos(&vm).i, g_nops, g_ops[0], g_ops[1], g_ops[2], ok ? "PASS" : "FAIL");
    fails += !ok;

    // (2) no probe → opt-in, no-op: a NULL probe records nothing and the run is unchanged.
    g_nops = 0;
    vm_t vm2; memset(&vm2, 0, sizeof vm2); jav_vm_init(&vm2);
    bbq_ctx_init(&vm2.frame.code, code, sizeof code);   // vm2.probe stays NULL
    interp_run(&vm2, &heap);
    int ok2 = (jav_tos(&vm2).i == 8 && g_nops == 0);
    printf("  no probe installed -> no-op       result=%d nops=%d [%s]\n", jav_tos(&vm2).i, g_nops, ok2 ? "PASS" : "FAIL");
    fails += !ok2;

    jav_vm_free(&vm); jav_vm_free(&vm2);
    printf("\nprobe seam (§3.3.3 stop-before instrumentation): %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
