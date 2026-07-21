/* wasm_module.h — assemble a complete .wasm module from a compiled program.
 *
 * The backend (burg + structured emit) produces each function's body BYTES; this
 * decodes them with the shared `jav_func_body_read` into structs and serializes
 * the whole module with the one shared `jav_module_write` — the same encoder
 * `water` uses, so there is a single binary-layout authority (wasm.bbq) and the
 * decode doubles as a spec-grammar verification gate on the backend's bytes. */
#ifndef WASM_MODULE_H
#define WASM_MODULE_H

#include "javelina/compiler/emit_wasm.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/compiler.h"
#include "javelina/compiler/wasm_types.h"
#include "gen/sir_ast.h"

/* Assemble a complete multi-function module from a compiled program into
 * `out->code`: registers the emitted functions on `wt` (the function-index
 * authority), codegens every emitted method's body, decodes each via
 * `jav_func_body_read`, and serializes type/function/export/code sections with
 * `jav_module_write`. `methods[0..mc)` is compiler_compile's output; `cctx`
 * supplies the per-method scope sidecar; `sctx` the signatures. Returns false
 * (and leaves a diagnostic on stderr) if any body fails the spec-grammar decode
 * or serialization fails — a backend bug, surfaced, never shipped. This is the
 * E0 backbone the later phases grow (GC type / tag / global / element sections). */
bool wasm_assemble_program(compiler_ctx_t* cctx, const sema_ctx_t* sctx,
                           wasm_types_t* wt, sir_method_t** methods, int mc,
                           emit_wasm_ctx* out);

#endif /* WASM_MODULE_H */
