#!/bin/sh
# test_peg.sh — the pegc Java backend, end to end.
#
#   test/peg/Fixture.peg  --pegc -lang java-->  FixtureParser.java
#   FixtureParser.java + PegSmoke.java  --javelinac-->  peg-smoke.wasm
#   peg-smoke.wasm  --javelina -nojit / -jit-->  identical output, zero failures
#
# This is the level the pegc-side text pins cannot reach: that the emitted
# save/restore pairs actually implement ordered choice, that a labeled break
# commits to the right alternative, and that the generated parser means the
# same thing interpreted and jitted.
#
# The generated parser is written to build/ and is NOT checked in: it is
# regenerated here every run, so a backend change that breaks the shape shows
# up as a failure rather than as a stale artifact that still works.
set -e
cd "$(dirname "$0")/.."           # compiler/
B=build
BBQ=${BBQ:-../BBQ}
PEGC=$BBQ/build/pegc/pegc
FRAMES=$BBQ/pegc/backend/frames
GEN=$B/peg

if [ ! -x "$PEGC" ]; then
    echo "test_peg: $PEGC not built — build BBQ first"
    exit 1
fi

mkdir -p $GEN
$PEGC test/peg/Fixture.peg -lang java -frames "$FRAMES" -o $GEN \
      -prefix Fixture -java-runtime javelina.peg

# The fixture is genuinely recursive (a group contains a Start), so the
# recursion-cycle warning is expected output, not noise to hide.
#
# The exit status is checked BEFORE the warning filter. Piping javelinac into
# grep hands the pipeline grep's status, so a compilation failure looked like a
# success and the script went on to run whatever .wasm was left over from the
# previous run — which passed, because it was built from the previous sources.
if ! $B/javelinac --libdir lib/java $GEN/FixtureParser.java test/peg/PegSmoke.java \
        test/peg/MachineSmoke.java test/peg/RegexSmoke.java \
        -o $GEN/peg-smoke.wasm > $GEN/compile.log 2>&1; then
    sed 's/^/  /' $GEN/compile.log
    echo "test_peg: javelinac failed"
    exit 1
fi
grep -v 'recursion cycle' $GEN/compile.log || true

fail=0
for tier in -nojit -jit; do
    $B/javelina --jre $B/jre.wasm $tier $GEN/peg-smoke.wasm > $GEN/out$tier.txt
    if ! grep -q '^PEG-RESULT failures=0$' $GEN/out$tier.txt; then
        echo "test_peg: FAILURES under $tier:"
        grep '^FAIL' $GEN/out$tier.txt | sed 's/^/  /'
        fail=1
    fi
done

# A parser that agrees with itself is the whole point of having two tiers.
if ! diff -q $GEN/out-nojit.txt $GEN/out-jit.txt > /dev/null; then
    echo "test_peg: interpreter and JIT disagree — that is a miscompile, not a slowdown:"
    diff $GEN/out-nojit.txt $GEN/out-jit.txt | sed 's/^/  /'
    fail=1
fi

if [ "$fail" -ne 0 ]; then exit 1; fi
echo "test_peg: $(grep -c '^ok' $GEN/out-nojit.txt) cases, both tiers agree"
