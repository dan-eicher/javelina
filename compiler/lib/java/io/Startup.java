package java.io;

// javelina-internal: the program-entry argument bridge. The runner writes the command-line
// arguments as NUL-separated UTF-8 into the shared I/O staging memory and calls the compiler-
// synthesized `$main(argc, base)`, which delegates here to build the String[] the program's
// main(String[]) expects. Reads the staging bytes through Mem.load8 (the same GC↔linear-memory
// seam the host I/O floor uses) and UTF-8-decodes each argument into a String.
public final class Startup {
    private Startup() {}

    // Decode `argc` NUL-terminated UTF-8 arguments starting at `base` in the staging memory
    // into a String[]. Malformed continuation bytes are taken at face value (no exception —
    // args come from the trusted embedder, and a program should still start).
    public static String[] args(int argc, int base) {
        String[] out = new String[argc];
        int p = base;
        for (int i = 0; i < argc; i++) {
            int start = p;
            while (Mem.load8(p) != 0) p++;          // find this argument's terminating NUL
            int nbytes = p - start;
            p++;                                     // step past the NUL to the next argument
            char[] cs = new char[nbytes];            // char count <= byte count (UTF-8), safe upper bound
            int n = 0, q = start, end = start + nbytes;
            while (q < end) {
                int b = Mem.load8(q++);
                int c;
                if (b < 0x80) {                                              // 1-byte (ASCII)
                    c = b;
                } else if ((b & 0xE0) == 0xC0 && q < end) {                  // 2-byte
                    c = ((b & 0x1F) << 6) | (Mem.load8(q++) & 0x3F);
                } else if ((b & 0xF0) == 0xE0 && q + 1 < end) {             // 3-byte
                    c = ((b & 0x0F) << 12) | ((Mem.load8(q++) & 0x3F) << 6) | (Mem.load8(q++) & 0x3F);
                } else if ((b & 0xF8) == 0xF0 && q + 2 < end) {             // 4-byte (supplementary)
                    c = ((b & 0x07) << 18) | ((Mem.load8(q++) & 0x3F) << 12)
                      | ((Mem.load8(q++) & 0x3F) << 6) | (Mem.load8(q++) & 0x3F);
                } else {
                    c = b & 0xFF;                                            // lone/invalid byte: pass through
                }
                if (c > 0xFFFF) {                                            // encode as a UTF-16 surrogate pair
                    c -= 0x10000;
                    cs[n++] = (char)(0xD800 + (c >> 10));
                    cs[n++] = (char)(0xDC00 + (c & 0x3FF));
                } else {
                    cs[n++] = (char)c;
                }
            }
            out[i] = new String(cs, 0, n);
        }
        return out;
    }
}
