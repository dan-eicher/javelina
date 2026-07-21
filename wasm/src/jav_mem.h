#ifndef JAV_MEM_H
#define JAV_MEM_H
/* §4.6.8 linear-memory ACCESS — INLINE natives (folded into the handler/stencil, no extern). The heap owns
 * the storage LAYER (buffer alloc + memory.grow's realloc + memory.init's segment read); the bytecode does
 * the bounds-checked byte read/write. `static inline` so opgen's `inline native` direct call folds them in;
 * clang turns the fixed-size memcpy into a mov (no libc reloc). (memory.fill/copy/init/grow/size stay native:
 * variable-length memset/memmove can't ride a stencil, and grow/size touch the storage layer.) */
#include "heap.h"        /* full heap_t (h->mems = vec of jav_mem_t) + (via runtime_api.h) vm_t/frame_t + the value model (v128_t/f4/f8) */
#include "bbq_vec.h"     /* bbq_vec_len over heap->mems */
#include <string.h>      /* memcpy / memset for the byte access */

/* Every access in this header fails one way — §7.10 "out of bounds memory access".
 * Bulk ops over TABLES trap with a different reason and say so at their call site. */
#define MEM_TRAP(vm) JAV_TRAP_WITH(vm, JAV_TRAP_OutOfBoundsMemoryAccess)

/* Resolve a module memidx to its store meminst (§4.2.3) through the bound instance's memaddr table. */
static inline jav_mem_t* mem_at(vm_t* vm, heap_t* h, int memidx) {
    if (memidx < 0 || (u4)memidx >= vm->frame.ctx->num_mems) return NULL;
    int addr = (int)vm->frame.ctx->mem_addrs[memidx];
    return addr < bbq_vec_len(h->mems) ? &h->mems[addr] : NULL;
}
/* ea + n within the live size, overflow-safe (ea/n are 64-bit so memory64 addresses arrive un-truncated). */
static inline int mem_ok(jav_mem_t* m, u8 ea, u8 n) { return m && ea <= m->size && n <= m->size - ea; }

/* §4.6.8 the memory-access precondition, as a DSL-callable predicate: every memarg op declares
 * `error: (. !mem_in_bounds(memidx, addr + offset, N) .) -> OutOfBoundsMemoryAccess`, so the
 * bounds contract is visible to a verifier instead of buried in the accessor. N is the STORAGE
 * width (load8_u is 1, not 4) — the same width the accessor uses and the same one the spec's
 * `align` column names; gen_trap_reasons asserts that equality on every build. */
static inline int mem_in_bounds(vm_t* vm, heap_t* h, s4 mi, s8 ea, s8 n) {
    return mem_ok(mem_at(vm, h, mi), (u8)ea, (u8)n);
}

/* §4.6.7 passive-segment length, so memory.init / table.init can declare their SOURCE bound the
 * same way they declare the destination one. A dropped segment is ε — length 0, NOT an
 * unconditional trap — and an out-of-range segidx reads as 0 so the `s+n > len` test rejects it.
 * (`data.drop`/`elem.drop` on an out-of-range index are already no-ops.) */
static inline s8 data_seg_len(vm_t* vm, heap_t* h, s4 seg) { (void)h;
    if ((u4)seg >= vm->frame.ctx->num_data_segs) return 0;
    return vm->frame.ctx->data_dropped[seg] ? 0 : (s8)vm->frame.ctx->data_segs[seg].len;
}
static inline s8 elem_seg_len(vm_t* vm, heap_t* h, s4 seg) { (void)h;
    if ((u4)seg >= vm->frame.ctx->num_elem_segs) return 0;
    return vm->frame.ctx->elem_dropped[seg] ? 0 : (s8)vm->frame.ctx->elem_segs[seg].len;
}
/* The data-segment STRIDE for an array type: array.new_data/init_data read n*w bytes, not n.
 * The RTT is the one authority for it (always set for an array type). */
static inline s8 array_elem_width(vm_t* vm, heap_t* h, s4 typ) { (void)h;
    if ((u4)typ >= vm->frame.ctx->num_struct_rtts) return 0;
    return (s8)vm->frame.ctx->struct_rtts[typ]->elem_store_w;
}

/* memory.size / .grow addr-result helpers (the page count + addrtype-width flag for the push_addr result). */
static inline s8  mem_pages(vm_t* vm, heap_t* h, s4 mi) { jav_mem_t* m = mem_at(vm,h,mi); return (s8)(m ? m->size/65536 : 0); }
static inline int mem_is64 (vm_t* vm, heap_t* h, s4 mi) { jav_mem_t* m = mem_at(vm,h,mi); return m && m->is64; }

/* ── full-width integer / float / v128 load + store (§4.6.8) ── */
static inline s4 mem_load_i32(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u4 v; memcpy(&v,m->data+(u8)ea,4); return (s4)v; }
static inline s4 mem_load_i8 (vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); return (s4)(s1)m->data[(u8)ea]; }
static inline s4 mem_load_i16(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); s2 v; memcpy(&v,m->data+(u8)ea,2); return (s4)v; }
static inline s8 mem_load_i64(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u8 v; memcpy(&v,m->data+(u8)ea,8); return (s8)v; }
static inline f4 mem_load_f32(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); f4 v; memcpy(&v,m->data+(u8)ea,4); return v; }
static inline f8 mem_load_f64(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); f8 v; memcpy(&v,m->data+(u8)ea,8); return v; }
static inline void mem_store_i32(vm_t* vm, heap_t* h, s4 mi, s8 ea, s4 v) { jav_mem_t* m=mem_at(vm,h,mi); memcpy(m->data+(u8)ea,&v,4); }
static inline void mem_store_i8 (vm_t* vm, heap_t* h, s4 mi, s8 ea, s4 v) { jav_mem_t* m=mem_at(vm,h,mi); m->data[(u8)ea] = (u1)v; }
static inline void mem_store_i16(vm_t* vm, heap_t* h, s4 mi, s8 ea, s4 v) { jav_mem_t* m=mem_at(vm,h,mi); s2 x=(s2)v; memcpy(m->data+(u8)ea,&x,2); }
static inline void mem_store_i64(vm_t* vm, heap_t* h, s4 mi, s8 ea, s8 v) { jav_mem_t* m=mem_at(vm,h,mi); memcpy(m->data+(u8)ea,&v,8); }
static inline void mem_store_f32(vm_t* vm, heap_t* h, s4 mi, s8 ea, f4 v) { jav_mem_t* m=mem_at(vm,h,mi); memcpy(m->data+(u8)ea,&v,4); }
static inline void mem_store_f64(vm_t* vm, heap_t* h, s4 mi, s8 ea, f8 v) { jav_mem_t* m=mem_at(vm,h,mi); memcpy(m->data+(u8)ea,&v,8); }
static inline v128_t mem_load_v128(vm_t* vm, heap_t* h, s4 mi, s8 ea)     { v128_t z; memset(&z,0,sizeof z); jav_mem_t* m=mem_at(vm,h,mi); memcpy(&z,m->data+(u8)ea,16); return z; }
static inline void   mem_store_v128(vm_t* vm, heap_t* h, s4 mi, s8 ea, v128_t v) { jav_mem_t* m=mem_at(vm,h,mi); memcpy(m->data+(u8)ea,&v,16); }

/* ── sub-word load-and-extendˢˣ / wrap-and-store (§4.6.8) ── */
static inline s4 mem_load_i32_8u (vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u1 v=m->data[(u8)ea];                  return (s4)(u4)v; }
static inline s4 mem_load_i32_16u(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u2 v; memcpy(&v,m->data+(u8)ea,2);  return (s4)(u4)v; }
static inline s8 mem_load_i64_8s (vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); s1 v=(s1)m->data[(u8)ea];              return (s8)v; }
static inline s8 mem_load_i64_8u (vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u1 v=m->data[(u8)ea];                  return (s8)(u8)v; }
static inline s8 mem_load_i64_16s(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); s2 v; memcpy(&v,m->data+(u8)ea,2);  return (s8)v; }
static inline s8 mem_load_i64_16u(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u2 v; memcpy(&v,m->data+(u8)ea,2);  return (s8)(u8)v; }
static inline s8 mem_load_i64_32s(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); s4 v; memcpy(&v,m->data+(u8)ea,4);  return (s8)v; }
static inline s8 mem_load_i64_32u(vm_t* vm, heap_t* h, s4 mi, s8 ea) { jav_mem_t* m=mem_at(vm,h,mi); u4 v; memcpy(&v,m->data+(u8)ea,4);  return (s8)(u8)v; }
static inline void mem_store_i64_8 (vm_t* vm, heap_t* h, s4 mi, s8 ea, s8 v) { jav_mem_t* m=mem_at(vm,h,mi); u1 b=(u1)v;            m->data[(u8)ea]=b; }
static inline void mem_store_i64_16(vm_t* vm, heap_t* h, s4 mi, s8 ea, s8 v) { jav_mem_t* m=mem_at(vm,h,mi); u2 b=(u2)v; memcpy(m->data+(u8)ea,&b,2); }
static inline void mem_store_i64_32(vm_t* vm, heap_t* h, s4 mi, s8 ea, s8 v) { jav_mem_t* m=mem_at(vm,h,mi); u4 b=(u4)v; memcpy(m->data+(u8)ea,&b,4); }

/* ── SIMD widening loads (§4.6.8 v128.loadKxM_sx / loadN_zero): 8 bytes, M lanes extendˢˣ to 2K ── */
static inline v128_t mem_load8x8_s (vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<8;k++) r.i16[k]=(s2)(s1)m->data[(u8)ea+k]; return r; }
static inline v128_t mem_load8x8_u (vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<8;k++) r.i16[k]=(s2)(u2)(u1)m->data[(u8)ea+k]; return r; }
static inline v128_t mem_load16x4_s(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<4;k++){ s2 v; memcpy(&v,m->data+(u8)ea+k*2,2); r.i32[k]=(s4)v; } return r; }
static inline v128_t mem_load16x4_u(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<4;k++){ u2 v; memcpy(&v,m->data+(u8)ea+k*2,2); r.i32[k]=(s4)(u4)v; } return r; }
static inline v128_t mem_load32x2_s(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<2;k++){ s4 v; memcpy(&v,m->data+(u8)ea+k*4,4); r.i64[k]=(s8)v; } return r; }
static inline v128_t mem_load32x2_u(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); for(int k=0;k<2;k++){ u4 v; memcpy(&v,m->data+(u8)ea+k*4,4); r.i64[k]=(s8)(u8)v; } return r; }
static inline v128_t mem_load32_zero(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); s4 v; memcpy(&v,m->data+(u8)ea,4); r.i32[0]=v; return r; }
static inline v128_t mem_load64_zero(vm_t* vm, heap_t* h, s4 mi, s8 ea) { v128_t r; memset(&r,0,sizeof r); jav_mem_t* m=mem_at(vm,h,mi); s8 v; memcpy(&v,m->data+(u8)ea,8); r.i64[0]=v; return r; }

#endif /* JAV_MEM_H */
