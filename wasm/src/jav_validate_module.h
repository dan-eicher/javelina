// jav_validate_module.h — module-level STRUCTURAL well-formedness (§5.5.1), the
// first piece of the OPTIONAL validator pass that runs over a parsed
// jav_module_t (after jav_module_read). It is deliberately NOT part of the
// parser: jav_module_read decodes bytes; this pass checks the cross-section
// invariants the per-section grammar can't express. An embedder that trusts its
// producer can skip it; one consuming untrusted modules must run it.
//
// Checks (mirroring the reference decoder's module-level `require`s):
//   - sections appear in the prescribed order, each non-custom at most once
//     (custom may appear anywhere);
//   - the function and code sections have matching lengths;
//   - the data count, if present, equals the number of data segments.
// Type validation (§3.4 / §7.6, via jav_typecheck) is the next piece.
#ifndef JAV_VALIDATE_MODULE_H
#define JAV_VALIDATE_MODULE_H

#include "jav_reader.h"

// Returns true if structurally well-formed. On failure returns false and, if
// `err` is non-NULL, sets *err to a static description.
bool jav_module_wf(const jav_module_t *m, const char **err);

#endif /* JAV_VALIDATE_MODULE_H */
