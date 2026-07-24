package java.util;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;

// java.util.Properties (JLS 1.0 §21.6) — a Hashtable of String→String with a `.properties`
// text format (load/save) and an optional chain of `defaults`.
public class Properties extends Hashtable {
    protected Properties defaults;

    public Properties() { this(null); }
    public Properties(Properties defaults) { this.defaults = defaults; }

    // §21.6: parse the `.properties` stream — logical lines (physical lines joined on a
    // trailing odd backslash), `#`/`!` comments, `=`/`:`/whitespace key-value separators,
    // and \t\r\n\f\\ / \\uXXXX escapes.
    public void load(InputStream inRaw) throws IOException {
        BufferedInputStream in = new BufferedInputStream(inRaw);
        while (true) {
            String line = readLine(in);
            if (line == null) return;
            if (line.length() == 0) continue;
            char first = line.charAt(0);
            if (first == '#' || first == '!') continue;

            while (continueLine(line)) {
                String next = readLine(in);
                if (next == null) next = "";
                String lopped = line.substring(0, line.length() - 1);
                int s = 0;
                while (s < next.length() && " \t\r\n\f".indexOf(next.charAt(s)) != -1) s = s + 1;
                line = lopped + next.substring(s, next.length());
            }

            int len = line.length();
            int keyStart = 0;
            while (keyStart < len && " \t\r\n\f".indexOf(line.charAt(keyStart)) != -1) keyStart = keyStart + 1;

            int sep = keyStart;
            while (sep < len) {
                char c = line.charAt(sep);
                if (c == '\\') { sep = sep + 1; }
                else if ("=: \t\r\n\f".indexOf(c) != -1) break;
                sep = sep + 1;
            }

            int valStart = sep;
            while (valStart < len && " \t\r\n\f".indexOf(line.charAt(valStart)) != -1) valStart = valStart + 1;
            if (valStart < len && "=:".indexOf(line.charAt(valStart)) != -1) valStart = valStart + 1;
            while (valStart < len && " \t\r\n\f".indexOf(line.charAt(valStart)) != -1) valStart = valStart + 1;

            String key = line.substring(keyStart, sep);
            String value = (sep < len) ? line.substring(valStart, len) : "";
            put(loadConvert(key), loadConvert(value));
        }
    }

    // One line as ISO-8859-1 (low 8 bits), terminator ('\n', '\r', "\r\n") stripped; null at EOF.
    private String readLine(BufferedInputStream in) throws IOException {
        int c = in.read();
        if (c == -1) return null;
        StringBuffer buf = new StringBuffer();
        while (c != -1 && c != '\n' && c != '\r') {
            buf.append((char) (c & 0xFF));
            c = in.read();
        }
        if (c == '\r') {                       // absorb the LF of a CRLF pair
            in.mark(1);
            int c2 = in.read();
            if (c2 != '\n' && c2 != -1) in.reset();
        }
        return buf.toString();
    }

    private boolean continueLine(String line) {
        int slashes = 0;
        int i = line.length() - 1;
        while (i >= 0 && line.charAt(i) == '\\') { slashes = slashes + 1; i = i - 1; }
        return (slashes % 2) == 1;
    }

    private String loadConvert(String s) {
        int len = s.length();
        StringBuffer out = new StringBuffer(len);
        int x = 0;
        while (x < len) {
            char c = s.charAt(x); x = x + 1;
            if (c == '\\') {
                c = s.charAt(x); x = x + 1;
                if (c == 'u') {
                    int value = 0;
                    for (int i = 0; i < 4; i = i + 1) {
                        char h = s.charAt(x); x = x + 1;
                        if (h >= '0' && h <= '9') value = (value << 4) + (h - '0');
                        else if (h >= 'a' && h <= 'f') value = (value << 4) + 10 + (h - 'a');
                        else if (h >= 'A' && h <= 'F') value = (value << 4) + 10 + (h - 'A');
                    }
                    out.append((char) value);
                } else {
                    if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == 'n') c = '\n';
                    else if (c == 'f') c = '\f';
                    out.append(c);
                }
            } else out.append(c);
        }
        return out.toString();
    }

    public void save(OutputStream out, String header) {
        PrintStream prnt = new PrintStream(out);
        if (header != null) prnt.println("#" + header);
        prnt.println("#" + new Date().toString());
        for (Enumeration e = keys(); e.hasMoreElements(); ) {
            String key = (String) e.nextElement();
            String val = (String) get(key);
            prnt.println(saveConvert(key) + "=" + saveConvert(val));
        }
        prnt.flush();
    }

    private String saveConvert(String s) {
        int len = s.length();
        StringBuffer out = new StringBuffer(len * 2);
        for (int x = 0; x < len; x = x + 1) {
            char c = s.charAt(x);
            if (c == '\\') out.append("\\\\");
            else if (c == '\t') out.append("\\t");
            else if (c == '\n') out.append("\\n");
            else if (c == '\r') out.append("\\r");
            else if (c == '\f') out.append("\\f");
            else if (c == '=' || c == ':' || c == '#' || c == '!') { out.append('\\'); out.append(c); }
            else out.append(c);
        }
        return out.toString();
    }

    public String getProperty(String key) {
        Object oval = super.get(key);
        String sval = (oval instanceof String) ? (String) oval : null;
        return ((sval == null) && (defaults != null)) ? defaults.getProperty(key) : sval;
    }

    public String getProperty(String key, String defaultValue) {
        String val = getProperty(key);
        return (val == null) ? defaultValue : val;
    }

    public Enumeration propertyNames() {
        Hashtable h = new Hashtable();
        enumerate(h);
        return h.keys();
    }

    private void enumerate(Hashtable h) {
        if (defaults != null) defaults.enumerate(h);
        for (Enumeration e = keys(); e.hasMoreElements(); ) {
            String key = (String) e.nextElement();
            h.put(key, get(key));
        }
    }

    public void list(PrintStream out) {
        out.println("-- listing properties --");
        Hashtable h = new Hashtable();
        enumerate(h);
        for (Enumeration e = h.keys(); e.hasMoreElements(); ) {
            String key = (String) e.nextElement();
            String val = (String) h.get(key);
            if (val.length() > 40) val = val.substring(0, 37) + "...";
            out.println(key + "=" + val);
        }
    }
}
