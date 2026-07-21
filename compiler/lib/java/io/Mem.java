package java.io;

// javelina-internal: raw byte access to the module's I/O staging linear memory — the GC-byte[]↔
// linear-memory bounce buffer for the host I/O floor. `load8`/`store8` are COMPILER INTRINSICS,
// lowered to the wasm i32.load8_u / i32.store8 instructions (never real calls / imports).
public final class Mem {
    private Mem() {}
    public static native int load8(int addr);         // → MemLoad8  (i32.load8_u, zero-extended)
    public static native void store8(int addr, int val);   // → MemStore8 (i32.store8, low 8 bits)
}
