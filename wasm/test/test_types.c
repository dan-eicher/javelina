// test_types.c — P1 gate for the full §5.3 / §5.5.4 type section. Spec-complete:
// one fixture exercises every form (num/vec/ref/abstract-heap valtypes, packed
// storage, mutability, func/struct/array composite types, sub final/open with
// supertypes, an explicit recursive group, and the singleton shorthands), with
//   (a) structural spot-checks per form, and
//   (b) the read∘write == identity round-trip over the whole section.
// Negative fixtures then prove the parser is fail-closed: invalid valtype byte,
// invalid mutability, and unknown composite/rectype discriminants are rejected.

#include "jav_reader.h"
#include "jav_writer.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
static void check(const char *name, int ok) {
    printf("  %-52s [%s]\n", name, ok ? "PASS" : "FAIL");
    if (!ok) fails++;
}

// A type-section payload (vec(rectype)): count=10, then one entry per form.
static const uint8_t TS[] = {
    0x0A,                                     // 10 types
    /* 0 */ 0x60,0x02,0x7F,0x7F,0x01,0x7F,    // func (i32,i32)->(i32)
    /* 1 */ 0x60,0x00,0x00,                    // func ()->()
    /* 2 */ 0x5F,0x02, 0x7F,0x01, 0x7C,0x00,   // struct {mut i32, const f64}
    /* 3 */ 0x5E,0x78,0x01,                     // array mut i8 (packed)
    /* 4 */ 0x5E,0x77,0x00,                     // array const i16 (packed)
    /* 5 */ 0x60,0x04, 0x7B,0x70,0x63,0x6E,0x64,0x00, 0x01,0x6F,
            // func (v128, funcref, (ref null any), (ref $0)) -> (externref)
    /* 6 */ 0x4F,0x01,0x00, 0x60,0x00,0x00,     // sub final [super 0] func ()->()
    /* 7 */ 0x50,0x00, 0x5F,0x01,0x7F,0x00,     // sub open [] struct {const i32}
    /* 8 */ 0x4E,0x02, 0x60,0x00,0x00, 0x5F,0x00, // rec group (func ()->(), struct{})
    /* 9 */ 0x60,0x0C, 0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,0x73,0x74, 0x00,
            // func with all 12 abstract heap-type shorthand params -> ()
};

static int reject(const uint8_t *buf, size_t n) {
    bbq_ctx_t c; bbq_ctx_init(&c, buf, n);
    jav_type_section_t ts; memset(&ts, 0, sizeof ts);
    int r = jav_type_section_read(&c, &ts) == false;   // must fail-closed
    jav_type_section_free(&ts);                        // a partial parse still allocated rows
    return r;
}

int main(void) {
    bbq_ctx_t c; bbq_ctx_init(&c, TS, sizeof TS);
    jav_type_section_t ts; memset(&ts, 0, sizeof ts);
    check("type section parses", jav_type_section_read(&c, &ts));
    check("consumed whole payload", c.pos == sizeof TS);
    check("10 types", ts.count == 10 && ts.types.count == 10);
    if (ts.types.count != 10) { printf("\nP1 types: FAILURES\n"); return 1; }

    jav_rec_type_t *T = ts.types.items;

    // 0: func (i32,i32)->(i32)
    const jav_func_type_t *f0 = &T[0].body.u.case_5;
    check("0: func, head 0x60",
          T[0].head == 0x60 && T[0].body.tag == 0x60);
    check("0: params i32,i32 -> i32",
          f0->param_count == 2 && f0->params.count == 2 &&
          f0->params.items[0].head == 0x7F && f0->params.items[1].head == 0x7F &&
          !f0->params.items[0].ht.has_value &&
          f0->result_count == 1 && f0->results.items[0].head == 0x7F);

    // 2: struct {mut i32, const f64}
    const jav_struct_type_t *s2 = &T[2].body.u.case_4;
    check("2: struct, 2 fields, mut+packing flags",
          T[2].head == 0x5F && s2->field_count == 2 &&
          s2->fields.items[0].storage.head == 0x7F && s2->fields.items[0].mut == 1 &&
          s2->fields.items[1].storage.head == 0x7C && s2->fields.items[1].mut == 0);

    // 3: array mut i8 (packed storage type)
    const jav_array_type_t *a3 = &T[3].body.u.case_3;
    check("3: array of packed i8 (0x78), mutable",
          T[3].head == 0x5E && a3->field.storage.head == 0x78 && a3->field.mut == 1);
    check("4: array of packed i16 (0x77), const",
          T[4].head == 0x5E && T[4].body.u.case_3.field.storage.head == 0x77 &&
          T[4].body.u.case_3.field.mut == 0);

    // 5: reference-type valtypes, incl. explicit (ref null ht) / (ref ht)
    const jav_func_type_t *f5 = &T[5].body.u.case_5;
    check("5: v128 + funcref + (ref null any) + (ref $0) -> externref",
          f5->params.count == 4 &&
          f5->params.items[0].head == 0x7B &&                 // v128
          f5->params.items[1].head == 0x70 &&                 // funcref shorthand
          f5->params.items[2].head == 0x63 && f5->params.items[2].ht.has_value && // ref null any
          f5->params.items[3].head == 0x64 && f5->params.items[3].ht.has_value && // ref $0
          f5->params.items[3].ht.value.x == 0 &&              // typeidx 0
          f5->results.items[0].head == 0x6F);                 // externref

    // 6: sub final with one supertype, body func
    const jav_sub_type_t *u6 = &T[6].body.u.case_1;
    check("6: sub final, super [0], comptype func",
          T[6].head == 0x4F && u6->super_count == 1 && u6->supers.items[0] == 0 &&
          u6->body.head == 0x60 && u6->body.body.tag == 0x60);

    // 7: sub open, no supertypes, body struct
    const jav_sub_type_t *u7 = &T[7].body.u.case_2;
    check("7: sub open, no supers, comptype struct",
          T[7].head == 0x50 && u7->super_count == 0 && u7->body.head == 0x5F);

    // 8: explicit recursive group of two members
    const jav_rec_group_t *g8 = &T[8].body.u.case_0;
    check("8: rec group of 2 (func, struct)",
          T[8].head == 0x4E && g8->count == 2 && g8->members.count == 2 &&
          g8->members.items[0].head == 0x60 && g8->members.items[1].head == 0x5F);

    // 9: every abstract heap-type shorthand as a valtype
    const jav_func_type_t *f9 = &T[9].body.u.case_5;
    check("9: all 12 abstract heap-type valtypes (0x69..0x74)",
          f9->param_count == 12 &&
          f9->params.items[0].head == 0x69 && f9->params.items[11].head == 0x74);

    // Round-trip: the whole section must re-encode byte-for-byte.
    uint8_t out[sizeof TS + 32];
    bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
    int rt = jav_type_section_write(&w, &ts) &&
             w.pos == sizeof TS && memcmp(out, TS, sizeof TS) == 0;
    check("round-trip read->write == identity", rt);
    bbq_write_ctx_free(&w);   // the writer's internal length-backpatch stack

    // ── Fail-closed negatives (each must be rejected) ─────────────────────
    check("reject: invalid valtype byte 0x00",
          reject((uint8_t[]){0x01, 0x60,0x01,0x00, 0x00}, 5));
    check("reject: mutability flag 0x02 (>1)",
          reject((uint8_t[]){0x01, 0x5F,0x01,0x7F,0x02}, 5));
    check("reject: unknown rectype discriminant 0x61",
          reject((uint8_t[]){0x01, 0x61}, 2));
    check("reject: unknown comptype byte 0x61 inside sub",
          reject((uint8_t[]){0x01, 0x4F,0x00,0x61}, 4));

    jav_type_section_free(&ts);
    printf("\nP1 types: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
