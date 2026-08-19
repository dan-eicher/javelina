/* test_host_memory.c — the →HOST contract's STAGING-MEMORY rule (docs/host-abi.md).
 *
 * Every value that is not a wasm primitive crosses the GC↔host boundary as an
 * (offset, length) span into the module's staging linear memory. The host reaches
 * that memory through a raw `wasm_memory_data()` pointer, so a span is a pointer
 * the GUEST chose and the floor is the only thing standing between it and the
 * process. Two rules govern it, and this suite is where they are pinned:
 *
 *   SPAN REFUSAL. A span that does not lie wholly inside the memory is refused —
 *   never clamped, never partially serviced. `io_span_ok` is the check, and every
 *   native that touches the memory must run it.
 *
 *   THE THREE-WAY RESULT. A native that WRITES a variable-length answer reports the
 *   length its answer needs, and writes only when the whole answer fits. `-1` keeps
 *   its single meaning: absent. Conflating "did not fit" with "absent" is a
 *   silent-wrong-answer bug, not a safe degradation — `System.getProperty` answered
 *   null for a value too long to stage, and null is what the specification reserves
 *   for a property that is not defined.
 *
 * Why an embedder-level suite and not a Java-level one: the compiled library is a
 * COOPERATIVE caller. It stages at offset 0, sizes its own spans, and never asks for
 * a span it did not measure — so no program written in Java can reach these paths.
 * A third-party plugin can: `javelina.c:227` binds a plugin's non-`jre` imports
 * straight through `exec_host_for`, so a hand-written module declares
 * `(import "java.io.HostIO" "fd_read" ...)` and calls it with whatever offsets it likes.
 * This suite IS that module — it resolves the natives exactly as instantiation does
 * and calls them with hostile arguments.
 *
 * Run it under SAN=1 as well as plain: the assertions below catch a native that
 * answers wrongly, and AddressSanitizer catches one that answers correctly after
 * having already read or written past the end.
 */
#include "host_io.h"        /* the embedder host-native floor (includes <wasm.h>) */
#include "javelina_test.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── the staging memory, and the sentinel that proves nothing was written ──── */

#define PAGE 0x10000
#define SENTINEL 0xA5

/* The suite's embedder context — the floor's state, held the way an application holds it. */
static jav_host_t     g_host;
static wasm_memory_t* g_mem;
static int            g_membytes;

static void staging_new(wasm_store_t* st, uint32_t pages) {
    wasm_limits_t lim = { pages, 0, true };
    wasm_memorytype_t* mt = wasm_memorytype_new(WASM_I32, &lim);
    g_mem = wasm_memory_new(st, mt);
    wasm_memorytype_delete(mt);
    g_membytes = (int)wasm_memory_data_size(g_mem);
    g_host.mem = g_mem;
}

static void staging_fill(void) { memset(wasm_memory_data(g_mem), SENTINEL, (size_t)g_membytes); }

/* Is [off, off+len) still all sentinel? The write-side natives are checked with
 * this rather than by trusting their return value: refusing and then writing
 * anyway is exactly the failure the return value cannot show. */
static int untouched(int off, int len) {
    const byte_t* m = wasm_memory_data(g_mem);
    for (int i = off; i < off + len; i++) if ((unsigned char)m[i] != SENTINEL) return 0;
    return 1;
}

/* Stage `s` at `off` and answer its length — the guest half of a path/key call. */
static int stage(int off, const char* s) {
    byte_t* m = wasm_memory_data(g_mem);
    int n = (int)strlen(s);
    memcpy(m + off, s, (size_t)n);
    return n;
}

/* ── resolve-and-call, the way instantiation does ─────────────────────────── */

static wasm_store_t* g_st;

static wasm_functype_t* ft_of(const char* sig) {
    /* "params:results", one char per value: i=i32 I=i64 f=f32 F=f64 r=externref */
    wasm_valtype_t* pv[8]; wasm_valtype_t* rv[8];
    size_t np = 0, nr = 0; int after = 0;
    for (const char* c = sig; *c; c++) {
        if (*c == ':') { after = 1; continue; }
        wasm_valtype_t* t =
            *c == 'i' ? wasm_valtype_new(WASM_I32) : *c == 'I' ? wasm_valtype_new(WASM_I64) :
            *c == 'f' ? wasm_valtype_new(WASM_F32) : *c == 'F' ? wasm_valtype_new(WASM_F64) :
                        wasm_valtype_new(WASM_EXTERNREF);
        if (after) rv[nr++] = t; else pv[np++] = t;
    }
    wasm_valtype_vec_t p, r;
    wasm_valtype_vec_new(&p, np, pv);
    wasm_valtype_vec_new(&r, nr, rv);
    return wasm_functype_new(&p, &r);
}

/* Call (mod, fld) at `sig` with `argc` i32 arguments and answer its scalar result.
 * A trap answers TRAPPED — no native here is allowed to trap, so it reads as a
 * failure at every call site rather than being silently folded into a value. */
#define TRAPPED (-999999)
static int64_t call_on(jav_host_t* h, const char* mod, const char* fld, const char* sig,
                       const int* argv, size_t argc) {
    wasm_name_t m, f;
    wasm_name_new_from_string(&m, mod);
    wasm_name_new_from_string(&f, fld);
    wasm_functype_t* ft = ft_of(sig);
    wasm_func_t* fn = exec_host_for(h, g_st, ft, &m, &f);

    wasm_val_t a[6];
    for (size_t i = 0; i < argc; i++) { a[i].kind = WASM_I32; a[i].of.i32 = argv[i]; }
    wasm_val_t rbuf[1] = { WASM_INIT_VAL };
    int nres = strchr(sig, ':')[1] ? 1 : 0;
    wasm_val_vec_t args = { argc, a }, res = { (size_t)nres, rbuf };
    wasm_trap_t* t = wasm_func_call(fn, &args, &res);

    int64_t out = TRAPPED;
    if (!t) out = nres == 0 ? 0 : (rbuf[0].kind == WASM_I64 ? rbuf[0].of.i64 : rbuf[0].of.i32);
    else wasm_trap_delete(t);
    wasm_func_delete(fn);
    wasm_functype_delete(ft);
    wasm_name_delete(&m); wasm_name_delete(&f);
    return out;
}
static int64_t call_n(const char* mod, const char* fld, const char* sig,
                      const int* argv, size_t argc) {
    return call_on(&g_host, mod, fld, sig, argv, argc);
}
#define CALL2(mod, fld, sig, a0, a1)         call_n(mod, fld, sig, (const int[]){a0, a1}, 2)
#define CALL3(mod, fld, sig, a0, a1, a2)     call_n(mod, fld, sig, (const int[]){a0, a1, a2}, 3)
#define CALL4(mod, fld, sig, a0, a1, a2, a3) call_n(mod, fld, sig, (const int[]){a0, a1, a2, a3}, 4)

/* ── the sandbox the path natives resolve under ───────────────────────────── */

static char g_root[256];

static void root_new(void) {
    snprintf(g_root, sizeof g_root, "/tmp/javhostmem_%d", (int)getpid());
    mkdir(g_root, 0777);
    g_host.root = g_root;
}

int main(void) {
    wasm_engine_t* eng = wasm_engine_new();
    g_st = wasm_store_new(eng);
    staging_new(g_st, 1);
    root_new();

    /* A real fd, so the span checks below are reached rather than short-circuited
     * on a closed descriptor. The table is empty here, so this is fd 0. */
    int fd = (int)call_n("java.io.HostIO", "fd_open_temp", ":i", NULL, 0);
    CHECK(fd >= 0, "fd_open_temp gives the suite a real descriptor to aim at");

    /* ── 1. Span refusal, read side. A span that runs off the end of the memory
     *      must be refused. The near cases overrun by a few bytes (what a
     *      length miscalculation looks like); the far cases are a hostile
     *      offset, which is what a third-party plugin looks like. ── */
    staging_fill();
    const int NEAR = g_membytes - 8;        /* 8 bytes of room, 64 asked for */
    const int FAR  = 0x7ffffff0;            /* nowhere near the memory */

    CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", NEAR, 64) == -1,
          "checksum refuses a span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", FAR, 64) == -1,
          "checksum refuses a wild offset");
    CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", 0, -1) == -1,
          "checksum refuses a negative length");
    CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", -1, 4) == -1,
          "checksum refuses a negative offset");
    CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", g_membytes, 0) == 0,
          "checksum accepts the empty span at the very end (off == size, len == 0)");

    CHECK(CALL3("java.io.HostIO", "fd_write", "iii:i", fd, NEAR, 64) == -1,
          "fd_write refuses a span that runs off the end");
    CHECK(CALL3("java.io.HostIO", "fd_write", "iii:i", fd, FAR, 64) == -1,
          "fd_write refuses a wild offset");

    /* ── 2. Span refusal, write side — the one that corrupts rather than leaks.
     *      fd_read writes the bytes it read INTO the memory, and hio_list writes
     *      a directory's entry names there, both unbounded before this suite. ── */
    CHECK(CALL3("java.io.HostIO", "fd_read", "iii:i", fd, NEAR, 64) == -1,
          "fd_read refuses a span that runs off the end");
    CHECK(untouched(NEAR, 8), "fd_read wrote nothing after refusing");
    CHECK(CALL3("java.io.HostIO", "fd_read", "iii:i", fd, FAR, 64) == -1,
          "fd_read refuses a wild offset");

    /* ── 3. Every path native reads its name through io_hostpath, which bounded
     *      the LENGTH and never the OFFSET. One check covers all of them, so one
     *      case per native is the coverage — a shared helper is exactly the place
     *      a per-native assumption hides. Each answers its own documented
     *      "no such path" value; none may read outside the memory to decide. ── */
    CHECK(CALL3("java.io.HostIO", "open", "iii:i", NEAR, 40, 0) == -1,
          "open refuses a path span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "stat", "ii:i", NEAR, 40) == 0,
          "stat refuses a path span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "fileSize", "ii:I", NEAR, 40) == -1,
          "fileSize refuses a path span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "fileModified", "ii:I", NEAR, 40) == 0,
          "fileModified refuses a path span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "unlink", "ii:i", NEAR, 40) == -1,
          "unlink refuses a path span that runs off the end");
    CHECK(CALL2("java.io.HostIO", "mkdir", "ii:i", NEAR, 40) == -1,
          "mkdir refuses a path span that runs off the end");
    CHECK(CALL4("java.io.HostIO", "rename", "iiii:i", NEAR, 40, 0, 4) == -1,
          "rename refuses a from-path span that runs off the end");
    CHECK(CALL4("java.io.HostIO", "rename", "iiii:i", 0, 4, NEAR, 40) == -1,
          "rename refuses a to-path span that runs off the end");
    CHECK(CALL3("java.io.HostIO", "list", "iii:i", FAR, 40, 0) == -1,
          "list refuses a path span at a wild offset");
    CHECK(CALL3("java.io.HostIO", "getprop", "iii:i", NEAR, 40, 0) == -1,
          "getprop refuses a key span that runs off the end");

    /* ── 4. The composed host path is `root + '/' + guest path` into a fixed
     *      buffer, and the ROOT is the embedder's, not the guest's — `javelina
     *      --root DIR` takes it from the command line unchecked. A deep root plus
     *      a long guest path overflowed a 512-byte stack buffer, which is a stack
     *      smash reached from an in-bounds, entirely legal guest call. ── */
    {
        char deep[400];
        memset(deep, 'd', sizeof deep - 1); deep[sizeof deep - 1] = 0;
        deep[0] = '/';
        const char* saved = g_host.root;
        g_host.root = deep;
        int n = stage(0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        CHECK(CALL2("java.io.HostIO", "stat", "ii:i", 0, n) == 0,
              "a root too deep to compose with the guest path is refused, not overflowed");
        g_host.root = saved;
    }

    /* ── 5. The three-way result: getprop. The value is longer than the room the
     *      guest left, so the answer is the length it NEEDS — not -1, which means
     *      the property is not defined, and not a truncated write. ── */
    {
        static char big[4096];
        memset(big, 'v', sizeof big - 1); big[sizeof big - 1] = 0;
        static hio_prop_t props[] = { { "big.value", NULL }, { "small.value", "ok" }, { NULL, NULL } };
        props[0].val = big;
        g_host.props = props;

        staging_fill();
        int klen = stage(0, "big.value");
        int need = (int)strlen(big);

        /* Too little room: the answer is the size, and NOTHING is written. */
        int tight = g_membytes - 16;
        int64_t r = CALL3("java.io.HostIO", "getprop", "iii:i", 0, klen, tight);
        CHECK(r == need, "getprop answers the length it needs when the value does not fit");
        CHECK(untouched(tight, 16), "getprop wrote nothing when the value did not fit");

        /* Room: the same length, and the bytes are there. */
        staging_fill();
        klen = stage(0, "big.value");
        r = CALL3("java.io.HostIO", "getprop", "iii:i", 0, klen, klen);
        CHECK(r == need, "getprop answers the same length when the value fits");
        CHECK(!untouched(klen, need) && untouched(klen + need, 16),
              "getprop wrote exactly the value and no more");

        /* -1 keeps ONE meaning. This is the case the old conflation destroyed:
         * with "did not fit" also spelled -1, a plugin could not tell a property
         * it must stage a bigger buffer for from one the embedder withheld. */
        staging_fill();
        klen = stage(0, "no.such.property");
        CHECK(CALL3("java.io.HostIO", "getprop", "iii:i", 0, klen, klen) == -1,
              "getprop answers -1 only for a property that is absent");
        g_host.props = NULL;
    }

    /* ── 6. The three-way result: propnames and list, which write a NUL-separated
     *      run whose length the guest cannot know in advance. Same rule; list
     *      keeps -1 for "not a directory", which is the answer File.list() turns
     *      into null, so a large directory must NOT reach it. ── */
    {
        static hio_prop_t props[] = { { "one", "1" }, { "two", "2" }, { "three", "3" }, { NULL, NULL } };
        g_host.props = props;
        int need = 4 + 4 + 6;                     /* each name plus its NUL */

        staging_fill();
        int tight = g_membytes - 4;
        CHECK(call_n("java.io.HostIO", "propnames", "i:i", (const int[]){tight}, 1) == need,
              "propnames answers the total it needs when the names do not fit");
        CHECK(untouched(tight, 4), "propnames wrote nothing when the names did not fit");

        staging_fill();
        CHECK(call_n("java.io.HostIO", "propnames", "i:i", (const int[]){0}, 1) == need,
              "propnames answers the same total when the names fit");
        CHECK(!untouched(0, need) && untouched(need, 16),
              "propnames wrote exactly the names and no more");
        g_host.props = NULL;
    }
    {
        /* A directory with entries whose names are known, so the needed total is
         * arithmetic rather than an observation of the answer under test. */
        char d[512];
        snprintf(d, sizeof d, "%s/listing", g_root);
        mkdir(d, 0777);
        const char* ents[] = { "alpha", "beta", "gamma", NULL };
        int need = 0;
        for (int i = 0; ents[i]; i++) {
            char f[600]; snprintf(f, sizeof f, "%s/%s", d, ents[i]);
            FILE* fp = fopen(f, "wb"); if (fp) fclose(fp);
            need += (int)strlen(ents[i]) + 1;
        }

        staging_fill();
        int nlen = stage(0, "listing");
        int tight = g_membytes - 4;
        CHECK(CALL3("java.io.HostIO", "list", "iii:i", 0, nlen, tight) == need,
              "list answers the total it needs when the entries do not fit");
        CHECK(untouched(tight, 4), "list wrote nothing when the entries did not fit");

        staging_fill();
        nlen = stage(0, "listing");
        CHECK(CALL3("java.io.HostIO", "list", "iii:i", 0, nlen, nlen) == need,
              "list answers the same total when the entries fit");
        CHECK(untouched(nlen + need, 16), "list wrote exactly the entries and no more");

        staging_fill();
        nlen = stage(0, "no_such_dir");
        CHECK(CALL3("java.io.HostIO", "list", "iii:i", 0, nlen, nlen) == -1,
              "list answers -1 only for a path that is not a directory");
    }

    /* ── 7. The cooperative path still works. Every refusal above is worthless if
     *      it also refuses the calls the compiled library actually makes, so pin
     *      one ordinary round trip: stage bytes, write them, seek, read them back
     *      into a different part of the memory, and checksum what arrived. ── */
    {
        staging_fill();
        byte_t* m = wasm_memory_data(g_mem);
        m[0] = 10; m[1] = 20; m[2] = (byte_t)200;
        CHECK(CALL3("java.io.HostIO", "fd_write", "iii:i", fd, 0, 3) == 3, "fd_write writes an in-bounds span");
        CHECK(CALL2("java.io.HostIO", "fd_seek", "ii:", fd, 0) == 0, "fd_seek rewinds");
        CHECK(CALL3("java.io.HostIO", "fd_read", "iii:i", fd, 64, 3) == 3, "fd_read reads into an in-bounds span");
        CHECK(CALL2("java.io.HostIO", "checksum", "ii:i", 64, 3) == 230, "the bytes made the round trip intact");
        CHECK(call_n("java.io.HostIO", "fd_close", "i:", (const int[]){fd}, 1) == 0, "fd_close closes");
    }

    /* ── 8. Two contexts in one process, each seeing only its own state ────────────────
     * §7.1.5 makes the store the unit of state and §7.1.8 allocates a host function in one, so
     * a floor whose state is process-scoped cannot serve two stores. Every assertion here is
     * about that: two contexts, two staging memories, two filesystem roots, two fd tables. An
     * application holding two documents is exactly this shape. */
    {
        jav_host_t a, b;
        memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);

        char ra[300], rb[300];
        snprintf(ra, sizeof ra, "%s/ctxA", g_root); mkdir(ra, 0777); a.root = ra;
        snprintf(rb, sizeof rb, "%s/ctxB", g_root); mkdir(rb, 0777); b.root = rb;

        wasm_limits_t lim = { 1, 0, true };
        wasm_memorytype_t* mt = wasm_memorytype_new(WASM_I32, &lim);
        wasm_memory_t* ma = wasm_memory_new(g_st, mt);
        wasm_memory_t* mb = wasm_memory_new(g_st, mt);
        wasm_memorytype_delete(mt);
        a.mem = ma; b.mem = mb;

        /* Distinct staging memories: what A stages is not what B reads. */
        memset(wasm_memory_data(ma), 0, 16); memset(wasm_memory_data(mb), 0, 16);
        memcpy(wasm_memory_data(ma), "kid", 3);
        memcpy(wasm_memory_data(mb), "kid", 3);
        ((byte_t*)wasm_memory_data(ma))[3] = (byte_t)7;
        CHECK(call_on(&a, "java.io.HostIO", "checksum", "ii:i", (const int[]){3, 1}, 2) == 7,
              "context A reads its OWN staging memory");
        CHECK(call_on(&b, "java.io.HostIO", "checksum", "ii:i", (const int[]){3, 1}, 2) == 0,
              "context B does not see what A staged");

        /* Distinct roots: a directory created through A is not visible through B. */
        CHECK(call_on(&a, "java.io.HostIO", "mkdir", "ii:i", (const int[]){0, 3}, 2) == 0,
              "mkdir through context A succeeds under A's root");
        CHECK(call_on(&a, "java.io.HostIO", "stat",  "ii:i", (const int[]){0, 3}, 2) != 0,
              "and A sees it");
        CHECK(call_on(&b, "java.io.HostIO", "stat",  "ii:i", (const int[]){0, 3}, 2) == 0,
              "while B, rooted elsewhere, does not");

        /* Distinct fd tables: each context numbers its own descriptors from zero. */
        int fa = (int)call_on(&a, "java.io.HostIO", "fd_open_temp", ":i", NULL, 0);
        int fb = (int)call_on(&b, "java.io.HostIO", "fd_open_temp", ":i", NULL, 0);
        CHECK(fa == 0 && fb == 0, "each context numbers its own fds from zero");
        CHECK(a.fds[0] && b.fds[0] && a.fds[0] != b.fds[0],
              "and fd 0 in one is not fd 0 in the other");
        call_on(&a, "java.io.HostIO", "fd_close", "i:", (const int[]){fa}, 1);
        call_on(&b, "java.io.HostIO", "fd_close", "i:", (const int[]){fb}, 1);

        wasm_memory_delete(ma); wasm_memory_delete(mb);
    }

    exec_host_release();
    wasm_memory_delete(g_mem);
    wasm_store_delete(g_st); wasm_engine_delete(eng);
    return TEST_SUMMARY("host abi staging memory");
}
