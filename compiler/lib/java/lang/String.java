package java.lang;

public final class String {
    // The backing UTF-16 code units (JLS 1.0 §20.12 / E6: String is a GC overlay
    // on a char[]). char is unsigned 16-bit → (array (mut i16)), read get_u.
    private char[] value;

    public String() { this.value = new char[0]; }
    public String(String value) { this.value = value.value; }   // immutable → may share
    public String(StringBuffer buffer) { this.value = buffer.toString().value; }
    public String(char[] value) {
        // §20.12.10: the array is COPIED — a String is immutable, so it must not
        // alias a caller-held char[] that could later be mutated.
        char[] v = new char[value.length];
        int i = 0;
        while (i < value.length) { v[i] = value[i]; i = i + 1; }
        this.value = v;
    }
    public String(char[] value, int offset, int count) {
        char[] v = new char[count];
        int i = 0;
        while (i < count) { v[i] = value[offset + i]; i = i + 1; }
        this.value = v;
    }
    public String(byte[] ascii, int hibyte) { this(ascii, hibyte, 0, ascii.length); }
    public String(byte[] ascii, int hibyte, int offset, int count) {
        // §20.12.5 deprecated hibyte ctor: char = (hibyte<<8) | (ascii[i] & 0xFF).
        char[] v = new char[count];
        int hi = hibyte << 8;
        int i = 0;
        while (i < count) { v[i] = (char) (hi | (ascii[offset + i] & 0xff)); i = i + 1; }
        this.value = v;
    }
    public String toString() { return this; }
    public boolean equals(Object anObject) {
        if (this == anObject) return true;
        if (anObject instanceof String) {
            String other = (String) anObject;
            if (value.length != other.value.length) return false;
            int i = 0;
            while (i < value.length) {
                if (value[i] != other.value[i]) return false;
                i = i + 1;
            }
            return true;
        }
        return false;
    }
    public int hashCode() {
        int h = 0;
        int i = 0;
        while (i < value.length) {
            h = 31 * h + value[i];
            i = i + 1;
        }
        return h;
    }
    public int length() { return value.length; }
    // §20.12.12: "If the index argument is negative or not less than the length
    // (§20.12.11) of this string, then an IndexOutOfBoundsException is thrown."
    // The raw array read already throws one — ArrayIndexOutOfBoundsException — so
    // the letter of §20.12.12 was met. But StringIndexOutOfBoundsException is the
    // subclass a String index error carries, and it is what code catches: this
    // library's own FloatingDecimal.readJavaFormatString wraps its charAt scan in
    // `catch (StringIndexOutOfBoundsException)`, and against the array's exception
    // that catch never fired — Double.valueOf("1e") escaped as
    // ArrayIndexOutOfBoundsException instead of NumberFormatException.
    public char charAt(int index) {
        if (index < 0 || index >= value.length)
            throw new StringIndexOutOfBoundsException(index);
        return value[index];
    }
    public void getBytes(int srcBegin, int srcEnd, byte dst[], int dstBegin) throws NullPointerException, IndexOutOfBoundsException {
        // §20.12.16 (deprecated): the low 8 bits of each char become a byte.
        if (srcBegin < 0) throw new StringIndexOutOfBoundsException(srcBegin);
        if (srcEnd > value.length) throw new StringIndexOutOfBoundsException(srcEnd);
        if (srcBegin > srcEnd) throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        int i = srcBegin;
        int j = dstBegin;
        while (i < srcEnd) { dst[j] = (byte) value[i]; i = i + 1; j = j + 1; }
    }
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) throws NullPointerException, IndexOutOfBoundsException {
        if (srcBegin < 0) throw new StringIndexOutOfBoundsException(srcBegin);
        if (srcEnd > value.length) throw new StringIndexOutOfBoundsException(srcEnd);
        if (srcBegin > srcEnd) throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        int i = srcBegin;
        int j = dstBegin;
        while (i < srcEnd) { dst[j] = value[i]; i = i + 1; j = j + 1; }
    }
    public char[] toCharArray() {
        char[] result = new char[value.length];
        int i = 0;
        while (i < value.length) { result[i] = value[i]; i = i + 1; }
        return result;
    }
    public boolean equalsIgnoreCase(String anotherString) {   // §20.12.16, case-folded via Character
        if (this == anotherString) return true;
        if (anotherString == null || anotherString.value.length != value.length) return false;
        return regionMatches(true, 0, anotherString, 0, value.length);
    }
    public int compareTo(String anotherString) throws NullPointerException {
        int len1 = value.length;
        int len2 = anotherString.length();
        int n = len1 < len2 ? len1 : len2;
        int i = 0;
        while (i < n) {
            int c1 = value[i];
            int c2 = anotherString.charAt(i);
            if (c1 != c2) return c1 - c2;
            i = i + 1;
        }
        return len1 - len2;
    }
    public boolean regionMatches(int toffset, String other, int ooffset, int len) throws NullPointerException {
        if (toffset < 0 || ooffset < 0 || toffset + len > value.length || ooffset + len > other.length()) return false;
        int i = 0;
        while (i < len) { if (value[toffset + i] != other.charAt(ooffset + i)) return false; i = i + 1; }
        return true;
    }
    public boolean regionMatches(boolean ignoreCase, int toffset, String other, int ooffset, int len) throws NullPointerException {
        if (toffset < 0 || ooffset < 0 || toffset + len > value.length || ooffset + len > other.length()) return false;
        int i = 0;
        while (i < len) {
            char c1 = value[toffset + i];
            char c2 = other.charAt(ooffset + i);
            if (c1 != c2) {
                if (!ignoreCase) return false;
                // §20.12.24: compare via BOTH upper- and lower-case (some scripts need both).
                char u1 = Character.toUpperCase(c1);
                char u2 = Character.toUpperCase(c2);
                if (u1 != u2 && Character.toLowerCase(u1) != Character.toLowerCase(u2)) return false;
            }
            i = i + 1;
        }
        return true;
    }
    public boolean startsWith(String prefix) throws NullPointerException { return startsWith(prefix, 0); }
    public boolean startsWith(String prefix, int toffset) throws NullPointerException {
        int pn = prefix.length();
        if (toffset < 0 || toffset + pn > value.length) return false;
        int i = 0;
        while (i < pn) { if (value[toffset + i] != prefix.charAt(i)) return false; i = i + 1; }
        return true;
    }
    public boolean endsWith(String suffix) throws NullPointerException { return startsWith(suffix, value.length - suffix.length()); }
    public int indexOf(int ch) { return indexOf(ch, 0); }
    public int indexOf(int ch, int fromIndex) {
        int len = value.length;
        int i = fromIndex < 0 ? 0 : fromIndex;
        while (i < len) { if (value[i] == ch) return i; i = i + 1; }
        return -1;
    }
    public int indexOf(String str) throws NullPointerException { return indexOf(str, 0); }
    public int indexOf(String str, int fromIndex) throws NullPointerException {
        int len = value.length;
        int slen = str.length();
        int start = fromIndex < 0 ? 0 : fromIndex;
        if (slen == 0) return start < len ? start : len;
        int i = start;
        while (i + slen <= len) {
            int j = 0;
            while (j < slen && value[i + j] == str.charAt(j)) j = j + 1;
            if (j == slen) return i;
            i = i + 1;
        }
        return -1;
    }
    public int lastIndexOf(int ch) { return lastIndexOf(ch, value.length - 1); }
    public int lastIndexOf(int ch, int fromIndex) {
        int i = fromIndex >= value.length ? value.length - 1 : fromIndex;
        while (i >= 0) { if (value[i] == ch) return i; i = i - 1; }
        return -1;
    }
    public int lastIndexOf(String str) throws NullPointerException { return lastIndexOf(str, value.length); }
    public int lastIndexOf(String str, int fromIndex) throws NullPointerException {
        int len = value.length;
        int slen = str.length();
        int max = len - slen;
        int start = fromIndex > max ? max : fromIndex;
        int i = start;
        while (i >= 0) {
            int j = 0;
            while (j < slen && value[i + j] == str.charAt(j)) j = j + 1;
            if (j == slen) return i;
            i = i - 1;
        }
        return -1;
    }
    public String substring(int beginIndex) { return substring(beginIndex, value.length); }
    public String substring(int beginIndex, int endIndex) {
        if (beginIndex < 0) throw new StringIndexOutOfBoundsException(beginIndex);
        if (endIndex > value.length) throw new StringIndexOutOfBoundsException(endIndex);
        if (beginIndex > endIndex) throw new StringIndexOutOfBoundsException(endIndex - beginIndex);
        return new String(value, beginIndex, endIndex - beginIndex);
    }
    public String concat(String str) throws NullPointerException {
        int otherLen = str.length();
        if (otherLen == 0) return this;
        char[] buf = new char[value.length + otherLen];
        int i = 0;
        while (i < value.length) { buf[i] = value[i]; i = i + 1; }
        int j = 0;
        while (j < otherLen) { buf[value.length + j] = str.charAt(j); j = j + 1; }
        return new String(buf);
    }
    public String replace(char oldChar, char newChar) {
        if (oldChar == newChar) return this;
        int len = value.length;
        char[] buf = new char[len];
        int i = 0;
        while (i < len) { char c = value[i]; buf[i] = (c == oldChar) ? newChar : c; i = i + 1; }
        return new String(buf);
    }
    public String toLowerCase() {   // §20.12.44 — per-char via Character (full BMP Unicode)
        char[] result = new char[value.length];
        int i = 0;
        while (i < value.length) { result[i] = Character.toLowerCase(value[i]); i = i + 1; }
        return new String(result);
    }
    public String toUpperCase() {   // §20.12.48
        char[] result = new char[value.length];
        int i = 0;
        while (i < value.length) { result[i] = Character.toUpperCase(value[i]); i = i + 1; }
        return new String(result);
    }
    public String trim() {
        int len = value.length;
        int st = 0;
        while (st < len && value[st] <= ' ') st = st + 1;
        while (st < len && value[len - 1] <= ' ') len = len - 1;
        return (st > 0 || len < value.length) ? substring(st, len) : this;
    }
    public static String valueOf(Object obj) { return (obj == null) ? "null" : obj.toString(); }
    public static String valueOf(char[] data) throws NullPointerException { return new String(data); }
    public static String valueOf(char[] data, int offset, int count) throws NullPointerException, IndexOutOfBoundsException { return new String(data, offset, count); }
    public static String valueOf(boolean b) { return b ? "true" : "false"; }
    public static String valueOf(char c) {
        char[] buf = new char[1];
        buf[0] = c;
        return new String(buf);
    }
    public static String valueOf(int i)  { return Integer.toString(i); }
    public static String valueOf(long l) { return Long.toString(l); }
    public static String valueOf(float f)  { return Float.toString(f); }   // §20.9.16
    public static String valueOf(double d) { return Double.toString(d); }  // §20.10.15
    // §20.12.30: a canonical pool s.t. a.intern() == b.intern() iff a.equals(b). Java-side pool
    // (real Java uses a VM table for speed; a Hashtable is spec-correct). Lazy — no static-init
    // dependency on java.util from String's clinit.
    private static java.util.Hashtable internPool;
    public String intern() {
        if (internPool == null) internPool = new java.util.Hashtable();
        String existing = (String) internPool.get(this);
        if (existing != null) return existing;
        internPool.put(this, this);
        return this;
    }
}
