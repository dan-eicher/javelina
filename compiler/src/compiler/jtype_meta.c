/* jtype_meta.c — definitions for the JT_* metadata tables. */

#include <limits.h>
#include <stdint.h>
#include "javelina/compiler/jtype_meta.h"


/* JLS §5.1.2 numeric range bounds for primitive integer tags.
 * Used for compile-time constant narrowing checks. Non-numeric
 * tags get 0/0 — callers must filter on tag before consulting. */
const int32_t jtype_min[] = {
    [JT_BOOL]  = 0,           /* false */
    [JT_BYTE]  = -128,
    [JT_SHORT] = -32768,
    [JT_CHAR]  = 0,           /* char is unsigned 16-bit */
    [JT_INT]   = INT32_MIN,
};
const int32_t jtype_max[] = {
    [JT_BOOL]  = 1,           /* true */
    [JT_BYTE]  = 127,
    [JT_SHORT] = 32767,
    [JT_CHAR]  = 65535,
    [JT_INT]   = INT32_MAX,
};

/* JVMS §4.3 type-descriptor characters. JT_CLASS / JT_ARRAY are
 * structural (followed by a class name + ';' or by an element
 * type), so callers using this table for those tags must still
 * emit the trailing structure. */
const char jtype_desc_char[] = {
    [JT_BOOL]   = 'Z',
    [JT_BYTE]   = 'B',
    [JT_SHORT]  = 'S',
    [JT_INT]    = 'I',
    [JT_CHAR]   = 'C',
    [JT_LONG]   = 'J',
    [JT_FLOAT]  = 'F',
    [JT_DOUBLE] = 'D',
    [JT_V128]   = 'Q',   /* internal-only: JVMS has no v128; 'Q' never leaves the compiler */
    [JT_VOID]   = 'V',
    [JT_CLASS]  = 'L',
    [JT_ARRAY]  = '[',
    [JT_ERROR]  = 0,   /* + JT_NULL: 0 — the explicit high entry sizes the table to the FULL
                        * tag enum, so no tag (char/long/float/double were appended AFTER this
                        * table was first written) can index past its end (JVMS §4.3). */
};
