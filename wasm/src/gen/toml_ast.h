/* ============================================================
 * Auto-generated from ASDL — do not edit by hand.
 * ============================================================ */
#ifndef TOML_AST_H
#define TOML_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "bbq_arena.h"

/* ── Tag enums for sum types ────────────────────────────── */

typedef enum {
    TOML_BAREKEY,
    TOML_QUOTEDKEY
} toml_key_part_t_tag;

typedef enum {
    TOML_VSTR,
    TOML_VINT,
    TOML_VFLOAT,
    TOML_VBOOL,
    TOML_VDATETIME,
    TOML_VARRAY,
    TOML_VINLINETABLE
} toml_value_t_tag;

typedef enum {
    TOML_KVITEM,
    TOML_TABLEHEADER,
    TOML_ARRAYTABLEHEADER
} toml_item_t_tag;

/* ── Forward declarations ───────────────────────────────── */

typedef struct toml_key_part_t toml_key_part_t;
typedef struct toml_key_path_t toml_key_path_t;
typedef struct toml_date_time_t toml_date_time_t;
typedef struct toml_value_t toml_value_t;
typedef struct toml_key_value_t toml_key_value_t;
typedef struct toml_item_t toml_item_t;
typedef struct toml_document_t toml_document_t;

/* ── Source location ────────────────────────────────────── */

typedef struct {
    const char* file;
    int line;
    int col;
} toml_srcloc;

/* ── Multi-constructor sum types (tagged unions) ────────── */

struct toml_key_part_t {
    toml_key_part_t_tag tag;
    toml_srcloc loc;
    union {
        struct {
            const char* name;
        } bare_key;
        struct {
            const char* text;
        } quoted_key;
    };
};

struct toml_value_t {
    toml_value_t_tag tag;
    toml_srcloc loc;
    union {
        struct {
            const char* text;
        } vstr;
        struct {
            int64_t n;
        } vint;
        struct {
            double v;
        } vfloat;
        struct {
            bool b;
        } vbool;
        struct {
            toml_date_time_t* dt;
        } vdate_time;
        struct {
            toml_value_t** elems;
            int elems_count;
        } varray;
        struct {
            toml_key_value_t** entries;
            int entries_count;
        } vinline_table;
    };
};

struct toml_item_t {
    toml_item_t_tag tag;
    toml_srcloc loc;
    union {
        struct {
            toml_key_value_t* kv;
        } kv_item;
        struct {
            toml_key_path_t* path;
        } table_header;
        struct {
            toml_key_path_t* path;
        } array_table_header;
    };
};

/* ── Single-constructor sum types (plain structs) ───────── */

struct toml_key_path_t {
    toml_srcloc loc;
    toml_key_part_t** parts;
    int parts_count;
};

struct toml_date_time_t {
    toml_srcloc loc;
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t nanosec;
    int32_t tz_offset_min;
    bool has_date;
    bool has_time;
    bool has_offset;
};

struct toml_key_value_t {
    toml_srcloc loc;
    toml_key_path_t* key;
    toml_value_t* v;
};

struct toml_document_t {
    toml_srcloc loc;
    toml_item_t** items;
    int items_count;
};

/* ── Product types ──────────────────────────────────────── */

/* ── Constructor functions ──────────────────────────────── */

static inline toml_key_part_t* toml_bare_key(bbq_arena* _a, const char* name) {
    toml_key_part_t* _n = (toml_key_part_t*)bbq_arena_alloc(_a, sizeof(toml_key_part_t));
    _n->tag = TOML_BAREKEY;
    _n->bare_key.name = name;
    return _n;
}

static inline toml_key_part_t* toml_quoted_key(bbq_arena* _a, const char* text) {
    toml_key_part_t* _n = (toml_key_part_t*)bbq_arena_alloc(_a, sizeof(toml_key_part_t));
    _n->tag = TOML_QUOTEDKEY;
    _n->quoted_key.text = text;
    return _n;
}

static inline toml_key_path_t* toml_key_path(bbq_arena* _a, toml_key_part_t** parts, int parts_count) {
    toml_key_path_t* _n = (toml_key_path_t*)bbq_arena_alloc(_a, sizeof(toml_key_path_t));
    _n->parts = parts;
    _n->parts_count = parts_count;
    return _n;
}

static inline toml_date_time_t* toml_date_time(bbq_arena* _a, int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second, int32_t nanosec, int32_t tz_offset_min, bool has_date, bool has_time, bool has_offset) {
    toml_date_time_t* _n = (toml_date_time_t*)bbq_arena_alloc(_a, sizeof(toml_date_time_t));
    _n->year = year;
    _n->month = month;
    _n->day = day;
    _n->hour = hour;
    _n->minute = minute;
    _n->second = second;
    _n->nanosec = nanosec;
    _n->tz_offset_min = tz_offset_min;
    _n->has_date = has_date;
    _n->has_time = has_time;
    _n->has_offset = has_offset;
    return _n;
}

static inline toml_value_t* toml_vstr(bbq_arena* _a, const char* text) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VSTR;
    _n->vstr.text = text;
    return _n;
}

static inline toml_value_t* toml_vint(bbq_arena* _a, int64_t n) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VINT;
    _n->vint.n = n;
    return _n;
}

static inline toml_value_t* toml_vfloat(bbq_arena* _a, double v) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VFLOAT;
    _n->vfloat.v = v;
    return _n;
}

static inline toml_value_t* toml_vbool(bbq_arena* _a, bool b) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VBOOL;
    _n->vbool.b = b;
    return _n;
}

static inline toml_value_t* toml_vdate_time(bbq_arena* _a, toml_date_time_t* dt) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VDATETIME;
    _n->vdate_time.dt = dt;
    return _n;
}

static inline toml_value_t* toml_varray(bbq_arena* _a, toml_value_t** elems, int elems_count) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VARRAY;
    _n->varray.elems = elems;
    _n->varray.elems_count = elems_count;
    return _n;
}

static inline toml_value_t* toml_vinline_table(bbq_arena* _a, toml_key_value_t** entries, int entries_count) {
    toml_value_t* _n = (toml_value_t*)bbq_arena_alloc(_a, sizeof(toml_value_t));
    _n->tag = TOML_VINLINETABLE;
    _n->vinline_table.entries = entries;
    _n->vinline_table.entries_count = entries_count;
    return _n;
}

static inline toml_key_value_t* toml_key_value(bbq_arena* _a, toml_key_path_t* key, toml_value_t* v) {
    toml_key_value_t* _n = (toml_key_value_t*)bbq_arena_alloc(_a, sizeof(toml_key_value_t));
    _n->key = key;
    _n->v = v;
    return _n;
}

static inline toml_item_t* toml_kv_item(bbq_arena* _a, toml_key_value_t* kv) {
    toml_item_t* _n = (toml_item_t*)bbq_arena_alloc(_a, sizeof(toml_item_t));
    _n->tag = TOML_KVITEM;
    _n->kv_item.kv = kv;
    return _n;
}

static inline toml_item_t* toml_table_header(bbq_arena* _a, toml_key_path_t* path) {
    toml_item_t* _n = (toml_item_t*)bbq_arena_alloc(_a, sizeof(toml_item_t));
    _n->tag = TOML_TABLEHEADER;
    _n->table_header.path = path;
    return _n;
}

static inline toml_item_t* toml_array_table_header(bbq_arena* _a, toml_key_path_t* path) {
    toml_item_t* _n = (toml_item_t*)bbq_arena_alloc(_a, sizeof(toml_item_t));
    _n->tag = TOML_ARRAYTABLEHEADER;
    _n->array_table_header.path = path;
    return _n;
}

static inline toml_document_t* toml_document(bbq_arena* _a, toml_item_t** items, int items_count) {
    toml_document_t* _n = (toml_document_t*)bbq_arena_alloc(_a, sizeof(toml_document_t));
    _n->items = items;
    _n->items_count = items_count;
    return _n;
}

#endif /* TOML_AST_H */
