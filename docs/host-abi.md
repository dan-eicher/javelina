# The javelina host ABI (the →HOST native contract)

A compiled Java program is a WebAssembly module. Everything it can do that is not pure computation —
read a file, print a line, ask the time, stop the process — it must ask the **host** for, through a
WebAssembly *import*. This document is the complete list of those imports: the environment edges an
embedder must supply for a javelina program to run.

It is WASI-shaped but **not WASI**: the names, the numbering, and the error conventions are
javelina's own, chosen to sit directly under the `java.*` library rather than under libc.

The reference implementation of every entry below is `compiler/driver/host_io.h`, shared verbatim by
the shipped runner (`javelina`) and the test harness. The two differ only in *policy* — which files
fds 0/1/2 name, whether the clock is real — never in the contract.

## How values cross

WebAssembly GC aggregates are **opaque to the host** (WASM 3.0 §7.1): a host function may receive a
`String` or an `int[]` as an `externref` handle, but it cannot read its characters, and it cannot
construct one to give back. Every native below therefore trades in **scalars and linear memory**.

The module exports one linear memory, `memory`, used as a bounce buffer. When the guest wants to
hand the host bytes (a path, a property key, the contents of a `byte[]`), it copies them into that
memory and passes an `(offset, length)` pair. When the host wants to hand the guest bytes, it writes
them at an offset the guest chose and returns the length. `java.lang.System` and `java.io.*` own the
copying; the host only ever touches `wasm_memory_data(memory)` within the span it was given, and a
span that does not lie inside the memory is refused rather than clamped.

This is why, for example, `System.getProperty` is **not** a native. It cannot be: a host function
cannot build the `String` it would have to return. It is Java code over the `HostIO.getprop` native
below.

## Fail-closed

`exec_host_for` resolves an import by its **qualified** name — the module and the field, both halves,
as WebAssembly declares them (§5.5.5). The tables below are the key: `HostIO.open` is one import and
`Whatever.open` is a different one that this floor does not answer. **A name it does not know gets a
stub that traps**, naming it — never a zero, never an echo of its argument. A silently wrong native is
indistinguishable from a working one; a trap is not.

**The declared signature is checked, not adopted.** `wasm_func_new` is handed the functype the *guest*
declared, so a disagreement would otherwise link and go wrong at call time — WASI preview1's
`fd_write` is `(fd, iovs, iovs_len, nwritten) → errno`, four arguments, and would have bound straight
onto this floor's three-argument `HostIO.fd_write` and read an iovec pointer as a buffer offset. An
import whose declared type is not the one tabulated below falls through to the trapping stub.

This is also the security model. **Withholding a capability is how you sandbox.** An embedder that
declines to supply `open` (or supplies a trapping stub for it) has denied the guest the filesystem:
the module either fails to link, or the call traps. It cannot silently succeed. A restricted profile
is a smaller `exec_host_for`, not a patched `jre.wasm`.

An embedder may add natives of its own — an application exposing its functions to the guest — through
the `g_io_host_extra` hook:

```c
wasm_func_t* (*g_io_host_extra)(wasm_store_t*, const wasm_functype_t*,
                                const wasm_name_t* mod, const wasm_name_t* fld);
```

It receives **both** halves of the name, so an application owns a module namespace of its own —
answer for `App.*` and return `NULL` for everything else. It is consulted for every import this
contract does not claim, so an application native never has to contend with a java.lang method that
happens to share its field name: `App.open` and `HostIO.open` are different imports and both resolve.
Returning `NULL` falls through to the trapping stub, so registering application natives never reopens
the hole.

A module name is *not* a capability boundary by itself — withholding is still the sandbox. What the
qualified key buys is that a guest cannot reach a native by asking for its field name under some other
module, and that an application's namespace is its own.

---

## The contract

Import names are `Class.method`, matching the `native` declaration in the library source. `i32`/`i64`
are the WebAssembly scalar types; `externref` is an opaque GC handle.

### `HostIO` — the file-descriptor and filesystem floor

Offsets and lengths index the staging memory. Paths are raw bytes, resolved under the embedder's
root; a path containing `..` is refused.

| Import | Functype | Semantics |
| --- | --- | --- |
| `HostIO.open` | `(i32 nameoff, i32 namelen, i32 flags) → i32` | Open the path → fd, or `-1`. `flags`: 0 read, 1 write/truncate, 2 read+write (create, no truncate). |
| `HostIO.fd_read` | `(i32 fd, i32 off, i32 len) → i32` | Read ≤ `len` bytes into memory at `off`. Returns the count, or `-1` at end of file (`InputStream.read`'s convention). |
| `HostIO.fd_write` | `(i32 fd, i32 off, i32 len) → i32` | Write `len` bytes from memory at `off`. Returns the count written. |
| `HostIO.fd_seek` | `(i32 fd, i32 pos) → ()` | Seek to absolute `pos`. |
| `HostIO.fd_close` | `(i32 fd) → ()` | Close. fds 0/1/2 are never closed. |
| `HostIO.fd_size` | `(i32 fd) → i64` | Current length in bytes (flushes buffered writes), or `-1`. Backs `RandomAccessFile.length`. |
| `HostIO.fd_open_temp` | `() → i32` | Open a fresh anonymous read+write temp file → fd, or `-1`. |
| `HostIO.stat` | `(i32 nameoff, i32 namelen) → i32` | A flags word, `0` if the path does not exist: `1` exists, `2` directory, `4` regular file, `8` readable, `16` writable. One call answers all of `File.exists/isDirectory/isFile/canRead/canWrite`. |
| `HostIO.fileSize` | `(i32 nameoff, i32 namelen) → i64` | Length of a regular file, else `-1`. |
| `HostIO.fileModified` | `(i32 nameoff, i32 namelen) → i64` | Last-modified time, epoch milliseconds; `0` if unknown. |
| `HostIO.unlink` | `(i32 nameoff, i32 namelen) → i32` | Delete a file or empty directory. `0` ok, `-1` otherwise. |
| `HostIO.mkdir` | `(i32 nameoff, i32 namelen) → i32` | Create a directory. `0` ok, `-1` otherwise. |
| `HostIO.rename` | `(i32 fromoff, i32 fromlen, i32 tooff, i32 tolen) → i32` | Rename. `0` ok, `-1` otherwise. |
| `HostIO.list` | `(i32 nameoff, i32 namelen, i32 outoff) → i32` | Write the directory's entries NUL-separated at `outoff`; return the total byte count, or `-1` if not a directory. `.` and `..` are omitted. |
| `HostIO.checksum` | `(i32 off, i32 len) → i32` | Sum of `len` staging bytes at `off`. A probe: it exists so a test can prove the host observes the guest's memory. |

fds 0, 1 and 2 are pre-opened by the embedder and are `System.in`, `System.out`, `System.err`. The
runner maps them to the process's real standard streams; the test harness maps them to capture files.

### `HostIO` — the system-property source (§20.18.7)

| Import | Functype | Semantics |
| --- | --- | --- |
| `HostIO.getprop` | `(i32 keyoff, i32 keylen, i32 outoff) → i32` | Look up the property named by the key bytes. On a hit, write the value bytes at `outoff` and return their length; on a miss (or no room at `outoff`), return `-1`. |
| `HostIO.propnames` | `(i32 outoff) → i32` | Write every property name, NUL-separated, at `outoff`; return the total byte count. Backs `System.getProperties`. |

An absent property is **not an error** — it is how an embedder withholds one. `System.getProperty`
answers `null`, exactly as the specification requires for an undefined property.

The runner publishes `java.version`, `java.vendor`, `java.vendor.url`, `java.class.version`,
`os.name`, `os.arch`, `os.version`, `file.separator`, `path.separator`, `line.separator`, `user.dir`.

### `System` — the process environment

| Import | Functype | Semantics |
| --- | --- | --- |
| `System.currentTimeMillis` | `() → i64` | Milliseconds since the epoch. The runner reads `CLOCK_REALTIME`; the harness returns a deterministic counter, so tests are reproducible. |
| `System.exit` | `(i32 status) → ()` | Terminate the process with `status`. **Does not return.** The runner flushes the standard streams first. An embedder that leaves this a no-op lets the guest run on past `System.exit`, which the specification forbids — so a real embedder must implement it. |
| `System.gc` | `() → ()` | Advisory; the collector is not on-demand. No-op. |
| `System.runFinalization` | `() → ()` | No-op — there are no finalizers to run (see `Object.finalize`). |
| `System.load` | `(externref) → ()` | No-op. There is no dynamic-linking surface. |
| `System.loadLibrary` | `(externref) → ()` | No-op, as above. |
| `System.getSecurityManager` | `() → externref` | `null`. There is no security manager; capability restriction is done by withholding imports (above), not by an in-guest policy object. |
| `System.setSecurityManager` | `(externref) → ()` | Accepts and discards, consistent with `getSecurityManager` answering `null`. |
| `System.setProperties` | `(externref) → ()` | Accepts and discards; the property set is the host's. |

### `Float` / `Double` — IEEE-754 bit reinterpretation

No WebAssembly-GC primitive reinterprets a float's bits as an integer, so the embedder supplies the
four casts. They are pure: the same input always gives the same output.

| Import | Functype |
| --- | --- |
| `Float.floatToIntBits`, `Float.floatToRawIntBits` | `(f32) → i32` |
| `Float.intBitsToFloat` | `(i32) → f32` |
| `Double.doubleToLongBits`, `Double.doubleToRawLongBits` | `(f64) → i64` |
| `Double.longBitsToDouble` | `(i64) → f64` |

### `Object` — the threadless exclusions

Java 1.0 minus `synchronized` has no threads, so §17's monitors have no meaning on this target.

| Import | Functype | Semantics |
| --- | --- | --- |
| `Object.wait` (3 overloads), `Object.notify`, `Object.notifyAll` | `(externref, …) → ()` | **Trap.** There is no monitor to wait on or notify. This is a deliberate, documented exclusion, not an omission. |
| `Object.finalize` | `(externref) → ()` | No-op — `Object.finalize`'s own body is empty, and the collector does not resurrect. |

### Reserved

`identityHashCode : () → i32` is resolved by the floor but is not currently imported: `java.lang.Object`
implements `hashCode()` with an allocation-ordered counter of its own. It remains in the contract for
an embedder that wants to supply a real identity-hash source.

---

## Imports that are *not* host calls

Several library methods are declared `native` in the Java source yet never reach the host: the
compiler recognises them by identity and emits WebAssembly instructions in their place. Their imports
appear in `jre.wasm`'s import section — sema emits an import for every `native` declaration — but no
call site ever targets them, so the host's trapping stub for them is never entered.

| Method | Lowered to |
| --- | --- |
| `Math.sqrt`, `Math.ceil`, `Math.floor`, `Math.rint` | `f64.sqrt` / `f64.ceil` / `f64.floor` / `f64.nearest` |
| `Mem.load8`, `Mem.store8` | `i32.load8_u` / `i32.store8` |
| `System.arraycopy` | `array.copy` |
| `Object.getClass` | a read of the class's `Class` singleton |

Others were natives once and are now ordinary Java, written over the contract above:
`System.getProperty`/`getProperties` (over `HostIO.getprop`/`propnames`), `Boolean.getBoolean`,
`Integer.getInteger`, `Long.getLong` (over `System.getProperty`), and `Math.random`
(§20.11.20: a single `java.util.Random`, created on first call, seeded from
`System.currentTimeMillis`). The host supplies no PRNG.

That these are lowered rather than faked is *checked*, not assumed: since the floor traps on any
native it does not name, a call that leaked through to the host would fail the test suite rather than
quietly return its own argument.
