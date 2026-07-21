# The javelina host ABI — the →HOST native contract (E7.1b)

This is the complete set of host functions a javelina embedder must supply for a compiled Java
program to run. It is the **only** thing the embedder provides beyond loading `jre.wasm` and the
plugin: everything else in the JRE is compiled Java (GC overlays) or a compiler intrinsic. Withhold
any function here and a module that needs it is **unlinkable** — that is the capability-sandboxing
story (a restricted profile is just a `jre.wasm` linked against a subset of these).

The authoritative implementation is `compiler/driver/host_io.h` (shared by the shipped runner
`javelina` and the test harness `test/exec.h`; they differ only in the fd-0/1/2 preopen policy and
the filesystem root). The compiled Java side declares these as `native` methods on the internal
seam classes `java.io.HostIO`, `java.io.Mem`, `java.lang.System`, etc.

## The GC↔host boundary (why args are primitives)

Per WebAssembly §7.1, GC aggregates (structs/arrays, hence every Java object incl. `String` and
`byte[]`) are **opaque** to the host — a host function receives them only as untyped references it
cannot read. So nothing here takes an object. Strings and byte buffers cross as **byte ranges in the
shared I/O staging linear memory**: the guest copies bytes into that memory (via `Mem.store8`), then
passes an `(offset, length)` pair of `i32`s; the host reads/writes those bytes via
`wasm_memory_data`. The staging memory is the jre module's exported `"memory"` (one 32-bit page); a
plugin imports it, so plugin-direct and jre-library I/O share one buffer.

Types below are WASM valtypes (`i32`/`i64`/`f32`/`f64`). Every `(off, len)` pair indexes the staging
memory.

## File-descriptor I/O floor (`java.io.HostIO`)

| native | functype | semantics |
|---|---|---|
| `open(off, len, flags)` | `(i32,i32,i32) → i32` | Open the path in staging bytes `[off,off+len)`. `flags`: `0`=read, `1`=write+truncate, `2`=read+write (create if absent). Returns an fd ≥ 0, or `-1` on failure. |
| `fd_write(fd, off, len)` | `(i32,i32,i32) → i32` | Write `len` staging bytes at `off` to `fd`. Returns bytes written. |
| `fd_read(fd, off, len)` | `(i32,i32,i32) → i32` | Read up to `len` bytes from `fd` into staging at `off`. Returns bytes read, or `-1` at EOF (the `InputStream.read` contract). |
| `fd_seek(fd, pos)` | `(i32,i32) → ()` | Seek `fd` to absolute byte `pos`. |
| `fd_close(fd)` | `(i32) → ()` | Close `fd`. (fds 0/1/2 are never closed by the runner.) |
| `fd_size(fd)` | `(i32) → i64` | Current size of `fd`'s file in bytes, or `-1`. |

Preopened fds: **0 = stdin, 1 = stdout, 2 = stderr** (`FileDescriptor.in/out/err`). The shipped
runner maps these to the real process streams; the test harness maps them to capture temp files.

## File metadata / directory floor (`java.io.HostIO`, for `java.io.File`)

| native | functype | semantics |
|---|---|---|
| `stat(off, len)` | `(i32,i32) → i32` | Bit flags for the path: `1`=exists, `2`=directory, `4`=regular file, `8`=readable, `16`=writable. `0` if absent. |
| `fileSize(off, len)` | `(i32,i32) → i64` | Regular-file size in bytes, or `-1`. |
| `fileModified(off, len)` | `(i32,i32) → i64` | Last-modified time in ms since the epoch, or `0`. |
| `unlink(off, len)` | `(i32,i32) → i32` | Delete the path. `0` on success, `-1` on failure. |
| `mkdir(off, len)` | `(i32,i32) → i32` | Create a directory. `0`/`-1`. |
| `rename(fOff, fLen, tOff, tLen)` | `(i32,i32,i32,i32) → i32` | Rename `from`→`to` (both `(off,len)` in staging). `0`/`-1`. |
| `list(off, len, outOff)` | `(i32,i32,i32) → i32` | Write the directory's entry names (NUL-separated) into staging at `outOff`. Returns total bytes written, or `-1`. |

Paths resolve under an embedder-chosen root (`javelina --root DIR`, default cwd); `..` components are
rejected (no parent-directory escape).

## Environment (`java.lang.System`, `java.lang.Object`)

| native | functype | semantics |
|---|---|---|
| `currentTimeMillis()` | `() → i64` | Wall-clock ms since the epoch (UTC). |
| `exit(code)` | `(i32) → ()` | Terminate the process with `code`. In the runner this does not return; the test harness makes it a no-op. |
| `identityHashCode()` | `() → i32` | A fresh identity-hash id (the caller caches it on the object). Must be stable per object and non-zero. |
| `random()` | `(f64)` | A `double` in `[0, 1)`. (Backs `Math.random`; `java.util.Random`'s LCG is compiled Java.) |

`getProperty`/`getProperties`/`load`/`loadLibrary` are additional documented `System` edges an
embedder MAY supply; the base runner does not, so a program using them is unlinkable against it.

## Threadless exclusions (`java.lang.Object`)

`wait` / `notify` / `notifyAll` — there is no monitor on a threadless target (WASM has no threads),
so these **trap**. This is the same basis as the `−synchronized` language scope: a deliberate,
documented exclusion, never a silent no-op.

## Not part of the contract

- **Compiler-lowered, never host calls:** `Mem.load8`/`store8` (i32.load8_u/store8),
  `Float/Double.*RawBits` (the `Move*` reinterpret intrinsics), `System.arraycopy` (array.copy),
  `Math.sqrt`/`floor`/`ceil`/`rint`/`min`/`max` (f64 opcodes). These are in the module's own code.
- **Test-harness only:** `checksum`, `fd_open_temp` — used by `test/exec.h` to exercise the staging
  memory; not a production edge.

Reflection edges still owed by the runtime (`Class.forName`/`newInstance`, `Throwable.
fillInStackTrace`/`printStackTrace` stack capture) are tracked in E7.2; when they land here, this
table gets their rows. The embedder-facing prose (`docs/embedding.md`) is E9 and cites this table.
