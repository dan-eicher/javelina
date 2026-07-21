// test_instr.c — P2 gate for the §5.4 instruction tree. Reads the opcode table
// from spec/instructions.toml (the artifact generated once from the spec's
// reference decoder) via the pinched TOML parser, and for EVERY instruction:
// synthesizes a minimal valid instruction, wraps it as an expression, and
// asserts it decodes to one instruction with the right opcode AND round-trips
// (decode→encode == bytes). Then asserts fail-closed behavior on reserved
// opcodes / sub-opcodes, a misplaced `else`, and an unterminated expression;
// and a nested fixture for non-empty bodies / else / catch / memidx / vecs.

#include "jav_reader.h"
#include "jav_writer.h"
#include "yoctojc/toml/toml_doc.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { C_NONE,C_IDX,C_IDX2,C_I32,C_I64,C_F32,C_F64,C_HEAP,C_LANE,C_V128,
  C_MEMARG,C_MEMLANE,C_BRTABLE,C_SELECTT,C_BRONCAST,C_BLOCK,C_IF,C_TRYTABLE } icat_t;
typedef struct { int prefix; int code; icat_t cat; } iop_t;

static icat_t shape_cat(const char *s) {
    struct { const char *n; icat_t c; } M[] = {
        {"none",C_NONE},{"idx",C_IDX},{"idx2",C_IDX2},{"i32",C_I32},{"i64",C_I64},
        {"f32",C_F32},{"f64",C_F64},{"heap",C_HEAP},{"lane",C_LANE},{"v128",C_V128},
        {"memarg",C_MEMARG},{"memlane",C_MEMLANE},{"brtable",C_BRTABLE},{"selectt",C_SELECTT},
        {"broncast",C_BRONCAST},{"block",C_BLOCK},{"if",C_IF},{"trytable",C_TRYTABLE},
    };
    for (size_t i = 0; i < sizeof M/sizeof M[0]; i++)
        if (strcmp(s, M[i].n) == 0) return M[i].c;
    fprintf(stderr, "unknown shape '%s'\n", s); exit(2);
}

static void put_uleb(uint8_t *buf, size_t *n, uint64_t v) {
    do { uint8_t b = v & 0x7f; v >>= 7; if (v) b |= 0x80; buf[(*n)++] = b; } while (v);
}
static void put_imm(uint8_t *buf, size_t *n, icat_t cat) {
    switch (cat) {
        case C_NONE: break;
        case C_IDX:     buf[(*n)++]=0x00; break;
        case C_IDX2:    buf[(*n)++]=0x00; buf[(*n)++]=0x00; break;
        case C_I32:     buf[(*n)++]=0x00; break;
        case C_I64:     buf[(*n)++]=0x00; break;
        case C_F32:     memset(buf+*n,0,4); *n+=4; break;
        case C_F64:     memset(buf+*n,0,8); *n+=8; break;
        case C_HEAP:    buf[(*n)++]=0x00; break;
        case C_LANE:    buf[(*n)++]=0x00; break;
        case C_V128:    memset(buf+*n,0,16); *n+=16; break;
        case C_MEMARG:  buf[(*n)++]=0x00; buf[(*n)++]=0x00; break;
        case C_MEMLANE: buf[(*n)++]=0x00; buf[(*n)++]=0x00; buf[(*n)++]=0x00; break;
        case C_BRTABLE: buf[(*n)++]=0x00; buf[(*n)++]=0x00; break;
        case C_SELECTT: buf[(*n)++]=0x00; break;
        case C_BRONCAST:buf[(*n)++]=0x00; buf[(*n)++]=0x00; buf[(*n)++]=0x00; buf[(*n)++]=0x00; break;
        case C_BLOCK:   buf[(*n)++]=0x40; buf[(*n)++]=0x0B; break;
        case C_IF:      buf[(*n)++]=0x40; buf[(*n)++]=0x0B; break;
        case C_TRYTABLE:buf[(*n)++]=0x40; buf[(*n)++]=0x00; buf[(*n)++]=0x0B; break;
    }
}
static size_t build_instr(uint8_t *buf, const iop_t *op) {
    size_t n = 0;
    if (op->prefix == 0) buf[n++] = (uint8_t)op->code;
    else { buf[n++] = (uint8_t)op->prefix; put_uleb(buf, &n, (uint64_t)op->code); }
    put_imm(buf, &n, op->cat);
    return n;
}
static int roundtrip_one(const iop_t *op) {
    uint8_t in[64]; size_t n = build_instr(in, op);
    in[n++] = 0x0B;
    bbq_ctx_t c; bbq_ctx_init(&c, in, n);
    jav_expr_t e; memset(&e, 0, sizeof e);
    if (!jav_expr_read(&c, &e)) { jav_expr_free(&e); return 0; }
    int r = (c.pos == n && e.instrs.count == 1);
    if (r) {
        uint8_t first = (op->prefix == 0) ? (uint8_t)op->code : (uint8_t)op->prefix;
        if (e.instrs.items[0].op != first) r = 0;
        else {
            uint8_t out[64]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
            r = jav_expr_write(&w, &e) && w.pos == n && memcmp(in, out, n) == 0;
        }
    }
    jav_expr_free(&e);
    return r;
}
static int rejects(const uint8_t *b, size_t n) {
    bbq_ctx_t c; bbq_ctx_init(&c, b, n);
    jav_expr_t e; memset(&e, 0, sizeof e);
    int r = jav_expr_read(&c, &e) == false;
    jav_expr_free(&e);
    return r;
}

// ── Nested / structured coverage (non-empty bodies, else, catch, memidx, vecs) ──
static const uint8_t NESTED[] = {
    0x02,0x40, 0x41,0x01, 0x41,0x02, 0x6A, 0x0B,
    0x04,0x40, 0x01, 0x05, 0x1A, 0x0B,
    0x03,0x40, 0x0C,0x00, 0x0B,
    0x1F,0x40, 0x01, 0x02,0x00, 0x01, 0x0B,
    0x1C,0x01,0x7F,
    0x0E,0x02,0x00,0x01,0x02,
    0x28,0x43,0x01,0x08,
    0xFD,0x0C, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    0x0B,
};
static void test_nested(int *fails) {
    bbq_ctx_t c; bbq_ctx_init(&c, NESTED, sizeof NESTED);
    jav_expr_t e; memset(&e, 0, sizeof e);
    int ok = jav_expr_read(&c, &e) && c.pos == sizeof NESTED && e.instrs.count == 8;
    if (ok) {
        const jav_instr_t *I = e.instrs.items;
        ok = ok && I[0].op==0x02 && I[0].body.u.case_1.instrs.count==3;
        ok = ok && I[1].op==0x04 && I[1].body.u.case_2.else_body.has_value;
        ok = ok && I[3].op==0x1F && I[3].body.u.case_15.count==1 &&
                   I[3].body.u.case_15.catches.items[0].kind==0x02 &&
                   !I[3].body.u.case_15.catches.items[0].tag.has_value;
        ok = ok && I[4].op==0x1C && I[4].body.u.case_14.count==1;
        ok = ok && I[5].op==0x0E && I[5].body.u.case_6.count==2 &&
                   I[5].body.u.case_6.default_target==2;
        ok = ok && I[6].op==0x28 && I[6].body.u.case_17.memidx.has_value &&
                   I[6].body.u.case_17.memidx.value==1 && I[6].body.u.case_17.offset==8;
        ok = ok && I[7].op==0xFD && I[7].body.u.case_31.sub==12;
    }
    uint8_t out[128]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
    ok = ok && jav_expr_write(&w, &e) && w.pos == sizeof NESTED &&
         memcmp(out, NESTED, sizeof NESTED) == 0;
    printf("  nested/structured round-trip + structure          [%s]\n", ok ? "PASS" : "FAIL");
    if (!ok) (*fails)++;
    jav_expr_free(&e);
}

static char *slurp(const char *path, long *len) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)*len);
    if (fread(b, 1, (size_t)*len, f) != (size_t)*len) { perror("fread"); exit(2); }
    fclose(f); return b;
}

int main(int argc, char **argv) {
    const char *toml_path = argc > 1 ? argv[1] : "../spec/instructions.toml";
    long len; char *src = slurp(toml_path, &len);
    bbq_arena arena; bbq_arena_init(&arena, 1 << 16);
    toml_doc_t *doc = toml_parse(src, (int)len, &arena);
    if (toml_doc_has_errors(doc)) { fprintf(stderr, "instructions.toml parse error\n"); return 2; }
    const toml_val_t *arr = toml_tbl_get(toml_doc_root(doc), "instr");
    if (!arr || arr->type != TOML_VT_ARRAY) { fprintf(stderr, "no [[instr]] array\n"); return 2; }

    int fails = 0, ok = 0, bad = 0;
    for (int i = 0; i < arr->u.array.count; i++) {
        const toml_val_t *row = arr->u.array.items[i];
        if (row->type != TOML_VT_TABLE) { fprintf(stderr, "instr[%d] not a table\n", i); return 2; }
        const toml_tbl_t *t = row->u.table;
        const toml_val_t *opv = toml_tbl_get(t, "opcode");
        const toml_val_t *shv = toml_tbl_get(t, "shape");
        const char *sh; int64_t a, b;
        if (!opv || !toml_val_as_string(shv, &sh)) { fprintf(stderr, "instr[%d] missing fields\n", i); return 2; }
        iop_t op;
        if (opv->type == TOML_VT_INT) { op.prefix = 0; toml_val_as_int(opv, &a); op.code = (int)a; }
        else if (opv->type == TOML_VT_ARRAY && opv->u.array.count == 2 &&
                 toml_val_as_int(opv->u.array.items[0], &a) && toml_val_as_int(opv->u.array.items[1], &b)) {
            op.prefix = (int)a; op.code = (int)b;
        } else { fprintf(stderr, "instr[%d] bad opcode\n", i); return 2; }
        op.cat = shape_cat(sh);
        if (roundtrip_one(&op)) ok++;
        else { bad++; if (bad <= 12) printf("  FAIL op prefix=0x%02x code=%d shape=%s\n", op.prefix, op.code, sh); }
    }
    printf("  every-opcode decode->encode round-trip: %d/%d\n", ok, ok + bad);
    if (bad) fails++;

    struct { const char *name; uint8_t b[6]; size_t n; } neg[] = {
        {"reserved main 0x06",        {0x06,0x0B},            2},
        {"reserved main 0x27",        {0x27,0x0B},            2},
        {"reserved main 0xc5",        {0xC5,0x0B},            2},
        {"reserved 0xfc sub 18",      {0xFC,0x12,0x0B},       3},
        {"reserved 0xfd sub 154",     {0xFD,0x9A,0x01,0x0B},  4},
        {"reserved 0xfb sub 99",      {0xFB,0x63,0x0B},       3},
        {"misplaced else 0x05",       {0x05,0x0B},            2},
        {"unterminated expr (no end)",{0x01},                 1},
    };
    for (size_t i = 0; i < sizeof neg/sizeof neg[0]; i++) {
        int r = rejects(neg[i].b, neg[i].n);
        printf("  reject %-28s [%s]\n", neg[i].name, r ? "PASS" : "FAIL");
        if (!r) fails++;
    }
    test_nested(&fails);

    bbq_arena_free(&arena); free(src);
    printf("\nP2 instructions: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
