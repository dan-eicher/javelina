#include "jav_validate_module.h"

// Prescribed section order as a rank per id. Transcribed from §5.5.17's `module` production,
// which lists: typesec(1) importsec(2) funcsec(3) tablesec(4) memsec(5) TAGSEC(13)
// globalsec(6) exportsec(7) startsec(8) elemsec(9) DATACNTSEC(12) codesec(10) datasec(11).
// Not id order — §5.5.2's own note says so ("Section ids do not always correspond to the
// order of sections"), and the two that move are the ones easy to get wrong: tag sits between
// memory and global, and the data count precedes the code it exists to let a single pass
// validate. id 0 (custom) ranks -1 and is skipped — "Custom sections may be inserted at any
// place in this sequence".
static int section_rank(int id) {
    static const signed char R[14] = { -1, 0, 1, 2, 3, 4, 6, 7, 8, 9, 11, 12, 10, 5 };
    return (id >= 0 && id <= 13) ? R[id] : 127;
}

// Does any instruction in the sequence (recursing into block/loop/if/try_table
// bodies) reference a data segment? memory.init (0xFC 8) and data.drop (0xFC 9)
// are the two §5.5.15 triggers that make the data count section REQUIRED.
static bool instrs_use_data(const jav_instr_t *items, size_t n);

static bool instr_uses_data(const jav_instr_t *in) {
    switch (in->op) {                                 // body.tag == the matched op
        case 0x02: case 0x03:                         // block, loop
            return instrs_use_data(in->body.u.case_1.instrs.items,
                                   in->body.u.case_1.instrs.count);
        case 0x04: {                                  // if / else
            const jav_if_t *f = &in->body.u.case_2;
            if (instrs_use_data(f->then_body.items, f->then_body.count)) return true;
            return f->else_body.has_value
                && instrs_use_data(f->else_body.value.instrs.items,
                                   f->else_body.value.instrs.count);
        }
        case 0x1F:                                    // try_table
            return instrs_use_data(in->body.u.case_15.instrs.items,
                                   in->body.u.case_15.instrs.count);
        case 0xFC:                                    // misc prefix: memory.init (8), data.drop (9)
            return in->body.u.case_30.sub == 8 || in->body.u.case_30.sub == 9;
        case 0xFB:                                    // GC prefix: array.new_data (9), array.init_data (18)
            return in->body.u.case_29.sub == 9 || in->body.u.case_29.sub == 18;
        default:
            return false;
    }
}

static bool instrs_use_data(const jav_instr_t *items, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (instr_uses_data(&items[i])) return true;
    return false;
}

bool jav_module_wf(const jav_module_t *m, const char **err) {
    int last_rank = -1;
    uint32_t func_count = 0, code_count = 0, data_count = 0, data_segs = 0;
    bool have_data_count = false;
    const jav_code_section_t *code = NULL;

    for (size_t i = 0; i < m->sections.count; i++) {
        const jav_section_t *s = &m->sections.items[i];
        int id = s->body.tag;
        if (id == 0) continue;                        // custom: any position, repeatable
        if (id < 1 || id > 13) { if (err) *err = "unknown section id"; return false; }

        int r = section_rank(id);
        if (r <= last_rank) {
            if (err) *err = "section out of order or duplicated";
            return false;
        }
        last_rank = r;

        switch (id) {
            case 3:  func_count = s->body.u.case_3.count;  break;   // function
            case 10: code_count = s->body.u.case_10.count; code = &s->body.u.case_10; break;
            case 11: data_segs  = s->body.u.case_11.count; break;   // data
            case 12: data_count = s->body.u.case_12.count; have_data_count = true; break; // datacount
            default: break;
        }
    }

    // §5.5: function and code section list lengths must match (absent = 0).
    if (func_count != code_count) {
        if (err) *err = "function and code sections have inconsistent lengths";
        return false;
    }
    // §5.5.15: data count, if present, must equal the number of data segments.
    if (have_data_count && data_count != data_segs) {
        if (err) *err = "data count and data section have inconsistent lengths";
        return false;
    }
    for (size_t i = 0; code && i < code->entries.count; i++) {
        const jav_func_body_t *fb = &code->entries.items[i].body;
        // §5.5.13: the total local count (sum over the RLE groups) must fit u32.
        uint64_t locals = 0;
        for (size_t j = 0; j < fb->locals.count; j++)
            locals += fb->locals.items[j].count;
        if (locals > 0xFFFFFFFFull) {
            if (err) *err = "too many locals";
            return false;
        }
        // §5.5.15: memory.init / data.drop require the data count section.
        if (!have_data_count
            && instrs_use_data(fb->body.instrs.items, fb->body.instrs.count)) {
            if (err) *err = "data count section required";
            return false;
        }
    }
    return true;
}
