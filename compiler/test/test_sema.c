// test_sema.c — Section 2 (sema): AST + java.lang environment -> typed AST.
// Plain-C, exit-code based. The class environment is supplied the spec way:
// java.lang is parsed from lib/java/lang/*.java (real signatures, JLS 1.0 ch.20)
// and merged into the program ahead of user code — no .exp loader. Tests are
// grounded in the JLS check rules, not guesswork.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "gen/simd_intrinsics.h"   /* SIMD_INTRINSIC_COUNT — every stub is stamped */
#include "bbq_arena.h"
#include "bbq_vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define JT_REPORT_RSS   /* this suite compiles the whole prelude per case */
#include "javelina_test.h"

// The §7.3-correct prelude+user parse (per-unit programs; see jtest_units.h).
#include "jtest_units.h"

// Build units + run sema; the caller inspects ctx and must sema_destroy it.
// Returns false if parsing failed (a FAIL was already recorded).
static bool analyze_into(const char* user_src, bbq_arena* arena, sema_ctx_t* ctx) {
    jtest_program_t jp;
    if (!jtest_build(user_src, arena, &jp)) return false;
    sema_init(ctx, arena);
    sema_analyze_units(ctx, jp.units, jp.nunits);
    bbq_vec_free(jp.units);
    return true;
}

/* ── The PLUGIN import set, as sema hands it downstream ──────────────────────────────────
 *
 * The mode must be set BEFORE analysis: sema_note_import (sema.c:2269) records a resolved
 * target "iff it is NOT a defined function", and whether a library method is defined is
 * itself mode-dependent (sema_method_is_defined, sema.c:4419). So in PLUGIN mode the
 * accumulator holds the REFERENCED library methods; in WHOLE mode it holds only natives.
 *
 * These probes read the FINAL set — what sema actually publishes through sema_import_count /
 * sema_import_at — because that is what the module assembler consumes. */
typedef struct {
    bool ok;          /* the source analyzed without errors */
    int  total;       /* sema_import_count */
    int  for_class;   /* entries whose owning class is `class_name` */
    int  virt_of;     /* entries that are VIRTUAL methods of `class_name` */
    int  n_virt_of;   /* how many virtual methods `class_name` has in total */
} jt_imports_t;

static jt_imports_t plugin_imports(const char* user_src, const char* class_name,
                                   bbq_arena* arena) {
    jt_imports_t r; memset(&r, 0, sizeof r);
    r.total = r.for_class = r.virt_of = r.n_virt_of = -1;
    jtest_program_t jp;
    if (!jtest_build(user_src, arena, &jp)) return r;
    sema_ctx_t ctx;
    sema_init(&ctx, arena);
    /* BOTH, and before analysis. num_library_classes is what marks the prelude types as
     * library (import_pkg >= 0); without it the PLUGIN sweep's `c->import_pkg >= 0` guard
     * never matches and the probe silently measures WHOLE-mode behaviour instead. */
    ctx.num_library_classes = jp.nlib;
    ctx.mode = SEMA_MODE_PLUGIN;
    sema_analyze_units(&ctx, jp.units, jp.nunits);
    bbq_vec_free(jp.units);
    if (sema_error_count(&ctx) == 0) {
        r.ok = true;
        r.total = r.for_class = r.virt_of = 0;
        for (int i = 0; i < sema_ref_count(&ctx); i++) {
            sema_ref_ent_t e = sema_ref_at(&ctx, i);
            /* §13.1's rules are for "a Java binary representation FOR A CLASS": count only the
             * references belonging to the classes this compilation is producing. */
            if (sema_get_class(&ctx, e.from_class)->import_pkg >= 0) continue;
            r.total++;
            if (e.kind != SEMA_REF_METHOD) continue;
            const sema_class_t* c = sema_get_class(&ctx, e.decl_class);
            if (!c || !c->name || !class_name || strcmp(c->name, class_name)) continue;
            r.for_class++;
            if (sema_is_virtual_method(&c->methods[e.member])) r.virt_of++;
        }
        r.n_virt_of = 0;
        for (int ci = 0; ci < (int)bbq_vec_len(ctx.classes); ci++) {
            const sema_class_t* c = sema_get_class(&ctx, ci);
            if (!c->name || !class_name || strcmp(c->name, class_name)) continue;
            for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
                if (sema_is_virtual_method(&c->methods[mi])) r.n_virt_of++;
            break;
        }
    }
    sema_destroy(&ctx);
    return r;
}

/* ONE recorded reference, resolved by the method's simple name. §13.1 is a rule about an
 * individual reference — "a reference to a method or constructor must be resolved ... to a
 * symbolic reference to the class or interface in which the denoted method or constructor is
 * declared, plus the signature" — so the probe has to expose the row, not a count.
 *
 * decl_class is the class the row NAMES; param0 is the first parameter's type as sema typed
 * it, which is how the signature half is observed with the row shape that exists today. */
typedef struct {
    bool found;
    char decl_class[64];
    char param0[64];
    int  nparams;
} jt_ref_t;

/* §13.1 calls a parameter type "a symbolic reference to the type of each parameter". Render
 * it the way the spec names it: a primitive by its keyword, a reference by its class name. */
static void jt_type_name(const sema_ctx_t* ctx, java_type_t t, char* out, size_t n) {
    switch (t.tag) {
    case JT_BOOL:    snprintf(out, n, "boolean"); return;
    case JT_BYTE:    snprintf(out, n, "byte");    return;
    case JT_SHORT:   snprintf(out, n, "short");   return;
    case JT_CHAR:    snprintf(out, n, "char");    return;
    case JT_INT:     snprintf(out, n, "int");     return;
    case JT_LONG:    snprintf(out, n, "long");    return;
    case JT_FLOAT:   snprintf(out, n, "float");   return;
    case JT_DOUBLE:  snprintf(out, n, "double");  return;
    case JT_CLASS: {
        const sema_class_t* c = sema_get_class(ctx, t.class_id);
        snprintf(out, n, "%s", (c && c->name) ? c->name : "?");
        return;
    }
    default: snprintf(out, n, "(tag %d)", (int)t.tag); return;
    }
}

/* The reference kinds recorded FOR one named class — §13.1's per-class rules and its
 * constant-field exclusion are both statements about a single class's compiled form. */
typedef struct {
    bool ok;
    int  ref_K, ref_mut;    /* field references named "K" / "mutable_" */
    int  n_super, n_iface;  /* SUPERCLASS / SUPERINTERFACE records */
} jt_kinds_t;

static jt_kinds_t plugin_ref_kinds(const char* user_src, const char* of_class,
                                   bbq_arena* arena) {
    jt_kinds_t r; memset(&r, 0, sizeof r);
    jtest_program_t jp;
    if (!jtest_build(user_src, arena, &jp)) return r;
    sema_ctx_t ctx;
    sema_init(&ctx, arena);
    ctx.num_library_classes = jp.nlib;
    ctx.mode = SEMA_MODE_PLUGIN;
    sema_analyze_units(&ctx, jp.units, jp.nunits);
    bbq_vec_free(jp.units);
    if (sema_error_count(&ctx) == 0) {
        r.ok = true;
        for (int i = 0; i < sema_ref_count(&ctx); i++) {
            sema_ref_ent_t e = sema_ref_at(&ctx, i);
            const sema_class_t* from = sema_get_class(&ctx, e.from_class);
            if (!from->name || strcmp(from->name, of_class)) continue;
            if (e.kind == SEMA_REF_SUPERCLASS)     { r.n_super++; continue; }
            if (e.kind == SEMA_REF_SUPERINTERFACE) { r.n_iface++; continue; }
            if (e.kind != SEMA_REF_FIELD) continue;
            const sema_class_t* d = sema_get_class(&ctx, e.decl_class);
            const char* fn = d->fields[e.member].name;
            if (fn && !strcmp(fn, "K"))        r.ref_K++;
            if (fn && !strcmp(fn, "mutable_")) r.ref_mut++;
        }
    }
    sema_destroy(&ctx);
    return r;
}

static jt_ref_t plugin_ref_for(const char* user_src, const char* method_name,
                               bbq_arena* arena) {
    jt_ref_t r; memset(&r, 0, sizeof r);
    jtest_program_t jp;
    if (!jtest_build(user_src, arena, &jp)) return r;
    sema_ctx_t ctx;
    sema_init(&ctx, arena);
    ctx.num_library_classes = jp.nlib;
    ctx.mode = SEMA_MODE_PLUGIN;
    sema_analyze_units(&ctx, jp.units, jp.nunits);
    bbq_vec_free(jp.units);
    if (sema_error_count(&ctx) == 0) {
        for (int i = 0; i < sema_ref_count(&ctx); i++) {
            sema_ref_ent_t e = sema_ref_at(&ctx, i);
            if (sema_get_class(&ctx, e.from_class)->import_pkg >= 0) continue;  /* refs OF user classes */
            if (e.kind != SEMA_REF_METHOD) continue;
            const sema_class_t* c = sema_get_class(&ctx, e.decl_class);
            if (!c || e.member >= (int)bbq_vec_len(c->methods)) continue;
            const sema_method_t* m = &c->methods[e.member];
            if (!m->name || strcmp(m->name, method_name)) continue;
            r.found = true;
            snprintf(r.decl_class, sizeof r.decl_class, "%s", c->name ? c->name : "?");
            r.nparams = m->param_count;
            if (m->param_count > 0) jt_type_name(&ctx, m->param_types[0],
                                                 r.param0, sizeof r.param0);
            break;
        }
    }
    sema_destroy(&ctx);
    return r;
}

// Analyze user source against java.lang; return the error count (and dump diags).
static int analyze(const char* user_src, bool dump) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    sema_ctx_t ctx;
    if (!analyze_into(user_src, &arena, &ctx)) { bbq_arena_free(&arena); return -1; }
    int n = 0;
    const sema_diag_t* diags = sema_diags(&ctx, &n);
    if (dump) for (int i = 0; i < n; i++)
        printf("    diag %s:%d:%d  %s\n", diags[i].loc.file ? diags[i].loc.file : "<user>",
               diags[i].loc.line, diags[i].loc.col, diags[i].message);
    int errs = sema_error_count(&ctx);
    sema_destroy(&ctx);              /* 31 htrees/vecs, none of them arena-backed */
    bbq_arena_free(&arena);
    return errs;
}

// Count WARNING diags of one KIND — the pin for diagnostic precision (a warning
// that should not fire, or one that must keep firing). Kind is the diagnostic's
// structural identity; the message is prose and free to be reworded.
static int warn_count(const char* user_src, sema_diag_kind_t kind) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    sema_ctx_t ctx;
    if (!analyze_into(user_src, &arena, &ctx)) { bbq_arena_free(&arena); return -1; }
    int n = 0, hits = 0;
    const sema_diag_t* diags = sema_diags(&ctx, &n);
    for (int i = 0; i < n; i++) {
        /* This harness analyzes the jre sources as ordinary units, so lib
         * diags (real ones — the jre has warnings) share the vec with the
         * fixture's. The USER unit is the one parsed with no file name:
         * loc.file == NULL is its structural identity, so count only it. */
        if (diags[i].loc.file != NULL) continue;
        if (diags[i].level == DIAG_WARNING && diags[i].kind == kind)
            hits++;
    }
    sema_destroy(&ctx);
    bbq_arena_free(&arena);
    return hits;
}

// The (class, "name") → class-local method index lookup the call-graph pins use.
static int method_of(const sema_ctx_t* s, int cid, const char* name) {
    const sema_class_t* c = sema_get_class(s, cid);
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
        if (c->methods[i].name && !strcmp(c->methods[i].name, name)) return i;
    return -1;
}

int main(void) {
    // 1. java.lang must type-check against itself (self-consistent environment).
    printf("== java.lang self-check ==\n");
    int builtin_errors = analyze(NULL, true);
    CHECK(builtin_errors == 0, "java.lang environment type-checks with 0 errors");

    // 1a2. Constructor throws clause (JLS §8.8.5): a checked exception thrown in a ctor body is
    // covered by the ctor's OWN throws clause; without it, it's an "unhandled checked exception".
    printf("== constructor throws clause ==\n");
    CHECK(analyze("class C { C() throws Exception { throw new Exception(); } }", false) == 0,
          "ctor throwing a checked exception WITH a matching throws clause type-checks");
    CHECK(analyze("class C { C() { throw new Exception(); } }", false) > 0,
          "ctor throwing a checked exception WITHOUT a throws clause is rejected");

    // 1a3. Qualified name in expression position (JLS §6.5.2): a package-qualified static field access
    // (java.lang.Integer.MAX_VALUE) reclassifies its base as a type, not an "undefined 'java'" value.
    printf("== qualified static field access ==\n");
    CHECK(analyze("class C { static int f(){ return java.lang.Integer.MAX_VALUE; } }", false) == 0,
          "package-qualified static field access (java.lang.Integer.MAX_VALUE) resolves");

    // 1a3b. The same reclassification in a method INVOCATION. §15.11.1 takes a
    // MethodName, which is an AmbiguousName followed by an identifier, and
    // §6.5.2 reclassifies that AmbiguousName exactly as it does for a field
    // access — so if the field form above resolves, the call form must too.
    printf("== qualified static method invocation ==\n");
    CHECK(analyze("class C { static int f(){ return java.lang.Integer.parseInt(\"42\"); } }",
                  false) == 0,
          "package-qualified static method call (java.lang.Integer.parseInt) resolves");
    CHECK(analyze("class C { static int f(){ return java.lang.Math.abs(-7); } }", false) == 0,
          "package-qualified static method call on a one-package name resolves");
    CHECK(analyze("class C { static String f(){ return java.lang.String.valueOf(1); } }",
                  false) == 0,
          "package-qualified static call returning a reference type resolves");
    // The single-segment form already worked; it is here so a regression that
    // "fixes" the qualified case by breaking the simple one cannot pass.
    CHECK(analyze("class C { static int f(){ return Integer.parseInt(\"42\"); } }", false) == 0,
          "the unqualified form still resolves");
    // And a name that really is undefined must still be an error, so the fix
    // cannot be "stop reporting undefined identifiers".
    CHECK(analyze("class C { static int f(){ return no.such.Thing.m(); } }", false) > 0,
          "an unresolvable qualified call is still an error");
    // §15.11.3: with a TypeName qualifier the declaration must be static —
    // there is no reference to serve as `this`. Accepting it would emit a call
    // with no receiver, so this is a miscompile guard, not a style rule.
    CHECK(analyze("class C { static int f(){ return java.lang.Integer.intValue(); } }",
                  false) > 0,
          "§15.11.3 an instance method through a TypeName qualifier is rejected");

    // 1a4. JLS §12.4 needs_init: a class needs initialization iff it (or a superCLASS) declares static-init code.
    printf("== class initialization analysis (JLS 12.4) ==\n");
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        sema_ctx_t ctx;
        analyze_into(
            "class A { static int x = 5; }"               /* own static field init → needs */
            "class B { static { int y = 1; } }"           /* static block → needs */
            "class Cc {}"                                 /* nothing → no */
            "class D extends A {}"                        /* super needs → inherits */
            "class E extends Cc { int z = 3; }", &arena, &ctx); /* instance init only, clean super → no */
        int a=sema_find_class(&ctx,"A"), b=sema_find_class(&ctx,"B"), cc=sema_find_class(&ctx,"Cc"),
            d=sema_find_class(&ctx,"D"), e=sema_find_class(&ctx,"E");
        CHECK(a>=0  && sema_get_class(&ctx,a)->needs_init,   "static field init → needs_init");
        CHECK(b>=0  && sema_get_class(&ctx,b)->needs_init,   "static block → needs_init");
        CHECK(cc>=0 && !sema_get_class(&ctx,cc)->needs_init, "no static init → no needs_init");
        CHECK(d>=0  && sema_get_class(&ctx,d)->needs_init,   "subclass of needs_init class inherits needs_init");
        CHECK(e>=0  && !sema_get_class(&ctx,e)->needs_init,  "instance-init-only w/ clean super → no needs_init");
        sema_destroy(&ctx); bbq_arena_free(&arena);
    }

    // 1a5. §12.4.2 synthesis: a needs_init class gets a $initstate static field AND a $ensure_init method
    // (the barrier target); a class that needs no init gets neither.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        sema_ctx_t ctx;
        analyze_into(
            "class N { static int s = 7; }"      /* needs init */
            "class P {}", &arena, &ctx);         /* does not */
        const sema_class_t* nc = sema_get_class(&ctx, sema_find_class(&ctx, "N"));
        const sema_class_t* pc = sema_get_class(&ctx, sema_find_class(&ctx, "P"));
        bool n_state = false, n_ensure = false, p_ensure = false;
        for (int i = 0; i < (int)bbq_vec_len(nc->fields); i++)
            if (strcmp(nc->fields[i].name, "$initstate") == 0) n_state = true;
        for (int i = 0; i < (int)bbq_vec_len(nc->methods); i++)
            if (strcmp(nc->methods[i].name, "$ensure_init") == 0) n_ensure = true;
        for (int i = 0; i < (int)bbq_vec_len(pc->methods); i++)
            if (strcmp(pc->methods[i].name, "$ensure_init") == 0) p_ensure = true;
        CHECK(n_state,   "needs_init class N gets a $initstate static field");
        CHECK(n_ensure,  "needs_init class N gets a $ensure_init method");
        CHECK(!p_ensure, "non-needs_init class P gets NO $ensure_init");
        sema_destroy(&ctx); bbq_arena_free(&arena);
    }

    // 1b. §10 arrays: the RefArray overlay is synthesized as a real class (every
    // reference array is represented by it). Structural pin of the synthesis.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        sema_ctx_t ctx;
        analyze_into("class T {}", &arena, &ctx);
        int ra = sema_refarray_id(&ctx);
        CHECK(ra >= 0, "RefArray synthesized (sema_refarray_id resolved)");
        if (ra >= 0) {
            const sema_class_t* c = sema_get_class(&ctx, ra);
            CHECK(!c->is_interface && c->super_id >= 0
                  && strcmp(sema_get_class(&ctx, c->super_id)->name, "Object") == 0,
                  "RefArray is a final class extending Object");
            CHECK((int)bbq_vec_len(c->fields) == 2, "RefArray has 2 instance fields (elementClass, data)");
            if ((int)bbq_vec_len(c->fields) == 2) {
                CHECK(c->fields[0].type.tag == JT_CLASS, "RefArray.elementClass is a Class ref (§10.10 store check)");
                CHECK(c->fields[1].type.tag == JT_ARRAY && c->fields[1].type.element
                      && c->fields[1].type.element->tag == JT_NULL,
                      "RefArray.data is an array of the top reference (JT_NULL element)");
            }
        }
        sema_destroy(&ctx); bbq_arena_free(&arena);
    }

    // 1c. §20.9/§20.10 raw bit accessors are stamped with their Move* intrinsic kind ONCE
    //     (read at each call site, never re-strcmp'd); ordinary methods stay 0.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        sema_ctx_t ctx;
        analyze_into("class T {}", &arena, &ctx);
        struct { const char* cls; const char* meth; int kind; } want[] = {
            { "java.lang.Float",  "floatToRawIntBits",   1 }, { "java.lang.Float",  "intBitsToFloat",   2 },
            { "java.lang.Double", "doubleToRawLongBits", 3 }, { "java.lang.Double", "longBitsToDouble", 4 },
        };
        for (int w = 0; w < 4; w++) {
            int cid = sema_find_class(&ctx, want[w].cls);
            CHECK(cid >= 0, want[w].cls);
            const sema_class_t* c = cid >= 0 ? sema_get_class(&ctx, cid) : NULL;
            int found = 0;
            for (int i = 0; c && i < (int)bbq_vec_len(c->methods); i++)
                if (strcmp(c->methods[i].name, want[w].meth) == 0) {
                    found = 1; CHECK(c->methods[i].move_kind == want[w].kind, want[w].meth); }
            CHECK(found, want[w].meth);
        }
        // A non-intrinsic method (Float.floatValue) must NOT be stamped.
        int fid = sema_find_class(&ctx, "java.lang.Float");
        const sema_class_t* fc = fid >= 0 ? sema_get_class(&ctx, fid) : NULL;
        for (int i = 0; fc && i < (int)bbq_vec_len(fc->methods); i++)
            if (strcmp(fc->methods[i].name, "floatValue") == 0)
                CHECK(fc->methods[i].move_kind == 0, "Float.floatValue not a move intrinsic");
        sema_destroy(&ctx); bbq_arena_free(&arena);
    }

    // 2. A trivial class (implicitly extends Object) — must pass.
    CHECK(analyze("class T { int f() { return 0; } }", false) == 0,
          "trivial class type-checks");

    // 3. JLS §14.8: if condition must be boolean — a non-boolean is an error.
    CHECK(analyze("class T { void f(int x) { if (x + 1) return; } }", false) > 0,
          "non-boolean if condition rejected (JLS 14.8)");

    // 4. The synchronized rejection (our one deliberate departure).
    CHECK(analyze("class T { synchronized void f() {} }", false) > 0,
          "synchronized method rejected");

    // 5. JLS §14.15: return type compatibility — returning a ref from an int method.
    CHECK(analyze("class T { int f() { return this; } }", false) > 0,
          "incompatible return type rejected (JLS 14.15)");

    // 6. Full Java 1.0 primitives now type-check (the extension over the JC subset).
    CHECK(analyze("class T { void f() { long a = 5L; float b = 2.0f; double c = 1.5; char d = 'x'; } }", false) == 0,
          "long/float/double/char locals type-check");

    // 7. JLS §5.6.2 binary numeric promotion: int + long is long.
    CHECK(analyze("class T { long f() { int x = 1; return x + 2L; } }", false) == 0,
          "int+long promotes to long, assignable to long return");

    // 8. JLS §5.1.3 / §5.2: narrowing without a cast is rejected.
    CHECK(analyze("class T { void f() { int i = 5; short s = i; } }", false) > 0,
          "int -> short without cast rejected (JLS 5.2)");

    // 8b. §5.6.2 + §5.2: an arithmetic result is the PROMOTED type, so a wider result
    // narrowed into a smaller variable/return without a cast is rejected (regression guard
    // for the old `result=jt_prim(JT_INT)` force, which used to accept these silently).
    CHECK(analyze("class T { int f(long a, long b) { return a + b; } }", false) > 0,
          "long+long narrowed to int return without cast rejected (JLS 5.2)");
    CHECK(analyze("class T { void f(long a) { int x = a * 2; } }", false) > 0,
          "long*int narrowed to int local without cast rejected (JLS 5.2)");
    CHECK(analyze("class T { void f(double d) { int x = -d; } }", false) > 0,
          "unary -(double) narrowed to int without cast rejected (JLS 5.6.1/5.2)");
    // …and the WIDENING direction still type-checks (result promotes cleanly).
    CHECK(analyze("class T { long f(int a, long b) { return a + b; } }", false) == 0,
          "int+long result is long, assignable to long return");

    // 9. JLS §5.2: a small int constant IS assignable to short (narrowing of constants).
    CHECK(analyze("class T { void f() { short s = 5; } }", false) == 0,
          "small constant assignable to short (JLS 5.2)");

    // 9b. JLS §5.2: an in-range int constant IS assignable to char (char is 0..65535).
    CHECK(analyze("class T { void f() { char c = 65000; } }", false) == 0,
          "in-range constant assignable to char (JLS 5.2)");
    CHECK(analyze("class T { void f() { byte b = 100; } }", false) == 0,
          "in-range constant assignable to byte (JLS 5.2)");
    // …and an out-of-range constant is NOT (still a §5.2 narrowing error).
    CHECK(analyze("class T { void f() { char c = 70000; } }", false) > 0,
          "out-of-range constant to char rejected (JLS 5.2)");
    CHECK(analyze("class T { void f() { char c = -1; } }", false) > 0,
          "negative constant to char rejected (JLS 5.2)");

    // 9c. §5.2's premise is "a constant expression of type byte, short, CHAR, or
    // int" — every case above has an int literal as the source, so the char half
    // of the rule went untested and was in fact rejected. `bytes[i] = '\n';` is
    // the ordinary way to write a newline into a byte buffer.
    CHECK(analyze("class T { void f() { byte b = 'A'; } }", false) == 0,
          "char constant assignable to byte (JLS 5.2, source is char)");
    CHECK(analyze("class T { void f() { byte b = '\\n'; } }", false) == 0,
          "char escape assignable to byte (JLS 5.2)");
    CHECK(analyze("class T { void f() { short s = 'A'; } }", false) == 0,
          "char constant assignable to short (JLS 5.2)");
    CHECK(analyze("class T { void f() { byte[] x = new byte[2]; x[0] = '\\n'; } }", false) == 0,
          "char constant assignable to a byte ARRAY ELEMENT (JLS 5.2)");
    CHECK(analyze("class T { byte f() { return 'A'; } }", false) == 0,
          "char constant assignable to a byte RETURN (JLS 5.2)");
    // …and representability still decides: Ā is 256, which no byte holds.
    CHECK(analyze("class T { void f() { byte b = '\\u0100'; } }", false) > 0,
          "out-of-range char constant to byte rejected (JLS 5.2)");

    // 9c. JLS §15.18.1: `+` with a String operand is string concatenation → String;
    // the other operand (any non-void type) undergoes string conversion.
    CHECK(analyze("class T { void f() { String s = \"a\" + 1; } }", false) == 0,
          "String + int is String (JLS 15.18.1)");
    CHECK(analyze("class T { void f() { String s = 1 + \"a\"; } }", false) == 0,
          "int + String is String (JLS 15.18.1)");
    CHECK(analyze("class T { void f() { String s = \"a\" + true + 'c' + null; } }", false) == 0,
          "String + boolean + char + null chain is String");
    CHECK(analyze("class T { void f(Object o) { String s = \"a\" + o; } }", false) == 0,
          "String + reference is String (via toString)");
    CHECK(analyze("class T { void f() { int x = \"a\" + 1; } }", false) > 0,
          "concat result (String) not assignable to int");
    CHECK(analyze("class T { void f() { int x = 1 + 2; String s = x; } }", false) > 0,
          "numeric + is still numeric (int not assignable to String)");

    // 10. JLS §11.2: an uncaught checked exception is a compile-time error...
    CHECK(analyze("class T { void f() { throw new Exception(); } }", false) > 0,
          "uncaught checked exception rejected (JLS 11.2)");
    // ...but declaring `throws` makes it legal.
    CHECK(analyze("class T { void f() throws Exception { throw new Exception(); } }", false) == 0,
          "checked exception with throws is legal");
    // ...and a RuntimeException is unchecked.
    CHECK(analyze("class T { void f() { throw new NullPointerException(); } }", false) == 0,
          "unchecked (RuntimeException) throw needs no throws");

    // 11. JLS §14.18: a catch clause's parameter must be a Throwable.
    CHECK(analyze("class T { void f() { try { } catch (String e) { } } }", false) > 0,
          "catch of non-Throwable rejected (JLS 14.18)");
    CHECK(analyze("class T { void f() { try { } catch (Exception e) { } } }", false) == 0,
          "catch of Exception is legal");

    // 12. JLS §15.11: calling an undefined method is an error.
    CHECK(analyze("class T { void f() { g(); } }", false) > 0,
          "call to undefined method rejected (JLS 15.11)");

    // 13. Arrays: 1-D and (jagged) multi-dimensional are legal.
    CHECK(analyze("class T { void f() { int[] a = new int[3]; } }", false) == 0,
          "1-D array legal");
    CHECK(analyze("class Box {} class T { void f() { Box[] a = new Box[3]; } }", false) == 0,
          "1-D reference array legal");
    CHECK(analyze("class T { void f() { int[][] a = new int[2][]; } }", false) == 0,
          "jagged multi-dim array type + creation legal");

    // 12. Diagnostic lattices at full-Java scale (no fixed-size fail-open caps).
    // (a) §16: definite assignment must keep working past 256 locals (legal to
    //     65535). A method with 300 int locals where the LAST one is read
    //     uninitialized — the error must still fire.
    {
        static char src[32768]; int off = 0;
        off += snprintf(src + off, sizeof src - (size_t)off, "class T { int f() { ");
        for (int i = 0; i < 299; i++)
            off += snprintf(src + off, sizeof src - (size_t)off, "int v%d = 0; ", i);
        off += snprintf(src + off, sizeof src - (size_t)off,
                        "int u; return u; } }");
        CHECK(analyze(src, false) > 0,
              "definite assignment fires past 256 locals (no fail-open bitmap cap)");
    }
    // (b) §14.19/§11.2: the inner-try exception subtraction must not drop caught
    //     types past a fixed cap. 66 checked exceptions all thrown and all caught
    //     by an inner try — the enclosing catch of the 66th is unreachable and
    //     must be flagged.
    {
        static char src[65536]; int off = 0;
        for (int i = 0; i < 66; i++)
            off += snprintf(src + off, sizeof src - (size_t)off,
                            "class E%d extends Exception { }\n", i);
        off += snprintf(src + off, sizeof src - (size_t)off,
                        "class T { void f() { try { try {");
        for (int i = 0; i < 66; i++)
            off += snprintf(src + off, sizeof src - (size_t)off,
                            " if (hashCode() == %d) throw new E%d();", i, i);
        off += snprintf(src + off, sizeof src - (size_t)off, " }");
        for (int i = 0; i < 66; i++)
            off += snprintf(src + off, sizeof src - (size_t)off,
                            " catch (E%d e) { }", i);
        off += snprintf(src + off, sizeof src - (size_t)off,
                        " } catch (E65 e) { } } }");
        CHECK(analyze(src, false) > 0,
              "enclosing catch of a fully-inner-caught type is unreachable "
              "past 64 caught types (no silent to_remove cap)");
    }
    // (c) §15.17.2: long division can throw ArithmeticException; the catch is
    //     legal (confirms the thrown-set seed covers JT_LONG).
    CHECK(analyze("class T { long f(long x) { try { return 5L / x; } "
                  "catch (ArithmeticException e) { return 0L; } } }", false) == 0,
          "catch(ArithmeticException) over long division is legal");

    // Spec §7/§10 — THE DEFUNCTIONALIZED CALL GRAPH (the analysis side, Java type-space).
    //
    //   §10: the combined analysis "CONSUMES the lowered value graph + the defunctionalized
    //   call graph" — consumes, never builds. §7: "the defunctionalized call_ref target set
    //   makes this precise and finite — the VFG paper spends its whole scalability budget
    //   approximating exactly what you already have."
    //
    //   The paper (Li/Cifuentes/Keynes, ISMM'13, "Precise and Scalable Context-Sensitive
    //   Pointer Analysis via Value Flow Graph" — ~/Documents) analyzes C, where a call
    //   target is a pointer VALUE: its call graph is an OUTPUT of the pointer analysis the
    //   call graph itself feeds — a cycle. Its own words: it computes "full points-to sets
    //   for FUNCTION POINTERS only" (the one query it cannot defer), and its clone-repair
    //   re-runs whenever "there are extra targets for a function pointer". Breaking that
    //   cycle is what its Algorithms 3-4 spend. (The OTHER half of its budget — input-
    //   relative summaries against exponential call paths — spec §7 ADOPTS from the same
    //   paper for stage 5. Adopted technique, not avoided cost.)
    //
    //   Java 1.0 has no function values. Every indirect call is virtual dispatch, and sema
    //   AUTHORS the map: ONE rule, sema_resolve_virtual (JLS §8.4.8) — the same rule the
    //   vtable builder materializes into ref.func rows (pinned in test_wasm_types) and the
    //   devirtualizer refines (pinned in test_sir). So a call site's COMPLETE target set is
    //   enumerable from the class table alone, before any analysis exists; "extra targets"
    //   is an impossible event, and pts can only SHRINK a site toward a singleton.
    printf("== the defunctionalized call graph (spec §7/§10) ==\n");
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        sema_ctx_t ctx;
        analyze_into(
            "class A { int m() { return 1; } int n() { return 9; } }"
            "class B extends A { int m() { return 2; } }"
            "class C extends B { }"
            "class D extends A { int m() { return 4; } }"
            "class E { int m() { return 8; } }"
            "abstract class X { abstract int m(); }", &arena, &ctx);
        int a = sema_find_class(&ctx, "A"), b = sema_find_class(&ctx, "B"),
            c = sema_find_class(&ctx, "C"), d = sema_find_class(&ctx, "D"),
            e = sema_find_class(&ctx, "E"), x = sema_find_class(&ctx, "X");
        int am = method_of(&ctx, a, "m"), an = method_of(&ctx, a, "n");
        int bm = method_of(&ctx, b, "m"), dm = method_of(&ctx, d, "m");
        CHECK(a >= 0 && b >= 0 && c >= 0 && d >= 0 && e >= 0 && x >= 0
              && am >= 0 && an >= 0 && bm >= 0 && dm >= 0, "call-graph hierarchy resolves");

        /* THE RULE, per exact receiver class — what a vtable row holds, what devirt asks. */
        int rc = -1, rm = -1;
        CHECK(sema_resolve_virtual(&ctx, a, a, am, &rc, &rm) && rc == a && rm == am,
              "call graph: exact A → A.m (the declaration)");
        CHECK(sema_resolve_virtual(&ctx, b, a, am, &rc, &rm) && rc == b && rm == bm,
              "call graph: exact B → B.m (the override)");
        CHECK(sema_resolve_virtual(&ctx, c, a, am, &rc, &rm) && rc == b && rm == bm,
              "call graph: exact C → B.m (the INHERITED override — C declares nothing)");
        CHECK(sema_resolve_virtual(&ctx, d, a, am, &rc, &rm) && rc == d && rm == dm,
              "call graph: exact D → D.m (the sibling override)");
        CHECK(sema_resolve_virtual(&ctx, c, a, an, &rc, &rm) && rc == a && rm == an,
              "call graph: exact C → A.n (never overridden anywhere)");
        CHECK(!sema_resolve_virtual(&ctx, x, x, method_of(&ctx, x, "m"), &rc, &rm),
              "call graph: an ABSTRACT method with no impl is NOT RESOLVABLE — spec §7's "
              "bottom-method boundary, fail-closed (the devirtualizer declines on this)");

        /* §15.12.4.4 step 1 searches for "a declaration for a NON-ABSTRACT method named m
         * with the same descriptor". §8.4.3.4: "A compile-time error occurs if a native
         * method is declared abstract" — so a native declaration is non-abstract, it ENDS
         * the lookup, and it is what dispatch lands on. Having no body in THIS module is an
         * emission fact (sema_method_is_defined), not the lookup's predicate: reading it as
         * "abstract" leaves every vtable slot of every native method ref.null, and the
         * dispatch's ref.cast then fails at run time. */
        {
            int obj = ctx.wk.object_id;
            int gc  = method_of(&ctx, obj, "getClass");
            CHECK(obj >= 0 && gc >= 0, "Object.getClass is declared (precondition)");
            CHECK(sema_resolve_virtual(&ctx, obj, obj, gc, &rc, &rm) && rc == obj && rm == gc,
                  "call graph: a NATIVE declaration is non-abstract (§8.4.3.4) and resolves");
            CHECK(sema_resolve_virtual(&ctx, a, obj, gc, &rc, &rm) && rc == obj && rm == gc,
                  "call graph: a subclass inherits the native — §15.12.4.4 step 2 finds it");
        }

        /* THE SET: the complete target set of `((A)r).m()`, enumerated from the class
         * table ALONE — every class in the program (java.lang included), filtered by JLS
         * §4.10.2 subtyping, resolved by the one rule. No engine, no pts, no fixpoint
         * anywhere in this block. This enumeration IS the thing the VFG paper's
         * function-pointer points-to sets exist to approximate. */
        int set_c[8], set_m[8], nset = 0; bool overflow = false;
        for (int k = 0; k < (int)bbq_vec_len(ctx.classes); k++) {
            if (!sema_ref_is_subtype(&ctx, k, a)) continue;
            if (!sema_resolve_virtual(&ctx, k, a, am, &rc, &rm)) continue;
            bool dup = false;
            for (int i = 0; i < nset; i++) if (set_c[i] == rc && set_m[i] == rm) dup = true;
            if (dup) continue;
            if (nset == 8) { overflow = true; break; }
            set_c[nset] = rc; set_m[nset] = rm; nset++;
        }
        bool has_am = false, has_bm = false, has_dm = false, has_e = false;
        for (int i = 0; i < nset; i++) {
            if (set_c[i] == a && set_m[i] == am) has_am = true;
            if (set_c[i] == b && set_m[i] == bm) has_bm = true;
            if (set_c[i] == d && set_m[i] == dm) has_dm = true;
            if (set_c[i] == e) has_e = true;
        }
        CHECK(!overflow && nset == 3 && has_am && has_bm && has_dm,
              "the COMPLETE target set of `A.m()` is exactly {A.m, B.m, D.m} — finite, "
              "closed, computed from the class table with no analysis in sight");
        CHECK(!has_e,
              "E.m (same signature, UNRELATED class) is NOT a target — membership is "
              "receiver subtyping (§4.10.2), not signature match");
        sema_destroy(&ctx); bbq_arena_free(&arena);
    }

    // ── javelina.simd.V128 value semantics ─────────────────────────────────
    // The V128 class is nominally a class to the parser but a VALUE width to
    // sema (resolve_type maps it to JT_V128). The legal positions compile; every
    // reference-semantics use is a compile error. Each negative pins the
    // resolve_type hook: with the hook absent, V128 is an ordinary final class
    // and every one of these compiles — which is exactly the failure mode.
    // Snippets are unnamed-package units, so the simd names need their §7.5
    // import — exactly what a real user writes.
    #define SIMD "import javelina.simd.*; "
    printf("== javelina.simd V128 value semantics ==\n");
    CHECK(analyze(SIMD "class C { static V128 f; V128 m(V128 x) { V128 y = x; return y; } }", false) == 0,
          "V128 local/param/return/static-field declarations compile");
    CHECK(analyze(SIMD "class C { V128[] a; V128 g(V128[] b) { return b[0]; } }", false) == 0,
          "V128[] array declarations and element reads compile");
    CHECK(analyze(SIMD "class C { V128 m() { return null; } }", false) > 0,
          "V128 is not nullable: `return null` is a compile error");
    CHECK(analyze(SIMD "class C { boolean m(V128 a, V128 b) { return a == b; } }", false) > 0,
          "V128 has no == (not numeric, not a reference)");
    CHECK(analyze(SIMD "class C { Object m(V128 a) { return a; } }", false) > 0,
          "V128 does not assignment-convert to Object");
    CHECK(analyze(SIMD "class C { Object m(V128 a) { return (Object)a; } }", false) > 0,
          "V128 does not cast to Object");
    CHECK(analyze(SIMD "class C { V128 m(Object o) { return (V128)o; } }", false) > 0,
          "Object does not cast to V128");
    CHECK(analyze(SIMD "class C { boolean m(Object o) { return o instanceof V128; } }", false) > 0,
          "instanceof V128 is a compile error (not a reference type)");
    CHECK(analyze(SIMD "class C { V128 m(V128 a, V128 b) { return a + b; } }", false) > 0,
          "no operator arithmetic on V128 (ops are intrinsics, not +)");
    CHECK(analyze(SIMD "class C { String m(V128 a) { return \"\" + a; } }", false) > 0,
          "no string concatenation of a V128 (no §5.4 conversion exists)");
    CHECK(analyze(SIMD "class C { void m(java.util.Vector v, V128 a) { v.addElement(a); } }", false) > 0,
          "V128 cannot enter Vector (no Object conversion)");

    // ── javelina.simd intrinsic calls + §15.27 immediate validation ────────
    printf("== javelina.simd intrinsic calls ==\n");
    CHECK(analyze(SIMD "class C { V128 m(V128 a, V128 b) { return I32x4.add(a, b); } }", false) == 0,
          "static intrinsic call through the lane class compiles");
    CHECK(analyze(SIMD "class C { V128 m(V128 a, V128 b) { return V128.and(a, b); } }", false) == 0,
          "static intrinsic call through V128 itself compiles");
    CHECK(analyze(SIMD "class C { int m(V128 a) { return I32x4.extract_lane(a, 2); } }", false) == 0,
          "constant in-range lane compiles");
    CHECK(analyze(SIMD "class C { int m(V128 a, int i) { return I32x4.extract_lane(a, i); } }", false) > 0,
          "NON-constant lane is a compile error (no runtime-lane fallback)");
    CHECK(analyze(SIMD "class C { int m(V128 a) { return I32x4.extract_lane(a, 4); } }", false) > 0,
          "lane 4 of i32x4 is out of range (0..3)");
    CHECK(analyze(SIMD "class C { V128 m() { return V128.const_(1L, 2L); } }", false) == 0,
          "v128.const with constant halves compiles");
    CHECK(analyze(SIMD "class C { V128 m(long x) { return V128.const_(x, 2L); } }", false) > 0,
          "v128.const with a non-constant half is a compile error");
    CHECK(analyze(SIMD "class C { V128 m(V128 a, V128 b) {"
                  " return I8x16.shuffle(a, b, 0x0706050403020100L, 0x0F0E0D0C0B0A0908L); } }", false) == 0,
          "shuffle with an in-range constant mask compiles");
    CHECK(analyze(SIMD "class C { V128 m(V128 a, V128 b) {"
                  " return I8x16.shuffle(a, b, 0x20L, 0L); } }", false) > 0,
          "shuffle mask byte 0x20 (32) is out of range (lane indices are 0..31)");

    // ── linear-memory intrinsics (javelina.simd.Mem; §15.27 lane gate) ─────
    printf("== linear-memory intrinsics (Mem) ==\n");
    CHECK(analyze(SIMD "class C { V128 m(int p) { return Mem.v128_load(p); } }", false) == 0,
          "Mem.v128_load(addr) compiles");
    CHECK(analyze(SIMD "class C { void m(int p, V128 a) { Mem.v128_store(p, a); } }", false) == 0,
          "Mem.v128_store(addr, v) compiles");
    CHECK(analyze(SIMD "class C { int m(int p) { return Mem.i32_load(p); } }", false) == 0,
          "Mem.i32_load(addr) compiles (the scalar family)");
    CHECK(analyze(SIMD "class C { void m(int p, double d) { Mem.f64_store(p, d); } }", false) == 0,
          "Mem.f64_store(addr, v) compiles");
    CHECK(analyze(SIMD "class C { int m() { return Mem.memory_size(); } }", false) == 0,
          "Mem.memory_size() compiles");
    CHECK(analyze(SIMD "class C { void m(int d, int s, int n) { Mem.memory_copy(d, s, n); } }", false) == 0,
          "Mem.memory_copy(dst, src, len) compiles");
    CHECK(analyze(SIMD "class C { V128 m(int p, V128 a) { return Mem.v128_load8_lane(p, a, 15); } }", false) == 0,
          "v128_load8_lane lane 15 (max of 16 lanes) compiles");
    CHECK(analyze(SIMD "class C { V128 m(int p, V128 a) { return Mem.v128_load8_lane(p, a, 16); } }", false) > 0,
          "v128_load8_lane lane 16 is out of range (0..15)");
    CHECK(analyze(SIMD "class C { void m(int p, V128 a) { Mem.v128_store64_lane(p, a, 2); } }", false) > 0,
          "v128_store64_lane lane 2 is out of range (0..1)");
    CHECK(analyze(SIMD "class C { V128 m(int p, V128 a, int i) { return Mem.v128_load32_lane(p, a, i); } }", false) > 0,
          "NON-constant memlane lane is a compile error (no runtime-lane fallback)");

    // ── JLS §7 packages + §6.5.4 type-name resolution ──────────────────────
    // Before the fix: the class
    // table was SIMPLE-name keyed (two Mems clobbered each other), imports
    // were ignored, and every simple name resolved globally.
    printf("== JLS §7 packages / §6.5.4 resolution ==\n");
    CHECK(analyze("package a; public class Twin {}", false) == 0,
          "a named-package compilation unit compiles");
    CHECK(analyze("class C { V128 f; }", false) > 0,
          "V128 WITHOUT the import does not resolve (§6.5.4.1 — no global namespace)");
    CHECK(analyze("class C { javelina.simd.V128 f; javelina.simd.V128 m() { return f; } }", false) == 0,
          "the fully qualified name works without an import (§6.5.4.2)");
    CHECK(analyze("import javelina.simd.V128; class C { V128 f; }", false) == 0,
          "a single-type-import makes the simple name available (§7.5.1)");
    CHECK(analyze("import javelina.simd.NoSuch; class C {}", false) > 0,
          "importing a type that does not exist is a compile error (§7.5.1)");
    CHECK(analyze("import bogus.pkg.*; class C {}", false) > 0,
          "import-on-demand of an unknown package is a compile error (§7.5.2)");
    CHECK(analyze("import java.util.Vector; class Vector { }", false) > 0,
          "the JLS §7.5.1 example: import + same-simple-name local decl = error");
    CHECK(analyze("import java.util.Vector; import java.io.File; class C {"
                  " Vector v; java.io.File f; }", false) == 0,
          "two single-type-imports with distinct simple names coexist");
    CHECK(analyze("import java.util.Vector; import java.util.Vector; class C { Vector v; }", false) == 0,
          "a duplicate import of the SAME type is an ignored dup (§7.5.1)");
    CHECK(analyze("package my; public class Mem { public static int x; }", false) == 0,
          "a second class named Mem in another package coexists with java.io.Mem (FQN keying)");
    CHECK(analyze("package java.io; class File {}", false) > 0,
          "duplicate FQN java.io.File is a compile-time error (was a silent clobber)");

    // Diagnostic quality: the recursion-cycle warning fires on NON-TAIL recursion
    // only (a pure tail cycle lowers to return_call* — O(1) stack), mirroring the
    // burg Return(tail) shape exactly: direct return-of-call, exact result type,
    // outside every protected region. And the AST interval lattice knows `x & m`
    // is within [0, m] for a non-negative mask — the classic self-bounding index.
    printf("== warning precision: tail recursion + masked indexes ==\n");
    CHECK(warn_count("class C { static int f(int n, int a){ if (n == 0) return a;"
                     " return f(n - 1, a + n); } }", SEMA_DIAG_RECURSION_CYCLE) == 0,
          "a pure tail-recursive cycle does NOT warn (return_call reuses the frame)");
    CHECK(warn_count("class C { static int g(int n){ if (n < 2) return n;"
                     " return g(n - 1) + g(n - 2); } }", SEMA_DIAG_RECURSION_CYCLE) == 1,
          "non-tail recursion warns (each call grows a frame)");
    CHECK(warn_count("class C { static int h(int n){ try { if (n == 0) return 0;"
                     " return h(n - 1); } catch (RuntimeException e) { return 1; } } }",
                     SEMA_DIAG_RECURSION_CYCLE) == 1,
          "a `return h(...)` INSIDE a try is a non-tail edge (the frame holds the handlers)");
    CHECK(warn_count("class C { static int k(int x){ int[] a = new int[8];"
                     " return a[x & 7]; } }", SEMA_DIAG_ARRAY_BOUNDS) == 0,
          "a[x & 7] on int[8] is provably in bounds (mask rule) — no warning");
    CHECK(warn_count("class C { static int m(int x){ int[] a = new int[8];"
                     " return a[x & 15]; } }", SEMA_DIAG_ARRAY_BOUNDS) == 1,
          "a[x & 15] on int[8] still warns — the checker did not go blind");

    /* ── Two-lattice parity (the sema side of the plan's audit table; the
     * Click twins live in test_sir's "range strides" + parity blocks — a
     * rule added to one side must be added to the other or its twin pin
     * goes red). */
    printf("== interval parity: shift / narrowing conv / length / wrap ==\n");
    CHECK(warn_count("class C { static int f(int x){ int[] a = new int[8];"
                     " return a[(x & 3) << 1]; } }", SEMA_DIAG_ARRAY_BOUNDS) == 0,
          "spec T2 <<: a[(x & 3) << 1] is [0,6] on int[8] — no warning");
    CHECK(warn_count("class C { static int g(int x){ if (x < 0) return 0; if (x > 299) return 0;"
                     " int[] a = new int[300]; return a[(byte) x]; } }",
                     SEMA_DIAG_ARRAY_BOUNDS) == 1,
          "SOUNDNESS: (byte)x of [0,299] does NOT fit — the cast can go negative, warn");
    CHECK(warn_count("class C { static int h(int x){ if (x < 0) return 0; if (x > 100) return 0;"
                     " int[] a = new int[128]; return a[(byte) x]; } }",
                     SEMA_DIAG_ARRAY_BOUNDS) == 0,
          "(byte)x of [0,100] FITS — the interval passes through, no warning");
    CHECK(warn_count("class C { static int k(int[] a){ int[] b = new int[4];"
                     " return b[a.length % 4]; } }", SEMA_DIAG_ARRAY_BOUNDS) == 0,
          "spec T3: ANY length is >= 0, so length % 4 is [0,3] — no warning");
    CHECK(warn_count("class C { static int w(int x){ if (x < 0) return 0;"
                     " int[] a = new int[8]; return a[(x * 4) % 8]; } }",
                     SEMA_DIAG_ARRAY_BOUNDS) == 1,
          "SOUNDNESS: x*4 can WRAP int for large x, so %8 can go negative — warn");

    /* ══ The PLUGIN import set — six properties, each failing for its own reason ═══════════
     *
     * §13.4.5 (p.245): "No incompatibility with pre-existing binaries is caused by adding a
     * class member ... References to the original field or method were resolved at compile
     * time to a SYMBOLIC REFERENCE containing the name of the class in which they were
     * declared." That guarantee rests on a mechanism — one symbolic reference PER USE — and
     * a plugin importing the library's whole surface has no such record.
     *
     * The set has to be pinned in BOTH directions, and the reason is a failure I already had:
     * narrowing it to (referenced ∪ virtual ∪ synthetic) made the emitted module fail
     * validation with "unknown function". Minimality alone would have called that narrowing
     * an improvement. So MIN-* say what must not be in the set, COMPLETE-* say what must,
     * and a narrowing has to satisfy all of them at once.
     *
     * All six read the FINAL set (sema_import_count / sema_import_at) because that is what
     * the module assembler consumes. Where a property holds today it holds TRIVIALLY — the
     * set is currently "everything" — and it is written down precisely so that the eventual
     * narrowing cannot break it silently. */

    /* §13.1 DECLARING CLASS. "A reference to a method or constructor must be resolved at
     * compile time to a symbolic reference to THE CLASS OR INTERFACE IN WHICH the denoted
     * method or constructor IS DECLARED, plus the signature of the method or constructor.
     * ... this makes the binaries more robust."
     *
     * So the recorded class is where the method is DECLARED, not the static type it was
     * reached through. `sb.equals(o)` on a StringBuffer must record java.lang.Object, which
     * declares equals — recording StringBuffer would be a reference to a method StringBuffer
     * does not declare, and the robustness §13.1 is buying is exactly that a later release
     * may add StringBuffer.equals without invalidating this binary.
     *
     * This is the one §13.1 rule the existing (class_id, method_id) row can already express,
     * which is why it is the first one written. */
    {
        bbq_arena ar; bbq_arena_init(&ar, 1 << 16);
        jt_ref_t r = plugin_ref_for(
            "class T { static boolean f(Object o){ StringBuffer b = new StringBuffer();"
            " return b.equals(o); } }", "equals", &ar);
        printf("    JLS 13.1 b.equals(o) recorded against class '%s'\n",
               r.found ? r.decl_class : "(not recorded)");
        CHECK(r.found && !strcmp(r.decl_class, "Object"),
              "JLS 13.1: a method reference records the class that DECLARES it "
              "(Object.equals, not StringBuffer.equals)");
        bbq_arena_free(&ar);
    }

    /* §13.1 SIGNATURE. "The signature of a method must include all of the following: the
     * simple name of the method; the number of parameters to the method; a symbolic reference
     * to the type of each parameter" — and "a reference to a method must also include either
     * a symbolic reference to the return type of the denoted method or an indication that the
     * denoted method is declared void".
     *
     * So a reference must name ONE overload, not a name. StringBuffer.append is overloaded on
     * int, char, String, Object...; `b.append(x)` with an int x must record the (int) one, and
     * the recorded row must be distinguishable from the (String) one. */
    {
        bbq_arena ar; bbq_arena_init(&ar, 1 << 16);
        jt_ref_t i_ref = plugin_ref_for(
            "class T { static void f(int x){ StringBuffer b = new StringBuffer();"
            " b.append(x); } }", "append", &ar);
        bbq_arena ar2; bbq_arena_init(&ar2, 1 << 16);
        jt_ref_t s_ref = plugin_ref_for(
            "class T { static void f(String x){ StringBuffer b = new StringBuffer();"
            " b.append(x); } }", "append", &ar2);
        printf("    JLS 13.1 append(int) recorded param0='%s'; append(String) param0='%s'\n",
               i_ref.found ? i_ref.param0 : "(none)", s_ref.found ? s_ref.param0 : "(none)");
        CHECK(i_ref.found && s_ref.found
              && !strcmp(i_ref.param0, "int") && !strcmp(s_ref.param0, "String"),
              "JLS 13.1: a method reference names ONE overload — the signature carries a "
              "symbolic reference to each parameter type");
        bbq_arena_free(&ar); bbq_arena_free(&ar2);
    }

    /* §13.1 CONSTANT FIELDS — a mandated ABSENCE. "References to fields that are static, final,
     * and initialized with compile-time constant expressions are resolved at compile time to the
     * constant value that is denoted. NO REFERENCE to such a constant field should be present in
     * the code in a binary file (except in the class or interface containing the constant field,
     * which will have code to initialize it)."
     *
     * Paired so it cannot pass vacuously: a NON-constant static field read must still be
     * recorded. Recording nothing at all would satisfy the absence half alone. */
    {
        bbq_arena ar; bbq_arena_init(&ar, 1 << 16);
        jt_kinds_t k = plugin_ref_kinds(
            "class T {\n"
            "    static final int K = 7;\n"           /* constant: folded, no reference */
            "    static int mutable_;\n"              /* not final: a real reference */
            "    static int f(){ return K + mutable_; }\n"
            "}", "T", &ar);
        printf("    JLS 13.1 field refs from T: K=%d  mutable_=%d\n", k.ref_K, k.ref_mut);
        CHECK(k.ok && k.ref_mut > 0 && k.ref_K == 0,
              "JLS 13.1: a constant field read leaves NO reference; a non-constant one does");
        bbq_arena_free(&ar);
    }

    /* §13.1 PER-CLASS. "A Java binary representation for a class or interface must also contain
     * all of the following: If it is a class and is not class java.lang.Object, then a symbolic
     * reference to the direct superclass of this class; a symbolic reference to each direct
     * superinterface, if any." */
    {
        bbq_arena ar; bbq_arena_init(&ar, 1 << 16);
        jt_kinds_t k = plugin_ref_kinds(
            "class T implements Cloneable { int v; }", "T", &ar);
        CHECK(k.ok && k.n_super == 1 && k.n_iface == 1,
              "JLS 13.1: a class records its direct superclass and each direct superinterface");
        bbq_arena_free(&ar);
    }

    /* MIN-1: a library class the program never mentions contributes NOTHING. java.util.Vector
     * is parsed (javelinac globs all of lang/util/io) but unreferenced here, so under
     * "one symbolic reference per use" it must not appear.
     *
     * RED TODAY: sema.c:4336-4343 sweeps every real java.lang source class into the set. */
    {
        bbq_arena ar; bbq_arena_init(&ar, 1 << 16);
        jt_imports_t r = plugin_imports("class T { static int f(int x){ return x + 1; } }",
                                        "Vector", &ar);
        CHECK(r.ok && r.for_class == 0,
              "MIN-1: an unreferenced library class contributes no imports (JLS 13.4.5)");
        bbq_arena_free(&ar);
    }

    /* MIN-2: the set RESPONDS to what the program touches. Two programs, one trivial and one
     * using StringBuffer; if the counts are equal the set is a constant and cannot be a
     * record of uses at all. This is the summary property — it is what actually breaks a
     * previously-built plugin when lib/java gains a member.
     *
     * RED TODAY, for the same cause as MIN-1. Kept separate because a narrowing could fix
     * one and not the other: filtering only unreferenced CLASSES leaves MIN-2 red while
     * MIN-1 passes, since an unreferenced METHOD of a referenced class still rides along. */
    {
        bbq_arena a1; bbq_arena_init(&a1, 1 << 16);
        jt_imports_t few = plugin_imports("class T { static int f(int x){ return x + 1; } }",
                                          "StringBuffer", &a1);
        bbq_arena a2; bbq_arena_init(&a2, 1 << 16);
        jt_imports_t many = plugin_imports(
            "class T { static int f(int x){ StringBuffer b = new StringBuffer();"
            " b.append(x); return b.toString().length(); } }", "StringBuffer", &a2);

        printf("    plugin imports: trivial=%d  uses-StringBuffer=%d\n", few.total, many.total);
        CHECK(few.ok && many.ok && few.total > 0 && few.total < many.total,
              "MIN-2: the import set is a record of USES — touching more of java.lang "
              "imports more (JLS 13.4.5)");
        bbq_arena_free(&a1); bbq_arena_free(&a2);
    }

    /* ── The well-known stamps land on the method they NAME ───────────────────
     *
     * resolve_wellknown_methods attaches lowering decisions to library methods:
     * math_kind picks an f64 opcode, move_kind a raw bitcast, class_kind a call
     * form. Every one of those stampers found its target by NAME and took the
     * first hit — and §8.4.7 lets any number of methods share a name. In
     * java.lang.Math alone, abs/min/max/round have twelve overloads between
     * them, so "the name is unique" was a property of today's prelude, not an
     * invariant, and the failure mode is an intrinsic bound to the wrong
     * overload: `Math.abs(double)` lowering to an i32 op, silently.
     *
     * ret_nonnull already stamped every overload (its own comment cites
     * String.valueOf's nine and StringBuffer.append's sixteen). Nothing pinned
     * either behaviour, so this pins both: the kind stamps land on exactly one
     * method and it is the one with the right signature, and the nonnull stamp
     * covers a whole overload set rather than one member of it. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        sema_ctx_t s;
        bool ok = analyze_into("class T { static int f(){ return 0; } }", &a, &s);
        CHECK(ok, "well-known: the prelude analyzed");
        if (ok) {
            /* (kind, name, params, return) for each f64 math intrinsic. */
            struct { int kind; const char* name; } mk[] = {
              { 1, "sqrt" }, { 2, "floor" }, { 3, "ceil" }, { 4, "rint" },
            };
            int math_id = sema_find_class(&s, "java.lang.Math");
            CHECK(math_id >= 0, "well-known: found java.lang.Math");
            if (math_id >= 0) {
                const sema_class_t* mc = sema_get_class(&s, math_id);
                for (int k = 0; k < (int)(sizeof mk / sizeof mk[0]); k++) {
                    int hits = 0, good = 0;
                    for (int i = 0; i < (int)bbq_vec_len(mc->methods); i++) {
                        const sema_method_t* m = &mc->methods[i];
                        if (m->math_kind != mk[k].kind) continue;
                        hits++;
                        if (strcmp(m->name, mk[k].name) == 0 && m->param_count == 1
                            && m->param_types[0].tag == JT_DOUBLE
                            && m->return_type.tag == JT_DOUBLE) good++;
                    }
                    char lbl[128];
                    snprintf(lbl, sizeof lbl,
                             "Math.%s: exactly one method carries the intrinsic, "
                             "and it is (double)->double", mk[k].name);
                    CHECK(hits == 1 && good == 1, lbl);
                }
                /* The overloaded neighbours must carry NO intrinsic at all. */
                int stray = 0;
                for (int i = 0; i < (int)bbq_vec_len(mc->methods); i++) {
                    const sema_method_t* m = &mc->methods[i];
                    if (m->math_kind == 0) continue;
                    if (strcmp(m->name, "sqrt") && strcmp(m->name, "floor")
                        && strcmp(m->name, "ceil") && strcmp(m->name, "rint")) stray++;
                }
                CHECK(stray == 0,
                      "Math: abs/min/max/round carry no f64 intrinsic (a name-keyed "
                      "stamp would reach the first overload of one of them)");
            }
            /* move_kind: each bitcast is a distinct (param, return) pair. */
            struct { const char* cls; const char* name; int kind;
                     java_type_tag_t p, r; } mv[] = {
              { "java.lang.Float",  "floatToRawIntBits",   1, JT_FLOAT,  JT_INT    },
              { "java.lang.Float",  "intBitsToFloat",      2, JT_INT,    JT_FLOAT  },
              { "java.lang.Double", "doubleToRawLongBits", 3, JT_DOUBLE, JT_LONG   },
              { "java.lang.Double", "longBitsToDouble",    4, JT_LONG,   JT_DOUBLE },
            };
            for (int k = 0; k < (int)(sizeof mv / sizeof mv[0]); k++) {
                int cid = sema_find_class(&s, mv[k].cls);
                int hits = 0, good = 0;
                if (cid >= 0) {
                    const sema_class_t* c = sema_get_class(&s, cid);
                    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++) {
                        const sema_method_t* m = &c->methods[i];
                        if (m->move_kind != mv[k].kind) continue;
                        hits++;
                        if (strcmp(m->name, mv[k].name) == 0 && m->param_count == 1
                            && m->param_types[0].tag == mv[k].p
                            && m->return_type.tag == mv[k].r) good++;
                    }
                }
                char lbl[128];
                snprintf(lbl, sizeof lbl,
                         "%s: the bitcast intrinsic is on the right signature", mv[k].name);
                CHECK(cid >= 0 && hits == 1 && good == 1, lbl);
            }
            /* Every javelina.simd stub carries exactly one opcode, and it is the
             * one whose generated descriptor matches its signature. The bind used
             * to be name-only with first-match-wins, so the first convenience
             * overload the API grows would have taken the wrong opcode. */
            {
                bbq_arena sa; bbq_arena_init(&sa, 1 << 20);
                sema_ctx_t ss;
                bool sok = analyze_into(
                    SIMD "class T { static int f(V128 a){ return I32x4.bitmask(a); } }",
                    &sa, &ss);
                int stamped = 0, dup = 0;
                /* Every id in the generated table must be claimed by exactly one
                 * stub: `stamped` counts the binds, and a repeated id means two
                 * stubs took the same opcode — the shape a name-keyed bind
                 * produces the moment a name is overloaded. */
                int* seen = (int*)calloc(SIMD_INTRINSIC_COUNT + 1, sizeof(int));
                for (int c = 0; sok && c < (int)bbq_vec_len(ss.classes); c++) {
                    const sema_class_t* sc = sema_get_class(&ss, c);
                    for (int i = 0; i < (int)bbq_vec_len(sc->methods); i++) {
                        int id = sc->methods[i].simd_id;
                        if (id <= 0 || id > SIMD_INTRINSIC_COUNT) continue;
                        stamped++;
                        if (seen[id]++) dup++;
                    }
                }
                free(seen);
                char lbl[144];
                snprintf(lbl, sizeof lbl,
                         "javelina.simd: all %d table entries bound to a stub, one "
                         "each (%d bound, %d collisions)", SIMD_INTRINSIC_COUNT, stamped, dup);
                CHECK(sok && stamped == SIMD_INTRINSIC_COUNT && dup == 0, lbl);
                sema_destroy(&ss); bbq_arena_free(&sa);
            }
            /* ret_nonnull is a WHOLE-overload-set contract, not one member. */
            struct { const char* cls; const char* name; } nn[] = {
              { "java.lang.StringBuffer", "append"    },
              { "java.lang.StringBuffer", "insert"    },
              { "java.lang.String",       "valueOf"   },
              { "java.lang.String",       "substring" },
            };
            for (int k = 0; k < (int)(sizeof nn / sizeof nn[0]); k++) {
                int cid = sema_find_class(&s, nn[k].cls);
                int named = 0, stamped = 0;
                if (cid >= 0) {
                    const sema_class_t* c = sema_get_class(&s, cid);
                    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
                        if (strcmp(c->methods[i].name, nn[k].name) == 0) {
                            named++;
                            if (c->methods[i].ret_nonnull) stamped++;
                        }
                }
                char lbl[144];
                snprintf(lbl, sizeof lbl,
                         "%s.%s: all %d overloads carry ret_nonnull, not just the first",
                         nn[k].cls, nn[k].name, named);
                CHECK(cid >= 0 && named > 1 && stamped == named, lbl);
            }
        }
        sema_destroy(&s); bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_sema");
}
