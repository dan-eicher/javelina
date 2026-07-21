// test_call_ref.c — call_ref (0x14), both tiers. WASM 3.0 §4.6.2 (execution: pop a funcref;
// null traps; else nested-call the function — the validator proved the static type, no
// dynamic gate) and §3.4 (validation: t1* (ref null x) -> t2*). A caller does
// `i32.const 41; ref.func 0; call_ref $t` and must get f0(41) = 42.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TRAP (-0x6BADBAD)
static const jav_valtype_t I32[1] = { WVT_I32 };
// the (i32)->i32 type, shared as type 0 + func 0's signature
static const jav_functype_t FT = { I32, 1, I32, 1, NULL, NULL };
static const uint32_t FTIDX[1] = { 0 };   // func 0 -> typeidx 0
// §3.3 lattice: one func type, no supertype — the ref subtype checks (ref.func -> call_ref) need it
static const uint8_t KINDS[1] = { WST_FUNC };
static const int32_t SUPERS[1] = { -1 };
static const jav_subtype_ctx_t LAT = { KINDS, SUPERS, 1 };

// callee f0: local.get 0; i32.const 1; i32.add; end   — returns arg+1
static const uint8_t CALLEE[] = { 0x20,0x00, 0x41,0x01, 0x6a, 0x0b };

// Returns the §7 verdict; the side-table (possibly NULL/empty for a branchless body —
// an empty bbq_vec IS NULL) comes back via *out, so a valid empty table isn't mistaken
// for a rejection.
static int tc(const uint8_t* code, size_t n, jav_vctx_t* cx, jav_st_entry_t** out) {
    unsigned k;
    return jav_typecheck(code, n, cx, out, &k);
}

static int run(const uint8_t* code, size_t n, int jit) {
    // validation context: 1 func (f0:(i32)->i32, type 0), 1 type, the function returns i32
    jav_vctx_t cx = {0};
    cx.types = &FT; cx.ntypes = 1;
    cx.func_sigs = &FT; cx.nfuncs = 1; cx.func_type_idx = FTIDX;
    cx.results = I32; cx.nresults = 1; cx.lattice = &LAT;
    jav_st_entry_t* st = NULL;
    if (!tc(code, n, &cx, &st)) return -999;
    // callee validated on its own param/result context
    jav_vctx_t ccx = {0}; ccx.results = I32; ccx.nresults = 1;
    static const jav_valtype_t P[1] = { WVT_I32 };
    ccx.locals = P; ccx.nlocals = 1;   // the param is local 0
    jav_st_entry_t* cst = NULL;
    if (!tc(CALLEE, sizeof CALLEE, &ccx, &cst)) { bbq_vec_free(st); return -998; }

    jav_func_t f[1]; memset(f, 0, sizeof f);
    f[0] = (jav_func_t){ .code=CALLEE, .code_len=sizeof CALLEE, .num_params=1, .num_results=1,
                         .type_index=0, .sidetable=cst };
    struct heap_t heap; memset(&heap, 0, sizeof heap);
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    vm.cluster.functions=f; vm.cluster.num_functions=1; vm.cluster.types=&FT; vm.cluster.num_types=1; vm.heap=&heap;
    bbq_ctx_init(&vm.frame.code, code, n); vm.frame.sidetable=st; vm.frame.num_locals=0;
    if (jit) jav_jit_run(&vm); else interp_run(&vm,&heap);
    int r = vm.trapped ? TRAP : jav_tos(&vm).i;
    jav_heap_free_mems(&heap); bbq_vec_free(st); bbq_vec_free(cst);
    return r;
}

static int fails=0;
static void val(const char* nm, const uint8_t* c, size_t n, int exp) {
    int i=run(c,n,0), j=run(c,n,1);
    int ok=(i==j && i==exp);
    printf("  %-34s interp=%-12d jit=%-12d exp=%-12d [%s]\n", nm,i,j,exp, ok?"PASS":"FAIL"); fails+=!ok;
}

int main(void) {
    // i32.const 41; ref.func 0; call_ref 0; end   -> f0(41) = 42
    static const uint8_t a[] = { 0x41,0x29, 0xd2,0x00, 0x14,0x00, 0x0b };
    val("call_ref f0(41) = 42", a, sizeof a, 42);
    // i32.const 0; ref.null $0; call_ref 0; end   -> null funcref of the right type traps
    static const uint8_t b[] = { 0x41,0x00, 0xd0,0x00, 0x14,0x00, 0x0b };
    val("call_ref on null funcref traps", b, sizeof b, TRAP);
    printf("\ncall_ref (0x14) interp == JIT, spec §4.6.2/§3.4: %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
