// test_codegen_object.c — #26 end-to-end. The struct-op field path through the
// REAL pipeline: parse → sema → ddcg → burg, with wasm_types supplying field
// layout. This is the level the hand-built test_codegen_wasm cannot reach: it
// proves GetField/PutField emit the correct ABSOLUTE struct field index (the
// declaring class's field base + the field's instance position), not a dead
// -1 constant-pool token the ddcg used to pass.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/codegen_method.h"
#include "javelina/compiler/wasm_types.h"
#include "javelina/compiler/type_lattice.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}
static ast_program_t* parse_src(const char* src) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16); pc->result = NULL; pc->file = NULL;
    peg_state p; java_parser_init(&p, src, (int)strlen(src)); p.user_data = pc;
    return java_parser_parse(&p) ? pc->result : NULL;
}
static ast_program_t* build_program(const char* user_src, bbq_arena* arena) {
    ast_type_decl_t** t = NULL; int tc = 0, cap = 0;
    #define PUSH(td) do { if(tc==cap){cap=cap?cap*2:64;t=realloc(t,(size_t)cap*sizeof(*t));} t[tc++]=(td);}while(0)
    DIR* d = opendir("lib/java/lang");
    if (d) { struct dirent* e;
        while ((e = readdir(d))) { size_t L=strlen(e->d_name);
            if (L<6 || strcmp(e->d_name+L-5,".java")) continue;
            char path[512]; snprintf(path,sizeof path,"lib/java/lang/%s",e->d_name);
            char* s = read_file(path); if(!s) continue;
            ast_program_t* p = parse_src(s); if(!p) continue;
            for (int i=0;i<p->types_count;i++) PUSH(p->types[i]);
        } closedir(d);
    }
    ast_program_t* up = parse_src(user_src);
    if (up) for (int i=0;i<up->types_count;i++) PUSH(up->types[i]);
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr,t,(size_t)tc*sizeof(*arr)); free(t);
    return ast_program(arena, NULL, NULL, 0, arr, tc);
    #undef PUSH
}
static int find_class(const sema_ctx_t* s, const char* name) {
    for (int i = 0; i < (int)bbq_vec_len(s->classes); i++) {
        const sema_class_t* c = sema_get_class(s, i);
        if (c->name && !strcmp(c->name, name)) return i;
    }
    return -1;
}
static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}
/* Search for "0xFB <op2> <typeidx> <field>" — LEB-aware, since the topologically
 * remapped class typeidx can be ≥ 128 (so not a single byte). */
static int has_structop(const uint8_t* body, int n, uint8_t op2, int32_t typeidx, int field) {
    emit_wasm_ctx w = {0};
    ew_byte(&w, 0xFB); ew_byte(&w, op2);
    ew_u32(&w, (uint32_t)typeidx); ew_u32(&w, (uint32_t)field);
    int r = contains(body, n, w.code, (int)bbq_vec_len(w.code));
    bbq_vec_free(w.code);
    return r;
}

/* Compile `src`; structured-emit method `name` with `wt` threaded for field
 * layout; return the body bytes (len in *out_len). */
static const uint8_t* emit(bbq_arena* a, const char* src, const char* name,
                           sema_ctx_t* sctx, wasm_types_t* wt, int* out_len) {
    ast_program_t* prog = build_program(src, a);
    sema_init(sctx, a); sema_analyze(sctx, prog);
    static compiler_ctx_t cctx; compiler_init(&cctx, a, sctx);
    int mc = 0; sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    wasm_types_build(wt, sctx);
    for (int i = 0; i < mc; i++) {
        if (!methods[i]->name || strcmp(methods[i]->name, name)) continue;
        int nsc = 0; const compiler_fact_t* sc = compiler_get_facts(&cctx, i, &nsc);
        static burg_ctx_t bc; bc = (burg_ctx_t){0}; burg_ctx_init(&bc);
        bc.types = wt;                      /* the layout authority for field_abs */
        codegen_method_structured(methods[i], sc, nsc, &bc);
        *out_len = (int)bbq_vec_len(bc.emit.code);
        return bc.emit.code;
    }
    *out_len = 0; return NULL;
}

int main(void) {
    const char* SRC =
        "class Obj { int a; int b;"
        "  int getA(){ return a; }"
        "  int getB(){ return b; }"
        "  void setB(int v){ b = v; } }";

    /* getA: read field a (instance index 0) → struct.get Obj 0. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, SRC, "getA", &s, &wt, &n);
        int obj = find_class(&s, "Obj");
        CHECK(body && obj >= 0, "getA compiled; found Obj");
        CHECK(has_structop(body, n, 0x02, wasm_types_class_typeidx(&wt, obj), 2),
              "getA: struct.get Obj field a (absolute idx 2 = header + inherited hash + a)");
        sema_destroy(&s); bbq_arena_free(&a);
    }
    /* getB: read field b (instance index 1) → struct.get Obj 1 — the absolute
     * index, NOT the dead -1 the old CP path produced. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, SRC, "getB", &s, &wt, &n);
        int obj = find_class(&s, "Obj");
        CHECK(body != NULL, "getB compiled");
        CHECK(has_structop(body, n, 0x02, wasm_types_class_typeidx(&wt, obj), 3),
              "getB: struct.get Obj field b (absolute idx 3, not -1)");
        sema_destroy(&s); bbq_arena_free(&a);
    }
    /* setB: write field b → struct.set Obj 1. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, SRC, "setB", &s, &wt, &n);
        int obj = find_class(&s, "Obj");
        CHECK(body != NULL, "setB compiled");
        CHECK(has_structop(body, n, 0x05, wasm_types_class_typeidx(&wt, obj), 3),
              "setB: struct.set Obj field b (absolute idx 3)");
        sema_destroy(&s); bbq_arena_free(&a);
    }
    /* allocation: new Obj → struct.new at Obj's (topo-remapped) typeidx, with
     * field 0 = the class's populated vtable global and the data fields defaulted
     * (struct.new, NOT struct.new_default — every object carries its vtable). The
     * expected sequence comes from the one authority (wasm_types_emit_new). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, "class Obj { int v; static Obj mk(){ return new Obj(); } }", "mk", &s, &wt, &n);
        int obj = find_class(&s, "Obj");
        CHECK(body && obj >= 0, "new compiled; found Obj");
        emit_wasm_ctx w = {0};
        wasm_types_emit_new(&wt, &w, obj);
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "new: struct.new Obj (global.get vtable + field defaults)");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* LoadThis: `this` is local 0, re-narrowed to the method's class for member
     * access — local.get 0; ref.cast (ref class). A virtual method takes param 0
     * at the root class, so the cast is load-bearing; for a non-virtual method it
     * is the identity (local 0 is already the own class). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, "class W { int x; int get(){ return x; } }", "get", &s, &wt, &n);
        int w = find_class(&s, "W");
        CHECK(body && w >= 0, "this: get compiled; found W");
        emit_wasm_ctx lt = {0};
        ew_byte(&lt, 0x20); ew_u32(&lt, 0);                       /* local.get 0 */
        ew_byte(&lt, 0xFB); ew_u32(&lt, 0x16);                    /* ref.cast (ref W) */
        ew_i32(&lt, wasm_types_class_typeidx(&wt, w));
        CHECK(contains(body, n, lt.code, (int)bbq_vec_len(lt.code)),
              "this: local.get 0; ref.cast (ref W)");
        bbq_vec_free(lt.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* INHERITANCE: B extends A; B's methods access A's field x. The struct.get/
     * set must target the DECLARING class A (typeidx A, field 0), not the
     * receiver B — `this` (a B) is a subtype of A, so the access is valid. This
     * is what sema_field_decl_class buys: receiver class != declaring class. */
    const char* INH =
        "class A { int x; }"
        "class B extends A { int get(){ return x; } void set(int v){ x = v; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, INH, "get", &s, &wt, &n);
        int ca = find_class(&s, "A");
        CHECK(body && ca >= 0, "inherited get compiled; found A");
        CHECK(has_structop(body, n, 0x02, wasm_types_class_typeidx(&wt, ca), 2),
              "inherited read: struct.get DECLARING class A, field 0");
        sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, INH, "set", &s, &wt, &n);
        int ca = find_class(&s, "A");
        CHECK(body != NULL, "inherited set compiled");
        CHECK(has_structop(body, n, 0x05, wasm_types_class_typeidx(&wt, ca), 2),
              "inherited write: struct.set DECLARING class A, field 0");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* QUALIFIED access to an inherited field: `b.x` where b is a B and x is
     * declared in A. The qualified-read (GetFieldOp) and qualified-write
     * (assign_instance_field) paths must also resolve to declaring class A. */
    const char* QUAL =
        "class A { int x; }"
        "class B extends A { }"
        "class C { int rd(B b){ return b.x; } void wr(B b, int v){ b.x = v; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, QUAL, "rd", &s, &wt, &n);
        int ca = find_class(&s, "A");
        CHECK(body && ca >= 0, "qualified rd compiled; found A");
        CHECK(has_structop(body, n, 0x02, wasm_types_class_typeidx(&wt, ca), 2),
              "qualified read b.x: struct.get declaring class A");
        sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, QUAL, "wr", &s, &wt, &n);
        int ca = find_class(&s, "A");
        CHECK(body != NULL, "qualified wr compiled");
        CHECK(has_structop(body, n, 0x05, wasm_types_class_typeidx(&wt, ca), 2),
              "qualified write b.x: struct.set declaring class A");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* STATICS: a static field is a module global. GetStatic→global.get (0x23),
     * PutStatic→global.set (0x24), at the field's global index (which depends on
     * static fields in the java.lang stubs, so compute it via the registry). */
    const char* ST =
        "class S { static int g;"
        "  static int rd(){ return g; }"
        "  static void wr(int v){ g = v; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, ST, "rd", &s, &wt, &n);
        int sid = find_class(&s, "S");
        CHECK(body && sid >= 0, "static rd compiled; found S");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0x23); ew_u32(&w, (uint32_t)wasm_global_index(&wt, sid, 0)); /* global.get g */
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "static read g: global.get at g's global index");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, ST, "wr", &s, &wt, &n);
        int sid = find_class(&s, "S");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0x24); ew_u32(&w, (uint32_t)wasm_global_index(&wt, sid, 0)); /* global.set g */
        CHECK(body != NULL, "static wr compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "static write g: global.set at g's global index");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* ARRAYS: int[] create / load / store / length. The array typeidx is
     * registered on demand; read it back from wt after emit. array.new_default
     * (FB 07), array.get (FB 0B), array.set (FB 0E), array.len (FB 0F). */
    const char* AR =
        "class Arr {"
        "  int[] make(){ return new int[3]; }"
        "  int get0(int[] a){ return a[0]; }"
        "  void set0(int[] a, int v){ a[0] = v; }"
        "  int len(int[] a){ return a.length; } }";
    {   /* new int[3] → i32.const 3; array.new_default <int[]> */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AR, "make", &s, &wt, &n);
        int32_t tid = wasm_types_array_for_dt(&wt, SIR_DTINT);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x07); ew_u32(&w, (uint32_t)tid);
        CHECK(body != NULL, "array make compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "make: array.new_default int[]");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {   /* a[0] → array.get <int[]> */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AR, "get0", &s, &wt, &n);
        int32_t tid = wasm_types_array_for_dt(&wt, SIR_DTINT);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x0B); ew_u32(&w, (uint32_t)tid);
        CHECK(body != NULL, "array get0 compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "get0: array.get int[]");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {   /* a[0] = v → array.set <int[]> */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AR, "set0", &s, &wt, &n);
        int32_t tid = wasm_types_array_for_dt(&wt, SIR_DTINT);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x0E); ew_u32(&w, (uint32_t)tid);
        CHECK(body != NULL, "array set0 compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "set0: array.set int[]");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {   /* a.length → array.len (no typeidx) */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AR, "len", &s, &wt, &n);
        const uint8_t want[] = { 0xFB, 0x0F };   /* array.len */
        CHECK(body != NULL, "array len compiled");
        CHECK(contains(body, n, want, 2), "len: array.len");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* INVOKES: direct static calls → call <funcidx> (0x10). funcidx via the
     * registry. First a no-arg call (rule + funcidx), then a call WITH args to
     * prove burgc auto-tiles the variable-arity arg children before the call. */
    /* Non-tail calls (the value is stored, not directly returned) so these
     * exercise the regular `call` form; the tail form (`return f();` →
     * return_call) is covered by test_exec. */
    const char* CALL =
        "class Call {"
        "  static int f(){ return 5; }"
        "  static int g(){ int x = f(); return x; }"
        "  static int add(int a, int b){ return a + b; }"
        "  static int use(){ int x = add(2, 3); return x; } }";
    {   /* g() calls f() — no args → call <f's funcidx> */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, CALL, "g", &s, &wt, &n);
        int cid = find_class(&s, "Call");
        CHECK(body && cid >= 0, "g compiled; found Call");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0x10); ew_u32(&w, (uint32_t)wasm_func_index(&wt, cid, 0)); /* call f (method 0) */
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "g: call f at f's funcidx");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {   /* use() calls add(2,3) — two args must be pushed before the call */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, CALL, "use", &s, &wt, &n);
        int cid = find_class(&s, "Call");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0x10); ew_u32(&w, (uint32_t)wasm_func_index(&wt, cid, 2)); /* call add (method 2) */
        CHECK(body != NULL, "use compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "use: call add at add's funcidx");
        /* the args (2 and 3) must materialize before the call: two i32.const. */
        int consts = 0; for (int i = 0; i + 1 < n; i++) if (body[i]==0x41) consts++;
        CHECK(consts >= 2, "use: both args tiled (>=2 i32.const before the call)");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* InvokeSpecial: a private instance method is a non-virtual direct call —
     * receiver (`this`) pushed, then call <funcidx>. */
    const char* SPEC =
        "class Sp { private int helper(){ return 1; }"
        "  int use(){ int x = helper(); return x; } }";   /* non-tail → regular call */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, SPEC, "use", &s, &wt, &n);
        int sp = find_class(&s, "Sp");
        CHECK(body && sp >= 0, "InvokeSpecial use compiled; found Sp");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0x10); ew_u32(&w, (uint32_t)wasm_func_index(&wt, sp, 0)); /* call helper (method 0) */
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "use: invokespecial helper → call at helper's funcidx");
        const uint8_t this_get[] = { 0x20, 0x00 };   /* this (local.get 0) pushed as receiver */
        CHECK(contains(body, n, this_get, 2), "use: receiver `this` pushed before the call");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* instanceof / cast: ref.test (FB 14, non-null) and ref.cast_null (FB 17),
     * heaptype = the target class typeidx (== class_id). */
    const char* IC =
        "class A { }"
        "class B extends A {"
        "  boolean isB(A x){ return x instanceof B; }"
        "  B cast(A x){ return (B)x; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, IC, "isB", &s, &wt, &n);
        int bid = find_class(&s, "B");
        CHECK(body && bid >= 0, "isB compiled; found B");
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x14); ew_i32(&w, wasm_types_class_typeidx(&wt, bid));
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "isB: ref.test B");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, IC, "cast", &s, &wt, &n);
        int bid = find_class(&s, "B");
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x17); ew_i32(&w, wasm_types_class_typeidx(&wt, bid));
        CHECK(body != NULL, "cast compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "cast: ref.cast_null B");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* §15.19.2 ARRAY instanceof/cast. A single-dim primitive array is precisely its PrimArray
     * overlay → ref.cast/ref.test the overlay struct. A REFERENCE array shares the RefArray
     * struct, so its element type is checked at RUNTIME (Class.isInstance, a `call`) before the
     * structural ref.cast to RefArray — never a static ref.test that couldn't tell String[]
     * from Integer[] (the JavaCard atype leftover). */
    const char* AC =
        "class T {"
        "  int[] pcast(Object x){ return (int[]) x; }"
        "  String[] rcast(Object x){ return (String[]) x; }"
        "  boolean risa(Object x){ return x instanceof String[]; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AC, "pcast", &s, &wt, &n);
        int ia = lat_primarray_class(&s, SIR_DTINT);       /* int[] IS precisely IntArray */
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x17); ew_i32(&w, wasm_types_class_typeidx(&wt, ia));
        CHECK(body != NULL && ia >= 0, "prim-array cast compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "(int[])x → ref.cast_null IntArray (precise overlay, not Object)");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AC, "rcast", &s, &wt, &n);
        int ra = lat_refarray_class(&s);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x17); ew_i32(&w, wasm_types_class_typeidx(&wt, ra));
        uint8_t call = 0x10;                                /* WOP_CALL — the isInstance reflection guard */
        CHECK(body != NULL, "ref-array cast compiled");
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "(String[])x → ref.cast_null RefArray (structural narrow)");
        CHECK(contains(body, n, &call, 1),
              "(String[])x → runtime Class.isInstance call (element-precise guard)");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, AC, "risa", &s, &wt, &n);
        uint8_t call = 0x12;   /* return_call — `return x instanceof T` tail-calls Class.isInstance */
        CHECK(body != NULL, "ref-array instanceof compiled");
        CHECK(contains(body, n, &call, 1),
              "x instanceof String[] → runtime Class.isInstance call (not a static ref.test)");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* Switch → stacked-block br_table. case 0/1 + default (contiguous, lo=0):
     * four nested blocks, br_table [0,1] default 2, case bodies, breaks → br. */
    const char* SW =
        "class Sw { void f(int x){ int r;"
        "  switch (x) { case 0: r = 10; break; case 1: r = 20; break; default: r = 30; } } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, SW, "f", &s, &wt, &n);
        CHECK(body != NULL, "switch compiled");
        const uint8_t blocks[] = { 0x02,0x40, 0x02,0x40, 0x02,0x40, 0x02,0x40 }; /* 4 nested blocks */
        const uint8_t brtbl[]  = { 0x0E, 0x02, 0x00, 0x01, 0x02 };               /* br_table [0,1] def 2 */
        const uint8_t c10[]    = { 0x41,0x0A, 0x21,0x02 };  /* case 0: r = 10 */
        const uint8_t c20[]    = { 0x41,0x14, 0x21,0x02 };  /* case 1: r = 20 */
        const uint8_t c30[]    = { 0x41,0x1E, 0x21,0x02 };  /* default: r = 30 */
        const uint8_t brk2[]   = { 0x0C, 0x02 };            /* case 0 break → br 2 */
        const uint8_t brk1[]   = { 0x0C, 0x01 };            /* case 1 break → br 1 */
        CHECK(contains(body, n, blocks, 8), "switch: four nested blocks");
        CHECK(contains(body, n, brtbl, 5), "switch: br_table [0,1] default 2");
        CHECK(contains(body, n, c10, 4) && contains(body, n, c20, 4) && contains(body, n, c30, 4),
              "switch: case bodies r=10 / r=20 / r=30");
        CHECK(contains(body, n, brk2, 2) && contains(body, n, brk1, 2),
              "switch: breaks → br 2 (case 0) and br 1 (case 1)");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* Virtual dispatch: m() is a non-private instance method, so use() calls it
     * virtually → load the receiver's vtable (struct.get V 0, an (array funcref)),
     * `i32.const slot; array.get $globalvtable` for the funcref, ref.cast it to the
     * slot's concrete func type, then call_ref that func type. */
    const char* VD =   /* non-tail (stored) → regular call_ref; tail form in test_exec */
        "class V { int m(){ return 7; } int use(){ int x = m(); return x; } }";
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, VD, "use", &s, &wt, &n);
        int v = find_class(&s, "V");
        CHECK(body && v >= 0, "virtual use compiled; found V");
        /* struct.get V 0 → the object's vtable array. */
        CHECK(has_structop(body, n, 0x02, wasm_types_class_typeidx(&wt, v), 0),
              "virtual: struct.get V field 0 (vtable header)");
        /* i32.const slot; array.get $globalvtable → the method's funcref. */
        emit_wasm_ctx sg = {0};
        ew_byte(&sg, 0x41); ew_i32(&sg, wasm_vtable_slot(&wt, v, 0));
        ew_byte(&sg, 0xFB); ew_u32(&sg, 0x0B); ew_u32(&sg, (uint32_t)wasm_vtable_typeidx(&wt, v));
        CHECK(contains(body, n, sg.code, (int)bbq_vec_len(sg.code)), "virtual: array.get vtable slot");
        /* ref.cast (0xFB 0x16) the slot's func type, then call_ref (0x14) it. */
        emit_wasm_ctx rc = {0}; ew_byte(&rc, 0xFB); ew_u32(&rc, 0x16); ew_i32(&rc, wasm_functype_idx(&wt, v, 0));
        CHECK(contains(body, n, rc.code, (int)bbq_vec_len(rc.code)), "virtual: ref.cast slot func type");
        emit_wasm_ctx cr = {0}; ew_byte(&cr, 0x14); ew_i32(&cr, wasm_functype_idx(&wt, v, 0));
        CHECK(contains(body, n, cr.code, (int)bbq_vec_len(cr.code)), "virtual: call_ref m's functype");
        bbq_vec_free(sg.code); bbq_vec_free(rc.code); bbq_vec_free(cr.code);
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* throw e → push the exception ref, then throw the single Java tag (0). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, "class T { void f(Throwable e){ throw e; } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "throw compiled");
        /* the exception is spilled to a temp, loaded, then thrown with tag 0;
         * body ends ...local.get <t>; throw 0; end (the temp slot is unstable). */
        const uint8_t thr[] = { 0x08, 0x00, 0x0B };   /* throw tag 0; end */
        CHECK(contains(body, n, thr, 3), "throw: throw the single Java tag (0)");
        int got = 0;
        for (int i = 0; i + 3 < n; i++)
            if (body[i] == 0x20 && body[i+2] == 0x08 && body[i+3] == 0x00) got = 1;
        CHECK(got, "throw: exception ref (local.get) pushed before throw");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* try/catch → try_table. block $after; block $handler; try_table (catch
     * $jexn 0) { body } end; br $after; end; handler stores the exn. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { void f(int[] a){ try { a[0] = 1; } catch (Throwable e) { } } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "try/catch compiled");
        int thr = find_class(&s, "Throwable");
        /* $after (void) then $handler whose block-type RESULT is (ref null Throwable):
         * a `catch tag $l` branches to $l carrying the tag's params, so the catch
         * target's result type is the tag's param type. */
        emit_wasm_ctx hb = {0};
        ew_byte(&hb, 0x02); ew_byte(&hb, 0x40);                  /* block $after (void)             */
        ew_byte(&hb, 0x02); wasm_types_emit_ref(&hb, wasm_types_class_typeidx(&wt, thr)); /* block $handler */
        const uint8_t trytbl[] = { 0x1F, 0x40, 0x01, 0x00, 0x00, 0x00 }; /* try_table void, catch tag0 label0 */
        const uint8_t brafter[] = { 0x0C, 0x01 };                    /* normal completion → br $after */
        CHECK(contains(body, n, hb.code, (int)bbq_vec_len(hb.code)), "try: $after + $handler (ref Throwable) blocks");
        CHECK(contains(body, n, trytbl, 6), "try: try_table with catch $jexn → $handler");
        CHECK(contains(body, n, brafter, 2), "try: normal completion brs to $after");
        bbq_vec_free(hb.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* typed catch: the structurer's dispatch tests the catch type (ref.test) then
     * branches into the handler (if), ref.casting the exn to the catch type before
     * the body. No per-handler eqz/re-throw — the no-match re-throw is the
     * catch-all body's `throw 0`. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { void f(int[] a){ try { a[0] = 1; } catch (ArithmeticException ae) { } } }", "f", &s, &wt, &n);
        int ae = find_class(&s, "ArithmeticException");
        CHECK(body && ae >= 0, "typed-catch compiled; found ArithmeticException");
        emit_wasm_ctx w = {0};
        ew_byte(&w, 0xFB); ew_u32(&w, 0x14); ew_i32(&w, wasm_types_class_typeidx(&wt, ae));   /* ref.test AE */
        ew_byte(&w, 0x04); ew_byte(&w, 0x40);                  /* if void (immediately — no eqz) */
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)),
              "typed-catch: ref.test catch type then if (no eqz)");
        emit_wasm_ctx rc = {0};
        ew_byte(&rc, 0xFB); ew_u32(&rc, 0x16); ew_i32(&rc, wasm_types_class_typeidx(&wt, ae)); /* ref.cast AE */
        CHECK(contains(body, n, rc.code, (int)bbq_vec_len(rc.code)),
              "typed-catch: ref.cast the exn to the catch type");
        const uint8_t rethrow[] = { 0x08, 0x00 };              /* throw 0 — the catch-all rethrow */
        CHECK(contains(body, n, rethrow, 2), "typed-catch: catch-all re-throws (throw 0)");
        bbq_vec_free(w.code); bbq_vec_free(rc.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* multi-catch → ONE try_table whose handler is a source-order if/else-if chain
     * (a ref.test per catch type), NOT a try_table per catch. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { void f(int[] a){ try { a[0] = 1; }"
            "  catch (ArithmeticException e) { } catch (ClassCastException c) { } } }", "f", &s, &wt, &n);
        int ae = find_class(&s, "ArithmeticException"), cc = find_class(&s, "ClassCastException");
        CHECK(body && ae >= 0 && cc >= 0, "multi-catch compiled");
        int trytables = 0;
        for (int i = 0; i + 1 < n; i++) if (body[i] == 0x1F && body[i+1] == 0x40) trytables++;
        CHECK(trytables == 1, "multi-catch: ONE try_table (dispatch is an if-chain)");
        emit_wasm_ctx ta = {0}; ew_byte(&ta,0xFB); ew_u32(&ta,0x14); ew_i32(&ta, wasm_types_class_typeidx(&wt, ae));
        emit_wasm_ctx tc = {0}; ew_byte(&tc,0xFB); ew_u32(&tc,0x14); ew_i32(&tc, wasm_types_class_typeidx(&wt, cc));
        CHECK(contains(body,n,ta.code,(int)bbq_vec_len(ta.code)) &&
              contains(body,n,tc.code,(int)bbq_vec_len(tc.code)),
              "multi-catch: ref.test for both catch types");
        bbq_vec_free(ta.code); bbq_vec_free(tc.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    /* try/finally → a catch-all (catch_class_id 0, no type test) that runs the
     * finally body and re-throws; finally also runs inline on normal exit. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { void f(int[] a){ try { a[0] = 1; } finally { a[0] = 2; } } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "try/finally compiled");
        const uint8_t tt[] = { 0x1F, 0x40 };
        CHECK(contains(body, n, tt, 2), "try/finally: try_table present");
        /* finally body a[0]=2 (i32.const 2; array.set) appears at least twice
         * (normal-exit inline + exceptional catch-all). */
        int c2 = 0; for (int i = 0; i + 1 < n; i++) if (body[i] == 0x41 && body[i+1] == 0x02) c2++;
        CHECK(c2 >= 2, "try/finally: finally body duplicated (normal + exceptional)");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* sparse switch: cases 0 and 2 (gap at 1) → padded br_table over [0..2]:
     * value 0→case0(depth0), 1→default(depth2), 2→case1(depth1), default→2. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class Sw2 { void f(int x){ int r;"
            "  switch (x) { case 0: r = 1; break; case 2: r = 2; break; default: r = 3; } } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "sparse switch compiled");
        const uint8_t brtbl[] = { 0x0E, 0x03, 0x00, 0x02, 0x01, 0x02 }; /* span 3: [0(case0),2(def),1(case1)] def 2 */
        CHECK(contains(body, n, brtbl, 6), "sparse switch: padded br_table routes the gap to default");
        sema_destroy(&s); bbq_arena_free(&a);
    }

    /* wide-element array CREATION (was NewArrayInvalid → panic before the atype
     * extension): new long[3] / new double[3] → array.new_default of the right
     * array type. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, "class T { long[] f(){ return new long[3]; } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "new long[] compiled");
        int32_t tid = wasm_types_array_for_dt(&wt, SIR_DTLONG);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x07); ew_u32(&w, (uint32_t)tid);
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "long[]: array.new_default of long[] type");
        if(!contains(body,n,w.code,(int)bbq_vec_len(w.code))){printf("    want tid=%d body:",tid);for(int i=0;i<n;i++)printf(" %02X",body[i]);printf("\n");}
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, "class T { double[] f(){ return new double[3]; } }", "f", &s, &wt, &n);
        CHECK(body != NULL, "new double[] compiled");
        int32_t tid = wasm_types_array_for_dt(&wt, SIR_DTDOUBLE);
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x07); ew_u32(&w, (uint32_t)tid);
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "double[]: array.new_default of double[] type");
        bbq_vec_free(w.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* §10.2 reference arrays → the ONE RefArray struct (String[]/Object[] are the same
     * WASM type, covariance free). The backing is the covariant top-ref array
     * (array_for_dt DTREF, an anyref array); the wrapper is a struct.new RefArray.
     * new String[3] → array.new_default of the backing + struct.new RefArray;
     * o[0] on Object[] → array.get of the backing + ref.cast to the element (Object). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { String[] mk(){ return new String[3]; } Object get(Object[] o){ return o[0]; } }", "mk", &s, &wt, &n);
        CHECK(body != NULL, "new String[] compiled");
        int32_t backing = wasm_types_array_for_dt(&wt, SIR_DTREF);
        int32_t rat     = wasm_types_class_typeidx(&wt, sema_refarray_id(&s));
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x07); ew_u32(&w, (uint32_t)backing);
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "ref array new: array.new_default of the RefArray backing");
        bbq_vec_free(w.code);
        emit_wasm_ctx w2 = {0}; ew_byte(&w2, 0xFB); ew_u32(&w2, 0x00); ew_u32(&w2, (uint32_t)rat);
        CHECK(contains(body, n, w2.code, (int)bbq_vec_len(w2.code)), "ref array new: struct.new RefArray wrapper");
        bbq_vec_free(w2.code); sema_destroy(&s); bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a,
            "class T { Object get(Object[] o){ return o[0]; } }", "get", &s, &wt, &n);
        CHECK(body != NULL, "ref array load compiled");
        int32_t backing = wasm_types_array_for_dt(&wt, SIR_DTREF);
        int32_t objt    = wasm_types_class_typeidx(&wt, sema_find_class(&s, "Object"));
        emit_wasm_ctx w = {0}; ew_byte(&w, 0xFB); ew_u32(&w, 0x0B); ew_u32(&w, (uint32_t)backing);
        CHECK(contains(body, n, w.code, (int)bbq_vec_len(w.code)), "ref array load: array.get of the RefArray backing");
        bbq_vec_free(w.code);
        emit_wasm_ctx w2 = {0}; ew_byte(&w2, 0xFB); ew_u32(&w2, 0x17); ew_i32(&w2, objt);
        CHECK(contains(body, n, w2.code, (int)bbq_vec_len(w2.code)), "ref array load: ref.cast to the element type");
        bbq_vec_free(w2.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    /* interface dispatch: i.m() through an interface-typed receiver → cast to
     * the root struct, read its vtable header, index the signature slot (shared
     * with the implementing class's override), call_ref. */
    const char* IF =
        "interface I { int m(); }"
        "class C implements I { public int m(){ return 1; } }"
        "class Use { int call(I i){ int x = i.m(); return x; } }";   /* non-tail → call_ref; tail form in test_exec */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        sema_ctx_t s; wasm_types_t wt; int n = 0;
        const uint8_t* body = emit(&a, IF, "call", &s, &wt, &n);
        int iid = find_class(&s, "I");
        CHECK(body && iid >= 0, "interface call compiled; found I");
        int32_t root = wasm_types_class_typeidx(&wt, wasm_root_class(&wt));
        emit_wasm_ctx rc = {0}; ew_byte(&rc, 0xFB); ew_u32(&rc, 0x16); ew_i32(&rc, root); /* ref.cast root */
        CHECK(contains(body, n, rc.code, (int)bbq_vec_len(rc.code)),
              "interface: ref.cast receiver to the root struct");
        emit_wasm_ctx sg = {0}; ew_byte(&sg, 0xFB); ew_u32(&sg, 0x02); ew_u32(&sg, (uint32_t)root); ew_u32(&sg, 0);
        CHECK(contains(body, n, sg.code, (int)bbq_vec_len(sg.code)),
              "interface: struct.get root field 0 (vtable header)");
        const uint8_t callref[] = { 0x14 };   /* call_ref */
        CHECK(contains(body, n, callref, 1), "interface: call_ref");
        bbq_vec_free(rc.code); bbq_vec_free(sg.code); sema_destroy(&s); bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_codegen_object");
}
