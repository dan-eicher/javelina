package java.lang;

import java.io.HostIO;
import javelina.simd.Mem;
import java.util.Properties;

public final class System {
    public static java.io.InputStream in = new java.io.FileInputStream(java.io.FileDescriptor.in);
    public static java.io.PrintStream out = new java.io.PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.out));
    public static java.io.PrintStream err = new java.io.PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.err));
    public static native SecurityManager getSecurityManager();
    public static native void setSecurityManager(SecurityManager s)
        throws SecurityException;
    public static native long currentTimeMillis();

    // ── §20.18.7-.10 system properties. Not natives: a host function cannot construct a String
    // (GC aggregates are opaque across the §7.1 boundary), so the host's property table is read
    // through the I/O staging memory as bytes and the Strings are built here. `props` is the
    // "current set" the spec speaks of — created on first use, replaceable by setProperties, and
    // forgotten (re-created from the host on next use) when that is passed null. ──
    private static Properties props;

    public static Properties getProperties() throws SecurityException {
        if (props == null) props = initProperties();
        return props;
    }
    public static void setProperties(Properties p) throws SecurityException { props = p; }

    public static String getProperty(String key) throws SecurityException {
        if (props == null) props = initProperties();
        return props.getProperty(key);
    }
    public static String getProperty(String key, String defaults) throws SecurityException {
        String value = getProperty(key);
        return value == null ? defaults : value;
    }

    // The host's property table, read once. propnames writes every key NUL-separated at offset 0;
    // the names are decoded BEFORE any getprop call, because staging a key overwrites that region.
    private static Properties initProperties() {
        Properties p = new Properties();
        int total = HostIO.propnames(0);
        if (total <= 0) return p;
        if (total > Mem.memory_size() * 65536) {        // answered the size, wrote nothing
            if (!HostIO.ensureRoom(0, total)) return p;
            total = HostIO.propnames(0);
            if (total <= 0) return p;
        }
        int count = 0;
        for (int i = 0; i < total; i++) if (Mem.i32_load8_u(i) == 0) count++;
        String[] names = new String[count];
        int idx = 0;
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < total; i++) {
            int b = Mem.i32_load8_u(i);
            if (b == 0) { names[idx++] = sb.toString(); sb = new StringBuffer(); }
            else sb.append((char) b);
        }
        for (int i = 0; i < count; i++) {
            String value = hostProperty(names[i]);
            if (value != null) p.put(names[i], value);
        }
        return p;
    }

    // Stage the key at offset 0; the host writes the value bytes just past it, answers the length the
    // value NEEDS, or -1 for a property that is absent. A value too long for the room offered is a
    // retry, not a miss: -1 is the answer §20.18.7 reserves for an undefined property, and returning
    // it for a value that merely did not fit is a wrong answer, not a safe one.
    private static String hostProperty(String key) {
        int n = key.length();
        for (int i = 0; i < n; i++) Mem.i32_store8(i, key.charAt(i));
        int len = HostIO.getprop(0, n, n);
        if (len < 0) return null;
        if (n + len > Mem.memory_size() * 65536) {      // answered the size, wrote nothing
            if (!HostIO.ensureRoom(n, len)) return null;
            len = HostIO.getprop(0, n, n);              // the key is still staged; now it fits
            if (len < 0) return null;
        }
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < len; i++) sb.append((char) Mem.i32_load8_u(n + i));
        return sb.toString();
    }

    public static native void exit(int status) throws SecurityException;
    public static native void gc();
    public static native void runFinalization();
    public static native void load(String filename)
        throws SecurityException, UnsatisfiedLinkError;
    public static native void loadLibrary(String libname)
        throws SecurityException, UnsatisfiedLinkError;
    // §20.18.16. For CONCRETE primitive-array arguments the compiler lowers a direct call to the
    // WASM array.copy intrinsic (fast, overlap-safe). This body is the general case: erased-Object
    // arguments (runtime kind dispatch) and reference arrays. Reference arrays copy element-by-element
    // so each store is an aastore carrying the §10.10 ArrayStoreException check; primitive arrays,
    // once narrowed to a concrete type, recurse — which the SAME-primitive intrinsic lowers to
    // array.copy at that site (no element loop). Incompatible array kinds throw ArrayStoreException.
    public static void arraycopy(Object src, int srcOffset, Object dst, int dstOffset, int length)
        throws NullPointerException, ArrayStoreException, IndexOutOfBoundsException {
        if (src == null || dst == null) throw new NullPointerException();
        if (src instanceof Object[] && dst instanceof Object[]) {
            Object[] s = (Object[]) src, d = (Object[]) dst;
            acCheck(s.length, srcOffset, d.length, dstOffset, length);
            if (s == d && dstOffset > srcOffset) for (int i = length - 1; i >= 0; i--) d[dstOffset + i] = s[srcOffset + i];
            else                                 for (int i = 0; i < length; i++)         d[dstOffset + i] = s[srcOffset + i];
        }
        // Each primitive kind delegates to a small FLAT helper. Inlining all eight intrinsic
        // expansions (each with its own bounds-check control flow) into this one method's nested
        // if-else chain makes the backend's per-method emitted code explode; a helper keeps each
        // intrinsic in its own straight-line method (concrete same-primitive args -> array.copy).
        else if (src instanceof int[]     && dst instanceof int[])     acInt((int[]) src, srcOffset, (int[]) dst, dstOffset, length);
        else if (src instanceof long[]    && dst instanceof long[])    acLong((long[]) src, srcOffset, (long[]) dst, dstOffset, length);
        else if (src instanceof char[]    && dst instanceof char[])    acChar((char[]) src, srcOffset, (char[]) dst, dstOffset, length);
        else if (src instanceof byte[]    && dst instanceof byte[])    acByte((byte[]) src, srcOffset, (byte[]) dst, dstOffset, length);
        else if (src instanceof short[]   && dst instanceof short[])   acShort((short[]) src, srcOffset, (short[]) dst, dstOffset, length);
        else if (src instanceof float[]   && dst instanceof float[])   acFloat((float[]) src, srcOffset, (float[]) dst, dstOffset, length);
        else if (src instanceof double[]  && dst instanceof double[])  acDouble((double[]) src, srcOffset, (double[]) dst, dstOffset, length);
        else if (src instanceof boolean[] && dst instanceof boolean[]) acBool((boolean[]) src, srcOffset, (boolean[]) dst, dstOffset, length);
        else throw new ArrayStoreException();   // not both arrays, or incompatible element kinds
    }

    private static void acInt(int[] s, int so, int[] d, int dOff, int len)         { arraycopy(s, so, d, dOff, len); }
    private static void acLong(long[] s, int so, long[] d, int dOff, int len)       { arraycopy(s, so, d, dOff, len); }
    private static void acChar(char[] s, int so, char[] d, int dOff, int len)       { arraycopy(s, so, d, dOff, len); }
    private static void acByte(byte[] s, int so, byte[] d, int dOff, int len)       { arraycopy(s, so, d, dOff, len); }
    private static void acShort(short[] s, int so, short[] d, int dOff, int len)    { arraycopy(s, so, d, dOff, len); }
    private static void acFloat(float[] s, int so, float[] d, int dOff, int len)    { arraycopy(s, so, d, dOff, len); }
    private static void acDouble(double[] s, int so, double[] d, int dOff, int len) { arraycopy(s, so, d, dOff, len); }
    private static void acBool(boolean[] s, int so, boolean[] d, int dOff, int len) { arraycopy(s, so, d, dOff, len); }

    // §20.18.16 range checks, in JLS order (thrown before any element is copied).
    private static void acCheck(int srcLen, int srcOffset, int dstLen, int dstOffset, int length) {
        if (srcOffset < 0 || dstOffset < 0 || length < 0
                || srcOffset + length > srcLen || dstOffset + length > dstLen)
            throw new IndexOutOfBoundsException();
    }
}
