#!/bin/sh
# bench.sh — orchestrate the optimization-level benchmark matrix.
#
# The MEASUREMENT lives in Java (bench/java/Bench.java main(): warmup, reps, ms timing via the
# runner's real clock, min/median, per-rep checksum gate) so all four configs run the identical
# methodology in-guest. This script keeps only what Java structurally cannot do:
#   1. compile the two artifacts     (javelinac without/with -O; jre at the SAME level)
#   2. launch the four configs       (javelina -nojit / -jit on each artifact)
#   3. diff the four outputs         (checksums must AGREE across configs — a config that
#                                     computes a different answer is wrong, not slow)
#   4. merge the RESULT lines into one side-by-side table
#
# Usage: bench.sh [--quick]     --quick: tiny scales, correctness gate only (the suite runs this)
# See bench/README.md for the kernel catalog, how to read the table, the sensitivity
# floor, and the burg-cost-analysis workflow (diff build/bench-<config>.out across changes).
#
# SENSITIVITY FLOOR (measured 2026-07-20, do not claim past it): min-of-reps WITHIN one process is
# tight, but BETWEEN-process variance is ~±7% — one matrix run supports the big ratios (jit x,
# sieve-sized opt x) and does NOT support cross-config deltas under ~20%. The first matrix
# "found" a 17% -O-under-JIT pessimization on arith/fib; opcode-stream dumps (identical code,
# only slot numbers differed), isolated 50M-iteration runs, and rerun overlap all disproved it.
# For a small-delta claim: rerun the matrix several times and compare distributions — or dump and
# diff the opcode streams first, because identical streams mean there is nothing to measure.
set -e
case "$1" in -h|--help)
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    exit 0 ;;
esac
cd "$(dirname "$0")/.."           # compiler/
B=build
QUICK=""; [ "$1" = "--quick" ] && QUICK=quick
T_START=$(date +%s)
# The wall-clock BUDGET (full mode): the whole matrix — compiles included — must
# stay under this, or it stops being run and stops measuring anything. When the
# warning fires, the fix is recalibrating Bench.java's full[] scales (the formula
# is in its comment), not deleting kernels and not living with it.
BUDGET=600

# History worth keeping: this harness's FIRST run caught a Click -O miscompile (array-element
# stores took §2 strong updates, so a rotating dispatch devirtualized on a false singleton and
# died on its guard cast). bench/VirtRepro.java is the 15-line e2e pin;
# test_pts_arraystore_weak_on_concrete_array is the unit pin. -O output had no e2e coverage
# anywhere before this gate existed.

echo "== artifacts (jre + Bench at each opt level) =="
make -s javelinac javelina
# -O is javelinac's DEFAULT since 2026-07-24 — the O0 legs must say so explicitly.
$B/javelinac --mode jre --libdir lib/java -O0 -o $B/jre-O0.wasm
$B/javelinac --mode jre --libdir lib/java -O -o $B/jre-O.wasm
$B/javelinac --libdir lib/java -O0 bench/java/Bench.java -o $B/bench-O0.wasm
$B/javelinac --libdir lib/java -O bench/java/Bench.java -o $B/bench-O.wasm
for f in jre-O0 jre-O bench-O0 bench-O; do
    printf '  %-14s %8d bytes\n' "$f.wasm" "$(wc -c < $B/$f.wasm)"
done

echo "== running the matrix =="
# Two axes: javelinac's optimizer off/on, and javelina's execution tier. The tier
# axis has three levels — interpret, copy-and-patch, copy-and-patch with
# operand-stack caching — so the matrix is 2 x 3.
CFGS="O0-t0 O0-t1 O0-t2 O-t0 O-t1 O-t2"
for cfg in $CFGS; do
    case $cfg in
        O0-*) jre=jre-O0.wasm; plug=bench-O0.wasm ;;
        O-*)  jre=jre-O.wasm;  plug=bench-O.wasm  ;;
    esac
    case $cfg in
        *-t0) tier="--tier 0" ;;
        *-t1) tier="--tier 1" ;;
        *-t2) tier="--tier 2" ;;
    esac
    echo "  -- $cfg"
    $B/javelina --jre $B/$jre $tier $B/$plug $QUICK > $B/bench-$cfg.out
    sed 's/^/     /' $B/bench-$cfg.out
done

echo "== checksum agreement across configs (the correctness gate) =="
awk '/^RESULT/{print $2, $3}' $B/bench-O0-t0.out > $B/bench-ref.chk
for cfg in $CFGS; do
    [ "$cfg" = O0-t0 ] && continue
    awk '/^RESULT/{print $2, $3}' $B/bench-$cfg.out > $B/bench-$cfg.chk
    if ! diff $B/bench-ref.chk $B/bench-$cfg.chk >/dev/null; then
        echo "CHECKSUM MISMATCH between O0-t0 and $cfg — that config is WRONG, not slow:"
        diff $B/bench-ref.chk $B/bench-$cfg.chk || true
        exit 1
    fi
done
echo "  all six configs agree on every checksum"

[ -n "$QUICK" ] && exit 0

echo ""
echo "== comparison (min_ms; ratios vs O0-t0) =="
# RESULT lines are 5 fields wide, so config k's min= is field 5k+4.
paste $B/bench-O0-t0.out $B/bench-O0-t1.out $B/bench-O0-t2.out \
      $B/bench-O-t0.out  $B/bench-O-t1.out  $B/bench-O-t2.out | \
awk '/^RESULT/ {
    name=$2;
    split($4,a,"="); split($9,b,"="); split($14,c,"=");
    split($19,d,"="); split($24,e,"="); split($29,f,"=");
    m0=a[2]; m1=b[2]; m2=c[2]; m3=d[2]; m4=e[2]; m5=f[2];
    if (!hdr) { printf "  %-7s %9s %9s %9s %9s %9s %9s   %7s %7s %7s\n",
        "bench","O0-t0","O0-t1","O0-t2","O-t0","O-t1","O-t2",
        "t1x","t2x","t2/t1"; hdr=1 }
    printf "  %-7s %7dms %7dms %7dms %7dms %7dms %7dms   %6.2fx %6.2fx %6.2fx\n",
        name, m0, m1, m2, m3, m4, m5,
        (m4>0 ? m3/m4 : 0),          # what the JIT buys over the interpreter, at -O
        (m5>0 ? m3/m5 : 0),          # what the CACHING JIT buys over the interpreter
        (m5>0 ? m4/m5 : 0)           # …and over the plain JIT, which is the tier-2 question
}'
echo ""
echo "raw per-config outputs: $B/bench-<config>.out (stable RESULT lines — diff them across compiler changes)"
T_ELAPSED=$(( $(date +%s) - T_START ))
echo "matrix wall clock: ${T_ELAPSED}s (budget ${BUDGET}s)"
if [ "$T_ELAPSED" -gt "$BUDGET" ]; then
    echo "WARNING: over budget — recalibrate Bench.java full[] scales (see its comment)"
fi
