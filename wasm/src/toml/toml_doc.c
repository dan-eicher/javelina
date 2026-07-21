/* toml_doc.c — TOML document resolver + typed accessor API.
 *
 * Consumes the flat item stream from the pegc-generated parser
 * (src/gen/toml_parser.c, grammar/toml.peg) and produces the
 * resolved tables-and-values document defined in
 * include/yoctojc/toml/toml_doc.h.
 *
 * Resolution enforces TOML v1.0 §5 (tables) and §6 (inline tables):
 *
 *   - A key cannot be defined twice in the same table.
 *   - `[a]` headers cannot be re-declared after the table already
 *     has an explicit-header mark.
 *   - `[a.b]` creates intermediate `a` if missing; that intermediate
 *     gets the "from_dotted_intermediate" mark so a later `[a]`
 *     header can formally define it, but not vice versa.
 *   - An inline-table (`{...}`) cannot be extended by a later header
 *     or dotted-key.
 *   - `[[foo]]` creates an array-of-tables entry; consecutive
 *     `[[foo]]` append elements. Each element is its own table.
 *   - Type-mismatch (`a = 1` followed by `[a]`) is an error.
 */

#include "yoctojc/toml/toml_doc.h"
#include "toml_ast.h"       /* generated from grammar/toml.asdl */
#include "toml_parser.h"    /* generated, declares toml_parse_ctx_t */
#include "peg_runtime.h"

#include "bbq_arena.h"
#include "bbq_vec.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Small arena-vec helpers ───────────────────────────────────── */

static void** doc_push(bbq_arena* a, void** old, int* count, void* item) {
    int n = *count;
    void** nw = (void**)bbq_arena_alloc(a, (size_t)(n + 1) * sizeof(void*));
    if (old && n > 0) memcpy(nw, old, (size_t)n * sizeof(void*));
    nw[n] = item;
    *count = n + 1;
    return nw;
}

static const char* doc_dup(bbq_arena* a, const char* s) {
    size_t n = strlen(s);
    char* d = (char*)bbq_arena_alloc(a, n + 1);
    memcpy(d, s, n + 1);
    return d;
}

static toml_doc_srcloc_t from_parser_loc(toml_srcloc loc) {
    toml_doc_srcloc_t out = { loc.line, loc.col };
    return out;
}

/* ── Error accumulation ────────────────────────────────────────── */

static void doc_err(toml_doc_t* doc, bbq_arena* a,
                    toml_doc_srcloc_t loc, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

static void doc_err(toml_doc_t* doc, bbq_arena* a,
                    toml_doc_srcloc_t loc, const char* fmt, ...) {
    toml_err_t* e = (toml_err_t*)bbq_arena_alloc(a, sizeof(*e));
    e->loc = loc;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
    doc->errors = (toml_err_t**)doc_push(
        a, (void**)doc->errors, &doc->errors_count, e);
}

/* ── Key-part name extraction ──────────────────────────────────── */

static const char* key_part_name(const toml_key_part_t* p) {
    if (!p) return NULL;
    if (p->tag == TOML_BAREKEY)   return p->bare_key.name;
    if (p->tag == TOML_QUOTEDKEY) return p->quoted_key.text;
    return NULL;
}

/* ── Table construction ────────────────────────────────────────── */

static toml_tbl_t* new_tbl(bbq_arena* a) {
    toml_tbl_t* t = (toml_tbl_t*)bbq_arena_alloc(a, sizeof(*t));
    memset(t, 0, sizeof(*t));
    return t;
}

static toml_val_t* new_val(bbq_arena* a, toml_val_type_t type,
                            toml_doc_srcloc_t loc) {
    toml_val_t* v = (toml_val_t*)bbq_arena_alloc(a, sizeof(*v));
    memset(v, 0, sizeof(*v));
    v->type = type;
    v->loc = loc;
    return v;
}

/* Search a table for a key by name (linear; tables are typically
 * small enough that a hash would be ceremony). Returns the entry
 * or NULL. */
static toml_tbl_entry_t* tbl_lookup(const toml_tbl_t* t, const char* key) {
    for (int i = 0; i < t->entries_count; i++) {
        if (strcmp(t->entries[i]->key, key) == 0) return t->entries[i];
    }
    return NULL;
}

/* Add a new entry to a table. Caller has already checked no
 * collision exists. */
static toml_tbl_entry_t* tbl_insert(toml_tbl_t* t, bbq_arena* a,
                                     const char* key, toml_val_t* v,
                                     toml_doc_srcloc_t loc) {
    toml_tbl_entry_t* e = (toml_tbl_entry_t*)bbq_arena_alloc(a, sizeof(*e));
    e->key = doc_dup(a, key);
    e->value = v;
    e->loc = loc;
    t->entries = (toml_tbl_entry_t**)doc_push(
        a, (void**)t->entries, &t->entries_count, e);
    return e;
}

/* ── Value resolution (convert parsed value → resolved value) ── */

static toml_val_t* resolve_value(const toml_value_t* pv, bbq_arena* a,
                                  toml_doc_t* doc);

static toml_val_t* resolve_value(const toml_value_t* pv, bbq_arena* a,
                                  toml_doc_t* doc) {
    toml_doc_srcloc_t loc = from_parser_loc(pv->loc);
    switch (pv->tag) {
    case TOML_VSTR: {
        toml_val_t* v = new_val(a, TOML_VT_STRING, loc);
        v->u.s = doc_dup(a, pv->vstr.text);
        return v;
    }
    case TOML_VINT: {
        toml_val_t* v = new_val(a, TOML_VT_INT, loc);
        v->u.i = pv->vint.n;
        return v;
    }
    case TOML_VFLOAT: {
        toml_val_t* v = new_val(a, TOML_VT_FLOAT, loc);
        v->u.f = pv->vfloat.v;
        return v;
    }
    case TOML_VBOOL: {
        toml_val_t* v = new_val(a, TOML_VT_BOOL, loc);
        v->u.b = pv->vbool.b;
        return v;
    }
    case TOML_VDATETIME: {
        toml_val_t* v = new_val(a, TOML_VT_DATETIME, loc);
        /* Copy from the parser's ASDL-generated type into the
         * public toml_dt_t shape so consumers don't need to
         * include generated headers. */
        toml_dt_t* dt = (toml_dt_t*)bbq_arena_alloc(a, sizeof(*dt));
        const toml_date_time_t* src = pv->vdate_time.dt;
        dt->year          = src->year;
        dt->month         = src->month;
        dt->day           = src->day;
        dt->hour          = src->hour;
        dt->minute        = src->minute;
        dt->second        = src->second;
        dt->nanosec       = src->nanosec;
        dt->tz_offset_min = src->tz_offset_min;
        dt->has_date      = src->has_date;
        dt->has_time      = src->has_time;
        dt->has_offset    = src->has_offset;
        v->u.dt = dt;
        return v;
    }
    case TOML_VARRAY: {
        toml_val_t* v = new_val(a, TOML_VT_ARRAY, loc);
        const toml_val_t** items = NULL;
        int n = 0;
        for (int i = 0; i < pv->varray.elems_count; i++) {
            toml_val_t* child = resolve_value(pv->varray.elems[i], a, doc);
            items = (const toml_val_t**)doc_push(a, (void**)items, &n, child);
        }
        v->u.array.items = items;
        v->u.array.count = n;
        return v;
    }
    case TOML_VINLINETABLE: {
        toml_val_t* v = new_val(a, TOML_VT_TABLE, loc);
        toml_tbl_t* t = new_tbl(a);
        t->inline_defined = true;
        for (int i = 0; i < pv->vinline_table.entries_count; i++) {
            const toml_key_value_t* kv = pv->vinline_table.entries[i];
            /* Inline-table entries must be simple (non-dotted) keys
             * per TOML v1.0 — actually dotted keys ARE permitted in
             * inline tables (§6). Resolve the key path against this
             * nascent table. */
            toml_tbl_t* target = t;
            toml_doc_srcloc_t kv_loc = from_parser_loc(kv->loc);
            for (int p = 0; p < kv->key->parts_count - 1; p++) {
                const char* name = key_part_name(kv->key->parts[p]);
                if (!name) continue;
                toml_tbl_entry_t* e = tbl_lookup(target, name);
                if (e) {
                    if (e->value->type != TOML_VT_TABLE) {
                        doc_err(doc, a, kv_loc,
                                "inline-table key '%s' already defined as non-table", name);
                        return v;
                    }
                    target = e->value->u.table;
                } else {
                    toml_val_t* nv = new_val(a, TOML_VT_TABLE, kv_loc);
                    nv->u.table = new_tbl(a);
                    nv->u.table->from_dotted_intermediate = true;
                    tbl_insert(target, a, name, nv, kv_loc);
                    target = nv->u.table;
                }
            }
            const char* leaf = key_part_name(
                kv->key->parts[kv->key->parts_count - 1]);
            if (!leaf) continue;
            if (tbl_lookup(target, leaf)) {
                doc_err(doc, a, kv_loc,
                        "duplicate key '%s' in inline table", leaf);
                continue;
            }
            toml_val_t* child = resolve_value(kv->v, a, doc);
            tbl_insert(target, a, leaf, child, kv_loc);
        }
        v->u.table = t;
        return v;
    }
    }
    /* Unreachable. */
    return new_val(a, TOML_VT_STRING, loc);
}

/* ── Key-path navigation ───────────────────────────────────────── */

/* Walk a key path from `from`, creating intermediate tables for
 * segments that don't exist. If an intermediate IS an existing
 * non-table value, report error and return NULL. Returns the
 * table that holds the final segment. Writes the final key into
 * *out_leaf_name. */
static toml_tbl_t* nav_kv_path(toml_tbl_t* from, const toml_key_path_t* kp,
                                toml_doc_srcloc_t loc,
                                const char** out_leaf_name,
                                bbq_arena* a, toml_doc_t* doc) {
    toml_tbl_t* cur = from;
    for (int i = 0; i < kp->parts_count - 1; i++) {
        const char* name = key_part_name(kp->parts[i]);
        if (!name) return NULL;
        toml_tbl_entry_t* e = tbl_lookup(cur, name);
        if (e) {
            if (e->value->type != TOML_VT_TABLE) {
                doc_err(doc, a, loc,
                        "key '%s' already defined as non-table, "
                        "cannot use as dotted-key prefix", name);
                return NULL;
            }
            if (e->value->u.table->inline_defined) {
                doc_err(doc, a, loc,
                        "cannot extend inline-table '%s' with dotted key", name);
                return NULL;
            }
            cur = e->value->u.table;
        } else {
            toml_val_t* nv = new_val(a, TOML_VT_TABLE, loc);
            toml_tbl_t* nt = new_tbl(a);
            nt->from_dotted_intermediate = true;
            nv->u.table = nt;
            tbl_insert(cur, a, name, nv, loc);
            cur = nt;
        }
    }
    const char* leaf = key_part_name(kp->parts[kp->parts_count - 1]);
    if (!leaf) return NULL;
    *out_leaf_name = leaf;
    return cur;
}

/* Navigate a table-header path from root. Creates intermediates.
 * Returns the target table (the one whose entries subsequent
 * keyvals will land in). Handles error cases:
 *  - Header targets an inline-table (error: can't extend).
 *  - Header targets an existing non-table (error).
 *  - Header re-declares an already-explicitly-defined table
 *    (error: no re-definition).
 */
static toml_tbl_t* nav_std_header(toml_tbl_t* root, const toml_key_path_t* kp,
                                   toml_doc_srcloc_t loc,
                                   bbq_arena* a, toml_doc_t* doc) {
    toml_tbl_t* cur = root;
    for (int i = 0; i < kp->parts_count; i++) {
        const char* name = key_part_name(kp->parts[i]);
        if (!name) return NULL;
        toml_tbl_entry_t* e = tbl_lookup(cur, name);
        if (e) {
            if (e->value->type == TOML_VT_TABLE) {
                toml_tbl_t* sub = e->value->u.table;
                if (sub->inline_defined) {
                    doc_err(doc, a, loc,
                            "cannot extend inline-table '%s' with header", name);
                    return NULL;
                }
                bool is_last = (i == kp->parts_count - 1);
                if (is_last && sub->explicit_header) {
                    doc_err(doc, a, loc,
                            "table '%s' already defined", name);
                    return NULL;
                }
                cur = sub;
            } else if (e->value->type == TOML_VT_ARRAY
                       && i == kp->parts_count - 1) {
                doc_err(doc, a, loc,
                        "cannot define table '%s' — already an array", name);
                return NULL;
            } else if (e->value->type == TOML_VT_ARRAY
                       && e->value->u.array.count > 0) {
                /* Array-of-tables: current element is the target. */
                const toml_val_t* last = e->value->u.array.items[e->value->u.array.count - 1];
                if (last->type == TOML_VT_TABLE) {
                    cur = last->u.table;
                } else {
                    doc_err(doc, a, loc,
                            "cannot descend into array '%s'", name);
                    return NULL;
                }
            } else {
                doc_err(doc, a, loc,
                        "key '%s' already defined as non-table", name);
                return NULL;
            }
        } else {
            toml_val_t* nv = new_val(a, TOML_VT_TABLE, loc);
            toml_tbl_t* nt = new_tbl(a);
            nv->u.table = nt;
            tbl_insert(cur, a, name, nv, loc);
            cur = nt;
        }
    }
    cur->explicit_header = true;
    cur->from_dotted_intermediate = false;
    return cur;
}

/* Navigate an array-table-header path from root. Final segment is
 * an array-of-tables; append a new empty table to it and return
 * that new table. */
static toml_tbl_t* nav_array_header(toml_tbl_t* root, const toml_key_path_t* kp,
                                     toml_doc_srcloc_t loc,
                                     bbq_arena* a, toml_doc_t* doc) {
    toml_tbl_t* cur = root;
    /* Walk intermediates (all but last), same rules as nav_std_header
     * for creation / extension. */
    for (int i = 0; i < kp->parts_count - 1; i++) {
        const char* name = key_part_name(kp->parts[i]);
        if (!name) return NULL;
        toml_tbl_entry_t* e = tbl_lookup(cur, name);
        if (e) {
            if (e->value->type == TOML_VT_TABLE) {
                if (e->value->u.table->inline_defined) {
                    doc_err(doc, a, loc,
                            "cannot extend inline-table '%s'", name);
                    return NULL;
                }
                cur = e->value->u.table;
            } else if (e->value->type == TOML_VT_ARRAY
                       && e->value->u.array.count > 0) {
                const toml_val_t* last = e->value->u.array.items[e->value->u.array.count - 1];
                if (last->type == TOML_VT_TABLE) cur = last->u.table;
                else {
                    doc_err(doc, a, loc, "cannot descend into '%s'", name);
                    return NULL;
                }
            } else {
                doc_err(doc, a, loc,
                        "key '%s' already defined as non-table/non-array", name);
                return NULL;
            }
        } else {
            toml_val_t* nv = new_val(a, TOML_VT_TABLE, loc);
            toml_tbl_t* nt = new_tbl(a);
            nv->u.table = nt;
            tbl_insert(cur, a, name, nv, loc);
            cur = nt;
        }
    }

    /* Final segment: array-of-tables. Create or append. */
    const char* leaf = key_part_name(kp->parts[kp->parts_count - 1]);
    if (!leaf) return NULL;
    toml_tbl_entry_t* e = tbl_lookup(cur, leaf);
    toml_val_t* arr;
    if (e) {
        if (e->value->type != TOML_VT_ARRAY) {
            doc_err(doc, a, loc,
                    "'%s' already defined with non-array type", leaf);
            return NULL;
        }
        arr = e->value;
    } else {
        arr = new_val(a, TOML_VT_ARRAY, loc);
        arr->u.array.items = NULL;
        arr->u.array.count = 0;
        tbl_insert(cur, a, leaf, arr, loc);
    }
    toml_val_t* elem = new_val(a, TOML_VT_TABLE, loc);
    toml_tbl_t* elem_t = new_tbl(a);
    elem_t->explicit_header = true;
    elem->u.table = elem_t;
    arr->u.array.items = (const toml_val_t**)doc_push(
        a, (void**)arr->u.array.items, &arr->u.array.count, elem);
    return elem_t;
}

/* ── Top-level driver ──────────────────────────────────────────── */

toml_doc_t* toml_resolve(const toml_document_t* parsed, bbq_arena* arena) {
    toml_doc_t* doc = (toml_doc_t*)bbq_arena_alloc(arena, sizeof(*doc));
    memset(doc, 0, sizeof(*doc));
    doc->root = new_tbl(arena);

    toml_tbl_t* cur = doc->root;

    for (int i = 0; i < parsed->items_count; i++) {
        const toml_item_t* it = parsed->items[i];
        switch (it->tag) {
        case TOML_KVITEM: {
            const toml_key_value_t* kv = it->kv_item.kv;
            toml_doc_srcloc_t loc = from_parser_loc(kv->loc);
            const char* leaf;
            toml_tbl_t* tgt = nav_kv_path(cur, kv->key, loc, &leaf, arena, doc);
            if (!tgt) break;
            if (tbl_lookup(tgt, leaf)) {
                doc_err(doc, arena, loc,
                        "duplicate key '%s'", leaf);
                break;
            }
            toml_val_t* v = resolve_value(kv->v, arena, doc);
            tbl_insert(tgt, arena, leaf, v, loc);
            break;
        }
        case TOML_TABLEHEADER: {
            toml_doc_srcloc_t loc = { 0, 0 };
            toml_tbl_t* t = nav_std_header(doc->root, it->table_header.path,
                                            loc, arena, doc);
            if (t) cur = t;
            break;
        }
        case TOML_ARRAYTABLEHEADER: {
            toml_doc_srcloc_t loc = { 0, 0 };
            toml_tbl_t* t = nav_array_header(doc->root,
                                              it->array_table_header.path,
                                              loc, arena, doc);
            if (t) cur = t;
            break;
        }
        }
    }
    return doc;
}

bool toml_doc_has_errors(const toml_doc_t* doc) {
    return doc && doc->errors_count > 0;
}

const toml_tbl_t* toml_doc_root(const toml_doc_t* doc) {
    return doc->root;
}

int toml_doc_error_count(const toml_doc_t* doc) {
    return doc ? doc->errors_count : 0;
}

const toml_err_t* toml_doc_error_at(const toml_doc_t* doc, int i) {
    if (!doc || i < 0 || i >= doc->errors_count) return NULL;
    return doc->errors[i];
}

/* One-shot parse + resolve. Hides the pegc parser context from
 * consumers. On parse failure, returns a doc with exactly one
 * error populated via toml_format_parse_error; root table is
 * empty but non-NULL so accessor API calls are safe. */
toml_doc_t* toml_parse(const char* src, int len, bbq_arena* arena) {
    toml_parse_ctx_t pctx = { .result = NULL };
    bbq_arena_init(&pctx.arena, 4096);
    peg_state p;
    toml_parser_init(&p, src, len);
    p.user_data = &pctx;

    if (!toml_parser_parse(&p)) {
        /* Build a minimal doc with the parse error attached. */
        toml_doc_t* doc = (toml_doc_t*)bbq_arena_alloc(arena, sizeof(*doc));
        memset(doc, 0, sizeof(*doc));
        doc->root = new_tbl(arena);
        toml_err_t* e = (toml_err_t*)bbq_arena_alloc(arena, sizeof(*e));
        e->loc.line = p.furthest.line ? p.furthest.line : 1;
        e->loc.col  = p.furthest.col  ? p.furthest.col  : 1;
        toml_format_parse_error(&p, e->message, (int)sizeof(e->message));
        doc->errors = (toml_err_t**)doc_push(
            arena, NULL, &doc->errors_count, e);
        bbq_arena_free(&pctx.arena);
        return doc;
    }

    toml_doc_t* doc = toml_resolve(pctx.result, arena);
    /* The parser's arena is no longer reachable from doc — the
     * resolver deep-copies strings into the caller arena. */
    bbq_arena_free(&pctx.arena);
    return doc;
}

/* ── Accessors ─────────────────────────────────────────────────── */

const toml_val_t* toml_tbl_get(const toml_tbl_t* t, const char* dotted_key) {
    if (!t || !dotted_key) return NULL;
    const char* p = dotted_key;
    const toml_tbl_t* cur = t;
    char seg[256];
    for (;;) {
        const char* dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len >= sizeof(seg)) return NULL;
        memcpy(seg, p, len);
        seg[len] = '\0';
        toml_tbl_entry_t* e = tbl_lookup(cur, seg);
        if (!e) return NULL;
        if (!dot) return e->value;
        if (e->value->type != TOML_VT_TABLE) return NULL;
        cur = e->value->u.table;
        p = dot + 1;
    }
}

int toml_tbl_entry_count(const toml_tbl_t* t) {
    return t ? t->entries_count : 0;
}

const toml_tbl_entry_t* toml_tbl_entry_at(const toml_tbl_t* t, int i) {
    if (!t || i < 0 || i >= t->entries_count) return NULL;
    return t->entries[i];
}

toml_val_type_t toml_val_type(const toml_val_t* v) {
    return v ? v->type : TOML_VT_STRING;
}

bool toml_val_as_string(const toml_val_t* v, const char** out) {
    if (!v || v->type != TOML_VT_STRING) return false;
    *out = v->u.s;
    return true;
}

bool toml_val_as_int(const toml_val_t* v, int64_t* out) {
    if (!v || v->type != TOML_VT_INT) return false;
    *out = v->u.i;
    return true;
}

bool toml_val_as_float(const toml_val_t* v, double* out) {
    if (!v || v->type != TOML_VT_FLOAT) return false;
    *out = v->u.f;
    return true;
}

bool toml_val_as_bool(const toml_val_t* v, bool* out) {
    if (!v || v->type != TOML_VT_BOOL) return false;
    *out = v->u.b;
    return true;
}

const toml_dt_t* toml_val_as_datetime(const toml_val_t* v) {
    if (!v || v->type != TOML_VT_DATETIME) return NULL;
    return v->u.dt;
}

const toml_tbl_t* toml_val_as_table(const toml_val_t* v) {
    if (!v || v->type != TOML_VT_TABLE) return NULL;
    return v->u.table;
}

int toml_val_array_count(const toml_val_t* v) {
    if (!v || v->type != TOML_VT_ARRAY) return 0;
    return v->u.array.count;
}

const toml_val_t* toml_val_array_at(const toml_val_t* v, int i) {
    if (!v || v->type != TOML_VT_ARRAY) return NULL;
    if (i < 0 || i >= v->u.array.count) return NULL;
    return v->u.array.items[i];
}

/* ── Parse-error formatter ─────────────────────────────────────── */

const char* toml_format_parse_error(const void* peg_state_ptr,
                                     char* buf, int buf_cap) {
    const peg_state* p = (const peg_state*)peg_state_ptr;
    int line = p->furthest.line ? p->furthest.line : 1;
    int col  = p->furthest.col  ? p->furthest.col  : 1;
    const char* fpos = p->furthest.pos ? p->furthest.pos : p->input;

    /* Find the line containing the failure position. */
    const char* ls = fpos;
    while (ls > p->input && ls[-1] != '\n') ls--;
    const char* le = fpos;
    while (le < p->end && *le != '\n' && *le != '\r') le++;

    int line_len = (int)(le - ls);
    if (line_len > 80) line_len = 80;

    /* Describe the character that blocked progress. */
    char ch_desc[16];
    if (fpos >= p->end) {
        snprintf(ch_desc, sizeof(ch_desc), "EOF");
    } else if ((unsigned char)*fpos < 0x20) {
        snprintf(ch_desc, sizeof(ch_desc), "\\x%02x", (unsigned char)*fpos);
    } else {
        snprintf(ch_desc, sizeof(ch_desc), "'%c'", *fpos);
    }

    int pad = col > 0 ? col - 1 : 0;
    if (pad > 80) pad = 80;

    snprintf(buf, (size_t)buf_cap,
        "TOML syntax error at line %d:%d (saw %s)\n"
        "  %.*s\n"
        "  %*s^",
        line, col, ch_desc,
        line_len, ls,
        pad, "");
    return buf;
}
