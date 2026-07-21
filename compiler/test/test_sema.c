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

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL  %s\n", (m)); fails++; } } while (0)

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    buf[n] = '\0'; fclose(f);
    return buf;
}

// Parse one source string into a program AST (each parse gets its own arena;
// nothing is freed — fine for a test).
static ast_program_t* parse_src(const char* src) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16);
    pc->result = NULL; pc->file = NULL;
    peg_state p;
    java_parser_init(&p, src, (int)strlen(src));
    p.user_data = pc;
    if (!java_parser_parse(&p)) return NULL;
    return pc->result;
}

// Build one program = every java.lang stub's type decls + the user source's.
static ast_program_t* build_program(const char* user_src, bbq_arena* arena) {
    ast_type_decl_t** types = NULL; int tc = 0, cap = 0;
    #define PUSH(td) do { if (tc==cap){ cap=cap?cap*2:64; types=realloc(types,(size_t)cap*sizeof(*types)); } types[tc++]=(td); } while(0)

    /* The stdlib environment: java.lang no longer stands alone — java.lang.System.out/err/in are
     * java.io.PrintStream/InputStream, so java.io (and its java.util deps) must be in the env too. */
    const char* dirs[] = { "lib/java/lang", "lib/java/util", "lib/java/io" };
    struct dirent* e;
    for (int di = 0; di < 3; di++) {
        DIR* d = opendir(dirs[di]);
        if (!d) { printf("  FAIL  cannot open %s (run from compiler/)\n", dirs[di]); fails++; return NULL; }
        while ((e = readdir(d)) != NULL) {
            size_t L = strlen(e->d_name);
            if (L < 6 || strcmp(e->d_name + L - 5, ".java") != 0) continue;
            char path[512]; snprintf(path, sizeof(path), "%s/%s", dirs[di], e->d_name);
            char* src = read_file(path);
            if (!src) { printf("  FAIL  read %s\n", path); fails++; continue; }
            ast_program_t* p = parse_src(src);
            if (!p) { printf("  FAIL  parse %s\n", path); fails++; continue; }
            for (int i = 0; i < p->types_count; i++) PUSH(p->types[i]);
        }
        closedir(d);
    }

    if (user_src) {
        ast_program_t* up = parse_src(user_src);
        if (!up) { printf("  FAIL  parse user source\n"); fails++; }
        else for (int i = 0; i < up->types_count; i++) PUSH(up->types[i]);
    }

    ast_type_decl_t** arr = (ast_type_decl_t**)bbq_arena_alloc(arena, (size_t)tc * sizeof(*arr));
    memcpy(arr, types, (size_t)tc * sizeof(*arr));
    free(types);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
    #undef PUSH
}

// Analyze user source against java.lang; return the error count (and dump diags).
static int analyze(const char* user_src, bool dump) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    ast_program_t* prog = build_program(user_src, &arena);
    if (!prog) return -1;
    sema_ctx_t ctx;
    sema_init(&ctx, &arena);
    sema_analyze(&ctx, prog);
    int n = 0;
    const sema_diag_t* diags = sema_diags(&ctx, &n);
    if (dump) for (int i = 0; i < n; i++)
        printf("    diag %d:%d  %s\n", diags[i].loc.line, diags[i].loc.col, diags[i].message);
    return sema_error_count(&ctx);
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
        ast_program_t* prog = build_program(
            "class A { static int x = 5; }"               /* own static field init → needs */
            "class B { static { int y = 1; } }"           /* static block → needs */
            "class Cc {}"                                 /* nothing → no */
            "class D extends A {}"                        /* super needs → inherits */
            "class E extends Cc { int z = 3; }", &arena); /* instance init only, clean super → no */
        sema_ctx_t ctx; sema_init(&ctx, &arena); sema_analyze(&ctx, prog);
        int a=sema_find_class(&ctx,"A"), b=sema_find_class(&ctx,"B"), cc=sema_find_class(&ctx,"Cc"),
            d=sema_find_class(&ctx,"D"), e=sema_find_class(&ctx,"E");
        CHECK(a>=0  && sema_get_class(&ctx,a)->needs_init,   "static field init → needs_init");
        CHECK(b>=0  && sema_get_class(&ctx,b)->needs_init,   "static block → needs_init");
        CHECK(cc>=0 && !sema_get_class(&ctx,cc)->needs_init, "no static init → no needs_init");
        CHECK(d>=0  && sema_get_class(&ctx,d)->needs_init,   "subclass of needs_init class inherits needs_init");
        CHECK(e>=0  && !sema_get_class(&ctx,e)->needs_init,  "instance-init-only w/ clean super → no needs_init");
    }

    // 1a5. §12.4.2 synthesis: a needs_init class gets a $initstate static field AND a $ensure_init method
    // (the barrier target); a class that needs no init gets neither.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        ast_program_t* prog = build_program(
            "class N { static int s = 7; }"      /* needs init */
            "class P {}", &arena);               /* does not */
        sema_ctx_t ctx; sema_init(&ctx, &arena); sema_analyze(&ctx, prog);
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
    }

    // 1b. §10 arrays: the RefArray overlay is synthesized as a real class (every
    // reference array is represented by it). Structural pin of the synthesis.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        ast_program_t* prog = build_program("class T {}", &arena);
        sema_ctx_t ctx; sema_init(&ctx, &arena); sema_analyze(&ctx, prog);
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
    }

    // 1c. §20.9/§20.10 raw bit accessors are stamped with their Move* intrinsic kind ONCE
    //     (read at each call site, never re-strcmp'd); ordinary methods stay 0.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        ast_program_t* prog = build_program("class T {}", &arena);
        sema_ctx_t ctx; sema_init(&ctx, &arena); sema_analyze(&ctx, prog);
        struct { const char* cls; const char* meth; int kind; } want[] = {
            { "Float",  "floatToRawIntBits",   1 }, { "Float",  "intBitsToFloat",   2 },
            { "Double", "doubleToRawLongBits", 3 }, { "Double", "longBitsToDouble", 4 },
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
        int fid = sema_find_class(&ctx, "Float");
        const sema_class_t* fc = fid >= 0 ? sema_get_class(&ctx, fid) : NULL;
        for (int i = 0; fc && i < (int)bbq_vec_len(fc->methods); i++)
            if (strcmp(fc->methods[i].name, "floatValue") == 0)
                CHECK(fc->methods[i].move_kind == 0, "Float.floatValue not a move intrinsic");
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
        ast_program_t* prog = build_program(
            "class A { int m() { return 1; } int n() { return 9; } }"
            "class B extends A { int m() { return 2; } }"
            "class C extends B { }"
            "class D extends A { int m() { return 4; } }"
            "class E { int m() { return 8; } }"
            "abstract class X { abstract int m(); }", &arena);
        sema_ctx_t ctx; sema_init(&ctx, &arena); sema_analyze(&ctx, prog);
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
        bbq_arena_free(&arena);
    }

    if (fails) { printf("test_sema: %d FAILED\n", fails); return 1; }
    printf("test_sema: OK\n");
    return 0;
}
