package java.util;

// java.util.Date (JLS 1.0 §21.2) — a point in time, millisecond precision. With no timezone
// native, the local zone IS UTC (getTimezoneOffset() == 0), the embedder's default floor;
// System.currentTimeMillis() likewise reports UTC. Ported minus `synchronized`.
public class Date {
    private long value;                    // milliseconds since 1970-01-01T00:00:00Z

    private static final String[] wtb = {  // day-of-week names, Sunday first (getDay() order)
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    private static final String[] mtb = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    public Date() { this(System.currentTimeMillis()); }
    public Date(long time) { value = time; }
    public Date(int year, int month, int date) { value = UTC(year, month, date, 0, 0, 0); }
    public Date(int year, int month, int date, int hrs, int min) { value = UTC(year, month, date, hrs, min, 0); }
    public Date(int year, int month, int date, int hrs, int min, int sec) { value = UTC(year, month, date, hrs, min, sec); }
    public Date(String s) { value = parse(s); }

    // ── calendar conversion (proleptic Gregorian, H. Hinnant's algorithms) ───────────────
    private static long fdiv(long a, long b) {          // floor division (Java / truncates)
        long q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0))) q = q - 1;
        return q;
    }

    private static long daysFromCivil(long y, long m, long d) {   // m in 1..12, y actual year
        y = y - (m <= 2 ? 1 : 0);
        long era = (y >= 0 ? y : y - 399) / 400;
        long yoe = y - era * 400;
        long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    public static long UTC(int year, int month, int date, int hrs, int min, int sec) {
        long y  = (long) year + 1900;
        long mo = (long) month;
        y  = y + fdiv(mo, 12);                    // normalize month into 0..11, carrying years
        mo = mo - fdiv(mo, 12) * 12;
        long days = daysFromCivil(y, mo + 1, date);
        return days * 86400000L + (long) hrs * 3600000L + (long) min * 60000L + (long) sec * 1000L;
    }

    private long dayNumber() { return fdiv(value, 86400000L); }

    private long[] civil() {                       // { actual-year, month 1..12, day 1..31 }
        long z   = dayNumber() + 719468;
        long era = (z >= 0 ? z : z - 146096) / 146097;
        long doe = z - era * 146097;
        long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        long y   = yoe + era * 400;
        long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long mp  = (5 * doy + 2) / 153;
        long d   = doy - (153 * mp + 2) / 5 + 1;
        long m   = mp + (mp < 10 ? 3 : -9);
        y = y + (m <= 2 ? 1 : 0);
        long[] r = new long[3];
        r[0] = y; r[1] = m; r[2] = d;
        return r;
    }
    private long timeOfDay() { return value - dayNumber() * 86400000L; }   // [0, 86400000)

    public int getYear()    { return (int) (civil()[0] - 1900); }
    public int getMonth()   { return (int) (civil()[1] - 1); }
    public int getDate()    { return (int) (civil()[2]); }
    public int getDay()     { long w = (dayNumber() + 4) % 7; if (w < 0) w = w + 7; return (int) w; }
    public int getHours()   { return (int) (timeOfDay() / 3600000L); }
    public int getMinutes() { return (int) ((timeOfDay() / 60000L) % 60); }
    public int getSeconds() { return (int) ((timeOfDay() / 1000L) % 60); }

    public void setYear(int year)   { long[] c = civil(); value = UTC(year, (int) (c[1] - 1), (int) c[2], getHours(), getMinutes(), getSeconds()); }
    public void setMonth(int month) { long[] c = civil(); value = UTC((int) (c[0] - 1900), month, (int) c[2], getHours(), getMinutes(), getSeconds()); }
    public void setDate(int date)   { long[] c = civil(); value = UTC((int) (c[0] - 1900), (int) (c[1] - 1), date, getHours(), getMinutes(), getSeconds()); }
    public void setHours(int hrs)   { long[] c = civil(); value = UTC((int) (c[0] - 1900), (int) (c[1] - 1), (int) c[2], hrs, getMinutes(), getSeconds()); }
    public void setMinutes(int min) { long[] c = civil(); value = UTC((int) (c[0] - 1900), (int) (c[1] - 1), (int) c[2], getHours(), min, getSeconds()); }
    public void setSeconds(int sec) { long[] c = civil(); value = UTC((int) (c[0] - 1900), (int) (c[1] - 1), (int) c[2], getHours(), getMinutes(), sec); }

    public long getTime() { return value; }
    public void setTime(long time) { value = time; }

    public boolean before(Date when) { return value < when.getTime(); }
    public boolean after(Date when)  { return value > when.getTime(); }

    public boolean equals(Object obj) { return (obj instanceof Date) && (value == ((Date) obj).getTime()); }
    public int hashCode() { return (int) (value ^ (value >>> 32)); }

    public int getTimezoneOffset() { return 0; }   // UTC floor: no timezone native, so no offset

    private static String two(int n) { return (n < 10) ? ("0" + n) : Integer.toString(n); }

    public String toString() {              // "EEE MMM dd HH:mm:ss GMT yyyy"
        long[] c = civil();
        return wtb[getDay()] + " " + mtb[(int) (c[1] - 1)] + " " + two((int) c[2]) + " "
             + two(getHours()) + ":" + two(getMinutes()) + ":" + two(getSeconds())
             + " GMT " + Long.toString(c[0]);
    }

    public String toLocaleString() { return toString(); }

    public String toGMTString() {           // "d MMM yyyy HH:mm:ss GMT"
        long[] c = civil();
        return Long.toString(c[2]) + " " + mtb[(int) (c[1] - 1)] + " " + Long.toString(c[0]) + " "
             + two(getHours()) + ":" + two(getMinutes()) + ":" + two(getSeconds()) + " GMT";
    }

    // ── parse (§21.3.31) ────────────────────────────────────────────────────────────────────
    // Transcribed from the spec's rules rather than guessed from shape. The old scanner
    // classified a number by how it LOOKED ("a 4-digit or >31 number is the year") and threw
    // away '/' as a separator; §21.3.31 classifies each number by WHAT FOLLOWS IT, and a number
    // followed by a slash IS the month — so `2003/9/6` could not be read at all.
    private static final String[] MONTHS = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };
    private static final String[] DAYS = {
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    };
    // EST/CST/MST/PST are 5/6/7/8 hours west; EDT/CDT/MDT/PDT are the same zones in daylight
    // saving time, i.e. one hour less west. Minutes west of Greenwich, parallel to ZONES.
    private static final String[] ZONES  = { "est","edt","cst","cdt","mst","mdt","pst","pdt" };
    private static final int[]    ZONEW  = {  300, 240,  360, 300,  420, 360,  480, 420  };

    private static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c; }

    /** s[a,b) equals w, ignoring case. */
    private static boolean wordEq(String s, int a, int b, String w) {
        if (b - a != w.length()) return false;
        for (int k = 0; k < w.length(); k++) if (lc(s.charAt(a + k)) != w.charAt(k)) return false;
        return true;
    }
    /** s[a,b) is a non-empty prefix of w, ignoring case. */
    private static boolean wordPrefixOf(String s, int a, int b, String w) {
        if (b <= a || b - a > w.length()) return false;
        for (int k = 0; k < b - a; k++) if (lc(s.charAt(a + k)) != w.charAt(k)) return false;
        return true;
    }

    public static long parse(String s) throws IllegalArgumentException {
        int year = -1, mon = -1, mday = -1, hour = -1, min = -1, sec = -1;
        int zoneWestMin = 0; boolean haveZone = false;
        int i = 0, n = s.length(), paren = 0;

        while (i < n) {
            char c = s.charAt(i);
            // "Any material in s that is within the ASCII parentheses ( and ) is ignored.
            //  Parentheses may be nested."
            if (c == '(') { paren = paren + 1; i = i + 1; continue; }
            if (c == ')') { if (paren > 0) paren = paren - 1; i = i + 1; continue; }
            if (paren > 0) { i = i + 1; continue; }

            // A sign is only meaningful in front of a time-zone offset; elsewhere '-' separates.
            int sign = 0;
            if ((c == '+' || c == '-') && i + 1 < n
                    && s.charAt(i + 1) >= '0' && s.charAt(i + 1) <= '9' && year >= 0) {
                sign = (c == '+') ? 1 : -1; i = i + 1; c = s.charAt(i);
            }

            if (c >= '0' && c <= '9') {
                int num = 0;
                while (i < n && (c = s.charAt(i)) >= '0' && c <= '9') { num = num * 10 + (c - '0'); i = i + 1; }
                char next = (i < n) ? s.charAt(i) : '\0';

                if (sign != 0) {
                    // "If a number is preceded by + or - and a year has already been recognized,
                    //  then the number is a time-zone offset. If it is less than 24, it is an
                    //  offset measured in hours. Otherwise ... in minutes, expressed in 24-hour
                    //  time format without punctuation. A preceding + means an eastward offset."
                    int west = (num < 24) ? num * 60 : (num / 100) * 60 + (num % 100);
                    zoneWestMin = (sign > 0) ? -west : west;
                    haveZone = true;
                } else if (num > 70) {
                    // "If a number is greater than 70, it is regarded as a year number. It must
                    //  be followed by a space, comma, slash, or end of string. If it is greater
                    //  than 1900, then 1900 is subtracted from it."
                    if (!(next == '\0' || next == ' ' || next == ',' || next == '/'
                          || Character.isSpace(next)))
                        throw new IllegalArgumentException();
                    year = (num > 1900) ? num - 1900 : num;
                } else if (next == ':') {
                    // "followed by a colon ... an hour, unless an hour has already been
                    //  recognized, in which case ... a minute."
                    if (hour < 0) hour = num; else min = num;
                    i = i + 1;
                } else if (next == '/') {
                    // "followed by a slash ... a month (decreased by 1 to produce 0 to 11),
                    //  unless a month has already been recognized, in which case ... a day."
                    if (mon < 0) mon = num - 1; else mday = num;
                    i = i + 1;
                } else if (next == '\0' || next == ',' || next == '-' || Character.isSpace(next)) {
                    // "if an hour has been recognized but not a minute, it is regarded as a
                    //  minute; otherwise, if a minute has been recognized but not a second, it is
                    //  regarded as a second; otherwise, it is regarded as a day of the month."
                    if (hour >= 0 && min < 0)      min = num;
                    else if (min >= 0 && sec < 0)  sec = num;
                    else if (mon >= 0 && mday >= 0 && year < 0) {
                        // §21.3.31 STOPS SHORT HERE, and its last clause ("otherwise, it is
                        // regarded as a day of the month") would overwrite the day already read
                        // and leave `2/28/08` with no year at all. The reference implementation
                        // takes the third component of m/d/yy as the year, with the two-digit
                        // window 00..68 => 20xx and 69..99 => 19xx. Filling the gap, not carving
                        // one out: without it a format this common cannot be parsed.
                        year = (num > 1900) ? num - 1900 : (num < 69 ? num + 100 : num);
                    }
                    else                           mday = num;
                } else {
                    throw new IllegalArgumentException();
                }
                continue;
            }

            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                int a = i;
                while (i < n && ((c = s.charAt(i)) >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z')) i = i + 1;

                if (wordEq(s, a, i, "am")) {
                    // "ignored (but the parse fails if an hour has not been recognized or is
                    //  less than 1 or greater than 12)". 12am is midnight, hour 0.
                    if (hour < 1 || hour > 12) throw new IllegalArgumentException();
                    if (hour == 12) hour = 0;
                } else if (wordEq(s, a, i, "pm")) {
                    if (hour < 1 || hour > 12) throw new IllegalArgumentException();
                    if (hour != 12) hour = hour + 12;
                } else if (wordEq(s, a, i, "gmt") || wordEq(s, a, i, "ut") || wordEq(s, a, i, "utc")) {
                    zoneWestMin = 0; haveZone = true;
                } else {
                    int k = 0; boolean done = false;
                    for (k = 0; !done && k < DAYS.length; k++)                 // ignored
                        if (wordPrefixOf(s, a, i, DAYS[k])) done = true;
                    // "considering them in the order given here" — so "Ma" is MARCH, not MAY.
                    for (k = 0; !done && k < MONTHS.length; k++)
                        if (wordPrefixOf(s, a, i, MONTHS[k])) { mon = k; done = true; }
                    for (k = 0; !done && k < ZONES.length; k++)
                        if (wordEq(s, a, i, ZONES[k])) { zoneWestMin = ZONEW[k]; haveZone = true; done = true; }
                    if (!done) throw new IllegalArgumentException();
                }
                continue;
            }

            if (c == ',' || c == '-' || c == '/' || c == ':' || Character.isSpace(c)) { i = i + 1; continue; }
            throw new IllegalArgumentException();                  // outside the permitted set
        }

        if (year < 0 || mon < 0 || mday < 0) throw new IllegalArgumentException();
        if (hour < 0) hour = 0;
        if (min  < 0) min  = 0;
        if (sec  < 0) sec  = 0;
        // "If a time zone or time-zone offset has been recognized, then the year, month, day of
        //  month, hour, minute, and second are interpreted in UTC and then the time-zone offset
        //  is applied. Otherwise ... in the local time zone." This runtime has no zone database
        //  and no host native for one, so local IS UTC and the two branches differ only by the
        //  offset — which is zero when none was recognized.
        long t = UTC(year, mon, mday, hour, min, sec);
        return haveZone ? t + (long) zoneWestMin * 60000L : t;
    }
}
