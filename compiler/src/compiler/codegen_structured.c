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
    /* Every spine node whose own bytes have already been emitted in this method.
     * The emit-once invariant (docs/ddcg-merge-labels.md §2.2) is the one thing
     * the join machinery exists to preserve, so it is cheaper to check the
     * invariant than to trust each of the paths that can lose it. */
    bbq_htree*             emitted;
    /* Branches the frontend marked as implicit-exception guards (COMPILER_FACT_GUARD,
     * keyed on the Branch). A guard is one-armed — `if (bad) throw` — and its false
     * edge is the CONTINUATION, not an else body. Without this the backend cannot
     * tell a guard from a genuine two-armed if whose arms both terminate, and it put
     * the whole remainder of the method into the guard's `else`. */
    bbq_htree*             guards;
} scope_env_t;

static uint32_t se_ptr_key(const void* p) {
    uint32_t k = (uint32_t)(uintptr_t)p;
    return k ? k : 1;                       /* htree disallows key 0 */
}

static void scope_env_build(scope_env_t* se, const compiler_fact_t* f, int n) {
    se->f = f; se->n = n;
    se->stack = NULL;
    se->head = bbq_htree_create();
    se->emitted = bbq_htree_create();
    se->guards  = bbq_htree_create();
    se->next = n ? (int*)malloc((size_t)n * sizeof(int)) : NULL;
    /* Chains DESCEND: head is the last row recorded for a key, `next` steps to
     * earlier ones. Records land inner-first (§2.1), so descending IS outer→inner —
     * the order the framing reads them in. Building the chain the other way meant
     * every node of every walk collected its rows into a scratch vector purely to
     * iterate it backwards: two chain walks and an allocation per node visited. */
    for (int i = 0; i < n; i++) {
        uint32_t k = se_ptr_key(f[i].key);
        void* h = bbq_htree_search(se->head, k);
        se->next[i] = h ? (int)(intptr_t)h - 1 : -1;
        bbq_htree_insert(se->head, k, (void*)(intptr_t)(i + 1));
    }
}

static void scope_env_free(scope_env_t* se) {
    bbq_htree_destroy(se->guards);
    bbq_htree_destroy(se->emitted);
    bbq_htree_destroy(se->head);
    bbq_vec_free(se->stack);
    free(se->next);
}

/* The FIRST row recorded for (node, kind) — the innermost construct's, since
 * records land inner-first. The chain descends, so that is the last match on it. */
static const compiler_fact_t* scope_at(const scope_env_t* se,
                                       const sir_node_t* node, int scope_kind) {
    const compiler_fact_t* first = NULL;
    void* h = bbq_htree_search(se->head, se_ptr_key(node));
    for (int i = h ? (int)(intptr_t)h - 1 : -1; i >= 0; i = se->next[i])
        if (se->f[i].kind == COMPILER_FACT_SCOPE && se->f[i].a == scope_kind
                && se->f[i].key == node)
            first = &se->f[i];
    return first;
}

static bool node_vec_has(sir_node_t* const* v, const sir_node_t* x) {
    for (int i = 0, n = (int)bbq_vec_len(v); i < n; i++)
        if (v[i] == x) return true;
    return false;
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
 * (temporarily detach .next so burg_rewrite reduces just this node).
 *
 * EMIT-ONCE (docs/ddcg-merge-labels.md §2.2). A node that a branch's transfers
 * converge on is a label: its code is emitted once and every other reference is
 * a br. When a Branch cannot resolve its recorded if-join, `ljoin` falls back to
 * the end of the enclosing region and BOTH arms emit the tail instead — 2^k for
 * k such ifs. That has now been the same defect three times over (the if-else-if
 * chain, 07-27's spilled condition, and a Branch for which no join was recorded
 * at all), each time from a different path losing the anchor.
 *
 * So the invariant is checked instead of the paths. A node reached for emission
 * twice in one method IS the duplication, whatever lost the anchor, and it is
 * named here at its first recurrence rather than discovered as an out-of-memory
 * an hour later. */
static void emit_node_only(sir_node_t* n, scope_env_t* se, burg_ctx_t* ctx) {
    if (bbq_htree_search(se->emitted, se_ptr_key(n)))
        burg_set_error("codegen: spine node emitted twice — a branch lost its if-join",
                       (int)n->tag, ctx);
    else
        bbq_htree_insert(se->emitted, se_ptr_key(n), (void*)1);
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
    /* Returning `cont` is the paper's `CG_jump L_next ⇒ ⟨⟨ ⟩⟩` — L_next is emitted
     * right here, so the transfer costs nothing. "`cont` is a recorded label" is
     * not a violation to detect here: a loop header is a label, and the statement
     * before the loop falls straight into it, which IS the label's one emission.
     * §2's rule is observable only at emission, where `emit_node_only` keeps the
     * `emitted` set and reports the second copy at the node that gets it. */
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
    if (cont == stop) return stop;
    return transfer(cont, se, sd, ctx);
}

/* Can the region starting at `node` REACH `target` — some path arriving there
 * rather than terminating or diverting to a `sink` (a label already framed, which
 * the emitted code brs to)? A label nothing reaches is not a label: the paper
 * emits no code for an unreferenced one, and framing it anyway wraps a block whose
 * `end` no path arrives at.
 *
 * §14.19 answers the same question at the SOURCE level, and the frontend records
 * the answer — `Ljoin` is nil when nothing can reach it. That is not a substitute:
 * the optimizer folds conditions, so a join that was reachable when the frontend
 * looked is not necessarily reachable by the time this runs. `try { if (1/0 == 0)
 * … }` folds to a guard that always throws, leaving the if's join recorded and
 * live-looking while no path arrives — and framing it emitted a block the §7
 * validator rejected.
 *
 * Conservative by construction: true unless it can PROVE the region cannot reach
 * `target`, so unmodelled shapes (switch/try, fuel exhaustion) frame the join. A
 * redundant frame is valid; a missing one miscompiles. */
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
                       scope_env_t* se, burg_ctx_t* ctx, bool* wasm_live) {
    /* `left`: did control leave this region (SIR sense — return/throw/br)? Drives
     * the fall-through return value callers read. `live`: is WASM control live
     * past this region's last byte? It diverges from !left only when a void
     * if/switch/try completes with all arms terminated — SIR-dead, yet the §7.6
     * validator resets control to *reachable* after every `end`. (Reported via
     * `wasm_live` for the function epilogue; NULL when a caller doesn't need it.) */
    bool left = false;
    bool live = true;
    /* A recorded error ends the walk. `burg_set_error` only REMEMBERS the first
     * message — nothing ever acted on it — so a "no cover" diagnostic went on
     * emitting bytes for the construct it had just failed to cover, and an
     * emit-once violation would otherwise run its full 2^k course before anyone
     * got to read the message. */
    while (node && node != stop && !ctx->burg_error_msg) {
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
            /* `own` is the ONE label a Branch standing here reads for itself: the
             * first BLOCK row, which the plain-if path picks up as `js` and turns
             * into a native if/else. Everything else keyed here is a label of some
             * ENCLOSING construct, which cannot be read where it is used and so is
             * framed here instead.
             *
             * Why anything is left over: §2.1 keys a join on "the branch head the
             * backend will encounter top-down" and §2.2 then reads it "at a
             * SIR_BRANCH node B". Those are the same node only when the condition
             * compiles to a bare Branch. A SPILLED condition — a ternary, a field
             * read, any complex operand — returns its spill chain's head, so the
             * record sits upstream of the Branch that needs it; and when the
             * condition's head is itself another construct's Branch
             * (`if ((b ? x : y) > 0)`) one node keys two joins. */
            const compiler_fact_t* own = (node->tag == SIR_BRANCH)
                                       ? block_scope_at(se, node) : NULL;
            /* Every BLOCK and MERGE label keyed here, outermost first.
             *
             * ONE ordered list, not BLOCK-then-MERGE. §2.1: records land inner-first,
             * so reverse record order IS the nesting, whatever kind each row is.
             * Framing the BLOCK rows outside the MERGE rows instead put a ternary's
             * value-join — recorded first, therefore innermost — outside the `&&`'s
             * Lf, and the join's code landed in the wrong frame.
             *
             * The count is a property of the method: no cap. Truncating dropped a br
             * target, and a dropped target is re-emitted inline rather than branched
             * to. */
            sir_node_t** bounds = NULL;
            bool others = false;      /* a surviving row that `own` does not cover */
            {
                void* h = bbq_htree_search(se->head, se_ptr_key(node));
                for (int i = h ? (int)(intptr_t)h - 1 : -1; i >= 0; i = se->next[i]) {
                    const compiler_fact_t* f = &se->f[i];
                    if (f->kind != COMPILER_FACT_SCOPE || f->key != node) continue;
                    if (f->a != COMPILER_SCOPE_BLOCK && f->a != COMPILER_SCOPE_MERGE) continue;
                    sir_node_t* X = f->aux;
                    /* §14.19 already answered "does anything reach this join": when
                     * nothing does, the frontend built no anchor and `aux` is nil. */
                    if (!X || X == stop || br_depth(se, sd, X) >= 0) continue;
                    /* …and something has to REACH it. The rows already framed are
                     * the sinks: a path that brs to one of those never arrives here,
                     * so it does not make X a label. */
                    if (!region_reaches(node, X, bounds, (int)bbq_vec_len(bounds),
                                        se, 1 << 20)) continue;
                    if (f != own) others = true;
                    /* One label, one frame — however many rules named it. Two rules
                     * can legitimately record the SAME destination on one head:
                     * `(b ? p : q) || r` has the ternary (its arms share the parent's
                     * Lt) and shortcircuit_pair (both sides share Lt) recording Lt on
                     * the ternary's Branch. Framing it twice emits its code twice. */
                    if (!node_vec_has(bounds, X)) bbq_vec_push(bounds, X);
                }
            }
            int nb = (int)bbq_vec_len(bounds);
            /* Nothing but `own` survives: the plain-if path reads it from the record
             * and emits the smaller native if/else, so frame nothing (§2.2 step 3 —
             * a plain if's pinned bytes must not change).
             *
             * The test is "was there another ROW", not "was there another LABEL".
             * They differ, and the difference is a whole class of if: in a ONE-ARMED
             * `if (a && b)` the chain's shared exit Lf IS the if's Ljoin, so the head
             * carries a MERGE row and a BLOCK row naming one node. Deduping leaves a
             * single bound, and reading that as "only the plain-if row" frames
             * nothing — the compound condition then emits as a native one-armed `if`
             * whose then-arm is the rest of the CONDITION, and the chain's branches
             * have no target left. */
            if (!others) nb = 0;
            if (nb > 0) {
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
                    bbq_vec_free(bounds);
                    continue;
                }
            }
            bbq_vec_free(bounds);
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
            sir_node_t* cond = node->branch.cond;
            sir_node_t* on_t = node->branch.on_true;
            sir_node_t* on_f = node->branch.on_false;
            int df = br_depth(se, sd, on_f);
            int dt = br_depth(se, sd, on_t);
            /* The three transfer paths below all end the same way: `br_if` one
             * destination, then hand the OTHER to CG_jump. Handing it to the walk
             * directly instead is the paper's `CG_jump L_next` case asserted rather
             * than tested — and it is wrong whenever that arm is itself a framed
             * label, which emits its code a second time. p.13 gives all three:
             * `brt Lt` when Lf is next, `brf Lf` when Lt is next, `brt Lt; jmp Lf`
             * when neither is. `advance` is the jmp. */
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
                node = advance(on_f, stop, se, sd, ctx);
                if (!node && on_f != stop) { left = true; live = false; }
            } else if (df >= 0) {
                /* transfer on FALSE (loop test, or a framed merge label): branch
                 * away when the condition is false, so this site wants the
                 * INVERSE — and now asks for it. There is no hardcoded i32.eqz
                 * here any more: inverting is `ncond`'s job and its price is in
                 * the grammar, so the DP decides whether to invert the operand or
                 * the result. */
                if (emit_branch_cond(cond, false, ctx)) ew_emit(&ctx->emit, WOP_I32_EQZ);
                ew_emit(&ctx->emit, WOP_BR_IF); ew_u32(&ctx->emit, (uint32_t)df);
                node = advance(on_t, stop, se, sd, ctx);
                if (!node && on_t != stop) { left = true; live = false; }
            } else if (dt >= 0) {
                /* transfer on TRUE: cond; br_if; then the false arm. Same polarity
                 * need as the back-edge case above. */
                if (emit_branch_cond(cond, true, ctx)) ew_emit(&ctx->emit, WOP_I32_EQZ);
                ew_emit(&ctx->emit, WOP_BR_IF); ew_u32(&ctx->emit, (uint32_t)dt);
                node = advance(on_f, stop, se, sd, ctx);
                if (!node && on_f != stop) { left = true; live = false; }
            } else {
                /* Plain if / ternary: a single-Branch test, both arms inheriting
                 * the recorded Ljoin (Dybvig Fig.5). Native WASM if/else/end whose
                 * `end` is the join; one-armed if (false arm IS the join) emits no
                 * else. Compound short-circuit conditions were framed above, so the
                 * condition here is a single Branch. */
                sir_node_t* ljoin = js ? js->aux : stop;
                /* A GUARD's control destination is not a looked-up join — it is the
                 * successor that does NOT throw, and the frontend recorded which one
                 * that is (`d`: 1 = the TRUE successor throws). Without it `ljoin`
                 * stayed nil, `arm_f != ljoin` was trivially true, and the guard was
                 * emitted two-armed with the entire remainder of the method inside
                 * its `else` — one nesting level per guard. Reading the recorded
                 * destination lets the existing if-no-else test below do its job. */
                if (!js) {
                    void* g = bbq_htree_search(se->guards, se_ptr_key(node));
                    if (g) ljoin = ((int)(intptr_t)g - 1) ? on_f : on_t;
                }
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
                /* A THROW GUARD is one-armed. `if (bad) throw …` has a then-arm that
                 * terminates and no recorded join (§1 exempts the guards precisely
                 * because their throw arm shares nothing), so `ljoin` is nil and
                 * `arm_f != ljoin` is trivially true — the ok path was then emitted
                 * as an `else`, putting the whole remainder of the method inside the
                 * guard, and the next guard inside that one. Linear in guards, so it
                 * looked harmless; it is what drives the frame depth to 181 in
                 * ASCIIToBinaryBuffer.doubleValue and what -O hides by eliminating
                 * the guards. Measured: 123 of these in the prelude, every one
                 * `then=terminated two_armed=1`.
                 *
                 * The paper's shape for a terminating arm is the one-armed form —
                 * close the `if` and continue at the false destination in THIS
                 * region, at this depth. The IR already says the arm terminated;
                 * `emit_spine` returned it. */
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
                /* Falling out of the `if` continues at the join — but "continues at"
                 * is CG_jump, not "emit it here". When the join is a label framed
                 * further out (this region is bounded at something nested inside it),
                 * walking into it emits its code a second time; the first copy is the
                 * one its own frame emits. */
                node = ljoin ? advance(ljoin, stop, se, sd, ctx) : NULL;
                if (!node && ljoin != stop) left = true;
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
            /* The LAYOUT: the groups in source order, the `default:` label taking
             * its place among them (§14.9 — `default:` is a label like any other
             * and falls through to whatever follows it textually). A group reached
             * only by fall-through shares its head with no one, but a group that is
             * BOTH a case target and the default (`case 1: default:`) appears once.
             *
             * A group whose only statement is a JUMP — `break`, `continue`, `break L` — has that
             * jump's target as its case target, because the jump is the whole body and the
             * frontend elides a Goto onto the next chain node. Such a target is not a group: it
             * is the switch exit, or a label of an enclosing scope. Laying it out would give
             * someone else's label a block here and emit its code as a case body, then again
             * where it really belongs. Those values br to the target's own depth instead.
             *
             * `lbreak` is tested by identity because the switch's own $break block is not pushed
             * until below; every other such target is an ENCLOSING scope and so is already on
             * the stack at sd0. */
            int di = node->switch_.default_index;
            sir_node_t** groups = NULL;
            #define SW_IS_JUMP(t) ((t) == lbreak || br_depth(se, sd0, (t)) >= 0)
            for (int i = 0; i <= nc; i++) {
                if (i == di && !SW_IS_JUMP(def) && !node_vec_has(groups, def))
                    bbq_vec_push(groups, def);
                if (i < nc && !SW_IS_JUMP(node->switch_.case_targets[i])
                        && !node_vec_has(groups, node->switch_.case_targets[i]))
                    bbq_vec_push(groups, node->switch_.case_targets[i]);
            }
            int ng = (int)bbq_vec_len(groups);
            if (has_exit && !inherit) { ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID); se->stack[sd++] = lbreak; }
            /* With no `default:` label, out-of-range values leave the switch: they
             * need a landing, and it is a block of its own outside every group. */
            int ddepth = ng;
            if (di < 0) { ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID); se->stack[sd++] = def; }
            else for (int i = 0; i < ng; i++) if (groups[i] == def) { ddepth = i; break; }
            scope_room(se, sd, ng);
            for (int i = ng - 1; i >= 0; i--) {
                ew_emit(&ctx->emit, WOP_BLOCK); ew_byte(&ctx->emit, WBT_VOID);
                se->stack[sd++] = groups[i];
            }
            emit_value(node->switch_.selector, ctx);
            /* Pad the br_table over the whole [lo..hi] span so gaps (sparse
             * switches) route to the default. A value v maps to the DEPTH of the
             * group holding it — which is that group's index in the layout, since
             * groups[0] is innermost. (Extreme sparsity → a giant table; an if-else
             * chain below a density threshold is a size refinement.) */
            /* A jump-only group's landing, asked of the scope stack NOW that every block of this
             * switch is pushed — so a depth counts them, and the inherited-$break case answers
             * with the enclosing scope. */
            if (di >= 0 && SW_IS_JUMP(def)) {
                int dd = br_depth(se, sd, def);
                if (dd >= 0) ddepth = dd;
            }
            int32_t lo = node->switch_.case_values[0], hi = lo;
            for (int i = 1; i < nc; i++) {
                int32_t v = node->switch_.case_values[i];
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            if (lo != 0) {
                ew_emit(&ctx->emit, WOP_I32_CONST); ew_i32(&ctx->emit, lo);
                ew_emit(&ctx->emit, WOP_I32_SUB);
            }
            int64_t span = (int64_t)hi - (int64_t)lo + 1;
            ew_emit(&ctx->emit, WOP_BR_TABLE); ew_u32(&ctx->emit, (uint32_t)span);
            for (int64_t v = lo; v <= hi; v++) {
                int d = ddepth;
                for (int i = 0; i < nc; i++)
                    if (node->switch_.case_values[i] == v) {
                        if (SW_IS_JUMP(node->switch_.case_targets[i])) {
                            int jd = br_depth(se, sd, node->switch_.case_targets[i]);
                            if (jd >= 0) { d = jd; break; }   /* the group is just a jump */
                        }
                        for (int g = 0; g < ng; g++)
                            if (groups[g] == node->switch_.case_targets[i]) { d = g; break; }
                        break;
                    }
                ew_u32(&ctx->emit, (uint32_t)d);
            }
            ew_u32(&ctx->emit, (uint32_t)ddepth);
            for (int i = 0; i < ng; i++) {
                ew_byte(&ctx->emit, W_END); sd--;       /* close $group[i] */
                /* Bounded at the NEXT group: reaching it is the fall-through, and
                 * the next iteration is what emits it. */
                emit_spine(groups[i], (i + 1 < ng) ? groups[i + 1]
                                     : (di < 0 ? def : lbreak),sd, se, ctx, NULL);
            }
            if (di < 0) {
                ew_byte(&ctx->emit, W_END); sd--;       /* close the no-default landing */
                emit_spine(def, lbreak,sd, se, ctx, NULL);
            }
            if (has_exit && !inherit) ew_byte(&ctx->emit, W_END);   /* close $break */
            sd = sd0;
            bbq_vec_free(groups);
            #undef SW_IS_JUMP
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
            emit_node_only(catchall, se, ctx);  /* local.set ex_tmp (landing store) */
            emit_typed_catches(node->try_region.next, ex_tmp, ljoin,sd, se, ctx);
            bool caft = emit_spine(sir_get_next(catchall), ljoin,sd, se, ctx, NULL);
            if (inherit) {
                /* Typed bodies branch to the enclosing join themselves, and the
                 * catch-all body ends in the re-throw — so nothing falls through.
                 * `emit_spine` is what knows that; ask it rather than assert it,
                 * because on the inherited path there is no `end` left for a
                 * fall-through to land on and it would run off the handler. */
                if (caft)
                    burg_set_error("codegen: catch-all body completed normally",
                                   (int)catchall->tag, ctx);
                sd = sd0; node = NULL; left = true;
            } else {
                ew_byte(&ctx->emit, W_END);   /* $after end                */
                sd = sd0; node = ljoin;
            }
        } else if (is_terminator(node)) {
            emit_node_only(node, se, ctx);
            node = NULL; left = true; live = false;   /* return/throw: WASM dead after */
        } else if (node->tag == SIR_NOP) {
            sir_node_t* nx = sir_get_next(node);
            node = advance(nx, stop, se, sd, ctx);
            if (!node && nx != stop) { left = true; live = false; }   /* br'd away */
        } else {
            emit_node_only(node, se, ctx);              /* StoreLocal / ExprEffect / Inc */
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
 * source order — the JLS §14.18.1 leftmost-match order. Each is a sibling
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
    emit_node_only(H, se, ctx);                           /* local.set <catch var slot> */
    se->stack[sd] = H;                                        /* the `if` is one control level */
    bool hft = emit_spine(sir_get_next(H), ljoin,sd + 1, se, ctx, NULL);
    if (hft) transfer(ljoin, se, sd + 1, ctx);   /* body fell through → br to the join */
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
    /* Guard rows are a different KIND and so are not in the SCOPE table above, but
     * the structurer needs them: a throwing guard's surviving edge is the
     * CONTINUATION, not an else body, and `d` says which edge that is.
     *
     * Two kinds do not qualify. DIV_OVERFLOW "THROWS NOTHING: both arms produce the
     * value, so throw_on_true is meaningless for it" — it is a real merge and takes
     * its join the ordinary way, from binary_intdiv_guarded's MERGE record.
     * ARRAY_STORE is keyed on an ExprEffect rather than a Branch (it throws from
     * inside a callee), so it could never match this lookup anyway.
     *
     * The stored value is `d + 1`, so that a hit is never NULL. */
    for (int i = 0; i < nfacts; i++) {
        const compiler_fact_t* g = &facts[i];
        if (g->kind != COMPILER_FACT_GUARD || !g->key) continue;
        if (g->a == COMPILER_GUARD_DIV_OVERFLOW) continue;
        if (g->a == COMPILER_GUARD_ARRAY_STORE)  continue;
        bbq_htree_insert(se.guards, se_ptr_key(g->key), (void*)(intptr_t)(g->d + 1));
    }
    bool live = true;                     /* se.stack grows to whatever depth is reached */
    emit_spine(method->entry, NULL,0, &se, ctx, &live);
    /* A non-void method whose body leaves WASM control live at the function end
     * (a synthetic merge after an if/switch all of whose arms returned — §7.6
     * resets reachability after every `end`) would meet the terminating `end`
     * with an empty operand stack but a declared result. JLS §8.4.5 guarantees
     * the point is unreachable, so cap it with `unreachable`: the validator
     * accepts the end and the op never executes. */
    if (live && method_returns_value(ctx, method))
        ew_emit(&ctx->emit, WOP_UNREACHABLE);
    ew_byte(&ctx->emit, W_END);   /* §5.4.1 function body terminating `end` */
    scope_env_free(&se);
    bbq_vec_free(sc);
}
