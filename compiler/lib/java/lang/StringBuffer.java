package java.lang;

public class StringBuffer {
    private char[] value;
    private int count;

    public StringBuffer() { value = new char[16]; count = 0; }
    public StringBuffer(int length) { value = new char[length]; count = 0; }
    public StringBuffer(String str) {
        int len = str.length();
        value = new char[len + 16];
        int i = 0;
        while (i < len) { value[i] = str.charAt(i); i = i + 1; }
        count = len;
    }

    public int length()   { return count; }
    public int capacity() { return value.length; }
    public String toString() { return new String(value, 0, count); }

    public char charAt(int index) throws IndexOutOfBoundsException {
        if (index < 0 || index >= count) throw new StringIndexOutOfBoundsException(index);
        return value[index];
    }
    public void setCharAt(int index, char ch) throws IndexOutOfBoundsException {
        if (index < 0 || index >= count) throw new StringIndexOutOfBoundsException(index);
        value[index] = ch;
    }

    public void ensureCapacity(int minimumCapacity) {
        if (minimumCapacity > value.length) {
            int newCap = value.length * 2 + 2;
            if (newCap < minimumCapacity) newCap = minimumCapacity;
            char[] n = new char[newCap];
            int i = 0;
            while (i < count) { n[i] = value[i]; i = i + 1; }
            value = n;
        }
    }
    public void setLength(int newLength) throws IndexOutOfBoundsException {
        if (newLength < 0) throw new StringIndexOutOfBoundsException(newLength);
        ensureCapacity(newLength);
        int i = count;
        while (i < newLength) { value[i] = (char)0; i = i + 1; }   // §20.13.13: pad with NUL
        count = newLength;
    }

    public StringBuffer append(String str) {
        if (str == null) str = "null";                            // §20.13.8 null → "null"
        int slen = str.length();
        ensureCapacity(count + slen);
        int i = 0;
        while (i < slen) { value[count + i] = str.charAt(i); i = i + 1; }
        count = count + slen;
        return this;
    }
    public StringBuffer append(char c) {
        ensureCapacity(count + 1);
        value[count] = c;
        count = count + 1;
        return this;
    }
    public StringBuffer append(char[] str) throws NullPointerException {
        return append(str, 0, str.length);
    }
    public StringBuffer append(char[] str, int offset, int len) throws NullPointerException, IndexOutOfBoundsException {
        ensureCapacity(count + len);
        int i = 0;
        while (i < len) { value[count + i] = str[offset + i]; i = i + 1; }
        count = count + len;
        return this;
    }
    public StringBuffer append(boolean b) { return append(b ? "true" : "false"); }

    public StringBuffer append(Object obj) { return append(String.valueOf(obj)); }
    public StringBuffer append(int i)      { return append(Integer.toString(i)); }
    public StringBuffer append(long l)     { return append(Long.toString(l)); }
    public StringBuffer append(float f)    { return append(String.valueOf(f)); }
    public StringBuffer append(double d)   { return append(String.valueOf(d)); }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) throws NullPointerException, IndexOutOfBoundsException {
        if (srcBegin < 0) throw new StringIndexOutOfBoundsException(srcBegin);
        if (srcEnd > count) throw new StringIndexOutOfBoundsException(srcEnd);
        if (srcBegin > srcEnd) throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        int i = srcBegin;
        int j = dstBegin;
        while (i < srcEnd) { dst[j] = value[i]; i = i + 1; j = j + 1; }
    }

    public StringBuffer insert(int offset, String str) throws IndexOutOfBoundsException {
        if (offset < 0 || offset > count) throw new StringIndexOutOfBoundsException(offset);
        if (str == null) str = "null";
        int slen = str.length();
        ensureCapacity(count + slen);
        int i = count - 1;
        while (i >= offset) { value[i + slen] = value[i]; i = i - 1; }   // shift right
        int j = 0;
        while (j < slen) { value[offset + j] = str.charAt(j); j = j + 1; }
        count = count + slen;
        return this;
    }
    public StringBuffer insert(int offset, char c) throws IndexOutOfBoundsException {
        if (offset < 0 || offset > count) throw new StringIndexOutOfBoundsException(offset);
        ensureCapacity(count + 1);
        int i = count - 1;
        while (i >= offset) { value[i + 1] = value[i]; i = i - 1; }
        value[offset] = c;
        count = count + 1;
        return this;
    }
    public StringBuffer insert(int offset, Object obj) throws IndexOutOfBoundsException { return insert(offset, String.valueOf(obj)); }
    public StringBuffer insert(int offset, char[] str) throws NullPointerException, IndexOutOfBoundsException { return insert(offset, String.valueOf(str)); }
    public StringBuffer insert(int offset, boolean b) throws IndexOutOfBoundsException { return insert(offset, String.valueOf(b)); }
    public StringBuffer insert(int offset, int i) throws IndexOutOfBoundsException  { return insert(offset, Integer.toString(i)); }
    public StringBuffer insert(int offset, long l) throws IndexOutOfBoundsException { return insert(offset, Long.toString(l)); }
    public StringBuffer insert(int offset, float f) throws IndexOutOfBoundsException  { return insert(offset, String.valueOf(f)); }
    public StringBuffer insert(int offset, double d) throws IndexOutOfBoundsException { return insert(offset, String.valueOf(d)); }

    public StringBuffer reverse() {
        int n = count - 1;
        int j = 0;
        while (j < n) {
            char tmp = value[j];
            value[j] = value[n];
            value[n] = tmp;
            j = j + 1;
            n = n - 1;
        }
        return this;
    }
}
