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
    jav_module_t* mod = wat_assemble(wat, (int)strlen(wat), &el, &ec);
    if (!mod) { fprintf(stderr, "wat_assemble failed at %d:%d\n", el, ec); exit(2); }
    bbq_write_ctx_t w; bbq_write_ctx_init_growable(&w, strlen(wat) + 64);
    bbq_write_set_endian(&w, true);
    if (!jav_module_write(&w, mod)) { fprintf(stderr, "serialize failed\n"); exit(2); }
    wasm_byte_vec_new(out, w.pos, (const wasm_byte_t*)w.data);
    bbq_write_ctx_free(&w); jav_module_free(mod); free(mod);
}

// A run of one exported function on a fresh engine/store at the requested tier.
// The argument and result are wasm_val_t rather than int32_t: every case here
// used to be (i32)->(i32), which meant no i64/f32/f64 shape could be expressed
// at all — and an operand whose class differs from its neighbour's is exactly
// where the tier-2 cache and the instruction part company.
typedef struct { wasm_val_t result; int trapped; uint32_t jit_count; unsigned probe_ops; } run_t;

// WASM_*_VAL are brace initializers, not expressions, so they cannot be passed
// as an argument directly. These are the expression forms.
static wasm_val_t i32v(int32_t x) { wasm_val_t v = WASM_I32_VAL(x); return v; }
static wasm_val_t i64v(int64_t x) { wasm_val_t v = WASM_I64_VAL(x); return v; }
static wasm_val_t f32v(float x)   { wasm_val_t v = WASM_F32_VAL(x); return v; }
static wasm_val_t f64v(double x)  { wasm_val_t v = WASM_F64_VAL(x); return v; }

// The scalar behind whichever kind came back, so a case can compare one number.
static int64_t rnum(const run_t* r) {
    switch (r->result.kind) {
    case WASM_I32: return r->result.of.i32;
    case WASM_I64: return r->result.of.i64;
    case WASM_F32: return (int64_t)r->result.of.f32;
    case WASM_F64: return (int64_t)r->result.of.f64;
    default:       return 0;
    }
}

static unsigned g_probe_ops;
static void probe_cb(void* ctx, uint8_t op) { (void)ctx; (void)op; g_probe_ops++; }

static run_t run_at_tier(const wasm_byte_vec_t* bin, int jit, const char* export_name, wasm_val_t arg) {
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

    /* wasm_module_new takes const and copies internally — the caller keeps
     * the vec, so a per-run copy with no delete was a slow leak. */
    wasm_module_t* mod = wasm_module_new(store, bin);
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
        /* The function's OWN arity, not a fixed one. Every case here used to take
         * exactly one i32, so a zero-parameter export could not be called at all
         * — it came back as an arity trap that reads like an engine defect. */
        wasm_val_t args[1] = { arg };
        wasm_val_t res[1]  = { WASM_INIT_VAL };
        size_t np = wasm_func_param_arity(fn), nr = wasm_func_result_arity(fn);
        wasm_val_vec_t av = { np, np ? args : NULL }, rv = { nr, nr ? res : NULL };
        wasm_trap_t* t = wasm_func_call(fn, &av, &rv);
        if (t) { r.trapped = 1; wasm_trap_delete(t); } else if (nr) r.result = res[0];
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

// A branch that CARRIES A VALUE out of a block — br_table.wast's `as-block-value`.
// This lives here rather than beside the stitcher's own fixtures because those
// hand-build a jav_tctx_t and enter the compiled handle directly, while this goes
// through the instantiate seam: jav_module_tctx, the per-function class arrays,
// the function table, and the call. A body the stitcher gets right in isolation
// and wrong here isolates the defect BETWEEN those two, which is the level that
// was missing — the corpus was doing this job and could only say "something".
static const char* WAT_BR_VALUE =
    "(module"
    "  (func $dummy)"
    "  (func (export \"as-block-value\") (result i32)"
    "    (block (result i32)"
    "      (nop) (call $dummy) (br_table 0 0 0 (i32.const 2) (i32.const 0)))))";

// Stores whose two operands are DIFFERENT storage classes. `i32.store` takes
// (i32 addr, i32 value) — one class — and passes on both tiers; every store that
// mismatches under tier-2 takes (i32 addr, T value) for some T that is not i32.
// In state k the top k operands are cached, so a mixed pair is precisely where
// the slot's class and the instruction's class part company, and no fixture had
// one. `i32_store` is here as the CONTROL: if it ever fails alongside the
// others, the discriminator is not the class pair after all.
static const char* WAT_STORE_MIX =
    "(module (memory 1)"
    "  (func (export \"i32_store\") (param i32) (result i32)"
    "    (i32.store (i32.const 0) (local.get 0)) (i32.load (i32.const 0)))"
    "  (func (export \"i64_store\") (param i64) (result i64)"
    "    (i64.store (i32.const 0) (local.get 0)) (i64.load (i32.const 0)))"
    "  (func (export \"i64_store16\") (param i64) (result i64)"
    "    (i64.store16 (i32.const 0) (local.get 0)) (i64.load16_u (i32.const 0)))"
    "  (func (export \"i64_store32\") (param i64) (result i64)"
    "    (i64.store32 (i32.const 0) (local.get 0)) (i64.load32_u (i32.const 0)))"
    "  (func (export \"f32_store\") (param f32) (result f32)"
    "    (f32.store (i32.const 0) (local.get 0)) (f32.load (i32.const 0)))"
    "  (func (export \"f64_store\") (param f64) (result f64)"
    "    (f64.store (i32.const 0) (local.get 0)) (f64.load (i32.const 0))))";

// A value arriving from a CALL, per result class. Grouping the conformance
// residue by what actually differs points here and not at the stores it was
// hiding behind: `i32_store` returns `(call $i32_load_little …)` and passes,
// while `i64_store` returns `(call $i64_load_little …)` and fails — same shape,
// different result class. The conversion cases (`f32.reinterpret_i32 (call …)`,
// `i64.extend_i32_s (call_indirect …)`) are the same thing with the class change
// spelled out. `call_i32` is the CONTROL: it must keep passing, or the
// discriminator is the call rather than the class.
static const char* WAT_CALL_CLASS =
    "(module"
    "  (type $ri32 (func (result i32)))"
    "  (func $ret_i32 (result i32) (i32.const 42))"
    "  (func $ret_i64 (result i64) (i64.const 42))"
    "  (func $ret_f32 (result f32) (f32.const 42))"
    "  (func $ret_f64 (result f64) (f64.const 42))"
    "  (table 1 1 funcref) (elem (i32.const 0) $ret_i32)"
    "  (func (export \"call_i32\") (result i32) (call $ret_i32))"
    "  (func (export \"call_i64\") (result i64) (call $ret_i64))"
    "  (func (export \"call_f32\") (result f32) (call $ret_f32))"
    "  (func (export \"call_f64\") (result f64) (call $ret_f64))"
    "  (func (export \"extend_call\") (result i64)"
    "    (i64.extend_i32_u (call $ret_i32)))"
    "  (func (export \"reinterpret_call\") (result f32)"
    "    (f32.reinterpret_i32 (call $ret_i32)))"
    "  (func (export \"extend_call_indirect\") (result i64)"
    "    (i64.extend_i32_u (call_indirect (type $ri32) (i32.const 0)))))";

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
        run_t i = run_at_tier(&bin, 0, "sum", i32v(10));
        run_t j = run_at_tier(&bin, 1, "sum", i32v(10));
        CK(!i.trapped && !j.trapped);
        CK(rnum(&i) == 90);              // 10*9
        CK(rnum(&j) == rnum(&i));        // tier choice is semantics-free
        CK(i.jit_count == 0);            // default engine: pure interp
        CK(j.jit_count == 2);            // $dbl + $sum both on the JIT tier
        CK(i.probe_ops > 0);             // the interp fired the debug probe …
        CK(j.probe_ops == 0);            // … and a JIT'd function does not
        printf("  loop+call         interp=%lld jit=%lld  jit_count=%u  probe interp=%u jit=%u [%s]\n",
               (long long)rnum(&i), (long long)rnum(&j), j.jit_count, i.probe_ops, j.probe_ops,
               (rnum(&i) == rnum(&j) && j.jit_count == 2 && j.probe_ops == 0) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    // 1b. a value carried out of a block by a branch — the tiers must agree.
    {
        wasm_byte_vec_t bin; assemble(WAT_BR_VALUE, &bin);
        run_t i = run_at_tier(&bin, 0, "as-block-value", i32v(0));
        run_t j = run_at_tier(&bin, 1, "as-block-value", i32v(0));
        CK(!i.trapped && !j.trapped);
        CK(rnum(&i) == 2);
        CK(rnum(&j) == rnum(&i));
        printf("  br carries value  interp=%lld jit=%lld [%s]\n",
               (long long)rnum(&i), (long long)rnum(&j),
               (rnum(&i) == rnum(&j) && rnum(&i) == 2) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    // 1c. stores with a mixed operand-class pair — round-trip through memory.
    {
        wasm_byte_vec_t bin; assemble(WAT_STORE_MIX, &bin);
        struct { const char* ex; wasm_val_t arg; int64_t want; } cases[] = {
            { "i32_store",   WASM_I32_VAL(0x0DEDCAFE),        0x0DEDCAFE },
            { "i64_store",   WASM_I64_VAL(0x0123456789ABCDLL), 0x0123456789ABCDLL },
            { "i64_store16", WASM_I64_VAL(0xCAFE),            0xCAFE },
            { "i64_store32", WASM_I64_VAL(0x0DEDCAFE),        0x0DEDCAFE },
            { "f32_store",   WASM_F32_VAL(42.0f),             42 },
            { "f64_store",   WASM_F64_VAL(42.0),              42 },
        };
        for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            run_t i = run_at_tier(&bin, 0, cases[k].ex, cases[k].arg);
            run_t j = run_at_tier(&bin, 1, cases[k].ex, cases[k].arg);
            int ok = !i.trapped && !j.trapped
                  && rnum(&i) == cases[k].want && rnum(&j) == rnum(&i);
            CK(ok);
            printf("  %-12s      interp=%lld jit=%lld [%s]\n", cases[k].ex,
                   (long long)rnum(&i), (long long)rnum(&j), ok ? "PASS" : "FAIL");
        }
        wasm_byte_vec_delete(&bin);
    }

    // 1d. a value arriving from a call, per result class.
    {
        wasm_byte_vec_t bin; assemble(WAT_CALL_CLASS, &bin);
        static const char* ex[] = { "call_i32", "call_i64", "call_f32", "call_f64",
                                    "extend_call", "reinterpret_call",
                                    "extend_call_indirect" };
        for (size_t k = 0; k < sizeof ex / sizeof ex[0]; k++) {
            run_t i = run_at_tier(&bin, 0, ex[k], i32v(0));
            run_t j = run_at_tier(&bin, 1, ex[k], i32v(0));
            /* reinterpret_call's bits are not 42 as a float — the interpreter is
             * the oracle, so only AGREEMENT is asserted, not a literal. */
            int ok = !i.trapped && !j.trapped && rnum(&j) == rnum(&i);
            CK(ok);
            printf("  %-20s interp=%lld jit=%lld [%s]\n", ex[k],
                   (long long)rnum(&i), (long long)rnum(&j), ok ? "PASS" : "FAIL");
        }
        wasm_byte_vec_delete(&bin);
    }

    // 2. traps agree across tiers.
    {
        wasm_byte_vec_t bin; assemble(WAT_TRAP, &bin);
        run_t i = run_at_tier(&bin, 0, "boom", i32v(7));
        run_t j = run_at_tier(&bin, 1, "boom", i32v(7));
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
        run_t i = run_at_tier(&bin, 0, "go", i32v(21));
        run_t j = run_at_tier(&bin, 1, "go", i32v(21));
        CK(!i.trapped && !j.trapped);
        CK(rnum(&i) == 42);              // caught payload 21 + field 21
        CK(rnum(&j) == rnum(&i));
        CK(j.jit_count == 2);
        printf("  gc + try/throw    interp=%lld jit=%lld [%s]\n",
               (long long)rnum(&i), (long long)rnum(&j),
               (!i.trapped && !j.trapped && rnum(&i) == 42 && rnum(&j) == 42) ? "PASS" : "FAIL");
        wasm_byte_vec_delete(&bin);
    }

    printf("\ntier selection: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
