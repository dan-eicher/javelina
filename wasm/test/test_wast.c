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
    if (wast_exec_store_init()) {                    // build the spectest host once, register it
        uint8_t *sb; size_t sl;
        if (wat_string_to_bytes(SPECTEST_WAT, &sb, &sl)) { wast_exec_spectest(sb, sl); g_store_ready = 1; }
    }
    if (!g_store_ready) fprintf(stderr, "warning: spectest store init failed — execution skipped\n");
    for (int a = 1; a < argc; a++) {
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
            if (g_store_ready) exec_dispatch(cmd);    // §execution: instantiate / invoke / assert_*
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
    int eok = 0, ebad = 0, eexcl = 0;
    if (g_store_ready) {
        const char *ereason;
        wast_exec_counts(&eok, &ebad, &eexcl, &ereason);
        fprintf(sum, "wast execution conformance: %d ok, %d mismatched, %d excluded\n",
                eok, ebad, eexcl);
        fprintf(sum, "wast trap-reason (jav_trap_reason_str vs .wast string): %d trapped for the WRONG reason\n",
                wast_exec_trap_msgbad());
        if (getenv("WAST_EXCL")) wast_exec_print_breakdown(sum);
    }
    // EVERY meter this runner prints is gated. It used to gate three of six, so the text
    // validation gate, the reject-reason meter, the execution gate and the trap-reason
    // meter could all move without the suite failing — a regression that took text
    // validation from 5135/0 to 4928/74 still printed PASS. A number worth printing is a
    // number worth failing on; if one is expected to be non-zero it belongs in
    // docs/test-baseline.md as a committed figure, not silently ungated here.
    return (g_bad || g_wat_bad || g_val_bad || g_tval_bad || g_val_msgbad
            || ebad || wast_exec_trap_msgbad()) ? 1 : 0;
}
