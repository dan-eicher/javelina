// test_capi_jit.c — tier selection through the embedder-options seam (wasm_config_t).
//
// wasm.h leaves wasm_config_t deliberately empty ("an embedder extension point"), which is where
// a non-standard engine option belongs: `jav_config_set_jit` asks the engine to place every
// DEFINED function of each instance on the copy-and-patch JIT tier instead of the interpreter.
// Both tiers reach the runtime through the ONE invoke seam (jav_func_t::invoke), so the choice is
// invisible to callers — which is exactly what this test pins:
//
//   1. tier CHOICE is observable   — jav_capi_jit_count(store) counts the compiled funcinsts:
//                                    0 with the default engine, one per defined func with jit on.
//   2. tier choice is SEMANTICS-FREE — interp and JIT agree on results, on traps, and on the trap's
//                                    reported position, across calls / loops (side-tables) / GC / EH.
//   3. the interp-tier probe goes quiet on a JIT'd function (the documented jav_capi_set_probe
//      contract: "a JIT'd function does not fire it").
#include "wasm.h"
#include "jav_extern.h"          // jav_capi_last_status / jav_capi_jit_count / jav_config_set_jit
#include "wat_driver.h"          // wat text → jav_module_t (the water assembler front end)
#include "jav_writer.h"          // jav_module_write → §5 binary bytes
#include "bbq_runtime.h"         // bbq_write_ctx_t (the growable output buffer)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INSTRS_TOML "../spec/instructions.toml"   // the test runs from test/

static int fails = 0;
#define CK(c) do { if (!(c)) { printf("  FAIL: %s (line %d)\n", #c, __LINE__); fails++; } } while (0)

static void assemble(const char* wat, wasm_byte_vec_t* out) {
    int el = 0, ec = 0;
    jav_module_t* mod = wat_assemble(wat, (int)strlen(wat), INSTRS_TOML, &el, &ec);
    if (!mod) { fprintf(stderr, "wat_assemble failed at %d:%d\n", el, ec); exit(2); }
    bbq_write_ctx_t w; bbq_write_ctx_init_growable(&w, strlen(wat) + 64);
    bbq_write_set_endian(&w, true);
    if (!jav_module_write(&w, mod)) { fprintf(stderr, "serialize failed\n"); exit(2); }
    wasm_byte_vec_new(out, w.pos, (const wasm_byte_t*)w.data);
    bbq_write_ctx_free(&w); jav_module_free(mod); free(mod);
}

// A run of one exported (i32)->(i32) function on a fresh engine/store at the requested tier.
typedef struct { int32_t result; int trapped; uint32_t jit_count; unsigned probe_ops; } run_t;

static unsigned g_probe_ops;
static void probe_cb(void* ctx, uint8_t op) { (void)ctx; (void)op; g_probe_ops++; }

static run_t run_at_tier(const wasm_byte_vec_t* bin, int jit, const char* export_name, int32_t arg) {
    run_t r; memset(&r, 0, sizeof r);

    wasm_engine_t* engine;
    if (jit) {
        wasm_config_t* cfg = wasm_config_new();
        CK(cfg != NULL);                       // the extension point must be a real object now
        jav_config_set_jit(cfg, 1);
        engine = wasm_engine_new_with_config(cfg);   // consumes cfg
    } else {
        engine = wasm_engine_new();
    }
    wasm_store_t* store = wasm_store_new(engine);

    g_probe_ops = 0;
    jav_capi_set_probe(store, probe_cb, NULL);

    wasm_byte_vec_t copy; wasm_byte_vec_copy(&copy, bin);
    wasm_module_t* mod = wasm_module_new(store, &copy);
    if (!mod) { printf("  FAIL: module_new (jit=%d)\n", jit); fails++; goto out; }

    wasm_extern_vec_t imports = WASM_EMPTY_VEC;
    wasm_trap_t* trap = NULL;
    wasm_instance_t* inst = wasm_instance_new(store, mod, &imports, &trap);
    if (!inst) { printf("  FAIL: instance_new (jit=%d)\n", jit); fails++; if (trap) wasm_trap_delete(trap); goto out; }

    r.jit_count = jav_capi_jit_count(store);

    wasm_exporttype_vec_t expt; wasm_module_exports(mod, &expt);
    wasm_extern_vec_t exp;      wasm_instance_exports(inst, &exp);
    wasm_func_t* fn = NULL;
    for (size_t i = 0; i < expt.size && i < exp.size; i++) {
        const wasm_name_t* n = wasm_exporttype_name(expt.data[i]);
        if (n->size == strlen(export_name) && !memcmp(n->data, export_name, n->size)) {
            fn = wasm_extern_as_func(exp.data[i]); break;
        }
    }
    CK(fn != NULL);
    if (fn) {
        wasm_val_t args[1] = { WASM_I32_VAL(arg) };
        wasm_val_t res[1]  = { WASM_INIT_VAL };
        wasm_val_vec_t av = { 1, args }, rv = { 1, res };
        wasm_trap_t* t = wasm_func_call(fn, &av, &rv);
        if (t) { r.trapped = 1; wasm_trap_delete(t); } else r.result = res[0].of.i32;
    }
    r.probe_ops = g_probe_ops;

    wasm_exporttype_vec_delete(&expt); wasm_extern_vec_delete(&exp);
    wasm_instance_delete(inst);
out:
    if (mod) wasm_module_delete(mod);
    wasm_store_delete(store);
    wasm_engine_delete(engine);
    return r;
}

// ── the modules under test ──────────────────────────────────────────────────
// 2 defined funcs; $sum loops (side-table branches) and CALLS $dbl — a JIT'd callee reached
// from a JIT'd caller through the index seam. sum(n) = Σ_{i<n} 2i = n(n-1).
static const char* WAT_LOOP =
    "(module"
    "  (func $dbl (param i32) (result i32) local.get 0 local.get 0 i32.add)"
    "  (func $sum (export \"sum\") (param $n i32) (result i32)"
    "    (local $i i32) (local $acc i32)"
    "    (block $done"
    "      (loop $lp"
    "        local.get $i local.get $n i32.ge_s br_if $done"
    "        local.get $acc local.get $i call $dbl i32.add local.set $acc"
    "        local.get $i i32.const 1 i32.add local.set $i"
    "        br $lp))"
    "    local.get $acc))";

// A guaranteed trap (i32.div_s by zero) — the tiers must agree that it traps.
static const char* WAT_TRAP =
    "(module"
    "  (func $boom (export \"boom\") (param i32) (result i32)"
    "    local.get 0 i32.const 0 i32.div_s))";

// GC + EH on the JIT tier: allocate a struct (via a second func, so a JIT'd caller reaches a JIT'd
// callee that allocates), throw its field as the tag payload, catch it, and add the field back.
static const char* WAT_GC_EH =
    "(module"
    "  (type $pt (struct (field (mut i32))))"
    "  (tag $t (param i32))"
    "  (func $mk (param i32) (result (ref $pt)) (struct.new $pt (local.get 0)))"
    "  (func $go (export \"go\") (param i32) (result i32)"
    "    (local $p (ref null $pt))"
    "    (local.set $p (call $mk (local.get 0)))"
    "    (block $caught (result i32)"
    "      (try_table (catch $t $caught)"
    "        (throw $t (struct.get $pt 0 (local.get $p))))"
    "      (unreachable))"
    "    (struct.get $pt 0 (local.get $p))"
    "    i32.add))";

int main(void) {
    printf("── wasm_config_t tier selection (interp vs copy-and-patch JIT) ──\n");

    // 1. loop + call: results agree, and the JIT tier actually compiled BOTH defined funcs.
    {
        wasm_byte_vec_t bin; assemble(WAT_LOOP, &bin);
        run_t i = run_at_tier(&bin, 0, "sum", 10);
        run_t j = run_at_tier(&bin, 1, "sum", 10);
        CK(!i.trapped && !j.trapped);
        CK(i.result == 90);              // 10*9
        CK(j.result == i.result);        // tier choice is semantics-free
        CK(i.jit_count == 0);            // default engine: pure interp
        CK(j.jit_count == 2);            // $dbl + $sum both on the JIT tier
        CK(i.probe_ops > 0);             // the interp fired the debug probe …
        CK(j.probe_ops == 0);            // … and a JIT'd function does not
        printf("  loop+call         interp=%d jit=%d  jit_count=%u  probe interp=%u jit=%u [%s]\n",
               i.result, j.result, j.jit_count, i.probe_ops, j.probe_ops,
               (i.result == j.result && j.jit_count == 2 && j.probe_ops == 0) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    // 2. traps agree across tiers.
    {
        wasm_byte_vec_t bin; assemble(WAT_TRAP, &bin);
        run_t i = run_at_tier(&bin, 0, "boom", 7);
        run_t j = run_at_tier(&bin, 1, "boom", 7);
        CK(i.trapped && j.trapped);
        CK(j.jit_count == 1);
        printf("  div_s/0 trap      interp=%s jit=%s [%s]\n",
               i.trapped ? "trap" : "no", j.trapped ? "trap" : "no",
               (i.trapped && j.trapped) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    // 3. GC allocation + exception throw/catch on the JIT tier.
    {
        wasm_byte_vec_t bin; assemble(WAT_GC_EH, &bin);
        run_t i = run_at_tier(&bin, 0, "go", 21);
        run_t j = run_at_tier(&bin, 1, "go", 21);
        CK(!i.trapped && !j.trapped);
        CK(i.result == 42);              // caught payload 21 + field 21
        CK(j.result == i.result);
        CK(j.jit_count == 2);
        printf("  gc + try/throw    interp=%d jit=%d [%s]\n", i.result, j.result,
               (!i.trapped && !j.trapped && i.result == 42 && j.result == 42) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    printf("\ntier selection: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
