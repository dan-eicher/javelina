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
    jav_module_t* mod = wat_assemble(wat, (int)strlen(wat), INSTRS_TOML, &el, &ec);
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
    wasm_engine_t* engine = wasm_engine_new();
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
        wasm_ref_t* g0 = wasm_table_get(t, 0); CK(g0 != NULL);
        CK(wasm_table_get(t, 1) == NULL);                          // null slot
        CK(wasm_table_set(t, 1, g0) == true);                      // copy the funcref into slot 1
        wasm_ref_t* g1 = wasm_table_get(t, 1); CK(g1 != NULL && wasm_ref_same(g0, g1));
        CK(wasm_table_get(t, 9) == NULL);                          // OOB
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
            wasm_ref_t* c0 = wasm_table_get(tab, 1);
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
            wasm_ref_t* g4 = wasm_table_get(tab, 4);
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
            wasm_ref_t* survive = wasm_table_get(tab, 4);
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

        wasm_limits_t mlim = { 1, 4 };
        wasm_memorytype_t* mt = wasm_memorytype_new(&mlim);
        wasm_memory_t* hm = wasm_memory_new(store, mt);
        wasm_memorytype_delete(mt);
        CK(wasm_memory_size(hm) == 1 && wasm_memory_data(hm) != NULL);
        CK(wasm_memory_grow(hm, 1) == true && wasm_memory_size(hm) == 2);

        wasm_limits_t tlim = { 2, wasm_limits_max_default };
        wasm_tabletype_t* tt2 = wasm_tabletype_new(wasm_valtype_new_funcref(), &tlim);
        wasm_table_t* ht = wasm_table_new(store, tt2, NULL);
        wasm_tabletype_delete(tt2);
        CK(wasm_table_size(ht) == 2 && wasm_table_get(ht, 0) == NULL);

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

    // ── (A-limits) explicit has_max: grow caps at the declared max (fail-closed past it), an unbounded
    //    limit grows freely, max==0 forbids ALL growth (the old fail-open footgun), and the limit
    //    round-trips through wasm_memory_type/wasm_table_type WITHOUT a sentinel (no max ⇒ the public
    //    wasm_limits_max_default, never a ceiling value). Guards §3.2.15/§3.2.16 vs the 0/0xFFFFFFFF/65536
    //    sentinel overloading the corpus can't distinguish (pure-wasm never re-reads a host limit). ──
    {
        wasm_limits_t l2 = { 1, 2 };                                      // memory, declared max 2 pages
        wasm_memorytype_t* mt = wasm_memorytype_new(&l2);
        wasm_memory_t* m = wasm_memory_new(store, mt); wasm_memorytype_delete(mt);
        CK(wasm_memory_grow(m, 1) == true && wasm_memory_size(m) == 2);   // 1 → 2 ok
        CK(wasm_memory_grow(m, 1) == false && wasm_memory_size(m) == 2);  // past max → fail-closed
        wasm_memorytype_t* rt = wasm_memory_type(m);
        CK(wasm_memorytype_limits(rt)->max == 2);                         // declared max round-trips
        wasm_memorytype_delete(rt); wasm_memory_delete(m);

        wasm_limits_t l0 = { 0, 0 };                                      // memory, max 0 — the fail-open footgun
        wasm_memorytype_t* mt0 = wasm_memorytype_new(&l0);
        wasm_memory_t* m0 = wasm_memory_new(store, mt0); wasm_memorytype_delete(mt0);
        CK(wasm_memory_grow(m0, 1) == false);                            // max 0 ⇒ NO growth (not "unlimited")
        wasm_memory_delete(m0);

        wasm_limits_t lu = { 1, wasm_limits_max_default };               // memory, no max
        wasm_memorytype_t* mtu = wasm_memorytype_new(&lu);
        wasm_memory_t* mu = wasm_memory_new(store, mtu); wasm_memorytype_delete(mtu);
        CK(wasm_memory_grow(mu, 5) == true && wasm_memory_size(mu) == 6);     // unbounded grow
        wasm_memorytype_t* rtu = wasm_memory_type(mu);
        CK(wasm_memorytype_limits(rtu)->max == wasm_limits_max_default);      // no max ⇒ default, NOT a ceiling value
        wasm_memorytype_delete(rtu); wasm_memory_delete(mu);

        wasm_limits_t t3 = { 1, 3 };                                      // table, declared max 3
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new_funcref(), &t3);
        wasm_table_t* tb = wasm_table_new(store, tt, NULL); wasm_tabletype_delete(tt);
        CK(wasm_table_grow(tb, 2, NULL) == true && wasm_table_size(tb) == 3);   // 1 → 3 ok
        CK(wasm_table_grow(tb, 1, NULL) == false && wasm_table_size(tb) == 3);  // past max → fail-closed
        wasm_table_delete(tb);

        wasm_limits_t tu = { 0, wasm_limits_max_default };               // table, no max
        wasm_tabletype_t* ttu = wasm_tabletype_new(wasm_valtype_new_funcref(), &tu);
        wasm_table_t* tbu = wasm_table_new(store, ttu, NULL); wasm_tabletype_delete(ttu);
        CK(wasm_table_grow(tbu, 4, NULL) == true && wasm_table_size(tbu) == 4);   // unbounded grow
        wasm_tabletype_t* rtt = wasm_table_type(tbu);
        CK(wasm_tabletype_limits(rtt)->max == wasm_limits_max_default);
        wasm_tabletype_delete(rtt); wasm_table_delete(tbu);
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
        // config_new (spec: NULL — no standard fields) + engine_new_with_config (ignores it)
        wasm_engine_t* e2 = wasm_engine_new_with_config(wasm_config_new());
        CK(e2 != NULL);
        wasm_store_t* s2 = wasm_store_new(e2);

        // val_copy: a num copies by value; valkind_is_ref classifies kinds
        CK(wasm_valkind_is_ref(WASM_EXTERNREF) && !wasm_valkind_is_ref(WASM_I32));
        wasm_val_t v0 = WASM_I32_VAL(7), v1;
        wasm_val_copy(&v1, &v0);
        CK(v1.kind == WASM_I32 && v1.of.i32 == 7);

        // tabletype_element reads back the element valtype
        wasm_limits_t lim = { 1, 10 };
        wasm_tabletype_t* tt = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), &lim);
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

    wasm_store_delete(store);
    wasm_engine_delete(engine);
    printf("wasm-c-api (... + host objects + serialize + host-info + trap trace + tags + exceptions + ref_type/match): %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
