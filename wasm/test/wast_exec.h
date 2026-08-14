// wast_exec.h — the .wast EXECUTION runner interface. Implemented in wast_exec.c (a c-lite
// TU: it speaks the index/instance type-world, which cannot co-exist with the owning reader
// in test_wast.c). The owning side assembles every module to bytes and hands them across.
#ifndef WAST_EXEC_H
#define WAST_EXEC_H
#include "wast_sexpr.h"   // Node
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>        // FILE
#include <string.h>

// The .wast expected-failure string (a "…" token, quotes included) against what the
// engine reported. Substring either way: the spec's vocabulary is the shorter of the
// two often enough that an exact compare would reject correct answers. One authority
// for both the validation side (jav_err_str) and the execution side
// (jav_trap_reason_str) — the two must not drift into separate matchers.
// 1 = matches, or the command carries no expected string.
static inline int wast_msg_matches(const char *actual, const char *expected_tok) {
    if (!expected_tok || expected_tok[0] != '"') return 1;
    char exp[256]; int L = (int)strlen(expected_tok); if (L >= 2) L -= 2; else L = 0;
    if (L > 255) L = 255; memcpy(exp, expected_tok + 1, (size_t)L); exp[L] = 0;
    if (!exp[0]) return 1;
    return strstr(actual, exp) != NULL || strstr(exp, actual) != NULL;
}

// Which execution tier the store runs the corpus on. A number this runner prints
// is a claim about ONE of these, so the tier is chosen rather than assumed —
// running the same corpus on each is how the three are held to each other.
//
// WAST_TIER_2 is declared before it can run. The alternative is to leave it out
// and have a three-way differential quietly compare tier-1 with itself, which
// would agree perfectly and mean nothing; naming it makes the store refuse until
// the stitcher exists.
typedef enum {
    WAST_TIER_INTERP = 0,   // the threaded interpreter — the canonical tier
    WAST_TIER_1      = 1,   // copy-and-patch, one stencil per opcode
    WAST_TIER_2      = 2,   // tiled: the tree builder + the burg cover
} wast_tier_t;
const char* wast_tier_name(wast_tier_t t);
// Which of the two JIT tiers THIS build has. The cache size was fixed when the
// stencil table was generated, so it is a property of the binary, not a choice —
// a caller that just wants "the compiling tier" has to ask rather than pick.
wast_tier_t wast_exec_jit_tier(void);
// Functions the JIT declined over this run, summed across the per-file stores.
// A fallback is correct and therefore silent, so the count is what separates
// "the tier ran" from "the tier was on and did nothing".
uint32_t    wast_exec_jit_declined(void);
// ...and the ones it took. The two together are every function the JIT was OFFERED,
// which is the only independent count of what the tree builder should have seen: it
// is maintained by jav_instance.c per instantiation, not by the builder.
uint32_t    wast_exec_jit_compiled(void);
// Operand-stack slots this build's JIT can use. Zero means tier-2 still builds
// the tree and covers it but has no variant to choose, so "nothing was cached" is
// the right answer rather than a missing gate.
int         wast_exec_cache_slots(void);

// Build the shared store on `tier`; 1 on success, 0 if that tier cannot run.
int  wast_exec_store_init(wast_tier_t tier);
void wast_exec_spectest(uint8_t *bytes, size_t len);          // instantiate + keep spectest (owns bytes)
void wast_exec_file_reset(void);                              // fresh per-file scope; re-registers spectest
// ONE module channel: instantiate (owns bytes), compare the verdict to `expect` (a jav_status_t
// value: JAV_OK keeps+registers; UNLINKABLE/UNINSTANTIABLE/TRAP are the assert_* outcomes).
void wast_exec_module(uint8_t *bytes, size_t len, const char *id, int expect);
void wast_exec_register(const char *name, int nlen, const Node *idn);
void wast_exec_define(const char *id, uint8_t *bytes, size_t len);   // (module definition $id …)
void wast_exec_instance(const char *id_inst, const char *id_def);    // (module instance $inst $def)
void wast_exec_action(const Node *act);                       // bare invoke/get (side effects)
void wast_exec_assert_return(const Node *cmd);
void wast_exec_assert_trap(const Node *cmd);                  // also assert_exhaustion
void wast_exec_assert_exception(const Node *cmd);            // §7.1.8 uncaught WASM exception escaped
void wast_exec_note_excl(const char *reason);                 // a command the owning side couldn't assemble
void wast_exec_teardown(void);
void wast_exec_counts(int *ok, int *bad, int *excl, const char **reason);
int  wast_exec_trap_msgbad(void);   // trapped right, WRONG reason — the trap-cause debt meter
void wast_exec_print_breakdown(FILE *f);                     // itemized exclusion ledger (named, reconciled)

#endif // WAST_EXEC_H
