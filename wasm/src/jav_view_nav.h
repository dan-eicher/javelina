// jav_view_nav.h — navigation over the c-lite zero-copy span index (bbq_lite).
// The VM's module-read path: the .wasm is mmap'd/read once and indexed by the
// generated jav_view_reader; this layer locates sections by id and recovers a
// code body as an offset+length span into the SAME image — the byte interval the
// overlay already records for the body node. The runtime executes off that span —
// the owning tree is never built.
#ifndef JAV_VIEW_NAV_H
#define JAV_VIEW_NAV_H

#include "bbq_lite.h"   // bbq_capture_metadata / bbq_field_capture / bbq_node_int
#include "bbq_read.h"   // bbq_bytes_t

// ── span-index navigation primitives (shared by every loader TU — index, validate,
// instantiate — so the by-name/by-section walks live in ONE place, not copy-pasted) ──
// Direct child of `n` named `name` (NULL if absent); child count; and the named element
// array under section `id`'s body (e.g. id 6 / "globals").
const bbq_field_capture* jav_view_field(const bbq_field_capture* n, const char* name);
uint32_t                 jav_view_nchild(const bbq_field_capture* n);
// The chosen arm of a discriminated-union / `switch` node (its lone child), or NULL if none — so a
// loader reads `switch_node`'s matched production by name instead of a positional children[0] reach.
const bbq_field_capture* jav_view_choice(const bbq_field_capture* n);
const bbq_field_capture* jav_view_section_array(const bbq_field_capture* root, int id,
                                                const char* field, const uint8_t* buf);

// Read+index a module image; the resolved index lives in `arena` (caller-owned,
// must outlive the returned root). Thin wrapper over jav_view_module_read.
bbq_capture_metadata jav_view_module(const uint8_t* data, size_t len, bbq_arena* arena);

// Locate the section with the given id (e.g. 10 = code) by decoding each
// section's `id` leaf against the image. NULL if absent.
const bbq_field_capture* jav_view_find_section(const bbq_field_capture* root, int id,
                                               const uint8_t* buf);

// Recover the runnable FuncBody span of the entry_index'th code entry as an
// offset+length into the image — the body node's own [start,end) interval.
// {NULL,0} on an out-of-range index or a malformed tree.
bbq_bytes_t jav_view_code_entry_bytes(const bbq_field_capture* code_section,
                                      int entry_index, const uint8_t* buf);

#endif // JAV_VIEW_NAV_H
