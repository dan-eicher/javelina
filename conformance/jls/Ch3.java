// Ch3 — JLS chapter 3, Lexical Structure. One method per leaf section; the `// JLS <n>`
// marker is read by conformance/join-ledger.sh, so it is load-bearing, not decoration.
//
// §3.8's identifiers are the interesting ones here: a Java letter is ANY Unicode letter, so
// `λ` and `café` are ordinary names — and `☃` is not, because it is a symbol. Both directions
// are asserted; a grammar that over-accepts is as wrong as one that under-accepts.
//
// What CANNOT be asserted from inside a running program is the rejection half: `int ☃ = 5;`
// is a compile-time error, so it cannot appear in a file that must compile. Those live in
// test_exec.c, which can assert that a program fails to build.

public class Ch3 {

    // JLS 3.8
    static void s3_8() {
        // A Java letter is any Unicode letter (§20.5.17), plus '_' and '$'.
        int lambda = 1;          // ASCII, the ordinary case
        int _under = 2;
        int $dollar = 3;
        Check.eq("3.8", "an ASCII identifier is a Java letter sequence", lambda + _under + $dollar, 6);

        int λ = 10;          // GREEK SMALL LETTER LAMDA, written as an escape (§3.3)
        int café = 20;           // LATIN SMALL LETTER E WITH ACUTE, written literally
        int Ω = 30;              // GREEK CAPITAL LETTER OMEGA
        Check.eq("3.8", "a Unicode letter is a legal identifier character", λ + café + Ω, 60);

        // §3.8: "Two identifiers are the same only if they are identical, that is, have the
        // same Unicode character for each letter or digit." LATIN A, GREEK ALPHA and CYRILLIC
        // A look alike and are three distinct names.
        int A = 1;               // LATIN CAPITAL LETTER A
        int Α = 2;               // GREEK CAPITAL LETTER ALPHA   (U+0391)
        int А = 4;               // CYRILLIC CAPITAL LETTER A    (U+0410)
        Check.eq("3.8", "look-alike letters from different scripts are DISTINCT names", A + Α + А, 7);

        // A digit may not start an identifier but may continue one, and the digit need not be
        // ASCII either — §3.8's JavaLetterOrDigit is Character.isJavaLetterOrDigit.
        int a1 = 5, a٢ = 6;      // ARABIC-INDIC DIGIT TWO (U+0662) as a continue character
        Check.eq("3.8", "a Unicode digit continues an identifier", a1 + a٢, 11);
    }

    // JLS 3.9
    static void s3_9() {
        // The 47 reserved keywords cannot be identifiers. `const` and `goto` are reserved and
        // unused, which is why they are keywords rather than available names — a program using
        // one as a name is rejected, and that rejection is asserted in test_exec.c.
        // Observable here: a name that merely CONTAINS a keyword is fine.
        int classroom = 1, ifs = 2, forth = 3, newer = 4;
        Check.eq("3.9", "a keyword is only a keyword whole — `classroom` is a name",
                 classroom + ifs + forth + newer, 10);
    }

    public static void run() {
        s3_8();
        s3_9();
    }
}
