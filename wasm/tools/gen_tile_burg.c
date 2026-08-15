/* gen_tile_burg.c — the tier-2 tiling grammar, from the two authorities that
 * decide it:
 *
 *   src/gen/jav_sigtab.h        the storage-class signature vocabulary — the
 *                               terminals, and which opcodes resolve to each.
 *   src/gen/jav_stencil_table.h every stencil's MEASURED code_size, as jitterator
 *                               extracted it from the compiled object.
 *
 * The grammar cannot come out of opgen, which is where the terminals do: which
 * (signature, state) pairs the variant family actually PROVIDES is not known until
 * the stencils have been compiled and jitterator has read the table — downstream of
 * opgen. So the grammar is emitted here, and a rule exists exactly
 * when the stencil it would stamp exists. That is also §2.5's omission rule for
 * free: a (signature, state) pair the variant family does not provide has no
 * rule, and the tiler reaches it through a transition instead of guessing.
 *
 * A rule's COST is a separate question, and it is not the stencil's size. Bytes
 * cannot see a dispatch, so a cover priced in them prices the wrong thing; the
 * scale is Ertl's own (§2.6, printed 36) and it is derived from the SIGNATURE —
 * how many words move, and whether a dispatch happens that otherwise would not.
 * See rule_cost(). The measured code_size is still read, for the omission rule
 * above and for the size figures the plan's G3 records; it no longer prices.
 *
 * Nothing here parses anything. Both inputs are C data this program includes.
 */
#include "jav_jit_meta.h"      /* jav_jit_meta[] — opcode -> stencil */
#include "jav_sigtab.h"        /* the vocabulary */
#include "jav_stencil_table.h" /* the measurement */
#include "jav_ttree.h"         /* JAV_TILE_CLS_BITS — the action's packing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ertl's MINIMAL organization (§2.3, Fig 2.14/2.16): one cache state per NUMBER
 * of cached items, not per assignment of items to registers. In state k the top k
 * stack items live in reg0..reg[k-1], reg0 being the topmost. That is what keeps
 * the rule set linear in n instead of exponential in the arity: an operand's
 * location is decided by its distance from the top, so a signature of arity a
 * contributes one rule per entry state, not one per combination. */
/* The ceiling is the TILE MAP's packing, not this generator: `in_pack`/`out_pack`
 * carry JAV_TILE_CLS_BITS (4) per slot in a uint32, so eight slots fit. The other
 * two limits are elsewhere and are both empirical — the stencil signature is
 * `vm + n slots` against preserve_none's 12 argument registers, and clang's own
 * spilling inside a stencil body once too few registers are left to work in.
 *
 * n=4 is the first size at which a BINARY SIMD op can have both operands cached,
 * since a v128 spends two slots; below it that is structurally impossible. */
#define MAX_N 8

static const char* const kClassName[] = { "i32", "i64", "f32", "f64", "v128", "ref" };

/* Where operand `i` of an arity-`a` signature sits in entry state `k`: the top of
 * the stack is the LAST operand, so distance from the top is a-1-i. */
/* The slot operand `i` of an arity-`a` signature STARTS at: the widths of
 * everything above it. Items are not slots — a v128 spends two — so this is a sum
 * and not a subtraction, and `jav_class_width` is the one place that knows. */
static uint8_t kid_class(const jav_sig_t* s, int i) {
    int kid = 0;
    for (int p = 0; p < s->nparams; p++) {
        if (s->params[p] == JSC_STK) continue;
        if (kid == i) return s->params[p];
        kid++;
    }
    return JSC_COUNT;
}

static int operand_slot_of(const jav_sig_t* s, int i) {
    int slot = 0;
    for (int j = s->nkids - 1; j > i; j--) slot += jav_class_width[kid_class(s, j)];
    return slot;
}

/* Slots, not items — everything that reasons about cache capacity counts these. */
static int sig_operand_slots(const jav_sig_t* s) {
    int n = 0;
    for (int i = 0; i < s->nkids; i++) n += jav_class_width[kid_class(s, i)];
    return n;
}
static int sig_result_slots(const jav_sig_t* s) {
    int n = 0;
    for (int i = 0; i < s->nresults; i++) n += jav_class_width[s->results[i]];
    return n;
}

static void operand_loc(const jav_sig_t* s, int i, int k, char* out, size_t cap, uint8_t cls) {
    int slot = operand_slot_of(s, i);
    /* A class the variant family will not put in a register is always in memory,
     * whatever the state says — reading that from jav_class_cacheable rather than
     * deciding it here is what keeps this grammar from naming registers no
     * stencil ever fills. Cached only if the WHOLE value fits inside the state:
     * half a v128 in a register is not a value. */
    if (cls < 6 && jav_class_cacheable[cls] && slot + jav_class_width[cls] <= k)
        snprintf(out, cap, "%s_reg%d", kClassName[cls], slot);
    else
        snprintf(out, cap, "%s_mem", kClassName[cls]);
}

/* What a rule costs, in Ertl's cycles (§2.6, printed 36: loads, stores, moves and
 * sp updates cost one; instruction dispatches cost four).
 *
 * This is a function of the RULE — the arity, how many operands the state puts in
 * registers, where the result goes — and not of the opcode, which is why it needs
 * no averaging over the opcodes sharing a terminal. The earlier model measured
 * stencil `code_size` and averaged, with a note arguing the averaging was honest
 * because every location assignment aggregates the same opcode set. That was
 * true and beside the point: bytes are the wrong axis. Stack caching removes
 * memory traffic, so memory traffic is what pricing it has to count.
 *
 * An instruction's own dispatch is omitted — it happens whichever variant runs,
 * so it is equal across every cover of the same tree. What a transition adds is
 * a dispatch that would not otherwise occur, and that is where the 4 goes. */
static uint32_t rule_cost(const jav_sig_t* s, int state, int cached_res) {
    /* How many OPERANDS the state covers — counted by walking down from the top
     * and spending each one's width, because the state is a slot count and an
     * item may cost two of them. */
    int cached_slots = 0;
    for (int i = s->nkids - 1; i >= 0; i--) {
        uint8_t c = kid_class(s, i);
        if (operand_slot_of(s, i) + jav_class_width[c] > state) break;
        cached_slots += jav_class_width[c];
    }
    /* Counted in SLOTS, because the unit is a WORD MOVED and a v128 is two of
     * them — on both sides at once. It occupies two registers, and its stack
     * access copies sixteen bytes where every other class copies eight. Item
     * count stood in for this only while every class was one word wide; it stops
     * being a proxy the moment one is not. */
    int mem = (sig_operand_slots(s) - cached_slots)             /* operands loaded */
            + (s->nresults && !cached_res ? sig_result_slots(s) : 0);  /* stored */
    /* §2.3: "the stack pointer need not be updated in instruction
     * implementations that can access all stack items in registers." */
    return (uint32_t)((mem + (mem ? 1 : 0)) * JAV_COST_MEM);
}

/* Whether the variant family PROVIDES this (terminal, state, exit state) — the
 * rule-existence condition, and the whole of it: one rule per (signature ×
 * operand/result location assignment) that the stencil variant family provides.
 *
 * The family is per OPCODE and a rule is per SIGNATURE, so "provides" is a claim
 * about every member. Two answers are possible and they are not the same thing:
 *
 *   no variant at this state  — §2.5's omission. That member is not offering
 *                               this form and the tiler reaches its state by
 *                               transition. Not a disagreement.
 *   a variant landing the result SOMEWHERE ELSE — the family does not provide
 *                               this rule. Emitting it anyway hands the tiling a
 *                               placement the stamped stencil will not honour.
 *
 * The second case is a rule the tiling could hand to a member that lands its
 * result elsewhere. The exit state is read back from the family rather than
 * assumed, so the walk then has a gap to bridge that no cover asked for — and
 * where the widths differ it cannot be bridged at all. */
static int mean_size(int t, int state, int want_fs, uint32_t* out, int* nops) {
    uint64_t total = 0; int n = 0, miss = 0;
    for (int op = 0; op < 256; op++) {
        const jav_opcode_sig_t* rows[2] = { &jav_opcode_sig[op], jav_opcode_sig_sub[op] };
        int lens[2] = { 1, jav_opcode_sig_sub[op] ? jav_opcode_sig_sub_len[op] : 0 };
        for (int which = 0; which < 2; which++) {
            for (int s = 0; s < lens[which]; s++) {
                const jav_opcode_sig_t* r = which ? &rows[1][s] : rows[0];
                if (!r || !r->present) continue;
                const jav_sig_t* d = &jav_sigtab[r->sig];
                int hit = 0;
                for (int q = 0; q < d->nresolve; q++) if (d->resolves_to[q] == t) hit = 1;
                if (!hit) continue;
                jav_jit_meta_t m = which ? jav_jit_meta_sub[op][s] : jav_jit_meta[op];
                if (m.stencil < 0) continue;
                /* The variant for this state, or none — in which case this opcode
                 * contributes nothing here and, if no opcode does, the rule does
                 * not exist and the tiler transitions instead (§2.5).
                 * A pw terminal reads the POLY-WIDE family: same rows, same
                 * entry-state axis, every `word` slot priced at two registers —
                 * the arithmetic this rule loop already does with the resolved
                 * v128 classes, so the two sides agree by construction. */
                int pw = jav_sigtab[t].pw;
                int sid = (state <= JAV_TIER2_N)
                              ? (pw ? jav_variant_pw : jav_variant)[m.stencil][state] : -1;
                int fs  = (state <= JAV_TIER2_N)
                              ? (pw ? jav_variant_pw_fs : jav_variant_fs)[m.stencil][state] : -1;
                /* The PLAIN stencil is the memory-result form: it reads its
                 * operands from memory and pushes its result there, which is
                 * exactly (entry 0, nothing cached on exit). Offering it here is
                 * what lets a signature reduce at `X_mem` as well as at
                 * `X_reg0` — and it is the only form a class with no *_reg0 rule
                 * (v128, ref) can ever use, so without it those terminals have no
                 * rule at all and every body containing one declines. */
                if (state == 0 && want_fs == 0 && fs != 0) { sid = m.stencil; fs = 0; }
                /* Above state 0 the memory-result form is its own stencil —
                 * same cached operands, result pushed inline. It exits at `left`,
                 * which is 0 wherever the form exists, so it answers want_fs 0. */
                if (state > 0 && want_fs == 0 && fs != 0
                    && (pw ? jav_variant_pw_m : jav_variant_m)[m.stencil][state] >= 0) {
                    sid = (pw ? jav_variant_pw_m : jav_variant_m)[m.stencil][state]; fs = 0;
                }
                /* No variant at this state is not a disagreement: that opcode is
                 * not offering this form, and the omission rule already says the tiler reaches
                 * such a state by transition. What cannot stand is two opcodes
                 * that BOTH answer and answer differently.
                 *
                 * An earlier build PRICED the transition here — the worst member's
                 * descend deficit added to the rule — and the corpus refuted it:
                 * one exotic non-provider (struct.new's variadic form has no
                 * variant at any state) taxed every rule its terminal shares,
                 * the DP fled to memory forms, and the traffic the price exists
                 * to minimize DOUBLED (mem slots 21100 -> 39816, code +18%) to
                 * avoid 24 unpriced slots. A per-signature cost cannot carry
                 * per-member truth; the descend stays a METERED residue
                 * (jav_ttree_stats.descends, baselined), and the recorded next
                 * move if a corpus ever shows descends at scale is per-opcode
                 * terminals for the disagreeing family, not a shared-rule tax. */
                if (sid < 0) continue;
                /* Existing is not enough — it has to land the result WHERE THIS
                 * RULE SAYS. Opcodes sharing a signature need not agree, and the
                 * DP is what chooses between them; counting them together
                 * promised a register the stencil never wrote. */
                if (fs != want_fs) { miss = 1; continue; }
                total += stencil_table[sid].code_size;   /* reported, not priced */
                n++;
            }
        }
    }
    *nops = n;
    *out = (uint32_t)(n ? (total + n / 2) / n : 0);
    return n != 0 && !miss;
}

int main(int argc, char** argv) {
    int n = 1;
    const char* out_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) n = atoi(argv[++i]);
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }
    if (n < 0 || n > MAX_N) {
        fprintf(stderr, "gen_tile_burg: -n %d out of range (0..%d)\n", n, MAX_N);
        return 2;
    }
    FILE* o = out_path ? fopen(out_path, "w") : stdout;
    if (!o) { perror(out_path); return 1; }

    fprintf(o,
        "// AUTO-GENERATED by tools/gen_tile_burg.c — do not edit.\n"
        "// Terminals are storage-class signatures; nonterminals are cache states\n"
        "// (storage class x location). Costs are MEASURED stencil code_size, so a\n"
        "// rule exists only where the stencil it stamps does.\n"
        "//\n"
        "// n = %d cached item(s): Ertl's minimal organization, one state per NUMBER\n"
        "// of cached items (§2.3). In state k the top k items are in reg0..reg%d.\n\n"
        "(. #include \"jav_ttree_burg.h\" .)\n\n"
        // burgc emits its matcher API as plain externals named burg_*, so two matchers
        // in one program collide at link. The compiler's SIR matcher is the other one,
        // and a test that compiles Java and then RUNS it links both. COMPILER is burgc's
        // namespace directive; the tile matcher takes it because its caller is one file.
        "COMPILER jav_tile\n\n", n, n - 1);

    for (int t = 0; t < JAV_SIG_COUNT; t++)
        if (jav_sigtab[t].final) fprintf(o, "TERM %s = %d\n", jav_sigtab[t].name, t);

    fputs("\nSTART stmt\n\nRULES\n\n"
          "// A region root: whatever the region computed, left where the next region\n"
          "// can find it.\n", o);
    for (unsigned c = 0; c < 6; c++) {
        int used = 0;
        for (int t = 0; t < JAV_SIG_COUNT; t++)
            if (jav_sigtab[t].final && jav_sigtab[t].nresults &&
                jav_sigtab[t].results[0] == c) used = 1;
        if (used) fprintf(o, "stmt: %s_mem = 0;\n", kClassName[c]);
    }

    fputs("\n// A value the previous region left behind is already where it belongs.\n", o);
    for (unsigned c = 0; c < 6; c++)
        fprintf(o, "%s_mem: %s = 0;\n", kClassName[c], jav_sigtab[jav_carried_sig[c]].name);

    /* One rule per (terminal, entry state) the variant family provides. Only the
     * memory form exists until the family does, so today every terminal
     * contributes its state-0 rule and nothing else — with a real cost, which is
     * the first time this model can tell two covers apart. */
    /* C3: the transitions, as chain rules. They are how the DP learns that a
     * state choice is not free — a cover that leaves a value in a register where
     * the next instruction wants it in memory pays the spill here. The stitcher
     * never reads which chain rule fired: it derives the same transitions from
     * the state sequence, and these exist so the cost model agrees with it. */
    if (n) {
        fputs("\n// Transitions: spilling a cached value, and filling a memory one.\n", o);
        for (unsigned c = 0; c < 6; c++) {
            if (!jav_class_cacheable[c]) continue;
            for (int s = 0; s < n; s++) {
                int sp = jav_spill[c][s], fi = jav_fill[c][s];
                /* A transition is its memory access PLUS a dispatch, because it
                 * is an extra stencil that would not otherwise run. That is the
                 * whole of what a byte count could not see, and it is what makes
                 * a round trip through memory lose to going there directly. */
                /* …and the transition costs its words plus its dispatch, so a
                 * v128 crossing is 2 + 4 rather than 1 + 4: one extra stencil,
                 * twice the data. */
                unsigned tcost = (unsigned)(jav_class_width[c] * JAV_COST_MEM
                                            + JAV_COST_DISPATCH);
                if (sp >= 0)
                    fprintf(o, "%s_mem: %s_reg%d = %u;\n", kClassName[c], kClassName[c], s, tcost);
                if (fi >= 0)
                    fprintf(o, "%s_reg%d: %s_mem = %u;\n", kClassName[c], s, kClassName[c], tcost);
                /* …and the SURVIVOR SHIFT, which is neither a spill nor a fill: a
                 * value reaches a deeper slot by staying in the cache while
                 * something is pushed above it. The consuming variant already does
                 * it — `CACHE_R1 = CACHE_R0` is emitted as part of the stencil that
                 * was going to run anyway — so it is free, and 0 is the cost of a
                 * thing that happens no matter what.
                 *
                 * Without this edge the only way into a deeper slot is a fill from
                 * memory, so the DP's sole route to state 2 is to spill a value and
                 * refill it one slot down. It declines that round trip, correctly,
                 * and every state above the first is then unreachable — the
                 * variants generated for them dead, and the meters identical at
                 * n=1 and n=2 because nothing ever entered the state.
                 *
                 * The shift moves a value DOWN one slot, so its far end moves too:
                 * the target has to hold the WHOLE value, which for a v128 is the
                 * pair (s+1, s+2). A shift whose far end is past the last slot
                 * names a place the machine does not have. */
                if (s + 1 + jav_class_width[c] <= n)
                    fprintf(o, "%s_reg%d: %s_reg%d = 0;\n", kClassName[c], s + 1,
                            kClassName[c], s);
            }
        }
    }

    fputs("\n// One rule per (signature, cache state), costed at what it stamps.\n", o);
    long rules = 0, uncosted = 0;
    for (int t = 0; t < JAV_SIG_COUNT; t++) {
        const jav_sig_t* s = &jav_sigtab[t];
        if (!s->final || s->name[0] != 'S') continue;   /* carried leaves handled above */
        int any = 0;
        /* Two states can describe the SAME rule: an instruction with no operands
         * and no result reads nothing from the cache and writes nothing to it, so
         * its state-0 and state-1 forms are the same pattern with the same left
         * side. burg dedups on (nonterm, pattern, guard) and rejects the repeat as
         * a duplicate — correctly, since the matcher could not choose between
         * them. The cheapest state that produces a given rule is the one emitted. */
        char seen[(MAX_N + 2) * (MAX_N + 2)][256]; int nseen = 0;
        for (int state = 0; state <= n; state++)
        for (int fs = 0; fs <= n; fs++) {
            uint32_t cost; int nops;
            /* Which placement `fs` describes for THIS signature. What survived
             * this instruction is fixed by the arity, so the exit state says the
             * one remaining thing: whether the result joined it in the cache. An
             * fs that is neither is not a placement, and no variant reports it. */
            int a_slots = sig_operand_slots(s), r_slots = sig_result_slots(s);
            int left = state > a_slots ? state - a_slots : 0;
            int cached_res;
            if (s->nresults && fs == left + r_slots) cached_res = 1;
            else if (fs == left)                     cached_res = 0;
            else continue;
            /* A class that never enters a slot has no reg0 form to reduce at. A
             * `word` result's variant caches unconditionally — it cannot know the
             * class, that is the tile's answer — so this is where the answer is
             * applied, and it is the whole of what keeps a managed reference off
             * the register file. Without it the grammar offers `ref_reg0` and the
             * collector loses a root it cannot see or relocate. */
            if (cached_res && !jav_class_cacheable[s->results[0]]) continue;
            /* The non-cacheable fence: a class that never enters a slot (ref —
             * the collector must see and relocate it in memory) pins the state
             * below itself; a rule whose window touches one names a register no
             * stencil fills. A pw terminal's v128 slots are NOT fenced: the
             * poly-wide stencil family (`__sK_pw`) moves them two registers
             * wide, the same widths this loop's resolved-class arithmetic
             * prices, so the pairing mean_size makes below is exact. An `aw`
             * terminal's v128 IS fenced — that wide value crossed an `any`
             * slot, whose carrier no stencil flavor widens (any_t moves its
             * second half in `hi`, one slot at every width), so a cached form
             * would read half a value and a cached result would truncate one. */
            {
                int fenced = 0;
                for (int i = 0; i < s->nkids && !fenced; i++) {
                    uint8_t c = kid_class(s, i);
                    if (operand_slot_of(s, i) < state
                        && (c >= 6 || !jav_class_cacheable[c]
                            || (s->aw && c == JSC_V128))) fenced = 1;
                }
                if (cached_res && s->aw && s->results[0] == JSC_V128) fenced = 1;
                if (fenced) continue;
            }
            if (!mean_size(t, state, fs, &cost, &nops)) continue;
            cost = rule_cost(s, state, cached_res);   /* cycles, not bytes */
            /* In state k the top k operands are in registers; a cached result
             * becomes the new top, so it lands in slot 0. The locations here and
             * the slot the stencil reads come from the same rule: distance from
             * the top. */
            char lhs[32];
            if (!s->nresults)      snprintf(lhs, sizeof lhs, "stmt");
            else if (cached_res)   snprintf(lhs, sizeof lhs, "%s_reg0", kClassName[s->results[0]]);
            else                   snprintf(lhs, sizeof lhs, "%s_mem",  kClassName[s->results[0]]);
            char rule[256];
            int at = snprintf(rule, sizeof rule, "%s: %s", lhs, s->name);
            if (s->nkids) {
                at += snprintf(rule + at, sizeof rule - at, "(");
                int kid = 0;
                for (int i = 0; i < s->nparams; i++) {
                    if (s->params[i] == JSC_STK) continue;
                    char loc[32];
                    operand_loc(s, kid, state, loc, sizeof loc, s->params[i]);
                    at += snprintf(rule + at, sizeof rule - at, "%s%s", kid ? ", " : "", loc);
                    kid++;
                }
                at += snprintf(rule + at, sizeof rule - at, ")");
            }
            /* An operand held in a register while a LATER operand's subtree runs
             * is only realizable if that subtree fits in the slots left over. The
             * rule cannot say so — it states where operands sit, not how much room
             * evaluating one takes — so the builder supplies the peak per node and
             * a guard reads it.
             *
             * Only where something is actually WAITING. With nothing held there is
             * nothing to displace, so the all-memory forms stay unguarded and every
             * terminal keeps a rule the coverage analysis can see. */
            char guard[192]; int gat = 0, unrealizable = 0;
            if (s->nkids) {
                int occ = state > sig_operand_slots(s) ? state - sig_operand_slots(s) : 0;
                int kid = 0;
                for (int i = 0; i < s->nparams; i++) {
                    if (s->params[i] == JSC_STK) continue;
                    if (kid > 0 && occ > 0) {
                        if (occ >= n) { unrealizable = 1; break; }
                        gat += snprintf(guard + gat, sizeof guard - gat,
                                        "%sJAV_TNEED(node,%d) <= %d",
                                        gat ? " && " : "", kid, n - occ);
                    }
                    uint8_t c = s->params[i];
                    if (c < 6 && jav_class_cacheable[c]
                        && operand_slot_of(s, kid) + jav_class_width[c] <= state)
                        occ += jav_class_width[c];
                    kid++;
                }
            }
            if (unrealizable) continue;
            if (gat) at += snprintf(rule + at, sizeof rule - at, " where (. %s .)", guard);
            int dup = 0;
            for (int k = 0; k < nseen; k++) if (strcmp(seen[k], rule) == 0) dup = 1;
            if (dup) continue;
            snprintf(seen[nseen++], sizeof seen[0], "%s", rule);
            fputs(rule, o);
            /* The cache is top-first: slot d holds the operand d from the top, so
             * in state k the top k operands occupy reg0..reg[k-1]. Pack the class
             * of each, three bits per slot, exactly as operand_loc placed them —
             * an operand whose class never enters a register is in memory
             * whatever the state says, and reads JSC_COUNT here.
             *
             * On exit only slot 0 is nameable: it holds this instruction's result
             * if the result was cached. Whatever survived underneath came from
             * some earlier instruction and this rule has never seen it, so the
             * stitcher carries those classes along the walk. */
            uint32_t in_pack = 0, out_pack = 0;
            for (int d = 0; d < 32 / JAV_TILE_CLS_BITS; d++) {
                in_pack  |= (uint32_t)JSC_COUNT << (d * JAV_TILE_CLS_BITS);
                out_pack |= (uint32_t)JSC_COUNT << (d * JAV_TILE_CLS_BITS);
            }
            {
                for (int kid = 0; kid < s->nkids; kid++) {
                    uint8_t c = kid_class(s, kid);
                    int slot = operand_slot_of(s, kid);
                    int w = jav_class_width[c];
                    if (slot + w > state || c >= 6 || !jav_class_cacheable[c]) continue;
                    /* EVERY slot the value occupies names its class, not just the
                     * first: a v128 spans two, and a transition asks about the
                     * slot it is moving, so a slot left unnamed reads as "no
                     * class" and the gap cannot be bridged. */
                    for (int q = 0; q < w; q++) {
                        int sh = (slot + q) * JAV_TILE_CLS_BITS;
                        in_pack = (in_pack & ~(JAV_TILE_CLS_MASK << sh))
                                | ((uint32_t)c << sh);
                    }
                }
                if (cached_res)
                    for (int q = 0; q < jav_class_width[s->results[0]]; q++) {
                        int sh = q * JAV_TILE_CLS_BITS;   /* a v128 result fills two */
                        out_pack = (out_pack & ~(JAV_TILE_CLS_MASK << sh))
                                 | ((uint32_t)s->results[0] << sh);
                    }
            }
            // The action IS the emitter: a rule is a (signature, cache state) pair,
            // and firing it stamps this node's stencil variant for that state —
            // burg matches the tree and emits from the reduce. The driver
            // owns the emission context the reduce runs inside and bridges the
            // machine's carried state to this rule's before stamping.
            fprintf(o, " = %u (. jav_t2_stamp(node, %d, 0x%xu, 0x%xu); .);\n",
                    cost, state, in_pack, out_pack);
            rules++; any = 1;
        }
        if (!any) uncosted++;
    }

    if (out_path) fclose(o);
    fprintf(stderr, "gen_tile_burg: n=%d, %ld rules, %ld terminal(s) with no stencil\n",
            n, rules, uncosted);
    return 0;
}
