// test_sections.c — P3 gate for the module sections (§5.5). Builds one module
// byte-image exercising every section id (custom/type/import/function/table/
// memory/global/export/start/element/data/datacount/tag) — including a table with
// an init expr, element variants 0 & 1, data variants 0 & 1, all four limits-bearing
// types, and every externtype kind reachable here — then asserts:
//   (a) it parses and consumes the whole image,
//   (b) structural spot-checks per section,
//   (c) read∘write == identity (byte round-trip),
//   (d) fail-closed on an unknown section id, a bad limits flag, a bad import
//       kind, and an unknown element-segment flag.

#include "jav_reader.h"
#include "jav_writer.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
static void check(const char *n, int ok) { printf("  %-46s [%s]\n", n, ok?"PASS":"FAIL"); if(!ok) fails++; }

// Append a section [id][uleb size][payload] (payloads here are all < 128 bytes).
static void sec(uint8_t *m, size_t *n, uint8_t id, const uint8_t *p, size_t plen) {
    m[(*n)++] = id; m[(*n)++] = (uint8_t)plen;
    memcpy(m + *n, p, plen); *n += plen;
}

static const jav_section_t *find(const jav_module_t *mod, int tag) {
    for (size_t i = 0; i < mod->sections.count; i++)
        if (mod->sections.items[i].body.tag == tag) return &mod->sections.items[i];
    return NULL;
}

int main(void) {
    uint8_t m[512]; size_t n = 0;
    static const uint8_t pre[] = {0x00,0x61,0x73,0x6D, 0x01,0x00,0x00,0x00};
    memcpy(m, pre, sizeof pre); n = sizeof pre;

    sec(m,&n, 1,  (uint8_t[]){0x01, 0x60,0x00,0x00}, 4);              // type: func ()->()
    sec(m,&n, 2,  (uint8_t[]){0x01, 0x01,0x6D, 0x01,0x66, 0x00,0x00}, 7); // import "m"."f" func 0
    sec(m,&n, 3,  (uint8_t[]){0x01, 0x00}, 2);                        // function: typeidx 0
    sec(m,&n, 4,  (uint8_t[]){0x02, 0x70,0x00,0x01,                   // table: plain funcref {1..}
                              0x40,0x00, 0x70,0x00,0x01, 0xD0,0x70,0x0B}, 12); // init table + (ref.null func)
    sec(m,&n, 5,  (uint8_t[]){0x01, 0x01,0x01,0x02}, 4);              // memory: limits {1..2}
    sec(m,&n, 6,  (uint8_t[]){0x01, 0x7F,0x01, 0x41,0x00,0x0B}, 6);   // global: mut i32 = (i32.const 0)
    sec(m,&n, 7,  (uint8_t[]){0x01, 0x01,0x67, 0x03,0x00}, 5);        // export "g" global 0
    sec(m,&n, 8,  (uint8_t[]){0x00}, 1);                             // start: func 0
    sec(m,&n, 9,  (uint8_t[]){0x08,                                  // element: all 8 segment variants
        0x00, 0x41,0x00,0x0B, 0x01,0x00,                             //  0 active t0 funcidx
        0x01, 0x00, 0x01,0x00,                                       //  1 passive elemkind funcidx
        0x02, 0x00, 0x41,0x00,0x0B, 0x00, 0x01,0x00,                 //  2 active table elemkind funcidx
        0x03, 0x00, 0x01,0x00,                                       //  3 declarative elemkind funcidx
        0x04, 0x41,0x00,0x0B, 0x01, 0xD0,0x70,0x0B,                  //  4 active t0 expr-list
        0x05, 0x70, 0x01, 0xD0,0x70,0x0B,                            //  5 passive reftype expr-list
        0x06, 0x00, 0x41,0x00,0x0B, 0x70, 0x01, 0xD0,0x70,0x0B,      //  6 active table reftype expr-list
        0x07, 0x70, 0x01, 0xD0,0x70,0x0B}, 53);                      //  7 declarative reftype expr-list
    sec(m,&n, 12, (uint8_t[]){0x03}, 1);                             // datacount: 3
    sec(m,&n, 10, (uint8_t[]){0x01, 0x02, 0x00,0x0B}, 4);            // code: 1 body {no locals; end}
    sec(m,&n, 11, (uint8_t[]){0x03, 0x00, 0x41,0x00,0x0B, 0x02,0xAA,0xBB, // data0 active "\xAA\xBB"
                              0x01, 0x01,0xCC,                       // data1 passive "\xCC"
                              0x02, 0x00, 0x41,0x00,0x0B, 0x01,0xDD}, 18); // data2 active mem "\xDD"
    sec(m,&n, 13, (uint8_t[]){0x01, 0x00,0x00}, 3);                  // tag: attr 0, type 0
    sec(m,&n, 0,  (uint8_t[]){0x01,0x78, 0x59,0x5A}, 4);             // custom "x" data "YZ"

    bbq_ctx_t c; bbq_ctx_init(&c, m, n);
    jav_module_t mod; memset(&mod, 0, sizeof mod);
    check("module parses", jav_module_read(&c, &mod));
    check("consumed whole image", c.pos == n);
    bbq_ctx_free(&c);

    const jav_section_t *s;
    check("import: 1 import, func kind, typeidx 0",
          (s=find(&mod,2)) && s->body.u.case_2.count==1 &&
          s->body.u.case_2.imports.items[0].desc.kind==0x00);
    check("table: 2 (plain + init), init carries an expr",
          (s=find(&mod,4)) && s->body.u.case_4.count==2 &&
          s->body.u.case_4.tables.items[1].tag==0x40 /* TableInit */);
    check("memory: limits has max (flag 0x01)",
          (s=find(&mod,5)) && s->body.u.case_5.mems.items[0].limits.max.has_value);
    check("global: mut i32, init present",
          (s=find(&mod,6)) && s->body.u.case_6.globals.items[0].type.mut==1 &&
          s->body.u.case_6.globals.items[0].init.instrs.count==1);
    check("export: global 0", (s=find(&mod,7)) && s->body.u.case_7.exports.items[0].kind==0x03);
    check("start: func 0", (s=find(&mod,8)) && s->body.u.case_8.func==0);
    int elem_ok = (s=find(&mod,9)) && s->body.u.case_9.count==8;
    for (int i = 0; elem_ok && i < 8; i++) elem_ok = s->body.u.case_9.elems.items[i].flag==(uint32_t)i;
    check("element: all 8 segment variants (flags 0..7)", elem_ok);
    check("datacount: 3", (s=find(&mod,12)) && s->body.u.case_12.count==3);
    int data_ok = (s=find(&mod,11)) && s->body.u.case_11.count==3;
    for (int i = 0; data_ok && i < 3; i++) data_ok = s->body.u.case_11.datas.items[i].flag==(uint32_t)i;
    check("data: all 3 segment variants (flags 0..2)", data_ok);
    check("tag: 1 tag", (s=find(&mod,13)) && s->body.u.case_13.count==1);
    check("custom: name 'x', 2 data bytes",
          (s=find(&mod,0)) && s->body.u.case_0.name.bytes.length==1 &&
          s->body.u.case_0.name.bytes.data[0]=='x' && s->body.u.case_0.data.length==2);

    // (c) byte round-trip. Corrupt every STORED section size first, to prove the writer
    // COMPUTES @rest sizes (uleb backpatch) instead of echoing the struct. Writer is
    // growable; output is {w.data, w.pos}.
    for (size_t si = 0; si < mod.sections.count; si++) mod.sections.items[si].size = 0xDEADBEEF;
    bbq_write_ctx_t w; bbq_write_ctx_init_growable(&w, 512);
    int rt = jav_module_write(&w, &mod) && w.pos==n && memcmp(w.data, m, n)==0;
    bbq_write_ctx_free(&w);
    check("module round-trips byte-for-byte (@rest sizes computed, not stored)", rt);

    // (d) fail-closed negatives: a tiny module (preamble + one bad section).
    #define REJECT(name, ...) do { \
        uint8_t b[64]; size_t bn=sizeof pre; memcpy(b,pre,sizeof pre); \
        uint8_t pl[] = __VA_ARGS__; sec(b,&bn, pl[0], pl+1, sizeof pl-1); \
        bbq_ctx_t cc; bbq_ctx_init(&cc,b,bn); jav_module_t mm; memset(&mm,0,sizeof mm); \
        check("reject " name, jav_module_read(&cc,&mm)==false); \
        jav_module_free(&mm); bbq_ctx_free(&cc); } while(0)   /* free the partial tree from the rejected parse */
    REJECT("unknown section id 14", {14, 0x00});
    REJECT("bad limits flag 0x08",  {5, 0x01, 0x08,0x01});       // memory, flag bit3 illegal
    REJECT("bad import kind 0x05",  {2, 0x01, 0x00, 0x00, 0x05}); // import "" "" kind 5
    REJECT("unknown elem flag 8",   {9, 0x01, 0x08});            // element segment flag 8

    jav_module_free(&mod);
    printf("\nP3 sections: %s\n", fails==0 ? "ALL PASS" : "FAILURES");
    return fails ? 1 : 0;
}
