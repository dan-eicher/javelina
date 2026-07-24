// test_wasm_module.c — assemble a compiled program into a .wasm module via the
// shared jav_module_write path (wasm_assemble_program). Asserts the assembler
// succeeds (every func body passed the jav_func_body_read spec-grammar gate), the
// module carries the magic/version, and each signature shape encodes the right
// valtypes. Execution is covered by test_exec; this pins the byte-level assembly.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/wasm_module.h"
#include "wasm.h"   /* the c-api — the §7.6 validity pin runs the REAL validator */
#include "bbq_arena.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

/* §7.3 per-unit parse (see jtest_units.h) — the flat program still feeds
 * compiler_compile; sema gets the unit list via jtest_analyze. */
#include "jtest_units.h"

/* Compile + assemble `src` into `out`; returns the assembler's ok flag. */
static bool assemble(bbq_arena* a, const char* src, emit_wasm_ctx* out) {
    ast_program_t* prog = jtest_build_flat(jtest_with_imports(JTEST_STD_IMPORTS, src), a);
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = jtest_last_nlib; jtest_analyze(sctx);
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    wasm_types_t wt; wasm_types_build(&wt, sctx);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out);
    wasm_types_free(&wt);
    sema_destroy(sctx);              /* 31 htrees/vecs, none of them arena-backed */
    free(sctx); free(cctx);
    return ok;
}

static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}

/* Assemble `src`, assert ok + magic/version, and that `sig` bytes appear (the
 * functype encoding for the method's signature). */
static void check_sig(const char* src, const uint8_t* sig, int siglen, const char* label) {
    bbq_arena a; bbq_arena_init(&a, 1 << 18);
    emit_wasm_ctx mod = {0};
    bool ok = assemble(&a, src, &mod);
    CHECK(ok, label);
    int n = (int)bbq_vec_len(mod.code);
    const uint8_t magic[] = { 0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00 };
    CHECK(ok && n >= 8 && !memcmp(mod.code, magic, 8), "module begins with \\0asm + version 1");
    CHECK(ok && contains(mod.code, n, sig, siglen), label);
    bbq_vec_free(mod.code);
    bbq_arena_free(&a);
}

int main(void) {
    /* (i32,i32)->i32 functype: 0x60 02 7F 7F 01 7F */
    { const uint8_t s[] = { 0x60,0x02,0x7F,0x7F,0x01,0x7F };
      check_sig("class T { static int add(int a, int b){ return a+b; } }", s, 6,
                "i32 functype (i32,i32)->i32"); }
    /* void return → zero results: 0x60 00 00 */
    { const uint8_t s[] = { 0x60,0x00,0x00 };
      check_sig("class T { static void f(){ return; } }", s, 3, "void functype (60 00 00)"); }
    /* instance method → `this` is a concrete (ref null $T): 0x60 01 63 <heaptype>…
     * (the typeidx LEB depends on the topo class order, so pin just the shape —
     * func, 1 param, a nullable concrete ref — proving the eqref placeholder is gone). */
    { const uint8_t s[] = { 0x60,0x01,0x63 };
      check_sig("class T { int f(){ return 0; } }", s, 3, "instance functype prepends concrete `this` ref"); }
    /* long → i64: 0x60 01 7E 01 7E */
    { const uint8_t s[] = { 0x60,0x01,0x7E,0x01,0x7E };
      check_sig("class T { static long f(long a){ return a; } }", s, 5, "long functype i64->i64"); }
    /* double → f64: 0x60 01 7C 01 7C */
    { const uint8_t s[] = { 0x60,0x01,0x7C,0x01,0x7C };
      check_sig("class T { static double f(double a){ return a; } }", s, 5, "double functype f64->f64"); }
    /* a body local → the code section carries an i32 locals run (01 01 7F). */
    { bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
      bool ok = assemble(&a, "class T { static int f(int x){ int y = x + 1; return y; } }", &mod);
      CHECK(ok, "locals: assembled f");
      const uint8_t run[] = { 0x01, 0x01, 0x7F };
      CHECK(ok && contains(mod.code, (int)bbq_vec_len(mod.code), run, 3),
            "locals: code section carries the i32 locals run");
      bbq_vec_free(mod.code); bbq_arena_free(&a); }

    /* ── §7.6 VALIDITY of the emitted module — the compiler-owned pin. The
     * assembler's own audit is §5.5.1 wellformedness only; nothing at the
     * compiler level asserted "the bytes type-check" until a v128 module
     * shipped a §3.4.7 struct.set mismatch that only the VM (rightly)
     * rejected. Runs the REAL validation stack via the c-api. The control
     * case guards the harness; the v128 cases are the pin. */
    {
        struct { const char* src; const char* label; } vc[] = {
          { "class T { static int f(){ return 42; } }",
            "§7.6: a plain module validates (harness control)" },
          { "class T { static int f(){"
            "  V128 v = I32x4.splat(7); return I32x4.extract_lane(v, 2); } }",
            "§7.6: a v128 locals/intrinsics module validates" },
          { "class T { static int f(){"
            "  V128[] a = new V128[3]; a[1] = I32x4.splat(7);"
            "  return I32x4.extract_lane(a[1], 0); } }",
            "§7.6: a V128[] (overlay + (array v128) backing) module validates" },
        };
        for (size_t i = 0; i < sizeof vc / sizeof vc[0]; i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, vc[i].src, &mod);
            bool valid = false;
            if (ok) {
                wasm_engine_t* eng = wasm_engine_new();
                wasm_store_t* st = wasm_store_new(eng);
                wasm_byte_vec_t bin;
                wasm_byte_vec_new_uninitialized(&bin, bbq_vec_len(mod.code));
                memcpy(bin.data, mod.code, bbq_vec_len(mod.code));
                valid = wasm_module_validate(st, &bin);
                wasm_byte_vec_delete(&bin);
                wasm_store_delete(st); wasm_engine_delete(eng);
            }
            CHECK(ok && valid, vc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    return TEST_SUMMARY("test_wasm_module");
}
