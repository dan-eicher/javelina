#ifndef JNM_TYPES_H
#define JNM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bbq_runtime.h"

typedef struct jnm_n_name jnm_n_name_t;
typedef struct jnm_n_map jnm_n_map_t;
typedef struct jnm_n_skip jnm_n_skip_t;
typedef struct jnm_n_indirect_map jnm_n_indirect_map_t;
typedef struct jnm_name_data jnm_name_data_t;
typedef struct jnm_n_module_name jnm_n_module_name_t;
typedef struct jnm_n_assoc jnm_n_assoc_t;
typedef struct jnm_n_indirect_assoc jnm_n_indirect_assoc_t;
typedef struct jnm_n_subsec jnm_n_subsec_t;

struct jnm_n_name {
    uint32_t count;
    bbq_bytes_t bytes;
};

struct jnm_n_map {
    uint32_t count;
    struct { jnm_n_assoc_t* items; size_t count; } entries;
};

struct jnm_n_skip {
    bbq_bytes_t data;
};

struct jnm_n_indirect_map {
    uint32_t count;
    struct { jnm_n_indirect_assoc_t* items; size_t count; } entries;
};

struct jnm_name_data {
    struct { jnm_n_subsec_t* items; size_t count; } subsecs;
};

struct jnm_n_module_name {
    jnm_n_name_t name;
};

struct jnm_n_assoc {
    uint32_t idx;
    jnm_n_name_t name;
};

struct jnm_n_indirect_assoc {
    uint32_t idx;
    jnm_n_map_t map;
};

struct jnm_n_subsec {
    uint8_t id;
    uint32_t size;
    struct { int tag; union { jnm_n_module_name_t case_0; jnm_n_map_t case_1; jnm_n_indirect_map_t case_2; jnm_n_map_t case_3; jnm_n_indirect_map_t case_4; jnm_n_map_t case_5; jnm_n_skip_t default_val; } u; } body;
};


// ── Free functions ──

#include <stdlib.h>

static inline void jnm_n_name_free(jnm_n_name_t* p);
static inline void jnm_n_map_free(jnm_n_map_t* p);
static inline void jnm_n_skip_free(jnm_n_skip_t* p);
static inline void jnm_n_indirect_map_free(jnm_n_indirect_map_t* p);
static inline void jnm_name_data_free(jnm_name_data_t* p);
static inline void jnm_n_module_name_free(jnm_n_module_name_t* p);
static inline void jnm_n_assoc_free(jnm_n_assoc_t* p);
static inline void jnm_n_indirect_assoc_free(jnm_n_indirect_assoc_t* p);
static inline void jnm_n_subsec_free(jnm_n_subsec_t* p);

static inline void jnm_n_name_free(jnm_n_name_t* p) {
    (void)p;
    free((void*)p->bytes.data);
}

static inline void jnm_n_map_free(jnm_n_map_t* p) {
    (void)p;
    for (size_t i = 0; i < p->entries.count; i++)
        jnm_n_assoc_free(&p->entries.items[i]);
    free(p->entries.items);
}

static inline void jnm_n_skip_free(jnm_n_skip_t* p) {
    (void)p;
    free((void*)p->data.data);
}

static inline void jnm_n_indirect_map_free(jnm_n_indirect_map_t* p) {
    (void)p;
    for (size_t i = 0; i < p->entries.count; i++)
        jnm_n_indirect_assoc_free(&p->entries.items[i]);
    free(p->entries.items);
}

static inline void jnm_name_data_free(jnm_name_data_t* p) {
    (void)p;
    for (size_t i = 0; i < p->subsecs.count; i++)
        jnm_n_subsec_free(&p->subsecs.items[i]);
    free(p->subsecs.items);
}

static inline void jnm_n_module_name_free(jnm_n_module_name_t* p) {
    (void)p;
    jnm_n_name_free(&p->name);
}

static inline void jnm_n_assoc_free(jnm_n_assoc_t* p) {
    (void)p;
    jnm_n_name_free(&p->name);
}

static inline void jnm_n_indirect_assoc_free(jnm_n_indirect_assoc_t* p) {
    (void)p;
    jnm_n_map_free(&p->map);
}

static inline void jnm_n_subsec_free(jnm_n_subsec_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 0:
        jnm_n_module_name_free(&p->body.u.case_0);
        break;
    case 1:
        jnm_n_map_free(&p->body.u.case_1);
        break;
    case 2:
        jnm_n_indirect_map_free(&p->body.u.case_2);
        break;
    case 4:
        jnm_n_map_free(&p->body.u.case_3);
        break;
    case 10:
        jnm_n_indirect_map_free(&p->body.u.case_4);
        break;
    case 11:
        jnm_n_map_free(&p->body.u.case_5);
        break;
    default:
        jnm_n_skip_free(&p->body.u.default_val);
        break;
    }
}

#endif /* JNM_TYPES_H */
