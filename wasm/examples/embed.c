// embed.c — an embedder, written the way an application embeds this engine: link ONLY the public
// W3C header (wasm.h) plus the javelina library, with no view into the jav_* core.
//
// It does the four things an embedding actually consists of, and nothing else:
//
//   §7.1.6 module_decode + module_validate   wasm_module_new (the two fused; one decode, one
//                                            validate — validating the bytes separately first
//                                            would decode them twice for nothing)
//   §7.1.6 module_imports                    what the module demands of its host
//   §7.1.8 func_alloc                        satisfy those demands with host functions, each
//                                            carrying the embedder's own state as its env
//   §7.1.6 module_instantiate                link and run
//   §7.1.7 instance_export                   reach an export BY NAME, rather than by zipping
//                                            wasm_module_exports against wasm_instance_exports
//                                            and trusting an ordering the header never states
//
// Usage: embed <module.wasm>
// The module must import "env" "add1" : (i32)->i32 and export "run" : (i32)->i32; the harness
// assembles one that calls add1 twice, so run(40) is 42 iff the host function was really reached.
#include "wasm.h"
#include <stdio.h>
#include <stdlib.h>

// The embedder's state. It reaches the host function through the §7.1.8 hostfunc env rather than
// through a global, which is what lets one process hold more than one of these at a time.
typedef struct { int calls; int32_t bias; } app_t;

static wasm_trap_t* app_add1(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    app_t* app = (app_t*)env;
    app->calls++;
    if (results->size > 0)
        results->data[0] = (wasm_val_t)WASM_I32_VAL(args->data[0].of.i32 + app->bias);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <module.wasm>\n", argv[0]); return 2; }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    wasm_byte_vec_t bin; wasm_byte_vec_new_uninitialized(&bin, (size_t)n);
    if (fread(bin.data, 1, (size_t)n, f) != (size_t)n) { perror("fread"); return 2; }
    fclose(f);

    wasm_engine_t* engine = wasm_engine_new();          // the default tier, which is what an
    wasm_store_t*  store  = wasm_store_new(engine);     // embedder gets for asking nothing
    app_t app = { 0, 1 };
    int rc = 1;

    wasm_module_t* module = wasm_module_new(store, &bin);
    if (!module) { fprintf(stderr, "module rejected\n"); goto out_bin; }

    // Satisfy each import in the order the module declares them: module_instantiate takes the
    // externs POSITIONALLY, so this vector is built by walking module_imports.
    wasm_importtype_vec_t imptypes; wasm_module_imports(module, &imptypes);
    wasm_extern_vec_t imports; wasm_extern_vec_new_uninitialized(&imports, imptypes.size);
    for (size_t i = 0; i < imptypes.size; i++) {
        const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(imptypes.data[i]));
        if (!ft) { fprintf(stderr, "unsupported import kind\n"); goto out_imports; }
        // §7.1.8 func_alloc, with the embedder's state as the hostfunc closure.
        imports.data[i] = wasm_func_as_extern(
            wasm_func_new_with_env(store, ft, app_add1, &app, NULL));
    }

    wasm_trap_t* trap = NULL;
    wasm_instance_t* instance = wasm_instance_new(store, module, &imports, &trap);
    if (!instance) {
        fprintf(stderr, "instantiation failed\n");
        if (trap) wasm_trap_delete(trap);
        goto out_imports;
    }

    // §7.1.7 instance_export: by name, not by index.
    wasm_name_t want; wasm_name_new_from_string(&want, "run");
    wasm_extern_t* ex = wasm_instance_export(instance, &want);
    wasm_name_delete(&want);
    if (!ex) { fprintf(stderr, "no export named 'run'\n"); goto out_instance; }
    wasm_func_t* run = wasm_extern_as_func(ex);
    if (!run) { fprintf(stderr, "'run' is not a function\n"); wasm_extern_delete(ex); goto out_instance; }

    wasm_val_t args[1] = { WASM_I32_VAL(40) }, results[1] = { WASM_INIT_VAL };
    wasm_val_vec_t av = WASM_ARRAY_VEC(args), rv = WASM_ARRAY_VEC(results);
    trap = wasm_func_call(run, &av, &rv);
    if (trap) { fprintf(stderr, "call trapped\n"); wasm_trap_delete(trap); wasm_extern_delete(ex); goto out_instance; }

    int32_t r = results[0].of.i32;
    printf("run(40) = %d after %d host call(s)\n", r, app.calls);
    rc = (r == 42 && app.calls == 2) ? 0 : 1;
    wasm_extern_delete(ex);

out_instance:
    wasm_instance_delete(instance);
out_imports:
    for (size_t i = 0; i < imports.size; i++)
        if (imports.data[i]) wasm_func_delete(wasm_extern_as_func(imports.data[i]));
    free(imports.data);
    wasm_importtype_vec_delete(&imptypes);
    wasm_module_delete(module);
out_bin:
    wasm_store_delete(store);
    wasm_engine_delete(engine);
    wasm_byte_vec_delete(&bin);
    return rc;
}
