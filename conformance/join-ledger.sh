#!/bin/sh
# join-ledger.sh — the COVERAGE JOIN, and the gate that makes it impossible to lie about.
#
# Coverage is COMPUTED from the artifacts, never typed. A test claims the sections it covers
# with a `// JLS <section>` marker; this scans for those markers and reconciles them against
# conformance/jls-ledger.tsv.
#
# It fails in BOTH directions, which is the point:
#
#   marker with no COVERED row   writing a test and not claiming it — the UNCOVERED count
#                                overstates the work left, and the ledger stops being the whole
#   COVERED row with no marker   claiming a section whose test was deleted, renamed, or never
#                                existed — coverage that evaporated without the number moving
#
# One direction alone is useless. A gate that only catches the first lets a stale COVERED row
# sit forever; one that only catches the second lets real coverage go unrecorded, which is how
# `UNCOVERED` stayed at 553 while 64 checks were passing.
#
# Usage:  sh conformance/join-ledger.sh          reconcile + rewrite the ledger, fail on mismatch
#         sh conformance/join-ledger.sh --check  reconcile only, never write (the gate)

set -e
cd "$(dirname "$0")/.."

LEDGER=conformance/jls-ledger.tsv
CHECK_ONLY=0
[ "$1" = "--check" ] && CHECK_ONLY=1

[ -f "$LEDGER" ] || { echo "  FAIL  join-ledger: $LEDGER missing"; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ── collect the markers ─────────────────────────────────────────────────────
# `// JLS <section>` in any .java under the corpus. The file that carries the marker is the
# covering artifact, so the ledger records WHICH test covers a section, not merely that one does.
#
# conformance/reject counts the same way. A rule of the form "a compile-time error occurs" is
# covered by a program that FAILS to compile for that reason — there is no runnable assertion
# to write, and leaving those sections UNCOVERED would say the work was outstanding when the
# only artifact the rule admits already exists.
find conformance/jls conformance/reject conformance/generated -name '*.java' 2>/dev/null \
  | sort \
  | while IFS= read -r f; do
      sed -n 's|^[[:space:]]*//[[:space:]]*JLS[[:space:]]\{1,\}\([0-9][0-9.]*\).*|\1|p' "$f" \
        | while IFS= read -r sec; do printf '%s\t%s\n' "$sec" "$(basename "$f" .java)"; done
    done | sort -u > "$TMP/markers"

# section -> comma-joined covering artifacts
awk -F'\t' '{ if ($1 in a) a[$1] = a[$1] "," $2; else a[$1] = $2 }
            END { for (s in a) printf "%s\t%s\n", s, a[s] }' "$TMP/markers" | sort > "$TMP/covers"

# ── reconcile ───────────────────────────────────────────────────────────────
awk -F'\t' -v COVERS="$TMP/covers" -v OUT="$TMP/ledger" -v CHECK="$CHECK_ONLY" '
BEGIN {
    while ((getline line < COVERS) > 0) { n = split(line, f, "\t"); cov[f[1]] = f[2] }
    fails = 0; covered = 0; na = 0; unc = 0
}
/^#/ || /^[ \t]*$/ { print > OUT; next }
{
    sec = $1; status = $4; reason = $5; note = $6
    have = (sec in cov)

    # a section the ledger does not know cannot be claimed
    if (have) seen[sec] = 1

    if (have && status == "N/A") {
        printf "  FAIL  %s: a test claims it (%s) but the ledger says N/A — one of them is wrong\n", sec, cov[sec]
        fails++
    }
    if (have) {
        # --check is the GATE, so a stale ledger has to FAIL rather than be silently
        # recomputed: a test carries a marker but the checked-in row does not say COVERED
        # (or names different artifacts). Without this the "writing without claiming"
        # direction only ever got fixed, never reported, and the gate would pass on a
        # ledger nobody had regenerated.
        if (CHECK && ($4 != "COVERED" || $5 != cov[sec])) {
            printf "  FAIL  %s: covered by %s, but the ledger says %s%s — run join-ledger.sh\n",
                   sec, cov[sec], $4, ($4 == "COVERED" ? " (" $5 ")" : "")
            fails++
        }
        status = "COVERED"; reason = cov[sec]
    }
    else if (status == "COVERED") {
        printf "  FAIL  %s: marked COVERED (%s) but NO test carries a `// JLS %s` marker\n", sec, reason, sec
        fails++
        status = "UNCOVERED"; reason = ""
    }

    if (status == "COVERED") covered++
    else if (status == "N/A")  na++
    else                       unc++
    printf "%s\t%s\t%s\t%s\t%s\t%s\n", $1, $2, $3, status, reason, note > OUT
}
END {
    for (s in cov)
        if (!(s in seen)) { printf "  FAIL  %s: claimed by %s but there is NO SUCH SECTION in the ledger\n", s, cov[s]; fails++ }
    printf "\n  join: %d covered, %d n/a, %d uncovered\n", covered, na, unc
    if (fails) { printf "  FAIL  join-ledger: %d problem(s)\n", fails; exit 1 }
    printf "  ....  join-ledger: every marker has a row, every COVERED row has a marker\n"
}
' "$LEDGER"

if [ "$CHECK_ONLY" -eq 0 ]; then
    cp "$TMP/ledger" "$LEDGER"
    echo "  ....  ledger updated"
fi
