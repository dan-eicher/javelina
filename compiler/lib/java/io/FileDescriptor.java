package java.io;

// java.io.FileDescriptor (JLS 1.0 §22.26) — an opaque handle to an open host file or stream. It wraps
// the embedder's integer fd (from the host I/O surface). in/out/err are the three standard streams.
public final class FileDescriptor {
    int fd;   // package-private: the embedder's fd (-1 if invalid)

    public FileDescriptor() { fd = -1; }
    FileDescriptor(int fd) { this.fd = fd; }

    public boolean valid() { return fd >= 0; }

    public static final FileDescriptor in  = new FileDescriptor(0);
    public static final FileDescriptor out = new FileDescriptor(1);
    public static final FileDescriptor err = new FileDescriptor(2);
}
