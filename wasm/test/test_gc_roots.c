// test_gc_roots.c — the GC wired into the runtime: the collector (behind heap->gc)
// drives a collection using the vm's roots (managed T_GCREF slots on the value
// stack). A rooted object + everything it reaches survives; dropping the root
// reclaims it. Exercises the heap_t seam + jav_gc_enum_roots end to end.
#include "interp.h"
#include "heap.h"
#include <stdio.h>
#include <string.h>

/* ABI-exact mirror of gc_rtt_t with a trailing ref_offsets[] (a FAM can't sit mid-struct under -Werror).
 * NOTE: this is a COLLECTOR unit test — the RTTs here are synthetic and the engine never produces
 * them, so nothing in this file can see a build_rtts defect. That seam is covered by
 * test_gc_roots_real.c, which drives real modules through the real RTT builder. */
typedef struct { uint32_t size, nrefs; uint16_t nfields; uint8_t kind, elem_is_ref, elem_store_w, elem_heap_w; const uint32_t* field_off; int32_t gid; uint32_t off[2]; } rtt_ref2_t;
static const gc_rtt_t  LEAF = { .size = (uint32_t)(sizeof(gc_obj_t) + 8), .kind = GC_KIND_STRUCT, .gid = -1 };
static const rtt_ref2_t PAIR = { .size = (uint32_t)(sizeof(gc_obj_t) + 16), .nrefs = 2, .kind = GC_KIND_STRUCT, .gid = -1,
                                 .off = { (uint32_t)(sizeof(gc_obj_t) + 0), (uint32_t)(sizeof(gc_obj_t) + 8) } };
#define RTT(x) ((const gc_rtt_t*)&(x))
#define GNEW(vm,x) jav_gc_new((vm), RTT(x), (x).size)

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-46s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    struct heap_t heap; memset(&heap,0,sizeof heap);
    vm.heap = &heap;
    jav_heap_gc_init(&heap, &vm);          /* bind the collector; roots = this vm */
    gc_heap_t* gh = heap.gc.self;

    /* P -> {A, B}; A carries a payload. Allocate through the heap's collector. */
    gc_obj_t* A = GNEW(&vm, LEAF);
    gc_obj_t* B = GNEW(&vm, LEAF);
    gc_obj_t* P = GNEW(&vm, PAIR);
    CK(A && B && P, "jav_gc_new through heap->gc.alloc");
    gc_obj_t** pf = (gc_obj_t**)gc_obj_payload(P); pf[0]=A; pf[1]=B;
    *(int64_t*)gc_obj_payload(A) = 0x1234;

    /* root P: put it on the value stack, tagged T_GCREF (an 8-byte pointer in .l) */
    vm.frame.stack[0].l = (s8)(uintptr_t)P;
    vm.frame.stack_types[0] = T_GCREF;
    vm.frame.sp = 1;

    heap.gc.collect(heap.gc.self);          /* collect via the seam — uses jav_gc_enum_roots */
    P  = (gc_obj_t*)(uintptr_t)vm.frame.stack[0].l;   /* re-read (collector may relocate) */
    pf = (gc_obj_t**)gc_obj_payload(P);
    CK(gc_obj_live(gh, P) && gc_obj_live(gh, pf[0]) && gc_obj_live(gh, pf[1]),
       "rooted P + reachable A,B survive the root scan");
    CK(*(int64_t*)gc_obj_payload(pf[0]) == 0x1234, "payload intact after collect");

    /* drop the root -> nothing reachable -> all blocks reclaimed */
    vm.frame.sp = 0; vm.frame.stack_types[0] = T_VOID;
    size_t total = imx_space_total_blocks(&gh->space);
    heap.gc.collect(heap.gc.self);
    CK(imx_space_all_reclaimed(&gh->space) && total >= 1, "drop root -> no block holds live data");

    jav_heap_gc_destroy(&heap);
    jav_vm_free(&vm);
    printf("\nGC wired into the runtime (root scan via heap_t): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
