package java.io;

// java.io.DataInputStream (JLS 1.0 §22.5) — a FilterInputStream that reads primitives in the
// portable big-endian binary format written by DataOutputStream (the DataInput contract).
// End-of-stream in the middle of a multi-byte value throws EOFException.
public class DataInputStream extends FilterInputStream implements DataInput {
    private int pushBack = -1;      // readLine() CR/LF lookahead

    public DataInputStream(InputStream in) { super(in); }

    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }

    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) {
            int count = in.read(b, off + n, len - n);
            if (count < 0) throw new EOFException();
            n = n + count;
        }
    }

    public final int skipBytes(int n) throws IOException {
        int total = 0;
        int cur = 0;
        while ((total < n) && ((cur = (int) in.skip(n - total)) > 0)) total = total + cur;
        return total;
    }

    public final boolean readBoolean() throws IOException {
        int ch = in.read();
        if (ch < 0) throw new EOFException();
        return ch != 0;
    }

    public final byte readByte() throws IOException {
        int ch = in.read();
        if (ch < 0) throw new EOFException();
        return (byte) ch;
    }

    public final int readUnsignedByte() throws IOException {
        int ch = in.read();
        if (ch < 0) throw new EOFException();
        return ch;
    }

    public final short readShort() throws IOException {
        int ch1 = in.read();
        int ch2 = in.read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (short) ((ch1 << 8) + (ch2 << 0));
    }

    public final int readUnsignedShort() throws IOException {
        int ch1 = in.read();
        int ch2 = in.read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (ch1 << 8) + (ch2 << 0);
    }

    public final char readChar() throws IOException {
        int ch1 = in.read();
        int ch2 = in.read();
        if ((ch1 | ch2) < 0) throw new EOFException();
        return (char) ((ch1 << 8) + (ch2 << 0));
    }

    public final int readInt() throws IOException {
        int ch1 = in.read();
        int ch2 = in.read();
        int ch3 = in.read();
        int ch4 = in.read();
        if ((ch1 | ch2 | ch3 | ch4) < 0) throw new EOFException();
        return (ch1 << 24) + (ch2 << 16) + (ch3 << 8) + (ch4 << 0);
    }

    public final long readLong() throws IOException {
        return ((long) readInt() << 32) + ((long) readInt() & 0xFFFFFFFFL);
    }

    public final float readFloat() throws IOException {
        return Float.intBitsToFloat(readInt());
    }

    public final double readDouble() throws IOException {
        return Double.longBitsToDouble(readLong());
    }

    // §22.5: deprecated — bytes are zero-extended to chars; a line ends at '\n', '\r', or '\r\n'.
    public final String readLine() throws IOException {
        StringBuffer buf = new StringBuffer();
        int c = -1;
        boolean eol = false;
        boolean any = false;
        while (!eol) {
            if (pushBack != -1) { c = pushBack; pushBack = -1; }
            else c = in.read();
            if (c == -1 || c == '\n') {
                eol = true;
            } else if (c == '\r') {
                eol = true;
                int c2 = in.read();
                if (c2 != '\n' && c2 != -1) pushBack = c2;
            } else {
                buf.append((char) c);
                any = true;
            }
        }
        if ((c == -1) && !any) return null;
        return buf.toString();
    }

    public final String readUTF() throws IOException {
        int utflen = readUnsignedShort();
        char[] str = new char[utflen];
        int count = 0;
        int strlen = 0;
        while (count < utflen) {
            int c = readUnsignedByte();
            int t = c >> 4;
            if (t <= 7) {                        // 0xxxxxxx
                count = count + 1;
                str[strlen] = (char) c;
                strlen = strlen + 1;
            } else if (t == 12 || t == 13) {     // 110xxxxx 10xxxxxx
                count = count + 2;
                if (count > utflen) throw new UTFDataFormatException();
                int c2 = readUnsignedByte();
                if ((c2 & 0xC0) != 0x80) throw new UTFDataFormatException();
                str[strlen] = (char) (((c & 0x1F) << 6) | (c2 & 0x3F));
                strlen = strlen + 1;
            } else if (t == 14) {                // 1110xxxx 10xxxxxx 10xxxxxx
                count = count + 3;
                if (count > utflen) throw new UTFDataFormatException();
                int c2 = readUnsignedByte();
                int c3 = readUnsignedByte();
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) throw new UTFDataFormatException();
                str[strlen] = (char) (((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | ((c3 & 0x3F) << 0));
                strlen = strlen + 1;
            } else {
                throw new UTFDataFormatException();
            }
        }
        return new String(str, 0, strlen);
    }
}
