/* javelina — the runner: load a compiled Java plugin `.wasm`, link it against the
 * jre.wasm runtime, and run it. The production embedder (the shipped counterpart of
 * the test harness's exec.h): it reaches the VM through the PUBLIC wasm.h only, links
 * the plugin's java.lang imports to jre's exports by name over a shared store/heap,
 * supplies the →HOST native floor (driver/host_io.h) with real stdin/stdout/stderr,
 * and invokes the program.
 *
 *   javelina [options] prog.wasm [args...]     run prog's main(String[]) with args
 *   javelina --call NAME[:i32,...] prog.wasm    call an export directly (dev/test)
 *
 * The program entry is the compiler-synthesized `$main(argc,argv)->i32` wrapper
 * (E7.1a): argv is marshaled as NUL-separated UTF-8 into the shared staging memory,
 * and the returned i32 is the process exit code. */
#include "host_io.h"
#include <stdbool.h>
#include <time.h>

static const char* prog_name = "javelina";

/* The c-api's sanctioned store-scoped readouts + the tier option (not part of wasm.h). */
extern int         jav_capi_last_error(const wasm_store_t* store);
extern const char* jav_err_str(int err);
extern void        jav_capi_set_probe(wasm_store_t*, void (*)(void*, uint8_t), void*);
extern void        jav_config_set_jit(wasm_config_t*, int);
extern uint32_t    jav_capi_jit_count(const wasm_store_t*);

/* Where `make install` puts the runtime. Overridden by the build (-DJAVELINA_JRE_PATH=...). */
#ifndef JAVELINA_JRE_PATH
#define JAVELINA_JRE_PATH "/usr/local/share/javelina/jre.wasm"
#endif

static uint8_t g_last_op;
static void probe_cb(void* c, uint8_t op) { (void)c; g_last_op = op; }

static int usage(FILE* f, int code) {
    fprintf(f,
        "usage: %s [options] prog.wasm [args...]\n"
        "\n"
        "Run a compiled Java plugin module on the javelina VM, linked against jre.wasm.\n"
        "\n"
        "options:\n"
        "  --jre PATH        the runtime module (default: $JAVELINA_JRE, then %s,\n"
        "                    then ./jre.wasm, then build/jre.wasm)\n"
        "  --call NAME[:a,b] call the export NAME directly with i32 args, print its i32\n"
        "                    result — a development hook, bypasses main()\n"
        "  --root DIR        filesystem root guest paths resolve under (default: cwd)\n"
        "  -nojit            run the interpreter tier (default)\n"
        "  -jit              run the copy-and-patch JIT tier\n"
        "  --version         print the version and exit\n"
        "  -h, --help        print this help and exit\n",
        prog_name, JAVELINA_JRE_PATH);
    return code;
}

static uint8_t* read_bin(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    uint8_t* b = (uint8_t*)malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f); if (b) *len = (size_t)n; return b;
}

/* ── The shared runtime: jre.wasm instantiated once on a store/heap that outlives the
 * plugin, with its exports (the plugin link target) and staging memory captured. ── */
static struct {
    wasm_engine_t* engine; wasm_store_t* store;
    wasm_module_t* mod;    wasm_instance_t* inst;
    wasm_byte_vec_t bin;
    wasm_importtype_vec_t impt; wasm_extern_vec_t imp;
    wasm_exporttype_vec_t expt; wasm_extern_vec_t exp;
} jre;

static bool jre_init(const uint8_t* bytes, size_t len, int jit) {
    if (jit) {
        wasm_config_t* cfg = wasm_config_new();
        jav_config_set_jit(cfg, 1);
        jre.engine = wasm_engine_new_with_config(cfg);   /* consumes cfg */
    } else {
        jre.engine = wasm_engine_new();
    }
    jre.store = wasm_store_new(jre.engine);
    /* The per-opcode probe observes the interp tier only; on the JIT tier it never fires, so the
     * `last op` in a trap report is meaningful exactly when it can be. */
    jav_capi_set_probe(jre.store, probe_cb, NULL);
    wasm_byte_vec_new_uninitialized(&jre.bin, len); memcpy(jre.bin.data, bytes, len);
    if (!wasm_module_validate(jre.store, &jre.bin)) {
        fprintf(stderr, "%s: jre.wasm rejected: %s\n", prog_name, jav_err_str(jav_capi_last_error(jre.store))); return false; }
    jre.mod = wasm_module_new(jre.store, &jre.bin); if (!jre.mod) return false;
    wasm_module_imports(jre.mod, &jre.impt);
    wasm_extern_vec_new_uninitialized(&jre.imp, jre.impt.size);
    for (size_t i = 0; i < jre.impt.size; i++) {
        const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(jre.impt.data[i]));
        jre.imp.data[i] = ft ? wasm_func_as_extern(exec_host_for(jre.store, ft, wasm_importtype_name(jre.impt.data[i]))) : NULL;
    }
    wasm_trap_t* trap = NULL;
    jre.inst = wasm_instance_new(jre.store, jre.mod, &jre.imp, &trap);
    if (!jre.inst) { fprintf(stderr, "%s: jre.wasm instantiation failed: %s\n", prog_name, jav_err_str(jav_capi_last_error(jre.store)));
                     if (trap) wasm_trap_delete(trap); return false; }
    wasm_module_exports(jre.mod, &jre.expt); wasm_instance_exports(jre.inst, &jre.exp);
    for (size_t i = 0; i < jre.expt.size && i < jre.exp.size; i++) {   /* the shared I/O staging memory */
        const wasm_name_t* en = wasm_exporttype_name(jre.expt.data[i]);
        if (en->size == 6 && !memcmp(en->data, "memory", 6)) { g_io_mem = wasm_extern_as_memory(jre.exp.data[i]); break; }
    }
    return true;
}

/* Link a plugin against jre + host natives, and hand back its instance + exports.
 * Returns NULL and prints why on failure. Caller frees via plugin_free. */
typedef struct { wasm_module_t* mod; wasm_instance_t* inst;
                 wasm_importtype_vec_t impt; wasm_extern_vec_t imp;
                 wasm_exporttype_vec_t expt; wasm_extern_vec_t exp;
                 wasm_byte_vec_t bin; } plugin_t;

static bool plugin_link(plugin_t* p, const uint8_t* bytes, size_t len) {
    memset(p, 0, sizeof *p);
    wasm_byte_vec_new_uninitialized(&p->bin, len); memcpy(p->bin.data, bytes, len);
    if (!wasm_module_validate(jre.store, &p->bin)) {
        fprintf(stderr, "%s: module rejected: %s\n", prog_name, jav_err_str(jav_capi_last_error(jre.store))); return false; }
    p->mod = wasm_module_new(jre.store, &p->bin); if (!p->mod) return false;
    wasm_module_imports(p->mod, &p->impt);
    wasm_extern_vec_new_uninitialized(&p->imp, p->impt.size);
    for (size_t i = 0; i < p->impt.size; i++) {
        const wasm_name_t* im = wasm_importtype_module(p->impt.data[i]);
        const wasm_name_t* fl = wasm_importtype_name(p->impt.data[i]);
        if (im->size == 3 && !memcmp(im->data, "jre", 3)) {
            int fi = -1;
            for (size_t j = 0; j < jre.expt.size && j < jre.exp.size; j++) {
                const wasm_name_t* en = wasm_exporttype_name(jre.expt.data[j]);
                if (en->size == fl->size && !memcmp(en->data, fl->data, en->size)) { fi = (int)j; break; }
            }
            if (fi < 0) { fprintf(stderr, "%s: unresolved jre import %.*s\n", prog_name, (int)fl->size, fl->data); return false; }
            p->imp.data[i] = jre.exp.data[fi];               /* borrowed — owned by jre.exp */
        } else {
            const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(p->impt.data[i]));
            p->imp.data[i] = ft ? wasm_func_as_extern(exec_host_for(jre.store, ft, fl)) : NULL;
        }
    }
    wasm_trap_t* trap = NULL;
    p->inst = wasm_instance_new(jre.store, p->mod, &p->imp, &trap);
    if (!p->inst) { fprintf(stderr, "%s: instantiation failed: %s", prog_name, jav_err_str(jav_capi_last_error(jre.store)));
                    if (trap) { wasm_message_t tm = WASM_EMPTY_VEC; wasm_trap_message(trap, &tm);
                                fprintf(stderr, " | start trap: %.*s", (int)tm.size, tm.data ? tm.data : "");
                                if (tm.size) wasm_byte_vec_delete(&tm); wasm_trap_delete(trap); }
                    fprintf(stderr, "\n"); return false; }
    wasm_module_exports(p->mod, &p->expt); wasm_instance_exports(p->inst, &p->exp);
    return true;
}

static wasm_func_t* plugin_export(plugin_t* p, const char* name, size_t nlen) {
    for (size_t i = 0; i < p->expt.size && i < p->exp.size; i++) {
        const wasm_name_t* en = wasm_exporttype_name(p->expt.data[i]);
        if (en->size == nlen && !memcmp(en->data, name, nlen)) return wasm_extern_as_func(p->exp.data[i]);
    }
    return NULL;
}

/* System.exit(n): the guest calls the `exit` host native, which terminates the process with
 * the guest's code (the documented →HOST contract). Flush the std streams first — the guest's
 * output may be block-buffered when stdout is a pipe. */
static void runner_exit(int code) { fflush(stdout); fflush(stderr); exit(code); }

/* The real clock (the harness keeps a reproducible counter). Math.random is java.util.Random
 * over this clock (§20.11.20) — the host supplies no PRNG. */
static int64_t runner_clock(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* The property set this runner exposes. §20.18.7 requires values for exactly these fifteen keys.
 * Withholding one is a capability decision: the guest sees it as absent (System.getProperty → null),
 * never as an error. */
static const hio_prop_t runner_props[] = {
    { "java.version",       "1.0" },
    { "java.vendor",        "javelina" },
    { "java.vendor.url",    "https://github.com/javelina" },
    { "java.home",          "/usr/local/share/javelina" },
    { "java.class.version", "45.3" },
    { "java.class.path",    "." },
    { "os.name",            "javelina" },
    { "os.arch",            "wasm32" },
    { "os.version",         "1.0" },
    { "file.separator",     "/" },
    { "path.separator",     ":" },
    { "line.separator",     "\n" },
    { "user.name",          "javelina" },
    { "user.home",          "/" },
    { "user.dir",           "." },
    { NULL, NULL },
};

/* Print a trap (message + trace) to stderr. */
static void report_trap(wasm_trap_t* trap) {
    wasm_message_t msg = WASM_EMPTY_VEC; wasm_trap_message(trap, &msg);
    fprintf(stderr, "%s: uncaught trap: %.*s (last op 0x%02x)", prog_name, (int)msg.size, msg.data ? msg.data : "", g_last_op);
    wasm_frame_vec_t tr = WASM_EMPTY_VEC; wasm_trap_trace(trap, &tr);
    for (size_t k = 0; k < tr.size; k++)
        fprintf(stderr, " | #%zu func=%u pc=%zu", k, wasm_frame_func_index(tr.data[k]), wasm_frame_func_offset(tr.data[k]));
    if (tr.size) wasm_frame_vec_delete(&tr);
    fprintf(stderr, "\n");
    if (msg.size) wasm_byte_vec_delete(&msg);
}

int main(int argc, char** argv) {
    const char* jre_path  = NULL;
    const char* call_spec = NULL;
    const char* root      = ".";
    const char* prog_path = NULL;
    int prog_argc = 0; char** prog_argv = NULL;
    int want_jit = 0;

    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if      (!strcmp(a, "-h") || !strcmp(a, "--help")) return usage(stdout, 0);
        else if (!strcmp(a, "--version")) { printf("javelina %s\n", "0.1.0"); return 0; }
        else if (!strcmp(a, "--jre"))  { if (++i >= argc) return usage(stderr, 2); jre_path = argv[i]; }
        else if (!strcmp(a, "--call")) { if (++i >= argc) return usage(stderr, 2); call_spec = argv[i]; }
        else if (!strcmp(a, "--root")) { if (++i >= argc) return usage(stderr, 2); root = argv[i]; }
        else if (!strcmp(a, "-jit")  || !strcmp(a, "--jit"))   want_jit = 1;
        else if (!strcmp(a, "-nojit")|| !strcmp(a, "--nojit")) want_jit = 0;
        else if (a[0] == '-' && a[1]) { fprintf(stderr, "%s: unknown option '%s'\n", prog_name, a); return usage(stderr, 2); }
        else { prog_path = a; prog_argv = &argv[i + 1]; prog_argc = argc - i - 1; break; }
    }
    if (!prog_path) { fprintf(stderr, "%s: no program\n", prog_name); return usage(stderr, 2); }

    /* Locate jre.wasm: --jre, else $JAVELINA_JRE, else the installed copy, else an in-tree build. */
    size_t jlen = 0; uint8_t* jbytes = NULL;
    const char* tried[4]; int nt = 0;
    const char* env_jre = getenv("JAVELINA_JRE");
    if (jre_path)      { jbytes = read_bin(jre_path, &jlen); tried[nt++] = jre_path; }
    else if (env_jre)  { jbytes = read_bin(env_jre,  &jlen); tried[nt++] = env_jre;  }
    else {
        jbytes = read_bin(JAVELINA_JRE_PATH, &jlen); tried[nt++] = JAVELINA_JRE_PATH;
        if (!jbytes) { jbytes = read_bin("jre.wasm", &jlen);       tried[nt++] = "jre.wasm"; }
        if (!jbytes) { jbytes = read_bin("build/jre.wasm", &jlen); tried[nt++] = "build/jre.wasm"; }
    }
    if (!jbytes) {
        fprintf(stderr, "%s: cannot find jre.wasm (tried:", prog_name);
        for (int t = 0; t < nt; t++) fprintf(stderr, " %s", tried[t]);
        fprintf(stderr, ") — build it with 'make -C compiler jre' or pass --jre PATH\n");
        return 2;
    }

    size_t plen = 0; uint8_t* pbytes = read_bin(prog_path, &plen);
    if (!pbytes) { fprintf(stderr, "%s: cannot read '%s'\n", prog_name, prog_path); free(jbytes); return 2; }

    /* Real environment: fds 0/1/2 = stdin/stdout/stderr; guest paths resolve under root; a real
     * clock and a clock-seeded PRNG; System.exit terminates the process with the guest's code. */
    g_io_root = root;
    g_io_fds[0] = stdin; g_io_fds[1] = stdout; g_io_fds[2] = stderr;
    g_io_exit   = runner_exit;
    g_io_clock  = runner_clock;
    g_io_props  = runner_props;

    int rc = 1;
    if (!jre_init(jbytes, jlen, want_jit)) goto out;

    plugin_t p;
    if (!plugin_link(&p, pbytes, plen)) goto out;

    if (call_spec) {
        /* Dev hook: --call NAME[:i32,i32,...] — call the export, print its i32 result. */
        char name[256]; wasm_val_t args[8]; int na = 0;
        const char* colon = strchr(call_spec, ':');
        size_t nlen = colon ? (size_t)(colon - call_spec) : strlen(call_spec);
        if (nlen >= sizeof name) nlen = sizeof name - 1;
        memcpy(name, call_spec, nlen); name[nlen] = 0;
        if (colon) { const char* s = colon + 1;
            while (*s && na < 8) { args[na++] = (wasm_val_t)WASM_I32_VAL((int32_t)strtol(s, NULL, 10));
                                   const char* c = strchr(s, ','); if (!c) break; s = c + 1; } }
        wasm_func_t* fn = plugin_export(&p, name, nlen);
        if (!fn) { fprintf(stderr, "%s: no export '%s'\n", prog_name, name); goto out; }
        wasm_val_t res[1] = { WASM_INIT_VAL };
        wasm_val_vec_t av = { (size_t)na, args }, rv = { 1, res };
        wasm_trap_t* trap = wasm_func_call(fn, &av, &rv);
        if (trap) { report_trap(trap); wasm_trap_delete(trap); goto out; }
        printf("%d\n", res[0].of.i32);
        rc = 0;
        goto out;
    }

    /* Standard entry: the synthesized $main(argc, argv) -> i32. Marshal argv as
     * NUL-separated UTF-8 into the staging memory at a fixed offset above the bounce
     * region; pass (argc, offset). The i32 result is the process exit code. */
    wasm_func_t* mainfn = plugin_export(&p, "$main", 5);
    if (!mainfn) {
        fprintf(stderr, "%s: '%s' has no main(String[]) entry (no $main export). "
                        "Use --call NAME to invoke an export directly.\n", prog_name, prog_path);
        goto out;
    }
    const int ARGV_OFF = 4096;                       /* argv staging base (above the I/O bounce region) */
    byte_t* mem = io_membytes();
    int w = ARGV_OFF;
    if (mem) for (int k = 0; k < prog_argc; k++) { const char* s = prog_argv[k];
                 while (*s) mem[w++] = (byte_t)*s++; mem[w++] = 0; }
    wasm_val_t margs[2] = { WASM_I32_VAL(prog_argc), WASM_I32_VAL(ARGV_OFF) };
    wasm_val_t mres[1]  = { WASM_INIT_VAL };
    wasm_val_vec_t av = { 2, margs }, rv = { 1, mres };
    wasm_trap_t* trap = wasm_func_call(mainfn, &av, &rv);
    fflush(stdout);
    if (trap) { report_trap(trap); wasm_trap_delete(trap); rc = 1; goto out; }
    rc = mres[0].of.i32;

out:
    free(jbytes); free(pbytes);
    return rc;
}
