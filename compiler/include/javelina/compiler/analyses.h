/* analyses.h — CFG + lattice dataflow engine over sema's tagged AST. */
#ifndef JAVELINA_COMPILER_ANALYSES_H
#define JAVELINA_COMPILER_ANALYSES_H

#include "gen/java_ast.h"
#include "javelina/compiler/sema.h"
#include "bbq_arena.h"
#include "bbq_htree.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── CFG representation ───────────────────────────────────────── */

typedef enum {
    CFG_NODE_ENTRY,
    CFG_NODE_EXIT,
    CFG_NODE_STMT,
} cfg_node_kind_t;

typedef enum {
    CFG_EDGE_NORMAL,     /* fall-through / unconditional */
    CFG_EDGE_TRUE,       /* conditional test → true branch */
    CFG_EDGE_FALSE,      /* conditional test → false branch */
    CFG_EDGE_CASE,       /* switch case matched (guard = case value) */
    CFG_EDGE_DEFAULT,    /* switch default path */
    CFG_EDGE_EXCEPTION,  /* exception flow → catch handler or EXIT */
} cfg_edge_kind_t;

typedef struct cfg_node cfg_node_t;

typedef struct cfg_edge {
    cfg_node_t* from;
    cfg_node_t* to;
    cfg_edge_kind_t kind;
    const ast_expr_t* guard;   /* TRUE/FALSE: test expr; CASE: case value */
    bool is_back;              /* DFS-identified back-edge (loop) */
} cfg_edge_t;

struct cfg_node {
    cfg_node_kind_t kind;
    int id;                    /* dense index 0..node_count-1 */
    const ast_stmt_t* stmt;    /* NULL for ENTRY/EXIT */
    cfg_edge_t* succs;         /* bbq_vec */
    cfg_node_t** preds;        /* bbq_vec; populated after all edges added */
    /* Set only on synthetic catch-handler entry nodes; the owning
     * catch clause (for slot lookup, type access). NULL otherwise. */
    const ast_catch_clause_t* catch_clause;
};

typedef struct cfg {
    bbq_arena* arena;
    cfg_node_t* entry;
    cfg_node_t* exit;
    cfg_node_t** nodes;        /* bbq_vec, indexed by cfg_node_t.id */
} cfg_t;

/* JLS §14.19: can `s` complete normally, assuming it is reachable? The spec's structural
 * rules, not a dataflow result. Codegen consults it so it never builds a normal-completion
 * successor for a statement the language says has none. */
bool jls_can_complete_normally(const sema_ctx_t* ctx, const ast_stmt_t* s);

/* Same rules, plus: an expression statement calling a method in `nrset` (one that provably never
 * returns) cannot complete normally. Not a JLS rule — the leniency §8.4.7 is applied with. */
bool jls_can_complete_normally_nr(const sema_ctx_t* ctx, const ast_stmt_t* s,
                                  const bbq_htree* nrset);

/* JLS §14.19's reachability half: report "unreachable statement" for every statement no execution
 * can arrive at, and the noreturn-aware "dead code" warning for the extra ones `nrset` reveals.
 * The body of a method / constructor / static initializer is reachable by definition. */
void jls_check_reachability(sema_ctx_t* ctx, const ast_stmt_t* body, const bbq_htree* nrset);

/* JLS §15.27: is `e` a constant expression with value `true` / `false`? The ONE authority for the
 * question §14.19 asks of a while/do/for condition — the same question the CFG builder asks to
 * place the loop's edges, and the backend asks to decide such a loop has no test and no false
 * edge. (Evaluator: const_expr.c.) */
bool jls_is_constant_true (const sema_ctx_t* ctx, const ast_expr_t* e);
bool jls_is_constant_false(const sema_ctx_t* ctx, const ast_expr_t* e);

/* Build a CFG for `body` (typically a method body stmt). Node and
 * edge storage is allocated from `arena`. bbq_vec fields (succs,
 * preds, nodes) live on the heap and are released by cfg_destroy.
 * `sema` supplies the §15.27 evaluation §14.19's constant-condition special cases need; it is
 * the only expression VALUE the builder ever looks at. */
void cfg_build(cfg_t* out, const sema_ctx_t* sema, bbq_arena* arena, const ast_stmt_t* body);
void cfg_destroy(cfg_t* g);

/* Node count — convenience for state-table sizing. */
int cfg_node_count(const cfg_t* g);

/* ── Lattice + worklist engine ─────────────────────────────────── */

typedef struct lattice_ops lattice_ops_t;

struct lattice_ops {
    /* Bottom element — state at unvisited nodes. */
    void* (*bottom)(bbq_arena* a);
    /* Join at CFG meet points (non-back edges). */
    void* (*meet)(bbq_arena* a, const void* x, const void* y);
    /* Widening at back-edge meets. Required for lattices of infinite
     * height (intervals); others can leave NULL and the engine uses
     * meet. Takes `prev` (destination's current state) and
     * `incoming` (the new value arriving via the back-edge). */
    void* (*widen)(bbq_arena* a, const void* prev, const void* incoming);
    /* Monotone check x ≤ y in the lattice ordering. */
    bool  (*le)(const void* x, const void* y);
    /* Effect of traversing `edge`. `self` points at the ops struct
     * driving this run; lattices that carry state declare their
     * own struct with `lattice_ops_t ops` as first field and cast
     * `self` back to it. */
    void* (*transfer)(const lattice_ops_t* self, bbq_arena* a,
                      const cfg_edge_t* edge, const void* in);
};

/* Run Kildall's worklist to fixpoint. On return, out_state[i] holds
 * the in-state at the node with id i. `entry_state` seeds the entry
 * node; all other nodes start at bottom(). */
void cfg_fixpoint(const cfg_t* g, const lattice_ops_t* ops,
                  bbq_arena* arena, const void* entry_state,
                  void** out_state);

/* ── Nullability lattice ───────────────────────────────────────── */

typedef enum {
    NULL_BOT     = 0,   /* not observed */
    NULL_NULL    = 1,
    NULL_NONNULL = 2,
    NULL_MAYBE   = 3,
} null_val_t;

extern const lattice_ops_t nullability_ops;

/* Entry-state constructor: marks each name in `names` as MAYBE —
 * the default for reference-typed parameters at method entry. */
void* nullability_entry_state(bbq_arena* a,
                              const char* const* names, int count);

/* Look up the tracked nullability of `name` in a lattice state
 * produced by cfg_fixpoint with nullability_ops. */
null_val_t nullability_lookup(const void* state, const char* name);

/* ── Interval lattice ──────────────────────────────────────────── */

/* Closed interval [lo, hi] with INT64 ±∞ sentinels. BOT is represented
 * by lo > hi (specifically lo = INT64_MAX, hi = INT64_MIN). */
#define INTERVAL_POS_INF  INT64_MAX
#define INTERVAL_NEG_INF  INT64_MIN

typedef struct {
    int64_t lo;
    int64_t hi;
} interval_val_t;

static inline interval_val_t interval_bot(void) {
    interval_val_t v = { INTERVAL_POS_INF, INTERVAL_NEG_INF };
    return v;
}
static inline interval_val_t interval_top(void) {
    interval_val_t v = { INTERVAL_NEG_INF, INTERVAL_POS_INF };
    return v;
}
static inline interval_val_t interval_const(int64_t n) {
    interval_val_t v = { n, n };
    return v;
}
static inline bool interval_is_bot(interval_val_t v) {
    return v.lo > v.hi;
}
static inline bool interval_is_top(interval_val_t v) {
    return v.lo == INTERVAL_NEG_INF && v.hi == INTERVAL_POS_INF;
}

extern const lattice_ops_t interval_ops;

/* Entry-state constructor: every named ident is ⊤ (unknown). */
void* interval_entry_state(bbq_arena* a,
                           const char* const* names, int count);

/* Query the tracked interval for `name` in a state. */
interval_val_t interval_lookup(const void* state, const char* name);

/* ── Reachability lattice ──────────────────────────────────────── */

/* Per-node "can this node execute?" Two-point lattice with
 * noreturn-call awareness: NORMAL fallthrough from a stmt that
 * invokes a noreturn method propagates `unreachable`, not `reachable`. */
typedef struct reachability {
    lattice_ops_t ops;  /* must be first — transfer casts `self` here */
    const bbq_htree* noreturn_stmts;  /* ast_stmt_t* → sentinel; may be NULL */
} reachability_t;

/* State values are these two pointer-sized sentinels. */
#define REACHABILITY_BOTTOM ((void*)(uintptr_t)0)
#define REACHABILITY_ON     ((void*)(uintptr_t)1)

void reachability_init(reachability_t* r, const bbq_htree* noreturn_stmts);

/* ── Top-level entry ──────────────────────────────────────────── */

/* Run all analyses against the validated AST in `ctx`. Invoked from
 * sema_analyze after analyze_bodies. Diagnostics land in ctx->diags. */
void analyses_run(sema_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* JAVELINA_COMPILER_ANALYSES_H */
