/* gen_trap_reasons.c — the execution-trap reason vocabulary, from the spec.
 *
 * Joins the two authorities and emits src/gen/jav_trap_reason.h:
 *
 *   spec/instructions.toml  `traps`  — the canonical message text, extracted from
 *                                      the reference interpreter's raise sites.
 *                                      This is the ONLY place trap text comes from.
 *   spec/wasm.def           `error:` — which reasons the spec actually declares a
 *                                      guard for. opgen turns each name into
 *                                      OPGEN_ERR_<Name>; the emitted bridge maps
 *                                      those back to a reason.
 *
 * A guard name is the message in CamelCase ("integer divide by zero" ->
 * IntegerDivideByZero) — mechanical, so the two files cannot drift silently. A
 * declared name with no matching message is a hard error: that is the coverage
 * ledger's cross-check, applied at generation time.
 *
 * Reasons raised by the substrate rather than by a declared guard (the natives in
 * jav_runtime.c / jav_mem.h) still get an enum member — the vocabulary is the
 * spec's complete set, not the subset opgen happens to declare today.
 */
#include "yoctojc/toml/toml_doc.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REASONS 64
#define MAX_DECLARED 64

static char  g_msg[MAX_REASONS][128];   /* canonical text, first-appearance order */
static char  g_name[MAX_REASONS][128];  /* CamelCase of the same */
static int   g_nreasons;
static char  g_decl[MAX_DECLARED][128]; /* names wasm.def declares a guard for */
static int   g_ndecl;

/* ── The coverage ledger: the spec table joined to wasm.def BY OPCODE ──────────
 * Every spec instruction lands in exactly one bucket, so a trap the engine does
 * not carry is a counted row rather than a silent gap. */
#define MAX_INSTRS 600
typedef struct { char key[16]; char name[48]; short traps[4]; int ntraps;
                 const toml_val_t* ops; char type[96]; int align; } instr_t;
typedef struct { char key[16]; short reasons[8]; int nreasons;
                 char imm[8][16]; int nimm;
                 char pin[8][12]; int npin;      /* declared stack inputs  */
                 char pout[4][12]; int npout;    /* declared stack outputs */
                 int variadic;                   /* a `[count]` prefix: arity not static */
                 int membytes; } defop_t;        /* width from `mem_in_bounds(…, N)`, 0 if none */

static int is_ident_ch(char c);

/* Parse a `( i32 a, i32 b -- i32 result )` stack effect. Only the TYPE of each slot
 * matters here; names are the body's business. A `[expr]` count prefix marks the slot
 * dynamically-sized, which is itself a fact §4.2 cares about. */
static void parse_stack(const char* s, defop_t* d, const char* subst) {
    d->npin = d->npout = d->variadic = 0;
    const char* arrow = strstr(s, "--");
    if (!arrow) return;
    for (int side = 0; side < 2; side++) {
        const char* p = side ? arrow + 2 : s;
        const char* end = side ? strchr(s, ')') : arrow;
        if (!end) return;
        while (p < end) {
            while (p < end && (*p == ' ' || *p == ',' || *p == '(')) p++;
            if (p >= end) break;
            if (*p == '[') { d->variadic = 1; while (p < end && *p != ']') p++; if (p < end) p++; continue; }
            char ty[12]; size_t o = 0;
            while (p < end && o + 1 < sizeof ty && is_ident_ch(*p)) ty[o++] = *p++;
            ty[o] = 0;
            if (!o) { p++; continue; }
            /* a family's `T` is the member's concrete type */
            if (subst && !strcmp(ty, "T")) snprintf(ty, sizeof ty, "%s", subst);
            while (p < end && *p == ' ') p++;          /* the slot NAME follows the type */
            while (p < end && is_ident_ch(*p)) p++;
            /* an output may carry `= "expr"` naming the value it forwards; that is
             * part of THIS slot, not the start of another */
            while (p < end && *p == ' ') p++;
            if (p < end && *p == '=') {
                p++;
                while (p < end && *p == ' ') p++;
                if (p < end && *p == '"') { p++; while (p < end && *p != '"') p++; if (p < end) p++; }
                else while (p < end && *p != ',' && *p != ')') p++;
            }
            if (!side) { if (d->npin  < 8) snprintf(d->pin[d->npin++],   sizeof d->pin[0],  "%s", ty); }
            else       { if (d->npout < 4) snprintf(d->pout[d->npout++], sizeof d->pout[0], "%s", ty); }
        }
    }
}

/* ── §6.2 decode contract: immediate signatures ────────────────────────────────
 * Both files name the same bytes in different vocabularies — wasm.def names the
 * READER (uleb32/sleb64/memarg/u8), the spec table names the TYPE (funcidx/i64/
 * memarg/laneidx). Compare by encoding class, not by spelling. */
typedef enum { K_BAD = 0, K_IDX, K_I32, K_I64, K_F32, K_F64, K_MEMARG, K_BYTE,
               K_BLOCKTYPE, K_TRYTABLE, K_BRTABLE, K_LABELVEC, K_CATCHVEC,
               K_SELECTVEC, K_CASTOP, K_BYTE16 } opclass_t;

static opclass_t def_class(const char* t) {
    if (!strcmp(t, "uleb32"))    return K_IDX;
    if (!strcmp(t, "sleb32"))    return K_I32;
    if (!strcmp(t, "sleb64"))    return K_I64;   /* also heaptype: sleb-encoded */
    if (!strcmp(t, "f32"))       return K_F32;
    if (!strcmp(t, "f64"))       return K_F64;
    if (!strcmp(t, "memarg"))    return K_MEMARG;
    if (!strcmp(t, "u8"))        return K_BYTE;
    if (!strcmp(t, "blocktype")) return K_BLOCKTYPE;
    if (!strcmp(t, "trytable"))  return K_TRYTABLE;
    if (!strcmp(t, "brtable"))   return K_BRTABLE;
    if (!strcmp(t, "selectvec")) return K_SELECTVEC;
    return K_BAD;
}
static opclass_t toml_class(const char* t) {
    size_t n = strlen(t);
    if (n > 3 && !strcmp(t + n - 3, "idx") && strcmp(t, "laneidx")) return K_IDX;
    if (!strcmp(t, "u32"))            return K_IDX;   /* uleb32, same class as an index */
    if (!strcmp(t, "i32"))            return K_I32;
    if (!strcmp(t, "i64"))            return K_I64;
    if (!strcmp(t, "heaptype"))       return K_I64;
    if (!strcmp(t, "f32"))            return K_F32;
    if (!strcmp(t, "f64"))            return K_F64;
    if (!strcmp(t, "memarg"))         return K_MEMARG;
    if (!strcmp(t, "laneidx"))        return K_BYTE;
    if (!strcmp(t, "blocktype"))      return K_BLOCKTYPE;
    if (!strcmp(t, "vec(catch)"))     return K_CATCHVEC;
    if (!strcmp(t, "vec(labelidx)"))  return K_LABELVEC;
    if (!strcmp(t, "vec(valtype)"))   return K_SELECTVEC;
    if (!strcmp(t, "castop"))         return K_CASTOP;
    if (!strcmp(t, "byte^16"))        return K_BYTE16;
    if (!strcmp(t, "laneidx^16"))     return K_BYTE16;
    return K_BAD;
}

/* The four places javelina's spelling differs in ARITY from the spec's, each an
 * explicit claim that the two describe the same bytes. Anything not listed here and
 * not element-wise equal is a real disagreement. */
typedef struct { opclass_t def[4]; int ndef; opclass_t tml[4]; int ntml; const char* why; } equiv_t;
static const equiv_t EQUIV[] = {
    { {K_TRYTABLE},                    1, {K_BLOCKTYPE, K_CATCHVEC}, 2,
      "one trytable reader spans the spec's blocktype + vec(catch)" },
    { {K_BRTABLE},                     1, {K_LABELVEC, K_IDX},       2,
      "one brtable reader spans the spec's vec(labelidx) + default labelidx" },
    { {K_BYTE, K_IDX, K_I64, K_I64},   4, {K_CASTOP},                1,
      "the spec folds br_on_cast's flags+label+2 heaptypes into one `castop`" },
    { {K_F64, K_F64},                  2, {K_BYTE16},                1,
      "v128.const's 16 immediate bytes, carried as two f64 lanes" },
};
static instr_t g_instr[MAX_INSTRS]; static int g_ninstr;
static defop_t g_defop[MAX_INSTRS]; static int g_ndefop;

/* "0x00" -> "00"; [0xFB, 12] -> "fb:12". The join key; the toml itself was built
 * by joining decode.ml to the §7.10 index on exactly this. */
static void opkey_toml(const toml_val_t* v, char* out, size_t cap) {
    int64_t a, b;
    if (toml_val_as_int(v, &a)) { snprintf(out, cap, "%02x", (unsigned)a); return; }
    if (toml_val_type(v) == TOML_VT_ARRAY && toml_val_array_count(v) == 2 &&
        toml_val_as_int(toml_val_array_at(v, 0), &a) &&
        toml_val_as_int(toml_val_array_at(v, 1), &b)) {
        snprintf(out, cap, "%02x:%d", (unsigned)a, (int)b); return;
    }
    out[0] = 0;
}

/* "out of bounds memory access" -> "OutOfBoundsMemoryAccess". Word chars only;
 * the digits in "null i31 reference" ride through, giving NullI31Reference. */
static void camel(const char* s, char* out, size_t cap) {
    size_t o = 0; int start = 1;
    for (const char* p = s; *p && o + 1 < cap; p++) {
        if (*p == ' ') { start = 1; continue; }
        char c = *p;
        if (start && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[o++] = c; start = 0;
    }
    out[o] = 0;
}

static int reason_index(const char* msg) {
    for (int i = 0; i < g_nreasons; i++) if (!strcmp(g_msg[i], msg)) return i;
    return -1;
}

static char* slurp(const char* path, int* len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gen_trap_reasons: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "gen_trap_reasons: cannot read %s\n", path); exit(1);
    }
    b[n] = 0; fclose(f); *len = (int)n; return b;
}

/* Collect the distinct trap messages across every [[instr]], in the order the
 * spec table lists them. */
static void collect_reasons(const char* toml_path, bbq_arena* ar) {
    int len; char* src = slurp(toml_path, &len);
    toml_doc_t* doc = toml_parse(src, len, ar);
    if (toml_doc_has_errors(doc)) {
        fprintf(stderr, "gen_trap_reasons: %s: %s\n", toml_path,
                toml_doc_error_at(doc, 0)->message);
        exit(1);
    }
    const toml_val_t* instrs = toml_tbl_get(toml_doc_root(doc), "instr");
    int n = toml_val_array_count(instrs);
    if (n == 0) { fprintf(stderr, "gen_trap_reasons: no [[instr]] entries\n"); exit(1); }
    for (int i = 0; i < n; i++) {
        const toml_tbl_t* t = toml_val_as_table(toml_val_array_at(instrs, i));
        if (!t) continue;
        const toml_val_t* traps = toml_tbl_get(t, "traps");
        if (traps) for (int j = 0; j < toml_val_array_count(traps); j++) {
            const char* msg;
            if (!toml_val_as_string(toml_val_array_at(traps, j), &msg)) continue;
            if (reason_index(msg) >= 0) continue;
            if (g_nreasons == MAX_REASONS) {
                fprintf(stderr, "gen_trap_reasons: more than %d reasons\n", MAX_REASONS);
                exit(1);
            }
            snprintf(g_msg[g_nreasons], sizeof g_msg[0], "%s", msg);
            camel(msg, g_name[g_nreasons], sizeof g_name[0]);
            g_nreasons++;
        }
        /* the ledger row */
        if (g_ninstr == MAX_INSTRS) { fprintf(stderr, "gen_trap_reasons: too many instrs\n"); exit(1); }
        instr_t* r = &g_instr[g_ninstr];
        const char* nm; const toml_val_t* ov = toml_tbl_get(t, "opcode");
        if (toml_val_as_string(toml_tbl_get(t, "name"), &nm))
            snprintf(r->name, sizeof r->name, "%s", nm);
        opkey_toml(ov, r->key, sizeof r->key);
        r->ops = toml_tbl_get(t, "operands");
        const char* tystr;
        r->type[0] = 0;
        if (toml_val_as_string(toml_tbl_get(t, "type"), &tystr))
            snprintf(r->type, sizeof r->type, "%s", tystr);
        /* §6.2 natural alignment = the ACCESS WIDTH in bytes; the memarg guards declare that same
         * width, and the two must agree or a bounds check covers the wrong number of bytes. */
        { int64_t av; r->align = toml_val_as_int(toml_tbl_get(t, "align"), &av) ? (int)av : 0; }
        r->ntraps = 0;
        /* Truncating here would silently drop a spec trap and let the ledger call the
         * instruction fully declared — a false GREEN, the worst failure this gate has. */
        if (traps) for (int j = 0; j < toml_val_array_count(traps); j++) {
            const char* msg;
            if (!toml_val_as_string(toml_val_array_at(traps, j), &msg)) continue;
            if (r->ntraps == (int)(sizeof r->traps / sizeof r->traps[0])) {
                fprintf(stderr, "gen_trap_reasons: %s declares more than %d traps — "
                        "raise instr_t::traps\n", r->name, r->ntraps);
                exit(1);
            }
            r->traps[r->ntraps++] = (short)reason_index(msg);
        }
        if (r->key[0]) g_ninstr++;
    }
    free(src);
}

/* Scan wasm.def for `error: (. cond .) -> Name` and record Name. */
static void collect_declared(const char* def_path) {
    int len; char* src = slurp(def_path, &len);
    for (char* p = src; (p = strstr(p, "error:")) != NULL; ) {
        char* arrow = strstr(p, "->");
        char* eol   = strchr(p, '\n');
        if (!arrow || (eol && arrow > eol)) { p += 6; continue; }
        char* q = arrow + 2;
        while (*q == ' ' || *q == '\t') q++;
        char name[128]; size_t o = 0;
        while (o + 1 < sizeof name &&
               ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                (*q >= '0' && *q <= '9'))) name[o++] = *q++;
        name[o] = 0;
        if (o) {
            int dup = 0;
            for (int i = 0; i < g_ndecl; i++) if (!strcmp(g_decl[i], name)) dup = 1;
            if (!dup) {
                if (g_ndecl == MAX_DECLARED) {
                    fprintf(stderr, "gen_trap_reasons: more than %d declared guards\n",
                            MAX_DECLARED);
                    exit(1);
                }
                snprintf(g_decl[g_ndecl++], sizeof g_decl[0], "%s", name);
            }
        }
        p = arrow;
    }
    free(src);
}

static int name_index(const char* name) {
    for (int i = 0; i < g_nreasons; i++) if (!strcmp(g_name[i], name)) return i;
    return -1;
}

static int is_ident_ch(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* Parse wasm.def into (opcode -> declared reasons). Two shapes carry an opcode: a
 * standalone `name 0xNN [imm] ( … )` whose `error:` lines follow the header, and a
 * `family ( … )` whose guards precede a `{ ty name 0xNN … }` member list — there the
 * guards belong to EVERY member. Both are needed: the div/rem guards live on families. */
static void collect_defops(const char* def_path) {
    int len; char* src = slurp(def_path, &len);
    short pend[8]; int npend = 0;       /* guards seen since the last header */
    char fam_stack[256]; fam_stack[0] = 0;   /* a family's `( T a, T b -- T r )` text */
    defop_t* cur = NULL;                /* standalone op the guards attach to */
    int in_family = 0, in_members = 0;

    for (char* line = strtok(src, "\n"); line; line = strtok(NULL, "\n")) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || !*p) continue;

        if (!strncmp(p, "error:", 6)) {
            char* arrow = strstr(p, "->");
            if (!arrow) continue;
            char* q = arrow + 2; while (*q == ' ' || *q == '\t') q++;
            char nm[128]; size_t o = 0;
            while (o + 1 < sizeof nm && is_ident_ch(*q) && *q != '_') nm[o++] = *q++;
            nm[o] = 0;
            int ri = o ? name_index(nm) : -1;
            if (ri < 0) continue;                       /* main() reports unknown names */
            /* §6.2: capture the width a memarg guard declares, so it can be checked against the
             * spec's natural alignment. Only the single-access `mem_in_bounds(memidx, ea, N)`
             * shape — the bulk ops pass a runtime `n` and have no static width to compare. */
            if (cur) {
                const char* mb = strstr(p, "mem_in_bounds(");
                if (mb) {
                    const char* c2 = strrchr(mb, ',');
                    int w = c2 ? atoi(c2 + 1) : 0;
                    if (w > 0) cur->membytes = w;
                }
            }
            /* Dropping a guard here would misreport the op as still-hidden. Loud. */
            if (in_family) {
                if (npend == (int)(sizeof pend / sizeof pend[0])) {
                    fprintf(stderr, "gen_trap_reasons: family has more than %d guards\n", npend);
                    exit(1);
                }
                pend[npend++] = (short)ri;
            } else if (cur) {
                if (cur->nreasons == (int)(sizeof cur->reasons / sizeof cur->reasons[0])) {
                    fprintf(stderr, "gen_trap_reasons: opcode %s has more than %d guards\n",
                            cur->key, cur->nreasons);
                    exit(1);
                }
                cur->reasons[cur->nreasons++] = (short)ri;
            }
            continue;
        }

        if (!strncmp(p, "family", 6) && !is_ident_ch(p[6])) {
            in_family = 1; npend = 0; cur = NULL;
            const char* op_paren = strchr(p, '(');    /* the shared `( T a -- T r )` */
            fam_stack[0] = 0;
            if (op_paren) snprintf(fam_stack, sizeof fam_stack, "%s", op_paren);
            /* the compact one-line form puts the member list on THIS line:
             *   family ( T a -- T result ) (. … .)  { f32x4 f32x4_abs 0xfd 224 … } */
            char* brace = strchr(p, '{');
            if (!brace) continue;
            p = brace;
        }

        /* A member list runs from `{` to `}`, possibly across several lines. */
        if (in_family && (*p == '{' || in_members)) {
            in_members = 1;
            for (char* q = p; *q; q++) {
                if (q[0] != '0' || q[1] != 'x') continue;
                if (g_ndefop == MAX_INSTRS) break;
                char* e; unsigned a = (unsigned)strtoul(q, &e, 16);
                char* s = e; while (*s == ' ') s++;
                /* walk back over "<membertype> <name> 0x..": name, then the type */
                char mty[12]; mty[0] = 0;
                {
                    char* b = q - 1;
                    while (b > p && *b == ' ') b--;
                    while (b > p && is_ident_ch(*b)) b--;          /* the member's name */
                    while (b > p && *b == ' ') b--;
                    char* te = b + 1;
                    while (b > p && is_ident_ch(*(b - 1))) b--;
                    size_t tl = (size_t)(te - b);
                    if (tl && tl < sizeof mty) { memcpy(mty, b, tl); mty[tl] = 0; }
                }
                defop_t* d = &g_defop[g_ndefop++];
                /* a prefixed member carries a decimal sub-opcode; a plain one is
                 * followed by the next member's type name, which is alphabetic */
                if (*s >= '0' && *s <= '9') { snprintf(d->key, sizeof d->key, "%02x:%d", a, atoi(s)); e = s; }
                else                          snprintf(d->key, sizeof d->key, "%02x", a);
                d->nreasons = npend;
                for (int i = 0; i < npend; i++) d->reasons[i] = pend[i];
                d->nimm = 0;
                parse_stack(fam_stack, d, mty[0] ? mty : NULL);
                q = e - 1;
            }
            if (strchr(p, '}')) { in_members = 0; in_family = 0; npend = 0; }
            continue;
        }

        /* standalone header: identifier, space, 0xNN [, space, decimal sub-opcode] */
        if (is_ident_ch(*p) && !(*p >= '0' && *p <= '9')) {
            char* q = p; while (is_ident_ch(*q)) q++;
            if (*q != ' ') continue;
            while (*q == ' ') q++;
            if (q[0] != '0' || q[1] != 'x') continue;
            if (g_ndefop == MAX_INSTRS) continue;
            char* e; unsigned a = (unsigned)strtoul(q, &e, 16);
            while (*e == ' ') e++;
            defop_t* d = &g_defop[g_ndefop++];
            if (*e >= '0' && *e <= '9') { snprintf(d->key, sizeof d->key, "%02x:%d", a, atoi(e));
                                          while (*e >= '0' && *e <= '9') e++; }
            else                          snprintf(d->key, sizeof d->key, "%02x", a);
            d->nreasons = 0; d->nimm = 0;
            /* the immediate list: `[uleb32 type, uleb32 table]` — kind then name */
            while (*e == ' ') e++;
            if (*e == '[') {
                for (char* q = e + 1; *q && *q != ']'; ) {
                    while (*q == ' ' || *q == ',') q++;
                    char kind[16]; size_t o = 0;
                    while (o + 1 < sizeof kind && is_ident_ch(*q)) kind[o++] = *q++;
                    kind[o] = 0;
                    if (!o) break;
                    if (d->nimm == (int)(sizeof d->imm / sizeof d->imm[0])) {
                        fprintf(stderr, "gen_trap_reasons: %s has more than %d immediates\n",
                                d->key, d->nimm);
                        exit(1);
                    }
                    snprintf(d->imm[d->nimm++], sizeof d->imm[0], "%s", kind);
                    while (*q && *q != ',' && *q != ']') q++;   /* skip the operand's name */
                }
                while (*e && *e != ']') e++;
                if (*e) e++;
            }
            const char* sp = strchr(e, '(');
            if (sp) parse_stack(sp, d, NULL);
            cur = d; in_family = 0; npend = 0;
        }
    }
    free(src);
}

static const defop_t* defop_by_key(const char* key) {
    for (int i = 0; i < g_ndefop; i++) if (!strcmp(g_defop[i].key, key)) return &g_defop[i];
    return NULL;
}
static int has_reason(const defop_t* d, short r) {
    for (int i = 0; i < d->nreasons; i++) if (d->reasons[i] == r) return 1;
    return 0;
}

/* §6.2: does this opcode's immediate signature agree with the spec's?
 * 1 = agrees, 0 = disagrees. `note` is set to the equivalence used, if any. */
static int imm_agrees(const defop_t* d, const toml_val_t* ops, const char** note) {
    opclass_t dc[8], tc[8]; int nd = 0, nt = 0;
    *note = NULL;
    /* An unrecognised word on either side must NOT quietly compare equal to another
     * unrecognised word — that would turn a vocabulary gap into a silent pass. */
    for (int i = 0; i < d->nimm && nd < 8; i++) {
        dc[nd] = def_class(d->imm[i]);
        if (dc[nd] == K_BAD) { *note = "unknown wasm.def immediate kind"; return 0; }
        nd++;
    }
    for (int i = 0; i < toml_val_array_count(ops) && nt < 8; i++) {
        const char* s;
        if (!toml_val_as_string(toml_val_array_at(ops, i), &s)) continue;
        tc[nt] = toml_class(s);
        if (tc[nt] == K_BAD) { *note = "unknown spec operand kind"; return 0; }
        nt++;
    }
    if (nd == nt) {                                   /* the common case: same shape */
        int same = 1;
        for (int i = 0; i < nd; i++) if (dc[i] != tc[i]) same = 0;
        if (same) return 1;
    }
    for (size_t e = 0; e < sizeof EQUIV / sizeof EQUIV[0]; e++) {
        const equiv_t* q = &EQUIV[e];
        if (nd != q->ndef || nt != q->ntml) continue;
        int ok = 1;
        for (int i = 0; i < nd; i++) if (dc[i] != q->def[i]) ok = 0;
        for (int i = 0; i < nt; i++) if (tc[i] != q->tml[i]) ok = 0;
        if (ok) { *note = q->why; return 1; }
    }
    return 0;
}

/* ── §6.3 signature honesty: the `type` column vs the declared stack effect ─────
 * The spec's §7.10 typing is the independent authority on an instruction's stack
 * effect. Where it is stack-polymorphic (`t*`) no static arity exists and the row is
 * excluded; where it is concrete, wasm.def must pop and push the same number of
 * slots. An ARITY disagreement is §4.2's liar — an opcode whose native moves the
 * stack behind the signature that generated the validator's transfer function. */

/* Count the slots on one side of a spec type string, or -1 if stack-polymorphic.
 * `s` points just past '[' and the run ends at the matching ']'. */
static int spec_arity(const char* s, const char** after) {
    int n = 0;
    while (*s && *s != ']') {
        while (*s == ' ') s++;
        if (!*s || *s == ']') break;
        if (*s == '(') { int d = 0; do { if (*s=='(') d++; else if (*s==')') d--; s++; } while (*s && d); n++; continue; }
        const char* t = s;
        while (*s && *s != ' ' && *s != ']') s++;
        if (s - t >= 2 && t[1] == '*') { if (after) *after = NULL; return -1; }   /* t* : polymorphic */
        if (s - t == 2 && t[0] == 't' && t[1] >= '1' && t[1] <= '9') { if (after) *after = NULL; return -1; }
        /* The unparenthesised reference type `ref ht` / `ref null ht` is ONE slot
         * spelled as several space-separated words (the parenthesised `(ref null x)`
         * form is handled above). Consume its tail so it counts once. */
        if (s - t == 3 && !strncmp(t, "ref", 3)) {
            while (*s == ' ') s++;
            if (!strncmp(s, "null", 4)) { s += 4; while (*s == ' ') s++; }
            while (*s && *s != ' ' && *s != ']') s++;              /* the heaptype */
        }
        n++;
    }
    if (after) *after = (*s == ']') ? s + 1 : s;
    return n;
}

/* A wasm.def slot type that deliberately loses precision: it names a CARRIER, not the
 * spec's type. Verifiable in principle, lossy in practice — counted, not failed. */
static int is_carrier(const char* t) {
    return !strcmp(t, "any") || !strcmp(t, "word") || !strcmp(t, "addr") ||
           !strcmp(t, "externref");
}

/* The gate. Every spec instruction lands in exactly one bucket; a guard whose reason
 * the spec does not list for that instruction is an error, not a bucket. */
static int ledger(FILE* o) {
    int declared = 0, partial = 0, hidden = 0, notrap = 0, unmapped = 0, bad = 0, imm_equiv = 0;
    int sig_exact = 0, sig_carrier = 0, sig_liar = 0, sig_poly = 0, sig_variadic = 0;
    for (int i = 0; i < g_ninstr; i++) {
        const instr_t* r = &g_instr[i];
        const defop_t* d = defop_by_key(r->key);
        if (!d) {                                       /* in the spec table, not in wasm.def */
            unmapped++;
            if (getenv("TRAP_LEDGER_V")) fprintf(stderr, "  unmapped: %-28s %s\n", r->name, r->key);
            continue;
        }

        /* §6.3 signature honesty: declared stack effect vs the spec's typing. */
        if (r->type[0] == '[') {
            const char* p = r->type + 1; const char* rest = NULL;
            int sin = spec_arity(p, &rest);
            int sout = -1;
            if (rest) { const char* rb = strchr(rest, '['); if (rb) sout = spec_arity(rb + 1, NULL); }
            if (sin < 0 || sout < 0)      sig_poly++;
            else if (d->variadic)         sig_variadic++;
            else if (sin != d->npin || sout != d->npout) {
                sig_liar++;
                if (getenv("TRAP_LEDGER_V"))
                    fprintf(stderr, "  liar: %-22s spec [%d]->[%d], wasm.def [%d]->[%d]\n",
                            r->name, sin, sout, d->npin, d->npout);
            } else {
                int lossy = 0;
                for (int j = 0; j < d->npin;  j++) if (is_carrier(d->pin[j]))  lossy = 1;
                for (int j = 0; j < d->npout; j++) if (is_carrier(d->pout[j])) lossy = 1;
                if (lossy) {
                    sig_carrier++;
                    /* NAME them, as liars are named above. A carrier is a slot where wasm.def does
                     * not state the spec's type, so anything derived FROM the signature — the
                     * validator's transfer function, and every obligation a VC backend emits — is
                     * derived from the wrong type. A bare count says how much debt exists but not
                     * which signature to fix next, and the work is per-signature. */
                    if (getenv("TRAP_LEDGER_V")) {
                        fprintf(stderr, "  carrier: %-24s wasm.def (", r->name);
                        for (int j = 0; j < d->npin; j++)
                            fprintf(stderr, "%s%s%s", j ? " " : "", d->pin[j],
                                    is_carrier(d->pin[j]) ? "*" : "");
                        fprintf(stderr, " -- ");
                        for (int j = 0; j < d->npout; j++)
                            fprintf(stderr, "%s%s%s", j ? " " : "", d->pout[j],
                                    is_carrier(d->pout[j]) ? "*" : "");
                        fprintf(stderr, ")   spec %s\n", r->type[0] ? r->type : "?");
                    }
                } else sig_exact++;
            }
        }

        /* §6.2 decode contract: the immediate signature must agree with the spec's. */
        if (r->ops) {
            const char* why;
            if (!imm_agrees(d, r->ops, &why)) {
                fprintf(stderr, "gen_trap_reasons: %s (%s) immediates disagree with the spec: "
                        "wasm.def has", r->name, r->key);
                for (int j = 0; j < d->nimm; j++) fprintf(stderr, " %s", d->imm[j]);
                if (!d->nimm) fprintf(stderr, " (none)");
                fprintf(stderr, ", spec says");
                for (int j = 0; j < toml_val_array_count(r->ops); j++) {
                    const char* s;
                    if (toml_val_as_string(toml_val_array_at(r->ops, j), &s)) fprintf(stderr, " %s", s);
                }
                if (!toml_val_array_count(r->ops)) fprintf(stderr, " (none)");
                fprintf(stderr, "\n");
                bad = 1;
            } else if (why) {
                imm_equiv++;
            }
        }

        for (int j = 0; j < d->nreasons; j++) {         /* wrong-reason guard = hard error */
            int ok = 0;
            for (int k = 0; k < r->ntraps; k++) if (r->traps[k] == d->reasons[j]) ok = 1;
            if (!ok) {
                fprintf(stderr, "gen_trap_reasons: %s (%s) declares `-> %s`, which the spec "
                        "table does not list among its traps\n",
                        r->name, r->key, g_name[d->reasons[j]]);
                bad = 1;
            }
        }
        /* §6.2 the memarg bounds contract: a declared guard width that disagrees with the spec's
         * natural alignment means the check covers the wrong number of bytes. Once the accessors
         * stop re-checking, that IS an out-of-bounds read/write — so it fails the build. */
        /* `align` is the §5.4.5 alignment EXPONENT, so the natural width is 1 << align. */
        if (d->membytes && (1 << r->align) != d->membytes) {
            fprintf(stderr, "gen_trap_reasons: %s (%s) guards %d bytes but the spec's natural "
                    "alignment is 2^%d = %d — the bounds check covers the wrong width\n",
                    r->name, r->key, d->membytes, r->align, 1 << r->align);
            bad = 1;
        }
        if (r->ntraps == 0) { notrap++; continue; }
        int got = 0;
        for (int k = 0; k < r->ntraps; k++) if (has_reason(d, r->traps[k])) got++;
        if (got == r->ntraps)  declared++;
        else if (got > 0)      partial++;
        else {
            hidden++;
            /* §7 names this the "unverifiable" list and asks for it as a FIRST-CLASS
             * output, not just a count — a number alone says how much debt there is but
             * not which contract to write next. Each row prints the reasons the spec
             * demands, so the missing `error:` guard can be read straight off it. */
            if (getenv("TRAP_LEDGER_V")) {
                fprintf(stderr, "  native-hidden: %-24s needs:", r->name);
                for (int k = 0; k < r->ntraps; k++) fprintf(stderr, " %s;", g_name[r->traps[k]]);
                fputc('\n', stderr);
            }
        }
    }
    /* This ledger measures DECLARATION, not behaviour: a native-hidden op may well
     * report the right reason at runtime (the substrate names it) — it just does so
     * where no contract is visible. That is the distinction the VC backend cares
     * about, and why this count is higher than the .wast trap-reason mismatch count. */
    fprintf(o, "trap coverage ledger (%d spec instructions):\n", g_ninstr);
    fprintf(o, "  %4d declared      every spec trap has an `error:` guard\n", declared);
    fprintf(o, "  %4d partial       some reasons declared, some still hidden\n", partial);
    fprintf(o, "  %4d native-hidden traps per spec, no guard — cause carried by a native,%s\n",
            hidden, getenv("TRAP_LEDGER_V") ? "" : " (TRAP_LEDGER_V=1 names them)");
    fprintf(o, "                     so the contract is invisible to a verifier\n");
    fprintf(o, "  %4d no-trap       spec defines no trap, and none is declared\n", notrap);
    fprintf(o, "  %4d unmapped      in the spec table, absent from wasm.def\n", unmapped);
    fprintf(o, "decode contract (§6.2): immediate signatures agree with the spec; "
               "%d via a declared arity equivalence\n", imm_equiv);
    fprintf(o, "signature honesty (§6.3, vs the spec `type` column):\n");
    fprintf(o, "  %4d exact         arity and slot types match the spec\n", sig_exact);
    fprintf(o, "  %4d carrier       arity matches; a slot is a carrier (any/word/addr/externref)\n", sig_carrier);
    fprintf(o, "  %4d LIAR          arity disagrees — the native moves the stack behind the signature\n", sig_liar);
    fprintf(o, "  %4d variadic      declared `[count]`; no static arity to compare\n", sig_variadic);
    fprintf(o, "  %4d polymorphic   spec type is stack-polymorphic (t*); no static arity\n", sig_poly);
    if (unmapped) {
        fprintf(stderr, "gen_trap_reasons: %d spec instructions did not join to wasm.def "
                "(set TRAP_LEDGER_V=1 to list them) — the ledger must cover every row\n", unmapped);
        bad = 1;
    }
    return bad;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <instructions.toml> <wasm.def> <out.h>\n", argv[0]);
        return 1;
    }
    bbq_arena ar; bbq_arena_init(&ar, 0);
    collect_reasons(argv[1], &ar);
    collect_declared(argv[2]);

    /* The cross-check: every declared guard name must name a spec trap message. */
    int bad = 0;
    for (int i = 0; i < g_ndecl; i++) {
        if (name_index(g_decl[i]) < 0) {
            fprintf(stderr, "gen_trap_reasons: wasm.def declares `-> %s`, which is not a "
                            "trap message in %s\n", g_decl[i], argv[1]);
            bad = 1;
        }
    }
    if (bad) return 1;

    FILE* o = fopen(argv[3], "wb");
    if (!o) { fprintf(stderr, "gen_trap_reasons: cannot write %s\n", argv[3]); return 1; }

    fprintf(o,
"/* GENERATED by tools/gen_trap_reasons.c from spec/instructions.toml + spec/wasm.def.\n"
" * Do not edit. The message text is the reference interpreter's, carried by the\n"
" * `traps` column of the spec table; this header is the single place it lives on\n"
" * the execution side, as jav_err_str is on the validation side. */\n"
"#ifndef JAV_TRAP_REASON_H\n"
"#define JAV_TRAP_REASON_H\n"
"\n"
"/* X(Name, \"message\") over the spec's complete trap vocabulary, in table order. */\n"
"#define JAV_TRAP_REASONS(X) \\\n");
    for (int i = 0; i < g_nreasons; i++)
        fprintf(o, "    X(%s, \"%s\")%s\n", g_name[i], g_msg[i],
                i + 1 < g_nreasons ? " \\" : "");

    fprintf(o,
"\n"
"typedef enum {\n"
"    JAV_TRAP_NONE = 0,\n"
"#define X(n, s) JAV_TRAP_##n,\n"
"    JAV_TRAP_REASONS(X)\n"
"#undef X\n"
"    JAV_TRAP_REASON_COUNT\n"
"} jav_trap_reason_t;\n"
"\n"
"static inline const char* jav_trap_reason_str(jav_trap_reason_t r) {\n"
"    switch (r) {\n"
"#define X(n, s) case JAV_TRAP_##n: return s;\n"
"    JAV_TRAP_REASONS(X)\n"
"#undef X\n"
"    case JAV_TRAP_NONE: case JAV_TRAP_REASON_COUNT: break;\n"
"    }\n"
"    return \"trap\";\n"
"}\n"
"\n"
"/* Bridge from opgen's guard code to a reason. A MACRO, for two reasons that both\n"
" * bite: it expands inside the generated handler/stencil bodies, i.e. AFTER\n"
" * runtime_api.h has declared OPGEN_ERR_* — so the mapping is written over the\n"
" * NAMES and never assumes opgen's declaration-order numbering — and because `e`\n"
" * is a literal at every guard site the whole chain folds to a constant. A real\n"
" * function would be an unresolved symbol in a copy-and-patch stencil (jitterator\n"
" * rejects any non-_HOLE_ relocation). */\n");
    if (g_ndecl == 0) {
        fprintf(o, "#define JAV_REASON_OF_OPGEN(e) ((void)(e), JAV_TRAP_NONE)\n");
    } else {
        fprintf(o, "#define JAV_REASON_OF_OPGEN(e) ( \\\n");
        for (int i = 0; i < g_ndecl; i++)
            fprintf(o, "    (e) == OPGEN_ERR_%s ? JAV_TRAP_%s : \\\n", g_decl[i], g_decl[i]);
        fprintf(o, "    JAV_TRAP_NONE)\n");
    }
    fprintf(o, "\n#endif /* JAV_TRAP_REASON_H */\n");
    fclose(o);

    fprintf(stderr, "gen_trap_reasons: %d spec trap reasons, %d declared by wasm.def\n",
            g_nreasons, g_ndecl);
    collect_defops(argv[2]);
    int gate = ledger(stderr);
    bbq_arena_free(&ar);
    return gate;
}
