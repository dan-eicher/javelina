// test_wat.c — the .wat reader (pegc) must build the SAME jav_module_t the
// binary reader builds. The oracle is struct-equivalence: parse a module as
// text and as the equivalent hand-encoded bytes, then compare the structs
// field-by-field. (Byte round-trip via the writer is a separate gate, pending
// the writer's @rest size backpatch.) Grows with the grammar, slice by slice.

#include "wat_driver.h"      // wat_assemble: the shared two-pass assemble driver
#include "jav_reader.h"     // the binary reader (the oracle)
#include "jav_writer.h"     // jav_func_body_write: tree -> bytes (round-trip check)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Assemble a .wat string into a module via the shared driver (the SAME code path
// water and the conformance harness use); NULL + a diagnostic on a parse failure.
static jav_module_t* parse_text(const char* src) {
    int line = 0, col = 0;
    jav_module_t* m = wat_assemble(src, (int)strlen(src), "../spec/instructions.toml", &line, &col);
    if (!m) fprintf(stderr, "text parse failed at %d:%d\n", line, col);
    return m;
}

// Parse + free; returns 1 if the text parsed. For assertions that only check whether a
// form is accepted/rejected (no struct inspection), so the module doesn't leak.
static int parse_ok(const char* src) {
    jav_module_t* m = parse_text(src);
    if (m) { jav_module_free(m); free(m); return 1; }
    return 0;
}

static jav_module_t read_bin(const uint8_t* b, size_t n) {
    bbq_ctx_t c; bbq_ctx_init(&c, b, n);
    jav_module_t m; memset(&m, 0, sizeof m);
    assert(jav_module_read(&c, &m) && bbq_at_end(&c));
    bbq_ctx_free(&c);
    return m;
}

static const jav_section_t* find_sec(const jav_module_t* m, int id) {
    for (size_t i = 0; i < m->sections.count; i++)
        if (m->sections.items[i].id == (uint8_t)id) return &m->sections.items[i];
    return NULL;
}

// Parse `(module (func <inner>))`, re-serialize the code entry's FuncBody, and
// check it equals the expected body bytes (locals vec + instr encodings + 0x0B).
// Full text→tree→bytes round-trip — verifies the instruction's immediate emission.
static int body_roundtrips(const char* inner, const uint8_t* expect, size_t en) {
    char src[256]; snprintf(src, sizeof src, "(module (func %s))", inner);
    jav_module_t* m = parse_text(src);
    if (!m) return 0;
    const jav_section_t* cs = find_sec(m, 10);
    int ok = cs && cs->body.u.case_10.entries.count == 1;
    if (ok) {
        uint8_t out[256]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
        bbq_write_set_endian(&w, true);
        ok = jav_func_body_write(&w, &cs->body.u.case_10.entries.items[0].body)
             && w.pos == en && memcmp(out, expect, en) == 0;
    }
    jav_module_free(m); free(m);
    return ok;
}

// Re-serialize one code entry's FuncBody (for multi-func modules; body_roundtrips
// only covers the single-func case).
static int body_bytes_eq(const jav_module_t* m, size_t entry, const uint8_t* expect, size_t en) {
    const jav_section_t* cs = find_sec(m, 10);
    if (!cs || entry >= cs->body.u.case_10.entries.count) return 0;
    uint8_t out[256]; bbq_write_ctx_t w; bbq_write_ctx_init(&w, out, sizeof out);
    bbq_write_set_endian(&w, true);
    return jav_func_body_write(&w, &cs->body.u.case_10.entries.items[entry].body)
           && w.pos == en && memcmp(out, expect, en) == 0;
}

// $id resolution + §6.4.15 typeuse: forward refs, (type $x) reuse, insert ordering.
static void test_idresolve(void) {
    {   // §6.3.5 quoted identifiers $"..." + name equivalence: an id's NAME is its idchars
        // or the decoded quoted string, so $fh ≡ $"fh", and escapes decode ($"\41B" = "AB").
        assert(parse_ok("(module (func $fh) (func (call $\"fh\")))") && "$fh referenced as $\"fh\"");
        assert(parse_ok("(module (func $\"fi\") (func (call $fi)))") && "$\"fi\" referenced as $fi");
        assert(parse_ok("(module (func $\"\\41B\") (func (call $\"AB\") (call $\"\\41\\42\")))")
               && "escape-decoded names equal (\\41B == AB)");
        assert(parse_ok("(module (func $\" a b \") (func (call $\" a b \")))") && "quoted id with spaces");
        assert(!parse_ok("(module (func (call $\"b\")))") && "distinct names stay unresolved");
        printf("OK  test_wat: quoted identifiers + name equivalence (§6.3.5)\n");
    }
    // Forward $id func reference: $a (func 0) calls $b (func 1, defined AFTER it).
    // Two-pass resolution: pass 1 binds $a,$b; pass 2 resolves call $b -> 1.
    {
        jav_module_t* m = parse_text("(module (func $a call $b) (func $b))");
        assert(m && "forward call $id parse");
        static const uint8_t b0[] = {0x00, 0x10, 0x01, 0x0b};   // locals0, call funcidx=1, end
        static const uint8_t b1[] = {0x00, 0x0b};
        assert(body_bytes_eq(m, 0, b0, sizeof b0) && "call $b resolved to funcidx 1");
        assert(body_bytes_eq(m, 1, b1, sizeof b1));
        printf("OK  test_wat: forward $id func ref (call $b -> funcidx 1)\n");
        jav_module_free(m); free(m);
    }
    // (type $t) typeuse: the func reuses $t (type 0); no fresh type is inserted.
    {
        jav_module_t* m = parse_text(
            "(module (type $t (func (param i32) (result i32))) (func (type $t)))");
        assert(m && "(type $t) parse");
        const jav_section_t* ts = find_sec(m, 1);
        const jav_section_t* fs = find_sec(m, 3);
        assert(ts && ts->body.u.case_1.count == 1 && "only the explicit type exists");
        assert(fs && fs->body.u.case_3.type_indices.items[0] == 0 && "func uses type 0");
        printf("OK  test_wat: (type $t) typeuse ref (no fresh type)\n");
        jav_module_free(m); free(m);
    }
    // §6.4.15 ordering: an inline-signature func BEFORE an explicit (type) def. The
    // explicit type keeps index 0; the inserted fresh type goes AFTER it at index 1.
    {
        jav_module_t* m = parse_text("(module (func (param i32)) (type $t (func)))");
        assert(m && "insert-ordering parse");
        const jav_section_t* ts = find_sec(m, 1);
        const jav_section_t* fs = find_sec(m, 3);
        assert(ts && ts->body.u.case_1.count == 2 && "explicit + inserted");
        const jav_rec_type_t* t = ts->body.u.case_1.types.items;
        assert(t[0].body.u.case_5.param_count == 0 && "explicit (type $t) first at 0");
        assert(t[1].body.u.case_5.param_count == 1 && "inserted fresh type after at 1");
        assert(fs->body.u.case_3.type_indices.items[0] == 1 && "func -> inserted type 1");
        printf("OK  test_wat: typeuse insert ordering (explicit 0, inserted 1)\n");
        jav_module_free(m); free(m);
    }
    // An unbound $id is a clean failure, not a silent 0.
    {
        jav_module_t* m = parse_text("(module (func call $nope))");
        assert(!m && "unbound $id rejected");
        printf("OK  test_wat: unbound $id rejected (not silent 0)\n");
    }
    // §6.4.6 multi-valtype abbreviation: (param i32 i64) = two params, etc.
    {
        jav_module_t* m = parse_text("(module (func (param i32 i64) (result i32 i64)))");
        assert(m && "multi-valtype param/result parse");
        const jav_section_t* ts = find_sec(m, 1);
        const jav_func_type_t* ft = &ts->body.u.case_1.types.items[0].body.u.case_5;
        assert(ft->param_count == 2 && ft->result_count == 2 && "two params, two results");
        printf("OK  test_wat: multi-valtype (param i32 i64) / (result i32 i64)\n");
        jav_module_free(m); free(m);
    }
    // §6.4.15 dedup: two funcs with the same inline signature share ONE type.
    {
        jav_module_t* m = parse_text(
            "(module (func (param i32) (result i32)) (func (param i32) (result i32)))");
        assert(m && "dedup parse");
        const jav_section_t* ts = find_sec(m, 1);
        const jav_section_t* fs = find_sec(m, 3);
        assert(ts && ts->body.u.case_1.count == 1 && "one shared type");
        assert(fs->body.u.case_3.type_indices.items[0] == 0 &&
               fs->body.u.case_3.type_indices.items[1] == 0 && "both funcs -> type 0");
        printf("OK  test_wat: inline typeuse dedup (2 funcs -> 1 type)\n");
        jav_module_free(m); free(m);
    }
    // §6.4.15 "smallest existing index" spans the FULL type set: an inline sig matches
    // an explicit (type) def even when that def appears textually AFTER the func.
    {
        jav_module_t* m = parse_text("(module (func (param i32)) (type $t (func (param i32))))");
        assert(m && "match-later-type parse");
        const jav_section_t* ts = find_sec(m, 1);
        const jav_section_t* fs = find_sec(m, 3);
        assert(ts && ts->body.u.case_1.count == 1 && "reuses $t, no fresh type");
        assert(fs->body.u.case_3.type_indices.items[0] == 0 && "func -> type 0 ($t)");
        printf("OK  test_wat: typeuse matches a later-defined explicit type\n");
        jav_module_free(m); free(m);
    }
}

// Structured ops: nesting, relative $label resolution, else, the 3 blocktype forms,
// and try_table catches (incl. the catch-label-resolves-in-OUTER-scope rule).
static void test_structured(void) {
    static const uint8_t b_blk[]  = {0x00, 0x02,0x40, 0x0b, 0x0b};                  // block (empty) end
    static const uint8_t b_blkr[] = {0x00, 0x02,0x7f, 0x0b, 0x0b};                  // block (result i32) end
    static const uint8_t b_nest[] = {0x00, 0x02,0x40, 0x02,0x40, 0x0c,0x01, 0x0b,0x0b, 0x0b}; // br $a = rel 1
    static const uint8_t b_if[]   = {0x00, 0x04,0x7f, 0x41,0x01, 0x05, 0x41,0x02, 0x0b, 0x0b}; // if/else
    static const uint8_t b_mv[]   = {0x00, 0x02,0x00, 0x0b, 0x0b};                  // multivalue bt -> typeidx 0
    static const uint8_t b_tt[]   = {0x00, 0x1f,0x40, 0x01, 0x02,0x00, 0x0b, 0x0b}; // try_table (catch_all 0)
    static const uint8_t b_ttl[]  = {0x00, 0x02,0x40, 0x1f,0x40, 0x01,0x02,0x00, 0x0b, 0x0b, 0x0b}; // catch $outer
    assert(body_roundtrips("block end", b_blk, sizeof b_blk));
    assert(body_roundtrips("block (result i32) end", b_blkr, sizeof b_blkr));
    assert(body_roundtrips("block $a block $b br $a end end", b_nest, sizeof b_nest));
    assert(body_roundtrips("if (result i32) i32.const 1 else i32.const 2 end", b_if, sizeof b_if));
    assert(body_roundtrips("block (param i32) (result i32 i64) end", b_mv, sizeof b_mv));
    assert(body_roundtrips("try_table (catch_all 0) end", b_tt, sizeof b_tt));
    assert(body_roundtrips("block $b try_table (catch_all $b) end end", b_ttl, sizeof b_ttl));
    // §6.5 br_on_cast resolves a SYMBOLIC label like br/br_table (was num-only): castflags
    // (null bits of the two reftypes) + labelidx + ht1 + ht2. (ref null any)=nullable any(-18→0x6e),
    // (ref i31)=non-null i31(-20→0x6c) → flags 0b01. $l = innermost = 0.
    static const uint8_t b_boc[] = {0x00, 0x02,0x40, 0xfb,0x18, 0x01, 0x00, 0x6e, 0x6c, 0x0b, 0x0b};
    assert(body_roundtrips("block $l br_on_cast $l (ref null any) (ref i31) end", b_boc, sizeof b_boc));
    printf("OK  test_wat: structured ops (block/loop/if/try_table, labels, blocktype, catch, br_on_cast $l)\n");
    {   // §5.3.3 a concrete (ref null typeidx) blocktype is TWO LEBs: 0x63 (ref null) + the
        // heaptype sleb. Decoding bt as a bare sleb64 dropped the heaptype — for typeidx 0/1
        // it was silently mis-read as unreachable/nop, for ≥2 (0x02=block) decoding broke.
        jav_module_t* m = parse_text(
            "(module (type $a (func)) (type $b (func)) (type $c (func))"
            " (func (block (result (ref null $c)) unreachable)))");
        assert(m && "ref-typeidx blocktype parse");
        static const uint8_t b[] = {0x00, 0x02, 0x63, 0x02, 0x00, 0x0b, 0x0b};  // block (ref null 2) unreachable end
        assert(body_bytes_eq(m, 0, b, sizeof b) && "block bt = ref null typeidx 2");
        jav_module_free(m); free(m);
        printf("OK  test_wat: blocktype (ref null typeidx) two-LEB decode (§5.3.3)\n");
    }
}

// Module fields: strings (§6.3.4 incl. escapes), export (name + kind + $id), start,
// and binary section ordering independent of text order.
static void test_module_fields(void) {
    {   // §6.6.13 named module: (module id? modulefield*) — the id binds at script level,
        // carries no struct field, but MUST parse (and not collide with a leading field).
        jav_module_t* m = parse_text("(module $m (func $f) (start $f))");
        assert(m && "named module parse");
        assert(find_sec(m, 8) && find_sec(m, 8)->body.u.case_8.func == 0 && "named module body intact");
        jav_module_free(m); free(m);
        jav_module_t* e = parse_text("(module $only)");      // id with no fields
        assert(e && "named empty module parse");
        jav_module_free(e); free(e);
        printf("OK  test_wat: named module (§6.6.13 id?)\n");
    }
    {   // start section: (start 0)
        jav_module_t* m = parse_text("(module (func) (start 0))");
        assert(m && "start parse");
        const jav_section_t* ss = find_sec(m, 8);
        assert(ss && ss->body.u.case_8.func == 0 && "start -> func 0");
        printf("OK  test_wat: start section\n");
        jav_module_free(m); free(m);
    }
    {   // export of a func by $id (forward-resolved)
        jav_module_t* m = parse_text("(module (func $f) (export \"foo\" (func $f)))");
        assert(m && "export parse");
        const jav_section_t* es = find_sec(m, 7);
        assert(es && es->body.u.case_7.count == 1);
        const jav_export_t* e = &es->body.u.case_7.exports.items[0];
        assert(e->kind == 0 && e->idx == 0 && "export func 0");
        assert(e->name.count == 3 && !memcmp(e->name.bytes.data, "foo", 3) && "name foo");
        printf("OK  test_wat: export (func $id) + name\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 inline export on definitions: (func (export "n") …), index assigned in order
        jav_module_t* m = parse_text("(module (func $f (export \"main\")) (func (export \"other\")))");
        assert(m && "inline export parse");
        const jav_export_section_t* es = &find_sec(m, 7)->body.u.case_7;
        assert(es->count == 2);
        assert(es->exports.items[0].kind == 0 && es->exports.items[0].idx == 0
               && es->exports.items[0].name.count == 4 && !memcmp(es->exports.items[0].name.bytes.data, "main", 4));
        assert(es->exports.items[1].kind == 0 && es->exports.items[1].idx == 1
               && !memcmp(es->exports.items[1].name.bytes.data, "other", 5));
        printf("OK  test_wat: inline export on def\n");
        jav_module_free(m); free(m);
    }
    {   // inline export index counts preceding imports (imported func 0, defined func 1)
        jav_module_t* m = parse_text("(module (import \"m\" \"n\" (func)) (func (export \"e\")))");
        assert(m && "inline export + import parse");
        const jav_export_t* e = &find_sec(m, 7)->body.u.case_7.exports.items[0];
        assert(e->kind == 0 && e->idx == 1 && "exported func index = 1 (after 1 import)");
        printf("OK  test_wat: inline export index counts imports\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 inline import on a func def: the def becomes an import (func index 0);
        // a later defined func's `call $f` resolves to that import index.
        jav_module_t* m = parse_text(
            "(module (func $f (import \"m\" \"n\") (param i32)) (func call $f))");
        assert(m && "inline import on func parse");
        const jav_section_t* isec = find_sec(m, 2);
        assert(isec && isec->body.u.case_2.count == 1 && "one func import");
        const jav_import_t* im = &isec->body.u.case_2.imports.items[0];
        assert(im->desc.kind == 0 && "func import");
        assert(im->module.count == 1 && im->module.bytes.data[0] == 'm');
        assert(im->field.count == 1 && im->field.bytes.data[0] == 'n');
        static const uint8_t b[] = {0x00, 0x10, 0x00, 0x0b};   // locals0, call funcidx=0, end
        assert(body_bytes_eq(m, 0, b, sizeof b) && "call $f -> import funcidx 0");
        printf("OK  test_wat: inline import on func def\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 inline import + export on one func def: an import that is also exported.
        jav_module_t* m = parse_text("(module (func (export \"e\") (import \"m\" \"n\")))");
        assert(m && "inline import+export combo parse");
        const jav_section_t* isec = find_sec(m, 2);
        assert(isec && isec->body.u.case_2.count == 1 && "import recorded");
        const jav_export_t* e = &find_sec(m, 7)->body.u.case_7.exports.items[0];
        assert(e->kind == 0 && e->idx == 0 && e->name.count == 1 && e->name.bytes.data[0] == 'e'
               && "export targets the import (func index 0)");
        printf("OK  test_wat: inline import + export combo\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 inline import on memory + global: each def becomes a typed import.
        jav_module_t* m = parse_text(
            "(module (memory (import \"m\" \"mem\") 1 2) (global (import \"m\" \"g\") (mut i32)))");
        assert(m && "inline import on mem+global parse");
        const jav_section_t* isec = find_sec(m, 2);
        assert(isec && isec->body.u.case_2.count == 2 && "two imports in source order");
        const jav_import_t* im0 = &isec->body.u.case_2.imports.items[0];
        const jav_import_t* im1 = &isec->body.u.case_2.imports.items[1];
        assert(im0->desc.kind == 2 && im0->desc.body.u.case_2.min == 1
               && im0->desc.body.u.case_2.max.has_value && im0->desc.body.u.case_2.max.value == 2
               && "memory import limits");
        assert(im1->desc.kind == 3 && im1->desc.body.u.case_3.type.head == 0x7f
               && im1->desc.body.u.case_3.mut == 1 && "global import (mut i32)");
        assert(!find_sec(m, 5) && !find_sec(m, 6) && "no mem/global def sections");
        printf("OK  test_wat: inline import on memory + global\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 inline import on table + tag (the remaining extern kinds).
        jav_module_t* m = parse_text(
            "(module (table (import \"m\" \"t\") 1 funcref) (tag (import \"m\" \"x\") (param i32)))");
        assert(m && "inline import on table+tag parse");
        const jav_section_t* isec = find_sec(m, 2);
        assert(isec && isec->body.u.case_2.count == 2);
        const jav_import_t* im0 = &isec->body.u.case_2.imports.items[0];
        const jav_import_t* im1 = &isec->body.u.case_2.imports.items[1];
        assert(im0->desc.kind == 1 && im0->desc.body.u.case_1.reftype.head == 0x70
               && im0->desc.body.u.case_1.limits.min == 1 && "table import (funcref, min 1)");
        assert(im1->desc.kind == 4 && "tag import");
        assert(!find_sec(m, 4) && !find_sec(m, 13) && "no table/tag def sections");
        printf("OK  test_wat: inline import on table + tag\n");
        jav_module_free(m); free(m);
    }
    {   // §6.3.4 string escapes: \6c (hex byte 'l'), \n (0x0a)
        jav_module_t* m = parse_text("(module (func) (export \"a\\6cb\\n\" (func 0)))");
        assert(m && "escape parse");
        const jav_export_t* e = &find_sec(m, 7)->body.u.case_7.exports.items[0];
        static const uint8_t want[] = {0x61, 0x6c, 0x62, 0x0a};
        assert(e->name.count == 4 && !memcmp(e->name.bytes.data, want, 4) && "decoded escapes");
        printf("OK  test_wat: string escapes (\\HH, \\n)\n");
        jav_module_free(m); free(m);
    }
    {   // sections emitted in binary id order regardless of text order
        jav_module_t* m = parse_text("(module (export \"f\" (func $f)) (start $f) (func $f))");
        assert(m && "order parse");
        uint8_t ids[8]; int n = 0;
        for (size_t i = 0; i < m->sections.count; i++) ids[n++] = m->sections.items[i].id;
        assert(n == 5 && ids[0]==1 && ids[1]==3 && ids[2]==7 && ids[3]==8 && ids[4]==10
               && "binary section order: type,function,export,start,code");
        printf("OK  test_wat: sections in binary order (text order independent)\n");
        jav_module_free(m); free(m);
    }
    {   // memory + limits (min, max -> flag bit0)
        jav_module_t* m = parse_text("(module (memory 1 2))");
        assert(m && "memory parse");
        const jav_mem_entry_t* me = &find_sec(m, 5)->body.u.case_5.mems.items[0];
        assert(me->limits.min == 1 && me->limits.max.has_value && me->limits.max.value == 2
               && me->limits.flag == 1 && "limits min/max/flag");
        printf("OK  test_wat: memory section + limits\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6.5 inline-data memory abbreviation: (memory id? addrtype? (data b*)) ≡
        //   (memory id? m m) + (data (memory idx) (i32.const 0) b*), m = ceil(|b|/65536).
        //   Explicit (memory idx) ref → active variant 2, matching ElementField/TableField convention.
        jav_module_t* m = parse_text("(module (memory (data \"abc\")))");
        assert(m && "inline-data memory parse");
        const jav_mem_entry_t* me = &find_sec(m, 5)->body.u.case_5.mems.items[0];
        assert(me->limits.min == 1 && me->limits.max.has_value && me->limits.max.value == 1
               && me->limits.flag == 1 && "inline-data: min=max=ceil(3/65536)=1, fixed");
        const jav_data_t* d = &find_sec(m, 11)->body.u.case_11.datas.items[0];
        assert(d->flag == 2 && d->body.tag == 2 && d->body.u.case_2.memidx == 0 && "active explicit variant 2");
        assert(d->body.u.case_2.offset.instrs.items[0].op == 0x41 && "offset i32.const 0");
        assert(d->body.u.case_2.data.count == 3 && d->body.u.case_2.data.bytes.data[0] == 'a' && "data bytes");
        jav_module_free(m); free(m);

        // empty inline data → 0 pages, 0-byte segment.
        jav_module_t* e = parse_text("(module (memory (data)))");
        assert(e && "empty inline-data parse");
        const jav_mem_entry_t* em = &find_sec(e, 5)->body.u.case_5.mems.items[0];
        assert(em->limits.min == 0 && em->limits.max.value == 0 && "empty: 0 pages");
        assert(find_sec(e, 11)->body.u.case_11.datas.items[0].body.u.case_2.data.count == 0 && "0 bytes");
        jav_module_free(e); free(e);

        // i64 addrtype + $id: (memory $m i64 (data …)) sets the 64-bit limits flag bit.
        jav_module_t* p1 = parse_text("(module (memory $m i64 (data \"x\")))");
        assert(p1 && "i64 inline-data parse");
        assert((find_sec(p1, 5)->body.u.case_5.mems.items[0].limits.flag & 4) && "i64 addrtype flag bit 2");
        jav_module_free(p1); free(p1);
        printf("OK  test_wat: inline-data memory (§6.6.5)\n");
    }
    {   // global: (mut i32) + linear init expr
        jav_module_t* m = parse_text("(module (global $g (mut i32) i32.const 7))");
        assert(m && "global parse");
        const jav_global_t* g = &find_sec(m, 6)->body.u.case_6.globals.items[0];
        assert(g->type.type.head == 0x7f && g->type.mut == 1 && "globaltype (mut i32)");
        assert(g->init.instrs.count == 1 && g->init.instrs.items[0].op == 0x41
               && g->init.end == 0x0b && "init expr i32.const");
        printf("OK  test_wat: global section (mut + init expr)\n");
        jav_module_free(m); free(m);
    }
    {   // tag: typeuse -> an inserted type (attr 0 = exception)
        jav_module_t* m = parse_text("(module (tag (param i32)))");
        assert(m && "tag parse");
        const jav_tag_type_t* t = &find_sec(m, 13)->body.u.case_13.tags.items[0];
        assert(t->attr == 0 && t->type == 0 && "tag attr/type");
        printf("OK  test_wat: tag section (typeuse)\n");
        jav_module_free(m); free(m);
    }
    {   // ValType breadth: v128 + reference types in a signature
        jav_module_t* m = parse_text("(module (func (param v128 funcref externref)))");
        assert(m && "valtype-breadth parse");
        const jav_func_type_t* ft = &find_sec(m, 1)->body.u.case_1.types.items[0].body.u.case_5;
        assert(ft->param_count == 3 && ft->params.items[0].head == 0x7b
               && ft->params.items[1].head == 0x70 && ft->params.items[2].head == 0x6f
               && "v128 / funcref / externref");
        printf("OK  test_wat: ValType breadth (v128, funcref, externref)\n");
        jav_module_free(m); free(m);
    }
    {   // table (plain): limits + reftype element
        jav_module_t* m = parse_text("(module (table 1 2 funcref))");
        assert(m && "table parse");
        const jav_table_t* tb = &find_sec(m, 4)->body.u.case_4.tables.items[0];
        assert(tb->tag == jav_table_default_val);
        assert(tb->u.default_val.type.limits.min == 1 && tb->u.default_val.type.limits.max.value == 2);
        assert(tb->u.default_val.type.reftype.head == 0x70 && "funcref element");
        printf("OK  test_wat: table section (limits + reftype)\n");
        jav_module_free(m); free(m);
    }
    /* elem segments (inline-table + all variants + forms) live in test_elements() — §6.6.9/6 */
    {   // §6.5.5/§6.5.6 bulk memory/table ops: memidx/tableidx are OPTIONAL (default 0),
        // and the `init` ops reverse text vs binary operand order. (The biggest .wat corpus gap.)
        jav_module_t* m = parse_text(
            "(module (memory 1) (memory 1) (data \"x\")"
            " (func i32.const 0 i32.const 8 i32.const 4 memory.copy)"
            " (func i32.const 0 i32.const 0 i32.const 1 memory.init 1 0))");
        assert(m && "bulk mem ops parse");
        // memory.copy with no indices → FC 0A 00 00 (both memidx default 0)
        static const uint8_t b0[] = {0x00, 0x41,0x00, 0x41,0x08, 0x41,0x04, 0xFC,0x0A,0x00,0x00, 0x0B};
        assert(body_bytes_eq(m, 0, b0, sizeof b0) && "memory.copy implied memidx 0,0");
        // memory.init 1 0: text [memidx=1, dataidx=0] → binary [dataidx=0, memidx=1] = FC 08 00 01
        static const uint8_t b1[] = {0x00, 0x41,0x00, 0x41,0x00, 0x41,0x01, 0xFC,0x08,0x00,0x01, 0x0B};
        assert(body_bytes_eq(m, 1, b1, sizeof b1) && "memory.init text→binary reversal");
        printf("OK  test_wat: bulk mem ops (implied memidx 0; init text/binary reversal)\n");
        jav_module_free(m); free(m);
    }
    {   // table.copy with no indices → FC 0E 00 00; table.init <elem> → tableidx default 0.
        jav_module_t* m = parse_text(
            "(module (table 1 funcref) (func $f) (elem func $f)"
            " (func i32.const 0 i32.const 0 i32.const 0 table.copy)"
            " (func i32.const 0 i32.const 0 i32.const 0 table.init 0))");
        assert(m && "bulk table ops parse");
        static const uint8_t b0[] = {0x00, 0x41,0x00, 0x41,0x00, 0x41,0x00, 0xFC,0x0E,0x00,0x00, 0x0B};
        assert(body_bytes_eq(m, 1, b0, sizeof b0) && "table.copy implied tableidx 0,0");
        // table.init 0: text [elemidx=0] → binary [elemidx=0, tableidx=0] = FC 0C 00 00
        static const uint8_t b1[] = {0x00, 0x41,0x00, 0x41,0x00, 0x41,0x00, 0xFC,0x0C,0x00,0x00, 0x0B};
        assert(body_bytes_eq(m, 2, b1, sizeof b1) && "table.init implied tableidx 0");
        printf("OK  test_wat: bulk table ops (implied tableidx 0)\n");
        jav_module_free(m); free(m);
    }
    {   // §5.3.4 reference type with a CONCRETE typeidx heaptype given by $id: `(ref null $t)`.
        // HeapType must resolve a $id against the type space (not just numeric). (linking/GC gap.)
        jav_module_t* m = parse_text("(module (type $t (func)) (global (ref null $t) (ref.null $t)))");
        assert(m && "(ref null $t) parse");
        const jav_global_t* g = &find_sec(m, 6)->body.u.case_6.globals.items[0];
        assert(g->type.type.head == 0x63 && "ref null");                 // 0x63 = (ref null ht)
        assert(g->type.type.ht.has_value && g->type.type.ht.value.x == 0 && "heaptype = typeidx $t (0)");
        printf("OK  test_wat: (ref null $t) concrete-typeidx heaptype\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6.6 table with an explicit init expr: `(table tt expr)` → the (0x40 0x00) init form
        // (not the abbreviated default-ref.null form). NOTE: added retroactively (fix already landed).
        jav_module_t* m = parse_text("(module (table 1 funcref (ref.null func)))");
        assert(m && "table init-expr parse");
        const jav_table_t* t = &find_sec(m, 4)->body.u.case_4.tables.items[0];
        assert(t->tag == jav_table_case_0 && "init form (0x40 0x00)");
        assert(t->u.case_0.marker0 == 0x40 && t->u.case_0.marker1 == 0x00);
        assert(t->u.case_0.type.reftype.head == 0x70 && t->u.case_0.type.limits.min == 1);
        assert(t->u.case_0.init.instrs.count == 1 && t->u.case_0.init.instrs.items[0].op == 0xD0 && "init = ref.null func");
        printf("OK  test_wat: table explicit init expr (0x40 0x00 form)\n");
        jav_module_free(m); free(m);
    }
    {   // data: passive + active (linear offset)
        jav_module_t* m = parse_text("(module (data \"abc\") (data (offset i32.const 0) \"x\"))");
        assert(m && "data parse");
        const jav_data_section_t* ds = &find_sec(m, 11)->body.u.case_11;
        assert(ds->count == 2);
        const jav_data_t* d0 = &ds->datas.items[0];
        assert(d0->flag == 1 && d0->body.tag == 1 && d0->body.u.case_1.data.count == 3
               && !memcmp(d0->body.u.case_1.data.bytes.data, "abc", 3) && "passive data");
        const jav_data_t* d1 = &ds->datas.items[1];
        assert(d1->flag == 0 && d1->body.tag == 0
               && d1->body.u.case_0.offset.instrs.items[0].op == 0x41
               && d1->body.u.case_0.data.count == 1 && "active data mem0");
        printf("OK  test_wat: data section (passive + active)\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6 bare-folded-offset abbreviation: (data (i32.const 7) "y") == (offset …)
        jav_module_t* m = parse_text("(module (data (i32.const 7) \"y\"))");
        assert(m && "bare-folded-offset parse");
        const jav_data_t* d = &find_sec(m, 11)->body.u.case_11.datas.items[0];
        assert(d->flag == 0 && d->body.tag == 0 && d->body.u.case_0.offset.instrs.count == 1
               && d->body.u.case_0.offset.instrs.items[0].op == 0x41 && "bare offset -> active mem0");
        printf("OK  test_wat: bare folded offset (data (i32.const N) …)\n");
        jav_module_free(m); free(m);
    }
    {   // import: §6.6 imports-first indexing — $g (imported) = 0, $h (defined) = 1
        jav_module_t* m = parse_text(
            "(module (import \"m\" \"f\" (func $g)) (func $h call $g) (func call $h))");
        assert(m && "import parse");
        const jav_section_t* is = find_sec(m, 2);
        assert(is && is->body.u.case_2.count == 1);
        const jav_import_t* im = &is->body.u.case_2.imports.items[0];
        assert(im->desc.kind == 0 && "func import");
        assert(im->module.count == 1 && im->module.bytes.data[0] == 'm');
        assert(im->field.count == 1 && im->field.bytes.data[0] == 'f');
        static const uint8_t bh[] = {0x00, 0x10, 0x00, 0x0b};   // $h: call $g  -> import idx 0
        static const uint8_t bx[] = {0x00, 0x10, 0x01, 0x0b};   // 3rd: call $h -> def idx 1
        assert(body_bytes_eq(m, 0, bh, sizeof bh) && "call imported func -> 0");
        assert(body_bytes_eq(m, 1, bx, sizeof bx) && "call defined func -> 1 (after import)");
        printf("OK  test_wat: import (imports-first indexing: import 0, def 1)\n");
        jav_module_free(m); free(m);
    }
    {   // multi-memory memarg: a non-zero memidx sets align bit 6 + emits the memidx
        jav_module_t* m = parse_text("(module (memory 1) (memory 1) (func i32.load 1 offset=0))");
        assert(m && "multi-memory memarg parse");
        static const uint8_t b[] = {0x00, 0x28, 0x42, 0x01, 0x00, 0x0b};   // align=2|0x40, memidx 1, offset 0
        assert(body_bytes_eq(m, 0, b, sizeof b) && "memarg memidx 1");
        printf("OK  test_wat: multi-memory memarg (align bit 6 + memidx)\n");
        jav_module_free(m); free(m);
    }
    {   // §5.4.5 the memarg offset is a u64 (memory64): an offset ≥ 2^32 must encode AND
        // re-decode (the reader read it as u32, truncating/rejecting anything past 2^32).
        jav_module_t* m = parse_text("(module (func i32.const 0 i32.load offset=0x1_0000_0000 drop))");
        assert(m && "u64 memarg offset parse");
        // i32.const 0 | i32.load align=2 offset=2^32 (uleb 80 80 80 80 10) | drop | end
        static const uint8_t b[] = {0x00, 0x41,0x00, 0x28, 0x02, 0x80,0x80,0x80,0x80,0x10, 0x1a, 0x0b};
        assert(body_bytes_eq(m, 0, b, sizeof b) && "offset 2^32 round-trips as u64 uleb");
        jav_module_free(m); free(m);
        printf("OK  test_wat: memarg u64 offset (≥2^32, §5.4.5)\n");
    }
}

// §6.4.6 GC composite types: struct, array (packed + mut), sub (with supertype), rec group.
static void test_gctypes(void) {
    {   // struct with a const i32 field and a mut i64 field
        jav_module_t* m = parse_text("(module (type (struct (field i32) (field (mut i64)))))");
        assert(m && "struct parse");
        const jav_rec_type_t* rt = &find_sec(m, 1)->body.u.case_1.types.items[0];
        assert(rt->head == 0x5F && rt->body.tag == 0x5F);
        const jav_struct_type_t* s = &rt->body.u.case_4;
        assert(s->field_count == 2);
        assert(s->fields.items[0].storage.head == 0x7f && s->fields.items[0].mut == 0);
        assert(s->fields.items[1].storage.head == 0x7e && s->fields.items[1].mut == 1);
        printf("OK  test_wat: GC struct type (fields + mut)\n");
        jav_module_free(m); free(m);
    }
    {   // array of mutable packed i8
        jav_module_t* m = parse_text("(module (type (array (mut i8))))");
        assert(m && "array parse");
        const jav_rec_type_t* rt = &find_sec(m, 1)->body.u.case_1.types.items[0];
        assert(rt->head == 0x5E && rt->body.u.case_3.field.storage.head == 0x78
               && rt->body.u.case_3.field.mut == 1);
        printf("OK  test_wat: GC array type (packed i8, mut)\n");
        jav_module_free(m); free(m);
    }
    {   // sub type with a supertype reference
        jav_module_t* m = parse_text("(module (type $b (func)) (type (sub $b (func))))");
        assert(m && "sub parse");
        const jav_rec_type_t* rt = &find_sec(m, 1)->body.u.case_1.types.items[1];
        assert(rt->head == 0x50);                                  // open sub
        const jav_sub_type_t* st = &rt->body.u.case_2;
        assert(st->super_count == 1 && st->supers.items[0] == 0 && st->body.head == 0x60);
        printf("OK  test_wat: GC sub type (supertype + comptype)\n");
        jav_module_free(m); free(m);
    }
    {   // rec group with two members (struct, func)
        jav_module_t* m = parse_text("(module (rec (type (struct)) (type (func))))");
        assert(m && "rec parse");
        const jav_rec_type_t* rt = &find_sec(m, 1)->body.u.case_1.types.items[0];
        assert(rt->head == 0x4E && rt->body.u.case_0.count == 2);
        const jav_rec_member_t* mem = rt->body.u.case_0.members.items;
        assert(mem[0].head == 0x5F && mem[1].head == 0x60);
        printf("OK  test_wat: GC rec group (struct + func members)\n");
        jav_module_free(m); free(m);
    }
    {   // §6.6.2 symbolic struct field names: struct.get/set resolve a $field to its position
        // in the referenced type's field space. struct.get = 0xFB 0x02 [typeidx, fieldidx].
        jav_module_t* m = parse_text(
            "(module (type $t (struct (field $x i32) (field $y i64)))"
            " (func (param (ref $t)) (result i64) (struct.get $t $y (local.get 0))))");
        assert(m && "struct.get $field parse");
        static const uint8_t b[] = {0x00, 0x20,0x00, 0xfb,0x02, 0x00, 0x01, 0x0b};  // folded: local.get 0 then struct.get t0 f1
        assert(body_bytes_eq(m, 0, b, sizeof b) && "struct.get $t $y -> [type 0, field 1]");
        jav_module_free(m); free(m);
        // numeric field index still works; an unknown field $id fails cleanly (not silently 0).
        assert(parse_ok("(module (type $t (struct (field i32))) (func (param (ref $t)) (result i32) (struct.get $t 0 (local.get 0))))")
               && "numeric fieldidx");
        assert(!parse_ok("(module (func (struct.get $t $nope (local.get 0))) (type $t (struct (field $x i32))))")
               && "unknown field $id rejected");
        printf("OK  test_wat: struct field $id resolution (§6.6.2)\n");
    }
}

// §6.5.11 folded instructions: must produce the SAME bytes as the linear expansion.
static void test_folded(void) {
    static const uint8_t b_add[] = {0x00, 0x20,0x00, 0x20,0x01, 0x6a, 0x0b};   // children then head
    assert(body_roundtrips("(i32.add (local.get 0) (local.get 1))", b_add, sizeof b_add));
    static const uint8_t b_blk[] = {0x00, 0x02,0x7f, 0x41,0x00, 0x0b, 0x0b};   // ( block … ) == block … end
    assert(body_roundtrips("(block (result i32) (i32.const 0))", b_blk, sizeof b_blk));
    // ( if bt <cond> (then …) (else …) ): condition emits first, then if/bt/then/else/end
    static const uint8_t b_if[] = {0x00, 0x41,0x01, 0x04,0x7f, 0x41,0x02, 0x05, 0x41,0x03, 0x0b, 0x0b};
    assert(body_roundtrips("(if (result i32) (i32.const 1) (then (i32.const 2)) (else (i32.const 3)))",
                           b_if, sizeof b_if));
    printf("OK  test_wat: folded instructions (plain, block, if == linear)\n");
    {   // the payoff: folded init expressions now parse (conventional `(global … (i32.const N))`)
        jav_module_t* m = parse_text("(module (global i32 (i32.const 7)))");
        assert(m && "folded init parse");
        const jav_global_t* g = &find_sec(m, 6)->body.u.case_6.globals.items[0];
        assert(g->init.instrs.count == 1 && g->init.instrs.items[0].op == 0x41 && "folded init expr");
        printf("OK  test_wat: folded init expr (global (i32.const 7))\n");
        jav_module_free(m); free(m);
    }
}

// Completeness gate: every instruction in instructions.toml must, for the
// implemented immediate shapes, translate text -> the exact spec opcode encoding.
// Deferred shapes are counted and reported — nothing is silently skipped.
static int all_instr_gate(void) {
    FILE* f = fopen("../spec/instructions.toml", "rb");
    if (!f) { fprintf(stderr, "no instructions.toml\n"); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = malloc((size_t)len);
    if (fread(src, 1, (size_t)len, f) != (size_t)len) { fclose(f); return 1; }
    fclose(f);
    bbq_arena a; bbq_arena_init(&a, 1u << 20);
    toml_doc_t* doc = toml_parse(src, (int)len, &a);
    const toml_val_t* arr = toml_tbl_get(toml_doc_root(doc), "instr");

    int enc = 0, deferred = 0, fail = 0;
    for (int i = 0; i < arr->u.array.count; i++) {
        const toml_tbl_t* t = arr->u.array.items[i]->u.table;
        const char* nm; const char* sh; int64_t pfx = 0, code = 0;
        toml_val_as_string(toml_tbl_get(t, "name"), &nm);
        toml_val_as_string(toml_tbl_get(t, "shape"), &sh);
        const toml_val_t* opv = toml_tbl_get(t, "opcode");
        if (opv->type == TOML_VT_INT) toml_val_as_int(opv, &code);
        else { toml_val_as_int(opv->u.array.items[0], &pfx);
               toml_val_as_int(opv->u.array.items[1], &code); }

        const char* ops; uint8_t imm[16]; int nimm; memset(imm, 0, sizeof imm);
        if      (!strcmp(sh, "none")) { ops = "";    nimm = 0; }
        else if (!strcmp(sh, "idx"))  { ops = "0";   nimm = 1; }
        else if (!strcmp(sh, "idx2")) { ops = "0 0"; nimm = 2; }
        else if (!strcmp(sh, "i32"))  { ops = "0";   nimm = 1; }
        else if (!strcmp(sh, "i64"))  { ops = "0";   nimm = 1; }
        else if (!strcmp(sh, "lane")) { ops = "0";   nimm = 1; }
        else if (!strcmp(sh, "memarg")) {              // defaults: natural align + offset 0
            int64_t alv = 0; toml_val_as_int(toml_tbl_get(t, "align"), &alv);
            ops = ""; imm[0] = (uint8_t)alv; nimm = 2;
        }
        else if (!strcmp(sh, "memlane")) {             // memarg defaults + lane index
            int64_t alv = 0; toml_val_as_int(toml_tbl_get(t, "align"), &alv);
            ops = "0"; imm[0] = (uint8_t)alv; nimm = 3;
        }
        else if (!strcmp(sh, "brtable")) { ops = "0 0"; imm[0] = 1; nimm = 3; }   // [len=1, l0=0, default=0]
        else if (!strcmp(sh, "selectt")) { ops = "(result i32)"; imm[0] = 1; imm[1] = 0x7f; nimm = 2; }
        else if (!strcmp(sh, "f32"))     { ops = "0"; nimm = 4; }                 // 0.0f -> 4 zero bytes
        else if (!strcmp(sh, "f64"))     { ops = "0"; nimm = 8; }                 // 0.0  -> 8 zero bytes
        else if (!strcmp(sh, "heap")) {                // ref.null heaptype / ref.test|cast reftype
            if (pfx == 0) { ops = "func"; imm[0] = 0x70; nimm = 1; }              // ref.null func
            else { ops = (code & 1) ? "(ref null func)" : "(ref func)";          // null variant = odd subop
                   imm[0] = 0x70; nimm = 1; }
        }
        else if (!strcmp(sh, "broncast")) {            // flags=0, label=0, ht1=func, ht2=func
            ops = "0 (ref func) (ref func)"; imm[2] = 0x70; imm[3] = 0x70; nimm = 4;
        }
        else if (!strcmp(sh, "v128")) {                // 16 zero bytes (const lanes or shuffle indices)
            ops = (code == 12) ? "i32x4 0 0 0 0" : "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"; nimm = 16;
        }
        else if (!strcmp(sh, "block") || !strcmp(sh, "if")) {   // empty blocktype + immediate end
            ops = "end"; imm[0] = 0x40; imm[1] = 0x0b; nimm = 2;
        }
        else if (!strcmp(sh, "trytable")) {            // empty blocktype, 0 catches, immediate end
            ops = "end"; imm[0] = 0x40; imm[1] = 0x00; imm[2] = 0x0b; nimm = 3;
        }
        else { deferred++; continue; }                 // shape not yet implemented

        // An idx2 op with operands [typeidx, tableidx] is call_indirect/return_call_indirect
        // (kind-based, no name hack): its text is a typeuse, not two bare indices — but it
        // encodes to the same typeidx+tableidx bytes (op 00 00).
        if (!strcmp(sh, "idx2")) {
            const toml_val_t* opnds = toml_tbl_get(t, "operands");
            const char *k0 = "", *k1 = "";
            if (opnds && opnds->type == TOML_VT_ARRAY && opnds->u.array.count == 2) {
                toml_val_as_string(opnds->u.array.items[0], &k0);
                toml_val_as_string(opnds->u.array.items[1], &k1);
            }
            if (!strcmp(k0, "typeidx") && !strcmp(k1, "tableidx")) ops = "(type 0)";
        }

        // Expected body: 00 (locals) | opcode (byte, or prefix+uleb sub) | imm | 0B.
        uint8_t exp[40]; int n = 0; exp[n++] = 0x00;
        if (pfx) { exp[n++] = (uint8_t)pfx; uint64_t v = (uint64_t)code;
                   do { uint8_t b = v & 0x7f; v >>= 7; if (v) b |= 0x80; exp[n++] = b; } while (v); }
        else     { exp[n++] = (uint8_t)code; }
        for (int k = 0; k < nimm; k++) exp[n++] = imm[k];
        exp[n++] = 0x0b;

        char inner[96]; snprintf(inner, sizeof inner, "%s %s", nm, ops);
        if (body_roundtrips(inner, exp, (size_t)n)) enc++;
        else { fail++; if (fail <= 12) printf("  FAIL encode %-24s (shape %s)\n", nm, sh); }
    }
    free(src); bbq_arena_free(&a);
    printf("  all-instr gate: %d/%d encoded, %d deferred, %d FAIL\n",
           enc, enc + deferred + fail, deferred, fail);
    return fail;
}

// ── §6.6.9 / §6.6.6 ELEMENT SEGMENTS — the whole feature in ONE auditable place ──
// (A) every text FORM parses (spec-enumerated coverage table); (B) all 8 binary variant
// flags are selected correctly; (C) the §6.6.6 inline-table-elem abbreviation. A failure
// names the exact form/variant — no .wast-count guessing. To audit elem coverage, read this.
static int test_elements(void) {
    int fail = 0;

    // (A) FORM coverage — passive/active/declarative × func-funcidx / reftype-elemexpr / bare
    //     funcidx × (item …)/folded elemexpr × explicit/omitted tableuse × (offset …)/folded.
    static const char* PRE =
        "(module (func $f) (func $g) (table $t 1 funcref) (table 1 externref) ";
    static const struct { const char* form; const char* desc; } forms[] = {
        { "(elem func $f $g)",                                    "passive func-funcidx" },
        { "(elem $e func $f)",                                    "passive func + id" },
        { "(elem funcref (ref.func $f) (ref.null func))",         "passive reftype + folded elemexprs" },
        { "(elem funcref (item (ref.func $f)))",                  "passive reftype + (item …)" },
        { "(elem externref (ref.null extern))",                   "passive externref reftype" },
        { "(elem func)",                                          "passive empty func" },
        { "(elem funcref)",                                       "passive empty reftype list" },
        { "(elem (table $t) (offset (i32.const 0)) func $f)",     "active explicit table + (offset expr)" },
        { "(elem (table 0) (i32.const 0) funcref (ref.func $f))", "active table + folded offset + reftype" },
        { "(elem (offset (i32.const 0)) func $f)",                "active tableuse omitted (=0)" },
        { "(elem (i32.const 0) func $f)",                         "active omitted table + folded offset" },
        { "(elem (i32.const 0) $f $g)",                           "active bare funcidx compat" },
        { "(elem $e (table 0) (offset (i32.const 0)) func $f)",   "active + id" },
        { "(elem declare func $f)",                               "declarative func" },
        { "(elem $e declare func $f)",                            "declarative + id" },
        { "(elem declare funcref (ref.func $f))",                 "declarative reftype" },
        { "(table funcref (elem $f $g))",                         "inline-table-elem funcidx shorthand" },
        { "(table funcref (elem funcref (ref.func $f)))",         "inline-table-elem expr form" },
        { "(table funcref (elem (ref.func $f) (ref.null func)))", "inline-table bare elemexpr list (§6.6.6)" },
    };
    char buf[256];
    for (int i = 0; i < (int)(sizeof forms / sizeof forms[0]); i++) {
        snprintf(buf, sizeof buf, "%s%s)", PRE, forms[i].form);
        jav_module_t* m = parse_text(buf);
        if (m) { jav_module_free(m); free(m); }
        else { fail++; printf("  ELEM form FAIL: %s\n      %s\n", forms[i].desc, forms[i].form); }
    }

    // (B) all 8 binary variant FLAGS (§5.5.12), one segment each, in flag order 0..7.
    {
        jav_module_t* m = parse_text(
            "(module (func $f) (table $t 1 funcref)"
            " (elem (offset (i32.const 0)) func $f)"                          // 0 active table0 funcidx
            " (elem func $f)"                                                 // 1 passive funcidx
            " (elem (table $t) (offset (i32.const 0)) func $f)"               // 2 active expl-table funcidx
            " (elem declare func $f)"                                         // 3 declarative funcidx
            " (elem (offset (i32.const 0)) funcref (ref.func $f))"            // 4 active table0 expr
            " (elem funcref (ref.func $f))"                                   // 5 passive expr
            " (elem (table $t) (offset (i32.const 0)) funcref (ref.func $f))" // 6 active expl-table expr
            " (elem declare funcref (ref.func $f)))");                        // 7 declarative expr
        if (!m) { fail++; printf("  ELEM 8-variant module did not parse\n"); }
        else {
            const jav_element_section_t* es = &find_sec(m, 9)->body.u.case_9;
            if (es->count != 8) { fail++; printf("  ELEM variants: %u segments, want 8\n", es->count); }
            else for (int i = 0; i < 8; i++)
                if (es->elems.items[i].flag != (uint8_t)i) {
                    fail++; printf("  ELEM variant %d → flag %u (want %d)\n", i, es->elems.items[i].flag, i);
                }
            jav_module_free(m); free(m);
        }
    }

    // (C) §6.6.6 inline-table-elem: table limits min=max=|elems| + a generated active elem on
    // that table at (i32.const 0) — variant 2 (funcidx shorthand) / variant 6 (expr form).
    {
        jav_module_t* m = parse_text("(module (func $a) (func $b) (table funcref (elem $a $b)))");
        if (!m) { fail++; printf("  ELEM inline-table funcidx: parse\n"); }
        else {
            const jav_table_t* t = &find_sec(m, 4)->body.u.case_4.tables.items[0];
            const jav_elem_t* e = &find_sec(m, 9)->body.u.case_9.elems.items[0];
            if (!(t->u.default_val.type.limits.min == 2 && t->u.default_val.type.limits.max.value == 2
                  && e->flag == 2 && e->body.u.case_2.funcs.count == 2
                  && e->body.u.case_2.offset.instrs.items[0].op == 0x41))
                { fail++; printf("  ELEM inline-table funcidx: structure\n"); }
            jav_module_free(m); free(m);
        }
        m = parse_text("(module (func $a) (table funcref (elem funcref (ref.func $a))))");
        if (!m) { fail++; printf("  ELEM inline-table expr: parse\n"); }
        else {
            const jav_elem_t* e = &find_sec(m, 9)->body.u.case_9.elems.items[0];
            if (!(e->flag == 6 && e->body.u.case_6.exprs.exprs.items[0].instrs.items[0].op == 0xD2))
                { fail++; printf("  ELEM inline-table expr: structure\n"); }
            jav_module_free(m); free(m);
        }
    }

    printf("  elem segments: %d forms + 8 variants + inline-table — %s\n",
           (int)(sizeof forms / sizeof forms[0]), fail ? "GAPS (above)" : "all covered");
    return fail;
}

// §6.3.1/§6.3.2 literal VALUES, byte-exact — the conformance corpus only checks
// parse verdicts, so the '_'-separator and hex-float conversions are locked here
// (the old parser silently read `1_000` as 1). Plus one reject per strictness
// rule the corpus enforces in bulk (tokens, labels, ids, imports, typeuse).
static void test_literals(void) {
    // 1_000 -> sleb 0xE8 0x07; 0x7fff_ffff -> sleb 0xFF..0x07
    static const uint8_t b_dec[] = {0x00, 0x41, 0xE8, 0x07, 0x0B};
    assert(body_roundtrips("i32.const 1_000", b_dec, sizeof b_dec));
    static const uint8_t b_hex[] = {0x00, 0x41, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x0B};
    assert(body_roundtrips("i32.const 0x7fff_ffff", b_hex, sizeof b_hex));
    // i64.const -0x8000000000000000 (the most negative s64)
    static const uint8_t b_min[] = {0x00, 0x42, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x7F, 0x0B};
    assert(body_roundtrips("i64.const -0x8000_0000_0000_0000", b_min, sizeof b_min));
    // hex float without exponent: 0x1p4 == 16.0f == 0x41800000
    static const uint8_t b_f32[] = {0x00, 0x43, 0x00, 0x00, 0x80, 0x41, 0x0B};
    assert(body_roundtrips("f32.const 0x1p4", b_f32, sizeof b_f32));
    // underscored float: 1_000.5 == 0x408F440000000000
    static const uint8_t b_f64[] = {0x00, 0x44, 0x00,0x00,0x00,0x00,0x00,0x44,0x8F,0x40, 0x0B};
    assert(body_roundtrips("f64.const 1_000.5", b_f64, sizeof b_f64));
    // nan payload: f32 nan:0x200000 -> 0x7FA00000
    static const uint8_t b_nan[] = {0x00, 0x43, 0x00, 0x00, 0xA0, 0x7F, 0x0B};
    assert(body_roundtrips("f32.const nan:0x200000", b_nan, sizeof b_nan));
    printf("OK  test_wat: literal values ('_' separators, hex floats, nan payload)\n");

    // One reject per strictness rule (the corpus carries the breadth).
    assert(!parse_ok("(module (func i32.const 1__0))")        && "double underscore");
    assert(!parse_ok("(module (func i32.const 0x))")          && "0x with no digits");
    assert(!parse_ok("(module (func i32.const 99_))")         && "trailing underscore");
    assert(!parse_ok("(module (func i32.const 4294967296))")  && "i32 out of range");
    assert(!parse_ok("(module (func f32.const nan:0x0))")     && "nan payload zero");
    assert(!parse_ok("(module (func f32.const nan:1))")       && "nan payload not 0x");
    assert(!parse_ok("(module (func f32.const 0x1p128))")     && "rounds out of f32 range");
    assert(!parse_ok("(module (func (v128.const i8x16 256 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)))")
           && "i8 lane out of range");
    assert(!parse_ok("(module (data\"a\"))")                  && "string glued to keyword");
    assert(!parse_ok("(module (data \"a\"\"b\"))")            && "string glued to string");
    assert(!parse_ok("(module (func block $a end $b))")       && "mismatching end label");
    assert(!parse_ok("(module (func $x) (func $x))")          && "duplicate func id");
    assert(!parse_ok("(module (func (param $p i32) (local $p i32)))") && "duplicate local id");
    assert(!parse_ok("(module (func $\"\"))")                 && "empty quoted id");
    assert(!parse_ok("(module (func) (import \"m\" \"n\" (func)))")   && "import after definition");
    assert(!parse_ok("(module (type (func)) (func (type 0) (param i32)))") && "inline type mismatch");
    assert(!parse_ok("(module (import \"\\80\" \"n\" (func)))") && "name not UTF-8");
    assert(!parse_ok("(module (start 0) (start 0))")           && "multiple start");
    assert(!parse_ok("(module (func (block (param $x i32))))") && "id in blocktype param");
    printf("OK  test_wat: strictness rejects (one per rule; corpus has the breadth)\n");
}

int main(void) {
    // (module (type (func (param i32) (result i32))))
    const char* text = "(module (type (func (param i32) (result i32))))";
    static const uint8_t bin[] = {
        0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,   // magic, version
        0x01, 0x06, 0x01, 0x60, 0x01,0x7f, 0x01,0x7f // type sec: size 6, 1 type, func [i32]->[i32]
    };

    jav_module_t* mt = parse_text(text);
    assert(mt && "text parse");
    jav_module_t mb = read_bin(bin, sizeof bin);

    assert(mt->magic == mb.magic && mt->version == mb.version);
    assert(mt->sections.count == 1 && mb.sections.count == 1);

    const jav_section_t *st = &mt->sections.items[0], *sb = &mb.sections.items[0];
    assert(st->id == 1 && sb->id == 1);
    assert(st->body.tag == 1 && sb->body.tag == 1);            // type section

    const jav_type_section_t *ts = &st->body.u.case_1, *tb = &sb->body.u.case_1;
    assert(ts->count == 1 && tb->count == 1);
    assert(ts->types.count == 1 && tb->types.count == 1);

    const jav_rec_type_t *rt = &ts->types.items[0], *rb = &tb->types.items[0];
    assert(rt->head == 0x60 && rb->head == 0x60);              // func comptype
    assert(rt->body.tag == 0x60 && rb->body.tag == 0x60);      // switch tag = matched head byte

    const jav_func_type_t *ft = &rt->body.u.case_5, *fb = &rb->body.u.case_5;
    assert(ft->param_count == 1 && fb->param_count == 1);
    assert(ft->params.items[0].head == 0x7f && fb->params.items[0].head == 0x7f);
    assert(ft->result_count == 1 && fb->result_count == 1);
    assert(ft->results.items[0].head == 0x7f && fb->results.items[0].head == 0x7f);

    printf("OK  test_wat: text==binary struct (module + type section)\n");
    jav_module_free(mt); free(mt);
    jav_module_free(&mb);

    // (module (func (param i32) (result i32)))  -> type + function + code sections.
    // One text func emits three binary sections; the code body is the FuncBody tree.
    const char* ftext = "(module (func (param i32) (result i32)))";
    static const uint8_t fbin[] = {
        0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,
        0x01, 0x06, 0x01, 0x60, 0x01,0x7f, 0x01,0x7f,  // type: func [i32]->[i32]
        0x03, 0x02, 0x01, 0x00,                        // function: 1 func, type 0
        0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b             // code: 1 entry, size 2, body 00(locals) 0b(end)
    };
    jav_module_t* ftm = parse_text(ftext); assert(ftm && "func text parse");
    jav_module_t fbm = read_bin(fbin, sizeof fbin);
    assert(ftm->sections.count == 3 && fbm.sections.count == 3);

    const jav_section_t *xt = find_sec(ftm, 3), *xb = find_sec(&fbm, 3);   // function section
    assert(xt && xb);
    assert(xt->body.u.case_3.count == 1 && xb->body.u.case_3.count == 1);
    assert(xt->body.u.case_3.type_indices.items[0] == 0 &&
           xb->body.u.case_3.type_indices.items[0] == 0);

    const jav_section_t *ct = find_sec(ftm, 10), *cb = find_sec(&fbm, 10); // code section
    assert(ct && cb);
    assert(ct->body.u.case_10.count == 1 && cb->body.u.case_10.count == 1);
    const jav_code_entry_t *et = &ct->body.u.case_10.entries.items[0];
    const jav_code_entry_t *eb = &cb->body.u.case_10.entries.items[0];
    assert(et->body.local_count == 0 && eb->body.local_count == 0);            // FuncBody: no locals
    assert(et->body.body.instrs.count == 0 && eb->body.body.instrs.count == 0); // empty expr
    assert(et->body.body.end == 0x0b && eb->body.body.end == 0x0b);            // implicit end

    printf("OK  test_wat: text==binary struct (func -> type+function+code)\n");
    jav_module_free(ftm); free(ftm);
    jav_module_free(&fbm);

    // (module (func i32.add))  -> a body with one plain (none-shape) instruction.
    // The mnemonic is resolved against instructions.toml and translated to bytes,
    // then decoded into the FuncBody tree.
    const char* itext = "(module (func i32.add))";
    static const uint8_t ibin[] = {
        0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,        // type: func []->[]
        0x03, 0x02, 0x01, 0x00,                    // function: 1 func, type 0
        0x0a, 0x05, 0x01, 0x03, 0x00, 0x6a, 0x0b   // code: 1 entry size 3, body 00 6a 0b
    };
    jav_module_t* itm = parse_text(itext); assert(itm && "instr text parse");
    jav_module_t ibm = read_bin(ibin, sizeof ibin);

    const jav_section_t *ict = find_sec(itm, 10), *icb = find_sec(&ibm, 10);
    assert(ict && icb);
    const jav_func_body_t *bt = &ict->body.u.case_10.entries.items[0].body;
    const jav_func_body_t *bb = &icb->body.u.case_10.entries.items[0].body;
    assert(bt->local_count == 0 && bb->local_count == 0);
    assert(bt->body.instrs.count == 1 && bb->body.instrs.count == 1);
    assert(bt->body.instrs.items[0].op == 0x6a && bb->body.instrs.items[0].op == 0x6a);  // i32.add
    assert(bt->body.end == 0x0b && bb->body.end == 0x0b);

    printf("OK  test_wat: text==binary struct (func body: i32.add via toml lookup)\n");
    jav_module_free(itm); free(itm);
    jav_module_free(&ibm);

    // Integer-immediate shapes: text -> tree -> bytes must equal the spec encoding.
    static const uint8_t b_add[]  = {0x00, 0x6a, 0x0b};                   // none
    static const uint8_t b_lget[] = {0x00, 0x20, 0x00, 0x0b};            // idx
    static const uint8_t b_ci[]   = {0x00, 0x11, 0x00, 0x00, 0x0b};      // call_indirect (type 0) → typeidx 0, tableidx 0
    static const uint8_t b_i32[]  = {0x00, 0x41, 0x2a, 0x0b};            // i32 (i32.const 42)
    static const uint8_t b_i64[]  = {0x00, 0x42, 0x2a, 0x0b};            // i64 (i64.const 42)
    assert(body_roundtrips("i32.add",                b_add,  sizeof b_add));
    assert(body_roundtrips("local.get 0",            b_lget, sizeof b_lget));
    assert(body_roundtrips("call_indirect (type 0)", b_ci,   sizeof b_ci));  // §6.5.3 typeuse, not a bare typeidx
    assert(body_roundtrips("i32.const 42",           b_i32,  sizeof b_i32));
    assert(body_roundtrips("i64.const 42",           b_i64,  sizeof b_i64));
    printf("OK  test_wat: instr immediates round-trip (none/idx/call_indirect typeuse/i32/i64)\n");

    // f32/f64.const bare nan/inf -> the canonical bit patterns (the gate only feeds 0).
    static const uint8_t b_f32nan[] = {0x00, 0x43, 0x00,0x00,0xc0,0x7f, 0x0b};
    static const uint8_t b_f32inf[] = {0x00, 0x43, 0x00,0x00,0x80,0x7f, 0x0b};
    static const uint8_t b_f64nan[] = {0x00, 0x44, 0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x7f, 0x0b};
    assert(body_roundtrips("f32.const nan", b_f32nan, sizeof b_f32nan));
    assert(body_roundtrips("f32.const inf", b_f32inf, sizeof b_f32inf));
    assert(body_roundtrips("f64.const nan", b_f64nan, sizeof b_f64nan));
    printf("OK  test_wat: f32/f64.const nan/inf canonical bits\n");

    // signed inf/nan + nan:payload + a signed decimal — bit-exact
    static const uint8_t b_f32ninf[] = {0x00, 0x43, 0x00,0x00,0x80,0xff, 0x0b};               // -inf
    static const uint8_t b_f32nnan[] = {0x00, 0x43, 0x00,0x00,0xc0,0xff, 0x0b};               // -nan
    static const uint8_t b_f32pay[]  = {0x00, 0x43, 0x01,0x00,0x80,0x7f, 0x0b};               // nan:0x1
    static const uint8_t b_f32neg[]  = {0x00, 0x43, 0x00,0x00,0xc0,0xbf, 0x0b};               // -1.5
    static const uint8_t b_f64npay[] = {0x00, 0x44, 0x01,0x00,0x00,0x00,0x00,0x00,0xf0,0xff, 0x0b}; // -nan:0x1
    assert(body_roundtrips("f32.const -inf",     b_f32ninf, sizeof b_f32ninf));
    assert(body_roundtrips("f32.const -nan",     b_f32nnan, sizeof b_f32nnan));
    assert(body_roundtrips("f32.const nan:0x1",  b_f32pay,  sizeof b_f32pay));
    assert(body_roundtrips("f32.const -1.5",     b_f32neg,  sizeof b_f32neg));
    assert(body_roundtrips("f64.const -nan:0x1", b_f64npay, sizeof b_f64npay));
    printf("OK  test_wat: f32/f64.const signed inf/nan + nan:payload\n");

    // §6.3.2 v128.const f32x4/f64x2 lanes carry the same inf/nan/payload syntax as the
    // scalar consts (op 0xFD subop 12), packed bit-exact LE — int lanes must reject inf/nan.
    {   // f32x4 nan nan nan nan -> 4× canonical f32 nan (0x7FC00000 LE)
        static const uint8_t b[] = {0x00, 0xfd,0x0c, 0x00,0x00,0xc0,0x7f, 0x00,0x00,0xc0,0x7f,
                                    0x00,0x00,0xc0,0x7f, 0x00,0x00,0xc0,0x7f, 0x0b};
        assert(body_roundtrips("v128.const f32x4 nan nan nan nan", b, sizeof b));
    }
    {   // f32x4 mixed: nan:0x1, -inf, 1.5, -nan
        static const uint8_t b[] = {0x00, 0xfd,0x0c, 0x01,0x00,0x80,0x7f, 0x00,0x00,0x80,0xff,
                                    0x00,0x00,0xc0,0x3f, 0x00,0x00,0xc0,0xff, 0x0b};
        assert(body_roundtrips("v128.const f32x4 nan:0x1 -inf 1.5 -nan", b, sizeof b));
    }
    {   // f64x2 inf -inf -> exp all-ones, sign on lane 1
        static const uint8_t b[] = {0x00, 0xfd,0x0c, 0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0x7f,
                                    0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xff, 0x0b};
        assert(body_roundtrips("v128.const f64x2 inf -inf", b, sizeof b));
    }
    {   // an integer lane shape must NOT accept inf/nan (malformed → no parse)
        assert(!parse_ok("(module (func v128.const i32x4 nan 0 0 0))") && "i32x4 nan rejected");
    }
    printf("OK  test_wat: v128.const f32x4/f64x2 inf/nan/payload (+ int-lane reject)\n");

    // relaxed-SIMD i32x4.relaxed_trunc_f64x2_s_zero (0xFD subop 259): the §7.10 index drops
    // the `_zero` the text format/opcode keep, so the spec-derived mnemonic must restore it.
    {
        static const uint8_t b[] = {0x00, 0x20,0x00, 0xfd,0x83,0x02, 0x0b};  // local.get 0; 0xFD 259
        assert(body_roundtrips("(i32x4.relaxed_trunc_f64x2_s_zero (local.get 0))", b, sizeof b));
        printf("OK  test_wat: relaxed_trunc_f64x2_s_zero mnemonic (_zero restored)\n");
    }

    // select with 17 result types must not truncate (the buffer was capped at 16).
    {
        char inner[512]; uint8_t exp[64]; int n = 0;
        strcpy(inner, "select"); exp[n++] = 0x00; exp[n++] = 0x1c; exp[n++] = 17;
        for (int i = 0; i < 17; i++) { strcat(inner, " (result i32)"); exp[n++] = 0x7f; }
        exp[n++] = 0x0b;
        assert(body_roundtrips(inner, exp, (size_t)n));
        printf("OK  test_wat: select 17 result types (no truncation past old cap of 16)\n");
    }

    // §6.5.2 typed select takes `(result t*)*` — an EMPTY `(result)`, multiple valtypes in
    // one clause, and several clauses must all PARSE (arity>1 is a validation error, not a
    // parse error, so assert_invalid bodies like `select (result i32) (result)` reach here).
    {
        static const uint8_t b_empty[]  = {0x00, 0x1c, 0x00, 0x0b};                  // select (result)
        static const uint8_t b_multi[]  = {0x00, 0x1c, 0x02, 0x7f,0x7e, 0x0b};       // select (result i32 i64)
        static const uint8_t b_clauses[]= {0x00, 0x1c, 0x02, 0x7f,0x7e, 0x0b};       // select (result i32) (result i64)
        assert(body_roundtrips("select (result)", b_empty, sizeof b_empty));
        assert(body_roundtrips("select (result i32 i64)", b_multi, sizeof b_multi));
        assert(body_roundtrips("select (result i32) (result i64)", b_clauses, sizeof b_clauses));
        // implicit-arity folded select with no result clause stays plain select (0x1b).
        assert(parse_ok("(module (func (select (nop) (nop) (i32.const 1))))") && "folded implicit select");
        printf("OK  test_wat: select (result t*)* — empty / multi-valtype / multi-clause / folded\n");
    }

    // Locals: declared locals emit a run-length-grouped locals vec (was hardcoded 0).
    static const uint8_t b_locals[] = {0x02, 0x02,0x7f, 0x01,0x7e, 0x0b};   // (2×i32)(1×i64)
    assert(body_roundtrips("(local i32 i32 i64)", b_locals, sizeof b_locals));
    // local.get resolves $id against params (locals 0..) then the declared locals.
    static const uint8_t b_lget2[] = {0x01,0x01,0x7f, 0x20,0x00, 0x20,0x01, 0x0b};
    assert(body_roundtrips("(param $a i32) (local $b i32) local.get $a local.get $b",
                           b_lget2, sizeof b_lget2));
    printf("OK  test_wat: locals vec (RLE) + local.get $id resolution\n");

    test_idresolve();
    test_literals();
    test_structured();
    test_folded();
    test_gctypes();
    test_module_fields();
    assert(test_elements() == 0);

    assert(all_instr_gate() == 0);
    return 0;
}
