// test_scope_sidecar.c — the DDCG records a control-flow scope per
// loop/if (the sidecar the WASM structured emit reads instead of recovering
// structure). Pin that the records are present, correctly kinded, and anchored
// on the right Nop labels, in nesting order (inner-first, since the rules build
// inside-out). This is the foundation the if/while emit (7b) consumes.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "gen/sir_ast.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

/* §7.3 per-unit parse (see jtest_units.h) — the flat program still feeds
 * compiler_compile; sema gets the unit list via jtest_analyze. */
#include "jtest_units.h"
#define build_program jtest_build_flat

/* Compile `src`, return method `name`'s SCOPE rows out of the one fact table (and
 * *n). The sidecar holds every kind — guards, allocs, regions — so this filters to
 * the kind under test; a SCOPE row carries (key = header, aux = exit, a = kind). */
#define MAXROWS 256
static compiler_fact_t scope_rows[MAXROWS];

static const compiler_fact_t* scopes_of(bbq_arena* a, const char* src,
                                        const char* name, int* n) {
    ast_program_t* prog = build_program(src, a);
    static sema_ctx_t sctx;            /* static: outlives this call for the test */
    static bool sctx_live = false;     /* ...so release the previous one here, */
    if (sctx_live) sema_destroy(&sctx);/* rather than abandon its 31 htrees. */
    sema_init(&sctx, a); sctx_live = true; jtest_analyze(&sctx);
    static compiler_ctx_t cctx;
    compiler_init(&cctx, a, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    for (int i = 0; i < mc; i++)
        if (methods[i]->class_id >= jtest_last_nlib &&   /* user snippet only */
            methods[i]->name && strcmp(methods[i]->name, name) == 0) {
            int nf = 0;
            const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
            int k = 0;
            for (int j = 0; j < nf && k < MAXROWS; j++)
                if (f[j].kind == COMPILER_FACT_SCOPE) scope_rows[k++] = f[j];
            *n = k;
            return scope_rows;
        }
    *n = 0; return NULL;
}

int main(void) {
    /* ── if → one BLOCK scope, KEYED by the test-head Branch, exit = Ljoin Nop ──
     * Dybvig Fig.5: the arms inherit the if's control destination γ (= Ljoin); the
     * frontend carries γ forward keyed by the test head, so the backend READS it
     * (one sidecar scan by node) rather than recomputing the merge by a walk. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x){ if (x > 0) { return 1; } return 0; } }", "f", &n);
        CHECK(n == 1, "if: exactly one scope");
        CHECK(s && s[0].a == COMPILER_SCOPE_BLOCK, "if: kind = BLOCK");
        CHECK(s && s[0].key && s[0].key->tag == SIR_BRANCH, "if: header is the test-head Branch");
        CHECK(s && s[0].aux && s[0].aux->tag == SIR_NOP, "if: exit is the Ljoin Nop");
        bbq_arena_free(&a);
    }

    /* ── while → one LOOP scope, Ltop + Lbreak both Nops ── */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { void f(int x){ while (x > 0) { x = x - 1; } } }", "f", &n);
        CHECK(n == 1, "while: exactly one scope");
        CHECK(s && s[0].a == COMPILER_SCOPE_LOOP, "while: kind = LOOP");
        CHECK(s && s[0].key && s[0].key->tag == SIR_NOP, "while: header is Ltop Nop");
        CHECK(s && s[0].aux && s[0].aux->tag == SIR_NOP, "while: exit is Lbreak Nop");
        CHECK(s && s[0].key != s[0].aux, "while: Ltop and Lbreak are distinct nodes");
        bbq_arena_free(&a);
    }

    /* ── nested (if inside while) → 2 scopes, inner-first (built inside-out) ── */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { void f(int x){ while (x > 0) { if (x > 5) { x = 1; } x = x - 1; } } }", "f", &n);
        CHECK(n == 2, "nested: two scopes");
        CHECK(s && s[0].a == COMPILER_SCOPE_BLOCK, "nested: inner if recorded first (BLOCK)");
        CHECK(s && s[1].a == COMPILER_SCOPE_LOOP, "nested: outer while recorded last (LOOP)");
        bbq_arena_free(&a);
    }

    /* ── MERGE records (docs/ddcg-merge-labels.md §2.1): a shared control label the
     * ddcg emits once. Helpers count kind occurrences and locate a record. ── */
    #define COUNT_KIND(S,N,K) ({ int _c=0; for(int _i=0;_i<(N);_i++) if((S)[_i].a==(K)) _c++; _c; })

    /* && in if-else: shortcircuit records ONE MERGE (the shared else Lf), keyed on
     * the SAME test-head Branch as the if's BLOCK join (Fig. 7). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y){ if (x > 0 && y > 0) { return 1; } else { return 2; } } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 1, "&&: one MERGE record");
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK) == 1, "&&: one BLOCK (if-join) record");
        const compiler_fact_t *mg=NULL,*bl=NULL;
        for (int i=0;i<n;i++){ if(s[i].a==COMPILER_SCOPE_MERGE) mg=&s[i]; if(s[i].a==COMPILER_SCOPE_BLOCK) bl=&s[i]; }
        CHECK(mg && bl && mg->key == bl->key, "&&: MERGE and BLOCK keyed on the same test head");
        CHECK(mg && mg->aux && mg->aux != (bl?bl->aux:NULL), "&&: MERGE exit (shared else) != if-join");
        bbq_arena_free(&a);
    }

    /* || mirrors && — one MERGE (the shared then Lt). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y){ if (x > 0 || y > 0) { return 1; } else { return 2; } } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 1, "||: one MERGE record");
        bbq_arena_free(&a);
    }

    /* else-if && chain: one MERGE per level (each level's shared else). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { void f(int x, int y){ if (x>0 && y>0) x=1; else if (x>1 && y>1) x=2;"
            " else if (x>2 && y>2) x=3; else x=0; } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 3, "else-if && chain: one MERGE per level");
        bbq_arena_free(&a);
    }

    /* guarded int div (single-label γ): the zero/-1 guard records a MERGE (its arms'
     * shared continuation), kind MERGE, keyed on the guard Branch. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y){ int z = x / y; return z + 1; } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 1, "guarded div: one MERGE record");
        const compiler_fact_t* mg=NULL; for(int i=0;i<n;i++) if(s[i].a==COMPILER_SCOPE_MERGE) mg=&s[i];
        CHECK(mg && mg->key && mg->key->tag == SIR_BRANCH, "guarded div: MERGE keyed on the guard Branch");
        bbq_arena_free(&a);
    }

    /* checked ref cast in a single-label γ (local init): the null/isInstance diamond
     * records a MERGE (ok_null/ok_inst share the γ continuation). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { void f(Object a){ String b = (String) a; b.length(); } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) >= 1, "ref cast: a MERGE record for the diamond tail");
        bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_scope_sidecar");
}
