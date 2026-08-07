// Pins for java.util.regex.
//
// The regexredux section is the point of the exercise: those are the actual
// patterns and the actual substitutions the Computer Language Benchmarks Game
// task runs, and they are the reason this package exists at all.
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

public class PatternSmoke {

    static int failures = 0;

    public static int run() {
        basics();
        groups();
        quantifiers();
        classesAndEscapes();
        replacement();
        splitting();
        rejected();
        regexredux();
        return failures;
    }

    static void ok(boolean cond, String what) {
        if (!cond) failures++;
        System.out.println((cond ? "ok   " : "FAIL ") + "pattern: " + what);
    }

    static int count(String regex, String subject) {
        Matcher m = Pattern.compile(regex).matcher(subject);
        int n = 0;
        while (m.find()) n++;
        return n;
    }

    static void basics() {
        ok(Pattern.matches("abc", "abc"), "matches an exact string");
        ok(!Pattern.matches("abc", "abcd"), "matches() needs the WHOLE input");
        ok(Pattern.compile("abc").matcher("abcd").lookingAt(), "lookingAt takes a prefix");
        ok(Pattern.compile("b").matcher("abc").find(), "find searches");
        ok(!Pattern.compile("z").matcher("abc").find(), "find reports absence");

        // Ordered alternation reaches the public API unchanged: this is the one
        // behaviour that separates a regex from a regular expression.
        Matcher m = Pattern.compile("a|ab").matcher("ab");
        ok(m.find() && m.end() == 1, "a|ab matches just \"a\" - alternation is ordered");
        Matcher m2 = Pattern.compile("ab|a").matcher("ab");
        ok(m2.find() && m2.end() == 2, "ab|a matches \"ab\"");
    }

    static void groups() {
        Matcher m = Pattern.compile("(a+)(b+)").matcher("xaaabbz");
        ok(m.find(), "grouped pattern is found");
        ok("aaabb".equals(m.group()), "group() is the whole match");
        ok(m.start() == 1 && m.end() == 6, "start/end bound the whole match");
        ok("aaa".equals(m.group(1)), "group 1");
        ok("bb".equals(m.group(2)), "group 2");
        ok(m.groupCount() == 2, "groupCount counts the parenthesised groups only");

        // A group inside an alternative that was not taken did not participate.
        Matcher m3 = Pattern.compile("(a)|(b)").matcher("b");
        ok(m3.find() && m3.group(1) == null && "b".equals(m3.group(2)),
           "a group that did not participate is null, not empty");

        // (?:...) groups without capturing, so it does not shift the numbering.
        Matcher m4 = Pattern.compile("(?:a)(b)").matcher("ab");
        ok(m4.find() && "b".equals(m4.group(1)) && m4.groupCount() == 1,
           "(?:) does not take a group number");
    }

    static void quantifiers() {
        ok(Pattern.matches("a*", ""), "a* matches empty");
        ok(Pattern.matches("a*", "aaa"), "a* matches many");
        ok(!Pattern.matches("a+", ""), "a+ needs one");
        ok(Pattern.matches("ab?c", "ac"), "b? may be absent");
        ok(Pattern.matches("ab?c", "abc"), "b? may be present");
        ok(Pattern.matches("a{3}", "aaa"), "a{3}");
        ok(!Pattern.matches("a{3}", "aa"), "a{3} rejects two");
        ok(Pattern.matches("a{2,4}", "aaa"), "a{2,4}");
        ok(!Pattern.matches("a{2,4}", "a"), "a{2,4} rejects one");
        ok(Pattern.matches("a{2,}", "aaaaa"), "a{2,} is unbounded above");

        // Lazy takes as little as it can, greedy as much.
        Matcher lazy = Pattern.compile("<(.*?)>").matcher("<a><b>");
        ok(lazy.find() && "a".equals(lazy.group(1)), "lazy .*? stops at the first >");
        Matcher greedy = Pattern.compile("<(.*)>").matcher("<a><b>");
        ok(greedy.find() && "a><b".equals(greedy.group(1)), "greedy .* runs to the last >");
    }

    static void classesAndEscapes() {
        ok(Pattern.matches("[abc]+", "cab"), "a character class");
        ok(Pattern.matches("[a-z]+", "hello"), "a range");
        ok(!Pattern.matches("[a-z]+", "Hello"), "a range is case sensitive");
        ok(Pattern.matches("[^0-9]+", "abc"), "a negated class");
        ok(Pattern.matches("[cgt]gggtaaa", "cgggtaaa"), "regexredux-shaped class");
        ok(Pattern.matches("\\d+", "12345"), "\\d");
        ok(Pattern.matches("\\w+", "a_1"), "\\w");
        ok(Pattern.matches("a\\.b", "a.b"), "an escaped dot is literal");
        ok(!Pattern.matches("a\\.b", "axb"), "and does not match any character");
        ok(Pattern.matches("\\|+", "||"), "an escaped pipe");
        ok(Pattern.compile("a$").matcher("ba").find(), "$ at end of input");
        ok(Pattern.compile("^a").matcher("ab").find(), "^ at start of input");
    }

    static void replacement() {
        ok("xbc".equals(Pattern.compile("a").matcher("abc").replaceAll("x")),
           "replaceAll one occurrence");
        ok("xxx".equals(Pattern.compile("a").matcher("aaa").replaceAll("x")),
           "replaceAll every occurrence");
        ok("xaa".equals(Pattern.compile("a").matcher("aaa").replaceFirst("x")),
           "replaceFirst stops after one");
        ok("[b][d]".equals(Pattern.compile("a(b)c(d)e").matcher("abcde")
                                  .replaceAll("[$1][$2]")),
           "$n in a replacement names a group");
        ok("a$b".equals(Pattern.compile("X").matcher("aXb").replaceAll("\\$")),
           "a backslash escapes the dollar");
    }

    static void splitting() {
        String[] a = Pattern.compile(",").split("a,b,c");
        ok(a.length == 3 && "a".equals(a[0]) && "c".equals(a[2]), "split on a literal");
        String[] b = Pattern.compile("\\s+").split("a  b\tc");
        ok(b.length == 3, "split on whitespace runs");
        String[] c = Pattern.compile(",").split("a,b,c", 2);
        ok(c.length == 2 && "b,c".equals(c[1]), "split honours a limit");
        ok(Pattern.matches(Pattern.quote("a.b"), "a.b"), "quote makes a literal pattern");
        ok(!Pattern.matches(Pattern.quote("a.b"), "axb"), "and it really is literal");
    }

    // A backreference is not expressible as a PEG and the paper does not cover
    // it, so it is rejected with a message that says why rather than silently
    // treated as a literal.
    static void rejected() {
        boolean caught = false;
        String msg = "";
        try {
            Pattern.compile("(a)\\1");
        } catch (PatternSyntaxException e) {
            caught = true;
            msg = e.getMessage();
        }
        ok(caught, "a backreference is rejected");
        ok(msg.indexOf("backreference") >= 0, "and the message says what was wrong");

        boolean caught2 = false;
        try { Pattern.compile("(a"); } catch (PatternSyntaxException e) { caught2 = true; }
        ok(caught2, "an unbalanced group is rejected");
    }

    // ── The Computer Language Benchmarks Game's regexredux ──
    //
    // The nine 8-mer patterns and the five substitutions, verbatim from the
    // task description. This is what the whole PEG machine was built for.
    static void regexredux() {
        String[] variants = {
            "agggtaaa|tttaccct",
            "[cgt]gggtaaa|tttaccc[acg]",
            "a[act]ggtaaa|tttacc[agt]t",
            "ag[act]gtaaa|tttac[agt]ct",
            "agg[act]taaa|ttta[agt]cct",
            "aggg[acg]aaa|ttt[cgt]ccct",
            "agggt[cgt]aa|tt[acg]accct",
            "agggta[cgt]a|t[acg]taccct",
            "agggtaa[cgt]|[acg]ttaccct"
        };
        boolean allCompiled = true;
        for (int i = 0; i < variants.length; i++) {
            try { Pattern.compile(variants[i]); }
            catch (PatternSyntaxException e) { allCompiled = false; }
        }
        ok(allCompiled, "all nine regexredux 8-mer patterns compile");

        // A subject with a known number of hits: three agggtaaa and two
        // tttaccct, so the first variant must find five.
        String dna = "ttagggtaaaccagggtaaagtttaccctgcagggtaaatttaccctaa";
        ok(count(variants[0], dna) == 5,
           "variant 1 finds all five occurrences (3 forward, 2 reverse)");
        // Variant 2 requires a flanking [cgt] / [acg], which none of the five
        // above has — every one is preceded by 'a' and followed by 't'. So the
        // right answer here is none, and a subject that DOES satisfy the
        // flanks is a separate case.
        ok(count(variants[1], dna) == 0,
           "variant 2 rejects all five: the flanks do not match");
        ok(count(variants[1], "cgggtaaaxxtttacccg") == 2,
           "variant 2 finds both forms when the flanks do match");

        // The five substitutions, applied in order, exactly as the task states.
        String seq = "tHaNtaNdaNBYcaNHaDWaS";
        seq = Pattern.compile("tHa[Nt]").matcher(seq).replaceAll("<4>");
        seq = Pattern.compile("aND|caN|Ha[DS]|WaS").matcher(seq).replaceAll("<3>");
        seq = Pattern.compile("a[NSt]|BY").matcher(seq).replaceAll("<2>");
        seq = Pattern.compile("<[^>]*>").matcher(seq).replaceAll("|");
        seq = Pattern.compile("\\|[^|][^|]*\\|").matcher(seq).replaceAll("-");
        ok(seq.indexOf('<') < 0 && seq.indexOf('>') < 0,
           "the five magic substitutions run in order and leave no markers");

        // The <...> to | rule and the |...| to - rule composing, which is the
        // step that actually shortens the sequence.
        String t = Pattern.compile("<[^>]*>").matcher("a<xx>b<yy>c").replaceAll("|");
        ok("a|b|c".equals(t), "<[^>]*> collapses each marker to a pipe");
        String u = Pattern.compile("\\|[^|][^|]*\\|").matcher("a|bb|c").replaceAll("-");
        ok("a-c".equals(u), "\\|[^|][^|]*\\| collapses a piped run to a dash");
    }
}
