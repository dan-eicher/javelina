/* jav_eqsat.h — tier-3: equality saturation over the tier-2 tree.
 *
 * Tier-3 IS tier-2 with this pass inserted between jav_ttree_build and the
 * burg reduce: per region, the pure subtrees intern into an e-graph, the
 * generated rule set saturates it (src/gen/jav_rewrite.h, from
 * spec/jav_axioms.burg), and each root extracts the cheapest equal term.
 * An extraction identical to the original keeps the ORIGINAL subtree,
 * pointer and all, so with zero rules tier-3 is tier-2 STRUCTURALLY — the
 * same trees, the same reduce, the same code, which the suite gates as a
 * byte-for-byte identity rather than trusting the construction.
 *
 * Fail closed, engine aborts never: any refusal — caps, an intern the
 * fence rejects, an extraction that cannot be verified — keeps the original
 * for that region and is COUNTED. A tier-3 decline is correct and therefore
 * silent and therefore metered.
 */
#ifndef JAV_EQSAT_H
#define JAV_EQSAT_H

#include "jav_ttree.h"
#include "bbq_hmap.h"

/* Every counter is printed by the tier-3 stats block and gated: rewritten is
 * an identity (0 while the rule set is empty; a recorded baseline after),
 * refusals are legal and counted, and the internal-identity failure is the
 * one that must be zero at ANY rule set — an extraction that neither matched
 * the original nor rebuilt is the pass disagreeing with itself. */
typedef struct {
    uint64_t bodies;            /* bodies the pass ran over */
    uint64_t regions;           /* regions interned */
    uint64_t roots;             /* roots extracted */
    uint64_t rewritten;         /* roots whose extraction differed AND rebuilt */
    uint64_t cap_refusals;      /* regions dropped at the node budget */
    uint64_t rebuild_refusals;  /* differing extractions the rebuild refused
                                 * (splice version, an impure or carried drop,
                                 * reordered originals) — original kept, counted */
    uint64_t identity_fails;    /* extraction != original with nothing to rebuild it */
    uint64_t enodes_peak;       /* largest post-saturation graph any region reached */
} jav_eqsat_stats_t;

const jav_eqsat_stats_t* jav_eqsat_stats(void);
void                     jav_eqsat_stats_reset(void);

/* A synthesized node's stamp record: the emitter reads it when a node has no
 * pc. Two immediates cover the whole vocabulary — a scalar const carries its
 * value, an extracted local its slot, a lane op its lane, and a 16-byte
 * vector immediate (v128.const, i8x16.shuffle) rides both halves exactly as
 * the byte decode feeds the stencil's two raw-8-byte holes. `prefixed` says
 * op is 0xFD and `sub` the sub-opcode. */
typedef struct {
    uint8_t  op;
    uint8_t  prefixed;
    uint32_t sub;
    int64_t  imm;
    int64_t  imm2;
} jav_synth_t;

/* Run the pass over one body's tree, IN PLACE: a root whose extraction is
 * cheaper and rebuildable is replaced (tree->nnodes adjusted so the picks
 * identity holds); every refusal keeps the original and is counted. `a` is
 * the compile's arena; `synth` is the emitter's per-body sidecar — rebuilt
 * nodes register their jav_synth_t records there. */
void jav_eqsat_body(jav_ttree_t* tree, const jav_tctx_t* tcx, bbq_arena* a,
                    bbq_hmap* synth);

/* The per-rule fire counts the generated round maintains (a fire = one
 * saturation round in which that rule added information), for the wins-by-
 * family meter. Returns the rule count; the arrays are the generated
 * matcher's own. */
int jav_eqsat_rule_stats(const char* const** names,
                         const unsigned long long** fires);

#endif /* JAV_EQSAT_H */
