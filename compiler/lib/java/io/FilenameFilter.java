package java.io;

// java.io.FilenameFilter (JLS 1.0 §22.7) — a predicate on directory entries, used by File.list.
public interface FilenameFilter {
    boolean accept(File dir, String name);
}
