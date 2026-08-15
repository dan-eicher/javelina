/*
 * wat_check.h — WASM 3.0 §7.6, over the instruction tree BBQ already decoded.
 *
 * §7.6: "the algorithm is expressed over the flat sequence of opcodes as occurring
 * in the binary format, and performs only a single pass over it. Consequently, it
 * can be integrated directly into a decoder." Here the decoding already happened —
 * `wasm.bbq`'s `Expr` is `array<Instr>(...)` and the owning reader built it — so the
 * walk iterates `jav_expr_t.instrs[]` and nothing re-decodes a LEB.
 *
 * This is a transcription, not a design: every function the spec prints in §7.6.1
 * has a function of the same name in wat_check.c, and §7.6.2's switch is the switch.
 *
 * It is NOT the VM's validator and shares no code with it. water and the engine are
 * separate tools; what they share is opgen's generated per-opcode transfer table
 * (`jav_opsig[]`, from `wasm.def`) and the §3.3 subtype lattice, so the two cannot
 * disagree about an instruction's arity or about subtyping unless the generator
 * changes under both.
 *
 * The second product of the one walk is the PRODUCER EDGE per operand, which is what
 * §6.5.11's folded form needs and what nothing else in the tree can supply. §7.6.2's
 * closing Note is why those edges form a tree: "It is an invariant under the current
 * WebAssembly instruction set that an operand of Bot type is never duplicated on the
 * stack." No dup ⟹ every value consumed exactly once ⟹ a tree, never a DAG.
 */
#ifndef WAT_CHECK_H
#define WAT_CHECK_H

#include <stdint.h>

#include "bbq_arena.h"
#include "jav_error.h"
#include "jav_types.h"

/* §7.6.1 "Context" — the module-level half (types, funcs, tables, mems, globals,
 * tags), projected once from the sections the reader produced. The per-function
 * half (return_type, locals, locals_init) is built per body by wat_check_body. */
typedef struct wat_check_ctx wat_check_ctx_t;

/* Project `m`. Returns NULL when a section the context needs is malformed in a way
 * the reader accepted but §7 cannot use (an out-of-range type index in the function
 * section, say) — which is a §7 rejection, reported through wat_module_err. */
wat_check_ctx_t* wat_check_ctx_build(const jav_module_t* m, bbq_arena* a, jav_err_t* err);

/* What the walk knows about one instruction.
 *
 * `producer[i]` is the instruction that pushed operand i, in operand order, or NULL
 * where §7.6's pop_val yielded Bot — dead code below the frame height has no
 * producer because nothing pushed it. `noperands` is the pop count §7.6 used, which
 * for a variadic instruction (call, struct.new, throw, …) comes from the context and
 * not from a table.
 *
 * `fold` is §6.5.11 rule 1's admissible depth: the largest k such that the run
 * producing the last k operands contains exactly those operands' subtrees. It BOUNDS
 * the layout cover's action set — the cover may choose less when a narrower fold
 * lays out better, and may never choose more. */
typedef struct {
    const jav_instr_t*        in;         /* the instruction this row is about */
    /* Its ordinal in §7.6's own unit — "the flat sequence of opcodes as occurring in
     * the binary format". The struct tree folds `end` and `else` into fields of the
     * block that carries them, so this walk counts them anyway: any other walk over
     * the same body meets the same opcodes in the same order, and that is what makes
     * two independently-written walks comparable without either one carrying byte
     * positions the reader did not keep. */
    uint32_t                  seq;
    const jav_instr_t* const* producer;   /* noperands entries; NULL = no producer */
    uint32_t                  noperands;
    uint32_t                  fold;
} wat_info_t;

/* One body's verdict and the tree it carries. `info` is in §5 order — the order the
 * single pass met the instructions, which is also a pre-order walk of the struct
 * tree. `fail` names the instruction the walk stopped on, and is the "where" that
 * makes a rejection worth reading; it is NULL when ok. */
typedef struct {
    const wat_info_t* info;
    uint32_t          ninfo;
    int               ok;
    jav_err_t         err;
    const jav_instr_t* fail;
    /* The rejection's "where", for a diagnostic: `fail`'s flat §5 ordinal
     * within the body, and the top of the operand stack as the walk saw it
     * (captured at the one point every rejection funnels through). Empty
     * when ok. */
    uint32_t          fail_seq;
    char              fail_stack[96];
} wat_body_t;

/* §3.5 module-level validation — the half of §7 that no body carries, and which
 * §7.6's own sentence sets aside as "(Other aspects of validation are straightforward
 * to implement.)": limits, type-section subtyping, tag and start signatures, export
 * index bounds and name distinctness, and the constant expressions in globals, tables
 * and segments. A module can be §7-invalid with every body well-typed, so water needs
 * both this and wat_check_body before it may write anything.
 *
 * Returns 1 if the module passes, 0 with *err set to the specific reason. */
int wat_check_module(const wat_check_ctx_t* cx, bbq_arena* a, jav_err_t* err);

/* Run §7.6 over `body`, which is defined function `funcidx` of the module `cx` was
 * built from. Returns 1 iff the body is well-typed; `*out` is filled either way, so
 * a caller that wants the location of a rejection reads it from a 0 return.
 * Everything allocated comes from `a`. */
int wat_check_body(const wat_check_ctx_t* cx, uint32_t funcidx,
                   const jav_func_body_t* body, bbq_arena* a, wat_body_t* out);

/* The walk's row for one instruction, or NULL if the walk never reached it (the
 * body was rejected before it, or the instruction is not in this body). Backed by a
 * pointer-keyed bbq_hmap, which is the structure its own header describes for
 * exactly this question. */
const wat_info_t* wat_info(const wat_body_t* b, const jav_instr_t* in);

#endif /* WAT_CHECK_H */
