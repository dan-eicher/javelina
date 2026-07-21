package java.lang;

public final class Character {
    public static final char MIN_VALUE = (char)0;       // spec MIN_VALUE, \u deferred
    public static final char MAX_VALUE = (char)0xffff;  // spec MAX_VALUE
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;
    private char value;
    public Character(char value) { this.value = value; }
    public native String toString();
    public boolean equals(Object obj) {
        if (obj instanceof Character) {
            return value == ((Character)obj).charValue();
        }
        return false;
    }
    public native int hashCode();
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
    public static int digit(char ch, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) return -1;
        int val = -1;
        if (ch >= '0' && ch <= '9')      val = ch - '0';
        else if (ch >= 'a' && ch <= 'z') val = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 10;
        return (val < radix) ? val : -1;
    }
    public static char forDigit(int digit, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX || digit < 0 || digit >= radix) return (char)0;
        if (digit < 10) return (char)('0' + digit);
        return (char)('a' + digit - 10);
    }
}
