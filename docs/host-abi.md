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
copying; the host only ever touches `wasm_memory_data(memory)` within the span it was given.

Two rules govern that span, and both are the **embedder's obligation**, not this implementation's
convenience. A span is a pointer the *guest* chose. A compiled Java program chooses cooperatively; a
third-party plugin does not, and the floor cannot tell them apart.

**Spans are refused, not clamped.** A span that does not lie wholly inside the memory is refused —
the native answers its documented refusal value and touches nothing. Never trimmed to fit, never
partially serviced. The check is `io_span_ok` in `host_io.h`, and it is written to be overflow-free
(`len <= size - off`, never `off + len <= size`); it re-reads `wasm_memory_data` on every call, so a
`memory.grow` between calls cannot leave it holding a stale base. Every native that dereferences the
staging pointer runs it first. `test_host_memory.c` calls each of them with hostile spans and is the
gate.

**A variable-length answer is three-way.** A native that writes an answer whose length the guest
cannot know in advance — `list`, `getprop`, `propnames` — reports the length its answer **needs**,
and writes only when the whole answer fits. So the guest reads the result as:

| result | meaning |
| --- | --- |
| `-1` | **absent** — no such property, not a directory. Nothing else ever answers `-1`. |
| `n` ≤ the room offered | the answer, `n` bytes, written |
| `n` > the room offered | nothing was written; the answer needs `n` bytes — grow and ask again |

`java.io.HostIO.ensureRoom(off, need)` is the retry, and the only place that arithmetic lives.
Collapsing the third case into `-1` is what this convention replaced: a property value or a
directory listing larger than the staging memory came back as *absent*, so `System.getProperty`
answered `null` — which §20.18.7 reserves for a property that is not defined — and `File.list()`
answered `null`, which §22.4 reserves for a path that is not a directory. Both are wrong answers a
caller cannot detect, not degradations it can.

This is why, for example, `System.getProperty` is **not** a native. It cannot be: a host function
cannot build the `String` it would have to return. It is Java code over the `HostIO.getprop` native
below.

## Fail-closed

`exec_host_for` resolves an import by **both halves** of its name, as WebAssembly declares them
(§5.5.5). The tables below are the key: module `java.io.HostIO` field `open` is one import, and
module `Whatever` field `open` is a different one that this floor does not answer. **A name it does not know gets a
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

**An import is a PAIR, and this document never joins it.** WebAssembly names an import by two
strings (§5.5.5) — a module and a field — and the module half is the declaring class's
**fully-qualified** name: `java.io.HostIO`, not `HostIO`. Each section below names the module once;
the tables name the field. They are written apart on purpose. A wasm name is arbitrary UTF-8, so
there is no character that can safely join them: `("java.io.HostIO", "fd_write")` and
`("java.io", "HostIO.fd_write")` are two different imports, and any spelling that renders both as
`java.io.HostIO.fd_write` has thrown away the thing that tells them apart. The resolver keys on the
pair for the same reason (`hio_key` in `host_io.h`).

The module name is fully qualified because **the ABI is primary**: it fixes literal `(module, field)`
strings, and every binding must be able to *emit* them. Java is the awkward one, because it derives
the string from a language construct — and a Java simple name cannot contain a dot, so a scheme built
on simple names cannot express a dotted module string at all. `fq_name` yields dot-joined identifier
segments, which is the intersection of what Java, C and Zig can each spell.

`i32`/`i64` are the WebAssembly scalar types; `externref` is an opaque GC handle.

### module `java.io.HostIO` — the file-descriptor and filesystem floor

Offsets and lengths index the staging memory. Paths are raw bytes, resolved under the embedder's
root; a path containing `..` is refused.

| Field | Functype | Semantics |
| --- | --- | --- |
| `open` | `(i32 nameoff, i32 namelen, i32 flags) → i32` | Open the path → fd, or `-1`. `flags`: 0 read, 1 write/truncate, 2 read+write (create, no truncate). |
| `fd_read` | `(i32 fd, i32 off, i32 len) → i32` | Read ≤ `len` bytes into memory at `off`. Returns the count, or `-1` at end of file (`InputStream.read`'s convention) — and `-1` for a refused span or an unusable fd, which the caller sees as end of file. |
| `fd_write` | `(i32 fd, i32 off, i32 len) → i32` | Write `len` bytes from memory at `off`. Returns the count written, or `-1` for a refused span or an unusable fd. |
| `fd_seek` | `(i32 fd, i32 pos) → ()` | Seek to absolute `pos`. |
| `fd_close` | `(i32 fd) → ()` | Close. fds 0/1/2 are never closed. |
| `fd_size` | `(i32 fd) → i64` | Current length in bytes (flushes buffered writes), or `-1`. Backs `RandomAccessFile.length`. |
| `fd_open_temp` | `() → i32` | Open a fresh anonymous read+write temp file → fd, or `-1`. |
| `stat` | `(i32 nameoff, i32 namelen) → i32` | A flags word, `0` if the path does not exist: `1` exists, `2` directory, `4` regular file, `8` readable, `16` writable. One call answers all of `File.exists/isDirectory/isFile/canRead/canWrite`. |
| `fileSize` | `(i32 nameoff, i32 namelen) → i64` | Length of a regular file, else `-1`. |
| `fileModified` | `(i32 nameoff, i32 namelen) → i64` | Last-modified time, epoch milliseconds; `0` if unknown. |
| `unlink` | `(i32 nameoff, i32 namelen) → i32` | Delete a file or empty directory. `0` ok, `-1` otherwise. |
| `mkdir` | `(i32 nameoff, i32 namelen) → i32` | Create a directory. `0` ok, `-1` otherwise. |
| `rename` | `(i32 fromoff, i32 fromlen, i32 tooff, i32 tolen) → i32` | Rename. `0` ok, `-1` otherwise. |
| `list` | `(i32 nameoff, i32 namelen, i32 outoff) → i32` | Write the directory's entries NUL-separated at `outoff`; return the total byte count **needed** (written only if they all fit — the three-way result above), or `-1` if not a directory. `.` and `..` are omitted. |
| `checksum` | `(i32 off, i32 len) → i32` | Sum of `len` staging bytes at `off`, or `-1` for a refused span. A probe: it exists so a test can prove the host observes the guest's memory. A byte sum is never negative, so `-1` is unambiguous. |

fds 0, 1 and 2 are pre-opened by the embedder and are `System.in`, `System.out`, `System.err`. The
runner maps them to the process's real standard streams; the test harness maps them to capture files.

### module `java.io.HostIO` — the system-property source (§20.18.7)

| Field | Functype | Semantics |
| --- | --- | --- |
| `getprop` | `(i32 keyoff, i32 keylen, i32 outoff) → i32` | Look up the property named by the key bytes. On a hit, return the value's length and write the bytes at `outoff` if they all fit; on a miss, return `-1`. |
| `propnames` | `(i32 outoff) → i32` | Return the total byte count of every property name NUL-separated, and write them at `outoff` if they all fit. Backs `System.getProperties`. |

An absent property is **not an error** — it is how an embedder withholds one. `System.getProperty`
answers `null`, exactly as the specification requires for an undefined property. A value too long to
stage is *not* that case: it answers its length, and the caller grows the memory and asks again.

The runner publishes `java.version`, `java.vendor`, `java.vendor.url`, `java.class.version`,
`os.name`, `os.arch`, `os.version`, `file.separator`, `path.separator`, `line.separator`, `user.dir`.

### module `java.lang.System` — the process environment

| Field | Functype | Semantics |
| --- | --- | --- |
| `currentTimeMillis` | `() → i64` | Milliseconds since the epoch. The runner reads `CLOCK_REALTIME`; the harness returns a deterministic counter, so tests are reproducible. |
| `exit` | `(i32 status) → ()` | Terminate the process with `status`. **Does not return.** The runner flushes the standard streams first. An embedder that leaves this a no-op lets the guest run on past `System.exit`, which the specification forbids — so a real embedder must implement it. |
| `gc` | `() → ()` | Advisory; the collector is not on-demand. No-op. |
| `runFinalization` | `() → ()` | No-op — there are no finalizers to run (see `Object.finalize`). |
| `load` | `(externref) → ()` | No-op. There is no dynamic-linking surface. |
| `loadLibrary` | `(externref) → ()` | No-op, as above. |
| `getSecurityManager` | `() → externref` | `null`. There is no security manager; capability restriction is done by withholding imports (above), not by an in-guest policy object. |
| `setSecurityManager` | `(externref) → ()` | Accepts and discards, consistent with `getSecurityManager` answering `null`. |
| `setProperties` | `(externref) → ()` | Accepts and discards; the property set is the host's. |

### modules `java.lang.Float` / `java.lang.Double` — IEEE-754 bit reinterpretation

No WebAssembly-GC primitive reinterprets a float's bits as an integer, so the embedder supplies the
four casts. They are pure: the same input always gives the same output.

| Module | Field | Functype |
| --- | --- | --- |
| `java.lang.Float` | `floatToIntBits`, `floatToRawIntBits` | `(f32) → i32` |
| `java.lang.Float` | `intBitsToFloat` | `(i32) → f32` |
| `java.lang.Double` | `doubleToLongBits`, `doubleToRawLongBits` | `(f64) → i64` |
| `java.lang.Double` | `longBitsToDouble` | `(i64) → f64` |

### module `java.lang.Object` — the threadless exclusions

Java 1.0 minus `synchronized` has no threads, so §17's monitors have no meaning on this target.

| Field | Functype | Semantics |
| --- | --- | --- |
| `wait` (3 overloads), `notify`, `notifyAll` | `(externref, …) → ()` | **Trap.** There is no monitor to wait on or notify. This is a deliberate, documented exclusion, not an omission. |
| `finalize` | `(externref) → ()` | No-op — `Object.finalize`'s own body is empty, and the collector does not resurrect. |

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
