/* descriptor.h — JVM type and method descriptor parsing/generation
 *
 * Parses JVM descriptors from export files:
 *   "B" → JT_BYTE, "[S" → array of short, "Ljava/lang/String;" → class ref
 *   "([BSB)V" → params=[array(byte), short, byte], return=void
 *
 * Generates JVM descriptors from java_type_t for export file writing.
 */
#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include "javelina/compiler/sema.h"

/* Parse a single type descriptor starting at desc[*pos].
   Advances *pos past the consumed characters.
   Returns JT_ERROR on failure. */
java_type_t desc_parse_type(bbq_arena* a, const char* desc, int* pos,
                         const sema_ctx_t* ctx);

/* Parse a method descriptor "(params)return".
   Returns param types and count via out params, return type as result.
   Returns JT_ERROR on failure. */
java_type_t desc_parse_method(bbq_arena* a, const char* desc,
                           java_type_t** out_params, int* out_param_count,
                           const sema_ctx_t* ctx);

/* Generate a type descriptor string from a java_type_t.
   Returns arena-allocated string. */
const char* desc_from_type(bbq_arena* a, java_type_t type, const sema_ctx_t* ctx);

/* Generate a method descriptor string from param/return types.
   Returns arena-allocated string like "([BSB)V". */
const char* desc_from_method(bbq_arena* a, const java_type_t* params, int param_count,
                             java_type_t return_type, const sema_ctx_t* ctx);

#endif /* DESCRIPTOR_H */
