/* analyses.c — CFG construction + dataflow engine. */

#include "javelina/compiler/analyses.h"
#include "javelina/compiler/const_expr.h"
#include "javelina/compiler/jbound.h"   /* the bound-arithmetic core */
#include "bbq_vec.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Builder state (lives only during cfg_build) ───────────────── */

typedef struct {
    cfg_node_t* node;
    cfg_edge_kind_t kind;
    const ast_expr_t* guard;
} pending_t;

typedef struct {
    cfg_node_t* cont_target;   /* loop header; NULL for switch */
    pending_t* break_pending;  /* bbq_vec — break stmts stash fall-out here */
    const char* label;         /* NULL when unlabeled */
} frame_t;

typedef struct {
    cfg_node_t** handlers;     /* bbq_vec — one entry node per catch clause */
} try_frame_t;

typedef struct {
    bbq_arena* arena;
    cfg_t* g;
    pending_t* pending;        /* bbq_vec */
    frame_t* frames;           /* bbq_vec, innermost last */
    try_frame_t* tries;        /* bbq_vec, innermost last */
    const char* next_label;    /* carried from enclosing AST_LABELED */
    /* §14.19: "Except for the special treatment of while, do, and for statements whose condition
     * expression has the constant value true, the values of expressions are not taken into account
     * in the flow analysis." Those special cases — and the mirror-image constant `false`, which
     * makes a loop's contained statement unreachable — are the ONLY places this builder consults an
     * expression's value, and it consults the §15.27 evaluator to do it (never a literal check). */
    const sema_ctx_t* sema;
} builder_t;

/* ── Node / edge primitives ────────────────────────────────────── */

static cfg_node_t* mk_node(builder_t* b, cfg_node_kind_t k, const ast_stmt_t* s) {
    cfg_node_t* n = (cfg_node_t*)bbq_arena_alloc(b->arena, sizeof(*n));
    memset(n, 0, sizeof(*n));
    n->kind = k;
    n->stmt = s;
    n->id = bbq_vec_len(b->g->nodes);
    bbq_vec_push(b->g->nodes, n);
    return n;
}

static void add_edge(cfg_node_t* from, cfg_node_t* to,
                     cfg_edge_kind_t k, const ast_expr_t* guard) {
    cfg_edge_t e;
    e.from = from;
    e.to = to;
    e.kind = k;
    e.guard = guard;
    e.is_back = false;
    bbq_vec_push(from->succs, e);
}

static void attach_pending(builder_t* b, cfg_node_t* to) {
    for (int i = 0; i < bbq_vec_len(b->pending); i++) {
        pending_t p = b->pending[i];
        add_edge(p.node, to, p.kind, p.guard);
    }
    bbq_vec_clear(b->pending);
}

static void push_pending(builder_t* b, cfg_node_t* n,
                         cfg_edge_kind_t k, const ast_expr_t* guard) {
    pending_t p;
    p.node = n;
    p.kind = k;
    p.guard = guard;
    bbq_vec_push(b->pending, p);
}

/* Over-approximation: every stmt inside a try body may throw,
 * so wire it to handlers of the innermost enclosing try. Stmts
 * outside any try get no exception edge — the throw leaves the
 * method and has no intra-method effect worth tracking. */
static void add_exception_edges(builder_t* b, cfg_node_t* n) {
    int depth = bbq_vec_len(b->tries);
    if (depth == 0) return;
    try_frame_t* tf = &b->tries[depth - 1];
    for (int j = 0; j < bbq_vec_len(tf->handlers); j++)
        add_edge(n, tf->handlers[j], CFG_EDGE_EXCEPTION, NULL);
}

static frame_t* find_frame(builder_t* b, const char* label, bool need_cont) {
    for (int i = bbq_vec_len(b->frames) - 1; i >= 0; i--) {
        frame_t* f = &b->frames[i];
        if (need_cont && !f->cont_target) continue;
        if (label) {
            if (f->label && strcmp(f->label, label) == 0) return f;
        } else {
            return f;
        }
    }
    return NULL;
}

/* Move all entries of b->pending onto `sink`, clear b->pending. */
static void drain_pending_to(builder_t* b, pending_t** sink) {
    for (int i = 0; i < bbq_vec_len(b->pending); i++)
        bbq_vec_push(*sink, b->pending[i]);
    bbq_vec_clear(b->pending);
}

static void redirect_pending(builder_t* b, cfg_node_t* to) {
    for (int i = 0; i < bbq_vec_len(b->pending); i++) {
        pending_t p = b->pending[i];
        add_edge(p.node, to, p.kind, p.guard);
    }
    bbq_vec_clear(b->pending);
}

/* ── Statement walker ──────────────────────────────────────────── */

static void build_stmt(builder_t* b, const ast_stmt_t* s);

static void build_block_stmts(builder_t* b, ast_stmt_t** stmts, int n) {
    for (int i = 0; i < n; i++) build_stmt(b, stmts[i]);
}

static void build_stmt(builder_t* b, const ast_stmt_t* s) {
    if (!s) return;
    switch (s->tag) {
    case AST_BLOCK:
        build_block_stmts(b, s->block.stmts, s->block.stmts_count);
        return;

    case AST_EMPTY:
        return;

    case AST_EXPRSTMT:
    case AST_LOCALVARDECL: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        add_exception_edges(b, n);
        push_pending(b, n, CFG_EDGE_NORMAL, NULL);
        return;
    }

    case AST_RETURN: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        add_exception_edges(b, n);
        add_edge(n, b->g->exit, CFG_EDGE_NORMAL, NULL);
        return;
    }

    case AST_THROW: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        int depth = bbq_vec_len(b->tries);
        if (depth > 0) {
            try_frame_t* tf = &b->tries[depth - 1];
            for (int j = 0; j < bbq_vec_len(tf->handlers); j++)
                add_edge(n, tf->handlers[j], CFG_EDGE_EXCEPTION, NULL);
        } else {
            add_edge(n, b->g->exit, CFG_EDGE_EXCEPTION, NULL);
        }
        return;
    }

    case AST_BREAK: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        frame_t* f = find_frame(b, s->break_.label, false);
        if (f) {
            pending_t p;
            p.node = n;
            p.kind = CFG_EDGE_NORMAL;
            p.guard = NULL;
            bbq_vec_push(f->break_pending, p);
        }
        return;
    }

    case AST_CONTINUE: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        frame_t* f = find_frame(b, s->continue_.label, true);
        if (f) add_edge(n, f->cont_target, CFG_EDGE_NORMAL, NULL);
        return;
    }

    case AST_IF: {
        cfg_node_t* n = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, n);
        add_exception_edges(b, n);

        push_pending(b, n, CFG_EDGE_TRUE, s->if_.test);
        build_stmt(b, s->if_.then);
        pending_t* then_end = NULL;
        drain_pending_to(b, &then_end);

        push_pending(b, n, CFG_EDGE_FALSE, s->if_.test);
        if (s->if_.else_) build_stmt(b, s->if_.else_);

        for (int i = 0; i < bbq_vec_len(then_end); i++)
            bbq_vec_push(b->pending, then_end[i]);
        bbq_vec_free(then_end);
        return;
    }

    case AST_WHILE: {
        cfg_node_t* header = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, header);
        add_exception_edges(b, header);

        frame_t frame;
        frame.cont_target = header;
        frame.break_pending = NULL;
        frame.label = b->next_label;
        b->next_label = NULL;
        bbq_vec_push(b->frames, frame);

        /* §14.19: "The contained statement is reachable iff the while statement is reachable and
         * the condition expression is not a constant expression whose value is false." */
        if (!jls_is_constant_false(b->sema, s->while_.test))
            push_pending(b, header, CFG_EDGE_TRUE, s->while_.test);
        build_stmt(b, s->while_.body);
        redirect_pending(b, header);

        frame_t finished = b->frames[bbq_vec_len(b->frames) - 1];
        bbq__vec_hdr(b->frames)->len--;

        /* §14.19: "A while statement can complete normally iff … the condition expression is not a
         * constant expression with value true; [or] there is a reachable break…". So a constant-true
         * condition has no false exit — the code after the loop is reachable only via `break`.
         * Omitting the FALSE edge also keeps definite-assignment precise (the after-loop state is
         * the meet of the break points, not the pre-body header). */
        if (!jls_is_constant_true(b->sema, s->while_.test))
            push_pending(b, header, CFG_EDGE_FALSE, s->while_.test);
        for (int i = 0; i < bbq_vec_len(finished.break_pending); i++)
            bbq_vec_push(b->pending, finished.break_pending[i]);
        bbq_vec_free(finished.break_pending);
        return;
    }

    case AST_DOWHILE: {
        /* cond node at the loop tail; body-start is the first node
         * created while walking body (or cond itself if body is empty).
         * Unlike a while/for header, this node does NOT stand for the statement's entry (the
         * body's first node consumes the incoming edge) — it stands for the tail condition
         * EXPRESSION. */
        cfg_node_t* cond = mk_node(b, CFG_NODE_STMT, s);
        int body_start_idx = bbq_vec_len(b->g->nodes);

        frame_t frame;
        frame.cont_target = cond;
        frame.break_pending = NULL;
        frame.label = b->next_label;
        b->next_label = NULL;
        bbq_vec_push(b->frames, frame);

        /* Initial pending still in b->pending — body's first real node
         * will consume it. Empty body: pending rolls through to cond. */
        build_stmt(b, s->do_while.body);
        redirect_pending(b, cond);

        frame_t finished = b->frames[bbq_vec_len(b->frames) - 1];
        bbq__vec_hdr(b->frames)->len--;

        add_exception_edges(b, cond);
        cfg_node_t* body_start = (body_start_idx < bbq_vec_len(b->g->nodes))
                               ? b->g->nodes[body_start_idx] : cond;
        /* A constant-false tail test never takes the back-edge; a constant-true one never exits
         * (§14.19: "A do statement can complete normally iff the contained statement can complete
         * normally and the condition expression is not a constant expression with value true"). */
        if (!jls_is_constant_false(b->sema, s->do_while.test))
            add_edge(cond, body_start, CFG_EDGE_TRUE, s->do_while.test);

        if (!jls_is_constant_true(b->sema, s->do_while.test))
            push_pending(b, cond, CFG_EDGE_FALSE, s->do_while.test);
        for (int i = 0; i < bbq_vec_len(finished.break_pending); i++)
            bbq_vec_push(b->pending, finished.break_pending[i]);
        bbq_vec_free(finished.break_pending);
        return;
    }

    case AST_FOR: {
        if (s->for_.init) build_stmt(b, s->for_.init);

        cfg_node_t* header = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, header);
        add_exception_edges(b, header);

        /* Update region: one synthetic AST_EXPRSTMT per update
         * expression, chained NORMAL so their effects compose in
         * order. apply_stmt picks up each expr's assignment. */
        cfg_node_t* update_head = NULL;
        cfg_node_t* update_tail = NULL;
        for (int i = 0; i < s->for_.update_count; i++) {
            ast_stmt_t* upd = (ast_stmt_t*)bbq_arena_alloc(b->arena, sizeof(*upd));
            memset(upd, 0, sizeof(*upd));
            upd->tag = AST_EXPRSTMT;
            upd->loc = s->for_.update[i]->loc;
            upd->expr_stmt.e = s->for_.update[i];
            cfg_node_t* un = mk_node(b, CFG_NODE_STMT, upd);
            if (!update_head) update_head = un;
            if (update_tail) add_edge(update_tail, un, CFG_EDGE_NORMAL, NULL);
            update_tail = un;
        }

        frame_t frame;
        frame.cont_target = update_head ? update_head : header;
        frame.break_pending = NULL;
        frame.label = b->next_label;
        b->next_label = NULL;
        bbq_vec_push(b->frames, frame);

        /* §14.19: "The contained statement is reachable iff the for statement is reachable and the
         * condition expression is not a constant expression whose value is false." An ABSENT
         * condition behaves as the constant `true`. */
        bool has_test = (s->for_.test != NULL);
        if (!has_test)
            push_pending(b, header, CFG_EDGE_NORMAL, NULL);
        else if (!jls_is_constant_false(b->sema, s->for_.test))
            push_pending(b, header, CFG_EDGE_TRUE, s->for_.test);

        build_stmt(b, s->for_.body);

        if (update_head) {
            redirect_pending(b, update_head);
            add_edge(update_tail, header, CFG_EDGE_NORMAL, NULL);
        } else {
            redirect_pending(b, header);
        }

        frame_t finished = b->frames[bbq_vec_len(b->frames) - 1];
        bbq__vec_hdr(b->frames)->len--;

        /* §14.19: "A for statement can complete normally iff … there is a condition expression, and
         * the condition expression is not a constant expression with value true; [or] there is a
         * reachable break…" — so `for (;;)` and `for (; true;)` have no false exit. */
        if (has_test && !jls_is_constant_true(b->sema, s->for_.test))
            push_pending(b, header, CFG_EDGE_FALSE, s->for_.test);
        for (int i = 0; i < bbq_vec_len(finished.break_pending); i++)
            bbq_vec_push(b->pending, finished.break_pending[i]);
        bbq_vec_free(finished.break_pending);
        return;
    }

    case AST_SWITCH: {
        cfg_node_t* sel = mk_node(b, CFG_NODE_STMT, s);
        attach_pending(b, sel);
        add_exception_edges(b, sel);

        frame_t frame;
        frame.cont_target = NULL;
        frame.break_pending = NULL;
        frame.label = b->next_label;
        b->next_label = NULL;
        bbq_vec_push(b->frames, frame);

        bool saw_default = false;
        for (int ci = 0; ci < s->switch_.cases_count; ci++) {
            ast_switch_case_t* sc = s->switch_.cases[ci];
            if (sc->value)
                push_pending(b, sel, CFG_EDGE_CASE, sc->value);
            else {
                push_pending(b, sel, CFG_EDGE_DEFAULT, NULL);
                saw_default = true;
            }
            build_block_stmts(b, sc->stmts, sc->stmts_count);
        }

        frame_t finished = b->frames[bbq_vec_len(b->frames) - 1];
        bbq__vec_hdr(b->frames)->len--;

        if (!saw_default)
            push_pending(b, sel, CFG_EDGE_DEFAULT, NULL);
        for (int i = 0; i < bbq_vec_len(finished.break_pending); i++)
            bbq_vec_push(b->pending, finished.break_pending[i]);
        bbq_vec_free(finished.break_pending);
        return;
    }

    case AST_TRY: {
        /* Snapshot pre-try pending for finally seeding: even if every
         * path through try+catches exits via return/throw (clearing
         * the natural fall-through), finally still runs. Feeding the
         * pre-try pending in gives finally a predecessor. */
        pending_t* pre_pending = NULL;
        if (s->try_.finally_) {
            for (int i = 0; i < bbq_vec_len(b->pending); i++)
                bbq_vec_push(pre_pending, b->pending[i]);
        }

        try_frame_t tf;
        tf.handlers = NULL;
        for (int i = 0; i < s->try_.catches_count; i++) {
            ast_catch_clause_t* cc = s->try_.catches[i];
            cfg_node_t* h = mk_node(b, CFG_NODE_STMT, cc->body);
            h->catch_clause = cc;
            bbq_vec_push(tf.handlers, h);
        }
        /* Seed each handler with exception edges from pending-into-try. */
        for (int j = 0; j < bbq_vec_len(b->pending); j++) {
            pending_t p = b->pending[j];
            for (int k = 0; k < bbq_vec_len(tf.handlers); k++)
                add_edge(p.node, tf.handlers[k], CFG_EDGE_EXCEPTION, NULL);
        }
        bbq_vec_push(b->tries, tf);

        build_stmt(b, s->try_.body);

        pending_t* all_ends = NULL;
        drain_pending_to(b, &all_ends);

        try_frame_t finished = b->tries[bbq_vec_len(b->tries) - 1];
        bbq__vec_hdr(b->tries)->len--;

        for (int i = 0; i < s->try_.catches_count; i++) {
            push_pending(b, finished.handlers[i], CFG_EDGE_NORMAL, NULL);
            build_stmt(b, s->try_.catches[i]->body);
            drain_pending_to(b, &all_ends);
        }
        bbq_vec_free(finished.handlers);

        if (s->try_.finally_) {
            /* Build finally with pre-try pending as its entry preds.
             * Discard finally-tail afterwards: post-try reachability
             * is governed by the real body+catches fall-out (all_ends),
             * not by finally's structural fall-through. */
            for (int i = 0; i < bbq_vec_len(pre_pending); i++)
                bbq_vec_push(b->pending, pre_pending[i]);
            build_stmt(b, s->try_.finally_);
            bbq_vec_clear(b->pending);
            bbq_vec_free(pre_pending);
        }

        for (int i = 0; i < bbq_vec_len(all_ends); i++)
            bbq_vec_push(b->pending, all_ends[i]);
        bbq_vec_free(all_ends);
        return;
    }

    case AST_LABELED: {
        const char* saved = b->next_label;
        b->next_label = s->labeled.label;
        build_stmt(b, s->labeled.body);
        b->next_label = saved;
        return;
    }
    }
}

/* ── Public API ────────────────────────────────────────────────── */

static cfg_node_t* mk_sentinel(bbq_arena* a, cfg_t* g, cfg_node_kind_t k) {
    cfg_node_t* n = (cfg_node_t*)bbq_arena_alloc(a, sizeof(*n));
    memset(n, 0, sizeof(*n));
    n->kind = k;
    n->id = bbq_vec_len(g->nodes);
    bbq_vec_push(g->nodes, n);
    return n;
}

/* ── JLS §14.19 "can complete normally" ────────────────────────────
 *
 * The spec's own structural recursion, transcribed rule for rule. It is deliberately NOT a
 * dataflow result: §14.19 says "Except for the special treatment of while, do, and for
 * statements whose condition expression has the constant value true, the values of
 * expressions are not taken into account in the flow analysis."
 *
 * Every rule below is stated "assuming S is reachable", which is how callers use it: a
 * statement that cannot complete normally has no normal-completion successor, so codegen
 * must not build one (a `try` whose block cannot complete normally has no normal exit, and
 * therefore no inlined-finally-then-join chain).
 *
 * Where a rule needs "there is a reachable break that exits S", we ask only whether such a
 * break EXISTS syntactically. That is conservative in the safe direction: claiming a
 * statement can complete normally when it cannot merely leaves an unreachable successor in
 * the graph (which is what the compiler did for every statement before this analysis
 * existed); claiming the reverse would delete a live path. */

/* §14.19's special treatment of a while/do/for condition that "is a constant expression with value
 * true" reads §15.27's evaluator (const_expr.c) — the ONE authority, shared with the backend's loop
 * lowering, so `while (true)`, `while (1 == 1)`, `while (CONST)` and `for (;;)` agree everywhere. */
bool jls_is_constant_true(const sema_ctx_t* ctx, const ast_expr_t* e) {
    return jls_const_is_true(ctx, e);
}

/* The mirror image, which §14.19 needs for "the condition expression is not a constant expression
 * whose value is false" — the rule that makes `while (false) { x = 3; }` an unreachable statement
 * while `if (false) { x = 3; }` stays legal. */
bool jls_is_constant_false(const sema_ctx_t* ctx, const ast_expr_t* e) {
    return jls_const_is_false(ctx, e);
}

/* Does a `break` that exits `owner` occur inside `s`? `depth` counts the breakable statements
 * (loops and switches) entered since `owner`, so an unlabeled break belongs to `owner` only at
 * depth 0. A labeled break belongs to `owner` iff `owner_label` names it. */
static bool has_break_exiting(const ast_stmt_t* s, const char* owner_label, int depth) {
    if (!s) return false;
    switch (s->tag) {
    case AST_BREAK:
        return s->break_.label ? (owner_label && strcmp(s->break_.label, owner_label) == 0)
                               : (depth == 0);
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            if (has_break_exiting(s->block.stmts[i], owner_label, depth)) return true;
        return false;
    case AST_IF:
        return has_break_exiting(s->if_.then, owner_label, depth)
            || has_break_exiting(s->if_.else_, owner_label, depth);
    case AST_LABELED:
        return has_break_exiting(s->labeled.body, owner_label, depth);
    /* Entering a nested loop/switch captures unlabeled breaks. */
    case AST_WHILE:   return has_break_exiting(s->while_.body,   owner_label, depth + 1);
    case AST_DOWHILE: return has_break_exiting(s->do_while.body, owner_label, depth + 1);
    case AST_FOR:     return has_break_exiting(s->for_.body,     owner_label, depth + 1);
    case AST_SWITCH:
        for (int i = 0; i < s->switch_.cases_count; i++) {
            const ast_switch_case_t* c = s->switch_.cases[i];
            for (int j = 0; j < c->stmts_count; j++)
                if (has_break_exiting(c->stmts[j], owner_label, depth + 1)) return true;
        }
        return false;
    case AST_TRY:
        if (has_break_exiting(s->try_.body, owner_label, depth)) return true;
        for (int i = 0; i < s->try_.catches_count; i++)
            if (has_break_exiting(s->try_.catches[i]->body, owner_label, depth)) return true;
        return has_break_exiting(s->try_.finally_, owner_label, depth);
    default:
        return false;
    }
}

/* Does `s` contain a `break L` whose label L is bound OUTSIDE `s`? Such a break leaves `s`, so
 * for a loop it reaches the loop's exit or somewhere past it. §14.13: `break L` completes the
 * labeled statement named L normally — and when L labels this very loop (the common
 * `outer: while (true) { … break outer; }`), that program point IS the loop's exit, which the
 * compiler represents with the loop's own break target. We cannot tell from the loop node which
 * labels are attached to it, so any free labeled break counts. Erring this way only leaves an
 * unreachable successor in the graph; erring the other way would delete a live one.
 *
 * `bound` is the label chain introduced by AST_LABELED statements inside `s`. */
typedef struct label_link { const char* name; const struct label_link* outer; } label_link_t;

static bool label_is_bound(const char* name, const label_link_t* bound) {
    for (const label_link_t* l = bound; l; l = l->outer)
        if (l->name && name && strcmp(l->name, name) == 0) return true;
    return false;
}

static bool has_free_labeled_break(const ast_stmt_t* s, const label_link_t* bound) {
    if (!s) return false;
    switch (s->tag) {
    case AST_BREAK:
        return s->break_.label && !label_is_bound(s->break_.label, bound);
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            if (has_free_labeled_break(s->block.stmts[i], bound)) return true;
        return false;
    case AST_IF:
        return has_free_labeled_break(s->if_.then, bound)
            || has_free_labeled_break(s->if_.else_, bound);
    case AST_LABELED: {
        label_link_t link = { s->labeled.label, bound };
        return has_free_labeled_break(s->labeled.body, &link);
    }
    case AST_WHILE:   return has_free_labeled_break(s->while_.body,   bound);
    case AST_DOWHILE: return has_free_labeled_break(s->do_while.body, bound);
    case AST_FOR:     return has_free_labeled_break(s->for_.body,     bound);
    case AST_SWITCH:
        for (int i = 0; i < s->switch_.cases_count; i++) {
            const ast_switch_case_t* c = s->switch_.cases[i];
            for (int j = 0; j < c->stmts_count; j++)
                if (has_free_labeled_break(c->stmts[j], bound)) return true;
        }
        return false;
    case AST_TRY:
        if (has_free_labeled_break(s->try_.body, bound)) return true;
        for (int i = 0; i < s->try_.catches_count; i++)
            if (has_free_labeled_break(s->try_.catches[i]->body, bound)) return true;
        return has_free_labeled_break(s->try_.finally_, bound);
    default:
        return false;
    }
}

/* A loop's exit is reachable iff an unlabeled break at this level, or a labeled break that
 * escapes the loop body, can transfer control to it. */
static bool loop_exit_reachable(const ast_stmt_t* body) {
    return has_break_exiting(body, NULL, 0) || has_free_labeled_break(body, NULL);
}

/* The same question for a switch: "there is a reachable break statement that exits the switch
 * statement". Its block is the concatenation of the case groups' statements. */
static bool switch_exit_reachable(const ast_stmt_t* s) {
    for (int i = 0; i < s->switch_.cases_count; i++) {
        const ast_switch_case_t* c = s->switch_.cases[i];
        for (int j = 0; j < c->stmts_count; j++)
            if (has_break_exiting(c->stmts[j], NULL, 0) || has_free_labeled_break(c->stmts[j], NULL))
                return true;
    }
    return false;
}

/* The rules below are shared by two callers with different leniency:
 *   nrset == NULL — §14.19 exactly. Drives the "unreachable statement" compile-time error.
 *   nrset != NULL — the same rules, plus the knowledge that an expression statement calling a
 *                   method that never returns cannot complete normally. Not a JLS rule; it lets
 *                   `if (c) return 1; else fail();` pass §8.4.5 instead of demanding a throw.
 * ONE set of rules, two readings — never two implementations. */
static bool ccn(const sema_ctx_t* ctx, const ast_stmt_t* s, const bbq_htree* nrset);

bool jls_can_complete_normally(const sema_ctx_t* ctx, const ast_stmt_t* s) {
    return ccn(ctx, s, NULL);
}
bool jls_can_complete_normally_nr(const sema_ctx_t* ctx, const ast_stmt_t* s,
                                  const bbq_htree* nrset) {
    return ccn(ctx, s, nrset);
}

static bool ccn(const sema_ctx_t* ctx, const ast_stmt_t* s, const bbq_htree* nrset) {
    if (!s) return true;
    switch (s->tag) {

    /* "A break, continue, return, or throw statement cannot complete normally." */
    case AST_BREAK: case AST_CONTINUE: case AST_RETURN: case AST_THROW:
        return false;

    /* "An empty block ... can complete normally iff it is reachable. A nonempty block ...
     * can complete normally iff the last statement in it can complete normally." Walking the
     * whole list is the same thing: each statement is reachable iff its predecessor can
     * complete normally, so the block completes normally iff the walk survives to the end. */
    case AST_BLOCK: {
        for (int i = 0; i < s->block.stmts_count; i++)
            if (!ccn(ctx, s->block.stmts[i], nrset)) return false;
        return true;
    }

    /* "A local variable declaration statement / an empty statement / an expression statement
     * can complete normally iff it is reachable." */
    case AST_LOCALVARDECL: case AST_EMPTY:
        return true;
    case AST_EXPRSTMT:
        /* The one extension beyond §14.19 (only when a noreturn set is supplied): a call to a
         * method that provably never returns does not complete normally. */
        return !(nrset && bbq_htree_contains(nrset, (uint32_t)(uintptr_t)s));

    /* "ACTUAL: An if-then statement can complete normally iff it is reachable." (The rule is
     * deliberately NOT the hypothetical one, so `if (false) x = 3;` stays legal.)
     * "ACTUAL: An if-then-else statement can complete normally iff the then-statement can
     * complete normally or the else-statement can complete normally." */
    case AST_IF:
        if (!s->if_.else_) return true;
        return ccn(ctx, s->if_.then, nrset) || ccn(ctx, s->if_.else_, nrset);

    /* "A labeled statement can complete normally if at least one of the following is true:
     * the contained statement can complete normally; there is a reachable break statement
     * that exits the labeled statement." */
    case AST_LABELED:
        return ccn(ctx, s->labeled.body, nrset)
            || has_break_exiting(s->labeled.body, s->labeled.label, 0);

    /* "A while statement can complete normally iff at least one of the following is true:
     * the while statement is reachable and the condition expression is not a constant
     * expression with value true; there is a reachable break statement that exits it." */
    case AST_WHILE:
        return !jls_is_constant_true(ctx, s->while_.test)
            || loop_exit_reachable(s->while_.body);

    /* "A do statement can complete normally iff at least one of the following is true: the
     * contained statement can complete normally and the condition expression is not a
     * constant expression with value true; there is a reachable break that exits it." */
    case AST_DOWHILE:
        return (ccn(ctx, s->do_while.body, nrset)
                    && !jls_is_constant_true(ctx, s->do_while.test))
            || loop_exit_reachable(s->do_while.body);

    /* "A for statement can complete normally iff at least one of the following is true: the
     * for statement is reachable, THERE IS A CONDITION EXPRESSION, and the condition
     * expression is not a constant expression with value true; there is a reachable break
     * that exits it." — `for (;;)` and `while (true)` are the same statement here. */
    case AST_FOR:
        return (s->for_.test != NULL && !jls_is_constant_true(ctx, s->for_.test))
            || loop_exit_reachable(s->for_.body);

    /* "A try statement can complete normally iff both of the following are true: the try
     * block can complete normally or any catch block can complete normally; if the try
     * statement has a finally block, then the finally block can complete normally." */
    case AST_TRY: {
        if (s->try_.finally_ && !ccn(ctx, s->try_.finally_, nrset)) return false;
        if (ccn(ctx, s->try_.body, nrset)) return true;
        for (int i = 0; i < s->try_.catches_count; i++)
            if (ccn(ctx, s->try_.catches[i]->body, nrset)) return true;
        return false;
    }

    /* "A switch statement can complete normally iff at least one of the following is true:
     *   ◆ The last statement in the switch block can complete normally.
     *   ◆ The switch block is empty or contains only switch labels.
     *   ◆ There is at least one switch label after the last switch block statement group.
     *   ◆ There is a reachable break statement that exits the switch statement."
     *
     * Those four are §14.19 verbatim, and they are INCOMPLETE: they contradict §14.9, the switch
     * statement's own execution semantics, which state "If no case matches and there is no default
     * label, then no further action is taken and the switch statement completes normally." A
     * `switch (x) { case 1: return 1; }` with x == 2 plainly falls out of the switch. The fifth
     * disjunct below — "the switch block does not contain a default label" — is what the second
     * edition added to repair this; it is what §14.9 already requires, not an invention. Omitting
     * it would make the statement after such a switch "unreachable" and, once codegen trusts this
     * predicate, would delete that statement's landing pad and miscompile the fall-out. */
    case AST_SWITCH: {
        int n = s->switch_.cases_count;
        bool only_labels = true, has_default = false;
        for (int i = 0; i < n; i++) {
            if (s->switch_.cases[i]->stmts_count) only_labels = false;
            if (!s->switch_.cases[i]->value) has_default = true;   /* `default:` has no case value */
        }
        if (n == 0 || only_labels) return true;                     /* empty, or only labels */
        if (!has_default) return true;                              /* §14.9: falls out when nothing matches */
        if (s->switch_.cases[n - 1]->stmts_count == 0) return true; /* a label past the last group */
        if (switch_exit_reachable(s)) return true;                  /* a break that exits it */
        const ast_switch_case_t* last = s->switch_.cases[n - 1];
        return ccn(ctx, last->stmts[last->stmts_count - 1], nrset);
    }

    /* (§14.18 synchronized has no rule here: the statement is not in this language.) */
    default:
        return true;
    }
}

/* ── JLS §14.19, the other half: which statements are REACHABLE ────
 *
 * "It is a compile-time error if a statement cannot be executed because it is unreachable."
 *
 * The reachability sentences are transcribed here, beside the completion rules they interlock with,
 * so that a statement's reachability and its ability to complete normally can never be decided by
 * two analyses that disagree. Note what is NOT a statement, and so can never be reported: a
 * ForUpdate expression (§14.12 makes it a StatementExpressionList) and a do-while's tail condition.
 * This walk simply never visits them.
 *
 * `sr` is reachability under §14.19 as written; `nr` is reachability under the noreturn-aware
 * extension (see ccn). !sr is the error; sr && !nr is the warning. */
static void emit_diag(sema_ctx_t* ctx, diag_level_t level, ast_srcloc loc, const char* msg);

static void jls_walk_reach(sema_ctx_t* ctx, const bbq_htree* nrset,
                           const ast_stmt_t* s, bool sr, bool nr);

/* A statement sequence: a block's statements, or one switch group's. */
static void jls_walk_seq(sema_ctx_t* ctx, const bbq_htree* nrset,
                         ast_stmt_t* const* stmts, int count, bool sr, bool nr) {
    for (int i = 0; i < count; i++) {
        jls_walk_reach(ctx, nrset, stmts[i], sr, nr);
        if (!sr) break;                       /* reported once; the rest of the sequence is dead too */
        nr = nr && ccn(ctx, stmts[i], nrset);
        sr = ccn(ctx, stmts[i], NULL);
    }
}

static void jls_walk_reach(sema_ctx_t* ctx, const bbq_htree* nrset,
                           const ast_stmt_t* s, bool sr, bool nr) {
    if (!s) return;
    if (!sr) { emit_diag(ctx, DIAG_ERROR, s->loc, "unreachable statement"); return; }
    if (!nr)   emit_diag(ctx, DIAG_WARNING, s->loc, "dead code after noreturn call");

    switch (s->tag) {
    /* "The first statement in a nonempty block … is reachable iff the block is reachable. Every
     * other statement S … is reachable iff the statement preceding S can complete normally." */
    case AST_BLOCK:
        jls_walk_seq(ctx, nrset, s->block.stmts, s->block.stmts_count, sr, nr);
        return;

    /* "ACTUAL: … The then-statement is reachable iff the if–then[–else] statement is reachable. The
     * else-statement is reachable iff the if–then–else statement is reachable." Deliberately NOT
     * conditioned on the test's constant value — that is what keeps `if (false) x = 3;` legal. */
    case AST_IF:
        jls_walk_reach(ctx, nrset, s->if_.then,  sr, nr);
        jls_walk_reach(ctx, nrset, s->if_.else_, sr, nr);
        return;

    /* "The contained statement is reachable iff the labeled statement is reachable." */
    case AST_LABELED:
        jls_walk_reach(ctx, nrset, s->labeled.body, sr, nr);
        return;

    /* "The contained statement is reachable iff the while statement is reachable and the condition
     * expression is not a constant expression whose value is false." */
    case AST_WHILE: {
        bool live = !jls_is_constant_false(ctx, s->while_.test);
        jls_walk_reach(ctx, nrset, s->while_.body, sr && live, nr && live);
        return;
    }

    /* "The contained statement is reachable iff the do statement is reachable." */
    case AST_DOWHILE:
        jls_walk_reach(ctx, nrset, s->do_while.body, sr, nr);
        return;

    /* "The contained statement is reachable iff the for statement is reachable and the condition
     * expression is not a constant expression whose value is false." An absent condition behaves as
     * the constant `true`. The init clause runs whenever the for statement is reached. */
    case AST_FOR: {
        jls_walk_reach(ctx, nrset, s->for_.init, sr, nr);
        bool live = s->for_.test == NULL || !jls_is_constant_false(ctx, s->for_.test);
        jls_walk_reach(ctx, nrset, s->for_.body, sr && live, nr && live);
        return;
    }

    /* "A switch block is reachable iff its switch statement is reachable. A statement in a switch
     * block is reachable iff its switch statement is reachable and at least one of: it bears a case
     * or default label; there is a statement preceding it in the switch block and that preceding
     * statement can complete normally." A group's first statement bears its label, so it restarts
     * reachability; within a group, statements chain. */
    case AST_SWITCH:
        for (int i = 0; i < s->switch_.cases_count; i++) {
            const ast_switch_case_t* c = s->switch_.cases[i];
            jls_walk_seq(ctx, nrset, c->stmts, c->stmts_count, sr, nr);
        }
        return;

    /* "The try block is reachable iff the try statement is reachable." "If a finally block is
     * present, it is reachable iff the try statement is reachable." A catch block's own reachability
     * is the §11.2-scoped rule, checked in ef_check_try; its body is walked as reachable. */
    case AST_TRY:
        jls_walk_reach(ctx, nrset, s->try_.body, sr, nr);
        for (int i = 0; i < s->try_.catches_count; i++)
            jls_walk_reach(ctx, nrset, s->try_.catches[i]->body, sr, nr);
        jls_walk_reach(ctx, nrset, s->try_.finally_, sr, nr);
        return;

    default:
        return;   /* a leaf statement: nothing contained to reach */
    }
}

/* "The block that is the body of a constructor, method, or static initializer is reachable." */
void jls_check_reachability(sema_ctx_t* ctx, const ast_stmt_t* body, const bbq_htree* nrset) {
    jls_walk_reach(ctx, nrset, body, true, true);
}

static void mark_back_edges(cfg_t* g);

void cfg_build(cfg_t* out, const sema_ctx_t* sema, bbq_arena* arena, const ast_stmt_t* body) {
    memset(out, 0, sizeof(*out));
    out->arena = arena;
    out->entry = mk_sentinel(arena, out, CFG_NODE_ENTRY);
    out->exit  = mk_sentinel(arena, out, CFG_NODE_EXIT);

    builder_t b;
    memset(&b, 0, sizeof(b));
    b.arena = arena;
    b.g = out;
    b.sema = sema;

    push_pending(&b, out->entry, CFG_EDGE_NORMAL, NULL);
    build_stmt(&b, body);

    for (int i = 0; i < bbq_vec_len(b.pending); i++) {
        pending_t p = b.pending[i];
        add_edge(p.node, out->exit, p.kind, p.guard);
    }
    bbq_vec_clear(b.pending);

    /* Rebuild predecessor lists. */
    for (int i = 0; i < bbq_vec_len(out->nodes); i++) {
        cfg_node_t* n = out->nodes[i];
        for (int j = 0; j < bbq_vec_len(n->succs); j++) {
            cfg_node_t* succ = n->succs[j].to;
            bbq_vec_push(succ->preds, n);
        }
    }

    mark_back_edges(out);

    bbq_vec_free(b.pending);
    bbq_vec_free(b.frames);
    bbq_vec_free(b.tries);
}

typedef struct { int id; int si; } dfs_frame_t;

/* DFS from entry marking edges to gray (on-path) vertices as
 * back-edges. Classic Tarjan. */
static void mark_back_edges(cfg_t* g) {
    int n = bbq_vec_len(g->nodes);
    if (n == 0) return;
    uint8_t* color = (uint8_t*)bbq_arena_alloc(g->arena, (size_t)n);
    memset(color, 0, (size_t)n);

    dfs_frame_t* stk = NULL;
    bbq_vec_reserve(stk, n);
    color[g->entry->id] = 1;
    dfs_frame_t f0 = { g->entry->id, 0 };
    bbq_vec_push(stk, f0);

    while (bbq_vec_len(stk) > 0) {
        dfs_frame_t* top = &stk[bbq_vec_len(stk) - 1];
        cfg_node_t* u = g->nodes[top->id];
        if (top->si < bbq_vec_len(u->succs)) {
            cfg_edge_t* e = &u->succs[top->si];
            top->si++;
            int vid = e->to->id;
            if (color[vid] == 1)        e->is_back = true;
            else if (color[vid] == 0) {
                color[vid] = 1;
                dfs_frame_t fv = { vid, 0 };
                bbq_vec_push(stk, fv);
            }
        } else {
            color[top->id] = 2;
            (void)bbq_vec_pop(stk);
        }
    }
    bbq_vec_free(stk);
}

void cfg_destroy(cfg_t* g) {
    if (!g->nodes) return;
    for (int i = 0; i < bbq_vec_len(g->nodes); i++) {
        cfg_node_t* n = g->nodes[i];
        bbq_vec_free(n->succs);
        bbq_vec_free(n->preds);
    }
    bbq_vec_free(g->nodes);
    g->nodes = NULL;
    g->entry = g->exit = NULL;
}

int cfg_node_count(const cfg_t* g) {
    return bbq_vec_len(g->nodes);
}

/* ── Worklist engine ───────────────────────────────────────────── */

void cfg_fixpoint(const cfg_t* g, const lattice_ops_t* ops,
                  bbq_arena* arena, const void* entry_state,
                  void** out_state) {
    int n = bbq_vec_len(g->nodes);
    void* bot = ops->bottom(arena);
    for (int i = 0; i < n; i++) out_state[i] = bot;
    out_state[g->entry->id] = (void*)entry_state;

    /* Worklist — a dense bool array plus a stack of node ids. */
    /* Seed every node onto the worklist: each node must visit its
     * outgoing edges at least once so transfer runs, even when the
     * predecessor's state is bot and wouldn't otherwise trigger an
     * update. Subsequent iterations are the usual incremental work. */
    bool* in_wl = (bool*)bbq_arena_alloc(arena, (size_t)n * sizeof(bool));
    int* wl = NULL;
    bbq_vec_reserve(wl, n);
    for (int i = 0; i < n; i++) {
        bbq_vec_push(wl, i);
        in_wl[i] = true;
    }

    while (bbq_vec_len(wl) > 0) {
        int id = bbq_vec_pop(wl);
        in_wl[id] = false;
        cfg_node_t* src = g->nodes[id];
        const void* src_state = out_state[id];

        for (int i = 0; i < bbq_vec_len(src->succs); i++) {
            const cfg_edge_t* e = &src->succs[i];
            void* arriving = ops->transfer(ops, arena, e, src_state);
            cfg_node_t* dst = e->to;
            /* Widen at back-edges when the lattice supplies a widen;
             * ordinary join otherwise. Widening trades precision for
             * termination on infinite-height lattices (e.g., intervals). */
            void* merged;
            if (e->is_back && ops->widen)
                merged = ops->widen(arena, out_state[dst->id], arriving);
            else
                merged = ops->meet(arena, out_state[dst->id], arriving);
            /* If `merged` ≤ existing, state didn't grow — skip. */
            if (ops->le(merged, out_state[dst->id])) continue;
            out_state[dst->id] = merged;
            if (!in_wl[dst->id]) {
                bbq_vec_push(wl, dst->id);
                in_wl[dst->id] = true;
            }
        }
    }
    bbq_vec_free(wl);
}

/* ── Nullability lattice ───────────────────────────────────────── */

typedef struct {
    uint32_t key;
    uint8_t  val;  /* null_val_t */
} null_entry_t;

typedef struct {
    const null_entry_t* entries;  /* arena-owned, sorted ascending by key */
    int count;
} null_state_t;

static uint32_t fnv1a(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

static null_val_t null_join(null_val_t a, null_val_t b) {
    if (a == NULL_BOT) return b;
    if (b == NULL_BOT) return a;
    if (a == b) return a;
    return NULL_MAYBE;
}

static bool null_val_le(null_val_t a, null_val_t b) {
    if (a == NULL_BOT) return true;
    if (a == b) return true;
    return b == NULL_MAYBE;
}

static null_val_t state_get(const null_state_t* s, uint32_t key) {
    int lo = 0, hi = s->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s->entries[mid].key < key) lo = mid + 1;
        else if (s->entries[mid].key > key) hi = mid;
        else return (null_val_t)s->entries[mid].val;
    }
    return NULL_BOT;
}

static null_state_t* state_set(bbq_arena* a, const null_state_t* src,
                                uint32_t key, null_val_t val) {
    int lo = 0, hi = src->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (src->entries[mid].key < key) lo = mid + 1;
        else hi = mid;
    }
    bool replace = (lo < src->count && src->entries[lo].key == key);
    if (replace && src->entries[lo].val == (uint8_t)val)
        return (null_state_t*)src;  /* no change */

    null_state_t* out = (null_state_t*)bbq_arena_alloc(a, sizeof(*out));
    int new_count = src->count + (replace ? 0 : 1);
    null_entry_t* ne = (null_entry_t*)bbq_arena_alloc(
        a, sizeof(*ne) * (new_count > 0 ? new_count : 1));
    for (int i = 0; i < lo; i++) ne[i] = src->entries[i];
    ne[lo].key = key;
    ne[lo].val = (uint8_t)val;
    int src_start = lo + (replace ? 1 : 0);
    int dst_pos = lo + 1;
    for (int i = src_start; i < src->count; i++) ne[dst_pos++] = src->entries[i];
    out->entries = ne;
    out->count = new_count;
    return out;
}

static void* null_bottom(bbq_arena* a) {
    null_state_t* s = (null_state_t*)bbq_arena_alloc(a, sizeof(*s));
    s->entries = NULL;
    s->count = 0;
    return s;
}

static void* null_meet(bbq_arena* a, const void* xv, const void* yv) {
    const null_state_t* x = (const null_state_t*)xv;
    const null_state_t* y = (const null_state_t*)yv;
    int maxc = x->count + y->count;
    null_entry_t* buf = (null_entry_t*)bbq_arena_alloc(
        a, sizeof(*buf) * (maxc > 0 ? maxc : 1));
    int i = 0, j = 0, k = 0;
    while (i < x->count && j < y->count) {
        if (x->entries[i].key < y->entries[j].key) buf[k++] = x->entries[i++];
        else if (x->entries[i].key > y->entries[j].key) buf[k++] = y->entries[j++];
        else {
            buf[k].key = x->entries[i].key;
            buf[k].val = (uint8_t)null_join(
                (null_val_t)x->entries[i].val,
                (null_val_t)y->entries[j].val);
            k++; i++; j++;
        }
    }
    while (i < x->count) buf[k++] = x->entries[i++];
    while (j < y->count) buf[k++] = y->entries[j++];
    null_state_t* out = (null_state_t*)bbq_arena_alloc(a, sizeof(*out));
    out->entries = buf;
    out->count = k;
    return out;
}

static bool null_le(const void* xv, const void* yv) {
    const null_state_t* x = (const null_state_t*)xv;
    const null_state_t* y = (const null_state_t*)yv;
    int j = 0;
    for (int i = 0; i < x->count; i++) {
        uint32_t k = x->entries[i].key;
        while (j < y->count && y->entries[j].key < k) j++;
        null_val_t yv_ = (j < y->count && y->entries[j].key == k)
            ? (null_val_t)y->entries[j].val : NULL_BOT;
        if (!null_val_le((null_val_t)x->entries[i].val, yv_)) return false;
    }
    return true;
}

static null_val_t eval_null(const ast_expr_t* e, const null_state_t* s) {
    if (!e) return NULL_MAYBE;
    switch (e->tag) {
    case AST_NULLLIT: return NULL_NULL;
    case AST_NEW:
    case AST_NEWARRAY:
    case AST_ARRAYINIT:
        return NULL_NONNULL;
    case AST_IDENT: {
        null_val_t v = state_get(s, fnv1a(e->ident.name));
        return (v == NULL_BOT) ? NULL_MAYBE : v;
    }
    case AST_CAST:
        return eval_null(e->cast.e, s);
    default:
        return NULL_MAYBE;
    }
}

/* Apply `stmt`'s effect to an input state; for each assignment/decl
 * of a local, update the tracked nullability. */
static const null_state_t* apply_stmt(bbq_arena* a,
                                       const ast_stmt_t* stmt,
                                       const null_state_t* in) {
    if (!stmt) return in;
    const null_state_t* cur = in;
    switch (stmt->tag) {
    case AST_EXPRSTMT: {
        const ast_expr_t* e = stmt->expr_stmt.e;
        if (e && e->tag == AST_ASSIGN &&
            e->assign.target->tag == AST_IDENT) {
            null_val_t v = eval_null(e->assign.value, cur);
            cur = state_set(a, cur, fnv1a(e->assign.target->ident.name), v);
        }
        break;
    }
    case AST_LOCALVARDECL:
        for (int i = 0; i < stmt->local_var_decl.decls_count; i++) {
            ast_var_decl_t* d = stmt->local_var_decl.decls[i];
            if (d->init) {
                null_val_t v = eval_null(d->init, cur);
                cur = state_set(a, cur, fnv1a(d->name), v);
            }
        }
        break;
    default:
        break;
    }
    return cur;
}

/* If `guard` is a (x != null) / (x == null) comparison, return the
 * ident side and set *is_ne accordingly. NULL on mismatch. */
static const ast_expr_t* null_check_ident(const ast_expr_t* g, bool* is_ne) {
    if (!g || g->tag != AST_BINARY) return NULL;
    if (g->binary.op != AST_EQ && g->binary.op != AST_NE) return NULL;
    const ast_expr_t* l = g->binary.lhs;
    const ast_expr_t* r = g->binary.rhs;
    *is_ne = (g->binary.op == AST_NE);
    if (l->tag == AST_IDENT && r->tag == AST_NULLLIT) return l;
    if (r->tag == AST_IDENT && l->tag == AST_NULLLIT) return r;
    return NULL;
}

static void* null_transfer(const lattice_ops_t* self, bbq_arena* a,
                           const cfg_edge_t* e, const void* inv) {
    (void)self;
    const null_state_t* in = (const null_state_t*)inv;
    const null_state_t* cur = apply_stmt(a, e->from->stmt, in);

    if (e->kind == CFG_EDGE_TRUE || e->kind == CFG_EDGE_FALSE) {
        bool is_ne = false;
        const ast_expr_t* id = null_check_ident(e->guard, &is_ne);
        if (id) {
            bool true_branch = (e->kind == CFG_EDGE_TRUE);
            null_val_t narrow = (is_ne == true_branch)
                                ? NULL_NONNULL : NULL_NULL;
            cur = state_set(a, cur, fnv1a(id->ident.name), narrow);
        }
    }
    return (void*)cur;
}

const lattice_ops_t nullability_ops = {
    null_bottom, null_meet, NULL, null_le, null_transfer,
};

void* nullability_entry_state(bbq_arena* a,
                              const char* const* names, int count) {
    null_state_t* s = (null_state_t*)null_bottom(a);
    for (int i = 0; i < count; i++) {
        s = state_set(a, s, fnv1a(names[i]), NULL_MAYBE);
    }
    return s;
}

null_val_t nullability_lookup(const void* state, const char* name) {
    const null_state_t* s = (const null_state_t*)state;
    return state_get(s, fnv1a(name));
}

/* ── Interval lattice ──────────────────────────────────────────── */

typedef struct {
    uint32_t key;
    interval_val_t val;
} interval_entry_t;

typedef struct {
    const interval_entry_t* entries;
    int count;
} interval_state_t;

/* --- scalar saturating arithmetic (handles ±∞ sentinels) --- */

static int64_t i64_min(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t i64_max(int64_t a, int64_t b) { return a > b ? a : b; }

static int64_t iv_neg_sat(int64_t a) {
    if (a == INTERVAL_NEG_INF) return INTERVAL_POS_INF;
    if (a == INTERVAL_POS_INF) return INTERVAL_NEG_INF;
    return -a;
}

static int64_t iv_add_sat(int64_t a, int64_t b) {
    if (a == INTERVAL_NEG_INF || b == INTERVAL_NEG_INF) {
        if (a == INTERVAL_POS_INF || b == INTERVAL_POS_INF)
            return 0;  /* −∞ + +∞ is undefined; choose 0 conservatively */
        return INTERVAL_NEG_INF;
    }
    if (a == INTERVAL_POS_INF || b == INTERVAL_POS_INF) return INTERVAL_POS_INF;
    int64_t r;
    if (__builtin_add_overflow(a, b, &r))
        return (a > 0) ? INTERVAL_POS_INF : INTERVAL_NEG_INF;
    if (r >= INTERVAL_POS_INF - 1) return INTERVAL_POS_INF;
    if (r <= INTERVAL_NEG_INF + 1) return INTERVAL_NEG_INF;
    return r;
}

static int64_t iv_sub_sat(int64_t a, int64_t b) {
    return iv_add_sat(a, iv_neg_sat(b));
}

static int64_t iv_mul_sat(int64_t a, int64_t b) {
    bool a_inf = (a == INTERVAL_NEG_INF || a == INTERVAL_POS_INF);
    bool b_inf = (b == INTERVAL_NEG_INF || b == INTERVAL_POS_INF);
    if ((a_inf && b == 0) || (b_inf && a == 0)) return 0;
    if (a_inf || b_inf) {
        bool a_pos = a_inf ? (a == INTERVAL_POS_INF) : (a > 0);
        bool b_pos = b_inf ? (b == INTERVAL_POS_INF) : (b > 0);
        return (a_pos == b_pos) ? INTERVAL_POS_INF : INTERVAL_NEG_INF;
    }
    int64_t r;
    if (__builtin_mul_overflow(a, b, &r))
        return ((a < 0) == (b < 0)) ? INTERVAL_POS_INF : INTERVAL_NEG_INF;
    if (r >= INTERVAL_POS_INF - 1) return INTERVAL_POS_INF;
    if (r <= INTERVAL_NEG_INF + 1) return INTERVAL_NEG_INF;
    return r;
}

/* --- per-value lattice ops --- */

/* The lattice ops. The interval algebra is jbound.h's, shared with the
 * optimizer's lattice; what stays here is this side's ⊥ handling — the linter's
 * "no fact" is BOT, and BOT is the join identity but the meet's annihilator. */

/* Join (LUB): enclose both. */
static interval_val_t iv_join(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a)) return b;
    if (interval_is_bot(b)) return a;
    jbound_t j = jbound_hull(a.lo, a.hi, b.lo, b.hi);
    interval_val_t r = { j.lo, j.hi };
    return r;
}

/* Meet (GLB): intersect — used for narrowing, not CFG meet. */
static interval_val_t iv_intersect(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    jbound_t m = jbound_meet(a.lo, a.hi, b.lo, b.hi);
    if (!m.ok) return interval_bot();                 /* empty intersection */
    interval_val_t r = { m.lo, m.hi };
    return r;
}

/* x ≤ y iff x is contained in y (interval inclusion). */
static bool iv_le_val(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a)) return true;
    if (interval_is_bot(b)) return false;
    return jbound_contains(a.lo, a.hi, b.lo, b.hi);
}

/* Widening: preserve bounds that held; send growing ones to ±∞. The DECISION is
 * the shared one; ±∞ is this side's replacement policy (the optimizer snaps to
 * its per-method K set instead — same rule, a longer chain). */
static interval_val_t iv_widen(interval_val_t prev, interval_val_t in) {
    if (interval_is_bot(prev)) return in;
    if (interval_is_bot(in)) return prev;
    interval_val_t r;
    r.lo = jbound_widen_lo_grew(prev.lo, in.lo) ? INTERVAL_NEG_INF : prev.lo;
    r.hi = jbound_widen_hi_grew(prev.hi, in.hi) ? INTERVAL_POS_INF : prev.hi;
    return r;
}

/* --- interval arithmetic --- */

/* WHICH CORNERS bound the result is jbound.h's — the same algebra the optimizer's
 * range folds run. What differs, and stays here, is the response when a corner
 * leaves the rails: this lattice SATURATES to ±∞ (a diagnostic interval is
 * unbounded, and the int32 wrap rule is applied separately by iv_int_wrap_guard),
 * where the optimizer forfeits to its no-fact element. Same rule, two policies —
 * which is why the core reports the overflow instead of deciding it. */
static interval_val_t iv_arith_add(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    jbound_t j = jbound_add(a.lo, a.hi, b.lo, b.hi, INTERVAL_NEG_INF, INTERVAL_POS_INF);
    if (!j.ok || j.overflow) {
        interval_val_t s = { iv_add_sat(a.lo, b.lo), iv_add_sat(a.hi, b.hi) };
        return s;
    }
    interval_val_t r = { j.lo, j.hi };
    return r;
}
static interval_val_t iv_arith_sub(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    jbound_t j = jbound_sub(a.lo, a.hi, b.lo, b.hi, INTERVAL_NEG_INF, INTERVAL_POS_INF);
    if (!j.ok || j.overflow) {
        interval_val_t s = { iv_sub_sat(a.lo, b.hi), iv_sub_sat(a.hi, b.lo) };
        return s;
    }
    interval_val_t r = { j.lo, j.hi };
    return r;
}
static interval_val_t iv_arith_mul(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    jbound_t j = jbound_mul(a.lo, a.hi, b.lo, b.hi, INTERVAL_NEG_INF, INTERVAL_POS_INF);
    if (!j.ok || j.overflow) {
        int64_t p1 = iv_mul_sat(a.lo, b.lo);
        int64_t p2 = iv_mul_sat(a.lo, b.hi);
        int64_t p3 = iv_mul_sat(a.hi, b.lo);
        int64_t p4 = iv_mul_sat(a.hi, b.hi);
        interval_val_t s = { i64_min(i64_min(p1, p2), i64_min(p3, p4)),
                             i64_max(i64_max(p1, p2), i64_max(p3, p4)) };
        return s;
    }
    interval_val_t r = { j.lo, j.hi };
    return r;
}
static interval_val_t iv_arith_neg(interval_val_t a) {
    if (interval_is_bot(a)) return interval_bot();
    jbound_t j = jbound_neg(a.lo, a.hi, INTERVAL_NEG_INF, INTERVAL_POS_INF);
    if (!j.ok || j.overflow) {
        interval_val_t s = { iv_neg_sat(a.hi), iv_neg_sat(a.lo) };
        return s;
    }
    interval_val_t r = { j.lo, j.hi };
    return r;
}

static int64_t iv_div_sat(int64_t a, int64_t b) {
    /* Caller guarantees b != 0. */
    bool a_inf = (a == INTERVAL_NEG_INF || a == INTERVAL_POS_INF);
    bool b_inf = (b == INTERVAL_NEG_INF || b == INTERVAL_POS_INF);
    if (a_inf && b_inf) return 0;  /* indeterminate — conservative 0 */
    if (a_inf) {
        bool a_pos = (a == INTERVAL_POS_INF);
        bool b_pos = (b > 0);
        return (a_pos == b_pos) ? INTERVAL_POS_INF : INTERVAL_NEG_INF;
    }
    if (b_inf) return 0;  /* finite / ±∞ rounds to zero */
    if (a == INT64_MIN && b == -1) return INTERVAL_POS_INF;  /* overflow */
    return a / b;
}

/* An unbounded end is a SENTINEL, not a number: dividing it would name a finite
 * bound the value set does not have. So the shared corner rule applies exactly
 * when every end is finite — which is the case the optimizer's width-bounded
 * lattice is always in, and is why only this side carries the ±∞ arms below. */
static bool iv_finite(interval_val_t v) {
    return v.lo != INTERVAL_NEG_INF && v.lo != INTERVAL_POS_INF
        && v.hi != INTERVAL_NEG_INF && v.hi != INTERVAL_POS_INF;
}

static interval_val_t iv_arith_div(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    /* Zero-in-divisor — runtime throws ArithmeticException, so the
     * fallthrough post-stmt is effectively unreachable. Conservative
     * TOP here; upstream users treat TOP as "don't emit diags". */
    if (b.lo <= 0 && b.hi >= 0) return interval_top();
    if (iv_finite(a) && iv_finite(b)) {
        jbound_t j = jbound_div(a.lo, a.hi, b.lo, b.hi, INT64_MIN);
        if (!j.ok) return interval_top();
        interval_val_t r = { j.lo, j.hi };
        return r;
    }
    int64_t p1 = iv_div_sat(a.lo, b.lo);
    int64_t p2 = iv_div_sat(a.lo, b.hi);
    int64_t p3 = iv_div_sat(a.hi, b.lo);
    int64_t p4 = iv_div_sat(a.hi, b.hi);
    interval_val_t r = { i64_min(i64_min(p1, p2), i64_min(p3, p4)),
                         i64_max(i64_max(p1, p2), i64_max(p3, p4)) };
    return r;
}

static interval_val_t iv_arith_rem(interval_val_t a, interval_val_t b) {
    if (interval_is_bot(a) || interval_is_bot(b)) return interval_bot();
    if (b.lo <= 0 && b.hi >= 0) return interval_top();
    if (iv_finite(a) && iv_finite(b)) {
        jbound_t j = jbound_rem(a.lo, a.hi, b.lo, b.hi, INT64_MIN);
        if (!j.ok) return interval_top();
        interval_val_t r = { j.lo, j.hi };
        return r;
    }
    /* max |b| — guard against INT64_MIN abs overflow. */
    int64_t blo_abs = (b.lo == INTERVAL_NEG_INF) ? INTERVAL_POS_INF
                      : (b.lo < 0 ? -b.lo : b.lo);
    int64_t bhi_abs = (b.hi == INTERVAL_NEG_INF) ? INTERVAL_POS_INF
                      : (b.hi < 0 ? -b.hi : b.hi);
    int64_t bmax = i64_max(blo_abs, bhi_abs);
    int64_t bound = (bmax == INTERVAL_POS_INF) ? INTERVAL_POS_INF : (bmax - 1);
    interval_val_t r;
    if (a.lo >= 0) {
        r.lo = 0; r.hi = bound;
    } else if (a.hi <= 0) {
        r.lo = (bound == INTERVAL_POS_INF) ? INTERVAL_NEG_INF : -bound;
        r.hi = 0;
    } else {
        r.lo = (bound == INTERVAL_POS_INF) ? INTERVAL_NEG_INF : -bound;
        r.hi = bound;
    }
    return r;
}

/* --- per-ident state map --- */

static interval_val_t iv_state_get(const interval_state_t* s, uint32_t key) {
    int lo = 0, hi = s->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s->entries[mid].key < key) lo = mid + 1;
        else if (s->entries[mid].key > key) hi = mid;
        else return s->entries[mid].val;
    }
    return interval_bot();
}

static interval_state_t* iv_state_set(bbq_arena* a, const interval_state_t* src,
                                      uint32_t key, interval_val_t val) {
    int lo = 0, hi = src->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (src->entries[mid].key < key) lo = mid + 1;
        else hi = mid;
    }
    bool replace = (lo < src->count && src->entries[lo].key == key);
    if (replace && src->entries[lo].val.lo == val.lo
                && src->entries[lo].val.hi == val.hi)
        return (interval_state_t*)src;

    interval_state_t* out = (interval_state_t*)bbq_arena_alloc(a, sizeof(*out));
    int new_count = src->count + (replace ? 0 : 1);
    interval_entry_t* ne = (interval_entry_t*)bbq_arena_alloc(
        a, sizeof(*ne) * (new_count > 0 ? new_count : 1));
    for (int i = 0; i < lo; i++) ne[i] = src->entries[i];
    ne[lo].key = key;
    ne[lo].val = val;
    int src_start = lo + (replace ? 1 : 0);
    int dst_pos = lo + 1;
    for (int i = src_start; i < src->count; i++) ne[dst_pos++] = src->entries[i];
    out->entries = ne;
    out->count = new_count;
    return out;
}

/* --- state-level lattice ops (used by the engine) --- */

static void* iv_bottom(bbq_arena* a) {
    interval_state_t* s = (interval_state_t*)bbq_arena_alloc(a, sizeof(*s));
    s->entries = NULL;
    s->count = 0;
    return s;
}

typedef interval_val_t (*iv_merge_fn)(interval_val_t, interval_val_t);

static void* iv_merge_states(bbq_arena* a, const void* xv, const void* yv,
                             iv_merge_fn f) {
    const interval_state_t* x = (const interval_state_t*)xv;
    const interval_state_t* y = (const interval_state_t*)yv;
    int maxc = x->count + y->count;
    interval_entry_t* buf = (interval_entry_t*)bbq_arena_alloc(
        a, sizeof(*buf) * (maxc > 0 ? maxc : 1));
    int i = 0, j = 0, k = 0;
    while (i < x->count && j < y->count) {
        if (x->entries[i].key < y->entries[j].key) buf[k++] = x->entries[i++];
        else if (x->entries[i].key > y->entries[j].key) buf[k++] = y->entries[j++];
        else {
            buf[k].key = x->entries[i].key;
            buf[k].val = f(x->entries[i].val, y->entries[j].val);
            k++; i++; j++;
        }
    }
    while (i < x->count) buf[k++] = x->entries[i++];
    while (j < y->count) buf[k++] = y->entries[j++];
    interval_state_t* out = (interval_state_t*)bbq_arena_alloc(a, sizeof(*out));
    out->entries = buf;
    out->count = k;
    return out;
}

static void* iv_meet(bbq_arena* a, const void* xv, const void* yv) {
    return iv_merge_states(a, xv, yv, iv_join);
}
static void* iv_widen_state(bbq_arena* a, const void* pv, const void* iv) {
    return iv_merge_states(a, pv, iv, iv_widen);
}

static bool iv_le_state(const void* xv, const void* yv) {
    const interval_state_t* x = (const interval_state_t*)xv;
    const interval_state_t* y = (const interval_state_t*)yv;
    int j = 0;
    for (int i = 0; i < x->count; i++) {
        uint32_t k = x->entries[i].key;
        while (j < y->count && y->entries[j].key < k) j++;
        interval_val_t yv_ = (j < y->count && y->entries[j].key == k)
                           ? y->entries[j].val : interval_bot();
        if (!iv_le_val(x->entries[i].val, yv_)) return false;
    }
    return true;
}

/* --- expression evaluation --- */

/* Java int arithmetic WRAPS at 32 bits; the lattice's saturating i64 does
 * not. A result crossing the int32 rails may have wrapped to the other
 * sign at runtime, so passing the un-wrapped interval on is a false SAFE
 * claim — forfeit to TOP instead. (The shrinking ops — div/rem/mask —
 * cannot newly exceed their inputs and skip this. The Click twin is the
 * γ folds' overflow-forfeits discipline.) */
static interval_val_t iv_int_wrap_guard(interval_val_t v) {
    if (interval_is_bot(v)) return v;
    if (v.lo < INT32_MIN || v.hi > INT32_MAX) return interval_top();
    return v;
}

static interval_val_t iv_eval(const ast_expr_t* e, const interval_state_t* s) {
    if (!e) return interval_top();
    switch (e->tag) {
    case AST_INTLIT: return interval_const(e->int_lit.value);
    case AST_IDENT:
        /* Return BOT directly — if the ident is unassigned on this
         * path, propagating BOT prevents spurious widening on early
         * back-edge visits before the source has real state. Callers
         * that need "unknown" explicitly should seed TOP at entry. */
        return iv_state_get(s, fnv1a(e->ident.name));
    case AST_NEWARRAY:
        /* Array length comes from the outer (first) dimension expression. */
        return iv_eval(e->new_array.dims[0], s);
    case AST_FIELDACCESS:
        /* `arr.length` — spec T3: a length is ALWAYS within [0, INT32_MAX],
         * tracked or not (an untracked array is a param, not an unassigned
         * local — §16 definite assignment already rejects those). The
         * tracked interval, when present, tightens both bounds. */
        if (e->field_access.field &&
            strcmp(e->field_access.field, "length") == 0 &&
            e->field_access.obj &&
            e->field_access.obj->tag == AST_IDENT) {
            interval_val_t v = iv_state_get(
                s, fnv1a(e->field_access.obj->ident.name));
            interval_val_t r;
            r.lo = interval_is_bot(v) ? 0 : i64_max(0, v.lo);
            r.hi = interval_is_bot(v) ? INT32_MAX : i64_min(INT32_MAX, v.hi);
            if (r.lo > r.hi) return interval_top();  /* inconsistent: no claim */
            return r;
        }
        return interval_top();
    case AST_CAST: {
        /* Narrowing casts TRUNCATE (JLS §5.1.3): the interval passes through
         * only when it FITS the target; otherwise the exact claim is the
         * target's own total range. Anything else passes through. (The
         * Click twins are the i2s/i2b/s2b/i2c range folds.) */
        interval_val_t v = iv_eval(e->cast.e, s);
        const ast_type_t* ty = e->cast.ty;
        int64_t tlo, thi;
        if      (ty && ty->tag == AST_BYTETYPE)  { tlo = -128;      thi = 127; }
        else if (ty && ty->tag == AST_SHORTTYPE) { tlo = -32768;    thi = 32767; }
        else if (ty && ty->tag == AST_CHARTYPE)  { tlo = 0;         thi = 65535; }
        else if (ty && ty->tag == AST_INTTYPE)   { tlo = INT32_MIN; thi = INT32_MAX; }
        else return v;
        if (interval_is_bot(v)) return v;
        if (v.lo >= tlo && v.hi <= thi) return v;    /* fits — value-preserving */
        interval_val_t r = { tlo, thi };
        return r;
    }
    case AST_BINARY: {
        interval_val_t lhs = iv_eval(e->binary.lhs, s);
        interval_val_t rhs = iv_eval(e->binary.rhs, s);
        switch (e->binary.op) {
        case AST_ADD: return iv_int_wrap_guard(iv_arith_add(lhs, rhs));
        case AST_SUB: return iv_int_wrap_guard(iv_arith_sub(lhs, rhs));
        case AST_MUL: return iv_int_wrap_guard(iv_arith_mul(lhs, rhs));
        case AST_DIV: return iv_arith_div(lhs, rhs);
        case AST_REM: return iv_arith_rem(lhs, rhs);
        case AST_SHL: {
            /* x << k for a KNOWN in-range count is ×2^k (spec T2); Java
             * wraps, so the guard forfeits overflowing shifts. A
             * non-constant count claims nothing. */
            if (interval_is_bot(lhs) || interval_is_bot(rhs))
                return interval_bot();
            if (rhs.lo != rhs.hi || rhs.lo < 0 || rhs.lo > 31)
                return interval_top();
            return iv_int_wrap_guard(
                iv_arith_mul(lhs, interval_const((int64_t)1 << rhs.lo)));
        }
        case AST_BITAND: {
            /* x & m with a wholly non-negative m is within [0, hi(m)] for ANY
             * x — two's complement: a non-negative mask clears the sign bit —
             * so the classic `i & 7` index bounds itself with i unknown. */
            if (interval_is_bot(lhs) || interval_is_bot(rhs))
                return interval_bot();
            int64_t hi = INTERVAL_POS_INF;
            bool nn = false;
            if (lhs.lo >= 0) { nn = true; hi = lhs.hi; }
            if (rhs.lo >= 0) { nn = true; hi = i64_min(hi, rhs.hi); }
            if (!nn) return interval_top();
            interval_val_t r = { 0, hi };
            return r;
        }
        default:      return interval_top();
        }
    }
    case AST_UNARY:
        if (e->unary.op == AST_NEG)
            return iv_int_wrap_guard(iv_arith_neg(iv_eval(e->unary.e, s)));
        return interval_top();
    default:
        return interval_top();
    }
}

/* --- narrowing from comparison guards --- */

/* Given `l OP r` on the `true_branch` side, compute the interval
 * implied for l (rhs-value is `rhs_v`). Conservative TOP for ops we
 * don't handle. */
/* The linter's half of the branch-narrow rule. The normalisation and the bound
 * reading are jbound.h's, shared with the optimizer's twin table; what stays here
 * is this lattice's policy — "proves nothing" is TOP, and the rails are ±∞
 * rather than a type width, because a diagnostic interval is not width-bound
 * until iv_int_wrap_guard says so. */
static interval_val_t iv_range_for_lhs(ast_binop_t op, bool true_branch,
                                       interval_val_t rhs_v) {
    jbound_cmp_t c;
    switch (op) {
    case AST_EQ: c = JB_EQ; break;
    case AST_NE: c = JB_NE; break;
    case AST_LT: c = JB_LT; break;
    case AST_LE: c = JB_LE; break;
    case AST_GT: c = JB_GT; break;
    case AST_GE: c = JB_GE; break;
    default: return interval_top();
    }
    if (!true_branch) c = jbound_cmp_negate(c);
    jbound_t r = jbound_narrow_by_cmp(c, rhs_v.lo, rhs_v.hi,
                                      INTERVAL_NEG_INF, INTERVAL_POS_INF);
    if (!r.ok) return interval_top();          /* no interval proved */
    interval_val_t out = { r.lo, r.hi };
    return out;
}

static ast_binop_t iv_swap_op(ast_binop_t op) {
    switch (op) {
    case AST_LT: return AST_GT;
    case AST_LE: return AST_GE;
    case AST_GT: return AST_LT;
    case AST_GE: return AST_LE;
    default: return op;
    }
}

/* Narrow `ident_name`'s entry by intersecting with `range`. If the
 * ident has no prior state (BOT), leave it alone — synthesizing TOP
 * from BOT during narrowing produces spurious intervals before the
 * source of the variable has been visited. Parameters that enter the
 * method seed TOP explicitly, so real narrowing still works on them. */
static const interval_state_t* iv_narrow(bbq_arena* a,
                                          const interval_state_t* s,
                                          const char* name,
                                          interval_val_t range) {
    uint32_t k = fnv1a(name);
    interval_val_t cur = iv_state_get(s, k);
    if (interval_is_bot(cur)) return s;
    interval_val_t refined = iv_intersect(cur, range);
    return iv_state_set(a, s, k, refined);
}

static const interval_state_t* iv_narrow_side(bbq_arena* a,
                                               const interval_state_t* s,
                                               const ast_expr_t* side,
                                               interval_val_t range) {
    if (side->tag == AST_IDENT) {
        return iv_narrow(a, s, side->ident.name, range);
    }
    if (side->tag == AST_FIELDACCESS &&
        side->field_access.field &&
        strcmp(side->field_access.field, "length") == 0 &&
        side->field_access.obj &&
        side->field_access.obj->tag == AST_IDENT) {
        /* Narrowing `arr.length` narrows arr's tracked length interval. */
        return iv_narrow(a, s, side->field_access.obj->ident.name, range);
    }
    return s;
}

static const interval_state_t* iv_apply_narrow(bbq_arena* a,
                                                const interval_state_t* s,
                                                const ast_expr_t* g,
                                                bool true_branch) {
    if (!g || g->tag != AST_BINARY) return s;
    ast_binop_t op = g->binary.op;
    if (op != AST_EQ && op != AST_NE && op != AST_LT && op != AST_LE
        && op != AST_GT && op != AST_GE) return s;
    const ast_expr_t* lhs = g->binary.lhs;
    const ast_expr_t* rhs = g->binary.rhs;

    /* Narrow LHS given RHS. */
    {
        interval_val_t rv = iv_eval(rhs, s);
        interval_val_t range = iv_range_for_lhs(op, true_branch, rv);
        s = iv_narrow_side(a, s, lhs, range);
    }
    /* Narrow RHS given LHS via the swapped comparison. */
    {
        interval_val_t lv = iv_eval(lhs, s);
        interval_val_t range = iv_range_for_lhs(iv_swap_op(op), true_branch, lv);
        s = iv_narrow_side(a, s, rhs, range);
    }
    return s;
}

static const interval_state_t* iv_apply_stmt(bbq_arena* a,
                                              const ast_stmt_t* s,
                                              const interval_state_t* in) {
    if (!s) return in;
    const interval_state_t* cur = in;
    switch (s->tag) {
    case AST_EXPRSTMT: {
        const ast_expr_t* e = s->expr_stmt.e;
        if (e && e->tag == AST_ASSIGN &&
            e->assign.target->tag == AST_IDENT) {
            interval_val_t v = iv_eval(e->assign.value, cur);
            cur = iv_state_set(a, cur, fnv1a(e->assign.target->ident.name), v);
        }
        break;
    }
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++) {
            ast_var_decl_t* d = s->local_var_decl.decls[i];
            if (d->init) {
                interval_val_t v = iv_eval(d->init, cur);
                cur = iv_state_set(a, cur, fnv1a(d->name), v);
            }
        }
        break;
    default: break;
    }
    return cur;
}

static void* iv_transfer(const lattice_ops_t* self, bbq_arena* a,
                         const cfg_edge_t* e, const void* inv) {
    (void)self;
    const interval_state_t* in = (const interval_state_t*)inv;
    const interval_state_t* cur = iv_apply_stmt(a, e->from->stmt, in);
    if (e->kind == CFG_EDGE_TRUE || e->kind == CFG_EDGE_FALSE)
        cur = iv_apply_narrow(a, cur, e->guard, e->kind == CFG_EDGE_TRUE);
    return (void*)cur;
}

const lattice_ops_t interval_ops = {
    iv_bottom, iv_meet, iv_widen_state, iv_le_state, iv_transfer,
};

void* interval_entry_state(bbq_arena* a,
                           const char* const* names, int count) {
    interval_state_t* s = (interval_state_t*)iv_bottom(a);
    for (int i = 0; i < count; i++)
        s = iv_state_set(a, s, fnv1a(names[i]), interval_top());
    return s;
}

interval_val_t interval_lookup(const void* state, const char* name) {
    const interval_state_t* s = (const interval_state_t*)state;
    return iv_state_get(s, fnv1a(name));
}

/* ── Definite-assignment lattice ───────────────────────────────── */

/* 256-bit bitmap — enough for the definite-assignment slot bitmap
 * (255 slots). State value
 * at program point = set of slots definitely assigned on every path
 * reaching here. Meet is intersection (assigned on ALL paths).
 *
 * Note on lattice direction: unlike nullability/interval which grow
 * from bottom upward, DA shrinks from the optimistic ⊤ (all-bits-
 * set) downward as pessimism (intersection at joins) accumulates.
 * The engine's `le(merged, old)` skip rule expects us to report
 * "no more informative than old" — for DA that means the new state
 * is a superset of the old bits (no pessimism added). */

typedef struct {
    uint64_t bits[];   /* da_nwords words — sized per method (§16 must
                          hold to the 65535-local limit, not fail open
                          past a fixed cap) */
} da_state_t;

/* Words in every da_state_t of the CURRENT method's fixpoint — set by
 * the per-method driver before the DA run (analyses run one method at
 * a time). */
static int da_nwords = 4;

static size_t da_size(void) { return sizeof(uint64_t) * (size_t)da_nwords; }

static void* da_bottom(bbq_arena* a) {
    /* ⊤-as-bottom: start every non-entry node at all-bits-set, the
     * identity for intersection. */
    da_state_t* s = (da_state_t*)bbq_arena_alloc(a, da_size());
    memset(s->bits, 0xFF, da_size());
    return s;
}

static void* da_meet(bbq_arena* a, const void* xv, const void* yv) {
    const da_state_t* x = (const da_state_t*)xv;
    const da_state_t* y = (const da_state_t*)yv;
    da_state_t* out = (da_state_t*)bbq_arena_alloc(a, da_size());
    for (int i = 0; i < da_nwords; i++) out->bits[i] = x->bits[i] & y->bits[i];
    return out;
}

/* x ≤ y iff x's set is a superset of y's (x adds no pessimism). */
static bool da_le(const void* xv, const void* yv) {
    const da_state_t* x = (const da_state_t*)xv;
    const da_state_t* y = (const da_state_t*)yv;
    for (int i = 0; i < da_nwords; i++)
        if ((y->bits[i] & ~x->bits[i]) != 0) return false;
    return true;
}

static bool da_bit_get(const da_state_t* s, int slot) {
    if (slot < 0 || slot >= da_nwords * 64) return true;  /* out of range = assume ok */
    return (s->bits[slot / 64] >> (slot % 64)) & 1;
}

static da_state_t* da_bit_set(bbq_arena* a, const da_state_t* src, int slot) {
    if (slot < 0 || slot >= da_nwords * 64) return (da_state_t*)src;
    if (da_bit_get(src, slot)) return (da_state_t*)src;
    da_state_t* out = (da_state_t*)bbq_arena_alloc(a, da_size());
    memcpy(out, src, da_size());
    out->bits[slot / 64] |= (uint64_t)1 << (slot % 64);
    return out;
}

/* If `target` is a local/param ident, set its slot in the outgoing
 * state. Other targets (field, array) don't affect DA of locals. */
static const da_state_t* da_mark_target(sema_ctx_t* ctx, bbq_arena* a,
                                         const da_state_t* s,
                                         const ast_expr_t* target) {
    if (!target || target->tag != AST_IDENT) return s;
    const sema_ident_info_t* info = sema_ident_kind(ctx, target);
    if (!info) return s;
    if (info->kind != SEMA_IDENT_LOCAL && info->kind != SEMA_IDENT_PARAM)
        return s;
    return da_bit_set(a, s, info->slot);
}

/* Mark every assignment / compound / ++/-- TARGET that DEFINITELY executes in `e`.
 * Recurses into a nested assignment's VALUE (which always evaluates) so a CHAINED
 * assignment `z = w = expr` marks BOTH w and z (JLS §16.1). Deliberately does NOT
 * descend into call args / ternary arms — those don't definitely assign, so marking
 * them would be unsound. */
static const da_state_t* da_mark_expr_assigns(sema_ctx_t* ctx, bbq_arena* a,
                                              const da_state_t* s, const ast_expr_t* e) {
    if (!e) return s;
    if (e->tag == AST_ASSIGN) {
        s = da_mark_expr_assigns(ctx, a, s, e->assign.value);
        s = da_mark_target(ctx, a, s, e->assign.target);
    } else if (e->tag == AST_COMPOUNDASSIGN) {
        s = da_mark_expr_assigns(ctx, a, s, e->compound_assign.value);
        s = da_mark_target(ctx, a, s, e->compound_assign.target);
    } else if (e->tag == AST_UNARY) {
        if (e->unary.op == AST_PREINC || e->unary.op == AST_PREDEC ||
            e->unary.op == AST_POSTINC || e->unary.op == AST_POSTDEC)
            s = da_mark_target(ctx, a, s, e->unary.e);
        else
            s = da_mark_expr_assigns(ctx, a, s, e->unary.e);
    } else if (e->tag == AST_BINARY) {
        /* Both operands of a non-short-circuiting binary op definitely execute; for
         * && and || only the LEFT does (JLS §16.1). So `(x = f()) > 0` marks x. */
        s = da_mark_expr_assigns(ctx, a, s, e->binary.lhs);
        if (e->binary.op != AST_AND && e->binary.op != AST_OR)
            s = da_mark_expr_assigns(ctx, a, s, e->binary.rhs);
    } else if (e->tag == AST_TERNARY) {
        /* Only the condition definitely executes; the two arms are conditional. */
        s = da_mark_expr_assigns(ctx, a, s, e->ternary.test);
    }
    return s;
}

/* DA lattice carries the owning sema_ctx_t so transfer can resolve
 * ident targets via `sema_ident_kind`. */
typedef struct {
    lattice_ops_t ops;    /* first field — engine casts self */
    sema_ctx_t* ctx;
} da_lattice_t;

static void* da_transfer(const lattice_ops_t* self_ops, bbq_arena* a,
                         const cfg_edge_t* e, const void* inv) {
    const da_lattice_t* self = (const da_lattice_t*)self_ops;
    sema_ctx_t* ctx = self->ctx;
    const da_state_t* cur = (const da_state_t*)inv;
    const ast_stmt_t* s = e->from->stmt;
    if (s) switch (s->tag) {
    case AST_EXPRSTMT:
        cur = da_mark_expr_assigns(ctx, a, cur, s->expr_stmt.e);
        break;
    /* A branch condition definitely executes before either successor edge, so an
     * assignment embedded in it (`if ((c = cmp()) > 0) … else if (c < 0) …`) is
     * definitely assigned in both arms. The if/while/do header owns only its post-test
     * out-edges; `for` is excluded (its init/update share the AST_FOR node). */
    case AST_IF:
        cur = da_mark_expr_assigns(ctx, a, cur, s->if_.test);
        break;
    case AST_WHILE:
        cur = da_mark_expr_assigns(ctx, a, cur, s->while_.test);
        break;
    case AST_DOWHILE:
        cur = da_mark_expr_assigns(ctx, a, cur, s->do_while.test);
        break;
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++) {
            ast_var_decl_t* d = s->local_var_decl.decls[i];
            if (d && d->init) {
                const sema_ident_info_t info_fake = {0};
                (void)info_fake;
                /* Resolve the declarator's slot via the same lookup
                 * sema uses for locals. */
                int32_t slot = sema_slot(ctx, d);
                if (slot >= 0) cur = da_bit_set(a, cur, slot);
            }
        }
        break;
    default:
        break;
    }
    /* Entering a catch handler definitely binds its catch variable. */
    if (e->to->catch_clause) {
        int32_t slot = sema_slot(ctx, e->to->catch_clause);
        if (slot >= 0) cur = da_bit_set(a, cur, slot);
    }
    return (void*)cur;
}

static void da_lattice_init(da_lattice_t* l, sema_ctx_t* ctx) {
    l->ops.bottom   = da_bottom;
    l->ops.meet     = da_meet;
    l->ops.widen    = NULL;
    l->ops.le       = da_le;
    l->ops.transfer = da_transfer;
    l->ctx          = ctx;
}

static void* da_entry_state(bbq_arena* a, const sema_method_t* m) {
    da_state_t* s = (da_state_t*)bbq_arena_alloc(a, da_size());
    memset(s->bits, 0, da_size());
    /* `this` (slot 0) for instance methods + constructors. */
    if (!(m->modifiers & ACC_STATIC))
        s->bits[0] |= 1ULL;
    for (int i = 0; i < m->param_count; i++) {
        int slot = sema_param_slot(m, i);
        if (slot >= 0 && slot < da_nwords * 64)
            s->bits[slot / 64] |= (uint64_t)1 << (slot % 64);
    }
    return s;
}

/* Walk reachable stmt expressions; emit on reads of locals whose
 * slot bit is clear in the entry state of the containing node. */
static void check_da_expr(sema_ctx_t* ctx, const ast_expr_t* e,
                          const da_state_t* state) {
    if (!e) return;
    switch (e->tag) {
    case AST_IDENT: {
        const sema_ident_info_t* info = sema_ident_kind(ctx, e);
        if (info && (info->kind == SEMA_IDENT_LOCAL ||
                     info->kind == SEMA_IDENT_PARAM)
            && !da_bit_get(state, info->slot)) {
            sema_diag_t d = {0};
            d.level = DIAG_ERROR;
            d.loc = e->loc;
            snprintf(d.message, sizeof(d.message),
                     "variable '%s' may not have been initialized",
                     e->ident.name);
            bbq_vec_push(ctx->diags, d);
        }
        return;
    }
    case AST_ASSIGN:
        /* LHS write: descend only if target is a field/array (subexprs
         * are read). Pure ident target: don't read-check. */
        if (e->assign.target && e->assign.target->tag != AST_IDENT)
            check_da_expr(ctx, e->assign.target, state);
        check_da_expr(ctx, e->assign.value, state);
        return;
    case AST_COMPOUNDASSIGN:
        /* Read-modify-write: always check the target as a read. */
        check_da_expr(ctx, e->compound_assign.target, state);
        check_da_expr(ctx, e->compound_assign.value, state);
        return;
    case AST_UNARY:
        check_da_expr(ctx, e->unary.e, state);
        return;
    case AST_BINARY:
        check_da_expr(ctx, e->binary.lhs, state);
        check_da_expr(ctx, e->binary.rhs, state);
        return;
    case AST_TERNARY:
        check_da_expr(ctx, e->ternary.test, state);
        check_da_expr(ctx, e->ternary.then, state);
        check_da_expr(ctx, e->ternary.else_, state);
        return;
    case AST_CAST:       check_da_expr(ctx, e->cast.e, state); return;
    case AST_INSTANCEOF: check_da_expr(ctx, e->instance_of.e, state); return;
    case AST_FIELDACCESS: check_da_expr(ctx, e->field_access.obj, state); return;
    case AST_ARRAYACCESS:
        check_da_expr(ctx, e->array_access.arr, state);
        check_da_expr(ctx, e->array_access.index, state);
        return;
    case AST_METHODCALL:
        check_da_expr(ctx, e->method_call.obj, state);
        for (int i = 0; i < e->method_call.args_count; i++)
            check_da_expr(ctx, e->method_call.args[i], state);
        return;
    case AST_NEW:
        for (int i = 0; i < e->new_.args_count; i++)
            check_da_expr(ctx, e->new_.args[i], state);
        return;
    case AST_NEWARRAY:
        for (int _d = 0; _d < e->new_array.dims_count; _d++)
            if (e->new_array.dims[_d]) check_da_expr(ctx, e->new_array.dims[_d], state);
        return;
    case AST_ARRAYINIT:
        for (int i = 0; i < e->array_init.elems_count; i++)
            check_da_expr(ctx, e->array_init.elems[i], state);
        return;
    default: return;
    }
}

static void check_da_stmt(sema_ctx_t* ctx, const ast_stmt_t* s,
                          const da_state_t* state) {
    if (!s) return;
    switch (s->tag) {
    case AST_EXPRSTMT: check_da_expr(ctx, s->expr_stmt.e, state); return;
    case AST_LOCALVARDECL: {
        /* Thread intra-declaration assignments left-to-right: a later declarator's
         * initializer may read an earlier one (`int hx = f(), ix = hx & …`), which
         * IS definitely assigned by then (JLS §16). Check each init against the
         * running state, then mark that declarator (if it has an initializer). */
        /* da_state_t carries a flexible bits[] — copy through the
         * arena, never by value. */
        da_state_t* cur = (da_state_t*)bbq_arena_alloc(ctx->arena, da_size());
        memcpy(cur->bits, state->bits, da_size());
        for (int i = 0; i < s->local_var_decl.decls_count; i++) {
            const ast_var_decl_t* d = s->local_var_decl.decls[i];
            check_da_expr(ctx, d->init, cur);
            if (d->init) {
                int32_t slot = sema_slot(ctx, (ast_var_decl_t*)d);
                if (slot >= 0 && slot < da_nwords * 64)
                    cur->bits[slot / 64] |= (uint64_t)1 << (slot % 64);
            }
        }
        return;
    }
    case AST_IF:     check_da_expr(ctx, s->if_.test, state);     return;
    case AST_WHILE:  check_da_expr(ctx, s->while_.test, state);  return;
    case AST_DOWHILE: check_da_expr(ctx, s->do_while.test, state); return;
    case AST_FOR:
        check_da_expr(ctx, s->for_.test, state);
        for (int i = 0; i < s->for_.update_count; i++)
            check_da_expr(ctx, s->for_.update[i], state);
        return;
    case AST_SWITCH: check_da_expr(ctx, s->switch_.selector, state); return;
    case AST_RETURN: check_da_expr(ctx, s->return_.value, state); return;
    case AST_THROW:  check_da_expr(ctx, s->throw_.e, state);     return;
    default: return;
    }
}

/* ── Reachability lattice ──────────────────────────────────────── */

static void* reach_bottom(bbq_arena* a) {
    (void)a;
    return REACHABILITY_BOTTOM;
}

static void* reach_meet(bbq_arena* a, const void* x, const void* y) {
    (void)a;
    return (x == REACHABILITY_ON || y == REACHABILITY_ON)
        ? REACHABILITY_ON : REACHABILITY_BOTTOM;
}

static bool reach_le(const void* x, const void* y) {
    if (x == REACHABILITY_BOTTOM) return true;
    return y == REACHABILITY_ON;
}

static void* reach_transfer(const lattice_ops_t* self_ops,
                            bbq_arena* a, const cfg_edge_t* e,
                            const void* in) {
    (void)a;
    if (in == REACHABILITY_BOTTOM) return REACHABILITY_BOTTOM;
    /* NORMAL fallthrough from a noreturn-call stmt is unreachable.
     * EXCEPTION / TRUE / FALSE / CASE / DEFAULT edges still propagate —
     * the throw itself is the live flow. */
    if (e->kind == CFG_EDGE_NORMAL && e->from && e->from->stmt) {
        const reachability_t* r = (const reachability_t*)self_ops;
        if (r->noreturn_stmts &&
            bbq_htree_contains(r->noreturn_stmts,
                               (uint32_t)(uintptr_t)e->from->stmt))
            return REACHABILITY_BOTTOM;
    }
    return (void*)in;
}

void reachability_init(reachability_t* r, const bbq_htree* noreturn_stmts) {
    r->ops.bottom   = reach_bottom;
    r->ops.meet     = reach_meet;
    r->ops.widen    = NULL;
    r->ops.le       = reach_le;
    r->ops.transfer = reach_transfer;
    r->noreturn_stmts = noreturn_stmts;
}

/* ── Whole-program noreturn fixpoint ───────────────────────────── */

static bool method_body_stmt(const sema_method_t* m, ast_stmt_t** out) {
    if (!m->ast_node) return false;
    if (m->ast_node->tag == AST_METHODDECL)
        *out = m->ast_node->method_decl.body;
    else if (m->ast_node->tag == AST_CONSTRUCTORDECL)
        *out = m->ast_node->constructor_decl.body;
    else return false;
    return *out != NULL;
}

static bool is_throwable_class(const sema_ctx_t* ctx, int class_id) {
    return sema_is_subclass_of(ctx, class_id, ctx->wk.throwable_id);
}

/* Method is noreturn iff EXIT's entry-state is BOT after the
 * reachability fixpoint — transfer-pruning from noreturn calls is
 * already factored in at that point. */
static bool method_cant_complete(const cfg_t* g, void* const* state) {
    return state[g->exit->id] == REACHABILITY_BOTTOM;
}

/* Scan a CFG for bare method-call stmts whose resolved method is in
 * `noreturn_methods`; return an htree of those stmts. */
static bbq_htree* collect_noreturn_stmts(const sema_ctx_t* ctx,
                                          const cfg_t* g,
                                          const bbq_htree* noreturn_methods) {
    bbq_htree* out = bbq_htree_create();
    for (int i = 0; i < bbq_vec_len(g->nodes); i++) {
        const cfg_node_t* nd = g->nodes[i];
        if (nd->kind != CFG_NODE_STMT || !nd->stmt) continue;
        if (nd->stmt->tag != AST_EXPRSTMT) continue;
        const ast_expr_t* e = nd->stmt->expr_stmt.e;
        if (!e || e->tag != AST_METHODCALL) continue;
        const sema_method_t* m = sema_resolved_method(ctx, e);
        if (!m) continue;
        if (bbq_htree_contains(noreturn_methods, (uint32_t)(uintptr_t)m))
            bbq_htree_insert(out, (uint32_t)(uintptr_t)nd->stmt, (void*)1);
    }
    return out;
}

static void seed_framework_noreturn(const sema_ctx_t* ctx, bbq_htree* nr) {
    for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
        const sema_class_t* c = &ctx->classes[ci];
        if (!is_throwable_class(ctx, ci)) continue;
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            const sema_method_t* m = &c->methods[mi];
            if (m->name && strcmp(m->name, "throwIt") == 0)
                bbq_htree_insert(nr, (uint32_t)(uintptr_t)m, (void*)1);
        }
    }
}

/* Iterate per-method reachability until the noreturn-method set
 * stabilizes. Bounded by O(M²) in the worst case; converges in 1-2
 * passes for typical code. */
static void compute_noreturn_set(sema_ctx_t* ctx, bbq_htree* noreturn_methods) {
    seed_framework_noreturn(ctx, noreturn_methods);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
            sema_class_t* c = &ctx->classes[ci];
            if (c->import_pkg >= 0) continue;
            for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
                sema_method_t* m = &c->methods[mi];
                if (bbq_htree_contains(noreturn_methods,
                                        (uint32_t)(uintptr_t)m)) continue;
                ast_stmt_t* body;
                if (!method_body_stmt(m, &body)) continue;

                cfg_t g;
                cfg_build(&g, ctx, ctx->arena, body);
                bbq_htree* nr_stmts = collect_noreturn_stmts(
                    ctx, &g, noreturn_methods);
                reachability_t r;
                reachability_init(&r, nr_stmts);
                int n = cfg_node_count(&g);
                void** state = (void**)bbq_arena_alloc(
                    ctx->arena, sizeof(void*) * n);
                cfg_fixpoint(&g, &r.ops, ctx->arena, REACHABILITY_ON, state);

                if (method_cant_complete(&g, state)) {
                    bbq_htree_insert(noreturn_methods,
                                     (uint32_t)(uintptr_t)m, (void*)1);
                    changed = true;
                }
                bbq_htree_destroy(nr_stmts);
                cfg_destroy(&g);
            }
        }
    }
}

/* ── analyses_run: per-method driver ───────────────────────────── */

/* Emit a warning to ctx->diags. Mirrors sema.c's static diag() helper;
 * pushed to the shared diag vec so formatting is uniform. */
static void warn(sema_ctx_t* ctx, ast_srcloc loc, const char* fmt, ...) {
    sema_diag_t d = {0};
    d.level = DIAG_WARNING;
    d.loc = loc;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d.message, sizeof(d.message), fmt, ap);
    va_end(ap);
    bbq_vec_push(ctx->diags, d);
}

/* Walk sub-expressions of `e`, warning on any method invocation whose
 * receiver is a local/param we've tracked as NULL at program point
 * `state`. MAYBE receivers are not warned at Phase A — they fire on
 * every unannotated reference parameter and destroy the signal. */
static void check_exprs(sema_ctx_t* ctx, const ast_expr_t* e,
                        const null_state_t* state) {
    if (!e) return;
    switch (e->tag) {
    case AST_METHODCALL: {
        const ast_expr_t* obj = e->method_call.obj;
        if (obj && obj->tag == AST_IDENT &&
            strcmp(obj->ident.name, "this") != 0) {
            null_val_t v = state_get(state, fnv1a(obj->ident.name));
            if (v == NULL_NULL)
                warn(ctx, obj->loc,
                     "method invocation on null reference '%s'",
                     obj->ident.name);
        }
        check_exprs(ctx, obj, state);
        for (int i = 0; i < e->method_call.args_count; i++)
            check_exprs(ctx, e->method_call.args[i], state);
        return;
    }
    case AST_FIELDACCESS:
        check_exprs(ctx, e->field_access.obj, state);
        return;
    case AST_ARRAYACCESS:
        check_exprs(ctx, e->array_access.arr, state);
        check_exprs(ctx, e->array_access.index, state);
        return;
    case AST_ASSIGN:
        check_exprs(ctx, e->assign.target, state);
        check_exprs(ctx, e->assign.value, state);
        return;
    case AST_COMPOUNDASSIGN:
        check_exprs(ctx, e->compound_assign.target, state);
        check_exprs(ctx, e->compound_assign.value, state);
        return;
    case AST_BINARY:
        check_exprs(ctx, e->binary.lhs, state);
        check_exprs(ctx, e->binary.rhs, state);
        return;
    case AST_UNARY:
        check_exprs(ctx, e->unary.e, state);
        return;
    case AST_TERNARY:
        check_exprs(ctx, e->ternary.test, state);
        check_exprs(ctx, e->ternary.then, state);
        check_exprs(ctx, e->ternary.else_, state);
        return;
    case AST_CAST:
        check_exprs(ctx, e->cast.e, state);
        return;
    case AST_INSTANCEOF:
        check_exprs(ctx, e->instance_of.e, state);
        return;
    case AST_NEW:
        for (int i = 0; i < e->new_.args_count; i++)
            check_exprs(ctx, e->new_.args[i], state);
        return;
    case AST_NEWARRAY:
        for (int _d = 0; _d < e->new_array.dims_count; _d++)
            if (e->new_array.dims[_d]) check_exprs(ctx, e->new_array.dims[_d], state);
        return;
    default:
        return;
    }
}

/* Walk expressions looking for `arr[i]` and warn when we can prove
 * the index is not always inside [0, length). Fires when:
 *   - the array length is known precisely (av.lo == av.hi),
 *   - the index has a tracked interval,
 *   - and (i.lo < 0 OR i.hi >= length).
 * Skips arrays of unknown length (params, fields) to avoid a flood
 * of false positives on ref-typed inputs. */
static void check_bounds(sema_ctx_t* ctx, const ast_expr_t* e,
                         const interval_state_t* state, ast_srcloc floc) {
    if (!e) return;
    switch (e->tag) {
    case AST_ARRAYACCESS: {
        const ast_expr_t* arr = e->array_access.arr;
        const ast_expr_t* idx = e->array_access.index;
        if (arr && arr->tag == AST_IDENT) {
            interval_val_t av = iv_state_get(state, fnv1a(arr->ident.name));
            interval_val_t iv = iv_eval(idx, state);
            if (!interval_is_bot(av) && !interval_is_bot(iv)
                && av.lo == av.hi && av.lo >= 0) {
                int64_t len = av.lo;
                bool safe = (iv.lo >= 0 && iv.hi < len);
                if (!safe) {
                    sema_diag_t d = {0};
                    d.level = DIAG_WARNING;
                    d.kind  = SEMA_DIAG_ARRAY_BOUNDS;
                    /* Interior expression nodes may carry no stamped loc
                     * (a 0:0 loc renders as a useless "<input>:0:0") — fall
                     * back to the enclosing STATEMENT's loc so the warning
                     * always lands somewhere a person can look. */
                    d.loc = (idx && idx->loc.line) ? idx->loc
                          : e->loc.line            ? e->loc
                          : floc;
                    if (iv.lo == iv.hi)
                        snprintf(d.message, sizeof(d.message),
                                 "array index %lld out of bounds for length %lld",
                                 (long long)iv.lo, (long long)len);
                    else
                        snprintf(d.message, sizeof(d.message),
                                 "array index in [%lld, %lld] not provably within [0, %lld)",
                                 (long long)iv.lo, (long long)iv.hi,
                                 (long long)len);
                    bbq_vec_push(ctx->diags, d);
                }
            }
        }
        check_bounds(ctx, arr, state, floc);
        check_bounds(ctx, idx, state, floc);
        return;
    }
    case AST_FIELDACCESS:
        check_bounds(ctx, e->field_access.obj, state, floc);
        return;
    case AST_METHODCALL:
        check_bounds(ctx, e->method_call.obj, state, floc);
        for (int i = 0; i < e->method_call.args_count; i++)
            check_bounds(ctx, e->method_call.args[i], state, floc);
        return;
    case AST_ASSIGN:
        check_bounds(ctx, e->assign.target, state, floc);
        check_bounds(ctx, e->assign.value, state, floc);
        return;
    case AST_COMPOUNDASSIGN:
        check_bounds(ctx, e->compound_assign.target, state, floc);
        check_bounds(ctx, e->compound_assign.value, state, floc);
        return;
    case AST_BINARY:
        check_bounds(ctx, e->binary.lhs, state, floc);
        check_bounds(ctx, e->binary.rhs, state, floc);
        return;
    case AST_UNARY:  check_bounds(ctx, e->unary.e, state, floc); return;
    case AST_TERNARY:
        check_bounds(ctx, e->ternary.test, state, floc);
        check_bounds(ctx, e->ternary.then, state, floc);
        check_bounds(ctx, e->ternary.else_, state, floc);
        return;
    case AST_CAST: {
        /* Narrowing cast to byte / short — warn if subexpr's tracked
         * interval can't fit the target range. Only fires when the
         * interval is precisely bounded (not TOP). */
        const ast_type_t* ty = e->cast.ty;
        const char* tn = NULL;
        int64_t tmin = 0, tmax = 0;
        bool narrow = false;
        if (ty && ty->tag == AST_BYTETYPE) {
            tmin = -128; tmax = 127; narrow = true; tn = "byte";
        } else if (ty && ty->tag == AST_SHORTTYPE) {
            tmin = -32768; tmax = 32767; narrow = true; tn = "short";
        }
        if (narrow) {
            interval_val_t iv = iv_eval(e->cast.e, state);
            if (!interval_is_bot(iv) && !interval_is_top(iv)
                && (iv.lo < tmin || iv.hi > tmax)) {
                sema_diag_t d = {0};
                d.level = DIAG_WARNING;
                d.kind  = SEMA_DIAG_NARROWING_CAST;
                d.loc = e->loc;
                snprintf(d.message, sizeof(d.message),
                         "narrowing cast to %s: value in [%lld, %lld] exceeds range [%lld, %lld]",
                         tn, (long long)iv.lo, (long long)iv.hi,
                         (long long)tmin, (long long)tmax);
                bbq_vec_push(ctx->diags, d);
            }
        }
        check_bounds(ctx, e->cast.e, state, floc);
        return;
    }
    case AST_INSTANCEOF:  check_bounds(ctx, e->instance_of.e, state, floc); return;
    case AST_NEW:
        for (int i = 0; i < e->new_.args_count; i++)
            check_bounds(ctx, e->new_.args[i], state, floc);
        return;
    case AST_NEWARRAY:    for (int _d = 0; _d < e->new_array.dims_count; _d++)
                              if (e->new_array.dims[_d]) check_bounds(ctx, e->new_array.dims[_d], state, floc);
                          return;
    default: return;
    }
}

static void check_bounds_stmt(sema_ctx_t* ctx, const ast_stmt_t* s,
                              const interval_state_t* state) {
    if (!s) return;
    switch (s->tag) {
    case AST_EXPRSTMT:     check_bounds(ctx, s->expr_stmt.e, state, s->loc); return;
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++)
            check_bounds(ctx, s->local_var_decl.decls[i]->init, state, s->loc);
        return;
    case AST_IF:       check_bounds(ctx, s->if_.test, state, s->loc); return;
    case AST_WHILE:    check_bounds(ctx, s->while_.test, state, s->loc); return;
    case AST_DOWHILE:  check_bounds(ctx, s->do_while.test, state, s->loc); return;
    case AST_FOR:
        check_bounds(ctx, s->for_.test, state, s->loc);
        for (int i = 0; i < s->for_.update_count; i++)
            check_bounds(ctx, s->for_.update[i], state, s->loc);
        return;
    case AST_SWITCH:   check_bounds(ctx, s->switch_.selector, state, s->loc); return;
    case AST_RETURN:   check_bounds(ctx, s->return_.value, state, s->loc); return;
    case AST_THROW:    check_bounds(ctx, s->throw_.e, state, s->loc); return;
    default: return;
    }
}

/* ── Enum-like switch coverage ─────────────────────────────────── */

static int find_class_owning_field(const sema_ctx_t* ctx,
                                    const sema_field_t* f) {
    for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
        const sema_class_t* c = &ctx->classes[ci];
        for (int fi = 0; fi < bbq_vec_len(c->fields); fi++)
            if (&c->fields[fi] == f) return ci;
    }
    return -1;
}

static const sema_field_t* resolve_const_ref(const sema_ctx_t* ctx,
                                              const ast_expr_t* e) {
    if (!e) return NULL;
    if (e->tag == AST_FIELDACCESS)
        return sema_resolved_field(ctx, e);
    if (e->tag == AST_IDENT) {
        const sema_ident_info_t* info = sema_ident_kind(ctx, e);
        if (info && (info->kind == SEMA_IDENT_INSTANCE_FIELD ||
                     info->kind == SEMA_IDENT_STATIC_FIELD))
            return info->field;
    }
    return NULL;
}

static bool is_static_final_primitive(const sema_field_t* f) {
    if (!f) return false;
    if (!(f->modifiers & ACC_STATIC)) return false;
    if (!(f->modifiers & ACC_FINAL)) return false;
    return f->type.tag == JT_BYTE || f->type.tag == JT_SHORT
        || f->type.tag == JT_INT;
}

/* Warn when a switch covers only some of a class's `NAME_*` static
 * final primitive constants and lacks a default. Heuristic: all case
 * values are static-final primitives from the same user class,
 * sharing an underscore-prefix, and the class has more such
 * constants than the switch covers. */
static void check_switch_enum_coverage(sema_ctx_t* ctx,
                                        const ast_stmt_t* s) {
    if (!s || s->tag != AST_SWITCH) return;
    const sema_switch_info_t* info = sema_switch_info(ctx, s);
    if (!info || info->default_idx >= 0) return;
    if (s->switch_.cases_count < 3) return;

    const sema_field_t** refs = NULL;
    int class_id = -1;
    for (int i = 0; i < s->switch_.cases_count; i++) {
        ast_switch_case_t* sc = s->switch_.cases[i];
        if (!sc->value) { bbq_vec_free(refs); return; }
        const sema_field_t* f = resolve_const_ref(ctx, sc->value);
        if (!is_static_final_primitive(f)) { bbq_vec_free(refs); return; }
        int cid = find_class_owning_field(ctx, f);
        if (cid < 0) { bbq_vec_free(refs); return; }
        if (class_id < 0) class_id = cid;
        else if (class_id != cid) { bbq_vec_free(refs); return; }
        bbq_vec_push(refs, f);
    }
    const sema_class_t* c = &ctx->classes[class_id];
    if (c->import_pkg >= 0) { bbq_vec_free(refs); return; }

    /* Common prefix up through the last shared '_'. */
    const char* first_name = refs[0]->name;
    int plen = (int)strlen(first_name);
    for (int i = 1; i < bbq_vec_len(refs); i++) {
        const char* n = refs[i]->name;
        int k = 0;
        while (k < plen && n[k] && first_name[k] == n[k]) k++;
        plen = k;
    }
    int uspos = -1;
    for (int i = plen - 1; i >= 0; i--)
        if (first_name[i] == '_') { uspos = i; break; }
    if (uspos < 0) { bbq_vec_free(refs); return; }
    int prefix_len = uspos + 1;

    const sema_field_t** missing = NULL;
    for (int fi = 0; fi < bbq_vec_len(c->fields); fi++) {
        const sema_field_t* f = &c->fields[fi];
        if (!is_static_final_primitive(f)) continue;
        if ((int)strlen(f->name) < prefix_len) continue;
        if (strncmp(f->name, first_name, prefix_len) != 0) continue;
        bool covered = false;
        for (int i = 0; i < bbq_vec_len(refs); i++)
            if (refs[i] == f) { covered = true; break; }
        if (!covered) bbq_vec_push(missing, f);
    }

    if (bbq_vec_len(missing) > 0) {
        sema_diag_t d = {0};
        d.level = DIAG_WARNING;
        d.loc = s->loc;
        int w = snprintf(d.message, sizeof(d.message),
                         "switch missing case for constant%s: ",
                         bbq_vec_len(missing) == 1 ? "" : "s");
        int cap = 5;
        int shown = bbq_vec_len(missing) < cap ? bbq_vec_len(missing) : cap;
        for (int i = 0; i < shown && w < (int)sizeof(d.message); i++) {
            w += snprintf(d.message + w, sizeof(d.message) - w,
                          "%s%s", i == 0 ? "" : ", ", missing[i]->name);
        }
        if (bbq_vec_len(missing) > cap && w < (int)sizeof(d.message))
            snprintf(d.message + w, sizeof(d.message) - w,
                     ", +%d more", bbq_vec_len(missing) - cap);
        bbq_vec_push(ctx->diags, d);
    }

    bbq_vec_free(refs);
    bbq_vec_free(missing);
}

/* Walk the expressions inside a stmt at its CFG node's entry state. */
static void check_stmt_exprs(sema_ctx_t* ctx, const ast_stmt_t* s,
                             const null_state_t* state) {
    if (!s) return;
    switch (s->tag) {
    case AST_EXPRSTMT:
        check_exprs(ctx, s->expr_stmt.e, state);
        return;
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++)
            check_exprs(ctx, s->local_var_decl.decls[i]->init, state);
        return;
    case AST_IF:
        check_exprs(ctx, s->if_.test, state);
        return;
    case AST_WHILE:
        check_exprs(ctx, s->while_.test, state);
        return;
    case AST_DOWHILE:
        check_exprs(ctx, s->do_while.test, state);
        return;
    case AST_FOR:
        check_exprs(ctx, s->for_.test, state);
        for (int i = 0; i < s->for_.update_count; i++)
            check_exprs(ctx, s->for_.update[i], state);
        return;
    case AST_SWITCH:
        check_exprs(ctx, s->switch_.selector, state);
        return;
    case AST_RETURN:
        check_exprs(ctx, s->return_.value, state);
        return;
    case AST_THROW:
        check_exprs(ctx, s->throw_.e, state);
        return;
    default:
        return;
    }
}

static void emit_diag(sema_ctx_t* ctx, diag_level_t level,
                      ast_srcloc loc, const char* msg) {
    sema_diag_t d = {0};
    d.level = level;
    d.loc = loc;
    snprintf(d.message, sizeof(d.message), "%s", msg);
    bbq_vec_push(ctx->diags, d);
}

static void check_discarded_pure_call(sema_ctx_t* ctx, const ast_stmt_t* s,
                                       const bbq_htree* pure_set);
static void check_exception_flow(sema_ctx_t* ctx, const ast_stmt_t* body);

/* Bundle of per-run context — noreturn set, pure set, per-framework-
 * class resolved method sets. Threaded through run_on_method so that
 * adding a new framework analysis doesn't grow the per-method call
 * signature. */
typedef struct {
    const bbq_htree* noreturn_methods;
    const bbq_htree* pure_set;
} analyses_env_t;

static void run_on_method(sema_ctx_t* ctx, const sema_method_t* m,
                          ast_stmt_t* body,
                          const analyses_env_t* env) {
    cfg_t g;
    cfg_build(&g, ctx, ctx->arena, body);
    int n = cfg_node_count(&g);

    /* §14.19's own rules — both halves, structural (jls_check_reachability) — emit the
     * "unreachable statement" error and the noreturn-aware "dead code" warning. The CFG is not
     * consulted for them: a dataflow reachability is a DIFFERENT analysis, and when it disagrees
     * with the completion rules the backend reads, the disagreement becomes bad code rather than a
     * diagnostic. `nr_reach` survives only to gate the auxiliary lattice checks below, which are
     * meaningless at a node no execution reaches. */
    bbq_htree* nr_stmts = collect_noreturn_stmts(ctx, &g, env->noreturn_methods);
    jls_check_reachability(ctx, body, nr_stmts);

    reachability_t r_nr;
    reachability_init(&r_nr, nr_stmts);
    void** nr_reach = (void**)bbq_arena_alloc(ctx->arena, sizeof(void*) * n);
    cfg_fixpoint(&g, &r_nr.ops, ctx->arena, REACHABILITY_ON, nr_reach);

    /* Nullability fixpoint. */
    const char** names = NULL;
    for (int i = 0; i < m->param_count; i++) {
        if (jt_is_reference(m->param_types[i]) && m->param_names[i])
            bbq_vec_push(names, m->param_names[i]);
    }
    void* null_entry = nullability_entry_state(
        ctx->arena, names, bbq_vec_len(names));
    void** nulls = (void**)bbq_arena_alloc(ctx->arena, sizeof(void*) * n);
    cfg_fixpoint(&g, &nullability_ops, ctx->arena, null_entry, nulls);

    /* Interval fixpoint — params enter as ⊤. */
    const char** iv_names = NULL;
    for (int i = 0; i < m->param_count; i++)
        if (m->param_names[i]) bbq_vec_push(iv_names, m->param_names[i]);
    void* iv_entry = interval_entry_state(
        ctx->arena, iv_names, bbq_vec_len(iv_names));
    void** ivs = (void**)bbq_arena_alloc(ctx->arena, sizeof(void*) * n);
    cfg_fixpoint(&g, &interval_ops, ctx->arena, iv_entry, ivs);

    /* DA fixpoint — params + `this` seeded, intersection at joins.
     * Size the bitmap to THIS method's slot count (§16 must hold to
     * the 65535-local limit — a fixed cap fails open past it). */
    {
        int nslots = (int)sema_max_user_slots(m) + 1;
        da_nwords = (nslots + 63) / 64;
        if (da_nwords < 4) da_nwords = 4;
    }
    da_lattice_t da; da_lattice_init(&da, ctx);
    void* da_initial = da_entry_state(ctx->arena, m);
    void** das = (void**)bbq_arena_alloc(ctx->arena, sizeof(void*) * n);
    cfg_fixpoint(&g, &da.ops, ctx->arena, da_initial, das);

    /* §8.4.5: "A compile-time error occurs if
     * the body of the method can complete normally." That is §14.19's completion predicate, so it
     * is the SAME rules — read here with the noreturn extension, so `if (c) return 1; else fail();`
     * is accepted rather than demanding an explicit throw. */
    if (!m->is_constructor && m->return_type.tag != JT_VOID
        && jls_can_complete_normally_nr(ctx, body, nr_stmts)) {
        sema_diag_t d = {0};
        d.level = DIAG_ERROR;
        d.loc = m->ast_node ? m->ast_node->loc : body->loc;
        snprintf(d.message, sizeof(d.message),
                 "missing return statement in method '%s'",
                 m->name ? m->name : "");
        bbq_vec_push(ctx->diags, d);
    }

    /* The auxiliary lattice checks run only where execution can arrive. */
    for (int i = 0; i < n; i++) {
        cfg_node_t* nd = g.nodes[i];
        if (nd->kind != CFG_NODE_STMT || !nd->stmt) continue;
        if (nr_reach[i] == REACHABILITY_BOTTOM) continue;
        check_stmt_exprs(ctx, nd->stmt, (const null_state_t*)nulls[i]);
        check_bounds_stmt(ctx, nd->stmt, (const interval_state_t*)ivs[i]);
        check_da_stmt(ctx, nd->stmt, (const da_state_t*)das[i]);
        check_switch_enum_coverage(ctx, nd->stmt);
        check_discarded_pure_call(ctx, nd->stmt, env->pure_set);
    }

    check_exception_flow(ctx, body);

    bbq_htree_destroy(nr_stmts);
    cfg_destroy(&g);
    bbq_vec_free(names);
    bbq_vec_free(iv_names);
}

/* ── Exception-flow completeness ───────────────────────────────── */

/* Cached class_ids of common java.lang RuntimeExceptions. Negative if
 * the corresponding class isn't loaded (e.g. lang.exp absent). Used to
 * seed the thrown-set with implicit exceptions raised by primitive
 * operations (array access → NPE/AIOOBE, cast → CCE, div/mod →
 * ArithmeticException, new T[n] → NegativeArraySizeException). Without
 * this, `catch (RuntimeException)` or `catch (NullPointerException)` on
 * a try body that only contains dereferences would be spuriously
 * flagged as unreachable. */
typedef struct {
    int npe_id;
    int aioobe_id;
    int cce_id;
    int arith_id;
    int nase_id;
    /* §11.2's unchecked-exception boundary: "The unchecked exceptions classes are the class
     * RuntimeException and its subclasses, and the class Error and its subclasses." */
    int rte_id;
    int error_id;
} ef_builtins_t;

static void ef_resolve_builtins(const sema_ctx_t* ctx, ef_builtins_t* b) {
    /* The §15 implicit-exception classes, from the well-known registry
     * (resolve_wellknown) — not re-looked-up by name here. */
    b->npe_id    = ctx->wk.null_pointer_id;
    b->aioobe_id = ctx->wk.array_index_oob_id;
    b->cce_id    = ctx->wk.class_cast_id;
    b->arith_id  = ctx->wk.arithmetic_id;
    b->nase_id   = ctx->wk.negative_array_size_id;
    b->rte_id    = ctx->wk.runtime_exception_id;
    b->error_id  = ctx->wk.error_id;
}

/* §14.19 makes a catch block reachable iff "some expression or throw statement in the try block is
 * reachable and can throw an exception whose type is assignable to the parameter of the catch
 * clause". §11.2 scopes the compile-time exception analysis to CHECKED exceptions, because an
 * unchecked one may result from almost any expression: a null dereference, a division, an array
 * index, an allocation failure. So a catch clause is never unreachable when
 *   - its parameter type is itself unchecked (RuntimeException / Error and their subclasses), or
 *   - its parameter type is assignable FROM an unchecked class (Exception, Throwable),
 * and only a checked parameter type is subject to the rule. `try { } catch (Exception e) {}` is
 * legal; `try { } catch (IOException e) {}` is not. */
static bool ef_catch_type_is_always_reachable(const sema_ctx_t* ctx, const ef_builtins_t* b,
                                              int catch_id) {
    int unchecked_roots[2] = { b->rte_id, b->error_id };
    for (int i = 0; i < 2; i++) {
        int root = unchecked_roots[i];
        if (root < 0) continue;
        if (sema_is_subclass_of(ctx, catch_id, root)) return true;   /* the caught type is unchecked */
        if (sema_is_subclass_of(ctx, root, catch_id)) return true;   /* it catches an unchecked type */
    }
    return false;
}

static void ef_add(bbq_htree* out, int class_id) {
    if (class_id >= 0)
        bbq_htree_insert(out, (uint32_t)class_id, (void*)1);
}

static void ef_add_method_throws(const sema_method_t* m, bbq_htree* out) {
    if (!m) return;
    for (int i = 0; i < m->thrown_count; i++) {
        if (m->thrown_types[i].tag == JT_CLASS)
            ef_add(out, m->thrown_types[i].class_id);
    }
}

/* Does `ident` syntactically refer to a class (static access), not a
 * value? We treat an identifier whose name resolves via sema_find_class
 * as a class-name ref. Used to skip NPE emission for `ClassName.field`
 * and `ClassName.method(...)`. */
static bool ef_is_class_ref(const sema_ctx_t* ctx, const ast_expr_t* e) {
    if (!e || e->tag != AST_IDENT) return false;
    /* Read sema's §6.5.4 classification — never re-resolve the name here
     * (resolution is unit-relative; a bare table lookup is wrong under §7). */
    const sema_ident_info_t* info = sema_ident_kind(ctx, e);
    return info && info->kind == SEMA_IDENT_CLASSREF;
}

static void ef_collect_expr(sema_ctx_t* ctx, const ef_builtins_t* b,
                            const ast_expr_t* e, bbq_htree* out);
static void ef_collect_stmt(sema_ctx_t* ctx, const ef_builtins_t* b,
                            const ast_stmt_t* s, bbq_htree* out);

static void ef_collect_args(sema_ctx_t* ctx, const ef_builtins_t* b,
                            ast_expr_t** args, int n, bbq_htree* out) {
    for (int i = 0; i < n; i++) ef_collect_expr(ctx, b, args[i], out);
}

static void ef_collect_expr(sema_ctx_t* ctx, const ef_builtins_t* b,
                            const ast_expr_t* e, bbq_htree* out) {
    if (!e) return;
    switch (e->tag) {
    case AST_METHODCALL: {
        const sema_method_t* m = sema_resolved_method(ctx, e);
        ef_add_method_throws(m, out);
        /* Instance call on a null-capable ref may NPE. */
        if ((!m || !(m->modifiers & ACC_STATIC))
            && !ef_is_class_ref(ctx, e->method_call.obj))
            ef_add(out, b->npe_id);
        ef_collect_expr(ctx, b, e->method_call.obj, out);
        ef_collect_args(ctx, b, e->method_call.args, e->method_call.args_count, out);
        return;
    }
    case AST_SUPERCALL:
        ef_add_method_throws(sema_resolved_super_method(ctx, e), out);
        ef_collect_args(ctx, b, e->super_call.args, e->super_call.args_count, out);
        return;
    case AST_NEW:
        ef_add_method_throws(sema_resolved_constructor(ctx, e), out);
        ef_collect_args(ctx, b, e->new_.args, e->new_.args_count, out);
        return;
    case AST_CONSTRUCTORCALL:
        ef_add_method_throws(sema_resolved_constructor(ctx, e), out);
        ef_collect_args(ctx, b, e->constructor_call.args,
                        e->constructor_call.args_count, out);
        return;
    case AST_ASSIGN:
        ef_collect_expr(ctx, b, e->assign.target, out);
        ef_collect_expr(ctx, b, e->assign.value, out);
        return;
    case AST_COMPOUNDASSIGN:
        ef_collect_expr(ctx, b, e->compound_assign.target, out);
        ef_collect_expr(ctx, b, e->compound_assign.value, out);
        return;
    case AST_BINARY:
        if (e->binary.op == AST_DIV || e->binary.op == AST_REM) {
            /* §15.16.2/.3: INTEGER division/remainder throws on a zero
             * divisor — int AND long (float/double yield NaN/∞). */
            java_type_t t = sema_type_of(ctx, e);
            if (t.tag == JT_BYTE || t.tag == JT_SHORT || t.tag == JT_CHAR
                || t.tag == JT_INT || t.tag == JT_LONG)
                ef_add(out, b->arith_id);
        }
        ef_collect_expr(ctx, b, e->binary.lhs, out);
        ef_collect_expr(ctx, b, e->binary.rhs, out);
        return;
    case AST_UNARY:       ef_collect_expr(ctx, b, e->unary.e, out); return;
    case AST_TERNARY:
        ef_collect_expr(ctx, b, e->ternary.test, out);
        ef_collect_expr(ctx, b, e->ternary.then, out);
        ef_collect_expr(ctx, b, e->ternary.else_, out);
        return;
    case AST_CAST: {
        /* CCE only for reference casts. */
        java_type_t tt = sema_type_of(ctx, e);
        if (tt.tag == JT_CLASS || tt.tag == JT_ARRAY)
            ef_add(out, b->cce_id);
        ef_collect_expr(ctx, b, e->cast.e, out);
        return;
    }
    case AST_INSTANCEOF:  ef_collect_expr(ctx, b, e->instance_of.e, out); return;
    case AST_FIELDACCESS: {
        const ast_expr_t* obj = e->field_access.obj;
        /* Instance field access through null-capable ref may NPE. */
        if (!ef_is_class_ref(ctx, obj))
            ef_add(out, b->npe_id);
        ef_collect_expr(ctx, b, obj, out);
        return;
    }
    case AST_ARRAYACCESS:
        ef_add(out, b->npe_id);
        ef_add(out, b->aioobe_id);
        ef_collect_expr(ctx, b, e->array_access.arr, out);
        ef_collect_expr(ctx, b, e->array_access.index, out);
        return;
    case AST_NEWARRAY:
        ef_add(out, b->nase_id);
        for (int _d = 0; _d < e->new_array.dims_count; _d++)
            if (e->new_array.dims[_d]) ef_collect_expr(ctx, b, e->new_array.dims[_d], out);
        return;
    case AST_ARRAYINIT:
        for (int i = 0; i < e->array_init.elems_count; i++)
            ef_collect_expr(ctx, b, e->array_init.elems[i], out);
        return;
    default: return;
    }
}


/* Remove from `thrown` every type assignable to any catch clause of
 * this try. Used when flowing a nested try's uncaught set into its
 * enclosing try's body set (W.3 — nested-try subtraction). */
static void ef_subtract_catches(sema_ctx_t* ctx, const ast_stmt_t* try_s,
                                bbq_htree* thrown) {
    bbq_htree_iter it;
    bbq_htree_iter_init(thrown, &it);
    bbq_htree_leaf* leaf;
    uint32_t* to_remove = NULL;   /* bbq_vec — a fixed cap silently kept
                                     caught types in the enclosing set */
    while ((leaf = bbq_htree_next(&it)) != NULL) {
        for (int i = 0; i < try_s->try_.catches_count; i++) {
            int catch_id = sema_catch_class_id(ctx, try_s->try_.catches[i]);
            if (catch_id < 0) continue;
            if (sema_is_subclass_of(ctx, (int)leaf->key, catch_id)) {
                bbq_vec_push(to_remove, leaf->key);
                break;
            }
        }
    }
    for (int i = 0; i < (int)bbq_vec_len(to_remove); i++)
        bbq_htree_delete(thrown, to_remove[i]);
    bbq_vec_free(to_remove);
}

static void ef_collect_stmt(sema_ctx_t* ctx, const ef_builtins_t* b,
                            const ast_stmt_t* s, bbq_htree* out) {
    if (!s) return;
    switch (s->tag) {
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            ef_collect_stmt(ctx, b, s->block.stmts[i], out);
        return;
    case AST_EXPRSTMT:    ef_collect_expr(ctx, b, s->expr_stmt.e, out); return;
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++)
            ef_collect_expr(ctx, b, s->local_var_decl.decls[i]->init, out);
        return;
    case AST_IF:
        ef_collect_expr(ctx, b, s->if_.test, out);
        ef_collect_stmt(ctx, b, s->if_.then, out);
        ef_collect_stmt(ctx, b, s->if_.else_, out);
        return;
    case AST_WHILE:
        ef_collect_expr(ctx, b, s->while_.test, out);
        ef_collect_stmt(ctx, b, s->while_.body, out);
        return;
    case AST_DOWHILE:
        ef_collect_expr(ctx, b, s->do_while.test, out);
        ef_collect_stmt(ctx, b, s->do_while.body, out);
        return;
    case AST_FOR:
        ef_collect_stmt(ctx, b, s->for_.init, out);
        ef_collect_expr(ctx, b, s->for_.test, out);
        for (int i = 0; i < s->for_.update_count; i++)
            ef_collect_expr(ctx, b, s->for_.update[i], out);
        ef_collect_stmt(ctx, b, s->for_.body, out);
        return;
    case AST_SWITCH:
        ef_collect_expr(ctx, b, s->switch_.selector, out);
        for (int i = 0; i < s->switch_.cases_count; i++)
            for (int j = 0; j < s->switch_.cases[i]->stmts_count; j++)
                ef_collect_stmt(ctx, b, s->switch_.cases[i]->stmts[j], out);
        return;
    case AST_TRY: {
        /* W.3: compute the body's throw set separately, subtract what
         * the nested catches cover, then flow only the survivors up to
         * the enclosing scope. Catch bodies and finally contribute as
         * normal. */
        bbq_htree* body_thrown = bbq_htree_create();
        ef_collect_stmt(ctx, b, s->try_.body, body_thrown);
        ef_subtract_catches(ctx, s, body_thrown);
        bbq_htree_iter it;
        bbq_htree_iter_init(body_thrown, &it);
        bbq_htree_leaf* leaf;
        while ((leaf = bbq_htree_next(&it)) != NULL)
            bbq_htree_insert(out, leaf->key, (void*)1);
        bbq_htree_destroy(body_thrown);
        for (int i = 0; i < s->try_.catches_count; i++)
            ef_collect_stmt(ctx, b, s->try_.catches[i]->body, out);
        ef_collect_stmt(ctx, b, s->try_.finally_, out);
        return;
    }
    case AST_RETURN:      ef_collect_expr(ctx, b, s->return_.value, out); return;
    case AST_THROW: {
        ef_collect_expr(ctx, b, s->throw_.e, out);
        java_type_t t = sema_type_of(ctx, s->throw_.e);
        if (t.tag == JT_CLASS)
            bbq_htree_insert(out, (uint32_t)t.class_id, (void*)1);
        return;
    }
    case AST_LABELED:     ef_collect_stmt(ctx, b, s->labeled.body, out); return;
    default: return;
    }
}

/* Walk a stmt tree, calling check on every AST_TRY found. */
static void ef_walk_for_tries(sema_ctx_t* ctx, const ef_builtins_t* b,
                              const ast_stmt_t* s);

static void ef_check_try(sema_ctx_t* ctx, const ef_builtins_t* b,
                         const ast_stmt_t* s) {
    bbq_htree* thrown = bbq_htree_create();
    ef_collect_stmt(ctx, b, s->try_.body, thrown);

    for (int i = 0; i < s->try_.catches_count; i++) {
        ast_catch_clause_t* cc = s->try_.catches[i];
        int catch_id = sema_catch_class_id(ctx, cc);
        if (catch_id < 0) continue;
        if (ef_catch_type_is_always_reachable(ctx, b, catch_id)) continue;

        /* Is any thrown type assignable to the catch type? */
        bool covered = false;
        bbq_htree_iter it;
        bbq_htree_iter_init(thrown, &it);
        bbq_htree_leaf* leaf;
        while ((leaf = bbq_htree_next(&it)) != NULL) {
            if (sema_is_subclass_of(ctx, (int)leaf->key, catch_id)) {
                covered = true;
                break;
            }
        }

        if (!covered) {
            /* §14.19: "It is a compile-time error if a statement cannot be executed because it is
             * unreachable." A catch block is a Block, and this one is unreachable — no reachable
             * expression or throw statement in the try block can throw a checked exception
             * assignable to its parameter. */
            sema_diag_t d = {0};
            d.level = DIAG_ERROR;
            d.loc = cc->loc;
            const sema_class_t* c = sema_get_class(ctx, catch_id);
            snprintf(d.message, sizeof(d.message),
                "unreachable catch clause: '%s' is never thrown in the body of "
                "the corresponding try statement",
                c && c->name ? c->name : "?");
            bbq_vec_push(ctx->diags, d);
        }
    }
    bbq_htree_destroy(thrown);
}

static void ef_walk_for_tries(sema_ctx_t* ctx, const ef_builtins_t* b,
                              const ast_stmt_t* s) {
    if (!s) return;
    switch (s->tag) {
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            ef_walk_for_tries(ctx, b, s->block.stmts[i]);
        return;
    case AST_TRY:
        ef_check_try(ctx, b, s);
        ef_walk_for_tries(ctx, b, s->try_.body);
        for (int i = 0; i < s->try_.catches_count; i++)
            ef_walk_for_tries(ctx, b, s->try_.catches[i]->body);
        ef_walk_for_tries(ctx, b, s->try_.finally_);
        return;
    case AST_IF:
        ef_walk_for_tries(ctx, b, s->if_.then);
        ef_walk_for_tries(ctx, b, s->if_.else_);
        return;
    case AST_WHILE:   ef_walk_for_tries(ctx, b, s->while_.body); return;
    case AST_DOWHILE: ef_walk_for_tries(ctx, b, s->do_while.body); return;
    case AST_FOR:
        ef_walk_for_tries(ctx, b, s->for_.init);
        ef_walk_for_tries(ctx, b, s->for_.body);
        return;
    case AST_SWITCH:
        for (int i = 0; i < s->switch_.cases_count; i++)
            for (int j = 0; j < s->switch_.cases[i]->stmts_count; j++)
                ef_walk_for_tries(ctx, b, s->switch_.cases[i]->stmts[j]);
        return;
    case AST_LABELED: ef_walk_for_tries(ctx, b, s->labeled.body); return;
    default: return;
    }
}

static void check_exception_flow(sema_ctx_t* ctx, const ast_stmt_t* body) {
    ef_builtins_t b;
    ef_resolve_builtins(ctx, &b);
    ef_walk_for_tries(ctx, &b, body);
}

/* ── Recursion detection (stack-depth) ─────────────────────────── */

typedef struct {
    const sema_method_t** callees;      /* bbq_vec, deduped — NON-TAIL call edges */
    const sema_method_t** tail_callees; /* bbq_vec, deduped — tail-call edges only */
    java_type_t           mret;         /* the owning method's declared return type */
} cg_entry_t;

static void cg_add(cg_entry_t* e, const sema_method_t* m, bool tail) {
    if (!m) return;
    const sema_method_t*** list = tail ? &e->tail_callees : &e->callees;
    for (int i = 0; i < bbq_vec_len(*list); i++)
        if ((*list)[i] == m) return;
    bbq_vec_push(*list, m);
}

/* Structural java_type_t equality. The tail shape needs an EXACT result-type
 * match: an implicit §5.2 widening puts a conversion node between Return and
 * the call, which takes the normal (frame-growing) path. */
static bool cg_type_eq(java_type_t a, java_type_t b) {
    if (a.tag != b.tag) return false;
    if (a.tag == JT_CLASS) return a.class_id == b.class_id;
    if (a.tag == JT_ARRAY)
        return a.element && b.element && cg_type_eq(*a.element, *b.element);
    return true;
}

static void cg_walk_expr(sema_ctx_t* ctx, const ast_expr_t* e, cg_entry_t* out);

static void cg_walk_args(sema_ctx_t* ctx, ast_expr_t** args, int n,
                         cg_entry_t* out) {
    for (int i = 0; i < n; i++) cg_walk_expr(ctx, args[i], out);
}

static void cg_walk_expr(sema_ctx_t* ctx, const ast_expr_t* e, cg_entry_t* out) {
    if (!e) return;
    switch (e->tag) {
    case AST_METHODCALL:
        cg_add(out, sema_resolved_method(ctx, e), false);
        cg_walk_expr(ctx, e->method_call.obj, out);
        cg_walk_args(ctx, e->method_call.args, e->method_call.args_count, out);
        return;
    case AST_SUPERCALL:
        cg_add(out, sema_resolved_super_method(ctx, e), false);
        cg_walk_args(ctx, e->super_call.args, e->super_call.args_count, out);
        return;
    case AST_NEW:
        cg_add(out, sema_resolved_constructor(ctx, e), false);
        cg_walk_args(ctx, e->new_.args, e->new_.args_count, out);
        return;
    case AST_CONSTRUCTORCALL:
        cg_add(out, sema_resolved_constructor(ctx, e), false);
        cg_walk_args(ctx, e->constructor_call.args,
                     e->constructor_call.args_count, out);
        return;
    case AST_ASSIGN:
        cg_walk_expr(ctx, e->assign.target, out);
        cg_walk_expr(ctx, e->assign.value, out);
        return;
    case AST_COMPOUNDASSIGN:
        cg_walk_expr(ctx, e->compound_assign.target, out);
        cg_walk_expr(ctx, e->compound_assign.value, out);
        return;
    case AST_BINARY:
        cg_walk_expr(ctx, e->binary.lhs, out);
        cg_walk_expr(ctx, e->binary.rhs, out);
        return;
    case AST_UNARY:       cg_walk_expr(ctx, e->unary.e, out); return;
    case AST_TERNARY:
        cg_walk_expr(ctx, e->ternary.test, out);
        cg_walk_expr(ctx, e->ternary.then, out);
        cg_walk_expr(ctx, e->ternary.else_, out);
        return;
    case AST_CAST:        cg_walk_expr(ctx, e->cast.e, out); return;
    case AST_INSTANCEOF:  cg_walk_expr(ctx, e->instance_of.e, out); return;
    case AST_FIELDACCESS: cg_walk_expr(ctx, e->field_access.obj, out); return;
    case AST_ARRAYACCESS:
        cg_walk_expr(ctx, e->array_access.arr, out);
        cg_walk_expr(ctx, e->array_access.index, out);
        return;
    case AST_NEWARRAY:    for (int _d = 0; _d < e->new_array.dims_count; _d++)
                              if (e->new_array.dims[_d]) cg_walk_expr(ctx, e->new_array.dims[_d], out);
                          return;
    case AST_ARRAYINIT:
        for (int i = 0; i < e->array_init.elems_count; i++)
            cg_walk_expr(ctx, e->array_init.elems[i], out);
        return;
    default: return;
    }
}

/* `prot` counts enclosing protected regions (a try's body or a finally body —
 * the AST mirror of the DDCG's rho_in_protected): a `return f()` inside one
 * does NOT lower to return_call (the tail call would pop the frame and its
 * handlers with it), so its call edge is a plain frame-growing edge. */
static void cg_walk_stmt(sema_ctx_t* ctx, const ast_stmt_t* s, cg_entry_t* out,
                         int prot) {
    if (!s) return;
    switch (s->tag) {
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            cg_walk_stmt(ctx, s->block.stmts[i], out, prot);
        return;
    case AST_EXPRSTMT:    cg_walk_expr(ctx, s->expr_stmt.e, out); return;
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++)
            cg_walk_expr(ctx, s->local_var_decl.decls[i]->init, out);
        return;
    case AST_IF:
        cg_walk_expr(ctx, s->if_.test, out);
        cg_walk_stmt(ctx, s->if_.then, out, prot);
        cg_walk_stmt(ctx, s->if_.else_, out, prot);
        return;
    case AST_WHILE:
        cg_walk_expr(ctx, s->while_.test, out);
        cg_walk_stmt(ctx, s->while_.body, out, prot);
        return;
    case AST_DOWHILE:
        cg_walk_expr(ctx, s->do_while.test, out);
        cg_walk_stmt(ctx, s->do_while.body, out, prot);
        return;
    case AST_FOR:
        cg_walk_stmt(ctx, s->for_.init, out, prot);
        cg_walk_expr(ctx, s->for_.test, out);
        for (int i = 0; i < s->for_.update_count; i++)
            cg_walk_expr(ctx, s->for_.update[i], out);
        cg_walk_stmt(ctx, s->for_.body, out, prot);
        return;
    case AST_SWITCH:
        cg_walk_expr(ctx, s->switch_.selector, out);
        for (int i = 0; i < s->switch_.cases_count; i++)
            for (int j = 0; j < s->switch_.cases[i]->stmts_count; j++)
                cg_walk_stmt(ctx, s->switch_.cases[i]->stmts[j], out, prot);
        return;
    case AST_TRY:
        cg_walk_stmt(ctx, s->try_.body, out, prot + 1);
        for (int i = 0; i < s->try_.catches_count; i++)          /* a catch body has
                left its own region — only OUTER regions still protect it */
            cg_walk_stmt(ctx, s->try_.catches[i]->body, out, prot);
        cg_walk_stmt(ctx, s->try_.finally_, out, prot + 1);
        return;
    case AST_RETURN: {
        /* Only a DIRECT `return f(...)` with an EXACT result-type match,
         * outside every protected region, lowers to return_call* (burg's
         * `stmt: Return(tail)`) — mirror precisely that shape. The call's
         * receiver and arguments are still ordinary (frame-growing) edges. */
        const ast_expr_t* v = s->return_.value;
        if (v && prot == 0
                && (v->tag == AST_METHODCALL || v->tag == AST_SUPERCALL)) {
            const sema_method_t* callee = v->tag == AST_METHODCALL
                ? sema_resolved_method(ctx, v)
                : sema_resolved_super_method(ctx, v);
            if (callee && cg_type_eq(callee->return_type, out->mret)) {
                cg_add(out, callee, true);
                if (v->tag == AST_METHODCALL) {
                    cg_walk_expr(ctx, v->method_call.obj, out);
                    cg_walk_args(ctx, v->method_call.args,
                                 v->method_call.args_count, out);
                } else {
                    cg_walk_args(ctx, v->super_call.args,
                                 v->super_call.args_count, out);
                }
                return;
            }
        }
        cg_walk_expr(ctx, s->return_.value, out);
        return;
    }
    case AST_THROW:       cg_walk_expr(ctx, s->throw_.e, out); return;
    case AST_LABELED:     cg_walk_stmt(ctx, s->labeled.body, out, prot); return;
    default: return;
    }
}

/* Reach `target` from `from` along call edges, where the path must carry at
 * least one NON-TAIL edge to count. A cycle sustained purely by tail calls
 * runs in O(1) stack — burg's `Return(tail)` emits return_call*, reusing the
 * frame — so it cannot exhaust anything and must not warn. `seen` rides the
 * visited key's low bit (method pointers are aligned), so a node is revisited
 * at most once per state. */
static bool cg_reaches_nontail(bbq_htree* graph,
                               const sema_method_t* from,
                               const sema_method_t* target, bool seen,
                               bbq_htree* visited) {
    cg_entry_t* e = (cg_entry_t*)bbq_htree_search(
        graph, (uint32_t)(uintptr_t)from);
    if (!e) return false;
    for (int t = 0; t < 2; t++) {
        const sema_method_t** list = t ? e->tail_callees : e->callees;
        bool s2 = t ? seen : true;           /* a non-tail edge sets the bit */
        for (int i = 0; i < bbq_vec_len(list); i++) {
            const sema_method_t* c = list[i];
            if (c == target && s2) return true;
            uint32_t key = (uint32_t)(uintptr_t)c | (s2 ? 1u : 0u);
            if (bbq_htree_contains(visited, key)) continue;
            bbq_htree_insert(visited, key, (void*)1);
            if (cg_reaches_nontail(graph, c, target, s2, visited)) return true;
        }
    }
    return false;
}

static bool method_in_nontail_cycle(bbq_htree* graph, const sema_method_t* m) {
    bbq_htree* visited = bbq_htree_create();
    bool in_cycle = cg_reaches_nontail(graph, m, m, false, visited);
    bbq_htree_destroy(visited);
    return in_cycle;
}

static void check_recursion(sema_ctx_t* ctx) {
    bbq_htree* graph = bbq_htree_create();
    cg_entry_t** entries = NULL;
    int ncls = bbq_vec_len(ctx->classes);

    for (int ci = 0; ci < ncls; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->import_pkg >= 0) continue;
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            ast_stmt_t* body;
            if (!method_body_stmt(m, &body)) continue;
            cg_entry_t* e = (cg_entry_t*)bbq_arena_alloc(
                ctx->arena, sizeof(*e));
            e->callees = NULL;
            e->tail_callees = NULL;
            e->mret = m->return_type;
            cg_walk_stmt(ctx, body, e, 0);
            bbq_htree_insert(graph, (uint32_t)(uintptr_t)m, e);
            bbq_vec_push(entries, e);
        }
    }

    for (int ci = 0; ci < ncls; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->import_pkg >= 0) continue;
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            if (!bbq_htree_contains(graph, (uint32_t)(uintptr_t)m)) continue;
            /* Only NON-TAIL recursion warns: a pure tail-call cycle lowers to
             * return_call* and runs in O(1) stack, so "can exhaust the call
             * stack" would simply be false there. */
            if (method_in_nontail_cycle(graph, m)) {
                sema_diag_t d = {0};
                d.level = DIAG_WARNING;
                d.kind  = SEMA_DIAG_RECURSION_CYCLE;
                d.loc = m->ast_node ? m->ast_node->loc : (ast_srcloc){0};
                snprintf(d.message, sizeof(d.message),
                    "method '%s' participates in a recursion cycle through a "
                    "non-tail call; deep recursion can exhaust the call stack",
                    m->name ? m->name : "");
                bbq_vec_push(ctx->diags, d);
            }
        }
    }

    for (int i = 0; i < bbq_vec_len(entries); i++) {
        bbq_vec_free(entries[i]->callees);
        bbq_vec_free(entries[i]->tail_callees);
    }
    bbq_vec_free(entries);
    bbq_htree_destroy(graph);
}

/* ── Purity analysis ───────────────────────────────────────────── */

static bool pur_target_writes_state(sema_ctx_t* ctx, const ast_expr_t* t) {
    if (!t) return false;
    if (t->tag == AST_FIELDACCESS || t->tag == AST_ARRAYACCESS) return true;
    if (t->tag == AST_IDENT) {
        /* Unqualified field reference inside the same class. */
        const sema_ident_info_t* info = sema_ident_kind(ctx, t);
        if (info && (info->kind == SEMA_IDENT_INSTANCE_FIELD
                     || info->kind == SEMA_IDENT_STATIC_FIELD))
            return true;
    }
    return false;
}

static bool pur_expr_impure(sema_ctx_t* ctx, const ast_expr_t* e,
                            const bbq_htree* pure_set);

static bool pur_stmt_impure(sema_ctx_t* ctx, const ast_stmt_t* s,
                            const bbq_htree* pure_set) {
    if (!s) return false;
    switch (s->tag) {
    case AST_BLOCK:
        for (int i = 0; i < s->block.stmts_count; i++)
            if (pur_stmt_impure(ctx, s->block.stmts[i], pure_set)) return true;
        return false;
    case AST_EXPRSTMT:
        return pur_expr_impure(ctx, s->expr_stmt.e, pure_set);
    case AST_LOCALVARDECL:
        for (int i = 0; i < s->local_var_decl.decls_count; i++)
            if (pur_expr_impure(ctx, s->local_var_decl.decls[i]->init, pure_set))
                return true;
        return false;
    case AST_IF:
        return pur_expr_impure(ctx, s->if_.test, pure_set)
            || pur_stmt_impure(ctx, s->if_.then, pure_set)
            || pur_stmt_impure(ctx, s->if_.else_, pure_set);
    case AST_WHILE:
        return pur_expr_impure(ctx, s->while_.test, pure_set)
            || pur_stmt_impure(ctx, s->while_.body, pure_set);
    case AST_DOWHILE:
        return pur_expr_impure(ctx, s->do_while.test, pure_set)
            || pur_stmt_impure(ctx, s->do_while.body, pure_set);
    case AST_FOR:
        if (pur_stmt_impure(ctx, s->for_.init, pure_set)) return true;
        if (pur_expr_impure(ctx, s->for_.test, pure_set)) return true;
        for (int i = 0; i < s->for_.update_count; i++)
            if (pur_expr_impure(ctx, s->for_.update[i], pure_set)) return true;
        return pur_stmt_impure(ctx, s->for_.body, pure_set);
    case AST_SWITCH:
        if (pur_expr_impure(ctx, s->switch_.selector, pure_set)) return true;
        for (int i = 0; i < s->switch_.cases_count; i++)
            for (int j = 0; j < s->switch_.cases[i]->stmts_count; j++)
                if (pur_stmt_impure(ctx, s->switch_.cases[i]->stmts[j], pure_set))
                    return true;
        return false;
    case AST_TRY:
        if (pur_stmt_impure(ctx, s->try_.body, pure_set)) return true;
        for (int i = 0; i < s->try_.catches_count; i++)
            if (pur_stmt_impure(ctx, s->try_.catches[i]->body, pure_set))
                return true;
        return pur_stmt_impure(ctx, s->try_.finally_, pure_set);
    case AST_RETURN:
        return pur_expr_impure(ctx, s->return_.value, pure_set);
    case AST_THROW:
        return true;  /* throw is an observable side effect */
    case AST_LABELED:
        return pur_stmt_impure(ctx, s->labeled.body, pure_set);
    default: return false;
    }
}

static bool pur_expr_impure(sema_ctx_t* ctx, const ast_expr_t* e,
                            const bbq_htree* pure_set) {
    if (!e) return false;
    switch (e->tag) {
    case AST_METHODCALL: {
        const sema_method_t* m = sema_resolved_method(ctx, e);
        if (!m || !bbq_htree_contains(pure_set, (uint32_t)(uintptr_t)m))
            return true;
        if (pur_expr_impure(ctx, e->method_call.obj, pure_set)) return true;
        for (int i = 0; i < e->method_call.args_count; i++)
            if (pur_expr_impure(ctx, e->method_call.args[i], pure_set))
                return true;
        return false;
    }
    case AST_NEW:
    case AST_NEWARRAY:
        return true;  /* allocation escapes via return */
    case AST_SUPERCALL:
    case AST_CONSTRUCTORCALL:
        return true;
    case AST_ASSIGN:
        if (pur_target_writes_state(ctx, e->assign.target)) return true;
        return pur_expr_impure(ctx, e->assign.target, pure_set)
            || pur_expr_impure(ctx, e->assign.value, pure_set);
    case AST_COMPOUNDASSIGN:
        if (pur_target_writes_state(ctx, e->compound_assign.target)) return true;
        return pur_expr_impure(ctx, e->compound_assign.target, pure_set)
            || pur_expr_impure(ctx, e->compound_assign.value, pure_set);
    case AST_UNARY:
        if ((e->unary.op == AST_PREINC || e->unary.op == AST_PREDEC
             || e->unary.op == AST_POSTINC || e->unary.op == AST_POSTDEC)
            && pur_target_writes_state(ctx, e->unary.e))
            return true;
        return pur_expr_impure(ctx, e->unary.e, pure_set);
    case AST_BINARY:
        return pur_expr_impure(ctx, e->binary.lhs, pure_set)
            || pur_expr_impure(ctx, e->binary.rhs, pure_set);
    case AST_TERNARY:
        return pur_expr_impure(ctx, e->ternary.test, pure_set)
            || pur_expr_impure(ctx, e->ternary.then, pure_set)
            || pur_expr_impure(ctx, e->ternary.else_, pure_set);
    case AST_CAST:       return pur_expr_impure(ctx, e->cast.e, pure_set);
    case AST_INSTANCEOF: return pur_expr_impure(ctx, e->instance_of.e, pure_set);
    case AST_FIELDACCESS:
        return pur_expr_impure(ctx, e->field_access.obj, pure_set);
    case AST_ARRAYACCESS:
        return pur_expr_impure(ctx, e->array_access.arr, pure_set)
            || pur_expr_impure(ctx, e->array_access.index, pure_set);
    case AST_ARRAYINIT:
        for (int i = 0; i < e->array_init.elems_count; i++)
            if (pur_expr_impure(ctx, e->array_init.elems[i], pure_set))
                return true;
        return false;
    default: return false;
    }
}

static void compute_pure_set(sema_ctx_t* ctx, bbq_htree* pure_set) {
    int ncls = bbq_vec_len(ctx->classes);
    /* Seed all user-defined non-constructor methods with bodies. */
    for (int ci = 0; ci < ncls; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->import_pkg >= 0) continue;
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            if (m->is_constructor) continue;
            ast_stmt_t* body;
            if (!method_body_stmt(m, &body)) continue;
            bbq_htree_insert(pure_set, (uint32_t)(uintptr_t)m, (void*)1);
        }
    }
    /* Monotone shrink until stable. */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ci = 0; ci < ncls; ci++) {
            sema_class_t* c = &ctx->classes[ci];
            if (c->import_pkg >= 0) continue;
            for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
                sema_method_t* m = &c->methods[mi];
                if (!bbq_htree_contains(pure_set, (uint32_t)(uintptr_t)m))
                    continue;
                ast_stmt_t* body;
                if (!method_body_stmt(m, &body)) continue;
                if (pur_stmt_impure(ctx, body, pure_set)) {
                    bbq_htree_delete(pure_set, (uint32_t)(uintptr_t)m);
                    changed = true;
                }
            }
        }
    }
}

/* Warn on bare call to a non-void pure method whose result is discarded. */
static void check_discarded_pure_call(sema_ctx_t* ctx, const ast_stmt_t* s,
                                       const bbq_htree* pure_set) {
    if (!s || s->tag != AST_EXPRSTMT) return;
    const ast_expr_t* e = s->expr_stmt.e;
    if (!e || e->tag != AST_METHODCALL) return;
    const sema_method_t* m = sema_resolved_method(ctx, e);
    if (!m) return;
    if (m->return_type.tag == JT_VOID) return;
    if (!bbq_htree_contains(pure_set, (uint32_t)(uintptr_t)m)) return;
    sema_diag_t d = {0};
    d.level = DIAG_WARNING;
    d.loc = s->loc;
    snprintf(d.message, sizeof(d.message),
        "call to pure method '%s' discards its return value",
        m->name ? m->name : "");
    bbq_vec_push(ctx->diags, d);
}

/* ── Wrong-override detection ──────────────────────────────────── */

static bool sig_exact_match(const sema_method_t* a, const sema_method_t* b) {
    if (!jt_eq(a->return_type, b->return_type)) return false;
    for (int i = 0; i < a->param_count; i++)
        if (!jt_eq(a->param_types[i], b->param_types[i])) return false;
    return true;
}

static void check_wrong_overrides(sema_ctx_t* ctx) {
    int ncls = bbq_vec_len(ctx->classes);
    for (int ci = 0; ci < ncls; ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->import_pkg >= 0) continue;
        if (c->super_id < 0) continue;

        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            if (m->is_constructor) continue;
            if (m->modifiers & ACC_STATIC) continue;
            if (m->modifiers & ACC_PRIVATE) continue;
            if (!m->name) continue;

            int super_id = c->super_id;
            int depth = 0;
            while (super_id >= 0 && depth++ < ncls) {
                const sema_class_t* s = &ctx->classes[super_id];
                for (int smi = 0; smi < bbq_vec_len(s->methods); smi++) {
                    const sema_method_t* sm = &s->methods[smi];
                    if (sm->is_constructor) continue;
                    if (!sm->name || strcmp(sm->name, m->name) != 0) continue;
                    if (sm->param_count != m->param_count) continue;
                    if (sig_exact_match(m, sm)) goto next_method;
                    /* Same name + arity, signature differs: suspicious. */
                    sema_diag_t d = {0};
                    d.level = DIAG_WARNING;
                    d.loc = m->ast_node ? m->ast_node->loc : (ast_srcloc){0};
                    snprintf(d.message, sizeof(d.message),
                        "method '%s' has the same name and arity as '%s.%s' "
                        "but does not override it — signature mismatch",
                        m->name, s->name ? s->name : "<anonymous>",
                        sm->name);
                    bbq_vec_push(ctx->diags, d);
                    goto next_method;
                }
                super_id = s->super_id;
            }
            next_method: ;
        }
    }
}

void analyses_run(sema_ctx_t* ctx) {
    check_wrong_overrides(ctx);
    check_recursion(ctx);
    bbq_htree* noreturn_methods = bbq_htree_create();
    compute_noreturn_set(ctx, noreturn_methods);
    bbq_htree* pure_set = bbq_htree_create();
    compute_pure_set(ctx, pure_set);

    analyses_env_t env;
    env.noreturn_methods = noreturn_methods;
    env.pure_set         = pure_set;

    for (int ci = 0; ci < bbq_vec_len(ctx->classes); ci++) {
        sema_class_t* c = &ctx->classes[ci];
        if (c->import_pkg >= 0) continue;
        for (int mi = 0; mi < bbq_vec_len(c->methods); mi++) {
            sema_method_t* m = &c->methods[mi];
            ast_stmt_t* body;
            if (!method_body_stmt(m, &body)) continue;
            run_on_method(ctx, m, body, &env);
        }
    }
    bbq_htree_destroy(pure_set);
    bbq_htree_destroy(noreturn_methods);
}
