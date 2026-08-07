package java.util.regex;

// A pattern that is not a pattern. Unchecked, like the JDK's, because a bad
// pattern is a programming error rather than a condition a caller recovers
// from.
public class PatternSyntaxException extends IllegalArgumentException {

    private String desc;
    private String pattern;
    private int index;

    public PatternSyntaxException(String desc, String regex, int index) {
        super(desc);
        this.desc = desc;
        this.pattern = regex;
        this.index = index;
    }

    public String getDescription() { return desc; }
    public String getPattern()     { return pattern; }
    public int getIndex()          { return index; }

    public String getMessage() {
        StringBuffer sb = new StringBuffer();
        sb.append(desc);
        if (index >= 0) {
            sb.append(" near index ");
            sb.append(index);
        }
        sb.append("\n");
        sb.append(pattern);
        if (index >= 0) {
            sb.append("\n");
            for (int i = 0; i < index; i++) sb.append(" ");
            sb.append("^");
        }
        return sb.toString();
    }
}
