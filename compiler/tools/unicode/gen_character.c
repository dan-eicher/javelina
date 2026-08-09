/* gen_character.c — generate java.lang.CharacterData from UnicodeData.txt.
 *
 * BMP-only (Java `char` is a 16-bit UTF-16 code unit, so 0x0000..0xFFFF is the whole
 * reachable domain; supplementary planes cannot be expressed in a char and are dead).
 * For each Character predicate we compute per-code-point membership from the UCD General
 * Category (field 2), coalesce contiguous runs into ranges, and emit a *binary-search
 * if-tree* as the method body — O(log n), no runtime data array (which the WASM engine
 * caps via array.new_fixed), no data segment. Case mapping (fields 12/13) coalesces into
 * (range, constant-delta) tuples and emits a tree returning cp+delta.
 *
 * Regenerate; do NOT hand-edit CharacterData.java.
 *   cc -O2 -o gen_character gen_character.c && ./gen_character UnicodeData.txt > ../../lib/java/lang/CharacterData.java
 *
 * UnicodeData.txt is the Unicode Character Database, under the Unicode License
 * v3 (https://www.unicode.org/license.txt). The pinned in-tree copy is a
 * Unicode 10.0-or-later revision and is the version of record; see THIRD_PARTY.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BMP 0x10000

static int is_letter[BMP], is_digit[BMP], is_upper[BMP], is_lower[BMP], is_title[BMP], is_defined[BMP];
static int up_map[BMP], lo_map[BMP], ti_map[BMP];   /* mapping target, or -1 */
/* UCD field 6, the DECIMAL DIGIT VALUE, for category-Nd code points; -1 elsewhere. `is_digit`
 * alone says a character is a digit but not WHICH one, and §20.5.9's `digit` has to answer that
 * for every script, not just ASCII — Tamil '௧' is 1 and Tibetan '༧' is 7. Reuses the
 * case-map emitter unchanged: an Nd block runs 0..9 from its start S, so value - cp is the
 * constant -S over the block and coalesce_map folds each script's digits into ONE range. */
static int dig_val[BMP];

/* GENERAL categories that count as a Java letter (§20.5): Lu Ll Lt Lm Lo. */
static int cat_is_letter(const char* c){
    return c[0]=='L' && (c[1]=='u'||c[1]=='l'||c[1]=='t'||c[1]=='m'||c[1]=='o');
}

#define PAD(n) do { for (int _i = 0; _i < (n); _i++) fputs("        ", o); } while (0)

/* coalesce a boolean membership into ranges; returns count. */
static int coalesce_bool(const int* member, int* lo, int* hi){
    int n = 0, i = 0;
    while (i < BMP){
        if (member[i]){ int s = i; while (i < BMP && member[i]) i++; lo[n] = s; hi[n] = i - 1; n++; }
        else i++;
    }
    return n;
}
/* coalesce a mapping (map[cp]>=0) into (lo,hi,delta) with delta = map[cp]-cp constant. */
static int coalesce_map(const int* map, int* lo, int* hi, int* delta){
    int n = 0, i = 0;
    while (i < BMP){
        if (map[i] >= 0){ int d = map[i] - i, s = i;
            while (i < BMP && map[i] >= 0 && map[i] - i == d) i++;
            lo[n] = s; hi[n] = i - 1; delta[n] = d; n++; }
        else i++;
    }
    return n;
}

static void emit_bool_tree(FILE* o, int* lo, int* hi, int a, int b, int ind){
    if (a > b){ PAD(ind); fputs("return false;\n", o); return; }
    int mid = (a + b) / 2;
    PAD(ind); fprintf(o, "if (cp < %d) {\n", lo[mid]);
    emit_bool_tree(o, lo, hi, a, mid - 1, ind + 1);
    PAD(ind); fprintf(o, "} else if (cp <= %d) {\n", hi[mid]);
    PAD(ind + 1); fputs("return true;\n", o);
    PAD(ind); fputs("} else {\n", o);
    emit_bool_tree(o, lo, hi, mid + 1, b, ind + 1);
    PAD(ind); fputs("}\n", o);
}
static void emit_map_tree(FILE* o, int* lo, int* hi, int* delta, int a, int b, int ind){
    if (a > b){ PAD(ind); fputs("return cp;\n", o); return; }
    int mid = (a + b) / 2;
    PAD(ind); fprintf(o, "if (cp < %d) {\n", lo[mid]);
    emit_map_tree(o, lo, hi, delta, a, mid - 1, ind + 1);
    PAD(ind); fprintf(o, "} else if (cp <= %d) {\n", hi[mid]);
    PAD(ind + 1); fprintf(o, "return cp + (%d);\n", delta[mid]);
    PAD(ind); fputs("} else {\n", o);
    emit_map_tree(o, lo, hi, delta, mid + 1, b, ind + 1);
    PAD(ind); fputs("}\n", o);
}

static void emit_bool_method(FILE* o, const char* name, const int* member){
    static int lo[BMP], hi[BMP];
    int n = coalesce_bool(member, lo, hi);
    fprintf(o, "    static boolean %s(int cp) {\n", name);
    emit_bool_tree(o, lo, hi, 0, n - 1, 1);
    fprintf(o, "    }\n");
    fprintf(stderr, "  %s: %d ranges\n", name, n);
}

/* The same coalesced ranges, emitted for the COMPILER's lexer rather than the runtime's
 * Character. §3.8 defines an identifier as JavaLetter{JavaLetterOrDigit}, and a Java letter is
 * any Unicode letter — a fact the lexer needs in C, before any Java runs. Deriving it from the
 * same UnicodeData.txt in the same pass is the point: a second hand-maintained table would
 * drift from Character and the two would disagree about what an identifier is. */
static void emit_bool_c(FILE* o, const char* name, const int* member){
    static int lo[BMP], hi[BMP];
    int n = coalesce_bool(member, lo, hi);
    fprintf(o, "static inline bool %s(int cp) {\n", name);
    emit_bool_tree(o, lo, hi, 0, n - 1, 1);
    fprintf(o, "}\n\n");
    fprintf(stderr, "  [C] %s: %d ranges\n", name, n);
}
static void emit_map_method(FILE* o, const char* name, const int* map){
    static int lo[BMP], hi[BMP], delta[BMP];
    int n = coalesce_map(map, lo, hi, delta);
    fprintf(o, "    static int %s(int cp) {\n", name);
    emit_map_tree(o, lo, hi, delta, 0, n - 1, 1);
    fprintf(o, "    }\n");
    fprintf(stderr, "  %s: %d ranges\n", name, n);
}

int main(int argc, char** argv){
    if (argc < 2){ fprintf(stderr, "usage: %s UnicodeData.txt [-c]\n", argv[0]); return 2; }
    int c_mode = (argc >= 3 && strcmp(argv[2], "-c") == 0);   /* -c: the lexer's C header */
    for (int i = 0; i < BMP; i++){ up_map[i] = -1; lo_map[i] = -1; ti_map[i] = -1; dig_val[i] = -1; }
    FILE* f = fopen(argv[1], "r");
    if (!f){ perror("UnicodeData.txt"); return 2; }
    char line[1024];
    int range_start = -1, rL = 0, rD = 0, rU = 0, rLc = 0, rT = 0;
    while (fgets(line, sizeof line, f)){
        line[strcspn(line, "\r\n")] = 0;   /* the LAST field (titlecase) would otherwise keep the trailing \n */
        char* fld[16]; int nf = 0; char* p = line;
        fld[nf++] = p;
        while (*p && nf < 16){ if (*p == ';'){ *p = 0; fld[nf++] = p + 1; } p++; }
        if (nf < 14) continue;
        int cp = (int)strtol(fld[0], NULL, 16);
        const char* name = fld[1];
        const char* cat  = fld[2];
        int up = fld[12][0] ? (int)strtol(fld[12], NULL, 16) : -1;
        int lw = fld[13][0] ? (int)strtol(fld[13], NULL, 16) : -1;
        int ti = (nf >= 15 && fld[14][0]) ? (int)strtol(fld[14], NULL, 16) : -1;
        int L = cat_is_letter(cat), D = (cat[0]=='N'&&cat[1]=='d'),
            U = (cat[0]=='L'&&cat[1]=='u'), Lc = (cat[0]=='L'&&cat[1]=='l'), T = (cat[0]=='L'&&cat[1]=='t');
        if (strstr(name, "First>")){ range_start = cp; rL = L; rD = D; rU = U; rLc = Lc; rT = T; continue; }
        if (strstr(name, "Last>")){
            for (int x = range_start; x <= cp; x++) if (x >= 0 && x < BMP){
                is_letter[x] = rL; is_digit[x] = rD; is_upper[x] = rU; is_lower[x] = rLc; is_title[x] = rT; is_defined[x] = 1; }
            range_start = -1; continue;
        }
        if (cp < 0 || cp >= BMP) continue;   /* BMP only */
        is_letter[cp] = L; is_digit[cp] = D; is_upper[cp] = U; is_lower[cp] = Lc; is_title[cp] = T; is_defined[cp] = 1;
        /* Only for Nd: field 6 is also present on No/Nl code points (Roman numerals, fractions),
         * which are NOT digits for §20.5.9 and must not answer `digit`. */
        if (D && fld[6][0]) dig_val[cp] = (int)strtol(fld[6], NULL, 10);
        if (up >= 0 && up < BMP) up_map[cp] = up;   /* char->char only */
        if (lw >= 0 && lw < BMP) lo_map[cp] = lw;
        if (ti >= 0 && ti < BMP) ti_map[cp] = ti;
    }
    fclose(f);

    /* Full-BMP self-check: a rolling hash of the ORACLE, in the exact int arithmetic Character will use
     * at runtime. The test loops 0..0xFFFF calling the generated Character and must reproduce this — so a
     * tree-emission bug in ANY range (not just the sampled scripts) is caught. */
    {
        unsigned int h = 0;
        for (int c = 0; c < BMP; c++){
            h = h * 31u + (unsigned)(is_letter[c] ? 1 : 0);
            h = h * 31u + (unsigned)(is_digit[c] ? 1 : 0);
            h = h * 31u + (unsigned)(up_map[c] >= 0 ? up_map[c] : c);
            h = h * 31u + (unsigned)(lo_map[c] >= 0 ? lo_map[c] : c);
        }
        fprintf(stderr, "  FULL-BMP oracle checksum = %d   (hardcode in test_exec's Character full-range check)\n", (int)h);
    }

    FILE* o = stdout;

    if (c_mode) {
        fputs("/* GENERATED by tools/unicode/gen_character.c -c from the pinned UnicodeData.txt.\n", o);
        fputs(" * DO NOT EDIT — regenerate. Same coalesced BMP ranges as java.lang.CharacterData,\n", o);
        fputs(" * emitted in C because §3.8 makes a JavaLetter any Unicode letter and the LEXER has\n", o);
        fputs(" * to know that before any Java runs. One source, two emitters: a hand-kept second\n", o);
        fputs(" * table would drift, and the compiler and the runtime would disagree about what an\n", o);
        fputs(" * identifier is.\n", o);
        fputs(" *\n", o);
        fputs(" * Source: the Unicode Character Database, Unicode License v3; see THIRD_PARTY.md. */\n", o);
        fputs("#ifndef JAVELINA_UNICODE_ID_H\n#define JAVELINA_UNICODE_ID_H\n\n", o);
        fputs("#include <stdbool.h>\n\n", o);
        emit_bool_c(o, "juni_is_letter", is_letter);
        emit_bool_c(o, "juni_is_digit",  is_digit);
        /* §3.8: "A Java letter is a character for which Character.isJavaLetter returns true" —
         * §20.5.17 defines that as a letter, or the ASCII underscore or dollar sign. Spelled
         * here exactly as java.lang.Character spells it, so the two cannot diverge. */
        fputs("/* §3.8 / §20.5.17 JavaLetter: a Unicode letter, or '_' or '$'. */\n", o);
        fputs("static inline bool juni_is_java_letter(int cp) {\n", o);
        fputs("    return juni_is_letter(cp) || cp == '_' || cp == '$';\n}\n\n", o);
        fputs("/* §3.8 / §20.5.18 JavaLetterOrDigit: a Java letter, or a Unicode digit. */\n", o);
        fputs("static inline bool juni_is_java_letter_or_digit(int cp) {\n", o);
        fputs("    return juni_is_java_letter(cp) || juni_is_digit(cp);\n}\n\n", o);
        fputs("#endif /* JAVELINA_UNICODE_ID_H */\n", o);
        return 0;
    }

    fputs("package java.lang;\n\n", o);
    fputs("// GENERATED by tools/unicode/gen_character.c from a pinned UnicodeData.txt (BMP only,\n", o);
    fputs("// U+0000..U+FFFF — the whole domain a 16-bit `char` can reach). DO NOT EDIT — regenerate.\n", o);
    fputs("// Each method is a binary-search if-tree over coalesced UCD ranges (no runtime data array).\n", o);
    fputs("//\n", o);
    fputs("// Source data: the Unicode Character Database (UnicodeData.txt), a Unicode 10.0-or-later\n", o);
    fputs("// revision; the pinned copy in tools/unicode/ is the version of record. The UCD is under\n", o);
    fputs("// the Unicode License v3 (https://www.unicode.org/license.txt); see THIRD_PARTY.md.\n", o);
    fputs("class CharacterData {\n", o);
    emit_bool_method(o, "isLetter", is_letter);
    emit_bool_method(o, "isDigit", is_digit);
    emit_bool_method(o, "isUpperCase", is_upper);
    emit_bool_method(o, "isLowerCase", is_lower);
    emit_bool_method(o, "isTitleCase", is_title);
    emit_bool_method(o, "isDefined", is_defined);
    emit_map_method(o, "toUpperCase", up_map);
    emit_map_method(o, "toLowerCase", lo_map);
    emit_map_method(o, "toTitleCase", ti_map);
    /* Guarded by isDigit at the call site: the map tree returns `cp` for an unmapped code point
     * (the identity a CASE map wants), which is not a digit value. */
    emit_map_method(o, "digitValue", dig_val);
    fputs("}\n", o);
    return 0;
}
