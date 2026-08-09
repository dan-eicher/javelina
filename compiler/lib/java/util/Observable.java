package java.util;

// java.util.Observable (JLS 1.0 §21.7) — an observable object. Ported minus `synchronized`
// (javelina targets Java 1.0 − synchronized; the lock-scoped observer-snapshot is dropped).
public class Observable {
    private boolean changed = false;
    private Vector obs;

    public Observable() {
        obs = new Vector();
    }

    // §21.7: "An observer may be any object that IMPLEMENTS INTERFACE Observer", and §21.7.1 adds
    // it "provided that it is not the same as some observer already in the set" — sameness being
    // equals (§20.1.3). null is neither: it implements nothing, and the test the method is
    // defined in terms of cannot be evaluated for it. Storing it anyway only defers the
    // NullPointerException to notifyObservers, which raises it from a frame that has nothing to
    // do with the mistake.
    public void addObserver(Observer o) throws NullPointerException {
        if (o == null) throw new NullPointerException();
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
