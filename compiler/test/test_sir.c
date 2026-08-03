// test_sir.c — Section 3 (AST→SIR via ddcg). Proves the pure-CPS Cmp
// redesign: a comparison is a DISTINCT node (Eq/Ne/Lt/Le/Gt/Ge), matched
// structurally — never an op-dispatched Cmp. Also the full-Java-1.0
// constant leaves (LoadLongConst/Float/Double).
#include "java_parser.h"
#include "javelina/compiler/java_source.h"   /* §3.2 step 1 — the ONE parse entry (see header) */
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/sir_support.h"
#include "javelina/compiler/sir_optimizer.h"
#include "javelina/compiler/type_lattice.h"
#include "javelina/compiler/sir_op_gamma.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define JT_REPORT_RSS   /* this suite compiles the whole prelude per case */
#include "javelina_test.h"

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); return b;
}
// Parse `src`, handing back the context that owns the resulting AST so the
// caller can decide its lifetime. The AST is arena-backed by that context;
// idents and literals are jdup'd into it, so `src` itself need not survive.
static ast_program_t* parse_into(const char* src, java_parse_ctx_t** ctx_out) {
    java_parse_ctx_t* pc = (java_parse_ctx_t*)malloc(sizeof(*pc));
    bbq_arena_init(&pc->arena, 1 << 16); pc->result = NULL; pc->file = NULL;
    peg_state p; char* tsrc = NULL; const char* terr = NULL;
    if (!java_source_init(&p, src, (int)strlen(src), &tsrc, &terr)) return NULL;
    p.user_data = pc;
    ast_program_t* prog = java_parser_parse(&p) ? pc->result : NULL;
    free(tsrc);
    if (ctx_out) *ctx_out = pc;
    return prog;
}
// java.lang stubs + user source, merged into one program.
// Shared compile session — ONE arena reused across every full-prelude compile in
// this file. Reset before each compile so the previous compilation (2.2 MB of
// java.lang source, ~454 methods, plus its engines) is reclaimed instead of leaked.
// A returned arena-backed pointer (sir_node_t*, method array, engine) is valid until
// the NEXT sess_arena() call. Invariant, verified against the suite: no test holds a
// result from one compile across the next compile. The process keeps this single
// arena to exit (the OS reclaims it) — bounded, not the O(compiles) leak it replaces.
static bbq_arena* sess_arena(void) {
    static bbq_arena a; static bool live = false;
    if (!live) { bbq_arena_init(&a, 1 << 18); live = true; } else bbq_arena_reset(&a);
    return &a;
}

// The java.lang prelude — 110 files, 2.2 MB of source, the generated
// CharacterData.java alone being 2 MB — is identical for every compile here, so
// it is parsed ONCE and shared.
//
// Sharing it takes one extra step, because sema DESUGARS into the AST it is
// given: JLS §8.8.7 prepends the implicit super() into every constructor body
// that lacks an explicit one, allocating the statement and a replacement
// statement array from ctx->arena and writing them back into the node. The
// prepend is guarded — it re-reads stmts[0] and skips when a constructor call
// is already there — so a second compile over the same AST does not duplicate
// it. What it DOES do is dereference stmts[0], and if the first compile wrote
// that pointer out of a per-test arena which has since been reset, the read is
// of freed memory.
//
// So the prelude is desugared ONCE against storage that is never released, and
// every later compile finds the rewrite already present and valid. The prelude
// arena is process-lifetime by design; the OS reclaims it.
static ast_type_decl_t** g_lib_types  = NULL;
static int               g_lib_ntypes = -1;      /* <0 until the one-time build runs */
static ast_program_t**   g_lib_units  = NULL;    /* per-FILE programs — §7.3 units */
static int               g_lib_nunits = 0;
static bbq_arena         g_prelude_arena;        /* owns the shared desugar; never freed */

static void prelude_once(void) {
    if (g_lib_ntypes >= 0) return;
    /* The full prelude (lang/util/io/simd): java.lang's own §7.5 imports name
     * java.io types, so a lang-only environment is no longer legal Java. Each
     * FILE stays its own §7.3 unit (its package + imports drive resolution). */
    static const char* dirs[] = { "lib/java/lang", "lib/java/util", "lib/java/io",
                                  "lib/javelina/simd" };
    ast_type_decl_t** t = NULL; int tc = 0, cap = 0;
    for (int di = 0; di < 4; di++) {
        DIR* d = opendir(dirs[di]);
        if (!d) { printf("  FAIL  cannot open %s\n", dirs[di]); TEST_FAILED(); continue; }
        struct dirent* e;
        while ((e = readdir(d))) { size_t L=strlen(e->d_name);
            if (L<6 || strcmp(e->d_name+L-5,".java")) continue;
            char path[512]; snprintf(path,sizeof path,"%s/%s",dirs[di],e->d_name);
            char* s = read_file(path); if(!s) continue;
            ast_program_t* p = parse_into(s, NULL);  /* context kept for the run */
            free(s);                       /* idents/literals are jdup'd into pc->arena */
            if(!p){printf("  FAIL parse %s\n",path);TEST_FAILED();continue;}
            { static int ucap = 0;
              if (g_lib_nunits==ucap){ucap=ucap?ucap*2:64;g_lib_units=realloc(g_lib_units,(size_t)ucap*sizeof(*g_lib_units));}
              g_lib_units[g_lib_nunits++]=p; }
            for (int i=0;i<p->types_count;i++) {
                if(tc==cap){cap=cap?cap*2:64;t=realloc(t,(size_t)cap*sizeof(*t));}
                t[tc++]=p->types[i];
            }
        } closedir(d);
    }
    g_lib_types = t; g_lib_ntypes = tc;

    /* Warm the desugar into permanent storage. sema_destroy releases this pass's
     * hash tables; the AST rewrite it made stays in g_prelude_arena. */
    bbq_arena_init(&g_prelude_arena, 1 << 20);
    sema_ctx_t warm; sema_init(&warm, &g_prelude_arena);
    warm.num_library_classes = tc;
    sema_analyze_units(&warm, g_lib_units, g_lib_nunits);
    sema_destroy(&warm);
}

// The user source's parse context, released at the START of the next build —
// the same "valid until the next call" lifetime the session arena documents.
static java_parse_ctx_t* g_user_ctx = NULL;

// The current §7.3 unit list (lib units + this build's user unit) — what
// sir_analyze feeds sema_analyze_units. Rebuilt by every build_program call.
static ast_program_t** g_units = NULL;
static int             g_nunits = 0;

static ast_program_t* build_program(const char* user_src, bbq_arena* arena, int* nlib_out) {
    prelude_once();
    if (g_user_ctx) { bbq_arena_free(&g_user_ctx->arena); free(g_user_ctx); g_user_ctx = NULL; }

    int nlib = g_lib_ntypes;
    if (nlib_out) *nlib_out = nlib;               /* prelude classes occupy [0, nlib) */
    ast_program_t* up = parse_into(user_src, &g_user_ctx);
    if (!up) { printf("  FAIL  parse user source\n"); TEST_FAILED(); }

    free(g_units);
    g_nunits = g_lib_nunits + (up ? 1 : 0);
    g_units = (ast_program_t**)malloc((size_t)g_nunits * sizeof(*g_units));
    memcpy(g_units, g_lib_units, (size_t)g_lib_nunits * sizeof(*g_units));
    if (up) g_units[g_lib_nunits] = up;

    int nuser = up ? up->types_count : 0;
    int tc = nlib + nuser;
    ast_type_decl_t** arr = bbq_arena_alloc(arena,(size_t)tc*sizeof(*arr));
    memcpy(arr, g_lib_types, (size_t)nlib*sizeof(*arr));
    for (int i = 0; i < nuser; i++) arr[nlib+i] = up->types[i];
    return ast_program(arena, NULL, NULL, 0, arr, tc);
}

// The sema entry for this suite: the §7.3-correct unit list from the last
// build_program. (The flat program still feeds compiler_compile.)
static bool sir_analyze(sema_ctx_t* c) { return sema_analyze_units(c, g_units, g_nunits); }

// The sidecar is ONE table of all kinds (compiler.h's PAYLOAD TABLE). A pin about
// §15 guards filters to the GUARD rows: key = the guard's Branch (an ExprEffect for
// ARRAY_STORE), a = the guard kind, b = subject slot, c = aux slot, d = throw-on-true.
// The engine reads the same rows — there is no test-only path into its structure.
#define MAXFACTS 512
static compiler_fact_t guard_rows[MAXFACTS];

static const compiler_fact_t* guards_of(const compiler_ctx_t* c, int mi, int* n) {
    int nf = 0;
    const compiler_fact_t* f = compiler_get_facts(c, mi, &nf);
    int k = 0;
    for (int i = 0; i < nf && k < MAXFACTS; i++)
        if (f[i].kind == COMPILER_FACT_GUARD) guard_rows[k++] = f[i];
    *n = k;
    return guard_rows;
}

// Collect every EXCEPTING node in the graph — the JLS §11.1 set: throws, calls,
// allocations (and ClassConstruct, a call_ref). The §31 completeness pin walks with
// this: tests may walk; the optimizer may not.
static void collect_excepting(const sir_node_t* n, const sir_node_t*** seen,
                              const sir_node_t*** out) {
    if (!n) return;
    for (int i = 0; i < (int)bbq_vec_len(*seen); i++)
        if ((*seen)[i] == n) return;
    bbq_vec_push(*seen, n);
    switch (n->tag) {
        case SIR_THROW:
        case SIR_NEW: case SIR_NEWARRAY: case SIR_NEWREFARRAY:
        case SIR_CLASSCONSTRUCT:
        case SIR_INVOKEVIRTUAL: case SIR_INVOKESPECIAL:
        case SIR_INVOKESTATIC:  case SIR_INVOKEINTERFACE:
            bbq_vec_push(*out, n);
            break;
        default: break;
    }
    for (int i = 0; i < sir_arity((sir_node_t*)n); i++)
        collect_excepting(sir_child((sir_node_t*)n, i), seen, out);
    for (int i = 0; i < sir_succ_count(n); i++)
        collect_excepting(sir_succ(n, i), seen, out);
}

// EXCEPT_REGION rows for one node: how many, and (via *a0) one region id.
static int except_rows_of(const compiler_fact_t* f, int nf,
                          const sir_node_t* key, int* a0) {
    int c = 0;
    for (int i = 0; i < nf; i++)
        if (f[i].kind == COMPILER_FACT_EXCEPT_REGION && f[i].key == key) {
            if (c == 0 && a0) *a0 = f[i].a;
            c++;
        }
    return c;
}

// Walk the FINAL graph exactly as codegen does — from the entry, through tree
// children AND spine successors — recording which slots are written (StoreLocal /
// Inc / ExceptionEntry, the three definers) and which are read (any LoadLocal).
// This is the reader the post-Click SSA invariant (§29) is checked with: codegen
// emits what it finds here, so this is the set that matters, not the optimizer's
// own view of what it thinks is reachable.
static void collect_slot_defs_uses(const sir_node_t* n, bool* def, bool* use,
                                   int sc, const sir_node_t*** seen) {
    if (!n) return;
    for (int i = 0; i < (int)bbq_vec_len(*seen); i++)   /* a merge is reached twice */
        if ((*seen)[i] == n) return;
    bbq_vec_push(*seen, n);

    if (n->tag == SIR_STORELOCAL) {
        int s = n->store_local.slot;
        if (s >= 0 && s < sc) def[s] = true;
    } else if (n->tag == SIR_INC) {
        int s = n->inc.slot;
        if (s >= 0 && s < sc) def[s] = true;
    } else if (n->tag == SIR_EXCEPTIONENTRY) {
        int s = n->exception_entry.local_slot;
        if (s >= 0 && s < sc) def[s] = true;
    } else if (n->tag == SIR_LOADLOCAL) {
        int s = n->load_local.slot;
        if (s >= 0 && s < sc) use[s] = true;
    }
    for (int i = 0; i < sir_arity((sir_node_t*)n); i++)
        collect_slot_defs_uses(sir_child((sir_node_t*)n, i), def, use, sc, seen);
    for (int i = 0; i < sir_succ_count((sir_node_t*)n); i++)
        collect_slot_defs_uses(sir_succ((sir_node_t*)n, i), def, use, sc, seen);
}

// Bounded DFS over tree children + spine successors (depth bound handles
// loop back-edges). Returns the first node with the given tag.
static const sir_node_t* find_tag(const sir_node_t* n, int tag, int depth) {
    if (!n || depth <= 0) return NULL;
    if ((int)n->tag == tag) return n;
    for (int i = 0; i < sir_arity(n); i++) {
        const sir_node_t* r = find_tag(sir_child(n, i), tag, depth - 1);
        if (r) return r;
    }
    for (int i = 0; i < sir_succ_count(n); i++) {
        const sir_node_t* r = find_tag(sir_succ(n, i), tag, depth - 1);
        if (r) return r;
    }
    return NULL;
}

/* Count DISTINCT reachable nodes of `tag` (a value node shared by two parents
 * counts once — the visited set handles the post-Click DAG). Tests may walk;
 * the optimizer may not. */
static int count_tag_r(const sir_node_t* n, int tag, const sir_node_t** seen,
                       int* nseen, int depth) {
    if (!n || depth <= 0) return 0;
    for (int i = 0; i < *nseen; i++) if (seen[i] == n) return 0;
    if (*nseen < 512) seen[(*nseen)++] = n;
    int c = ((int)n->tag == tag) ? 1 : 0;
    for (int i = 0; i < sir_arity(n); i++)
        c += count_tag_r(sir_child(n, i), tag, seen, nseen, depth - 1);
    for (int i = 0; i < sir_succ_count(n); i++)
        c += count_tag_r(sir_succ(n, i), tag, seen, nseen, depth - 1);
    return c;
}

/* Compile, optionally Click, then count `tag` in the named USER method —
 * the guard-merge pins. -1 = method not found. */
static int compile_count_in(const char* user_src, const char* mname, int tag, int opt) {
    bbq_arena* arena = sess_arena();
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
    compiler_ctx_t cctx; compiler_init(&cctx, arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, mname)) continue;
        if (opt) sir_optimize(&cctx, i);
        const sir_node_t* seen[512]; int nseen = 0;
        return count_tag_r(methods[i]->entry, tag, seen, &nseen, 800);
    }
    return -1;
}

// Compile user source (merged with java.lang) to SIR; return the first node of
// `tag` across the USER classes' methods, or NULL. The search is scoped to the
// user source (class_id >= nlib): the bundled java.lang now has REAL compiled
// method bodies (String.equals' loop, etc.), so a prelude-wide search would find
// their control-flow/comparison nodes, not the test program's.
static const sir_node_t* compile_find(const char* user_src, int tag, int opt) {
    bbq_arena* arena = sess_arena();
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
    compiler_ctx_t cctx; compiler_init(&cctx, arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;          // skip java.lang prelude
        if (opt) sir_optimize(&cctx, i);                    // run Click
        const sir_node_t* r = find_tag(methods[i]->entry, tag, 400);
        if (r) return r;
    }
    return NULL;
}

// Compile user source through the full pipeline, then bin-pack the named
// user method's slots against the REAL sema (the path sir_optimize takes in
// the driver). Returns the packed max_locals, or -1 if the method is absent.
static int packed_max_locals(const char* user_src, const char* mname) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, &arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
    compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, mname)) continue;
        cp_pack(methods[i], &sctx, &arena, methods[i]->max_locals);
        int packed = methods[i]->max_locals;
        sema_destroy(&sctx); bbq_arena_free(&arena);          /* the result is a scalar; nothing escapes */
        return packed;
    }
    sema_destroy(&sctx); bbq_arena_free(&arena);
    return -1;
}

/* Walk the spine breadth-first; true if some reachable Branch still tests
 * the rem: either a REM directly in its cond tree, or the branch reads a
 * slot whose value a REM-bearing StoreLocal just defined (the optimizer's
 * spilled form: `s = Rem(...); if (s == 0)`). */
static bool any_branch_tests_rem(const sir_node_t* entry) {
    const sir_node_t* seen[512]; int ns = 0;
    const sir_node_t* work[512]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn) {
        const sir_node_t* n = work[wi++];
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup || ns >= 512) continue;
        seen[ns++] = n;
        if (n->tag == SIR_BRANCH &&
            find_tag(n->branch.cond, SIR_REM, 64)) return true;
        if (n->tag == SIR_STORELOCAL &&
            find_tag(n->store_local.value, SIR_REM, 64)) {
            const sir_node_t* nx = sir_get_next((sir_node_t*)n);
            if (nx && nx->tag == SIR_BRANCH && nx->branch.cond &&
                nx->branch.cond->tag != SIR_LOADCONST &&
                find_tag(nx->branch.cond, SIR_LOADLOCAL, 8)) {
                const sir_node_t* ll = find_tag(nx->branch.cond, SIR_LOADLOCAL, 8);
                if (ll->load_local.slot == n->store_local.slot) return true;
            }
        }
        for (int k = 0; k < sir_succ_count(n) && wn < 512; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
    return false;
}

/* §4.12.5 oracle: the (single) Return's value is a LoadLocal of a slot whose def is
 * `StoreLocal(slot, LoadConst 0)` — the default-init the scalar rewrite emits. */
static bool retslot_defaults_to_zero(const sir_node_t* entry) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    work[wn++] = entry;
    int ret_slot = -1;
    bool zeroed[256] = { false };
    while (wi < wn) {
        const sir_node_t* x = work[wi++];
        if (!x) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == x) { dup = true; break; }
        if (dup || ns >= 1024) continue;
        seen[ns++] = x;
        if (x->tag == SIR_RETURN) {
            const sir_node_t* v = x->return_.value;
            if (!v || v->tag != SIR_LOADLOCAL) return false;
            ret_slot = v->load_local.slot;
        }
        if (x->tag == SIR_STORELOCAL && x->store_local.slot >= 0
                && x->store_local.slot < 256 && x->store_local.value
                && x->store_local.value->tag == SIR_LOADCONST
                && x->store_local.value->load_const.value == 0)
            zeroed[x->store_local.slot] = true;
        for (int i = 0; i < sir_arity((sir_node_t*)x) && wn < 1024; i++)
            work[wn++] = sir_child((sir_node_t*)x, i);
        for (int m = 0; m < sir_succ_count((sir_node_t*)x) && wn < 1024; m++)
            work[wn++] = sir_succ((sir_node_t*)x, m);
    }
    return ret_slot >= 0 && ret_slot < 256 && zeroed[ret_slot];
}

/* §11.3.1 oracle: the slot some StoreLocal writes the constant `k` into IS the slot a
 * Return's LoadLocal reads — i.e. the try's field write and the catch's field read were
 * rewritten onto the SAME local. */
static bool try_write_reaches_catch_read(const sir_node_t* entry, int32_t k) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    work[wn++] = entry;
    int k_slot = -1, ret_slot = -2;
    while (wi < wn) {
        const sir_node_t* x = work[wi++];
        if (!x) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == x) { dup = true; break; }
        if (dup || ns >= 1024) continue;
        seen[ns++] = x;
        if (x->tag == SIR_STORELOCAL && x->store_local.value
                && x->store_local.value->tag == SIR_LOADCONST
                && x->store_local.value->load_const.value == k)
            k_slot = x->store_local.slot;
        if (x->tag == SIR_RETURN && x->return_.value
                && x->return_.value->tag == SIR_LOADLOCAL)
            ret_slot = x->return_.value->load_local.slot;
        for (int i = 0; i < sir_arity((sir_node_t*)x) && wn < 1024; i++)
            work[wn++] = sir_child((sir_node_t*)x, i);
        for (int m = 0; m < sir_succ_count((sir_node_t*)x) && wn < 1024; m++)
            work[wn++] = sir_succ((sir_node_t*)x, m);
    }
    return k_slot >= 0 && k_slot == ret_slot;
}

/* Is any slot READ that nothing WRITES? JLS §16: a local may be read only where it is
 * definitely assigned, so a rewrite that deletes a slot's DEF while leaving a READ of it
 * has produced code that loads an unwritten local — and that is INVISIBLE to a "the New is
 * gone" assertion, which is why §32.8 exists. (For methods with no parameters, which is
 * every hand-built site here, every slot must be written before it is read.) */
static bool reads_a_defless_slot(const sir_node_t* entry) {
    bool wrote[256]; bool readb[256];
    memset(wrote, 0, sizeof wrote); memset(readb, 0, sizeof readb);
    const sir_node_t* seen[2048]; int ns = 0;
    const sir_node_t* work[2048]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn) {
        const sir_node_t* x = work[wi++];
        if (!x) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == x) { dup = true; break; }
        if (dup || ns >= 2048) continue;
        seen[ns++] = x;
        int s = -1;
        if (x->tag == SIR_STORELOCAL)          s = x->store_local.slot;
        else if (x->tag == SIR_INC)            s = x->inc.slot;
        else if (x->tag == SIR_EXCEPTIONENTRY) s = x->exception_entry.local_slot;
        if (s >= 0 && s < 256) wrote[s] = true;
        if (x->tag == SIR_LOADLOCAL && x->load_local.slot >= 0
                                    && x->load_local.slot < 256)
            readb[x->load_local.slot] = true;
        for (int i = 0; i < sir_arity((sir_node_t*)x) && wn < 2048; i++)
            work[wn++] = sir_child((sir_node_t*)x, i);
        for (int k = 0; k < sir_succ_count((sir_node_t*)x) && wn < 2048; k++)
            work[wn++] = sir_succ((sir_node_t*)x, k);
    }
    for (int s = 0; s < 256; s++) if (readb[s] && !wrote[s]) return true;
    return false;
}

/* Count nodes of `tag` reachable from the spine (values + successors). */
static int count_tag(const sir_node_t* entry, int tag) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    work[wn++] = entry;
    int n = 0;
    while (wi < wn) {
        const sir_node_t* x = work[wi++];
        if (!x) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == x) { dup = true; break; }
        if (dup || ns >= 1024) continue;
        seen[ns++] = x;
        if ((int)x->tag == tag) n++;
        for (int i = 0; i < sir_arity((sir_node_t*)x) && wn < 1024; i++)
            work[wn++] = sir_child((sir_node_t*)x, i);
        for (int k = 0; k < sir_succ_count((sir_node_t*)x) && wn < 1024; k++)
            work[wn++] = sir_succ((sir_node_t*)x, k);
    }
    return n;
}

/* count_tag, restricted to field ops ON one class — a partial-escape
 * materialization chain legitimately contains PutFields of the ESCAPING class
 * (the real-object writes at the escape point), so "the virtual class's ops are
 * gone" must not count those. */
static int count_field_ops_of_class(const sir_node_t* entry, int class_id) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    work[wn++] = entry;
    int n = 0;
    while (wi < wn) {
        const sir_node_t* x = work[wi++];
        if (!x) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == x) { dup = true; break; }
        if (dup || ns >= 1024) continue;
        seen[ns++] = x;
        if (x->tag == SIR_GETFIELD && x->get_field.class_id == class_id) n++;
        if (x->tag == SIR_PUTFIELD && x->put_field.class_id == class_id) n++;
        for (int i = 0; i < sir_arity((sir_node_t*)x) && wn < 1024; i++)
            work[wn++] = sir_child((sir_node_t*)x, i);
        for (int k = 0; k < sir_succ_count((sir_node_t*)x) && wn < 1024; k++)
            work[wn++] = sir_succ((sir_node_t*)x, k);
    }
    return n;
}

/* Compile user source, optimize the named user method, count `tag` in it. */
static int opt_method_tag_count(const char* user_src, const char* mname, int tag) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, &arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
    compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, mname)) continue;
        sir_optimize(&cctx, i);
        int n = count_tag(methods[i]->entry, tag);
        sema_destroy(&sctx); bbq_arena_free(&arena);          /* the result is a scalar; nothing escapes */
        return n;
    }
    sema_destroy(&sctx); bbq_arena_free(&arena);
    return -1;
}

/* Compile user source, optimize the named method, return whether a
 * Branch-with-Rem survives (the tokenizer loop-condition shape). */
static bool opt_method_keeps_rem_branch(const char* user_src, const char* mname) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, &arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
    compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, mname)) continue;
        sir_optimize(&cctx, i);
        bool r = any_branch_tests_rem(methods[i]->entry);
        sema_destroy(&sctx); bbq_arena_free(&arena);          /* the result is a scalar; nothing escapes */
        return r;
    }
    sema_destroy(&sctx); bbq_arena_free(&arena);
    return false;
}

/* Debugging aid (env-gated, not a test): SIR_DUMP=1 dumps the optimized
 * spine of `skip` from SIR_DUMP_SRC (numeric tags — load/store/branch
 * shape); SIR_DUMP_RAW=1 skips the optimizer for the unlowered shape. */
static int dbg_id(const sir_node_t** tab, int* n, const sir_node_t* x) {
    for (int i = 0; i < *n; i++) if (tab[i] == x) return i;
    tab[(*n)++] = x; return *n - 1;
}
static void dbg_tree(const sir_node_t* e, int depth) {
    if (!e) { printf("()"); return; }
    printf("(%s", sir_op_gamma[e->tag].mnemonic);
    if (e->tag == SIR_LOADLOCAL) printf(" s%d", e->load_local.slot);
    if (e->tag == SIR_LOADCONST) printf(" =%d", e->load_const.value);
    if (e->tag == SIR_GETFIELD)  printf(" f%d", e->get_field.field_idx);
    if (depth < 8) {
        for (int i = 0; i < sir_arity(e); i++) {
            printf(" ");
            dbg_tree(sir_child((sir_node_t*)e, i), depth + 1);
        }
    }
    printf(")");
}
/* Is any spine node reachable from `entry` of this tag? Walks successors only —
 * no dominance, no ordering, just "does the graph still contain one". */
static bool spine_has_tag(const sir_node_t* entry, int tag) {
    const sir_node_t* seen[512]; int ns = 0;
    const sir_node_t* work[512]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn && ns < 500) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        if ((int)n->tag == tag) return true;
        for (int k = 0; k < sir_succ_count((sir_node_t*)n) && wn < 500; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
    return false;
}

/* The value node the engine built for expression `expr`. */
static cp_vnode_t* vnode_for(cp_engine_t* e, const sir_node_t* expr) {
    for (int i = 0; i < e->vnode_count; i++)
        if (e->vnodes[i]->expr == expr) return e->vnodes[i];
    return NULL;
}

static int vnode_idx_for(cp_engine_t* e, const sir_node_t* expr) {
    for (int i = 0; i < e->vnode_count; i++)
        if (e->vnodes[i]->kind == CP_VN_EXPR && e->vnodes[i]->expr == expr) return i;
    return -1;
}

/* Test-side mirror of the engine's value-leader walk (copies via LoadLocal
 * inputs, then Followers, then Refine inputs), so a pin can state WHERE two
 * values' resolutions land. An oracle for the composition under test, not a
 * reimplementation inside it. */
static int leader_walk(cp_engine_t* e, int vi) {
    for (int hops = 0; hops < 256; hops++) {
        for (int u = 0; u < 128; u++) {
            if (vi < 0 || vi >= e->vnode_count) return vi;
            cp_vnode_t* v = e->vnodes[vi];
            if (v->kind != CP_VN_EXPR || !v->expr
                    || v->expr->tag != SIR_LOADLOCAL || v->input_count != 1) break;
            if (v->inputs[0] == vi) break;
            vi = v->inputs[0];
        }
        if (vi < 0 || vi >= e->vnode_count) return vi;
        cp_vnode_t* v = e->vnodes[vi];
        int next = vi;
        if (v->leader >= 0) next = v->leader;
        else if (v->kind == CP_VN_REFINE && v->input_count >= 1 && v->inputs[0] >= 0)
            next = v->inputs[0];
        if (next == vi) return vi;
        vi = next;
    }
    return vi;
}


/* The first two nodes of `tag` reachable from `entry`, in walk order (spine node or
 * anywhere in its expression trees). */
static void collect_two_expr(const sir_node_t* e, int tag,
                             const sir_node_t** a, const sir_node_t** b) {
    if (!e || *b) return;
    if ((int)e->tag == tag) { if (!*a) *a = e; else if (!*b) *b = e; }
    for (int i = 0; i < sir_arity((sir_node_t*)e); i++)
        collect_two_expr(sir_child((sir_node_t*)e, i), tag, a, b);
}

/* The first `New` of a GIVEN CLASS. Walk order is NOT enough to find the allocation under
 * test: the §15 guards allocate too (`neg_size_guard` / `null_guard` throw `new
 * NegativeArraySizeException()` / `new NullPointerException()` via `throw_new_noarg`), and the
 * Branch puts that throw arm FIRST — so "the first New" is usually the guard's exception
 * object, not the object the source asked for. Selecting by class is the only stable handle. */
static const sir_node_t* find_new_of_class_expr(const sir_node_t* e, int class_id) {
    if (!e) return NULL;
    if (e->tag == SIR_NEW && e->new_.class_id == class_id) return e;
    for (int i = 0; i < sir_arity((sir_node_t*)e); i++) {
        const sir_node_t* r = find_new_of_class_expr(sir_child((sir_node_t*)e, i), class_id);
        if (r) return r;
    }
    return NULL;
}

static const sir_node_t* find_new_of_class(const sir_node_t* entry, int class_id) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn && ns < 1000) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        int sc = sir_succ_count((sir_node_t*)n);
        for (int i = 0; i < sir_arity((sir_node_t*)n); i++) {
            const sir_node_t* c = sir_child((sir_node_t*)n, i);
            bool is_succ = false;
            for (int k = 0; k < sc; k++)
                if (sir_succ((sir_node_t*)n, k) == c) { is_succ = true; break; }
            if (is_succ) continue;
            const sir_node_t* r = find_new_of_class_expr(c, class_id);
            if (r) return r;
        }
        for (int k = 0; k < sc && wn < 1000; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
    return NULL;
}

static void collect_two(const sir_node_t* entry, int tag,
                        const sir_node_t** a, const sir_node_t** b) {
    const sir_node_t* seen[1024]; int ns = 0;
    const sir_node_t* work[1024]; int wn = 0, wi = 0;
    *a = NULL; *b = NULL;
    work[wn++] = entry;
    while (wi < wn && ns < 1000 && !*b) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        int sc = sir_succ_count((sir_node_t*)n);
        for (int i = 0; i < sir_arity((sir_node_t*)n); i++) {
            const sir_node_t* c = sir_child((sir_node_t*)n, i);
            bool is_succ = false;
            for (int k = 0; k < sc; k++)
                if (sir_succ((sir_node_t*)n, k) == c) { is_succ = true; break; }
            if (!is_succ) collect_two_expr(c, tag, a, b);
        }
        for (int k = 0; k < sc && wn < 1000; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
}

/* The compiler lowers `(A) x` (JLS §5.5 + §15.16) as a GUARDED cast:
 *
 *      if (x == null)          -> checkcast x      // null passes any cast
 *      else if (x instanceof A)-> checkcast x      // the ok arm
 *      else                    -> throw new ClassCastException
 *
 * so ONE source-level cast produces TWO CheckCast nodes and a Branch on `instanceof`.
 * Which one a test means is a structural question — walk order answers it by accident
 * (the first CheckCast reachable is the NULL-path one, whose pts is {⊥null}, and a pin
 * that lands there is vacuous). These pick by shape. */
static const sir_node_t* first_spine_expr(const sir_node_t* from, int tag) {
    const sir_node_t *a = NULL, *b = NULL;
    collect_two(from, tag, &a, &b);
    return a;
}
/* The first SPINE node of `tag` reachable from `from` (collect_two searches expression
 * trees only, so it can never see a Return). */
/* §47p helpers: count SIR_NEW in expression trees along the SINGLE-successor spine
 * prefix (up to the first surviving Branch), and across a whole method graph. */
static int count_new_expr(const sir_node_t* e, int depth) {
    if (!e || depth <= 0) return 0;
    int c = (e->tag == SIR_NEW) ? 1 : 0;
    for (int i = 0; i < sir_arity(e); i++) c += count_new_expr(sir_child(e, i), depth - 1);
    return c;
}
static int count_new_prefix(const sir_node_t* n, const sir_node_t** out_branch) {
    int c = 0;
    if (out_branch) *out_branch = NULL;
    for (int guard = 0; n && guard < 512; guard++) {
        if (n->tag == SIR_BRANCH) { if (out_branch) *out_branch = n; break; }
        for (int i = 0; i < sir_arity(n); i++) c += count_new_expr(sir_child(n, i), 64);
        if (sir_succ_count(n) != 1) break;
        n = sir_succ(n, 0);
    }
    return c;
}
static int count_new_graph(const sir_node_t* from) {
    const sir_node_t* seen[512]; int ns = 0;
    const sir_node_t* work[512]; int wn = 0, wi = 0;
    int c = 0;
    work[wn++] = from;
    while (wi < wn && ns < 500) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        for (int i = 0; i < sir_arity(n); i++) c += count_new_expr(sir_child(n, i), 64);
        for (int i = 0; i < sir_succ_count(n) && wn < 510; i++)
            work[wn++] = (const sir_node_t*)sir_succ((sir_node_t*)n, i);
    }
    return c;
}

static const sir_node_t* first_spine_node(const sir_node_t* from, int tag) {
    const sir_node_t* seen[512]; int ns = 0;
    const sir_node_t* work[512]; int wn = 0, wi = 0;
    work[wn++] = from;
    while (wi < wn && ns < 500) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        if ((int)n->tag == tag) return n;
        for (int k = 0; k < sir_succ_count((sir_node_t*)n) && wn < 500; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
    return NULL;
}
/* The Branch whose condition has `tag` (SIR_INSTANCEOF, or an EQ against null). */
static const sir_node_t* branch_on(const sir_node_t* entry, int cond_tag, bool null_cmp) {
    const sir_node_t* seen[512]; int ns = 0;
    const sir_node_t* work[512]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn && ns < 500) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = n;
        if (n->tag == SIR_BRANCH && n->branch.cond) {
            const sir_node_t* c = n->branch.cond;
            if (!null_cmp && (int)c->tag == cond_tag) return n;
            if (null_cmp && c->tag == SIR_EQ
                && (sir_child((sir_node_t*)c, 0)->tag == SIR_LOADNULL
                 || sir_child((sir_node_t*)c, 1)->tag == SIR_LOADNULL)) return n;
        }
        for (int k = 0; k < sir_succ_count((sir_node_t*)n) && wn < 500; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
    }
    return NULL;
}

static void dbg_dump_method(const char* user_src, const char* mname) {
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    int nlib = 0;
    ast_program_t* prog = build_program(user_src, &arena, &nlib);
    sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
    if (!sir_analyze(&sctx)) printf("  (sema errors)\n");
    compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++) {
        if (methods[i]->class_id < nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, mname)) continue;
        if (!getenv("SIR_DUMP_RAW")) sir_optimize(&cctx, i);
        const sir_node_t* tab[512]; int nid = 0;
        const sir_node_t* work[512]; int wn = 0, wi = 0;
        work[wn++] = methods[i]->entry;
        dbg_id(tab, &nid, methods[i]->entry);
        printf("== %s optimized spine ==\n", mname);
        while (wi < wn && wn < 500) {
            const sir_node_t* n = work[wi++];
            printf("#%d %s", dbg_id(tab, &nid, n), sir_op_gamma[n->tag].mnemonic);
            if (n->tag == SIR_STORELOCAL) {
                printf(" slot=%d val=", n->store_local.slot);
                dbg_tree(n->store_local.value, 0);
            } else if (n->tag == SIR_PUTFIELD) {
                printf(" f%d val=", n->put_field.field_idx);
                dbg_tree(n->put_field.value, 0);
            } else if (n->tag == SIR_BRANCH) {
                printf(" cond=");
                dbg_tree(n->branch.cond, 0);
            } else if (n->tag == SIR_RETURN) {
                printf(" val=");
                dbg_tree(n->return_.value, 0);
            } else if (n->tag == SIR_INC) {
                printf(" slot=%d", n->inc.slot);
            }
            printf(" ->");
            for (int k = 0; k < sir_succ_count(n); k++) {
                const sir_node_t* s = sir_succ((sir_node_t*)n, k);
                int before = nid;
                int id = dbg_id(tab, &nid, s);
                printf(" #%d", id);
                if (nid > before && wn < 500) work[wn++] = s;
            }
            printf("\n");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
        return;
    }
    printf("  (method %s not found)\n", mname);
    sema_destroy(&sctx); bbq_arena_free(&arena);
}

/* Does the EXPRESSION TREE under `e` contain `tag`? Children only — never
 * successors — so a prefix walk stays a prefix walk. */
static bool expr_has_tag(const sir_node_t* e, int tag, int depth) {
    if (!e || depth <= 0) return false;
    if ((int)e->tag == tag) return true;
    for (int i = 0; i < sir_arity((sir_node_t*)e); i++)
        if (expr_has_tag(sir_child((sir_node_t*)e, i), tag, depth - 1)) return true;
    return false;
}

int main(void) {
    // 1. A comparison in VALUE context is a distinct Lt node (not a Cmp).
    const sir_node_t* lt = compile_find(
        "class T { boolean f(int x, int y) { return x < y; } }", SIR_LT, 0);
    CHECK(lt != NULL, "x < y lowers to a distinct SIR_LT node");
    CHECK(lt && sir_arity(lt) == 2, "SIR_LT has two children (left,right)");

    // 2. A comparison in TEST context is the Branch's condition child —
    //    Branch(Lt(...)), which burg can match structurally (the whole point).
    const sir_node_t* br = compile_find(
        "class T { void f(int x, int y) { if (x < y) { return; } } }", SIR_BRANCH, 0);
    CHECK(br != NULL, "if (x<y) produces a SIR_BRANCH");
    CHECK(br && sir_child(br, 0) && (int)sir_child(br, 0)->tag == SIR_LT,
          "Branch condition child is SIR_LT (pure CPS, structurally matchable)");

    // 3. Distinct nodes for the other operators.
    CHECK(compile_find("class T { boolean f(int a, int b) { return a == b; } }", SIR_EQ, 0),
          "== lowers to SIR_EQ");
    CHECK(compile_find("class T { boolean f(int a, int b) { return a >= b; } }", SIR_GE, 0),
          ">= lowers to SIR_GE");

    // 4. Full Java 1.0 constant leaves.
    CHECK(compile_find("class T { long f()   { return 5L; } }",  SIR_LOADLONGCONST, 0),
          "long literal -> SIR_LOADLONGCONST");
    CHECK(compile_find("class T { float f()  { return 2.0f; } }", SIR_LOADFLOATCONST, 0),
          "float literal -> SIR_LOADFLOATCONST");
    CHECK(compile_find("class T { double f() { return 1.5; } }",  SIR_LOADDOUBLECONST, 0),
          "double literal -> SIR_LOADDOUBLECONST");

    // 5. Click (sir_optimizer) over the distinct comparison nodes: a constant
    //    comparison folds to its 0/1 result (proves the §3.8 fold dispatches by
    //    tag). `3 < 5` -> the SIR_LT disappears, replaced by a LoadConst.
    CHECK(compile_find("class T { boolean f() { return 3 < 5; } }", SIR_LT, 1) == NULL,
          "Click folds the constant comparison (no SIR_LT survives)");
    CHECK(compile_find("class T { boolean f() { return 3 < 5; } }", SIR_LOADCONST, 1) != NULL,
          "Click folds 3 < 5 to a LoadConst");

    // 6. §5.1 primitive conversions: a Java cast lowers to the right SIR
    //    conversion node end-to-end (sema classifies → ddcg emits). Operands
    //    are method params (non-constant) so the node survives opt=0.
    CHECK(compile_find("class T { long f(int i)    { return (long)i; } }",   SIR_I2L, 0),
          "(long)int -> SIR_I2L");
    CHECK(compile_find("class T { int f(long l)    { return (int)l; } }",    SIR_L2I, 0),
          "(int)long -> SIR_L2I");
    CHECK(compile_find("class T { double f(float x){ return (double)x; } }", SIR_F2D, 0),
          "(double)float -> SIR_F2D");
    CHECK(compile_find("class T { int f(double x)  { return (int)x; } }",    SIR_D2I, 0),
          "(int)double -> SIR_D2I (JLS narrowing)");
    CHECK(compile_find("class T { char f(int i)    { return (char)i; } }",   SIR_I2C, 0),
          "(char)int -> SIR_I2C");
    // Composite narrowing (byte)long = i2b(l2i) — both nodes emitted.
    CHECK(compile_find("class T { byte f(long l)   { return (byte)l; } }",   SIR_L2I, 0),
          "(byte)long emits the L2I half of the composite");
    CHECK(compile_find("class T { byte f(long l)   { return (byte)l; } }",   SIR_I2B, 0),
          "(byte)long emits the I2B half of the composite");

    // 7. cp_pack's parameter anchor from sema: sema numbers ONE slot per
    //    param regardless of type (a WASM local is one index — no 2-cell
    //    wides), so args_cells = this? + param_count. Disjoint locals past
    //    the params must coalesce into one packed cell; an over-counted
    //    anchor wrongly pins them at their original slots.
    CHECK(packed_max_locals(
        "class T { static int f(int x, int y) { int a = x + y; int b = a + 1; return b; } }",
        "f") == 3,
        "cp_pack sema anchor: 2 int params = 2 anchored cells; "
        "disjoint locals coalesce (max_locals 3)");
    CHECK(packed_max_locals(
        "class T { static int g(long l, int x) { int a = x + 1; int b = a + 1; return b; } }",
        "g") == 3,
        "cp_pack sema anchor: a long param occupies ONE slot (max_locals 3)");

    // 8. §15.15.5 bitwise complement: ~x lowers to Xor(x, minus-one) and the
    //    minus-one leaf must match the operand's WIDTH. The burg's only
    //    LoadConst tile is i32.const, so a long ~ needs a LoadLongConst leaf —
    //    a LoadConst(dt=long) makes the whole Xor spine uncoverable.
    const sir_node_t* nxl = compile_find(
        "class T { static long f(long x) { return ~x; } }", SIR_XOR, 0);
    CHECK(nxl != NULL, "~long lowers to SIR_XOR");
    CHECK(nxl && sir_child(nxl, 1) &&
          (int)sir_child(nxl, 1)->tag == SIR_LOADLONGCONST,
          "~long's minus-one leaf is SIR_LOADLONGCONST (i64-tileable)");
    const sir_node_t* nxi = compile_find(
        "class T { static int f(int x) { return ~x; } }", SIR_XOR, 0);
    CHECK(nxi && sir_child(nxi, 1) &&
          (int)sir_child(nxi, 1)->tag == SIR_LOADCONST,
          "~int's minus-one leaf stays SIR_LOADCONST (i32)");
    // The BitSet.clear shape: ~ under & feeding an array store — same rule.
    const sir_node_t* nxb = compile_find(
        "class T { static void f(long[] a, int b) {"
        " a[0] = a[0] & (~(1L << (b % 64))); } }", SIR_XOR, 0);
    CHECK(nxb != NULL, "array-store &~ shape lowers to SIR_XOR");
    CHECK(nxb && sir_child(nxb, 1) &&
          (int)sir_child(nxb, 1)->tag == SIR_LOADLONGCONST,
          "&~(1L<<s) minus-one leaf is SIR_LOADLONGCONST");

    // 9. StringTokenizer.skipDelimiters shape: a while whose && chain
    //    re-reads a FIELD the body advances. The (cur % 3) == 0 arm is NOT
    //    loop-invariant — Click must not fold its branch away (the jre
    //    tokenizer consumed every char as a delimiter when it did).
    CHECK(opt_method_keeps_rem_branch(
        "class T { int cur; int max; boolean ret;"
        " void skip() { while (!ret && cur < max && (cur % 3) == 0) {"
        " cur = cur + 1; } } }", "skip"),
        "the field-advancing loop keeps its rem-test branch under Click");

    // 10. Three independent ternaries over three DISTINCT calls. Each merge
    //     takes contributors {1, 0}, positionally identical — so a phi whose
    //     merge point is not part of its identity makes all three congruent,
    //     the peer-phi canonicalization rewrites every read onto the first,
    //     and the other two diamonds (calls included) are deleted as dead:
    //     `(g(3)?1:0)*100 + (g(65)?1:0)*10 + (g(4)?1:0)` became `s*111`.
    //     All three calls must survive (BitSet.get across a word boundary).
    CHECK(opt_method_tag_count(
        "class T { static boolean g(int i) { return i == 3 || i == 65; }"
        " static int f(int x) { return (g(3)?1:0)*100 + (g(65)?1:0)*10"
        " + (g(4)?1:0); } }", "f", SIR_INVOKESTATIC) == 3,
        "three ternaries over three distinct calls keep all three calls "
        "(phis at different merges are not congruent)");

    // 11. The §15 guard sidecar. The DDCG is the stage that KNOWS which
    //     branches are implicit-exception guards and what each one tests, so it
    //     records that — the optimizer must never rediscover it by matching on
    //     `Branch(Eq(x, LoadNull))`, which is a local copy of the frontend's
    //     knowledge that rots when a lowering changes.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int v; } class T { static int f(C c) { return c.v; } }",
            &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int found = -1;
        for (int i = 0; i < mc; i++)
            if (ms[i]->class_id >= nlib && ms[i]->name && !strcmp(ms[i]->name, "f"))
                found = i;
        CHECK(found >= 0, "the guard-sidecar probe method compiles");
        int ng = 0;
        const compiler_fact_t* gs = guards_of(&cctx, found, &ng);
        CHECK(ng == 1, "`c.v` on a parameter emits exactly ONE guard (the NPE)");
        CHECK(ng == 1 && gs[0].a == COMPILER_GUARD_NPE,
              "the guard is tagged as an NPE guard");
        CHECK(ng == 1 && gs[0].key && gs[0].key->tag == SIR_BRANCH,
              "the guard record points at the Branch node");
        CHECK(ng == 1 && gs[0].d == 1,
              "the NPE guard throws on its TRUE arm (c == null)");
        CHECK(ng == 1 && gs[0].b >= 0,
              "the guard names the SLOT it tests — not a node pointer, which the "
              "optimizer's rewrites would leave dangling");
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 12. §15 NPE guard elimination — the payoff. A deref of a freshly
    //     allocated object CANNOT be null, so its guard must go; a deref of a
    //     parameter may be null, so its guard must STAY (fail-closed: an
    //     unproven guard is always kept). Measured through the sidecar: an
    //     eliminated guard's Branch has been re-tagged away from SIR_BRANCH.
    {
        struct { const char* src; int want_gone; const char* label; } gk[] = {
          { "class C { int v; } class T { static int f() { C c = new C(); return c.v; } }",
            1, "the NPE guard on a freshly-allocated object is ELIMINATED" },
          { "class C { int v; } class T { static int f(C c) { return c.v; } }",
            0, "the NPE guard on a PARAMETER is kept — it may be null (fail-closed)" },
          { "class C { int v; } class T { static int f(boolean b) {"
            "  C c = b ? new C() : null; return c.v; } }",
            0, "a maybe-null merge keeps its guard" },
        };
        for (int i = 0; i < 3; i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(gk[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            /* Find the NPE guard BEFORE optimizing (the sidecar is the census). */
            int npe = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_NPE) npe = g;
            CHECK(npe >= 0, "an NPE guard was emitted for the deref");
            sir_optimize(&cctx, mi);
            int gone = (npe >= 0 && gs[npe].key->tag != SIR_BRANCH);
            CHECK(gone == gk[i].want_gone, gk[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 13. §15.13.1 upper-bounds guard, spec §5: "array.len(a) ⟹ [0,∞) and BINDS
    //     AN INDEX VAR TO THAT LENGTH; branch refinement on <, <=, == narrows the
    //     taken edge" ⟹ "drop ArrayIndexOutOfBounds when idx ⊑ [0, len)".
    //     No interval can express `i < a.length` — the bound is a VALUE — so the
    //     range element carries a SYMBOLIC upper bound naming that value, and the
    //     existing per-arm refinement puts it on the index's uses in the body.
    //     Fail-closed: a loop bounded by anything else keeps its guard.
    {
        struct { const char* src; int want_emitted, want_gone; const char* label; } bk[] = {
          { "class T { static int f(int[] a) { int s = 0;"
            "  for (int i = 0; i < a.length; i++) s += a[i]; return s; } }",
            1, 1, "`i >= a.length` is ELIMINATED inside `for (i=0; i<a.length; i++)`" },
          { "class T { static int f(int[] a, int n) { int s = 0;"
            "  for (int i = 0; i < n; i++) s += a[i]; return s; } }",
            1, 0, "a loop bounded by an UNRELATED n keeps its bounds guard (fail-closed)" },
          /* §15.10.1 + §10.7: (new T[n]).length IS n, so BOTH the read of `a` and the
           * write into the fresh `r` are provably in bounds — the store's bound is the
           * allocation's own size operand, which is the loop bound. Needs the load to
           * forward to the stored value and the array length to be the allocation's
           * size, and needs `a` inside the loop to be the same value as `a` outside it
           * (i.e. the loop's copy-φ for `a` subsumed, per spec §1). */
          { "class T { static int[] f(int[] a) { int[] r = new int[a.length]; int i = 0;"
            "  while (i < a.length) { r[i] = a[i]; i = i + 1; } return r; } }",
            2, 2, "`r[i] = a[i]` with `r = new int[a.length]`: BOTH bounds guards go" },
          /* Fail-closed: the fresh array is a different size than the loop bound, so
           * r's guard must survive on its own merits. */
          { "class T { static int[] f(int[] a) { int[] r = new int[a.length - 1]; int i = 0;"
            "  while (i < a.length) { r[i] = a[i]; i = i + 1; } return r; } }",
            2, 1, "`r = new int[a.length - 1]` keeps r's guard, drops a's" },
        };
        for (int i = 0; i < (int)(sizeof bk / sizeof bk[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(bk[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int emitted = 0;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH) emitted++;
            CHECK(emitted == bk[i].want_emitted,
                  "the expected number of upper-bounds guards was emitted");
            sir_optimize(&cctx, mi);
            int gone = 0;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                        && gs[g].key->tag != SIR_BRANCH) gone++;
            CHECK(gone == bk[i].want_gone, bk[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    {
        // §46 E2 (the IDX_HIGH −1) — pinned on the ACTUAL String.replace (`lib/java/lang/String.java`).
        //     It emits 2 bounds guards; BOTH must drop. `value[i]` (BEFORE the ternary) always
        //     folded; `buf[i]` (AFTER the ternary's merge) was the −1: pass B's strict-parity
        //     "refinement drops at joins" (plan §R.1 item 3) reset the index's `i < len` bound at
        //     the diamond's merge, so the second guard read the raw header φ. Fixed by the SCCP
        //     join (spec §4: the meet of the EDGE values — an all-agree merge keeps the
        //     refinement); the L0 lemma is test_click_partition's
        //     test_cp_refine_survives_all_agree_merge, seen RED first. Two earlier hypotheses
        //     both FALSIFIED against this pin: the plan's `cp_value_leader` swap, and
        //     "ArrayLength-receiver refine-transparency" (a NONNULL Refine is already a
        //     congruence-transparent copy-follower — cp_partition_init pass 2).
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program("class U {}", &arena, &nlib);   /* just pull in the prelude */
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int s_id = sema_find_class(&sctx, "java.lang.String");
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == s_id && ms[k]->name && !strcmp(ms[k]->name, "replace")) mi = k;
        CHECK(mi >= 0, "§46: String.replace resolves in the prelude");
        int ng = 0;
        const compiler_fact_t* gs = mi >= 0 ? guards_of(&cctx, mi, &ng) : NULL;
        int emitted = 0;
        for (int g = 0; g < ng; g++)
            if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH) emitted++;
        sir_optimize(&cctx, mi);
        int gone = 0;
        for (int g = 0; g < ng; g++)
            if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH && gs[g].key->tag != SIR_BRANCH) gone++;
        CHECK(emitted == 2 && gone == 2,
              "§46: BOTH of String.replace's bounds guards drop — value[i] straight-line under the "
              "loop bound; buf[i] past the ternary's merge, which must KEEP the all-agree "
              "refinement (SCCP join, spec §4). The IDX_HIGH −1 recovered.");
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 14. SOUNDNESS: the optimizer must never conclude that a method which cannot
    //     throw always throws. Every guard it drops must be dropped on its OK arm.
    //     `new int[2][2][2]` miscompiled exactly this way: a store's value never
    //     reached its load (the store was not re-armed when the memory version
    //     REACHING it changed — a store has no def-use edges, so only the reverse
    //     index can bring it back), the load then read the cell seed's null, an NPE
    //     guard "proved" a non-null array null and folded to its THROW arm, and the
    //     method's Return became unreachable and was deleted. Only the e2e corpus
    //     caught it, which is a failure of THIS suite.
    //
    //     The oracle is structural and needs no execution: a method whose source
    //     dereferences nothing null must still be able to return.
    {
        struct { const char* src; const char* label; } liveness[] = {
          { "class T { static int f(int x){ int[][][] a = new int[2][2][2];"
            "  a[1][1][1] = x; return a[1][1][1] + a[0][0][0]; } }",
            "3-D rectangular array: the method can still RETURN (no guard folded to throw)" },
          { "class T { static int f(int x){ int[][] a = new int[2][3];"
            "  a[1][2] = x; return a[1][2] + a[0][0]; } }",
            "2-D rectangular array: the method can still RETURN" },
          { "class T { static int f(int x){ int[][] a = new int[2][]; a[0] = new int[3];"
            "  a[0][1] = x; return a[0][1]; } }",
            "jagged array: the method can still RETURN" },
        };
        for (int i = 0; i < (int)(sizeof liveness / sizeof liveness[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(liveness[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            sir_optimize(&cctx, mi);
            CHECK(spine_has_tag(ms[mi]->entry, SIR_RETURN), liveness[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 15. SOUNDNESS, and the fail-closed twin of the IMMUTABLE-CELL rule.
    //
    //     The optimizer drops the memory edge of a read of the array overlay's
    //     backing-store field from VALUE IDENTITY: that field is written once, at
    //     allocation, and no Java program can name it (§10.7 gives an array only
    //     `length`, which is final). Without it, `a.length` before and after ANY store
    //     are two different values and a bounds guard can never see its loop's bound
    //     (§13 above is what proves it fires — disable the rule and §13 goes red).
    //
    //     It is the most dangerous rule in the analysis: it deliberately makes two
    //     loads congruent ACROSS AN INTERVENING STORE. So it needs a pin in the OTHER
    //     direction too — an ordinary MUTABLE field must NOT be treated that way, or
    //     the optimizer CSEs a stale read and the program computes the wrong value.
    //
    //     `d.f` is read, a store happens through a DIFFERENT reference `e`, and `d.f`
    //     is read again. `e` MAY ALIAS `d` — nothing says otherwise — so the second
    //     read may see 5 and the two reads are DIFFERENT values.
    //
    //     The store must go through `e`, not through `d`: a store through `d` itself is
    //     forwarded to its stored value (the load-after-store identity), which would
    //     separate the two reads for an unrelated reason and mask the very leak this
    //     pin exists to catch. That is exactly how the first version of this test came
    //     out vacuous.
    {
        const char* src =
            "class T { int f;"
            "  static int g(T d, T e){ int a = d.f; e.f = 5; return a + d.f; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
        /* Ask GVN directly: are the two reads the same VALUE? Counting nodes would not
         * see this — a leak makes them congruent, but the CSE lift is separately gated,
         * so the node count is unchanged while the analysis is already wrong. The
         * partition is the observable. */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
        CHECK(e != NULL, "an engine over the compiled method");
        const sir_node_t* rd1 = NULL;
        const sir_node_t* rd2 = NULL;
        collect_two(ms[mi]->entry, SIR_GETFIELD, &rd1, &rd2);
        CHECK(rd1 && rd2, "the source reads the mutable field twice");
        if (rd1 && rd2) {
            cp_vnode_t* v1 = vnode_for(e, rd1);
            cp_vnode_t* v2 = vnode_for(e, rd2);
            CHECK(v1 && v2 && v1->partition != v2->partition,
                  "two reads of a MUTABLE field across a store to it are DIFFERENT "
                  "values — the immutable-cell rule must not leak onto ordinary "
                  "fields, or the optimizer CSEs a stale read");
        }
        cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 16. Spec §2, the CAST TRANSFER: `v ← ref.cast τ(u)` ⟹ `{ O ∈ pts(u) | classOf(O)
    //     ≤ τ }` — a cast FILTERS the points-to set, and `br_on_cast` (here: a branch on
    //     `instanceof`) SPLITS it along BOTH successor edges.
    //
    //     This is a LATTICE-A TRANSFER, so it belongs to the substrate, not to the
    //     consumer that later drops the CheckCast node. It needs a real class hierarchy,
    //     so it is pinned here and not in the partition suite (which hand-builds SIR with
    //     no sema, and therefore cannot answer `classOf(O) ≤ τ` at all).
    //
    //     `classOf(O) ≤ τ` is JLS §4.10.2 subtyping — sema_ref_is_subtype. Answering it
    //     with the EXTENDS CHAIN (sema_is_subclass_of) says NO for an interface, and this
    //     filter DROPS on NO: casting to an interface would delete every object that
    //     implements it. The interface case below is that pin, and it is the reason the
    //     two questions now have two names.
    {
        struct { const char* src; const char* label; bool keeps_both; } casts[] = {
          /* Unrelated classes: the cast to A proves the B object cannot be here. */
          { "class A { } class B { }"
            " class T { static Object g(boolean c){ Object o;"
            "   if (c) o = new A(); else o = new B(); return (A) o; } }",
            "a cast FILTERS pts: the object of an unrelated class is dropped", false },
          /* FAIL-CLOSED: both implement the target interface, so neither may be dropped
           * — and an interface is in NOBODY's extends chain. */
          { "interface I { } class A implements I { } class B implements I { }"
            " class T { static Object g(boolean c){ Object o;"
            "   if (c) o = new A(); else o = new B(); return (I) o; } }",
            "a cast to an INTERFACE drops NOTHING that implements it — §4.10.2 subtyping, "
            "not the extends chain", true },
          /* FAIL-CLOSED: B extends A, so a cast to A keeps both. */
          { "class A { } class B extends A { }"
            " class T { static Object g(boolean c){ Object o;"
            "   if (c) o = new A(); else o = new B(); return (A) o; } }",
            "a cast to a SUPERCLASS keeps the subclass's object", true },
        };
        for (int i = 0; i < (int)(sizeof casts / sizeof casts[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(casts[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            CHECK(mi >= 0, "the test source compiled");
            if (mi < 0) continue;
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
            /* The cast on the OK arm of its own guard — not the null-path one. */
            const sir_node_t* isa = branch_on(ms[mi]->entry, SIR_INSTANCEOF, false);
            const sir_node_t* cc  = isa ? first_spine_expr(isa->branch.on_true,
                                                           SIR_CHECKCAST) : NULL;
            const sir_node_t *na = NULL, *nb = NULL;
            collect_two(ms[mi]->entry, SIR_NEW, &na, &nb);
            CHECK(cc && na && nb, "the source has one cast and two allocation sites");
            if (cc && na && nb) {
                /* Which site is the cast's own class? Ask the nodes, not the walk order. */
                const sir_node_t* same =
                    (na->new_.class_id == cc->check_cast.class_id) ? na : nb;
                const sir_node_t* other = (same == na) ? nb : na;
                cp_pts_t p = cp_pts_of_expr(e, cc);
                CHECK(cp_pts_has(e, p, cp_obj_of(e, same)),
                      "the cast keeps the object whose class IS the target");
                if (casts[i].keeps_both)
                    CHECK(cp_pts_has(e, p, cp_obj_of(e, other)), casts[i].label);
                else
                    CHECK(!cp_pts_has(e, p, cp_obj_of(e, other)), casts[i].label);
            }
            cp_free(e);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 16b. …and ⊥null SURVIVES a cast. `(A) null` is legal (JLS §5.5) — the cast does not
    //      throw and does not produce an object. Dropping ⊥null here would tell every
    //      downstream deref that the value is NonNull and delete a live NPE guard.
    {
        const char* src =
            "class A { int x; }"
            " class T { static Object g(boolean c){ Object o = null;"
            "   if (c) o = new A(); return (A) o; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
        /* The cast on the NULL path of its own guard — the one whose input IS null. */
        const sir_node_t* nb2 = branch_on(ms[mi]->entry, 0, true);
        const sir_node_t* cc  = nb2 ? first_spine_expr(nb2->branch.on_true,
                                                       SIR_CHECKCAST) : NULL;
        CHECK(cc != NULL, "the source has a guarded cast with a null path");
        if (cc) {
            cp_pts_t p = cp_pts_of_expr(e, cc);
            CHECK(cp_pts_has(e, p, CP_OBJ_NULL),
                  "⊥null survives a cast — `(A) null` succeeds, so the value may still "
                  "be null and its NPE guard must stand");
        }
        cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 16c. …and the branch on `instanceof` splits pts along BOTH edges (spec §2's
    //      `br_on_cast`). BOTH is the half a one-armed implementation silently skips.
    //
    //      The arms hold a PLAIN USE of `o` — no cast. A cast inside the arm would carry
    //      its OWN guard (see above), and that guard's refinement would filter the value
    //      whether or not the source-level `instanceof` refined anything: the pin would
    //      pass with the false edge unimplemented. The value returned from each arm is
    //      refined by that arm's edge and by nothing else.
    {
        const char* src =
            "class A { } class B { }"
            " class T { static Object g(boolean c){ Object o;"
            "   if (c) o = new A(); else o = new B();"
            "   if (o instanceof A) { return o; }"
            "   return o; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
        const sir_node_t *na = NULL, *nb = NULL;
        collect_two(ms[mi]->entry, SIR_NEW, &na, &nb);
        const sir_node_t* isa = branch_on(ms[mi]->entry, SIR_INSTANCEOF, false);
        CHECK(isa && na && nb, "an `instanceof` branch and two allocation sites");
        if (isa && na && nb) {
            int tested = isa->branch.cond->instance_of.class_id;
            const sir_node_t* admitted = (na->new_.class_id == tested) ? na : nb;
            const sir_node_t* excluded = (admitted == na) ? nb : na;
            const sir_node_t* rt = first_spine_node(isa->branch.on_true,  SIR_RETURN);
            const sir_node_t* rf = first_spine_node(isa->branch.on_false, SIR_RETURN);
            CHECK(rt && rf && rt->return_.value && rf->return_.value,
                  "each arm returns the tested value");
            if (rt && rf && rt->return_.value && rf->return_.value) {
                cp_pts_t pt = cp_pts_of_expr(e, rt->return_.value);
                cp_pts_t pf = cp_pts_of_expr(e, rf->return_.value);
                CHECK(cp_pts_has(e, pt, cp_obj_of(e, admitted))
                      && !cp_pts_has(e, pt, cp_obj_of(e, excluded)),
                      "on the TRUE edge of `o instanceof A` the value can only be an A");
                CHECK(cp_pts_has(e, pf, cp_obj_of(e, excluded))
                      && !cp_pts_has(e, pf, cp_obj_of(e, admitted)),
                      "and on the FALSE edge it can NOT be an A — `br_on_cast` splits "
                      "pts along BOTH successor edges, not just the taken one");
            }
        }
        cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 17. Spec §5's other consumer: "drop the INT_MIN/-1 overflow-wrap guard when the
    //     range excludes that pair."
    //
    //     JLS §15.17.2 says `MIN_VALUE / -1` WRAPS to MIN_VALUE; WASM's i32.div_s TRAPS
    //     on it. So the DDCG emits a `divisor == -1` arm that negates instead of
    //     dividing — a branch on every single integer division in the program, throwing
    //     nothing, and until now not recorded as a guard at all: not a guard KIND, not
    //     in the census, not pinned. It was invisible, so "how many are there and are
    //     any removed" had no answer. It is a RANGE question and nothing more, which is
    //     why it belongs to stage 2 and not to the type lattice.
    {
        struct { const char* src; int want_gone; const char* label; } dv[] = {
          { "class T { static int f(int a){ return a / 3; } }",
            1, "a divisor that is a CONSTANT other than -1 drops the overflow arm" },
          { "class T { static int f(int a, int b){ if (b > 0) { return a / b; } return 0; } }",
            1, "a divisor REFINED to > 0 by a branch drops it — the range excludes -1" },
          { "class T { static int f(int a, int b){ return a / b; } }",
            0, "an UNKNOWN divisor KEEPS it (fail-closed): b may be -1 and a may be "
               "MIN_VALUE, and WASM would trap where Java wraps" },
          { "class T { static int f(int a){ return a / -1; } }",
            1, "a divisor that IS -1 folds the other way — to the negate arm" },
        };
        for (int i = 0; i < (int)(sizeof dv / sizeof dv[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(dv[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int ov = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_DIV_OVERFLOW) ov = g;
            CHECK(ov >= 0, "the INT_MIN/-1 arm is RECORDED as a guard — a branch the "
                           "lowering inserted, which a proof can remove");
            if (ov < 0) continue;
            sir_optimize(&cctx, mi);
            int gone = (gs[ov].key->tag != SIR_BRANCH);
            CHECK(gone == dv[i].want_gone, dv[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 18. Spec §5/§8: range widening happens on the LOOP BACK EDGE, and which merges are
    //     loop headers is READ FROM THE SIDECAR (the DDCG built the loop and recorded its
    //     header) — never recovered with a dominator-based natural-loop finder.
    //
    //     Widening used to run at EVERY φ, which sidesteps the question: sound, and no
    //     dominance, but it widens where the spec says not to. It is now gated on the
    //     recorded header — and THAT IS WHAT THIS PINS. Only a cycle can ascend forever;
    //     if the header the DDCG recorded were not found (wrong scope kind, a lowering
    //     that stops recording), the accumulator's range would climb without bound and
    //     the fixpoint would NEVER TERMINATE. So this test's failure mode is a HANG, not
    //     a wrong answer — which is loud, and is exactly the risk the fail-closed cases
    //     in cp_node_const (no scopes recorded ⟹ widen; unrecorded merge ⟹ widen) exist
    //     to bound.
    //
    //     The accumulator must also NOT fold to a constant: it starts at the entry value
    //     and must fall once the back edge is live.
    {
        const char* src =
            "class T { static int f(int n){ int s = 0;"
            "  for (int i = 0; i < n; i = i + 1) { s = s + i; }"
            "  return s; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
        int loops = 0;
        for (int i = 0; i < nsc; i++)
            if (sc[i].kind == COMPILER_FACT_SCOPE && sc[i].a == COMPILER_SCOPE_LOOP)
                loops++;
        CHECK(loops >= 1, "the DDCG recorded the loop it built — the widening point is "
                          "LOOKED UP, not rediscovered");
        cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);       /* terminates */
        CHECK(e != NULL, "the fixpoint CONVERGED: widening fired at the recorded header");
        const sir_node_t* rt = first_spine_node(ms[mi]->entry, SIR_RETURN);
        CHECK(rt && rt->return_.value, "the method returns the accumulator");
        if (rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state != CP_C_KNOWN,
                  "…and the accumulator is NOT a constant — it starts at the entry value "
                  "and must fall once the back edge goes live");
        }
        cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 19. Spec §3, Lattice B: "Do NOT invent a second type domain — thread the existing
    //     type lattice (the WASM-repr authority) as a combined element:
    //     `τ̂(v) = ⨆_{O ∈ pts(v)} exactClassOf(O)`, meet/join over the class hierarchy.
    //     DERIVED, so it's free once pts runs."
    //
    //     DERIVED means a QUERY, not a stored field: a stored copy is exactly the second
    //     type domain the spec forbids, and it drifts from pts the moment pts refines.
    //     `exactClassOf(O)` is READ from the allocation node's own lattice type — the
    //     type lattice already computed it — and the join is `type_meet`, the lattice's
    //     own LUB. Nothing here re-derives a class hierarchy.
    //
    //     FAIL-CLOSED: one object of unknown class — a phantom, an Oret, the
    //     catch-all — and τ̂ is BOTTOM. Never a concrete class.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        /* One program, four methods — one per property of the join. */
        const char* src =
            "class P { } class A extends P { } class B extends P { } class C { }"
            " class T {"
            "   static Object one(){ return new C(); }"
            "   static Object sib(boolean c){ Object o;"
            "     if (c) o = new A(); else o = new B(); return o; }"
            "   static Object unk(Object p){ return p; }"
            "   static Object nul(boolean c){ Object o = null;"
            "     if (c) o = new C(); return o; } }";
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);

        /* The Type* is interned in the ENGINE's pool and dies with it, so take the two
         * fields the assertions are about while the engine is alive. Class ids are
         * sema's and outlive every engine. */
        struct { int kind; int32_t cls; int32_t site_a, site_b; } q[4];
        const char* names[4] = { "one", "sib", "unk", "nul" };
        for (int i = 0; i < 4; i++) {
            q[i].kind = -1; q[i].cls = -1; q[i].site_a = -1; q[i].site_b = -1;
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name
                    && !strcmp(ms[k]->name, names[i])) mi = k;
            CHECK(mi >= 0, "the test method compiled");
            if (mi < 0) continue;
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
            const sir_node_t *na = NULL, *nb = NULL;
            collect_two(ms[mi]->entry, SIR_NEW, &na, &nb);
            if (na) q[i].site_a = na->new_.class_id;
            if (nb) q[i].site_b = nb->new_.class_id;
            const sir_node_t* rt = first_spine_node(ms[mi]->entry, SIR_RETURN);
            const Type* t = (rt && rt->return_.value)
                          ? cp_tau_of_expr(e, rt->return_.value) : NULL;
            if (t) {
                q[i].kind = (int)t->kind;
                if (t->kind == TK_REF) q[i].cls = t->ref.class_id;
            }
            cp_free(e);
        }

        /* 1. A lone allocation: τ̂ is that class EXACTLY — `new C` allocates a C, never
         *    a subclass, so the exact class is known. */
        CHECK(q[0].kind == TK_REF && q[0].cls >= 0 && q[0].cls == q[0].site_a,
              "τ̂ of a lone `new C()` is exactly C");

        /* 2. Two siblings: the JOIN, which is NEITHER of them. */
        CHECK(q[1].kind == TK_REF && q[1].site_a >= 0 && q[1].site_b >= 0,
              "τ̂ of two sibling allocations is still a reference type");
        if (q[1].kind == TK_REF) {
            CHECK(q[1].cls != q[1].site_a && q[1].cls != q[1].site_b,
                  "…and it is NEITHER sibling — ⨆ is the JOIN over the class hierarchy, "
                  "not a pick of one");
            CHECK(sema_ref_is_subtype(&sctx, q[1].site_a, q[1].cls)
                  && sema_ref_is_subtype(&sctx, q[1].site_b, q[1].cls),
                  "…it is an UPPER BOUND of both — their common superclass P");
        }

        /* 3. FAIL-CLOSED: one object of unknown class poisons the join. */
        CHECK(q[2].kind == TK_BOTTOM,
              "τ̂ of a PARAMETER is BOTTOM — its class is unknown, and an unknown class "
              "must never join to a concrete one (fail closed)");

        /* 4. ⊥null joins AWAY: JLS §4.10.2 makes null a subtype of every reference type,
         *    so `c ? new C() : null` is still exactly C. Its NPE guard is §4's business,
         *    not §3's — the two elements are independent. */
        CHECK(q[3].kind == TK_REF && q[3].cls >= 0 && q[3].cls == q[3].site_a,
              "⊥null does not poison τ̂ — a maybe-null C is still a C");
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 20. Spec §2 ("if it drops nothing, the guard is dead") + §3 ("drop the ref.test
    //     ClassCast guard"). The FILTER is stage 1's; this is the CONSUMER.
    //
    //     THE TRAP, and it is written into the DDCG next to the record_guard call: this
    //     guard THROWS ON ITS FALSE ARM — `if (x instanceof A) ok else throw CCE` — the
    //     opposite of every other guard, which is exactly why the sidecar RECORDS the arm
    //     instead of letting a consumer assume it. Fold it the wrong way and a provably
    //     safe cast becomes an unconditional throw: the method stops being able to
    //     return, which is the same shape of miscompile stage 1b produced.
    //
    //     So the last case here is the §14 soundness oracle, not a nicety: a method whose
    //     cast CANNOT fail must still be able to RETURN.
    {
        struct { const char* src; int want_gone; const char* label; } cc[] = {
          { "class A { } class T { static A g(){ Object o = new A(); return (A) o; } }",
            1, "a cast whose object is provably an A drops its CLASS_CAST guard" },
          { "interface I { } class A implements I { }"
            " class T { static I g(){ Object o = new A(); return (I) o; } }",
            1, "…and so does a cast to an INTERFACE the object implements (§4.10.2, not "
               "the extends chain)" },
          { "class A { } class B extends A { }"
            " class T { static A g(){ Object o = new B(); return (A) o; } }",
            1, "…and a cast to a SUPERCLASS of the object's exact class" },
          { "class A { } class B { }"
            " class T { static A g(boolean c){ Object o;"
            "   if (c) o = new A(); else o = new B(); return (A) o; } }",
            0, "a cast that CAN fail KEEPS its guard — the B object is not an A, and this "
               "cast must still throw ClassCastException (fail-closed)" },
          { "class A { } class T { static A g(Object o){ return (A) o; } }",
            0, "a cast of a PARAMETER keeps its guard — an unknown object's class is "
               "unknown, and an unknown class proves nothing (fail-closed)" },
        };
        for (int i = 0; i < (int)(sizeof cc / sizeof cc[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(cc[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int cg = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_CLASS_CAST) cg = g;
            CHECK(cg >= 0, "the DDCG emitted a CLASS_CAST guard for the cast");
            if (cg < 0) continue;
            sir_optimize(&cctx, mi);
            CHECK((gs[cg].key->tag != SIR_BRANCH) == cc[i].want_gone, cc[i].label);
            /* THE ORACLE. Whichever way the guard went, the method must still be able to
             * reach a Return — folding a safe cast INTO its throw arm would delete it. */
            CHECK(spine_has_tag(ms[mi]->entry, SIR_RETURN),
                  "…and the method can still RETURN: a guard is only ever folded to the "
                  "arm that does NOT throw (the sidecar records which one that is)");
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 21. Spec §3's other consumer: "`pts(v)` singleton with exact class ⟹ DEVIRTUALIZE
    //     the vtable `call_ref` to a direct call (which re-exposes constants/inlining
    //     INSIDE the same fixpoint)."
    //
    //     The direct-call node is InvokeSpecial, NOT InvokeStatic: a static call has no
    //     receiver, and an instance method needs `this`. InvokeSpecial(obj, cls, midx)
    //     tiles to a plain `call` — which is precisely a devirtualized dispatch.
    //
    //     IDENTITY vs TYPE, and this distinction is what miscompiled `new int[2][2][2]`:
    //     Obj naming is 1-limited, so an allocation site inside a LOOP is a SUMMARY of
    //     many objects and may NOT be strongly updated. But every object that site ever
    //     produces has the SAME CLASS — so a singleton site IS exact for TYPE, even
    //     though it is not for identity. Devirt needs the class, not the identity, and
    //     the loop case below pins exactly that.
    {
        struct { const char* src; int want_direct; const char* label; } dv[] = {
          { "class A { int m(){ return 1; } }"
            " class T { static int g(){ A a = new A(); return a.m(); } }",
            1, "a call on a singleton exact class DEVIRTUALIZES to a direct call" },
          { "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } }"
            " class T { static int g(boolean c){ A a; if (c) a = new A(); else a = new B();"
            "   return a.m(); } }",
            0, "two possible classes do NOT devirtualize — the call is polymorphic and "
               "must still dispatch (fail-closed)" },
          { "class A { int m(){ return 1; } }"
            " class T { static int g(A a){ return a.m(); } }",
            0, "a call on a PARAMETER does NOT devirtualize — an unknown object may be "
               "any subclass, and an override would be skipped (fail-closed)" },
          { "class A { int m(){ return 1; } }"
            " class T { static int g(int n){ int s = 0;"
            "   for (int i = 0; i < n; i = i + 1) { A a = new A(); s = s + a.m(); }"
            "   return s; } }",
            1, "a LOOP's allocation site still devirtualizes: it is a SUMMARY for "
               "IDENTITY (no strong update) but every object it makes has the same "
               "CLASS, and devirt needs the class" },
          { "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } }"
            " class T { static int g(){ A a = new B(); return a.m(); } }",
            1, "…and it resolves the OVERRIDE: an exact B runs B.m, not A.m (JLS §8.4.8)" },
        };
        for (int i = 0; i < (int)(sizeof dv / sizeof dv[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(dv[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            CHECK(mi >= 0, "the test method compiled");
            if (mi < 0) continue;
            const sir_node_t *v0 = NULL, *x0 = NULL;
            collect_two(ms[mi]->entry, SIR_INVOKEVIRTUAL, &v0, &x0);
            CHECK(v0 != NULL, "the source contains a virtual call");
            sir_optimize(&cctx, mi);
            /* The node is rewritten IN PLACE, so the virtual call is gone from the graph
             * exactly when it devirtualized. */
            const sir_node_t *v1 = NULL, *x1 = NULL;
            collect_two(ms[mi]->entry, SIR_INVOKEVIRTUAL, &v1, &x1);
            CHECK((v1 == NULL) == (dv[i].want_direct != 0), dv[i].label);
            if (dv[i].want_direct) {
                const sir_node_t *s1 = NULL, *y1 = NULL;
                collect_two(ms[mi]->entry, SIR_INVOKESPECIAL, &s1, &y1);
                CHECK(s1 != NULL, "…and it became a DIRECT call (InvokeSpecial), which "
                                  "still passes the receiver — a static call would drop it");
            }
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 22. Spec §3's OTHER consumer — the half the plan silently dropped: "drop the
    //     ref.test ClassCast guard AND THE COVARIANT-STORE ARRAYSTORE GUARD when the
    //     element class is provably ≤ the array's component."
    //
    //     JLS §10.10: `a[i] = v` on a reference array must throw ArrayStoreException if v's
    //     runtime class is not assignable to the array's ACTUAL component type — because
    //     §10.2 array covariance lets `Object[] o = new String[1]`. The DDCG emits a CALL
    //     for it (`Class.arrayStoreCheck(a.elemClass, v)`), not a Branch, since the target
    //     is a runtime Class. That different SHAPE is why it was never recorded, never
    //     counted, and quietly dropped from the plan: 19 of them in the jre, invisible.
    //
    //     The array's component class is a FACT, not a pattern: the elemClass field is
    //     written at allocation with LoadClass(C), and the guard reads it back — the
    //     load-after-store identity forwards it, so the value the check tests against
    //     resolves to that LoadClass. Then τ̂ decides.
    {
        struct { const char* src; int want_gone; const char* label; } as[] = {
          { "class A { }"
            " class T { static void g(){ A[] a = new A[2]; a[0] = new A(); } }",
            1, "a store of the component's EXACT class drops the §10.10 check" },
          { "class A { } class B extends A { }"
            " class T { static void g(){ A[] a = new A[2]; a[0] = new B(); } }",
            1, "…and so does a store of a SUBCLASS of the component" },
          { "class A { }"
            " class T { static void g(){ A[] a = new A[2]; a[0] = null; } }",
            1, "…and a null store: storing null into a reference array NEVER throws "
               "ArrayStoreException (JLS §10.10)" },
          /* THE COVARIANCE CASE. This program is legal Java and MUST throw at runtime. */
          { "class A { } class B { }"
            " class T { static void g(){ Object[] o = new A[1]; o[0] = new B(); } }",
            0, "a genuinely COVARIANT store KEEPS its check — `Object[] o = new A[1]` is "
               "legal (§10.2) and `o[0] = new B()` must throw ArrayStoreException" },
          { "class A { }"
            " class T { static void g(Object[] o, A v){ o[0] = v; } }",
            0, "an array of UNKNOWN component keeps its check — a parameter may be an "
               "array of anything (fail-closed)" },
        };
        for (int i = 0; i < (int)(sizeof as / sizeof as[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(as[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int ag = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_ARRAY_STORE) ag = g;
            CHECK(ag >= 0, "the §10.10 array-store check is RECORDED as a guard — a check "
                           "the lowering inserted, which a proof can remove");
            if (ag < 0) continue;
            sir_optimize(&cctx, mi);
            CHECK((gs[ag].key->tag != SIR_EXPREFFECT) == as[i].want_gone, as[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 23. Spec §5: "branch refinement on `<`, `<=`, `==` narrows the taken edge."
    //
    //     The CONSTANT refinement already did all three (and their mirrors: `k > x` is
    //     `x < k` flipped, and the else-arm is the negation — so six comparisons collapse
    //     onto three shapes, and `!=` correctly narrows nothing).
    //
    //     What did NOT: the SYMBOLIC bound, where the bound is a VALUE and no interval can
    //     name it. `i < B` recorded B and `i <= B` recorded NOTHING, because the carrier
    //     (`hi_vn1`) means "strictly less than" and there was no bit for "≤". So an
    //     inclusive loop bound bound no index at all.
    //
    //     What an inclusive bound PROVES: the guard is `i >= len`, and `i <= B` makes that
    //     false exactly when `B + 1 ≤ len` — i.e. when `len ≡ B+1`, which is the shape
    //     `new int[n+1]` + `for (i = 0; i <= n; i++)`. The array-length identity already
    //     gives `len ≡ Add(n,1)`, so GVN can see it.
    //
    //     AND THE CASE THAT MUST NOT FIRE: `new int[n]` with `i <= n` is a genuine
    //     out-of-bounds (i reaches n == len). Its guard MUST stay. That is the pin that
    //     matters; the other two are the payoff.
    {
        struct { const char* src; int want_gone; const char* label; } le[] = {
          { "class T { static int f(int n){ int[] a = new int[n + 1]; int s = 0;"
            "  for (int i = 0; i <= n; i = i + 1) { s = s + a[i]; }"
            "  return s; } }",
            1, "an INCLUSIVE loop bound drops the guard when the array is one longer: "
               "`i <= n` and `len ≡ n+1` ⟹ `i < len`" },
          /* THE SOUNDNESS PIN. */
          { "class T { static int f(int n){ int[] a = new int[n]; int s = 0;"
            "  for (int i = 0; i <= n; i = i + 1) { s = s + a[i]; }"
            "  return s; } }",
            0, "…but `new int[n]` with `i <= n` KEEPS its guard — i reaches n, which IS "
               "len, and the program must still throw ArrayIndexOutOfBounds" },
          /* The strict case must not regress. */
          { "class T { static int f(int n){ int[] a = new int[n]; int s = 0;"
            "  for (int i = 0; i < n; i = i + 1) { s = s + a[i]; }"
            "  return s; } }",
            1, "…and the STRICT bound still drops it (`i < n`, `len ≡ n`)" },
        };
        for (int i = 0; i < (int)(sizeof le / sizeof le[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(le[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int hi = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH) hi = g;
            CHECK(hi >= 0, "the DDCG emitted an upper-bounds guard for the array read");
            if (hi < 0) continue;
            sir_optimize(&cctx, mi);
            CHECK((gs[hi].key->tag != SIR_BRANCH) == le[i].want_gone, le[i].label);
            CHECK(spine_has_tag(ms[mi]->entry, SIR_RETURN),
                  "…and the method can still RETURN");
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    {
        // §47 — the E3 optimistic-solve SOUNDNESS lemmas, pinned at L1 (real Java source
        //     through the real DDCG + optimizer), NOT hand-built SIR. These are the same
        //     lemmas test_click_partition pins at L0; the point of layering is that they
        //     hold at EVERY level. want_gone=0 ⟹ the bounds guard MUST survive; folding it
        //     is a miscompile the optimistic solve must not commit.
        struct { const char* src; int want_gone; const char* label; } opt[] = {
          // Click Fig 2.2 (prime-the-pump): the ONLY bound on i comes from the loop's own
          // `i < n`, which is checked AFTER the access and is unrelated to a.length. On
          // entry i is an unbounded param, so a[i] at the loop top can be out of range.
          { "class T { static int f(int[] a, int i, int n){ int s = 0;"
            "  while (true) { s = s + a[i]; if (i < n) { i = i + 1; } else { return s; } } } }",
            0, "§47a (Click Fig 2.2, prime-the-pump): a[i]'s guard at the loop top must "
               "SURVIVE — the entry i is unbounded and the `i < n` bound is neither before "
               "the access nor related to a.length" },
          // Click fn.4 (monotone descent): i is bounded `< n` on ENTRY but the latch
          // redefines it to n (out of range). The header's transiently-kept bound must
          // descend, so a[i]'s guard must SURVIVE (i == n ≥ a.length on the 2nd iteration).
          { "class T { static int f(int[] a, int i, int n){ int s = 0;"
            "  if (i < n) { int k = 0; while (k < 2) { s = s + a[i]; i = n; k = k + 1; } }"
            "  return s; } }",
            0, "§47b (Click fn.4, monotone descent): i's entry bound is killed by the latch "
               "`i = n`, so a[i]'s guard must SURVIVE — folding it on the stale header bound "
               "reads out of bounds unguarded" },
        };
        for (int i = 0; i < (int)(sizeof opt / sizeof opt[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(opt[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            int hi = -1;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH) hi = g;
            CHECK(hi >= 0, "§47: the DDCG emitted an upper-bounds guard for a[i]");
            if (hi < 0) continue;
            sir_optimize(&cctx, mi);
            CHECK((gs[hi].key->tag != SIR_BRANCH) == opt[i].want_gone, opt[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    {
        // §47c — Integer.toString's shape (a DECREMENTING index into a fixed buffer,
        //     `int p = buf.length; while (…) { buf[--p] = …; }`), pinned at L1. The
        //     IDX_LOW guard (`p-1 >= 0`) can be dropped only if the analysis bounds the
        //     iteration count — which it cannot (the loop condition is on `i`, not `p`).
        //     So the LOWER-bounds guard MUST survive; folding it writes below index 0.
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static void f(char[] buf, int i){ int p = buf.length;"
            "  while (i <= -10) { buf[--p] = (char)(48 - (i % 10)); i = i / 10; } } }",
            &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        int ng = 0;
        const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
        int lo = -1;
        for (int g = 0; g < ng; g++)
            if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_LOW) lo = g;
        CHECK(lo >= 0, "§47c: the DDCG emitted a lower-bounds guard for buf[--p]");
        if (lo >= 0) {
            sir_optimize(&cctx, mi);
            CHECK(gs[lo].key->tag == SIR_BRANCH,
                  "§47c (decrement-into-fixed-buffer): buf[--p]'s IDX_LOW guard must "
                  "SURVIVE — the analysis cannot bound the decrement count, so `p-1 >= 0` "
                  "is not provable; folding it writes below index 0");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    {
        // §47d — File.list's shape (COUNT-then-FILL): a first loop counts the zero
        //     bytes into `count`, then `names = new String[count]`, then a second loop
        //     fills `names[idx++]` once per zero byte. The fold `idx < names.length`
        //     needs the VFG array-length identity (names.length ≡ count) AND `idx <
        //     count` — but `idx < count` is a CROSS-LOOP semantic invariant nothing in
        //     the SIR establishes. So the IDX_HIGH guard on names[idx] must SURVIVE;
        //     the optimistic solve must not manufacture the bound. This is the
        //     memory-fact (VFG) × range combination in Click's one fixpoint.
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static String[] f(int total, byte[] mem){"
            "  int count = 0;"
            "  for (int i = 0; i < total; i = i + 1) { if (mem[i] == 0) count = count + 1; }"
            "  String[] names = new String[count];"
            "  int idx = 0;"
            "  for (int i = 0; i < total; i = i + 1) { if (mem[i] == 0) { names[idx] = null; idx = idx + 1; } }"
            "  return names; } }",
            &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        int ng = 0;
        const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
        /* the IDX_HIGH guard whose array is `names` (the store target), not `mem` */
        int hiNames = -1;
        for (int g = 0; g < ng; g++)
            if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_HIGH) hiNames = g;  /* last-emitted = the fill loop's */
        CHECK(hiNames >= 0, "§47d: the DDCG emitted an upper-bounds guard for names[idx]");
        if (hiNames >= 0) {
            sir_optimize(&cctx, mi);
            CHECK(gs[hiNames].key->tag == SIR_BRANCH,
                  "§47d (count-then-fill, VFG length-identity × range): names[idx]'s "
                  "IDX_HIGH guard must SURVIVE — `idx < count` is a cross-loop invariant "
                  "the SIR does not establish; the optimistic solve must not fold it");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }



    // 24. JLS §13.1 + §4.12.4: a use of a CONSTANT VARIABLE — a `final` field of primitive
    //     type whose initializer is a constant expression — is resolved to its VALUE at
    //     compile time. It must NOT reach the SIR as a GetStatic.
    //
    //     The fold used to handle byte/short/int and give up on the other five, using its
    //     own int-only predicate rather than the ONE §15.27 evaluator. So a `static final
    //     long/double/float/char/boolean` emitted a real GetStatic — which is not just a
    //     missed optimization: a `static final boolean DEBUG = false` could not be seen as
    //     a constant `false`, so §14.19 could not call `if (DEBUG) {…}`'s body unreachable,
    //     and every range/type consumer downstream lost a constant it was entitled to.
    {
        struct { const char* src; const char* label; } cv[] = {
          { "class T { static final int  K = 15;   static int  f(){ return K; } }",
            "a static final INT use folds (this one always worked)" },
          { "class T { static final long K = 99L;  static long f(){ return K; } }",
            "a static final LONG use folds — it used to emit a GetStatic" },
          { "class T { static final double K = 0.5; static double f(){ return K; } }",
            "a static final DOUBLE use folds" },
          { "class T { static final float K = 0.5f; static float f(){ return K; } }",
            "a static final FLOAT use folds" },
          { "class T { static final char K = 'x';  static char f(){ return K; } }",
            "a static final CHAR use folds" },
          { "class T { static final boolean K = true; static boolean f(){ return K; } }",
            "a static final BOOLEAN use folds" },
          /* §15.27's arithmetic over constant variables is itself constant. */
          { "class T { static final int A = 3; static final int B = A * 4 + 1;"
            "  static int f(){ return B; } }",
            "an initializer BUILT from other constant variables folds (§15.27)" },
        };
        for (int i = 0; i < (int)(sizeof cv / sizeof cv[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(cv[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
            CHECK(mi >= 0, "the test method compiled");
            if (mi < 0) continue;
            const sir_node_t *g = NULL, *g2 = NULL;
            collect_two(ms[mi]->entry, SIR_GETSTATIC, &g, &g2);
            CHECK(g == NULL, cv[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 24d. The CALL TARGETS the ddcg records. burg emits a call's funcidx from the Invoke
    //      node's own (class, method) — codegen_wasm.burg's Invoke rules all read
    //      `wasm_func_index(node->invoke_*.class_id, ...)` — and the ddcg chose that pair when
    //      it picked the §15.12 dispatch form. So it says so, and the backend looks it up
    //      instead of re-deriving the set by walking the graph.
    //
    //      Both pipelines, because -O0 is the correctness base: a fact only the optimizer
    //      records is a fact the unoptimized compiler does not have. The -O arm additionally
    //      asks whether Click's devirtualized target needs a record of its own, or whether it
    //      is already reachable as the vtable-slot occupant sema published.
    {
        const char* src =
            "class B24 { int m(){ return 1; } }\n"
            "class D24 extends B24 { public int m(){ return 2; } }\n"
            "class T { static int g(int x){ return x + 1; }\n"
            "          static int f(int x){ return g(x); }\n"
            "          static int h(){ D24 d = new D24(); return d.m(); } }";
        for (int opt = 0; opt <= 1; opt++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            cctx.optimize = (opt != 0);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            for (int i = 0; i < mc; i++) if (opt) sir_optimize(&cctx, i);

            int g_cls = -1, g_mid = -1, fi = -1;
            for (int ci = 0; ci < (int)bbq_vec_len(sctx.classes); ci++) {
                const sema_class_t* c = sema_get_class(&sctx, ci);
                if (!c->name || strcmp(c->name, "T")) continue;
                g_cls = ci;
                for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
                    if (c->methods[mi].name && !strcmp(c->methods[mi].name, "g")) g_mid = mi;
            }
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) fi = k;
            CHECK(g_cls >= 0 && g_mid >= 0 && fi >= 0, "T.g and T.f resolved (precondition)");

            int nf = 0, found = 0;
            const compiler_fact_t* f = fi >= 0 ? compiler_get_facts(&cctx, fi, &nf) : NULL;
            for (int i = 0; i < nf; i++)
                if (f[i].kind == COMPILER_FACT_CALL_TARGET
                    && f[i].a == g_cls && f[i].b == g_mid) found++;
            CHECK(found > 0, opt ? "-O:  T.f records a call target naming T.g (JLS 13.1's pair)"
                                 : "-O0: T.f records a call target naming T.g (JLS 13.1's pair)");

            /* The devirtualization question, decided rather than assumed. T.h's `d.m()` names
             * B24.m at the call site; Click rewrites it to a direct call on D24.m, a node the
             * ddcg never built. If that target is reachable without a Click-side record, no
             * such recorder is needed — and it should be, because a devirtualized target is by
             * construction the occupant of that vtable slot, which sema already published. */
            int d_cls = -1, d_mid = -1;
            for (int ci = 0; ci < (int)bbq_vec_len(sctx.classes); ci++) {
                const sema_class_t* c = sema_get_class(&sctx, ci);
                if (!c->name || strcmp(c->name, "D24")) continue;
                d_cls = ci;
                for (int mi = 0; mi < (int)bbq_vec_len(c->methods); mi++)
                    if (c->methods[mi].name && !strcmp(c->methods[mi].name, "m")) d_mid = mi;
            }
            int as_occupant = 0;
            for (int i = 0; i < sema_vtarget_count(&sctx); i++) {
                sema_vtarget_ent_t v = sema_vtarget_at(&sctx, i);
                if (v.impl_class == d_cls && v.impl_method == d_mid) as_occupant++;
            }
            CHECK(d_cls >= 0 && d_mid >= 0 && as_occupant > 0,
                  "a devirtualizable override is reachable as a vtable-slot occupant, so the "
                  "optimizer's minted target needs no record of its own");

            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 24c. JLS §13.1, the IDENTITY half of the same rule. A reference is "resolved at compile
    //      time to a symbolic reference to THE CLASS OR INTERFACE IN WHICH the denoted method
    //      or constructor IS DECLARED" — and likewise for a field, "the class or interface in
    //      which the field is declared". Not the type the access was written through.
    //
    //      §13.1 gives the reason: it "makes the binaries more robust", because a later release
    //      may ADD `Sub.g` or `Sub.k` without invalidating a binary that named `Base`. Recording
    //      the access type instead silently re-points the reference at whatever the subclass
    //      grows later, which is §13.4.5's incompatibility rather than its guarantee.
    //
    //      The DDCG reads sema for both (sema_method_decl_class / sema_field_decl_class,
    //      compiler.ddcg:241/237) rather than re-deriving from the AST, which is the whole
    //      point — but nothing held it there, so a regression to the access type would have
    //      been invisible until a plugin failed to link. Sub declares NEITHER member, so the
    //      access type and the owner are provably different class ids.
    {
        const char* src =
            "class Base { int k; int g(){ return k; } }\n"
            "class Sub extends Base { }\n"
            "class T { static int f(Sub s){ return s.g() + s.k; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);

        int base_id = -1, sub_id = -1, mi = -1;
        for (int ci = 0; ci < (int)bbq_vec_len(sctx.classes); ci++) {
            const sema_class_t* c = sema_get_class(&sctx, ci);
            if (!c->name) continue;
            if (!strcmp(c->name, "Base")) base_id = ci;
            if (!strcmp(c->name, "Sub"))  sub_id  = ci;
        }
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        CHECK(base_id >= 0 && sub_id >= 0 && base_id != sub_id && mi >= 0,
              "Base and Sub are distinct classes and T.f compiled (precondition)");

        if (mi >= 0) {
            const sir_node_t *iv = NULL, *iv2 = NULL, *gf = NULL, *gf2 = NULL;
            collect_two(ms[mi]->entry, SIR_INVOKEVIRTUAL, &iv, &iv2);
            collect_two(ms[mi]->entry, SIR_GETFIELD,      &gf, &gf2);
            CHECK(iv && iv->invoke_virtual.class_id == base_id,
                  "JLS 13.1: s.g() names Base — the class that DECLARES g, not the access type Sub");
            CHECK(gf && gf->get_field.class_id == base_id,
                  "JLS 13.1: s.k names Base — the class that DECLARES k, not the access type Sub");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 24b. …and the payoff that is NOT an optimization: §14.19 reachability. With the
    //      boolean constant folded, `if (DEBUG) {…}` has a constant-false condition, so its
    //      body is dead and the guard inside it goes with it. Before, DEBUG was a GetStatic
    //      and none of that could be seen.
    {
        const char* src =
            "class T { static final boolean DEBUG = false;"
            "  static int f(int[] a){ if (DEBUG) { return a[0]; } return 0; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        sir_optimize(&cctx, mi);
        CHECK(!spine_has_tag(ms[mi]->entry, SIR_ARRAYLOAD),
              "`if (DEBUG)` with `static final boolean DEBUG = false` is DEAD — the array "
              "read inside it, and its guards, are gone (§14.19 via §13.1)");
        CHECK(spine_has_tag(ms[mi]->entry, SIR_RETURN),
              "…and the method still returns");
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 25. BRANCH REFINEMENTS COMPOSE. A value tested by a nested branch is ALREADY refined
    //     by the enclosing one, and the inner Refine must be built on the OUTER one — not
    //     on the raw value, which discards the enclosing fact.
    //
    //     `if (i >= 0) { if (i <= n + 1) { … a[i] … } }`: the outer arm proves `i >= 0`, so
    //     the LOWER-bounds guard (`i < 0`) is dead inside it. Built on the raw value, the
    //     inner Refine intersects with a fact that no longer carries `[0, MAX]`, and the
    //     lower-bounds guard comes back — the enclosing refinement silently thrown away.
    //
    //     Found in ASCIIToBinaryBuffer: `if (exp >= 0) { if (exp <= MAX_SMALL_TEN + slop)`,
    //     where the second condition's right operand is an Add, so it takes the symbolic
    //     path and builds a second Refine. Three IDX_LOW guards were lost to it.
    {
        const char* src =
            "class T { static int f(int i, int n, int[] a){"
            "  if (i >= 0) { if (i <= n + 1) { return a[i]; } }"
            "  return 0; } }";
        bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        int ng = 0;
        const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
        int lo = -1;
        for (int g = 0; g < ng; g++)
            if (gs[g].a == COMPILER_GUARD_ARRAY_INDEX_LOW) lo = g;
        CHECK(lo >= 0, "the DDCG emitted a lower-bounds guard for the array read");
        if (lo >= 0) {
            sir_optimize(&cctx, mi);
            CHECK(gs[lo].key->tag != SIR_BRANCH,
                  "the ENCLOSING `i >= 0` still proves the lower-bounds guard dead inside a "
                  "nested branch — refinements COMPOSE, the inner one does not discard the "
                  "outer one");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 26. Spec §0: "the DEFUNCTIONALIZED `call_ref` TARGET SET (the usually-hard part, given
    //     precise)" — "the VFG paper spends its whole scalability budget approximating
    //     exactly what you already have."
    //
    //     A call site's target set is DERIVED: the receiver's possible classes (lattice A),
    //     each resolved through JLS §8.4.8 (sema_resolve_virtual — the same authority the
    //     WASM vtable builder fills its slots with). Devirtualize when that SET is a
    //     singleton. §3's "pts singleton with exact class" is only the special case where
    //     the set has one element because there is one OBJECT — and that is all I built the
    //     first time. Three things it threw away, and the first two are sound in every mode:
    //
    //       (a) two receiver classes, ONE implementation (a subclass that doesn't override);
    //       (b) a FINAL method or FINAL class — no override can exist, whatever the receiver;
    //       (c) CHA — only one implementation program-wide. UNSOUND unless the world is
    //           CLOSED: in RUNTIME/PLUGIN mode a later-loaded plugin can subclass a jre class
    //           and override it. Gated on SEMA_MODE_WHOLE, and pinned that way below.
    {
        struct { const char* src; int mode; int want_direct; const char* label; } tg[] = {
          /* (a) TWO objects, ONE target — B inherits A.m without overriding it. */
          { "class A { int m(){ return 1; } } class B extends A { }"
            " class T { static int g(boolean c){ A a; if (c) a = new A(); else a = new B();"
            "   return a.m(); } }",
            SEMA_MODE_WHOLE, 1,
            "two receiver CLASSES that share ONE implementation devirtualize — the TARGET "
            "set is the singleton, though pts is not" },
          /* FAIL-CLOSED: B overrides ⟹ two targets ⟹ the call is polymorphic. */
          { "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } }"
            " class T { static int g(boolean c){ A a; if (c) a = new A(); else a = new B();"
            "   return a.m(); } }",
            SEMA_MODE_WHOLE, 0,
            "…but two IMPLEMENTATIONS do not — the call must still dispatch" },
          /* (b) a FINAL method: no override can exist, so an unknown receiver still has one
           *     target. (The receiver is non-null on the deref's OK arm, so the direct call
           *     cannot skip an NPE.) */
          { "class A { final int m(){ return 1; } }"
            " class T { static int g(A a){ return a.m(); } }",
            SEMA_MODE_WHOLE, 1,
            "a FINAL method devirtualizes even on an UNKNOWN receiver — no override can "
            "exist, so the target set is a singleton" },
          /* FAIL-CLOSED: the same call, method NOT final, receiver unknown. */
          { "class A { int m(){ return 1; } }"
            " class T { static int g(A a){ return a.m(); } }",
            SEMA_MODE_RUNTIME, 0,
            "a NON-final method on an unknown receiver KEEPS its dispatch in RUNTIME mode — "
            "the world is OPEN, and a plugin may subclass A and override m()" },
        };
        for (int i = 0; i < (int)(sizeof tg / sizeof tg[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(tg[i].src, &arena, &nlib);
  /* WHOLE-program: the §7 call-graph summaries this block checks reach into
   * the java.lang ctor chain, so the prelude bodies must be compiled. */
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; /* analyze_from stays 0 */
            sctx.mode = (sema_mode_t)tg[i].mode;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            CHECK(mi >= 0, "the test method compiled");
            if (mi < 0) continue;
            const sir_node_t *v0 = NULL, *x0 = NULL;
            collect_two(ms[mi]->entry, SIR_INVOKEVIRTUAL, &v0, &x0);
            CHECK(v0 != NULL, "the source contains a virtual call");
            sir_optimize(&cctx, mi);
            const sir_node_t *v1 = NULL, *x1 = NULL;
            collect_two(ms[mi]->entry, SIR_INVOKEVIRTUAL, &v1, &x1);
            CHECK((v1 == NULL) == (tg[i].want_direct != 0), tg[i].label);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 27. Spec §6 — LATTICE E, ESCAPE. `NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥)`, meet =
    //     min, monotone DOWNWARD. Keyed on the abstract OBJECT (§6's own domain is "per
    //     object site O"), transferred per NODE, inside the ONE fixpoint (§8's membership
    //     test — not a Fig-6 side-pass, whatever §6's wording suggests on its own).
    //
    //     The SOURCES are §6's, and the one for a call is §7's, not mine: there are no
    //     summaries yet, so every call is a BOTTOM METHOD, and §7 says "a ref passed to a
    //     native → ArgEscape". A phantom and an `Oret` are ALREADY external, so they seed at
    //     ArgEscape — which is what makes "stored into a param-reachable object" fall out of
    //     the plain heap rule rather than needing a case of its own.
    {
        struct { const char* src; int want; const char* label; bool summarize; } es[] = {
          /* THE STANDING RED PIN, FLIPPED GREEN once the harness gained summaries (07-15). It was red, deliberately, from
           * the day stage 4 landed: `new C()` calls the synthesized default ctor (JLS §8.8.9),
           * so with no summaries the receiver is passed to a §7 BOTTOM METHOD and escapes. With
           * `summarize` set, this case runs the REAL path — the ctor chain summarized
           * supers-first, exactly as the reverse-topological driver orders it — and the CLEAN chain leaves the
           * object NoEscape. The src and the CP_ESC_NONE expectation are the original falsifier,
           * UNTOUCHED; only the harness gained the summaries the driver always provides.
           * The remaining cases run WITHOUT summaries on purpose: they pin the BOTTOM-GRAPH
           * rules, which stay real for natives and plugin-mode jre imports. */
          { "class C { int f; }"
            " class T { static int g(){ C c = new C(); c.f = 5; return c.f; } }",
            CP_ESC_NONE,
            "an object that never leaves the method is NoEscape — its ctor chain is CLEAN "
            "via the §7 summaries", true },
          { "class C { int f; }"
            " class T { static C g(){ return new C(); } }",
            CP_ESC_ARG,
            "an object that flows to a RETURN is ArgEscape", false },
          { "class C { int f; }"
            " class T { static C S; static void g(){ S = new C(); } }",
            CP_ESC_GLOBAL,
            "an object stored into a STATIC is GlobalEscape (§2: a global.set gives every "
            "object in pts(x) a GlobalEscape source)", false },
          { "class C { int f; }"
            " class T { static void h(C c){ } static void g(){ h(new C()); } }",
            CP_ESC_ARG,
            "an object PASSED TO A CALL is ArgEscape — with no summary the callee is a "
            "BOTTOM METHOD, and §7 says a ref passed to one escapes", false },
          { "class C { int f; C n; }"
            " class T { static void g(C p){ p.n = new C(); } }",
            CP_ESC_ARG,
            "an object stored into a PARAM's field is ArgEscape — the param is a phantom, "
            "which seeds at ArgEscape, and the heap rule does the rest", false },
          /* §2: "a static-field global imported from jre is EXTERNAL: pts = {Oext},
           * GlobalEscape". So what a STATIC holds is GlobalEscape, and storing THROUGH it must
           * confer GlobalEscape — §6's "stored into a field of an already-GlobalEscape object".
           * Seeding every external object uniformly at ArgEscape (which is what this did
           * before) stops one level short and hands §7's MapsTo the wrong field. */
          { "class C { int f; C n; }"
            " class T { static C S; static void g(){ C x = new C(); S.n = x; } }",
            CP_ESC_GLOBAL,
            "an object stored into a field of an object read from a STATIC is GlobalEscape — "
            "a static's contents are reachable from a global, not merely from a param", false },
          /* §6 / JLS §11.3.1 — THE EXCEPTION VALUE REACHES THE HANDLER'S VARIABLE.
           *
           * The handler is a MERGE (spec §1), and what each exceptional edge carries into
           * the landing slot is the value THROWN on that edge. If the landing slot is
           * instead defined by a blanket opaque, the caught reference is a different value
           * from the thrown object — pts cannot connect them — and a catch body that LEAKS
           * the exception is invisible to the escape lattice.
           *
           * This is observable from SOURCE despite the ctor mask, because the two states
           * differ: `new C()` runs a ctor, so §7's bottom graph already makes it ArgEscape;
           * storing the CAUGHT ref into a static must take it all the way to GlobalEscape.
           * ArgEscape here means the store was applied to some unknown object instead of to
           * the one actually thrown. */
          { "class C extends Exception { }"
            " class T { static C S;"
            "   static void g(){ try { throw new C(); } catch (C e) { S = e; } } }",
            CP_ESC_GLOBAL,
            "a CAUGHT exception that is then stored into a STATIC is GlobalEscape — the "
            "caught value IS the thrown object (JLS §11.3.1); an opaque landing slot severs "
            "that edge and the leak goes unseen", false },
          /* The anti-overshoot control: connecting the edge must not make every caught
           * object escape globally. Contained-and-not-leaked stays where the ctor left it. */
          { "class C extends Exception { }"
            " class T { static void g(){ try { throw new C(); } catch (C e) { } } }",
            CP_ESC_ARG,
            "a caught exception that does NOT leak is not GlobalEscape — wiring the value "
            "edge must not by itself escape the object (it is ArgEscape only via its ctor)", false },
        };
        /* The HEAP RULE, both directions — and the only allocation stage 4 can prove local.
         *
         * A §10.7 array is the one site the DDCG builds with NO ctor call: `backing =
         * NewArray(n)`, `wrapper = New(PrimArray)`, `PutField(wrapper.data = backing)`. So it
         * is (a) the control proving the lattice is not vacuously escaping everything, and
         * (b) the only way to exercise the heap rule's NEGATIVE case from Java source — every
         * object with a Java-visible field has a ctor, hence is ArgEscape, hence always
         * confers escape as a receiver.
         *
         * The pair is the whole rule: the store's receiver decides. A NoEscape wrapper confers
         * NOTHING on the backing it holds; a GlobalEscape one confers GlobalEscape — and that
         * second one only comes out right if the sweep SATURATES, since the PutField is
         * reached before the PutStatic that sinks the wrapper. */
        struct { const char* src; int want_wrap; int want_back; const char* label; } ea[] = {
          { "class T { static int g(){ int[] a = new int[4]; a[0] = 7; return a[0]; } }",
            CP_ESC_NONE, CP_ESC_NONE,
            "a local ARRAY — the one allocation with no ctor call — is NoEscape, and so is its "
            "BACKING: a store into a NON-escaping receiver confers nothing (without this the "
            "lattice collapses to `everything escapes`)" },
          { "class T { static int[] S;"
            "  static void g(){ int[] a = new int[4]; a[0] = 7; S = a; } }",
            CP_ESC_GLOBAL, CP_ESC_GLOBAL,
            "an array stored into a STATIC is GlobalEscape — and so is its BACKING, which only "
            "falls if the sweep SATURATES (the PutField is reached before the PutStatic)" },
        };
        for (int i = 0; i < (int)(sizeof es / sizeof es[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(es[i].src, &arena, &nlib);
  /* WHOLE-program: these cases summarize the java.lang ctor chain (§7), so the
   * prelude bodies must be compiled. */
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; /* analyze_from stays 0 */
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            CHECK(mi >= 0, "the test method compiled");
            if (mi < 0) continue;
            if (es[i].summarize) {
                /* The REAL path: the ctor chain summarized supers-first, as the reverse-topological driver orders it. */
                int cid0 = sema_find_class(&sctx, "C");
                const sema_class_t* csc0 = sema_get_class(&sctx, cid0);
                int sup0 = csc0 ? csc0->super_id : -1;
                for (int pass = 0; pass < 2; pass++) {
                    int want_cls = (pass == 0) ? sup0 : cid0;
                    for (int k = 0; k < mc; k++) {
                        if (ms[k]->class_id != want_cls) continue;
                        const sema_class_t* xc = sema_get_class(&sctx, want_cls);
                        if (xc && ms[k]->method_id >= 0
                            && ms[k]->method_id < (int)bbq_vec_len((void*)xc->methods)
                            && xc->methods[ms[k]->method_id].is_constructor)
                            sir_summarize(&cctx, k);
                    }
                }
            }
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
            /* BY CLASS, never by walk order — the §15 guards allocate their own exception
             * objects and the throw arm comes first (see find_new_of_class). */
            int cid = sema_find_class(&sctx, "C");
            CHECK(cid >= 0, "class C resolves");
            const sir_node_t* n0 = find_new_of_class(ms[mi]->entry, cid);
            CHECK(n0 != NULL, "the source's `new C()` is in the graph");
            if (n0) CHECK((int)cp_escape_of_expr(e, n0) == es[i].want, es[i].label);
            cp_free(e);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
        for (int i = 0; i < (int)(sizeof ea / sizeof ea[0]); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(ea[i].src, &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
            CHECK(mi >= 0, "the array test method compiled");
            if (mi < 0) continue;
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
            /* The wrapper by its CLASS (the per-width PrimArray overlay — the lattice owns
             * that mapping); the backing is the one NewArray, which no guard allocates. */
            int pac = lat_primarray_class(&sctx, SIR_DTINT);
            CHECK(pac >= 0, "the int PrimArray overlay class resolves");
            const sir_node_t* wrap = find_new_of_class(ms[mi]->entry, pac);
            const sir_node_t *back = NULL, *unused = NULL;
            collect_two(ms[mi]->entry, SIR_NEWARRAY, &back, &unused);
            CHECK(wrap != NULL, "the source allocates an array WRAPPER");
            CHECK(back != NULL, "the source allocates a BACKING store");
            if (wrap && back) {
                CHECK((int)cp_escape_of_expr(e, wrap) == ea[i].want_wrap, ea[i].label);
                CHECK((int)cp_escape_of_expr(e, back) == ea[i].want_back,
                      "…and the BACKING store's state follows the heap rule from the wrapper");
            }
            cp_free(e);
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 28. Spec §6 — the THROW rule, and §6's CPS advantage:
    //
    //       "A `throw` marks its exception object escaping ONLY IF it can leave the method —
    //        with CPS/`try_table` the catch continuation is *in the graph*, so a throw caught
    //        by an enclosing `try_table` in the same method does NOT escape (escape §5: 'kill
    //        only try-block-local refs'); an uncaught (re-thrown) one is ArgEscape via the
    //        method's exceptional exit."
    //
    //     HAND-BUILT SIR, deliberately — and this is the ONLY level that can own this pin.
    //     From Java source the rule is UNOBSERVABLE today: `throw new E()` also passes the
    //     exception to its own CONSTRUCTOR, and with no summary that ctor is a §7 bottom
    //     method, so the object is already ArgEscape before the throw rule is ever consulted.
    //     A source-level pin could therefore never go green even with the rule fully built —
    //     it would be testing the ctor, not the throw. (That subsumption is also why the jre
    //     census cannot move on this: measured, deleting the throw rule outright moves it by
    //     ZERO. That fact says the JRE does not exercise the rule; it does NOT say the rule is
    //     optional — the jre is not the only program this compiler will ever see.)
    //
    //     A hand-built method has no ctor call, so the throw is the ONLY escape source
    //     touching the object, and the rule is observable in isolation.
    {
        struct { int catch_cls_of; int want; const char* label; } tc[] = {
          /* 0: a handler in THIS method that CATCHES the thrown class. The catch continuation
           *    is in the graph, so the object never leaves — NoEscape. */
          { 0, CP_ESC_NONE,
            "an exception thrown and CAUGHT in the same method does not escape — the catch "
            "continuation is in the graph (§6's CPS advantage)" },
          /* 1: no handler at all — it leaves by the method's exceptional exit. */
          { -2, CP_ESC_ARG,
            "an UNCAUGHT throw is ArgEscape — it leaves by the method's exceptional exit" },
          /* 2: a handler for an UNRELATED class. It does not catch this throw, so the object
           *    still leaves. FAIL-CLOSED: a handler is not a licence on its own. */
          { 1, CP_ESC_ARG,
            "a throw whose enclosing handler catches an UNRELATED class still escapes "
            "(fail-closed — a try region is not a licence by itself)" },
        };
        for (int i = 0; i < (int)(sizeof tc / sizeof tc[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            /* Two unrelated user exception classes, so "catches it" vs "catches something
             * else" is a real JLS §11.2 subtype question, not a tautology. */
            build_program(
                "class E extends Exception { } class U extends Exception { } class T { }",
                &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            int e_id = sema_find_class(&sctx, "E");
            int u_id = sema_find_class(&sctx, "U");
            CHECK(e_id >= 0 && u_id >= 0, "the two exception classes resolve");

            /* try { t0 = new E(); throw t0; } catch (<cls> x) { return; } */
            sir_node_t* alloc = sir_new(&a, e_id);
            sir_node_t* thrw  = sir_throw(&a, sir_load_local(&a, 0, SIR_DTREF, NULL));
            sir_node_t* body  = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, e_id),
                                                alloc, thrw);
            sir_node_t* entry;
            if (tc[i].catch_cls_of == -2) {
                entry = body;                      /* no try region: exc stays NULL */
            } else {
                int cls = tc[i].catch_cls_of == 0 ? e_id : u_id;
                sir_node_t* handler = sir_exception_entry(&a, 1, cls, sir_return_void(&a));
                entry = sir_try_region(&a, handler, body);
                /* What the DDCG's patch_excepts would have stamped: the throw's exception
                 * continuation IS the region chain, carried ON the node (spec §1's second
                 * γ). The engine reads the graph — there is nothing else to feed it. */
                thrw->exc = entry;
            }
            sir_method_t* m = sir_method(&a, "f", 0, 0, 2, entry);

            cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
            CHECK(e != NULL, "the hand-built method builds");
            if (e) {
                CHECK((int)cp_escape_of_expr(e, alloc) == tc[i].want, tc[i].label);
                cp_free(e);
            }
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    // 29. Spec §7's BOTTOM GRAPH, made precise by §6: a call can only touch what it was
    //     HANDED (and what is reachable from that, or from a global). An object that is still
    //     NoEscape was handed to nobody, so the callee holds no reference to it and CANNOT have
    //     written its fields — its cells SURVIVE the call.
    //
    //     This is the substrate deliverable stage 1 deferred TO stage 4 (the ledger's §1 row:
    //     "an invoke's WIDE kill shadows ALL cells with ONE memory name … Lifting (b) needs the
    //     escape lattice (stage 4)"), and cp_node_heap said so itself: "without the escape
    //     lattice (stage 4) we cannot say it was not [passed to the callee]".
    //
    //     HAND-BUILT, and it has to be: the observable is a NoEscape object with a MUTABLE
    //     FIELD surviving a call, and from Java source no such object exists — everything with
    //     a Java-visible field has a constructor, hence is ArgEscape until stage 5. (The one
    //     ctor-less allocation, the §10.7 array, cannot show it either: an ArrayLoad never
    //     forwards a stored value, because §1 makes an array's cell MONOLITHIC — "each array
    //     as a single object" — so a store to a[j] says nothing about a[i].)
    //
    //     `o = new C; o.f = new D; h(); o.f` — o is never handed to h(), so h() cannot have
    //     written o.f, and the load must still see the D. Before the lift, ANY call flattened
    //     every mutable cell of every object to the catch-all and the D was lost.
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program(
            "class D { } class C { D f; } class T { static void h(){ } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int d_id = sema_find_class(&sctx, "D");
        int t_id = sema_find_class(&sctx, "T");
        CHECK(c_id >= 0 && d_id >= 0 && t_id >= 0, "C, D and T resolve");
        /* h()'s index, LOOKED UP — sema also synthesizes a default ctor into T's methods
         * (JLS §8.8.9), so it is not 0 just because h is the only thing in the source. */
        int h_idx = -1;
        const sema_class_t* tc = sema_get_class(&sctx, t_id);
        for (int k = 0; tc && k < (int)bbq_vec_len(tc->methods); k++)
            if (!strcmp(tc->methods[k].name, "h")) h_idx = k;
        CHECK(h_idx >= 0, "T.h() resolves");

        sir_node_t* alloc_c = sir_new(&a, c_id);
        sir_node_t* alloc_d = sir_new(&a, d_id);
        sir_node_t* load    = sir_get_field(&a, SIR_DTREF,
                                  sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                  c_id, 0);
        sir_node_t* ret     = sir_return(&a, load, SIR_DTREF);
        /* the CALL — an ExprEffect wrapping an invoke is CP_CELL_ALL: a wide kill of every
         * mutable cell. (There is no void sir_datatype_t; a void call is DtInt under an
         * is_void ExprEffect, which is what the DDCG emits.) */
        sir_node_t* call    = sir_expr_effect(&a,
                                  sir_invoke_static(&a, t_id, h_idx, NULL, 0, SIR_DTINT),
                                  1, ret);
        sir_node_t* store   = sir_put_field(&a, SIR_DTREF,
                                  sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                  c_id, 0, alloc_d, call);
        sir_node_t* entry   = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                              alloc_c, store);
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 1, entry);

        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        CHECK(e != NULL, "the hand-built surviving-cell method builds");
        if (e) {
            CHECK(cp_escape_of_expr(e, alloc_c) == CP_ESC_NONE,
                  "the object is NoEscape (hand-built: no ctor call to escape it)");
            cp_vnode_t* ld = NULL;
            for (int k = 0; k < e->vnode_count && !ld; k++)
                if (e->vnodes[k]->expr == load) ld = e->vnodes[k];
            CHECK(ld != NULL, "the load after the call is in the graph");
            if (ld) {
                CHECK(!cp_pts_has(e, ld->pts, CP_OBJ_EXT),
                      "a NoEscape object's cell SURVIVES a call — the callee was never handed "
                      "it, so §7's bottom graph says it cannot have written it");
                CHECK(cp_pts_count(e, ld->pts) > 0,
                      "…and the load still names what was stored, not nothing");
            }
            cp_free(e);
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    // 29. THE POST-CLICK SSA INVARIANT: no slot is READ that is never WRITTEN.
    //
    //     A LoadLocal whose slot has no StoreLocal anywhere in the final graph is
    //     reading an uninitialized local — a miscompile, full stop. It is also the
    //     shape a whole class of optimizer bugs collapses into, so pin the INVARIANT
    //     rather than any one symptom of it.
    //
    //     The bug this caught (07-13): cp_compute_reachability decides a Branch's live
    //     arms from cp_cond_const, which reads the condition's vnode out of expr_idx and
    //     answered TOP ("not yet decided") when the node was ABSENT. That is the right,
    //     optimistic reading DURING the solve (Click §4.4.1). But cp_rewrite runs the
    //     pass AGAIN afterwards, and the rewrites REPLACE condition nodes with fresh ones
    //     that are not in expr_idx — so a live branch read as TOP had BOTH arms marked
    //     unreachable. Liveness (which only visits reachable nodes) then never saw the
    //     LoadLocals below it, DSE deleted the StoreLocals feeding them, and codegen —
    //     which walks from entry and has never heard of reachable[] — emitted the loads
    //     anyway. A ref slot hit the assembler with no threaded descriptor (fail-loud,
    //     module rejected); an int slot just returned garbage (silent). "I have never
    //     seen this node" is BOTTOM, not TOP.
    //
    //     Only e2e caught it, which is a failure OF THIS SUITE: the invariant is
    //     structural and needs no execution. VERIFIED RED against the TOP reading.
    {
        static const char* srcs[] = {
            /* the ctor field store: b.v read twice, guards spill it to ref temps */
            "class B { char[] v; B(char[] a){ this.v = a; }"
            " static int f(){ char[] a = new char[3]; a[0]=65; a[1]=66; a[2]=67;"
            " B b = new B(a); return b.v.length * 100 + b.v[2]; } }",
            /* covariant ref-array store through a field */
            "class T { Object[] o; static int f(){ T t = new T();"
            " t.o = new String[2]; t.o[0] = \"x\"; return t.o.length; } }",
            /* a loop with a guard whose condition the rewrite rebuilds */
            "class T { static int f(int n){ int[] a = new int[n]; int s = 0;"
            " for (int i = 0; i < n; i++) { a[i] = i; s = s + a[i]; } return s; } }",
        };
        for (int i = 0; i < (int)(sizeof srcs / sizeof *srcs); i++) {
            bbq_arena arena; bbq_arena_init(&arena, 1 << 18);
            int nlib = 0;
            ast_program_t* prog = build_program(srcs[i], &arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            for (int mi = 0; mi < mc; mi++) {
                if (ms[mi]->class_id < nlib) continue;      /* skip the java.lang prelude */
                sir_optimize(&cctx, mi);                    /* Click + the slot pack */
                int sc = ms[mi]->max_locals > 0 ? ms[mi]->max_locals : 1;
                bool* def = (bool*)calloc((size_t)sc, sizeof(bool));
                bool* use = (bool*)calloc((size_t)sc, sizeof(bool));
                const sir_node_t** seen = NULL;
                collect_slot_defs_uses(ms[mi]->entry, def, use, sc, &seen);
                bbq_vec_free(seen);
                /* Params are defined by the caller, not by a StoreLocal. */
                const sema_class_t* cls = sema_get_class(&sctx, ms[mi]->class_id);
                const sema_method_t* sm = &cls->methods[ms[mi]->method_id];
                int params = ((sm->modifiers & ACC_STATIC) ? 0 : 1) + sm->param_count;
                for (int s = params; s < sc; s++) {
                    char msg[192];
                    snprintf(msg, sizeof msg,
                             "post-Click: %s.%s slot %d is READ but never WRITTEN "
                             "(a load of an uninitialized local — src %d)",
                             cls->name ? cls->name : "?",
                             ms[mi]->name ? ms[mi]->name : "?", s, i);
                    CHECK(!(use[s] && !def[s]), msg);
                }
                free(def); free(use);
            }
            sema_destroy(&sctx); bbq_arena_free(&arena);
        }
    }

    // 30. JLS §11.3.1 — EXCEPTIONS ARE PRECISE. The handler is a MERGE of every excepting
    //     point in its region, not a continuation of the region's ENTRY.
    //
    //       "when the transfer of control takes place, all effects of the statements
    //        executed and expressions evaluated BEFORE the point from which the exception
    //        is thrown must appear to have taken place."
    //
    //     The miscompile this pins (found 07-13, shipping): `x` is NonNull before the try;
    //     the try nulls it, then calls something that throws; the catch derefs `x`. We read
    //     the handler's reaching defs from the TRY-ENTRY state, proved `x` NonNull, and
    //     DELETED the NPE guard — so at runtime the deref would trap (uncatchable) instead
    //     of throwing NullPointerException. The §15 guard layer exists precisely so that no
    //     Java-visible exceptional condition ever reaches the VM as a trap.
    //
    //     The guard must SURVIVE: on the exceptional edge out of the call, `x` is null, so
    //     the handler's φ meets {null, "alive"} and nullability is Maybe. VERIFIED RED
    //     against the pre-R.2b compiler (it eliminated 1/1).
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 18);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class E {"
            "  static void boom(int n) { if (n == 0) throw new RuntimeException(); }"
            "  static int f(int n) {"
            /* A fresh allocation, NOT a string literal: since §3.10.5 a literal lowers to
             * new String(...).intern(), and intern() is a CALL whose result has unknown
             * nullness — which would silently destroy this probe's premise. */
            "    String x = new String(new char[5]);"  /* NonNull at try entry          */
            "    try { x = null; boom(n); }"           /* …null BEFORE a throwing call  */
            "    catch (RuntimeException e) { return x.length(); }"   /* must NPE at run time */
            "    return 7;"
            "  }"
            "}", &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "f")) mi = k;
        CHECK(mi >= 0, "the precise-exception probe method compiles");
        if (mi >= 0) {
            sir_optimize(&cctx, mi);
            int ng = 0;
            const compiler_fact_t* gs = guards_of(&cctx, mi, &ng);
            /* Select by SURVIVAL, not by position. The method carries a second NPE guard now:
             * `"alive"` lowers to new String(...).intern() (§3.10.5), whose receiver is a
             * fresh allocation and therefore provably NonNull, so that guard folds. Taking
             * the LAST guard picked up the folded one and read it as a regression.
             * What §11.3.1 asserts is that the catch block's x.length() guard is NOT folded,
             * so ask exactly that. If the handler merged the try-ENTRY state (x NonNull),
             * every NPE guard here would fold and none would remain a branch. */
            int npe = 0, npe_live = 0;
            for (int g = 0; g < ng; g++)
                if (gs[g].a == COMPILER_GUARD_NPE) {
                    npe++;
                    if (gs[g].key->tag == SIR_BRANCH) npe_live++;
                }
            CHECK(npe > 0, "the catch block's `x.length()` emitted an NPE guard");
            CHECK(npe_live > 0,
                  "JLS §11.3.1: the NPE guard in the CATCH block SURVIVES — the handler "
                  "merges the state at the excepting call, where x is null, not the "
                  "try-entry state where it was NonNull");
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // 31. COMPLETENESS OF THE EXCEPT RECORDING (spec §1 / JLS §11.1). Every excepting
    //     node — throw, call, allocation — minted INSIDE a try region must carry an
    //     EXCEPT_REGION row. The frontend is the ONLY authority for "can transfer to a
    //     handler"; a mint site that forgets to record is invisible to the optimizer and
    //     silently un-merges the handler (the E.f class of bug, reintroduced by omission).
    //     This makes that a test failure instead of a VM session.
    //
    //     By construction the ONE uncovered excepting node per method below is the
    //     catch-all's rethrow: it runs INSIDE the handler, is recorded against the PARENT
    //     region, and there is none. So: misses == 1, and the miss is a Throw.
    //     VERIFIED RED by disabling record_except_regions in build_invoke_node.
    {
        bbq_arena arena; bbq_arena_init(&arena, 1 << 18);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class Q { int k; Q() { k = 1; } int v() { return k; }"
            "  static int g(int n) { return n + 1; } }"
            "class T {"
            "  static int f(int n) {"                    /* every §11.1 kind, in one try */
            "    try {"
            "      int[] a = new int[n];"                /* NewArray + wrapper New       */
            "      Q q = new Q();"                       /* New + ctor InvokeSpecial     */
            "      int x = Q.g(n) + q.v() + a.length;"   /* InvokeStatic + InvokeVirtual */
            "      if (x == 0) throw new RuntimeException();"
            "      return x;"
            "    } catch (RuntimeException e) { return -1; }"
            "  }"
            "  static int h(int n) {"                    /* NESTED: one row per region   */
            "    try {"
            "      try { if (n == 0) throw new RuntimeException(); }"
            "      catch (ArithmeticException e) { return 1; }"
            "    } catch (RuntimeException e) { return 2; }"
            "    return 3;"
            "  }"
            "}", &arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &arena); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &arena, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);   /* NOT optimized: the pin
                                                                   * is on the RECORDING */
        int fi = -1, hi = -1;
        for (int k = 0; k < mc; k++) {
            if (ms[k]->class_id < nlib || !ms[k]->name) continue;
            if (!strcmp(ms[k]->name, "f")) fi = k;
            if (!strcmp(ms[k]->name, "h")) hi = k;
        }
        CHECK(fi >= 0 && hi >= 0, "the completeness probe methods compile");

        if (fi >= 0) {
            int nf = 0;
            const compiler_fact_t* f = compiler_get_facts(&cctx, fi, &nf);
            const sir_node_t** seen = NULL; const sir_node_t** exs = NULL;
            collect_excepting(ms[fi]->entry, &seen, &exs);
            int total = (int)bbq_vec_len(exs), misses = 0, miss_tag = -1;
            for (int i = 0; i < total; i++)
                if (except_rows_of(f, nf, exs[i], NULL) == 0) {
                    misses++;
                    miss_tag = exs[i]->tag;
                }
            CHECK(total >= 10, "the probe minted a real excepting population "
                               "(guards' throw arms included)");
            CHECK(misses == 1, "EVERY excepting node inside the try carries an "
                               "EXCEPT_REGION row — the one miss is the catch-all's "
                               "rethrow, which no region here encloses");
            CHECK(miss_tag == SIR_THROW, "…and that one miss IS the rethrow Throw");
            bbq_vec_free(seen); bbq_vec_free(exs);
        }

        if (hi >= 0) {
            int nf = 0;
            const compiler_fact_t* f = compiler_get_facts(&cctx, hi, &nf);
            const sir_node_t** seen = NULL; const sir_node_t** exs = NULL;
            collect_excepting(ms[hi]->entry, &seen, &exs);
            /* The user Throw sits under TWO regions ⟹ exactly one row per region. */
            int two_row_throws = 0;
            for (int i = 0; i < (int)bbq_vec_len(exs); i++) {
                if (exs[i]->tag != SIR_THROW) continue;
                if (except_rows_of(f, nf, exs[i], NULL) == 2) two_row_throws++;
            }
            CHECK(two_row_throws >= 1, "a throw under NESTED regions records one row "
                                       "per enclosing region (innermost first)");
            bbq_vec_free(seen); bbq_vec_free(exs);
        }
        sema_destroy(&sctx); bbq_arena_free(&arena);
    }

    // ── §32 spec §6's CONSUMER — scalar replacement ──────────────────────
    //
    //     "NoEscape ⟹ scalar-replace the struct.new — its fields become SSA values /
    //     LOCALS, zero GC allocation." The observable is the OPTIMIZED SIR: the New is
    //     GONE, and the value a field held still computes.
    //
    //     HAND-BUILT for the same reason §27 is: from Java source, every object with a
    //     field has a constructor, so §7's bottom graph makes it ArgEscape until stage 5.
    //     These pins are about the CONSUMER, not about that limitation.
    {
        // §32.1 the object with a stored-and-reloaded field: replaced, and the New dies.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");
        CHECK(c_id >= 0 && t_id >= 0, "§32: C and T resolve");

        /* o = new C; o.f = 7; return o.f;  ⟹ the New is removable and the answer is 7. */
        sir_node_t* alloc = sir_new(&a, c_id);
        sir_node_t* load  = sir_get_field(&a, SIR_DTINT,
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                c_id, 0);
        sir_node_t* ret   = sir_return(&a, load, SIR_DTINT);
        sir_node_t* store = sir_put_field(&a, SIR_DTINT,
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                c_id, 0, sir_load_const(&a, 7, SIR_DTINT), ret);
        sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            alloc, store);
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 1, entry);

        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        CHECK(e != NULL, "§32.1: the hand-built scalar-replacement method builds");
        if (e) {
            CHECK(cp_escape_of_expr(e, alloc) == CP_ESC_NONE,
                  "§32.1: the object is NoEscape (no ctor call to escape it)");
            cp_rewrite(e);
            CHECK(e->scalar_count == 1, "§32.1: the site QUALIFIES for scalar replacement");
            cp_free(e);
        }
        CHECK(count_tag(m->entry, SIR_NEW) == 0,
              "§32.1: spec §6 — the NoEscape struct.new is GONE from the optimized SIR");
        CHECK(count_tag(m->entry, SIR_GETFIELD) == 0
           && count_tag(m->entry, SIR_PUTFIELD) == 0,
              "§32.1: …and its field ops became LOCAL ops, not heap ops");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §32.2 JLS §4.12.5 — a field READ BEFORE ANY STORE reads the DEFAULT (0), not
        //       whatever the fresh slot happened to hold.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");

        /* o = new C; return o.f;  ⟹ 0 */
        sir_node_t* alloc = sir_new(&a, c_id);
        sir_node_t* load  = sir_get_field(&a, SIR_DTINT,
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                c_id, 0);
        sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            alloc, sir_return(&a, load, SIR_DTINT));
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 1, entry);
        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) { cp_rewrite(e);
                 CHECK(e->scalar_count == 1, "§32.2: the never-stored site qualifies");
                 cp_free(e); }
        CHECK(count_tag(m->entry, SIR_NEW) == 0, "§32.2: the New is gone");
        CHECK(count_tag(m->entry, SIR_GETFIELD) == 0,
              "§32.2: the field read became a local read");
        /* §4.12.5: the value returned is the field's slot, and that slot's def stores
         * the DEFAULT (0). (Not a LoadConst at the Return — the solve ran before the
         * slot existed, so no fold; the def-init chain is the observable.) */
        CHECK(retslot_defaults_to_zero(m->entry),
              "§32.2: JLS §4.12.5 — an unwritten field reads its DEFAULT (0): the "
              "returned slot is initialized with 0 AT the allocation");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §32.3 FAIL-CLOSED — the ref is COMPARED. Reference identity is observable, so
        //       the object must stay materialized (unknown ⟹ keep).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");

        /* o = new C; return (o == p) ? … — a ref compare against a PARAMETER (slot 1),
         * which the folder cannot decide, so the compare survives and observes o. */
        sir_node_t* alloc = sir_new(&a, c_id);
        sir_node_t* cmp   = sir_eq(&a,
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                sir_load_local(&a, 1, SIR_DTREF, sir_class_ref(&a, c_id)));
        sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            alloc, sir_return(&a, cmp, SIR_DTINT));
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 2, entry);
        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) { cp_rewrite(e);
                 CHECK(e->scalar_count == 0,
                       "§32.3: FAIL-CLOSED — a ref COMPARE observes identity, so the site "
                       "is declined");
                 cp_free(e); }
        CHECK(count_tag(m->entry, SIR_NEW) == 1,
              "§32.3: …and the allocation is still there");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §32.4 FAIL-CLOSED — the receiver's pts is not a singleton (two sites merge into
        //       one local), so no ONE site's slots can stand for the load.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");

        /* if (p) o = new C; else o = new C;  return o.f; — the load's receiver is a φ of
         * TWO sites: pts = {O1, O2}. */
        sir_node_t* a1 = sir_new(&a, c_id);
        sir_node_t* a2 = sir_new(&a, c_id);
        sir_node_t* load = sir_get_field(&a, SIR_DTINT,
                               sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                               c_id, 0);
        sir_node_t* join = sir_return(&a, load, SIR_DTINT);
        sir_node_t* t_arm = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            a1, join);
        sir_node_t* f_arm = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            a2, join);
        sir_node_t* entry = sir_branch(&a,
                                sir_load_local(&a, 1, SIR_DTINT, NULL), t_arm, f_arm);
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 2, entry);
        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) { cp_rewrite(e);
                 CHECK(e->scalar_count == 0,
                       "§32.4: FAIL-CLOSED — a load whose receiver may be EITHER of two "
                       "sites (pts not a singleton) declines both");
                 cp_free(e); }
        CHECK(count_tag(m->entry, SIR_NEW) == 2,
              "§32.4: …and both allocations survive");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §32.5 JLS §11.3.1 — a field written in the TRY, read in the CATCH. The write
        //       precedes the throw point, so the catch must see it (sentence one). Slots
        //       carry that for free — the pin exists so a later "clever" rewrite cannot
        //       quietly break the pairing. Hand-built on the §28 pattern (the DDCG's
        //       patch_excepts stamp is applied by hand: thrw->exc = the region).
        //
        //       t0 = new C; try { t0.f = 7; t1 = new E; throw t1; }
        //                   catch (E t2) { return t0.f; }
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program(
            "class C { int f; } class E extends Exception { } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int e_id = sema_find_class(&sctx, "E");
        CHECK(c_id >= 0 && e_id >= 0, "§32.5: C and E resolve");

        sir_node_t* alloc_c = sir_new(&a, c_id);
        sir_node_t* alloc_e = sir_new(&a, e_id);
        sir_node_t* handler = sir_exception_entry(&a, 2, e_id,
                                  sir_return(&a,
                                      sir_get_field(&a, SIR_DTINT,
                                          sir_load_local(&a, 0, SIR_DTREF,
                                                         sir_class_ref(&a, c_id)),
                                          c_id, 0),
                                      SIR_DTINT));
        sir_node_t* thrw  = sir_throw(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
        sir_node_t* spill = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, e_id),
                                            alloc_e, thrw);
        sir_node_t* put   = sir_put_field(&a, SIR_DTINT,
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                c_id, 0, sir_load_const(&a, 7, SIR_DTINT), spill);
        sir_node_t* region = sir_try_region(&a, handler, put);
        sir_node_t* entry  = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                             alloc_c, region);
        thrw->exc = region;      /* what the DDCG's patch_excepts would have stamped */
        sir_method_t* m = sir_method(&a, "g", 0, 0, 3, entry);

        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) { cp_rewrite(e);
                 CHECK(e->scalar_count == 1,
                       "§32.5: C's site qualifies (E's is thrown, hence declined)");
                 cp_free(e); }
        CHECK(count_tag(m->entry, SIR_NEW) == 1,
              "§32.5: C's New is gone; the thrown E's remains");
        /* Class-aware: a partial-escape materialization of E legitimately WRITES
         * E's (Throwable's) fields at the escape point — only C's ops must be
         * slot ops. */
        CHECK(count_field_ops_of_class(m->entry, c_id) == 0,
              "§32.5: both of C's field ops became slot ops");
        /* §11.3.1 sentence one: the try's `f = 7` precedes the throw point, so the catch
         * must read THAT value. Structurally: the store of 7 and the catch's returned
         * load name the SAME slot. */
        CHECK(try_write_reaches_catch_read(m->entry, 7),
              "§32.5: JLS §11.3.1 — the catch's read is the slot the try wrote 7 into");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §32.6 the SUMMARY site (the recorded ALLOC fact says the site can run more
        //       than once: a loop), BOTH halves of the §5.4 per-visit rule:
        //       (a) never-escaping, no name of the object live entering the def — the
        //           def re-runs each visit (LoadNull + §4.12.5 defaults re-initialize
        //           the slots: a per-visit RESET), so the slots stand for THE CURRENT
        //           visit's object only ⟹ fully virtualized;
        //       (b) FAIL-CLOSED — a name still live entering the def (a ref of the
        //           PREVIOUS visit's object read AFTER the reset) ⟹ the site declines.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");
        {   /* (a) o = new C; return o.f — summary-recorded, nothing live across. */
            sir_node_t* alloc = sir_new(&a, c_id);
            sir_node_t* load  = sir_get_field(&a, SIR_DTINT,
                                    sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                    c_id, 0);
            sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                                alloc, sir_return(&a, load, SIR_DTINT));
            sir_method_t* m = sir_method(&a, "g", t_id, 0, 1, entry);
            /* The DDCG's record: this site can run MORE THAN ONCE (payload a != 0). With
             * any fact recorded, the unit-harness "no DDCG ran" fallback does not fire
             * (R.3 BUG 2). */
            compiler_fact_t facts[1] = { { alloc, NULL, COMPILER_FACT_ALLOC, 1, 0, 0, 0 } };
            cp_engine_t* e = cp_build(m, &sctx, &a, facts, 1);
            if (e) { cp_rewrite(e);
                     CHECK(e->scalar_count == 1,
                           "§32.6a: a never-escaping SUMMARY site with no live-across "
                           "name IS replaced (§5.4 per-visit reset)");
                     cp_free(e); }
            CHECK(count_tag(m->entry, SIR_NEW) == 0,
                  "§32.6a: …and the allocation is gone");
        }
        {   /* (b) a REAL cycle (the copy must flow around the back edge to name the
             * object — test_cp_phi_at_loop_header's pattern):
             *     b = null;
             *     loop: o = new C;            <- the reset
             *           t = b.f;              <- the PREVIOUS visit's object, read
             *                                    AFTER the reset — would see the
             *                                    NEXT visit's slot state: UNSOUND
             *           o.f = 7;
             *           b = o;                <- the name carried across the edge
             * b (slot1) is live entering the def ⟹ the site declines. */
            sir_node_t* header = sir_nop(&a, NULL);
            sir_node_t* alloc  = sir_new(&a, c_id);
            sir_node_t* copy   = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, c_id),
                                    sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                    header);
            sir_node_t* put    = sir_put_field(&a, SIR_DTINT,
                                    sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)),
                                    c_id, 0, sir_load_const(&a, 7, SIR_DTINT), copy);
            sir_node_t* readb  = sir_store_local(&a, 2, SIR_DTINT, NULL,
                                    sir_get_field(&a, SIR_DTINT,
                                        sir_load_local(&a, 1, SIR_DTREF,
                                                       sir_class_ref(&a, c_id)),
                                        c_id, 0),
                                    put);
            sir_node_t* def    = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                                 alloc, readb);
            sir_set_next(header, def);
            sir_node_t* init   = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, c_id),
                                                 sir_load_null(&a), header);
            sir_node_t* entry  = sir_nop(&a, init);
            sir_method_t* m = sir_method(&a, "g", t_id, 0, 3, entry);
            compiler_fact_t facts[1] = { { alloc, NULL, COMPILER_FACT_ALLOC, 1, 0, 0, 0 } };
            cp_engine_t* e = cp_build(m, &sctx, &a, facts, 1);
            if (e) { cp_rewrite(e);
                     CHECK(e->scalar_count == 0,
                           "§32.6b: FAIL-CLOSED — a name live entering the def (the "
                           "previous visit's object read after the reset) declines");
                     cp_free(e); }
            CHECK(count_tag(m->entry, SIR_NEW) == 1,
                  "§32.6b: …and the allocation survives");
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // §32.7 FAIL-CLOSED — a site whose FIELD HAS NO NAMEABLE SLOT TYPE.
        //
        //     The rewrite types each new slot with the field's ref descriptor. The SIR's
        //     descriptor vocabulary is ClassRef | ArrayRef | PrimArray — every one names an
        //     OVERLAY — so it cannot name the overlay's own CONCRETE BACKING (`data`:
        //     JT_ARRAY_RAW for PrimArray, a JT_NULL element for RefArray). No nameable slot
        //     type ⟹ the site DECLINES.
        //
        //     THIS PIN EXISTS BECAUSE ITS RED STATE WAS 584 EXEC FAILURES. The descriptor
        //     re-overlaid the backing, so the rewrite typed a slot (ref $PrimArray) while it
        //     held (array i32), and the §7.6 validator rejected the jre outright. The jre's
        //     only ctor-less structs ARE these overlays, so this is the case that fires —
        //     and it was caught only end-to-end. Pin it at the level that owns it.
        //     (§1 forbids it independently: an array is monolithic, no fields to split.)
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int t_id = sema_find_class(&sctx, "T");

        /* The int PrimArray OVERLAY — sema-synthesized; its `data` field is the raw backing. */
        int pa = lat_primarray_class(&sctx, SIR_DTINT);
        CHECK(pa >= 0, "§32.7: the int PrimArray overlay class resolves");

        sir_node_t* alloc = sir_new(&a, pa);
        sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, pa),
                                            alloc, sir_return_void(&a));
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 1, entry);
        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) {
            CHECK(cp_escape_of_expr(e, alloc) == CP_ESC_NONE,
                  "§32.7: the overlay site IS NoEscape — so only the fail-closed decline stops it");
            cp_rewrite(e);
            CHECK(e->scalar_count == 0,
                  "§32.7: FAIL-CLOSED — a field the SIR cannot NAME (the overlay's raw "
                  "backing) declines the site; typing that slot as the overlay is what "
                  "made the jre type-invalid");
            cp_free(e);
        }
        CHECK(count_tag(m->entry, SIR_NEW) == 1,
              "§32.7: …and the allocation survives");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // ── §35 THE SPINE COLLECTOR — the ONE place continuation edges are followed ──────
        //
        //     "A linear scan of spine[] is not a traversal. Following a successor is."
        //     THAT is the line, and this is the one function allowed to cross it. Every
        //     other consumer reads the LIST it returns.
        //
        //     It existed TWICE (the engine's cp_collect_spine and cp_pack's own DFS) with
        //     NO test, so when the summary driver needed a spine it was cheaper to write a THIRD copy than
        //     to reuse one — which is exactly what happened, and exactly the economics that
        //     produce walkers. One authority, one pin.
        //
        //     The contract, and every clause of it is a way the naive version is wrong:
        //       - EVERY reachable spine node, exactly ONCE (a shared join is not two nodes);
        //       - a Branch's BOTH arms;
        //       - a TryRegion's HANDLER (sir_succ[0]) — "follow .next" silently loses every
        //         catch body in the program;
        //       - a loop BACK EDGE terminates instead of spinning;
        //       - and NOTHING from inside an expression tree (a StoreLocal's value is an
        //         operand, not a spine node — that is sir_arity/sir_child's job).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");

        /* A graph with every shape that breaks a naive walker:
         *
         *        entry(Nop) ──► branch ──true──► t_arm(StoreLocal) ─┐
         *             ▲            └──false──► f_arm(StoreLocal) ─┤
         *             │                                            ▼
         *             └────────────────── back(Branch) ◄──── join(Nop) ──► ret
         *
         * `join` is reached from BOTH arms (must appear once); `back` closes a loop to
         * `entry` (must terminate); a TryRegion hangs a handler off the join. */
        sir_node_t* ret   = sir_return(&a, sir_load_const(&a, 0, SIR_DTINT), SIR_DTINT);
        sir_node_t* hbody = sir_return(&a, sir_load_const(&a, 1, SIR_DTINT), SIR_DTINT);
        sir_node_t* hand  = sir_exception_entry(&a, 1, -1, hbody);
        sir_node_t* back  = sir_branch(&a, sir_load_const(&a, 0, SIR_DTINT), NULL, ret);
        sir_node_t* tr    = sir_try_region(&a, hand, back);
        sir_node_t* join  = sir_nop(&a, tr);
        sir_node_t* t_arm = sir_store_local(&a, 0, SIR_DTINT, NULL,
                                            sir_load_const(&a, 1, SIR_DTINT), join);
        sir_node_t* f_arm = sir_store_local(&a, 0, SIR_DTINT, NULL,
                                            sir_load_const(&a, 2, SIR_DTINT), join);
        sir_node_t* br    = sir_branch(&a, sir_load_const(&a, 1, SIR_DTINT), t_arm, f_arm);
        sir_node_t* entry = sir_nop(&a, br);
        back->branch.on_true = entry;                     /* the back edge */
        (void)c_id;

        sir_node_t** sp = sir_collect_spine(entry);
        int n = (int)bbq_vec_len(sp);

        int n_entry = 0, n_br = 0, n_t = 0, n_f = 0, n_join = 0, n_tr = 0,
            n_hand = 0, n_hbody = 0, n_back = 0, n_ret = 0, n_expr = 0;
        for (int i = 0; i < n; i++) {
            if (sp[i] == entry) n_entry++;
            else if (sp[i] == br)    n_br++;
            else if (sp[i] == t_arm) n_t++;
            else if (sp[i] == f_arm) n_f++;
            else if (sp[i] == join)  n_join++;
            else if (sp[i] == tr)    n_tr++;
            else if (sp[i] == hand)  n_hand++;
            else if (sp[i] == hbody) n_hbody++;
            else if (sp[i] == back)  n_back++;
            else if (sp[i] == ret)   n_ret++;
            else if (sp[i]->tag == SIR_LOADCONST) n_expr++;   /* must never happen */
        }
        CHECK(n_entry == 1 && n_br == 1 && n_back == 1 && n_ret == 1,
              "§35: every reachable spine node is collected");
        CHECK(n_t == 1 && n_f == 1,
              "§35: a Branch's BOTH arms are collected");
        CHECK(n_join == 1,
              "§35: a node reached from TWO predecessors appears EXACTLY ONCE — a shared "
              "join is one node, not two (dedup is on identity, not on path)");
        CHECK(n_tr == 1 && n_hand == 1 && n_hbody == 1,
              "§35: a TryRegion's HANDLER is a successor and is collected — 'follow .next' "
              "would silently lose every catch body in the program");
        CHECK(n_expr == 0,
              "§35: NOTHING from inside an expression tree — a StoreLocal's value is an "
              "operand (sir_arity/sir_child), never a spine node");
        CHECK(n == 10,
              "§35: …and the list is EXACTLY those 10 nodes — the back edge terminates "
              "rather than spinning, and nothing else is invented");
        bbq_vec_free(sp);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // ── §34 spec §7/§10 — THE DEFUNCTIONALIZED CALL GRAPH, as the compiler holds it ──
        //
        //     §10: the analysis "CONSUMES the lowered value graph + the defunctionalized
        //     call graph". test_sema pins the RULE (a call site's complete target set is
        //     enumerable from the class table alone) and test_wasm_types pins the emitted
        //     vtable rows. THIS pins the graph the compiler actually hands the summary driver: real Java
        //     source → compiler_compile → compiler_build_callgraph.
        //
        //     The property that makes stage 5 possible at all: the target set is COMPLETE
        //     and CLOSED before any analysis runs. A virtual site fans out to EVERY
        //     override; a static/special site names its callee outright (JLS §15.11 — it
        //     does not dispatch); a call with no compiled body (native / abstract / jre
        //     import) yields NO edge, which is §7's bottom-method boundary.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class A { int m(){ return 1; } }"
            " class B extends A { int m(){ return 2; } }"
            " class C extends B { }"
            " class T {"
            "   static int s(){ return 5; }"
            "   static int callvirt(A r){ return r.m(); }"
            "   static int callstatic(){ return s(); }"
            " }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        CHECK(ms != NULL && mc > 0, "§34: the program compiles");
        compiler_build_callgraph(&cctx);

        int a_id = sema_find_class(&sctx, "A"), b_id = sema_find_class(&sctx, "B"),
            t_id = sema_find_class(&sctx, "T");
        int cv = -1, cs = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "callvirt"))   cv = k;
                if (!strcmp(ms[k]->name, "callstatic")) cs = k;
            }
        CHECK(cv >= 0 && cs >= 0, "§34: the two call-site methods resolve");

        /* A VIRTUAL site fans out to its COMPLETE target set. `r.m()` on a declared A
         * reaches A.m and B.m — and C, which declares nothing, resolves to the INHERITED
         * B.m, so it adds no NEW edge. Exactly two m-targets: not one, and not three.
         *
         * `callvirt` has OTHER callees too, and they are real: `r.m()` carries a §15 NPE
         * guard whose throw arm does `new NullPointerException()` — an InvokeSpecial on
         * that class's ctor, which is a genuine call edge and must be in the graph (it is
         * exactly the kind of edge stage 5's ctor summary will consume). So assert on the
         * DISPATCH targets precisely, rather than on the total edge count. */
        int want_am = -1, want_bm = -1;
        for (int k = 0; k < mc; k++) {
            if (ms[k]->class_id == a_id && ms[k]->name && !strcmp(ms[k]->name, "m")) want_am = k;
            if (ms[k]->class_id == b_id && ms[k]->name && !strcmp(ms[k]->name, "m")) want_bm = k;
        }
        CHECK(want_am >= 0 && want_bm >= 0, "§34: A.m and B.m have compiled bodies");
        bool saw_am = false, saw_bm = false;
        int n_mtargets = 0;
        for (int k = 0; k < compiler_callee_count(&cctx, cv); k++) {
            int t = compiler_callee(&cctx, cv, k);
            if (t == want_am) { saw_am = true; n_mtargets++; }
            if (t == want_bm) { saw_bm = true; n_mtargets++; }
            /* no override of m may reach here except those two */
            if (t != want_am && t != want_bm && ms[t]->name && !strcmp(ms[t]->name, "m")
                && sema_ref_is_subtype(&sctx, ms[t]->class_id, a_id))
                n_mtargets = -100;
        }
        CHECK(saw_am && saw_bm && n_mtargets == 2,
              "§34: a VIRTUAL site's edges are its COMPLETE target set {A.m, B.m} — every "
              "override, deduped, and no other m (C inherits B.m and adds no edge)");

        /* A STATIC site names its callee outright — one edge, no fan-out. */
        int want_s = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "s")) want_s = k;
        CHECK(want_s >= 0, "§34: T.s has a compiled body");
        CHECK(compiler_callee_count(&cctx, cs) == 1
              && compiler_callee(&cctx, cs, 0) == want_s,
              "§34: a STATIC site names its callee outright — exactly one edge, no dispatch "
              "(JLS §15.11)");

        /* The map's fail-closed direction: a (class, method) with no compiled body is not
         * a node of this graph — that is §7's BOTTOM METHOD, and the driver must see -1. */
        CHECK(compiler_method_index(&cctx, a_id, 999) == -1,
              "§34: a callee with no compiled body is NOT a node — §7's bottom-method "
              "boundary, and it must be -1 rather than a silent 0");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // ── §38 spec §7 — MapsTo CONSUMER (Choi Fig 7): escape + §7.2 side-effects ──
        //
        //     Isolates the consumer with a MANUAL callee summary. A caller passes an array
        //     (no ctor call, so its only escape source is the f() call) to a static f, whose
        //     summary is set to CLEAN / ARG / GLOBAL — the array's escape must follow it
        //     (Fig 7: GlobalEscape propagates; CLEAN does not lower). This is the pin I
        //     skipped before, which let an unsound consumer reach e2e.
        struct { unsigned char param0; int want; const char* label; } mt[] = {
          { COMPILER_ESC_NONE,   CP_ESC_NONE,
            "§38: MapsTo — passing an array to a CLEAN param does NOT escape it" },
          { COMPILER_ESC_ARG,    CP_ESC_ARG,
            "§38: MapsTo — an ARG param escapes the actual as ArgEscape (bottom level)" },
          { COMPILER_ESC_GLOBAL, CP_ESC_GLOBAL,
            "§38: MapsTo — a GLOBAL param propagates GlobalEscape to the actual" },
        };
        for (int i = 0; i < (int)(sizeof mt / sizeof mt[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(
                "class T {"
                "  static void f(int[] p){}"
                "  static void g(){ int[] a = new int[4]; a[0] = 7; f(a); }"
                " }", &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int t_id = sema_find_class(&sctx, "T");
            int i_f = -1, i_g = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id == t_id && ms[k]->name) {
                    if (!strcmp(ms[k]->name, "f")) i_f = k;
                    if (!strcmp(ms[k]->name, "g")) i_g = k;
                }
            CHECK(i_f >= 0 && i_g >= 0, "§38: f and g resolve");
            cctx.summaries = (compiler_summary_t*)bbq_arena_alloc(&a,
                                 (size_t)(mc > 0 ? mc : 1) * sizeof(compiler_summary_t));
            memset(cctx.summaries, 0, (size_t)(mc > 0 ? mc : 1) * sizeof(compiler_summary_t));
            unsigned char* se = (unsigned char*)bbq_arena_alloc(&a, 1);
            se[0] = mt[i].param0;
            cctx.summaries[i_f].computed    = true;
            cctx.summaries[i_f].this_escape = COMPILER_ESC_NA;
            cctx.summaries[i_f].ret_escape  = COMPILER_ESC_NA;
            cctx.summaries[i_f].slot_count  = 1;
            cctx.summaries[i_f].slot_escape = se;
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, i_g, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i_g], sc, nsc);
            int pac = lat_primarray_class(&sctx, SIR_DTINT);
            const sir_node_t* wrap = find_new_of_class(ms[i_g]->entry, pac);
            CHECK(e != NULL && wrap != NULL, "§38: the int[] wrapper is in g's graph");
            if (e && wrap)
                CHECK((int)cp_escape_of_expr(e, wrap) == mt[i].want, mt[i].label);
            if (e) cp_free(e);
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    {
        // ── §37 spec §7 — THE PER-METHOD ESCAPE SUMMARY (Choi §4.2) ────────────────
        //
        //     §7: "Per method f, the summary is: escape — which formals/return/globals
        //     escape." Produced as a READOUT of the SOLVED escape lattice (no mutation), the
        //     same domain the census at wasm_module already reads. A CALLER consumes it
        //     (the MapsTo mapping) instead of §7's bottom graph.
        //
        //     The distinction the summary carries (spec §6/§7, Choi Fig 7): a formal that is
        //     GlobalEscape in the callee → the caller's actual escapes; a formal at ArgEscape
        //     is the neutral seed and does NOT. This pin asserts the readout DISTINGUISHES
        //     those two — a param leaked to a static vs a param merely used.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int f; }"
            " class T {"
            "   static C S;"
            "   static void leak(C p){ S = p; }"    /* p → static ⟹ GlobalEscape */
            "   static void keep(C p){ p.f = 5; }"  /* p used, not leaked ⟹ ArgEscape (seed) */
            " }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; cctx.optimize = true;   /* the summary is a readout of the solve */
        compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        CHECK(ms != NULL && mc > 0, "§37: the program compiles");
        int t_id = sema_find_class(&sctx, "T");
        int i_leak = -1, i_keep = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "leak")) i_leak = k;
                if (!strcmp(ms[k]->name, "keep")) i_keep = k;
            }
        CHECK(i_leak >= 0 && i_keep >= 0, "§37: leak and keep resolve");

        /* Producing the summary is a readout of the solve — sir_optimize runs it. */
        sir_optimize(&cctx, i_leak);
        sir_optimize(&cctx, i_keep);

        const compiler_summary_t* s_leak = compiler_method_summary(&cctx, i_leak);
        const compiler_summary_t* s_keep = compiler_method_summary(&cctx, i_keep);
        CHECK(s_leak && s_leak->computed, "§37: leak has a computed summary");
        CHECK(s_keep && s_keep->computed, "§37: keep has a computed summary");

        /* Formal 0 (p) is a ref in slot 0 of these static methods. The summary carries the
         * CLASSIFICATION (how the callee escapes the formal), NOT the seed-polluted intra
         * escape state: leak stores p into a static ⟹ GLOBAL; keep only writes p's OWN field
         * (p is the receiver, not the stored value) ⟹ CLEAN. A caller must treat CLEAN as
         * not-escaping — that is the §27 win, and the whole reason the summary exists. */
        if (s_leak && s_leak->slot_escape && s_leak->slot_count > 0)
            CHECK(s_leak->slot_escape[0] == COMPILER_ESC_GLOBAL,
                  "§37: leak's formal 0 is GLOBAL — it is stored into a static");
        if (s_keep && s_keep->slot_escape && s_keep->slot_count > 0)
            CHECK(s_keep->slot_escape[0] == COMPILER_ESC_NONE,
                  "§37: keep's formal 0 is CLEAN — used as a receiver but never leaked "
                  "(not stored-as-value / returned / thrown / passed to a leaking callee)");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // ── §37c spec §8.1.1 — THE PUBLISHED FACTS EQUAL THE SOLVE ─────────────────
        //
        //     Click §4.10 applies results by walking the solved partitioning, which is only
        //     possible if the solve's facts outlive the engine. cp_publish_facts copies them
        //     into the context; this pins that the copy is FAITHFUL — every vnode's partition,
        //     leader, constant, type and kind, every object's escape state, and the rebuilt
        //     node index, equal what a live engine over the same graph reports. A publisher
        //     that merely allocates the right shape passes nothing here.
        //
        //     Compared against sir_summarize, which publishes WITHOUT rewriting: the facts are
        //     a function of the analysis (Choi §4.2), so the comparison engine must see the
        //     same graph the analysis saw.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int f; }"
            " class T {"
            "   static C S;"
            "   static int m(C p, int k){ int t = k + 1; C q = new C(); q.f = t;"
            "                             if (k > 3) { S = q; } return q.f + t; }"
            " }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; cctx.optimize = true;
        compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_m = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "m")) i_m = k;
        CHECK(ms != NULL && i_m >= 0, "§37c: the program compiles and m resolves");

        sir_summarize(&cctx, i_m);      /* publishes; never mutates the SIR */
        const struct compiler_click_facts* pf = compiler_click_facts_of(&cctx, i_m);
        CHECK(pf != NULL, "§37c: m's facts were published");

        int nf = 0;
        const compiler_fact_t* fv = compiler_get_facts(&cctx, i_m, &nf);
        cp_engine_t* live = cp_build_ctx(&cctx, ms[i_m], fv, nf);
        CHECK(live != NULL, "§37c: a live engine builds over the same graph");
        if (pf && live) {
            CHECK(pf->vnode_count == live->vnode_count,
                  "§37c: published vnode_count matches the solve");
            CHECK(pf->obj_count == live->obj_count && pf->obj_words == live->obj_words
                  && pf->obj_first_site == live->obj_first_site,
                  "§37c: published object model matches the solve");
            bool all = (pf->vnode_count == live->vnode_count);
            int bad = -1;
            for (int k = 0; all && k < live->vnode_count; k++) {
                const cp_vnode_t* vn = live->vnodes[k];
                if (!vn) continue;
                /* τ̂ is compared by CONTENT: each pool hash-conses independently, so the
                 * published type is a different pointer to the same lattice element. */
                bool ty = (pf->v[k].type == NULL) == (vn->type == NULL);
                if (ty && vn->type) {
                    ty = pf->v[k].type->kind == vn->type->kind;
                    if (ty) switch (vn->type->kind) {
                        case TK_PRIM: ty = pf->v[k].type->prim.width == vn->type->prim.width; break;
                        case TK_REF:  ty = pf->v[k].type->ref.class_id == vn->type->ref.class_id; break;
                        case TK_ARRAY: ty = pf->v[k].type->array.dim == vn->type->array.dim
                                         && pf->v[k].type->array.class_id == vn->type->array.class_id; break;
                        case TK_PRIM_ARRAY: ty = pf->v[k].type->prim_array.dim == vn->type->prim_array.dim
                                              && pf->v[k].type->prim_array.width == vn->type->prim_array.width; break;
                        default: break;   /* TOP/BOTTOM/NULL carry no payload */
                    }
                }
                if (pf->v[k].partition != vn->partition
                    || pf->v[k].leader != vn->leader
                    || pf->v[k].kind != (uint8_t)vn->kind
                    || !ty
                    || pf->v[k].constant.state != vn->constant.state
                    || pf->v[k].constant.value != vn->constant.value) { all = false; bad = k; }
            }
            if (!all) printf("  §37c detail: first mismatching vnode = %d\n", bad);
            CHECK(all, "§37c: every published vnode fact (partition/leader/kind/type/"
                       "constant) equals the live solve's");
            bool esc_ok = (pf->obj_count == live->obj_count);
            for (int o = 0; esc_ok && o < live->obj_count; o++)
                if (pf->obj_escape[o] != (uint8_t)cp_escape_of(live, o)) esc_ok = false;
            CHECK(esc_ok, "§37c: every published object's escape state equals the solve's");
            bool idx_ok = true;
            for (int k = 0; idx_ok && k < live->vnode_count; k++) {
                const cp_vnode_t* vn = live->vnodes[k];
                if (!vn || vn->kind != CP_VN_EXPR || !vn->expr) continue;
                if (compiler_click_vnode_of(pf, vn->expr) != k) idx_ok = false;
            }
            CHECK(idx_ok, "§37c: the rebuilt SIR-node→vnode index resolves every EXPR node");
            cp_free(live);
        }
        /* ── §37d spec §8.1.1 — A LOADED ENGINE EQUALS A SOLVED ONE ────────────────
         *
         *   Click §4.10 applies results by walking the solved partitioning, so the engine the
         *   application runs on must hold exactly what the analysis concluded. cp_build_ctx_loaded
         *   rebuilds the graph and restores the published facts instead of re-solving; this
         *   compares it field-for-field against a fully solved engine over the same method.
         *
         *   Any divergence means the published set is INCOMPLETE — a solved fact the
         *   application reads that nobody published. It names the field and the vnode so the
         *   missing fact can be added, which is the fix; the design is not in question.
         */
        /* Over EVERY method, not just one: the field that mattered (`heap`) only appeared on
         * memory-state nodes, and the summary-ordering defect only showed on methods with
         * callees. A single-method pin would have passed through both. */
        int built = 0; const char* diverged = NULL; int at = -1, in_m = -1;
        for (int mi = 0; mi < mc; mi++) {
            if (!ms[mi] || !ms[mi]->entry || diverged) continue;
            sir_summarize(&cctx, mi);
            int nf2 = 0;
            const compiler_fact_t* fv2 = compiler_get_facts(&cctx, mi, &nf2);
            const struct compiler_click_facts* pf2 = compiler_click_facts_of(&cctx, mi);
            if (!pf2) continue;
            /* The loaded engine lives in its own arena — the method lifetime — so the
             * comparison owns and releases one, exactly as sir_optimize does. */
            bbq_arena marena; bbq_arena_init(&marena, 1 << 18);
            cp_engine_t* L = cp_build_ctx_loaded(&cctx, ms[mi], fv2, nf2, pf2, &marena);
            cp_engine_t* S = cp_build_ctx(&cctx, ms[mi], fv2, nf2);
            if (!L || !S) { diverged = "engine-build"; in_m = mi;
                            if (L) cp_free(L); if (S) cp_free(S);
                            bbq_arena_free(&marena); continue; }
            built++;
            if (L->vnode_count != S->vnode_count || L->obj_count != S->obj_count
                || L->obj_words != S->obj_words || L->spine_count != S->spine_count) {
                diverged = "shape"; in_m = mi;
            }
            int nwv = S->obj_words;
            for (int k = 0; !diverged && k < S->vnode_count; k++) {
                const cp_vnode_t* a = L->vnodes[k]; const cp_vnode_t* b = S->vnodes[k];
                if (!a || !b) continue;
                if (a->partition != b->partition)        { diverged = "partition"; at = k; in_m = mi; }
                else if (a->leader != b->leader)         { diverged = "leader";    at = k; in_m = mi; }
                else if (a->constant.state != b->constant.state
                      || a->constant.value != b->constant.value) { diverged = "constant"; at = k; in_m = mi; }
                else if ((a->pts.bits == NULL) != (b->pts.bits == NULL)) { diverged = "pts(presence)"; at = k; in_m = mi; }
                else if (a->pts.bits && b->pts.bits
                         && memcmp(a->pts.bits, b->pts.bits, (size_t)nwv * sizeof(uint64_t)))
                                                         { diverged = "pts(bits)"; at = k; in_m = mi; }
                /* `heap` is deliberately NOT compared: it is analysis-internal. Every reader
                 * (cp_update_heap, cp_mark_bottom, cp_follow_field, cp_node_pts,
                 * cp_summary_differ) is an analysis transfer between lines 5049-6020; nothing
                 * in the rewrite tree touches it. Publishing it cost 1.4 GB and changed no
                 * output. The scope here is "facts the APPLICATION reads" — §8.1.1. */
            }
            for (int o = 0; !diverged && o < S->obj_count; o++)
                if (cp_escape_of(L, o) != cp_escape_of(S, o)) { diverged = "escape"; at = o; in_m = mi; }
            for (int s = 0; !diverged && s < S->spine_count; s++)
                if ((L->reachable ? L->reachable[s] : false)
                    != (S->reachable ? S->reachable[s] : false)) { diverged = "reachable"; at = s; in_m = mi; }
            cp_free(L); cp_free(S);
            bbq_arena_free(&marena);
        }
        CHECK(built > 1, "§37d: more than one method was compared (a single-method pin would "
                         "miss `heap`, which only exists on memory-state nodes)");
        if (diverged)
            printf("  §37d detail: method %d (%s.%s), field %s at %d\n", in_m,
                   (in_m >= 0 && ms[in_m] && ms[in_m]->class_id >= 0
                    && sema_get_class(&sctx, ms[in_m]->class_id)
                    && sema_get_class(&sctx, ms[in_m]->class_id)->name)
                       ? sema_get_class(&sctx, ms[in_m]->class_id)->name : "?",
                   (in_m >= 0 && ms[in_m] && ms[in_m]->name) ? ms[in_m]->name : "?",
                   diverged, at);
        CHECK(diverged == NULL,
              "§37d: over EVERY method, a loaded engine equals a solved one — every solved "
              "fact the application reads survives the round trip (a divergence NAMES the "
              "unpublished fact)");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §37b THE TRANSITIVE CASE — a formal stored into a LOCAL that then escapes must NOT
        //      read CLEAN. A one-pass "is the store's receiver already escaping" scan misses
        //      this (the receiver escapes LATER); only the escape FIXPOINT catches it. This
        //      is the bug that a §38-passing consumer turned into a 3-test e2e miscompile —
        //      pinned at the producer where it belongs.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { C n; }"
            " class T {"
            "   static C S;"
            /* p stored into c.n; c THEN stored into a static ⟹ p reaches global TRANSITIVELY */
            "   static void sink(C p){ C c = new C(); c.n = p; S = c; }"
            " }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_sink = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "sink")) i_sink = k;
        CHECK(i_sink >= 0, "§37b: sink resolves");
        sir_optimize(&cctx, i_sink);
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_sink);
        CHECK(s && s->computed, "§37b: sink has a computed summary");
        if (s && s->slot_escape && s->slot_count > 0)
            CHECK(s->slot_escape[0] != COMPILER_ESC_NONE,
                  "§37b: a formal stored into a local that LATER escapes is NOT CLEAN — the "
                  "escape fixpoint catches the transitive reach a one-pass scan misses");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §37c §7.2 SIDE-EFFECTED CELLS (producer): a method that writes a formal's field
        //      records (formal, cell) in its summary — the write set a caller kills. This is
        //      the half whose deferral caused the ctor field-store miscompile.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int v; int w; }"
            " class T { static void wr(C p){ p.v = 5; } }",   /* writes p.v, NOT p.w */
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_wr = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "wr")) i_wr = k;
        CHECK(i_wr >= 0, "§37c: wr resolves");
        sir_optimize(&cctx, i_wr);
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_wr);
        CHECK(s && s->computed, "§37c: wr has a computed summary");
        /* Exactly one written cell on parameter 0 (p) — C.v, never p.w. The sub-graph's
         * `wcell` (parameter-indexed via slot_obj[0]) is the one record of side-effected cells. */
        int p_sid = (s && s->slot_obj && s->slot_count > 0) ? s->slot_obj[0] : -1;
        int n_on_p = (s && p_sid >= 0 && s->wcell_off)
                     ? s->wcell_off[p_sid + 1] - s->wcell_off[p_sid] : -1;
        CHECK(n_on_p == 1,
              "§37c: wr's summary records ONE side-effected cell on parameter 0 — the caller "
              "kills exactly p.v (not p.w), so a ctor receiver's written field is not stale");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §37d DEEP WRITE — Fig 7's field-edge (Choi §4.4). A formal written BELOW its own
        //      fields (`p.child.x`, not `p.x`) does NOT make the formal escape: `p` is only
        //      READ, so it stays CLEAN. Soundness comes from the caller FOLLOWING the field
        //      `O.f ↦ Ô.g` and clobbering the reachable object's written cell — NOT from
        //      downgrading `p` to ARG (the conservative hack this replaces, which killed the
        //      scalar-replacement of `o` for no reason). Two halves, both pinned:
        //        (producer) `p` is CLEAN, and f's summary records the deep write on a NON-root
        //                   object reached via the `child` edge;
        //        (consumer) a caller that does `o.child.x = 9; f(o); return o.child.x;` must NOT
        //                   fold the return to 9 — f wrote 5 through `o.child`, so the clobber
        //                   makes the post-call value unknown.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int x; C child; }"
            " class T {"
            "   static void f(C p){ p.child.x = 5; }"
            "   static int g(){ C o = new C(); o.child = new C(); o.child.x = 9;"
            "                   f(o); return o.child.x; }"
            " }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_f = -1, i_g = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "f")) i_f = k;
                if (!strcmp(ms[k]->name, "g")) i_g = k;
            }
        CHECK(i_f >= 0 && i_g >= 0, "§37d: f and g resolve");
        sir_optimize(&cctx, i_f);       /* produce f's summary (consumed when g is analyzed) */
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_f);
        CHECK(s && s->computed, "§37d: f has a computed summary");
        /* PRODUCER: `p` is CLEAN — a deep write does not escape the formal. */
        if (s && s->slot_escape && s->slot_count > 0)
            CHECK(s->slot_escape[0] == COMPILER_ESC_NONE,
                  "§37d: a formal written only DEEP (p.child.x, never p itself) stays CLEAN — "
                  "the field-edge, NOT a conservative ARG downgrade");
        /* PRODUCER: the deep write is recorded on a NON-root summary object, reached by an edge. */
        bool deep_recorded = false;
        if (s && s->n_obj > 0) {
            int p_sid = (s->slot_obj && s->slot_count > 0) ? s->slot_obj[0] : -1;
            for (int k = 0; k < s->n_obj; k++)
                if (k != p_sid && k != s->this_obj && s->wcell_off
                    && s->wcell_off[k] < s->wcell_off[k + 1]) deep_recorded = true;
        }
        CHECK(deep_recorded,
              "§37d: f's summary records the deep write on a reachable (non-root) object — the "
              "NonLocalGraph a caller follows via O.f ↦ Ô.g");
        /* CONSUMER: g must not fold the stale pre-call 9 — the clobber invalidates o.child.x. */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_g, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_g], sc, nsc);
        CHECK(e != NULL, "§37d: g's engine builds");
        const sir_node_t* rt = first_spine_node(ms[i_g]->entry, SIR_RETURN);
        CHECK(rt && rt->return_.value, "§37d: g returns o.child.x");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state != CP_C_KNOWN,
                  "§37d: o.child.x is NOT a known constant after f(o) — the field-following "
                  "clobber (Fig 7) invalidated the pre-call 9 that f overwrote to 5");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §37e DEEP GlobalEscape (Fig 7 §4.4) + its negative control. Only GlobalEscape
        //      propagates to a MapsTo image. POSITIVE: a callee that stores a formal's field
        //      `p.f` into a STATIC makes the caller's `o.f` object GlobalEscape — the caller's
        //      reachable object genuinely leaks globally (the soundness half the direct-only
        //      §7.2 producer missed). NEGATIVE (anti-over-approximation): a callee that only
        //      READS `p.f` and drops it leaves `o.f` NoEscape — the field-follow must not
        //      spuriously escalate. Both pinned so a census swing is attributable to REAL escape.
        struct { const char* leaker; const char* caller; int want; const char* label; } dg[] = {
          { "static void act(C p){ s = p.f; }",   "leak",
            (int)CP_ESC_GLOBAL,
            "§37e: a callee storing p.f to a static makes the caller's o.f GlobalEscape (deep)" },
          { "static void act(C p){ D x = p.f; if (x == null) return; }", "noleak",
            (int)CP_ESC_ARG,
            "§37e: a callee that only READS p.f does NOT escalate o.f to GlobalEscape — it stays "
            "ArgEscape (from its ctor); the field-follow adds no spurious global escape" },
        };
        for (int i = 0; i < (int)(sizeof dg / sizeof dg[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            char src[512];
            snprintf(src, sizeof src,
                "class D {} class C { D f; }"
                " class G { static D s;"
                "   %s"
                "   static void run(){ C o = new C(); o.f = new D(); act(o); } }",
                dg[i].leaker);
            ast_program_t* prog = build_program(src, &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int g_id = sema_find_class(&sctx, "G");
            int i_act = -1, i_run = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id == g_id && ms[k]->name) {
                    if (!strcmp(ms[k]->name, "act")) i_act = k;
                    if (!strcmp(ms[k]->name, "run")) i_run = k;
                }
            CHECK(i_act >= 0 && i_run >= 0, "§37e: act and run resolve");
            sir_optimize(&cctx, i_act);      /* produce act's summary */
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
            int d_id = sema_find_class(&sctx, "D");
            const sir_node_t* nd = e ? find_new_of_class(ms[i_run]->entry, d_id) : NULL;
            CHECK(e && nd, "§37e: the new D() (o.f) is in run's graph");
            if (e && nd)
                CHECK((int)cp_escape_of_expr(e, nd) == dg[i].want, dg[i].label);
            if (e) cp_free(e);
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }
    {
        // §37f spec §7.2 — THE WRITE SET IS TRANSITIVE. `f(p){ g(p); }` writes p.r because its
        //      CALLEE g does; a summary built from f's own stores alone exports "writes nothing",
        //      and the caller's memory kill then PRESERVES a NoEscape receiver's cell across the
        //      call. The bite is the PTS shape (stale VALUE folds are blocked regardless — the
        //      store→load forwarder demands the store BE the load's memory input, and the call's
        //      kill vnode interposes; but a surviving kill row preserves pts): post-call,
        //      pts(o.r) still reads {d} with NO ⊥null while g wrote null at runtime — nullability
        //      proves NonNull and a null test folds the wrong way. NO escape-lattice symptom
        //      (everything is CLEAN; that is what makes it invisible). The engine already computed
        //      the transitive set (MapsTo marks the clobber matrix during f's solve); the summary
        //      must READ IT OUT, not rescan.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class D { int x; }"
            " class C { D r; }"
            " class T {"
            "   static void g(C q){ q.r = null; }"
            "   static void f(C p){ g(p); }"
            "   static int h(){ C o = new C(); o.r = new D(); f(o);"
            "                   if (o.r == null) return 1; return 2; } }",
            &a, &nlib);
  /* WHOLE-program: these cases summarize the java.lang ctor chain (§7), so the
   * prelude bodies must be compiled. */
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0 */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int c_id = sema_find_class(&sctx, "C"), t_id = sema_find_class(&sctx, "T");
        const sema_class_t* csc = sema_get_class(&sctx, c_id);
        int obj_id = csc ? csc->super_id : -1;
        int i_g = -1, i_f = -1, i_h = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "g")) i_g = k;
                if (!strcmp(ms[k]->name, "f")) i_f = k;
                if (!strcmp(ms[k]->name, "h")) i_h = k;
            }
        CHECK(i_g >= 0 && i_f >= 0 && i_h >= 0, "§37f: g, f, h resolve");
        /* Reverse-topological by hand: ctor chain (supers first) so o is NoEscape, then g
         * (the writer), then f (which must IMPORT g's write through the clobber matrix). */
        for (int pass = 0; pass < 2; pass++) {
            int want = (pass == 0) ? obj_id : c_id;
            for (int k = 0; k < mc; k++) {
                if (ms[k]->class_id != want) continue;
                const sema_class_t* sc = sema_get_class(&sctx, want);
                if (sc && ms[k]->method_id >= 0
                    && ms[k]->method_id < (int)bbq_vec_len((void*)sc->methods)
                    && sc->methods[ms[k]->method_id].is_constructor)
                    sir_summarize(&cctx, k);
            }
        }
        sir_summarize(&cctx, i_g);
        sir_summarize(&cctx, i_f);
        /* PRODUCER: f's summary must carry the write on its parameter 0 even though f's own
         * body stores nothing — the transitive set, read out of the clobber matrix. */
        const compiler_summary_t* sf = compiler_method_summary(&cctx, i_f);
        int p_sid = (sf && sf->slot_obj && sf->slot_count > 0) ? sf->slot_obj[0] : -1;
        int n_on_p = (sf && p_sid >= 0 && sf->wcell_off)
                     ? sf->wcell_off[p_sid + 1] - sf->wcell_off[p_sid] : 0;
        CHECK(n_on_p >= 1,
              "§37f (producer): f's summary records the write on parameter 0 that its CALLEE g "
              "performs — the transitive §7.2 set, not a rescan of f's own stores");
        int nsc = 0;
        const compiler_fact_t* sc2 = compiler_get_facts(&cctx, i_h, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_h], sc2, nsc);
        const sir_node_t* onew = e ? find_new_of_class(ms[i_h]->entry, c_id) : NULL;
        const sir_node_t* getr = e ? find_tag(ms[i_h]->entry, SIR_GETFIELD, 400) : NULL;
        CHECK(e && onew && getr, "§37f: h's new C() and the o.r read are in the graph");
        if (e && onew) {
            /* The control that gives the consumer check its teeth: o IS NoEscape (CLEAN chain
             * all the way), so the memory kill preserves its rows — ONLY the transitive clobber
             * can invalidate them. If this reads ArgEscape the test is vacuous, not passing. */
            CHECK(cp_escape_of_expr(e, onew) == CP_ESC_NONE,
                  "§37f: o is NoEscape through the CLEAN chain — the control that makes the "
                  "consumer check meaningful");
        }
        if (e && getr) {
            /* CONSUMER: post-call pts(o.r) must admit ⊥null — g wrote null at runtime. Without
             * the transitive export the preserved row reads {d} sans null, nullability proves
             * NonNull, and `o.r == null` folds the wrong way. */
            cp_pts_t rp = cp_pts_of_expr(e, getr);
            CHECK(cp_pts_has(e, rp, CP_OBJ_NULL),
                  "§37f (consumer): after f(o), pts(o.r) admits ⊥null — the clobber from f's "
                  "transitive write set wiped the preserved row, so the null test cannot fold");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §37g spec §7 — `this` IS the receiver SLOT, one object. The DDCG compiles a `this.f`
        //      receiver as LoadLocal(0), so `this` and slot 0 must denote ONE abstract object;
        //      when they were two, the summary read this_escape / this_obj off an obj_this a
        //      compiled body never references (spuriously NoEscape, empty write set), and a
        //      field-initializer ctor's `this.v = 7` landed on the slot-0 phantom instead —
        //      making the ctor look like a no-op that scalar replacement then dropped (the field-init exec
        //      miscompile). This pins the OWNING level: a field-init ctor's summary records the
        //      write on this_obj; an initializer-free ctor's stays empty; and a ctor that leaks
        //      `this` reports a real (non-NoEscape) this_escape. RED before the unification
        //      (the store-scan proved wcells on this_obj == 0 for every ctor).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class W { int v = 7; }"            /* synthetic-default ctor WITH a field init  */
            " class E { int v; }"              /* synthetic-default ctor, NO initializer     */
            " class L { static L sink;"        /* leaks `this` to a static in its ctor       */
            "           L(){ sink = this; } }"
            " class T { static int use(){ W w = new W(); E e = new E(); return w.v + e.v; } }",
            &a, &nlib);
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int w_id = sema_find_class(&sctx, "W"), e_id = sema_find_class(&sctx, "E"),
            l_id = sema_find_class(&sctx, "L");
        /* Summarize each class's ctor (its super, Object, is a no-op — supers-first is moot). */
        int i_wc = -1, i_ec = -1, i_lc = -1;
        for (int k = 0; k < mc; k++) {
            const sema_class_t* sc = sema_get_class(&sctx, ms[k]->class_id);
            if (!sc || ms[k]->method_id < 0 || ms[k]->method_id >= (int)bbq_vec_len((void*)sc->methods)
                || !sc->methods[ms[k]->method_id].is_constructor) continue;
            if (ms[k]->class_id == w_id) i_wc = k;
            if (ms[k]->class_id == e_id) i_ec = k;
            if (ms[k]->class_id == l_id) i_lc = k;
        }
        CHECK(i_wc >= 0 && i_ec >= 0 && i_lc >= 0, "§37g: the W, E, L constructors resolve");
        if (i_wc >= 0) sir_summarize(&cctx, i_wc);
        if (i_ec >= 0) sir_summarize(&cctx, i_ec);
        if (i_lc >= 0) sir_summarize(&cctx, i_lc);

        const compiler_summary_t* sw = i_wc >= 0 ? compiler_method_summary(&cctx, i_wc) : NULL;
        int w_writes = (sw && sw->this_obj >= 0 && sw->wcell_off)
                       ? sw->wcell_off[sw->this_obj + 1] - sw->wcell_off[sw->this_obj] : -1;
        CHECK(w_writes >= 1,
              "§37g: a field-initializer ctor records `this.v = 7` on this_obj — the write is on "
              "`this` (LoadLocal(0)), which must be the SAME object as this_obj");

        const compiler_summary_t* se = i_ec >= 0 ? compiler_method_summary(&cctx, i_ec) : NULL;
        int e_writes = (se && se->this_obj >= 0 && se->wcell_off)
                       ? se->wcell_off[se->this_obj + 1] - se->wcell_off[se->this_obj] : -1;
        CHECK(se && se->this_obj >= 0 && e_writes == 0,
              "§37g: an initializer-free synthetic-default ctor writes NOTHING on this_obj — the "
              "provably-no-op shape scalar replacement is permitted to drop");

        const compiler_summary_t* sl = i_lc >= 0 ? compiler_method_summary(&cctx, i_lc) : NULL;
        CHECK(sl && sl->this_escape != COMPILER_ESC_NONE,
              "§37g: a ctor that stores `this` into a static reports a REAL this_escape — proof "
              "the unification made this_escape reflect the receiver actually used, not a phantom "
              "nothing references");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §39 spec §7 — VIRTUAL FAN-OUT (Choi §4.3: "merge the solution after processing each
        //      method the dispatch can resolve to"). An interface call whose receiver has a known
        //      exact class resolves to its finite target set; consulting the target's CLEAN
        //      summary leaves the array arg NoEscape — where the bottom-graph fallback (all the
        //      virtual/interface cases used to do) would ArgEscape it. The falsifier for "the
        //      fan-out actually consults summaries instead of conservatively escaping".
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "interface I { int m(int[] p); }"
            " class A implements I { public int m(int[] p){ return p.length; } }"
            " class T { static void run(){ I x = new A(); int[] arr = new int[4];"
            "                              arr[0] = 7; x.m(arr); } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int a_id = sema_find_class(&sctx, "A"), t_id = sema_find_class(&sctx, "T");
        int i_m = -1, i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->name) {
                if (ms[k]->class_id == a_id && !strcmp(ms[k]->name, "m"))   i_m   = k;
                if (ms[k]->class_id == t_id && !strcmp(ms[k]->name, "run")) i_run = k;
            }
        CHECK(i_m >= 0 && i_run >= 0, "§39: A.m and run resolve");
        sir_optimize(&cctx, i_m);        /* produce A.m's CLEAN summary */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        int pac = lat_primarray_class(&sctx, SIR_DTINT);
        const sir_node_t* arr = e ? find_new_of_class(ms[i_run]->entry, pac) : NULL;
        CHECK(e && arr, "§39: the int[] is in run's graph");
        if (e && arr)
            CHECK(cp_escape_of_expr(e, arr) == CP_ESC_NONE,
                  "§39: an array passed to a monomorphic interface call whose ONE target (A.m) has "
                  "a CLEAN summary stays NoEscape — the fan-out consulted the summary, not the "
                  "bottom graph");
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §40 spec §7 / Choi §4 — ITERATE-TO-CONVERGENCE over a CYCLIC call graph. `a` and `b`
        //     are mutually recursive; a reverse-topological single pass sees one of them as a
        //     bottom method (the back edge). The convergence wrapper must (1) TERMINATE — a
        //     broken monotonicity would spin, caught here by the fact the call returns at all —
        //     and (2) yield a SOUND summary: `p` is passed AROUND the cycle, so it is (at least)
        //     ArgEscape at the fixpoint, never wrongly CLEAN. (The wrapper's precision GAIN shows
        //     up on the return once return-pts lands; formal escape converges to the same
        //     conservative answer a single pass gives, which is exactly what soundness requires.)
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C {}"
            " class T {"
            "   static void a(C p, int n){ if (n > 0) b(p, n - 1); }"    /* a → b */
            "   static void b(C p, int n){ if (n > 0) a(p, n - 1); }"    /* b → a (cycle) */
            " }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        cctx.optimize = true;
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_a = -1, i_b = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "a")) i_a = k;
                if (!strcmp(ms[k]->name, "b")) i_b = k;
            }
        CHECK(i_a >= 0 && i_b >= 0, "§40: a and b resolve");
        compiler_build_callgraph(&cctx);
        /* Drive the convergence LOGIC on just the cyclic pair (reverse-topo b, a) — the driver's
         * `compiler_summarize_to_convergence` runs this same loop over EVERY method, which for a
         * test that pulls in the whole library is thousands of solves; the property under test is
         * that iterating a cycle's summaries STABILIZES (summary_changed clears) in a bounded
         * number of passes and lands on the sound answer. */
        bool stabilized = false;
        for (int k = 0; k < 6 && !stabilized; k++) {
            cctx.summary_changed = false;
            sir_summarize(&cctx, i_b);
            sir_summarize(&cctx, i_a);
            if (!cctx.summary_changed) stabilized = true;
        }
        CHECK(stabilized, "§40: iterating the a↔b cycle's summaries CONVERGES (stabilizes) in a "
              "bounded number of passes — monotone, not spinning");
        const compiler_summary_t* sa = compiler_method_summary(&cctx, i_a);
        const compiler_summary_t* sb = compiler_method_summary(&cctx, i_b);
        CHECK(sa && sa->computed && sb && sb->computed,
              "§40: both cyclic methods have summaries after convergence");
        /* p is parameter 0 of each; passed around the cycle ⟹ ArgEscape (sound), never CLEAN. */
        if (sa && sa->slot_escape && sa->slot_count > 0)
            CHECK(sa->slot_escape[0] == COMPILER_ESC_ARG,
                  "§40: a's `p`, passed around the a↔b cycle, converges to ArgEscape (sound) — "
                  "not wrongly CLEAN");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §41 spec §7.2 — RETURN-PTS (FORMAL aliasing). A method whose every return yields the
        //      same formal (`id(C p){ return p; }`) lets the caller ALIAS the call result to that
        //      actual: `id(a)`'s result points to a's OBJECT (a known, concrete site — so the
        //      result carries a's class/escape), not the opaque `Oret`. The producer records
        //      ret_kind=FORMAL(param 0); the pts transfer consumes it.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C {}"
            " class T {"
            "   static C id(C p){ return p; }"
            "   static void run(){ C x = new C(); C y = id(x); if (y == null) return; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T"), c_id = sema_find_class(&sctx, "C");
        int i_id = -1, i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "id"))  i_id  = k;
                if (!strcmp(ms[k]->name, "run")) i_run = k;
            }
        CHECK(i_id >= 0 && i_run >= 0, "§41: id and run resolve");
        sir_optimize(&cctx, i_id);       /* produce id's summary — ret_kind = FORMAL(param 0) */
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_id);
        CHECK(s && s->ret_kind == COMPILER_RET_FORMAL && s->ret_param == 0,
              "§41: id's summary records ret_kind = FORMAL, parameter 0 (`return p`)");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* xnew = e ? find_new_of_class(ms[i_run]->entry, c_id) : NULL;
        const sir_node_t* call = e ? find_tag(ms[i_run]->entry, SIR_INVOKESTATIC, 400) : NULL;
        CHECK(e && xnew && call, "§41: the new C() and the id(x) call are in run's graph");
        if (e && xnew && call) {
            cp_pts_t rp = cp_pts_of_expr(e, call);
            CHECK(cp_pts_has(e, rp, cp_obj_of(e, xnew)),
                  "§41: id(x)'s result ALIASES x — its pts holds x's OBJECT (the return-pts "
                  "aliasing), not just the opaque Oret");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §42p spec §7 — PRODUCER side of §42, pinned SEPARATELY so an integration failure is
        //      localized to producer-vs-consumer, not chased with probes. store(C p, D a){ p.v=a }
        //      must summarize as a CLEAN object write of a into p.v: an edge p→a, and a wcell on p
        //      whose flag is NOT INCOMPLETE (no Oext / bottom sub-call) — the precondition for a
        //      caller to REPLACE CP_OBJ_EXT.
        {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(
                "class D {} class C { D v; }"
                " class T { static void store(C p, D a){ p.v = a; } }", &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            int t_id = sema_find_class(&sctx, "T"), i_st = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "store")) i_st = k;
            CHECK(i_st >= 0, "§42p: store resolves");
            sir_optimize(&cctx, i_st);
            const compiler_summary_t* s = i_st >= 0 ? compiler_method_summary(&cctx, i_st) : NULL;
            CHECK(s && s->computed && s->slot_count >= 2, "§42p: store has a computed summary");
            if (s && s->computed && s->slot_count >= 2) {
                int po = s->slot_obj[0], ao = s->slot_obj[1];   /* p, a */
                CHECK(po >= 0 && ao >= 0, "§42p: p and a have summary objects");
                bool edge_pa = false;
                if (po >= 0 && ao >= 0)
                    for (int ei = s->edge_off[po]; ei < s->edge_off[po + 1]; ei++)
                        if (s->edge_dst[ei] == ao) edge_pa = true;
                CHECK(edge_pa, "§42p: p has a field edge to a (p.v = a captured as an object write)");
                CHECK(po >= 0 && s->obj_leaked && !s->obj_leaked[po],
                      "§42p: the receiver p is NOT leaked to a bottom method — the write is fully "
                      "captured, so a caller may REPLACE CP_OBJ_EXT (spec/plan: receiver CLEAN)");
            }
            sema_destroy(&sctx); bbq_arena_free(&a);
        }

        // §42 spec §7 / Fig 7 "Updating Caller Edges" — ADDED CALLER EDGES. `store(C p, D a){ p.v =
        //      a; }` should make the caller's `o.v` point to EXACTLY x, not the CP_OBJ_EXT the
        //      clobber conservatively leaves. **The machinery IS landed and SOUND** — the per-cell
        //      injection matrix, the per-object completeness guard (obj_leaked, spec's "receiver
        //      CLEAN"), the DIRECT-vs-TRANSITIVE flag, and maybe_null (§37f/§42p/§42d/§42b all
        //      green; exec green). **DELIBERATE RED PIN** for the last step: reaching EXACTLY x
        //      needs the pre-call `o.v` to be CLEAN (else the entry-value edge maps to Oext and the
        //      guard keeps EXT), which requires GATE 5's kill-narrowing — gated OFF in
        //      cp_update_heap pending the ref-array-overlay clobber completeness (the survives
        //      extension miscompiles 3 arraycopy exec cases). So §42 flips GREEN when Gate 5 lands;
        //      it stays SOUND (o.v = EXT) until then. Do NOT weaken the assertion.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class D {} class C { D v; }"
            " class T {"
            "   static void store(C p, D a){ p.v = a; }"
            "   static void run(){ C o = new C(); D x = new D(); store(o, x);"
            "                      D y = o.v; if (y == null) return; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T"), d_id = sema_find_class(&sctx, "D");
        int i_st = -1, i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "store")) i_st  = k;
                if (!strcmp(ms[k]->name, "run"))   i_run = k;
            }
        CHECK(i_st >= 0 && i_run >= 0, "§42: store and run resolve");
        compiler_summarize_to_convergence(&cctx);   /* store + the ctors, as the real driver does */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* xnew = e ? find_new_of_class(ms[i_run]->entry, d_id) : NULL;
        const sir_node_t* getv = e ? find_tag(ms[i_run]->entry, SIR_GETFIELD, 400) : NULL;
        CHECK(e && xnew && getv, "§42: the new D() and the o.v read are in run's graph");
        if (e && xnew && getv) {
            cp_pts_t gp = cp_pts_of_expr(e, getv);
            /* EXACTLY x — the injected edge must REPLACE the conservative CP_OBJ_EXT, not sit
             * alongside it. Adding x while EXT still swamps every consumer is the weasel the
             * plan rejects (green test, zero real precision), so both halves are asserted. */
            CHECK(cp_pts_has(e, gp, cp_obj_of(e, xnew)) && !cp_pts_has(e, gp, CP_OBJ_EXT),
                  "§42: after store(o,x), o.v points to EXACTLY x — Fig 7's added caller edge "
                  "REPLACES the clobber's conservative unknown (CP_OBJ_EXT gone)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44 spec §7 / VFG ISMM'13 §4.1 (verbatim: "a STORE ∗p=x to object O can kill all
        //      previous values stored to O … a sparse data-flow analysis … whether a STORE can
        //      reach a LOAD … without being killed by another STORE"). A call is NOT a wide kill:
        //      its memory effect is its callee's side-effecting STOREs. `g()` writes nothing and
        //      touches no global (¬writes_global), so `o.x = 5; g(); return o.x` FORWARDS the 5
        //      across the call. Under CP_CELL_ALL the call versioned every cell and the load read
        //      the kill (opaque) — RED. Gate 5 (per-cell-key kill from the callee write set) makes
        //      it KNOWN 5. Value forwarding is the half `survives` (pts-only) cannot deliver.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int x; }"
            " class T {"
            "   static void g(){ }"
            "   static int run(){ C o = new C(); o.x = 5; g(); return o.x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "run")) i_run = k;
        CHECK(i_run >= 0, "§44: run resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* rt = first_spine_node(ms[i_run]->entry, SIR_RETURN);
        CHECK(e && rt && rt->return_.value, "§44: run returns o.x");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state == CP_C_KNOWN && cp_known_i64(rv->constant) == 5,
                  "§44: o.x = 5 forwards across g() (writes nothing, no global) — a call versions "
                  "ONLY the cells its callee writes, not all memory (VFG ISMM'13 §4.1)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44b Gate 5 SOUNDNESS at the integration level — a callee that WRITES the cell blocks the
        //      forward. `w(C p){ p.x = 9; }` writes p.x, so after `o.x = 5; w(o);` the load `o.x`
        //      must NOT fold to 5 — the summary's write set clobbers o.x, so the kill-walk's
        //      `preserve` is false. Green both before and after Gate 5 (nothing forwarded before);
        //      it exists so an over-eager forward (ignoring clobbered) turns it RED here, not in exec.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int x; }"
            " class T {"
            "   static void w(C p){ p.x = 9; }"
            "   static int run(){ C o = new C(); o.x = 5; w(o); return o.x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "run")) i_run = k;
        CHECK(i_run >= 0, "§44b: run resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* rt = first_spine_node(ms[i_run]->entry, SIR_RETURN);
        CHECK(e && rt && rt->return_.value, "§44b: run returns o.x");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state != CP_C_KNOWN,
                  "§44b: w(o) writes o.x, so o.x must NOT forward the stale 5 across the call "
                  "(the callee's write set clobbers the cell — preserve is false)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44c Gate 5 branch — `clobbered` is CELL-SPECIFIC. `setv(C p){ p.v = 7; }` writes p.v and
        //      NEVER p.w, so after `o.w = 5; setv(o);` the load `o.w` still forwards the 5 (its cell
        //      is un-clobbered; o stays NoEscape as a CLEAN receiver). The value analogue of §42b
        //      (which pinned pts survival) — this pins VALUE forwarding of an un-written sibling cell.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int v; int w; }"
            " class T {"
            "   static void setv(C p){ p.v = 7; }"
            "   static int run(){ C o = new C(); o.w = 5; setv(o); return o.w; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "run")) i_run = k;
        CHECK(i_run >= 0, "§44c: run resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* rt = first_spine_node(ms[i_run]->entry, SIR_RETURN);
        CHECK(e && rt && rt->return_.value, "§44c: run returns o.w");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state == CP_C_KNOWN && cp_known_i64(rv->constant) == 5,
                  "§44c: setv writes p.v not p.w, so o.w = 5 forwards across setv(o) — clobbered is "
                  "cell-specific, an un-written sibling cell still forwards its value");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44d RETRACTION of the executable-edge flag — spec §4's per-edge fact over a fact that
        //      RISES. Gate 5 is "a revocable load-follower IN the fixpoint": `y = o.x` computes
        //      TOP→BOTTOM (heap still opaque) → KNOWN 5 when the forward premises mature. During
        //      the BOTTOM window the guard `y != 5` justifies BOTH arms, and the taken arm holds a
        //      bottom call h() whose escape poisoning is STICKY (has_bottom_call) — so a mark that
        //      cannot retract keeps the arm lit past the window, h() is walked, survives(o) turns
        //      false, and the forward itself dies: return o.x stays opaque. With retraction the
        //      cond's rise un-lights the arm inside the same drain, before the escape sweep runs.
        //      Click never needs this (his types only descend); the riser is OUR extension, so the
        //      retraction is its dual. FALSIFIED 07-29 by disabling retraction (one line): §42/
        //      §44/§44c went RED — they are the falsifiers for retraction + the dequeue-time
        //      identity as a unit. THIS pin stayed green under that break (the dequeue-time
        //      identity wins the race in-drain here, so the arm never lights), so it is COVERAGE
        //      for the sticky-poison shape, not an independent falsifier — kept because the shape
        //      (a bottom call in the transiently-lit arm) appears in none of the other three.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int x; }"
            " class T {"
            "   static native void h();"
            "   static int run(){ C o = new C(); o.x = 5; int y = o.x;"
            "                     if (y != 5) { h(); } return o.x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "run")) i_run = k;
        CHECK(i_run >= 0, "§44d: run resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        /* The discriminator is y's OWN load — the one whose transient BOTTOM lit the arm.
         * (The post-merge `return o.x` reads the merge's cell-φ, which the forward walk
         * does not chase — a pre-existing, separate limitation; asserting on it would pin
         * a claim this test is not about.) Under non-retracting marks the lit arm's h()
         * stickily sets has_bottom_call, Gate 5 revokes, and y ends BOTTOM. */
        const sir_node_t* yload = e ? find_tag(ms[i_run]->entry, SIR_GETFIELD, 400) : NULL;
        CHECK(e && yload, "§44d: y = o.x is in run's graph");
        if (e && yload) {
            cp_vnode_t* yv = vnode_for(e, yload);
            CHECK(yv && yv->constant.state == CP_C_KNOWN && cp_known_i64(yv->constant) == 5,
                  "§44d: a dead arm's bottom call must not poison the forward — the edge lit "
                  "during y's transient BOTTOM retracts when the cond settles KNOWN false, so "
                  "h() is never walked and y = o.x still forwards (edge flag = f(current "
                  "facts), retracting because Gate 5's rise is revocable optimism)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44e the PER-KIND Follower revert gate — Click §4.7.4: "Each Node has a constant time
        //      test to determine if it is a Follower", i.e. the justification is per kind, and
        //      §4.7.5 line 6.1's revert judges the node's OWN identity. `b & b` is a SAME-INPUT
        //      follower (structural, never reverts); before the gate, the §4.8 identity revert —
        //      which fires on ANY 2-input EXPR follower at dequeue — reverted it (b carries no
        //      KNOWN identity const), and the apply sweep re-made it next round: a livelock once
        //      transitions were enqueued per Fig 4.7 line 7. The pin asserts the link SURVIVES the
        //      solve with its own kind recorded.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static int band(int b){ return b & b; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_b = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "band")) i_b = k;
        CHECK(i_b >= 0, "§44e: band resolves");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_b, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_b], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_b]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt && rt->return_.value, "§44e: band returns b & b");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->leader >= 0 && rv->follower_kind == CP_FK_SAMEIN,
                  "§44e: b & b survives the solve as a SAME-INPUT follower — the identity revert "
                  "judges only its own links (§4.7.4 per-kind test), so a structural follower on "
                  "an identity-capable op is not reverted by a rule that did not make it");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44f W2-T1 — THE LOOP BACK EDGE OPENS WHEN THE LOOP CONDITION FALLS (the String.trim /
        //      FloatingDecimal expLoop shape, plan §R.6's first ordered pin). A loop-header φ has
        //      an entry input and a back-edge input; the back edge is live only once the loop
        //      condition's fact falls below ⊤ (§4.3/§4.4.1). If the marking leaves it dead, the φ
        //      sees ONE live input, becomes its Follower (§4.9), and every consumer folds the
        //      loop-carried value to its ENTRY value — in FloatingDecimal.readJavaFormatString
        //      that made `i ≡ expAt` a congruence, folded `i == expAt` TRUE, and rejected every
        //      exponent string with NumberFormatException at runtime (input never consulted).
        //      Two shapes: the plain count loop, and expLoop's break-from-middle.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T {"
            "   static int count(int n){ int i = 0; while (i < n) { i = i + 1; } return i; }"
            "   static int scan(int n, int d){ int i = 0;"
            "       while (i < n) { if (d >= 48 && d <= 57) { i = i + 1; } else { break; } }"
            "       return i; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_c = -1, i_s = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "count")) i_c = k;
                if (!strcmp(ms[k]->name, "scan"))  i_s = k;
            }
        CHECK(i_c >= 0 && i_s >= 0, "§44f: count and scan resolve");
        for (int wcase = 0; wcase < 2; wcase++) {
            int mi = wcase == 0 ? i_c : i_s;
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, mi, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[mi], sc, nsc);
            const sir_node_t* rt = e ? first_spine_node(ms[mi]->entry, SIR_RETURN) : NULL;
            CHECK(e && rt && rt->return_.value, "§44f: the loop method returns i");
            if (e && rt && rt->return_.value) {
                cp_vnode_t* rv = vnode_for(e, rt->return_.value);
                CHECK(rv && !(rv->constant.state == CP_C_KNOWN
                              && rv->constant.cwidth == CP_W_I32
                              && rv->constant.value == 0),
                      wcase == 0
                      ? "§44f: count(n)'s return is NOT folded to the entry value 0 — the "
                        "back edge is live once `i < n` falls, so the header φ keeps both "
                        "inputs (§4.3; a φ following its entry input here folds the loop away)"
                      : "§44f: scan(n,d)'s return is NOT folded to 0 — expLoop's break-from-"
                        "middle shape; a dead back edge here is what made readJavaFormatString "
                        "reject every exponent string");
            }
            if (e) cp_free(e);
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44g W2-T1 ENRICHED — FloatingDecimal.readJavaFormatString's exponent shape distilled:
        //      the sign switch WITH FALLTHROUGH, the `expAt = i` copy, a CALL inside the loop
        //      (a kill row on the memory chain), and the post-loop `i == expAt` compare. The
        //      runtime miscompile: every "NeM" string throws NumberFormatException because the
        //      compare folds TRUE — `i ≡ expAt` held as a congruence past the loop, i.e. the
        //      loop-carried φ was treated as its entry value. §44f's plain shapes hold, so
        //      whatever collapses the φ needs THIS much context; the pin localizes it.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T {"
            "   static int g(int i){ return i + 48; }"
            "   static int px(int len, int c0){"
            "       int i = 0;"
            "       i = i + 1;"                       /* the ++i crossing 'e' */
            "       switch (c0) {"
            "       case 45: i = i + 1;"              /* '-' — FALLTHROUGH */
            "       case 43: i = i + 1;"              /* '+' */
            "       }"
            "       int expAt = i;"
            "       while (i < len) {"
            "           int c = g(i);"
            "           if (c >= 48 && c <= 57) { i = i + 1; } else { break; }"
            "       }"
            "       if (i == expAt) { return -1; }"
            "       return i; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_px = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "px")) i_px = k;
        CHECK(i_px >= 0, "§44g: px resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_px, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_px], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_px]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt, "§44g: px has a return");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && !(rv->constant.state == CP_C_KNOWN
                          && rv->constant.cwidth == CP_W_I32
                          && rv->constant.value == -1),
                  "§44g: px's reachable return is NOT the folded -1 — `i == expAt` must not "
                  "hold past a loop whose back edge can run (the exponent-rejection miscompile)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44h THE DEAD CYCLE MUST DIE — lit_in is a refcount, and a cycle self-sustains: when
        //      the loop's ENTRY edge retracts (y rises to KNOWN 1 via Gate 5, cond `y != 1`
        //      settles false), the back edge still holds lit_in ≥ 1 unless retraction collapses
        //      the cycle. A sustained "live" body containing a bottom call h() stickily poisons
        //      escape (has_bottom_call) and Gate 5 then revokes the very forward that killed the
        //      loop — observable as y NOT folding. From-scratch derivations kill such cycles;
        //      the incremental marking must too.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C { int x; }"
            " class T {"
            "   static native void h();"
            "   static int f(){ C o = new C(); o.x = 1; int y = o.x;"
            "                   while (y != 1) { h(); } return y; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_f = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "f")) i_f = k;
        CHECK(i_f >= 0, "§44h: f resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_f, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_f], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_f]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt && rt->return_.value, "§44h: f returns y");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && rv->constant.state == CP_C_KNOWN && cp_known_i64(rv->constant) == 1,
                  "§44h: a retracted loop entry collapses the cycle — the body's bottom call "
                  "must not stay lit through the back edge's self-sustaining lit_in and poison "
                  "the forward that killed the loop (y folds KNOWN 1)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44i the ARRLEN follower's §4.7.5 line-6.1 revert — its premise runs through
        //      cp_value_leader, which follows CURRENT follower links: a φ-follower transiently
        //      puts one allocation in reach, the arraylen link forms on it, the φ reverts, and
        //      the `.length` would follow the WRONG size without its own premise-recheck. The
        //      shape: a diamond whose arms allocate different sizes.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static int g(int c){"
            "   int[] a;"
            "   if (c != 0) { a = new int[5]; } else { a = new int[7]; }"
            "   return a.length; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_g = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "g")) i_g = k;
        CHECK(i_g >= 0, "§44i: g resolves");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_g, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_g], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_g]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt && rt->return_.value, "§44i: g returns a.length");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && !(rv->constant.state == CP_C_KNOWN
                          && (cp_known_i64(rv->constant) == 5 || cp_known_i64(rv->constant) == 7)),
                  "§44i: a.length after a two-size diamond folds to NEITHER size — an arraylen "
                  "follower formed on a transient single-live-arm φ must revert when the φ does "
                  "(a KNOWN 5 or 7 here is a runtime-dependent value folded to one arm's)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44j W2-T3 — CONTROL IS NOT PART OF CONGRUENCE for data nodes (Click A.4.4: encoding
        //      the control input into value numbering "ends up doing only local value
        //      numbering"). Two textually identical exprs in DIFFERENT arms stay congruent, so
        //      the join φ sees one partition and follows it (§4.9). If the edge-flag work ever
        //      leaks control into the data hash, this splits and the φ stays a real merge.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static int j(int c, int x){"
            "   int r;"
            "   if (c != 0) { r = x + 1; } else { r = x + 1; }"
            "   return r; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_j = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "j")) i_j = k;
        CHECK(i_j >= 0, "§44j: j resolves");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_j, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_j], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_j]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt && rt->return_.value, "§44j: j returns r");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            /* Both arms' `x+1` congruent ⟹ the join φ's live inputs share one partition ⟹
             * it is a Follower (§4.9) — the cross-block congruence A.4.4 protects. */
            CHECK(rv && (rv->leader >= 0
                         || (rv->input_count == 1 && rv->inputs[0] >= 0
                             && e->vnodes[rv->inputs[0]]->leader >= 0)),
                  "§44j: identical exprs in different arms stay congruent (A.4.4 — control is "
                  "not in the data hash), so the join φ follows their one partition");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §44k W2-T2 — the Vector.trimToSize shape: a CONDITIONAL REASSIGN (half-diamond). The
        //      local's φ merges the entry allocation with one arm's; a LoadLocal follower of
        //      that φ carries its constant THROUGH the leader chain, so when the φ descends
        //      (both inputs live, different allocations) the follower must descend with it —
        //      07-29's defect was a follower keeping the entry allocation's KNOWN past the
        //      φ's fall, folding `.length` to the wrong size.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static int t(int c){"
            "   int[] a = new int[3];"
            "   if (c != 0) { a = new int[7]; }"
            "   return a.length; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_t = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "t")) i_t = k;
        CHECK(i_t >= 0, "§44k: t resolves");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_t, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_t], sc, nsc);
        const sir_node_t* rt = e ? first_spine_node(ms[i_t]->entry, SIR_RETURN) : NULL;
        CHECK(e && rt && rt->return_.value, "§44k: t returns a.length");
        if (e && rt && rt->return_.value) {
            cp_vnode_t* rv = vnode_for(e, rt->return_.value);
            CHECK(rv && !(rv->constant.state == CP_C_KNOWN
                          && (cp_known_i64(rv->constant) == 3 || cp_known_i64(rv->constant) == 7)),
                  "§44k: after a conditional reassign, a.length folds to NEITHER size — the "
                  "follower's constant descends with its φ leader (the trimToSize shape)");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §45p E1 / VFG Rule 1 return half / JLS §15.9.4 — PRODUCER. A factory `m(){ return new C(); }`
        //      classifies its return COMPILER_RET_FRESH: the Oret identity is NOT mintable at the caller
        //      (Obj naming is per-site), but a `new` never returns null, so the sound achievable fact —
        //      the result is NonNull — is recorded. Pinned at the producer before its consumer (L0→L1).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C {}"
            " class T { static C m(){ return new C(); } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_m = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "m")) i_m = k;
        CHECK(i_m >= 0, "§45p: m resolves");
        compiler_summarize_to_convergence(&cctx);
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_m);
        CHECK(s && s->computed, "§45p: m has a computed summary");
        CHECK(s && s->ret_kind == COMPILER_RET_FRESH && !s->ret_maybe_null,
              "§45p: return new C() classifies FRESH — NonNull result (a `new` never returns null), "
              "even though its Oret identity is not mintable at the caller (per-site naming)");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §45 E1 — CONSUMER (L1). The result of a FRESH-returning callee is NonNull: `x = m()` with
        //      m returning `new C()` ⟹ pts(x) excludes ⊥null. The Oret identity remains (its site is
        //      unmintable at the caller), but the null is gone — a caller can drop the NPE on x. This
        //      is the behavioral payoff the producer §45p enables; verified RED-by-construction (no
        //      call result dropped null before this consumer existed).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C {}"
            " class T { static C m(){ return new C(); }"
            "           static C run(){ C x = m(); return x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "run")) i_run = k;
        CHECK(i_run >= 0, "§45: run resolves");
        compiler_summarize_to_convergence(&cctx);
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* call = e ? find_tag(ms[i_run]->entry, SIR_INVOKESTATIC, 400) : NULL;
        CHECK(e && call, "§45: the m() call is in run's graph");
        if (e && call) {
            cp_pts_t p = cp_pts_of_expr(e, call);
            CHECK(!cp_pts_has(e, p, CP_OBJ_NULL),
                  "§45: the result of a FRESH-returning callee is NonNull — pts drops ⊥null (a `new` "
                  "never returns null); the Oret identity remains");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §45n E1's TRANSITIVE half. `run(){ return m(); }` where m is FRESH: run's return is not
        //      FRESH (the objects are m's Oret, not run's sites) but it is provably NEVER NULL —
        //      the E1 consumer already dropped ⊥null from the m() result inside run's own solve.
        //      COMPILER_RET_NONNULL records exactly that (identity stays Oret); the consumer treats
        //      it like FRESH's null half. Transitive at any depth via the convergence loop:
        //      pass 1 marks m FRESH, pass 2 sees run's return null-free and marks it NONNULL.
        //      PRODUCER + CONSUMER pinned here (h's call result drops ⊥null through run).
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class C {}"
            " class T { static C m(){ return new C(); }"
            "           static C run(){ return m(); }"
            "           static C h(){ C x = run(); return x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_run = -1, i_h = -1;
        for (int k = 0; k < mc; k++) {
            if (ms[k]->class_id != t_id || !ms[k]->name) continue;
            if (!strcmp(ms[k]->name, "run")) i_run = k;
            if (!strcmp(ms[k]->name, "h"))   i_h = k;
        }
        CHECK(i_run >= 0 && i_h >= 0, "§45n: run and h resolve");
        compiler_summarize_to_convergence(&cctx);
        const compiler_summary_t* s = compiler_method_summary(&cctx, i_run);
        CHECK(s && s->computed && s->ret_kind == COMPILER_RET_NONNULL && !s->ret_maybe_null,
              "§45n producer: run(){ return m(); } with m FRESH classifies NONNULL — not FRESH "
              "(the objects are m's, unmintable here), but provably never null, transitively "
              "via the convergence loop");
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_h, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_h], sc, nsc);
        const sir_node_t* call = e ? find_tag(ms[i_h]->entry, SIR_INVOKESTATIC, 400) : NULL;
        CHECK(e && call, "§45n: the run() call is in h's graph");
        if (e && call) {
            cp_pts_t p = cp_pts_of_expr(e, call);
            CHECK(!cp_pts_has(e, p, CP_OBJ_NULL),
                  "§45n consumer: the result of a NONNULL-returning callee drops ⊥null — the "
                  "same consumer FRESH rides, one more kind admitted");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §46c (spec §7.2's VALUE half — lattice D made interprocedural).
        //      PRODUCER: a numeric-returning method exports the MEET over its reachable returns —
        //      KNOWN for one value, RANGE for several, the right width for long. CONSUMER: the
        //      caller's invoke transfer reads the summary (the call's vnode const IS the fact), and
        //      a virtual call MEETS all defunctionalized targets' exports. Fail-closed negatives:
        //      a bottom (native) callee exports nothing and the caller claims nothing.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class A { int g(){ return 5; } } class B extends A { int g(){ return 7; } }"
            " class T {"
            "   static int five(){ return 5; }"
            "   static int pick(boolean p){ if (p) return 1; return 2; }"
            "   static long big(){ return 9000000000L; }"
            "   static int useK(){ return five() + 1; }"
            "   static int usev(boolean p){ A q = p ? new A() : new B(); return q.g(); } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_five = -1, i_pick = -1, i_big = -1, i_useK = -1, i_usev = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "five")) i_five = k;
                if (!strcmp(ms[k]->name, "pick")) i_pick = k;
                if (!strcmp(ms[k]->name, "big"))  i_big  = k;
                if (!strcmp(ms[k]->name, "useK")) i_useK = k;
                if (!strcmp(ms[k]->name, "usev")) i_usev = k;
            }
        CHECK(i_five >= 0 && i_pick >= 0 && i_big >= 0 && i_useK >= 0 && i_usev >= 0,
              "§46c: five/pick/big/useK/usev resolve");
        compiler_summarize_to_convergence(&cctx);
        const compiler_summary_t* sf = compiler_method_summary(&cctx, i_five);
        CHECK(sf && sf->computed && sf->ret_cstate == COMPILER_RETC_KNOWN
                 && sf->ret_cwidth == 0 && sf->ret_clo == 5 && sf->ret_chi == 5,
              "§46c producer: five() exports KNOWN i32 5");
        const compiler_summary_t* sp = compiler_method_summary(&cctx, i_pick);
        CHECK(sp && sp->computed && sp->ret_cstate == COMPILER_RETC_RANGE
                 && sp->ret_clo == 1 && sp->ret_chi == 2,
              "§46c producer: pick() exports the MEET of its returns — RANGE [1,2]");
        const compiler_summary_t* sb = compiler_method_summary(&cctx, i_big);
        CHECK(sb && sb->computed && sb->ret_cstate == COMPILER_RETC_KNOWN
                 && sb->ret_cwidth == 1 && sb->ret_clo == 9000000000LL,
              "§46c producer: big() exports KNOWN at the i64 width — no i32 truncation");
        {
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, i_useK, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i_useK], sc, nsc);
            const sir_node_t* call = e ? find_tag(ms[i_useK]->entry, SIR_INVOKESTATIC, 400) : NULL;
            CHECK(e && call, "§46c: the five() call is in useK's graph");
            if (e && call) {
                cp_vnode_t* cv = vnode_for(e, call);
                CHECK(cv && cv->constant.state == CP_C_KNOWN && cp_known_i64(cv->constant) == 5,
                      "§46c consumer: the call's vnode const IS the callee's exported KNOWN 5");
            }
            if (e) cp_free(e);
        }
        {
            int nsc = 0;
            const compiler_fact_t* sc = compiler_get_facts(&cctx, i_usev, &nsc);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i_usev], sc, nsc);
            const sir_node_t* call = e ? find_tag(ms[i_usev]->entry, SIR_INVOKEVIRTUAL, 400) : NULL;
            CHECK(e && call, "§46c: the q.g() call is in usev's graph");
            if (e && call) {
                cp_vnode_t* cv = vnode_for(e, call);
                CHECK(cv && cv->constant.state == CP_C_RANGE
                         && cv->constant.lo == 5 && cv->constant.hi == 7,
                      "§46c consumer: a virtual call over concrete-site pts MEETS all "
                      "defunctionalized targets' exports — {5,7} ⟹ [5,7] (a phantom receiver "
                      "stays fail-closed; widening that gate is stage-3 devirt, not the interprocedural value half)");
            }
            if (e) cp_free(e);
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §47p (Stadler §5.1–5.4 — PARTIAL escape: virtual per BRANCH, materialize at the
        //      escape point, recorded merges only). The diamond parity case guards the
        //      cp_sr→cp_pea swap. Shapes:
        //      f: escapes ONLY on the then-arm (PutStatic) — the ok path keeps NO allocation,
        //         the arm gains the materialized one (New sunk to the recorded escape site).
        //      h: mixed merge — escaped on one pred, virtual on the other ⟹ materialize on the
        //         virtual pred edge; no allocation remains in the straight-line prefix.
        //      m: alloc-in-loop, never escapes ⟹ fully virtual across the back edge (§5.4's
        //         speculative loop iteration) — zero allocations in the whole graph.
        //      g (parity, green TODAY): whole-method NoEscape diamond — cp_sr already replaces.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class E { int c; }"
            " class T { static E sink;"
            "   static int f(int x){ E e = new E(); e.c = x + 1;"
            "                        if (x < 0) { sink = e; return -1; }"
            "                        return e.c; }"
            "   static int h(int x){ E e = new E(); e.c = 5;"
            "                        if (x > 0) { sink = e; }"
            "                        return e.c; }"
            "   static int m(int x){ int s = 0;"
            "                        for (int i = 0; i < x; i++) { E e = new E(); e.c = i; s = s + e.c; }"
            "                        return s; }"
            "   static int g(int x){ E e = new E();"
            "                        if (x > 0) e.c = 1; else e.c = 2;"
            "                        return e.c; } }",
            &a, &nlib);
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_f = -1, i_h = -1, i_m = -1, i_g = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "f")) i_f = k;
                if (!strcmp(ms[k]->name, "h")) i_h = k;
                if (!strcmp(ms[k]->name, "m")) i_m = k;
                if (!strcmp(ms[k]->name, "g")) i_g = k;
            }
        CHECK(i_f >= 0 && i_h >= 0 && i_m >= 0 && i_g >= 0, "§47p: f/h/m/g resolve");
        compiler_summarize_to_convergence(&cctx);
        sir_optimize(&cctx, i_f);
        sir_optimize(&cctx, i_h);
        sir_optimize(&cctx, i_m);
        sir_optimize(&cctx, i_g);
        {
            const sir_node_t* br = NULL;
            int pre = count_new_prefix(ms[i_f]->entry, &br);
            CHECK(pre == 0 && br != NULL,
                  "§47p f: an alloc escaping ONLY on the then-arm is VIRTUAL on the straight "
                  "line — no New before the branch (Stadler: materialize at the escape point)");
            if (br) CHECK(find_tag(br->branch.on_true, SIR_NEW, 200) != NULL,
                  "§47p f: the materialized New lives on the escaping arm");
        }
        {
            const sir_node_t* br = NULL;
            int pre = count_new_prefix(ms[i_h]->entry, &br);
            CHECK(pre == 0 && br != NULL,
                  "§47p h: mixed merge — virtual on one pred, escaped on the other — sinks the "
                  "allocation out of the prefix (materialized on the pred edges, §5.3)");
        }
        CHECK(count_new_graph(ms[i_m]->entry) == 0,
              "§47p m: an in-loop alloc that never escapes is fully virtual across the back "
              "edge (§5.4 speculative loop iteration) — zero allocations remain");
        CHECK(count_new_graph(ms[i_g]->entry) == 0,
              "§47p g PARITY: the whole-method NoEscape diamond stays scalar-replaced across "
              "the cp_sr→cp_pea swap");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §42b spec §7 — an invoke kills ONLY the cells its summary says it writes. `setv` writes
        //      p.v and NEVER p.w, so a caller's `o.w` survives setv(o,…) unchanged. GREEN: here `o`
        //      stays NoEscape (setv's param is CLEAN — o's field is written, o itself is not
        //      leaked), so o.w survives by the existing NoEscape rule. The stronger case — an
        //      ArgEscape object whose un-clobbered cell survives (GATE 5's kill-narrowing) — is NOT
        //      exercised here and stays gated OFF in cp_update_heap pending the ref-array clobber
        //      completeness; a dedicated ArgEscape pin lands with that focused pass.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class D {} class C { D v; D w; }"
            " class T {"
            "   static void setv(C p, D a){ p.v = a; }"
            "   static void run(){ C o = new C(); D x = new D(); o.w = x; setv(o, new D());"
            "                      D y = o.w; if (y == null) return; } }",
            &a, &nlib);
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T"), d_id = sema_find_class(&sctx, "D");
        int i_sv = -1, i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "setv")) i_sv  = k;
                if (!strcmp(ms[k]->name, "run"))  i_run = k;
            }
        CHECK(i_sv >= 0 && i_run >= 0, "§42b: setv and run resolve");
        compiler_summarize_to_convergence(&cctx);   /* setv + the ctors, as the real driver does */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* xnew = e ? find_new_of_class(ms[i_run]->entry, d_id) : NULL;
        const sir_node_t* getw = e ? find_tag(ms[i_run]->entry, SIR_GETFIELD, 400) : NULL;
        CHECK(e && xnew && getw, "§42b: the new D() x and the o.w read are in run's graph");
        if (e && xnew && getw) {
            cp_pts_t gp = cp_pts_of_expr(e, getw);
            CHECK(cp_pts_has(e, gp, cp_obj_of(e, xnew)) && !cp_pts_has(e, gp, CP_OBJ_EXT),
                  "§42b: o.w survives setv(o,…) — the invoke kills only p.v (its summary's write "
                  "set), never p.w, so o.w still points to EXACTLY x");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §42d spec §7 — THE COMPLETENESS GUARD (the reason this is careful, not a cram). `store`
        //      writes p.v = a, then passes p to `leak`, which is a BOTTOM method (never analyzed).
        //      leak could ALSO write p.v with something we cannot see, so store's summary marks
        //      p.v INCOMPLETE — and the caller's `o.v` must STAY CP_OBJ_EXT (the injected x alone
        //      would be UNSOUND). This pins that the guard keeps EXT where the write is not fully
        //      captured; a naive "inject x, drop EXT" would wrongly pass §42 and miscompile here.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class D {} class C { D v; }"
            " class T {"
            "   static void leak(C p){ }"                       /* left BOTTOM — never summarized */
            "   static void store(C p, D a){ p.v = a; leak(p); }"
            "   static void run(){ C o = new C(); D x = new D(); store(o, x);"
            "                      D y = o.v; if (y == null) return; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int t_id = sema_find_class(&sctx, "T");
        int i_st = -1, i_run = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "store")) i_st  = k;   /* leak is NOT optimized → bottom */
                if (!strcmp(ms[k]->name, "run"))   i_run = k;
            }
        CHECK(i_st >= 0 && i_run >= 0, "§42d: store and run resolve");
        sir_optimize(&cctx, i_st);       /* store's summary: p.v written, but INCOMPLETE (leak) */
        int nsc = 0;
        const compiler_fact_t* sc = compiler_get_facts(&cctx, i_run, &nsc);
        cp_engine_t* e = cp_build_ctx(&cctx, ms[i_run], sc, nsc);
        const sir_node_t* getv = e ? find_tag(ms[i_run]->entry, SIR_GETFIELD, 400) : NULL;
        CHECK(e && getv, "§42d: the o.v read is in run's graph");
        if (e && getv) {
            cp_pts_t gp = cp_pts_of_expr(e, getv);
            CHECK(cp_pts_has(e, gp, CP_OBJ_EXT),
                  "§42d: o.v STAYS CP_OBJ_EXT — a bottom sub-call (leak) could also write p.v, so "
                  "the write is not fully captured and the precise injection must NOT fire");
        }
        if (e) cp_free(e);
        sema_destroy(&sctx); bbq_arena_free(&a);
    }
    {
        // §43 spec §6.1 — CTOR-AWARE SCALAR REPLACEMENT, the first customer, with a
        //     FIELD-INITIALIZER ctor (the case the no-op-only predicate DECLINED). `class W {
        //     int v = 7; }` has a synthesized-default ctor whose body is `super(); this.v = 7`
        //     (JLS §12.5). The object is torn apart, so the ctor's initializer is MATERIALIZED
        //     onto its slot — `this.v = 7` becomes `StoreLocal(slot_v, 7)` — and the alloc + ctor
        //     call are gone. RED before the materialization (the old predicate declined any chain
        //     that wrote `this`, so `scalar_total` never moved for an init-bearing ctor). The
        //     runtime value that the slot actually carries `7` is guarded by test_exec.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class W { int v = 7; }"
            " class T { static int g(){ W w = new W(); return w.v; } }",
            &a, &nlib);
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int w_id = sema_find_class(&sctx, "W"), t_id = sema_find_class(&sctx, "T");
        const sema_class_t* wsc = sema_get_class(&sctx, w_id);
        int obj_id = wsc ? wsc->super_id : -1;
        /* Summarize the ctor chain supers-first, so W.<init> sees a CLEAN Object.<init>. */
        for (int pass = 0; pass < 2; pass++) {
            int want = (pass == 0) ? obj_id : w_id;
            for (int k = 0; k < mc; k++) {
                if (ms[k]->class_id != want) continue;
                const sema_class_t* sc = sema_get_class(&sctx, want);
                if (sc && ms[k]->method_id >= 0 && ms[k]->method_id < (int)bbq_vec_len((void*)sc->methods)
                    && sc->methods[ms[k]->method_id].is_constructor)
                    sir_summarize(&cctx, k);
            }
        }
        int i_g = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "g")) i_g = k;
        CHECK(i_g >= 0, "§43: g resolves");
        int scalar_before = cctx.scalar_total;
        sir_optimize(&cctx, i_g);
        CHECK(cctx.scalar_total > scalar_before,
              "§43: a local `new W()` with a FIELD-INIT ctor is SCALAR-REPLACED — its `this.v = 7`"
              " materialized onto the slot, not declined");
        CHECK(find_new_of_class(ms[i_g]->entry, w_id) == NULL,
              "§43: the `new W()` is GONE from g's SIR — allocation removed, ctor materialized");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // §44 — scalar replacement FIRES on representative Java. The POINT is the optimization
        //       (a NoEscape object torn into slots, its ctor's initializers materialized), proved
        //       by compiling small programs — NOT by whether the jre happens to have the shape.
        //       Each row builds a local object, uses its fields, and must come out with the
        //       allocation GONE. compiler_summarize_to_convergence provides every ctor's summary
        //       exactly as the real driver does. Runtime values are guarded by test_exec.
        struct { const char* src; const char* cls; const char* meth; const char* label; } sr[] = {
          { "class W { int v = 7; }"
            " class T { static int g(){ W w = new W(); return w.v; } }",
            "W", "g", "§44: field-init synth-default ctor scalar-replaces" },
          { "class P { int x; P(int a){ x = a; } }"
            " class T { static int g(){ P p = new P(5); return p.x; } }",
            "P", "g", "§44: USER ctor with a param scalar-replaces (this.x = a → slot bound to arg)" },
          { "class Pt { int x, y; Pt(int a, int b){ x = a; y = b; } }"
            " class T { static int g(){ Pt p = new Pt(3, 4); return p.x * 10 + p.y; } }",
            "Pt", "g", "§44: multi-field user ctor scalar-replaces" },
          { "class A { int a = 3; } class B extends A { int b = 4; }"
            " class T { static int g(){ B o = new B(); return o.a + o.b; } }",
            "B", "g", "§44: super-chain field inits scalar-replace" },
        };
        for (int t = 0; t < (int)(sizeof(sr) / sizeof(sr[0])); t++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(sr[t].src, &a, &nlib);
  /* WHOLE-program: this block's checks need the java.lang bodies compiled —
   * the ctor chain and the §7 call-graph summaries that escape analysis reads
   * reach into the prelude, so analyze_from stays 0. */
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; /* analyze_from stays 0: see below */
            sir_analyze(&sctx);
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            compiler_summarize_to_convergence(&cctx);      /* every ctor summary, as the driver does */
            int cls = sema_find_class(&sctx, sr[t].cls);
            int t_id = sema_find_class(&sctx, "T"), mi = -1;
            for (int k = 0; k < mc; k++)
                if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, sr[t].meth)) mi = k;
            CHECK(mi >= 0, sr[t].label);
            if (mi < 0) { sema_destroy(&sctx); bbq_arena_free(&a); continue; }
            int before = cctx.scalar_total;
            sir_optimize(&cctx, mi);
            CHECK(cctx.scalar_total > before && find_new_of_class(ms[mi]->entry, cls) == NULL,
                  sr[t].label);
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    {
        // ── §36 THE ANALYSIS ORDER — reverse-topological over the call graph ─────
        //
        //     Choi §4: "iterate over the nodes in the call graph in a reverse topological
        //     order … we ignore back edges." The driver runs the per-method fixpoint in
        //     THIS order so a callee's summary exists before its caller is analyzed. It is
        //     a DFS POSTORDER over the (pinned) call graph — every callee before its caller
        //     — NOT an SCC/dominator structure (the forbidden list); cycles are the
        //     driver's convergence iteration's job, not a structure on the graph.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        /* Declared CALLER-FIRST on purpose: method INDEX order is top < mid < leaf, the
         * OPPOSITE of the required callee-first order. A correct postorder must actively
         * reverse it (→ leaf, mid, top); a broken PREORDER falls back to index order
         * (→ top, mid, leaf) and FAILS the assertion. Declaring callee-first (the obvious
         * way) makes index order already satisfy it, so preorder passes and the test has
         * no teeth — which is exactly what the first cut did. */
        ast_program_t* prog = build_program(
            "class T {"
            "   static int top(){ return mid(); }"     /* top → mid  */
            "   static int mid(){ return leaf(); }"    /* mid → leaf */
            "   static int leaf(){ return 1; }"
            " }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        CHECK(ms != NULL && mc > 0, "§36: the program compiles");
        compiler_build_callgraph(&cctx);

        int* order = (int*)bbq_arena_alloc(&a, (size_t)(mc > 0 ? mc : 1) * sizeof(int));
        int n = compiler_analysis_order(&cctx, order);
        CHECK(n == mc, "§36: the order lists every method");

        /* It is a PERMUTATION — every method index exactly once (no drops, no repeats). */
        int* seen = (int*)bbq_arena_alloc(&a, (size_t)(mc > 0 ? mc : 1) * sizeof(int));
        memset(seen, 0, (size_t)(mc > 0 ? mc : 1) * sizeof(int));
        bool perm = (n == mc);
        for (int i = 0; i < n; i++)
            if (order[i] < 0 || order[i] >= mc || seen[order[i]]++) perm = false;
        CHECK(perm, "§36: the order is a PERMUTATION of every method — no method dropped or "
                    "visited twice");

        /* The chain leaf ← mid ← top is acyclic, so the order must place each CALLEE before
         * its CALLER: pos[leaf] < pos[mid] < pos[top]. */
        int t_id = sema_find_class(&sctx, "T");
        int i_leaf = -1, i_mid = -1, i_top = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name) {
                if (!strcmp(ms[k]->name, "leaf")) i_leaf = k;
                if (!strcmp(ms[k]->name, "mid"))  i_mid  = k;
                if (!strcmp(ms[k]->name, "top"))  i_top  = k;
            }
        int p_leaf = -1, p_mid = -1, p_top = -1;
        for (int i = 0; i < n; i++) {
            if (order[i] == i_leaf) p_leaf = i;
            if (order[i] == i_mid)  p_mid  = i;
            if (order[i] == i_top)  p_top  = i;
        }
        CHECK(i_leaf >= 0 && i_mid >= 0 && i_top >= 0
              && p_leaf < p_mid && p_mid < p_top,
              "§36: reverse-topological — a callee is analyzed BEFORE its caller "
              "(leaf before mid before top)");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // §32.8 THE COPY CHAIN — a recorded divergence, settled by TEST, not argument.
        //
        //     The plan says a ref copy is not a use, and that once the field ops are
        //     rewritten "the copy's slot has no readers, so DSE removes it". The code does
        //     NOT rely on that: the struct pass (cp_pea) Nops the copies ITSELF, arguing DSE
        //     is one pass over one liveness solution, so in a chain t1→t2→q it deletes the
        //     outermost copy while the inner ones survive — still READING a slot whose def
        //     is gone. One of those two is wrong. This pin decides it.
        //
        //     It asserts the PROPERTY (JLS §16: no LoadLocal may read a slot nothing writes),
        //     never the mechanism — so it stays valid whichever side removes the copies.
        //
        //     t0 = new C; t1 = t0; t2 = t1; t2.f = 7; return t2.f;
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        build_program("class C { int f; } class T { }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        int c_id = sema_find_class(&sctx, "C");
        int t_id = sema_find_class(&sctx, "T");
        CHECK(c_id >= 0, "§32.8: class C resolves");

        sir_node_t* alloc = sir_new(&a, c_id);
        sir_node_t* ret   = sir_return(&a,
                                sir_get_field(&a, SIR_DTINT,
                                    sir_load_local(&a, 2, SIR_DTREF, sir_class_ref(&a, c_id)),
                                    c_id, 0),
                                SIR_DTINT);
        sir_node_t* put   = sir_put_field(&a, SIR_DTINT,
                                sir_load_local(&a, 2, SIR_DTREF, sir_class_ref(&a, c_id)),
                                c_id, 0, sir_load_const(&a, 7, SIR_DTINT), ret);
        sir_node_t* cp2   = sir_store_local(&a, 2, SIR_DTREF, sir_class_ref(&a, c_id),
                                sir_load_local(&a, 1, SIR_DTREF, sir_class_ref(&a, c_id)), put);
        sir_node_t* cp1   = sir_store_local(&a, 1, SIR_DTREF, sir_class_ref(&a, c_id),
                                sir_load_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id)), cp2);
        sir_node_t* entry = sir_store_local(&a, 0, SIR_DTREF, sir_class_ref(&a, c_id),
                                            alloc, cp1);
        sir_method_t* m = sir_method(&a, "g", t_id, 0, 3, entry);

        cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
        if (e) { cp_rewrite(e);      /* cp_rewrite ends in liveness + DSE — both are exercised */
                 CHECK(e->scalar_count == 1,
                       "§32.8: a site whose ref is COPIED THROUGH A CHAIN still qualifies — "
                       "a copy is the slot mechanism, not a use (spec §2)");
                 cp_free(e); }
        CHECK(count_tag(m->entry, SIR_NEW) == 0, "§32.8: the New is gone");
        CHECK(count_tag(m->entry, SIR_GETFIELD) == 0
           && count_tag(m->entry, SIR_PUTFIELD) == 0, "§32.8: the field ops became slot ops");
        CHECK(!reads_a_defless_slot(m->entry),
              "§32.8: …and NO LoadLocal is left reading a slot whose def was deleted — the "
              "copy chain is fully removed (JLS §16). THIS is the assertion that decides "
              "it fails the moment one link of t0→t1→t2 survives its own def");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    {
        // ── §33 spec §6's THROW RULE — only a real catch clause contains the exception ──
        //
        //     §6: "a `throw` marks its exception object escaping ONLY IF it can leave the
        //     method: when an enclosing handler PROVABLY CATCHES the thrown class the object
        //     never leaves; an uncaught (re-thrown) one is ArgEscape."
        //
        //     The DDCG ends EVERY try chain with a catch-all that RE-THROWS — it is how
        //     "matched no catch clause" propagates (JLS §11.3: an exception matching no catch
        //     clause of the try LEAVES the try, and the finally runs "during propagation").
        //     It is not a catch clause, and it used to be minted carrying Throwable, so
        //     `sema_ref_is_subtype(cls, Throwable)` said yes to everything and EVERY throw
        //     inside ANY try looked contained.
        //
        //     THE NODE SAYS WHICH IT IS. A handler with no declared type carries
        //     catch_class_id = -1 — the asdl's "no type info (finally / throwable-catch)", a
        //     field that exists (its words) "so downstream passes (Click type lattice) don't
        //     have to rediscover it". Its LANDING type is (ref null Throwable), but that is a
        //     representation fact and the type lattice owns it (lat_handler_landing_class) —
        //     it does not belong in the semantic field. Nothing here reads a fact row, and
        //     nothing reads the handler's BODY to see whether it re-throws.
        //
        //     HAND-BUILT, and not from source, for the reason §27/§32 are: `new E()` runs a
        //     ctor, so §7's bottom graph escapes the object anyway and a source-level pin
        //     would go green without the rule ever firing.
        //
        //     try { t1 = new E; throw t1; } <handlers per row>
        struct { int typed_class_of;   /* 0 = none, 1 = E, 2 = X, 3 = Throwable */
                 int want; const char* label; } tr[] = {
          { 0, CP_ESC_ARG,
            "§33.1: a throw whose try has ONLY the synthesized catch-all ESCAPES — the "
            "catch-all re-throws (JLS §11.3), so the object leaves the method" },
          { 1, CP_ESC_NONE,
            "§33.2: a throw caught by a REAL matching catch clause does NOT escape — §6's "
            "refinement survives (this is the anti-gutting control)" },
          { 2, CP_ESC_ARG,
            "§33.3: a real catch clause of an UNRELATED class does not catch it, and the "
            "catch-all behind it does not either — it escapes" },
          { 3, CP_ESC_NONE,
            "§33.4: a REAL `catch (Throwable)` written in the source DOES catch — the rule "
            "keys on the RECORDED ROW, never on the handler's class being Throwable" },
        };
        for (int i = 0; i < (int)(sizeof tr / sizeof tr[0]); i++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            build_program(
                "class E extends Exception { } class X extends Exception { } class T { }",
                &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            sir_analyze(&sctx);
            int e_id = sema_find_class(&sctx, "E");
            int x_id = sema_find_class(&sctx, "X");
            int t_id = sema_find_class(&sctx, "T");
            int th_id = sctx.wk.throwable_id;
            CHECK(e_id >= 0 && x_id >= 0 && th_id >= 0, "§33: E, X and Throwable resolve");

            /* the protected body: t1 = new E; throw t1; */
            sir_node_t* alloc_e = sir_new(&a, e_id);
            sir_node_t* thrw    = sir_throw(&a, sir_load_local(&a, 1, SIR_DTREF, NULL));
            sir_node_t* spill   = sir_store_local(&a, 1, SIR_DTREF,
                                                  sir_class_ref(&a, e_id), alloc_e, thrw);
            sir_node_t* try_start = sir_nop(&a, spill);

            /* The DDCG's outermost handler: no declared type (catch_class_id -1), body
             * re-throws. Its landing slot is still a Throwable ref — that is the ref
             * DESCRIPTOR's job, not the semantic field's. */
            sir_node_t* sentinel = sir_exception_entry(&a, 2, -1,
                sir_throw(&a, sir_load_local(&a, 2, SIR_DTREF,
                                             sir_class_ref(&a, th_id))));

            sir_node_t* chain = try_start;
            if (tr[i].typed_class_of) {
                int tc = tr[i].typed_class_of == 1 ? e_id
                       : tr[i].typed_class_of == 2 ? x_id : th_id;
                sir_node_t* typed = sir_exception_entry(&a, 3, tc, sir_return_void(&a));
                chain = sir_try_region(&a, typed, chain);
            }
            sir_node_t* outer = sir_try_region(&a, sentinel, chain);
            thrw->exc = outer;          /* what the DDCG's patch_excepts stamps */
            sir_method_t* m = sir_method(&a, "g", t_id, 0, 4, outer);

            cp_engine_t* e = cp_build(m, &sctx, &a, NULL, 0);
            CHECK(e != NULL, "§33: the hand-built try/throw method builds");
            if (e) {
                CHECK((int)cp_escape_of_expr(e, alloc_e) == tr[i].want, tr[i].label);
                cp_free(e);
            }
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    {
        // §33.5 …and the DDCG really does MINT it that way. Everything above is hand-built,
        //       so it pins the RULE while ASSUMING the premise: that a compiled try/catch
        //       produces a catch-all whose catch_class_id is -1 and typed handlers carrying
        //       their declared class. If the DDCG regressed to writing `throwable_id` there
        //       (which is what it used to do — the landing type belongs to the lattice, not
        //       the semantic field), every pin above would stay GREEN while the real
        //       compiler silently went back to "every throw in every try is contained".
        //       Pin the premise, from SOURCE.
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class E extends Exception { }"
            " class T { static void g() { try { throw new E(); } catch (E e) { } } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id >= nlib && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
        CHECK(mi >= 0, "§33.5: the try/catch method compiles");
        int e_id = sema_find_class(&sctx, "E");

        /* Walk the region's TryRegion chain: outermost is the synthesized catch-all, then
         * the typed catches. (The chain is the node's own structure — sir.asdl's
         * TryRegion(handler, next) — not a graph traversal.) */
        int n_catchall = 0, n_typed = 0, n_throwable_typed = 0;
        for (int i = 0; mi >= 0 && i < 1; i++) {
            const sir_node_t* n = ms[mi]->entry;
            while (n && n->tag != SIR_TRYREGION) n = sir_get_next((sir_node_t*)n);
            for (const sir_node_t* tr = n; tr && tr->tag == SIR_TRYREGION;
                 tr = tr->try_region.next) {
                const sir_node_t* h = tr->try_region.handler;
                if (!h || h->tag != SIR_EXCEPTIONENTRY) continue;
                int cc = h->exception_entry.catch_class_id;
                if (cc < 0) n_catchall++;
                else {
                    n_typed++;
                    if (cc == sctx.wk.throwable_id) n_throwable_typed++;
                    CHECK(cc == e_id, "§33.5: the typed handler carries its DECLARED class");
                }
            }
        }
        CHECK(n_catchall == 1,
              "§33.5: the DDCG mints EXACTLY ONE catch-all, and it carries catch_class_id "
              "-1 — the asdl's 'no type info (finally / throwable-catch)'");
        CHECK(n_typed == 1, "§33.5: …and exactly one typed catch clause");
        CHECK(n_throwable_typed == 0,
              "§33.5: the catch-all is NOT minted as a catch of Throwable — that is what "
              "made sema_ref_is_subtype say 'caught' to every throw in every try");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    if (getenv("SIR_DUMP"))
        dbg_dump_method(
            getenv("SIR_DUMP_SRC") ? getenv("SIR_DUMP_SRC") :
            "class T { int cur; int max; boolean ret;"
            " void skip() { while (!ret && cur < max && (cur % 3) == 0) {"
            " cur = cur + 1; } } }",
            getenv("SIR_DUMP_METHOD") ? getenv("SIR_DUMP_METHOD") : "skip");

    /* ── MemSize in the fixpoint — Mem guard merging. Each Mem access
     * carries a 2-branch guard chain (addr < 0;
     * addr+width > memory.size*64Ki). MemSize reads are congruent BETWEEN
     * kills (the memsize cell's follower rule), so two identical adjacent
     * loads share one guard — and a grow or a call between them is a kill,
     * so the soundness negatives MUST keep both. */
    printf("== Mem guard merging (MemSize in the fixpoint) ==\n");
    {
        /* The address is a PARAMETER — a constant one lets SCCP fold the
         * `addr < 0` arms on its own and the counts stop measuring the
         * memsize machinery (the first draft of these pins passed that way). */
        #define SIMDI "import javelina.simd.*; "
        const char* TWO = SIMDI
            "class T { static int f(int p){ return Mem.i32_load(p) + Mem.i32_load(p); } }";
        CHECK(compile_count_in(TWO, "f", SIR_BRANCH, 0) == 4,
              "unoptimized: two loads carry two full guard chains (4 branches)");
        /* The gate: the second access's WHOLE guard chain folds. The lo
         * arm folds on the slot's p ≥ 0 range; the hi arm folds by the §15
         * array mechanism — the guard is emitted in the ARRAY SHAPE
         * (`(long)p > limit − w`, tested side = the slot through one I2L),
         * guard 1's fall-through records the inclusive symbolic bound on p,
         * and the two limit expressions are congruent through the MemSize
         * memory-input keying. NOT condition congruence: the lo-guard's
         * range Refine is a partition Leader, so the two hi-CONDITIONS are
         * never congruent — the bound expression is p-free, which is the
         * whole point of the shape. */
        CHECK(compile_count_in(TWO, "f", SIR_BRANCH, 1) == 2,
              "the second load's lo AND hi guards both fold");
        const char* GROW = SIMDI
            "class T { static int f(int p){ int a = Mem.i32_load(p);"
            " Mem.memory_grow(1); return a + Mem.i32_load(p); } }";
        /* 3, not 4: the SECOND `p < 0` arm folds via the first's range refine
         * on p — sound across any kill (p is untouched). The two hi-arms
         * (the MemSize compares) are what the kill must preserve. */
        { int g = compile_count_in(GROW, "f", SIR_BRANCH, 1);
          if (g != 3) printf("    (grow-between counted %d branches, want 3)\n", g);
          CHECK(g == 3,
              "SOUNDNESS: memory_grow between the loads keeps BOTH hi-guards"); }
        /* The callee CAN grow (conditionally) — a summary must treat it as a
         * memsize writer; an empty callee would be LEGITIMATELY seen through. */
        const char* CALL = SIMDI
            "class T { static int g(int x){ if (x > 0) Mem.memory_grow(1); return 0; }"
            " static int f(int p){ int a = Mem.i32_load(p); g(p);"
            " return a + Mem.i32_load(p); } }";
        { int c = compile_count_in(CALL, "f", SIR_BRANCH, 1);
          if (c != 3) printf("    (call-between counted %d branches, want 3)\n", c);
          CHECK(c == 3,
              "SOUNDNESS: a growing call between the loads keeps BOTH hi-guards"); }
        /* mem_range_guard (fill/copy) carries a 3-branch chain (base < 0;
         * len < 0; (long)base > limit − len) in the SAME §15 array shape as
         * the load guard above, so two adjacent fills fold the SECOND chain
         * whole: its lo and len arms off the first's base >= 0 / len >= 0, and
         * its hi off the symbolic bound the first hi's ok edge recorded. The
         * ddcg re-spills base and len into fresh temps before each fill, and
         * that copy carries the refinement (a redefinition holding the same
         * value keeps its proven facts). Owning-level pins:
         * test_cp_memrange_second_full_chain_folds and
         * test_cp_refine_survives_a_copying_redefinition. */
        const char* FILL2 = SIMDI
            "class T { static void f(int x, int n){ Mem.memory_fill(x, 0, n);"
            " Mem.memory_fill(x, 0, n); } }";
        CHECK(compile_count_in(FILL2, "f", SIR_BRANCH, 0) == 6,
              "unoptimized: two fills carry two 3-branch range guards");
        { int r = compile_count_in(FILL2, "f", SIR_BRANCH, 1);
          if (r != 3) printf("    (two-fill counted %d branches, want %d)\n", r, 3);
          CHECK(r == 3,
              "the second fill's WHOLE range guard folds (lo, len, and the §15-shaped hi)"); }
        #undef SIMDI
    }

    /* ── Same-input rules — x-x/x^x fold to 0 and integer cmp(v,v)
     * decides, with the value UNKNOWN. Float is the soundness negative
     * (NaN: x==x is false; inf-inf is NaN). */
    printf("== same-input rules ==\n");
    {
        const char* SI = "class T { static int f(int x){ return (x - x) + (x ^ x); } }";
        CHECK(compile_count_in(SI, "f", SIR_SUB, 0) == 1 &&
              compile_count_in(SI, "f", SIR_XOR, 0) == 1,
              "unoptimized: the sub and xor are present");
        CHECK(compile_count_in(SI, "f", SIR_SUB, 1) == 0 &&
              compile_count_in(SI, "f", SIR_XOR, 1) == 0,
              "x-x and x^x fold to 0 with x unknown");
        const char* CB = "class T { static int f(int x){ if (x == x) return 1; return 2; } }";
        CHECK(compile_count_in(CB, "f", SIR_BRANCH, 1) == 0,
              "integer x==x folds TRUE and its branch deletes");
        const char* FB = "class T { static int f(float x){ if (x == x) return 1; return 2; } }";
        CHECK(compile_count_in(FB, "f", SIR_BRANCH, 1) == 1,
              "SOUNDNESS: float x==x does NOT fold (NaN)");
    }

    /* ── Condition-verdict identity facts. A branch whose
     * condition VALUE was already decided on every surviving path folds;
     * the verdict is matched by value identity (cp_value_leader), and a
     * diamond's rejoin intersects the arms' contradictory verdicts away. */
    printf("== condition-verdict identity ==\n");
    {
        /* Exit shape: branch 1's true arm returns, so the fall-through
         * carries (c == false) to branch 2, which folds to its else arm. */
        const char* EX = "class T { static int f(int x){ boolean c = x > 3; "
                         "if (c) return 9; int r = 1; if (c) r = 2; return r; } }";
        CHECK(compile_count_in(EX, "f", SIR_BRANCH, 0) == 2,
              "unoptimized: both branches on c are present");
        CHECK(compile_count_in(EX, "f", SIR_BRANCH, 1) == 1,
              "exit shape: the second branch on c folds via its verdict");
        /* Diamond: branch 1's arms REJOIN, so (c==true) meets (c==false)
         * at the merge and no verdict survives to branch 2. */
        const char* DI = "class T { static int g(int x){ boolean c = x > 3; int r = 0; "
                         "if (c) r = 1; if (c) r += 2; return r; } }";
        CHECK(compile_count_in(DI, "g", SIR_BRANCH, 1) == 2,
              "SOUNDNESS: after a rejoined diamond no verdict survives");
    }

    /* ── §4.6 cong_fold partition channel: two loads that share an opcode
     * bucket until CAUSE_SPLITS separates them (a[i] vs b[i]) must NOT
     * same-input-fold — the transient coarse congruence has to be walked
     * back by the partition-consumer re-arm (the BitSet.xor miscompile). */
    printf("== cong_fold transient-congruence soundness ==\n");
    {
        const char* AB = "class T { static void f(int[] a, int[] b) { "
                         "for (int i = 0; i < b.length; i = i + 1) a[i] = a[i] ^ b[i]; } }";
        CHECK(compile_count_in(AB, "f", SIR_XOR, 1) == 1,
              "SOUNDNESS: a[i] ^ b[i] never folds (distinct arrays)");
    }

    /* ── Strides live end-to-end (Click §4.5). MUL/SHL PRODUCE a
     * strided range, DIV carries it through, REM/AND consume it to a KNOWN,
     * and EQ consumes stride disjointness by gcd. Each positive needs the
     * whole chain; the negatives pin where the claim must stop. */
    printf("== range strides produce + consume ==\n");
    {
        const char* MA = "class T { static int f(int x){ if (x < 0 || x > 100) return 0; "
                         "int i = x * 4; if ((i & 3) == 0) return 1; return 2; } }";
        CHECK(compile_count_in(MA, "f", SIR_BRANCH, 0) == 3,
              "unoptimized: guard pair + mask test present");
        CHECK(compile_count_in(MA, "f", SIR_BRANCH, 1) == 2,
              "MUL stride 4 -> (i & 3) == 0 folds TRUE");
        const char* SR = "class T { static int g(int x){ if (x < 0 || x > 100) return 0; "
                         "int i = x << 3; if (i % 8 == 0) return 1; return 2; } }";
        CHECK(compile_count_in(SR, "g", SIR_BRANCH, 1) == 2,
              "SHL stride 8 -> (i % 8) == 0 folds TRUE");
        const char* DR = "class T { static int h(int x){ if (x < 0 || x > 100) return 0; "
                         "int i = x * 4; int j = i / 2; if (j % 2 == 0) return 1; return 2; } }";
        CHECK(compile_count_in(DR, "h", SIR_BRANCH, 1) == 2,
              "DIV keeps stride 4/2=2 -> (j % 2) == 0 folds TRUE");
        const char* EQ = "class T { static int e(int x, int y){ "
                         "if (x < 0 || x > 50) return 0; if (y < 0 || y > 50) return 0; "
                         "int a = x * 4; int b = y * 6 + 1; if (a == b) return 9; return 1; } }";
        CHECK(compile_count_in(EQ, "e", SIR_BRANCH, 0) == 5 &&
              compile_count_in(EQ, "e", SIR_BRANCH, 1) == 4,
              "gcd(4,6)=2 base parity disagrees -> a == b folds FALSE");
        /* Negatives: a mask wider than the stride proves nothing; an
         * unbounded multiplicand overflows the corner products so the
         * whole chain must stay BOTTOM. */
        const char* NM = "class T { static int n(int x){ if (x < 0 || x > 100) return 0; "
                         "int i = x * 4 + 2; if ((i & 7) == 0) return 1; return 2; } }";
        CHECK(compile_count_in(NM, "n", SIR_BRANCH, 1) == 3,
              "SOUNDNESS: stride 4 cannot decide (i & 7)");
        const char* NO = "class T { static int o(int x){ if (x > 100) return 0; "
                         "int i = x * 4; if (i % 4 == 0) return 1; return 2; } }";
        CHECK(compile_count_in(NO, "o", SIR_BRANCH, 1) == 2,
              "SOUNDNESS: unbounded x -> x*4 overflows, no stride claim");
    }

    /* ── Spec §5 CONSUMER rows, pinned row by row against the doc (the
     * transfer rows are covered above and by the machinery tests): drop
     * /,% by-zero when the divisor range excludes 0; drop the INT_MIN/-1
     * wrap arm when the range excludes -1; drop NegativeArraySize when
     * the size range is ≥ 0; array.len ⟹ [0,∞); `==` narrows the taken
     * edge. Each guard is emitted slot-vs-constant, so these ride the
     * constant-side refine + range compare folds. */
    printf("== spec §5: guard-consumer coverage ==\n");
    {
        const char* DZ = "class T { static int f(int x){ if (x < 1 || x > 100) return 0; "
                         "return 1000 / x + 1000 % x; } }";
        CHECK(compile_count_in(DZ, "f", SIR_BRANCH, 0) == 5,
              "unoptimized: 2 range checks + div zero/-1 arms + rem zero arm");
        CHECK(compile_count_in(DZ, "f", SIR_BRANCH, 1) == 2,
              "divisor in [1,100]: by-zero AND -1-wrap guards fold");
        const char* DK = "class T { static int g(int x){ if (x < -5 || x > 100) return 0; "
                         "return 1000 / x; } }";
        CHECK(compile_count_in(DK, "g", SIR_BRANCH, 1) == 4,
              "SOUNDNESS: divisor range spans 0 and -1 -> both arms stay");
        /* Unoptimized 3 = x<0 + NegativeArraySize + the NPE guard on
         * a.length. Optimized 1 = only x<0 survives: the size guard folds
         * on x's [0,MAX] refine (§5) and the NPE guard folds on the fresh
         * allocation's NonNull (§4). */
        const char* NS = "class T { static int h(int x){ if (x < 0) return 9; "
                         "int[] a = new int[x]; return a.length; } }";
        CHECK(compile_count_in(NS, "h", SIR_BRANCH, 0) == 3 &&
              compile_count_in(NS, "h", SIR_BRANCH, 1) == 1,
              "size range >= 0: the NegativeArraySize guard folds");
        /* Unoptimized 2 = the NPE guard on a.length + n<0. Optimized 1:
         * n<0 folds on array.len's [0,MAX] transfer; the NPE guard on the
         * NULLABLE param correctly STAYS — folding it would be the unsound
         * direction. */
        const char* AL = "class T { static int k(int[] a){ int n = a.length; "
                         "if (n < 0) return 9; return 1; } }";
        CHECK(compile_count_in(AL, "k", SIR_BRANCH, 0) == 2 &&
              compile_count_in(AL, "k", SIR_BRANCH, 1) == 1,
              "array.len is [0, MAX): a negative test on it folds; the param NPE guard stays");
        const char* EQ7 = "class T { static int e(int x){ if (x == 7) return 100 / x; "
                          "return 1; } }";
        CHECK(compile_count_in(EQ7, "e", SIR_BRANCH, 0) == 3 &&
              compile_count_in(EQ7, "e", SIR_BRANCH, 1) == 1,
              "== narrows the taken edge: x KNOWN 7 folds the div guards");
    }

    /* ── Two-lattice parity (the Click side of the plan's audit table; the
     * sema twins live in test_sema's "interval parity" block — a rule
     * added to one side must be added to the other or its twin goes red).
     * Each pin folds a branch only the generalized rule can decide. */
    printf("== interval parity: mask sign / div-rem ranges / i2c ==\n");
    {
        const char* MS = "class T { static int f(int x){ if (x > 100) return 0; "
                         "if ((x & 7) >= 0) return 1; return 2; } }";
        CHECK(compile_count_in(MS, "f", SIR_BRANCH, 1) == 1,
              "mask rule holds for a SIGN-SPANNING x: (x & 7) >= 0 folds TRUE");
        const char* DR = "class T { static int g(int d){ if (d < 2 || d > 9) return 0; "
                         "int q = 1000 / d; if (q <= 500) return 1; return 2; } }";
        CHECK(compile_count_in(DR, "g", SIR_BRANCH, 1) == 2,
              "div by a RANGE divisor [2,9]: q is [111,500], q <= 500 folds TRUE");
        const char* RS = "class T { static int h(int x){ if (x > -1) return 0; "
                         "int r = x % 8; if (r <= 0) return 1; return 2; } }";
        CHECK(compile_count_in(RS, "h", SIR_BRANCH, 1) == 1,
              "rem of a NEGATIVE dividend: x%8 is [-7,0], r <= 0 folds TRUE");
        const char* CC = "class T { static int e(int x){ if (x < 0 || x > 60000) return 0; "
                         "char c = (char) x; if (c <= 60000) return 1; return 2; } }";
        CHECK(compile_count_in(CC, "e", SIR_BRANCH, 1) == 2,
              "(char)x of a FITTING range passes through: c <= 60000 folds TRUE");
    }

    /* ── §4.8 idempotent same-input followers: x&x and x|x ARE x (a
     * follower transition — the result forwards, no constant is minted).
     * Identity-matched only; distinct operands must survive. */
    printf("== idempotent same-input followers ==\n");
    {
        const char* ID = "class T { static int f(int x){ return (x & x) + (x | x); } }";
        CHECK(compile_count_in(ID, "f", SIR_AND, 0) == 1 &&
              compile_count_in(ID, "f", SIR_OR, 0) == 1,
              "unoptimized: the and and or are present");
        CHECK(compile_count_in(ID, "f", SIR_AND, 1) == 0 &&
              compile_count_in(ID, "f", SIR_OR, 1) == 0,
              "x&x and x|x forward to x");
        const char* NE = "class T { static int g(int x, int y){ return (x & y) + (x | y); } }";
        CHECK(compile_count_in(NE, "g", SIR_AND, 1) == 1 &&
              compile_count_in(NE, "g", SIR_OR, 1) == 1,
              "SOUNDNESS: x&y and x|y survive (distinct operands)");
    }

    /* ── x==y refines the symbolic LOWER bound (lo_vn1), the mirror of the
     * `i < a.length` upper-bound fold. On the `x == y` true edge x inherits y as
     * both an inclusive upper and lower bound, so a `x < y` guard inside proves
     * false (x >= y) and its branch folds. A general shape — x and y are ordinary
     * user params, not the RTL. */
    printf("== x==y refines the symbolic lower bound ==\n");
    {
        const char* EQ = "class T { static int f(int x, int y){ if (x == y) { if (x < y) return 1; return 2; } return 3; } }";
        CHECK(compile_count_in(EQ, "f", SIR_BRANCH, 0) == 2,
              "unoptimized: both the == and the inner < branch are present");
        CHECK(compile_count_in(EQ, "f", SIR_BRANCH, 1) == 1,
              "x==y folds the inner x<y branch via lo_vn1 (x >= y)");
        /* SOUNDNESS: with no ==, x<y is a real test on unrelated values — keep it. */
        const char* NEG = "class T { static int f(int x, int y){ if (x < y) return 1; return 2; } }";
        CHECK(compile_count_in(NEG, "f", SIR_BRANCH, 1) == 1,
              "SOUNDNESS: a bare x<y never folds");
    }

    /* ── Memory DSE (W9d): a store nothing can observe goes; every way of
     * observing one keeps it. The negatives ARE the liveness proof — each
     * names one channel through which a later reader reaches the store. */
    printf("== memory DSE ==\n");
    {
        const char* OVER = "class C { int f; }"
            " class T { static void g(C o){ o.f = 1; o.f = 2; } }";
        CHECK(compile_count_in(OVER, "g", SIR_PUTFIELD, 0) == 2,
              "unoptimized: both stores are present");
        CHECK(compile_count_in(OVER, "g", SIR_PUTFIELD, 1) == 1,
              "o.f = 1 is dead: o.f = 2 overwrites the same location, unread");
        /* SOUNDNESS: same CELL, different RECEIVER — the cell key is
         * (class, field), so this is the aliasing case a cell-only rule gets
         * wrong. Nothing overwrites anything here. */
        /* FRESH receivers, so no null-guard sits between the stores to keep the
         * first alive for an unrelated reason — the must-alias test is then the
         * only thing preventing the deletion, and this count moves if it breaks. */
        const char* TWOOBJ = "class C { int f; }"
            " class T { static void g(){ C a = new C(); C b = new C(); a.f = 1; b.f = 2; } }";
        CHECK(compile_count_in(TWOOBJ, "g", SIR_PUTFIELD, 1) == 2,
              "SOUNDNESS: a.f and b.f share a cell but not a location — both stay");
        /* SOUNDNESS: a read between them observes the first value. */
        const char* READ = "class C { int f; }"
            " class T { static int g(C o){ o.f = 1; int r = o.f; o.f = 2; return r; } }";
        CHECK(compile_count_in(READ, "g", SIR_PUTFIELD, 1) == 2,
              "SOUNDNESS: a read between the stores keeps the first");
        /* SOUNDNESS: a CALL between them may read the field. */
        const char* CALL = "class C { int f; }"
            " class T { static void h(){} static void g(C o){ o.f = 1; h(); o.f = 2; } }";
        CHECK(compile_count_in(CALL, "g", SIR_PUTFIELD, 1) == 2,
              "SOUNDNESS: a call between the stores may read the field — both stay");
        /* SOUNDNESS: a lone store is never dead — the object outlives the frame. */
        const char* LONE = "class C { int f; }"
            " class T { static void g(C o){ o.f = 1; } }";
        CHECK(compile_count_in(LONE, "g", SIR_PUTFIELD, 1) == 1,
              "SOUNDNESS: a lone store escapes with the object — never dead");
        /* SOUNDNESS: overwritten only on ONE arm — the other path's value is
         * observable, so the version reaches a cell-φ and is not dead. */
        const char* COND = "class C { int f; }"
            " class T { static void g(C o, boolean c){ o.f = 1; if (c) o.f = 2; } }";
        CHECK(compile_count_in(COND, "g", SIR_PUTFIELD, 1) == 2,
              "SOUNDNESS: an overwrite on only one arm keeps the first store");
        /* Statics have no receiver: the cell IS the location. */
        const char* ST = "class T { static int s;"
            " static void g(){ s = 1; s = 2; } }";
        CHECK(compile_count_in(ST, "g", SIR_PUTSTATIC, 0) == 2,
              "unoptimized: both static stores present");
        CHECK(compile_count_in(ST, "g", SIR_PUTSTATIC, 1) == 1,
              "a static store overwritten before any read is dead");
        /* SOUNDNESS: array elements are a MONOLITHIC cell — a[0] and a[1] share
         * it, so a second ArrayStore proves nothing about the first's index. */
        const char* ARR = "class T { static void g(int[] a){ a[0] = 1; a[1] = 2; } }";
        CHECK(compile_count_in(ARR, "g", SIR_ARRAYSTORE, 1) == 2,
              "SOUNDNESS: array elements share one cell — neither store is proved dead");
    }

    /* ── cp_solve returns AT a fixpoint ───────────────────────────────────────────────────
     *
     * Arm every node and run one more round: a fixpoint is idempotent, so a value that moves
     * was left un-recomputed because its transfer read a fact reaching it by no def-use edge.
     * Distinct from "the worklists are empty", which says only that no def-use edge is
     * pending — the case at issue leaves no edge to be pending.
     *
     * Swept over every compiled method, not a named list: a whitelist rots as soon as a new
     * shape appears. */
    printf("== cp_solve returns at a fixpoint ==\n");
    {
        bbq_arena* arena = sess_arena();
        int nlib = 0;
        ast_program_t* prog = build_program("class FxP { static int g(){ return 1; } }",
                                            arena, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, arena);
        /* analyze_from 0, so the PRELUDE bodies are analyzed and compiled too. The rest of
         * this suite starts at nlib because it only inspects user code; here the prelude is
         * the corpus — it is what javelinac compiles and where the loop and Follower shapes
         * live. */
        sctx.num_library_classes = nlib; sctx.analyze_from = 0;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, arena, &sctx);
        int mc = 0;
        sir_method_t** methods = compiler_compile(&cctx, prog, &mc);

        /* Through cp_build_ctx, the entry sir_optimize uses. cp_build takes no sema and no
         * ddcg facts, so an engine built with it is not in the configuration production runs
         * — no recorded scopes, no §15 guards, no regions. */
        int checked = 0, stale = 0;
        char first[160] = {0};
        for (int i = 0; i < mc; i++) {
            int fc = 0;
            const compiler_fact_t* facts = compiler_get_facts(&cctx, i, &fc);
            cp_engine_t* e = cp_build_ctx(&cctx, methods[i], facts, fc);
            if (e) {
                checked++;
                if (!cp_at_fixpoint(e)) {
                    stale++;
                    if (!first[0])
                        snprintf(first, sizeof first, "%s (class %d)",
                                 methods[i]->name ? methods[i]->name : "?",
                                 methods[i]->class_id);
                }
                cp_free(e);
            }
        }
        /* The count is asserted, not just printed: a sweep is only as good as its coverage,
         * and one that examined a handful of methods would pass forever. */
        printf("    swept %d methods, %d not at a fixpoint%s%s\n",
               checked, stale, stale ? "; first: " : "", stale ? first : "");
        CHECK(checked > 200 && stale == 0,
              "every method's solve returns AT a fixpoint — one more armed round moves nothing");
    }

    /* ── Dybvig Figure 8 case 1 covers EVERY literal, not just the int-family ones ──
     * `is_simple_operand` classifies the side-effect-free leaves; its doc comment says
     * "literal", but it listed only AST_INTLIT/BOOLLIT/NULLLIT — a JCVM inheritance,
     * that VM having no long/float/double. So `d > 2.0f` matched no specialised case
     * (case 1 needs both operands simple; case 2 needs a CONSTANT lhs; case 3 needs a
     * COMPLEX lhs) and fell through to case 4, spilling BOTH operands for a comparison
     * that needs no temp at all.
     *
     * That spill is what made it a bug rather than a wart: `record_scope(test, Ljoin, 0)`
     * keys the if-join on the condition's head, which the spill makes a StoreLocal, and
     * Click then DSEs that dead temp — exactly what case 1 exists to avoid. With the
     * keyed node gone the join is unreachable, `ljoin` falls back to the enclosing
     * region, and both arms re-emit the method's whole tail: 2^k for k such ifs (jre
     * Math.pow was 6.6x its -O0 size).
     *
     * Pinned HERE, on the ddcg's own output at opt=0, because that is the level that
     * owns it: the defect is "the literal spilled", one node deep. Module size under -O
     * is three layers downstream and only a symptom. */
    {
        const sir_node_t* b; const sir_node_t* c;

        b = compile_find("class T { void g(){} void f(int d)    { if (d > 2)    g(); } }",
                         SIR_BRANCH, 0);
        c = b ? sir_child(b, 0) : NULL;
        CHECK(c && (int)c->tag == SIR_GT && sir_child(c, 1)
                && (int)sir_child(c, 1)->tag == SIR_LOADCONST,
              "int literal inlines into the compare (Figure 8 case 1, no spill temp)");

        b = compile_find("class T { void g(){} void f(long d)   { if (d > 2L)   g(); } }",
                         SIR_BRANCH, 0);
        c = b ? sir_child(b, 0) : NULL;
        CHECK(c && (int)c->tag == SIR_GT && sir_child(c, 1)
                && (int)sir_child(c, 1)->tag == SIR_LOADLONGCONST,
              "long literal inlines into the compare (no spill temp)");

        b = compile_find("class T { void g(){} void f(float d)  { if (d > 2.0f) g(); } }",
                         SIR_BRANCH, 0);
        c = b ? sir_child(b, 0) : NULL;
        CHECK(c && (int)c->tag == SIR_GT && sir_child(c, 1)
                && (int)sir_child(c, 1)->tag == SIR_LOADFLOATCONST,
              "float literal inlines into the compare (no spill temp)");

        b = compile_find("class T { void g(){} void f(double d) { if (d > 2.0)  g(); } }",
                         SIR_BRANCH, 0);
        c = b ? sir_child(b, 0) : NULL;
        CHECK(c && (int)c->tag == SIR_GT && sir_child(c, 1)
                && (int)sir_child(c, 1)->tag == SIR_LOADDOUBLECONST,
              "double literal inlines into the compare (no spill temp)");

        /* A char literal is an int-family constant leaf and inlines the same way —
         * `c == 'x'` is the jre's commonest comparison shape. */
        b = compile_find("class T { void g(){} void f(char d)   { if (d > 'x')  g(); } }",
                         SIR_BRANCH, 0);
        c = b ? sir_child(b, 0) : NULL;
        CHECK(c && (int)c->tag == SIR_GT && sir_child(c, 1)
                && (int)sir_child(c, 1)->tag == SIR_LOADCONST,
              "char literal inlines into the compare (no spill temp)");
    }

    /* ── §15 IDX_HIGH dies in DOWN-counting loops too ────────────────────────
     * The up-count (`i < a.length` then a[i]) already folds: the branch
     * refinement mints the symbolic bound and the §15 consumer reads it. The
     * DOWN-count (`i = a.length - 1; i >= 0; i--`) has no refinement carrying
     * the UPPER bound — it must come through the VALUE path: Sub(len,1) MINTS
     * `< len`, the decrement PRESERVES it, the header φ/widen carries agreeing
     * bounds through. Oracle = the recorded GUARD row: an eliminated guard's
     * key was retagged away from SIR_BRANCH (the same fact the census counts). */
    {
        struct { const char* src; const char* m; int want_surviving; const char* what; } cases[] = {
          { "class T { static int f(int[] a){ int h=0;"
            "  for (int i = a.length - 1; i >= 0; i--) h += a[i];"
            "  return h; } }", "f", 0,
            "down-count for (i-- Inc): IDX_HIGH dies" },
          { "class T { static int f(int[] a){ int h=0; int i = a.length - 1;"
            "  while (i >= 0) { h += a[i]; i = i - 1; }"
            "  return h; } }", "f", 0,
            "down-count while (i = i - 1 Sub): IDX_HIGH dies" },
          /* trim's shape — needs the arraylength self-bound, which in turn needs
           * the SPLIT to compare VALUE identity without the symbolic bounds
           * (cp_const_value_eq): a per-node bound must not split congruent
           * length reads. The up-count/new-int[]/§46 pins earlier in this suite
           * are the other half of the proof — they are exactly what regressed
           * when the seed landed WITHOUT the comparator split. */
          { "class T { static int f(int[] a){"
            "  int len = a.length; int st = 0;"
            "  while (st < len && a[len - 1] > 3) len = len - 1;"
            "  return len; } }", "f", 0,
            "decrement of a length-seeded var, access at len-1 (trim's shape): IDX_HIGH dies" },
          /* lastIndexOf's shape — the GE-false refinement (`from >= a.length`
           * fell through ⟹ from < len), minted for USER compares only: §15
           * guards are skipped by the recorded-guard set, because a guard's
           * fall-through fact is the guard machinery's own job and each guard
           * reads the length through its OWN node — minting there made
           * same-meaning refines content-DIFFERENT (distinct congruent length
           * vnodes in the predicate) and broke merge all-agreement. The §46 and
           * r[i]=a[i] pins earlier in this suite are the no-regression proof. */
          { "class T { static int f(int[] a, int from){"
            "  int n = a.length;"
            "  int i = from >= n ? n - 1 : from;"
            "  while (i >= 0) { if (a[i] == 7) return i; i = i - 1; }"
            "  return -1; } }", "f", 0,
            "ternary-seeded down-count, shared length read: IDX_HIGH dies" },
          /* lastIndexOf's TWO-READ form: the source names `a.length` TWICE, so
           * the mint (Sub over read 2) and the refine (GE-false over read 1)
           * store DIFFERENT bound ids for one value. The reads are CONGRUENT —
           * same partition — and the φ's meet counts two ids as one when their
           * vnodes are partition-equal. That agreement is optimistic (a split
           * can separate them), so it is a recorded premise: the φ is a §4.7.4
           * other.def_use reader of both bound vnodes and a split re-arms it.
           * The retraction half is pinned at its owning level, in
           * test_click_partition. */
          { "class T { static int f(int[] a, int from){"
            "  int i = from >= a.length ? a.length - 1 : from;"
            "  while (i >= 0) { if (a[i] == 7) return i; i = i - 1; }"
            "  return -1; } }", "f", 0,
            "ternary-seeded down-count, TWO congruent length reads: IDX_HIGH dies" },
          /* NEGATIVE controls — a false eliminate is a miscompile the census
           * cannot see; these must KEEP their guard forever. */
          /* The two reads name DIFFERENT arrays, so they are not congruent and
           * the partition agreement must not fire: b's length says nothing
           * about an index into a. */
          { "class T { static int f(int[] a, int[] b, int from){"
            "  int i = from >= a.length ? b.length - 1 : from;"
            "  while (i >= 0) { if (a[i] == 7) return i; i = i - 1; }"
            "  return -1; } }", "f", 1,
            "two-read seed over TWO arrays (b.length - 1 under a's refine): "
            "IDX_HIGH SURVIVES" },
          /* Same VARIABLE, two reaching definitions: `a = c` between the reads
           * makes read 2 a length of c. Syntactic sameness is not congruence —
           * the agreement is a partition test, and these two are in different
           * partitions. */
          { "class T { static int f(int[] a, int[] c, int from){"
            "  int n = a.length; a = c; int h = 0;"
            "  int i = from >= n ? a.length - 1 : from;"
            "  while (i >= 0) { h += a[i]; i = i - 1; }"
            "  return h; } }", "f", 1,
            "two-read seed with the array slot REASSIGNED between the reads: "
            "IDX_HIGH SURVIVES" },
          { "class T { static int f(int[] a){"
            "  int len = a.length; int h = 0;"
            "  while (len >= 0) { h += a[len]; len = len - 1; }"
            "  return h; } }", "f", 1,
            "access AT the length-seeded var (a[len], len = a.length): IDX_HIGH SURVIVES" },
          { "class T { static int f(int[] a){ int h=0;"
            "  for (int i = a.length; i >= 0; i--) h += a[i];"
            "  return h; } }", "f", 1,
            "off-by-one start (i = a.length): IDX_HIGH SURVIVES" },
          { "class T { static int f(int[] a, int[] b){ int h=0;"
            "  for (int i = a.length - 1; i >= 0; i--) h += b[i];"
            "  return h; } }", "f", 1,
            "other array (b[i] under a's bound): IDX_HIGH SURVIVES" },
          /* ── A SURVIVING check constrains the code after it ────────────────
           * ABCD's C5 row (Table 1, p.6): `check A[v]` falling through gives
           * `v ≤ A.length − 1`, and it is what makes a repeated access
           * subsume its own guard (their §7.2). The fact rides the ordinary
           * per-edge refinement — a guard branch is a compare like any other,
           * and its false edge already means `idx < len`.
           *
           * The paper's own warning is the first negative below: the
           * constraint must name the π's RESULT, never its operand, "otherwise
           * it could erroneously lead to elimination of some bound checks,
           * including the check itself" (§3). Here the Refine IS that new
           * name, and the branch's condition reads the operand, so a guard
           * cannot fold itself — pinned, not assumed. */
          { "class T { static int f(int[] a, int i){ return a[i] + a[i]; } }",
            "f", 1,
            "second access to the SAME index subsumes its guard (C5): one "
            "IDX_HIGH survives, not two" },
          { "class T { static int f(int[] a, int i){ return a[i]; } }",
            "f", 1,
            "a guard NEVER proves itself — the lone access keeps its IDX_HIGH" },
          { "class T { static int f(int[] a, int[] b, int i){"
            "  return a[i] + b[i]; } }", "f", 2,
            "different arrays: the first check says nothing about b — BOTH "
            "IDX_HIGH survive" },
          { "class T { static int f(int[] a, int i){"
            "  int s = a[i]; i = i + 1; return s + a[i]; } }", "f", 2,
            "index redefined between the accesses (i+1 can reach length): "
            "BOTH IDX_HIGH survive" },
          /* ── A guard on a SUM bounds an addend by a DIFFERENCE ─────────────
           * The indexOf shape: `toffset + pn <= value.length` with `i < pn`
           * proves `value[toffset + i]` in bounds — toffset ≤ len − pn and
           * i < pn give toffset + i < len. That bound is a DIFFERENCE of two
           * values, past every constraint ABCD generates: its C1–C3 are one
           * variable plus a CONSTANT, and p.3 says a variable defined outside
           * them "is considered unconstrained". ONE Add; multi-Add chains are
           * out of scope.
           *
           * Both addends must be provably non-negative before the difference
           * can be minted: `len − pn` bounds nothing when pn < 0, and the
           * addition must not wrap (§15.18.2). The negatives below are that
           * requirement and the three ways the premise can fail to hold. */
          { "class T { static int f(char[] value, int toffset, int pn){ int s = 0;"
            "  if (toffset >= 0 && pn >= 0 && toffset + pn <= value.length)"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 0,
            "indexOf shape: value[toffset + i] under toffset+pn<=len and i<pn — "
            "IDX_HIGH dies" },
          /* TWO sums, one Add each, each offset carrying its OWN bound. Two
           * bounds on ONE value is a different thing and is not expressible:
           * the representation holds a single symbolic bound and an
           * intersection keeps the incumbent, which S-SUM extends with a
           * subtracted id, not with a second bound. */
          { "class T { static int f(char[] value, int aoff, int boff, int pn){"
            "  int s = 0;"
            "  if (aoff >= 0 && boff >= 0 && pn >= 0"
            "      && aoff + pn <= value.length && boff + pn <= value.length)"
            "    for (int i = 0; i < pn; i++) s += value[aoff + i] + value[boff + i];"
            "  return s; } }", "f", 0,
            "two sums, one Add each, each offset with its own guard: BOTH "
            "IDX_HIGH die" },
          /* `t + p ≤ L` bounds BOTH addends — it says `p ≤ L − t` exactly as
           * much as it says `t ≤ L − p`. Which one a program then indexes by
           * is its own business, so the mirror is a positive too: here the
           * guard is `aoff + boff <= len` and the loop counts to `aoff`, so it
           * is `boff` that needs the difference bound. */
          { "class T { static int f(char[] value, int aoff, int boff){ int s = 0;"
            "  if (aoff >= 0 && boff >= 0 && aoff + boff <= value.length)"
            "    for (int i = 0; i < aoff; i++) s += value[boff + i];"
            "  return s; } }", "f", 0,
            "the SYMMETRIC mint: the guard bounds the OTHER addend, and "
            "value[boff + i] under i < aoff folds — IDX_HIGH dies" },
          /* The FACT is `toffset + pn ≤ len`; which comparison spells it is
           * incidental. A transfer keyed to one syntax passes a corpus and is
           * wrong for the next program, so the other spellings are pinned as
           * positives too. */
          { "class T { static int f(char[] value, int toffset, int pn){"
            "  if (toffset < 0 || pn < 0 || toffset + pn > value.length) return 0;"
            "  int s = 0;"
            "  for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 0,
            "the SAME fact spelled `>` and taken on the FALL-THROUGH: IDX_HIGH dies" },
          { "class T { static int f(char[] value, int toffset, int pn){ int s = 0;"
            "  if (toffset >= 0 && pn >= 0 && value.length >= toffset + pn)"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 0,
            "…and with the sum on the RIGHT of the compare: IDX_HIGH dies" },
          /* NEGATIVES for the sum rule — each kills one leg. */
          { "class T { static int f(char[] value, int toffset, int pn){ int s = 0;"
            "  if (pn >= 0 && toffset + pn <= value.length)"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 1,
            "missing lo-fence: toffset may be negative, so the add may wrap and "
            "len-pn bounds nothing — IDX_HIGH SURVIVES" },
          { "class T { static int f(char[] value, char[] other, int toffset, int pn){"
            "  int s = 0;"
            "  if (toffset >= 0 && pn >= 0 && toffset + pn <= other.length)"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 1,
            "the guard bounds a DIFFERENT length (other.length): IDX_HIGH SURVIVES" },
          { "class T { static int f(char[] value, int toffset, int pn, int m){"
            "  int s = 0;"
            "  if (toffset >= 0 && pn >= 0 && toffset + pn <= value.length) {"
            "    pn = m;"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i]; }"
            "  return s; } }", "f", 1,
            "pn reassigned between guard and loop — the difference names the OLD "
            "pn, the loop the new one (premise id mismatch): IDX_HIGH SURVIVES" },
          { "class T { static int f(char[] value, int toffset){ int s = 0;"
            "  int pn = 2147483647;"
            "  if (toffset >= 0 && toffset + pn <= value.length)"
            "    for (int i = 0; i < pn; i++) s += value[toffset + i];"
            "  return s; } }", "f", 1,
            "the WRAP shape: pn at INT_MAX scale puts toffset+pn outside the "
            "fence — IDX_HIGH SURVIVES" },
        };
        for (int t = 0; t < (int)(sizeof cases / sizeof cases[0]); t++) {
            bbq_arena* arena = sess_arena();
            int nlib = 0;
            ast_program_t* prog = build_program(cases[t].src, arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, arena);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
            compiler_ctx_t cctx; compiler_init(&cctx, arena, &sctx);
            int mc = 0;
            sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
            int surviving = -1;
            for (int i = 0; i < mc; i++) {
                if (methods[i]->class_id < nlib) continue;
                if (!methods[i]->name || strcmp(methods[i]->name, cases[t].m)) continue;
                sir_optimize(&cctx, i);
                int nf = 0;
                const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
                surviving = 0;
                for (int j = 0; j < nf; j++)
                    if (f[j].kind == COMPILER_FACT_GUARD
                            && f[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                            && f[j].key && (int)f[j].key->tag == SIR_BRANCH)
                        surviving++;
                break;
            }
            if (surviving != cases[t].want_surviving)
                printf("        %s: %d IDX_HIGH surviving, want %d\n",
                       cases[t].what, surviving, cases[t].want_surviving);
            CHECK(surviving == cases[t].want_surviving, cases[t].what);
        }
    }

    /* ── V-class: `index < count ⟹ index < data.length` ───────────────────────
     * A user guard refines the index against a COUNT FIELD, while the §15
     * IDX_HIGH tests it against `arraylength(data-field)`. The missing link is
     * the CLASS INVARIANT `count ≤ data.length`, which every writer of either
     * field maintains — so the proof is whole-program, and this block runs the
     * real driver (`compiler_summarize_to_convergence`) rather than optimizing
     * one method in isolation.
     *
     * Each negative kills exactly ONE leg: the AND over writers, the object
     * identity, the memory-version compatibility, and the bound's source.
     * The version negative is the one a graph SCAN would get wrong while an
     * input-read gets right — a store to `data` between the count-read and the
     * access makes the two fields' reaching versions incompatible, which is a
     * property of the load's inputs, not of anything reachable by walking. */
    {
        struct { const char* src; const char* cls; const char* m;
                 int want_surviving; int kind; const char* what; } vcases[] = {
          /* The growth arm carries a defensive re-check. This fixture is the
           * FIRST-LAYER pin: the re-check is the minimal growth whose
           * data-store proves from the method's own facts alone — C4's
           * fall-through at the store's row — with no invariant assumed. The
           * UNCHECKED doubling is the assume-verify layer's positive, pinned
           * separately below; this one stays as written so the first layer
           * keeps its own red. */
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void add(int x){"
            "    if (count == data.length) {"
            "      int[] n = new int[count * 2 + 1];"
            "      if (count > n.length) return;"
            "      int i = 0;"
            "      while (i < count) { n[i] = data[i]; i = i + 1; } data = n; }"
            "    data[count] = x; count = count + 1; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", "V", "get", 0, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "mini-Vector: get's IDX_HIGH dies on the class invariant "
            "count <= data.length" },
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void bad(){ count = data.length + 1; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", "V", "get", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "(a) ONE writer violates the invariant (count = data.length + 1) — "
            "the AND over writers kills the pair: IDX_HIGH SURVIVES" },
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 4; }"
            "  static int g(V v1, V v2, int index){"
            "    if (index < 0 || index >= v1.count) return -1;"
            "    return v2.data[index]; } }", "V", "g", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "(b) two DISTINCT objects (v1.count guarding v2.data): IDX_HIGH "
            "SURVIVES" },
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 4; }"
            "  static int g(V v, int index, int[] other){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    v.data = other;"
            "    return v.data[index]; } }", "V", "g", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "(c) a store to `data` between the count-read and the access — the "
            "two fields' versions are incompatible: IDX_HIGH SURVIVES" },
          { "class V { int count; int[] data; int other;"
            "  V(){ data = new int[4]; count = 4; other = 99; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.other) return -1;"
            "    return v.data[index]; } }", "V", "g", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "(d) the index is bounded by a field that is NOT the pair's count: "
            "IDX_HIGH SURVIVES" },
          /* The NATURAL container, with no defensive re-check anywhere: growth
           * re-establishes the invariant FROM the invariant, which is what every
           * real maintainer does. The data store `data = n` proves because
           * `arraylength(n) = count*2+1 >= count` — the monotone Mul ordering
           * fact under the assumed `count >= 0` — and the count store proves from
           * the §15 check it stands under. Mutual induction: base case §12.5 in
           * the constructor, inductive step here. */
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void add(int x){"
            "    if (count == data.length) {"
            "      int[] n = new int[count * 2 + 1];"
            "      int i = 0;"
            "      while (i < count) { n[i] = data[i]; i = i + 1; } data = n; }"
            "    data[count] = x; count = count + 1; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", "V", "get", 0, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "the NATURAL mini-Vector: unchecked doubling growth, get's IDX_HIGH "
            "dies on the assume-verify invariant" },
          /* The assumption is READOUT-LOCAL. It is made while proving a writer
           * obligation and must not become an engine fact: if `count <= data.length`
           * leaked onto the count READ, the Sub transfer would carry it down to
           * `count - 1 < data.length` and delete a check that must fire. The pair
           * itself holds here (both count stores are KNOWN 0), so the assumption
           * IS being made — this pins that nothing outside the readout can see it. */
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void dec(){ if (count >= 1) { data[count - 1] = 0; } count = 0; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", "V", "dec", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "the assumption never LEAKS: a §15 guard inside the writer's own "
            "body does not fold on the assumed bound — IDX_HIGH SURVIVES" },
          /* SELF-assumption only. Two pairs share one data array, so `a = b` is
           * safe exactly BECAUSE of the other pair's invariant (`b <= p.length`).
           * Mutual induction ACROSS pairs is sound in principle and is not this
           * part: P1's obligation may not spend P2's invariant, so P1 dies. */
          { "class V { int a; int b; int[] p;"
            "  V(){ p = new int[4]; a = 0; b = 0; }"
            "  void w(){ a = b; }"
            "  static int g1(V v, int i){"
            "    if (i < 0 || i >= v.a) return -1; return v.p[i]; }"
            "  static int g2(V v, int i){"
            "    if (i < 0 || i >= v.b) return -1; return v.p[i]; } }", "V", "g1", 1, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "no CROSS-PAIR assumption: `a = b` is provable only from the OTHER "
            "pair's invariant, so this pair dies — IDX_HIGH SURVIVES" },
          { "class V { int a; int b; int[] p;"
            "  V(){ p = new int[4]; a = 0; b = 0; }"
            "  void w(){ a = b; }"
            "  static int g1(V v, int i){"
            "    if (i < 0 || i >= v.a) return -1; return v.p[i]; }"
            "  static int g2(V v, int i){"
            "    if (i < 0 || i >= v.b) return -1; return v.p[i]; } }", "V", "g2", 0, COMPILER_GUARD_ARRAY_INDEX_HIGH,
            "…and the pair whose own writers prove is UNAFFECTED by the other's "
            "death: its IDX_HIGH still falls" },
          /* The SIGN half of the assumption must not leak either. `count >= 0`
           * is assumed while the table is being VERIFIED — it is the induction
           * hypothesis, not a fact about every int field — so once the table is
           * published it must be gone: `data[count]`'s LOWER check is the guard
           * that would fold on it, and it must still be there. Together with the
           * `dec` case above this pins both halves of the seed. */
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void put(int x){ data[count] = x; count = count + 1; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1; "
            "    return data[index]; } }", "V", "put", 1,
            COMPILER_GUARD_ARRAY_INDEX_LOW,
            "the assumed SIGN never leaks either: `count >= 0` is the hypothesis "
            "the table is verified UNDER, so `data[count]`'s IDX_LOW SURVIVES" },
        };
        for (int t = 0; t < (int)(sizeof vcases / sizeof vcases[0]); t++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(vcases[t].src, &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            compiler_summarize_to_convergence(&cctx);   /* the real driver */
            int cid = sema_find_class(&sctx, vcases[t].cls);
            int surviving = -1;
            for (int i = 0; i < mc; i++) {
                if (ms[i]->class_id != cid) continue;
                if (!ms[i]->name || strcmp(ms[i]->name, vcases[t].m)) continue;
                /* LOCALIZATION, positive only, read BEFORE the optimize: the
                 * consumer's first condition is the index carrying the user
                 * guard's count bound. The no-writer class pins this in the
                 * table block; this is the same read on the class where a
                 * WRITER exists, so a red here says the bound is lost upstream
                 * of the verdict and a red on the count alone says it is lost
                 * at the verdict's own conditions. */
                if (t == 0) {
                    int nfe = 0;
                    const compiler_fact_t* fe = compiler_get_facts(&cctx, i, &nfe);
                    cp_engine_t* ge = cp_build_ctx(&cctx, ms[i], fe, nfe);
                    const sir_node_t* gc = NULL;
                    for (int j = 0; j < nfe; j++)
                        if (fe[j].kind == COMPILER_FACT_GUARD
                                && fe[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                                && fe[j].key && (int)fe[j].key->tag == SIR_BRANCH) {
                            gc = fe[j].key->branch.cond; break;
                        }
                    cp_vnode_t* gv = (ge && gc) ? vnode_for(ge, gc) : NULL;
                    cp_vnode_t* gi2 = (gv && gv->input_count > 0 && gv->inputs[0] >= 0)
                                    ? ge->vnodes[gv->inputs[0]] : NULL;
                    if (gi2 && gi2->constant.hi_vn1 == 0)
                        printf("        writer-present class: guard index carries NO "
                               "symbolic bound (state=%d)\n", (int)gi2->constant.state);
                    CHECK(gi2 && gi2->constant.hi_vn1 != 0,
                          "writer-present class: the guard's index still carries the "
                          "user guard's count bound");
                    if (ge) cp_free(ge);
                }
                sir_optimize(&cctx, i);
                int nf = 0;
                const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
                surviving = 0;
                for (int j = 0; j < nf; j++)
                    if (f[j].kind == COMPILER_FACT_GUARD
                            && f[j].a == vcases[t].kind
                            && f[j].key && (int)f[j].key->tag == SIR_BRANCH)
                        surviving++;
                break;
            }
            if (surviving != vcases[t].want_surviving)
                printf("        %s: %d IDX_HIGH surviving, want %d\n",
                       vcases[t].what, surviving, vcases[t].want_surviving);
            CHECK(surviving == vcases[t].want_surviving, vcases[t].what);
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    /* ── The growth data-store's four facts, on the REAL lowered shape ───────
     *
     * `data = new int[count]` is what a growth writer's obligation is read off,
     * and the readout composes exactly four facts. Each is pinned separately
     * HERE, because a red on the table's verdict says only "the pair died" and
     * cannot say WHICH fact is missing — and the shape has to be the one the
     * lowering actually emits (the size is spilled to a temp, and §15.10.1's
     * check tests that temp), so a hand-built method would pin a shape no Java
     * program produces. */
    {
        const char* src =
          "class V { int count; int[] data;"
          "  V(){ data = new int[4]; count = 0; }"
          "  void grow(){ int[] n = new int[count]; data = n; }"
          "  static int g(V v, int index){"
          "    if (index < 0 || index >= v.count) return -1;"
          "    return v.data[index]; } }";
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(src, &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        compiler_summarize_to_convergence(&cctx);
        int cid = sema_find_class(&sctx, "V");
        for (int i = 0; i < mc; i++) {
            if (!ms[i]->name || strcmp(ms[i]->name, "grow")) continue;
            int nf = 0;
            const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i], f, nf);
            if (!e) { CHECK(false, "growth facts: the engine builds"); break; }

            /* The class filter matters: the array OVERLAY lowers `new int[n]`
             * through its own DTREF backing store, whose receiver is the fresh
             * array — the pair's store is the one on class V. */
            int srow = -1;
            for (int s = 0; s < e->spine_count; s++)
                if (e->spine[s]->tag == SIR_PUTFIELD
                        && e->spine[s]->put_field.data_type == SIR_DTREF
                        && e->spine[s]->put_field.class_id == cid) { srow = s; break; }
            CHECK(srow >= 0, "the data store is a spine row");
            cp_vnode_t* alloc = NULL;
            for (int j = 0; j < e->vnode_count && !alloc; j++)
                if (e->vnodes[j]->kind == CP_VN_EXPR && e->vnodes[j]->expr
                        && e->vnodes[j]->expr->tag == SIR_NEWARRAY
                        && e->vnodes[j]->expr != NULL
                        && e->vnodes[j]->expr->new_array.size != NULL)
                    alloc = e->vnodes[j];
            /* FACT 1 — the stored value IS the allocation, one heap hop in:
             * the user-visible array is the OVERLAY struct, and the raw
             * storage whose size is the §10.7 length sits in its IMMUTABLE
             * backing cell — a CP_MEM_STORE row on an immutable cell, receiver
             * congruent with the stored value, whose stored value is the
             * array.new. This is the resolution the obligation makes (pts
             * object → site → backing cell), stated on the memory rows the
             * test can read. */
            cp_vnode_t* stored = (srow >= 0)
                ? vnode_for(e, e->spine[srow]->put_field.value) : NULL;
            bool backing_ok = false;
            for (int r = 0; stored && r < e->mem_rows && !backing_ok; r++) {
                if (e->mem_kind[r] != CP_MEM_STORE) continue;
                int c = e->mem_cell[r];
                if (c < 0 || c >= e->mem_cell_count || !e->cell_immutable[c]) continue;
                if (e->mem_obj[r] < 0 || e->mem_val[r] < 0) continue;
                if (e->vnodes[e->mem_obj[r]]->partition < 0
                        || e->vnodes[e->mem_obj[r]]->partition != stored->partition)
                    continue;
                backing_ok = alloc
                    && e->vnodes[e->mem_val[r]]->partition == alloc->partition;
            }
            if (!backing_ok)
                printf("        FACT 1: no immutable-cell store from the stored "
                       "value's partition %d to the array.new's partition %d\n",
                       stored ? stored->partition : -2,
                       alloc ? alloc->partition : -2);
            CHECK(backing_ok,
                  "FACT 1: the stored value resolves to the ALLOCATION through "
                  "the overlay's immutable backing cell");

            /* FACT 2 — §10.7: the allocation's size is the array's length, and
             * it is the pair's COUNT read. No `n.length` read need exist. */
            cp_vnode_t* szv = (alloc && alloc->input_count >= 1 && alloc->inputs[0] >= 0)
                            ? e->vnodes[alloc->inputs[0]] : NULL;
            cp_vnode_t* cntv = NULL;
            for (int j = 0; j < e->vnode_count && !cntv; j++)
                if (e->vnodes[j]->kind == CP_VN_EXPR && e->vnodes[j]->expr
                        && e->vnodes[j]->expr->tag == SIR_GETFIELD
                        && e->vnodes[j]->expr->get_field.data_type == SIR_DTINT)
                    cntv = e->vnodes[j];
            /* Through the REFINE, deliberately: §15.10.1's check has already
             * refined the size on this arm, and a Refine is a Leader of its own
             * — congruence must not merge a checked value with an unchecked one.
             * So the walk descends leaders AND refine inputs, which is the same
             * hop-walk the crossed-check readout makes for the same reason. */
            int sres = -1;
            for (int hv = (alloc && alloc->input_count >= 1) ? alloc->inputs[0] : -1, hop = 0;
                    hv >= 0 && hv < e->vnode_count && hop < 256; hop++) {
                cp_vnode_t* hn = e->vnodes[hv];
                if (cntv && hn->partition >= 0 && hn->partition == cntv->partition)
                    { sres = hv; break; }
                int next = hv;
                if (hn->leader >= 0) next = hn->leader;
                else if (hn->kind == CP_VN_REFINE && hn->input_count >= 1
                         && hn->inputs[0] >= 0) next = hn->inputs[0];
                if (next == hv) break;
                hv = next;
            }
            if (sres < 0)
                printf("        FACT 2: size part %d, count read part %d, no "
                       "refine/leader hop reaches it\n",
                       szv ? szv->partition : -2, cntv ? cntv->partition : -2);
            CHECK(sres >= 0,
                  "FACT 2: the allocation's SIZE resolves to the count read "
                  "(§10.7), through the check's refine");

            /* FACT 3 — the wrap fence's second form: §15.10.1's negative-size
             * check has fallen through at the allocation, so the size carries a
             * non-negative floor THERE. A wrapped product is negative, so this
             * is what excludes it. */
            if (!szv || szv->constant.state != CP_C_RANGE || szv->constant.lo < 0)
                printf("        FACT 3: size const state=%d lo=%lld\n",
                       szv ? (int)szv->constant.state : -2,
                       szv ? (long long)szv->constant.lo : 0);
            CHECK(szv && szv->constant.state == CP_C_RANGE && szv->constant.lo >= 0,
                  "FACT 3: the fallen-through negative-size check floors the "
                  "allocation's size at 0");

            /* FACT 4 — the PRE-STATE match: the count value the assumption is
             * made about is the version the store's row sees. The read names its
             * own version as its last du input; the row names one through
             * slot_in. A call that cannot reach the cell does not end a version,
             * so both settle through kills before they are compared. */
            int cmem = (cntv && cntv->input_count >= 2)
                     ? cntv->inputs[cntv->input_count - 1] : -1;
            int cell = (cmem >= 0 && cmem < e->mem_rows) ? e->mem_cell[cmem] : -1;
            int rowver = (cell >= 0 && srow >= 0 && srow < e->slot_in_rows)
                       ? e->slot_in[srow][e->slot_count + cell] : -1;
            if (cmem < 0 || cmem != rowver)
                printf("        FACT 4: read ver %d, row ver %d, cell %d "
                       "(kinds %d/%d)\n", cmem, rowver, cell,
                       cmem >= 0 && cmem < e->mem_rows ? (int)e->mem_kind[cmem] : -1,
                       rowver >= 0 && rowver < e->mem_rows ? (int)e->mem_kind[rowver] : -1);
            CHECK(cmem >= 0 && cmem == rowver,
                  "FACT 4: the count read IS the version reaching the store's row "
                  "— the same row, with nothing to cross");

            /* FACT 4b — and it is a read of THE SAME OBJECT the store writes.
             * NOT a congruence or pts claim: each receiver read sits under its
             * own null-check whose Refine is a Leader of its own with a
             * per-site phantom, so those authorities correctly refuse across
             * the excepting allocation. The rule that holds is the receiver's
             * own duality: `this` has two lowered forms (LoadThis, and the
             * synthesized slot-0 LoadLocal), §15.8.3 makes it unassignable,
             * and both reads here ARE this-forms — one object. */
            sir_node_t* sro = (srow >= 0) ? e->spine[srow]->put_field.obj : NULL;
            sir_node_t* cro = cntv ? sir_child((sir_node_t*)cntv->expr, 0) : NULL;
            bool s_this = sro && (sro->tag == SIR_LOADTHIS
                          || (sro->tag == SIR_LOADLOCAL && sro->load_local.slot == 0));
            bool c_this = cro && (cro->tag == SIR_LOADTHIS
                          || (cro->tag == SIR_LOADLOCAL && cro->load_local.slot == 0));
            if (!s_this || !c_this)
                printf("        FACT 4b: store recv tag %d, count-read recv tag "
                       "%d — not both this-forms\n",
                       sro ? (int)sro->tag : -1, cro ? (int)cro->tag : -1);
            CHECK(s_this && c_this,
                  "FACT 4b: the count read and the store name the SAME receiver "
                  "— both are this-forms (§15.8.3: `this` is unassignable)");

            /* FACT 5 — the hypothesis is GONE once the table is published. The
             * assumption is what the obligations are verified UNDER; it is not a
             * fact about the field, and a published `count >= 0` would fold
             * lower-bound checks all over the program. The phase bit is what
             * separates the two, so this reads the count's published constant
             * and requires no assumed floor on it. */
            if (cntv && cntv->constant.state == CP_C_RANGE && cntv->constant.lo >= 0)
                printf("        FACT 5: published count read carries lo=%lld\n",
                       (long long)cntv->constant.lo);
            CHECK(!cntv || cntv->constant.state != CP_C_RANGE
                  || cntv->constant.lo < 0,
                  "FACT 5: the assumed floor does not survive into the PUBLISHED "
                  "facts — it is a hypothesis, not a property of the field");
            cp_free(e);

            /* FACT 6 — and the hypothesis is READOUT-LOCAL: even in the
             * VERIFYING phase, the ENGINE never sees `count >= 0`. The
             * assumption is made inside the writer-obligation readout and
             * nowhere else — a leaked assumption is the unsoundness, because
             * everything in a solve (branch folding, range composition,
             * published facts) would inherit a fact that is only a hypothesis.
             * Reproduced by putting the phase bit where verification has it. */
            cctx.vinv_published = false;
            e = cp_build_ctx(&cctx, ms[i], f, nf);
            cctx.vinv_published = true;
            if (!e) { CHECK(false, "growth facts: the engine rebuilds"); break; }
            cp_vnode_t* alloc2 = NULL;
            for (int j = 0; j < e->vnode_count && !alloc2; j++)
                if (e->vnodes[j]->kind == CP_VN_EXPR && e->vnodes[j]->expr
                        && e->vnodes[j]->expr->tag == SIR_NEWARRAY)
                    alloc2 = e->vnodes[j];
            cp_vnode_t* cnt2 = NULL; int cnt2i = -1;
            for (int j = 0; j < e->vnode_count && !cnt2; j++)
                if (e->vnodes[j]->kind == CP_VN_EXPR && e->vnodes[j]->expr
                        && e->vnodes[j]->expr->tag == SIR_GETFIELD
                        && e->vnodes[j]->expr->get_field.data_type == SIR_DTINT)
                    { cnt2 = e->vnodes[j]; cnt2i = j; }
            if (cnt2 && cnt2->constant.state == CP_C_RANGE && cnt2->constant.lo >= 0)
                printf("        FACT 6a: verifying-phase count read LEAKED the "
                       "hypothesis (state=%d lo=%lld)\n",
                       (int)cnt2->constant.state, (long long)cnt2->constant.lo);
            CHECK(cnt2 && !(cnt2->constant.state == CP_C_RANGE
                            && cnt2->constant.lo >= 0),
                  "FACT 6a: even WHILE VERIFYING, the engine never sees "
                  "`count >= 0` — the hypothesis is readout-local");

            /* FACT 8 — the identity term of the data-store route, on its own,
             * in the SAME verifying engine: the allocation's size and the count
             * read RESOLVE to one value through the leader walk. */
            int szi = alloc2
                ? vnode_idx_for(e, sir_child((sir_node_t*)alloc2->expr, 0)) : -1;
            int t_sz = szi >= 0 ? leader_walk(e, szi) : -2;
            int t_pre = cnt2i >= 0 ? leader_walk(e, cnt2i) : -3;
            if (t_sz < 0 || t_sz != t_pre)
                printf("        FACT 8b: size terminal %d (from vn%d), count "
                       "read terminal %d (from vn%d)\n", t_sz, szi, t_pre, cnt2i);
            CHECK(t_sz >= 0 && t_sz == t_pre,
                  "FACT 8b: the size and the count read resolve to ONE value — "
                  "the route's identity term");
            cp_free(e);

            /* FACT 7 — the verdict, ONE WRITER AT A TIME, through the
             * production path. The table's `holds` is an AND over every store
             * in every method, so its red names the pair and not the writer.
             * `sir_summarize` is the driver's own per-method step: reset the
             * pair, run one method in the verifying phase, read the table.
             * A red here names the METHOD whose obligation fails. */
            if (cctx.vinv_count == 1) {
                cctx.vinv_published = false;
                for (int m = 0; m < mc; m++) {
                    if (!ms[m]->name) continue;
                    bool is_grow = !strcmp(ms[m]->name, "grow");
                    bool is_ctor = !strcmp(ms[m]->name, "<init>")
                                || !strcmp(ms[m]->name, "V");
                    if (!is_grow && !is_ctor) continue;
                    cctx.vinv_holds[0] = true;
                    sir_summarize(&cctx, m);
                    if (!cctx.vinv_holds[0])
                        printf("        FACT 7: writer obligations FAIL in %s "
                               "alone\n", ms[m]->name);
                    CHECK(cctx.vinv_holds[0], is_grow
                          ? "FACT 7: grow's data store proves ON ITS OWN in the "
                            "verifying phase"
                          : "FACT 7: the ctor's stores prove ON THEIR OWN in the "
                            "verifying phase");
                }
                cctx.vinv_published = true;
            } else {
                CHECK(false, "FACT 7: expected exactly one pair in the table");
            }
            break;
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── The invariant TABLE itself, not just what it buys ────────────────────
     * The verdict is a published fact, so it is pinned as one: after the driver
     * converges, the candidate must BE in the table, and its `holds` must say
     * what the writers actually proved. Without this the table's state is only
     * ever visible through a temporary probe, and "why is it empty" becomes a
     * question answered by theorising instead of by the suite.
     *
     * A candidate is entered only when a SURVIVING guard exhibits the shape, so
     * this also pins the ORDERING: discovery happens during a solve, and the
     * convergence loop has to still be running to read the writers of a pair it
     * has just learned about. */
    {
        struct { const char* src; int want_pairs; int want_holds; const char* what; } tcases[] = {
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", 1, 1,
            "the (V, count, data) pair is IN the published table after "
            "convergence, and its writers proved it" },
          { "class V { private int count; private int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void bad(){ count = data.length + 1; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", 1, 0,
            "…and one unprovable writer makes the SAME pair's verdict false — "
            "the AND, published, not merely absent" },
        };
        for (int t = 0; t < (int)(sizeof tcases / sizeof tcases[0]); t++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(tcases[t].src, &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            compiler_summarize_to_convergence(&cctx);
            int after_sum = cctx.vinv_count;
            /* HALF 2, on the REAL lowering: the guard's length operand has to
             * resolve to a read of the DATA FIELD. The hand-built pin in
             * test_click_partition shows half 1 (the index's bound names the
             * count field) already works — and the array overlay's backing
             * accessor is exactly what can sit between `arraylength` and the
             * GetField. Read BEFORE the per-method optimize: consumption
             * retags the eliminated guard, so the shape is only visible on the
             * pre-optimize facts. */
            if (t == 0) {
                int nf2 = 0;
                const compiler_fact_t* f2 = compiler_get_facts(&cctx, 0, &nf2);
                for (int i = 0; i < mc; i++)
                    if (ms[i]->class_id == sema_find_class(&sctx, "V")
                            && ms[i]->name && !strcmp(ms[i]->name, "get"))
                        f2 = compiler_get_facts(&cctx, i, &nf2);
                const sir_node_t* lenop = NULL;
                for (int j = 0; j < nf2; j++)
                    if (f2[j].kind == COMPILER_FACT_GUARD
                            && f2[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                            && f2[j].key && (int)f2[j].key->tag == SIR_BRANCH) {
                        const sir_node_t* c = f2[j].key->branch.cond;
                        if (c) lenop = sir_child((sir_node_t*)c, 1);
                        break;
                    }
                /* HALF 1 on the REAL lowering: rebuild the engine over `get`
                 * and read the guard index's own fact. The hand-built pin shows
                 * the mint works; this says whether it survives to the §15
                 * guard once the whole program is in play. */
                int gidx = -1;
                for (int i = 0; i < mc; i++)
                    if (ms[i]->class_id == sema_find_class(&sctx, "V")
                            && ms[i]->name && !strcmp(ms[i]->name, "get")) gidx = i;
                if (gidx >= 0) {
                    int nfe = 0;
                    const compiler_fact_t* fe = compiler_get_facts(&cctx, gidx, &nfe);
                    cp_engine_t* ge = cp_build_ctx(&cctx, ms[gidx], fe, nfe);
                    const sir_node_t* gc = NULL;
                    for (int j = 0; j < nfe; j++)
                        if (fe[j].kind == COMPILER_FACT_GUARD
                                && fe[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                                && fe[j].key && (int)fe[j].key->tag == SIR_BRANCH) {
                            gc = fe[j].key->branch.cond; break;
                        }
                    cp_vnode_t* gv = (ge && gc) ? vnode_for(ge, gc) : NULL;
                    cp_vnode_t* gi2 = (gv && gv->input_count > 0 && gv->inputs[0] >= 0)
                                    ? ge->vnodes[gv->inputs[0]] : NULL;
                    if (gi2 && gi2->constant.hi_vn1 == 0)
                        printf("        half 1 (real lowering): the guard index carries "
                               "NO symbolic bound (state=%d)\n", (int)gi2->constant.state);
                    CHECK(gi2 && gi2->constant.hi_vn1 != 0,
                          "half 1 (real lowering): the guard's index still carries the "
                          "user guard's count bound");
                    /* …and WHAT it names. The hand-built pin says GetField; the
                     * lowering may spill `count` into a temp first, exactly as
                     * it spills a sum before a compare. */
                    if (ge && gi2 && gi2->constant.hi_vn1) {
                        int bv = gi2->constant.hi_vn1 - 1;
                        const sir_node_t* bex = (bv >= 0 && bv < ge->vnode_count
                                                 && ge->vnodes[bv]->kind == CP_VN_EXPR)
                                              ? ge->vnodes[bv]->expr : NULL;
                        if (!bex || (int)bex->tag != SIR_GETFIELD)
                            printf("        half 1c: the bound names tag %d, not "
                                   "SIR_GETFIELD (%d)\n",
                                   bex ? (int)bex->tag : -1, (int)SIR_GETFIELD);
                        CHECK(bex && (int)bex->tag == SIR_GETFIELD,
                              "half 1c: the index's bound names the COUNT FIELD read");
                    }
                    if (ge) cp_free(ge);
                }
                CHECK(lenop && (int)lenop->tag == SIR_ARRAYLENGTH,
                      "half 2a: the IDX_HIGH guard tests against an ArrayLength");
                if (lenop && (int)lenop->tag == SIR_ARRAYLENGTH) {
                    const sir_node_t* arr = sir_child((sir_node_t*)lenop, 0);
                    if (arr && (int)arr->tag != SIR_GETFIELD)
                        printf("        half 2b: length operand's child tag is %d, "
                               "not SIR_GETFIELD (%d)\n", (int)arr->tag, (int)SIR_GETFIELD);
                    CHECK(arr && (int)arr->tag == SIR_GETFIELD,
                          "half 2b: …and that ArrayLength reads the DATA FIELD directly");
                }
            }
            /* …and again after the per-method optimize. The two counts LOCALISE
             * the entry: both zero means the shape is never recognised in the
             * whole-program setting at all; zero-then-nonzero means it is
             * recognised only in the pass that runs AFTER the driver, which is
             * an ordering fault and not a detection one. Without both numbers
             * the single "table is empty" is a fact with two possible causes. */
            int cid2 = sema_find_class(&sctx, "V");
            for (int i = 0; i < mc; i++) {
                if (ms[i]->class_id != cid2) continue;
                if (!ms[i]->name || strcmp(ms[i]->name, "get")) continue;
                sir_optimize(&cctx, i);
                break;
            }
            int after_opt = cctx.vinv_count;
            int holds = -1;
            for (int p = 0; p < cctx.vinv_count; p++)
                if (cctx.vinv_holds[p]) holds = 1; else if (holds < 0) holds = 0;
            if (after_sum != tcases[t].want_pairs || after_opt != tcases[t].want_pairs)
                printf("        %s: pairs after summarize=%d after optimize=%d, want %d\n",
                       tcases[t].what, after_sum, after_opt, tcases[t].want_pairs);
            CHECK(after_opt == tcases[t].want_pairs,
                  "the guard's shape IS recognised in the whole-program setting");
            CHECK(after_sum == tcases[t].want_pairs, tcases[t].what);
            if (after_sum == tcases[t].want_pairs)
                CHECK(holds == tcases[t].want_holds,
                      tcases[t].want_holds ? "…its verdict HOLDS"
                                           : "…its verdict is FALSE");
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    /* ── The verdict's FOUR conditions, one fixture per condition ─────────────
     * Elimination is conditioned on four things at once: the index carries a
     * bound naming a COUNT field; that read and the length's DATA read name ONE
     * object; the two reads see COMPATIBLE memory versions; and the pair's
     * verdict HOLDS in the published table. A negative is only a pin on its own
     * condition when the other three PASS — otherwise it says "kept", the reader
     * believes the leg is covered, and the leg is not covered at all.
     *
     * That is exactly what the block above cannot say. Its (b)/(c)/(d) classes
     * are constructed with `V(){ … count = 4; }` and `other = 99`, whose count
     * store cannot be proved against the data length from the constructor's own
     * facts — so their verdict is FALSE and every one of them is kept by the AND
     * no matter what the other three conditions do. Same fixtures, rebuilt on
     * the constructor whose writers DO prove (`count = 0`, the form the table
     * pins record holding), so that each keeps for the reason it is named for.
     *
     * The positive is in the same family and is load-bearing: without it, four
     * negatives all passing is equally consistent with a consumer that never
     * fires. */
    {
        /* One class shape, five variations. The constructor is the SAME in all
         * of them and its two stores are provable, so the pair's verdict holds
         * for every case here and only the named condition can decide. */
        struct { const char* src; const char* m; int want_surviving;
                 const char* what; } pcases[] = {
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", "g", 0,
            "POSITIVE: all four conditions hold — IDX_HIGH is ELIMINATED (without "
            "this the four negatives below are also consistent with a consumer "
            "that never fires)" },
          /* The cross-object guard is not the shape piece 1 collects — that shape
           * names ONE receiver — so this class would have no candidate at all if
           * `g` were its only guard, and the case would prove nothing. `one`
           * supplies the same-object guard that makes the pair, and `g` is the
           * case: a holding pair that must still not reach a guard on a
           * DIFFERENT object. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int one(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; }"
            "  static int g(V v1, V v2, int index){"
            "    if (index < 0 || index >= v1.count) return -1;"
            "    return v2.data[index]; } }", "g", 1,
            "condition 2 ALONE: two DISTINCT objects (v1.count guarding "
            "v2.data) — the pair's verdict HOLDS, so only obj != obj' can keep "
            "this guard" },
          /* The intervening store has to be one the writers' AND ACCEPTS, or the
           * fixture tests the AND instead of the versions. `count = 0` is such a
           * store — zero is below every length, so it proves — while a store to
           * `data` is not: proving one needs the stored array's length against
           * the count value, which no method but a constructor currently has.
           * So the version leg is pinned on the count cell; V1's (c) keeps its
           * data-store shape and is kept by the AND, which is what it can say. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    v.count = 0;"
            "    return v.data[index]; } }", "g", 1,
            "condition 3 ALONE: a PROVABLE store to `count` between the count-read "
            "and the access — the pair still holds, so only the version leg can "
            "keep this guard" },
          /* Condition 1 is about the BOUND's source, so the guard under test must
           * carry a bound naming no field at all — a constant. The class still
           * needs a holding pair for the other three conditions to be satisfied,
           * so `g` supplies it and `h` is the case. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; }"
            "  static int h(V v, int index){"
            "    if (index < 0 || index >= 4) return -1;"
            "    return v.data[index]; } }", "h", 1,
            "condition 1 ALONE: the index's bound is a CONSTANT, naming no count "
            "field — the pair holds and is irrelevant to this guard" },
        };
        for (int t = 0; t < (int)(sizeof pcases / sizeof pcases[0]); t++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(pcases[t].src, &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
            compiler_summarize_to_convergence(&cctx);
            /* The other three conditions are only "satisfied" if the verdict is
             * one the consumer can act on, so the fixture's own premise is
             * asserted before its outcome: a case that keeps because its pair
             * died is not testing the leg its name claims. */
            int holds = 0;
            for (int p = 0; p < cctx.vinv_count; p++) if (cctx.vinv_holds[p]) holds = 1;
            if (!holds)
                printf("        %s: PREMISE BROKEN — no pair in the table holds, "
                       "so this fixture cannot isolate its condition\n", pcases[t].what);
            CHECK(holds == 1,
                  "the fixture's own premise: its (count, data) pair's verdict HOLDS");
            int cid = sema_find_class(&sctx, "V");
            int surviving = -1;
            for (int i = 0; i < mc; i++) {
                if (ms[i]->class_id != cid) continue;
                if (!ms[i]->name || strcmp(ms[i]->name, pcases[t].m)) continue;
                sir_optimize(&cctx, i);
                int nf = 0;
                const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
                surviving = 0;
                for (int j = 0; j < nf; j++)
                    if (f[j].kind == COMPILER_FACT_GUARD
                            && f[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                            && f[j].key && (int)f[j].key->tag == SIR_BRANCH)
                        surviving++;
                break;
            }
            if (surviving != pcases[t].want_surviving)
                printf("        %s: %d IDX_HIGH surviving, want %d\n",
                       pcases[t].what, surviving, pcases[t].want_surviving);
            CHECK(surviving == pcases[t].want_surviving, pcases[t].what);
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    /* ── The table's OWN three clauses: demand, the v1 restriction, the AND ────
     * These are properties of the table rather than of any one guard, so each is
     * read off the published table directly.
     *
     * DEMAND is the clause that keeps the table the size of the shapes a program
     * USES: a pair is entered only when some surviving guard exhibits the shape,
     * never by scanning the program's field pairs. A class carrying both fields
     * and no such guard is the negative that says so — and it is the one a
     * "collect every (int, int[]) pair in the class" implementation passes every
     * other test without failing.
     *
     * The v1 RESTRICTION is that a writer proof may not assume the invariant. A
     * store that is safe ONLY because `count <= data.length` already held is the
     * cyclic case the restriction excludes: its pair dies, and its guards stay.
     *
     * The AND is over every writer in EVERY method, so it cannot depend on where
     * the violating writer falls in the analysis order — the same violation is
     * placed before and after the guard's method. */
    {
        struct { const char* src; int want_pairs; int want_holds;
                 const char* what; } gcases[] = {
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.data.length) return -1;"
            "    return v.data[index]; } }", 0, -1,
            "DEMAND: a class with both fields and NO guard of the cross-field "
            "shape enters NO candidate — the table is not a scan of field pairs" },
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void shrink(int k){ if (k <= count) count = k; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 0,
            "the COMPANION clause: `k <= count` proves the upper half from the "
            "assumption, but k may be NEGATIVE, so `count >= 0` fails — DIES" },
          /* The flip of the case above, and the reason the companion clause is
           * part of the invariant rather than an afterthought: guard BOTH sides
           * and the same writer proves. Upper half from the assumed
           * `count <= data.length`, lower half from the guard's own `k >= 0`. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void shrink(int k){ if (k >= 0 && k <= count) count = k; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "…and the SIGN-GUARDED shrink proves both clauses: the pair HOLDS" },
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void bad(){ count = data.length + 1; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 0,
            "the AND, violator declared BEFORE the guard's method: verdict FALSE" },
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; }"
            "  void bad(){ count = data.length + 1; } }", 1, 0,
            "…and declared AFTER it: the AND is over every method, so the "
            "verdict is FALSE either way (order-independence)" },
          /* The writer that stands under a CHECK. `count = count + 1` after
           * `data[count] = x` stores one more than a value the §15 check just
           * proved below the length, so `count + 1 <= data.length` holds — from
           * this method's own facts, with no invariant assumed. The growth path
           * is left out on purpose: it makes the OTHER store (`data = n`) the
           * subject, and this fixture is about the count store alone. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void add(int x){ data[count] = x; count = count + 1; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "a count store under the array check proves from the check itself: "
            "`count < len` gives `count + 1 <= len`" },
          /* The base case the whole induction stands on is §12.5, and §12.5 is
           * about a CONSTRUCTOR's freshly created object. The identical store in
           * an ordinary method has no default to lean on — the object it writes
           * has been live and its count may hold anything — so the discharge must
           * not apply there, and the pair dies fail-closed. */
          { "class V { int count; int[] data;"
            "  V(){ count = 0; }"
            "  void init(){ data = new int[4]; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 0,
            "§12.5 is a CONSTRUCTOR's fresh object: the same data-store in an "
            "ordinary method proves nothing, so the pair DIES" },
          /* The DATA-store mirror of the check-readout: a growth arm that
           * re-checks its own postcondition. The crossed `count > n.length`
           * fall-through is the obligation verbatim — `count <=
           * arraylength(stored)`, at the store's row — proved from the
           * method's own facts with no invariant assumed, which is what keeps
           * this route independent of the assume-verify layer's. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){"
            "    int[] n = new int[count * 2 + 1];"
            "    if (count > n.length) return;"
            "    data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "a data store under its own re-check proves from the check itself: "
            "the crossed `count > n.length` fall-through is the obligation" },
          /* The assume-verify ladder, one obligation per rung, so a red names
           * WHICH half of the induction is missing instead of "the mini-Vector
           * does not fold".
           *
           * Rung 0: the allocation's size IS the count. No arithmetic at all —
           * `count <= arraylength(new int[count])` is the §10.7 identity and
           * nothing else, so a red here is the pre-state count read or the
           * stored value's identity, never the ordering rows. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count]; data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "rung 0: a data store of an array allocated AT the count proves by "
            "§10.7 alone — the size is the length" },
          /* Rung 0b: one constant added. C3's row read downward — adding a
           * non-negative constant cannot decrease — with no multiplication. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count + 1]; data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "rung 0b: …and with a constant added to the size, which cannot "
            "decrease it" },
          /* Rung 1: the doubling data store ALONE, with no
           * re-check and no other writer — `arraylength(new int[count*2+1])`
           * reaches count because the allocation's size is its length (§10.7),
           * `x * c >= x` for c >= 1, and the assumed `count >= 0`. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count * 2 + 1]; data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "rung 1: the UNCHECKED doubling data store proves on its own — the "
            "allocation's size is the length it will carry" },
          /* Rung 2: the same store with the element COPY LOOP between the
           * allocation and it. The loop reads both fields and defines a φ for
           * every slot it carries, so this is where a proof that depends on the
           * stored value's identity would lose it. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count * 2 + 1];"
            "    int i = 0;"
            "    while (i < count) { n[i] = data[i]; i = i + 1; }"
            "    data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 1,
            "rung 2: …and still proves with the element copy loop between the "
            "allocation and the store" },
          /* The wrap fence's OWN negatives. `count * 3` can wrap back into the
           * non-negative range (3·(2^30+1) mod 2^32 is positive and SMALLER
           * than count), so the crossed NegativeArraySize check no longer
           * excludes the wrap and `size >= count` is simply false on real
           * inputs — the shape must be refused, not proved. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count * 3]; data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 0,
            "wrap fence: `count * 3` can wrap back POSITIVE, so the negative-"
            "size check excludes nothing and the pair DIES" },
          /* Same boundary from the Add side: `count * 2` is even, so a wrapped
           * product is negative — but `+ 2` can carry it past 2^32 back to
           * non-negative (count = 2^31 - 1 gives 0), where `size >= count` is
           * false. k <= 1 cannot cross; k = 2 must be refused. */
          { "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void grow(){ int[] n = new int[count * 2 + 2]; data = n; }"
            "  static int g(V v, int index){"
            "    if (index < 0 || index >= v.count) return -1;"
            "    return v.data[index]; } }", 1, 0,
            "wrap fence: `count * 2 + 2` can wrap back to ZERO at the boundary, "
            "so the pair DIES" },
        };
        for (int t = 0; t < (int)(sizeof gcases / sizeof gcases[0]); t++) {
            bbq_arena a; bbq_arena_init(&a, 1 << 16);
            int nlib = 0;
            ast_program_t* prog = build_program(gcases[t].src, &a, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, &a);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
            compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
            int mc = 0;
            compiler_compile(&cctx, prog, &mc);
            compiler_summarize_to_convergence(&cctx);
            if (cctx.vinv_count != gcases[t].want_pairs)
                printf("        %s: %d pairs, want %d\n",
                       gcases[t].what, cctx.vinv_count, gcases[t].want_pairs);
            CHECK(cctx.vinv_count == gcases[t].want_pairs, gcases[t].what);
            if (gcases[t].want_holds >= 0 && cctx.vinv_count == gcases[t].want_pairs) {
                int holds = 0;
                for (int p = 0; p < cctx.vinv_count; p++) if (cctx.vinv_holds[p]) holds = 1;
                CHECK(holds == gcases[t].want_holds,
                      gcases[t].want_holds ? "…and its verdict HOLDS"
                                           : "…and its verdict is FALSE");
            }
            /* IMMUTABLE during any later per-method solve — the property that
             * lets a consumer read a verdict with no re-arm machinery. Snapshot
             * the whole table, run the per-method pass over EVERY method, and
             * compare: a table that grows or whose verdicts move during that
             * pass has already been read by a guard that folded on the old one. */
            int pre_count = cctx.vinv_count;
            bool pre[8];
            for (int p = 0; p < pre_count && p < 8; p++) pre[p] = cctx.vinv_holds[p];
            for (int i = 0; i < mc; i++) sir_optimize(&cctx, i);
            bool same = (cctx.vinv_count == pre_count);
            for (int p = 0; p < pre_count && p < 8 && same; p++)
                same = (cctx.vinv_holds[p] == pre[p]);
            if (!same)
                printf("        table MOVED during the per-method pass: %d pairs "
                       "before, %d after\n", pre_count, cctx.vinv_count);
            CHECK(same, "the published table is IMMUTABLE across the later "
                        "per-method solve — discovery and verification are closed");
            sema_destroy(&sctx); bbq_arena_free(&a);
        }
    }

    /* ── Two reads of ONE field are one value, in the LOWERED shape ───────────
     * The sibling of §7.3: an element write cannot change what a FIELD read
     * sees, so `count` before `data[count] = x` and `count` after it are the
     * same value — one cell, one receiver, one reaching version. Every proof
     * that spans a statement rests on this, and it was pinned only on
     * hand-built SIR, where the lowering's spills and overlay accessors are
     * absent. Asserted here on the real thing, with the version question split
     * out: if the reads carry DIFFERENT versions, the element store killed a
     * cell it cannot have touched, and that is a different defect from two
     * congruent reads failing to land in one partition. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void add(int x){ data[count] = x; count = count + 1; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int reads = 0, same_ver = -1, same_part = -1;
        for (int i = 0; i < mc; i++) {
            if (ms[i]->class_id < nlib) continue;
            if (!ms[i]->name || strcmp(ms[i]->name, "add")) continue;
            int nf = 0;
            const compiler_fact_t* fs = compiler_get_facts(&cctx, i, &nf);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i], fs, nf);
            if (!e) break;
            int first = -1;
            for (int v = 0; v < e->vnode_count; v++) {
                const cp_vnode_t* vn = e->vnodes[v];
                if (vn->kind != CP_VN_EXPR || !vn->expr) continue;
                if (vn->expr->tag != SIR_GETFIELD) continue;
                if (vn->expr->get_field.field_idx != 0) continue;   /* count */
                if (vn->expr->get_field.class_id < nlib) continue;
                reads++;
                if (first < 0) { first = v; continue; }
                const cp_vnode_t* f0 = e->vnodes[first];
                same_part = (f0->partition >= 0 && f0->partition == vn->partition);
                same_ver = (f0->input_count >= 2 && vn->input_count >= 2
                            && f0->inputs[f0->input_count - 1]
                               == vn->inputs[vn->input_count - 1]);
            }
            cp_free(e);
            break;
        }
        if (reads < 2 || same_ver != 1 || same_part != 1)
            printf("        lowered field reads: count=%d same_version=%d "
                   "same_partition=%d\n", reads, same_ver, same_part);
        CHECK(reads >= 2, "the fixture really does read `count` twice");
        CHECK(same_ver == 1,
              "an element write cannot change a FIELD's contents, so both reads "
              "carry the same reaching version");
        CHECK(same_part == 1,
              "…and two reads of one cell, one receiver, one version are ONE "
              "value: same partition");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── The readout's four links, on the REAL lowering ───────────────────────
     * The hand-built pins show the channel works: the check's fall-through is
     * recorded at the store's row and the checked value is the stored value's
     * operand. The whole-program obligation still fails, so one of those links
     * does not survive the lowering — the spilled index, the overlay's backing
     * accessor, the extra guards. Each link is asserted separately here so the
     * failure names which one, instead of "the verdict is false". */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class V { int count; int[] data;"
            "  V(){ data = new int[4]; count = 0; }"
            "  void add(int x){ data[count] = x; count = count + 1; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int have_row = 0, have_fact = 0, have_ge = 0, same_val = 0;
        for (int i = 0; i < mc; i++) {
            if (ms[i]->class_id < nlib) continue;
            if (!ms[i]->name || strcmp(ms[i]->name, "add")) continue;
            int nf = 0;
            const compiler_fact_t* fs = compiler_get_facts(&cctx, i, &nf);
            cp_engine_t* e = cp_build_ctx(&cctx, ms[i], fs, nf);
            if (!e) break;
            for (int r = 0; r < e->spine_count; r++) {
                sir_node_t* n = e->spine[r];
                if (n->tag != SIR_PUTFIELD) continue;
                if (!e->verdict_words || r >= e->verdict_rows) continue;
                have_row = 1;
                /* Resolve to the value it ultimately IS BEFORE matching its
                 * shape: the lowering spills both the sum and the compared
                 * value into temps, so the store names a slot read and the Add
                 * is a hop further in. Written out rather than calling the
                 * engine's own walk — an oracle that calls the routine under
                 * test proves nothing. */
                cp_vnode_t* xv = vnode_for(e, n->put_field.value);
                for (int pass = 0; pass < 2; pass++) {
                    for (int h = 0; h < 64 && xv; h++) {
                        if (xv->leader >= 0) { xv = e->vnodes[xv->leader]; continue; }
                        if (xv->kind == CP_VN_REFINE && xv->input_count >= 1
                                && xv->inputs[0] >= 0)
                            { xv = e->vnodes[xv->inputs[0]]; continue; }
                        if (xv->kind == CP_VN_EXPR && xv->expr
                                && xv->expr->tag == SIR_LOADLOCAL
                                && xv->input_count == 1 && xv->inputs[0] >= 0)
                            { xv = e->vnodes[xv->inputs[0]]; continue; }
                        break;
                    }
                    if (pass == 0 && xv && xv->kind == CP_VN_EXPR && xv->expr
                            && xv->expr->tag == SIR_ADD && xv->input_count == 2
                            && xv->inputs[0] >= 0)
                        xv = e->vnodes[xv->inputs[0]];
                }
                int xvn = (xv && xv->partition >= 0) ? xv->partition : -1;
                const uint64_t* w = e->verdict_words + (size_t)r * e->verdict_stride;
                for (int q = 0; q < e->verdict_stride; q++) {
                    uint64_t bits = w[q];
                    while (bits) {
                        int fid = q * 64 + __builtin_ctzll(bits);
                        bits &= bits - 1;
                        if (fid & 1) continue;
                        have_fact = 1;
                        int fb = e->fact_branch[fid >> 1];
                        int cvn = fb >= 0 ? e->branch_cond_vn[fb] : -1;
                        if (cvn < 0 || cvn >= e->vnode_count) continue;
                        const cp_vnode_t* cv = e->vnodes[cvn];
                        if (cv->kind != CP_VN_EXPR || !cv->expr
                                || cv->expr->tag != SIR_GE || cv->input_count < 2) continue;
                        have_ge = 1;
                        cp_vnode_t* tv = cv->inputs[0] >= 0
                                       ? e->vnodes[cv->inputs[0]] : NULL;
                        for (int h = 0; h < 64 && tv; h++) {
                            if (tv->leader >= 0) { tv = e->vnodes[tv->leader]; continue; }
                            if (tv->kind == CP_VN_REFINE && tv->input_count >= 1
                                    && tv->inputs[0] >= 0)
                                { tv = e->vnodes[tv->inputs[0]]; continue; }
                            if (tv->kind == CP_VN_EXPR && tv->expr
                                    && tv->expr->tag == SIR_LOADLOCAL
                                    && tv->input_count == 1 && tv->inputs[0] >= 0)
                                { tv = e->vnodes[tv->inputs[0]]; continue; }
                            break;
                        }
                        if (tv && xvn >= 0 && tv->partition == xvn) same_val = 1;
                    }
                }
            }
            cp_free(e);
            break;
        }
        if (!(have_row && have_fact && have_ge && same_val))
            printf("        readout links on the real lowering: row=%d fact=%d "
                   "ge=%d same_value=%d\n", have_row, have_fact, have_ge, same_val);
        CHECK(have_row, "LINK 1: the count store's row has a published verdict row");
        CHECK(have_fact, "LINK 2: …carrying a FALL-THROUGH fact of some check");
        CHECK(have_ge, "LINK 3: …one of which is a GE — the §15 upper-bound shape");
        CHECK(same_val, "LINK 4: …whose tested value IS the value the store "
                        "increments (one partition)");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── The blessed no-op ctor is blessed from DECLARATIONS, and fails closed ─
     * A §8.8.9 synthesized default constructor runs `super()` and this class's
     * instance-variable initializers and nothing else, so an imported one with
     * neither is a call the analysis knows is empty — that is what lets a user
     * ctor's `this` survive its own `super()` when the library is
     * declaration-only, and it is the base case §12.5's discharge stands on.
     * The blessing is only sound while every link of the chain has that shape.
     * Here the superclass DECLARES its constructors, so the chain breaks, the
     * call stays a bottom method, `this` is poisoned, and the discharge that
     * needs it fails — the pair's verdict goes false rather than being taken on
     * trust. The same fixture with the default chain holds it (the table pins
     * above), so this is the fail-closed half of that pair. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class W extends java.util.Vector {"
            "  int count; int[] data;"
            "  W(){ data = new int[4]; count = 0; }"
            "  int get(int index){"
            "    if (index < 0 || index >= count) return -1;"
            "    return data[index]; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        compiler_compile(&cctx, prog, &mc);
        compiler_summarize_to_convergence(&cctx);
        int holds = -1;
        for (int p = 0; p < cctx.vinv_count; p++)
            if (cctx.vinv_holds[p]) holds = 1; else if (holds < 0) holds = 0;
        if (holds == 1)
            printf("        import-ctor: a superclass that DECLARES its ctors was "
                   "still treated as an empty call\n");
        CHECK(holds != 1,
              "a super chain that does not consist of synthesized default ctors "
              "is NOT blessed: the call stays bottom and the §12.5 discharge "
              "fails closed");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── The published-fact strip, at the boundary that can actually leak ─────
     * A symbolic bound is a per-method VNODE ID and means nothing in another
     * method. The summary struct cannot carry one — its range is two scalars —
     * so the leak cannot happen there. It can happen at the CONSUMER: whatever a
     * caller builds from a callee's summary is a fresh fact, and if it were
     * given a symbolic half, the id would name some unrelated node of the
     * CALLER, and a guard could fold against a bound that was never about it.
     *
     * The callee returns a value carrying exactly such a bound (`i` under
     * `i < a.length`), and the caller does nothing but call and index, so any
     * upper-bound id on the index is one that crossed the boundary. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T {"
            "  static int idx(int[] a, int i){"
            "    if (i < 0 || i >= a.length) return 0;"
            "    return i; }"
            "  static int g(int[] b, int[] a, int i){"
            "    return b[idx(a, i)]; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        compiler_summarize_to_convergence(&cctx);
        int leaked = -1, surviving = -1;
        for (int i = 0; i < mc; i++) {
            if (ms[i]->class_id < nlib) continue;
            if (!ms[i]->name || strcmp(ms[i]->name, "g")) continue;
            int nf = 0;
            const compiler_fact_t* fe = compiler_get_facts(&cctx, i, &nf);
            cp_engine_t* ge = cp_build_ctx(&cctx, ms[i], fe, nf);
            if (ge) {
                leaked = 0; surviving = 0;
                for (int j = 0; j < nf; j++) {
                    if (fe[j].kind != COMPILER_FACT_GUARD) continue;
                    if (fe[j].a != COMPILER_GUARD_ARRAY_INDEX_HIGH) continue;
                    if (!fe[j].key || (int)fe[j].key->tag != SIR_BRANCH) continue;
                    surviving++;
                    cp_vnode_t* gv = vnode_for(ge, fe[j].key->branch.cond);
                    if (gv && gv->input_count > 0 && gv->inputs[0] >= 0
                            && ge->vnodes[gv->inputs[0]]->constant.hi_vn1 != 0)
                        leaked = 1;
                }
                cp_free(ge);
            }
            break;
        }
        if (leaked != 0)
            printf("        strip: the caller's index carries a symbolic upper "
                   "bound that can only have come from the callee\n");
        CHECK(leaked == 0,
              "the published strip holds at the CONSUMER: a callee's per-method "
              "vnode id never becomes a bound in its caller");
        CHECK(surviving == 1,
              "…and the pin is not vacuous: the caller's IDX_HIGH is there to "
              "have been folded by such a bound");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── §7.3: "no statement can change the size of an array" ─────────────────
     * The paper's aliasing argument, and the engine leans on it every time two
     * reads of one array's length are counted as one value. A store to an
     * unrelated field sits between the guard and the access here: if the length
     * reads either side of it are two values, the bound the guard established
     * cannot reach the access and its check survives. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { int f;"
            "  static int g(T o, int[] a, int i){"
            "    if (i < 0 || i >= a.length) return -1;"
            "    o.f = i;"
            "    return a[i]; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int surviving = -1;
        for (int i = 0; i < mc; i++) {
            if (ms[i]->class_id < nlib) continue;
            if (!ms[i]->name || strcmp(ms[i]->name, "g")) continue;
            sir_optimize(&cctx, i);
            int nf = 0;
            const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
            surviving = 0;
            for (int j = 0; j < nf; j++)
                if (f[j].kind == COMPILER_FACT_GUARD
                        && f[j].a == COMPILER_GUARD_ARRAY_INDEX_HIGH
                        && f[j].key && (int)f[j].key->tag == SIR_BRANCH)
                    surviving++;
            break;
        }
        if (surviving != 0)
            printf("        §7.3: %d IDX_HIGH surviving, want 0\n", surviving);
        CHECK(surviving == 0,
              "§7.3: a store to an unrelated field cannot change an array's "
              "size, so the guard's bound still reaches the access");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── §6/§6.2: a check does not MOVE ───────────────────────────────────────
     * ABCD's PRE inserts compensation checks to make a partially redundant check
     * fully redundant; its own §6.2 concedes traps cannot move, and here the
     * guard IS the trap with its location pinned by precise exceptions. So a
     * loop-INVARIANT bounds check — the paper's own motivating example for PRE —
     * must stay inside the loop, however invariant it is. The oracle is the
     * recorded LOOP row, the same sidecar the hoisting pins below consult. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class T { static int f(int[] a, int n){ int h = 0;"
            "  for (int i = 0; i < n; i++) h += a[3];"
            "  return h; } }", &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a);
        sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
        if (!sir_analyze(&sctx)) printf("  (note: sema reported errors)\n");
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        int in_prefix = -1;
        for (int i = 0; i < mc; i++) {
            if (ms[i]->class_id < nlib) continue;
            if (!ms[i]->name || strcmp(ms[i]->name, "f")) continue;
            sir_optimize(&cctx, i);
            int nf = 0;
            const compiler_fact_t* fs = compiler_get_facts(&cctx, i, &nf);
            const sir_node_t* ltop = NULL;
            for (int j = 0; j < nf; j++)
                if (fs[j].kind == COMPILER_FACT_SCOPE && fs[j].a == COMPILER_SCOPE_LOOP)
                    ltop = fs[j].key;
            if (!ltop) break;
            /* Is any surviving IDX guard's Branch in the ENTRY PREFIX — i.e. did
             * a check leave the loop? */
            in_prefix = 0;
            for (int j = 0; j < nf; j++) {
                if (fs[j].kind != COMPILER_FACT_GUARD) continue;
                if (fs[j].a != COMPILER_GUARD_ARRAY_INDEX_HIGH
                        && fs[j].a != COMPILER_GUARD_ARRAY_INDEX_LOW) continue;
                if (!fs[j].key || (int)fs[j].key->tag != SIR_BRANCH) continue;
                for (sir_node_t* n = ms[i]->entry; n && n != ltop; n = sir_get_next(n))
                    if (n == fs[j].key) { in_prefix = 1; break; }
                if (in_prefix) break;
            }
            break;
        }
        if (in_prefix != 0)
            printf("        §6.2: a bounds check moved into the entry prefix "
                   "(in_prefix=%d)\n", in_prefix);
        CHECK(in_prefix == 0,
              "§6.2: the guard IS the trap and precise exceptions pin its "
              "location — a loop-invariant check is never hoisted out");
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    /* ── LICM-lite: a pure loop-invariant tree computes ONCE, before the header ──
     * Oracle: after sir_optimize, the SIR_MUL of `x * y` (both operands defined
     * before the loop) lives in the ENTRY PREFIX — the next-chain from
     * method->entry BEFORE the first recorded loop header — not in the body.
     * The loop header comes from the SIDECAR (the recorded LOOP scope row), so
     * the test consults the record, not a rediscovered CFG. */
    {
        struct { const char* src; bool want_hoisted; const char* what; } lcases[] = {
          { "class T { static int f(int x, int y, int n){ int h = 0;"
            "  for (int i = 0; i < n; i++) h += x * y;"
            "  return h; } }", true,
            "x * y (both pre-loop) is hoisted before the header" },
          { "class T { static int f(int x, int n){ int h = 0;"
            "  for (int i = 0; i < n; i++) { int t = x * h; h += t; }"
            "  return h; } }", false,
            "x * h (h defined in the body) stays in the loop" },
          { "class T { static int g(int v){ return v; }"
            "  static int f(int x, int n){ int h = 0;"
            "  for (int i = 0; i < n; i++) h += g(x * h);"
            "  return h; } }", false,
            "a body-tainted mul under a call argument stays in the loop" },
          /* The CASCADE: rows are recorded inner-first and cp_licm processes
           * them in table order, so the inner splice is IN the outer body when
           * the outer row runs — the tree re-qualifies and hoists again. The
           * boundary below is the LAST loop row (the OUTER header), so this
           * pins "outside BOTH loops", which the count alone cannot. */
          { "class T { static int f(int x, int y, int n){ int h = 0;"
            "  for (int i = 0; i < n; i++)"
            "    for (int j = 0; j < n; j++) h += x * y;"
            "  return h; } }", true,
            "a doubly-invariant mul cascades out of BOTH loops" },
        };
        for (int t = 0; t < (int)(sizeof lcases / sizeof lcases[0]); t++) {
            bbq_arena* arena = sess_arena();
            int nlib = 0;
            ast_program_t* prog = build_program(lcases[t].src, arena, &nlib);
            sema_ctx_t sctx; sema_init(&sctx, arena);
            sctx.num_library_classes = nlib; sctx.analyze_from = nlib;
            if (!sir_analyze(&sctx)) { printf("  (note: sema reported errors)\n"); }
            compiler_ctx_t cctx; compiler_init(&cctx, arena, &sctx);
            int mc = 0;
            sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
            int hoisted = -1;
            for (int i = 0; i < mc; i++) {
                if (methods[i]->class_id < nlib) continue;
                if (!methods[i]->name || strcmp(methods[i]->name, "f")) continue;
                sir_optimize(&cctx, i);
                /* The OUTERMOST recorded loop header: rows are inner-first, so
                 * take the LAST loop row — the prefix then excludes every loop,
                 * and "hoisted" means hoisted out of ALL of them. */
                int nf = 0;
                const compiler_fact_t* fs = compiler_get_facts(&cctx, i, &nf);
                const sir_node_t* ltop = NULL;
                for (int j = 0; j < nf; j++)
                    if (fs[j].kind == COMPILER_FACT_SCOPE && fs[j].a == COMPILER_SCOPE_LOOP)
                        ltop = fs[j].key;
                if (!ltop) break;
                /* Walk the ENTRY PREFIX (next-chain until the header); is a MUL
                 * in any node's expression trees there? */
                hoisted = 0;
                for (sir_node_t* n = methods[i]->entry;
                     n && n != ltop; n = sir_get_next(n)) {
                    for (int c = 0; c < sir_arity(n); c++)
                        if (expr_has_tag(sir_child(n, c), SIR_MUL, 64))
                            { hoisted = 1; break; }
                    if (hoisted) break;
                }
                break;
            }
            if (hoisted != (lcases[t].want_hoisted ? 1 : 0))
                printf("        %s: hoisted=%d want=%d\n",
                       lcases[t].what, hoisted, lcases[t].want_hoisted ? 1 : 0);
            CHECK(hoisted == (lcases[t].want_hoisted ? 1 : 0), lcases[t].what);
        }
    }

    {
        /* ── PEA ctor replay must not lose a NON-CONSTANT argument ─────────────
         * `new P(q + 1)` with P's ctor `x = a`: the replay binds the formal to
         * the call's arg, which the frontend spilled to a temp. The temp's only
         * ORIGINAL reader is the invoke PEA deletes; the replay's bind is a NEW
         * reader, and every consumer of the row set (reachability, liveness,
         * DSE) must see it — or the temp's store looks dead, the add is deleted,
         * and the field reads its §4.12.5 default. A constant arg cannot catch
         * this: the solve folds the bind to the constant and nothing reads the
         * temp. `q = 9` as the second arg FORCES the spill (§15.12.4.2
         * left-to-right evaluation: q + 1 must be captured before q is
         * clobbered) — an inline-arg shape can compile correctly by accident.
         * Same harness as §44 — the replay only fires under converged ctor
         * summaries, and the scalar_total premise keeps the pin from passing
         * vacuously when it doesn't. */
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int nlib = 0;
        ast_program_t* prog = build_program(
            "class P { int x; int y; P(int a, int b){ x = a; y = b; } }"
            " class T { static int g(int q){ P p = new P(q + 1, q = 9); return p.x; } }",
            &a, &nlib);
        sema_ctx_t sctx; sema_init(&sctx, &a); sctx.num_library_classes = nlib;
        sir_analyze(&sctx);
        compiler_ctx_t cctx; compiler_init(&cctx, &a, &sctx);
        int mc = 0;
        sir_method_t** ms = compiler_compile(&cctx, prog, &mc);
        compiler_summarize_to_convergence(&cctx);
        int p_id = sema_find_class(&sctx, "P");
        int t_id = sema_find_class(&sctx, "T"), mi = -1;
        for (int k = 0; k < mc; k++)
            if (ms[k]->class_id == t_id && ms[k]->name && !strcmp(ms[k]->name, "g")) mi = k;
        CHECK(mi >= 0, "PEA arg pin: g resolves");
        if (mi >= 0) {
            /* Callee-first, as the driver orders it: the replay walks the ctor's
             * CURRENT SIR, so the pin must replay an OPTIMIZED ctor, not a
             * pristine one. */
            const sema_class_t* psc = sema_get_class(&sctx, p_id);
            int obj_id = psc ? psc->super_id : -1;
            for (int pass = 0; pass < 2; pass++) {
                int want = (pass == 0) ? obj_id : p_id;
                const sema_class_t* sc2 = sema_get_class(&sctx, want);
                for (int k = 0; k < mc; k++) {
                    if (ms[k]->class_id != want) continue;
                    if (sc2 && ms[k]->method_id >= 0
                        && ms[k]->method_id < (int)bbq_vec_len((void*)sc2->methods)
                        && sc2->methods[ms[k]->method_id].is_constructor)
                        sir_optimize(&cctx, k);
                }
            }
            int before = cctx.scalar_total;
            sir_optimize(&cctx, mi);
            CHECK(cctx.scalar_total > before && find_new_of_class(ms[mi]->entry, p_id) == NULL,
                  "PEA arg pin: the non-constant-arg ctor scalar-replaces (premise)");
            CHECK(count_tag(ms[mi]->entry, SIR_ADD) >= 1,
                  "PEA ctor replay keeps the argument's computation (q + 1 survives)");
        }
        sema_destroy(&sctx); bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_sir");
}
