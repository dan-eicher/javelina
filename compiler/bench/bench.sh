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
#
# SENSITIVITY FLOOR (measured 2026-07-20, do not claim past it): min-of-reps WITHIN one process is
# tight, but BETWEEN-process variance is ~±7% — one matrix run supports the big ratios (jit x,
# sieve-sized opt x) and does NOT support cross-config deltas under ~20%. The first matrix
# "found" a 17% -O-under-JIT pessimization on arith/fib; opcode-stream dumps (identical code,
# only slot numbers differed), isolated 50M-iteration runs, and rerun overlap all disproved it.
# For a small-delta claim: rerun the matrix several times and compare distributions — or dump and
# diff the opcode streams first, because identical streams mean there is nothing to measure.
set -e
cd "$(dirname "$0")/.."           # compiler/
B=build
QUICK=""; [ "$1" = "--quick" ] && QUICK=quick

# History worth keeping: this harness's FIRST run caught a Click -O miscompile (array-element
# stores took §2 strong updates, so a rotating dispatch devirtualized on a false singleton and
# died on its guard cast). bench/VirtRepro.java is the 15-line e2e pin;
# test_pts_arraystore_weak_on_concrete_array is the unit pin. -O output had no e2e coverage
# anywhere before this gate existed.

echo "== artifacts (jre + Bench at each opt level) =="
make -s javelinac javelina
$B/javelinac --mode jre --libdir lib/java -o $B/jre-O0.wasm
$B/javelinac --mode jre --libdir lib/java -O -o $B/jre-O.wasm
$B/javelinac --libdir lib/java bench/java/Bench.java -o $B/bench-O0.wasm
$B/javelinac --libdir lib/java bench/java/Bench.java -O -o $B/bench-O.wasm
for f in jre-O0 jre-O bench-O0 bench-O; do
    printf '  %-14s %8d bytes\n' "$f.wasm" "$(wc -c < $B/$f.wasm)"
done

echo "== running the matrix =="
for cfg in O0-interp O0-jit O-interp O-jit; do
    case $cfg in
        O0-*) jre=jre-O0.wasm; plug=bench-O0.wasm ;;
        O-*)  jre=jre-O.wasm;  plug=bench-O.wasm  ;;
    esac
    case $cfg in
        *-interp) tier=-nojit ;;
        *-jit)    tier=-jit   ;;
    esac
    echo "  -- $cfg"
    $B/javelina --jre $B/$jre $tier $B/$plug $QUICK > $B/bench-$cfg.out
    sed 's/^/     /' $B/bench-$cfg.out
done

echo "== checksum agreement across configs (the correctness gate) =="
awk '/^RESULT/{print $2, $3}' $B/bench-O0-interp.out > $B/bench-ref.chk
for cfg in O0-jit O-interp O-jit; do
    awk '/^RESULT/{print $2, $3}' $B/bench-$cfg.out > $B/bench-$cfg.chk
    if ! diff $B/bench-ref.chk $B/bench-$cfg.chk >/dev/null; then
        echo "CHECKSUM MISMATCH between O0-interp and $cfg — that config is WRONG, not slow:"
        diff $B/bench-ref.chk $B/bench-$cfg.chk || true
        exit 1
    fi
done
echo "  all four configs agree on every checksum"

[ -n "$QUICK" ] && exit 0

echo ""
echo "== comparison (min_ms; ratios vs O0-interp) =="
paste $B/bench-O0-interp.out $B/bench-O0-jit.out $B/bench-O-interp.out $B/bench-O-jit.out | \
awk '/^RESULT/ {
    name=$2; split($4,a,"="); split($9,b,"="); split($14,c,"="); split($19,d,"=");
    m0=a[2]; m1=b[2]; m2=c[2]; m3=d[2];
    if (NR==1 || !hdr) { printf "  %-7s %10s %10s %10s %10s   %7s %7s %7s\n",
        "bench","O0-int","O0-jit","O-int","O-jit","jit×","opt×","both×"; hdr=1 }
    printf "  %-7s %8dms %8dms %8dms %8dms   %6.2fx %6.2fx %6.2fx\n",
        name, m0, m1, m2, m3,
        (m1>0 ? m0/m1 : 0), (m2>0 ? m0/m2 : 0), (m3>0 ? m0/m3 : 0)
}'
echo ""
echo "raw per-config outputs: $B/bench-<config>.out (stable RESULT lines — diff them across compiler changes)"
