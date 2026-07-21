// test_container.c — Phase 1A gate. Parses the checked-in add.wasm through
// bbqc's GENERATED container reader and asserts the module skeleton: one
// function type (i32,i32)->i32, one function using it, and one code body of the
// expected raw bytes. This is the bbqc↔WASM seam; the code body bytes it exposes
// are the opgen decoder's input in the next steps.

#include "jav_reader.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
static void check(const char *name, int ok) {
    printf("  %-36s [%s]\n", name, ok ? "PASS" : "FAIL");
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

    bbq_ctx_t ctx; bbq_ctx_init(&ctx, buf, (size_t)n);
    jav_module_t mod;
    check("module parses", jav_module_read(&ctx, &mod));
    check("consumed whole file", bbq_at_end(&ctx));
    bbq_ctx_free(&ctx);

    // Type section: one (i32,i32) -> i32.
    const jav_section_t *ts = find_section(&mod, 1);
    check("type section present", ts != NULL);
    if (ts) {
        const jav_type_section_t *t = &ts->body.u.case_1;
        check("one type", t->types.count == 1);
        if (t->types.count == 1) {
            // §5.5.4: a type entry is a rectype; here a bare func comptype (0x60).
            const jav_rec_type_t *rt = &t->types.items[0];
            check("type is a func comptype (0x60)", rt->head == 0x60);
            const jav_func_type_t *ft = &rt->body.u.case_5;
            check("2 params", ft->param_count == 2);
            check("params i32,i32", ft->params.count == 2 &&
                  ft->params.items[0].head == 0x7F && ft->params.items[1].head == 0x7F);
            check("1 result i32", ft->result_count == 1 &&
                  ft->results.count == 1 && ft->results.items[0].head == 0x7F);
        }
    }

    // Function section: one function, type index 0.
    const jav_section_t *fs = find_section(&mod, 3);
    check("function section present", fs != NULL);
    if (fs) {
        const jav_function_section_t *fn = &fs->body.u.case_3;
        check("one function -> type 0",
              fn->count == 1 && fn->type_indices.count == 1 && fn->type_indices.items[0] == 0);
    }

    // Code section: one entry, parsed into the shared FuncBody tree —
    //   no locals; local.get 0; local.get 1; i32.add; end.
    const jav_section_t *cs = find_section(&mod, 10);
    check("code section present", cs != NULL);
    if (cs) {
        const jav_code_section_t *c = &cs->body.u.case_10;
        check("one code entry", c->count == 1 && c->entries.count == 1);
        if (c->entries.count == 1) {
            const jav_func_body_t *fb = &c->entries.items[0].body;
            check("code body: no local decls", fb->local_count == 0);
            check("code body: local.get×2; i32.add; end",
                  fb->body.instrs.count == 3 &&
                  fb->body.instrs.items[0].op == 0x20 &&
                  fb->body.instrs.items[1].op == 0x20 &&
                  fb->body.instrs.items[2].op == 0x6A);
        }
    }

    jav_module_free(&mod);
    free(buf);
    printf("\n1A container gate: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
