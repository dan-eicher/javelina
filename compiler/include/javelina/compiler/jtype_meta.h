/* jtype_meta.h — per-Java-type-tag metadata tables.
 *
 * Centralizes the spec-defined facts about each JT_* value:
 *   - jtype_min[t] / jtype_max[t]: representable signed range of
 *                            primitive numeric tags per JLS §5.1.2
 *                            (BYTE: -128..127, SHORT: -32768..32767,
 *                            INT: INT32_MIN..INT32_MAX, BOOL: 0..1).
 *                            Both zero for non-numeric tags.
 *   - jtype_desc_char[t]: JVMS §4.3 type-descriptor character
 *                            (BOOL='Z', BYTE='B', SHORT='S', INT='I',
 *                            VOID='V', CLASS='L', ARRAY='['). Zero
 *                            for tags without a single-char form
 *                            (NULL, ERROR).
 *
 * Indexed by `java_type_tag_t`. Out-of-enum indices are zero. */
#ifndef JAVELINA_COMPILER_JTYPE_META_H
#define JAVELINA_COMPILER_JTYPE_META_H

#include <stdint.h>
#include "javelina/compiler/sema.h"

extern const int32_t jtype_min[];
extern const int32_t jtype_max[];
extern const char    jtype_desc_char[];

#endif
