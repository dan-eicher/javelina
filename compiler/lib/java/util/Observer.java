package java.util;

// java.util.Observer (JLS 1.0 §21.7) — implemented by classes that observe an Observable.
public interface Observer {
    void update(Observable o, Object arg);
}
