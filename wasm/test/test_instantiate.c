// test_instantiate.c — Phase 2: the loader pipeline through the §4.5 instance.
// index → §7 validate → instantiate → export lookup → call, plus global-init eval
// (step c): numeric inits, global.get chaining (imports + earlier globals in scope), and
// ref.func in a global init. No hand-wired tables — jav_instantiate builds them. Step (a)
// host imports: positional type-matched externvals dropped into the low slots, a host
// callback invoked through an imported func, and the unlinkable type-mismatch verdict.
#include "jav_view_nav.h"
#include "jav_module_index.h"
#include "jav_module_validate.h"
#include "jav_instance.h"
#include "heap.h"          // struct heap_t / jav_mem_t / jav_heap_free_mems (linear memory)
#include "bbq_vec.h"
#include "bbq_arena.h"
#include "interp.h"
#include "runtime_api.h"   // jav_call, T_*, JAV_NULLREF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c) do { if (!(c)) { printf("  FAIL: %s\n", #c); fails++; } } while (0)

// A host import IS an invoke thunk: returns its single i32 param + 1.
static jav_status_t host_inc(vm_t* vm, heap_t* h, void* ctx) {
    (void)h; (void)ctx;                       /* results go on the frame stack, like a wasm callee */
    vm->frame.stack[0].i = vm->frame.locals[0].i + 1;
    vm->frame.stack_types[0] = T_INT; vm->frame.sp = 1; return JAV_RETURN;
}

// Build a func extern over a host invoke thunk of the (i32)->i32 shape.
static const jav_valtype_t TY_I32[] = { WVT_I32 };
static const jav_functype_t FT_I32_I32 = { TY_I32, 1, TY_I32, 1, NULL, NULL };
static jav_extern_t host_func_ext(jav_status_t (*fn)(vm_t*, heap_t*, void*)) {
    jav_extern_t x; memset(&x, 0, sizeof x);
    x.kind = 0; x.u.func.type = &FT_I32_I32;
    x.u.func.func.invoke = fn;
    x.u.func.func.num_params = 1; x.u.func.func.num_results = 1;
    return x;
}
// A global import BY REFERENCE: the extern carries the exporter's slot, so `backing` (caller-
// owned, must outlive instantiation) holds the value and globals[i] will alias it.
static jav_extern_t global_i32_ext(slot_t* backing, int32_t v, uint8_t mut) {
    backing->i = v;
    jav_extern_t x; memset(&x, 0, sizeof x);
    x.kind = 3; x.u.global.type = WVT_I32; x.u.global.mut = mut; x.u.global.slot = backing;
    return x;
}

// load → index → §7 validate → instantiate (with positional host imports); arena/buf
// returned for the caller to free.
static int load_inst(const char* path, bbq_arena* a, uint8_t** pbuf,
                     jav_modidx_t* mod, jav_instance_t* inst, vm_t* vm,
                     const jav_extern_t* imports, uint32_t nimports, jav_status_t* inst_st) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { perror("fread"); exit(2); }
    fclose(f); *pbuf = buf;
    bbq_arena_init(a, 0);
    bbq_capture_metadata m = jav_view_module(buf, (size_t)n, a);
    if (!m.success) return 0;
    if (!jav_module_index(m.root, buf, a, mod)) return 0;
    jav_err_t err;
    if (jav_module_validate(m.root, buf, mod, &err) != JAV_OK) return 0;
    jav_status_t s = jav_instantiate(vm, m.root, buf, mod, imports, nimports, inst, &err); // vm first (the engine/self)
    if (inst_st) *inst_st = s;
    return 1;
}

int main(void) {
    // ── rich.wasm: imports a host func (i32->i32) + an i32 global into the low slots; the
    // exported "add" is funcidx 1, defined (mut i64) global = 7 at index 1 ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        slot_t gimp = {0};
        jav_extern_t imps[2] = { host_func_ext(host_inc), global_i32_ext(&gimp, 99, 0) };
        jav_status_t s;
        CK(load_inst("rich.wasm", &a, &buf, &mod, &inst, &vm, imps, 2, &s));
        CK(s == JAV_OK);
        CK(mod.nimport_funcs == 1 && (uint32_t)bbq_vec_len(inst.funcs) == 3);
        CK(inst.funcs[0].invoke == host_inc);      // import installed in the low slot
        int32_t add = jav_instance_export(&inst, "add", 0);
        CK(add == 1);
        CK(mod.nimport_globals == 1 && (uint32_t)bbq_vec_len(inst.globals) == 2);
        CK(inst.globals[0]->i == 99);                      // imported global value linked in
        CK(inst.globals[1]->l == 7);                       // defined (mut i64) (i64.const 7)
        CK(heap.mems[0].size == 65536);                   // (memory 1 4) → 1 page
        CK(memcmp(heap.mems[0].data, "hi", 2) == 0);      // active (data (i32.const 0) "hi")
        CK((uint32_t)bbq_vec_len(inst.tables[0]->refs) == 2 && inst.tables[0]->refs[0] == (s8)(uintptr_t)&inst.funcs[1]); // active (elem func 1) — funcref = &funcinst
        CK((uint32_t)bbq_vec_len(inst.data_segs) == 1 && inst.data_dropped[0] == 1); // active data dropped post-init

        jav_instance_bind(&vm, &inst);
        CK(vm.frame.ctx->tables == inst.tables);                    // §8: bind points the active context at the instance
        CK(vm.frame.ctx->globals[1]->l == 7);                       // bound onto the vm (via frame.ctx)
        vm.frame.stack[0].i = 3; vm.frame.stack_types[0] = T_INT;
        vm.frame.stack[1].i = 5; vm.frame.stack_types[1] = T_INT;
        vm.frame.sp = 2; vm.frame.num_locals = 0;
        CK(jav_call(&vm, vm.heap, (u4)add) == JAV_OK && jav_tos(&vm).i == 8);
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── rich.wasm with a TYPE-MISMATCHED func import → JAV_UNLINKABLE ("incompatible import
    // type"). The supplied func is a global instead — wrong kind at position 0. ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        slot_t gb0 = {0}, gb1 = {0};
        jav_extern_t imps[2] = { global_i32_ext(&gb0, 0, 0), global_i32_ext(&gb1, 0, 0) };  // pos 0 should be a func
        jav_status_t s = JAV_OK;
        CK(load_inst("rich.wasm", &a, &buf, &mod, &inst, &vm, imps, 2, &s));
        CK(s == JAV_UNLINKABLE);
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── arity mismatch: rich declares 2 imports, supply 1 → JAV_UNLINKABLE ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_extern_t imps[1] = { host_func_ext(host_inc) };
        jav_status_t s = JAV_OK;
        CK(load_inst("rich.wasm", &a, &buf, &mod, &inst, &vm, imps, 1, &s));
        CK(s == JAV_UNLINKABLE);
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── import_call.wasm: an exported func that CALLS the imported host func — exercises
    // the host invoke seam through a linked import ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_extern_t imps[1] = { host_func_ext(host_inc) };
        jav_status_t s;
        CK(load_inst("import_call.wasm", &a, &buf, &mod, &inst, &vm, imps, 1, &s));
        CK(s == JAV_OK);
        int32_t callit = jav_instance_export(&inst, "callit", 0);
        CK(callit >= 0);
        jav_instance_bind(&vm, &inst);
        vm.frame.stack[0].i = 41; vm.frame.stack_types[0] = T_INT;
        vm.frame.sp = 1; vm.frame.num_locals = 0;
        CK(jav_call(&vm, vm.heap, (u4)callit) == JAV_OK && jav_tos(&vm).i == 42);  // host_inc(41)
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── import_memtab.wasm: imports a memory + a table; active data/elem write into the
    //    embedder-owned (borrowed) storage, reached through mem_addrs / a borrowed table 0 ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        uint32_t hmem = (uint32_t)jav_mem_add(&heap, 1, 65536, 1, 0);   // host memory in the store
        int64_t* htab = NULL; uint8_t* htty = NULL;   // slot-sized refs + parallel tags (T_REF funcref slots)
        { int64_t nul = -1; uint8_t ty = T_REF;
          bbq_vec_push(htab, nul); bbq_vec_push(htty, ty); bbq_vec_push(htab, nul); bbq_vec_push(htty, ty); }
        jav_tableinst_t host_ti; memset(&host_ti, 0, sizeof host_ti);   // §4.2.4 the imported table IS a store object
        host_ti.refs = htab; host_ti.types = htty; host_ti.reftype = WVT_REF; host_ti.reftype_ht = (int32_t)HT_FUNC;
        jav_extern_t imps[2]; memset(imps, 0, sizeof imps);
        imps[0].kind = 2; imps[0].u.mem.memidx = hmem; imps[0].u.mem.min = 1; imps[0].u.mem.is64 = 0;
        imps[1].kind = 1; imps[1].u.table.tab = &host_ti;   // share the host table by pointer
        jav_status_t s;
        CK(load_inst("import_memtab.wasm", &a, &buf, &mod, &inst, &vm, imps, 2, &s));
        CK(s == JAV_OK);
        CK(inst.tables[0] == &host_ti && inst.tables[0]->refs == htab);  // table 0 IS the imported store table (shared)
        CK(memcmp(heap.mems[hmem].data, "yo", 2) == 0);   // active data into the imported memory
        CK(htab[0] == (s8)(uintptr_t)&inst.funcs[0]);      // active elem (func 0) into the imported table — funcref = &funcinst
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); bbq_vec_free(htab); bbq_vec_free(htty); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── refs.wasm: imports an i32 global (low slot 0); global.get chaining + ref.func init
    //   + active elem (func 0 1). g1 $a=(i32.const 1); g2 $b=(global.get 1)->$a=1; g3 $c=(ref.func 0)
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        slot_t gimp = {0};
        jav_extern_t imps[1] = { global_i32_ext(&gimp, 7, 0) };
        CK(load_inst("refs.wasm", &a, &buf, &mod, &inst, &vm, imps, 1, NULL));
        CK((uint32_t)bbq_vec_len(inst.globals) == 4 && mod.nimport_globals == 1);
        CK(inst.globals[0]->i == 7);                        // imported i32 global linked in
        CK(inst.globals[1]->i == 1);                        // (i32.const 1)
        CK(inst.globals[2]->i == 1);                        // (global.get 1) sees $a
        CK(inst.globals[3]->l == (s8)(uintptr_t)&inst.funcs[0]);  // (ref.func 0) -> &funcinst (word-sized ref)
        CK((uint32_t)bbq_vec_len(inst.tables[0]->refs) == 4);                 // (table 4 funcref)
        CK(inst.tables[0]->refs[0] == (s8)(uintptr_t)&inst.funcs[0] && inst.tables[0]->refs[1] == (s8)(uintptr_t)&inst.funcs[1]);  // funcref = &funcinst (func 0 1)
        CK((uint32_t)bbq_vec_len(inst.elem_segs) == 2);    // active + the passive (ref.func 1)(ref.null) stashed
        CK(inst.elem_segs[1].len == 2 && inst.elem_segs[1].values[0] == (s8)(uintptr_t)&inst.funcs[1] && inst.elem_segs[1].values[1] == (s8)JAV_NULLREF);  // (ref.func 1)(ref.null)
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── passive_segs.wasm: passive + active data/elem segments — the instantiator stashes every
    //    segment (memory.init / array.new_* reach passive ones); active ones applied then dropped ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_status_t s;
        CK(load_inst("passive_segs.wasm", &a, &buf, &mod, &inst, &vm, NULL, 0, &s));
        CK(s == JAV_OK);
        CK((uint32_t)bbq_vec_len(inst.data_segs) == 2);
        CK(inst.data_dropped[0] == 0 && memcmp(inst.data_segs[0].bytes, "hi", 2) == 0); // passive data stashed
        CK(inst.data_dropped[1] == 1 && memcmp(heap.mems[0].data, "XY", 2) == 0);       // active applied + dropped
        CK((uint32_t)bbq_vec_len(inst.elem_segs) == 2);
        CK(inst.elem_segs[0].len == 1 && inst.elem_segs[0].values[0] == (s8)(uintptr_t)&inst.funcs[0]);  // passive elem stashed (funcref = &funcinst)
        CK(inst.tables[0]->refs[0] == (s8)(uintptr_t)&inst.funcs[0]);                      // active elem applied (funcref = &funcinst)
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── start_ok.wasm (§4.5.10): no imports; start runs at instantiate, sets global 0 = 42 ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_status_t s;
        CK(load_inst("start_ok.wasm", &a, &buf, &mod, &inst, &vm, NULL, 0, &s));
        CK(s == JAV_OK);
        CK(vm.frame.ctx->globals[0]->i == 42);                      // §8: start ran — read via the bound context
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── start_trap.wasm: start traps (unreachable) → JAV_UNINSTANTIABLE ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_status_t s = JAV_OK;
        CK(load_inst("start_trap.wasm", &a, &buf, &mod, &inst, &vm, NULL, 0, &s));
        CK(s == JAV_UNINSTANTIABLE);                       // instantiate failed via the start trap
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── import subtyping (§3.3.16): import_gsub imports an IMMUTABLE (global funcref) [nullable].
    //    A provided NON-NULL (ref func) global links by subtyping — exact-match would reject it. ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_extern_t g; memset(&g, 0, sizeof g); slot_t gb = {0};
        g.kind = 3; g.u.global.type = WVT_REF_NN; g.u.global.type_ht = (int32_t)HT_FUNC; g.u.global.mut = 0; g.u.global.slot = &gb;  // (ref func)
        jav_status_t s = JAV_OK;
        CK(load_inst("import_gsub.wasm", &a, &buf, &mod, &inst, &vm, &g, 1, &s));
        CK(s == JAV_OK);                                   // (ref func) <: (ref null func)
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // ── the verifier is not nerfed: an externref global for a funcref import → distinct
    //    hierarchy → not a subtype → JAV_UNLINKABLE. ──
    {
        bbq_arena a; uint8_t* buf; jav_modidx_t mod; jav_instance_t inst;
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        vm_t vm; memset(&vm, 0, sizeof vm); jav_vm_init(&vm); vm.heap = &heap;
        jav_extern_t g; memset(&g, 0, sizeof g);
        g.kind = 3; g.u.global.type = WVT_REF; g.u.global.type_ht = (int32_t)HT_EXTERN; g.u.global.mut = 0;
        jav_status_t s = JAV_OK;
        CK(load_inst("import_gsub.wasm", &a, &buf, &mod, &inst, &vm, &g, 1, &s));
        CK(s == JAV_UNLINKABLE);                           // externref ≰ funcref
        jav_vm_free(&vm); jav_instance_free(&inst); jav_modidx_free_bodies(&mod); jav_heap_free_mems(&heap); bbq_arena_free(&a); free(buf);
    }
    // Cross-module concrete (ref $func) GLOBAL subtyping matches through the §4.5.2 session
    // registry, which reads the provider's gcanon off a REAL instance — so it is exercised
    // end-to-end in test/regress_xmod_subtype.wast (provider + importer modules through the
    // store), not via a synthetic hand-built externval that has no defining instance here.
    // ── cross-instance store (§4.2.3): A owns a memory and writes 42 to mem[8]; B imports A's
    //    memory and reads mem[8]. ONE shared store/heap, with a dummy memory at store index 0
    //    (modeling a prior instance) so A's memory is NOT at heap index 0. This exposes whether
    //    a memidx resolves through the instance's memaddr map (the spec store) or as a raw heap
    //    index. Same meminst ⇒ A's write is visible to B. ──
    {
        struct heap_t heap; memset(&heap, 0, sizeof heap);
        (void)jav_mem_add(&heap, 1, 65536, 1, 0);   // dummy memory at store index 0 (a prior "instance")

        bbq_arena aa; uint8_t* bufa; jav_modidx_t moda; jav_instance_t insta;
        vm_t vma; memset(&vma, 0, sizeof vma); jav_vm_init(&vma); vma.heap = &heap;
        jav_status_t sa;
        CK(load_inst("xinst_a.wasm", &aa, &bufa, &moda, &insta, &vma, NULL, 0, &sa));
        CK(sa == JAV_OK);
        uint32_t a_memaddr = insta.mem_addrs[0];
        CK(a_memaddr == 1);                       // A's memory is the 2nd in the store (after the dummy)

        jav_instance_bind(&vma, &insta);          // A.poke → writes 42 to A's memory[8]
        int32_t poke = jav_instance_export(&insta, "poke", 0); CK(poke >= 0);
        vma.frame.sp = 0; vma.frame.num_locals = 0;
        CK(jav_call(&vma, vma.heap, (u4)poke) == JAV_OK);
        CK(heap.mems[a_memaddr].data[8] == 42);   // the write must land in A's ACTUAL memory (store idx 1)

        bbq_arena ab; uint8_t* bufb; jav_modidx_t modb; jav_instance_t instb;   // B imports A's memory
        vm_t vmb; memset(&vmb, 0, sizeof vmb); jav_vm_init(&vmb); vmb.heap = &heap;
        jav_extern_t imp; memset(&imp, 0, sizeof imp);
        imp.kind = 2; imp.u.mem.memidx = a_memaddr; imp.u.mem.min = 1; imp.u.mem.is64 = 0;
        jav_status_t sb;
        CK(load_inst("xinst_b.wasm", &ab, &bufb, &modb, &instb, &vmb, &imp, 1, &sb));
        CK(sb == JAV_OK);
        CK(instb.mem_addrs[0] == a_memaddr);      // B borrowed A's memory address

        jav_instance_bind(&vmb, &instb);          // B.peek → must see A's write (same meminst)
        int32_t peek = jav_instance_export(&instb, "peek", 0); CK(peek >= 0);
        vmb.frame.sp = 0; vmb.frame.num_locals = 0;
        CK(jav_call(&vmb, vmb.heap, (u4)peek) == JAV_OK && jav_tos(&vmb).i == 42);

        jav_vm_free(&vma); jav_instance_free(&insta); jav_modidx_free_bodies(&moda); bbq_arena_free(&aa); free(bufa);
        jav_vm_free(&vmb); jav_instance_free(&instb); jav_modidx_free_bodies(&modb); bbq_arena_free(&ab); free(bufb);
        jav_heap_free_mems(&heap);
    }

    // ── cross-instance call context (§4.2.6): B imports A's getter "getg" (returns A's global
    //    = 111); B has its OWN global = 222 and calls the import. A funcinst carries its
    //    DEFINING instance, so the call must run getg against A — returning 111, not B's 222. ──
    {
        bbq_arena aa; uint8_t* bufa; jav_modidx_t moda; jav_instance_t insta;
        struct heap_t heapa; memset(&heapa, 0, sizeof heapa);
        vm_t vma; memset(&vma, 0, sizeof vma); jav_vm_init(&vma); vma.heap = &heapa;
        jav_status_t sa;
        CK(load_inst("xinst_ga.wasm", &aa, &bufa, &moda, &insta, &vma, NULL, 0, &sa));
        CK(sa == JAV_OK && insta.globals[0]->i == 111);
        int32_t getg = jav_instance_export(&insta, "getg", 0); CK(getg >= 0);

        bbq_arena ab; uint8_t* bufb; jav_modidx_t modb; jav_instance_t instb;
        struct heap_t heapb; memset(&heapb, 0, sizeof heapb);
        vm_t vmb; memset(&vmb, 0, sizeof vmb); jav_vm_init(&vmb); vmb.heap = &heapb;
        jav_extern_t imp; memset(&imp, 0, sizeof imp);
        imp.kind = 0; imp.u.func.func = insta.funcs[getg];          // carries getg's defining instance (A)
        imp.u.func.type = &insta.mod->func_sigs[getg];
        jav_status_t sb;
        CK(load_inst("xinst_gb.wasm", &ab, &bufb, &modb, &instb, &vmb, &imp, 1, &sb));
        CK(sb == JAV_OK && instb.globals[0]->i == 222);

        jav_instance_bind(&vmb, &instb);
        int32_t callg = jav_instance_export(&instb, "callg", 0); CK(callg >= 0);
        vmb.frame.sp = 0; vmb.frame.num_locals = 0;
        CK(jav_call(&vmb, vmb.heap, (u4)callg) == JAV_OK && jav_tos(&vmb).i == 111);  // A's global, not B's 222

        jav_vm_free(&vma); jav_instance_free(&insta); jav_modidx_free_bodies(&moda); jav_heap_free_mems(&heapa); bbq_arena_free(&aa); free(bufa);
        jav_vm_free(&vmb); jav_instance_free(&instb); jav_modidx_free_bodies(&modb); jav_heap_free_mems(&heapb); bbq_arena_free(&ab); free(bufb);
    }

    printf("loader: instantiate + imports/host-call + export-call + global/segment/start + import-subtyping + cross-module-concrete + cross-instance-mem + cross-instance-call  [%s]\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
