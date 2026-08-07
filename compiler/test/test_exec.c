/* test_exec.c — E0: EXECUTE compiled modules in the VM (the trusted oracle),
 * via the public wasm.h harness (exec.h). Proves the whole-program assembler:
 * multi-function modules, internal static-call resolution, and all scalar widths
 * (i32/i64/f32/f64) — assembled, loaded, called, and asserted on the result.
 *
 * Not a byte-shape test (that's test_wasm_module): here the bytes must actually
 * RUN and compute the right value. */
#include "java_parser.h"
#include "javelina/compiler/java_source.h"   /* §3.2 step 1 — the ONE parse entry (see header) */
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/wasm_module.h"
#include "bbq_arena.h"
#include "bbq_vec.h"
#include "exec.h"
#define exec_call exec_call_shared   /* Stage D: route the whole corpus through the shared long-lived jre */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#define JT_VERBOSE      /* a four-minute suite wants a progress log */
#define JT_REPORT_RSS   /* this suite compiles the whole prelude per case */
#include "javelina_test.h"
#include "jtest_units.h"   /* JTEST_STD_IMPORTS + jtest_with_imports (§7.5 snippet header) */

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}
/* Parse `src` into a fresh context whose arena owns the AST — it must outlive the
 * compile, so the context is pushed onto `*ctxs` (a bbq_vec) for the caller to free
 * afterward. Idents/literals are jdup'd into the arena, so the source buffer (`src`)
 * need not survive the call. */
static ast_program_t* parse_src(const char* src, java_parse_ctx_t*** ctxs) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16); pc->result = NULL; pc->file = NULL;
    peg_state p; char* tsrc = NULL; const char* terr = NULL;
    if (!java_source_init(&p, src, (int)strlen(src), &tsrc, &terr)) return NULL;
    p.user_data = pc;
    ast_program_t* prog = java_parser_parse(&p) ? pc->result : NULL;
    free(tsrc);
    bbq_vec_push(*ctxs, pc);
    return prog;
}
/* java.lang stubs (for sema's Object root etc.) + the user source, merged. The
 * stubs are pushed FIRST (lowest class_ids); *nlib_out reports how many, so the
 * driver can tell sema which classes are the library (host imports). Each file's
 * parse context is recorded in `*ctxs` for the caller to free post-compile. */
/* Glob one prelude package dir (`lib/java/<pkg>`), parsing every *.java into `types`.
 * The per-file `package` decl (not the dir) drives package resolution in sema. */
static void glob_lib_dir(const char* dir, ast_type_decl_t*** types,
                         java_parse_ctx_t*** ctxs) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) { size_t L=strlen(e->d_name);
        if (L<6 || strcmp(e->d_name+L-5,".java")) continue;
        char path[512]; snprintf(path,sizeof path,"%s/%s",dir,e->d_name);
        char* s = read_file(path); if(!s) continue;
        ast_program_t* p = parse_src(s, ctxs); free(s); if(!p) continue;
        for (int i=0;i<p->types_count;i++) bbq_vec_push(*types, p->types[i]);
    } closedir(d);
}

/* The prelude (the lib/java tree) is identical for every test, but big (the generated CharacterData is
 * ~24k lines). Parse it ONCE into process-lifetime storage and reuse the AST across all assemble()
 * calls. (Before this cache, re-globbing + re-parsing the lib per test dominated the run: ~6 min.)
 *
 * Sharing it is safe, but NOT because sema leaves the AST alone — it does not. Resolving a
 * constructor DESUGARS into the AST: JLS §8.8.7 prepends the implicit super(), allocating the
 * statement and a replacement statement array from ctx->arena and writing them back into the node.
 * The prepend is guarded (it re-reads stmts[0] and skips when a constructor call is already there),
 * so it happens exactly once — but every later pass DEREFERENCES stmts[0] to check.
 *
 * What makes that safe here is an ordering invariant worth stating, because nothing enforces it:
 * the FIRST sema over the prelude is assemble_jre(), run against jre_arena, and jre_arena is not
 * freed until the end of main. So the rewrite lands in storage that outlives every per-test arena.
 * Move the jre build later, or free jre_arena early, and all the per-test compiles start reading
 * freed memory — silently, since the bytes usually survive. (test_sir makes the same guarantee
 * explicitly with a dedicated prelude arena.) */
static ast_type_decl_t** g_lib_types = NULL;
static int               g_lib_ntypes = 0;
static ast_program_t**   g_lib_units  = NULL;   /* per-FILE programs — §7.3 units */
static int               g_lib_nunits = 0;

/* The current §7.3 unit list (cached lib units + this build's user unit) —
 * what exec_analyze feeds sema_analyze_units. Rebuilt per build_program call. */
static ast_program_t** g_units = NULL;
static int             g_nunits = 0;

static ast_program_t* build_program(const char* user_src, bbq_arena* arena, int* nlib_out,
                                    java_parse_ctx_t*** ctxs) {
    if (!g_lib_types) {                          /* parse the lib once; its ctxs persist (never freed) */
        static java_parse_ctx_t** lib_ctxs = NULL;
        ast_type_decl_t** lt = NULL;
        glob_lib_dir("lib/java/lang", &lt, &lib_ctxs);   /* §20 java.lang */
        glob_lib_dir("lib/java/util", &lt, &lib_ctxs);   /* §21 java.util */
        glob_lib_dir("lib/java/io",   &lt, &lib_ctxs);   /* §22 java.io */
        glob_lib_dir("lib/javelina/simd", &lt, &lib_ctxs);   /* javelina.simd intrinsics */
        g_lib_ntypes = (int)bbq_vec_len(lt);
        g_lib_types  = (ast_type_decl_t**)malloc((size_t)g_lib_ntypes * sizeof(*g_lib_types));
        memcpy(g_lib_types, lt, (size_t)g_lib_ntypes * sizeof(*g_lib_types));
        bbq_vec_free(lt);
        /* The units are the per-file parse results, in glob order. */
        g_lib_nunits = 0;
        g_lib_units = (ast_program_t**)malloc(bbq_vec_len(lib_ctxs) * sizeof(*g_lib_units));
        for (int i = 0; i < (int)bbq_vec_len(lib_ctxs); i++)
            if (lib_ctxs[i]->result) g_lib_units[g_lib_nunits++] = lib_ctxs[i]->result;
    }
    ast_type_decl_t** types = NULL;              /* bbq_vec: cached lib types + this test's user types */
    for (int i = 0; i < g_lib_ntypes; i++) bbq_vec_push(types, g_lib_types[i]);
    if (nlib_out) *nlib_out = g_lib_ntypes;      /* everything from the lib is library */
    ast_program_t* up = user_src
        ? parse_src(jtest_with_imports(JTEST_STD_IMPORTS, user_src), ctxs)
        : NULL;                                  /* NULL user_src = prelude only (jre build) */
    /* A user source that did not PARSE is a failure, not an empty contribution. Without this
     * the library alone assembles and assemble_plugin returns true, so a case with a syntax
     * error passes vacuously — the harness could not express "must not compile", and any test
     * whose snippet silently stopped parsing would go green. (NULL user_src is the legitimate
     * prelude-only build and keeps its meaning.) */
    if (user_src && !up) { bbq_vec_free(types); return NULL; }
    if (up) for (int i=0;i<up->types_count;i++) bbq_vec_push(types, up->types[i]);
    free(g_units);
    g_nunits = g_lib_nunits + (up ? 1 : 0);
    g_units = (ast_program_t**)malloc((size_t)g_nunits * sizeof(*g_units));
    memcpy(g_units, g_lib_units, (size_t)g_lib_nunits * sizeof(*g_units));
    if (up) g_units[g_lib_nunits] = up;
    int tc = (int)bbq_vec_len(types);
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr,types,(size_t)tc*sizeof(*arr));
    bbq_vec_free(types);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
}

/* The sema entry: the §7.3 unit list from the last build_program. */
static bool exec_analyze(sema_ctx_t* c) { return sema_analyze_units(c, g_units, g_nunits); }

static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}

/* JAVELINA_CLICK=1 runs the WHOLE corpus with the Click optimizer ON (the
 * re-enable gate: every case must execute identically optimized). */
static bool click_on(void) {
    const char* e = getenv("JAVELINA_CLICK");
    return e && *e && strcmp(e, "0") != 0;
}

/* Compile `src` and assemble the whole-program module into `out`. Returns false if
 * sema reported ANY error (fail-closed — a real embedder must not emit code for a
 * program that failed type-checking; silently compiling past sema errors is how a
 * mis-resolved overload / bad IR reaches the validator as an opaque "rejected") or a
 * func body failed the spec-grammar gate. Sema diagnostics are printed so a failure
 * is legible at the source, not just as a downstream validator reject. */
static bool assemble(bbq_arena* a, const char* src, emit_wasm_ctx* out) {
    java_parse_ctx_t** ctxs = NULL;          /* bbq_vec of parse contexts to free post-compile */
    int nlib = 0;
    ast_program_t* prog = build_program(src, a, &nlib, &ctxs);
    if (!prog) return false;                     /* did not parse — see build_program */
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = nlib;
    sctx->mode = SEMA_MODE_PLUGIN;               /* Stage D: corpus runs as linked plugins */
    exec_analyze(sctx);
    bool sema_ok = true;
    if (sema_error_count(sctx) > 0) {
        sema_ok = false;
        int nd = 0; const sema_diag_t* diags = sema_diags(sctx, &nd);
        for (int di = 0; di < nd; di++)
            if (diags[di].level == DIAG_ERROR)
                fprintf(stderr, "  [sema error] %d:%d  %s\n", diags[di].loc.line, diags[di].loc.col, diags[di].message);
    }
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    cctx->optimize = click_on();
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    int nct = 0; sema_func_ent_t* cts = compiler_call_targets(cctx, mc, &nct);
    wasm_types_t wt; wasm_types_build(&wt, sctx, cts, nct);
    bbq_vec_free(cts);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out) && sema_ok;
    wasm_types_free(&wt);
    compiler_destroy(cctx); free(cctx);
    sema_destroy(sctx);     free(sctx);
    for (int i = 0; i < (int)bbq_vec_len(ctxs); i++) { bbq_arena_free(&ctxs[i]->arena); free(ctxs[i]); }
    bbq_vec_free(ctxs);
    return ok;
}

/* Build the java.lang runtime module (jre.wasm) ONCE: the prelude alone, RUNTIME mode — every
 * library function + Class-singleton/static-field global is EXPORTED for plugins to import.
 * Same pipeline as assemble(), no user source. */
static bool assemble_jre(bbq_arena* a, emit_wasm_ctx* out) {
    java_parse_ctx_t** ctxs = NULL;
    int nlib = 0;
    ast_program_t* prog = build_program(NULL, a, &nlib, &ctxs);   /* NULL user = prelude only */
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = nlib; sctx->mode = SEMA_MODE_RUNTIME;
    exec_analyze(sctx);
    bool sema_ok = sema_error_count(sctx) == 0;
    if (!sema_ok) {
        int nd = 0; const sema_diag_t* diags = sema_diags(sctx, &nd);
        for (int di = 0; di < nd; di++)
            if (diags[di].level == DIAG_ERROR)
                fprintf(stderr, "  [jre sema error] %d:%d  %s\n", diags[di].loc.line, diags[di].loc.col, diags[di].message);
    }
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    cctx->optimize = click_on();
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    int nct = 0; sema_func_ent_t* cts = compiler_call_targets(cctx, mc, &nct);
    wasm_types_t wt; wasm_types_build(&wt, sctx, cts, nct);
    bbq_vec_free(cts);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out) && sema_ok;
    wasm_types_free(&wt);
    compiler_destroy(cctx); free(cctx);
    sema_destroy(sctx);     free(sctx);
    for (int i = 0; i < (int)bbq_vec_len(ctxs); i++) { bbq_arena_free(&ctxs[i]->arena); free(ctxs[i]); }
    bbq_vec_free(ctxs);
    return ok;
}

/* Build a thin PLUGIN module: user `src` compiled with java.lang IMPORTED from jre. Same
 * pipeline as assemble(), PLUGIN mode. Used by the linked (shared-jre) tests. */
static bool assemble_plugin(bbq_arena* a, const char* src, emit_wasm_ctx* out) {
    java_parse_ctx_t** ctxs = NULL;
    int nlib = 0;
    ast_program_t* prog = build_program(src, a, &nlib, &ctxs);
    if (!prog) return false;                     /* did not parse — see build_program */
    sema_ctx_t* sctx = (sema_ctx_t*)malloc(sizeof *sctx);
    sema_init(sctx, a); sctx->num_library_classes = nlib; sctx->mode = SEMA_MODE_PLUGIN;
    exec_analyze(sctx);
    bool sema_ok = sema_error_count(sctx) == 0;
    if (!sema_ok) {
        int nd = 0; const sema_diag_t* diags = sema_diags(sctx, &nd);
        for (int di = 0; di < nd; di++)
            if (diags[di].level == DIAG_ERROR)
                fprintf(stderr, "  [plugin sema error] %d:%d  %s\n", diags[di].loc.line, diags[di].loc.col, diags[di].message);
    }
    compiler_ctx_t* cctx = (compiler_ctx_t*)malloc(sizeof *cctx);
    compiler_init(cctx, a, sctx);
    cctx->optimize = click_on();
    int mc = 0; sir_method_t** methods = compiler_compile(cctx, prog, &mc);
    int nct = 0; sema_func_ent_t* cts = compiler_call_targets(cctx, mc, &nct);
    wasm_types_t wt; wasm_types_build(&wt, sctx, cts, nct);
    bbq_vec_free(cts);
    bool ok = wasm_assemble_program(cctx, sctx, &wt, methods, mc, out) && sema_ok;
    wasm_types_free(&wt);
    compiler_destroy(cctx); free(cctx);
    sema_destroy(sctx);     free(sctx);
    for (int i = 0; i < (int)bbq_vec_len(ctxs); i++) { bbq_arena_free(&ctxs[i]->arena); free(ctxs[i]); }
    bbq_vec_free(ctxs);
    return ok;
}

int main(void) {
    /* (Stage D infrastructure) Build jre.wasm ONCE and instantiate it on a shared store that
     * outlives every plugin. The linked tests below compile as thin PLUGINs (assemble_plugin) and
     * run against this jre via exec_call_shared — jre's java.lang bodies compiled a single time.
     * The main corpus still runs self-contained WHOLE modules (exec_call); flipping it wholesale
     * onto the linked path is gated on the plugin type-layout supporting user-introduced object/
     * array/string types (a plugin using only primitives links + runs today, see Stage C). */
    bbq_arena jre_arena; bbq_arena_init(&jre_arena, 1 << 20);
    emit_wasm_ctx jre = {0};
    bool jre_ok = assemble_jre(&jre_arena, &jre) && exec_jre_init(jre.code, bbq_vec_len(jre.code));
    CHECK(jre_ok, "jre.wasm: builds + instantiates once on the shared store");

    /* (Stage C) a thin plugin links against the shared jre and runs. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int add(int x, int y){ return x + y; }"
            "  static int sum(){ return add(3, 5) + add(10, 20); } }", &mod);
        CHECK(pb, "Stage C: plugin(T.sum) builds");
        if (pb) {
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.sum", NULL, 0, res, 1);
            CHECK(st == EXEC_OK && res[0].of.i32 == 38, "Stage C: linked plugin T.sum() == 38");
        }
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* Virtual dispatch through the shared jre: an imported Object.hashCode() call from a
     * plugin is consistent for one object; a plugin-local override resolves most-derived. */
    {
        struct { const char* src; int32_t want; const char* label; } vd[] = {
          { "class Box { } class T { static int f(){ Box b = new Box();"
            "  return b.hashCode() == b.hashCode() ? 1 : 0; } }",
            1, "imported Object.hashCode() dispatch consistent for one object" },
          { "class A { int m(){ return 5; } } class B extends A { int m(){ return 7; } }"
            " class T { static int f(){ A a = new B(); return a.m(); } }",
            7, "virtual override: A a = new B(); a.m() == 7" },
          /* Construction stays visible through calls that cannot write a final
           * cell: construct, mutate the final field's REFERENT through calls
           * in a loop, read the field after — the value is the pin. The cooc
           * bench kernel caught this as an -O NullPointerException when the
           * constructor's writes went invisible (calls under ExprEffect /
           * StoreLocal wrappers were never recognized as ctor invocations, so
           * final cells kept no record of construction); this is the same
           * shape at its owning level. Three +1 passes leave every element 3,
           * so s = 3·(31³+31²+31+1) = 3·30784 = 92352. */
          /* §15.26.2 / §15.14.1: a compound assignment or increment on an
           * array component performs the SAME §15.12 checks as a plain access
           * — the write-back lowerings emitted none (no null check, no bounds
           * pair), so an OOB `a[i] += v` reached the VM's own trap instead of
           * throwing a catchable ArrayIndexOutOfBoundsException. */
          { "class T { static int f(){ int[] a = new int[4];"
            "  try { a[5] += 1; } catch (ArrayIndexOutOfBoundsException e)"
            "  { return 1; } return 0; } }",
            1, "OOB compound assignment throws catchable AIOOBE" },
          { "class T { static int f(){ int[] a = new int[4];"
            "  try { a[7]++; } catch (ArrayIndexOutOfBoundsException e)"
            "  { return 2; } return 0; } }",
            2, "OOB post-increment throws catchable AIOOBE" },
          { "class T { static int f(){ int[] a = null;"
            "  try { a[0] += 1; } catch (NullPointerException e)"
            "  { return 3; } return 0; } }",
            3, "compound assignment on a null array throws catchable NPE" },
          { "class Q { final int[] y = new int[4];"
            "  void m(){ int k = 0; while (k < 4) { y[k] = y[k] + 1; k = k + 1; } } }"
            " class T { static int f(){"
            "   Q c = new Q();"
            "   int i = 0;"
            "   while (i < 3) { c.m(); i = i + 1; }"
            "   int s = 0;"
            "   i = 0;"
            "   while (i < 4) { s = s * 31 + c.y[i]; i = i + 1; }"
            "   return s; } }",
            92352, "a final field read after a call loop sees CONSTRUCTION — "
                   "the ctor's writes reach every later reader" },
        };
        for (int i = 0; i < (int)(sizeof vd / sizeof vd[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
            bool pb = assemble_plugin(&a, vd[i].src, &mod);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = pb ? exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
            CHECK(pb && st == EXEC_OK && res[0].of.i32 == vd[i].want, vd[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* §15.11.1 / §6.5.2 — a package-qualified static CALL, end to end. The
     * qualified-field form (java.lang.Integer.MAX_VALUE) already worked; the
     * call form is the same reclassification of the same AmbiguousName, so it
     * has to reach the same place and actually run. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){"
            "  int n = java.lang.Integer.parseInt(\"40\");"
            "  int m = java.lang.Math.abs(-2);"
            "  String s = java.lang.String.valueOf(n + m);"
            "  return java.lang.Integer.parseInt(s); } }", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = pb ? exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(pb && st == EXEC_OK && res[0].of.i32 == 42,
              "§6.5.2 package-qualified static calls compile and run");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §20.1.2 Object.toString() = getClass().getName() + '@' + hex(hashCode()); stable per
     * object. The default toString() was the one E7.2 library obligation without a committed
     * case — array Class names (§10.8), clone (§10.7/§20.1.5), ArrayStoreException (§10.10),
     * and the property wrappers/Math.random (§20) are already covered further below. ASCII
     * only, so nothing here touches the deferred §3.3 Unicode source-model gap. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){"
            "  Object o = new Object(); String s = o.toString();"
            "  if (!s.startsWith(\"java.lang.Object@\")) return 0;"
            "  if (!o.toString().equals(s)) return 0;"
            "  if (!(\"x\" + o).startsWith(\"xjava.lang.Object@\")) return 0;"
            "  return 1; } }", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = pb ? exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(pb && st == EXEC_OK && res[0].of.i32 == 1, "§20.1.2 Object.toString() over identity hashCode");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §7.5 import canonical descriptor — a type has ONE descriptor regardless of spelling.
     * `sz` declares its param with the FULLY-QUALIFIED `java.util.Vector`; `f` names the SAME
     * type by the IMPORTed simple name. If the spelling leaked into the mangled descriptor
     * (the old bug — import → `Ljava/util/Vector;` vs flat `LVector;`), the internal `sz` call
     * and the cross-module `Vector` methods would fail to link. Runs == 2. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "import java.util.Vector;"
            "class T {"
            "  static int sz(java.util.Vector v) { return v.size(); }"
            "  static int f(){ Vector v = new Vector(); v.addElement(\"a\"); v.addElement(\"b\"); return sz(v); }"
            "}", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = pb ? exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(pb && st == EXEC_OK && res[0].of.i32 == 2,
              "§7.5 import/FQN/simple resolve to ONE canonical descriptor (cross-module link)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §3.2 step 1 / §3.3 Unicode escapes + §3.10.6 escapes + UTF-8 source decode.
     * `char` is a UTF-16 code unit, so a decoded escape/byte becomes one (or, for astral, two)
     * char[] elements. Covers: \u in string and char, multiple 'u', the even/odd-backslash rule
     * (`"\\u0041"` is a literal backslash + u0041), UTF-8 (`café`'s é = U+00E9 = one code
     * unit), octal/\t, and — since §3.3 is now a translation over the RAW stream rather than a
     * per-literal decode — an escape spelling an IDENTIFIER and a KEYWORD. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){"
            "  String u = \"\\u0041\\u0042\";"
            "  if (u.length()!=2 || u.charAt(0)!=65 || u.charAt(1)!=66) return 0;"
            "  char cu = '\\u0043'; if (cu != 67) return 0;"
            "  if (!\"\\uuu0041\".equals(\"A\")) return 0;"
            "  String utf = \"caf\xc3\xa9\";"
            "  if (utf.length()!=4 || utf.charAt(3)!=0x00e9) return 0;"
            "  String bs = \"\\\\u0041\";"
            "  if (bs.length()!=6 || bs.charAt(0)!=92 || bs.charAt(1)!=117) return 0;"
            "  String oc = \"\\101\\t\";"
            "  if (oc.length()!=2 || oc.charAt(0)!=65 || oc.charAt(1)!=9) return 0;"
            "  \\u0069\\u006e\\u0074 \\u0041 = 7; if (\\u0041 != 7) return 0;"
            "  char om = '\\u03a9'; if (om != 0x03a9) return 0;"
            "  return 1; } }", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = pb ? exec_call_shared(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(pb && st == EXEC_OK && res[0].of.i32 == 1,
              "§3.3 \\u escapes: literals, multi-u, even/odd, UTF-8, §3.10.6 octal — and an "
              "escape spelling an identifier and the keyword `int`");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §3.3's no-re-participation rule, which is a REJECTION and not a value.
     *
     *   "The character produced by a Unicode escape does not participate in further Unicode
     *    escapes ... \u005cu005a results in the six characters \ u 0 0 5 a ... It does not
     *    result in the character Z."
     *
     * Those six characters are then tokenized, and §3.10.5's StringCharacter is
     * `InputCharacter but not " or \` OR an EscapeSequence — a bare backslash must begin an
     * EscapeSequence, and §3.10.6 has no \u. So the literal is invalid in step 3, exactly as
     * §3.10.5 says of "\u000a": "the string literal is not valid in step 3".
     *
     * This check previously asserted a 6-character string, which is neither Z nor Java — it
     * pinned a per-literal \u decoder that ran AFTER translation and so contradicted §3.3. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){ String nr = \"\\u005cu005a\"; return nr.length(); } }", &mod);
        CHECK(!pb, "§3.3 no re-participation: \\u005cu005a yields a bare backslash, and "
                   "§3.10.5/§3.10.6 make that an invalid string literal — REJECTED, not 'Z'");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §3.8's rejection half. An identifier is JavaLetter{JavaLetterOrDigit}, and a Java letter
     * is any Unicode LETTER — not any non-ASCII character. Accepting a symbol would be as wrong
     * as rejecting a letter, and only one of those two directions can be asserted from inside a
     * program that has to compile, so the other lives here. U+2603 SNOWMAN is category So. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){ int ☃ = 5; return ☃; } }", &mod);
        CHECK(!pb, "§3.8: U+2603 SNOWMAN is a SYMBOL, not a Java letter — rejected as an identifier");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    {   /* …and the same shape written as an escape, since §3.3 translation runs first and the
         * classifier must see the same code point either way. */
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a,
            "class T { static int f(){ int \\u2603 = 5; return \\u2603; } }", &mod);
        CHECK(!pb, "§3.8: \\u2603 spells the same symbol — rejected identically to the literal form");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    {   /* §3.9: a keyword is not available as a name. `const` and `goto` are reserved-and-unused
         * precisely so this stays true. */
        bbq_arena a; bbq_arena_init(&a, 1 << 18); emit_wasm_ctx mod = {0};
        bool pb = assemble_plugin(&a, "class T { static int f(){ int goto = 5; return goto; } }", &mod);
        CHECK(!pb, "§3.9: `goto` is reserved-and-unused, so it is still not an identifier");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* (1) multi-function + internal static call: sum() calls add() twice. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a,
            "class T {"
            "  static int add(int x, int y){ return x + y; }"
            "  static int sum(){ return add(3, 5) + add(10, 20); }"
            "}", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.sum", NULL, 0, res, 1);
        CHECK(st == EXEC_OK, "sum: validated + instantiated + called (no trap)");
        CHECK(st == EXEC_OK && res[0].of.i32 == 38, "sum: add(3,5)+add(10,20) == 38");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (1s) javelina.simd: explicit SIMD intrinsics lower INLINE to v128 opcodes —
     * never imports (the import path failing to instantiate is exactly what this
     * pins: no harness echo exists for javelina.simd, by design). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a,
            "class T {"
            "  static int lanes(){"
            "    V128 x = I32x4.splat(7);"
            "    V128 y = I32x4.splat(35);"
            "    V128 s = I32x4.add(x, y);"
            "    return I32x4.extract_lane(s, 2);"
            "  }"
            "}", &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.lanes", NULL, 0, res, 1);
        CHECK(st == EXEC_OK, "simd: splat/add/extract module validates + runs (no import)");
        CHECK(st == EXEC_OK && res[0].of.i32 == 42, "simd: i32x4 splat(7)+splat(35) lane 2 == 42");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (1s-b..e) javelina.simd: one executed case per remaining family —
     * Un/Shift/TestI, Tern/Const/Replace, the L/F/D splat/replace/extract
     * quartets, and Shuffle. Lane-exact expectations, computed by hand. */
    {
        struct { const char* src; int32_t want; const char* label; } sc[] = {
          { "class T { static int f(){"
            "  int a = I32x4.extract_lane(I32x4.neg(I32x4.splat(5)), 1);"      /* Un: -5 */
            "  int b = I32x4.extract_lane(I32x4.shl(I32x4.splat(3), 2), 0);"   /* Shift: 12 */
            "  int c = I32x4.all_true(I32x4.splat(1));"                        /* TestI: 1 */
            "  int d = V128.any_true(I32x4.splat(0));"                         /* TestI: 0 */
            "  return a + b + c + d; } }", 8,
            "simd: Un/Shift/TestI (neg, shl, all_true, any_true) == 8" },
          { "class T { static int f(){"
            "  V128 m = V128.const_(0xFFL, 0L);"                               /* Const: lane0 mask 0xFF */
            "  V128 s = V128.bitselect(I32x4.splat(0xF0), I32x4.splat(0x0F), m);" /* Tern: lane0 = 0xF0 */
            "  V128 r = I32x4.replace_lane(s, 7, 3);"                          /* ReplaceI */
            "  return I32x4.extract_lane(s, 0) + I32x4.extract_lane(r, 3); } }", 247,
            "simd: Tern/Const/ReplaceI (bitselect mask + replace) == 240+7" },
          { "class T { static int f(){"
            "  long l = I64x2.extract_lane(I64x2.replace_lane(I64x2.splat(9L), 20L, 0), 0);"
            "  float g = F32x4.extract_lane(F32x4.replace_lane(F32x4.splat(1.0f), 2.5f, 1), 1);"
            "  double d = F64x2.extract_lane(F64x2.replace_lane(F64x2.splat(0.25), 8.5, 1), 1);"
            "  return (int)l + (int)(g + g) + (int)d; } }", 33,
            "simd: L/F/D splat/replace/extract == 20+5+8" },
          { "class T { static int f(){"
            "  V128 x = I32x4.splat(77);"
            "  V128 y = I32x4.splat(88);"
            "  V128 id = I8x16.shuffle(x, y, 0x0706050403020100L, 0x0F0E0D0C0B0A0908L);"
            "  return I32x4.extract_lane(id, 3); } }", 77,
            "simd: Shuffle identity mask reproduces the first operand" },
        };
        for (size_t i = 0; i < sizeof sc / sizeof sc[0]; i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            assemble(&a, sc[i].src, &mod);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1);
            CHECK(st == EXEC_OK && res[0].of.i32 == sc[i].want, sc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }
    /* (1s-V4) the v128 VM-hardening battery: every seam where a v128 crosses a
     * representation boundary, run under BOTH tiers, lane-exact. Each probe's
     * `mix` weights the four i32 lanes 1/2/3/4, so a 64-bit truncation (the
     * `any_t` carrier risk: {s8 bits; u1 kind} cannot hold 128 bits) zeroes
     * lanes 2/3 and CHANGES the result — the assert reads the whole vector.
     * On mismatch the harness prints got/want per tier, not a boolean. */
    {
        static const char* MIX =
            "  static int mix(V128 v){ return I32x4.extract_lane(v,0)"
            " + 2*I32x4.extract_lane(v,1) + 3*I32x4.extract_lane(v,2)"
            " + 4*I32x4.extract_lane(v,3); }";
        struct { const char* pre; const char* body; int32_t want; const char* label; } pv[] = {
          { "", "V128 r = h(I32x4.splat(41)); return mix(r);", 420,
            "v128 param+result across a static call (locals seam)" },
          { "", "P p = new P(); int z = mix(p.v);"
                " p.v = I32x4.splat(9); return z + mix(p.v);", 90,
            "v128 GC STRUCT FIELD: default init (zeros) + set/get" },
          { "", "V128[] a = new V128[3]; a[1] = I32x4.splat(7);"
                " return mix(a[1]) + I32x4.extract_lane(a[0], 3);", 70,
            "V128[] element: array.new default + set/get (lane 3 of the default read)" },
          { "", "Object o = new V128[3]; V128[] a = (V128[]) o; a[1] = I32x4.splat(7);"
                " return mix(a[1]) + I32x4.extract_lane(a[0], 3);", 70,
            "V128[] through Object and back: (V128[]) casts to the V128Array overlay" },
          { "", "Object o = new V128[3];"
                " return (o instanceof V128[] ? 70 : 0) + (o instanceof int[] ? 5 : 0);", 70,
            "instanceof V128[] is precise: true for V128[], false for int[]" },
          { "", "int z = mix(G.g); G.g = I32x4.splat(5); return z + mix(G.g);", 50,
            "v128 MODULE GLOBAL: v128.const init + mutable set/get" },
          { "", "B b = new D(); return mix(b.m(I32x4.splat(10)));", 110,
            "v128 through virtual dispatch (call_ref seam)" },
          { "", "long v = I64x2.extract_lane(I64x2.replace_lane("
                "I64x2.splat(1L), 0x123456789L, 1), 1);"
                " return (int)(v >> 32) + (int)v;", 591751050,
            "i64x2 HIGH lane holds all 64 bits (the truncation kill-shot)" },
        };
        for (size_t i = 0; i < sizeof pv / sizeof pv[0]; i++) {
            char src[2048];
            snprintf(src, sizeof src,
                "class P { V128 v; }"
                "class G { static V128 g; }"
                "class B { V128 m(V128 x){ return x; } }"
                "class D extends B { V128 m(V128 x){ return I32x4.add(x, I32x4.splat(1)); } }"
                "class T {%s"
                "  static V128 h(V128 v){ return I32x4.add(v, I32x4.splat(1)); }"
                "  static int f(){ %s } }", MIX, pv[i].body);
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            assemble(&a, src, &mod);
            int32_t got[2] = {0, 0};
            exec_status sts[2];
            for (int tier = 0; tier < 2; tier++) {
                g_exec_jit = tier;
                wasm_val_t res[1] = { WASM_INIT_VAL };
                sts[tier] = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1);
                got[tier] = res[0].of.i32;
            }
            g_exec_jit = 0;
            bool ok = sts[0] == EXEC_OK && sts[1] == EXEC_OK &&
                      got[0] == pv[i].want && got[1] == pv[i].want;
            if (!ok)
                printf("  FAIL-DETAIL %s: want %d, interp(st=%d) %d, jit(st=%d) %d\n",
                       pv[i].label, pv[i].want, sts[0], got[0], sts[1], got[1]);
            CHECK(ok, pv[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }
    /* (1s-V5) linear-memory SIMD: ALL 22 memarg/memlane ops round-trip through
     * the I/O-floor staging memory, both tiers, lane-exact. Every probe PRIMES
     * the memory itself (zero-extension is proven against a pre-filled NONZERO
     * pattern; lane ops assert the targeted lane AND a preserved neighbor;
     * partial stores assert the written bytes AND an adjacent untouched byte). */
    {
        static const char* MIX =
            "  static int mix(V128 v){ return I32x4.extract_lane(v,0)"
            " + 2*I32x4.extract_lane(v,1) + 3*I32x4.extract_lane(v,2)"
            " + 4*I32x4.extract_lane(v,3); }";
        struct { const char* body; int32_t want; const char* label; } mv[] = {
          /* v128.store + v128.load: 16 exact bytes there and back */
          { "Mem.v128_store(64, V128.const_(0x0000000200000001L, 0x0000000400000003L));"
            " return mix(Mem.v128_load(64));", 30,
            "v128.store + v128.load: lanes 1,2,3,4 round-trip" },
          /* load8x8: bytes 01 80 7F FF 02 FE 00 40 -> i16x8, signed vs unsigned */
          { "Mem.v128_store(64, V128.const_(0x4000FE02FF7F8001L, 0L));"
            " V128 e = Mem.v128_load8x8_s(64);"
            " return I16x8.extract_lane_s(e,1) + 3*I16x8.extract_lane_s(e,3)"
            "      + I16x8.extract_lane_s(e,7);", -67,
            "v128.load8x8_s: byte 0x80 extends to -128" },
          { "Mem.v128_store(64, V128.const_(0x4000FE02FF7F8001L, 0L));"
            " V128 e = Mem.v128_load8x8_u(64);"
            " return I16x8.extract_lane_u(e,1) + 3*I16x8.extract_lane_u(e,3)"
            "      + I16x8.extract_lane_u(e,7);", 957,
            "v128.load8x8_u: byte 0x80 extends to 128" },
          /* load16x4: words 0001 8000 7FFF FFFF -> i32x4 */
          { "Mem.v128_store(64, V128.const_(0xFFFF7FFF80000001L, 0L));"
            " return mix(Mem.v128_load16x4_s(64));", 32762,
            "v128.load16x4_s: word 0x8000 extends to -32768" },
          { "Mem.v128_store(64, V128.const_(0xFFFF7FFF80000001L, 0L));"
            " return mix(Mem.v128_load16x4_u(64));", 425978,
            "v128.load16x4_u: word 0x8000 extends to 32768" },
          /* load32x2: words32 [5, -7] -> i64x2 */
          { "Mem.v128_store(64, V128.const_(0xFFFFFFF900000005L, 0L));"
            " V128 e = Mem.v128_load32x2_s(64);"
            " return (int)I64x2.extract_lane(e,0) + (int)I64x2.extract_lane(e,1)"
            "      + (int)(I64x2.extract_lane(e,1) >> 32);", -3,
            "v128.load32x2_s: word -7 sign-extends (high word -1)" },
          { "Mem.v128_store(64, V128.const_(0xFFFFFFF900000005L, 0L));"
            " V128 e = Mem.v128_load32x2_u(64);"
            " return (int)I64x2.extract_lane(e,0) + (int)I64x2.extract_lane(e,1)"
            "      + (int)(I64x2.extract_lane(e,1) >> 32);", -2,
            "v128.load32x2_u: word -7 zero-extends (high word 0)" },
          /* loadN_splat */
          { "Mem.v128_store(64, V128.const_(0x2AL, 0L));"
            " V128 e = Mem.v128_load8_splat(64);"
            " return I8x16.extract_lane_u(e,0) + 2*I8x16.extract_lane_u(e,15);", 126,
            "v128.load8_splat: byte 42 in lanes 0 and 15" },
          { "Mem.v128_store(64, V128.const_(0xABCDL, 0L));"
            " V128 e = Mem.v128_load16_splat(64);"
            " return I16x8.extract_lane_u(e,0) + I16x8.extract_lane_u(e,7);", 87962,
            "v128.load16_splat: word 0xABCD in lanes 0 and 7" },
          { "Mem.v128_store(64, V128.const_(0x01020304L, 0L));"
            " return mix(Mem.v128_load32_splat(64));", 169090600,
            "v128.load32_splat: 0x01020304 in all four lanes" },
          { "Mem.v128_store(64, V128.const_(0x0000000123456789L, 0L));"
            " long v = I64x2.extract_lane(Mem.v128_load64_splat(64), 1);"
            " return (int)(v >> 32) + (int)v;", 591751050,
            "v128.load64_splat: all 64 bits reach lane 1" },
          /* loadN_zero — memory PRE-FILLED nonzero, so the zeroing is proven */
          { "Mem.v128_store(64, V128.const_(0x0000006F0000004DL, 0x0000000400000003L));"
            " return mix(Mem.v128_load32_zero(64));", 77,
            "v128.load32_zero: lanes 1-3 zeroed (memory was nonzero there)" },
          { "Mem.v128_store(64, V128.const_(0x0000006F0000004DL, 0x0000000400000003L));"
            " return mix(Mem.v128_load64_zero(64));", 299,
            "v128.load64_zero: lanes 2-3 zeroed (memory was nonzero there)" },
          /* loadN_lane — targeted lane replaced, neighbors preserved */
          { "Mem.v128_store(64, V128.const_(0x0000000B00000016L, 0L));"
            " V128 e = Mem.v128_load8_lane(64, I32x4.splat(0), 5);"
            " return I8x16.extract_lane_u(e,5)*10 + I8x16.extract_lane_u(e,6);", 220,
            "v128.load8_lane: byte 22 into lane 5, lane 6 preserved" },
          { "Mem.v128_store(64, V128.const_(0x0000000B00000016L, 0L));"
            " V128 e = Mem.v128_load16_lane(64, I16x8.splat(1), 3);"
            " return I16x8.extract_lane_u(e,3)*100 + I16x8.extract_lane_u(e,2)"
            "      + I16x8.extract_lane_u(e,4);", 2202,
            "v128.load16_lane: word 22 into lane 3, lanes 2/4 preserved" },
          { "Mem.v128_store(64, V128.const_(0x0000000B00000016L, 0L));"
            " return mix(Mem.v128_load32_lane(64, I32x4.splat(3), 2));", 87,
            "v128.load32_lane: word 22 into lane 2, others stay 3" },
          { "Mem.v128_store(64, V128.const_(0x0000000123456789L, 0L));"
            " V128 e = Mem.v128_load64_lane(64, I64x2.splat(7L), 1);"
            " long v = I64x2.extract_lane(e,1);"
            " return (int)(v >> 32) + (int)v + (int)I64x2.extract_lane(e,0);", 591751057,
            "v128.load64_lane: all 64 bits into lane 1, lane 0 stays 7" },
          /* storeN_lane — the written bytes AND an adjacent untouched byte */
          { "Mem.v128_store(64, V128.const_(0x0807060504030201L, 0x100F0E0D0C0B0A09L));"
            " Mem.v128_store8_lane(64, I8x16.splat(0x55), 0);"
            " V128 r = Mem.v128_load(64);"
            " return I8x16.extract_lane_u(r,0)*1000 + I8x16.extract_lane_u(r,1);", 85002,
            "v128.store8_lane: ONE byte written, next byte untouched" },
          { "Mem.v128_store(64, V128.const_(0x0807060504030201L, 0x100F0E0D0C0B0A09L));"
            " Mem.v128_store16_lane(66, I16x8.splat(0x6666), 2);"
            " V128 r = Mem.v128_load(64);"
            " return I8x16.extract_lane_u(r,2)*10000 + I8x16.extract_lane_u(r,3)*100"
            "      + I8x16.extract_lane_u(r,4);", 1030205,
            "v128.store16_lane: two bytes written, byte 4 untouched" },
          { "Mem.v128_store(64, V128.const_(0x0807060504030201L, 0x100F0E0D0C0B0A09L));"
            " Mem.v128_store32_lane(72, I32x4.splat(0x11223344), 1);"
            " V128 r = Mem.v128_load(64);"
            " return I32x4.extract_lane(r,2) + I8x16.extract_lane_u(r,12);", 287454033,
            "v128.store32_lane: four bytes written, byte 12 untouched" },
          { "Mem.v128_store(64, V128.const_(0x0807060504030201L, 0x100F0E0D0C0B0A09L));"
            " Mem.v128_store64_lane(64, I64x2.splat(0x123456789L), 0);"
            " V128 r = Mem.v128_load(64);"
            " long v = I64x2.extract_lane(r,0);"
            " return (int)(v >> 32) + (int)v + I8x16.extract_lane_u(r,8);", 591751059,
            "v128.store64_lane: eight bytes written, byte 8 untouched" },
          /* v128.store adjacency: the 16th byte lands, the 17th does not */
          { "Mem.v128_store(80, V128.const_(0L, 0L));"
            " Mem.v128_store(64, V128.const_(0x0807060504030201L, 0x100F0E0D0C0B0A09L));"
            " V128 r = Mem.v128_load(65);"
            " return I8x16.extract_lane_u(r,14)*100 + I8x16.extract_lane_u(r,15);", 1600,
            "v128.store writes exactly 16 bytes (unaligned re-read straddles the edge)" },
          /* ── the scalar family: sign/zero extension + width exactness ── */
          { "Mem.i32_store(64, 0xCAFE9081);"
            " return Mem.i32_load8_s(64) + Mem.i32_load8_u(64)*1000"
            "      + Mem.i32_load16_s(64);", 100330,   /* -127 + 129000 - 28543 */
            "i32 store + load8_s/-u/load16_s: byte 0x81 is -127 / 129, word 0x9081 is -28543" },
          { "Mem.i64_store(64, 0x8000000180000002L);"
            " return (int)Mem.i64_load32_s(64) + (int)(Mem.i64_load32_u(64) >> 31)"
            "      + (int)(Mem.i64_load(64) >> 62);", -2147483647,  /* -2147483646 + 1 - 2 */
            "i64 store + load32_s/-u/full: sign vs zero extension of word 0x80000002" },
          { "Mem.f32_store(64, 1.5f); Mem.f64_store(72, 2.25);"
            " return (int)(Mem.f32_load(64)*4.0f) + (int)(Mem.f64_load(72)*4.0);", 15,
            "f32/f64 store + load round-trip exact" },
          /* Delta-form: the corpus shares the long-lived jre instance, so the
           * interp tier's grow PERSISTS into the JIT tier — absolute sizes
           * differ per tier, the deltas cannot. */
          { "int before = Mem.memory_size(); int old = Mem.memory_grow(1);"
            " return (old - before)*10 + (Mem.memory_size() - before);", 1,
            "memory_size/grow: grow(1) returns the old size, size advances by 1" },
          { "int before = Mem.memory_size(); Mem.memory_grow(1);"
            " return Mem.memory_size() - before;", 1,
            "STATEMENT-position memory_grow executes (an Effectful value is "
            "never dropped by effect delivery)" },
          { "Mem.memory_fill(64, 0x5A, 3); Mem.i32_store8(67, 7);"
            " return Mem.i32_load8_u(64) + Mem.i32_load8_u(66)*10 + Mem.i32_load8_u(67);", 997,
            "memory.fill: exactly len bytes, the next byte untouched" },
          { "Mem.memory_fill(64, 9, 4); Mem.memory_copy(80, 64, 4);"
            " return Mem.i32_load8_u(80) + Mem.i32_load8_u(83)*10 + Mem.i32_load8_u(84)*100;", 99,
            "memory.copy: exactly len bytes copied, the byte after untouched" },
          /* ── OOB is a CATCHABLE Java exception, never a VM trap ── */
          { "try { int x = Mem.i32_load(-4); return x; }"
            " catch (IndexOutOfBoundsException e) { return 42; }", 42,
            "OOB i32_load(-4) throws catchable IndexOutOfBoundsException (no trap)" },
          { "int lim = Mem.memory_size() * 65536;"
            " try { Mem.v128_store(lim - 15, I32x4.splat(1)); return 1; }"
            " catch (IndexOutOfBoundsException e) { return 43; }", 43,
            "v128_store straddling the memory end throws (16-byte span checked)" },
          { "int lim = Mem.memory_size() * 65536;"
            " Mem.i32_store8(lim - 1, 5);"
            " return Mem.i32_load8_u(lim - 1) + 60;", 65,
            "the LAST byte is in bounds (the guard is exact, not off-by-one)" },
          { "try { Mem.memory_copy(64, 128, -1); return 1; }"
            " catch (IndexOutOfBoundsException e) { return 44; }", 44,
            "memory.copy with a negative length throws (no wrap-around)" },
          /* ── the E8.3 bounce helpers (plain Java over the ops) ── */
          { "byte[] s = new byte[4]; s[0]=1; s[1]=2; s[2]=3; s[3]=4;"
            " Mem.copyIn(s, 0, 4, 64);"
            " byte[] d = new byte[4]; Mem.copyOut(64, d, 0, 4);"
            " return d[0] + d[1]*10 + d[2]*100 + d[3]*1000;", 4321,
            "byte[] copyIn/copyOut round-trips through the memory" },
          { "V128[] s = new V128[2]; s[0] = I32x4.splat(3); s[1] = I32x4.splat(7);"
            " Mem.copyIn(s, 0, 2, 64);"
            " V128[] d = new V128[2]; Mem.copyOut(64, d, 0, 2);"
            " return mix(d[0]) + mix(d[1]);", 100,
            "V128[] copyIn/copyOut round-trips lane-exact (30 + 70)" },
        };
        for (size_t i = 0; i < sizeof mv / sizeof mv[0]; i++) {
            char src[4096];
            snprintf(src, sizeof src,
                "class T {%s"
                "  static int f(){ %s } }", MIX, mv[i].body);
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            assemble(&a, src, &mod);
            int32_t got[2] = {0, 0};
            exec_status sts[2];
            for (int tier = 0; tier < 2; tier++) {
                g_exec_jit = tier;
                wasm_val_t res[1] = { WASM_INIT_VAL };
                sts[tier] = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1);
                got[tier] = res[0].of.i32;
            }
            g_exec_jit = 0;
            bool ok = sts[0] == EXEC_OK && sts[1] == EXEC_OK &&
                      got[0] == mv[i].want && got[1] == mv[i].want;
            if (!ok)
                printf("  FAIL-DETAIL %s: want %d, interp(st=%d) %d, jit(st=%d) %d\n",
                       mv[i].label, mv[i].want, sts[0], got[0], sts[1], got[1]);
            CHECK(ok, mv[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }
    /* (2) i32 args. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a, "class T { static int add(int x, int y){ return x + y; } }", &mod);
        wasm_val_t args[2] = { WASM_I32_VAL(3), WASM_I32_VAL(5) };
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.add", args, 2, res, 1);
        CHECK(st == EXEC_OK && res[0].of.i32 == 8, "i32: add(3,5) == 8");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (3) i64. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a, "class T { static long dbl(long a){ return a + a; } }", &mod);
        wasm_val_t args[1] = { WASM_I64_VAL(21) };
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.dbl", args, 1, res, 1);
        CHECK(st == EXEC_OK && res[0].of.i64 == 42, "i64: dbl(21) == 42");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (4) f64. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a, "class T { static double add(double a, double b){ return a + b; } }", &mod);
        wasm_val_t args[2] = { WASM_F64_VAL(1.5), WASM_F64_VAL(2.25) };
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.add", args, 2, res, 1);
        CHECK(st == EXEC_OK && fabs(res[0].of.f64 - 3.75) < 1e-9, "f64: add(1.5,2.25) == 3.75");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (5) f32. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a, "class T { static float add(float a, float b){ return a + b; } }", &mod);
        wasm_val_t args[2] = { WASM_F32_VAL(1.5f), WASM_F32_VAL(2.25f) };
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.add", args, 2, res, 1);
        CHECK(st == EXEC_OK && fabsf(res[0].of.f32 - 3.75f) < 1e-6f, "f32: add(1.5,2.25) == 3.75");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* (5b) §5.6.1 unary numeric promotion — `-` on a long/double keeps the wide
     *      width (a regression forced it to int, so `(int)(-(long%n))` spilled
     *      an i64 value into an i32-typed temp and the module failed validation). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a,
            "class T {"
            "  static long negL(long x){ return -x; }"                 /* -(long) is long */
            "  static double negD(double x){ return -x; }"             /* -(double) is double */
            "  static int digit(long x, int r){ return (int)(-(x % r)); }"  /* the toString(radix) shape */
            "}", &mod);
        CHECK(ok, "unary-promo module assembles + validates");
        if (ok) {
            wasm_val_t a1[1] = { WASM_I64_VAL(255) };
            wasm_val_t r1[1] = { WASM_INIT_VAL };
            exec_status s1 = exec_call(mod.code, bbq_vec_len(mod.code), "T.negL", a1, 1, r1, 1);
            CHECK(s1 == EXEC_OK && r1[0].of.i64 == -255, "-(255L) == -255 (long width kept)");

            wasm_val_t a2[1] = { WASM_F64_VAL(2.5) };
            wasm_val_t r2[1] = { WASM_INIT_VAL };
            exec_status s2 = exec_call(mod.code, bbq_vec_len(mod.code), "T.negD", a2, 1, r2, 1);
            CHECK(s2 == EXEC_OK && fabs(r2[0].of.f64 + 2.5) < 1e-9, "-(2.5) == -2.5 (double width kept)");

            wasm_val_t a3[2] = { WASM_I64_VAL(255), WASM_I32_VAL(16) };
            wasm_val_t r3[1] = { WASM_INIT_VAL };
            exec_status s3 = exec_call(mod.code, bbq_vec_len(mod.code), "T.digit", a3, 2, r3, 1);
            CHECK(s3 == EXEC_OK && r3[0].of.i32 == -(255 % 16), "(int)(-(255L%16)) == -15");
        }
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* §15.25 conditional numeric promotion: a `long ? : int` (mixed arms) promotes
     * BOTH arms to long — the int arm must widen (it was emitting i32 into the i64
     * result slot → validation "type mismatch"; surfaced by the Dragon4 overlay). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a,
            "class T {"
            "  static long selL(long a, int flag){ return flag != 0 ? a : 0; }"        /* int arm 0 → long */
            "  static long selD(double d, int flag){ return (long)(flag != 0 ? d : 0); }"/* int arm 0 → double */
            "}", &mod);
        CHECK(ok, "mixed-type ternary module assembles + validates");
        if (ok) {
            wasm_val_t on[2]  = { WASM_I64_VAL(5000000000LL), WASM_I32_VAL(1) };
            wasm_val_t off[2] = { WASM_I64_VAL(5000000000LL), WASM_I32_VAL(0) };
            wasm_val_t r[1] = { WASM_INIT_VAL };
            exec_status s1 = exec_call(mod.code, bbq_vec_len(mod.code), "T.selL", on, 2, r, 1);
            CHECK(s1 == EXEC_OK && r[0].of.i64 == 5000000000LL, "(flag?longA:0) picks longA (>2^32)");
            exec_status s2 = exec_call(mod.code, bbq_vec_len(mod.code), "T.selL", off, 2, r, 1);
            CHECK(s2 == EXEC_OK && r[0].of.i64 == 0, "(flag?longA:0) int arm widened to 0L");
            wasm_val_t da[2] = { WASM_F64_VAL(2.5), WASM_I32_VAL(0) };
            exec_status s3 = exec_call(mod.code, bbq_vec_len(mod.code), "T.selD", da, 2, r, 1);
            CHECK(s3 == EXEC_OK && r[0].of.i64 == 0, "(flag?double:0) int arm widened to 0.0");
        }
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* (6) tail recursion: `return sumTo(...)` is a tail call → return_call (0x12),
     *     O(1) stack. sumTo(100,0)=5050. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a,
            "class T {"
            "  static int sumTo(int n, int acc){ if (n == 0) return acc; return sumTo(n - 1, acc + n); }"
            "  static int run(){ return sumTo(100, 0); }"
            "}", &mod);
        CHECK(ok, "tailrec: assembled");
        const uint8_t rc[] = { 0x12 };   /* return_call present */
        CHECK(ok && contains(mod.code, (int)bbq_vec_len(mod.code), rc, 1),
              "tailrec: emits return_call (0x12)");
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.run", NULL, 0, res, 1);
        CHECK(st == EXEC_OK && res[0].of.i32 == 5050, "tailrec: sumTo(100,0) == 5050");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (7) non-tail recursion: `return n * fac(n-1)` — the call is under Mul, NOT a
     *     tail call → plain call+return. fac(5)=120. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a,
            "class T { static int fac(int n){ if (n <= 1) return 1; return n * fac(n - 1); } }", &mod);
        wasm_val_t args[1] = { WASM_I32_VAL(5) };
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.fac", args, 1, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 120, "rec: fac(5) == 120");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* (8) conversions across widths, executed. */
    {
        struct { const char* src; const char* fn; int argkind; int64_t ia; double da;
                 int reskind; int64_t iexp; double dexp; const char* label; } cs[] = {
          { "class T { static long f(int x){ return x; } }",        "T.f", 0, 7, 0, 1, 7, 0, "I2L (implicit): (long)7 == 7" },
          { "class T { static int f(long x){ return (int)x; } }",   "T.f", 1, 7, 0, 0, 7, 0, "L2I (cast): (int)7L == 7" },
          { "class T { static double f(int x){ return x; } }",      "T.f", 0, 3, 0, 2, 0, 3.0, "I2D (implicit): (double)3 == 3.0" },
          { "class T { static int f(double x){ return (int)x; } }", "T.f", 2, 0, 3.9, 0, 3, 0, "D2I (cast): (int)3.9 == 3" },
          { "class T { static float f(int x){ return x; } }",       "T.f", 0, 5, 0, 3, 0, 5.0, "I2F (implicit): (float)5 == 5.0" },
          { "class T { static double f(float x){ return x; } }",    "T.f", 3, 0, 2.5, 2, 0, 2.5, "F2D (implicit): (double)2.5f == 2.5" },
          { "class T { static double f(long x){ return x; } }",     "T.f", 1, 9, 0, 2, 0, 9.0, "L2D (implicit): (double)9L == 9.0" },
        };
        for (int i = 0; i < (int)(sizeof cs / sizeof cs[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cs[i].src, &mod);
            wasm_val_t arg;
            if      (cs[i].argkind == 0) arg = (wasm_val_t)WASM_I32_VAL((int32_t)cs[i].ia);
            else if (cs[i].argkind == 1) arg = (wasm_val_t)WASM_I64_VAL(cs[i].ia);
            else if (cs[i].argkind == 3) arg = (wasm_val_t)WASM_F32_VAL((float)cs[i].da);
            else                         arg = (wasm_val_t)WASM_F64_VAL(cs[i].da);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), cs[i].fn, &arg, 1, res, 1);
            int good = ok && st == EXEC_OK;
            if (good) {
                if      (cs[i].reskind == 0) good = res[0].of.i32 == (int32_t)cs[i].iexp;
                else if (cs[i].reskind == 1) good = res[0].of.i64 == cs[i].iexp;
                else if (cs[i].reskind == 2) good = res[0].of.f64 == cs[i].dexp;
                else                         good = res[0].of.f32 == (float)cs[i].dexp;
            }
            CHECK(good, cs[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E1: control-flow families, executed (one i32 arg → asserted result) ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } cf[] = {
          { "class T { static int f(int x){ if (x < 0) return 0 - x; else return x; } }",
            -5, 5, "if/else: abs(-5)=5" },
          { "class T { static int f(int x){ if (x < 0) return 0 - x; else return x; } }",
            7, 7, "if/else: abs(7)=7" },
          { "class T { static int f(int n){ int s=0; while (n>0){ s=s+n; n=n-1; } return s; } }",
            5, 15, "while: sum 1..5=15" },
          { "class T { static int f(int n){ int s=0; for(int i=1;i<=n;i=i+1){ s=s+i; } return s; } }",
            5, 15, "for: sum 1..5=15" },
          { "class T { static int f(int n){ int c=0; do { c=c+1; n=n-1; } while(n>0); return c; } }",
            3, 3, "do-while: count=3" },
          { "class T { static int f(int n){ int c=0; do { c=c+1; n=n-1; } while(n>0); return c; } }",
            0, 1, "do-while: body runs once at n=0" },
          { "class T { static int f(int x){ switch(x){ case 0: return 10; case 1: return 20; default: return 30; } } }",
            1, 20, "switch: case 1=20" },
          { "class T { static int f(int x){ switch(x){ case 0: return 10; case 1: return 20; default: return 30; } } }",
            9, 30, "switch: default=30" },
          { "class T { static int f(int n){ int s=0; for(int i=0;i<100;i=i+1){ if(i==n) break; s=s+i; } return s; } }",
            3, 3, "break: sum 0..2=3" },
          { "class T { static int f(int n){ int s=0; for(int i=0;i<n;i=i+1){ if(i-(i/2)*2==1) continue; s=s+i; } return s; } }",
            6, 6, "continue: evens 0+2+4=6" },
          { "class T { static int f(int n){ int s=0; outer: for(int i=0;i<n;i=i+1){ for(int j=0;j<n;j=j+1){ if(i+j==n) break outer; s=s+1; } } return s; } }",
            2, 3, "labeled break: =3" },
          { "class T { static int f(int n){ int s=0; for(int i=0;i<n;i++){ s=s+i; } return s; } }",
            5, 10, "for with i++ update: sum 0..4=10" },
          { "class T { static int f(int n){ int s=0; int i=n; while(i>0){ i--; s=s+i; } return s; } }",
            4, 6, "while with i-- body: sum 3..0=6" },
          { "class T { static int f(int n){ int s=0; outer: for(int i=0;i<n;i=i+1){ for(int j=0;j<n;j=j+1){ if(j==1) continue outer; s=s+1; } } return s; } }",
            3, 3, "labeled continue: =3" },
          /* All four inc/dec operators in BOTH value-dropped (statement) and
           * value-used contexts. Value-dropped → Inc node; value-used → the
           * old-value spill + Inc delivery path. */
          { "class T { static int f(int x){ int i=x; i++; return i; } }",     5, 6, "stmt i++ == 6" },
          { "class T { static int f(int x){ int i=x; ++i; return i; } }",     5, 6, "stmt ++i == 6" },
          { "class T { static int f(int x){ int i=x; i--; return i; } }",     5, 4, "stmt i-- == 4" },
          { "class T { static int f(int x){ int i=x; --i; return i; } }",     5, 4, "stmt --i == 4" },
          { "class T { static int f(int x){ int i=x; int j=i++; return j*100+i; } }", 5, 506, "value i++ == 506" },
          { "class T { static int f(int x){ int i=x; int j=++i; return j*100+i; } }", 5, 606, "value ++i == 606" },
          { "class T { static int f(int x){ int i=x; int j=i--; return j*100+i; } }", 5, 504, "value i-- == 504" },
          { "class T { static int f(int x){ int i=x; int j=--i; return j*100+i; } }", 5, 404, "value --i == 404" },
          { "class T { static int f(int x){ long a=x; a++; a--; a--; return (int)a; } }", 9, 8, "long ++/-- == 8" },
        };
        for (int i = 0; i < (int)(sizeof cf / sizeof cf[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cf[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(cf[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == cf[i].want, cf[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E2: objects — new, instance field write/read, executed ── */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        const char* src =
            "class Box { int v; }"
            "class T { static int f(int x){ Box b = new Box(); b.v = x; return b.v; } }";
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(42);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 42, "object: new Box; b.v=x; return b.v == 42");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── E6 regression: reference-array field STORE via a ctor param spill ──
     * `this.v = a` where v is char[] — the ddcg spills the param to a temp whose
     * StoreLocal must carry the array ref descriptor, else the temp defaults to
     * (ref null Object) and struct.set rejects (Object ⊄ char[]). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        const char* src =
            "class B {"
            "  char[] v;"
            "  B(char[] a){ this.v = a; }"
            "  static int f(){"
            "    char[] a = new char[3]; a[0]=65; a[1]=66; a[2]=67;"
            "    B b = new B(a);"
            "    return b.v.length * 100 + b.v[2];"
            "  }"
            "}";
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "B.f", NULL, 0, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 367,
              "char[] field store via ctor (this.v=a): len*100+v[2] == 367");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* Same, but a reference-CLASS array (Box[]) field — the descriptor must carry
     * the element class, not just primitive arrays. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        const char* src =
            "class Box { int v; }"
            "class H {"
            "  Box[] items;"
            "  H(Box[] xs){ this.items = xs; }"
            "  static int f(){"
            "    Box[] xs = new Box[2]; xs[0] = new Box(); xs[0].v = 41;"
            "    H h = new H(xs);"
            "    return h.items[0].v + h.items.length;"
            "  }"
            "}";
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "H.f", NULL, 0, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 43,
              "Box[] field store via ctor (this.items=xs): items[0].v + len == 43");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── E6: String GC overlay — new String(char[]) ctor stores the backing
     * char[] (the unblocked ref-array field store), and the COMPILED length()/
     * charAt() overlays read it. Exercised end-to-end through the VM. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        const char* src =
            "class T {"
            "  static int f(){"
            "    char[] c = new char[2]; c[0]=72; c[1]=105;"   /* \"Hi\" */
            "    String s = new String(c);"
            "    return s.length() * 1000 + s.charAt(1);"       /* 2*1000 + 105 = 2105 */
            "  }"
            "}";
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 2105,
              "new String(char[]).length()*1000 + charAt(1) == 2105");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── E2: arrays — new, store, load, length, executed ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ar[] = {
          { "class T { static int f(int x){ int[] a = new int[3]; a[0]=x; a[1]=x+1; return a[0]+a[1]; } }",
            10, 21, "int[]: a[0]=x; a[1]=x+1; sum == 21" },
          { "class T { static int f(int n){ int[] a = new int[n]; return a.length; } }",
            5, 5, "int[]: new int[n].length == n" },
          { "class T { static int f(int x){ int[] a = new int[4]; int s=0; for(int i=0;i<4;i=i+1){ a[i]=i*x; } "
            "for(int i=0;i<4;i=i+1){ s=s+a[i]; } return s; } }",
            3, 18, "int[]: fill a[i]=i*x then sum == 18" },
          { "class T { static int f(int n){ int[] a = new int[n]; a[n-1]=n; return a[n-1]; } }",
            3000, 3000, "int[]: int-typed size+index (a[2999]) == 3000" },
          /* §10.6: an int[] initializer's ELEMENT type is int (the declared component
           * type), NOT the "narrowest that fits the constants". If the
           * backing width were inferred from {1,2,3} (all fit byte), a later wide store
           * would truncate. Mutate past byte/short range and read it back. */
          { "class T { static int f(int x){ int[] a = {1,2,3}; a[0]=x; return a[0]; } }",
            100000, 100000, "int[] initializer stays int-width: a[0]=100000 round-trips" },
          /* §10.6 recursively: a NESTED int[][] initializer allocates the inner arrays as
           * int[] (ref elements) and keeps int width — a[0][0]=100000 round-trips. */
          { "class T { static int f(int x){ int[][] a = {{1,2,3}}; a[0][0]=x; return a[0][0]; } }",
            100000, 100000, "int[][] nested initializer: inner int[] created, a[0][0] round-trips" },
          /* §10.6 reference-element initializer: Box[] a = {new Box()} allocates a ref array
           * of the concrete element type (NewRefArray), so a[0].v reads back. */
          { "class Box { int v; } class T { static int f(int x){ Box[] a = {new Box(), new Box()}; a[1].v=x; return a[1].v + a.length; } }",
            40, 42, "Box[] reference-element initializer: a[1].v=x, +length == 42" },
        };
        for (int i = 0; i < (int)(sizeof ar / sizeof ar[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ar[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ar[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ar[i].want, ar[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.9.16 / §20.10.15: Float/Double.toString — the shortest decimal that
     *    round-trips (Dragon4), compiled via FloatingDecimal. Exact-string checks
     *    ARE the shortest-round-trip test (0.1 must be "0.1", not "0.10000...").
     *    Each module returns 0 if all checks pass, else the 1-based failing index. ── */
    {
        struct { const char* src; const char* label; } dt[] = {
          { "class T { static int f(int z){"
            "  if(!Double.toString(0.0).equals(\"0.0\")) return 1;"
            "  if(!Double.toString(1.0).equals(\"1.0\")) return 2;"
            "  if(!Double.toString(-1.0).equals(\"-1.0\")) return 3;"
            "  if(!Double.toString(0.1).equals(\"0.1\")) return 4;"
            "  if(!Double.toString(0.5).equals(\"0.5\")) return 5;"
            "  if(!Double.toString(100.0).equals(\"100.0\")) return 6;"
            "  if(!Double.toString(123.456).equals(\"123.456\")) return 7;"
            "  if(!Double.toString(0.001).equals(\"0.001\")) return 8;"
            "  if(!Double.toString(1000000.0).equals(\"1000000.0\")) return 9;"
            "  if(!Double.toString(10000000.0).equals(\"1.0E7\")) return 10;"
            "  if(!Double.toString(0.0001).equals(\"1.0E-4\")) return 11;"
            "  if(!Double.toString(1.0E20).equals(\"1.0E20\")) return 12;"
            "  if(!Double.toString(-5.5).equals(\"-5.5\")) return 13;"
            "  return 0; } }",
            "Double.toString spec-value table (shortest round-trip; 0=all pass)" },
          { "class T { static int f(int z){"
            "  if(!Double.toString(Double.POSITIVE_INFINITY).equals(\"Infinity\")) return 1;"
            "  if(!Double.toString(Double.NEGATIVE_INFINITY).equals(\"-Infinity\")) return 2;"
            "  if(!Double.toString(Double.NaN).equals(\"NaN\")) return 3;"
            "  return 0; } }",
            "Double.toString Infinity/NaN" },
          { "class T { static int f(int z){"
            "  if(!Float.toString(1.0f).equals(\"1.0\")) return 1;"
            "  if(!Float.toString(0.1f).equals(\"0.1\")) return 2;"
            "  if(!Float.toString(100.0f).equals(\"100.0\")) return 3;"
            "  if(!Float.toString(0.5f).equals(\"0.5\")) return 4;"
            "  if(!Float.toString(0.0001f).equals(\"1.0E-4\")) return 5;"
            "  if(!Float.toString(10000000.0f).equals(\"1.0E7\")) return 6;"
            "  return 0; } }",
            "Float.toString spec-value table (shortest round-trip)" },
          { "class T { static int f(int z){"
            "  if(!String.valueOf(2.5).equals(\"2.5\")) return 1;"
            "  if(!(\"x=\" + 1.5).equals(\"x=1.5\")) return 2;"     /* StringBuffer.append(double) path */
            "  return 0; } }",
            "String.valueOf(double) + concat(double) path" },
        };
        for (int i = 0; i < (int)(sizeof dt / sizeof dt[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, dt[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0), res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == 0, dt[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E2: wide-element arrays, ref arrays, instanceof, checkcast, executed ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } e2[] = {
          { "class T { static int f(int x){ long[] a=new long[2]; a[0]=x; a[1]=a[0]+1; return (int)(a[0]+a[1]); } }",
            10, 21, "long[]: store/load wide elems + promotion == 21" },
          { "class T { static int f(int x){ double[] a=new double[2]; a[0]=x; a[1]=2; return (int)(a[0]*a[1]); } }",
            5, 10, "double[]: a[0]*a[1] == 10" },
          { "class Box { int v; } class T { static int f(int x){ Object o = new Box(); if (o instanceof Box) return 1; return 0; } }",
            0, 1, "instanceof: new Box is Box == 1" },
          /* interface instanceof: an interface target needs a runtime type check — interface refs
           * are typed as the root, so a structural test can't tell implementors apart. */
          { "interface I {} class A implements I {} class B {}"
            "class T { static int f(int x){ Object a = new A(); Object b = new B();"
            "  return ((a instanceof I) && !(b instanceof I)) ? 42 : 0; } }",
            0, 42, "interface instanceof: A implements I, B does not" },
          /* §20.1.1 getClass(): returns the object's Class (field 0) — non-null, and the
           * same per-class singleton on repeated calls / across instances (identity). */
          { "class Box { int v; } class T { static int f(int x){ Object o = new Box();"
            "  return (o.getClass() != null) ? 1 : 0; } }",
            0, 1, "getClass() != null" },
          { "class Box {} class T { static int f(int x){ Box a = new Box(); Box b = new Box();"
            "  return (a.getClass() == b.getClass()) ? 5 : 0; } }",
            0, 5, "getClass() is a per-class singleton (same for two Box instances)" },
          /* §20.3.2 getName() end-to-end: getClass().getName() = the fq name; reflect_init
           * (module start) must have set the Class singletons' field0 so dispatch works. */
          { "class Box {} class T { static int f(int x){"
            "  return new Box().getClass().getName().length(); } }",
            0, 3, "getClass().getName() == \"Box\" (length 3) — reflection end-to-end" },
          /* §20.22.4 Throwable.toString() = getClass().getName() + ": " + message, where
           * getName() is the FULLY-QUALIFIED name (§20.3.2): "java.lang.Throwable" (19)
           * + ": hi" (4) = 23. */
          { "class T { static int f(int x){"
            "  return new Throwable(\"hi\").toString().length(); } }",
            0, 23, "Throwable.toString() == \"java.lang.Throwable: hi\" (length 23)" },
          /* §20.3.4 getSuperclass, §20.3.3 isInterface, §20.3.5 getInterfaces — reflect_init
           * sets the singletons' superclass/interfaces refs. */
          { "class A {} class B extends A {} class T { static int f(int x){"
            "  return (new B().getClass().getSuperclass() == new A().getClass()) ? 7 : 0; } }",
            0, 7, "getSuperclass(): B.class.getSuperclass() == A.class" },
          { "class A {} class T { static int f(int x){"
            "  return new A().getClass().isInterface() ? 0 : 1; } }",
            0, 1, "isInterface(): a class Class is not an interface" },
          { "interface I {} class A implements I {} class T { static int f(int x){"
            "  return new A().getClass().getInterfaces()[0].isInterface() ? 9 : 0; } }",
            0, 9, "getInterfaces()[0].isInterface(): A's interface I's Class is an interface" },
          { "class Box { int v; } class T { static int f(int x){ Object o = new Box(); Box b=(Box)o; b.v=x; return b.v; } }",
            7, 7, "checkcast: (Box)o then b.v == 7" },
          { "class Box { int v; } class T { static int f(int x){ Box[] a=new Box[1]; a[0]=new Box(); a[0].v=x; return a[0].v; } }",
            9, 9, "Box[]: a[0]=new Box; a[0].v=x == 9" },
          { "class T { static int f(int x){ long a = x; return (int)(a + 1); } }",
            10, 11, "binary promotion: long + int literal == 11" },
          { "class T { static int f(int x){ double d = x; return (int)(d * 2); } }",
            5, 10, "binary promotion: double * int literal == 10" },
          { "class T { static int f(int x){ long a = x; a += 1; return (int)a; } }",
            10, 11, "compound assign: long += int (§15.26.2) == 11" },
          { "class T { static long s; static int f(int x){ s = x; s += 1; return (int)s; } }",
            20, 21, "compound assign: static long += int == 21" },
          { "class T { static int f(int x){ long[] a = new long[1]; a[0] = x; a[0] += 1; return (int)a[0]; } }",
            30, 31, "compound assign: long[] elem += int == 31" },
          { "class T { static int f(int x){ int[][] a = new int[2][]; a[0] = new int[3]; a[0][1] = x; return a[0][1]; } }",
            42, 42, "multi-dim: jagged int[][] store/load == 42" },
          { "class Box { int v; } class T { static int f(int x){ Box[][] a = new Box[2][]; a[0] = new Box[3]; a[0][0] = new Box(); a[0][0].v = x; return a[0][0].v; } }",
            13, 13, "multi-dim: jagged Box[][] ref nesting == 13" },
          { "class T { static int f(int x){ int[][] a = new int[2][3]; a[1][2] = x; return a[1][2] + a[0][0]; } }",
            7, 7, "multi-dim: rectangular new int[2][3] == 7" },
          { "class T { static int f(int x){ int[][] a = new int[x][3]; a[x-1][2] = 9; return a[x-1][2] + a[0][1]; } }",
            4, 9, "multi-dim: rectangular runtime dims == 9" },
          { "class T { static int f(int x){ int[][][] a = new int[2][2][2]; a[1][1][1] = x; return a[1][1][1] + a[0][0][0]; } }",
            5, 5, "multi-dim: rectangular 3-D == 5" },
        };
        for (int i = 0; i < (int)(sizeof e2 / sizeof e2[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, e2[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(e2[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == e2[i].want, e2[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §10.2 reference-array COVARIANCE (the RefArray overlay). String[] <: Object[],
     * every array <: Object (§10.7). These were COMPILE ERRORS before the overlay
     * (invariant concrete arrays); now String[] and Object[] are the one RefArray type,
     * so a widening assignment is an identity and reads/stores flow through the same
     * backing. (A wrong covariant store would surface as a ClassCast on read until the
     * §10.10 ArrayStore guard — Stage C; these all store compatible elements.) ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } cov[] = {
          { "class T { static int f(int x){ String[] sa = new String[2]; Object[] oa = sa;"
            "  oa[1] = \"hey\"; return ((String)oa[1]).length(); } }",
            0, 3, "§10.2 covariant assign Object[]=String[], store+read+cast → 3" },
          { "class T { static int f(int x){ Object[] oa = new String[5]; return oa.length; } }",
            0, 5, "§10.2 covariant new + length: Object[] = new String[5], .length == 5" },
          { "class T { static int f(int x){ Object o = new String[3]; return (o == null) ? 0 : 1; } }",
            0, 1, "§10.7 a reference array is an Object: Object o = String[] non-null (RefArray <: Object)" },
          /* EXPECTED FAIL — the Stage D (§10.7/§10.8 arrays-as-Objects) marker: a PRIMITIVE
           * array stays a concrete WASM array, which is not a struct-subtype of Object, so
           * `Object o = int[]` can't be an identity assign yet. Stays red until the
           * array-as-Object representation lands; do NOT nerf to pass. */
          { "class T { static int f(int x){ int[] ia = new int[3]; Object o = ia; return (o == null) ? 0 : 1; } }",
            0, 1, "STAGE D FAIL-CASE §10.7: a primitive array is an Object (Object o = int[])" },
          { "class T { static int f(int x){ String[] sa = new String[1]; Object[] oa = sa;"
            "  sa[0] = \"xy\"; return ((String)oa[0]).length(); } }",
            0, 2, "§10.2 aliasing: write via String[], read via Object[] (same array) → 2" },
        };
        for (int i = 0; i < (int)(sizeof cov / sizeof cov[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cov[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(cov[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == cov[i].want, cov[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §10.10 ArrayStoreException: a covariant store whose runtime value isn't
     * assignable to the array's ACTUAL element type throws (catchable). The spec's
     * Point/ColoredPoint example + an interface element type. The check is a compiled
     * Class.arrayStoreCheck → Class.assignableFrom (§5.1.4 subtype), run before the store. ── */
    {
        struct { const char* src; int32_t want; const char* label; } as[] = {
          /* §10.10 spec example: ColoredPoint[] viewed as Point[]; storing a bare Point throws. */
          { "class Point { int x; } class ColoredPoint extends Point { int c; }"
            "class T { static int f(int z){ ColoredPoint[] cpa = new ColoredPoint[2]; Point[] pa = cpa;"
            "  try { pa[0] = new Point(); return 0; } catch (ArrayStoreException e) { return 1; } } }",
            1, "§10.10 bad covariant store (Point into ColoredPoint[]) → ArrayStoreException caught" },
          /* the assignable store does NOT throw (ColoredPoint into ColoredPoint[]). */
          { "class Point { int x; } class ColoredPoint extends Point { int c; }"
            "class T { static int f(int z){ Point[] pa = new ColoredPoint[2];"
            "  pa[1] = new ColoredPoint(); return 7; } }",
            7, "§10.10 assignable store (ColoredPoint into ColoredPoint[]) does not throw" },
          /* storing null is always allowed (§10.10: null assignable to any reference). */
          { "class Point { int x; } class ColoredPoint extends Point { int c; }"
            "class T { static int f(int z){ Point[] pa = new ColoredPoint[2]; pa[0] = null; return 5; } }",
            5, "§10.10 storing null is always allowed" },
          /* interface element type: I[] holds an implementor; a non-implementor throws. */
          { "interface I {} class C implements I {} class D {}"
            "class T { static int f(int z){ I[] ia = new I[2]; ia[0] = new C();"
            "  Object[] oa = ia; try { oa[1] = new D(); return 0; } catch (ArrayStoreException e) { return 1; } } }",
            1, "§10.10 interface element: non-implementor store → ArrayStoreException" },
        };
        for (int i = 0; i < (int)(sizeof as / sizeof as[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, as[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == as[i].want, as[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §10.10 MULTI-DIMENSIONAL ArrayStore: the array's element is itself an array
     * type. The elementClass is the component's array Class (e.g. String[]'s Class), and
     * assignableFrom decides §10.2 array covariance via the componentType link. ── */
    {
        struct { const char* src; int32_t want; const char* label; } ms[] = {
          { "class T { static int f(int z){ Object[][] a = new String[3][]; a[0] = new String[1]; return 7; } }",
            7, "§10.10 multi-dim: String[] into String[][] (elem String[]) OK" },
          { "class T { static int f(int z){ Object[][] a = new String[3][];"
            "  try { a[0] = new Integer[1]; return 0; } catch (ArrayStoreException e) { return 1; } } }",
            1, "§10.10 multi-dim: Integer[] into String[][] (via Object[][]) → ArrayStoreException" },
          { "class T { static int f(int z){ Object[][] o = new Object[2][]; o[0] = new String[1]; return 9; } }",
            9, "§10.10 multi-dim covariance: String[] into Object[][] (elem Object[]) OK" },
          { "class T { static int f(int z){ Object[][] o = new Object[2][]; o[0] = null; return 4; } }",
            4, "§10.10 multi-dim: storing null is always allowed" },
        };
        for (int i = 0; i < (int)(sizeof ms / sizeof ms[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ms[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ms[i].want, ms[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.1.5 Object.clone(): shallow copy of a Cloneable class; throws
     * CloneNotSupportedException otherwise. The copy is a compiler-synthesized per-type
     * internalClone reached through Object.clone(), so super.clone() copies the runtime type. ── */
    {
        struct { const char* src; int32_t want; const char* label; } cl[] = {
          /* standard idiom: Box implements Cloneable + public clone()=super.clone(). Copy is
           * independent (b != c) and carries the fields. */
          { "class Box implements Cloneable { int x; int y;"
            "  public Object clone() { try { return super.clone(); } catch (CloneNotSupportedException e) { return null; } } }"
            "class T { static int f(int z){ Box b = new Box(); b.x = 5; b.y = 7;"
            "  Box c = (Box) b.clone(); c.x = 9; return c.x*100 + c.y*10 + b.x + (b==c?0:1000); } }",
            /* c.x=9,c.y=7,b.x still 5 (independent),b!=c → 900+70+5+1000 */ 1975,
            "§20.1.5 clone() copies fields + is independent (super.clone idiom)" },
          /* clone() on a non-Cloneable class throws CloneNotSupportedException (catchable). */
          { "class Foo { int x;"
            "  public int tryClone(){ try { clone(); return 0; } catch (CloneNotSupportedException e) { return 1; } } }"
            "class T { static int f(int z){ return new Foo().tryClone(); } }",
            1, "§20.1.5 clone() on non-Cloneable → CloneNotSupportedException" },
          /* clone().getClass() == original.getClass() (§20.1.5 same runtime class). */
          { "class Box implements Cloneable { int x;"
            "  public Object clone() { try { return super.clone(); } catch (CloneNotSupportedException e) { return null; } } }"
            "class T { static int f(int z){ Box b = new Box(); Object c = b.clone();"
            "  return (c.getClass() == b.getClass()) ? 42 : 0; } }",
            42, "§20.1.5 clone().getClass() == original.getClass()" },
          /* §10.7 arrays are Cloneable: arr.clone() deep-copies the backing (array.copy),
           * the clone is a distinct array of the same length, elements copied. */
          { "class T { static int f(int z){ int[] a = {1,2,3}; int[] b = (int[]) a.clone(); b[0] = 9;"
            "  return b[0]*100 + a[0]*10 + b.length + (a==b?0:1000); } }",
            /* b[0]=9, a[0] untouched=1, len 3, a!=b → 900+10+3+1000 */ 1913,
            "§20.1.5 int[].clone(): independent deep copy of the backing" },
          { "class T { static int f(int z){ int[] a = {4,5,6}; int[] b = (int[]) a.clone();"
            "  return (b.getClass() == a.getClass()) ? (b[0]+b[1]+b[2]) : 0; } }",
            15, "§20.1.5 int[].clone().getClass() == a.getClass(), contents copied" },
          /* reference array: element refs copied shallowly (same objects), array distinct. */
          { "class T { static int f(int z){ String[] a = new String[2]; a[0] = \"hi\";"
            "  String[] b = (String[]) a.clone(); return (b[0] == a[0] && a != b) ? 7 : 0; } }",
            7, "§20.1.5 String[].clone(): shallow element copy (same refs), distinct array" },
        };
        for (int i = 0; i < (int)(sizeof cl / sizeof cl[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cl[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == cl[i].want, cl[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §10.7 reference arrays implement Cloneable (and are Objects). The §10.8
     * per-component Class and the primitive-array overlay are a separate migration. ── */
    {
        struct { const char* src; int32_t want; const char* label; } cl[] = {
          { "class Box { int v; } class T { static int f(int z){ return (new Box[2] instanceof Cloneable) ? 1 : 0; } }",
            1, "§10.7 a reference array is Cloneable (Box[] instanceof Cloneable)" },
          { "class Box { int v; } class T { static int f(int z){ Box[] b = new Box[1]; Cloneable c = b; return (c == b) ? 1 : 0; } }",
            1, "§10.7 a reference array widens to Cloneable" },
        };
        for (int i = 0; i < (int)(sizeof cl / sizeof cl[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cl[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == cl[i].want, cl[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E3: static fields (module globals) — read/write, wide, default init ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } e3[] = {
          { "class T { static int s; static int f(int x){ s = x; s = s + 1; return s; } }",
            10, 11, "static int: write/read/write == 11" },
          { "class T { static long sl; static int f(int x){ sl = x; sl = sl + 1; return (int)sl; } }",
            20, 21, "static long: wide global write/read == 21" },
          { "class T { static int a; static int b; static int f(int x){ a = x; b = x + x; return a + b; } }",
            4, 12, "two static fields distinct globals == 12" },
          { "class T { static int s = 7; static int f(int x){ return s; } }",
            0, 7, "clinit: static init runs at module start == 7" },
          { "class T { static int a = 3; static int b = a + 4; static int f(int x){ return b; } }",
            0, 7, "clinit: init order (b reads a) == 7" },
          { "class T { static long sl = 5; static int f(int x){ return (int)(sl + x); } }",
            6, 11, "clinit: wide static init + widening == 11" },
          { "class T { static int s; static { s = 7; } static int f(int x){ return s; } }",
            0, 7, "clinit: static block runs == 7" },
          { "class T { static int s; static { int t = 5; s = t * 2; } static int f(int x){ return s; } }",
            0, 10, "clinit: static block with local == 10" },
          { "class T { static int a = 3; static { a = a + 4; } static int f(int x){ return a; } }",
            0, 7, "clinit: field init then block, textual order == 7" },
          { "class T { static int s; static { for (int i = 0; i < 4; i++) s = s + i; } static int f(int x){ return s; } }",
            0, 6, "clinit: static block with i++ loop == 6" },
        };
        for (int i = 0; i < (int)(sizeof e3 / sizeof e3[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, e3[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(e3[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == e3[i].want, e3[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E3.5: sub-int value model — narrow-on-store (compound/inc/dec) +
     * sign/zero extension. JLS §15.25.2 (E1 op= E2 ≡ E1=(T)(E1 op E2)),
     * §15.13/§15.14 (inc/dec narrow to T before store), §5.1.2 (byte/short
     * sign-extend, char zero-extend). Compound/inc cases are RED on the
     * unpacked-i32 build (implicit (T) cast missing); cast/extension cases
     * guard the packing impl. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } si[] = {
          /* compound assign overflow — implicit (T) narrowing (JLS §15.25.2) */
          { "class T { static int f(int x){ byte b=100; b+=x; return b; } }",
            100, -56, "compound: local byte 100+=100 == -56 (T)" },
          { "class T { static int f(int x){ short s=30000; s+=x; return s; } }",
            10000, -25536, "compound: local short 30000+=10000 == -25536 (T)" },
          { "class T { static int f(int x){ char c=65000; c+=x; return c; } }",
            1000, 464, "compound: local char 65000+=1000 == 464 (T,zero-ext)" },
          { "class T { static byte b; static int f(int x){ b=100; b+=x; return b; } }",
            100, -56, "compound: static byte == -56 (T)" },
          { "class C { byte b; } class T { static int f(int x){ C c=new C(); c.b=100; c.b+=x; return c.b; } }",
            100, -56, "compound: instance byte field == -56 (T)" },
          /* Scalar-replacement exec pins (spec §6.1 gate 3): under Click these objects are SCALAR-REPLACED —
           * the allocation gone, fields are locals, the ctor's initializers materialized onto the
           * slots. Semantics must be identical with the optimizer off. The byte cases above are the
           * narrow-slot pins on the same path. */
          { "class C { int v; } class T { static int f(int x){ C c=new C(); c.v=x; int a=c.v;"
            " c.v=a+2; return c.v*10+a; } }",
            5, 75, "scalar-replaced object field store/load/re-store == 75" },
          { "class P { int x; P(int a){ x = a; } }"
            " class T { static int f(int x){ P p = new P(x + 1); return p.x; } }",
            5, 6, "user ctor param bound to arg, scalar-replaced == 6" },
          { "class Pt { int x, y; Pt(int a, int b){ x = a; y = b; } }"
            " class T { static int f(int x){ Pt p = new Pt(x, x * 2); return p.x * 100 + p.y; } }",
            3, 306, "multi-field user ctor, scalar-replaced == 306" },
          { "class A { int a = 3; } class B extends A { int b = 4; }"
            " class T { static int f(int x){ B o = new B(); return o.a * 10 + o.b; } }",
            0, 34, "super-chain field inits, scalar-replaced == 34" },
          { "class T { static int f(int x){ byte[] a=new byte[1]; a[0]=100; a[0]+=x; return a[0]; } }",
            100, -56, "compound: byte[] elem == -56 (T)" },
          /* inc/dec overflow — implicit (T) narrowing (JLS §15.13/§15.14) */
          { "class T { static int f(int x){ byte b=127; b++; return b; } }",
            0, -128, "inc: local byte 127++ == -128 (T)" },
          { "class T { static int f(int x){ char c=65535; c++; return c; } }",
            0, 0, "inc: local char 65535++ == 0 (T)" },
          { "class T { static short s; static int f(int x){ s=32767; s++; return s; } }",
            0, -32768, "inc: static short 32767++ == -32768 (T)" },
          { "class C { byte b; } class T { static int f(int x){ C c=new C(); c.b=127; c.b++; return c.b; } }",
            0, -128, "inc: instance byte field 127++ == -128 (T)" },
          { "class T { static int f(int x){ short[] a=new short[1]; a[0]=32767; a[0]++; return a[0]; } }",
            0, -32768, "inc: short[] elem 32767++ == -32768 (T)" },
          /* sign/zero extension on read (JLS §5.1.2) — guards the packing impl */
          { "class C { byte b; } class T { static int f(int x){ C c=new C(); c.b=(byte)-1; return c.b; } }",
            0, -1, "ext: byte field -1 sign-extends to -1" },
          { "class C { char c; } class T { static int f(int x){ C o=new C(); o.c=(char)65535; int r=o.c; return r; } }",
            0, 65535, "ext: char field 0xFFFF zero-extends to 65535" },
          { "class T { static int f(int x){ short[] a=new short[1]; a[0]=(short)-1; return a[0]; } }",
            0, -1, "ext: short[] -1 sign-extends to -1" },
          { "class T { static int f(int x){ char[] a=new char[1]; a[0]=(char)65535; int r=a[0]; return r; } }",
            0, 65535, "ext: char[] 0xFFFF zero-extends to 65535" },
          /* explicit cast truncation (JLS §5.1.3) — regression guards */
          { "class T { static int f(int x){ return (byte)x; } }",
            300, 44, "cast: (byte)300 == 44" },
          { "class T { static int f(int x){ return (short)x; } }",
            40000, -25536, "cast: (short)40000 == -25536" },
          { "class T { static int f(int x){ char c=(char)x; return c; } }",
            -1, 65535, "cast: (char)-1 == 65535" },
          /* array initializers — the full type set */
          { "class T { static int f(int x){ char[] a = {65535, 7}; int r=a[0]; return r; } }",
            0, 65535, "arrayinit: char[]{65535} == 65535" },
          { "class T { static int f(int x){ long[] a = {100, 200}; return (int)(a[0]+a[1]); } }",
            0, 300, "arrayinit: long[]{100,200} sum == 300" },
          { "class T { static int f(int x){ double[] a = {1.5, 2.5}; return (int)(a[0]+a[1]); } }",
            0, 4, "arrayinit: double[]{1.5,2.5} sum == 4" },
          { "class T { static int f(int x){ char[] a = new char[2]; a[0]=(char)65535; int r=a[0]; return r; } }",
            0, 65535, "newarray: new char[2] a[0]=0xFFFF == 65535" },
          { "class T { static int f(int x){ short[] a = new short[2]; a[0]=(short)-1; return a[0]; } }",
            0, -1, "newarray: new short[2] a[0]=-1 == -1" },
          { "class T { static int f(int x){ byte[] a = new byte[2]; a[0]=(byte)-1; return a[0]; } }",
            0, -1, "newarray: new byte[2] a[0]=-1 == -1" },
          /* static ARRAY fields (clinit concrete-ref typing) incl. wide/char literals */
          { "class T { static int[] a = new int[3]; static int f(int x){ a[0]=x; return a[0]; } }",
            7, 7, "static int[] field (new int[3]) == 7" },
          { "class T { static long[] a = {100,200}; static int f(int x){ return (int)(a[0]+a[1]); } }",
            0, 300, "static long[] field {100,200} == 300" },
          { "class T { static char[] a = {65535}; static int f(int x){ return a[0]; } }",
            0, 65535, "static char[] field {65535} == 65535" },
          { "class C { int v = 5; C(){} } class T { static int f(int x){ C c=new C(); return c.v; } }",
            0, 5, "instance field init, explicit ctor (int v=5) == 5" },
          { "class C { int v; C(){ v = 9; } } class T { static int f(int x){ C c=new C(); return c.v; } }",
            0, 9, "ISOLATE: explicit ctor body assigns v=9 == 9" },
          /* JLS §8.8.9 default ctor synthesis: a class with NO ctor still runs its
           * field initializers (in the synthesized default ctor's step 4). */
          { "class C { int v = 7; } class T { static int f(int x){ C c=new C(); return c.v; } }",
            0, 7, "default ctor (no decl): field init v=7 runs == 7" },
          /* JLS §12.5 ORDER: super() (step 3) runs BEFORE the subclass field inits
           * (step 4). A's ctor sets a=5; B's initializer b=a+1 must see a==5 → 6.
           * (If field init ran before super, b would be 0+1=1.) */
          { "class A { int a; A(){ a = 5; } } class B extends A { int b = a + 1; } "
            "class T { static int f(int x){ B o=new B(); return o.b; } }",
            0, 6, "ctor order: super() before subclass field init (b=a+1==6)" },
          /* Inherited field initialized through the super-ctor chain. */
          { "class A { int a = 3; } class B extends A { int b = 4; } "
            "class T { static int f(int x){ B o=new B(); return o.a + o.b; } }",
            0, 7, "super-chain field init: inherited a=3 + own b=4 == 7" },
          /* ── E4 dispatch: virtual call through the populated vtable ── */
          { "class A { int m(){ return 11; } } "
            "class T { static int f(int x){ A a=new A(); return a.m(); } }",
            0, 11, "virtual call: a.m() through vtable == 11" },
          /* override: a BASE-typed reference to a DERIVED object dispatches the
           * most-derived override (the whole point of vtable population). */
          { "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } } "
            "class T { static int f(int x){ A a=new B(); return a.m(); } }",
            0, 2, "override: A a=new B(); a.m() resolves B.m == 2" },
          /* the base method still resolves when the object IS the base. */
          { "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } } "
            "class T { static int f(int x){ A a=new A(); return a.m(); } }",
            0, 1, "override: A a=new A(); a.m() resolves A.m == 1" },
          /* virtual call reaching an INHERITED (non-overridden) method. */
          { "class A { int m(){ return 5; } } class B extends A { } "
            "class T { static int f(int x){ B b=new B(); return b.m(); } }",
            0, 5, "inherited virtual: B b=new B(); b.m() resolves A.m == 5" },
          /* interface dispatch shares the same slot mechanism as virtual. */
          { "interface I { int m(); } class C implements I { public int m(){ return 9; } } "
            "class T { static int f(int x){ I i=new C(); return i.m(); } }",
            0, 9, "interface call: I i=new C(); i.m() == 9" },
          { "class C { static int g(){ return 3; } } class T { static int f(int x){ return C.g(); } }",
            0, 3, "ISOLATE: cross-class static call C.g() == 3" },
          /* MIXED vtable: a class with BOTH a native (host-import → ref.null slot)
           * and a compiled (ref.func slot) instance method. Dispatching the compiled
           * one must validate + run despite the ref.null sibling slot. This is the
           * String.length()/charAt() shape, isolated from the prelude. */
          { "class A { native int n(); int c(){ return 5; } } "
            "class T { static int f(int x){ A a=new A(); return a.c(); } }",
            0, 5, "MIXED vtable: dispatch compiled c() on class with native n() == 5" },
          /* Closer to String: the dispatched compiled method READS a char[] field. */
          { "class A { char[] v; native int n(); int len(){ return v.length; } } "
            "class T { static int f(int x){ A a=new A(); a.v=new char[3]; return a.len(); } }",
            0, 3, "MIXED vtable: dispatch compiled len() reading char[] field == 3" },
          /* String's EXACT shape as a USER class: final, char[] value, char[] ctor,
           * native sibling, compiled length()+charAt(int), same call expression. */
          /* OVERLOADED ctors: `new M(char[])` must resolve M(char[]), not M() or
           * M(int). String has 7 ctors — a single-ctor class can't surface this. */
          { "class M { int x; M(){ x=1; } M(int a){ x=a+10; } M(char[] c){ x=c.length+100; } } "
            "class T { static int f(int z){ char[] c=new char[5]; M m=new M(c); return m.x; } }",
            0, 105, "overloaded ctors: new M(char[5]) resolves M(char[]) → 105" },
          /* ACCESS in the export surface: a PRIVATE method must NOT be exported —
           * here a private v(int) shares the name of the public v(), so exporting it
           * would collide on "A.v" (§2.5.10). It still dispatches internally. */
          { "class A { public int v(){ return 10; } private int v(int x){ return x; } } "
            "class T { static int f(int z){ A a = new A(); return a.v(); } }",
            0, 10, "private overload v(int) not exported (no \"A.v\" collision); a.v()==10" },
          /* UNQUALIFIED instance self-call: g() calls h(5) with implicit `this`. */
          { "class A { int g(){ return h(5); } int h(int x){ return x + 1; } } "
            "class T { static int f(int z){ A a = new A(); return a.g(); } }",
            0, 6, "unqualified instance self-call g()->h(5) == 6" },
          /* Reference identity == / != (→ ref.eq): needed for Object.equals, null
           * checks, and identity comparisons. */
          { "class B { } class T { static int f(int z){ B a=new B(); B b=a; B c=new B();"
            "  return (a==b ? 10 : 0) + (a!=c ? 1 : 0); } }",
            0, 11, "reference identity: (a==b)*10 + (a!=c) == 11" },
          /* Object.equals as a COMPILED overlay (this == obj) dispatched virtually on a
           * user object that inherits it — was native (ref.null slot → trap). */
          { "class B { } class T { static int f(int z){ B a=new B(); B b=a; B c=new B();"
            "  return (a.equals(b) ? 10 : 0) + (a.equals(c) ? 0 : 1); } }",
            0, 11, "Object.equals overlay (identity), inherited+virtual == 11" },
          /* INHERITED virtual method dispatched on a subclass (B inherits A.m, doesn't
           * override) — resolve_slots' super-first walk must fill B's slot with A.m. */
          { "class A { int m(){ return 7; } } class B extends A { int n(){ return 3; } } "
            "class T { static int f(int z){ B b = new B(); return b.m() + b.n(); } }",
            0, 10, "inherited virtual: b.m() (from A) + b.n() == 10" },
          /* STRING LITERAL is a real String (was the yoctojc char[] sugar): "Hi" lowers
           * to new String(char[]{'H','i'}); the overlay length()/charAt() read it. */
          { "class T { static int f(int z){ String s = \"Hi\"; return s.length()*1000 + s.charAt(1); } }",
            0, 2105, "string literal: String s=\"Hi\"; s.length()*1000+charAt(1) == 2105" },
          { "class T { static int f(int z){ return \"Hello\".length(); } }",
            0, 5, "method on string literal: \"Hello\".length() == 5" },
          /* String.equals (value compare: ref==, instanceof, cast, char[] loop) +
           * toString (returns this). Exercises early-return-from-loop + another
           * instance's private `value` field access. */
          { "class T { static int f(int z){ String a = \"ab\";"
            "  return (a.equals(\"ab\") ? 1000 : 0) + (a.equals(\"ac\") ? 0 : 100)"
            "       + (a.equals(\"abc\") ? 0 : 10) + a.toString().length(); } }",
            0, 1112, "String.equals value-compare + toString().length() == 1112" },
          /* String.hashCode — the JLS §20.12.30 polynomial s[0]*31^(n-1)+…, a
           * compiled overlay (h = 31*h + value[i]) over the char[]. */
          { "class T { static int f(int z){ return \"ab\".hashCode(); } }",
            0, 3105, "String.hashCode: \"ab\" == 97*31+98 == 3105" },
          { "final class S { char[] value; S(char[] v){ this.value=v; } public native int hashCode();"
            "  int length(){ return value.length; } char charAt(int i){ return value[i]; } } "
            "class T { static int f(int x){ char[] c=new char[2]; c[0]=72; c[1]=105;"
            "  S s=new S(c); return s.length()*1000 + s.charAt(1); } }",
            0, 2105, "String-shape USER class: s.length()*1000 + s.charAt(1) == 2105" },
          /* HOST BOUNDARY: a ref-carrying native is reached through a bridging
           * forwarder. The import is typed externref (the embedder holds host refs
           * as externref; `any` and `extern` are disjoint cones), and the forwarder
           * converts at the edge: extern.convert_any out, any.convert_extern +
           * ref.cast back. The echo host returns its argument unchanged, so object
           * IDENTITY must survive the round trip. STATIC native, called directly: */
          { "class T { static native Object identity(Object o);"
            "  static int f(int z){ T a = new T(); Object b = identity(a);"
            "  return (a == b) ? 777 : 0; } }",
            0, 777, "native ref round-trip (static, direct forwarder): a==identity(a) == 777" },
          /* VIRTUAL instance native, dispatched through the vtable: the slot holds
           * the forwarder funcref (uniform with compiled overrides), call_ref lands
           * on it, it marshals `this` ↔ externref around the host import. */
          { "class B { native Object self(); }"
            "class T { static int f(int z){ B a = new B(); Object b = a.self();"
            "  return (a == b) ? 555 : 0; } }",
            0, 555, "native ref round-trip (virtual, vtable forwarder): a.self()==a == 555" },
          /* §20.1.4 identity hash: a COMPILED Object.hashCode() over a stored per-object
           * `hash` field (lazily assigned from a counter). It must be stable across GC, so
           * it can't be the object's address (the collector moves objects) — it's a field.
           * §20.1.6 contract: consistent within a run (same object → same non-zero hash). */
          { "class T { static int f(int z){ Object o = new Object();"
            "  return (o.hashCode() == o.hashCode() && o.hashCode() != 0) ? 1 : 0; } }",
            0, 1, "Object.hashCode(): stored identity hash, consistent + non-zero" },
          /* INHERITED + DISTINCTNESS: a class that doesn't override hashCode inherits
           * Object's compiled hashCode; two distinct objects get distinct stored hashes. */
          { "class B { } class T { static int f(int z){ B a = new B(); B b = new B();"
            "  return (a.hashCode() != b.hashCode()) ? 1 : 0; } }",
            0, 1, "inherited Object.hashCode() → distinct per object" },
          /* MIXED slot: String OVERRIDES hashCode (its §20.12.30 polynomial) while plain
           * objects use Object's compiled identity hash — both compiled, one shared slot.
           * "ab" → 97*31+98 == 3105, distinct from the identity-hash path. */
          { "class T { static int f(int z){ Object s = \"ab\"; return s.hashCode(); } }",
            0, 3105, "virtual hashCode on String-as-Object → compiled override (3105), not the forwarder" },
          /* `null` literal + reference comparison to null (LoadNull → ref.null none;
           * Eq(ref,ref) → ref.eq). Honest coverage of the two codegen fixes the Object
           * wiring surfaced — independent of any extern. */
          { "class T { static int f(int z){ String s = null; return (s == null) ? 1 : 0; } }",
            0, 1, "null literal: String s = null; s == null == 1" },
          { "class A { } class T { static A mk(){ return new A(); }"
            "  static int f(int z){ return (mk() == null) ? 9 : 4; } }",
            0, 4, "non-Object ref call-result == null direct (binop ref temp typed) == 4" },
          /* NOTE getClass()/toString()/clone() are NOT extern — per E6 design (B)
           * they are COMPILED in the java-runtime module (Class singletons / a
           * toString overlay / a per-class shallow copy), since a host can't mint a
           * module GC object. They are intentionally left unwired here rather than
           * stubbed green; they land with the compiled runtime. */
          /* finalize() — the default is an empty method; calling it returns. (Compiled
           * empty body in the runtime; a no-op either way.) */
          { "class T { int g() throws Throwable { this.finalize(); return 7; }"
            "  static int f(int z) throws Throwable { return new T().g(); } }",
            0, 7, "Object.finalize() (empty) returns == 7" },
          /* §20.22 Throwable detail-message overlay: the message is a ref field; the
           * (String) ctors chain super(s) up the whole §11.5 hierarchy, getMessage()
           * reads it. A message-carrying throw round-trips it; the no-arg ctor → null. */
          { "class T { static int f(int z){ try { throw new ArithmeticException(\"xyz\"); }"
            "  catch (ArithmeticException e) { return e.getMessage().length(); } } }",
            0, 3, "Throwable.getMessage() propagates: new ArithmeticException(\"xyz\") == 3" },
          { "class T { static int f(int z){ try { throw new Exception(); }"
            "  catch (Exception e) { return (e.getMessage() == null) ? 1 : 0; } } }",
            0, 1, "Throwable.getMessage() == null for the no-arg ctor == 1" },
          /* §15.16.2 (E7): integer / and % by zero must THROW a CATCHABLE
           * ArithmeticException (an explicit guard before the div — a WASM trap
           * would be uncatchable). */
          { "class T { static int f(int z){ try { return 10 / z; }"
            "  catch (ArithmeticException e) { return 77; } } }",
            0, 77, "10/0 throws catchable ArithmeticException (§15.16.2)" },
          { "class T { static int f(int z){ try { return 10 % z; }"
            "  catch (ArithmeticException e) { return 88; } } }",
            0, 88, "10%0 throws catchable ArithmeticException" },
          { "class T { static int f(int z){ try { return 10 / z; }"
            "  catch (ArithmeticException e) { return 77; } } }",
            2, 5, "10/2 == 5 (guard doesn't disturb the non-zero path)" },
          /* §15.16.2: Integer.MIN_VALUE / -1 WRAPS to MIN_VALUE (Java), it does NOT
           * trap — the -1 guard arm takes -a to dodge WASM's i32.div_s overflow trap. */
          { "class T { static int f(int z){ int m = -2147483648; return m / z; } }",
            -1, (int32_t)0x80000000, "MIN_VALUE / -1 wraps to MIN_VALUE (no trap)" },
          { "class T { static int f(int z){ int m = -2147483648; return m % z; } }",
            -1, 0, "MIN_VALUE % -1 == 0 (no trap)" },
          /* The unary field-range invariant, behaviorally: an established
           * divisor field divides (the guard folds under -O, the answer must
           * not move); a field any writer stores unprovably keeps its guard,
           * so dividing by the stored 0 still raises the catchable §11
           * ArithmeticException; and a ctor that hands `this` out BEFORE
           * establishing lets the §12.5 default 0 be READ mid-construction —
           * the invariant must include 0, the observer's own guard must
           * survive, and the observed division must throw, not trap. */
          { "class V { int stride; V(){ stride = 4; } }"
            "class T { static int f(int z){ V v = new V(); return 100 / v.stride; } }",
            0, 25, "established divisor field: 100 / stride == 25" },
          { "class V { int stride; V(){ stride = 4; } void set(int s){ stride = s; } }"
            "class T { static int f(int z){ V v = new V(); v.set(z);"
            "  try { return 100 / v.stride; } catch (ArithmeticException e) { return 77; } } }",
            0, 77, "a parameter store keeps the guard: dividing by the stored 0 throws" },
          { "class H3 { int r; void probe(V2 v){"
            "  try { r = 10 / v.d; } catch (ArithmeticException e) { r = 55; } } }"
            "class V2 { int d; V2(H3 h){ h.probe(this); d = 5; } }"
            "class T { static int f(int z){ H3 h = new H3(); V2 v = new V2(h); return h.r; } }",
            0, 55, "a mid-construction observer reads the §12.5 default: its guard "
                   "survives and the division throws catchably" },
          /* §15.13.1 (E7): array access null-checks + bounds-checks throw CATCHABLE
           * NullPointerException / ArrayIndexOutOfBoundsException, not WASM traps. */
          { "class T { static int f(int z){ int[] a = null;"
            "  try { return a[0]; } catch (NullPointerException e) { return 11; } } }",
            0, 11, "null array read → catchable NullPointerException" },
          { "class T { static int f(int z){ int[] a = new int[3];"
            "  try { return a[5]; } catch (ArrayIndexOutOfBoundsException e) { return 22; } } }",
            0, 22, "read past end → catchable ArrayIndexOutOfBoundsException" },
          { "class T { static int f(int z){ int[] a = new int[3];"
            "  try { return a[-1]; } catch (ArrayIndexOutOfBoundsException e) { return 33; } } }",
            0, 33, "read negative index → ArrayIndexOutOfBoundsException" },
          /* The index is UNKNOWN here, so the whole §15.13.1 pair survives the
           * solve and (under -O) is merged into one unsigned compare — a
           * negative index must still raise the same catchable AIOOBE through
           * the merged test. The constant-index twins above never exercise the
           * merge: their low half folds KNOWN. */
          { "class T { static int f(int z){ int[] a = new int[3];"
            "  try { return a[z]; } catch (ArrayIndexOutOfBoundsException e) { return 34; } } }",
            -1, 34, "unknown negative index → AIOOBE through the merged unsigned test" },
          { "class T { static int f(int z){ int[] a = new int[3];"
            "  try { return a[z]; } catch (ArrayIndexOutOfBoundsException e) { return 35; } } }",
            5, 35, "unknown too-large index → AIOOBE through the merged unsigned test" },
          { "class T { static int f(int z){ int[] a = new int[3]; a[1] = 8;"
            "  try { return a[z]; } catch (ArrayIndexOutOfBoundsException e) { return 36; } } }",
            1, 8, "unknown in-bounds index reads through the merged unsigned test" },
          { "class T { static int f(int z){ int[] a = null;"
            "  try { a[0] = 1; return 0; } catch (NullPointerException e) { return 44; } } }",
            0, 44, "null array write → catchable NullPointerException" },
          { "class T { static int f(int z){ int[] a = new int[3];"
            "  try { a[9] = 1; return 0; } catch (ArrayIndexOutOfBoundsException e) { return 55; } } }",
            0, 55, "write past end → ArrayIndexOutOfBoundsException" },
          { "class T { static int f(int z){ int[] a = new int[3]; a[1] = 7; return a[1]; } }",
            0, 7, "in-bounds array read/write undisturbed by the guards" },
          { "class T { static int f(int z){ int[] a = new int[3];"    /* AIOOBE is a RuntimeException */
            "  try { return a[5]; } catch (RuntimeException e) { return 66; } } }",
            0, 66, "ArrayIndexOutOfBounds caught as RuntimeException (subtype dispatch)" },
          /* §15.11 / §15.12 / §10.7 (E7): instance field access, method dispatch, and
           * array.length on a null receiver throw CATCHABLE NullPointerException. */
          { "class A { int x; int m(){ return 9; } }"
            "class T { static int f(int z){ A a = null;"
            "  try { return a.x; } catch (NullPointerException e) { return 61; } } }",
            0, 61, "null.field read → catchable NullPointerException" },
          { "class A { int x; int m(){ return 9; } }"
            "class T { static int f(int z){ A a = null;"
            "  try { a.x = 1; return 0; } catch (NullPointerException e) { return 62; } } }",
            0, 62, "null.field write → catchable NullPointerException" },
          { "class T { static int f(int z){ int[] a = null;"
            "  try { return a.length; } catch (NullPointerException e) { return 63; } } }",
            0, 63, "null.length → catchable NullPointerException" },
          { "class A { int x; int m(){ return 9; } }"
            "class T { static int f(int z){ A a = null;"
            "  try { return a.m(); } catch (NullPointerException e) { return 64; } } }",
            0, 64, "null.method() → catchable NullPointerException" },
          { "class A { int x; int m(){ return 9; } }"
            "class T { static int f(int z){ A a = new A(); a.x = 7; return a.x + a.m(); } }",
            0, 16, "non-null field/dispatch undisturbed by the null guards (7+9)" },
          { "class A { int x; }"
            "class T { static int f(int z){ A a = null;"
            "  try { a.x++; return 0; } catch (NullPointerException e) { return 71; } } }",
            0, 71, "null.field++ → catchable NullPointerException" },
          { "class A { int x; }"
            "class T { static int f(int z){ A a = null;"
            "  try { a.x += 5; return 0; } catch (NullPointerException e) { return 72; } } }",
            0, 72, "null.field op= → catchable NullPointerException" },
          /* §15.10.1 (E7): a negative array size throws CATCHABLE NegativeArraySizeException. */
          { "class T { static int f(int z){"
            "  try { int[] a = new int[z]; return a.length; }"
            "  catch (NegativeArraySizeException e) { return 81; } } }",
            -5, 81, "new int[-5] → catchable NegativeArraySizeException" },
          { "class T { static int f(int z){"
            "  try { Object[] a = new Object[z]; return a.length; }"
            "  catch (NegativeArraySizeException e) { return 82; } } }",
            -1, 82, "new Object[-1] (ref array) → NegativeArraySizeException" },
          { "class T { static int f(int z){"
            "  try { int[][] a = new int[z][3]; return a.length; }"
            "  catch (NegativeArraySizeException e) { return 83; } } }",
            -2, 83, "new int[-2][3] (multi-dim) → NegativeArraySizeException" },
          { "class T { static int f(int z){ int[] a = new int[z]; return a.length; } }",
            4, 4, "new int[4] (non-negative) undisturbed by the guard" },
          /* §15.16 (E7): a bad reference cast throws CATCHABLE ClassCastException;
           * a valid cast and a null cast do NOT throw ((T)null == null). */
          { "class A {} class B {}"
            "class T { static int f(int z){ Object o = new A();"
            "  try { B b = (B)o; return 0; } catch (ClassCastException e) { return 91; } } }",
            0, 91, "bad cast (B)anA → catchable ClassCastException" },
          { "class A { int x; } class T { static int f(int z){ Object o = new A();"
            "  A a = (A)o; a.x = 5; return a.x; } }",
            0, 5, "valid cast (A)anA undisturbed by the guard" },
          { "class A { } class T { static int f(int z){ Object o = null;"
            "  A a = (A)o; return (a == null) ? 7 : 0; } }",
            0, 7, "(A)null == null (cast guard does not throw on null)" },
          { "class A {} class B {}"
            "class T { static int f(int z){ Object o = new A();"     /* CCE is a RuntimeException */
            "  try { B b = (B)o; return 0; } catch (RuntimeException e) { return 92; } } }",
            0, 92, "ClassCastException caught as RuntimeException (subtype dispatch)" },
          /* §20.22.3 StringIndexOutOfBoundsException(int) — the spec message
           * "String index out of range: " + index, built via string concat in a
           * compiled library ctor (append(int)→Integer.toString, native). Message
           * len 28, charAt(27)=='5'(53) → 28*1000+53. */
          { "class T { static int f(int z){"
            "  String m = new StringIndexOutOfBoundsException(5).getMessage();"
            "  return m.length()*1000 + m.charAt(27); } }",
            0, 28053, "StringIndexOutOfBoundsException(5).getMessage() == \"String index out of range: 5\"" },
          /* FOUNDATION: a class-typed (reference-to-class) INSTANCE FIELD — store a
           * String into a field via the ctor, read it back, call a method on it.
           * This is the path Throwable.detailMessage exercised; it had no test. */
          { "class A { String s; A(String v){ this.s = v; } }"
            "class T { static int f(int z){ A a = new A(\"hi\"); return a.s.length(); } }",
            0, 2, "class-typed (String) instance field: new A(\"hi\").s.length() == 2" },
          /* FOUNDATION: a packed sub-int (char) INSTANCE FIELD alongside an int[] in
           * one module — the exact shape Character.value exercised. */
          { "class A { char c; A(char v){ this.c = v; } }"
            "class T { static int f(int z){ int[] x = new int[2]; x[0] = 5; A a = new A('A'); return x[0] + a.c; } }",
            0, 70, "packed char instance field + int[] in one module: 5 + 'A'(65) == 70" },
          /* §20.7 Integer.parseInt (decimal/radix, sign, MIN_VALUE, overflow→NFE) +
           * §20.5 Character.digit/forDigit + the value/equals/hashCode overlays. */
          { "class T { static int f(int z){ return Integer.parseInt(\"12345\"); } }",
            0, 12345, "Integer.parseInt(\"12345\") == 12345" },
          { "class T { static int f(int z){ return Integer.parseInt(\"-2147483648\"); } }",
            0, (int32_t)0x80000000, "Integer.parseInt MIN_VALUE round-trips == 0x80000000" },
          { "class T { static int f(int z){ return Integer.parseInt(\"ff\", 16); } }",
            0, 255, "Integer.parseInt(\"ff\",16) == 255" },
          { "class T { static int f(int z){ return Integer.parseInt(\"z\", 36); } }",
            0, 35, "Integer.parseInt(\"z\",36) == 35" },
          { "class T { static int f(int z){ return Character.digit('A',16)*100 + Character.forDigit(11,16); } }",
            0, 1098, "Character.digit('A',16)==10, forDigit(11,16)=='b'(98) → 1098" },
          { "class T { static int f(int z){ Integer a = new Integer(42); return a.intValue() + a.hashCode(); } }",
            0, 84, "new Integer(42): intValue()+hashCode() == 84" },
          { "class T { static int f(int z){ return (new Integer(7).equals(new Integer(7)) ? 10 : 0)"
            "  + (new Integer(7).equals(new Integer(8)) ? 0 : 1); } }",
            0, 11, "Integer.equals: 7==7 (10) + 7!=8 (1) == 11" },
          { "class T { static int f(int z){ try { Integer.parseInt(\"xyz\"); return 0; }"
            "  catch (NumberFormatException e) { return 1; } } }",
            0, 1, "Integer.parseInt(\"xyz\") → NumberFormatException == 1" },
          { "class T { static int f(int z){ try { Integer.parseInt(\"9999999999\"); return 0; }"
            "  catch (NumberFormatException e) { return 1; } } }",
            0, 1, "Integer.parseInt overflow → NumberFormatException == 1" },
        };
        for (int i = 0; i < (int)(sizeof si / sizeof si[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, si[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(si[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == si[i].want, si[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Object monitor methods: wait/notify/notifyAll are part of Object's contract,
     * but this target has no threads/monitors, so the embedder TRAPS on them (§17 has
     * no meaning here). They dispatch through the forwarder to the host, which returns
     * a trap → the call does not complete (EXEC_TRAP). ── */
    {
        struct { const char* src; const char* label; } tr[] = {
          { "class T { static int f(int z) throws InterruptedException { Object o = new Object(); o.wait(); return 1; } }",
            "Object.wait() → embedder trap (no threads)" },
          { "class T { static int f(int z){ Object o = new Object(); o.notify(); return 1; } }",
            "Object.notify() → embedder trap (no threads)" },
          { "class T { static int f(int z){ Object o = new Object(); o.notifyAll(); return 1; } }",
            "Object.notifyAll() → embedder trap (no threads)" },
        };
        for (int i = 0; i < (int)(sizeof tr / sizeof tr[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, tr[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_TRAP, tr[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E3.5: array.length is int (JLS §10.7), switch selector consumed as i32 ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } vm[] = {
          { "class T { static int f(int x){ int[] a = new int[7]; return a.length; } }",
            0, 7, "array.length (int) == 7" },
          { "class T { static int f(int x){ char c=(char)x; switch(c){ case 65: return 1; case 66: return 2; default: return 9; } } }",
            66, 2, "switch on char selector == 2" },
          { "class T { static int f(int x){ short s=(short)x; switch(s){ case 1: return 10; case 2: return 20; default: return 0; } } }",
            2, 20, "switch on short selector == 20" },
        };
        for (int i = 0; i < (int)(sizeof vm / sizeof vm[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, vm[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(vm[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == vm[i].want, vm[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E5: exceptions (JLS §14.19 — leftmost subtype catch, finally each path,
     * rethrow on no match) — caught cases return via T.f(int)->int ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ex[] = {
          { "class Ex extends Exception {} class T { static int f(int x){ try { if (x>0) throw new Ex(); return 1; } catch (Ex e){ return 2; } } }",
            1, 2, "try/catch: typed Ex caught == 2" },
          { "class Ex extends Exception {} class T { static int f(int x){ try { if (x>0) throw new Ex(); return 1; } catch (Ex e){ return 2; } } }",
            0, 1, "try/catch: no throw → normal path == 1" },
          { "class Ex extends Exception {} class T { static int f(int x){ try { throw new Ex(); } catch (Exception e){ return 5; } } }",
            0, 5, "catch supertype: Exception catches Ex == 5" },
          /* §14.19: a catch block is reachable only if the try block can throw a type assignable to
           * its parameter, so BOTH checked types must be throwable here — `try { throw new B(); }
           * catch (A a) …` is not legal Java (A is never thrown), and javac rejects it too. */
          { "class A extends Exception {} class B extends Exception {}"
            " class T { static int f(int x){ try { if (x > 0) throw new A(); throw new B(); }"
            " catch (A a){ return 1; } catch (B b){ return 2; } } }",
            0, 2, "multi-catch: leftmost MATCHING clause wins — B skips catch(A) == 2" },
          { "class A extends Exception {} class B extends Exception {}"
            " class T { static int f(int x){ try { if (x > 0) throw new A(); throw new B(); }"
            " catch (A a){ return 1; } catch (B b){ return 2; } } }",
            1, 1, "multi-catch: first clause matches (A) == 1" },
        };
        for (int i = 0; i < (int)(sizeof ex / sizeof ex[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ex[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ex[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ex[i].want, ex[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* uncaught: thrown B not assignable to catch(A) → propagates out → trap. */
        {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            /* Both A and B are throwable from the try (else catch(A) would be an unreachable catch
             * clause, §14.19); at run time B is thrown, catch(A) does not match, and it propagates. */
            const char* src = "class A extends Exception {} class B extends Exception {} "
                "class T { static int f(int x) throws B { try { if (x > 0) throw new A(); throw new B(); }"
                " catch (A a){ return 1; } } }";
            bool ok = assemble(&a, src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_TRAP, "uncaught (B not caught by A) → propagates/traps");
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* finally runs on EVERY exit path (§14.20.2): the normal-completion path and
         * the caught-exception path both inline the finally block. */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } fin[] = {
          { "class Ex extends Exception {} class T { static int f(int x){ int r=0; "
            "try { r=1; if (x>0) throw new Ex(); } catch (Ex e){ r=2; } finally { r=r+10; } return r; } }",
            0, 11, "finally on normal path (1+10) == 11" },
          { "class Ex extends Exception {} class T { static int f(int x){ int r=0; "
            "try { r=1; if (x>0) throw new Ex(); } catch (Ex e){ r=2; } finally { r=r+10; } return r; } }",
            1, 12, "finally on caught path (2+10) == 12" },
          /* §14.18.2: "If the CATCH BLOCK completes abruptly for reason R, then the finally
           * block is executed. ... If the finally block completes normally, then the try
           * statement completes abruptly for reason R."
           *
           * The three cases above are the ones an implementation gets right by accident: the
           * try block's normal exit and its caught-exception exit both leave through the
           * try_table, whose catch-all runs the finally. A throw from inside the CATCH body has
           * already left the table, so nothing runs the finally for it — javelinac skipped it
           * entirely, and `seen` stayed 0. */
          { "class Ex extends Exception {} class T { static int seen; "
            "static int g() throws Ex { try { throw new Ex(); } catch (Ex e) { "
            "throw new RuntimeException(); } finally { seen = 7; } } "
            "static int f(int x){ seen = 0; try { g(); } catch (Throwable t) { } return seen; } }",
            0, 7, "finally runs when the CATCH completes abruptly (§14.18.2)" },
          /* ...and reason R survives a finally that completes normally: the RuntimeException
           * the catch threw is what the caller sees, not the Ex the try threw. */
          { "class Ex extends Exception {} class T { static int seen; "
            "static int g() throws Ex { try { throw new Ex(); } catch (Ex e) { "
            "throw new RuntimeException(); } finally { seen = 1; } } "
            "static int f(int x){ seen = 0; try { g(); } "
            "catch (RuntimeException r) { seen = seen + 20; } "
            "catch (Throwable t) { seen = seen + 300; } return seen; } }",
            0, 21, "the catch's reason R survives a normally-completing finally" },
        };
        for (int i = 0; i < (int)(sizeof fin / sizeof fin[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, fin[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(fin[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == fin[i].want, fin[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* finally runs while an exception PROPAGATES through it (no local catch),
         * then the exception is caught by the outer frame. Observed via a static
         * field the finally writes before the exception leaves g(). */
        {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            const char* src = "class Ex extends Exception {} class T { static int seen; "
                "static int g(int x) throws Ex { try { throw new Ex(); } finally { seen = 7; } } "
                "static int f(int x){ try { g(x); } catch (Ex e){ } return seen; } }";
            bool ok = assemble(&a, src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == 7, "finally runs on propagation (seen==7)");
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* explicit rethrow: a catch that re-throws escapes to the caller (trap here,
         * no outer handler). Exercises Throw of a caught reference. */
        {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            const char* src = "class Ex extends Exception {} class T { static int f(int x) throws Ex { "
                "try { throw new Ex(); } catch (Ex e){ throw e; } } }";
            bool ok = assemble(&a, src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_TRAP, "explicit rethrow → escapes/traps");
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── E3.5 anti-weasel: the TYPE SECTION must actually pack sub-int fields
     * (i8=0x78 / i16=0x77), not silently keep i32 (0x7f). A class with byte+int
     * fields → struct member sequence …i8 mut, i32 mut… = 0x78 0x01 0x7f 0x01. ── */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        assemble(&a, "class C { byte b; int g; } class T { static int f(int x){ return x; } }", &mod);
        const uint8_t i8field[] = { 0x78, 0x01, 0x7f, 0x01 };   /* (i8 mut)(i32 mut) */
        const uint8_t i16field[] = { 0x77, 0x01 };              /* (i16 mut) */
        CHECK(contains(mod.code, (int)bbq_vec_len(mod.code), i8field, 4),
              "binary: byte field packs to i8 storage (0x78), not i32");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
        bbq_arena a2; bbq_arena_init(&a2, 1 << 18);
        emit_wasm_ctx mod2 = {0};
        assemble(&a2, "class C { short s; char c; } class T { static int f(int x){ return x; } }", &mod2);
        CHECK(contains(mod2.code, (int)bbq_vec_len(mod2.code), i16field, 2),
              "binary: short/char field packs to i16 storage (0x77)");
        bbq_vec_free(mod2.code); bbq_arena_free(&a2);
    }

    /* ── E6: imports / host natives — a `native` method is a FUNCTION IMPORT
     * (occupying funcidx [0, nimports); defined funcs follow). The host supplies
     * it at instantiation (here exec_call_host's identity echo). ── */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        const char* src = "class T { static native int ext(int x); "
                          "static int f(int x){ return ext(x) + 1; } }";
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(41);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 42,
              "import: native ext(41) host-echoed, +1 == 42");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math: compiled abs/min/max (no externs) ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } mi[] = {
          { "class T { static int f(int x){ return Math.abs(x); } }", -5, 5, "Math.abs(-5) == 5" },
          { "class T { static int f(int x){ return Math.abs(x); } }", 5, 5, "Math.abs(5) == 5" },
          { "class T { static int f(int x){ return Math.abs(x); } }",
            (int32_t)0x80000000, (int32_t)0x80000000, "Math.abs(MIN_VALUE) overflows to MIN_VALUE" },
          { "class T { static int f(int x){ return Math.min(x, 7); } }", 3, 3, "Math.min(3,7) == 3" },
          { "class T { static int f(int x){ return Math.max(x, 7); } }", 3, 7, "Math.max(3,7) == 7" },
          { "class T { static int f(int x){ return Math.min(Math.max(x,0),10); } }", 15, 10, "Math.min/max clamp(15,0,10)==10" },
        };
        for (int i = 0; i < (int)(sizeof mi / sizeof mi[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, mi[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(mi[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == mi[i].want, mi[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        struct { const char* src; const char* fn; int argkind; int64_t ia; double da;
                 int reskind; int64_t iexp; double dexp; const char* label; } mc[] = {
          { "class T { static long f(long x){ return Math.abs(x); } }",   "T.f", 1, -9, 0, 1, 9, 0, "Math.abs(-9L) == 9L" },
          { "class T { static long f(long x){ return Math.max(x, 4L); } }","T.f", 1, 10, 0, 1, 10, 0, "Math.max(10L,4L) == 10L" },
          { "class T { static double f(double x){ return Math.abs(x); } }","T.f", 2, 0, -2.5, 2, 0, 2.5, "Math.abs(-2.5) == 2.5" },
          { "class T { static float f(float x){ return Math.abs(x); } }", "T.f", 3, 0, -1.5, 3, 0, 1.5, "Math.abs(-1.5f) == 1.5f" },
        };
        for (int i = 0; i < (int)(sizeof mc / sizeof mc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, mc[i].src, &mod);
            wasm_val_t arg;
            if      (mc[i].argkind == 1) arg = (wasm_val_t)WASM_I64_VAL(mc[i].ia);
            else if (mc[i].argkind == 3) arg = (wasm_val_t)WASM_F32_VAL((float)mc[i].da);
            else                         arg = (wasm_val_t)WASM_F64_VAL(mc[i].da);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), mc[i].fn, &arg, 1, res, 1);
            int good = ok && st == EXEC_OK;
            if (good) {
                if      (mc[i].reskind == 1) good = res[0].of.i64 == mc[i].iexp;
                else if (mc[i].reskind == 2) good = res[0].of.f64 == mc[i].dexp;
                else                         good = res[0].of.f32 == (float)mc[i].dexp;
            }
            CHECK(good, mc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.4 Boolean: value/booleanValue/equals/hashCode + ASCII parseBoolean ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bi[] = {
          { "class T { static int f(int x){ return new Boolean(true).booleanValue() ? 1 : 0; } }", 0, 1, "Boolean(true).booleanValue()" },
          { "class T { static int f(int x){ return new Boolean(false).booleanValue() ? 1 : 0; } }", 0, 0, "Boolean(false).booleanValue()" },
          { "class T { static int f(int x){ return new Boolean(true).hashCode(); } }", 0, 1231, "Boolean(true).hashCode()==1231" },
          { "class T { static int f(int x){ return new Boolean(false).hashCode(); } }", 0, 1237, "Boolean(false).hashCode()==1237" },
          { "class T { static int f(int x){ return Boolean.TRUE.booleanValue() ? 1 : 0; } }", 0, 1, "Boolean.TRUE static final (clinit)" },
          { "class T { static int f(int x){ return new Boolean(\"True\").booleanValue() ? 1 : 0; } }", 0, 1, "Boolean(\"True\") parses (case-insensitive)" },
          { "class T { static int f(int x){ return new Boolean(\"yes\").booleanValue() ? 1 : 0; } }", 0, 0, "Boolean(\"yes\") is false" },
          { "class T { static int f(int x){ return (new Boolean(true).equals(new Boolean(true)) ? 10 : 0)"
            "  + (new Boolean(true).equals(new Boolean(false)) ? 0 : 1); } }", 0, 11, "Boolean.equals: t==t + t!=f == 11" },
          { "class T { static int f(int x){ return (Boolean.valueOf(\"TRUE\").booleanValue() ? 10 : 0)"
            "  + (Boolean.valueOf(\"no\").booleanValue() ? 0 : 1); } }", 0, 11, "Boolean.valueOf returns TRUE/FALSE statics == 11" },
        };
        for (int i = 0; i < (int)(sizeof bi / sizeof bi[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bi[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bi[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == bi[i].want, bi[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.8 Long: parseLong (long accumulation), value/equals/hashCode ── */
    {
        struct { const char* src; const char* fn; int argkind; int64_t ia; double da;
                 int reskind; int64_t iexp; double dexp; const char* label; } lc[] = {
          { "class T { static long f(long z){ return Long.parseLong(\"9999999999\"); } }", "T.f", 1, 0, 0, 1, 9999999999LL, 0, "Long.parseLong(\"9999999999\")" },
          { "class T { static long f(long z){ return Long.parseLong(\"-9223372036854775808\"); } }", "T.f", 1, 0, 0, 1, (int64_t)0x8000000000000000LL, 0, "Long.parseLong MIN_VALUE round-trips" },
          { "class T { static long f(long z){ return Long.parseLong(\"ff\", 16); } }", "T.f", 1, 0, 0, 1, 255, 0, "Long.parseLong(\"ff\",16) == 255" },
          { "class T { static long f(long z){ return Long.MIN_VALUE; } }", "T.f", 1, 0, 0, 1, (int64_t)0x8000000000000000LL, 0, "Long.MIN_VALUE (0x8000..L bit-63 literal) == MIN_VALUE" },
          { "class T { static long f(long z){ return Long.parseLong(\"-100\"); } }", "T.f", 1, 0, 0, 1, -100, 0, "Long.parseLong(\"-100\") == -100" },
          { "class T { static long f(long z){ return new Long(42).longValue(); } }", "T.f", 1, 0, 0, 1, 42, 0, "new Long(42).longValue() == 42" },
        };
        for (int i = 0; i < (int)(sizeof lc / sizeof lc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, lc[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I64_VAL(lc[i].ia);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), lc[i].fn, &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i64 == lc[i].iexp, lc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        struct { const char* src; int32_t arg; int32_t want; const char* label; } li[] = {
          { "class T { static int f(int z){ Long a = new Long(7); return a.intValue() + a.hashCode(); } }", 0, 14, "new Long(7): intValue()+hashCode() == 14" },
          { "class T { static int f(int z){ return (new Long(5).equals(new Long(5)) ? 10 : 0)"
            "  + (new Long(5).equals(new Long(6)) ? 0 : 1); } }", 0, 11, "Long.equals: 5==5 + 5!=6 == 11" },
          { "class T { static int f(int z){ try { Long.parseLong(\"99999999999999999999\"); return 0; }"
            "  catch (NumberFormatException e) { return 1; } } }", 0, 1, "Long.parseLong overflow → NumberFormatException" },
        };
        for (int i = 0; i < (int)(sizeof li / sizeof li[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, li[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(li[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == li[i].want, li[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.9/§20.10 Float/Double: accessors, isNaN, isInfinite, ctors (extern-free
     * parts; equals/hashCode are compiled but call the floatToIntBits/doubleToLongBits
     * intrinsics, which need a real host, so they aren't exercised here). ── */
    {
        struct { const char* src; const char* fn; int argkind; int64_t ia; double da;
                 int reskind; int64_t iexp; double dexp; const char* label; } fc[] = {
          { "class T { static float f(float x){ return new Float(x).floatValue(); } }", "T.f", 3, 0, 1.5, 3, 0, 1.5, "new Float(1.5f).floatValue()" },
          { "class T { static float f(float x){ return new Float(2.5).floatValue(); } }", "T.f", 3, 0, 0, 3, 0, 2.5, "Float(double) ctor narrows to 2.5f" },
          { "class T { static double f(double x){ return new Double(x).doubleValue(); } }", "T.f", 2, 0, 2.5, 2, 0, 2.5, "new Double(2.5).doubleValue()" },
          { "class T { static double f(double x){ return new Double(x).floatValue(); } }", "T.f", 2, 0, 1.5, 2, 0, 1.5, "Double.floatValue widens back to 1.5" },
        };
        for (int i = 0; i < (int)(sizeof fc / sizeof fc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, fc[i].src, &mod);
            wasm_val_t arg = (fc[i].argkind == 3) ? (wasm_val_t)WASM_F32_VAL((float)fc[i].da)
                                                  : (wasm_val_t)WASM_F64_VAL(fc[i].da);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), fc[i].fn, &arg, 1, res, 1);
            int good = ok && st == EXEC_OK;
            if (good) good = (fc[i].reskind == 2) ? (res[0].of.f64 == fc[i].dexp) : (res[0].of.f32 == (float)fc[i].dexp);
            CHECK(good, fc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        struct { const char* src; int32_t arg; int32_t want; const char* label; } fi[] = {
          { "class T { static int f(int x){ return Float.isNaN(0.0f/0.0f) ? 1 : 0; } }", 0, 1, "Float.isNaN(NaN) == true" },
          { "class T { static int f(int x){ return Float.isNaN(1.0f) ? 1 : 0; } }", 0, 0, "Float.isNaN(1.0f) == false" },
          { "class T { static int f(int x){ return Float.isInfinite(1.0f/0.0f) ? 1 : 0; } }", 0, 1, "Float.isInfinite(+inf) == true" },
          { "class T { static int f(int x){ return new Float(0.0f/0.0f).isNaN() ? 1 : 0; } }", 0, 1, "new Float(NaN).isNaN() == true" },
          { "class T { static int f(int x){ return Double.isNaN(0.0/0.0) ? 1 : 0; } }", 0, 1, "Double.isNaN(NaN) == true" },
          { "class T { static int f(int x){ return Double.isInfinite(-1.0/0.0) ? 1 : 0; } }", 0, 1, "Double.isInfinite(-inf) == true" },
          { "class T { static int f(int x){ return new Double(7.9).intValue(); } }", 0, 7, "new Double(7.9).intValue() truncates to 7" },
        };
        for (int i = 0; i < (int)(sizeof fi / sizeof fi[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, fi[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(fi[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == fi[i].want, fi[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.9.6/.7, §20.10.6/.7 Float/Double equals+hashCode: the COMPILED-library-
     * method → library-NATIVE call path. hashCode/equals are compiled bodies that call
     * the native floatToIntBits/doubleToLongBits statics; the harness now supplies the
     * real reinterpret intrinsic. If the compiled-lib→native import doesn't register,
     * the module is rejected ("unknown function") → EXEC_INVALID (prints the reason). ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bc[] = {
          { "class T { static int f(int x){ return new Float(1.5f).hashCode(); } }",
            0, 1069547520, "Float.hashCode() == floatToIntBits(1.5f) == 0x3FC00000 (compiled→native)" },
          { "class T { static int f(int x){ return new Float(1.5f).equals(new Float(1.5f)) ? 1 : 0; } }",
            0, 1, "Float.equals(equal) == true (floatToIntBits compare)" },
          { "class T { static int f(int x){ return new Float(1.5f).equals(new Float(2.5f)) ? 1 : 0; } }",
            0, 0, "Float.equals(unequal) == false" },
          { "class T { static int f(int x){ return new Double(1.5).hashCode(); } }",
            0, 1073217536, "Double.hashCode() == (int)(bits^bits>>>32) == 0x3FF80000 (compiled→native)" },
          { "class T { static int f(int x){ return new Double(1.5).equals(new Double(1.5)) ? 1 : 0; } }",
            0, 1, "Double.equals(equal) == true (doubleToLongBits compare)" },
        };
        for (int i = 0; i < (int)(sizeof bc / sizeof bc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bc[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bc[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == bc[i].want, bc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.11 Math intrinsics: the genuine environment-floor natives, computed
     * FAITHFULLY by the test host (an echo would make sqrt(x)==x). Compiled Math.* →
     * native import → the libm host. Cast to int for the int-return harness. ── */
    {
        struct { const char* src; int32_t want; const char* label; } mc[] = {
          { "class T { static int f(int x){ return (int)Math.sqrt(16.0); } }", 4, "Math.sqrt(16.0) == 4.0 (host libm, NOT echo)" },
          { "class T { static int f(int x){ return (int)Math.pow(2.0, 10.0); } }", 1024, "Math.pow(2,10) == 1024" },
          { "class T { static int f(int x){ return (int)(Math.sin(0.0)*1000.0); } }", 0, "Math.sin(0) == 0.0" },
          { "class T { static int f(int x){ return (int)(Math.cos(0.0)*1000.0); } }", 1000, "Math.cos(0) == 1.0" },
          { "class T { static int f(int x){ return (int)Math.floor(3.7); } }", 3, "Math.floor(3.7) == 3.0" },
          { "class T { static int f(int x){ return (int)Math.ceil(3.2); } }", 4, "Math.ceil(3.2) == 4.0" },
          { "class T { static int f(int x){ return (int)Math.round(2.5); } }", 3, "Math.round(2.5) == 3 (floor(a+0.5), long overload)" },
          { "class T { static int f(int x){ return (int)(Math.log(Math.exp(1.0))*1000.0); } }", 1000, "Math.log(Math.exp(1)) == 1.0" },
          { "class T { static int f(int x){ return (int)(Math.atan2(1.0,1.0)*1000.0); } }", 785, "Math.atan2(1,1) == PI/4 ≈ 0.785" },
        };
        for (int i = 0; i < (int)(sizeof mc / sizeof mc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, mc[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == mc[i].want, mc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §10.8/§20.3.2 per-component array Class: getClass().getName() is the JVM array
     * descriptor. Each distinct array type has its own Class object (field-0 header set at
     * allocation); getName() reads its descriptor. ── */
    {
        struct { const char* src; int32_t want; const char* label; } ac[] = {
          { "class T { static int f(int x){ return new int[3].getClass().getName().length(); } }", 2, "int[].getName().length() == 2 (\"[I\")" },
          { "class T { static int f(int x){ return new int[3].getClass().getName().charAt(0); } }", 91, "int[].getName()[0] == '[' (91)" },
          { "class T { static int f(int x){ return new int[3].getClass().getName().charAt(1); } }", 73, "int[].getName()[1] == 'I' (73)" },
          { "class T { static int f(int x){ return new long[1].getClass().getName().charAt(1); } }", 74, "long[].getName()[1] == 'J' (74)" },
          { "class T { static int f(int x){ return new Object[2].getClass().getName().length(); } }", 19, "Object[].getName().length() == 19 (\"[Ljava.lang.Object;\")" },
          { "class T { static int f(int x){ return new Object[2].getClass().getName().charAt(1); } }", 76, "Object[].getName()[1] == 'L' (76)" },
          { "class T { static int f(int x){ return (new int[1].getClass() == new int[2].getClass()) ? 1 : 0; } }", 1, "int[].getClass() identity across allocations" },
          { "class T { static int f(int x){ return (new int[1].getClass() == new long[1].getClass()) ? 1 : 0; } }", 0, "int[].getClass() != long[].getClass()" },
          { "class T { static int f(int x){ return new int[2][3].getClass().getName().length(); } }", 3, "int[][].getName().length() == 3 (\"[[I\")" },
        };
        for (int i = 0; i < (int)(sizeof ac / sizeof ac[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ac[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ac[i].want, ac[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.12 String batch — char[]-construction methods (substring, concat,
     * replace, trim, toCharArray, getChars, valueOf(char[]/char)) ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ss[] = {
          { "class T { static int f(int x){ return \"Hello\".substring(1,4).length(); } }", 0, 3, "\"Hello\".substring(1,4).length() == 3" },
          { "class T { static int f(int x){ return \"Hello\".substring(1,4).charAt(0); } }", 0, 101, "\"Hello\".substring(1,4).charAt(0) == 'e'" },
          { "class T { static int f(int x){ return \"Hello\".substring(3).length(); } }", 0, 2, "\"Hello\".substring(3).length() == 2" },
          { "class T { static int f(int x){ String r = \"ab\".concat(\"cd\"); return r.length()*100 + r.charAt(3); } }", 0, 500, "\"ab\".concat(\"cd\") len 4, charAt(3)='d'(100) → 500" },
          { "class T { static int f(int x){ return \"a.b.c\".replace('.','-').charAt(1); } }", 0, 45, "\"a.b.c\".replace('.','-').charAt(1)=='-'(45)" },
          { "class T { static int f(int x){ return \"  hi  \".trim().length(); } }", 0, 2, "\"  hi  \".trim().length() == 2" },
          { "class T { static int f(int x){ return String.valueOf('X').charAt(0); } }", 0, 88, "String.valueOf('X').charAt(0)=='X'(88)" },
          { "class T { static int f(int x){ char[] c = \"abc\".toCharArray(); return c.length*100 + c[1]; } }", 0, 398, "\"abc\".toCharArray(): len 3, [1]='b'(98) → 398" },
          { "class T { static int f(int x){ char[] d = new char[3]; \"hello\".getChars(1,4,d,0); return d[0]*100 + d[2]; } }", 0, 10208, "\"hello\".getChars(1,4,d,0): d[0]='e'(101),d[2]='l'(108) → 10208" },
        };
        for (int i = 0; i < (int)(sizeof ss / sizeof ss[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ss[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ss[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ss[i].want, ss[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.12 String batch — scan methods (indexOf/lastIndexOf, startsWith/
     * endsWith, compareTo, regionMatches) ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } s2[] = {
          { "class T { static int f(int x){ return \"hello\".indexOf('l'); } }", 0, 2, "\"hello\".indexOf('l') == 2" },
          { "class T { static int f(int x){ return \"hello\".indexOf('l', 3); } }", 0, 3, "\"hello\".indexOf('l',3) == 3" },
          { "class T { static int f(int x){ return \"hello\".indexOf('z'); } }", 0, -1, "\"hello\".indexOf('z') == -1" },
          { "class T { static int f(int x){ return \"hello\".lastIndexOf('l'); } }", 0, 3, "\"hello\".lastIndexOf('l') == 3" },
          { "class T { static int f(int x){ return \"ababab\".indexOf(\"bab\"); } }", 0, 1, "\"ababab\".indexOf(\"bab\") == 1" },
          { "class T { static int f(int x){ return \"ababab\".lastIndexOf(\"ab\"); } }", 0, 4, "\"ababab\".lastIndexOf(\"ab\") == 4" },
          { "class T { static int f(int x){ return \"ababab\".indexOf(\"xy\"); } }", 0, -1, "\"ababab\".indexOf(\"xy\") == -1" },
          { "class T { static int f(int x){ return \"hello\".startsWith(\"he\") ? 1 : 0; } }", 0, 1, "\"hello\".startsWith(\"he\") == true" },
          { "class T { static int f(int x){ return \"hello\".startsWith(\"lo\", 3) ? 1 : 0; } }", 0, 1, "\"hello\".startsWith(\"lo\",3) == true" },
          { "class T { static int f(int x){ return \"hello\".endsWith(\"lo\") ? 1 : 0; } }", 0, 1, "\"hello\".endsWith(\"lo\") == true" },
          { "class T { static int f(int x){ return \"abc\".compareTo(\"abd\"); } }", 0, -1, "\"abc\".compareTo(\"abd\") == -1" },
          { "class T { static int f(int x){ return \"abc\".compareTo(\"ab\"); } }", 0, 1, "\"abc\".compareTo(\"ab\") == 1 (len diff)" },
          { "class T { static int f(int x){ return \"abc\".compareTo(\"abc\"); } }", 0, 0, "\"abc\".compareTo(\"abc\") == 0" },
          { "class T { static int f(int x){ return \"hello\".regionMatches(1, \"yell\", 1, 3) ? 1 : 0; } }", 0, 1, "regionMatches(1,\"yell\",1,3) matches \"ell\" == true" },
        };
        for (int i = 0; i < (int)(sizeof s2 / sizeof s2[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, s2[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(s2[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == s2[i].want, s2[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.13 StringBuffer batch SB1: ctors, append(char/String/char[]/boolean),
     * grow, toString, charAt, setCharAt, setLength ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } sb[] = {
          { "class T { static int f(int x){ return new StringBuffer().append(\"ab\").append(\"cd\").length(); } }", 0, 4, "SB append chains: length 4" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(); b.append(\"hi\"); return b.toString().length()*100 + b.toString().charAt(1); } }", 0, 305, "SB.toString() == \"hi\": len 2, charAt(1)='i'(105) → 305" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(\"x=\"); b.append('Q'); return b.charAt(2); } }", 0, 81, "SB(String)+append(char): charAt(2)='Q'(81)" },
          { "class T { static int f(int x){ return new StringBuffer().append(true).append(false).length(); } }", 0, 9, "SB append(true)+append(false): len 4+5=9" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(2); int i=0; while(i<50){ b.append('z'); i=i+1; } return b.length(); } }", 0, 50, "SB grows past initial cap(2) to 50" },
          { "class T { static int f(int x){ char[] c = new char[3]; c[0]='a'; c[1]='b'; c[2]='c'; return new StringBuffer().append(c).append(c,1,2).length(); } }", 0, 5, "SB append(char[]) + append(char[],1,2): len 3+2=5" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(\"hello\"); b.setCharAt(0,'J'); return b.charAt(0); } }", 0, 74, "SB.setCharAt(0,'J'): charAt(0)=='J'(74)" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(\"abcde\"); b.setLength(2); return b.length()*100 + b.toString().charAt(1); } }", 0, 298, "SB.setLength(2): len 2, charAt(1)='b'(98) → 298" },
        };
        for (int i = 0; i < (int)(sizeof sb / sizeof sb[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, sb[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(sb[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == sb[i].want, sb[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Bare (unqualified) instance-field write inside a VIRTUAL method: `this` is
     * typed at the root in the shared functype, so the field op must re-narrow it
     * (LoadThis), not raw LoadLocal(0). Covers simple-assign, compound, inc, super. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bf[] = {
          { "class C { int n; int bump(){ n = 5; return n; } } class T { static int f(int x){ return new C().bump(); } }", 0, 5, "bare instance-field simple assign (n=5) in virtual method" },
          { "class C { int n; int bump(){ n += 5; n++; return n; } } class T { static int f(int x){ return new C().bump(); } }", 0, 6, "bare instance-field compound+inc (n+=5; n++) == 6" },
          { "class C { int[] a; int go(){ a = new int[3]; a[1] = 9; return a[1] + a.length; } } class T { static int f(int x){ return new C().go(); } }", 0, 12, "bare ref-array field reassign + use == 12" },
          { "class A { int a; } class B extends A { int go(){ super.a = 7; return super.a; } } class T { static int f(int x){ return new B().go(); } }", 0, 7, "super.a write + read in virtual method == 7" },
        };
        for (int i = 0; i < (int)(sizeof bf / sizeof bf[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bf[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bf[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == bf[i].want, bf[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Number→String formatting cascade (pure Java, no intrinsics): Integer/Long
     * toString + radix/hex, String.valueOf(int/long/bool/Object), StringBuffer
     * append(int)/insert/reverse, Boolean.toString — verified via String.equals. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ns[] = {
          { "class T { static int f(int x){ return Integer.toString(12345).equals(\"12345\") ? 1 : 0; } }", 0, 1, "Integer.toString(12345)==\"12345\"" },
          { "class T { static int f(int x){ return Integer.toString(-2147483648).equals(\"-2147483648\") ? 1 : 0; } }", 0, 1, "Integer.toString(MIN_VALUE) (no overflow)" },
          { "class T { static int f(int x){ return Integer.toString(255, 16).equals(\"ff\") ? 1 : 0; } }", 0, 1, "Integer.toString(255,16)==\"ff\"" },
          { "class T { static int f(int x){ return Integer.toHexString(-1).equals(\"ffffffff\") ? 1 : 0; } }", 0, 1, "Integer.toHexString(-1)==\"ffffffff\"" },
          { "class T { static int f(int x){ return Integer.toBinaryString(5).equals(\"101\") ? 1 : 0; } }", 0, 1, "Integer.toBinaryString(5)==\"101\"" },
          { "class T { static int f(int x){ return Integer.parseInt(Integer.toString(-98765)) == -98765 ? 1 : 0; } }", 0, 1, "Integer toString/parseInt round-trip" },
          { "class T { static int f(int x){ return Long.toString(9999999999L).equals(\"9999999999\") ? 1 : 0; } }", 0, 1, "Long.toString(9999999999)" },
          { "class T { static int f(int x){ return Long.toString(-9223372036854775808L).equals(\"-9223372036854775808\") ? 1 : 0; } }", 0, 1, "Long.toString(MIN_VALUE)" },
          { "class T { static int f(int x){ return Long.toHexString(-1L).equals(\"ffffffffffffffff\") ? 1 : 0; } }", 0, 1, "Long.toHexString(-1)==16 f's" },
          { "class T { static int f(int x){ return Long.toString(255L, 16).equals(\"ff\") ? 1 : 0; } }", 0, 1, "Long.toString(255,16)==\"ff\" (radix path)" },
          { "class T { static int f(int x){ return Long.toString(-255L, 16).equals(\"-ff\") ? 1 : 0; } }", 0, 1, "Long.toString(-255,16)==\"-ff\"" },
          { "class T { static int f(int x){ return Long.parseLong(Long.toString(-9223372036854775808L, 36), 36) == -9223372036854775808L ? 1 : 0; } }", 0, 1, "Long.toString(MIN_VALUE,36) round-trips via parseLong" },
          { "class T { static int f(int x){ return String.valueOf(42).equals(\"42\") ? 1 : 0; } }", 0, 1, "String.valueOf(42)" },
          { "class T { static int f(int x){ return String.valueOf(true).equals(\"true\") ? 1 : 0; } }", 0, 1, "String.valueOf(true)" },
          { "class T { static int f(int x){ return new Boolean(false).toString().equals(\"false\") ? 1 : 0; } }", 0, 1, "Boolean(false).toString()" },
          { "class T { static int f(int x){ return new StringBuffer(\"x=\").append(42).toString().equals(\"x=42\") ? 1 : 0; } }", 0, 1, "SB.append(int)" },
          { "class T { static int f(int x){ return new StringBuffer(\"abc\").reverse().toString().equals(\"cba\") ? 1 : 0; } }", 0, 1, "SB.reverse()" },
          { "class T { static int f(int x){ return new StringBuffer(\"ac\").insert(1, \"b\").toString().equals(\"abc\") ? 1 : 0; } }", 0, 1, "SB.insert(1,\"b\")" },
          { "class T { static int f(int x){ String s = \"v=\" + 7 + \"!\"; return s.equals(\"v=7!\") ? 1 : 0; } }", 0, 1, "string concat \"v=\"+7+\"!\" == \"v=7!\" (payoff)" },
        };
        for (int i = 0; i < (int)(sizeof ns / sizeof ns[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ns[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ns[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ns[i].want, ns[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── JLS §15.12 overload resolution × argument widening (the arg-widening change
     * had zero coverage against overloads). Each picks the resolved overload's param
     * type — a wrong method index would mis-widen the arg. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ov[] = {
          { "class T { public static int p(int x){ return 1; } public static int p(long x){ return 2; }"
            "  static int f(int z){ return p(z)*10 + p((long)z); } }", 0, 12, "overload p(int)=1 vs p(long)=2 by static arg type (both PUBLIC → export-mangled)" },
          { "class T { public static int g(long x){ return (int)x; }"
            "  static int f(int z){ return g(z); } }", 5, 5, "single long overload: int arg widens (g(5)==5)" },
          { "class T { public static int g(long x){ return (int)(x+1); } public static int g(int x, int y){ return x+y; }"
            "  static int f(int z){ return g(z); } }", 5, 6, "different-arity overload: g(z) resolves g(long), widens == 6" },
          { "class T { public static int g(long x){ return 100; } public static int g(int x, int y){ return x+y; }"
            "  static int f(int z){ return g(z, z); } }", 5, 10, "different-arity overload: g(z,z) resolves g(int,int) == 10" },
          { "class T { public static long w(long a, int b){ return a + b; }"
            "  public static long f2(long z){ return w(z, 3); } static int f(int z){ return (int)f2((long)z); } }", 7, 10, "mixed long+int params, int arg to long+int == 10" },
          { "class C { public C(long v){ } public C(int a, int b){ } public long g(long x){ return x; } }"
            "  class T { static int f(int z){ return (int) new C(z).g(z); } }", 9, 9, "ctor overload C(long) via int arg + method widen == 9" },
        };
        for (int i = 0; i < (int)(sizeof ov / sizeof ov[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ov[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ov[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ov[i].want, ov[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── String immutability / no-aliasing (§20.12.10): constructing a String from a
     * char[] or a StringBuffer must COPY, and toString() must be independent — so
     * later mutation of the source can't corrupt the String. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } im[] = {
          { "class T { static int f(int x){ char[] c = new char[2]; c[0]='H'; c[1]='i';"
            "  String s = new String(c); c[0]='X'; return s.charAt(0); } }", 0, 'H', "new String(char[]) copies: source mutation doesn't change String" },
          { "class T { static int f(int x){ StringBuffer b = new StringBuffer(\"hi\");"
            "  String s = b.toString(); b.setCharAt(0,'X'); return s.charAt(0); } }", 0, 'h', "SB.toString() independent: buffer mutation doesn't change String" },
          { "class T { static int f(int x){ char[] c = new char[2]; c[0]='a'; c[1]='b';"
            "  StringBuffer b = new StringBuffer(new String(c)); c[0]='Z'; return b.charAt(0); } }", 0, 'a', "SB(String) copy chain independent of the original char[]" },
        };
        for (int i = 0; i < (int)(sizeof im / sizeof im[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, im[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(im[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == im[i].want, im[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §15.18.1 string concatenation (the ddcg StringBuffer defunctionalization):
     * every operand type, chaining, and left-associativity vs numeric +. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } cc[] = {
          { "class T { static int f(int x){ return (\"v=\" + 7).equals(\"v=7\") ? 1 : 0; } }", 0, 1, "String + int" },
          { "class T { static int f(int x){ return (7 + \"x\").equals(\"7x\") ? 1 : 0; } }", 0, 1, "int + String (left operand numeric)" },
          { "class T { static int f(int x){ return (\"a\" + \"b\" + \"c\").equals(\"abc\") ? 1 : 0; } }", 0, 1, "String + String + String chain" },
          { "class T { static int f(int x){ return (\"c=\" + 'X').equals(\"c=X\") ? 1 : 0; } }", 0, 1, "String + char" },
          { "class T { static int f(int x){ return (\"b=\" + true).equals(\"b=true\") ? 1 : 0; } }", 0, 1, "String + boolean" },
          { "class T { static int f(int x){ return (\"L=\" + 9999999999L).equals(\"L=9999999999\") ? 1 : 0; } }", 0, 1, "String + long" },
          { "class T { static int f(int x){ return (\"n=\" + (0-5)).equals(\"n=-5\") ? 1 : 0; } }", 0, 1, "String + negative int" },
          { "class T { static int f(int x){ return (\"x\" + null).equals(\"xnull\") ? 1 : 0; } }", 0, 1, "String + null → \"null\"" },
          { "class T { static int f(int x){ String s=\"hi\"; return (\"[\" + s + \"]\").equals(\"[hi]\") ? 1 : 0; } }", 0, 1, "String + (String var) + String" },
          { "class T { static int f(int x){ return (\"a\" + 1 + 2).equals(\"a12\") ? 1 : 0; } }", 0, 1, "LEFT-ASSOC: \"a\"+1+2 == \"a12\" (not \"a3\")" },
          { "class T { static int f(int x){ return (1 + 2 + \"x\").equals(\"3x\") ? 1 : 0; } }", 0, 1, "LEFT-ASSOC: 1+2+\"x\" == \"3x\" (numeric add then concat)" },
          { "class T { static int f(int x){ return (\"n=\" + x + \"!\").equals(\"n=42!\") ? 1 : 0; } }", 42, 1, "String + (int param) + String" },
          { "class T { static int f(int x){ return \"\".length(); } }", 0, 0, "empty string literal \"\".length() == 0 (§10.6 new char[0])" },
          { "class T { static int f(int x){ return (\"\" + x).length(); } }", 12345, 5, "\"\" + int produces the decimal digits" },
        };
        for (int i = 0; i < (int)(sizeof cc / sizeof cc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, cc[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(cc[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == cc[i].want, cc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §20.18.16 System.arraycopy — the foundational stdlib extern (array.copy intrinsic).
     * Concrete static-typed array args (what the whole stdlib passes); guards in JLS order:
     * NPE(src/dst) → IndexOutOfBounds(offsets/len). array.copy is overlap-safe (memmove). ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ac[] = {
          { "class T { static int f(int x){ int[] a=new int[3]; a[0]=10;a[1]=20;a[2]=30;"
            " int[] b=new int[3]; System.arraycopy(a,0,b,0,3); return b[0]+b[1]+b[2]; } }",
            0, 60, "arraycopy int[] full copy == 60" },
          { "class T { static int f(int x){ int[] a=new int[5]; a[1]=7;a[2]=8;"
            " int[] b=new int[5]; System.arraycopy(a,1,b,3,2); return b[3]*10+b[4]; } }",
            0, 78, "arraycopy int[] with src/dst offsets == 78" },
          { "class T { static int f(int x){ char[] a=new char[3]; a[0]='X';a[1]='Y';a[2]='Z';"
            " char[] b=new char[3]; System.arraycopy(a,0,b,0,3); return b[0]+b[1]+b[2]; } }",
            0, 267, "arraycopy char[] (i16 width) == 'X'+'Y'+'Z'" },
          { "class T { static int f(int x){ long[] a=new long[2]; a[0]=42L;a[1]=100L;"
            " long[] b=new long[2]; System.arraycopy(a,0,b,0,2); return (int)(b[0]+b[1]); } }",
            0, 142, "arraycopy long[] (i64 width) == 142" },
          { "class T { static int f(int x){ String[] a=new String[2]; a[0]=\"hi\";a[1]=\"yo\";"
            " String[] b=new String[2]; System.arraycopy(a,0,b,0,2); return b[0].length()*10+b[1].length(); } }",
            0, 22, "arraycopy String[] (ref array, covariant backing) == 22" },
          { "class T { static int f(int x){ int[] a=new int[5]; for(int i=0;i<5;i=i+1)a[i]=i;"
            " System.arraycopy(a,0,a,1,4); return a[0]*10000+a[1]*1000+a[2]*100+a[3]*10+a[4]; } }",
            0, 123, "arraycopy overlapping same-array forward (memmove) == 0,0,1,2,3" },
          { "class T { static int f(int x){ int[] a=new int[3]; try{ System.arraycopy(a,0,null,0,1); return 0; }"
            " catch(NullPointerException e){ return 1; } } }",
            0, 1, "arraycopy null literal dst → NullPointerException" },
          { "class T { static int f(int x){ int[] a=new int[3]; int[] b=new int[3];"
            " try{ System.arraycopy(a,0,b,0,5); return 0; }catch(IndexOutOfBoundsException e){ return 1; } } }",
            0, 1, "arraycopy len past dst → IndexOutOfBoundsException" },
          { "class T { static int f(int x){ int[] a=new int[3]; int[] b=new int[3];"
            " try{ System.arraycopy(a,-1,b,0,1); return 0; }catch(IndexOutOfBoundsException e){ return 1; } } }",
            0, 1, "arraycopy negative srcOffset → IndexOutOfBoundsException" },
          /* ── increment 2: erased-Object src (runtime kind dispatch) + §10.10 ArrayStore ── */
          { "class T { static int f(int x){ int[] a=new int[3]; a[0]=5;a[1]=6;a[2]=7;"
            " Object o=a; int[] b=new int[3]; System.arraycopy(o,0,b,0,3); return b[0]+b[1]+b[2]; } }",
            0, 18, "arraycopy erased-Object src (runtime int[] dispatch) == 18" },
          { "class T { static int f(int x){ int[] a=new int[2]; Object o=a; long[] c=new long[2]; Object p=c;"
            " try{ System.arraycopy(o,0,p,0,2); return 0; }catch(ArrayStoreException e){ return 1; } } }",
            0, 1, "arraycopy erased int[]→long[] incompatible → ArrayStoreException" },
          { "class T { static int f(int x){ String[] d=new String[2]; Object[] view=d;"
            " Object[] s=new Object[2]; s[0]=new Object(); s[1]=new Object();"
            " try{ System.arraycopy(s,0,view,0,2); return 0; }catch(ArrayStoreException e){ return 1; } } }",
            0, 1, "arraycopy §10.10: Object element into String[] runtime → ArrayStoreException" },
          { "class T { static int f(int x){ String[] a=new String[2]; a[0]=\"ab\";a[1]=\"cd\";"
            " Object[] d=new Object[2]; System.arraycopy(a,0,d,0,2);"
            " return ((String)d[0]).length()*10+((String)d[1]).length(); } }",
            0, 22, "arraycopy §10.10: compatible ref copy (String→Object[]) does not throw" },
          /* §20.18.16: when a ref element fails the store check at index k, the k elements BEFORE it
           * are already in the destination and nothing at or past k has been touched. A copy that
           * pre-checks the whole range (or copies nothing) passes the throw-test above but fails here. */
          { "class T { static int f(int x){ String[] d=new String[3]; Object[] view=d;"
            " Object[] s=new Object[3]; s[0]=\"ok\"; s[1]=new Object(); s[2]=\"z\";"
            " try{ System.arraycopy(s,0,view,0,3); return 0; }"
            " catch(ArrayStoreException e){ int r=0;"
            "   if (d[0]!=null && d[0].equals(\"ok\")) r=r+1;"   /* prefix [0,k) copied */
            "   if (d[1]==null) r=r+2;"                          /* the failing index untouched */
            "   if (d[2]==null) r=r+4;"                          /* nothing past it touched */
            "   return r; } } }",
            0, 7, "arraycopy §20.18.16: ArrayStore leaves the [0,k) prefix copied, k.. untouched" },
        };
        /* ── §10.8 the Class object of an array type: one `[` per dimension, then the component's
         * field descriptor (B/C/D/F/I/J/S/Z, or `L` + the binary name + `;`). §10.7 clone() is a
         * shallow copy of the same class. ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } acl[] = {
          { "class T { static int f(int x){ int r=0;"
            " int[] ai=new int[1];       if (ai.getClass().getName().equals(\"[I\")) r=r+1;"
            " byte[] ab=new byte[1];     if (ab.getClass().getName().equals(\"[B\")) r=r+2;"
            " char[] ac=new char[1];     if (ac.getClass().getName().equals(\"[C\")) r=r+4;"
            " double[] ad=new double[1]; if (ad.getClass().getName().equals(\"[D\")) r=r+8;"
            " float[] af=new float[1];   if (af.getClass().getName().equals(\"[F\")) r=r+16;"
            " long[] al=new long[1];     if (al.getClass().getName().equals(\"[J\")) r=r+32;"
            " short[] as=new short[1];   if (as.getClass().getName().equals(\"[S\")) r=r+64;"
            " boolean[] az=new boolean[1]; if (az.getClass().getName().equals(\"[Z\")) r=r+128;"
            " return r; } }",
            0, 255, "§10.8 array Class.getName(): all eight primitive component types" },
          { "class T { static int f(int x){ int r=0;"
            " Object[] ao=new Object[1]; if (ao.getClass().getName().equals(\"[Ljava.lang.Object;\")) r=r+1;"
            " String[] as=new String[1]; if (as.getClass().getName().equals(\"[Ljava.lang.String;\")) r=r+2;"
            " int[][] am=new int[2][3];  if (am.getClass().getName().equals(\"[[I\")) r=r+4;"
            " if (am[0].getClass().getName().equals(\"[I\")) r=r+8;"
            " String[][] sm=new String[1][1];"
            " if (sm.getClass().getName().equals(\"[[Ljava.lang.String;\")) r=r+16;"
            " return r; } }",
            0, 31, "§10.8 array Class.getName(): reference + multi-dimensional component types" },
          { "class T { static int f(int x){ int r=0;"
            " int[] a=new int[3]; a[0]=1;a[1]=2;a[2]=3;"
            " int[] b=(int[])a.clone(); b[0]=9;"
            " if (a[0]==1) r=r+1;"                       /* source untouched */
            " if (b[0]==9 && b[1]==2 && b[2]==3) r=r+2;" /* contents copied */
            " if (b.length==3) r=r+4;"
            " if (a!=b) r=r+8;"                          /* a distinct object */
            " if (b.getClass()==a.getClass()) r=r+16;"   /* of the same array class */
            " return r; } }",
            0, 31, "§10.7 array clone(): shallow copy, same class, distinct identity" },
        };
        for (int i = 0; i < (int)(sizeof acl / sizeof acl[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, acl[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(acl[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == acl[i].want, acl[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }

        /* ── §14.18 try-catch: the try BLOCK is everything lexically inside it, including the
         * expression of a `return` statement. An exception thrown while evaluating that expression is
         * thrown "in the try block" and must be caught by a matching handler — the abrupt return only
         * happens once the value exists. (§11.2: an unchecked exception may be thrown by any call, so
         * a `catch (RuntimeException)` is never unreachable.) ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } tryret[] = {
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ try { return g(x); } catch (RuntimeException e) { return 7; } } }",
            0, 7, "§14.18 throw from a `return` expression inside try → caught" },
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ try { return g(x); } catch (RuntimeException e) { return 7; } } }",
            1, 1, "§14.18 non-throwing `return` expression inside try → returns normally" },
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ try { return g(x); } catch (Throwable t) { return 8; } } }",
            0, 8, "§14.18 throw from a `return` expression inside try → caught by catch(Throwable)" },
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ try { g(x); return 1; } catch (RuntimeException e) { return 9; } } }",
            0, 9, "§14.18 throw from a call STATEMENT inside try → caught" },
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ int r=0; try { return g(x); } finally { r=1; } } }",
            1, 1, "§14.20 `return` expression inside try-finally: finally runs, value preserved" },
          { "class T { static int g(int x){ if (x==0) throw new RuntimeException(\"b\"); return 1; }"
            " static int f(int x){ try { return g(x) + g(x); } catch (RuntimeException e) { return 5; } } }",
            0, 5, "§14.18 throw from a compound `return` expression inside try → caught" },
        };
        /* ── §14.19 "can complete normally". The values of expressions are NOT taken into account,
         * except for a while/do/for condition that is the constant `true`. So a `for` whose body
         * always returns still completes normally when it has a non-constant condition (the loop may
         * run zero times) — and its update expression, which §14.14 makes a StatementExpressionList
         * rather than a Statement, is not an "unreachable statement" even when it can never run. ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ccn[] = {
          { "class T { static int f(int x){ for (int i = 0; i < 10; i++) { return 1; } return 0; } }",
            0, 1, "§14.19 for with a non-constant condition can complete normally (update unreachable, legal)" },
          { "class T { static int f(int x){ for (int i = 0; i < 0; i++) { return 1; } return 7; } }",
            0, 7, "§14.19 the same `for`, taken zero times, reaches the statement after it" },
          { "class T { static int f(int x){ int i = 0; while (i < 10) { return 1; } return 0; } }",
            0, 1, "§14.19 while with a non-constant condition can complete normally" },
          { "class T { static int f(int x){ for (;;) { return 3; } } }",
            0, 3, "§14.19 for(;;) cannot complete normally — nothing may follow it" },
          { "class T { static int f(int x){ while (true) { return 4; } } }",
            0, 4, "§14.19 while(true) cannot complete normally — the same rule as for(;;)" },
          { "class T { static int f(int x){ for (;;) { if (x == 0) break; return 5; } return 6; } }",
            0, 6, "§14.19 for(;;) with a reachable break CAN complete normally" },
          { "class T { static int f(int x){ if (false) { x = 3; } return 8; } }",
            0, 8, "§14.19 ACTUAL if-then rule: `if (false) x = 3;` is reachable, not an error" },
          /* §15.27: a constant expression is not merely a literal. Each condition below is a
           * constant expression with value `true`, so §14.19 says the loop cannot complete
           * normally — §8.4.7 is satisfied with no `return` after it, and no exit is built. */
          { "class T { static int f(int x){ while (1 == 1) { return 1; } } }",
            0, 1, "§15.27 while(1==1): a relational operator over literals is constant-true" },
          { "class T { static int f(int x){ while (true && !false) { return 2; } } }",
            0, 2, "§15.27 while(true && !false): && and ! are constant operators" },
          { "class T { static int f(int x){ while ((char)65 == 'A') { return 3; } } }",
            0, 3, "§15.27 while((char)65 == 'A'): a cast to a primitive type is constant" },
          { "class T { static final boolean D = true;"
            " static int f(int x){ while (D) { return 4; } } }",
            0, 4, "§15.27 while(D): a simple name for a static final variable with a constant initializer" },
          { "class T { static int f(int x){ final boolean L = true; while (L) { return 5; } } }",
            0, 5, "§15.27 while(L): a final LOCAL variable with a constant initializer" },
          { "class T { static final int N = 5;"
            " static int f(int x){ while (N > 3 ? true : false) { return 6; } } }",
            0, 6, "§15.27 the ternary conditional operator over constant operands" },
          { "class T { static int f(int x){ for (; 1 == 1;) { return 7; } } }",
            0, 7, "§15.27 for(;1==1;): the same rule as while (§14.19)" },
          { "class T { static int f(int x){ do { return 8; } while (2 > 1); } }",
            0, 8, "§15.27 do…while(2>1): constant-true tail test, no exit" },
          /* §15.27: an integer division by zero denotes NO value, so it is not a constant
           * expression. The loop keeps its test and throws at run time. */
          { "class T { static int f(int x){ final int Z = 0;"
            " try { while (1 / Z == 0) { return 0; } return 1; }"
            " catch (ArithmeticException e) { return 9; } } }",
            0, 9, "§15.27 `1/Z` with Z a constant 0 is NOT constant — evaluated, throws" },
          /* A final variable whose initializer is NOT a constant expression is not a constant
           * variable, so the loop keeps its test and can complete normally. */
          { "class T { static int g(){ return 1; }"
            " static int f(int x){ final int V = g(); int n = 0;"
            " while (V == 1) { n = n + 1; if (n > 2) break; } return n; } }",
            0, 3, "§15.27 a final variable with a non-constant initializer is not a constant" },
          /* §14.9: "If no case matches and there is no default label, then no further action is
           * taken and the switch statement completes normally" — control reaches the statement
           * after the switch. §14.19's four disjuncts omit this case (the second edition added a
           * fifth, "the switch block does not contain a default label"); taking them literally
           * would call the following statement unreachable and, once codegen believed it, would
           * drop the br_table's default target. These RUN the fall-out rather than just compile it. */
          { "class T { static int f(int x){ switch (x) { case 1: return 1; } return 99; } }",
            2, 99, "§14.9 switch, no default, no case matches → falls out to the next statement" },
          { "class T { static int f(int x){ switch (x) { case 1: return 1; } return 99; } }",
            1, 1, "§14.9 switch, no default, case matches → the group's abrupt exit wins" },
          { "class T { static int f(int x){ switch (x) { case 1: return 1; default: return 2; } } }",
            2, 2, "§14.19 switch WITH a default whose every group returns: no fall-out exists" },
          { "class T { static int f(int x){ int r = 0; switch (x) { case 1: r = 1; } return r + 10; } }",
            2, 10, "§14.19 the last group falls out of the switch (no break needed)" },
          { "class T { static int f(int x){ int r = 0;"
            " switch (x) { case 1: r = 1; break; case 2: r = 2; } return r + 20; } }",
            3, 20, "§14.19 a break exits the switch → the next statement runs" },
          /* A `Nop` in the SIR is a label anchor for a control MERGE. §14.19 decides whether the
           * merge exists, so a switch's exit and an if's join are built only when something can
           * arrive there. These pin the shapes where nothing can — and, for the switch, the
           * interlock that makes it safe: a switch that cannot complete normally necessarily has a
           * `default:` label, so the br_table's default target is never the (absent) exit anchor. */
          { "class T { static int f(int x){"
            " switch (x) { case 1: return 10; case 2: return 20; default: return 30; } } }",
            9, 30, "switch with no exit merge (default present, every group returns)" },
          { "class T { static int f(int x){ switch (x) { case 1: return 10; } return 99; } }",
            5, 99, "switch WITH an exit merge: the br_table default target survives" },
          { "class T { static int f(int x){ if (x > 0) return 1; else return 2; } }",
            -1, 2, "if-then-else with no join: both arms complete abruptly" },
          { "class T { static int f(int x){ int r = 0; if (x > 0) return 1; else r = 5; return r; } }",
            -1, 5, "if-then-else WITH a join: one arm falls through" },
          { "class T { static int f(int x){ if (x > 0) return 1; return 7; } }",
            -1, 7, "if-then always has a join (the false path reaches it)" },
          { "class T { static int f(int x){"
            " if (x > 0) { switch (x) { case 1: return 1; default: return 2; } } else return 3; } }",
            4, 2, "a switch with no exit merge nested in an if with no join" },
          { "class T { static int f(int x){ do { return 9; } while (true); } }",
            0, 9, "§14.19 do-while(true): the tail condition is an expression, not an unreachable statement" },
          /* A statement that cannot complete normally has NO successor, so codegen must not build
           * one. When it did, the dead chain carried the try's inlined `finally` — real code the
           * WASM validator then rejected (§7.6 resets reachability after every `end`, so the dead
           * chain was type-checked). Each loop form below cannot complete normally. */
          { "class T { static int g(){ return 1; }"
            " static int f(int x){ int r=0; try { for(;;) { return g(); } } finally { r=g(); } } }",
            0, 1, "§14.19 for(;;) in try-finally: no normal exit is built, module validates" },
          { "class T { static int g(){ return 1; }"
            " static int f(int x){ int r=0; try { while(true) { return g(); } } finally { r=g(); } } }",
            0, 1, "§14.19 while(true) in try-finally: no normal exit is built" },
          { "class T { static int g(){ return 1; }"
            " static int f(int x){ int r=0; try { do { return g(); } while(true); } finally { r=g(); } } }",
            0, 1, "§14.19 do-while(true) in try-finally: no normal exit is built" },
          /* §14.15: `break L` completes the labeled statement normally. For a labeled loop that
           * program point IS the loop's exit (the compiler gives them one break target), so the
           * loop's exit is live even though no unlabeled break exits it. The correctly-rounded
           * decimal parser is written exactly this way. */
          { "class T { static int f(int x){ int i=0;"
            " outer: while (true) { i = i + 1; if (i > 3) break outer; }"
            " return i; } }",
            0, 4, "§14.15/§14.19 labeled while(true) exited only by `break label` — exit is live" },
          { "class T { static int f(int x){ int i=0;"
            " outer: for (;;) { i = i + 1; if (i > 2) break outer; }"
            " return i; } }",
            0, 3, "§14.15/§14.19 labeled for(;;) exited only by `break label`" },
          { "class T { static int f(int x){ int i=0;"
            " outer: while (true) { inner: { if (x == 0) break inner; } i = 1; break outer; }"
            " return i; } }",
            0, 1, "§14.19 a break bound to a label INSIDE the loop does not make the loop's exit live" },
        };
        for (int i = 0; i < (int)(sizeof ccn / sizeof ccn[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ccn[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ccn[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ccn[i].want, ccn[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }

        for (int i = 0; i < (int)(sizeof tryret / sizeof tryret[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, tryret[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(tryret[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == tryret[i].want, tryret[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }

        /* ── The host-gated §20 library: system properties (§20.18.7-.10) and everything defined over
         * them (§20.4.10, §20.7.21-.23, §20.8.21-.23), plus §20.11.20 Math.random. None of these is a
         * host native any more — they are Java over the HostIO.getprop/propnames byte seam and over
         * java.util.Random. The harness publishes a fixed property set (exec.h `harness_props`). ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } prop[] = {
          { "class T { static int f(int x){ int r=0;"
            " if (\"1.0\".equals(System.getProperty(\"java.version\"))) r=r+1;"
            " if (System.getProperty(\"no.such.prop\") == null) r=r+2;"
            " if (\"dflt\".equals(System.getProperty(\"no.such.prop\",\"dflt\"))) r=r+4;"
            " if (\"/\".equals(System.getProperty(\"file.separator\"))) r=r+8;"
            " if (\"1.0\".equals(System.getProperty(\"java.version\",\"other\"))) r=r+16;"
            " return r; } }",
            0, 31, "§20.18.9/.10 System.getProperty: hit, miss→null, miss→default, hit-over-default" },
          { "class T { static int f(int x){ int r=0;"
            " java.util.Properties p = System.getProperties();"
            " if (p != null) r=r+1;"
            " if (\"javelina\".equals(p.getProperty(\"os.name\"))) r=r+2;"
            " java.util.Properties q = new java.util.Properties();"
            " q.put(\"k\",\"v\");"
            " System.setProperties(q);"
            " if (\"v\".equals(System.getProperty(\"k\"))) r=r+4;"
            " if (System.getProperty(\"os.name\") == null) r=r+8;"   /* the replaced set is the whole set */
            " System.setProperties(null);"
            " if (\"javelina\".equals(System.getProperty(\"os.name\"))) r=r+16;"  /* forgotten → recreated */
            " return r; } }",
            0, 31, "§20.18.7/.8 getProperties/setProperties: current set, replace, forget→recreate" },
          { "class T { static int f(int x){ int r=0;"
            " if (Boolean.getBoolean(\"test.true\")) r=r+1;"          /* value \"TrUe\", ignoring case */
            " if (!Boolean.getBoolean(\"no.such.prop\")) r=r+2;"
            " if (!Boolean.getBoolean(\"test.dec\")) r=r+4;"
            " return r; } }",
            0, 7, "§20.4.10 Boolean.getBoolean over the property set (case-insensitive \"true\")" },
          { "class T { static int f(int x){ int r=0;"
            " if (Integer.getInteger(\"test.dec\").intValue()==1234) r=r+1;"
            " if (Integer.getInteger(\"test.hex\").intValue()==42) r=r+2;"     /* 0x2a */
            " if (Integer.getInteger(\"test.hash\").intValue()==42) r=r+4;"    /* #2a  */
            " if (Integer.getInteger(\"test.oct\").intValue()==42) r=r+8;"     /* 052  */
            " if (Integer.getInteger(\"test.bad\") == null) r=r+16;"
            " if (Integer.getInteger(\"no.such.prop\") == null) r=r+32;"
            " if (Integer.getInteger(\"no.such.prop\",7).intValue()==7) r=r+64;"
            " if (Integer.getInteger(\"test.bad\",9).intValue()==9) r=r+128;"
            " if (Integer.getInteger(\"test.dec\",9).intValue()==1234) r=r+256;"
            " return r; } }",
            0, 511, "§20.7.21-.23 Integer.getInteger: decimal/hex/octal radix rules + defaults" },
          { "class T { static int f(int x){ int r=0;"
            " if (Long.getLong(\"test.dec\").longValue()==1234L) r=r+1;"
            " if (Long.getLong(\"test.hex\").longValue()==42L) r=r+2;"
            " if (Long.getLong(\"test.hash\").longValue()==42L) r=r+4;"
            " if (Long.getLong(\"test.oct\").longValue()==42L) r=r+8;"
            " if (Long.getLong(\"test.bad\") == null) r=r+16;"
            " if (Long.getLong(\"no.such.prop\",5L).longValue()==5L) r=r+32;"
            " if (Long.getLong(\"test.bad\",9L).longValue()==9L) r=r+64;"
            " return r; } }",
            0, 127, "§20.8.21-.23 Long.getLong: decimal/hex/octal radix rules + defaults" },
          { "class T { static int f(int x){ int r=0;"
            " double a = Math.random();"
            " if (a >= 0.0 && a < 1.0) r=r+1;"
            " double b = Math.random();"
            " if (b >= 0.0 && b < 1.0) r=r+2;"
            " if (a != b) r=r+4;"                     /* one generator, advanced — not a constant */
            " return r; } }",
            0, 7, "§20.11.20 Math.random: java.util.Random over the clock, in [0,1), advancing" },
        };
        /* ── §20.3 the reflection tail. `getClassLoader` is null (§20.3.7 — this model has no class
         * loaders). `forName` (§20.3.8) resolves a fully-qualified name against the whole-world AOT
         * registry the reflection bootstrap builds, and throws ClassNotFoundException otherwise.
         * `newInstance` (§20.3.6) constructs "exactly as if by a class instance creation expression
         * with an empty argument list", and throws InstantiationException where §8.1.2 / §11.5.1.2
         * say a `new` would be a compile-time error (interface, abstract class) or where no
         * no-argument constructor exists. ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } refl[] = {
          { "class T { static int f(int x){ try {"
            " Class c = Class.forName(\"java.lang.String\");"
            " return c == \"abc\".getClass() ? 1 : 0;"
            " } catch (ClassNotFoundException e) { return -1; } } }",
            0, 1, "§20.3.8 forName resolves a library class to THE Class singleton (identity)" },
          { "class Widget { } class T { static int f(int x){ try {"
            " Class c = Class.forName(\"Widget\");"
            " return c == new Widget().getClass() ? 2 : 0;"
            " } catch (ClassNotFoundException e) { return -1; } } }",
            0, 2, "§20.3.8 forName resolves a user-source class" },
          { "class T { static int f(int x){ try {"
            " Class.forName(\"no.such.Klass\"); return 0;"
            " } catch (ClassNotFoundException e) { return 3; } } }",
            0, 3, "§20.3.8 forName throws ClassNotFoundException for an unknown name" },
          { "class T { static int f(int x){ try {"
            " return Class.forName(\"java.lang.Object\").getClassLoader() == null ? 4 : 0;"
            " } catch (ClassNotFoundException e) { return -1; } } }",
            0, 4, "§20.3.7 getClassLoader is null — this model has no class loaders" },
          { "class Widget { int v = 7; } class T { static int f(int x){ try {"
            " Object o = Class.forName(\"Widget\").newInstance();"
            " return ((Widget) o).v;"
            " } catch (Exception e) { return -1; } } }",
            0, 7, "§20.3.6 newInstance runs the no-arg constructor (field initializers included)" },
          /* A user interface, referenced (so whole-world AOT registers it) by a class that
           * implements it. newInstance on it → InstantiationException (§8.1.2/§11.5.1.2). */
          { "interface Face {} class Impl implements Face {}"
            " class T { static int f(int x){ try {"
            " Face g = new Impl();"                    /* reference Face + Impl so both exist */
            " Class.forName(\"Face\").newInstance(); return 0;"
            " } catch (InstantiationException e) { return 5; }"
            " catch (Exception e) { return -1; } } }",
            0, 5, "§20.3.6 newInstance on an interface → InstantiationException (§11.5.1.2)" },
          { "class Widget { Widget(int a) {} } class T { static int f(int x){ try {"
            " Class.forName(\"Widget\").newInstance(); return 0;"
            " } catch (InstantiationException e) { return 6; }"
            " catch (Exception e) { return -1; } } }",
            0, 6, "§20.3.6 newInstance with no no-arg constructor → InstantiationException" },
        };
        for (int i = 0; i < (int)(sizeof refl / sizeof refl[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, refl[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(refl[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == refl[i].want, refl[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        for (int i = 0; i < (int)(sizeof prop / sizeof prop[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, prop[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(prop[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == prop[i].want, prop[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* ── Loop-carried-refinement ISOLATION pins: pure-Java skeletons of the property
         * path's constructs (no host seam), each computing a checkable value. These exist
         * because the optimizer's loop-carried keep miscompiled the REAL property methods
         * while every analysis-level pin stayed green — whichever skeleton reds is the
         * minimal executable reproducer, kept as a permanent regression pin. ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } keep[] = {
          { "class T { static int f(int x){"
            " if (x < 0) return -1; int s = 0;"
            " for (int i = 0; i < x; i++) s = s + 1;"
            " return s; } }",
            5, 5, "keep-iso: guard then counter loop bounded by the guarded param" },
          { "class T { static int f(int x){"
            " int n = x; if (n <= 0) return -1; int r = 0;"
            " for (int c = 0; c < 3; c++) { r = r + n; }"
            " return r; } }",
            4, 12, "keep-iso: invariant bound kept across the loop, read in the body" },
          { "class T { static int f(int x){"
            " int count = 0;"
            " for (int i = 0; i < x; i++) { if ((i & 1) == 0) count = count + 1; }"
            " return count; } }",
            5, 3, "keep-iso: conditional counter (initProperties loop 1 shape)" },
          { "class T { static int f(int x){"
            " if (x <= 0) return -1; int c = 0;"
            " for (int i = 0; i < x; i++) c = c + 1;"
            " int s = 0;"
            " for (int i = 0; i < c; i++) s = s + 2;"
            " return s; } }",
            4, 8, "keep-iso: second loop bounded by the first loop's count (loop 1→2 shape)" },
          { "class T { static int f(int x){"
            " int[] a = new int[x]; if (x <= 0) return -1;"
            " for (int i = 0; i < x; i++) a[i] = i + 1;"
            " int s = 0;"
            " for (int i = 0; i < x; i++) s = s + a[i];"
            " return s; } }",
            4, 10, "keep-iso: invariant array receiver filled then summed (names[] shape)" },
          { "class T { static int f(int x){"
            " String k = \"abcd\"; int n = k.length(); int s = 0;"
            " for (int i = 0; i < n; i++) s = s + k.charAt(i) - 'a';"
            " return s; } }",
            0, 6, "keep-iso: invariant String receiver charAt loop (key staging shape)" },
          { "class N { N next; int v; } class T { static int f(int x){"
            " N a = new N(); a.v = 1; N b = new N(); b.v = 2; N c = new N(); c.v = 4;"
            " a.next = b; b.next = c; int s = 0;"
            " for (N e = a; e != null; e = e.next) s = s + e.v;"
            " return s; } }",
            0, 7, "keep-iso: ref chain walk, loop condition is the null test (Hashtable.get shape)" },
          { "class T { static int f(int x){"
            " StringBuffer sb = new StringBuffer(); int parts = 0;"
            " for (int i = 0; i < x; i++) {"
            "   if (i == 2) { parts = parts + sb.length(); sb = new StringBuffer(); }"
            "   else sb.append('a');"
            " }"
            " return parts * 10 + sb.length(); } }",
            5, 22, "keep-iso: ref conditionally REPLACED in the loop (sb reset shape)" },
          /* The FULL initProperties body, transplanted: the host seam stubbed with an int
           * array ("ab\0c\0"), Properties.put dropped; the checksum is what the parsing
           * loops build. The CLICK_ONLY bisect named System.initProperties as the ONE
           * method whose optimization breaks the 5 property tests — this is its body as a
           * plugin snippet, so a red here is the minimal executable reproducer. */
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x;"
            " if (total <= 0) return -1;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " String[] names = new String[count];"
            " int idx = 0;"
            " StringBuffer sb = new StringBuffer();"
            " for (int i = 0; i < total; i++) {"
            "   int b = m[i];"
            "   if (b == 0) { names[idx++] = sb.toString(); sb = new StringBuffer(); }"
            "   else sb.append((char) b);"
            " }"
            " int s = count * 100;"
            " for (int i = 0; i < count; i++) s = s + names[i].length();"
            " return s; } }",
            5, 203, "keep-iso: the initProperties body transplanted (the bisect-named method)" },
          /* Reduction ladder for the red transplant — the red/green pattern across these
           * names the miscompiling construct. */
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x; if (total <= 0) return -1;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " int[] out = new int[count]; int idx = 0;"
            " for (int i = 0; i < total; i++) { int b = m[i]; if (b == 0) out[idx++] = i; }"
            " int s = count * 100;"
            " for (int i = 0; i < count; i++) s = s + out[i];"
            " return s; } }",
            5, 206, "keep-iso reduce V1: post-inc index + conditional counter, int arrays only" },
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x; if (total <= 0) return -1;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " String[] names = new String[count]; int idx = 0;"
            " for (int i = 0; i < total; i++) { int b = m[i]; if (b == 0) names[idx++] = \"kk\"; }"
            " int s = count * 100;"
            " for (int i = 0; i < count; i++) s = s + names[i].length();"
            " return s; } }",
            5, 204, "keep-iso reduce V2: ref-array store of a constant string, no StringBuffer" },
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " String[] names = new String[count];"
            " int idx = 0;"
            " StringBuffer sb = new StringBuffer();"
            " for (int i = 0; i < total; i++) {"
            "   int b = m[i];"
            "   if (b == 0) { names[idx++] = sb.toString(); sb = new StringBuffer(); }"
            "   else sb.append((char) b);"
            " }"
            " int s = count * 100;"
            " for (int i = 0; i < count; i++) s = s + names[i].length();"
            " return s; } }",
            5, 203, "keep-iso reduce V3: the transplant WITHOUT the entry guard (no kept bound)" },
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x; if (total <= 0) return -1;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " String[] names = new String[count];"
            " int idx = 0;"
            " StringBuffer sb = new StringBuffer();"
            " for (int i = 0; i < total; i++) {"
            "   int b = m[i];"
            "   if (b == 0) { names[idx] = sb.toString(); idx = idx + 1; sb = new StringBuffer(); }"
            "   else sb.append((char) b);"
            " }"
            " int s = count * 100;"
            " for (int i = 0; i < count; i++) s = s + names[i].length();"
            " return s; } }",
            5, 203, "keep-iso reduce V4: the transplant with idx++ split into store-then-increment" },
          { "class T { static int f(int x){"
            " int[] m = new int[5];"
            " m[0] = 97; m[1] = 98; m[2] = 0; m[3] = 99; m[4] = 0;"
            " int total = x; if (total <= 0) return -1;"
            " int count = 0;"
            " for (int i = 0; i < total; i++) if (m[i] == 0) count++;"
            " int acc = 0;"
            " StringBuffer sb = new StringBuffer();"
            " for (int i = 0; i < total; i++) {"
            "   int b = m[i];"
            "   if (b == 0) { acc = acc + sb.length(); sb = new StringBuffer(); }"
            "   else sb.append((char) b);"
            " }"
            " return count * 100 + acc; } }",
            5, 203, "keep-iso reduce V5: sb/toString chain with NO array store" },
        };
        /* The STATICS axis of the same isolation (the property set IS a lazily-initialized,
         * null-tested, replaceable static — none of the core shapes above has a static). */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } keeps[] = {
          { "class P { static int[] cache;"
            " static int[] get(){"
            "  if (cache == null) { cache = new int[4]; for (int i = 0; i < 4; i++) cache[i] = i + 1; }"
            "  return cache; } }"
            " class T { static int f(int x){"
            " int[] a = P.get(); int s = 0;"
            " for (int i = 0; i < a.length; i++) s = s + a[i];"
            " int[] b = P.get();"
            " return s * 10 + (a == b ? 1 : 0); } }",
            0, 101, "keep-iso: lazily-initialized null-tested static (props init shape)" },
          { "class C { static int total; } class T { static int f(int x){"
            " C.total = 0;"
            " for (int i = 0; i < x; i++) C.total = C.total + 1;"
            " int s = 0;"
            " for (int i = 0; i < C.total; i++) s = s + 2;"
            " return s; } }",
            4, 8, "keep-iso: static counter accumulated then bounding a second loop" },
          { "class Q { static int[] cur; } class T { static int f(int x){"
            " Q.cur = new int[2]; Q.cur[0] = 5; int[] old = Q.cur;"
            " Q.cur = new int[3];"
            " return old.length * 10 + Q.cur.length; } }",
            0, 23, "keep-iso: static ref replaced then re-read (setProperties shape)" },
          { "class L { static int v; static boolean done;"
            " static int get(){"
            "  if (!done) { int s = 0; for (int i = 0; i < 5; i++) s = s + i; v = s; done = true; }"
            "  return v; } }"
            " class T { static int f(int x){ return L.get() + L.get(); } }",
            0, 20, "keep-iso: lazy static computed by a loop in the initializer, read twice" },
        };
        for (int i = 0; i < (int)(sizeof keeps / sizeof keeps[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, keeps[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(keeps[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == keeps[i].want, keeps[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        for (int i = 0; i < (int)(sizeof keep / sizeof keep[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, keep[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(keep[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == keep[i].want, keep[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        /* ── Partial-escape ISOLATION pins (Stadler §5.1–5.4): pure-Java
         * skeletons of every cp_pea mechanism, checked by VALUE through the whole
         * pipeline. Each pins the SEMANTICS, not the mechanism — whichever way the
         * optimizer decides a site (virtualize, sink, decline), the value must
         * hold. ── */
        struct { const char* src; int32_t arg; int32_t want; const char* label; } pea[] = {
          { "class E { int v; } class T { static E sink;"
            " static int f(int x){ E e = new E(); e.v = x + 1;"
            "   if (x < 0) { sink = e; return sink.v; }"
            "   return e.v; } }",
            5, 6, "pea-iso: arm escape — the straight (virtual) path's value" },
          { "class E { int v; } class T { static E sink;"
            " static int f(int x){ E e = new E(); e.v = x + 1;"
            "   if (x < 0) { sink = e; return sink.v; }"
            "   return e.v; } }",
            -3, -2, "pea-iso: arm escape — the escaping arm reads the SUNK object" },
          { "class E { int v; } class T { static E sink;"
            " static int f(int x){ E e = new E(); e.v = 5;"
            "   if (x > 0) sink = e;"
            "   e.v = 7;"
            "   return (x > 0) ? sink.v : e.v; } }",
            1, 7, "pea-iso: mixed merge — a write AFTER materialization reaches the real object" },
          { "class E { int v; } class T { static E sink;"
            " static int f(int x){ E e = new E(); e.v = 5;"
            "   if (x > 0) sink = e;"
            "   e.v = 7;"
            "   return (x > 0) ? sink.v : e.v; } }",
            0, 7, "pea-iso: mixed merge — the virtual pred's value" },
          { "class E { int v; } class T {"
            " static int f(int x){ int s = 0;"
            "   for (int i = 0; i < x; i++) { E e = new E(); e.v = i; s = s + e.v; }"
            "   return s; } }",
            5, 10, "pea-iso: in-loop never-escaping alloc (§5.4 per-visit)" },
          { "class E { int v; } class T {"
            " static int f(int x){ E b = new E(); b.v = 100; int s = 0;"
            "   for (int i = 0; i < x; i++) { s = s + b.v; b = new E(); b.v = i; }"
            "   return s; } }",
            3, 101, "pea-iso: read-before-def carried around the back edge" },
          { "class E { int v; } class T {"
            " static int f(int x){ E a = new E(); a.v = 1; int s = 0;"
            "   for (int i = 0; i < x; i++) { E b = a; a = new E(); a.v = i + 2; s = s + b.v; }"
            "   return s; } }",
            3, 6, "pea-iso: a name live across the reset (the b=a shape) — value holds" },
          { "class C2 { int v; C2(int v){ this.v = v; } } class T { static C2 sink;"
            " static int f(int x){ C2 c = new C2(x + 3);"
            "   if (x > 10) { sink = c; return sink.v; }"
            "   return c.v; } }",
            5, 8, "pea-iso: explicit-ctor site, virtual path (ctor replay)" },
          { "class C2 { int v; C2(int v){ this.v = v; } } class T { static C2 sink;"
            " static int f(int x){ C2 c = new C2(x + 3);"
            "   if (x > 10) { sink = c; return sink.v; }"
            "   return c.v; } }",
            20, 23, "pea-iso: explicit-ctor site, escaping arm (materialized ctor state)" },
          { "class E { int v; } class T {"
            " static int f(int x){ E c = new E();"
            "   try { c.v = 7; if (x > 0) throw new Exception(); c.v = 9; }"
            "   catch (Exception t) { return c.v; }"
            "   return c.v + 1; } }",
            1, 7, "pea-iso: JLS §11.3.1 — the catch reads the pre-throw field state" },
          { "class E { int v; } class T {"
            " static int f(int x){ E c = new E();"
            "   try { c.v = 7; if (x > 0) throw new Exception(); c.v = 9; }"
            "   catch (Exception t) { return c.v; }"
            "   return c.v + 1; } }",
            0, 10, "pea-iso: the non-throwing path's post-try field state" },
        };
        for (int i = 0; i < (int)(sizeof pea / sizeof pea[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, pea[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(pea[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == pea[i].want, pea[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
        for (int i = 0; i < (int)(sizeof ac / sizeof ac[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ac[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ac[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ac[i].want, ac[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util — the two exceptions (prerequisite for Enumeration/Stack/iterators):
     * exist, are throwable/catchable, and are RuntimeExceptions (subtype dispatch). ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ju[] = {
          { "class T { static int f(int x){ try{ throw new NoSuchElementException(); }"
            " catch(NoSuchElementException e){ return 1; } } }",
            0, 1, "java.util.NoSuchElementException throw/catch" },
          { "class T { static int f(int x){ try{ throw new EmptyStackException(); }"
            " catch(EmptyStackException e){ return 1; } } }",
            0, 1, "java.util.EmptyStackException throw/catch" },
          { "class T { static int f(int x){ try{ throw new NoSuchElementException(); }"
            " catch(RuntimeException e){ return 1; } } }",
            0, 1, "NoSuchElementException caught as RuntimeException (subtype)" },
        };
        for (int i = 0; i < (int)(sizeof ju / sizeof ju[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ju[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ju[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ju[i].want, ju[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Enumeration + Vector (the growable Object[] collection): full public API,
     * grow, insert/remove, indexOf/contains, elements(), clone independence, and the guards. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } vv[] = {
          { "class Ctr implements Enumeration { int i; int n; Ctr(int m){ this.n=m; }"
            "  public boolean hasMoreElements(){ return this.i < this.n; }"
            "  public Object nextElement(){ this.i=this.i+1; return null; } }"
            " class T { static int f(int x){ Enumeration e=new Ctr(4); int c=0;"
            "  while(e.hasMoreElements()){ e.nextElement(); c=c+1; } return c; } }",
            0, 4, "Enumeration: user class implements + iterate == 4" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\"); v.addElement(\"b\");"
            "  v.addElement(\"c\"); return v.size(); } }",
            0, 3, "Vector addElement + size() == 3" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"x\"); v.addElement(\"y\");"
            "  return ((String)v.elementAt(1)).length() + v.size()*10; } }",
            0, 21, "Vector elementAt(1) round-trip ('y'.len 1 + size 2*10)" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\"); v.addElement(\"b\");"
            "  return v.indexOf(\"b\")*10 + (v.contains(\"a\")?1:0); } }",
            0, 11, "Vector indexOf('b')==1 *10 + contains('a')" },
          { "class T { static int f(int x){ Vector v=new Vector(); for(int i=0;i<20;i=i+1) v.addElement(\"z\");"
            "  return v.size(); } }",
            0, 20, "Vector grows past initial capacity (size==20, no trap on realloc)" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\"); Vector c=(Vector)v.clone();"
            "  c.addElement(\"b\"); return v.size()*10 + c.size(); } }",
            0, 12, "Vector clone() independence (orig=1, clone=2)" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\"); v.addElement(\"b\");"
            "  v.addElement(\"c\"); Enumeration e=v.elements(); int n=0;"
            "  while(e.hasMoreElements()){ e.nextElement(); n=n+1; } return n; } }",
            0, 3, "Vector.elements() enumerates all 3" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\"); v.addElement(\"c\");"
            "  v.insertElementAt(\"b\",1); v.removeElementAt(0);"
            "  return v.size()*100 + ((String)v.elementAt(0)).length() + (v.elementAt(1).equals(\"c\")?10:0); } }",
            0, 211, "Vector insert@1 + remove@0 → [b,c] (size 2, [0]='b'.len 1, [1]='c')" },
          { "class T { static int f(int x){ Vector v=new Vector(); try{ v.firstElement(); return 0; }"
            "  catch(NoSuchElementException e){ return 1; } } }",
            0, 1, "Vector.firstElement() on empty → NoSuchElementException" },
          { "class T { static int f(int x){ Vector v=new Vector(); v.addElement(\"a\");"
            "  try{ v.elementAt(5); return 0; }catch(ArrayIndexOutOfBoundsException e){ return 1; } } }",
            0, 1, "Vector.elementAt(oob) → ArrayIndexOutOfBoundsException" },
        };
        for (int i = 0; i < (int)(sizeof vv / sizeof vv[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, vv[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(vv[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == vv[i].want, vv[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Dictionary (abstract) + Hashtable (chained hashing, rehash, key/value
     * enumeration, clone). Keys/values are Strings (String.hashCode/equals drive the buckets). ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ht[] = {
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"a\",\"1\"); h.put(\"b\",\"2\");"
            "  return ((String)h.get(\"a\")).length() + ((String)h.get(\"b\")).length() + h.size()*10; } }",
            0, 22, "Hashtable put/get + size (1+1+2*10)" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"k\",\"v\");"
            "  return (h.containsKey(\"k\")?1:0)*10 + (h.contains(\"v\")?1:0); } }",
            0, 11, "Hashtable containsKey('k') + contains('v')" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"k\",\"old\");"
            "  String r=(String)h.put(\"k\",\"new\"); return r.length()*10 + ((String)h.get(\"k\")).length(); } }",
            0, 33, "Hashtable put overwrite returns old ('old'*10 + 'new')" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"k\",\"v\"); h.remove(\"k\");"
            "  return (h.get(\"k\")==null?1:0)*10 + h.size(); } }",
            0, 10, "Hashtable remove → get null, size 0" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(3);"
            "  for(int i=0;i<50;i=i+1){ h.put(String.valueOf(i), String.valueOf(i)); } return h.size(); } }",
            0, 50, "Hashtable rehash: 50 entries into initial-capacity-3 table" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(3);"
            "  for(int i=0;i<50;i=i+1){ h.put(String.valueOf(i), String.valueOf(i*2)); }"
            "  return Integer.parseInt((String)h.get(\"25\")); } }",
            0, 50, "Hashtable after rehash: get('25') == '50' (25*2)" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"a\",\"1\"); h.put(\"b\",\"2\");"
            "  h.put(\"c\",\"3\"); Enumeration e=h.keys(); int n=0;"
            "  while(e.hasMoreElements()){ e.nextElement(); n=n+1; } return n; } }",
            0, 3, "Hashtable.keys() enumerates all 3" },
          { "class T { static int f(int x){ Hashtable h=new Hashtable(); h.put(\"a\",\"1\");"
            "  Hashtable c=(Hashtable)h.clone(); c.put(\"b\",\"2\"); return h.size()*10 + c.size(); } }",
            0, 12, "Hashtable clone() independence (orig=1, clone=2)" },
        };
        for (int i = 0; i < (int)(sizeof ht / sizeof ht[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, ht[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ht[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && st == EXEC_OK && res[0].of.i32 == ht[i].want, ht[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Stack (extends Vector): LIFO push/pop/peek/empty/search + EmptyStackException. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } st[] = {
          { "class T { static int f(int x){ Stack s=new Stack(); s.push(new Integer(1)); s.push(new Integer(2));"
            "  s.push(new Integer(3)); return ((Integer)s.pop()).intValue()*100 + ((Integer)s.pop()).intValue()*10"
            "  + ((Integer)s.pop()).intValue(); } }",
            0, 321, "Stack LIFO: pop 3,2,1 → 321" },
          { "class T { static int f(int x){ Stack s=new Stack(); s.push(new Integer(5));"
            "  int p=((Integer)s.peek()).intValue(); return p*10 + s.size(); } }",
            0, 51, "Stack.peek() doesn't remove (5, size 1)" },
          { "class T { static int f(int x){ Stack s=new Stack();"
            "  int a = s.empty()?1:0; s.push(new Integer(1)); int b = s.empty()?0:1; return a*10+b; } }",
            0, 11, "Stack.empty() true before, false after push" },
          { "class T { static int f(int x){ Stack s=new Stack(); s.push(new Integer(1)); s.push(new Integer(2));"
            "  s.push(new Integer(3)); return s.search(new Integer(2)); } }",
            0, 2, "Stack.search(2) == 2 (distance from top)" },
          { "class T { static int f(int x){ Stack s=new Stack(); try{ s.pop(); return 0; }"
            "  catch(EmptyStackException e){ return 1; } } }",
            0, 1, "Stack.pop() on empty → EmptyStackException" },
        };
        for (int i = 0; i < (int)(sizeof st / sizeof st[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, st[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(st[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == st[i].want, st[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.StringTokenizer (implements Enumeration): tokenizing over String. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } tk[] = {
          { "class T { static int f(int x){ StringTokenizer t=new StringTokenizer(\"a b c\"); return t.countTokens(); } }",
            0, 3, "StringTokenizer default (space): countTokens == 3" },
          { "class T { static int f(int x){ StringTokenizer t=new StringTokenizer(\"a b c\"); int n=0;"
            "  while(t.hasMoreTokens()){ t.nextToken(); n=n+1; } return n; } }",
            0, 3, "StringTokenizer iterate: 3 tokens" },
          { "class T { static int f(int x){ StringTokenizer t=new StringTokenizer(\"x,y,z\", \",\");"
            "  return (t.nextToken().equals(\"x\") && t.nextToken().equals(\"y\") && t.nextToken().equals(\"z\")) ? 1 : 0; } }",
            0, 1, "StringTokenizer custom delim ',' → x,y,z" },
          { "class T { static int f(int x){ StringTokenizer t=new StringTokenizer(\"  lead   spaces  \");"
            "  return t.countTokens(); } }",
            0, 2, "StringTokenizer skips leading/multiple delims → 2 tokens" },
          { "class T { static int f(int x){ StringTokenizer t=new StringTokenizer(\"a\"); t.nextToken();"
            "  try{ t.nextToken(); return 0; }catch(NoSuchElementException e){ return 1; } } }",
            0, 1, "StringTokenizer nextToken past end → NoSuchElementException" },
        };
        for (int i = 0; i < (int)(sizeof tk / sizeof tk[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, tk[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(tk[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == tk[i].want, tk[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.BitSet: set/clear/get across word boundaries + and/or/xor + grow. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bs[] = {
          { "class T { static int f(int x){ BitSet b=new BitSet(); b.set(3); b.set(65);"
            "  return (b.get(3)?1:0)*100 + (b.get(65)?1:0)*10 + (b.get(4)?1:0); } }",
            0, 110, "BitSet set/get across word boundary (3,65 set; 4 unset)" },
          { "class T { static int f(int x){ BitSet b=new BitSet(); b.set(5); b.clear(5); return b.get(5)?0:1; } }",
            0, 1, "BitSet clear(5) → get(5) false" },
          { "class T { static int f(int x){ BitSet a=new BitSet(); a.set(1); a.set(2);"
            "  BitSet c=new BitSet(); c.set(2); c.set(3); a.and(c);"
            "  return (a.get(2)?1:0)*10 + (a.get(1)?0:1); } }",
            0, 11, "BitSet and: keeps common bit 2, drops 1" },
          { "class T { static int f(int x){ BitSet a=new BitSet(); a.set(1);"
            "  BitSet c=new BitSet(); c.set(70); a.or(c);"
            "  return (a.get(1) && a.get(70)) ? 1 : 0; } }",
            0, 1, "BitSet or: union (grows) 1 and 70" },
          { "class T { static int f(int x){ BitSet a=new BitSet(); a.set(2); a.set(3);"
            "  BitSet c=new BitSet(); c.set(3); c.set(4); a.xor(c);"
            "  return (a.get(2)?1:0)*100 + (a.get(3)?0:1)*10 + (a.get(4)?1:0); } }",
            0, 111, "BitSet xor: 2 and 4 set, 3 cancels" },
        };
        for (int i = 0; i < (int)(sizeof bs / sizeof bs[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bs[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bs[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == bs[i].want, bs[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Two codegen fixes surfaced while building java.util (both at the type-lattice authority): ──
     *  (1) a `null` literal method argument types its reference spill temp from the PARAM type
     *      (a null literal has no descriptor of its own), and
     *  (2) `((Interface) x).method()` — a cast-to-interface receiver narrows to ROOT (interfaces
     *      erase to root; the membership check is the iface_instanceof guard), not the interface's
     *      own struct type. Dispatch resolves because the implementor lives in the same module. */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } fx[] = {
          { "class T { static int g(Object o){ return o == null ? 7 : 3; }"
            "  static int f(int x){ return g(null); } }",
            0, 7, "null-literal arg to Object-param method (ref temp typed from param)" },
          { "interface I { int m(); }"
            " class A implements I { public int m(){ return 42; } }"
            " class T { static int f(int x){ Object o = new A(); return ((I) o).m(); } }",
            0, 42, "((I)o).m(): cast-to-interface receiver narrows to root + dispatches" },
        };
        for (int i = 0; i < (int)(sizeof fx / sizeof fx[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, fx[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(fx[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == fx[i].want, fx[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Observer (interface) + Observable: the jre defines Observer with NO jre
     *    implementor, so its dispatch functype comes from the synthesized abstract-functype path
     *    (unoccupied vtable slot). A plugin's Ctr implements it; dispatch matches by §3.3.10. ── */
    {
        const char* prelude =
          "class Model extends Observable { void change(){ setChanged(); notifyObservers(); } }"
          " class Ctr implements Observer { int n; public void update(Observable o, Object a){ n=n+1; } }";
        struct { const char* src; int32_t arg; int32_t want; const char* label; } ob[] = {
          { "class T { static int f(int x){ Model m=new Model(); Ctr c=new Ctr(); m.addObserver(c);"
            "  m.change(); m.change(); return c.n; } }",
            0, 2, "Observable: 2 setChanged→notify → observer.update called twice" },
          { "class T { static int f(int x){ Model m=new Model(); Ctr c=new Ctr(); m.addObserver(c);"
            "  m.notifyObservers(); return c.n; } }",
            0, 0, "Observable: notify WITHOUT setChanged → no callback" },
          { "class T { static int f(int x){ Model m=new Model(); Ctr c=new Ctr(); m.addObserver(c);"
            "  int a=m.countObservers(); m.deleteObserver(c); return a*10 + m.countObservers(); } }",
            0, 10, "Observable countObservers: 1 then delete → 0" },
        };
        for (int i = 0; i < (int)(sizeof ob / sizeof ob[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            char full[1024]; snprintf(full, sizeof full, "%s %s", prelude, ob[i].src);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, full, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(ob[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == ob[i].want, ob[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Random: the spec's exact 48-bit LCG (canonical known-answer sequence),
     *    plus nextDouble range, nextGaussian determinism (exercises do-while + Math.sqrt/log). ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } rn[] = {
          { "class T { static int f(int x){ return new Random(42).nextInt(); } }",
            0, -1170105035, "Random(42).nextInt() == -1170105035 (known-answer)" },
          { "class T { static int f(int x){ Random r = new Random(42);"
            "  return (r.nextInt() == -1170105035 && r.nextInt() == 234785527) ? 1 : 0; } }",
            0, 1, "Random(42) sequence: -1170105035, 234785527" },
          { "class T { static int f(int x){ return new Random(0).nextLong() == -4962768465676381896L ? 1 : 0; } }",
            0, 1, "Random(0).nextLong() == -4962768465676381896 (canonical)" },
          { "class T { static int f(int x){ double d = new Random(7).nextDouble();"
            "  return (d >= 0.0 && d < 1.0) ? 1 : 0; } }",
            0, 1, "Random(7).nextDouble() in [0,1)" },
          { "class T { static int f(int x){"
            "  return new Random(1).nextGaussian() == new Random(1).nextGaussian() ? 1 : 0; } }",
            0, 1, "Random(1).nextGaussian() deterministic (do-while + sqrt/log)" },
          { "class T { static int f(int x){ Random r = new Random(99); r.nextInt();"
            "  r.setSeed(42); return r.nextInt() == -1170105035 ? 1 : 0; } }",
            0, 1, "Random.setSeed(42) resets the sequence" },
        };
        for (int i = 0; i < (int)(sizeof rn / sizeof rn[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, rn[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(rn[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == rn[i].want, rn[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 java.util.Date: UTC calendar conversion (known-answer), UTC() + round-trip,
     *    comparison/equals, setters, and toGMTString→parse round-trip. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } dt[] = {
          { "class T { static int f(int x){ Date d=new Date(0L); return (d.getYear()==70 && d.getMonth()==0"
            "  && d.getDate()==1 && d.getDay()==4 && d.getHours()==0) ? 1 : 0; } }",
            0, 1, "Date(0) = 1970-01-01 Thu 00:00:00 UTC" },
          { "class T { static int f(int x){ Date d=new Date(1000000000000L);"
            "  return (d.getYear()==101 && d.getMonth()==8 && d.getDate()==9 && d.getDay()==0"
            "  && d.getHours()==1 && d.getMinutes()==46 && d.getSeconds()==40) ? 1 : 0; } }",
            0, 1, "Date(1e12) = 2001-09-09 Sun 01:46:40 UTC" },
          { "class T { static int f(int x){ return Date.UTC(95,7,12,13,30,0) == 808234200000L ? 1 : 0; } }",
            0, 1, "Date.UTC(95,Aug,12,13:30:00) == 808234200000" },
          { "class T { static int f(int x){ Date d=new Date(Date.UTC(95,7,12,13,30,0));"
            "  return (d.getYear()==95 && d.getMonth()==7 && d.getDate()==12 && d.getHours()==13"
            "  && d.getMinutes()==30) ? 1 : 0; } }",
            0, 1, "Date field ctor round-trips through UTC" },
          { "class T { static int f(int x){ return (new Date(0L).before(new Date(1L))"
            "  && new Date(1L).after(new Date(0L)) && new Date(5L).equals(new Date(5L))"
            "  && !new Date(5L).equals(new Date(6L))) ? 1 : 0; } }",
            0, 1, "Date before/after/equals" },
          { "class T { static int f(int x){ Date d=new Date(0L); d.setYear(100); d.setMonth(5); d.setDate(15);"
            "  return (d.getYear()==100 && d.getMonth()==5 && d.getDate()==15) ? 1 : 0; } }",
            0, 1, "Date setYear/setMonth/setDate" },
          { "class T { static int f(int x){ Date d=new Date(808234200000L);"
            "  return Date.parse(d.toGMTString()) == 808234200000L ? 1 : 0; } }",
            0, 1, "Date.parse(toGMTString()) round-trips" },
        };
        for (int i = 0; i < (int)(sizeof dt / sizeof dt[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, dt[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(dt[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == dt[i].want, dt[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §22 java.io exceptions: the checked IOException hierarchy, throw/catch + subclassing. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } io[] = {
          { "class T { static int f(int x){ try { throw new IOException(\"x\"); } catch(IOException e){ return 1; } } }",
            0, 1, "IOException throw/catch (checked)" },
          { "class T { static int f(int x){ try { throw new EOFException(); } catch(IOException e){ return 1; } } }",
            0, 1, "EOFException caught by IOException (subclass)" },
          { "class T { static int f(int x){ try { throw new FileNotFoundException(\"no\"); }"
            "  catch(IOException e){ return e.getMessage().equals(\"no\") ? 1 : 0; } } }",
            0, 1, "FileNotFoundException getMessage() via IOException" },
          { "class T { static int f(int x){ try { throw new UTFDataFormatException(); } catch(IOException e){ return 1; } } }",
            0, 1, "UTFDataFormatException is-a IOException" },
        };
        for (int i = 0; i < (int)(sizeof io / sizeof io[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, io[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(io[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == io[i].want, io[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §22 ByteArrayOutputStream/InputStream: in-memory byte round-trip over the abstract
     *    OutputStream/InputStream (exercises write()/read() overrides + the base read(b,off,len)/
     *    write(b,off,len) + String(byte[]) via toString). Pure Java, no host I/O. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bs[] = {
          { "class T { static int f(int x){ ByteArrayOutputStream o=new ByteArrayOutputStream();"
            "  o.write(65); o.write(66); o.write(67); byte[] b=o.toByteArray();"
            "  ByteArrayInputStream in=new ByteArrayInputStream(b);"
            "  return in.read()*10000 + in.read()*100 + in.read(); } }",
            0, 656667, "BAOS.write→toByteArray→BAIS.read round-trip (A,B,C)" },
          { "class T { static int f(int x){ ByteArrayOutputStream o=new ByteArrayOutputStream();"
            "  byte[] data = new byte[5]; data[0]=1; data[1]=2; data[2]=3; data[3]=4; data[4]=5;"
            "  o.write(data, 1, 3); return o.size()*100 + (o.toByteArray()[0] & 255); } }",
            0, 302, "BAOS.write(b,off,len) + size (3 bytes, first==2)" },
          { "class T { static int f(int x){ ByteArrayOutputStream o=new ByteArrayOutputStream();"
            "  o.write(72); o.write(105); return o.toString().equals(\"Hi\") ? 1 : 0; } }",
            0, 1, "BAOS.toString() via String(byte[]) == \"Hi\"" },
          { "class T { static int f(int x){ ByteArrayOutputStream o=new ByteArrayOutputStream(4);"
            "  int i=0; while(i<10){ o.write(i); i=i+1; } return o.size()*100 + (o.toByteArray()[9] & 255); } }",
            0, 1009, "BAOS grows past initial capacity (10 bytes, [9]==9)" },
          { "class T { static int f(int x){ byte[] b=new byte[1]; b[0]=9;"
            "  ByteArrayInputStream in=new ByteArrayInputStream(b);"
            "  int a=in.read(); int c=in.read(); return a*10 + (c==-1 ? 1 : 0); } }",
            0, 91, "BAIS read then EOF (-1)" },
          { "class T { static int f(int x){ byte[] b=new byte[3]; b[0]=1; b[1]=2; b[2]=3;"
            "  ByteArrayInputStream in=new ByteArrayInputStream(b);"
            "  in.read(); in.mark(0); in.read(); in.reset(); return in.read(); } }",
            0, 2, "BAIS mark/reset" },
        };
        for (int i = 0; i < (int)(sizeof bs / sizeof bs[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bs[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bs[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == bs[i].want, bs[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Decorator-pattern minimal repro (isolates the §22 Buffered trap in pure user code):
     *    a virtual call on a field whose static type is an ABSTRACT class, holding a concrete
     *    subclass. (A) field declared in the same class; (B) field inherited from a middle class. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } dc[] = {
          { "abstract class Bse { abstract void put(int v); }"
            " class Snk extends Bse { int last; public void put(int v){ last=v; } }"
            " class Wrp extends Bse { Bse inner; public void put(int v){ inner.put(v); } }"
            " class T { static int f(int x){ Snk s=new Snk(); Wrp w=new Wrp(); w.inner=s; w.put(9); return s.last; } }",
            0, 9, "(A) virtual call on abstract-typed field (declared in same class)" },
          { "abstract class Bs2 { abstract void put(int v); }"
            " class Snk2 extends Bs2 { int last; public void put(int v){ last=v; } }"
            " class Mid extends Bs2 { Bs2 inner; public void put(int v){ inner.put(v); } }"
            " class Wrp2 extends Mid { }"
            " class T { static int f(int x){ Snk2 s=new Snk2(); Wrp2 w=new Wrp2(); w.inner=s;"
            "  return (w.inner == s) ? 1 : 0; } }",
            0, 1, "(B) external write+read of INHERITED ref field round-trips (valid Mid)" },
          { "abstract class Bs3 { abstract void put(int v); }"
            " class Snk3 extends Bs3 { int last; public void put(int v){ last=v; } }"
            " class Mid3 extends Bs3 { Bs3 inner; public void put(int v){ inner.put(v); } }"
            " class Wrp3 extends Mid3 { public void put(int v){ inner.put(v); } }"
            " class T { static int f(int x){ Snk3 s=new Snk3(); Wrp3 w=new Wrp3(); w.inner=s; w.put(9); return s.last; } }",
            0, 9, "(C) method reads INHERITED ref field + dispatches on it (flushBuffer pattern)" },
          { "abstract class Bs4 { abstract void put(int v); }"
            " class Snk4 extends Bs4 { int last; public void put(int v){ last=v; } }"
            " class Mid4 extends Bs4 { Bs4 inner; Mid4(Bs4 in){ this.inner = in; } public void put(int v){ inner.put(v); } }"
            " class Wrp4 extends Mid4 { Wrp4(Bs4 in){ super(in); } public void put(int v){ inner.put(v); } }"
            " class T { static int f(int x){ Snk4 s=new Snk4(); Wrp4 w=new Wrp4(s); w.put(9); return s.last; } }",
            0, 9, "(D) inherited field set in super's ctor via super() + method dispatch (Buffered ctor pattern)" },
          { "abstract class Bs5 { abstract void put(byte[] b, int n); }"
            " class Snk5 extends Bs5 { int sum; public void put(byte[] b, int n){ int i=0; while(i<n){ sum=sum+b[i]; i=i+1; } } }"
            " class Mid5 extends Bs5 { Bs5 inner; Mid5(Bs5 in){ inner=in; } public void put(byte[] b, int n){ inner.put(b,n); } }"
            " class Wrp5 extends Mid5 { byte[] buf; Wrp5(Bs5 in){ super(in); buf=new byte[4]; }"
            "   void go(){ buf[0]=7; buf[1]=8; inner.put(buf, 2); } }"
            " class T { static int f(int x){ Snk5 s=new Snk5(); Wrp5 w=new Wrp5(s); w.go(); return s.sum; } }",
            0, 15, "(E) own byte[] field passed to virtual call on INHERITED ref field (out.write(buf,...))" },
          { "class T { static int f(int x) throws java.io.IOException { ByteArrayOutputStream bo=new ByteArrayOutputStream();"
            "  BufferedOutputStream out=new BufferedOutputStream(bo, 2); return 5; } }",
            0, 5, "(F) REAL BufferedOutputStream: construct only" },
          { "class T { static int f(int x) throws java.io.IOException { ByteArrayOutputStream bo=new ByteArrayOutputStream();"
            "  BufferedOutputStream out=new BufferedOutputStream(bo, 2); out.write(1); return 5; } }",
            0, 5, "(G) REAL BufferedOutputStream: construct + one write (no flush)" },
        };
        for (int i = 0; i < (int)(sizeof dc / sizeof dc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, dc[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(dc[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == dc[i].want, dc[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §22 Filter/Buffered stream decorators over ByteArray: buffered read across a refill,
     *    buffered write+flush to the sink, mark/reset within the buffer. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } bf[] = {
          { "class T { static int f(int x) throws java.io.IOException { byte[] s=new byte[3]; s[0]=10; s[1]=20; s[2]=30;"
            "  BufferedInputStream in=new BufferedInputStream(new ByteArrayInputStream(s), 2);"
            "  return in.read()*10000 + in.read()*100 + in.read(); } }",
            0, 102030, "BufferedInputStream reads across a refill (buf size 2)" },
          { "class T { static int f(int x) throws java.io.IOException { ByteArrayOutputStream bo=new ByteArrayOutputStream();"
            "  BufferedOutputStream out=new BufferedOutputStream(bo, 2);"
            "  out.write(1); out.write(2); out.write(3); out.flush();"
            "  byte[] r=bo.toByteArray(); return r.length*100 + (r[2] & 255); } }",
            0, 303, "BufferedOutputStream buffers+flushes to sink (3 bytes, [2]==3)" },
          { "class T { static int f(int x) throws java.io.IOException { byte[] s=new byte[3]; s[0]=1; s[1]=2; s[2]=3;"
            "  BufferedInputStream in=new BufferedInputStream(new ByteArrayInputStream(s));"
            "  in.read(); in.mark(10); in.read(); in.reset(); return in.read(); } }",
            0, 2, "BufferedInputStream mark/reset within buffer" },
        };
        for (int i = 0; i < (int)(sizeof bf / sizeof bf[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, bf[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(bf[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == bf[i].want, bf[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Move* bit-accessor intrinsics (JLS §20.9/§20.10) + DataInput/OutputStream over ByteArray.
     *    Spec IEEE-754 known-answers, NaN canonicalisation, per-type big-endian round-trips. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } mv[] = {
          { "class T { static int f(int x){ return Float.floatToRawIntBits(1.0f); } }",
            0, 0x3F800000, "floatToRawIntBits(1.0f) = 0x3F800000 (spec)" },
          { "class T { static int f(int x){ float inf = 1.0f/0.0f; return Float.floatToRawIntBits(inf); } }",
            0, 0x7F800000, "floatToRawIntBits(+inf) = 0x7F800000 (spec §20.9.18)" },
          { "class T { static int f(int x){ float ninf = -1.0f/0.0f; return Float.floatToRawIntBits(ninf); } }",
            0, (int32_t)0xFF800000, "floatToRawIntBits(-inf) = 0xFF800000 (spec)" },
          { "class T { static int f(int x){ return Float.floatToRawIntBits(Float.intBitsToFloat(0x12345678)) == 0x12345678 ? 1 : 0; } }",
            0, 1, "float raw bits round-trip (intBitsToFloat inverse of floatToRawIntBits)" },
          { "class T { static int f(int x){ float nan = Float.intBitsToFloat(0x7F800001); return Float.floatToIntBits(nan); } }",
            0, 0x7FC00000, "floatToIntBits canonicalises NaN to 0x7FC00000 (spec §20.9.18)" },
          { "class T { static int f(int x){ float nan = Float.intBitsToFloat(0x7F800001); return Float.floatToRawIntBits(nan); } }",
            0, 0x7F800001, "floatToRawIntBits preserves the raw NaN bits" },
          { "class T { static int f(int x){ return Double.doubleToRawLongBits(1.0) == 0x3FF0000000000000L ? 1 : 0; } }",
            0, 1, "doubleToRawLongBits(1.0) = 0x3FF0000000000000 (spec)" },
          { "class T { static int f(int x){ return Double.doubleToRawLongBits(Double.longBitsToDouble(0x123456789ABCDEF0L)) == 0x123456789ABCDEF0L ? 1 : 0; } }",
            0, 1, "double raw bits round-trip" },
          { "class T { static int f(int x){ return (Double.doubleToLongBits(0.0/0.0) == 0x7FF8000000000000L) ? 1 : 0; } }",
            0, 1, "doubleToLongBits canonicalises NaN to 0x7FF8000000000000 (spec §20.10.17)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); DataOutputStream d=new DataOutputStream(bo);"
            "  d.writeInt(123456789); d.writeShort(1000); d.writeLong(9000000000L);"
            "  DataInputStream di=new DataInputStream(new ByteArrayInputStream(bo.toByteArray()));"
            "  return (di.readInt()==123456789 && di.readShort()==1000 && di.readLong()==9000000000L)?1:0; } }",
            0, 1, "DataStream int/short/long big-endian round-trip" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); DataOutputStream d=new DataOutputStream(bo);"
            "  d.writeFloat(3.5f); d.writeDouble(2.25);"
            "  DataInputStream di=new DataInputStream(new ByteArrayInputStream(bo.toByteArray()));"
            "  return (di.readFloat()==3.5f && di.readDouble()==2.25)?1:0; } }",
            0, 1, "DataStream float/double round-trip (via Move intrinsics)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); DataOutputStream d=new DataOutputStream(bo);"
            "  d.writeBoolean(true); d.writeByte(200); d.writeChar('Z');"
            "  DataInputStream di=new DataInputStream(new ByteArrayInputStream(bo.toByteArray()));"
            "  return (di.readBoolean() && di.readUnsignedByte()==200 && di.readChar()=='Z')?1:0; } }",
            0, 1, "DataStream boolean/unsignedByte/char round-trip" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  String s = \"A\" + (char)233 + \"B\";"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); DataOutputStream d=new DataOutputStream(bo);"
            "  d.writeUTF(s);"
            "  DataInputStream di=new DataInputStream(new ByteArrayInputStream(bo.toByteArray()));"
            "  return di.readUTF().equals(s)?1:0; } }",
            0, 1, "DataStream writeUTF/readUTF round-trip (1- and 2-byte UTF-8)" },
        };
        for (int i = 0; i < (int)(sizeof mv / sizeof mv[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, mv[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(mv[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == mv[i].want, mv[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §22 pure-Java InputStream overlays: Pushback (unread), StringBuffer (String low bytes),
     *    LineNumber (CR/CRLF/LF → one '\n', line count). All over ByteArray, no host. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } sv[] = {
          { "class T { static int f(int x) throws java.io.IOException {"
            "  byte[] s=new byte[3]; s[0]=10; s[1]=20; s[2]=30;"
            "  PushbackInputStream p=new PushbackInputStream(new ByteArrayInputStream(s));"
            "  int a=p.read(); p.unread(a); int b=p.read(); int c=p.read();"
            "  return a*10000 + b*100 + c; } }",
            0, 101020, "PushbackInputStream unread + re-read (a=10, unread, b=10, c=20)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  StringBufferInputStream sb=new StringBufferInputStream(\"ABC\");"
            "  return sb.read()*10000 + sb.read()*100 + sb.read(); } }",
            0, 65*10000 + 66*100 + 67, "StringBufferInputStream reads String low bytes (A,B,C)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  byte[] s=new byte[5]; s[0]=65; s[1]=13; s[2]=10; s[3]=66; s[4]=10;"
            "  LineNumberInputStream ln=new LineNumberInputStream(new ByteArrayInputStream(s));"
            "  int c1=ln.read(); int c2=ln.read(); int c3=ln.read(); int c4=ln.read(); int c5=ln.read();"
            "  return ln.getLineNumber()*1000 + c1*10 + ((c2=='\\n' && c4=='\\n' && c5==-1)?1:0); } }",
            0, 2651, "LineNumberInputStream: CRLF+LF = 2 lines, terminators -> '\\n', EOF" },
          { "interface If { int id(); } class Cf implements If { public int id(){ return 42; } }"
            " class Boxf { If x; Boxf(){} }"
            " class T { static int f(int n){ Boxf b=new Boxf();"    // interface field default = null (was ref.null $If, invalid)
            "  if (b.x != null) return 1; b.x = new Cf(); return b.x.id(); } }",
            0, 42, "interface-typed field: new (null default) + PutField + GetField + dispatch (§3.3 struct.new)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  byte[] a=new byte[2]; a[0]=1; a[1]=2; byte[] b=new byte[2]; b[0]=3; b[1]=4;"
            "  SequenceInputStream s=new SequenceInputStream(new ByteArrayInputStream(a), new ByteArrayInputStream(b));"
            "  int r1=s.read(); int r2=s.read(); int r3=s.read(); int r4=s.read(); int r5=s.read();"
            "  return r1*10000 + r2*1000 + r3*100 + r4*10 + (r5==-1?1:0); } }",
            0, 12341, "SequenceInputStream concatenates two streams (Enumeration field; reads 1,2,3,4,EOF)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); PrintStream p=new PrintStream(bo);"
            "  p.print(42); p.print('/'); p.println(true);"
            "  byte[] r=bo.toByteArray();"
            "  return (r.length==8 && r[0]=='4' && r[2]=='/' && r[3]=='t' && r[7]=='\\n')?1:0; } }",
            0, 1, "PrintStream print(int)/print(char)/println(boolean) -> '42/true\\n'" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  ByteArrayOutputStream bo=new ByteArrayOutputStream(); PrintStream p=new PrintStream(bo);"
            "  p.print(\"x=\"); p.println(9000000000L);"
            "  return bo.toByteArray().length; } }",
            0, 13, "PrintStream print(String)/println(long) -> 'x=9000000000\\n' (13 bytes)" },
        };
        for (int i = 0; i < (int)(sizeof sv / sizeof sv[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, sv[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(sv[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == sv[i].want, sv[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── §21 Properties over ByteArray streams (unblocked by java.io): load (=/: seps, # comment,
     *    empty value, missing-key default; \ continuation + \uXXXX); save→load round-trip. ── */
    {
        struct { const char* src; int32_t arg; int32_t want; const char* label; } pr[] = {
          { "class T { static int f(int x) throws java.io.IOException {"
            "  String d = \"x=hello\\ny : world\\n#comment\\nempty=\\n\";"
            "  byte[] by = new byte[d.length()]; for (int i=0;i<d.length();i++) by[i]=(byte)d.charAt(i);"
            "  Properties p = new Properties(); p.load(new ByteArrayInputStream(by));"
            "  return (p.getProperty(\"x\").equals(\"hello\") && p.getProperty(\"y\").equals(\"world\")"
            "    && p.getProperty(\"empty\").equals(\"\") && p.getProperty(\"z\",\"def\").equals(\"def\")) ? 1 : 0; } }",
            0, 1, "Properties.load: = and : separators, # comment, empty value, getProperty default" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  StringBuffer sb = new StringBuffer(); sb.append(\"k=a\"); sb.append((char)92); sb.append('\\n');"
            "  sb.append(\"b\"); sb.append('\\n'); sb.append(\"u=\"); sb.append((char)92); sb.append(\"u0041\"); sb.append('\\n');"
            "  String d = sb.toString(); byte[] by = new byte[d.length()]; for (int i=0;i<d.length();i++) by[i]=(byte)d.charAt(i);"
            "  Properties p = new Properties(); p.load(new ByteArrayInputStream(by));"
            "  return (p.getProperty(\"k\").equals(\"ab\") && p.getProperty(\"u\").equals(\"A\")) ? 1 : 0; } }",
            0, 1, "Properties.load: \\ line-continuation + \\uXXXX escape (loadConvert)" },
          { "class T { static int f(int x) throws java.io.IOException {"
            "  Properties p = new Properties(); p.put(\"k\",\"v\"); p.put(\"eq\",\"a=b\");"
            "  ByteArrayOutputStream bo = new ByteArrayOutputStream(); p.save(bo, \"hdr\");"
            "  Properties q = new Properties(); q.load(new ByteArrayInputStream(bo.toByteArray()));"
            "  return (q.getProperty(\"k\").equals(\"v\") && q.getProperty(\"eq\").equals(\"a=b\")) ? 1 : 0; } }",
            0, 1, "Properties.save -> load round-trip (escaped '=' in value survives)" },
        };
        for (int i = 0; i < (int)(sizeof pr / sizeof pr[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            bool ok = assemble(&a, pr[i].src, &mod);
            wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(pr[i].arg);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
            CHECK(ok && stx == EXEC_OK && res[0].of.i32 == pr[i].want, pr[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    /* ── Host I/O floor foundation: the GC-byte[]↔linear-memory bounce primitives. Mem.store8/load8
     *    lower to i32.store8 / i32.load8_u over the emitted staging memory. Round-trip incl. an
     *    unsigned byte (200 > 127 must zero-extend) and a runtime value. ── */
    {
        const char* src = "class T { static int f(int x){"
            "  Mem.i32_store8(5, 65); Mem.i32_store8(6, 200); Mem.i32_store8(7, x & 255);"
            "  return Mem.i32_load8_u(5)*100000 + Mem.i32_load8_u(6)*100 + Mem.i32_load8_u(7); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(42);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 65*100000 + 200*100 + 42,
              "Mem.store8/load8 round-trip through linear memory (i32.store8 / i32.load8_u, unsigned)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Host reaches the guest's staging bytes: guest writes to the I/O memory (Mem.store8), the host
     *    native reads them via wasm_memory_data (HostIO.checksum) — the host half of the fd bounce. ── */
    {
        const char* src = "class T { static int f(int x){"
            "  Mem.i32_store8(0, 10); Mem.i32_store8(1, 20); Mem.i32_store8(2, 200); return HostIO.checksum(0, 3); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 10 + 20 + 200,
              "host native reads the guest's staging memory (Mem.store8 -> HostIO.checksum via wasm_memory_data)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Full host file round-trip: guest stages bytes -> fd_write to a real temp file -> rewind ->
     *    fd_read back into a different memory region -> guest reads them. Exercises the whole bounce
     *    (guest↔memory via Mem, memory↔file via the host fd natives over wasm_memory_data). ── */
    {
        const char* src = "class T { static int f(int x){"
            "  int fd = HostIO.fd_open_temp();"
            "  Mem.i32_store8(0, 65); Mem.i32_store8(1, 66); Mem.i32_store8(2, 67);"
            "  HostIO.fd_write(fd, 0, 3);"
            "  HostIO.fd_seek(fd, 0);"
            "  int n = HostIO.fd_read(fd, 10, 3);"
            "  HostIO.fd_close(fd);"
            "  return n*1000000 + Mem.i32_load8_u(10)*10000 + Mem.i32_load8_u(11)*100 + Mem.i32_load8_u(12); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 3*1000000 + 65*10000 + 66*100 + 67,
              "host file round-trip: fd_write -> temp file -> fd_read back through the staging memory");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── The real File stream API end to end: FileOutputStream(name).write(byte[]) -> a host file ->
     *    FileInputStream(name).read(byte[]) reads it back. Exercises FileDescriptor/open + the byte[]
     *    bounce in the actual java.io classes (not just the raw HostIO natives). ── */
    {
        const char* src = "class T { static int f(int x) throws java.io.IOException {"
            "  java.io.FileOutputStream o = new java.io.FileOutputStream(\"rt\");"
            "  byte[] data = new byte[5];"
            "  data[0]=10; data[1]=20; data[2]=30; data[3]=40; data[4]=50;"
            "  o.write(data); o.close();"
            "  java.io.FileInputStream in = new java.io.FileInputStream(\"rt\");"
            "  byte[] buf = new byte[8];"
            "  int n = in.read(buf); in.close();"
            "  int sum = 0; for (int i = 0; i < n; i++) sum += (buf[i] & 255);"
            "  return n * 1000 + sum; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 5 * 1000 + (10 + 20 + 30 + 40 + 50),
              "FileOutputStream.write -> file -> FileInputStream.read round-trip (real java.io API)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §22.4 java.io.File: the path abstraction over the host stat/action floor. Write a file, then
     *    query it (exists/isFile/isDirectory/length/getName/canRead), delete it, and mkdir/isDirectory/
     *    delete a directory. Each bit is one spec-method assertion; all set → 2047. ── */
    {
        const char* src = "class T { static int f(int x) throws java.io.IOException {"
            "  java.io.FileOutputStream o = new java.io.FileOutputStream(\"ftest\");"
            "  byte[] d = new byte[4]; d[0]=1; d[1]=2; d[2]=3; d[3]=4; o.write(d); o.close();"
            "  java.io.File f = new java.io.File(\"ftest\");"
            "  int r = 0;"
            "  if (f.exists()) r += 1;"
            "  if (f.isFile()) r += 2;"
            "  if (!f.isDirectory()) r += 4;"
            "  if (f.length() == 4) r += 8;"
            "  if (f.getName().equals(\"ftest\")) r += 16;"
            "  if (f.canRead()) r += 32;"
            "  if (f.delete()) r += 64;"
            "  if (!f.exists()) r += 128;"
            "  java.io.File dir = new java.io.File(\"dtest\");"
            "  if (dir.mkdir()) r += 256;"
            "  if (dir.isDirectory()) r += 512;"
            "  if (dir.delete()) r += 1024;"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 2047,
              "java.io.File: exists/isFile/isDirectory/length/getName/canRead/delete + mkdir (§22.4)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §22.4 java.io.File nesting: mkdirs (recursive), getName/getParent decomposition, list a
     *    directory's entries, and renameTo — over the real nested-path host floor. All bits → 511. ── */
    {
        const char* src = "class T { static int f(int x) throws java.io.IOException {"
            "  int r = 0;"
            "  java.io.File d = new java.io.File(\"a/b/c\");"
            "  if (d.mkdirs()) r += 1;"                                     /* recursive mkdir */
            "  if (d.isDirectory()) r += 2;"
            "  if (new java.io.File(\"a/b\").isDirectory()) r += 4;"        /* parent created by mkdirs */
            "  java.io.File f = new java.io.File(\"a/b\", \"hello.txt\");"   /* two-arg ctor joins with separator */
            "  if (f.getName().equals(\"hello.txt\")) r += 8;"
            "  if (f.getParent().equals(\"a/b\")) r += 16;"
            "  java.io.FileOutputStream o = new java.io.FileOutputStream(f.getPath());"
            "  byte[] data = new byte[2]; data[0]=65; data[1]=66; o.write(data); o.close();"
            "  if (f.exists()) r += 32;"
            "  String[] names = new java.io.File(\"a/b\").list();"          /* directory listing */
            "  boolean found = false;"
            "  for (int i = 0; i < names.length; i++) if (names[i].equals(\"hello.txt\")) found = true;"
            "  if (found) r += 64;"
            "  java.io.File g = new java.io.File(\"a/b/world.txt\");"
            "  if (f.renameTo(g)) r += 128;"
            "  if (g.exists() && !f.exists()) r += 256;"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 511,
              "java.io.File: mkdirs/getParent/list/renameTo over nested paths (§22.4)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §22.4 java.io.RandomAccessFile: write DataOutput primitives, then seek back and read them
     *    via DataInput; check length/getFilePointer/seek/skipBytes. All bits → 511. ── */
    {
        const char* src = "class T { static int f(int x) throws java.io.IOException {"
            "  int r = 0;"
            "  java.io.RandomAccessFile raf = new java.io.RandomAccessFile(\"raftest\", \"rw\");"
            "  raf.writeInt(0x01020304);"                        /* 4 bytes */
            "  raf.writeLong(0x1122334455667788L);"              /* 8 bytes */
            "  raf.writeUTF(\"hi\");"                            /* 2 len + 2 chars = 4 bytes */
            "  raf.writeByte(99);"                               /* 1 byte → total 17 */
            "  if (raf.length() == 17) r += 1;"
            "  if (raf.getFilePointer() == 17) r += 2;"
            "  raf.seek(0);"
            "  if (raf.readInt() == 0x01020304) r += 4;"
            "  if (raf.readLong() == 0x1122334455667788L) r += 8;"
            "  if (raf.readUTF().equals(\"hi\")) r += 16;"
            "  if (raf.readUnsignedByte() == 99) r += 32;"
            "  raf.seek(4);"
            "  if (raf.getFilePointer() == 4) r += 64;"
            "  if (raf.readLong() == 0x1122334455667788L) r += 128;"   /* random re-read after seek */
            "  if (raf.skipBytes(2) == 2) r += 256;"
            "  raf.close();"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 511,
              "java.io.RandomAccessFile: write/seek/read DataInput+DataOutput primitives (§22.4)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.22.6 Throwable.printStackTrace(PrintStream): routed through a ByteArrayOutputStream so
     *    the header (toString) is asserted directly (no VM-exposed frames yet). ── */
    {
        const char* src = "class T { static int f(int x) throws java.io.IOException {"
            "  java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream();"
            "  java.io.PrintStream ps = new java.io.PrintStream(baos);"
            "  Throwable t = new RuntimeException(\"boom\");"
            "  t.printStackTrace(ps); ps.flush();"
            "  String out = baos.toString();"
            "  if (out.startsWith(\"java.lang.RuntimeException: boom\")) return 1;"
            "  return 0; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 1,
              "Throwable.printStackTrace(PrintStream): header to the stream (§20.22.6)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Immix large-object space: arrays past a block's data area (~4k elems at the 8-byte in-heap
     *    stride) formerly TRAPPED (no LOS); now they allocate, survive a GC while live, and their
     *    contents are intact. `a` stays live across churn that allocates + drops many large arrays. ── */
    {
        const char* src = "class T { static int f(int x){"
            "  int[] a = new int[10000];"                         /* 10000*8 + hdr ≈ 80 KB → LOS (was a trap) */
            "  a[0] = 7; a[9999] = 11; a[5000] = 13;"
            "  for (int i = 0; i < 100; i++) { int[] junk = new int[5000]; junk[0] = i; }"   /* LOS churn while `a` is live */
            "  return a[0] + a[9999] + a[5000] + (a.length / 1000); } }";                     /* 7+11+13+10 = 41 */
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 41,
              "large array (10000 elems) allocates + survives GC via the large-object space (no trap)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Qualified name in expression position (JLS §6.5.2): a package-qualified static field access
     *    resolves + emits (java.lang.Integer.MAX_VALUE). ── */
    {
        const char* src = "class T { static int f(int x){ return java.lang.Integer.MAX_VALUE; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 2147483647,
              "package-qualified static field access java.lang.Integer.MAX_VALUE == 2147483647");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── System.out wiring: System.out (PrintStream over FileOutputStream(FileDescriptor.out=fd 1)) writes
     *    to the fd-1 capture; read it back to confirm the bytes reached the host sink. ── */
    {
        const char* src = "class T { static int f(int x){"
            "  System.out.print(\"Hi!\");"
            "  HostIO.fd_seek(1, 0);"
            "  int n = HostIO.fd_read(1, 0, 8);"
            "  return n*1000000 + Mem.i32_load8_u(0)*10000 + Mem.i32_load8_u(1)*100 + Mem.i32_load8_u(2); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 3*1000000 + 'H'*10000 + 'i'*100 + '!',
              "System.out.print -> fd 1 -> host sink (PrintStream over FileOutputStream(FileDescriptor.out))");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS §12.4.1 lazy class init (the spec's own Super/One/Two example): `new Two()` initializes its
     *    SUPERclass Sup first then Two, and the UNUSED class One is NEVER initialized. A shared static
     *    counter records init order (Sup=1, Two=2, One=9); `One o = null` is not an active use. ── */
    {
        const char* src =
            "class Marker { static int log; }"
            "class Sup { static { Marker.log = Marker.log * 10 + 1; } }"
            "class Two extends Sup { static { Marker.log = Marker.log * 10 + 2; } }"
            "class One { static int probe(){ return 7; } static { Marker.log = Marker.log * 10 + 9; } }"
            "class TT { static int f(int x){ Two t = new Two();"
            "    if (x > 0) { return One.probe(); }"   /* references One (compiled, not pruned) but x=0 → never taken → One never inits */
            "    return Marker.log; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 12,
              "JLS 12.4.1: new Two() inits Super-then-Two lazily; One (referenced but not reached) NEVER inits (log==12, no 9)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS §12.4.1: a COMPOUND assignment to a static field is an active use — inits its class. C's
     *    static block sets M.log=7; `C.s += 5` (the only use of C) must trigger it (log stays 0 otherwise). ── */
    {
        const char* src =
            "class M { static int log; }"
            "class C { static int s; static { M.log = 7; } }"
            "class TT { static int f(int x){ C.s += 5; return M.log; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 7,
              "JLS 12.4.1: compound-assign `C.s += 5` triggers C init (log==7)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS 1.0 §12.4.1: creating an array of element class T is an active use of T. `new E[4]` (the only
     *    use of E) must trigger E's static block (M.log=3); log stays 0 otherwise. (Note: later JLS editions
     *    dropped this; JLS 1.0 — which governs here — keeps it.) ── */
    {
        const char* src =
            "class M { static int log; }"
            "class E { static int v; static { M.log = 3; } }"
            "class TT { static int f(int x){ E[] a = new E[4]; return M.log; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 3,
              "JLS 1.0 12.4.1: new E[4] (array of class E) triggers E init (log==3)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS §12.4.2 steps 5/10/11: a THROWING static initializer. Boom's static block divides by zero
     *    (ArithmeticException, a non-Error) → step 10/11 wrap it in ExceptionInInitializerError and mark Boom
     *    ERRONEOUS. A SECOND active use of the erroneous Boom → step 5 throws NoClassDefFoundError.
     *    r == 42 (40 from the EIIE catch + 2 from the NCDFE catch) proves both. ── */
    {
        const char* src =
            "class Boom { static int v; static { v = 1; int z = 5 / (v - 1); } }"
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    try { int z = Boom.v; } catch (ExceptionInInitializerError e) { r = r + 40; }"
            "    try { int z = Boom.v; } catch (NoClassDefFoundError e) { r = r + 2; }"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 42,
              "JLS 12.4.2 steps 5/10/11: throwing init → ExceptionInInitializerError, re-use → NoClassDefFoundError (r==42)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── A `null`-only local of an otherwise-unused class: `One o = null` carries no ref descriptor of its
     *    own, so the slot's WASM type must come from the DECLARED type (sema's stored resolution). Regression
     *    for the empty-error plugin rejection (slot ref typeidx was never threaded). ── */
    {
        const char* src = "class One {} class TT { static int f(int x){ One o = null; return 5; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 5,
              "null-only local of an unused class: One o = null (slot typed from declared type)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.23: ExceptionInInitializerError.getException() returns the wrapped cause. ── */
    {
        const char* src =
            "class Boom2 { static int v; static { v = 1; int z = 7 / (v - 1); } }"
            "class TT { static int g(int x){"
            "    try { int z = Boom2.v; return 0; }"
            "    catch (ExceptionInInitializerError e) {"
            "        Throwable c = e.getException();"
            "        return c == null ? -1 : (c instanceof ArithmeticException ? 5 : 9); } } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.g", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 5,
              "EIIE.getException() returns the wrapped ArithmeticException (not null)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §12.4.2 step 7: a SUPERCLASS init failure is rethrown as the SAME exception — the subclass must
     *    NOT re-wrap it in a second ExceptionInInitializerError. Base's block throws (5/0 → wrapped once);
     *    `new Derived()` must surface that single EIIE, whose cause is the ArithmeticException, not an EIIE. ── */
    {
        const char* src =
            "class Base { static int v; static { v = 1; int z = 5 / (v - 1); } }"
            "class Derived extends Base {}"
            "class TT { static int f(int x){"
            "    try { Derived d = new Derived(); return 0; }"
            "    catch (ExceptionInInitializerError e) {"
            "        Throwable c = e.getException();"
            "        return c instanceof ExceptionInInitializerError ? 2 : 1; } } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 1,
              "JLS 12.4.2 step 7: super-init failure rethrows the SAME exception (no double-wrap)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS 1.0 §12.4.1 (the spec's I/J/K example): using KK.j (a non-constant field DECLARED in JJ)
     *    initializes JJ only — NOT its superinterface II, and NOT KK. Interface init does not chain to
     *    superinterfaces. M.log records init order (II=1, JJ=3, KK=5); expect just 3. ── */
    {
        const char* src =
            "class M { static int log; }"
            "class M2 { static int rec(int v){ M.log = M.log * 10 + v; return v; } }"
            "interface II { int i = M2.rec(1); }"
            "interface JJ extends II { int j = M2.rec(3); }"
            "interface KK extends JJ { int k = M2.rec(5); }"
            "class TT { static int f(int x){ int z = KK.j; return M.log; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 3,
              "JLS 12.4.1: KK.j (inherited from super-iface JJ) inits JJ only, not II nor KK (log==3)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── null-literal slot typing at the OTHER spill sites (same class of bug as the local-decl fix): a
     *    `return null` inside a try/finally spills the return value to a ref temp, and a ternary null arm
     *    spills to the result temp — both must type the slot from context, not the (descriptor-less) null. ── */
    {
        const char* src =
            "class Foo {}"
            "class TT { static Foo g(int x){ try { return null; } finally { int q = x; } }"
            "    static int f(int x){ Foo z = g(0); Foo w = (x > 0) ? new Foo() : null; return 5; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 5,
              "null at finally-return + ternary-arm spill sites: slot typed from context (validates)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── C-style declarator dims: `Foo o[] = null` — the declared type is Foo[], so sema_var_type must fold
     *    the declarator dims (else the null slot is typed Foo, not Foo[]). Plus inherited class static
     *    (B2.a reads A2's global via the declaring-class fix). ── */
    {
        const char* src =
            "class Foo {}"
            "class A2 { static int a; static { a = 9; } }"
            "class B2 extends A2 {}"
            "class TT { static int f(int x){ Foo o[] = null; return B2.a + (o == null ? 0 : 1); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 9,
              "C-style dims `Foo o[] = null` validate + inherited static B2.a reads A2's global (==9)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── JLS §5.1.2/§5.1.3 mixed-width primitive conversions (L2I wrap, I2L sign-extend, D2I trunc, I2D,
     *    F2I via arithmetic) — audit-verify the (D) dependency is real, not just node-defined. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    long big = 5000000000L;"          /* > INT_MAX */
            "    int a = (int) big;"               /* L2I wrap → 705032704 */
            "    int b = (int)((long)(x - 1) + 1L);" /* I2L then L2I: (x=0) → (long)-1 + 1 = 0 */
            "    int c = (int) 3.9;"               /* D2I trunc → 3 */
            "    double d = (double) 7;"           /* I2D → 7.0 */
            "    int e = (int) d;"                 /* D2I → 7 */
            "    float g = (float) 2.5;"           /* → 2.5f */
            "    int h = (int)(g * 2.0f);"         /* F2I → 5 */
            "    return (a==705032704?1:0)+(b==0?2:0)+(c==3?4:0)+(e==7?8:0)+(h==5?16:0); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 31,
              "JLS 5.1 mixed-width conversions L2I/I2L/D2I/I2D/F2I (==31)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §12.5 host super-chain: a USER class extending a LIBRARY class that declares ctors — construction
     *    must run super() up through the library ctor chain (Exception→Throwable→Object). Audit the (D) item. ── */
    {
        const char* src =
            "class Sub extends Exception {"
            "    int tag;"
            "    Sub(){ super(\"boom\"); tag = 7; }"
            "}"
            "class TT { static int f(int x){ Sub s = new Sub(); return s.tag + (s.getMessage() == null ? 0 : 100); } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 107,
              "§12.5 host super-chain: user class extends Exception, super(msg) runs (tag==7, message set)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.9 Math + §20.12 String: verified-working library computation (regression). (§20.5 Character
     *    classification/case is a SEPARATE, verified gap — all native stubs — tracked in the plan's E6.rest.) ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.abs(-7) == 7 && Math.abs(5) == 5) r |= 1;"
            "    if (Math.sqrt(16.0) == 4.0 && Math.pow(2.0,10.0) == 1024.0) r |= 2;"
            "    if (Math.floor(3.7) == 3.0 && Math.ceil(3.2) == 4.0) r |= 4;"
            "    if (new String(\"hi\").length() == 2) r |= 8;"
            "    if (\"abc\".indexOf('b') == 1 && \"hello\".substring(1,3).length() == 2) r |= 16;"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 31,
              "Math (abs/sqrt/pow/floor/ceil) + String (ctor/length/indexOf/substring) compute correctly (==31)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.5 Character — now implemented via the generated CharacterData (UCD range tables). Positive +
     *    NEGATIVE cases, and NON-ASCII (Greek/Cyrillic/symbol) to prove it's real Unicode, not ASCII. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Character.isDigit('5') && !Character.isDigit('x')) r |= 1;"
            "    if (Character.isLetter('x') && !Character.isLetter('5')) r |= 2;"
            "    if (Character.toUpperCase('a') == 'A' && Character.toLowerCase('B') == 'b') r |= 4;"
            "    if (Character.toUpperCase((char)0x3B1) == (char)0x391 && Character.toLowerCase((char)0x391) == (char)0x3B1) r |= 8;"  /* Greek α↔Α */
            "    if (Character.isLetter((char)0x5D0) && Character.isLetter((char)0x627) && Character.isLetter((char)0x4E00)) r |= 16;"  /* Hebrew, Arabic, CJK */
            "    if (Character.isLetter((char)0xAC00) && Character.isLetter((char)0xE01) && Character.isLetter((char)0x905)) r |= 32;"  /* Hangul, Thai, Devanagari */
            "    if (Character.isDigit((char)0x660) && Character.isDigit((char)0x966)) r |= 64;"                       /* Arabic-Indic, Devanagari digits */
            "    if (!Character.isLetter((char)0x2603) && !Character.isDigit((char)0x2603)) r |= 128;"                 /* snowman: neither */
            "    if (Character.isDefined((char)0x41) && !Character.isDefined((char)0x378)) r |= 256;"                  /* A defined; U+0378 unassigned */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 511,
              "§20.5 Character: classification/case across 7 scripts + non-ASCII digits + isDefined (Unicode, ==511)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.5 Character FULL-RANGE cross-check: loop every code point 0x0000..0xFFFF through the generated
     *    classifier and reproduce the generator's oracle checksum. Not sampled examples — the WHOLE domain,
     *    so a tree-emission bug in any script/range fails here. Constant printed by gen_character.c. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int h = 0;"
            "    for (int c = 0; c < 0x10000; c++) {"
            "        h = h*31 + (Character.isLetter((char)c) ? 1 : 0);"
            "        h = h*31 + (Character.isDigit((char)c) ? 1 : 0);"
            "        h = h*31 + (int)Character.toUpperCase((char)c);"
            "        h = h*31 + (int)Character.toLowerCase((char)c);"
            "    }"
            "    return h; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == -418477434,
              "§20.5 Character FULL BMP (0x0000-0xFFFF) matches the UCD oracle checksum");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math f64 intrinsics: sqrt/floor/ceil/rint lower to f64.sqrt/floor/ceil/nearest (NOT a host
     *    import). The HOP libm backing for these is DELETED from exec.h, so passing PROVES the intrinsic. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.sqrt(16.0) == 4.0 && Math.sqrt(2.0) > 1.4142 && Math.sqrt(2.0) < 1.4143) r |= 1;"
            "    if (Math.floor(3.7) == 3.0 && Math.floor(-2.5) == -3.0) r |= 2;"
            "    if (Math.ceil(3.2) == 4.0 && Math.ceil(-2.5) == -2.0) r |= 4;"
            "    if (Math.rint(2.5) == 2.0 && Math.rint(3.5) == 4.0) r |= 8;"   /* round-half-to-even (f64.nearest) */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 15,
              "§20.11 Math.sqrt/floor/ceil/rint via f64 opcode intrinsic (no host libm, ==15)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.12 String methods now implemented in Java over the real Character (no host echo): case
     *    mapping (incl. Greek), equalsIgnoreCase, getBytes, intern pool. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (\"Hello\".toLowerCase().equals(\"hello\")) r |= 1;"
            "    if (\"Hello\".toUpperCase().equals(\"HELLO\")) r |= 2;"
            "    if (\"ABC\".equalsIgnoreCase(\"abc\") && !\"ABC\".equalsIgnoreCase(\"abd\")) r |= 4;"
            "    char[] ca = new char[1]; ca[0] = (char)0x3b1;"                        /* Greek α */
            "    if (new String(ca).toUpperCase().charAt(0) == (char)0x391) r |= 8;"   /* → Α */
            "    byte[] b = new byte[4];"
            "    \"Hi\".getBytes(0, 2, b, 0);"
            "    if (b[0] == 'H' && b[1] == 'i') r |= 16;"
            "    String s1 = new String(\"pool\"); String s2 = new String(\"pool\");"
            "    if (s1.intern() == s2.intern() && s1 != s2) r |= 32;"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 63,
              "§20.12 String toLowerCase/toUpperCase(+Greek)/equalsIgnoreCase/getBytes/intern (==63)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.min/max(float,double) in Java (JDK algorithm) — NaN propagation + signed-zero
     *    tie-break, the edge cases a naive impl / echo gets wrong. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.min(3.0, 5.0) == 3.0 && Math.max(3.0, 5.0) == 5.0) r |= 1;"
            "    if (Math.min(3.0f, 5.0f) == 3.0f && Math.max(3.0f, 5.0f) == 5.0f) r |= 2;"
            "    if (Double.doubleToRawLongBits(Math.min(-0.0, 0.0)) == 0x8000000000000000L) r |= 4;"  /* -0.0 */
            "    if (Double.doubleToRawLongBits(Math.max(-0.0, 0.0)) == 0L) r |= 8;"                    /* +0.0 */
            "    double nan = ((double)x) / ((double)x);"                                               /* x=0 → NaN */
            "    if (Math.min(nan, 1.0) != Math.min(nan, 1.0)) r |= 16;"                                /* NaN propagates */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 31,
              "§20.11 Math.min/max(float,double): NaN + signed-zero correct (==31)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.round in Java over the floor intrinsic (HOP backing deleted). ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.round(2.4) == 2L && Math.round(2.5) == 3L && Math.round(-2.5) == -2L) r |= 1;"
            "    if (Math.round(2.5f) == 3 && Math.round(2.4f) == 2) r |= 2;"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 3,
              "§20.11 Math.round(float,double) via floor intrinsic (==3)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.exp — faithful fdlibm port (OpenJDK FdLibm.Exp), HOP libm backing DELETED. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.exp(0.0) == 1.0) r |= 1;"
            "    double e1 = Math.exp(1.0); if (e1 > 2.7182818284590 && e1 < 2.7182818284591) r |= 2;"
            "    double e2 = Math.exp(2.0); if (e2 > 7.3890560989306 && e2 < 7.3890560989307) r |= 4;"
            "    double em = Math.exp(-1.0); if (em > 0.36787944117144 && em < 0.36787944117145) r |= 8;"
            "    if (Math.exp(1000.0) > 1.0e308) r |= 16;"    /* overflow → +inf */
            "    if (Math.exp(-1000.0) == 0.0) r |= 32;"      /* underflow → 0 */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 63,
              "§20.11 Math.exp fdlibm port: exp(0/1/2/-1) + overflow/underflow (==63)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Backend TILE COVERAGE: every JLS operator × every operand type must have a burg
     *    tile. Enumerated by construction, NOT sampled through whatever the library happens
     *    to use: `~long` was only ever exercised because BitSet used it, and `float %` /
     *    `double %` (no f32.rem/f64.rem opcode in WASM) were used NOWHERE in the jre, so a
     *    library-wide compile never touched them — they silently emitted a truncated body.
     *    A missing tile now fails the assemble loudly, so this sweep is the gate. ── */
    {
        static const char* types[] = { "byte", "short", "char", "int", "long", "float", "double" };
        static const char* arith[] = { "+", "-", "*", "/", "%" };
        static const char* bitwise[]= { "&", "|", "^", "<<", ">>", ">>>" };
        static const char* cmp[]   = { "==", "!=", "<", "<=", ">", ">=" };
        int holes = 0;
        for (int t = 0; t < 7; t++) {
            bool integral = (t < 5);                      /* byte..long */
            /* Binary numeric promotion (§5.6.2): byte/short/char compute as int. */
            const char* res = (t < 4) ? "int" : types[t];
            char src[512];
            for (int o = 0; o < 5; o++) {                 /* arithmetic incl. % */
                snprintf(src, sizeof src, "class TT { static %s f(%s a, %s b){ return a %s b; } }",
                         res, types[t], types[t], arith[o]);
                bbq_arena a; bbq_arena_init(&a, 1 << 16); emit_wasm_ctx m = {0};
                if (!assemble(&a, src, &m)) { holes++; printf("    no tile: %s %s %s\n", types[t], arith[o], types[t]); }
                bbq_vec_free(m.code); bbq_arena_free(&a);
            }
            if (integral) for (int o = 0; o < 6; o++) {    /* bitwise + shifts */
                snprintf(src, sizeof src, "class TT { static %s f(%s a, %s b){ return a %s b; } }",
                         res, types[t], types[t], bitwise[o]);
                bbq_arena a; bbq_arena_init(&a, 1 << 16); emit_wasm_ctx m = {0};
                if (!assemble(&a, src, &m)) { holes++; printf("    no tile: %s %s %s\n", types[t], bitwise[o], types[t]); }
                bbq_vec_free(m.code); bbq_arena_free(&a);
            }
            for (int o = 0; o < 6; o++) {                 /* comparisons */
                snprintf(src, sizeof src, "class TT { static boolean f(%s a, %s b){ return a %s b; } }",
                         types[t], types[t], cmp[o]);
                bbq_arena a; bbq_arena_init(&a, 1 << 16); emit_wasm_ctx m = {0};
                if (!assemble(&a, src, &m)) { holes++; printf("    no tile: %s %s %s\n", types[t], cmp[o], types[t]); }
                bbq_vec_free(m.code); bbq_arena_free(&a);
            }
            {                                             /* unary - and (integral) ~ */
                snprintf(src, sizeof src, "class TT { static %s f(%s a){ return -a; } }", res, types[t]);
                bbq_arena a; bbq_arena_init(&a, 1 << 16); emit_wasm_ctx m = {0};
                if (!assemble(&a, src, &m)) { holes++; printf("    no tile: unary - %s\n", types[t]); }
                bbq_vec_free(m.code); bbq_arena_free(&a);
            }
            if (integral) {
                snprintf(src, sizeof src, "class TT { static %s f(%s a){ return ~a; } }", res, types[t]);
                bbq_arena a; bbq_arena_init(&a, 1 << 16); emit_wasm_ctx m = {0};
                if (!assemble(&a, src, &m)) { holes++; printf("    no tile: ~%s\n", types[t]); }
                bbq_vec_free(m.code); bbq_arena_free(&a);
            }
        }
        CHECK(holes == 0, "backend tile coverage: every operator x operand type assembles "
                          "(no silently-untileable op)");
    }

    /* ── §15.17.3 floating-point remainder. `%` on float/double is the TRUNCATED remainder
     *    (C fmod), NOT Math.IEEEremainder: the result takes the sign of the DIVIDEND and
     *    r = a - (b * q) with q = a/b truncated toward zero. WASM has no f32.rem/f64.rem,
     *    so this lowers to the fdlibm fmod already in java.lang.Math. Every operand class
     *    the spec enumerates, incl. the ones that must NOT throw (a float `% 0.0` is NaN,
     *    where an integer `% 0` throws ArithmeticException). ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (7.5 % 2.0 == 1.5) r |= 1;"                                     /* basic */
            "    double n = -7.5; if (n % 2.0 == -1.5) r |= 2;"                     /* sign of the DIVIDEND */
            "    double p = 7.5;  if (p % -2.0 == 1.5) r |= 4;"                     /* divisor sign irrelevant */
            "    double z = 5.0;  double zero = 0.0; double q = z % zero;"
            "    if (q != q) r |= 8;"                                               /* x % 0.0 = NaN, no throw */
            "    double inf = 1.0/0.0; double w = 5.0 % inf; if (w == 5.0) r |= 16;"/* finite % inf = dividend */
            "    double v = inf % 2.0; if (v != v) r |= 32;"                        /* inf % finite = NaN */
            "    float  fa = 7.5f; float fb = 2.0f; if (fa % fb == 1.5f) r |= 64;"  /* float path */
            "    float  fn = -7.5f; if (fn % fb == -1.5f) r |= 128;"
            "    double m = 5.5; double mm = m % 2.0;"
            "    if (mm > 1.4999 && mm < 1.5001) r |= 256;"                         /* non-constant operands */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 511,
              "§15.17.3 float/double %% : truncated remainder, sign of dividend, "
              "NaN on zero/inf divisor (no ArithmeticException) (==511)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.pow — faithful fdlibm port (OpenJDK FdLibm.Pow), HOP libm backing DELETED. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.pow(2.0,-1.0) == 0.5 && Math.pow(5.0,0.0) == 1.0 && Math.pow(7.0,1.0) == 7.0) r |= 1;"
            "    if (Math.pow(4.0,0.5) == 2.0) r |= 2;"                            /* y==0.5 sqrt path */
            "    double a = Math.pow(2.0,10.0); if (a > 1023.9999 && a < 1024.0001) r |= 4;"
            "    double b = Math.pow(3.0,3.0); if (b > 26.9999 && b < 27.0001) r |= 8;"
            "    double c = Math.pow(-2.0,3.0); if (c > -8.0001 && c < -7.9999) r |= 16;"   /* odd exp → negative */
            "    double d = Math.pow(-2.0,2.0); if (d > 3.9999 && d < 4.0001) r |= 32;"     /* even exp → positive */
            "    double e = Math.pow(2.0,0.5); if (e > 1.41421356 && e < 1.41421357) r |= 64;"
            "    double g = Math.pow(10.0,3.0); if (g > 999.999 && g < 1000.001) r |= 128;"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 255,
              "§20.11 Math.pow fdlibm port: powers, roots, neg-base odd/even sign (==255)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.log — faithful fdlibm port (e_log), HOP libm backing DELETED. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.log(1.0) == 0.0) r |= 1;"
            "    double l = Math.log(2.718281828459045); if (l > 0.9999999999 && l < 1.0000000001) r |= 2;"
            "    double l10 = Math.log(10.0); if (l10 > 2.302585092 && l10 < 2.302585094) r |= 4;"
            "    double l2 = Math.log(2.0); if (l2 > 0.6931471805 && l2 < 0.6931471806) r |= 8;"
            "    if (Math.log(0.0) < -1.0e300) r |= 16;"                            /* log(0) = -inf */
            "    double neg = Math.log(-1.0); if (neg != neg) r |= 32;"             /* log(-1) = NaN */
            "    double rt = Math.log(Math.exp(3.0)); if (rt > 2.9999999 && rt < 3.0000001) r |= 64;"  /* round-trip */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 127,
              "§20.11 Math.log fdlibm port: log(1/e/10/2) + inf/NaN + exp round-trip (==127)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.asin/acos/atan/atan2 — faithful fdlibm ports, HOP libm backing DELETED. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    double a1 = Math.atan(1.0); if (a1 > 0.7853981 && a1 < 0.7853982) r |= 1;"      /* pi/4 */
            "    if (Math.atan(0.0) == 0.0) r |= 2;"
            "    double as = Math.asin(1.0); if (as > 1.5707962 && as < 1.5707964) r |= 4;"      /* pi/2 */
            "    double a5 = Math.asin(0.5); if (a5 > 0.5235987 && a5 < 0.5235988) r |= 8;"      /* pi/6 */
            "    if (Math.acos(1.0) == 0.0) r |= 16;"
            "    double ac = Math.acos(0.0); if (ac > 1.5707962 && ac < 1.5707964) r |= 32;"     /* pi/2 */
            "    double acn = Math.acos(-1.0); if (acn > 3.1415926 && acn < 3.1415927) r |= 64;" /* pi */
            "    double t2 = Math.atan2(1.0,1.0); if (t2 > 0.7853981 && t2 < 0.7853982) r |= 128;"
            "    double t0 = Math.atan2(1.0,0.0); if (t0 > 1.5707962 && t0 < 1.5707964) r |= 256;"
            "    double tn = Math.atan2(0.0,-1.0); if (tn > 3.1415926 && tn < 3.1415927) r |= 512;"
            "    double bad = Math.asin(2.0); if (bad != bad) r |= 1024;"                        /* NaN */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 2047,
              "§20.11 Math.asin/acos/atan/atan2 fdlibm ports incl. quadrants + NaN (==2047)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.sin/cos — fdlibm ports incl. the FULL argument reduction. sin(1e22)/cos(1e22) exercise
     *    the Payne–Hanek k_rem_pio2 (the classic case naive reduction gets wrong). HOP backing DELETED. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.sin(0.0) == 0.0) r |= 1;"
            "    if (Math.cos(0.0) == 1.0) r |= 2;"
            "    double s = Math.sin(1.5707963267948966); if (s > 0.99999999999 && s <= 1.0) r |= 4;"     /* sin(pi/2) */
            "    double c = Math.cos(3.141592653589793); if (c >= -1.0 && c < -0.99999999999) r |= 8;"     /* cos(pi) */
            "    double s6 = Math.sin(0.5235987755982988); if (s6 > 0.4999999 && s6 < 0.5000001) r |= 16;"  /* sin(pi/6) */
            "    double s100 = Math.sin(100.0); if (s100 > -0.50636565 && s100 < -0.50636563) r |= 32;"     /* medium reduction */
            "    double th = 12.34; double id = Math.sin(th)*Math.sin(th) + Math.cos(th)*Math.cos(th);"
            "    if (id > 0.9999999 && id < 1.0000001) r |= 64;"                                            /* sin^2+cos^2=1 */
            "    double sh = Math.sin(1.0e22); if (sh > -0.85220085 && sh < -0.85220084) r |= 128;"         /* Payne-Hanek */
            "    double ch = Math.cos(1.0e22); if (ch > 0.52321478 && ch < 0.52321479) r |= 256;"           /* Payne-Hanek */
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 511,
              "§20.11 Math.sin/cos fdlibm incl. Payne-Hanek reduction sin/cos(1e22) (==511)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.11 Math.tan + IEEEremainder — the last fdlibm ports; NO Math.* HOP backing remains. ── */
    {
        const char* src =
            "class TT { static int f(int x){"
            "    int r = 0;"
            "    if (Math.tan(0.0) == 0.0) r |= 1;"
            "    double t1 = Math.tan(1.0); if (t1 > 1.5574077 && t1 < 1.5574078) r |= 2;"
            "    double t6 = Math.tan(0.5235987755982988); if (t6 > 0.5773502 && t6 < 0.5773503) r |= 4;"
            "    double t100 = Math.tan(100.0); if (t100 > -0.5872140 && t100 < -0.5872139) r |= 8;"       /* medium reduction */
            "    if (Math.IEEEremainder(5.0, 3.0) == -1.0) r |= 16;"
            "    double rm = Math.IEEEremainder(5.3, 2.0); if (rm > -0.7000001 && rm < -0.6999999) r |= 32;"
            "    if (Math.IEEEremainder(-8.0, 3.0) == 1.0) r |= 64;"
            "    return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t arg = (wasm_val_t)WASM_I32_VAL(0);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status stx = exec_call(mod.code, bbq_vec_len(mod.code), "TT.f", &arg, 1, res, 1);
        CHECK(ok && stx == EXEC_OK && res[0].of.i32 == 127,
              "§20.11 Math.tan + IEEEremainder fdlibm ports (==127) — ALL transcendentals now Java");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── §20.10.13/§20.9.14 Double/Float.valueOf(String) — correctly-rounded parse via the
     *    FloatingDecimal + FDBigInteger port. BIT-EXACT vs host strtod/strtof, incl. the hard
     *    halfway cases that only the FDBigInteger correction gets right: 0.30000000000000004,
     *    2^53+1 → 2^53, 1e23, min/max normal, subnormals down to the smallest (4.9e-324). ── */
    {
        const char* src =
            "class T { static int f(){ int r = 0;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"1.5\").doubleValue()) == 0x3ff8000000000000L) r |= 1;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"0.1\").doubleValue()) == 0x3fb999999999999aL) r |= 2;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"100\").doubleValue()) == 0x4059000000000000L) r |= 4;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"3.14159\").doubleValue()) == 0x400921f9f01b866eL) r |= 8;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"0.30000000000000004\").doubleValue()) == 0x3fd3333333333334L) r |= 16;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"2.2250738585072014e-308\").doubleValue()) == 0x0010000000000000L) r |= 32;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"1.7976931348623157e308\").doubleValue()) == 0x7fefffffffffffffL) r |= 64;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"9007199254740993\").doubleValue()) == 0x4340000000000000L) r |= 128;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"1e23\").doubleValue()) == 0x44b52d02c7e14af6L) r |= 256;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"1.1e-300\").doubleValue()) == 0x01a792bc89ab7215L) r |= 512;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"-0.0\").doubleValue()) == 0x8000000000000000L) r |= 1024;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"123456789.123456789\").doubleValue()) == 0x419d6f34547e6b75L) r |= 2048;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"1e-323\").doubleValue()) == 0x0000000000000002L) r |= 4096;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"4.9e-324\").doubleValue()) == 0x0000000000000001L) r |= 8192;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"10000000000000002\").doubleValue()) == 0x4341c37937e08001L) r |= 16384;"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = ok ? exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 0x7FFF,
              "§20.10.13 Double.valueOf(String) correctly-rounded, bit-exact vs strtod incl. halfway cases (==32767)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    {
        const char* src =
            "class T { static int g(){ int r = 0;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"1.5\").floatValue()) == 0x3fc00000) r |= 1;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"0.1\").floatValue()) == 0x3dcccccd) r |= 2;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"100\").floatValue()) == 0x42c80000) r |= 4;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"3.14159\").floatValue()) == 0x40490fd0) r |= 8;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"0.30000000000000004\").floatValue()) == 0x3e99999a) r |= 16;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"2.2250738585072014e-308\").floatValue()) == 0x00000000) r |= 32;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"1.7976931348623157e308\").floatValue()) == 0x7f800000) r |= 64;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"9007199254740993\").floatValue()) == 0x5a000000) r |= 128;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"1e23\").floatValue()) == 0x65a96816) r |= 256;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"1.1e-300\").floatValue()) == 0x00000000) r |= 512;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"-0.0\").floatValue()) == 0x80000000) r |= 1024;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"123456789.123456789\").floatValue()) == 0x4ceb79a3) r |= 2048;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"1e-323\").floatValue()) == 0x00000000) r |= 4096;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"4.9e-324\").floatValue()) == 0x00000000) r |= 8192;"
            "  if (Float.floatToRawIntBits(Float.valueOf(\"10000000000000002\").floatValue()) == 0x5a0e1bca) r |= 16384;"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = ok ? exec_call(mod.code, bbq_vec_len(mod.code), "T.g", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 0x7FFF,
              "§20.9.14 Float.valueOf(String) correctly-rounded, bit-exact vs strtof incl. overflow/underflow (==32767)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }
    /* Special values, whitespace trim, trailing type suffix, and NumberFormatException. */
    {
        const char* src =
            "class T { static int f(){ int r = 0;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"Infinity\").doubleValue()) == 0x7ff0000000000000L) r |= 1;"
            "  if (Double.doubleToRawLongBits(Double.valueOf(\"-Infinity\").doubleValue()) == 0xfff0000000000000L) r |= 2;"
            "  if (Double.isNaN(Double.valueOf(\"NaN\").doubleValue())) r |= 4;"
            "  if (Double.valueOf(\"  12.5  \").doubleValue() == 12.5) r |= 8;"
            "  if (Double.valueOf(\"1.25f\").doubleValue() == 1.25) r |= 16;"
            "  boolean t1 = false; try { Double.valueOf(\"abc\"); } catch (NumberFormatException e) { t1 = true; } if (t1) r |= 32;"
            "  boolean t2 = false; try { Double.valueOf(\"\"); } catch (NumberFormatException e) { t2 = true; } if (t2) r |= 64;"
            "  boolean t3 = false; try { Double.valueOf(\"1.2.3\"); } catch (NumberFormatException e) { t3 = true; } if (t3) r |= 128;"
            "  return r; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        emit_wasm_ctx mod = {0};
        bool ok = assemble(&a, src, &mod);
        wasm_val_t res[1] = { WASM_INIT_VAL };
        exec_status st = ok ? exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1) : EXEC_INVALID;
        CHECK(ok && st == EXEC_OK && res[0].of.i32 == 255,
              "Double.valueOf: Infinity/-Infinity/NaN/trim/suffix + NumberFormatException on bad input (==255)");
        bbq_vec_free(mod.code); bbq_arena_free(&a);
    }

    /* ── Memory DSE (W9d) — the BEHAVIOUR, which is what a wrong answer here
     * costs. test_click_partition pins the decision and test_sir the node
     * counts; these RUN the program and read the field back, so a store deleted
     * when something could still observe it shows up as a wrong number. The
     * aliasing pair is the point: identical source, one call passing the same
     * object twice and one passing two, with different correct answers. */
    printf("== memory DSE behaviour ==\n");
    {
        struct { const char* src; int32_t want; const char* label; } dse[] = {
          { "class C { int f; }"
            " class T { static int f(){ C o = new C(); o.f = 1; o.f = 2; return o.f; } }", 2,
            "dse: the overwritten store goes and the surviving value is what is read" },
          /* Two FRESH objects: provably non-null, so no guard sits between the
           * stores and nothing else keeps the first one alive. Then the ONLY
           * thing standing between this and a wrong answer is the must-alias
           * test — which is what makes this case a falsifier and not decoration
           * (with two PARAMETERS instead, each null-check leaves a reachable
           * observer and the case passes whether must-alias works or not). */
          { "class C { int f; }"
            " class T { static int get(C c){ return c.f; }"
            "           static int f(){ C a = new C(); C b = new C();"
            "                           a.f = 1; b.f = 2; return get(a); } }", 1,
            "dse SOUNDNESS: two fresh objects share a cell, not a location — a.f is 1" },
          { "class C { int f; }"
            " class T { static int g(C a, C b){ a.f = 1; b.f = 2; return a.f; }"
            "           static int f(){ C o = new C(); return g(o, o); } }", 2,
            "dse: ALIASED receivers — b.f = 2 really did overwrite a.f, so a.f reads 2" },
          { "class C { int f; }"
            " class T { static int f(){ C o = new C(); o.f = 1; int r = o.f; o.f = 2;"
            "                           return r * 10 + o.f; } }", 12,
            "dse SOUNDNESS: a read between the stores still sees 1" },
          { "class T { static int s;"
            "          static int f(){ s = 1; s = 2; return s; } }", 2,
            "dse: a static overwritten before any read, and the survivor is read" },
          { "class T { static int f(){ int[] a = new int[2]; a[0] = 1; a[1] = 2;"
            "                          return a[0] * 10 + a[1]; } }", 12,
            "dse SOUNDNESS: array elements share a cell — both stores survive" },
          { "class C { int f; }"
            " class T { static int f(){ C o = new C(); o.f = 1;"
            "   try { o.f = 2; throw new Exception(); } catch (Exception e) { }"
            "   return o.f; } }", 2,
            "dse: a store before a throw is observable in the catch's continuation" },
        };
        for (size_t i = 0; i < sizeof dse / sizeof dse[0]; i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 18);
            emit_wasm_ctx mod = {0};
            assemble(&a, dse[i].src, &mod);
            wasm_val_t res[1] = { WASM_INIT_VAL };
            exec_status st = exec_call(mod.code, bbq_vec_len(mod.code), "T.f", NULL, 0, res, 1);
            CHECK(st == EXEC_OK && res[0].of.i32 == dse[i].want, dse[i].label);
            bbq_vec_free(mod.code); bbq_arena_free(&a);
        }
    }

    exec_jre_teardown();                         /* tear down the shared jre + store once, at the end */
    bbq_vec_free(jre.code); bbq_arena_free(&jre_arena);

    return TEST_SUMMARY("test_exec");
}
