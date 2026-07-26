// test_array_gc.c — GC tracing through ARRAY ELEMENTS. A WASM function builds an
// array<(ref leaf)> holding a leaf{42} and leaves it rooted on the stack; a
// collection must trace the array's element to keep the leaf alive.
#include "interp.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = leaf{ i32 };  $1 = array<(ref $0)> (elements are managed refs) */
static const gc_rtt_t RTT_LEAF = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_STRUCT };
static const gc_rtt_t RTT_ARR  = { .size = (uint32_t)sizeof(gc_obj_t) + 8, .kind = GC_KIND_ARRAY, .elem_is_ref = 1, .elem_store_w = 4 };
static const gc_rtt_t* RTTS[2] = { &RTT_LEAF, &RTT_ARR };

static const jav_valtype_t F_LEAF[1] = { WVT_I32 };  static const uint32_t T_LEAF[1] = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {F_LEAF,T_LEAF,1}, {0} };   /* [0]=leaf; [1] unused */
static const jav_arraytype_t  ARRAYTYPES[2]  = { {0}, { WVT_REF, 0 } }; /* [1]=array<(ref $0)> */
static const jav_valtype_t RES[1] = { WVT_REF };
static const uint32_t RES_T[1] = { 1 };

static const uint8_t LKINDS[2] = { WST_STRUCT, WST_ARRAY };
static const int32_t LSUP[2] = { -1, -1 };
static const jav_subtype_ctx_t LAT = { LKINDS, LSUP, 2 };

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-46s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    /* const 42; struct.new $0 (leaf); const 1 (len); array.new $1 (array<ref>, init=leaf); end */
    static const uint8_t code[] = { 0x41,0x2a, 0xfb,0x00,0x00, 0x41,0x01, 0xfb,0x06,0x01, 0x0b };
    jav_func_t f[1]; memset(f,0,sizeof f);
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx={0}; cx.results=RES; cx.result_tidx=RES_T; cx.nresults=1;
                        cx.structtypes=STRUCTTYPES; cx.nstructtypes=2; cx.arraytypes=ARRAYTYPES; cx.narraytypes=2; cx.lattice=&LAT;
    CK(jav_typecheck(code,sizeof code,&cx,&st,&k), "validates: array<(ref leaf)>");
    f[0]=(jav_func_t){.code=code,.code_len=sizeof code,.num_results=1,.sidetable=st};
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.cluster.functions=f; vm.cluster.num_functions=1;
    vm.cluster.struct_rtts=RTTS; vm.cluster.num_struct_rtts=2;
    struct heap_t heap; memset(&heap,0,sizeof heap); vm.heap=&heap; jav_heap_gc_init(&heap,&vm);
    bbq_ctx_init(&vm.frame.code,code,sizeof code); vm.frame.sidetable=st;
    interp_run(&vm,&heap);
    gc_heap_t* gh = heap.gc.self;

    heap.gc.collect(heap.gc.self);                 /* array rooted; leaf survives ONLY if the element is traced */
    gc_obj_t* arr  = (gc_obj_t*)(uintptr_t)vm.frame.stack[0].l;
    gc_obj_t* leaf = (gc_obj_t*)((slot_t*)((uint8_t*)gc_obj_payload(arr) + 8))[0].l;   /* element 0 (after the length) */
    CK(gc_obj_live(gh, arr) && gc_obj_live(gh, leaf), "array + leaf (traced via element) survive");
    CK(((slot_t*)gc_obj_payload(leaf))[0].i == 42, "leaf payload reached through the traced element");

    vm.frame.sp = 0; vm.frame.stack_types[0] = T_VOID;
    heap.gc.collect(heap.gc.self);
    CK(imx_space_all_reclaimed(&gh->space), "drop root -> array + leaf reclaimed (no live lines)");

    jav_heap_gc_destroy(&heap); jav_vm_free(&vm); bbq_vec_free(st);
    printf("\nGC tracing through array elements: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
