/* codegen_structured.c — destination-driven structured WASM emit over the SIR.
 *
 * The SIR is already the output of destination-driven codegen (the DDCG = Dybvig
 * et al.'s CG function); this maps its control structure to WASM *structured*
 * control (block/loop/if/br) rather than the gotos the original targets. Fig. 5
 * of "Destination-Driven Code Generation" gives the shapes:
 *   while → (block $break (loop $continue …))   if → if/else/end
 *   sequence → fall-through                      return → return
 * and CGjump's rule gives the transfer: target is the next node → emit nothing;
 * return → return; else → br <depth> off the scope stack.
 *
 * Value subtrees (conds, store values) are tiled by the burg via burg_rewrite
 * (the value-as-statement chains make it emit just the value bytes). Plain spine
 * nodes are emitted one at a time through the burg (detach .next so burg_rewrite
 * sees a single node). The loop/if framing + br-depths are this file's job; the
 * loop/break/br machinery lands incrementally — `if` first. */
#include "javelina/compiler/codegen_method.h"
#include "javelina/compiler/compiler.h"    /* compiler_fact_t — the sidecar */
#include "javelina/compiler/wasm_types.h"  /* burg_ctx_t.types → sema (method result arity) */
#include "javelina/compiler/type_lattice.h" /* lat_value_class — catch-type → struct typeidx */
#include "bbq_vec.h"
#include "bbq_htree.h"   /* the per-method scope index (scope_env_t) */
#include <stdlib.h>      /* malloc/free — scope_env_t's chain array */
#include <stdio.h>       /* the depth-bound invariant report */

/* Empty (void) block type for block/loop/if (§5.3.6 / §5.4.1). */
#define WBT_VOID 0x40

/* The structurer reads SCOPE rows out of the sidecar: `key` = the header, `aux` =
 * the exit label, `a` = the compiler_scope_kind_t. It frames a wasm block/loop from
 * what the DDCG RECORDED as it built the statement — it never rediscovers a loop.
 * (Rows of other kinds — guards, allocs, throw regions — belong to other stages;
 * each stage reads the kinds it owns.) */
/* The per-method scope table plus its BY-KEY index. emit_spine queries by header
 * node at every step; the linear scan this replaces was 17.5% of the -O0 jre build
 * (callgrind, 07-31 — the 07-13 kind-filter took the same number from ~8% down
 * once, and the corpus grew past it again). The index is DERIVED from the one
 * table at build time and nothing writes through it: `head` maps a key pointer
 * (32-bit-hashed) to its first row, `next[i]` chains rows sharing that hash in
 * ascending sidecar order. Hash collisions merge chains harmlessly — every reader
 * still filters rows by the FULL `key == node` compare, so a merged chain costs a
 * wasted hop, never a wrong row. */
typedef struct {
    const compiler_fact_t* f;    /* the SCOPE rows, sidecar order */
    int                    n;
    bbq_htree*             head; /* ptr_key(fact.key) -> first row index + 1 */
    int*                   next; /* row -> next row with the same key hash, or -1 */
    /* The scope stack itself — the enclosing block/loop labels, innermost last.
     * It lives HERE rather than in a caller's array because it GROWS: a fixed
     * bound silently stopped pushing once full, br_depth could then no longer
     * find the target, and emit_spine re-emitted the target region inline
     * instead of branching to it — a lost loop back edge, or an unbounded
     * re-emission cycle. Holding it in the env that every frame already carries
     * means a reallocation is visible to all of them; an array passed by value
     * would leave outer frames pointing at freed memory. */
    sir_node_t**           stack;
} scope_env_t;

static uint32_t se_ptr_key(const void* p) {
    uint32_t k = (uint32_t)(uintptr_t)p;
    return k ? k : 1;                       /* htree disallows key 0 */
}

static void scope_env_build(scope_env_t* se, const compiler_fact_t* f, int n) {
    se->f = f; se->n = n;
    se->stack = NULL;
    se->head = bbq_htree_create();
    se->next = n ? (int*)malloc((size_t)n * sizeof(int)) : NULL;
    for (int i = n - 1; i >= 0; i--) {      /* descending, so chains ascend */
        uint32_t k = se_ptr_key(f[i].key);
        void* h = bbq_htree_search(se->head, k);
        se->next[i] = h ? (int)(intptr_t)h - 1 : -1;
        bbq_htree_insert(se->head, k, (void*)(intptr_t)(i + 1));
    }
}

static void scope_env_free(scope_env_t* se) {
    bbq_htree_destroy(se->head);
    bbq_vec_free(se->stack);
    free(se->next);
}

static const compiler_fact_t* scope_at(const scope_env_t* se,
                                       const sir_node_t* node, int scope_kind) {
    void* h = bbq_htree_search(se->head, se_ptr_key(node));
    for (int i = h ? (int)(intptr_t)h - 1 : -1; i >= 0; i = se->next[i])
        if (se->f[i].kind == COMPILER_FACT_SCOPE && se->f[i].a == scope_kind
                && se->f[i].key == node)
            return &se->f[i];
    return NULL;
}

/* Make room for `k` more frames at depth `sd`. Always succeeds — the stack grows
 * to whatever the method needs. There is no cap to exceed and so no case where
 * framing is quietly skipped. */
static void scope_room(scope_env_t* se, int sd, int k) {
    while ((int)bbq_vec_len(se->stack) < sd + k)
        bbq_vec_push(se->stack, (sir_node_t*)NULL);
}

/* Is `node` the merge anchor (Ljoin) of some recorded if-BLOCK scope? */
static const compiler_fact_t* block_scope_at(const scope_env_t* se,
                                             const sir_node_t* node) {
    return scope_at(se, node, COMPILER_SCOPE_BLOCK);
}

/* Is `node` a loop header (Ltop) of some recorded LOOP scope? */
static const compiler_fact_t* loop_scope_at(const scope_env_t* se,
                                            const sir_node_t* node) {
    return scope_at(se, node, COMPILER_SCOPE_LOOP);
}

/* The switch scope (exit = Lbreak) keyed by the Switch node, or NULL. */
static const compiler_fact_t* switch_scope_at(const scope_env_t* se,
                                              const sir_node_t* node) {
    return scope_at(se, node, COMPILER_SCOPE_SWITCH);
}


/* WASM relative br-depth of `target` in the scope stack (innermost = depth 0),
 * or -1 if `target` is not an enclosing scope label. This is CGjump's "is the
 * continuation a label?" test. */
/* Backstop for the forwarding-Nop walk below — a termination guard against a
 * CYCLIC chain (which would be a ddcg defect), not a bound on program shape. The
 * scope stack itself has no bound — it grows; see scope_room. */
#define NOP_FORWARD_GUARD 4096
static int br_depth(const scope_env_t* se, int sd, const sir_node_t* target) {
    /* direct: target IS an enclosing scope label. */
    for (int i = sd - 1; i >= 0; i--)
        if (se->stack[i] == target) return sd - 1 - i;
    /* CGjump inheritance (Dybvig et al. §3): if the target is a forwarding
     * landing-pad — a Nop that does nothing but redirect to its continuation —
     * thread THROUGH it to the ultimate enclosing scope, so a break inherits the
     * outer construct's control destination and jumps once rather than jump-to-a-
     * jump. (Only fires when the target is NOT itself framed on the stack, so
     * framed scopes are unaffected.) */
    int hops = 0;
    for (const sir_node_t* t = target; t && t->tag == SIR_NOP && hops < NOP_FORWARD_GUARD; hops++) {
        t = sir_get_next(t);
        for (int i = sd - 1; i >= 0; i--)
            if (se->stack[i] == t) return sd - 1 - i;
    }
    return -1;
}

/* Emit a value subtree's bytes (leaves the value on the operand stack). The
 * value-as-statement chains let burg_rewrite tile a bare value toward stmt with
 * no extra bytes. */
static void emit_value(sir_node_t* v, burg_ctx_t* ctx) {
    burg_rewrite(v, ctx);
}

/* Emit a branch CONDITION's bytes at the polarity the site wants.
 *
 * A condition carries a context a value does not: it is about to be consumed as
 * a truth test, so the matcher is asked for a condition GOAL by name rather than
 * tiled through the value chain. `cond` emits the condition's truth, `ncond` its
 * inverse, and both are real covers the DP has priced — so a site that wants the
 * inverse asks for it instead of emitting an i32.eqz over the truth, and a site
 * that wants the truth asks for that. Which goal was reduced IS the answer;
 * nothing travels out of band, and no state survives the call.
 *
 * `want_truth` says which polarity the caller needs. Returns true if the caller
 * must emit ONE i32.eqz to get it — that happens only when the opposite goal is
 * strictly cheaper even after paying for the inversion, which is a decision made
 * on real bytes by comparing the two costs the labeller computed.
 *
 * This is the same shape as `stmt: Return(tail)` choosing between return_call and
 * call/return: a context alternative expressed as a goal and settled by cost. */
static bool emit_branch_cond(sir_node_t* c, bool want_truth, burg_ctx_t* ctx) {
    burg_state_t* st = burg_label_root(c, ctx);
    if (!st) {
        burg_set_error("burg: no cover at a branch condition", (int)c->tag, ctx);
        return false;
    }
    int want = want_truth ? cond_NT : ncond_NT;
    int other = want_truth ? ncond_NT : cond_NT;
    int cw = burg_rule(st, want)  ? burg_cost(st, want)  : BURG_MAX_COST;
    int co = burg_rule(st, other) ? burg_cost(st, other) : BURG_MAX_COST;
    if (cw == BURG_MAX_COST && co == BURG_MAX_COST) {
        burg_set_error("burg: no cond cover at a branch condition", (int)c->tag, ctx);
        return false;
    }
    /* Ties go to the goal that needs no extra instruction: `co + 1` must be
     * STRICTLY cheaper to be worth inverting afterwards. */
    bool invert = co != BURG_MAX_COST && co + 1 < cw;
    burg_reduce(c, st, invert ? other : want, ctx);
    return invert;
}

/* Emit one spine node's own bytes via the burg, WITHOUT walking its successor
 * (temporarily detach .next so burg_rewrite reduces just this node). */
static void emit_node_only(sir_node_t* n, burg_ctx_t* ctx) {
    sir_node_t* saved = sir_get_next(n);
    if (saved) sir_set_next(n, NULL);
    burg_rewrite(n, ctx);
    if (saved) sir_set_next(n, saved);
}

static bool is_terminator(const sir_node_t* n) {
    return n->tag == SIR_RETURN || n->tag == SIR_RETURNVOID || n->tag == SIR_THROW;
}

/* CGjump for a plain continuation edge: if `cont` is an enclosing scope label,
 * emit `br <depth>` and return NULL (control transferred); otherwise fall through
 * (return `cont` to emit next). */
static sir_node_t* transfer(sir_node_t* cont, const scope_env_t* se, int sd,
                            burg_ctx_t* ctx) {
    int d = br_depth(se, sd, cont);
    if (d >= 0) { ew_emit(&ctx->emit, WOP_BR); ew_u32(&ctx->emit, (uint32_t)d); return NULL; }
    return cont;
}

/* Step a spine node's continuation. If it is this region's own boundary
 * (`stop` — e.g. an if-else arm's continuation IS the shared Ljoin the arms
 * inherit), fall through to it: the paper's goto-to-the-next-label is a
 * fall-through into the structured `end`, not a jump. Only past the boundary
 * does br_depth's multi-hop apply (a back-edge / inherited enclosing scope);
 * without this guard it would carry an arm's join-Goto on PAST the merge to an
 * enclosing loop header, duplicating the back-edge into each arm. */
static sir_node_t* advance(sir_node_t* cont, sir_node_t* stop,
                           const scope_env_t* se, int sd, burg_ctx_t* ctx) {
    return (cont == stop) ? stop : transfer(cont, se, sd, ctx);
}

/* Read-only JLS §14.21 completion. From `node`, can `target` be reached by
 * fall-through — some path reaching it rather than terminating or diverting to a
 * `sink` (a framed merge the emitted code `br`s to)? Used to frame the if-join
 * ONLY when it is a real label: the fall-through arm reaches it and must `br` over
 * the else. When every arm terminates, the join is referenced by nothing and (per
 * the paper) gets no block. Conservative — returns true unless it can PROVE the
 * region cannot reach `target` (a redundant frame is valid; a missing one would
 * miscompile), so unmodelled shapes (switch/try, fuel exhaustion) frame the join. */
static bool region_reaches(sir_node_t* node, const sir_node_t* target,
                           sir_node_t* const* sinks, int nsinks,
                           const scope_env_t* se, int fuel) {
    while (node) {
        if (node == target) return true;
        for (int i = 0; i < nsinks; i++) if (node == sinks[i]) return false;  /* br'd to a merge */
        if (--fuel < 0) return true;                          /* conservative */
        if (is_terminator(node)) return false;
        if (node->tag == SIR_BRANCH)
            return region_reaches(node->branch.on_true, target, sinks, nsinks, se, fuel) ||
                   region_reaches(node->branch.on_false, target, sinks, nsinks, se, fuel);
        if (node->tag == SIR_SWITCH || node->tag == SIR_TRYREGION)
            return true;                                       /* can complete — frame the join */
        const compiler_fact_t* lp = loop_scope_at(se, node);
        if (lp) { node = lp->aux; continue; }                 /* a loop exits at Lbreak */
        node = sir_get_next(node);
    }
    return true;                                               /* ran off the end unproven — conservative */
}

/* Emit the source-order typed-catch `if`-chain for a try region (defined below;
 * mutually recursive with emit_spine via the catch handler bodies). */
static void emit_typed_catches(sir_node_t* tr, int ex_tmp, sir_node_t* ljoin,
                               int sd, scope_env_t* se, burg_ctx_t* ctx);

/* Emit the spine from `node` up to (not including) `stop`, with `se->stack[0..sd)`
 * the enclosing block/loop labels (innermost last). Returns true if emission
 * reached `stop` by FALL-THROUGH (control continues into stop), false if every
 * path branched away first (node became NULL — a back-edge / return / inherited
 * br). The loop framing needs this: a while exits its body by branching out (no
 * fall-through past `end`), but a do-while's tail test falls through on false. */
static bool emit_spine(sir_node_t* node, sir_node_t* stop, int sd,
                       scope_env_t* se, burg_ctx_t* ctx,
                       bool* wasm_live) {
    /* `left`: did control leave this region (SIR sense — return/throw/br)? Drives
     * the fall-through return value callers read. `live`: is WASM control live
     * past this region's last byte? It diverges from !left only when a void
     * if/switch/try completes with all arms terminated — SIR-dead, yet the §7.6
     * validator resets control to *reachable* after every `end`. (Reported via
     * `wasm_live` for the function epilogue; NULL when a caller doesn't need it.) */
    bool left = false;
    bool live = true;
    /* The if-join the ddcg recorded for the construct we are currently inside, carried from
     * the node it is KEYED ON to the Branch that consumes it.
     *
     * `record_scope(test, Ljoin, 0)` keys the join on the head of the CONDITION's code. When
     * the condition spills — a field read, an array length, anything needing a temp — that
     * head is a StoreLocal, and by the time the walk reaches the SIR_BRANCH the lookup by
     * node finds nothing. `ljoin` then fell back to `stop`, i.e. the end of the enclosing
     * region, so BOTH arms were emitted all the way to it and the whole tail of the method
     * was duplicated into each arm — once per such if, so 2^k. Measured on Graph.los: 130
     * branches lost their anchor and were re-emitted 5599 times, overflowing javelinac's own
     * C stack; the same shape also nests wasm control frames past the validator's depth cap.
     *
     * Reading the recorded fact and carrying it forward is what the sidecar is for. The
     * alternative — recovering the join by walking the graph to find where the arms converge
     * — is the merge-rediscovery this design exists to avoid. */
    sir_node_t* pending_join = NULL;
    while (node && node != stop) {
        /* Emit-once merge labels (docs/ddcg-merge-labels.md §2.2). A SIR node a
         * branch's transfers converge on is a label whose code must be emitted ONCE;
         * every other reference is a br. The ddcg records these (COMPILER_SCOPE_MERGE,
         * the if-join as BLOCK) keyed by the construct HEAD — which for a spilled
         * condition (instanceof, a complex operand) is the spill StoreLocal, not the
         * SIR_BRANCH — so the check must run for ANY node, at the top of the walk.
         * Collect this head's MERGE labels in REVERSE sidecar order (inner-first
         * records → outer→inner); if any survive, prepend the if-join as the outermost
         * bound. Frame each as a block, emit the subtree bounded at the innermost, then
         * close inner→outer emitting each label's code once. On re-entry the labels sit
         * on the scope stack, so this collection is empty and the walk proceeds through
         * the ordinary paths below (self-stabilizing). */
        {
            const compiler_fact_t* mjs = block_scope_at(se, node);
            sir_node_t* mjoin = (mjs && mjs->aux && mjs->aux != stop &&
                                 br_depth(se, sd, mjs->aux) < 0) ? mjs->aux : NULL;
            /* EVERY merge label keyed on this node — the count is a property of the
             * method, not of a constant. Truncating here dropped a br target, and a
             * dropped target is re-emitted inline rather than branched to. */
            sir_node_t** mrg = NULL;
            /* The index chain ascends in sidecar order; the scan this replaces ran
             * BACKWARD (nsc-1 … 0), and that order is load-bearing — records are
             * inner-first, so backward = outer→inner, the framing order. Collect
             * ascending, then process in reverse. */
            {
                int* rows = NULL;
                void* h = bbq_htree_search(se->head, se_ptr_key(node));
                for (int i = h ? (int)(intptr_t)h - 1 : -1; i >= 0; i = se->next[i])
                    if (se->f[i].kind == COMPILER_FACT_SCOPE
                            && se->f[i].a == COMPILER_SCOPE_MERGE && se->f[i].key == node)
                        bbq_vec_push(rows, i);
                for (int r = (int)bbq_vec_len(rows) - 1; r >= 0; r--) {
                    sir_node_t* X = se->f[rows[r]].aux;
                    if (X && X != stop && X != mjoin && br_depth(se, sd, X) < 0)
                        bbq_vec_push(mrg, X);
                }
                bbq_vec_free(rows);
            }
            int nm = (int)bbq_vec_len(mrg);
            /* The if-join is framed only when it is an actual label — the fall-through
             * arm reaches it (over the else). If every arm terminates, nothing brs to
             * it: drop it (docs/ddcg-merge-labels.md §2.2; the merges `mrg` are the
             * branches' br targets, i.e. the sinks the fall-through path diverts to). */
            if (mjoin && nm > 0 && !region_reaches(node, mjoin, mrg, nm, se, 1 << 20))
                mjoin = NULL;
            if (nm > 0) {
                sir_node_t** bounds = NULL;
                if (mjoin) bbq_vec_push(bounds, mjoin);                  /* outermost */
                for (int i = 0; i < nm; i++) bbq_vec_push(bounds, mrg[i]);  /* outer→inner */
                int nb = (int)bbq_vec_len(bounds);
                {
                    scope_room(se, sd, nb);
                    int base = sd;
                    for (int i = 0; i < nb; i++) {
                        ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID);
                        se->stack[sd++] = bounds[i];
                    }
                    emit_spine(node, bounds[nb - 1],sd, se, ctx, NULL);
                    for (int i = nb - 1; i >= 1; i--) {           /* close inner→outer */
                        ew_byte(&ctx->emit, W_END); sd--;
                        emit_spine(bounds[i], bounds[i - 1],sd, se, ctx, NULL);
                    }
                    ew_byte(&ctx->emit, W_END); sd = base;        /* close outermost */
                    node = bounds[0];
                    bbq_vec_free(bounds); bbq_vec_free(mrg);
                    continue;
                }
                bbq_vec_free(bounds);
            }
            bbq_vec_free(mrg);
            /* Not framed here (no merge labels, or no room): remember the join so the Branch
             * ending this condition still finds it. `mjoin` is already NULL when the join sits
             * on the scope stack — there the branch resolves it by br_depth — or when it IS
             * `stop`, where the fallback is right anyway. */
            if (mjoin) pending_join = mjoin;
        }
        const compiler_fact_t* loop = loop_scope_at(se, node);
        if (loop && (scope_room(se, sd, 2), true)) {
            /* Dybvig Fig.5 while = loop(if T B break): block $break (loop $top …).
             * The loop body is node->next; the back-edge to `node` (Ltop) and
             * breaks to `exit` (Lbreak) become br off the scope stack. BUT if the
             * loop's exit forwards directly to an ENCLOSING scope (the loop is in
             * tail position of an outer construct — the paper's nested-while case),
             * inherit that control destination: frame ONLY the loop, route its
             * exits straight to the enclosing scope (br_depth threads through), and
             * emit no $break wrapper — one jump, not jump-to-a-jump. */
            if (!loop->aux) {
                /* §14.19: the loop cannot complete normally — no break exits it and there is no
                 * fall-through. It has no exit label, so there is nothing to frame a $break block
                 * around and nothing follows the loop. */
                ew_emit(&ctx->emit, WOP_LOOP); ew_byte(&ctx->emit, WBT_VOID);
                se->stack[sd] = node;           /* continue target (Ltop) → depth 0 */
                emit_spine(sir_get_next(node), NULL,sd + 1, se, ctx, NULL);
                ew_byte(&ctx->emit, W_END); /* loop end */
                node = NULL; left = true;
            } else if (br_depth(se, sd, loop->aux) >= 0) {
                ew_emit(&ctx->emit, WOP_LOOP); ew_byte(&ctx->emit, WBT_VOID);
                se->stack[sd] = node;           /* continue target (Ltop) → depth 0 */
                bool ft = emit_spine(sir_get_next(node), loop->aux,sd + 1, se, ctx, NULL);
                ew_byte(&ctx->emit, W_END); /* loop end */
                /* A while's exits branched out (ft=false → post-loop unreachable).
                 * A do-while's tail test falls through on false (ft=true): br that
                 * fall-through ONCE to the inherited enclosing scope here (loop->aux
                 * IS that scope — often an outer loop header, so handing it back as a
                 * node would re-frame the outer loop; transfer brs it instead). */
                node = ft ? transfer(loop->aux, se, sd, ctx) : NULL;
                if (!node) left = true;
            } else {
                ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID);
                ew_emit(&ctx->emit, WOP_LOOP);  ew_byte(&ctx->emit, WBT_VOID);
                se->stack[sd]     = loop->aux;     /* break target → depth 1 from inside */
                se->stack[sd + 1] = node;           /* continue target (Ltop) → depth 0   */
                emit_spine(sir_get_next(node), loop->aux,sd + 2, se, ctx, NULL);
                ew_byte(&ctx->emit, W_END);     /* loop end  */
                ew_byte(&ctx->emit, W_END);     /* block end */
                node = loop->aux;
            }
        } else if (node->tag == SIR_BRANCH) {
            /* Merge labels were framed at the top of the walk (any head, incl. a
             * spilled condition's StoreLocal), so on arrival here they resolve on the
             * scope stack and the branch emits through the transfer paths below. */
            const compiler_fact_t* js = block_scope_at(se, node);
            sir_node_t* carried = pending_join;   /* this branch ends the condition it belongs to */
            pending_join = NULL;
            sir_node_t* cond = node->branch.cond;
            sir_node_t* on_t = node->branch.on_true;
            sir_node_t* on_f = node->branch.on_false;
            int df = br_depth(se, sd, on_f);
            int dt = br_depth(se, sd, on_t);
            if (dt >= 0 && loop_scope_at(se, on_t)) {
                /* back-edge test: on_true is an enclosing loop HEADER — the
                 * do-while TAIL test (Branch(test, Ltop, Lbreak)) or an
                 * `if (c) continue;`. br_if to the header (loop back), fall
                 * through to the false (exit/next) arm. Must precede the df
                 * break-on-false reading: that would set node=Ltop and the
                 * loop header would be re-entered as a node and re-framed. */
                /* br_if branches when the condition is TRUE and the back-edge
                 * target is fixed, so this site needs the truth. It gets it from
                 * `cond`, unless `ncond` plus one i32.eqz is strictly cheaper. */
                if (emit_branch_cond(cond, true, ctx)) ew_emit(&ctx->emit, WOP_I32_EQZ);
                ew_emit(&ctx->emit, WOP_BR_IF); ew_u32(&ctx->emit, (uint32_t)dt);
                node = on_f;
            } else if (df >= 0) {
                /* transfer on FALSE (loop test, or a framed merge label): branch
                 * away when the condition is false, so this site wants the
                 * INVERSE — and now asks for it. There is no hardcoded i32.eqz
                 * here any more: inverting is `ncond`'s job and its price is in
                 * the grammar, so the DP decides whether to invert the operand or
                 * the result. */
                if (emit_branch_cond(cond, false, ctx)) ew_emit(&ctx->emit, WOP_I32_EQZ);
                ew_emit(&ctx->emit, WOP_BR_IF); ew_u32(&ctx->emit, (uint32_t)df);
                node = on_t;
            } else if (dt >= 0) {
                /* transfer on TRUE: cond; br_if; fall to the other arm. Same
                 * polarity need as the back-edge case above. */
                if (emit_branch_cond(cond, true, ctx)) ew_emit(&ctx->emit, WOP_I32_EQZ);
                ew_emit(&ctx->emit, WOP_BR_IF); ew_u32(&ctx->emit, (uint32_t)dt);
                node = on_f;
            } else {
                /* Plain if / ternary: a single-Branch test, both arms inheriting
                 * the recorded Ljoin (Dybvig Fig.5). Native WASM if/else/end whose
                 * `end` is the join; one-armed if (false arm IS the join) emits no
                 * else. Compound short-circuit conditions were framed above, so the
                 * condition here is a single Branch. */
                sir_node_t* ljoin = js ? js->aux
                                  : carried ? carried
                                  : stop;
                /* The `if` is the consumer that can take an inversion for nothing:
                 * a negated condition is the same construct with its two arms
                 * exchanged. Everything else about the frame is untouched — same
                 * block type, same scope push, same inherited join.
                 *
                 * "For nothing" has one exception, and it decides whether the
                 * exchange is even offered. WASM's `if` has no else-only form, so
                 * exchanging the arms of a ONE-ARMED if (the false arm IS the
                 * join) does not remove work — it moves the body into an `else`
                 * and leaves an empty then, trading a saved i32.eqz for the 0x05
                 * it now needs: same bytes, a shape nobody wants, a dead block for
                 * the validator to walk. So a one-armed if asks for the truth and
                 * takes source order, full stop. Where BOTH arms are real the
                 * exchange is free, so the site asks for `ncond` when that is
                 * strictly cheaper and swaps — the DP prices the choice, and a tie
                 * keeps source order. (When the TRUE arm is the join, `on_f !=
                 * ljoin` holds and swapping additionally removes the empty arm the
                 * un-swapped form would have emitted.) */
                bool two_armed = (on_f != ljoin);
                bool swap = false;
                if (two_armed) {
                    burg_state_t* st = burg_label_root(cond, ctx);
                    int cc = (st && burg_rule(st, cond_NT))  ? burg_cost(st, cond_NT)  : BURG_MAX_COST;
                    int cn = (st && burg_rule(st, ncond_NT)) ? burg_cost(st, ncond_NT) : BURG_MAX_COST;
                    swap = (cn < cc);
                    if (!st || (cc == BURG_MAX_COST && cn == BURG_MAX_COST))
                        burg_set_error("burg: no cond cover at a branch condition",
                                       (int)cond->tag, ctx);
                    else
                        burg_reduce(cond, st, swap ? ncond_NT : cond_NT, ctx);
                } else if (emit_branch_cond(cond, true, ctx)) {
                    ew_emit(&ctx->emit, WOP_I32_EQZ);
                }
                sir_node_t* arm_t = swap ? on_f : on_t;  /* runs when the emitted value is true  */
                sir_node_t* arm_f = swap ? on_t : on_f;  /* runs when it is false                */
                ew_emit(&ctx->emit, WOP_IF); ew_byte(&ctx->emit, WBT_VOID);
                /* The WASM `if` is itself a control frame: a br out of an arm (a
                 * continue/break to an enclosing loop/block) must count it. Push the
                 * test node as the frame's (unaddressed) label and emit the arms at
                 * sd+1 so those br-depths are right. Nothing targets the if itself —
                 * the arms inherit ljoin, which sits at sd. */
                int isd = sd;
                scope_room(se, sd, 1); se->stack[sd] = node; isd = sd + 1;
                emit_spine(arm_t, ljoin,isd, se, ctx, NULL);
                if (arm_f != ljoin) {        /* if-no-else: the false path IS the join */
                    ew_byte(&ctx->emit, W_ELSE);
                    emit_spine(arm_f, ljoin,isd, se, ctx, NULL);
                }
                ew_byte(&ctx->emit, W_END);
                /* §14.19: the frontend built a join anchor iff the if statement can complete
                 * normally. A NULL join means both arms completed abruptly and nothing follows —
                 * no need to rediscover that by watching whether the spines fell through. (WASM
                 * treats control after the `end` as reachable, so a non-void function end is
                 * still capped by the epilogue's `unreachable`.) */
                node = ljoin;
                if (!node) left = true;
            }
        } else if (node->tag == SIR_SWITCH &&
                   switch_scope_at(se, node) &&
                   (scope_room(se, sd, node->switch_.case_targets_count + 2), true)) {
            /* Dybvig switch → WASM stacked-block br_table. Blocks (outer→inner):
             * $break, $default, $case[nc-1..0]. The br_table (innermost) sends a
             * 0-based selector value i → depth i (case i's body), and any gap or
             * out-of-range value → depth nc (default). Java fall-through is the
             * natural fall from one case body into the next. (Dense/contiguous
             * case values; sparse tables are a later refinement.) */
            const compiler_fact_t* sw = switch_scope_at(se, node);
            int nc = node->switch_.case_targets_count;
            sir_node_t* lbreak = sw->aux;
            sir_node_t* def = node->switch_.default_target;
            /* Inherit the control destination when the switch is in tail position:
             * its $break forwards to an enclosing scope AND a real default landing
             * pad carries the fall-through exit. Then omit the $break wrapper —
             * every case break and the default exit multi-hop straight to the
             * enclosing scope. One jump per arm, not jump-to-the-$break-trampoline-
             * then-jump (the same redundancy the loop case elides; verified the
             * $break here is a pure collect-and-forward block). A switch with no
             * default landing (def == lbreak: fall-through IS the exit) keeps the
             * wrapper — there's no Nop to carry that exit.
             *
             * §14.19: `lbreak` is NULL when the switch cannot complete normally — no break exits
             * it, its last group completes abruptly, and it has a `default:` label (so `def` is a
             * real landing, never lbreak). Nothing arrives at the exit, so there is no $break
             * block to frame and nothing follows the switch. */
            bool has_exit = lbreak != NULL;
            bool inherit = has_exit && br_depth(se, sd, lbreak) >= 0 && def != lbreak;
            int sd0 = sd;
            if (has_exit && !inherit) { ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID); se->stack[sd++] = lbreak; }
            ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID); se->stack[sd++] = def;
            for (int i = nc - 1; i >= 0; i--) {
                ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID);
                se->stack[sd++] = node->switch_.case_targets[i];
            }
            emit_value(node->switch_.selector, ctx);
            /* case_values are sorted; pad the br_table over the whole [lo..hi]
             * span so gaps (sparse switches) route to the default. A value v
             * maps to its case's depth, or to nc (default) if v is not a case
             * or out of range. (Extreme sparsity → a giant table; an if-else
             * chain below a density threshold is a size refinement.) */
            int32_t lo = node->switch_.case_values[0];
            int32_t hi = node->switch_.case_values[nc - 1];
            if (lo != 0) {
                ew_emit(&ctx->emit, WOP_I32_CONST); ew_i32(&ctx->emit, lo);
                ew_emit(&ctx->emit, WOP_I32_SUB);
            }
            int64_t span = (int64_t)hi - (int64_t)lo + 1;
            ew_emit(&ctx->emit, WOP_BR_TABLE); ew_u32(&ctx->emit, (uint32_t)span);
            for (int64_t v = lo; v <= hi; v++) {
                int ci = -1;
                for (int i = 0; i < nc; i++)
                    if (node->switch_.case_values[i] == v) { ci = i; break; }
                ew_u32(&ctx->emit, (uint32_t)(ci >= 0 ? ci : nc));
            }
            ew_u32(&ctx->emit, (uint32_t)nc);          /* default → depth nc */
            for (int i = 0; i < nc; i++) {
                ew_byte(&ctx->emit, W_END); sd--;       /* close $case[i] */
                sir_node_t* stop = (i + 1 < nc) ? node->switch_.case_targets[i + 1] : def;
                emit_spine(node->switch_.case_targets[i], stop,sd, se, ctx, NULL);
            }
            ew_byte(&ctx->emit, W_END); sd--;           /* close $default */
            emit_spine(def, lbreak,sd, se, ctx, NULL);
            if (has_exit && !inherit) ew_byte(&ctx->emit, W_END);   /* close $break */
            sd = sd0;
            /* Continue at the switch exit. For the inherited (no-$break) case
             * the default falls through to here (advance elided its Goto at
             * the stop) and the single post-switch transfer brs it to the
             * enclosing scope — one back-edge, shared, not one per arm. */
            node = lbreak;
            if (!node) left = true;
        } else if (node->tag == SIR_TRYREGION && (scope_room(se, sd, 3), true)) {
            /* try/catch/finally → WASM try_table. Frame: block $after (the join),
             * block $handler; the try_table (catch $jexn → $handler, depth 0) wraps
             * the try body. Normal completion brs to $after; on a caught exception
             * the tag payload (the Throwable ref) lands at $handler's end, where the
             * DISPATCH runs: store the exn into the landing slot, then test each
             * TYPED catch in source order (ref.test → ref.cast → store catch var →
             * handler body), falling through to the catch-all body (frontend-inlined
             * finally → Throw = rethrow on no match). The TryRegion chain is built
             * outermost-first as TryRegion(catch-all, …TryRegion(typed_i, body)); the
             * chained typed regions are NOT emitted as nested try_tables — one
             * try_table wraps the real body found past the whole chain. */
            sir_node_t* catchall = node->try_region.handler;       /* ExceptionEntry (Throwable sentinel) */
            sir_node_t* tbody    = node->try_region.next;          /* skip the chained typed regions to the body */
            while (tbody->tag == SIR_TRYREGION) tbody = tbody->try_region.next;
            int ex_tmp = catchall->exception_entry.local_slot;     /* the Throwable landing slot */
            /* The try's join is the recorded l_join the normal-exit and catch
             * bodies inherit — READ it (keyed by the outermost try node), as the
             * if reads its Ljoin. A chained inner catch region isn't recorded; it
             * inherits the SAME join via the region `stop`. */
            const compiler_fact_t* js = block_scope_at(se, node);
            sir_node_t* ljoin   = js ? js->aux : stop;
            /* Tail-position inheritance (as loops/switch): when the join forwards
             * to an enclosing scope, omit the $after wrapper — normal completion
             * and the handler exit br straight there, no collect-and-forward
             * trampoline. (Not when ljoin is NULL: a finally catch-all whose
             * handler re-throws never reaches a join.) */
            bool inherit = ljoin && br_depth(se, sd, ljoin) >= 0;
            int sd0 = sd;
            if (!inherit) { ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID); se->stack[sd++] = ljoin; }
            /* $handler's block type has RESULT (ref null Throwable): a `catch tag $l`
             * branches to $l carrying the tag's params, so the catch target's result
             * type must equal the tag's param type. The exn lands as the block result
             * at $handler's end, where the dispatch picks it up. */
            int32_t thrty = wasm_types_class_typeidx(ctx->types,
                lat_handler_landing_class(ctx->types->sema,
                                          catchall->exception_entry.catch_class_id));
            ew_emit(&ctx->emit, WOP_BLOCK); wasm_types_emit_ref(&ctx->emit, thrty); se->stack[sd++] = catchall;
            ew_emit(&ctx->emit, WOP_TRY_TABLE); ew_byte(&ctx->emit, WBT_VOID);
            ew_u32(&ctx->emit, 1);            /* one catch clause          */
            ew_byte(&ctx->emit, 0x00);        /* kind: catch <tag> <label> */
            ew_u32(&ctx->emit, 0);            /* tag 0 = $jexn             */
            ew_u32(&ctx->emit, 0);            /* label 0 = $handler        */
            se->stack[sd++] = node;               /* the try_table is itself a control level */
            emit_spine(tbody, ljoin,sd, se, ctx, NULL);
            sd--;                             /* pop the try_table level   */
            ew_byte(&ctx->emit, W_END);       /* try_table end             */
            /* normal completion → the join. Inherited: br straight to the
             * enclosing scope; framed: br to the $after block (over $handler). */
            if (inherit) transfer(ljoin, se, sd, ctx);
            else { ew_emit(&ctx->emit, WOP_BR); ew_u32(&ctx->emit, 1); }
            ew_byte(&ctx->emit, W_END); sd--; /* $handler end ← caught exn on stack */
            /* Dispatch: store the caught Throwable into the landing slot, run the
             * source-order typed-catch if-chain, then the catch-all body — which
             * inlines finally and re-throws (the no-match fall-through). */
            emit_node_only(catchall, ctx);    /* local.set ex_tmp (landing store) */
            emit_typed_catches(node->try_region.next, ex_tmp, ljoin,sd, se, ctx);
            (void)emit_spine(sir_get_next(catchall), ljoin,sd, se, ctx, NULL);
            if (inherit) {
                /* typed bodies branch to the enclosing join themselves; the catch-all
                 * body always terminates (Throw) — nothing falls through here. */
                sd = sd0; node = NULL; left = true;
            } else {
                ew_byte(&ctx->emit, W_END);   /* $after end                */
                sd = sd0; node = ljoin;
            }
        } else if (is_terminator(node)) {
            emit_node_only(node, ctx);
            node = NULL; left = true; live = false;   /* return/throw: WASM dead after */
        } else if (node->tag == SIR_NOP) {
            sir_node_t* nx = sir_get_next(node);
            node = advance(nx, stop, se, sd, ctx);
            if (!node && nx != stop) { left = true; live = false; }   /* br'd away */
        } else {
            emit_node_only(node, ctx);                            /* StoreLocal / ExprEffect / Inc */
            sir_node_t* nx = sir_get_next(node);
            node = advance(nx, stop, se, sd, ctx);
            if (!node && nx != stop) { left = true; live = false; }   /* br'd away */
        }
    }
    if (wasm_live) *wasm_live = live;
    return !left;   /* control falls through past this region (incl. node==stop) */
}

/* Emit the source-order typed-catch `if`-chain for the TryRegion chain `tr` (each
 * TryRegion wraps the next-inner catch; the innermost wraps the body and is the
 * FIRST source catch). Recurses to the body first, so the `if`s emit on unwind in
 * source order — the JLS §14.19.1 leftmost-match order. Each is a sibling
 * `if (ref.test (ref $Ci)) { ref.cast; local.set <catch var>; <handler body> }`;
 * the caught Throwable lives in `ex_tmp`, re-tested per arm. A matching arm's body
 * branches to the join (`ljoin`); a non-match falls through to the next arm, and
 * past the last arm to the caller's catch-all (finally + rethrow). `tr` that is
 * not a TryRegion is the try body itself — the recursion's base case. */
static void emit_typed_catches(sir_node_t* tr, int ex_tmp, sir_node_t* ljoin,
                               int sd, scope_env_t* se, burg_ctx_t* ctx) {
    if (!tr || tr->tag != SIR_TRYREGION) return;          /* base case: the try body */
    emit_typed_catches(tr->try_region.next, ex_tmp, ljoin,sd, se, ctx);
    sir_node_t* H = tr->try_region.handler;               /* ExceptionEntry */
    int32_t cti = wasm_types_class_typeidx(ctx->types,
                    lat_value_class(ctx->types->sema, H->exception_entry.catch_class_id));
    ew_emit(&ctx->emit, WOP_LOCAL_GET); ew_u32(&ctx->emit, (uint32_t)ex_tmp);
    ew_emit(&ctx->emit, WOP_REF_TEST);  ew_i32(&ctx->emit, cti);
    ew_emit(&ctx->emit, WOP_IF);        ew_byte(&ctx->emit, WBT_VOID);
    ew_emit(&ctx->emit, WOP_LOCAL_GET); ew_u32(&ctx->emit, (uint32_t)ex_tmp);
    ew_emit(&ctx->emit, WOP_REF_CAST);  ew_i32(&ctx->emit, cti);
    emit_node_only(H, ctx);                               /* local.set <catch var slot> */
    se->stack[sd] = H;                                        /* the `if` is one control level */
    bool hft = emit_spine(sir_get_next(H), ljoin,sd + 1, se, ctx, NULL);
    if (hft) transfer(ljoin, se, sd + 1, ctx);         /* body fell through → br to the join */
    ew_byte(&ctx->emit, W_END);                           /* close the `if` */
}

/* Does the method return a value (non-void)? Read from sema via the burg's type
 * table — the §7.6 result arity the function end must satisfy. */
static bool method_returns_value(burg_ctx_t* ctx, const sir_method_t* m) {
    const sema_ctx_t* s = ctx->types ? ctx->types->sema : NULL;
    if (!s || m->method_id < 0) return false;
    const sema_class_t* c = sema_get_class(s, m->class_id);
    return c && c->methods[m->method_id].return_type.tag != JT_VOID;
}

void codegen_method_structured(sir_method_t* method, const compiler_fact_t* facts,
                               int nfacts, burg_ctx_t* ctx) {
    if (!method || !method->entry) return;
    /* The structurer owns the SCOPE rows; filter them out of the whole-method table ONCE.
     * emit_spine queries by header node at every step, and scanning the guard/alloc/except
     * rows on each query cost ~8% of the jre build (callgrind, 07-13). Function-local, the
     * rows this stage owns, freed on exit — the same table, not a new context. */
    compiler_fact_t* sc = NULL;
    for (int i = 0; i < nfacts; i++)
        if (facts[i].kind == COMPILER_FACT_SCOPE) bbq_vec_push(sc, facts[i]);
    scope_env_t se;                       /* the by-key index over the filtered rows */
    scope_env_build(&se, sc, (int)bbq_vec_len(sc));
    bool live = true;                     /* se.stack grows to whatever depth is reached */
    emit_spine(method->entry, NULL,0, &se, ctx, &live);
    /* A non-void method whose body leaves WASM control live at the function end
     * (a synthetic merge after an if/switch all of whose arms returned — §7.6
     * resets reachability after every `end`) would meet the terminating `end`
     * with an empty operand stack but a declared result. JLS §8.4.7 guarantees
     * the point is unreachable, so cap it with `unreachable`: the validator
     * accepts the end and the op never executes. */
    if (live && method_returns_value(ctx, method))
        ew_emit(&ctx->emit, WOP_UNREACHABLE);
    ew_byte(&ctx->emit, W_END);   /* §5.4.1 function body terminating `end` */
    scope_env_free(&se);
    bbq_vec_free(sc);
}
