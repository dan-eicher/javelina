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
CAP=${2:-40}
PERCASE=${3:-8}

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

fail=0
for f in "$OUT"/Case*.java; do
    name=$(basename "$f" .java)
    exp="$OUT/$name.expected"
    [ -f "$exp" ] || { echo "  FAIL  $name has no composed expectation"; fail=1; continue; }

    # Each case is its own module: one failing case must not take the others' results with it,
    # and the case name has to appear in the failure.
    if ! $B/javelinac --libdir $LIBDIR -O0 "$f" -o "$B/conf-$name.wasm" > "$B/conf-$name.log" 2>&1; then
        echo "  FAIL  $name did not compile — a generator bug or a javelinac bug, never a test bug"
        sed 's/^/        | /' "$B/conf-$name.log" | head -6
        fail=1
        continue
    fi

    for tier in -nojit -jit; do
        if ! $B/javelina --jre $B/conf-jre-O0.wasm $tier "$B/conf-$name.wasm" \
                 > "$B/conf-$name$tier.out" 2>&1; then
            echo "  FAIL  $name died at run time ($tier)"
            sed 's/^/        | /' "$B/conf-$name$tier.out" | head -6
            fail=1
            continue
        fi
        if ! diff -q "$exp" "$B/conf-$name$tier.out" > /dev/null; then
            echo "  FAIL  $name ($tier): output differs from its COMPOSED expectation"
            diff "$exp" "$B/conf-$name$tier.out" | sed 's/^/        | /' | head -12
            fail=1
        fi
    done
done

if [ $fail -ne 0 ]; then
    echo "  FAIL  stitched corpus: $ncase cases, at least one wrong"
    exit 1
fi

echo "  ....  stitched corpus: $ncase cases match their composed expectation on both tiers"
