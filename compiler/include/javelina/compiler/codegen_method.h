/* codegen_method.h — SIR method → WASM function-body bytes.
 *
 * The calc/yoctojc codegen_method shape: burgc's generated matcher tiles the
 * value sub-trees and walks the cg_jump spine, emitting opcodes into the burg
 * context's byte buffer; this caps the result with the structured body `end`.
 * The caller owns the burg_ctx (and reads ctx->emit.code for the bytes). */
#ifndef CODEGEN_METHOD_H
#define CODEGEN_METHOD_H

#include "gen/codegen_matcher.h"        /* burg_ctx_t, burg_rewrite */
#include "gen/sir_ast.h"                /* sir_method_t */
#include "javelina/compiler/compiler.h" /* compiler_fact_t — the sidecar */

/* Emit method's function body (instruction stream + terminating `end`) into
 * ctx->emit. Straight-line + branchless only — burg_rewrite walks the spine. */
void codegen_method_body(sir_method_t* method, burg_ctx_t* ctx);

/* Destination-driven structured emit: handles control flow (if/while) by framing
 * block/loop/if from the sidecar's SCOPE rows (the DDCG recorded them as it built
 * the statements — the structurer never rediscovers a loop), tiling values via the
 * burg. Emits the full body + terminating `end`. Takes the method's whole fact
 * table and reads the kinds it owns. */
void codegen_method_structured(sir_method_t* method, const compiler_fact_t* facts,
                               int nfacts, burg_ctx_t* ctx);

#endif /* CODEGEN_METHOD_H */
