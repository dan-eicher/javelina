/*
 * wat_layout_burg.h — the burg adapter over water's render-tree node.
 *
 * BURG_NODE_* is a fixed set of names, so defining it is a claim on the whole
 * translation unit, and only one burg client may hold it (src/jav_ttree_burg.h
 * records the same constraint for the tiler). The generated wat_layout.c is
 * that client here; nothing else includes this header.
 *
 * wat_rec is the rule actions' one entry point: every rule records the id the
 * cover chose for its node. Actions fire in postorder — children before the
 * parent — so the recording cannot print; the emitter walks the tree top-down
 * afterwards, reading the recorded rule per node.
 */
#ifndef WAT_LAYOUT_BURG_H
#define WAT_LAYOUT_BURG_H

#include "wat_tnode.h"

#define BURG_NODE_TYPE          wat_tnode_t*
#define BURG_NODE_OP(n)         ((n)->tag)
#define BURG_NODE_ARITY(n)      ((n)->nkids)
#define BURG_NODE_CHILD(n, i)   ((n)->kids[i])
#define BURG_NODE_ID(n)         ((void*)(n))
#define BURG_NODE_SUCC_COUNT(n) (0)
#define BURG_NODE_SUCC(n, i)    ((wat_tnode_t*)0)

struct wat_layout_burg_ctx_t;
void wat_rec(wat_tnode_t* node, int rule, struct wat_layout_burg_ctx_t* ctx);

#endif /* WAT_LAYOUT_BURG_H */
