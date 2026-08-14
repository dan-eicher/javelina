/*
 * wat_emit.c — the §6 text. See wat_emit.h.
 *
 * RED-FIRST SKELETON, for the same reason as wat_check.c: PIN C-0a/C-0b are authored
 * with PIN A-1, before Part A's code, and they have to fail on the string they wanted
 * rather than on a missing symbol. Part C replaces this with the labeller and the
 * reduce walk over wat_layout.burg.
 */
#include "wat_emit.h"

int wat_emit_module(const jav_module_t* m, const wat_check_ctx_t* cx, int width,
                    bbq_arena* a, const char** out, size_t* out_len) {
    (void)m; (void)cx; (void)width; (void)a; (void)out; (void)out_len;
    return 0;
}
