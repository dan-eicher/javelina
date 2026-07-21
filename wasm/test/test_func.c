// test_func.c — code bodies as instruction trees. The code section keeps each
// body as a raw byte slice (the in-place interpreter's input); this test parses
// that SAME slice into the shared Func tree (locals + Expr) via jav_func_body_read —
// the representation the text reader will produce — and asserts decode→encode
// round-trips. Run on the checked-in add.wasm: body = 00 20 00 20 01 6A 0B
// (no locals; local.get 0; local.get 1; i32.add; end).

#include "jav_reader.h"
#include "jav_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void check(const char *name, int ok) {
    printf("  %-44s [%s]\n", name, ok ? "PASS" : "FAIL");
    if (!ok) fails++;
}

static const jav_section_t *find_section(const jav_module_t *m, int tag) {
    for (size_t i = 0; i < m->sections.count; i++)
        if (m->sections.items[i].body.tag == tag) return &m->sections.items[i];
    return NULL;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "add.wasm";
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { perror("fread"); return 2; }
    fclose(f);

    bbq_ctx_t cx; bbq_ctx_init(&cx, buf, (size_t)n);
    jav_module_t mod;
    if (!jav_module_read(&cx, &mod)) { fprintf(stderr, "module parse failed\n"); bbq_ctx_free(&cx); return 2; }
    bbq_ctx_free(&cx);

    const jav_section_t *cs = find_section(&mod, 10);
    check("code section present", cs != NULL);
    if (!cs) { jav_module_free(&mod); free(buf); return 1; }

    // The owning reader already parsed the code body into the shared FuncBody tree
    // (locals + Expr) — the representation the text reader also produces, via
    // jav_func_body_read. Assert that tree, and that it re-encodes (decode -> encode)
    // to the original body bytes.
    static const uint8_t expect[] = {0x00,0x20,0x00,0x20,0x01,0x6A,0x0B};
    const jav_func_body_t *fn = &cs->body.u.case_10.entries.items[0].body;
    check("no local declarations", fn->local_count == 0 && fn->locals.count == 0);
    check("3 instructions in body",
          fn->body.instrs.count == 3 &&
          fn->body.instrs.items[0].op == 0x20 &&   // local.get
          fn->body.instrs.items[1].op == 0x20 &&   // local.get
          fn->body.instrs.items[2].op == 0x6A);    // i32.add

    // Round-trip: the FuncBody tree re-encodes to the body bytes.
    uint8_t out[64]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
    int rt = jav_func_body_write(&w, fn) &&
             w.pos == sizeof expect && memcmp(out, expect, sizeof expect) == 0;
    check("func tree round-trips to the body bytes", rt);
    jav_module_free(&mod);
    free(buf);
    printf("\nP2 code-body tree: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
