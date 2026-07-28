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
#
# THE CITATION MUST NAME ITS SPEC. javelina cites four authorities in the same files -- the JLS,
# Click's combined-analysis paper, the WASM core spec, and Unicode -- and a bare "§4.7" is
# ambiguous between them. Reading one as a JLS section produced a permanent
#
#   note  4.7: deferred in .../sir_optimizer.c — no such leaf section in the ledger
#
# on every single run, from a comment that said "deferred to Pass 2 because it is a §4.7 COPY
# Follower": ordinary English about pass ordering, citing CLICK. It was this gate's only hit,
# so the gate reported nothing but noise -- and noise is how the real thing gets missed, since
# a genuine JLS deferral prints an identically-shaped line.
#
# So: only `JLS §N` counts, and an UNATTRIBUTED citation on a deferral line is a failure rather
# than a guess. That is a rule about writing comments, which is the cheap end to fix.
SECTION_RE='§[0-9]+(\.[0-9]+)*'
AUTHORITY_RE='(JLS|Click|Choi|Dybvig|Braun|WASM|Wasm|wasm|JVMS|Unicode|IEEE|ECMA|POSIX|RFC)'

# The verb list is DELIBERATELY BROAD. The narrow one -- "not yet (implemented|covered|carried
# |supported)" -- missed java/lang/Throwable.java's "The VM does not yet EXPOSE stack frames",
# a real deferral citing a real section, because "expose" was not on the list. A deferral gate
# that only catches four verbs teaches you it is working while things walk past it.
#
# Breadth is safe here because the CITATION filter does the narrowing: a line only reaches the
# ledger check if it carries `JLS §N`. So the dataflow vocabulary that legitimately says "not
# yet known" (a lattice TOP in sir_optimizer.c) is dropped for citing nothing, not for being
# phrased carefully.
DEFER_RE='not yet|does ?n.t yet|is ?n.t yet|no .* yet|yet to be|later refinement|future (refinement|work)|deferred|deliberately absent|(is|are) not implemented|unimplemented|for now|stub|placeholder|TODO'

grep -rniE "$DEFER_RE" \
     --include=*.c --include=*.h --include=*.peg --include=*.ddcg --include=*.java \
     compiler/src compiler/grammar compiler/lib 2>/dev/null \
  | grep -vE '^compiler/src/gen/' \
  | grep -E "$SECTION_RE" > "$TMP/lines" || true

# Unattributed citations first: they cannot be checked, so they are reported, not assumed.
: > "$TMP/unattributed"
while IFS= read -r line; do
    [ -n "$line" ] || continue
    stripped=$(printf '%s\n' "$line" | sed -E "s/$AUTHORITY_RE[[:space:]]*$SECTION_RE//g")
    if printf '%s\n' "$stripped" | grep -qE "$SECTION_RE"; then
        printf '%s\n' "${line%%:*}: ${line#*:}" >> "$TMP/unattributed"
    fi
done < "$TMP/lines"

if [ -s "$TMP/unattributed" ]; then
    echo "  FAIL  check-deferrals: a deferral cites a section without naming its spec."
    echo "        javelina cites the JLS, Click, WASM and Unicode in the same files, so a bare"
    echo "        §N cannot be checked against the JLS ledger. Write \`JLS §N\` or \`Click §N\`."
    sed 's/^/        | /' "$TMP/unattributed"
    exit 1
fi

# Only JLS-attributed citations are the ledger's business.
while IFS= read -r line; do
    [ -n "$line" ] || continue
    file=${line%%:*}
    printf '%s\n' "$line" \
      | grep -oE "JLS[[:space:]]*$SECTION_RE" \
      | grep -oE '[0-9]+(\.[0-9]+)*' \
      | while IFS= read -r sec; do printf '%s\t%s\n' "$sec" "$file"; done
done < "$TMP/lines" | sort -u > "$TMP/deferred"

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
