// jav_load.c — the bytes → §5 decode → §7 validate pipeline in one place.
#include "jav_load.h"
#include "jav_view_nav.h"        // jav_view_module (c-lite decode/index)
#include "jav_module_index.h"    // jav_module_index (flatten)
#include "jav_module_validate.h" // jav_module_validate (§7 gate)
#include "jav_module_struct.h"   // jav_module_struct (§5.5 structure gate)
#include "bbq_arena.h"

jav_status_t jav_validate_bytes(const uint8_t* bytes, size_t len, jav_err_t* err) {
    if (err) *err = JAV_E_NONE;
    bbq_arena a; bbq_arena_init(&a, 0);
    bbq_capture_metadata m = jav_view_module(bytes, len, &a);
    jav_status_t r;
    jav_err_t se;
    if (!m.success) {
        r = JAV_MALFORMED;                          // §5: not a well-formed module image
    } else if ((se = jav_module_struct(m.root, bytes)) != JAV_E_NONE) {
        // §5.5 structure. Still MALFORMED — the grammar settled the bytes, this settles
        // the conditions §5.5 states across the decoded sections. It runs BEFORE the
        // index because the index reads counts this gate is what makes trustworthy.
        if (err) *err = se;
        r = JAV_MALFORMED;
    } else {
        jav_modidx_t mod;
        if (!jav_module_index(m.root, bytes, &a, &mod))
            r = JAV_INVALID;                        // §7: index could not be built (out-of-range / unsupported)
        else {
            r = jav_module_validate(m.root, bytes, &mod, err);
            jav_modidx_free_bodies(&mod);   /* this path answers valid/invalid and keeps nothing */
        }
    }
    bbq_arena_free(&a);
    return r;
}
