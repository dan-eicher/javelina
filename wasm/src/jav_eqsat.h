/* jav_eqsat.h — tier-3: equality saturation over the tier-2 tree.
 *
 * Tier-3 IS tier-2 with this pass inserted between jav_ttree_build and the
 * burg reduce (the plan's D1): per region, the pure subtrees intern into an
 * e-graph, the generated rule set saturates it (src/gen/jav_rewrite.h, from
 * spec/jav_axioms.burg), and each root extracts the cheapest equal term.
 * An extraction identical to the original keeps the ORIGINAL subtree,
 * pointer and all, so with zero rules tier-3 is tier-2 STRUCTURALLY — the
 * same trees, the same reduce, the same code (PIN B-1).
 *
 * Fail closed, engine aborts never (D7): any refusal — caps, an intern the
 * fence rejects, an extraction that cannot be verified — keeps the original
 * for that region and is COUNTED. A tier-3 decline is correct and therefore
 * silent and therefore metered.
 */
#ifndef JAV_EQSAT_H
#define JAV_EQSAT_H

#include "jav_ttree.h"

/* Every counter is printed by the tier-3 stats block and gated: rewritten is
 * an identity (0 while the rule set is empty; a recorded baseline after),
 * refusals are legal and counted, and the internal-identity failure is the
 * one that must be zero at ANY rule set — an extraction that neither matched
 * the original nor rebuilt is the pass disagreeing with itself. */
typedef struct {
    uint64_t bodies;            /* bodies the pass ran over */
    uint64_t regions;           /* regions interned */
    uint64_t roots;             /* roots extracted */
    uint64_t rewritten;         /* roots whose extraction differed (0 at zero rules) */
    uint64_t cap_refusals;      /* regions dropped at the node budget */
    uint64_t identity_fails;    /* extraction != original with nothing to rebuild it */
} jav_eqsat_stats_t;

const jav_eqsat_stats_t* jav_eqsat_stats(void);
void                     jav_eqsat_stats_reset(void);

/* Run the pass over one body's tree, in place. Regions are independent
 * graphs; `a` is the compile's arena (version maps live there). Always
 * returns with the tree valid for the reduce — a refusal anywhere keeps the
 * original subtrees for that region. */
void jav_eqsat_body(const jav_ttree_t* tree, const jav_tctx_t* tcx, bbq_arena* a);

#endif /* JAV_EQSAT_H */
