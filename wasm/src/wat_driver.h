/*
 * wat_driver.h — drive the generated .wat reader end to end.
 *
 * The pegc parser exposes only per-pass primitives (wat_parser_init/parse) over a
 * wat_ctx_t; assembling a module is the two-pass dance (pass 1 binds every $id so
 * forward references resolve, pass 2 builds + resolves) plus the ctx teardown.
 * This wraps that into one call so the converter (water) and the tests share ONE
 * driver instead of each re-deriving it.
 */
#ifndef WAT_DRIVER_H
#define WAT_DRIVER_H

#include "wat_parser.h"   /* wat_ctx_t + the generated parser + jav_types.h */

/* Parse `len` bytes of .wat `src` into a jav_module_t (malloc'd; the caller frees
 * it with jav_module_free + free). Returns NULL on a parse failure, writing the
 * 1-based failure line and column through err_line and err_col when non-NULL.
 * All parse scratch is reclaimed before returning, success or failure.
 *
 * The mnemonic table is generated from spec/instructions.toml at build time
 * (gen/wat_mnemonics.h), so there is no data file to locate at runtime. */
jav_module_t *wat_assemble(const char *src, int len,
                            int *err_line, int *err_col);

#endif /* WAT_DRIVER_H */
