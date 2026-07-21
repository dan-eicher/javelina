package java.io;

// java.io.RandomAccessFile (JLS 1.0 §22.4) — random-access reads/writes to a host file via the
// embedder's fd surface + the byte[]↔linear-memory bounce (Mem). Implements DataInput/DataOutput
// with the same portable big-endian composition as Data{Input,Output}Stream, over its own read()/
// write(). The file pointer is tracked in the guest, kept in sync with the host fd position.
public class RandomAccessFile implements DataInput, DataOutput {
    private FileDescriptor fd;
    private long filePointer;

    public RandomAccessFile(String name, String mode) throws IOException {
        boolean rw = mode.equals("rw");
        if (!rw && !mode.equals("r")) throw new IllegalArgumentException("mode must be \"r\" or \"rw\"");
        int len = name.length();
        for (int i = 0; i < len; i++) Mem.store8(i, name.charAt(i));
        int h = HostIO.open(0, len, rw ? 2 : 0);   // 2 = read+write (create, no truncate); 0 = read
        if (h < 0) throw new FileNotFoundException(name);
        fd = new FileDescriptor(h);
        filePointer = 0;
    }
    public RandomAccessFile(File file, String mode) throws IOException {
        this(file.getPath(), mode);
    }

    public final FileDescriptor getFD() throws IOException { return fd; }

    // ── position + length ──
    public long getFilePointer() throws IOException { return filePointer; }
    public void seek(long pos) throws IOException {
        if (pos < 0) throw new IOException("negative seek offset");
        HostIO.fd_seek(fd.fd, (int) pos);
        filePointer = pos;
    }
    public long length() throws IOException { return HostIO.fd_size(fd.fd); }
    public int skipBytes(int n) throws IOException {
        if (n <= 0) return 0;
        long pos = getFilePointer();
        long len = length();
        long newpos = pos + n;
        if (newpos > len) newpos = len;
        seek(newpos);
        return (int) (newpos - pos);
    }
    public void close() throws IOException { HostIO.fd_close(fd.fd); }

    // ── raw read/write over the fd + bounce buffer ──
    public int read() throws IOException {
        int n = HostIO.fd_read(fd.fd, 0, 1);
        if (n <= 0) return -1;
        filePointer = filePointer + 1;
        return Mem.load8(0);
    }
    public int read(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || len > b.length - off) throw new IndexOutOfBoundsException();
        if (len == 0) return 0;
        int chunk = len > 65536 ? 65536 : len;
        int n = HostIO.fd_read(fd.fd, 0, chunk);
        if (n <= 0) return -1;
        for (int j = 0; j < n; j++) b[off + j] = (byte) Mem.load8(j);
        filePointer = filePointer + n;
        return n;
    }
    public int read(byte[] b) throws IOException { return read(b, 0, b.length); }

    public void write(int b) throws IOException {
        Mem.store8(0, b);
        HostIO.fd_write(fd.fd, 0, 1);
        filePointer = filePointer + 1;
    }
    public void write(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || len > b.length - off) throw new IndexOutOfBoundsException();
        int i = 0;
        while (i < len) {
            int chunk = (len - i) > 65536 ? 65536 : (len - i);
            for (int j = 0; j < chunk; j++) Mem.store8(j, b[off + i + j]);
            HostIO.fd_write(fd.fd, 0, chunk);
            i = i + chunk;
        }
        filePointer = filePointer + len;
    }
    public void write(byte[] b) throws IOException { write(b, 0, b.length); }

    // ── DataInput (§22.5): the big-endian composition of DataInputStream, over this.read() ──
    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) {
            int count = read(b, off + n, len - n);
            if (count < 0) throw new EOFException();
            n = n + count;
        }
    }
    public final boolean readBoolean() throws IOException {
        int ch = read(); if (ch < 0) throw new EOFException(); return ch != 0;
    }
    public final byte readByte() throws IOException {
        int ch = read(); if (ch < 0) throw new EOFException(); return (byte) ch;
    }
    public final int readUnsignedByte() throws IOException {
        int ch = read(); if (ch < 0) throw new EOFException(); return ch;
    }
    public final short readShort() throws IOException {
        int ch1 = read(); int ch2 = read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (short) ((ch1 << 8) + (ch2 << 0));
    }
    public final int readUnsignedShort() throws IOException {
        int ch1 = read(); int ch2 = read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (ch1 << 8) + (ch2 << 0);
    }
    public final char readChar() throws IOException {
        int ch1 = read(); int ch2 = read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (char) ((ch1 << 8) + (ch2 << 0));
    }
    public final int readInt() throws IOException {
        int ch1 = read(); int ch2 = read(); int ch3 = read(); int ch4 = read();
        if ((ch1 | ch2 | ch3 | ch4) < 0) throw new EOFException();
        return (ch1 << 24) + (ch2 << 16) + (ch3 << 8) + (ch4 << 0);
    }
    public final long readLong() throws IOException {
        return ((long) readInt() << 32) + ((long) readInt() & 0xFFFFFFFFL);
    }
    public final float readFloat() throws IOException { return Float.intBitsToFloat(readInt()); }
    public final double readDouble() throws IOException { return Double.longBitsToDouble(readLong()); }
    public final String readLine() throws IOException {
        StringBuffer buf = new StringBuffer();
        int c = -1; boolean eol = false; boolean any = false;
        while (!eol) {
            c = read();
            if (c == -1 || c == '\n') { eol = true; }
            else if (c == '\r') {
                eol = true;
                long cur = getFilePointer();
                int c2 = read();
                if (c2 != '\n' && c2 != -1) seek(cur);   // not the LF of a CRLF → push it back
            } else { buf.append((char) c); any = true; }
        }
        if ((c == -1) && !any) return null;
        return buf.toString();
    }
    public final String readUTF() throws IOException {
        int utflen = readUnsignedShort();
        char[] str = new char[utflen];
        int count = 0; int strlen = 0;
        while (count < utflen) {
            int c = readUnsignedByte();
            int t = c >> 4;
            if (t <= 7) {
                count = count + 1; str[strlen] = (char) c; strlen = strlen + 1;
            } else if (t == 12 || t == 13) {
                count = count + 2; if (count > utflen) throw new UTFDataFormatException();
                int c2 = readUnsignedByte();
                if ((c2 & 0xC0) != 0x80) throw new UTFDataFormatException();
                str[strlen] = (char) (((c & 0x1F) << 6) | (c2 & 0x3F)); strlen = strlen + 1;
            } else if (t == 14) {
                count = count + 3; if (count > utflen) throw new UTFDataFormatException();
                int c2 = readUnsignedByte(); int c3 = readUnsignedByte();
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) throw new UTFDataFormatException();
                str[strlen] = (char) (((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | ((c3 & 0x3F) << 0)); strlen = strlen + 1;
            } else {
                throw new UTFDataFormatException();
            }
        }
        return new String(str, 0, strlen);
    }

    // ── DataOutput (§22.6): the composition of DataOutputStream, over this.write() ──
    public final void writeBoolean(boolean v) throws IOException { write(v ? 1 : 0); }
    public final void writeByte(int v) throws IOException { write(v); }
    public final void writeShort(int v) throws IOException {
        write((v >>> 8) & 0xFF); write((v >>> 0) & 0xFF);
    }
    public final void writeChar(int v) throws IOException {
        write((v >>> 8) & 0xFF); write((v >>> 0) & 0xFF);
    }
    public final void writeInt(int v) throws IOException {
        write((v >>> 24) & 0xFF); write((v >>> 16) & 0xFF); write((v >>> 8) & 0xFF); write((v >>> 0) & 0xFF);
    }
    public final void writeLong(long v) throws IOException {
        write((int) (v >>> 56) & 0xFF); write((int) (v >>> 48) & 0xFF);
        write((int) (v >>> 40) & 0xFF); write((int) (v >>> 32) & 0xFF);
        write((int) (v >>> 24) & 0xFF); write((int) (v >>> 16) & 0xFF);
        write((int) (v >>> 8) & 0xFF);  write((int) (v >>> 0) & 0xFF);
    }
    public final void writeFloat(float v) throws IOException { writeInt(Float.floatToIntBits(v)); }
    public final void writeDouble(double v) throws IOException { writeLong(Double.doubleToLongBits(v)); }
    public final void writeBytes(String s) throws IOException {
        int len = s.length();
        for (int i = 0; i < len; i = i + 1) write((byte) s.charAt(i));
    }
    public final void writeChars(String s) throws IOException {
        int len = s.length();
        for (int i = 0; i < len; i = i + 1) { int v = s.charAt(i); write((v >>> 8) & 0xFF); write((v >>> 0) & 0xFF); }
    }
    public final void writeUTF(String str) throws IOException {
        int strlen = str.length(); int utflen = 0;
        for (int i = 0; i < strlen; i = i + 1) {
            int c = str.charAt(i);
            if ((c >= 0x0001) && (c <= 0x007F)) utflen = utflen + 1;
            else if (c > 0x07FF) utflen = utflen + 3;
            else utflen = utflen + 2;
        }
        if (utflen > 65535) throw new UTFDataFormatException();
        write((utflen >>> 8) & 0xFF); write((utflen >>> 0) & 0xFF);
        for (int i = 0; i < strlen; i = i + 1) {
            int c = str.charAt(i);
            if ((c >= 0x0001) && (c <= 0x007F)) {
                write(c);
            } else if (c > 0x07FF) {
                write(0xE0 | ((c >> 12) & 0x0F)); write(0x80 | ((c >> 6) & 0x3F)); write(0x80 | ((c >> 0) & 0x3F));
            } else {
                write(0xC0 | ((c >> 6) & 0x1F)); write(0x80 | ((c >> 0) & 0x3F));
            }
        }
    }
}
