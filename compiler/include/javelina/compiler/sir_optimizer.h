/* sir_optimizer.h — public entry to the SIR optimizer plus the
 * value-graph data structures the engine builds. The engine is
 * Click §4.2 partition refinement (CCP + UCE + GVN unified into a
 * single monotone solve); the public API is one function. The
 * partition machinery (cp_engine_t, cp_build, cp_rewrite, cp_pack,
 * cp_compute_liveness) is exposed below so test_click_partition.c
 * can pin engine invariants directly — no production caller touches
 * those.
 *
 * The engine is Click's thesis Chapter 4 in our terms. */
#ifndef JAVELINA_COMPILER_SIR_OPTIMIZER_H
#define JAVELINA_COMPILER_SIR_OPTIMIZER_H

#include "bbq_arena.h"
#include "bbq_htree.h"
#include "bbq_hmap.h"
#include "gen/sir_ast.h"
#include "javelina/compiler/compiler.h"   /* the ONE compiler context */
#include "javelina/compiler/sema.h"
#include "javelina/compiler/type_lattice.h"

#include <stdbool.h>
#include <stdint.h>

/* ── Public API ──────────────────────────────────────────────── */

/* Optimize the SIR for `method` in place. Builds the value-graph
 * partition engine, runs the converged Click-§4.2 rewrite, then slot-
 * bin-packs the result. Correctness-preserving.
 *
 * `sema` is read for class-hierarchy info (subtype meets, method /
 * field reference types). Optional — if NULL the pass degrades to
 * exact-class-match semantics, losing precision on subtype-related
 * folds but staying sound. */
/* Optimize `ctx->methods[method_idx]` in place — or the synthesized <clinit>
 * when method_idx == SIR_OPT_CLINIT.
 *
 * Takes THE CONTEXT, not a pile of parts. Everything the optimizer needs it
 * pulls out itself: the method, the sema, the arena, the DDCG's recorded
 * control-flow scopes (compiler_get_scopes) and its §15 guard table
 * (compiler_get_guards). Anything Click LEARNS that a later stage needs goes
 * back into the same context. Adding a new fact must never change this
 * signature — that is the whole point of there being one context object.
 *
 * THE MERGES COME FROM THE RECORDED SCOPES. The DDCG built the control flow, so
 * the DDCG is what knows where control converges; the optimizer reads that and
 * never rediscovers it. Recomputing it is how a dominance walk grew inside the
 * CSE lift (the cp_host_dominates_* family — deleted). */
#define SIR_OPT_CLINIT (-1)

void sir_optimize(compiler_ctx_t* ctx, int method_idx);

/* Slot-bin-pack `method_idx` and nothing else — no analysis, no rewrite.
 *
 * Packing is NOT an optimization. The DDCG mints a fresh slot per SIR temporary, so an
 * unpacked frame grows with the method's expression count, and past the engine's per-frame
 * local cap the emitted function cannot be CALLED at all — the module validates and then
 * traps, naming no cause, at every call site. `-O0` is the bisection mode, i.e. exactly the
 * mode you reach for on a method too big to reason about, so shipping it inside `sir_optimize`
 * made the largest methods the ones -O0 could not run.
 *
 * `sir_optimize` still packs as its final step; this is the same call without the parts that
 * are optional. */
void sir_pack_slots(compiler_ctx_t* ctx, int method_idx);

/* Build + solve + summarize `method_idx` WITHOUT rewriting it — the summarize-only pass. */
void sir_summarize(compiler_ctx_t* ctx, int method_idx);

/* §7 / Choi §4: iterate the reverse-topological summarize pass to a fixpoint (interprocedural
 * summaries converge for recursion). Run BEFORE the rewrite pass so every method rewrites with
 * converged callee summaries. */
void compiler_summarize_to_convergence(compiler_ctx_t* ctx);

/* ── Engine internals exposed for test_click_partition.c ─────────
 *
 * Everything below this line is engine-internal — declarations exist
 * here so the unit suite can construct hand-built fixtures and check
 * cp_build / cp_rewrite outputs at the data-structure level.
 * Production code calls sir_optimize and nothing else. */

/* A value node: a SIR expression node, a synthetic φ at a
 * control-flow merge, or an opaque value with no known structure —
 * a method parameter, an increment result, a caught exception.
 *
 * The φ is a vnode-table entry, NOT a SIR-level node. It is keyed by
 * (merge spine-node, slot) and carries one contributor per predecessor
 * edge of its merge — see phi_merge / phi_slot / phi_pred below.
 * Reaching-def resolution (cp_resolve) synthesizes these from the
 * spine; the optimizer treats them uniformly with expression vnodes
 * during partition refinement.
 *
 * Promoting φ to a sir.asdl node would add a second representation
 * that every downstream consumer (ddcg's output contract, BURG
 * patterns, the assembler) would have to handle and then eliminate
 * before emit — for zero benefit. Click §4.2 needs φ to be a
 * partition member, and a vnode-table entry already is. If a future
 * design proposes adding PhiNode to sir.asdl, that proposal has
 * rederived this badly. The choice predates the current shape and
 * is load-bearing for the SIR / BURG / assembler contract. */
/* parent_spine sentinel: the expression is reached from two different spine rows, so it has no
 * single owning control row. Distinct from -1 ("none recorded") because a consumer must fail
 * closed on it rather than treat it as unresolved-and-retryable. */
#define CP_SPINE_AMBIGUOUS (-2)

/* WHICH rule made a node a Follower — Click §4.7.4: "Each Node has a constant time test to
 * determine if it is a Follower (for now, the test is 'x.opcode = COPY')", i.e. the
 * Follower-justification is PER KIND, and §4.7.5 line 6.1's revert ("is not an identity")
 * judges the node's OWN identity, not some other rule's. Without this, the identity revert
 * fired on load/same-input followers it did not create — reverting a sound link (the §44
 * forwarding pins) and ping-ponging with the apply sweep (a livelock, once transitions were
 * enqueued per Fig 4.7 line 7). One authority per rule: each cp_revert_* judges only links
 * its own cp_apply_* made. */
typedef enum {
    CP_FK_NONE = 0,   /* not a Follower */
    CP_FK_COPY,       /* §4.7 construction COPY (LoadLocal / pts-Refine chain) — no revert */
    CP_FK_IDENT,      /* §4.8 algebraic 1-constant identity — cp_revert_identity_follower */
    CP_FK_SAMEIN,     /* §4.8 same-input idempotent — structural, never reverts */
    CP_FK_LOAD,       /* store→load forward (spec §1) — cp_revert_load_follower (Gate 5) */
    CP_FK_ARRLEN,     /* §10.7 (new T[n]).length ≡ n — re-arms via the leader chain */
    CP_FK_PHI,        /* §4.9 all-live-inputs-one-partition — cp_revert_phi_follower */
} cp_follower_kind_t;

typedef enum {
    CP_VN_EXPR,
    CP_VN_PHI,
    CP_VN_OPAQUE,
    /* Path-sensitive lattice refinement (PoPA Ch.6 Condition
     * Propagation). A Refine vnode has one input (the value being
     * refined) and a static predicate (the per-arm intersection
     * derived from the enclosing Branch's Cmp). Its constant is
     * `cp_input_const(input[0]) ⊓ predicate`. LoadLocal vnodes in
     * the arm subtree are rewired to read the Refine as their
     * input, so Click §4.7's COPY-Follower invariant holds with
     * Refine as the Leader (Follower.const == Leader.const). At
     * the post-arm merge, the φ meets Refine_then ⊔ Refine_else =
     * unrefined original — un-refine is automatic. */
    CP_VN_REFINE,
} cp_vnode_kind_t;

/* op values for the non-expression node kinds — above every
 * sir_node_t_tag (small consecutive enum values), so they cannot
 * collide with one. */
#define CP_OP_PHI    0x10000
#define CP_OP_OPAQUE 0x10001
#define CP_OP_REFINE 0x10002

/* An input edge not resolved to a value node. */
#define CP_INPUT_UNRESOLVED (-1)

/* Constant-propagation fact: an optimistic TOP, a single known 32-bit
 * value, a bounded RANGE of int32 values (optionally with a stride),
 * a known reference identity, or BOTTOM — proven not a compile-time
 * constant.
 *
 * KNOWN(k) is kept distinct from the degenerate RANGE(k, k) so the
 * common fast path (constant-vs-constant identity, simple fold of two
 * known operands) is a single int compare. RANGE arises when the meet
 * of two distinct knowns appears at a PHI, or when widening narrows
 * an unbounded interval to a K-snapped bound.
 *
 * RANGE carries an optional `stride` (Click thesis §4.5 "ranges of
 * integer constants with strides"). stride == 1 means dense range
 * [lo, hi]; stride == k > 1 means {lo, lo+k, ..., hi} with
 * (hi - lo) % k == 0; stride == 0 means "stride unknown" after a
 * non-congruent meet (the range stays sound at dense [lo, hi] but
 * loses the strided-disjointness proof). Only the approximation
 * functions (meet, widen) change for stride per §4.5; the partition-
 * refinement algorithm is unchanged.
 *
 * REF carries a reference identity (Click thesis §8 "pointer
 * constants"): REF_NEW(node_id) for an alloc site, REF_STATIC(field)
 * for a final-static load whose <clinit> resolves to a single New.
 * The only verifier-legal consumer is aliasing precision inside the
 * memory analysis (devirtualizing a virtual call against a known
 * receiver). Two REFs are equal iff
 * their identities match; meet of distinct REFs is BOTTOM.
 *
 * The interval lattice has infinite ascending chains (e.g. [0, 1] ⊑
 * [0, 2] ⊑ [0, 3] ⊑ ...); naive Kleene iteration through a loop
 * would not stabilize. Widening (cp_const_widen, applied at PHI meet)
 * snaps each new bound to a finite K-set (type boundaries plus the
 * method's integer literals), bounding stabilization to ≤ |K|+1
 * steps per vnode per Nielson & Nielson "Principles of Program
 * Analysis" §4.2's K-bounded widening theorem. Without widening, the
 * value lattice would lose the finite-height property the engine's
 * termination guarantee depends on. */
typedef enum {
    CP_C_TOP, CP_C_KNOWN, CP_C_RANGE, CP_C_REF, CP_C_BOTTOM
} cp_const_state_t;
typedef enum {
    CP_REF_NEW,      /* ref_id = vnode index of the New site */
    CP_REF_STATIC,   /* ref_id = packed (class_id, field_idx) of the final-static field */
} cp_ref_kind_t;
/* Which carrier holds a KNOWN value. CP_W_I32 (the default, ordinal 0) covers
 * byte/short/char/int — all i32 — and is the only width with a RANGE lattice.
 * The wider WASM types are KNOWN-or-not (no range/stride): i64 in `lvalue`,
 * f32 in `fvalue`, f64 in `dvalue`.
 *
 * An f32 gets its OWN carrier rather than riding in the double: f32 ⊂ f64 holds
 * for *numeric* values but NOT for bit patterns. Widening a signaling NaN to
 * double sets the mantissa MSB (quieting it) and narrowing back keeps it set, so
 * a double carrier turns raw 0x7F800001 into 0x7FC00001 — and the raw payload is
 * observable (§20.9.18 Float.floatToRawIntBits, which — unlike floatToIntBits —
 * does not canonicalise). Arithmetic likewise computes in the operand's own
 * width: f32 division rounded through double can double-round (JLS §15.17). */
typedef enum {
    CP_W_I32 = 0, CP_W_I64, CP_W_F32, CP_W_F64
} cp_cwidth_t;
typedef struct cp_const_t {
    cp_const_state_t state;
    cp_cwidth_t      cwidth;      /* width of the KNOWN/RANGE value (i32 default) */
    int32_t          value;       /* KNOWN, cwidth == CP_W_I32 */
    int64_t          lvalue;      /* KNOWN, cwidth == CP_W_I64 */
    float            fvalue;      /* KNOWN, cwidth == CP_W_F32 (exact bits) */
    double           dvalue;      /* KNOWN, cwidth == CP_W_F64 */
    int64_t          lo, hi;      /* RANGE bounds; lo ≤ hi. Width per cwidth (i32/i64) */
    int64_t          stride;      /* RANGE: 0=unknown,1=dense,k>1=strided */
    /* RANGE, spec §5: "array.len(a) ⟹ [0,∞) and BINDS AN INDEX VAR TO THAT
     * LENGTH". An interval cannot express `i < a.length` — the bound is a VALUE,
     * not a constant — so a range may additionally carry a SYMBOLIC upper bound:
     * the value is strictly less than whatever the named vnode computes. This is
     * what lets the §15 upper-bounds guard fall: inside
     * `for (i = 0; i < a.length; i++)` the taken-edge refinement puts the length
     * on `i`, and the guard tests `i >= that same length`.
     *
     * Stored as vnode-id PLUS ONE, so that 0 means "no symbolic bound": every
     * cp_const_t in this file is built with designated initializers, which
     * zero-fill the rest — and vnode 0 is a perfectly valid node.
     *
     * hi_vn_incl distinguishes `i < B` (0 — strict) from `i <= B` (1 — inclusive), the
     * two shapes spec §5's branch refinement names. Without the bit, `i <= B` bound
     * nothing at all: the carrier could only say "strictly less than", so an inclusive
     * loop bound was simply dropped. With it, `i <= B` is the fact `i < B+1`, and a
     * consumer proves `i >= len` false whenever `len ≡ B+1` — the shape
     * `new int[n+1]` + `for (i = 0; i <= n; i++)`. (`new int[n]` with `i <= n` proves
     * NOTHING, and must not: i reaches len, and the read is out of bounds.) */
    int              hi_vn1;
    int              hi_vn_incl;
    /* The bound may be a DIFFERENCE: when non-zero, the value is less than
     * (or ≤, per hi_vn_incl) `value(hi_vn1) − value(hi_sub_vn1)`, same
     * vnode-id-PLUS-ONE encoding. One symbolic id cannot say `off ≤ len − n`,
     * which is what a guard on a SUM leaves behind (`off + n ≤ len`), and that
     * is the fact an access at `v[off + i]` under `i < n` needs. Exactly ONE
     * subtracted id: this is a difference bound, not a general constraint
     * system, and nothing composes two of them.
     *
     * A bound is the WHOLE triple — two ids and the inclusivity. Two bounds
     * are the same bound only when both ids match; every site that compared
     * `hi_vn1` compares the pair. */
    int              hi_sub_vn1;
    /* The symbolic LOWER bound, the mirror of hi_vn1: the value is strictly
     * greater than (lo_vn_incl 0) or ≥ (lo_vn_incl 1) whatever the named vnode
     * computes. Same vnode-id-PLUS-ONE encoding (0 = none). Minted on an
     * `x == y`-taken edge — where x inherits y as BOTH bounds, so `x == y` pins
     * x to y from both sides — and consumed by a lower-bound guard arm
     * (`x >= L` folds true when x's symbolic lower bound is L). `x < B` refines
     * the upper bound (hi_vn1); `x > B` / `x >= B` refine this one. */
    int              lo_vn1;
    int              lo_vn_incl;
    cp_ref_kind_t    ref_kind;    /* valid iff state == CP_C_REF */
    uint32_t         ref_id;      /* valid iff state == CP_C_REF */
} cp_const_t;

/* Width-aware accessors. i32 and i64 share the RANGE lattice (bounds keyed by
 * cwidth); floats have no range (KNOWN-or-BOTTOM). Each reads the carrier its
 * width names, converting only when a caller asks for the other float width
 * (f32→f64 widening is value-exact; it is not bit-exact — see above). */
static inline int64_t cp_known_i64(cp_const_t c) {
    return c.cwidth == CP_W_I64 ? c.lvalue : (int64_t)c.value;
}
static inline float cp_known_f32(cp_const_t c) {
    return c.cwidth == CP_W_F32 ? c.fvalue : (float)c.dvalue;
}
static inline double cp_known_f64(cp_const_t c) {
    return c.cwidth == CP_W_F32 ? (double)c.fvalue : c.dvalue;
}
static inline int64_t cp_width_min(cp_cwidth_t w) {
    return w == CP_W_I64 ? INT64_MIN : INT32_MIN;
}
static inline int64_t cp_width_max(cp_cwidth_t w) {
    return w == CP_W_I64 ? INT64_MAX : INT32_MAX;
}

/* ── Points-to: lattice A of the combined analysis ─────────────
 *
 * Abstract objects, 1-limited: one per SITE (both papers name objects this way).
 * The id space, in order:
 *
 *   CP_OBJ_NULL  (0) — the null object (⊥null). Nullability reads exactly this.
 *   CP_OBJ_EXT   (1) — the catch-all phantom: an unknown we have no better name
 *                      for (the contents of an unknown object's field, a static,
 *                      a bottom method's result — until those get their own).
 *   [2 .. obj_first_site)  — PHANTOMS, spec §1's `Oext@param`: "a phantom/
 *                      external object for anything reachable from a formal
 *                      parameter or a global … ONE PER (SITE, TYPE)". Two kinds,
 *                      covering the two halves of "reachable from":
 *
 *                      · one per PARAMETER — the site of an incoming reference is
 *                        the SLOT whose entry value is its SEED (JLS §16's definite
 *                        assignment means a non-parameter local can never read its
 *                        seed, so a seeded ref slot IS a formal parameter). `this`
 *                        gets one too (external, but never null). One phantom per
 *                        param is what stops two parameters from being forced to
 *                        alias each other.
 *                      · one per MEMORY CELL — what is reachable THROUGH a param is
 *                        the contents of its fields, and a field is named by a cell.
 *                        `p.f` and `p.g` are two different unknowns, so they get two
 *                        different phantoms; a cell's phantom is what its own SEED
 *                        row holds, so `p.f.f.f` names the same phantom as `p.f` and
 *                        the naming stays FINITE at any depth. Cells are syntactic
 *                        (the (class, field) pairs the method mentions) — no fixpoint
 *                        dependency, which is what D3 requires of an object NAME.
 *
 *                      A phantom is Maybe-null, is τ̂-unknown, and is never concrete
 *                      (so never strongly updated) — it fails closed in every
 *                      consumer. THIS IS NOT A CONVENIENCE, IT IS WHAT MAKES THE
 *                      NAMING SOUND: two phantoms MAY ALIAS at runtime (`m(T p, T q)`
 *                      can be handed the same object twice), and the analysis does not
 *                      model that — a store through p's phantom is invisible at q's.
 *                      What saves it is that q's field still reads a PHANTOM, and a
 *                      phantom means "anything", so no consumer can conclude anything
 *                      false from the missing element. Any consumer that reads a
 *                      phantom as a KNOWN object, or as an EMPTY set, breaks it.
 *   [obj_first_site ..)  — one per ALLOCATION SITE (New/NewArray/NewRefArray),
 *                      which includes every string literal (the PEG desugars one
 *                      to New(String) + ArrayInit(char)).
 *
 * CONCRETE vs SUMMARY — the distinction §2's strong update turns on. "Singleton
 * {O}" in the spec (VFG Rule 3 / Theorem 3) means O is ONE CONCRETE RUNTIME
 * OBJECT, not that the pts SET has one element. A phantom stands for an unknown
 * object and two phantoms may alias at runtime; an allocation site inside a loop
 * is a SUMMARY of every object it produces. Neither may be strongly updated.
 * cp_obj_is_concrete is the ONE place that answers this.
 *
 * A `pts` value is a bitset over Obj ids. `∅` is ⊥ — "no object reaches here"
 * (an unreached node, or a non-ref value); join is ∪; the domain is finite, so
 * the ascending chain terminates with no widening. There is deliberately NO ⊤:
 * "may point to anything unknown" is spelled with a phantom, which keeps every
 * consumer's question answerable (e.g. NonNull ⟺ pts ≠ ∅ ∧ ⊥null ∉ pts — the
 * spec's own §4 formulation).
 *
 * pts does NOT participate in congruence: it is a derived property, not value
 * identity, and cp_split_by_facts_one must never see it (two nodes with
 * different pts can be the same value; splitting on it would over-split). */
#define CP_OBJ_NULL 0
#define CP_OBJ_EXT  1
#define CP_OBJ_FIRST_PHANTOM 2

typedef struct { uint64_t* bits; } cp_pts_t;   /* NULL bits == ∅ */

/* WHAT AN Obj IS — the one taxonomy. cp_enumerate_objects hands ids out in exactly this
 * order and records the boundaries, so the kind is READ, never re-derived. Anything that
 * needs to know what an object is asks cp_obj_kind; growing a private test (decoding a cell
 * key, comparing against obj_first_site by hand) is how five slightly-different answers to
 * the same question end up in this file. */
typedef enum {
    CP_OBJK_NONE = 0,  /* not an object id at all — fail closed */
    CP_OBJK_NULL,      /* ⊥null */
    CP_OBJK_CATCHALL,  /* CP_OBJ_EXT — any pre-existing object; an invoke's wide kill names it */
    CP_OBJK_PARAM,     /* §1 `Oext@param`: a formal's phantom (and `this`) */
    CP_OBJK_CELL,      /* §1: the phantom for what a pre-existing object holds in a cell */
    CP_OBJK_RET,       /* §1 `Oret@callee` */
    CP_OBJK_SITE,      /* an allocation site in THIS method — the only kind we may remove */
} cp_obj_kind_t;

/* ── §6, lattice E: ESCAPE, per abstract OBJECT ──
 *
 * `NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥)`, meet = min, MONOTONE DOWNWARD: an object
 * starts NoEscape and only ever descends. Keyed on the Obj id, not on a value node — §6's
 * own domain is "per object site O" — but its TRANSFER is per-node (each node lowers the
 * state of every object its operands name), which is what keeps it inside the one combined
 * fixpoint (§8's membership test, §9).
 *
 * An EXTERNAL object — a phantom (`Oext@param`), an `Oret`, the catch-all — is ArgEscape by
 * construction: it came from outside, so it is already reachable from outside. That is not a
 * special case; it is the seed that makes §6's "stored into a param-reachable object" fall
 * out of the ordinary heap rule. */
typedef enum {
    CP_ESC_NONE = 0,   /* NoEscape (⊤): never reachable from outside this method */
    CP_ESC_ARG,        /* ArgEscape: reachable from a param / a return / a callee */
    CP_ESC_GLOBAL,     /* GlobalEscape (⊥): reachable from a static, or a finalizer */
} cp_escape_t;

/* Which points-to filter a CP_VN_REFINE applies on its arm (spec §2: a
 * refinement filters pts along a successor edge). §4's nullability needs the two
 * null predicates; §2's `br_on_cast` — "splits pts along BOTH successor edges" —
 * needs the two class predicates, which carry the tested type (refine_atype /
 * refine_class) and ask `classOf(O) ≤ τ` of the ONE subtype authority.
 *
 * A pts refinement NEVER changes value identity (D2): the refined vnode is pinned as
 * a §4.7 COPY follower of its input, so GVN still sees one value. */
typedef enum {
    CP_REFINE_PTS_NONE = 0,   /* not a reference refinement (a range, say) */
    CP_REFINE_PTS_NONNULL,    /* the arm where the value survived a null test */
    CP_REFINE_PTS_NULL,       /* the arm where it was null */
    CP_REFINE_PTS_ISA,        /* the arm where `v instanceof τ` was TRUE  */
    CP_REFINE_PTS_NOT_ISA,    /* …and the arm where it was FALSE          */
} cp_refine_pts_t;

/* What a memory-state vnode represents (see cp_engine_t::mem_kind). */
#define CP_MEM_NONE  0
#define CP_MEM_STORE 1
#define CP_MEM_WIDE  2
#define CP_MEM_SEED  3
/* An invoke's kill OF ONE CELL. §7's bottom graph: a bottom method can only touch what it was
 * handed (and what is reachable from that, or from a global), so the rows of objects it cannot
 * reach — the NoEscape ones — SURVIVE the call. Keeping them is why the kill is per-cell:
 * a memory vnode holds one `Obj ↦ pts` matrix, so one shared CP_CELL_ALL name cannot carry
 * "cell c's row for object O" for more than one cell. */
#define CP_MEM_KILL  4

typedef struct {
    cp_vnode_kind_t kind;
    /* The SIR tag for CP_VN_EXPR; CP_OP_PHI or CP_OP_OPAQUE otherwise. */
    int op;
    /* Operand value-node indices — length input_count, arena-
     * allocated. A LoadLocal's one input is its reaching definition;
     * other expression nodes carry their tree children; a φ carries
     * one contributor per predecessor edge of its merge; an opaque
     * node has none. */
    int* inputs;
    int  input_count;
    /* The expression node for CP_VN_EXPR; NULL otherwise. */
    sir_node_t* expr;
    /* The merge node and merged slot, for CP_VN_PHI. phi_slot is the
     * local slot for a slot-PHI; phi_cell is the memory cell index
     * for a cell-PHI (Click §8.1.1 memory analysis); exactly one is
     * non-negative on any PHI. */
    sir_node_t* phi_merge;
    int         phi_slot;
    int         phi_cell;
    /* For CP_VN_PHI: per input, the predecessor spine-node index it
     * arrives from (-1 for the method-start edge). */
    int*        phi_pred;
    /* The spine node this expression lives under — its OWNING control row.
     *
     * RECORDED, never attributed after the fact: cp_resolve_loads already walks each spine
     * node's own expression tree with the row index in hand, so this is written where that
     * loop already is. Deriving it later would mean indexing the whole graph, which is the
     * dominance-by-another-name this file exists without.
     *
     * Used by cp_node_const for path-sensitive refinements from enclosing Branch conds
     * (PoPA Ch.6 Condition Propagation — the spine-scoped analog of cp_phi_input_live's
     * predecessor lookup), and it is what Click §4.1.2's control input needs: "Nodes have an
     * input from the basic block (REGION) in which they reside."
     *
     * -1 = no owning row recorded. CP_SPINE_AMBIGUOUS = reached from two DIFFERENT rows, so
     * it cannot be attributed to either; every consumer must fail closed on it. */
    int         parent_spine;
    /* CP_VN_OPAQUE only: the slot this node is the SEED of, or -1. A seeded ref
     * slot is a formal parameter (JLS §16: a local cannot be read before it is
     * assigned), so this is what names the slot's §1 phantom. */
    int         seed_slot;
    /* CP_VN_OPAQUE inc-def only: `i = input(inputs[0]) + inc_delta`, computed as an
     * ANALYSIS TRANSFER in cp_node_const (§5-D induction; an opaque, not a foldable
     * EXPR — the transform never touches it). 0 for a non-inc opaque. */
    int         inc_delta;
    /* Congruence-class id, and the doubly-linked member-list links
     * within that partition (vnode indices, -1 at the ends). */
    int partition;
    int part_prev;
    int part_next;
    /* Scratch link for the refinement step's touched list. */
    int touched_next;
    /* Epoch marker: set to the engine's `touched_gen` when this node
     * is touched during a CAUSE_SPLITS effective-position pass, so
     * commutative-op users folded across positions are not double-
     * counted in the partition's touched_count. */
    int touched_gen;
    /* Type-lattice fact for this node (TK_TOP until propagated). */
    const Type* type;
    /* Points-to fact (lattice A). ∅ until propagated; meaningful only for
     * ref-valued nodes. Read by consumers, NEVER by cp_split_by_facts_one. */
    cp_pts_t    pts;
    /* MEMORY-state vnodes only (a cell-φ, a store, a cell seed): the cell's
     * contents as `Obj ↦ pts` — heap[O] is what O.f may hold here. Allocated
     * ONCE, then recomputed in place: the transfer reads only this node's inputs
     * (O(inputs)), never walks the graph. Object-sensitivity lives in the VALUE;
     * the cell GRAPH stays keyed by (class, field), which is static and sound. */
    cp_pts_t*   heap;       /* [eng->obj_count], or NULL for a non-memory node */
    /* CP_VN_OPAQUE only: the seed's lattice type, routed from the slot
     * it stands for (its declared width / interned referent). A seed
     * typed BOTTOM would poison every φ over it — an int φ and a long
     * φ would carry identical (no-information) type facts and never
     * split, letting cross-valtype values share a partition. */
    const Type* opaque_type;
    /* Constant-propagation fact (CP_C_TOP until propagated). */
    cp_const_t  constant;
    /* For CP_VN_REFINE: the per-arm predicate to intersect with
     * input[0]'s constant. Encoded as a cp_const_t — typically a
     * RANGE/KNOWN/BOTTOM derived from Cmp(op, k) + arm sense. */
    cp_const_t  refine_predicate;
    /* For CP_VN_REFINE over a REFERENCE: which pts filter this arm applies.
     * Spec §2 — a refinement FILTERS the points-to set along a successor edge
     * (`ref.cast` keeps `{O | classOf(O) ≤ τ}`; `br_on_cast` "splits pts(u)
     * along its two successor edges the same way"). §4's nullability is the
     * same device with the null predicate, and it is why there is no separate
     * nullability lattice to keep in step: `null̂(v)` is DERIVED from pts
     * (`⊥null ∈ pts` ⟺ may-be-null), so narrowing pts narrows nullability. */
    cp_refine_pts_t refine_pts;
    /* The type an ISA / NOT_ISA refinement tested — τ in `classOf(O) ≤ τ`. Only
     * SIR_ATCLASS is filtered: an array target is a Class.isInstance check the
     * lattice does not model, and an unmodelled test may drop NOTHING. */
    sir_atype_t refine_atype;
    int         refine_class;
    /* §4.8 Leader/Follower: -1 means this node is a Leader. A non-
     * negative value is the vnode index of the Leader this node
     * follows via an algebraic 1-constant identity; a Follower
     * lives in its Leader's partition (Click thesis §4.8). */
    int         leader;
    /* WHICH rule made the link (cp_follower_kind_t) — §4.7.4's per-kind Follower
     * test. Each cp_revert_* judges ONLY links its own cp_apply_* made. */
    uint8_t     follower_kind;
    /* Click §4.7.1: "Nodes can make the Follower ⇒ Leader transition only once."
     * Set by every cp_revert_*; every apply refuses a burned node. With the
     * transitions running INSIDE PROPAGATE (at the dequeue and the lines 17-21
     * walk) this is the TERMINATION argument, not merely the O(n) cost bound:
     * without it a link whose premise oscillates mid-solve re-forms at every
     * dequeue and the outer drain never empties. */
    bool        f2l_once;
    /* Symbolic-bound premise endpoints (cp_symbolic_bound_const): the vnodes whose
     * PARTITION (bound/lim/other) or CONSTANT (the Add's operands) the transfer
     * reads OFF the def-use graph, recorded as §4.7.4 other.def_use edges so a
     * premise move re-runs the transfer. -1 = slot unused. Slot-diffed: re-pointing
     * a slot removes the old edge, so the segment stays ≤6 live entries per node.
     * A COMPARE (cp_symbolic_bound_const): 0 = the symbolic bound · 1 = the limit
     * (strict) / the Add's non-1 side (inclusive) · 2/3 = the Add's operands
     * (inclusive) or the range carrier · 4 = the bound's SUBTRACTED id.
     * A φ (cp_node_const): the endpoint pairs whose ids it counted as one —
     * 0/1 the upper bound · 2/3 the lower · 4/5 the subtracted id. */
    int         prem_dep[6];
    /* Click §4.7.5 X.cprop list links: this Node is in
     * eng->partitions[partition]->cprop_head's linked list when its
     * type is pending recomputation. -1 when not in the list. */
    int         cprop_next;
    int         cprop_prev;
    bool        in_cprop;
} cp_vnode_t;

/* A congruence class: a doubly-linked list of member value nodes
 * (head index, -1 if empty) and a member count. on_worklist marks
 * it pending as a splitter; touched_* are per-refinement-step
 * scratch. */
typedef struct {
    int  head;
    int  count;
    /* Count of members that are Leaders (`leader == -1`). The split
     * threshold in CAUSE_SPLITS is `|Z.Leader|` (§4.7.5 step 43),
     * not the total member count — Followers are excluded. */
    int  leader_count;
    bool on_worklist;
    int  touched_head;
    int  touched_count;
    /* Click §4.7.5 X.cprop: head of the linked list of vnodes in
     * this partition whose type is pending recomputation. -1 if
     * empty. on_cprop_wl tracks the global cprop worklist. */
    int  cprop_head;
    bool on_cprop_wl;
    /* Cached partition type (§4.3.2 invariant: all members share). */
    const Type* type;
    cp_const_t  constant;
} cp_partition_t;

/* Slot dt/referent summary from the ONE slot-type scanner
 * (cp_scan_slot_types): first dt seen per slot + the interned referent
 * Type for ref slots. Routed into the OPAQUE slot seeds (typed seeds —
 * a BOTTOM seed would poison every φ over it) and into cp_pack's width
 * pools / ref coalescing. */
typedef struct {
    sir_datatype_t* dt;
    bool*           seen;
    const Type**    ref;       /* interned referent Type, NULL = none */
    bool*           ref_uniq;  /* conflicting referents on one slot */
} cp_slot_types_t;

/* Exact pointer-keyed map: `node → the index we gave it`. Every node-identity map in the
 * engine is one of these, and they are read millions of times per compile.
 *
 * A flat open-addressed bbq_hmap on the FULL 64-bit pointer. It used to be a bbq_htree
 * (a nibble trie) keyed on a 32-bit hash of the pointer, plus a collision chain — and the
 * chain was not optional: the old comment here recorded that "the hash compresses a 64-bit
 * pointer, so at whole-jre scale 32-bit collisions are a FACT — a raw htree keyed by the
 * hash silently aliases two distinct nodes (stale slot renames, merged value nodes)". So a
 * lookup was a hash, then up to 8 DEPENDENT pointer loads down the trie, then a chain walk
 * comparing real pointers. Three structures, ~20% of the compile, and a truncation that had
 * to be defended against.
 *
 * The hmap keeps the whole key, so there is no truncation to collide and no chain to walk;
 * a lookup is one probe. bbq_htree remains the right structure for the SPARSE integer keys
 * it was made for (the memory-cell and callee keys still use it). */
typedef struct { bbq_hmap map; } cp_pmap_t;

/* Per-method engine state, allocated from the caller's arena by
 * cp_build. cp_free releases the non-arena tables. */
typedef struct {
    bbq_arena*        arena;
    /* Where the application phase MINTS SIR. cp_rewrite creates nodes that become part of the
     * program graph and must outlive the engine, so they cannot come from `arena` when that is
     * a per-method scratch the caller frees. Defaults to `arena`, so a build that owns its
     * allocations behaves exactly as before. */
    bbq_arena*        out;
    sir_method_t*     method;
    const sema_ctx_t* sema;     /* may be NULL — degrades, stays sound */

    /* Every reachable continuation node, in DFS order. */
    sir_node_t**  spine;        /* bbq_vec */
    int           spine_count;
    cp_pmap_t     spine_idx;    /* sir_node_t* -> (spine index + 1) */

    /* The value nodes, densely indexed. */
    cp_vnode_t**  vnodes;       /* bbq_vec of arena-allocated nodes */
    int           vnode_count;
    cp_pmap_t     expr_idx;     /* expression sir_node_t* -> (index + 1) */

    /* Refine identity: hash(input, predicate, pts, atype, class) -> (index + 1).
     * An EXPR node gets its congruence from cp_partition_init's opcode buckets +
     * CAUSE_SPLITS; a Refine is not a SIR op and has no row there, so without this
     * the same fact refined at two branches would be two values and every
     * expression over them incongruent. Interning at construction is the same
     * "a name is a function of its inputs" rule, applied where Refines are made. */
    bbq_hmap      refine_intern;

    /* Reverse def-use: value node v is used by du_user[k] at operand
     * position du_input[k], for k in du_off[v] .. du_off[v]+du_cnt[v]. */
    int* du_off;
    int* du_cnt;
    int* du_user;
    int* du_input;

    /* Congruence partitions; vnode v belongs to partitions[v.partition]. */
    cp_partition_t** partitions;   /* bbq_vec of arena-allocated partitions */
    int              partition_count;
    int*             worklist;     /* bbq_vec — partitions pending as splitters */

    /* Click §4.7.5 global cprop worklist: partitions with non-empty
     * X.cprop pending PROPAGATE. */
    int*             cprop_worklist;   /* bbq_vec — partition ids */

    /* Click §4.7.5 fallen set, populated by cp_compute_facts and drained
     * by the post-split apply phase (cp_apply_followers_pass) per
     * §4.7.5 lines 14-22: SPLIT comes BEFORE Follower-apply so apply
     * reads post-split partitions. Vnodes whose type/constant fell in
     * the latest drain. */
    int*             fallen;       /* bbq_vec of vnode indices */

    /* The DISCOVERED-edge overflow — §4.7.4's `x.other.def_use` ("later this set will
     * hold edges leading from constants to algebraic 1-constant-input identities"):
     * per-node singly-linked segments of extra def-use edges, appended when a Follower
     * link forms whose Leader is NOT one of the Follower's inputs (LOAD's stored value,
     * ARRLEN's size — both discovered at solve time), removed at revert (§4.7.5 line
     * 6.4's re-segregation, as deletion). Every fact walk iterates the du window PLUS
     * this segment, so lines 10-11 reach ALL followers and no side chain exists — the
     * old per-leader `follower_head` chain was our invention; the paper keeps Followers
     * in partition-level sets reached through edges (§4.7.1 / STEP line 25). Input-
     * linked Follower kinds (COPY/IDENT/PHI/SAMEIN) need no entry here: their leader IS
     * an input, so the ordinary window already carries the edge. */
    int*             du_ov_head;   /* per vnode; -1 = empty */
    int*             du_ov_user;   /* bbq_vec: edge target (-1 = removed) */
    int*             du_ov_next;   /* bbq_vec: next edge in the segment */

    /* Fact-driven escape (W4): the SOURCE-ROW index, recorded at construction, so §6's
     * lowering runs on the rows whose inputs moved instead of sweeping the spine.
     *   esc_src        per row: bit0 = tag source (Return/PutStatic/Throw/PutField/
     *                  ArrayStore), bit1 = the row's tree holds call sites;
     *   esc_call_*     CSR: the call nodes per row (found ONCE at build — the per-round
     *                  tree re-walk was the sweep's real cost);
     *   esc_dep_*      CSR: operand vnode → tag-source rows (a source is a SPINE row,
     *                  not a vnode, so no du edge can carry its operand's pts event —
     *                  the condrow pattern);
     *   esc_pending    the row worklist. CALL rows are re-added COARSELY when any
     *                  pts/heap fact moved (esc_facts_moved): cp_mapsto_graph's Fig-7
     *                  recursion walks pts-DIRECTED heap reachability, so a call's
     *                  input set is itself dynamic — not cheaply enumerable. The
     *                  lowering still runs at the ROUND-END evaluation point (dead rows
     *                  skipped at drain time): evaluating at the change instant would
     *                  lower sticky escape from TRANSIENTLY-live rows. */
    uint8_t*         esc_src;
    int*             esc_call_off;
    sir_node_t**     esc_call_list;
    int*             esc_dep_off;
    int*             esc_dep_list;
    int              esc_dep_rows;
    bool*            esc_pending_flag;
    int*             esc_pending;      /* bbq_vec */
    bool             esc_lowered;      /* any cp_escape_lower moved this drain */
    bool             esc_facts_moved;  /* pts/heap moved since the last escape drain */

    /* Click §4.10 peer-PHI canonical slot. part_canon_phi[partition_id]
     * = vnode idx of the canonical PHI in the partition when ≥2 peer
     * PHIs at different slots sit together, otherwise -1. Populated
     * once before cp_rewrite. */
    int*             part_canon_phi;
    int              part_canon_phi_cap;

    /* Spine index currently being rewritten — set by cp_rewrite_spine_node
     * before each cp_rewrite_expr call so sub-expressions can query
     * slot_in[rewrite_spine_idx][slot] without threading the index
     * through every recursive call. */
    int              rewrite_spine_idx;


    int           slot_count;   /* method->max_locals, at least 1 */

    /* Hash-consed Type lattice for cprop. */
    type_pool_t   pool;

    /* UCE: reachable[i] is true for reachable spine node i; reach_count is how many.
     *
     * WRITTEN by the monotone edge marking (cp_mark_from), never by a per-round walker.
     * Spec §4: "per-edge facts, exactly SCCP's executable-edge mechanism — carried on
     * the SIR edge, no dominance"; §9: "dead regions never get analyzed because the
     * executable-edge flag gates every sub-lattice". A row, once live, never dies
     * (Click §4.3's {U,R} lattice falls U→R and stops); an edge, once executable, is
     * never retracted — so each edge is set at most once over the whole solve and the
     * work is fact-driven, not per-round. The old from-scratch walker is what allowed
     * the non-monotone "swap" (an arm retired as another opened) and forced the exit
     * test to compare whole sets. Post-rewrite the tables are REBUILT over the
     * rewritten graph (cp_reach_reset_and_mark — a §4.10 post-solve consumer). */
    bool*         reachable;
    /* Rows allocated in reachable[] — spine nodes spliced after the
     * table was computed have no entry and are reachable
     * by construction; cp_spine_reachable is the one accessor. */
    int           reachable_rows;
    int           reach_count;
    /* The executable-edge flag (§4/§9): CSR over each row's sir_succ edges, TARGETS
     * RECORDED AT CONSTRUCTION so the solve never walks the graph. Edge k of row i is
     * edge_off[i]+k; its target row is edge_target[·], its flag edge_exec[·].
     * Exceptional (EXCEPT-row) edges are NOT here — a φ input arriving on one falls
     * back to the pred's node bit, exactly the pre-edge behavior for a non-branch pred. */
    int*          edge_off;          /* edge_rows + 1 entries */
    int*          edge_target;
    /* The flag is BIDIRECTIONAL: set when the condition's fact justifies the arm,
     * CLEARED when it stops justifying it. Click's marks never retract because his
     * lattice has no rising facts; ours does — Gate 5 is a revocable load-follower
     * whose formation raises a GetField BOTTOM→KNOWN, so a mark placed on the
     * transient must come back off (see cp_reach_reeval). */
    bool*         edge_exec;
    int*          edge_lit_in;       /* per row: lit in-edges (entry holds a virtual +1) */
    int           edge_rows;
    int           entry_row;         /* spine row of method->entry, -1 if absent */
    /* cond vnode → rows testing it (Branch cond / Switch selector) — the recorded
     * re-arm path for the marking when a condition's fact falls: the branch is a spine
     * row, not a vnode, so no def-use edge can carry the event. */
    int*          condrow_off;       /* condrow_rows + 1 entries, indexed by vnode id */
    int*          condrow_list;
    int           condrow_rows;
    /* Rows with a nonempty §3.6 verdict row — re-marked when a Follower link REVERTS
     * (a verdict matches by cp_value_leader identity, which a revert retracts; marking
     * is monotone, so a re-mark can only open arms, never close them). bbq_vec. */
    int*          verdict_row_list;
    /* φ id range of each merge row (-1 = not a merge). The range has HOLES — subsumed
     * trivial φs are re-kinded OPAQUE in place — so consumers SKIP non-φ entries and
     * bound the walk by row_phi_last, never by "first non-φ" (that starved the φs
     * behind a subsumed sibling of the edge-flip event). */
    int*          row_phi_first;
    int*          row_phi_last;
    /* JAVELINA_VERIFY_FIXPOINT: set when recomputing every node after cp_solve returned
     * changed the answer — some transfer read a fact that is not one of its def-use inputs
     * and nothing re-armed it. Always false when the check is off. */
    bool          verify_failed;

    /* Monotonic epoch counter for CAUSE_SPLITS effective-position
     * dedup — incremented at each (call, position) iteration. */
    int           touched_gen;

    /* Condition-verdict facts (spec §3.6 channel (a)): per spine node,
     * the set of (branch, taken-bit) facts every path from entry to the
     * node crosses — a forward must-analysis computed by the Pass-B
     * sweep (meet = set intersection at merges). A fact means "branch
     * b's condition VALUE is `bit` here"; value facts never die, so
     * there are no kills. Consumers match a later branch's condition
     * against a recorded one by cp_value_leader identity ONLY — never
     * by partition membership (a range Refine is a partition Leader, so
     * congruence across an interposed guard does not exist to consult),
     * and never by writing into vnode->constant (a verdict is an edge
     * fact; storing it as the value's constant would feed
     * cp_split_by_facts_one and split the very partition that justified it).
     *   verdict_words[i]   = bitset over fact ids at spine node i
     *   verdict_stride     = words per node
     *   branch_fact_ord[b] = branch spine b's fact ordinal (-1 = none);
     *                        fact id = 2*ord + taken-bit
     *   branch_cond_vn[b]  = the condition's vnode at phase-R time
     * NULL until cp_compute_branch_refinements has run. */
    uint64_t*     verdict_words;
    int           verdict_stride;
    int           verdict_rows;
    int*          branch_fact_ord;
    int*          branch_cond_vn;
    int*          fact_branch;      /* ordinal → branch spine index */
    int           branch_fact_count;

    /* Backward slot liveness — Kildall worklist over the spine,
     * computed once as the rewrite's prelude. NULL until
     * cp_compute_liveness has run.
     * live_in[i][s]  = slot s is read before next write on some path
     *                  from spine node i.
     * live_out[i][s] = same at i's exit (∪ live_in[succ][s]). */
    bool**        live_in;
    bool**        live_out;

    /* Reaching-def per spine node per slot: slot_in[i][s] is the
     * vnode index that defines slot `s` at the entry to spine[i].
     * Published from cp_resolve's rd.in_state — the slot_vn
     * dominance proxy used by the rewrite to find CSE lift points
     * without building an explicit dominator tree.
     * slot_in_rows is the spine_count at publish time: CSE lifts
     * splice NEW spine nodes (indices ≥ slot_in_rows) that have no
     * row — a reader must treat those as "reaching defs unknown",
     * never index past the published rows. */
    int**         slot_in;
    int           slot_in_rows;

    /* Per-slot dt/referent summary (see cp_slot_types_t). */
    cp_slot_types_t slot_types;

    /* Memory cells for Click §8.1.1 reaching-stores. cp_resolve
     * unifies slot and memory-cell reaching-defs over one state
     * space: slot_in[i][s] for s in 0..slot_count-1 is the slot
     * reaching-def; for s in slot_count..slot_count+mem_cell_count-1
     * it is the (s - slot_count)'th cell's reaching writer. Cells
     * are enumerated by cp_enumerate_memory_cells; GetField /
     * GetStatic / ArrayLoad's memory input (last in v->inputs)
     * reads the reaching writer so the partition-refinement engine
     * collapses two reads of the same cell with the same writer. */
    int           mem_cell_count;
    uint32_t*     mem_cell_keys;   /* [c] = packed (kind, class, field) or (kind, data_type) */
    bbq_htree*    mem_cell_idx;    /* packed key (uint32_t+1) -> (cell_idx + 1) */

    /* §7 mapsto PURE-LOOKUP caches (profile-driven, 07-18: the escape transfer ran
     * cp_cell_lookup 124.9M times per jre build re-translating the SAME summary keys
     * every solver visit). Both are derived once from state that is FIXED for the
     * engine's lifetime — the caller's cell interning and each callee's summary key
     * arrays — so a visit is an array index, never a search.
     *   mapsto_tr[gi]   = per-callee translation: wcell_key/edge_key → local cell
     *                     (one int array, wcells first then edges; -1 = no local
     *                     cell), built on the callee's first visit.
     *   cell_row_*      = cell → memory-SSA rows CSR (mem_cell[] inverted once),
     *                     replacing cp_follow_field's per-call scan of every row. */
    int**         mapsto_tr;
    int*          cell_row_off;
    int*          cell_row_cnt;
    int*          cell_row_list;

    /* Path-sensitive lattice refinement (PoPA Ch.6) has NO engine state: a refinement
     * is a PER-EDGE fact (spec §4). cp_compute_branch_refinements parses each Branch
     * once into per-edge Refine vnodes and re-derives the slot states across those
     * edges, rewiring the loads — all transient to that pass; only the Refine vnodes
     * (ordinary CP_VN_REFINE values) outlive it, wired before cp_partition_init so a
     * rewired LoadLocal is a Follower of its Refine. There is no per-spine table and
     * no arm marking: the old ones were a dominance region computed by traversal (§8). */

    /* ── Points-to (lattice A) ──
     * obj_count abstract objects; obj_words = (obj_count + 63) / 64 bitset
     * words. obj_of_vnode[v] is the Obj id an allocation vnode names, or -1
     * for a non-allocation; vnode_of_obj is its inverse — the allocation vnode
     * naming Obj o, so a consumer can ask an object what it was built from (its
     * size operand, its class). All fixed at cp_build time — Obj naming is
     * SYNTACTIC (one per allocation site), so it needs no fixpoint.
     * NB obj_of_vnode is sized to the vnode count AT ENUMERATION; later passes
     * append vnodes (Refine, CSE), so index it only via vnode_of_obj. */
    int       obj_count;
    int       obj_words;
    int*      obj_of_vnode;
    int*      vnode_of_obj;
    /* Spec §1's `Oext@param`, one phantom per (site, type). obj_of_slot[s] is the
     * phantom naming the reference that ENTERS the method in slot s (i.e. the slot
     * whose entry value is its seed — a formal parameter), or -1 for a slot that is
     * not a seeded ref. obj_this names `this`. obj_of_cell[c] is the phantom naming
     * what an already-existing object holds in cell c — the OTHER half of "reachable
     * from a parameter", and the name that makes the reachable set finite: the seed
     * row of cell c holds obj_of_cell[c], so following .f from a phantom lands back
     * on a phantom. Allocation sites begin at obj_first_site, so
     * `o >= obj_first_site` IS the concrete/summary test. */
    int*      obj_of_slot;
    int       obj_this;
    int*      obj_of_cell;
    /* §1's `Oret@callee`, interned by (class_id, method_idx) — the callee, not the call
     * site, so a loop that calls the same method does not mint an object per iteration.
     * The object id is on obj_of_vnode for each invoke vnode; this map is what shares
     * one id across every call to the same callee. */
    bbq_htree* callee_idx;
    /* The kind BOUNDARIES, recorded where the ids are assigned (cp_enumerate_objects hands
     * them out in kind order). cp_obj_kind reads these — nobody re-derives a kind. */
    int       obj_first_cell;
    int       obj_first_ret;
    int       obj_first_site;

    /* ── Escape (lattice E, spec §6) ──
     * NoEscape(⊤) ⊐ ArgEscape ⊐ GlobalEscape(⊥); meet = min; MONOTONE DOWNWARD.
     * Keyed on the Obj — §6's own domain — because escape is a property of an OBJECT, not
     * of a value naming it: two vnodes naming the same allocation cannot disagree. Sized
     * obj_count, seeded at cp_build, lowered by the per-node transfer INSIDE cp_solve as
     * pts grows (§9: one fixpoint). pts only grows and escape only descends, so the
     * combined loop still terminates. Change detection = esc_lowered (set per actual
     * descent in cp_escape_lower) — no snapshot array. */
    cp_escape_t* escape;
    /* §7.2's side-effected cells, applied at CALL sites by MapsTo: clobbered[o*mem_cell_count+c]
     * is true once object `o`'s cell `c` is written by a callee it was handed to (its escape
     * summary's write set). The memory KILL preserves a NoEscape object's cell only if it is
     * NOT clobbered — a CLEAN-escape ctor receiver is NoEscape yet its written field is stale.
     * Monotone (only set); `clobbered_moved` drives the same re-arm escape movement does. */
    bool*        clobbered;
    bool         clobbered_moved;
    /* §42 (Fig 7 "Updating Caller Edges") — the PRECISE value a callee left in a clobbered cell,
     * so the memory KILL can REPLACE CP_OBJ_EXT with it. `inject` is a per-(obj,cell) pts, flat
     * as obj·cells·obj_words words; `inject_bad[o*cells+c]` = the write is INCOMPLETE (a bottom
     * sub-call could also have written it), so EXT must stay. `obj_bottom[o]` = `o` was passed to
     * a bottom method, so its UN-written cells cannot survive either (Gate 5's guard). All
     * monotone; `inject_moved` re-arms the kills exactly as `clobbered_moved` does. */
    uint64_t*    inject;
    bool*        inject_bad;
    bool*        obj_bottom;
    bool         inject_moved;
    /* Gate 5 soundness: a BOTTOM or NATIVE call can reach an ArgEscape object WITHOUT being
     * handed it (via the caller's disposition — an escaped object may be global-reachable), so
     * obj_bottom (reachability from passed args) is not enough. Once the method makes any such
     * call, no ArgEscape object's un-clobbered cell may survive it. */
    bool         has_bottom_call;
    /* §7.2 transitive-write OVERFLOW: a callee's write on a cell THIS method never mentions has
     * no row in `clobbered` (the matrix is indexed by the method's own syntactic cell table), so
     * a pass-through `f(p){ g(p); }` would drop the fact at the summary boundary. Recorded here
     * as (engine obj, GLOBAL cell key) pairs instead — consumed ONLY by cp_summarize's readout
     * (the method's own kills need nothing: it has no loads of an unmentioned cell to preserve).
     * Parallel bbq_vecs; duplicates possible across sweeps, deduped at readout. */
    int*          clobx_obj;
    unsigned int* clobx_key;
    /* §7.2 field-following scratch (Fig 7's MapsToObj), reused across every call site and every
     * escape sweep so the hot fixpoint allocates nothing: mto[k] is the set of CALLER objects a
     * callee summary object k maps to at the current call, mto_wl the grow-worklist, mto_tgt one
     * follow's target set. Grown to the largest summary's n_obj; reset (memset) per call. */
    cp_pts_t*    mto;
    int*         mto_wl;
    bool*        mto_inq;     /* is summary obj k already on mto_wl (dedup, so |wl| ≤ n_obj) */
    int          mto_cap;
    cp_pts_t     mto_tgt;
    /* §3's devirtualization count for this method — how many vtable dispatches became
     * direct calls. Reported through the SAME census the guards use (the sidecar already
     * holds what was emitted); there is no second reporting mechanism. */
    int       devirt_count;
    /* §6's scalar-replacement count for this method — allocation SITES whose every use
     * the rewrite could remove. Same census, no second reporting path. */
    int       scalar_count;
    /* §6's scalar replacement: `GetField expr` → the `LoadLocal` of the field's slot.
     * Consumed by a lookup at the TOP of cp_rewrite_expr — the same seam the KNOWN-constant
     * substitution uses, so the rewrite needs no traversal of its own. */
    cp_pmap_t scalar_subst;
    /* Per Obj: is this site's `New` actually IN the optimized graph? A site in a region
     * the solve proved dead (an eliminated guard's throw arm) is not — and it does not
     * escape either, since a dead region's values hold ∅ pts. Counting such a site as a
     * removable allocation double-counts a win guard elimination already took, which is
     * what the first escape census did. Filled by the scalar-replacement sweep. */
    bool*     obj_alloced;
    /* One scratch cell-map, so the heap transfer can recompute a memory-SSA name's
     * value from its inputs and commit it only if it changed — without allocating
     * per call. The transfer is not reentrant, so one suffices. */
    cp_pts_t* heap_scratch;

    /* ── What the DDCG recorded about the control flow it BUILT ──
     * `is_merge[i]` is true iff the DDCG recorded spine node i as a point where
     * control converges — an if-join (BLOCK/MERGE exit), a loop header (LOOP
     * header) or a loop/switch break target (LOOP/SWITCH exit). THE OPTIMIZER
     * NEVER DERIVES THIS. Every "is this a merge / does this reach that"
     * question is answered here; recomputing it is how a dominance walk grew in
     * the CSE lift. A method with no branches records nothing, and nothing is
     * the COMPLETE record for it — not "unknown". */
    /* …and which of those merges is a LOOP HEADER — the only place spec §5's range
     * widening belongs (§8: "read the loop scope from the sidecar, not a dominator-
     * based natural-loop finder"). Same table, same source: the DDCG recorded the loop
     * it built. Sized merge_rows. */
    bool*     is_loop_header;
    /* Did the DDCG record ANY scope for this method? A hand-built SIR (the unit
     * harness) has none — and then the loop-widener must fail CLOSED and widen at
     * every merge, because "no loop was recorded" and "there is no loop" are not the
     * same statement. Resolved once, beside is_loop_header: the φ meet is a transfer
     * and may not scan the fact table. */
    bool      any_scope_recorded;
    /* §6's throw rule, indexed pre-solve: throw_catches[i] is a bbq_vec of the catch classes
     * of every try region enclosing spine node i (NULL when it is not a Throw, or when no
     * region encloses it). The EXTENT of a region is codegen's rule read off the recorded
     * scopes — never derived here. Sized spine_count. */
    int**     throw_catches;

    /* THE EXCEPTIONAL SUCCESSOR EDGES — spec §1 / JLS §11.3.1.
     *
     * exc_succ[i] is a bbq_vec of the spine indices of the HANDLERS that spine node i can
     * transfer to by throwing (NULL when it cannot throw, or throws into no region here).
     * A handler therefore has more than one predecessor and IS A MERGE, which is the whole
     * point: JLS §11.3.1 says exceptions are precise — "all effects of the statements
     * executed and expressions evaluated BEFORE the point from which the exception is
     * thrown must appear to have taken place" — so the catch must see the state AT the
     * excepting point, not at the region's entry.
     *
     * A CACHE OF THE GRAPH'S OWN FIELDS, nothing more: the exception continuation lives ON
     * the node (`exc`, spec §1's second γ — stamped by the DDCG's patch_excepts when the
     * try rule closes its chain), and cp_hoist_exc_edges lifts a contained expression's γ
     * to its spine node in one field-reading pass. No facts are consulted, and WHICH nodes
     * can throw is never decided here — a "can this node throw" classifier would be a
     * second effect authority, the same disease as a dominator walk (§8).
     *
     * cp_build_pred_csr (⟹ φ placement), liveness, and cp_pack's own liveness all get the
     * edges from the SAME one hoist function over the graph, so no two consumers can hold
     * different control flow. */
    int**     exc_succ;
    int       exc_succ_rows;   /* spine_count when exc_succ was built */
    /* The ONE context. Everything the DDCG recorded for this method is reachable
     * from here; the fields below are what this engine takes from it. A hand-built
     * method (the unit harness) has no context and therefore no recorded facts. */
    compiler_ctx_t* ctx;

    /* THE SIDECAR — every fact the DDCG recorded for this method, all kinds, in
     * record order. ONE table: the SCOPEs the structurer and the loop-widener read,
     * the §15 GUARDs the eliminator prunes, the ALLOC sites that tell a concrete
     * object from a summary, the TRY_REGIONs, the THROW_REGIONs the escape lattice
     * reads. Filter on `.kind`; see the PAYLOAD TABLE in compiler.h.
     *
     * THIS IS WHERE STRUCTURE COMES FROM. The optimizer does not walk the SIR to
     * recover what the frontend already knew — that is a second authority for the
     * same fact, and spec §8 ("Why there is no dominator tree") rules it out.
     * Need something the DDCG knows? RECORD IT: one enum value, one payload row,
     * one record_* call in the grammar. Never a walker. */
    const compiler_fact_t* facts;
    int       fact_count;

    /* Resolved ONCE from the ALLOC rows (cp_index_concrete_objects): is object o ONE
     * concrete runtime object? §2's strong update asks this on every store
     * recompute, and a transfer must be O(its inputs) — so it is a lookup here,
     * never a scan of the facts.
     *
     * FAIL-CLOSED: when the DDCG recorded ANY site for this method, a site missing
     * from the rows is a SUMMARY. Only when nothing at all was recorded (a hand-built
     * SIR, which has no DDCG) is a site taken as concrete — there, no loop can have
     * been lowered, because no lowering happened. */
    bool*     obj_concrete;
    /* The spine_count when is_loop_header was built; an index past it has no row.
     * Same rule as slot_in_rows / reachable_rows. */
    int       merge_rows;


    /* Memory-state side table (stage 1b). A store's state vnode is an INPUT-LESS
     * opaque — pure state identity, with no link to the storing node, the prior
     * cell state, or the stored value — so the heap transfer has nothing to read
     * from the vnode alone. cp_resolve, which already computed the reaching
     * state, records it here instead. Indexed by vnode; rows sized mem_rows
     * (vnodes minted later — Refine nodes — have no row and are never memory).
     *   mem_kind: CP_MEM_NONE / _STORE / _WIDE (an invoke's CP_CELL_ALL kill) /
     *             _SEED (the pre-method contents of a cell)
     *   mem_prev/_obj/_val: vnode ids, or -1. _obj is -1 for a static.
     *   mem_cell: the cell this name is a version OF, or -1 when it is not one cell
     *             (a WIDE kill shadows every cell at once). A SEED needs it to know
     *             which cell's phantom its rows hold.
     * mem_dep_* is the reverse index: which store vnodes must be recomputed when
     * vnode v's pts changes (v is their obj or value operand). Without it a store
     * would never be revisited — it has no def-use edges, having no inputs. */
    signed char* mem_kind;
    int*      mem_prev;
    int*      mem_obj;
    int*      mem_val;
    int*      mem_cell;
    /* The spine node this version is DEFINED by, or -1. The other fields say what a
     * version is; this says where it was written, which is what a rewrite needs to
     * retag the store it came from. Set for CP_MEM_STORE only — a WIDE/KILL version
     * belongs to a call, which is never deletable. */
    int*      mem_spine;
    /* mem_elem: this STORE writes an ARRAY-ELEMENT cell (ArrayStore), not a field. §2's strong
     * update needs one runtime LOCATION, and a concrete receiver only supplies that for a FIELD
     * (one `O.f` per object); an element cell is keyed by element type and summarizes EVERY
     * index of the array, so a store through it is always WEAK — even on a concrete array.
     * (CWZ PLDI'90 / Lhoták&Chung POPL'11: array stores take no strong update absent index
     * must-alias, which this analysis does not model.) Treating the summary-LOCATION cell as
     * strongly updatable made three stores into one array keep only the LAST one's pts — a
     * false singleton that devirtualized a rotating dispatch into a failing cast. */
    bool*     mem_elem;
    int       mem_rows;
    /* Per CELL: can any code write it after the allocation? An invoke kills every cell it
     * COULD write — but it cannot write the array overlay's backing store (written once at
     * allocation, unnameable from Java), so it must not shadow that one. Spec §1: a store
     * reaches a load iff no KILLING store intervenes, and none can. Sized mem_cell_count;
     * lat_is_array_data_cell (the type lattice) is the authority for which cells. */
    bool*     cell_immutable;
    int*      mem_dep_off;
    int*      mem_dep_cnt;
    int*      mem_dep_list;

    /* Widening K-set for cp_const_widen: static type-boundary
     * constants plus the method's integer literals (from LoadConst
     * vnodes), sorted ascending, deduplicated. K cardinality bounds
     * widening chain length per Nielson & Nielson PoPA §4.2; per-
     * method literals let loop-bound widening converge to the
     * method's actual `< N` constants instead of overshooting to
     * the next type boundary. Populated during cp_build. */
    int32_t*      widen_k;
    int           widen_k_count;
} cp_engine_t;

/* Build the value graph and solve, taking EVERYTHING from the ONE context: the
 * method, the sema, the arena, and the method's fact table (scopes, §15 guards,
 * allocation sites, try/throw regions — one table, all kinds). This is the entry
 * point the compiler uses.
 *
 * Facts go IN THE CONTEXT and the engine takes what it needs FROM it — so adding
 * the next recorded fact never changes this signature. The table is passed
 * explicitly only because the synthesized <clinit> is not in `all_facts[]` (it has
 * no method_idx); everything else comes off the ctx. (`cp_build` below is the test
 * harness's entry: a hand-made sir_method_t, for which there is no context.) */
cp_engine_t* cp_build_ctx(compiler_ctx_t* ctx, sir_method_t* method,
                          const compiler_fact_t* facts, int fact_count);

/* ── The published analysis facts (spec §8.1.1) ──────────────────────────────
 *
 * Click §4.10 applies results by walking the SOLVED partitioning — O(N)+O(E) — so the solved
 * facts must outlive the engine. These are exactly the ones the application phase reads.
 * What it computes for itself (reachability, liveness, the canonical φ choice, the scalar
 * substitution) is not here, and must not be added. */
typedef struct {
    int         partition;      /* congruence class — §4.10's "partition" */
    int         leader;         /* -1 = Leader; else the vnode this one follows (§4.8) */
    cp_const_t  constant;       /* the solved constant-propagation fact */
    const Type* type;           /* τ̂, the type-lattice fact */
    cp_pts_t    pts;            /* lattice A; bits arena-copied, NULL bits == ∅ */
    uint8_t     kind;           /* cp_vn_kind_t: EXPR / PHI / REFINE / OPAQUE */
} compiler_click_vfact_t;

struct compiler_click_facts {
    bool computed;              /* false = this method was never analyzed (a §7 bottom method) */
    int  vnode_count;
    int  obj_count, obj_first_site, obj_words;

    /* Executable-edge reachability, [spine_count]. cp_compute_reachability runs INSIDE
     * cp_solve's loop — it is an element of the fixpoint (§3.7's UCE+CCP), so it is solve
     * output like any other lattice fact, not something a consumer re-derives. */
    int   spine_count;
    bool* reachable;
    int   reach_count;

    compiler_click_vfact_t* v;  /* [vnode_count] */

    /* Per abstract object: the §6 escape state and the object model the scalar-replacement
     * consumer and the census walk. */
    uint8_t* obj_escape;        /* [obj_count], cp_escape_t */
    bool*    obj_alloced;       /* [obj_count] */
    int*     vnode_of_obj;      /* [obj_count] */

    /* The index without which none of the above is addressable: SIR node -> vnode+1.
     * Owned here (the engine's own map dies with it). */
    cp_pmap_t expr_idx;
};

/* Publish `eng`'s solved facts for `method_idx` into ctx->click_facts. Called at the point
 * the analysis finishes — before anything transforms the graph, per Choi §4.2 (the summary,
 * and by the same argument every other solved fact, is a function of the ANALYSIS). */
void cp_publish_facts(compiler_ctx_t* ctx, int method_idx, cp_engine_t* eng);

/* The published facts for a method, or NULL if it was never analyzed. */
const struct compiler_click_facts* compiler_click_facts_of(const compiler_ctx_t* ctx,
                                                           int method_idx);

/* The vnode index a SIR node was solved as, or -1. The application phase's entry point into
 * the facts — every per-node lookup goes through it. */
int compiler_click_vnode_of(const struct compiler_click_facts* f, const sir_node_t* e);

/* Build the graph for `method` and LOAD `pf` into it in place of solving — what the
 * application phase runs on. NULL if the shapes disagree (the caller then solves).
 *
 * The engine is built in `scratch`, which the CALLER frees once it is done with the engine:
 * that is the method lifetime. Only the SIR the rewrite mints escapes, into ctx->arena. */
cp_engine_t* cp_build_ctx_loaded(compiler_ctx_t* ctx, sir_method_t* method,
                                 const compiler_fact_t* facts, int fact_count,
                                 const struct compiler_click_facts* pf,
                                 bbq_arena* scratch);

/* Build the value graph for `method`: collect the spine, enumerate
 * every reachable expression node, then resolve operand edges —
 * tree children directly, LoadLocal operands to their reaching
 * definitions, with a φ node at each control-flow merge, then place
 * every node in an initial congruence partition.
 * Returns NULL iff method or its entry is NULL. */
cp_engine_t* cp_build(sir_method_t* method, const sema_ctx_t* sema,
                       bbq_arena* arena,
                       const compiler_fact_t* facts, int fact_count);

/* Test-only: same as cp_build but stops BEFORE the outer
 * PROPAGATE+CAUSE_SPLITS solve. Lets tests inspect the engine in its
 * pre-solve state (partitions assigned, but facts uninitialized).
 * Subsequently call cp_init_facts then cp_solve to complete. */
cp_engine_t* cp_build_no_solve(sir_method_t* method, const sema_ctx_t* sema,
                                bbq_arena* arena,
                                const compiler_fact_t* facts, int fact_count);

/* Click §4.4.2: one-time fact init. Sets every vnode's type to TOP
 * and constant to CP_C_TOP, then enqueues every vnode onto its
 * partition's cprop list (and the partition onto the global cprop
 * worklist). Called once from cp_solve before the outer
 * PROPAGATE+CAUSE_SPLITS loop. Test-visible so unit tests can verify
 * the post-init engine state. */
void cp_init_facts(cp_engine_t* eng);

/* Test-only: run the outer PROPAGATE+CAUSE_SPLITS solve loop on an
 * already-built engine. Assumes cp_init_facts was called. */
void cp_solve(cp_engine_t* eng);

/* Is the engine AT a fixpoint? Arms every node and runs one more round: a fixpoint is
 * idempotent under recomputation, so anything that moves is a value the solve left stale
 * because its transfer read a fact off the def-use graph.
 *
 * ONE authority — the JAVELINA_VERIFY_FIXPOINT diagnostic and the test suite both call this,
 * so a pin cannot pass against a weaker check than the one that reports. Advances the engine
 * state; call it after cp_solve and do not reuse that engine for codegen. */
bool cp_at_fixpoint(cp_engine_t* eng);

/* Release the bbq_vec and bbq_htree tables held by the engine; the
 * arena-allocated nodes are freed with the arena. */
void cp_free(cp_engine_t* eng);

/* ── Points-to queries (lattice A) ──
 * `obj` is an Obj id (CP_OBJ_NULL / CP_OBJ_EXT / an allocation's id).
 * cp_pts_has: is `obj` in the set? cp_pts_empty: is the set ⊥ (∅)?
 * cp_pts_count: |set| — a singleton is what licenses a strong update.
 * cp_obj_of: the Obj id an allocation EXPRESSION names, or -1. */
bool cp_pts_has(const cp_engine_t* eng, cp_pts_t s, int obj);
bool cp_pts_empty(const cp_engine_t* eng, cp_pts_t s);
int  cp_pts_count(const cp_engine_t* eng, cp_pts_t s);
int  cp_obj_of(const cp_engine_t* eng, const sir_node_t* alloc);
/* The pts fact on the vnode for expression `e` (∅ if `e` has no vnode). */
cp_pts_t cp_pts_of_expr(const cp_engine_t* eng, const sir_node_t* e);

/* The escape state of Obj `obj` (GlobalEscape for an id out of range — fail closed). */
cp_escape_t cp_escape_of(const cp_engine_t* eng, int obj);
/* …of the object an allocation EXPRESSION names. */
cp_escape_t cp_escape_of_expr(const cp_engine_t* eng, const sir_node_t* alloc);

/* ── §5, Lattice D: integer-range operators (exposed for unit tests) ──
 * The join (interval hull), the narrowing meet (intersection), and the widening
 * operator — pinned directly so each PoPA §4.2 piece is verified in isolation. */
cp_const_t cp_const_meet(cp_const_t a, cp_const_t b);
cp_const_t cp_const_intersect(cp_const_t a, cp_const_t b);
cp_const_t cp_const_widen(const cp_engine_t* eng, cp_const_t old, cp_const_t new_val);

/* ── §3, lattice B: `τ̂(v) = ⨆_{O ∈ pts(v)} exactClassOf(O)` ──
 * DERIVED from pts and computed ON DEMAND — there is no τ̂ field, because a stored copy
 * is the second type domain §3 forbids and would drift from pts the moment a cast
 * refines it. BOTTOM when any object's class is unknown (fail closed); TOP when nothing
 * reaches. The returned Type is interned in the ENGINE's pool and dies with it. */
const Type* cp_tau_of_vnode(cp_engine_t* eng, int vi);
const Type* cp_tau_of_expr(cp_engine_t* eng, const sir_node_t* e);

/* Compute backward slot liveness over the spine — Kildall worklist
 * over per-slot bitvectors, monotone on a finite lattice, runs once.
 * Uses `eng->reachable` for precision: dead spine nodes don't
 * propagate liveness. Populates `eng->live_in` and `eng->live_out`. */
void cp_compute_liveness(cp_engine_t* eng);

/* Rewrite the method's SIR once, off the converged engine state.
 * Mutates spine and expression trees in place: KNOWN-constant nodes
 * become LoadConsts; Followers emit their Leader; dead StoreLocals
 * (per liveness) drop; unreachable arms (per reachability) prune to
 * Goto. Single pass — no iteration. */
void cp_rewrite(cp_engine_t* eng);

/* Slot bin-packing post-pass. Runs after cp_rewrite has mutated the
 * SIR. Re-collects the post-rewrite spine, computes backward slot
 * liveness, builds a slot-interference graph, greedy-colors slots
 * within per-valtype pools (every WASM local is one index, so all
 * pools are width 1), and rewrites every slot reference in place.
 * Anchors caller-set
 * parameter cells at their original positions.
 * Sets method->max_locals to the minimum frame size. Independent of
 * cp_engine_t — cp_rewrite has freed it by here. */
/* The EXCEPTIONAL edges (spec §1) come off the graph's own `exc` fields — cp_pack hoists
 * them over its post-rewrite node list with the same one function the engine used, so a
 * slot whose only consumer is a catch block stays live at every excepting node and the
 * packer cannot see different edges than the analysis did. */
void cp_pack(sir_method_t* method, const sema_ctx_t* sema,
              bbq_arena* arena, int initial_max_locals);

#endif
