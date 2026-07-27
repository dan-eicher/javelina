#!/bin/sh
# check-deferrals.sh — a deferral comment and a coverage claim cannot coexist.
#
# The pattern this exists for, in the compiler's own words:
#
#   Java.peg          "(Literal interning, §3.10.5, is a later refinement; each occurrence
#                      currently builds a fresh String.)"
#   const_expr.c      "NOT YET COVERED, and deliberately absent rather than approximated:
#                      the String half ... §3.10.5 interning is not implemented either"
#   Character.java    "// spec MIN_VALUE, \u deferred"
#
# Every one was true, written down, and invisible. The third was itself a compile error under
# §3.2 and nobody found out for as long as the rule it deferred went unimplemented. A note in
# a source file is not a record — it is a place things go to be forgotten.
#
# So: if a comment defers something AND names a JLS section, that section must not be claimed
# as COVERED. The comment becomes a forced UNCOVERED row — visible in the ledger, counted by
# the ratchet, and impossible to satisfy by writing prose.
#
# This does NOT try to find every deferral. It finds the ones that cite a section, which are
# exactly the ones that can be reconciled mechanically. A deferral with no citation is invisible
# to it — and that is itself the argument for citing one.

set -e
cd "$(dirname "$0")/.."

LEDGER=conformance/jls-ledger.tsv
[ -f "$LEDGER" ] || { echo "  FAIL  check-deferrals: $LEDGER missing"; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Deferral language, on a line that also cites a §section. `grep -o` per line keeps the pairing.
grep -rniE '(not yet (implemented|covered|carried|supported)|later refinement|deferred|deliberately absent|is not implemented|unimplemented)' \
     --include=*.c --include=*.h --include=*.peg --include=*.ddcg --include=*.java \
     compiler/src compiler/grammar compiler/lib 2>/dev/null \
  | grep -vE '^compiler/src/gen/' \
  | while IFS= read -r line; do
      file=${line%%:*}
      printf '%s\n' "$line" \
        | grep -oE '§[0-9]+(\.[0-9]+)*' \
        | sed 's/§//' \
        | while IFS= read -r sec; do printf '%s\t%s\n' "$sec" "$file"; done
    done | sort -u > "$TMP/deferred"

if [ ! -s "$TMP/deferred" ]; then
    echo "  ....  check-deferrals: no section-citing deferral comments"
    exit 0
fi

awk -F'\t' -v DEF="$TMP/deferred" '
BEGIN {
    while ((getline line < DEF) > 0) { n = split(line, f, "\t"); where[f[1]] = f[2]; want[f[1]] = 1 }
    fails = 0; checked = 0
}
/^#/ || /^[ \t]*$/ { next }
{
    sec = $1
    if (!(sec in want)) next
    seen[sec] = 1; checked++
    if ($4 == "COVERED") {
        printf "  FAIL  %s: %s DEFERS it, but the ledger claims COVERED (%s).\n", sec, where[sec], $5
        printf "        One of them is lying. Delete the comment or drop the claim.\n"
        fails++
    }
}
END {
    for (s in want)
        if (!(s in seen))
            printf "  note  %s: deferred in %s — no such leaf section in the ledger (a parent, or a typo)\n", s, where[s]
    printf "  ....  check-deferrals: %d section-citing deferral(s), none claimed as covered\n", checked
    if (fails) exit 1
}
' "$LEDGER"
