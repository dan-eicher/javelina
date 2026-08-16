// test_scope_sidecar.c — the DDCG records a control-flow scope per
// loop/if (the sidecar the WASM structured emit reads instead of recovering
// structure). Pin that the records are present, correctly kinded, and anchored
// on the right Nop labels, in nesting order (inner-first, since the rules build
// inside-out). This is the foundation the if/while emit (7b) consumes.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/sir_support.h"     /* sir_succ / sir_child — fact-liveness walk */
#include "javelina/compiler/sir_optimizer.h"   /* sir_optimize — Click, which can delete a key */
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

/* Is `target` still in the method's graph? BFS over spine successors AND expression
 * children — a fact keyed on a node Click deleted is unreachable from the entry, and
 * the backend's lookup-by-node will never find it. */
static bool reachable_from(const sir_node_t* entry, const sir_node_t* target) {
    if (!entry || !target) return false;
    const sir_node_t* seen[2048]; int ns = 0;
    const sir_node_t* work[2048]; int wn = 0, wi = 0;
    work[wn++] = entry;
    while (wi < wn) {
        const sir_node_t* n = work[wi++];
        if (!n) continue;
        if (n == target) return true;
        bool dup = false;
        for (int i = 0; i < ns; i++) if (seen[i] == n) { dup = true; break; }
        if (dup || ns >= 2048) continue;
        seen[ns++] = n;
        for (int k = 0; k < sir_succ_count((sir_node_t*)n) && wn < 2048; k++)
            work[wn++] = sir_succ((sir_node_t*)n, k);
        for (int k = 0; k < sir_arity((sir_node_t*)n) && wn < 2048; k++)
            work[wn++] = sir_child((sir_node_t*)n, k);
    }
    return false;
}

/* Compile `src`, run Click over method `name`, and count how many recorded if-joins
 * (BLOCK scopes) had their KEY node deleted out from under them. Returns false if the
 * method is absent. Only scalars escape, so the contexts are local and released here. */
static bool orphaned_joins(bbq_arena* a, const char* src, const char* name,
                           int* out_total, int* out_orphans) {
    ast_program_t* prog = build_program(src, a);
    sema_ctx_t sctx; sema_init(&sctx, a); jtest_analyze(&sctx);
    compiler_ctx_t cctx; compiler_init(&cctx, a, &sctx);
    int mc = 0;
    sir_method_t** methods = compiler_compile(&cctx, prog, &mc);
    bool found = false;
    for (int i = 0; i < mc && !found; i++) {
        if (methods[i]->class_id < jtest_last_nlib) continue;
        if (!methods[i]->name || strcmp(methods[i]->name, name) != 0) continue;
        sir_optimize(&cctx, i);                       /* the pass that can delete the key */
        int nf = 0;
        const compiler_fact_t* f = compiler_get_facts(&cctx, i, &nf);
        int total = 0, orphans = 0;
        for (int j = 0; j < nf; j++) {
            if (f[j].kind != COMPILER_FACT_SCOPE) continue;
            if (f[j].a != COMPILER_SCOPE_BLOCK) continue;
            total++;
            if (!reachable_from(methods[i]->entry, f[j].key)) orphans++;
        }
        *out_total = total; *out_orphans = orphans;
        found = true;
    }
    sema_destroy(&sctx);
    return found;
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
        /* `a && b` is TWO Branches. Both records above are keyed on the chain HEAD
         * (the `a` test), so the `b` Branch carries nothing of its own. §2.2 intends
         * the head's framing to put the shared labels on the scope stack so `b`
         * resolves by br_depth — which holds only while the walk actually frames at
         * the head. This counts the distinct Branch keys, the same question that
         * caught the cast and div diamonds. */
        const sir_node_t* ak[16]; int nak = 0;
        for (int i = 0; i < n; i++)
            if ((s[i].a == COMPILER_SCOPE_MERGE || s[i].a == COMPILER_SCOPE_BLOCK)
                && s[i].key && s[i].key->tag == SIR_BRANCH) {
                int dup = 0;
                for (int j = 0; j < nak; j++) if (ak[j] == s[i].key) dup = 1;
                if (!dup && nak < 16) ak[nak++] = s[i].key;
            }
        printf("        &&: %d distinct Branch keys recorded\n", nak);
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

    /* §15.24 in a BOOLEAN-CONTROL position. `if (b ? p : q)` gens the conditional
     * with γ = pair(Lthen, Lelse); Fig. 5 hands that same γ to both arms, so each
     * ends in its OWN branch to the SHARED Lt and Lf — two shared destinations, two
     * MERGE records, and no value join (nothing converges to deliver a value). The
     * `_other` arm that used to swallow `pair` built a value join instead, from a
     * `cg_jump(pair, Lnext)` that calls a pair a caller error and returns Lnext —
     * so the if's arms were never branched to at all. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { void f(boolean p, boolean q, boolean b, int x){ if (b ? p : q) x=1; } }",
            "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 2,
              "ternary in test position: both shared destinations recorded");
        int paired = 0;
        for (int i = 0; i < n; i++)
            if (s[i].a == COMPILER_SCOPE_MERGE && s[i].key && s[i].key->tag == SIR_BRANCH)
                paired++;
        CHECK(paired == 2, "ternary in test position: both MERGEs keyed on the test Branch");
        bbq_arena_free(&a);
    }

    /* §14.6 a labelled BLOCK. `loop_frame` is the FRONTEND's break-target
     * environment; the backend needs the same label as a scope row or it frames
     * nothing and each `break L` emits the exit inline. Keyed on the body head —
     * "the branch head the backend will encounter top-down" (§2.1). */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x){ int r = 0;"
            " L: { if (x == 0) break L; r = 1; if (x == 1) break L; r = 2; }"
            " return r; } }", "f", &n);
        /* two inner ifs + the labelled block itself */
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK) == 3,
              "labelled block: its exit is recorded alongside the two if-joins");
        bbq_arena_free(&a);
    }

    /* §14.14 the CONTINUE target. Two rows, two consumers: kind 4 keyed on the
     * target itself is the OPTIMIZER's merge marker (nothing may be lifted across
     * the step), and a MERGE keyed on the BODY head is the structurer's emit-once
     * label. The second exists only when a `continue` actually reaches it — with no
     * continue the update has one reference and the paper emits no code for an
     * unreferenced label, so framing it would cost every for-loop a block. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x){ int s2 = 0;"
            " for (int i = 0; i < x; i++) { if (i == 2) continue; s2 = s2 + i; } return s2; } }",
            "f", &n);
        int cont = 0, self = 0;
        for (int i = 0; i < n; i++) if (s[i].a == 4) { cont++; if (s[i].key == s[i].aux) self++; }
        CHECK(cont == 1 && self == 1, "for+continue: the kind-4 row is keyed on the target itself");
        const compiler_fact_t* mg = NULL;
        for (int i = 0; i < n; i++) if (s[i].a == COMPILER_SCOPE_MERGE) mg = &s[i];
        CHECK(mg && mg->key != mg->aux,
              "for+continue: the MERGE row is keyed on the body head, not on the label");
        bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x){ int s2 = 0;"
            " for (int i = 0; i < x; i++) { s2 = s2 + i; } return s2; } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 0,
              "for with NO continue: the update is not a label, so no MERGE row");
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

    /* ── a ternary in VALUE context ───────────────────────────────────────────
     * The paper has no ternary rule: a conditional whose data destination is a
     * location is Figure 5's two-armed if with δ = that location, and p.12 states
     * the normalization outright (`E_bool ⇒ if E_bool (int 1) (int 0)`). Its arms
     * are therefore `CG_store O A L` (p.13) — both storing to the SAME location and
     * both `CG_jump L` to the SAME L. That shared L is a merge by §2's rule, no
     * different from the guarded div's arms pinned above.
     *
     * `ASCIIToBinaryBuffer.doubleValue` is built out of these
     * (`ieeeBits += overvalue ? -1 : 1;`) and emits its tail twice. This pin asks
     * the FIRST question — is the shared continuation recorded at all — so that a
     * green here moves the search to the backend's lookup and a red keeps it in the
     * frontend, instead of it being settled by reading code. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, boolean b){ int r = x; r = r + (b ? 0 - 1 : 1); return r; } }", "f", &n);
        int merges = COUNT_KIND(s,n,COMPILER_SCOPE_MERGE);
        int blocks = COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK);
        printf("        value ternary: %d MERGE, %d BLOCK\n", merges, blocks);
        CHECK(merges + blocks >= 1, "value ternary: the arms' shared continuation is recorded");
        bbq_arena_free(&a);
    }

    /* ── the shape that actually duplicates in the shipped prelude ─────────────
     * `ASCIIToBinaryBuffer.doubleValue` emits its tail twice, and the branches that
     * lose their join are synthesized (no srcloc), `SIR_EQ` conditions with
     * StoreLocal arms, five deep at method top level. The distinguishing ingredient
     * over the fixtures above is that the condition is an instance FIELD, not a
     * parameter: the read spills AND goes through `this`, so the recorded key is a
     * different node again. This harness carries types, so it can ask. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { boolean over; int bits;"
            " int f(){ bits = bits + (over ? 0 - 1 : 1);"
            "          bits = bits + (over ? 0 - 2 : 2);"
            "          bits = bits + (over ? 0 - 3 : 3); return bits; } }", "f", &n);
        int merges = COUNT_KIND(s,n,COMPILER_SCOPE_MERGE);
        int blocks = COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK);
        printf("        field-cond ternary x3: %d MERGE, %d BLOCK\n", merges, blocks);
        CHECK(merges + blocks >= 3,
              "field-cond ternary: one join recorded per ternary");
        bbq_arena_free(&a);
    }

    /* The same, with the field condition COMPARED — `SIR_EQ` with StoreLocal arms is
     * what the debugger showed at the failing branch, and a bare boolean field read
     * may not produce one. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int exp; double d;"
            " int f(int x){ x = x + (exp == 0 ? 1 : 2);"
            "               x = x + (d == 0.0 ? 3 : 4);"
            "               x = x + (exp == 1 || d == 1.0 ? 5 : 6); return x; } }", "f", &n);
        int merges = COUNT_KIND(s,n,COMPILER_SCOPE_MERGE);
        int blocks = COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK);
        printf("        field-cmp ternary x3: %d MERGE, %d BLOCK\n", merges, blocks);
        CHECK(merges + blocks >= 3,
              "field-cmp ternary: one join recorded per ternary");
        bbq_arena_free(&a);
    }

    /* ── a ternary INSIDE a short-circuit condition ────────────────────────────
     * `if ((b ? x : y) > 0 && x > 0)` is the minimal shape that makes the backend
     * emit a spine node twice (test_codegen_structured, "ternary in && cond"). Two
     * join-bearing constructs are nested in one condition: the `&&` chain, whose
     * MERGE is keyed on the chain HEAD, and the ternary beneath it, which mints its
     * own Ljoin and records a BLOCK on its own test head.
     *
     * This asks the frontend half only — are both records present and keyed on
     * distinct nodes? Green here puts the fault in the backend's lookup; red puts
     * it in the recording. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y, boolean b){"
            " if ((b ? x : y) > 0 && x > 0) x = x + 1; return x; } }", "f", &n);
        int merges = COUNT_KIND(s,n,COMPILER_SCOPE_MERGE);
        int blocks = COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK);
        printf("        ternary in && cond: %d MERGE, %d BLOCK\n", merges, blocks);
        CHECK(merges >= 1, "ternary in && cond: the chain's shared exit is recorded");
        CHECK(blocks >= 2, "ternary in && cond: the if AND the ternary each record a join");
        bbq_arena_free(&a);
    }

    /* ── LHS-ternary vs RHS-ternary: what differs in the RECORDS ───────────────
     * `if ((b?x:y) > 0)` duplicates; `if (0 < (b?x:y))` does not. Both select a
     * Figure-8 operand rule (binary_arith_cs / _sc) that is structurally identical
     * — same temp, same `single(binop_head)` to the complex operand, and both
     * RETURN that operand's head. So the two shapes should record the same thing.
     * Printing both says whether they do; if they do, the difference is not in the
     * recording and the search moves back to the backend. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16); int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y, boolean b){ if ((b ? x : y) > 0) x=x+1; return x; } }",
            "f", &n);
        const sir_node_t* k[8]; int nk = 0, rows = 0;
        for (int i = 0; i < n; i++)
            if (s[i].a == COMPILER_SCOPE_BLOCK) {
                rows++; int dup = 0;
                for (int j = 0; j < nk; j++) if (k[j] == s[i].key) dup = 1;
                if (!dup && nk < 8) k[nk++] = s[i].key;
            }
        printf("        LHS ternary: %d BLOCK rows on %d distinct keys\n", rows, nk);
        bbq_arena_free(&a);
    }
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16); int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, int y, boolean b){ if (0 < (b ? x : y)) x=x+1; return x; } }",
            "f", &n);
        const sir_node_t* k[8]; int nk = 0, rows = 0;
        for (int i = 0; i < n; i++)
            if (s[i].a == COMPILER_SCOPE_BLOCK) {
                rows++; int dup = 0;
                for (int j = 0; j < nk; j++) if (k[j] == s[i].key) dup = 1;
                if (!dup && nk < 8) k[nk++] = s[i].key;
            }
        printf("        RHS ternary: %d BLOCK rows on %d distinct keys\n", rows, nk);
        bbq_arena_free(&a);
    }

    /* ── a guard whose arms CONVERGE needs a record ────────────────────────────
     * §1 exempts the implicit-exception guards from a MERGE record on the grounds
     * that they "share nothing" — their throw arm terminates, so the ok arm is the
     * join's only reference. That premise is what the exemption rests on, and it is
     * false wherever a guard's throw arm can complete normally: then both arms reach
     * the continuation, the join has two references, and nothing recorded it.
     *
     * Measured in `ASCIIToBinaryBuffer.doubleValue`: five guard-shaped Branches
     * (true arm `Store(SIR_NEW)` — the allocating throw path) where emit_spine
     * reports the then-arm FELL THROUGH for all five. With no record and, at method
     * top level, no enclosing region bound, `ljoin` becomes NULL and both arms
     * absorb the method tail — 2^k for k such guards.
     *
     * The pin asks the frontend question: an array store needing a bounds guard,
     * in a method with a tail to duplicate, must leave a recorded join for every
     * Branch that can reach it from both sides. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int[] v, int i){ v[i] = 1; v[i+1] = 2; return v.length; } }",
            "f", &n);
        int guards = 0, recorded = 0;
        for (int i = 0; i < n; i++)
            if (s[i].key && s[i].key->tag == SIR_BRANCH) {
                guards++;
                if (s[i].a == COMPILER_SCOPE_MERGE || s[i].a == COMPILER_SCOPE_BLOCK)
                    recorded++;
            }
        printf("        guarded array stores: %d Branch-keyed rows, %d are join records\n",
               guards, recorded);
        bbq_arena_free(&a);
    }

    /* The SAME construct in TAIL position records NOTHING, and that is correct:
     * with γ = `ret` each arm mints its own Return and terminates, so there is no
     * shared node to label (§2.2's γ-cases; paper p.13, `CG_jump return ⇒ ret` —
     * return is a Label but never a branch target). Pinned so that a later change
     * which starts recording here — fabricating a join no arm points at — is a red
     * rather than a silent miscompile; the spec records that exact mistake breaking
     * 15 test_exec cases. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 16);
        int n = 0;
        const compiler_fact_t* s = scopes_of(&a,
            "class T { int f(int x, boolean b){ return b ? x : 0 - x; } }", "f", &n);
        CHECK(COUNT_KIND(s,n,COMPILER_SCOPE_MERGE) == 0
              && COUNT_KIND(s,n,COMPILER_SCOPE_BLOCK) == 0,
              "return ternary (γ=ret): arms terminate, nothing recorded");
        bbq_arena_free(&a);
    }

    /* ── a recorded if-join must SURVIVE Click ────────────────────────────────
     * `record_scope(test, Ljoin, 0)` keys the join on the condition's HEAD. When the
     * condition spills — a call, a cast, anything needing a temp — that head is a
     * StoreLocal rather than the Branch (pinned above for the simple case). If Click
     * then deletes that node, the fact is keyed on something no longer in the graph:
     * the backend's lookup-by-node finds nothing, `ljoin` falls back to the enclosing
     * region's end, and BOTH arms re-emit the method's whole tail — 2^k for k such ifs.
     *
     * The orphan is the defect, so the orphan is what this pins. Module size under -O
     * is the symptom several layers downstream, and the e2e pin that measured it
     * (test_wasm_module.c "an if's join must survive a spilled condition") read green
     * through this whole class because its fixture only ever spilled on the int path.
     *
     * A spill here is CORRECT and expected — a call must go through a temp. What must
     * not happen is the recorded join losing its anchor. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 18);
        int total = 0, orphans = 0;

        CHECK(orphaned_joins(&a,
                "class T { static boolean p(double v){ return v != v; }\n"
                "  static void g(){}\n"
                "  static void f(double y){ if (p(y)) g(); } }", "f", &total, &orphans),
              "call-in-condition: method compiles");
        CHECK(total > 0, "call-in-condition: an if-join was recorded at all");
        CHECK(orphans == 0, "a CALL in the condition keeps its if-join anchored through Click");

        total = orphans = 0;
        CHECK(orphaned_joins(&a,
                "class T { static void g(){}\n"
                "  static void f(long a, double y){ if (((double) a) == y) g(); } }",
                "f", &total, &orphans),
              "cast-in-condition: method compiles");
        CHECK(total > 0, "cast-in-condition: an if-join was recorded at all");
        CHECK(orphans == 0, "a CAST in the condition keeps its if-join anchored through Click");

        /* The already-fixed shape, kept as the control: a wide literal is Dybvig-simple,
         * so this one does not even spill. It must stay anchored too. */
        total = orphans = 0;
        CHECK(orphaned_joins(&a,
                "class T { static void g(){}\n"
                "  static void f(float d){ if (d > 2.0f) g(); } }", "f", &total, &orphans),
              "wide-literal condition: method compiles");
        CHECK(orphans == 0, "a wide-literal condition keeps its if-join anchored through Click");

        bbq_arena_free(&a);
    }

    /* ── A nested loop keeps its own LOOP record however big the outer body is ──
     *
     * The structured emit frames a loop only when loop_scope_at finds a record for
     * it; with no record the loop node is walked as ordinary spine, no `loop`
     * opcode is emitted, and the body runs once and falls through. nbody's
     * energy() hits exactly that: the inner pair loop runs one iteration per outer
     * pass. It is triggered by the SIZE of the statement preceding the inner loop
     * — six array-element reads in the outer body compile correctly, eight do
     * not — so the record count is pinned against both. */
    {
        bbq_arena a; bbq_arena_init(&a, 1 << 20);
        static const char* SMALL =
          "class B { double x,y,z,vx,vy,vz,mass; }"
          "class T { static double f(B[] a){ double e=0;"
          "  for (int i=0;i<a.length;++i){"
          "    e += 1.0;"
          "    for (int j=i+1;j<a.length;++j) e -= a[j].mass; }"
          "  return e; } }";
        /* Deep enough to have overflowed the old fixed stack. Depth is driven by
         * the bounds-check branch each array access contributes, not by source
         * nesting, so the outer body carries enough of them to saturate — the
         * shape nbody's energy() has, which ran its inner loop exactly once. */
        static const char* BIG =
          "class B { double x,y,z,vx,vy,vz,mass; }"
          "class T { static double f(B[] a){ double dx,dy,dz,d; double e=0;"
          "  for (int i=0;i<a.length;++i){"
          "    e += 0.5*a[i].mass*( a[i].vx*a[i].vx + a[i].vy*a[i].vy"
          "                       + a[i].vz*a[i].vz );"
          "    for (int j=i+1;j<a.length;++j){"
          "      dx=a[i].x-a[j].x; dy=a[i].y-a[j].y; dz=a[i].z-a[j].z;"
          "      d=Math.sqrt(dx*dx+dy*dy+dz*dz);"
          "      e -= (a[i].mass*a[j].mass)/d; } }"
          "  return e; } }";
        struct { const char* src; const char* what; } lc[] = {
          { SMALL, "small outer statement" }, { BIG, "large outer statement" },
        };
        for (int k = 0; k < 2; k++) {
            int n = 0;
            const compiler_fact_t* rows = scopes_of(&a, lc[k].src, "f", &n);
            int loops = 0;
            for (int i = 0; i < n; i++)
                if (rows && rows[i].a == COMPILER_SCOPE_LOOP) loops++;
            char lbl[128];
            snprintf(lbl, sizeof lbl,
                     "%s: BOTH loops record a LOOP scope (got %d)", lc[k].what, loops);
            CHECK(loops == 2, lbl);
        }
        bbq_arena_free(&a);
    }

    return TEST_SUMMARY("test_scope_sidecar");
}
