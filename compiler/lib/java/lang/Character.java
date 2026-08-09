package java.lang;

public final class Character {
    // §20.5.1 MIN_VALUE, spelled (char)0 rather than as a unicode escape. Writing the escape
    // in a COMMENT is not free: §3.2 translates escapes over the raw stream BEFORE comments
    // are discarded, so a backslash-u with fewer than four hex digits after it is a
    // compile-time error wherever it appears. This line used to carry exactly that.
    public static final char MIN_VALUE = (char)0;
    public static final char MAX_VALUE = (char)0xffff;  // spec MAX_VALUE
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;
    private char value;
    public Character(char value) { this.value = value; }
    // §20.5 — a String of length 1 holding this Character's value. NOT a native: nothing here is
    // an environment edge, so declaring it native only produces an import the host contract does
    // not cover, which traps at the call.
    public String toString() { return String.valueOf(value); }
    public boolean equals(Object obj) {
        if (obj instanceof Character) {
            return value == ((Character)obj).charValue();
        }
        return false;
    }
    // §20.5 — the value itself, which is what makes equal Characters hash equal (§20.1.4's
    // contract, given `equals` above compares values). Same reason as toString: not an
    // environment edge, so not a native.
    public int hashCode() { return (int)value; }
    public char charValue() { return value; }
    // §20.5 classification/case — delegated to the generated CharacterData (BMP Unicode range tables).
    public static boolean isLowerCase(char ch) { return CharacterData.isLowerCase(ch); }
    public static boolean isUpperCase(char ch) { return CharacterData.isUpperCase(ch); }
    public static boolean isDigit(char ch) { return CharacterData.isDigit(ch); }
    public static boolean isLetter(char ch) { return CharacterData.isLetter(ch); }
    public static boolean isLetterOrDigit(char ch) { return CharacterData.isLetter(ch) || CharacterData.isDigit(ch); }
    public static boolean isJavaLetter(char ch) { return CharacterData.isLetter(ch) || ch == '_' || ch == '$'; }
    public static boolean isJavaLetterOrDigit(char ch) { return isJavaLetter(ch) || CharacterData.isDigit(ch); }
    public static boolean isSpace(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\f' || ch == '\r'; }
    public static char toLowerCase(char ch) { return (char) CharacterData.toLowerCase(ch); }
    public static char toUpperCase(char ch) { return (char) CharacterData.toUpperCase(ch); }
    public static boolean isDefined(char ch) { return CharacterData.isDefined(ch); }
    public static boolean isTitleCase(char ch) { return CharacterData.isTitleCase(ch); }
    public static char toTitleCase(char ch) { return (char) CharacterData.toTitleCase(ch); }
    // §20.5.9 — the ASCII forms are the alphanumeric ones (a letter stands for 10..35, which is
    // why only ASCII letters qualify), but a DECIMAL digit is a decimal digit in every script:
    // Tamil '௧' is 1 and Tibetan '༧' is 7, and both are legal in any radix above their
    // value. The generated table carries the UCD's decimal value, guarded by isDigit because the
    // map tree answers `cp` for a code point it does not cover.
    public static int digit(char ch, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) return -1;
        int val = -1;
        if (ch >= '0' && ch <= '9')      val = ch - '0';
        else if (ch >= 'a' && ch <= 'z') val = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 10;
        else if (CharacterData.isDigit(ch)) val = CharacterData.digitValue(ch);
        return (val >= 0 && val < radix) ? val : -1;
    }
    public static char forDigit(int digit, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX || digit < 0 || digit >= radix) return (char)0;
        if (digit < 10) return (char)('0' + digit);
        return (char)('a' + digit - 10);
    }
}
