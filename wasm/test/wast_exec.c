// wast_exec.c — the .wast EXECUTION conformance runner. It drives the PUBLIC wasm.h C-API and
// NOTHING ELSE: every module is decoded (wasm_module_new), instantiated (wasm_instance_new with a
// positional import vector the harness resolves by name, exactly as §7.1 embedding requires), and
// every action runs through wasm_func_call / wasm_global_get. The engine therefore makes every
// type and semantic decision; this file only parses the .wast script and checks the §6.5 result
// oracle. The ONE non-wasm.h readout is jav_capi_last_status/error — the §5/§4.5 verdict the spec
// surfaces to an embedder only as NULL+trap, which a conformance harness legitimately needs to tell
// malformed / invalid / unlinkable / uninstantiable / trap apart against what the .wast asserts.
#define _POSIX_C_SOURCE 200809L      // sigsetjmp/siglongjmp/_exit (engine-fault isolation)
#include "wasm.h"                     // the public C-API — the ONLY engine entry points this file calls
#include "jav_extern.h"                // jav_capi_last_status/error (sanctioned readout) + value types/enums
#include "wast_sexpr.h"
#include "wast_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

// A conformance runner must survive an engine FAULT (e.g. a SIGFPE from an unguarded div/rem
// overflow) — isolate each invocation so a crash is a counted failure, not process death. The
// fault is reported (a Phase-3 escape), never silently swallowed.
static sigjmp_buf g_fault_jmp;
static volatile sig_atomic_t g_in_call;
static void fault_handler(int sig) { if (g_in_call) { g_in_call = 0; siglongjmp(g_fault_jmp, sig); } else _exit(128 + sig); }

// Out-of-enum sentinels for outcomes the §5/§4.5 jav_status_t can't name: a harness-side issue or an
// engine fault (instantiate/invoke), distinct from a real trap/exception.
#define ACT_NOFUNC ((jav_status_t)-100)   // malformed action (no name)
#define ACT_CRASH  ((jav_status_t)-101)   // engine FAULT (Phase-3 escape)
#define ACT_NOMOD  ((jav_status_t)-102)   // target module absent / didn't instantiate
#define ACT_NOEXP  ((jav_status_t)-103)   // named export missing / wrong kind
#define ACT_BADARG ((jav_status_t)-104)   // an argument value form unsupported
#define ACT_EXN    ((jav_status_t)-105)   // §7.1.8 an uncaught exception escaped (vs a trap)

static int g_exec_ok, g_exec_bad, g_exec_excl;
static int g_trap_msgbad;          // trapped correctly, but for the WRONG reason (message ≠ .wast string)
static char g_trap_msg[256];       // the reason the last trapping action reported
static const char *g_excl_reason = "none";
typedef struct { const char *r; int n; } ExclReason;   // per-reason tally (named, reconcilable)
static ExclReason *g_excl_tally;                        // bbq_vec
void wast_exec_note_excl(const char *r) {
    g_exec_excl++; g_excl_reason = r;
    for (int i = 0; i < (int)bbq_vec_len(g_excl_tally); i++)
        if (strcmp(g_excl_tally[i].r, r) == 0) { g_excl_tally[i].n++; return; }
    ExclReason e = { r, 1 }; bbq_vec_push(g_excl_tally, e);
}
void wast_exec_counts(int *ok, int *bad, int *excl, const char **reason) {
    *ok = g_exec_ok; *bad = g_exec_bad; *excl = g_exec_excl; *reason = g_excl_reason;
}
int wast_exec_trap_msgbad(void) { return g_trap_msgbad; }
// Itemized exclusion ledger — every "excluded" is counted under a named reason, so the bucket is
// auditable (no silent drops).
void wast_exec_print_breakdown(FILE *f) {
    fprintf(f, "  execution exclusions by reason (total %d):\n", g_exec_excl);
    int sum = 0;
    for (int i = 0; i < (int)bbq_vec_len(g_excl_tally); i++) {
        fprintf(f, "    %7d  %s\n", g_excl_tally[i].n, g_excl_tally[i].r); sum += g_excl_tally[i].n;
    }
    if (sum != g_exec_excl) fprintf(f, "    !! RECONCILE FAIL: itemized %d != counted %d\n", sum, g_exec_excl);
}

///////////////////////////////////////////////////////////////////////////////
// Harness state. ONE wasm_engine + ONE wasm_store per .wast file (§4.2.3): the store holds the
// shared heap, GC, and every instance. Modules + their exports are tracked here so the harness can
// resolve named imports into the positional vector wasm_instance_new consumes.

static wasm_engine_t *g_engine;
static wasm_store_t  *g_store;

// One instantiated (or attempted) module: its decoded form, its instance (NULL unless it linked +
// ran cleanly), and its exports paired with their field names (so imports resolve by name).
typedef struct {
    char           *id;          // script-local $id (NULL if anonymous)
    wasm_module_t  *module;      // decoded module (owns its byte copy)
    wasm_instance_t *instance;   // NULL unless instantiation succeeded
    wasm_extern_t **exports;     // bbq_vec, parallel to names — borrowed-as-imports by later modules
    char          **names;       // bbq_vec of export field names (raw bytes; WASM names may embed NUL)
    int            *name_lens;   // bbq_vec, parallel: each name's true byte length (NOT strlen)
    int             ok;
} EMod;
static EMod **g_mods;            // bbq_vec, retained per-file
static EMod  *g_last;            // default action target (most recent OK module)

typedef struct { char *name; int nlen; EMod *mod; } RegEntry;   // (register "name" $id) → importable module
static RegEntry *g_reg;          // bbq_vec

typedef struct { char *id; uint8_t *buf; size_t len; } DefMod;   // (module definition $id …): bytes, not yet instantiated
static DefMod *g_defs;           // bbq_vec

static uint8_t *g_spec_bytes;    // retained spectest image — re-instantiated into each file's store
static size_t   g_spec_len;

// §6.5 host-reference space: (ref.extern N) denotes a stable host reference identified by N. We mint
// one wasm_foreign per N (interned), so the SAME N yields the SAME reference (eq) and a returned
// externref can be matched back to its N via wasm_ref_as_foreign.
typedef struct { uint64_t n; wasm_foreign_t *f; } ExtRef;
static ExtRef *g_externrefs;     // bbq_vec

static char *dupz(const char *s, int n) { char *r = malloc(n + 1); memcpy(r, s, n); r[n] = 0; return r; }

static wasm_foreign_t *foreign_for(uint64_t n) {
    for (int i = 0; i < (int)bbq_vec_len(g_externrefs); i++)
        if (g_externrefs[i].n == n) return g_externrefs[i].f;
    wasm_foreign_t *f = wasm_foreign_new(g_store);
    ExtRef e = { n, f }; bbq_vec_push(g_externrefs, e);
    return f;
}

///////////////////////////////////////////////////////////////////////////////
// Value marshaling. WVal is the parsed .wast literal (script side); it is converted to a wasm_val_t
// for arguments and compared against a wasm_val_t the engine produced for results.
//
// is_nan: 0 exact / 1 canonical / 2 arithmetic (scalar f32/f64). For v128, `vshape` is the lane
// shape (0 i8x16 .. 5 f64x2) and `lane_nan[i]` the per-lane NaN class of a float shape. ref_wild is
// a bare (ref.func)/(ref.extern) — "some non-null reference of that kind". is_null marks the
// (ref.null ht) form; any other T_REF carries the host handle N of (ref.extern N) in v.r.
//
// is_null is an explicit FLAG, deliberately: this previously overloaded the value, spelling null as
// v.r == JAV_NULLREF, which baked the engine's internal null bit pattern into the ORACLE. That held
// only while the sentinel happened to be a value no test used as a host handle — the moment null
// moved (to 0, per §2.3.4's reserved tag bit), `(ref.extern 0)` and `(ref.null)` became the same
// parsed value and 14 assert_returns started comparing equal to the wrong thing. A test harness
// must not encode the representation it is checking.
// has_ref_kind: an abstract-heaptype reference assertion ((ref.i31)/(ref.struct)/(ref.array)/
// (ref.eq)/(ref.any)/(ref.none)) — the result must be a non-null reference whose §7.1.14 runtime
// type ≤ ref_kind (§7.1.15). ref_wild is the looser bare (ref.func)/(ref.extern) "any non-null".
typedef struct { slot_t v; uint8_t t; int is_nan; uint8_t vshape; uint8_t lane_nan[16]; uint8_t ref_wild;
                 uint8_t has_ref_kind; uint8_t is_null; wasm_valkind_t ref_kind; } WVal;
static char *tokz(const Node *n) { return dupz(n->tok, n->tlen); }
static uint64_t parse_int(const char *s) {
    char buf[80]; int k = 0, neg = 0; const char *p = s;
    if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
    for (; *p && k < 79; p++) if (*p != '_') buf[k++] = *p; buf[k] = 0;
    // §6.3.1: a WASM integer literal is hex (0x…) or DECIMAL — a leading zero is NOT octal. Pick the
    // base explicitly (strtoull base 0 would mis-read "012345" as octal).
    int base = (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) ? 16 : 10;
    uint64_t u = strtoull(buf, NULL, base);
    return neg ? (uint64_t)(-(int64_t)u) : u;
}
// §6.3.3 float literal → exact bits, matching the .wat parser's rounding (strtof for f32, not
// (float)strtod which double-rounds). NaN forms: `nan`/`nan:0x<payload>` are EXACT bit patterns
// (is_nan=0); `nan:canonical`/`nan:arithmetic` are CLASSES (is_nan=1/2, §6.3.3).
static void parse_fp(const char *s, int is64, WVal *w) {
    w->t = is64 ? T_DOUBLE : T_FLOAT; w->is_nan = 0;
    int neg = 0; const char *p = s;
    if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
    uint64_t qbit = is64 ? 0x0008000000000000ull : 0x00400000ull;        // significand MSB (quiet)
    uint64_t expmask = is64 ? 0x7ff0000000000000ull : 0x7f800000ull;     // exponent all-ones
    uint64_t signb = is64 ? 0x8000000000000000ull : 0x80000000ull;
    if (strncmp(p, "nan", 3) == 0) {
        uint64_t b = (neg ? signb : 0) | expmask;
        if      (strncmp(p + 3, ":canonical", 10) == 0)  { w->is_nan = 1; b |= qbit; }
        else if (strncmp(p + 3, ":arithmetic", 11) == 0) { w->is_nan = 2; b |= qbit; }
        else if (p[3] == ':') b |= (strtoull(p + 4, NULL, 0) & (is64 ? 0x000fffffffffffffull : 0x7fffffu)); // nan:0x<payload>, exact
        else b |= qbit;                                  // bare `nan` = canonical bits, exact
        if (is64) memcpy(&w->v.d, &b, 8); else { uint32_t b32 = (uint32_t)b; memcpy(&w->v.f, &b32, 4); }
        return;
    }
    if (strncmp(p, "inf", 3) == 0) { if (is64) w->v.d = neg ? -INFINITY : INFINITY; else w->v.f = neg ? -INFINITY : INFINITY; return; }
    char buf[256]; int k = 0; for (const char *q = p; *q && k < 255; q++) if (*q != '_') buf[k++] = *q; buf[k] = 0;
    if (is64) { double d = strtod(buf, NULL); w->v.d = neg ? -d : d; }
    else      { float  f = strtof(buf, NULL); w->v.f = neg ? -f : f; }
}
static int parse_wval(const Node *n, WVal *w) {
    memset(w, 0, sizeof *w);
    if (!n->is_list || n->nkids < 1) return 0;
    const Node *h = n->kids[0];
    if (tok_is(h, "i32.const") && n->nkids > 1) { char *s = tokz(n->kids[1]); w->t = T_INT;  w->v.i = (int32_t)parse_int(s); free(s); return 1; }
    if (tok_is(h, "i64.const") && n->nkids > 1) { char *s = tokz(n->kids[1]); w->t = T_LONG; w->v.l = (int64_t)parse_int(s); free(s); return 1; }
    if (tok_is(h, "f32.const") && n->nkids > 1) { char *s = tokz(n->kids[1]); parse_fp(s, 0, w); free(s); return 1; }
    if (tok_is(h, "f64.const") && n->nkids > 1) { char *s = tokz(n->kids[1]); parse_fp(s, 1, w); free(s); return 1; }
    if (tok_is(h, "v128.const") && n->nkids > 2) {       // §6.5.10 (v128.const shape lane*)
        char *shp = tokz(n->kids[1]);
        int isf = (shp[0] == 'f'), lanew, nlane, shape;
        if      (!strcmp(shp, "i8x16")) { lanew = 1; nlane = 16; shape = 0; }
        else if (!strcmp(shp, "i16x8")) { lanew = 2; nlane = 8;  shape = 1; }
        else if (!strcmp(shp, "i32x4")) { lanew = 4; nlane = 4;  shape = 2; }
        else if (!strcmp(shp, "i64x2")) { lanew = 8; nlane = 2;  shape = 3; }
        else if (!strcmp(shp, "f32x4")) { lanew = 4; nlane = 4;  shape = 4; }
        else if (!strcmp(shp, "f64x2")) { lanew = 8; nlane = 2;  shape = 5; }
        else { free(shp); return 0; }
        free(shp);
        if (n->nkids - 2 != nlane) return 0;
        w->t = T_V128; w->vshape = (uint8_t)shape;
        for (int i = 0; i < nlane; i++) {
            char *ls = tokz(n->kids[2 + i]);
            if (!isf) { uint64_t u = parse_int(ls);
                if (lanew == 1) w->v.v.u8[i] = (uint8_t)u; else if (lanew == 2) w->v.v.u16[i] = (uint16_t)u;
                else if (lanew == 4) w->v.v.u32[i] = (uint32_t)u; else w->v.v.u64[i] = u;
            } else { WVal lw; memset(&lw, 0, sizeof lw); parse_fp(ls, lanew == 8, &lw);
                w->lane_nan[i] = (uint8_t)lw.is_nan;
                if (lanew == 4) memcpy(&w->v.v.f32[i], &lw.v.f, 4); else memcpy(&w->v.v.f64[i], &lw.v.d, 8);
            }
            free(ls);
        }
        return 1;
    }
    if (tok_is(h, "ref.null")) {   // §6.5: a null ref's hierarchy is fixed by its heaptype operand
        w->t = T_REF; w->is_null = 1; w->v.r = 0; w->ref_kind = WASM_FUNCREF;
        if (n->nkids > 1) {
            char *ht = tokz(n->kids[1]);
            if      (!strcmp(ht, "extern") || !strcmp(ht, "noextern")) w->ref_kind = WASM_EXTERNREF;
            else if (!strcmp(ht, "func")   || !strcmp(ht, "nofunc"))   w->ref_kind = WASM_FUNCREF;
            else if (!strcmp(ht, "exn")    || !strcmp(ht, "noexn"))    w->ref_kind = WASM_EXNREF;
            else                                                        w->ref_kind = WASM_ANYREF;  // any/eq/i31/struct/array/none
            free(ht);
        }
        return 1;
    }
    if ((tok_is(h, "ref.extern") || tok_is(h, "ref.host")) && n->nkids > 1) {   // a host reference with handle N
        char *s = tokz(n->kids[1]); w->t = T_REF; w->v.r = (ref_t)parse_int(s); free(s);
        w->ref_kind = tok_is(h, "ref.host") ? WASM_ANYREF : WASM_EXTERNREF;   // host ref = any hierarchy; extern ref = extern
        return 1; }
    // §6.5 abstract-heaptype reference assertions: "some non-null ref whose runtime type ≤ this".
    if (tok_is(h, "ref.i31"))    { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_I31REF;    return 1; }
    if (tok_is(h, "ref.struct")) { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_STRUCTREF; return 1; }
    if (tok_is(h, "ref.array"))  { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_ARRAYREF;  return 1; }
    if (tok_is(h, "ref.eq"))     { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_EQREF;      return 1; }
    if (tok_is(h, "ref.any"))    { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_ANYREF;     return 1; }
    if (tok_is(h, "ref.none"))   { w->t = T_REF; w->has_ref_kind = 1; w->ref_kind = WASM_NULLREF;    return 1; }
    // Bare (ref.func) / (ref.extern): the wast wildcard for "some non-null reference" of that kind.
    if (tok_is(h, "ref.func") || tok_is(h, "ref.extern")) { w->t = T_REF; w->ref_wild = 1; return 1; }
    return 0;
}

// WVal → wasm_val_t for an argument. A null reference carries its heaptype's hierarchy in ref_kind
// (so wasm_func_call's §7.1.8 arg typing accepts it against the matching param); (ref.extern N) rides
// as the interned host foreign for N, wrapped as an externref.
static void wval_to_val(const WVal *w, wasm_val_t *out) {
    memset(out, 0, sizeof *out);
    switch (w->t) {
        case T_INT:    out->kind = WASM_I32; out->of.i32 = w->v.i; break;
        case T_LONG:   out->kind = WASM_I64; out->of.i64 = w->v.l; break;
        case T_FLOAT:  out->kind = WASM_F32; out->of.f32 = w->v.f; break;
        case T_DOUBLE: out->kind = WASM_F64; out->of.f64 = w->v.d; break;
        case T_V128:   out->kind = WASM_V128; memcpy(out->of.v128.bytes, &w->v.v, 16); break;
        default: /* T_REF */
            if (w->is_null) { out->kind = w->ref_kind ? w->ref_kind : WASM_FUNCREF; out->of.ref = NULL; }
            else if (w->ref_wild) { out->kind = WASM_FUNCREF; out->of.ref = NULL; }
            else { out->kind = w->ref_kind ? w->ref_kind : WASM_EXTERNREF; out->of.ref = wasm_foreign_as_ref(foreign_for((uint64_t)(u4)w->v.r)); }
            break;
    }
}

// Engine-produced wasm_val_t `got` vs the parsed expected `exp` (§6.5 result oracle).
static int val_match(const wasm_val_t *got, const WVal *exp) {
    switch (exp->t) {
    case T_INT:   return got->of.i32 == exp->v.i;
    case T_LONG:  return got->of.i64 == exp->v.l;
    case T_FLOAT:  { uint32_t g; memcpy(&g, &got->of.f32, 4);
        if (exp->is_nan == 1) return (g & 0x7fffffffu) == 0x7fc00000u;                 // §6.3.3 canonical NaN
        if (exp->is_nan == 2) return (g & 0x7f800000u) == 0x7f800000u && (g & 0x00400000u); // arithmetic (quiet) NaN
        uint32_t e; memcpy(&e, &exp->v.f, 4); return g == e; }
    case T_DOUBLE: { uint64_t g; memcpy(&g, &got->of.f64, 8);
        if (exp->is_nan == 1) return (g & 0x7fffffffffffffffull) == 0x7ff8000000000000ull;
        if (exp->is_nan == 2) return (g & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (g & 0x0008000000000000ull);
        uint64_t e; memcpy(&e, &exp->v.d, 8); return g == e; }
    case T_REF:
        if (exp->has_ref_kind) {                                                // §7.1.14/15: runtime type ≤ ref_kind
            if (got->of.ref == NULL) return 0;
            wasm_valtype_t *rt = wasm_ref_type(g_store, got->of.ref), *at = wasm_valtype_new(exp->ref_kind);
            int ok = wasm_match_valtype(g_store, rt, at);
            wasm_valtype_delete(rt); wasm_valtype_delete(at);
            return ok;
        }
        if (exp->ref_wild) return got->of.ref != NULL;                          // wildcard: any non-null ref
        if (exp->is_null) return got->of.ref == NULL;                           // (ref.null): the null reference
        return got->of.ref != NULL &&                                          // (ref.extern N): the host handle N
               wasm_ref_as_foreign(got->of.ref) == foreign_for((uint64_t)(u4)exp->v.r);
    case T_V128:
        if (exp->vshape < 4) return memcmp(got->of.v128.bytes, &exp->v.v, 16) == 0;   // i8x16..i64x2: exact bytes
        if (exp->vshape == 4) for (int i = 0; i < 4; i++) {                            // f32x4: per-lane, NaN-class aware
            uint32_t g; memcpy(&g, got->of.v128.bytes + 4 * i, 4);
            if (exp->lane_nan[i] == 1) { if ((g & 0x7fffffffu) != 0x7fc00000u) return 0; }
            else if (exp->lane_nan[i] == 2) { if (!((g & 0x7f800000u) == 0x7f800000u && (g & 0x00400000u))) return 0; }
            else { uint32_t e; memcpy(&e, &exp->v.v.f32[i], 4); if (g != e) return 0; }
        } else for (int i = 0; i < 2; i++) {                                          // f64x2
            uint64_t g; memcpy(&g, got->of.v128.bytes + 8 * i, 8);
            if (exp->lane_nan[i] == 1) { if ((g & 0x7fffffffffffffffull) != 0x7ff8000000000000ull) return 0; }
            else if (exp->lane_nan[i] == 2) { if (!((g & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (g & 0x0008000000000000ull))) return 0; }
            else { uint64_t e; memcpy(&e, &exp->v.v.f64[i], 8); if (g != e) return 0; }
        }
        return 1;
    default: return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Module tracking + name-based import resolution.

static EMod *mod_by_id(const Node *idn) {
    if (!idn) return g_last;
    for (int i = (int)bbq_vec_len(g_mods) - 1; i >= 0; i--)
        if (g_mods[i]->id && tok_is(idn, g_mods[i]->id)) return g_mods[i];
    return g_last;
}
// Names are compared by their TRUE byte length, never strlen — a WASM name (§6.3.4) may embed NUL.
static EMod *reg_by_name(const uint8_t *name, size_t len) {
    for (int i = (int)bbq_vec_len(g_reg) - 1; i >= 0; i--)
        if ((size_t)g_reg[i].nlen == len && memcmp(g_reg[i].name, name, len) == 0) return g_reg[i].mod;
    return NULL;
}
static wasm_extern_t *find_export(EMod *m, const uint8_t *field, size_t len) {
    for (int i = 0; i < (int)bbq_vec_len(m->names); i++)
        if ((size_t)m->name_lens[i] == len && memcmp(m->names[i], field, len) == 0) return m->exports[i];
    return NULL;
}
// Pair each instance export with its field name (wasm_instance_exports + wasm_module_exports share
// the module's export order), so imports of this module resolve by name.
static void build_exports(EMod *m) {
    wasm_extern_vec_t ev; wasm_instance_exports(m->instance, &ev);
    wasm_exporttype_vec_t tv; wasm_module_exports(m->module, &tv);
    for (size_t i = 0; i < ev.size; i++) {
        bbq_vec_push(m->exports, ev.data[i]);
        const wasm_name_t *nm = (i < tv.size) ? wasm_exporttype_name(tv.data[i]) : NULL;
        int nl = nm ? (int)nm->size : 0;
        bbq_vec_push(m->names, dupz(nm ? nm->data : "", nl));
        bbq_vec_push(m->name_lens, nl);
    }
    free(ev.data);            // keep the export handles (stored in m->exports); free only the vec shell
    wasm_exporttype_vec_delete(&tv);
}

// Decode `bytes` (the module owns its copy), resolve declared imports by name into a positional
// vector, and instantiate. Returns the §5/§4.5 verdict (read from the engine via jav_capi_last_*):
// MALFORMED/INVALID (decode/validate), UNLINKABLE (a missing name OR an engine link mismatch),
// UNINSTANTIABLE (a start/elem trap), or OK.
static jav_status_t instantiate_emod(EMod *m, const uint8_t *bytes, size_t len, const char *id) {
    m->id = id ? dupz(id, (int)strlen(id)) : NULL;
    wasm_byte_vec_t bin = { len, (wasm_byte_t *)(uintptr_t)bytes };
    m->module = wasm_module_new(g_store, &bin);
    if (!m->module) return jav_capi_last_status(g_store);   // MALFORMED or INVALID

    wasm_importtype_vec_t imps; wasm_module_imports(m->module, &imps);
    wasm_extern_vec_t importvec; importvec.size = imps.size;
    importvec.data = imps.size ? calloc(imps.size, sizeof(wasm_extern_t *)) : NULL;
    int unresolved = 0;
    for (size_t i = 0; i < imps.size; i++) {
        const wasm_name_t *mod = wasm_importtype_module(imps.data[i]);
        const wasm_name_t *fld = wasm_importtype_name(imps.data[i]);
        EMod *src = reg_by_name((const uint8_t *)mod->data, mod->size);
        wasm_extern_t *ext = src ? find_export(src, (const uint8_t *)fld->data, fld->size) : NULL;
        if (!ext) { unresolved = 1; break; }               // §4.5.2: the embedder can't supply this name → unlinkable
        importvec.data[i] = ext;
    }
    wasm_importtype_vec_delete(&imps);
    if (unresolved) { free(importvec.data); return JAV_UNLINKABLE; }

    wasm_trap_t *trap = NULL;
    if (sigsetjmp(g_fault_jmp, 1)) { free(importvec.data); return ACT_CRASH; }   // a fault in start/elem
    g_in_call = 1;
    m->instance = wasm_instance_new(g_store, m->module, &importvec, &trap);
    g_in_call = 0;
    free(importvec.data);                                  // borrowed export handles — free the shell only
    jav_status_t s = jav_capi_last_status(g_store);
    if (m->instance) { m->ok = 1; build_exports(m); }      // s == JAV_OK
    if (trap) wasm_trap_delete(trap);                      // (the trapped instance, if any, is store-owned)
    return s;
}

static void free_emod_inst(EMod *m) { if (m->instance) { wasm_instance_delete(m->instance); m->instance = NULL; } }
static void free_emod_rest(EMod *m) {
    for (int i = 0; i < (int)bbq_vec_len(m->exports); i++) wasm_extern_delete(m->exports[i]);
    bbq_vec_free(m->exports);
    for (int i = 0; i < (int)bbq_vec_len(m->names); i++) free(m->names[i]);
    bbq_vec_free(m->names);
    bbq_vec_free(m->name_lens);
    if (m->module) wasm_module_delete(m->module);
    free(m->id); free(m);
}

///////////////////////////////////////////////////////////////////////////////
// Per-file scope. ONE store per .wast file; spectest is re-instantiated + registered into it.

// Free the current file's objects in dependency order: OK instances before the heap they live in;
// then the store (which frees any trapped instances — those still reference their modules); then the
// modules and the rest.
static void file_scope_free(void) {
    for (int i = 0; i < (int)bbq_vec_len(g_mods); i++) free_emod_inst(g_mods[i]);
    if (g_store) { wasm_store_delete(g_store); g_store = NULL; }
    for (int i = 0; i < (int)bbq_vec_len(g_mods); i++) free_emod_rest(g_mods[i]);
    bbq_vec_clear(g_mods);
    for (int i = 0; i < (int)bbq_vec_len(g_defs); i++) { free(g_defs[i].id); free(g_defs[i].buf); }
    bbq_vec_clear(g_defs);
    for (int i = 0; i < (int)bbq_vec_len(g_reg); i++) free(g_reg[i].name);
    bbq_vec_clear(g_reg);
    // The interned host foreigns are harness-owned; the store is already gone (no host box still
    // references them), so delete them now.
    for (int i = 0; i < (int)bbq_vec_len(g_externrefs); i++) wasm_foreign_delete(g_externrefs[i].f);
    bbq_vec_clear(g_externrefs);
    g_last = NULL;
}

static void register_mod(const uint8_t *name, size_t nlen, EMod *m) {
    RegEntry e = { dupz((const char *)name, (int)nlen), (int)nlen, m }; bbq_vec_push(g_reg, e);
}

// ── public API ──
int wast_exec_store_init(void) {
    g_engine = wasm_engine_new();
    if (!g_engine) return 0;
    struct sigaction sa; memset(&sa, 0, sizeof sa);     // persistent + re-entrant (SA_NODEFER)
    sa.sa_handler = fault_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = SA_NODEFER;
    sigaction(SIGFPE, &sa, NULL); sigaction(SIGSEGV, &sa, NULL); sigaction(SIGILL, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
    return 1;
}
void wast_exec_spectest(uint8_t *bytes, size_t len) {
    g_spec_bytes = bytes; g_spec_len = len;   // retained; re-instantiated into each file's store
}
void wast_exec_file_reset(void) {
    file_scope_free();
    g_store = wasm_store_new(g_engine);
    if (g_spec_bytes) {                                   // re-instantiate + register spectest into the new store
        EMod *sp = calloc(1, sizeof *sp);
        if (instantiate_emod(sp, g_spec_bytes, g_spec_len, NULL) == JAV_OK) {
            bbq_vec_push(g_mods, sp);
            register_mod((const uint8_t *)"spectest", 8, sp);
        } else { free_emod_inst(sp); free_emod_rest(sp); }
    }
}
// ONE module channel: decode+instantiate, compare the verdict to what the .wast asserts. `expect`
// is the §5/§4.5 outcome: JAV_OK (a plain module / definition — kept + registerable), UNLINKABLE
// (assert_unlinkable), UNINSTANTIABLE (assert_uninstantiable), or JAV_TRAP (assert_trap on a module
// whose start traps — UNINSTANTIABLE is the same outcome, accepted too).
void wast_exec_module(uint8_t *bytes, size_t len, const char *id, int expect) {
    EMod *m = calloc(1, sizeof *m);
    jav_status_t s = instantiate_emod(m, bytes, len, id);
    free(bytes);                                          // wasm_module_new copied them
    int ok = (expect == JAV_TRAP) ? (s == JAV_TRAP || s == JAV_UNINSTANTIABLE) : ((int)s == expect);
    if (ok && expect == JAV_OK) { bbq_vec_push(g_mods, m); g_last = m; g_exec_ok++; return; }
    if (ok) g_exec_ok++;
    else { g_exec_bad++; if (getenv("WAST_E")) fprintf(stderr, "  EXEC module expect=%d got=%d\n", expect, s); }
    // §4.5.4: an UNINSTANTIABLE module's trapped instance is store-owned (it persists for shared-table
    // references); keep the EMod so its module outlives that instance until file teardown. UNLINKABLE /
    // malformed / invalid allocated nothing persistent → free now.
    if (s == JAV_UNINSTANTIABLE) { bbq_vec_push(g_mods, m); return; }
    free_emod_inst(m); free_emod_rest(m);
}
void wast_exec_register(const char *name, int nlen, const Node *idn) {
    EMod *m = mod_by_id(idn);
    if (m && m->ok) register_mod((const uint8_t *)name, (size_t)nlen, m);
}
// (module definition $id …): assembled bytes kept for later instantiation. Takes ownership of `bytes`.
void wast_exec_define(const char *id, uint8_t *bytes, size_t len) {
    DefMod d = { id ? dupz(id, (int)strlen(id)) : NULL, bytes, len };
    bbq_vec_push(g_defs, d);
}
// (module instance $inst $def): instantiate the named definition GENERATIVELY (§4.7.2 — a fresh
// instance per command), tracked under $inst like any other module.
void wast_exec_instance(const char *id_inst, const char *id_def) {
    for (int i = (int)bbq_vec_len(g_defs) - 1; i >= 0; i--)
        if (g_defs[i].id && id_def && strcmp(g_defs[i].id, id_def) == 0) {
            uint8_t *copy = malloc(g_defs[i].len ? g_defs[i].len : 1);
            memcpy(copy, g_defs[i].buf, g_defs[i].len);
            wast_exec_module(copy, g_defs[i].len, id_inst, JAV_OK);   // owns `copy`; tracks under $inst
            return;
        }
    wast_exec_note_excl("target module definition not found");
}

///////////////////////////////////////////////////////////////////////////////
// Actions. invoke/get run through wasm_func_call / wasm_global_get; the engine sets every result
// kind and makes every semantic decision.

// Run an invoke/get action. On JAV_RETURN, *nres results are written to res[] (the caller owns any
// reference handle and deletes them). On a trap → JAV_TRAP; on an escaped exception → ACT_EXN.
static jav_status_t run_action(const Node *act, int *nres, wasm_val_t **res) {
    *nres = 0; *res = NULL;
    g_trap_msg[0] = 0;      // never let the previous action's reason answer for this one
    int ai = 1; const Node *idn = NULL;
    if (ai < act->nkids && !act->kids[ai]->is_str) { idn = act->kids[ai]; ai++; }
    if (ai >= act->nkids || !act->kids[ai]->is_str) return ACT_NOFUNC;
    static uint8_t namebuf[2048]; int nl = decode_str(act->kids[ai], namebuf); namebuf[nl] = 0; ai++;
    EMod *m = mod_by_id(idn);
    if (!m || !m->ok) return ACT_NOMOD;

    if (head_is(act, "get")) {
        wasm_extern_t *e = find_export(m, namebuf, (size_t)nl);
        if (!e || wasm_extern_kind(e) != WASM_EXTERN_GLOBAL) return ACT_NOEXP;
        *res = malloc(sizeof **res);
        wasm_global_get(wasm_extern_as_global(e), &(*res)[0]);
        *nres = 1; return JAV_RETURN;                     // global read (by value)
    }

    wasm_extern_t *e = find_export(m, namebuf, (size_t)nl);
    if (!e || wasm_extern_kind(e) != WASM_EXTERN_FUNC) return ACT_NOEXP;
    wasm_func_t *fn = wasm_extern_as_func(e);
    int nargs = act->nkids - ai;
    wasm_val_vec_t args; wasm_val_vec_new_uninitialized(&args, (size_t)nargs);
    for (int j = 0; j < nargs; j++) {
        WVal w; if (!parse_wval(act->kids[ai + j], &w)) { wasm_val_vec_delete(&args); return ACT_BADARG; }
        wval_to_val(&w, &args.data[j]);
    }
    size_t narr = wasm_func_result_arity(fn);
    wasm_val_vec_t results; wasm_val_vec_new_uninitialized(&results, narr);

    jav_status_t ret;
    if (sigsetjmp(g_fault_jmp, 1)) { wasm_val_vec_delete(&args); free(results.data); return ACT_CRASH; }
    g_in_call = 1;
    wasm_trap_t *trap = wasm_func_call(fn, &args, &results);
    g_in_call = 0;
    if (trap) {
        ret = wasm_trap_is_exception(trap) ? ACT_EXN : JAV_TRAP;
        wasm_message_t msg; wasm_trap_message(trap, &msg);   // the §7.10 reason, for the assert_trap match
        size_t c = msg.size < sizeof g_trap_msg - 1 ? msg.size : sizeof g_trap_msg - 1;
        memcpy(g_trap_msg, msg.data, c); g_trap_msg[c] = 0;
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
        free(results.data);                               // no results were produced (uninitialized)
    } else {
        *res = results.data;                              // hand the WHOLE array to the caller (uncapped; caller frees)
        *nres = (int)narr;
        ret = JAV_RETURN;
    }
    wasm_val_vec_delete(&args);                           // frees arg ref handles (not the interned foreigns)
    return ret;
}

// Free the result array run_action handed back: each value's ref handle, then the array itself.
static void free_res(wasm_val_t *res, int n) { for (int i = 0; i < n; i++) wasm_val_delete(&res[i]); free(res); }

void wast_exec_action(const Node *act) {
    int nr; wasm_val_t *r = NULL; jav_status_t s = run_action(act, &nr, &r);
    if (s == JAV_RETURN) free_res(r, nr);
}
// A non-runnable run_action status → its named exclusion reason (NULL = a real outcome to check).
static const char *act_excl_reason(int s) {
    switch (s) {
    case ACT_NOMOD:  return "target module not instantiated (cross-instance import)";
    case ACT_NOEXP:  return "named export missing";
    case ACT_BADARG:
    case ACT_NOFUNC: return "argument value form unsupported";
    default:         return NULL;
    }
}
// Match a result against an expected node: 1 match, 0 mismatch, -1 unsupported value form. Handles
// the §6.5 (either …) form (relaxed-SIMD non-determinism): the result must equal ANY alternative.
static int match_expected(const wasm_val_t *got, const Node *e) {
    if (head_is(e, "either")) {
        int unsupported = 0;
        for (int k = 1; k < e->nkids; k++) { int r = match_expected(got, e->kids[k]); if (r == 1) return 1; if (r < 0) unsupported = 1; }
        return unsupported ? -1 : 0;
    }
    WVal w; if (!parse_wval(e, &w)) return -1;
    return val_match(got, &w) ? 1 : 0;
}
void wast_exec_assert_return(const Node *cmd) {
    if (cmd->nkids < 2) return;
    int nr; wasm_val_t *res = NULL;
    jav_status_t s = run_action(cmd->kids[1], &nr, &res);
    if (s == ACT_CRASH) { g_exec_bad++; g_excl_reason = "engine FAULT (Phase-3 escape)"; if (getenv("WAST_E")) fprintf(stderr, "  EXEC assert_return CRASH\n"); return; }
    const char *xr = act_excl_reason(s); if (xr) { wast_exec_note_excl(xr); return; }
    int nexp = cmd->nkids - 2; int good = (s == JAV_RETURN) && nr == nexp;
    for (int j = 0; good && j < nexp; j++) {
        int r = match_expected(&res[j], cmd->kids[2 + j]);
        if (r < 0) { if (s == JAV_RETURN) free_res(res, nr); wast_exec_note_excl("expected value form unsupported"); return; }
        if (r == 0) good = 0;
    }
    if (getenv("WAST_DBG") && !good) {
        int slen = (cmd->s1 && cmd->s0 && cmd->s1 > cmd->s0) ? (int)(cmd->s1 - cmd->s0) : 0;
        fprintf(stderr, "MM [%.*s] st=%d nr=%d nexp=%d", slen > 150 ? 150 : slen, cmd->s0 ? cmd->s0 : "", s, nr, nexp);
        for (int j = 0; j < nr && j < nexp && s == JAV_RETURN; j++) {
            fprintf(stderr, " | g%d k=%d ", j, res[j].kind);
            if (res[j].kind == WASM_V128) { for (int b = 0; b < 16; b++) fprintf(stderr, "%02x", res[j].of.v128.bytes[b]); }
            else fprintf(stderr, "%016llx", (unsigned long long)res[j].of.i64);
        }
        fprintf(stderr, "\n");
    }
    if (s == JAV_RETURN) free_res(res, nr);
    if (good) g_exec_ok++; else { g_exec_bad++; if (getenv("WAST_E")) fprintf(stderr, "  EXEC assert_return FAIL status=%d nr=%d nexp=%d\n", s, nr, nexp); }
}
void wast_exec_assert_trap(const Node *cmd) {
    if (cmd->nkids < 2) return;
    const Node *act = cmd->kids[1];
    if (!head_is(act, "invoke") && !head_is(act, "get")) { wast_exec_note_excl("trap on instantiation (start)"); return; }
    int nr; wasm_val_t *res = NULL;
    jav_status_t s = run_action(act, &nr, &res);
    if (s == JAV_RETURN) free_res(res, nr);
    if (s == ACT_CRASH) { g_exec_bad++; g_excl_reason = "engine FAULT (Phase-3 escape)"; if (getenv("WAST_E")) fprintf(stderr, "  EXEC assert_trap CRASH\n"); return; }
    const char *xr = act_excl_reason(s); if (xr) { wast_exec_note_excl(xr); return; }
    if (s != JAV_TRAP) {
        g_exec_bad++;
        if (getenv("WAST_E")) fprintf(stderr, "  EXEC assert_trap NOT-TRAP status=%d\n", s);
        return;
    }
    // Right verdict; now the REASON. The .wast carries the expected §7.10 message as the
    // trailing "…" token — trapping for the wrong cause is a conformance failure the
    // verdict alone cannot see, and is counted separately so the debt is visible.
    // assert_exhaustion shares this path but is NOT a §7.10 trap: the reference
    // interpreter raises Exhaustion, not Trapping, and instructions.toml excludes host
    // exhaustion from `traps` on purpose. There is no spec reason to match it against.
    if (head_is(cmd, "assert_exhaustion")) { g_exec_ok++; return; }

    char want[256]; want[0] = 0;                       // tok/tlen is a span, not NUL-terminated
    if (cmd->nkids > 2 && cmd->kids[2]->is_str) {
        int wl = cmd->kids[2]->tlen; if (wl > (int)sizeof want - 1) wl = (int)sizeof want - 1;
        memcpy(want, cmd->kids[2]->tok, (size_t)wl); want[wl] = 0;
    }
    if (!wast_msg_matches(g_trap_msg, want[0] ? want : NULL)) {
        g_trap_msgbad++;
        if (getenv("WAST_VV"))
            fprintf(stderr, "  EXEC TRAP-REASON expect=%s got=\"%s\"\n", want, g_trap_msg);
        return;
    }
    g_exec_ok++;
}
void wast_exec_assert_exception(const Node *cmd) {
    if (cmd->nkids < 2) return;
    const Node *act = cmd->kids[1];
    if (!head_is(act, "invoke")) { wast_exec_note_excl("exception on non-invoke action"); return; }
    int nr; wasm_val_t *res = NULL;
    jav_status_t s = run_action(act, &nr, &res);
    if (s == JAV_RETURN) free_res(res, nr);
    if (s == ACT_CRASH) { g_exec_bad++; g_excl_reason = "engine FAULT (Phase-3 escape)"; return; }
    const char *xr = act_excl_reason(s); if (xr) { wast_exec_note_excl(xr); return; }
    if (s == ACT_EXN) g_exec_ok++; else { g_exec_bad++; if (getenv("WAST_E")) fprintf(stderr, "  EXEC assert_exception NOT-EXN status=%d\n", s); }
}
void wast_exec_teardown(void) {
    file_scope_free();
    bbq_vec_free(g_mods); bbq_vec_free(g_defs); bbq_vec_free(g_reg); bbq_vec_free(g_externrefs);
    free(g_spec_bytes);
    if (g_engine) wasm_engine_delete(g_engine);
}
