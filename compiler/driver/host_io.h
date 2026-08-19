/* host_io.h — the javelina embedder's host-native floor (the →HOST contract).
 *
 * This is the set of genuine environment edges a javelina embedder must supply for
 * a compiled Java program to run: the file-descriptor I/O floor (HostIO fd ops + open
 * and File stat/dir ops), the IEEE bit-reinterpret intrinsics, and the clock/exit/
 * identity-hash/random source. Everything crosses the GC↔host boundary as PRIMITIVES
 * through a linear-memory staging buffer (§7.1: GC aggregates are opaque to the host;
 * the guest copies its byte[] into linear memory, the host touches only that).
 *
 * Shared by the test harness (test/exec.h — fds 0/1/2 are capture temp files so tests
 * can assert what was written) and the shipped runner (driver/javelina.c — fds 0/1/2
 * are real stdin/stdout/stderr). The two differ ONLY in how the standard fds are
 * preopened and in the filesystem root; the native implementations here are identical.
 *
 * Boundary rule: includes only <wasm.h>. */
#ifndef JAVELINA_HOST_IO_H
#define JAVELINA_HOST_IO_H

#include "wasm.h"
#include "bbq_dict.h"   /* the →HOST resolution index (qualified name -> row) */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>   /* File stat floor: stat/mkdir */
#include <unistd.h>     /* access() for canRead/canWrite */
#include <dirent.h>     /* opendir/readdir for File.list */

#define IO_MAX_FDS 16

/* ── The system-property source (§20.18.7). Properties cross as BYTES, never as Strings: a
 * host function cannot construct a GC String (§7.1 — aggregates are opaque to the host), so
 * `java.lang.System` asks for a value through the staging memory and builds the String itself.
 * A NULL table (or a key not in it) is an ABSENT property, not an error. ── */
typedef struct { const char* key; const char* val; } hio_prop_t;

typedef struct jav_host_s jav_host_t;

/* ── THE embedder context: everything the →HOST floor needs, in ONE object the embedder owns.
 *
 * State here is per-context, never per-process, and that is a spec requirement rather than a
 * preference: §7.1.5 makes the STORE the unit of state, and §7.1.8 allocates a host function IN
 * a store (`func_alloc(store, deftype, hostfunc)`). Two stores in one process must therefore not
 * share a staging memory, an fd table or a filesystem root — an application holding two
 * documents is the case that breaks if they do.
 *
 * The store is opaque to embedders and carries no host_info slot (it is WASM_DECLARE_OWN; only
 * WASM_DECLARE_REF_BASE types get one), so the carrier is §7.1.8's `hostfunc` closure —
 * wasm_func_new_with_env. Every native below is registered with it and reaches this object
 * through its env. ── */
struct jav_host_s {
    /* The GC↔host bridge: the executing module's exported I/O staging memory, captured at
     * instantiation. Natives read/write the guest's bytes here via wasm_memory_data. */
    wasm_memory_t* mem;

    /* The filesystem root every guest path resolves under. NULL ⇒ a lazily-created per-context
     * temp sandbox (the test default); the runner sets it to a real directory. */
    const char* root;
    char        sandbox[64];        /* the lazily-created default, per context, not per process */

    /* The fd table: a small fd → FILE* map. fds 0/1/2 are preopened by the embedder
     * (test: capture temp files; runner: real std streams). */
    FILE* fds[IO_MAX_FDS];

    const hio_prop_t* props;        /* terminated by a NULL key; NULL ⇒ no properties */

    /* The environment sources the embedder owns. NULL ⇒ the reproducible defaults, which is what
     * the test harness wants; the runner installs the real clock and exit. */
    int64_t (*clock)(void);         /* epoch milliseconds */
    void    (*exit_fn)(int);        /* non-NULL ⇒ terminate with the guest's code */
    int64_t ctm_ticks;              /* the deterministic clock's counter, per context */
    int32_t next_id;                /* identity-hash source, per context */

    /* Application natives: an embedder may answer imports this contract does not name (an app
     * exposing its own functions to the guest). Returning NULL falls through to the fail-closed
     * stub, so registering a hook never re-opens the silent-echo hole. */
    wasm_func_t* (*host_extra)(jav_host_t*, wasm_store_t*, const wasm_functype_t*,
                               const wasm_name_t* mod, const wasm_name_t* fld);
};

/* What a registered native receives as its env: the context, plus the per-row detail a few rows
 * need (the op tag, the store a trap is built on, the qualified name the unimplemented stub
 * reports). One allocation per registered function, released by hio_bind_free. */
typedef struct { jav_host_t* h; wasm_store_t* store; int op; char* name; } hio_bind_t;

static void hio_bind_free(void* p) { hio_bind_t* b = (hio_bind_t*)p; if (b) { free(b->name); free(b); } }

static byte_t* io_membytes(jav_host_t* h) { return h && h->mem ? wasm_memory_data(h->mem) : NULL; }
static size_t  io_memsize(jav_host_t* h)  { return h && h->mem ? wasm_memory_data_size(h->mem) : 0; }
static int     io_fd_ok(jav_host_t* h, int fd) { return h && fd >= 0 && fd < IO_MAX_FDS && h->fds[fd]; }

/* ── THE span check. A staging-memory span the host may touch: [off, off+len)
 * wholly inside the memory. Every native that dereferences the staging pointer
 * runs this first, and a span that fails it is REFUSED — never clamped, never
 * partially serviced (docs/host-abi.md, "Spans are refused, not clamped").
 *
 * The arithmetic is written to be overflow-free rather than obviously-correct:
 * `off + len <= n` would wrap for a hostile pair, so the length is compared
 * against the room that remains. `io_membytes()` re-reads wasm_memory_data on
 * every call, so a memory.grow between calls cannot leave a stale base behind.
 *
 * This is a contract obligation, not an implementation detail of this file. The
 * guest chooses the offsets; a compiled Java program chooses them cooperatively,
 * a third-party plugin chooses them adversarially, and the floor cannot tell the
 * two apart — `javelina.c` binds a plugin's non-`jre` imports straight to these
 * natives. Pinned by test_host_memory.c. ── */
static int io_span_ok(jav_host_t* h, int off, int len) {
    size_t n = io_memsize(h);
    return off >= 0 && len >= 0 && (size_t)off <= n && (size_t)len <= n - (size_t)off;
}

/* The composed host path buffer every path native declares. Both halves are
 * bounded against it in io_hostpath: the guest's bytes AND the embedder's root,
 * which arrives from `javelina --root DIR` and is not the guest's to shorten. */
#define IO_PATH_MAX 512

/* Map a guest path (staging bytes at off,len) to a host path under the context's root,
 * PRESERVING '/' so directory nesting + File.list work. Rejects any ".." (no parent
 * escape). If the root is NULL, a fresh temp sandbox is created FOR THIS CONTEXT —
 * per-context, not per-process, so two embeddings do not share a directory.
 * Answers 0 — which every caller already maps to its own "no such path" value —
 * for a span outside the memory, and for a composed path that does not fit. */
static int io_hostpath(jav_host_t* h, int off, int len, char* out) {
    byte_t* m = io_membytes(h);
    if (!m || !io_span_ok(h, off, len)) return 0;
    const char* root = h->root;
    if (!root) {
        if (!h->sandbox[0]) {
            snprintf(h->sandbox, sizeof h->sandbox, "/tmp/javio_%d_%p", (int)getpid(), (void*)h);
            mkdir(h->sandbox, 0777);
        }
        root = h->sandbox;
    }
    size_t rl = strlen(root);
    if (rl + 1 + (size_t)len + 1 > IO_PATH_MAX) return 0;   /* root + '/' + path + NUL */
    for (int i = 0; i + 1 < len; i++)
        if (m[off + i] == '.' && m[off + i + 1] == '.') return 0;   /* reject parent-directory escape */
    int k = 0;
    for (const char* p = root; *p; p++) out[k++] = *p;
    out[k++] = '/';
    for (int i = 0; i < len; i++) out[k++] = (char)m[off + i];      /* keep '/' → real nesting */
    out[k] = 0;
    return 1;
}

/* ── The host natives. Each is a §7.1 host function; the dispatch is exec_host_for. ── */

/* IEEE-754 bit reinterprets — Float/Double.{floatToIntBits,doubleToLongBits} + inverses.
 * No WASM-GC primitive reinterprets a float's bits; the embedder supplies them. */
/* Every native takes the §7.1.8 hostfunc env. HOST_H unwraps the context from it; the few that
 * need the per-row detail read the binding's other fields directly. */
#define HOST_H(env) (((hio_bind_t*)(env))->h)

static wasm_trap_t* hio_f2i(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; float f = a->data[0].of.f32; int32_t i; memcpy(&i, &f, 4);
    r->data[0] = (wasm_val_t)WASM_I32_VAL(i); return NULL;
}
static wasm_trap_t* hio_i2f(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; int32_t i = a->data[0].of.i32; float f; memcpy(&f, &i, 4);
    r->data[0] = (wasm_val_t)WASM_F32_VAL(f); return NULL;
}
static wasm_trap_t* hio_d2l(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; double d = a->data[0].of.f64; int64_t l; memcpy(&l, &d, 8);
    r->data[0] = (wasm_val_t)WASM_I64_VAL(l); return NULL;
}
static wasm_trap_t* hio_l2d(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; int64_t l = a->data[0].of.i64; double d; memcpy(&d, &l, 8);
    r->data[0] = (wasm_val_t)WASM_F64_VAL(d); return NULL;
}

/* The clock/exit/identity-hash source — the genuine environment floor. One callback dispatches
 * on the binding's op tag. Deterministic by default and per context, so two embeddings do not
 * share a counter and each is reproducible on its own; the runner wires the real clock.
 * (Math.random is NOT here: it is java.util.Random over this clock, §20.11.20.) */
typedef enum { HOP_CTM, HOP_EXIT, HOP_IDHASH } host_op;

static wasm_trap_t* hio_env(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    hio_bind_t* b = (hio_bind_t*)env; jav_host_t* h = b->h;
    switch ((host_op)b->op) {
    case HOP_CTM:    r->data[0] = (wasm_val_t)WASM_I64_VAL(h->clock ? h->clock() : ++h->ctm_ticks); break;
    case HOP_IDHASH: r->data[0] = (wasm_val_t)WASM_I32_VAL(h->next_id++);   break;
    case HOP_EXIT:   if (h->exit_fn) h->exit_fn(a->size > 0 ? a->data[0].of.i32 : 0); break;
    }
    return NULL;
}

/* ── §20.18.7 system properties, over the staging memory (see hio_prop_t).
 * getprop(keyoff, keylen, outoff) → value length written at outoff, or -1 if the key is absent.
 * propnames(outoff)              → total bytes of the NUL-separated key list written at outoff. ── */
static wasm_trap_t* hio_getprop(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int koff = a->data[0].of.i32, klen = a->data[1].of.i32, ooff = a->data[2].of.i32;
    byte_t* m = io_membytes(h);
    int n = -1;                                        /* -1 is ABSENT, and only absent */
    if (m && io_span_ok(h, koff, klen)) {
        for (const hio_prop_t* p = h->props; p && p->key; p++) {
            size_t kl = strlen(p->key);
            if (kl != (size_t)klen || memcmp(m + koff, p->key, kl)) continue;
            size_t vl = strlen(p->val);
            n = (int)vl;                               /* the length the answer NEEDS */
            if (io_span_ok(h, ooff, (int)vl)) memcpy(m + ooff, p->val, vl); /* written only if it ALL fits */
            break;
        }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n); return NULL;
}
static wasm_trap_t* hio_propnames(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int ooff = a->data[0].of.i32, total = 0;
    byte_t* m = io_membytes(h);
    for (const hio_prop_t* p = h->props; p && p->key; p++) total += (int)strlen(p->key) + 1;
    if (m && io_span_ok(h, ooff, total)) {             /* the WHOLE run or none of it */
        int k = 0;
        for (const hio_prop_t* p = h->props; p && p->key; p++) {
            size_t kl = strlen(p->key);
            memcpy(m + ooff + k, p->key, kl); k += (int)kl;
            m[ooff + k++] = 0;
        }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(total); return NULL;
}

/* Object monitor ops on a threadless target: no monitor exists → trap (§17 is meaningless here). */
static wasm_trap_t* hio_monitor_trap(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; (void)results;
    wasm_message_t msg; wasm_name_new_from_string_nt(&msg, "monitor op on threadless target");
    wasm_trap_t* t = wasm_trap_new(((hio_bind_t*)env)->store, &msg);
    wasm_byte_vec_delete(&msg);
    return t;
}

/* A probe: sum `len` staging bytes at `off`, so a test can prove the host observes the
 * guest's memory. A byte sum is never negative, so -1 is free to mean a refused span. */
static wasm_trap_t* hio_checksum(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int off = a->size > 0 ? a->data[0].of.i32 : 0;
    int len = a->size > 1 ? a->data[1].of.i32 : 0;
    byte_t* m = io_membytes(h);
    int sum = -1;
    if (m && io_span_ok(h, off, len)) { sum = 0;
                                        for (int i = 0; i < len; i++) sum += (unsigned char)m[off + i]; }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(sum); return NULL;
}

static wasm_trap_t* hio_fd_open_temp(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env); (void)a; int fd = -1;
    for (int i = 0; i < IO_MAX_FDS; i++) if (!h->fds[i]) { h->fds[i] = tmpfile(); if (h->fds[i]) fd = i; break; }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(fd); return NULL;
}
static wasm_trap_t* hio_open(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int flags = a->data[2].of.i32, fd = -1;
    char path[IO_PATH_MAX];
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path)) {
        FILE* fp;
        if (flags == 2) { fp = fopen(path, "r+b"); if (!fp) fp = fopen(path, "w+b"); }  /* read+write: open else create */
        else fp = fopen(path, (flags & 1) ? "wb" : "rb");                                /* 1 = write/truncate, 0 = read */
        if (fp) for (int i = 0; i < IO_MAX_FDS; i++) if (!h->fds[i]) { h->fds[i] = fp; fd = i; break; }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(fd); return NULL;
}
static wasm_trap_t* hio_fd_write(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int fd = a->data[0].of.i32, off = a->data[1].of.i32, len = a->data[2].of.i32, n = -1;
    byte_t* m = io_membytes(h);
    if (m && io_fd_ok(h, fd) && io_span_ok(h, off, len)) {
        n = (int)fwrite(m + off, 1, (size_t)len, h->fds[fd]);
        if (fd == 1 || fd == 2) fflush(h->fds[fd]);   /* flush std streams promptly */
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n); return NULL;   /* -1 = refused (bad fd or span) */
}
static wasm_trap_t* hio_fd_read(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int fd = a->data[0].of.i32, off = a->data[1].of.i32, len = a->data[2].of.i32, n = 0;
    byte_t* m = io_membytes(h);
    if (m && io_fd_ok(h, fd) && io_span_ok(h, off, len)) n = (int)fread(m + off, 1, (size_t)len, h->fds[fd]);
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n > 0 ? n : -1);   /* -1 = EOF (InputStream.read) or refused */
    return NULL;
}
static wasm_trap_t* hio_fd_seek(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    (void)r; int fd = a->data[0].of.i32, pos = a->data[1].of.i32;
    if (io_fd_ok(h, fd)) fseek(h->fds[fd], pos, SEEK_SET);
    return NULL;
}
static wasm_trap_t* hio_fd_close(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    (void)r; int fd = a->data[0].of.i32;
    if (io_fd_ok(h, fd) && fd > 2) { fclose(h->fds[fd]); h->fds[fd] = NULL; }   /* never close std streams */
    return NULL;
}
static wasm_trap_t* hio_fd_size(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    int fd = a->data[0].of.i32; long long sz = -1;
    if (io_fd_ok(h, fd)) {
        FILE* fp = h->fds[fd];
        fflush(fp);
        long cur = ftell(fp);
        if (fseek(fp, 0, SEEK_END) == 0) { long end = ftell(fp); if (end >= 0) sz = (long long)end; }
        if (cur >= 0) fseek(fp, cur, SEEK_SET);
    }
    r->data[0] = (wasm_val_t)WASM_I64_VAL(sz); return NULL;
}

/* ── §22.4 File stat/action floor. ── */
static wasm_trap_t* hio_stat(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; int flags = 0; struct stat st;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0) {
        flags |= 1;                                        /* exists */
        if (S_ISDIR(st.st_mode)) flags |= 2;               /* dir    */
        if (S_ISREG(st.st_mode)) flags |= 4;               /* file   */
        if (access(path, R_OK) == 0) flags |= 8;           /* canRead */
        if (access(path, W_OK) == 0) flags |= 16;          /* canWrite*/
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(flags); return NULL;
}
static wasm_trap_t* hio_file_size(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; long long sz = -1; struct stat st;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0 && S_ISREG(st.st_mode))
        sz = (long long)st.st_size;
    r->data[0] = (wasm_val_t)WASM_I64_VAL(sz); return NULL;
}
static wasm_trap_t* hio_file_modified(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; long long ms = 0; struct stat st;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0)
        ms = (long long)st.st_mtime * 1000;
    r->data[0] = (wasm_val_t)WASM_I64_VAL(ms); return NULL;
}
static wasm_trap_t* hio_unlink(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; int rc = -1;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path)) rc = (remove(path) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
static wasm_trap_t* hio_mkdir(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; int rc = -1;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path)) rc = (mkdir(path, 0777) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
static wasm_trap_t* hio_rename(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char from[IO_PATH_MAX], to[IO_PATH_MAX]; int rc = -1;
    if (io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, from) &&
        io_hostpath(h, a->data[2].of.i32, a->data[3].of.i32, to)) rc = (rename(from, to) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
/* The entry names, NUL-separated, at outoff — or, when they do not all fit, the total
 * they need and nothing written. The total is COUNTED either way: a directory bigger
 * than the staging memory is not "not a directory", and -1 is what File.list() turns
 * into null, which §22.4 reserves for a path that is not a directory. A guest that
 * gets a total larger than the room it left grows the memory and asks again. */
static wasm_trap_t* hio_list(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    jav_host_t* h = HOST_H(env);
    char path[IO_PATH_MAX]; int total = -1; byte_t* m = io_membytes(h);
    int outoff = a->data[2].of.i32;
    if (m && io_hostpath(h, a->data[0].of.i32, a->data[1].of.i32, path)) {
        DIR* dp = opendir(path);
        if (dp) {
            total = 0; struct dirent* de;
            while ((de = readdir(dp)) != NULL) {
                const char* nm = de->d_name;
                if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0))) continue;   /* skip . and .. */
                total += (int)strlen(nm) + 1;               /* the name plus its NUL separator */
            }
            if (io_span_ok(h, outoff, total)) {             /* the WHOLE listing or none of it */
                rewinddir(dp);
                int k = 0;
                while ((de = readdir(dp)) != NULL) {
                    const char* nm = de->d_name;
                    if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0))) continue;
                    size_t nl = strlen(nm);
                    if (k + (int)nl + 1 > total) break;     /* the directory changed under us */
                    memcpy(m + outoff + k, nm, nl); k += (int)nl;
                    m[outoff + k++] = 0;
                }
                total = k;
            }
            closedir(dp);
        }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(total); return NULL;
}

/* A native the embedder does not implement. Calling it TRAPS, naming the method — the engine's
 * fail-closed answer to an unsatisfied environment edge. It is never an echo or a zero: a silently
 * wrong native is indistinguishable from a working one, and every native this floor does not name
 * is, by definition, either compiler-lowered (never called) or not yet implemented. The binding
 * carries the method name, owned there and freed with the func. */
static wasm_trap_t* hio_unimplemented(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; (void)results;
    hio_bind_t* b = (hio_bind_t*)env;
    char buf[256];
    snprintf(buf, sizeof buf, "unimplemented native '%s' (not part of the javelina host contract)",
             b->name ? b->name : "?");
    wasm_message_t msg; wasm_name_new_from_string_nt(&msg, buf);
    wasm_trap_t* t = wasm_trap_new(b->store, &msg);
    wasm_byte_vec_delete(&msg);
    return t;
}

/* §20.18 no-op natives: methods whose contract is satisfied by doing nothing on this target.
 * `gc`/`runFinalization` are advisory (the collector is not on-demand); `load`/`loadLibrary` have
 * no dynamic-linking surface; `finalize` is Object's empty body; `setSecurityManager`/`setProperties`
 * accept and discard (there is no security manager — getSecurityManager answers null, §20.18.4). */
static wasm_trap_t* hio_noop(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; (void)a; (void)r; return NULL;
}
static wasm_trap_t* hio_null_ref(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)env; (void)a; r->data[0] = (wasm_val_t)WASM_INIT_VAL; return NULL;   /* a null externref */
}

/* ── the →HOST table ─────────────────────────────────────────────────────────
 * One row per import in docs/host-abi.md, keyed on the QUALIFIED name and
 * carrying the contract's functype. Both halves are load-bearing:
 *
 * THE MODULE NAME IS PART OF THE KEY. A WASM import is a two-part name
 * (§5.5.5) and jre.wasm declares both — `HostIO.open`, `System.exit`,
 * `F32x4.ceil`. Matching on the field alone let any module claim any native
 * (`Whatever.open` reached the real filesystem open) and made the
 * host_extra hook unable to own a namespace: consulted last and handed
 * only the field name, an application's `open` was shadowed by this table's,
 * silently. The hook now receives both names and is asked about every name
 * this table does not claim, so an app owns `App.*` outright.
 *
 * THE SIGNATURE IS CHECKED, NOT ADOPTED. wasm_func_new is handed the type the
 * GUEST declared, so a disagreement used to link and go wrong at call time —
 * `wasi_snapshot_preview1.fd_write` is (fd, iovs, iovs_len, nwritten) → errno
 * and bound straight onto this floor's three-argument (fd, off, len) → i32,
 * reading an iovec pointer as a buffer offset. A row only answers when the
 * declared type is the contract's; otherwise the import falls through to the
 * fail-closed stub, which traps naming the qualified name.
 *
 * `sig` is "params:results", one character per value — i=i32 I=i64 f=f32
 * F=f64 r=any reference — or NULL where the contract admits several shapes
 * (Object.wait has three overloads, and every one of them traps anyway).
 *
 * A name absent from this table is NOT an error: sema emits an import for
 * every `native` declaration, so jre.wasm imports the whole Mem/V128/I8x16../
 * Math intrinsic surface that the compiler lowers to instructions and never
 * calls (host-abi.md, "Imports that are not host calls"). Those bind to the
 * trapping stub, which is what makes a lowering that stopped firing go red
 * instead of quietly returning its own argument. */
#define HIO_PLAIN   (-1)
#define HIO_MONITOR (-2)
typedef struct { const char* mod; const char* fld; const char* sig;
                 wasm_func_callback_with_env_t fn; int op; } hio_row_t;

static const hio_row_t hio_table[] = {
    /* §17 monitors on a threadless target: no monitor exists. */
    { "java.lang.Object", "wait",      NULL,     NULL, HIO_MONITOR },
    { "java.lang.Object", "notify",    NULL,     NULL, HIO_MONITOR },
    { "java.lang.Object", "notifyAll", NULL,     NULL, HIO_MONITOR },

    /* The fd + filesystem floor. */
    { "java.io.HostIO", "checksum",     "ii:i",   hio_checksum,      HIO_PLAIN },
    { "java.io.HostIO", "fd_open_temp", ":i",     hio_fd_open_temp,  HIO_PLAIN },
    { "java.io.HostIO", "open",         "iii:i",  hio_open,          HIO_PLAIN },
    { "java.io.HostIO", "fd_write",     "iii:i",  hio_fd_write,      HIO_PLAIN },
    { "java.io.HostIO", "fd_read",      "iii:i",  hio_fd_read,       HIO_PLAIN },
    { "java.io.HostIO", "fd_seek",      "ii:",    hio_fd_seek,       HIO_PLAIN },
    { "java.io.HostIO", "fd_close",     "i:",     hio_fd_close,      HIO_PLAIN },
    { "java.io.HostIO", "fd_size",      "i:I",    hio_fd_size,       HIO_PLAIN },
    { "java.io.HostIO", "stat",         "ii:i",   hio_stat,          HIO_PLAIN },
    { "java.io.HostIO", "fileSize",     "ii:I",   hio_file_size,     HIO_PLAIN },
    { "java.io.HostIO", "fileModified", "ii:I",   hio_file_modified, HIO_PLAIN },
    { "java.io.HostIO", "unlink",       "ii:i",   hio_unlink,        HIO_PLAIN },
    { "java.io.HostIO", "mkdir",        "ii:i",   hio_mkdir,         HIO_PLAIN },
    { "java.io.HostIO", "rename",       "iiii:i", hio_rename,        HIO_PLAIN },
    { "java.io.HostIO", "list",         "iii:i",  hio_list,          HIO_PLAIN },

    /* The system-property source (bytes through the staging memory). */
    { "java.io.HostIO", "getprop",      "iii:i",  hio_getprop,       HIO_PLAIN },
    { "java.io.HostIO", "propnames",    "i:i",    hio_propnames,     HIO_PLAIN },

    /* IEEE bit reinterprets: no WASM-GC primitive reinterprets a float's bits. */
    { "java.lang.Float",  "floatToIntBits",       "f:i", hio_f2i, HIO_PLAIN },
    { "java.lang.Float",  "floatToRawIntBits",    "f:i", hio_f2i, HIO_PLAIN },
    { "java.lang.Float",  "intBitsToFloat",       "i:f", hio_i2f, HIO_PLAIN },
    { "java.lang.Double", "doubleToLongBits",     "F:I", hio_d2l, HIO_PLAIN },
    { "java.lang.Double", "doubleToRawLongBits",  "F:I", hio_d2l, HIO_PLAIN },
    { "java.lang.Double", "longBitsToDouble",     "I:F", hio_l2d, HIO_PLAIN },

    /* §20.18 no-ops + the absent security manager. */
    { "java.lang.System", "gc",                 ":",   hio_noop,     HIO_PLAIN },
    { "java.lang.System", "runFinalization",    ":",   hio_noop,     HIO_PLAIN },
    { "java.lang.System", "load",               "r:",  hio_noop,     HIO_PLAIN },
    { "java.lang.System", "loadLibrary",        "r:",  hio_noop,     HIO_PLAIN },
    { "java.lang.System", "setSecurityManager", "r:",  hio_noop,     HIO_PLAIN },
    { "java.lang.System", "setProperties",      "r:",  hio_noop,     HIO_PLAIN },
    { "java.lang.Object", "finalize",           "r:",  hio_noop,     HIO_PLAIN },
    { "java.lang.System", "getSecurityManager", ":r",  hio_null_ref, HIO_PLAIN },

    /* The clock / exit / identity-hash sources. */
    { "java.lang.System", "currentTimeMillis", ":I", NULL, HOP_CTM },
    { "java.lang.System", "exit",              "i:", NULL, HOP_EXIT },
    { "java.lang.System", "identityHashCode",  ":i", NULL, HOP_IDHASH },
};

/* The resolution index: qualified name -> row + 1, built from hio_table on first
 * use. A dict rather than a scan of the rows because the rows are a CONTRACT, and
 * an embedder with a real API surface has hundreds of them — the shape should not
 * have to change when the table grows. bbq_dict compares the whole name on every
 * hit, so a digest collision costs a hop and can never bind a guest to a host
 * function it did not name (see bbq_dict.h for what that mistake looked like in
 * sema). */
static bbq_dict* g_hio_index = NULL;

/* The index key is the PAIR, length-prefixed — NOT `mod + '.' + fld`.
 *
 * A wasm import name is arbitrary UTF-8 (§5.5.5), so no byte is safe as a separator: any
 * character a separator could use may appear inside a name. Flattening on '.' was injective
 * only by accident, because module names were bare Java simple names and a Java simple name
 * cannot contain a dot. §2e.1's move to fully-qualified module names removes that accident —
 * ("java.io.HostIO","fd_write") and ("java.io","HostIO.fd_write") are two distinct imports
 * that flatten to one key — so the key stops being a string. Prefixing the module's length
 * is injective for every pair of byte strings.
 *
 * This is the sema scope-table collision one layer up, and the reason bbq_dict stores and
 * memcmps the whole key rather than trusting a digest (bbq_dict.h). Pinned by
 * test_host_abi.c case 7. */
#define HIO_KEY_MAX 96

static int hio_key(char* buf, const wasm_name_t* mod, const wasm_name_t* fld) {
    size_t n = 2 + mod->size + fld->size;
    if (mod->size > 0xffff || n > HIO_KEY_MAX) return -1;
    buf[0] = (char)(mod->size & 0xff);
    buf[1] = (char)((mod->size >> 8) & 0xff);
    memcpy(buf + 2, mod->data, mod->size);
    memcpy(buf + 2 + mod->size, fld->data, fld->size);
    return (int)n;
}

static void hio_index_build(void) {
    if (g_hio_index) return;
    g_hio_index = bbq_dict_create();
    char q[HIO_KEY_MAX];
    for (size_t i = 0; i < sizeof hio_table / sizeof hio_table[0]; i++) {
        wasm_name_t m, f;                                   /* borrowed views of the row's literals */
        m.size = strlen(hio_table[i].mod); m.data = (byte_t*)(uintptr_t)hio_table[i].mod;
        f.size = strlen(hio_table[i].fld); f.data = (byte_t*)(uintptr_t)hio_table[i].fld;
        int n = hio_key(q, &m, &f);
        if (n > 0) bbq_dict_put(g_hio_index, q, (size_t)n, (void*)(uintptr_t)(i + 1));
    }
}

/* Release the index. One allocation for the process, not a growing leak, but an
 * embedder that tears its engine down has somewhere to put this. */
static inline void exec_host_release(void) {
    if (g_hio_index) { bbq_dict_destroy(g_hio_index); g_hio_index = NULL; }
}

/* Does `ft` — the type the GUEST declared — match the contract's `sig`? */
static int hio_sig_ok(const wasm_functype_t* ft, const char* sig) {
    if (!sig) return 1;                       /* the contract admits several shapes */
    const wasm_valtype_vec_t* p = wasm_functype_params(ft);
    const wasm_valtype_vec_t* r = wasm_functype_results(ft);
    const char* colon = strchr(sig, ':');
    size_t np = (size_t)(colon - sig), nr = strlen(colon + 1);
    if (p->size != np || r->size != nr) return 0;
    for (size_t i = 0; i < np + nr; i++) {
        char c = i < np ? sig[i] : colon[1 + (i - np)];
        wasm_valkind_t k = wasm_valtype_kind((i < np ? p : r)->data[i < np ? i : i - np]);
        int ok = c == 'i' ? k == WASM_I32 : c == 'I' ? k == WASM_I64
               : c == 'f' ? k == WASM_F32 : c == 'F' ? k == WASM_F64
               : wasm_valkind_is_ref(k);      /* 'r': any reference type */
        if (!ok) return 0;
    }
    return 1;
}

/* Resolve the embedder host function for an import by its QUALIFIED name — the way a real embedder
 * registers built-ins. This function IS the →HOST contract (docs/host-abi.md): every name it answers
 * is an environment edge the embedder owns. Anything else traps (hio_unimplemented).
 *
 * WITHHOLDING is the sandbox: an embedder that returns an unimplemented stub for `open`, or refuses
 * the import outright, denies the guest that capability — the module is then unlinkable or the call
 * traps, never silently succeeds. */
/* Bind one native to a §7.1.8 hostfunc closure carrying the context. Every row goes through
 * wasm_func_new_with_env: a callback registered without an env has nowhere to reach its state
 * from, which is the whole reason that state would otherwise have to be process-scoped. */
static wasm_func_t* hio_bind(jav_host_t* h, wasm_store_t* store, const wasm_functype_t* ft,
                             wasm_func_callback_with_env_t cb, int op, char* owned_name) {
    hio_bind_t* b = (hio_bind_t*)calloc(1, sizeof *b);
    b->h = h; b->store = store; b->op = op; b->name = owned_name;
    return wasm_func_new_with_env(store, ft, cb, b, hio_bind_free);
}

static wasm_func_t* exec_host_for(jav_host_t* h, wasm_store_t* store, const wasm_functype_t* ft,
                                  const wasm_name_t* mod, const wasm_name_t* fld) {
    char q[HIO_KEY_MAX];
    int qn = hio_key(q, mod, fld);
    if (qn > 0) {
        hio_index_build();
        size_t slot = (size_t)(uintptr_t)bbq_dict_get(g_hio_index, q, (size_t)qn);
        if (slot) {
            const hio_row_t* row = &hio_table[slot - 1];
            /* A declared type that is not the contract's falls through to the
             * fail-closed stub rather than binding — the WASI fd_write shape. */
            if (hio_sig_ok(ft, row->sig)) {
                if (row->op == HIO_MONITOR) return hio_bind(h, store, ft, hio_monitor_trap, 0, NULL);
                if (row->op != HIO_PLAIN)   return hio_bind(h, store, ft, hio_env, row->op, NULL);
                return hio_bind(h, store, ft, row->fn, 0, NULL);
            }
        }
    }

    /* Not ours: the application's hook gets the context AND the whole two-part name, so it can
     * own a namespace of its own rather than race this table for a bare field name. */
    if (h && h->host_extra) { wasm_func_t* f = h->host_extra(h, store, ft, mod, fld); if (f) return f; }

    char* owned = (char*)malloc(mod->size + 1 + fld->size + 1);
    memcpy(owned, mod->data, mod->size); owned[mod->size] = '.';
    memcpy(owned + mod->size + 1, fld->data, fld->size); owned[mod->size + 1 + fld->size] = 0;
    return hio_bind(h, store, ft, hio_unimplemented, 0, owned);
}

#endif /* JAVELINA_HOST_IO_H */
