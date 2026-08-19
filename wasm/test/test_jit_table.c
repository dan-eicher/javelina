// test_jit_table.c — the stencil table's STRUCTURAL invariants.
//
// jav_stencil_table.h is committed generated output, but its bytes are a function
// of the C compiler, not only of jav_stencils.c. A toolchain bump rewrites nearly
// every line of it while jav_stencils.c regenerates byte-identical — clang 21.1.8
// moved ~19,900 lines that way — so byte-equality against the previous table is
// not a check anyone can run, and "the diff is large" is not evidence either way.
//
// What CAN be checked is that the table is internally consistent, and that is what
// this gate does. Each invariant below is one way a regeneration could go wrong
// without the differential tiers noticing, because a malformed row does not fail
// loudly: jit_driver reads these fields and stamps machine code from them.
//
//   1. Every patch site lies inside the code it patches. A PatchEntry.offset past
//      code_size makes emit_stencil write over whatever the next stencil emitted
//      into the buffer — silent corruption of an unrelated opcode.
//   2. Every patch names a hole that exists. hole_index is an index into the
//      values array the driver builds from hole_count entries; out of range reads
//      off the end of it and bakes garbage as an address.
//   3. Every role index names the hole it claims to. h_cont/h_ip/h_pc/h_trap/
//      h_resync/h_memidx/h_offmap/h_codelen are resolved BY NAME when the table is
//      generated and used POSITIONALLY when it is consumed, so they are the one
//      place a compiler reordering can silently desynchronise the two. It is not
//      hypothetical: clang 21 reordered st_v128_load64_zero's holes from
//      {pc,memidx,offset,trap,cont} to {…,cont,trap}, and h_cont/h_trap had to
//      follow (4→3 and 3→4). They did. Nothing was checking that they had to.
//   4. Hole names are unique within a stencil. The driver resolves operand holes
//      by name (find_hole), so a duplicate makes the answer depend on scan order.
//   5. data_hole_count never exceeds hole_count, and the constant-pool patches are
//      exactly the holes it counts.
//
// Cheap, deterministic, and toolchain-independent: it reads only the committed
// table. That is the point — it makes a regeneration diff self-certifying instead
// of trusted.
#include "jav_stencil_table.h"
#include <stdio.h>
#include <string.h>

static int g_checked, g_bad;

/* A violation here is systematic, not isolated — one wrong role field is wrong in every
 * stencil that has that role, which is thousands of them. Print the first few and count
 * the rest: a gate that buries its own summary under 6000 identical lines is unreadable
 * exactly when it matters. The COUNT is what goes in the exit code either way. */
#define BAD_SHOWN 20
static void bad(int s, const char *what) {
    if (g_bad < BAD_SHOWN) printf("  stencil %d: %s\n", s, what);
    g_bad++;
}
static void bad_role(int s, const char *role, int idx, const char *found) {
    if (g_bad < BAD_SHOWN)
        printf("  stencil %d: %s points at hole %d, whose name is %s\n", s, role, idx, found);
    g_bad++;
}
static void bad_orphan(int s, int idx, const char *name) {
    if (g_bad < BAD_SHOWN)
        printf("  stencil %d: hole %d is %s but the role field does not name it\n", s, idx, name);
    g_bad++;
}

// Every patch is a 32-bit field except PATCH_ABS64, which is 64.
static uint32_t patch_width(PatchType t) { return t == PATCH_ABS64 ? 8u : 4u; }

// The role fields, paired with the hole name each one is required to name. The
// generator resolves these by name; the driver uses them as indices. This table is
// the only place the two halves are compared.
typedef struct { const char *name; size_t off; } role_t;
#define ROLE(field, hole) { hole, offsetof(StencilDef, field) }
static const role_t ROLES[] = {
    ROLE(h_cont,    "_HOLE_cont"),
    ROLE(h_ip,      "_HOLE_ip"),
    ROLE(h_pc,      "_HOLE_pc"),
    ROLE(h_trap,    "_HOLE_trap"),
    ROLE(h_resync,  "_HOLE_resync"),
    ROLE(h_memidx,  "_HOLE_memidx"),
    ROLE(h_offmap,  "_HOLE_offmap"),
    ROLE(h_codelen, "_HOLE_codelen"),
};

static int16_t role_of(const StencilDef *d, const role_t *r) {
    int16_t v;
    memcpy(&v, (const char *)d + r->off, sizeof v);
    return v;
}

int main(void) {
    for (int s = 0; s < STENCIL_COUNT; s++) {
        const StencilDef *d = &stencil_table[s];

        // A stencil with code must have code; one without is a table hole, which
        // would be stamped as a zero-length body rather than reported.
        g_checked++;
        if (!d->code || d->code_size == 0) { bad(s, "empty code body"); continue; }

        // 1 + 2. Patch sites inside the code, hole indices inside the hole array.
        for (uint32_t i = 0; i < d->patch_count; i++) {
            const PatchEntry *p = &d->patches[i];
            uint32_t w = patch_width(p->type);
            g_checked++;
            if ((uint64_t)p->offset + w > (uint64_t)d->code_size)
                bad(s, "patch site runs past the end of the stencil's code");
            g_checked++;
            if (p->hole_index >= d->hole_count)
                bad(s, "patch names a hole index the stencil does not have");
        }

        // 3. Every role index is -1 (absent) or names its own hole.
        for (size_t r = 0; r < sizeof ROLES / sizeof ROLES[0]; r++) {
            int16_t idx = role_of(d, &ROLES[r]);
            g_checked++;
            if (idx < 0) continue;                    // absent is legitimate
            if (idx >= (int16_t)d->hole_count) { bad(s, "role index past hole_count"); continue; }
            const char *nm = d->hole_names ? d->hole_names[idx] : NULL;
            if (!nm || strcmp(nm, ROLES[r].name))
                bad_role(s, ROLES[r].name, (int)idx, nm ? nm : "(null)");
        }

        // ...and the converse: a hole that IS one of the machine roles must be the
        // one the role field names. Checking only one direction would let a role
        // field sit at -1 while the hole it should have named is present, which
        // reads to the driver as "this stencil has no continuation".
        for (uint16_t i = 0; i < d->hole_count; i++) {
            const char *nm = d->hole_names ? d->hole_names[i] : NULL;
            if (!nm) continue;
            for (size_t r = 0; r < sizeof ROLES / sizeof ROLES[0]; r++) {
                if (strcmp(nm, ROLES[r].name)) continue;
                g_checked++;
                if (role_of(d, &ROLES[r]) != (int16_t)i) bad_orphan(s, (int)i, nm);
            }
        }

        // 4. Hole names unique within the stencil — find_hole resolves by name.
        for (uint16_t i = 0; i < d->hole_count; i++) {
            const char *a = d->hole_names ? d->hole_names[i] : NULL;
            if (!a) continue;
            for (uint16_t j = (uint16_t)(i + 1); j < d->hole_count; j++) {
                const char *b = d->hole_names[j];
                g_checked++;
                if (b && !strcmp(a, b)) bad(s, "duplicate hole name — resolution by name is ambiguous");
            }
        }

        // 5. The constant-pool count is a subset of the holes, and matches the
        //    number of distinct holes reached by a PATCH_REL_DATA site.
        g_checked++;
        if (d->data_hole_count > d->hole_count) bad(s, "data_hole_count exceeds hole_count");
        {
            unsigned char seen[64];
            memset(seen, 0, sizeof seen);
            uint16_t ndata = 0;
            for (uint32_t i = 0; i < d->patch_count; i++) {
                const PatchEntry *p = &d->patches[i];
                if (p->type != PATCH_REL_DATA) continue;
                if (p->hole_index >= sizeof seen) continue;      // covered by check 2
                if (!seen[p->hole_index]) { seen[p->hole_index] = 1; ndata++; }
            }
            g_checked++;
            if (ndata != d->data_hole_count)
                bad(s, "data_hole_count disagrees with the distinct PATCH_REL_DATA holes");
        }
    }

    printf("\n%d stencils, %d structural checks, %d violations\n", STENCIL_COUNT, g_checked, g_bad);
    printf("JIT stencil table structure: %s\n", g_bad ? "FAIL" : "PASS");
    return g_bad ? 1 : 0;
}
