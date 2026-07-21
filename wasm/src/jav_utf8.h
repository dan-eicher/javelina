/*
 * jav_utf8.h — §5.2.4 WASM name validation (the spec's `utf8` grammar over Unicode
 * scalar values). A SHARED toolchain helper: every reader's name `where`-clause calls
 * it — the binary reader, the .wat text reader (a separate TU), and the c-lite view
 * reader — so it lives in hand-written code with ONE definition, not duplicated in the
 * grammar's @source. The grammar just `@header (. #include "jav_utf8.h" .)` so the
 * generated readers see the declaration; everyone links the single jav_utf8.o.
 */
#ifndef JAV_UTF8_H
#define JAV_UTF8_H

#include "bbq_read.h"   /* bbq_bytes_t */
#include <stdbool.h>

bool jav_name_utf8_ok(bbq_bytes_t b);

#endif /* JAV_UTF8_H */
