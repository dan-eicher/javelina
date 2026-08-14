// test_wast.c — drive the official WebAssembly testsuite (.wast) against the
// readers. A .wast is an S-expression script of commands; this is a small,
// comment/string-aware S-expr extractor (NOT a full .wat parser) that pulls out
// module commands + their expected outcome:
//   (module binary "…")                  -> binary reader must SUCCEED
//   (assert_malformed (module binary …))  -> binary reader must FAIL
//   (assert_invalid   (module binary …))  -> binary reader must SUCCEED (valid
//                                            bytes, invalid types — validator's job)
// TEXT modules `(module …)` are fed (by raw source span) to the .wat reader, with
// the same expected outcome (assert_malformed → must fail, else → must parse). This
// validates the binary reader (P0–P3) AND the .wat text reader (P4/P5) against the
// real corpus — not hand-authored fixtures. `(module quote …)` is still skipped.

#include "jav_reader.h"
#include "jav_validate_module.h"
#include "wat_parser.h"          // the .wat text reader (exercise it on the real corpus)
#include "jav_writer.h"          // jav_module_write: module → .wasm bytes (for the §7 text gate)
#include "jav_load.h"            // the loader entry: bytes → §5 decode → §7 verdict + jav_err_t
#include "bbq_arena.h"
#include "wast_sexpr.h"          // the shared .wast S-expr reader (Node + helpers)
#include "wast_exec.h"           // the c-lite execution runner (separate TU: type-world split)
#include "jav_ttree.h"           // PIN B-4: the tier-2 sweep's counters
#include "jav_eqsat.h"           // PIN B-1 (tier-3): the eqsat pass's counters
#include "wat_check.h"           // PIN A-4: water's own §7.6, against the engine's
#include "jav_view_nav.h"        // PIN A-5: the span index that aligns the two walks
#include "jav_module_index.h"    // PIN A-5: jav_module_index / jav_module_tctx
#include "jav_module_struct.h"   // PIN A-5: the §5.5 gate the index reads counts through
#include "jav_module_validate.h" // PIN A-5: fills body_locals, which the tctx needs
#include "jav_sigtab.h"          // PIN A-5: jav_sclass_of_valtype
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


// ── .wat text-module path: feed the raw (module …) span to the real .wat reader ──
// Each module parse gets its own ctx and arena. The mnemonic table is generated
// into the binary (gen/wat_mnemonics.h), so there is nothing per-parse to load —
// this used to keep a persistent ctx holding instructions.toml precisely because
// re-reading it per module (5000×) turned the run into a multi-minute hang.
static int g_wat_ok, g_wat_bad, g_wat_excl;
static char g_wat_frag[64];          // WAST_CAT: source fragment at the parser's furthest point
static const char *g_curfile = "";   // basename of the .wast file currently being scanned
// The module command's source span, for the §7 differential's report: which of a
// file's hundred assert_invalid modules diverged is the whole question, and the file
// name cannot answer it.
static const char *g_modsrc = ""; static int g_modlen = 0;

static void free_parse_vecs(wat_ctx_t *c) {
    for (int i = 0; i < SP_N; i++) {
        for (int j = 0; j < (int)bbq_vec_len(c->sp[i]); j++) free(c->sp[i][j]);
        bbq_vec_free(c->sp[i]);
        for (int j = 0; j < (int)bbq_vec_len(c->sp_imp[i]); j++) free(c->sp_imp[i][j]);
        bbq_vec_free(c->sp_imp[i]);
    }
    for (int i = 0; i < (int)bbq_vec_len(c->xtypes); i++) {
        free(c->xtypes[i].params.items); free(c->xtypes[i].results.items);
    }
    bbq_vec_free(c->xtypes);
    wat_type_fields_free(c);                // §6.6.2 per-type struct field-name space
    for (int i = 0; i < (int)bbq_vec_len(c->locals); i++) free(c->locals[i]);
    bbq_vec_free(c->locals);
    for (int i = 0; i < (int)bbq_vec_len(c->labels); i++) free(c->labels[i]);
    bbq_vec_free(c->labels);
    bbq_vec_free(c->ins); bbq_vec_free(c->iexports); bbq_vec_free(c->iimports);
    bbq_vec_free(c->el_funcs); bbq_vec_free(c->el_exprs);
}

// Parse a standalone (module …) text span with the real .wat reader; 1 if it parses.
static int wat_parses(const char *src, int len) {
    wat_ctx_t ctx; memset(&ctx, 0, sizeof ctx);
    bbq_arena_init(&ctx.arena, 64 * 1024);
    peg_state p;
    ctx.pass = 1;
    wat_parser_init(&p, src, len); p.user_data = &ctx;
    int ok = wat_parser_parse(&p);
    if (ok) {
        ctx.pass = 2; ctx.mod = calloc(1, sizeof *ctx.mod);
        wat_parser_init(&p, src, len); p.user_data = &ctx;
        ok = wat_parser_parse(&p);
    }
    g_wat_frag[0] = 0;                            // capture the construct at the failure point
    if (!ok) {
        const char *fp = p.furthest.pos ? p.furthest.pos : src;
        const char *lim = src + len; int k = 0;
        for (const char *q = fp; q < lim && k < (int)sizeof g_wat_frag - 1; q++) {
            char ch = *q;
            if (ch==' '||ch=='\t'||ch=='\n'||ch=='\r') { if (k && g_wat_frag[k-1] != ' ') g_wat_frag[k++]=' '; }
            else g_wat_frag[k++] = ch;
        }
        g_wat_frag[k] = 0;
    }
    bbq_arena_free(&ctx.arena);
    wat_wbufs_free(&ctx);                         // emit-scratch pool (incl. backtrack-abandoned buffers)
    wat_scratch_free(&ctx);                       // ctx-rooted scratch vecs (same abandonment story)
    wat_assembly_free(&ctx);                       // abandoned module-assembly state (deep)
    free_parse_vecs(&ctx);
    if (ctx.mod) { jav_module_free(ctx.mod); free(ctx.mod); }   // calloc'd → zeroed → free-safe even on partial
    return ok;
}

static int g_ok, g_bad;
static const char *g_msg = "";

// Resolve a module-bearing node to its effective .wat source for the text gates. Handles:
//   plain (module …)        → the raw source span
//   (module quote "…"*)     → the decoded string (§6.6.13 bare fields wrapped in (module …))
//   (module definition $id …) → (module $id …)   — core .wast scripting; the inner module is core wasm
//   (module instance …)     → 0 (an instantiation command, NOT a module to parse/validate)
//   binary modules          → 0 (run_module handles those)
// `scratch` is a caller buffer (≥ 1<<20). Returns 1 + fills src/len, or 0 to skip.
static int module_wat_source(const Node *m, char *scratch, const char **src_out, int *len_out) {
    if (binary_strs_at(m) >= 0) return 0;
    if (m->nkids >= 2 && tok_is(m->kids[1], "instance")) return 0;
    if (m->nkids >= 2 && tok_is(m->kids[1], "definition")) {       // strip the "definition" keyword
        const char *after = m->kids[1]->tok + m->kids[1]->tlen;
        if (after > m->s1) return 0;
        int rest = (int)(m->s1 - after);
        memcpy(scratch, "(module", 7); memcpy(scratch + 7, after, rest); scratch[7 + rest] = 0;
        *src_out = scratch; *len_out = 7 + rest; return 1;
    }
    if (is_quote_module(m)) {                                      // §6.2.5 annotations are skipped by the reader
        char *txt = scratch + 8; int qn = 0;
        for (int i = 1; i < m->nkids; i++) if (m->kids[i]->is_str) qn += decode_str(m->kids[i], (uint8_t*)txt + qn);
        int k = 0; while (k < qn && strchr(" \t\n\r", txt[k])) k++;
        if (qn - k >= 7 && memcmp(txt + k, "(module", 7) == 0 && (qn - k == 7 || strchr(" \t\n\r()", txt[k+7]))) {
            *src_out = txt; *len_out = qn;                         // already a (module …) form
        } else { memcpy(scratch, "(module ", 8); txt[qn] = ')'; *src_out = scratch; *len_out = 8 + qn + 1; }  // bare fields: wrap
        return 1;
    }
    if (!m->s0 || m->s1 <= m->s0) return 0;
    *src_out = m->s0; *len_out = (int)(m->s1 - m->s0); return 1;
}

// Text (module …): feed its effective .wat source to the .wat reader; same expectation.
static void run_module_text(const Node *m, int expect_ok) {
    static char qbuf[1<<20];
    const char *src; int len;
    /* The corpus carries material beyond core 3.0. `(module instance …)` is
     * module-linking's INSTANTIATION command — not a module at all, so there is
     * nothing for the .wat reader to score. It is EXCLUDED and counted, never
     * silently dropped: an exclusion nobody can see reads exactly like a pass.
     *
     * NOT excluded, deliberately: `(module definition $id …)` — that is .wast
     * scripting around a module whose body IS core wasm, which we parse and
     * score like any other (module_wat_source strips the keyword). And binary
     * modules are the binary gate's business, scored there. */
    if (m->nkids >= 2 && tok_is(m->kids[1], "instance")) { g_wat_excl++; return; }
    if (!module_wat_source(m, qbuf, &src, &len)) return;   // binary module → the other gate
    int parsed = wat_parses(src, len);
    if (parsed == expect_ok) { g_wat_ok++; return; }
    g_wat_bad++;
    if (getenv("WAST_CAT"))                       // machine-readable: <file>\t<failing fragment>
        printf("%s\t%s\t%s\n", expect_ok?"shouldparse":"shouldreject", g_curfile, g_wat_frag);
    const char *wvmax = getenv("WAST_VMAX");
    if (getenv("WAST_V") && g_wat_bad <= (wvmax ? atoi(wvmax) : 60)) {
        int snlen = (int)(m->s1 - m->s0); if (snlen > 160) snlen = 160;
        fprintf(stderr, "  WAT MISMATCH expect=%s got=%s  msg=%.*s\n    %.*s\n",
                expect_ok?"ok":"FAIL", parsed?"ok":"FAIL",
                (g_msg[0]=='"')?(int)strlen(g_msg):0, g_msg, snlen, m->s0);
    }
}

static void run_module(const Node *m, int expect_ok) {
    int si = binary_strs_at(m);
    if (si < 0) return;                                  // not a binary module — skip (needs .wat)
    static uint8_t buf[1<<20]; int n = 0;
    for (int i = si; i < m->nkids; i++)
        if (m->kids[i]->is_str) n += decode_str(m->kids[i], buf+n);
    bbq_ctx_t c; bbq_ctx_init(&c, buf, (size_t)n);
    jav_module_t mod; memset(&mod, 0, sizeof mod);
    // read (byte decode) + the optional structural validator pass.
    int parsed = jav_module_read(&c, &mod) && bbq_at_end(&c) && jav_module_wf(&mod, NULL);
    jav_module_free(&mod);                              // owned tree, even from a rejected parse
    bbq_ctx_free(&c);
    if (parsed == expect_ok) { g_ok++; return; }
    g_bad++;
    const char *vmax = getenv("WAST_VMAX");
    if (getenv("WAST_V") && g_bad <= (vmax ? atoi(vmax) : 60)) {
        fprintf(stderr, "  MISMATCH expect=%s got=%s  msg=%.*s\n", expect_ok?"ok":"FAIL", parsed?"ok":"FAIL",
                (g_msg[0]=='"')?(int)strlen(g_msg):0, g_msg);
    }
}

// ── §7 validation gate (Phase 1): drive the loader (jav_validate_bytes — the same entry
// the wasm-c-api shim will wrap) over the module bytes and check the verdict against the
// command. kind: 0 valid, 1 assert_invalid (assert_malformed is the decoder's job, covered
// by run_module). Valid → JAV_OK; assert_invalid → rejected (MALFORMED/INVALID). The reason
// text (jav_err_str, via jav_lasterror on the shim) is the Phase-3 message-match refinement.
static int g_val_ok, g_val_bad;
static int g_val_msgbad;   // rejected correctly, but for the WRONG reason (jav_err_str ≠ .wast string)
// §"fine error-reason model": the .wast expected error is matched against jav_err_str — the
// official vocabulary. The matcher is wast_msg_matches (wast_exec.h), shared with the
// execution side so the two reason gates cannot drift apart.
#define err_matches(actual, expected_tok) wast_msg_matches((actual), (expected_tok))
static void run_module_wat76(const uint8_t *bytes, int n, int vm_accepted, jav_err_t vm_err);
static void run_module_validate(const Node *m, int kind) {
    // assert_malformed is scored here too. It used to be skipped as "the decoder's job,
    // covered by run_module" — but run_module drives the OWNING reader plus jav_module_wf,
    // and the engine loads through the c-lite index, which never saw a malformed image.
    // The §5.5 conditions are the loader's to enforce whichever tree it parses into.
    int si = binary_strs_at(m);
    if (si < 0) return;                                  // binary modules only (text needs the .wat path)
    static uint8_t vbuf[1<<20]; int n = 0;
    for (int i = si; i < m->nkids; i++)
        if (m->kids[i]->is_str) n += decode_str(m->kids[i], vbuf+n);
    jav_err_t err = JAV_E_NONE;
    int accepted = (jav_validate_bytes(vbuf, (size_t)n, &err) == JAV_OK);
    run_module_wat76(vbuf, n, accepted, err);            // PIN A-4
    int want_accept = (kind == 0);
    if (accepted == want_accept) {
        if (!want_accept && !err_matches(jav_err_str(err), g_msg)) {   // right verdict, wrong reason
            g_val_msgbad++;
            if (getenv("WAST_VV")) fprintf(stderr, "  §7 MSG-MISMATCH %s expect=%s got=\"%s\"\n", g_curfile, g_msg, jav_err_str(err));
            return;
        }
        g_val_ok++; return;
    }
    g_val_bad++;
    const char *vmax = getenv("WAST_VMAX");
    if (getenv("WAST_VV") && g_val_bad <= (vmax ? atoi(vmax) : 60))
        fprintf(stderr, "  §7 MISMATCH [%s] %s (%d bytes) expect=%s got=%s reason=%s\n",
                kind ? "invalid" : "valid", g_curfile, n, want_accept?"accept":"reject",
                accepted?"accept":"reject", jav_err_str(err));
}

// ── PIN A-4: water's §7.6 against the engine's, over the same bytes ──────────
// water transcribes §7.6 itself and links no engine translation unit; what the two
// share is opgen's generated per-opcode transfer table. This is the check on that
// arrangement — two independent readings of one published algorithm, run over every
// module in the corpus.
//
// PIN A-7: both directions. With §3.5 in (A7), water's verdict is the whole of §7, so
// the gate is an IDENTITY — every module, accepted or rejected, must get the same
// answer from both. A module the engine rejects that water accepts is the dangerous
// direction (water would write .wat for something invalid); the converse is water
// being wrong about a good module. Both are counted here.
//
// A module the OWNING reader cannot even read is not scored: §5 decode is
// run_module's gate, over the same bytes, and it is already 810/0.
// ── PIN A-5: water's §7.6 producer edges against jav_ttree_build's tree ──────
//
// Two independently-written walks over one instruction stream. water's iterates the
// OWNING reader's jav_instr_t and has no byte positions; the tier-2 builder decodes
// BYTES and its `pc` is one. The shared coordinate is the c-lite span index, which
// this runner is holding anyway — the VM's module index is built out of it.
//
// The correspondence is positional: the k-th Instr capture in §5 pre-order is the k-th
// row water recorded, because both readers are generated from wasm.bbq and take the
// same switch arms over the same bytes. That is an assumption, so the count is checked
// before anything is compared.
static int g_a5_ok, g_a5_bad, g_a5_bodies, g_a5_unaligned;
// jav_ttree_stats is a global accumulator whose intended producer is the JIT compiling
// bodies. This differential is a SECOND producer of the same counters, so it tracks
// exactly what it added and the sweep's report subtracts it. The alternative — teaching
// the builder to stop counting on request — would be putting test scaffolding in the
// engine to keep a test from breaking another test.
static jav_ttree_stats_t g_a5_delta;

// water's row for a given flat §5 ordinal, or NULL.
static const wat_info_t *a5_row(const wat_body_t *wb, uint32_t seq) {
    for (uint32_t i = 0; i < wb->ninfo; i++) if (wb->info[i].seq == seq) return &wb->info[i];
    return NULL;
}

static void a5_compare(const wat_body_t *wb, const jav_ttree_t *t, const char *what) {
    for (uint32_t r = 0; r < t->nregions; r++)
        for (uint32_t k = 0; k < t->regions[r].nroots; k++) {
            const jav_tnode_t *stack[256]; int sp = 0;
            stack[sp++] = t->regions[r].roots[k];
            while (sp) {
                const jav_tnode_t *nd = stack[--sp];
                if (nd->seq == JAV_TNODE_NO_SEQ) continue;   // a carried leaf: no instruction
                const wat_info_t *info = a5_row(wb, nd->seq);
                // The tier-2 tree does not model a VARIADIC instruction's argument
                // group as children — it turns that group into roots and keeps only
                // what its signature fixes — so its kids are a SUFFIX of the operands
                // water popped. Both walks pop from the top, so the two align from the
                // RIGHT: ttree kid j is water operand (noperands - nkids + j).
                int shift = (info && info->noperands >= nd->nkids)
                          ? (int)info->noperands - (int)nd->nkids : 0;
                for (uint8_t j = 0; j < nd->nkids; j++) {
                    const jav_tnode_t *kid = nd->kids[j];
                    if (sp < 256) stack[sp++] = kid;
                    if (kid->seq == JAV_TNODE_NO_SEQ) continue;   // carried, not an edge
                    uint32_t wj = (uint32_t)(shift + j);
                    const jav_instr_t *wp = (info && wj < info->noperands) ? info->producer[wj] : NULL;
                    // water's producer is an instruction POINTER; find its row by
                    // identity, then compare that row's ordinal with the tree's.
                    int agree = 0; uint32_t wseq = 0xffffffffu;
                    for (uint32_t q = 0; wp && q < wb->ninfo && !agree; q++)
                        if (wb->info[q].in == wp) { wseq = wb->info[q].seq; agree = (wseq == kid->seq); }
                    if (agree) g_a5_ok++;
                    else {
                        g_a5_bad++;
                        const char *vmax = getenv("WAST_VMAX");
                        if (getenv("WAST_VV") && g_a5_bad <= (vmax ? atoi(vmax) : 20))
                            fprintf(stderr, "  A-5 EDGE DIFFERS: %s %s op 0x%02x seq=%u kid %u/%u "
                                            "ttree-kid-seq=%u water-nops=%u water-seq=%d row=%s\n",
                                    g_curfile, what, nd->pc ? nd->pc[0] : 0, nd->seq, j, nd->nkids,
                                    kid->seq, info ? info->noperands : 0,
                                    wp ? (int)wseq : -1, info ? "yes" : "MISSING");
                    }
                }
            }
        }
}

// One body: build the tier-2 tree over the same bytes and compare its edges with the
// rows water just produced. The per-FUNCTION half of the tier-2 context is the locals
// validation decoded plus this function's own results (§7.6's outermost frame) — the
// same fill jav_instance.c does at its compile site.
static void a5_body(const jav_modidx_t *vmod, jav_tctx_t *tcx, bbq_arena *ta,
                    const bbq_field_capture *ventries, const uint8_t *bytes,
                    uint32_t nimp, uint32_t d, const wat_body_t *wb) {
    const bbq_field_capture *entry = &ventries->children[d];
    const bbq_field_capture *expr  = jav_view_field(jav_view_field(entry, "body"), "body");
    if (!expr) return;
    const jav_functype_t *sig = &vmod->func_sigs[nimp + d];
    jav_valtype_t *locals = vmod->body_locals[d];
    uint32_t nloc = (uint32_t)bbq_vec_len(locals);
    uint8_t *lcls = nloc ? (uint8_t *)bbq_arena_alloc(ta, nloc) : NULL;
    for (uint32_t i = 0; i < nloc; i++) lcls[i] = jav_sclass_of_valtype(locals[i]);
    uint8_t *rcls = sig->nresults ? (uint8_t *)bbq_arena_alloc(ta, sig->nresults) : NULL;
    for (uint32_t i = 0; i < sig->nresults; i++) rcls[i] = jav_sclass_of_valtype(sig->results[i]);
    tcx->local_class = lcls;  tcx->nlocals  = nloc;
    tcx->result_class = rcls; tcx->nresults = sig->nresults;

    const uint8_t *code = bytes + expr->start_offset;
    size_t code_len = expr->end_offset - expr->start_offset;
    bbq_ctx_t cc; bbq_ctx_init(&cc, code, code_len);
    bbq_arena tra; bbq_arena_init(&tra, 64 * 1024);
    jav_ttree_t tt; memset(&tt, 0, sizeof tt);
    jav_ttree_stats_t pre = *jav_ttree_stats();
    int built = jav_ttree_build(cc, tcx, &tra, &tt);    // a decline is not a divergence
    const jav_ttree_stats_t *post = jav_ttree_stats();
    g_a5_delta.bodies_built    += post->bodies_built    - pre.bodies_built;
    g_a5_delta.bodies_declined += post->bodies_declined - pre.bodies_declined;
    g_a5_delta.nodes           += post->nodes           - pre.nodes;
    g_a5_delta.nodes_carried   += post->nodes_carried   - pre.nodes_carried;
    if (built) { a5_compare(wb, &tt, "func"); g_a5_bodies++; }
    bbq_arena_free(&tra);
}

static int g_wat76_ok, g_wat76_bad;
static void run_module_wat76(const uint8_t *bytes, int n, int vm_accepted, jav_err_t vm_err) {
    bbq_ctx_t c; bbq_ctx_init(&c, bytes, (size_t)n);
    jav_module_t mod; memset(&mod, 0, sizeof mod);
    int read_ok = jav_module_read(&c, &mod) && bbq_at_end(&c);
    bbq_ctx_free(&c);
    if (!read_ok) { jav_module_free(&mod); return; }

    bbq_arena a; bbq_arena_init(&a, 256 * 1024);
    jav_err_t err = JAV_E_NONE;
    wat_check_ctx_t *wcx = wat_check_ctx_build(&mod, &a, &err);
    int bad = 0; uint32_t bad_fn = 0; const jav_instr_t *bad_in = NULL;
    jav_err_t bad_err = err;
    int wat_accepted = 1;
    if (!wcx) {
        wat_accepted = 0;
    } else if (!wat_check_module(wcx, &a, &bad_err)) {
        wat_accepted = 0;                                  // §3.5 rejected it (A7)
    } else {
        // PIN A-5's other side: the VM's own index over the SAME bytes, which is what
        // jav_ttree_build's context is projected from and what carries the spans that
        // align the two walks. Built only for a module the engine accepted, because a
        // tier-2 tree is only defined for one.
        bbq_arena va, ta; bbq_capture_metadata vmeta; jav_modidx_t vmod;
        const bbq_field_capture *ventries = NULL; jav_tctx_t tcx; int a5 = 0;
        if (vm_accepted) {
            bbq_arena_init(&va, 0);
            vmeta = jav_view_module(bytes, (size_t)n, &va);
            jav_err_t ve;
            if (vmeta.success && jav_module_struct(vmeta.root, bytes) == JAV_E_NONE &&
                jav_module_index(vmeta.root, bytes, &va, &vmod) &&
                jav_module_validate(vmeta.root, bytes, &vmod, &ve) == JAV_OK) {
                ventries = jav_view_section_array(vmeta.root, 10, "entries", bytes);
                bbq_arena_init(&ta, 8 * 1024);
                tcx = jav_module_tctx(&vmod, &ta);
                a5 = 1;
            } else {
                bbq_arena_free(&va);
            }
        }
        // The code section's entries are the DEFINED functions, which follow the
        // imported ones in the function index space (§5.5.5).
        uint32_t nimp = 0;
        for (size_t i = 0; i < mod.sections.count; i++)
            if (mod.sections.items[i].id == 2) {
                const jav_import_section_t *is = &mod.sections.items[i].body.u.case_2;
                for (size_t k = 0; k < is->imports.count; k++)
                    if (is->imports.items[k].desc.kind == 0x00) nimp++;
            }
        for (size_t i = 0; i < mod.sections.count && !bad; i++) {
            if (mod.sections.items[i].id != 10) continue;
            const jav_code_section_t *cs = &mod.sections.items[i].body.u.case_10;
            for (size_t d = 0; d < cs->entries.count; d++) {
                wat_body_t r;
                if (!wat_check_body(wcx, nimp + (uint32_t)d, &cs->entries.items[d].body, &a, &r)) {
                    bad = 1; bad_fn = nimp + (uint32_t)d; bad_in = r.fail; bad_err = r.err;
                    break;
                }
                if (a5 && ventries && d < jav_view_nchild(ventries))
                    a5_body(&vmod, &tcx, &ta, ventries, bytes, nimp, (uint32_t)d, &r);
            }
        }
        if (bad) wat_accepted = 0;
        if (a5) { jav_modidx_free_bodies(&vmod); bbq_arena_free(&ta); bbq_arena_free(&va); }
    }
    if (wat_accepted == vm_accepted) g_wat76_ok++;
    else {
        g_wat76_bad++;
        const char *vmax = getenv("WAST_VMAX");
        if (getenv("WAST_VV") && g_wat76_bad <= (vmax ? atoi(vmax) : 60))
            fprintf(stderr, "  wat §7 DIFFERS: %s engine=%s wat=%s func %u op 0x%02x "
                            "engine-said=\"%s\" wat-said=\"%s\" wast-expects=%s\n",
                    g_curfile, vm_accepted ? "accept" : "reject",
                    wat_accepted ? "accept" : "reject",
                    bad_fn, bad_in ? bad_in->op : 0,
                    jav_err_str(vm_err), jav_err_str(bad_err), g_msg);
        if (getenv("WAST_VV") && g_wat76_bad <= (vmax ? atoi(vmax) : 60) && g_modlen) {
            int sn = g_modlen > 200 ? 200 : g_modlen;
            fprintf(stderr, "      ");
            for (int i = 0; i < sn; i++) { char ch = g_modsrc[i]; putc((ch=='\n'||ch=='\t')?' ':ch, stderr); }
            putc('\n', stderr);
        }
    }
    bbq_arena_free(&a);
    jav_module_free(&mod);
}

// ── §7 validation gate over TEXT modules: assemble (the water path: wat_assemble →
// jav_module_write → bytes) then run the SAME jav_validate_bytes. This is what carries
// the GC validation corpus (type-subtyping/array/struct .wast, all text-form) into the
// validator. A module that doesn't assemble (text-level malformed or syntax the .wat
// reader doesn't cover) is EXCLUDED, not scored — surfaced in the summary, never silent.
static int g_tval_ok, g_tval_bad, g_tval_excl;
static void run_module_validate_text(const Node *m, int kind) {
    if (kind == 2) return;                               // malformed is the reader's gate, not §7
    static char qbuf[1<<20];
    const char *src; int len;
    if (!module_wat_source(m, qbuf, &src, &len)) return; // binary modules → run_module_validate; instance → not a module
    g_modsrc = src; g_modlen = len;
    wat_ctx_t ctx; memset(&ctx, 0, sizeof ctx);
    bbq_arena_init(&ctx.arena, 64 * 1024);
    peg_state p;
    ctx.pass = 1; wat_parser_init(&p, src, len); p.user_data = &ctx;
    int ok = wat_parser_parse(&p);
    if (ok) {
        ctx.pass = 2; ctx.mod = calloc(1, sizeof *ctx.mod);
        wat_parser_init(&p, src, len); p.user_data = &ctx;
        ok = wat_parser_parse(&p);
    }
    // free the parse scratch but KEEP ctx.mod (malloc-owned, survives teardown — as wat_assemble does)
    bbq_arena_free(&ctx.arena); wat_wbufs_free(&ctx); wat_scratch_free(&ctx);
    wat_assembly_free(&ctx); free_parse_vecs(&ctx);
    if (!ok || !ctx.mod) { if (ctx.mod) { jav_module_free(ctx.mod); free(ctx.mod); } g_tval_excl++; return; }
    bbq_write_ctx_t w;
    bbq_write_ctx_init_growable(&w, (size_t)len + 64);
    bbq_write_set_endian(&w, true);
    int wrote = jav_module_write(&w, ctx.mod);
    jav_module_free(ctx.mod); free(ctx.mod);
    if (!wrote) { bbq_write_ctx_free(&w); g_tval_excl++; return; }
    jav_err_t err = JAV_E_NONE;
    int accepted = (jav_validate_bytes(w.data, w.pos, &err) == JAV_OK);
    // PIN A-4 over the TEXT corpus too — which is where the GC and SIMD validation
    // cases live, so it is the half of the differential with the type-system reach.
    run_module_wat76(w.data, (int)w.pos, accepted, err);
    bbq_write_ctx_free(&w);
    int want_accept = (kind == 0);
    if (accepted == want_accept) {
        if (!want_accept && !err_matches(jav_err_str(err), g_msg)) {   // right verdict, wrong reason
            g_val_msgbad++;
            if (getenv("WAST_VV")) fprintf(stderr, "  §7 TEXT MSG-MISMATCH %s expect=%s got=\"%s\"\n", g_curfile, g_msg, jav_err_str(err));
            return;
        }
        g_tval_ok++; return;
    }
    g_tval_bad++;
    const char *vmax = getenv("WAST_VMAX");
    if (getenv("WAST_VV") && g_tval_bad <= (vmax ? atoi(vmax) : 60)) {
        int sn = len > 240 ? 240 : (int)len;
        fprintf(stderr, "  §7 TEXT MISMATCH [%s] %s expect=%s got=%s reason=%s\n    ",
                kind ? "invalid" : "valid", g_curfile, want_accept?"accept":"reject",
                accepted?"accept":"reject", jav_err_str(err));
        for (int i = 0; i < sn; i++) { char ch = m->s0[i]; putc((ch=='\n'||ch=='\t')?' ':ch, stderr); }
        putc('\n', stderr);
    }
}

// ═══════════════════════ Phase 5: execution conformance runner ═══════════════════════
// Drive the .wast EXECUTION assertions through the engine: instantiate modules into a
// shared store, run invoke/get actions, and check assert_return / assert_trap /
// assert_exhaustion / assert_unlinkable / assert_uninstantiable. The canonical tier is
// the interpreter; the JIT tier re-runs each pure assert_return for differential
// agreement. Every command is accounted for — unsupported forms are NAMED + counted.

// One assembled module's bytes (binary decode / text or quote assemble). 1 on success.
static int module_to_bytes(const Node *m, uint8_t **out, size_t *outlen) {
    int si = binary_strs_at(m);
    if (si >= 0) {                                   // (module binary "…")
        size_t cap = 0; for (int i = si; i < m->nkids; i++) if (m->kids[i]->is_str) cap += m->kids[i]->tlen;
        uint8_t *b = malloc(cap + 1); int n = 0;
        for (int i = si; i < m->nkids; i++) if (m->kids[i]->is_str) n += decode_str(m->kids[i], b + n);
        *out = b; *outlen = (size_t)n; return 1;
    }
    if (out_of_scope(m)) return 0;                   // module definition/instance, annotations
    // Text / quote → assemble via the .wat reader (passes 1+2) → jav_module_write → bytes.
    const char *src = m->s0; int len = (int)(m->s1 - m->s0);
    char *qbuf = NULL;
    if (is_quote_module(m)) {                         // decode the string body into .wat source
        qbuf = malloc((1<<20)); char *txt = qbuf + 8; int qn = 0;
        for (int i = 1; i < m->nkids; i++) if (m->kids[i]->is_str) qn += decode_str(m->kids[i], (uint8_t*)txt + qn);
        int k = 0; while (k < qn && strchr(" \t\n\r", txt[k])) k++;
        if (qn - k >= 7 && memcmp(txt + k, "(module", 7) == 0 && (qn - k == 7 || strchr(" \t\n\r()", txt[k+7]))) {
            src = txt; len = qn;
        } else { memcpy(qbuf, "(module ", 8); txt[qn] = ')'; src = qbuf; len = 8 + qn + 1; }
    }
    wat_ctx_t ctx; memset(&ctx, 0, sizeof ctx);
    bbq_arena_init(&ctx.arena, 64 * 1024);
    peg_state p; ctx.pass = 1; wat_parser_init(&p, src, len); p.user_data = &ctx;
    int ok = wat_parser_parse(&p);
    if (ok) { ctx.pass = 2; ctx.mod = calloc(1, sizeof *ctx.mod); wat_parser_init(&p, src, len); p.user_data = &ctx; ok = wat_parser_parse(&p); }
    bbq_arena_free(&ctx.arena); wat_wbufs_free(&ctx); wat_scratch_free(&ctx); wat_assembly_free(&ctx); free_parse_vecs(&ctx);
    if (!ok || !ctx.mod) { if (ctx.mod) { jav_module_free(ctx.mod); free(ctx.mod); } free(qbuf); return 0; }
    bbq_write_ctx_t w; bbq_write_ctx_init_growable(&w, (size_t)len + 64); bbq_write_set_endian(&w, true);
    int wrote = jav_module_write(&w, ctx.mod);
    jav_module_free(ctx.mod); free(ctx.mod); free(qbuf);
    if (!wrote) { bbq_write_ctx_free(&w); return 0; }
    uint8_t *b = malloc(w.pos ? w.pos : 1); memcpy(b, w.data, w.pos); *out = b; *outlen = w.pos;
    bbq_write_ctx_free(&w); return 1;
}

// The synthetic spectest host module, as WAT (empty print* funcs are valid no-ops).
static const char *SPECTEST_WAT =
    "(module"
    " (func (export \"print\"))"
    " (func (export \"print_i32\") (param i32))"
    " (func (export \"print_i64\") (param i64))"
    " (func (export \"print_f32\") (param f32))"
    " (func (export \"print_f64\") (param f64))"
    " (func (export \"print_i32_f32\") (param i32 f32))"
    " (func (export \"print_f64_f64\") (param f64 f64))"
    " (global (export \"global_i32\") i32 (i32.const 666))"
    " (global (export \"global_i64\") i64 (i64.const 666))"
    " (global (export \"global_f32\") f32 (f32.const 666.6))"
    " (global (export \"global_f64\") f64 (f64.const 666.6))"
    " (table (export \"table\") 10 20 funcref)"
    " (table (export \"table64\") i64 10 20 funcref)"
    " (memory (export \"memory\") 1 2)"
    " (memory (export \"memory64\") i64 1 2))";

static int g_store_ready;

// Assemble a WAT source string to bytes (for spectest). 1 on success.
static int wat_string_to_bytes(const char *wat, uint8_t **out, size_t *len) {
    Sc s = { wat, wat + strlen(wat) }; Node *m = parse_value(&s);
    if (!m) return 0;
    int ok = module_to_bytes(m, out, len);
    free_node(m); return ok;
}

// The module's $id (the `$…` token right after "module"), or NULL.
static const char *module_id(const Node *m) {
    static char idb[64];
    for (int i = 1; i < m->nkids; i++) { const Node *k = m->kids[i];
        if (!k->is_list && !k->is_str && k->tlen && k->tok[0] == '$') { int L=k->tlen>63?63:k->tlen; memcpy(idb,k->tok,L); idb[L]=0; return idb; }
        if (!k->is_list && !k->is_str) continue; else break;
    }
    return NULL;
}

// Owning-side execution dispatch: assemble bytes (the .wat path lives in this TU) then
// hand them to the c-lite runner. Every command is accounted for.
// A module-shaped command (a bare module, or an assert_* that instantiates one) → the single
// wast_exec_module channel with the verdict the .wast declares. `assert_trap` with a module
// inside is a start-trap; with an action inside it's a runtime trap (the action path below).
static int module_expect(const Node *cmd) {
    if (head_is(cmd, "module"))               return JAV_OK;
    if (head_is(cmd, "assert_unlinkable"))     return JAV_UNLINKABLE;
    if (head_is(cmd, "assert_uninstantiable")) return JAV_UNINSTANTIABLE;
    if (head_is(cmd, "assert_trap") && module_of(cmd)) return JAV_TRAP;   // (assert_trap (module …(start)) …)
    return -1;   // not a module-shaped command
}
// Copy an atom token ($id / keyword) into `buf` as a C string (empty for list/str/NULL).
static const char *tok_str(const Node *n, char *buf) {
    if (!n || n->is_list || n->is_str) { buf[0] = 0; return buf; }
    int L = n->tlen > 63 ? 63 : n->tlen; memcpy(buf, n->tok, L); buf[L] = 0; return buf;
}
// `(module definition $id …fields)` → the equivalent `(module $id …fields)` source (splice out the
// "definition" keyword) so the .wat reader assembles it like any module.
static char *def_module_source(const Node *m) {
    const Node *kw = m->kids[1];                 // the "definition" atom
    const char *after = kw->tok + kw->tlen;      // just past "definition"
    if (after > m->s1) return NULL;
    size_t rest = (size_t)(m->s1 - after);
    char *src = malloc(7 + rest + 1);
    memcpy(src, "(module", 7); memcpy(src + 7, after, rest); src[7 + rest] = 0;
    return src;
}
static void exec_dispatch(const Node *cmd) {
    // (module definition $id …) / (module instance $inst? $def) — core .wast generative-instantiation
    // scripting (NOT a proposal): assemble+store a named definition, or instantiate one generatively.
    if (head_is(cmd, "module") && cmd->nkids >= 2 && tok_is(cmd->kids[1], "definition")) {
        char idb[64]; const char *id = tok_str(cmd->kids[2], idb);
        char *src = def_module_source(cmd); uint8_t *by; size_t bl;
        if (src && wat_string_to_bytes(src, &by, &bl)) wast_exec_define(id, by, bl);
        else wast_exec_note_excl("module didn't assemble");
        free(src);
        return;
    }
    if (head_is(cmd, "module") && cmd->nkids >= 3 && tok_is(cmd->kids[1], "instance")) {
        char ib[64], db[64];
        const Node *inst = cmd->nkids >= 4 ? cmd->kids[2] : NULL;   // (module instance $inst $def) or (module instance $def)
        const Node *def  = cmd->nkids >= 4 ? cmd->kids[3] : cmd->kids[2];
        wast_exec_instance(inst ? tok_str(inst, ib) : NULL, tok_str(def, db));
        return;
    }
    int expect = module_expect(cmd);
    if (expect >= 0) {
        const Node *m = module_of(cmd); if (!m) return;
        uint8_t *by; size_t bl;
        if (!module_to_bytes(m, &by, &bl)) { wast_exec_note_excl("module didn't assemble"); return; }
        wast_exec_module(by, bl, module_id(m), expect);   // takes ownership of `by`
        return;
    }
    if (head_is(cmd, "register")) {
        if (cmd->nkids < 2 || !cmd->kids[1]->is_str) return;
        static uint8_t nb[1024]; int nl = decode_str(cmd->kids[1], nb); nb[nl] = 0;
        const Node *idn = (cmd->nkids >= 3 && !cmd->kids[2]->is_str) ? cmd->kids[2] : NULL;
        wast_exec_register((const char *)nb, nl, idn);
        return;
    }
    if (head_is(cmd, "invoke") || head_is(cmd, "get")) { wast_exec_action(cmd); return; }
    if (head_is(cmd, "assert_return"))                  { wast_exec_assert_return(cmd); return; }
    if (head_is(cmd, "assert_trap") || head_is(cmd, "assert_exhaustion")) { wast_exec_assert_trap(cmd); return; }
    if (head_is(cmd, "assert_exception")) { wast_exec_assert_exception(cmd); return; }
    // assert_invalid / assert_malformed → the validation gates own these; nothing to run.
}

int main(int argc, char **argv) {
    int files = 0;
    // --tier=<interp|1|2> picks which engine the corpus RUNS on; every conformance
    // number below is that tier's claim, not a tier-independent one. Default is the
    // interpreter, which is what the committed figures are measured against.
    //
    // --sweep instantiates every module and runs no assertions: the tier-2 tree
    // builder is built during jit_compile, so putting a whole corpus of real bodies
    // through it needs a compiling store but not a single invoke. In that mode this
    // runner makes exactly ONE claim and prints exactly one meter.
    wast_tier_t tier = WAST_TIER_INTERP;
    int sweep = 0;
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--tier=interp") == 0) tier = WAST_TIER_INTERP;
        else if (strcmp(argv[a], "--tier=1") == 0) tier = WAST_TIER_1;
        else if (strcmp(argv[a], "--tier=2") == 0) tier = WAST_TIER_2;
        else if (strcmp(argv[a], "--tier=3") == 0) tier = WAST_TIER_3;
        else if (strcmp(argv[a], "--sweep") == 0) { sweep = 1; tier = wast_exec_jit_tier(); }
    }
    if (wast_exec_store_init(tier)) {                // build the spectest host once, register it
        uint8_t *sb; size_t sl;
        if (wat_string_to_bytes(SPECTEST_WAT, &sb, &sl)) { wast_exec_spectest(sb, sl); g_store_ready = 1; }
    }
    if (!g_store_ready) fprintf(stderr, "warning: spectest store init failed — execution skipped\n");
    for (int a = 1; a < argc; a++) {
        if (strncmp(argv[a], "--", 2) == 0) continue;
        FILE *f = fopen(argv[a], "rb"); if (!f) continue;
        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
        char *b = malloc(sz+1); if (fread(b,1,sz,f)!=(size_t)sz){fclose(f);free(b);continue;} b[sz]=0; fclose(f);
        files++;
        const char *bn = strrchr(argv[a], '/'); g_curfile = bn ? bn+1 : argv[a];
        if (g_store_ready) wast_exec_file_reset();    // fresh script scope (spectest stays registered)
        Sc s = { b, b+sz };
        for (;;) {
            wsx_skip_ws(&s); if (s.p >= s.e) break;
            const char *before = s.p;
            Node *cmd = parse_value(&s);
            if (!cmd || s.p == before) { free_node(cmd); break; }   // defensive: guarantee progress
            if (!cmd->is_list) { free_node(cmd); continue; }
            const Node *m = module_of(cmd);
            if (m) {                                  // module-bearing → the 4 validation/parse gates
                int kind = head_is(cmd, "assert_malformed") ? 2 : head_is(cmd, "assert_invalid") ? 1 : 0;
                int expect_ok = (kind != 2);                         // malformed→fail; else parse-ok
                g_msg = "";
                for (int i = cmd->nkids-1; i >= 0; i--)
                    if (cmd->kids[i]->is_str) { static char mb[128]; int L=cmd->kids[i]->tlen; if(L>127)L=127;
                        memcpy(mb, cmd->kids[i]->tok, L); mb[L]=0; g_msg=mb; break; }
                run_module(m, expect_ok);        // binary modules → binary reader (decode/malformed gate)
                run_module_text(m, expect_ok);   // text modules → .wat reader
                run_module_validate(m, kind);    // binary modules → §7 validation gate (Phase 1)
                run_module_validate_text(m, kind); // text modules → assemble → §7 validation gate
            }
            // In sweep mode only the module channel runs: instantiation is what
            // reaches jit_compile, and no assertion is being claimed.
            if (g_store_ready && (!sweep || m)) exec_dispatch(cmd);
            free_node(cmd);
        }
        free(b);
    }
    if (g_store_ready) wast_exec_teardown();
    FILE *sum = getenv("WAST_CAT") ? stderr : stdout;   // keep the CAT dump clean on stdout
    fprintf(sum, "wast binary-module conformance: %d ok, %d mismatched (%d files)\n", g_ok, g_bad, files);
    fprintf(sum, "wast §7 validation gate (Phase 1): %d ok, %d mismatched\n", g_val_ok, g_val_bad);
    fprintf(sum, "wast §7 validation gate (text modules): %d ok, %d mismatched, %d excluded (didn't assemble)\n",
            g_tval_ok, g_tval_bad, g_tval_excl);
    fprintf(sum, "wast §7 reject-reason (jav_err_str vs .wast string): %d rejected for the WRONG reason\n", g_val_msgbad);
    fprintf(sum, "wast text-module (.wat reader) conformance: %d ok, %d mismatched, %d excluded (non-core-3.0)\n",
            g_wat_ok, g_wat_bad, g_wat_excl);
    // PIN A-4. water transcribes §7.6 itself and links no engine TU; this is the
    // check on that — every module the engine accepts, water's own walk accepts.
    fprintf(sum, "wast §7 differential (water vs engine): %d ok, %d verdicts differ\n",
            g_wat76_ok, g_wat76_bad);
    // PIN A-5. Two walks that share no code — water's §7.6 over the owning reader's
    // structs, and jav_ttree_build decoding bytes — must agree on every producer edge.
    // The point of the second implementation is that EITHER side can be the wrong one,
    // so a body the harness cannot align is a body this check did not make: `unaligned`
    // is in the exit code exactly like `differ`, because an uncompared body is where a
    // real disagreement would sit unseen.
    fprintf(sum, "wast §7.6 edge differential (water vs tier-2 tree): %d edges over %d bodies, "
                 "%d differ, %d unaligned\n",
            g_a5_ok, g_a5_bodies, g_a5_bad, g_a5_unaligned);
    int eok = 0, ebad = 0, eexcl = 0;
    if (g_store_ready && !sweep) {
        const char *ereason;
        wast_exec_counts(&eok, &ebad, &eexcl, &ereason);
        // Named with its tier: these are that engine's numbers, and the committed
        // figures are the interpreter's. The name comes from the tier that RAN —
        // spelling one in the format string is how a tier-2 run printed a tier-1
        // heading, which is the one thing this line exists to be exact about.
        fprintf(sum, "wast execution conformance%s%s%s: %d ok, %d mismatched, %d excluded\n",
                tier == WAST_TIER_INTERP ? "" : " [",
                tier == WAST_TIER_INTERP ? "" : wast_tier_name(tier),
                tier == WAST_TIER_INTERP ? "" : "]",
                eok, ebad, eexcl);
        fprintf(sum, "wast trap-reason (jav_trap_reason_str vs .wast string): %d trapped for the WRONG reason\n",
                wast_exec_trap_msgbad());
        if (getenv("WAST_EXCL")) wast_exec_print_breakdown(sum);
    }
    // PIN B-4. The arity claim alone would be green over zero nodes, so the sweep
    // reports what it built: a body it declined is one the tier-2 walk is short a
    // fact for, and the first one names the instruction it stopped on.
    // …minus what the §7.6 edge differential built for its own comparison, which lands
    // in the same global counters but is not the JIT compiling anything. Arity and
    // order breaks are NOT subtracted: they are gated at zero and a non-zero from
    // either producer is the same real defect in the same builder.
    jav_ttree_stats_t jit_only = *jav_ttree_stats();
    jit_only.bodies_built    -= g_a5_delta.bodies_built;
    jit_only.bodies_declined -= g_a5_delta.bodies_declined;
    jit_only.nodes           -= g_a5_delta.nodes;
    jit_only.nodes_carried   -= g_a5_delta.nodes_carried;
    const jav_ttree_stats_t *ts = &jit_only;
    /* Tier-3 runs the WHOLE tier-2 machinery (the rewrite sits between build
     * and reduce), so every tier-2 meter and gate applies there verbatim. */
    int t2plus = tier == WAST_TIER_2 || tier == WAST_TIER_3;
    if (tier != WAST_TIER_INTERP) {
        fprintf(sum, "wast tier-2 tree sweep: %llu bodies, %llu nodes, "
                     "%llu arity mismatches, %llu order breaks, %llu declined\n",
                (unsigned long long)ts->bodies_built, (unsigned long long)ts->nodes,
                (unsigned long long)ts->arity_mismatches,
                (unsigned long long)ts->order_breaks,
                (unsigned long long)ts->bodies_declined);
        /* Unpicked must be EXACTLY the carried leaves: those stand for a value
         * already in memory and have no instruction to stamp, so no rule fires
         * for them. Any other unpicked node is an instruction the cover accepted
         * and then said nothing about — silently correct, silently unoptimized. */
        fprintf(sum, "wast tier-2 tiling: %llu covered, %llu uncovered, "
                     "%llu nodes picked, %llu unpicked (%llu carried)\n",
                (unsigned long long)ts->bodies_covered,
                (unsigned long long)ts->bodies_uncovered,
                (unsigned long long)ts->nodes_picked,
                (unsigned long long)ts->nodes_unpicked,
                (unsigned long long)ts->nodes_carried);
        /* What the CACHE did, which none of the lines above can say. A corpus can
         * be green with every body covered and still never put a value in a
         * register — the stitcher would stamp nothing and the answers would be
         * identical, because tier-2 is an optimization on top of a tier that
         * already works. So the meter that says whether this ran AS TIER-2 is
         * the count of instructions that executed with an operand in a register,
         * and the transitions between them; `dropped` is bodies where a gap could
         * not be bridged and the walk re-stamped them at tier-1, which is legal
         * (D8) and exactly the "green by bailing out" this line exists to expose. */
        fprintf(sum, "wast tier-2 stitch: %llu cached state(s) (%llu above slot 0), "
                     "%llu transition(s) [%llu spill, %llu fill, %llu at a region "
                     "boundary], %llu body(ies) dropped to tier-1\n",
                (unsigned long long)ts->states_cached,
                (unsigned long long)ts->states_deep,
                (unsigned long long)ts->transitions,
                (unsigned long long)ts->trans_spill,
                (unsigned long long)ts->trans_fill,
                (unsigned long long)ts->trans_boundary,
                (unsigned long long)ts->bridge_fails);
        /* …and the one that says whether any of it was worth doing. */
        fprintf(sum, "wast tier-2 memory: %llu operand-stack slot(s) touched\n",
                (unsigned long long)ts->mem_slots);
        /* B3's quantity: stamps whose opcode lacked the rule's state, so the
         * stitcher descended and the bridge spilled slots the cover never
         * priced. Legal, so the exit-code gate is the meter's own identity;
         * the VALUE is a recorded baseline (docs/test-baseline.md law). */
        fprintf(sum, "wast tier-2 descend: %llu stamp(s) below the rule's state, "
                     "%llu unpriced slot(s)%s",
                (unsigned long long)ts->descends,
                (unsigned long long)ts->descend_slots,
                ts->have_descend ? "" : "\n");
        if (ts->have_descend)
            fprintf(sum, "; first: op 0x%02x sub %u state %d -> %d\n",
                    ts->first_descend_op, ts->first_descend_sub,
                    ts->first_descend_from, ts->first_descend_to);
    }
    /* Tier-3's own meters (PIN B-1). `rewritten` is the identity while the
     * rule set is empty: zero rules ⇒ every extraction equals its original ⇒
     * the reduce consumes the identical tree and the code is tier-2's, byte
     * for byte (the make-level compare of the two runs' code lines is the
     * other half of the pin). `identity_fails` must be zero at ANY rule set —
     * it is the pass disagreeing with itself. Refusals are legal, counted,
     * and expected 0 on this corpus (a cap that binds is a fact to record). */
    const jav_eqsat_stats_t* es = jav_eqsat_stats();
    if (tier == WAST_TIER_3)
        fprintf(sum, "wast tier-3 eqsat: %llu bodies, %llu regions, %llu roots, "
                     "%llu rewritten, %llu cap refusal(s), %llu identity failure(s)\n",
                (unsigned long long)es->bodies, (unsigned long long)es->regions,
                (unsigned long long)es->roots, (unsigned long long)es->rewritten,
                (unsigned long long)es->cap_refusals,
                (unsigned long long)es->identity_fails);
    /* Bodies the JIT declined OUTRIGHT, which stay on the interpreter. Distinct
     * from every meter above: those describe tier-2's decisions inside a body the
     * JIT took, while this is the JIT not taking it at all. It is the quietest
     * failure the engine has — the module runs and every answer agrees — so it is
     * printed and gated rather than left for someone to notice. */
    if (tier != WAST_TIER_INTERP && !sweep) {
        uint32_t dec = wast_exec_jit_declined();
        fprintf(sum, "wast jit coverage: %u function(s) declined by the JIT\n", dec);
    }
    /* EVERY function the JIT was offered got a tree built for it — the one claim that
     * says the tier-2 meters cover the run rather than some part of it. `bodies_built`
     * is the builder's own count and was gated only against ZERO, so a run that swept a
     * third of the corpus passed exactly like one that swept all of it. The count it is
     * checked against is maintained somewhere else entirely, by jav_instance.c at each
     * instantiation, so the two can only agree by both being right.
     *
     * It is not a constant: the corpus decides how many functions there are, and this
     * says the builder saw all of them in the same run that counted them. */
    uint32_t jit_offered = 0;
    if (t2plus) {
        jit_offered = wast_exec_jit_compiled() + wast_exec_jit_declined();
        fprintf(sum, "wast tier-2 body coverage: %llu built + %llu declined, "
                     "of %u function(s) the JIT was offered\n",
                (unsigned long long)ts->bodies_built,
                (unsigned long long)ts->bodies_declined, jit_offered);
    }
    if (tier != WAST_TIER_INTERP) {
        if (ts->have_unbridged)
            fprintf(sum, "  first unbridged: op 0x%02x @%u, state %d -> %d, class %d\n",
                    ts->first_unbridged_op, ts->first_unbridged_off,
                    ts->first_unbridged_from, ts->first_unbridged_to,
                    ts->first_unbridged_cls);
        if (ts->code_bodies)
            fprintf(sum, "wast tier-2 code: %llu bytes over %llu bodies (%llu mean)\n",
                    (unsigned long long)ts->code_bytes,
                    (unsigned long long)ts->code_bodies,
                    (unsigned long long)(ts->code_bytes / ts->code_bodies));
        if (ts->have_uncovered)
            fprintf(sum, "  first uncovered at signature %d\n", ts->first_uncovered_sig);
        /* A body whose reduce-driven walk failed re-stamped plain — correct
         * (D8), silent, therefore counted and gated: a fallback is a body the
         * tier-2 meters above are NOT about, so a nonzero here makes them lie
         * by omission. */
        if (ts->tree_fallbacks)
            fprintf(sum, "wast tier-2 fallbacks: %llu body(ies) re-stamped plain; "
                         "first: op 0x%02x @%u entry %d why %d\n",
                    (unsigned long long)ts->tree_fallbacks,
                    ts->first_fallback_op, ts->first_fallback_bpos,
                    ts->first_fallback_entry, ts->first_fallback_why);
        if (ts->bodies_declined) {          // the work list, busiest instruction first
            uint32_t left[256];
            memcpy(left, ts->decline_op, sizeof left);
            fprintf(sum, "  declines by instruction:");
            for (int shown = 0; shown < 6; shown++) {
                int best = -1;
                for (int k = 0; k < 256; k++)
                    if (left[k] && (best < 0 || left[k] > left[best])) best = k;
                if (best < 0) break;
                fprintf(sum, " 0x%02x=%u", best, left[best]);
                left[best] = 0;
            }
            fputc('\n', sum);
        }
    }
    // EVERY meter this runner prints is gated. It used to gate three of six, so the text
    // validation gate, the reject-reason meter, the execution gate and the trap-reason
    // meter could all move without the suite failing — a regression that took text
    // validation from 5135/0 to 4928/74 still printed PASS. A number worth printing is a
    // number worth failing on; if one is expected to be non-zero it belongs in
    // docs/test-baseline.md as a committed figure, not silently ungated here.
    return (g_bad || g_wat_bad || g_val_bad || g_tval_bad || g_val_msgbad
            || g_wat76_bad || g_a5_bad || g_a5_unaligned || g_a5_bodies == 0
            || ebad || (!sweep && wast_exec_trap_msgbad())
            /* The tree and tiling meters belong to TIER-2: tier-1 compiles with no
             * tiling context, so no tree is built and there is nothing for them to
             * be about. Gating them on "any JIT tier" made a correct tier-1 run
             * fail for having built zero trees. */
            /* `bodies_declined` was printed, and given a by-instruction histogram
             * when non-zero, and gated by nothing — so the builder's own refusals
             * were the one decline of the three Amendment #9 named that could still
             * grow silently. It is the meter every check INSIDE the builder reports
             * through (arity, emission order, a walk that did not consume the body),
             * which makes an ungated one a check that cannot fail. 0 for this corpus. */
            || (t2plus
                && (ts->arity_mismatches || ts->order_breaks || ts->bodies_built == 0
                    || ts->bodies_declined || ts->bodies_uncovered
                    || ts->tree_fallbacks
                    || ts->bodies_built + ts->bodies_declined != jit_offered
                    || ts->nodes_unpicked != ts->nodes_carried))
            /* …and the stitch meters, on the tier that HAS a cache. A body that
             * dropped back to tier-1 is a body this run did not test, so a green
             * over a corpus full of them is a green for the tier below; and zero
             * cached states means the corpus never exercised the mechanism at
             * all, which is the same green for a different reason. */
            || (t2plus && ts->bridge_fails)
            /* The descend meter's own identity: each descend is at least one
             * slot, and a named first implies a count (and the reverse). The
             * magnitude is a baseline, not a gate — a descend is the grammar's
             * C5 contract working, so zero would be the wrong assertion. */
            || (t2plus
                && (ts->descend_slots < ts->descends
                    || (ts->descends != 0) != (ts->have_descend != 0)))
            /* …and "the cache was actually used" only where there IS one. At
             * TIER2_N=0 tier-2 builds the tree and covers it, but every rule is a
             * state-0 rule and nothing is ever cached — correctly, and the meter
             * says so. */
            /* `states_cached` alone. `transitions == 0` was here as a second
             * vacuity check and it was WRONG in the same way it was wrong in
             * test_jit_tier2.c: a transition is the mechanism FAILING to be
             * invisible — the access it would have done inline, plus a jump — so
             * none of them over a corpus is the best result available, not an
             * unexercised one. It started failing the moment the grammar stopped
             * offering rules the variant family does not provide, which is when
             * n=1 went from 10,545 transitions to none. */
            || (t2plus && wast_exec_cache_slots() > 0
                && ts->states_cached == 0)
            /* Tier-3: the pass RAN (bodies == the tier-2 built count — every
             * tree the builder produced went through the rewrite), nothing was
             * rewritten (the zero-rule identity; Part C amends this to a
             * recorded baseline WITH the axioms), nothing refused, and the
             * pass never disagreed with itself. */
            || (tier == WAST_TIER_3
                && (es->bodies != ts->bodies_built
                    || es->rewritten || es->cap_refusals || es->identity_fails
                    || (es->bodies && !es->roots)))
            /* …and no function silently off the tier. 0 is the committed figure
             * for this corpus on both JIT tiers; an opcode that legitimately
             * cannot be compiled (`flag:no_jit` — there are none today) would
             * move it, and moving it is a decision to record here, not a number
             * to let drift. */
            || (tier != WAST_TIER_INTERP && !sweep && wast_exec_jit_declined())) ? 1 : 0;
}
