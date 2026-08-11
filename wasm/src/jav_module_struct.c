// jav_module_struct.c — §5.5 module structure over the c-lite span index (see the header).
#include "jav_module_struct.h"
#include "jav_view_nav.h"   // jav_view_field / jav_view_nchild / bbq_node_int

// §5.5.17's prescribed sequence, transcribed from the `module` production:
//
//   customsec* type*:typesec      customsec* import*:importsec
//   customsec* typeidx*:funcsec   customsec* table*:tablesec
//   customsec* mem*:memsec        customsec* tag*:tagsec
//   customsec* global*:globalsec  customsec* export*:exportsec
//   customsec* start?:startsec    customsec* elem*:elemsec
//   customsec* n?:datacntsec      customsec* (local*, expr)*:codesec
//   customsec* data*:datasec      customsec*
//
// as a rank per section id. It is NOT id order, and §5.5.2's own note says so
// ("Section ids do not always correspond to the order of sections in the encoding of
// a module"): the tag section (13) sits between memory (5) and global (6), and the
// data count section (12) precedes the code section (10) — which is exactly why the
// data count exists, per §5.5.15's note about single-pass validation.
//
// Custom sections rank -1: "Custom sections may be inserted at any place in this
// sequence", so they neither advance the rank nor collide with each other.
static int section_rank(int id) {
    switch (id) {
    case 0:  return -1;                          // custom: anywhere, repeatable
    case 1:  return 0;   case 2:  return 1;      // type, import
    case 3:  return 2;   case 4:  return 3;      // function, table
    case 5:  return 4;   case 13: return 5;      // memory, TAG
    case 6:  return 6;   case 7:  return 7;      // global, export
    case 8:  return 8;   case 9:  return 9;      // start, element
    case 12: return 10;  case 10: return 11;     // DATA COUNT, code
    case 11: return 12;                          // data
    default: return -1;                          // unreachable: the grammar rejects other ids
    }
}

// A section's `count` leaf, or `absent` when the section is not present at all.
// §5.5.2: "Every section is optional; an omitted section is equivalent to the section
// being present with empty contents" — so an absent list section counts 0, while an
// absent data COUNT section is a distinct state (ε, not 0) that its own rule tests for.
static int64_t section_count(const bbq_field_capture* root, const uint8_t* base,
                             int id, int64_t absent) {
    const bbq_field_capture* s = jav_view_find_section(root, id, base);
    if (!s) return absent;
    return bbq_node_int(jav_view_field(jav_view_field(s, "body"), "count"), base);
}

jav_err_t jav_module_struct(const bbq_field_capture* root, const uint8_t* base) {
    const bbq_field_capture* sections = jav_view_field(root, "sections");
    if (!sections) return JAV_E_NONE;                    // no sections is a legal module

    // ── §5.5.17: non-custom sections occur at most once and in the prescribed order.
    // Strictly increasing rank gives both at once: a repeat has an equal rank and a
    // transposition a lower one.
    int last_rank = -1;
    for (uint32_t i = 0; i < jav_view_nchild(sections); i++) {
        int id = (int)bbq_node_int(jav_view_field(&sections->children[i], "id"), base);
        int r = section_rank(id);
        if (r < 0) continue;                             // custom
        if (r <= last_rank) return JAV_E_SECTION_ORDER;
        last_rank = r;
    }

    // ── §5.5.17: "The lengths of lists produced by the (possibly empty) function and
    // code section must match up." Absent = empty, hence 0 on both sides.
    if (section_count(root, base, 3, 0) != section_count(root, base, 10, 0))
        return JAV_E_FUNC_CODE_LENGTHS;

    // ── §5.5.15: "If this count does not match the length of the data segment list,
    // the module is malformed." Only when the data count section is present (ε ≠ 0:
    // no data count section at all is legal, a data count of 0 with one segment is not).
    int64_t datacnt = section_count(root, base, 12, -1);
    if (datacnt >= 0 && datacnt != section_count(root, base, 11, 0))
        return JAV_E_DATA_COUNT_LENGTHS;

    // ── §5.5.13: `func ::= loc**:list(locals) e:expr ⇒ (+ loc**, e)  if |+ loc**| < 2^32`.
    // The locals are run-length encoded, so the bound is on the SUM the groups expand
    // to — a value no interval bounds, since these are slots and not bytes. Summed in
    // u64 precisely so the overflow this rule exists to reject cannot wrap away.
    const bbq_field_capture* entries = jav_view_section_array(root, 10, "entries", base);
    for (uint32_t d = 0; d < jav_view_nchild(entries); d++) {
        const bbq_field_capture* groups =
            jav_view_field(jav_view_field(&entries->children[d], "body"), "locals");
        uint64_t total = 0;
        for (uint32_t g = 0; g < jav_view_nchild(groups); g++) {
            total += (uint64_t)(uint32_t)bbq_node_int(
                jav_view_field(&groups->children[g], "count"), base);
            if (total >= (uint64_t)1 << 32) return JAV_E_TOO_MANY_LOCALS;
        }
    }

    return JAV_E_NONE;
}
