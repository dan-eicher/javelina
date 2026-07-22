// test_module_wf.c — jav_module_wf's §5.5 structural rules, one hand-built
// violation each.
//
// Why this exists: jav_module_wf is the audit the COMPILER runs over every
// module it assembles (wasm_assemble_program), and it is what caught the tag
// section being emitted after codesec. But the compiler only ever exercises the
// section-ORDER rule — a correct emitter never trips the other five, so five of
// six rules had no test and could not go red. The conformance runner does not
// cover them either: it feeds jav_module_wf modules that PARSED, so a rule only
// fires there by accident.
//
// Each case below builds a jav_module_t violating exactly ONE rule and asserts
// both the rejection and the reason. Everything is stack/array storage — nothing
// here goes through jav_module_read, precisely so the violation is the only
// thing under test.

#include "jav_validate_module.h"
#include "javelina_test.h"
#include <string.h>

// A section carrying only what jav_module_wf reads: the discriminant and the
// per-kind count. `id` is the union tag (the validator reads body.tag, which is
// what jav_module_read sets from the section id byte).
static jav_section_t sec(int id) {
    jav_section_t s;
    memset(&s, 0, sizeof s);
    s.id = (uint8_t)id;
    s.body.tag = id;
    return s;
}

static bool wf(jav_section_t *secs, size_t n, const char **err) {
    jav_module_t m;
    memset(&m, 0, sizeof m);
    m.sections.items = secs;
    m.sections.count = n;
    *err = NULL;
    return jav_module_wf(&m, err);
}

// Reject, AND for the stated reason — a rule that fires with the wrong message
// is a rule that will mislead whoever hits it.
static void reject_because(jav_section_t *secs, size_t n, const char *want, const char *label) {
    const char *err = NULL;
    bool ok = wf(secs, n, &err);
    CHECK(!ok, label);
    CHECK(!ok && err && strcmp(err, want) == 0, want);
}

int main(void) {
    // ── the positive control: a minimal well-formed module ──
    // type(1), function(3) with one entry, code(10) with one matching body.
    {
        jav_code_entry_t entry; memset(&entry, 0, sizeof entry);
        jav_section_t s[3] = { sec(1), sec(3), sec(10) };
        s[1].body.u.case_3.count = 1;
        s[2].body.u.case_10.count = 1;
        s[2].body.u.case_10.entries.items = &entry;
        s[2].body.u.case_10.entries.count = 1;
        const char *err = NULL;
        CHECK(wf(s, 3, &err), "a well-formed module is accepted");
        CHECK(err == NULL, "...and reports no reason");
    }

    // ── §5.5.2: the section ids are 0..13 ──
    {
        jav_section_t s[1] = { sec(14) };
        reject_because(s, 1, "unknown section id", "section id 14 is rejected");
    }

    // ── §5.5.17: sections occur at most once and in the prescribed order.
    // global(6) then memory(5) is backwards — memsec precedes globalsec. ──
    {
        jav_section_t s[2] = { sec(6), sec(5) };
        reject_because(s, 2, "section out of order or duplicated",
                       "memory after global is rejected (order)");
    }
    {
        jav_section_t s[2] = { sec(1), sec(1) };
        reject_because(s, 2, "section out of order or duplicated",
                       "a repeated type section is rejected (duplicate)");
    }
    // Custom sections (id 0) are exempt: any position, repeatable.
    {
        jav_section_t s[5] = { sec(0), sec(1), sec(0), sec(5), sec(0) };
        const char *err = NULL;
        CHECK(wf(s, 5, &err), "custom sections may repeat and sit anywhere");
    }

    // ── §5.5: function and code list lengths must match ──
    {
        jav_code_entry_t entry; memset(&entry, 0, sizeof entry);
        jav_section_t s[2] = { sec(3), sec(10) };
        s[0].body.u.case_3.count = 2;              // two declared
        s[1].body.u.case_10.count = 1;             // one body
        s[1].body.u.case_10.entries.items = &entry;
        s[1].body.u.case_10.entries.count = 1;
        reject_because(s, 2, "function and code sections have inconsistent lengths",
                       "2 functions with 1 code body is rejected");
    }

    // ── §5.5.15: datacount, when present, equals the data segment count ──
    {
        jav_section_t s[2] = { sec(12), sec(11) };
        s[0].body.u.case_12.count = 2;             // datacount says 2
        s[1].body.u.case_11.count = 1;             // one segment present
        reject_because(s, 2, "data count and data section have inconsistent lengths",
                       "datacount 2 with 1 data segment is rejected");
    }

    // ── §5.5.13: the summed local count must fit u32 ──
    {
        jav_locals_t groups[2];
        memset(groups, 0, sizeof groups);
        groups[0].count = 0xFFFFFFFFu;             // each fits alone;
        groups[1].count = 1;                       // the SUM does not
        jav_code_entry_t entry; memset(&entry, 0, sizeof entry);
        entry.body.locals.items = groups;
        entry.body.locals.count = 2;
        jav_section_t s[2] = { sec(3), sec(10) };
        s[0].body.u.case_3.count = 1;
        s[1].body.u.case_10.count = 1;
        s[1].body.u.case_10.entries.items = &entry;
        s[1].body.u.case_10.entries.count = 1;
        reject_because(s, 2, "too many locals",
                       "locals summing past u32 is rejected");
    }

    // ── §5.5.15: memory.init / data.drop require the datacount section ──
    {
        jav_instr_t ins;                           // memory.init = 0xFC 8
        memset(&ins, 0, sizeof ins);
        ins.op = 0xFC;
        ins.body.tag = 31;                         // the 0xFC-prefixed family
        ins.body.u.case_31.sub = 8;
        jav_code_entry_t entry; memset(&entry, 0, sizeof entry);
        entry.body.body.instrs.items = &ins;
        entry.body.body.instrs.count = 1;
        jav_section_t s[2] = { sec(3), sec(10) };
        s[0].body.u.case_3.count = 1;
        s[1].body.u.case_10.count = 1;
        s[1].body.u.case_10.entries.items = &entry;
        s[1].body.u.case_10.entries.count = 1;
        reject_because(s, 2, "data count section required",
                       "memory.init without a datacount section is rejected");
    }

    return TEST_SUMMARY("test_module_wf");
}
