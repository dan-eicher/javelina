/* wat_driver.c — the two-pass .wat assemble driver (see wat_driver.h). */

#include "wat_driver.h"
#include <stdlib.h>
#include <string.h>

/* Free every per-parse allocation hanging off the ctx — the id-name maps, the
 * pass-1 typeuse-dedup copies, the field-name spaces, and the emit/scratch/
 * assembly pools. On a SUCCESSFUL parse the Wat root already consumed the
 * assembly + inline vecs (they froze into the module), so those frees are
 * no-ops; on a mid-parse FAILURE they hold the abandoned remnants. */
static void wat_ctx_teardown(wat_ctx_t *c) {
    bbq_arena_free(&c->arena);
    wat_wbufs_free(c);                      /* instruction emit-scratch pool */
    wat_scratch_free(c);                    /* ctx-rooted production scratch vecs */
    wat_assembly_free(c);                   /* abandoned module-assembly state (deep) */
    for (int i = 0; i < SP_N; i++) {        /* id-name maps (defs + imports) */
        for (int j = 0; j < (int)bbq_vec_len(c->sp[i]); j++) free(c->sp[i][j]);
        bbq_vec_free(c->sp[i]);
        for (int j = 0; j < (int)bbq_vec_len(c->sp_imp[i]); j++) free(c->sp_imp[i][j]);
        bbq_vec_free(c->sp_imp[i]);
    }
    for (int i = 0; i < (int)bbq_vec_len(c->xtypes); i++) {   /* pass-1 typeuse copies */
        free(c->xtypes[i].params.items);
        free(c->xtypes[i].results.items);
    }
    bbq_vec_free(c->xtypes);
    bbq_vec_free(c->xtype_rec);
    wat_type_fields_free(c);                /* §6.6.2 per-type struct field-name space */
    for (int i = 0; i < (int)bbq_vec_len(c->locals); i++) free(c->locals[i]);
    bbq_vec_free(c->locals);
    for (int i = 0; i < (int)bbq_vec_len(c->labels); i++) free(c->labels[i]);
    bbq_vec_free(c->labels);
    bbq_vec_free(c->el_funcs);              /* §6.6.9 elemlist scratch */
    bbq_vec_free(c->el_exprs);
}

jav_module_t *wat_assemble(const char *src, int len,
                            int *err_line, int *err_col) {
    wat_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    bbq_arena_init(&ctx.arena, 64 * 1024);

    peg_state p;
    /* pass 1: collect every $id binding (forward references resolve in pass 2). */
    ctx.pass = 1;
    wat_parser_init(&p, src, len);
    p.user_data = &ctx;
    int ok = wat_parser_parse(&p);
    if (ok) {
        /* pass 2: build the module + resolve references against the pass-1 maps. */
        ctx.pass = 2;
        ctx.mod = calloc(1, sizeof *ctx.mod);
        wat_parser_init(&p, src, len);
        p.user_data = &ctx;
        ok = wat_parser_parse(&p);
    }

    jav_module_t *out = NULL;
    if (ok) {
        out = ctx.mod;
    } else {
        if (err_line) *err_line = p.furthest.line;
        if (err_col)  *err_col = p.furthest.col;
        if (ctx.mod) { jav_module_free(ctx.mod); free(ctx.mod); }
    }
    wat_ctx_teardown(&ctx);
    return out;
}
