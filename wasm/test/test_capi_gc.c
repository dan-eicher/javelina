// test_capi_gc.c — the c-api store's GC roots under REAL collections, via the public wasm.h
// surface only. capi_extra_roots scans host globals/tables, C-held managed refs (s->gc_refs),
// rooted host exceptions, and every instance — and none of it had ever run under a collection
// (test_capi never collects). Collections here are NATURAL: `churn` allocates 4096 dropped
// structs per call, so allocation pressure drives the collector with the embedder's refs live.
//
// The held value is an externref-wrapped struct (§2.3.4: the extern/any hierarchies are
// isomorphic; javelina converts by identity), so the embedder's handle is a live GC aggregate —
// the exact shape the store's root scan exists for.
//
// FALSIFIED: with the s->gc_refs loop in capi_extra_roots disabled, the read-back rows go red
// (wrong value / engine trap) while the module's own rows stay green; restored, all green.
#include "wasm.h"
#include "wat_driver.h"
#include "jav_writer.h"
#include "bbq_runtime.h"
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

static int call1(wasm_func_t* f, const wasm_val_t* arg, wasm_val_t* res) {
    wasm_val_vec_t av = { arg ? 1u : 0u, (wasm_val_t*)arg };   /* empty vecs, never NULL */
    wasm_val_vec_t rv = { res ? 1u : 0u, res };
    wasm_trap_t* t = wasm_func_call(f, &av, &rv);
    if (t) { wasm_trap_delete(t); return 0; }
    return 1;
}

int main(void) {
    wasm_engine_t* engine = wasm_engine_new();
    wasm_store_t*  store  = wasm_store_new(engine);

    wasm_byte_vec_t bin; assemble(
        "(module (type $leaf (struct (field i32)))"
        "  (func (export \"make\") (param i32) (result externref)"
        "    (extern.convert_any (struct.new $leaf (local.get 0))))"
        "  (func (export \"read\") (param externref) (result i32)"
        "    (struct.get $leaf 0 (ref.cast (ref $leaf) (any.convert_extern (local.get 0)))))"
        "  (func (export \"churn\")"
        "    (local $i i32)"
        "    (block $done (loop $more"
        "      (br_if $done (i32.ge_s (local.get $i) (i32.const 4096)))"
        "      (drop (struct.new $leaf (i32.const 0x5EED)))"
        "      (local.set $i (i32.add (local.get $i) (i32.const 1)))"
        "      (br $more)))))", &bin);
    wasm_module_t* mod = wasm_module_new(store, &bin);
    wasm_byte_vec_delete(&bin);
    CK(mod != NULL);

    wasm_extern_vec_t iv = WASM_EMPTY_VEC;
    wasm_trap_t* trap = NULL;
    wasm_instance_t* inst = wasm_instance_new(store, mod, &iv, &trap);
    CK(inst != NULL && trap == NULL);
    wasm_extern_vec_t ex; wasm_instance_exports(inst, &ex);
    wasm_func_t* make  = wasm_extern_as_func(ex.data[0]);
    wasm_func_t* read  = wasm_extern_as_func(ex.data[1]);
    wasm_func_t* churn = wasm_extern_as_func(ex.data[2]);
    CK(make && read && churn);

    // hold TWO embedder refs across heavy allocation pressure
    wasm_val_t a42 = WASM_I32_VAL(42), a7 = WASM_I32_VAL(7);
    wasm_val_t h1 = WASM_INIT_VAL, h2 = WASM_INIT_VAL, r = WASM_INIT_VAL;
    CK(call1(make, &a42, &h1) && h1.kind == WASM_EXTERNREF && h1.of.ref);
    CK(call1(make, &a7,  &h2) && h2.kind == WASM_EXTERNREF && h2.of.ref);

    for (int i = 0; i < 32; i++) CK(call1(churn, NULL, NULL));   // ~128K allocations → collections

    CK(call1(read, &h1, &r) && r.of.i32 == 42);   // s->gc_refs kept + relocated the held aggregate
    CK(call1(read, &h2, &r) && r.of.i32 == 7);

    // release one handle (unroots it), churn again, the OTHER must still be intact
    wasm_val_delete(&h1);
    for (int i = 0; i < 32; i++) CK(call1(churn, NULL, NULL));
    CK(call1(read, &h2, &r) && r.of.i32 == 7);

    wasm_val_delete(&h2);
    wasm_extern_vec_delete(&ex);
    wasm_instance_delete(inst); wasm_module_delete(mod);
    wasm_store_delete(store); wasm_engine_delete(engine);
    printf("c-api store roots under collection: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
