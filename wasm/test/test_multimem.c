// test_multimem.c — multiple memories + memory64 VALIDATION (§3.4.5). The runtime
// keeps a single backing store for now; what is exercised here is the full-spec
// decode + validator: the memidx (default 0 or bit 6 of the memarg align-flags)
// bounded against nmemories, the memarg checks (2^align <= N/8, offset < 2^|at|), and
// the address operand typed at the memory's index type (i32 / i64). Pure typecheck.
#include "validate.h"
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t I32R[1] = { WVT_I32 };
static const jav_valtype_t I64R[1] = { WVT_I64 };

static int chk(const uint8_t* code, size_t n, unsigned nmem, const uint8_t* is64, const jav_valtype_t* res) {
    jav_vctx_t cx = {0};
    cx.results = res; cx.nresults = 1; cx.nmemories = nmem; cx.mem_is64 = is64;
    jav_st_entry_t* sd = NULL; unsigned ns;
    int ok = jav_typecheck(code, n, &cx, &sd, &ns);
    free(sd); return ok;
}

static int fails = 0;
#define T(cond, msg) do { int _c = (cond); printf("  %-52s [%s]\n", msg, _c ? "PASS" : "FAIL"); fails += !_c; } while (0)

// Execute a function on a heap of `nmem` memories (1 page each, max 10), returning the
// i32 result (or -0x6BADBAD on trap). Validates with nmemories=nmem, runs the chosen tier.
#define RTRAP (-0x6BADBAD)
static int runx(const uint8_t* code, size_t n, unsigned nmem, int jit) {
    jav_vctx_t cx = {0}; cx.results = I32R; cx.nresults = 1; cx.nmemories = nmem;
    jav_st_entry_t* sd = NULL; unsigned ns;
    if (!jav_typecheck(code, n, &cx, &sd, &ns)) { free(sd); return -999; }
    struct heap_t heap; memset(&heap, 0, sizeof heap);
    uint32_t ma[16]; for (unsigned i = 0; i < nmem; i++) { jav_mem_add(&heap, 1, 10, 1, 0); ma[i] = i; }   // identity memidx→heap map
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap; vm.cluster.mem_addrs = ma; vm.cluster.num_mems = nmem;
    bbq_ctx_init(&vm.frame.code, code, n); vm.frame.sidetable = sd;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm, &heap);
    int r = vm.trapped ? RTRAP : jav_tos(&vm).i;
    (void)st; jav_vm_free(&vm); jav_heap_free_mems(&heap); free(sd);
    return r;
}
// interp == jit AND both equal exp
static int runeq(const uint8_t* code, size_t n, unsigned nmem, int exp) {
    int i = runx(code, n, nmem, 0), j = runx(code, n, nmem, 1);
    return i == j && i == exp;
}

int main(void) {
    printf("multi-memory + memory64 validation (§3.4.5):\n");
    static const uint8_t is64[1] = { 1 };

    // i32.load mem0, align 2, offset 0  ->  accept (mem32, one memory)
    uint8_t ld[]    = { 0x41,0x00, 0x28,0x02,0x00, 0x0B };
    T(chk(ld, sizeof ld, 1, NULL, I32R),  "i32.load a=2 o=0 mem0 accepted");
    T(!chk(ld, sizeof ld, 0, NULL, I32R), "i32.load with 0 memories declared rejected");

    // over-alignment: align 3 means 2^3=8 > 4 (i32 width) -> reject
    uint8_t lda3[]  = { 0x41,0x00, 0x28,0x03,0x00, 0x0B };
    T(!chk(lda3, sizeof lda3, 1, NULL, I32R), "i32.load a=3 (over-aligned) rejected");

    // explicit memidx 1 via align bit 6: flags 0x42 = bit6 | align2, then memidx, offset
    uint8_t ldm1[]  = { 0x41,0x00, 0x28,0x42,0x01,0x00, 0x0B };
    T(!chk(ldm1, sizeof ldm1, 1, NULL, I32R), "i32.load memidx 1 with 1 memory rejected");
    T(chk(ldm1, sizeof ldm1, 2, NULL, I32R),  "i32.load memidx 1 with 2 memories accepted");

    // offset bound on mem32: offset must be < 2^32
    uint8_t offbig[] = { 0x41,0x00, 0x28,0x02, 0x80,0x80,0x80,0x80,0x10, 0x0B }; // off = 2^32
    T(!chk(offbig, sizeof offbig, 1, NULL, I32R), "i32.load offset 2^32 on mem32 rejected");
    uint8_t offmax[] = { 0x41,0x00, 0x28,0x02, 0xFF,0xFF,0xFF,0xFF,0x0F, 0x0B }; // off = 2^32-1
    T(chk(offmax, sizeof offmax, 1, NULL, I32R),  "i32.load offset 2^32-1 on mem32 accepted");

    // memory64: the address operand is typed i64
    uint8_t ld64[]    = { 0x42,0x00, 0x28,0x02,0x00, 0x0B };  // i64.const 0; i32.load
    uint8_t ld64bad[] = { 0x41,0x00, 0x28,0x02,0x00, 0x0B };  // i32.const 0; i32.load  (wrong addr type)
    T(chk(ld64, sizeof ld64, 1, is64, I32R),      "memory64: i32.load with i64 address accepted");
    T(!chk(ld64bad, sizeof ld64bad, 1, is64, I32R), "memory64: i32.load with i32 address rejected");
    // memory64 lifts the offset bound (offset < 2^64)
    uint8_t off64[] = { 0x42,0x00, 0x28,0x02, 0x80,0x80,0x80,0x80,0x10, 0x0B };
    T(chk(off64, sizeof off64, 1, is64, I32R),    "memory64: offset 2^32 accepted");

    // memory.size result type follows the address type: mem32 -> i32, mem64 -> i64
    uint8_t sz[] = { 0x3F,0x00, 0x0B };
    T(chk(sz, sizeof sz, 1, NULL, I32R),  "memory.size mem32 -> i32 accepted");
    T(!chk(sz, sizeof sz, 1, NULL, I64R), "memory.size mem32 not i64 (rejected)");
    T(chk(sz, sizeof sz, 1, is64, I64R),  "memory.size mem64 -> i64 accepted");

    // memory.grow: [at] -> [at]; mem32 grow takes/returns i32
    uint8_t gr[] = { 0x41,0x00, 0x40,0x00, 0x0B };  // i32.const 0; memory.grow 0
    T(chk(gr, sizeof gr, 1, NULL, I32R),  "memory.grow mem32 i32->i32 accepted");
    T(!chk(gr, sizeof gr, 1, is64, I32R), "memory.grow on mem64 needs i64 delta (rejected)");

    // ── runtime: memidx routing is real (two independent memories) ──
    // store 0xBB to mem0@0, 0xAA to mem1@0 (explicit memidx via bit 6), load mem0@0 -> 0xBB.
    uint8_t route[] = {
        0x41,0x00, 0x41,0xBB,0x01, 0x36,0x02,0x00,        // i32.store mem0 @0 = 187
        0x41,0x00, 0x41,0xAA,0x01, 0x36,0x42,0x01,0x00,   // i32.store mem1 @0 = 170 (flags 0x42, memidx 1)
        0x41,0x00, 0x28,0x02,0x00, 0x0B };                // i32.load  mem0 @0
    T(runeq(route, sizeof route, 2, 187), "two memories independent: mem0 keeps 0xBB  interp==jit");

    // ── runtime: memory.grow actually grows the backing ──
    // grow mem0 by 1 page (returns old size 1), drop it, memory.size -> 2.
    uint8_t grow[] = { 0x41,0x01, 0x40,0x00, 0x1A, 0x3F,0x00, 0x0B };
    T(runeq(grow, sizeof grow, 1, 2), "memory.grow 1: size 1 -> 2  interp==jit");

    // the freshly grown page (addr 65536) is now usable: store 99 there, load it back.
    uint8_t usegrown[] = {
        0x41,0x01, 0x40,0x00, 0x1A,                          // grow by 1 (now 2 pages)
        0x41,0x80,0x80,0x04, 0x41,0xE3,0x00, 0x36,0x02,0x00, // i32.store @65536 = 99
        0x41,0x80,0x80,0x04, 0x28,0x02,0x00, 0x0B };         // i32.load  @65536
    T(runeq(usegrown, sizeof usegrown, 1, 99), "grown page @65536 usable (store/load 99)  interp==jit");

    // before growing, @65536 is out of bounds -> trap.
    uint8_t oobpregrow[] = { 0x41,0x80,0x80,0x04, 0x28,0x02,0x00, 0x0B };
    T(runx(oobpregrow, sizeof oobpregrow, 1, 0) == RTRAP, "pre-grow load @65536 traps (interp)");

    printf("\nmulti-memory: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
