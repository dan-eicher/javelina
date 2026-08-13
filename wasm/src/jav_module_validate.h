// jav_module_validate.h — the §7 module validation gate over the c-lite index.
//
// The reader's success (m.success) is the §5 malformed gate; this is the §7 *invalid*
// gate layered on top of a successfully-indexed module. It returns the precise verdict
// JAV_OK / JAV_INVALID (the runtime machine still only ever yields OK/TRAP/RETURN).
#ifndef JAV_MODULE_VALIDATE_H
#define JAV_MODULE_VALIDATE_H

#include "jav_module_index.h"   // jav_modidx_t (+ validate.h / runtime_api.h via it)
#include "jav_error.h"          // jav_err_t (the fine reason behind JAV_INVALID)

// Validate a successfully-indexed module. Returns JAV_OK or JAV_INVALID; on JAV_INVALID
// (and only then) *err is set to the precise reason (JAV_E_NONE on OK). `err` may be NULL.
jav_status_t jav_module_validate(const bbq_field_capture* root, const uint8_t* base,
                                 jav_modidx_t* mod, jav_err_t* err);

// Re-derive a defined function body's §7.6 side-table: decode its RLE locals, build the cx
// from the index (jav_module_cx), and run jav_typecheck_ex. `entry` is the code-section
// entry node, `sig` its signature; *out_ndecl (if non-NULL) gets the declared-local slot
// count. Returns 1 with *st/*tr malloc'd (caller frees), 0 on a type error. Shared by the
// validator (gate) and the instantiator (which keeps the side-table).
// `func_ref_declared` = the C.refs bitmap (per funcidx) for ref.func validation in the body;
// NULL skips that check (the instantiator re-derives an already-validated module).
// `out_locals` (optional): the FLAT locals — params then the RLE-decoded declared
// ones — which this function builds anyway to type the body. A caller that passes
// non-NULL takes ownership (bbq_vec_free); passing NULL frees them here, as before.
// Handing them back is what keeps a second consumer from re-walking §5.5.13's RLE
// groups and getting a different answer.
int jav_body_typecheck(const jav_modidx_t* mod, const uint8_t* base,
                       const bbq_field_capture* entry, const jav_functype_t* sig,
                       const uint8_t* func_ref_declared,
                       uint32_t* out_ndecl, jav_st_entry_t** st, unsigned* nst,
                       jav_try_t** tr, unsigned* ntr, jav_err_t* out_err,
                       jav_valtype_t** out_locals);

#endif // JAV_MODULE_VALIDATE_H
