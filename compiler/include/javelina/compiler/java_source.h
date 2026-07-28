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

/* Translate `src` (of `len` bytes) per §3.3 and initialise `p` over the result.
 *
 * On success *owned holds the translated buffer; free it AFTER parsing (idents and literals are
 * duplicated into the parse arena, so it need not outlive the parse). On a malformed escape
 * returns false with *err set — §3.3: "a compile-time error occurs" — and *owned NULL.
 *
 * THE LENGTH IS A PARAMETER, not strlen(src). It used to be recovered here, and a source file
 * containing a NUL byte was therefore silently truncated at it: javelinac exited 0 having
 * compiled only the prefix, so a class declared after the NUL simply did not exist and the only
 * symptom was an "undefined" error reported against whichever OTHER file referenced it.
 *
 * A NUL is legal Java. §3.1 makes a program a sequence of Unicode characters, §3.7 makes a
 * comment's content any InputCharacter, and §3.5 exempts exactly one character from the input
 * — "the ASCII SUB character ... is ignored if it is the last character" — which is not this
 * one. Every layer below already carried a length (java_unicode_translate takes one,
 * java_parser_init takes one); only this function invented it. Callers with a real byte count
 * pass it; callers holding a C string literal pass strlen and are correct to. */
static inline bool java_source_init(peg_state* p, const char* src, int len,
                                    char** owned, const char** err) {
    int tlen = 0;
    *err = NULL;
    char* t = java_unicode_translate(src, len, &tlen, err);
    if (!t) { *owned = NULL; return false; }
    java_parser_init(p, t, tlen);
    *owned = t;
    return true;
}

#endif /* JAVELINA_COMPILER_JAVA_SOURCE_H */
