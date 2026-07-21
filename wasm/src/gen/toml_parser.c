#include "toml_parser.h"



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

PEG_INTERNAL bool peg_match(peg_state* p, const char* str) {
    int len = (int)strlen(str);
    if (p->pos + len > p->end) return false;
    if (memcmp(p->pos, str, (size_t)len) != 0) return false;
    for (int i = 0; i < len; i++) peg_advance(p);
    return true;
}

PEG_INTERNAL bool peg_match_charset(peg_state* p, bool (*fn)(char)) {
    if (p->pos < p->end && fn(*p->pos)) {
        peg_advance(p);
        return true;
    }
    return false;
}

/* ── Lookahead (no consume) ──────────────────────────────── */

PEG_INTERNAL bool peg_peek_at(const peg_state* p, const char* str) {
    int len = (int)strlen(str);
    if (p->pos + len > p->end) return false;
    return memcmp(p->pos, str, (size_t)len) == 0;
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
}

PEG_INTERNAL void peg_add_comment(peg_state* p, const char* open,
                            const char* close, bool nested) {
    if (p->comment_count < PEG_MAX_COMMENTS) {
        p->comments[p->comment_count].open = open;
        p->comments[p->comment_count].close = close;
        p->comments[p->comment_count].nested = nested;
        p->comment_count++;
    }
}

PEG_INTERNAL bool peg_skip_comment(peg_state* p, const peg_comment_spec* spec) {
    int open_len = (int)strlen(spec->open);
    if (p->pos + open_len > p->end) return false;
    if (memcmp(p->pos, spec->open, (size_t)open_len) != 0) return false;
    for (int i = 0; i < open_len; i++) peg_advance(p);
    int close_len = (int)strlen(spec->close);
    int depth = 1;
    while (p->pos < p->end && depth > 0) {
        if (spec->nested && p->pos + open_len <= p->end &&
            memcmp(p->pos, spec->open, (size_t)open_len) == 0) {
            for (int i = 0; i < open_len; i++) peg_advance(p);
            depth++;
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
    for (;;) {
        bool skipped = false;
        while (p->pos < p->end && p->ws_fn && p->ws_fn(*p->pos)) {
            peg_advance(p);
            skipped = true;
        }
        bool found_comment = false;
        for (int i = 0; i < p->comment_count; i++) {
            if (peg_skip_comment(p, &p->comments[i])) {
                found_comment = true;
                break;
            }
        }
        if (!found_comment && !skipped) break;
    }
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

static bool is_digit1_9(char c) {
    return (c >= 49 && c <= 57);
}

static bool is_digit0_7(char c) {
    return (c >= 48 && c <= 55);
}

static bool is_digit0_1(char c) {
    return (c >= 48 && c <= 49);
}

static bool is_hex_digit(char c) {
    return ((is_digit(c) || (c >= 97 && c <= 102)) || (c >= 65 && c <= 70));
}

static bool is_alpha(char c) {
    return ((c >= 97 && c <= 122) || (c >= 65 && c <= 90));
}

static bool is_unquoted_key(char c) {
    return (((is_alpha(c) || is_digit(c)) || (c == 45)) || (c == 95));
}

static bool is_bad_ctl(char c) {
    return ((c >= 0 && c <= 8) || (c >= 10 && c <= 31));
}

static bool is_del(char c) {
    return (c == 127);
}

static bool toml_ws_predicate(char c) {
    return ((c == 32) || (c == 9));
}

static void toml_setup_skip(peg_state* p) {
    peg_set_whitespace(p, toml_ws_predicate);
}

static bool toml_tok_newline(peg_state* p, peg_span* out);
static bool toml_tok_comment(peg_state* p, peg_span* out);
static bool toml_tok_bare_key(peg_state* p, peg_span* out);
static bool toml_tok_integer(peg_state* p, peg_span* out);
static bool toml_tok_float(peg_state* p, peg_span* out);
static bool toml_tok_basic_string(peg_state* p, peg_span* out);
static bool toml_tok_ml_basic_string(peg_state* p, peg_span* out);
static bool toml_tok_literal_string(peg_state* p, peg_span* out);
static bool toml_tok_ml_literal_string(peg_state* p, peg_span* out);
static bool toml_tok_date_time(peg_state* p, peg_span* out);
static bool toml_parse_toml(peg_state* p);
static bool toml_parse_expression(peg_state* p, toml_item_t*** items, int* ic);
static bool toml_parse_newline(peg_state* p);
static bool toml_parse_comment(peg_state* p);
static bool toml_parse_key_path(peg_state* p, toml_key_path_t** result);
static bool toml_parse_key_part(peg_state* p, toml_key_part_t** result);
static bool toml_parse_key_value(peg_state* p, toml_key_value_t** result);
static bool toml_parse_value(peg_state* p, toml_value_t** result);
static bool toml_parse_array_elems(peg_state* p, toml_value_t*** elems, int* ec);
static bool toml_parse_array_ws_cn(peg_state* p);
static bool toml_parse_inline_table_entries(peg_state* p, toml_key_value_t*** entries, int* kc);


void toml_parser_init(peg_state* p, const char* input, int length) {
    peg_init(p, input, length);
    toml_setup_skip(p);
}

bool toml_parser_parse(peg_state* p) {
    peg_skip(p);
    return toml_parse_toml(p);
}


static bool toml_tok_newline(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (peg_peek_at(p, "\n")) {
        if (!peg_match(p, "\n")) return false;
    } else {
        if (!peg_match(p, "\r")) return false;
        if (!peg_match(p, "\n")) return false;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_comment(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match(p, "#")) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    if (peg_at_end(p) || !is_bad_ctl(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok1 = true;
                } while(0);
                peg_restore(p, _m1);
                if (_ok1) break;
            }
            if (peg_at_end(p)) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_bare_key(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (peg_at_end(p) || !is_unquoted_key(peg_peek_char(p))) return false;
    peg_advance(p);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_at_end(p) || !is_unquoted_key(peg_peek_char(p))) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_integer(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (!peg_match(p, "0")) break;
                if (!peg_match(p, "x")) break;
                if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        {
                            peg_mark _m3 = peg_save(p);
                            {
                                bool _ok4 = false;
                                do {
                                    if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok4 = true;
                                } while(0);
                                if (!_ok4) {
                                    peg_restore(p, _m3);
                                } else goto _choice_done3;
                            }
                            if (!peg_match(p, "_")) break;
                            if (peg_at_end(p) || !is_hex_digit(peg_peek_char(p))) break;
                            peg_advance(p);
                        _choice_done3:;
                        }
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
            bool _ok5 = false;
            do {
                if (!peg_match(p, "0")) break;
                if (!peg_match(p, "o")) break;
                if (peg_at_end(p) || !is_digit0_7(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        {
                            peg_mark _m7 = peg_save(p);
                            {
                                bool _ok8 = false;
                                do {
                                    if (peg_at_end(p) || !is_digit0_7(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok8 = true;
                                } while(0);
                                if (!_ok8) {
                                    peg_restore(p, _m7);
                                } else goto _choice_done7;
                            }
                            if (!peg_match(p, "_")) break;
                            if (peg_at_end(p) || !is_digit0_7(peg_peek_char(p))) break;
                            peg_advance(p);
                        _choice_done7:;
                        }
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) { peg_restore(p, _m6); break; }
                }
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                if (!peg_match(p, "0")) break;
                if (!peg_match(p, "b")) break;
                if (peg_at_end(p) || !is_digit0_1(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        {
                            peg_mark _m11 = peg_save(p);
                            {
                                bool _ok12 = false;
                                do {
                                    if (peg_at_end(p) || !is_digit0_1(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok12 = true;
                                } while(0);
                                if (!_ok12) {
                                    peg_restore(p, _m11);
                                } else goto _choice_done11;
                            }
                            if (!peg_match(p, "_")) break;
                            if (peg_at_end(p) || !is_digit0_1(peg_peek_char(p))) break;
                            peg_advance(p);
                        _choice_done11:;
                        }
                        _ok10 = true;
                    } while(0);
                    if (!_ok10) { peg_restore(p, _m10); break; }
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
                {
                    peg_mark _m14 = peg_save(p);
                    bool _ok14 = false;
                    do {
                        if (peg_peek_at(p, "+")) {
                            if (!peg_match(p, "+")) break;
                        } else {
                            if (!peg_match(p, "-")) break;
                        }
                        _ok14 = true;
                    } while(0);
                    if (!_ok14) peg_restore(p, _m14);
                }
                if (peg_at_end(p) || !is_digit1_9(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m15 = peg_save(p);
                    bool _ok15 = false;
                    do {
                        {
                            peg_mark _m16 = peg_save(p);
                            {
                                bool _ok17 = false;
                                do {
                                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    _ok17 = true;
                                } while(0);
                                if (!_ok17) {
                                    peg_restore(p, _m16);
                                } else goto _choice_done16;
                            }
                            if (!peg_match(p, "_")) break;
                            if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                            peg_advance(p);
                        _choice_done16:;
                        }
                        _ok15 = true;
                    } while(0);
                    if (!_ok15) { peg_restore(p, _m15); break; }
                }
                _ok13 = true;
            } while(0);
            if (!_ok13) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            peg_mark _m18 = peg_save(p);
            bool _ok18 = false;
            do {
                if (peg_peek_at(p, "+")) {
                    if (!peg_match(p, "+")) break;
                } else {
                    if (!peg_match(p, "-")) break;
                }
                _ok18 = true;
            } while(0);
            if (!_ok18) peg_restore(p, _m18);
        }
        if (!peg_match(p, "0")) return false;
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_float(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_peek_at(p, "+")) {
                            if (!peg_match(p, "+")) break;
                        } else {
                            if (!peg_match(p, "-")) break;
                        }
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) peg_restore(p, _m2);
                }
                if (!peg_match(p, "inf")) break;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (peg_peek_at(p, "+")) {
                            if (!peg_match(p, "+")) break;
                        } else {
                            if (!peg_match(p, "-")) break;
                        }
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) peg_restore(p, _m4);
                }
                if (!peg_match(p, "nan")) break;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            peg_mark _m5 = peg_save(p);
            bool _ok5 = false;
            do {
                if (peg_peek_at(p, "+")) {
                    if (!peg_match(p, "+")) break;
                } else {
                    if (!peg_match(p, "-")) break;
                }
                _ok5 = true;
            } while(0);
            if (!_ok5) peg_restore(p, _m5);
        }
        {
            peg_mark _m6 = peg_save(p);
            {
                bool _ok7 = false;
                do {
                    if (peg_at_end(p) || !is_digit1_9(peg_peek_char(p))) break;
                    peg_advance(p);
                    for (;;) {
                        peg_mark _m8 = peg_save(p);
                        bool _ok8 = false;
                        do {
                            {
                                peg_mark _m9 = peg_save(p);
                                {
                                    bool _ok10 = false;
                                    do {
                                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                        peg_advance(p);
                                        _ok10 = true;
                                    } while(0);
                                    if (!_ok10) {
                                        peg_restore(p, _m9);
                                    } else goto _choice_done9;
                                }
                                if (!peg_match(p, "_")) break;
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                            _choice_done9:;
                            }
                            _ok8 = true;
                        } while(0);
                        if (!_ok8) { peg_restore(p, _m8); break; }
                    }
                    _ok7 = true;
                } while(0);
                if (!_ok7) {
                    peg_restore(p, _m6);
                } else goto _choice_done6;
            }
            if (!peg_match(p, "0")) return false;
        _choice_done6:;
        }
        {
            peg_mark _m11 = peg_save(p);
            {
                bool _ok12 = false;
                do {
                    if (!peg_match(p, ".")) break;
                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                    peg_advance(p);
                    for (;;) {
                        peg_mark _m13 = peg_save(p);
                        bool _ok13 = false;
                        do {
                            {
                                peg_mark _m14 = peg_save(p);
                                {
                                    bool _ok15 = false;
                                    do {
                                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                        peg_advance(p);
                                        _ok15 = true;
                                    } while(0);
                                    if (!_ok15) {
                                        peg_restore(p, _m14);
                                    } else goto _choice_done14;
                                }
                                if (!peg_match(p, "_")) break;
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                            _choice_done14:;
                            }
                            _ok13 = true;
                        } while(0);
                        if (!_ok13) { peg_restore(p, _m13); break; }
                    }
                    {
                        peg_mark _m16 = peg_save(p);
                        bool _ok16 = false;
                        do {
                            if (peg_peek_at(p, "e")) {
                                if (!peg_match(p, "e")) break;
                            } else {
                                if (!peg_match(p, "E")) break;
                            }
                            {
                                peg_mark _m17 = peg_save(p);
                                bool _ok17 = false;
                                do {
                                    if (peg_peek_at(p, "+")) {
                                        if (!peg_match(p, "+")) break;
                                    } else {
                                        if (!peg_match(p, "-")) break;
                                    }
                                    _ok17 = true;
                                } while(0);
                                if (!_ok17) peg_restore(p, _m17);
                            }
                            if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                            peg_advance(p);
                            for (;;) {
                                peg_mark _m18 = peg_save(p);
                                bool _ok18 = false;
                                do {
                                    {
                                        peg_mark _m19 = peg_save(p);
                                        {
                                            bool _ok20 = false;
                                            do {
                                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                                peg_advance(p);
                                                _ok20 = true;
                                            } while(0);
                                            if (!_ok20) {
                                                peg_restore(p, _m19);
                                            } else goto _choice_done19;
                                        }
                                        if (!peg_match(p, "_")) break;
                                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                        peg_advance(p);
                                    _choice_done19:;
                                    }
                                    _ok18 = true;
                                } while(0);
                                if (!_ok18) { peg_restore(p, _m18); break; }
                            }
                            _ok16 = true;
                        } while(0);
                        if (!_ok16) peg_restore(p, _m16);
                    }
                    _ok12 = true;
                } while(0);
                if (!_ok12) {
                    peg_restore(p, _m11);
                } else goto _choice_done11;
            }
            if (peg_peek_at(p, "e")) {
                if (!peg_match(p, "e")) return false;
            } else {
                if (!peg_match(p, "E")) return false;
            }
            {
                peg_mark _m21 = peg_save(p);
                bool _ok21 = false;
                do {
                    if (peg_peek_at(p, "+")) {
                        if (!peg_match(p, "+")) break;
                    } else {
                        if (!peg_match(p, "-")) break;
                    }
                    _ok21 = true;
                } while(0);
                if (!_ok21) peg_restore(p, _m21);
            }
            if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
            peg_advance(p);
            for (;;) {
                peg_mark _m22 = peg_save(p);
                bool _ok22 = false;
                do {
                    {
                        peg_mark _m23 = peg_save(p);
                        {
                            bool _ok24 = false;
                            do {
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                peg_advance(p);
                                _ok24 = true;
                            } while(0);
                            if (!_ok24) {
                                peg_restore(p, _m23);
                            } else goto _choice_done23;
                        }
                        if (!peg_match(p, "_")) break;
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                    _choice_done23:;
                    }
                    _ok22 = true;
                } while(0);
                if (!_ok22) { peg_restore(p, _m22); break; }
            }
        _choice_done11:;
        }
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_basic_string(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match(p, "\"")) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                {
                    bool _ok2 = false;
                    do {
                        if (!peg_match(p, "\\")) break;
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
                        if (!peg_match(p, "\"")) break;
                        _ok3 = true;
                    } while(0);
                    peg_restore(p, _m3);
                    if (_ok3) break;
                }
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        if (!peg_match(p, "\\")) break;
                        _ok4 = true;
                    } while(0);
                    peg_restore(p, _m4);
                    if (_ok4) break;
                }
                {
                    peg_mark _m5 = peg_save(p);
                    bool _ok5 = false;
                    do {
                        if (peg_at_end(p) || !is_bad_ctl(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok5 = true;
                    } while(0);
                    peg_restore(p, _m5);
                    if (_ok5) break;
                }
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        if (peg_at_end(p) || !is_del(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok6 = true;
                    } while(0);
                    peg_restore(p, _m6);
                    if (_ok6) break;
                }
                if (peg_at_end(p)) break;
                peg_advance(p);
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (!peg_match(p, "\"")) return false;
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_ml_basic_string(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match(p, "\"")) return false;
    if (!peg_match(p, "\"")) return false;
    if (!peg_match(p, "\"")) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_peek_at(p, "\r")) {
                if (!peg_match(p, "\r")) break;
                if (!peg_match(p, "\n")) break;
            } else {
                if (!peg_match(p, "\n")) break;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            {
                peg_mark _m2 = peg_save(p);
                {
                    bool _ok3 = false;
                    do {
                        if (!peg_match(p, "\\")) break;
                        if (peg_at_end(p)) break;
                        peg_advance(p);
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok4 = false;
                    do {
                        if (!peg_match(p, "\r")) break;
                        if (!peg_match(p, "\n")) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok5 = false;
                    do {
                        if (!peg_match(p, "\n")) break;
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok6 = false;
                    do {
                        if (!peg_match(p, "\"")) break;
                        if (!peg_match(p, "\"")) break;
                        {
                            peg_mark _m7 = peg_save(p);
                            bool _ok7 = false;
                            do {
                                if (!peg_match(p, "\"")) break;
                                _ok7 = true;
                            } while(0);
                            peg_restore(p, _m7);
                            if (_ok7) break;
                        }
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok8 = false;
                    do {
                        if (!peg_match(p, "\"")) break;
                        {
                            peg_mark _m9 = peg_save(p);
                            bool _ok9 = false;
                            do {
                                if (!peg_match(p, "\"")) break;
                                _ok9 = true;
                            } while(0);
                            peg_restore(p, _m9);
                            if (_ok9) break;
                        }
                        _ok8 = true;
                    } while(0);
                    if (!_ok8) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (!peg_match(p, "\"")) break;
                        _ok10 = true;
                    } while(0);
                    peg_restore(p, _m10);
                    if (_ok10) break;
                }
                {
                    peg_mark _m11 = peg_save(p);
                    bool _ok11 = false;
                    do {
                        if (!peg_match(p, "\\")) break;
                        _ok11 = true;
                    } while(0);
                    peg_restore(p, _m11);
                    if (_ok11) break;
                }
                {
                    peg_mark _m12 = peg_save(p);
                    bool _ok12 = false;
                    do {
                        if (peg_at_end(p) || !is_bad_ctl(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok12 = true;
                    } while(0);
                    peg_restore(p, _m12);
                    if (_ok12) break;
                }
                {
                    peg_mark _m13 = peg_save(p);
                    bool _ok13 = false;
                    do {
                        if (peg_at_end(p) || !is_del(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok13 = true;
                    } while(0);
                    peg_restore(p, _m13);
                    if (_ok13) break;
                }
                if (peg_at_end(p)) break;
                peg_advance(p);
            _choice_done2:;
            }
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    if (!peg_match(p, "\"")) return false;
    if (!peg_match(p, "\"")) return false;
    if (!peg_match(p, "\"")) return false;
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_literal_string(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match(p, "'")) return false;
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    if (!peg_match(p, "'")) break;
                    _ok1 = true;
                } while(0);
                peg_restore(p, _m1);
                if (_ok1) break;
            }
            {
                peg_mark _m2 = peg_save(p);
                bool _ok2 = false;
                do {
                    if (peg_at_end(p) || !is_bad_ctl(peg_peek_char(p))) break;
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
                    if (peg_at_end(p) || !is_del(peg_peek_char(p))) break;
                    peg_advance(p);
                    _ok3 = true;
                } while(0);
                peg_restore(p, _m3);
                if (_ok3) break;
            }
            if (peg_at_end(p)) break;
            peg_advance(p);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    if (!peg_match(p, "'")) return false;
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_ml_literal_string(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    if (!peg_match(p, "'")) return false;
    if (!peg_match(p, "'")) return false;
    if (!peg_match(p, "'")) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            if (peg_peek_at(p, "\r")) {
                if (!peg_match(p, "\r")) break;
                if (!peg_match(p, "\n")) break;
            } else {
                if (!peg_match(p, "\n")) break;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            {
                peg_mark _m2 = peg_save(p);
                {
                    bool _ok3 = false;
                    do {
                        if (!peg_match(p, "\r")) break;
                        if (!peg_match(p, "\n")) break;
                        _ok3 = true;
                    } while(0);
                    if (!_ok3) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok4 = false;
                    do {
                        if (!peg_match(p, "\n")) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok5 = false;
                    do {
                        if (!peg_match(p, "'")) break;
                        if (!peg_match(p, "'")) break;
                        {
                            peg_mark _m6 = peg_save(p);
                            bool _ok6 = false;
                            do {
                                if (!peg_match(p, "'")) break;
                                _ok6 = true;
                            } while(0);
                            peg_restore(p, _m6);
                            if (_ok6) break;
                        }
                        _ok5 = true;
                    } while(0);
                    if (!_ok5) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    bool _ok7 = false;
                    do {
                        if (!peg_match(p, "'")) break;
                        {
                            peg_mark _m8 = peg_save(p);
                            bool _ok8 = false;
                            do {
                                if (!peg_match(p, "'")) break;
                                _ok8 = true;
                            } while(0);
                            peg_restore(p, _m8);
                            if (_ok8) break;
                        }
                        _ok7 = true;
                    } while(0);
                    if (!_ok7) {
                        peg_restore(p, _m2);
                    } else goto _choice_done2;
                }
                {
                    peg_mark _m9 = peg_save(p);
                    bool _ok9 = false;
                    do {
                        if (!peg_match(p, "'")) break;
                        _ok9 = true;
                    } while(0);
                    peg_restore(p, _m9);
                    if (_ok9) break;
                }
                {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_bad_ctl(peg_peek_char(p))) break;
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
                        if (peg_at_end(p) || !is_del(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok11 = true;
                    } while(0);
                    peg_restore(p, _m11);
                    if (_ok11) break;
                }
                if (peg_at_end(p)) break;
                peg_advance(p);
            _choice_done2:;
            }
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    if (!peg_match(p, "'")) return false;
    if (!peg_match(p, "'")) return false;
    if (!peg_match(p, "'")) return false;
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_tok_date_time(peg_state* p, peg_span* out) {
    const char* _start = peg_pos(p);
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (!peg_match(p, "-")) break;
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (!peg_match(p, "-")) break;
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        if (peg_peek_at(p, "T")) {
                            if (!peg_match(p, "T")) break;
                        } else                         if (peg_peek_at(p, "t")) {
                            if (!peg_match(p, "t")) break;
                        } else {
                            if (!peg_match(p, " ")) break;
                        }
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        if (!peg_match(p, ":")) break;
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        if (!peg_match(p, ":")) break;
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        {
                            peg_mark _m3 = peg_save(p);
                            bool _ok3 = false;
                            do {
                                if (!peg_match(p, ".")) break;
                                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
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
                            if (!_ok3) peg_restore(p, _m3);
                        }
                        {
                            peg_mark _m5 = peg_save(p);
                            bool _ok5 = false;
                            do {
                                {
                                    peg_mark _m6 = peg_save(p);
                                    {
                                        bool _ok7 = false;
                                        do {
                                            if (!peg_match(p, "Z")) break;
                                            _ok7 = true;
                                        } while(0);
                                        if (!_ok7) {
                                            peg_restore(p, _m6);
                                        } else goto _choice_done6;
                                    }
                                    {
                                        bool _ok8 = false;
                                        do {
                                            if (!peg_match(p, "z")) break;
                                            _ok8 = true;
                                        } while(0);
                                        if (!_ok8) {
                                            peg_restore(p, _m6);
                                        } else goto _choice_done6;
                                    }
                                    if (peg_peek_at(p, "+")) {
                                        if (!peg_match(p, "+")) break;
                                    } else {
                                        if (!peg_match(p, "-")) break;
                                    }
                                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    if (!peg_match(p, ":")) break;
                                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                    if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                                    peg_advance(p);
                                _choice_done6:;
                                }
                                _ok5 = true;
                            } while(0);
                            if (!_ok5) peg_restore(p, _m5);
                        }
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) peg_restore(p, _m2);
                }
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        if (!peg_match(p, ":")) return false;
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        if (!peg_match(p, ":")) return false;
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) return false;
        peg_advance(p);
        {
            peg_mark _m9 = peg_save(p);
            bool _ok9 = false;
            do {
                if (!peg_match(p, ".")) break;
                if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                peg_advance(p);
                for (;;) {
                    peg_mark _m10 = peg_save(p);
                    bool _ok10 = false;
                    do {
                        if (peg_at_end(p) || !is_digit(peg_peek_char(p))) break;
                        peg_advance(p);
                        _ok10 = true;
                    } while(0);
                    if (!_ok10) { peg_restore(p, _m10); break; }
                }
                _ok9 = true;
            } while(0);
            if (!_ok9) peg_restore(p, _m9);
        }
    _choice_done0:;
    }
    if (out) { out->ptr = _start; out->len = (int)(peg_pos(p) - _start); }
    return true;
}

static bool toml_parse_toml(peg_state* p) {
    toml_item_t** items = NULL; int ic = 0;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!toml_parse_expression(p, &items, &ic)) break;
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    for (;;) {
        peg_mark _m1 = peg_save(p);
        bool _ok1 = false;
        do {
            peg_skip(p);
            if (!toml_parse_newline(p)) break;
            {
                peg_mark _m2 = peg_save(p);
                bool _ok2 = false;
                do {
                    peg_skip(p);
                    if (!toml_parse_expression(p, &items, &ic)) break;
                    _ok2 = true;
                } while(0);
                if (!_ok2) peg_restore(p, _m2);
            }
            _ok1 = true;
        } while(0);
        if (!_ok1) { peg_restore(p, _m1); break; }
    }
    peg_skip(p);
         if (!peg_at_end(p)) { toml_report_parse_error(p); return false; }
    CTX->result = toml_document(A, items, ic);
    return true;
}

static bool toml_parse_expression(peg_state* p, toml_item_t*** items, int* ic) {
    toml_item_t* item; toml_key_path_t* path; toml_key_value_t* kv;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!peg_match(p, "[[")) break;
                peg_skip(p);
                if (!toml_parse_key_path(p, &path)) break;
                peg_skip(p);
                if (!peg_match(p, "]]")) break;
                {
                    peg_mark _m2 = peg_save(p);
                    bool _ok2 = false;
                    do {
                        peg_skip(p);
                        if (!toml_parse_comment(p)) break;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) peg_restore(p, _m2);
                }
                item = toml_array_table_header(A, path);
           *items = (toml_item_t**)tpush(A, (void**)(*items), ic, item);
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
                if (!peg_match(p, "[")) break;
                peg_skip(p);
                if (!toml_parse_key_path(p, &path)) break;
                peg_skip(p);
                if (!peg_match(p, "]")) break;
                {
                    peg_mark _m4 = peg_save(p);
                    bool _ok4 = false;
                    do {
                        peg_skip(p);
                        if (!toml_parse_comment(p)) break;
                        _ok4 = true;
                    } while(0);
                    if (!_ok4) peg_restore(p, _m4);
                }
                item = toml_table_header(A, path);
           *items = (toml_item_t**)tpush(A, (void**)(*items), ic, item);
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
                if (!toml_parse_key_value(p, &kv)) break;
                {
                    peg_mark _m6 = peg_save(p);
                    bool _ok6 = false;
                    do {
                        peg_skip(p);
                        if (!toml_parse_comment(p)) break;
                        _ok6 = true;
                    } while(0);
                    if (!_ok6) peg_restore(p, _m6);
                }
                item = toml_kv_item(A, kv);
           *items = (toml_item_t**)tpush(A, (void**)(*items), ic, item);
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!toml_parse_comment(p)) return false;
    _choice_done0:;
    }
    return true;
}

static bool toml_parse_newline(peg_state* p) {
    peg_skip(p);
    if (!toml_tok_newline(p, NULL)) return false;
    return true;
}

static bool toml_parse_comment(peg_state* p) {
    peg_skip(p);
    if (!toml_tok_comment(p, NULL)) return false;
    return true;
}

static bool toml_parse_key_path(peg_state* p, toml_key_path_t** result) {
    toml_key_part_t* part;
       toml_key_part_t** parts = NULL; int pc = 0;
    peg_skip(p);
    if (!toml_parse_key_part(p, &part)) return false;
    parts = (toml_key_part_t**)tpush(A, (void**)parts, &pc, part);
    for (;;) {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!peg_match(p, ".")) break;
            peg_skip(p);
            if (!toml_parse_key_part(p, &part)) break;
            parts = (toml_key_part_t**)tpush(A, (void**)parts, &pc, part);
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    *result = toml_key_path(A, parts, pc);
    return true;
}

static bool toml_parse_key_part(peg_state* p, toml_key_part_t** result) {
    peg_span s; const char* qs;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                peg_skip(p);
                if (!toml_tok_bare_key(p, &s)) break;
                *result = toml_bare_key(A, tdup(A, s));
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
                if (!toml_tok_basic_string(p, &s)) break;
                qs = tom_decode_basic_span(A, s);
                                 if (!qs) return false;
                                 *result = toml_quoted_key(A, qs);
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        peg_skip(p);
        if (!toml_tok_literal_string(p, &s)) return false;
        qs = tom_literal_span(A, s, 1);
                                 *result = toml_quoted_key(A, qs);
    _choice_done0:;
    }
    return true;
}

static bool toml_parse_key_value(peg_state* p, toml_key_value_t** result) {
    toml_key_path_t* key;
       toml_value_t* v;
       toml_srcloc _loc;
    _loc = TLOC;
    peg_skip(p);
    if (!toml_parse_key_path(p, &key)) return false;
    peg_skip(p);
    if (!peg_match(p, "=")) return false;
    peg_skip(p);
    if (!toml_parse_value(p, &v)) return false;
    *result = toml_key_value(A, key, v);
         (*result)->loc = _loc;
    return true;
}

static bool toml_parse_value(peg_state* p, toml_value_t** result) {
    peg_span s;
       toml_date_time_t* dt;
       toml_value_t** elems = NULL; int ec = 0;
       toml_key_value_t** entries = NULL; int kc = 0;
       const char* str;
       toml_srcloc _loc;
    {
        peg_mark _m0 = peg_save(p);
        {
            bool _ok1 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_ml_basic_string(p, &s)) break;
                str = tom_decode_ml_basic_span(A, s);
           if (!str) return false;
           *result = toml_vstr(A, str);
           (*result)->loc = _loc;
                _ok1 = true;
            } while(0);
            if (!_ok1) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok2 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_ml_literal_string(p, &s)) break;
                str = tom_literal_span(A, s, 3);
           *result = toml_vstr(A, str);
           (*result)->loc = _loc;
                _ok2 = true;
            } while(0);
            if (!_ok2) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok3 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_basic_string(p, &s)) break;
                str = tom_decode_basic_span(A, s);
           if (!str) return false;
           *result = toml_vstr(A, str);
           (*result)->loc = _loc;
                _ok3 = true;
            } while(0);
            if (!_ok3) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok4 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_literal_string(p, &s)) break;
                str = tom_literal_span(A, s, 1);
           *result = toml_vstr(A, str);
           (*result)->loc = _loc;
                _ok4 = true;
            } while(0);
            if (!_ok4) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok5 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!peg_match(p, "true")) break;
                *result = toml_vbool(A, true);
           (*result)->loc = _loc;
                _ok5 = true;
            } while(0);
            if (!_ok5) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok6 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!peg_match(p, "false")) break;
                *result = toml_vbool(A, false);
           (*result)->loc = _loc;
                _ok6 = true;
            } while(0);
            if (!_ok6) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok7 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_date_time(p, &s)) break;
                dt = tom_parse_datetime(A, s);
           *result = toml_vdate_time(A, dt);
           (*result)->loc = _loc;
                _ok7 = true;
            } while(0);
            if (!_ok7) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok8 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_float(p, &s)) break;
                *result = toml_vfloat(A, tom_parse_float(s));
           (*result)->loc = _loc;
                _ok8 = true;
            } while(0);
            if (!_ok8) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok9 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!toml_tok_integer(p, &s)) break;
                *result = toml_vint(A, tom_parse_int(s));
           (*result)->loc = _loc;
                _ok9 = true;
            } while(0);
            if (!_ok9) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        {
            bool _ok10 = false;
            do {
                _loc = TLOC;
                peg_skip(p);
                if (!peg_match(p, "[")) break;
                peg_skip(p);
                if (!toml_parse_array_elems(p, &elems, &ec)) break;
                peg_skip(p);
                if (!peg_match(p, "]")) break;
                *result = toml_varray(A, elems, ec);
           (*result)->loc = _loc;
                _ok10 = true;
            } while(0);
            if (!_ok10) {
                peg_restore(p, _m0);
            } else goto _choice_done0;
        }
        _loc = TLOC;
        peg_skip(p);
        if (!peg_match(p, "{")) return false;
        peg_skip(p);
        if (!toml_parse_inline_table_entries(p, &entries, &kc)) return false;
        peg_skip(p);
        if (!peg_match(p, "}")) return false;
        *result = toml_vinline_table(A, entries, kc);
           (*result)->loc = _loc;
    _choice_done0:;
    }
    return true;
}

static bool toml_parse_array_elems(peg_state* p, toml_value_t*** elems, int* ec) {
    toml_value_t* elem;
    peg_skip(p);
    if (!toml_parse_array_ws_cn(p)) return false;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!toml_parse_value(p, &elem)) break;
            *elems = (toml_value_t**)tpush(A, (void**)(*elems), ec, elem);
            peg_skip(p);
            if (!toml_parse_array_ws_cn(p)) break;
            for (;;) {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    peg_skip(p);
                    if (!peg_match(p, ",")) break;
                    peg_skip(p);
                    if (!toml_parse_array_ws_cn(p)) break;
                    {
                        peg_mark _m2 = peg_save(p);
                        bool _ok2 = false;
                        do {
                            peg_skip(p);
                            if (!toml_parse_value(p, &elem)) break;
                            *elems = (toml_value_t**)tpush(A, (void**)(*elems), ec, elem);
                            peg_skip(p);
                            if (!toml_parse_array_ws_cn(p)) break;
                            _ok2 = true;
                        } while(0);
                        if (!_ok2) peg_restore(p, _m2);
                    }
                    _ok1 = true;
                } while(0);
                if (!_ok1) { peg_restore(p, _m1); break; }
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    return true;
}

static bool toml_parse_array_ws_cn(peg_state* p) {
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
                        if (!toml_parse_newline(p)) break;
                        _ok2 = true;
                    } while(0);
                    if (!_ok2) {
                        peg_restore(p, _m1);
                    } else goto _choice_done1;
                }
                peg_skip(p);
                if (!toml_parse_comment(p)) break;
                peg_skip(p);
                if (!toml_parse_newline(p)) break;
            _choice_done1:;
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) { peg_restore(p, _m0); break; }
    }
    return true;
}

static bool toml_parse_inline_table_entries(peg_state* p, toml_key_value_t*** entries, int* kc) {
    toml_key_value_t* kv;
    {
        peg_mark _m0 = peg_save(p);
        bool _ok0 = false;
        do {
            peg_skip(p);
            if (!toml_parse_key_value(p, &kv)) break;
            *entries = (toml_key_value_t**)tpush(A, (void**)(*entries), kc, kv);
            for (;;) {
                peg_mark _m1 = peg_save(p);
                bool _ok1 = false;
                do {
                    peg_skip(p);
                    if (!peg_match(p, ",")) break;
                    peg_skip(p);
                    if (!toml_parse_key_value(p, &kv)) break;
                    *entries = (toml_key_value_t**)tpush(A, (void**)(*entries), kc, kv);
                    _ok1 = true;
                } while(0);
                if (!_ok1) { peg_restore(p, _m1); break; }
            }
            _ok0 = true;
        } while(0);
        if (!_ok0) peg_restore(p, _m0);
    }
    return true;
}

