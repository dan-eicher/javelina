#include "wat_parser.h"



/* ── Init ────────────────────────────────────────────────── */

PEG_INTERNAL void peg_init(peg_state* p, const char* input, int length) {
    memset(p, 0, sizeof(*p));
    p->input = input;
    p->pos = input;
    p->end = input + length;
    p->line = 1;
    p->col = 1;
}

/* ── Position ────────────────────────────────────────────── */

PEG_INTERNAL int peg_line(const peg_state* p) { return p->line; }
PEG_INTERNAL int peg_col(const peg_state* p) { return p->col; }
PEG_INTERNAL bool peg_at_end(const peg_state* p) { return p->pos >= p->end; }
PEG_INTERNAL const char* peg_pos(const peg_state* p) { return p->pos; }
PEG_INTERNAL int peg_remaining(const peg_state* p) { return (int)(p->end - p->pos); }
PEG_INTERNAL char peg_peek_char(const peg_state* p) { return p->pos < p->end ? *p->pos : '\0'; }

/* ── Advance ─────────────────────────────────────────────── */

PEG_INTERNAL void peg_advance(peg_state* p) {
    if (p->pos >= p->end) return;
    if (*p->pos == '\n') { p->line++; p->col = 1; }
    else { p->col++; }
    p->pos++;
    if (p->pos > p->furthest.pos) {
        p->furthest.pos = p->pos;
        p->furthest.line = p->line;
        p->furthest.col = p->col;
    }
}

/* ── Save / Restore ──────────────────────────────────────── */

PEG_INTERNAL peg_mark peg_save(const peg_state* p) {
    peg_mark m = { p->pos, p->line, p->col };
    return m;
}

PEG_INTERNAL void peg_restore(peg_state* p, peg_mark m) {
    p->pos = m.pos;
    p->line = m.line;
    p->col = m.col;
}

/* ── Character matching ──────────────────────────────────── */

PEG_INTERNAL bool peg_match_char(peg_state* p, char c) {
    if (p->pos < p->end && *p->pos == c) {
        peg_advance(p);
        return true;
    }
    return false;
}

/* Length-carrying form — the emitter knows every literal's length at generation
 * time, so its call sites pass a CONSTANT: no strlen per match (measured 7.4M
 * calls compiling one real corpus), and the C compiler folds the fixed-size
 * memcmp of a short literal into direct byte compares. */
PEG_INTERNAL bool peg_match_n(peg_state* p, const char* str, int len) {
    if (p->pos + len > p->end) return false;
    if (memcmp(p->pos, str, (size_t)len) != 0) return false;
    for (int i = 0; i < len; i++) peg_advance(p);
    return true;
}

PEG_INTERNAL bool peg_match(peg_state* p, const char* str) {
    return peg_match_n(p, str, (int)strlen(str));
}

PEG_INTERNAL bool peg_match_charset(peg_state* p, bool (*fn)(char)) {
    if (p->pos < p->end && fn(*p->pos)) {
        peg_advance(p);
        return true;
    }
    return false;
}

/* ── Lookahead (no consume) ──────────────────────────────── */

PEG_INTERNAL bool peg_peek_at_n(const peg_state* p, const char* str, int len) {
    if (p->pos + len > p->end) return false;
    return memcmp(p->pos, str, (size_t)len) == 0;
}

PEG_INTERNAL bool peg_peek_at(const peg_state* p, const char* str) {
    return peg_peek_at_n(p, str, (int)strlen(str));
}

PEG_INTERNAL bool peg_peek_at_char(const peg_state* p, char c) {
    return p->pos < p->end && *p->pos == c;
}

PEG_INTERNAL bool peg_peek_charset(const peg_state* p, bool (*fn)(char)) {
    return p->pos < p->end && fn(*p->pos);
}

/* ── Span (consume while charset matches) ────────────────── */

PEG_INTERNAL void peg_span_chars(peg_state* p, bool (*fn)(char)) {
    while (p->pos < p->end && fn(*p->pos))
        peg_advance(p);
}

/* Scan forward to delimiter, capturing raw content as span. Skips
   over C-style `//` line comments and `/` `*` ... `*` `/` block
   comments — a delimiter inside a comment doesn't terminate the
   scan. This matters for embedded code blocks like `(. ... .)`
   where C source legitimately contains the close marker in `//`
   comment text. */
PEG_INTERNAL bool peg_scan_to(peg_state* p, const char* delim, peg_span* out) {
    int dlen = (int)strlen(delim);
    const char* start = p->pos;
    while (p->pos + dlen <= p->end) {
        if (p->pos + 1 < p->end && p->pos[0] == '/' && p->pos[1] == '/') {
            while (p->pos < p->end && *p->pos != '\n') peg_advance(p);
            continue;
        }
        if (p->pos + 1 < p->end && p->pos[0] == '/' && p->pos[1] == '*') {
            peg_advance(p); peg_advance(p);
            while (p->pos + 1 < p->end &&
                   !(p->pos[0] == '*' && p->pos[1] == '/')) {
                peg_advance(p);
            }
            if (p->pos + 1 < p->end) { peg_advance(p); peg_advance(p); }
            continue;
        }
        if (memcmp(p->pos, delim, (size_t)dlen) == 0) {
            out->ptr = start;
            out->len = (int)(p->pos - start);
            for (int i = 0; i < dlen; i++) peg_advance(p);
            return true;
        }
        peg_advance(p);
    }
    return false;
}

/* ── Whitespace + comment skipping ───────────────────────── */

PEG_INTERNAL void peg_set_whitespace(peg_state* p, bool (*fn)(char)) {
    p->ws_fn = fn;
    /* Tabulate the predicate once — it is a pure character classification, and
     * the skip loop paid an indirect call per character through it. */
    for (int c = 0; c < 256; c++) p->ws_tab[c] = fn ? fn((char)c) : false;
}

PEG_INTERNAL void peg_add_comment(peg_state* p, const char* open,
                            const char* close, bool nested, bool structured) {
    if (p->comment_count < PEG_MAX_COMMENTS) {
        p->comments[p->comment_count].open = open;
        p->comments[p->comment_count].close = close;
        p->comments[p->comment_count].open_len = (int)strlen(open);
        p->comments[p->comment_count].close_len = (int)strlen(close);
        p->comments[p->comment_count].nested = nested;
        p->comments[p->comment_count].structured = structured;
        p->comment_count++;
    }
}

/* A STRUCTURED skip element (e.g. WAT `(@id … )` annotations, §6.2.5): from `open`, walk to
 * the matching close, counting GENERIC ()-nesting, but consuming string literals (with \-escapes)
 * and inner line/block comments so their parens/quotes don't perturb the balance. The element is
 * recognized only if an idchar or `"` follows `open` (an annotid) — a bare `(@)` or `(@ ` is left
 * for the grammar to reject. Returns true (and advances past the close) iff it skipped one. */
/* §6.2.1 a source character is illegal if it is an ASCII control char other than the three
 * format-effectors (tab/LF/CR), or DEL — illegal even inside an annotation's token sequence. */
static bool peg_illegal_src(unsigned char c) {
    return (c < 0x20 && c != 0x09 && c != 0x0a && c != 0x0d) || c == 0x7f;
}
static int peg_hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* §6.3.5 idchar: alphanumeric + a fixed punctuation set (the annotid is idchar+ or a name). */
static bool peg_is_idchar(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
    case '-': case '.': case '/': case ':': case '<': case '=': case '>': case '?':
    case '@': case '\\': case '^': case '_': case '`': case '|': case '~': return true;
    default: return false;
    }
}
PEG_INTERNAL bool peg_skip_structured(peg_state* p, const peg_comment_spec* spec) {
    int open_len = spec->open_len;
    if (p->pos + open_len > p->end) return false;
    if (memcmp(p->pos, spec->open, (size_t)open_len) != 0) return false;
    char a = (p->pos + open_len < p->end) ? p->pos[open_len] : ' ';   /* §6.2.5 annotid = idchar+ | name */
    if (!(a == '"' || peg_is_idchar((unsigned char)a))) return false;
    peg_mark start = peg_save(p);                 /* a skip can't reject; on a malformed annotation,
                                                   * restore + return false so the grammar chokes on `(@`. */
    for (int i = 0; i < open_len; i++) peg_advance(p);
    if (p->pos + 1 < p->end && p->pos[0] == '"' && p->pos[1] == '"') { peg_restore(p, start); return false; }  /* §6.2.5 empty annotation id */
    int depth = 1;
    while (p->pos < p->end && depth > 0) {
        unsigned char c = (unsigned char)*p->pos;
        if (c == '"') {                                  /* string literal: skip with \-escapes */
            peg_advance(p);
            int cont = 0;                                /* §6.3.3 expected UTF-8 continuation bytes */
            while (p->pos < p->end && *p->pos != '"') {
                unsigned char sc = (unsigned char)*p->pos;
                if (sc < 0x20 || sc == 0x7f) { peg_restore(p, start); return false; }   /* raw control in a string → illegal */
                int b = -1;                              /* the effective byte this position contributes (for UTF-8 check) */
                if (sc == '\\' && p->pos + 1 < p->end) {
                    unsigned char e = (unsigned char)p->pos[1];
                    int h1 = peg_hexval(e), h2 = (p->pos + 2 < p->end) ? peg_hexval((unsigned char)p->pos[2]) : -1;
                    if (h1 >= 0 && h2 >= 0) { b = (h1 << 4) | h2; peg_advance(p); peg_advance(p); }   /* \HH byte */
                    else if (e == 'u') { while (p->pos < p->end && *p->pos != '}') peg_advance(p); b = -1; } /* \u{…} scalar (assume valid) */
                    else { peg_advance(p); b = e; }      /* \t \n \r \" \' \\ → an ASCII byte */
                    peg_advance(p);
                } else { b = sc; peg_advance(p); }        /* raw byte (≥0x80 = UTF-8 multibyte) */
                if (b >= 0) {                            /* run the UTF-8 state machine over effective bytes */
                    if (cont > 0) { if (b < 0x80 || b > 0xbf) { peg_restore(p, start); return false; } cont--; }
                    else if (b < 0x80) { /* ASCII */ }
                    else if (b >= 0xc2 && b <= 0xdf) cont = 1;
                    else if (b >= 0xe0 && b <= 0xef) cont = 2;
                    else if (b >= 0xf0 && b <= 0xf4) cont = 3;
                    else { peg_restore(p, start); return false; }   /* 0x80–0xc1, 0xf5–0xff lead → malformed UTF-8 */
                }
            }
            if (cont != 0) { peg_restore(p, start); return false; }                 /* truncated UTF-8 sequence */
            if (p->pos < p->end) peg_advance(p); else { peg_restore(p, start); return false; }  /* unterminated string */
        } else if (c == ';' && p->pos + 1 < p->end && p->pos[1] == ';') {   /* line comment */
            while (p->pos < p->end && *p->pos != '\n') peg_advance(p);
        } else if (c == '(' && p->pos + 1 < p->end && p->pos[1] == ';') {   /* block comment (nested) */
            int bd = 1; peg_advance(p); peg_advance(p);
            while (p->pos < p->end && bd > 0) {
                if (p->pos + 1 < p->end && p->pos[0] == '(' && p->pos[1] == ';') { bd++; peg_advance(p); peg_advance(p); }
                else if (p->pos + 1 < p->end && p->pos[0] == ';' && p->pos[1] == ')') { bd--; peg_advance(p); peg_advance(p); }
                else peg_advance(p);
            }
        } else if (c == '(') { depth++; peg_advance(p); }
        else if (c == ')') { depth--; peg_advance(p); }
        else if (peg_illegal_src(c) || c >= 0x80) { peg_restore(p, start); return false; }  /* control or non-ASCII outside a string → illegal */
        else peg_advance(p);
    }
    if (depth != 0) { peg_restore(p, start); return false; }   /* unterminated annotation → reject */
    return true;
}

PEG_INTERNAL bool peg_skip_comment(peg_state* p, const peg_comment_spec* spec) {
    if (spec->structured) return peg_skip_structured(p, spec);
    int open_len = spec->open_len;
    if (p->pos + open_len > p->end) return false;
    if (memcmp(p->pos, spec->open, (size_t)open_len) != 0) return false;
    for (int i = 0; i < open_len; i++) peg_advance(p);
    int close_len = spec->close_len;
    /* A single-LF close is the line-comment idiom; a line ends at LF or CR (or CRLF),
     * so such a comment terminates on a bare CR too — standard line-comment behavior. */
    int line_close = (close_len == 1 && spec->close[0] == '\n');
    int depth = 1;
    while (p->pos < p->end && depth > 0) {
        if (spec->nested && p->pos + open_len <= p->end &&
            memcmp(p->pos, spec->open, (size_t)open_len) == 0) {
            for (int i = 0; i < open_len; i++) peg_advance(p);
            depth++;
        } else if (line_close && (*p->pos == '\n' || *p->pos == '\r')) {
            peg_advance(p);
            depth--;
        } else if (p->pos + close_len <= p->end &&
                   memcmp(p->pos, spec->close, (size_t)close_len) == 0) {
            for (int i = 0; i < close_len; i++) peg_advance(p);
            depth--;
        } else {
            peg_advance(p);
        }
    }
    return true;
}

PEG_INTERNAL void peg_skip(peg_state* p) {
    /* The memo first. Backtracking re-enters at positions already skipped; the skip
     * is deterministic in pos, so the recorded (from → to, line, col) IS the answer. */
    if (p->pos == p->skip_from && p->skip_to) {
        p->pos  = p->skip_to;
        p->line = p->skip_to_line;
        p->col  = p->skip_to_col;
        return;
    }
    const char* from = p->pos;
    for (;;) {
        bool skipped = false;
        while (p->pos < p->end && p->ws_tab[(unsigned char)*p->pos]) {
            peg_advance(p);
            skipped = true;
        }
        bool found_comment = false;
        for (int i = 0; i < p->comment_count; i++) {
            /* First-byte rejection: at a non-comment position (almost all of them)
             * each spec dies on one byte compare instead of a memcmp. */
            if (p->pos < p->end && *p->pos != p->comments[i].open[0]) continue;
            if (peg_skip_comment(p, &p->comments[i])) {
                found_comment = true;
                break;
            }
        }
        if (!found_comment && !skipped) break;
    }
    p->skip_from    = from;
    p->skip_to      = p->pos;
    p->skip_to_line = p->line;
    p->skip_to_col  = p->col;
}

/* ── Error reporting ─────────────────────────────────────── */

PEG_INTERNAL void peg_expected(peg_state* p, const char* what) {
    if (p->error_count < PEG_MAX_ERRORS) {
        peg_error* e = &p->errors[p->error_count++];
        e->line = p->line;
        e->col = p->col;
        snprintf(e->message, PEG_ERROR_LEN, "expected %s", what);
    }
}

PEG_INTERNAL void peg_error_msg(peg_state* p, const char* msg) {
    if (p->error_count < PEG_MAX_ERRORS) {
        peg_error* e = &p->errors[p->error_count++];
        e->line = p->line;
        e->col = p->col;
        snprintf(e->message, PEG_ERROR_LEN, "%s", msg);
    }
}

PEG_INTERNAL bool peg_has_errors(const peg_state* p) {
    return p->error_count > 0;
}

static bool is_digit(char c) {
    return (c >= 48 && c <= 57);
}

static bool is_hexdig(char c) {
    return ((is_digit(c) || (c >= 97 && c <= 102)) || (c >= 65 && c <= 70));
}

static bool is_sign(char c) {
    return ((c == 43) || (c == 45));
}

static bool is_epref(char c) {
    return ((c == 101) || (c == 69));
}

static bool is_ppref(char c) {
    return ((c == 112) || (c == 80));
}

static bool is_letter(char c) {
    return ((c >= 97 && c <= 122) || (c >= 65 && c <= 90));
}

static bool is_symbol(char c) {
    return (((((((((((((((((((((((c == 33) || (c == 35)) || (c == 36)) || (c == 37)) || (c == 38)) || (c == 39)) || (c == 42)) || (c == 43)) || (c == 45)) || (c == 46)) || (c == 47)) || (c == 58)) || (c == 60)) || (c == 61)) || (c == 62)) || (c == 63)) || (c == 64)) || (c == 92)) || (c == 94)) || (c == 95)) || (c == 96)) || (c == 124)) || (c == 126));
}

static bool is_idchar(char c) {
    return ((is_digit(c) || is_letter(c)) || is_symbol(c));
}

static bool is_glue(char c) {
    return (is_idchar(c) || (c == 34));
}

static bool wat_ws_predicate(char c) {
    return ((((c == 32) || (c == 9)) || (c == 13)) || (c == 10));
}

static void wat_setup_skip(peg_state* p) {
    peg_set_whitespace(p, wat_ws_predicate);
    peg_add_comment(p, ";;", "\n", false, false);
    peg_add_comment(p, "(;", ";)", true, false);
    peg_add_comment(p, "(@", ")", false, true);
}

static bool wat_atom(peg_state* p, peg_span* out);
static bool wat_nat(peg_state* p, peg_span* out);
static bool wat_snum(peg_state* p, peg_span* out);
static bool wat_fnum(peg_state* p, peg_span* out);
static bool wat_hexnat(peg_state* p, peg_span* out);
static bool wat_id(peg_state* p, peg_span* out);
static bool wat_string(peg_state* p, peg_span* out);
static bool wat_parse_wat(peg_state* p);
static bool wat_parse_func_field(peg_state* p, uint32_t** func_tidx, jav_code_entry_t** entries);
static bool wat_parse_instr(peg_state* p, bbq_write_ctx_t* w);
static bool wat_parse_linear_instr(peg_state* p, bbq_write_ctx_t* w);
static bool wat_parse_folded_plain(peg_state* p, bbq_write_ctx_t* w);
static bool wat_parse_folded_instr(peg_state* p, bbq_write_ctx_t* w);
static bool wat_parse_idx_ref(peg_state* p, peg_span* out);
static bool wat_parse_call_indirect_op(peg_state* p, bbq_write_ctx_t* w, int shape, int sp0, int sp1);
static bool wat_parse_idx_op(peg_state* p, bbq_write_ctx_t* w, int shape, int sp0, int sp1);
static bool wat_parse_num_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_mem_arg_op(peg_state* p, bbq_write_ctx_t* w, int shape, int align);
static bool wat_parse_br_table_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_select_t_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_float_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_type_field(peg_state* p, jav_rec_type_t** types);
static bool wat_parse_rec_member_p(peg_state* p, jav_rec_member_t** members);
static bool wat_parse_rec_field(peg_state* p, jav_rec_type_t** types);
static bool wat_parse_storage_type_p(peg_state* p, jav_storage_type_t* out);
static bool wat_parse_field_type_p(peg_state* p, jav_field_type_t* out);
static bool wat_parse_field_decl(peg_state* p, jav_field_type_t** fields);
static bool wat_parse_comp_type(peg_state* p, jav_comp_type_t* out);
static bool wat_parse_sub_type_p(peg_state* p, jav_sub_type_t* out, uint8_t* head);
static bool wat_parse_param(peg_state* p, jav_val_type_t** list);
static bool wat_parse_b_param(peg_state* p, jav_val_type_t** list);
static bool wat_parse_result(peg_state* p, jav_val_type_t** list);
static bool wat_parse_local(peg_state* p, jav_val_type_t** list);
static bool wat_parse_val_type(peg_state* p, jav_val_type_t* out);
static bool wat_parse_ref_type_val(peg_state* p, jav_ref_type_t* out);
static bool wat_parse_limits(peg_state* p, jav_limits_t* out);
static bool wat_parse_expr(peg_state* p, jav_expr_t* out);
static bool wat_parse_offset_clause(peg_state* p, jav_expr_t* out);
static bool wat_parse_heap_type(peg_state* p, int64_t* ht);
static bool wat_parse_ref_type(peg_state* p, int* nullable, int64_t* ht);
static bool wat_parse_ref_null_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op, uint8_t prefix);
static bool wat_parse_ref_cast_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op, uint8_t prefix);
static bool wat_parse_br_on_cast_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_v128_const_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op);
static bool wat_parse_v128_shuffle_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op);
static bool wat_parse_block_type(peg_state* p, bbq_write_ctx_t* w);
static bool wat_parse_block_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_if_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_try_table_op(peg_state* p, bbq_write_ctx_t* w, int shape);
static bool wat_parse_catch(peg_state* p, wat_catch_t** out);
static bool wat_parse_name(peg_state* p, jav_name_t* out);
static bool wat_parse_export_field(peg_state* p, jav_export_t** exports);
static bool wat_parse_start_field(peg_state* p, uint32_t* start_func, int* has_start);
static bool wat_parse_memory_field(peg_state* p, jav_mem_entry_t** mems, jav_data_t** datas);
static bool wat_parse_global_field(peg_state* p, jav_global_t** globals);
static bool wat_parse_tag_field(peg_state* p, jav_tag_type_t** tags);
static bool wat_parse_table_field(peg_state* p, jav_table_t** tables, jav_elem_t** elems);
static bool wat_parse_data_string(peg_state* p, jav_byte_vec_t* out);
static bool wat_parse_data_field(peg_state* p, jav_data_t** datas);
static bool wat_parse_elem_expr(peg_state* p, jav_expr_t* out);
static bool wat_parse_elem_list(peg_state* p);
static bool wat_parse_element_field(peg_state* p, jav_elem_t** elems);
static bool wat_parse_import_field(peg_state* p);


void wat_parser_init(peg_state* p, const char* input, int length) {
    peg_init(p, input, length);
    wat_setup_skip(p);
}

bool wat_parser_parse(peg_state* p) {
    peg_skip(p);
    return wat_parse_wat(p);
}


static bool wat_atom(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (peg_at_end(p) || !is_letter(peg_peek_char(p))) return false;
    peg_advance(p);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_idchar(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok1 = true;
        } while(0);
        peg_restore(p, _m1);
        if (_ok1) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_nat(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "0x", 2)) break;
                if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        {
                            peg_mark _m3 = peg_save(p);
                            bool _ok3 = false;
                            do {
                                if (!peg_match_n(p, "_", 1)) break;
                                _ok3 = true;
                            } while(0);
                            if (!_ok3) peg_restore(p, _m3);
                        }
                        if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m5 = peg_save(p);
            bool _ok5 = false;
            do {
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (!peg_match_n(p, "_", 1)) break;
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) peg_restore(p, _m6);
                }
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok5 = true;
            } while(0);
            if (!_ok5) { peg_restore(p, _m5); break; }
        }
        {
            peg_mark _m7 = peg_save(p);
            bool _ok7 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok7 = true;
            } while(0);
            peg_restore(p, _m7);
            if (_ok7) return false;
        }
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_snum(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_sign(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                if (!peg_match_n(p, "0x", 2)) break;
                if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        {
                            peg_mark _m4 = peg_save(p);
                            bool _ok4 = false;
                            do {
                                if (!peg_match_n(p, "_", 1)) break;
                                _ok4 = true;
                            } while(0);
                            if (!_ok4) peg_restore(p, _m4);
                        }
                        if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m5 = peg_save(p);
            bool _ok5 = false;
            do {
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (!peg_match_n(p, "_", 1)) break;
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) peg_restore(p, _m6);
                }
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok5 = true;
            } while(0);
            if (!_ok5) { peg_restore(p, _m5); break; }
        }
    _choice_done1:;
    }
    {
        peg_mark _m7 = peg_save(p);
        bool _ok7 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok7 = true;
        } while(0);
        peg_restore(p, _m7);
        if (_ok7) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_fnum(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_sign(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                if (!peg_match_n(p, "0x", 2)) break;
                if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        {
                            peg_mark _m4 = peg_save(p);
                            bool _ok4 = false;
                            do {
                                if (!peg_match_n(p, "_", 1)) break;
                                _ok4 = true;
                            } while(0);
                            if (!_ok4) peg_restore(p, _m4);
                        }
                        if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        if (!peg_match_n(p, ".", 1)) break;
                        {
                            peg_mark _m6 = peg_save(p);
                            bool _ok6 = false;
                            do {
                                if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                                peg_advance(p);
                                for (;;) {
                                    peg_mark _m7 = peg_save(p);
                                    bool _ok7 = false;
                                    do {
                                        {
                                            peg_mark _m8 = peg_save(p);
                                            bool _ok8 = false;
                                            do {
                                                if (!peg_match_n(p, "_", 1)) break;
                                                _ok8 = true;
                                            } while(0);
                                            if (!_ok8) peg_restore(p, _m8);
                                        }
                                        if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
                                        peg_advance(p);
                                        _ok7 = true;
                                    } while(0);
                                    if (!_ok7) { peg_restore(p, _m7); break; }
                                }
                                _ok6 = true;
                            } while(0);
                            if (!_ok6) peg_restore(p, _m6);
                        }
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) peg_restore(p, _m5);
                }
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        if (peg_at_end(p) || !is_ppref(peg_peek_char(p))) break;
                        peg_advance(p);
                        {
                            peg_mark _m10 = peg_save(p);
                            bool _ok10 = false;
                            do {
                                if (peg_at_end(p) || !is_sign(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok10 = true;
                            } while(0);
                            if (!_ok10) peg_restore(p, _m10);
                        }
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        for (;;) {
                            peg_mark _m11 = peg_save(p);
                            bool _ok11 = false;
                            do {
                                {
                                    peg_mark _m12 = peg_save(p);
                                    bool _ok12 = false;
                                    do {
                                        if (!peg_match_n(p, "_", 1)) break;
                                        _ok12 = true;
                                    } while(0);
                                    if (!_ok12) peg_restore(p, _m12);
                                }
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok11 = true;
                            } while(0);
                            if (!_ok11) { peg_restore(p, _m11); break; }
                        }
                        _ok9 = true;
                    } while(0);
                    if (!_ok9) peg_restore(p, _m9);
                }
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m13 = peg_save(p);
            bool _ok13 = false;
            do {
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (!peg_match_n(p, "_", 1)) break;
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) peg_restore(p, _m14);
                }
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok13 = true;
            } while(0);
            if (!_ok13) { peg_restore(p, _m13); break; }
        }
        {
            peg_mark _m15 = peg_save(p);
            bool _ok15 = false;
            do {
                if (!peg_match_n(p, ".", 1)) break;
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        for (;;) {
                            peg_mark _m17 = peg_save(p);
                            bool _ok17 = false;
                            do {
                                {
                                    peg_mark _m18 = peg_save(p);
                                    bool _ok18 = false;
                                    do {
                                        if (!peg_match_n(p, "_", 1)) break;
                                        _ok18 = true;
                                    } while(0);
                                    if (!_ok18) peg_restore(p, _m18);
                                }
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok17 = true;
                            } while(0);
                            if (!_ok17) { peg_restore(p, _m17); break; }
                        }
                        _ok16 = true;
                    } while(0);
                    if (!_ok16) peg_restore(p, _m16);
                }
                _ok15 = true;
            } while(0);
            if (!_ok15) peg_restore(p, _m15);
        }
        {
            peg_mark _m19 = peg_save(p);
            bool _ok19 = false;
            do {
                if (peg_at_end(p) || !is_epref(peg_peek_char(p))) break;
                peg_advance(p);
                {
                    peg_mark _m20 = peg_save(p);
                    bool _ok20 = false;
                    do {
                        if (peg_at_end(p) || !is_sign(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok20 = true;
                    } while(0);
                    if (!_ok20) peg_restore(p, _m20);
                }
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m21 = peg_save(p);
                    bool _ok21 = false;
                    do {
                        {
                            peg_mark _m22 = peg_save(p);
                            bool _ok22 = false;
                            do {
                                if (!peg_match_n(p, "_", 1)) break;
                                _ok22 = true;
                            } while(0);
                            if (!_ok22) peg_restore(p, _m22);
                        }
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok21 = true;
                    } while(0);
                    if (!_ok21) { peg_restore(p, _m21); break; }
                }
                _ok19 = true;
            } while(0);
            if (!_ok19) peg_restore(p, _m19);
        }
    _choice_done1:;
    }
    {
        peg_mark _m23 = peg_save(p);
        bool _ok23 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok23 = true;
        } while(0);
        peg_restore(p, _m23);
        if (_ok23) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_hexnat(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "0x", 2)) return false;
    if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) return false;
    peg_advance(p);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    if (!peg_match_n(p, "_", 1)) break;
                    _ok1 = true;
                } while(0);
                if (!_ok1) peg_restore(p, _m1);
            }
            if (peg_at_end(p) || !is_hexdig(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok2 = true;
        } while(0);
        peg_restore(p, _m2);
        if (_ok2) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_id(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "$", 1)) break;
                if (!peg_match_n(p, "\"", 1)) break;
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        {
                            peg_mark _m3 = peg_save(p);
                            {
                                bool _ok4 = false;
                                do {
                                    if (!peg_match_n(p, "\\", 1)) break;
                                    if (peg_at_end(p)) break;
                                    peg_advance(p);
                                    _ok4 = true;
                                } while(0);
                                if (!_ok4) {
                                    peg_restore(p, _m3);
                                } else goto _choice_done3;
                            }
                            {
                                peg_mark _m5 = peg_save(p);
                                bool _ok5 = false;
                                do {
                                    if (!peg_match_n(p, "\"", 1)) break;
                                    _ok5 = true;
                                } while(0);
                                peg_restore(p, _m5);
                                if (_ok5) break;
                            }
                            if (peg_at_end(p)) break;
                            peg_advance(p);
                        _choice_done3:;
                        }
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                if (!peg_match_n(p, "\"", 1)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (!peg_match_n(p, "$", 1)) return false;
        if (peg_at_end(p) || !is_idchar(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m7 = peg_save(p);
            bool _ok7 = false;
            do {
                if (peg_at_end(p) || !is_idchar(peg_peek_char(p))) break;
                peg_advance(p);
                _ok7 = true;
            } while(0);
            if (!_ok7) { peg_restore(p, _m7); break; }
        }
        {
            peg_mark _m8 = peg_save(p);
            bool _ok8 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok8 = true;
            } while(0);
            peg_restore(p, _m8);
            if (_ok8) return false;
        }
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_string(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "\"", 1)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                {
                    bool _ok2 = false;
                    do {
                        if (!peg_match_n(p, "\\", 1)) break;
                        if (peg_at_end(p)) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        if (!peg_match_n(p, "\"", 1)) break;
                        _ok3 = true;
                    } while(0);
                    peg_restore(p, _m3);
                    if (_ok3) break;
                }
                if (peg_at_end(p)) break;
                peg_advance(p);
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (!peg_match_n(p, "\"", 1)) return false;
    {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok4 = true;
        } while(0);
        peg_restore(p, _m4);
        if (_ok4) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool wat_parse_wat(peg_state* p) {
    uint32_t start_func = 0; int has_start = 0;   /* start section */
       peg_span mid = {0,0};                   /* §6.6.13 optional module id — script-level, no struct field */
       jav_section_t s;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "module", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &mid)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_type_field(p, &AS_TYPES)) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok5 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_rec_field(p, &AS_TYPES)) break;
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok6 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_import_field(p)) break;
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_func_field(p, &AS_FTIDX, &AS_ENTRIES)) break;
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok8 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_table_field(p, &AS_TABLES, &AS_ELEMS)) break;
                        _ok8 = true;
                    } while(0);
                    if (!_ok8) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok9 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_memory_field(p, &AS_MEMS, &AS_DATAS)) break;
                        _ok9 = true;
                    } while(0);
                    if (!_ok9) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok10 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_global_field(p, &AS_GLOBALS)) break;
                        _ok10 = true;
                    } while(0);
                    if (!_ok10) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok11 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_export_field(p, &AS_EXPORTS)) break;
                        _ok11 = true;
                    } while(0);
                    if (!_ok11) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok12 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_element_field(p, &AS_ELEMS)) break;
                        _ok12 = true;
                    } while(0);
                    if (!_ok12) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok13 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_data_field(p, &AS_DATAS)) break;
                        _ok13 = true;
                    } while(0);
                    if (!_ok13) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                {
                    bool _ok14 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_tag_field(p, &AS_TAGS)) break;
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!wat_parse_start_field(p, &start_func, &has_start)) break;
            _choice_done3:;
            }
            wat_wbufs_recycle(CTX);
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) {                          /* pass 1 only collected $ids */
             bbq_vec_free(AS_TYPES); bbq_vec_free(AS_FTIDX); bbq_vec_free(AS_ENTRIES);
             bbq_vec_free(AS_TABLES); bbq_vec_free(AS_MEMS); bbq_vec_free(AS_GLOBALS);
             bbq_vec_free(AS_EXPORTS); bbq_vec_free(AS_ELEMS); bbq_vec_free(AS_DATAS); bbq_vec_free(AS_TAGS);
             bbq_vec_free(CTX->iimports); bbq_vec_free(CTX->iexports);
             peg_skip(p); if (!peg_at_end(p)) { report_parse_error(p); return false; }
             return true;
         }
         /* §6.4.15: inserted types (funcs + blocktypes) come after all explicit ones. */
         for (int i = 0; i < (int)bbq_vec_len(CTX->ins); i++) bbq_vec_push(AS_TYPES, CTX->ins[i]);
         bbq_vec_free(CTX->ins);
         jav_module_t* m = CTX->mod;
         m->magic = 0x6d736100u; m->version = 1u;
         if (bbq_vec_len(AS_TYPES)) {                        /* type section (id 1) */
             memset(&s, 0, sizeof s); s.id = 1; s.body.tag = 1;
             WAT_FREEZE(AS_TYPES, s.body.u.case_1.types);
             s.body.u.case_1.count = (uint32_t)s.body.u.case_1.types.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_TYPES);
         if (bbq_vec_len(CTX->iimports)) {                /* import section (id 2) */
             memset(&s, 0, sizeof s); s.id = 2; s.body.tag = 2;
             WAT_FREEZE(CTX->iimports, s.body.u.case_2.imports);
             s.body.u.case_2.count = (uint32_t)s.body.u.case_2.imports.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(CTX->iimports);
         if (bbq_vec_len(AS_FTIDX)) {                    /* function section (id 3) */
             memset(&s, 0, sizeof s); s.id = 3; s.body.tag = 3;
             WAT_FREEZE(AS_FTIDX, s.body.u.case_3.type_indices);
             s.body.u.case_3.count = (uint32_t)s.body.u.case_3.type_indices.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_FTIDX);
         if (bbq_vec_len(AS_TABLES)) {                       /* table section (id 4) */
             memset(&s, 0, sizeof s); s.id = 4; s.body.tag = 4;
             WAT_FREEZE(AS_TABLES, s.body.u.case_4.tables);
             s.body.u.case_4.count = (uint32_t)s.body.u.case_4.tables.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_TABLES);
         if (bbq_vec_len(AS_MEMS)) {                         /* memory section (id 5) */
             memset(&s, 0, sizeof s); s.id = 5; s.body.tag = 5;
             WAT_FREEZE(AS_MEMS, s.body.u.case_5.mems);
             s.body.u.case_5.count = (uint32_t)s.body.u.case_5.mems.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_MEMS);
         if (bbq_vec_len(AS_GLOBALS)) {                      /* global section (id 6) */
             memset(&s, 0, sizeof s); s.id = 6; s.body.tag = 6;
             WAT_FREEZE(AS_GLOBALS, s.body.u.case_6.globals);
             s.body.u.case_6.count = (uint32_t)s.body.u.case_6.globals.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_GLOBALS);
         for (int i = 0; i < (int)bbq_vec_len(CTX->iexports); i++)   /* merge inline (export …) */
             bbq_vec_push(AS_EXPORTS, CTX->iexports[i]);
         bbq_vec_free(CTX->iexports);
         if (bbq_vec_len(AS_EXPORTS)) {                      /* export section (id 7) */
             memset(&s, 0, sizeof s); s.id = 7; s.body.tag = 7;
             WAT_FREEZE(AS_EXPORTS, s.body.u.case_7.exports);
             s.body.u.case_7.count = (uint32_t)s.body.u.case_7.exports.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_EXPORTS);
         if (has_start) {                                 /* start section (id 8) */
             memset(&s, 0, sizeof s); s.id = 8; s.body.tag = 8;
             s.body.u.case_8.func = start_func;
             bbq_vec_push(AS_SECS, s);
         }
         if (bbq_vec_len(AS_ELEMS)) {                        /* element section (id 9) */
             memset(&s, 0, sizeof s); s.id = 9; s.body.tag = 9;
             WAT_FREEZE(AS_ELEMS, s.body.u.case_9.elems);
             s.body.u.case_9.count = (uint32_t)s.body.u.case_9.elems.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_ELEMS);
         if (bbq_vec_len(AS_ENTRIES)) {                      /* code section (id 10) */
             memset(&s, 0, sizeof s); s.id = 10; s.body.tag = 10;
             WAT_FREEZE(AS_ENTRIES, s.body.u.case_10.entries);
             s.body.u.case_10.count = (uint32_t)s.body.u.case_10.entries.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_ENTRIES);
         if (bbq_vec_len(AS_DATAS)) {                        /* data section (id 11) */
             memset(&s, 0, sizeof s); s.id = 11; s.body.tag = 11;
             WAT_FREEZE(AS_DATAS, s.body.u.case_11.datas);
             s.body.u.case_11.count = (uint32_t)s.body.u.case_11.datas.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_DATAS);
         if (bbq_vec_len(AS_TAGS)) {                         /* tag section (id 13) */
             memset(&s, 0, sizeof s); s.id = 13; s.body.tag = 13;
             WAT_FREEZE(AS_TAGS, s.body.u.case_13.tags);
             s.body.u.case_13.count = (uint32_t)s.body.u.case_13.tags.count;
             bbq_vec_push(AS_SECS, s);
         } else bbq_vec_free(AS_TAGS);
         WAT_FREEZE(AS_SECS, m->sections);
         peg_skip(p);
         if (!peg_at_end(p)) { report_parse_error(p); return false; }
    return true;
}

static bool wat_parse_func_field(peg_state* p, uint32_t** func_tidx, jav_code_entry_t** entries) {
    jav_func_type_t ft; jav_name_t nm;
       jav_name_t imod = {0}, ifld = {0}; int has_import = 0;
       peg_span idsp = {0,0}, tref = {0,0}; int has_ref = 0;
       bbq_write_ctx_t* w = NULL;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "func", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    w = wat_wbuf_open(CTX);                          /* func-body bytes: pooled, grows on demand */
         bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);   /* reclaim abandoned scratch */
         bbq_vec_free(SC_F_LOCALS); wat_enames_clear(CTX);
         wat_func_reset(CTX); CTX->bind_locals = 1;
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "export", 6)) break;
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            peg_restore(p, _m5);
                            if (_ok5) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_name(p, &nm)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        if (CTX->pass == 2) bbq_vec_push(SC_ENAMES, nm); else free((void*)nm.bytes.data);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "import", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_name(p, &imod)) break;
                peg_skip(p);
                if (!wat_parse_name(p, &ifld)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                if (CTX->pass == 2 && CTX->defs_seen) {              /* §6.6: imports precede definitions */
             free((void*)imod.bytes.data); free((void*)ifld.bytes.data); wat_wbuf_close(CTX, w); return false; }
             has_import = 1;
            _choice_done3:;
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    if (CTX->pass == 1) { if (!(has_import ? wat_imp_add(CTX, SP_FUNC, idsp)
                                                         : wat_id_add(CTX, SP_FUNC, idsp))) return false; }
    {
        peg_mark _m7 = peg_save(p);
        bool _ok7 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "type", 4)) break;
            {
                peg_mark _m8 = peg_save(p);
                bool _ok8 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok8 = true;
                } while(0);
                peg_restore(p, _m8);
                if (_ok8) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_ref = 1;
            _ok7 = true;
        } while(0);
        if (!_ok7) peg_restore(p, _m7);
    }
    for (;;) {
        peg_mark _m9 = peg_save(p);
        bool _ok9 = false;
        do {
            peg_skip(p);
            if (!wat_parse_param(p, &SC_F_PARAMS)) break;
            _ok9 = true;
        } while(0);
        if (!_ok9) { peg_restore(p, _m9); break; }
    }
    for (;;) {
        peg_mark _m10 = peg_save(p);
        bool _ok10 = false;
        do {
            peg_skip(p);
            if (!wat_parse_result(p, &SC_F_RESULTS)) break;
            _ok10 = true;
        } while(0);
        if (!_ok10) { peg_restore(p, _m10); break; }
    }
    /* §6.5.1: params given only via `(type $x)` still occupy the leading local-index slots, so a
            named local declared after must be offset by the param count — reserve those slots here. */
         if (has_ref && bbq_vec_len(SC_F_PARAMS) == 0) {
             int64_t tr = wat_resolve(CTX, SP_TYPE, tref);
             if (tr >= 0) { uint32_t np = wat_type_nparams(CTX, tr); peg_span anon = {0, 0};
                 for (uint32_t i = 0; i < np; i++) if (!wat_name_push(&CTX->locals, anon)) return false; }
         }
    for (;;) {
        peg_mark _m11 = peg_save(p);
        bool _ok11 = false;
        do {
            peg_skip(p);
            if (!wat_parse_local(p, &SC_F_LOCALS)) break;
            _ok11 = true;
        } while(0);
        if (!_ok11) { peg_restore(p, _m11); break; }
    }
    jav_val_type_t* lvts = SC_F_LOCALS;             /* locals vec = run-length groups */
         int nlv = (int)bbq_vec_len(lvts); uint32_t runs = 0;
         for (int k = 0; k < nlv; ) {
             int j = k + 1; while (j < nlv && wat_valtype_eq(&lvts[k], &lvts[j])) j++;
             runs++; k = j;
         }
         bbq_write_uleb128_u32(w, runs);
         for (int k = 0; k < nlv; ) {
             int j = k + 1; while (j < nlv && wat_valtype_eq(&lvts[k], &lvts[j])) j++;
             bbq_write_uleb128_u32(w, (uint32_t)(j - k));
             bbq_write_u8(w, lvts[k].head);
             if (lvts[k].ht.has_value) bbq_write_sleb128_i64(w, lvts[k].ht.value.x);
             k = j;
         }
         bbq_vec_free(SC_F_LOCALS);
    for (;;) {
        peg_mark _m12 = peg_save(p);
        bool _ok12 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok12 = true;
        } while(0);
        if (!_ok12) { peg_restore(p, _m12); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    bbq_write_u8(w, 0x0B);                           /* expr terminator */
         if (CTX->pass != 2) { bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS); wat_wbuf_close(CTX, w);
             if (has_import) { free((void*)imod.bytes.data); free((void*)ifld.bytes.data); } return true; }
         uint32_t tidx;
         if (has_ref) {                                   /* (type $x): reuse that type, no fresh one */
             int64_t r = wat_resolve(CTX, SP_TYPE, tref);
             if (r < 0 || !wat_typeuse_ref(CTX, r, &SC_F_PARAMS, &SC_F_RESULTS)) {
                 bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);
                 wat_wbuf_close(CTX, w); return false;
             }
             tidx = (uint32_t)r;
         } else {                                         /* inline sig: §6.4.15 typeuse */
             memset(&ft, 0, sizeof ft);
             WAT_FREEZE(SC_F_PARAMS, ft.params);   ft.param_count  = (uint32_t)ft.params.count;
             WAT_FREEZE(SC_F_RESULTS, ft.results); ft.result_count = (uint32_t)ft.results.count;
             tidx = wat_typeuse(CTX, &ft);                 /* find-or-insert; takes ownership of arrays */
         }
         uint32_t myidx;
         if (has_import) {                                 /* §6.6 the func is an import, not a def */
             jav_extern_type_t d; memset(&d, 0, sizeof d);
             d.kind = 0; d.body.tag = 0; d.body.u.case_0.x = tidx;
             myidx = wat_inline_import(CTX, imod, ifld, d, SP_FUNC);  /* imports carry no body */
         } else {
             myidx = (uint32_t)(bbq_vec_len(CTX->sp_imp[SP_FUNC]) + bbq_vec_len(*func_tidx));
             bbq_vec_push(*func_tidx, tidx);
             CTX->defs_seen = 1;
             jav_code_entry_t e; memset(&e, 0, sizeof e);
             bbq_ctx_t rc; bbq_ctx_init(&rc, w->data, w->pos);   /* decode the scratch -> tree */
             if (!jav_func_body_read(&rc, &e.body)) { bbq_ctx_free(&rc); wat_wbuf_close(CTX, w); return false; }
             bbq_ctx_free(&rc);
             /* the FuncBody tree owns copies of any bytes */
             bbq_vec_push(*entries, e);
         }
         wat_wbuf_close(CTX, w);
         wat_flush_exports(CTX, SC_ENAMES, 0, myidx);      /* §6.6 inline exports, now that the index is known */
         SC_ENAMES = NULL;
    return true;
}

static bool wat_parse_instr(peg_state* p, bbq_write_ctx_t* w) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!wat_parse_folded_instr(p, w)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_linear_instr(p, w)) return false;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_linear_instr(peg_state* p, bbq_write_ctx_t* w) {
    peg_span s; uint8_t prefix = 0; uint32_t op = 0;
       int shape = -1, align = -1, sp0 = -1, sp1 = -1;
    peg_skip(p);
    if (!wat_atom(p, &s)) return false;
    if (!wat_find_instr(s.ptr, (int)s.len, &prefix, &op, &shape, &align, &sp0, &sp1))
             return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    peg_skip(p);
                    if (!peg_match_n(p, "(", 1)) break;
                    peg_skip(p);
                    if (!peg_match_n(p, "result", 6)) break;
                    _ok1 = true;
                } while(0);
                peg_restore(p, _m1);
                if (!_ok1) break;
            }
            if (op == 0x1b) { op = 0x1c; shape = WSH_SELECTT; }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    if (shape != WSH_HEAP) {                          /* heap emits its own op (ref.test/cast: null-dependent) */
           if (prefix) { bbq_write_u8(w, prefix); bbq_write_uleb128_u32(w, op); }
           else        { bbq_write_u8(w, (uint8_t)op); }
         }
    {
        peg_mark _m2 = peg_save(p);
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!wat_parse_call_indirect_op(p, w, shape, sp0, sp1)) break;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!wat_parse_idx_op(p, w, shape, sp0, sp1)) break;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!wat_parse_br_table_op(p, w, shape)) break;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!wat_parse_select_t_op(p, w, shape)) break;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!wat_parse_float_op(p, w, shape)) break;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!wat_parse_mem_arg_op(p, w, shape, align)) break;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!wat_parse_ref_null_op(p, w, shape, op, prefix)) break;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok10 = false;
            do {
                peg_skip(p);
                if (!wat_parse_ref_cast_op(p, w, shape, op, prefix)) break;
                _ok10 = true;
            } while(0);
            if (!_ok10) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok11 = false;
            do {
                peg_skip(p);
                if (!wat_parse_v128_const_op(p, w, shape, op)) break;
                _ok11 = true;
            } while(0);
            if (!_ok11) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok12 = false;
            do {
                peg_skip(p);
                if (!wat_parse_v128_shuffle_op(p, w, shape, op)) break;
                _ok12 = true;
            } while(0);
            if (!_ok12) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok13 = false;
            do {
                peg_skip(p);
                if (!wat_parse_br_on_cast_op(p, w, shape)) break;
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok14 = false;
            do {
                peg_skip(p);
                if (!wat_parse_block_op(p, w, shape)) break;
                _ok14 = true;
            } while(0);
            if (!_ok14) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok15 = false;
            do {
                peg_skip(p);
                if (!wat_parse_if_op(p, w, shape)) break;
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        {
            bool _ok16 = false;
            do {
                peg_skip(p);
                if (!wat_parse_try_table_op(p, w, shape)) break;
                _ok16 = true;
            } while(0);
            if (!_ok16) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        peg_skip(p);
        if (!wat_parse_num_op(p, w, shape)) return false;
    _choice_done2:;
    }
    return true;
}

static bool wat_parse_folded_plain(peg_state* p, bbq_write_ctx_t* w) {
    bbq_write_ctx_t* tw = wat_wbuf_open(CTX);
    peg_skip(p);
    if (!wat_parse_linear_instr(p, tw)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    for (size_t i = 0; i < tw->pos; i++) bbq_write_u8(w, tw->data[i]);    /* append deferred head */
         wat_wbuf_close(CTX, tw);
    return true;
}

static bool wat_parse_folded_instr(peg_state* p, bbq_write_ctx_t* w) {
    peg_span lab = {0,0}; wat_catch_t* cats = NULL;
       bbq_write_ctx_t* tw = NULL;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "block", 5)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &lab)) break;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) peg_restore(p, _m3);
                }
                bbq_write_u8(w, 0x02); if (!wat_name_push(&CTX->labels, lab)) return false;
                peg_skip(p);
                if (!wat_parse_block_type(p, w)) break;
                for (;;) {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) { peg_restore(p, _m4); break; }
                }
                bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "loop", 4)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &lab)) break;
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) peg_restore(p, _m7);
                }
                bbq_write_u8(w, 0x03); if (!wat_name_push(&CTX->labels, lab)) return false;
                peg_skip(p);
                if (!wat_parse_block_type(p, w)) break;
                for (;;) {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok8 = true;
                    } while(0);
                    if (!_ok8) { peg_restore(p, _m8); break; }
                }
                bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "if", 2)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                {
                    peg_mark _m11 = peg_save(p);
                    bool _ok11 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &lab)) break;
                        _ok11 = true;
                    } while(0);
                    if (!_ok11) peg_restore(p, _m11);
                }
                tw = wat_wbuf_open(CTX);
                peg_skip(p);
                if (!wat_parse_block_type(p, tw)) break;
                for (;;) {
                    peg_mark _m12 = peg_save(p);
                    bool _ok12 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_folded_instr(p, w)) break;
                        _ok12 = true;
                    } while(0);
                    if (!_ok12) { peg_restore(p, _m12); break; }
                }
                if (!wat_name_push(&CTX->labels, lab)) return false;            /* if-label scopes then/else only */
             bbq_write_u8(w, 0x04);
             for (size_t i = 0; i < tw->pos; i++) bbq_write_u8(w, tw->data[i]);
             wat_wbuf_close(CTX, tw);
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "then", 4)) break;
                {
                    peg_mark _m13 = peg_save(p);
                    bool _ok13 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok13 = true;
                    } while(0);
                    peg_restore(p, _m13);
                    if (_ok13) break;
                }
                for (;;) {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) { peg_restore(p, _m14); break; }
                }
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                {
                    peg_mark _m15 = peg_save(p);
                    bool _ok15 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "else", 4)) break;
                        {
                            peg_mark _m16 = peg_save(p);
                            bool _ok16 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok16 = true;
                            } while(0);
                            peg_restore(p, _m16);
                            if (_ok16) break;
                        }
                        bbq_write_u8(w, 0x05);
                        for (;;) {
                            peg_mark _m17 = peg_save(p);
                            bool _ok17 = false;
                            do {
                                peg_skip(p);
                                if (!wat_parse_instr(p, w)) break;
                                _ok17 = true;
                            } while(0);
                            if (!_ok17) { peg_restore(p, _m17); break; }
                        }
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        _ok15 = true;
                    } while(0);
                    if (!_ok15) peg_restore(p, _m15);
                }
                bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok18 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "try_table", 9)) break;
                {
                    peg_mark _m19 = peg_save(p);
                    bool _ok19 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok19 = true;
                    } while(0);
                    peg_restore(p, _m19);
                    if (_ok19) break;
                }
                {
                    peg_mark _m20 = peg_save(p);
                    bool _ok20 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &lab)) break;
                        _ok20 = true;
                    } while(0);
                    if (!_ok20) peg_restore(p, _m20);
                }
                bbq_write_u8(w, 0x1F);
                peg_skip(p);
                if (!wat_parse_block_type(p, w)) break;
                for (;;) {
                    peg_mark _m21 = peg_save(p);
                    bool _ok21 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_catch(p, &cats)) break;
                        _ok21 = true;
                    } while(0);
                    if (!_ok21) { peg_restore(p, _m21); break; }
                }
                int nc = (int)bbq_vec_len(cats);
             bbq_write_uleb128_u32(w, (uint32_t)nc);
             for (int i = 0; i < nc; i++) {
                 bbq_write_u8(w, cats[i].kind);
                 if (cats[i].kind == 0 || cats[i].kind == 1) bbq_write_uleb128_u32(w, cats[i].tag);
                 bbq_write_uleb128_u32(w, cats[i].label);
             }
             bbq_vec_free(cats); if (!wat_name_push(&CTX->labels, lab)) return false;
                for (;;) {
                    peg_mark _m22 = peg_save(p);
                    bool _ok22 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok22 = true;
                    } while(0);
                    if (!_ok22) { peg_restore(p, _m22); break; }
                }
                bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
                _ok18 = true;
            } while(0);
            if (!_ok18) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_folded_plain(p, w)) return false;
    _choice_done0:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_idx_ref(peg_state* p, peg_span* out) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!wat_nat(p, out)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_id(p, out)) return false;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_call_indirect_op(peg_state* p, bbq_write_ctx_t* w, int shape, int sp0, int sp1) {
    peg_span tref = {0,0}, tyref = {0,0}; int has_tab = 0, has_ref = 0;
       jav_func_type_t ft;
       int sv = CTX->bind_locals; CTX->bind_locals = 0;
    if (!(shape == WSH_IDX2 && sp0 == SP_TYPE && sp1 == SP_TABLE)) { CTX->bind_locals = sv; return false; }
         bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS);
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tref)) break;
            has_tab = 1;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "type", 4)) break;
            {
                peg_mark _m2 = peg_save(p);
                bool _ok2 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok2 = true;
                } while(0);
                peg_restore(p, _m2);
                if (_ok2) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tyref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_ref = 1;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m3 = peg_save(p);
        bool _ok3 = false;
        do {
            peg_skip(p);
            if (!wat_parse_b_param(p, &SC_I_PARAMS)) break;
            _ok3 = true;
        } while(0);
        if (!_ok3) { peg_restore(p, _m3); break; }
    }
    for (;;) {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            peg_skip(p);
            if (!wat_parse_result(p, &SC_I_RESULTS)) break;
            _ok4 = true;
        } while(0);
        if (!_ok4) { peg_restore(p, _m4); break; }
    }
    CTX->bind_locals = sv;
         uint32_t tidx;
         if (has_ref) { int64_t r = wat_resolve(CTX, SP_TYPE, tyref);
                        if (r < 0 || !wat_typeuse_ref(CTX, r, &SC_I_PARAMS, &SC_I_RESULTS)) {
                            bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS); return false;
                        }
                        tidx = (uint32_t)r; }
         else { memset(&ft, 0, sizeof ft);
                WAT_FREEZE(SC_I_PARAMS, ft.params);  ft.param_count  = (uint32_t)ft.params.count;
                WAT_FREEZE(SC_I_RESULTS, ft.results); ft.result_count = (uint32_t)ft.results.count;
                tidx = wat_typeuse(CTX, &ft); }
         uint32_t tab = 0;
         if (has_tab) { int64_t t = wat_resolve(CTX, SP_TABLE, tref); if (t < 0) return false; tab = (uint32_t)t; }
         bbq_write_uleb128_u32(w, tidx);                       /* binary: typeidx then tableidx */
         bbq_write_uleb128_u32(w, tab);
    return true;
}

static bool wat_parse_idx_op(peg_state* p, bbq_write_ctx_t* w, int shape, int sp0, int sp1) {
    peg_span r0 = {0,0}, r1 = {0,0}; int nr = 0;
    if (shape != WSH_IDX && shape != WSH_IDX2) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &r0)) break;
            nr = 1;
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    peg_skip(p);
                    if (!wat_parse_idx_ref(p, &r1)) break;
                    nr = 2;
                    _ok1 = true;
                } while(0);
                if (!_ok1) peg_restore(p, _m1);
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    /* §6.5.5/§6.5.6: memory & table indices are OPTIONAL in text, defaulting to 0
            (`memory.copy` ≡ `memory.copy 0 0`, `memory.fill` ≡ `memory.fill 0`, …); and the
            `init` ops REVERSE text vs binary order (text `memory.init <memidx> <dataidx>` →
            binary `[dataidx, memidx]`). Classify by the operand index-space (sp0/sp1, from
            the toml `operands`) — a memidx/tableidx operand is the omittable one. */
         int opt0 = (sp0 == SP_MEM || sp0 == SP_TABLE);
         int opt1 = (sp1 == SP_MEM || sp1 == SP_TABLE);
         if (shape == WSH_IDX) {                            /* one index (req, or omittable if mem/table) */
             uint32_t v = 0;
             if (nr >= 1) { int64_t x = wat_resolve(CTX, sp0, r0); if (x < 0) return false; v = (uint32_t)x; }
             else if (!opt0) return false;                  /* a non-mem/table idx is required */
             if (nr > 1) return false;
             bbq_write_uleb128_u32(w, v);
         } else if (opt0 && sp1 == sp0) {                   /* copy: `op` | `op dst src`, same order, default 0 */
             if (nr == 1) return false;
             uint32_t a = 0, b = 0;
             if (nr == 2) {
                 int64_t x0 = wat_resolve(CTX, sp0, r0); if (x0 < 0) return false; a = (uint32_t)x0;
                 int64_t x1 = wat_resolve(CTX, sp1, r1); if (x1 < 0) return false; b = (uint32_t)x1;
             }
             bbq_write_uleb128_u32(w, a); bbq_write_uleb128_u32(w, b);
         } else if (opt1 && (sp0 == SP_DATA || sp0 == SP_ELEM)) {  /* init: text [mem/tab, data/elem] → binary [data/elem, mem/tab(0)] */
             if (nr == 0) return false;
             uint32_t req = 0, mt = 0;
             if (nr == 2) {                                  /* both given: text r0=mem/tab (sp1), r1=data/elem (sp0) */
                 int64_t xm = wat_resolve(CTX, sp1, r0); if (xm < 0) return false; mt  = (uint32_t)xm;
                 int64_t xr = wat_resolve(CTX, sp0, r1); if (xr < 0) return false; req = (uint32_t)xr;
             } else {                                        /* one given: the required data/elem idx; mem/tab = 0 */
                 int64_t xr = wat_resolve(CTX, sp0, r0); if (xr < 0) return false; req = (uint32_t)xr;
             }
             bbq_write_uleb128_u32(w, req); bbq_write_uleb128_u32(w, mt);
         } else {                                            /* normal idx2 (struct.get, array ops, call_indirect): both required, same order */
             if (nr != 2) return false;
             int64_t x0 = wat_resolve(CTX, sp0, r0); if (x0 < 0) return false; bbq_write_uleb128_u32(w, (uint32_t)x0);
             int64_t x1 = (sp1 < 0) ? wat_resolve_field(CTX, r0, r1)   /* §6.6.2 fieldidx: per-type field $id space */
                                    : wat_resolve(CTX, sp1, r1);
             if (x1 < 0) return false; bbq_write_uleb128_u32(w, (uint32_t)x1);
         }
    return true;
}

static bool wat_parse_num_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span ns; int nv = 0;
    if (!(shape == WSH_NONE || shape == WSH_I32 ||
               shape == WSH_I64 || shape == WSH_LANE)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_snum(p, &ns)) break;
            nv++;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    int ok; uint64_t v;
         switch (shape) {
         case WSH_NONE: if (nv != 0) return false; break;
         case WSH_I32:  if (nv != 1) return false;                /* iN.const: [-2^31, 2^32) */
                        v = wat_int_lit(ns, 32, 1, &ok); if (!ok) return false;
                        bbq_write_sleb128_i32(w, (int32_t)v); break;
         case WSH_I64:  if (nv != 1) return false;
                        v = wat_int_lit(ns, 64, 1, &ok); if (!ok) return false;
                        bbq_write_sleb128_i64(w, (int64_t)v); break;
         case WSH_LANE: if (nv != 1) return false;                /* laneidx: u8, unsigned */
                        v = wat_int_lit(ns, 8, 0, &ok); if (!ok) return false;
                        bbq_write_u8(w, (uint8_t)v); break;
         }
    return true;
}

static bool wat_parse_mem_arg_op(peg_state* p, bbq_write_ctx_t* w, int shape, int align) {
    peg_span r; peg_span pos[2]; int npos = 0;
       int has_off = 0, has_al = 0; uint64_t off = 0; uint64_t al = 0;
    if (!(shape == WSH_MEMARG || shape == WSH_MEMLANE)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                {
                    bool _ok2 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "offset=", 7)) break;
                        peg_skip(p);
                        if (!wat_nat(p, &r)) break;
                        int ok; off = wat_int_lit(r, 64, 0, &ok);   /* §6.3.1: u64, no sign */
                             if (!ok) return false; has_off = 1;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                {
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "align=", 6)) break;
                        peg_skip(p);
                        if (!wat_nat(p, &r)) break;
                        int ok; al = wat_int_lit(r, 64, 0, &ok);    /* §6.3.1: u64, no sign */
                             if (!ok) return false; has_al = 1;
                             if (al == 0 || (al & (al - 1)) != 0) return false;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &r)) break;
                if (npos < 2) pos[npos] = r; npos++;
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    uint32_t memidx = 0; int64_t lane = 0;
         if (shape == WSH_MEMARG) {                              /* [memidx] offset?/align? */
             if (npos > 1) return false;
             if (npos == 1) { int64_t m = wat_resolve(CTX, SP_MEM, pos[0]); if (m < 0) return false; memidx = (uint32_t)m; }
         } else {                                                /* memlane: [memidx] … laneidx (last) */
             if (npos < 1 || npos > 2) return false;
             int lok; lane = (int64_t)wat_int_lit(pos[npos - 1], 8, 0, &lok);   /* laneidx: u8 */
             if (!lok) return false;
             if (npos == 2) { int64_t m = wat_resolve(CTX, SP_MEM, pos[0]); if (m < 0) return false; memidx = (uint32_t)m; }
         }
         uint32_t a = has_al ? wat_ilog2(al) : (uint32_t)align;
         if (memidx != 0) a |= 0x40;                             /* §5.4.9 bit 6: a memidx follows */
         bbq_write_uleb128_u32(w, a);
         if (memidx != 0) bbq_write_uleb128_u32(w, memidx);
         bbq_write_uleb128_u64(w, has_off ? off : 0);
         if (shape == WSH_MEMLANE) bbq_write_u8(w, (uint8_t)lane);
    return true;
}

static bool wat_parse_br_table_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span r; uint32_t* L = NULL;
    if (shape != WSH_BRTABLE) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &r)) break;
            int64_t x = wat_resolve(CTX, SP_LABEL, r);
                      if (x < 0) { bbq_vec_free(L); return false; }   /* unbound $label */
                      uint32_t xx = (uint32_t)x; bbq_vec_push(L, xx);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    int nL = (int)bbq_vec_len(L);
         if (nL < 1) { bbq_vec_free(L); return false; }    /* at least the default */
         bbq_write_uleb128_u32(w, (uint32_t)(nL - 1));     /* table length */
         for (int i = 0; i < nL - 1; i++) bbq_write_uleb128_u32(w, L[i]);
         bbq_write_uleb128_u32(w, L[nL - 1]);              /* default */
         bbq_vec_free(L);
    return true;
}

static bool wat_parse_select_t_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    jav_val_type_t t;
    if (shape != WSH_SELECTT) return false;
         bbq_vec_free(SC_SELT);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "result", 6)) break;
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok1 = true;
                } while(0);
                peg_restore(p, _m1);
                if (_ok1) break;
            }
            for (;;) {
                peg_mark _m2 = peg_save(p);
                bool _ok2 = false;
                do {
                    peg_skip(p);
                    if (!wat_parse_val_type(p, &t)) break;
                    bbq_vec_push(SC_SELT, t);
                    _ok2 = true;
                } while(0);
                if (!_ok2) { peg_restore(p, _m2); break; }
            }
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    jav_val_type_t* ts = SC_SELT;
         int nt = (int)bbq_vec_len(ts);
         bbq_write_uleb128_u32(w, (uint32_t)nt);
         for (int i = 0; i < nt; i++) {
             bbq_write_u8(w, ts[i].head);
             if (ts[i].ht.has_value) bbq_write_sleb128_i64(w, ts[i].ht.value.x);
         }
         bbq_vec_free(SC_SELT);
    return true;
}

static bool wat_parse_float_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span ns; int neg = 0, isinf = 0, isnan = 0, ispay = 0; uint64_t pay = 0; int fw;
       uint64_t b = 0; int isnum = 0;
    if (shape != WSH_F32 && shape != WSH_F64) return false; fw = (shape == WSH_F32) ? 4 : 8;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!wat_fnum(p, &ns)) break;
                isnum = 1;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            peg_mark _m2 = peg_save(p);
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (peg_peek_at_n(p, "+", 1)) {
                    peg_skip(p);
                    if (!peg_match_n(p, "+", 1)) break;
                } else {
                    peg_skip(p);
                    if (!peg_match_n(p, "-", 1)) break;
                    neg = 1;
                }
                _ok2 = true;
            } while(0);
            if (!_ok2) peg_restore(p, _m2);
        }
        {
            peg_mark _m3 = peg_save(p);
            {
                bool _ok4 = false;
                do {
                    peg_skip(p);
                    if (!peg_match_n(p, "inf", 3)) break;
                    {
                        peg_mark _m5 = peg_save(p);
                        bool _ok5 = false;
                        do {
                            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                            peg_advance(p);
                            _ok5 = true;
                        } while(0);
                        peg_restore(p, _m5);
                        if (_ok5) break;
                    }
                    isinf = 1;
                    _ok4 = true;
                } while(0);
                if (!_ok4) {
                    peg_restore(p, _m3);
                } else goto _choice_done3;
            }
            peg_skip(p);
            if (!peg_match_n(p, "nan", 3)) return false;
            {
                peg_mark _m6 = peg_save(p);
                {
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, ":", 1)) break;
                        peg_skip(p);
                        if (!wat_hexnat(p, &ns)) break;
                        isnan = 1; ispay = 1;
                                  if (!wat_nan_payload(ns, fw, &pay)) return false;
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) {
                        peg_restore(p, _m6);
                    } else goto _choice_done6;
                }
                {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok8 = true;
                    } while(0);
                    peg_restore(p, _m8);
                    if (_ok8) return false;
                }
                isnan = 1;
            _choice_done6:;
            }
        _choice_done3:;
        }
    _choice_done0:;
    }
    if (isnum) { int ok; b = wat_float_lit(ns, fw, &ok); if (!ok) return false; }
         else b = wat_float_bits(fw, neg, isinf, isnan, ispay, pay, 0);
         if (fw == 4) bbq_write_u32le(w, (uint32_t)b); else bbq_write_u64le(w, b);
    return true;
}

static bool wat_parse_type_field(peg_state* p, jav_rec_type_t** types) {
    jav_rec_type_t rt; memset(&rt, 0, sizeof rt); peg_span idsp = {0,0};
       jav_comp_type_t ct; memset(&ct, 0, sizeof ct);
       jav_sub_type_t st; memset(&st, 0, sizeof st); uint8_t subhead = 0; int issub = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "type", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    if (CTX->pass == 1 && !wat_id_add(CTX, SP_TYPE, idsp)) return false;
    {
        peg_mark _m2 = peg_save(p);
        {
            bool _ok3 = false;
            do {
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "sub", 3)) break;
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (!_ok4) break;
                }
                peg_skip(p);
                if (!wat_parse_sub_type_p(p, &st, &subhead)) break;
                issub = 1;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        peg_skip(p);
        if (!wat_parse_comp_type(p, &ct)) return false;
    _choice_done2:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass == 1) { wat_xtypes_capture(CTX, issub, &ct, &st); return true; }
         if (issub) {                                     /* singleton sub type */
             rt.head = subhead; rt.body.tag = subhead;
             if (subhead == 0x4F) rt.body.u.case_1 = st; else rt.body.u.case_2 = st;
         } else {                                         /* singleton comptype (shorthand) */
             rt.head = ct.head; rt.body.tag = ct.head;
             if      (ct.head == 0x5E) rt.body.u.case_3 = ct.body.u.case_0;   /* array */
             else if (ct.head == 0x5F) rt.body.u.case_4 = ct.body.u.case_1;   /* struct */
             else                      rt.body.u.case_5 = ct.body.u.case_2;   /* func */
         }
         bbq_vec_push(*types, rt);
    return true;
}

static bool wat_parse_rec_member_p(peg_state* p, jav_rec_member_t** members) {
    jav_rec_member_t rm; memset(&rm, 0, sizeof rm); peg_span idsp = {0,0};
       jav_comp_type_t ct; memset(&ct, 0, sizeof ct);
       jav_sub_type_t st; memset(&st, 0, sizeof st); uint8_t subhead = 0; int issub = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "type", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    if (CTX->pass == 1 && !wat_id_add(CTX, SP_TYPE, idsp)) return false;
    {
        peg_mark _m2 = peg_save(p);
        {
            bool _ok3 = false;
            do {
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "sub", 3)) break;
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (!_ok4) break;
                }
                peg_skip(p);
                if (!wat_parse_sub_type_p(p, &st, &subhead)) break;
                issub = 1;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        peg_skip(p);
        if (!wat_parse_comp_type(p, &ct)) return false;
    _choice_done2:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass == 1) { wat_xtypes_capture(CTX, issub, &ct, &st); return true; }
         if (issub) { rm.head = subhead; rm.body.tag = subhead;
                      if (subhead == 0x4F) rm.body.u.case_0 = st; else rm.body.u.case_1 = st; }
         else { rm.head = ct.head; rm.body.tag = ct.head;
                if      (ct.head == 0x5E) rm.body.u.case_2 = ct.body.u.case_0;   /* array */
                else if (ct.head == 0x5F) rm.body.u.case_3 = ct.body.u.case_1;   /* struct */
                else                      rm.body.u.case_4 = ct.body.u.case_2; } /* func */
         bbq_vec_push(*members, rm);
    return true;
}

static bool wat_parse_rec_field(peg_state* p, jav_rec_type_t** types) {
    jav_rec_type_t rt; memset(&rt, 0, sizeof rt); uint32_t xbase = 0;
       bbq_vec_free(SC_MEMBERS);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "rec", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    xbase = (uint32_t)bbq_vec_len(CTX->xtypes);
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_rec_member_p(p, &SC_MEMBERS)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { wat_xtypes_seal_recgroup(CTX, xbase); bbq_vec_free(SC_MEMBERS); return true; }
         rt.head = 0x4E; rt.body.tag = 0x4E;
         WAT_FREEZE(SC_MEMBERS, rt.body.u.case_0.members);
         rt.body.u.case_0.count = (uint32_t)rt.body.u.case_0.members.count;
         bbq_vec_push(*types, rt);
    return true;
}

static bool wat_parse_storage_type_p(peg_state* p, jav_storage_type_t* out) {
    jav_val_type_t v;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i8", 2)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                memset(out, 0, sizeof *out); out->head = 0x78;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i16", 3)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                memset(out, 0, sizeof *out); out->head = 0x77;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_val_type(p, &v)) return false;
        memset(out, 0, sizeof *out); out->head = v.head;
                       out->ht.has_value = v.ht.has_value; out->ht.value = v.ht.value;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_field_type_p(peg_state* p, jav_field_type_t* out) {
    jav_storage_type_t s; memset(&s, 0, sizeof s);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "mut", 3)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                peg_skip(p);
                if (!wat_parse_storage_type_p(p, &s)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                out->storage = s; out->mut = 1;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_storage_type_p(p, &s)) return false;
        out->storage = s; out->mut = 0;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_field_decl(peg_state* p, jav_field_type_t** fields) {
    jav_field_type_t ft; peg_span idsp = {0,0}, none = {0,0}; int fi = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "field", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!wat_parse_field_type_p(p, &ft)) break;
            bbq_vec_push(*fields, ft);
                           if (CTX->pass == 1) {
                               peg_span fsp = fi == 0 ? idsp : none;
                               if (fsp.len && wat_name_find(CTX->cur_fields, fsp) >= 0) return false;  /* duplicate field */
                               if (!wat_name_push(&CTX->cur_fields, fsp)) return false;
                           }
                           fi++;
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_comp_type(peg_state* p, jav_comp_type_t* out) {
    jav_field_type_t aft; memset(&aft, 0, sizeof aft);
       bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);   /* reclaim abandoned scratch */
       bbq_vec_free(SC_FIELDS);
       CTX->bind_locals = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "func", 4)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_param(p, &SC_F_PARAMS)) break;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                for (;;) {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_result(p, &SC_F_RESULTS)) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) { peg_restore(p, _m4); break; }
                }
                memset(out, 0, sizeof *out); out->head = 0x60; out->body.tag = 0x60;
             WAT_FREEZE(SC_F_PARAMS, out->body.u.case_2.params);   out->body.u.case_2.param_count  = (uint32_t)out->body.u.case_2.params.count;
             WAT_FREEZE(SC_F_RESULTS, out->body.u.case_2.results); out->body.u.case_2.result_count = (uint32_t)out->body.u.case_2.results.count;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "struct", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                for (;;) {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_field_decl(p, &SC_FIELDS)) break;
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) { peg_restore(p, _m7); break; }
                }
                memset(out, 0, sizeof *out); out->head = 0x5F; out->body.tag = 0x5F;
             WAT_FREEZE(SC_FIELDS, out->body.u.case_1.fields); out->body.u.case_1.field_count = (uint32_t)out->body.u.case_1.fields.count;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "array", 5)) return false;
        {
            peg_mark _m8 = peg_save(p);
            bool _ok8 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok8 = true;
            } while(0);
            peg_restore(p, _m8);
            if (_ok8) return false;
        }
        peg_skip(p);
        if (!wat_parse_field_type_p(p, &aft)) return false;
        memset(out, 0, sizeof *out); out->head = 0x5E; out->body.tag = 0x5E; out->body.u.case_0.field = aft;
    _choice_done0:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_sub_type_p(peg_state* p, jav_sub_type_t* out, uint8_t* head) {
    peg_span sr; jav_comp_type_t ct;
       bbq_vec_free(SC_IDXS);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "sub", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    *head = 0x50;
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "final", 5)) break;
            {
                peg_mark _m2 = peg_save(p);
                bool _ok2 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok2 = true;
                } while(0);
                peg_restore(p, _m2);
                if (_ok2) break;
            }
            *head = 0x4F;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m3 = peg_save(p);
        bool _ok3 = false;
        do {
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &sr)) break;
            int64_t s = wat_resolve(CTX, SP_TYPE, sr);
                       if (s < 0) { bbq_vec_free(SC_IDXS); return false; }
                       uint32_t ss = (uint32_t)s; bbq_vec_push(SC_IDXS, ss);
            _ok3 = true;
        } while(0);
        if (!_ok3) { peg_restore(p, _m3); break; }
    }
    peg_skip(p);
    if (!wat_parse_comp_type(p, &ct)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    memset(out, 0, sizeof *out);
         WAT_FREEZE(SC_IDXS, out->supers); out->super_count = (uint32_t)out->supers.count;
         out->body = ct;
    return true;
}

static bool wat_parse_param(peg_state* p, jav_val_type_t** list) {
    jav_val_type_t t; peg_span idsp = {0,0};
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "param", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!wat_id(p, &idsp)) break;
                peg_skip(p);
                if (!wat_parse_val_type(p, &t)) break;
                bbq_vec_push(*list, t);
           if (CTX->bind_locals) {
               if (idsp.len && wat_name_find(CTX->locals, idsp) >= 0) return false;   /* duplicate local */
               if (!wat_name_push(&CTX->locals, idsp)) return false;
           }
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        for (;;) {
            peg_mark _m3 = peg_save(p);
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!wat_parse_val_type(p, &t)) break;
                bbq_vec_push(*list, t);
             if (CTX->bind_locals) {
               if (idsp.len && wat_name_find(CTX->locals, idsp) >= 0) return false;   /* duplicate local */
               if (!wat_name_push(&CTX->locals, idsp)) return false;
           }
                _ok3 = true;
            } while(0);
            if (!_ok3) { peg_restore(p, _m3); break; }
        }
    _choice_done1:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_b_param(peg_state* p, jav_val_type_t** list) {
    jav_val_type_t t;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "param", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_val_type(p, &t)) break;
            bbq_vec_push(*list, t);
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_result(peg_state* p, jav_val_type_t** list) {
    jav_val_type_t t;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "result", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_val_type(p, &t)) break;
            bbq_vec_push(*list, t);
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_local(peg_state* p, jav_val_type_t** list) {
    jav_val_type_t t; peg_span idsp = {0,0};
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "local", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!wat_id(p, &idsp)) break;
                peg_skip(p);
                if (!wat_parse_val_type(p, &t)) break;
                bbq_vec_push(*list, t);
           if (CTX->bind_locals) {
               if (idsp.len && wat_name_find(CTX->locals, idsp) >= 0) return false;   /* duplicate local */
               if (!wat_name_push(&CTX->locals, idsp)) return false;
           }
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        for (;;) {
            peg_mark _m3 = peg_save(p);
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!wat_parse_val_type(p, &t)) break;
                bbq_vec_push(*list, t);
             if (CTX->bind_locals) {
               if (idsp.len && wat_name_find(CTX->locals, idsp) >= 0) return false;   /* duplicate local */
               if (!wat_name_push(&CTX->locals, idsp)) return false;
           }
                _ok3 = true;
            } while(0);
            if (!_ok3) { peg_restore(p, _m3); break; }
        }
    _choice_done1:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    return true;
}

static bool wat_parse_val_type(peg_state* p, jav_val_type_t* out) {
    int64_t htv = 0; int nullable = 0;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i32", 3)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                *out = wat_numtype(0x7f);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i64", 3)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                *out = wat_numtype(0x7e);
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "f32", 3)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                *out = wat_numtype(0x7d);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "f64", 3)) break;
                {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok8 = true;
                    } while(0);
                    peg_restore(p, _m8);
                    if (_ok8) break;
                }
                *out = wat_numtype(0x7c);
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "v128", 4)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                *out = wat_numtype(0x7b);
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok11 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "funcref", 7)) break;
                {
                    peg_mark _m12 = peg_save(p);
                    bool _ok12 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok12 = true;
                    } while(0);
                    peg_restore(p, _m12);
                    if (_ok12) break;
                }
                *out = wat_numtype(0x70);
                _ok11 = true;
            } while(0);
            if (!_ok11) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok13 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "externref", 9)) break;
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok14 = true;
                    } while(0);
                    peg_restore(p, _m14);
                    if (_ok14) break;
                }
                *out = wat_numtype(0x6f);
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok15 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "anyref", 6)) break;
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok16 = true;
                    } while(0);
                    peg_restore(p, _m16);
                    if (_ok16) break;
                }
                *out = wat_numtype(0x6e);
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok17 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "eqref", 5)) break;
                {
                    peg_mark _m18 = peg_save(p);
                    bool _ok18 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok18 = true;
                    } while(0);
                    peg_restore(p, _m18);
                    if (_ok18) break;
                }
                *out = wat_numtype(0x6d);
                _ok17 = true;
            } while(0);
            if (!_ok17) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok19 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i31ref", 6)) break;
                {
                    peg_mark _m20 = peg_save(p);
                    bool _ok20 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok20 = true;
                    } while(0);
                    peg_restore(p, _m20);
                    if (_ok20) break;
                }
                *out = wat_numtype(0x6c);
                _ok19 = true;
            } while(0);
            if (!_ok19) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok21 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "structref", 9)) break;
                {
                    peg_mark _m22 = peg_save(p);
                    bool _ok22 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok22 = true;
                    } while(0);
                    peg_restore(p, _m22);
                    if (_ok22) break;
                }
                *out = wat_numtype(0x6b);
                _ok21 = true;
            } while(0);
            if (!_ok21) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok23 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "arrayref", 8)) break;
                {
                    peg_mark _m24 = peg_save(p);
                    bool _ok24 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok24 = true;
                    } while(0);
                    peg_restore(p, _m24);
                    if (_ok24) break;
                }
                *out = wat_numtype(0x6a);
                _ok23 = true;
            } while(0);
            if (!_ok23) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok25 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "exnref", 6)) break;
                {
                    peg_mark _m26 = peg_save(p);
                    bool _ok26 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok26 = true;
                    } while(0);
                    peg_restore(p, _m26);
                    if (_ok26) break;
                }
                *out = wat_numtype(0x69);
                _ok25 = true;
            } while(0);
            if (!_ok25) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok27 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullref", 7)) break;
                {
                    peg_mark _m28 = peg_save(p);
                    bool _ok28 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok28 = true;
                    } while(0);
                    peg_restore(p, _m28);
                    if (_ok28) break;
                }
                *out = wat_numtype(0x71);
                _ok27 = true;
            } while(0);
            if (!_ok27) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok29 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullexternref", 13)) break;
                {
                    peg_mark _m30 = peg_save(p);
                    bool _ok30 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok30 = true;
                    } while(0);
                    peg_restore(p, _m30);
                    if (_ok30) break;
                }
                *out = wat_numtype(0x72);
                _ok29 = true;
            } while(0);
            if (!_ok29) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok31 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullfuncref", 11)) break;
                {
                    peg_mark _m32 = peg_save(p);
                    bool _ok32 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok32 = true;
                    } while(0);
                    peg_restore(p, _m32);
                    if (_ok32) break;
                }
                *out = wat_numtype(0x73);
                _ok31 = true;
            } while(0);
            if (!_ok31) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok33 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullexnref", 10)) break;
                {
                    peg_mark _m34 = peg_save(p);
                    bool _ok34 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok34 = true;
                    } while(0);
                    peg_restore(p, _m34);
                    if (_ok34) break;
                }
                *out = wat_numtype(0x74);
                _ok33 = true;
            } while(0);
            if (!_ok33) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "(", 1)) return false;
        peg_skip(p);
        if (!peg_match_n(p, "ref", 3)) return false;
        {
            peg_mark _m35 = peg_save(p);
            bool _ok35 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok35 = true;
            } while(0);
            peg_restore(p, _m35);
            if (_ok35) return false;
        }
        {
            peg_mark _m36 = peg_save(p);
            bool _ok36 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "null", 4)) break;
                {
                    peg_mark _m37 = peg_save(p);
                    bool _ok37 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok37 = true;
                    } while(0);
                    peg_restore(p, _m37);
                    if (_ok37) break;
                }
                nullable = 1;
                _ok36 = true;
            } while(0);
            if (!_ok36) peg_restore(p, _m36);
        }
        peg_skip(p);
        if (!wat_parse_heap_type(p, &htv)) return false;
        peg_skip(p);
        if (!peg_match_n(p, ")", 1)) return false;
        memset(out, 0, sizeof *out); out->head = nullable ? 0x63 : 0x64;
         out->ht.has_value = true; out->ht.value.x = htv;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_ref_type_val(peg_state* p, jav_ref_type_t* out) {
    jav_val_type_t v;
    peg_skip(p);
    if (!wat_parse_val_type(p, &v)) return false;
    memset(out, 0, sizeof *out); out->head = v.head;
         out->ht.has_value = v.ht.has_value; out->ht.value = v.ht.value;
    return true;
}

static bool wat_parse_limits(peg_state* p, jav_limits_t* out) {
    peg_span ns; int is64 = 0;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                {
                    bool _ok2 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "i64", 3)) break;
                        {
                            peg_mark _m3 = peg_save(p);
                            bool _ok3 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok3 = true;
                            } while(0);
                            peg_restore(p, _m3);
                            if (_ok3) break;
                        }
                        is64 = 1;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!peg_match_n(p, "i32", 3)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!wat_nat(p, &ns)) return false;
    int ok; memset(out, 0, sizeof *out);
                  out->min = wat_int_lit(ns, 64, 0, &ok); if (!ok) return false;
    {
        peg_mark _m5 = peg_save(p);
        bool _ok5 = false;
        do {
            peg_skip(p);
            if (!wat_nat(p, &ns)) break;
            int ok; out->flag |= 1; out->max.has_value = true;
                    out->max.value = wat_int_lit(ns, 64, 0, &ok); if (!ok) return false;
            _ok5 = true;
        } while(0);
        if (!_ok5) peg_restore(p, _m5);
    }
    if (is64) out->flag |= 4;
    return true;
}

static bool wat_parse_expr(peg_state* p, jav_expr_t* out) {
    bbq_write_ctx_t* w = wat_wbuf_open(CTX);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    bbq_write_u8(w, 0x0B);
         if (CTX->pass != 2) { wat_wbuf_close(CTX, w); return true; }   /* pass 1: consumed, not decoded */
         bbq_ctx_t rc; bbq_ctx_init(&rc, w->data, w->pos);   /* decode the scratch -> tree */
         int ok = jav_expr_read(&rc, out);                  /* tree owns byte copies */
         bbq_ctx_free(&rc);
         wat_wbuf_close(CTX, w);
         if (!ok) return false;
    return true;
}

static bool wat_parse_offset_clause(peg_state* p, jav_expr_t* out) {
    bbq_write_ctx_t* w = wat_wbuf_open(CTX);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "offset", 6)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_folded_instr(p, w)) return false;
    _choice_done0:;
    }
    bbq_write_u8(w, 0x0B);
         if (CTX->pass != 2) { wat_wbuf_close(CTX, w); return true; }
         bbq_ctx_t rc; bbq_ctx_init(&rc, w->data, w->pos);   /* decode the scratch -> tree */
         int ok = jav_expr_read(&rc, out);                  /* tree owns byte copies */
         bbq_ctx_free(&rc);
         wat_wbuf_close(CTX, w);
         if (!ok) return false;
    return true;
}

static bool wat_parse_heap_type(peg_state* p, int64_t* ht) {
    peg_span ns;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "func", 4)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                *ht = -16;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "extern", 6)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                *ht = -17;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "any", 3)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                *ht = -18;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "eq", 2)) break;
                {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok8 = true;
                    } while(0);
                    peg_restore(p, _m8);
                    if (_ok8) break;
                }
                *ht = -19;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i31", 3)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                *ht = -20;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok11 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "struct", 6)) break;
                {
                    peg_mark _m12 = peg_save(p);
                    bool _ok12 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok12 = true;
                    } while(0);
                    peg_restore(p, _m12);
                    if (_ok12) break;
                }
                *ht = -21;
                _ok11 = true;
            } while(0);
            if (!_ok11) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok13 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "array", 5)) break;
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok14 = true;
                    } while(0);
                    peg_restore(p, _m14);
                    if (_ok14) break;
                }
                *ht = -22;
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok15 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "exn", 3)) break;
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok16 = true;
                    } while(0);
                    peg_restore(p, _m16);
                    if (_ok16) break;
                }
                *ht = -23;
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok17 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "none", 4)) break;
                {
                    peg_mark _m18 = peg_save(p);
                    bool _ok18 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok18 = true;
                    } while(0);
                    peg_restore(p, _m18);
                    if (_ok18) break;
                }
                *ht = -15;
                _ok17 = true;
            } while(0);
            if (!_ok17) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok19 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nofunc", 6)) break;
                {
                    peg_mark _m20 = peg_save(p);
                    bool _ok20 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok20 = true;
                    } while(0);
                    peg_restore(p, _m20);
                    if (_ok20) break;
                }
                *ht = -13;
                _ok19 = true;
            } while(0);
            if (!_ok19) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok21 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "noextern", 8)) break;
                {
                    peg_mark _m22 = peg_save(p);
                    bool _ok22 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok22 = true;
                    } while(0);
                    peg_restore(p, _m22);
                    if (_ok22) break;
                }
                *ht = -14;
                _ok21 = true;
            } while(0);
            if (!_ok21) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok23 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "noexn", 5)) break;
                {
                    peg_mark _m24 = peg_save(p);
                    bool _ok24 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok24 = true;
                    } while(0);
                    peg_restore(p, _m24);
                    if (_ok24) break;
                }
                *ht = -12;
                _ok23 = true;
            } while(0);
            if (!_ok23) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok25 = false;
            do {
                peg_skip(p);
                if (!wat_nat(p, &ns)) break;
                int ok; uint64_t tv = wat_int_lit(ns, 32, 0, &ok);   /* concrete typeidx (u32) */
                           if (!ok) return false; *ht = (int64_t)tv;
                _ok25 = true;
            } while(0);
            if (!_ok25) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_id(p, &ns)) return false;
        int64_t r = wat_resolve(CTX, SP_TYPE, ns);   /* concrete typeidx ($id) */
                           if (r < 0) return false; *ht = r;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_ref_type(peg_state* p, int* nullable, int64_t* ht) {
    *nullable = 0;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "ref", 3)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "null", 4)) break;
                        {
                            peg_mark _m4 = peg_save(p);
                            bool _ok4 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok4 = true;
                            } while(0);
                            peg_restore(p, _m4);
                            if (_ok4) break;
                        }
                        *nullable = 1;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) peg_restore(p, _m3);
                }
                peg_skip(p);
                if (!wat_parse_heap_type(p, ht)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "funcref", 7)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                *nullable = 1; *ht = -16;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "externref", 9)) break;
                {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok8 = true;
                    } while(0);
                    peg_restore(p, _m8);
                    if (_ok8) break;
                }
                *nullable = 1; *ht = -17;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "anyref", 6)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                *nullable = 1; *ht = -18;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok11 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "eqref", 5)) break;
                {
                    peg_mark _m12 = peg_save(p);
                    bool _ok12 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok12 = true;
                    } while(0);
                    peg_restore(p, _m12);
                    if (_ok12) break;
                }
                *nullable = 1; *ht = -19;
                _ok11 = true;
            } while(0);
            if (!_ok11) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok13 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i31ref", 6)) break;
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok14 = true;
                    } while(0);
                    peg_restore(p, _m14);
                    if (_ok14) break;
                }
                *nullable = 1; *ht = -20;
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok15 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "structref", 9)) break;
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok16 = true;
                    } while(0);
                    peg_restore(p, _m16);
                    if (_ok16) break;
                }
                *nullable = 1; *ht = -21;
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok17 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "arrayref", 8)) break;
                {
                    peg_mark _m18 = peg_save(p);
                    bool _ok18 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok18 = true;
                    } while(0);
                    peg_restore(p, _m18);
                    if (_ok18) break;
                }
                *nullable = 1; *ht = -22;
                _ok17 = true;
            } while(0);
            if (!_ok17) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok19 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "exnref", 6)) break;
                {
                    peg_mark _m20 = peg_save(p);
                    bool _ok20 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok20 = true;
                    } while(0);
                    peg_restore(p, _m20);
                    if (_ok20) break;
                }
                *nullable = 1; *ht = -23;
                _ok19 = true;
            } while(0);
            if (!_ok19) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok21 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullref", 7)) break;
                {
                    peg_mark _m22 = peg_save(p);
                    bool _ok22 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok22 = true;
                    } while(0);
                    peg_restore(p, _m22);
                    if (_ok22) break;
                }
                *nullable = 1; *ht = -15;
                _ok21 = true;
            } while(0);
            if (!_ok21) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok23 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullfuncref", 11)) break;
                {
                    peg_mark _m24 = peg_save(p);
                    bool _ok24 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok24 = true;
                    } while(0);
                    peg_restore(p, _m24);
                    if (_ok24) break;
                }
                *nullable = 1; *ht = -13;
                _ok23 = true;
            } while(0);
            if (!_ok23) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok25 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "nullexternref", 13)) break;
                {
                    peg_mark _m26 = peg_save(p);
                    bool _ok26 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok26 = true;
                    } while(0);
                    peg_restore(p, _m26);
                    if (_ok26) break;
                }
                *nullable = 1; *ht = -14;
                _ok25 = true;
            } while(0);
            if (!_ok25) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "nullexnref", 10)) return false;
        {
            peg_mark _m27 = peg_save(p);
            bool _ok27 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok27 = true;
            } while(0);
            peg_restore(p, _m27);
            if (_ok27) return false;
        }
        *nullable = 1; *ht = -12;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_ref_null_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op, uint8_t prefix) {
    int64_t ht = 0;
    if (shape != WSH_HEAP || prefix != 0) return false;
    peg_skip(p);
    if (!wat_parse_heap_type(p, &ht)) return false;
    bbq_write_u8(w, (uint8_t)op); bbq_write_sleb128_i64(w, ht);
    return true;
}

static bool wat_parse_ref_cast_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op, uint8_t prefix) {
    int nullable = 0; int64_t ht = 0;
    if (shape != WSH_HEAP || prefix == 0) return false;
    peg_skip(p);
    if (!wat_parse_ref_type(p, &nullable, &ht)) return false;
    bbq_write_u8(w, prefix);
         bbq_write_uleb128_u32(w, op + (uint32_t)nullable);
         bbq_write_sleb128_i64(w, ht);
    return true;
}

static bool wat_parse_br_on_cast_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span ns; uint32_t label = 0; int n1 = 0, n2 = 0; int64_t h1 = 0, h2 = 0;
    if (shape != WSH_BRONCAST) return false;
    peg_skip(p);
    if (!wat_parse_idx_ref(p, &ns)) return false;
    int64_t r = wat_resolve(CTX, SP_LABEL, ns); if (r < 0) return false;  /* $label or num */
                     label = (uint32_t)r;
    peg_skip(p);
    if (!wat_parse_ref_type(p, &n1, &h1)) return false;
    peg_skip(p);
    if (!wat_parse_ref_type(p, &n2, &h2)) return false;
    bbq_write_u8(w, (uint8_t)(n1 | (n2 << 1)));
         bbq_write_uleb128_u32(w, label);
         bbq_write_sleb128_i64(w, h1);
         bbq_write_sleb128_i64(w, h2);
    return true;
}

static bool wat_parse_v128_const_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op) {
    peg_span ns; uint64_t bits[16]; int nv = 0, count = 0, width = 0, isf = 0;
       int neg = 0, isinf = 0, isnan = 0, ispay = 0, special = 0; uint64_t pay = 0;
    if (shape != WSH_V128 || op != 12) return false;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i8x16", 5)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                count = 16; width = 1;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i16x8", 5)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                count = 8;  width = 2;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i32x4", 5)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                count = 4;  width = 4;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "i64x2", 5)) break;
                {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok8 = true;
                    } while(0);
                    peg_restore(p, _m8);
                    if (_ok8) break;
                }
                count = 2;  width = 8;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "f32x4", 5)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                count = 4;  width = 4; isf = 1;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "f64x2", 5)) return false;
        {
            peg_mark _m11 = peg_save(p);
            bool _ok11 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok11 = true;
            } while(0);
            peg_restore(p, _m11);
            if (_ok11) return false;
        }
        count = 2;  width = 8; isf = 1;
    _choice_done0:;
    }
    for (;;) {
        peg_mark _m12 = peg_save(p);
        bool _ok12 = false;
        do {
            neg = isinf = isnan = ispay = special = 0; pay = 0;
            {
                peg_mark _m13 = peg_save(p);
                {
                    bool _ok14 = false;
                    do {
                        {
                            peg_mark _m15 = peg_save(p);
                            bool _ok15 = false;
                            do {
                                peg_skip(p);
                                if (peg_peek_at_n(p, "+", 1)) {
                                    peg_skip(p);
                                    if (!peg_match_n(p, "+", 1)) break;
                                } else {
                                    peg_skip(p);
                                    if (!peg_match_n(p, "-", 1)) break;
                                    neg = 1;
                                }
                                _ok15 = true;
                            } while(0);
                            if (!_ok15) peg_restore(p, _m15);
                        }
                        {
                            peg_mark _m16 = peg_save(p);
                            {
                                bool _ok17 = false;
                                do {
                                    peg_skip(p);
                                    if (!peg_match_n(p, "inf", 3)) break;
                                    {
                                        peg_mark _m18 = peg_save(p);
                                        bool _ok18 = false;
                                        do {
                                            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                            peg_advance(p);
                                            _ok18 = true;
                                        } while(0);
                                        peg_restore(p, _m18);
                                        if (_ok18) break;
                                    }
                                    isinf = 1; special = 1;
                                    _ok17 = true;
                                } while(0);
                                if (!_ok17) {
                                    peg_restore(p, _m16);
                                } else goto _choice_done16;
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, "nan", 3)) break;
                            {
                                peg_mark _m19 = peg_save(p);
                                {
                                    bool _ok20 = false;
                                    do {
                                        peg_skip(p);
                                        if (!peg_match_n(p, ":", 1)) break;
                                        peg_skip(p);
                                        if (!wat_hexnat(p, &ns)) break;
                                        isnan = 1; ispay = 1; special = 1;
                                    if (!wat_nan_payload(ns, width, &pay)) return false;
                                        _ok20 = true;
                                    } while(0);
                                    if (!_ok20) {
                                        peg_restore(p, _m19);
                                    } else goto _choice_done19;
                                }
                                {
                                    peg_mark _m21 = peg_save(p);
                                    bool _ok21 = false;
                                    do {
                                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                        peg_advance(p);
                                        _ok21 = true;
                                    } while(0);
                                    peg_restore(p, _m21);
                                    if (_ok21) break;
                                }
                                isnan = 1; special = 1;
                            _choice_done19:;
                            }
                        _choice_done16:;
                        }
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) {
                        peg_restore(p, _m13);
                    } else goto _choice_done13;
                }
                peg_skip(p);
                if (!wat_fnum(p, &ns)) break;
                special = 0;
            _choice_done13:;
            }
            if (special && !isf) return false;                            /* inf/nan in an integer lane is malformed */
           int ok; uint64_t bv;
           if (special)    bv = wat_float_bits(width, neg, isinf, isnan, ispay, pay, 0);
           else if (isf) { bv = wat_float_lit(ns, width, &ok); if (!ok) return false; }
           else          { bv = wat_int_lit(ns, width * 8, 1, &ok); if (!ok) return false; }
           if (nv < 16) bits[nv] = bv;
           nv++;
            _ok12 = true;
        } while(0);
        if (!_ok12) { peg_restore(p, _m12); break; }
    }
    if (nv != count) return false;
         for (int i = 0; i < count; i++) switch (width) {
           case 1: bbq_write_u8(w, (uint8_t)bits[i]); break;
           case 2: bbq_write_u16le(w, (uint16_t)bits[i]); break;
           case 4: bbq_write_u32le(w, (uint32_t)bits[i]); break;
           case 8: bbq_write_u64le(w, bits[i]); break;
         }
    return true;
}

static bool wat_parse_v128_shuffle_op(peg_state* p, bbq_write_ctx_t* w, int shape, uint32_t op) {
    peg_span ns; uint64_t iv[16]; int nv = 0;
    if (shape != WSH_V128 || op != 13) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_nat(p, &ns)) break;
            int ok; uint64_t v = wat_int_lit(ns, 8, 0, &ok);    /* laneidx: u8 */
                    if (!ok) return false;
                    if (nv < 16) iv[nv] = v; nv++;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (nv != 16) return false;
         for (int i = 0; i < 16; i++) bbq_write_u8(w, (uint8_t)iv[i]);
    return true;
}

static bool wat_parse_block_type(peg_state* p, bbq_write_ctx_t* w) {
    jav_func_type_t ft;
       peg_span tref = {0,0}; int has_ref = 0; int sv = CTX->bind_locals; CTX->bind_locals = 0;
       bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS);
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "type", 4)) break;
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok1 = true;
                } while(0);
                peg_restore(p, _m1);
                if (_ok1) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_ref = 1;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!wat_parse_b_param(p, &SC_I_PARAMS)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    for (;;) {
        peg_mark _m3 = peg_save(p);
        bool _ok3 = false;
        do {
            peg_skip(p);
            if (!wat_parse_result(p, &SC_I_RESULTS)) break;
            _ok3 = true;
        } while(0);
        if (!_ok3) { peg_restore(p, _m3); break; }
    }
    CTX->bind_locals = sv;
         jav_val_type_t* rs = SC_I_RESULTS;
         int np = (int)bbq_vec_len(SC_I_PARAMS), nr = (int)bbq_vec_len(rs);
         if (has_ref) {
             int64_t r = wat_resolve(CTX, SP_TYPE, tref);
             if (r < 0 || !wat_typeuse_ref(CTX, r, &SC_I_PARAMS, &SC_I_RESULTS)) {
                 bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS); return false;
             }
             bbq_write_sleb128_i64(w, r);
         } else if (np == 0 && nr == 0) {
             bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS);
             bbq_write_u8(w, 0x40);                        /* empty */
         } else if (np == 0 && nr == 1) {
             bbq_write_u8(w, rs[0].head);                  /* single result valtype */
             if (rs[0].ht.has_value) bbq_write_sleb128_i64(w, rs[0].ht.value.x);
             bbq_vec_free(SC_I_PARAMS); bbq_vec_free(SC_I_RESULTS);
         } else {                                          /* multi-value -> typeidx (s33) */
             memset(&ft, 0, sizeof ft);
             WAT_FREEZE(SC_I_PARAMS, ft.params);  ft.param_count  = (uint32_t)ft.params.count;
             WAT_FREEZE(SC_I_RESULTS, ft.results); ft.result_count = (uint32_t)ft.results.count;
             bbq_write_sleb128_i64(w, (int64_t)wat_typeuse(CTX, &ft));
         }
    return true;
}

static bool wat_parse_block_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span lab = {0,0}, elab = {0,0};
    if (shape != WSH_BLOCK) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &lab)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    if (!wat_name_push(&CTX->labels, lab)) return false;
    peg_skip(p);
    if (!wat_parse_block_type(p, w)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "end", 3)) return false;
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok2 = true;
        } while(0);
        peg_restore(p, _m2);
        if (_ok2) return false;
    }
    {
        peg_mark _m3 = peg_save(p);
        bool _ok3 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &elab)) break;
            _ok3 = true;
        } while(0);
        if (!_ok3) peg_restore(p, _m3);
    }
    if (!wat_endlabel_ok(lab, elab)) return false;   /* §6.5.3 mismatching label */
         bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
    return true;
}

static bool wat_parse_if_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span lab = {0,0}, elab = {0,0}, xlab = {0,0};
    if (shape != WSH_IF) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &lab)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    if (!wat_name_push(&CTX->labels, lab)) return false;
    peg_skip(p);
    if (!wat_parse_block_type(p, w)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "else", 4)) break;
            {
                peg_mark _m3 = peg_save(p);
                bool _ok3 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok3 = true;
                } while(0);
                peg_restore(p, _m3);
                if (_ok3) break;
            }
            {
                peg_mark _m4 = peg_save(p);
                bool _ok4 = false;
                do {
                    peg_skip(p);
                    if (!wat_id(p, &elab)) break;
                    _ok4 = true;
                } while(0);
                if (!_ok4) peg_restore(p, _m4);
            }
            if (!wat_endlabel_ok(lab, elab)) return false;  /* §6.5.3 mismatching label */
           bbq_write_u8(w, 0x05);
            for (;;) {
                peg_mark _m5 = peg_save(p);
                bool _ok5 = false;
                do {
                    peg_skip(p);
                    if (!wat_parse_instr(p, w)) break;
                    _ok5 = true;
                } while(0);
                if (!_ok5) { peg_restore(p, _m5); break; }
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) peg_restore(p, _m2);
    }
    peg_skip(p);
    if (!peg_match_n(p, "end", 3)) return false;
    {
        peg_mark _m6 = peg_save(p);
        bool _ok6 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok6 = true;
        } while(0);
        peg_restore(p, _m6);
        if (_ok6) return false;
    }
    {
        peg_mark _m7 = peg_save(p);
        bool _ok7 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &xlab)) break;
            _ok7 = true;
        } while(0);
        if (!_ok7) peg_restore(p, _m7);
    }
    if (!wat_endlabel_ok(lab, xlab)) return false;    /* §6.5.3 mismatching label */
         bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
    return true;
}

static bool wat_parse_try_table_op(peg_state* p, bbq_write_ctx_t* w, int shape) {
    peg_span lab = {0,0}, elab = {0,0}; wat_catch_t* cats = NULL;
    if (shape != WSH_TRYTABLE) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &lab)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!wat_parse_block_type(p, w)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_parse_catch(p, &cats)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    int nc = (int)bbq_vec_len(cats);
         bbq_write_uleb128_u32(w, (uint32_t)nc);
         for (int i = 0; i < nc; i++) {
             bbq_write_u8(w, cats[i].kind);
             if (cats[i].kind == 0 || cats[i].kind == 1) bbq_write_uleb128_u32(w, cats[i].tag);
             bbq_write_uleb128_u32(w, cats[i].label);
         }
         bbq_vec_free(cats);
         if (!wat_name_push(&CTX->labels, lab)) return false;
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!wat_parse_instr(p, w)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "end", 3)) return false;
    {
        peg_mark _m3 = peg_save(p);
        bool _ok3 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok3 = true;
        } while(0);
        peg_restore(p, _m3);
        if (_ok3) return false;
    }
    {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &elab)) break;
            _ok4 = true;
        } while(0);
        if (!_ok4) peg_restore(p, _m4);
    }
    if (!wat_endlabel_ok(lab, elab)) return false;   /* §6.5.3 mismatching label */
         bbq_write_u8(w, 0x0B); wat_label_pop(CTX);
    return true;
}

static bool wat_parse_catch(peg_state* p, wat_catch_t** out) {
    wat_catch_t c; peg_span tg = {0,0}, lb = {0,0};
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    memset(&c, 0, sizeof c);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "catch_ref", 9)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &tg)) break;
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &lb)) break;
                c.kind = 1;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "catch_all_ref", 13)) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &lb)) break;
                c.kind = 3;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "catch_all", 9)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &lb)) break;
                c.kind = 2;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "catch", 5)) return false;
        {
            peg_mark _m7 = peg_save(p);
            bool _ok7 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok7 = true;
            } while(0);
            peg_restore(p, _m7);
            if (_ok7) return false;
        }
        peg_skip(p);
        if (!wat_parse_idx_ref(p, &tg)) return false;
        peg_skip(p);
        if (!wat_parse_idx_ref(p, &lb)) return false;
        c.kind = 0;
    _choice_done0:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    int64_t lbl = wat_resolve(CTX, SP_LABEL, lb); if (lbl < 0) return false;
         c.label = (uint32_t)lbl;
         if (c.kind == 0 || c.kind == 1) {
             int64_t tag = wat_resolve(CTX, SP_TAG, tg); if (tag < 0) return false;
             c.tag = (uint32_t)tag;
         }
         bbq_vec_push(*out, c);
    return true;
}

static bool wat_parse_name(peg_state* p, jav_name_t* out) {
    peg_span s;
    peg_skip(p);
    if (!wat_string(p, &s)) return false;
    size_t blen; uint8_t* b = wat_parse_string(s, &blen);
         if (!b) return false;                            /* §6.3.3 malformed escape */
         out->count = (uint32_t)blen; out->bytes.data = b; out->bytes.length = blen;
         if (!jav_name_utf8_ok(out->bytes)) {            /* §6.3.4: a name is valid UTF-8 */
             free(b); memset(out, 0, sizeof *out); return false;
         }
    return true;
}

static bool wat_parse_export_field(peg_state* p, jav_export_t** exports) {
    jav_name_t nm; uint8_t kind = 0; int space = SP_FUNC; peg_span r = {0,0};
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "export", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    peg_skip(p);
    if (!wat_parse_name(p, &nm)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "func", 4)) break;
                {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    peg_restore(p, _m3);
                    if (_ok3) break;
                }
                kind = 0; space = SP_FUNC;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "table", 5)) break;
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok5 = true;
                    } while(0);
                    peg_restore(p, _m5);
                    if (_ok5) break;
                }
                kind = 1; space = SP_TABLE;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "memory", 6)) break;
                {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok7 = true;
                    } while(0);
                    peg_restore(p, _m7);
                    if (_ok7) break;
                }
                kind = 2; space = SP_MEM;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "global", 6)) break;
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok9 = true;
                    } while(0);
                    peg_restore(p, _m9);
                    if (_ok9) break;
                }
                kind = 3; space = SP_GLOBAL;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        peg_skip(p);
        if (!peg_match_n(p, "tag", 3)) return false;
        {
            peg_mark _m10 = peg_save(p);
            bool _ok10 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok10 = true;
            } while(0);
            peg_restore(p, _m10);
            if (_ok10) return false;
        }
        kind = 4; space = SP_TAG;
    _choice_done1:;
    }
    peg_skip(p);
    if (!wat_parse_idx_ref(p, &r)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { free((void*)nm.bytes.data); return true; }
         int64_t idx = wat_resolve(CTX, space, r);
         if (idx < 0) { free((void*)nm.bytes.data); return false; }
         jav_export_t e; memset(&e, 0, sizeof e);
         e.name = nm; e.kind = kind; e.idx = (uint32_t)idx;
         bbq_vec_push(*exports, e);
    return true;
}

static bool wat_parse_start_field(peg_state* p, uint32_t* start_func, int* has_start) {
    peg_span r = {0,0};
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "start", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    peg_skip(p);
    if (!wat_parse_idx_ref(p, &r)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) return true;
         if (*has_start) return false;                    /* §6.6.16: at most one start */
         int64_t idx = wat_resolve(CTX, SP_FUNC, r); if (idx < 0) return false;
         *start_func = (uint32_t)idx; *has_start = 1;
    return true;
}

static bool wat_parse_memory_field(peg_state* p, jav_mem_entry_t** mems, jav_data_t** datas) {
    jav_mem_entry_t m; memset(&m, 0, sizeof m); peg_span idsp = {0,0}; jav_name_t nm;
       jav_name_t imod = {0}, ifld = {0}; int has_import = 0, has_inline_data = 0, is64 = 0;
       jav_byte_vec_t bytes; memset(&bytes, 0, sizeof bytes);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "memory", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    wat_enames_clear(CTX);
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "export", 6)) break;
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            peg_restore(p, _m5);
                            if (_ok5) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_name(p, &nm)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        if (CTX->pass == 2) bbq_vec_push(SC_ENAMES, nm); else free((void*)nm.bytes.data);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "import", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_name(p, &imod)) break;
                peg_skip(p);
                if (!wat_parse_name(p, &ifld)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                if (CTX->pass == 2 && CTX->defs_seen) {              /* §6.6: imports precede definitions */
             free((void*)imod.bytes.data); free((void*)ifld.bytes.data); return false; }
             has_import = 1;
            _choice_done3:;
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    if (CTX->pass == 1) { if (!(has_import ? wat_imp_add(CTX, SP_MEM, idsp)
                                                         : wat_id_add(CTX, SP_MEM, idsp))) return false; }
    {
        peg_mark _m7 = peg_save(p);
        {
            bool _ok8 = false;
            do {
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        {
                            peg_mark _m10 = peg_save(p);
                            {
                                bool _ok11 = false;
                                do {
                                    peg_skip(p);
                                    if (!peg_match_n(p, "i64", 3)) break;
                                    {
                                        peg_mark _m12 = peg_save(p);
                                        bool _ok12 = false;
                                        do {
                                            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                            peg_advance(p);
                                            _ok12 = true;
                                        } while(0);
                                        peg_restore(p, _m12);
                                        if (_ok12) break;
                                    }
                                    is64 = 1;
                                    _ok11 = true;
                                } while(0);
                                if (!_ok11) {
                                    peg_restore(p, _m10);
                                } else goto _choice_done10;
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, "i32", 3)) break;
                            {
                                peg_mark _m13 = peg_save(p);
                                bool _ok13 = false;
                                do {
                                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok13 = true;
                                } while(0);
                                peg_restore(p, _m13);
                                if (_ok13) break;
                            }
                        _choice_done10:;
                        }
                        _ok9 = true;
                    } while(0);
                    if (!_ok9) peg_restore(p, _m9);
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "data", 4)) break;
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok14 = true;
                    } while(0);
                    peg_restore(p, _m14);
                    if (_ok14) break;
                }
                peg_skip(p);
                if (!wat_parse_data_string(p, &bytes)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                has_inline_data = 1;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m7);
            } else goto _choice_done7;
        }
        peg_skip(p);
        if (!wat_parse_limits(p, &m.limits)) return false;
    _choice_done7:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { if (has_import) { free((void*)imod.bytes.data); free((void*)ifld.bytes.data); }
             free((void*)bytes.bytes.data); return true; }
         uint32_t myidx;
         if (has_inline_data) {            /* §6.6.5: min=max=ceil(|b|/65536), fixed limits, addrtype flag */
             uint64_t pages = ((uint64_t)bytes.count + 65535u) / 65536u;
             m.limits.min = pages; m.limits.max.has_value = true; m.limits.max.value = pages;
             m.limits.flag = (uint8_t)(1 | (is64 ? 4 : 0));
         }
         if (has_import) { jav_extern_type_t d; memset(&d, 0, sizeof d);
             d.kind = 2; d.body.tag = 2; d.body.u.case_2 = m.limits;
             myidx = wat_inline_import(CTX, imod, ifld, d, SP_MEM); }
         else { myidx = (uint32_t)(bbq_vec_len(CTX->sp_imp[SP_MEM]) + bbq_vec_len(*mems)); bbq_vec_push(*mems, m); CTX->defs_seen = 1; }
         if (has_inline_data) {            /* the generated active data: (data (memory myidx) (i32.const 0) b*) */
             jav_data_t d; memset(&d, 0, sizeof d);
             d.flag = 2; d.body.tag = 2; d.body.u.case_2.memidx = myidx;
             d.body.u.case_2.offset = wat_const0_expr(CTX, is64); d.body.u.case_2.data = bytes;
             bbq_vec_push(*datas, d);
         }
         wat_flush_exports(CTX, SC_ENAMES, 2, myidx); SC_ENAMES = NULL;
    return true;
}

static bool wat_parse_global_field(peg_state* p, jav_global_t** globals) {
    jav_global_t g; memset(&g, 0, sizeof g); peg_span idsp = {0,0}; jav_name_t nm;
       jav_name_t imod = {0}, ifld = {0}; int has_import = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "global", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    wat_enames_clear(CTX);
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "export", 6)) break;
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            peg_restore(p, _m5);
                            if (_ok5) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_name(p, &nm)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        if (CTX->pass == 2) bbq_vec_push(SC_ENAMES, nm); else free((void*)nm.bytes.data);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "import", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_name(p, &imod)) break;
                peg_skip(p);
                if (!wat_parse_name(p, &ifld)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                if (CTX->pass == 2 && CTX->defs_seen) {              /* §6.6: imports precede definitions */
             free((void*)imod.bytes.data); free((void*)ifld.bytes.data); return false; }
             has_import = 1;
            _choice_done3:;
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    if (CTX->pass == 1) { if (!(has_import ? wat_imp_add(CTX, SP_GLOBAL, idsp)
                                                         : wat_id_add(CTX, SP_GLOBAL, idsp))) return false; }
    {
        peg_mark _m7 = peg_save(p);
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "mut", 3)) break;
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok9 = true;
                    } while(0);
                    peg_restore(p, _m9);
                    if (_ok9) break;
                }
                peg_skip(p);
                if (!wat_parse_val_type(p, &g.type.type)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                g.type.mut = 1;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m7);
            } else goto _choice_done7;
        }
        peg_skip(p);
        if (!wat_parse_val_type(p, &g.type.type)) return false;
        g.type.mut = 0;
    _choice_done7:;
    }
    peg_skip(p);
    if (!wat_parse_expr(p, &g.init)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { if (has_import) { free((void*)imod.bytes.data); free((void*)ifld.bytes.data); } return true; }
         uint32_t myidx;
         if (has_import) { jav_extern_type_t d; memset(&d, 0, sizeof d);
             d.kind = 3; d.body.tag = 3; d.body.u.case_3.type = g.type.type; d.body.u.case_3.mut = g.type.mut;
             myidx = wat_inline_import(CTX, imod, ifld, d, SP_GLOBAL); }
         else { myidx = (uint32_t)(bbq_vec_len(CTX->sp_imp[SP_GLOBAL]) + bbq_vec_len(*globals)); bbq_vec_push(*globals, g); CTX->defs_seen = 1; }
         wat_flush_exports(CTX, SC_ENAMES, 3, myidx); SC_ENAMES = NULL;
    return true;
}

static bool wat_parse_tag_field(peg_state* p, jav_tag_type_t** tags) {
    jav_tag_type_t t; memset(&t, 0, sizeof t); peg_span idsp = {0,0}, tref = {0,0};
       int has_ref = 0; jav_func_type_t ft;
       jav_name_t nm; jav_name_t imod = {0}, ifld = {0}; int has_import = 0;
       CTX->bind_locals = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "tag", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);   /* reclaim abandoned scratch */
         wat_enames_clear(CTX);
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "export", 6)) break;
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            peg_restore(p, _m5);
                            if (_ok5) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_name(p, &nm)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        if (CTX->pass == 2) bbq_vec_push(SC_ENAMES, nm); else free((void*)nm.bytes.data);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "import", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_name(p, &imod)) break;
                peg_skip(p);
                if (!wat_parse_name(p, &ifld)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                if (CTX->pass == 2 && CTX->defs_seen) {              /* §6.6: imports precede definitions */
             free((void*)imod.bytes.data); free((void*)ifld.bytes.data); return false; }
             has_import = 1;
            _choice_done3:;
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    if (CTX->pass == 1) { if (!(has_import ? wat_imp_add(CTX, SP_TAG, idsp)
                                                         : wat_id_add(CTX, SP_TAG, idsp))) return false; }
    {
        peg_mark _m7 = peg_save(p);
        bool _ok7 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "type", 4)) break;
            {
                peg_mark _m8 = peg_save(p);
                bool _ok8 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok8 = true;
                } while(0);
                peg_restore(p, _m8);
                if (_ok8) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_ref = 1;
            _ok7 = true;
        } while(0);
        if (!_ok7) peg_restore(p, _m7);
    }
    for (;;) {
        peg_mark _m9 = peg_save(p);
        bool _ok9 = false;
        do {
            peg_skip(p);
            if (!wat_parse_param(p, &SC_F_PARAMS)) break;
            _ok9 = true;
        } while(0);
        if (!_ok9) { peg_restore(p, _m9); break; }
    }
    for (;;) {
        peg_mark _m10 = peg_save(p);
        bool _ok10 = false;
        do {
            peg_skip(p);
            if (!wat_parse_result(p, &SC_F_RESULTS)) break;
            _ok10 = true;
        } while(0);
        if (!_ok10) { peg_restore(p, _m10); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);
             if (has_import) { free((void*)imod.bytes.data); free((void*)ifld.bytes.data); } return true; }
         uint32_t tidx;
         if (has_ref) { int64_t r = wat_resolve(CTX, SP_TYPE, tref);
                        if (r < 0 || !wat_typeuse_ref(CTX, r, &SC_F_PARAMS, &SC_F_RESULTS)) {
                            bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS); return false;
                        }
                        tidx = (uint32_t)r; }
         else { memset(&ft, 0, sizeof ft);
                WAT_FREEZE(SC_F_PARAMS, ft.params);  ft.param_count  = (uint32_t)ft.params.count;
                WAT_FREEZE(SC_F_RESULTS, ft.results); ft.result_count = (uint32_t)ft.results.count;
                tidx = wat_typeuse(CTX, &ft); }
         uint32_t myidx;
         if (has_import) { jav_extern_type_t d; memset(&d, 0, sizeof d);
             d.kind = 4; d.body.tag = 4; d.body.u.case_4.attr = 0; d.body.u.case_4.type = tidx;
             myidx = wat_inline_import(CTX, imod, ifld, d, SP_TAG); }
         else { myidx = (uint32_t)(bbq_vec_len(CTX->sp_imp[SP_TAG]) + bbq_vec_len(*tags));
                t.attr = 0; t.type = tidx; bbq_vec_push(*tags, t); CTX->defs_seen = 1; }
         wat_flush_exports(CTX, SC_ENAMES, 4, myidx); SC_ENAMES = NULL;
    return true;
}

static bool wat_parse_table_field(peg_state* p, jav_table_t** tables, jav_elem_t** elems) {
    jav_table_t tb; memset(&tb, 0, sizeof tb); peg_span idsp = {0,0}; jav_name_t nm;
       jav_name_t imod = {0}, ifld = {0};
       jav_expr_t init; memset(&init, 0, sizeof init);
       int has_import = 0, has_inline_elem = 0, has_init = 0, is64 = 0;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "table", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    wat_enames_clear(CTX);
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            {
                peg_mark _m3 = peg_save(p);
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "export", 6)) break;
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            peg_restore(p, _m5);
                            if (_ok5) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_name(p, &nm)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        if (CTX->pass == 2) bbq_vec_push(SC_ENAMES, nm); else free((void*)nm.bytes.data);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m3);
                    } else goto _choice_done3;
                }
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "import", 6)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                peg_skip(p);
                if (!wat_parse_name(p, &imod)) break;
                peg_skip(p);
                if (!wat_parse_name(p, &ifld)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                if (CTX->pass == 2 && CTX->defs_seen) {              /* §6.6: imports precede definitions */
             free((void*)imod.bytes.data); free((void*)ifld.bytes.data); return false; }
             has_import = 1;
            _choice_done3:;
            }
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    if (CTX->pass == 1) { if (!(has_import ? wat_imp_add(CTX, SP_TABLE, idsp)
                                                         : wat_id_add(CTX, SP_TABLE, idsp))) return false; }
    {
        peg_mark _m7 = peg_save(p);
        {
            bool _ok8 = false;
            do {
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        {
                            peg_mark _m10 = peg_save(p);
                            {
                                bool _ok11 = false;
                                do {
                                    peg_skip(p);
                                    if (!peg_match_n(p, "i64", 3)) break;
                                    {
                                        peg_mark _m12 = peg_save(p);
                                        bool _ok12 = false;
                                        do {
                                            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                            peg_advance(p);
                                            _ok12 = true;
                                        } while(0);
                                        peg_restore(p, _m12);
                                        if (_ok12) break;
                                    }
                                    is64 = 1;
                                    _ok11 = true;
                                } while(0);
                                if (!_ok11) {
                                    peg_restore(p, _m10);
                                } else goto _choice_done10;
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, "i32", 3)) break;
                            {
                                peg_mark _m13 = peg_save(p);
                                bool _ok13 = false;
                                do {
                                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok13 = true;
                                } while(0);
                                peg_restore(p, _m13);
                                if (_ok13) break;
                            }
                        _choice_done10:;
                        }
                        _ok9 = true;
                    } while(0);
                    if (!_ok9) peg_restore(p, _m9);
                }
                peg_skip(p);
                if (!wat_parse_ref_type_val(p, &tb.u.default_val.type.reftype)) break;
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "elem", 4)) break;
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok14 = true;
                    } while(0);
                    peg_restore(p, _m14);
                    if (_ok14) break;
                }
                peg_skip(p);
                if (!wat_parse_elem_list(p)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                has_inline_elem = 1;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m7);
            } else goto _choice_done7;
        }
        peg_skip(p);
        if (!wat_parse_limits(p, &tb.u.default_val.type.limits)) return false;
        peg_skip(p);
        if (!wat_parse_ref_type_val(p, &tb.u.default_val.type.reftype)) return false;
        {
            peg_mark _m15 = peg_save(p);
            bool _ok15 = false;
            do {
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        _ok16 = true;
                    } while(0);
                    peg_restore(p, _m16);
                    if (_ok16) break;
                }
                peg_skip(p);
                if (!wat_parse_expr(p, &init)) break;
                has_init = 1;
                _ok15 = true;
            } while(0);
            if (!_ok15) peg_restore(p, _m15);
        }
    _choice_done7:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { if (has_import) { free((void*)imod.bytes.data); free((void*)ifld.bytes.data); }
             bbq_vec_free(CTX->el_funcs); bbq_vec_free(CTX->el_exprs); return true; }
         uint32_t myidx;
         if (has_import) { jav_extern_type_t d; memset(&d, 0, sizeof d);
             d.kind = 1; d.body.tag = 1;
             d.body.u.case_1.reftype = tb.u.default_val.type.reftype;
             d.body.u.case_1.limits  = tb.u.default_val.type.limits;
             myidx = wat_inline_import(CTX, imod, ifld, d, SP_TABLE); }
         else {
             myidx = (uint32_t)(bbq_vec_len(CTX->sp_imp[SP_TABLE]) + bbq_vec_len(*tables));
             if (has_inline_elem) {            /* §6.6.6: limits min=max=|elems|, addrtype flag */
                 uint32_t n = CTX->el_is_funcidx ? (uint32_t)bbq_vec_len(CTX->el_funcs)
                                                 : (uint32_t)bbq_vec_len(CTX->el_exprs);
                 tb.u.default_val.type.limits.min = n;
                 tb.u.default_val.type.limits.max.has_value = true;
                 tb.u.default_val.type.limits.max.value = n;
                 tb.u.default_val.type.limits.flag = (uint8_t)(1 | (is64 ? 4 : 0));
             }
             if (has_init) {                             /* §6.6.6 explicit init expr → the (0x40 0x00) init form */
                 jav_table_type_t tt = tb.u.default_val.type;
                 tb.tag = jav_table_case_0;
                 tb.u.case_0.marker0 = 0x40; tb.u.case_0.marker1 = 0x00;
                 tb.u.case_0.type = tt; tb.u.case_0.init = init;
             } else {
                 tb.tag = jav_table_default_val;        /* abbreviated form (init defaults to ref.null) */
             }
             bbq_vec_push(*tables, tb);
             CTX->defs_seen = 1;
             if (has_inline_elem) {            /* the generated active elem: (elem (table myidx) (i32.const 0) …) */
                 jav_elem_t el; memset(&el, 0, sizeof el);
                 /* §3.5.9: a bare funcidx list has element type (ref func); in a concrete
                  * (ref $t) table that is not <: the table type, so emit the funcs as
                  * (ref.func) exprs carrying the table's own reftype instead. */
                 jav_ref_type_t trt = tb.u.default_val.type.reftype;
                 int concrete_rt = (trt.head == 0x63 || trt.head == 0x64) && trt.ht.has_value && trt.ht.value.x >= 0;
                 if (CTX->el_is_funcidx && !concrete_rt) {
                     jav_idx_vec_t fv; memset(&fv, 0, sizeof fv);
                     WAT_FREEZE(CTX->el_funcs, fv.idxs); fv.count = (uint32_t)fv.idxs.count; bbq_vec_free(CTX->el_exprs);
                     el.flag = 2; el.body.tag = 2; el.body.u.case_2.table = myidx;
                     el.body.u.case_2.offset = wat_const0_expr(CTX, is64); el.body.u.case_2.elemkind = 0;
                     el.body.u.case_2.funcs = fv;
                 } else if (CTX->el_is_funcidx) {  /* concrete table reftype: funcidxs → (ref.func) exprs */
                     jav_expr_t* tmp = NULL;
                     for (uint32_t fi = 0; fi < (uint32_t)bbq_vec_len(CTX->el_funcs); fi++)
                         bbq_vec_push(tmp, wat_reffunc_expr(CTX, CTX->el_funcs[fi]));
                     jav_expr_vec_t ev; memset(&ev, 0, sizeof ev);
                     WAT_FREEZE(tmp, ev.exprs); ev.count = (uint32_t)ev.exprs.count;
                     bbq_vec_free(CTX->el_funcs); bbq_vec_free(CTX->el_exprs);
                     el.flag = 6; el.body.tag = 6; el.body.u.case_6.table = myidx;
                     el.body.u.case_6.offset = wat_const0_expr(CTX, is64);
                     el.body.u.case_6.reftype = trt; el.body.u.case_6.exprs = ev;
                 } else {
                     jav_expr_vec_t ev; memset(&ev, 0, sizeof ev);
                     WAT_FREEZE(CTX->el_exprs, ev.exprs); ev.count = (uint32_t)ev.exprs.count; bbq_vec_free(CTX->el_funcs);
                     el.flag = 6; el.body.tag = 6; el.body.u.case_6.table = myidx;
                     el.body.u.case_6.offset = wat_const0_expr(CTX, is64);
                     el.body.u.case_6.reftype = tb.u.default_val.type.reftype; el.body.u.case_6.exprs = ev;
                 }
                 bbq_vec_push(*elems, el);
             }
         }
         wat_flush_exports(CTX, SC_ENAMES, 1, myidx); SC_ENAMES = NULL;
    return true;
}

static bool wat_parse_data_string(peg_state* p, jav_byte_vec_t* out) {
    peg_span s; uint8_t* buf = NULL;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!wat_string(p, &s)) break;
            size_t bl; uint8_t* b = wat_parse_string(s, &bl);
                      for (size_t i = 0; i < bl; i++) bbq_vec_push(buf, b[i]); free(b);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    size_t n = bbq_vec_len(buf);
         uint8_t* plain = (uint8_t*)malloc(n ? n : 1); if (n) memcpy(plain, buf, n);
         bbq_vec_free(buf);
         out->count = (uint32_t)n; out->bytes.data = plain; out->bytes.length = n;
    return true;
}

static bool wat_parse_data_field(peg_state* p, jav_data_t** datas) {
    jav_data_t d; memset(&d, 0, sizeof d); peg_span idsp = {0,0}, mref = {0,0};
       int has_mem = 0, has_off = 0;
       jav_byte_vec_t bytes; memset(&bytes, 0, sizeof bytes);
       jav_expr_t off; memset(&off, 0, sizeof off);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "data", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    if (CTX->pass == 1 && !wat_id_add(CTX, SP_DATA, idsp)) return false;
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "memory", 6)) break;
            {
                peg_mark _m3 = peg_save(p);
                bool _ok3 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok3 = true;
                } while(0);
                peg_restore(p, _m3);
                if (_ok3) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &mref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_mem = 1;
            _ok2 = true;
        } while(0);
        if (!_ok2) peg_restore(p, _m2);
    }
    {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            peg_skip(p);
            if (!wat_parse_offset_clause(p, &off)) break;
            has_off = 1;
            _ok4 = true;
        } while(0);
        if (!_ok4) peg_restore(p, _m4);
    }
    peg_skip(p);
    if (!wat_parse_data_string(p, &bytes)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { free((void*)bytes.bytes.data); return true; }
         if (!has_off) { d.flag = 1; d.body.tag = 1; d.body.u.case_1.data = bytes; }            /* passive */
         else if (!has_mem) { d.flag = 0; d.body.tag = 0;                                       /* active mem 0 */
             d.body.u.case_0.offset = off; d.body.u.case_0.data = bytes; }
         else { int64_t mi = wat_resolve(CTX, SP_MEM, mref); if (mi < 0) return false;          /* active explicit */
             d.flag = 2; d.body.tag = 2; d.body.u.case_2.memidx = (uint32_t)mi;
             d.body.u.case_2.offset = off; d.body.u.case_2.data = bytes; }
         bbq_vec_push(*datas, d);
    return true;
}

static bool wat_parse_elem_expr(peg_state* p, jav_expr_t* out) {
    bbq_write_ctx_t* w = wat_wbuf_open(CTX);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "item", 4)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_instr(p, w)) break;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!wat_parse_folded_instr(p, w)) return false;
    _choice_done0:;
    }
    bbq_write_u8(w, 0x0B);
         if (CTX->pass != 2) { wat_wbuf_close(CTX, w); return true; }
         bbq_ctx_t rc; bbq_ctx_init(&rc, w->data, w->pos);   /* decode the scratch -> tree */
         int ok = jav_expr_read(&rc, out);                  /* tree owns byte copies */
         bbq_ctx_free(&rc);
         wat_wbuf_close(CTX, w);
         if (!ok) return false;
    return true;
}

static bool wat_parse_elem_list(peg_state* p) {
    CTX->el_is_funcidx = 0; CTX->el_funcs = NULL; CTX->el_exprs = NULL;
       memset(&CTX->el_rtv, 0, sizeof CTX->el_rtv);
       peg_span fr = {0,0}; jav_ref_type_t rtv; memset(&rtv, 0, sizeof rtv); jav_expr_t ie;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "func", 4)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_idx_ref(p, &fr)) break;
                        int64_t x = wat_resolve(CTX, SP_FUNC, fr); if (x < 0) return false;
               uint32_t xx = (uint32_t)x; bbq_vec_push(CTX->el_funcs, xx);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                CTX->el_is_funcidx = 1;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!wat_parse_ref_type_val(p, &rtv)) break;
                for (;;) {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_elem_expr(p, &ie)) break;
                        bbq_vec_push(CTX->el_exprs, ie);
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) { peg_restore(p, _m5); break; }
                }
                CTX->el_rtv = rtv;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!wat_parse_elem_expr(p, &ie)) break;
                bbq_vec_push(CTX->el_exprs, ie);
                for (;;) {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_elem_expr(p, &ie)) break;
                        bbq_vec_push(CTX->el_exprs, ie);
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) { peg_restore(p, _m7); break; }
                }
                CTX->el_is_funcidx = 0; CTX->el_rtv.head = 0x70;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        for (;;) {
            peg_mark _m8 = peg_save(p);
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &fr)) break;
                int64_t x = wat_resolve(CTX, SP_FUNC, fr); if (x < 0) return false;
               uint32_t xx = (uint32_t)x; bbq_vec_push(CTX->el_funcs, xx);
                _ok8 = true;
            } while(0);
            if (!_ok8) { peg_restore(p, _m8); break; }
        }
        CTX->el_is_funcidx = 1; CTX->el_rtv.head = 0x70;
    _choice_done0:;
    }
    return true;
}

static bool wat_parse_element_field(peg_state* p, jav_elem_t** elems) {
    jav_elem_t el; memset(&el, 0, sizeof el); peg_span idsp = {0,0}, tref = {0,0};
       int is_declare = 0, has_table = 0, has_off = 0;
       jav_expr_t off; memset(&off, 0, sizeof off);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "elem", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!wat_id(p, &idsp)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    if (CTX->pass == 1 && !wat_id_add(CTX, SP_ELEM, idsp)) return false;
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "declare", 7)) break;
            {
                peg_mark _m3 = peg_save(p);
                bool _ok3 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok3 = true;
                } while(0);
                peg_restore(p, _m3);
                if (_ok3) break;
            }
            is_declare = 1;
            _ok2 = true;
        } while(0);
        if (!_ok2) peg_restore(p, _m2);
    }
    {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "(", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "table", 5)) break;
            {
                peg_mark _m5 = peg_save(p);
                bool _ok5 = false;
                do {
                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok5 = true;
                } while(0);
                peg_restore(p, _m5);
                if (_ok5) break;
            }
            peg_skip(p);
            if (!wat_parse_idx_ref(p, &tref)) break;
            peg_skip(p);
            if (!peg_match_n(p, ")", 1)) break;
            has_table = 1;
            _ok4 = true;
        } while(0);
        if (!_ok4) peg_restore(p, _m4);
    }
    {
        peg_mark _m6 = peg_save(p);
        bool _ok6 = false;
        do {
            peg_skip(p);
            if (!wat_parse_offset_clause(p, &off)) break;
            has_off = 1;
            _ok6 = true;
        } while(0);
        if (!_ok6) peg_restore(p, _m6);
    }
    peg_skip(p);
    if (!wat_parse_elem_list(p)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { bbq_vec_free(CTX->el_funcs); bbq_vec_free(CTX->el_exprs); return true; }
         int is_funcidx = CTX->el_is_funcidx; jav_ref_type_t rtv = CTX->el_rtv;
         jav_idx_vec_t fv; memset(&fv, 0, sizeof fv);
         jav_expr_vec_t ev; memset(&ev, 0, sizeof ev);
         if (is_funcidx) { WAT_FREEZE(CTX->el_funcs, fv.idxs); fv.count = (uint32_t)fv.idxs.count; bbq_vec_free(CTX->el_exprs); }
         else            { WAT_FREEZE(CTX->el_exprs, ev.exprs); ev.count = (uint32_t)ev.exprs.count; bbq_vec_free(CTX->el_funcs); }
         uint32_t tbl = 0;
         if (has_table) { int64_t t = wat_resolve(CTX, SP_TABLE, tref); if (t < 0) return false; tbl = (uint32_t)t; }
         int ff = (rtv.head == 0x70 && !rtv.ht.has_value);    /* element type is funcref */
         if (is_declare) {
             if (is_funcidx) { el.flag = 3; el.body.tag = 3; el.body.u.case_3.elemkind = 0; el.body.u.case_3.funcs = fv; }
             else            { el.flag = 7; el.body.tag = 7; el.body.u.case_7.reftype = rtv; el.body.u.case_7.exprs = ev; }
         } else if (has_off) {                                /* active */
             if (is_funcidx) {
                 if (has_table) { el.flag = 2; el.body.tag = 2; el.body.u.case_2.table = tbl;
                                  el.body.u.case_2.offset = off; el.body.u.case_2.elemkind = 0; el.body.u.case_2.funcs = fv; }
                 else           { el.flag = 0; el.body.tag = 0; el.body.u.case_0.offset = off; el.body.u.case_0.funcs = fv; }
             } else if (!has_table && ff) { el.flag = 4; el.body.tag = 4; el.body.u.case_4.offset = off; el.body.u.case_4.exprs = ev; }
             else { el.flag = 6; el.body.tag = 6; el.body.u.case_6.table = tbl; el.body.u.case_6.offset = off;
                    el.body.u.case_6.reftype = rtv; el.body.u.case_6.exprs = ev; }
         } else {                                             /* passive */
             if (is_funcidx) { el.flag = 1; el.body.tag = 1; el.body.u.case_1.elemkind = 0; el.body.u.case_1.funcs = fv; }
             else            { el.flag = 5; el.body.tag = 5; el.body.u.case_5.reftype = rtv; el.body.u.case_5.exprs = ev; }
         }
         bbq_vec_push(*elems, el);
    return true;
}

static bool wat_parse_import_field(peg_state* p) {
    jav_import_t im; memset(&im, 0, sizeof im);
       peg_span idsp = {0,0}, tref = {0,0}; int has_ref = 0, kind = -1;
       jav_limits_t lim; memset(&lim, 0, sizeof lim);
       jav_ref_type_t rtv; memset(&rtv, 0, sizeof rtv);
       jav_val_type_t gvt; memset(&gvt, 0, sizeof gvt); uint8_t gmut = 0;
       CTX->bind_locals = 0;
       bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS);
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "import", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    peg_skip(p);
    if (!wat_parse_name(p, &im.module)) return false;
    peg_skip(p);
    if (!wat_parse_name(p, &im.field)) return false;
    if (CTX->pass == 2 && CTX->defs_seen) {          /* §6.6: imports precede definitions */
             free((void*)im.module.bytes.data); free((void*)im.field.bytes.data); return false; }
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "func", 4)) break;
                {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    peg_restore(p, _m3);
                    if (_ok3) break;
                }
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &idsp)) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) peg_restore(p, _m4);
                }
                kind = 0; if (CTX->pass == 1 && !wat_imp_add(CTX, SP_FUNC, idsp)) return false;
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "type", 4)) break;
                        {
                            peg_mark _m6 = peg_save(p);
                            bool _ok6 = false;
                            do {
                                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok6 = true;
                            } while(0);
                            peg_restore(p, _m6);
                            if (_ok6) break;
                        }
                        peg_skip(p);
                        if (!wat_parse_idx_ref(p, &tref)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        has_ref = 1;
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) peg_restore(p, _m5);
                }
                for (;;) {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_param(p, &SC_F_PARAMS)) break;
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) { peg_restore(p, _m7); break; }
                }
                for (;;) {
                    peg_mark _m8 = peg_save(p);
                    bool _ok8 = false;
                    do {
                        peg_skip(p);
                        if (!wat_parse_result(p, &SC_F_RESULTS)) break;
                        _ok8 = true;
                    } while(0);
                    if (!_ok8) { peg_restore(p, _m8); break; }
                }
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "table", 5)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                {
                    peg_mark _m11 = peg_save(p);
                    bool _ok11 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &idsp)) break;
                        _ok11 = true;
                    } while(0);
                    if (!_ok11) peg_restore(p, _m11);
                }
                kind = 1; if (CTX->pass == 1 && !wat_imp_add(CTX, SP_TABLE, idsp)) return false;
                peg_skip(p);
                if (!wat_parse_limits(p, &lim)) break;
                peg_skip(p);
                if (!wat_parse_ref_type_val(p, &rtv)) break;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok12 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "memory", 6)) break;
                {
                    peg_mark _m13 = peg_save(p);
                    bool _ok13 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok13 = true;
                    } while(0);
                    peg_restore(p, _m13);
                    if (_ok13) break;
                }
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &idsp)) break;
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) peg_restore(p, _m14);
                }
                kind = 2; if (CTX->pass == 1 && !wat_imp_add(CTX, SP_MEM, idsp)) return false;
                peg_skip(p);
                if (!wat_parse_limits(p, &lim)) break;
                _ok12 = true;
            } while(0);
            if (!_ok12) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        {
            bool _ok15 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "global", 6)) break;
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok16 = true;
                    } while(0);
                    peg_restore(p, _m16);
                    if (_ok16) break;
                }
                {
                    peg_mark _m17 = peg_save(p);
                    bool _ok17 = false;
                    do {
                        peg_skip(p);
                        if (!wat_id(p, &idsp)) break;
                        _ok17 = true;
                    } while(0);
                    if (!_ok17) peg_restore(p, _m17);
                }
                kind = 3; if (CTX->pass == 1 && !wat_imp_add(CTX, SP_GLOBAL, idsp)) return false;
                {
                    peg_mark _m18 = peg_save(p);
                    {
                        bool _ok19 = false;
                        do {
                            peg_skip(p);
                            if (!peg_match_n(p, "(", 1)) break;
                            peg_skip(p);
                            if (!peg_match_n(p, "mut", 3)) break;
                            {
                                peg_mark _m20 = peg_save(p);
                                bool _ok20 = false;
                                do {
                                    if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok20 = true;
                                } while(0);
                                peg_restore(p, _m20);
                                if (_ok20) break;
                            }
                            peg_skip(p);
                            if (!wat_parse_val_type(p, &gvt)) break;
                            peg_skip(p);
                            if (!peg_match_n(p, ")", 1)) break;
                            gmut = 1;
                            _ok19 = true;
                        } while(0);
                        if (!_ok19) {
                            peg_restore(p, _m18);
                        } else goto _choice_done18;
                    }
                    peg_skip(p);
                    if (!wat_parse_val_type(p, &gvt)) break;
                    gmut = 0;
                _choice_done18:;
                }
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        peg_skip(p);
        if (!peg_match_n(p, "tag", 3)) return false;
        {
            peg_mark _m21 = peg_save(p);
            bool _ok21 = false;
            do {
                if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                peg_advance(p);
                _ok21 = true;
            } while(0);
            peg_restore(p, _m21);
            if (_ok21) return false;
        }
        {
            peg_mark _m22 = peg_save(p);
            bool _ok22 = false;
            do {
                peg_skip(p);
                if (!wat_id(p, &idsp)) break;
                _ok22 = true;
            } while(0);
            if (!_ok22) peg_restore(p, _m22);
        }
        kind = 4; if (CTX->pass == 1 && !wat_imp_add(CTX, SP_TAG, idsp)) return false;
        {
            peg_mark _m23 = peg_save(p);
            bool _ok23 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!peg_match_n(p, "type", 4)) break;
                {
                    peg_mark _m24 = peg_save(p);
                    bool _ok24 = false;
                    do {
                        if (peg_at_end(p) || !is_glue(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok24 = true;
                    } while(0);
                    peg_restore(p, _m24);
                    if (_ok24) break;
                }
                peg_skip(p);
                if (!wat_parse_idx_ref(p, &tref)) break;
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                has_ref = 1;
                _ok23 = true;
            } while(0);
            if (!_ok23) peg_restore(p, _m23);
        }
        for (;;) {
            peg_mark _m25 = peg_save(p);
            bool _ok25 = false;
            do {
                peg_skip(p);
                if (!wat_parse_param(p, &SC_F_PARAMS)) break;
                _ok25 = true;
            } while(0);
            if (!_ok25) { peg_restore(p, _m25); break; }
        }
        for (;;) {
            peg_mark _m26 = peg_save(p);
            bool _ok26 = false;
            do {
                peg_skip(p);
                if (!wat_parse_result(p, &SC_F_RESULTS)) break;
                _ok26 = true;
            } while(0);
            if (!_ok26) { peg_restore(p, _m26); break; }
        }
    _choice_done1:;
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    if (CTX->pass != 2) { free((void*)im.module.bytes.data); free((void*)im.field.bytes.data);
                               bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS); return true; }
         im.desc.kind = (uint8_t)kind; im.desc.body.tag = kind;
         if (kind == 0 || kind == 4) {                   /* func/tag: a typeidx via typeuse */
             uint32_t tidx;
             if (has_ref) { int64_t r = wat_resolve(CTX, SP_TYPE, tref);
                            if (r < 0 || !wat_typeuse_ref(CTX, r, &SC_F_PARAMS, &SC_F_RESULTS)) {
                                bbq_vec_free(SC_F_PARAMS); bbq_vec_free(SC_F_RESULTS); return false;
                            }
                            tidx = (uint32_t)r; }
             else { jav_func_type_t ft; memset(&ft, 0, sizeof ft);
                    WAT_FREEZE(SC_F_PARAMS, ft.params);  ft.param_count  = (uint32_t)ft.params.count;
                    WAT_FREEZE(SC_F_RESULTS, ft.results); ft.result_count = (uint32_t)ft.results.count;
                    tidx = wat_typeuse(CTX, &ft); }
             if (kind == 0) im.desc.body.u.case_0.x = tidx;
             else { im.desc.body.u.case_4.attr = 0; im.desc.body.u.case_4.type = tidx; }
         } else if (kind == 1) { im.desc.body.u.case_1.reftype = rtv; im.desc.body.u.case_1.limits = lim; }
         else if (kind == 2) { im.desc.body.u.case_2 = lim; }
         else { im.desc.body.u.case_3.type = gvt; im.desc.body.u.case_3.mut = gmut; }
         wat_inline_import(CTX, im.module, im.field, im.desc, wat_import_space(im.desc.kind));
    return true;
}

