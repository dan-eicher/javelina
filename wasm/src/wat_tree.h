/*
 * wat_tree.h — the render forest `water -d` lays out and prints.
 *
 * The builder turns a VALIDATED module (wat_check's verdict is a
 * precondition: only valid wat is ever written) into a forest of §6 groups:
 * one label root per declaration, statement, type entry, typeuse and import
 * descriptor. Inside a group the only tree structure is what §6.5.11 rule 1
 * folds — the operand spine, bounded by the producer edges wat_check
 * recorded — because §6's module syntax is flat records; every other
 * connection is an r1/r2 span into `roots`, and every list is either an
 * atom span (av) or a root span the emitter iterates.
 *
 * Everything the text will say is decided HERE: which §6.1.2 abbreviations
 * fire (settled by the module, not by width), how deep each fold goes (the
 * admissible k), how every immediate, name, limit and constant is spelled
 * (rendered once into the pool, with its byte width beside it). The emitter
 * adds only whitespace — which §6.1.1 makes free — so a group's flat width
 * is computable exactly before a byte is printed.
 */
#ifndef WAT_TREE_H
#define WAT_TREE_H

#include "bbq_arena.h"
#include "jav_error.h"
#include "jav_types.h"
#include "wat_check.h"
#include "wat_tnode.h"

/* One pooled token: `pool + off`, NUL-terminated, `w` bytes wide. */
typedef struct {
    uint32_t off;
    uint32_t w;
} wat_atom_t;

typedef struct {
    wat_tnode_t**     roots;    /* every label root; r1/r2 spans index here */
    uint32_t          nroots;
    const uint32_t*   decls;    /* indices into roots: the module's fields, in order */
    uint32_t          ndecls;
    const char*       pool;     /* payload text */
    const wat_atom_t* atoms;    /* av spans index here */
    uint32_t          natoms;
    uint32_t          mod_id;   /* pool offset of the §7.7.1 module id, or ~0u */
    /* §7.7.3 has no `tag` in its place vocabulary; a custom section at a
     * position no word can express is counted here (and placed after last),
     * never silently misplaced. Structurally zero today — the counter is the
     * fail-closed guard on that argument. */
    uint32_t          custom_unplaceable;
} wat_forest_t;

/* Build the forest for `m`, which `cx` was projected from and which already
 * passed wat_check_module and every wat_check_body. Returns 1 on success;
 * 0 with *err set when a body walk refuses (which a validated module's
 * cannot, so a 0 here is a builder bug surfacing, not a rendering path). */
int wat_tree_build(const jav_module_t* m, const wat_check_ctx_t* cx,
                   bbq_arena* a, wat_forest_t* out, jav_err_t* err);

#endif /* WAT_TREE_H */
