// test_gc_roots_real.c — the collector's contract, over objects the ENGINE builds.
//
// jav_gc_enum_roots (jav_runtime.c:654) scans FOUR root sources: the operand stack, frame locals,
// module globals, and table entries. Each gates on the slot's tag == T_GCREF. The contract has two
// halves and a test that checks only one is worthless in the other direction:
//   reachable through a root => survives, payload intact   (miss this and you ship use-after-free)
//   unreachable              => reclaimed                   (miss this and you ship a leak)
//
// What existed before this file, and why the seam leaked:
//   - test_gc / test_gc_roots / test_struct_gc run collections, but over HAND-BUILT gc_rtt_ts the
//     engine never produces. They are collector unit tests; they cannot see a build_rtts defect.
//   - test_gc_funcref checks build_rtts' output but never runs a collection.
// So a bug IN build_rtts was invisible to both sides — which is exactly where the ref.i31
// misclassification lived (an i31 element handed to gc_mark1 and dereferenced, a VM SIGSEGV on
// validated bytecode). This file closes that seam: real module -> real build_rtts -> real
// allocation -> real collection.
//
// The $collect host import runs a collection MID-EXECUTION. That is the only way to hold a root
// that exists solely while a frame is live — a LOCAL, or an operand on the stack. Every prior GC
// test collected between calls, when those roots are already gone, so two of the four root sources
// had no coverage at all.
//
// FALSIFICATION RECORD (each break rebuilt, seen red AND localized, then restored):
//   - locals root scan disabled (jav_runtime.c enum_roots)      -> local rows red, operand green
//   - exn-field ref_offsets disabled (jav_exn_rtt_for)          -> the 3 exn rows red, others green
//   - init-global tag upgrade disabled (jav_instance.c)         -> the 2 init-global rows red
//   - mark-clear disabled (gc_collect/imx_space_clear_marks)    -> the 2 reclaim rows red
//   - elem-segment scans disabled (enum_roots + visit_roots)    -> the elem release row red
//     (the elem READ rows stay green on stale free()d bytes — the delta row is the instrument)
// Two instruments FAILED falsification and were replaced: payload re-reads (a dead small object
// line-shares with live neighbours; a dead LOS object's free()d bytes stay readable until
// re-malloc'd) — hence live_bytes deltas with ~64 KB payloads throughout.
//
// The elem rows also caught a REAL driver bug that is itself a documented embedder obligation:
// without §4.7.2 step-24 tracking (on_inst_alloc + extra_roots), a collection triggered by a
// LATER instantiation-time allocation freed an EARLIER one (the passive segment's array died to
// the active segment's eval_const). The c-api store implements the same contract via
// capi_track_inst; this driver now exercises it on the bare-VM path.
#include "interp.h"
#include "heap.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "jav_instance.h"
#include "jav_view_nav.h"
#include "immix/jav_gc.h"
#include "immix/immix_space.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(cond, msg) do { int ok=(cond); printf("  %-56s [%s]\n", msg, ok?"PASS":"FAIL"); fails+=!ok; } while (0)

static heap_t* g_heap;                     /* the $collect import needs the live heap */
static size_t  g_live_bytes;               /* survivors of the LAST mid-execution collection */

/* Collect with the caller's frame live, and record how many bytes the tracer counted as SURVIVING.
 *
 * live_bytes is the direct observation of "was this root scanned"; payload survival is NOT. A
 * wrongly-freed object's memory is not overwritten by the collection, and Immix reclaims at block
 * granularity, so re-reading it — even after churning hundreds of allocations — still returns the
 * right value. Verified twice: with the locals root scan entirely disabled, a payload-survival
 * version of this test stayed green, first without churn and then with 512 churned allocations.
 * A test that cannot go red is not a test. */
static jav_status_t host_collect(vm_t* vm, heap_t* h, void* p) {
    (void)vm; (void)p;
    heap_t* hp = h ? h : g_heap;
    if (hp && hp->gc.self) {
        hp->gc.collect(hp->gc.self);                      /* collect with the CALLER's frame live */
        g_live_bytes = ((gc_heap_t*)hp->gc.self)->live_bytes;
    }
    return JAV_OK;
}
static const jav_functype_t FT_VOID_VOID = { NULL, 0, NULL, 0, NULL, NULL };
static jav_extern_t collect_ext(void) {
    jav_extern_t x; memset(&x, 0, sizeof x);
    x.kind = 0; x.u.func.type = &FT_VOID_VOID;
    x.u.func.func.invoke = host_collect;
    x.u.func.func.num_params = 0; x.u.func.func.num_results = 0;
    return x;
}

static uint8_t* slurp(const char* path, long* n) {
    FILE* f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
    fseek(f,0,SEEK_END); *n = ftell(f); fseek(f,0,SEEK_SET);
    uint8_t* b = malloc((size_t)*n);
    if (fread(b,1,(size_t)*n,f) != (size_t)*n) { perror("fread"); exit(2); }
    fclose(f); return b;
}

/* one instance for the whole file: roots persist across calls, which is the point */
static bbq_arena     A;
static uint8_t*      BUF;
static jav_modidx_t  MOD;
static jav_instance_t INST;
static vm_t          VM;
static heap_t        HEAP;

/* §4.7.2 step 24: the spec puts the instance in the store AT ALLOCATION, before active segments
 * and start run — so instantiation-time allocations (a global's struct.new init, an elem item's
 * array.new) are rooted while LATER init steps allocate and can trigger a collection. The engine
 * exposes exactly this as on_inst_alloc + extra_roots (the c-api store does the same via
 * capi_track_inst); an embedder that skips it loses earlier init allocations to a collection
 * triggered by later ones — which this driver did, and the elem release row caught. */
static jav_instance_t* g_tracked;
static void track_inst(void* c, void* inst) { (void)c; g_tracked = (jav_instance_t*)inst; }
static void bare_extra_roots(void* c, jav_root_visit_fn v, void* vc) {
    (void)c;
    if (g_tracked) jav_instance_visit_roots(g_tracked, v, vc);
}

static int call(const char* name, int* out) {
    int32_t fx = jav_instance_export(&INST, name, 0);
    if (fx < 0) { printf("  export missing: %s\n", name); fails++; return 0; }
    VM.frame.sp = 0; VM.frame.num_locals = 0;
    jav_status_t s = jav_call(&VM, VM.heap, (u4)fx);
    if (s != JAV_OK) return 0;
    if (out) *out = jav_tos(&VM).i;
    return 1;
}
/* C-side collections update the same live_bytes instrument as the $collect import: survival of a
 * payload is NOT observable (a dead small object line-shares with live neighbours; a dead LOS
 * object's free()d bytes stay readable until re-malloc'd — both verified by falsifiers that
 * stayed green), but the tracer COUNTING the object is. */
static void collect2(void){
    HEAP.gc.collect(HEAP.gc.self); HEAP.gc.collect(HEAP.gc.self);
    g_live_bytes = ((gc_heap_t*)HEAP.gc.self)->live_bytes;
}

int main(void) {
    long n; BUF = slurp("gc_roots_real.wasm", &n);
    bbq_arena_init(&A, 0);
    bbq_capture_metadata m = jav_view_module(BUF, (size_t)n, &A);
    CK(m.success, "module views");
    CK(jav_module_index(m.root, BUF, &A, &MOD), "module indexes (real build_rtts)");
    jav_err_t err;
    int valid = jav_module_validate(m.root, BUF, &MOD, &err) == JAV_OK;
    if (!valid) printf("  (validator: %s)\n", jav_err_str(err));
    CK(valid, "module validates");
    /* a module that failed any setup step must not be instantiated — running an invalid module is
     * a crash, not a diagnostic */
    if (fails) { printf("\nGC roots over real modules: FAIL (setup)\n"); return 1; }

    memset(&HEAP, 0, sizeof HEAP); memset(&VM, 0, sizeof VM);
    jav_vm_init(&VM); VM.heap = &HEAP; g_heap = &HEAP;
    jav_heap_gc_init(&HEAP, &VM);   /* collector BEFORE instantiate, as the c-api does at store
                                     * creation — $ginit's struct.new init allocates during it */
    VM.on_inst_alloc = track_inst;  /* §4.7.2 step 24: root the instance AT allocation (see above) */
    VM.extra_roots = bare_extra_roots;
    jav_extern_t imp = collect_ext();
    CK(jav_instantiate(&VM, m.root, BUF, &MOD, &imp, 1, &INST, &err) == JAV_OK, "instantiates");
    jav_instance_bind(&VM, &INST);
    gc_heap_t* gh = HEAP.gc.self;
    if (fails) { printf("\nGC roots over real modules: FAIL (setup)\n"); return 1; }

    int v;
    printf("\n-- root: MODULE GLOBAL --\n");
    CK(call("global_hold", NULL), "hold a struct in a global");
    collect2();
    CK(call("global_read", &v) && v == 42, "payload intact after 2 collections");

    printf("\n-- root: TABLE ENTRY --\n");
    CK(call("table_hold", NULL), "hold a struct in a table slot");
    collect2();
    CK(call("table_read", &v) && v == 42, "payload intact after 2 collections");

    /* Baseline: a mid-execution collection holding NOTHING. Every case below must count strictly
     * more surviving bytes than this — that difference IS the root being scanned. Comparing against
     * a baseline rather than an absolute makes the assertion independent of allocator bookkeeping. */
    CK(call("nothing_held", NULL), "baseline: collect with no object held");
    size_t base = g_live_bytes;

    printf("\n-- root: FRAME LOCAL (collected mid-execution) --\n");
    CK(call("local_survives_gc", &v) && v == 42, "local: payload intact");
    CK(g_live_bytes > base, "local: the collector COUNTED it live (root was scanned)");

    printf("\n-- root: OPERAND STACK (collected mid-execution) --\n");
    CK(call("operand_survives_gc", &v) && v == 42, "operand: payload intact");
    CK(g_live_bytes > base, "operand: the collector COUNTED it live (root was scanned)");

    printf("\n-- transitive reachability (collected mid-execution) --\n");
    CK(call("transitive_survives_gc", &v) && v == 42, "struct -> struct: payload intact");
    CK(g_live_bytes > base, "struct -> struct: BOTH counted live (field traced)");
    CK(call("array_elem_survives_gc", &v) && v == 42, "array -> struct: payload intact");
    CK(g_live_bytes > base, "array -> struct: BOTH counted live (element traced)");

    /* every held payload below is ~64 KB, so "the tracer counted it" is a >60000-byte delta in
     * live_bytes between collections with identical background — unfakeable by line-sharing or
     * stale free()d memory */
    size_t L0, L1;

    printf("\n-- root: GLOBAL INITIALIZED BY A GC CONST-EXPR (§4.2.10) --\n");
    /* the 64 KB array was allocated at INSTANTIATION; nothing but the init-tag upgrade roots it */
    collect2();
    CK(call("init_global_read", &v) && v == 42, "init-time allocation intact after collections");
    L0 = g_live_bytes;
    CK(call("init_global_drop", NULL), "drop it");
    collect2(); L1 = g_live_bytes;
    CK(L0 - L1 > 60000, "the tracer had been COUNTING it live (drop released ~64 KB)");

    printf("\n-- EXCEPTION FIELDS (§4.2.16): ref payload traced through the exn instance --\n");
    collect2(); L0 = g_live_bytes;
    CK(call("exn_field_hold", NULL), "throw, catch_ref, hold the exnref in a global");
    collect2(); L1 = g_live_bytes;
    CK(L1 - L0 > 60000, "the 64 KB payload is COUNTED live through the exn's ref_offsets");
    CK(call("exn_field_read", &v) && v == 42, "payload reads back through throw_ref/catch");
    CK(call("exn_drop", NULL), "drop the exn root");
    collect2();
    CK(L1 - g_live_bytes > 60000, "dropping the exn released the payload");

    printf("\n-- LARGE OBJECT SPACE: 64 KB array, mark-in-place path --\n");
    collect2(); L0 = g_live_bytes;
    CK(call("los_hold", NULL), "hold a 64 KB array (LOS)");
    collect2(); L1 = g_live_bytes;
    CK(L1 - L0 > 60000, "the LOS object is COUNTED live");
    CK(call("los_read", &v) && v == 42, "LOS array length + payload intact");

    printf("\n-- ELEMENT INSTANCES (§4.2.12): refs parked in segments + written by active elems --\n");
    /* every earlier section collected repeatedly, so both 64 KB arrays — parked passive since
     * instantiation, and slot 1 written by the active segment — have had every chance to be freed
     * if segments are not scanned or the active write mis-tagged the slot */
    CK(call("active_elem_read", &v) && v == 42, "active-elem slot intact (write carried the tag)");
    CK(call("elem_init_read", &v) && v == 42, "passive seg intact; table.init carried the tag");
    collect2(); L0 = g_live_bytes;
    CK(call("elem_drop_all", NULL), "elem.drop + null both slots");
    collect2();
    if (!(L0 - g_live_bytes > 120000))
        printf("  (live_bytes: before drop %zu, after %zu)\n", L0, g_live_bytes);
    CK(L0 - g_live_bytes > 120000, "both parked 64 KB arrays released (they WERE being counted)");

    printf("\n-- fragmentation stress: 64 held leaves across churned collections --\n");
    CK(call("frag_hold", NULL), "hold 64 leaves interleaved with garbage");
    for (int i = 0; i < 8; i++) { call("churn256", NULL); HEAP.gc.collect(HEAP.gc.self); }
    CK(call("frag_check", &v) && v == 2016, "all 64 payloads intact across 8 churn+collect cycles");

    printf("\n-- the other half: unreachable objects are RECLAIMED --\n");
    CK(call("global_drop", NULL) && call("table_drop", NULL) && call("init_global_drop", NULL),
       "drop the global + table + init-global roots");
    collect2();
    size_t total = imx_space_total_blocks(&gh->space);
    CK(imx_space_free_blocks(&gh->space) == total, "all roots dropped -> heap fully reclaimed");
    int allocated = 0;
    for (int i = 0; i < 64; i++) allocated += call("garbage", NULL);
    CK(allocated == 64, "allocate 64 immediately-unreachable structs");
    collect2();
    CK(imx_space_free_blocks(&gh->space) == total, "64 unreachable allocations -> fully reclaimed");

    printf("\nGC roots over real modules: %s\n", fails ? "FAIL" : "ALL PASS");
    jav_heap_gc_destroy(&HEAP); jav_instance_free(&INST); jav_vm_free(&VM);
    bbq_arena_free(&A); free(BUF);
    return fails ? 1 : 0;
}
