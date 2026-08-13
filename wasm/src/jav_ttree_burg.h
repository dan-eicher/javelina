// jav_ttree_burg.h — the contract burgc's generated tile matcher compiles against.
//
// BURG_NODE_* is a fixed set of names, so defining it is a claim on the whole
// translation unit: only ONE burg client can be in scope at a time. The compiler has
// its own (sir_support.h, over sir_node_t), and these two must never meet. They did:
// this block lived in jav_ttree.h, which jav_module_index.h includes for jav_tctx_t
// alone, so every TU reaching the module index — including compiler-side tests that
// legitimately hold both a sir tree and the VM's loader — inherited a redefinition
// against a different node type, and -Wmacro-redefined stopped the build.
//
// The type header is now just types. This header is the adapter, included only where
// the matcher is actually instantiated: jit_driver.c and the generated jav_tile.c.
#ifndef JAV_TTREE_BURG_H
#define JAV_TTREE_BURG_H

#include "jav_ttree.h"

// The operator IS the resolved signature — that is the whole point of the vocabulary —
// and a region is a TREE, so there are no successor edges: the root list is a sequence
// the caller walks, not a spine the matcher threads.
/* What a generated `where` clause reads: the slots kid `i`'s subtree needs to
 * evaluate. A rule states shape; a quantity comes from the analysis. */
#define JAV_TNEED(n, i)         ((int)(n)->kids[i]->need)

#define BURG_NODE_TYPE          jav_tnode_t*
#define BURG_NODE_OP(n)         ((n)->sig)
#define BURG_NODE_ARITY(n)      ((n)->nkids)
#define BURG_NODE_CHILD(n, i)   ((n)->kids[i])
#define BURG_NODE_ID(n)         ((void*)(n))
#define BURG_NODE_SUCC_COUNT(n) (0)
#define BURG_NODE_SUCC(n, i)    ((jav_tnode_t*)0)

#endif /* JAV_TTREE_BURG_H */
