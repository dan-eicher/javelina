/* toml_doc.h — resolved TOML document + typed accessor API.
 *
 * The parser (generated from grammar/toml.peg) emits a flat item
 * stream matching the ABNF. This header defines the resolved view:
 *
 *   toml_doc_t — resolved document, root table, plus any errors
 *                encountered during resolution (re-definition,
 *                collision between key and table, inline-table
 *                mutation, etc., per TOML v1.0 §5 / §6).
 *
 *   toml_tbl_t / toml_val_t — resolved table and value.
 *
 * Typical consumer flow:
 *
 *   toml_doc_t* doc = toml_resolve(parsed_doc, arena);
 *   if (toml_doc_has_errors(doc)) { ...handle errors...; return; }
 *   const toml_tbl_t* root = toml_doc_root(doc);
 *   const toml_val_t* v = toml_tbl_get(root, "package.name");
 *   const char* name;
 *   if (toml_val_as_string(v, &name)) { ... }
 *
 * Lookups accept dotted keys ("a.b.c") as a single argument; all
 * intermediate lookups walk through nested tables. Type queries
 * return false if the value's type doesn't match; consumers decide
 * whether that's an error or a try-next-shape.
 *
 * Memory: every node is arena-allocated. The consumer supplies the
 * arena to toml_resolve; freeing the arena frees the doc.
 */

#ifndef YOCTOJC_TOML_DOC_H
#define YOCTOJC_TOML_DOC_H

#include <stdbool.h>
#include <stdint.h>
#include "bbq_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration — the parser's AST type; consumers should
 * not reach into it directly. */
struct toml_document_t;
struct toml_srcloc;

/* Public date-time shape. Covers all four TOML v1.0 date-time
 * flavors (offset-datetime, local-datetime, local-date,
 * local-time) via the has_* flags:
 *   - offset-datetime: has_date && has_time && has_offset
 *   - local-datetime : has_date && has_time && !has_offset
 *   - local-date     : has_date && !has_time
 *   - local-time     : !has_date && has_time
 *
 * nanosec is 0..999_999_999. tz_offset_min is minutes east of
 * UTC, -720..+840; Z → 0. Fields outside the "present" set per
 * has_* are zero. */
typedef struct toml_dt {
    int32_t year, month, day;
    int32_t hour, minute, second;
    int32_t nanosec;
    int32_t tz_offset_min;
    bool    has_date, has_time, has_offset;
} toml_dt_t;

/* Format a human-readable parse-error description (caret under
 * the failure position) into the caller's buffer. Returns buf
 * so the call chains into other formatters. Consumers call this
 * when toml_parser_parse returned false. The pegc runtime's
 * peg_state type is used directly; callers will already have
 * included peg_runtime.h to drive the parser itself. */
struct peg_state;
const char* toml_format_parse_error(const void* peg_state_ptr,
                                     char* buf, int buf_cap);

/* Resolved value type tag. */
typedef enum {
    TOML_VT_STRING,
    TOML_VT_INT,
    TOML_VT_FLOAT,
    TOML_VT_BOOL,
    TOML_VT_DATETIME,
    TOML_VT_ARRAY,
    TOML_VT_TABLE,
} toml_val_type_t;

typedef struct toml_tbl_entry toml_tbl_entry_t;
typedef struct toml_tbl       toml_tbl_t;
typedef struct toml_val       toml_val_t;
typedef struct toml_err       toml_err_t;
typedef struct toml_doc       toml_doc_t;

/* Source location for diagnostics. Mirrors the parser's shape. */
typedef struct toml_doc_srcloc {
    int line;
    int col;
} toml_doc_srcloc_t;

/* A resolved value. Discriminated by `type`. */
struct toml_val {
    toml_val_type_t   type;
    toml_doc_srcloc_t loc;
    union {
        const char*  s;     /* TOML_VT_STRING, owned by doc arena */
        int64_t      i;     /* TOML_VT_INT */
        double       f;     /* TOML_VT_FLOAT */
        bool         b;     /* TOML_VT_BOOL */
        const toml_dt_t* dt;  /* TOML_VT_DATETIME */
        struct {
            const toml_val_t** items;
            int count;
        } array;            /* TOML_VT_ARRAY */
        toml_tbl_t*  table; /* TOML_VT_TABLE — nested table or inline-table */
    } u;
};

/* Entry in a table: key + value pair. Insertion-order preserved. */
struct toml_tbl_entry {
    const char*       key;
    toml_val_t*       value;
    toml_doc_srcloc_t loc;
};

/* Resolved table — preserves insertion order of entries, supports
 * dotted-key lookups through nested tables. */
struct toml_tbl {
    toml_tbl_entry_t** entries;  /* bbq_vec of entry pointers */
    int entries_count;
    /* Resolver metadata — used to enforce TOML v1.0 §5/§6 rules. */
    bool explicit_header;        /* `[x]` or `[[x]]` header landed here */
    bool inline_defined;         /* produced by `{...}` — immutable */
    bool from_dotted_intermediate; /* created as intermediate for dotted key */
};

/* Resolution error. Carries a line/col pointing at the offending
 * item and a short message. Consumers translate these into their
 * own diag format. */
struct toml_err {
    toml_doc_srcloc_t loc;
    char              message[256];
};

/* Resolved document. Root is always a table (possibly empty). On
 * error, partial state may still be set — consumers should check
 * toml_doc_has_errors before using the root. */
struct toml_doc {
    toml_tbl_t*   root;
    toml_err_t**  errors;
    int           errors_count;
};

/* ── Entry points ──────────────────────────────────────────────── */

/* One-shot: parse TOML source → resolved document. Callers
 * supply an arena that owns every allocation reachable from the
 * returned doc. On parse failure, sets `doc->errors` with a
 * caret-annotated message (see toml_format_parse_error) and the
 * root table is empty but present, so accessor-API calls on the
 * partial doc are safe.
 *
 * This is the preferred entry point — consumers should not need
 * to deal with the pegc-generated parser types. */
toml_doc_t* toml_parse(const char* src, int len, bbq_arena* arena);

/* Resolve a pre-parsed flat-item stream into a document. Only
 * needed by internal uses that already have a toml_document_t*
 * from the generated parser (e.g. the conformance driver that
 * wants parser-level error access). Most callers use toml_parse. */
toml_doc_t* toml_resolve(const struct toml_document_t* parsed, bbq_arena* arena);

/* True if resolution or the preceding parse saw any errors. */
bool toml_doc_has_errors(const toml_doc_t* doc);

/* Root table of the document. Never NULL post-resolve. */
const toml_tbl_t* toml_doc_root(const toml_doc_t* doc);

/* Read-only iteration over accumulated errors. */
int toml_doc_error_count(const toml_doc_t* doc);
const toml_err_t* toml_doc_error_at(const toml_doc_t* doc, int i);

/* ── Table accessors ───────────────────────────────────────────── */

/* Look up a value by simple or dotted key. Returns NULL if any
 * segment is missing or not a table on the way down. */
const toml_val_t* toml_tbl_get(const toml_tbl_t* t, const char* dotted_key);

/* Iterate the entries of a table (insertion order preserved). */
int toml_tbl_entry_count(const toml_tbl_t* t);
const toml_tbl_entry_t* toml_tbl_entry_at(const toml_tbl_t* t, int i);

/* ── Value type queries ────────────────────────────────────────── */

toml_val_type_t toml_val_type(const toml_val_t* v);

/* Out-param getters — return false if v is NULL or type mismatch.
 * Consumer decides whether mismatch is an error or a "try next." */
bool toml_val_as_string (const toml_val_t* v, const char** out);
bool toml_val_as_int    (const toml_val_t* v, int64_t*     out);
bool toml_val_as_float  (const toml_val_t* v, double*      out);
bool toml_val_as_bool   (const toml_val_t* v, bool*        out);
const toml_dt_t* toml_val_as_datetime(const toml_val_t* v);
const toml_tbl_t* toml_val_as_table(const toml_val_t* v);

/* Array access. */
int  toml_val_array_count(const toml_val_t* v);
const toml_val_t* toml_val_array_at(const toml_val_t* v, int i);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* YOCTOJC_TOML_DOC_H */
