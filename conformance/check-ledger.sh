#!/bin/sh
# check-ledger.sh — the integrity gate over conformance/jls-ledger.tsv.
#
# The ledger is TRANSCRIBED from a PDF table of contents, so there is no `diff` against a
# machine-readable original to lean on. These checks exist because the failure mode of
# transcription is DROPPING A LINE, and a dropped line is invisible in a list you wrote
# yourself. The ToC is numbered and page-stamped, which makes the omission detectable:
#
#   1. gapless numbering  — children of each parent run 1..n; a dropped line leaves a hole
#   2. monotone pages     — printed pages never decrease down the file; a mis-ordered or
#                           invented row breaks it
#   3. chapter anchoring  — each chapter's sections live between its start page and the
#                           next chapter's, per the ToC's chapter lines
#
# Plus the status discipline: every row has a status, and N/A must carry a reason.
#
# Exit code is the contract. Prints the UNCOVERED count, which is the number that must fall
# and must never rise.

set -e
cd "$(dirname "$0")"

LEDGER=jls-ledger.tsv
[ -f "$LEDGER" ] || { echo "  FAIL  check-ledger: $LEDGER missing"; exit 1; }

# The ratchet's ceiling — the last number, so the file can carry its own explanation.
FLOOR=$(grep -vE '^[[:space:]]*(#|$)' uncovered-floor 2>/dev/null | tail -1)
[ -n "$FLOOR" ] || { echo "  FAIL  check-ledger: conformance/uncovered-floor missing or empty"; exit 1; }

awk -F'\t' -v FLOOR="$FLOOR" '
BEGIN {
    # Chapter start pages, read off the ToC chapter lines (java-langspec-1.0.pdf pp.7-19).
    # 23 is a sentinel: the body ends at 765, Index begins at 767.
    split("1 7 11 29 51 77 113 127 183 193 201 215 237 263 301 383 399 419 433 455 615 665 767", cs, " ")
    fails = 0; uncovered = 0; n_a = 0; covered = 0; rows = 0; prevpage = 0
}
/^#/ || /^[ \t]*$/ { next }
{
    rows++
    sec = $1; title = $2; page = $3 + 0; status = $4; reason = $5

    if (NF < 4) { printf "  FAIL  row %d (%s): fewer than 4 fields\n", NR, sec; fails++; next }
    if (title == "") { printf "  FAIL  %s: empty title\n", sec; fails++ }

    # ── status discipline ──
    if (status == "UNCOVERED") uncovered++
    else if (status == "COVERED") {
        covered++
        if (reason == "") { printf "  FAIL  %s: COVERED with no template ids\n", sec; fails++ }
    }
    else if (status == "N/A") {
        n_a++
        if (reason == "") { printf "  FAIL  %s: N/A with no reason — \"out of scope\" is not a reason\n", sec; fails++ }
        # N/A means NO PROGRAM ON THIS TARGET CAN OBSERVE IT. It does not mean "we have not
        # written it" — that is what UNCOVERED is for, and the difference matters because an
        # N/A row is invisible to the ratchet FOREVER while an UNCOVERED row is work the
        # count still owes.
        #
        # This check exists because N/A became the hiding place: five library sections were
        # marked N/A with reasons like "No Runtime.java", "exists but is a 5-line empty
        # placeholder", "not part of the implemented runtime". Every one describes what has
        # been WRITTEN, not what is OBSERVABLE — java.io.StreamTokenizer is pure Java needing
        # no thread and no host capability, so it is unwritten, not inapplicable.
        #
        # A reason naming a missing file, a stub, or a deferral is therefore rejected. It
        # cannot catch an implementation statement dressed in other words — but it closes the
        # phrasing that actually got used.
        if (reason ~ /not part of the implemented/ || reason ~ /placeholder/ ||
            reason ~ /-line stub/ || reason ~ /unwritten/ || reason ~ /not yet implemented/ ||
            reason ~ /[Dd]eferred/ || reason ~ /[Nn]o [A-Z][A-Za-z]*\.java/) {
            printf "  FAIL  %s: N/A justified by IMPLEMENTATION STATE, not observability:\n", sec
            printf "        \"%s\"\n", substr(reason, 1, 90)
            printf "        Unwritten is UNCOVERED. N/A is for what no program on this target can observe.\n"
            fails++
        }
    }
    else { printf "  FAIL  %s: status \"%s\" is not COVERED|N/A|UNCOVERED\n", sec, status; fails++ }

    # ── 2. monotone pages ──
    if (page < prevpage) {
        printf "  FAIL  %s: page %d follows page %d — pages must not decrease\n", sec, page, prevpage
        fails++
    }
    prevpage = page

    # ── 1. gapless numbering ──
    n = split(sec, part, ".")
    parent = ""
    for (i = 1; i < n; i++) parent = parent (i > 1 ? "." : "") part[i]
    child = part[n] + 0
    key = (parent == "" ? "<root>" : parent)
    expect = last[key] + 1
    if (child != expect) {
        printf "  FAIL  %s: expected %s%s%d — a hole here means a dropped ToC line\n", \
               sec, parent, (parent == "" ? "" : "."), expect
        fails++
    }
    if (child > last[key]) last[key] = child

    # ── 3. chapter anchoring ──
    ch = part[1] + 0
    seen_ch[ch] = 1
    if (ch < 1 || ch > 22) { printf "  FAIL  %s: chapter %d outside 1..22\n", sec, ch; fails++ }
    else if (page < cs[ch] || page >= cs[ch + 1]) {
        printf "  FAIL  %s: page %d outside chapter %d (%d..%d)\n", sec, page, ch, cs[ch], cs[ch + 1] - 1
        fails++
    }
}
END {
    # A whole chapter can go missing without tripping the per-parent check above, because its
    # rows simply never appear and nothing is left to leave a hole in.
    for (c = 1; c <= 22; c++)
        if (!seen_ch[c]) { printf "  FAIL  chapter %d has no rows at all\n", c; fails++ }
    printf "\n  ledger: %d rows — %d covered, %d n/a, %d UNCOVERED (ceiling %d)\n",
           rows, covered, n_a, uncovered, FLOOR
    # THE RATCHET. Standing still is allowed; going backwards is not. A rise means a test was
    # deleted, a marker dropped, or a section reclassified out of COVERED — each of which
    # wants explaining, and none of which should be absorbable by a number nobody reads.
    if (uncovered > FLOOR + 0) {
        printf "  FAIL  UNCOVERED rose from %d to %d — coverage went BACKWARDS.\n", FLOOR, uncovered
        printf "        Do not raise conformance/uncovered-floor to make this pass.\n"
        fails++
    } else if (uncovered < FLOOR + 0) {
        printf "  ....  UNCOVERED fell to %d (ceiling %d) — lower the floor in conformance/uncovered-floor\n",
               uncovered, FLOOR
    }
    if (fails) { printf "  FAIL  check-ledger: %d problem(s)\n", fails; exit 1 }
    printf "  ....  check-ledger: numbering gapless, pages monotone, chapters anchored, ratchet held\n"
}
' "$LEDGER"
