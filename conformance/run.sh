#!/bin/sh
# conformance/run.sh — the Java e2e conformance corpus.
#
# Invoked by the root Makefile as `sh conformance/run.sh`, with no arguments and
# no environment, from the repository root. Its EXIT CODE is the whole contract:
# nonzero fails `make test`. The Makefile comment that reserved this slot is the
# spec it has to meet — "real .java programs with expected stdout and exit code,
# driven through the SHIPPED javelinac + javelina binaries, both tiers" and "it
# must never pass silently."
#
# Never passing silently is why the RESULT-line count is asserted rather than
# just diffed. A star-diff alone passes vacuously when every config produces no
# output at all, which is the same defect as an exclusion counter stuck at zero.
#
# The oracle is differential: the same program, compiled at both optimisation
# levels and run on both execution tiers, must agree on every checksum. A
# disagreement names a config that is WRONG, not slow — and because the program
# prints its seed, the failing run replays exactly:
#
#     sh conformance/run.sh --full --seed 12345
#
# Usage:
#   sh conformance/run.sh                 quick corpus (what `make test` runs)
#   sh conformance/run.sh --full          full scales
#   sh conformance/run.sh --seed N        replay a specific seed

set -e

cd "$(dirname "$0")/.."                   # repository root, however we were invoked

B=compiler/build
LIBDIR=compiler/lib/java
SRC=conformance/src
MODE=quick
SEED=20260725

while [ $# -gt 0 ]; do
    case "$1" in
        --full)  MODE=full; shift ;;
        --quick) MODE=quick; shift ;;
        --seed)  SEED=$2; shift 2 ;;
        -h|--help)
            sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            echo "  FAIL  conformance: unknown argument '$1'"
            exit 2 ;;
    esac
done

# The number of kernels GcTorture.NAMES declares. A kernel that stops running --
# because dispatch fell through, or the program died half way -- changes this
# count, and that must fail rather than quietly shrink the corpus.
EXPECT_KERNELS=11

fail=0

# ── artifacts ───────────────────────────────────────────────────────────────
# Built here rather than assumed: this target has no prerequisites, so a bare
# `make test-java-conformance` has to stand on its own. javelina links the VM's
# prebuilt engine objects, so those come first.
make -s -C wasm all
make -s -C compiler javelinac javelina

# -O is javelinac's default, so the O0 legs must say so explicitly. The jre is
# compiled at the SAME level as the program it links against: a plugin imports
# java.lang from the jre module, and mixing levels would test a configuration
# nobody ships.
$B/javelinac --mode jre --libdir $LIBDIR -O0 -o $B/conf-jre-O0.wasm
$B/javelinac --mode jre --libdir $LIBDIR -O  -o $B/conf-jre-O.wasm
$B/javelinac --libdir $LIBDIR -O0 $SRC -o $B/conf-gct-O0.wasm
$B/javelinac --libdir $LIBDIR -O  $SRC -o $B/conf-gct-O.wasm

# ── the matrix ──────────────────────────────────────────────────────────────
if [ "$MODE" = quick ]; then ARG=quick; else ARG=full; fi

for cfg in O0-interp O0-jit O-interp O-jit; do
    case $cfg in
        O0-*) jre=conf-jre-O0.wasm; plug=conf-gct-O0.wasm ;;
        O-*)  jre=conf-jre-O.wasm;  plug=conf-gct-O.wasm  ;;
    esac
    case $cfg in
        *-interp) tier=-nojit ;;
        *-jit)    tier=-jit   ;;
    esac
    if $B/javelina --jre $B/$jre $tier $B/$plug "$ARG" "$SEED" > $B/conf-$cfg.out 2> $B/conf-$cfg.err; then
        echo "  ....  gc-torture $cfg"
    else
        echo "  FAIL  gc-torture $cfg  (exit $?, seed $SEED)"
        sed 's/^/        | /' $B/conf-$cfg.out
        sed 's/^/        | /' $B/conf-$cfg.err
        fail=1
    fi
done

if [ $fail -ne 0 ]; then
    echo "java e2e conformance: 0 passed, 1 failed"
    exit 1
fi

# ── the differential gate ───────────────────────────────────────────────────
# The whole RESULT line is compared, not a projection of it: unlike the bench
# harness these lines carry no timings, so there is nothing on them that is
# allowed to differ between configs.
awk '/^RESULT/' $B/conf-O0-interp.out > $B/conf-ref.chk

n=$(wc -l < $B/conf-ref.chk | tr -d ' ')
if [ "$n" -ne "$EXPECT_KERNELS" ]; then
    echo "  FAIL  gc-torture: $n RESULT lines, expected $EXPECT_KERNELS (seed $SEED)"
    echo "        a corpus that shrinks silently is the defect this check exists for"
    echo "java e2e conformance: 0 passed, 1 failed"
    exit 1
fi

for cfg in O0-jit O-interp O-jit; do
    awk '/^RESULT/' $B/conf-$cfg.out > $B/conf-$cfg.chk
    if ! diff $B/conf-ref.chk $B/conf-$cfg.chk > /dev/null; then
        echo "  FAIL  gc-torture: O0-interp and $cfg disagree — that config is WRONG, not slow"
        echo "        replay: sh conformance/run.sh --$MODE --seed $SEED"
        diff $B/conf-ref.chk $B/conf-$cfg.chk | sed 's/^/        | /' || true
        echo "java e2e conformance: 0 passed, 1 failed"
        exit 1
    fi
done

echo "  PASS  gc-torture ($EXPECT_KERNELS kernels × 4 configs agree, seed $SEED, $MODE)"
echo "java e2e conformance: 1 passed, 0 failed"
