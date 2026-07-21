// embed.c — the Phase-4 gate: a standalone embedder that links ONLY the public
// W3C header (wasm.h) + the javelina library, with no view into the jav_* core.
// It decodes + validates a module, instantiates it (no imports), finds the first
// export, and calls it — proving the wasm-c-api surface is real end to end.
//
// Usage: embed <module.wasm>   (the module must export an (i32,i32)->i32 as its
// first export; the harness assembles one that computes add and expects add(3,5)=8.)
#include "wasm.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <module.wasm>\n", argv[0]); return 2; }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    wasm_byte_vec_t bin; wasm_byte_vec_new_uninitialized(&bin, (size_t)n);
    if (fread(bin.data, 1, (size_t)n, f) != (size_t)n) { perror("fread"); return 2; }
    fclose(f);

    wasm_engine_t* engine = wasm_engine_new();
    wasm_store_t* store = wasm_store_new(engine);

    if (!wasm_module_validate(store, &bin)) { fprintf(stderr, "module did not validate\n"); return 1; }
    wasm_module_t* module = wasm_module_new(store, &bin);
    if (!module) { fprintf(stderr, "wasm_module_new failed\n"); return 1; }

    wasm_trap_t* trap = NULL;
    wasm_extern_vec_t imports = WASM_EMPTY_VEC;
    wasm_instance_t* instance = wasm_instance_new(store, module, &imports, &trap);
    if (!instance) { fprintf(stderr, "instantiation failed\n"); return 1; }

    wasm_extern_vec_t exports; wasm_instance_exports(instance, &exports);
    if (exports.size < 1) { fprintf(stderr, "no exports\n"); return 1; }
    wasm_func_t* func = wasm_extern_as_func(exports.data[0]);
    if (!func) { fprintf(stderr, "first export is not a function\n"); return 1; }

    wasm_val_t args[2] = { WASM_I32_VAL(3), WASM_I32_VAL(5) };
    wasm_val_t results[1] = { WASM_INIT_VAL };
    wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
    wasm_val_vec_t results_vec = WASM_ARRAY_VEC(results);
    trap = wasm_func_call(func, &args_vec, &results_vec);
    if (trap) { fprintf(stderr, "call trapped\n"); wasm_trap_delete(trap); return 1; }

    int32_t r = results[0].of.i32;
    printf("add(3, 5) = %d\n", r);

    wasm_extern_vec_delete(&exports);
    wasm_instance_delete(instance);
    wasm_module_delete(module);
    wasm_store_delete(store);
    wasm_engine_delete(engine);
    wasm_byte_vec_delete(&bin);

    return r == 8 ? 0 : 1;
}
