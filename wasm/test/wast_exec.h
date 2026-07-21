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

int  wast_exec_store_init(void);                              // build the shared store; 1 on success
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
