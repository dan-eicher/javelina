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
#include "version.h"
#include "jav_eqsat.h"   /* tier-3's counters, for --jit-stats at --tier 3 */
#include <stdbool.h>
#include <time.h>

static const char* prog_name = "javelina";

/* The c-api's sanctioned store-scoped readouts + the tier option (not part of wasm.h). */
extern int         jav_capi_last_error(const wasm_store_t* store);
extern int         jav_capi_last_status(const wasm_store_t* store);
extern const char* jav_err_str(int err);
extern void        jav_capi_set_probe(wasm_store_t*, void (*)(void*, uint8_t), void*);
extern void        jav_config_set_jit(wasm_config_t*, int);
extern void        jav_config_set_verify_heap(wasm_config_t*, int);
extern uint32_t    jav_capi_jit_count(const wasm_store_t*);
extern uint32_t    jav_capi_jit_declined(const wasm_store_t*);
extern int         jav_jit_cache_slots(void);
extern void        jav_jit_cache_stats(uint64_t*, uint64_t*, uint64_t*, uint64_t*,
                                        uint64_t*, uint64_t*, uint64_t*);
extern void        jav_jit_cache_stats_reset(void);

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
        "  --tier N          how guest code EXECUTES; the answer is identical at every\n"
        "                    level, traps included, so this is a speed/compile-time knob\n"
        "                      0  interpret\n"
        "                      1  copy-and-patch JIT\n"
        "                      2  copy-and-patch JIT with operand-stack caching (default)\n"
        "                      3  tier 2 with the eq-sat rewrite in front\n"
        "  --jit-stats       after the run, report to stderr what the JIT compiled and\n"
        "                    what the operand cache did (functions, cached uses,\n"
        "                    uses above the first slot, spills and fills)\n"
        "  --verify-heap     check the collector's invariants after every collection and\n"
        "                    abort naming the one that failed (slow; walks the live graph)\n"
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

/* The runner's embedder context — the →HOST floor's state, reaching every native as the §7.1.8
 * hostfunc env. */
static jav_host_t host;

static bool jre_init(const uint8_t* bytes, size_t len, int tier, int verify_heap) {
    if (tier || verify_heap) {
        wasm_config_t* cfg = wasm_config_new();
        if (tier) jav_config_set_jit(cfg, tier);
        if (verify_heap) jav_config_set_verify_heap(cfg, 1);
        jre.engine = wasm_engine_new_with_config(cfg);   /* consumes cfg */
    } else {
        jre.engine = wasm_engine_new();
    }
    jre.store = wasm_store_new(jre.engine);
    /* The per-opcode probe observes the interp tier only; on the JIT tier it never fires, so the
     * `last op` in a trap report is meaningful exactly when it can be. */
    jav_capi_set_probe(jre.store, probe_cb, NULL);
    wasm_byte_vec_new_uninitialized(&jre.bin, len); memcpy(jre.bin.data, bytes, len);
    /* wasm_module_new validates — a caller is allowed to call it alone, so the c-api makes
     * it decode and check and return NULL on an invalid module, recording the reason on the
     * store. Asking wasm_module_validate first therefore ran the whole §5/§7 pass, threw the
     * verdict away, and had wasm_module_new run it again over the same bytes. The reason it
     * printed is the one already on the store at the NULL. */
    jre.mod = wasm_module_new(jre.store, &jre.bin);
    if (!jre.mod) {
        fprintf(stderr, "%s: jre.wasm rejected: %s\n", prog_name, jav_err_str(jav_capi_last_error(jre.store)));
        return false;
    }
    wasm_module_imports(jre.mod, &jre.impt);
    wasm_extern_vec_new_uninitialized(&jre.imp, jre.impt.size);
    for (size_t i = 0; i < jre.impt.size; i++) {
        const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(jre.impt.data[i]));
        jre.imp.data[i] = ft ? wasm_func_as_extern(exec_host_for(&host, jre.store, ft,
                                   wasm_importtype_module(jre.impt.data[i]),
                                   wasm_importtype_name(jre.impt.data[i]))) : NULL;
    }
    wasm_trap_t* trap = NULL;
    jre.inst = wasm_instance_new(jre.store, jre.mod, &jre.imp, &trap);
    if (!jre.inst) {
        /* §4.5.4: a module that LINKS can still fail to instantiate — a segment out of bounds,
         * or the start function trapping. The status says which stage; the trap says why, and
         * discarding it left the reason blank. */
        fprintf(stderr, "%s: jre.wasm instantiation failed (status %d): %s\n", prog_name,
                (int)jav_capi_last_status(jre.store), jav_err_str(jav_capi_last_error(jre.store)));
        if (trap) {
            wasm_message_t msg = {0};
            wasm_trap_message(trap, &msg);
            fprintf(stderr, "%s:   start-function trap: %.*s\n", prog_name,
                    (int)msg.size, msg.data ? msg.data : "");
            wasm_byte_vec_delete(&msg);
            /* §7.1.8 the frame trace, innermost first: each frame's funcidx and the byte offset
             * of the instruction it stopped at. A start-function trap has no other locator. */
            wasm_frame_vec_t fr = {0};
            wasm_trap_trace(trap, &fr);
            for (size_t i = 0; i < fr.size; i++)
                fprintf(stderr, "%s:     at func %u +0x%x (module 0x%zx)\n", prog_name,
                        wasm_frame_func_index(fr.data[i]), (unsigned)wasm_frame_func_offset(fr.data[i]),
                        wasm_frame_module_offset(fr.data[i]));
            wasm_frame_vec_delete(&fr);
            wasm_trap_delete(trap);
        }
        return false;
    }
    wasm_module_exports(jre.mod, &jre.expt); wasm_instance_exports(jre.inst, &jre.exp);
    /* The shared I/O staging memory. Borrowed out of jre.exp rather than taken through
     * wasm_instance_export because wasm_extern_as_memory aliases the handle's storage, so the
     * memory pointer is valid only while some extern denoting it is alive — and jre.exp is. */
    for (size_t i = 0; i < jre.expt.size && i < jre.exp.size; i++) {
        const wasm_name_t* en = wasm_exporttype_name(jre.expt.data[i]);
        if (en->size == 6 && !memcmp(en->data, "memory", 6)) { host.mem = wasm_extern_as_memory(jre.exp.data[i]); break; }
    }
    return true;
}

/* Release the runtime. Safe on a partly-built jre, since jre_init can fail at any
 * step and `out:` runs regardless.
 *
 * This exists for two reasons that have nothing to do with tidiness at exit.
 * FIRST, javelina is the reference embedder for libjavelina.a, and an embedder
 * that creates and destroys stores runs this path — code the driver had never
 * executed once, because it relied on the process ending. SECOND, a heap that is
 * never freed cannot be checked: a use-after-free is undetectable when nothing is
 * ever released, so it waits for the first caller who does release. Freeing here
 * is what makes valgrind's verdict over javelina mean something.
 *
 * `jre.imp` is all host natives from exec_host_for, all ours — unlike a plugin's,
 * which mixes borrowed jre exports in (see plugin_free). Order: the exports go
 * before the instance that owns them, the instance before its module, and both
 * before the store. */
static void jre_teardown(void) {
    host.mem = NULL;                     /* borrowed from jre.exp, about to go */
    for (size_t i = 0; i < jre.imp.size; i++)
        if (jre.imp.data[i]) wasm_func_delete(wasm_extern_as_func(jre.imp.data[i]));
    free(jre.imp.data); jre.imp.data = NULL; jre.imp.size = 0;
    if (jre.impt.size) wasm_importtype_vec_delete(&jre.impt);
    if (jre.exp.size)  wasm_extern_vec_delete(&jre.exp);
    if (jre.expt.size) wasm_exporttype_vec_delete(&jre.expt);
    if (jre.inst) { wasm_instance_delete(jre.inst); jre.inst = NULL; }
    if (jre.mod)  { wasm_module_delete(jre.mod);    jre.mod  = NULL; }
    wasm_byte_vec_delete(&jre.bin);
    if (jre.store)  { wasm_store_delete(jre.store);   jre.store  = NULL; }
    if (jre.engine) { wasm_engine_delete(jre.engine); jre.engine = NULL; }
}

/* Link a plugin against jre + host natives, and hand back its instance + exports.
 * Returns false and prints why on failure. Caller frees via plugin_free. */
typedef struct { wasm_module_t* mod; wasm_instance_t* inst;
                 wasm_importtype_vec_t impt; wasm_extern_vec_t imp;
                 wasm_exporttype_vec_t expt; wasm_extern_vec_t exp;
                 wasm_byte_vec_t bin; } plugin_t;

static bool plugin_link(plugin_t* p, const uint8_t* bytes, size_t len) {
    memset(p, 0, sizeof *p);
    wasm_byte_vec_new_uninitialized(&p->bin, len); memcpy(p->bin.data, bytes, len);
    p->mod = wasm_module_new(jre.store, &p->bin);          /* validates; see jre_init */
    if (!p->mod) {
        fprintf(stderr, "%s: module rejected: %s\n", prog_name, jav_err_str(jav_capi_last_error(jre.store)));
        return false;
    }
    wasm_module_imports(p->mod, &p->impt);
    wasm_extern_vec_new_uninitialized(&p->imp, p->impt.size);
    for (size_t i = 0; i < p->impt.size; i++) {
        const wasm_name_t* im = wasm_importtype_module(p->impt.data[i]);
        const wasm_name_t* fl = wasm_importtype_name(p->impt.data[i]);
        if (im->size == 3 && !memcmp(im->data, "jre", 3)) {
            /* §7.1.7 instance_export. Owned, and released in plugin_free once instantiation has
             * copied what it needs. */
            p->imp.data[i] = wasm_instance_export(jre.inst, fl);
            if (!p->imp.data[i]) { fprintf(stderr, "%s: unresolved jre import %.*s\n", prog_name, (int)fl->size, fl->data); return false; }
        } else {
            const wasm_functype_t* ft = wasm_externtype_as_functype_const(wasm_importtype_type(p->impt.data[i]));
            p->imp.data[i] = ft ? wasm_func_as_extern(exec_host_for(&host, jre.store, ft, im, fl)) : NULL;
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

/* Release everything plugin_link allocated. Safe on a zero-initialized plugin_t,
 * because `goto out` can be taken before the plugin is linked at all.
 *
 * `p->imp` does not go through wasm_extern_vec_delete. Every element is ours, but the
 * two halves free differently: a host func from exec_host_for carries a §7.1.8 env
 * finalizer that only wasm_func_delete runs, while a jre extern from §7.1.7
 * instance_export is released with wasm_extern_delete.
 *
 * This function was easier to promise than to write: the comment above plugin_link
 * claimed a plugin_free for a long time while valgrind reported five definite losses
 * here — the module bytes, both type vectors and both extern vectors, 52 KB for a
 * hello-world. */
static void plugin_free(plugin_t* p) {
    for (size_t i = 0; i < p->impt.size && i < p->imp.size; i++) {
        const wasm_name_t* im = wasm_importtype_module(p->impt.data[i]);
        if (!p->imp.data[i]) continue;
        if (im->size == 3 && !memcmp(im->data, "jre", 3)) wasm_extern_delete(p->imp.data[i]);
        else                                              wasm_func_delete(wasm_extern_as_func(p->imp.data[i]));
    }
    free(p->imp.data); p->imp.data = NULL; p->imp.size = 0;
    wasm_importtype_vec_delete(&p->impt);
    /* The instance's exports ARE ours (wasm_instance_exports hands over owned
     * externs), so this one deletes elements and array together. It invalidates
     * anything plugin_export handed out, which is why it runs at `out:` and not
     * before the entry point returns. */
    wasm_extern_vec_delete(&p->exp);
    wasm_exporttype_vec_delete(&p->expt);
    if (p->inst) { wasm_instance_delete(p->inst); p->inst = NULL; }
    if (p->mod)  { wasm_module_delete(p->mod);    p->mod  = NULL; }
    wasm_byte_vec_delete(&p->bin);
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
    int want_tier = 2, want_verify = 0, want_stats = 0;   /* tier 2 — the engine's default (JAV_DEFAULT_TIER) */

    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if      (!strcmp(a, "-h") || !strcmp(a, "--help")) return usage(stdout, 0);
        else if (!strcmp(a, "--version")) { printf("javelina %s\n", JAVELINA_VERSION); return 0; }
        else if (!strcmp(a, "--jre"))  { if (++i >= argc) return usage(stderr, 2); jre_path = argv[i]; }
        else if (!strcmp(a, "--call")) { if (++i >= argc) return usage(stderr, 2); call_spec = argv[i]; }
        else if (!strcmp(a, "--root")) { if (++i >= argc) return usage(stderr, 2); root = argv[i]; }
        /* The execution TIER: each level keeps the semantics and spends more
         * compile time for faster code. Not `-O`, which belongs to javelinac and
         * means something else — the optimizer runs over the program and changes
         * what code exists, while this only changes how the same code is run. */
        else if (!strcmp(a, "--tier")) {
            if (++i >= argc) return usage(stderr, 2);
            char* end = NULL; long t = strtol(argv[i], &end, 10);
            if (!end || *end || t < 0 || t > 3) {
                fprintf(stderr, "%s: --tier wants 0, 1, 2 or 3\n", prog_name);
                return usage(stderr, 2);
            }
            want_tier = (int)t;
        }
        else if (!strcmp(a, "--jit-stats")) want_stats = 1;
        else if (!strcmp(a, "--verify-heap")) want_verify = 1;
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
    host.root = root;
    host.fds[0] = stdin; host.fds[1] = stdout; host.fds[2] = stderr;
    host.exit_fn = runner_exit;
    host.clock   = runner_clock;
    host.props   = runner_props;

    int rc = 1;
    if (!jre_init(jbytes, jlen, want_tier, want_verify)) goto out;

    /* Zeroed at declaration, not by plugin_link: `goto out` above this line is
     * reachable (a failed jre_init), and plugin_free runs at the label. */
    plugin_t p = {0};
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
    byte_t* mem = io_membytes(&host);
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
    /* What the tier actually did, over REAL COMPILED CODE. The conformance corpus
     * answers "is it correct"; it cannot answer "is a cache of this size worth
     * having", because its instruction mix is a coverage policy rather than a
     * workload. These counters over a compiled program are the population that
     * question needs, and the ratio discriminates where a wall-clock benchmark
     * does not — between-process variance swamps the deltas, the ratio does not. */
    if (want_stats && jre.store) {
        uint64_t cached = 0, deep = 0, occ = 0, trans = 0, spill = 0, fill = 0, mem = 0;
        jav_jit_cache_stats(&cached, &deep, &occ, &trans, &spill, &fill, &mem);
        /* `mem` is the measured quantity: operand-stack slots the compiled code
         * still moves between a register and memory. It is what stack caching
         * removes, so it is the figure to compare cache sizes on, and it is
         * reported rather than derived.
         *
         * What was printed here was `occupancy * 2 - transitions`, called "net
         * stack accesses avoided". It was wrong twice. Occupancy sums each
         * instruction's ENTRY STATE, so a value idle in a register for five
         * instructions counted five while being read at most once — it is not a
         * count of anything avoided. And a transition relocates the access it
         * performs (the GPUSH inside a spill is the one the all-memory form does
         * inline), so subtracting it charged the same access a second time.
         *
         * STATIC, and the heading says so: these count instructions COMPILED, not
         * executed, because the JIT compiles every defined function eagerly at
         * instantiation. An op in a hot loop counts once and so does one in dead
         * code. That makes this a measure of what a MODULE contains, useful for
         * comparing cache sizes over the same code, and useless for comparing
         * workloads — two programs linking the same runtime report identical
         * figures by construction, which is what four attempts at a SIMD
         * comparison kept discovering.
         *
         * Ertl measured the other way (§2.6): he instrumented a RUNNING Forth and
         * reported executed instructions in the millions with per-instruction
         * load rates. The dynamic equivalent here needs the §3.3.3 probe to carry
         * an IP so per-site execution counts can weight the per-site decisions
         * the tile map already holds by byte offset. */
        fprintf(stderr,
                "javelina: --tier %d, cache %d slot(s)\n"
                "  jit:   %u function(s) compiled, %u declined\n"
                "  cache: %llu instruction(s) ran with an operand in a register, "
                "%llu of them above slot 0\n"
                "         %llu register-slot(s) held, summed over instructions\n"
                "  cost:  %llu transition(s) stamped (%llu spill, %llu fill)\n"
                "  traffic: %llu operand-stack slot(s) still moved to or from memory\n",
                want_tier, jav_jit_cache_slots(),
                jav_capi_jit_count(jre.store), jav_capi_jit_declined(jre.store),
                (unsigned long long)cached, (unsigned long long)deep,
                (unsigned long long)occ,
                (unsigned long long)trans, (unsigned long long)spill,
                (unsigned long long)fill, (unsigned long long)mem);
        /* The tier-3 rewrite's own accounting, same static caveat: what the
         * MODULES contained for it, not what execution touched. */
        if (want_tier >= 3) {
            const jav_eqsat_stats_t* es = jav_eqsat_stats();
            fprintf(stderr,
                "  eqsat: %llu bodies, %llu regions, %llu roots; "
                "%llu rewritten, %llu rebuild-refused, %llu cap-refused, "
                "%llu identity failure(s), %llu e-node peak\n",
                (unsigned long long)es->bodies, (unsigned long long)es->regions,
                (unsigned long long)es->roots, (unsigned long long)es->rewritten,
                (unsigned long long)es->rebuild_refusals,
                (unsigned long long)es->cap_refusals,
                (unsigned long long)es->identity_fails,
                (unsigned long long)es->enodes_peak);
            const char* const* rn; const unsigned long long* rf;
            int nr = jav_eqsat_rule_stats(&rn, &rf);
            fprintf(stderr, "  rules:");
            int any = 0;
            for (int k = 0; k < nr; k++)
                if (rf[k]) { fprintf(stderr, " %s=%llu", rn[k], rf[k]); any = 1; }
            fprintf(stderr, "%s\n", any ? "" : " (none fired)");
        }
    }
    /* The lookup indexes are the driver's, not the engine's, so they are released
     * here rather than left for exit: a leak checker over javelinac/javelina
     * should have nothing of ours to report. */
    plugin_free(&p);            /* borrows jre exports, so it goes first */
    jre_teardown();
    exec_host_release();
    free(jbytes); free(pbytes);
    return rc;
}
