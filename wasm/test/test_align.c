// test_align.c — §6.2 decode contract: the validator's natural-alignment table vs
// the spec.
//
// validate.c derives a memarg's maximum alignment from a HAND-WRITTEN opcode->width
// switch (`int maxa = N==8?0 : ...`). §3.4.5 says align must not exceed the natural
// width, so a wrong row there silently accepts a module the spec rejects, or rejects
// a valid one — and nothing checked that table against the spec.
//
// instructions.toml carries `align` (log2 of natural alignment) for all 45 memarg
// instructions, extracted from §6.5.6. This drives the real validator entry point
// with align = natural (must not raise JAV_E_ALIGNMENT) and align = natural + 1
// (must raise it). The alignment check precedes stack typing in validate.c, so the
// body needs no valid operands: any other verdict is an accepted answer for the
// in-range case, only the ALIGNMENT reason itself is under test.
#include "toml/toml_doc.h"
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u(uint8_t *b, int *n, uint32_t v) {   // uleb128
    do { uint8_t c = v & 0x7f; v >>= 7; if (v) c |= 0x80; b[(*n)++] = c; } while (v);
}

// A module with one memory and one `[] -> []` function whose body is the single
// instruction under test. Deliberately not stack-correct — see the header comment.
static int build(uint8_t *m, const uint8_t *instr, int ilen) {
    int n = 0;
    static const uint8_t hdr[] = {0,0x61,0x73,0x6d,1,0,0,0};
    memcpy(m + n, hdr, 8); n += 8;
    static const uint8_t sects[] = {
        1, 4, 1, 0x60, 0, 0,        // type:   [] -> []
        3, 2, 1, 0,                 // func:   one function, type 0
        5, 3, 1, 0, 1,              // memory: min 1 page
    };
    memcpy(m + n, sects, sizeof sects); n += sizeof sects;

    int body_len = 1 + ilen + 1;                 // locals count + instr + end
    m[n++] = 10;                                 // code section
    m[n++] = (uint8_t)(1 + 1 + body_len);        // section size
    m[n++] = 1;                                  // one body
    m[n++] = (uint8_t)body_len;
    m[n++] = 0;                                  // no local groups
    memcpy(m + n, instr, (size_t)ilen); n += ilen;
    m[n++] = 0x0b;                               // end
    return n;
}

static jav_err_t validate(const uint8_t *bytes, int len) {
    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata cm = jav_view_module((uint8_t *)bytes, (size_t)len, &a);
    jav_err_t err = JAV_E_NONE;
    if (cm.success) {
        jav_modidx_t mod;
        if (jav_module_index(cm.root, (uint8_t *)bytes, &a, &mod))
            jav_module_validate(cm.root, (uint8_t *)bytes, &mod, &err);
    }
    bbq_arena_free(&a);
    return err;
}

// One instruction: opcode (with prefix), memarg flags = align, offset 0, optional lane.
static int encode(uint8_t *b, int prefix, int code, uint32_t align, int has_lane) {
    int n = 0;
    if (prefix) { b[n++] = (uint8_t)prefix; put_u(b, &n, (uint32_t)code); }
    else        { b[n++] = (uint8_t)code; }
    put_u(b, &n, align);        // memarg flags (no 0x40 bit: memory 0)
    put_u(b, &n, 0);            // offset
    if (has_lane) b[n++] = 0;   // laneidx
    return n;
}

static char *slurp(const char *p, long *len) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)*len + 1);
    if (fread(s, 1, (size_t)*len, f) != (size_t)*len) { perror("fread"); exit(2); }
    s[*len] = 0; fclose(f); return s;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "../spec/instructions.toml";
    long len; char *src = slurp(path, &len);
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    toml_doc_t *doc = toml_parse(src, (int)len, &arena);
    if (toml_doc_has_errors(doc)) { fprintf(stderr, "instructions.toml parse error\n"); return 2; }
    const toml_val_t *arr = toml_tbl_get(toml_doc_root(doc), "instr");
    if (!arr) { fprintf(stderr, "no [[instr]] array\n"); return 2; }

    int checked = 0, bad = 0;
    for (int i = 0; i < toml_val_array_count(arr); i++) {
        const toml_tbl_t *t = toml_val_as_table(toml_val_array_at(arr, i));
        if (!t) continue;
        const toml_val_t *av = toml_tbl_get(t, "align");
        if (!av) continue;                       // not a memarg instruction
        int64_t align = 0;
        if (!toml_val_as_int(av, &align)) continue;

        const char *name = "?"; toml_val_as_string(toml_tbl_get(t, "name"), &name);
        const toml_val_t *ov = toml_tbl_get(t, "opcode");
        int prefix = 0, code = 0; int64_t a, b;
        if (toml_val_as_int(ov, &a)) code = (int)a;
        else if (toml_val_array_count(ov) == 2 &&
                 toml_val_as_int(toml_val_array_at(ov, 0), &a) &&
                 toml_val_as_int(toml_val_array_at(ov, 1), &b)) { prefix = (int)a; code = (int)b; }
        else continue;

        // operands = ["memarg", "laneidx"] on the lane forms
        const toml_val_t *ops = toml_tbl_get(t, "operands");
        int has_lane = 0;
        for (int k = 0; k < toml_val_array_count(ops); k++) {
            const char *o;
            if (toml_val_as_string(toml_val_array_at(ops, k), &o) && !strcmp(o, "laneidx")) has_lane = 1;
        }

        uint8_t instr[16], mod[128];
        int il = encode(instr, prefix, code, (uint32_t)align, has_lane);
        jav_err_t at_natural = validate(mod, build(mod, instr, il));

        il = encode(instr, prefix, code, (uint32_t)align + 1, has_lane);
        jav_err_t over = validate(mod, build(mod, instr, il));

        checked++;
        if (at_natural == JAV_E_ALIGNMENT) {
            printf("  %-26s align=%lld REJECTED as over-natural, spec says it is natural\n",
                   name, (long long)align); bad++;
        }
        if (over != JAV_E_ALIGNMENT) {
            printf("  %-26s align=%lld+1 ACCEPTED, spec natural is %lld (got err=%d)\n",
                   name, (long long)align, (long long)align, (int)over); bad++;
        }
    }
    printf("\n%d memarg instructions checked against the spec `align` column, %d wrong\n",
           checked, bad);
    if (!checked) { printf("natural alignment: FAIL (no rows checked)\n"); return 1; }
    printf("natural alignment: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
