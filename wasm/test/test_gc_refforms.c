// test_gc_refforms.c — §4.2.1's value production, complete, against the live collector.
//
//   val  ::= num | vec | ref
//   num  ::= numtype.const num_numtype                    (i32 i64 f32 f64)
//   vec  ::= vectype.const vec_vectype                    (v128)
//   ref  ::= ref.i31 u31 | ref.null | ref.struct structaddr | ref.array arrayaddr
//          | ref.func funcaddr | ref.exn exnaddr | ref.host hostaddr | ref.extern ref
//
// §4.2.1: "Any of the aforementioned references can furthermore be wrapped up as an external
// reference" — so ref.extern may wrap a ref.struct, and an externref CAN carry a store address.
// §4.2.3: the store's collectable instances are structs, arrays and exns.
// => the tracer must follow EXACTLY: ref.struct, ref.array, ref.exn, and ref.extern wrapping one
//    of those; and must never dereference ref.i31 (a 31-bit integer), ref.null, ref.func
//    (a funcaddr) or ref.host (an embedder address).
//
// javelina decides this with ONE BIT per aggregate (gc_rtt_t.elem_is_ref, from rtt_field_is_ref,
// jav_module_index.c:451). jav_gc.c:142 hands every selected element to gc_mark1, whose only guards
// are NULL and the all-ones sentinel before `o->forward`. One bit cannot separate the inhabitants of
// anyref/eqref (ref.i31 vs ref.struct) or of externref (ref.host vs ref.extern-wrapping-a-struct):
// whichever way it is set, one inhabitant is handled wrongly — dereferenced as a pointer, or dropped
// while live.
//
// Everything here is BEHAVIOURAL: it asserts what the collector does, over a water-assembled module
// (gc_refforms.wat) driven through the real loader → validator → instantiate → jav_call → GC path.
// No type index, gc_rtt_t layout, or opcode byte is restated on this side — the .wat is the source
// of those and the engine is the only authority consulted.
//
// Each case runs in a FORKED CHILD, so a case that faults inside the collector is attributed to its
// §4.2.1 form instead of taking the matrix down with it.
#include "interp.h"
#include "heap.h"
#include "jav_hostref.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "jav_instance.h"
#include "jav_subtype.h"
#include "jav_view_nav.h"
#include "bbq_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// CHK_SURVIVES_GC: call `name`, collect twice, then call `check` and require 42 — the witness is
//   read from the INNER object, so a missed trace shows up as a freed payload rather than being
//   masked by the outer aggregate (which is a root and survives unconditionally).
// CHK_NO_FAULT:    call `name`, collect twice; the only claim is that the collector did not
//   dereference the stored value. A signal is the finding.
// CHK_I32_ONE:     call `name`, require 1 — value fidelity, compared inside wasm.
typedef enum { CHK_SURVIVES_GC, CHK_NO_FAULT, CHK_I32_ONE } check_t;

#define WITNESS      42
#define R_PASS       1
#define R_CALL_BAD  (-1)
#define R_NO_EXPORT (-2)
#define R_LOAD_BAD  (-3)
#define R_NULL_REF  (-4)
#define R_WRONG_VAL (-5)
#define R_CHECK_BAD (-6)

// §4.2.1 ref.host: "an uninterpreted form of host address defined by the embedder". The engine's
// representation contract is that a host address enters the VM ONLY through jav_host_box_new — a
// gc_obj_t wrapping the pointer — because externref slots are traced (§2.3.4 makes extern/any
// isomorphic and javelina converts by identity, so an externref can hold a live aggregate). The
// c-api boxes at every entry (wasm_capi.c val_to_slot / global write); an embedder passing a RAW
// pointer with tag T_REF violates the contract. An earlier version of this file did exactly that
// — a state invented from jav_extern_t's fields, not from any documented entry path.
static uint64_t host_object[4] = { 0xC0FFEE, 0, 0, 0 };
static slot_t   host_slot;

static int call_export(vm_t* vm, jav_instance_t* inst, const char* name){
    int32_t fx = jav_instance_export(inst, name, 0);
    if (fx < 0) return R_NO_EXPORT;
    vm->frame.sp = 0; vm->frame.num_locals = 0;
    return jav_call(vm, vm->heap, (u4)fx) == JAV_OK ? R_PASS : R_CALL_BAD;
}

static int run_case(const char* name, const char* check_name, check_t check){
    FILE* f = fopen("gc_refforms.wasm", "rb");
    if (!f) return R_LOAD_BAD;
    fseek(f,0,SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf,1,(size_t)n,f) != (size_t)n) { fclose(f); return R_LOAD_BAD; }
    fclose(f);

    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, &a);
    if (!m.success) return R_LOAD_BAD;
    jav_modidx_t mod;
    if (!jav_module_index(m.root, buf, &a, &mod)) return R_LOAD_BAD;
    jav_err_t err;
    if (jav_module_validate(m.root, buf, &mod, &err) != JAV_OK) return R_LOAD_BAD;

    struct heap_t heap; memset(&heap,0,sizeof heap);
    vm_t vm; memset(&vm,0,sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
    jav_heap_gc_init(&heap, &vm);      /* collector first, as the c-api does at store creation —
                                        * boxing the host import below allocates */

    // the imported externref global carrying a §4.2.1 ref.host — BOXED, per the contract above
    host_slot.l = (s8)(uintptr_t)jav_host_box_new(&vm, host_object);
    jav_extern_t imp; memset(&imp, 0, sizeof imp);
    imp.kind = 3;
    imp.u.global.slot = &host_slot;
    imp.u.global.type = WVT_REF;
    imp.u.global.type_ht = HT_EXTERN;
    imp.u.global.mut = 0;
    imp.u.global.tag = T_GCREF;        // the box is a managed heap object

    jav_instance_t inst;
    if (jav_instantiate(&vm, m.root, buf, &mod, &imp, 1, &inst, &err) != JAV_OK) return R_LOAD_BAD;
    jav_instance_bind(&vm, &inst);

    int r = call_export(&vm, &inst, name);
    if (r != R_PASS) return r;

    if (check == CHK_I32_ONE) return jav_tos(&vm).i == 1 ? R_PASS : R_WRONG_VAL;

    /* The aggregate is rooted — as this call's result for CHK_NO_FAULT, or in the $held global for
     * CHK_SURVIVES_GC — so these collections trace its elements. Twice, so evacuation also runs. */
    heap.gc.collect(heap.gc.self);
    heap.gc.collect(heap.gc.self);

    if (check == CHK_NO_FAULT) return jav_tos(&vm).l == 0 ? R_NULL_REF : R_PASS;

    /* Reach the INNER object and read its payload back. A trace that was skipped frees it. */
    r = call_export(&vm, &inst, check_name);
    if (r == R_CALL_BAD) return R_CHECK_BAD;      /* the CHECK trapped, not the build */
    if (r != R_PASS) return r;
    return jav_tos(&vm).i == WITNESS ? R_PASS : R_WRONG_VAL;
}

typedef struct { const char* form; const char* export_name; const char* check_name; check_t check; } case_t;

static int fails = 0;
static void chk(const case_t* c){
    int fd[2]; if (pipe(fd)) { perror("pipe"); exit(2); }
    pid_t p = fork();
    if (p == 0) { close(fd[0]); int v = run_case(c->export_name, c->check_name, c->check);
                  ssize_t w = write(fd[1], &v, sizeof v); (void)w; _exit(0); }
    close(fd[1]);
    int v = 0; ssize_t got = read(fd[0], &v, sizeof v); close(fd[0]);
    int status = 0; waitpid(p, &status, 0);
    char buf[64]; const char* how;
    /* A signal can arrive from the collector OR from the read-back (both dereference refs), so do
     * not name a culprit here — the label said "in collector" and was wrong for the i31 read-back
     * rows, which faulted in value_heaptype. */
    if (WIFSIGNALED(status)) { snprintf(buf,sizeof buf,"FAULT (sig %d)", WTERMSIG(status)); how = buf; }
    else if (got != (ssize_t)sizeof v) how = "no result";
    else switch (v) {
        case R_PASS:      how = (c->check == CHK_I32_ONE)     ? "value round-trips"
                              : (c->check == CHK_SURVIVES_GC) ? "inner payload survives"
                                                              : "not dereferenced";  break;
        case R_CALL_BAD:  how = "BUILD trapped";               break;
        case R_CHECK_BAD: how = "CHECK trapped (read-back)";   break;
        case R_NO_EXPORT: how = "export missing";              break;
        case R_LOAD_BAD:  how = "module load failed";          break;
        case R_NULL_REF:  how = "ref NULLED by the GC";        break;
        case R_WRONG_VAL: how = "PAYLOAD LOST (freed while live)"; break;
        default:          how = "unknown";                     break;
    }
    int ok = !WIFSIGNALED(status) && got == (ssize_t)sizeof v && v == R_PASS;
    printf("  %-38s %-30s [%s]\n", c->form, how, ok ? "PASS" : "FAIL");
    fails += !ok;
}

int main(void){
    static const case_t cases[] = {
      /* ── ref forms carrying a STORE address: the tracer must follow them, and the witness
       *    is read from the INNER object after the collection ──────────────────────────── */
      { "ref.struct  in structref[]",  "build_struct_in_structref",   "check_struct_in_structref",   CHK_SURVIVES_GC },
      { "ref.struct  in anyref[]",     "build_struct_in_anyref",      "check_struct_in_anyref",      CHK_SURVIVES_GC },
      { "ref.struct  in eqref[]",      "build_struct_in_eqref",       "check_struct_in_eqref",       CHK_SURVIVES_GC },
      { "ref.array   in arrayref[]",   "build_array_in_arrayref",     "check_array_in_arrayref",     CHK_SURVIVES_GC },
      { "ref.array   in anyref[]",     "build_array_in_anyref",       "check_array_in_anyref",       CHK_SURVIVES_GC },
      { "ref.exn     in exnref[]",     "build_exn_in_exnref",         "check_exn_in_exnref",         CHK_SURVIVES_GC },
      { "ref.extern(struct) in extref[]","build_extern_wrapping_struct","check_extern_wrapping_struct",CHK_SURVIVES_GC },
      /* ── ref forms carrying NO store address: never dereference them ─────────────────── */
      { "ref.i31     in i31ref[]",     "i31_in_i31ref",     NULL, CHK_NO_FAULT },
      { "ref.i31     in anyref[]",     "i31_in_anyref",     NULL, CHK_NO_FAULT },
      { "ref.i31     in eqref[]",      "i31_in_eqref",      NULL, CHK_NO_FAULT },
      { "ref.func    in funcref[]",    "func_in_funcref",   NULL, CHK_NO_FAULT },
      { "ref.host    in externref[]",  "host_in_externref", NULL, CHK_NO_FAULT },
      { "ref.null    in externref[]",  "null_in_externref", NULL, CHK_NO_FAULT },
      { "ref.null    in anyref[]",     "null_in_anyref",    NULL, CHK_NO_FAULT },
      { "ref.null    in funcref[]",    "null_in_funcref",   NULL, CHK_NO_FAULT },
      { "ref.null    in exnref[]",     "null_in_exnref",    NULL, CHK_NO_FAULT },
      /* ── num / vec: the stored form must read back identical ─────────────────────────── */
      { "num i32.const round-trip",    "num_i32",       NULL, CHK_I32_ONE },
      { "num i64.const round-trip",    "num_i64",       NULL, CHK_I32_ONE },
      { "num f32.const round-trip",    "num_f32",       NULL, CHK_I32_ONE },
      { "num f64.const round-trip",    "num_f64",       NULL, CHK_I32_ONE },
      { "vec v128.const round-trip",   "vec_v128",      NULL, CHK_I32_ONE },
      /* ── ref.i31's tagged representation (v << 3) | 1 — the properties that make it sound.
       *    NOT implied by test_i31, which was written against the untagged encoding and passes
       *    either way; nor by the 60113-case corpus, which stayed green through this change. ── */
      { "i31 0 is not null",           "i31_zero_not_null",  NULL, CHK_I32_ONE },
      { "i31 u31-max is not null",     "i31_max_not_null",   NULL, CHK_I32_ONE },
      { "i31.get_u 0",                 "i31_get_u_zero",     NULL, CHK_I32_ONE },
      { "i31.get_u u31-max",           "i31_get_u_max",      NULL, CHK_I32_ONE },
      { "i31.get_s u31-max = -1",      "i31_get_s_max",      NULL, CHK_I32_ONE },
      { "i31.get_s bit30 = INT31_MIN", "i31_get_s_signbit",  NULL, CHK_I32_ONE },
      { "ref.i31 masks to 31 bits",    "i31_masks_high_bits",NULL, CHK_I32_ONE },
      { "ref.test i31ref on an i31",   "i31_is_i31",         NULL, CHK_I32_ONE },
      { "ref.test structref on an i31","i31_is_not_struct",  NULL, CHK_I32_ONE },
      { "ref.eq equal i31s",           "i31_eq_same",        NULL, CHK_I32_ONE },
      { "ref.eq differing i31s",       "i31_eq_diff",        NULL, CHK_I32_ONE },
      /* payload INTACT across evacuation (gc_mark1's return rewrites the slot), at boundaries */
      { "i31 u31-max intact across GC","build_i31_max_gc",    "check_i31_max_gc",     CHK_SURVIVES_GC },
      { "i31 0 intact across GC",      "build_i31_zero_gc",   "check_i31_zero_gc",    CHK_SURVIVES_GC },
      { "i31 bit30 intact across GC",  "build_i31_signbit_gc","check_i31_signbit_gc", CHK_SURVIVES_GC },
      /* ── ref.null's representation. It MOVED (all-ones -> 0) because §2.3.4's one reserved tag
       *    bit makes every i31 odd, so null must be even. Nothing pinned the bit pattern, so every
       *    site that spelled null as a LITERAL instead of through JAV_NULLREF kept the old value
       *    and quietly stopped being null (jav_instance.c's table fill was one: 3632 corpus cases).
       *    One row per container that has to produce a null — a literal breaks exactly one. ── */
      { "ref.null func",               "null_func_is_null",       NULL, CHK_I32_ONE },
      { "ref.null extern",             "null_extern_is_null",     NULL, CHK_I32_ONE },
      { "ref.null any",                "null_any_is_null",        NULL, CHK_I32_ONE },
      { "ref.null exn",                "null_exn_is_null",        NULL, CHK_I32_ONE },
      { "ref.null none",               "null_none_is_null",       NULL, CHK_I32_ONE },
      { "ref.null nofunc",             "null_nofunc_is_null",     NULL, CHK_I32_ONE },
      { "ref.null i31",                "null_i31_is_null",        NULL, CHK_I32_ONE },
      { "zero-init ref local is null", "null_local_is_null",      NULL, CHK_I32_ONE },
      { "untouched funcref table slot","null_table_slot_funcref", NULL, CHK_I32_ONE },
      { "untouched anyref table slot", "null_table_slot_anyref",  NULL, CHK_I32_ONE },
      { "table.set null round-trips",  "null_table_set_get",      NULL, CHK_I32_ONE },
      { "table.fill null round-trips", "null_table_fill_get",     NULL, CHK_I32_ONE },
      { "table.grow null round-trips", "null_table_grow_get",     NULL, CHK_I32_ONE },
      { "default struct ref field",    "null_struct_field",       NULL, CHK_I32_ONE },
      { "default array ref element",   "null_array_elem",         NULL, CHK_I32_ONE },
      { "ref.func is NOT null",        "funcref_is_not_null",     NULL, CHK_I32_ONE },
      { "struct ref is NOT null",      "struct_is_not_null",      NULL, CHK_I32_ONE },
      { "i31 0 is NOT null",           "i31_zero_is_not_null2",   NULL, CHK_I32_ONE },
      /* ── §4.2.1 default values ───────────────────────────────────────────────────────── */
      { "default_iN  = (iN.const 0)",  "default_i32",   NULL, CHK_I32_ONE },
      { "default_fN  = (fN.const +0)", "default_f64",   NULL, CHK_I32_ONE },
      { "default_vN  = (vN.const 0)",  "default_v128",  NULL, CHK_I32_ONE },
      { "default_ref null ht = ref.null","default_ref", NULL, CHK_I32_ONE },
    };
    printf("§4.2.1 value production vs the collector (real loader -> instantiate -> GC):\n");
    for (unsigned i = 0; i < sizeof cases/sizeof cases[0]; i++) { chk(&cases[i]); fflush(stdout); }
    printf("\n§4.2.1 complete value production: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails ? 1 : 0;
}
