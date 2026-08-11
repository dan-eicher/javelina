// test_module_struct.c — the §5.5 structure gate, one hand-built violation each,
// driven through the loader entry the engine actually uses (jav_validate_bytes).
//
// Why this exists. These four conditions are stated by §5.5 over decoded section
// values, so no grammar rule can carry them and the c-lite reader accepts every one
// of these images happily. Until this gate existed the engine loaded all of them:
// measured against the official corpus, 25 assert_malformed binaries were ACCEPTED by
// jav_validate_bytes and 2 more took the process down. The conformance runner did not
// say so because it scored assert_malformed against the OWNING reader (which has its
// own copy of these rules), never against the tree the engine loads through.
//
// Each case violates exactly ONE condition and asserts both the verdict and the
// official reason string — a rule that fires for the wrong reason misleads whoever
// hits it, and the reason is what the .wast corpus matches on.
#include "jav_load.h"
#include "jav_error.h"
#include "javelina_test.h"
#include <stdio.h>
#include <string.h>

#define PREAMBLE 0x00,'a','s','m', 0x01,0x00,0x00,0x00

static int fails;

static void expect(const uint8_t *img, size_t n, jav_status_t want_st, jav_err_t want_err,
                   const char *label) {
    jav_err_t got_err = JAV_E_NONE;
    jav_status_t got_st = jav_validate_bytes(img, n, &got_err);
    if (got_st != want_st || got_err != want_err) {
        printf("  FAIL %-34s want=(%d,\"%s\") got=(%d,\"%s\")\n", label,
               want_st, jav_err_str(want_err), got_st, jav_err_str(got_err));
        fails++;
    }
}

int main(void) {
    // §5.5.17 "other sections must occur at most once and in the prescribed order".
    // Two start sections: the second cannot advance the rank.
    static const uint8_t dup_section[] = {
        PREAMBLE,
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x03,0x02,0x01,0x00,               // func:   1 function, type 0
        0x08,0x01,0x00,                    // start:  function 0
        0x08,0x01,0x00,                    // start:  AGAIN
        0x0a,0x04,0x01,0x02,0x00,0x0b,     // code:   1 empty body
    };
    expect(dup_section, sizeof dup_section, JAV_MALFORMED, JAV_E_SECTION_ORDER,
           "duplicate start section");

    // §5.5.17 again, transposed rather than repeated: the tag section (13) is ranked
    // between memory (5) and global (6), so emitting it after global is out of order
    // even though 13 > 6. This is the case an id-ordered check gets wrong.
    static const uint8_t tag_after_global[] = {
        PREAMBLE,
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x06,0x06,0x01,0x7f,0x00,0x41,0x00,0x0b,  // global: i32 immutable = 0
        0x0d,0x03,0x01,0x00,0x00,          // tag:    1 tag, attr 0, type 0
    };
    expect(tag_after_global, sizeof tag_after_global, JAV_MALFORMED, JAV_E_SECTION_ORDER,
           "tag section after global");

    // §5.5.17 "The lengths of lists produced by the (possibly empty) function and code
    // section must match up." One declared function, no code entry.
    static const uint8_t func_without_code[] = {
        PREAMBLE,
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x03,0x02,0x01,0x00,               // func:   1 function
        0x0a,0x01,0x00,                    // code:   0 entries
    };
    expect(func_without_code, sizeof func_without_code, JAV_MALFORMED, JAV_E_FUNC_CODE_LENGTHS,
           "function without code entry");

    // §5.5.15 "If this count does not match the length of the data segment list, the
    // module is malformed." Data count says 2, one segment follows.
    static const uint8_t datacount_mismatch[] = {
        PREAMBLE,
        0x05,0x03,0x01,0x00,0x01,          // memory: 1 memory, min 1
        0x0c,0x01,0x02,                    // datacount: 2
        0x0b,0x06,0x01,0x00,0x41,0x00,0x0b,0x00,  // data: 1 passive-form active segment, empty
    };
    expect(datacount_mismatch, sizeof datacount_mismatch, JAV_MALFORMED, JAV_E_DATA_COUNT_LENGTHS,
           "data count vs data segments");

    // A data count section with NO data section is legal only at count 0 — and the
    // absent-section state (ε) must not be confused with a count of 0. This one is
    // well-formed and must reach the bytecode verifier, so it pins that the rule does
    // not fire on the legal shape.
    static const uint8_t datacount_zero_no_data[] = {
        PREAMBLE,
        0x05,0x03,0x01,0x00,0x01,          // memory: 1 memory, min 1
        0x0c,0x01,0x00,                    // datacount: 0, and no data section
    };
    expect(datacount_zero_no_data, sizeof datacount_zero_no_data, JAV_OK, JAV_E_NONE,
           "data count 0, no data section");

    // §5.5.13 `func ::= loc**:list(locals) e:expr ⇒ (+ loc**, e)  if |+ loc**| < 2^32`.
    // Two groups summing to 2^32 + 1. This is the image that used to take the process
    // down: the sum was materialized as flat local slots before anyone checked it.
    static const uint8_t too_many_locals[] = {
        PREAMBLE,
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x03,0x02,0x01,0x00,               // func:   1 function
        0x0a,0x0c,0x01,                    // code:   1 entry, 12 bytes
          0x0a,0x02,                       //   entry size 10, 2 local groups
          0xff,0xff,0xff,0xff,0x0f,0x7f,   //   0xFFFFFFFF x i32
          0x02,0x7e,                       //   0x00000002 x i64
          0x0b,                            //   end
    };
    expect(too_many_locals, sizeof too_many_locals, JAV_MALFORMED, JAV_E_TOO_MANY_LOCALS,
           "too many locals");

    // The sum is what the rule bounds, not any single group: 4 x 2^30 is 2^32 exactly,
    // and no individual count is anywhere near the bound. A per-group check passes this.
    static const uint8_t too_many_locals_split[] = {
        PREAMBLE,
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x03,0x02,0x01,0x00,               // func:   1 function
        0x0a,0x1c,0x01,                    // code:   1 entry, 28 bytes
          0x1a,0x04,                       //   entry size 26, 4 local groups
          0x80,0x80,0x80,0x80,0x04,0x7f,   //   0x40000000 x i32
          0x80,0x80,0x80,0x80,0x04,0x7e,   //   0x40000000 x i64
          0x80,0x80,0x80,0x80,0x04,0x7d,   //   0x40000000 x f32
          0x80,0x80,0x80,0x80,0x04,0x7c,   //   0x40000000 x f64
          0x0b,                            //   end
    };
    expect(too_many_locals_split, sizeof too_many_locals_split, JAV_MALFORMED, JAV_E_TOO_MANY_LOCALS,
           "too many locals across groups");

    // The gate must not reject what §5.5 allows. Custom sections go anywhere and repeat;
    // this one puts them before, between and after the real sections, out of id order
    // relative to everything around them.
    static const uint8_t customs_everywhere[] = {
        PREAMBLE,
        0x00,0x03,0x02,'h','i',            // custom "hi"
        0x01,0x04,0x01,0x60,0x00,0x00,     // type:   () -> ()
        0x00,0x03,0x02,'h','i',            // custom "hi" again
        0x03,0x02,0x01,0x00,               // func:   1 function
        0x0a,0x04,0x01,0x02,0x00,0x0b,     // code:   1 empty body
        0x00,0x03,0x02,'h','i',            // custom "hi" once more
    };
    expect(customs_everywhere, sizeof customs_everywhere, JAV_OK, JAV_E_NONE,
           "custom sections anywhere");

    printf("module_struct: §5.5 structure gate over the loaded tree  [%s]\n",
           fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
