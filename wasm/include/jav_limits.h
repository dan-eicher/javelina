/* jav_limits.h — the engine's per-frame caps, as a PRODUCER-facing contract.
 *
 * WebAssembly §A ("Implementation Limitations") lets an engine set its own bounds and says
 * nothing about what they are, so these are javelina's and a module that exceeds them is
 * well-formed but unrunnable HERE: §7.6 types a body against the locals it declares, so
 * validation admits it, and the frame guard in jav_call_fn then traps at every call.
 *
 * That makes them part of the interface a compiler targeting this engine has to respect,
 * which is why they live in the public include directory rather than beside the frame
 * layout. javelinac reads them and refuses to emit a function it knows cannot be called —
 * a compile-time error naming the method beats a run-time trap naming a frame index.
 *
 * ONE definition. The engine includes this header for the same constants it enforces; a
 * second copy on the producer side is exactly the drift this arrangement prevents.
 */
#ifndef JAV_LIMITS_H
#define JAV_LIMITS_H

/* Maximum operand-stack height of a single frame. Enforced by the validator (§7.6 rejects a
 * body whose maximum height exceeds it), so a module carrying one never loads. */
#define MAX_STACK  1024

/* Maximum locals (parameters + declared locals) of a single frame. NOT enforced by the
 * validator — nothing in §7.6 bounds the count — so it is checked when the frame is carved. */
#define MAX_LOCALS 1024

/* Maximum pages of a single linear memory: 2^20 pages = 64 GiB, backed by one allocation.
 *
 * §3.2.15 bounds a memory type at 2^(|addrtype|-16) pages, which for a 64-bit addrtype is 2^48
 * — 2^64 bytes, an amount no host can back and a byte count that does not fit a u64 at all.
 * Enforced at ALLOCATION rather than validation, so a module carrying such a type still decodes
 * and validates exactly as §3.2.15 says it must: instantiation reports JAV_E_ALLOCATION_FAILED,
 * and memory.grow answers -1 as §4.5.3.8 already permits.
 *
 * Every memory32 is admitted unchanged — its own ceiling is 65536 pages. */
#define JAV_MAX_MEMORY_PAGES (UINT64_C(1) << 20)

/* Maximum elements of a single table: 2^24 = 16,777,216 slots, backed by one bbq_vec.
 *
 * §3.2.16 bounds a table type at 2^|addrtype| − 1 elements — 2^32 − 1 for table32, 2^64 − 1 for a
 * 64-bit addrtype — so a VALID module may declare `(table 4000000000 funcref)` and a running guest
 * may `table.grow` toward that ceiling. Each slot costs a ref word plus a tag byte, so the ceiling
 * is tens of gigabytes the engine would try to touch a slot at a time, and past 2^31 slots the
 * bbq_vec length/capacity (int) wraps — a heap overrun, not a clean failure. This is the table
 * analogue of JAV_MAX_MEMORY_PAGES, and enforced the same way: at ALLOCATION, never at validation,
 * so a module carrying such a type still decodes and validates exactly as §3.2.16 requires —
 * instantiation reports JAV_E_ALLOCATION_FAILED and table.grow answers -1 as §4.5.3.10 permits. */
#define JAV_MAX_TABLE_ELEMS (UINT64_C(1) << 24)

#endif /* JAV_LIMITS_H */
