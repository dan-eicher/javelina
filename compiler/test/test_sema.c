// test_sema.c — Section 2 (sema): AST + java.lang environment -> typed AST.
// Plain-C, exit-code based. The class environment is supplied the spec way:
// java.lang is parsed from lib/java/lang/*.java (real signatures, JLS 1.0 ch.20)
// and merged into the program ahead of user code — no .exp loader. Tests are
// grounded in the JLS check rules, not guesswork.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
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

    return TEST_SUMMARY("test_sema");
}
