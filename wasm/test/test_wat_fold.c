// test_wat_fold.c — §7.6's producer edges, and the fold depth §6.5.11 admits.
//
// Both cases are whole hand-encoded `.wasm` modules read through the real owning
// reader, because that is the path `water -d` takes: the instructions arrive already
// decoded (wasm.bbq's `Expr` is `array<Instr>`), and the walk adds the edges.
#include "wat_check.h"
#include "jav_reader.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
static void CK(const char *msg, long got, long want) {
    int ok = (got == want);
    printf("  %-58s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

#define PRE 0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00   // magic + version

// The code section's first body, or NULL.
static const jav_func_body_t *body0(const jav_module_t *m) {
    for (size_t i = 0; i < m->sections.count; i++) {
        const jav_section_t *s = &m->sections.items[i];
        if (s->id != 10) continue;
        const jav_code_section_t *cs = &s->body.u.case_10;
        if (cs->entries.count == 0) return NULL;
        return &cs->entries.items[0].body;
    }
    return NULL;
}

// Read `bytes`, project the context, run §7.6 over body 0. `mod` and the arena are
// the caller's so the decoded instruction pointers stay live for the assertions.
static int run(const uint8_t *bytes, size_t n, jav_module_t *mod, bbq_arena *a,
               wat_body_t *out) {
    bbq_ctx_t cx;
    bbq_ctx_init(&cx, bytes, n);
    memset(mod, 0, sizeof *mod);
    memset(out, 0, sizeof *out);
    if (!jav_module_read(&cx, mod)) { bbq_ctx_free(&cx); return -1; }
    bbq_ctx_free(&cx);
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *wcx = wat_check_ctx_build(mod, a, &err);
    if (!wcx) return -2;
    const jav_func_body_t *b = body0(mod);
    if (!b) return -3;
    return wat_check_body(wcx, 0, b, a, out);
}

/* ── PIN A-1 — SpecFoldExampleIsATree ───────────────────────────────────────────
 *
 * §6.5.11's Note (printed 233), as the tree Part A must produce:
 *
 *     (local.get $x) (i32.const 2) i32.add (i32.const 3) i32.mul
 *   folds into
 *     (i32.mul (i32.add (local.get $x) (i32.const 2)) (i32.const 3))
 *
 * The spec prints the answer, so this test does not invent one. The STRING is PIN
 * C-0a/C-0b in test_wat_emit.c; the TREE is here, which is where it is owned.
 *
 *   (module (type (func (param i32) (result i32)))
 *           (func (type 0) local.get 0  i32.const 2  i32.add
 *                          i32.const 3  i32.mul))
 */
static void spec_fold_example_is_a_tree(void) {
    printf("SpecFoldExampleIsATree: local.get 0; i32.const 2; i32.add; i32.const 3; i32.mul\n");
    static const uint8_t wasm[] = {
        PRE,
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,   // type:  (i32) -> (i32)
        0x03, 0x02, 0x01, 0x00,                            // func:  0 : type 0
        0x0a, 0x0c, 0x01, 0x0a, 0x00,                      // code:  1 body, 10 bytes, 0 locals
              0x20, 0x00,                                  //   local.get 0
              0x41, 0x02,                                  //   i32.const 2
              0x6a,                                        //   i32.add
              0x41, 0x03,                                  //   i32.const 3
              0x6c,                                        //   i32.mul
              0x0b,                                        //   end
    };
    jav_module_t mod; bbq_arena a; wat_body_t r;
    bbq_arena_init(&a, 4096);
    CK("§7.6 accepts", run(wasm, sizeof wasm, &mod, &a, &r), 1);

    const jav_func_body_t *b = body0(&mod);
    if (!b || b->body.instrs.count != 5) {
        CK("five instructions decoded", b ? (long)b->body.instrs.count : -1, 5);
        jav_module_free(&mod); bbq_arena_free(&a); return;
    }
    const jav_instr_t *get = &b->body.instrs.items[0];   // local.get 0
    const jav_instr_t *c2  = &b->body.instrs.items[1];   // i32.const 2
    const jav_instr_t *add = &b->body.instrs.items[2];   // i32.add
    const jav_instr_t *c3  = &b->body.instrs.items[3];   // i32.const 3
    const jav_instr_t *mul = &b->body.instrs.items[4];   // i32.mul

    const wat_info_t *im = wat_info(&r, mul);
    CK("i32.mul has a row", im != NULL, 1);
    if (im) {
        CK("i32.mul pops 2", im->noperands, 2);
        CK("i32.mul operand 0 is the i32.add", im->noperands == 2 && im->producer[0] == add, 1);
        CK("i32.mul operand 1 is the i32.const 3", im->noperands == 2 && im->producer[1] == c3, 1);
        CK("i32.mul admits fold depth 2", im->fold, 2);
    }
    const wat_info_t *ia = wat_info(&r, add);
    CK("i32.add has a row", ia != NULL, 1);
    if (ia) {
        CK("i32.add pops 2", ia->noperands, 2);
        CK("i32.add operand 0 is the local.get", ia->noperands == 2 && ia->producer[0] == get, 1);
        CK("i32.add operand 1 is the i32.const 2", ia->noperands == 2 && ia->producer[1] == c2, 1);
        CK("i32.add admits fold depth 2", ia->fold, 2);
    }
    const wat_info_t *ig = wat_info(&r, get);
    CK("local.get pops nothing", ig ? (long)ig->noperands : -1, 0);
    CK("local.get folds nothing", ig ? (long)ig->fold : -1, 0);
    jav_module_free(&mod); bbq_arena_free(&a);
}

/* ── PIN A-3 — AdmissibilityStopsAtANonOperand ──────────────────────────────────
 *
 * §6.5.11 rule 1 is `'(' plaininstr instrs ')' ≡ instrs plaininstr`, so the folded
 * content is an arbitrary instruction SEQUENCE and the spec would permit
 * `(i32.add (i32.const 1) (nop) (i32.const 2))`. The plan's §3.6 does not: the
 * admissible depth is the largest k whose producing run contains EXACTLY those
 * operands' subtrees, and the `nop` is in the run and in no subtree. So k = 1.
 *
 *   (module (func i32.const 1  nop  i32.const 2  i32.add  drop))
 */
static void admissibility_stops_at_a_non_operand(void) {
    printf("AdmissibilityStopsAtANonOperand: i32.const 1; nop; i32.const 2; i32.add; drop\n");
    static const uint8_t wasm[] = {
        PRE,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,                // type:  () -> ()
        0x03, 0x02, 0x01, 0x00,                            // func:  0 : type 0
        0x0a, 0x0b, 0x01, 0x09, 0x00,                      // code:  1 body, 9 bytes, 0 locals
              0x41, 0x01,                                  //   i32.const 1
              0x01,                                        //   nop
              0x41, 0x02,                                  //   i32.const 2
              0x6a,                                        //   i32.add
              0x1a,                                        //   drop
              0x0b,                                        //   end
    };
    jav_module_t mod; bbq_arena a; wat_body_t r;
    bbq_arena_init(&a, 4096);
    CK("§7.6 accepts", run(wasm, sizeof wasm, &mod, &a, &r), 1);

    const jav_func_body_t *b = body0(&mod);
    if (!b || b->body.instrs.count != 5) {
        CK("five instructions decoded", b ? (long)b->body.instrs.count : -1, 5);
        jav_module_free(&mod); bbq_arena_free(&a); return;
    }
    const jav_instr_t *c1  = &b->body.instrs.items[0];   // i32.const 1
    const jav_instr_t *c2  = &b->body.instrs.items[2];   // i32.const 2
    const jav_instr_t *add = &b->body.instrs.items[3];   // i32.add

    const wat_info_t *ia = wat_info(&r, add);
    CK("i32.add has a row", ia != NULL, 1);
    if (ia) {
        // The EDGES are complete — both operands have real producers. Only the
        // FOLD is bounded, and by the nop between them, not by a missing edge.
        CK("i32.add pops 2", ia->noperands, 2);
        CK("operand 0 is still the i32.const 1", ia->noperands == 2 && ia->producer[0] == c1, 1);
        CK("operand 1 is still the i32.const 2", ia->noperands == 2 && ia->producer[1] == c2, 1);
        CK("the nop bounds the fold to 1", ia->fold, 1);
    }
    jav_module_free(&mod); bbq_arena_free(&a);
}

/* ── PIN A-6 — BlockOperandsAreItsOwn ───────────────────────────────────────────
 *
 * A block instruction's row stays open across its nested walk, so it is the one row
 * that can be closed on someone else's operands. Each of the three shapes here has a
 * different instruction ready to be mistaken for its own:
 *
 *   block (param i32)   the last `drop` inside it
 *   if                  the `drop` inside the then-arm
 *   try_table (param i32) with a `catch` — the handler's own pop_ctrl, which pops a
 *                       value the try_table itself pushed
 *
 * And the fold each admits comes from §6.5.11 (printed 233), where `foldedinstr*`
 * appears in exactly ONE of the five productions: `'(' 'if' label blocktype
 * foldedinstr* '(' 'then' ... ')' ... ')'`. `block`, `loop` and `try_table` have no
 * such slot, so a blocktype parameter is emitted BEFORE them and never folded in.
 *
 *   (module (type (func)) (type (func (param i32))) (type (func (result i32)))
 *           (tag (type 1))
 *           (func (type 0)
 *             i32.const 7  (block (type 1) drop i32.const 8 i32.const 9 i32.add drop)
 *             i32.const 1  (if           nop  i32.const 5 drop)
 *             (block (type 2)
 *               i32.const 6
 *               (try_table (type 1) (catch 0 0) drop i32.const 4 drop)
 *               i32.const 0)
 *             drop))
 */
static void block_operands_are_its_own(void) {
    printf("BlockOperandsAreItsOwn: block/if/try_table close on their OWN pops\n");
    static const uint8_t wasm[] = {
        PRE,
        0x01, 0x0c, 0x03,                                  // type: 3 types
              0x60, 0x00, 0x00,                            //   0: () -> ()
              0x60, 0x01, 0x7f, 0x00,                      //   1: (i32) -> ()
              0x60, 0x00, 0x01, 0x7f,                      //   2: () -> (i32)
        0x03, 0x02, 0x01, 0x00,                            // func: 0 : type 0
        0x0d, 0x03, 0x01, 0x00, 0x01,                      // tag:  0 : attr 0, type 1
        0x0a, 0x2c, 0x01, 0x2a, 0x00,                      // code: 1 body, 42 bytes, 0 locals
              0x41, 0x07,                                  //   i32.const 7
              0x02, 0x01,                                  //   block (type 1)
                    0x1a,                                  //     drop        (the param)
                    0x41, 0x08,                            //     i32.const 8
                    0x41, 0x09,                            //     i32.const 9
                    0x6a,                                  //     i32.add
                    0x1a,                                  //     drop
              0x0b,                                        //   end
              0x41, 0x01,                                  //   i32.const 1
              0x04, 0x40,                                  //   if (empty)
                    0x01,                                  //     nop
                    0x41, 0x05,                            //     i32.const 5
                    0x1a,                                  //     drop
              0x0b,                                        //   end
              0x02, 0x02,                                  //   block (type 2)
                    0x41, 0x06,                            //     i32.const 6
                    0x1f, 0x01, 0x01, 0x00, 0x00, 0x00,    //     try_table (type 1) (catch 0 0)
                          0x1a,                            //       drop     (the param)
                          0x41, 0x04,                      //       i32.const 4
                          0x1a,                            //       drop
                    0x0b,                                  //     end
                    0x41, 0x00,                            //     i32.const 0
              0x0b,                                        //   end
              0x1a,                                        //   drop
              0x0b,                                        //   end
    };
    jav_module_t mod; bbq_arena a; wat_body_t r;
    bbq_arena_init(&a, 8192);
    CK("§7.6 accepts", run(wasm, sizeof wasm, &mod, &a, &r), 1);

    const jav_func_body_t *b = body0(&mod);
    if (!b || b->body.instrs.count != 6) {
        CK("six top-level instructions decoded", b ? (long)b->body.instrs.count : -1, 6);
        jav_module_free(&mod); bbq_arena_free(&a); return;
    }
    const jav_instr_t *c7  = &b->body.instrs.items[0];   // i32.const 7
    const jav_instr_t *blk = &b->body.instrs.items[1];   // block (type 1)
    const jav_instr_t *c1  = &b->body.instrs.items[2];   // i32.const 1
    const jav_instr_t *iff = &b->body.instrs.items[3];   // if
    const jav_instr_t *out = &b->body.instrs.items[4];   // block (type 2)

    const wat_info_t *ib = wat_info(&r, blk);
    CK("block has a row", ib != NULL, 1);
    if (ib) {
        CK("block pops its one blocktype param", ib->noperands, 1);
        CK("block operand 0 is the i32.const 7", ib->noperands == 1 && ib->producer[0] == c7, 1);
        CK("block admits no fold (§6.5.11 has no slot)", ib->fold, 0);
    }
    const wat_info_t *ii = wat_info(&r, iff);
    CK("if has a row", ii != NULL, 1);
    if (ii) {
        CK("if pops its condition", ii->noperands, 1);
        CK("if operand 0 is the i32.const 1", ii->noperands == 1 && ii->producer[0] == c1, 1);
        CK("if admits fold depth 1", ii->fold, 1);
    }
    const jav_block_t *ob = &out->body.u.case_1;
    if (ob->instrs.count != 3) {
        CK("the outer block holds three instructions", (long)ob->instrs.count, 3);
        jav_module_free(&mod); bbq_arena_free(&a); return;
    }
    const jav_instr_t *c6 = &ob->instrs.items[0];        // i32.const 6
    const jav_instr_t *tt = &ob->instrs.items[1];        // try_table (type 1)
    const wat_info_t *it = wat_info(&r, tt);
    CK("try_table has a row", it != NULL, 1);
    if (it) {
        CK("try_table pops its one blocktype param", it->noperands, 1);
        CK("try_table operand 0 is the i32.const 6", it->noperands == 1 && it->producer[0] == c6, 1);
        CK("try_table admits no fold (§6.5.11 has no slot)", it->fold, 0);
    }
    jav_module_free(&mod); bbq_arena_free(&a);
}

int main(void) {
    spec_fold_example_is_a_tree();
    admissibility_stops_at_a_non_operand();
    block_operands_are_its_own();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
