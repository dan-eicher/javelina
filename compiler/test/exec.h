/* exec.h — execute a compiled .wasm module image in the javelina VM, through the
 * PUBLIC wasm.h c-api ONLY (no jav_* internals). The VM is the trusted oracle
 * (it runs the conformance suite); these helpers assemble→load→call→assert so
 * every implemented family is verified by EXECUTION, not just byte shape.
 *
 * The host-native floor (the →HOST contract: fd I/O, File stat, bit intrinsics,
 * clock/exit/random) is the SHARED driver/host_io.h — the same natives the shipped
 * runner (driver/javelina.c) supplies. exec.h layers the test-harness policy on top:
 * fds 0/1/2 are capture temp files so a test can read back what was written.
 *
 * Boundary rule: this file includes only <wasm.h> (via host_io.h). It links against
 * the VM's wasm_capi.c + engine objects, but reaches into none of them. */
#ifndef JAVELINA_COMPILER_TEST_EXEC_H
#define JAVELINA_COMPILER_TEST_EXEC_H

#include "host_io.h"       /* the embedder host-native floor (includes <wasm.h>) */
#include <stdbool.h>

/* ── The harness's OWN natives. These are not part of the →HOST contract: they exist so a test can
 * exercise the compiler's native-call forwarders (a static direct call, a virtual vtable slot, and a
 * plain imported int native) against a host function with a known, trivial semantics — the identity.
 * They are registered by NAME through the embedder-extension hook, so every OTHER unnamed native
 * still hits the shipped floor's fail-closed stub and a missing library impl cannot hide here. ── */
static wasm_trap_t* harness_identity(const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    if (results->size > 0) {
        if (args->size > 0) wasm_val_copy(&results->data[0], &args->data[0]);
        else                results->data[0] = (wasm_val_t)WASM_I32_VAL(0);
    }
    return NULL;
}
static wasm_func_t* harness_host_for(wasm_store_t* store, const wasm_functype_t* ft, const wasm_name_t* nm) {
    static const char* const echoes[] = { "identity", "self", "ext", NULL };
    for (int i = 0; echoes[i]; i++)
        if (nm->size == strlen(echoes[i]) && !memcmp(nm->data, echoes[i], nm->size))
            return wasm_func_new(store, ft, harness_identity);
    return NULL;   /* everything else: the shipped floor decides (and traps if unimplemented) */
}

/* The property set the harness publishes: the fifteen §20.18.7 keys with FIXED values, so a test can
 * assert on them, plus two crafted entries that exercise Integer/Long.getInteger/getLong's radix rules. */
static const hio_prop_t harness_props[] = {
    { "java.version", "1.0" },        { "java.vendor", "javelina" },
    { "java.vendor.url", "https://example.invalid/javelina" },
    { "java.home", "/javelina" },     { "java.class.version", "45.3" },
    { "java.class.path", "." },       { "os.name", "javelina" },
    { "os.arch", "wasm32" },          { "os.version", "1.0" },
    { "file.separator", "/" },        { "path.separator", ":" },
    { "line.separator", "\n" },       { "user.name", "javelina" },
    { "user.home", "/" },             { "user.dir", "." },
    { "test.dec", "1234" },           { "test.hex", "0x2a" },
    { "test.hash", "#2a" },           { "test.oct", "052" },
    { "test.bad", "not-a-number" },   { "test.true", "TrUe" },
    { NULL, NULL },
};

/* The c-api's sanctioned store-scoped error readout (wasm_capi.c): the precise
 * §5/§4.5/§7.6 reason a module was rejected or failed to instantiate. Declared
 * here (these are not part of wasm.h, but ARE the conformance error-readout seam)
 * so the harness REPORTS why a module was rejected — no per-bug instrumentation. */
extern int         jav_capi_last_error(const wasm_store_t* store);
extern const char* jav_err_str(int err);
extern void        jav_config_set_jit(wasm_config_t* c, int jit);   /* the engine tier choice */

/* Tier toggle for exec_call: 0 = interpreter (the corpus default), 1 = JIT.
 * The v128 parity probes run each module under BOTH and diff lane-exactly. */
static int g_exec_jit = 0;

/* Outcome of a call: did the module validate / instantiate, and did the call
 * complete without trapping. `results` is filled on a clean (non-trapping) call. */
typedef enum { EXEC_OK, EXEC_INVALID, EXEC_NO_INSTANCE, EXEC_NO_EXPORT, EXEC_TRAP } exec_status;

/* Validate `mod[0..modlen)`, supply a host function for every import (library ctors +
 * natives are the extern API — via exec_host_for), find the export named `name`, call
 * it with `args[0..nargs)`, and write up to `nres` results. Self-contained: creates and
 * tears down its own engine/store/module/instance per call. */
static exec_status exec_call(const uint8_t* mod, size_t modlen, const char* name,
                             const wasm_val_t* args, size_t nargs,
                             wasm_val_t* results, size_t nres) {
    g_io_host_extra = harness_host_for;
    g_io_props      = harness_props;
    wasm_byte_vec_t bin;
    wasm_byte_vec_new_uninitialized(&bin, modlen);
    memcpy(bin.data, mod, modlen);

    wasm_engine_t* engine;
    if (g_exec_jit) {                       /* the V-probe tier toggle: run the SAME
                                             * module under the JIT for parity diffing */
        wasm_config_t* c = wasm_config_new();
        jav_config_set_jit(c, 1);
        engine = wasm_engine_new_with_config(c);
    } else {
        engine = wasm_engine_new();
    }
    wasm_store_t*  store  = wasm_store_new(engine);

    exec_status st = EXEC_OK;
    wasm_module_t* module = NULL;
    wasm_instance_t* instance = NULL;
    wasm_extern_vec_t exports = WASM_EMPTY_VEC;
    wasm_exporttype_vec_t exptypes = WASM_EMPTY_VEC;
    wasm_importtype_vec_t imptypes = WASM_EMPTY_VEC;
    wasm_extern_vec_t imports = WASM_EMPTY_VEC;

    if (!wasm_module_validate(store, &bin)) {
        fprintf(stderr, "  [exec:%s] module rejected: %s\n", name, jav_err_str(jav_capi_last_error(store)));
        st = EXEC_INVALID; goto done; }
    module = wasm_module_new(store, &bin);
    if (!module) { st = EXEC_INVALID; goto done; }

    wasm_module_imports(module, &imptypes);
    wasm_extern_vec_new_uninitialized(&imports, imptypes.size);
    for (size_t i = 0; i < imptypes.size; i++) {
        const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(imptypes.data[i]));
        imports.data[i] = ft ? wasm_func_as_extern(
            exec_host_for(store, ft, wasm_importtype_name(imptypes.data[i]))) : NULL;
    }

    wasm_trap_t* trap = NULL;
    instance = wasm_instance_new(store, module, &imports, &trap);
    if (!instance) {
        fprintf(stderr, "  [exec:%s] instantiation failed: %s\n", name, jav_err_str(jav_capi_last_error(store)));
        if (trap) wasm_trap_delete(trap); st = EXEC_NO_INSTANCE; goto done; }

    wasm_module_exports(module, &exptypes);
    wasm_instance_exports(instance, &exports);
    int idx = -1;
    for (size_t i = 0; i < exptypes.size && i < exports.size; i++) {
        const wasm_name_t* en = wasm_exporttype_name(exptypes.data[i]);
        if (en->size == strlen(name) && !memcmp(en->data, name, en->size)) { idx = (int)i; break; }
    }
    if (idx < 0) { st = EXEC_NO_EXPORT; goto done; }

    wasm_func_t* func = wasm_extern_as_func(exports.data[idx]);
    if (!func) { st = EXEC_NO_EXPORT; goto done; }

    wasm_val_vec_t args_vec   = { nargs, (wasm_val_t*)args };
    wasm_val_vec_t result_vec = { nres,  results };
    trap = wasm_func_call(func, &args_vec, &result_vec);
    if (trap) {
        wasm_message_t msg = WASM_EMPTY_VEC; wasm_trap_message(trap, &msg);
        fprintf(stderr, "  [exec:%s] trap: %.*s\n", name, (int)msg.size, msg.data ? msg.data : "");
        if (msg.size) wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap); st = EXEC_TRAP; goto done; }

done:
    for (size_t i = 0; i < imports.size; i++)
        if (imports.data[i]) wasm_func_delete(wasm_extern_as_func(imports.data[i]));
    if (imports.data) { free(imports.data); imports.data = NULL; imports.size = 0; }
    if (imptypes.size) wasm_importtype_vec_delete(&imptypes);
    if (exports.size)  wasm_extern_vec_delete(&exports);
    if (exptypes.size) wasm_exporttype_vec_delete(&exptypes);
    if (instance) wasm_instance_delete(instance);
    if (module)   wasm_module_delete(module);
    wasm_store_delete(store);
    wasm_engine_delete(engine);
    wasm_byte_vec_delete(&bin);
    return st;
}

/* §3.3.3 opcode probe: record the last executed op so a trap names its trapping instruction. */
extern void jav_capi_set_probe(wasm_store_t*, void (*)(void*, uint8_t), void*);
static uint8_t g_last_op;
static void jav_probe_cb(void* c, uint8_t op) { (void)c; g_last_op = op; }

/* The ONE long-lived jre.wasm runtime, instantiated on a shared store/heap that outlives every
 * plugin (Stage D). Built once by exec_jre_init; every exec_call links a fresh plugin against it. */
static struct {
    bool inited;
    wasm_engine_t* engine;
    wasm_store_t*  store;
    wasm_module_t* mod;
    wasm_instance_t* inst;
    wasm_byte_vec_t bin;
    wasm_importtype_vec_t impt;   wasm_extern_vec_t imp;    /* jre's host imports (kept to free at teardown) */
    wasm_exporttype_vec_t expt;   wasm_extern_vec_t exp;    /* jre's exports (the plugin link target) */
} g_jre;

/* Build jre ONCE on a fresh shared store and capture its exports. Returns false on failure. */
static bool exec_jre_init(const uint8_t* jre, size_t jrelen) {
    if (g_jre.inited) return true;
    g_io_host_extra = harness_host_for;
    g_io_props      = harness_props;
    g_jre.engine = wasm_engine_new();
    g_jre.store  = wasm_store_new(g_jre.engine);
    jav_capi_set_probe(g_jre.store, jav_probe_cb, NULL);   /* record the last op for trap diagnosis */
    wasm_byte_vec_new_uninitialized(&g_jre.bin, jrelen); memcpy(g_jre.bin.data, jre, jrelen);
    if (!wasm_module_validate(g_jre.store, &g_jre.bin)) {
        fprintf(stderr, "  [jre] rejected: %s\n", jav_err_str(jav_capi_last_error(g_jre.store))); return false; }
    g_jre.mod = wasm_module_new(g_jre.store, &g_jre.bin); if (!g_jre.mod) return false;
    wasm_module_imports(g_jre.mod, &g_jre.impt);
    wasm_extern_vec_new_uninitialized(&g_jre.imp, g_jre.impt.size);
    for (size_t i = 0; i < g_jre.impt.size; i++) {
        const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(g_jre.impt.data[i]));
        g_jre.imp.data[i] = ft ? wasm_func_as_extern(exec_host_for(g_jre.store, ft, wasm_importtype_name(g_jre.impt.data[i]))) : NULL;
    }
    wasm_trap_t* trap = NULL;
    g_jre.inst = wasm_instance_new(g_jre.store, g_jre.mod, &g_jre.imp, &trap);
    if (!g_jre.inst) { fprintf(stderr, "  [jre] instantiation failed: %s\n", jav_err_str(jav_capi_last_error(g_jre.store)));
                       if (trap) wasm_trap_delete(trap); return false; }
    wasm_module_exports(g_jre.mod, &g_jre.expt); wasm_instance_exports(g_jre.inst, &g_jre.exp);
    for (size_t i = 0; i < g_jre.expt.size && i < g_jre.exp.size; i++) {   /* the ONE shared I/O staging memory */
        const wasm_name_t* en = wasm_exporttype_name(g_jre.expt.data[i]);
        if (en->size == 6 && !memcmp(en->data, "memory", 6)) { g_io_mem = wasm_extern_as_memory(g_jre.exp.data[i]); break; }
    }
    /* Preopen the three standard fds (System.in/out/err = fd 0/1/2). Here they're capture temp files so
     * tests can read back what was written; the runner maps them to real stdin/stdout/stderr. */
    if (!g_io_fds[0]) g_io_fds[0] = tmpfile();
    if (!g_io_fds[1]) g_io_fds[1] = tmpfile();
    if (!g_io_fds[2]) g_io_fds[2] = tmpfile();
    g_jre.inited = true;
    return true;
}

static void exec_jre_teardown(void) {
    if (!g_jre.inited) return;
    for (size_t i = 0; i < g_jre.imp.size; i++) if (g_jre.imp.data[i]) wasm_func_delete(wasm_extern_as_func(g_jre.imp.data[i]));
    if (g_jre.imp.data) free(g_jre.imp.data);
    if (g_jre.impt.size) wasm_importtype_vec_delete(&g_jre.impt);
    if (g_jre.exp.size)  wasm_extern_vec_delete(&g_jre.exp);
    if (g_jre.expt.size) wasm_exporttype_vec_delete(&g_jre.expt);
    if (g_jre.inst) wasm_instance_delete(g_jre.inst);
    if (g_jre.mod)  wasm_module_delete(g_jre.mod);
    wasm_byte_vec_delete(&g_jre.bin);
    wasm_store_delete(g_jre.store);
    wasm_engine_delete(g_jre.engine);
    memset(&g_jre, 0, sizeof g_jre);
}

/* Link a PLUGIN module against the shared long-lived jre and call `name`: java.lang imports
 * (module "jre") resolve to jre's exports by name (structural GC-type match via the shared-heap
 * gcanon); genuine host imports resolve via exec_host_for. The plugin instance/module are freed
 * per call; jre + the store persist. Requires exec_jre_init to have succeeded. */
static exec_status exec_call_shared(const uint8_t* mod, size_t modlen, const char* name,
                             const wasm_val_t* args, size_t nargs,
                             wasm_val_t* results, size_t nres) {
    if (!g_jre.inited) { fprintf(stderr, "  [plugin:%s] jre not initialized\n", name); return EXEC_NO_INSTANCE; }
    wasm_store_t* store = g_jre.store;
    exec_status st = EXEC_OK;
    wasm_byte_vec_t pbin; wasm_byte_vec_new_uninitialized(&pbin, modlen); memcpy(pbin.data, mod, modlen);
    wasm_module_t* pmod = NULL;
    wasm_instance_t* pinst = NULL;
    wasm_importtype_vec_t pimpt = WASM_EMPTY_VEC;
    wasm_extern_vec_t pimp = WASM_EMPTY_VEC;
    wasm_exporttype_vec_t pexpt = WASM_EMPTY_VEC;
    wasm_extern_vec_t pexp = WASM_EMPTY_VEC;
    wasm_trap_t* trap = NULL;

    if (!wasm_module_validate(store, &pbin)) {
        fprintf(stderr, "  [plugin:%s] rejected: %s\n", name, jav_err_str(jav_capi_last_error(store))); st = EXEC_INVALID; goto done; }
    pmod = wasm_module_new(store, &pbin); if (!pmod) { st = EXEC_INVALID; goto done; }
    wasm_module_imports(pmod, &pimpt);
    wasm_extern_vec_new_uninitialized(&pimp, pimpt.size);
    for (size_t i = 0; i < pimpt.size; i++) {
        const wasm_name_t* im = wasm_importtype_module(pimpt.data[i]);
        const wasm_name_t* fl = wasm_importtype_name(pimpt.data[i]);
        if (im->size == 3 && !memcmp(im->data, "jre", 3)) {
            int fi = -1;
            for (size_t j = 0; j < g_jre.expt.size && j < g_jre.exp.size; j++) {
                const wasm_name_t* en = wasm_exporttype_name(g_jre.expt.data[j]);
                if (en->size == fl->size && !memcmp(en->data, fl->data, en->size)) { fi = (int)j; break; }
            }
            if (fi < 0) { fprintf(stderr, "  [plugin:%s] unresolved jre import %.*s\n", name, (int)fl->size, fl->data);
                          st = EXEC_NO_INSTANCE; goto done; }
            pimp.data[i] = g_jre.exp.data[fi];            /* borrowed — owned by g_jre.exp */
        } else {
            const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(pimpt.data[i]));
            pimp.data[i] = ft ? wasm_func_as_extern(exec_host_for(store, ft, fl)) : NULL;
        }
    }
    pinst = wasm_instance_new(store, pmod, &pimp, &trap);
    if (!pinst) { fprintf(stderr, "  [plugin:%s] instantiation failed: %s", name, jav_err_str(jav_capi_last_error(store)));
                  if (trap) { wasm_message_t tm = WASM_EMPTY_VEC; wasm_trap_message(trap, &tm);
                              fprintf(stderr, " | start trap: %.*s", (int)tm.size, tm.data ? tm.data : "");
                              if (tm.size) wasm_byte_vec_delete(&tm); wasm_trap_delete(trap); trap = NULL; }
                  fprintf(stderr, "\n"); st = EXEC_NO_INSTANCE; goto done; }
    wasm_module_exports(pmod, &pexpt); wasm_instance_exports(pinst, &pexp);
    int idx = -1;
    for (size_t i = 0; i < pexpt.size && i < pexp.size; i++) {
        const wasm_name_t* en = wasm_exporttype_name(pexpt.data[i]);
        if (en->size == strlen(name) && !memcmp(en->data, name, en->size)) { idx = (int)i; break; }
    }
    if (idx < 0) { st = EXEC_NO_EXPORT; goto done; }
    wasm_func_t* func = wasm_extern_as_func(pexp.data[idx]);
    if (!func) { st = EXEC_NO_EXPORT; goto done; }
    { wasm_val_vec_t av = { nargs, (wasm_val_t*)args }, rv = { nres, results };
      trap = wasm_func_call(func, &av, &rv);
      if (trap) { wasm_message_t msg = WASM_EMPTY_VEC; wasm_trap_message(trap, &msg);
                  fprintf(stderr, "  [plugin:%s] trap: %.*s last_op=0x%02x", name, (int)msg.size, msg.data ? msg.data : "", g_last_op);
                  wasm_frame_vec_t tr = WASM_EMPTY_VEC; wasm_trap_trace(trap, &tr);
                  for (size_t k = 0; k < tr.size; k++)
                      fprintf(stderr, " | #%zu func=%u pc=%zu", k, wasm_frame_func_index(tr.data[k]), wasm_frame_func_offset(tr.data[k]));
                  if (tr.size) wasm_frame_vec_delete(&tr);
                  fprintf(stderr, "\n");
                  if (msg.size) wasm_byte_vec_delete(&msg); wasm_trap_delete(trap); trap = NULL; st = EXEC_TRAP; goto done; } }

done:
    for (size_t i = 0; i < pimp.size && i < pimpt.size; i++) {
        const wasm_name_t* im = wasm_importtype_module(pimpt.data[i]);
        if (pimp.data[i] && !(im->size == 3 && !memcmp(im->data, "jre", 3)))
            wasm_func_delete(wasm_extern_as_func(pimp.data[i]));
    }
    if (pimp.data)  free(pimp.data);
    if (pimpt.size) wasm_importtype_vec_delete(&pimpt);
    if (pexp.size)  wasm_extern_vec_delete(&pexp);
    if (pexpt.size) wasm_exporttype_vec_delete(&pexpt);
    if (pinst) wasm_instance_delete(pinst);
    if (pmod)  wasm_module_delete(pmod);
    wasm_byte_vec_delete(&pbin);
    return st;
}

#endif /* JAVELINA_COMPILER_TEST_EXEC_H */
