/* test_host_abi.c — the →HOST contract's RESOLUTION rule (docs/host-abi.md).
 *
 * A WebAssembly import is a TWO-part name (§5.5.5: module ++ field), and jre.wasm
 * declares both: HostIO.open, System.exit, Object.finalize, F32x4.ceil. The
 * embedder floor resolved on the field half alone, which made the module name
 * decoration and produced three defects this suite pins:
 *
 *   1. ANY module could claim ANY host function. `Whatever.open` reached
 *      HostIO's real filesystem open, because nothing compared "HostIO".
 *
 *   2. An application native registered through the documented g_io_host_extra
 *      hook was SHADOWED by a built-in of the same field name — silently, since
 *      the hook is consulted last. An app exposing its own `open`/`read`/`gc`
 *      never got called and never learned why. The hook could not even work
 *      around it: it was handed the field name only, so it had no way to tell
 *      its own namespace from java.lang's.
 *
 *   3. The DECLARED functype is what wasm_func_new is handed, so a mismatch
 *      LINKS. `wasi_snapshot_preview1.fd_write` — (fd, iovs, iovs_len,
 *      nwritten) → errno — bound onto HostIO's three-argument
 *      (fd, off, len) → i32 and read an iovec pointer as a buffer offset.
 *      Wrong answers at call time instead of an error at link time.
 *
 * Unknown names must still resolve to the FAIL-CLOSED trapping stub rather than
 * to a link error: sema emits an import for every `native` declaration, so
 * jre.wasm imports the whole Mem/V128/I8x16../Math intrinsic surface that the
 * compiler lowers to instructions and never calls. Those must keep binding to a
 * stub that traps if ever entered (host-abi.md, "Imports that are not host
 * calls"). Case 5 pins that.
 */
#include "host_io.h"        /* the embedder host-native floor (includes <wasm.h>) */
#include "javelina_test.h"
#include <string.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

static wasm_functype_t* ft_of(const char* sig) {
    /* "params:results", one char per value: i=i32 I=i64 f=f32 F=f64 r=externref */
    wasm_valtype_t* pv[8]; wasm_valtype_t* rv[8];
    size_t np = 0, nr = 0; int after = 0;
    for (const char* c = sig; *c; c++) {
        if (*c == ':') { after = 1; continue; }
        wasm_valtype_t* t =
            *c == 'i' ? wasm_valtype_new(WASM_I32) : *c == 'I' ? wasm_valtype_new(WASM_I64) :
            *c == 'f' ? wasm_valtype_new(WASM_F32) : *c == 'F' ? wasm_valtype_new(WASM_F64) :
                        wasm_valtype_new(WASM_EXTERNREF);
        if (after) rv[nr++] = t; else pv[np++] = t;
    }
    wasm_valtype_vec_t p, r;
    wasm_valtype_vec_new(&p, np, pv);
    wasm_valtype_vec_new(&r, nr, rv);
    return wasm_functype_new(&p, &r);
}

/* Resolve (module, field) at the declared signature, call it, and report only
 * whether it TRAPPED — which is how the fail-closed stub is told from a real
 * binding without depending on any particular native's return value. */
static int traps(wasm_store_t* st, const char* mod, const char* fld, const char* sig,
                 wasm_val_t* argv, size_t argc, wasm_val_t* out) {
    wasm_name_t m, f;
    wasm_name_new_from_string(&m, mod);
    wasm_name_new_from_string(&f, fld);
    wasm_functype_t* ft = ft_of(sig);
    wasm_func_t* fn = exec_host_for(st, ft, &m, &f);
    wasm_val_t rbuf[4]; memset(rbuf, 0, sizeof rbuf);
    wasm_val_vec_t args = { argc, argv }, res = { out ? 1u : 0u, rbuf };
    wasm_trap_t* t = wasm_func_call(fn, &args, &res);
    if (out) *out = rbuf[0];
    int trapped = t != NULL;
    if (t) wasm_trap_delete(t);
    wasm_functype_delete(ft);
    wasm_name_delete(&m); wasm_name_delete(&f);
    return trapped;
}

/* ── the application-native hook, as an embedder would write it ──────────── */

static int g_hook_calls;
static wasm_name_t g_hook_last_mod;

static wasm_trap_t* app_gc(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)a; r->data[0].kind = WASM_I32; r->data[0].of.i32 = 4242; return NULL;
}

/* Answers ONLY for module "App" — which is the whole point: a hook that cannot
 * see the module name cannot scope itself, and must either grab every field
 * name it recognises or none. */
static wasm_func_t* app_host_for(wasm_store_t* st, const wasm_functype_t* ft,
                                 const wasm_name_t* mod, const wasm_name_t* fld) {
    g_hook_calls++;
    if (g_hook_last_mod.data) wasm_name_delete(&g_hook_last_mod);
    wasm_name_new(&g_hook_last_mod, mod->size, mod->data);
    if (mod->size == 3 && !memcmp(mod->data, "App", 3) &&
        fld->size == 2 && !memcmp(fld->data, "gc", 2))
        return wasm_func_new(st, ft, app_gc);
    return NULL;
}

int main(void) {
    wasm_engine_t* eng = wasm_engine_new();
    wasm_store_t*  st  = wasm_store_new(eng);

    /* 1. The contract still works: a qualified name the floor names binds. */
    CHECK(!traps(st, "System", "gc", ":", NULL, 0, NULL),
          "System.gc resolves to the floor's no-op");

    /* 2. The module name is load-bearing. A different module asking for the
     *    same field is NOT the same import, and must fail closed. */
    CHECK(traps(st, "Whatever", "gc", ":", NULL, 0, NULL),
          "Whatever.gc does NOT reach System.gc (module name is checked)");
    {
        wasm_val_t a[3] = { {.kind=WASM_I32,.of.i32=-1}, {.kind=WASM_I32,.of.i32=0},
                            {.kind=WASM_I32,.of.i32=0} };
        CHECK(traps(st, "Attacker", "fd_write", "iii:i", a, 3, NULL),
              "Attacker.fd_write does NOT reach HostIO's filesystem write");
    }

    /* 3. An application native is reachable even when its field name collides
     *    with a built-in — the hook owns its own module. */
    g_io_host_extra = app_host_for;
    {
        wasm_val_t out;
        int t = traps(st, "App", "gc", ":i", NULL, 0, &out);
        CHECK(!t && out.of.i32 == 4242,
              "App.gc reaches the application native, not System.gc's no-op");
    }

    /* 4. ...and registering the hook does not let it steal java.lang's names. */
    CHECK(!traps(st, "System", "gc", ":", NULL, 0, NULL),
          "System.gc still reaches the floor with a hook registered");

    /* 5. The intrinsic surface stays fail-closed: sema emits an import for every
     *    `native`, the compiler lowers these to instructions, and a call that
     *    leaked through must trap rather than return something plausible. */
    CHECK(traps(st, "F32x4", "ceil", "iI:i", NULL, 0, NULL),
          "F32x4.ceil (lowered, never called) binds to a trapping stub");
    CHECK(traps(st, "Mem", "i32_load", "ii:i",
                (wasm_val_t[]){ {.kind=WASM_I32,.of.i32=0}, {.kind=WASM_I32,.of.i32=0} }, 2, NULL),
          "Mem.i32_load (lowered, never called) binds to a trapping stub");

    /* 6. A declared signature that disagrees with the contract must NOT bind.
     *    This is the WASI shape: preview1's fd_write is (fd, iovs, iovs_len,
     *    nwritten) → errno, four arguments where HostIO's takes three. */
    {
        wasm_val_t a[4] = { {.kind=WASM_I32,.of.i32=-1}, {.kind=WASM_I32,.of.i32=0},
                            {.kind=WASM_I32,.of.i32=0},  {.kind=WASM_I32,.of.i32=0} };
        CHECK(traps(st, "HostIO", "fd_write", "iiii:i", a, 4, NULL),
              "HostIO.fd_write at the wrong arity does NOT bind to the 3-arg native");
    }
    /*    The same rule on a native that ignores its arguments, so the check is
     *    the resolver's and not wasm_func_call's argument-kind check: hio_noop
     *    would happily accept a spurious i32 and return cleanly. (System.exit
     *    is the wrong native to pin this with — bound at the right signature it
     *    ends the test process.) */
    {
        wasm_val_t a[1] = { {.kind=WASM_I32,.of.i32=0} };
        CHECK(traps(st, "System", "gc", "i:", a, 1, NULL),
              "System.gc declared (i32) does NOT bind to the ()-arity no-op");
    }

    g_io_host_extra = NULL;
    if (g_hook_last_mod.data) wasm_name_delete(&g_hook_last_mod);
    wasm_store_delete(st); wasm_engine_delete(eng);
    return TEST_SUMMARY("host abi resolution");
}
