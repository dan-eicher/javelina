// jav_view_nav.c — section/code navigation over the c-lite span index.
#include "jav_view_nav.h"
#include "jav_view_reader.h"   // jav_view_module_read
#include <string.h>

// ── the shared span-index navigation primitives (one home, used by every loader TU) ──
const bbq_field_capture* jav_view_field(const bbq_field_capture* n, const char* name) {
    if (!n) return NULL;
    for (int i = 0; i < n->child_count; i++)
        if (n->children[i].name && strcmp(n->children[i].name, name) == 0)
            return &n->children[i];
    return NULL;
}
uint32_t jav_view_nchild(const bbq_field_capture* n) { return n ? (uint32_t)n->child_count : 0; }

// The chosen production of a discriminated-union node — a `switch`/choice rule (e.g. §5.5.7 Table:
// `tabletype` short form vs `0x40 0x00 tabletype expr` explicit-init form) parses into a wrapper whose
// LONE child is the matched arm. Returning that child by this named accessor keeps the union's
// node-shape assumption in ONE place, rather than scattering raw `children[0]` reaches through the loader.
const bbq_field_capture* jav_view_choice(const bbq_field_capture* n) {
    return (n && n->child_count > 0) ? &n->children[0] : NULL;
}

const bbq_field_capture* jav_view_section_array(const bbq_field_capture* root, int id,
                                                const char* field, const uint8_t* buf) {
    const bbq_field_capture* s = jav_view_find_section(root, id, buf);
    return s ? jav_view_field(jav_view_field(s, "body"), field) : NULL;
}

bbq_capture_metadata jav_view_module(const uint8_t* data, size_t len, bbq_arena* arena) {
    return jav_view_module_read(data, len, arena);
}

const bbq_field_capture* jav_view_find_section(const bbq_field_capture* root, int id,
                                               const uint8_t* buf) {
    const bbq_field_capture* sections = jav_view_field(root, "sections");
    if (!sections) return NULL;
    for (int i = 0; i < sections->child_count; i++) {
        const bbq_field_capture* sec = &sections->children[i];
        const bbq_field_capture* sid = jav_view_field(sec, "id");
        if (sid && (int)bbq_node_int(sid, buf) == id) return sec;
    }
    return NULL;
}

bbq_bytes_t jav_view_code_entry_bytes(const bbq_field_capture* code_section,
                                      int entry_index, const uint8_t* buf) {
    bbq_bytes_t out = { NULL, 0 };
    const bbq_field_capture* entries = jav_view_field(jav_view_field(code_section, "body"), "entries");
    if (!entries || entry_index < 0 || entry_index >= entries->child_count) return out;
    const bbq_field_capture* body = jav_view_field(&entries->children[entry_index], "body");
    if (!body) return out;
    out.data = buf + body->start_offset;
    out.length = body->end_offset - body->start_offset;
    return out;
}
