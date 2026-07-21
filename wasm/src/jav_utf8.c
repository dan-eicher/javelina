#include "jav_utf8.h"
#include <stdint.h>
#include <stddef.h>

/* §5.2.4 name validation: the spec's `utf8` grammar over Unicode scalar values.
 * Per width, the FIRST continuation byte's range is narrowed so exactly the
 * scalar values encode: E0/F0 exclude overlongs, ED excludes the surrogate
 * block U+D800..U+DFFF, F4 caps at U+10FFFF; C0/C1 (2-byte overlongs) and
 * F5..FF (out of range / 5-6-byte forms) are never valid first bytes, and a
 * truncated or mis-tagged continuation rejects. */
bool jav_name_utf8_ok(bbq_bytes_t b) {
    const uint8_t* p = b.data; size_t n = b.length, i = 0;
    while (i < n) {
        uint8_t b0 = p[i];
        if (b0 < 0x80) { i += 1; continue; }                       /* U+0000..U+007F */
        if (b0 < 0xC2) return false;                               /* stray continuation / overlong */
        if (b0 < 0xE0) {                                           /* U+0080..U+07FF */
            if (n - i < 2 || (p[i+1] & 0xC0) != 0x80) return false;
            i += 2; continue;
        }
        if (b0 < 0xF0) {                                           /* U+0800..U+FFFF minus surrogates */
            if (n - i < 3) return false;
            uint8_t lo = (b0 == 0xE0) ? 0xA0 : 0x80;
            uint8_t hi = (b0 == 0xED) ? 0x9F : 0xBF;
            if (p[i+1] < lo || p[i+1] > hi || (p[i+2] & 0xC0) != 0x80) return false;
            i += 3; continue;
        }
        if (b0 < 0xF5) {                                           /* U+10000..U+10FFFF */
            if (n - i < 4) return false;
            uint8_t lo = (b0 == 0xF0) ? 0x90 : 0x80;
            uint8_t hi = (b0 == 0xF4) ? 0x8F : 0xBF;
            if (p[i+1] < lo || p[i+1] > hi
                || (p[i+2] & 0xC0) != 0x80 || (p[i+3] & 0xC0) != 0x80) return false;
            i += 4; continue;
        }
        return false;                                              /* F5..FF */
    }
    return true;
}
