# Third-party code and its licenses

javelina itself is public-domain (Unlicense — see [LICENSE](LICENSE)). The files
below are *not* original javelina work; each keeps the license it came under.
This page is the ledger so credit is discoverable in one place; the authoritative
notice for each item is in the file (or directory) named.

## GNU GPL v2 with the Classpath Exception

`compiler/lib/` — the Java runtime library — is licensed as a whole under
GPLv2+CPE (full text: [compiler/lib/LICENSE](compiler/lib/LICENSE)), because it
contains code ported from **OpenJDK**:

| file | ported from |
|---|---|
| `compiler/lib/java/lang/FloatingDecimal.java` | OpenJDK `jdk.internal.math.FloatingDecimal` (String→binary parse path) |
| `compiler/lib/java/lang/BinaryToASCIIBuffer.java` | OpenJDK `jdk.internal.math.FloatingDecimal` (binary→String / dtoa path) |
| `compiler/lib/java/lang/ASCIIToBinaryBuffer.java` | OpenJDK `jdk.internal.math.FloatingDecimal.ASCIIToBinaryBuffer` |
| `compiler/lib/java/lang/FDBigInteger.java` | OpenJDK `jdk.internal.math.FDBigInteger` |
| `compiler/lib/java/lang/Math.java` | OpenJDK `FdLibm` (exp/pow and the word-access helpers) |

The originally-authored classes in `compiler/lib/` are placed under the same
terms so the library is licensed uniformly.

## Permissive / notice-only

| item | origin | license |
|---|---|---|
| `compiler/lib/java/lang/Math.java` (sin/cos/tan, asin/acos/atan/atan2, log, fmod, remainder, copysign/scalbn) | Sun **fdlibm** | freely granted, notice preserved (in the file header) |
| `wasm/include/wasm.h` | **WebAssembly/wasm-c-api** (the community C embedding API, originated by Andreas Rossberg) | Apache-2.0 |
| `compiler/tools/unicode/UnicodeData.txt` + the generated `compiler/lib/java/lang/CharacterData.java` | The **Unicode Character Database** (a Unicode 10.0-or-later revision; the pinned file in-tree is the copy of record) | Unicode License v3 |

## Not redistributed (kept locally, not committed)

The reference specifications the code cites by section number are gitignored,
not vendored: the Sun/Oracle *Java Language Specification 1.0*, the *WebAssembly
Core Specification*, and the Alves-Foss/Frincke *Formal Grammar for Java* paper.
Download your own copy if you need one — nothing in the build reads them
(`spec/instructions.toml` is the committed artifact; `gen_instr_toml.py` re-derives
it from the WebAssembly PDF only on demand, and is never a build step).
