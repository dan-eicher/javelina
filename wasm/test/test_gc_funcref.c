// test_gc_funcref.c — build_rtts' trace classification, per §4.2.1's value production.
//
// The rule: traced iff the type can be inhabited by a value carrying a gc_obj_t address.
//   - funcref: a §4.2.1 ref.func is a funcaddr (&ctx->functions[i]) — never a gc_obj_t → NOT traced.
//   - EXTERNREF: TRACED. This file previously asserted the opposite ("an externref is a host
//     pointer") and that premise was FALSE — a use-after-free asserted as correct behaviour.
//     §2.3.4 makes the extern and any hierarchies "inhabited by an isomorphic set of values", and
//     javelina's extern.convert_any is IDENTITY (wasm.def), so an externref slot can hold a live
//     struct/array; and a §4.2.1 ref.host enters the VM only as a HOST BOX (jav_host_box_new),
//     which is also a gc_obj_t. Every non-null externref inhabitant is a heap object. Not tracing
//     them freed a live aggregate behind extern.convert_any (pinned by test_gc_refforms).
//   - anyref/eqref: traced; their ref.i31 inhabitants are skipped BY VALUE in gc_mark1 (§2.3.4
//     pointer tagging), not by this per-type bit — one bit cannot decide for a union.
// This pins the classification at its source (the rtt builder), the earliest point the tracer's
// contract is set.
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_subtype.h"
#include "bbq_arena.h"
#include "immix/jav_gc.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-56s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while (0)

// A type-section-only module (all other index spaces empty). Types:
//   0: struct {}                         (a GC struct — a heap ref target)
//   1: (array (mut (ref null 0)))        struct-ref array   → elem_is_ref = 1
//   2: (array (mut funcref))             funcref array      → elem_is_ref = 0
//   3: (array (mut externref))           externref array    → elem_is_ref = 0
//   4: (array (mut anyref))              anyref array       → elem_is_ref = 1
//   5: struct { mut funcref, mut anyref } → nrefs = 1 (the anyref only)
static const uint8_t MODULE[] = {
    0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00,          // magic + version
    0x01, 0x16, 0x06,                                  // type section: size 22, 6 types
    0x5F, 0x00,                                        // 0: struct, 0 fields
    0x5E, 0x63, 0x00, 0x01,                            // 1: array (mut (ref null $0))
    0x5E, 0x70, 0x01,                                  // 2: array (mut funcref)
    0x5E, 0x6F, 0x01,                                  // 3: array (mut externref)
    0x5E, 0x6E, 0x01,                                  // 4: array (mut anyref)
    0x5F, 0x02, 0x70, 0x01, 0x6E, 0x01,                // 5: struct { mut funcref, mut anyref }
};

int main(void) {
    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module(MODULE, sizeof MODULE, &a);
    CK(m.success, "type-section module views");
    jav_modidx_t mod;
    CK(jav_module_index(m.root, MODULE, &a, &mod), "module indexes (build_rtts runs)");
    CK(mod.ntypes == 6, "6 types flattened");

    // Arrays: traced iff the element type admits a gc_obj_t-carrying inhabitant.
    CK(mod.rtts[1] && mod.rtts[1]->kind == GC_KIND_ARRAY && mod.rtts[1]->elem_is_ref == 1,
       "array of (ref struct) — traced");
    CK(mod.rtts[2] && mod.rtts[2]->kind == GC_KIND_ARRAY && mod.rtts[2]->elem_is_ref == 0,
       "array of funcref — NOT traced (funcaddr, not a heap object)");
    CK(mod.rtts[3] && mod.rtts[3]->kind == GC_KIND_ARRAY && mod.rtts[3]->elem_is_ref == 1,
       "array of externref — TRACED (identity-wrapped aggregate or host box, both gc_obj_t)");
    CK(mod.rtts[4] && mod.rtts[4]->kind == GC_KIND_ARRAY && mod.rtts[4]->elem_is_ref == 1,
       "array of anyref — traced");

    // Struct fields: the funcref field is skipped; anyref AND externref fields are ref offsets.
    CK(mod.rtts[5] && mod.rtts[5]->kind == GC_KIND_STRUCT && mod.rtts[5]->nrefs == 1,
       "struct {funcref, anyref} — funcref skipped, anyref traced");

    printf("%s: %s\n", "test_gc_funcref", fails ? "FAIL" : "OK");
    jav_modidx_free_bodies(&mod); bbq_arena_free(&a);   // release the c-lite index arena
    return fails ? 1 : 0;
}
