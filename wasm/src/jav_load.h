// jav_load.h — the module loader entry: raw bytes → §5 decode (the c-lite zero-copy
// index) → §7 validation verdict, in ONE place. This is the bytes→verdict glue that the
// wasm-c-api shim (`wasm_module_new` / `wasm_module_validate`, Phase 3) wraps and the
// conformance runner consumes — neither re-implements the decode/index/validate pipeline.
#ifndef JAV_LOAD_H
#define JAV_LOAD_H

#include "jav_error.h"        // jav_err_t (the fine reason)
#include "runtime_api.h"      // jav_status_t
#include <stdint.h>
#include <stddef.h>

// Decode + validate a module image. Returns:
//   JAV_OK        — decodes and is §7-valid;
//   JAV_MALFORMED — §5 decode failed (not a well-formed module image);
//   JAV_INVALID   — decodes but fails §7 validation (or a construct not yet supported).
// On a non-OK result *err (if non-NULL) carries the fine reason; JAV_E_NONE on OK.
jav_status_t jav_validate_bytes(const uint8_t* bytes, size_t len, jav_err_t* err);

#endif // JAV_LOAD_H
