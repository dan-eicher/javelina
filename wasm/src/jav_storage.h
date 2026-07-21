/*
 * jav_storage.h — pack/unpack a 16-byte operand `slot_t` to/from natural-width
 * storage (a heap field/element, a global cell, a constant pool slot).
 *
 * The recurring hazard this kills: the operand stack stores values as 16-byte
 * `slot_t`, but everywhere else a value lives at its NATURAL width. Aiming a
 * `slot_t*` (or assuming 16-byte stride) at narrower storage is the type-pun that
 * produced the GC field-stride bug. Go through these helpers instead — they copy
 * exactly `jav_valtype_size(t)` bytes (the one generated source of truth), never
 * reinterpret a pointer, so the width can't be gotten wrong at the call site.
 *
 * INCLUDE ORDER: the includer must have `slot_t` in scope already (i.e. include
 * runtime_api.h / the value model before this). Targets little-endian (the JIT is
 * x86-64-only): a value occupies the low bytes of its slot, so copying the low
 * `size` bytes is the value. jav_valtype.h supplies jav_valtype_size.
 */
#ifndef JAV_STORAGE_H
#define JAV_STORAGE_H
#include <string.h>
#include "jav_valtype.h"

/* Write a value of type t into storage at p: exactly jav_valtype_size(t) bytes. */
static inline void jav_slot_store(void* p, jav_valtype_t t, slot_t v) {
    memcpy(p, &v, jav_valtype_size(t));
}

/* Read a value of type t from storage at p into a zeroed slot (high bytes clean so
 * a narrow value never carries stale upper bits into the operand stack). */
static inline slot_t jav_slot_load(const void* p, jav_valtype_t t) {
    slot_t v;
    memset(&v, 0, sizeof v);
    memcpy(&v, p, jav_valtype_size(t));
    return v;
}

#endif /* JAV_STORAGE_H */
