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

/* ── The GC↔host bridge: the executing module's exported I/O staging memory, captured
 * at instantiation. Host I/O natives read/write the guest's bytes here via
 * wasm_memory_data. Set before each call. ── */
static wasm_memory_t* g_io_mem = NULL;

/* ── The filesystem root every guest path resolves under. NULL ⇒ a lazily-created
 * per-process temp sandbox (the test default); the runner sets it to a real directory. ── */
static const char* g_io_root = NULL;

/* ── The fd table: a small fd → FILE* map. fds 0/1/2 are preopened by the embedder
 * (test: capture temp files; runner: real std streams). ── */
#define IO_MAX_FDS 16
static FILE* g_io_fds[IO_MAX_FDS];

/* ── The system-property source (§20.18.7). Properties cross as BYTES, never as Strings: a
 * host function cannot construct a GC String (§7.1 — aggregates are opaque to the host), so
 * `java.lang.System` asks for a value through the staging memory and builds the String itself.
 * A NULL table (or a key not in it) is an ABSENT property, not an error. ── */
typedef struct { const char* key; const char* val; } hio_prop_t;
static const hio_prop_t* g_io_props = NULL;   /* terminated by a NULL key */

/* ── The environment sources the embedder owns. NULL ⇒ the reproducible defaults below, which is
 * what the test harness wants; the runner installs the real clock/PRNG/exit. ── */
static int64_t (*g_io_clock)(void)  = NULL;   /* epoch milliseconds */

/* ── Application natives: an embedder may answer imports this contract does not name (the
 * host_plugin shape — an app exposing its own functions to the guest). Returning NULL falls
 * through to the fail-closed stub, so registering a hook never re-opens the silent-echo hole. ── */
static wasm_func_t* (*g_io_host_extra)(wasm_store_t*, const wasm_functype_t*,
                                       const wasm_name_t* mod, const wasm_name_t* fld) = NULL;

static byte_t* io_membytes(void) { return g_io_mem ? wasm_memory_data(g_io_mem) : NULL; }
static size_t  io_memsize(void)  { return g_io_mem ? wasm_memory_data_size(g_io_mem) : 0; }
static int     io_fd_ok(int fd)  { return fd >= 0 && fd < IO_MAX_FDS && g_io_fds[fd]; }
/* A staging-memory span the host may touch: [off, off+len) inside the memory. */
static int     io_span_ok(int off, int len) {
    size_t n = io_memsize();
    return off >= 0 && len >= 0 && (size_t)off <= n && (size_t)len <= n - (size_t)off;
}

/* Map a guest path (staging bytes at off,len) to a host path under g_io_root,
 * PRESERVING '/' so directory nesting + File.list work. Rejects any ".." (no parent
 * escape). If g_io_root is NULL, a fresh per-run temp sandbox is created. */
static int io_hostpath(int off, int len, char* out) {
    byte_t* m = io_membytes();
    if (!m || len < 0 || len >= 400) return 0;
    static char sandbox[64] = "";
    const char* root = g_io_root;
    if (!root) {
        if (!sandbox[0]) { snprintf(sandbox, sizeof sandbox, "/tmp/javio_%d", (int)getpid()); mkdir(sandbox, 0777); }
        root = sandbox;
    }
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
static wasm_trap_t* hio_f2i(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    float f = a->data[0].of.f32; int32_t i; memcpy(&i, &f, 4);
    r->data[0] = (wasm_val_t)WASM_I32_VAL(i); return NULL;
}
static wasm_trap_t* hio_i2f(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int32_t i = a->data[0].of.i32; float f; memcpy(&f, &i, 4);
    r->data[0] = (wasm_val_t)WASM_F32_VAL(f); return NULL;
}
static wasm_trap_t* hio_d2l(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    double d = a->data[0].of.f64; int64_t l; memcpy(&l, &d, 8);
    r->data[0] = (wasm_val_t)WASM_I64_VAL(l); return NULL;
}
static wasm_trap_t* hio_l2d(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int64_t l = a->data[0].of.i64; double d; memcpy(&d, &l, 8);
    r->data[0] = (wasm_val_t)WASM_F64_VAL(d); return NULL;
}

/* The clock/exit/identity-hash source — the genuine environment floor. One with-env callback
 * dispatches on an op tag. Deterministic here (reproducible); the runner wires the real clock.
 * (Math.random is NOT here: it is java.util.Random over this clock, §20.11.20.) */
typedef enum { HOP_CTM, HOP_EXIT, HOP_IDHASH } host_op;
static int64_t  hio_ctm_ticks = 0;
static int32_t  hio_next_id    = 1;

/* Set by the embedder: HOP_EXIT terminates the process with the guest's code when
 * non-NULL (the runner); NULL ⇒ the test no-op. */
static void (*g_io_exit)(int) = NULL;

static wasm_trap_t* hio_env(void* env, const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    switch ((host_op)(intptr_t)env) {
    case HOP_CTM:    r->data[0] = (wasm_val_t)WASM_I64_VAL(g_io_clock ? g_io_clock() : ++hio_ctm_ticks); break;
    case HOP_IDHASH: r->data[0] = (wasm_val_t)WASM_I32_VAL(hio_next_id++);   break;
    case HOP_EXIT:   if (g_io_exit) g_io_exit(a->size > 0 ? a->data[0].of.i32 : 0); break;
    }
    return NULL;
}

/* ── §20.18.7 system properties, over the staging memory (see hio_prop_t).
 * getprop(keyoff, keylen, outoff) → value length written at outoff, or -1 if the key is absent.
 * propnames(outoff)              → total bytes of the NUL-separated key list written at outoff. ── */
static wasm_trap_t* hio_getprop(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int koff = a->data[0].of.i32, klen = a->data[1].of.i32, ooff = a->data[2].of.i32;
    byte_t* m = io_membytes();
    int n = -1;
    if (m && io_span_ok(koff, klen)) {
        for (const hio_prop_t* p = g_io_props; p && p->key; p++) {
            size_t kl = strlen(p->key);
            if (kl != (size_t)klen || memcmp(m + koff, p->key, kl)) continue;
            size_t vl = strlen(p->val);
            if (!io_span_ok(ooff, (int)vl)) break;      /* no room: report absent rather than corrupt memory */
            memcpy(m + ooff, p->val, vl);
            n = (int)vl;
            break;
        }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n); return NULL;
}
static wasm_trap_t* hio_propnames(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int ooff = a->data[0].of.i32, total = 0;
    byte_t* m = io_membytes();
    if (m) for (const hio_prop_t* p = g_io_props; p && p->key; p++) {
        size_t kl = strlen(p->key);
        if (!io_span_ok(ooff + total, (int)kl + 1)) { total = -1; break; }
        memcpy(m + ooff + total, p->key, kl); total += (int)kl;
        m[ooff + total++] = 0;
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(total); return NULL;
}

/* Object monitor ops on a threadless target: no monitor exists → trap (§17 is meaningless here). */
static wasm_trap_t* hio_monitor_trap(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; (void)results;
    wasm_message_t msg; wasm_name_new_from_string_nt(&msg, "monitor op on threadless target");
    wasm_trap_t* t = wasm_trap_new((wasm_store_t*)env, &msg);
    wasm_byte_vec_delete(&msg);
    return t;
}

/* Test-only: sum `len` staging bytes at `off` — proves the host can read the guest's memory. */
static wasm_trap_t* hio_checksum(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int off = a->size > 0 ? a->data[0].of.i32 : 0;
    int len = a->size > 1 ? a->data[1].of.i32 : 0;
    int sum = 0;
    if (g_io_mem) { byte_t* m = wasm_memory_data(g_io_mem);
                    for (int i = 0; i < len; i++) sum += (unsigned char)m[off + i]; }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(sum); return NULL;
}

static wasm_trap_t* hio_fd_open_temp(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)a; int fd = -1;
    for (int i = 0; i < IO_MAX_FDS; i++) if (!g_io_fds[i]) { g_io_fds[i] = tmpfile(); if (g_io_fds[i]) fd = i; break; }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(fd); return NULL;
}
static wasm_trap_t* hio_open(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int flags = a->data[2].of.i32, fd = -1;
    char path[512];
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path)) {
        FILE* fp;
        if (flags == 2) { fp = fopen(path, "r+b"); if (!fp) fp = fopen(path, "w+b"); }  /* read+write: open else create */
        else fp = fopen(path, (flags & 1) ? "wb" : "rb");                                /* 1 = write/truncate, 0 = read */
        if (fp) for (int i = 0; i < IO_MAX_FDS; i++) if (!g_io_fds[i]) { g_io_fds[i] = fp; fd = i; break; }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(fd); return NULL;
}
static wasm_trap_t* hio_fd_write(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int fd = a->data[0].of.i32, off = a->data[1].of.i32, len = a->data[2].of.i32, n = 0;
    byte_t* m = io_membytes();
    if (m && io_fd_ok(fd)) { n = (int)fwrite(m + off, 1, (size_t)len, g_io_fds[fd]);
                             if (fd == 1 || fd == 2) fflush(g_io_fds[fd]); }   /* flush std streams promptly */
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n); return NULL;
}
static wasm_trap_t* hio_fd_read(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int fd = a->data[0].of.i32, off = a->data[1].of.i32, len = a->data[2].of.i32, n = 0;
    byte_t* m = io_membytes();
    if (m && io_fd_ok(fd)) n = (int)fread(m + off, 1, (size_t)len, g_io_fds[fd]);
    r->data[0] = (wasm_val_t)WASM_I32_VAL(n > 0 ? n : -1);   /* -1 = EOF (InputStream.read) */
    return NULL;
}
static wasm_trap_t* hio_fd_seek(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)r; int fd = a->data[0].of.i32, pos = a->data[1].of.i32;
    if (io_fd_ok(fd)) fseek(g_io_fds[fd], pos, SEEK_SET);
    return NULL;
}
static wasm_trap_t* hio_fd_close(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)r; int fd = a->data[0].of.i32;
    if (io_fd_ok(fd) && fd > 2) { fclose(g_io_fds[fd]); g_io_fds[fd] = NULL; }   /* never close std streams */
    return NULL;
}
static wasm_trap_t* hio_fd_size(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    int fd = a->data[0].of.i32; long long sz = -1;
    if (io_fd_ok(fd)) {
        FILE* fp = g_io_fds[fd];
        fflush(fp);
        long cur = ftell(fp);
        if (fseek(fp, 0, SEEK_END) == 0) { long end = ftell(fp); if (end >= 0) sz = (long long)end; }
        if (cur >= 0) fseek(fp, cur, SEEK_SET);
    }
    r->data[0] = (wasm_val_t)WASM_I64_VAL(sz); return NULL;
}

/* ── §22.4 File stat/action floor. ── */
static wasm_trap_t* hio_stat(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; int flags = 0; struct stat st;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0) {
        flags |= 1;                                        /* exists */
        if (S_ISDIR(st.st_mode)) flags |= 2;               /* dir    */
        if (S_ISREG(st.st_mode)) flags |= 4;               /* file   */
        if (access(path, R_OK) == 0) flags |= 8;           /* canRead */
        if (access(path, W_OK) == 0) flags |= 16;          /* canWrite*/
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(flags); return NULL;
}
static wasm_trap_t* hio_file_size(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; long long sz = -1; struct stat st;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0 && S_ISREG(st.st_mode))
        sz = (long long)st.st_size;
    r->data[0] = (wasm_val_t)WASM_I64_VAL(sz); return NULL;
}
static wasm_trap_t* hio_file_modified(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; long long ms = 0; struct stat st;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path) && stat(path, &st) == 0)
        ms = (long long)st.st_mtime * 1000;
    r->data[0] = (wasm_val_t)WASM_I64_VAL(ms); return NULL;
}
static wasm_trap_t* hio_unlink(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; int rc = -1;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path)) rc = (remove(path) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
static wasm_trap_t* hio_mkdir(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; int rc = -1;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path)) rc = (mkdir(path, 0777) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
static wasm_trap_t* hio_rename(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char from[512], to[512]; int rc = -1;
    if (io_hostpath(a->data[0].of.i32, a->data[1].of.i32, from) &&
        io_hostpath(a->data[2].of.i32, a->data[3].of.i32, to)) rc = (rename(from, to) == 0) ? 0 : -1;
    r->data[0] = (wasm_val_t)WASM_I32_VAL(rc); return NULL;
}
static wasm_trap_t* hio_list(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    char path[512]; int total = -1; byte_t* m = io_membytes();
    int outoff = a->data[2].of.i32;
    if (m && io_hostpath(a->data[0].of.i32, a->data[1].of.i32, path)) {
        DIR* dp = opendir(path);
        if (dp) {
            total = 0; struct dirent* de;
            while ((de = readdir(dp)) != NULL) {
                const char* nm = de->d_name;
                if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0))) continue;   /* skip . and .. */
                for (int i = 0; nm[i]; i++) m[outoff + total++] = (byte_t)nm[i];
                m[outoff + total++] = 0;                    /* NUL separator */
            }
            closedir(dp);
        }
    }
    r->data[0] = (wasm_val_t)WASM_I32_VAL(total); return NULL;
}

/* A native the embedder does not implement. Calling it TRAPS, naming the method — the engine's
 * fail-closed answer to an unsatisfied environment edge. It is never an echo or a zero: a silently
 * wrong native is indistinguishable from a working one, and every native this floor does not name
 * is, by definition, either compiler-lowered (never called) or not yet implemented. `env` is the
 * method name, owned here and freed with the func. */
static wasm_trap_t* hio_unimplemented(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    (void)args; (void)results;
    char buf[256];
    snprintf(buf, sizeof buf, "unimplemented native '%s' (not part of the javelina host contract)", (const char*)env);
    wasm_message_t msg; wasm_name_new_from_string_nt(&msg, buf);
    wasm_trap_t* t = wasm_trap_new(NULL, &msg);
    wasm_byte_vec_delete(&msg);
    return t;
}

/* §20.18 no-op natives: methods whose contract is satisfied by doing nothing on this target.
 * `gc`/`runFinalization` are advisory (the collector is not on-demand); `load`/`loadLibrary` have
 * no dynamic-linking surface; `finalize` is Object's empty body; `setSecurityManager`/`setProperties`
 * accept and discard (there is no security manager — getSecurityManager answers null, §20.18.4). */
static wasm_trap_t* hio_noop(const wasm_val_vec_t* a, wasm_val_vec_t* r) { (void)a; (void)r; return NULL; }
static wasm_trap_t* hio_null_ref(const wasm_val_vec_t* a, wasm_val_vec_t* r) {
    (void)a; r->data[0] = (wasm_val_t)WASM_INIT_VAL; return NULL;   /* a null externref */
}

/* ── the →HOST table ─────────────────────────────────────────────────────────
 * One row per import in docs/host-abi.md, keyed on the QUALIFIED name and
 * carrying the contract's functype. Both halves are load-bearing:
 *
 * THE MODULE NAME IS PART OF THE KEY. A WASM import is a two-part name
 * (§5.5.5) and jre.wasm declares both — `HostIO.open`, `System.exit`,
 * `F32x4.ceil`. Matching on the field alone let any module claim any native
 * (`Whatever.open` reached the real filesystem open) and made the
 * g_io_host_extra hook unable to own a namespace: consulted last and handed
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
                 wasm_func_callback_t fn; int op; } hio_row_t;

static const hio_row_t hio_table[] = {
    /* §17 monitors on a threadless target: no monitor exists. */
    { "Object", "wait",      NULL,     NULL, HIO_MONITOR },
    { "Object", "notify",    NULL,     NULL, HIO_MONITOR },
    { "Object", "notifyAll", NULL,     NULL, HIO_MONITOR },

    /* The fd + filesystem floor. */
    { "HostIO", "checksum",     "ii:i",   hio_checksum,      HIO_PLAIN },
    { "HostIO", "fd_open_temp", ":i",     hio_fd_open_temp,  HIO_PLAIN },
    { "HostIO", "open",         "iii:i",  hio_open,          HIO_PLAIN },
    { "HostIO", "fd_write",     "iii:i",  hio_fd_write,      HIO_PLAIN },
    { "HostIO", "fd_read",      "iii:i",  hio_fd_read,       HIO_PLAIN },
    { "HostIO", "fd_seek",      "ii:",    hio_fd_seek,       HIO_PLAIN },
    { "HostIO", "fd_close",     "i:",     hio_fd_close,      HIO_PLAIN },
    { "HostIO", "fd_size",      "i:I",    hio_fd_size,       HIO_PLAIN },
    { "HostIO", "stat",         "ii:i",   hio_stat,          HIO_PLAIN },
    { "HostIO", "fileSize",     "ii:I",   hio_file_size,     HIO_PLAIN },
    { "HostIO", "fileModified", "ii:I",   hio_file_modified, HIO_PLAIN },
    { "HostIO", "unlink",       "ii:i",   hio_unlink,        HIO_PLAIN },
    { "HostIO", "mkdir",        "ii:i",   hio_mkdir,         HIO_PLAIN },
    { "HostIO", "rename",       "iiii:i", hio_rename,        HIO_PLAIN },
    { "HostIO", "list",         "iii:i",  hio_list,          HIO_PLAIN },

    /* The system-property source (bytes through the staging memory). */
    { "HostIO", "getprop",      "iii:i",  hio_getprop,       HIO_PLAIN },
    { "HostIO", "propnames",    "i:i",    hio_propnames,     HIO_PLAIN },

    /* IEEE bit reinterprets: no WASM-GC primitive reinterprets a float's bits. */
    { "Float",  "floatToIntBits",       "f:i", hio_f2i, HIO_PLAIN },
    { "Float",  "floatToRawIntBits",    "f:i", hio_f2i, HIO_PLAIN },
    { "Float",  "intBitsToFloat",       "i:f", hio_i2f, HIO_PLAIN },
    { "Double", "doubleToLongBits",     "F:I", hio_d2l, HIO_PLAIN },
    { "Double", "doubleToRawLongBits",  "F:I", hio_d2l, HIO_PLAIN },
    { "Double", "longBitsToDouble",     "I:F", hio_l2d, HIO_PLAIN },

    /* §20.18 no-ops + the absent security manager. */
    { "System", "gc",                 ":",   hio_noop,     HIO_PLAIN },
    { "System", "runFinalization",    ":",   hio_noop,     HIO_PLAIN },
    { "System", "load",               "r:",  hio_noop,     HIO_PLAIN },
    { "System", "loadLibrary",        "r:",  hio_noop,     HIO_PLAIN },
    { "System", "setSecurityManager", "r:",  hio_noop,     HIO_PLAIN },
    { "System", "setProperties",      "r:",  hio_noop,     HIO_PLAIN },
    { "Object", "finalize",           "r:",  hio_noop,     HIO_PLAIN },
    { "System", "getSecurityManager", ":r",  hio_null_ref, HIO_PLAIN },

    /* The clock / exit / identity-hash sources. */
    { "System", "currentTimeMillis", ":I", NULL, HOP_CTM },
    { "System", "exit",              "i:", NULL, HOP_EXIT },
    { "System", "identityHashCode",  ":i", NULL, HOP_IDHASH },
};

/* The resolution index: qualified name -> row + 1, built from hio_table on first
 * use. A dict rather than a scan of the rows because the rows are a CONTRACT, and
 * an embedder with a real API surface has hundreds of them — the shape should not
 * have to change when the table grows. bbq_dict compares the whole name on every
 * hit, so a digest collision costs a hop and can never bind a guest to a host
 * function it did not name (see bbq_dict.h for what that mistake looked like in
 * sema). */
static bbq_dict* g_hio_index = NULL;

/* Longest row is "HostIO.fileModified" — 19. A guest may declare names of any
 * length, and one longer than this matches no row, so it needs no buffer. */
#define HIO_QNAME_MAX 64

static int hio_qname(char* buf, const wasm_name_t* mod, const wasm_name_t* fld) {
    size_t n = mod->size + 1 + fld->size;
    if (n > HIO_QNAME_MAX) return -1;
    memcpy(buf, mod->data, mod->size);
    buf[mod->size] = '.';
    memcpy(buf + mod->size + 1, fld->data, fld->size);
    return (int)n;
}

static void hio_index_build(void) {
    if (g_hio_index) return;
    g_hio_index = bbq_dict_create();
    char q[HIO_QNAME_MAX];
    for (size_t i = 0; i < sizeof hio_table / sizeof hio_table[0]; i++) {
        int n = snprintf(q, sizeof q, "%s.%s", hio_table[i].mod, hio_table[i].fld);
        if (n > 0 && n <= (int)sizeof q)
            bbq_dict_put(g_hio_index, q, (size_t)n, (void*)(uintptr_t)(i + 1));
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
static wasm_func_t* exec_host_for(wasm_store_t* store, const wasm_functype_t* ft,
                                  const wasm_name_t* mod, const wasm_name_t* fld) {
    char q[HIO_QNAME_MAX];
    int qn = hio_qname(q, mod, fld);
    if (qn > 0) {
        hio_index_build();
        size_t slot = (size_t)(uintptr_t)bbq_dict_get(g_hio_index, q, (size_t)qn);
        if (slot) {
            const hio_row_t* row = &hio_table[slot - 1];
            /* A declared type that is not the contract's falls through to the
             * fail-closed stub rather than binding — the WASI fd_write shape. */
            if (hio_sig_ok(ft, row->sig)) {
                if (row->op == HIO_MONITOR)
                    return wasm_func_new_with_env(store, ft, hio_monitor_trap, store, NULL);
                if (row->op != HIO_PLAIN)
                    return wasm_func_new_with_env(store, ft, hio_env, (void*)(intptr_t)row->op, NULL);
                return wasm_func_new(store, ft, row->fn);
            }
        }
    }

    /* Not ours: the application's hook gets the whole two-part name, so it can own a namespace
     * of its own rather than race this table for a bare field name. */
    if (g_io_host_extra) { wasm_func_t* f = g_io_host_extra(store, ft, mod, fld); if (f) return f; }

    char* owned = (char*)malloc(mod->size + 1 + fld->size + 1);
    memcpy(owned, mod->data, mod->size); owned[mod->size] = '.';
    memcpy(owned + mod->size + 1, fld->data, fld->size); owned[mod->size + 1 + fld->size] = 0;
    return wasm_func_new_with_env(store, ft, hio_unimplemented, owned, free);
}

#endif /* JAVELINA_HOST_IO_H */
