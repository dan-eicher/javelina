#ifndef JAV_TYPES_H
#define JAV_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bbq_runtime.h"

typedef struct jav_name jav_name_t;
typedef struct jav_tag_type jav_tag_type_t;
typedef struct jav_limits jav_limits_t;
typedef struct jav_idx_vec jav_idx_vec_t;
typedef struct jav_catch jav_catch_t;
typedef struct jav_byte_vec jav_byte_vec_t;
typedef struct jav_expr jav_expr_t;
typedef struct jav_expr_vec jav_expr_vec_t;
typedef struct jav_heap_type jav_heap_type_t;
typedef struct jav_idx_imm jav_idx_imm_t;
typedef struct jav_else_clause jav_else_clause_t;
typedef struct jav_i64_imm jav_i64_imm_t;
typedef struct jav_f32_imm jav_f32_imm_t;
typedef struct jav_v128_imm jav_v128_imm_t;
typedef struct jav_data_count_section jav_data_count_section_t;
typedef struct jav_heap_imm jav_heap_imm_t;
typedef struct jav_table_section jav_table_section_t;
typedef struct jav_f64_imm jav_f64_imm_t;
typedef struct jav_import_section jav_import_section_t;
typedef struct jav_global_section jav_global_section_t;
typedef struct jav_element_section jav_element_section_t;
typedef struct jav_br_on_cast jav_br_on_cast_t;
typedef struct jav_export_section jav_export_section_t;
typedef struct jav_tag_section jav_tag_section_t;
typedef struct jav_select_t jav_select_t_t;
typedef struct jav_start_section jav_start_section_t;
typedef struct jav_idx2_imm jav_idx2_imm_t;
typedef struct jav_empty jav_empty_t;
typedef struct jav_mem_arg jav_mem_arg_t;
typedef struct jav_code_section jav_code_section_t;
typedef struct jav_struct_type jav_struct_type_t;
typedef struct jav_memory_section jav_memory_section_t;
typedef struct jav_function_section jav_function_section_t;
typedef struct jav_rec_group jav_rec_group_t;
typedef struct jav_type_section jav_type_section_t;
typedef struct jav_func_type jav_func_type_t;
typedef struct jav_i32_imm jav_i32_imm_t;
typedef struct jav_module jav_module_t;
typedef struct jav_data_section jav_data_section_t;
typedef struct jav_br_table jav_br_table_t;
typedef struct jav_lane_imm jav_lane_imm_t;
typedef struct jav_export jav_export_t;
typedef struct jav_custom_section jav_custom_section_t;
typedef struct jav_mem_entry jav_mem_entry_t;
typedef struct jav_elem1 jav_elem1_t;
typedef struct jav_elem3 jav_elem3_t;
typedef struct jav_data1 jav_data1_t;
typedef struct jav_data2 jav_data2_t;
typedef struct jav_data0 jav_data0_t;
typedef struct jav_elem2 jav_elem2_t;
typedef struct jav_func_body jav_func_body_t;
typedef struct jav_elem0 jav_elem0_t;
typedef struct jav_elem4 jav_elem4_t;
typedef struct jav_val_type jav_val_type_t;
typedef struct jav_block_type jav_block_type_t;
typedef struct jav_ref_type jav_ref_type_t;
typedef struct jav_storage_type jav_storage_type_t;
typedef struct jav_misc_instr jav_misc_instr_t;
typedef struct jav_gc_instr jav_gc_instr_t;
typedef struct jav_mem_lane_imm jav_mem_lane_imm_t;
typedef struct jav_section jav_section_t;
typedef struct jav_data jav_data_t;
typedef struct jav_code_entry jav_code_entry_t;
typedef struct jav_global_type jav_global_type_t;
typedef struct jav_locals jav_locals_t;
typedef struct jav_try_table jav_try_table_t;
typedef struct jav_block jav_block_t;
typedef struct jav_if jav_if_t;
typedef struct jav_elem7 jav_elem7_t;
typedef struct jav_elem6 jav_elem6_t;
typedef struct jav_elem5 jav_elem5_t;
typedef struct jav_table_type jav_table_type_t;
typedef struct jav_field_type jav_field_type_t;
typedef struct jav_simd_instr jav_simd_instr_t;
typedef struct jav_global jav_global_t;
typedef struct jav_elem jav_elem_t;
typedef struct jav_extern_type jav_extern_type_t;
typedef struct jav_table_plain jav_table_plain_t;
typedef struct jav_table_init jav_table_init_t;
typedef struct jav_array_type jav_array_type_t;
typedef struct jav_instr jav_instr_t;
typedef struct jav_import jav_import_t;
typedef struct jav_table jav_table_t;
typedef struct jav_comp_type jav_comp_type_t;
typedef struct jav_sub_type jav_sub_type_t;
typedef struct jav_rec_member jav_rec_member_t;
typedef struct jav_rec_type jav_rec_type_t;

struct jav_name {
    uint32_t count;
    bbq_bytes_t bytes;
};

struct jav_tag_type {
    uint8_t attr;
    uint32_t type;
};

struct jav_limits {
    uint8_t flag;
    uint64_t min;
    struct { bool has_value; uint64_t value; } max;
};

struct jav_idx_vec {
    uint32_t count;
    struct { uint32_t* items; size_t count; } idxs;
};

struct jav_catch {
    uint8_t kind;
    struct { bool has_value; uint32_t value; } tag;
    uint32_t label;
};

struct jav_byte_vec {
    uint32_t count;
    bbq_bytes_t bytes;
};

struct jav_expr {
    struct { jav_instr_t* items; size_t count; } instrs;
    uint8_t end;
};

struct jav_expr_vec {
    uint32_t count;
    struct { jav_expr_t* items; size_t count; } exprs;
};

struct jav_heap_type {
    int64_t x;
};

struct jav_idx_imm {
    uint32_t x;
};

struct jav_else_clause {
    uint8_t marker;
    struct { jav_instr_t* items; size_t count; } instrs;
};

struct jav_i64_imm {
    int64_t v;
};

struct jav_f32_imm {
    float v;
};

struct jav_v128_imm {
    bbq_bytes_t bytes;
};

struct jav_data_count_section {
    uint32_t count;
};

struct jav_heap_imm {
    int64_t ht;
};

struct jav_table_section {
    uint32_t count;
    struct { jav_table_t* items; size_t count; } tables;
};

struct jav_f64_imm {
    double v;
};

struct jav_import_section {
    uint32_t count;
    struct { jav_import_t* items; size_t count; } imports;
};

struct jav_global_section {
    uint32_t count;
    struct { jav_global_t* items; size_t count; } globals;
};

struct jav_element_section {
    uint32_t count;
    struct { jav_elem_t* items; size_t count; } elems;
};

struct jav_br_on_cast {
    uint8_t flags;
    uint32_t label;
    int64_t ht1;
    int64_t ht2;
};

struct jav_export_section {
    uint32_t count;
    struct { jav_export_t* items; size_t count; } exports;
};

struct jav_tag_section {
    uint32_t count;
    struct { jav_tag_type_t* items; size_t count; } tags;
};

struct jav_select_t {
    uint32_t count;
    struct { jav_val_type_t* items; size_t count; } types;
};

struct jav_start_section {
    uint32_t func;
};

struct jav_idx2_imm {
    uint32_t x;
    uint32_t y;
};

struct jav_empty {
};

struct jav_mem_arg {
    uint32_t align;
    struct { bool has_value; uint32_t value; } memidx;
    uint64_t offset;
};

struct jav_code_section {
    uint32_t count;
    struct { jav_code_entry_t* items; size_t count; } entries;
};

struct jav_struct_type {
    uint32_t field_count;
    struct { jav_field_type_t* items; size_t count; } fields;
};

struct jav_memory_section {
    uint32_t count;
    struct { jav_mem_entry_t* items; size_t count; } mems;
};

struct jav_function_section {
    uint32_t count;
    struct { uint32_t* items; size_t count; } type_indices;
};

struct jav_rec_group {
    uint32_t count;
    struct { jav_rec_member_t* items; size_t count; } members;
};

struct jav_type_section {
    uint32_t count;
    struct { jav_rec_type_t* items; size_t count; } types;
};

struct jav_func_type {
    uint32_t param_count;
    struct { jav_val_type_t* items; size_t count; } params;
    uint32_t result_count;
    struct { jav_val_type_t* items; size_t count; } results;
};

struct jav_i32_imm {
    int32_t v;
};

struct jav_module {
    uint32_t magic;
    uint32_t version;
    struct { jav_section_t* items; size_t count; } sections;
};

struct jav_data_section {
    uint32_t count;
    struct { jav_data_t* items; size_t count; } datas;
};

struct jav_br_table {
    uint32_t count;
    struct { uint32_t* items; size_t count; } targets;
    uint32_t default_target;
};

struct jav_lane_imm {
    uint8_t lane;
};

struct jav_export {
    jav_name_t name;
    uint8_t kind;
    uint32_t idx;
};

struct jav_custom_section {
    jav_name_t name;
    bbq_bytes_t data;
};

struct jav_mem_entry {
    jav_limits_t limits;
};

struct jav_elem1 {
    uint8_t elemkind;
    jav_idx_vec_t funcs;
};

struct jav_elem3 {
    uint8_t elemkind;
    jav_idx_vec_t funcs;
};

struct jav_data1 {
    jav_byte_vec_t data;
};

struct jav_data2 {
    uint32_t memidx;
    jav_expr_t offset;
    jav_byte_vec_t data;
};

struct jav_data0 {
    jav_expr_t offset;
    jav_byte_vec_t data;
};

struct jav_elem2 {
    uint32_t table;
    jav_expr_t offset;
    uint8_t elemkind;
    jav_idx_vec_t funcs;
};

struct jav_func_body {
    uint32_t local_count;
    struct { jav_locals_t* items; size_t count; } locals;
    jav_expr_t body;
};

struct jav_elem0 {
    jav_expr_t offset;
    jav_idx_vec_t funcs;
};

struct jav_elem4 {
    jav_expr_t offset;
    jav_expr_vec_t exprs;
};

struct jav_val_type {
    uint8_t head;
    struct { bool has_value; jav_heap_type_t value; } ht;
};

struct jav_block_type {
    int64_t bt;
    struct { bool has_value; jav_heap_type_t value; } ht;
};

struct jav_ref_type {
    uint8_t head;
    struct { bool has_value; jav_heap_type_t value; } ht;
};

struct jav_storage_type {
    uint8_t head;
    struct { bool has_value; jav_heap_type_t value; } ht;
};

struct jav_misc_instr {
    uint32_t sub;
    struct { int tag; union { jav_empty_t case_0; jav_idx2_imm_t case_1; jav_idx_imm_t case_2; jav_idx2_imm_t case_3; jav_idx_imm_t case_4; jav_idx2_imm_t case_5; jav_idx_imm_t case_6; jav_idx2_imm_t case_7; jav_idx_imm_t case_8; } u; } body;
};

struct jav_gc_instr {
    uint32_t sub;
    struct { int tag; union { jav_idx_imm_t case_0; jav_idx2_imm_t case_1; jav_idx_imm_t case_2; jav_idx2_imm_t case_3; jav_idx_imm_t case_4; jav_empty_t case_5; jav_idx_imm_t case_6; jav_idx2_imm_t case_7; jav_heap_imm_t case_8; jav_br_on_cast_t case_9; jav_empty_t case_10; } u; } body;
};

struct jav_mem_lane_imm {
    jav_mem_arg_t mem;
    uint8_t lane;
};

struct jav_section {
    uint8_t id;
    uint32_t size;
    struct { int tag; union { jav_custom_section_t case_0; jav_type_section_t case_1; jav_import_section_t case_2; jav_function_section_t case_3; jav_table_section_t case_4; jav_memory_section_t case_5; jav_global_section_t case_6; jav_export_section_t case_7; jav_start_section_t case_8; jav_element_section_t case_9; jav_code_section_t case_10; jav_data_section_t case_11; jav_data_count_section_t case_12; jav_tag_section_t case_13; } u; } body;
};

struct jav_data {
    uint32_t flag;
    struct { int tag; union { jav_data0_t case_0; jav_data1_t case_1; jav_data2_t case_2; } u; } body;
};

struct jav_code_entry {
    uint32_t size;
    jav_func_body_t body;
};

struct jav_global_type {
    jav_val_type_t type;
    uint8_t mut;
};

struct jav_locals {
    uint32_t count;
    jav_val_type_t type;
};

struct jav_try_table {
    jav_block_type_t bt;
    uint32_t count;
    struct { jav_catch_t* items; size_t count; } catches;
    struct { jav_instr_t* items; size_t count; } instrs;
    uint8_t end;
};

struct jav_block {
    jav_block_type_t bt;
    struct { jav_instr_t* items; size_t count; } instrs;
    uint8_t end;
};

struct jav_if {
    jav_block_type_t bt;
    struct { jav_instr_t* items; size_t count; } then_body;
    struct { bool has_value; jav_else_clause_t value; } else_body;
    uint8_t end;
};

struct jav_elem7 {
    jav_ref_type_t reftype;
    jav_expr_vec_t exprs;
};

struct jav_elem6 {
    uint32_t table;
    jav_expr_t offset;
    jav_ref_type_t reftype;
    jav_expr_vec_t exprs;
};

struct jav_elem5 {
    jav_ref_type_t reftype;
    jav_expr_vec_t exprs;
};

struct jav_table_type {
    jav_ref_type_t reftype;
    jav_limits_t limits;
};

struct jav_field_type {
    jav_storage_type_t storage;
    uint8_t mut;
};

struct jav_simd_instr {
    uint32_t sub;
    struct { int tag; union { jav_mem_arg_t case_0; jav_v128_imm_t case_1; jav_empty_t case_2; jav_lane_imm_t case_3; jav_empty_t case_4; jav_mem_lane_imm_t case_5; jav_mem_arg_t case_6; jav_empty_t case_7; jav_empty_t case_8; jav_empty_t case_9; jav_empty_t case_10; jav_empty_t case_11; jav_empty_t case_12; jav_empty_t case_13; jav_empty_t case_14; jav_empty_t case_15; jav_empty_t case_16; jav_empty_t case_17; jav_empty_t case_18; jav_empty_t case_19; } u; } body;
};

struct jav_global {
    jav_global_type_t type;
    jav_expr_t init;
};

struct jav_elem {
    uint32_t flag;
    struct { int tag; union { jav_elem0_t case_0; jav_elem1_t case_1; jav_elem2_t case_2; jav_elem3_t case_3; jav_elem4_t case_4; jav_elem5_t case_5; jav_elem6_t case_6; jav_elem7_t case_7; } u; } body;
};

struct jav_extern_type {
    uint8_t kind;
    struct { int tag; union { jav_idx_imm_t case_0; jav_table_type_t case_1; jav_limits_t case_2; jav_global_type_t case_3; jav_tag_type_t case_4; } u; } body;
};

struct jav_table_plain {
    jav_table_type_t type;
};

struct jav_table_init {
    uint8_t marker0;
    uint8_t marker1;
    jav_table_type_t type;
    jav_expr_t init;
};

struct jav_array_type {
    jav_field_type_t field;
};

struct jav_instr {
    uint8_t op;
    struct { int tag; union { jav_empty_t case_0; jav_block_t case_1; jav_if_t case_2; jav_idx_imm_t case_3; jav_empty_t case_4; jav_idx_imm_t case_5; jav_br_table_t case_6; jav_empty_t case_7; jav_idx_imm_t case_8; jav_idx2_imm_t case_9; jav_idx_imm_t case_10; jav_idx2_imm_t case_11; jav_idx_imm_t case_12; jav_empty_t case_13; jav_select_t_t case_14; jav_try_table_t case_15; jav_idx_imm_t case_16; jav_mem_arg_t case_17; jav_idx_imm_t case_18; jav_i32_imm_t case_19; jav_i64_imm_t case_20; jav_f32_imm_t case_21; jav_f64_imm_t case_22; jav_empty_t case_23; jav_heap_imm_t case_24; jav_empty_t case_25; jav_idx_imm_t case_26; jav_empty_t case_27; jav_idx_imm_t case_28; jav_gc_instr_t case_29; jav_misc_instr_t case_30; jav_simd_instr_t case_31; } u; } body;
};

struct jav_import {
    jav_name_t module;
    jav_name_t field;
    jav_extern_type_t desc;
};

enum jav_table_tag {
    jav_table_case_0 = 64,
    jav_table_default_val = -1,
};

struct jav_table {
    enum jav_table_tag tag;
    union {
        jav_table_init_t case_0;
        jav_table_plain_t default_val;
    } u;
};

struct jav_comp_type {
    uint8_t head;
    struct { int tag; union { jav_array_type_t case_0; jav_struct_type_t case_1; jav_func_type_t case_2; } u; } body;
};

struct jav_sub_type {
    uint32_t super_count;
    struct { uint32_t* items; size_t count; } supers;
    jav_comp_type_t body;
};

struct jav_rec_member {
    uint8_t head;
    struct { int tag; union { jav_sub_type_t case_0; jav_sub_type_t case_1; jav_array_type_t case_2; jav_struct_type_t case_3; jav_func_type_t case_4; } u; } body;
};

struct jav_rec_type {
    uint8_t head;
    struct { int tag; union { jav_rec_group_t case_0; jav_sub_type_t case_1; jav_sub_type_t case_2; jav_array_type_t case_3; jav_struct_type_t case_4; jav_func_type_t case_5; } u; } body;
};


// ── Free functions ──

#include <stdlib.h>

static inline void jav_name_free(jav_name_t* p);
static inline void jav_idx_vec_free(jav_idx_vec_t* p);
static inline void jav_byte_vec_free(jav_byte_vec_t* p);
static inline void jav_expr_free(jav_expr_t* p);
static inline void jav_expr_vec_free(jav_expr_vec_t* p);
static inline void jav_else_clause_free(jav_else_clause_t* p);
static inline void jav_v128_imm_free(jav_v128_imm_t* p);
static inline void jav_table_section_free(jav_table_section_t* p);
static inline void jav_import_section_free(jav_import_section_t* p);
static inline void jav_global_section_free(jav_global_section_t* p);
static inline void jav_element_section_free(jav_element_section_t* p);
static inline void jav_export_section_free(jav_export_section_t* p);
static inline void jav_tag_section_free(jav_tag_section_t* p);
static inline void jav_select_t_free(jav_select_t_t* p);
static inline void jav_code_section_free(jav_code_section_t* p);
static inline void jav_struct_type_free(jav_struct_type_t* p);
static inline void jav_memory_section_free(jav_memory_section_t* p);
static inline void jav_function_section_free(jav_function_section_t* p);
static inline void jav_rec_group_free(jav_rec_group_t* p);
static inline void jav_type_section_free(jav_type_section_t* p);
static inline void jav_func_type_free(jav_func_type_t* p);
static inline void jav_module_free(jav_module_t* p);
static inline void jav_data_section_free(jav_data_section_t* p);
static inline void jav_br_table_free(jav_br_table_t* p);
static inline void jav_export_free(jav_export_t* p);
static inline void jav_custom_section_free(jav_custom_section_t* p);
static inline void jav_elem1_free(jav_elem1_t* p);
static inline void jav_elem3_free(jav_elem3_t* p);
static inline void jav_data1_free(jav_data1_t* p);
static inline void jav_data2_free(jav_data2_t* p);
static inline void jav_data0_free(jav_data0_t* p);
static inline void jav_elem2_free(jav_elem2_t* p);
static inline void jav_func_body_free(jav_func_body_t* p);
static inline void jav_elem0_free(jav_elem0_t* p);
static inline void jav_elem4_free(jav_elem4_t* p);
static inline void jav_misc_instr_free(jav_misc_instr_t* p);
static inline void jav_gc_instr_free(jav_gc_instr_t* p);
static inline void jav_section_free(jav_section_t* p);
static inline void jav_data_free(jav_data_t* p);
static inline void jav_code_entry_free(jav_code_entry_t* p);
static inline void jav_try_table_free(jav_try_table_t* p);
static inline void jav_block_free(jav_block_t* p);
static inline void jav_if_free(jav_if_t* p);
static inline void jav_elem7_free(jav_elem7_t* p);
static inline void jav_elem6_free(jav_elem6_t* p);
static inline void jav_elem5_free(jav_elem5_t* p);
static inline void jav_simd_instr_free(jav_simd_instr_t* p);
static inline void jav_global_free(jav_global_t* p);
static inline void jav_elem_free(jav_elem_t* p);
static inline void jav_extern_type_free(jav_extern_type_t* p);
static inline void jav_table_init_free(jav_table_init_t* p);
static inline void jav_instr_free(jav_instr_t* p);
static inline void jav_import_free(jav_import_t* p);
static inline void jav_table_free(jav_table_t* p);
static inline void jav_comp_type_free(jav_comp_type_t* p);
static inline void jav_sub_type_free(jav_sub_type_t* p);
static inline void jav_rec_member_free(jav_rec_member_t* p);
static inline void jav_rec_type_free(jav_rec_type_t* p);

static inline void jav_name_free(jav_name_t* p) {
    (void)p;
    free((void*)p->bytes.data);
}

static inline void jav_idx_vec_free(jav_idx_vec_t* p) {
    (void)p;
    free(p->idxs.items);
}

static inline void jav_byte_vec_free(jav_byte_vec_t* p) {
    (void)p;
    free((void*)p->bytes.data);
}

static inline void jav_expr_free(jav_expr_t* p) {
    (void)p;
    for (size_t i = 0; i < p->instrs.count; i++)
        jav_instr_free(&p->instrs.items[i]);
    free(p->instrs.items);
}

static inline void jav_expr_vec_free(jav_expr_vec_t* p) {
    (void)p;
    for (size_t i = 0; i < p->exprs.count; i++)
        jav_expr_free(&p->exprs.items[i]);
    free(p->exprs.items);
}

static inline void jav_else_clause_free(jav_else_clause_t* p) {
    (void)p;
    for (size_t i = 0; i < p->instrs.count; i++)
        jav_instr_free(&p->instrs.items[i]);
    free(p->instrs.items);
}

static inline void jav_v128_imm_free(jav_v128_imm_t* p) {
    (void)p;
    free((void*)p->bytes.data);
}

static inline void jav_table_section_free(jav_table_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->tables.count; i++)
        jav_table_free(&p->tables.items[i]);
    free(p->tables.items);
}

static inline void jav_import_section_free(jav_import_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->imports.count; i++)
        jav_import_free(&p->imports.items[i]);
    free(p->imports.items);
}

static inline void jav_global_section_free(jav_global_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->globals.count; i++)
        jav_global_free(&p->globals.items[i]);
    free(p->globals.items);
}

static inline void jav_element_section_free(jav_element_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->elems.count; i++)
        jav_elem_free(&p->elems.items[i]);
    free(p->elems.items);
}

static inline void jav_export_section_free(jav_export_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->exports.count; i++)
        jav_export_free(&p->exports.items[i]);
    free(p->exports.items);
}

static inline void jav_tag_section_free(jav_tag_section_t* p) {
    (void)p;
    free(p->tags.items);
}

static inline void jav_select_t_free(jav_select_t_t* p) {
    (void)p;
    free(p->types.items);
}

static inline void jav_code_section_free(jav_code_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->entries.count; i++)
        jav_code_entry_free(&p->entries.items[i]);
    free(p->entries.items);
}

static inline void jav_struct_type_free(jav_struct_type_t* p) {
    (void)p;
    free(p->fields.items);
}

static inline void jav_memory_section_free(jav_memory_section_t* p) {
    (void)p;
    free(p->mems.items);
}

static inline void jav_function_section_free(jav_function_section_t* p) {
    (void)p;
    free(p->type_indices.items);
}

static inline void jav_rec_group_free(jav_rec_group_t* p) {
    (void)p;
    for (size_t i = 0; i < p->members.count; i++)
        jav_rec_member_free(&p->members.items[i]);
    free(p->members.items);
}

static inline void jav_type_section_free(jav_type_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->types.count; i++)
        jav_rec_type_free(&p->types.items[i]);
    free(p->types.items);
}

static inline void jav_func_type_free(jav_func_type_t* p) {
    (void)p;
    free(p->params.items);
    free(p->results.items);
}

static inline void jav_module_free(jav_module_t* p) {
    (void)p;
    for (size_t i = 0; i < p->sections.count; i++)
        jav_section_free(&p->sections.items[i]);
    free(p->sections.items);
}

static inline void jav_data_section_free(jav_data_section_t* p) {
    (void)p;
    for (size_t i = 0; i < p->datas.count; i++)
        jav_data_free(&p->datas.items[i]);
    free(p->datas.items);
}

static inline void jav_br_table_free(jav_br_table_t* p) {
    (void)p;
    free(p->targets.items);
}

static inline void jav_export_free(jav_export_t* p) {
    (void)p;
    jav_name_free(&p->name);
}

static inline void jav_custom_section_free(jav_custom_section_t* p) {
    (void)p;
    jav_name_free(&p->name);
    free((void*)p->data.data);
}

static inline void jav_elem1_free(jav_elem1_t* p) {
    (void)p;
    jav_idx_vec_free(&p->funcs);
}

static inline void jav_elem3_free(jav_elem3_t* p) {
    (void)p;
    jav_idx_vec_free(&p->funcs);
}

static inline void jav_data1_free(jav_data1_t* p) {
    (void)p;
    jav_byte_vec_free(&p->data);
}

static inline void jav_data2_free(jav_data2_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_byte_vec_free(&p->data);
}

static inline void jav_data0_free(jav_data0_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_byte_vec_free(&p->data);
}

static inline void jav_elem2_free(jav_elem2_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_idx_vec_free(&p->funcs);
}

static inline void jav_func_body_free(jav_func_body_t* p) {
    (void)p;
    free(p->locals.items);
    jav_expr_free(&p->body);
}

static inline void jav_elem0_free(jav_elem0_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_idx_vec_free(&p->funcs);
}

static inline void jav_elem4_free(jav_elem4_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_expr_vec_free(&p->exprs);
}

static inline void jav_misc_instr_free(jav_misc_instr_t* p) {
    (void)p;
    switch (p->body.tag) {
    default: break;
    }
}

static inline void jav_gc_instr_free(jav_gc_instr_t* p) {
    (void)p;
    switch (p->body.tag) {
    default: break;
    }
}

static inline void jav_section_free(jav_section_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 0:
        jav_custom_section_free(&p->body.u.case_0);
        break;
    case 1:
        jav_type_section_free(&p->body.u.case_1);
        break;
    case 2:
        jav_import_section_free(&p->body.u.case_2);
        break;
    case 3:
        jav_function_section_free(&p->body.u.case_3);
        break;
    case 4:
        jav_table_section_free(&p->body.u.case_4);
        break;
    case 5:
        jav_memory_section_free(&p->body.u.case_5);
        break;
    case 6:
        jav_global_section_free(&p->body.u.case_6);
        break;
    case 7:
        jav_export_section_free(&p->body.u.case_7);
        break;
    case 9:
        jav_element_section_free(&p->body.u.case_9);
        break;
    case 10:
        jav_code_section_free(&p->body.u.case_10);
        break;
    case 11:
        jav_data_section_free(&p->body.u.case_11);
        break;
    case 13:
        jav_tag_section_free(&p->body.u.case_13);
        break;
    default: break;
    }
}

static inline void jav_data_free(jav_data_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 0:
        jav_data0_free(&p->body.u.case_0);
        break;
    case 1:
        jav_data1_free(&p->body.u.case_1);
        break;
    case 2:
        jav_data2_free(&p->body.u.case_2);
        break;
    default: break;
    }
}

static inline void jav_code_entry_free(jav_code_entry_t* p) {
    (void)p;
    jav_func_body_free(&p->body);
}

static inline void jav_try_table_free(jav_try_table_t* p) {
    (void)p;
    free(p->catches.items);
    for (size_t i = 0; i < p->instrs.count; i++)
        jav_instr_free(&p->instrs.items[i]);
    free(p->instrs.items);
}

static inline void jav_block_free(jav_block_t* p) {
    (void)p;
    for (size_t i = 0; i < p->instrs.count; i++)
        jav_instr_free(&p->instrs.items[i]);
    free(p->instrs.items);
}

static inline void jav_if_free(jav_if_t* p) {
    (void)p;
    for (size_t i = 0; i < p->then_body.count; i++)
        jav_instr_free(&p->then_body.items[i]);
    free(p->then_body.items);
    if (p->else_body.has_value)
        jav_else_clause_free(&p->else_body.value);
}

static inline void jav_elem7_free(jav_elem7_t* p) {
    (void)p;
    jav_expr_vec_free(&p->exprs);
}

static inline void jav_elem6_free(jav_elem6_t* p) {
    (void)p;
    jav_expr_free(&p->offset);
    jav_expr_vec_free(&p->exprs);
}

static inline void jav_elem5_free(jav_elem5_t* p) {
    (void)p;
    jav_expr_vec_free(&p->exprs);
}

static inline void jav_simd_instr_free(jav_simd_instr_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 12:
    case 13:
        jav_v128_imm_free(&p->body.u.case_1);
        break;
    default: break;
    }
}

static inline void jav_global_free(jav_global_t* p) {
    (void)p;
    jav_expr_free(&p->init);
}

static inline void jav_elem_free(jav_elem_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 0:
        jav_elem0_free(&p->body.u.case_0);
        break;
    case 1:
        jav_elem1_free(&p->body.u.case_1);
        break;
    case 2:
        jav_elem2_free(&p->body.u.case_2);
        break;
    case 3:
        jav_elem3_free(&p->body.u.case_3);
        break;
    case 4:
        jav_elem4_free(&p->body.u.case_4);
        break;
    case 5:
        jav_elem5_free(&p->body.u.case_5);
        break;
    case 6:
        jav_elem6_free(&p->body.u.case_6);
        break;
    case 7:
        jav_elem7_free(&p->body.u.case_7);
        break;
    default: break;
    }
}

static inline void jav_extern_type_free(jav_extern_type_t* p) {
    (void)p;
    switch (p->body.tag) {
    default: break;
    }
}

static inline void jav_table_init_free(jav_table_init_t* p) {
    (void)p;
    jav_expr_free(&p->init);
}

static inline void jav_instr_free(jav_instr_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 2:
    case 3:
        jav_block_free(&p->body.u.case_1);
        break;
    case 4:
        jav_if_free(&p->body.u.case_2);
        break;
    case 14:
        jav_br_table_free(&p->body.u.case_6);
        break;
    case 28:
        jav_select_t_free(&p->body.u.case_14);
        break;
    case 31:
        jav_try_table_free(&p->body.u.case_15);
        break;
    case 251:
        jav_gc_instr_free(&p->body.u.case_29);
        break;
    case 252:
        jav_misc_instr_free(&p->body.u.case_30);
        break;
    case 253:
        jav_simd_instr_free(&p->body.u.case_31);
        break;
    default: break;
    }
}

static inline void jav_import_free(jav_import_t* p) {
    (void)p;
    jav_name_free(&p->module);
    jav_name_free(&p->field);
    jav_extern_type_free(&p->desc);
}

static inline void jav_table_free(jav_table_t* p) {
    (void)p;
    switch ((*p).tag) {
    case 64:
        jav_table_init_free(&(*p).u.case_0);
        break;
    default: break;
    }
}

static inline void jav_comp_type_free(jav_comp_type_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 95:
        jav_struct_type_free(&p->body.u.case_1);
        break;
    case 96:
        jav_func_type_free(&p->body.u.case_2);
        break;
    default: break;
    }
}

static inline void jav_sub_type_free(jav_sub_type_t* p) {
    (void)p;
    free(p->supers.items);
    jav_comp_type_free(&p->body);
}

static inline void jav_rec_member_free(jav_rec_member_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 79:
        jav_sub_type_free(&p->body.u.case_0);
        break;
    case 80:
        jav_sub_type_free(&p->body.u.case_1);
        break;
    case 95:
        jav_struct_type_free(&p->body.u.case_3);
        break;
    case 96:
        jav_func_type_free(&p->body.u.case_4);
        break;
    default: break;
    }
}

static inline void jav_rec_type_free(jav_rec_type_t* p) {
    (void)p;
    switch (p->body.tag) {
    case 78:
        jav_rec_group_free(&p->body.u.case_0);
        break;
    case 79:
        jav_sub_type_free(&p->body.u.case_1);
        break;
    case 80:
        jav_sub_type_free(&p->body.u.case_2);
        break;
    case 95:
        jav_struct_type_free(&p->body.u.case_4);
        break;
    case 96:
        jav_func_type_free(&p->body.u.case_5);
        break;
    default: break;
    }
}


#include "jav_utf8.h"

#endif /* JAV_TYPES_H */
