/* sema.h — Java 1.0 semantic analyzer (type checker)
 *
 * Two-pass analysis following the blossom/pixie Sema pattern:
 *   Pass 1: collect_decls — register classes, fields, methods
 *   Pass 2: analyze_bodies — type-check method/constructor bodies
 *
 * JLS references: Ch 4 (types), Ch 5 (conversions), Ch 6 (names),
 * Ch 8 (classes), Ch 9 (interfaces), Ch 15.12 (method invocation).
 */
#ifndef SEMA_H
#define SEMA_H

#include "gen/java_ast.h"
#include "bbq_arena.h"
#include "bbq_htree.h"
#include "bbq_vec.h"

/* ── ast_type_t representation ──────────────────────────────────── */

typedef enum {
    JT_NULL = 0,  /* null / uninitialized — maps to C NULL in htrees,
                   * so "absence of a value" in both languages is 0. */
    JT_BYTE,
    JT_SHORT,
    JT_INT,
    JT_BOOL,
    JT_VOID,
    JT_CLASS,   /* class or interface reference */
    JT_ARRAY,   /* array (element type recorded; multi-dim nests) */
    /* Full Java 1.0 primitives (appended so existing tag values are
     * stable for the designated-initializer jtype_meta tables). */
    JT_CHAR,    /* 16-bit unsigned */
    JT_LONG,    /* 64-bit */
    JT_FLOAT,   /* 32-bit IEEE */
    JT_DOUBLE,  /* 64-bit IEEE */
    JT_V128,    /* WASM v128 (SIMD) — the javelina.simd value type; not a JLS type,
                 * appended before the sentinel so jtype_meta's high entry still
                 * sizes the tables */
    JT_ERROR    /* error sentinel — suppresses cascading errors */
} java_type_tag_t;

typedef struct java_type_t {
    java_type_tag_t tag;
    int class_id;              /* JT_CLASS: index into class_table */
    struct java_type_t* element;  /* JT_ARRAY: element type (arena-allocated) */
} java_type_t;

/* Convenience constructors */
static inline java_type_t jt_prim(java_type_tag_t tag) {
    return (java_type_t){ tag, -1, NULL };
}
static inline java_type_t jt_class(int id) {
    return (java_type_t){ JT_CLASS, id, NULL };
}
static inline java_type_t jt_array(java_type_t* elem) {
    return (java_type_t){ JT_ARRAY, -1, elem };
}
/* A JT_ARRAY marked (class_id == JT_ARRAY_RAW) is the CONCRETE backing of an array overlay
 * — the overlay's own `data` field, an (array W)/(array anyref) that must NOT be re-overlaid
 * into a PrimArray/RefArray. lat_array_overlay_class maps it to -1 (concrete), the same role
 * a JT_NULL element plays for RefArray's backing. Real Java array types have class_id == -1. */
#define JT_ARRAY_RAW 1
static inline java_type_t jt_raw_array(java_type_t* elem) {
    return (java_type_t){ JT_ARRAY, JT_ARRAY_RAW, elem };
}
static inline java_type_t jt_error(void) {
    return (java_type_t){ JT_ERROR, -1, NULL };
}
static inline java_type_t jt_null(void) {
    return (java_type_t){ JT_NULL, -1, NULL };
}

/* Predicates (JLS §4.2) */
static inline bool jt_is_integral(java_type_t t) {
    return t.tag == JT_BYTE || t.tag == JT_SHORT || t.tag == JT_CHAR
        || t.tag == JT_INT  || t.tag == JT_LONG;
}
static inline bool jt_is_floating(java_type_t t) {
    return t.tag == JT_FLOAT || t.tag == JT_DOUBLE;
}
static inline bool jt_is_numeric(java_type_t t) {
    return jt_is_integral(t) || jt_is_floating(t);
}
static inline bool jt_is_reference(java_type_t t) {
    return t.tag == JT_CLASS || t.tag == JT_ARRAY || t.tag == JT_NULL;
}
static inline bool jt_is_error(java_type_t t) {
    return t.tag == JT_ERROR;
}
static inline bool jt_eq(java_type_t a, java_type_t b) {
    if (a.tag != b.tag) return false;
    if (a.tag == JT_CLASS) return a.class_id == b.class_id;
    if (a.tag == JT_ARRAY) return a.element && b.element && jt_eq(*a.element, *b.element);
    return true;
}

/* ── ast_modifier_t flags (bitfield) ────────────────────────────── */

#define ACC_PUBLIC      0x0001
#define ACC_PRIVATE     0x0002
#define ACC_PROTECTED   0x0004
#define ACC_STATIC      0x0008
#define ACC_FINAL       0x0010
#define SEMA_ACC_ABSTRACT    0x0020
#define ACC_NATIVE      0x0040
#define ACC_SYNCHRONIZED 0x0080
#define ACC_TRANSIENT   0x0100
#define ACC_VOLATILE    0x0400
#define ACC_INTERFACE   0x0200

/* ── SIR data-type tags (returned by sema_data_type) ───────────────
 *
 * Numeric values must agree with compiler.c's SIR_DT_* defines and
 * codegen.burg's DT_* defines (Phase D of the DDCG plan unifies all
 * three; for now they are duplicated by value). These tag the
 * effective width of an expression, used by BURS to pick s* vs i*
 * vs a* opcodes.
 */
#define SEMA_DT_BYTE   0
#define SEMA_DT_SHORT  1
#define SEMA_DT_INT    2
#define SEMA_DT_REF    3
#define SEMA_DT_CHAR   4
#define SEMA_DT_LONG   5
#define SEMA_DT_FLOAT  6
#define SEMA_DT_DOUBLE 7
#define SEMA_DT_V128   8

/* ── Ident kind enum (returned by sema_ident_kind) ─────────────────
 *
 * Classifies what an AST_IDENT expression resolves to, so DDCG can
 * dispatch directly without re-doing the scope_lookup → field
 * fallback cascade that compile.c:202 currently performs. */
typedef enum {
    SEMA_IDENT_LOCAL,           /* body local or catch variable */
    SEMA_IDENT_PARAM,           /* method parameter or `this` */
    SEMA_IDENT_INSTANCE_FIELD,  /* unqualified instance field of current class */
    SEMA_IDENT_STATIC_FIELD,    /* unqualified static field */
    SEMA_IDENT_CLASSREF,        /* a class name used as a value base (§6.5.4) —
                                 * recorded so downstream passes never re-resolve
                                 * the name (resolution is unit-relative) */
} sema_ident_kind_t;

/* ── Invoke kind enum (returned by sema_invoke_kind) ───────────────
 *
 * Classifies a method-call expression by which sir_invoke_* node
 * DDCG should emit. compile.c today reads modifier bits inline and
 * only emits static/virtual; private instance methods (special) and
 * interface dispatch are silently miscompiled. */
typedef enum {
    SEMA_INVOKE_STATIC,     /* invokestatic */
    SEMA_INVOKE_VIRTUAL,    /* invokevirtual */
    SEMA_INVOKE_SPECIAL,    /* invokespecial: private instance, super.f(), super()/this() */
    SEMA_INVOKE_INTERFACE,  /* invokeinterface */
} sema_invoke_kind_t;

typedef struct sema_field_t sema_field_t;  /* fwd; full def below */

typedef struct {
    sema_ident_kind_t kind;
    int32_t slot;                /* LOCAL, PARAM: pre-allocated slot */
    int32_t dt;                  /* LOCAL, PARAM, *_FIELD: SEMA_DT_* */
    const sema_field_t* field;   /* INSTANCE_FIELD, STATIC_FIELD: resolved field */
    /* LOCAL: the declaration's `final` flag and its initializer, so §15.27 can decide whether the
     * name refers to a final variable whose initializer is a constant expression. A parameter has
     * no initializer and is never a constant variable. */
    bool var_is_final;
    const ast_expr_t* var_init;
} sema_ident_info_t;

/* ── Symbol table entries ─────────────────────────────────── */

struct sema_field_t {
    const char* name;
    java_type_t type;
    int modifiers;
    int owner;          /* declaring class id — the IDENTITY, stamped at stamp_member_identity */
    int index;          /* field index within its declaring class */
    const ast_expr_t* init_expr; /* declaration-site initializer, or NULL */
};

typedef struct {
    const char* name;
    java_type_t return_type;
    java_type_t* param_types;  /* arena-allocated array */
    const char** param_names;
    int param_count;
    int modifiers;
    bool is_constructor;
    bool is_synthetic_default;    /* JLS §8.8.9 compiler-provided default ctor (no source decl) */
    bool is_synthetic_clone;      /* §20.1.5 compiler-synthesized internalClone override (per-type shallow copy) */
    bool is_synthetic_ensure_init; /* JLS §12.4.2 compiler-synthesized `$ensure_init` (lazy class-init barrier target) */
    bool is_synthetic_main;       /* E7.1a compiler-synthesized `$main(argc,base)->int` program-entry wrapper */
    bool is_synthetic_new_instance; /* §20.3.6 compiler-synthesized `static Object $newInstance()` — the
                                     * class's no-arg factory, held as a funcref by its Class singleton */
    java_type_t* thrown_types;    /* arena-allocated; checked exceptions declared by throws */
    int thrown_count;
    ast_member_t* ast_node;       /* back-pointer for body analysis */
    int32_t max_user_slots;       /* Phase B: this + params + body locals */
    int owner;                    /* declaring class id — the IDENTITY (with `index`), so a resolved */
    int index;                    /* method's (class, position) is READ off the struct, never recovered */
    int move_kind;                /* bit-accessor intrinsic kind (0 none, 1 F2I, 2 I2F, 3 D2L, 4 L2D), */
    int math_kind;                /* f64 math-op intrinsic (0 none, 1 sqrt, 2 floor, 3 ceil, 4 rint) — §20.11 */
    int class_kind;               /* §20.3.6 Class.newInstance intrinsic over the receiver Class:
                                   * 0 none, 1 instantiable? (i32), 2 construct (ref) — lowers inline, never a call */
    int simd_id;                  /* javelina.simd intrinsic: 0 none, else 1 + the row index into the
                                   * generated simd_intrinsics[] table (family/wop/lanes live THERE) */
} sema_method_t;                  /* stamped once in resolve_wellknown_methods — READ, not re-strcmp'd. */

typedef struct {
    const char* name;
    const char* fq_name;    /* fully qualified: package.ClassName */
    int super_id;           /* class_table index, -1 for Object */
    int* interface_ids;     /* arena-allocated array */
    int interface_count;
    sema_field_t* fields;      /* bbq_vec */
    sema_method_t* methods;    /* bbq_vec */
    int modifiers;
    bool is_interface;
    bool needs_init;               /* JLS §12.4: has static field-init/static-block, OR super needs_init.
                                    * Gates the §12.4.2 init barrier at active-use sites. */
    int import_pkg;                /* >=0 marks a bundled-library class (host-import side of the
                                    * module split), -1 = user code. Historical name. */
    int unit_idx;                  /* owning compilation unit (ctx->units), -1 = synthetic */
    ast_type_decl_t* ast_node;     /* back-pointer */
} sema_class_t;

/* One emitted module function: the (class, method) it came from. The function
 * table (ctx->functions) lists these in module-function-index order — the single
 * authority for "which methods become module functions, and at what index",
 * computed in sema (where it is fully known) and READ by the DDCG/backend, never
 * re-derived. Library (java.lang) methods are excluded — they are host imports. */
typedef struct { int class_id; int method_id; } sema_func_ent_t;

/* The spec-defined well-known java.lang classes (JLS 1.0 §20/§11.5), resolved ONCE
 * from the loaded prelude at sema_analyze (resolve_wellknown) and read BY class_id
 * everywhere — never re-looked-up by string, never `strcmp(name,"Throwable")`. The
 * compiler OWNS the JRE interface, so a missing one is a hard error (a broken
 * prelude), caught at resolution, not a silent -1 surfacing later. */
typedef struct {
    int object_id;               /* the class-tree root */
    int throwable_id;            /* exception hierarchy root */
    int exception_id;
    int error_id;
    int runtime_exception_id;    /* §11.2 unchecked-exception boundary */
    int cloneable_id;
    int string_id;
    int string_buffer_id;
    int class_reflect_id;        /* java.lang.Class (§20.3) — getClass()'s return + the reflection API */
    int getclass_method_id;      /* Object.getClass()'s method index (§20.1.1) — its forwarder reads field 0 */
    int finalize_method_id;      /* Object.finalize()'s method index (§20.1.7) — the escape analysis's
                                  * finalizer root: an object whose class overrides it is reachable
                                  * from the finalizer and can never be method-local */
    int arraystore_check_method_id; /* Class.arrayStoreCheck(Class,Object)'s index (§10.10 store guard) */
    int is_instance_method_id;   /* Class.isInstance(Object)'s index (§15.19.2 instanceof/checkcast guard) */
    int system_id;               /* java.lang.System (§20.18) — the arraycopy intrinsic's target class */
    int arraycopy_method_id;     /* System.arraycopy(Object,int,Object,int,int)'s index (§20.18.16) */
    int float_id;                /* java.lang.Float — the raw bit-accessor Move* intrinsics' target */
    int double_id;               /* java.lang.Double — likewise (doubleToRawLongBits/longBitsToDouble) */
    int math_id;                 /* java.lang.Math — also the §15.17.3 float-remainder helper's home */
    int fmod_float_id;           /* Math.fmod(float,float)   — `float % float`  (WASM has no f32.rem) */
    int fmod_double_id;          /* Math.fmod(double,double) — `double % double` (no f64.rem either) */
    /* §15 implicit-exception classes the compiler emits as catchable throws */
    int null_pointer_id;
    int array_index_oob_id;
    int class_cast_id;
    int negative_array_size_id;
    int arithmetic_id;
    int array_store_id;
    int index_oob_id;            /* base IndexOutOfBoundsException — §20.18.16 arraycopy throws THIS, not the array subclass */
    int exc_in_init_id;          /* §12.4.2 step 10: ExceptionInInitializerError wrapping a non-Error initializer throw */
    int no_class_def_id;         /* §12.4.2 step 5: NoClassDefFoundError on re-entry to an ERRONEOUS class */
    /* E7.1a program entry. main_class_id/main_method_id = the user `public static void
     * main(String[])` (both -1 if none / in RUNTIME mode). The synthesized `$main(argc,base)
     * ->int` wrapper (build_main) reads these plus its helpers: java.io.Startup.args(int,int)
     * ->String[] (the argv UTF-8 decoder) and Throwable.printStackTrace() (the top-level
     * uncaught handler). All -1 when no entry is synthesized. */
    int main_class_id, main_method_id;
    int startup_id, startup_args_method_id;
    int throwable_pst_method_id;
    /* §10 arrays: the synthesized RefArray class — the ONE struct overlay every
     * REFERENCE array (T[], T a reference type) is represented by, so covariance
     * (§10.2) is free (String[] and Object[] are the same WASM type) and §10.10
     * ArrayStore has a home for the runtime element type. Not a prelude .java class:
     * its backing `data` field is a raw array of the top reference type, which has
     * no Java source syntax — so sema synthesizes it (ast_node NULL). */
    int refarray_id;
    /* §10.7/§10.8: the synthesized per-width PrimArray overlays — one struct
     * `{ Class header; (array W) data }` per WASM backing width, so a primitive array is
     * an Object (assignable to Object, has a Class/getClass) and its ops reach the
     * concrete backing via GetField (no downcast). Indexed by lat_prim_storage_index,
     * keyed by SIR datatype so char has its own overlay/backing typeidx distinct from
     * short (they'd share i16 bytes but are distinct WASM array types): 0=byte(+bool)
     * 1=short 2=char 3=int 4=long 5=float 6=double. */
    int primarray_ids[8];
    /* javelina.simd.V128 — the v128 VALUE class. resolve_type maps this class to
     * jt_prim(JT_V128) at every use site, so a V128 is never a reference. -1 when
     * the simd library is absent (soft — plain-Java programs need no simd). */
    int v128_id;
} sema_wellknown_t;

/* ── Diagnostics ──────────────────────────────────────────── */

typedef enum { DIAG_ERROR, DIAG_WARNING } diag_level_t;

/* Diagnostic identity — what a test (or tool) matches on. The message is
 * PROSE, free to be reworded; the kind is the contract. GENERIC (0) is the
 * default for diagnostics no consumer needs to discriminate yet. */
typedef enum {
    SEMA_DIAG_GENERIC = 0,
    SEMA_DIAG_RECURSION_CYCLE,   /* non-tail recursion can exhaust the stack */
    SEMA_DIAG_ARRAY_BOUNDS,      /* index not provably within [0, length)    */
    SEMA_DIAG_NARROWING_CAST,    /* value range exceeds the cast target      */
} sema_diag_kind_t;

typedef struct {
    diag_level_t level;
    sema_diag_kind_t kind;
    ast_srcloc loc;
    char message[256];
} sema_diag_t;

/* ── Scope entry ──────────────────────────────────────────── */

typedef struct {
    java_type_t type;
    bool is_final;
    bool is_param;    /* Phase B: declared as param/this rather than body local */
    int32_t slot;     /* Phase B: slot index assigned at scope_declare */
    const ast_expr_t* init_expr;  /* declaration-site initializer, or NULL — §15.27 constant variables */
} sema_var_t;

/* ── Compilation units (JLS §7.3) ─────────────────────────── */

/* One compilation unit: its package (§7.4) and its validated import lists
 * (§7.5). Built by sema_analyze_units from each parsed program, in input
 * order. Synthetic classes (RefArray, the overlays, array Classes) carry
 * unit_idx -1: internal, unnamed package, no imports. */
typedef struct {
    const char* package;     /* interned "java.io", or NULL = unnamed (§7.4.2) */
    const char** singles;    /* bbq_vec: single-type-import FQNs (§7.5.1), source order */
    const char** ondemands;  /* bbq_vec: on-demand package names (§7.5.2);
                              * "java.lang" is always present (§7.5.3) */
    const ast_program_t* prog;
} sema_unit_t;

typedef struct { const char* name; bool is_loop; } sema_label_t;

/* Compile-time scope frame — one per ρ push at codegen time.
 * Loops always push a frame; labeled-stmts whose body isn't a
 * loop/switch push one too. Labeled-stmts wrapping a loop/switch
 * don't push their own — the inner loop's frame takes the label. */
typedef enum {
    SEMA_FRAME_LOOP,           /* unlabeled or labeled loop */
    SEMA_FRAME_SWITCH,         /* switch — valid break target, not continue */
    SEMA_FRAME_LABELED_BLOCK   /* labeled block (continue can't target) */
} sema_frame_kind_t;
typedef struct {
    sema_frame_kind_t kind;
    const char* label;         /* NULL if unlabeled */
} sema_frame_t;

/* ── Semantic analysis context ────────────────────────────── */

/* Emission mode (INPUT, set by the driver before sema_analyze). Governs whether the
 * bundled java.lang library is compiled INTO this module or IMPORTED from a separately
 * built runtime module (jre.wasm). Default 0 = WHOLE = a self-contained module (today's
 * behavior). RUNTIME = the jre.wasm build: WHOLE + also EXPORT the library funcs/globals.
 * PLUGIN = a thin user module: the java.lang SOURCE classes become IMPORTS (their bodies
 * live in jre.wasm), only user code is defined/emitted. The struct TYPES are always
 * defined locally (WASM has no type imports); the shared-heap gcanon unifies them. */
typedef enum { SEMA_MODE_WHOLE = 0, SEMA_MODE_RUNTIME, SEMA_MODE_PLUGIN } sema_mode_t;

typedef struct {
    bbq_arena* arena;

    /* Class table (bbq_vec of sema_class_t) */
    sema_class_t* classes;
    bbq_htree* class_by_name;   /* name hash → class index */

    /* Per-function state */
    java_type_t current_return_type;
    int current_class_id;
    int loop_depth;
    bool in_static_context;
    bool in_static_init;  /* true only inside a static-initializer block (JLS §8.7):
                           * where a blank `static final` may be definitely assigned. */
    sema_label_t* labels; /* bbq_vec */
    sema_frame_t* frames; /* bbq_vec — ρ-frame stack mirroring codegen */
    const char* pending_frame_label; /* AST_LABELED → child loop/switch */
    int32_t next_slot;    /* Phase B: per-method monotonic slot counter */
    int32_t clinit_next_slot; /* shared local-slot counter across ALL static-init
                               * blocks (they fold into one <clinit> → distinct
                               * slot ranges); total = the module init's locals */
    bool declaring_params; /* Phase B: scope_declare → is_param flag */
    bool declaring_final;  /* sema hardening: scope_declare → is_final flag */
    const ast_expr_t* declaring_init;  /* scope_declare → init_expr (§15.27 constant variables) */
    bool in_constructor;   /* sema hardening: allow final field writes */
    const sema_method_t* current_method; /* for throws-clause checking */
    java_type_t* caught_types; /* bbq_vec: exception types caught by enclosing try blocks */
    int static_init_field_limit; /* §8.3.3: max field index visible in
                                  * current static initializer; -1 = no
                                  * restriction (method body context) */

    /* Scope stack (bbq_vec of bbq_htree*) */
    bbq_htree** scopes;

    /* Side tables (output) — keyed by (uint32_t)(uintptr_t)ptr */
    bbq_htree* expr_types;       /* ast_expr_t* → java_type_t* */
    bbq_htree* resolved_methods; /* MethodCall* → sema_method_t* */
    bbq_htree* resolved_fields;  /* FieldAccess* → sema_field_t* */
    bbq_htree* data_types;       /* ast_expr_t* → (int)(SEMA_DT_* + 1) */
    bbq_htree* simd_imms;        /* simd MethodCall* → sema_simd_imm_t* (validated §15.27
                                  * lane / const halves / shuffle mask — the ddcg reads
                                  * these as SIR payloads, never re-evaluating) */
    bbq_htree* slot_allocs;      /* decl ptr → (int)(slot + 1) */
    bbq_htree* local_types;      /* var_decl ptr → java_type_t* (the resolved declared type, for the slot's ref descriptor) */
    bbq_htree* ident_kinds;      /* AST_IDENT* → sema_ident_info_t* */
    bbq_htree* resolved_ctors;   /* AST_NEW* / AST_CONSTRUCTORCALL* → sema_method_t* */
    bbq_htree* invoke_kinds;     /* call expr → (int)(SEMA_INVOKE_* + 1) */
    bbq_htree* target_classes;   /* call expr → (int)(class_id + 1) */
    bbq_htree* side_effects;     /* expr → (void*)1 if may have effects, missing otherwise */
    bbq_htree* array_init_elem_types; /* AST_ARRAYINIT → array element type (atype) (10/11/12/13) */
    bbq_htree* instanceof_types; /* AST_INSTANCEOF → resolved target java_type (the expr's own type is boolean) */
    bbq_htree* switch_infos;      /* AST_SWITCH stmt → sema_switch_info_t* */
    bbq_htree* break_target_depths;    /* AST_BREAK*    → (int)(depth + 1) */
    bbq_htree* continue_target_depths; /* AST_CONTINUE* → (int)(depth + 1) */
    bbq_htree* type_class_ids;   /* ast_type_t* (CLASSTYPE) → (int)(class_id + 1) —
                                  * recorded by resolve_type, THE §6.5.4 resolution of
                                  * each spelled type node. Post-sema queries (catch
                                  * class, instanceof/cast target) read this record;
                                  * they never re-resolve (resolution is unit-relative). */

    /* True once a throw/try is seen during analysis → the module needs the
     * exception tag section + tag functype. (Default false; set on sighting.) */
    bool uses_exceptions;

    /* Compilation units (bbq_vec of sema_unit_t) — §7.3. Class → unit via
     * sema_class_t.unit_idx; the unit carries the package + import lists
     * type-name resolution (§6.5.4.1) reads. */
    sema_unit_t* units;

    /* Library boundary (INPUT, set by the driver before sema_analyze): the first
     * `num_library_classes` registered classes are the bundled java.lang runtime
     * (lowest class_ids). sema_analyze marks them import_pkg>=0 so they are
     * excluded from the emitted function table (they become host imports). 0 (the
     * default) = no library, every class is user code. */
    int num_library_classes;

    /* First class index whose method BODIES should be type-checked (INPUT,
     * default 0 = all of them, which is what every shipped path does).
     *
     * Classes below this index are still REGISTERED — names, signatures,
     * hierarchy, synthesized members — so anything can resolve against them.
     * Only the body type-check is skipped, and compiler_compile skips lowering
     * the same range: a body whose expression types were never recorded cannot
     * be lowered.
     *
     * This exists for tests that isolate one stage. test_sir inspects the SIR of
     * USER methods and explicitly skips class_id < num_library_classes, so the
     * ~450 java.lang bodies it never looks at need not be checked or lowered.
     *
     * It is NOT a way to compile a plugin more cheaply. A plugin's imports must
     * line up with jre.wasm's exports — same names, signatures and funcidx
     * order — and both sides derive that by compiling the same prelude source in
     * the same order. Skipping work there desynchronises the two. */
    int analyze_from;


    /* Emission mode (INPUT): WHOLE / RUNTIME / PLUGIN — see sema_mode_t above. */
    sema_mode_t mode;

    /* Function table (OUTPUT, built by sema_analyze): the emitted module functions
     * in module-index order. The single authority read by the backend. */
    sema_func_ent_t* functions;   /* bbq_vec */

    /* Function imports (OUTPUT): the distinct NATIVE (bodiless) methods reached by
     * a resolved call, in first-referenced order — recorded at call resolution.
     * They become WASM function imports occupying funcidx [0, count); the backend
     * offsets defined functions past them. bbq_vec of sema_func_ent_t. */
    sema_func_ent_t* import_funcs;

    /* Well-known java.lang class ids, resolved once (resolve_wellknown). Read by
     * class_id; the single home for the canonical names — replaces the scattered
     * sema_find_class("Object"/"Throwable"/...) + strcmp(name,"Throwable") walks. */
    sema_wellknown_t wk;

    /* §10.8 per-component array Class objects: the distinct array TYPES the program
     * uses (registered during type resolution), each synthesized after body analysis
     * into its own Class object (a data-only class named by the §20.3.2 descriptor —
     * "[I", "[Ljava.lang.Object;"), so getClass()/getName() on an array is exact. Two
     * parallel bbq_vecs: array_class_types[i] is the type, array_class_ids[i] its class. */
    java_type_t* array_class_types;
    int*         array_class_ids;

    /* Diagnostics (bbq_vec of sema_diag_t) */
    sema_diag_t* diags;
} sema_ctx_t;

/* ── Public API ───────────────────────────────────────────── */

/* Initialize context. arena must be pre-initialized. */
void sema_init(sema_ctx_t* ctx, bbq_arena* arena);

/* Run semantic analysis on a parsed program. Returns true if no errors.
 * After analysis, ctx->functions holds the emitted-function table (see below). */
bool sema_analyze(sema_ctx_t* ctx, ast_program_t* program);

/* The §7.3-correct entry: one parsed program PER COMPILATION UNIT, each
 * carrying its own package declaration and import list. Classes register by
 * fully qualified name (§7.4.1); type names resolve per §6.5.4 against the
 * referencing unit's package + imports. sema_analyze(p) = the 1-unit case. */
bool sema_analyze_units(sema_ctx_t* ctx, ast_program_t** units, int n);


/* Is `m` (declared in `class_id`) emitted as a defined module function? True iff
 * its class is user code (not library) and it has a body. The single predicate
 * for "compiled into the module" — used by both the compiler and the table build
 * so the emitted set and the function index can never drift. */
bool sema_method_is_defined(const sema_ctx_t* ctx, int class_id, const sema_method_t* m);

/* The emitted-function table (ctx->functions), built by sema_analyze. */
int             sema_func_count(const sema_ctx_t* ctx);
sema_func_ent_t sema_func_at(const sema_ctx_t* ctx, int i);
/* The module function index of (class_id, method_id), or -1 if not emitted
 * (a library method → host import). The authority the InvokeStatic immediate and
 * the module assembler both read. */
int             sema_func_index(const sema_ctx_t* ctx, int class_id, int method_id);

/* Query expression type (returns JT_ERROR if not found). */
java_type_t sema_type_of(const sema_ctx_t* ctx, const ast_expr_t* expr);
java_type_t sema_instanceof_type(const sema_ctx_t* ctx, const ast_expr_t* expr);  /* §15.20.2 resolved instanceof target type */

/* Query the precomputed SIR data type for an expression.
 * Returns SEMA_DT_BYTE/SHORT/INT/REF, or -1 if the expression is
 * not in the data_types side table (not analyzed). */
int32_t sema_data_type(const sema_ctx_t* ctx, const ast_expr_t* expr);

/* JLS §15.17.3 floating-point remainder. `%` on float/double is the TRUNCATED remainder
 * (C fmod, sign of the dividend) — not Math.IEEEremainder — and WASM has no f32.rem /
 * f64.rem opcode, so the ddcg desugars it to a call to Math's fdlibm fmod (the same shape
 * as the String-concat desugar below). Returns Math's class id and the fmod overload for
 * the operand width; -1 if the helper is absent (the backend then fails loud). */
int  sema_frem_class(const sema_ctx_t* ctx);
int  sema_frem_method(const sema_ctx_t* ctx, int32_t dt);

/* JLS §15.18.1 string-concatenation support (well-known StringBuffer identities for
 * the ddcg's defunctionalizing desugar). */
bool sema_binary_is_concat(const sema_ctx_t* ctx, const ast_expr_t* node);
int  sema_string_buffer_id(const sema_ctx_t* ctx);
int  sema_sb_ctor_index(const sema_ctx_t* ctx);
int  sema_sb_tostring_index(const sema_ctx_t* ctx);
int  sema_sb_append_index(const sema_ctx_t* ctx, const ast_expr_t* operand);
int  sema_sb_append_param_class(const sema_ctx_t* ctx, const ast_expr_t* operand);

/* Query the slot index assigned to a variable declaration.
 * `decl` is an `ast_var_decl_t*` (LocalVarDecl declarator) or
 * `ast_catch_clause_t*` (catch parameter). Returns -1 on miss.
 * For `this` (slot 0 if non-static) and parameters, use
 * sema_param_slot() — those are not stored in this side table. */
int32_t sema_slot(const sema_ctx_t* ctx, const void* decl);
/* The resolved declared type of a local var_decl (the authority for its slot's ref descriptor);
 * jt_error() if the decl was not a recorded local. */
java_type_t sema_var_type(const sema_ctx_t* ctx, const void* decl);

/* Compute the slot index of a method parameter by index.
 * Returns the same value compiler.c's monotonic scope_declare would
 * assign: slot 0 is `this` for instance methods, then params take
 * slots 1..N (or 0..N-1 for static). One slot per param regardless
 * of width — matches compiler.c (a latent int-takes-2-slots bug
 * lives there independently). */
int32_t sema_param_slot(const sema_method_t* m, int idx);

/* Per-method count of pre-allocated slots (this + params + body
 * locals + catch vars). DDCG temps start at this value. */
int32_t sema_max_user_slots(const sema_method_t* m);

/* Total local slots used across all static-initializer blocks (folded into the
 * one synthesized <clinit>). The module-init builder bases its temps past these. */
int32_t sema_clinit_slots(const sema_ctx_t* ctx);

/* Query the resolved kind of an AST_IDENT expression. Returns NULL
 * for non-IDENT expressions, idents that resolve to a class name,
 * or undefined idents. The returned struct is arena-allocated and
 * valid until sema_destroy. DDCG uses this to dispatch ident loads
 * without re-doing scope_lookup or the field fallback cascade. */
const sema_ident_info_t* sema_ident_kind(const sema_ctx_t* ctx,
                                          const ast_expr_t* expr);

/* Query the resolved constructor for an AST_NEW or
 * AST_CONSTRUCTORCALL expression. Returns NULL if unresolved.
 *
 * For `new T(args)` with no explicit constructor and zero args,
 * the returned method is the synthesized super-default constructor
 * that DDCG should invokespecial. */
const sema_method_t* sema_resolved_constructor(const sema_ctx_t* ctx,
                                                const ast_expr_t* expr);

/* Query the resolved parent-class method for an AST_SUPERCALL
 * expression. Returns NULL if unresolved. (Aliases the existing
 * resolved_methods table — the type-specific name documents intent
 * at the DDCG call site.) */
const sema_method_t* sema_resolved_super_method(const sema_ctx_t* ctx,
                                                 const ast_expr_t* supercall);

/* Query the resolved parent-class field for an AST_SUPERACCESS
 * expression. Returns NULL if unresolved. (Aliases the existing
 * resolved_fields table.) */
const sema_field_t* sema_resolved_super_field(const sema_ctx_t* ctx,
                                               const ast_expr_t* superaccess);

/* Query the invoke kind for a call expression (AST_METHODCALL,
 * AST_SUPERCALL, or AST_CONSTRUCTORCALL). Returns -1 if not in the
 * side table. DDCG dispatches on this to choose between
 * sir_invoke_static / virtual / special / interface. */
int32_t sema_invoke_kind(const sema_ctx_t* ctx, const ast_expr_t* call);

/* §15 implicit-exception codegen support (backend guards synthesize `new <exc>()`). */
int sema_arithmetic_exc_id(const sema_ctx_t* ctx);
int sema_null_pointer_exc_id(const sema_ctx_t* ctx);
int sema_array_index_exc_id(const sema_ctx_t* ctx);
int sema_neg_array_size_exc_id(const sema_ctx_t* ctx);
int sema_class_cast_exc_id(const sema_ctx_t* ctx);
int sema_index_oob_exc_id(const sema_ctx_t* ctx);   /* base IndexOutOfBoundsException (§20.18.16 arraycopy) */
int sema_array_store_exc_id(const sema_ctx_t* ctx); /* ArrayStoreException (§10.10) */
bool sema_is_arraycopy(const sema_ctx_t* ctx, const ast_expr_t* node); /* §20.18.16 System.arraycopy w/ concrete-array src */
int  sema_move_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node); /* Float/Double raw bit accessor → Move* (0 none, 1 F2I, 2 I2F, 3 D2L, 4 L2D) */
bool sema_is_move_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node);   /* the predicate form (move_kind != 0) — the ddcg where-guard */
int  sema_math_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node); /* Math.sqrt/floor/ceil/rint → f64 op (0 none, 1 sqrt, 2 floor, 3 ceil, 4 rint) */
bool sema_is_math_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node);   /* the predicate form (math_kind != 0) — the ddcg where-guard */

/* javelina.simd intrinsics — resolved-call accessors over the generated table
 * (family/op) and the validated-immediates stash (lane / const halves). */
typedef struct { int32_t lane; int64_t lo, hi; } sema_simd_imm_t;
bool    sema_is_simd_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node);
int     sema_simd_family(const sema_ctx_t* ctx, const ast_expr_t* node);  /* 1..23, 0 none */
int     sema_simd_op(const sema_ctx_t* ctx, const ast_expr_t* node);      /* the WOP_* enum value */
int     sema_simd_align(const sema_ctx_t* ctx, const ast_expr_t* node);   /* memarg/memlane rows: the toml align column */
int     sema_simd_awidth(const sema_ctx_t* ctx, const ast_expr_t* node);  /* the access width in bytes (1 << align) */
int32_t sema_simd_lane(const sema_ctx_t* ctx, const ast_expr_t* node);
int64_t sema_simd_lo(const sema_ctx_t* ctx, const ast_expr_t* node);
int64_t sema_simd_hi(const sema_ctx_t* ctx, const ast_expr_t* node);
int  sema_class_intrinsic_kind(const sema_ctx_t* ctx, const ast_expr_t* node); /* §20.3.6 Class.newInstance helper (0 none, 1 instantiable?, 2 construct) */
bool sema_is_class_intrinsic(const sema_ctx_t* ctx, const ast_expr_t* node);   /* the predicate form (class_kind != 0) — the ddcg where-guard */
int sema_class_reflect_id(const sema_ctx_t* ctx);   /* java.lang.Class's class id */
int sema_refarray_id(const sema_ctx_t* ctx);        /* the synthesized RefArray class id (§10) */
int sema_primarray_id(const sema_ctx_t* ctx, int storage_index);  /* the per-width PrimArray overlay id (§10.7/§10.8) */
int sema_array_class_id(const sema_ctx_t* ctx, java_type_t arr);  /* §10.8 the Class object for an array type (getClass()), or -1 */
int sema_array_component_class(const sema_ctx_t* ctx, int class_id);  /* §10.2 an array Class's component Class, or -1 */
bool sema_is_overlay(const sema_ctx_t* ctx, int class_id);            /* is class_id a RefArray/PrimArray value overlay? */
int  sema_array_class_overlay(const sema_ctx_t* ctx, int class_id);  /* a §10.8 array Class's value overlay, or -1 */
int sema_arraystore_check_method(const sema_ctx_t* ctx); /* Class.arrayStoreCheck's method index (§10.10), -1 if absent */
int sema_is_instance_method(const sema_ctx_t* ctx); /* Class.isInstance's method index (§15.19.2), -1 if absent */
int sema_getclass_method_id(const sema_ctx_t* ctx); /* Object.getClass()'s method index, or -1 */
int sema_object_id(const sema_ctx_t* ctx);          /* the class-tree root (Object) class id */
int sema_noarg_ctor_index(const sema_ctx_t* ctx, int class_id);

/* Query the receiver/target class for a call expression. For
 * AST_METHODCALL with an explicit obj, this is the obj's class
 * (matching compiler.c:519's inline computation). For implicit
 * `this` calls, this is the current class. For AST_SUPERCALL, this
 * is the super of the current class. For AST_CONSTRUCTORCALL, this
 * is the target class. Returns -1 if not in the side table. */
int32_t sema_target_class(const sema_ctx_t* ctx, const ast_expr_t* call);

/* Query whether an expression may have observable side effects:
 * any of call, allocation, assign/compound-assign, inc/dec,
 * array/field access (may throw NPE/AIOOBE), div/rem (may throw),
 * checked cast (may throw CCE), or any subexpression containing
 * one of those. Pure expressions (literals, local loads, additions)
 * return false. DDCG's binary case uses this to decide whether the
 * LHS must be spilled to a temp before the RHS is compiled. */
bool sema_may_have_effects(const sema_ctx_t* ctx, const ast_expr_t* expr);

/* Query the precomputed array element type (atype) tag for an AST_ARRAYINIT
 * expression (10=boolean, 11=byte, 12=short, 13=int). Mirrors the
 * narrowing logic at compiler.c:786 — for int-typed array inits, all
 * elements are scanned to find the narrowest type that fits.
 * Returns -1 for non-ARRAYINIT or unanalyzed expressions. */
int32_t sema_array_init_elem_type(const sema_ctx_t* ctx, const ast_expr_t* expr);

/* Switch-statement analysis results, attached per-AST_SWITCH during
 * sema. Lets the compiler consume pre-validated, pre-sorted data
 * instead of re-deriving it. default_idx is -1 when no default
 * clause is present. degenerate_kind lets the backend short-circuit
 * to a pop+goto when the switch has no discriminating work to do. */
typedef enum {
    SEMA_SWITCH_NORMAL = 0,        /* ≥1 case, cases jump to ≥2 distinct targets */
    SEMA_SWITCH_DEFAULT_ONLY,      /* no case clauses — selector is effect, goto default */
    SEMA_SWITCH_ALL_SAME_TARGET,   /* every case (and default, if present) has the same body */
} sema_switch_degeneracy_t;

typedef struct {
    int32_t* case_values;          /* sorted ascending, cases_count entries */
    int*     case_ast_indices;     /* parallel to case_values: source AST cases[] index */
    int      cases_count;          /* non-default case count */
    int      default_idx;          /* index into the source AST's cases[] array, or -1 */
    int32_t  low, high;            /* min / max over case_values; undefined if cases_count=0 */
    sema_switch_degeneracy_t degeneracy;
} sema_switch_info_t;

/* Query the switch analysis for an AST_SWITCH statement. Returns
 * NULL for non-switch statements or un-analyzed ones. */
const sema_switch_info_t* sema_switch_info(const sema_ctx_t* ctx,
                                            const ast_stmt_t* stmt);

/* Query resolved method for a call (returns NULL if not found). */
const sema_method_t* sema_resolved_method(const sema_ctx_t* ctx, const ast_expr_t* call);

/* The ONE resolution of a catch clause's exception class (simple or fully qualified name).
 * Read by §14.19's catch-block reachability check and by the backend's exception table. */
int sema_catch_class_id(const sema_ctx_t* ctx, const ast_catch_clause_t* cc);

/* Query resolved field for an access (returns NULL if not found). */
const sema_field_t* sema_resolved_field(const sema_ctx_t* ctx, const ast_expr_t* access);

/* Compile-time constant query (JLS §15.28). Returns true and writes
 * *out_value iff `e` is an integer constant expression — a literal,
 * a reference to a static final primitive whose initializer is a
 * constant expression, or a castx of same. Used by the compiler to
 * inline references to constants like ISO7816.SW_NO_ERROR rather
 * than emit getstatic + a CP entry the Export file marks token=255
 * (inline-me), which the verifier rejects at load. */
bool sema_int_constant(const sema_ctx_t* ctx, const ast_expr_t* e,
                        int32_t* out_value);

/* Look up a class by name (returns -1 if not found). */
int sema_find_class(const sema_ctx_t* ctx, const char* name);

/* §6.5.4: the meaning of a type name `spelled` (simple or qualified) as seen
 * from compilation unit `ui` (-1 = no unit context: FQN-or-unnamed only).
 * `probe` suppresses error emission (§6.5.2 reclassification probes).
 * Returns the class id, or -1. */
int sema_resolve_type(sema_ctx_t* ctx, int ui, const char* spelled,
                      ast_srcloc loc, bool probe);

/* Get class info by index. */
const sema_class_t* sema_get_class(const sema_ctx_t* ctx, int class_id);
bool sema_class_needs_init(const sema_ctx_t* ctx, int class_id);   /* JLS §12.4 — the init-barrier gate */
int  sema_ensure_init_cp(const sema_ctx_t* ctx, int class_id);     /* the $ensure_init method index (cp), or -1 */

/* Flatten an `ast_name_t` to "a.b.c" form, allocating in the
 * sema arena. Used for class lookups that need the full dotted
 * path (sema's own type resolution, the compiler's catch-type
 * handling, etc.). */
const char* sema_name_to_str(const sema_ctx_t* ctx, const ast_name_t* n);

/* True if sub_id is the same class as super_id or extends from it
 * transitively. Walks the extends chain; does not consider
 * implemented interfaces. Returns false for invalid ids.
 *
 * This is the EXTENDS-CHAIN question (what §11.2 exception matching asks), NOT
 * JLS §4.10.2 subtyping. For "is a value of class S assignable to type T" — what
 * a cast, an `instanceof` and the points-to filter ask — use
 * sema_ref_is_subtype: an interface and `Object` are subtypes nobody's extends
 * chain mentions, and answering with this one says NO where the truth is YES. */
bool sema_is_subclass_of(const sema_ctx_t* ctx, int sub_id, int super_id);

/* ── JLS §8.4.8 virtual dispatch — THE authority ────────────────────────────
 *
 * sema_is_virtual_method: does this method dispatch through the vtable? (Not static,
 * not private, not a constructor — those are direct calls.)
 *
 * sema_same_vsig: do two methods share a virtual signature (an override)? Name +
 * parameter types + RETURN type. The return is part of the identity because it is part
 * of the WASM functype the slot is typed at — JLS 1.0 has no covariant returns, so this
 * never splits a real override, but it does stop `File.length()→long` and
 * `String.length()→int` from sharing one slot.
 *
 * sema_resolve_virtual: given the EXACT runtime class and the method a call site named
 * (its declaring class + index), which method actually RUNS? Walks `exact`'s ancestry
 * for the nearest override of that signature. Returns false when it cannot answer — an
 * abstract/undefined method, or a signature the class does not have — and a caller that
 * cannot answer must not devirtualize.
 *
 * ONE copy of this rule. The WASM vtable builder resolves the same overrides to fill its
 * slots, and it calls these; the optimizer calls these to devirtualize. A second
 * signature comparator anywhere is how the two silently disagree. */
/* JLS §8.4.8: does class_id override Object.finalize()? The escape analysis's finalizer
 * root — such an object is reachable from the finalizer thread and is never method-local. */
bool sema_class_overrides_finalize(const sema_ctx_t* ctx, int class_id);

bool sema_is_virtual_method(const sema_method_t* m);
bool sema_same_vsig(const sema_method_t* a, const sema_method_t* b);
bool sema_resolve_virtual(const sema_ctx_t* ctx, int exact_class_id,
                          int decl_class_id, int decl_method_idx,
                          int* out_class_id, int* out_method_idx);

/* JLS §4.10.2 reference subtyping between two CLASS ids: identity, the extends
 * chain, an implemented interface (transitively, including interface-extends-
 * interface), and `Object`, which every reference type is a subtype of — including
 * every interface, which no extends chain records.
 *
 * THE authority for "classOf(O) ≤ τ". A cast/instanceof asks it to keep or drop an
 * abstract object, so an answer that is wrongly NO deletes an object the value may
 * really name — unsound. Returns false for invalid ids (fail-closed: an unknown
 * class is not provably a subtype of anything, so nothing may be dropped for it). */
bool sema_ref_is_subtype(const sema_ctx_t* ctx, int sub_id, int super_id);

/* Least common superclass of a and b (walks extends chains), or
 * -1 if the chains share no ancestor reachable from sema's class
 * table. Intended for type-lattice meets. */
int sema_common_superclass(const sema_ctx_t* ctx, int a, int b);

/* Diagnostics. */
int sema_error_count(const sema_ctx_t* ctx);
const sema_diag_t* sema_diags(const sema_ctx_t* ctx, int* count);

/* The class-local index of the field a field-access expr resolves to, and the
 * class that DECLARES it (which may be a superclass of the receiver). These
 * feed the WASM-GC field path: struct.get/set need the declaring class's
 * struct type and the
 * field's position within it. -1 if the expr resolves to no field. */
int sema_field_index(const sema_ctx_t* ctx, const ast_expr_t* expr);
int sema_field_decl_class(const sema_ctx_t* ctx, const ast_expr_t* expr);

/* The called method's index (position in its declaring class's methods vec) and
 * declaring class — the WASM function identity for a call expr.
 * -1 if no method resolves. */
int sema_method_index(const sema_ctx_t* ctx, const ast_expr_t* expr);
int sema_method_decl_class(const sema_ctx_t* ctx, const ast_expr_t* expr);

/* class_id of a class-typed AST type node (instanceof / cast target); -1 if
 * not a resolvable class type. Replaces the dead CP token on that path. */
int sema_type_class_id(const sema_ctx_t* ctx, const ast_type_t* ty);

/* Break/continue target resolution. Returns the number of ρ frames
 * to skip from the current scope to reach the target frame (0 =
 * innermost). Returns -1 if the break/continue couldn't be resolved
 * (sema also reports an error in that case; codegen falls through
 * defensively). */
int sema_break_target_depth(const sema_ctx_t* ctx, const ast_stmt_t* stmt);
int sema_continue_target_depth(const sema_ctx_t* ctx, const ast_stmt_t* stmt);

/* ── Spec-facing predicates / translations ─────────────────────────
 * These live on sema because sema owns modifier semantics. Downstream
 * stages (assembler, debug emit) consume them instead of re-deriving
 * — see [[feedback_pass_info_forward]]. */

/* "Externally visible" — class/method/field is reachable from another
 * package (PUBLIC or PROTECTED). Used by §6.14 descriptor token rules
 * (token == 0xFF iff not externally visible) and by Export emission. */
static inline bool sema_class_is_exported(const sema_class_t* c) {
    return (c->modifiers & (ACC_PUBLIC | ACC_PROTECTED)) != 0;
}
static inline bool sema_method_is_exported(const sema_method_t* m) {
    return (m->modifiers & (ACC_PUBLIC | ACC_PROTECTED)) != 0;
}
static inline bool sema_field_is_exported(const sema_field_t* f) {
    return (f->modifiers & (ACC_PUBLIC | ACC_PROTECTED)) != 0;
}

/* §6.14.2: static-final primitive fields with an initializer are
 * compile-time constants; codegen inlines references and the field is
 * excluded from descriptor.fields[] and the static_field_image. */
static inline bool sema_field_is_inlined_constant(const sema_field_t* f) {
    if (!f->init_expr) return false;
    if ((f->modifiers & (ACC_STATIC | ACC_FINAL)) != (ACC_STATIC | ACC_FINAL))
        return false;
    return f->type.tag == JT_BYTE || f->type.tag == JT_SHORT
        || f->type.tag == JT_INT  || f->type.tag == JT_BOOL;
}

/* If the field has a compile-time integer-constant initializer (a literal
 * or negated literal), write its value and return true. Used by the
 * assembler to bake non-default static field values into the §6.10 image. */
bool sema_field_const_int(const sema_field_t* f, int32_t* out);

/* Spec→sema modifier-bit translations. The CAP §6.14 tables use a
 * different bit layout from sema's internal modifier set, and each of
 * class/method/field has its own bit assignment (Tables 6-17/6-20/6-18). */
uint8_t  sema_class_access_flags(const sema_class_t* c);      /* §6.14.2 Table 6-17 */
uint8_t  sema_method_access_flags(const sema_method_t* m);    /* §6.14.4 Table 6-20 */
uint8_t  sema_field_access_flags(const sema_field_t* f);      /* §6.14.3 Table 6-18 */
uint16_t sema_class_debug_access_flags(const sema_class_t* c); /* §6.15.2 Table 6-21 */

bool sema_uses_exceptions(const sema_ctx_t* ctx);

/* §6.9.2.3: collect the transitive set of interfaces implemented by
 * `c` — direct interfaces, their superinterfaces, and the closure
 * walked through `c`'s super-classes. Order is super-before-sub by
 * insertion. Returns count written to `out` (caller-allocated). */
int sema_transitive_interfaces(const sema_ctx_t* ctx, const sema_class_t* c,
                                 const sema_class_t** out, int max_out);

/* §6.9.2.5: find the concrete method on `cls` (or any superclass)
 * that implements `iface_method`, by full signature match
 * (name + return type + parameter type sequence). Returns NULL if no
 * implementer is found — sema reports the missing-impl error
 * elsewhere; this resolver is for assembler consumption of the
 * implemented_interface_info.index[] mapping. */
const sema_method_t* sema_implementing_method(const sema_ctx_t* ctx,
                                                const sema_class_t* cls,
                                                const sema_method_t* iface_method);

/* Get imported packages registered via sema_load_export(). */

/* Function imports = the distinct native (bodiless) methods a resolved call
 * targets, in funcidx order. The backend numbers these [0, count) and offsets
 * defined functions past them; the import section is emitted from this list. */
int sema_import_count(const sema_ctx_t* ctx);
sema_func_ent_t sema_import_at(const sema_ctx_t* ctx, int i);

/* Format a diagnostic as "file:line:col: level: message".
 * Returns characters written (excluding NUL). */
int sema_diag_format(const sema_diag_t* d, char* buf, int bufsize);

/* Release resources (htrees, vecs — arena is caller's responsibility). */
void sema_destroy(sema_ctx_t* ctx);

#endif /* SEMA_H */
