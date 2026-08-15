// test_jit_holes.c — every _HOLE_ a stencil references must be patchable.
//
// This is a soundness gate, not a completeness nicety. jit_driver's
// fill_native_holes does `if (a) vals[i] = addr;` — a hole it cannot resolve is
// left at 0 and patched as ZERO, silently. The interpreter is unaffected, so the
// engine miscompiles in the JIT tier only: a sign mask that stops flipping the
// sign, a range bound that becomes 0.0. Nothing fails loudly, and a differential
// test catches it only if it happens to exercise that exact opcode.
//
// The class is real and recurred three times, all one root cause — opgen collected
// holes by scanning an opcode's BODY, so anything named only by an `error:` guard
// (a native, a float constant, the float sign mask) was referenced and never
// declared. Instances get fixed; this gate is what stops the next one.
//
// A hole is patchable if it is (a) machine plumbing the driver backpatches itself,
// (b) a native/libm address in the opgen symbol table, or (c) an operand or const
// hole listed in that opcode's jit meta.
#include "jav_jit_meta.h"
#include "jav_jit_symbols.h"
#include "jav_stencil_table.h"
#include <stdio.h>
#include <string.h>

// Holes the driver resolves structurally rather than by name lookup: the
// continuation edges it backpatches once the buffer base is known, the source
// offset it bakes, and the br_table/try_table tail cursors.
static const char *MACHINE_HOLES[] = {
    "_HOLE_pc", "_HOLE_cont", "_HOLE_trap", "_HOLE_resync", "_HOLE_ip",
    "_HOLE_offmap", "_HOLE_codelen", NULL
};

static int is_machine(const char *h) {
    for (int i = 0; MACHINE_HOLES[i]; i++) if (!strcmp(h, MACHINE_HOLES[i])) return 1;
    return 0;
}
static int is_symbol(const char *h) {
    for (int i = 0; i < jav_jit_symbols_count; i++)
        if (!strcmp(h, jav_jit_symbols[i].name)) return 1;
    return 0;
}

// Walk every meta table (top level + the 0xFB/0xFC/0xFD sub-tables) and mark, per
// stencil index, the operand/const hole names that opcode supplies.
#define MAX_PER_STENCIL 16
static const char *g_supplied[STENCIL_COUNT][MAX_PER_STENCIL];
static int g_nsupplied[STENCIL_COUNT];

static int g_overflow;   // a silent cap would weaken this gate into a false alarm

// Every hole one meta supplies, recorded against one stencil id.
static void note_one(const jav_jit_meta_t *m, int s) {
    for (int k = 0; k < m->operand_count; k++) {
        if (g_nsupplied[s] >= MAX_PER_STENCIL) {   // never truncate quietly
            printf("  stencil %d supplies more than %d holes — raise MAX_PER_STENCIL\n",
                   s, MAX_PER_STENCIL);
            g_overflow = 1; return;
        }
        g_supplied[s][g_nsupplied[s]++] = m->operands[k].hole;
        // A memarg carries a second, IMPLICIT value: with multi-memory the align
        // flag's 0x40 bit says a memidx follows. jit_driver patches it by literal
        // name (`_HOLE_memidx`) off the JOP_MEMARG operand rather than from a meta
        // entry, so mirror that rule here or every memory stencil reads as broken.
        if (m->operands[k].kind == JOP_MEMARG) {
            if (g_nsupplied[s] >= MAX_PER_STENCIL) { g_overflow = 1; return; }
            g_supplied[s][g_nsupplied[s]++] = "_HOLE_memidx";
        }
    }
}

// An opcode's meta supplies the holes for EVERY form of that opcode, because the
// driver picks the form from the cache state and then patches it from this same
// operand list — find_hole(&stencil_table[chosen], m.operands[k].hole). Walking
// only the base id leaves every cache variant looking like a stencil nothing
// patches: a few hundred false alarms at n=1, and no statement at all about the
// forms the tier actually stamps.
static void note_meta(const jav_jit_meta_t *m, int n) {
    for (int i = 0; i < n; i++) {
        int base = m[i].stencil;
        if (base < 0 || base >= STENCIL_COUNT || !m[i].operands) continue;
        note_one(&m[i], base);
        for (int st = 0; st <= JAV_TIER2_N; st++) {
            int v = jav_variant[base][st];
            if (v >= 0 && v != base) note_one(&m[i], v);
            /* …and D7s' memory-result form, which is a stencil of its own and is
             * chosen by the same driver from the same operand list
             * (jit_driver.c: jav_variant_m[m.stencil][entry]). Walking only
             * jav_variant left every `__sKm` looking unpatched. It read as clean
             * while no SIMD opcode had a cached form to have a memory-result
             * counterpart of — a green that was counting the wrong forms. */
            int vm = jav_variant_m[base][st];
            if (vm >= 0 && vm != base) note_one(&m[i], vm);
            /* …and the POLY-WIDE family, selected by the same driver from the
             * same operand list when the tile resolved a pw signature
             * (jit_driver.c: rs->pw). Same lesson as `__sKm` above: a family
             * the walk does not know reads as 71 stencils nothing patches. */
            int vp = jav_variant_pw[base][st];
            if (vp >= 0 && vp != base) note_one(&m[i], vp);
            int vpm = jav_variant_pw_m[base][st];
            if (vpm >= 0 && vpm != base) note_one(&m[i], vpm);
        }
    }
}
static int is_supplied(int s, const char *h) {
    for (int i = 0; i < g_nsupplied[s]; i++) if (!strcmp(h, g_supplied[s][i])) return 1;
    return 0;
}

int main(void) {
    note_meta(jav_jit_meta, 256);
    note_meta(jav_jit_meta_sub_fb, 31);
    note_meta(jav_jit_meta_sub_fc, 18);
    note_meta(jav_jit_meta_sub_fd, 276);

    int bad = 0, checked = 0;
    for (int s = 0; s < STENCIL_COUNT; s++) {
        const StencilDef *d = &stencil_table[s];
        for (int i = 0; i < d->hole_count; i++) {
            const char *h = d->hole_names[i];
            if (!h) continue;
            checked++;
            if (is_machine(h) || is_symbol(h) || is_supplied(s, h)) continue;
            printf("  stencil %d references %s — nothing patches it (would be baked as 0)\n", s, h);
            bad++;
        }
    }
    printf("\n%d stencil holes checked, %d unpatchable\n", checked, bad);
    printf("JIT hole resolution: %s\n", (bad || g_overflow) ? "FAIL" : "PASS");
    return (bad || g_overflow) ? 1 : 0;
}
