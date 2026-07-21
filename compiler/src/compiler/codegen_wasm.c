/* codegen_wasm.c — SIR method → WASM function-body bytes (see codegen_method.h). */
#include "javelina/compiler/codegen_method.h"

void codegen_method_body(sir_method_t* method, burg_ctx_t* ctx) {
    if (!method || !method->entry) return;
    /* burgc's matcher does the graph walk + tree tiling, emitting into ctx->emit
     * (the burg spine rules emit each node's opcodes; the RPO walk lays them out
     * in fall-through order). */
    burg_rewrite(method->entry, ctx);
    /* §5.4.1: a function body's instruction sequence is terminated by `end`. */
    ew_byte(&ctx->emit, W_END);
}
