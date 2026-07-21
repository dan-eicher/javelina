// test_addrtype_spec.c — §2.3.11 address types resolved the way the spec resolves them.
//
// §2.3.11 addrtype ::= i32 | i64; §2.3.15 memtype ::= addrtype limits page; §2.3.16 tabletype ::=
// addrtype limits reftype. So `at` is DECLARED per memory/table, and every typing rule reads it
// from the module (§3.4.4/§3.4.5), while §4.6.8 executes it as "Assert: Due to validation ... Pop
// the value (at.const i)" — asserted, never tested.
//
// javelina resolves the width at RUNTIME from the operand's value tag (GPOP_ADDR:
// `tag == T_LONG ? 64-bit : truncate`). A tested fact can disagree with the declared type; an
// asserted one cannot. The rows below construct that disagreement — an i64 address arriving with a
// tag that says otherwise, because it came out of a GC aggregate — and require the SPEC's answer.
//
// Written before the fix, and required to be RED against the current lowering. Controls bracket
// every claim: the same address spelled directly must trap, and the same carrier path in bounds
// must load — so a failure is attributable to the width and not to the plumbing.
#include "interp.h"
#include "heap.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "jav_instance.h"
#include "jav_view_nav.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { EXP_VAL, EXP_TRAP } expect_t;
typedef struct { const char* what; const char* fn; expect_t exp; int val; } row_t;

static int fails = 0;
static bbq_arena A; static uint8_t* BUF;
static jav_modidx_t MOD; static jav_instance_t INST; static vm_t VM; static heap_t HEAP;

static void chk(const row_t* r) {
    int32_t fx = jav_instance_export(&INST, r->fn, 0);
    if (fx < 0) { printf("  %-42s export missing            [FAIL]\n", r->what); fails++; return; }
    VM.frame.sp = 0; VM.frame.num_locals = 0;
    VM.trapped = 0;
    jav_status_t s = jav_call(&VM, VM.heap, (u4)fx);
    int trapped = (s != JAV_OK);
    int got = trapped ? 0 : jav_tos(&VM).i;
    int ok = (r->exp == EXP_TRAP) ? trapped : (!trapped && got == r->val);
    if (r->exp == EXP_TRAP)
        printf("  %-42s %-24s [%s]\n", r->what, trapped ? "trapped" : "DID NOT TRAP", ok?"PASS":"FAIL");
    else
        printf("  %-42s %-24s [%s]\n", r->what,
               trapped ? "trapped" : (got == r->val ? "correct value" : "WRONG VALUE"), ok?"PASS":"FAIL");
    fails += !ok;
}

int main(void) {
    FILE* f = fopen("addrtype_spec.wasm", "rb"); if (!f) { perror("addrtype_spec.wasm"); return 2; }
    fseek(f,0,SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
    BUF = malloc((size_t)n);
    if (fread(BUF,1,(size_t)n,f) != (size_t)n) { perror("fread"); return 2; }
    fclose(f);

    bbq_arena_init(&A, 0);
    bbq_capture_metadata m = jav_view_module(BUF, (size_t)n, &A);
    if (!m.success) { printf("module did not view\n"); return 1; }
    if (!jav_module_index(m.root, BUF, &A, &MOD)) { printf("module did not index\n"); return 1; }
    jav_err_t err;
    if (jav_module_validate(m.root, BUF, &MOD, &err) != JAV_OK) { printf("module did not validate\n"); return 1; }

    memset(&HEAP,0,sizeof HEAP); memset(&VM,0,sizeof VM);
    jav_vm_init(&VM); VM.heap = &HEAP;
    if (jav_instantiate(&VM, m.root, BUF, &MOD, NULL, 0, &INST, &err) != JAV_OK) { printf("did not instantiate\n"); return 1; }
    jav_instance_bind(&VM, &INST);
    jav_heap_gc_init(&HEAP, &VM);

    static const row_t rows[] = {
      /* controls: the declared addrtype IS the operand/result type (§3.4.4) */
      { "table.size $t64 : eps -> i64",        "size64_is_i64",              EXP_VAL,  1 },
      { "table.size $t32 : eps -> i32",        "size32_is_i32",              EXP_VAL,  1 },
      { "table.grow $t64 : rt i64 -> i64",     "grow64_is_i64",              EXP_VAL,  1 },
      { "table.copy len = min(at1,at2) = i32", "copy_len_is_min_at",         EXP_VAL,  1 },
      /* controls: these addresses really are out of bounds when spelled directly */
      { "mem64 @2^32 direct traps",            "mem64_direct_2p32_traps",    EXP_TRAP, 0 },
      { "table64 @2^32 direct traps",          "table64_direct_2p32_traps",  EXP_TRAP, 0 },
      /* control: the carrier path itself works in bounds */
      { "mem64 carried in-bounds loads",       "mem64_carried_inbounds_ok",  EXP_VAL,  0xABC },
      /* THE CONTRACT: width comes from the DECLARED addrtype, not the operand's runtime tag */
      { "mem64 @2^32 via carrier traps",       "mem64_carried_addr_traps",   EXP_TRAP, 0 },
      { "table64 @2^32 via carrier traps",     "table64_carried_index_traps",EXP_TRAP, 0 },
    };
    printf("§2.3.11 addrtype resolved from the DECLARED memtype/tabletype:\n");
    for (unsigned i = 0; i < sizeof rows/sizeof rows[0]; i++) chk(&rows[i]);
    printf("\naddrtype is declared, not tagged: %s\n", fails ? "FAIL" : "ALL PASS");

    jav_heap_gc_destroy(&HEAP); jav_instance_free(&INST); jav_vm_free(&VM);
    bbq_arena_free(&A); free(BUF);
    return fails ? 1 : 0;
}
