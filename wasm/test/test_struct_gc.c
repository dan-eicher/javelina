// test_struct_gc.c — GC tracing THROUGH a struct reference field. A WASM function
// builds box{ ref -> leaf{42} } and leaves the box on the stack (a managed root);
// a collection must trace the box's ref field and keep the leaf alive. This is the
// point of GC: reachability through object fields, driven by the rtt ref-offset map.
#include "interp.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = leaf{ i32 } (no refs);  $1 = box{ (ref $0) } (one ref field at payload+0) */
/* ABI-exact mirror of gc_rtt_t with a trailing ref_offsets[] (a FAM can't sit mid-struct under -Werror).
 * NOTE: synthetic RTTs — the engine never produces these, so a build_rtts defect is invisible here.
 * That seam is covered by test_gc_roots_real.c, which drives real modules through the real builder. */
typedef struct { uint32_t size, nrefs; uint16_t nfields; uint8_t kind, elem_is_ref, elem_store_w, elem_heap_w; const uint32_t* field_off; int32_t gid; uint32_t off[1]; } rtt_ref1_t;
static const gc_rtt_t  RTT_LEAF = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT, .gid = -1 };
static const rtt_ref1_t RTT_BOX = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .nrefs = 1, .kind = GC_KIND_STRUCT, .gid = -1, .off = { (uint32_t)sizeof(gc_obj_t) } };
static const gc_rtt_t* STRUCT_RTTS[2] = { &RTT_LEAF, (const gc_rtt_t*)&RTT_BOX };

static const jav_valtype_t F_LEAF[1] = { WVT_I32 };       static const uint32_t T_LEAF[1] = { 0 };
static const jav_valtype_t F_BOX[1]  = { WVT_REF };  static const uint32_t T_BOX[1]  = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {F_LEAF,T_LEAF,1}, {F_BOX,T_BOX,1} };
static const jav_valtype_t RES[1] = { WVT_REF };
static const uint32_t RES_T[1] = { 1 };

static const uint8_t LKINDS[2] = { WST_STRUCT, WST_STRUCT };
static const int32_t LSUP[2] = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-46s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    /* const 42; struct.new $0 (leaf); struct.new $1 (box, popping the leaf ref); end */
    static const uint8_t code[] = { 0x41,0x2a, 0xfb,0x00,0x00, 0xfb,0x00,0x01, 0x0b };
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES; cx.result_tidx=RES_T; cx.nresults=1; cx.structtypes=STRUCTTYPES; cx.nstructtypes=2; cx.lattice=&LAT;
    CK(jav_typecheck(code,sizeof code,&cx,&st,&k), "validates: box{ref->leaf}");
    f[0]=(jav_func_t){.code=code,.code_len=sizeof code,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=STRUCT_RTTS; vm.cluster.num_struct_rtts=2;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,sizeof code); vm.frame.sidetable=st;
    interp_run(&vm,&heap);                                  /* builds the graph; box left on the stack */
    gc_heap_t* gh = heap.gc.self;

    /* collect with the box rooted on the value stack — the leaf survives ONLY if the
     * box's ref field is traced. */
    heap.gc.collect(heap.gc.self);
    gc_obj_t* box  = (gc_obj_t*)(uintptr_t)vm.frame.stack[0].l;     /* re-read (collector may relocate) */
    gc_obj_t* leaf = (gc_obj_t*)((slot_t*)gc_obj_payload(box))[0].l;
    CK(gc_obj_live(gh, box) && gc_obj_live(gh, leaf), "box + leaf (traced via ref field) survive");
    CK(((slot_t*)gc_obj_payload(leaf))[0].i == 42, "leaf payload reached through the traced ref");

    /* drop the root -> both unreachable -> reclaimed */
    vm.frame.sp = 0; vm.frame.stack_types[0] = T_VOID;
    size_t total = imx_space_total_blocks(&gh->space);
    heap.gc.collect(heap.gc.self);
    CK(imx_space_free_blocks(&gh->space) == total, "drop root -> box + leaf reclaimed");

    jav_heap_gc_destroy(&heap); jav_vm_free(&vm); bbq_vec_free(st);
    printf("\nGC tracing through struct ref fields: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
