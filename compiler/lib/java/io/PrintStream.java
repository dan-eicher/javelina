package java.io;

// java.io.PrintStream (JLS 1.0 §22.22) — a FilterOutputStream that prints textual representations
// of values. It never throws IOException; a failure sets an internal flag (checkError()). An
// optional autoflush flushes after each byte-array write and after a '\n'.
public class PrintStream extends FilterOutputStream {
    private boolean autoflush;
    private boolean trouble;

    public PrintStream(OutputStream out) { super(out); }
    public PrintStream(OutputStream out, boolean autoflush) { super(out); this.autoflush = autoflush; }

    public void write(int b) {
        try {
            out.write(b);
            if (autoflush && b == '\n') out.flush();
        } catch (IOException e) { trouble = true; }
    }

    public void write(byte[] b, int off, int len) {
        try {
            out.write(b, off, len);
            if (autoflush) out.flush();
        } catch (IOException e) { trouble = true; }
    }

    // Each char is written as its low 8 bits (JLS 1.0 §22.14 — byte streams, no charset).
    private void writeString(String s) {
        int n = s.length();
        int i = 0;
        while (i < n) { write(s.charAt(i)); i = i + 1; }
    }

    public void print(boolean b)  { writeString(b ? "true" : "false"); }
    public void print(char c)     { write(c); }
    public void print(int i)      { writeString(String.valueOf(i)); }
    public void print(long l)     { writeString(String.valueOf(l)); }
    public void print(float f)    { writeString(String.valueOf(f)); }
    public void print(double d)   { writeString(String.valueOf(d)); }
    public void print(char[] s)   { writeString(new String(s)); }
    public void print(String s)   { writeString((s == null) ? "null" : s); }
    public void print(Object obj) { writeString(String.valueOf(obj)); }

    public void println()             { write('\n'); }
    public void println(boolean b)    { print(b); println(); }
    public void println(char c)       { print(c); println(); }
    public void println(int i)        { print(i); println(); }
    public void println(long l)       { print(l); println(); }
    public void println(float f)      { print(f); println(); }
    public void println(double d)     { print(d); println(); }
    public void println(char[] s)     { print(s); println(); }
    public void println(String s)     { print(s); println(); }
    public void println(Object obj)   { print(obj); println(); }

    public void flush() { try { out.flush(); } catch (IOException e) { trouble = true; } }
    public void close() { try { out.close(); } catch (IOException e) { trouble = true; } }

    public boolean checkError() { flush(); return trouble; }
    protected void setError()   { trouble = true; }
}
