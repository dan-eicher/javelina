package java.io;

// java.io.DataInput (JLS 1.0 §22.1) — reads primitive types from a binary stream in the
// portable (big-endian) format written by DataOutput. Implemented by DataInputStream /
// RandomAccessFile. End-of-stream mid-value is signalled by EOFException.
public interface DataInput {
    void readFully(byte[] b) throws IOException;
    void readFully(byte[] b, int off, int len) throws IOException;
    int skipBytes(int n) throws IOException;
    boolean readBoolean() throws IOException;
    byte readByte() throws IOException;
    int readUnsignedByte() throws IOException;
    short readShort() throws IOException;
    int readUnsignedShort() throws IOException;
    char readChar() throws IOException;
    int readInt() throws IOException;
    long readLong() throws IOException;
    float readFloat() throws IOException;
    double readDouble() throws IOException;
    String readLine() throws IOException;
    String readUTF() throws IOException;
}
