# hello-java — running Java, and embedding the runtime in your own C app

This is the "scripting without V8" path: compile a `.java` file to WebAssembly,
then run it — either with the shipped `javelina` CLI, or from inside your own C
program through the standard `wasm.h` C API.

## Run it with the CLIs

Two commands, from `compiler/`:

```sh
build/javelinac --mode plugin examples/hello-java/Hello.java -o Hello.wasm
build/javelina --jre build/jre.wasm Hello.wasm
```

```
Hello from javelina!
sum(1..10) = 55
```

`javelinac` compiles `Hello.java` into a **plugin** module — a `.wasm` that
imports `java.lang.*` from the runtime rather than bundling it. `jre.wasm` is
that runtime (the Java standard library, itself compiled to wasm). `javelina`
loads both, links them, and runs `main`.

## Embed it in your own C application

You do not need the `javelina` binary — the same thing is ~30 lines of `wasm.h`.
An embedder instantiates the runtime once, then links each plugin against it.
The full, production version of this is `compiler/driver/javelina.c`; the shape:

**1. An engine and a store.**

```c
wasm_engine_t* engine = wasm_engine_new();
wasm_store_t*  store  = wasm_store_new(engine);
```

**2. Instantiate the runtime (`jre.wasm`).** It imports the host edges —
`System.out`, `System.exit`, `currentTimeMillis`, and so on — so you supply a
host function for each of its imports. What those edges are, and their
signatures, is the host ABI in [`docs/host-abi.md`](../../../docs/host-abi.md).

```c
wasm_module_t* jre = wasm_module_new(store, &jre_bytes);
wasm_importtype_vec_t want; wasm_module_imports(jre, &want);
wasm_extern_vec_t give; wasm_extern_vec_new_uninitialized(&give, want.size);
for (size_t i = 0; i < want.size; i++)
    give.data[i] = wasm_func_as_extern(your_host_func(store, want.data[i]));
wasm_instance_t* jre_inst = wasm_instance_new(store, jre, &give, NULL);

// Capture the runtime's exports — they are the plugin's link target.
wasm_exporttype_vec_t jre_expt; wasm_module_exports(jre, &jre_expt);
wasm_extern_vec_t     jre_exp;  wasm_instance_exports(jre_inst, &jre_exp);
```

**3. Link a plugin against it.** A compiled program imports from module `"jre"`.
For each such import, find the runtime export of the same name and pass it
through; anything else is a host edge you supply as in step 2.

```c
wasm_module_t* plugin = wasm_module_new(store, &plugin_bytes);
wasm_importtype_vec_t pimp; wasm_module_imports(plugin, &pimp);
wasm_extern_vec_t pgive; wasm_extern_vec_new_uninitialized(&pgive, pimp.size);
for (size_t i = 0; i < pimp.size; i++) {
    const wasm_name_t* mod  = wasm_importtype_module(pimp.data[i]);
    const wasm_name_t* name = wasm_importtype_name(pimp.data[i]);
    if (name_eq(mod, "jre"))
        pgive.data[i] = jre_export_named(&jre_expt, &jre_exp, name);  // borrowed
    else
        pgive.data[i] = wasm_func_as_extern(your_host_func(store, pimp.data[i]));
}
wasm_instance_t* plugin_inst = wasm_instance_new(store, plugin, &pgive, NULL);
```

The runtime and every plugin share **one store** — that is what lets the
plugin's objects and the runtime's live on the same heap.

**4. Call the entry point.** `javelinac` synthesizes a
`$main(argc, argv) -> i32` export. Marshal `argv` as NUL-separated UTF-8 into
the runtime's exported memory and call it; the `i32` result is the exit code.

```c
wasm_func_t* main_fn = plugin_export_named(plugin_inst, "$main");
wasm_val_t   args[2] = { WASM_I32_VAL(argc), WASM_I32_VAL(argv_offset) };
wasm_val_t   res[1]  = { WASM_INIT_VAL };
wasm_val_vec_t av = { 2, args }, rv = { 1, res };
wasm_trap_t* trap = wasm_func_call(main_fn, &av, &rv);
int exit_code = trap ? 1 : res[0].of.i32;
```

That is the whole embedding surface: `wasm.h`, `libjavelina.a` (see
[`../../../README.md`](../../../README.md#embedding)), a `jre.wasm`, and the
plugins you compile. Link the runtime once, run as many plugins as you like.
