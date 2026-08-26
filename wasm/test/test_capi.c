// test_capi.c — the wasm-c-api (public wasm.h) surface, driven the way an embedder would: host
// functions imported + called from wasm, host-trap propagation, multi-result returns, host objects,
// serialize, host-info, trap traces, tags + exceptions, and ref_type/match. Modules are assembled
// from .wat at runtime via the `water` library path (wat_driver), so the test owns its inputs.
#include "wasm.h"
#include "jav_extern.h"           // jav_capi_last_status — the sanctioned non-wasm.h verdict readout
#include "wat_driver.h"          // wat text → jav_module_t (the water assembler front end)
#include "jav_writer.h"          // jav_module_write → §5 binary bytes
#include "bbq_runtime.h"         // bbq_write_ctx_t (the growable output buffer)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INSTRS_TOML "../spec/instructions.toml"   // the test runs from test/

// Assemble a .wat string to a wasm_byte_vec_t (caller deletes). Exits on assembler error.
static int fails = 0;
#define CK(c) do { if (!(c)) { printf("  FAIL: %s (line %d)\n", #c, __LINE__); fails++; } } while (0)
// Expect a rejection trap and OWN it: assert non-NULL, then delete the returned `own wasm_trap_t*`.
#define CK_TRAP(call) do { wasm_trap_t* _t = (call); CK(_t != NULL); if (_t) wasm_trap_delete(_t); } while (0)

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

// ── host callbacks ──────────────────────────────────────────────────────────
// inc: (i32)->(i32) returns arg+1.
static wasm_trap_t* host_inc(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    results->data[0] = (wasm_val_t)WASM_I32_VAL(args->data[0].of.i32 + 1);
    return NULL;
}
// boom: (i32)->(i32) always traps.
static wasm_trap_t* host_boom(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; (void)results;
    wasm_message_t m; wasm_name_new_from_string_nt(&m, "host boom");
    wasm_trap_t* t = wasm_trap_new(NULL, &m);   // copies m (const) — we still own m
    wasm_name_delete(&m);
    return t;
}
// addmul: (i32,i32)->(i32,i32) returns (a+b, a*b) — exercises a multi-result host callback.
static void fin_bump(void* p) { (*(int*)p)++; }   // host-info finalizer: bump a counter

static wasm_trap_t* host_addmul(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    int32_t a = args->data[0].of.i32, b = args->data[1].of.i32;
    results->data[0] = (wasm_val_t)WASM_I32_VAL(a + b);
    results->data[1] = (wasm_val_t)WASM_I32_VAL(a * b);
    return NULL;
}
// takesfunc: (funcref)->(i32) — exercises host-path arg typing; a non-funcref arg must be rejected.
static wasm_trap_t* host_takesfunc(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; results->data[0] = (wasm_val_t)WASM_I32_VAL(1); return NULL;
}
// ret99: ()->(i32) — verifies a host func round-tripped through as_ref/ref_as stays callable.
static wasm_trap_t* host_ret99(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; results->data[0] = (wasm_val_t)WASM_I32_VAL(99); return NULL;
}
// dbl: (i32)->(i32) — a host func placed into a guest table and reached via guest call_indirect.
static wasm_trap_t* host_dbl(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    results->data[0] = (wasm_val_t)WASM_I32_VAL(args->data[0].of.i32 * 2); return NULL;
}
// addenv: (i32)->(i32) returns arg + the env's base — exercises the with-env host-callback closure.
static int g_env_fin_ran = 0;
static void env_fin(void* p) { (void)p; g_env_fin_ran = 1; }
static wasm_trap_t* host_addenv(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    results->data[0] = (wasm_val_t)WASM_I32_VAL(args->data[0].of.i32 + *(int*)env); return NULL;
}
// reenter: RE-ENTERS the engine — calls g_inner (instance B's func) while THIS host call sits on the
// stack of instance A's outer(). Exercises the A3 re-entrancy fix (A's suspended context must survive).
static wasm_func_t* g_inner = NULL;
static wasm_trap_t* host_reenter(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args;
    wasm_val_vec_t none = WASM_EMPTY_VEC; wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r);
    wasm_func_call(g_inner, &none, &rv);   // drives instance B → switches the engine to B's context
    results->data[0] = r[0];
    return NULL;
}

// §3.3.3 debug-extension probe: records the interp op stream the embedder is handed.
static uint8_t g_probe_ops[64]; static int g_probe_ops_n;
static void probe_record(void* ctx, uint8_t op) { (void)ctx; if (g_probe_ops_n < 64) g_probe_ops[g_probe_ops_n++] = op; }

int main(void) {
    /* Explicit interpreter: the engine default is tier 2 (JAV_DEFAULT_TIER). This suite is
     * about the C API's own behaviour, so it names its tier rather than inheriting one —
     * the default is pinned separately, below. */
    wasm_config_t* cfg0 = wasm_config_new(); jav_config_set_jit(cfg0, 0);
    wasm_engine_t* engine = wasm_engine_new_with_config(cfg0);
    wasm_store_t* store = wasm_store_new(engine);

    // ── (4a) a host func imported as env.f, called from wasm: callit(41) → f(41) = 42 ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"f\" (func $f (param i32) (result i32)))"
            "        (func $callit (param i32) (result i32) local.get 0 call $f)"
            "        (export \"callit\" (func $callit)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);

        wasm_functype_t* ft = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* f = wasm_func_new(store, ft, host_inc);
        wasm_functype_delete(ft);

        wasm_extern_t* imports[1] = { wasm_func_as_extern(f) };
        wasm_extern_vec_t iv = WASM_ARRAY_VEC(imports);
        wasm_trap_t* trap = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &trap);
        CK(inst != NULL && trap == NULL);

        wasm_extern_vec_t exports; wasm_instance_exports(inst, &exports);
        wasm_func_t* callit = wasm_extern_as_func(exports.data[0]);
        wasm_val_t args[1] = { WASM_I32_VAL(41) }, res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(args), rv = WASM_ARRAY_VEC(res);
        trap = wasm_func_call(callit, &av, &rv);
        CK(trap == NULL);
        CK(res[0].of.i32 == 42);

        wasm_extern_vec_delete(&exports);
        wasm_instance_delete(inst);
        wasm_func_delete(f);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4a) a host func that traps propagates a trap through call_indirect/call ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"f\" (func $f (param i32) (result i32)))"
            "        (func $callit (param i32) (result i32) local.get 0 call $f)"
            "        (export \"callit\" (func $callit)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        wasm_functype_t* ft = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* f = wasm_func_new(store, ft, host_boom);
        wasm_functype_delete(ft);
        wasm_extern_t* imports[1] = { wasm_func_as_extern(f) };
        wasm_extern_vec_t iv = WASM_ARRAY_VEC(imports);
        wasm_trap_t* t = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &t);
        CK(inst != NULL);
        wasm_extern_vec_t exports; wasm_instance_exports(inst, &exports);
        wasm_func_t* callit = wasm_extern_as_func(exports.data[0]);
        wasm_val_t args[1] = { WASM_I32_VAL(7) }, res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(args), rv = WASM_ARRAY_VEC(res);
        wasm_trap_t* trap = wasm_func_call(callit, &av, &rv);
        CK(trap != NULL);                 // the host trap surfaced as a wasm_trap_t
        if (trap) wasm_trap_delete(trap);
        wasm_extern_vec_delete(&exports);
        wasm_instance_delete(inst);
        wasm_func_delete(f);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4b) a multi-result module function: swap (i32,i32)->(i32,i32) ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func $swap (param i32 i32) (result i32 i32) local.get 1 local.get 0)"
            "        (export \"swap\" (func $swap)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        wasm_extern_vec_t no_imports = WASM_EMPTY_VEC;
        wasm_trap_t* t = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imports, &t);
        CK(inst != NULL);
        wasm_extern_vec_t exports; wasm_instance_exports(inst, &exports);
        wasm_func_t* swap = wasm_extern_as_func(exports.data[0]);
        CK(wasm_func_result_arity(swap) == 2);
        wasm_val_t args[2] = { WASM_I32_VAL(3), WASM_I32_VAL(8) };
        wasm_val_t res[2] = { WASM_INIT_VAL, WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(args), rv = WASM_ARRAY_VEC(res);
        wasm_trap_t* trap = wasm_func_call(swap, &av, &rv);
        CK(trap == NULL);
        CK(res[0].of.i32 == 8 && res[1].of.i32 == 3);
        wasm_extern_vec_delete(&exports);
        wasm_instance_delete(inst);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4a) a multi-result HOST func, called directly through the api ──
    {
        wasm_functype_t* ft = wasm_functype_new_2_2(
            wasm_valtype_new_i32(), wasm_valtype_new_i32(),
            wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* f = wasm_func_new(store, ft, host_addmul);
        wasm_functype_delete(ft);
        wasm_val_t args[2] = { WASM_I32_VAL(4), WASM_I32_VAL(5) };
        wasm_val_t res[2] = { WASM_INIT_VAL, WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(args), rv = WASM_ARRAY_VEC(res);
        wasm_trap_t* trap = wasm_func_call(f, &av, &rv);
        CK(trap == NULL);
        CK(res[0].of.i32 == 9 && res[1].of.i32 == 20);
        wasm_func_delete(f);
    }

    // ── (4c) accessors + (4d) reflection over an instance with global/memory/table/func ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (global (export \"g\") (mut i32) (i32.const 7))"
            "        (memory (export \"m\") 1 4)"
            "        (table (export \"t\") 2 funcref)"
            "        (func (export \"f\") (result i32) global.get 0))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);

        // (4d) module-level reflection: 4 exports, names + kinds, parallel to instance order.
        wasm_exporttype_vec_t ets; wasm_module_exports(mod, &ets);
        CK(ets.size == 4);
        const char* want_names[4] = { "g", "m", "t", "f" };
        wasm_externkind_t want_kinds[4] = { WASM_EXTERN_GLOBAL, WASM_EXTERN_MEMORY, WASM_EXTERN_TABLE, WASM_EXTERN_FUNC };
        int gi = -1, mi = -1, ti = -1, fi = -1;
        for (size_t i = 0; i < ets.size && i < 4; i++) {
            const wasm_name_t* nm = wasm_exporttype_name(ets.data[i]);
            CK(nm->size == 1 && nm->data[0] == want_names[i][0]);
            CK(wasm_externtype_kind(wasm_exporttype_type(ets.data[i])) == want_kinds[i]);
            if (nm->data[0] == 'g') gi = (int)i; else if (nm->data[0] == 'm') mi = (int)i;
            else if (nm->data[0] == 't') ti = (int)i; else if (nm->data[0] == 'f') fi = (int)i;
        }
        wasm_exporttype_vec_delete(&ets);

        wasm_extern_vec_t no_imports = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imports, &tr);
        CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);

        // (4c) global: read 7, write 99, read back, and observe it through the exported func.
        wasm_global_t* g = wasm_extern_as_global(ex.data[gi]);
        wasm_val_t gv = WASM_INIT_VAL; wasm_global_get(g, &gv);
        CK(gv.kind == WASM_I32 && gv.of.i32 == 7);
        wasm_val_t set = WASM_I32_VAL(99); wasm_global_set(g, &set);
        wasm_global_get(g, &gv); CK(gv.of.i32 == 99);
        wasm_func_t* f = wasm_extern_as_func(ex.data[fi]);
        wasm_val_t res[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(res);
        wasm_val_vec_t empty = WASM_EMPTY_VEC;
        CK(wasm_func_call(f, &empty, &rv) == NULL && res[0].of.i32 == 99);

        // (4c) memory: 1 page → grow 1 → 2 pages; data_size tracks.
        wasm_memory_t* m = wasm_extern_as_memory(ex.data[mi]);
        CK(wasm_memory_size(m) == 1);
        CK(wasm_memory_data_size(m) == 65536);
        CK(wasm_memory_grow(m, 1) == true);
        CK(wasm_memory_size(m) == 2);

        // (4c) table: declared size 2.
        wasm_table_t* t = wasm_extern_as_table(ex.data[ti]);
        CK(wasm_table_size(t) == 2);

        wasm_extern_vec_delete(&ex);
        wasm_instance_delete(inst);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4d) imports reflection: import_call declares env.f : (i32)->(i32) ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"f\" (func $f (param i32) (result i32)))"
            "        (func $callit (param i32) (result i32) local.get 0 call $f)"
            "        (export \"callit\" (func $callit)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        wasm_importtype_vec_t its; wasm_module_imports(mod, &its);
        CK(its.size == 1);
        if (its.size == 1) {
            const wasm_name_t* mn = wasm_importtype_module(its.data[0]);
            const wasm_name_t* nn = wasm_importtype_name(its.data[0]);
            CK(mn->size == 3 && memcmp(mn->data, "env", 3) == 0);
            CK(nn->size == 1 && nn->data[0] == 'f');
            CK(wasm_externtype_kind(wasm_importtype_type(its.data[0])) == WASM_EXTERN_FUNC);
        }
        wasm_importtype_vec_delete(&its);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4e-i) funcref VALUES: a func returns a funcref (ref.func 0); table get/set ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module"
            "  (func $id (param i32) (result i32) local.get 0)"
            "  (table (export \"t\") 2 funcref)"
            "  (func (export \"getref\") (result funcref) ref.func $id)"
            "  (func (export \"isnull\") (result funcref) ref.null func)"
            "  (elem (i32.const 0) $id))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        wasm_extern_vec_t no_imports = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imports, &tr);
        CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        // exports order: t(table), getref(func), isnull(func)
        int et = -1, eg = -1, en = -1;
        wasm_exporttype_vec_t names; wasm_module_exports(mod, &names);
        for (size_t i = 0; i < names.size; i++) {
            const wasm_name_t* nm = wasm_exporttype_name(names.data[i]);
            if (nm->size == 1 && nm->data[0] == 't') et = (int)i;
            else if (!strncmp(nm->data, "getref", nm->size)) eg = (int)i;
            else if (!strncmp(nm->data, "isnull", nm->size)) en = (int)i;
        }
        wasm_exporttype_vec_delete(&names);

        // getref() returns a non-null funcref; isnull() returns the null funcref.
        wasm_func_t* getref = wasm_extern_as_func(ex.data[eg]);
        wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r);
        wasm_val_vec_t no_args = WASM_EMPTY_VEC;
        CK(wasm_func_call(getref, &no_args, &rv) == NULL);
        CK(r[0].kind == WASM_FUNCREF && r[0].of.ref != NULL);
        // A1 cross-boundary: the host RECOVERS the returned guest funcref to a callable view and calls
        // $id back into the guest (host→guest, non-re-entrant — getref already returned).
        wasm_func_t* recovered = wasm_ref_as_func((wasm_ref_t*)r[0].of.ref); CK(recovered != NULL);
        if (recovered) {
            wasm_val_t a7[1] = { WASM_I32_VAL(7) }, r7[1] = { WASM_INIT_VAL };
            wasm_val_vec_t av7 = WASM_ARRAY_VEC(a7), rv7 = WASM_ARRAY_VEC(r7);
            CK(wasm_func_call(recovered, &av7, &rv7) == NULL && r7[0].of.i32 == 7);   // $id(7) = 7
            wasm_func_delete(recovered);
        }
        wasm_val_delete(&r[0]);

        wasm_func_t* isnull = wasm_extern_as_func(ex.data[en]);
        wasm_val_t r2[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv2 = WASM_ARRAY_VEC(r2);
        CK(wasm_func_call(isnull, &no_args, &rv2) == NULL);
        CK(r2[0].kind == WASM_FUNCREF && r2[0].of.ref == NULL);   // null funcref = NULL ref

        // table.get(0) yields the elem-installed funcref; slot 1 is null; set + read back.
        wasm_table_t* t = wasm_extern_as_table(ex.data[et]);
        CK(wasm_table_size(t) == 2);
        wasm_ref_t* g0 = NULL; CK(wasm_table_read(t, 0, &g0) && g0 != NULL);
        wasm_ref_t* gn = NULL; CK(wasm_table_read(t, 1, &gn) && gn == NULL);   // in range, null slot
        CK(wasm_table_set(t, 1, g0) == true);                      // copy the funcref into slot 1
        wasm_ref_t* g1 = NULL; CK(wasm_table_read(t, 1, &g1) && g1 != NULL && wasm_ref_same(g0, g1));
        wasm_ref_t* goob = NULL; CK(!wasm_table_read(t, 9, &goob));            // OOB is an ERROR
        wasm_ref_delete(g0); wasm_ref_delete(g1);

        wasm_extern_vec_delete(&ex);
        wasm_instance_delete(inst);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── (4e-ii) externref host VALUES via the GC host-box ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module"
            "  (global $g (mut externref) (ref.null extern))"
            "  (func (export \"id\")   (param externref) (result externref) local.get 0)"
            "  (func (export \"setg\") (param externref) local.get 0 global.set $g)"
            "  (func (export \"getg\") (result externref) global.get $g))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        if (mod) {
            wasm_extern_vec_t no_imports = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
            wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imports, &tr);
            CK(inst != NULL);
            wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);   // [id, setg, getg]
            wasm_func_t* id = wasm_extern_as_func(ex.data[0]);
            wasm_func_t* setg = wasm_extern_as_func(ex.data[1]);
            wasm_func_t* getg = wasm_extern_as_func(ex.data[2]);
            wasm_val_vec_t no_args = WASM_EMPTY_VEC, no_res = WASM_EMPTY_VEC;

            // mint a host reference (a foreign object) and pass it as an externref.
            wasm_foreign_t* foreign = wasm_foreign_new(store);
            wasm_ref_t* eref = wasm_foreign_as_ref(foreign);

            // id(eref) returns the SAME host identity.
            wasm_val_t a[1]; a[0].kind = WASM_EXTERNREF; a[0].of.ref = eref;
            wasm_val_t r[1] = { WASM_INIT_VAL };
            wasm_val_vec_t av = WASM_ARRAY_VEC(a), rv = WASM_ARRAY_VEC(r);
            CK(wasm_func_call(id, &av, &rv) == NULL);
            CK(r[0].kind == WASM_EXTERNREF && r[0].of.ref != NULL && wasm_ref_same(r[0].of.ref, eref));
            wasm_val_delete(&r[0]);

            // store it in a global, then churn host-box allocations (GC pressure), then read
            // it back — the global's box is a T_GCREF root and survives.
            wasm_val_t sa[1]; sa[0].kind = WASM_EXTERNREF; sa[0].of.ref = eref;
            wasm_val_vec_t sav = WASM_ARRAY_VEC(sa);
            CK(wasm_func_call(setg, &sav, &no_res) == NULL);
            for (int k = 0; k < 20000; k++) {
                wasm_foreign_t* tmp = wasm_foreign_new(store);
                wasm_val_t ta[1]; ta[0].kind = WASM_EXTERNREF; ta[0].of.ref = wasm_foreign_as_ref(tmp);
                wasm_val_t tr2[1] = { WASM_INIT_VAL };
                wasm_val_vec_t tav = WASM_ARRAY_VEC(ta), trv = WASM_ARRAY_VEC(tr2);
                wasm_func_call(id, &tav, &trv);
                wasm_val_delete(&ta[0]); wasm_val_delete(&tr2[0]); wasm_foreign_delete(tmp);
            }
            wasm_val_t gr[1] = { WASM_INIT_VAL }; wasm_val_vec_t grv = WASM_ARRAY_VEC(gr);
            CK(wasm_func_call(getg, &no_args, &grv) == NULL);
            CK(gr[0].kind == WASM_EXTERNREF && gr[0].of.ref != NULL && wasm_ref_same(gr[0].of.ref, eref));
            CK(wasm_ref_as_foreign(gr[0].of.ref) == foreign);   // identity recovered
            wasm_val_delete(&gr[0]);

            // a null externref round-trips as the NULL reference.
            wasm_val_t na[1] = { WASM_INIT_VAL }, nr[1] = { WASM_INIT_VAL };
            wasm_val_vec_t nav = WASM_ARRAY_VEC(na), nrv = WASM_ARRAY_VEC(nr);
            CK(wasm_func_call(id, &nav, &nrv) == NULL);
            CK(nr[0].of.ref == NULL);

            wasm_ref_delete(eref); wasm_foreign_delete(foreign);
            wasm_extern_vec_delete(&ex);
            wasm_instance_delete(inst);
        }
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── externref TABLES: wasm table.set/get/fill on (table externref) + the c-api table
    //    ops + GC survival of table entries (the slot-sized, GC-traced table store) ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module"
            "  (table (export \"t\") 3 externref)"
            "  (func (export \"set\")    (param i32 externref) local.get 0 local.get 1 table.set 0)"
            "  (func (export \"get\")    (param i32) (result externref) local.get 0 table.get 0)"
            "  (func (export \"isnull\") (param i32) (result i32) local.get 0 table.get 0 ref.is_null)"
            "  (func (export \"fillall\")(param externref) i32.const 0 local.get 0 i32.const 3 table.fill 0))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        if (mod) {
            wasm_extern_vec_t no_imports = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
            wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imports, &tr);
            CK(inst != NULL);
            wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);  // [t, set, get, isnull, fillall]
            wasm_table_t* tab = wasm_extern_as_table(ex.data[0]);
            wasm_func_t* set = wasm_extern_as_func(ex.data[1]);
            wasm_func_t* get = wasm_extern_as_func(ex.data[2]);
            wasm_func_t* isnull = wasm_extern_as_func(ex.data[3]);
            wasm_func_t* fillall = wasm_extern_as_func(ex.data[4]);
            CK(tab && set && get && isnull && fillall);

            wasm_foreign_t* f1 = wasm_foreign_new(store);
            wasm_ref_t* e1 = wasm_foreign_as_ref(f1);
            wasm_val_vec_t no_res = WASM_EMPTY_VEC;

            // wasm table.set 0[1] = e1, then wasm table.get 0[1] round-trips by identity.
            wasm_val_t sa[2]; sa[0] = (wasm_val_t)WASM_I32_VAL(1); sa[1].kind = WASM_EXTERNREF; sa[1].of.ref = e1;
            wasm_val_vec_t sav = WASM_ARRAY_VEC(sa);
            CK(wasm_func_call(set, &sav, &no_res) == NULL);
            wasm_val_t ga[1] = { WASM_I32_VAL(1) }, gr[1] = { WASM_INIT_VAL };
            wasm_val_vec_t gav = WASM_ARRAY_VEC(ga), grv = WASM_ARRAY_VEC(gr);
            CK(wasm_func_call(get, &gav, &grv) == NULL);
            CK(gr[0].kind == WASM_EXTERNREF && wasm_ref_same(gr[0].of.ref, e1));
            wasm_val_delete(&gr[0]);

            // the c-api reads the same wasm-set externref by identity.
            wasm_ref_t* c0 = NULL; CK(wasm_table_read(tab, 1, &c0));
            CK(c0 && wasm_ref_same(c0, e1) && wasm_ref_as_foreign(c0) == f1);
            wasm_ref_delete(c0);

            // c-api write then wasm read.
            wasm_foreign_t* f2 = wasm_foreign_new(store); wasm_ref_t* e2 = wasm_foreign_as_ref(f2);
            CK(wasm_table_set(tab, 2, e2) == true);
            wasm_val_t ga2[1] = { WASM_I32_VAL(2) }, gr2[1] = { WASM_INIT_VAL };
            wasm_val_vec_t gav2 = WASM_ARRAY_VEC(ga2), grv2 = WASM_ARRAY_VEC(gr2);
            CK(wasm_func_call(get, &gav2, &grv2) == NULL && wasm_ref_same(gr2[0].of.ref, e2));
            wasm_val_delete(&gr2[0]);

            // table.fill via wasm fills every slot with e1; slot 0 (was null) is now non-null.
            wasm_val_t fa[1]; fa[0].kind = WASM_EXTERNREF; fa[0].of.ref = e1;
            wasm_val_vec_t fav = WASM_ARRAY_VEC(fa);
            CK(wasm_func_call(fillall, &fav, &no_res) == NULL);
            wasm_val_t ia[1] = { WASM_I32_VAL(0) }, ir[1] = { WASM_INIT_VAL };
            wasm_val_vec_t iav = WASM_ARRAY_VEC(ia), irv = WASM_ARRAY_VEC(ir);
            CK(wasm_func_call(isnull, &iav, &irv) == NULL && ir[0].of.i32 == 0);   // non-null after fill

            // c-api grow with an externref init, then read the new slot.
            CK(wasm_table_grow(tab, 2, e2) == true);
            CK(wasm_table_size(tab) == 5);
            wasm_ref_t* g4 = NULL; CK(wasm_table_read(tab, 4, &g4));
            CK(g4 && wasm_ref_same(g4, e2));
            wasm_ref_delete(g4);

            // GC churn: many throwaway host boxes overwrite slot 1; slot 4 (= e2, from grow)
            // is untouched and must survive collection (it's a T_GCREF table root).
            for (int k = 0; k < 20000; k++) {
                wasm_foreign_t* tmp = wasm_foreign_new(store);
                wasm_ref_t* te = wasm_foreign_as_ref(tmp);
                wasm_val_t ta[2]; ta[0] = (wasm_val_t)WASM_I32_VAL(1); ta[1].kind = WASM_EXTERNREF; ta[1].of.ref = te;
                wasm_val_vec_t tav = WASM_ARRAY_VEC(ta);
                wasm_func_call(set, &tav, &no_res);   // overwrite slot 1 with churn
                wasm_ref_delete(te); wasm_foreign_delete(tmp);
            }
            wasm_ref_t* survive = NULL; CK(wasm_table_read(tab, 4, &survive));
            CK(survive && wasm_ref_same(survive, e2));   // slot 4's externref survived GC churn
            wasm_ref_delete(survive);

            wasm_ref_delete(e1); wasm_ref_delete(e2);
            wasm_foreign_delete(f1); wasm_foreign_delete(f2);
            wasm_extern_vec_delete(&ex);
            wasm_instance_delete(inst);
        }
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── host-created (standalone) objects: direct use + linked as imports + serialize ──
    {
        // direct: a mutable i32 global, a memory, a funcref table.
        wasm_globaltype_t* gt = wasm_globaltype_new(wasm_valtype_new_i32(), WASM_VAR);
        wasm_val_t gv0 = WASM_I32_VAL(42);
        wasm_global_t* hg = wasm_global_new(store, gt, &gv0);
        wasm_globaltype_delete(gt);
        wasm_val_t gg = WASM_INIT_VAL; wasm_global_get(hg, &gg);
        CK(gg.kind == WASM_I32 && gg.of.i32 == 42);
        wasm_val_t gv1 = WASM_I32_VAL(99); wasm_global_set(hg, &gv1);
        wasm_global_get(hg, &gg); CK(gg.of.i32 == 99);

        wasm_limits_t mlim = { 1, 4, false };
        wasm_memorytype_t* mt = wasm_memorytype_new(WASM_I32, &mlim);
        wasm_memory_t* hm = wasm_memory_new(store, mt);
        wasm_memorytype_delete(mt);
        CK(wasm_memory_size(hm) == 1 && wasm_memory_data(hm) != NULL);
        CK(wasm_memory_grow(hm, 1) == true && wasm_memory_size(hm) == 2);

        wasm_limits_t tlim = { 2, 0, true };
        wasm_tabletype_t* tt2 = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &tlim);
        wasm_table_t* ht = wasm_table_new(store, tt2, NULL);
        wasm_tabletype_delete(tt2);
        { wasm_ref_t* h0 = NULL;
          CK(wasm_table_size(ht) == 2 && wasm_table_read(ht, 0, &h0) && h0 == NULL); }

        // link the host global as an import; the module reads it back.
        wasm_byte_vec_t bin; assemble(
            "(module (import \"h\" \"g\" (global $g i32))"
            "        (func (export \"readg\") (result i32) global.get $g))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        wasm_globaltype_t* gt2 = wasm_globaltype_new(wasm_valtype_new_i32(), WASM_CONST);
        wasm_val_t cv = WASM_I32_VAL(123);
        wasm_global_t* hg2 = wasm_global_new(store, gt2, &cv);
        wasm_globaltype_delete(gt2);
        wasm_extern_t* imps[1] = { wasm_global_as_extern(hg2) };
        wasm_extern_vec_t iv = WASM_ARRAY_VEC(imps); wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &tr);
        CK(inst != NULL);
        if (inst) {
            wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
            wasm_func_t* readg = wasm_extern_as_func(ex.data[0]);
            wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r), na = WASM_EMPTY_VEC;
            CK(wasm_func_call(readg, &na, &rv) == NULL && r[0].of.i32 == 123);   // host global linked in
            wasm_extern_vec_delete(&ex);
            wasm_instance_delete(inst);
        }

        // serialize → deserialize → re-instantiate (lossless module round-trip).
        wasm_byte_vec_t ser; wasm_module_serialize(mod, &ser);
        wasm_module_t* mod2 = wasm_module_deserialize(store, &ser);
        CK(mod2 != NULL);
        wasm_byte_vec_delete(&ser);

        wasm_global_delete(hg); wasm_global_delete(hg2);
        wasm_memory_delete(hm); wasm_table_delete(ht);
        wasm_module_delete(mod); wasm_module_delete(mod2);
        wasm_byte_vec_delete(&bin);
    }

    // ── host-info + finalizer ──
    {
        static int fin_count = 0;
        wasm_globaltype_t* gt = wasm_globaltype_new(wasm_valtype_new_i32(), WASM_VAR);
        wasm_val_t z = WASM_I32_VAL(0);
        wasm_global_t* hg = wasm_global_new(store, gt, &z);
        wasm_globaltype_delete(gt);
        int marker = 7;
        CK(wasm_global_get_host_info(hg) == NULL);
        wasm_global_set_host_info(hg, &marker);
        CK(wasm_global_get_host_info(hg) == &marker);
        // attach a finalizer that bumps fin_count, then delete → it runs.
        wasm_global_set_host_info_with_finalizer(hg, &fin_count, fin_bump);
        wasm_global_delete(hg);
        CK(fin_count == 1);   // finalizer ran on delete
    }

    // ── trap stack trace: unreachable in $inner, propagating through $outer and go ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func $inner unreachable)"
            "        (func $outer call $inner)"
            "        (func (export \"go\") call $outer))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr);
        CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* go = wasm_extern_as_func(ex.data[0]);
        wasm_val_vec_t na = WASM_EMPTY_VEC, nr = WASM_EMPTY_VEC;
        wasm_trap_t* trap = wasm_func_call(go, &na, &nr);
        CK(trap != NULL);
        if (trap) {
            wasm_frame_t* origin = wasm_trap_origin(trap);
            CK(origin != NULL && wasm_frame_func_index(origin) == 0);     // $inner is funcidx 0
            CK(wasm_frame_instance(origin) == inst);
            wasm_frame_t* ocopy = wasm_frame_copy(origin);               // 6.0 cov: frame_copy round-trips index+instance
            CK(ocopy && wasm_frame_func_index(ocopy) == 0 && wasm_frame_instance(ocopy) == inst);
            wasm_frame_delete(ocopy);
            wasm_frame_delete(origin);
            wasm_frame_vec_t fr; wasm_trap_trace(trap, &fr);
            CK(fr.size == 3);                                             // inner, outer, go
            if (fr.size == 3) {
                CK(wasm_frame_func_index(fr.data[0]) == 0);               // innermost = $inner
                CK(wasm_frame_func_index(fr.data[1]) == 1);               // $outer
                CK(wasm_frame_func_index(fr.data[2]) == 2);               // go
            }
            wasm_frame_vec_delete(&fr);
            wasm_trap_delete(trap);
        }
        wasm_extern_vec_delete(&ex);
        wasm_instance_delete(inst);
        wasm_module_delete(mod);
        wasm_byte_vec_delete(&bin);
    }

    // ── §7.1.8/§7.1.12 exception OUTCOME: a defined tag thrown + escaping is an exception, not a
    //    trap; the embedder reads its tag + values (exn_read / exn_tag) ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (tag $e (param i32 i64))"
            "        (func (export \"thr\") (param i32 i64) local.get 0 local.get 1 throw $e))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* thr = wasm_extern_as_func(ex.data[0]);
        wasm_val_t av[2] = { WASM_I32_VAL(7), WASM_I64_VAL(99) };
        wasm_val_vec_t args = WASM_ARRAY_VEC(av), res = WASM_EMPTY_VEC;
        wasm_trap_t* trap = wasm_func_call(thr, &args, &res);
        CK(trap != NULL && wasm_trap_is_exception(trap));   // §7.1.8 the `exception` outcome
        wasm_exception_t* exn = trap ? wasm_trap_exception(trap) : NULL;
        CK(exn != NULL);
        if (exn) {
            wasm_val_vec_t vals; wasm_exception_read(exn, &vals);   // §7.1.12 exn_read
            CK(vals.size == 2 && vals.data[0].of.i32 == 7 && vals.data[1].of.i64 == 99);
            wasm_val_vec_delete(&vals);
            wasm_tag_t* etag = wasm_exception_tag(exn); CK(etag != NULL); wasm_tag_delete(etag);   // exn_tag
        }
        if (trap) wasm_trap_delete(trap);
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── A2/B4: the exception store has NO cap and does NOT leak across calls (was a monotonic
    //    vm->exns[256] never reset by wasm_func_call). 300 escaping throws on ONE store — every one,
    //    incl. #257+, must surface as an EXCEPTION, not a bare trap. Pre-fix throw #257 hit the
    //    256-cap's EXN_TRAP and returned a plain trap. (Exns are now GC objects, reclaimed by liveness.)
    {
        wasm_byte_vec_t bin; assemble(
            "(module (tag $e (param i32))"
            "        (func (export \"boom\") (param i32) local.get 0 throw $e))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* boom = wasm_extern_as_func(ex.data[0]);
        int all_exn = 1;
        for (int i = 0; i < 300; i++) {
            wasm_val_t av[1] = { WASM_I32_VAL(i) };
            wasm_val_vec_t args = WASM_ARRAY_VEC(av), res = WASM_EMPTY_VEC;
            wasm_trap_t* t = wasm_func_call(boom, &args, &res);
            if (!t || !wasm_trap_is_exception(t)) all_exn = 0;
            if (t) wasm_trap_delete(t);
        }
        CK(all_exn);   // no 256-cap: every throw is an exception
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── B3: an exception carries MORE than 16 fields (was a fixed exn.fields[16], silently
    //    truncated). A tag with 17 i32 params; the 17th value (index 16) must read back correctly. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (tag $e (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))"
            "        (func (export \"thr17\")"
            "          i32.const 100 i32.const 101 i32.const 102 i32.const 103 i32.const 104 i32.const 105"
            "          i32.const 106 i32.const 107 i32.const 108 i32.const 109 i32.const 110 i32.const 111"
            "          i32.const 112 i32.const 113 i32.const 114 i32.const 115 i32.const 116 throw $e))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_val_vec_t args = WASM_EMPTY_VEC, res = WASM_EMPTY_VEC;
        wasm_trap_t* trap = wasm_func_call(wasm_extern_as_func(ex.data[0]), &args, &res);
        CK(trap && wasm_trap_is_exception(trap));
        wasm_exception_t* exn = trap ? wasm_trap_exception(trap) : NULL;
        if (exn) {
            wasm_val_vec_t vals; wasm_exception_read(exn, &vals);
            CK(vals.size == 17 && vals.data[16].of.i32 == 116);   // the 17th field survives (no fields[16] cap)
            wasm_val_vec_delete(&vals);
        }
        if (trap) wasm_trap_delete(trap);
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── §7.1.11 tag_alloc: a host-created tag imported into a module; the exception it throws
    //    carries that tag's identity (wasm_tag_same) ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"t\" (tag $t (param i32)))"
            "        (func (export \"boom\") (param i32) local.get 0 throw $t))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_valtype_vec_t p; wasm_valtype_vec_new_uninitialized(&p, 1); p.data[0] = wasm_valtype_new_i32();
        wasm_valtype_vec_t r; wasm_valtype_vec_new_empty(&r);
        wasm_tagtype_t* tt = wasm_tagtype_new(wasm_functype_new(&p, &r));
        wasm_tag_t* htag = wasm_tag_new(store, tt); wasm_tagtype_delete(tt);   // §7.1.11 tag_alloc
        CK(htag != NULL);
        wasm_extern_t* imports[1] = { wasm_tag_as_extern(htag) };
        wasm_extern_vec_t iv = WASM_ARRAY_VEC(imports); wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &tr); CK(inst != NULL);
        if (inst) {
            wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
            wasm_func_t* boom = wasm_extern_as_func(ex.data[0]);
            wasm_val_t av[1] = { WASM_I32_VAL(5) }; wasm_val_vec_t args = WASM_ARRAY_VEC(av), res = WASM_EMPTY_VEC;
            wasm_trap_t* trap = wasm_func_call(boom, &args, &res);
            CK(trap != NULL && wasm_trap_is_exception(trap));
            wasm_exception_t* exn = trap ? wasm_trap_exception(trap) : NULL;
            if (exn) { wasm_tag_t* et = wasm_exception_tag(exn); CK(et && wasm_tag_same(et, htag)); wasm_tag_delete(et); }
            if (trap) wasm_trap_delete(trap);
            wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        }
        wasm_tag_delete(htag); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── §7.1.12 exn_alloc: host creates an exception (host tag + values), reads it back ──
    {
        wasm_valtype_vec_t p; wasm_valtype_vec_new_uninitialized(&p, 2);
        p.data[0] = wasm_valtype_new_i32(); p.data[1] = wasm_valtype_new(WASM_F64);
        wasm_valtype_vec_t r; wasm_valtype_vec_new_empty(&r);
        wasm_tagtype_t* tt = wasm_tagtype_new(wasm_functype_new(&p, &r));
        wasm_tag_t* tag = wasm_tag_new(store, tt); wasm_tagtype_delete(tt);
        wasm_val_t av[2] = { WASM_I32_VAL(42), WASM_F64_VAL(2.5) };
        wasm_val_vec_t args = WASM_ARRAY_VEC(av);
        wasm_exception_t* exn = wasm_exception_new(store, tag, &args); CK(exn != NULL);   // §7.1.12 exn_alloc
        if (exn) {
            wasm_val_vec_t vals; wasm_exception_read(exn, &vals);
            CK(vals.size == 2 && vals.data[0].of.i32 == 42 && vals.data[1].of.f64 == 2.5);
            wasm_val_vec_delete(&vals);
            wasm_tag_t* et = wasm_exception_tag(exn); CK(et && wasm_tag_same(et, tag)); wasm_tag_delete(et);
            wasm_exception_delete(exn);
        }
        wasm_tag_delete(tag);
    }

    // ── §7.1.14/§7.1.15 ref_type + match_valtype: a returned (ref i31)'s runtime type is i31, which
    //    is ≤ eq but not ≤ struct ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func (export \"mk\") (result (ref i31)) i32.const 5 ref.i31))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        if (inst) {
            wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
            wasm_func_t* mk = wasm_extern_as_func(ex.data[0]);
            wasm_val_t rv[1]; wasm_val_vec_t na = WASM_EMPTY_VEC, res = WASM_ARRAY_VEC(rv);
            wasm_trap_t* trap = wasm_func_call(mk, &na, &res);
            CK(trap == NULL && res.data[0].of.ref != NULL);
            if (!trap && res.data[0].of.ref) {
                wasm_valtype_t* rt = wasm_ref_type(store, res.data[0].of.ref);   // §7.1.14 ref_type
                CK(wasm_valtype_kind(rt) == WASM_I31REF);
                wasm_valtype_t* eq = wasm_valtype_new(WASM_EQREF);
                wasm_valtype_t* st = wasm_valtype_new(WASM_STRUCTREF);
                CK(wasm_match_valtype(store, rt, eq));      // §7.1.15 i31 ≤ eq
                CK(!wasm_match_valtype(store, rt, st));     // i31 ≰ struct
                wasm_valtype_delete(rt); wasm_valtype_delete(eq); wasm_valtype_delete(st);
                wasm_val_delete(&res.data[0]);
            }
            wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        }
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── (B2) wasm_func_call validates args against the signature — fail-closed on the host entry.
    // Verified fail-open: a host number passed where a ref is expected was used by the guest as a ref
    // (wild deref); a wrong arity underflowed the operand stack. The call must REJECT, not run. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func (export \"f_i32\") (param i32) (result i32) local.get 0)"
            "        (func (export \"f_ref\") (param externref) (result i32) (ref.is_null (local.get 0))))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t no_imp = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imp, &tr);
        CK(inst != NULL && tr == NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* f_i32 = wasm_extern_as_func(ex.data[0]);
        wasm_func_t* f_ref = wasm_extern_as_func(ex.data[1]);
        wasm_val_t res[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(res);

        wasm_val_t good[1] = { WASM_I32_VAL(7) }; wasm_val_vec_t goodv = WASM_ARRAY_VEC(good);
        CK(wasm_func_call(f_i32, &goodv, &rv) == NULL && res[0].of.i32 == 7);   // control: correct call works

        wasm_val_vec_t none = WASM_EMPTY_VEC;
        CK_TRAP(wasm_func_call(f_i32, &none, &rv));                             // too few args → reject
        wasm_val_t many[2] = { WASM_I32_VAL(1), WASM_I32_VAL(2) }; wasm_val_vec_t manyv = WASM_ARRAY_VEC(many);
        CK_TRAP(wasm_func_call(f_i32, &manyv, &rv));                            // too many args → reject
        wasm_val_t wrongnum[1] = { WASM_F32_VAL(1.0f) }; wasm_val_vec_t wnv = WASM_ARRAY_VEC(wrongnum);
        CK_TRAP(wasm_func_call(f_i32, &wnv, &rv));                              // f32 for i32 → reject

        wasm_val_t numforref[1] = { WASM_I32_VAL(5) }; wasm_val_vec_t nfrv = WASM_ARRAY_VEC(numforref);
        CK_TRAP(wasm_func_call(f_ref, &nfrv, &rv));                             // number for ref param → reject (no deref)
        wasm_val_t nullref[1] = { WASM_INIT_VAL }; wasm_val_vec_t nrv = WASM_ARRAY_VEC(nullref);
        CK(wasm_func_call(f_ref, &nrv, &rv) == NULL);                           // control: null externref arg works

        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── (B2-host) a wasm_func_new host func validates ref args by §7.1.15 subtyping too ──
    {
        wasm_functype_t* htt = wasm_functype_new_1_1(wasm_valtype_new(WASM_FUNCREF), wasm_valtype_new_i32());
        wasm_func_t* hf = wasm_func_new(store, htt, host_takesfunc);
        wasm_functype_delete(htt);
        wasm_val_t res[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(res);

        wasm_val_t externarg[1] = { { .kind = WASM_EXTERNREF, .of = { .ref = NULL } } };
        wasm_val_vec_t ev = WASM_ARRAY_VEC(externarg);
        CK_TRAP(wasm_func_call(hf, &ev, &rv));         // externref ⊄ funcref → reject (wrong hierarchy)
        wasm_val_t numarg[1] = { WASM_I32_VAL(5) }; wasm_val_vec_t nv = WASM_ARRAY_VEC(numarg);
        CK_TRAP(wasm_func_call(hf, &nv, &rv));          // number for a ref param → reject
        wasm_val_t funcarg[1] = { { .kind = WASM_FUNCREF, .of = { .ref = NULL } } };
        wasm_val_vec_t fv = WASM_ARRAY_VEC(funcarg);
        CK(wasm_func_call(hf, &fv, &rv) == NULL);       // null funcref ⊑ funcref → accept (control)

        wasm_func_delete(hf);
    }

    // ── (B7) wasm_func_as_ref / wasm_ref_as_func round-trip on a host func keeps it callable ──
    {
        wasm_functype_t* ft = wasm_functype_new_0_1(wasm_valtype_new_i32());
        wasm_func_t* hf = wasm_func_new(store, ft, host_ret99);
        wasm_functype_delete(ft);
        wasm_ref_t* r = wasm_func_as_ref(hf); CK(r != NULL);
        wasm_func_t* hf2 = wasm_ref_as_func(r); CK(hf2 != NULL);
        wasm_val_t res[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(res);
        wasm_val_vec_t noargs = WASM_EMPTY_VEC;
        CK(wasm_func_call(hf2, &noargs, &rv) == NULL && res[0].of.i32 == 99);   // round-tripped host func still calls
        wasm_func_delete(hf2);   // borrowed view — must not free the closure
        wasm_ref_delete(r);
        wasm_func_delete(hf);    // owner — frees the closure
    }

    // ── (6.0 coverage) previously-untested ref API: match_externtype, ref_copy, instance funcref round-trip,
    //    and the as_ref stubs (NULL by spec — globals/tables/memories are not ref values) ──
    {
        // wasm_match_externtype (§7.1.15): func params contravariant / results covariant; global mut invariant.
        wasm_functype_t* fa = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_functype_t* fb = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_functype_t* fc = wasm_functype_new_1_1(wasm_valtype_new(WASM_I64), wasm_valtype_new_i32());
        CK(wasm_match_externtype(store, wasm_functype_as_externtype(fa), wasm_functype_as_externtype(fb)));   // identical → match
        CK(!wasm_match_externtype(store, wasm_functype_as_externtype(fa), wasm_functype_as_externtype(fc)));  // (i32)→ ≠ (i64)→
        wasm_globaltype_t* gk = wasm_globaltype_new(wasm_valtype_new_i32(), WASM_CONST);
        wasm_globaltype_t* gv = wasm_globaltype_new(wasm_valtype_new_i32(), WASM_VAR);
        CK(!wasm_match_externtype(store, wasm_functype_as_externtype(fa), wasm_globaltype_as_externtype(gk)));  // func ≠ global kind
        CK(!wasm_match_externtype(store, wasm_globaltype_as_externtype(gk), wasm_globaltype_as_externtype(gv))); // const ≠ var
        wasm_functype_delete(fa); wasm_functype_delete(fb); wasm_functype_delete(fc);
        wasm_globaltype_delete(gk); wasm_globaltype_delete(gv);

        // as_ref / ref_as stubs: a global/table/memory is not a ref value → NULL by spec.
        CK(wasm_global_as_ref(NULL) == NULL && wasm_ref_as_global(NULL) == NULL);
        CK(wasm_table_as_ref(NULL) == NULL && wasm_ref_as_memory(NULL) == NULL);

        // instance funcref: as_ref / ref_copy (identity) / ref_as_func round-trip + call.
        wasm_byte_vec_t bin; assemble("(module (func (export \"id\") (param i32) (result i32) local.get 0))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t no_imp = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imp, &tr); CK(inst != NULL && tr == NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* idf = wasm_extern_as_func(ex.data[0]);
        wasm_ref_t* r = wasm_func_as_ref(idf); CK(r != NULL);
        wasm_ref_t* rc = wasm_ref_copy(r); CK(rc != NULL && wasm_ref_same(r, rc));    // copy preserves funcref identity
        wasm_func_t* idf2 = wasm_ref_as_func(rc); CK(idf2 != NULL);
        wasm_val_t a[1] = { WASM_I32_VAL(12) }, res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(a), rv = WASM_ARRAY_VEC(res);
        CK(wasm_func_call(idf2, &av, &rv) == NULL && res[0].of.i32 == 12);            // round-tripped instance funcref calls
        wasm_func_delete(idf2); wasm_ref_delete(rc); wasm_ref_delete(r);
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── (A-limits) §2.3.12's optional maximum, and what it costs to encode as a sentinel: grow caps
    //    at the declared max (fail-closed past it), an unbounded limit grows freely, max==0 forbids
    //    ALL growth (the fail-open footgun), and each round-trips through wasm_memory_type /
    //    wasm_table_type as the same KIND of limit it was built from. Guards §3.2.15/§3.2.16 against
    //    the 0/0xFFFFFFFF/65536 sentinel overloading the corpus can't distinguish — pure-wasm never
    //    re-reads a host limit, so only a C-API pin can see it. ──
    {
        wasm_limits_t l2 = { 1, 2, false };                               // memory, declared max 2 pages
        wasm_memorytype_t* mt = wasm_memorytype_new(WASM_I32, &l2);
        wasm_memory_t* m = wasm_memory_new(store, mt); wasm_memorytype_delete(mt);
        CK(wasm_memory_grow(m, 1) == true && wasm_memory_size(m) == 2);   // 1 → 2 ok
        CK(wasm_memory_grow(m, 1) == false && wasm_memory_size(m) == 2);  // past max → fail-closed
        wasm_memorytype_t* rt = wasm_memory_type(m);
        CK(wasm_memorytype_limits(rt)->max == 2 && !wasm_memorytype_limits(rt)->unbounded);
        CK(wasm_memorytype_addrtype(rt) == WASM_I32);
        wasm_memorytype_delete(rt); wasm_memory_delete(m);

        wasm_limits_t l0 = { 0, 0, false };                               // memory, max 0 — the fail-open footgun
        wasm_memorytype_t* mt0 = wasm_memorytype_new(WASM_I32, &l0);
        wasm_memory_t* m0 = wasm_memory_new(store, mt0); wasm_memorytype_delete(mt0);
        CK(wasm_memory_grow(m0, 1) == false);                            // max 0 ⇒ NO growth (not "unlimited")
        wasm_memory_delete(m0);

        wasm_limits_t lu = { 1, 0, true };                               // memory, no max
        wasm_memorytype_t* mtu = wasm_memorytype_new(WASM_I32, &lu);
        wasm_memory_t* mu = wasm_memory_new(store, mtu); wasm_memorytype_delete(mtu);
        CK(wasm_memory_grow(mu, 5) == true && wasm_memory_size(mu) == 6);     // unbounded grow
        wasm_memorytype_t* rtu = wasm_memory_type(mu);
        CK(wasm_memorytype_limits(rtu)->unbounded);                          // absence is a FLAG, not a value
        wasm_memorytype_delete(rtu); wasm_memory_delete(mu);

        // The value that used to mean "absent" is an ordinary maximum, and must cap growth like
        // any other. Under a sentinel this memory grew without limit.
        wasm_limits_t lsent = { 1, 0xffffffffull, false };
        wasm_memorytype_t* mts = wasm_memorytype_new(WASM_I32, &lsent);
        wasm_memory_t* ms = wasm_memory_new(store, mts); wasm_memorytype_delete(mts);
        wasm_memorytype_t* rts = wasm_memory_type(ms);
        CK(!wasm_memorytype_limits(rts)->unbounded && wasm_memorytype_limits(rts)->max == 0xffffffffull);
        wasm_memorytype_delete(rts); wasm_memory_delete(ms);

        wasm_limits_t t3 = { 1, 3, false };                               // table, declared max 3
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &t3);
        wasm_table_t* tb = wasm_table_new(store, tt, NULL); wasm_tabletype_delete(tt);
        CK(wasm_table_grow(tb, 2, NULL) == true && wasm_table_size(tb) == 3);   // 1 → 3 ok
        CK(wasm_table_grow(tb, 1, NULL) == false && wasm_table_size(tb) == 3);  // past max → fail-closed
        wasm_table_delete(tb);

        wasm_limits_t tu = { 0, 0, true };                               // table, no max
        wasm_tabletype_t* ttu = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &tu);
        wasm_table_t* tbu = wasm_table_new(store, ttu, NULL); wasm_tabletype_delete(ttu);
        CK(wasm_table_grow(tbu, 4, NULL) == true && wasm_table_size(tbu) == 4);   // unbounded grow
        wasm_tabletype_t* rtt = wasm_table_type(tbu);
        CK(wasm_tabletype_limits(rtt)->unbounded && wasm_tabletype_addrtype(rtt) == WASM_I32);
        wasm_tabletype_delete(rtt); wasm_table_delete(tbu);

        // A host-created table64: the addrtype is part of the type (§2.3.16) and must survive the
        // instance, or an embedder cannot tell what width of index the table accepts.
        wasm_limits_t t64 = { 1, 4, false };
        wasm_tabletype_t* tt64 = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I64, &t64);
        wasm_table_t* tb64 = wasm_table_new(store, tt64, NULL); wasm_tabletype_delete(tt64);
        wasm_tabletype_t* rt64 = wasm_table_type(tb64);
        CK(wasm_tabletype_addrtype(rt64) == WASM_I64);
        wasm_tabletype_delete(rt64); wasm_table_delete(tb64);

        // ...and a page count no host can back is REFUSED, not narrowed into a small memory.
        wasm_limits_t lhuge = { 4294967296ull, 0, true };
        wasm_memorytype_t* mth = wasm_memorytype_new(WASM_I64, &lhuge);
        CK(wasm_memory_new(store, mth) == NULL);
        wasm_memorytype_delete(mth);

        // The table twin of the two memory refusals above. §3.2.16 admits up to 2^32-1 slots
        // (2^64-1 for table64), so a valid type — or a guest table.grow toward that ceiling — asks
        // for tens of GiB one slot at a time, and past 2^31 slots the bbq_vec int cap wraps into a
        // heap overrun. Both paths refuse past JAV_MAX_TABLE_ELEMS instead of narrowing or overrunning.
        // RED before the fix: wasm_table_new returned a handle (having tried the whole push loop) and
        // wasm_table_grow returned true — this pin then reads == NULL / == false as a clean failure.
        wasm_limits_t thuge = { (1ull << 24) + 1, 0, true };             // min just past the engine bound
        wasm_tabletype_t* tth = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &thuge);
        CK(wasm_table_new(store, tth, NULL) == NULL);
        wasm_tabletype_delete(tth);

        wasm_limits_t tgrow = { 0, 0, true };                           // unbounded table
        wasm_tabletype_t* ttg = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &tgrow);
        wasm_table_t* tbg = wasm_table_new(store, ttg, NULL); wasm_tabletype_delete(ttg);
        CK(wasm_table_grow(tbg, (1ull << 24) + 1, NULL) == false && wasm_table_size(tbg) == 0);
        wasm_table_delete(tbg);
    }

    // ── (A1) funcref encoding unification: a funcref the HOST writes into a guest table must dispatch
    //    correctly when GUEST code call_indirect's it. The engine stores a funcinst pointer in the slot;
    //    the old c-api wrote a funcidx there, so the guest read a small integer AS a pointer (wild deref).
    //    Neither direction is reachable by the conformance corpus (it never crosses host→guest via a
    //    host-set table slot), so it is checked here directly. ASAN would fault on the old encoding. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module"
            "  (type $ii (func (param i32) (result i32)))"
            "  (func (export \"dbl\") (param i32) (result i32) local.get 0 local.get 0 i32.add)"
            "  (table (export \"t\") 1 funcref)"
            "  (func (export \"via\") (param i32) (result i32) local.get 0 i32.const 0 call_indirect (type $ii)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t no_imp = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &no_imp, &tr); CK(inst != NULL && tr == NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);   // order: dbl(func), t(table), via(func)
        wasm_func_t*  dbl = wasm_extern_as_func(ex.data[0]);
        wasm_table_t* tab = wasm_extern_as_table(ex.data[1]);
        wasm_func_t*  via = wasm_extern_as_func(ex.data[2]);

        // (i) a GUEST export func, written by the host into the guest table, reached by guest call_indirect.
        wasm_ref_t* dref = wasm_func_as_ref(dbl); CK(dref != NULL);
        CK(wasm_table_set(tab, 0, dref) == true);
        wasm_val_t a[1] = { WASM_I32_VAL(21) }, res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(a), rv = WASM_ARRAY_VEC(res);
        CK(wasm_func_call(via, &av, &rv) == NULL && res[0].of.i32 == 42);   // 21+21 via the host-set slot
        wasm_ref_delete(dref);

        // (ii) a HOST func (wasm_func_new) in the guest table, reached by guest call_indirect: the host
        //    funcinst (inst_ctx==NULL) carries its own sig, so call_indirect's §4.6.2 type check is a
        //    structural match and dispatch lands on the host callback.
        wasm_functype_t* ft = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* hf = wasm_func_new(store, ft, host_dbl); wasm_functype_delete(ft);
        wasm_ref_t* href = wasm_func_as_ref(hf); CK(href != NULL);
        CK(wasm_table_set(tab, 0, href) == true);
        wasm_val_t a2[1] = { WASM_I32_VAL(20) }, res2[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av2 = WASM_ARRAY_VEC(a2), rv2 = WASM_ARRAY_VEC(res2);
        CK(wasm_func_call(via, &av2, &rv2) == NULL && res2[0].of.i32 == 40);   // host_dbl(20) via call_indirect
        wasm_ref_delete(href); wasm_func_delete(hf);

        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── B10: wasm_module_validate shares ONE verdict-setting path with wasm_module_new ──
    // §7.1.5 module_validate must record the §5/§4.5 outcome on the store (the sanctioned readout),
    // not run a divergent path that leaves last_status stale. Three hand-built modules, each pinning
    // a distinct verdict the bool return alone can't distinguish (false covers both invalid+malformed).
    {
        // valid: type ()->() + one empty func.
        static const uint8_t ok[] = { 0x00,0x61,0x73,0x6D,0x01,0x00,0x00,0x00,
                                      0x01,0x04,0x01,0x60,0x00,0x00, 0x03,0x02,0x01,0x00,
                                      0x0A,0x04,0x01,0x02,0x00,0x0B };
        // invalid (decodes, fails §7): result i32 but an empty body → stack-empty-at-end type error.
        static const uint8_t inv[] = { 0x00,0x61,0x73,0x6D,0x01,0x00,0x00,0x00,
                                       0x01,0x05,0x01,0x60,0x00,0x01,0x7F, 0x03,0x02,0x01,0x00,
                                       0x0A,0x04,0x01,0x02,0x00,0x0B };
        // malformed: truncated header (incomplete version word) → §5 decode fails.
        static const uint8_t mal[] = { 0x00,0x61,0x73,0x6D,0x01,0x00,0x00 };
        wasm_byte_vec_t bv;
        bv = (wasm_byte_vec_t){ sizeof ok, (wasm_byte_t*)ok };
        CK(wasm_module_validate(store, &bv) == true);
        CK(jav_capi_last_status(store) == JAV_OK);
        bv = (wasm_byte_vec_t){ sizeof inv, (wasm_byte_t*)inv };
        CK(wasm_module_validate(store, &bv) == false);
        CK(jav_capi_last_status(store) == JAV_INVALID);
        bv = (wasm_byte_vec_t){ sizeof mal, (wasm_byte_t*)mal };
        CK(wasm_module_validate(store, &bv) == false);
        CK(jav_capi_last_status(store) == JAV_MALFORMED);
    }

    // ── 6.0 coverage: §7.2 type-reflection accessors an embedder relies on but nothing tested —
    //    wasm_func_type / _param_arity / _result_arity → functype_params/results; wasm_global_type →
    //    globaltype_content/mutability. A module with a (i32 i64)->(f32) func + a (mut f64) global. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func (export \"f\") (param i32 i64) (result f32) f32.const 0)"
            "        (global (export \"g\") (mut f64) f64.const 0))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* fn = wasm_extern_as_func(ex.data[0]); CK(fn != NULL);
        CK(wasm_func_param_arity(fn) == 2 && wasm_func_result_arity(fn) == 1);
        wasm_functype_t* ft = wasm_func_type(fn); CK(ft != NULL);
        if (ft) {
            const wasm_valtype_vec_t* ps = wasm_functype_params(ft);
            const wasm_valtype_vec_t* rs = wasm_functype_results(ft);
            CK(ps->size == 2 && wasm_valtype_kind(ps->data[0]) == WASM_I32 && wasm_valtype_kind(ps->data[1]) == WASM_I64);
            CK(rs->size == 1 && wasm_valtype_kind(rs->data[0]) == WASM_F32);
            wasm_functype_delete(ft);
        }
        wasm_global_t* g = wasm_extern_as_global(ex.data[1]); CK(g != NULL);
        wasm_globaltype_t* gt = wasm_global_type(g); CK(gt != NULL);
        if (gt) {
            CK(wasm_valtype_kind(wasm_globaltype_content(gt)) == WASM_F64);
            CK(wasm_globaltype_mutability(gt) == WASM_VAR);
            wasm_globaltype_delete(gt);
        }
        // generic extern-type reflection: wasm_extern_type → externtype_kind + the as_X projection
        // (the right projection yields the type; the wrong one is NULL).
        wasm_externtype_t* etf = wasm_extern_type(ex.data[0]); CK(etf != NULL);
        if (etf) {
            CK(wasm_externtype_kind(etf) == WASM_EXTERN_FUNC);
            wasm_functype_t* eft = wasm_externtype_as_functype(etf); CK(eft != NULL);
            if (eft) CK(wasm_functype_params(eft)->size == 2 && wasm_functype_results(eft)->size == 1);
            CK(wasm_externtype_as_globaltype(etf) == NULL);   // wrong projection → NULL
            wasm_externtype_delete(etf);                      // frees the externtype (the as_X view shares it)
        }
        wasm_externtype_t* etg = wasm_extern_type(ex.data[1]); CK(etg != NULL);
        if (etg) {
            CK(wasm_externtype_kind(etg) == WASM_EXTERN_GLOBAL);
            wasm_globaltype_t* egt = wasm_externtype_as_globaltype(etg); CK(egt != NULL);
            if (egt) CK(wasm_valtype_kind(wasm_globaltype_content(egt)) == WASM_F64);
            wasm_externtype_delete(etg);
        }
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── 6.0 coverage: per-handle host-info (an embedder void* attached to a ref) — set/get round-trip. ──
    {
        wasm_functype_t* ft = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* hf = wasm_func_new(store, ft, host_dbl); wasm_functype_delete(ft);
        wasm_ref_t* r = wasm_func_as_ref(hf); CK(r != NULL);
        int marker = 7;
        wasm_ref_set_host_info(r, &marker);
        CK(wasm_ref_get_host_info(r) == &marker);   // the attached void* reads back
        wasm_ref_delete(r); wasm_func_delete(hf);
    }

    // ── 6.0 coverage: wasm_func_new_with_env (§7.1.8) — a host func carrying an environment closure +
    //    finalizer. The callback reads its env; the finalizer runs on func_delete. ──
    {
        int base = 100;
        wasm_functype_t* ft = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
        wasm_func_t* hf = wasm_func_new_with_env(store, ft, host_addenv, &base, env_fin);
        wasm_functype_delete(ft);
        wasm_val_t a[1] = { WASM_I32_VAL(5) }, res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = WASM_ARRAY_VEC(a), rv = WASM_ARRAY_VEC(res);
        CK(wasm_func_call(hf, &av, &rv) == NULL && res[0].of.i32 == 105);   // env base 100 + arg 5
        g_env_fin_ran = 0;
        wasm_func_delete(hf);
        CK(g_env_fin_ran == 1);   // the env finalizer ran on delete
    }

    // ── A3 (the §8 flat-cache collapse): a host callback RE-ENTERS the engine while a guest call is suspended.
    //    A.outer() calls host reenter(), which calls B.inner() (switching the engine to B's context);
    //    then A reads ITS OWN global — must be 111 (A's), not 222 (B's). Pre-§8 the flat instance cache
    //    switched to B and the suspended A call never restored it → outer read B's global (corruption). ──
    {
        wasm_byte_vec_t bb; assemble("(module (global $g (mut i32) (i32.const 222))"
                                     "        (func (export \"inner\") (result i32) global.get $g))", &bb);
        wasm_module_t* mB = wasm_module_new(store, &bb); CK(mB != NULL);
        wasm_extern_vec_t niB = WASM_EMPTY_VEC; wasm_trap_t* trB = NULL;
        wasm_instance_t* iB = wasm_instance_new(store, mB, &niB, &trB); CK(iB != NULL);
        wasm_extern_vec_t exB; wasm_instance_exports(iB, &exB);
        g_inner = wasm_extern_as_func(exB.data[0]);              // B.inner — the re-entrant target

        wasm_functype_t* ft = wasm_functype_new_0_1(wasm_valtype_new_i32());
        wasm_func_t* reenter = wasm_func_new(store, ft, host_reenter); wasm_functype_delete(ft);

        wasm_byte_vec_t ba; assemble("(module (import \"h\" \"reenter\" (func $r (result i32)))"
                                     "        (global $g (mut i32) (i32.const 111))"
                                     "        (func (export \"outer\") (result i32) call $r drop global.get $g))", &ba);
        wasm_module_t* mA = wasm_module_new(store, &ba); CK(mA != NULL);
        wasm_extern_t* imps[1] = { wasm_func_as_extern(reenter) }; wasm_extern_vec_t iv = WASM_ARRAY_VEC(imps);
        wasm_trap_t* trA = NULL;
        wasm_instance_t* iA = wasm_instance_new(store, mA, &iv, &trA); CK(iA != NULL);
        wasm_extern_vec_t exA; wasm_instance_exports(iA, &exA);
        wasm_func_t* outer = wasm_extern_as_func(exA.data[0]);
        wasm_val_vec_t none = WASM_EMPTY_VEC; wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r);
        CK(wasm_func_call(outer, &none, &rv) == NULL && r[0].of.i32 == 111);   // A's context survives the re-entry

        wasm_extern_vec_delete(&exA); wasm_instance_delete(iA); wasm_module_delete(mA);
        wasm_func_delete(reenter); wasm_byte_vec_delete(&ba);
        wasm_extern_vec_delete(&exB); wasm_instance_delete(iB); wasm_module_delete(mB); wasm_byte_vec_delete(&bb);
        g_inner = NULL;
    }


    // ── store as GC-root authority over the shared heap: a managed struct held ONLY by instance A's
    //    global (no C handle on it) must survive a collection driven through a DIFFERENT instance B.
    //    If A is not a scanned root, A's struct is the only reference and gets reclaimed during B's
    //    churn — its memory reused by a churn struct with a different field, so A.rd() reads the wrong
    //    value (deterministic, no reliance on ASAN inside the GC arena). A is tracked in store->insts
    //    at §4.7.2 step 24 by the capi_track_inst hook. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module"
            "  (type $s (struct (field i32)))"
            "  (global $g (mut (ref null $s)) (ref.null $s))"
            "  (func (export \"mk\") (param i32) (global.set $g (struct.new $s (local.get 0))))"
            "  (func (export \"rd\") (result i32) (struct.get $s 0 (global.get $g))))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin);
        CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* iA = wasm_instance_new(store, mod, &ni, &tr); CK(iA != NULL);   // two instances,
        wasm_instance_t* iB = wasm_instance_new(store, mod, &ni, &tr); CK(iB != NULL);   //   one shared store/heap
        wasm_extern_vec_t exA, exB; wasm_instance_exports(iA, &exA); wasm_instance_exports(iB, &exB);
        wasm_func_t* mkA = wasm_extern_as_func(exA.data[0]);
        wasm_func_t* rdA = wasm_extern_as_func(exA.data[1]);
        wasm_func_t* mkB = wasm_extern_as_func(exB.data[0]);
        wasm_val_vec_t no_res2 = WASM_EMPTY_VEC, no_arg = WASM_EMPTY_VEC;

        wasm_val_t mka[1] = { WASM_I32_VAL(0x5AFE) };           // A's struct: field = 0x5AFE, held only by A's global
        wasm_val_vec_t mkav = WASM_ARRAY_VEC(mka);
        CK(wasm_func_call(mkA, &mkav, &no_res2) == NULL);

        for (int k = 0; k < 20000; k++) {                       // churn: B mints+drops structs with a DIFFERENT field
            wasm_val_t ta[1] = { WASM_I32_VAL(0x1234) };
            wasm_val_vec_t tav = WASM_ARRAY_VEC(ta);
            wasm_func_call(mkB, &tav, &no_res2);
        }
        wasm_val_t rr[1] = { WASM_INIT_VAL }; wasm_val_vec_t rrv = WASM_ARRAY_VEC(rr);
        CK(wasm_func_call(rdA, &no_arg, &rrv) == NULL);
        CK(rr[0].kind == WASM_I32 && rr[0].of.i32 == 0x5AFE);   // A's struct survived: field intact, not reclaimed/reused

        wasm_extern_vec_delete(&exA); wasm_extern_vec_delete(&exB);
        wasm_instance_delete(iA); wasm_instance_delete(iB);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── §3.3.10/§4.5.2 cross-MODULE ref.test/ref.cast on a managed struct ref: module A mints a
    //    struct; module B — a DISTINCT module whose $s is the SAME closed type — receives it (imported
    //    func) and ref.tests/ref.casts it to the concrete $s. The object's rtt lives in A's module, not
    //    B's, so B must resolve its runtime type by the store-global closed id (as it already does for
    //    cross-module funcrefs), else the concrete cast wrongly fails/traps. RED before the gc_rtt.gid
    //    fix: ref.test returns 0 and ref.cast traps. ──
    {
        wasm_byte_vec_t ba; assemble(
            "(module (type $s (struct (field i32)))"
            "  (func (export \"mk\") (result (ref $s)) (struct.new $s (i32.const 42))))", &ba);
        wasm_module_t* mA = wasm_module_new(store, &ba); CK(mA != NULL);
        wasm_extern_vec_t niA = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* iA = wasm_instance_new(store, mA, &niA, &tr); CK(iA != NULL);
        wasm_extern_vec_t exA; wasm_instance_exports(iA, &exA);          // exA[0] = mk

        wasm_byte_vec_t bb; assemble(
            "(module (type $s (struct (field i32)))"
            "  (func $mk (import \"a\" \"mk\") (result (ref $s)))"
            "  (func (export \"test\") (result i32) (ref.test (ref $s) (call $mk)))"
            "  (func (export \"cast\") (result i32) (struct.get $s 0 (ref.cast (ref $s) (call $mk)))))", &bb);
        wasm_module_t* mB = wasm_module_new(store, &bb); CK(mB != NULL);
        wasm_extern_t* impB[1] = { exA.data[0] }; wasm_extern_vec_t niB = WASM_ARRAY_VEC(impB);   // A's mk = B's import 0
        wasm_instance_t* iB = wasm_instance_new(store, mB, &niB, &tr); CK(iB != NULL);
        wasm_extern_vec_t exB; wasm_instance_exports(iB, &exB);          // exB[0]=test, exB[1]=cast

        wasm_val_vec_t no_arg = WASM_EMPTY_VEC;
        wasm_val_t rt[1] = { WASM_INIT_VAL }; wasm_val_vec_t rtv = WASM_ARRAY_VEC(rt);
        CK(wasm_func_call(wasm_extern_as_func(exB.data[0]), &no_arg, &rtv) == NULL);
        CK(rt[0].kind == WASM_I32 && rt[0].of.i32 == 1);                 // ref.test: A's struct IS a $s cross-module
        wasm_val_t rc[1] = { WASM_INIT_VAL }; wasm_val_vec_t rcv = WASM_ARRAY_VEC(rc);
        CK(wasm_func_call(wasm_extern_as_func(exB.data[1]), &no_arg, &rcv) == NULL);   // ref.cast must NOT trap
        CK(rc[0].kind == WASM_I32 && rc[0].of.i32 == 42);               // the downcast reads the field

        wasm_extern_vec_delete(&exA); wasm_extern_vec_delete(&exB);
        wasm_instance_delete(iA); wasm_instance_delete(iB);
        wasm_module_delete(mA); wasm_module_delete(mB);
        wasm_byte_vec_delete(&ba); wasm_byte_vec_delete(&bb);
    }

    // ── trap-frame bytecode offsets (wasm.h §7.1.8 frame contract): a frame reports the trapping
    //    instruction's byte offset — func-relative and module-relative — NOT a hardcoded 0. ──
    {
        // body of $f (no locals): i32.const 7 = 41 07 @0; drop = 1a @2; unreachable = 00 @3
        wasm_byte_vec_t bin; assemble(
            "(module (func (export \"f\") i32.const 7 drop unreachable))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* f = wasm_extern_as_func(ex.data[0]);
        wasm_val_vec_t na = WASM_EMPTY_VEC, nr = WASM_EMPTY_VEC;
        wasm_trap_t* trap = wasm_func_call(f, &na, &nr);
        CK(trap != NULL);
        if (trap) {
            wasm_frame_t* fr = wasm_trap_origin(trap);
            CK(fr != NULL);
            if (fr) {
                CK(wasm_frame_func_offset(fr) == 3);                                   // the unreachable, func-relative
                CK(wasm_frame_module_offset(fr) > wasm_frame_func_offset(fr));         // module-relative includes the func's position
                wasm_frame_delete(fr);
            }
            wasm_trap_delete(trap);
        }
        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── §3.3.3 debug-extension probe: jav_capi_set_probe hands the embedder the interp op stream
    //    (a sanctioned sidecar extension, not in wasm.h). Opt-in; clears on cb=NULL. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (func (export \"f\") (result i32) i32.const 3 i32.const 4 i32.add))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &ni, &tr); CK(inst != NULL);
        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* f = wasm_extern_as_func(ex.data[0]);
        wasm_val_t rr[1] = { WASM_INIT_VAL }; wasm_val_vec_t na = WASM_EMPTY_VEC, rrv = WASM_ARRAY_VEC(rr);

        g_probe_ops_n = 0;
        jav_capi_set_probe(store, probe_record, NULL);
        CK(wasm_func_call(f, &na, &rrv) == NULL && rr[0].kind == WASM_I32 && rr[0].of.i32 == 7);
        jav_capi_set_probe(store, NULL, NULL);                       // clear
        CK(g_probe_ops_n >= 3 && g_probe_ops[0] == 0x41 && g_probe_ops[1] == 0x41 && g_probe_ops[2] == 0x6a);
        int before = g_probe_ops_n;
        CK(wasm_func_call(f, &na, &rrv) == NULL);                    // after clear: the probe fires no more
        CK(g_probe_ops_n == before);

        wasm_extern_vec_delete(&ex); wasm_instance_delete(inst);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── 6.0 coverage-floor closers: public wasm.h fns that had ZERO direct test (the remaining
    //    untested public symbols are upstream wasm.h inline/macro conveniences, not jav code) ──
    {
        // config_new + engine_new_with_config, and THE DEFAULT TIER. Asking for nothing gets
        // tier 2 (wasm_capi.c, JAV_DEFAULT_TIER) — the engine's claim about itself should not
        // be something an embedder has to discover. Pinned by behaviour rather than by reading
        // a field back: a default store must actually place functions on the compiled tier.
        wasm_engine_t* e2 = wasm_engine_new_with_config(wasm_config_new());
        CK(e2 != NULL);
        wasm_store_t* s2 = wasm_store_new(e2);
        {
            wasm_byte_vec_t db;
            assemble("(module (func (export \"d\") (param i32) (result i32)"
                     "  local.get 0 i32.const 3 i32.mul))", &db);
            wasm_module_t* dm = wasm_module_new(s2, &db); CK(dm != NULL);
            wasm_extern_vec_t dni = WASM_EMPTY_VEC; wasm_trap_t* dtr = NULL;
            wasm_instance_t* di = wasm_instance_new(s2, dm, &dni, &dtr); CK(di != NULL);
            wasm_extern_vec_t dex; wasm_instance_exports(di, &dex);
            wasm_val_t da[1] = { WASM_I32_VAL(14) }, dr[1] = { WASM_INIT_VAL };
            wasm_val_vec_t dav = WASM_ARRAY_VEC(da), drv = WASM_ARRAY_VEC(dr);
            CK(wasm_func_call(wasm_extern_as_func(dex.data[0]), &dav, &drv) == NULL && dr[0].of.i32 == 42);
            CK(jav_capi_jit_count(s2) > 0);   // a default engine COMPILES; it does not interpret
            wasm_extern_vec_delete(&dex); wasm_instance_delete(di);
            wasm_module_delete(dm); wasm_byte_vec_delete(&db);
        }

        // val_copy: a num copies by value; valkind_is_ref classifies kinds
        CK(wasm_valkind_is_ref(WASM_EXTERNREF) && !wasm_valkind_is_ref(WASM_I32));
        wasm_val_t v0 = WASM_I32_VAL(7), v1;
        wasm_val_copy(&v1, &v0);
        CK(v1.kind == WASM_I32 && v1.of.i32 == 7);

        // tabletype_element reads back the element valtype
        wasm_limits_t lim = { 1, 10, false };
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), WASM_I32, &lim);
        CK(wasm_valtype_kind(wasm_tabletype_element(tt)) == WASM_FUNCREF);
        wasm_tabletype_delete(tt);

        // import/exporttype_new + module/name accessors (each owns the externtype it's handed)
        wasm_name_t mnm, nnm, enm;
        wasm_name_new_from_string(&mnm, "env");
        wasm_name_new_from_string(&nnm, "f");
        wasm_importtype_t* it = wasm_importtype_new(&mnm, &nnm,
            wasm_functype_as_externtype(wasm_functype_new_0_0()));
        CK(it != NULL);
        CK(wasm_importtype_module(it)->size == 3 && memcmp(wasm_importtype_module(it)->data, "env", 3) == 0);
        CK(wasm_importtype_name(it)->size == 1 && wasm_importtype_name(it)->data[0] == 'f');
        wasm_importtype_delete(it);
        wasm_name_new_from_string(&enm, "g");
        wasm_exporttype_t* et = wasm_exporttype_new(&enm,
            wasm_functype_as_externtype(wasm_functype_new_0_0()));
        CK(et != NULL);
        wasm_exporttype_delete(et);

        // tag + tag_type + tagtype_functype + tag_copy: a tag of type (i32)->()
        wasm_tagtype_t* tgt = wasm_tagtype_new(wasm_functype_new_1_0(wasm_valtype_new(WASM_I32)));
        CK(wasm_functype_params(wasm_tagtype_functype(tgt))->size == 1);
        wasm_tag_t* tag = wasm_tag_new(s2, tgt);
        CK(tag != NULL);
        wasm_tagtype_t* tgt2 = wasm_tag_type(tag);                       // recovers the tag's type
        CK(tgt2 && wasm_functype_params(wasm_tagtype_functype(tgt2))->size == 1);
        wasm_tagtype_delete(tgt2);
        wasm_tag_t* tagc = wasm_tag_copy(tag);
        CK(tagc != NULL);
        wasm_tag_delete(tagc);
        wasm_tag_delete(tag);
        wasm_tagtype_delete(tgt);

        // trap_message: a host-made trap round-trips its message
        wasm_message_t mm; wasm_name_new_from_string(&mm, "boom");
        wasm_trap_t* tr = wasm_trap_new(s2, &mm);
        wasm_name_delete(&mm);
        wasm_message_t mo; wasm_trap_message(tr, &mo);
        CK(mo.size >= 4 && memcmp(mo.data, "boom", 4) == 0);
        wasm_name_delete(&mo);
        wasm_trap_delete(tr);

        wasm_store_delete(s2);
        wasm_engine_delete(e2);
    }

    // ── §7.1.7 instance_export(moduleinst, name) : externaddr | error ──────────────────
    //
    //   1. Assert: due to validity of the module instance, all its export names are different.
    //   2. If there exists an exportinst whose name equals name, return its addr.
    //   3. Else, return error.
    //
    // Absent until now, so every embedder hand-rolled it: driver/javelina.c and test/exec.h
    // each build a bbq_dict over the zipped export vectors, and the plan for the Inkscape
    // backend documented the zip as a property of the C API rather than as our omission.
    // Step 1's assertion is what makes a by-name lookup total — export names are distinct by
    // module-instance validity, so first match is the only match.
    {
        wasm_byte_vec_t bin;
        assemble("(module (func (export \"alpha\") (result i32) i32.const 11)"
                 "        (func (export \"beta\")  (result i32) i32.const 22)"
                 "        (global (export \"g\") i32 (i32.const 33)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);
        wasm_extern_vec_t ni = WASM_EMPTY_VEC; wasm_trap_t* tr = NULL;
        wasm_instance_t* i1 = wasm_instance_new(store, mod, &ni, &tr); CK(i1 != NULL);

        // The by-name answer must denote the same externaddr the zip yields at that index — the
        // two cannot drift, which is the whole reason the hand-rolled indices existed.
        wasm_exporttype_vec_t et; wasm_module_exports(mod, &et);
        wasm_extern_vec_t    ex; wasm_instance_exports(i1, &ex);
        int agreed = 1, checked = 0;
        for (size_t i = 0; i < et.size && i < ex.size; i++) {
            const wasm_name_t* nm = wasm_exporttype_name(et.data[i]);
            wasm_extern_t* byname = wasm_instance_export(i1, nm);
            if (!byname || wasm_extern_kind(byname) != wasm_extern_kind(ex.data[i])) agreed = 0;
            wasm_extern_delete(byname);
            checked++;
        }
        CK(checked == 3 && agreed);   // §7.1.7 by-name agrees with the zip on every export

        // §7.1.7 step 2a returns "the external address exportinst_i.addr" — the ADDRESS is the
        // identity, so two handles obtained for one export must compare same, and a handle must
        // compare same with its own copy. Comparing wrappers instead answered no to both, which
        // is the shape of bug an embedder trips over first and cannot work around.
        {
            wasm_name_t na; wasm_name_new_from_string(&na, "alpha");
            wasm_extern_t* x1 = wasm_instance_export(i1, &na);
            wasm_extern_t* x2 = wasm_instance_export(i1, &na);
            wasm_extern_t* xc = wasm_extern_copy(x1);
            CK(x1 != x2 && wasm_extern_same(x1, x2));   // distinct handles, one externaddr
            CK(xc != x1 && wasm_extern_same(x1, xc));   // a copy denotes what it copied
            CK(!wasm_extern_same(x1, ex.data[1]));      // and a DIFFERENT export is not the same
            // the typed views agree with the extern they came from
            CK(wasm_func_same(wasm_extern_as_func(x1), wasm_extern_as_func(x2)));
            wasm_extern_delete(xc); wasm_extern_delete(x2); wasm_extern_delete(x1);
            wasm_name_delete(&na);
        }

        // ...and it resolves each name to the RIGHT export, which a kind check alone would not
        // show: three exports, two of them funcs of the same type.
        {
            wasm_name_t na, nb; wasm_val_vec_t none = WASM_EMPTY_VEC;
            wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r);
            wasm_name_new_from_string(&na, "alpha"); wasm_name_new_from_string(&nb, "beta");
            wasm_extern_t* ea = wasm_instance_export(i1, &na);
            wasm_extern_t* eb = wasm_instance_export(i1, &nb);
            CK(wasm_func_call(wasm_extern_as_func(ea), &none, &rv) == NULL && r[0].of.i32 == 11);
            CK(wasm_func_call(wasm_extern_as_func(eb), &none, &rv) == NULL && r[0].of.i32 == 22);
            wasm_extern_delete(ea); wasm_extern_delete(eb);
            wasm_name_delete(&na); wasm_name_delete(&nb);
        }

        // Step 3: a name no export carries is an error, not a near miss.
        wasm_name_t absent; wasm_name_new_from_string(&absent, "gamma");
        CK(wasm_instance_export(i1, &absent) == NULL);
        wasm_name_delete(&absent);

        // The handle stays good after the caller drops the vector it could also have found the
        // export in — the externaddr belongs to the instance, not to that vector.
        wasm_name_t alpha; wasm_name_new_from_string(&alpha, "alpha");
        wasm_extern_vec_delete(&ex);
        wasm_extern_t* a = wasm_instance_export(i1, &alpha);
        CK(a != NULL);
        {   // and it is callable, so "valid" means valid
            wasm_val_vec_t none = WASM_EMPTY_VEC;
            wasm_val_t r[1] = { WASM_INIT_VAL }; wasm_val_vec_t rv = WASM_ARRAY_VEC(r);
            CK(wasm_func_call(wasm_extern_as_func(a), &none, &rv) == NULL && r[0].of.i32 == 11);
        }

        // Two instances of one module: each name resolves within ITS OWN instance. A lookup
        // keyed on the module rather than the instance would collapse these.
        wasm_instance_t* i2 = wasm_instance_new(store, mod, &ni, &tr); CK(i2 != NULL);
        wasm_extern_t* a2 = wasm_instance_export(i2, &alpha);
        CK(a2 != NULL && a2 != a && !wasm_extern_same(a2, a));   // different instance ⇒ different addr
        wasm_extern_delete(a2); wasm_extern_delete(a);
        wasm_name_delete(&alpha);

        wasm_exporttype_vec_delete(&et);
        wasm_instance_delete(i2); wasm_instance_delete(i1);
        wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    // ── §7.1.10 mem_read / mem_write — bounds-checked by construction ──────────────────
    // "If i is larger than or equal to the length of mi.bytes, then return error." The raw
    // wasm_memory_data pointer cannot express that, which is why every embedder was left to
    // reinvent the span check and why one of them shipped without it.
    {
        wasm_limits_t lim = { 1, 0, true };
        wasm_memorytype_t* mt = wasm_memorytype_new(WASM_I32, &lim);
        wasm_memory_t* mem = wasm_memory_new(store, mt); wasm_memorytype_delete(mt);
        CK(mem != NULL);
        size_t n = wasm_memory_data_size(mem);

        byte_t got = 0;
        CK(wasm_memory_write(mem, 0, (byte_t)0x5A));
        CK(wasm_memory_read(mem, 0, &got) && (unsigned char)got == 0x5A);
        CK(wasm_memory_write(mem, n - 1, (byte_t)0x3C));
        CK(wasm_memory_read(mem, n - 1, &got) && (unsigned char)got == 0x3C);

        CK(!wasm_memory_read(mem, n, &got));            // i == length → error, step 2
        CK(!wasm_memory_write(mem, n, (byte_t)1));
        CK(!wasm_memory_read(mem, (uint64_t)-1, &got)); // and a wild index is refused, not wrapped
        CK(!wasm_memory_write(mem, (uint64_t)-1, (byte_t)1));
        wasm_memory_delete(mem);
    }

    // ── §2.3.12 limits: [ u64 .. u64? ] ────────────────────────────────────────────────
    // A memtype (§2.3.15 `addrtype limits page`) and a tabletype (§2.3.16 `addrtype limits
    // reftype`) carry u64 bounds. §3.2.15 bounds a memory at k = 2^(|addrtype|−16), so a 64-bit
    // addrtype admits up to 2^48 pages; §3.2.16 bounds a table at 2^|addrtype| − 1. A u32
    // rendering of the bounds cannot carry either, and truncation is silent — the embedder reads
    // a plausible small number instead of an error. Read through the module's own import types,
    // which is the path an embedder walks to decide what to hand instantiation.
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"bigmem\" (memory i64 4294967296 4294967297))"
            "        (import \"env\" \"bigtab\" (table i64 4294967296 4294967297 funcref)))", &bin);
        wasm_module_t* m64 = wasm_module_new(store, &bin); CK(m64 != NULL);
        wasm_importtype_vec_t it; wasm_module_imports(m64, &it);
        CK(it.size == 2);

        const wasm_memorytype_t* mt = wasm_externtype_as_memorytype_const(wasm_importtype_type(it.data[0]));
        CK(mt != NULL);
        const wasm_limits_t* ml = wasm_memorytype_limits(mt);
        CK(ml->min == 4294967296ull);            // 2^32 pages: 0 once truncated to u32
        CK(ml->max == 4294967297ull && !ml->unbounded);
        CK(wasm_memorytype_addrtype(mt) == WASM_I64);

        const wasm_tabletype_t* tt = wasm_externtype_as_tabletype_const(wasm_importtype_type(it.data[1]));
        CK(tt != NULL);
        const wasm_limits_t* tl = wasm_tabletype_limits(tt);
        CK(tl->min == 4294967296ull);
        CK(tl->max == 4294967297ull && !tl->unbounded);
        CK(wasm_tabletype_addrtype(tt) == WASM_I64);

        wasm_importtype_vec_delete(&it);
        wasm_module_delete(m64); wasm_byte_vec_delete(&bin);

        // ...and the i32 case is not merely the absence of the i64 case: a plain memory reports
        // WASM_I32 and an absent maximum, which a memory64 with a declared max must not.
        wasm_byte_vec_t b32; assemble(
            "(module (import \"env\" \"m\" (memory 1))"
            "        (import \"env\" \"t\" (table 1 funcref)))", &b32);
        wasm_module_t* m32 = wasm_module_new(store, &b32); CK(m32 != NULL);
        wasm_importtype_vec_t it32; wasm_module_imports(m32, &it32);
        CK(it32.size == 2);
        const wasm_memorytype_t* mt32 = wasm_externtype_as_memorytype_const(wasm_importtype_type(it32.data[0]));
        CK(wasm_memorytype_addrtype(mt32) == WASM_I32 && wasm_memorytype_limits(mt32)->unbounded);
        const wasm_tabletype_t* tt32 = wasm_externtype_as_tabletype_const(wasm_importtype_type(it32.data[1]));
        CK(wasm_tabletype_addrtype(tt32) == WASM_I32 && wasm_tabletype_limits(tt32)->unbounded);
        wasm_importtype_vec_delete(&it32);
        wasm_module_delete(m32); wasm_byte_vec_delete(&b32);

        // A module DEFINING a memory no host can back: the type is valid (§3.2.15 admits 2^48
        // pages for a 64-bit addrtype), so it decodes — and instantiation must fail rather than
        // narrow the page count, which produced a 0-page memory and reported success.
        wasm_byte_vec_t bdef; assemble("(module (memory i64 4294967296))", &bdef);
        wasm_module_t* md = wasm_module_new(store, &bdef); CK(md != NULL);
        wasm_extern_vec_t noimp = WASM_EMPTY_VEC; wasm_trap_t* trd = NULL;
        wasm_instance_t* bad = wasm_instance_new(store, md, &noimp, &trd);
        CK(bad == NULL);
        if (bad) wasm_instance_delete(bad);
        if (trd) wasm_trap_delete(trd);
        wasm_module_delete(md); wasm_byte_vec_delete(&bdef);
    }

    // ── §7.1.14 val_default ────────────────────────────────────────────────────────────
    // "If default_valtype is not defined, then return error. Else, return the value."
    {
        wasm_val_t v;
        wasm_valtype_t* t;
        t = wasm_valtype_new(WASM_I32); CK(wasm_val_default(t, &v) && v.kind == WASM_I32 && v.of.i32 == 0); wasm_valtype_delete(t);
        t = wasm_valtype_new(WASM_I64); CK(wasm_val_default(t, &v) && v.kind == WASM_I64 && v.of.i64 == 0); wasm_valtype_delete(t);
        t = wasm_valtype_new(WASM_F32); CK(wasm_val_default(t, &v) && v.kind == WASM_F32 && v.of.f32 == 0.f); wasm_valtype_delete(t);
        t = wasm_valtype_new(WASM_F64); CK(wasm_val_default(t, &v) && v.kind == WASM_F64 && v.of.f64 == 0.0); wasm_valtype_delete(t);
        t = wasm_valtype_new(WASM_EXTERNREF);                       // §4.2.1 nullable ref → null
        CK(wasm_val_default(t, &v) && wasm_valkind_is_ref(v.kind) && v.of.ref == NULL); wasm_valtype_delete(t);
        CK(!wasm_val_default(NULL, &v));
    }

    // ── §7.1.9 table_read / §7.1.13 global_write — the two dropped error outcomes ──────
    {
        // table_read : ref | error. A table holds null references legitimately, so the NULL
        // One answer cannot mean both "the null reference" and "out of range".
        wasm_limits_t tl = { 2, 2, false };
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), WASM_I32, &tl);
        wasm_table_t* tab = wasm_table_new(store, tt, NULL); wasm_tabletype_delete(tt);
        CK(tab != NULL);
        wasm_ref_t* r = (wasm_ref_t*)(uintptr_t)1;
        CK(wasm_table_read(tab, 0, &r) && r == NULL);   // in range, and the entry IS the null ref
        CK(!wasm_table_read(tab, 2, &r));               // out of range → error, distinguishable
        CK(!wasm_table_read(tab, (wasm_table_size_t)-1, &r));
        wasm_table_delete(tab);

        // global_write : store | error — "If mut is empty, then return error."
        wasm_val_t init = WASM_I32_VAL(7), rd;
        wasm_globaltype_t* gi = wasm_globaltype_new(wasm_valtype_new(WASM_I32), WASM_CONST);
        wasm_global_t* gc_ = wasm_global_new(store, gi, &init); wasm_globaltype_delete(gi);
        wasm_val_t nv = WASM_I32_VAL(9);
        CK(gc_ != NULL && !wasm_global_set(gc_, &nv));  // immutable → refused, and SAID so
        wasm_global_get(gc_, &rd); CK(rd.of.i32 == 7);  // and unchanged
        wasm_global_delete(gc_);

        wasm_globaltype_t* gm = wasm_globaltype_new(wasm_valtype_new(WASM_I32), WASM_VAR);
        wasm_global_t* gv = wasm_global_new(store, gm, &init); wasm_globaltype_delete(gm);
        CK(gv != NULL && wasm_global_set(gv, &nv));     // mutable → accepted
        wasm_global_get(gv, &rd); CK(rd.of.i32 == 9);
        wasm_global_delete(gv);

        // The same rule on a MODULE's own globals, which reach a different branch than the
        // host-created ones above — an immutable global exported by an instance must refuse a
        // write just as loudly.
        wasm_byte_vec_t gb;
        assemble("(module (global (export \"k\") i32 (i32.const 5))"
                 "        (global (export \"m\") (mut i32) (i32.const 6)))", &gb);
        wasm_module_t* gmod = wasm_module_new(store, &gb); CK(gmod != NULL);
        wasm_extern_vec_t gni = WASM_EMPTY_VEC; wasm_trap_t* gtr = NULL;
        wasm_instance_t* gin = wasm_instance_new(store, gmod, &gni, &gtr); CK(gin != NULL);
        wasm_name_t nk, nm2;
        wasm_name_new_from_string(&nk, "k"); wasm_name_new_from_string(&nm2, "m");
        wasm_extern_t* ek = wasm_instance_export(gin, &nk);
        wasm_extern_t* em = wasm_instance_export(gin, &nm2);
        CK(ek && em);
        CK(!wasm_global_set(wasm_extern_as_global(ek), &nv));   // immutable module global → error
        wasm_global_get(wasm_extern_as_global(ek), &rd); CK(rd.of.i32 == 5);
        CK(wasm_global_set(wasm_extern_as_global(em), &nv));    // mutable module global → accepted
        wasm_global_get(wasm_extern_as_global(em), &rd); CK(rd.of.i32 == 9);
        wasm_extern_delete(ek); wasm_extern_delete(em);
        wasm_name_delete(&nk); wasm_name_delete(&nm2);
        wasm_instance_delete(gin); wasm_module_delete(gmod); wasm_byte_vec_delete(&gb);
    }

    // ── §4.2.4 tables are STORE objects: a table shared across the host↔guest boundary has ONE
    //    storage identity, so a grow that reallocs it is seen by every holder and no holder is left
    //    with a stale pointer. A host table imported into a module and grown BY THE GUEST past its
    //    capacity forces several bbq_vec reallocs; the host handle must then report the grown size
    //    and read the new storage. Before tables were store objects each importer COPIED the raw
    //    refs pointer, so the guest's realloc dangled the host's (a UAF ASAN catches, plus a wrong
    //    size) and store teardown double-freed. table_grow.wast never triggers it — it grows a
    //    shared table by 1, inside bbq_vec's doubling slack — so only this pin covers the realloc. ──
    {
        wasm_byte_vec_t bin; assemble(
            "(module (import \"env\" \"t\" (table $t 1 funcref))"
            "        (func (export \"grow\") (param i32) (result i32) (table.grow $t (ref.null func) (local.get 0)))"
            "        (func (export \"sz\") (result i32) (table.size $t)))", &bin);
        wasm_module_t* mod = wasm_module_new(store, &bin); CK(mod != NULL);

        wasm_limits_t tl = { 1, 0, true };                 // host table: min 1, unbounded → grow reallocs
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new_funcref(), WASM_I32, &tl);
        wasm_table_t* htab = wasm_table_new(store, tt, NULL); wasm_tabletype_delete(tt);
        CK(htab != NULL && wasm_table_size(htab) == 1);

        wasm_extern_t* imports[1] = { wasm_table_as_extern(htab) };
        wasm_extern_vec_t iv = WASM_ARRAY_VEC(imports); wasm_trap_t* tr = NULL;
        wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &tr); CK(inst != NULL && tr == NULL);

        wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
        wasm_func_t* grow = wasm_extern_as_func(ex.data[0]);   // export order: grow, then sz
        wasm_func_t* szf  = wasm_extern_as_func(ex.data[1]);
        wasm_val_t ga[1] = { WASM_I32_VAL(50) }, gr[1] = { WASM_INIT_VAL };
        wasm_val_vec_t gav = WASM_ARRAY_VEC(ga), grv = WASM_ARRAY_VEC(gr);
        CK(wasm_func_call(grow, &gav, &grv) == NULL && gr[0].of.i32 == 1);   // returns the OLD size (1)

        CK(wasm_table_size(htab) == 51);                   // the host handle sees the ONE store table's new size
        wasm_ref_t* got = (wasm_ref_t*)1;
        CK(wasm_table_read(htab, 50, &got) && got == NULL); // reads the NEW storage (stale pointer → UAF)
        CK(!wasm_table_read(htab, 51, &got));               // past the grown end

        wasm_val_t sr[1] = { WASM_INIT_VAL }; wasm_val_vec_t none2 = WASM_EMPTY_VEC, srv = WASM_ARRAY_VEC(sr);
        CK(wasm_func_call(szf, &none2, &srv) == NULL && sr[0].of.i32 == 51);   // guest agrees (same store table)

        wasm_extern_vec_delete(&ex);
        wasm_instance_delete(inst);                        // delete the importer — the store table survives it
        CK(wasm_table_size(htab) == 51);                   // still valid after the instance is gone
        wasm_table_delete(htab); wasm_module_delete(mod); wasm_byte_vec_delete(&bin);
    }

    wasm_store_delete(store);
    wasm_engine_delete(engine);
    printf("wasm-c-api (... + host objects + serialize + host-info + trap trace + tags + exceptions + ref_type/match): %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
