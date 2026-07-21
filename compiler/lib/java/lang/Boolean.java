package java.lang;

public final class Boolean {
    public static final Boolean TRUE = new Boolean(true);
    public static final Boolean FALSE = new Boolean(false);

    private boolean value;

    public Boolean(boolean value) { this.value = value; }
    public Boolean(String s) { this.value = parseBoolean(s); }

    public boolean booleanValue() { return value; }

    public boolean equals(Object obj) {
        if (obj instanceof Boolean) {
            return value == ((Boolean)obj).booleanValue();
        }
        return false;
    }

    public int hashCode() { return value ? 1231 : 1237; }

    // §20.4: equal, ignoring case, to "true" (ASCII — Character.toUpperCase is the
    // Unicode-table extern, but boolean parsing is defined only over the ASCII word).
    private static boolean parseBoolean(String s) {
        return s != null && s.length() == 4
            && (s.charAt(0) == 't' || s.charAt(0) == 'T')
            && (s.charAt(1) == 'r' || s.charAt(1) == 'R')
            && (s.charAt(2) == 'u' || s.charAt(2) == 'U')
            && (s.charAt(3) == 'e' || s.charAt(3) == 'E');
    }

    public static Boolean valueOf(String s) { return parseBoolean(s) ? TRUE : FALSE; }

    public String toString() { return value ? "true" : "false"; }

    // §20.4.10: true iff the system property named by the argument is equal, ignoring case, to "true".
    public static boolean getBoolean(String name) { return parseBoolean(System.getProperty(name)); }
}
