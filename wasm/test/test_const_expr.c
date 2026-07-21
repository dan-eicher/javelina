#include "interp.h"
#include "validate.h"
#include "bbq_vec.h"     // vm->globals is a bbq_vec (the length rides with it)
#include <stdio.h>
#include <string.h>

/* Extended-const: constant expressions (global / segment initializers) may use
 * t.const, global.get, and i32/i64 add/sub/mul. jav_validate_const_expr is the
 * admissibility gate; evaluation is the ordinary generated interpreter (a const
 * expr is just a tiny body). This checks both: the gate accepts the extended set
 * and rejects non-const ops, and the accepted exprs evaluate to the right value. */

/* opcode bytes */
#define I32_CONST 0x41
#define I64_CONST 0x42
#define OP_GLOBAL_GET 0x23
#define I32_ADD 0x6a
#define I32_MUL 0x6c
#define I64_SUB 0x7d
#define I32_DIV_S 0x6d
#define END 0x0b

static int64_t eval(const uint8_t* c, size_t n, int is64, int has_g, int64_t g0){
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm);
    slot_t* gstore = NULL; slot_t** gv = NULL; u1* gt = NULL;   // one global slot, BY REFERENCE
    slot_t z = {0}; bbq_vec_push(gstore, z); bbq_vec_push(gv, &gstore[0]); bbq_vec_push(gt, (u1)0);
    if (has_g) gstore[0].l = g0;
    vm.cluster.globals = gv; vm.cluster.global_types = gt;
    bbq_ctx_init(&vm.frame.code, c, n);
    interp_run(&vm, NULL);
    int64_t r = is64 ? jav_tos(&vm).l : (int64_t)jav_tos(&vm).i;
    bbq_vec_free(gstore); bbq_vec_free(gv); bbq_vec_free(gt); jav_vm_free(&vm);
    return r;
}

static int fails=0;
static void accept(const char* nm, const uint8_t* c, size_t n, int is64, int has_g, int64_t g0, int64_t exp){
    int v = jav_validate_const_expr(c, n);
    int64_t r = v ? eval(c,n,is64,has_g,g0) : 0;
    int ok = v && r==exp;
    printf("  %-34s valid=%d eval=%-6lld exp=%-6lld [%s]\n", nm, v,(long long)r,(long long)exp, ok?"ok":"FAIL");
    fails += !ok;
}
static void reject(const char* nm, const uint8_t* c, size_t n){
    int v = jav_validate_const_expr(c, n);
    printf("  %-34s valid=%d (want reject)             [%s]\n", nm, v, !v?"ok":"FAIL");
    fails += v;
}

int main(void){
    /* (2 * 3) + 1 = 7 — arithmetic in a constant expression (extended-const) */
    uint8_t a[]={ I32_CONST,2, I32_CONST,3, I32_MUL, I32_CONST,1, I32_ADD, END };
    accept("i32: (2*3)+1", a,sizeof a, 0,0,0, 7);
    /* global.get 0 + 5, with global[0]=10 -> 15 */
    uint8_t b[]={ OP_GLOBAL_GET,0, I32_CONST,5, I32_ADD, END };
    accept("i32: global0(10)+5", b,sizeof b, 0,1,10, 15);
    /* i64: 100 - 23 = 77 (sleb 100 = 0xE4 0x00; 23 = 0x17) */
    uint8_t d[]={ I64_CONST,0xE4,0x00, I64_CONST,23, I64_SUB, END };
    accept("i64: 100-23", d,sizeof d, 1,0,0, 77);
    /* a plain single const is the MVP-constant case, still admissible */
    uint8_t e[]={ I32_CONST,42, END };
    accept("i32: const 42", e,sizeof e, 0,0,0, 42);

    /* i32.div_s is NOT a constant instruction — reject */
    uint8_t f[]={ I32_CONST,6, I32_CONST,2, I32_DIV_S, END };
    reject("i32: 6/2 (div not const)", f,sizeof f);
    /* missing terminating `end` — reject */
    uint8_t g[]={ I32_CONST,1 };
    reject("no end", g,sizeof g);
    /* two results left on the stack — reject (arity != 1) */
    uint8_t h[]={ I32_CONST,1, I32_CONST,2, END };
    reject("two results", h,sizeof h);

    printf("\nextended-const validate + eval: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
