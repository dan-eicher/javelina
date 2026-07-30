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
#include "javelina/compiler/sir_support.h"   /* sir_arity / sir_child */
#include "wasm.h"   /* the c-api — the §7.6 validity pin runs the REAL validator */
#include "jav_load.h"    /* jav_validate_bytes — names the reject reason a bool cannot */
#include "jav_error.h"
#include "jav_extern.h"  /* jav_capi_last_status — the c-api's verdict, which a bool hides */
#include "jav_view_nav.h"      /* jav_view_module / jav_module_index — the two decode stages */
#include "jav_module_index.h"
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
    int nct = 0; sema_func_ent_t* cts = compiler_call_targets(cctx, mc, &nct);
    wasm_types_t wt; wasm_types_build(&wt, sctx, cts, nct);
    bbq_vec_free(cts);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out);
    wasm_types_free(&wt);
    sema_destroy(sctx);              /* 31 htrees/vecs, none of them arena-backed */
    free(sctx); free(cctx);
    return ok;
}

/* The u32 vector count that opens section `id`'s payload, or -1 if the section is absent.
 * Sections are id:u8, size:u32, payload — and every section this reads opens with a list
 * length (WASM Core 3.0 §5.5). */
static long section_vec_count(const uint8_t* p, int n, int id) {
    int i = 8;                                  /* past \0asm + version */
    while (i < n) {
        int sid = p[i++];
        uint32_t size = 0; int sh = 0;
        while (i < n) { uint8_t b = p[i++]; size |= (uint32_t)(b & 0x7F) << sh; sh += 7; if (!(b & 0x80)) break; }
        if (sid == id) {
            uint32_t cnt = 0; int csh = 0, j = i;
            while (j < n) { uint8_t b = p[j++]; cnt |= (uint32_t)(b & 0x7F) << csh; csh += 7; if (!(b & 0x80)) break; }
            return (long)cnt;
        }
        i += (int)size;
    }
    return -1;
}

/* Compile + assemble, and hand the caller the pieces needed to check the emitter against the
 * spec: the bytes, the type tables, and the methods those bytes were emitted from. */
/* `mode` matters to every index this checks. In WHOLE the prelude is compiled in, so a
 * library method resolves to a DEFINED function whether or not anything recorded a call to
 * it; only in PLUGIN must it be imported, and only there does an unrecorded target become a
 * funcidx of -1. A mode-blind gate over WHOLE alone cannot see that class of bug. */
static bool assemble_full_mode(bbq_arena* a, const char* src, int mode, emit_wasm_ctx* out,
                               wasm_types_t* wt, sir_method_t*** methods_out, int* mc_out,
                               sema_ctx_t** sctx_out) {
    ast_program_t* prog = jtest_build_flat(jtest_with_imports(JTEST_STD_IMPORTS, src), a);
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = jtest_last_nlib;
    sctx->mode = mode;
    jtest_analyze(sctx);
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    int nct = 0; sema_func_ent_t* cts = compiler_call_targets(cctx, mc, &nct);
    wasm_types_build(wt, sctx, cts, nct);
    bbq_vec_free(cts);
    bool ok = wasm_assemble_program(cctx, sctx, wt, methods, mc, out);
    *methods_out = methods; *mc_out = mc; *sctx_out = sctx;
    free(cctx);
    return ok;
}

#define assemble_full(a, src, out, wt, ms, mc, sc) \
    assemble_full_mode((a), (src), SEMA_MODE_WHOLE, (out), (wt), (ms), (mc), (sc))

/* Every call target named by the emitted code, collected the way burg reads them: off each
 * Invoke node's own class/method (codegen_wasm.burg's Invoke rules all emit
 * `wasm_func_index(node->invoke_*.class_id, ...)`). */
static void each_invoke_resolves(const wasm_types_t* wt, const sir_node_t* n,
                                 const sir_node_t** seen, int* nseen, int cap,
                                 int* checked, int* unresolved) {
    if (!n || *nseen >= cap) return;
    for (int i = 0; i < *nseen; i++) if (seen[i] == n) return;
    seen[(*nseen)++] = n;
    int cls = -1, mid = -1;
    if (n->tag == SIR_INVOKESTATIC)  { cls = n->invoke_static.class_id;  mid = n->invoke_static.method_idx; }
    if (n->tag == SIR_INVOKESPECIAL) { cls = n->invoke_special.class_id; mid = n->invoke_special.method_idx; }
    if (cls >= 0) {
        (*checked)++;
        if (wasm_func_index(wt, cls, mid) < 0) (*unresolved)++;
    }
    for (int i = 0; i < sir_arity((sir_node_t*)n); i++)
        each_invoke_resolves(wt, sir_child((sir_node_t*)n, i), seen, nseen, cap, checked, unresolved);
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
                if (!valid) {
                    bbq_arena da; bbq_arena_init(&da, 0);
                    bbq_capture_metadata m2 = jav_view_module(mod.code, bbq_vec_len(mod.code), &da);
                    jav_modidx_t mi2;
                    int indexed = m2.success && jav_module_index(m2.root, mod.code, &da, &mi2);
                    printf("    decode: view=%d index=%d (capi status %d)\n",
                           (int)m2.success, indexed, (int)jav_capi_last_status(st));
                    bbq_arena_free(&da);
                }
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

    /* ── The emitter against the binary-format spec ───────────────────────────────────────
     *
     * WASM Core 3.0 §5.5.16, verbatim: "The lengths of lists produced by the (possibly empty)
     * FUNCTION AND CODE SECTION must match up." Both count this module's DEFINED functions —
     * §2.5.1 puts imports first in the func index space, so neither counts them. A module that
     * gets this wrong is malformed before any body is type-checked, which is why the failure
     * surfaces as a bare "type mismatch" naming nothing.
     *
     * And the property that decides whether a call can be emitted at all: every Invoke node in
     * the methods these bytes came from must resolve to a funcidx. burg reads each call's index
     * off the node itself, so an unresolved one becomes an out-of-range operand and the module
     * fails validation with "unknown function". */
    {
        /* The ddcg SYNTHESIZES Invoke nodes the source never wrote, and each such lowering is
         * its own path to an unrecorded call target — invisible to a corpus of source-written
         * calls. One program per synthesizing lowering, so a new one that forgets to record
         * fails here rather than in an e2e run. */
        static const char* srcs[] = {
            "class M { static int f(int x){ return x + 1; } }",
            "class M { static int f(int x){ StringBuffer b = new StringBuffer(); b.append(x); return b.toString().length(); } }",
            "class M { int v; M(int x){ v = x; } static int f(int x){ return new M(x).v; } }",
            /* the §15.10 index guard synthesizes `new IndexOutOfBoundsException()` */
            "class M { static int f(int[] a){ return a[0] + a.length; } }",
            /* §15.16 and §15.19.2 both synthesize a Class.isInstance call, and they are
             * SEPARATE lowerings — kept in separate programs, because one records the same
             * (Class, isInstance) pair the other needs and would mask its omission. */
            "class M { static int f(Object o){ String[] s = (String[]) o; return s.length; } }",
            "class M { static int f(Object o){ return (o instanceof String[]) ? 1 : 0; } }",
            /* §12.4.1 the $ensure_init barrier on a cross-class static reference */
            "class A { static int x = 5; } class M { static int f(int y){ return A.x + y; } }",
            /* §15.18.1 string concatenation synthesizes the StringBuffer chain */
            "class M { static int f(int x){ return (\"v=\" + x + \"!\").length(); } }",
        };
        static const int modes[] = { SEMA_MODE_WHOLE, SEMA_MODE_PLUGIN };
        for (int mi = 0; mi < 2; mi++)
        for (int i = 0; i < (int)(sizeof srcs / sizeof srcs[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            wasm_types_t wt; sir_method_t** ms = NULL; int mc = 0; sema_ctx_t* sc = NULL;
            bool ok = assemble_full_mode(&a, srcs[i], modes[mi], &mod, &wt, &ms, &mc, &sc);
            CHECK(ok, "the module assembled (precondition)");
            if (ok) {
                int n = (int)bbq_vec_len(mod.code);
                long nfunc = section_vec_count(mod.code, n, 3);    /* function section */
                long ncode = section_vec_count(mod.code, n, 10);   /* code section */
                CHECK(nfunc >= 0 && ncode >= 0 && nfunc == ncode,
                      "JLS-free, WASM 5.5.16: the function and code section lengths match up");

                const sir_node_t** seen = (const sir_node_t**)malloc(65536 * sizeof *seen);
                int checked = 0, unresolved = 0;
                for (int m = 0; m < mc; m++) {
                    int nseen = 0;
                    each_invoke_resolves(&wt, ms[m]->entry, seen, &nseen, 65536,
                                         &checked, &unresolved);
                }
                free(seen);
                if (unresolved) printf("    src %d mode %d: %d unresolved\n", i, modes[mi], unresolved);
                CHECK(checked > 0 && unresolved == 0, modes[mi] == SEMA_MODE_PLUGIN
                      ? "PLUGIN: every Invoke node in the emitted methods resolves to a funcidx"
                      : "WHOLE: every Invoke node in the emitted methods resolves to a funcidx");

                /* The end property the walk above is a proxy for. A funcidx can also be named
                 * by an emitted vtable row or a const-expr, which no SIR walk reaches, so run
                 * the REAL §7 validator over the bytes — the same gate javelinac applies
                 * before it writes an artifact. An emitted -1 surfaces here as
                 * "unknown function". */
                jav_err_t verr = JAV_E_NONE;
                jav_status_t vst = jav_validate_bytes(mod.code, (size_t)n, &verr);
                if (vst != JAV_OK)
                    printf("    src %d mode %d: %s\n", i, modes[mi],
                           verr != JAV_E_NONE ? jav_err_str(verr) : "no §7 reason");
                CHECK(vst == JAV_OK, modes[mi] == SEMA_MODE_PLUGIN
                      ? "PLUGIN: the emitted module passes the VM's §7 validator"
                      : "WHOLE: the emitted module passes the VM's §7 validator");
            }
            wasm_types_free(&wt); sema_destroy(sc); free(sc);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    return TEST_SUMMARY("test_wasm_module");
}
