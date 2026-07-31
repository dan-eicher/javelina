#include "java_parser.h"



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

/* A STRUCTURED skip element (e.g. `(@id … )` annotations): from `open`, walk to the matching
 * close counting GENERIC ()-nesting, treating "…" string literals (with \-escapes) as opaque so
 * their parens don't perturb the balance. The `close` delimiter is unused (paren depth ends it).
 * Language-specific awareness (inner line/block comments, identifier validation) is NOT generic —
 * a grammar that needs it overrides this in its OWN frame copy (see -frames). Always succeeds. */
PEG_INTERNAL bool peg_skip_structured(peg_state* p, const peg_comment_spec* spec) {
    int open_len = spec->open_len;
    if (p->pos + open_len > p->end) return false;
    if (memcmp(p->pos, spec->open, (size_t)open_len) != 0) return false;
    for (int i = 0; i < open_len; i++) peg_advance(p);
    int depth = 1;
    while (p->pos < p->end && depth > 0) {
        char c = *p->pos;
        if (c == '"') {                                  /* string literal: skip with \-escapes */
            peg_advance(p);
            while (p->pos < p->end && *p->pos != '"') {
                if (*p->pos == '\\' && p->pos + 1 < p->end) peg_advance(p);
                peg_advance(p);
            }
            if (p->pos < p->end) peg_advance(p);
        } else if (c == '(') { depth++; peg_advance(p); }
        else if (c == ')') { depth--; peg_advance(p); }
        else peg_advance(p);
    }
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

static bool is_letter(char c) {
    return ((((c >= 97 && c <= 122) || (c >= 65 && c <= 90)) || (c == 95)) || (c == 36));
}

static bool is_digit(char c) {
    return (c >= 48 && c <= 57);
}

static bool is_nonzero_digit(char c) {
    return (c >= 49 && c <= 57);
}

static bool is_octal_digit(char c) {
    return (c >= 48 && c <= 55);
}

static bool is_hex_digit(char c) {
    return ((is_digit(c) || (c >= 97 && c <= 102)) || (c >= 65 && c <= 70));
}

static bool is_utf8_tail(char c) {
    return (c >= -128 && c <= -65);
}

static bool is_utf8_lead(char c) {
    return (c >= -64 && c <= -9);
}

static bool is_letter_or_digit(char c) {
    return (is_letter(c) || is_digit(c));
}

static bool is_float_suffix(char c) {
    return ((((c == 102) || (c == 70)) || (c == 100)) || (c == 68));
}

static bool is_sign_char(char c) {
    return ((c == 43) || (c == 45));
}

static bool java_ws_predicate(char c) {
    return (((((c == 13) || (c == 10)) || (c == 9)) || (c == 32)) || (c == 12));
}

static void java_setup_skip(peg_state* p) {
    peg_set_whitespace(p, java_ws_predicate);
    peg_add_comment(p, "//", "\n", false, false);
    peg_add_comment(p, "/*", "*/", false, false);
}

static bool java_keyword(peg_state* p, peg_span* out);
static bool java_ident_raw(peg_state* p, peg_span* out);
static bool java_integer(peg_state* p, peg_span* out);
static bool java_long_lit(peg_state* p, peg_span* out);
static bool java_float_lit(peg_state* p, peg_span* out);
static bool java_char_lit(peg_state* p, peg_span* out);
static bool java_string_lit(peg_state* p, peg_span* out);
static bool java_kw_abstract(peg_state* p, peg_span* out);
static bool java_kw_boolean(peg_state* p, peg_span* out);
static bool java_kw_break(peg_state* p, peg_span* out);
static bool java_kw_byte(peg_state* p, peg_span* out);
static bool java_kw_case(peg_state* p, peg_span* out);
static bool java_kw_catch(peg_state* p, peg_span* out);
static bool java_kw_char(peg_state* p, peg_span* out);
static bool java_kw_class(peg_state* p, peg_span* out);
static bool java_kw_continue(peg_state* p, peg_span* out);
static bool java_kw_default(peg_state* p, peg_span* out);
static bool java_kw_do(peg_state* p, peg_span* out);
static bool java_kw_double(peg_state* p, peg_span* out);
static bool java_kw_else(peg_state* p, peg_span* out);
static bool java_kw_extends(peg_state* p, peg_span* out);
static bool java_kw_false(peg_state* p, peg_span* out);
static bool java_kw_final(peg_state* p, peg_span* out);
static bool java_kw_finally(peg_state* p, peg_span* out);
static bool java_kw_float(peg_state* p, peg_span* out);
static bool java_kw_for(peg_state* p, peg_span* out);
static bool java_kw_if(peg_state* p, peg_span* out);
static bool java_kw_implements(peg_state* p, peg_span* out);
static bool java_kw_import(peg_state* p, peg_span* out);
static bool java_kw_instanceof(peg_state* p, peg_span* out);
static bool java_kw_int(peg_state* p, peg_span* out);
static bool java_kw_interface(peg_state* p, peg_span* out);
static bool java_kw_long(peg_state* p, peg_span* out);
static bool java_kw_native(peg_state* p, peg_span* out);
static bool java_kw_new(peg_state* p, peg_span* out);
static bool java_kw_null(peg_state* p, peg_span* out);
static bool java_kw_package(peg_state* p, peg_span* out);
static bool java_kw_private(peg_state* p, peg_span* out);
static bool java_kw_protected(peg_state* p, peg_span* out);
static bool java_kw_public(peg_state* p, peg_span* out);
static bool java_kw_return(peg_state* p, peg_span* out);
static bool java_kw_short(peg_state* p, peg_span* out);
static bool java_kw_static(peg_state* p, peg_span* out);
static bool java_kw_super(peg_state* p, peg_span* out);
static bool java_kw_switch(peg_state* p, peg_span* out);
static bool java_kw_synchronized(peg_state* p, peg_span* out);
static bool java_kw_this(peg_state* p, peg_span* out);
static bool java_kw_throw(peg_state* p, peg_span* out);
static bool java_kw_throws(peg_state* p, peg_span* out);
static bool java_kw_transient(peg_state* p, peg_span* out);
static bool java_kw_true(peg_state* p, peg_span* out);
static bool java_kw_try(peg_state* p, peg_span* out);
static bool java_kw_void(peg_state* p, peg_span* out);
static bool java_kw_volatile(peg_state* p, peg_span* out);
static bool java_kw_while(peg_state* p, peg_span* out);
static bool java_parse_java(peg_state* p);
static bool java_parse_ident(peg_state* p, peg_span* out);
static bool java_parse_import_decl(peg_state* p, ast_import_t** result);
static bool java_parse_type_decl_(peg_state* p, ast_type_decl_t** result);
static bool java_parse_class_decl_(peg_state* p, ast_type_decl_t** result, ast_modifier_t* mods, int mc);
static bool java_parse_interface_decl_(peg_state* p, ast_type_decl_t** result, ast_modifier_t* mods, int mc);
static bool java_parse_name_list(peg_state* p, ast_name_t*** list, int* count);
static bool java_parse_class_member(peg_state* p, ast_member_t*** list, int* count);
static bool java_parse_interface_member(peg_state* p, ast_member_t*** list, int* count);
static bool java_parse_member_decl(peg_state* p, ast_member_t** result);
static bool java_parse_static_init_decl(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc);
static bool java_parse_constructor_or_method_or_field(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc);
static bool java_parse_constructor_decl(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc);
static bool java_parse_field_rest(peg_state* p, ast_member_t** result, ast_type_t* ty, peg_span ns, ast_modifier_t* mods, int mc);
static bool java_parse_var_decl_item(peg_state* p, ast_var_decl_t*** decls, int* dc);
static bool java_parse_var_init(peg_state* p, ast_expr_t** result);
static bool java_parse_array_init_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_method_rest(peg_state* p, ast_member_t** result, ast_type_t* ty, peg_span ns, ast_modifier_t* mods, int mc);
static bool java_parse_formal_param_list(peg_state* p, ast_param_t*** params, int* pc);
static bool java_parse_formal_param(peg_state* p, ast_param_t** result);
static bool java_parse_type_ref(peg_state* p, ast_type_t** result);
static bool java_parse_base_type(peg_state* p, ast_type_t** result);
static bool java_parse_qual_name(peg_state* p, ast_name_t** result);
static bool java_parse_modifier_(peg_state* p, ast_modifier_t** mods, int* count);
static bool java_parse_block(peg_state* p, ast_stmt_t** result);
static bool java_parse_block_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_local_var_decl_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_statement(peg_state* p, ast_stmt_t** result);
static bool java_parse_if_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_while_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_do_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_for_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_for_init(peg_state* p, ast_stmt_t** result);
static bool java_parse_local_var_decl_stmt_no_semi(peg_state* p, ast_stmt_t** result);
static bool java_parse_for_init_expr(peg_state* p, ast_stmt_t** result);
static bool java_parse_for_update(peg_state* p, ast_expr_t*** list, int* count);
static bool java_parse_switch_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_switch_group(peg_state* p, ast_switch_case_t** result);
static bool java_parse_try_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_catch_clause_(peg_state* p, ast_catch_clause_t** result);
static bool java_parse_return_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_throw_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_labeled_or_expr_stmt(peg_state* p, ast_stmt_t** result);
static bool java_parse_expr_(peg_state* p, ast_expr_t** result);
static bool java_parse_assign_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_compound_assign_op(peg_state* p, ast_binop_t* op);
static bool java_parse_cond_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_cond_or_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_cond_and_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_inc_or_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_xor_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_and_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_equal_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_rel_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_shift_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_add_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_mult_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_unary_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_unary_expr_npm(peg_state* p, ast_expr_t** result);
static bool java_parse_prim_type(peg_state* p, ast_type_t** result);
static bool java_parse_postfix_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_primary_expr(peg_state* p, ast_expr_t** result);
static bool java_parse_arg_list(peg_state* p, ast_expr_t*** args, int* count);


void java_parser_init(peg_state* p, const char* input, int length) {
    peg_init(p, input, length);
    java_setup_skip(p);
}

bool java_parser_parse(peg_state* p) {
    peg_skip(p);
    return java_parse_java(p);
}


static bool java_keyword(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "abstract", 8)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                if (!peg_match_n(p, "boolean", 7)) break;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                if (!peg_match_n(p, "break", 5)) break;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                if (!peg_match_n(p, "byte", 4)) break;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                if (!peg_match_n(p, "case", 4)) break;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                if (!peg_match_n(p, "catch", 5)) break;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                if (!peg_match_n(p, "char", 4)) break;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                if (!peg_match_n(p, "class", 5)) break;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                if (!peg_match_n(p, "const", 5)) break;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok10 = false;
            do {
                if (!peg_match_n(p, "continue", 8)) break;
                _ok10 = true;
            } while(0);
            if (!_ok10) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok11 = false;
            do {
                if (!peg_match_n(p, "default", 7)) break;
                _ok11 = true;
            } while(0);
            if (!_ok11) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok12 = false;
            do {
                if (!peg_match_n(p, "double", 6)) break;
                _ok12 = true;
            } while(0);
            if (!_ok12) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok13 = false;
            do {
                if (!peg_match_n(p, "do", 2)) break;
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok14 = false;
            do {
                if (!peg_match_n(p, "else", 4)) break;
                _ok14 = true;
            } while(0);
            if (!_ok14) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok15 = false;
            do {
                if (!peg_match_n(p, "extends", 7)) break;
                _ok15 = true;
            } while(0);
            if (!_ok15) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok16 = false;
            do {
                if (!peg_match_n(p, "false", 5)) break;
                _ok16 = true;
            } while(0);
            if (!_ok16) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok17 = false;
            do {
                if (!peg_match_n(p, "finally", 7)) break;
                _ok17 = true;
            } while(0);
            if (!_ok17) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok18 = false;
            do {
                if (!peg_match_n(p, "final", 5)) break;
                _ok18 = true;
            } while(0);
            if (!_ok18) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok19 = false;
            do {
                if (!peg_match_n(p, "float", 5)) break;
                _ok19 = true;
            } while(0);
            if (!_ok19) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok20 = false;
            do {
                if (!peg_match_n(p, "for", 3)) break;
                _ok20 = true;
            } while(0);
            if (!_ok20) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok21 = false;
            do {
                if (!peg_match_n(p, "goto", 4)) break;
                _ok21 = true;
            } while(0);
            if (!_ok21) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok22 = false;
            do {
                if (!peg_match_n(p, "if", 2)) break;
                _ok22 = true;
            } while(0);
            if (!_ok22) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok23 = false;
            do {
                if (!peg_match_n(p, "implements", 10)) break;
                _ok23 = true;
            } while(0);
            if (!_ok23) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok24 = false;
            do {
                if (!peg_match_n(p, "import", 6)) break;
                _ok24 = true;
            } while(0);
            if (!_ok24) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok25 = false;
            do {
                if (!peg_match_n(p, "instanceof", 10)) break;
                _ok25 = true;
            } while(0);
            if (!_ok25) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok26 = false;
            do {
                if (!peg_match_n(p, "interface", 9)) break;
                _ok26 = true;
            } while(0);
            if (!_ok26) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok27 = false;
            do {
                if (!peg_match_n(p, "int", 3)) break;
                _ok27 = true;
            } while(0);
            if (!_ok27) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok28 = false;
            do {
                if (!peg_match_n(p, "long", 4)) break;
                _ok28 = true;
            } while(0);
            if (!_ok28) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok29 = false;
            do {
                if (!peg_match_n(p, "native", 6)) break;
                _ok29 = true;
            } while(0);
            if (!_ok29) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok30 = false;
            do {
                if (!peg_match_n(p, "new", 3)) break;
                _ok30 = true;
            } while(0);
            if (!_ok30) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok31 = false;
            do {
                if (!peg_match_n(p, "null", 4)) break;
                _ok31 = true;
            } while(0);
            if (!_ok31) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok32 = false;
            do {
                if (!peg_match_n(p, "package", 7)) break;
                _ok32 = true;
            } while(0);
            if (!_ok32) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok33 = false;
            do {
                if (!peg_match_n(p, "private", 7)) break;
                _ok33 = true;
            } while(0);
            if (!_ok33) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok34 = false;
            do {
                if (!peg_match_n(p, "protected", 9)) break;
                _ok34 = true;
            } while(0);
            if (!_ok34) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok35 = false;
            do {
                if (!peg_match_n(p, "public", 6)) break;
                _ok35 = true;
            } while(0);
            if (!_ok35) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok36 = false;
            do {
                if (!peg_match_n(p, "return", 6)) break;
                _ok36 = true;
            } while(0);
            if (!_ok36) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok37 = false;
            do {
                if (!peg_match_n(p, "short", 5)) break;
                _ok37 = true;
            } while(0);
            if (!_ok37) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok38 = false;
            do {
                if (!peg_match_n(p, "static", 6)) break;
                _ok38 = true;
            } while(0);
            if (!_ok38) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok39 = false;
            do {
                if (!peg_match_n(p, "super", 5)) break;
                _ok39 = true;
            } while(0);
            if (!_ok39) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok40 = false;
            do {
                if (!peg_match_n(p, "switch", 6)) break;
                _ok40 = true;
            } while(0);
            if (!_ok40) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok41 = false;
            do {
                if (!peg_match_n(p, "synchronized", 12)) break;
                _ok41 = true;
            } while(0);
            if (!_ok41) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok42 = false;
            do {
                if (!peg_match_n(p, "this", 4)) break;
                _ok42 = true;
            } while(0);
            if (!_ok42) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok43 = false;
            do {
                if (!peg_match_n(p, "throws", 6)) break;
                _ok43 = true;
            } while(0);
            if (!_ok43) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok44 = false;
            do {
                if (!peg_match_n(p, "throw", 5)) break;
                _ok44 = true;
            } while(0);
            if (!_ok44) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok45 = false;
            do {
                if (!peg_match_n(p, "transient", 9)) break;
                _ok45 = true;
            } while(0);
            if (!_ok45) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok46 = false;
            do {
                if (!peg_match_n(p, "true", 4)) break;
                _ok46 = true;
            } while(0);
            if (!_ok46) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok47 = false;
            do {
                if (!peg_match_n(p, "try", 3)) break;
                _ok47 = true;
            } while(0);
            if (!_ok47) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok48 = false;
            do {
                if (!peg_match_n(p, "void", 4)) break;
                _ok48 = true;
            } while(0);
            if (!_ok48) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok49 = false;
            do {
                if (!peg_match_n(p, "volatile", 8)) break;
                _ok49 = true;
            } while(0);
            if (!_ok49) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (!peg_match_n(p, "while", 5)) return false;
    _choice_done0:;
    }
    {
        peg_mark _m50 = peg_save(p);
        bool _ok50 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok50 = true;
        } while(0);
        peg_restore(p, _m50);
        if (_ok50) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_ident_raw(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (!java_keyword(p, NULL)) break;
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
                if (peg_at_end(p) || !is_letter(peg_peek_char(p))) break;
                peg_advance(p);
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        if (peg_at_end(p) || !is_utf8_lead(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m3 = peg_save(p);
            bool _ok3 = false;
            do {
                if (peg_at_end(p) || !is_utf8_tail(peg_peek_char(p))) break;
                peg_advance(p);
                _ok3 = true;
            } while(0);
            if (!_ok3) { peg_restore(p, _m3); break; }
        }
    _choice_done1:;
    }
    for (;;) {
        peg_mark _m4 = peg_save(p);
        bool _ok4 = false;
        do {
            {
                peg_mark _m5 = peg_save(p);
                {
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) {
                        peg_restore(p, _m5);
                    } else goto _choice_done5;
                }
                if (peg_at_end(p) || !is_utf8_lead(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        if (peg_at_end(p) || !is_utf8_tail(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) { peg_restore(p, _m7); break; }
                }
            _choice_done5:;
            }
            _ok4 = true;
        } while(0);
        if (!_ok4) { peg_restore(p, _m4); break; }
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_integer(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "0", 1)) break;
                if (peg_peek_at_n(p, "x", 1)) {
                    if (!peg_match_n(p, "x", 1)) break;
                } else {
                    if (!peg_match_n(p, "X", 1)) break;
                }
                if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                if (peg_at_end(p) || !is_nonzero_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) { peg_restore(p, _m4); break; }
                }
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (!peg_match_n(p, "0", 1)) return false;
        for (;;) {
            peg_mark _m5 = peg_save(p);
            bool _ok5 = false;
            do {
                if (peg_at_end(p) || !is_octal_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok5 = true;
            } while(0);
            if (!_ok5) { peg_restore(p, _m5); break; }
        }
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_long_lit(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "0", 1)) break;
                if (peg_peek_at_n(p, "x", 1)) {
                    if (!peg_match_n(p, "x", 1)) break;
                } else {
                    if (!peg_match_n(p, "X", 1)) break;
                }
                if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                if (peg_at_end(p) || !is_nonzero_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) { peg_restore(p, _m4); break; }
                }
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (!peg_match_n(p, "0", 1)) return false;
        for (;;) {
            peg_mark _m5 = peg_save(p);
            bool _ok5 = false;
            do {
                if (peg_at_end(p) || !is_octal_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok5 = true;
            } while(0);
            if (!_ok5) { peg_restore(p, _m5); break; }
        }
    _choice_done0:;
    }
    if (peg_peek_at_n(p, "l", 1)) {
        if (!peg_match_n(p, "l", 1)) return false;
    } else {
        if (!peg_match_n(p, "L", 1)) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_float_lit(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                if (!peg_match_n(p, ".", 1)) break;
                for (;;) {
                    peg_mark _m3 = peg_save(p);
                    bool _ok3 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) { peg_restore(p, _m3); break; }
                }
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_peek_at_n(p, "e", 1)) {
                            if (!peg_match_n(p, "e", 1)) break;
                        } else {
                            if (!peg_match_n(p, "E", 1)) break;
                        }
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                if (peg_at_end(p) || !is_sign_char(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok5 = true;
                            } while(0);
                            if (!_ok5) peg_restore(p, _m5);
                        }
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        for (;;) {
                            peg_mark _m6 = peg_save(p);
                            bool _ok6 = false;
                            do {
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok6 = true;
                            } while(0);
                            if (!_ok6) { peg_restore(p, _m6); break; }
                        }
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) peg_restore(p, _m4);
                }
                {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        if (peg_at_end(p) || !is_float_suffix(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) peg_restore(p, _m7);
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                if (!peg_match_n(p, ".", 1)) break;
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok9 = true;
                    } while(0);
                    if (!_ok9) { peg_restore(p, _m9); break; }
                }
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_peek_at_n(p, "e", 1)) {
                            if (!peg_match_n(p, "e", 1)) break;
                        } else {
                            if (!peg_match_n(p, "E", 1)) break;
                        }
                        {
                            peg_mark _m11 = peg_save(p);
                            bool _ok11 = false;
                            do {
                                if (peg_at_end(p) || !is_sign_char(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok11 = true;
                            } while(0);
                            if (!_ok11) peg_restore(p, _m11);
                        }
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        for (;;) {
                            peg_mark _m12 = peg_save(p);
                            bool _ok12 = false;
                            do {
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok12 = true;
                            } while(0);
                            if (!_ok12) { peg_restore(p, _m12); break; }
                        }
                        _ok10 = true;
                    } while(0);
                    if (!_ok10) peg_restore(p, _m10);
                }
                {
                    peg_mark _m13 = peg_save(p);
                    bool _ok13 = false;
                    do {
                        if (peg_at_end(p) || !is_float_suffix(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok13 = true;
                    } while(0);
                    if (!_ok13) peg_restore(p, _m13);
                }
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok14 = false;
            do {
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m15 = peg_save(p);
                    bool _ok15 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok15 = true;
                    } while(0);
                    if (!_ok15) { peg_restore(p, _m15); break; }
                }
                if (peg_peek_at_n(p, "e", 1)) {
                    if (!peg_match_n(p, "e", 1)) break;
                } else {
                    if (!peg_match_n(p, "E", 1)) break;
                }
                {
                    peg_mark _m16 = peg_save(p);
                    bool _ok16 = false;
                    do {
                        if (peg_at_end(p) || !is_sign_char(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok16 = true;
                    } while(0);
                    if (!_ok16) peg_restore(p, _m16);
                }
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m17 = peg_save(p);
                    bool _ok17 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok17 = true;
                    } while(0);
                    if (!_ok17) { peg_restore(p, _m17); break; }
                }
                {
                    peg_mark _m18 = peg_save(p);
                    bool _ok18 = false;
                    do {
                        if (peg_at_end(p) || !is_float_suffix(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok18 = true;
                    } while(0);
                    if (!_ok18) peg_restore(p, _m18);
                }
                _ok14 = true;
            } while(0);
            if (!_ok14) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m19 = peg_save(p);
            bool _ok19 = false;
            do {
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                _ok19 = true;
            } while(0);
            if (!_ok19) { peg_restore(p, _m19); break; }
        }
        if (peg_at_end(p) || !is_float_suffix(peg_peek_char(p))) return false;
        peg_advance(p);
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_char_lit(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "'", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match_n(p, "\\", 1)) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (!peg_match_n(p, "u", 1)) break;
                        _ok2 = true;
                    } while(0);
                    peg_restore(p, _m2);
                    if (_ok2) break;
                }
                {
                    peg_mark _m3 = peg_save(p);
                    {
                        bool _ok4 = false;
                        do {
                            if (peg_at_end(p) || !is_octal_digit(peg_peek_char(p))) break;
                            peg_advance(p);
                            {
                                peg_mark _m5 = peg_save(p);
                                bool _ok5 = false;
                                do {
                                    if (peg_at_end(p) || !is_octal_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    {
                                        peg_mark _m6 = peg_save(p);
                                        bool _ok6 = false;
                                        do {
                                            if (peg_at_end(p) || !is_octal_digit(peg_peek_char(p))) break;
                                            peg_advance(p);
                                            _ok6 = true;
                                        } while(0);
                                        if (!_ok6) peg_restore(p, _m6);
                                    }
                                    _ok5 = true;
                                } while(0);
                                if (!_ok5) peg_restore(p, _m5);
                            }
                            _ok4 = true;
                        } while(0);
                        if (!_ok4) {
                            peg_restore(p, _m3);
                        } else goto _choice_done3;
                    }
                    if (peg_at_end(p)) break;
                    peg_advance(p);
                _choice_done3:;
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            peg_mark _m7 = peg_save(p);
            bool _ok7 = false;
            do {
                if (!peg_match_n(p, "'", 1)) break;
                _ok7 = true;
            } while(0);
            peg_restore(p, _m7);
            if (_ok7) return false;
        }
        {
            peg_mark _m8 = peg_save(p);
            bool _ok8 = false;
            do {
                if (!peg_match_n(p, "\\", 1)) break;
                _ok8 = true;
            } while(0);
            peg_restore(p, _m8);
            if (_ok8) return false;
        }
        {
            peg_mark _m9 = peg_save(p);
            bool _ok9 = false;
            do {
                if (!peg_match_n(p, "\r", 1)) break;
                _ok9 = true;
            } while(0);
            peg_restore(p, _m9);
            if (_ok9) return false;
        }
        {
            peg_mark _m10 = peg_save(p);
            bool _ok10 = false;
            do {
                if (!peg_match_n(p, "\n", 1)) break;
                _ok10 = true;
            } while(0);
            peg_restore(p, _m10);
            if (_ok10) return false;
        }
        if (peg_at_end(p)) return false;
        peg_advance(p);
        for (;;) {
            peg_mark _m11 = peg_save(p);
            bool _ok11 = false;
            do {
                if (peg_at_end(p) || !is_utf8_tail(peg_peek_char(p))) break;
                peg_advance(p);
                _ok11 = true;
            } while(0);
            if (!_ok11) { peg_restore(p, _m11); break; }
        }
    _choice_done0:;
    }
    if (!peg_match_n(p, "'", 1)) return false;
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_string_lit(peg_state* p, peg_span* out) {
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
                        {
                            peg_mark _m3 = peg_save(p);
                            bool _ok3 = false;
                            do {
                                if (!peg_match_n(p, "u", 1)) break;
                                _ok3 = true;
                            } while(0);
                            peg_restore(p, _m3);
                            if (_ok3) break;
                        }
                        if (peg_at_end(p)) break;
                        peg_advance(p);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (!peg_match_n(p, "\"", 1)) break;
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        if (!peg_match_n(p, "\\", 1)) break;
                        _ok5 = true;
                    } while(0);
                    peg_restore(p, _m5);
                    if (_ok5) break;
                }
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (!peg_match_n(p, "\r", 1)) break;
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                {
                    peg_mark _m7 = peg_save(p);
                    bool _ok7 = false;
                    do {
                        if (!peg_match_n(p, "\n", 1)) break;
                        _ok7 = true;
                    } while(0);
                    peg_restore(p, _m7);
                    if (_ok7) break;
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
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_abstract(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "abstract", 8)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_boolean(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "boolean", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_break(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "break", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_byte(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "byte", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_case(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "case", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_catch(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "catch", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_char(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "char", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_class(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "class", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_continue(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "continue", 8)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_default(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "default", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_do(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "do", 2)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_double(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "double", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_else(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "else", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_extends(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "extends", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_false(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "false", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_final(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "final", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_finally(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "finally", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_float(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "float", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_for(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "for", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_if(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "if", 2)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_implements(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "implements", 10)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_import(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "import", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_instanceof(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "instanceof", 10)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_int(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "int", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_interface(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "interface", 9)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_long(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "long", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_native(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "native", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_new(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "new", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_null(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "null", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_package(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "package", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_private(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "private", 7)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_protected(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "protected", 9)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_public(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "public", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_return(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "return", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_short(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "short", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_static(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "static", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_super(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "super", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_switch(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "switch", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_synchronized(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "synchronized", 12)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_this(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "this", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_throw(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "throw", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_throws(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "throws", 6)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_transient(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "transient", 9)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_true(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "true", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_try(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "try", 3)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_void(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "void", 4)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_volatile(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "volatile", 8)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_kw_while(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match_n(p, "while", 5)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_letter_or_digit(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool java_parse_java(peg_state* p) {
    ast_import_t** imports = NULL; int ic = 0;
       ast_type_decl_t** types = NULL; int tc = 0;
       ast_import_t* imp; ast_type_decl_t* td; ast_name_t* pkg = NULL;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_kw_package(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_qual_name(p, &pkg)) break;
            peg_skip(p);
            if (!peg_match_n(p, ";", 1)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_parse_import_decl(p, &imp)) break;
            imports = (ast_import_t**)jpush(A, (void**)imports, &ic, imp);
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!java_parse_type_decl_(p, &td)) break;
            types = (ast_type_decl_t**)jpush(A, (void**)types, &tc, td);
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p); if (!peg_at_end(p)) { report_parse_error(p); return false; }
    CTX->result = ast_program(A, pkg, imports, ic, types, tc);
    return true;
}

static bool java_parse_ident(peg_state* p, peg_span* out) {
    peg_skip(p);
    if (!java_ident_raw(p, out)) return false;
    if (!jident_ok(*out)) return false;
    return true;
}

static bool java_parse_import_decl(peg_state* p, ast_import_t** result) {
    const char** parts = NULL; int pc = 0;
       peg_span s; bool wildcard = false;
    peg_skip(p);
    if (!java_kw_import(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_ident(p, &s)) return false;
    parts = (const char**)jpush(A, (void**)parts, &pc, (void*)jdup(A, s));
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ".", 1)) break;
            peg_skip(p);
            if (!java_parse_ident(p, &s)) break;
            parts = (const char**)jpush(A, (void**)parts, &pc, (void*)jdup(A, s));
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ".", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "*", 1)) break;
            wildcard = true;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    if (wildcard) *result = ast_wildcard_import(A, parts, pc);
       else          *result = ast_single_import(A, parts, pc);
    return true;
}

static bool java_parse_type_decl_(peg_state* p, ast_type_decl_t** result) {
    ast_modifier_t* mods = NULL; int mc = 0;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_modifier_(p, &mods, &mc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_parse_class_decl_(p, result, mods, mc)) break;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        peg_skip(p);
        if (!java_parse_interface_decl_(p, result, mods, mc)) return false;
    _choice_done1:;
    }
    return true;
}

static bool java_parse_class_decl_(peg_state* p, ast_type_decl_t** result, ast_modifier_t* mods, int mc) {
    peg_span ns; ast_srcloc _loc;
       ast_name_t* super = NULL;
       ast_name_t** ifaces = NULL; int ifc = 0;
       ast_member_t** members = NULL; int memc = 0;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_class(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_kw_extends(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_qual_name(p, &super)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_kw_implements(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_name_list(p, &ifaces, &ifc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    peg_skip(p);
    if (!peg_match_n(p, "{", 1)) return false;
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!java_parse_class_member(p, &members, &memc)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "}", 1)) return false;
    *result = ast_class_decl(A, jdup(A, ns), super,
           ifaces, ifc, mods, mc, members, memc);
       (*result)->loc = _loc;
    return true;
}

static bool java_parse_interface_decl_(peg_state* p, ast_type_decl_t** result, ast_modifier_t* mods, int mc) {
    peg_span ns; ast_srcloc _loc;
       ast_name_t** extends = NULL; int ec = 0;
       ast_member_t** members = NULL; int memc = 0;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_interface(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_kw_extends(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_name_list(p, &extends, &ec)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!peg_match_n(p, "{", 1)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_parse_interface_member(p, &members, &memc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "}", 1)) return false;
    *result = ast_interface_decl(A, jdup(A, ns),
           extends, ec, mods, mc, members, memc);
       (*result)->loc = _loc;
    return true;
}

static bool java_parse_name_list(peg_state* p, ast_name_t*** list, int* count) {
    ast_name_t* n;
    peg_skip(p);
    if (!java_parse_qual_name(p, &n)) return false;
    *list = (ast_name_t**)jpush(A, (void**)*list, count, n);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_qual_name(p, &n)) break;
            *list = (ast_name_t**)jpush(A, (void**)*list, count, n);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_class_member(peg_state* p, ast_member_t*** list, int* count) {
    ast_member_t* m;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_member_decl(p, &m)) break;
                *list = (ast_member_t**)jpush(A, (void**)*list, count, m);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, ";", 1)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_interface_member(peg_state* p, ast_member_t*** list, int* count) {
    ast_member_t* m;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_member_decl(p, &m)) break;
                *list = (ast_member_t**)jpush(A, (void**)*list, count, m);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, ";", 1)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_member_decl(peg_state* p, ast_member_t** result) {
    ast_modifier_t* mods = NULL; int mc = 0;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_modifier_(p, &mods, &mc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_parse_static_init_decl(p, result, mods, mc)) break;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m1);
            } else goto _choice_done1;
        }
        peg_skip(p);
        if (!java_parse_constructor_or_method_or_field(p, result, mods, mc)) return false;
    _choice_done1:;
    }
    return true;
}

static bool java_parse_static_init_decl(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc) {
    ast_stmt_t* body; ast_srcloc _loc; (void)mods; (void)mc;
    _loc = LOC;
    peg_skip(p);
    if (!java_parse_block(p, &body)) return false;
    *result = ast_static_init(A, body); (*result)->loc = _loc;
    return true;
}

static bool java_parse_constructor_or_method_or_field(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc) {
    ast_type_t* ty; peg_span ns; ast_srcloc _loc;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_constructor_decl(p, result, mods, mc)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                _loc = LOC;
                peg_skip(p);
                if (!java_kw_void(p, NULL)) break;
                ty = ast_void_type(A);
                peg_skip(p);
                if (!java_parse_ident(p, &ns)) break;
                peg_skip(p);
                if (!java_parse_method_rest(p, result, ty, ns, mods, mc)) break;
                (*result)->loc = _loc;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        _loc = LOC;
        peg_skip(p);
        if (!java_parse_type_ref(p, &ty)) return false;
        peg_skip(p);
        if (!java_parse_ident(p, &ns)) return false;
        {
            peg_mark _m3 = peg_save(p);
            {
                bool _ok4 = false;
                do {
                    peg_skip(p);
                    if (!java_parse_field_rest(p, result, ty, ns, mods, mc)) break;
                    (*result)->loc = _loc;
                    _ok4 = true;
                } while(0);
                if (!_ok4) {
                    peg_restore(p, _m3);
                } else goto _choice_done3;
            }
            peg_skip(p);
            if (!java_parse_method_rest(p, result, ty, ns, mods, mc)) return false;
            (*result)->loc = _loc;
        _choice_done3:;
        }
    _choice_done0:;
    }
    return true;
}

static bool java_parse_constructor_decl(peg_state* p, ast_member_t** result, ast_modifier_t* mods, int mc) {
    peg_span ns; ast_srcloc _loc;
       ast_param_t** params = NULL; int pc = 0;
       ast_name_t** throws = NULL; int thc = 0;
       ast_stmt_t* body;
    _loc = LOC;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_formal_param_list(p, &params, &pc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_kw_throws(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_name_list(p, &throws, &thc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    peg_skip(p);
    if (!java_parse_block(p, &body)) return false;
    *result = ast_constructor_decl(A, jdup(A, ns),
           params, pc, throws, thc, body, mods, mc);
       (*result)->loc = _loc;
    return true;
}

static bool java_parse_field_rest(peg_state* p, ast_member_t** result, ast_type_t* ty, peg_span ns, ast_modifier_t* mods, int mc) {
    ast_var_decl_t** decls = NULL; int dc = 0;
       int32_t dims = 0; ast_expr_t* init = NULL;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "[", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "]", 1)) break;
            dims++;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "=", 1)) break;
            peg_skip(p);
            if (!java_parse_var_init(p, &init)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    decls = (ast_var_decl_t**)jpush(A, (void**)decls, &dc,
             ast_var_decl(A, jdup(A, ns), dims, init));
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_var_decl_item(p, &decls, &dc)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    *result = ast_field_decl(A, ty, decls, dc, mods, mc);
    return true;
}

static bool java_parse_var_decl_item(peg_state* p, ast_var_decl_t*** decls, int* dc) {
    peg_span ns; int32_t dims = 0; ast_expr_t* init = NULL;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "[", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "]", 1)) break;
            dims++;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "=", 1)) break;
            peg_skip(p);
            if (!java_parse_var_init(p, &init)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    *decls = (ast_var_decl_t**)jpush(A, (void**)*decls, dc,
           ast_var_decl(A, jdup(A, ns), dims, init));
    return true;
}

static bool java_parse_var_init(peg_state* p, ast_expr_t** result) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_array_init_expr(p, result)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_expr_(p, result)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_array_init_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t** elems = NULL; int ec = 0; ast_expr_t* e;
    peg_skip(p);
    if (!peg_match_n(p, "{", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_var_init(p, &e)) break;
            elems = (ast_expr_t**)jpush(A, (void**)elems, &ec, e);
            for (;;) {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    peg_skip(p);
                    if (!peg_match_n(p, ",", 1)) break;
                    peg_skip(p);
                    if (!java_parse_var_init(p, &e)) break;
                    elems = (ast_expr_t**)jpush(A, (void**)elems, &ec, e);
                    _ok1 = true;
                } while(0);
                if (!_ok1) { peg_restore(p, _m1); break; }
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) peg_restore(p, _m2);
    }
    peg_skip(p);
    if (!peg_match_n(p, "}", 1)) return false;
    *result = ast_array_init(A, elems, ec);
    return true;
}

static bool java_parse_method_rest(peg_state* p, ast_member_t** result, ast_type_t* ty, peg_span ns, ast_modifier_t* mods, int mc) {
    ast_param_t** params = NULL; int pc = 0;
       ast_name_t** throws = NULL; int thc = 0;
       ast_stmt_t* body = NULL;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_formal_param_list(p, &params, &pc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_kw_throws(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_name_list(p, &throws, &thc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    {
        peg_mark _m2 = peg_save(p);
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!java_parse_block(p, &body)) break;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m2);
            } else goto _choice_done2;
        }
        peg_skip(p);
        if (!peg_match_n(p, ";", 1)) return false;
    _choice_done2:;
    }
    *result = ast_method_decl(A, ty, jdup(A, ns),
           params, pc, throws, thc, body, mods, mc);
    return true;
}

static bool java_parse_formal_param_list(peg_state* p, ast_param_t*** params, int* pc) {
    ast_param_t* fp;
    peg_skip(p);
    if (!java_parse_formal_param(p, &fp)) return false;
    *params = (ast_param_t**)jpush(A, (void**)*params, pc, fp);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_formal_param(p, &fp)) break;
            *params = (ast_param_t**)jpush(A, (void**)*params, pc, fp);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_formal_param(peg_state* p, ast_param_t** result) {
    ast_type_t* ty; peg_span ns;
    peg_skip(p);
    if (!java_parse_type_ref(p, &ty)) return false;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "[", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "]", 1)) break;
            ty = ast_array_type(A, ty);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    *result = ast_param(A, ty, jdup(A, ns));
    return true;
}

static bool java_parse_type_ref(peg_state* p, ast_type_t** result) {
    peg_skip(p);
    if (!java_parse_base_type(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "[", 1)) break;
            peg_skip(p);
            if (!peg_match_n(p, "]", 1)) break;
            *result = ast_array_type(A, *result);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_base_type(peg_state* p, ast_type_t** result) {
    ast_name_t* n;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_prim_type(p, result)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_qual_name(p, &n)) return false;
        *result = ast_class_type(A, n);
    _choice_done0:;
    }
    return true;
}

static bool java_parse_qual_name(peg_state* p, ast_name_t** result) {
    peg_span s;
    peg_skip(p);
    if (!java_parse_ident(p, &s)) return false;
    *result = ast_simple_name(A, jdup(A, s));
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ".", 1)) break;
            peg_skip(p);
            if (!java_parse_ident(p, &s)) break;
            *result = ast_qualified_name(A, *result, jdup(A, s));
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_modifier_(peg_state* p, ast_modifier_t** mods, int* count) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_kw_public(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_PUBLIC);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_kw_protected(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_PROTECTED);
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!java_kw_private(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_PRIVATE);
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!java_kw_static(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_STATIC);
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!java_kw_final(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_FINAL);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!java_kw_abstract(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_ABSTRACT);
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!java_kw_synchronized(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_SYNCHRONIZED);
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!java_kw_transient(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_TRANSIENT);
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!java_kw_volatile(p, NULL)) break;
                *mods = mpush(A, *mods, count, AST_VOLATILE);
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_kw_native(p, NULL)) return false;
        *mods = mpush(A, *mods, count, AST_NATIVE);
    _choice_done0:;
    }
    return true;
}

static bool java_parse_block(peg_state* p, ast_stmt_t** result) {
    ast_stmt_t** stmts = NULL; int sc = 0; ast_stmt_t* s; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!peg_match_n(p, "{", 1)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_block_stmt(p, &s)) break;
            stmts = (ast_stmt_t**)jpush(A, (void**)stmts, &sc, s);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "}", 1)) return false;
    *result = ast_block(A, stmts, sc); (*result)->loc = _loc;
    return true;
}

static bool java_parse_block_stmt(peg_state* p, ast_stmt_t** result) {
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
                        if (!java_kw_case(p, NULL)) break;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!java_kw_default(p, NULL)) break;
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        peg_restore(p, _m0);
        if (_ok0) return false;
    }
    {
        peg_mark _m3 = peg_save(p);
        {
            bool _ok4 = false;
            do {
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        {
                            peg_mark _m6 = peg_save(p);
                            {
                                bool _ok7 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_if(p, NULL)) break;
                                    _ok7 = true;
                                } while(0);
                                if (!_ok7) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok8 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_else(p, NULL)) break;
                                    _ok8 = true;
                                } while(0);
                                if (!_ok8) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok9 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_while(p, NULL)) break;
                                    _ok9 = true;
                                } while(0);
                                if (!_ok9) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok10 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_do(p, NULL)) break;
                                    _ok10 = true;
                                } while(0);
                                if (!_ok10) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok11 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_for(p, NULL)) break;
                                    _ok11 = true;
                                } while(0);
                                if (!_ok11) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok12 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_switch(p, NULL)) break;
                                    _ok12 = true;
                                } while(0);
                                if (!_ok12) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok13 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_try(p, NULL)) break;
                                    _ok13 = true;
                                } while(0);
                                if (!_ok13) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok14 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_catch(p, NULL)) break;
                                    _ok14 = true;
                                } while(0);
                                if (!_ok14) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok15 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_finally(p, NULL)) break;
                                    _ok15 = true;
                                } while(0);
                                if (!_ok15) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok16 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_throw(p, NULL)) break;
                                    _ok16 = true;
                                } while(0);
                                if (!_ok16) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok17 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_return(p, NULL)) break;
                                    _ok17 = true;
                                } while(0);
                                if (!_ok17) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok18 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_break(p, NULL)) break;
                                    _ok18 = true;
                                } while(0);
                                if (!_ok18) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok19 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_continue(p, NULL)) break;
                                    _ok19 = true;
                                } while(0);
                                if (!_ok19) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok20 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_new(p, NULL)) break;
                                    _ok20 = true;
                                } while(0);
                                if (!_ok20) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok21 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_this(p, NULL)) break;
                                    _ok21 = true;
                                } while(0);
                                if (!_ok21) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok22 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_super(p, NULL)) break;
                                    _ok22 = true;
                                } while(0);
                                if (!_ok22) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok23 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_null(p, NULL)) break;
                                    _ok23 = true;
                                } while(0);
                                if (!_ok23) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            {
                                bool _ok24 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_kw_true(p, NULL)) break;
                                    _ok24 = true;
                                } while(0);
                                if (!_ok24) {
                                    peg_restore(p, _m6);
                                } else goto _choice_done6;
                            }
                            peg_skip(p);
                            if (!java_kw_false(p, NULL)) break;
                        _choice_done6:;
                        }
                        _ok5 = true;
                    } while(0);
                    peg_restore(p, _m5);
                    if (_ok5) break;
                }
                peg_skip(p);
                if (!java_parse_local_var_decl_stmt(p, result)) break;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m3);
            } else goto _choice_done3;
        }
        peg_skip(p);
        if (!java_parse_statement(p, result)) return false;
    _choice_done3:;
    }
    return true;
}

static bool java_parse_local_var_decl_stmt(peg_state* p, ast_stmt_t** result) {
    ast_type_t* ty; ast_var_decl_t** decls = NULL; int dc = 0;
       ast_modifier_t* mods = NULL; int mc = 0; ast_srcloc _loc;
    _loc = LOC;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_modifier_(p, &mods, &mc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    peg_skip(p);
    if (!java_parse_type_ref(p, &ty)) return false;
    peg_skip(p);
    if (!java_parse_var_decl_item(p, &decls, &dc)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_var_decl_item(p, &decls, &dc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    *result = ast_local_var_decl(A, ty, decls, dc, mods, mc); (*result)->loc = _loc;
    return true;
}

static bool java_parse_statement(peg_state* p, ast_stmt_t** result) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_block(p, result)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_parse_if_stmt(p, result)) break;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!java_parse_while_stmt(p, result)) break;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!java_parse_do_stmt(p, result)) break;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!java_parse_for_stmt(p, result)) break;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!java_parse_switch_stmt(p, result)) break;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!java_parse_try_stmt(p, result)) break;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!java_parse_throw_stmt(p, result)) break;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!java_parse_return_stmt(p, result)) break;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok10 = false;
            do {
                ast_srcloc _bl = LOC;
                peg_skip(p);
                if (!java_kw_break(p, NULL)) break;
                const char* lbl = NULL; peg_span s;
                {
                    peg_mark _m11 = peg_save(p);
                    bool _ok11 = false;
                    do {
                        peg_skip(p);
                        if (!java_parse_ident(p, &s)) break;
                        lbl = jdup(A, s);
                        _ok11 = true;
                    } while(0);
                    if (!_ok11) peg_restore(p, _m11);
                }
                peg_skip(p);
                if (!peg_match_n(p, ";", 1)) break;
                *result = ast_break(A, lbl); (*result)->loc = _bl;
                _ok10 = true;
            } while(0);
            if (!_ok10) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok12 = false;
            do {
                ast_srcloc _cl = LOC;
                peg_skip(p);
                if (!java_kw_continue(p, NULL)) break;
                const char* lbl = NULL; peg_span s;
                {
                    peg_mark _m13 = peg_save(p);
                    bool _ok13 = false;
                    do {
                        peg_skip(p);
                        if (!java_parse_ident(p, &s)) break;
                        lbl = jdup(A, s);
                        _ok13 = true;
                    } while(0);
                    if (!_ok13) peg_restore(p, _m13);
                }
                peg_skip(p);
                if (!peg_match_n(p, ";", 1)) break;
                *result = ast_continue(A, lbl); (*result)->loc = _cl;
                _ok12 = true;
            } while(0);
            if (!_ok12) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok14 = false;
            do {
                peg_skip(p);
                if (!java_parse_labeled_or_expr_stmt(p, result)) break;
                _ok14 = true;
            } while(0);
            if (!_ok14) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, ";", 1)) return false;
        *result = ast_empty(A);
    _choice_done0:;
    }
    return true;
}

static bool java_parse_if_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* test; ast_stmt_t* then; ast_stmt_t* else_ = NULL; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_if(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!java_parse_expr_(p, &test)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!java_parse_statement(p, &then)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_kw_else(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_statement(p, &else_)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    *result = ast_if(A, test, then, else_); (*result)->loc = _loc;
    return true;
}

static bool java_parse_while_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* test; ast_stmt_t* body; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_while(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!java_parse_expr_(p, &test)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!java_parse_statement(p, &body)) return false;
    *result = ast_while(A, test, body); (*result)->loc = _loc;
    return true;
}

static bool java_parse_do_stmt(peg_state* p, ast_stmt_t** result) {
    ast_stmt_t* body; ast_expr_t* test; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_do(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_statement(p, &body)) return false;
    peg_skip(p);
    if (!java_kw_while(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!java_parse_expr_(p, &test)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    *result = ast_do_while(A, body, test); (*result)->loc = _loc;
    return true;
}

static bool java_parse_for_stmt(peg_state* p, ast_stmt_t** result) {
    ast_stmt_t* init = NULL; ast_expr_t* test = NULL;
       ast_expr_t** update = NULL; int uc = 0;
       ast_stmt_t* body; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_for(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_for_init(p, &init)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_parse_expr_(p, &test)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!java_parse_for_update(p, &update, &uc)) break;
            _ok2 = true;
        } while(0);
        if (!_ok2) peg_restore(p, _m2);
    }
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!java_parse_statement(p, &body)) return false;
    *result = ast_for(A, init, test, update, uc, body); (*result)->loc = _loc;
    return true;
}

static bool java_parse_for_init(peg_state* p, ast_stmt_t** result) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_local_var_decl_stmt_no_semi(p, result)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_for_init_expr(p, result)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_local_var_decl_stmt_no_semi(peg_state* p, ast_stmt_t** result) {
    ast_type_t* ty; ast_var_decl_t** decls = NULL; int dc = 0;
       ast_modifier_t* mods = NULL; int mc = 0; ast_srcloc _loc;
    _loc = LOC;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_modifier_(p, &mods, &mc)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    peg_skip(p);
    if (!java_parse_type_ref(p, &ty)) return false;
    peg_skip(p);
    if (!java_parse_var_decl_item(p, &decls, &dc)) return false;
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_var_decl_item(p, &decls, &dc)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    *result = ast_local_var_decl(A, ty, decls, dc, mods, mc); (*result)->loc = _loc;
    return true;
}

static bool java_parse_for_init_expr(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* e; ast_stmt_t** stmts = NULL; int sc = 0;
    peg_skip(p);
    if (!java_parse_expr_(p, &e)) return false;
    if (!jis_stmt_expr(e)) return false;
                    stmts = (ast_stmt_t**)jpush(A, (void**)stmts, &sc, ast_expr_stmt(A, e));
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_expr_(p, &e)) break;
            if (!jis_stmt_expr(e)) return false;
                          stmts = (ast_stmt_t**)jpush(A, (void**)stmts, &sc, ast_expr_stmt(A, e));
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    *result = (sc == 1) ? stmts[0] : ast_block(A, stmts, sc);
    return true;
}

static bool java_parse_for_update(peg_state* p, ast_expr_t*** list, int* count) {
    ast_expr_t* e;
    peg_skip(p);
    if (!java_parse_expr_(p, &e)) return false;
    if (!jis_stmt_expr(e)) return false;
                    *list = (ast_expr_t**)jpush(A, (void**)*list, count, e);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_expr_(p, &e)) break;
            if (!jis_stmt_expr(e)) return false;
                          *list = (ast_expr_t**)jpush(A, (void**)*list, count, e);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_switch_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* sel; ast_switch_case_t** cases = NULL; int cc = 0; ast_switch_case_t* sc; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_switch(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!java_parse_expr_(p, &sel)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "{", 1)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_switch_group(p, &sc)) break;
            cases = (ast_switch_case_t**)jpush(A, (void**)cases, &cc, sc);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    peg_skip(p);
    if (!peg_match_n(p, "}", 1)) return false;
    *result = ast_switch(A, sel, cases, cc); (*result)->loc = _loc;
    return true;
}

static bool java_parse_switch_group(peg_state* p, ast_switch_case_t** result) {
    ast_expr_t* val = NULL; ast_stmt_t** stmts = NULL; int sc = 0; ast_stmt_t* s;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_kw_case(p, NULL)) break;
                peg_skip(p);
                if (!java_parse_expr_(p, &val)) break;
                peg_skip(p);
                if (!peg_match_n(p, ":", 1)) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_kw_default(p, NULL)) return false;
        peg_skip(p);
        if (!peg_match_n(p, ":", 1)) return false;
    _choice_done0:;
    }
    for (;;) {
        peg_mark _m2 = peg_save(p);
        bool _ok2 = false;
        do {
            peg_skip(p);
            if (!java_parse_block_stmt(p, &s)) break;
            stmts = (ast_stmt_t**)jpush(A, (void**)stmts, &sc, s);
            _ok2 = true;
        } while(0);
        if (!_ok2) { peg_restore(p, _m2); break; }
    }
    *result = ast_switch_case(A, val, stmts, sc);
    return true;
}

static bool java_parse_try_stmt(peg_state* p, ast_stmt_t** result) {
    ast_stmt_t* body; ast_catch_clause_t** catches = NULL; int cc = 0;
       ast_catch_clause_t* c; ast_stmt_t* fin = NULL; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_try(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_block(p, &body)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_catch_clause_(p, &c)) break;
            catches = (ast_catch_clause_t**)jpush(A, (void**)catches, &cc, c);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!java_kw_finally(p, NULL)) break;
            peg_skip(p);
            if (!java_parse_block(p, &fin)) break;
            _ok1 = true;
        } while(0);
        if (!_ok1) peg_restore(p, _m1);
    }
    *result = ast_try(A, body, catches, cc, fin); (*result)->loc = _loc;
    return true;
}

static bool java_parse_catch_clause_(peg_state* p, ast_catch_clause_t** result) {
    ast_type_t* ty; peg_span ns; ast_stmt_t* body; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_catch(p, NULL)) return false;
    peg_skip(p);
    if (!peg_match_n(p, "(", 1)) return false;
    peg_skip(p);
    if (!java_parse_type_ref(p, &ty)) return false;
    peg_skip(p);
    if (!java_parse_ident(p, &ns)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ")", 1)) return false;
    peg_skip(p);
    if (!java_parse_block(p, &body)) return false;
    *result = ast_catch_clause(A, ty, jdup(A, ns), body); (*result)->loc = _loc;
    return true;
}

static bool java_parse_return_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* val = NULL; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_return(p, NULL)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!java_parse_expr_(p, &val)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    *result = ast_return(A, val); (*result)->loc = _loc;
    return true;
}

static bool java_parse_throw_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* e; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_kw_throw(p, NULL)) return false;
    peg_skip(p);
    if (!java_parse_expr_(p, &e)) return false;
    peg_skip(p);
    if (!peg_match_n(p, ";", 1)) return false;
    *result = ast_throw(A, e); (*result)->loc = _loc;
    return true;
}

static bool java_parse_labeled_or_expr_stmt(peg_state* p, ast_stmt_t** result) {
    ast_expr_t* e; peg_span s; ast_srcloc _loc; ast_stmt_t* body;
    _loc = LOC;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_parse_ident(p, &s)) break;
                peg_skip(p);
                if (!peg_match_n(p, ":", 1)) break;
                peg_skip(p);
                if (!java_parse_statement(p, &body)) break;
                *result = ast_labeled(A, jdup(A, s), body); (*result)->loc = _loc;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_expr_(p, &e)) return false;
        peg_skip(p);
        if (!peg_match_n(p, ";", 1)) return false;
        if (!jis_stmt_expr(e)) return false;
           *result = ast_expr_stmt(A, e); (*result)->loc = _loc;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_expr_(peg_state* p, ast_expr_t** result) {
    peg_skip(p);
    if (!java_parse_assign_expr(p, result)) return false;
    return true;
}

static bool java_parse_assign_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs; ast_binop_t op; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_parse_cond_expr(p, result)) return false;
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
                        if (!java_parse_compound_assign_op(p, &op)) break;
                        peg_skip(p);
                        if (!java_parse_assign_expr(p, &rhs)) break;
                        *result = ast_compound_assign(A, op, *result, rhs);
           (*result)->loc = _loc;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!peg_match_n(p, "=", 1)) break;
                peg_skip(p);
                if (!java_parse_assign_expr(p, &rhs)) break;
                *result = ast_assign(A, *result, rhs);
           (*result)->loc = _loc;
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    return true;
}

static bool java_parse_compound_assign_op(peg_state* p, ast_binop_t* op) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, ">>>=", 4)) break;
                *op = AST_USHR;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, ">>=", 3)) break;
                *op = AST_SHR;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "<<=", 3)) break;
                *op = AST_SHL;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "+=", 2)) break;
                *op = AST_ADD;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "-=", 2)) break;
                *op = AST_SUB;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "*=", 2)) break;
                *op = AST_MUL;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "/=", 2)) break;
                *op = AST_DIV;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "%=", 2)) break;
                *op = AST_REM;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "&=", 2)) break;
                *op = AST_BITAND;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok10 = false;
            do {
                peg_skip(p);
                if (!peg_match_n(p, "^=", 2)) break;
                *op = AST_BITXOR;
                _ok10 = true;
            } while(0);
            if (!_ok10) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "|=", 2)) return false;
        *op = AST_BITOR;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_cond_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* then; ast_expr_t* else_; ast_srcloc _loc;
    _loc = LOC;
    peg_skip(p);
    if (!java_parse_cond_or_expr(p, result)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "?", 1)) break;
            peg_skip(p);
            if (!java_parse_expr_(p, &then)) break;
            peg_skip(p);
            if (!peg_match_n(p, ":", 1)) break;
            peg_skip(p);
            if (!java_parse_cond_expr(p, &else_)) break;
            *result = ast_ternary(A, *result, then, else_);
           (*result)->loc = _loc;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    return true;
}

static bool java_parse_cond_or_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_cond_and_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "||", 2)) break;
            peg_skip(p);
            if (!java_parse_cond_and_expr(p, &rhs)) break;
            *result = ast_binary(A, AST_OR, *result, rhs);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_cond_and_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_inc_or_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "&&", 2)) break;
            peg_skip(p);
            if (!java_parse_inc_or_expr(p, &rhs)) break;
            *result = ast_binary(A, AST_AND, *result, rhs);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_inc_or_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_xor_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "|", 1)) break;
            peg_skip(p);
            if (!java_parse_xor_expr(p, &rhs)) break;
            *result = ast_binary(A, AST_BITOR, *result, rhs);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_xor_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_and_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "^", 1)) break;
            peg_skip(p);
            if (!java_parse_and_expr(p, &rhs)) break;
            *result = ast_binary(A, AST_BITXOR, *result, rhs);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_and_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_equal_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, "&", 1)) break;
            peg_skip(p);
            if (!java_parse_equal_expr(p, &rhs)) break;
            *result = ast_binary(A, AST_BITAND, *result, rhs);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_equal_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_rel_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (peg_peek_at_n(p, "==", 2)) {
                peg_skip(p);
                if (!peg_match_n(p, "==", 2)) break;
                peg_skip(p);
                if (!java_parse_rel_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_EQ, *result, rhs);
            } else {
                peg_skip(p);
                if (!peg_match_n(p, "!=", 2)) break;
                peg_skip(p);
                if (!java_parse_rel_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_NE, *result, rhs);
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_rel_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs; ast_type_t* ty;
    peg_skip(p);
    if (!java_parse_shift_expr(p, result)) return false;
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
                        if (!peg_match_n(p, "<=", 2)) break;
                        peg_skip(p);
                        if (!java_parse_shift_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_LE, *result, rhs);
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
                        if (!peg_match_n(p, ">=", 2)) break;
                        peg_skip(p);
                        if (!java_parse_shift_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_GE, *result, rhs);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                {
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "<", 1)) break;
                        peg_skip(p);
                        if (!java_parse_shift_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_LT, *result, rhs);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                {
                    bool _ok5 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, ">", 1)) break;
                        peg_skip(p);
                        if (!java_parse_shift_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_GT, *result, rhs);
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!java_kw_instanceof(p, NULL)) break;
                peg_skip(p);
                if (!java_parse_type_ref(p, &ty)) break;
                *result = ast_instance_of(A, *result, ty);
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_shift_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_add_expr(p, result)) return false;
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
                        if (!peg_match_n(p, ">>>", 3)) break;
                        peg_skip(p);
                        if (!java_parse_add_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_USHR, *result, rhs);
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
                        if (!peg_match_n(p, ">>", 2)) break;
                        peg_skip(p);
                        if (!java_parse_add_expr(p, &rhs)) break;
                        *result = ast_binary(A, AST_SHR,  *result, rhs);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!peg_match_n(p, "<<", 2)) break;
                peg_skip(p);
                if (!java_parse_add_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_SHL,  *result, rhs);
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_add_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_mult_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (peg_peek_at_n(p, "+", 1)) {
                peg_skip(p);
                if (!peg_match_n(p, "+", 1)) break;
                peg_skip(p);
                if (!java_parse_mult_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_ADD, *result, rhs);
            } else {
                peg_skip(p);
                if (!peg_match_n(p, "-", 1)) break;
                peg_skip(p);
                if (!java_parse_mult_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_SUB, *result, rhs);
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_mult_expr(peg_state* p, ast_expr_t** result) {
    ast_expr_t* rhs;
    peg_skip(p);
    if (!java_parse_unary_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (peg_peek_at_n(p, "*", 1)) {
                peg_skip(p);
                if (!peg_match_n(p, "*", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_MUL, *result, rhs);
            } else             if (peg_peek_at_n(p, "/", 1)) {
                peg_skip(p);
                if (!peg_match_n(p, "/", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_DIV, *result, rhs);
            } else {
                peg_skip(p);
                if (!peg_match_n(p, "%", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, &rhs)) break;
                *result = ast_binary(A, AST_REM, *result, rhs);
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_unary_expr(peg_state* p, ast_expr_t** result) {
    ast_srcloc _ul;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "++", 2)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_PREINC, *result); (*result)->loc = _ul;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "--", 2)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_PREDEC, *result); (*result)->loc = _ul;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "+", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_POS, *result); (*result)->loc = _ul;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "-", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_NEG, *result); (*result)->loc = _ul;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_unary_expr_npm(p, result)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_unary_expr_npm(peg_state* p, ast_expr_t** result) {
    ast_type_t* ty; ast_expr_t* e; ast_name_t* n; ast_srcloc _ul;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!java_parse_prim_type(p, &ty)) break;
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "[", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "]", 1)) break;
                        ty = ast_array_type(A, ty);
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) { peg_restore(p, _m2); break; }
                }
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, &e)) break;
                *result = ast_cast(A, ty, e); (*result)->loc = _ul;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "(", 1)) break;
                peg_skip(p);
                if (!java_parse_qual_name(p, &n)) break;
                ty = ast_class_type(A, n);
                for (;;) {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!peg_match_n(p, "[", 1)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "]", 1)) break;
                        ty = ast_array_type(A, ty);
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) { peg_restore(p, _m4); break; }
                }
                peg_skip(p);
                if (!peg_match_n(p, ")", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr_npm(p, &e)) break;
                *result = ast_cast(A, ty, e); (*result)->loc = _ul;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "~", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_BITNOT, *result); (*result)->loc = _ul;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                _ul = LOC;
                peg_skip(p);
                if (!peg_match_n(p, "!", 1)) break;
                peg_skip(p);
                if (!java_parse_unary_expr(p, result)) break;
                *result = ast_unary(A, AST_LOGNOT, *result); (*result)->loc = _ul;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_parse_postfix_expr(p, result)) return false;
    _choice_done0:;
    }
    return true;
}

static bool java_parse_prim_type(peg_state* p, ast_type_t** result) {
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_kw_byte(p, NULL)) break;
                *result = ast_byte_type(A);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_kw_short(p, NULL)) break;
                *result = ast_short_type(A);
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!java_kw_int(p, NULL)) break;
                *result = ast_int_type(A);
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!java_kw_long(p, NULL)) break;
                *result = ast_long_type(A);
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!java_kw_char(p, NULL)) break;
                *result = ast_char_type(A);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!java_kw_float(p, NULL)) break;
                *result = ast_float_type(A);
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!java_kw_double(p, NULL)) break;
                *result = ast_double_type(A);
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!java_kw_boolean(p, NULL)) return false;
        *result = ast_bool_type(A);
    _choice_done0:;
    }
    return true;
}

static bool java_parse_postfix_expr(peg_state* p, ast_expr_t** result) {
    peg_span s; ast_expr_t** args = NULL; int ac = 0; ast_expr_t* idx; ast_srcloc _ploc;
    peg_skip(p);
    if (!java_parse_primary_expr(p, result)) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            _ploc = LOC;
            peg_skip(p);
            if (peg_peek_at_n(p, ".", 1)) {
                peg_skip(p);
                if (!peg_match_n(p, ".", 1)) break;
                peg_skip(p);
                if (!java_parse_ident(p, &s)) break;
                {
                    peg_mark _m1 = peg_save(p);
                    {
                        bool _ok2 = false;
                        do {
                            peg_skip(p);
                            if (!peg_match_n(p, "(", 1)) break;
                            {
                                peg_mark _m3 = peg_save(p);
                                bool _ok3 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_parse_arg_list(p, &args, &ac)) break;
                                    _ok3 = true;
                                } while(0);
                                if (!_ok3) peg_restore(p, _m3);
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, ")", 1)) break;
                            *result = ast_method_call(A, *result, jdup(A, s), args, ac);
               (*result)->loc = _ploc; args = NULL; ac = 0;
                            _ok2 = true;
                        } while(0);
                        if (!_ok2) {
                            peg_restore(p, _m1);
                        } else goto _choice_done1;
                    }
                    *result = ast_field_access(A, *result, jdup(A, s));
               (*result)->loc = _ploc;
                _choice_done1:;
                }
            } else             if (peg_peek_at_n(p, "[", 1)) {
                peg_skip(p);
                if (!peg_match_n(p, "[", 1)) break;
                peg_skip(p);
                if (!java_parse_expr_(p, &idx)) break;
                peg_skip(p);
                if (!peg_match_n(p, "]", 1)) break;
                *result = ast_array_access(A, *result, idx); (*result)->loc = _ploc;
            } else             if (peg_peek_at_n(p, "++", 2)) {
                peg_skip(p);
                if (!peg_match_n(p, "++", 2)) break;
                *result = ast_unary(A, AST_POSTINC, *result); (*result)->loc = _ploc;
            } else {
                peg_skip(p);
                if (!peg_match_n(p, "--", 2)) break;
                *result = ast_unary(A, AST_POSTDEC, *result); (*result)->loc = _ploc;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool java_parse_primary_expr(peg_state* p, ast_expr_t** result) {
    int64_t n; peg_span s; peg_span isp; peg_span csp; peg_span fsp; peg_span lsp;
       ast_srcloc _loc; ast_name_t* name; ast_type_t* ty;
       ast_expr_t** args = NULL; int ac = 0; ast_expr_t* sz;
       ast_expr_t** dims = NULL; int dc = 0;
    _loc = LOC;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!java_float_lit(p, &fsp)) break;
                *result = jparse_float_lit(A, fsp);
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                peg_skip(p);
                if (!java_long_lit(p, &lsp)) break;
                *result = ast_long_lit(A, jparse_long(lsp));
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                peg_skip(p);
                if (!java_integer(p, &isp)) break;
                n = jparse_int(isp); *result = ast_int_lit(A, (int32_t)n);
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                peg_skip(p);
                if (!java_kw_true(p, NULL)) break;
                *result = ast_bool_lit(A, true);
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                peg_skip(p);
                if (!java_kw_false(p, NULL)) break;
                *result = ast_bool_lit(A, false);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                peg_skip(p);
                if (!java_kw_null(p, NULL)) break;
                *result = ast_null_lit(A);
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                peg_skip(p);
                if (!java_char_lit(p, &csp)) break;
                *result = ast_char_lit(A, jparse_char_lit(csp));
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                peg_skip(p);
                if (!java_string_lit(p, &s)) break;
                peg_span body = { s.ptr + 1, s.len - 2 };
                        *result = jstr_to_string(A, body);
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                peg_skip(p);
                if (!java_kw_this(p, NULL)) break;
                {
                    peg_mark _m10 = peg_save(p);
                    {
                        bool _ok11 = false;
                        do {
                            peg_skip(p);
                            if (!peg_match_n(p, "(", 1)) break;
                            {
                                peg_mark _m12 = peg_save(p);
                                bool _ok12 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_parse_arg_list(p, &args, &ac)) break;
                                    _ok12 = true;
                                } while(0);
                                if (!_ok12) peg_restore(p, _m12);
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, ")", 1)) break;
                            *result = ast_constructor_call(A, false, args, ac);
                            _ok11 = true;
                        } while(0);
                        if (!_ok11) {
                            peg_restore(p, _m10);
                        } else goto _choice_done10;
                    }
                    *result = ast_this(A);
                _choice_done10:;
                }
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok13 = false;
            do {
                peg_skip(p);
                if (!java_kw_super(p, NULL)) break;
                peg_skip(p);
                if (peg_peek_at_n(p, ".", 1)) {
                    peg_skip(p);
                    if (!peg_match_n(p, ".", 1)) break;
                    peg_skip(p);
                    if (!java_parse_ident(p, &s)) break;
                    {
                        peg_mark _m14 = peg_save(p);
                        {
                            bool _ok15 = false;
                            do {
                                peg_skip(p);
                                if (!peg_match_n(p, "(", 1)) break;
                                {
                                    peg_mark _m16 = peg_save(p);
                                    bool _ok16 = false;
                                    do {
                                        peg_skip(p);
                                        if (!java_parse_arg_list(p, &args, &ac)) break;
                                        _ok16 = true;
                                    } while(0);
                                    if (!_ok16) peg_restore(p, _m16);
                                }
                                peg_skip(p);
                                if (!peg_match_n(p, ")", 1)) break;
                                *result = ast_super_call(A, jdup(A, s), args, ac);
                                _ok15 = true;
                            } while(0);
                            if (!_ok15) {
                                peg_restore(p, _m14);
                            } else goto _choice_done14;
                        }
                        *result = ast_super_access(A, jdup(A, s));
                    _choice_done14:;
                    }
                } else {
                    peg_skip(p);
                    if (!peg_match_n(p, "(", 1)) break;
                    {
                        peg_mark _m17 = peg_save(p);
                        bool _ok17 = false;
                        do {
                            peg_skip(p);
                            if (!java_parse_arg_list(p, &args, &ac)) break;
                            _ok17 = true;
                        } while(0);
                        if (!_ok17) peg_restore(p, _m17);
                    }
                    peg_skip(p);
                    if (!peg_match_n(p, ")", 1)) break;
                    *result = ast_constructor_call(A, true, args, ac);
                }
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok18 = false;
            do {
                peg_skip(p);
                if (!java_kw_new(p, NULL)) break;
                {
                    peg_mark _m19 = peg_save(p);
                    {
                        bool _ok20 = false;
                        do {
                            peg_skip(p);
                            if (!java_parse_base_type(p, &ty)) break;
                            peg_skip(p);
                            if (!peg_match_n(p, "[", 1)) break;
                            peg_skip(p);
                            if (!java_parse_expr_(p, &sz)) break;
                            peg_skip(p);
                            if (!peg_match_n(p, "]", 1)) break;
                            dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, sz);
                            for (;;) {
                                peg_mark _m21 = peg_save(p);
                                bool _ok21 = false;
                                do {
                                    {
                                        peg_mark _m22 = peg_save(p);
                                        {
                                            bool _ok23 = false;
                                            do {
                                                peg_skip(p);
                                                if (!peg_match_n(p, "[", 1)) break;
                                                peg_skip(p);
                                                if (!java_parse_expr_(p, &sz)) break;
                                                peg_skip(p);
                                                if (!peg_match_n(p, "]", 1)) break;
                                                dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, sz);
                                                _ok23 = true;
                                            } while(0);
                                            if (!_ok23) {
                                                peg_restore(p, _m22);
                                            } else goto _choice_done22;
                                        }
                                        peg_skip(p);
                                        if (!peg_match_n(p, "[", 1)) break;
                                        peg_skip(p);
                                        if (!peg_match_n(p, "]", 1)) break;
                                        dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, NULL);
                                    _choice_done22:;
                                    }
                                    _ok21 = true;
                                } while(0);
                                if (!_ok21) { peg_restore(p, _m21); break; }
                            }
                            *result = ast_new_array(A, ty, dims, dc); dims = NULL; dc = 0;
                            _ok20 = true;
                        } while(0);
                        if (!_ok20) {
                            peg_restore(p, _m19);
                        } else goto _choice_done19;
                    }
                    peg_skip(p);
                    if (!java_parse_qual_name(p, &name)) break;
                    peg_skip(p);
                    if (peg_peek_at_n(p, "(", 1)) {
                        peg_skip(p);
                        if (!peg_match_n(p, "(", 1)) break;
                        {
                            peg_mark _m24 = peg_save(p);
                            bool _ok24 = false;
                            do {
                                peg_skip(p);
                                if (!java_parse_arg_list(p, &args, &ac)) break;
                                _ok24 = true;
                            } while(0);
                            if (!_ok24) peg_restore(p, _m24);
                        }
                        peg_skip(p);
                        if (!peg_match_n(p, ")", 1)) break;
                        *result = ast_new(A, name, args, ac);
                    } else {
                        peg_skip(p);
                        if (!peg_match_n(p, "[", 1)) break;
                        peg_skip(p);
                        if (!java_parse_expr_(p, &sz)) break;
                        peg_skip(p);
                        if (!peg_match_n(p, "]", 1)) break;
                        dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, sz);
                        for (;;) {
                            peg_mark _m25 = peg_save(p);
                            bool _ok25 = false;
                            do {
                                {
                                    peg_mark _m26 = peg_save(p);
                                    {
                                        bool _ok27 = false;
                                        do {
                                            peg_skip(p);
                                            if (!peg_match_n(p, "[", 1)) break;
                                            peg_skip(p);
                                            if (!java_parse_expr_(p, &sz)) break;
                                            peg_skip(p);
                                            if (!peg_match_n(p, "]", 1)) break;
                                            dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, sz);
                                            _ok27 = true;
                                        } while(0);
                                        if (!_ok27) {
                                            peg_restore(p, _m26);
                                        } else goto _choice_done26;
                                    }
                                    peg_skip(p);
                                    if (!peg_match_n(p, "[", 1)) break;
                                    peg_skip(p);
                                    if (!peg_match_n(p, "]", 1)) break;
                                    dims = (ast_expr_t**)jpush(A, (void**)dims, &dc, NULL);
                                _choice_done26:;
                                }
                                _ok25 = true;
                            } while(0);
                            if (!_ok25) { peg_restore(p, _m25); break; }
                        }
                        *result = ast_new_array(A, ast_class_type(A, name), dims, dc); dims = NULL; dc = 0;
                    }
                _choice_done19:;
                }
                _ok18 = true;
            } while(0);
            if (!_ok18) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok28 = false;
            do {
                peg_skip(p);
                if (!java_parse_ident(p, &s)) break;
                {
                    peg_mark _m29 = peg_save(p);
                    {
                        bool _ok30 = false;
                        do {
                            peg_skip(p);
                            if (!peg_match_n(p, "(", 1)) break;
                            {
                                peg_mark _m31 = peg_save(p);
                                bool _ok31 = false;
                                do {
                                    peg_skip(p);
                                    if (!java_parse_arg_list(p, &args, &ac)) break;
                                    _ok31 = true;
                                } while(0);
                                if (!_ok31) peg_restore(p, _m31);
                            }
                            peg_skip(p);
                            if (!peg_match_n(p, ")", 1)) break;
                            *result = ast_method_call(A, NULL, jdup(A, s), args, ac);
                            _ok30 = true;
                        } while(0);
                        if (!_ok30) {
                            peg_restore(p, _m29);
                        } else goto _choice_done29;
                    }
                    *result = ast_ident(A, jdup(A, s));
                _choice_done29:;
                }
                _ok28 = true;
            } while(0);
            if (!_ok28) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!peg_match_n(p, "(", 1)) return false;
        peg_skip(p);
        if (!java_parse_expr_(p, result)) return false;
        peg_skip(p);
        if (!peg_match_n(p, ")", 1)) return false;
    _choice_done0:;
    }
    (*result)->loc = _loc;
    return true;
}

static bool java_parse_arg_list(peg_state* p, ast_expr_t*** args, int* count) {
    ast_expr_t* e;
    peg_skip(p);
    if (!java_parse_expr_(p, &e)) return false;
    *args = (ast_expr_t**)jpush(A, (void**)*args, count, e);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match_n(p, ",", 1)) break;
            peg_skip(p);
            if (!java_parse_expr_(p, &e)) break;
            *args = (ast_expr_t**)jpush(A, (void**)*args, count, e);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

