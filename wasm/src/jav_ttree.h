/*
 * jav_ttree.h — the tier-2 tree IR: one forward pass over a function body that
 * turns the byte stream into the trees burg tiles.
 *
 * The operand stack is not a data structure the compiler has to keep: pushing a
 * NODE POINTER instead of a value (Ertl §3.2.1) makes the producer/consumer
 * edges explicit, and what comes out is a tree per value the region computes.
 * Wasm has no dup/over/swap/rot, so every stack value is consumed exactly once
 * and the result is a true tree, never a DAG.
 *
 * Deliberately NOT the validator. This walk consumes the same opgen-generated
 * tables the validator's transfer functions come from (jav_opcode_sig[],
 * jav_jit_meta[]), so the two cannot disagree about an opcode's arity unless the
 * generator changes under both — which is why this header does not include
 * validate.h and this walk re-checks nothing the module already passed.
 */
#ifndef JAV_TTREE_H
#define JAV_TTREE_H

#include <stdint.h>

#include "bbq_arena.h"
#include "bbq_lite.h"     /* bbq_ctx_t — the same cursor the interpreter reads with */
#include "jav_sigtab.h"

/* §3.1 The node — GENERATED from the schema (src/gen/jav_tnode.h, by asdl over
 * src/gen/jav_ttree.asdl through spec/templates/jav_tnode.inja), so the IR the
 * builder allocates and the schema burgc checks the tiling grammar's coverage
 * against are ONE artifact reaching C twice. PIN B-6 (test_ttree) compares the
 * two ends name-for-name at every id; sigemit numbers the finals in the schema's
 * own declaration order, so `sig` is simultaneously the jav_sigtab index, the
 * burg TERM and the generated tag value.
 *
 * What rides on the node beyond its tree shape is declared as the schema's
 * `attributes(...)`: `need` (cache slots this subtree needs to evaluate — a rule
 * states shape and cannot state a quantity, so a guard reads this), `seq` (the
 * instruction's flat §5 ordinal, JAV_TNODE_NO_SEQ for a carried leaf), and `pc`
 * (the instruction's first byte — the stitcher's payload, which burg never
 * reads). */
#include "jav_tnode.h"

/* §3.3 A region: the run between two cuts. Its roots are the nodes nothing
 * inside the region consumed — the effectful ones, and whatever the region left
 * on the stack — in stack order. */
typedef struct {
    jav_tnode_t** roots;     /* arena-allocated, nroots long */
    uint32_t      nroots;
    uint32_t      start;     /* byte offset of the region's first instruction */
} jav_tregion_t;

typedef struct {
    jav_tregion_t* regions;
    uint32_t       nregions;
    uint32_t       nnodes;   /* every node built, for the arity invariant */
} jav_ttree_t;

/* The facts §3.2's resolve() reads, as storage CLASSES (JSC_*) — projected by
 * the caller from the module index, so the builder never sees the validator's
 * type model. Every array is indexed by the spec's own index space.
 *
 * A local's or a global's class is §3.4.2/§3.4.3's declared type; a struct
 * field's and an array element's are §2.3.9's composite structure; a memory's
 * and a table's addrtype width is §2.3.11. The *_nparams arrays are the pop
 * counts the variadic instructions name (§3(b)), which the builder needs to know
 * how many nodes an argument group takes off the abstract stack. */
typedef struct {
    const uint8_t*        local_class;    uint32_t nlocals;   /* params then declared */
    const uint8_t*        result_class;   uint32_t nresults;  /* this function's own */
    const uint8_t*        global_class;   uint32_t nglobals;
    const uint8_t*        mem_is64;       uint32_t nmems;
    const uint8_t*        table_is64;     uint32_t ntables;
    /* Per typeidx: the composite structure (§2.3.9) and, for a functype, the
     * parameter and result classes a blocktype or a call takes its shape from. */
    const uint8_t* const* field_class;       /* [ntypes][nfields[t]] */
    const uint32_t*       nfields;           /* [ntypes] */
    const uint8_t*        elem_class;        /* [ntypes] */
    const uint8_t* const* type_param_class;  /* [ntypes][type_nparams[t]] */
    const uint32_t*       type_nparams;      /* [ntypes] */
    const uint8_t* const* type_result_class; /* [ntypes][type_nresults[t]] */
    const uint32_t*       type_nresults;     /* [ntypes] */
    uint32_t              ntypes;
    const uint32_t*       func_type_idx;  uint32_t nfuncs;
    const uint32_t*       tag_type_idx;   uint32_t ntags;
    /* The JIT tier this compile serves (the embedder's jit_enabled, clamped
     * [0,3]). The builder ignores it; jit_compile reads it to run the tier-3
     * rewrite (jav_eqsat_body) between build and reduce. 0/2 mean tier-2
     * machinery alone — a zeroed context stays exactly what it was. */
    uint8_t               tier;
} jav_tctx_t;

/* Build the regions of the body at `code` (positioned at the first instruction,
 * past the locals vector). Every allocation comes from `arena`, so the whole
 * tree frees with one reset.
 *
 * Returns 1 on success. Returns 0 when the walk meets something it cannot build
 * — an index the context does not cover, an opcode whose signature does not
 * resolve, an operand the abstract stack does not hold. That is a decline, not a
 * diagnosis: the module is already §7-valid, so a 0 means THIS walk is short a
 * fact, and the caller keeps the tier it already had. */
int jav_ttree_build(bbq_ctx_t code, const jav_tctx_t* ctx,
                    bbq_arena* arena, jav_ttree_t* out);

/* What the builder has seen since the process started. `arity_mismatches` is PIN
 * B-4's claim; the built/declined split is what keeps it from being vacuous — an
 * invariant over zero nodes is green for the wrong reason, so a sweep has to
 * report how much it actually built. `first_decline_*` names the instruction the
 * first declining body stopped on. */
typedef struct {
    uint64_t bodies_built, bodies_declined, nodes;
    uint64_t bodies_covered, bodies_uncovered;   /* the tiling's own verdict */
    uint64_t nodes_picked, nodes_unpicked;       /* did a rule fire for each? */
    uint64_t nodes_carried;                      /* leaves: no instruction to stamp */
    /* What the tier actually costs: bytes of machine code stamped, over how many
     * bodies. This is the number the cost model minimises, so it is the one that
     * says whether the cover is buying anything. */
    uint64_t code_bytes, code_bodies;
    int      have_uncovered;
    int      first_uncovered_sig;                /* the terminal burg had no rule for */
    uint64_t arity_mismatches;
    /* Regions whose roots, walked in order and each in postorder, did NOT visit
     * the instructions in byte order — the invariant a per-tree cover rests on. */
    uint64_t order_breaks;
    /* What the stitcher actually did. Every claim about cache states is true of a
     * machine with no cache, so a green says nothing until these are non-zero:
     * `states_cached` counts instructions that ran with something in a register,
     * `transitions` the spills and fills stamped between them, and `bridge_fails`
     * the gaps it could not close (a decline, not a miscompile — but a silent one
     * would leave the tier reporting a coverage it did not deliver). */
    uint64_t states_cached, transitions, bridge_fails;
    /* …split by kind and by whether any rule had a say. Inside a region the cover
     * can push inline instead of caching and spilling (D7s), and the DP prices
     * both, so a spill there is one it chose. A transition at a REGION BOUNDARY
     * is chosen by nothing: the next instruction belongs to another tree or to
     * none (a branch target, a resync after a call, dead code), and the stitcher
     * reconciles the two states with no rule having costed it. */
    uint64_t trans_spill, trans_fill, trans_boundary;
    /* THE MEASURE: operand-stack SLOTS the stamped code touches — every GPUSH and
     * every GPOP it will execute, counted once each, whichever stencil performs
     * it. This is the quantity stack caching removes, and the one to compare
     * cache sizes on.
     *
     * Neither counter above answers it. `states_cached` counts INSTRUCTIONS that
     * ran with an operand in a register, and an add reading two registers counts
     * once while saving twice. `transitions` counts extra STENCILS, hence extra
     * tail-calls — and the tail-call is the only patch point between templates,
     * so it joins any two of them whatever the cache holds. The GPUSH inside a
     * spill is the same one the all-memory form does inline, so a transition
     * relocates traffic rather than adding it. */
    uint64_t mem_slots;
    /* …and how many ran DEEPER than the first slot. Separate from states_cached
     * because a cache of any size keeps that one busy: at n>=2 the slots above it
     * are only reachable if a value can move down one as something is pushed
     * above it, and a zero here says they are dead however many were generated. */
    uint64_t states_deep;
    /* Cache OCCUPANCY: each instruction's entry state, summed. A value that sits
     * in a register across five instructions adds five, having been read at most
     * once — so this is register-slots-per-instruction, not slots read.
     *
     * It was documented as "slots read from a register", and on that reading
     * `slots_cached * 2 - transitions` was called the net accesses avoided. Both
     * halves are wrong. The code sums `entry`, which is occupancy; and mem_slots
     * above already says a transition RELOCATES traffic rather than adding it, so
     * subtracting one from the other prices the same access twice. mem_slots is
     * the quantity stack caching removes; this one is how full the cache ran. */
    uint64_t slots_cached;
    /* Which gap the first one was: the instruction it followed, the states it
     * had to get between, and the class it had in hand. A bare count says the
     * stitcher gave up somewhere; this says where, which is the difference
     * between a work list and a mystery. */
    int      have_unbridged;
    uint8_t  first_unbridged_op;
    uint32_t first_unbridged_off;
    int      first_unbridged_from, first_unbridged_to, first_unbridged_cls;
    int      have_decline;
    uint8_t  first_decline_op;
    uint32_t first_decline_sub;
    /* Which instruction each declining body stopped on. A decline is a body that
     * stays on the tier below, so this is the work list for widening coverage —
     * a bare count says how much is missing but not what. */
    uint32_t decline_op[256];
    /* Entry states, as a histogram rather than the two thresholds above.
     * `states_cached`/`states_deep` are its partial sums and stay for their
     * existing consumers; what they cannot say is "an instruction ran at state 4
     * exactly", which is what a v128 pair needs proving and what a fixture's
     * expected cover reduces to. Index clamps at the last bucket. */
    uint64_t entry_state[9];
    /* Stamps whose rule named a v128 in a register (operand or result). The class
     * axis the counters above are blind to — v128 caching shipped once with no
     * fixture able to see it, which is why this is a counter and not a comment. */
    uint64_t wide_cached;
    /* Regions whose first stamp found the machine's cache non-empty. The offmap
     * argument leans on region entries being canonical, so a non-zero here is a
     * fact to explain, not a curiosity. */
    uint64_t regions_hot;
    /* Bodies whose reduce-driven walk failed mid-emission and re-stamped through
     * the plain byte walk. Correct (D8's fallback) and therefore silent and
     * therefore counted — with the first failure named, because a count says how
     * much coverage was lost and not why. `why`: 1 capacity, 2 read, 3 no
     * stencil, 4 no variant at the entry state, 5 no exit state, 6 unbridged. */
    uint64_t tree_fallbacks;
    int      have_fallback;
    uint8_t  first_fallback_op;
    uint32_t first_fallback_bpos;
    int      first_fallback_entry, first_fallback_why;
    /* Stamps whose OPCODE provided no form at the rule's state, so the stitcher
     * descended to the nearest one it does provide (the C5 contract) and the
     * bridge spilled the difference — slots the cover never priced.
     * `descend_slots` is that summed deficit, the exact unpriced quantity; the
     * first one is named because a count is a total, not a work list. A descend
     * is LEGAL, so the gate on these is the meter's own identity
     * (descend_slots >= descends, both zero together) and a recorded baseline,
     * never a zero. */
    uint64_t descends;
    uint64_t descend_slots;
    int      have_descend;
    uint8_t  first_descend_op;
    uint32_t first_descend_sub;      /* sub-opcode when op is a prefix (0xfb/0xfc/0xfd), else 0 */
    int      first_descend_from, first_descend_to;
} jav_ttree_stats_t;

const jav_ttree_stats_t* jav_ttree_stats(void);
void                     jav_ttree_stats_reset(void);
/* One stamp descended from the rule's state `from` to the nearest state its
 * opcode provides, `to` — the bridge spills the difference (see the stats
 * fields above). `sub` is the sub-opcode when `op` is a prefix byte, else 0. */
void jav_ttree_note_descend(uint8_t op, uint32_t sub, int from, int to);
/* Point the EMISSION notes (stitch/transition/mem/wide/region-entry) at a
 * per-compile accumulator, or back at the globals (NULL). An emission can be
 * abandoned — a walk that fails mid-body re-stamps through the other one — and
 * meters that counted the abandoned half describe code that never shipped. */
void jav_ttree_stats_sink(jav_ttree_stats_t* s);
/* Fold a committed walk's accumulator into the globals. */
void jav_ttree_stats_commit(const jav_ttree_stats_t* d);

/* Record the tiling's verdict for one body. `sig` is burg's error argument — the
 * terminal it had no rule for — meaningful only when covered is 0. */
void jav_ttree_note_cover(int covered, int sig);

/* ── the stamping action ───────────────────────────────────
 *
 * The cover picks a rule per node, a rule is a (signature, cache state) pair,
 * and the rule's ACTION is where the stencil is stamped: burg matches the tree
 * and emits from the reduce, which is the design the generated matcher's own
 * header states ("label a tree, then … drive burg_reduce toward a goal").
 * There is no offset-keyed map between the cover and the emitter anymore — the
 * map existed to join a tree cover to a byte-driven stamping walk, and the walk
 * that consumed it survives only as tier-1 and as the decline fallback (D8),
 * where every stencil is the plain form and no state exists to record. */
/* ── what a cover costs (Ertl §2.6, printed 36) ─────────────
 *
 *   "the components have to be weighed and added. We used the following weights:
 *    loads, stores, moves and stack pointer updates cost one cycle, instruction
 *    dispatches cost four cycles."
 *
 * Cycles, not bytes. Stack caching removes memory traffic, so memory traffic is
 * what a cost model for it has to count, and a dispatch — the thing an extra
 * stencil ADDS — is worth four of them. A tiling cost model built on stencil
 * `code_size` sees the first only roughly and the second not at all, so it buys
 * small code by spending dispatches, which is backwards.
 *
 * Applied to a copy-and-patch backend, where a transition IS an extra stencil and
 * therefore an extra dispatch:
 *
 *   an operand or result in memory      1   (the load or the store)
 *   the sp update, if it touches memory 1
 *   a spill or a fill                   5   (its memory access + its dispatch)
 *   the survivor shift                  0   (a move the consuming stencil already
 *                                            performs; no dispatch of its own)
 *
 * An instruction's OWN dispatch is not counted: it happens whichever variant
 * runs, so it is equal across every cover of the same tree and cancels. That is
 * also what keeps these numbers single-digit, which an additive cost over a tree
 * needs — the footnote to §2.3.1 makes the same point about shifting the scale
 * into a finite range. */
#define JAV_COST_MEM       1   /* a load, a store, a move, an sp update */
#define JAV_COST_DISPATCH  4   /* …and what an extra stencil costs on top */
/* CAVEAT on the 4, recorded because it was imported as though settled. Ertl's
 * dispatch is a THREADED INTERPRETER'S — an indirect jump through a table on a
 * MIPS R3000, routinely mispredicted. Ours is a copy-and-patch `musttail`: a
 * DIRECT jmp whose target was patched at compile time, with no table load and no
 * indirect branch. It is plausibly 1 rather than 4, so this overcharges a
 * transition by around 4x.
 *
 * Selecting the cache size does not rest on it. n=1 -> n=2 costs 54 transitions
 * per additional cached use, and every transition is at minimum one memory access
 * — the thing being avoided — before any jump is counted; set the dispatch weight
 * to ZERO and n=2 still loses by 54x. What the weight does decide is marginal
 * choices WITHIN an n, such as C-4's cache-then-spill where 1+w against 2 flips
 * at w=1. Those are the claims not to make precisely until a dispatch is measured
 * on THIS backend instead of borrowed from that one. */
#define JAV_COST_TRANSITION (JAV_COST_MEM + JAV_COST_DISPATCH)

/* The classes this instruction expects to FIND in the cache and the one it LEAVES
 * in slot 0, packed per slot — slot 0 in the low bits, slot 1 above it. Packed
 * rather than one argument per slot because this action is generated into the
 * grammar and its signature must not change with the cache size.
 *
 * `in` names the cached OPERANDS, which the signature knows. `out` names only
 * what this instruction PRODUCES — a survivor's class came from whatever put it
 * there, which this rule cannot see, so the emitter carries those along the
 * reduce. A transition reads the side it is on: a spill moves the deepest cached
 * value, whose class the emitter is carrying; a fill loads what the instruction
 * being entered asked for, which is its own `in` pack. */
/* Four bits, not three: the field has to hold JSC_COUNT — the "no class here"
 * answer — and that is 9, because the enum carries the non-final classes (STK,
 * ADDR, POLY) after the six real ones. Three bits truncate it to 1, which is
 * JSC_I64, so every empty slot read back as a cached i64 and the emitter stamped
 * spills and fills for values that were never there. */
#define JAV_TILE_CLS_BITS 4
#define JAV_TILE_CLS_MASK 15u
_Static_assert(JSC_COUNT <= JAV_TILE_CLS_MASK, "a class must fit its packed field");
/* THE generated rule action: stamp this node's stencil variant for (state, packs),
 * bridging the machine's carried cache state to `state` first. Implemented by the
 * driver (jit_driver.c), which owns the emission context the reduce runs inside;
 * a call outside a driver-armed reduce is a bug and stamps nothing. */
void jav_t2_stamp(const jav_tnode_t* n, int state, uint32_t in_pack, uint32_t out_pack);
/* Per body: how many nodes the cover spoke for, against how many it was given.
 * A carried leaf carries no instruction, so it is expected to go unpicked. */
void jav_ttree_note_picks(uint32_t picked, uint32_t nodes);
void jav_ttree_note_code(uint64_t bytes);
/* One instruction the emitter placed: the state it ran in, how many transitions
 * were stamped at its seam, and whether a gap was left unbridged. */
void jav_ttree_note_stitch(int entry, int transitions, int bridge_failed);
/* One stamp whose rule put a v128 in a register (either pack names JSC_V128). */
void jav_ttree_note_wide(void);
/* A body whose reduce-driven walk failed and re-stamped plain (D8). */
void jav_ttree_note_fallback(uint8_t op, uint32_t bpos, int entry, int why);
/* A region's first stamp, with the cache state the machine carried in. */
void jav_ttree_note_region_entry(int live_state);
/* One transition stamped: `down` is a spill (a fill otherwise), `boundary` says
 * the next instruction carried no tile of its own, so no rule costed this. */
void jav_ttree_note_transition(int down, int boundary);
/* Slots this stencil moves between a register and the operand stack. */
void jav_ttree_note_mem(int slots);
/* Name the gap the stitcher could not close. */
void jav_ttree_note_unbridged(uint8_t op, uint32_t off, int from, int to, int cls);

/* The burg matcher's adapter — BURG_NODE_* and JAV_TNEED — is jav_ttree_burg.h.
 * Those names are a claim on the whole translation unit and only one burg client
 * can hold them, so this header stays types-only: jav_module_index.h includes it
 * for jav_tctx_t, which puts it in every TU that touches the module index. */

/* The class a node produces, or JSC_COUNT for a node with no result. */
static inline uint8_t jav_tnode_class(const jav_tnode_t* n) {
    const jav_sig_t* s = &jav_sigtab[n->sig];
    return s->nresults ? s->results[0] : (uint8_t)JSC_COUNT;
}

#endif /* JAV_TTREE_H */
