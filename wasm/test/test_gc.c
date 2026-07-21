// test_gc.c — the WASM GC shell over Immix: rtt-driven generic tracing, mark from
// roots, transitive closure, data survival, and reclamation of unreachable objects.
#include "jav_gc.h"
#include <stdio.h>
#include <string.h>

/* the real gc_rtt_t for the leaf; an ABI-exact mirror with a trailing ref_offsets[] for the ref-carrying one
 * (a FAM can't sit mid-struct under -Werror, so the mirror restates gc_rtt_t's layout field-for-field).
 * NOTE: this is the STANDALONE collector unit test (IMMIX_TESTS — links the GC only, no engine), so
 * synthetic RTTs are the only option here and a build_rtts defect is out of its reach by construction.
 * That seam is covered by test_gc_roots_real.c. */
typedef struct { uint32_t size, nrefs; uint8_t kind, elem_is_ref, elem_store_w; int32_t gid; uint32_t off[2]; } rtt_ref2_t;
static const gc_rtt_t  LEAF = { .size = (uint32_t)(sizeof(gc_obj_t) + 8), .kind = GC_KIND_STRUCT, .gid = -1 };  /* one i64 payload */
static const rtt_ref2_t PAIR = { .size = (uint32_t)(sizeof(gc_obj_t) + 16), .nrefs = 2, .kind = GC_KIND_STRUCT, .gid = -1,  /* two refs */
                                 .off = { (uint32_t)(sizeof(gc_obj_t) + 0), (uint32_t)(sizeof(gc_obj_t) + 8) } };
#define RTT(x) ((const gc_rtt_t*)&(x))
#define GALLOC(h,x) gc_alloc((h), RTT(x), (x).size)

static gc_obj_t* g_root;   /* the test's single root */
static void enum_roots(gc_heap_t* h, gc_root_visit_fn visit, void* ctx){ (void)h; if (g_root) visit(&g_root, ctx); }

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-48s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

int main(void){
    gc_heap_t h; gc_heap_init(&h, enum_roots, NULL);

    /* P -> {A, B}; C unreachable. A/B carry distinct payloads. */
    gc_obj_t* P = GALLOC(&h, PAIR);
    gc_obj_t* A = GALLOC(&h, LEAF);
    gc_obj_t* B = GALLOC(&h, LEAF);
    gc_obj_t* C = GALLOC(&h, LEAF);
    CK(P && A && B && C, "alloc 4 objects");
    gc_obj_t** pf = (gc_obj_t**)gc_obj_payload(P);
    pf[0] = A; pf[1] = B;
    *(int64_t*)gc_obj_payload(A) = 0xAAAA;
    *(int64_t*)gc_obj_payload(B) = 0xBBBB;
    g_root = P;

    gc_collect(&h);
    CK(gc_obj_live(&h, P) && gc_obj_live(&h, A) && gc_obj_live(&h, B), "P, A, B reachable -> live");
    CK(!gc_obj_live(&h, C), "C unreachable -> not marked");
    /* P's fields still reach A, B (updated if evacuated); payloads intact */
    pf = (gc_obj_t**)gc_obj_payload(P);
    CK(pf[0]==A && pf[1]==B, "P's reference fields intact after collect");
    CK(*(int64_t*)gc_obj_payload(A)==0xAAAA && *(int64_t*)gc_obj_payload(B)==0xBBBB, "leaf payloads survive");

    /* a second collection is stable */
    gc_collect(&h);
    CK(gc_obj_live(&h, P) && gc_obj_live(&h, A) && gc_obj_live(&h, B), "second collect: still live");

    /* drop the root -> everything unreachable -> the block is reclaimed to free */
    g_root = NULL;
    size_t total = imx_space_total_blocks(&h.space);
    gc_collect(&h);
    CK(imx_space_free_blocks(&h.space) == total && total >= 1, "drop root: all blocks reclaimed to free");

    gc_heap_destroy(&h);
    printf("\nwasm GC (Immix shell): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
