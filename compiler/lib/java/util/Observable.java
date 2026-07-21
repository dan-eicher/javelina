package java.util;

// java.util.Observable (JLS 1.0 §21.8) — an observable object. Ported minus `synchronized`
// (javelina targets Java 1.0 − synchronized; the lock-scoped observer-snapshot is dropped).
public class Observable {
    private boolean changed = false;
    private Vector obs;

    public Observable() {
        obs = new Vector();
    }

    public void addObserver(Observer o) {
        if (!obs.contains(o)) obs.addElement(o);
    }

    public void deleteObserver(Observer o) {
        obs.removeElement(o);
    }

    public void notifyObservers() {
        notifyObservers(null);
    }

    public void notifyObservers(Object arg) {
        if (!changed) return;
        clearChanged();
        for (int i = obs.size() - 1; i >= 0; i = i - 1) {
            ((Observer) obs.elementAt(i)).update(this, arg);
        }
    }

    public void deleteObservers() {
        obs.removeAllElements();
    }

    protected void setChanged() {
        changed = true;
    }

    protected void clearChanged() {
        changed = false;
    }

    public boolean hasChanged() {
        return changed;
    }

    public int countObservers() {
        return obs.size();
    }
}
