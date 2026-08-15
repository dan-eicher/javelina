// run_naive.c — PIN E3-3: the naive-producer corpus, timed. The question this
// answers is the one the compiler benchmark structurally cannot: javelinac's
// burg never emits what eq-sat folds, so tier-3 measured 1.00x there by
// construction. These kernels are lowered the way template producers lower —
// identity junk and mul-spelled shifts on every hot path — with a hand-clean
// twin of each as the ceiling. Per kernel, six timed configs — tier 1 (the
// plain copy-and-patch JIT) is the compiled baseline for both forms:
//
//     clean t1/t2/t3   the ceiling and its own tier story
//     naive t1         the junk at a dispatch per op — naivety's full price
//     naive t2         what the stack cache absorbs of it
//     naive t3         what only the fold removes — the measurement
//
// An EMBEDDER, deliberately: it consumes the .wasm water assembled at build
// time and links libjavelina.a against wasm.h + the jav_ extension seam —
// the same artifact a third party gets. Methodology mirrors bench.sh:
// compile at instantiation (outside every timed window), untimed warmup,
// min-of-reps on one instance, checksums gated across every config against
// the interpreted clean oracle. n is scaled ONCE per kernel (clean t2 to
// >= 100ms) and shared by all four configs.
#define _POSIX_C_SOURCE 200809L   /* clock_gettime under -std=c11 */
#include "wasm.h"
#include "jav_extern.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int fails = 0;
#define CK(c, ...) do { if (!(c)) { printf("FAIL "); printf(__VA_ARGS__); \
                                    printf("\n"); fails++; } } while (0)

static double now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void slurp(const char* path, wasm_byte_vec_t* out) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s (water builds it)\n", path); exit(2); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); exit(2); }
    fclose(f);
    wasm_byte_vec_new(out, (size_t)sz, (const wasm_byte_t*)buf);
    free(buf);
}

// One instantiated function, reusable across invokes.
typedef struct {
    wasm_engine_t* engine; wasm_store_t* store; wasm_module_t* mod;
    wasm_instance_t* inst; wasm_extern_vec_t exp; wasm_func_t* fn;
    uint64_t rewritten;
} live_t;

static live_t up(const wasm_byte_vec_t* bin, int tier) {
    live_t L; memset(&L, 0, sizeof L);
    wasm_config_t* cfg = wasm_config_new();
    jav_config_set_jit(cfg, tier);
    L.engine = wasm_engine_new_with_config(cfg);
    L.store = wasm_store_new(L.engine);
    /* wasm_module_new takes const and copies internally — no caller copy. */
    L.mod = wasm_module_new(L.store, bin);
    if (!L.mod) { fprintf(stderr, "module_new failed\n"); exit(2); }
    wasm_extern_vec_t imports = WASM_EMPTY_VEC;
    wasm_trap_t* trap = NULL;
    uint64_t rw0 = jav_capi_eqsat_rewritten();
    L.inst = wasm_instance_new(L.store, L.mod, &imports, &trap);
    L.rewritten = jav_capi_eqsat_rewritten() - rw0;
    if (!L.inst) { fprintf(stderr, "instantiate failed\n"); exit(2); }
    wasm_instance_exports(L.inst, &L.exp);
    L.fn = L.exp.size ? wasm_extern_as_func(L.exp.data[0]) : NULL;
    if (!L.fn) { fprintf(stderr, "no exported func\n"); exit(2); }
    return L;
}

static void down(live_t* L) {
    wasm_extern_vec_delete(&L->exp);
    wasm_instance_delete(L->inst);
    wasm_module_delete(L->mod);
    wasm_store_delete(L->store);
    wasm_engine_delete(L->engine);
}

static uint32_t invoke(live_t* L, int32_t n) {
    wasm_val_t args[1] = { WASM_I32_VAL(n) };
    wasm_val_t res[1]  = { WASM_INIT_VAL };
    wasm_val_vec_t av = { 1, args }, rv = { 1, res };
    wasm_trap_t* t = wasm_func_call(L->fn, &av, &rv);
    if (t) { fprintf(stderr, "trapped\n"); exit(2); }
    return (uint32_t)res[0].of.i32;
}

typedef struct { double min_ms; uint32_t sum; uint64_t rewritten; } meas_t;

static meas_t measure(const wasm_byte_vec_t* bin, int tier, int32_t n, int reps) {
    live_t L = up(bin, tier);
    meas_t m; m.rewritten = L.rewritten;
    m.sum = invoke(&L, n);                     /* untimed warmup */
    m.min_ms = 1e18;
    for (int r = 0; r < reps; r++) {
        double t0 = now_ms();
        uint32_t s = invoke(&L, n);
        double dt = now_ms() - t0;
        if (s != m.sum) { fprintf(stderr, "unstable checksum within one config\n"); exit(2); }
        if (dt < m.min_ms) m.min_ms = dt;
    }
    down(&L);
    return m;
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "build/naive";
    static const char* kernels[] = { "k_addr", "k_poly", "k_wide", "k_smand", "k_vmand" };
    const int REPS = 5;
    printf("naive-producer corpus, 5 kernels x {clean,naive} x {t1,t2,t3}, min of %d\n"
           "tier 1 is the compiled baseline for both: nt2/nt1 is what the stack\n"
           "cache absorbs of the junk, nt3/nt2 what only the fold removes.\n\n", REPS);
    printf("  %-8s %8s %8s %8s %8s %8s %8s %8s | %7s %7s %7s %7s %7s\n",
           "kernel", "n", "c-t1", "c-t2", "c-t3", "n-t1", "n-t2", "n-t3",
           "ct2/ct1", "nt2/nt1", "nt3/nt2", "nt3/ct2", "nt2/ct2");
    for (int k = 0; k < 5; k++) {
        char pn[256], pc[256];
        snprintf(pn, sizeof pn, "%s/%s_naive.wasm", dir, kernels[k]);
        snprintf(pc, sizeof pc, "%s/%s_clean.wasm", dir, kernels[k]);
        wasm_byte_vec_t bn, bc;
        slurp(pn, &bn); slurp(pc, &bc);

        /* the interpreted clean oracle, small n */
        live_t O = up(&bc, 0);
        uint32_t oracle_small = invoke(&O, 3);
        down(&O);

        /* scale n on clean t2 until the window is >= 100ms; every config
         * shares the scaled n so the four times are the same work */
        int32_t n = 256;
        for (;;) {
            live_t L = up(&bc, 2);
            invoke(&L, n);
            double t0 = now_ms(); invoke(&L, n); double t = now_ms() - t0;
            down(&L);
            if (t >= 100.0 || n >= (1 << 26)) break;
            n <<= 1;
        }

        meas_t ct1 = measure(&bc, 1, n, REPS);
        meas_t ct2 = measure(&bc, 2, n, REPS);
        meas_t ct3 = measure(&bc, 3, n, REPS);
        meas_t nt1 = measure(&bn, 1, n, REPS);
        meas_t nt2 = measure(&bn, 2, n, REPS);
        meas_t nt3 = measure(&bn, 3, n, REPS);

        /* the gates: every config's checksum equals every other's at this n,
         * and a small-n naive-t3 run agrees with the interpreted oracle */
        CK(ct1.sum == ct2.sum && ct2.sum == ct3.sum
           && ct1.sum == nt1.sum && ct2.sum == nt2.sum && ct2.sum == nt3.sum,
           "%s: checksum drift across configs (%08x %08x %08x %08x %08x %08x)",
           kernels[k], ct1.sum, ct2.sum, ct3.sum, nt1.sum, nt2.sum, nt3.sum);
        meas_t small = measure(&bn, 3, 3, 1);
        CK(small.sum == oracle_small,
           "%s: naive t3 (%08x) disagrees with the interpreted clean oracle at n=3",
           kernels[k], (unsigned)small.sum);
        CK(nt3.rewritten > 0, "%s: tier-3 rewrote nothing in the naive body", kernels[k]);
        CK(ct3.rewritten == 0, "%s: tier-3 rewrote the CLEAN twin (%llu)",
           kernels[k], (unsigned long long)ct3.rewritten);

        printf("  %-8s %8d %6.1fms %6.1fms %6.1fms %6.1fms %6.1fms %6.1fms | %6.2fx %6.2fx %6.2fx %6.2fx %6.2fx\n",
               kernels[k], (int)n, ct1.min_ms, ct2.min_ms, ct3.min_ms,
               nt1.min_ms, nt2.min_ms, nt3.min_ms,
               ct2.min_ms / ct1.min_ms, nt2.min_ms / nt1.min_ms,
               nt3.min_ms / nt2.min_ms, nt3.min_ms / ct2.min_ms,
               nt2.min_ms / ct2.min_ms);
    }
    printf("\n  nt2/nt1 is the junk cost the stack cache absorbs; nt3/nt2 < 1 is the\n"
           "  fold's own win on its target input; nt2/ct2 is what naivety costs\n"
           "  untreated at t2; nt3/ct2 -> 1.00 means the fold recovered it all.\n");
    printf("%s\n", fails ? "FAILED" : "ok (all checksums agree)");
    return fails != 0;
}
