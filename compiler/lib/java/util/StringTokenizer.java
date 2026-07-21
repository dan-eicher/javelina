package java.util;

// java.util.StringTokenizer (JLS 1.0 §21.5) — breaks a String into tokens on a set of
// delimiter characters. Ported minus `synchronized`.
public class StringTokenizer implements Enumeration {
    private int currentPosition;
    private int maxPosition;
    private String str;
    private String delimiters;
    private boolean retTokens;

    public StringTokenizer(String str, String delim, boolean returnTokens) {
        currentPosition = 0;
        this.str = str;
        maxPosition = str.length();
        delimiters = delim;
        retTokens = returnTokens;
    }
    public StringTokenizer(String str, String delim) { this(str, delim, false); }
    public StringTokenizer(String str) { this(str, " \t\n\r", false); }

    private void skipDelimiters() {
        while (!retTokens && (currentPosition < maxPosition) &&
               (delimiters.indexOf(str.charAt(currentPosition)) >= 0)) {
            currentPosition = currentPosition + 1;
        }
    }

    public boolean hasMoreTokens() {
        skipDelimiters();
        return currentPosition < maxPosition;
    }

    public String nextToken() {
        skipDelimiters();
        if (currentPosition >= maxPosition) throw new NoSuchElementException();
        int start = currentPosition;
        while ((currentPosition < maxPosition) &&
               (delimiters.indexOf(str.charAt(currentPosition)) < 0)) {
            currentPosition = currentPosition + 1;
        }
        if (retTokens && (start == currentPosition) &&
            (delimiters.indexOf(str.charAt(currentPosition)) >= 0)) {
            currentPosition = currentPosition + 1;
        }
        return str.substring(start, currentPosition);
    }

    public String nextToken(String delim) {
        delimiters = delim;
        return nextToken();
    }

    public boolean hasMoreElements() { return hasMoreTokens(); }
    public Object nextElement() { return nextToken(); }

    public int countTokens() {
        int count = 0;
        int currpos = currentPosition;
        while (currpos < maxPosition) {
            while (!retTokens && (currpos < maxPosition) &&
                   (delimiters.indexOf(str.charAt(currpos)) >= 0)) {
                currpos = currpos + 1;
            }
            if (currpos >= maxPosition) break;
            int start = currpos;
            while ((currpos < maxPosition) &&
                   (delimiters.indexOf(str.charAt(currpos)) < 0)) {
                currpos = currpos + 1;
            }
            if (retTokens && (start == currpos) &&
                (delimiters.indexOf(str.charAt(currpos)) >= 0)) {
                currpos = currpos + 1;
            }
            count = count + 1;
        }
        return count;
    }
}
