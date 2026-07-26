/* compiler.h — DDCG compiler: typed AST → SIR
 *
 * Implements destination-driven code generation per Dybvig, Hieb & Butler
 * (Indiana TR #302, 1990). Each AST form is compiled with a data
 * destination δ (effect | location) and a control destination γ (single
 * label | label pair | return), plus the Lnext fall-through hint from
 * page 13 of the paper. Rule bodies live in grammar/compiler.ddcg;
 * the dest types (delta_t/gamma_t/rho_t) are defined in compiler_runtime.h.
 */
#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>

#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"   /* type_pool_t — published τ̂ interning */
#include "gen/sir_ast.h"
#include "bbq_hmap.h"

/* ── THE SIDECAR: one fact row ───────────────────────────────
 *
 * Every stage RECORDS what it knows about a node at the moment it builds it, and
 * every later stage READS that instead of re-deriving it. There is ONE row type, ONE
 * recorder (`record_fact`, an AUX in grammar/compiler.ddcg) and ONE getter
 * (`compiler_get_facts`).
 *
 * WHY IT IS ONE. It used to be four near-identical schemas — try_region / scope /
 * guard / alloc — each with its own accumulator, its own `all_*` + `all_*_counts`
 * pair on the context, and its own getter. Four copies of "a row keyed by a
 * sir_node_t*, with a kind and some small ints". Nobody chose four; four is what you
 * get from four increments of copy-the-neighbour. The COST of that was not the
 * duplication: it was that recording the FIFTH fact meant a five-file ritual, while
 * `eng->spine[]` + a successor walk was thirty lines in one file — so the optimizer
 * kept growing hand-rolled walkers that recover structure the frontend already knew.
 * That is the single most expensive recurring mistake in this compiler, and spec §8
 * ("Why there is no dominator tree") says it must not happen at all.
 *
 * ADDING A FACT IS: one enum value, one row in the payload table below, one
 * `record_fact(...)` call in the grammar. Nothing else. **If that ever stops being
 * true, fix THIS — do not work around it.**
 *
 * PAYLOAD TABLE — what (key, aux, a, b, c, d) mean, per kind:
 *
 *  TRY_REGION    key = the try body's start Nop (exception-table start_pc)
 *                aux = the ExceptionEntry for this catch (handler_pc)
 *                a   = the catch type as CODEGEN's exception table wants it (the DDCG
 *                      records the CP classref; 0 = the catch-all sentinel; the
 *                      hand-built $ensure_init/$main rows carry sema class ids —
 *                      pre-existing asymmetry, consumed only by codegen). Semantic
 *                      catch classes are read off the ExceptionEntry NODE, never here.
 *                b   = the try's REGION ID (see EXCEPT_REGION)
 *                end_pc is NOT stored: codegen derives it as min(handler_pc) across
 *                the regions sharing this key — handlers always immediately follow
 *                their body.
 *
 *  SCOPE         key = header — loop: Ltop | if: the test | switch: the Switch node
 *                             | merge: the branch head
 *                aux = exit   — loop: Lbreak | if: Ljoin | switch: Lbreak
 *                             | merge: the shared label the transfers converge on
 *                               (emit-once; docs/ddcg-merge-labels.md)
 *                a   = compiler_scope_kind_t
 *                The DDCG builds while/for/if from structured source, so it knows
 *                every loop/merge as it constructs them. Recorded inside-out, so
 *                record order IS nesting order.
 *
 *  GUARD         key = the guard's Branch (an ExprEffect for ARRAY_STORE — see below)
 *                a   = compiler_guard_kind_t
 *                b   = subject slot (the local the guard tests; -1 if none)
 *                c   = aux slot (a second local — the array, for a bounds check; -1)
 *                d   = 1 if the TRUE successor throws, 0 if the FALSE one does
 *                SLOTS, not node pointers: the optimizer rewrites expression trees, so
 *                a pointer into a condition goes stale, while the reaching definition
 *                of a slot at the guard's spine node is what the engine already knows.
 *
 *  ALLOC         key = the New / NewArray / NewRefArray node
 *                a   = 1 if the site can run more than once
 *                Obj naming is 1-limited (one abstract object per SITE), so a looping
 *                site is a SUMMARY of every object it makes and may never be strongly
 *                updated. The DDCG knows things the optimizer cannot see: for a
 *                rectangular multi-dim `new` it emits the fill loop ITSELF, so every
 *                inner level is a summary even when the source has no loop at all.
 *                FAIL-CLOSED: an unrecorded site is a summary.
 *
 *  EXCEPT_REGION key = an EXCEPTING node (JLS §11.1: a Throw, an Invoke*, a New /
 *                      NewArray / NewRefArray, a ClassConstruct)
 *                a   = the REGION ID of one try enclosing it
 *                ONE ROW PER ENCLOSING REGION, innermost first — recorded at the mint
 *                site off ρ; the optimizer never classifies nodes (§8).
 *
 *                THE ROWS ARE THE RECORD; THE GRAPH CARRIES THE EDGE. When the try
 *                rule closes its handler chain (build order: handlers are built AFTER
 *                the body they protect, which is why the mint site can only name the
 *                region ID), `patch_excepts` stamps the node's `exc` attribute — spec
 *                §1's exception continuation, the second γ — with the INNERMOST
 *                enclosing region's chain, first write wins. The engine reads THE
 *                FIELD; these rows remain as the completeness oracle (test_sir §31)
 *                and the backpatch's worklist.
 *
 *                The catch-all's rethrow records against the PARENT ρ (it runs inside
 *                the handler, so its own try cannot catch it) — that hop is how outer
 *                regions are reached from an inner one.
 */
/* BLOCK / LOOP / SWITCH / MERGE are the STRUCTURER's vocabulary: it frames a
 * wasm block/loop from them and emits a MERGE's shared label once.
 *
 * CONTINUE is not one of those. It records the continue target — a `for`'s update
 * node, a do-while's test — which the body falls through to AND every `continue`
 * jumps to, so control converges there. That is a fact the OPTIMIZER needs (it is
 * a merge; nothing may be lifted across it as if the step were forced) and the
 * structurer must ignore (the loop's own br already reaches it; treating it as a
 * MERGE label makes emit_spine re-emit forever — which is exactly what happened
 * when I first recorded it as a MERGE). Each stage reads the kinds it owns. */
typedef enum { COMPILER_SCOPE_BLOCK = 0, COMPILER_SCOPE_LOOP = 1,
               COMPILER_SCOPE_SWITCH = 2, COMPILER_SCOPE_MERGE = 3,
               COMPILER_SCOPE_CONTINUE = 4 } compiler_scope_kind_t;

/* ── §15 implicit-exception guards ───────────────────────────
 *
 * The DDCG emits a guard before every operation the JLS says can throw an
 * implicit exception — naively and completely; pruning the ones that provably
 * cannot fire is the optimizer's job (the DDCG/Click division). The DDCG is the
 * stage that KNOWS which branches are guards and what value each one tests, so
 * it records that here rather than leaving the optimizer to recover it by
 * pattern-matching `Branch(Eq(x, LoadNull))` — a local re-derivation of the
 * frontend's knowledge, which rots the moment a lowering changes.
 *
 * The subject is recorded as a SLOT, not a node pointer: the optimizer rewrites
 * expression trees, so a node pointer into a condition can go stale, whereas the
 * reaching definition of a slot at the guard's spine node is exactly what the
 * engine's state table already answers. */
typedef enum {
    COMPILER_GUARD_NPE = 0,          /* subject: the receiver ref            */
    COMPILER_GUARD_ARRAY_INDEX_LOW,  /* subject: the index (i < 0)           */
    COMPILER_GUARD_ARRAY_INDEX_HIGH, /* subject: the index; aux: the array   */
    COMPILER_GUARD_NEG_ARRAY_SIZE,   /* subject: the requested size          */
    COMPILER_GUARD_DIV_ZERO,         /* subject: the divisor                 */
    COMPILER_GUARD_CLASS_CAST,       /* subject: the object being cast       */
    /* The INT_MIN/-1 arm. JLS §15.17.2 says `MIN_VALUE / -1` WRAPS to MIN_VALUE, but
     * WASM's i32.div_s/i64.div_s TRAP on it, so the DDCG emits a `divisor == -1` arm
     * that computes `-a` instead. It is a branch the LOWERING inserted and a proof can
     * remove — a guard in every sense that matters here — and the only one that THROWS
     * NOTHING: both arms produce the value, so `throw_on_true` is meaningless for it.
     * It is eliminable whenever the range lattice proves the divisor cannot be -1. */
    COMPILER_GUARD_DIV_OVERFLOW,     /* subject: the divisor                 */
    /* JLS §10.10's covariant-store check. THE ODD ONE OUT, and why it went unrecorded and
     * uncounted for so long: every other guard is a Branch with a New(Exception)+Throw arm
     * — which is what the census detects and what the branch-folder removes. This one is a
     * CALL: `ExprEffect(InvokeStatic(Class.arrayStoreCheck(a.elemClass, v)))`, which throws
     * ArrayStoreException from inside the callee (a static ref.test cannot do it — the
     * target is a runtime Class). It needed a different shape, so it was quietly dropped
     * from the plan and every array-store check in the jre went uncounted.
     *   `branch` holds the ExprEffect node; subject = the stored value's slot, aux = the
     *   array's slot. `throw_on_true` is meaningless (there is no arm). */
    COMPILER_GUARD_ARRAY_STORE,      /* subject: the stored value; aux: the array */
    COMPILER_GUARD_KIND_COUNT,
} compiler_guard_kind_t;

/* ── The fact row itself (see the PAYLOAD TABLE at the top) ── */

typedef enum {
    COMPILER_FACT_TRY_REGION = 0,
    COMPILER_FACT_SCOPE,
    COMPILER_FACT_GUARD,
    COMPILER_FACT_ALLOC,
    COMPILER_FACT_EXCEPT_REGION,
    COMPILER_FACT_KIND_COUNT,
} compiler_fact_kind_t;

typedef struct {
    sir_node_t* key;   /* the node this fact is ABOUT — spine identity, survives the optimizer */
    sir_node_t* aux;   /* a second node the fact names (handler / exit); NULL if it names none */
    int kind;          /* compiler_fact_kind_t */
    int a, b, c, d;    /* payload — see the PAYLOAD TABLE */
} compiler_fact_t;

/* ── §7's per-method ESCAPE SUMMARY ─────────────────────────
 *
 * The readout of a method's SOLVED escape lattice that a CALLER consumes (S5.3's MapsTo)
 * instead of §7's conservative bottom graph. Per FORMAL (including `this`): its post-solve
 * escape state, `0 NoEscape / 1 ArgEscape / 2 GlobalEscape` — the SAME encoding as the
 * optimizer's cp_escape_t, which this is a pure readout of (no mutation).
 *
 * WHAT THE DISTINCTION IS (spec §6/§7; Choi Fig 7, read 07-14): MapsTo propagates
 * **GlobalEscape only** from a callee formal to the caller's mapped actual — *"the escape
 * state of the nodes in MapsToObj(n) is marked GlobalEscape if the escape state of n is
 * GlobalEscape."* A formal at ArgEscape is the neutral seed and does NOT worsen the actual.
 * (The cross-parameter-store case — a formal stored into ANOTHER formal's graph — is carried
 * by the reachable sub-graph and Fig 7's edge mapping; this first cut records the per-formal
 * escape STATE, the sub-graph edges land with the full Fig 7 in S5.3.)
 *
 * `computed=false` (or a NULL summaries array) is a §7 BOTTOM METHOD — the caller falls back
 * to the bottom graph, exactly as today. Keyed by slot: `this` is separate (LOADTHIS, not a
 * slot); a ref formal in slot s is `slot_escape[s]`; a non-ref / non-formal slot is
 * COMPILER_ESC_NA. */
typedef enum {
    COMPILER_ESC_NONE = 0, COMPILER_ESC_ARG = 1, COMPILER_ESC_GLOBAL = 2,
    COMPILER_ESC_NA = 0xFF,          /* not a ref formal (or: a static method has no `this`) */
} compiler_escape_t;

/* §7.2 return classification (the pointer half). A method whose EVERY reachable `return` yields
 * the SAME formal (`return this` / `return arg`) lets a caller ALIAS the call result to that actual
 * — the result's pts/type/nullness are the actual's, not the opaque `Oret`. Anything else (a fresh
 * object, a field, a mix, a global, void/non-ref) is COMPILER_RET_UNKNOWN ⟹ the caller keeps `Oret`
 * (conservative, exactly as before). The factory case (`return new C()`) cannot MINT a fresh owned
 * object at the caller (Obj naming is per-SITE; the callee's site is outside the caller's Obj space),
 * so the OBJECT identity stays `Oret` — but the achievable, sound fact IS carried: a `new` never
 * returns null (JLS §15.9.4), so COMPILER_RET_FRESH marks the result **NonNull** while keeping Oret. */
typedef enum {
    COMPILER_RET_UNKNOWN = 0,   /* keep Oret — field/mixed/global/void */
    COMPILER_RET_FORMAL,        /* every return is `ret_param` (this=-1 else param idx) */
    COMPILER_RET_FRESH,         /* every return is a freshly-allocated object ⟹ Oret but NonNull */
    COMPILER_RET_NONNULL,       /* identity unknown (Oret) but provably never null — e.g. the
                                 * TRANSITIVE factory `run(){ return m(); }` whose m() result
                                 * already dropped ⊥null via a FRESH/NONNULL callee summary;
                                 * established across depth by the S5.1 convergence loop */
} compiler_ret_kind_t;

/* §7.2's VALUE half — the return CONSTANT/RANGE of a numeric-returning method (the
 * lattice-D completion of the summary row: Click's combining thesis is ALL lattices in
 * one fixpoint, and §7 makes that fixpoint interprocedural). EXPORTABLE facts only: a
 * symbolic bound (`hi_vn1`) is a per-method vnode id and is stripped; stride drops to
 * dense (a sound superset); REF constants are per-method site ids and never exported;
 * TOP/BOTTOM export nothing. Floats are exportable as KNOWN only, by bit pattern. */
typedef enum {
    COMPILER_RETC_UNKNOWN = 0,  /* no exportable value fact */
    COMPILER_RETC_KNOWN,        /* every reachable return yields ONE value (lo == hi) */
    COMPILER_RETC_RANGE,        /* the meet over the returns: [lo, hi], dense */
} compiler_ret_const_state_t;

struct compiler_summary {
    bool           computed;         /* false = bottom method (no summary) */
    unsigned char  this_escape;      /* `this`, or COMPILER_ESC_NA for a static method */
    unsigned char  ret_escape;       /* the return value, or COMPILER_ESC_NA for void/non-ref */
    unsigned char  ret_kind;         /* compiler_ret_kind_t */
    int            ret_param;        /* COMPILER_RET_FORMAL: -1 = `this`, else parameter index */
    bool           ret_maybe_null;   /* COMPILER_RET_FORMAL: a return can also yield null */
    unsigned char  ret_cstate;       /* compiler_ret_const_state_t (numeric returns only) */
    unsigned char  ret_cwidth;       /* 0=i32 1=i64 2=f32 3=f64 (mirrors cp_cwidth_t) */
    int64_t        ret_clo;          /* KNOWN: the value (floats: bit pattern); RANGE: lo */
    int64_t        ret_chi;          /* KNOWN: == ret_clo;                     RANGE: hi */
    int            slot_count;
    unsigned char* slot_escape;      /* [slot_count]: per ref formal slot, else COMPILER_ESC_NA */

    /* Fig-7 NonLocalGraph (Choi §4.2/§4.4) — the formal-reachable sub-graph a caller needs to
     * follow `O.f ↦ Ô.g` and clobber a REACHABLE object's written cell (`p.child.x`) as well as
     * a direct formal write (`p.x` / `this.f`), and to propagate GlobalEscape. Objects are dense
     * 0..n_obj-1 (a NoEscape object never appears — it is not reachable from a formal, by
     * construction). Roots (`this` / each ref parameter) are the base-case `aᵢ`; the caller seeds
     * MapsToObj(root) = pts(actual) and walks the edges in lock-step with its own heap. Empty
     * (n_obj==0) = no formal-reachable heap structure (a manually-built summary, or a method
     * touching no formal's fields). `slot_obj`/`slot_escape` are PARAMETER-indexed (arg i = param
     * i), the producer having resolved each parameter's local slot through sema_param_slot. */
    int            n_obj;
    unsigned char* obj_escape;   /* [n_obj]: CP escape of each object (ARG or GLOBAL) */
    int            this_obj;     /* summary obj id for `this`, or -1 */
    int*           slot_obj;     /* [slot_count]: summary obj id per ref formal slot, or -1 */
    /* Field edges, CSR over objects: obj k has edge (edge_key[e] → edge_dst[e]) for e in
     * edge_off[k] .. edge_off[k+1]. edge_key is the cell key (fid) — GLOBAL, so a caller
     * matches it against its OWN cells with plain equality. */
    int*           edge_off;     /* [n_obj+1] */
    unsigned int*  edge_key;     /* [n_edge] */
    int*           edge_dst;     /* [n_edge] */
    int            n_edge;
    /* Written cells, CSR over objects: obj k writes wcell_key[w] for w in
     * wcell_off[k] .. wcell_off[k+1]. Covers roots (a direct `p.x`, redundant with the flat
     * write_* above) and reachable objects (a deep `p.child.x`). */
    int*           wcell_off;    /* [n_obj+1] */
    unsigned int*  wcell_key;    /* [n_wcell] */
    int            n_wcell;
    /* Per written cell (§42, Fig 7 "Updating Caller Edges"): MAYBE_NULL = the callee's write may
     * be null, so a caller adds ⊥null to the injected value. Parallel to wcell_key. */
    unsigned char* wcell_flags;  /* [n_wcell]: COMPILER_WCELL_* bits */
    /* §42's COMPLETENESS GUARD (spec/plan: "receiver CLEAN ⟹ no bottom sub-call can write that
     * cell"). Per summary object: true iff object k was passed to a BOTTOM method in this callee,
     * so a bottom sub-call could have written its cells with something the summary cannot see. A
     * caller may REPLACE CP_OBJ_EXT with the mapped edges ONLY for a NOT-leaked object; a leaked
     * one keeps EXT. Per-object, NOT per-cell — a leaf's cell can carry imprecise Oext while its
     * receiver is clean. */
    bool*          obj_leaked;   /* [n_obj] */
};

#define COMPILER_WCELL_MAYBE_NULL  1u   /* the written value may be null (a DIRECT write) */
#define COMPILER_WCELL_TRANSITIVE  2u   /* written only via a sub-call — this method cannot see the
                                         * precise value (it has no such cell), so a caller keeps
                                         * CP_OBJ_EXT (+null). Only DIRECT writes inject precisely. */
typedef struct compiler_summary compiler_summary_t;

/* ── Compiler context ───────────────────────────────────────
 *
 * ONE context object for the whole compiler — sema → DDCG → Click → codegen →
 * assembler. Each stage ADDS what it learns to it and TAKES what it needs from
 * it. If the DDCG must tell Click something (where the merges are, where the §15
 * guards are), it goes in HERE — never a parallel sidecar struct, and never as
 * extra parameters threaded through a stage's signature. It is the class you
 * would write if C had classes; adding a new fact must never change a signature.
 */

typedef struct {
    bbq_arena* arena;
    const sema_ctx_t* sema;

    /* The compiled methods — the thing every later stage operates on.
     * compiler_compile fills this (and returns it, for convenience). */
    sir_method_t** methods;

    /* Run the Click optimizer (sir_optimize) on every method — and the
     * synthesized <clinit> — between SIR construction and codegen. Off by
     * default; javelinac -O / --optimize sets it. */
    bool optimize;

    /* THE SIDECAR — every recorded fact, all kinds, one table per method.
     * Populated by compiler_compile from the DDCG's per-method accumulator. */
    compiler_fact_t** all_facts;              /* arena array of bbq_vecs, one per method */
    int* all_fact_counts;                     /* arena array of counts */
    int method_count;                         /* number of compiled methods */
    /* §3 devirtualization: how many vtable dispatches the optimizer turned into direct
     * calls, across every method it has run on. Accumulated HERE — the census reports it
     * beside the guard counts rather than growing a second reporting mechanism. */
    int devirt_total;
    /* §6's escape yield: allocation SITES proved method-local, out of all local sites.
     * Same census, same context — the escape lattice gets no reporting path of its own. */
    int noescape_total;
    int alloc_total;
    /* …split by what the site allocates, because the CONSUMER differs: a struct's fields
     * become slots (§6), while an array's cell is monolithic (§1) and cannot be split
     * until the range lattice makes it index-sensitive. */
    int noescape_struct;
    int noescape_array;
    /* §6's consumer: sites whose every use the scalar-replacement rewrite can remove. */
    int scalar_total;

    /* The synthesized module initializer (<clinit>): runs static-field
     * declaration-site initializers at instantiation. NULL when the program has
     * no static initializers. Emitted by the assembler as one extra function past
     * the table functions, pointed at by the start section. */
    sir_method_t* clinit;
    /* The <clinit>'s facts (static blocks may contain loops/ifs, hence SCOPEs);
     * threaded to the structurer and the optimizer just like a method's. */
    compiler_fact_t* clinit_facts;            /* arena bbq_vec, or NULL */
    int clinit_fact_count;

    /* ── §7's DEFUNCTIONALIZED CALL GRAPH — an INPUT, never an output ──────────
     *
     * Spec §10: the combined analysis "CONSUMES the lowered value graph + the
     * defunctionalized call graph". Spec §7: "the defunctionalized call_ref target set
     * makes this precise and finite — the VFG paper spends its whole scalability budget
     * approximating exactly what you already have."
     *
     * Java 1.0 has no function values, so a call site's COMPLETE target set is enumerable
     * from the class table alone: the §4.10.2 subtype filter + sema_resolve_virtual, the
     * ONE dispatch rule the vtable rows also materialize. Nothing the analysis learns can
     * ADD an edge — pts can only shrink a site toward a singleton (§3's devirt). Contrast
     * the VFG paper, whose call graph is a fixpoint OUTPUT co-evolving with points-to
     * (it computes "full points-to sets for function pointers only", and re-clones when
     * "there are extra targets for a function pointer"). PINNED: test_sema "the
     * defunctionalized call graph", test_wasm_types (the emitted vtable rows).
     *
     * CSR over METHOD INDICES (the compiler_compile array): the callees of method m are
     * cg_edge[cg_off[m] .. cg_off[m] + cg_cnt[m]). Built ONCE by compiler_build_callgraph;
     * D5's driver reads it and never rebuilds it. */
    int* cg_edge;
    int* cg_off;
    int* cg_cnt;
    bool cg_built;

    /* (declaring class, class-local index) → method index. THE lookup, built once and shared:
     * the call-graph builder and compiler_method_index are the same question and must not be
     * two implementations of it. A linear scan here is quadratic-on-quadratic — a virtual site
     * enumerates every subtype and each subtype costs a full table scan — and the analysis
     * queries it per dispatch target, per call site, per fixpoint iteration. `mi_count` is the
     * method_count the index was built at, so a table that grew rebuilds instead of missing. */
    bbq_hmap method_index;
    bool     mi_built;
    int      mi_count;

    /* The Click analysis's solved facts, one entry per method index, lazily allocated: the
     * partitions, leaders, constants, types, pts and escape states the application phase
     * reads (spec §8.1.1 lists the set, and what is deliberately not in it).
     *
     * Opaque here: the payload is the optimizer's lattice types, defined in sir_optimizer.h.
     * The context holds THAT the analysis published, not what its lattices look like. */
    struct compiler_click_facts* click_facts;   /* arena array [method_count] */
    /* The pool published τ̂ types are interned in. Each engine hash-conses into its OWN pool
     * off its own arena, so a published `const Type*` would dangle the moment that arena is
     * freed; re-interning here keeps both the lifetime and pointer-equality (τ̂ comparison is
     * pointer comparison). Lives on ctx->arena. */
    type_pool_t click_types;
    bool        click_types_init;

    /* §7's per-method ESCAPE SUMMARY — see compiler_summary_t. One entry per method index,
     * allocated lazily; a NULL array or an entry with computed=false is a §7 BOTTOM METHOD.
     * The D5 driver runs methods callee-first, so a callee's summary is here when its caller
     * is analyzed. */
    struct compiler_summary* summaries;      /* arena array [method_count], or NULL */
    /* Choi §4's iterate-to-convergence signal: cp_summarize sets this true when a method's
     * recomputed summary DIFFERS from the one already stored. The driver clears it before each
     * reverse-topological summarize pass and repeats while it is set — so a back-edge callee
     * (a cycle) refines to a fixpoint. Summaries are monotone over finite domains, so it
     * terminates; an acyclic graph converges in one confirming pass. */
    bool summary_changed;
    /* Per-method refinement of the same signal, armed only by the convergence
     * driver: when non-NULL, cp_summarize also flags WHICH method moved, so a
     * pass re-summarizes only the CALLERS of moved methods (Kildall over the
     * call graph — the identical fixpoint without the dead re-solves; a method
     * none of whose callees moved recomputes its identical summary). */
    bool* sum_changed;
} compiler_ctx_t;

/* ── Public API ───────────────────────────────────────────── */

void compiler_init(compiler_ctx_t* ctx, bbq_arena* arena, const sema_ctx_t* sema);
sir_method_t** compiler_compile(compiler_ctx_t* ctx, ast_program_t* program, int* out_count);
void compiler_destroy(compiler_ctx_t* ctx);

/* THE ONE GETTER. Every fact recorded for this method, all kinds, in RECORD ORDER —
 * which for SCOPEs is nesting order (rules build inner scopes first), and for
 * THROW_REGIONs is innermost handler first. Readers filter on `.kind`:
 *
 *     int nf; const compiler_fact_t* f = compiler_get_facts(ctx, m, &nf);
 *     for (int i = 0; i < nf; i++) {
 *         if (f[i].kind != COMPILER_FACT_GUARD) continue;
 *         ...
 *     }
 *
 * Valid until compiler_destroy. method_idx indexes the array compiler_compile returns;
 * out of range yields (NULL, 0). */
const compiler_fact_t* compiler_get_facts(const compiler_ctx_t* ctx,
                                          int method_idx, int* count);

/* Build §7's defunctionalized call graph over ctx->methods (idempotent). Every call site's
 * COMPLETE target set, by the ONE dispatch rule — never an approximation, never refined by
 * the analysis. Callees are METHOD INDICES into ctx->methods; a call whose target is not a
 * compiled method (an abstract declaration, a native, a cross-module jre import) yields NO
 * edge — those are §7's BOTTOM METHODS and the driver must treat them as such. */
void compiler_build_callgraph(compiler_ctx_t* ctx);

/* The callees of method `m` (compiler_build_callgraph must have run). */
int compiler_callee_count(const compiler_ctx_t* ctx, int m);
int compiler_callee(const compiler_ctx_t* ctx, int m, int k);

/* The method index of (declaring class, class-local method index), or -1 when that method
 * is not one this compilation emits a body for (§7's bottom-method boundary). */
int compiler_method_index(const compiler_ctx_t* ctx, int class_id, int method_id);

/* This method's §7 escape summary, or NULL if it is a BOTTOM METHOD (not analyzed, native,
 * or its solve has not run yet). Produced as a readout of the solve inside sir_optimize;
 * consumed by a later method's solve in the D5 driver's reverse-topological order. */
const compiler_summary_t* compiler_method_summary(const compiler_ctx_t* ctx, int method_idx);

/* D5's ANALYSIS ORDER — reverse-topological over the call graph (Choi §4: "iterate over the
 * nodes in the call graph in a reverse topological order … we ignore back edges"). Fills
 * `order[0 .. method_count)` with every method index exactly once, each CALLEE before its
 * CALLER in the acyclic part; returns the count (== method_count). A DFS postorder — back
 * edges (recursion) fall out for free because a visited node is never re-emitted. NOT an
 * SCC condensation, NOT a dominator order (S5.1's forbidden list): cycles are resolved by
 * the driver's convergence iteration, not by a structure on the graph.
 * compiler_build_callgraph must have run. */
int compiler_analysis_order(compiler_ctx_t* ctx, int* order);

#endif /* COMPILER_H */
