// jav_module_struct.h — §5.5 module STRUCTURE: the malformed conditions that are
// stated over decoded section values rather than over bytes.
//
// THE LAYERING, which is the whole point of this file existing.
//
//   spec/wasm.bbq (BBQ)   the bytes match the grammar, every declared size is an
//                         interval that must be consumed exactly, and nothing the
//                         input claims can be turned into an allocation. Magic,
//                         version, known section ids, section framing, code-entry
//                         framing and every section's internal shape are ALL settled
//                         here, for every reader generated from the one grammar.
//   this file (§5.5)      the conditions §5.5 states across already-decoded sections:
//                         ordering, cross-section length agreement, and the locals
//                         sum. None of them can be a grammar rule — every section is
//                         optional and the prescribed order deliberately does not
//                         follow the id numbers — and none of them opens a function
//                         body.
//   validate.c (§7)       the bytecode. Not this file's business; anything needing to
//                         look at an instruction belongs there.
//
// Runs on the c-lite span index, because that is the tree the engine actually loads
// through (jav_load.c). A gate written against a tree nothing ships is not a gate.
#ifndef JAV_MODULE_STRUCT_H
#define JAV_MODULE_STRUCT_H

#include "bbq_lite.h"    // bbq_field_capture — the span-index node
#include "jav_error.h"   // jav_err_t

// JAV_E_NONE if the module's structure satisfies §5.5, else the specific reason.
// A JAV_E_NONE here means "hand it to the bytecode verifier", not "the module is
// valid" — §5 and §7 are separate verdicts.
jav_err_t jav_module_struct(const bbq_field_capture* root, const uint8_t* base);

#endif // JAV_MODULE_STRUCT_H
