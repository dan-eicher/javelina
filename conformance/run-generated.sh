#!/bin/sh
# run-generated.sh — P4 of crisp-tallying-chapters: the STITCHED corpus and its derived oracle.
#
# conformance/gen is a Java program that writes Java programs together with the exact stdout
# each must produce. Its expectations are COMPOSED from the snippets' `expects`, never observed
# by running — so this leg compares javelina's actual output against a value the generator
# computed independently of the compiler. Pinning what javelinac does today would make the
# corpus a snapshot; composing makes it an oracle.
#
# Cases land in conformance/generated/, which is what join-ledger.sh scans for the `// JLS <n>`
# markers Emit writes from each snippet's sections[]. That is the whole path by which stitching
# turns into counted coverage: a snippet declares its sections, the stitching carries the union,
# the case file carries the markers, the join reads them.
#
# Both tiers, because a case that agrees with its expectation under the interpreter and not
# under the JIT names a config that is WRONG — the same reason gc-torture runs four ways.
#
# Usage:  sh conformance/run-generated.sh [depth] [cap] [perCase]

set -e
cd "$(dirname "$0")/.."

B=compiler/build
LIBDIR=compiler/lib/java
OUT=conformance/generated
DEPTH=${1:-2}
# The cap is a budget per (type, depth), water-filled across the snippets offering that type.
# It therefore has to be at least the NUMBER OF SNIPPETS of the commonest type: below that,
# water-fill cannot give every snippet even one stitching and whole leaves get a quota of zero
# — which is how eleven of §5.1.3's twenty-three narrowings vanished while the ledger read
# COVERED. It is not a tuning knob at the low end; it is a floor that grows with the libraries.
# check-cardinality.sh is what catches it having been outgrown.
CAP=${2:-200}
PERCASE=${3:-25}

# Regenerated from scratch every run: the generator is deterministic (GenMain: "two runs of the
# same build write byte-identical files"), so a stale case surviving here would be a case whose
# snippets no longer exist still claiming their sections.
rm -rf "$OUT"
mkdir -p "$OUT"

$B/javelinac --libdir $LIBDIR -O0 conformance/gen -o $B/conf-gen.wasm
$B/javelina --jre $B/conf-jre-O0.wasm --root conformance $B/conf-gen.wasm generated \
            "$DEPTH" "$CAP" "$PERCASE" > $B/conf-gen.log 2>&1 || {
    echo "  FAIL  generator did not run"
    sed 's/^/        | /' $B/conf-gen.log
    exit 1
}

ncase=$(ls "$OUT"/Case*.java 2>/dev/null | wc -l | tr -d ' ')
if [ "$ncase" -eq 0 ]; then
    echo "  FAIL  the generator produced no cases — stitching is silently doing nothing"
    exit 1
fi

# Build and run every case — make's job, not a shell loop's. The cases are independent, so
# -j turns four serial minutes into one core's worth, and the dependency edges mean an
# unchanged case is not recompiled at all. The case list is a wildcard inside cases.mk, which
# is why this is a separate invocation: the generator above had to write them first.
JOBS=$( (nproc 2>/dev/null) || echo 4 )
if ! make -f conformance/cases.mk -j"$JOBS" OUT="$OUT" B="$B" LIBDIR="$LIBDIR" all; then
    echo "  FAIL  stitched corpus: a case failed to compile or died at run time"
    exit 1
fi

# The verdict. Still shell for the moment, and it should not be: comparing an output against
# its composed expectation is judgement, and crisp-tallying-chapters §3 puts the instrument in
# Java on the VM. Next step is a guest program reading these files, which it can — the RTL has
# FileInputStream; what it cannot do is spawn javelinac, which is the whole reason a driver
# exists at all.
fail=0
for f in "$OUT"/Case*.java; do
    name=$(basename "$f" .java)
    exp="$OUT/$name.expected"
    [ -f "$exp" ] || { echo "  FAIL  $name has no composed expectation"; fail=1; continue; }
    for tier in nojit jit; do
        if ! diff -q "$exp" "$OUT/$name.$tier.out" > /dev/null 2>&1; then
            echo "  FAIL  $name ($tier): output differs from its COMPOSED expectation"
            diff "$exp" "$OUT/$name.$tier.out" 2>&1 | sed 's/^/        | /' | head -12
            fail=1
        fi
    done
done

if [ $fail -ne 0 ]; then
    echo "  FAIL  stitched corpus: $ncase cases, at least one wrong"
    exit 1
fi

echo "  ....  stitched corpus: $ncase cases match their composed expectation on both tiers"
