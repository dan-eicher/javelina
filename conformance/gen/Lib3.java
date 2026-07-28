// Lib3 — the snippet library for JLS 1.0 chapter 3, Lexical Structure.
//
// Everything here is a LEAF: no holes, and expect() is the value the spec says the token
// denotes. That is the whole point of a lexical chapter — there is nothing to compose from,
// so the expectation has to come from the SPEC TEXT rather than from an operation.
//
// Where §3.10.1 states an identity outright (printed page 21: "The most negative hexadecimal
// and octal literals of type int are 0x80000000 and 020000000000, ... each of which
// represents the decimal value -2147483648"), the expectation is written in the OTHER
// spelling — hex/octal is rendered, decimal is expected. The two go through different code
// in javelinac, so their agreement is a real check rather than a tautology.
//
// Where §3.10.2 gives no second decimal spelling (3.14f has only one), the expectation is
// the IEEE 754 bit pattern through Float.intBitsToFloat / Double.longBitsToDouble. That is
// the spec's own device: printed page 23 pins Math.PI as
// Double.longBitsToDouble(0x400921FB54442D18L). A decimal literal re-typed into this file
// would be the same decimal-to-binary conversion under test on both sides of the comparison.
//
// A NOTE ON THIS FILE'S OWN SOURCE. It never contains a Unicode escape. A snippet that must
// RENDER one builds it from BS ("one backslash") — U("03a9") is the seven characters that
// javelinac will read as a Unicode escape, while this file holds only a backslash constant
// and the four hex digits. Nothing here is translated by the compiler that compiles it, so
// the rendered escape and the expected code point are genuinely independent. The same rule
// governs the comments: an escape in a comment is a translation-step-1 input too, and one
// for a line terminator would end the comment it sits in.
//
// WHY install() IS SPLIT INTO ONE METHOD PER SECTION. Not only taste. javelinac at -O0
// spills every reference argument to a body-local slot, and past roughly 150 of them in one
// method it emits a function that traps at its first instruction with no diagnostic at all
// (38 four-argument calls is enough; 37 is fine; -O is unaffected). One method per section
// keeps every body far under that, and it puts each group of tokens next to the sentence of
// the spec that fixes its value.
public class Lib3 {

    private Lib3() {}

    // ---- the characters this library has to spell without writing them -------------------

    private static final String BS = "\\";                        // exactly one backslash
    private static final String SQ = "'";
    private static final String DQ = "\"";
    private static final String HT = String.valueOf((char)  9);   // §3.6 horizontal tab
    private static final String LF = String.valueOf((char) 10);   // §3.4 line feed
    private static final String FF = String.valueOf((char) 12);   // §3.6 form feed
    private static final String CR = String.valueOf((char) 13);   // §3.4 carriage return

    /** The seven characters of a §3.3 Unicode escape: backslash, u, and four hex digits. */
    private static String U(String hex4) { return BS + "u" + hex4; }

    /** A parenthesised §3.10.4 character literal around already-escaped content. */
    private static String cl(String inner) { return "(" + SQ + inner + SQ + ")"; }

    /** A parenthesised §3.10.5 string literal around already-escaped content. */
    private static String sl(String inner) { return "(" + DQ + inner + DQ + ")"; }

    // ---- registration helpers, one per Val kind ------------------------------------------

    private static void i(Registry r, String id, String[] s, String text, int v) {
        r.register(new SnLex(id, s, "int", "(" + text + ")", Val.ofInt(v)));
    }
    private static void lg(Registry r, String id, String[] s, String text, long v) {
        r.register(new SnLex(id, s, "long", "(" + text + ")", Val.ofLong(v)));
    }
    private static void f(Registry r, String id, String[] s, String text, int bits) {
        r.register(new SnLex(id, s, "float", "(" + text + ")",
                             Val.ofFloat(Float.intBitsToFloat(bits))));
    }
    private static void d(Registry r, String id, String[] s, String text, long bits) {
        r.register(new SnLex(id, s, "double", "(" + text + ")",
                             Val.ofDouble(Double.longBitsToDouble(bits))));
    }
    private static void b(Registry r, String id, String[] s, String text, boolean v) {
        r.register(new SnLex(id, s, "boolean", "(" + text + ")", Val.ofBoolean(v)));
    }
    /** `expr` is the WHOLE parenthesised expression — several string snippets carry comments
     *  and line terminators between their tokens, so they cannot be built from a bare body. */
    private static void str(Registry r, String id, String[] s, String expr, String v) {
        r.register(new SnLex(id, s, "String", expr, Val.ofString(v)));
    }
    private static void ch(Registry r, String id, String[] s, String inner, int code) {
        r.register(new SnLex(id, s, "char", cl(inner), Val.ofChar((char) code)));
    }
    /** A §14.7 statement. It prints its own single line; `v` is that line. */
    private static void st(Registry r, String id, String[] s, String stmt, Val v) {
        r.register(new SnLex(id, s, "void", stmt, v));
    }

    // ═══════════════════════════════════════════════════════════════════════════════════════

    public static void install(Registry r) {
        unicode(r);                 // §3.1
        translations(r);            // §3.2
        unicodeEscapes(r);          // §3.3
        lineTerminators(r);         // §3.4
        tokenSeparation(r);         // §3.5
        whiteSpace(r);              // §3.6
        comments(r);                // §3.7
        identifiers(r);             // §3.8
        keywords(r);                // §3.9
        literalKinds(r);            // §3.10
        intDecimal(r);              // §3.10.1
        intOctal(r);
        intHex(r);
        longDecimal(r);
        longOctal(r);
        longHex(r);
        fpForms12(r);               // §3.10.2
        fpForms34(r);
        fpExtremes(r);
        booleanLiterals(r);         // §3.10.3
        charLiteralsPlain(r);       // §3.10.4
        charLiteralsEscaped(r);
        stringLiterals(r);          // §3.10.5
        stringInterning(r);
        namedEscapes(r);            // §3.10.6
        octalEscapes(r);
        nullLiteral(r);             // §3.10.7
        separators(r);              // §3.11
        operators(r);               // §3.12
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.1 Unicode — "Except for comments (§3.7), identifiers, and the contents of character
    // and string literals (§3.10.4, §3.10.5), all input elements (§3.5) in a Java program are
    // formed only from ASCII characters". In the three places a non-ASCII character IS legal
    // it carries its Unicode 2.0 code value.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void unicode(Registry r) {
        // GREEK CAPITAL LETTER OMEGA is U+03A9 = 937. The literal is spelled with an escape
        // (§3.3) because Emit gates the generated source to ASCII; the value is the code point.
        i(r, "lex3.uni.omega.code", Strs.of("3.1", "3.10.4", "3.3", "5.1.2"),
             "(int)" + cl(U("03a9")), 937);
        ch(r, "lex3.uni.latin1", Strs.of("3.1", "3.10.4", "3.3"), U("00e9"), 0xe9);
        str(r, "lex3.uni.str", Strs.of("3.1", "3.10.5", "3.3"),
              sl(U("03a9") + "A"), String.valueOf((char) 0x3a9) + "A");
        // A non-ASCII character inside a comment: legal, and discarded with the comment.
        str(r, "lex3.uni.comment", Strs.of("3.1", "3.7", "3.3"),
              "(" + DQ + "ok" + DQ + " /* " + U("2297") + " */)", "ok");
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.2 Lexical Translations — "Java always uses the longest possible translation at each
    // step, even if the result does not ultimately make a correct Java program." The spec's
    // own counter-example (a--b tokenizing as a, --, b) is by its own words "not part of any
    // grammatically correct Java program", so the coverable form of the rule is i+++j.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void translations(Registry r) {
        st(r, "lex3.longest.plusplus", Strs.of("3.2", "3.12", "15.13.2"),
             "{ int i = 3; int j = 4; System.out.println(i+++j); }", Val.ofInt(7));
        st(r, "lex3.longest.minusminus", Strs.of("3.2", "3.12", "15.14.3"),
             "{ int i = 3; int j = 4; System.out.println(i---j); }", Val.ofInt(-1));

        // §3.2 step 1 / §3.3 / §3.8: the escape is translated BEFORE tokenizing, so it can
        // spell a token — here the identifier A. This is the only shape that distinguishes a
        // real translation step from handling the escape inside the literal grammar.
        st(r, "lex3.step1.token", Strs.of("3.2", "3.3", "3.8"),
             "{ int " + U("0041") + " = 5; System.out.println(" + U("0041") + "); }",
             Val.ofInt(5));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.3 Unicode Escapes — UnicodeMarker: u | UnicodeMarker u, so any number of u's is one
    // escape. And "for each raw input character that is a backslash, input processing must
    // consider how many other \ characters contiguously precede it ... If this number is even,
    // then the \ is eligible to begin a Unicode escape; if the number is odd, then the \ is
    // not eligible to begin a Unicode escape."
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void unicodeEscapes(Registry r) {
        ch(r, "lex3.uesc.oneU",  Strs.of("3.3", "3.10.4"), U("0041"),       0x41);
        ch(r, "lex3.uesc.twoU",  Strs.of("3.3", "3.10.4"), BS + "uu0041",   0x41);
        ch(r, "lex3.uesc.fourU", Strs.of("3.3", "3.10.4"), BS + "uuuu0041", 0x41);
        ch(r, "lex3.uesc.ffff",  Strs.of("3.3", "3.10.4"), U("FFFF"),       0xFFFF);

        // The odd-backslash rule. The second backslash is preceded by one backslash, so it is
        // NOT eligible; the token is the escape for a backslash followed by the five ordinary
        // characters u005a. Six characters, and it is not "Z".
        str(r, "lex3.uesc.oddBackslash", Strs.of("3.3", "3.10.5", "3.10.6"),
              sl(BS + BS + "u005a"), BS + "u005a");

        // The spec's own worked example: a raw input of backslash backslash u 2 2 9 7 = and
        // then an escape for U+2297 "results in the eleven characters" — the first escape is
        // disabled by the odd backslash, the second is translated. The expected value is built
        // from code points, so no escape decides it.
        str(r, "lex3.uesc.evenOdd", Strs.of("3.3", "3.10.5", "3.10.6"),
              sl(BS + BS + "u2297=" + U("2297")),
              BS + "u2297=" + String.valueOf((char) 0x2297));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.4 Line Terminators — LF, CR, and CR LF each terminate exactly one line ("The two
    // characters CR immediately followed by LF are counted as one line terminator, not two"),
    // and the definition "also specifies the termination of the // form of a comment".
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void lineTerminators(Registry r) {
        str(r, "lex3.lineterm.lf", Strs.of("3.4", "3.7"),
              "(" + DQ + "a" + DQ + " + // ends at LF" + LF + DQ + "b" + DQ + ")", "ab");
        str(r, "lex3.lineterm.cr", Strs.of("3.4", "3.7"),
              "(" + DQ + "c" + DQ + " + // ends at CR" + CR + DQ + "d" + DQ + ")", "cd");
        str(r, "lex3.lineterm.crlf", Strs.of("3.4", "3.7"),
              "(" + DQ + "e" + DQ + " + // ends at CRLF" + CR + LF + DQ + "f" + DQ + ")", "ef");
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.5 Input Elements and Tokens — "White space (§3.6) and comments (§3.7) can serve to
    // separate tokens that, if adjacent, might be tokenized in another manner." Adjacent, the
    // two minus signs would be the single token -- (§3.12); separated, they are a binary
    // minus and a unary minus.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void tokenSeparation(Registry r) {
        i(r, "lex3.separate.comment", Strs.of("3.5", "3.7", "3.2", "3.12"), "1 -/* c */-1", 2);
        i(r, "lex3.separate.space",   Strs.of("3.5", "3.6", "3.2", "3.12"), "1 - -1", 2);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.6 White Space — "White space is defined as the ASCII space, horizontal tab, and form
    // feed characters, as well as line terminators (§3.4)." All four separate tokens.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void whiteSpace(Registry r) {
        i(r, "lex3.ws.sp",   Strs.of("3.6"), "1 + 2",                      3);
        i(r, "lex3.ws.ht",   Strs.of("3.6"), "1 +" + HT + "2",             3);
        i(r, "lex3.ws.ff",   Strs.of("3.6"), "1 +" + FF + "2",             3);
        i(r, "lex3.ws.lf",   Strs.of("3.6", "3.4"), "1 +" + LF + "2",      3);
        i(r, "lex3.ws.crlf", Strs.of("3.6", "3.4"), "1 +" + CR + LF + "2", 3);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.7 Comments — "Comments do not nest. /* and */ have no special meaning in comments
    // that begin with //. // has no special meaning in comments that begin with /* or /**."
    // The first snippet is the spec's own demonstration of all three at once.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void comments(Registry r) {
        // §3.1 makes a program "a sequence of Unicode characters" and §3.7 makes a comment's
        // content any InputCharacter, so a NUL inside a comment is ordinary text. §3.5 exempts
        // exactly one character from the input -- "the ASCII SUB character ... is ignored if it
        // is the last character" -- and this is not it.
        //
        // The escape is BUILT, never written: U("0000") is the six characters a §3.3 translation
        // turns into a NUL before the tokenizer runs, so this file (and the generated case, which
        // Emit gates to ASCII) holds only a backslash and four hex digits.
        //
        // WHAT THIS DOES NOT TEST, verified by removing the fix and watching it still pass: the
        // strlen truncation that java_source.h used to carry. ` ` is six ASCII bytes on
        // disk, so strlen measures the whole file; the NUL exists only in the buffer §3.3
        // produces, whose length is passed explicitly. The bug needed a RAW NUL byte in the
        // file, which Emit's ASCII gate forbids and should -- so that regression lives in
        // compiler/test, where a buffer and its length can be handed to the parser directly.
        // This template's claim is the narrower one it can actually make: a NUL inside a comment
        // is ordinary content and does not end the comment or the token stream.
        st(r, "lex3.comment.nul", Strs.of("3.7", "3.1", "3.3"),
             "{ int n = 1; /* a NUL, by escape: " + U("0000") + " */ n = n + 41;"
           + " System.out.println(n); }", Val.ofInt(42));

        // "As a result, the text /* this comment /* // /** ends here: */ is a single complete
        // comment."
        str(r, "lex3.comment.noNest", Strs.of("3.7"),
              "(" + DQ + "a" + DQ + " /* this comment /* // /** ends here: */ + "
                  + DQ + "b" + DQ + ")", "ab");
        str(r, "lex3.comment.doc", Strs.of("3.7"),
              "(" + DQ + "c" + DQ + " /** a documentation comment */ + " + DQ + "d" + DQ + ")",
              "cd");
        // "Note that /**/ is considered to be a documentation comment, while /* */ (with a
        // space between the asterisks) is a traditional comment." Both are discarded.
        str(r, "lex3.comment.empty", Strs.of("3.7"),
              "(" + DQ + "e" + DQ + " /**/ + " + DQ + "f" + DQ + ")", "ef");
        str(r, "lex3.comment.eol", Strs.of("3.7", "3.4"),
              "(" + DQ + "g" + DQ + " // a /* delimiter */ with no meaning here" + LF
                  + "+ " + DQ + "h" + DQ + ")", "gh");
        // "The lexical grammar implies that comments do not occur within character literals
        // (§3.10.4) or string literals (§3.10.5)."
        str(r, "lex3.comment.inString", Strs.of("3.7", "3.10.5"),
              "(" + DQ + "/*" + DQ + " + " + DQ + "*/" + DQ + " + " + DQ + "//" + DQ + ")",
              "/**///");
        ch(r, "lex3.comment.inChar", Strs.of("3.7", "3.10.4"), "/", 0x2f);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.8 Identifiers — "An identifier is an unlimited-length sequence of Java letters and
    // Java digits, the first of which must be a Java letter." The Java letters include "the
    // ASCII underscore and dollar sign"; the spec's example identifiers are String, i3,
    // MAX_VALUE and isLetterOrDigit.
    // ───────────────────────────────────────────────────────────────────────────────────────

    /** §3.9 — the 47 keywords, one rejection each, plus the boundary case that keeps them
     *  attributable. See Lib3Keywords / Sn3Keyword at the end of this file. */
    private static void keywords(Registry r) {
        String[] kws = Lib3Keywords.all();
        if (kws.length != 47)
            throw new RuntimeException("Lib3.keywords: p.18 lists 47, this has " + kws.length);
        for (int i = 0; i < kws.length; i++) r.register(new Sn3Keyword(kws[i]));
        r.register(new Sn3KeywordContains());
    }

    private static void identifiers(Registry r) {
        st(r, "lex3.ident.dollarUnderscore", Strs.of("3.8"),
             "{ int $x = 1; int _y = 2; int _ = 4; int $ = 8;"
             + " System.out.println($x + _y + _ + $); }", Val.ofInt(15));
        st(r, "lex3.ident.specExamples", Strs.of("3.8"),
             "{ int i3 = 1; int MAX_VALUE = 2; int isLetterOrDigit = 4;"
             + " System.out.println(i3 + MAX_VALUE + isLetterOrDigit); }", Val.ofInt(7));

        StringBuffer nb = new StringBuffer("v");
        for (int k = 0; k < 119; k++) nb.append('x');            // 120 Java letters
        String vlong = nb.toString();
        st(r, "lex3.ident.unlimited", Strs.of("3.8"),
             "{ int " + vlong + " = 9; System.out.println(" + vlong + "); }", Val.ofInt(9));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10 Literals — "A literal is the source code representation of a value of a primitive
    // type (§4.2), the String type (§4.3.3, §20.12), or the null type (§4.1)": IntegerLiteral,
    // FloatingPointLiteral, BooleanLiteral, CharacterLiteral, StringLiteral, NullLiteral. One
    // expression holding one of each of the six kinds.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void literalKinds(Registry r) {
        str(r, "lex3.literal.sixKinds",
              Strs.of("3.10", "3.10.1", "3.10.4", "3.10.7", "15.17.1.1"),
              "(" + DQ + DQ + " + 1 + 1.0 + true + " + SQ + "c" + SQ + " + "
                  + DQ + "s" + DQ + " + null)", "11.0truecsnull");
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.1 Integer Literals — DecimalNumeral | HexNumeral | OctalNumeral, each with an
    // optional l or L. Every expectation below is the value the SPEC states on printed
    // page 21, written in the opposite spelling from the one rendered.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void intDecimal(Registry r) {
        // DecimalNumeral: 0 | NonZeroDigit Digits_opt
        i(r, "lex3.int.dec.zero", Strs.of("3.10.1", "4.2.1"), "0",          0);
        i(r, "lex3.int.dec.two",  Strs.of("3.10.1"), "2",                   2);      // spec example
        i(r, "lex3.int.dec.1996", Strs.of("3.10.1"), "1996",                1996);   // spec example
        // "All decimal literals from 0 to 2147483647 may appear anywhere an int literal may
        // appear, but the literal 2147483648 may appear only as the operand of the unary
        // negation operator -."
        i(r, "lex3.int.dec.max",  Strs.of("3.10.1", "4.2.1"), "2147483647",  2147483647);
        i(r, "lex3.int.dec.min",  Strs.of("3.10.1", "4.2.1"), "-2147483648", -2147483648);
    }

    private static void intOctal(Registry r) {
        // OctalNumeral: 0 OctalDigit | OctalNumeral OctalDigit. "the numerals 0, 00, and 0x0
        // all represent exactly the same integer value."
        i(r, "lex3.int.oct.zero", Strs.of("3.10.1"), "00",             0);
        i(r, "lex3.int.oct.0372", Strs.of("3.10.1"), "0372",           250);  // spec example
        i(r, "lex3.int.oct.0777", Strs.of("3.10.1"), "0777",           511);
        // "The largest positive hexadecimal and octal literals of type int are 0x7fffffff and
        // 017777777777, respectively, which equal 2147483647 (2^31 - 1)."
        i(r, "lex3.int.oct.max",  Strs.of("3.10.1"), "017777777777",   2147483647);
        // "The most negative hexadecimal and octal literals of type int are 0x80000000 and
        // 020000000000, respectively, each of which represents the decimal value -2147483648."
        i(r, "lex3.int.oct.min",  Strs.of("3.10.1"), "020000000000",   -2147483648);
        // "The hexadecimal and octal literals 0xffffffff and 037777777777, respectively,
        // represent the decimal value -1."
        i(r, "lex3.int.oct.negOne", Strs.of("3.10.1"), "037777777777", -1);
    }

    private static void intHex(Registry r) {
        // HexNumeral: 0 x HexDigit | 0 X HexDigit | HexNumeral HexDigit. "each letter used as
        // a hexadecimal digit may be uppercase or lowercase."
        i(r, "lex3.int.hex.zero",     Strs.of("3.10.1"), "0x0",        0);
        i(r, "lex3.int.hex.dadaCafe", Strs.of("3.10.1"), "0xDadaCafe", -623195394); // spec example
        i(r, "lex3.int.hex.ff00ff",   Strs.of("3.10.1"), "0x00FF00FF", 16711935);   // spec example
        i(r, "lex3.int.hex.upperX",   Strs.of("3.10.1"), "0X1F",       31);
        i(r, "lex3.int.hex.mixed",    Strs.of("3.10.1"), "0xAbCdEf",   11259375);
        i(r, "lex3.int.hex.max",      Strs.of("3.10.1"), "0x7fffffff", 2147483647);
        i(r, "lex3.int.hex.min",      Strs.of("3.10.1"), "0x80000000", -2147483648);
        i(r, "lex3.int.hex.negOne",   Strs.of("3.10.1"), "0xffffffff", -1);
    }

    private static void longDecimal(Registry r) {
        // IntegerTypeSuffix: one of l L. "An integer literal is of type long if it is suffixed
        // with an ASCII letter L or l (ell); otherwise it is of type int."
        lg(r, "lex3.long.dec.zeroEll", Strs.of("3.10.1", "4.2.1"), "0l", 0L);        // spec example
        lg(r, "lex3.long.dec.2g",      Strs.of("3.10.1"), "2147483648L", 2147483648L);
        lg(r, "lex3.long.dec.max",     Strs.of("3.10.1", "4.2.1"),
             "9223372036854775807L",  9223372036854775807L);
        lg(r, "lex3.long.dec.min",     Strs.of("3.10.1", "4.2.1"),
             "-9223372036854775808L", -9223372036854775808L);
    }

    private static void longOctal(Registry r) {
        lg(r, "lex3.long.oct.0777",   Strs.of("3.10.1"), "0777L", 511L);             // spec example
        // "The largest positive hexadecimal and octal literals of type long are
        // 0x7fffffffffffffffL and 0777777777777777777777L, respectively, which equal
        // 9223372036854775807L (2^63 - 1)."
        lg(r, "lex3.long.oct.max",    Strs.of("3.10.1"),
             "0777777777777777777777L",  9223372036854775807L);
        // "The literals 0x8000000000000000L and 01000000000000000000000L are the most negative
        // long hexadecimal and octal literals, respectively."
        lg(r, "lex3.long.oct.min",    Strs.of("3.10.1"),
             "01000000000000000000000L", -9223372036854775808L);
        // "The hexadecimal and octal literals 0xffffffffffffffffL and
        // 01777777777777777777777L, respectively, represent the decimal value -1L."
        lg(r, "lex3.long.oct.negOne", Strs.of("3.10.1"),
             "01777777777777777777777L", -1L);
    }

    private static void longHex(Registry r) {
        lg(r, "lex3.long.hex.4g",     Strs.of("3.10.1"), "0x100000000L", 4294967296L); // spec ex.
        lg(r, "lex3.long.hex.c0b0",   Strs.of("3.10.1"), "0xC0B0L",      49328L);      // spec ex.
        lg(r, "lex3.long.hex.max",    Strs.of("3.10.1"),
             "0x7fffffffffffffffL",  9223372036854775807L);
        lg(r, "lex3.long.hex.min",    Strs.of("3.10.1"),
             "0x8000000000000000L",  -9223372036854775808L);
        lg(r, "lex3.long.hex.negOne", Strs.of("3.10.1"),
             "0xffffffffffffffffL",  -1L);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.2 Floating-Point Literals. The grammar has four alternatives; each is covered in
    // both the float and the double suffixing, with e and E, a signed exponent, and every one
    // of FloatTypeSuffix: one of f F d D. "A floating-point literal is of type float if it is
    // suffixed with an ASCII letter F or f; otherwise its type is double and it can optionally
    // be suffixed with an ASCII letter D or d."
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void fpForms12(Registry r) {
        // form 1: Digits . Digits_opt ExponentPart_opt FloatTypeSuffix_opt
        d(r, "lex3.double.f1.trailingDot", Strs.of("3.10.2", "4.2.3"), "2.",    0x4000000000000000L);
        d(r, "lex3.double.f1.pi",          Strs.of("3.10.2", "4.2.3"), "3.14",  0x40091eb851eb851fL);
        d(r, "lex3.double.f1.expD",        Strs.of("3.10.2", "4.2.3"), "1.5E2d",0x4062c00000000000L);
        f(r, "lex3.float.f1.trailingDot",  Strs.of("3.10.2", "4.2.3"), "2.f",   0x40000000);
        f(r, "lex3.float.f1.pi",           Strs.of("3.10.2", "4.2.3"), "3.14f", 0x4048f5c3);
        f(r, "lex3.float.f1.avogadro",     Strs.of("3.10.2", "4.2.3"),
             "6.022137e+23f", 0x66ff0c24);                          // spec example, signed exponent

        // form 2: . Digits ExponentPart_opt FloatTypeSuffix_opt
        d(r, "lex3.double.f2.leadingDot", Strs.of("3.10.2", "4.2.3"), ".3",    0x3fd3333333333333L);
        f(r, "lex3.float.f2.leadingDot",  Strs.of("3.10.2", "4.2.3"), ".3f",   0x3e99999a);
        f(r, "lex3.float.f2.expF",        Strs.of("3.10.2", "4.2.3"), ".5e1F", 0x40a00000);
    }

    private static void fpForms34(Registry r) {
        // form 3: Digits ExponentPart FloatTypeSuffix_opt
        d(r, "lex3.double.f3.e1",     Strs.of("3.10.2", "4.2.3"), "1e1",   0x4024000000000000L);
        d(r, "lex3.double.f3.negExp", Strs.of("3.10.2", "4.2.3"), "1e-9d", 0x3e112e0be826d695L);
        d(r, "lex3.double.f3.e137",   Strs.of("3.10.2", "4.2.3"), "1e137", 0x5c6132a095ce4930L);
        f(r, "lex3.float.f3.e1",      Strs.of("3.10.2", "4.2.3"), "1e1f",  0x41200000);
        f(r, "lex3.float.f3.posExp",  Strs.of("3.10.2", "4.2.3"), "1E+3f", 0x447a0000);

        // form 4: Digits ExponentPart_opt FloatTypeSuffix — here the suffix is what makes the
        // token a floating-point literal at all.
        f(r, "lex3.float.f4.zeroF",   Strs.of("3.10.2", "4.2.3"), "0f", 0x00000000);
        f(r, "lex3.float.f4.sevenF",  Strs.of("3.10.2", "4.2.3"), "7F", 0x40e00000);
        d(r, "lex3.double.f4.zeroD",  Strs.of("3.10.2", "4.2.3"), "0d", 0x0000000000000000L);
        d(r, "lex3.double.f4.sevenD", Strs.of("3.10.2", "4.2.3"), "7D", 0x401c000000000000L);
    }

    private static void fpExtremes(Registry r) {
        // "The largest positive finite float literal is 3.40282347e+38f. The smallest positive
        // finite nonzero literal of type float is 1.40239846e-45f." The latter rounds to a
        // DENORMALIZED number, which the spec says explicitly is not a compile-time error.
        f(r, "lex3.float.maxLiteral",  Strs.of("3.10.2", "4.2.3"), "3.40282347e+38f", 0x7f7fffff);
        f(r, "lex3.float.minDenormal", Strs.of("3.10.2", "4.2.3"), "1.40239846e-45f", 0x00000001);
        // "The largest positive finite double literal is 1.79769313486231570e+308. The smallest
        // positive finite nonzero literal of type double is 4.94065645841246544e-324."
        d(r, "lex3.double.maxLiteral",  Strs.of("3.10.2", "4.2.3"),
             "1.79769313486231570e+308", 0x7fefffffffffffffL);
        d(r, "lex3.double.minDenormal", Strs.of("3.10.2", "4.2.3"),
             "4.94065645841246544e-324", 0x0000000000000001L);

        // "A Java program can represent infinities without producing a compile-time error by
        // using constant expressions such as 1f/0f or -1d/0d."
        f(r, "lex3.float.infinity",     Strs.of("3.10.2", "4.2.3", "15.27"), "1f/0f", 0x7f800000);
        d(r, "lex3.double.negInfinity", Strs.of("3.10.2", "4.2.3", "15.27"), "-1d/0d",
             0xfff0000000000000L);
        d(r, "lex3.double.nan",         Strs.of("3.10.2", "4.2.3", "15.27"), "0.0d/0.0d",
             0x7ff8000000000000L);

        // The spec's own identity, printed page 23: "the value of
        // Double.longBitsToDouble(0x400921FB54442D18L) ... is equal to the value of Math.PI".
        // A hexadecimal long literal (§3.10.1) reaching a double through a bitcast.
        b(r, "lex3.double.piFromBits", Strs.of("3.10.2", "3.10.1", "4.2.3"),
             "Double.longBitsToDouble(0x400921FB54442D18L) == Math.PI", true);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.3 Boolean Literals — "The boolean type has two values, represented by the literals
    // true and false, formed from ASCII letters. A boolean literal is always of type boolean."
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void booleanLiterals(Registry r) {
        b(r, "lex3.bool.true",  Strs.of("3.10.3", "4.2.5"), "true",  true);
        b(r, "lex3.bool.false", Strs.of("3.10.3", "4.2.5"), "false", false);
        b(r, "lex3.bool.two",   Strs.of("3.10.3", "15.20.2"), "true != false", true);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.4 Character Literals — "A character literal is expressed as a character or an
    // escape sequence, enclosed in ASCII single quotes ... A character literal is always of
    // type char ... In Java, a character literal always represents exactly one character."
    // Every literal below is one of the spec's own listed examples.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void charLiteralsPlain(Registry r) {
        ch(r, "lex3.char.a",       Strs.of("3.10.4", "4.2.1"), "a", 0x61);   // spec example
        ch(r, "lex3.char.percent", Strs.of("3.10.4"), "%", 0x25);            // spec example
        ch(r, "lex3.char.space",   Strs.of("3.10.4"), " ", 0x20);
    }

    private static void charLiteralsEscaped(Registry r) {
        ch(r, "lex3.char.tab",       Strs.of("3.10.4", "3.10.6"), BS + "t",   0x09); // spec ex.
        ch(r, "lex3.char.backslash", Strs.of("3.10.4", "3.10.6"), BS + BS,    0x5c); // spec ex.
        ch(r, "lex3.char.quote",     Strs.of("3.10.4", "3.10.6"), BS + SQ,    0x27); // spec ex.
        ch(r, "lex3.char.omega",     Strs.of("3.10.4", "3.3", "3.1"), U("03a9"), 0x3a9);
        ch(r, "lex3.char.max",       Strs.of("3.10.4", "3.3"), U("FFFF"),     0xFFFF);
        ch(r, "lex3.char.oct177",    Strs.of("3.10.4", "3.10.6"), BS + "177", 0x7f); // spec ex.
        // "always represents exactly one character" — even at the top of the char range.
        i(r, "lex3.char.oneChar", Strs.of("3.10.4", "15.17.1.1"),
             "(" + DQ + DQ + " + " + SQ + U("FFFF") + SQ + ").length()", 1);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.5 String Literals — "A string literal consists of zero or more characters enclosed
    // in double quotes. Each character may be represented by an escape sequence. A string
    // literal is always of type String (§4.3.3, §20.12). A string literal always refers to the
    // same instance (§4.3.1) of class String."
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void stringLiterals(Registry r) {
        str(r, "lex3.str.empty",   Strs.of("3.10.5"), sl(""), "");                   // spec ex.
        str(r, "lex3.str.quote",   Strs.of("3.10.5", "3.10.6"), sl(BS + DQ), DQ);    // spec ex.
        str(r, "lex3.str.sixteen", Strs.of("3.10.5"), sl("This is a string"),
              "This is a string");                                                   // spec ex.
        i(r, "lex3.str.length16",  Strs.of("3.10.5"),
             sl("This is a string") + ".length()", 16);        // "a string containing 16 characters"
        // "actually a string-valued constant expression, formed from two string literals"
        str(r, "lex3.str.twoLine", Strs.of("3.10.5", "15.27", "15.17.1"),
              "(" + DQ + "This is a " + DQ + " + " + DQ + "two-line string" + DQ + ")",
              "This is a two-line string");                                          // spec ex.
        str(r, "lex3.str.escapes", Strs.of("3.10.5", "3.10.6"),
              sl("a" + BS + "tb" + BS + BS + "c" + BS + DQ + "d" + BS + SQ + "e"),
              "a" + HT + "b" + BS + "c" + DQ + "d" + SQ + "e");
        str(r, "lex3.str.octalEscapes", Strs.of("3.10.5", "3.10.6"),
              sl(BS + "101" + BS + "102" + BS + "103"), "ABC");
    }

    /** The four comparisons of the spec's own §3.10.5 test program that live inside a single
     *  compilation unit. (Its other two comparisons are cross-class and cross-package; a
     *  generated case is one class in one file, so they have no home here.) */
    private static void stringInterning(Registry r) {
        // "Literal strings within the same class (§8) in the same package (§7) represent
        // references to the same String object."
        b(r, "lex3.str.internLiteral", Strs.of("3.10.5", "4.3.1"),
             DQ + "Hello" + DQ + " == " + DQ + "Hello" + DQ, true);
        // "Strings computed by constant expressions (§15.27) are computed at compile time and
        // then treated as if they were literals."
        b(r, "lex3.str.internConstExpr", Strs.of("3.10.5", "4.3.1", "15.27"),
             DQ + "Hello" + DQ + " == (" + DQ + "Hel" + DQ + " + " + DQ + "lo" + DQ + ")", true);
        // "Strings computed at run time are newly created and therefore distinct."
        st(r, "lex3.str.internRuntime", Strs.of("3.10.5", "4.3.1"),
             "{ String lo = " + DQ + "lo" + DQ + "; System.out.println("
             + DQ + "Hello" + DQ + " == (" + DQ + "Hel" + DQ + " + lo)); }", Val.ofBoolean(false));
        // "The result of explicitly interning a computed string is the same string as any
        // pre-existing literal string with the same contents."
        // §20.12, not §20.12.47: the ToC — which is the inventory's basis, being ordered,
        // numbered and page-stamped — lists chapter 20 one row per CLASS. intern() has a
        // numbered section in the body but none the ledger tracks, so a claim on it names a
        // section that does not exist.
        st(r, "lex3.str.internExplicit", Strs.of("3.10.5", "4.3.1", "20.12"),
             "{ String lo = " + DQ + "lo" + DQ + "; System.out.println("
             + DQ + "Hello" + DQ + " == (" + DQ + "Hel" + DQ + " + lo).intern()); }",
             Val.ofBoolean(true));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.6 Escape Sequences for Character and String Literals — the eight named escapes
    // with the code points the spec writes beside them in the EscapeSequence production, and
    // OctalEscape in all three of its productions.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void namedEscapes(Registry r) {
        ch(r, "lex3.esc.backspace", Strs.of("3.10.6"), BS + "b", 0x0008);  // U+0008 backspace BS
        ch(r, "lex3.esc.tab",       Strs.of("3.10.6"), BS + "t", 0x0009);  // U+0009 horizontal tab
        ch(r, "lex3.esc.linefeed",  Strs.of("3.10.6"), BS + "n", 0x000a);  // U+000A linefeed LF
        ch(r, "lex3.esc.formfeed",  Strs.of("3.10.6"), BS + "f", 0x000c);  // U+000C form feed FF
        ch(r, "lex3.esc.creturn",   Strs.of("3.10.6"), BS + "r", 0x000d);  // U+000D carriage return
        ch(r, "lex3.esc.dquote",    Strs.of("3.10.6"), BS + DQ,  0x0022);  // U+0022 double quote
        ch(r, "lex3.esc.squote",    Strs.of("3.10.6"), BS + SQ,  0x0027);  // U+0027 single quote
        ch(r, "lex3.esc.backslash", Strs.of("3.10.6"), BS + BS,  0x005c);  // U+005C backslash
    }

    private static void octalEscapes(Registry r) {
        // OctalEscape: \ OctalDigit
        ch(r, "lex3.esc.oct1.zero",  Strs.of("3.10.6"), BS + "0",   0);
        ch(r, "lex3.esc.oct1.seven", Strs.of("3.10.6"), BS + "7",   7);
        // OctalEscape: \ OctalDigit OctalDigit
        ch(r, "lex3.esc.oct2.zero",  Strs.of("3.10.6"), BS + "00",  0);
        ch(r, "lex3.esc.oct2.max",   Strs.of("3.10.6"), BS + "77",  63);
        // OctalEscape: \ ZeroToThree OctalDigit OctalDigit
        ch(r, "lex3.esc.oct3.zero",  Strs.of("3.10.6"), BS + "000", 0);
        ch(r, "lex3.esc.oct3.A",     Strs.of("3.10.6"), BS + "101", 0x41);
        // "Octal escapes ... can express only Unicode values U+0000 through U+00FF."
        ch(r, "lex3.esc.oct3.max",   Strs.of("3.10.6"), BS + "377", 0xff);
        // The identity the spec states outright: '\101' == 'A'.
        b(r, "lex3.esc.octEqualsA", Strs.of("3.10.6", "3.10.4"),
             SQ + BS + "101" + SQ + " == " + SQ + "A" + SQ, true);
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.10.7 The Null Literal — "The null type has one value, the null reference, represented
    // by the literal null, which is formed from ASCII characters. A null literal is always of
    // the null type." §4.1: the null reference is assignable to a variable of any reference
    // type — class, interface or array.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void nullLiteral(Registry r) {
        b(r, "lex3.null.isOneValue", Strs.of("3.10.7", "4.1", "15.20.3"), "null == null", true);
        b(r, "lex3.null.notAString", Strs.of("3.10.7", "4.1", "15.20.3"),
             DQ + "x" + DQ + " == null", false);
        st(r, "lex3.null.toString", Strs.of("3.10.7", "4.1"),
             "{ String s = null; System.out.println(s); }", Val.ofString(null));
        st(r, "lex3.null.everyRefType", Strs.of("3.10.7", "4.1", "15.17.1.1"),
             "{ Object o = null; int[] a = null; String s = null; Cloneable c = null;"
             + " System.out.println(" + DQ + DQ + " + o + a + s + c); }",
             Val.ofString("nullnullnullnull"));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.11 Separators — "The following nine ASCII characters are the Java separators
    // (punctuators): ( ) { } [ ] ; , ."   All nine appear in this one statement.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void separators(Registry r) {
        st(r, "lex3.separator.allNine", Strs.of("3.11"),
             "{ int[] a = new int[2]; int b, c; a[0] = 3; b = 4; c = a[0] + b;"
             + " System.out.println(Math.abs(-c)); }", Val.ofInt(7));
    }

    // ───────────────────────────────────────────────────────────────────────────────────────
    // §3.12 Operators — "The following 37 tokens are the Java operators". Between them the
    // eight snippets below use every one, including the three- and four-character forms >>>
    // and >>>=, whose whole point is that the longest match (§3.2) wins.
    // ───────────────────────────────────────────────────────────────────────────────────────

    private static void operators(Registry r) {
        //  >>>
        i(r, "lex3.op.ushr", Strs.of("3.12", "3.2", "15.18"), "(-1) >>> 28", 15);
        //  >>>=
        st(r, "lex3.op.ushrAssign", Strs.of("3.12", "3.2", "15.25.2"),
             "{ int x = -1; x >>>= 28; System.out.println(x); }", Val.ofInt(15));
        //  = += -= *= /= %= &= |= ^= <<= >>=
        st(r, "lex3.op.compoundAssign", Strs.of("3.12", "15.25.2"),
             "{ int x = 100; x += 5; x -= 3; x *= 2; x /= 4; x %= 40; x &= 60; x |= 3;"
             + " x ^= 5; x <<= 2; x >>= 1; System.out.println(x); }", Val.ofInt(28));
        //  & | ^ ~ << >>
        st(r, "lex3.op.bitwise", Strs.of("3.12", "15.21", "15.18"),
             "{ int x = 12 & 10; x = x | (12 ^ 10); x = x ^ 3; x = ~x; x = x << 2;"
             + " x = x >> 1; System.out.println(x); }", Val.ofInt(-28));
        //  ++ --
        // The LEAVES, not the parents: §15.13 is "Postfix Expressions" and §15.14 "Unary
        // Operators", and each of the four operators here has its own section. Claiming a
        // parent marks its whole subtree covered on the strength of one child — and §15.13 is
        // N/A besides, being a grammar production, so the join rejects the claim outright.
        st(r, "lex3.op.incDec", Strs.of("3.12", "15.13.2", "15.13.3", "15.14.1", "15.14.2"),
             "{ int i = 5; i++; ++i; i--; --i; System.out.println(i); }", Val.ofInt(5));
        //  < ? :
        i(r, "lex3.op.conditional", Strs.of("3.12", "15.24"), "(1 < 2) ? 10 : 20", 10);
        //  > <= >= == != ! && ||
        b(r, "lex3.op.relational", Strs.of("3.12", "15.19", "15.20"),
             "(1 > 0) & (1 <= 1) & (2 >= 2) & (1 == 1) & (1 != 2) & !(false)"
             + " & (true && true) & (false || true)", true);
        //  + - * / %
        st(r, "lex3.op.arithmetic", Strs.of("3.12", "15.16", "15.17"),
             "{ int x = 7 + 3 - 2 * 4 / 2 % 3; System.out.println(x); }", Val.ofInt(9));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════
// The one Snippet class this library needs.
//
// Chapter 3 is a chapter of TOKENS: every snippet is a leaf, and a leaf's expect() takes no
// holes, so there is exactly one shape — (id, sections, type, source text, value). Writing
// 140 single-instance classes for it would be 140 copies of the same six methods, and the
// thing that actually varies — which sentence of the spec fixes which value — would be
// buried in them. Here it is visible in the install methods, one line per token.
// ═══════════════════════════════════════════════════════════════════════════════════════

/** A leaf snippet: fixed source text, fixed value, no holes. */
class SnLex implements Snippet {

    // Constructor-assigned, so not final (§8.3.1.2 wants the initializer in the declarator).
    private String   id;
    private String[] secs;
    private String   type;
    private String   text;
    private Val      value;

    SnLex(String id, String[] secs, String type, String text, Val value) {
        if (text == null)  throw new RuntimeException("SnLex " + id + ": no source text");
        if (value == null) throw new RuntimeException("SnLex " + id + ": no value");
        this.id = id; this.secs = secs; this.type = type; this.text = text; this.value = value;
    }

    public String   id()               { return id; }
    public String[] sections()         { return secs; }
    public String   type()             { return type; }
    public String[] holeTypes()        { return Strs.none(); }
    public String   render(String[] h) { return text; }
    public Val      expect(Val[] h)    { return value; }
}

/** §3.9 keywords, transcribed from p.18's table.
 *
 *  "The following character sequences, formed from ASCII letters, are reserved for use as
 *  keywords and cannot be used as identifiers (§3.8)" -- then the table, read left to right,
 *  top to bottom. FORTY-SEVEN entries, including `const` and `goto`, which the language
 *  reserves without using: that is why they are here and not available as names.
 *
 *  One rejection template per keyword rather than a sample. The list is closed and the spec
 *  prints all of it, so any subset would be a subset I chose -- and each id is distinct, so a
 *  missing keyword is a missing row in SECTIONS.tsv rather than an invisible gap. It is NOT a
 *  cardinality.tsv row: the spec prints the table but never names the number 47, and that file
 *  takes only counts its own text states (the same reason §4.4's eleven positions and §2.4's
 *  eight `for` forms are not rows).
 *
 *  ATTRIBUTION. javelinac answers all 47 with a bare "parse error", which a typo would also
 *  produce, so the diagnostic alone cannot show the program was rejected for BEING A KEYWORD.
 *  Two things supply that instead: each program is otherwise well formed, so the keyword is its
 *  only defect; and t3.kw.contains below compiles and RUNS the same shape with names that merely
 *  contain keywords. If the rejections came from the surrounding shape, that one would fail. */
class Lib3Keywords {

    static String[] all() {
        String[] k = {
            "abstract", "boolean", "break",   "byte",       "case",      "catch",  "char",
            "class",    "const",   "continue",
            "default",  "do",      "double",  "else",       "extends",   "final",  "finally",
            "float",    "for",     "goto",
            "if",       "implements", "import", "instanceof", "int",     "interface", "long",
            "native",   "new",     "package",
            "private",  "protected", "public", "return",    "short",     "static", "super",
            "switch",   "synchronized", "this",
            "throw",    "throws",  "transient", "try",      "void",      "volatile", "while"
        };
        return k;
    }
}

/** One keyword, used where §3.8 would allow an identifier. */
class Sn3Keyword implements Snippet {

    private String kw;

    Sn3Keyword(String kw) { this.kw = kw; }

    public String   id()        { return "t3.kw." + kw; }
    public String[] sections()  { return Strs.of("3.9"); }
    public String   type()      { return Registry.REJECT; }
    public String[] holeTypes() { return Strs.none(); }

    public String render(String[] h) {
        return "class T3Keyword {\n"
             + "    int " + kw + " = 1;\n"
             + "}";
    }
    public Val expect(Val[] h) { return Val.compileError("parse error"); }
}

/** §3.9 against §3.8: a keyword is a keyword only WHOLE. `classroom` contains `class`, `ifs`
 *  contains `if`, and both are ordinary identifiers, because §3.5's longest-match tokenization
 *  produces one Identifier token rather than a keyword followed by letters.
 *
 *  This is what makes the 47 rejections attributable: it is the same declaration shape they use,
 *  and it compiles and runs. */
class Sn3KeywordContains implements Snippet, Declaring {

    public String   id()        { return "t3.kw.contains"; }
    public String[] sections()  { return Strs.of("3.9", "3.5"); }
    public String   type()      { return "void"; }
    public String[] holeTypes() { return Strs.of("int"); }
    public String[] imports()   { return Strs.none(); }

    public String[] decls() {
        String[] d = { "class T3Contains {\n"
                     + "    int classroom = 1;\n"
                     + "    int ifs       = 2;\n"
                     + "    int forth     = 3;\n"
                     + "    int newer     = 4;\n"
                     + "    int thisIsFine = 5;\n"
                     + "}" };
        return d;
    }

    public String render(String[] h) {
        return "{ T3Contains o = new T3Contains();"
             + " System.out.println(o.classroom + o.ifs + o.forth + o.newer + o.thisIsFine"
             + " + (" + h[0] + ")); }";
    }

    public Val expect(Val[] h) { return Val.ofInt(15 + h[0].asInt()); }
}
