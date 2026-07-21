// test_trap_reason.c — a trap carries WHY it trapped, in the spec's own words.
//
// The two div_s guards are the case that proves the mechanism: before `error:`
// carried a declared condition, DivByZero and Overflow emitted byte-identical
// output, so no consumer could tell 10/0 from INT_MIN/-1. Both must now report
// their own §7.10 trap message, and the text must come from the generated
// vocabulary (instructions.toml), never an inline literal here.
#include "interp.h"
#include "jav_trap_reason.h"
#include <stdio.h>
#include <string.h>

// i32.const a; i32.const b; i32.div_s; end  — a bare code body (no locals decl,
// supplied by the harness below), executed straight off the interpreter.
static int run_div(int32_t a, int32_t b, jav_status_t* st, const char** why) {
    uint8_t code[32]; int n = 0;
    code[n++] = 0x00;                                   // local decl count = 0
    code[n++] = 0x41;                                   // i32.const
    for (int64_t v = a, more = 1; more; ) {             // sleb128
        uint8_t byte = (uint8_t)(v & 0x7f); v >>= 7;
        more = !((v == 0 && !(byte & 0x40)) || (v == -1 && (byte & 0x40)));
        code[n++] = (uint8_t)(byte | (more ? 0x80 : 0));
    }
    code[n++] = 0x41;                                   // i32.const
    for (int64_t v = b, more = 1; more; ) {
        uint8_t byte = (uint8_t)(v & 0x7f); v >>= 7;
        more = !((v == 0 && !(byte & 0x40)) || (v == -1 && (byte & 0x40)));
        code[n++] = (uint8_t)(byte | (more ? 0x80 : 0));
    }
    code[n++] = 0x6d;                                   // i32.div_s
    code[n++] = 0x0b;                                   // end

    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, (size_t)n);
    uint32_t nl = 0; bbq_read_uleb128_u32(&vm.frame.code, &nl); vm.frame.num_locals = 0;
    *st = interp_run(&vm, NULL);
    *why = jav_trap_reason_str((jav_trap_reason_t)vm.trap_reason);
    return jav_tos(&vm).i;
}

// f32.const x; i64.trunc_f32_u; end — the reason comes from a SUBSTRATE native
// (trunc_u64_f32), not an opgen guard, so this covers the other raise path. The
// spec gives this op two reasons and the old single range test could report
// neither; NaN must read "invalid conversion to integer" and an out-of-range
// magnitude "integer overflow".
static void run_trunc_u64(float x, jav_status_t* st, const char** why) {
    uint8_t code[16]; int n = 0;
    code[n++] = 0x00;                                   // local decl count = 0
    code[n++] = 0x43;                                   // f32.const
    memcpy(code + n, &x, 4); n += 4;
    code[n++] = 0xaf;                                   // i64.trunc_f32_u
    code[n++] = 0x0b;                                   // end

    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, (size_t)n);
    uint32_t nl = 0; bbq_read_uleb128_u32(&vm.frame.code, &nl); vm.frame.num_locals = 0;
    *st = interp_run(&vm, NULL);
    *why = jav_trap_reason_str((jav_trap_reason_t)vm.trap_reason);
}

static int check_trunc(const char* what, float x, const char* want) {
    jav_status_t st; const char* why;
    run_trunc_u64(x, &st, &why);
    int ok = (st == JAV_TRAP) && !strcmp(why, want);
    printf("  %-22s -> status=%d reason=\"%s\"  [%s]\n", what, st, why, ok ? "PASS" : "FAIL");
    if (!ok) printf("      wanted reason \"%s\"\n", want);
    return !ok;
}

static int check(const char* what, int32_t a, int32_t b, const char* want) {
    jav_status_t st; const char* why;
    run_div(a, b, &st, &why);
    int ok = (st == JAV_TRAP) && !strcmp(why, want);
    printf("  %-22s -> status=%d reason=\"%s\"  [%s]\n", what, st, why, ok ? "PASS" : "FAIL");
    if (!ok && st == JAV_TRAP) printf("      wanted reason \"%s\"\n", want);
    return !ok;
}

int main(void) {
    int fails = 0;

    // The vocabulary itself: every reason round-trips to its spec message.
    if (strcmp(jav_trap_reason_str(JAV_TRAP_IntegerDivideByZero), "integer divide by zero") ||
        strcmp(jav_trap_reason_str(JAV_TRAP_IntegerOverflow),     "integer overflow")) {
        printf("  generated vocabulary  [FAIL]\n"); fails++;
    } else {
        printf("  generated vocabulary  [PASS]\n");
    }

    // The two guards on i32.div_s, which used to be indistinguishable.
    fails += check("i32.div_s 10/0",       10,          0, "integer divide by zero");
    fails += check("i32.div_s INT_MIN/-1", INT32_MIN,  -1, "integer overflow");

    // The substrate path, and the NaN-vs-range split the spec table demands.
    fails += check_trunc("i64.trunc_f32_u NaN", 0.0f / 0.0f, "invalid conversion to integer");
    fails += check_trunc("i64.trunc_f32_u -2",  -2.0f,       "integer overflow");

    // A non-trapping divide must leave no reason behind.
    jav_status_t st; const char* why;
    int r = run_div(10, 3, &st, &why);
    int ok = (st == JAV_RETURN) && r == 3 && !strcmp(why, "trap");
    printf("  %-22s -> %d status=%d reason=\"%s\"  [%s]\n", "i32.div_s 10/3", r, st, why,
           ok ? "PASS" : "FAIL");
    fails += !ok;

    printf("\ntrap reason: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
