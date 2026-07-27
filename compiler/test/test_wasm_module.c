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

    /* ── the frame the engine can actually carve ────────────────────────────
     * A method's slot count is not a quality-of-codegen matter, it is a hard
     * interface: the DDCG mints one slot per SIR temporary, jav_limits.h caps a frame at
     * MAX_LOCALS, and nothing in §7.6 bounds the count — so an over-cap function VALIDATES
     * and then traps when its frame is carved, at every call, naming nothing.
     *
     * `assemble` leaves cctx->optimize false, which is the point: slot bin-packing used to
     * live inside sir_optimize, so -O0 emitted one local per temporary and any method with a
     * few hundred subexpressions became uncallable. That is exactly the mode you reach for
     * when a method is too big to reason about.
     *
     * The body is generated rather than spelled out because the threshold is a COUNT: the
     * case has to be big enough to cross MAX_LOCALS unpacked, and a literal that size in a
     * C string would be unreadable and unmaintainable. Each line contributes several
     * temporaries; 300 of them are comfortably past 1024 without packing, and collapse to a
     * handful with it. */
    {
        char* src = NULL;
        const char* head = "class T { static int g(int x){ return x; }\n"
                           "  static int f(){ int r = 0;\n";
        for (const char* p = head; *p; p++) bbq_vec_push(src, *p);
        for (int i = 0; i < 300; i++) {
            char line[64];
            int n = snprintf(line, sizeof line, "    r = r + g(%d) + g(%d);\n", i, i + 1);
            for (int k = 0; k < n; k++) bbq_vec_push(src, line[k]);
        }
        const char* tail = "    return r; } }\n";
        for (const char* p = tail; *p; p++) bbq_vec_push(src, *p);
        bbq_vec_push(src, '\0');

        bbq_arena a; bbq_arena_init(&a, 1 << 22);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        CHECK(ok, "a 300-expression method assembles at -O0 (its frame fits MAX_LOCALS)");

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
        CHECK(ok && valid, "…and the module it produces validates");

        bbq_vec_free(mod.code); bbq_arena_free(&a); bbq_vec_free(src);
    }

    /* ── an if's join must survive a SPILLED condition ──────────────────────
     * `record_scope(test, Ljoin, 0)` keys the if-join on the head of the CONDITION's code.
     * When the condition spills — `v.length`, a field read, anything needing a temp — that
     * head is a StoreLocal, not the SIR_BRANCH, so the backend's lookup-by-node at the branch
     * found nothing, and `ljoin` fell back to the enclosing region's `stop`. Both arms were
     * then emitted all the way to it, duplicating the whole tail of the method into each arm:
     * 2^k for k such ifs.
     *
     * The oracle is MODULE LENGTH, because that is what doubles. A byte-exact pin would not
     * catch it — the bytes stay individually well-formed, there are just exponentially many
     * of them — and the shape only becomes a crash much further out, when javelinac's own C
     * stack goes (Graph.los: 130 branches re-emitted 5599 times, SIGSEGV) or when the nesting
     * passes the validator's control-depth cap. Length is the earliest honest signal.
     *
     * Measured as a DELTA per if rather than a ratio: the module carries a fixed baseline
     * (types, imports, the RTL surface) that would dilute a ratio into uselessness. Linear
     * growth means the 6→10 delta matches the 2→6 delta; exponential means it is 2^4 times
     * larger. The bound is deliberately loose — the two are far enough apart that slack still
     * separates them, and a tight one would break on any ordinary codegen change. */
    {
        char* src[3] = {0};
        const int counts[3] = { 2, 6, 10 };
        int len[3] = {0};
        bool built = true;

        for (int c = 0; c < 3; c++) {
            char* s = NULL;
            const char* head = "class T { static void g(int x){}\n  static void f(int[] v){\n";
            for (const char* p = head; *p; p++) bbq_vec_push(s, *p);
            for (int i = 0; i < counts[c]; i++) {
                char line[64];
                int n = snprintf(line, sizeof line, "    if (v.length != %d) g(%d);\n", i, i);
                for (int k = 0; k < n; k++) bbq_vec_push(s, line[k]);
            }
            const char* tail = "  } }\n";
            for (const char* p = tail; *p; p++) bbq_vec_push(s, *p);
            bbq_vec_push(s, '\0');
            src[c] = s;

            bbq_arena a; bbq_arena_init(&a, 1 << 22);
            emit_wasm_ctx mod = {0};
            if (!assemble(&a, s, &mod)) built = false;
            len[c] = (int)bbq_vec_len(mod.code);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        CHECK(built, "spilled-condition ifs assemble");

        int d1 = len[1] - len[0];      /* 2 → 6 ifs  */
        int d2 = len[2] - len[1];      /* 6 → 10 ifs */
        bool linear = built && d1 > 0 && d2 > 0 && d2 < d1 * 4;
        if (!linear)
            printf("        %d/%d/%d bytes for 2/6/10 ifs — deltas %d then %d\n",
                   len[0], len[1], len[2], d1, d2);
        CHECK(linear, "a spilled condition keeps its if-join: module grows linearly, not 2^k");

        for (int c = 0; c < 3; c++) bbq_vec_free(src[c]);
    }

    /* ── EVERY optional section at once ─────────────────────────────────────
     * COVERAGE, not a regression pin — and the distinction matters, so it is stated rather
     * than implied.
     *
     * The assembler emits a fixed SET of section kinds, most of them conditional: memory(5)
     * and tag(13) only outside plugin mode, global(6) only with statics, start(8) only with a
     * class initializer, import(2) only with host imports. Every other case in this file
     * exercises two or three, so the widest configuration — all of them present — was never
     * assembled anywhere below the e2e corpus. That gap is why an overrun of the fixed
     * `jav_section_t secs[10]` that collected them could only ever be found by compiling the
     * whole RTL and chasing a decoder failure; nothing at this level built a module with
     * enough sections to reach it.
     *
     * That overrun is now impossible by construction (the collection is a bbq_vec, so there
     * is no bound to exceed), which is exactly why this case CANNOT be falsified by undoing
     * the fix: there is no failure mode left to trigger. What it does instead is assemble the
     * widest configuration and check the result is a module at all — the magic survived, the
     * bytes validate — so any FUTURE corruption of the section list is caught here, at the
     * level that owns module assembly, instead of e2e. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        emit_wasm_ctx mod = {0};
        /* static field  → global(6);  static initializer → start(8);
         * throw/catch   → tag(13);    non-plugin mode    → memory(5) + its export. */
        bool ok = assemble(&a,
            "class T {\n"
            "  static int counter;\n"
            "  static { counter = 7; }\n"
            "  static int f(int x) {\n"
            "    try { if (x < 0) throw new IllegalArgumentException(); return counter + x; }\n"
            "    catch (IllegalArgumentException e) { return -1; }\n"
            "  }\n"
            "}", &mod);
        CHECK(ok, "a module with every optional section assembles");

        int n = (int)bbq_vec_len(mod.code);
        bool magic = ok && n >= 8 && mod.code[0] == 0x00 && mod.code[1] == 0x61
                                  && mod.code[2] == 0x73 && mod.code[3] == 0x6D;
        if (ok && !magic)
            printf("        first 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                   mod.code[0], mod.code[1], mod.code[2], mod.code[3],
                   mod.code[4], mod.code[5], mod.code[6], mod.code[7]);
        CHECK(magic, "…and still begins with the wasm magic (nothing overran the section list)");

        bool valid = false;
        if (magic) {
            wasm_engine_t* eng = wasm_engine_new();
            wasm_store_t* st = wasm_store_new(eng);
            wasm_byte_vec_t bin;
            wasm_byte_vec_new_uninitialized(&bin, (size_t)n);
            memcpy(bin.data, mod.code, (size_t)n);
            valid = wasm_module_validate(st, &bin);
            wasm_byte_vec_delete(&bin);
            wasm_store_delete(st); wasm_engine_delete(eng);
        }
        CHECK(valid, "…and validates");

        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_wasm_module");
}
