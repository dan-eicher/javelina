// test_bench_naive.c — the naive-producer corpus's correctness gates
// (PIN E3-1 / E3-2). Five kernels, each written twice in bench/naive/:
// <k>_naive.wat lowered the way a template producer lowers (identity junk on
// every hot path — *1, +0, ^0, &-1, shl 0, wrap(extend), vector and-ones/
// xor-zero/shuffle-id/bitselect-ones, doubling spelled as a multiply), and
// <k>_clean.wat, the same algorithm hand-lowered clean. Because the junk is
// all identities, the checksums are equal BY CONSTRUCTION — so:
//
//   E3-1  checksum identity across {naive, clean} × {tier 0, 2, 3}, the
//         interpreter on the clean form being the oracle; plus the
//         scalar/vector differential (k_smand == k_vmand at every point).
//   E3-2  tier-3 REWRITES every naive body (rewritten > 0), and rewrites
//         nothing in the clean twin (rewritten == 0) — a rule firing on the
//         clean form would be rewriting non-junk, which is the corpus lying.
//
// This is an EMBEDDER: it consumes the fixtures water assembled at build time
// and links libjavelina.a against the public wasm.h plus the declared jav_
// extension seam — the same artifact and surfaces a third party gets, no
// engine translation unit, no assembler in-process.
//
// Falsified by running the same reads at tier 2 (rewritten is 0 everywhere,
// E3-2's naive arm goes red) and by the clean twins themselves.
#include "wasm.h"
#include "jav_extern.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, ...) do { if (!(c)) { printf("  FAIL "); printf(__VA_ARGS__); \
                                    printf("\n"); fails++; } } while (0)

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

typedef struct { uint32_t sum; int trapped; uint64_t rewritten; } res_t;

// One run of "run"(n) on a fresh engine at the given tier. The eqsat counter
// is read across instantiation, which is where tier-3 compiles.
static res_t run_at(const wasm_byte_vec_t* bin, int tier, int32_t n) {
    res_t r; memset(&r, 0, sizeof r); r.trapped = 1;
    wasm_config_t* cfg = wasm_config_new();
    jav_config_set_jit(cfg, tier);
    wasm_engine_t* engine = wasm_engine_new_with_config(cfg);
    wasm_store_t* store = wasm_store_new(engine);
    /* wasm_module_new takes const and copies internally — the caller keeps
     * the vec. The first draft copied it per call and never deleted the
     * copy; the leaks pass caught its own harness. */
    wasm_module_t* mod = wasm_module_new(store, bin);
    if (!mod) goto out_store;
    {
        wasm_extern_vec_t imports = WASM_EMPTY_VEC;
        wasm_trap_t* trap = NULL;
        uint64_t rw0 = jav_capi_eqsat_rewritten();
        wasm_instance_t* inst = wasm_instance_new(store, mod, &imports, &trap);
        r.rewritten = jav_capi_eqsat_rewritten() - rw0;
        if (!inst) { if (trap) wasm_trap_delete(trap); goto out_mod; }
        wasm_extern_vec_t exp; wasm_instance_exports(inst, &exp);
        wasm_func_t* fn = exp.size ? wasm_extern_as_func(exp.data[0]) : NULL;
        if (fn) {
            wasm_val_t args[1] = { WASM_I32_VAL(n) };
            wasm_val_t res[1]  = { WASM_INIT_VAL };
            wasm_val_vec_t av = { 1, args }, rv = { 1, res };
            wasm_trap_t* t = wasm_func_call(fn, &av, &rv);
            if (t) wasm_trap_delete(t);
            else { r.trapped = 0; r.sum = (uint32_t)res[0].of.i32; }
        }
        wasm_extern_vec_delete(&exp);
        wasm_instance_delete(inst);
    }
out_mod:
    wasm_module_delete(mod);
out_store:
    wasm_store_delete(store);
    wasm_engine_delete(engine);
    return r;
}

int main(void) {
    static const char* kernels[] = { "k_addr", "k_poly", "k_wide", "k_smand", "k_vmand" };
    const int32_t N = 3;
    uint32_t sums[5] = {0};
    printf("naive-producer corpus: checksum identity + rewrite pins (n=%d)\n", (int)N);
    for (int k = 0; k < 5; k++) {
        char pn[128], pc[128];
        snprintf(pn, sizeof pn, "../build/naive/%s_naive.wasm", kernels[k]);
        snprintf(pc, sizeof pc, "../build/naive/%s_clean.wasm", kernels[k]);
        wasm_byte_vec_t bn, bc;
        slurp(pn, &bn); slurp(pc, &bc);

        res_t oracle = run_at(&bc, 0, N);            // interp on the clean form
        CK(!oracle.trapped, "%s: the oracle trapped", kernels[k]);
        sums[k] = oracle.sum;

        struct { const wasm_byte_vec_t* bin; int tier; const char* what; } runs[] = {
            { &bn, 0, "naive t0" }, { &bn, 2, "naive t2" }, { &bn, 3, "naive t3" },
            { &bc, 2, "clean t2" }, { &bc, 3, "clean t3" },
        };
        uint64_t rw_naive = 0, rw_clean = 0;
        for (size_t i = 0; i < sizeof runs / sizeof runs[0]; i++) {
            res_t r = run_at(runs[i].bin, runs[i].tier, N);
            CK(!r.trapped, "%s %s: trapped", kernels[k], runs[i].what);
            CK(r.sum == oracle.sum, "E3-1 %s %s: checksum %08x, oracle %08x",
               kernels[k], runs[i].what, r.sum, oracle.sum);
            if (runs[i].tier == 3) {
                if (runs[i].bin == &bn) rw_naive = r.rewritten; else rw_clean = r.rewritten;
            }
        }
        CK(rw_naive > 0, "E3-2 %s: tier-3 rewrote NOTHING in the naive body — "
           "the corpus's junk is invisible to the rule set", kernels[k]);
        CK(rw_clean == 0, "E3-2 %s: tier-3 rewrote %llu root(s) of the CLEAN twin — "
           "a rule is rewriting non-junk", kernels[k], (unsigned long long)rw_clean);
        printf("  %-8s sum=%08x rewritten naive=%llu clean=%llu\n", kernels[k],
               oracle.sum, (unsigned long long)rw_naive, (unsigned long long)rw_clean);
        wasm_byte_vec_delete(&bn); wasm_byte_vec_delete(&bc);
    }
    // The scalar/vector differential: same cells, same constants, equal by
    // construction — the two kernels must agree with each other, not merely
    // each with itself.
    CK(sums[3] == sums[4], "E3-1 k_smand (%08x) != k_vmand (%08x) — the lanes "
       "and the scalar walk computed different grids", sums[3], sums[4]);

    // PIN E3-c — the corpus is comprehensive BY GATE, not by claim: across
    // the five naive kernels, EVERY rewrite rule in the axiom file fired at
    // least once. A rule this corpus cannot fire is either a missing junk
    // site here (add it) or vocabulary no input can reach (a finding either
    // way). Red names the rule. Falsified by removing any one junk site.
    {
        const char* const* names; const unsigned long long* rule_fires;
        int nrules = jav_capi_eqsat_rules(&names, &rule_fires);
        int dead = 0;
        for (int i = 0; i < nrules; i++)
            if (!rule_fires[i]) {
                printf("  FAIL E3-c: rule %s fired 0 times across the corpus\n",
                       names[i]);
                fails++; dead++;
            }
        printf("  rules: %d total, %d never fired\n", nrules, dead);
    }
    printf("%s\n", fails ? "FAILED" : "ok");
    return fails != 0;
}
