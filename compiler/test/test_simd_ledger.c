// test_simd_ledger.c — the SIMD generator ledger, wired into `make test`.
//
// The ONE authority for the intrinsic surface is the spec's instructions.toml;
// the generated table (src/gen/simd_intrinsics.h) is a build artifact of it.
// This suite re-reads the toml AT TEST TIME with the same reader pair the
// generator links and cross-checks the LIVE spec against the STAMPED table:
// a new spec revision (rows added/removed/reshaped) goes red here even if the
// stale generated header still compiles. The shape column is read verbatim —
// no re-derivation of the generator's classification, only the spec's own
// shape groups against the table's family groups.
//
// It also PRINTS the ledger (total + per-shape + per-family counts) on every
// run — the §V6 build artifact.
#include "toml/toml_doc.h"
#include "gen/simd_intrinsics.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "javelina_test.h"

static char* slurp(const char* path, int* len) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); *len = (int)n; return b;
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "../wasm/spec/instructions.toml";

    /* ── the LIVE spec: 0xFD rows by shape ─────────────────────────────── */
    bbq_arena ar; bbq_arena_init(&ar, 0);
    int len; char* src = slurp(path, &len);
    CHECK(src != NULL, "instructions.toml is readable");
    if (!src) return TEST_SUMMARY("test_simd_ledger");
    toml_doc_t* doc = toml_parse(src, len, &ar);
    CHECK(!toml_doc_has_errors(doc), "instructions.toml parses");
    const toml_val_t* instrs = toml_tbl_get(toml_doc_root(doc), "instr");
    int n = toml_val_array_count(instrs);
    /* The generated surface = every 0xFD row + every memarg/memlane row (the
     * scalar loads/stores) + the four memory admin ops — the generator's own
     * selection rule, re-derived here from the LIVE toml. */
    int toml_total = 0, sh_none = 0, sh_lane = 0, sh_v128 = 0,
        sh_memarg_v = 0, sh_memlane = 0, sh_memarg_s = 0, sh_admin = 0;
    for (int i = 0; i < n; i++) {
        const toml_tbl_t* t = toml_val_as_table(toml_val_array_at(instrs, i));
        if (!t) continue;
        const toml_val_t* ov = toml_tbl_get(t, "opcode");
        int64_t pfx, sub;
        int is_fd = toml_val_type(ov) == TOML_VT_ARRAY && toml_val_array_count(ov) == 2 &&
                    toml_val_as_int(toml_val_array_at(ov, 0), &pfx) &&
                    toml_val_as_int(toml_val_array_at(ov, 1), &sub) && pfx == 0xFD;
        const char* shape = "none";
        toml_val_as_string(toml_tbl_get(t, "shape"), &shape);
        const char* nm = NULL;
        toml_val_as_string(toml_tbl_get(t, "name"), &nm);
        int is_mem_shape = !strcmp(shape, "memarg") || !strcmp(shape, "memlane");
        int is_admin = nm && (!strcmp(nm, "memory.size") || !strcmp(nm, "memory.grow") ||
                              !strcmp(nm, "memory.fill") || !strcmp(nm, "memory.copy"));
        if (!is_fd && !is_mem_shape && !is_admin) continue;
        toml_total++;
        if      (is_admin)                  sh_admin++;
        else if (!strcmp(shape, "memlane")) sh_memlane++;
        else if (!strcmp(shape, "memarg"))  { if (is_fd) sh_memarg_v++; else sh_memarg_s++; }
        else if (!strcmp(shape, "lane"))    sh_lane++;
        else if (!strcmp(shape, "v128"))    sh_v128++;
        else                                sh_none++;
    }

    /* ── the STAMPED table: rows by family group ───────────────────────── */
    int fam_counts[36] = {0};
    for (int i = 0; i < SIMD_INTRINSIC_COUNT; i++) {
        int f = simd_intrinsics[i].family;
        CHECK(f >= 1 && f <= 35, "table family in range 1..35");
        if (f >= 1 && f <= 35) fam_counts[f]++;
    }
    int g_value = 0, g_lane = 0, g_bytes = 0, g_memarg_v = 0, g_memlane = 0,
        g_memarg_s = 0, g_admin = 0;
    for (int f = 1;  f <= 9;  f++) g_value    += fam_counts[f];
    for (int f = 10; f <= 17; f++) g_lane     += fam_counts[f];
    for (int f = 18; f <= 19; f++) g_bytes    += fam_counts[f];
    for (int f = 20; f <= 21; f++) g_memarg_v += fam_counts[f];
    for (int f = 22; f <= 23; f++) g_memlane  += fam_counts[f];
    for (int f = 24; f <= 31; f++) g_memarg_s += fam_counts[f];
    for (int f = 32; f <= 35; f++) g_admin    += fam_counts[f];

    /* ── the ledger (printed every `make test` run) ────────────────────── */
    static const char* fam_names[] = { "", "BIN", "UN", "SHIFT", "TERN", "TESTI",
        "SPLATI", "SPLATL", "SPLATF", "SPLATD", "EXTRACTI", "EXTRACTL", "EXTRACTF",
        "EXTRACTD", "REPLACEI", "REPLACEL", "REPLACEF", "REPLACED", "CONST", "SHUFFLE",
        "MEMLOAD", "MEMSTORE", "MEMLOADLANE", "MEMSTORELANE",
        "MEMLOADI", "MEMLOADL", "MEMLOADF", "MEMLOADD",
        "MEMSTOREI", "MEMSTOREL", "MEMSTOREF", "MEMSTORED",
        "MEMSIZE", "MEMGROW", "MEMFILL", "MEMCOPY" };
    printf("wasm-intrinsic ledger: toml %d ops (none %d, lane %d, v128 %d, "
           "v128-memarg %d, memlane %d, scalar-memarg %d, admin %d); table %d rows\n",
           toml_total, sh_none, sh_lane, sh_v128, sh_memarg_v, sh_memlane,
           sh_memarg_s, sh_admin, SIMD_INTRINSIC_COUNT);
    for (int f = 1; f <= 35; f++)
        printf("  %-12s %d\n", fam_names[f], fam_counts[f]);

    /* ── drift gates: the LIVE spec vs the STAMPED table ───────────────── */
    CHECK(toml_total == SIMD_INTRINSIC_COUNT,
          "toml surface row count == stamped table count (spec drift is a red build)");
    CHECK(sh_none     == g_value,    "shape none         == families 1-9 (value ops)");
    CHECK(sh_lane     == g_lane,     "shape lane         == families 10-17 (extract/replace)");
    CHECK(sh_v128     == g_bytes,    "shape v128         == families 18-19 (const/shuffle)");
    CHECK(sh_memarg_v == g_memarg_v, "v128 memarg        == families 20-21");
    CHECK(sh_memlane  == g_memlane,  "shape memlane      == families 22-23");
    CHECK(sh_memarg_s == g_memarg_s, "scalar memarg      == families 24-31");
    CHECK(sh_admin    == g_admin,    "memory admin ops   == families 32-35");

    free(src);
    bbq_arena_free(&ar);
    return TEST_SUMMARY("test_simd_ledger");
}
