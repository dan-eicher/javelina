// test_sigtab.c — the tier-2 storage-class signature vocabulary.
//
// The claims about the TABLE: its census, and that every opcode resolves into it.
// The expected numbers are literals here, recomputed from the tables rather than
// read back out of the generator's own #defines — a census that quotes the census
// is not a check. Regenerating after a wasm.def edit MOVES these numbers, and the
// pin going red is the design; the fix is recording the new counts first.
//
// The claim that the OTHER two artifacts agree with it — every terminal a TERM in
// jav_tile.burg, every terminal a constructor of matching arity in
// jav_ttree.asdl — belongs to `make tile-coverage`, where asdl and burgc parse
// their own formats with their own grammars. This file used to scan both as text,
// which is how it came to have a constructor scanner hard-coded to names starting
// `Sig_` that went quietly blind the day `Carried_` terminals appeared.
#include "jav_sigtab.h"

#include <stdio.h>
#include <string.h>

/* PIN A-1 — the census, per the plan's amendment of 2026-08-12 #10. A census is
 * not a lemma: these numbers move whenever the vocabulary legitimately grows, and
 * red means "the vocabulary changed, say so in the plan", not "something is
 * wrong". The lemmas are A-2's, below — every id in range, resolving only to
 * FINAL signatures, each a TERM at its own id and an ASDL constructor with
 * matching arity — and those hold across any count. */
#define WANT_OPCODES       499
#define WANT_TERMINALS     214   /* 198 + the wide-resolved POLY terminals. B2b
                                  * split the one PolyWide bucket by carrier —
                                  * `_pw` (a word slot went v128; the __sK_pw
                                  * family serves it) vs `_aw` (an any slot did;
                                  * fenced) — un-fusing the 7 shapes a word-op
                                  * and an any-op had shared (39 -> 17+27+2). */
#define WANT_OPEN_SIGS      34   /* declared shapes with an ADDR or POLY slot */
#define WANT_MULTI_RESULT    0
#define WANT_MAX_ARITY       5

static int fails = 0;
static void CK(const char* msg, long got, long want) {
    int ok = (got == want);
    printf("  %-58s %6ld  [%s]\n", msg, got, ok ? "PASS" : "FAIL");
    fails += !ok;
}

/* Every opcode row, top-level and prefixed alike. A sub-table is a dense array
 * indexed by subopcode: unfilled slots inside it are zeroed and `present` tells
 * them apart, and its length says where it ends. */
static void each_opcode(void (*fn)(const jav_opcode_sig_t*, void*), void* ud) {
    for (int i = 0; i < 256; i++) {
        if (jav_opcode_sig[i].present) fn(&jav_opcode_sig[i], ud);
        const jav_opcode_sig_t* sub = jav_opcode_sig_sub[i];
        for (int s = 0; sub && s < jav_opcode_sig_sub_len[i]; s++)
            if (sub[s].present) fn(&sub[s], ud);
    }
}

static void count_present(const jav_opcode_sig_t* r, void* ud) { (void)r; (*(long*)ud)++; }

/* PIN A-2's first half, per row: the declared id is in range, and every
 * signature it resolves to is FINAL — an opcode that resolved to an unresolved
 * signature would hand the tree builder a terminal that is not one. */
static long bad_decl, bad_res, no_res;
static void check_resolution(const jav_opcode_sig_t* r, void* ud) {
    (void)ud;
    if (r->sig >= JAV_SIG_COUNT) { bad_decl++; return; }
    const jav_sig_t* d = &jav_sigtab[r->sig];
    if (d->nresolve == 0) { no_res++; return; }
    for (int k = 0; k < d->nresolve; k++)
        if (d->resolves_to[k] >= JAV_SIG_COUNT || !jav_sigtab[d->resolves_to[k]].final)
            bad_res++;
}

int main(void) {
    printf("sigtab: the tier-2 storage-class signature vocabulary\n");

    /* ── PIN A-1: SigtabMatchesSpec ─────────────────────────── */
    long opcodes = 0;
    each_opcode(count_present, &opcodes);
    CK("opcodes carrying a signature", opcodes, WANT_OPCODES);

    long terminals = 0, multi = 0, max_arity = 0, nonfinal = 0;
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        const jav_sig_t* s = &jav_sigtab[i];
        if (!s->final) { nonfinal++; continue; }
        terminals++;
        if (s->nresults > 1) multi++;
        if (s->nkids > max_arity) max_arity = s->nkids;
    }
    CK("storage-class signatures (burg terminals)", terminals, WANT_TERMINALS);
    CK("declared shapes still carrying a resolution marker", nonfinal, WANT_OPEN_SIGS);
    CK("terminals with more than one result", multi, WANT_MULTI_RESULT);
    CK("max arity", max_arity, WANT_MAX_ARITY);

    /* A `final` row must have no resolution marker left in it, and a non-final
     * row must have at least one — otherwise the flag is decoration. */
    long mislabelled = 0;
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        const jav_sig_t* s = &jav_sigtab[i];
        int open = 0;
        for (int k = 0; k < s->nparams; k++)  open |= s->params[k]  >= JAV_SCLASS_FINAL;
        for (int k = 0; k < s->nresults; k++) open |= s->results[k] >= JAV_SCLASS_FINAL;
        mislabelled += (open == (int)s->final);
    }
    CK("rows whose `final` flag disagrees with their slots", mislabelled, 0);

    /* nkids is the burg arity: the params that are tree children. The variadic
     * group is a signature slot the matcher never sees, because opgen leaves
     * those operands on the value stack for the body to read in place. */
    long kid_mismatch = 0;
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        const jav_sig_t* s = &jav_sigtab[i];
        int kids = 0;
        for (int k = 0; k < s->nparams; k++) kids += (s->params[k] != JSC_STK);
        kid_mismatch += (kids != s->nkids);
    }
    CK("rows whose nkids is not their non-stack param count", kid_mismatch, 0);

    /* ── PIN A-2: EverySignatureIsATerm ─────────────────────── */
    bad_decl = bad_res = no_res = 0;
    each_opcode(check_resolution, NULL);
    CK("opcodes with an out-of-range signature id", bad_decl, 0);
    CK("opcodes whose signature resolves to nothing", no_res, 0);
    CK("resolutions that are not final signatures", bad_res, 0);

    /* Every terminal names itself, and no two share a name — the names ARE the
     * identity the .burg and the .asdl carry, so a collision would silently merge
     * two rules there. (That those two files hold the same set at the same
     * arities is checked by `make tile-coverage`, which parses them.) */
    long unnamed = 0, dup_names = 0;
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        if (!jav_sigtab[i].final) continue;
        if (!jav_sigtab[i].name || !jav_sigtab[i].name[0]) { unnamed++; continue; }
        for (int k = 0; k < i; k++)
            if (jav_sigtab[k].final && strcmp(jav_sigtab[k].name, jav_sigtab[i].name) == 0)
                dup_names++;
    }
    CK("terminals with no name", unnamed, 0);
    CK("terminals sharing a name", dup_names, 0);

    /* ── PIN A-3: WideMarksAreEarned ────────────────────────
     * `pw` and `aw` are WIDTH claims, so a row wearing either must actually
     * hold a v128 slot — a mark with no wide slot prices movements that do
     * not exist. And the split is identity, not annotation: the (v128)->()
     * shape must exist BOTH ways, because drop's word moves two registers
     * through the poly-wide family while throw_ref's any moves one carrier
     * slot, and one shared terminal carried rules wrong for one of them.
     * Falsified by folding aw back into pw in sigemit's resolve(). */
    long idle_marks = 0, split_pw = 0, split_aw = 0;
    for (int i = 0; i < JAV_SIG_COUNT; i++) {
        const jav_sig_t* s = &jav_sigtab[i];
        if (!s->final || (!s->pw && !s->aw)) continue;
        int wide = 0;
        for (int k = 0; k < s->nparams; k++)  wide |= s->params[k]  == JSC_V128;
        for (int k = 0; k < s->nresults; k++) wide |= s->results[k] == JSC_V128;
        idle_marks += !wide;
        if (s->nparams == 1 && s->params[0] == JSC_V128 && !s->nresults) {
            if (s->pw && !s->aw) split_pw++;
            if (s->aw && !s->pw) split_aw++;
        }
    }
    CK("pw/aw rows with no v128 slot", idle_marks, 0);
    CK("(v128)->() as a poly-wide terminal (drop's)", split_pw, 1);
    CK("(v128)->() as an any-wide terminal (throw_ref's)", split_aw, 1);

    printf("%s\n", fails ? "  FAILED" : "  ok");
    return fails != 0;
}
