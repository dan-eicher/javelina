// jav_extern.c — jav_project_export: the §4.5.2 externval projection (read an export off a LIVE
// instance entity into a jav_extern_t), shared by the c-api's positional import marshaling — one copy,
// no per-embedder clone. (The §4.2.3 store is wasm_store_t, wasm_capi.c.)
#include "jav_extern.h"
#include "heap.h"           // jav_mem_t (heap->mems) + page math
#include "bbq_vec.h"        // bbq_vec_len (table export size)
#include <string.h>         // memset

#define STORE_PAGE 65536u   // §4.2.8 the fixed WASM linear-memory page size (64 KiB)

// Project export #index of kind `kind` off the LIVE instance entity into an externval (§4.5.2: an
// external value is an address; its type is read off the live entity here, at import-match time).
void jav_project_export(heap_t* heap, const jav_instance_t* inst, const jav_modidx_t* gm,
                        uint8_t kind, uint32_t index, jav_extern_t* out) {
    memset(out, 0, sizeof *out);
    switch (kind) {
    case 0: out->kind = 0; out->u.func.type = &gm->func_sigs[index]; out->u.func.func = inst->funcs[index]; break;
    case 1: { out->kind = 1; jav_tableinst_t* ti = &inst->tables[index];
              out->u.table.data = ti->refs; out->u.table.types = ti->types;
              out->u.table.size = (uint32_t)bbq_vec_len(ti->refs);
              out->u.table.reftype = ti->reftype; out->u.table.reftype_ht = ti->reftype_ht;
              out->u.table.gcanon = inst->gcanon;   // §4.5.2 provider typeidx→global id (concrete reftype matching)
              out->u.table.is64 = ti->is64;         // §3.3.15 addrtype
              out->u.table.has_max = ti->has_max; out->u.table.max = ti->max; break; }
    case 2: { out->kind = 2; uint32_t memaddr = inst->mem_addrs[index]; jav_mem_t* mm = &heap->mems[memaddr];
              out->u.mem.memidx = memaddr; out->u.mem.min = mm->size / STORE_PAGE;
              out->u.mem.has_max = mm->has_max; out->u.mem.max = mm->max / STORE_PAGE;
              out->u.mem.is64 = mm->is64; break; }
    case 3: { out->kind = 3; out->u.global.slot = inst->globals[index];   // export the slot itself (mutable share)
              out->u.global.type = gm->global_types[index];
              out->u.global.type_ht = gm->global_tidx ? (int32_t)gm->global_tidx[index] : 0;
              out->u.global.gcanon = inst->gcanon;   // §4.5.2 provider typeidx→global id (concrete reftype matching)
              out->u.global.tag = inst->global_types[index];   // the value's runtime tag (T_GCREF for a managed ref)
              out->u.global.mut = gm->global_mut[index]; break; }
    case 4: out->kind = 4; out->u.tag.type = &gm->tags[index];           // §4.5.2 taginst.type
            out->u.tag.gcanon = inst->gcanon; out->u.tag.typeidx = gm->tag_typeidx[index];  // §3.3.12 closed-type id
            out->u.tag.tag_id = inst->tag_ids ? inst->tag_ids[index] : 0; break;             // §4.2 store tagaddr identity
    default: out->kind = kind; break;
    }
}
