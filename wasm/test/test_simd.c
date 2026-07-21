// test_simd.c — SIMD lane ops, the "supported" tier: opgen GENERATES a per-lane scalar
// loop from one body (splat broadcasts a scalar to every lane; add is lanewise), run
// identically on both tiers. interp == JIT. No v128.const needed — splat builds the
// vectors from scalars, so this exercises the lane machinery without the 16-byte
// immediate / footer.
#include "interp.h"
#include "jit_driver.h"
#include "heap.h"
#include "validate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const jav_valtype_t V128R[1] = { WVT_V128 };
static const jav_valtype_t I32R[1]  = { WVT_I32 };

// (-> v128)  i32.const 5; i32x4.splat; i32.const 3; i32x4.splat; i32x4.add  -> {8,8,8,8}
static const uint8_t add4[] = { 0x41,0x05, 0xFD,0x11, 0x41,0x03, 0xFD,0x11, 0xFD,0xAE,0x01, 0x0B };
// i8x16.splat(10) + i8x16.splat(20) -> 30/lane (8-bit lanes; splat=0xFD 15, add=0xFD 110)
static const uint8_t add16[] = { 0x41,0x0A, 0xFD,0x0F, 0x41,0x14, 0xFD,0x0F, 0xFD,0x6E, 0x0B };
// f32x4.splat(3.0) * f32x4.splat(2.0) -> 6.0/lane (float lanes; splat=0xFD 19, mul=0xFD 230)
static const uint8_t mulf[] = { 0x43,0,0,0x40,0x40, 0xFD,0x13, 0x43,0,0,0,0x40, 0xFD,0x13, 0xFD,0xE6,0x01, 0x0B };

static int fails = 0;
static int run_t(const uint8_t* code, size_t n, const jav_valtype_t* res, unsigned nres, int jit, slot_t* out) {
    jav_vctx_t cx = {0}; cx.results = res; cx.nresults = nres; cx.nmemories = 1;
    jav_st_entry_t* sd; unsigned ns;
    if (!jav_typecheck(code, n, &cx, &sd, &ns)) { free(sd); return 0; }
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm);
    bbq_ctx_init(&vm.frame.code, code, n); vm.frame.sidetable = sd;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm, NULL);
    *out = jav_tos(&vm);
    jav_vm_free(&vm); free(sd);
    return st == JAV_RETURN;
}
static int run(const uint8_t* code, size_t n, int jit, slot_t* out) {
    return run_t(code, n, V128R, 1, jit, out);
}
// Does this code TYPECHECK (with an i32 result)? (for the lane-bound reject test)
static int typechecks(const uint8_t* code, size_t n) {
    jav_vctx_t cx = {0}; cx.results = I32R; cx.nresults = 1; cx.nmemories = 1;
    jav_st_entry_t* sd = NULL; unsigned ns;
    int ok = jav_typecheck(code, n, &cx, &sd, &ns);
    free(sd); return ok;
}

// Run a function that touches linear memory (v128.load/store), returning the i32 result.
static int run_mem(const uint8_t* code, size_t n, int jit, int* out) {
    jav_vctx_t cx = {0}; cx.results = I32R; cx.nresults = 1; cx.nmemories = 1;
    jav_st_entry_t* sd; unsigned ns;
    if (!jav_typecheck(code, n, &cx, &sd, &ns)) { free(sd); return 0; }
    struct heap_t heap; memset(&heap, 0, sizeof heap); jav_mem_add(&heap, 1, 1, 1, 0);
    vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;  vm.cluster.mem_addrs=(uint32_t[]){0}; vm.cluster.num_mems=1;
    bbq_ctx_init(&vm.frame.code, code, n); vm.frame.sidetable = sd;
    jav_status_t st = jit ? jav_jit_run(&vm) : interp_run(&vm, &heap);
    *out = jav_tos(&vm).i; jav_vm_free(&vm); jav_heap_free_mems(&heap); free(sd);
    return st == JAV_RETURN;
}

int main(void) {
    printf("SIMD lane ops (supported tier), interp == JIT:\n");
    slot_t ri, rj;
    int oki = run(add4, sizeof add4, 0, &ri);
    int okj = run(add4, sizeof add4, 1, &rj);
    int ok = oki && okj;
    for (int k = 0; k < 4; k++) ok = ok && ri.v.i32[k] == 8 && rj.v.i32[k] == 8;
    printf("  i32x4.splat(5)+splat(3)=8/lane  interp={%d,%d,%d,%d} jit={%d,%d,%d,%d} [%s]\n",
           ri.v.i32[0], ri.v.i32[1], ri.v.i32[2], ri.v.i32[3],
           rj.v.i32[0], rj.v.i32[1], rj.v.i32[2], rj.v.i32[3], ok ? "PASS" : "FAIL");
    fails += !ok;

    // i8x16: 8-bit lanes — 10+20=30 in every byte
    slot_t bi, bj; int ok8 = run(add16, sizeof add16, 0, &bi) && run(add16, sizeof add16, 1, &bj);
    for (int k = 0; k < 16; k++) ok8 = ok8 && bi.v.i8[k] == 30 && bj.v.i8[k] == 30;
    printf("  i8x16.splat(10)+splat(20)=30/byte  interp/jit [%s]\n", ok8 ? "PASS" : "FAIL");
    fails += !ok8;

    // f32x4: float lanes — 3.0*2.0=6.0 in every lane
    slot_t fi, fj; int okf = run(mulf, sizeof mulf, 0, &fi) && run(mulf, sizeof mulf, 1, &fj);
    for (int k = 0; k < 4; k++) okf = okf && fi.v.f32[k] == 6.0f && fj.v.f32[k] == 6.0f;
    printf("  f32x4.splat(3.0)*splat(2.0)=6.0/lane interp/jit [%s]\n", okf ? "PASS" : "FAIL");
    fails += !okf;

    // extract_lane / replace_lane: splat(7); const 9; i32x4.replace_lane 2; i32x4.extract_lane L
    //   -> {7,7,9,7}, so extract_lane 2 = 9, extract_lane 0 = 7.
    uint8_t ex2[] = { 0x41,0x07, 0xFD,0x11, 0x41,0x09, 0xFD,0x1C,0x02, 0xFD,0x1B,0x02, 0x0B };
    uint8_t ex0[] = { 0x41,0x07, 0xFD,0x11, 0x41,0x09, 0xFD,0x1C,0x02, 0xFD,0x1B,0x00, 0x0B };
    slot_t e2i, e2j, e0i, e0j;
    int oke = run_t(ex2, sizeof ex2, I32R, 1, 0, &e2i) && run_t(ex2, sizeof ex2, I32R, 1, 1, &e2j)
           && run_t(ex0, sizeof ex0, I32R, 1, 0, &e0i) && run_t(ex0, sizeof ex0, I32R, 1, 1, &e0j);
    oke = oke && e2i.i == 9 && e2j.i == 9 && e0i.i == 7 && e0j.i == 7;
    printf("  replace_lane 2:=9 then extract  lane2=%d/%d lane0=%d/%d [%s]\n",
           e2i.i, e2j.i, e0i.i, e0j.i, oke ? "PASS" : "FAIL");
    fails += !oke;

    // shift + neg: splat(1)<<4 = 16/lane ; neg(splat(5)) = -5/lane  (shl=0xFD 171, neg=0xFD 161)
    uint8_t shl4[] = { 0x41,0x01, 0xFD,0x11, 0x41,0x04, 0xFD,0xAB,0x01, 0x0B };
    uint8_t neg5[] = { 0x41,0x05, 0xFD,0x11, 0xFD,0xA1,0x01, 0x0B };
    slot_t si, sj, ni, nj;
    int oks = run(shl4, sizeof shl4, 0, &si) && run(shl4, sizeof shl4, 1, &sj)
           && run(neg5, sizeof neg5, 0, &ni) && run(neg5, sizeof neg5, 1, &nj);
    for (int k = 0; k < 4; k++) oks = oks && si.v.i32[k] == 16 && sj.v.i32[k] == 16
                                          && ni.v.i32[k] == -5 && nj.v.i32[k] == -5;
    printf("  i32x4.shl(1,4)=16/lane, neg(5)=-5/lane  interp/jit [%s]\n", oks ? "PASS" : "FAIL");
    fails += !oks;

    // float lane intrinsics: f32x4.sqrt(splat 4.0)=2.0/lane ; f32x4.min(2.0,5.0)=2.0/lane
    uint8_t fsqrt[] = { 0x43,0,0,0x80,0x40, 0xFD,0x13, 0xFD,0xE3,0x01, 0x0B };  // sqrt=227
    uint8_t fmin_[] = { 0x43,0,0,0,0x40, 0xFD,0x13, 0x43,0,0,0xA0,0x40, 0xFD,0x13, 0xFD,0xE8,0x01, 0x0B }; // min=232
    slot_t qi, qj, mi2, mj2;
    int okq = run(fsqrt, sizeof fsqrt, 0, &qi) && run(fsqrt, sizeof fsqrt, 1, &qj)
           && run(fmin_, sizeof fmin_, 0, &mi2) && run(fmin_, sizeof fmin_, 1, &mj2);
    for (int k = 0; k < 4; k++) okq = okq && qi.v.f32[k] == 2.0f && qj.v.f32[k] == 2.0f
                                          && mi2.v.f32[k] == 2.0f && mj2.v.f32[k] == 2.0f;
    printf("  f32x4.sqrt(4)=2/lane, min(2,5)=2/lane  interp/jit [%s]\n", okq ? "PASS" : "FAIL");
    fails += !okq;

    // comparison: i32x4.eq(5,5) -> all -1 ; i32x4.eq(5,3) -> all 0  (eq=0xFD 55)
    uint8_t eqt[] = { 0x41,0x05, 0xFD,0x11, 0x41,0x05, 0xFD,0x11, 0xFD,0x37, 0x0B };
    uint8_t eqf[] = { 0x41,0x05, 0xFD,0x11, 0x41,0x03, 0xFD,0x11, 0xFD,0x37, 0x0B };
    slot_t ti, tj, ui, uj;
    int okc = run(eqt, sizeof eqt, 0, &ti) && run(eqt, sizeof eqt, 1, &tj)
           && run(eqf, sizeof eqf, 0, &ui) && run(eqf, sizeof eqf, 1, &uj);
    for (int k = 0; k < 4; k++) okc = okc && ti.v.i32[k] == -1 && tj.v.i32[k] == -1
                                          && ui.v.i32[k] == 0 && uj.v.i32[k] == 0;
    printf("  i32x4.eq(5,5)=-1, eq(5,3)=0 per lane  interp/jit [%s]\n", okc ? "PASS" : "FAIL");
    fails += !okc;

    // v128.load/store round-trip through linear memory: store splat(42) at addr 0,
    // load it back, extract lane 0 -> 42. (store=0xFD 11, load=0xFD 0, both memarg 0,0)
    uint8_t ldst[] = { 0x41,0x00, 0x41,0x2A, 0xFD,0x11, 0xFD,0x0B,0x00,0x00,
                       0x41,0x00, 0xFD,0x00,0x00,0x00, 0xFD,0x1B,0x00, 0x0B };
    int li = 0, lj = 0;
    int okm = run_mem(ldst, sizeof ldst, 0, &li) && run_mem(ldst, sizeof ldst, 1, &lj);
    okm = okm && li == 42 && lj == 42;
    printf("  v128.store/load round-trip = %d/%d (want 42)  [%s]\n", li, lj, okm ? "PASS" : "FAIL");
    fails += !okm;

    // v128.const {2,3,4,5} (i32 lanes, LE) then i32x4.extract_lane 2 -> 4. (const=0xFD 12)
    uint8_t vc[] = { 0xFD,0x0C, 2,0,0,0, 3,0,0,0, 4,0,0,0, 5,0,0,0, 0xFD,0x1B,0x02, 0x0B };
    slot_t ci, cj;
    int okv = run_t(vc, sizeof vc, I32R, 1, 0, &ci) && run_t(vc, sizeof vc, I32R, 1, 1, &cj);
    okv = okv && ci.i == 4 && cj.i == 4;
    printf("  v128.const{2,3,4,5}.extract(2)=%d/%d (want 4)  [%s]\n", ci.i, cj.i, okv ? "PASS" : "FAIL");
    fails += !okv;

    // shuffle: a=splat(0x11), b=splat(0x22), indices {0,16,...} alternate a/b; lane0 from a.
    uint8_t sh[] = { 0x41,0x11, 0xFD,0x0F, 0x41,0x22, 0xFD,0x0F,
                     0xFD,0x0D, 0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16,
                     0xFD,0x16,0x00, 0x0B };
    slot_t hi2, hj2;
    int oksh = run_t(sh, sizeof sh, I32R, 1, 0, &hi2) && run_t(sh, sizeof sh, I32R, 1, 1, &hj2);
    oksh = oksh && hi2.i == 0x11 && hj2.i == 0x11;
    printf("  i8x16.shuffle lane0 from a = %d/%d (want 17)  [%s]\n", hi2.i, hj2.i, oksh ? "PASS" : "FAIL");
    fails += !oksh;

    // shuffle laneidx >= 32 must be rejected by the validator.
    uint8_t shbad[] = { 0x41,0x11, 0xFD,0x0F, 0x41,0x22, 0xFD,0x0F,
                        0xFD,0x0D, 32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0xFD,0x16,0x00, 0x0B };
    int shrej = !typechecks(shbad, sizeof shbad);
    printf("  shuffle laneidx 32 rejected  [%s]\n", shrej ? "PASS" : "FAIL");
    fails += !shrej;

    // The validator must REJECT an out-of-range laneidx (i32x4.extract_lane 4, dim=4).
    uint8_t badlane[] = { 0x41,0x07, 0xFD,0x11, 0xFD,0x1B,0x04, 0x0B };
    int rej = !typechecks(badlane, sizeof badlane);
    printf("  extract_lane 4 on i32x4 (dim 4) rejected  [%s]\n", rej ? "PASS" : "FAIL");
    fails += !rej;

    // --- Relaxed SIMD (each op lowered from one body -> interp == JIT by construction) ---

    // relaxed_madd (f32x4): a*b+c = 3*2+1 = 7.0/lane. (261 = 0xFD 0x85 0x02)
    uint8_t rmadd[] = { 0x43,0,0,0x40,0x40, 0xFD,0x13, 0x43,0,0,0,0x40, 0xFD,0x13,
                        0x43,0,0,0x80,0x3F, 0xFD,0x13, 0xFD,0x85,0x02, 0x0B };
    // relaxed_nmadd (f32x4): -(a*b)+c = -(3*2)+1 = -5.0/lane. (262 = 0xFD 0x86 0x02)
    uint8_t rnmadd[] = { 0x43,0,0,0x40,0x40, 0xFD,0x13, 0x43,0,0,0,0x40, 0xFD,0x13,
                         0x43,0,0,0x80,0x3F, 0xFD,0x13, 0xFD,0x86,0x02, 0x0B };
    slot_t mai, maj, nai, naj;
    int okrm = run(rmadd, sizeof rmadd, 0, &mai) && run(rmadd, sizeof rmadd, 1, &maj)
            && run(rnmadd, sizeof rnmadd, 0, &nai) && run(rnmadd, sizeof rnmadd, 1, &naj);
    for (int k = 0; k < 4; k++) okrm = okrm && mai.v.f32[k] == 7.0f && maj.v.f32[k] == 7.0f
                                            && nai.v.f32[k] == -5.0f && naj.v.f32[k] == -5.0f;
    printf("  f32x4.relaxed_madd=7, nmadd=-5 /lane  interp/jit [%s]\n", okrm ? "PASS" : "FAIL");
    fails += !okrm;

    // relaxed_laneselect (i32x4): (a&c)|(b&~c) with c=0 -> b = 0x34/lane. (267 = 0xFD 0x8B 0x02)
    uint8_t rlsel[] = { 0x41,0x12, 0xFD,0x11, 0x41,0x34, 0xFD,0x11, 0x41,0x00, 0xFD,0x11,
                        0xFD,0x8B,0x02, 0x0B };
    slot_t lsi, lsj; int okls = run(rlsel, sizeof rlsel, 0, &lsi) && run(rlsel, sizeof rlsel, 1, &lsj);
    for (int k = 0; k < 4; k++) okls = okls && lsi.v.i32[k] == 0x34 && lsj.v.i32[k] == 0x34;
    printf("  i32x4.relaxed_laneselect(c=0)=b=0x34/lane  interp/jit [%s]\n", okls ? "PASS" : "FAIL");
    fails += !okls;

    // relaxed_min (f32x4): a<b?a:b = min(3,2) = 2.0/lane. (269 = 0xFD 0x8D 0x02)
    uint8_t rmin[] = { 0x43,0,0,0x40,0x40, 0xFD,0x13, 0x43,0,0,0,0x40, 0xFD,0x13, 0xFD,0x8D,0x02, 0x0B };
    slot_t rmi, rmj; int okrmin = run(rmin, sizeof rmin, 0, &rmi) && run(rmin, sizeof rmin, 1, &rmj);
    for (int k = 0; k < 4; k++) okrmin = okrmin && rmi.v.f32[k] == 2.0f && rmj.v.f32[k] == 2.0f;
    printf("  f32x4.relaxed_min(3,2)=2/lane  interp/jit [%s]\n", okrmin ? "PASS" : "FAIL");
    fails += !okrmin;

    // relaxed_trunc_f32x4_s: trunc(2.5) = 2/lane. (257 = 0xFD 0x81 0x02)
    uint8_t rtr[] = { 0x43,0,0,0x20,0x40, 0xFD,0x13, 0xFD,0x81,0x02, 0x0B };
    slot_t tri, trj; int oktr = run(rtr, sizeof rtr, 0, &tri) && run(rtr, sizeof rtr, 1, &trj);
    for (int k = 0; k < 4; k++) oktr = oktr && tri.v.i32[k] == 2 && trj.v.i32[k] == 2;
    printf("  i32x4.relaxed_trunc_f32x4_s(2.5)=2/lane  interp/jit [%s]\n", oktr ? "PASS" : "FAIL");
    fails += !oktr;

    // relaxed_swizzle: a=splat(0x12), b=splat(0) -> every index 0 -> a[0]=0x12. (256 = 0xFD 0x80 0x02)
    uint8_t rswz[] = { 0x41,0x12, 0xFD,0x0F, 0x41,0x00, 0xFD,0x0F, 0xFD,0x80,0x02, 0x0B };
    slot_t swi, swj; int okswz = run(rswz, sizeof rswz, 0, &swi) && run(rswz, sizeof rswz, 1, &swj);
    for (int k = 0; k < 16; k++) okswz = okswz && swi.v.i8[k] == 0x12 && swj.v.i8[k] == 0x12;
    printf("  i8x16.relaxed_swizzle(idx0)=0x12/byte  interp/jit [%s]\n", okswz ? "PASS" : "FAIL");
    fails += !okswz;

    // relaxed_dot_s (i16x8 from i8x16): a=splat(2), b=splat(3) -> 2*3+2*3 = 12/lane. (274 = 0xFD 0x92 0x02)
    uint8_t rdot[] = { 0x41,0x02, 0xFD,0x0F, 0x41,0x03, 0xFD,0x0F, 0xFD,0x92,0x02, 0x0B };
    slot_t doi, doj; int okdot = run(rdot, sizeof rdot, 0, &doi) && run(rdot, sizeof rdot, 1, &doj);
    for (int k = 0; k < 8; k++) okdot = okdot && doi.v.i16[k] == 12 && doj.v.i16[k] == 12;
    printf("  i16x8.relaxed_dot_s(2,3)=12/lane  interp/jit [%s]\n", okdot ? "PASS" : "FAIL");
    fails += !okdot;

    // relaxed_dot_add_s (i32x4): 4 bytes/lane of 2*3, plus addend c=1 -> 4*6+1 = 25/lane. (275 = 0xFD 0x93 0x02)
    uint8_t rdota[] = { 0x41,0x02, 0xFD,0x0F, 0x41,0x03, 0xFD,0x0F, 0x41,0x01, 0xFD,0x11,
                        0xFD,0x93,0x02, 0x0B };
    slot_t dai, daj; int okdota = run(rdota, sizeof rdota, 0, &dai) && run(rdota, sizeof rdota, 1, &daj);
    for (int k = 0; k < 4; k++) okdota = okdota && dai.v.i32[k] == 25 && daj.v.i32[k] == 25;
    printf("  i32x4.relaxed_dot_add_s(2,3,+1)=25/lane  interp/jit [%s]\n", okdota ? "PASS" : "FAIL");
    fails += !okdota;

    // relaxed_q15mulr_s (i16x8): (16384*16384+16384)>>15 = 8192/lane. (273 = 0xFD 0x91 0x02)
    uint8_t rq15[] = { 0x41,0x80,0x80,0x01, 0xFD,0x10, 0x41,0x80,0x80,0x01, 0xFD,0x10,
                       0xFD,0x91,0x02, 0x0B };
    slot_t qqi, qqj; int okq15 = run(rq15, sizeof rq15, 0, &qqi) && run(rq15, sizeof rq15, 1, &qqj);
    for (int k = 0; k < 8; k++) okq15 = okq15 && qqi.v.i16[k] == 8192 && qqj.v.i16[k] == 8192;
    printf("  i16x8.relaxed_q15mulr_s(.5*1)=8192/lane  interp/jit [%s]\n", okq15 ? "PASS" : "FAIL");
    fails += !okq15;

    // Float vector compare yields an INTEGER all-ones mask (not the float -1.0): f32x4.eq(5,5)
    // then i32x4.extract_lane 0 must read 0xFFFFFFFF = -1, NOT 0xBF800000. (eq=0xFD 65)
    uint8_t feq[] = { 0x43,0,0,0xA0,0x40, 0xFD,0x13, 0x43,0,0,0xA0,0x40, 0xFD,0x13,
                      0xFD,0x41, 0xFD,0x1B,0x00, 0x0B };
    slot_t fei, fej; int okfe = run_t(feq, sizeof feq, I32R, 1, 0, &fei) && run_t(feq, sizeof feq, I32R, 1, 1, &fej);
    okfe = okfe && fei.i == -1 && fej.i == -1;
    printf("  f32x4.eq(5,5) mask = 0x%08X/0x%08X (want 0xFFFFFFFF)  interp/jit [%s]\n",
           (unsigned)fei.i, (unsigned)fej.i, okfe ? "PASS" : "FAIL");
    fails += !okfe;

    printf("\nSIMD lane ops: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
