// test_typemodel.c — proves the type model is COMPLETE (no missing dimension the
// validator silently ignores): per-field nullability, concrete-ref globals, and
// ref.func's concrete type. Validation-only: each case asserts accept(1)/reject(0).
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* $0 = leaf struct{i32}; $1 = box struct{ (ref $0) NON-NULL }; $2 = a func type */
static const jav_valtype_t F_LEAF[1] = { WVT_I32 };          static const uint32_t T_LEAF[1] = { 0 };
static const jav_valtype_t F_BOX[1]  = { WVT_REF_NN };  static const uint32_t T_BOX[1]  = { 0 };
static const jav_structtype_t STRUCTTYPES[2] = { {F_LEAF,T_LEAF,1}, {F_BOX,T_BOX,1} };
static const uint8_t KINDS[3] = { WST_STRUCT, WST_STRUCT, WST_FUNC };
static const int32_t SUPERS[3] = { -1, -1, -1 };
static const jav_subtype_ctx_t LAT = { KINDS, SUPERS, 3 };

static const jav_valtype_t GLOB[1]  = { WVT_REF };  static const uint32_t GLOB_T[1] = { 0 };  /* global 0 : (ref null $0) */
static const uint8_t       GLOB_M[1] = { 1 };       /* global 0 is mutable (the set/get test) */
static const jav_functype_t FSIGS[1] = { { NULL,0, NULL,0 } };
static const uint32_t FUNCTYPE_IDX[1] = { 2 };            /* func 0 has type $2 */
static const jav_valtype_t RES_NONE[1] = { WVT_I32 };    /* unused for void-result cases */

static int fails = 0;
static int validates(const uint8_t* code, size_t n, const jav_valtype_t* res, const uint32_t* res_t, unsigned nres) {
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx = {0};
    cx.results = res; cx.result_tidx = res_t; cx.nresults = nres;
    cx.structtypes = STRUCTTYPES; cx.nstructtypes = 2;
    cx.globals = GLOB; cx.nglobals = 1; cx.global_tidx = GLOB_T; cx.global_mut = GLOB_M;
    cx.func_sigs = FSIGS; cx.nfuncs = 1; cx.func_type_idx = FUNCTYPE_IDX;
    cx.lattice = &LAT;
    int ok = jav_typecheck(code, n, &cx, &st, &k);
    if (ok) bbq_vec_free(st);
    return ok;
}
static void CK(const char* msg, int got, int want) {
    int ok = (got == want);
    printf("  %-52s [%s]\n", msg, ok ? "PASS" : "FAIL"); fails += !ok;
}

int main(void){
    static const jav_valtype_t R_BOX[1] = { WVT_REF }; static const uint32_t R_BOX_T[1] = { 1 };
    static const jav_valtype_t R_I32[1] = { WVT_I32 };
    (void)RES_NONE;

    /* NON-NULL FIELD — accept: a non-null (ref $0) fills box's non-null field. */
    static const uint8_t a[]={ 0x41,0x2a, 0xfb,0x00,0x00,   /* const 42; struct.new $0 -> (ref $0) */
                               0xfb,0x00,0x01, 0x0b };       /* struct.new $1 (field=(ref $0) nn); end */
    CK("non-null field accepts a non-null ref", validates(a,sizeof a, R_BOX, R_BOX_T, 1), 1);

    /* NON-NULL FIELD — reject: a nullable (ref null $0) cannot fill a non-null field. */
    static const uint8_t b[]={ 0xd0,0x00,                   /* ref.null $0 -> (ref null $0) */
                               0xfb,0x00,0x01, 0x0b };       /* struct.new $1 -> field is non-null: REJECT */
    CK("non-null field rejects a nullable ref", validates(b,sizeof b, R_BOX, R_BOX_T, 1), 0);

    /* GLOBAL concrete ref typeidx — set a (ref $0), get it back, read its field. */
    static const uint8_t c[]={ 0x41,0x2a, 0xfb,0x00,0x00, 0x24,0x00,   /* struct.new $0; global.set 0 */
                               0x23,0x00, 0xfb,0x02,0x00,0x00, 0x0b };  /* global.get 0; struct.get $0 0 -> i32 */
    CK("global (ref $0): set/get keeps the typeidx", validates(c,sizeof c, R_I32, NULL, 1), 1);

    /* ref.func carries the concrete func type $2 — usable where (ref $2)/(ref func) wanted. */
    static const uint8_t d[]={ 0xd2,0x00, 0xd1, 0x0b };     /* ref.func 0 -> (ref $2); ref.is_null -> i32 */
    CK("ref.func yields a usable concrete func ref", validates(d,sizeof d, R_I32, NULL, 1), 1);

    /* a struct ref is NOT a func ref — ref.func's result can't feed a struct slot. */
    static const uint8_t e[]={ 0xd2,0x00, 0xfb,0x02,0x00,0x00, 0x0b };  /* ref.func 0; struct.get $0 0: REJECT */
    CK("func ref rejected where struct ref required", validates(e,sizeof e, R_I32, NULL, 1), 0);

    printf("\ntype model completeness (nullability / global typeidx / ref.func): %s\n", fails?"FAIL":"ALL PASS");
    return fails?1:0;
}
