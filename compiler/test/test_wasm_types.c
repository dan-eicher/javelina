// test_wasm_types.c — #25 FOUNDATION. The WASM-GC type section: each class →
// a struct type (instance fields + the superclass as GC supertype), each array
// → an array type, all in one recursive group. Encoding pinned against the VM's
// reader (wasm/spec/wasm.bbq). This is what struct.new/get, array.*, ref.test
// and call_ref all build on.
#include "java_parser.h"
#include "javelina/compiler/sema.h"
#include "javelina/compiler/wasm_types.h"
#include "bbq_arena.h"
#include "bbq_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "javelina_test.h"

/* The §7.3-correct prelude+user parse — per-unit programs (jtest_units.h).
 * The full prelude (lang/util/io/simd): java.lang's own §7.5 imports name
 * java.io types, so a lang-only environment is no longer legal Java. */
#include "jtest_units.h"
#define build_program jtest_build_flat

static int find_class(const sema_ctx_t* s, const char* name) {
    for (int i = 0; i < (int)bbq_vec_len(s->classes); i++) {
        const sema_class_t* c = sema_get_class(s, i);
        if (c->name && !strcmp(c->name, name)) return i;
    }
    return -1;
}
static int contains(const uint8_t* hay, int hn, const uint8_t* needle, int nn) {
    for (int i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, (size_t)nn)) return 1;
    return 0;
}

/* JLS §15.12.4.4 steps 1-2, transcribed: "search class S, and then the superclasses of class
 * S" for "a declaration for a non-abstract method named m with the same descriptor". The
 * oracle deliberately does NOT call sema_resolve_virtual — that routine is what fills the
 * vtable, so using it here would make the check agree with the emitter by construction.
 * §8.4.3.4:
 * `abstract native` is a compile-time error, so a native declaration is non-abstract. */
static bool jls_locate_method(const sema_ctx_t* s, int recv, const sema_method_t* want,
                              int* out_c, int* out_m) {
    for (int id = recv; id >= 0; id = sema_get_class(s, id)->super_id) {
        const sema_class_t* c = sema_get_class(s, id);
        for (int j = 0; j < (int)bbq_vec_len(c->methods); j++) {
            if (!sema_is_virtual_method(&c->methods[j])) continue;
            if (!sema_same_vsig(&c->methods[j], want)) continue;
            if (c->methods[j].modifiers & SEMA_ACC_ABSTRACT) break;  /* step 2: try the super */
            *out_c = id; *out_m = j; return true;
        }
    }
    return false;
}

static int find_method(const sema_ctx_t* s, int cid, const char* name) {
    const sema_class_t* c = sema_get_class(s, cid);
    for (int i = 0; i < (int)bbq_vec_len(c->methods); i++)
        if (c->methods[i].name && !strcmp(c->methods[i].name, name)) return i;
    return -1;
}

/* Decode the emitted global-section CONTENT back out of the artifact bytes and return the
 * funcidx sitting in slot `want_slot` of global `target` (a vtable global): the ref.func
 * operand, or -1 when the row is ref.null, or <-1 on a malformed stream. Opcode-aware —
 * a "scan to 0x0B" would misparse `i32.const 11` — and FAIL-LOUD (-4) on any const-expr
 * opcode it does not know, so a new emission shape breaks this test instead of sliding by. */
/* `rows` (optional) receives EVERY slot of the target global, so a whole-vtable check costs
 * one pass instead of one per slot. */
static int32_t vtable_row_scan(const uint8_t* buf, int len, int target, int want_slot,
                               int32_t* rows, int nrows) {
    bbq_ctx_t rc; bbq_ctx_init(&rc, buf, (size_t)len);
    uint64_t count;
    if (!bbq_read_uleb128(&rc, &count, 32)) return -3;
    int32_t found = -5;
    for (int g = 0; g < (int)count; g++) {
        uint8_t b;
        if (!bbq_read_u8(&rc, &b)) return -3;
        if (b == 0x63 || b == 0x64) {                     /* (ref null $t) / (ref $t) */
            int32_t t; if (!bbq_read_sleb128_s33(&rc, &t)) return -3;
        }                                                  /* else: one-byte valtype */
        if (!bbq_read_u8(&rc, &b)) return -3;              /* mutability */
        int at = 0;                                        /* slot cursor within this init */
        for (;;) {
            uint8_t op; if (!bbq_read_u8(&rc, &op)) return -3;
            if (op == 0x0B) break;                         /* end of this global's init */
            int64_t s; uint64_t u; int32_t t; int32_t row = -1;
            switch (op) {
            case 0x41: if (!bbq_read_sleb128(&rc, &s, 32)) return -3; continue;  /* i32.const */
            case 0x42: if (!bbq_read_sleb128(&rc, &s, 64)) return -3; continue;  /* i64.const */
            case 0x43: for (int i = 0; i < 4; i++) if (!bbq_read_u8(&rc, &b)) return -3; continue;
            case 0x44: for (int i = 0; i < 8; i++) if (!bbq_read_u8(&rc, &b)) return -3; continue;
            case 0x23: if (!bbq_read_uleb128(&rc, &u, 32)) return -3; continue;  /* global.get */
            case 0xD0:                                                        /* ref.null ht */
                if (!bbq_read_sleb128_s33(&rc, &t)) return -3;
                break;
            case 0xD2:                                                        /* ref.func f */
                if (!bbq_read_uleb128(&rc, &u, 32)) return -3;
                row = (int32_t)u; break;
            case 0xFB:                        /* GC const: struct.new / array.new_fixed */
                if (!bbq_read_uleb128(&rc, &u, 32)) return -3;                /* sub-opcode */
                { uint64_t ti; if (!bbq_read_uleb128(&rc, &ti, 32)) return -3; }
                if (u == 0x08) { uint64_t n; if (!bbq_read_uleb128(&rc, &n, 32)) return -3; }
                continue;
            default: return -4;                /* unknown const-expr op: never skip past it */
            }
            if (g == target) {
                if (at == want_slot) found = row;
                if (rows && at < nrows) rows[at] = row;
            }
            at++;
        }
    }
    return found;                              /* -5: the target global never appeared */
}

/* Decode the emitted global-section CONTENT back out of the artifact bytes and return the
 * funcidx sitting in slot `want_slot` of global `target` (a vtable global): the ref.func
 * operand, or -1 when the row is ref.null, or <-1 on a malformed stream. Opcode-aware —
 * a "scan to 0x0B" would misparse `i32.const 11` — and FAIL-LOUD (-4) on any const-expr
 * opcode it does not know, so a new emission shape breaks this test instead of sliding by. */
static int32_t vtable_row_funcidx(const uint8_t* buf, int len, int target, int want_slot) {
    return vtable_row_scan(buf, len, target, want_slot, NULL, 0);
}

int main(void) {
    bbq_arena a; bbq_arena_init(&a, 1 << 16);
    build_program("class P { int x; }", &a);   /* registers the units jtest_analyze reads */
    sema_ctx_t sctx; sema_init(&sctx, &a); jtest_analyze(&sctx);

    wasm_types_t wt; wasm_types_build(&wt, &sctx, NULL, 0);

    int obj_id = find_class(&sctx, "Object");
    int p_id   = find_class(&sctx, "P");
    CHECK(obj_id >= 0, "found class Object");
    CHECK(p_id  >= 0, "found class P");

    /* struct typeidx is the topological position (a supertype gets a smaller
     * index, §3.2.11) — class ids are arbitrary, so P just sits past Object. */
    CHECK(wasm_types_class_typeidx(&wt, obj_id) < wasm_types_class_typeidx(&wt, p_id),
          "Object typeidx < P typeidx (super-first)");

    /* ref valtype: 0x63 then s33(typeidx). Decode the heaptype with the shared
     * bbq reader's sleb128 s33 helper (don't hand-roll LEB). */
    {
        emit_wasm_ctx r = {0};
        wasm_types_emit_ref(&r, wasm_types_class_typeidx(&wt, p_id));
        CHECK(bbq_vec_len(r.code) >= 2 && r.code[0] == 0x63, "ref starts 0x63 (ref null)");
        bbq_ctx_t rc; bbq_ctx_init(&rc, r.code + 1, (size_t)bbq_vec_len(r.code) - 1);
        int32_t v = 0; bool dok = bbq_read_sleb128_s33(&rc, &v);
        CHECK(dok && v == wasm_types_class_typeidx(&wt, p_id), "ref heaptype decodes to P's typeidx");
        bbq_ctx_free(&rc);
        bbq_vec_free(r.code);
    }

    /* §10.7: long[] is represented by the synthesized LongArray overlay (a class); its
     * concrete (array i64) backing is that overlay's `data` field type, emitted during
     * Pass 1 (class fields) — so it is a SIGNATURE array, its typeidx landing WITHIN the
     * signature-array region (after the structs and the rec-group header types), before the
     * func types. Every primitive-array backing is now an overlay data field this way, so none
     * are body-local. */
    CHECK(wt.num_sig_arrays >= 0, "signature-array region frozen by wasm_types_build");
    int32_t long_arr = wasm_types_array_typeidx(&wt, jt_prim(JT_LONG));
    int32_t hdr = wasm_hdr_type_count();
    CHECK(long_arr >= wt.num_classes + hdr && long_arr < wt.num_classes + hdr + wt.num_sig_arrays,
          "long[]'s (array i64) backing is a signature array (the LongArray overlay's data field)");

    emit_wasm_ctx sec = {0};
    wasm_types_emit_section(&wt, &sec);
    int slen = (int)bbq_vec_len(sec.code);

    /* section id 1; the content is the rectype vec: ONE rec group (the mutually-
     * recursive GC types — structs, the global vtable, the SIGNATURE arrays)
     * followed by the func types as STANDALONE top-level types (so they match a
     * host's structurally-equal functype at import, §3.3.10), then the BODY-LOCAL
     * arrays as standalone types past the func types. Pin the rec group + its member
     * count = structs + vtable + signature arrays only (a func type's index must not
     * float on the body-array count, which only finalizes during codegen). */
    CHECK(slen > 3 && sec.code[0] == 0x01, "type section id is 1");
    {
        emit_wasm_ctx rg = {0};
        ew_byte(&rg, 0x4E);                  /* rec group */
        /* structs + the header types (vtable, ifaceIds-array, factory functype) + signature arrays */
        ew_u32(&rg, (uint32_t)(wt.num_classes + hdr + wt.num_sig_arrays));
        CHECK(contains(sec.code, slen, rg.code, (int)bbq_vec_len(rg.code)),
              "rec group holds structs + header types + signature arrays; func types and body arrays are standalone");
        bbq_vec_free(rg.code);
    }
    /* §20.3.6 the Class factory field: a plain funcref, the LAST of Class's three synthesized
     * trailing fields. It is not a rec-group header type — a `[] -> [(ref null Object)]` type inside
     * the rec group would reference Object recursively and so be a DIFFERENT defined type from the
     * standalone functype every `$newInstance` carries. The field being funcref means ClassConstruct
     * ref.casts it to the factory functype before call_ref. */
    CHECK(wasm_class_factory_field_index(&wt) == wasm_class_ifaceids_field_index(&wt) + 1,
          "Class's `factory` is its last synthesized trailing field");

    /* P's struct subtype: open-sub of Object with THREE fields — the inherited Class
     * header (a (ref null Class), mut) at index 0, then the inherited Object.hash
     * (i32, mut) identity-hash field, then x (i32, mut). Every object's field 0 IS its
     * java.lang.Class (the unified runtime type); field 1 is the §20.1.4 identity hash. */
    {
        emit_wasm_ctx want = {0};
        ew_byte(&want, 0x50);
        ew_u32(&want, 1);
        ew_u32(&want, (uint32_t)wasm_types_class_typeidx(&wt, obj_id));   /* supertype = Object */
        ew_byte(&want, 0x5F);
        ew_u32(&want, 3);
        wasm_types_emit_ref(&want, wasm_class_reflect_typeidx(&wt));  /* field 0: (ref null Class) */
        ew_byte(&want, 0x01);                                /* …mut */
        ew_byte(&want, 0x7F); ew_byte(&want, 0x01);          /* field 1: hash (i32) mut */
        ew_byte(&want, 0x7F); ew_byte(&want, 0x01);          /* field 2: x (i32) mut */
        CHECK(contains(sec.code, slen, want.code, (int)bbq_vec_len(want.code)),
              "P struct subtype: Class header + inherited hash + one mutable i32 field");
        bbq_vec_free(want.code);
    }

    /* int[] array type: 0x5E (array) 0x7F (i32) 0x01 (mut) */
    {
        const uint8_t arr[] = { 0x5E, 0x7F, 0x01 };
        CHECK(contains(sec.code, slen, arr, 3), "int[] array type: 5E 7F 01");
    }

    bbq_vec_free(sec.code);
    wasm_types_free(&wt);
    bbq_arena_free(&a);

    /* inheritance field layout: A{int a} ; B extends A {int b}. A's own field
     * `a` sits at struct index 0; B's own field `b` sits AFTER A's inherited
     * field, at struct index 1. So field_base(A)=0, field_base(B)=1 — the
     * absolute index the burg must add to each field's class-local index. */
    {
        bbq_arena b; bbq_arena_init(&b, 1 << 16);
        build_program(
            "class A { int a; } class B extends A { int b; }", &b);
        sema_ctx_t s2; sema_init(&s2, &b); jtest_analyze(&s2);
        wasm_types_t wt2; wasm_types_build(&wt2, &s2, NULL, 0);

        int a_id = find_class(&s2, "A");
        int b_id = find_class(&s2, "B");
        CHECK(a_id >= 0 && b_id >= 0, "found classes A and B");
        /* Object contributes TWO inherited fields at the base: the Class header (index 0)
         * and the §20.1.4 identity-hash field (index 1). */
        CHECK(wasm_types_field_base(&wt2, a_id) == 2, "field_base(A) == 2 (header + Object.hash)");
        CHECK(wasm_types_field_base(&wt2, b_id) == 3, "field_base(B) == 3 (header + hash + A's one field)");

        wasm_types_free(&wt2);
        sema_destroy(&s2);
        bbq_arena_free(&b);
    }

    /* Override-aware vtable slots: B.m overrides A.m, so it must reuse A.m's
     * slot — else a virtual call through an A reference hits the wrong method. */
    {
        bbq_arena b; bbq_arena_init(&b, 1 << 16);
        build_program(
            "class A { int m(){ return 1; } } class B extends A { int m(){ return 2; } }", &b);
        sema_ctx_t s2; sema_init(&s2, &b); jtest_analyze(&s2);
        wasm_types_t wt2; wasm_types_build(&wt2, &s2, NULL, 0);
        int a_id = find_class(&s2, "A"), b_id = find_class(&s2, "B");
        int am = -1, bm = -1;
        const sema_class_t* ca = sema_get_class(&s2, a_id);
        for (int i = 0; i < (int)bbq_vec_len(ca->methods); i++)
            if (ca->methods[i].name && !strcmp(ca->methods[i].name, "m")) am = i;
        const sema_class_t* cb = sema_get_class(&s2, b_id);
        for (int i = 0; i < (int)bbq_vec_len(cb->methods); i++)
            if (cb->methods[i].name && !strcmp(cb->methods[i].name, "m")) bm = i;
        CHECK(am >= 0 && bm >= 0, "found A.m and B.m");
        CHECK(wasm_vtable_slot(&wt2, a_id, am) == wasm_vtable_slot(&wt2, b_id, bm),
              "override: B.m reuses A.m's vtable slot");
        wasm_types_free(&wt2); sema_destroy(&s2); bbq_arena_free(&b);
    }

    /* THE DEFUNCTIONALIZED CALL GRAPH, MATERIALIZED (spec §7/§10 — the assembler side).
     *
     * The ANALYSIS-side half is pinned in test_sema: sema_resolve_virtual enumerates a call
     * site's complete target set from the class table alone, closed-world, no analysis.
     * Runtime dispatch through these rows is pinned in test_exec. THIS pins the artifact:
     * each class's vtable global carries "override-resolved ref.func into each slot"
     * (wasm_types.h) as §3.3.10 constants — i.e. the emitted module CONTAINS the call
     * graph, before any analysis exists. The VFG paper (ISMM'13) must DISCOVER these rows
     * as pointer-analysis output ("full points-to sets for function pointers only"; its
     * clone repair re-runs on "extra targets for a function pointer"); here they are
     * compile-time bytes, and this test decodes them back OUT of the bytes and checks
     * them against the ONE rule — two independent walks (resolve_slots' same_sig chain vs
     * sema_resolve_virtual) that must agree, or dispatch and devirt have diverged. */
    {
        bbq_arena b2; bbq_arena_init(&b2, 1 << 16);
        build_program(
            "class A { int m(){ return 1; } int n(){ return 9; } }"
            " class B extends A { int m(){ return 2; } }"
            " class C extends B { }"
            " class D extends A { int m(){ return 4; } }", &b2);
        sema_ctx_t s3; sema_init(&s3, &b2); jtest_analyze(&s3);
        wasm_types_t wt3; wasm_types_build(&wt3, &s3, NULL, 0);
        int a_id = find_class(&s3, "A"), b_id = find_class(&s3, "B"),
            c_id = find_class(&s3, "C"), d_id = find_class(&s3, "D");
        int am = find_method(&s3, a_id, "m"), an = find_method(&s3, a_id, "n"),
            bm = find_method(&s3, b_id, "m"), dm = find_method(&s3, d_id, "m");
        CHECK(a_id >= 0 && c_id >= 0 && am >= 0 && an >= 0 && bm >= 0 && dm >= 0,
              "call-graph hierarchy resolves");

        /* The vsig IS the dispatch key: every override shares ONE global slot; a distinct
         * vsig gets its own. (This is what makes the target set a per-slot column.) */
        int slot = wasm_vtable_slot(&wt3, a_id, am);
        CHECK(slot >= 0 && wasm_vtable_slot(&wt3, b_id, bm) == slot
                        && wasm_vtable_slot(&wt3, d_id, dm) == slot,
              "one vsig, ONE slot: A.m / B.m / D.m all dispatch through it");
        CHECK(wasm_vtable_slot(&wt3, a_id, an) != slot,
              "a distinct vsig gets a distinct slot");

        emit_wasm_ctx g = {0};
        wasm_types_emit_globals_content(&wt3, &s3, &g);
        int glen = (int)bbq_vec_len(g.code);
        struct { int cls, rcls, rmid; const char* label; } rows[] = {
            { a_id, a_id, am, "artifact: vtable[A][slot m] = ref.func A.m (the declaration)" },
            { b_id, b_id, bm, "artifact: vtable[B][slot m] = ref.func B.m (the override)" },
            { c_id, b_id, bm, "artifact: vtable[C][slot m] = ref.func B.m — the INHERITED "
                              "override, materialized although C declares nothing" },
            { d_id, d_id, dm, "artifact: vtable[D][slot m] = ref.func D.m (the sibling)" },
        };
        for (int i = 0; i < 4; i++) {
            int rc = -1, rm = -1;
            CHECK(sema_resolve_virtual(&s3, rows[i].cls, a_id, am, &rc, &rm)
                  && rc == rows[i].rcls && rm == rows[i].rmid,
                  "the rule resolves this row (precondition; per-class pins in test_sema)");
            int32_t want = wasm_func_index(&wt3, rc, rm);
            int32_t got  = vtable_row_funcidx(g.code, glen,
                              wasm_vtable_global_index(&wt3, rows[i].cls), slot);
            CHECK(got >= 0 && got == want, rows[i].label);
        }
        bbq_vec_free(g.code);
        wasm_types_free(&wt3); sema_destroy(&s3); bbq_arena_free(&b2);
    }

    /* ── Every occupied vtable slot is filled ─────────────────────────────────────────────
     *
     * The fill reads sema's §15.11.4.4 dispatch table. The oracle here is independent: for
     * every class and every slot, the slot is OCCUPIED iff that class has a virtual method
     * with the slot's signature (§15.11.4.4's descriptor match — a slot IS a signature, and
     * classes with no subtype relation share one). An occupied slot holding ref.null is a
     * dispatch that traps at run time, with nothing in the trap naming the class.
     *
     * A search is legitimate HERE and not in the emitter — the test is the oracle, and its
     * whole value is being derived differently from the thing it checks. */
    for (int pass = 0; pass < 2; pass++) {
        int mode = pass ? SEMA_MODE_RUNTIME : SEMA_MODE_WHOLE;
        bbq_arena vb; bbq_arena_init(&vb, 1 << 16);
        build_program(
            "interface IX { int p(); }\n"
            "interface IY extends IX { int q(); }\n"
            "class VA { public String toString(){ return \"a\"; } }\n"
            "class VB extends VA implements IY { public int p(){ return 1; }\n"
            "                                    public int q(){ return 2; } }\n"
            "class VC { public String toString(){ return \"c\"; } }", &vb);
        sema_ctx_t vs; sema_init(&vs, &vb);
        vs.num_library_classes = jtest_last_nlib;
        vs.mode = mode;
        jtest_analyze(&vs);
        wasm_types_t vw; wasm_types_build(&vw, &vs, NULL, 0);

        emit_wasm_ctx g = {0};
        wasm_types_emit_globals_content(&vw, &vs, &g);
        int glen = (int)bbq_vec_len(g.code);

        int nslots = wasm_vtable_len(&vw);
        int ncls   = (int)bbq_vec_len(vs.classes);
        int32_t* rows = (int32_t*)malloc((size_t)(nslots > 0 ? nslots : 1) * sizeof *rows);

        int bad = 0, checked = 0; char first[240] = {0};
        for (int ci = 0; ci < ncls; ci++) {
            const sema_class_t* c = sema_get_class(&vs, ci);
            if (c->is_interface || wasm_is_imported_class(&vw, ci)) continue;
            for (int s = 0; s < nslots; s++) rows[s] = -1;
            vtable_row_scan(g.code, glen, wasm_vtable_global_index(&vw, ci), -1, rows, nslots);
            /* Every declaration a call site can name on a `ci` receiver: any method of any
             * supertype (sema owns §4.10.2 — the test does not re-walk the hierarchy). */
            for (int ai = 0; ai < ncls; ai++) {
                if (!sema_ref_is_subtype(&vs, ci, ai)) continue;
                const sema_class_t* a = sema_get_class(&vs, ai);
                for (int mi = 0; mi < (int)bbq_vec_len(a->methods); mi++) {
                    const sema_method_t* m = &a->methods[mi];
                    if (!sema_is_virtual_method(m)) continue;
                    int rcls = -1, rmid = -1;
                    if (!jls_locate_method(&vs, ci, m, &rcls, &rmid))
                        continue;                       /* abstract for ci: ref.null is right */
                    int32_t slot = wasm_vtable_slot(&vw, ai, mi);
                    if (slot < 0 || slot >= nslots) continue;
                    checked++;
                    /* The two things `struct.get vtable; array.get; ref.cast; call_ref` needs:
                     * the slot holds the resolved implementation, and that implementation's
                     * functype is the one the CALL SITE casts to — a mismatch is a runtime
                     * cast failure at the dispatch, with nothing naming the method. */
                    int32_t want = wasm_func_index(&vw, rcls, rmid);
                    int32_t ftd  = wasm_functype_idx(&vw, ai, mi);
                    int32_t fti  = wasm_functype_idx(&vw, rcls, rmid);
                    if (rows[slot] == want && ftd == fti) continue;
                    bad++;
                    if (!first[0])
                        snprintf(first, sizeof first,
                                 "%s receiver, %s.%s -> %s.%s: slot %d holds %d (want %d), "
                                 "cast-to type %d vs occupant type %d",
                                 c->name ? c->name : "?", a->name ? a->name : "?",
                                 m->name ? m->name : "?",
                                 sema_get_class(&vs, rcls)->name, m->name ? m->name : "?",
                                 (int)slot, (int)rows[slot], (int)want, (int)ftd, (int)fti);
                }
            }
        }
        free(rows);
        if (bad) printf("    mode %d: %d bad rows, first: %s\n", mode, bad, first);
        CHECK(checked > 0 && bad == 0, pass
              ? "RUNTIME: every dispatchable slot holds the resolved impl at the cast's type"
              : "WHOLE: every dispatchable slot holds the resolved impl at the cast's type");

        bbq_vec_free(g.code);
        wasm_types_free(&vw); sema_destroy(&vs); bbq_arena_free(&vb);
    }

    /* ── The PLUGIN import section ────────────────────────────────────────────────────────
     *
     * The import list is DERIVED from sema's JLS §13.1 reference set and §8.4.6.1 dispatch
     * table. Three properties, each failing for its own reason, because the failure modes are
     * different and a single end-to-end "does it validate" cannot tell them apart:
     *
     *   RESOLVES  every vtable slot occupant has a funcidx. Violated ⟹ the emitted ref.func
     *             names nothing and the module fails validation with "unknown function".
     *   MINIMAL   a library class neither referenced nor subclassed contributes nothing.
     *             Violated ⟹ §13.4.5's guarantee is false of the binary: adding a member to
     *             lib/java invalidates plugins built against an older jre.
     *   TYPED     each import's functype index is inside the func-type block. Violated ⟹
     *             "type mismatch", the import declared against a type that isn't its own.
     */
    {
        bbq_arena pb; bbq_arena_init(&pb, 1 << 16);
        /* Overrides nothing and calls nothing: every slot in its vtable is occupied by an
         * inherited library method that no reference names, so the dispatch table is the only
         * thing that can supply those funcrefs. */
        build_program("class PImp { int v; }", &pb);
        sema_ctx_t ps; sema_init(&ps, &pb);
        ps.num_library_classes = jtest_last_nlib;
        ps.mode = SEMA_MODE_PLUGIN;
        jtest_analyze(&ps);
        wasm_types_t pw; wasm_types_build(&pw, &ps, NULL, 0);

        int nimp = wasm_import_count(&pw);
        CHECK(nimp > 0, "a plugin imports something (precondition)");

        /* RESOLVES — the direct pin for "unknown function". Every occupant of every slot in
         * every vtable this module emits must have a funcidx: defined here, or imported. */
        int unresolved = 0, checked = 0;
        for (int i = 0; i < sema_vtarget_count(&ps); i++) {
            sema_vtarget_ent_t v = sema_vtarget_at(&ps, i);
            if (wasm_is_imported_class(&pw, v.class_id)) continue;   /* jre emits that vtable */
            if (v.impl_class < 0) continue;                          /* abstract: ref.null slot */
            checked++;
            if (wasm_func_index(&pw, v.impl_class, v.impl_method) < 0) unresolved++;
        }
        CHECK(checked > 0 && unresolved == 0,
              "every vtable slot occupant of a locally-emitted class resolves to a funcidx");

        /* MINIMAL — §13.4.5 at the consumer. Vector is parsed (javelinac globs all of
         * lang/util/io) and this plugin neither names nor extends it. */
        int vec_id = find_class(&ps, "Vector");
        int from_vector = 0;
        for (int i = 0; i < nimp; i++) {
            sema_func_ent_t e = wasm_import_at(&pw, i);
            if (e.class_id != vec_id) continue;
            from_vector++;
            if (from_vector <= 3)
                printf("    unreferenced-import: Vector.%s\n",
                       sema_get_class(&ps, e.class_id)->methods[e.method_id].name);
        }
        CHECK(vec_id >= 0 && from_vector == 0,
              "an unreferenced, unsubclassed library class contributes no imports (JLS 13.4.5)");

        /* TYPED — each import's functype must land in the func-type block, after the defined
         * functions and before the body-local arrays. An index outside it is a type mismatch
         * at instantiation, not a missing function. */
        int32_t ft_base = pw.num_classes + wasm_hdr_type_count() + pw.num_sig_arrays
                        + sema_func_count(&ps);
        int mistyped = 0;
        for (int i = 0; i < nimp; i++) {
            sema_func_ent_t e = wasm_import_at(&pw, i);
            int32_t ft = wasm_import_functype_idx(&pw, e.class_id, e.method_id);
            if (ft < ft_base || ft >= ft_base + 8 + nimp) mistyped++;   /* +8: clinit/tag/helper slack */
        }
        CHECK(mistyped == 0, "every import's functype index lies in the func-type block");

        wasm_types_free(&pw); sema_destroy(&ps); bbq_arena_free(&pb);
    }

    sema_destroy(&sctx);
    return TEST_SUMMARY("test_wasm_types");
}
