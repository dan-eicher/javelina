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

    // ── parse — a token scanner over the interoperable formats Date produces (and RFC 822):
    // day-of-week and "GMT"/"UTC" are ignored, a NAME picks the month, a "h:m[:s]" run is the
    // time, a 4-digit or >31 number is the year, else the first small number is the day. §21.2.
    private static int monthOf(String s, int start, int end) {
        for (int k = 0; k < 12; k++) {
            boolean hit = (end - start) >= 3;
            for (int j = 0; hit && j < 3; j++) {
                char a = s.charAt(start + j), b = mtb[k].charAt(j);
                if (a >= 'A' && a <= 'Z') a = (char) (a + 32);
                if (b >= 'A' && b <= 'Z') b = (char) (b + 32);
                if (a != b) hit = false;
            }
            if (hit) return k;
        }
        return -1;
    }

    public static long parse(String s) {
        int year = -1, mon = -1, mday = -1, hour = 0, min = 0, sec = 0;
        int i = 0, n = s.length();
        while (i < n) {
            char c = s.charAt(i);
            if (c <= ' ' || c == ',' || c == '-' || c == '/') { i = i + 1; continue; }
            if (c >= '0' && c <= '9') {
                int num = 0, digits = 0;
                while (i < n && (c = s.charAt(i)) >= '0' && c <= '9') { num = num * 10 + (c - '0'); i = i + 1; digits = digits + 1; }
                if (i < n && s.charAt(i) == ':') {                 // a "h:m[:s]" time run
                    hour = num; i = i + 1;
                    int m2 = 0; while (i < n && (c = s.charAt(i)) >= '0' && c <= '9') { m2 = m2 * 10 + (c - '0'); i = i + 1; }
                    min = m2;
                    if (i < n && s.charAt(i) == ':') {
                        i = i + 1; int s2 = 0;
                        while (i < n && (c = s.charAt(i)) >= '0' && c <= '9') { s2 = s2 * 10 + (c - '0'); i = i + 1; }
                        sec = s2;
                    }
                } else if (digits >= 4 || num > 31) {
                    year = num;
                } else if (mday < 0) {
                    mday = num;
                } else {
                    year = num;
                }
            } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                int start = i;
                while (i < n && ((c = s.charAt(i)) >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z')) i = i + 1;
                int mm = monthOf(s, start, i);
                if (mm >= 0) mon = mm;                             // else a day-of-week / GMT / UTC → ignore
            } else {
                i = i + 1;
            }
        }
        if (year >= 1900) year = year - 1900;                      // 4-digit year → year-1900
        return UTC(year, mon, mday, hour, min, sec);
    }
}
