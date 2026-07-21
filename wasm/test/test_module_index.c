// test_module_index.c — Phase 1: jav_module_index flattens EVERY index space off the
// c-lite span index. rich.wasm carries imports (func+global), a memory, a table, a
// defined mutable global, two funcs, a data + an elem segment — so each space is
// non-empty and the imports-low ordering is exercised.
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_subtype.h"   // HT_FUNC and the abstract heaptype codes
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL: %s\n", #cond); fails++; } } while (0)

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "rich.wasm";
    FILE* fp = fopen(path, "rb");
    if (!fp) { perror(path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { perror("fread"); return 2; }
    fclose(fp);

    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &a);
    CHECK(m.success);
    jav_modidx_t mod;
    CHECK(jav_module_index(m.root, buf, &a, &mod));

    // ── function space: 1 imported + 2 defined, import in the low slot ──
    CHECK(mod.nimport_funcs == 1);
    CHECK(mod.nfuncs == 3);
    CHECK(mod.func_sigs[0].nparams == 1 && mod.func_sigs[0].nresults == 1);    // import (i32)->(i32)
    CHECK(mod.func_sigs[1].nparams == 2 && mod.func_sigs[1].nresults == 1);    // add  (i32,i32)->(i32)
    CHECK(mod.func_sigs[1].params[0] == WVT_I32 && mod.func_sigs[1].results[0] == WVT_I32);
    CHECK(mod.func_sigs[2].nparams == 0 && mod.func_sigs[2].nresults == 1);    // ()->(f64)
    CHECK(mod.func_sigs[2].results[0] == WVT_F64);

    // ── global space: 1 imported immutable i32 (low) + 1 defined mutable i64 ──
    CHECK(mod.nimport_globals == 1 && mod.nglobals == 2);
    CHECK(mod.global_types[0] == WVT_I32 && mod.global_is_import[0] == 1 && mod.global_mut[0] == 0);
    CHECK(mod.global_types[1] == WVT_I64 && mod.global_is_import[1] == 0 && mod.global_mut[1] == 1);

    // ── table space: one funcref table, min 2, no max ──
    CHECK(mod.ntables == 1 && mod.nimport_tables == 0);
    CHECK(mod.table_reftype[0] == WVT_REF && mod.table_tidx[0] == (uint32_t)HT_FUNC &&
          mod.table_min[0] == 2 && mod.table_has_max[0] == 0);

    // ── memory space: limits 1..4, 32-bit ──
    CHECK(mod.nmems == 1);
    CHECK(mod.mem_min[0] == 1 && mod.mem_has_max[0] == 1 && mod.mem_max[0] == 4 && mod.mem_is64[0] == 0);

    // ── tags / segments ──
    CHECK(mod.ntags == 0);
    CHECK(mod.nelems == 1 && mod.ndatas == 1);

    // ── the §3.3 lattice is wired to the flattened type space ──
    CHECK(mod.lattice.ntypes == mod.ntypes && mod.lattice.kinds == mod.kinds);

    bbq_arena_free(&a); free(buf);
    printf("module_index: every index space flattened off the c-lite index  [%s]\n",
           fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
