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
typedef struct { uint32_t size, nrefs; uint16_t nfields; uint8_t kind, elem_is_ref, elem_store_w, elem_heap_w; const uint32_t* field_off; int32_t gid; uint32_t off[2]; } rtt_ref2_t;
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
    CK(imx_space_all_reclaimed(&h.space) && total >= 1, "drop root: no block holds live data");

    /* Per-RTT element stride. Every array type stores 8 bytes per element in the
     * heap EXCEPT a v128 array, which stores 16 (jav_module_index.c sets
     * elem_heap_w to the element width when it exceeds 8). gc_obj_size has to
     * read it: an object sized with the default stride is HALF its real extent,
     * so the collector would mark it and then let the allocator hand out its
     * tail. The visible consequence is that the LOS boundary sits at a different
     * LENGTH for a V128[] than for anything else, and both are pinned here —
     * these are the exact lengths conformance/src/Simd.java builds. */
    {
        static const gc_rtt_t V128ARR = { .size = (uint32_t)(sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET),
                                          .kind = GC_KIND_ARRAY, .elem_heap_w = 16, .gid = -1 };
        static const gc_rtt_t REFARR  = { .size = (uint32_t)(sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET),
                                          .kind = GC_KIND_ARRAY, .elem_heap_w = 8,  .gid = -1 };
        const uint32_t base = (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET;

        gc_obj_t* v = gc_alloc(&h, RTT(V128ARR), base + 2006u * 16u);
        gc_obj_t* r = gc_alloc(&h, RTT(REFARR),  base + 4012u * 8u);
        CK(v && r, "alloc a v128-stride and an 8-byte-stride array");
        *(uint32_t*)((uint8_t*)v + sizeof(gc_obj_t)) = 2006u;
        *(uint32_t*)((uint8_t*)r + sizeof(gc_obj_t)) = 4012u;

        CK(gc_obj_size(v) == base + 2006u * 16u, "gc_obj_size reads elem_heap_w (v128: 16 bytes/elem)");
        CK(gc_obj_size(r) == base + 4012u * 8u,  "gc_obj_size: the 8-byte default stride");
        CK(gc_obj_size(v) == IMX_MEDIUM_MAX, "V128[2006] is exactly the largest medium object");
        CK(gc_obj_size(r) == IMX_MEDIUM_MAX, "Object[4012] is exactly the largest medium object");
        CK(base + 2007u * 16u > IMX_MEDIUM_MAX && base + 4013u * 8u > IMX_MEDIUM_MAX,
           "one more element on either crosses into the LOS");
    }

    gc_heap_destroy(&h);
    printf("\nwasm GC (Immix shell): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
