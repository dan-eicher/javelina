/* java_source.h — the ONE way to hand Java source to the parser.
 *
 * JLS §3.2 lists three lexical translation steps "applied in turn", and step 1 (§3.3 Unicode
 * escapes) runs over the RAW stream, before line terminators and before tokens. So every parse
 * entry point has to translate first — not just the shipped driver.
 *
 * This header exists because wiring only the driver made the TESTS parse a different language
 * from the PRODUCT: `A` spelled an identifier in a real compile and a syntax error under
 * test_exec. Five call sites open-coding the same two lines is the same defect waiting to
 * happen again, so there is one function and everybody calls it.
 */
#ifndef JAVELINA_COMPILER_JAVA_SOURCE_H
#define JAVELINA_COMPILER_JAVA_SOURCE_H

#include "java_parser.h"
#include <string.h>
#include <stdlib.h>

/* Translate `src` per §3.3 and initialise `p` over the result.
 *
 * On success *owned holds the translated buffer; free it AFTER parsing (idents and literals are
 * duplicated into the parse arena, so it need not outlive the parse). On a malformed escape
 * returns false with *err set — §3.3: "a compile-time error occurs" — and *owned NULL. */
static inline bool java_source_init(peg_state* p, const char* src,
                                    char** owned, const char** err) {
    int tlen = 0;
    *err = NULL;
    char* t = java_unicode_translate(src, (int)strlen(src), &tlen, err);
    if (!t) { *owned = NULL; return false; }
    java_parser_init(p, t, tlen);
    *owned = t;
    return true;
}

#endif /* JAVELINA_COMPILER_JAVA_SOURCE_H */
