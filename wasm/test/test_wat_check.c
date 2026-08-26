// test_wat_check.c — §7.6 as water runs it, over the decoded instruction tree.
//
// The cases here are the ones the spec itself calls out, so the expected answers are
// not invented: §7.6.1's Note names the exact program a stack-polymorphic checker
// must still REJECT, and the reason it must.
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

/* ── PIN A-2 — DeadCodeIsStillTyped ─────────────────────────────────────────────
 *
 * §7.6.1's Note (printed 275), verbatim:
 *
 *   "Even with the unreachable flag set, consecutive operands are still pushed to
 *    and popped from the operand stack. That is necessary to detect invalid examples
 *    like (unreachable (i32.const) i64.add). However, a polymorphic stack cannot
 *    underflow, but instead generates Bot types as needed."
 *
 * Both halves are one test because either alone passes for the wrong reason: a walk
 * that SKIPS dead code accepts (a), and a walk that UNDERFLOWS on it rejects (b).
 */
static void dead_code_is_still_typed(void) {
    printf("DeadCodeIsStillTyped: the §7.6.1 Note, both halves\n");

    // (a) the spec's own invalid example — REJECTED.
    //   (module (func unreachable  i32.const 1  i64.add))
    {
        static const uint8_t wasm[] = {
            PRE,
            0x01, 0x04, 0x01, 0x60, 0x00, 0x00,            // type: () -> ()
            0x03, 0x02, 0x01, 0x00,                        // func: 0 : type 0
            0x0a, 0x08, 0x01, 0x06, 0x00,                  // code: 1 body, 6 bytes, 0 locals
                  0x00,                                    //   unreachable
                  0x41, 0x01,                              //   i32.const 1
                  0x7c,                                    //   i64.add
                  0x0b,                                    //   end
        };
        jav_module_t mod; bbq_arena a; wat_body_t r;
        bbq_arena_init(&a, 4096);
        CK("(unreachable (i32.const) i64.add) is REJECTED", run(wasm, sizeof wasm, &mod, &a, &r), 0);
        CK("...and the walk names where", r.fail != NULL, 1);
        if (r.fail) CK("...at the i64.add", r.fail->op, 0x7c);
        jav_module_free(&mod); bbq_arena_free(&a);
    }

    // (b) the polymorphic stack does not underflow — ACCEPTED.
    //   (module (func (result i32) unreachable))
    {
        static const uint8_t wasm[] = {
            PRE,
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,      // type: () -> (i32)
            0x03, 0x02, 0x01, 0x00,                        // func: 0 : type 0
            0x0a, 0x05, 0x01, 0x03, 0x00,                  // code: 1 body, 3 bytes, 0 locals
                  0x00,                                    //   unreachable
                  0x0b,                                    //   end
        };
        jav_module_t mod; bbq_arena a; wat_body_t r;
        bbq_arena_init(&a, 4096);
        int ok = run(wasm, sizeof wasm, &mod, &a, &r);
        // One check, not two: "no failure site" is true of a walk that never ran, so
        // on its own it is a green for the wrong reason.
        CK("unreachable satisfies an i32 result (Bot), no failure site",
           ok == 1 && r.fail == NULL, 1);
        jav_module_free(&mod); bbq_arena_free(&a);
    }
}

int main(void) {
    dead_code_is_still_typed();
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
