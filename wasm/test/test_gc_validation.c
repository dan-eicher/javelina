// test_gc_validation.c — the GC validator's §3.4 instruction obligations + §3.2.11
// type-section structural check, each proven as an accept/reject pair (the corpus
// CONFIRMS breadth; these are the spec-enumerated negatives that DISCOVER it).
//
// Part A (cx-based jav_typecheck): per-instruction obligations — mutability, packed
//   get_s/u vs plain get, defaultability, array.new_data/elem element kind, copy storage.
// Part B (inline .wasm → jav_module_validate): §3.2.11 declared-supertype validity +
//   the §3.3.8/§3.3.9 structural composite/field match (variance, width, finality, arity).
#include "validate.h"
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "bbq_arena.h"
#include "jav_subtype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
static void CK(const char* msg, int got, int want) {
    int ok = (got == want);
    printf("  %-58s [%s]\n", msg, ok ? "PASS" : "FAIL"); fails += !ok;
}

// ── the type universe shared by Part A ──────────────────────────────────────────
// $0 struct{ i32 mut, i32 const, i8 mut(packed) }   $1 struct{ (ref $0) non-null }
// $2 struct{ i32 mut }   $3 array{i32 mut}   $4 array{i8 mut(packed)}
// $5 array{(ref null func) mut}   $6 array{i32 const}   $7 array{(ref func) non-null mut}
static const jav_valtype_t S0F[3] = { WVT_I32, WVT_I32, WVT_I32 };
static const uint32_t      S0X[3] = { 0, 0, 0 };
static const uint8_t       S0M[3] = { 1, 0, 1 };
static const jav_valtype_t S1F[1] = { WVT_REF_NN };  static const uint32_t S1X[1] = { 0 };   static const uint8_t S1M[1] = { 0 };
static const jav_valtype_t S2F[1] = { WVT_I32 };     static const uint32_t S2X[1] = { 0 };   static const uint8_t S2M[1] = { 1 };
static const jav_structtype_t STRUCTS[8] = {
    { S0F, S0X, 3, S0M }, { S1F, S1X, 1, S1M }, { S2F, S2X, 1, S2M }, {0}, {0}, {0}, {0}, {0},
};
static const jav_arraytype_t ARRAYS[8] = {
    {0}, {0}, {0},
    { WVT_I32, 0, 1 },                       // $3 i32 mut
    { WVT_I32, 0, 1 },                       // $4 i8 mut (unpacked stored as i32; pack code below)
    { WVT_REF, (uint32_t)HT_FUNC, 1 },       // $5 (ref null func) mut
    { WVT_I32, 0, 0 },                       // $6 i32 const
    { WVT_REF_NN, (uint32_t)HT_FUNC, 1 },    // $7 (ref func) non-null mut
};
static const uint8_t PK0[3] = { 0, 0, 1 };   // $0 field 2 is i8
static const uint8_t PK4[1] = { 1 };         // $4 element is i8
static const uint8_t PKZ[1] = { 0 };
static const uint8_t* const PACKS[8] = { PK0, PKZ, PKZ, PKZ, PK4, PKZ, PKZ, PKZ };
static const uint8_t  KINDS[8] = { WST_STRUCT, WST_STRUCT, WST_STRUCT, WST_ARRAY, WST_ARRAY, WST_ARRAY, WST_ARRAY, WST_ARRAY };
static const int32_t  SUPERS[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
static const jav_subtype_ctx_t LAT = { KINDS, SUPERS, 8 };

static const jav_valtype_t LOC[6]  = { WVT_REF, WVT_REF, WVT_REF, WVT_REF, WVT_REF, WVT_REF };
static const uint32_t      LOCT[6] = { 0, 3, 4, 5, 6, 7 };   // (ref null $0/$3/$4/$5/$6/$7)
static const jav_valtype_t ELEMRT[1] = { WVT_REF };
static const uint32_t      ELEMTX[1] = { (uint32_t)HT_FUNC };  // one funcref element segment

static int validates(const uint8_t* code, size_t n) {
    jav_st_entry_t* st; unsigned k;
    jav_vctx_t cx = {0};
    cx.locals = LOC; cx.local_tidx = LOCT; cx.nlocals = 6;
    cx.structtypes = STRUCTS; cx.nstructtypes = 8;
    cx.arraytypes = ARRAYS; cx.narraytypes = 8;
    cx.type_field_packs = PACKS; cx.num_type_field_packs = 8;
    cx.ndatas = 1; cx.nelems = 1; cx.elem_reftype = ELEMRT; cx.elem_tidx = ELEMTX;
    cx.lattice = &LAT;
    int ok = jav_typecheck(code, n, &cx, &st, &k);
    if (ok) bbq_vec_free(st);
    return ok;
}

// ── Part B: build a module from inline bytes, return a 3-STATE verdict ───────────
// -1 = MALFORMED (failed to decode/index: a test-encoding bug, NOT a §7 verdict);
//  0 = well-formed but INVALID (the §7 reject we want to prove); 1 = VALID.
// Distinguishing these is the point: a wrong size byte must surface as -1, never
// masquerade as a "reject" — otherwise the error-path test proves nothing.
static int module_verdict(const uint8_t* bytes, size_t n) {
    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module((uint8_t*)bytes, n, &a);
    int r = -1;
    if (m.success) {
        jav_modidx_t mod;
        if (jav_module_index(m.root, bytes, &a, &mod)) {
            jav_err_t err;
            r = (jav_module_validate(m.root, bytes, &mod, &err) == JAV_OK) ? 1 : 0;
        }
    }
    bbq_arena_free(&a);
    return r;
}
#define WASM_HDR 0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00

int main(void) {
    // ════ Part A: §3.4 per-instruction obligations ════
    // struct.set: mutable field accepts, immutable field rejects (§3.4.7).
    static const uint8_t a1[] = { 0x20,0x00, 0x41,0x00, 0xfb,0x05,0x00,0x00, 0x0b };  // struct.set $0 0 (mut)
    static const uint8_t a2[] = { 0x20,0x00, 0x41,0x00, 0xfb,0x05,0x00,0x01, 0x0b };  // struct.set $0 1 (const)
    CK("struct.set on a mutable field accepts", validates(a1,sizeof a1), 1);
    CK("struct.set on an immutable field rejects", validates(a2,sizeof a2), 0);

    // struct.get: plain get INVALID on a packed field; valid on an unpacked field.
    static const uint8_t a3[] = { 0x20,0x00, 0xfb,0x02,0x00,0x00, 0x1a, 0x0b };  // get $0 0 (unpacked)
    static const uint8_t a4[] = { 0x20,0x00, 0xfb,0x02,0x00,0x02, 0x1a, 0x0b };  // get $0 2 (packed)
    CK("struct.get (plain) on an unpacked field accepts", validates(a3,sizeof a3), 1);
    CK("struct.get (plain) on a packed field rejects", validates(a4,sizeof a4), 0);

    // struct.get_s/u: REQUIRES a packed field; rejected on an unpacked one.
    static const uint8_t a5[] = { 0x20,0x00, 0xfb,0x03,0x00,0x02, 0x1a, 0x0b };  // get_s $0 2 (packed)
    static const uint8_t a6[] = { 0x20,0x00, 0xfb,0x04,0x00,0x00, 0x1a, 0x0b };  // get_u $0 0 (unpacked)
    CK("struct.get_s on a packed field accepts", validates(a5,sizeof a5), 1);
    CK("struct.get_u on an unpacked field rejects", validates(a6,sizeof a6), 0);

    // struct.new_default: every field must be defaultable (§3.4.7).
    static const uint8_t a7[] = { 0xfb,0x01,0x02, 0x1a, 0x0b };  // new_default $2 (i32 field)
    static const uint8_t a8[] = { 0xfb,0x01,0x01, 0x1a, 0x0b };  // new_default $1 ((ref $0) non-null field)
    CK("struct.new_default with defaultable fields accepts", validates(a7,sizeof a7), 1);
    CK("struct.new_default with a non-null ref field rejects", validates(a8,sizeof a8), 0);

    // array.set / array.fill: element must be mutable.
    static const uint8_t a9[]  = { 0x20,0x01, 0x41,0x00, 0x41,0x00, 0xfb,0x0e,0x03, 0x0b };  // set $3 (mut)
    static const uint8_t a10[] = { 0x20,0x04, 0x41,0x00, 0x41,0x00, 0xfb,0x0e,0x06, 0x0b };  // set $6 (const)
    CK("array.set on a mutable array accepts", validates(a9,sizeof a9), 1);
    CK("array.set on an immutable array rejects", validates(a10,sizeof a10), 0);

    // array.get: plain get INVALID on a packed element.
    static const uint8_t a11[] = { 0x20,0x01, 0x41,0x00, 0xfb,0x0b,0x03, 0x1a, 0x0b };  // get $3 (i32)
    static const uint8_t a12[] = { 0x20,0x02, 0x41,0x00, 0xfb,0x0b,0x04, 0x1a, 0x0b };  // get $4 (i8 packed)
    CK("array.get (plain) on an unpacked element accepts", validates(a11,sizeof a11), 1);
    CK("array.get (plain) on a packed element rejects", validates(a12,sizeof a12), 0);

    // array.get_s requires a packed element.
    static const uint8_t a13[] = { 0x20,0x02, 0x41,0x00, 0xfb,0x0c,0x04, 0x1a, 0x0b };  // get_s $4 (packed)
    static const uint8_t a14[] = { 0x20,0x01, 0x41,0x00, 0xfb,0x0c,0x03, 0x1a, 0x0b };  // get_s $3 (unpacked)
    CK("array.get_s on a packed element accepts", validates(a13,sizeof a13), 1);
    CK("array.get_s on an unpacked element rejects", validates(a14,sizeof a14), 0);

    // array.new_default: element must be defaultable (a non-null ref is not).
    static const uint8_t a15[] = { 0x41,0x00, 0xfb,0x07,0x03, 0x1a, 0x0b };  // new_default $3 (i32)
    static const uint8_t a16[] = { 0x41,0x00, 0xfb,0x07,0x07, 0x1a, 0x0b };  // new_default $7 ((ref func) non-null)
    CK("array.new_default with a defaultable element accepts", validates(a15,sizeof a15), 1);
    CK("array.new_default with a non-null ref element rejects", validates(a16,sizeof a16), 0);

    // array.new_data: element must be num/vec; array.new_elem: element must be a ref ⊒ seg.
    static const uint8_t a17[] = { 0x41,0x00, 0x41,0x00, 0xfb,0x09,0x03,0x00, 0x1a, 0x0b };  // new_data $3 (i32)
    static const uint8_t a18[] = { 0x41,0x00, 0x41,0x00, 0xfb,0x09,0x05,0x00, 0x1a, 0x0b };  // new_data $5 (ref)
    CK("array.new_data with a numeric element accepts", validates(a17,sizeof a17), 1);
    CK("array.new_data with a reference element rejects", validates(a18,sizeof a18), 0);
    static const uint8_t a19[] = { 0x41,0x00, 0x41,0x00, 0xfb,0x0a,0x05,0x00, 0x1a, 0x0b };  // new_elem $5 (ref ⊒ funcref)
    static const uint8_t a20[] = { 0x41,0x00, 0x41,0x00, 0xfb,0x0a,0x03,0x00, 0x1a, 0x0b };  // new_elem $3 (i32)
    CK("array.new_elem with a matching ref element accepts", validates(a19,sizeof a19), 1);
    CK("array.new_elem with a numeric element rejects", validates(a20,sizeof a20), 0);

    // array.copy: dest mutable + src storage ≤ dest storage (i8 ⊄ i32).
    static const uint8_t a21[] = { 0x20,0x01,0x41,0x00, 0x20,0x01,0x41,0x00, 0x41,0x00, 0xfb,0x11,0x03,0x03, 0x0b };  // copy $3<-$3
    static const uint8_t a22[] = { 0x20,0x01,0x41,0x00, 0x20,0x02,0x41,0x00, 0x41,0x00, 0xfb,0x11,0x03,0x04, 0x0b };  // copy $3<-$4 (i8)
    CK("array.copy with matching element storage accepts", validates(a21,sizeof a21), 1);
    CK("array.copy from a packed to an unpacked element rejects", validates(a22,sizeof a22), 0);

    // ════ Part B: §3.2.11 type-section structural validation ════
    // Each entry: 0x50 sub-open / 0x4F sub-final, supercount, supers*, 0x5F struct,
    // fieldcount, then (0x7F i32, mut)*. Sizes verified by hand; a wrong size surfaces
    // as verdict -1 (malformed), which FAILS the == 0 / == 1 assertions below.
    // valid: $1 = sub $0 widening struct{i32 mut} to struct{i32 mut, i32 mut}.
    static const uint8_t m_ok[] = { WASM_HDR, 0x01,0x10, 0x02,
        0x50,0x00, 0x5F,0x01, 0x7F,0x01,                       // $0 sub() struct{ i32 mut }            (6)
        0x50,0x01,0x00, 0x5F,0x02, 0x7F,0x01, 0x7F,0x01 };     // $1 sub($0) struct{ i32 mut, i32 mut } (9)
    CK("§3.2.11 width-extension sub type accepts", module_verdict(m_ok,sizeof m_ok), 1);

    // invalid: $1 declares fewer fields than $0 (width violation).
    static const uint8_t m_narrow[] = { WASM_HDR, 0x01,0x10, 0x02,
        0x50,0x00, 0x5F,0x02, 0x7F,0x01, 0x7F,0x01,            // $0 struct{ i32 mut, i32 mut }         (8)
        0x50,0x01,0x00, 0x5F,0x01, 0x7F,0x01 };                // $1 sub($0) struct{ i32 mut } ← fewer  (7)
    CK("§3.2.11 sub type with fewer fields rejects", module_verdict(m_narrow,sizeof m_narrow), 0);

    // invalid: supertype is final.
    static const uint8_t m_final[] = { WASM_HDR, 0x01,0x0E, 0x02,
        0x4F,0x00, 0x5F,0x01, 0x7F,0x00,                       // $0 sub final() struct{ i32 const }    (6)
        0x50,0x01,0x00, 0x5F,0x01, 0x7F,0x00 };                // $1 sub($0) — $0 is final ← reject     (7)
    CK("§3.2.11 sub type of a final type rejects", module_verdict(m_final,sizeof m_final), 0);

    // invalid: field mutability differs (const in super, mut in sub).
    static const uint8_t m_mut[] = { WASM_HDR, 0x01,0x0E, 0x02,
        0x50,0x00, 0x5F,0x01, 0x7F,0x00,                       // $0 struct{ i32 const }                (6)
        0x50,0x01,0x00, 0x5F,0x01, 0x7F,0x01 };                // $1 sub($0) struct{ i32 mut } ← variance (7)
    CK("§3.2.11 sub type changing mutability rejects", module_verdict(m_mut,sizeof m_mut), 0);

    // invalid: more than one declared supertype.
    static const uint8_t m_multi[] = { WASM_HDR, 0x01,0x15, 0x03,
        0x50,0x00, 0x5F,0x01, 0x7F,0x01,                       // $0 struct{ i32 mut }                  (6)
        0x50,0x00, 0x5F,0x01, 0x7F,0x01,                       // $1 struct{ i32 mut }                  (6)
        0x50,0x02,0x00,0x01, 0x5F,0x01, 0x7F,0x01 };           // $2 sub($0,$1) — two supers ← reject   (8)
    CK("§3.2.11 sub type with two supertypes rejects", module_verdict(m_multi,sizeof m_multi), 0);

    // GUARD: a deliberately malformed module (wrong size) must read as -1, NOT 0 — proving
    // the helper distinguishes a parse failure from a §7 reject (so the rejects above are real).
    static const uint8_t m_bad[] = { WASM_HDR, 0x01,0x05, 0x02, 0x50,0x00, 0x5F,0x01, 0x7F,0x01 };
    CK("malformed module reads as -1 (not a false reject)", module_verdict(m_bad,sizeof m_bad), -1);

    printf("\nGC validation obligations (§3.4 instructions + §3.2.11 sub types): %s\n", fails?"FAIL":"ALL PASS");
    return fails ? 1 : 0;
}
