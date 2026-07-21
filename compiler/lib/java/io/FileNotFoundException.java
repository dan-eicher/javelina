package java.io;

// java.io.FileNotFoundException (JLS 1.0 §22.30) — a file could not be opened.
public class FileNotFoundException extends IOException {
    public FileNotFoundException() { super(); }
    public FileNotFoundException(String s) { super(s); }
}
