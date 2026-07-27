#!/bin/sh
# check-cardinality.sh — the gate crisp-tallying-chapters §5 calls "the check that makes
# stitching impossible to skip".
#
# A `// JLS 5.1.2` marker means SOMETHING covering §5.1.2 was written. §5.1.2 names nineteen
# conversions, so one case out of nineteen produces exactly the same marker as all nineteen —
# and with a per-type cap discarding stitchings, one-out-of-nineteen is the normal outcome, not
# a corner. The join cannot see the difference; nothing could, until the count was written down.
#
# So: for every section whose own text states how many cases it has (conformance/cardinality.tsv,
# each row carrying the sentence its number comes from), count the DISTINCT ROOT SNIPPETS that
# reached a written case (conformance/generated/SECTIONS.tsv, emitted by the generator) and
# require at least that many.
#
# At least, not exactly: a section may legitimately carry extra snippets — §5.1.2 also owns the
# sign-extend/zero-extend pair and the spec's -46 example, which are rules about the nineteen
# rather than members of them. Fewer than declared is the failure this exists for.
#
# Usage:  sh conformance/check-cardinality.sh

set -e
cd "$(dirname "$0")/.."

TABLE=conformance/cardinality.tsv
MANIFEST=conformance/generated/SECTIONS.tsv

[ -f "$TABLE" ] || { echo "  FAIL  check-cardinality: $TABLE missing"; exit 1; }

# A missing manifest is the failure mode this gate exists for — it means the generator did not
# run, which is exactly how the deliverable went missing while every other check stayed green.
if [ ! -f "$MANIFEST" ]; then
    echo "  FAIL  check-cardinality: $MANIFEST missing — the generator did not run,"
    echo "        so every declared cardinality is unverified rather than met"
    exit 1
fi

awk -F'\t' -v M="$MANIFEST" '
BEGIN {
    # section -> number of distinct root snippets that reached a case
    while ((getline line < M) > 0) {
        if (line ~ /^#/ || line == "") continue
        n = split(line, f, "\t")
        if (n < 2) continue
        key = f[1] SUBSEP f[2]
        if (!(key in pair)) { pair[key] = 1; have[f[1]]++ }
    }
    fails = 0; rows = 0
}
/^#/ || /^[ \t]*$/ { next }
{
    sec = $1; want = $2 + 0; sentence = $3
    got = (sec in have) ? have[sec] : 0
    rows++
    if (got < want) {
        printf "  FAIL  %s declares %d cases, %d reached a written case\n", sec, want, got
        printf "        %s\n", sentence
        if (got == 0)
            printf "        zero means no snippet claims it at all — a missing generator, not a partial one\n"
        else
            printf "        the shortfall is the per-type cap discarding stitchings; raise it, or\n"
            printf "        register the missing conversions as their own root snippets\n"
        fails++
    } else {
        printf "  ....  %s: %d of %d declared cases\n", sec, got, want
    }
}
END {
    if (rows == 0) {
        print "  FAIL  check-cardinality: no rows in the table — the gate would pass vacuously"
        exit 1
    }
    if (fails) { printf "  FAIL  check-cardinality: %d section(s) short of their declared count\n", fails; exit 1 }
    printf "  ....  check-cardinality: %d declared cardinalities all met\n", rows
}
' "$TABLE"
