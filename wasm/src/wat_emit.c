/*
 * wat_emit.c — the §6 text. See wat_emit.h.
 *
 * The division of labour: the builder (wat_tree.c) decided every TOKEN; the
 * generated labeller chose every RULE (recorded per node by the grammar's
 * own actions, which fire postorder and so cannot print); this file decides
 * only the whitespace, which §6.1.1 makes free. A group lays flat exactly
 * when the line has room for its whole flat width — a test against computed
 * values, not an estimate, and test_wast's `wat width honesty` counter holds
 * the computation to the emitted bytes over both corpora.
 *
 * Flat width of a group = its rule costs (each rule's own literal bytes,
 * from the same generator loop that wrote the render rows) + the pooled
 * payload widths + one separator between adjacent pieces + its referenced
 * roots, recursively. The memo below and the printer walk the SAME slot
 * list, so they cannot count different pieces.
 */
#include "wat_emit.h"

#include <string.h>

#include "bbq_hmap.h"
#include "bbq_vec.h"
#include "wat_layout.h"
#include "wat_render.h"
#include "wat_tree.h"

/* The recording the grammar's actions write: node → chosen rule id. One
 * emit runs at a time (water is a tool, not a library thread pool), so the
 * live map is file-static for the actions to find. */
static bbq_hmap* g_rec;

static wat_emit_stats_t g_stats;
void wat_emit_stats(wat_emit_stats_t* out) { *out = g_stats; }
void wat_emit_stats_reset(void) { memset(&g_stats, 0, sizeof g_stats); }

/* Why the last wat_emit_module returned 0 — the builder's error when the
 * forest refused, JAV_E_NONE when the engine side failed. */
static jav_err_t g_last_err;
static const char* g_last_stage;
jav_err_t wat_emit_last_error(const char** stage) {
    if (stage) *stage = g_last_stage ? g_last_stage : "";
    return g_last_err;
}

void wat_rec(wat_tnode_t* node, int rule, struct wat_layout_burg_ctx_t* ctx) {
    (void)ctx;
    if (g_rec) bbq_hmap_put(g_rec, (uint64_t)(uintptr_t)node, (void*)(uintptr_t)rule);
}

typedef struct {
    const wat_forest_t* f;
    int                 width;
    char*               out;     /* bbq_vec */
    bbq_hmap            rec;     /* node → rule id */
    bbq_hmap            wmemo;   /* node → flat width + 1 */
    int                 col;
    int                 line_pieces;   /* break opportunities on the current line */
    int                 failed;
} we_t;

static int rule_of(we_t* e, const wat_tnode_t* n) {
    return (int)(uintptr_t)bbq_hmap_get(&e->rec, (uint64_t)(uintptr_t)n);
}

/* ── the flat width, memoized ───────────────────────────────────────────── */

static uint32_t flat_w(we_t* e, const wat_tnode_t* n) {
    void* hit = bbq_hmap_get(&e->wmemo, (uint64_t)(uintptr_t)n);
    if (hit) return (uint32_t)(uintptr_t)hit - 1;
    int rule = rule_of(e, n);
    if (rule <= 0 || rule >= WAT_RULE_COUNT) { e->failed = 1; return 0; }
    const wat_render_row_t* row = &wat_render_rows[rule];
    uint32_t w = row->cost;
    int first = (row->head == NULL);
    for (const char* s = row->slots; *s; s++) {
        switch (*s) {
        case 'P':
            if (n->ptxt != WAT_TNODE_NONE) { w += (first ? 0 : 1) + n->pw; first = 0; }
            break;
        case 'A':
            for (uint32_t i = 0; i < n->nav; i++) {
                w += (first ? 0 : 1) + e->f->atoms[n->av + i].w;
                first = 0;
            }
            break;
        case '0': {
            const wat_tnode_t* c = n->kids[0];
            while (c && c->tag == WAT_TAG_OP_CONS) {
                w += (first ? 0 : 1) + flat_w(e, c->kids[0]);
                first = 0;
                c = c->kids[1];
            }
            break;
        }
        case 'r':
            for (uint32_t i = 0; i < n->nr1; i++) {
                w += (first ? 0 : 1) + flat_w(e, e->f->roots[n->r1 + i]);
                first = 0;
            }
            break;
        case 's':
            for (uint32_t i = 0; i < n->nr2; i++) {
                w += (first ? 0 : 1) + flat_w(e, e->f->roots[n->r2 + i]);
                first = 0;
            }
            break;
        case 'T':   /* "(then" + ")" are in row->cost; the sep and contents are not */
            w += (first ? 0 : 1);
            first = 0;
            for (uint32_t i = 0; i < n->nr1; i++)
                w += 1 + flat_w(e, e->f->roots[n->r1 + i]);
            break;
        case 'E':   /* optional: " (else" + contents + ")" */
            if (n->nr2) {
                w += (first ? 0 : 1) + 6;
                first = 0;
                for (uint32_t i = 0; i < n->nr2; i++)
                    w += 1 + flat_w(e, e->f->roots[n->r2 + i]);
            }
            break;
        default:
            break;   /* 'K' rides the '0' chain walk */
        }
    }
    bbq_hmap_put(&e->wmemo, (uint64_t)(uintptr_t)n, (void*)(uintptr_t)(w + 1));
    return w;
}

/* ── printing ───────────────────────────────────────────────────────────── */

static void put(we_t* e, const char* s, size_t len) {
    for (size_t i = 0; i < len; i++) bbq_vec_push(e->out, s[i]);
    e->col += (int)len;
}
static void puts_(we_t* e, const char* s) { put(e, s, strlen(s)); }

/* Close the current line's accounting: an over-width line with more than one
 * piece had a break available (a bug the gate counts); one piece means one
 * unbreakable token, counted apart, never hidden. */
static void end_line(we_t* e) {
    if (e->col > e->width) {
        if (e->line_pieces > 1) {
            g_stats.long_lines++;
            if (getenv("WAT_TREE_VV")) {
                size_t n = bbq_vec_len(e->out), s = n;
                while (s > 0 && e->out[s - 1] != '\n') s--;
                fprintf(stderr, "wat_emit: long line (%d pieces): %.*s\n",
                        e->line_pieces, (int)(n - s), e->out + s);
            }
        } else {
            g_stats.atom_overflows++;
        }
    }
    e->line_pieces = 0;
}
static void nl(we_t* e, int indent) {
    end_line(e);
    bbq_vec_push(e->out, '\n');
    for (int i = 0; i < indent; i++) bbq_vec_push(e->out, ' ');
    e->col = indent;
}

static void pr_group(we_t* e, const wat_tnode_t* n, int indent, int reserve);

/* One piece inside a group: flat stays on the line, broken opens a fresh
 * one. */
static void pr_sep(we_t* e, int broken, int indent, int* first) {
    if (*first) { *first = 0; }
    else if (broken) nl(e, indent);
    else puts_(e, " ");
    e->line_pieces++;
}

/* The last slot this node renders anything for — the piece the group's own
 * ")" (and every pending ancestor ")") glues after, which is what the fit
 * tests below must reserve room for. */
static const char* last_render_slot(const wat_tnode_t* n, const wat_render_row_t* row) {
    const char* last = NULL;
    for (const char* s = row->slots; *s; s++) {
        int nonempty = 0;
        switch (*s) {
        case 'P': nonempty = (n->ptxt != WAT_TNODE_NONE); break;
        case 'A': nonempty = n->nav > 0; break;
        case '0': nonempty = (n->nkids > 0 && n->kids[0] &&
                              n->kids[0]->tag == WAT_TAG_OP_CONS); break;
        case 'r': nonempty = n->nr1 > 0; break;
        case 's': nonempty = n->nr2 > 0; break;
        case 'T': nonempty = 1; break;
        case 'E': nonempty = n->nr2 > 0; break;
        default: break;
        }
        if (nonempty) last = s;
    }
    return last;
}

/* `reserve` is the closers that will glue directly after this group's last
 * byte — its ancestors' pending ")" run. The fit test charges them, so the
 * line that ends "…))))" was budgeted for its whole tail. */
static void pr_group(we_t* e, const wat_tnode_t* n, int indent, int reserve) {
    int rule = rule_of(e, n);
    if (rule <= 0 || rule >= WAT_RULE_COUNT) { e->failed = 1; return; }
    const wat_render_row_t* row = &wat_render_rows[rule];
    int broken = (e->col + (int)flat_w(e, n) + reserve > e->width);
    /* Past this column the indent stops growing: §6 nests arbitrarily deep,
     * and an indent wider than the page would make every line an overflow
     * no break could fix. */
    int cap = e->width - 32;
    if (cap < 2) cap = 2;
    int inner = indent + 2;
    if (inner > cap) inner = cap;
    int own = row->head ? 1 : 0;
    const char* last = last_render_slot(n, row);
    size_t mark = bbq_vec_len(e->out);   /* the flat self-check reads back */
    if (row->head) {
        puts_(e, "(");
        puts_(e, row->head);
    }
    int first = (row->head == NULL);
    for (const char* s = row->slots; *s; s++) {
        int tail = (s == last) ? reserve + own : own;   /* closers after slot's last item */
        switch (*s) {
        case 'P':
            if (n->ptxt != WAT_TNODE_NONE) {
                /* The payload names the group; it stays on the head line. */
                if (!first) puts_(e, " ");
                first = 0;
                e->line_pieces++;
                puts_(e, e->f->pool + n->ptxt);
            }
            break;
        case 'A':
            for (uint32_t i = 0; i < n->nav; i++) {
                const wat_atom_t* at = &e->f->atoms[n->av + i];
                int res = (i + 1 == n->nav) ? tail : 0;
                if (first) first = 0;
                else if (broken && e->col + 1 + (int)at->w + res > e->width) nl(e, inner);
                else puts_(e, " ");
                e->line_pieces++;
                puts_(e, e->f->pool + at->off);
            }
            break;
        case '0': {
            const wat_tnode_t* c = n->kids[0];
            while (c && c->tag == WAT_TAG_OP_CONS) {
                int is_last = (c->kids[1] == NULL || c->kids[1]->tag != WAT_TAG_OP_CONS);
                pr_sep(e, broken, inner, &first);
                pr_group(e, c->kids[0], broken ? inner : indent, is_last ? tail : 0);
                c = c->kids[1];
            }
            break;
        }
        case 'r':
            for (uint32_t i = 0; i < n->nr1; i++) {
                pr_sep(e, broken, inner, &first);
                pr_group(e, e->f->roots[n->r1 + i], broken ? inner : indent,
                         (i + 1 == n->nr1) ? tail : 0);
            }
            break;
        case 's':
            for (uint32_t i = 0; i < n->nr2; i++) {
                pr_sep(e, broken, inner, &first);
                pr_group(e, e->f->roots[n->r2 + i], broken ? inner : indent,
                         (i + 1 == n->nr2) ? tail : 0);
            }
            break;
        case 'T': {
            pr_sep(e, broken, inner, &first);
            puts_(e, "(then");
            for (uint32_t i = 0; i < n->nr1; i++) {
                if (broken) nl(e, inner + 2);
                else        puts_(e, " ");
                e->line_pieces++;
                pr_group(e, e->f->roots[n->r1 + i], broken ? inner + 2 : indent,
                         (i + 1 == n->nr1) ? tail + 1 : 0);
            }
            puts_(e, ")");
            break;
        }
        case 'E': {
            if (!n->nr2) break;
            pr_sep(e, broken, inner, &first);
            puts_(e, "(else");
            for (uint32_t i = 0; i < n->nr2; i++) {
                if (broken) nl(e, inner + 2);
                else        puts_(e, " ");
                e->line_pieces++;
                pr_group(e, e->f->roots[n->r2 + i], broken ? inner + 2 : indent,
                         (i + 1 == n->nr2) ? tail + 1 : 0);
            }
            puts_(e, ")");
            break;
        }
        default:
            break;
        }
    }
    if (row->head) {
        /* A closer that no longer fits wraps to the group's own indent — a
         * ")" is a token and the whitespace before it is free (§6.1.1). A
         * flat group can never take this branch: its fit test charged the
         * closer, so the self-check below still reads contiguous bytes. */
        if (e->col + 1 > e->width) nl(e, indent < cap ? indent : cap);
        puts_(e, ")");
    }
    if (!broken) {
        g_stats.flat_groups++;
        if (bbq_vec_len(e->out) - mark != flat_w(e, n)) g_stats.width_mismatches++;
    }
}

/* ── the drive: label, reduce (record), then print ──────────────────────── */

int wat_emit_module(const jav_module_t* m, const wat_check_ctx_t* cx, int width,
                    bbq_arena* a, const char** out, size_t* out_len) {
    (void)m;
    jav_err_t err = JAV_E_NONE;
    g_last_err = JAV_E_NONE;
    g_last_stage = NULL;
    wat_forest_t f;
    if (!wat_tree_build(m, cx, a, &f, &err)) {
        g_last_err = err;
        g_last_stage = "build";
        return 0;
    }

    we_t e;
    memset(&e, 0, sizeof e);
    e.f = &f;
    e.width = width > 0 ? width : 100;
    bbq_hmap_init(&e.rec, 256);
    bbq_hmap_init(&e.wmemo, 256);

    /* The goal nonterminal per class, resolved by name against the generated
     * grammar so a renumbering on either side is a loud failure, not a skew.
     * The class order here is the schema's declaration order — the same
     * order the node header's WAT_CLS_* enum was generated in. */
    static const char* const cls_names[WAT_CLS_COUNT] = {
        "decl", "subtype", "comptype", "typeuse", "externdesc", "instr", "operands",
    };
    int goalnt[WAT_CLS_COUNT];
    for (int c = 0; c < WAT_CLS_COUNT; c++) {
        goalnt[c] = 0;
        for (int nt = 1; nt <= 32; nt++) {
            const char* nm = wat_layout_burg_nt_name(nt);
            if (!nm || !strcmp(nm, "<invalid>")) break;
            if (!strcmp(nm, cls_names[c])) { goalnt[c] = nt; break; }
        }
        if (!goalnt[c]) { bbq_hmap_free(&e.rec); bbq_hmap_free(&e.wmemo); return 0; }
    }

    wat_layout_burg_ctx_t bctx;
    wat_layout_burg_ctx_init(&bctx);
    g_rec = &e.rec;
    for (uint32_t i = 0; i < f.nroots && !e.failed; i++) {
        wat_tnode_t* root = f.roots[i];
        burg_state_t* st = wat_layout_burg_label_root(root, &bctx);
        if (!st) { e.failed = 1; g_last_stage = "label"; break; }
        int goal = goalnt[wat_tt_tag_cls[root->tag]];
        if (wat_layout_burg_rule(st, goal) == 0) { e.failed = 1; g_last_stage = "cover"; break; }
        wat_layout_burg_reduce(root, st, goal, &bctx);
        if (wat_layout_burg_has_error(&bctx)) { e.failed = 1; g_last_stage = "reduce"; break; }
    }
    g_rec = NULL;

    if (!e.failed) {
        for (uint32_t d = 0; d < f.ndecls; d++) {
            e.col = 2;   /* every field sits at the module's one indent */
            e.line_pieces = 0;
            put(&e, "  ", 2);
            pr_group(&e, f.roots[f.decls[d]], 2, 0);
            end_line(&e);
            bbq_vec_push(e.out, '\n');
            if (e.failed) break;
        }
    }

    int ok = !e.failed;
    if (ok) {
        size_t n = bbq_vec_len(e.out);
        char* buf = bbq_arena_alloc(a, n + 12 + 3);
        memcpy(buf, "(module\n", 8);
        memcpy(buf + 8, e.out, n);
        memcpy(buf + 8 + n, ")\n", 3);
        *out = buf;
        *out_len = 8 + n + 2;
    }
    wat_layout_burg_ctx_free(&bctx);
    bbq_hmap_free(&e.rec);
    bbq_hmap_free(&e.wmemo);
    bbq_vec_free(e.out);
    return ok;
}
