/*
 * gen_wat_ir.c — water's render-tree generator. ONE walk over two
 * authorities, FOUR projections:
 *
 *   spec/wat_module.asdl  (via its asdl --json sidecar + verbatim text)
 *   spec/instructions.toml (the in-tree TOML parser)
 *        │
 *        ├── src/gen/wat_ir.asdl      the module half VERBATIM ⊕ one
 *        │                            constructor per instruction (497) ⊕ the
 *        │                            §6.5.11 operand spine
 *        ├── src/gen/wat_layout.burg  TERM ids in the combined file's own
 *        │                            declaration order (== the node tag
 *        │                            enum; test_wat_layout compares the two
 *        │                            ends); one rule per §6 production;
 *        │                            cost = the rule's own literal bytes
 *        └── src/gen/wat_render.h     the SAME literals as per-rule segment
 *                                     rows, plus the (prefix,op)→tag map, the
 *                                     nonterminal census and the production
 *                                     inventory test_wat_layout checks
 *                                     against spec/wat.peg
 *
 * Because cost and segments come out of one loop iteration they cannot
 * disagree; test_wast's `wat width honesty` counter gates the sum against
 * emitted bytes over both corpora anyway.
 *
 * The json sidecar is machine output ("--json <file>: ... for downstream
 * tools"), so the fixed-shape reader below consumes generated output, not the
 * hand-written .asdl — the .asdl itself is copied through byte-for-byte, so
 * the hand file stays the authority for everything the module half says.
 *
 * Width accounting (mirrored by wat_emit.c's memo, gated by test_wast's
 * `wat width honesty` counter):
 *   - a rule's burg cost = its OWN unconditional literals: "(" + head + ")"
 *     (+ "(then"/")" for if), 0 for headless rows;
 *   - every other byte is a PIECE (payload text, atom, kid tree, referenced
 *     root, else-arm) and pieces are joined by single spaces the memo counts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bbq_arena.h"
#include "toml/toml_doc.h"

static const char* g_argv0 = "gen_wat_ir";
static void die(const char* msg, const char* arg) {
    fprintf(stderr, "%s: %s%s%s\n", g_argv0, msg, arg ? ": " : "", arg ? arg : "");
    exit(1);
}

static char* read_file(const char* path, long* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) die("cannot open", path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) die("cannot read", path);
    fclose(f);
    buf[n] = 0;
    if (out_len) *out_len = n;
    return buf;
}

/* ── a fixed-shape JSON reader for the asdl sidecar ─────────────────────────
 * Machine-emitted: no comments, plain ASCII identifiers, known nesting. */
typedef struct jv jv;
struct jv {
    enum { JV_NULL, JV_BOOL, JV_NUM, JV_STR, JV_ARR, JV_OBJ } t;
    char*  s;
    double n;
    int    b;
    jv**   items; int nitems;      /* JV_ARR */
    char** keys;  jv** vals; int nkeys;  /* JV_OBJ */
};
typedef struct { const char* p; const char* path; } jp;

static void jskip(jp* c) { while (*c->p && isspace((unsigned char)*c->p)) c->p++; }
static jv* jparse(jp* c);

static char* jstring(jp* c) {
    if (*c->p != '"') die("json: expected string", c->path);
    c->p++;
    size_t cap = 32, n = 0;
    char* s = malloc(cap);
    while (*c->p && *c->p != '"') {
        char ch = *c->p++;
        if (ch == '\\') {
            char e = *c->p++;
            switch (e) {
            case 'n': ch = '\n'; break; case 't': ch = '\t'; break;
            case 'r': ch = '\r'; break; case '"': case '\\': case '/': ch = e; break;
            default: die("json: unsupported escape in sidecar", c->path);
            }
        }
        if (n + 1 >= cap) { cap *= 2; s = realloc(s, cap); }
        s[n++] = ch;
    }
    if (*c->p != '"') die("json: unterminated string", c->path);
    c->p++;
    s[n] = 0;
    return s;
}

static jv* jnew(int t) { jv* v = calloc(1, sizeof *v); v->t = t; return v; }

static jv* jparse(jp* c) {
    jskip(c);
    if (*c->p == '{') {
        jv* v = jnew(JV_OBJ);
        c->p++;
        jskip(c);
        if (*c->p == '}') { c->p++; return v; }
        for (;;) {
            jskip(c);
            char* k = jstring(c);
            jskip(c);
            if (*c->p != ':') die("json: expected ':'", c->path);
            c->p++;
            jv* val = jparse(c);
            v->keys = realloc(v->keys, (v->nkeys + 1) * sizeof *v->keys);
            v->vals = realloc(v->vals, (v->nkeys + 1) * sizeof *v->vals);
            v->keys[v->nkeys] = k; v->vals[v->nkeys] = val; v->nkeys++;
            jskip(c);
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == '}') { c->p++; return v; }
            die("json: expected ',' or '}'", c->path);
        }
    }
    if (*c->p == '[') {
        jv* v = jnew(JV_ARR);
        c->p++;
        jskip(c);
        if (*c->p == ']') { c->p++; return v; }
        for (;;) {
            jv* item = jparse(c);
            v->items = realloc(v->items, (v->nitems + 1) * sizeof *v->items);
            v->items[v->nitems++] = item;
            jskip(c);
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == ']') { c->p++; return v; }
            die("json: expected ',' or ']'", c->path);
        }
    }
    if (*c->p == '"') { jv* v = jnew(JV_STR); v->s = jstring(c); return v; }
    if (!strncmp(c->p, "true", 4))  { c->p += 4; jv* v = jnew(JV_BOOL); v->b = 1; return v; }
    if (!strncmp(c->p, "false", 5)) { c->p += 5; jv* v = jnew(JV_BOOL); return v; }
    if (!strncmp(c->p, "null", 4))  { c->p += 4; return jnew(JV_NULL); }
    if (*c->p == '-' || isdigit((unsigned char)*c->p)) {
        jv* v = jnew(JV_NUM);
        char* end;
        v->n = strtod(c->p, &end);
        c->p = end;
        return v;
    }
    die("json: unexpected byte", c->path);
    return NULL;
}

static jv* jget(jv* o, const char* k) {
    if (!o || o->t != JV_OBJ) return NULL;
    for (int i = 0; i < o->nkeys; i++)
        if (!strcmp(o->keys[i], k)) return o->vals[i];
    return NULL;
}

/* ── the module-half render table ───────────────────────────────────────────
 * Keyed by constructor name and cross-checked both directions against the
 * schema — an entry here with no schema constructor is an error, a schema
 * constructor with no entry here is an error. §6's module syntax is flat
 * records, so every module constructor is a LEAF (burgc counts a field as a
 * pattern child only when its type is a non-enum sum, and the module half
 * bottoms out in enum-like sums); references between constructs ride the
 * r1/r2 spans, each target its own label root. Slot codes (wat_render.h's
 * vocabulary, in render order):
 *   P payload text   A atom vector   0 operand spine (kid 0)   K spine rest
 *   T then-arm(r1)   E else-arm(r2)  r roots r1   s roots r2
 */
typedef struct {
    const char* ctor;
    const char* head;   /* NULL = headless: no parens, no cost of its own */
    const char* slots;
} mod_row_t;

static const mod_row_t MOD[] = {
    { "W_type",        "type",    "Pr"   },  /* §6.6.2  */
    { "W_rec",         "rec",     "r"    },  /* §6.4.7  */
    { "W_import",      "import",  "Pr"   },  /* §6.6.11 */
    { "W_func",        "func",    "PsAr" },  /* §6.6.7: id, typeuse, locals, body */
    { "W_table",       "table",   "Pr"   },  /* §6.6.6  */
    { "W_memory",      "memory",  "P"    },  /* §6.6.5  */
    { "W_global",      "global",  "Pr"   },  /* §6.6.4  */
    { "W_tag",         "tag",     "Pr"   },  /* §6.6.3  */
    { "W_export",      "export",  "P"    },  /* §6.6.12 */
    { "W_start",       "start",   "P"    },  /* §6.6.10 */
    { "W_elem",        "elem",    "PrAs" },  /* §6.6.9  */
    { "W_data",        "data",    "PrA"  },  /* §6.6.8  */
    { "W_custom",      "@custom", "PA"   },  /* §7.7.3  */
    { "W_sub",         NULL,      "r"    },  /* §6.4.7 abbrev: bare comptype  */
    { "W_subx",        "sub",     "Pr"   },  /* §6.4.7 explicit sub           */
    { "W_comp_func",   "func",    "A"    },  /* §6.4.6  */
    { "W_comp_struct", "struct",  "A"    },  /* §6.4.6  */
    { "W_comp_array",  "array",   "P"    },  /* §6.4.6  */
    { "W_typeuse",     NULL,      "PA"   },  /* §6.4.15: "(type k)" + echo    */
    { "W_ed_func",     "func",    "Pr"   },  /* §6.6.11 */
    { "W_ed_table",    "table",   "P"    },
    { "W_ed_mem",      "memory",  "P"    },
    { "W_ed_global",   "global",  "P"    },
    { "W_ed_tag",      "tag",     "Pr"   },
};
enum { NMOD = sizeof MOD / sizeof MOD[0] };

static const mod_row_t* mod_find(const char* ctor) {
    for (int i = 0; i < NMOD; i++)
        if (!strcmp(MOD[i].ctor, ctor)) return &MOD[i];
    return NULL;
}

/* A rule's own literal bytes (see the header comment). */
static int rule_cost(const mod_row_t* r) {
    int c = 0;
    if (r->head) c += 2 + (int)strlen(r->head);   /* "(" head ")" */
    for (const char* s = r->slots; *s; s++)
        if (*s == 'T') c += 6;                     /* "(then" ")" — unconditional on if */
    return c;
}

/* ── the MAPS inventory: spec/wat.peg production → what renders it ──────────
 * kind: "nt" = a burg nonterminal; "atom" = a pooled token the builder
 * renders; "list" = an atom vector under the engine's fill policy; "roots" =
 * a span of label roots the engine iterates; "frame" = the engine's module
 * loop. test_wat_layout checks BOTH directions against the peg file's
 * inventory. */
typedef struct { const char* prod; const char* target; const char* kind; } map_row_t;
static const map_row_t MAPS[] = {
    { "Wat",            "module",   "frame" },
    { "TypeField",      "decl",     "nt" },
    { "RecField",       "decl",     "nt" },
    { "ImportField",    "decl",     "nt" },
    { "FuncField",      "decl",     "nt" },
    { "TableField",     "decl",     "nt" },
    { "MemoryField",    "decl",     "nt" },
    { "GlobalField",    "decl",     "nt" },
    { "TagField",       "decl",     "nt" },
    { "ExportField",    "decl",     "nt" },
    { "StartField",     "decl",     "nt" },
    { "ElementField",   "decl",     "nt" },
    { "DataField",      "decl",     "nt" },
    { "RecMemberP",     "decl",     "nt" },     /* a rec member renders as (type …) */
    { "SubTypeP",       "subtype",  "nt" },
    { "CompType",       "comptype", "nt" },
    { "Param",          "av",       "list" },
    { "BParam",         "av",       "list" },
    { "Result",         "av",       "list" },
    { "Local",          "av",       "list" },
    { "FieldDecl",      "av",       "list" },
    { "DataString",     "av",       "list" },
    { "Catch",          "av",       "list" },
    { "ElemList",       "av",       "list" },
    { "ValType",        "ptxt",     "atom" },
    { "RefType",        "ptxt",     "atom" },
    { "RefTypeVal",     "ptxt",     "atom" },
    { "HeapType",       "ptxt",     "atom" },
    { "StorageTypeP",   "ptxt",     "atom" },
    { "FieldTypeP",     "ptxt",     "atom" },
    { "Limits",         "ptxt",     "atom" },
    { "Name",           "ptxt",     "atom" },
    { "IdxRef",         "ptxt",     "atom" },
    { "BlockType",      "ptxt",     "atom" },
    { "Expr",           "roots",    "roots" },
    { "OffsetClause",   "roots",    "roots" },
    { "ElemExpr",       "roots",    "roots" },
    { "Instr",          "instr",    "nt" },
    { "LinearInstr",    "instr",    "nt" },
    { "IdxOp",          "instr",    "nt" },
    { "CallIndirectOp", "instr",    "nt" },
    { "NumOp",          "instr",    "nt" },
    { "FloatOp",        "instr",    "nt" },
    { "MemArgOp",       "instr",    "nt" },
    { "BrTableOp",      "instr",    "nt" },
    { "SelectTOp",      "instr",    "nt" },
    { "RefNullOp",      "instr",    "nt" },
    { "RefCastOp",      "instr",    "nt" },
    { "BrOnCastOp",     "instr",    "nt" },
    { "V128ConstOp",    "instr",    "nt" },
    { "V128ShuffleOp",  "instr",    "nt" },
    { "BlockOp",        "instr",    "nt" },
    { "IfOp",           "instr",    "nt" },
    { "TryTableOp",     "instr",    "nt" },
    { "FoldedPlain",    "operands", "nt" },
    { "FoldedInstr",    "operands", "nt" },
    /* §6.4.15 typeuse and §6.6.11's import descriptors are productions of
     * the SPEC that wat.peg inlines into their call sites, so the classes
     * that render them appear against those hosts — the relation stays
     * two-directional without inventing production names the peg lacks. */
    { "FuncField",      "typeuse",    "nt" },
    { "TagField",       "typeuse",    "nt" },
    { "ImportField",    "externdesc", "nt" },
    { "ImportField",    "typeuse",    "nt" },
};
enum { NMAPS = sizeof MAPS / sizeof MAPS[0] };

/* The nonterminal census wat_render.h publishes, so the inventory check
 * can walk from the grammar back to the productions. */
static const char* const NTS[] = { "grp", "decl", "subtype", "comptype",
                                   "typeuse", "externdesc", "instr", "operands" };
enum { NNTS = sizeof NTS / sizeof NTS[0] };

/* ── instructions.toml ─────────────────────────────────────────────────── */
typedef struct {
    const char* name;      /* .wat mnemonic */
    const char* shape;
    int prefix;            /* 0, or 0xFB/0xFC/0xFD */
    int op;
    char ctor[80];         /* I_<sanitized mnemonic> */
} instr_t;

static void sanitize(const char* mn, char* out, size_t cap) {
    size_t n = snprintf(out, cap, "I_%s", mn);
    if (n >= cap) die("mnemonic too long", mn);
    for (char* p = out; *p; p++)
        if (*p == '.') *p = '_';
}

int main(int argc, char** argv) {
    g_argv0 = argv[0];
    if (argc != 5)
        die("usage", "gen_wat_ir <wat_module.asdl> <wat_module.json> <instructions.toml> <outdir>");
    const char* asdl_path = argv[1];
    const char* json_path = argv[2];
    const char* toml_path = argv[3];
    const char* outdir    = argv[4];

    /* 1. The module half: json for order/shape, text for verbatim copy. */
    long asdl_len = 0;
    char* asdl_text = read_file(asdl_path, &asdl_len);
    char* json_text = read_file(json_path, NULL);
    jp cur = { json_text, json_path };
    jv* root = jparse(&cur);
    jv* defs = jget(root, "definitions");
    if (!defs || defs->t != JV_ARR || defs->nitems == 0)
        die("no definitions in", json_path);

    /* The combined declaration-order constructor list; tags = position. */
    enum { MAXC = 600 };
    const char* ctor_name[MAXC];
    const char* ctor_cls[MAXC];
    int nctor = 0;

    for (int d = 0; d < defs->nitems; d++) {
        jv* def = defs->items[d];
        const char* cls = jget(def, "name")->s;
        jv* types = jget(def, "types");
        for (int t = 0; t < types->nitems; t++) {
            jv* ty = types->items[t];
            const char* nm = jget(ty, "name")->s;
            jv* fields = jget(ty, "fields");
            int arity = fields ? fields->nitems : 0;
            const mod_row_t* mr = mod_find(nm);
            if (!mr) die("schema constructor has no render-table row", nm);
            if (arity != 0)
                die("module constructors are leaves; references ride r1/r2", nm);
            if (nctor >= MAXC) die("too many constructors", NULL);
            ctor_name[nctor] = nm;
            ctor_cls[nctor] = cls;
            nctor++;
        }
    }
    int nmodule = nctor;
    /* Reverse leg of the cross-check: every table row names a constructor. */
    for (int i = 0; i < NMOD; i++) {
        int seen = 0;
        for (int c = 0; c < nmodule; c++)
            if (!strcmp(ctor_name[c], MOD[i].ctor)) seen = 1;
        if (!seen) die("render-table row has no schema constructor", MOD[i].ctor);
    }

    /* 2. The instruction half. */
    bbq_arena arena;
    bbq_arena_init(&arena, 1 << 20);
    long toml_len = 0;
    char* toml_text = read_file(toml_path, &toml_len);
    toml_doc_t* doc = toml_parse(toml_text, (int)toml_len, &arena);
    if (toml_doc_has_errors(doc)) die("toml parse errors in", toml_path);
    const toml_val_t* arr = toml_tbl_get(toml_doc_root(doc), "instr");
    int ninstr = toml_val_array_count(arr);
    if (ninstr <= 0) die("no [[instr]] rows in", toml_path);
    instr_t* ins = calloc((size_t)ninstr, sizeof *ins);
    for (int i = 0; i < ninstr; i++) {
        const toml_tbl_t* row = toml_val_as_table(toml_val_array_at(arr, i));
        if (!row) die("instr row is not a table", NULL);
        if (!toml_val_as_string(toml_tbl_get(row, "name"), &ins[i].name) ||
            !toml_val_as_string(toml_tbl_get(row, "shape"), &ins[i].shape))
            die("instr row missing name/shape", NULL);
        const toml_val_t* opc = toml_tbl_get(row, "opcode");
        int64_t v;
        if (toml_val_as_int(opc, &v)) {
            ins[i].prefix = 0;
            ins[i].op = (int)v;
        } else if (toml_val_array_count(opc) == 2) {
            int64_t p, s;
            toml_val_as_int(toml_val_array_at(opc, 0), &p);
            toml_val_as_int(toml_val_array_at(opc, 1), &s);
            ins[i].prefix = (int)p;
            ins[i].op = (int)s;
        } else
            die("instr opcode is neither int nor [prefix,sub]", ins[i].name);
        sanitize(ins[i].name, ins[i].ctor, sizeof ins[i].ctor);
        /* Two instructions may share a mnemonic (§6.5.2: `select` is 0x1B
         * untyped and 0x1C typed). The constructor gets the opcode appended —
         * systematic, so no mnemonic ever needs a hand-picked alias. */
        for (int j = 0; j < i; j++)
            if (!strcmp(ins[j].ctor, ins[i].ctor)) {
                size_t len = strlen(ins[i].ctor);
                snprintf(ins[i].ctor + len, sizeof ins[i].ctor - len, "_x%02x",
                         (unsigned)ins[i].op);
                for (int k = 0; k < i; k++)
                    if (!strcmp(ins[k].ctor, ins[i].ctor))
                        die("mnemonic sanitization collided twice", ins[i].name);
            }
    }

    char path[512];

    /* ── projection 1: the combined schema ─────────────────────────────── */
    snprintf(path, sizeof path, "%s/wat_ir.asdl", outdir);
    FILE* fa = fopen(path, "w");
    if (!fa) die("cannot write", path);
    char* last_brace = strrchr(asdl_text, '}');
    if (!last_brace) die("no closing brace in", asdl_path);
    fprintf(fa, "// AUTO-GENERATED by tools/gen_wat_ir.c — do not edit.\n");
    fprintf(fa, "// spec/wat_module.asdl VERBATIM (the module half's authority), plus one\n");
    fprintf(fa, "// constructor per spec/instructions.toml row and the §6.5.11 operand spine.\n");
    fwrite(asdl_text, 1, (size_t)(last_brace - asdl_text), fa);
    fprintf(fa, "\n    // ── generated: the instruction half, one constructor per mnemonic ──\n");
    for (int i = 0; i < ninstr; i++)
        fprintf(fa, "    %s %s(operands a0)\n",
                i == 0 ? "instr =" : "          |", ins[i].ctor);
    fprintf(fa, "          attributes (u32 pw, u32 ptxt, u32 av, u32 nav, u32 r1, u32 nr1, u32 r2, u32 nr2)\n\n");
    fprintf(fa, "    // §6.5.11 rule 1: the folded operand sequence, as a spine.\n");
    fprintf(fa, "    operands = W_op_cons(instr a0, operands a1)\n");
    fprintf(fa, "             | W_op_nil\n");
    fprintf(fa, "             attributes (u32 pw, u32 ptxt, u32 av, u32 nav, u32 r1, u32 nr1, u32 r2, u32 nr2)\n");
    fprintf(fa, "}\n");
    fclose(fa);

    /* ── projections 2+3: the grammar and the render table, one loop ───── */
    snprintf(path, sizeof path, "%s/wat_layout.burg", outdir);
    FILE* fb = fopen(path, "w");
    if (!fb) die("cannot write", path);
    snprintf(path, sizeof path, "%s/wat_render.h", outdir);
    FILE* fr = fopen(path, "w");
    if (!fr) die("cannot write", path);

    fprintf(fb, "// AUTO-GENERATED by tools/gen_wat_ir.c — do not edit.\n");
    fprintf(fb, "// §6 as a labelled derivation: one nonterminal per syntactic class, one\n");
    fprintf(fb, "// rule per production. A rule's cost is the byte length of its OWN\n");
    fprintf(fb, "// literals — parens + head (+ if's then-arm) — emitted by the same loop\n");
    fprintf(fb, "// that writes those literals into wat_render.h, so the two cannot\n");
    fprintf(fb, "// disagree; test_wast's wat width honesty counter gates the sum against\n");
    fprintf(fb, "// emitted bytes anyway. Payload,\n");
    fprintf(fb, "// separators and referenced roots ride the width memo (wat_emit.c).\n");
    fprintf(fb, "// Actions RECORD the chosen rule per node (they fire postorder, so they\n");
    fprintf(fb, "// cannot print);\n");
    fprintf(fb, "// the emitter walks top-down from the recording.\n\n");
    fprintf(fb, "(. #include \"wat_layout_burg.h\" .)\n\n");
    fprintf(fb, "COMPILER wat_layout\n\n");
    for (int c = 0; c < nmodule; c++)
        fprintf(fb, "TERM %-16s = %d\n", ctor_name[c], c);
    for (int i = 0; i < ninstr; i++)
        fprintf(fb, "TERM %-32s = %d\n", ins[i].ctor, nmodule + i);
    fprintf(fb, "TERM %-16s = %d\n", "W_op_cons", nmodule + ninstr);
    fprintf(fb, "TERM %-16s = %d\n", "W_op_nil", nmodule + ninstr + 1);
    fprintf(fb, "\nSTART grp\n\nRULES\n\n");

    fprintf(fr, "// AUTO-GENERATED by tools/gen_wat_ir.c — do not edit.\n");
    fprintf(fr, "// The render side of gen/wat_layout.burg: for the rule the cover chose,\n");
    fprintf(fr, "// what to print. Rows are indexed by the RULE ID the grammar's own\n");
    fprintf(fr, "// actions record (wat_rec), so nothing depends on burgc's internal\n");
    fprintf(fr, "// numbering. Slot codes: P payload text, A atom vector, k/K module kid\n");
    fprintf(fr, "// 0/1, 0 operand spine, T then-arm (r1), E else-arm (r2), r roots r1,\n");
    fprintf(fr, "// s roots r2. head == NULL means a headless row: no parens, no cost.\n");
    fprintf(fr, "#ifndef WAT_RENDER_H\n#define WAT_RENDER_H\n\n#include <stdint.h>\n\n");
    fprintf(fr, "typedef struct {\n    const char* head;\n    const char* slots;\n");
    fprintf(fr, "    uint16_t    cost;   /* == the burg rule's cost, same loop */\n");
    fprintf(fr, "    uint16_t    tag;    /* the constructor this rule renders */\n} wat_render_row_t;\n\n");

    /* Rows accumulate here and print after the loop (counts come first). */
    enum { MAXR = 700 };
    struct { const char* head; const char* slots; int cost; int tag; } rows[MAXR];
    int nrules = 0;
    rows[nrules].head = NULL; rows[nrules].slots = ""; rows[nrules].cost = 0;
    rows[nrules].tag = 0;   /* rule 0 = "no rule", never recorded */
    nrules++;

    /* Module-half rules, in schema order — all leaves (see the MOD table). */
    for (int c = 0; c < nmodule; c++) {
        const mod_row_t* mr = mod_find(ctor_name[c]);
        int cost = rule_cost(mr);
        fprintf(fb, "%s: %s = %d (. wat_rec(node, %d, ctx); .);\n",
                ctor_cls[c], ctor_name[c], cost, nrules);
        rows[nrules].head = mr->head; rows[nrules].slots = mr->slots;
        rows[nrules].cost = cost; rows[nrules].tag = c;
        nrules++;
    }

    /* Instruction rules: "(mn" + payload + atoms + folded spine + bodies ")".
     * Slot layout by toml shape; every other immediate difference is payload
     * the BUILDER renders, so it never splits a rule. */
    fprintf(fb, "\n");
    for (int i = 0; i < ninstr; i++) {
        const char* slots = "PA0";                      /* the plain form */
        int cost = 2 + (int)strlen(ins[i].name);
        if (!strcmp(ins[i].shape, "block")) slots = "P0r";
        else if (!strcmp(ins[i].shape, "if")) { slots = "P0TE"; cost += 6; }
        else if (!strcmp(ins[i].shape, "trytable")) slots = "PA0r";
        fprintf(fb, "instr: %s(operands) = %d (. wat_rec(node, %d, ctx); .);\n",
                ins[i].ctor, cost, nrules);
        if (nrules >= MAXR) die("rule table overflow", NULL);
        rows[nrules].head = ins[i].name; rows[nrules].slots = slots;
        rows[nrules].cost = cost; rows[nrules].tag = nmodule + i;
        nrules++;
    }

    /* The spine, the start symbol, and its chains. */
    fprintf(fb, "\noperands: W_op_cons(instr, operands) = 0 (. wat_rec(node, %d, ctx); .);\n", nrules);
    rows[nrules].head = NULL; rows[nrules].slots = "0K";
    rows[nrules].cost = 0; rows[nrules].tag = nmodule + ninstr;
    nrules++;
    fprintf(fb, "operands: W_op_nil = 0 (. wat_rec(node, %d, ctx); .);\n", nrules);
    rows[nrules].head = NULL; rows[nrules].slots = "";
    rows[nrules].cost = 0; rows[nrules].tag = nmodule + ninstr + 1;
    nrules++;
    fprintf(fb, "\n// Every label root is one §6 group: a declaration or a statement.\n");
    fprintf(fb, "grp: decl = 0;\ngrp: instr = 0;\ngrp: subtype = 0;\n"
                "grp: comptype = 0;\ngrp: typeuse = 0;\ngrp: externdesc = 0;\n");
    fclose(fb);

    /* ── projection 3 continued: tables into wat_render.h ──────────────── */
    fprintf(fr, "enum { WAT_RULE_COUNT = %d, WAT_N_MODULE_CTOR = %d, WAT_N_INSTR = %d,\n"
                "       WAT_TAG_I_FIRST = %d, WAT_TAG_OP_CONS = %d, WAT_TAG_OP_NIL = %d };\n\n",
            nrules, nmodule, ninstr, nmodule, nmodule + ninstr, nmodule + ninstr + 1);
    fprintf(fr, "static const wat_render_row_t wat_render_rows[WAT_RULE_COUNT] __attribute__((unused)) = {\n");
    for (int r = 0; r < nrules; r++) {
        if (rows[r].head)
            fprintf(fr, "    { \"%s\", \"%s\", %d, %d },\n",
                    rows[r].head, rows[r].slots, rows[r].cost, rows[r].tag);
        else
            fprintf(fr, "    { 0, \"%s\", %d, %d },\n",
                    rows[r].slots, rows[r].cost, rows[r].tag);
    }
    fprintf(fr, "};\n\n");

    fprintf(fr, "/* (prefix, op) -> node tag, in instructions.toml order. The builder maps\n"
                " * a decoded jav_instr_t to its constructor through this, never through a\n"
                " * parallel count. */\n");
    fprintf(fr, "typedef struct { uint8_t prefix; uint16_t op; uint16_t tag; } wat_instr_tag_t;\n");
    fprintf(fr, "static const wat_instr_tag_t wat_instr_tags[WAT_N_INSTR] __attribute__((unused)) = {\n");
    for (int i = 0; i < ninstr; i++)
        fprintf(fr, "    { 0x%02x, 0x%03x, %d },\n", ins[i].prefix, ins[i].op, nmodule + i);
    fprintf(fr, "};\n\n");

    fprintf(fr, "/* The production inventory: spec/wat.peg production -> what renders it.\n * test_wat_layout checks both directions. */\n");
    fprintf(fr, "typedef struct { const char* prod; const char* target; const char* kind; } wat_map_row_t;\n");
    fprintf(fr, "static const wat_map_row_t wat_maps[] __attribute__((unused)) = {\n");
    for (int m = 0; m < NMAPS; m++)
        fprintf(fr, "    { \"%s\", \"%s\", \"%s\" },\n", MAPS[m].prod, MAPS[m].target, MAPS[m].kind);
    fprintf(fr, "};\nenum { WAT_N_MAPS = %d };\n\n", NMAPS);
    fprintf(fr, "static const char* const wat_nts[] __attribute__((unused)) = {\n");
    for (int k = 0; k < NNTS; k++)
        fprintf(fr, "    \"%s\",\n", NTS[k]);
    fprintf(fr, "};\nenum { WAT_N_NTS = %d };\n\n#endif /* WAT_RENDER_H */\n", NNTS);
    fclose(fr);

    fprintf(stderr, "gen_wat_ir: %d module constructors, %d instructions, %d rules\n",
            nmodule, ninstr, nrules - 1);
    return 0;
}
