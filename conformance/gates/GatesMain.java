// GatesMain -- the inventory and coverage gates, decided ON the VM.
//
//   javelina --jre <jre.wasm> --root <repo-root> gates.wasm <conformance-dir>
//
// crisp-tallying-chapters §3 puts the instrument in Java, and these are instrument: every one
// of them is a decision about files already on disk, which is precisely what the guest can do.
// What it cannot do is spawn javelinac -- §20.16 Runtime is not part of the implemented runtime
// -- so building and running stays with the driver outside, and nothing else does.
//
// Replaces check-ledger.sh, join-ledger.sh and check-cardinality.sh: ~310 lines of awk in three
// dialects that decided whether the corpus was honest while exercising nothing.
//
// The gates, and what each one catches:
//
//   transcription   gapless numbering, monotone pages, chapter anchoring -- §2's three, "each
//                   of which fails on exactly the mistake a transcriber makes"
//   status          every row COVERED|N/A|UNCOVERED, every N/A carrying a written reason, and
//                   no N/A justified by implementation state (an N/A row is invisible to the
//                   ratchet forever, so "not written yet" there is a permanent hiding place)
//   join            markers reconciled BOTH ways: a claim with no test, and a test the ledger
//                   has not recorded. One direction alone is useless.
//   ratchet         UNCOVERED may fall, never rise
//   cardinality     a section whose own text states a count has at least that many cases
public class GatesMain {

    public static void main(String[] args) {
        String dir = args.length > 0 ? args[0] : "conformance";

        Ledger L = Ledger.load(dir + "/jls-ledger.tsv");
        if (L == null) { fail("gates: " + dir + "/jls-ledger.tsv is unreadable"); return; }
        if (L.rows == 0) { fail("gates: the ledger is empty -- every gate below would pass vacuously"); return; }

        int fails = 0;
        fails += transcription(L);
        fails += statusDiscipline(L);

        Markers m = Markers.scan(dir);
        fails += join(L, m);
        fails += ratchet(L, dir);
        fails += cardinality(dir);

        if (fails != 0) {
            System.out.println("  FAIL  gates: " + fails + " problem(s)");
            System.exit(1);
            return;
        }
        System.out.println("  ....  gates: inventory, join, ratchet and cardinality all hold");
    }

    private static void fail(String msg) { System.out.println("  FAIL  " + msg); System.exit(1); }

    /* ── §2's three transcription invariants ────────────────────────────────────────── */
    private static int transcription(Ledger L) {
        int fails = 0, prevPage = 0;
        boolean[] seenCh = new boolean[24];

        for (int i = 0; i < L.rows; i++) {
            String s = L.sec[i];
            if (L.title[i].length() == 0) {
                System.out.println("  FAIL  " + s + ": empty title"); fails++;
            }

            // 2. monotone pages -- a mis-ordered or mistyped page shows here
            if (L.page[i] < prevPage) {
                System.out.println("  FAIL  " + s + ": page " + L.page[i] + " follows page "
                                 + prevPage + " -- pages must not decrease");
                fails++;
            }
            if (L.page[i] > prevPage) prevPage = L.page[i];

            // 1. gapless numbering -- children of each parent run 1..n; a dropped ToC line
            //    leaves a hole, and a hole is the shape of the mistake
            String parent = Ledger.parentOf(s);
            int leaf = Ledger.leafOf(s);
            if (parent.length() > 0 && leaf > 1) {
                String prevSibling = parent + "." + (leaf - 1);
                if (L.indexOf(prevSibling) < 0) {
                    System.out.println("  FAIL  " + s + ": expected " + prevSibling
                                     + " -- a hole here means a dropped ToC line");
                    fails++;
                }
            }

            // 3. chapter anchoring -- a section's page lies inside its own chapter's range
            int ch = Ledger.chapterOf(s);
            if (ch < 1 || ch > 22) {
                System.out.println("  FAIL  " + s + ": chapter " + ch + " outside 1..22");
                fails++;
            } else {
                seenCh[ch] = true;
                int lo = Ledger.CHAPTER_START[ch], hi = Ledger.CHAPTER_START[ch + 1] - 1;
                if (L.page[i] < lo || L.page[i] > hi) {
                    System.out.println("  FAIL  " + s + ": page " + L.page[i]
                                     + " outside chapter " + ch + " (" + lo + ".." + hi + ")");
                    fails++;
                }
            }
        }
        for (int c = 1; c <= 22; c++)
            if (!seenCh[c]) { System.out.println("  FAIL  chapter " + c + " has no rows at all"); fails++; }
        return fails;
    }

    /* ── status discipline ──────────────────────────────────────────────────────────── */
    private static int statusDiscipline(Ledger L) {
        int fails = 0;
        for (int i = 0; i < L.rows; i++) {
            String st = L.status[i], s = L.sec[i], why = L.reason[i];
            if (st.equals("COVERED")) {
                if (why.length() == 0) {
                    System.out.println("  FAIL  " + s + ": COVERED with no template ids"); fails++;
                }
            } else if (st.equals("N/A")) {
                if (why.length() == 0) {
                    System.out.println("  FAIL  " + s
                        + ": N/A with no reason -- \"out of scope\" is not a reason");
                    fails++;
                } else if (impliesUnwritten(why)) {
                    // An N/A row never appears in the ratchet again, so "not implemented yet"
                    // parked there is permanent. Unwritten is UNCOVERED.
                    System.out.println("  FAIL  " + s
                        + ": N/A justified by IMPLEMENTATION STATE, not observability:");
                    System.out.println("        " + why);
                    fails++;
                }
            } else if (!st.equals("UNCOVERED")) {
                System.out.println("  FAIL  " + s + ": status \"" + st
                                 + "\" is not COVERED|N/A|UNCOVERED");
                fails++;
            }
        }
        return fails;
    }

    /** Phrases that describe what has been WRITTEN rather than what is OBSERVABLE. */
    private static boolean impliesUnwritten(String why) {
        String w = lower(why);
        return w.indexOf("not implemented") >= 0
            || w.indexOf("unimplemented")   >= 0
            || w.indexOf("placeholder")     >= 0
            || w.indexOf("not written")     >= 0
            || w.indexOf("no such class")   >= 0
            || w.indexOf("does not exist in the runtime") >= 0;
    }

    private static String lower(String s) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            b.append(c >= 'A' && c <= 'Z' ? (char)(c + 32) : c);
        }
        return b.toString();
    }

    /* ── the join, failing BOTH ways ────────────────────────────────────────────────── */
    private static int join(Ledger L, Markers m) {
        int fails = 0;
        for (int i = 0; i < L.rows; i++) {
            boolean claimed = m.covers(L.sec[i]);
            if (claimed && L.status[i].equals("N/A")) {
                System.out.println("  FAIL  " + L.sec[i] + ": a test claims it (" + m.by(L.sec[i])
                                 + ") but the ledger says N/A -- one of them is wrong");
                fails++;
            } else if (claimed && !L.status[i].equals("COVERED")) {
                System.out.println("  FAIL  " + L.sec[i] + ": covered by " + m.by(L.sec[i])
                                 + ", but the ledger says " + L.status[i]);
                fails++;
            } else if (!claimed && L.status[i].equals("COVERED")) {
                System.out.println("  FAIL  " + L.sec[i] + ": marked COVERED (" + L.reason[i]
                                 + ") but NO test carries a `// JLS " + L.sec[i] + "` marker");
                fails++;
            }
        }
        // ...and the other direction: a marker naming a section the inventory does not have.
        for (int i = 0; i < m.count; i++) {
            if (L.indexOf(m.section[i]) < 0) {
                System.out.println("  FAIL  " + m.section[i] + ": claimed by " + m.artifact[i]
                                 + " but there is NO SUCH SECTION in the ledger");
                fails++;
            }
        }
        return fails;
    }

    /* ── the ratchet ────────────────────────────────────────────────────────────────── */
    private static int ratchet(Ledger L, String dir) {
        int uncovered = 0, covered = 0, na = 0;
        for (int i = 0; i < L.rows; i++) {
            if (L.status[i].equals("COVERED")) covered++;
            else if (L.status[i].equals("N/A")) na++;
            else uncovered++;
        }
        String f = Tsv.lastValueLine(dir + "/uncovered-floor");
        int floor = f == null ? -1 : Tsv.parseInt(f, -1);
        System.out.println("  ledger: " + L.rows + " rows -- " + covered + " covered, "
                         + na + " n/a, " + uncovered + " UNCOVERED (ceiling "
                         + (floor < 0 ? "?" : "" + floor) + ")");
        if (floor < 0) {
            System.out.println("  FAIL  conformance/uncovered-floor missing or not a number");
            return 1;
        }
        if (uncovered > floor) {
            System.out.println("  FAIL  UNCOVERED rose from " + floor + " to " + uncovered
                             + " -- coverage went BACKWARDS.");
            System.out.println("        Raise the floor only when hidden work became VISIBLE,"
                             + " never to absorb a deleted test.");
            return 1;
        }
        return 0;
    }

    /* ── declared cardinalities ─────────────────────────────────────────────────────── */
    private static int cardinality(String dir) {
        String[][] table = Tsv.read(dir + "/cardinality.tsv");
        if (table == null) { System.out.println("  FAIL  cardinality.tsv missing"); return 1; }
        String[][] man = Tsv.read(dir + "/generated/SECTIONS.tsv");
        if (man == null) {
            System.out.println("  FAIL  generated/SECTIONS.tsv missing -- the generator did not run,");
            System.out.println("        so every declared cardinality is unverified rather than met");
            return 1;
        }
        if (table.length == 0) {
            System.out.println("  FAIL  cardinality.tsv has no rows -- the gate would pass vacuously");
            return 1;
        }

        int fails = 0;
        for (int i = 0; i < table.length; i++) {
            String sec = table[i][0];
            int want = table[i].length > 1 ? Tsv.parseInt(table[i][1], -1) : -1;
            String sentence = table[i].length > 2 ? table[i][2] : "";
            int got = distinctFor(man, sec);
            if (got < want) {
                System.out.println("  FAIL  " + sec + " declares " + want + " cases, "
                                 + got + " reached a written case");
                System.out.println("        " + sentence);
                fails++;
            } else {
                System.out.println("  ....  " + sec + ": " + got + " of " + want + " declared cases");
            }
        }
        return fails;
    }

    /** Distinct snippet ids claiming `sec` in the generator's manifest. Distinct, because the
     *  same snippet reaching several cases is one case for the section, not several. */
    private static int distinctFor(String[][] man, String sec) {
        java.util.Vector seen = new java.util.Vector();
        for (int i = 0; i < man.length; i++) {
            if (man[i].length < 2) continue;
            if (!man[i][0].equals(sec)) continue;
            if (!seen.contains(man[i][1])) seen.addElement(man[i][1]);
        }
        return seen.size();
    }
}
