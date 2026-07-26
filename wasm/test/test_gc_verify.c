// test_gc_verify.c — the heap-invariant checker (the ORACLE).
//
// "It didn't crash" proves nothing about a collector: corruption surfaces two or three
// collections later as a mangled graph, long after the cause is gone. gc_verify walks the
// REACHABLE graph after a collection and names the first invariant that fails.
//
// The point of this file is the second half: every check is DELIBERATELY BROKEN here and the
// checker must name the right invariant. A checker that has only ever returned true is
// indistinguishable from `return true;`.
#include "jav_gc.h"
#include <stdio.h>
#include <string.h>

/* node { (ref next) @ payload+0 ; i64 val @ payload+8 } */
typedef struct { uint32_t size, nrefs; uint16_t nfields; uint8_t kind, elem_is_ref, elem_store_w, elem_heap_w; const uint32_t* field_off; int32_t gid; uint32_t off[1]; } rtt_ref1_t;
static const rtt_ref1_t NODE_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + 16, .nrefs = 1, .kind = GC_KIND_STRUCT, .gid = -1,
    .off = { (uint32_t)sizeof(gc_obj_t) }
};
#define NODE ((const gc_rtt_t*)&NODE_S)

/* a LOS-sized ref array: elements are managed refs */
static const gc_rtt_t ARR_S = {
    .size = (uint32_t)sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET, .kind = GC_KIND_ARRAY,
    .elem_is_ref = 1, .elem_store_w = 8, .gid = -1
};
#define ARR (&ARR_S)

static gc_obj_t* g_root;

/* A root that appears ONLY on the checker's walk, to exercise the reporting path (case 7).
 * gc_collect enumerates the roots once to mark, then gc_verify enumerates them again; handing
 * the rogue out on the second pass means the collection completes on a sound heap and the
 * VERIFICATION is what fails — the sequence the handler exists for. It must not be visible to
 * the tracer: it lives outside every block, so marking it would write line marks through a
 * fabricated block header. If gc_collect ever enumerates roots more than once this test goes
 * red rather than quiet, which is the right direction to fail in. */
static char      g_rogue_mem[256] __attribute__((aligned(IMX_OBJECT_ALIGN)));
static gc_obj_t* g_rogue;
static int       g_rogue_armed, g_rogue_pass;
static uint32_t  g_rogue_epoch;

static void enum_roots(gc_heap_t* h, gc_root_visit_fn visit, void* ctx) {
    (void)h; if (g_root) visit(&g_root, ctx);
    if (g_rogue_armed && ++g_rogue_pass == 2) {
        g_rogue = (gc_obj_t*)g_rogue_mem;
        g_rogue->rtt = NODE; g_rogue->forward = NULL; g_rogue->epoch = g_rogue_epoch;
        visit(&g_rogue, ctx);
    }
}
static gc_obj_t** node_next(gc_obj_t* o) { return (gc_obj_t**)gc_obj_payload(o); }
static int64_t*   node_val(gc_obj_t* o)  { return (int64_t*)((uint8_t*)gc_obj_payload(o) + 8); }

/* What the embedder is handed on a violation. The engine reports and returns; the policy — trap,
 * log, tear the store down — is the embedder's, so here it is just a record of the call. */
typedef struct { int hits; const char* inv; } corruption_log_t;
static void corruption_cb(void* ctx, const gc_verify_t* what) {
    corruption_log_t* log = (corruption_log_t*)ctx;
    log->hits++; log->inv = what->invariant;
}

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-62s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while(0)

/* Assert the checker REJECTS, and that it names an invariant mentioning `want`. */
static void expect_reject(gc_heap_t* h, const char* want, const char* msg) {
    gc_verify_t r;
    bool ok = gc_verify(h, &r);
    int named = !ok && r.invariant && strstr(r.invariant, want) != NULL;
    if (!named) printf("    (got: %s)\n", ok ? "VERIFIED — the corruption was not detected"
                                             : (r.invariant ? r.invariant : "<no invariant>"));
    printf("  %-62s [%s]\n", msg, named ? "PASS" : "FAIL");
    fails += !named;
}

int main(void) {
    gc_heap_t h; gc_heap_init(&h, enum_roots, NULL);

    /* A chain deep enough to span lines, with garbage between links so the live set is
     * scattered — the shape that makes evacuation move things. */
    for (int i = 0; i < 64; i++) {
        gc_obj_t* n = gc_alloc(&h, NODE, NODE_S.size);
        *node_next(n) = g_root; *node_val(n) = (int64_t)i;
        g_root = n;
        for (int g = 0; g < 3; g++) (void)gc_alloc(&h, NODE, NODE_S.size);
    }
    gc_collect(&h);

    gc_verify_t r;
    CK(gc_verify(&h, &r), "a collected heap VERIFIES");
    if (r.invariant) printf("    (unexpected: %s)\n", r.invariant);

    /* ── every invariant, deliberately broken ────────────────────────────────── */

    /* 1. FORWARDING — the missed slot update. This is the archetypal moving-collector bug,
     *    and the one that changed no checksum anywhere in the conformance corpus before the
     *    collector actually evacuated. Point a live slot at a forwarded source. */
    {
        gc_obj_t* victim = *node_next(g_root);
        gc_obj_t  saved  = *victim;
        victim->forward = g_root;                 /* pretend it moved, slot never updated */
        expect_reject(&h, "forwarding", "a reference to an EVACUATED source is rejected");
        *victim = saved;
    }

    /* 2. MARK — an object reachable but not marked for this epoch. A sweep would free it
     *    while it is still in use, which is the classic use-after-free-by-collector. */
    {
        gc_obj_t* victim = *node_next(g_root);
        uint32_t  saved  = victim->epoch;
        victim->epoch = saved - 1;
        expect_reject(&h, "mark", "a reachable but UNMARKED object is rejected");
        victim->epoch = saved;
    }

    /* 3. LINE MAP — a live object on a line the map calls free. The allocator would hand
     *    that storage out again and two objects would overlap. */
    {
        gc_obj_t* victim = *node_next(g_root);
        imx_block_t* b = imx_block_of(victim);
        size_t ln = imx_line_of(victim);
        b->line_marks.bits[ln / 8] &= (uint8_t)~(1u << (ln % 8));   /* no single-bit clear op */
        expect_reject(&h, "line map", "a live object on an UNMARKED line is rejected");
        imx_lb_set(&b->line_marks, ln);
    }

    /* 4. HEADER — a NULL rtt. Every consumer dereferences it; a checker that walks the graph
     *    must not itself fall over on one. */
    {
        gc_obj_t* victim = *node_next(g_root);
        const gc_rtt_t* saved = victim->rtt;
        victim->rtt = NULL;
        expect_reject(&h, "header", "a NULL rtt on a reachable object is rejected");
        victim->rtt = saved;
    }

    /* 5. RESIDENCY — a reference into memory no block owns. This is what a stale pointer
     *    into a reclaimed region looks like from the graph's side. */
    {
        gc_obj_t* victim = *node_next(g_root);
        static char rogue[256] __attribute__((aligned(IMX_OBJECT_ALIGN)));
        gc_obj_t* fake = (gc_obj_t*)rogue;
        fake->rtt = NODE; fake->forward = NULL; fake->epoch = h.epoch - 1;   /* "live" */
        *node_next(g_root) = fake;
        expect_reject(&h, "residency", "a reference into memory NO BLOCK owns is rejected");
        *node_next(g_root) = victim;
    }
    CK(gc_verify(&h, &r), "the heap verifies again once each corruption is undone");

    /* 6. LOS — a large object the sweep should have freed but did not. */
    {
        size_t asz = sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET + 5000u * GC_ARRAY_ELEM_BYTES;
        gc_obj_t* big = gc_alloc(&h, ARR, (uint32_t)asz);
        *(uint32_t*)((uint8_t*)big + sizeof(gc_obj_t)) = 5000;
        gc_obj_t** el = (gc_obj_t**)((uint8_t*)big + sizeof(gc_obj_t) + GC_ARRAY_ELEMS_OFFSET);
        for (int i = 0; i < 5000; i++) el[i] = NULL;
        el[0] = g_root;
        gc_obj_t* keep = g_root; g_root = big;
        gc_collect(&h);
        CK(gc_verify(&h, &r), "a heap with a live LOS entry verifies");
        /* Drop it from the roots FIRST: the invariant under test is "large_objs contains only
         * this epoch's survivors", which is about the SWEEP. While it is still reachable the
         * graph walk reaches it first and (correctly) reports the mark failure instead. */
        g_root = keep;
        uint32_t saved_ep = big->epoch;
        big->epoch = h.epoch - 2;                 /* an entry the sweep should have removed */
        expect_reject(&h, "LOS", "an unmarked LOS entry that survived the sweep is rejected");
        big->epoch = saved_ep;
    }

    /* 7. The REPORTING contract, which is as much a part of the design as the checks. An armed
     *    heap must hand a violation to the embedder's handler and RETURN — the engine is a
     *    library, and a library that aborts turns any bug into a way to kill the host process.
     *    That this test reaches its own next line is the assertion. */
    {
        corruption_log_t seen = { 0, NULL };
        h.on_corruption = corruption_cb; h.on_corruption_ctx = &seen;

        /* The fault has to be one the COLLECTOR leaves behind, not one injected into it: a
         * collection recomputes the marks and the line map and clears forwarding, so anything
         * corrupted beforehand is either repaired by the cycle or fatal to the tracer (a NULL
         * rtt segfaults gc_collect's own trace loop before the checker ever runs). Both walks
         * read the embedder's roots, so g_rogue — armed to appear only on the checker's pass —
         * is a reference the finished collection genuinely left unaccounted for. */
        g_rogue_epoch = h.epoch;             /* survivors carry epoch-1; the epoch bumps before verify */
        g_rogue_pass  = 0; g_rogue_armed = 1;
        gc_collect(&h);
        g_rogue_armed = 0;

        CK(seen.hits == 1, "an armed collection REPORTS a violation to the embedder's handler");
        CK(seen.inv && strstr(seen.inv, "residency") != NULL, "the handler is told WHICH invariant broke");
        CK(1, "control returned to the caller — the collector did not end the process");

        h.on_corruption = NULL; h.on_corruption_ctx = NULL;
        CK(gc_verify(&h, &r), "the heap itself was sound; only the injected root was not");
    }

    /* 8. And the whole point of an oracle: it must survive REAL churn, not just a toy heap,
     *    or it will be switched off the first time it cries wolf. */
    {
        const size_t churn = (size_t)GC_INITIAL_THRESHOLD * 16;
        for (size_t a = 0; a < churn; a += NODE_S.size) (void)gc_alloc(&h, NODE, NODE_S.size);
        gc_collect(&h);
        bool churn_ok = gc_verify(&h, &r);
        if (!churn_ok) printf("    (invariant: %s)\n", r.invariant ? r.invariant : "?");
        CK(churn_ok, "verifies after churn-driven collections (with evacuation active)");
        int len = 0;
        for (gc_obj_t* n = g_root; n; n = *node_next(n)) len++;
        CK(len == 64, "the chain is still 64 long after the churn");
    }

    gc_heap_destroy(&h);
    printf("\nGC heap-invariant checker: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
