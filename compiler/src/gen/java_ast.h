/* ============================================================
 * Auto-generated from ASDL — do not edit by hand.
 * ============================================================ */
#ifndef AST_AST_H
#define AST_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "bbq_arena.h"

/* ── Tag enums for sum types ────────────────────────────── */

typedef enum {
    AST_SINGLEIMPORT,
    AST_WILDCARDIMPORT
} ast_import_t_tag;

typedef enum {
    AST_CLASSDECL,
    AST_INTERFACEDECL
} ast_type_decl_t_tag;

typedef enum {
    AST_FIELDDECL,
    AST_METHODDECL,
    AST_CONSTRUCTORDECL,
    AST_STATICINIT
} ast_member_t_tag;

typedef enum {
    AST_PUBLIC,
    AST_PROTECTED,
    AST_PRIVATE,
    AST_STATIC,
    AST_FINAL,
    AST_ABSTRACT,
    AST_SYNCHRONIZED,
    AST_TRANSIENT,
    AST_VOLATILE,
    AST_NATIVE
} ast_modifier_t;

typedef enum {
    AST_SIMPLENAME,
    AST_QUALIFIEDNAME
} ast_name_t_tag;

typedef enum {
    AST_BYTETYPE,
    AST_SHORTTYPE,
    AST_INTTYPE,
    AST_LONGTYPE,
    AST_CHARTYPE,
    AST_FLOATTYPE,
    AST_DOUBLETYPE,
    AST_BOOLTYPE,
    AST_VOIDTYPE,
    AST_CLASSTYPE,
    AST_ARRAYTYPE
} ast_type_t_tag;

typedef enum {
    AST_BLOCK,
    AST_LOCALVARDECL,
    AST_EXPRSTMT,
    AST_IF,
    AST_WHILE,
    AST_DOWHILE,
    AST_FOR,
    AST_SWITCH,
    AST_TRY,
    AST_THROW,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_LABELED,
    AST_EMPTY
} ast_stmt_t_tag;

typedef enum {
    AST_INTLIT,
    AST_LONGLIT,
    AST_FLOATLIT,
    AST_DOUBLELIT,
    AST_CHARLIT,
    AST_BOOLLIT,
    AST_NULLLIT,
    AST_IDENT,
    AST_FIELDACCESS,
    AST_ARRAYACCESS,
    AST_METHODCALL,
    AST_NEW,
    AST_NEWARRAY,
    AST_ARRAYINIT,
    AST_CAST,
    AST_INSTANCEOF,
    AST_UNARY,
    AST_BINARY,
    AST_ASSIGN,
    AST_COMPOUNDASSIGN,
    AST_TERNARY,
    AST_THIS,
    AST_SUPER,
    AST_SUPERACCESS,
    AST_SUPERCALL,
    AST_CONSTRUCTORCALL
} ast_expr_t_tag;

typedef enum {
    AST_POS,
    AST_NEG,
    AST_BITNOT,
    AST_LOGNOT,
    AST_PREINC,
    AST_PREDEC,
    AST_POSTINC,
    AST_POSTDEC
} ast_unop_t;

typedef enum {
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_REM,
    AST_SHL,
    AST_SHR,
    AST_USHR,
    AST_BITAND,
    AST_BITOR,
    AST_BITXOR,
    AST_EQ,
    AST_NE,
    AST_LT,
    AST_GT,
    AST_LE,
    AST_GE,
    AST_AND,
    AST_OR
} ast_binop_t;

/* ── Forward declarations ───────────────────────────────── */

typedef struct ast_program_t ast_program_t;
typedef struct ast_import_t ast_import_t;
typedef struct ast_type_decl_t ast_type_decl_t;
typedef struct ast_member_t ast_member_t;
typedef struct ast_var_decl_t ast_var_decl_t;
typedef struct ast_param_t ast_param_t;
typedef struct ast_name_t ast_name_t;
typedef struct ast_type_t ast_type_t;
typedef struct ast_stmt_t ast_stmt_t;
typedef struct ast_switch_case_t ast_switch_case_t;
typedef struct ast_catch_clause_t ast_catch_clause_t;
typedef struct ast_expr_t ast_expr_t;

/* ── Source location ────────────────────────────────────── */

typedef struct {
    const char* file;
    int line;
    int col;
} ast_srcloc;

/* ── Multi-constructor sum types (tagged unions) ────────── */

struct ast_import_t {
    ast_import_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            const char** parts;
            int parts_count;
        } single_import;
        struct {
            const char** parts;
            int parts_count;
        } wildcard_import;
    };
};

struct ast_type_decl_t {
    ast_type_decl_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            const char* name;
            ast_name_t* super_class;
            ast_name_t** interfaces;
            int interfaces_count;
            ast_modifier_t* mods;
            int mods_count;
            ast_member_t** members;
            int members_count;
        } class_decl;
        struct {
            const char* name;
            ast_name_t** extends_;
            int extends__count;
            ast_modifier_t* mods;
            int mods_count;
            ast_member_t** members;
            int members_count;
        } interface_decl;
    };
};

struct ast_member_t {
    ast_member_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            ast_type_t* ty;
            ast_var_decl_t** decls;
            int decls_count;
            ast_modifier_t* mods;
            int mods_count;
        } field_decl;
        struct {
            ast_type_t* ret;
            const char* name;
            ast_param_t** params;
            int params_count;
            ast_name_t** throws_;
            int throws__count;
            ast_stmt_t* body;
            ast_modifier_t* mods;
            int mods_count;
        } method_decl;
        struct {
            const char* name;
            ast_param_t** params;
            int params_count;
            ast_name_t** throws_;
            int throws__count;
            ast_stmt_t* body;
            ast_modifier_t* mods;
            int mods_count;
        } constructor_decl;
        struct {
            ast_stmt_t* body;
        } static_init;
    };
};

struct ast_name_t {
    ast_name_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            const char* id;
        } simple_name;
        struct {
            ast_name_t* qualifier;
            const char* id;
        } qualified_name;
    };
};

struct ast_type_t {
    ast_type_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            ast_name_t* name;
        } class_type;
        struct {
            ast_type_t* element;
        } array_type;
    };
};

struct ast_stmt_t {
    ast_stmt_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            ast_stmt_t** stmts;
            int stmts_count;
        } block;
        struct {
            ast_type_t* ty;
            ast_var_decl_t** decls;
            int decls_count;
            ast_modifier_t* mods;
            int mods_count;
        } local_var_decl;
        struct {
            ast_expr_t* e;
        } expr_stmt;
        struct {
            ast_expr_t* test;
            ast_stmt_t* then;
            ast_stmt_t* else_;
        } if_;
        struct {
            ast_expr_t* test;
            ast_stmt_t* body;
        } while_;
        struct {
            ast_stmt_t* body;
            ast_expr_t* test;
        } do_while;
        struct {
            ast_stmt_t* init;
            ast_expr_t* test;
            ast_expr_t** update;
            int update_count;
            ast_stmt_t* body;
        } for_;
        struct {
            ast_expr_t* selector;
            ast_switch_case_t** cases;
            int cases_count;
        } switch_;
        struct {
            ast_stmt_t* body;
            ast_catch_clause_t** catches;
            int catches_count;
            ast_stmt_t* finally_;
        } try_;
        struct {
            ast_expr_t* e;
        } throw_;
        struct {
            ast_expr_t* value;
        } return_;
        struct {
            const char* label;
        } break_;
        struct {
            const char* label;
        } continue_;
        struct {
            const char* label;
            ast_stmt_t* body;
        } labeled;
    };
};

struct ast_expr_t {
    ast_expr_t_tag tag;
    ast_srcloc loc;
    union {
        struct {
            int32_t value;
        } int_lit;
        struct {
            int64_t value;
        } long_lit;
        struct {
            float value;
        } float_lit;
        struct {
            double value;
        } double_lit;
        struct {
            int32_t value;
        } char_lit;
        struct {
            bool value;
        } bool_lit;
        struct {
            const char* name;
        } ident;
        struct {
            ast_expr_t* obj;
            const char* field;
        } field_access;
        struct {
            ast_expr_t* arr;
            ast_expr_t* index;
        } array_access;
        struct {
            ast_expr_t* obj;
            const char* method;
            ast_expr_t** args;
            int args_count;
        } method_call;
        struct {
            ast_name_t* class_;
            ast_expr_t** args;
            int args_count;
        } new_;
        struct {
            ast_type_t* element;
            ast_expr_t** dims;
            int dims_count;
        } new_array;
        struct {
            ast_expr_t** elems;
            int elems_count;
        } array_init;
        struct {
            ast_type_t* ty;
            ast_expr_t* e;
        } cast;
        struct {
            ast_expr_t* e;
            ast_type_t* ty;
        } instance_of;
        struct {
            ast_unop_t op;
            ast_expr_t* e;
        } unary;
        struct {
            ast_binop_t op;
            ast_expr_t* lhs;
            ast_expr_t* rhs;
        } binary;
        struct {
            ast_expr_t* target;
            ast_expr_t* value;
        } assign;
        struct {
            ast_binop_t op;
            ast_expr_t* target;
            ast_expr_t* value;
        } compound_assign;
        struct {
            ast_expr_t* test;
            ast_expr_t* then;
            ast_expr_t* else_;
        } ternary;
        struct {
            const char* field;
        } super_access;
        struct {
            const char* method;
            ast_expr_t** args;
            int args_count;
        } super_call;
        struct {
            bool is_super;
            ast_expr_t** args;
            int args_count;
        } constructor_call;
    };
    int32_t etype;
};

/* ── Single-constructor sum types (plain structs) ───────── */

struct ast_program_t {
    ast_srcloc loc;
    ast_name_t* package_;
    ast_import_t** imports;
    int imports_count;
    ast_type_decl_t** types;
    int types_count;
};

struct ast_var_decl_t {
    ast_srcloc loc;
    const char* name;
    int32_t dims;
    ast_expr_t* init;
};

struct ast_param_t {
    ast_srcloc loc;
    ast_type_t* ty;
    const char* name;
};

struct ast_switch_case_t {
    ast_srcloc loc;
    ast_expr_t* value;
    ast_stmt_t** stmts;
    int stmts_count;
};

struct ast_catch_clause_t {
    ast_srcloc loc;
    ast_type_t* ty;
    const char* name;
    ast_stmt_t* body;
};

/* ── Product types ──────────────────────────────────────── */

/* ── Constructor functions ──────────────────────────────── */

static inline ast_program_t* ast_program(bbq_arena* _a, ast_name_t* package_, ast_import_t** imports, int imports_count, ast_type_decl_t** types, int types_count) {
    ast_program_t* _n = (ast_program_t*)bbq_arena_alloc(_a, sizeof(ast_program_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->package_ = package_;
    _n->imports = imports;
    _n->imports_count = imports_count;
    _n->types = types;
    _n->types_count = types_count;
    return _n;
}

static inline ast_import_t* ast_single_import(bbq_arena* _a, const char** parts, int parts_count) {
    ast_import_t* _n = (ast_import_t*)bbq_arena_alloc(_a, sizeof(ast_import_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SINGLEIMPORT;
    _n->single_import.parts = parts;
    _n->single_import.parts_count = parts_count;
    return _n;
}

static inline ast_import_t* ast_wildcard_import(bbq_arena* _a, const char** parts, int parts_count) {
    ast_import_t* _n = (ast_import_t*)bbq_arena_alloc(_a, sizeof(ast_import_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_WILDCARDIMPORT;
    _n->wildcard_import.parts = parts;
    _n->wildcard_import.parts_count = parts_count;
    return _n;
}

static inline ast_type_decl_t* ast_class_decl(bbq_arena* _a, const char* name, ast_name_t* super_class, ast_name_t** interfaces, int interfaces_count, ast_modifier_t* mods, int mods_count, ast_member_t** members, int members_count) {
    ast_type_decl_t* _n = (ast_type_decl_t*)bbq_arena_alloc(_a, sizeof(ast_type_decl_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CLASSDECL;
    _n->class_decl.name = name;
    _n->class_decl.super_class = super_class;
    _n->class_decl.interfaces = interfaces;
    _n->class_decl.interfaces_count = interfaces_count;
    _n->class_decl.mods = mods;
    _n->class_decl.mods_count = mods_count;
    _n->class_decl.members = members;
    _n->class_decl.members_count = members_count;
    return _n;
}

static inline ast_type_decl_t* ast_interface_decl(bbq_arena* _a, const char* name, ast_name_t** extends_, int extends__count, ast_modifier_t* mods, int mods_count, ast_member_t** members, int members_count) {
    ast_type_decl_t* _n = (ast_type_decl_t*)bbq_arena_alloc(_a, sizeof(ast_type_decl_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_INTERFACEDECL;
    _n->interface_decl.name = name;
    _n->interface_decl.extends_ = extends_;
    _n->interface_decl.extends__count = extends__count;
    _n->interface_decl.mods = mods;
    _n->interface_decl.mods_count = mods_count;
    _n->interface_decl.members = members;
    _n->interface_decl.members_count = members_count;
    return _n;
}

static inline ast_member_t* ast_field_decl(bbq_arena* _a, ast_type_t* ty, ast_var_decl_t** decls, int decls_count, ast_modifier_t* mods, int mods_count) {
    ast_member_t* _n = (ast_member_t*)bbq_arena_alloc(_a, sizeof(ast_member_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_FIELDDECL;
    _n->field_decl.ty = ty;
    _n->field_decl.decls = decls;
    _n->field_decl.decls_count = decls_count;
    _n->field_decl.mods = mods;
    _n->field_decl.mods_count = mods_count;
    return _n;
}

static inline ast_member_t* ast_method_decl(bbq_arena* _a, ast_type_t* ret, const char* name, ast_param_t** params, int params_count, ast_name_t** throws_, int throws__count, ast_stmt_t* body, ast_modifier_t* mods, int mods_count) {
    ast_member_t* _n = (ast_member_t*)bbq_arena_alloc(_a, sizeof(ast_member_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_METHODDECL;
    _n->method_decl.ret = ret;
    _n->method_decl.name = name;
    _n->method_decl.params = params;
    _n->method_decl.params_count = params_count;
    _n->method_decl.throws_ = throws_;
    _n->method_decl.throws__count = throws__count;
    _n->method_decl.body = body;
    _n->method_decl.mods = mods;
    _n->method_decl.mods_count = mods_count;
    return _n;
}

static inline ast_member_t* ast_constructor_decl(bbq_arena* _a, const char* name, ast_param_t** params, int params_count, ast_name_t** throws_, int throws__count, ast_stmt_t* body, ast_modifier_t* mods, int mods_count) {
    ast_member_t* _n = (ast_member_t*)bbq_arena_alloc(_a, sizeof(ast_member_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CONSTRUCTORDECL;
    _n->constructor_decl.name = name;
    _n->constructor_decl.params = params;
    _n->constructor_decl.params_count = params_count;
    _n->constructor_decl.throws_ = throws_;
    _n->constructor_decl.throws__count = throws__count;
    _n->constructor_decl.body = body;
    _n->constructor_decl.mods = mods;
    _n->constructor_decl.mods_count = mods_count;
    return _n;
}

static inline ast_member_t* ast_static_init(bbq_arena* _a, ast_stmt_t* body) {
    ast_member_t* _n = (ast_member_t*)bbq_arena_alloc(_a, sizeof(ast_member_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_STATICINIT;
    _n->static_init.body = body;
    return _n;
}

static inline ast_var_decl_t* ast_var_decl(bbq_arena* _a, const char* name, int32_t dims, ast_expr_t* init) {
    ast_var_decl_t* _n = (ast_var_decl_t*)bbq_arena_alloc(_a, sizeof(ast_var_decl_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->name = name;
    _n->dims = dims;
    _n->init = init;
    return _n;
}

static inline ast_param_t* ast_param(bbq_arena* _a, ast_type_t* ty, const char* name) {
    ast_param_t* _n = (ast_param_t*)bbq_arena_alloc(_a, sizeof(ast_param_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->ty = ty;
    _n->name = name;
    return _n;
}

static inline ast_name_t* ast_simple_name(bbq_arena* _a, const char* id) {
    ast_name_t* _n = (ast_name_t*)bbq_arena_alloc(_a, sizeof(ast_name_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SIMPLENAME;
    _n->simple_name.id = id;
    return _n;
}

static inline ast_name_t* ast_qualified_name(bbq_arena* _a, ast_name_t* qualifier, const char* id) {
    ast_name_t* _n = (ast_name_t*)bbq_arena_alloc(_a, sizeof(ast_name_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_QUALIFIEDNAME;
    _n->qualified_name.qualifier = qualifier;
    _n->qualified_name.id = id;
    return _n;
}

static inline ast_type_t* ast_byte_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BYTETYPE;
    return _n;
}

static inline ast_type_t* ast_short_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SHORTTYPE;
    return _n;
}

static inline ast_type_t* ast_int_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_INTTYPE;
    return _n;
}

static inline ast_type_t* ast_long_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_LONGTYPE;
    return _n;
}

static inline ast_type_t* ast_char_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CHARTYPE;
    return _n;
}

static inline ast_type_t* ast_float_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_FLOATTYPE;
    return _n;
}

static inline ast_type_t* ast_double_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_DOUBLETYPE;
    return _n;
}

static inline ast_type_t* ast_bool_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BOOLTYPE;
    return _n;
}

static inline ast_type_t* ast_void_type(bbq_arena* _a) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_VOIDTYPE;
    return _n;
}

static inline ast_type_t* ast_class_type(bbq_arena* _a, ast_name_t* name) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CLASSTYPE;
    _n->class_type.name = name;
    return _n;
}

static inline ast_type_t* ast_array_type(bbq_arena* _a, ast_type_t* element) {
    ast_type_t* _n = (ast_type_t*)bbq_arena_alloc(_a, sizeof(ast_type_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_ARRAYTYPE;
    _n->array_type.element = element;
    return _n;
}

static inline ast_stmt_t* ast_block(bbq_arena* _a, ast_stmt_t** stmts, int stmts_count) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BLOCK;
    _n->block.stmts = stmts;
    _n->block.stmts_count = stmts_count;
    return _n;
}

static inline ast_stmt_t* ast_local_var_decl(bbq_arena* _a, ast_type_t* ty, ast_var_decl_t** decls, int decls_count, ast_modifier_t* mods, int mods_count) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_LOCALVARDECL;
    _n->local_var_decl.ty = ty;
    _n->local_var_decl.decls = decls;
    _n->local_var_decl.decls_count = decls_count;
    _n->local_var_decl.mods = mods;
    _n->local_var_decl.mods_count = mods_count;
    return _n;
}

static inline ast_stmt_t* ast_expr_stmt(bbq_arena* _a, ast_expr_t* e) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_EXPRSTMT;
    _n->expr_stmt.e = e;
    return _n;
}

static inline ast_stmt_t* ast_if(bbq_arena* _a, ast_expr_t* test, ast_stmt_t* then, ast_stmt_t* else_) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_IF;
    _n->if_.test = test;
    _n->if_.then = then;
    _n->if_.else_ = else_;
    return _n;
}

static inline ast_stmt_t* ast_while(bbq_arena* _a, ast_expr_t* test, ast_stmt_t* body) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_WHILE;
    _n->while_.test = test;
    _n->while_.body = body;
    return _n;
}

static inline ast_stmt_t* ast_do_while(bbq_arena* _a, ast_stmt_t* body, ast_expr_t* test) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_DOWHILE;
    _n->do_while.body = body;
    _n->do_while.test = test;
    return _n;
}

static inline ast_stmt_t* ast_for(bbq_arena* _a, ast_stmt_t* init, ast_expr_t* test, ast_expr_t** update, int update_count, ast_stmt_t* body) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_FOR;
    _n->for_.init = init;
    _n->for_.test = test;
    _n->for_.update = update;
    _n->for_.update_count = update_count;
    _n->for_.body = body;
    return _n;
}

static inline ast_stmt_t* ast_switch(bbq_arena* _a, ast_expr_t* selector, ast_switch_case_t** cases, int cases_count) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SWITCH;
    _n->switch_.selector = selector;
    _n->switch_.cases = cases;
    _n->switch_.cases_count = cases_count;
    return _n;
}

static inline ast_stmt_t* ast_try(bbq_arena* _a, ast_stmt_t* body, ast_catch_clause_t** catches, int catches_count, ast_stmt_t* finally_) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_TRY;
    _n->try_.body = body;
    _n->try_.catches = catches;
    _n->try_.catches_count = catches_count;
    _n->try_.finally_ = finally_;
    return _n;
}

static inline ast_stmt_t* ast_throw(bbq_arena* _a, ast_expr_t* e) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_THROW;
    _n->throw_.e = e;
    return _n;
}

static inline ast_stmt_t* ast_return(bbq_arena* _a, ast_expr_t* value) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_RETURN;
    _n->return_.value = value;
    return _n;
}

static inline ast_stmt_t* ast_break(bbq_arena* _a, const char* label) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BREAK;
    _n->break_.label = label;
    return _n;
}

static inline ast_stmt_t* ast_continue(bbq_arena* _a, const char* label) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CONTINUE;
    _n->continue_.label = label;
    return _n;
}

static inline ast_stmt_t* ast_labeled(bbq_arena* _a, const char* label, ast_stmt_t* body) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_LABELED;
    _n->labeled.label = label;
    _n->labeled.body = body;
    return _n;
}

static inline ast_stmt_t* ast_empty(bbq_arena* _a) {
    ast_stmt_t* _n = (ast_stmt_t*)bbq_arena_alloc(_a, sizeof(ast_stmt_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_EMPTY;
    return _n;
}

static inline ast_switch_case_t* ast_switch_case(bbq_arena* _a, ast_expr_t* value, ast_stmt_t** stmts, int stmts_count) {
    ast_switch_case_t* _n = (ast_switch_case_t*)bbq_arena_alloc(_a, sizeof(ast_switch_case_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->value = value;
    _n->stmts = stmts;
    _n->stmts_count = stmts_count;
    return _n;
}

static inline ast_catch_clause_t* ast_catch_clause(bbq_arena* _a, ast_type_t* ty, const char* name, ast_stmt_t* body) {
    ast_catch_clause_t* _n = (ast_catch_clause_t*)bbq_arena_alloc(_a, sizeof(ast_catch_clause_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->ty = ty;
    _n->name = name;
    _n->body = body;
    return _n;
}

static inline ast_expr_t* ast_int_lit(bbq_arena* _a, int32_t value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_INTLIT;
    _n->int_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_long_lit(bbq_arena* _a, int64_t value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_LONGLIT;
    _n->long_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_float_lit(bbq_arena* _a, float value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_FLOATLIT;
    _n->float_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_double_lit(bbq_arena* _a, double value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_DOUBLELIT;
    _n->double_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_char_lit(bbq_arena* _a, int32_t value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CHARLIT;
    _n->char_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_bool_lit(bbq_arena* _a, bool value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BOOLLIT;
    _n->bool_lit.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_null_lit(bbq_arena* _a) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_NULLLIT;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_ident(bbq_arena* _a, const char* name) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_IDENT;
    _n->ident.name = name;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_field_access(bbq_arena* _a, ast_expr_t* obj, const char* field) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_FIELDACCESS;
    _n->field_access.obj = obj;
    _n->field_access.field = field;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_array_access(bbq_arena* _a, ast_expr_t* arr, ast_expr_t* index) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_ARRAYACCESS;
    _n->array_access.arr = arr;
    _n->array_access.index = index;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_method_call(bbq_arena* _a, ast_expr_t* obj, const char* method, ast_expr_t** args, int args_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_METHODCALL;
    _n->method_call.obj = obj;
    _n->method_call.method = method;
    _n->method_call.args = args;
    _n->method_call.args_count = args_count;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_new(bbq_arena* _a, ast_name_t* class_, ast_expr_t** args, int args_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_NEW;
    _n->new_.class_ = class_;
    _n->new_.args = args;
    _n->new_.args_count = args_count;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_new_array(bbq_arena* _a, ast_type_t* element, ast_expr_t** dims, int dims_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_NEWARRAY;
    _n->new_array.element = element;
    _n->new_array.dims = dims;
    _n->new_array.dims_count = dims_count;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_array_init(bbq_arena* _a, ast_expr_t** elems, int elems_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_ARRAYINIT;
    _n->array_init.elems = elems;
    _n->array_init.elems_count = elems_count;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_cast(bbq_arena* _a, ast_type_t* ty, ast_expr_t* e) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CAST;
    _n->cast.ty = ty;
    _n->cast.e = e;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_instance_of(bbq_arena* _a, ast_expr_t* e, ast_type_t* ty) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_INSTANCEOF;
    _n->instance_of.e = e;
    _n->instance_of.ty = ty;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_unary(bbq_arena* _a, ast_unop_t op, ast_expr_t* e) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_UNARY;
    _n->unary.op = op;
    _n->unary.e = e;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_binary(bbq_arena* _a, ast_binop_t op, ast_expr_t* lhs, ast_expr_t* rhs) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_BINARY;
    _n->binary.op = op;
    _n->binary.lhs = lhs;
    _n->binary.rhs = rhs;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_assign(bbq_arena* _a, ast_expr_t* target, ast_expr_t* value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_ASSIGN;
    _n->assign.target = target;
    _n->assign.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_compound_assign(bbq_arena* _a, ast_binop_t op, ast_expr_t* target, ast_expr_t* value) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_COMPOUNDASSIGN;
    _n->compound_assign.op = op;
    _n->compound_assign.target = target;
    _n->compound_assign.value = value;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_ternary(bbq_arena* _a, ast_expr_t* test, ast_expr_t* then, ast_expr_t* else_) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_TERNARY;
    _n->ternary.test = test;
    _n->ternary.then = then;
    _n->ternary.else_ = else_;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_this(bbq_arena* _a) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_THIS;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_super(bbq_arena* _a) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SUPER;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_super_access(bbq_arena* _a, const char* field) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SUPERACCESS;
    _n->super_access.field = field;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_super_call(bbq_arena* _a, const char* method, ast_expr_t** args, int args_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_SUPERCALL;
    _n->super_call.method = method;
    _n->super_call.args = args;
    _n->super_call.args_count = args_count;
    _n->etype = 0;
    return _n;
}

static inline ast_expr_t* ast_constructor_call(bbq_arena* _a, bool is_super, ast_expr_t** args, int args_count) {
    ast_expr_t* _n = (ast_expr_t*)bbq_arena_alloc(_a, sizeof(ast_expr_t));
    _n->loc = (ast_srcloc){0};   /* zero the common source location — arena_alloc doesn't; the parser stamps set nodes */
    _n->tag = AST_CONSTRUCTORCALL;
    _n->constructor_call.is_super = is_super;
    _n->constructor_call.args = args;
    _n->constructor_call.args_count = args_count;
    _n->etype = 0;
    return _n;
}

#endif /* AST_AST_H */
